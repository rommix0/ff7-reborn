/* The SPU: samples uploaded to its RAM, voices looping through ADPCM blocks under their envelopes, and CD audio mixed
 * in, as psx-spx describes them. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

unsigned short spu_read(unsigned int addr);
void spu_write(unsigned int addr, unsigned short v);
void spu_dma(unsigned int *words, unsigned int count, int to_spu);
void spu_mix(short *lr, int frames);
void spu_cd_input(const short *lr, int frames, int rate);

#define VOICE(n, reg) (0x1F801C00u + (n) * 0x10 + (reg))
#define VOL_L 0x0
#define VOL_R 0x2
#define PITCH 0x4
#define START 0x6
#define ADSR_LO 0x8
#define ADSR_HI 0xA
#define LEVEL 0xC
#define MAIN_L 0x1F801D80u
#define MAIN_R 0x1F801D82u
#define KON 0x1F801D88u
#define KOFF 0x1F801D8Cu
#define ENDX 0x1F801D9Cu
#define TRANSFER 0x1F801DA6u
#define FIFO 0x1F801DA8u
#define SPUCNT 0x1F801DAAu
#define CD_L 0x1F801DB0u
#define CD_R 0x1F801DB2u

/* One 16-byte ADPCM block at addr: shift and filter, flags, its first four nibbles, then the rest. */
static void upload_block(unsigned int addr, int header, int flags, int first, int rest)
{
    int i;

    spu_write(TRANSFER, addr / 8);
    spu_write(FIFO, header | flags << 8);
    spu_write(FIFO, first);
    for (i = 1; i < 7; i++)
        spu_write(FIFO, rest);
}

static void key_on(int pitch)
{
    spu_write(VOICE(0, PITCH), pitch);
    spu_write(KON, 1);
}

static int scaled(int sample, int level)
{
    return ((sample * level >> 15) * 0x7FFE >> 15) * 0x7FFE >> 15;
}

int main(void)
{
    static short out[400][2];
    unsigned int words[2] = {0x11223344, 0x55667788}, back[2];
    short cd[100][2];
    int i, level;

    spu_write(SPUCNT, 0xC000); /* on, unmuted */
    spu_write(MAIN_L, 0x3FFF);
    spu_write(MAIN_R, 0x3FFF);
    spu_write(VOICE(0, VOL_L), 0x3FFF);
    spu_write(VOICE(0, START), 0x1000 / 8);
    spu_write(VOICE(0, ADSR_LO), 0x00FF); /* linear attack +7 at every sample, a slow decay to the top */
    spu_write(VOICE(0, ADSR_HI), 0x1F00); /* a sustain too slow to move, and a release of -8 at every sample */

    /* transfers_round_trip: words in by DMA from the transfer address, back out from it */
    spu_write(TRANSFER, 0x2000 / 8);
    spu_dma(words, 2, 1);
    spu_write(TRANSFER, 0x2000 / 8);
    spu_dma(back, 2, 0);
    assert(back[0] == words[0] && back[1] == words[1]);

    /* end_flag_after_both_blocks: A (loop start) then B (end, repeat), 28 samples each at pitch 0x1000 */
    upload_block(0x1000, 0x00, 4, 0x7777, 0x7777);
    upload_block(0x1010, 0x00, 3, 0x7777, 0x7777);
    key_on(0x1000);
    assert(!(spu_read(ENDX) & 1));
    spu_mix(out[0], 55);
    assert(!(spu_read(ENDX) & 1));
    spu_mix(out[0], 1);
    assert(spu_read(ENDX) & 1);

    /* pitch_steps_through_samples: twice the pitch, through both blocks in half the time; key on clears the flag */
    key_on(0x2000);
    assert(!(spu_read(ENDX) & 1));
    spu_mix(out[0], 27);
    assert(!(spu_read(ENDX) & 1));
    spu_mix(out[0], 1);
    assert(spu_read(ENDX) & 1);

    /* sustained_voice_is_its_sample_times_its_volumes: 0x7000 samples (nibble 7, shift 0), left only */
    spu_mix(out[0], 200);
    level = spu_read(VOICE(0, LEVEL));
    assert(level >= 0x4000);
    spu_mix(out[0], 100);
    assert(spu_read(VOICE(0, LEVEL)) == level);
    for (i = 0; i < 100; i++)
        assert(out[i][0] == scaled(0x7000, level) && !out[i][1]);

    /* key_off_releases_to_silence */
    spu_write(KOFF, 1);
    spu_mix(out[0], 10);
    assert(!spu_read(VOICE(0, LEVEL)) && !out[9][0]);

    /* filter_predicts_from_the_last_sample: filter 1 is 60/64 of the one before; one nibble of 1 then zeros */
    upload_block(0x1020, 0x10, 7, 0x0001, 0);
    spu_write(VOICE(0, START), 0x1020 / 8);
    key_on(0x1000);
    spu_mix(out[0], 20);
    spu_mix(out[0], 6);
    for (i = 1; i < 6; i++)
        assert(abs(out[i][0] * 64 - out[i - 1][0] * 60) <= 4 * 64);
    assert(out[0][0] > 0x100 && out[5][0] < out[0][0]);
    spu_write(KOFF, 1);
    spu_mix(out[0], 10);

    /* cd_audio_mixes_at_its_volume: at 44.1 kHz it plays sample for sample */
    spu_write(SPUCNT, 0xC001);
    spu_write(CD_L, 0x7FFF);
    spu_write(CD_R, 0x7FFF);
    for (i = 0; i < 100; i++)
        cd[i][0] = 1000, cd[i][1] = -2000;
    spu_cd_input(cd[0], 100, 44100);
    spu_mix(out[0], 100);
    assert(abs(out[50][0] - 998) <= 2 && abs(out[50][1] + 2000) <= 2);
    spu_mix(out[0], 1);
    assert(!out[0][0]);

    /* cd_audio_is_resampled: 100 frames at 22.05 kHz last 200 */
    spu_cd_input(cd[0], 100, 22050);
    spu_mix(out[0], 220);
    assert(out[190][0] && !out[210][0]);
    puts("ok");
    return 0;
}
