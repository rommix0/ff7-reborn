/* buttons.c: the PC's glyphs drawn over the game's when its window texture arrives, and the game's back on request. */
#include <assert.h>
#include <stdio.h>
#include "gpu.h"

#define TEXEL(x, y) (vram[256 + (y)][960 + (x) / 4] >> (x) % 4 * 4 & 15)

static void upload(int x, int y, int w, int h, unsigned int fill)
{
    int i;

    gpu_gp0(0xA0000000);
    gpu_gp0((unsigned int)y << 16 | x);
    gpu_gp0((unsigned int)h << 16 | w);
    for (i = 0; i < (w * h + 1) / 2; i++)
        gpu_gp0(fill);
}

int main(void)
{
    gpu_uploaded = buttons_uploaded;
    buttons_show(BUTTONS_XBOX);

    /* window_texture_gets_the_pads_glyphs: A over o at texel (0x60, 0x40): its disc dark, its letter's crossbar
     * green, its corner clear. RB over R1 below it as the game draws R1, bevel and all, its R in colour 1 (the menu
     * draws the shoulders with another palette, where only the game's own colours read). Below those, View over
     * Select and Menu over Start (Customize's Help and Pause), in the same colour. */
    upload(960, 256, 64, 256, 0x99999999);
    assert(TEXEL(0x60 + 7, 0x40 + 8) == 6 && TEXEL(0x60 + 1, 0x40 + 8) == 15 && TEXEL(0x60, 0x40) == 0);
    assert(TEXEL(0x60 + 5, 0x50 + 3) == 4 && TEXEL(0x60, 0x50 + 4) == 5 && TEXEL(0x60 + 2, 0x50 + 4) == 1);
    assert(TEXEL(0x70 + 4, 0x60 + 5) == 1 && TEXEL(0x70, 0x60) == 0);
    assert(TEXEL(0x5F, 0x40) == 9 && TEXEL(0xA0, 0x40) == 9 && TEXEL(0x80, 0x60) == 9); /* the rest is the game's */

    /* playstation_gets_the_games_back */
    buttons_show(BUTTONS_PLAYSTATION);
    assert(TEXEL(0x60 + 7, 0x40 + 8) == 9 && TEXEL(0x60, 0x40) == 9);

    /* something_else_over_the_strip_is_left_alone: until the texture comes again */
    upload(984, 320, 1, 1, 0x33333333);
    buttons_show(BUTTONS_XBOX);
    assert(TEXEL(0x60, 0x40) == 3 && TEXEL(0x60 + 7, 0x40 + 8) == 9);
    upload(960, 256, 64, 256, 0x99999999);
    assert(TEXEL(0x60 + 7, 0x40 + 8) == 6);

    /* keyboard_gets_key_caps: J over o, K over x, a grey edge and a dark top; E over R1 in colour 1, as the game's
     * shoulders; Tab over Select */
    buttons_show(BUTTONS_KEYS);
    assert(TEXEL(0x60 + 1, 0x40 + 5) == 1 && TEXEL(0x60 + 2, 0x40 + 12) == 15 && TEXEL(0x60, 0x40) == 0);
    assert(TEXEL(0x60 + 7, 0x40 + 2) == 2 && TEXEL(0x70 + 4, 0x40 + 2) == 2);
    assert(TEXEL(0x60 + 4, 0x50 + 2) == 1 && TEXEL(0x60 + 5, 0x50 + 2) == 1 && TEXEL(0x60 + 2, 0x50 + 12) == 15);
    assert(TEXEL(0x60 + 10, 0x60 + 3) == 1);

    /* caps_show_the_bound_keys: o on N; a key with no letter (0) gets a blank cap */
    {
        char labels[16] = {0};

        labels[13] = 'N';
        buttons_keys(labels);
        assert(TEXEL(0x60 + 4, 0x40 + 2) == 2 && TEXEL(0x60 + 6, 0x40 + 2) == 15);
        assert(TEXEL(0x70 + 4, 0x40 + 2) == 15 && TEXEL(0x70 + 1, 0x40 + 5) == 1);
    }
    puts("ok");
    return 0;
}
