/* The window: the PS1's display scaled to 640x480 at 4:3, and the keyboard as pad 1. */
#include <stdio.h>
#define SDL_MAIN_HANDLED /* main.c keeps its own main */
#include <SDL.h>
#include "gpu.h"
#include "recomp.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

/* Keys for the pad's bits, in the PS1's order (bit 0 Select ... bit 15 Square). */
static const struct {
    SDL_Scancode key;
    int bit;
} keys[] = {
    {SDL_SCANCODE_BACKSPACE, 0}, {SDL_SCANCODE_RETURN, 3}, {SDL_SCANCODE_UP, 4}, {SDL_SCANCODE_RIGHT, 5},
    {SDL_SCANCODE_DOWN, 6}, {SDL_SCANCODE_LEFT, 7}, {SDL_SCANCODE_S, 12}, {SDL_SCANCODE_X, 13},
    {SDL_SCANCODE_Z, 14}, {SDL_SCANCODE_A, 15},
};

int sdl_open(void)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return -1;
    }
    window = SDL_CreateWindow("Final Fantasy VII", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
                              SDL_WINDOW_RESIZABLE);
    renderer = window ? SDL_CreateRenderer(window, -1, 0) : 0;
    /* hw.c paces the frames, so presenting doesn't wait for the monitor's vsync. */
    texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 512)
                       : 0;
    if (!texture) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return -1;
    }
    SDL_RenderSetLogicalSize(renderer, 640, 480);
    return 0;
}

/* Once a vblank: show the display area, read the keyboard. 0 when the player closes the window. */
int sdl_frame(void)
{
    static unsigned int rgba[640 * 512];
    const Uint8 *down;
    SDL_Rect area;
    SDL_Event e;
    Display d = gpu_display();
    unsigned short buttons = 0xFFFF;
    unsigned int i;

    display_to_rgba(&d, rgba);
    area.x = area.y = 0;
    area.w = d.w;
    area.h = d.h;
    SDL_UpdateTexture(texture, &area, rgba, d.w * 4);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, &area, 0);
    SDL_RenderPresent(renderer);
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
            return 0;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F12 && !e.key.repeat)
            save_bmp("screenshot.bmp", rgba, d.w, d.h);
    }
    down = SDL_GetKeyboardState(0);
    for (i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        if (down[keys[i].key])
            buttons &= ~(1u << keys[i].bit);
    }
    pad_buttons = buttons;
    return 1;
}
