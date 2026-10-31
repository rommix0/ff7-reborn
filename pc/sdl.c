/* The window: the PS1's display scaled to 640x480 at 4:3, the keyboard and the first game controller as pad 1, and
 * the SPU's sound. */
#include <stdio.h>
#define SDL_MAIN_HANDLED /* main.c keeps its own main */
#include <SDL.h>
#include "gpu.h"
#include "recomp.h"

#define AUDIO_RING 8192  /* stereo frames the SPU can be ahead of the sound card: 0.19 s */
#define AUDIO_START 2048 /* frames to have in before playing, and again after running dry */

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static short audio_ring[AUDIO_RING][2];
static SDL_atomic_t audio_in, audio_out; /* frames written and played, counted on from 0 */
static SDL_GameController *controller;

void lab_dump(void);

/* Keys for the pad's bits, in the PS1's order (bit 0 Select ... bit 15 Square). */
static const struct {
    SDL_Scancode key;
    int bit;
} keys[] = {
    {SDL_SCANCODE_BACKSPACE, 0}, {SDL_SCANCODE_RETURN, 3}, {SDL_SCANCODE_UP, 4}, {SDL_SCANCODE_RIGHT, 5},
    {SDL_SCANCODE_DOWN, 6}, {SDL_SCANCODE_LEFT, 7}, {SDL_SCANCODE_1, 8}, {SDL_SCANCODE_2, 9},
    {SDL_SCANCODE_Q, 10}, {SDL_SCANCODE_W, 11}, {SDL_SCANCODE_S, 12}, {SDL_SCANCODE_X, 13},
    {SDL_SCANCODE_Z, 14}, {SDL_SCANCODE_A, 15},
};

/* A controller's buttons by where they sit: the bottom face button is ×, the right one ○. */
static const struct {
    SDL_GameControllerButton button;
    int bit;
} buttons[] = {
    {SDL_CONTROLLER_BUTTON_BACK, 0}, {SDL_CONTROLLER_BUTTON_LEFTSTICK, 1}, {SDL_CONTROLLER_BUTTON_RIGHTSTICK, 2},
    {SDL_CONTROLLER_BUTTON_START, 3}, {SDL_CONTROLLER_BUTTON_DPAD_UP, 4}, {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 5},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, 6}, {SDL_CONTROLLER_BUTTON_DPAD_LEFT, 7},
    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 10}, {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 11},
    {SDL_CONTROLLER_BUTTON_Y, 12}, {SDL_CONTROLLER_BUTTON_B, 13}, {SDL_CONTROLLER_BUTTON_A, 14},
    {SDL_CONTROLLER_BUTTON_X, 15},
};

/* On SDL's audio thread: the SPU's frames, or silence until enough have come. */
static void play(void *unused, Uint8 *stream, int len)
{
    static int playing;
    short *dst = (short *)stream;
    unsigned int in = SDL_AtomicGet(&audio_in), out = SDL_AtomicGet(&audio_out);
    int i;

    (void)unused;
    if (in - out >= AUDIO_START)
        playing = 1;
    for (i = 0; i < len / 4; i++) {
        if (!playing || out == in) {
            playing = 0;
            dst[2 * i] = dst[2 * i + 1] = 0;
            continue;
        }
        dst[2 * i] = audio_ring[out % AUDIO_RING][0];
        dst[2 * i + 1] = audio_ring[out % AUDIO_RING][1];
        out++;
    }
    SDL_AtomicSet(&audio_out, out);
}

/* The SPU's frames, as the PS1's time passes; while the sound card is behind (or there's none) the rest is dropped.
 * ponytail: the PS1's clock and the card's drift apart by a frame now and then; resample if that's ever heard. */
void pc_audio(const short *lr, int frames)
{
    unsigned int in = SDL_AtomicGet(&audio_in), out = SDL_AtomicGet(&audio_out);
    int i;

    for (i = 0; i < frames && in - out < AUDIO_RING; i++, in++) {
        audio_ring[in % AUDIO_RING][0] = lr[2 * i];
        audio_ring[in % AUDIO_RING][1] = lr[2 * i + 1];
    }
    SDL_AtomicSet(&audio_in, in);
}

int sdl_open(void)
{
    SDL_AudioSpec want;
    SDL_AudioDeviceID device = 0;

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
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = play;
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER); /* controllers come in as events, those plugged in already too */
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        device = SDL_OpenAudioDevice(0, 0, &want, 0, 0);
    if (device)
        SDL_PauseAudioDevice(device, 0);
    else
        fprintf(stderr, "SDL: no sound: %s\n", SDL_GetError());
    return 0;
}

/* Once a vblank: show the display area, read the keyboard and controller. 0 when the player closes the window. F12
 * saves a screenshot, F11 dumps memory for the lab. */
int sdl_frame(void)
{
    static unsigned int rgba[640 * 512];
    const Uint8 *down;
    SDL_Rect area;
    SDL_Event e;
    Display d = gpu_display();
    unsigned short pad_buttons_held = 0xFFFF;
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
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F11 && !e.key.repeat)
            lab_dump();
        if (e.type == SDL_CONTROLLERDEVICEADDED && !controller)
            controller = SDL_GameControllerOpen(e.cdevice.which);
        if (e.type == SDL_CONTROLLERDEVICEREMOVED && controller
            && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
            SDL_GameControllerClose(controller);
            controller = 0;
        }
    }
    down = SDL_GetKeyboardState(0);
    for (i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        if (down[keys[i].key])
            pad_buttons_held &= ~(1u << keys[i].bit);
    }
    if (controller) {
        for (i = 0; i < sizeof buttons / sizeof buttons[0]; i++) {
            if (SDL_GameControllerGetButton(controller, buttons[i].button))
                pad_buttons_held &= ~(1u << buttons[i].bit);
        }
        /* the triggers as L2 and R2, the left stick as the d-pad: past half way */
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 0x4000)
            pad_buttons_held &= ~(1u << 8);
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0x4000)
            pad_buttons_held &= ~(1u << 9);
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) < -0x4000)
            pad_buttons_held &= ~(1u << 4);
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) > 0x4000)
            pad_buttons_held &= ~(1u << 5);
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) > 0x4000)
            pad_buttons_held &= ~(1u << 6);
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) < -0x4000)
            pad_buttons_held &= ~(1u << 7);
    }
    pad_buttons = pad_buttons_held;
    return 1;
}
