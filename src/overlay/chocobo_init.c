/* MINI/CHOCOBO.BIN's setup, 0x800A1630-0x800A2984: the race's state, its files, sound and scene. */

#include "chocobo.h"

int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
void SetGeomOffset(int x, int y);
void SetGeomScreen(int h);
void SetDispMask(int mask);
void SetBackColor(int r, int g, int b);
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
void SetPolyF4(void *p);
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* SetDefDrawEnv */

/* A chocobo's colours, as signed bytes. */
typedef struct Tint {
    signed char c[4];
} Tint;

extern unsigned char D_8009D288, D_8009D289; /* the gil's low bytes */
extern unsigned char D_8009D48E, D_8009D48F, D_8009D491, D_8009D499, D_8009D49F;
extern Tint D_800B1258[6];        /* each chocobo's */
extern Tint D_800B1270[5], D_800B1284[5]; /* the player's, by its colour: two shades */
extern POLY_FT4 D_800B1298, D_800B12C0; /* the course map's and the stamina frame's */
extern int D_800B74F8;
extern Tint D_800B7518[6]; /* the racers' */
extern int D_800B7530[], D_800F5030[]; /* the bss's start and end */
extern short D_800F5038, D_800F503A, D_800F503C;
extern int D_800F504C;
extern char *D_800F5120; /* where the next figure's primitives go */

void chocobo_init_figures(FigureFile *file, int alt);
void chocobo_load_files(int alt, int course);
void chocobo_load_sounds(void);

/* Writes a tint for chocobo_tint_figure. */
#define SET_TINT(p, v0, v1, v2, flag)                                                                                  \
    ((p)[0] = (v0), (p)[1] = (v0) >> 8, (p)[2] = (v1), (p)[3] = (v1) >> 8, (p)[4] = (v2), (p)[5] = (v2) >> 8,          \
     (p)[6] = (flag))

/* end of declarations */

/* Resets the race's state; all four shoulder buttons held pick the course's variant by the d-pad (debug). */
void chocobo_init_state(void)
{
    unsigned int held;

    D_800F5084 = (Course *)0x80110000;
    D_800B7514 = 0xFF;
    D_800F5070 = -16;
    D_800B759C = -1;
    D_800B74FC = D_800F5084->nsegments;
    D_800B7478 = 0;
    D_800B7500 = D_800F5084->segments;
    D_800B747C = -(D_8009D288 + (D_8009D289 << 8) >= 1000);
    held = func_8001C808() >> 16;
    if ((held & 0xF0) == 0xF0) {
        if (held & 8)
            D_8009D48E = 1;
        if (held & 2)
            D_8009D48E = 2;
    }
    if (D_8009D490)
        D_8009D491 = 0xFF;
    D_800B7538 = 0;
    D_800B74F8 = 0;
    D_800B753C = 0;
    D_800F504C = -1;
    D_800F508C = -1;
    D_800F5050 = (D_800F5098 + 3) * 2;
    D_800F5098 = D_8009D49F;
    if (D_8009D491)
        D_800B7A48 = -1;
    else
        D_800B7A48 = 0;
    D_800F5124 = 1;
    D_800F5088 = -1;
    D_800B7594 = -1;
    D_800F5040 = 0;
    D_800F5048 = 0;
    D_800B7A64 = 0;
    D_800F5044 = 0;
    D_800B7660 = 30;
    D_800B753C = 0;
}

void chocobo_init_sound(void)
{
    snd_request.id = 0x23;
    snd_request.arg[0] = 0x40;
    snd_request.arg[4] = 0;
    snd_request.arg[3] = 0;
    snd_request.arg[2] = 0;
    snd_request.arg[1] = 0;
    snd_submit_request();
    snd_request.id = 0xA0;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0xA1;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0xA2;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0xA3;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
}

/* Clears the bss, sets up the GTE and both display buffers (their environments, ordering tables, the course map's
 * racer markers, the fade and the HUD's templates), loads the course, then its figures and colours. */
#ifdef NON_MATCHING
/* Which buffer addresses GCC derives from others (it CSEs related addresses) and register choice. */
const RECT D_800A0018 = {0, 0, 0x140, 0x1E0};

void chocobo_init(void)
{
    RECT r = D_800A0018;
    int *p;
    int i;
    POLY_F4 *f0, *f1;

    for (p = D_800B7530; p != D_800F5030; p++)
        *p = 0;
    SetGeomOffset(160, 120);
    SetGeomScreen(270);
    SetDispMask(1);
    func_80043814(&D_800B7A68[0].draw, 0, 0, 320, 232);
    func_80043814(&D_800B7A68[1].draw, 0, 240, 320, 232);
    D_800B7A68[0].draw.tpage = GetTPage(0, 1, 0, 0);
    D_800B7A68[1].draw.tpage = D_800B7A68[0].draw.tpage;
    D_800B7A68[0].draw.isbg = 0;
    D_800B7A68[1].draw.isbg = 0;
    SetDefDispEnv(&D_800B7A68[0].disp, 0, 240, 320, 232);
    SetDefDispEnv(&D_800B7A68[1].disp, 0, 0, 320, 232);
    D_800B7A68[1].draw.dfe = 1;
    D_800B7A68[0].draw.dfe = 1;
    func_80044244(D_800B7A68[0].ot, 0x1000);
    func_80044244(D_800B7A68[1].ot, 0x1000);
    func_8004418C(D_800B7A68[0].ot_ground, 4);
    func_8004418C(D_800B7A68[1].ot_ground, 4);
    ClearImage(&r, 0, 0, 0);
    SetBackColor(64, 64, 64);
    SetDispMask(1);
    func_800444AC(&D_800B7A68[0].disp);
    PutDrawEnv(&D_800B7A68[1].draw);
    srand(VSync(-1));
    chocobo_init_state();
    D_800F5038 = 0;
    D_800F503A = 0;
    D_800F503C = 0;
    chocobo_load_sounds();
    chocobo_load_files(D_8009D48F, D_8009D49F);
    D_800F5054 = *D_800F5084->shots;
    if (D_800B7A48)
        D_800F5054.kind = -1;
    D_800F5054.kind = -1;
    for (i = 0; i < 6; i++) {
        f0 = &D_800B7A68[0].markers[i];
        f1 = &D_800B7A68[1].markers[i];
        SetPolyF4(f0);
        SetPolyF4(f1);
        if (i & 2)
            f0->r0 = 0xFF;
        else
            f0->r0 = 0;
        if (i & 4)
            f0->g0 = 0xFF;
        else
            f0->g0 = 0;
        if (i & 1)
            f0->b0 = 0xFF;
        else
            f0->b0 = 0;
        if (i & 2)
            f1->r0 = 0xFF;
        else
            f1->r0 = 0;
        if (i & 4)
            f1->g0 = 0xFF;
        else
            f1->g0 = 0;
        if (i & 1)
            f1->b0 = 0xFF;
        else
            f1->b0 = 0;
    }
    D_800B7A68[0].fade.y1 = 0;
    D_800B7A68[0].fade.y0 = 0;
    D_800B7A68[0].fade.x2 = 0;
    D_800B7A68[0].fade.x0 = 0;
    D_800B7A68[0].fade.x3 = 320;
    D_800B7A68[0].fade.x1 = 320;
    D_800B7A68[0].fade.y3 = 232;
    D_800B7A68[0].fade.y2 = 232;
    SetPolyF4(&D_800B7A68[0].fade);
    func_80046848(&D_800B7A68[0].fade, 1);
    D_800B7A68[1].fade = D_800B7A68[0].fade;
    D_800B7A68[0].map = D_800B1298;
    SetPolyFT4(&D_800B7A68[0].map);
    func_80046848(&D_800B7A68[0].map, 1);
    D_800B7A68[0].map.clut = GetClut(0x240, 0x80);
    D_800B7A68[0].map.tpage = GetTPage(0, 0, 0x180, 0);
    D_800B7A68[1].map = D_800B7A68[0].map;
    D_800B7A68[0].stamina_frame = D_800B12C0;
    SetPolyFT4(&D_800B7A68[0].stamina_frame);
    func_80046848(&D_800B7A68[0].stamina_frame, 1);
    D_800B7A68[0].stamina_frame.clut = GetClut(0x240, 0x81);
    D_800B7A68[0].stamina_frame.tpage = GetTPage(0, 0, 0x180, 0);
    D_800B7A68[1].stamina_frame = D_800B7A68[0].stamina_frame;
    SetPolyF4(&D_800B7A68[0].stamina_bar);
    D_800B7A68[0].stamina_bar.x2 = 25;
    D_800B7A68[0].stamina_bar.x0 = 25;
    D_800B7A68[0].stamina_bar.x3 = 30;
    D_800B7A68[0].stamina_bar.x1 = 30;
    D_800B7A68[0].stamina_bar.y3 = 209;
    D_800B7A68[0].stamina_bar.y2 = 209;
    D_800B7A68[1].stamina_bar = D_800B7A68[0].stamina_bar;
    chocobo_init_figures(D_800B1254, D_8009D48F);
    chocobo_load_cluts();
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_init", chocobo_init);
#endif

/* Sets up the figures: the six chocobos (in their colours), the riders and the rest; then lights the props that are
 * lit. */
void chocobo_init_figures(FigureFile *file, int alt)
{
    unsigned int i;
    int id, k;
    Figure *o;
    Prop *p;

    D_800F5120 = alt ? (char *)0x801AE000 : (char *)0x801AE800;
    *(int *)0x1F800000 = 3;
    *(int *)0x1F800200 = 1;
    for (i = 0; i < 6; i++) {
        D_800B75CC[i].chocobo = i;
        o = &file->figures[i];
        o->scale = 0x280;
        D_800F5120 = chocobo_setup_figure_prims(o, D_800F5120, i);
        if (D_8009D49E && i == 1) {
            SET_TINT(D_800B1250, -100, -100, -100, i);
            D_800B66CC[0] = 0;
            chocobo_tint_figure(o, D_800B1250);
            D_800B66CC[0] = i;
            chocobo_tint_figure(o, D_800B1250);
            D_800B7518[1].c[0] = D_800B7518[1].c[1] = D_800B7518[1].c[2] = 0x40;
        } else if (D_800B7A48 && i == 0) {
            k = D_8009D499;
            D_800B7518[0] = D_800B1270[k];
            SET_TINT(D_800B1250, D_800B1270[k].c[0], D_800B1270[k].c[1], D_800B1270[k].c[2], 1);
            D_800B66CC[0] = 0;
            chocobo_tint_figure(o, D_800B1250);
            SET_TINT(D_800B1250, D_800B1284[k].c[0], D_800B1284[k].c[1], D_800B1284[k].c[2], 1);
            D_800B66CC[0] = 1;
            chocobo_tint_figure(o, D_800B1250);
        } else {
            D_800B7518[i] = D_800B1258[i];
            SET_TINT(D_800B1250, D_800B1258[i].c[0], D_800B1258[i].c[1], D_800B1258[i].c[2], 1);
            D_800B66CC[0] = 0;
            chocobo_tint_figure(o, D_800B1250);
            SET_TINT(D_800B1250, D_800B1258[i].c[0], D_800B1258[i].c[1], D_800B1258[i].c[2], 1);
            D_800B66CC[0] = 1;
            chocobo_tint_figure(o, D_800B1250);
        }
    }
    id = 6;
    if (D_800B7A48) {
        D_800B75CC[0].rider = D_8009D48E + 6;
        o = &file->figures[D_8009D48E + 6];
    } else {
        D_800B75CC[0].rider = 10;
        o = &file->figures[10];
    }
    o->scale = 0x280;
    D_800F5120 = chocobo_setup_figure_prims(o, D_800F5120, id++);
    if (D_8009D49E) {
        D_800B75CC[1].rider = 9;
        o = &file->figures[9];
    } else {
        D_800B75CC[1].rider = 11;
        o = &file->figures[11];
    }
    o->scale = 0x280;
    D_800F5120 = chocobo_setup_figure_prims(o, D_800F5120, id++);
    for (i = 12; i < 16; i++) {
        D_800B75CC[i - 10].rider = i;
        o = &file->figures[i];
        o->scale = 0x280;
        D_800F5120 = chocobo_setup_figure_prims(o, D_800F5120, id++);
    }
    for (i = 16; i < file->count; i++) {
        o = &file->figures[i];
        o->scale = 0x280;
        D_800F5120 = chocobo_setup_figure_prims(o, D_800F5120, id++);
    }
    /* the game sets y where chocobo_orient_racers sets ry */
    for (i = 0; i < file->count; i++) {
        o = &file->figures[i];
        o->rx = 0xC0;
        o->y = 0;
        o->rz = 0x80;
    }
    for (i = 0; i < D_800F5084->nprops; i++) {
        p = (Prop *)(i * 16 + (int)D_800F5084->props); /* &props[i], with the game's addu operand order */
        if (p->pos.pad & 1) {
            o = &file->figures[p->figure];
            *(unsigned char *)0x1F800200 = 0xFF;
            chocobo_set_figure_semitrans(o, (unsigned char *)0x1F800200);
        }
    }
}


const RECT D_800A0020 = {0x180, 0, 0x100, 0x100};
const RECT D_800A0028 = {0x140, 0x100, 0x180, 0x100};
const int D_800A0030 = 0x20000; /* nothing reads it */

/* Loads the race's textures into VRAM through 0x80110000, then the files for this course into 0x80190000 and
 * 0x80110000. */
void chocobo_load_files(int alt, int course)
{
    RECT r0 = D_800A0020;
    RECT r1 = D_800A0028;

    cd_read_lzs(alt ? 0x3C1 : 0x32C, 0x20000, (void *)0x80110000, 0);
    while (cd_step())
        ;
    func_80044000(&r0, (void *)0x80110000);
    DrawSync(0);
    if (alt)
        cd_read_lzs(0x3F1, 0x1E000, (void *)0x80190000, 0);
    else
        cd_read_lzs(0x3CE, 0x1E800, (void *)0x80190000, 0);
    while (cd_step())
        ;
    switch (course) {
    case 0:
        cd_read_lzs(0x459, 0x30000, (void *)0x80110000, 0);
        break;
    case 1:
        cd_read_lzs(0x433, 0x30000, (void *)0x80110000, 0);
        break;
    case 2:
        cd_read_lzs(0x417, 0x30000, (void *)0x80110000, 0);
        break;
    case 3:
        cd_read_lzs(0x49C, 0x30000, (void *)0x80110000, 0);
        break;
    }
    while (cd_step())
        ;
    func_80044000(&r1, (void *)0x80110000);
    DrawSync(0);
    if (alt)
        cd_read_lzs(0x33E, 0x6A000, (void *)0x80110000, 0);
    else
        cd_read_lzs(0x293, 0x7D000, (void *)0x80110000, 0);
    while (cd_step())
        ;
}

/* Reads the race's four sound files into the exe's buffers. */
void chocobo_load_sounds(void)
{
    cd_read_lzs(0x4C9, 0x1000, D_80079F64, 0);
    while (cd_step())
        ;
    cd_read_lzs(0x4CA, 0x1000, D_8007AF64, 0);
    while (cd_step())
        ;
    cd_read_lzs(0x4C8, 0x800, D_8007BF64, 0);
    while (cd_step())
        ;
    cd_read_lzs(0x4C7, 0x800, D_8007C764, 0);
    while (cd_step())
        ;
}
