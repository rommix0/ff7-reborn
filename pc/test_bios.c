/* The BIOS in C: events, and an interrupt that runs the game's handler and leaves the interrupted code's registers as
 * they were. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "recomp.h"

Ctx ctx;
unsigned int cop0[32];
void hw_poll(void) {}
void recomp_missing(unsigned int addr) { fprintf(stderr, "missing %08X\n", addr); assert(0); }
void recomp_break(unsigned int code) { (void)code; }
void gte_cmd(unsigned int op) { (void)op; }
unsigned int gte_read(int reg) { return reg; }
void gte_write(int reg, unsigned int v) { (void)reg; (void)v; }
unsigned int gte_read_ctrl(int reg) { return reg; }
void gte_write_ctrl(int reg, unsigned int v) { (void)reg; (void)v; }
unsigned int io_read(unsigned int addr, int size) { (void)addr; (void)size; return 0; }
void io_write(unsigned int addr, unsigned int v, int size) { (void)addr; (void)v; (void)size; }
void gpu_gp0(unsigned int word) { (void)word; }

int bios_call(int table, int n);
void irq_raise(unsigned int bit);
extern unsigned int i_stat, i_mask;

static int event_calls, handler_ran;
static unsigned int handler_sp, handler_v0, event_sp;

static int on_event(void)
{
    event_sp = ctx.r[29];
    return ++event_calls;
}

static int call(int table, int n, unsigned int a0, unsigned int a1, unsigned int a2, unsigned int a3)
{
    ctx.r[4] = a0;
    ctx.r[5] = a1;
    ctx.r[6] = a2;
    ctx.r[7] = a3;
    return bios_call(table, n);
}

/* Stands in for libetc's handler: it trashes every register, as compiled code may, then leaves the way the game's
 * does, through ReturnFromException. */
static int interrupt_handler(void)
{
    int r;

    handler_ran++;
    handler_sp = ctx.r[29];
    handler_v0 = ctx.r[2]; /* setjmp returning again, from longjmp */
    i_stat &= ~1u; /* acknowledged, as the game's handler does */
    for (r = 1; r < 32; r++)
        ctx.r[r] = 0xDEAD0000 + r;
    ctx.hi = ctx.lo = 0xDEADDEAD;
    bios_call(0xB0, 0x17);
    return 0;
}

int main(void)
{
    static unsigned int exit_buf[12];
    unsigned int ev;
    int r;

    assert(mem_map() == 0);
    ctx.r[29] = 0x801FFF00;

    /* A callback event (EvMdINTR) runs on DeliverEvent; a flag event (EvMdNOINTR) is seen once by TestEvent. */
    ev = call(0xB0, 0x08, 0xF0000010, 0x0002, 0x1000, (unsigned int)on_event);
    assert(ev >> 24 == 0xF1);
    call(0xB0, 0x0C, ev, 0, 0, 0);
    call(0xB0, 0x07, 0xF0000010, 0x0002, 0, 0);
    assert(event_calls == 1);
    ev = call(0xB0, 0x08, 0xF0000011, 0x0004, 0x2000, 0);
    call(0xB0, 0x0C, ev, 0, 0, 0);
    assert(call(0xB0, 0x0B, ev, 0, 0, 0) == 0);
    call(0xB0, 0x07, 0xF0000011, 0x0004, 0, 0);
    assert(call(0xB0, 0x0B, ev, 0, 0, 0) == 1);
    assert(call(0xB0, 0x0B, ev, 0, 0, 0) == 0);

    /* interrupt_preserves_context */
    exit_buf[0] = (unsigned int)interrupt_handler;
    exit_buf[1] = 0x801FF000;
    call(0xB0, 0x19, (unsigned int)exit_buf, 0, 0, 0);
    i_mask = 1;
    cop0[12] = 0x401;
    for (r = 1; r < 32; r++)
        ctx.r[r] = 0x1000 + r;
    ctx.r[29] = 0x801FFF00;
    ctx.hi = 0x1111;
    ctx.lo = 0x2222;
    irq_raise(1);
    assert(handler_ran == 1 && handler_sp == 0x801FF000 && handler_v0 == 1);
    for (r = 1; r < 32; r++)
        assert(ctx.r[r] == (r == 29 ? 0x801FFF00u : 0x1000u + r));
    assert(ctx.hi == 0x1111 && ctx.lo == 0x2222 && cop0[12] == 0x401);

    /* In a critical section the interrupt waits, and arrives when it ends. */
    ctx.r[4] = 1;
    recomp_syscall(0);
    irq_raise(1);
    assert(handler_ran == 1);
    ctx.r[4] = 2;
    recomp_syscall(0);
    assert(handler_ran == 2);

    /* root_counter_callback_has_its_own_stack: the words below the interrupted code's $sp stay as they were, as
     * BATTLE.X's renderer needs (it saves registers there without moving $sp) */
    ev = call(0xB0, 0x08, 0xF2000000, 0x0002, 0x1000, (unsigned int)on_event);
    call(0xB0, 0x0C, ev, 0, 0, 0);
    i_mask = 0x11;
    ctx.r[29] = 0x801FFF00;
    irq_raise(0x10);
    assert(event_calls == 2 && (event_sp > 0x801FFF00 || event_sp < 0x801FF000));

    /* card_answers_through_events: _card_info and _card_load report a formatted card in slot 1 as done, and
     * write_card_sector (_card_clear) as done on the hardware-level event */
    remove("memcard1.mcd");
    {
        unsigned int done = call(0xB0, 0x08, 0xF4000001, 0x0004, 0x2000, 0);
        unsigned int hw_done = call(0xB0, 0x08, 0xF0000011, 0x0004, 0x2000, 0);
        static unsigned char sector[0x80], entry[40], data[0x2000];
        int fd;

        call(0xB0, 0x0C, done, 0, 0, 0);
        call(0xB0, 0x0C, hw_done, 0, 0, 0);
        call(0xA0, 0xAB, 0x00, 0, 0, 0);
        assert(call(0xB0, 0x0B, done, 0, 0, 0) == 1);
        call(0xA0, 0xAC, 0x00, 0, 0, 0);
        assert(call(0xB0, 0x0B, done, 0, 0, 0) == 1);
        assert(call(0xB0, 0x4E, 0x00, 0x3F, (unsigned int)sector, 0) == 1);
        assert(call(0xB0, 0x0B, hw_done, 0, 0, 0) == 1);

        /* card_files: the save menu's create, write, list, read and delete through the BIOS */
        fd = call(0xB0, 0x32, (unsigned int)"bu00:BASCUS-94163FF7-S01", 0x10200, 0, 0);
        assert(fd >= 2 && call(0xB0, 0x36, fd, 0, 0, 0) == fd);
        fd = call(0xB0, 0x32, (unsigned int)"bu00:BASCUS-94163FF7-S01", 2, 0, 0);
        data[5] = 0x5A;
        assert(call(0xB0, 0x35, fd, (unsigned int)data, sizeof data, 0) == sizeof data);
        call(0xB0, 0x36, fd, 0, 0, 0);
        assert(call(0xB0, 0x42, (unsigned int)"bu00:*", (unsigned int)entry, 0, 0) == (int)(unsigned int)entry);
        assert(!strcmp((char *)entry, "BASCUS-94163FF7-S01"));
        assert(!call(0xB0, 0x43, (unsigned int)entry, 0, 0, 0));
        fd = call(0xB0, 0x32, (unsigned int)"bu00:BASCUS-94163FF7-S01", 1, 0, 0);
        memset(data, 0, sizeof data);
        assert(call(0xB0, 0x34, fd, (unsigned int)data, 0x80, 0) == 0x80 && data[5] == 0x5A);
        call(0xB0, 0x36, fd, 0, 0, 0);
        assert(call(0xB0, 0x45, (unsigned int)"bu00:BASCUS-94163FF7-S01", 0, 0, 0) == 1);
        assert(!call(0xB0, 0x42, (unsigned int)"bu00:*", (unsigned int)entry, 0, 0));
    }
    remove("memcard1.mcd");

    /* printf_formats: the libraries' messages come out formatted (stderr goes to a file, so this test is last) */
    {
        char out[64] = {0};
        FILE *f;

        assert(freopen("test_bios.out", "w", stderr));
        call(0xA0, 0x3F, (unsigned int)"x=%08x %s %d%%\n", 0x1234, (unsigned int)"ok", 7);
        fclose(stderr);
        f = fopen("test_bios.out", "r");
        fread(out, 1, sizeof out - 1, f);
        fclose(f);
        remove("test_bios.out");
        if (strcmp(out, "x=00001234 ok 7%\n")) {
            printf("printf gave: %s\n", out);
            return 1;
        }
    }
    puts("ok");
    return 0;
}
