#ifndef PC_GPU_H
#define PC_GPU_H

/* The PS1 GPU in software (gpu.c): its two ports and 1 MB of VRAM, 1024x512 halfwords of 15-bit colour plus a mask
 * bit. */
extern unsigned short vram[512][1024];

void gpu_gp0(unsigned int word);
void gpu_gp1(unsigned int word);
unsigned int gpu_read(void);
unsigned int gpu_status(void);
void gpu_vblank(void); /* the field on screen changes */

/* The part of VRAM on screen: from GP1(05h), (07h) and (08h). */
typedef struct Display {
    int x, y, w, h, rgb24;
} Display;

Display gpu_display(void);
/* Called after each upload to VRAM, when set: buttons.c watches for the game's window texture. */
extern void (*gpu_uploaded)(int x, int y, int w, int h);

/* buttons.c: the button glyphs the game's text draws, for the device in use. */
enum { BUTTONS_PLAYSTATION, BUTTONS_XBOX, BUTTONS_KEYS };
void buttons_uploaded(int x, int y, int w, int h);
void buttons_show(int which);
void buttons_keys(const char labels[16]); /* each pad bit's key cap: a letter or digit, Enter, Tab, or 0 */

/* display.c: the display area as 0xAARRGGBB pixels (w x h), and a 32-bit BMP of such pixels. */
void display_to_rgba(const Display *d, unsigned int *out);
int save_bmp(const char *path, const unsigned int *rgba, int w, int h);

#endif
