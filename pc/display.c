/* What the PS1 puts on screen, as 32-bit pixels for a window or a screenshot. */
#include <stdio.h>
#include <string.h>
#include "gpu.h"

static unsigned int expand5(unsigned int c)
{
    return c << 3 | c >> 2; /* 1Fh becomes FFh */
}

void display_to_rgba(const Display *d, unsigned int *out)
{
    const unsigned char *line;
    unsigned int px;
    int x, y;

    for (y = 0; y < d->h; y++) {
        for (x = 0; x < d->w; x++) {
            if (d->rgb24) { /* three bytes a pixel, from the display's start in VRAM */
                line = (const unsigned char *)vram[(d->y + y) & 511];
                px = 0xFF000000u | (unsigned int)line[(d->x * 2 + x * 3) & 2047] << 16
                    | (unsigned int)line[(d->x * 2 + x * 3 + 1) & 2047] << 8 | line[(d->x * 2 + x * 3 + 2) & 2047];
            } else {
                px = vram[(d->y + y) & 511][(d->x + x) & 1023];
                px = 0xFF000000u | expand5(px & 31) << 16 | expand5(px >> 5 & 31) << 8 | expand5(px >> 10 & 31);
            }
            *out++ = px;
        }
    }
}

static void put32(unsigned char *p, unsigned int v)
{
    memcpy(p, &v, 4);
}

int save_bmp(const char *path, const unsigned int *rgba, int w, int h)
{
    unsigned char header[54] = {'B', 'M'};
    FILE *f = fopen(path, "wb");
    int y;

    if (!f)
        return -1;
    put32(header + 2, 54 + w * h * 4);
    put32(header + 10, 54);
    put32(header + 14, 40);
    put32(header + 18, w);
    put32(header + 22, h);
    header[26] = 1;  /* planes */
    header[28] = 32; /* bits per pixel: BGRA, which 0xAARRGGBB already is in memory */
    fwrite(header, 1, 54, f);
    for (y = h - 1; y >= 0; y--) /* bottom row first */
        fwrite(rgba + y * w, 4, w, f);
    return fclose(f);
}
