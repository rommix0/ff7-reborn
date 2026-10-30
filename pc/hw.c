/* The PS1 hardware Sony's libraries drive, as psx-spx's I/O map describes it: the interrupt registers, DMA to the
 * GPU, the SPU and the OT clear, the root counters, the SPU's registers and RAM, and the vblank at 59.94 Hz of real
 * time. Registers nothing here models read back what was last written. */
#include <math.h>
#include <string.h>
#include "recomp.h"

#ifdef _WIN32
#include <windows.h>

static double host_seconds(void)
{
    LARGE_INTEGER now, rate;

    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&rate);
    return (double)now.QuadPart / rate.QuadPart;
}
#else
#include <time.h>

static double host_seconds(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec / 1e9;
}
#endif

void gpu_gp0(unsigned int word);
void gpu_gp1(unsigned int word);
unsigned int gpu_read(void);
unsigned int gpu_status(void);
void gpu_vblank(void); /* the field on screen changes */

#define I_STAT 0x1F801070u
#define I_MASK 0x1F801074u
#define DMA_BASE 0x1F801080u /* channel n at + 0x10 * n: MADR, BCR, CHCR and an unused word */
#define DICR 0x1F8010F4u
#define TIMER_BASE 0x1F801100u /* timer n at + 0x10 * n: value, mode, target */
#define GP0 0x1F801810u
#define GP1 0x1F801814u
#define SPU_BASE 0x1F801C00u
#define SPU_TRANSFER_ADDR 0x1F801DA6u
#define SPU_FIFO 0x1F801DA8u
#define SPU_CNT 0x1F801DAAu
#define SPU_STAT 0x1F801DAEu
#define IRQ_VBLANK 0x01u
#define IRQ_DMA 0x08u
#define IRQ_TIMER0 0x10u
#define CPU_HZ 33868800.0
#define VBLANK_SECONDS (1 / 59.94)
/* The PS1's time is counted in polls, one per pass of a loop: the libraries' spin timeouts count passes (libetc's
 * VSync allows 32768, about 1.7 frames of a ~30-cycle pass), and recompiled code makes them far faster than the R3000
 * did. 32 cycles keeps a vblank inside that window. */
#define POLL_CYCLES 32
#define POLL_STRIDE 256 /* hw_poll advances the PS1's time once per this many calls */

unsigned char spu_ram[0x80000];
static unsigned char io_mem[IO_END - IO_START];
static unsigned int dma[7][4], dicr, dma_irq_pending;
static unsigned int spu_transfer;
static struct {
    double count;
    unsigned int mode, target;
} timers[3];
static double (*clock_now)(void) = host_seconds;
static double ps1_time, next_vblank = VBLANK_SECONDS, host_start = -HUGE_VAL;
static unsigned int polls;

void hw_set_clock(double (*now)(void))
{
    clock_now = now;
    host_start = -HUGE_VAL;
}

static unsigned int *dma_word(unsigned int madr)
{
    return mem_ptr(RAM_START + (madr & 0x1FFFFC));
}

static void dma_run(int ch)
{
    unsigned int madr = dma[ch][0], bcr = dma[ch][1], chcr = dma[ch][2], words, header, n, guard;
    int to_device = chcr & 1, sync = chcr >> 9 & 3;

    words = sync == 0 ? (bcr & 0xFFFF ? bcr & 0xFFFF : 0x10000) : (bcr & 0xFFFF) * (bcr >> 16 ? bcr >> 16 : 1);
    if (ch == 6) { /* OT clear: from the last entry down, each pointing at the one before */
        for (n = 0; n < words; n++, madr -= 4)
            *dma_word(madr) = n + 1 == words ? 0x00FFFFFF : (madr - 4) & 0x1FFFFF;
    } else if (ch == 2 && sync == 2) { /* linked list: a word count and a link per node */
        for (guard = 0; guard < 0x100000; guard++) {
            header = *dma_word(madr);
            for (n = 1; n <= header >> 24; n++)
                gpu_gp0(*dma_word(madr + 4 * n));
            if (header & 0x800000)
                break;
            madr = header & 0x1FFFFC;
        }
    } else if (ch == 2) {
        for (n = 0; n < words; n++, madr += 4) {
            if (to_device)
                gpu_gp0(*dma_word(madr));
            else
                *dma_word(madr) = gpu_read();
        }
    } else if (ch == 4) {
        for (n = 0; n < words; n++, madr += 4, spu_transfer += 4) {
            if (to_device)
                memcpy(spu_ram + (spu_transfer & 0x7FFFC), dma_word(madr), 4);
            else
                memcpy(dma_word(madr), spu_ram + (spu_transfer & 0x7FFFC), 4);
        }
    }
    dma[ch][2] &= ~0x11000000u; /* start and trigger clear: done at once */
    if (dicr >> (16 + ch) & 1) {
        dicr |= 1u << (24 + ch);
        dma_irq_pending = 1;
    }
}

static unsigned int dicr_value(void)
{
    unsigned int flags = dicr >> 24 & 0x7F, enabled = dicr >> 16 & 0x7F;
    int master = (dicr & 0x8000) || ((dicr & 0x800000) && (flags & enabled));

    return (dicr & 0x7FFFFFFF) | (unsigned int)master << 31;
}

static double timer_hz(int n)
{
    unsigned int source = timers[n].mode >> 8 & 3;

    if (n == 1 && source & 1)
        return 263 * 59.94; /* hblank */
    if (n == 2 && source & 2)
        return CPU_HZ / 8;
    return CPU_HZ;
}

static void timers_advance(double seconds)
{
    unsigned int end;
    int n;

    for (n = 0; n < 3; n++) {
        timers[n].count += seconds * timer_hz(n);
        end = timers[n].mode & 0x08 ? timers[n].target : 0xFFFF;
        if (end && timers[n].count >= end + 1) {
            timers[n].count = (unsigned int)timers[n].count % (end + 1);
            timers[n].mode |= timers[n].mode & 0x08 ? 0x0800 : 0x1000;
            if (timers[n].mode & (timers[n].mode & 0x08 ? 0x10 : 0x20))
                irq_raise(IRQ_TIMER0 << n);
        }
    }
}

void hw_poll(void)
{
    const double stride = POLL_STRIDE * POLL_CYCLES / CPU_HZ;
    double now;

    if (dma_irq_pending && dicr_value() >> 31) {
        dma_irq_pending = 0;
        irq_raise(IRQ_DMA);
    }
    if (i_stat & i_mask)
        irq_check();
    if (++polls % POLL_STRIDE)
        return;
    ps1_time += stride;
    timers_advance(stride);
    if (ps1_time < next_vblank)
        return;
    next_vblank += VBLANK_SECONDS;
    gpu_vblank();
    irq_raise(IRQ_VBLANK);
    pc_vblank();
    /* A frame takes real time: wait for the host to catch up, or, after a long stall (a load), start counting again
     * rather than rush. ponytail: spins a core while waiting; sleep when that matters. */
    now = clock_now();
    if (now - host_start - ps1_time > 0.25)
        host_start = now - ps1_time;
    while (now - host_start < ps1_time)
        now = clock_now();
}

/* A register's value as a word; a 16- or 8-bit access takes or replaces its part of it. */
static unsigned int word_read(unsigned int word)
{
    int ch = (word - DMA_BASE) / 0x10, n = (word - TIMER_BASE) / 0x10;

    if (word == I_STAT)
        return i_stat;
    if (word == I_MASK)
        return i_mask;
    if (word == DICR)
        return dicr_value();
    if (word >= DMA_BASE && word < DICR - 4)
        return dma[ch][(word - DMA_BASE) % 0x10 / 4];
    if (word >= TIMER_BASE && word < TIMER_BASE + 0x30) {
        switch (word & 0xF) {
        case 0:
            return (unsigned int)timers[n].count & 0xFFFF;
        case 4: {
            unsigned int mode = timers[n].mode;

            timers[n].mode &= ~0x1800u; /* reaching target or 0xFFFF reads once */
            return mode;
        }
        case 8:
            return timers[n].target;
        }
    }
    if (word == GP0)
        return gpu_read();
    if (word == GP1)
        return gpu_status();
    if (word == (SPU_STAT & ~3u)) /* SPUSTAT echoes SPUCNT's mode bits and is never busy */
        return (unsigned int)(*(unsigned short *)&io_mem[SPU_CNT - IO_START] & 0x3F) << 16
            | *(unsigned short *)&io_mem[(SPU_STAT & ~3u) - IO_START];
    return *(unsigned int *)&io_mem[word - IO_START];
}

static void word_write(unsigned int word, unsigned int v, unsigned int mask)
{
    int ch = (word - DMA_BASE) / 0x10, n = (word - TIMER_BASE) / 0x10;
    unsigned int merged = (word_read(word) & ~mask) | (v & mask);

    if (word == I_STAT) {
        i_stat &= v | ~mask; /* writing 0 acknowledges */
        return;
    }
    if (word == I_MASK) {
        i_mask = merged & 0x7FF;
        irq_check();
        return;
    }
    if (word == DICR) {
        dicr = (merged & 0x00FFFFFF) | (dicr & ~(v & mask) & 0x7F000000); /* flags clear by writing 1 */
        return;
    }
    if (word >= DMA_BASE && word < DICR - 4) {
        dma[ch][(word - DMA_BASE) % 0x10 / 4] = merged;
        if ((word & 0xF) == 8 && merged & 0x01000000)
            dma_run(ch);
        return;
    }
    if (word >= TIMER_BASE && word < TIMER_BASE + 0x30) {
        switch (word & 0xF) {
        case 0:
            timers[n].count = merged & 0xFFFF;
            break;
        case 4:
            timers[n].mode = (merged & 0x3FF) | 0x400; /* writing the mode restarts the count */
            timers[n].count = 0;
            break;
        case 8:
            timers[n].target = merged & 0xFFFF;
            break;
        }
        return;
    }
    if (word == GP0) {
        gpu_gp0(v);
        return;
    }
    if (word == GP1) {
        gpu_gp1(v);
        return;
    }
    if (word == (SPU_FIFO & ~3u) && mask & 0xFFFF) { /* manual transfer, a halfword at a time */
        memcpy(spu_ram + (spu_transfer & 0x7FFFE), &v, 2);
        spu_transfer += 2;
    }
    *(unsigned int *)&io_mem[word - IO_START] = merged;
    if (word == (SPU_TRANSFER_ADDR & ~3u) && mask >> 16)
        spu_transfer = (merged >> 16) * 8;
}

unsigned int io_read(unsigned int addr, int size)
{
    unsigned int shift = (addr & 3) * 8, v = word_read(addr & ~3u) >> shift;

    return size == 4 ? v : v & ((1u << size * 8) - 1);
}

void io_write(unsigned int addr, unsigned int v, int size)
{
    unsigned int shift = (addr & 3) * 8, mask = size == 4 ? 0xFFFFFFFFu : ((1u << size * 8) - 1) << shift;

    word_write(addr & ~3u, v << shift, mask);
}
