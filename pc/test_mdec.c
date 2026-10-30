/* The MDEC: libpress's three commands (tables, then run-level codes in), pixels out, as psx-spx describes them. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void mdec_write(unsigned int word);
void mdec_control(unsigned int word);
unsigned int mdec_read(void);
unsigned int mdec_status(void);

#define BUSY 0x20000000u
#define EOB 0xFE00

static void send_halfwords(unsigned int command, const unsigned short *codes, int count)
{
    int i;

    mdec_write(command | count / 2);
    for (i = 0; i < count; i += 2)
        mdec_write(codes[i] | (unsigned int)codes[i + 1] << 16);
}

static int near(int got, int want)
{
    return abs(got - want) <= 1;
}

int main(void)
{
    unsigned short tables[64], codes[32];
    unsigned int px[192];
    unsigned char *rgb = (unsigned char *)px;
    int i, u, x;

    mdec_control(0x80000000); /* reset */
    assert(!(mdec_status() & BUSY));

    /* Quantization: every luma step 2, chroma 2. Scale: the DCT basis in 1.15 fixed point, row by frequency */
    for (i = 0; i < 64; i++)
        tables[i] = 0x0202;
    send_halfwords(0x40000001, tables, 64);
    for (u = 0; u < 8; u++)
        for (x = 0; x < 8; x++)
            tables[u * 8 + x] = (short)floor(32768 * (u ? 1 : sqrt(0.5)) * cos((2 * x + 1) * u * M_PI / 16) + 0.5);
    send_halfwords(0x60000000, tables, 64);
    assert(!(mdec_status() & BUSY));

    /* flat_blocks_land_in_their_quadrants: Cr Cb Y1 Y2 Y3 Y4, DC only; a DC of 4n is a level of n (step 2, then the
     * IDCT's 1/8), and unsigned output adds 128. 15-bit with the mask bit asked for. */
    i = 0;
    codes[i++] = 0, codes[i++] = EOB; /* Cr */
    codes[i++] = 0, codes[i++] = EOB; /* Cb */
    codes[i++] = 160, codes[i++] = EOB;
    codes[i++] = -160 & 0x3FF, codes[i++] = EOB;
    codes[i++] = 0, codes[i++] = EOB;
    codes[i++] = 400, codes[i++] = EOB;
    send_halfwords(0x3A000000, codes, i);
    assert(mdec_status() & BUSY);
    for (i = 0; i < 128; i++)
        px[i] = mdec_read();
    assert(!(mdec_status() & BUSY));
#define PIXEL15(x, y) (px[((y) * 16 + (x)) / 2] >> ((x) & 1) * 16 & 0xFFFF)
#define GRAY15(v) (0x8000 | (v) >> 3 | (v) >> 3 << 5 | (v) >> 3 << 10)
    assert(PIXEL15(0, 0) == GRAY15(168) && PIXEL15(7, 7) == GRAY15(168));
    assert(PIXEL15(15, 0) == GRAY15(88) && PIXEL15(0, 15) == GRAY15(128) && PIXEL15(15, 15) == GRAY15(228));

    /* chroma_tints_the_whole_macroblock: Cr of 20 on a Y of 0, 24-bit: R = Y + 1.402 Cr, G = Y - 0.714 Cr */
    i = 0;
    codes[i++] = 80, codes[i++] = EOB;
    for (u = 0; u < 5; u++)
        codes[i++] = 0, codes[i++] = EOB;
    send_halfwords(0x30000000, codes, i);
    for (i = 0; i < 192; i++)
        px[i] = mdec_read();
    assert(near(rgb[0], 156) && near(rgb[1], 114) && rgb[2] == 128);
    assert(near(rgb[(15 * 16 + 15) * 3], 156));

    /* ac_codes_follow_the_zigzag: Y1's first AC code (run 0) is the lowest horizontal frequency, scaled by the
     * block's quantizer: (32 * 2 * 8 + 4) >> 3 = 64, which the IDCT makes 64 / (4 sqrt 2) cos(pi (2x + 1) / 16) */
    i = 0;
    codes[i++] = 0, codes[i++] = EOB;
    codes[i++] = 0, codes[i++] = EOB;
    codes[i++] = 8 << 10, codes[i++] = 32, codes[i++] = EOB, codes[i++] = EOB; /* a padding EOB before Y2 */
    for (u = 0; u < 3; u++)
        codes[i++] = 0, codes[i++] = EOB;
    send_halfwords(0x30000000, codes, i);
    for (i = 0; i < 192; i++)
        px[i] = mdec_read();
    assert(!(mdec_status() & BUSY));
#define Y24(x, y) rgb[((y) * 16 + (x)) * 3 + 1]
    assert(near(Y24(0, 0), 139) && near(Y24(7, 0), 117) && Y24(0, 7) == Y24(0, 0) && Y24(8, 0) == 128);
    puts("ok");
    return 0;
}
