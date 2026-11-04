/* The PC build's entry: maps the PS1's RAM, loads the user's own exe into it, and runs the game from the exe's entry
 * point, recompiled. usage: ff7 [ASSETS] [--frames N] [--shot FILE.bmp] [--trace FILE] [--input FILE] [--field N]
 * [--minigame N] [--battle N] [--command A,C,X,T] [--shots K N] [--dump-at N] [--code "AAAAAAAA VVVV"]... [--fast]
 *
 * With --frames, it saves what's on screen after N frames (to --shot, or screenshot.bmp) and exits. The rest is the
 * reversing lab (lab.c): --field N warps to field N (0x41 is the debug room) once the first field has loaded,
 * --minigame N then starts mini-game N (0-6) as a field would, --battle N battle N; --command puts in party member
 * A's command C (attack X, target mask T) once the battle's menu waits on A, and --shots saves N pictures K frames
 * apart from then on, then exits; --code holds a GameShark code every frame; --fast runs frames as quickly as the
 * host can instead of at 59.94 Hz. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cd_native.h"
#include "gpu.h"
#include "memory.h"
#include "recomp.h"

#define EXE_ENTRY 0x800110C0u

/* build/pc/gen/exe.c: the exe's functions that aren't in C, recompiled from the user's copy. */
extern const CodePatch exe_code[], exe_patches[];
extern const int exe_code_count, exe_patch_count;
int D_800110C0(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7); /* the entry point */
/* build/pc/gen/module_list.c: the modules recompiled from the user's discs (the Makefile's PC_MODULES). */
extern const Module *const pc_modules[];
extern const int pc_module_count;
/* hw.c: the clock frames wait for; --fast gives it the PS1's own time, which is always caught up. */
void hw_set_clock(double (*now)(void));
double ps1_seconds(void);
/* sdl.c: the window, unless --frames asks for a run without one. */
int sdl_open(void);
int sdl_frame(void);
int lab_open(const char *trace_path, const char *input_path, int warp_field, int dump_frame);
int lab_frame(void);
int lab_code(const char *code);
void lab_minigame(int game);
void lab_battle(int number);
void lab_command(int actor, int command, int attack, int targets);
void lab_shots(int every, int count);
void lab_dump_every(int frames);

Ctx ctx;
unsigned int cop0[32];
static int frames_left = -1;
static const char *shot = "screenshot.bmp";

void recomp_missing(unsigned int addr)
{
    fprintf(stderr, "reached %08X, which wasn't recompiled\n", addr);
    exit(1);
}

void recomp_break(unsigned int code)
{
    fprintf(stderr, "break %05X\n", code);
    exit(1);
}

void pc_vblank(void)
{
    static unsigned int rgba[640 * 512];
    Display d;

    if (frames_left < 0 && !sdl_frame())
        exit(0);
    if (lab_frame())
        exit(0);
    if (frames_left < 0 || --frames_left)
        return;
    d = gpu_display();
    display_to_rgba(&d, rgba);
    exit(save_bmp(shot, rgba, d.w, d.h) ? 1 : 0);
}

int main(int argc, char **argv)
{
    const char *assets = "assets", *trace = 0, *input = 0;
    char exe[600];
    int i, field = -1, dump_at = -1, a, c, x, t;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            frames_left = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc)
            shot = argv[++i];
        else if (!strcmp(argv[i], "--trace") && i + 1 < argc)
            trace = argv[++i];
        else if (!strcmp(argv[i], "--input") && i + 1 < argc)
            input = argv[++i];
        else if (!strcmp(argv[i], "--field") && i + 1 < argc)
            field = strtol(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--minigame") && i + 1 < argc)
            lab_minigame(strtol(argv[++i], 0, 0));
        else if (!strcmp(argv[i], "--battle") && i + 1 < argc)
            lab_battle(strtol(argv[++i], 0, 0));
        else if (!strcmp(argv[i], "--command") && i + 1 < argc) {
            if (sscanf(argv[++i], "%i,%i,%i,%i", &a, &c, &x, &t) != 4) {
                fprintf(stderr, "%s: expected actor,command,attack,targets\n", argv[i]);
                return 1;
            }
            lab_command(a, c, x, t);
        }
        else if (!strcmp(argv[i], "--shots") && i + 2 < argc) {
            a = atoi(argv[++i]);
            lab_shots(a, atoi(argv[++i]));
        }
        else if (!strcmp(argv[i], "--dump-at") && i + 1 < argc)
            dump_at = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump-every") && i + 1 < argc)
            lab_dump_every(atoi(argv[++i]));
        else if (!strcmp(argv[i], "--fast"))
            hw_set_clock(ps1_seconds);
        else if (!strcmp(argv[i], "--code") && i + 1 < argc) {
            if (lab_code(argv[++i])) {
                fprintf(stderr, "%s: not a GameShark code the lab takes (80AAAAAA VVVV or 30AAAAAA 00VV)\n", argv[i]);
                return 1;
            }
        }
        else
            assets = argv[i];
    }
    if (lab_open(trace, input, field, dump_at))
        return 1;
    if (pc_cd_open(assets) != 3) {
        fprintf(stderr, "%s: expected the three discs extracted with tools/extract_disc.py\n", assets);
        return 1;
    }
    snprintf(exe, sizeof(exe), "%s/SCUS_941.63", pc_cd_folder(1));
    if (mem_map() || exe_load(exe) != EXE_ENTRY) {
        fprintf(stderr, "%s: can't map the PS1's memory or load the exe\n", exe);
        return 1;
    }
    if (frames_left < 0 && sdl_open())
        return 1;
    code_register(exe_code, exe_code_count);
    patch_code_pointers(exe_patches, exe_patch_count);
    for (i = 0; i < pc_module_count; i++)
        module_register(pc_modules[i]);
    gpu_gp1(0);
    cop0[12] = 0x401;           /* interrupts on, as the BIOS leaves them; I_MASK keeps them away until the game asks */
    ctx.r[29] = 0x801FFF00;     /* the BIOS's stack, until the entry code sets the game's */
    D_800110C0(0, 0, 0, 0, 0, 0, 0, 0);
    return 0;
}
