#ifndef MAIN_H
#define MAIN_H

/* Where a file is on the disc. */
typedef struct DiscFile {
    unsigned int lba;
    unsigned int size;
} DiscFile;

extern DiscFile disc_files[]; /* by number */

/* libgpu's display and drawing environments (layouts from Sony's Libref). */
typedef struct RECT {
    short x, y, w, h;
} RECT;

typedef struct DISPENV {
    RECT disp;
    RECT screen;
    unsigned char isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct DRAWENV {
    RECT clip;
    short ofs[2];
    RECT tw;
    unsigned short tpage;
    unsigned char dtd, dfe, isbg, r0, g0, b0;
    unsigned int dr_env[16];
} DRAWENV;

/* Picks what runs next (func_800146A4). Shared with the VSync callback, hence volatile: the game reloads it and reads
 * it through lhu. */
extern volatile short game_mode;
/* Written twice in a row by func_800111E4, so volatile. */
extern volatile short D_800707BE;

/* The CD layer (cd.c): start a request, then poll cd_step until it returns 0. */
int cd_read(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int cd_seek(unsigned int lba, void (*callback)(void));
int cd_step(void);
void read_disc_file(int i, void *dest, void (*callback)(void));
void wait_disc_read(void (*idle)(void));
void func_80015CA0(void *src, void *dest); /* unpacks a module */
void func_800140F4(void);
int DrawSync(int mode);

#endif
