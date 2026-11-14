/* Pad 1 from the PC's controls (docs/superpowers/specs/2026-11-09-pc-buttons-design.md): keys and a controller's buttons
 * for each of the PS1 pad's bits, by default or as controls.txt binds them. A line there is a PS1 button, "=", then
 * its controls, comma-separated: a key by SDL's name ("J", "Return", "Left Shift") or "pad" and a controller's button
 * by SDL's ("pad a", "pad leftshoulder", "pad lefttrigger"). The button takes those instead of its defaults, and a
 * control named there leaves the button it had. "#" starts a comment. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "controls.h"

#define MAX_BINDINGS 64

typedef struct {
    int control, bit; /* a scancode, or a pad control (a button, or PAD_LEFT_TRIGGER/PAD_RIGHT_TRIGGER) */
} Binding;

/* The PS1's buttons, in its pad word's order (bit 0 Select ... bit 15 Square). */
static const char *const names[16] = {"select", "l3", "r3", "start", "up", "right", "down", "left",
                                      "l2", "r2", "l1", "r1", "triangle", "circle", "cross", "square"};

/* WASD or the arrows move, the right hand's J K L I are the face buttons as a pad's diamond (J OK, K cancel), Q/E and
 * 1/3 the shoulders. */
static const Binding default_keys[] = {
    {SDL_SCANCODE_TAB, 0}, {SDL_SCANCODE_RETURN, 3}, {SDL_SCANCODE_W, 4}, {SDL_SCANCODE_UP, 4},
    {SDL_SCANCODE_D, 5}, {SDL_SCANCODE_RIGHT, 5}, {SDL_SCANCODE_S, 6}, {SDL_SCANCODE_DOWN, 6}, {SDL_SCANCODE_A, 7},
    {SDL_SCANCODE_LEFT, 7}, {SDL_SCANCODE_1, 8}, {SDL_SCANCODE_3, 9}, {SDL_SCANCODE_Q, 10}, {SDL_SCANCODE_E, 11},
    {SDL_SCANCODE_I, 12}, {SDL_SCANCODE_J, 13}, {SDL_SCANCODE_K, 14}, {SDL_SCANCODE_L, 15},
};

/* A controller by function, as PC games have them: A (SDL's, by label) is OK, o; B is cancel, x. */
static const Binding default_pad[] = {
    {SDL_CONTROLLER_BUTTON_BACK, 0}, {SDL_CONTROLLER_BUTTON_LEFTSTICK, 1}, {SDL_CONTROLLER_BUTTON_RIGHTSTICK, 2},
    {SDL_CONTROLLER_BUTTON_START, 3}, {SDL_CONTROLLER_BUTTON_DPAD_UP, 4}, {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 5},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, 6}, {SDL_CONTROLLER_BUTTON_DPAD_LEFT, 7}, {PAD_LEFT_TRIGGER, 8},
    {PAD_RIGHT_TRIGGER, 9}, {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 10}, {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 11},
    {SDL_CONTROLLER_BUTTON_Y, 12}, {SDL_CONTROLLER_BUTTON_A, 13}, {SDL_CONTROLLER_BUTTON_B, 14},
    {SDL_CONTROLLER_BUTTON_X, 15},
};

static Binding keys[MAX_BINDINGS], pad[MAX_BINDINGS];
static int key_count, pad_count;

/* Drops the bindings of a bit (or of a control, with bit -1 and control set). */
static void unbind(Binding *list, int *count, int bit, int control)
{
    int i, kept = 0;

    for (i = 0; i < *count; i++)
        if (bit >= 0 ? list[i].bit != bit : list[i].control != control)
            list[kept++] = list[i];
    *count = kept;
}

static void bind(Binding *list, int *count, int control, int bit)
{
    unbind(list, count, -1, control);
    if (*count < MAX_BINDINGS)
        list[(*count)++] = (Binding){control, bit};
}

/* The text between from and to without the spaces round it, into out. */
static void trimmed(const char *from, const char *to, char *out, size_t size)
{
    size_t n;

    while (from < to && isspace((unsigned char)*from))
        from++;
    while (to > from && isspace((unsigned char)to[-1]))
        to--;
    n = (size_t)(to - from) < size - 1 ? (size_t)(to - from) : size - 1;
    memcpy(out, from, n);
    out[n] = 0;
}

static void read_line(const char *line, const char *end)
{
    const char *eq = memchr(line, '=', end - line), *at, *comma;
    char word[64];
    int bit, i, control;

    if (!eq)
        return;
    trimmed(line, eq, word, sizeof word);
    for (bit = 0; bit < 16 && SDL_strcasecmp(word, names[bit]); bit++)
        ;
    if (bit == 16) {
        fprintf(stderr, "controls.txt: no PS1 button called \"%s\"\n", word);
        return;
    }
    unbind(keys, &key_count, bit, 0);
    unbind(pad, &pad_count, bit, 0);
    for (at = eq + 1; at < end; at = comma + 1) {
        comma = memchr(at, ',', end - at);
        if (!comma)
            comma = end;
        trimmed(at, comma, word, sizeof word);
        if (!word[0])
            continue;
        if (!SDL_strncasecmp(word, "pad ", 4)) {
            for (i = 4; word[i] == ' '; i++)
                ;
            control = !SDL_strcasecmp(word + i, "lefttrigger") ? PAD_LEFT_TRIGGER
                      : !SDL_strcasecmp(word + i, "righttrigger") ? PAD_RIGHT_TRIGGER
                      : SDL_GameControllerGetButtonFromString(word + i);
            if (control != SDL_CONTROLLER_BUTTON_INVALID) {
                bind(pad, &pad_count, control, bit);
                continue;
            }
        } else if ((control = SDL_GetScancodeFromName(word)) != SDL_SCANCODE_UNKNOWN) {
            bind(keys, &key_count, control, bit);
            continue;
        }
        fprintf(stderr, "controls.txt: no key or pad button called \"%s\"\n", word);
    }
}

void controls_read(const char *text)
{
    const char *end;

    memcpy(keys, default_keys, sizeof default_keys);
    key_count = sizeof default_keys / sizeof default_keys[0];
    memcpy(pad, default_pad, sizeof default_pad);
    pad_count = sizeof default_pad / sizeof default_pad[0];
    for (; *text; text = *end ? end + 1 : end) {
        end = text + strcspn(text, "\n");
        read_line(text, text + strcspn(text, "#\n")); /* up to a comment */
    }
}

unsigned short controls_word(const Uint8 *keys_down, const unsigned char *pad_down)
{
    unsigned short word = 0xFFFF;
    int i;

    for (i = 0; i < key_count; i++)
        if (keys_down[keys[i].control])
            word &= ~(1u << keys[i].bit);
    for (i = 0; i < pad_count; i++)
        if (pad_down[pad[i].control])
            word &= ~(1u << pad[i].bit);
    return word;
}

char controls_label(int bit)
{
    const char *name;
    int i;

    for (i = 0; i < key_count && keys[i].bit != bit; i++)
        ;
    if (i == key_count)
        return 0;
    name = SDL_GetScancodeName(keys[i].control);
    if (!strcmp(name, "Return"))
        return '\r';
    if (!strcmp(name, "Tab"))
        return '\t';
    return name[0] && !name[1] && isalnum((unsigned char)name[0]) ? (char)toupper((unsigned char)name[0]) : 0;
}
