/* The hardware the libraries drive: DMA into a recording GPU, interrupt registers, and the vblank's pace. */
#include <assert.h>
#include <stdio.h>
#include "memory.h"
#include "recomp.h"

unsigned int i_stat, i_mask;
static unsigned int gp0[16], irqs[8];
static int gp0_count;
static double fake_now;

void gpu_gp0(unsigned int word) { gp0[gp0_count++ & 15] = word; }
void gpu_gp1(unsigned int word) { (void)word; }
unsigned int gpu_read(void) { return 0; }
unsigned int gpu_status(void) { return 0x1C000000; }
void irq_raise(unsigned int bit)
{
    int n;

    i_stat |= bit;
    for (n = 0; n < 8; n++)
        irqs[n] += bit >> n & 1;
}
void irq_check(void) {}
void pc_vblank(void) {}
void gpu_vblank(void) {}
void hw_set_clock(double (*now)(void));

static double ticking_clock(void) { return fake_now += 1e-6; }

#define DMA2_MADR 0x1F8010A0u
#define DMA2_BCR 0x1F8010A4u
#define DMA2_CHCR 0x1F8010A8u
#define DMA6_MADR 0x1F8010E0u
#define DMA6_BCR 0x1F8010E4u
#define DMA6_CHCR 0x1F8010E8u
#define DICR 0x1F8010F4u

int main(void)
{
    int poll;

    assert(mem_map() == 0);

    /* OT clear: 4 entries, MADR at the last one */
    mem_w32(DMA6_MADR, 0x0010000C);
    mem_w32(DMA6_BCR, 4);
    mem_w32(DMA6_CHCR, 0x11000002);
    assert(mem_r32(0x80100000) == 0x00FFFFFF);
    assert(mem_r32(0x80100004) == 0x00100000 && mem_r32(0x80100008) == 0x00100004);
    assert(mem_r32(0x8010000C) == 0x00100008);
    assert(!(mem_r32(DMA6_CHCR) & 0x01000000)); /* done */

    /* Two packets linked into the table, then the GPU's linked-list DMA from its last entry */
    mem_w32(0x80100100, 2 << 24 | 0x100004); /* in slot 2, before slot 1 */
    mem_w32(0x80100104, 0xAAAA0001);
    mem_w32(0x80100108, 0xAAAA0002);
    mem_w32(0x80100008, 0x100100);
    mem_w32(0x80100200, 1 << 24 | 0x100008); /* in slot 3 */
    mem_w32(0x80100204, 0xBBBB0001);
    mem_w32(0x8010000C, 0x100200);
    mem_w32(DICR, 0x00840000); /* master enable, IRQ for channel 2 */
    mem_w32(DMA2_MADR, 0x0010000C);
    mem_w32(DMA2_CHCR, 0x01000401);
    assert(gp0_count == 3 && gp0[0] == 0xBBBB0001 && gp0[1] == 0xAAAA0001 && gp0[2] == 0xAAAA0002);
    hw_poll();
    assert(irqs[3] == 1 && mem_r32(DICR) >> 31 == 1 && (mem_r32(DICR) >> 26 & 1));
    mem_w32(DICR, 0x04840000); /* acknowledge channel 2 */
    assert(mem_r32(DICR) >> 24 == 0);

    /* dma_unused_word_stays_put: a channel's fourth word (+0Ch) is no other register */
    mem_w32(DMA6_MADR - 0x10 + 0x0C, 0x12345678); /* channel 5's */
    assert(mem_r32(DMA6_MADR) != 0x12345678);
    mem_w32(DICR, 0x00840000);
    mem_w32(DMA6_MADR + 0x0C, 0xFFFFFFFF);
    assert(mem_r32(DICR) == 0x00840000);
    mem_w32(DICR, 0);

    /* I_STAT acknowledges by writing 0s; I_MASK is plain */
    i_stat = 0x9;
    mem_w16(0x1F801070, ~0x8u); /* acknowledges bit 3 only */
    assert(mem_r16(0x1F801070) == 0x1);
    mem_w32(0x1F801074, 0x7FF);
    assert(i_mask == 0x7FF);

    /* vblanks_follow_polls: a poll is a short loop's pass on the R3000 (libetc's VSync timeout counts 32768 of them
     * and expects a vblank in that time), so a second of passes makes 60 vblanks, however fast the host runs them */
    hw_set_clock(ticking_clock);
    irqs[0] = 0;
    for (poll = 0; poll < 33868800 / 32; poll++)
        hw_poll();
    assert(irqs[0] >= 59 && irqs[0] <= 61);
    /* frames_wait_for_the_host: and that second of passes doesn't end before a second of host time */
    assert(fake_now > 0.95);
    irqs[0] = 0;
    for (poll = 0; poll < 32768; poll++)
        hw_poll();
    assert(irqs[0] >= 1);

    /* GPU ports */
    gp0_count = 0;
    mem_w32(0x1F801810, 0xE1000000);
    assert(gp0_count == 1 && gp0[0] == 0xE1000000 && mem_r32(0x1F801814) == 0x1C000000);
    puts("ok");
    return 0;
}
