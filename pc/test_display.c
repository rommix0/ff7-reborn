/* What the display shows, as RGBA: 15-bit and 24-bit modes, and the BMP screenshot. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gpu.h"

int main(void)
{
    static unsigned int out[640 * 480];
    unsigned char header[54];
    unsigned char *bytes = (unsigned char *)vram[0];
    Display d = {0, 0, 2, 1, 0};
    FILE *f;

    vram[0][0] = 0x7FFF;
    vram[0][1] = 0x001F;
    display_to_rgba(&d, out);
    assert(out[0] == 0xFFFFFFFF && out[1] == 0xFFFF0000);

    d.rgb24 = 1;
    bytes[0] = 0x12; /* R G B of the first 24-bit pixel */
    bytes[1] = 0x34;
    bytes[2] = 0x56;
    display_to_rgba(&d, out);
    assert(out[0] == 0xFF123456);

    assert(save_bmp("build/pc/test_display.bmp", out, 2, 1) == 0);
    f = fopen("build/pc/test_display.bmp", "rb");
    assert(f && fread(header, 1, sizeof(header), f) == sizeof(header));
    fclose(f);
    assert(!memcmp(header, "BM", 2) && header[18] == 2 && header[22] == 1 && header[28] == 32);
    puts("ok");
    return 0;
}
