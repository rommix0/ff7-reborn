/* The PC build's entry: maps the PS1's RAM, loads the user's own exe into it, and runs the game from the exe's entry
 * point, recompiled. usage: ff7 [ASSETS] [--frames N] [--shot FILE.bmp]
 *
 * With --frames, it saves what's on screen after N frames (to --shot, or screenshot.bmp) and exits. */
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
/* sdl.c: the window, unless --frames asks for a run without one. */
int sdl_open(void);
int sdl_frame(void);

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

    if (frames_left < 0) {
        if (!sdl_frame())
            exit(0);
        return;
    }
    if (--frames_left)
        return;
    d = gpu_display();
    display_to_rgba(&d, rgba);
    exit(save_bmp(shot, rgba, d.w, d.h) ? 1 : 0);
}

int main(int argc, char **argv)
{
    const char *assets = "assets";
    char exe[600];
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            frames_left = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc)
            shot = argv[++i];
        else
            assets = argv[i];
    }
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
