/* The MDEC, the PS1's movie decoder, as psx-spx describes it: libpress uploads quantization and IDCT tables, then
 * sends a frame's run-level codes (DecDCTvlc's output) and reads back 16x16 macroblocks of 15- or 24-bit pixels.
 * Dequantizing matches tools/movie.py, so the MDEC and the converted AVIs start from the same coefficients.
 *
 * Macroblocks decode as they're read, so a frame needs no output buffer. ponytail: the 4- and 8-bit monochrome
 * depths and quantizer 0's special case aren't modelled; FF7 uses neither. */
#include <math.h>

#define END_OF_BLOCK 0xFE00

static unsigned char quant[2][64]; /* luma, chroma; in zigzag order */
static short idct[64]; /* the cosine basis in 1.15 fixed point, row by frequency */
static unsigned char zigzag[64]; /* zigzag position to row * 8 + column */
static unsigned int command, params_left;
static unsigned int params[0x10000];
static int param_count, code_pos; /* code_pos counts halfwords */
static unsigned int out[192];
static int out_pos, out_len;

static unsigned int code(int i)
{
    return params[i / 2] >> (i & 1) * 16 & 0xFFFF;
}

static int signed10(unsigned int n)
{
    return (int)(n & 0x3FF) - (int)(n & 0x200) * 2;
}

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* Skips the padding before a block; 0 when the codes have run out. */
static int block_ahead(void)
{
    while (code_pos < param_count * 2 && code(code_pos) == END_OF_BLOCK)
        code_pos++;
    return code_pos < param_count * 2;
}

static void decode_block(const unsigned char *q, int *pixels)
{
    int coef[64] = {0}, row[64], k = 0, scale, x, y, u;
    unsigned int n = code(code_pos++);
    long long sum;

    scale = n >> 10;
    coef[0] = clamp(signed10(n) * q[0], -1024, 1023);
    while (code_pos < param_count * 2 && (n = code(code_pos++)) != END_OF_BLOCK) {
        k += (n >> 10) + 1;
        if (k > 63)
            break;
        coef[zigzag[k]] = clamp((signed10(n) * q[k] * scale + 4) >> 3, -1024, 1023);
    }
    /* The basis is 2^16 times the IDCT's per-axis factor, so the two passes take 2^32 off */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            row[y * 8 + x] = 0;
            for (u = 0; u < 8; u++)
                row[y * 8 + x] += coef[y * 8 + u] * idct[u * 8 + x];
        }
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            for (sum = 0, u = 0; u < 8; u++)
                sum += (long long)idct[u * 8 + y] * row[u * 8 + x];
            pixels[y * 8 + x] = (int)((sum + (1LL << 31)) >> 32);
        }
}

/* Cr, Cb, then the four Y blocks left to right, top to bottom; out row by row. */
static void decode_macroblock(void)
{
    int cr[64], cb[64], luma[4][64], x, y, c, rgb[3], offset = command & 0x04000000 ? 0 : 128;
    unsigned char *bytes = (unsigned char *)out;
    unsigned int pixel;

    decode_block(quant[1], cr);
    block_ahead();
    decode_block(quant[1], cb);
    for (c = 0; c < 4; c++) {
        block_ahead();
        decode_block(quant[0], luma[c]);
    }
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            int l = luma[y / 8 * 2 + x / 8][y % 8 * 8 + x % 8], chroma = y / 2 * 8 + x / 2;

            rgb[0] = l + (int)lround(1.402 * cr[chroma]);
            rgb[1] = l - (int)lround(0.3437 * cb[chroma] + 0.7143 * cr[chroma]);
            rgb[2] = l + (int)lround(1.772 * cb[chroma]);
            for (c = 0; c < 3; c++)
                rgb[c] = clamp(rgb[c], -128, 127) + offset;
            if ((command >> 27 & 3) == 2) {
                for (c = 0; c < 3; c++)
                    bytes[(y * 16 + x) * 3 + c] = rgb[c];
                continue;
            }
            pixel = (rgb[0] & 0xFF) >> 3 | (rgb[1] & 0xFF) >> 3 << 5 | (rgb[2] & 0xFF) >> 3 << 10
                | (command >> 25 & 1) << 15;
            if (x & 1)
                out[(y * 16 + x) / 2] |= pixel << 16;
            else
                out[(y * 16 + x) / 2] = pixel;
        }
    out_pos = 0;
    out_len = (command >> 27 & 3) == 2 ? 192 : 128;
}

void mdec_write(unsigned int word)
{
    int i;

    if (params_left) {
        params[param_count++] = word;
        if (--params_left)
            return;
        if (command >> 29 == 2)
            for (i = 0; i < param_count * 4; i++)
                quant[i / 64][i % 64] = params[i / 4] >> i % 4 * 8;
        if (command >> 29 == 3)
            for (i = 0; i < 64; i++)
                idct[i] = code(i);
        return;
    }
    command = word;
    param_count = code_pos = out_pos = out_len = 0;
    switch (command >> 29) {
    case 1:
        params_left = command & 0xFFFF;
        break;
    case 2:
        params_left = command & 1 ? 32 : 16;
        break;
    case 3:
        params_left = 32;
        break;
    }
}

unsigned int mdec_read(void)
{
    if (out_pos == out_len) {
        if (command >> 29 != 1 || params_left || !block_ahead())
            return 0;
        decode_macroblock();
    }
    return out[out_pos++];
}

void mdec_control(unsigned int word)
{
    int k = 0, s, i;

    if (!(word & 0x80000000))
        return;
    command = params_left = param_count = code_pos = out_pos = out_len = 0;
    /* the zigzag walks each anti-diagonal, down-left on odd ones and up-right on even ones */
    for (s = 0; s < 15; s++)
        for (i = s < 8 ? 0 : s - 7; i <= (s < 8 ? s : 7); i++)
            zigzag[k++] = s & 1 ? i * 8 + (s - i) : (s - i) * 8 + i;
}

unsigned int mdec_status(void)
{
    int pending = out_pos < out_len || (command >> 29 == 1 && block_ahead());

    return (unsigned int)!pending << 31 | (unsigned int)(params_left || pending) << 29 | ((params_left - 1) & 0xFFFF);
}
