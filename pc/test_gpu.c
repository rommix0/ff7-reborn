/* The software GPU against psx-spx's rules, pixel by pixel. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gpu.h"

#define RGB15(r, g, b) ((r) | (g) << 5 | (b) << 10)

static void send(const unsigned int *words, int n)
{
    while (n--)
        gpu_gp0(*words++);
}

static void reset(void)
{
    gpu_gp1(0x00000000);
    memset(vram, 0, sizeof(vram));
    gpu_gp0(0xE3000000);           /* drawing area 0,0 - 1023,511 */
    gpu_gp0(0xE4000000 | 511 << 10 | 1023);
    gpu_gp0(0xE5000000);           /* no offset */
}

static int count_row(int y, int x0, int x1)
{
    int x, n = 0;

    for (x = x0; x <= x1; x++)
        n += vram[y][x] != 0;
    return n;
}

static void fill_aligns_to_16(void)
{
    unsigned int fill[] = {0x020000F8, 10 << 16 | 3, 1 << 16 | 5}; /* red 0xF8 */
    int x;

    reset();
    send(fill, 3);
    for (x = 0; x < 16; x++)
        assert(vram[10][x] == RGB15(31, 0, 0));
    assert(vram[10][16] == 0 && vram[11][0] == 0);
}

static void upload_then_copy(void)
{
    unsigned int upload[] = {0xA0000000, 10 << 16 | 10, 2 << 16 | 2, 0x22221111, 0x44443333};
    unsigned int copy[] = {0x80000000, 10 << 16 | 10, 100 << 16 | 100, 2 << 16 | 2};
    unsigned int read[] = {0xC0000000, 100 << 16 | 100, 2 << 16 | 2};

    reset();
    send(upload, 5);
    assert(vram[10][10] == 0x1111 && vram[10][11] == 0x2222 && vram[11][10] == 0x3333 && vram[11][11] == 0x4444);
    send(copy, 4);
    assert(vram[100][100] == 0x1111 && vram[101][101] == 0x4444);
    send(read, 3);
    assert(gpu_status() & 1 << 27);
    assert(gpu_read() == 0x22221111 && gpu_read() == 0x44443333);
}

static void triangle_fill_rule(void)
{
    unsigned int tri[] = {0x20FFFFFF, 0, 4, 4 << 16};

    reset();
    send(tri, 4);
    assert(count_row(0, 0, 8) == 4 && count_row(1, 0, 8) == 3 && count_row(2, 0, 8) == 2 && count_row(3, 0, 8) == 1);
    assert(count_row(4, 0, 8) == 0 && vram[0][0] == RGB15(31, 31, 31));
}

static void sprite_4bpp_clut(void)
{
    unsigned int clut_upload[] = {0xA0000000, 300 << 16 | 0, 1 << 16 | 4, RGB15(1, 0, 0) << 16 | 0, RGB15(0, 0, 3) << 16 | RGB15(0, 2, 0)};
    unsigned int tex_upload[] = {0xA0000000, 0 << 16 | 64, 1 << 16 | 2, 0x00003210};
    unsigned int page[] = {0xE1000001}; /* texture page x 64, 4-bit */
    unsigned int sprite[] = {0x65808080, 50 << 16 | 40, (300 << 6) << 16 | 0, 1 << 16 | 4}; /* raw texture, 4x1 */

    reset();
    send(clut_upload, 5);
    send(tex_upload, 4);
    send(page, 1);
    send(sprite, 4);
    assert(vram[50][40] == 0); /* index 0 is colour 0x0000: transparent */
    assert(vram[50][41] == RGB15(1, 0, 0) && vram[50][42] == RGB15(0, 2, 0) && vram[50][43] == RGB15(0, 0, 3));
}

static void semi_transparency_mode0(void)
{
    unsigned int page[] = {0xE1000000}; /* mode 0: B/2 + F/2 */
    unsigned int rect[] = {0x62000000 | (10 << 3) << 8 | 30 << 3, 5 << 16 | 5, 1 << 16 | 1}; /* F = r30 g10 b0 */

    reset();
    vram[5][5] = RGB15(10, 20, 30);
    send(page, 1);
    send(rect, 3);
    assert(vram[5][5] == RGB15(20, 15, 15));
}

static void mask_check_skips(void)
{
    unsigned int mask[] = {0xE6000002}; /* don't draw over bit 15 */
    unsigned int rect[] = {0x60FFFFFF, 7 << 16 | 7, 1 << 16 | 2};
    unsigned int set[] = {0xE6000001, 0x60FFFFFF, 9 << 16 | 9, 1 << 16 | 1};

    reset();
    vram[7][7] = 0x8000 | RGB15(1, 1, 1);
    send(mask, 1);
    send(rect, 3);
    assert(vram[7][7] == (0x8000 | RGB15(1, 1, 1)) && vram[7][8] == RGB15(31, 31, 31));
    send(set, 4);
    assert(vram[9][9] == (0x8000 | RGB15(31, 31, 31)));
}

static void gouraud_dither(void)
{
    static const int offset[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};
    unsigned int page[] = {0xE1000200}; /* dithering on */
    unsigned int quad[] = {0x38808080, 0, 0x808080, 8, 0x808080, 8 << 16, 0x808080, 8 << 16 | 8};
    int x, y, c;

    reset();
    send(page, 1);
    send(quad, 8);
    for (y = 0; y < 4; y++) {
        for (x = 0; x < 4; x++) {
            c = (128 + offset[y][x]) >> 3;
            assert(vram[y][x] == RGB15(c, c, c));
        }
    }
}

static void gpu_clip_and_oversize(void)
{
    unsigned int area[] = {0xE3000000, 0xE4000000 | 9 << 10 | 9};
    unsigned int tri[] = {0x20FFFFFF, 5, 15, 10 << 16 | 5};
    unsigned int wide[] = {0x20FFFFFF, 20 << 16 | (-512 & 0x7FF), 20 << 16 | 512, 30 << 16 | 0};
    int y;

    reset();
    send(area, 2);
    send(tri, 4);
    for (y = 0; y < 12; y++)
        assert(count_row(y, 10, 20) == 0);
    assert(count_row(0, 5, 9) == 5 && count_row(9, 0, 20) == 1 && count_row(10, 0, 20) == 0);
    reset();
    send(wide, 4);
    for (y = 20; y <= 30; y++)
        assert(count_row(y, 0, 1023) == 0);
}

static void polyline_until_terminator(void)
{
    /* flat polyline (0,0) -> (3,0) -> (3,2), terminated; then a 1x1 rect shows the GPU is taking commands again */
    unsigned int words[] = {0x48FFFFFF, 0, 3, 2 << 16 | 3, 0x55555555, 0x60FFFFFF, 9 << 16 | 9, 1 << 16 | 1};
    int x, y;

    reset();
    send(words, 8);
    for (x = 0; x <= 3; x++)
        assert(vram[0][x] == RGB15(31, 31, 31));
    for (y = 1; y <= 2; y++)
        assert(vram[y][3] == RGB15(31, 31, 31) && vram[y][2] == 0);
    assert(vram[9][9] == RGB15(31, 31, 31));
}

static void interlaced_field_flips_per_vblank(void)
{
    /* in 480i, bit 31 is the field on screen: games draw the other one (libgpu's GetODE), so it holds for a frame */
    unsigned int field;

    reset();
    gpu_gp1(0x08000027); /* 640x480, interlaced */
    field = gpu_status() >> 31;
    assert(gpu_status() >> 31 == field);
    gpu_vblank();
    assert(gpu_status() >> 31 == !field);
}

static void display_fits_vram(void)
{
    /* a vertical range wider than VRAM (GP1(07h) takes 10 bits) shows no more than VRAM's 512 lines */
    reset();
    gpu_gp1(0x08000027);
    gpu_gp1(0x07000000 | 0x3FF << 10);
    assert(gpu_display().h == 512);
}

int main(void)
{
    display_fits_vram();
    interlaced_field_flips_per_vblank();
    fill_aligns_to_16();
    upload_then_copy();
    triangle_fill_rule();
    sprite_4bpp_clut();
    semi_transparency_mode0();
    mask_check_skips();
    gouraud_dither();
    gpu_clip_and_oversize();
    polyline_until_terminator();
    puts("ok");
    return 0;
}
