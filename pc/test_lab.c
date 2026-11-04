/* The reversing lab: what the game does each frame in the trace, the pad played from a script, a warp to a field once
 * the game can take one, and dumps of RAM, VRAM and SPU RAM. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gpu.h"
#include "memory.h"

int lab_open(const char *trace_path, const char *input_path, int warp_field, int dump_frame);
int lab_frame(void);
void lab_file(const char *path, unsigned int sector);
int lab_code(const char *code);
void lab_minigame(int game);
void lab_battle(int battle);
void lab_command(int actor, int command, int attack, int targets);
void lab_shots(int every, int count);

#define GAME_MODE ((volatile short *)0x8009C560)
#define FIELD ((volatile unsigned short *)0x8009A05C)
#define REQUEST ((volatile unsigned char *)0x8009ABF5)
#define REQUEST_ID ((volatile unsigned short *)0x8009ABF6)
#define MINIGAME ((volatile unsigned char *)0x8009ACE6)
#define WAITING ((volatile unsigned short *)0x800F7DAC)
#define CHOSEN ((volatile unsigned short *)0x800F7DC2)
#define MENU_LIST ((volatile unsigned char *)0x800E7A48)
#define CHOICES ((volatile unsigned char *)0x800F3958)
#define CHOICE_READ ((volatile int *)0x800F39D8)
#define CHOICE_WRITE ((volatile int *)0x800F39DC)

unsigned short pad_buttons = 0xFFFF;
unsigned short vram[512][1024];
static unsigned short spu[0x40000];
const unsigned short *spu_memory(void) { return spu; }
Display gpu_display(void) { Display d = {0, 0, 4, 2, 0}; return d; }

static long file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;

    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);
    return size;
}

int main(void)
{
    static char text[4096];
    FILE *f;
    size_t n;

    assert(!mem_map());
    f = fopen("lab_input.txt", "w");
    fputs("# frame, then the buttons held from it\n3 up cross\n5 -\n", f);
    fclose(f);
    assert(!lab_open("lab_trace.txt", "lab_input.txt", 0x41, 4));

    /* frame 1: the opening movie asked for, so no warp yet */
    *GAME_MODE = 1, *FIELD = 0x74, *REQUEST = 3, *REQUEST_ID = 0x35;
    lab_frame();
    assert(*REQUEST == 3 && *REQUEST_ID == 0x35);
    /* frame 2: nothing asked for: the warp, as a field gateway asks for it */
    *REQUEST = 0;
    lab_frame();
    assert(*REQUEST == 1 && *REQUEST_ID == 0x41);
    /* frame 3: the field, still setting up, cleared it: asked for again; the script's buttons, active low; a read */
    *REQUEST = 0;
    lab_frame();
    assert(*REQUEST == 1 && *REQUEST_ID == 0x41);
    assert(pad_buttons == (unsigned short)~(1u << 4 | 1u << 14));
    lab_file("FIELD/MD1STIN.DAT", 2);
    /* frame 4: there: no more warps; the dumps */
    *FIELD = 0x41, *REQUEST = 0;
    lab_frame();
    assert(!*REQUEST);
    assert(file_size("dump_00004_ram.bin") == 0x200000 && file_size("dump_00004_vram.bin") == 0x100000
           && file_size("dump_00004_spu.bin") == 0x80000);
    /* frame 5: released */
    lab_frame();
    assert(pad_buttons == 0xFFFF);

    /* gameshark_codes: a 16-bit and an 8-bit constant write, held every frame; other code types aren't taken */
    assert(!lab_code("8009D2A6 03FF") && !lab_code("30010001 0012") && lab_code("D009D2A6 0000"));
    lab_frame();
    assert(*(unsigned short *)0x8009D2A6 == 0x03FF && *(unsigned char *)0x80010001 == 0x12);
    *(unsigned short *)0x8009D2A6 = 0;
    lab_frame();
    assert(*(unsigned short *)0x8009D2A6 == 0x03FF);

    /* a mini-game (frames 8-11), asked for as a field asks (request 0xC, back to this field, game 2), again if the
     * field clears it, and not again once the main loop has left the field for it */
    lab_minigame(2);
    *GAME_MODE = 1, *FIELD = 0x41, *REQUEST = 0;
    lab_frame();
    assert(*REQUEST == 0xC && *REQUEST_ID == 0x41 && *MINIGAME == 2);
    *REQUEST = 0;
    lab_frame();
    assert(*REQUEST == 0xC);
    *GAME_MODE = 6, *REQUEST = 0;
    lab_frame();
    *GAME_MODE = 1;
    lab_frame();
    assert(!*REQUEST);

    /* a battle (frames 12-14), asked for as an encounter asks (request 2, its number), until the main loop has left
     * the field for it */
    lab_battle(0x12C);
    *REQUEST = 0;
    lab_frame();
    assert(*REQUEST == 2 && *REQUEST_ID == 0x12C);
    *GAME_MODE = 2, *REQUEST = 0;
    lab_frame();
    *GAME_MODE = 1;
    lab_frame();
    assert(!*REQUEST);

    /* a command (frames 15-19): nothing while Cloud's menu isn't waiting; then as the menu puts one in
     * (func_800A4350): the ring's next choice, Cloud off the menu's list, his bit moved from waiting to chosen; once
     * only. Pictures from then on, every 2nd frame, 2 of them, and the lab is done. */
    lab_command(0, 1, 0, 0x10);
    lab_shots(2, 2);
    *GAME_MODE = 2, *CHOICE_READ = 3, *CHOICE_WRITE = 15, *WAITING = 4, *CHOSEN = 0;
    memcpy((void *)MENU_LIST, "\x02\x00\x01\xFF", 4);
    assert(!lab_frame() && *CHOICE_WRITE == 15 && file_size("shot_00015.bmp") < 0);
    *WAITING = 5;
    assert(!lab_frame() && *CHOICE_WRITE == 0);
    assert(!memcmp((void *)(CHOICES + 15 * 8 + 2), "\x00\x01\x00\x00\x10\x00", 6) && CHOICES[15 * 8] == 6);
    assert(!memcmp((void *)MENU_LIST, "\x02\x01\xFF", 3) && *WAITING == 4 && *CHOSEN == 1);
    assert(file_size("shot_00016.bmp") == 54 + 4 * 2 * 4);
    *WAITING = 5;
    assert(!lab_frame() && *CHOICE_WRITE == 0 && file_size("shot_00017.bmp") < 0);
    assert(lab_frame() && file_size("shot_00018.bmp") == 54 + 4 * 2 * 4);

    f = fopen("lab_trace.txt", "r");
    n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[n] = 0;
    assert(strstr(text, "1 state mode 1 field 0074 request 3 0035\n"));
    assert(strstr(text, "2 warp 0041\n"));
    assert(strstr(text, "2 state mode 1 field 0074 request 1 0041\n"));
    assert(strstr(text, "3 warp 0041\n") && !strstr(text, "4 warp"));
    assert(strstr(text, "3 pad up cross\n"));
    assert(strstr(text, "3 read FIELD/MD1STIN.DAT +2\n"));
    assert(strstr(text, "4 dump\n"));
    assert(strstr(text, "5 pad -\n"));
    assert(strstr(text, "8 minigame 2\n") && strstr(text, "9 minigame 2\n") && !strstr(text, "10 minigame"));
    remove("lab_input.txt");
    remove("lab_trace.txt");
    remove("dump_00004_ram.bin");
    remove("dump_00004_vram.bin");
    remove("dump_00004_spu.bin");
    remove("shot_00016.bmp");
    remove("shot_00018.bmp");
    puts("ok");
    return 0;
}
