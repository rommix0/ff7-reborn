/* The PS1's BIOS in C: the 54 functions the exe calls, the two syscalls, and the interrupt path the game's libraries
 * build on. Behaviour per psx-spx's BIOS pages; the memory card answers "no card" until saves come to the PC build. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp.h"

#define ARG(n) ctx.r[4 + (n)]
#define PTR(n) ((char *)mem_ptr(ARG(n)))

#define SR_INTERRUPTS 0x401u /* IEc, and IM2: the line the interrupt controller drives */
#define IRQ_VBLANK 0x01u
#define IRQ_TIMER0 0x10u     /* timers 0-2 are bits 4-6; the BIOS calls the vblank root counter 3 */
#define EV_INTERRUPT 0x0002u /* EvSpINT */
#define EV_ROOT_COUNTER 0xF2000000u
#define EV_CARD 0xF4000001u  /* SwCARD */
#define EV_TIMEOUT 0x0100u   /* EvSpTIMOUT: no card answered */
#define EV_MODE_CALLBACK 0x2000u /* EvMdINTR: DeliverEvent calls the function; otherwise it marks the event ready */
#define EVENT_COUNT 32

unsigned int i_stat, i_mask;
unsigned short pad_buttons = 0xFFFF;
void gpu_gp0(unsigned int word);

enum { EV_FREE, EV_DISABLED, EV_ENABLED, EV_READY };
static struct {
    unsigned int class, spec, mode, func;
    int status;
} events[EVENT_COUNT];

static unsigned int custom_exit;   /* SetCustomExitFromException's register buffer */
static jmp_buf exception_return;
static int in_exception;
static int clear_root_counter[4] = {1, 1, 1, 1}, clear_pad = 1;
static unsigned int pad_buffers[2];
static int pad_started;
static unsigned int rand_state;

static void deliver_event(unsigned int class, unsigned int spec)
{
    int i;

    for (i = 0; i < EVENT_COUNT; i++) {
        if (events[i].status != EV_ENABLED || events[i].class != class || events[i].spec != spec)
            continue;
        if (events[i].mode == EV_MODE_CALLBACK) {
            if (events[i].func)
                call_indirect(events[i].func);
        } else
            events[i].status = EV_READY;
    }
}

static void read_pads(void)
{
    unsigned char *buf;

    if (!pad_started)
        return;
    if (pad_buffers[0]) {
        buf = mem_ptr(pad_buffers[0]);
        buf[0] = 0x00; /* answered */
        buf[1] = 0x41; /* a digital pad, one halfword of buttons */
        buf[2] = pad_buttons & 0xFF;
        buf[3] = pad_buttons >> 8;
    }
    if (pad_buffers[1])
        *(unsigned char *)mem_ptr(pad_buffers[1]) = 0xFF; /* nothing in port 2 */
    if (clear_pad)
        i_stat &= ~IRQ_VBLANK;
}

void irq_raise(unsigned int bit)
{
    i_stat |= bit;
    irq_check();
}

/* The exception path: the BIOS's own handlers (pads, root-counter events), then the game's custom exit, which leaves
 * through ReturnFromException. The interrupted code gets its registers back either way. */
void irq_check(void)
{
    unsigned int sr = cop0[12], pending = i_stat & i_mask, *exit_regs;
    Ctx saved;
    int n;

    if (!pending || (sr & SR_INTERRUPTS) != SR_INTERRUPTS || in_exception)
        return;
    saved = ctx;
    in_exception = 1;
    cop0[12] = (sr & ~0x3Fu) | (sr << 2 & 0x3Fu); /* the mode stack pushes: interrupts off in the handler */
    if (pending & IRQ_VBLANK)
        read_pads();
    for (n = 0; n < 4; n++) {
        unsigned int bit = n == 3 ? IRQ_VBLANK : IRQ_TIMER0 << n;

        if (pending & bit) {
            deliver_event(EV_ROOT_COUNTER + n, EV_INTERRUPT);
            if (clear_root_counter[n])
                i_stat &= ~bit;
        }
    }
    if (custom_exit && !setjmp(exception_return)) {
        exit_regs = mem_ptr(custom_exit);
        ctx.r[31] = exit_regs[0];
        ctx.r[29] = exit_regs[1];
        ctx.r[30] = exit_regs[2];
        for (n = 0; n < 8; n++)
            ctx.r[16 + n] = exit_regs[3 + n];
        ctx.r[28] = exit_regs[11];
        ctx.r[2] = 1; /* the buffer's setjmp returns again, as from longjmp */
        call_indirect(ctx.r[31]);
    }
    ctx = saved;
    cop0[12] = sr;
    in_exception = 0;
}

void recomp_syscall(unsigned int code)
{
    (void)code;
    switch (ARG(0)) {
    case 1: /* EnterCriticalSection */
        ctx.r[2] = (cop0[12] & SR_INTERRUPTS) == SR_INTERRUPTS;
        cop0[12] &= ~SR_INTERRUPTS;
        break;
    case 2: /* ExitCriticalSection */
        cop0[12] |= SR_INTERRUPTS;
        irq_check();
        break;
    }
}

/* printf with the R3000's variadic convention: arguments after a0 continue in a1-a3, then at 4 * n above $sp. */
static int bios_printf(void)
{
    const char *f = PTR(0);
    char spec[16];
    unsigned int value;
    int arg = 1, n;

    while (*f) {
        if (*f != '%') {
            fputc(*f++, stderr);
            continue;
        }
        for (n = 0; f[n] && n < 14 && (n < 2 || !strchr("diouxXcsp%", f[n - 1])); n++)
            spec[n] = f[n];
        spec[n] = 0;
        f += n;
        if (spec[n - 1] == '%') {
            fputc('%', stderr);
            continue;
        }
        value = arg < 4 ? ctx.r[4 + arg] : mem_r32(ctx.r[29] + 4 * arg);
        arg++;
        if (spec[n - 1] == 's')
            fprintf(stderr, spec, (char *)mem_ptr(value));
        else
            fprintf(stderr, spec, value);
    }
    return 0;
}

static int bios_setjmp(void)
{
    unsigned int *buf = mem_ptr(ARG(0));
    int n;

    buf[0] = ctx.r[31];
    buf[1] = ctx.r[29];
    buf[2] = ctx.r[30];
    for (n = 0; n < 8; n++)
        buf[3 + n] = ctx.r[16 + n];
    buf[11] = ctx.r[28];
    return 0;
}

static int bios_strcmp(void) { return strcmp(PTR(0), PTR(1)); }
static int bios_strncmp(void) { return strncmp(PTR(0), PTR(1), ARG(2)); }
static int bios_strlen(void) { return strlen(PTR(0)); }
static int bios_bcopy(void) { memmove(PTR(1), PTR(0), ARG(2)); return 0; }
static int bios_memcpy(void) { memmove(PTR(0), PTR(1), ARG(2)); return ARG(0); }
static int bios_memset(void) { memset(PTR(0), ARG(1), ARG(2)); return ARG(0); }
static int bios_memchr(void)
{
    char *p = memchr(PTR(0), ARG(1), ARG(2));

    return p ? ARG(0) + (p - PTR(0)) : 0;
}
static int bios_rand(void)
{
    rand_state = rand_state * 0x41C64E6D + 0x3039;
    return rand_state >> 16 & 0x7FFF;
}
static int bios_srand(void) { rand_state = ARG(0); return 0; }
static int bios_nothing(void) { return 0; }
static int bios_gpu_command(void) { gpu_gp0(ARG(0)); return 0; }
static int bios_boot_failure(void)
{
    fprintf(stderr, "BIOS: boot or disc failure %c %d\n", (char)ARG(0), (int)ARG(1));
    exit(1);
}
static int bios_card_no_answer(void) { deliver_event(EV_CARD, EV_TIMEOUT); return 1; }
static int bios_fail(void) { return -1; }

static int bios_deliver_event(void) { deliver_event(ARG(0), ARG(1)); return 0; }
static int bios_open_event(void)
{
    int i;

    for (i = 0; i < EVENT_COUNT; i++) {
        if (events[i].status == EV_FREE) {
            events[i].class = ARG(0);
            events[i].spec = ARG(1);
            events[i].mode = ARG(2);
            events[i].func = ARG(3);
            events[i].status = EV_DISABLED;
            return 0xF1000000 | i;
        }
    }
    return -1;
}
static int event_index(void) { return ARG(0) & 0xFFFF; }
static int bios_close_event(void) { events[event_index()].status = EV_FREE; return 1; }
static int bios_test_event(void)
{
    if (events[event_index()].status != EV_READY)
        return 0;
    events[event_index()].status = EV_ENABLED;
    return 1;
}
static int bios_wait_event(void)
{
    if (events[event_index()].mode == EV_MODE_CALLBACK)
        return 0;
    while (events[event_index()].status != EV_READY)
        hw_poll();
    events[event_index()].status = EV_ENABLED;
    return 1;
}
static int bios_enable_event(void)
{
    if (events[event_index()].status == EV_DISABLED)
        events[event_index()].status = EV_ENABLED;
    return 1;
}
static int bios_disable_event(void) { events[event_index()].status = EV_DISABLED; return 1; }
static int bios_undeliver_event(void)
{
    int i;

    for (i = 0; i < EVENT_COUNT; i++)
        if (events[i].status == EV_READY && events[i].class == ARG(0) && events[i].spec == ARG(1))
            events[i].status = EV_ENABLED;
    return 0;
}
static int bios_init_pad(void)
{
    pad_buffers[0] = ARG(0);
    pad_buffers[1] = ARG(2);
    return 1;
}
static int bios_start_pad(void)
{
    pad_started = 1;
    i_mask |= IRQ_VBLANK;
    return 1;
}
static int bios_stop_pad(void) { pad_started = 0; return 1; }
static int bios_old_pad_start(void) { pad_buffers[0] = ARG(1); pad_buffers[1] = 0; return bios_start_pad(); }
static int bios_old_pad_buttons(void) { return pad_buttons ^ 0xFFFF; }
static int bios_return_from_exception(void)
{
    if (in_exception)
        longjmp(exception_return, 1);
    return 0;
}
static int bios_default_exit(void) { custom_exit = 0; return 0; }
static int bios_custom_exit(void) { custom_exit = ARG(0); return 0; }
static int bios_exit(void) { exit(ARG(0)); }
static int bios_puts(void) { fputs(PTR(0), stderr); return 0; }
static int bios_change_clear_pad(void) { clear_pad = ARG(0); return 0; }

/* GetC0Table / GetB0Table: libetc and libcard copy fixes into the kernel's exception handler (C0 entry 6) and its
 * pad routine (B0 entry 5Bh). The PC build runs no kernel code, so every entry points at zeroed scratch space that
 * takes those copies harmlessly. libcard finds its spot through the lui/ori pair at 0x70/0x74 of the handler, so
 * that pair points at scratch too. */
static unsigned int c0_table[0x20], b0_table[0x60], kernel_scratch[0x800], patch_scratch[0x400];

static void fill_table(unsigned int *table, int n)
{
    int i;

    for (i = 0; i < n; i++)
        table[i] = (unsigned int)kernel_scratch;
    kernel_scratch[0x70 / 4] = (unsigned int)patch_scratch >> 16;
    kernel_scratch[0x74 / 4] = (unsigned int)patch_scratch & 0xFFFF;
}
static int bios_c0_table(void) { fill_table(c0_table, 0x20); return (int)c0_table; }
static int bios_b0_table(void) { fill_table(b0_table, 0x60); return (int)b0_table; }
static int bios_change_clear_root_counter(void)
{
    int old = clear_root_counter[ARG(0) & 3];

    clear_root_counter[ARG(0) & 3] = ARG(1);
    return old;
}

static const struct {
    unsigned int table, n;
    int (*fn)(void);
} functions[] = {
    {0xA0, 0x13, bios_setjmp},
    {0xA0, 0x17, bios_strcmp},
    {0xA0, 0x18, bios_strncmp},
    {0xA0, 0x1B, bios_strlen},
    {0xA0, 0x27, bios_bcopy},
    {0xA0, 0x2A, bios_memcpy},
    {0xA0, 0x2B, bios_memset},
    {0xA0, 0x2E, bios_memchr},
    {0xA0, 0x2F, bios_rand},
    {0xA0, 0x30, bios_srand},
    {0xA0, 0x39, bios_nothing},        /* InitHeap: the game never calls malloc */
    {0xA0, 0x3F, bios_printf},
    {0xA0, 0x44, bios_nothing},        /* FlushCache */
    {0xA0, 0x49, bios_gpu_command},    /* GPU_cw */
    {0xA0, 0x70, bios_nothing},        /* _bu_init */
    {0xA0, 0x72, bios_nothing},        /* CdRemove */
    {0xA0, 0x9F, bios_nothing},        /* SetMemSize */
    {0xA0, 0xA1, bios_boot_failure},
    {0xA0, 0xAB, bios_card_no_answer}, /* _card_info */
    {0xA0, 0xAC, bios_card_no_answer}, /* _card_async_load_directory */
    {0xA0, 0xAD, bios_nothing},        /* set_card_auto_format */
    {0xB0, 0x07, bios_deliver_event},
    {0xB0, 0x08, bios_open_event},
    {0xB0, 0x09, bios_close_event},
    {0xB0, 0x0A, bios_wait_event},
    {0xB0, 0x0B, bios_test_event},
    {0xB0, 0x0C, bios_enable_event},
    {0xB0, 0x0D, bios_disable_event},
    {0xB0, 0x12, bios_init_pad},
    {0xB0, 0x13, bios_start_pad},
    {0xB0, 0x14, bios_stop_pad},
    {0xB0, 0x15, bios_old_pad_start},
    {0xB0, 0x16, bios_old_pad_buttons},
    {0xB0, 0x17, bios_return_from_exception},
    {0xB0, 0x18, bios_default_exit},
    {0xB0, 0x19, bios_custom_exit},
    {0xB0, 0x20, bios_undeliver_event},
    {0xB0, 0x32, bios_fail},           /* FileOpen: no memory card files */
    {0xB0, 0x34, bios_fail},           /* FileRead */
    {0xB0, 0x35, bios_fail},           /* FileWrite */
    {0xB0, 0x36, bios_nothing},        /* FileClose */
    {0xB0, 0x38, bios_exit},
    {0xB0, 0x3F, bios_puts},
    {0xB0, 0x41, bios_nothing},        /* FormatDevice: fails */
    {0xB0, 0x42, bios_nothing},        /* firstfile: none */
    {0xB0, 0x43, bios_nothing},        /* nextfile: none */
    {0xB0, 0x45, bios_nothing},        /* FileDelete: fails */
    {0xB0, 0x4A, bios_nothing},        /* InitCard */
    {0xB0, 0x4B, bios_nothing},        /* StartCard */
    {0xB0, 0x4C, bios_nothing},        /* StopCard */
    {0xB0, 0x4E, bios_nothing},        /* write_card_sector: fails */
    {0xB0, 0x50, bios_nothing},        /* allow_new_card */
    {0xB0, 0x56, bios_c0_table},
    {0xB0, 0x57, bios_b0_table},
    {0xB0, 0x5B, bios_change_clear_pad},
    {0xC0, 0x0A, bios_change_clear_root_counter},
};

int bios_call(int table, int n)
{
    unsigned int i;

    for (i = 0; i < sizeof(functions) / sizeof(functions[0]); i++)
        if (functions[i].table == (unsigned int)table && functions[i].n == (unsigned int)n)
            return functions[i].fn();
    fprintf(stderr, "BIOS function %X(%02Xh) isn't implemented\n", table, n);
    exit(1);
}
