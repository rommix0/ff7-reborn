#ifndef PC_CONTROLS_H
#define PC_CONTROLS_H

/* controls.c: pad 1's buttons from the keyboard and a controller, by default or as controls.txt binds them. */
#include <SDL.h>

/* A controller's controls: SDL's buttons, then its two triggers. */
#define PAD_LEFT_TRIGGER SDL_CONTROLLER_BUTTON_MAX
#define PAD_RIGHT_TRIGGER (SDL_CONTROLLER_BUTTON_MAX + 1)
#define PAD_CONTROLS (SDL_CONTROLLER_BUTTON_MAX + 2)

void controls_read(const char *text); /* the defaults, then controls.txt's lines over them */
unsigned short controls_word(const Uint8 *keys_down, const unsigned char *pad_down); /* the pad word, 0 = held */
char controls_label(int bit); /* the first key on a bit, as its key cap shows it; 0 for none */

#endif
