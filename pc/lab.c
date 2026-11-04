/* The reversing lab (ff7 --trace FILE --input FILE --field N --minigame N --battle N --command A,C,X,T --shots K N
 * --dump-at N --code "AAAAAAAA VVVV", and F11 in the window): a trace of what the game does each frame, the pad
 * played from a script, a warp to a field, a mini-game, a battle, a party member's command in it, pictures of the
 * screen, GameShark codes, and dumps of RAM, VRAM and SPU RAM.
 *
 * The trace has a line for each change: "FRAME state mode M field F request R ID" (the main loop's mode, the field
 * loaded, and the field module's request: 1 a field change, 2 a battle, 3 a movie, with its number), "FRAME pad
 * BUTTONS", "FRAME read PATH +SECTOR" where a CD read starts, "FRAME warp F", "FRAME minigame N", "FRAME battle N",
 * "FRAME command A C X T" and "FRAME dump".
 * An input script has a line for each change too: a frame, then the buttons held from it on ("-" for none); '#' starts
 * a comment. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu.h"
#include "recomp.h"

#define GAME_MODE ((volatile short *)0x8009C560)
#define FIELD ((volatile unsigned short *)0x8009A05C)
#define REQUEST ((volatile unsigned char *)0x8009ABF5)
#define REQUEST_ID ((volatile unsigned short *)0x8009ABF6)
#define MINIGAME ((volatile unsigned char *)0x8009ACE6) /* which, with request 0xC */
/* BATTLE.X's (game mode 2): the members whose menu waits on a command, those with one chosen, the menu's list of who
 * waits (FF ends it), and the ring of 16 commands chosen (8 bytes each) with its read and write places */
#define WAITING ((volatile unsigned short *)0x800F7DAC)
#define CHOSEN ((volatile unsigned short *)0x800F7DC2)
#define MENU_LIST ((volatile unsigned char *)0x800E7A48)
#define CHOICES ((volatile unsigned char *)0x800F3958)
#define CHOICE_READ ((volatile int *)0x800F39D8)
#define CHOICE_WRITE ((volatile int *)0x800F39DC)
#define MAX_INPUTS 4096
#define MAX_CODES 64

const unsigned short *spu_memory(void);

static const char *const button_names[16] = {"select", "l3", "r3", "start", "up", "right", "down", "left",
                                             "l2", "r2", "l1", "r1", "triangle", "circle", "cross", "square"};
static FILE *trace;
static struct {
    int frame;
    unsigned short buttons; /* active low */
} inputs[MAX_INPUTS];
static struct {
    unsigned int addr, value;
    int bits;
} codes[MAX_CODES];
static int code_count, input_count, next_input, warp_field = -1, dump_frame = -1, dump_every, frame;
static int minigame = -1, minigame_asked, battle = -1, battle_asked, shot_every, shots_left, shots_from;
static struct {
    int actor, command, attack, targets;
} command = {-1};
static int last_state[4] = {-1, -1, -1, -1}, last_pad = -1;

static void write_file(const char *suffix, const void *data, size_t size)
{
    char path[64];
    FILE *f;

    snprintf(path, sizeof path, "dump_%05d_%s.bin", frame, suffix);
    f = fopen(path, "wb");
    if (!f) {
        perror(path);
        return;
    }
    fwrite(data, 1, size, f);
    fclose(f);
}

void lab_dump(void)
{
    write_file("ram", (const void *)0x80000000, 0x200000);
    write_file("vram", vram, sizeof vram);
    write_file("spu", spu_memory(), 0x80000);
    if (trace)
        fprintf(trace, "%d dump\n", frame);
}

int lab_open(const char *trace_path, const char *input_path, int field, int dump_at)
{
    char line[512], *word;
    FILE *f;
    int i;

    warp_field = field;
    dump_frame = dump_at;
    if (trace_path && !(trace = fopen(trace_path, "w"))) {
        perror(trace_path);
        return -1;
    }
    if (!input_path)
        return 0;
    if (!(f = fopen(input_path, "r"))) {
        perror(input_path);
        return -1;
    }
    while (fgets(line, sizeof line, f) && input_count < MAX_INPUTS) {
        if (strchr(line, '#'))
            *strchr(line, '#') = 0;
        if (!(word = strtok(line, " \t\r\n")))
            continue;
        inputs[input_count].frame = atoi(word);
        inputs[input_count].buttons = 0xFFFF;
        while ((word = strtok(0, " \t\r\n"))) {
            for (i = 0; i < 16 && strcmp(word, button_names[i]); i++)
                ;
            if (i < 16)
                inputs[input_count].buttons &= ~(1u << i);
            else if (strcmp(word, "-"))
                fprintf(stderr, "%s: no button %s\n", input_path, word);
        }
        input_count++;
    }
    fclose(f);
    return 0;
}

/* A GameShark code held every frame, as the cartridge does: 80AAAAAA VVVV writes 16 bits, 30AAAAAA 00VV 8 bits. */
int lab_code(const char *code)
{
    unsigned int word, value;

    if (code_count == MAX_CODES || sscanf(code, "%x %x", &word, &value) != 2)
        return -1;
    if (word >> 24 != 0x80 && word >> 24 != 0x30)
        return -1;
    codes[code_count].addr = 0x80000000u | (word & 0x1FFFFF);
    codes[code_count].value = value;
    codes[code_count++].bits = word >> 24 == 0x80 ? 16 : 8;
    return 0;
}

/* Dump again every so many frames after --dump-at's. */
void lab_dump_every(int frames)
{
    dump_every = frames;
}

/* A mini-game, asked for from a field as its script does, after any warp: 0 the motorcycle, 1 the chocobo race, 2 the
 * snowboard, 3 Fort Condor, 4 the submarine, 5 the Speed Square, 6 the Gold Saucer snowboard. */
void lab_minigame(int game)
{
    minigame = game;
}

/* A battle, asked for from a field as an encounter asks, after any warp. */
void lab_battle(int number)
{
    battle = number;
}

/* A command for a party member (0-2) to carry out, put in once the battle's menu waits on that member: its command
 * number, attack (the spell, item or limit; 0 for a plain command) and target mask (bit 4 the first enemy). */
void lab_command(int actor, int command_, int attack, int targets)
{
    command.actor = actor, command.command = command_, command.attack = attack, command.targets = targets;
}

/* count pictures of the screen, every frames apart, from when the command goes in (from the start without one);
 * the lab is done after the last. */
void lab_shots(int every, int count)
{
    shot_every = every, shots_left = count;
}

static void shoot(void)
{
    static unsigned int rgba[640 * 512];
    char path[32];
    Display d = gpu_display();

    snprintf(path, sizeof path, "shot_%05d.bmp", frame);
    display_to_rgba(&d, rgba);
    save_bmp(path, rgba, d.w, d.h);
}

/* Where a CD read starts (cd_native.c). */
void lab_file(const char *path, unsigned int sector)
{
    if (trace)
        fprintf(trace, "%d read %s +%u\n", frame, path, sector);
}

/* Once a vblank, after the window has read the keyboard and pads; 1 when the lab is done. */
int lab_frame(void)
{
    volatile unsigned char *choice;
    int state[4], i, done = 0;

    frame++;
    while (next_input < input_count && inputs[next_input].frame <= frame)
        next_input++;
    if (next_input) /* from its first line on, the script holds the pad */
        pad_buttons = inputs[next_input - 1].buttons;
    /* the warp, as a gateway asks for it, in a field (mode 1) with nothing asked for (a movie is asked for, 3, and
     * plays, 4, through the request too); again until it's there, as a field setting up clears what's asked */
    if (warp_field == *FIELD)
        warp_field = -1;
    if (warp_field >= 0 && *GAME_MODE == 1 && *FIELD && !*REQUEST) {
        *REQUEST = 1;
        *REQUEST_ID = warp_field;
        if (trace)
            fprintf(trace, "%d warp %04X\n", frame, warp_field);
    }
    /* the same for a mini-game (back to this field after it), until the main loop has left the field for it */
    if (minigame_asked && *GAME_MODE != 1)
        minigame = -1;
    if (minigame >= 0 && warp_field < 0 && *GAME_MODE == 1 && *FIELD && !*REQUEST) {
        *REQUEST = 0xC;
        *REQUEST_ID = *FIELD;
        *MINIGAME = minigame;
        minigame_asked = 1;
        if (trace)
            fprintf(trace, "%d minigame %d\n", frame, minigame);
    }
    if (battle_asked && *GAME_MODE != 1)
        battle = -1;
    if (battle >= 0 && warp_field < 0 && *GAME_MODE == 1 && *FIELD && !*REQUEST) {
        *REQUEST = 2;
        *REQUEST_ID = battle;
        battle_asked = 1;
        if (trace)
            fprintf(trace, "%d battle %04X\n", frame, battle);
    }
    /* the command as the menu puts it in (func_800A4350), a limit ahead of the rest; an item isn't used up
     * (func_800A5660 isn't called) */
    if (command.actor >= 0 && *GAME_MODE == 2 && *WAITING >> command.actor & 1
        && *CHOICE_READ != ((*CHOICE_WRITE + 1) & 15)) {
        choice = CHOICES + 8 * *CHOICE_WRITE;
        choice[0] = command.command == 0x14 ? 5 : 6;
        choice[2] = command.actor;
        choice[3] = command.command;
        *(volatile unsigned short *)(choice + 4) = command.attack;
        *(volatile unsigned short *)(choice + 6) = command.targets;
        for (i = 0; i < 16 && MENU_LIST[i] != command.actor; i++)
            ;
        for (; i < 15 && MENU_LIST[i] != 0xFF; i++) /* off the menu's list (func_800A4D88) */
            MENU_LIST[i] = MENU_LIST[i + 1];
        *WAITING &= ~(1u << command.actor);
        *CHOSEN |= 1u << command.actor;
        *CHOICE_WRITE = (*CHOICE_WRITE + 1) & 15;
        if (trace)
            fprintf(trace, "%d command %d %d %d %04X\n", frame, command.actor, command.command, command.attack,
                    command.targets);
        command.actor = -1;
        shots_from = frame;
    }
    if (shots_left && command.actor < 0 && !((frame - shots_from) % shot_every)) {
        shoot();
        done = !--shots_left;
    }
    for (i = 0; i < code_count; i++) {
        if (codes[i].bits == 16)
            *(volatile unsigned short *)codes[i].addr = codes[i].value;
        else
            *(volatile unsigned char *)codes[i].addr = codes[i].value;
    }
    if (frame == dump_frame || (dump_every && dump_frame >= 0 && frame > dump_frame
                                && !((frame - dump_frame) % dump_every)))
        lab_dump();
    if (!trace)
        return done;
    state[0] = *GAME_MODE, state[1] = *FIELD, state[2] = *REQUEST, state[3] = *REQUEST_ID;
    if (memcmp(state, last_state, sizeof state)) {
        fprintf(trace, "%d state mode %d field %04X request %d %04X\n", frame, state[0], state[1], state[2], state[3]);
        memcpy(last_state, state, sizeof state);
    }
    if (pad_buttons != last_pad) {
        fprintf(trace, "%d pad", frame);
        for (i = 0; i < 16; i++) {
            if (!(pad_buttons >> i & 1))
                fprintf(trace, " %s", button_names[i]);
        }
        fputs(pad_buttons == 0xFFFF ? " -\n" : "\n", trace);
        last_pad = pad_buttons;
    }
    fflush(trace);
    return done;
}
