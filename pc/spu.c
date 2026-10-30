/* The SPU, as psx-spx describes it: 24 voices playing ADPCM from its 512 KiB of RAM under their ADSR envelopes, with
 * pitch modulation and noise, CD audio mixed in, and 44.1 kHz stereo out. ponytail: no reverb yet (its registers
 * are kept), no SPU interrupt, straight-line interpolation where the hardware's is Gaussian, and a volume sweep holds
 * the last fixed volume. */
#include <string.h>

#define SPU_BASE 0x1F801C00u
#define MAIN_L 0x1F801D80u
#define MAIN_R 0x1F801D82u
#define KON 0x1F801D88u
#define KOFF 0x1F801D8Cu
#define PMON 0x1F801D90u
#define NON 0x1F801D94u
#define ENDX 0x1F801D9Cu
#define TRANSFER 0x1F801DA6u
#define FIFO 0x1F801DA8u
#define SPUCNT 0x1F801DAAu
#define SPUSTAT 0x1F801DAEu
#define CD_L 0x1F801DB0u
#define CD_R 0x1F801DB2u
#define REG(addr) reg[((addr) - SPU_BASE) / 2]
#define VOICES 24
#define BLOCK_SAMPLES 28
#define CD_RING 0x8000 /* stereo frames: most of a second of XA */

enum { ATTACK, DECAY, SUSTAIN, RELEASE };

typedef struct {
    unsigned int addr, repeat;  /* the block playing and the one a loop goes back to, in SPU RAM */
    unsigned int counter;       /* the place in the block, in samples with a 12-bit fraction */
    int flags, repeat_written;  /* the block's loop flags; the game set the loop address since key on */
    short sample[BLOCK_SAMPLES + 1]; /* the block decoded, after the last sample of the one before */
    int phase, level, wait;     /* the envelope, and the samples until its next step */
} Voice;

static unsigned char spu_ram[0x80000];
static unsigned short reg[0x200];
static Voice voices[VOICES];
static int volumes[VOICES + 1][2]; /* each voice's then the main volume, left and right */
static unsigned int transfer, endx;
static int noise_level, noise_timer;
static short cd_ring[CD_RING][2];
static unsigned int cd_in, cd_out; /* frames queued and played, counted on from 0 */
static double cd_pos, cd_step;     /* between the frame at cd_out and the next, and the step for each output frame */

static void decode_block(Voice *v)
{
    static const int forward[5] = {0, 60, 115, 98, 122}, back[5] = {0, 0, -52, -55, -60};
    const unsigned char *block = spu_ram + v->addr;
    int shift = block[0] & 0xF, filter = block[0] >> 4 & 7, s1 = v->sample[BLOCK_SAMPLES],
        s2 = v->sample[BLOCK_SAMPLES - 1], s, j;

    if (shift > 12) /* 13 to 15 act as 9 */
        shift = 9;
    if (filter > 4)
        filter = 4;
    v->flags = block[1];
    if (v->flags & 4 && !v->repeat_written)
        v->repeat = v->addr;
    v->sample[0] = s1;
    for (j = 0; j < BLOCK_SAMPLES; j++) {
        s = ((short)((block[2 + j / 2] >> (j & 1) * 4 & 0xF) << 12) >> shift)
            + ((s1 * forward[filter] + s2 * back[filter] + 32) >> 6);
        s2 = s1;
        s1 = s < -0x8000 ? -0x8000 : s > 0x7FFF ? 0x7FFF : s;
        v->sample[j + 1] = s1;
    }
}

/* Off the end of a block: on to the next, or back to the loop's start. An end without a repeat silences the voice,
 * which plays on from the loop address. */
static void next_block(Voice *v, int n)
{
    if (!(v->flags & 1)) {
        v->addr = (v->addr + 16) & 0x7FFF0;
    } else {
        endx |= 1u << n;
        v->addr = v->repeat;
        if (!(v->flags & 2)) {
            v->phase = RELEASE;
            v->level = 0;
        }
    }
    decode_block(v);
}

static void key_on(int n)
{
    Voice *v = &voices[n];

    memset(v->sample, 0, sizeof v->sample);
    v->addr = reg[n * 8 + 3] * 8 & 0x7FFF0;
    v->counter = 0;
    v->repeat_written = 0;
    v->phase = ATTACK;
    v->level = v->wait = 0;
    endx &= ~(1u << n);
    decode_block(v);
}

/* One envelope step when its wait is over: shift and step set the rate; an exponential fall steps by a share of the
 * level, an exponential rise slows above 0x6000. */
static void envelope(Voice *v, int n)
{
    unsigned int adsr = reg[n * 8 + 4] | (unsigned int)reg[n * 8 + 5] << 16;
    int exponential, falling, shift, step;

    if (--v->wait > 0)
        return;
    switch (v->phase) {
    case ATTACK:
        exponential = adsr >> 15 & 1, falling = 0, shift = adsr >> 10 & 0x1F, step = 7 - (adsr >> 8 & 3);
        break;
    case DECAY:
        exponential = 1, falling = 1, shift = adsr >> 4 & 0xF, step = -8;
        break;
    case SUSTAIN:
        exponential = adsr >> 31, falling = adsr >> 30 & 1, shift = adsr >> 24 & 0x1F;
        step = falling ? -8 + (adsr >> 22 & 3) : 7 - (adsr >> 22 & 3);
        break;
    default:
        exponential = adsr >> 21 & 1, falling = 1, shift = adsr >> 16 & 0x1F, step = -8;
    }
    v->wait = 1 << (shift > 11 ? shift - 11 : 0);
    step *= 1 << (shift < 11 ? 11 - shift : 0);
    if (exponential && !falling && v->level > 0x6000)
        v->wait *= 4;
    if (exponential && falling)
        step = step * v->level >> 15;
    v->level += step;
    v->level = v->level < 0 ? 0 : v->level > 0x7FFF ? 0x7FFF : v->level;
    if (v->phase == ATTACK && v->level == 0x7FFF)
        v->phase = DECAY;
    else if (v->phase == DECAY && v->level <= (int)((adsr & 0xF) + 1) * 0x800)
        v->phase = SUSTAIN;
}

/* The noise generator: a 16-bit register shifting in the inverted parity of four of its bits, at SPUCNT's rate. */
static void noise_step(unsigned int cnt)
{
    int period = 0x20000 >> (cnt >> 10 & 0xF);

    noise_timer -= (cnt >> 8 & 3) + 4;
    if (noise_timer >= 0)
        return;
    noise_level = (noise_level << 1 | ((noise_level >> 15 ^ noise_level >> 12 ^ noise_level >> 11
                                        ^ noise_level >> 10 ^ 1) & 1)) & 0xFFFF;
    noise_timer += period;
    if (noise_timer < 0)
        noise_timer += period;
}

void spu_mix(short *lr, int frames)
{
    unsigned int cnt = REG(SPUCNT), pmon = REG(PMON) | (unsigned int)REG(PMON + 2) << 16,
                 noise = REG(NON) | (unsigned int)REG(NON + 2) << 16;
    int f, n, l, r, s, at, step, last, cd_l, cd_r;
    Voice *v;

    for (f = 0; f < frames; f++) {
        noise_step(cnt);
        l = r = last = 0;
        for (n = 0; n < VOICES; n++) {
            v = &voices[n];
            at = v->counter >> 12;
            s = noise >> n & 1 ? (short)noise_level
                               : v->sample[at] + ((v->sample[at + 1] - v->sample[at]) * (int)(v->counter & 0xFFF) >> 12);
            s = s * v->level >> 15;
            envelope(v, n);
            step = reg[n * 8 + 2] > 0x4000 ? 0x4000 : reg[n * 8 + 2];
            if (n && pmon >> n & 1) { /* modulated by the voice before's output */
                step = step * (last + 0x8000) >> 15;
                step = step > 0x4000 ? 0x4000 : step;
            }
            for (v->counter += step; v->counter >> 12 >= BLOCK_SAMPLES; v->counter -= BLOCK_SAMPLES << 12)
                next_block(v, n);
            last = s;
            l += s * volumes[n][0] >> 15;
            r += s * volumes[n][1] >> 15;
        }
        if (!(cnt & 0x4000)) /* muted, all but CD audio */
            l = r = 0;
        if (cd_out != cd_in) {
            short *a = cd_ring[cd_out % CD_RING], *b = cd_in - cd_out > 1 ? cd_ring[(cd_out + 1) % CD_RING] : a;

            cd_l = a[0] + (int)((b[0] - a[0]) * cd_pos);
            cd_r = a[1] + (int)((b[1] - a[1]) * cd_pos);
            if (cnt & 1) {
                l += cd_l * (short)REG(CD_L) >> 15;
                r += cd_r * (short)REG(CD_R) >> 15;
            }
            for (cd_pos += cd_step; cd_pos >= 1 && cd_out != cd_in; cd_pos -= 1)
                cd_out++;
        }
        l = l * volumes[VOICES][0] >> 15;
        r = r * volumes[VOICES][1] >> 15;
        lr[2 * f] = l < -0x8000 ? -0x8000 : l > 0x7FFF ? 0x7FFF : l;
        lr[2 * f + 1] = r < -0x8000 ? -0x8000 : r > 0x7FFF ? 0x7FFF : r;
    }
}

/* CD audio (a movie's XA sound) at its own rate; what doesn't fit is dropped. */
void spu_cd_input(const short *lr, int frames, int rate)
{
    int i;

    cd_step = rate / 44100.0;
    for (i = 0; i < frames && cd_in - cd_out < CD_RING; i++, cd_in++) {
        cd_ring[cd_in % CD_RING][0] = lr[2 * i];
        cd_ring[cd_in % CD_RING][1] = lr[2 * i + 1];
    }
}

void spu_dma(unsigned int *words, unsigned int count, int to_spu)
{
    for (; count; count--, words++, transfer += 4) {
        if (to_spu)
            memcpy(spu_ram + (transfer & 0x7FFFC), words, 4);
        else
            memcpy(words, spu_ram + (transfer & 0x7FFFC), 4);
    }
}

unsigned short spu_read(unsigned int addr)
{
    unsigned int i = (addr - SPU_BASE) / 2;

    if (i < VOICES * 8 && (i & 7) == 6)
        return voices[i / 8].level;
    if (i < VOICES * 8 && (i & 7) == 7)
        return voices[i / 8].repeat / 8;
    if (addr == ENDX || addr == ENDX + 2)
        return addr == ENDX ? endx & 0xFFFF : endx >> 16;
    if (addr == SPUSTAT) /* echoes SPUCNT's mode bits and is never busy */
        return REG(SPUCNT) & 0x3F;
    return reg[i];
}

void spu_write(unsigned int addr, unsigned short v)
{
    unsigned int i = (addr - SPU_BASE) / 2, keys;
    int n;

    reg[i] = v;
    if (i < VOICES * 8) {
        if ((i & 7) < 2 && !(v & 0x8000))
            volumes[i / 8][i & 1] = (short)(v << 1);
        if ((i & 7) == 6)
            voices[i / 8].level = v & 0x7FFF;
        if ((i & 7) == 7) {
            voices[i / 8].repeat = v * 8 & 0x7FFF0;
            voices[i / 8].repeat_written = 1;
        }
        return;
    }
    switch (addr) {
    case MAIN_L:
    case MAIN_R:
        if (!(v & 0x8000))
            volumes[VOICES][addr == MAIN_R] = (short)(v << 1);
        break;
    case KON:
    case KON + 2:
    case KOFF:
    case KOFF + 2:
        keys = addr & 2 ? (v & 0xFFu) << 16 : v;
        for (n = 0; n < VOICES; n++) {
            if (!(keys >> n & 1))
                continue;
            if (addr - KON < 4) {
                key_on(n);
            } else {
                voices[n].phase = RELEASE;
                voices[n].wait = 0;
            }
        }
        break;
    case TRANSFER:
        transfer = v * 8;
        break;
    case FIFO: /* a manual transfer, a halfword at a time */
        memcpy(spu_ram + (transfer & 0x7FFFE), &v, 2);
        transfer += 2;
        break;
    }
}
