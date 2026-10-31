/* The SPU, as psx-spx describes it: 24 voices playing ADPCM from its 512 KiB of RAM under their ADSR envelopes, with
 * pitch modulation, noise, volume sweeps and the interrupt at an address; CD audio mixed in; the reverb on its work
 * area at 22.05 kHz; and 44.1 kHz stereo out. The interpolation's weights and the low-pass around the reverb are our
 * own curves (the same as tools/akao_synth.py's), close to the hardware's tables but not them. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SPU_BASE 0x1F801C00u
#define MAIN_L 0x1F801D80u
#define MAIN_R 0x1F801D82u
#define REVERB_L 0x1F801D84u
#define KON 0x1F801D88u
#define KOFF 0x1F801D8Cu
#define PMON 0x1F801D90u
#define NON 0x1F801D94u
#define EON 0x1F801D98u
#define ENDX 0x1F801D9Cu
#define REVERB_BASE 0x1F801DA2u
#define IRQ_ADDR 0x1F801DA4u
#define TRANSFER 0x1F801DA6u
#define FIFO 0x1F801DA8u
#define SPUCNT 0x1F801DAAu
#define SPUSTAT 0x1F801DAEu
#define CD_L 0x1F801DB0u
#define CD_R 0x1F801DB2u
#define MAIN_NOW 0x1F801DB8u     /* the main volume as it is now, left and right */
#define REVERB_REGS 0x1F801DC0u  /* dAPF1, dAPF2, vIIR, vCOMB1-4, vWALL, vAPF1, vAPF2, the addresses, vLIN, vRIN */
#define VOICE_NOW 0x1F801E00u    /* each voice's volumes as they are now */
#define REG(addr) reg[((addr) - SPU_BASE) / 2]
#define REVERB(n) reg[(REVERB_REGS - SPU_BASE) / 2 + (n)]
#define VOICES 24
#define BLOCK_SAMPLES 28
#define CD_RING 0x8000 /* stereo frames: most of a second of XA */
#define TAPS 39        /* the low-pass around the reverb */

enum { ATTACK, DECAY, SUSTAIN, RELEASE };

typedef struct {
    int level, wait; /* and the samples until its next step */
} Envelope;

typedef struct {
    unsigned int addr, repeat;  /* the block playing and the one a loop goes back to, in SPU RAM */
    unsigned int counter;       /* the place in the block, in samples with a 12-bit fraction */
    int flags, repeat_written;  /* the block's loop flags; the game set the loop address since key on */
    short sample[BLOCK_SAMPLES + 3]; /* the block decoded, after the last three samples of the one before */
    int phase;
    Envelope adsr;
} Voice;

static unsigned short spu_ram[0x40000];
static unsigned short reg[0x200];
static Voice voices[VOICES];
static int volumes[VOICES + 1][2]; /* each voice's then the main volume, left and right, as they are now */
static Envelope sweeps[VOICES + 1][2];
static unsigned int transfer, endx;
static int irq_flag, irq_raised; /* SPUSTAT's bit 6, and whether it was set since spu_mix last returned */
static int noise_level, noise_timer;
static short cd_ring[CD_RING][2];
static unsigned int cd_in, cd_out; /* frames queued and played, counted on from 0 */
static double cd_pos, cd_step;     /* between the frame at cd_out and the next, and the step for each output frame */
static short gauss[256][4];        /* the interpolation's weights for each step between samples, oldest sample first */
static double lowpass[TAPS];
static int reverb_in[TAPS][2], reverb_out[TAPS][2]; /* the send, and the reverb's output at every other frame */
static int reverb_frame, reverb_odd;                /* the newest in those rings */
static unsigned int reverb_at;                      /* the work area's current address */

static int clamp16(int x)
{
    return x < -0x8000 ? -0x8000 : x > 0x7FFF ? 0x7FFF : x;
}

/* An access to SPU RAM: the interrupt, if it covers the IRQ address. */
static void touch(unsigned int addr, unsigned int size)
{
    unsigned int at = REG(IRQ_ADDR) * 8;

    if (REG(SPUCNT) & 0x40 && !irq_flag && at >= addr && at < addr + size)
        irq_flag = irq_raised = 1;
}

/* The curves, once: the interpolation is a Gaussian (sigma 0.85 samples) under a (1 - (d/2)^2)^3 window, each step's
 * weights adding up to 0x7F80 as the hardware's do; the low-pass is a windowed sinc (Kaiser, beta 13) cut off at
 * 11.025 kHz, the hardware's half-band in shape. */
static void make_curves(void)
{
    double d[4], sum, f, x, s, i0, i0_beta, term, power;
    int i, k;

    for (i = 0; i < 256; i++) {
        f = (i + 0.5) / 256;
        d[0] = 1 + f, d[1] = f, d[2] = 1 - f, d[3] = 2 - f;
        for (sum = k = 0; k < 4; k++)
            sum += d[k] = exp(-d[k] * d[k] / (2 * 0.85 * 0.85)) * pow(1 - d[k] * d[k] / 4, 3);
        for (k = 0; k < 4; k++)
            gauss[i][k] = (short)lround(d[k] / sum * 0x7F80);
    }
    for (sum = i = 0; i < TAPS; i++) {
        x = i - TAPS / 2;
        s = sqrt(1 - x * x / (TAPS / 2 * (TAPS / 2)));
        /* the Bessel function I0 at beta * s and at beta: the sum of ((z/2)^k / k!)^2 */
        for (i0 = i0_beta = term = power = 1, k = 1; k < 30; k++) {
            term *= 13.0 / 2 / k;
            power *= s;
            i0_beta += term * term;
            i0 += term * power * term * power;
        }
        sum += lowpass[i] = (x ? sin(M_PI * x / 2) / (M_PI * x / 2) : 1) / 2 * i0 / i0_beta;
    }
    for (i = 0; i < TAPS; i++)
        lowpass[i] /= sum;
}

static void decode_block(Voice *v)
{
    static const int forward[5] = {0, 60, 115, 98, 122}, back[5] = {0, 0, -52, -55, -60};
    const unsigned char *block = (unsigned char *)spu_ram + v->addr;
    int shift = block[0] & 0xF, filter = block[0] >> 4 & 7, s1 = v->sample[BLOCK_SAMPLES + 2],
        s2 = v->sample[BLOCK_SAMPLES + 1], s, j;

    if (shift > 12) /* 13 to 15 act as 9 */
        shift = 9;
    if (filter > 4)
        filter = 4;
    touch(v->addr, 16);
    v->flags = block[1];
    if (v->flags & 4 && !v->repeat_written)
        v->repeat = v->addr;
    memcpy(v->sample, v->sample + BLOCK_SAMPLES, 3 * sizeof v->sample[0]);
    for (j = 0; j < BLOCK_SAMPLES; j++) {
        s = ((short)((block[2 + j / 2] >> (j & 1) * 4 & 0xF) << 12) >> shift)
            + ((s1 * forward[filter] + s2 * back[filter] + 32) >> 6);
        s2 = s1;
        s1 = clamp16(s);
        v->sample[j + 3] = s1;
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
            v->adsr.level = 0;
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
    v->adsr.level = v->adsr.wait = 0;
    endx &= ~(1u << n);
    decode_block(v);
}

/* One step of an ADSR phase or a volume sweep when its wait is over: shift and step set the rate; an exponential
 * fall steps by a share of the level, an exponential rise slows above 0x6000. */
static void envelope_step(Envelope *e, int exponential, int falling, int shift, int step)
{
    if (--e->wait > 0)
        return;
    e->wait = 1 << (shift > 11 ? shift - 11 : 0);
    step *= 1 << (shift < 11 ? 11 - shift : 0);
    if (exponential && !falling && e->level > 0x6000)
        e->wait *= 4;
    if (exponential && falling)
        step = step * e->level >> 15;
    e->level += step;
    e->level = e->level < 0 ? 0 : e->level > 0x7FFF ? 0x7FFF : e->level;
}

static void envelope(Voice *v, int n)
{
    unsigned int adsr = reg[n * 8 + 4] | (unsigned int)reg[n * 8 + 5] << 16;
    int falling = adsr >> 30 & 1;

    switch (v->phase) {
    case ATTACK:
        envelope_step(&v->adsr, adsr >> 15 & 1, 0, adsr >> 10 & 0x1F, 7 - (adsr >> 8 & 3));
        break;
    case DECAY:
        envelope_step(&v->adsr, 1, 1, adsr >> 4 & 0xF, -8);
        break;
    case SUSTAIN:
        envelope_step(&v->adsr, adsr >> 31, falling, adsr >> 24 & 0x1F,
                      falling ? -8 + (int)(adsr >> 22 & 3) : 7 - (int)(adsr >> 22 & 3));
        break;
    default:
        envelope_step(&v->adsr, adsr >> 21 & 1, 1, adsr >> 16 & 0x1F, -8);
    }
    if (v->phase == ATTACK && v->adsr.level == 0x7FFF)
        v->phase = DECAY;
    else if (v->phase == DECAY && v->adsr.level <= (int)((adsr & 0xF) + 1) * 0x800)
        v->phase = SUSTAIN;
}

/* A volume register written: fixed, or a sweep from where the volume is now. */
static void set_volume(int n, int side, unsigned short v)
{
    if (!(v & 0x8000)) {
        volumes[n][side] = (short)(v << 1);
        return;
    }
    sweeps[n][side].level = abs(volumes[n][side]) > 0x7FFF ? 0x7FFF : abs(volumes[n][side]);
    sweeps[n][side].wait = 0;
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

/* The work area's sample at offset bytes from the current address; the area runs from REVERB_BASE to the end of RAM.
 * psx-spx doesn't know which of the reverb's reads and writes raise the interrupt, so all of them do. */
static short *work(int offset)
{
    unsigned int base = REG(REVERB_BASE) * 8;
    int size = 0x80000 - base, at = ((int)(reverb_at - base) + offset) % size;

    at = base + (at < 0 ? at + size : at);
    touch(at, 2);
    return (short *)spu_ram + at / 2;
}

/* One reverb sample a side (psx-spx's formula): reflections off the same side and the other, four comb taps, two
 * all-pass filters. Only SPUCNT's bit 7 lets it write its work area. */
static void reverb_step(const int in[2], int out[2])
{
    static const unsigned char reflections[4][3] = {{10, 16, 0}, {11, 17, 1}, {18, 25, 0}, {19, 24, 1}}; /* m, d, side */
    static const unsigned char combs[2][4] = {{12, 14, 20, 22}, {13, 15, 21, 23}}, apfs[2][2] = {{26, 28}, {27, 29}};
    int writes = REG(SPUCNT) & 0x80, k, side, x, prev, back, v;

    for (k = 0; k < 4; k++) {
        side = reflections[k][2];
        prev = *work(REVERB(reflections[k][0]) * 8 - 2);
        x = clamp16((in[side] * (short)REVERB(30 + side) >> 15)
                    + (*work(REVERB(reflections[k][1]) * 8) * (short)REVERB(7) >> 15) - prev);
        if (writes)
            *work(REVERB(reflections[k][0]) * 8) = clamp16((x * (short)REVERB(2) >> 15) + prev);
    }
    for (side = 0; side < 2; side++) {
        for (x = k = 0; k < 4; k++)
            x += *work(REVERB(combs[side][k]) * 8) * (short)REVERB(3 + k) >> 15;
        x = clamp16(x);
        for (k = 0; k < 2; k++) {
            v = (short)REVERB(8 + k);
            back = *work(REVERB(apfs[side][k]) * 8 - REVERB(k) * 8);
            x = clamp16(x - (back * v >> 15));
            if (writes)
                *work(REVERB(apfs[side][k]) * 8) = x;
            x = clamp16((x * v >> 15) + back);
        }
        out[side] = x * (short)REG(REVERB_L + side * 2) >> 15;
    }
    reverb_at = (reverb_at + 2) & 0x7FFFE;
    if (reverb_at < REG(REVERB_BASE) * 8u)
        reverb_at = REG(REVERB_BASE) * 8;
}

/* frames of 44.1 kHz stereo into lr. Nonzero if the interrupt went off. */
int spu_mix(short *lr, int frames)
{
    unsigned int cnt = REG(SPUCNT), pmon = REG(PMON) | (unsigned int)REG(PMON + 2) << 16,
                 noise = REG(NON) | (unsigned int)REG(NON + 2) << 16, eon = REG(EON) | (unsigned int)REG(EON + 2) << 16;
    int f, n, k, side, l, r, s, at, step, last, cd_l, cd_r, raised, wet[2];
    double sent[2], heard[2];
    const short *w;
    Voice *v;
    Envelope *e;

    if (!gauss[0][1])
        make_curves();
    for (f = 0; f < frames; f++) {
        for (n = 0; n <= VOICES; n++) {
            for (side = 0; side < 2; side++) {
                unsigned int vol = n < VOICES ? reg[n * 8 + side] : REG(MAIN_L + side * 2);

                if (!(vol & 0x8000))
                    continue;
                e = &sweeps[n][side];
                envelope_step(e, vol >> 14 & 1, vol >> 13 & 1, vol >> 2 & 0x1F,
                              vol & 0x2000 ? -8 + (int)(vol & 3) : 7 - (int)(vol & 3));
                /* psx-spx: a negative phase heads for -0x7FFF, but leaves an exponential decrease alone */
                volumes[n][side] = vol & 0x1000 && (vol & 0x6000) != 0x6000 ? -e->level : e->level;
            }
        }
        noise_step(cnt);
        l = r = last = 0;
        reverb_frame = (reverb_frame + 1) % TAPS;
        reverb_in[reverb_frame][0] = reverb_in[reverb_frame][1] = 0;
        for (n = 0; n < VOICES; n++) {
            v = &voices[n];
            at = v->counter >> 12;
            w = gauss[v->counter >> 4 & 0xFF];
            s = noise >> n & 1 ? (short)noise_level
                               : (v->sample[at] * w[0] + v->sample[at + 1] * w[1] + v->sample[at + 2] * w[2]
                                  + v->sample[at + 3] * w[3]) >> 15;
            s = s * v->adsr.level >> 15;
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
            if (eon >> n & 1) {
                reverb_in[reverb_frame][0] += s * volumes[n][0] >> 15;
                reverb_in[reverb_frame][1] += s * volumes[n][1] >> 15;
            }
        }
        cd_l = cd_r = 0;
        if (cd_out != cd_in) {
            short *a = cd_ring[cd_out % CD_RING], *b = cd_in - cd_out > 1 ? cd_ring[(cd_out + 1) % CD_RING] : a;

            if (cnt & 1) {
                cd_l = (a[0] + (int)((b[0] - a[0]) * cd_pos)) * (short)REG(CD_L) >> 15;
                cd_r = (a[1] + (int)((b[1] - a[1]) * cd_pos)) * (short)REG(CD_R) >> 15;
            }
            for (cd_pos += cd_step; cd_pos >= 1 && cd_out != cd_in; cd_pos -= 1)
                cd_out++;
        }
        if (cnt & 4) {
            reverb_in[reverb_frame][0] += cd_l;
            reverb_in[reverb_frame][1] += cd_r;
        }
        /* At 22.05 kHz: every other frame, the send low-passed into a reverb step, whose output goes in as an impulse
         * worth both frames; the low-pass again smooths it back out to 44.1 kHz. */
        wet[0] = wet[1] = 0;
        if ((reverb_odd ^= 1)) {
            sent[0] = sent[1] = 0;
            for (k = 0; k < TAPS; k++) {
                sent[0] += lowpass[k] * clamp16(reverb_in[(reverb_frame + TAPS - k) % TAPS][0]);
                sent[1] += lowpass[k] * clamp16(reverb_in[(reverb_frame + TAPS - k) % TAPS][1]);
            }
            reverb_step((int[2]){(int)sent[0], (int)sent[1]}, wet);
        }
        reverb_out[reverb_frame][0] = wet[0] * 2;
        reverb_out[reverb_frame][1] = wet[1] * 2;
        heard[0] = heard[1] = 0;
        for (k = 0; k < TAPS; k++) {
            heard[0] += lowpass[k] * reverb_out[(reverb_frame + TAPS - k) % TAPS][0];
            heard[1] += lowpass[k] * reverb_out[(reverb_frame + TAPS - k) % TAPS][1];
        }
        l += (int)heard[0];
        r += (int)heard[1];
        if (!(cnt & 0x4000)) /* muted, all but CD audio */
            l = r = 0;
        l = (l + cd_l) * volumes[VOICES][0] >> 15;
        r = (r + cd_r) * volumes[VOICES][1] >> 15;
        lr[2 * f] = clamp16(l);
        lr[2 * f + 1] = clamp16(r);
    }
    raised = irq_raised;
    irq_raised = 0;
    return raised;
}

/* Its 512 KiB, for the lab's dumps. */
const unsigned short *spu_memory(void)
{
    return spu_ram;
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
        touch(transfer & 0x7FFFC, 4);
        if (to_spu)
            memcpy((unsigned char *)spu_ram + (transfer & 0x7FFFC), words, 4);
        else
            memcpy(words, (unsigned char *)spu_ram + (transfer & 0x7FFFC), 4);
    }
}

unsigned short spu_read(unsigned int addr)
{
    unsigned int i = (addr - SPU_BASE) / 2;

    if (i < VOICES * 8 && (i & 7) == 6)
        return voices[i / 8].adsr.level;
    if (i < VOICES * 8 && (i & 7) == 7)
        return voices[i / 8].repeat / 8;
    if (addr >= VOICE_NOW && addr < VOICE_NOW + VOICES * 4)
        return volumes[(addr - VOICE_NOW) / 4][addr / 2 & 1];
    if (addr == MAIN_NOW || addr == MAIN_NOW + 2)
        return volumes[VOICES][addr / 2 & 1];
    if (addr == ENDX || addr == ENDX + 2)
        return addr == ENDX ? endx & 0xFFFF : endx >> 16;
    if (addr == SPUSTAT) /* echoes SPUCNT's mode bits, and is never busy */
        return (REG(SPUCNT) & 0x3F) | irq_flag << 6;
    return reg[i];
}

void spu_write(unsigned int addr, unsigned short v)
{
    unsigned int i = (addr - SPU_BASE) / 2, keys;
    int n;

    reg[i] = v;
    if (i < VOICES * 8) {
        if ((i & 7) < 2)
            set_volume(i / 8, i & 1, v);
        if ((i & 7) == 6)
            voices[i / 8].adsr.level = v & 0x7FFF;
        if ((i & 7) == 7) {
            voices[i / 8].repeat = v * 8 & 0x7FFF0;
            voices[i / 8].repeat_written = 1;
        }
        return;
    }
    switch (addr) {
    case MAIN_L:
    case MAIN_R:
        set_volume(VOICES, addr == MAIN_R, v);
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
                voices[n].adsr.wait = 0;
            }
        }
        break;
    case REVERB_BASE:
        reverb_at = v * 8;
        break;
    case TRANSFER:
        transfer = v * 8;
        break;
    case FIFO: /* a manual transfer, a halfword at a time */
        touch(transfer & 0x7FFFE, 2);
        spu_ram[(transfer & 0x7FFFE) / 2] = v;
        transfer += 2;
        break;
    case SPUCNT:
        if (!(v & 0x40)) /* acknowledges the interrupt */
            irq_flag = 0;
        break;
    }
}
