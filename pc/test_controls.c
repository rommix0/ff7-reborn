/* controls.c: the default bindings, and controls.txt's lines over them. */
#include <assert.h>
#include <stdio.h>
#define SDL_MAIN_HANDLED
#include "controls.h"

#define HELD(bit) (0xFFFF & ~(1u << (bit)))

int main(void)
{
    static Uint8 keys[SDL_NUM_SCANCODES];
    static unsigned char pad[PAD_CONTROLS];

    /* defaults: J is o (bit 13), and the pad's A; the left trigger L2 (bit 8); W and the up arrow up (bit 4); the key
     * caps show J, Enter for Start, W for up */
    controls_read("");
    keys[SDL_SCANCODE_J] = 1;
    assert(controls_word(keys, pad) == HELD(13));
    keys[SDL_SCANCODE_J] = 0;
    pad[SDL_CONTROLLER_BUTTON_A] = 1;
    assert(controls_word(keys, pad) == HELD(13));
    pad[SDL_CONTROLLER_BUTTON_A] = 0;
    pad[PAD_LEFT_TRIGGER] = 1;
    assert(controls_word(keys, pad) == HELD(8));
    pad[PAD_LEFT_TRIGGER] = 0;
    keys[SDL_SCANCODE_UP] = 1;
    assert(controls_word(keys, pad) == HELD(4));
    keys[SDL_SCANCODE_UP] = 0;
    assert(controls_label(13) == 'J' && controls_label(3) == '\r' && controls_label(4) == 'W');

    /* a_line_takes_over_its_button: o on N and the pad's X; J no longer presses anything, X leaves [] (bit 15), the
     * rest keep their defaults. Names in any case, spaces round them, comments and blank lines. */
    controls_read("# mine\n  Circle = n , pad X\n\ncross = K\n");
    keys[SDL_SCANCODE_N] = 1;
    assert(controls_word(keys, pad) == HELD(13));
    keys[SDL_SCANCODE_N] = 0;
    keys[SDL_SCANCODE_J] = 1;
    assert(controls_word(keys, pad) == 0xFFFF);
    keys[SDL_SCANCODE_J] = 0;
    pad[SDL_CONTROLLER_BUTTON_X] = 1;
    assert(controls_word(keys, pad) == HELD(13));
    pad[SDL_CONTROLLER_BUTTON_X] = 0;
    keys[SDL_SCANCODE_L] = 1;
    assert(controls_word(keys, pad) == HELD(15));
    keys[SDL_SCANCODE_L] = 0;
    assert(controls_label(13) == 'N' && controls_label(14) == 'K');

    /* triggers_and_named_keys: L2 on the right trigger and Left Shift; a key with no cap letter shows none */
    controls_read("l2 = pad righttrigger, Left Shift\n");
    pad[PAD_RIGHT_TRIGGER] = 1;
    assert(controls_word(keys, pad) == HELD(8));
    pad[PAD_RIGHT_TRIGGER] = 0;
    keys[SDL_SCANCODE_LSHIFT] = 1;
    assert(controls_word(keys, pad) == HELD(8));
    keys[SDL_SCANCODE_LSHIFT] = 0;
    assert(controls_label(8) == 0);

    /* unknown_names_are_skipped: the rest of the line still counts; a line with no known button changes nothing */
    controls_read("circle = Nokey, H\nbogus = K\nno equals sign\n");
    keys[SDL_SCANCODE_H] = 1;
    assert(controls_word(keys, pad) == HELD(13));
    keys[SDL_SCANCODE_H] = 0;
    keys[SDL_SCANCODE_K] = 1;
    assert(controls_word(keys, pad) == HELD(14));
    puts("ok");
    return 0;
}
