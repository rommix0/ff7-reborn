#include "common.h"
#include "sound.h"
#include "main.h"

/* The game's boot file (0x8001117C-0x800144D8): it boots the sound driver, sets up the display and runs the main loop.
 * Built like the sound driver: GCC 2.6.3, ASPSX 2.21 (indexed loads go through `addiu $at`). */

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

/* libgpu primitives: each starts with an ordering-table link, the next primitive's address in 24 bits. */
typedef struct OTag {
    unsigned int next : 24;
    unsigned int len : 8;
} OTag;

typedef struct DR_MODE {
    unsigned int tag;
    unsigned int code[2];
} DR_MODE;

typedef struct TILE {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    short w, h;
} TILE;

/* Links primitive p in after ordering-table entry ot, as libgpu's addPrim. */
#define add_prim(ot, p) (((OTag *)(p))->next = ((OTag *)(ot))->next, ((OTag *)(ot))->next = (unsigned int)(p))
/* libgpu's setRGB0. The game passes a fresh &array[frame] each time, so the pointer is rebuilt for every field. */
#define set_rgb(p, r, g, b) ((p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b))
#define set_xy(p, x, y) ((p)->x0 = (x), (p)->y0 = (y))
#define set_wh(p, _w, _h) ((p)->w = (_w), (p)->h = (_h))

/* A full-screen fade for one of the two buffers: its blend mode and two 160-pixel-wide tiles. */
typedef struct FadeTiles {
    DR_MODE mode;
    TILE tile[2];
    unsigned int pad;
} FadeTiles;

/* An entry of the table at 0x80074EB0, 0x84 bytes. */
typedef struct FieldObject {
    int x;                              /* 0x00 << 12 */
    int y;                              /* 0x04 << 12 */
    unsigned char pad_08[0x22];
    unsigned char unk_2A;
    unsigned char pad_2B[0x3B];
    unsigned short unk_66;
    unsigned char pad_68[0x1C];
} FieldObject;

/* State func_800119E4 saves and func_80011AEC puts back. Volatile: the game reads every field in order, and one field
 * twice. */
typedef struct SavedState {
    int unk_00;
    short mode;                         /* game_mode */
    short unk_06;
    short unk_08;
    short unk_0A;                       /* a FieldObject's x */
    short unk_0C;                       /* and y */
    short unk_0E;
    unsigned char unk_10;
    unsigned char unk_11;
    unsigned char unk_12;
} SavedState;

extern DiscFile instr_all_file;
extern DiscFile effect_all_file;
extern DiscFile instr_dat_file;
extern DiscFile instr2_all_file;
extern DiscFile instr2_dat_file;
extern DiscFile D_80048D24;
extern unsigned short D_800965E8;
extern short D_80071A5C;
extern DISPENV disp_env[2];
extern DRAWENV draw_env[2];
extern volatile short frame; /* which of the two buffers is being drawn */
extern unsigned char D_80071A58;
extern unsigned int D_8007E7A0[];
extern volatile short D_800965EC;
extern volatile SavedState D_8009D274;
extern short D_8009A05C;
extern short D_800965E0;
extern FieldObject D_80074EB0[];
extern unsigned char D_8009C540;
extern unsigned char D_8009AD2C;
extern int D_80071E28;
extern short D_8009AC58;
extern short D_8009ABF8;
extern short D_8009ABFA;
extern short D_8009AC16;
extern short D_8009AC18;
extern FadeTiles D_8009A068[2];
extern int D_8019DAA0;
extern unsigned int *D_8019D5E8;
extern DiscFile D_80048D2C;
extern int D_800730CC;
extern unsigned char D_80095DDC[];
extern short D_8009ABF6;
extern unsigned char D_80062D98;
extern unsigned char D_80062D99;
/* Play time: D_8009D26C gains 0x444 a frame, so its top half is a second at 60 Hz, counted in D_8009D264. D_8009D270
 * does the same for the countdown D_8009D268, which counts up instead while D_8009D2E7 bit 1 is set. */
extern volatile unsigned int D_8009D264;
extern volatile unsigned int D_8009D268;
extern volatile unsigned int D_8009D26C;
extern volatile unsigned int D_8009D270;
extern unsigned char D_8009D2E7;
extern short D_8007E768;
extern unsigned char D_8009D5E9;
extern short D_8009AC32;
extern unsigned char D_800716D0;
extern short D_800707BC;

/* A screen fade, shared with the VSync callback, hence volatile: the game reloads them and reads shorts through lhu.
 * func_80013624 moves the colour from D_8009AC4C-50 to D_8009AC52-56 over D_8009AC44 steps, D_8009AC42 counting. */
extern volatile short D_8009AC40;
extern volatile short D_8009AC42; /* a fade level, 0-255 */
extern volatile short D_8009AC44; /* and its step */
extern volatile short D_8009AC46, D_8009AC48, D_8009AC4A; /* the colour now */
extern volatile short D_8009AC4C, D_8009AC4E, D_8009AC50; /* from */
extern volatile short D_8009AC52, D_8009AC54, D_8009AC56; /* to */
extern volatile short D_80114488;
extern unsigned char D_8009AC2C;
extern volatile short D_80095DD4; /* what the VSync callback runs */

void vsync_callback(void);
void func_800140A4(void);
void func_800146A4(void);
short func_800135C0(short from, short to, short t, short n);
void func_800131B8(short r, short g, short b);
void func_80012DB0(short r, short g, short b);
void func_80012A8C(int n);
short func_800A460C(int *a, int *b, void *c, void *d);
void func_800D8D78(void);
void func_80022B5C(void);
void func_80013800(void);
void func_80013C9C(void);
void func_800CF60C(void);
void func_800A16CC(void);
void StopCallback(void);
void ResetCallback(void);
void VSyncCallback(void (*callback)(void));
void SetDispMask(int mask);
void InitGeom(void);
void func_80043938(int mode);
void func_80036298(void);
void func_80043BA8(int mode);
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* probably SetDefDrawEnv */
void func_800444AC(DISPENV *env);                             /* probably PutDispEnv */
void PutDrawEnv(DRAWENV *env);
void func_8004433C(unsigned int *ot);                         /* probably DrawOTag */
unsigned short GetTPage(int tp, int abr, int x, int y);
void func_80044A68(void *p, int dfe, int dtd, int tpage, RECT *tw); /* probably SetDrawMode */
void MoveImage(RECT *rect, int x, int y);
void func_80044244(unsigned int *ot, int n);
void SetTile(TILE *p);
void func_80046848(void *p, int on); /* probably SetSemiTrans */
void func_80046870(void *p, int on); /* probably SetShadeTex */

void play_effect_alone(unsigned short id)
{
    snd_request.id = 0xF1;
    snd_submit_request();
    snd_request.id = 0x20;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = id;
    snd_submit_request();
}

void func_800111E4(void)
{
    snd_request.id = 0xF4;
    snd_submit_request();
    if (!(D_8009D5E9 & 0x30))
        play_effect_alone(0x2B);
    D_800707BC = D_8009ABF6;
    D_800707BE = D_8009AC32;
    D_800707BE = D_800716D0 | D_8009AC32;
    func_800146A4();
    D_800716D0 = 0;
}

void snd_load_bank2(void)
{
    func_80033E34(instr2_dat_file.lba, instr2_dat_file.size, (void *)0x800E0000, 0);
    while (func_80034B44())
        ;
    func_80033E34(instr2_all_file.lba, instr2_all_file.size, (void *)0x800A0000, 0);
    while (func_80034B44())
        ;
    load_instrument_bank2((unsigned int *)0x800A0000, (unsigned int *)0x800E0000);
}

void func_800112E8(void)
{
    if (D_800965EC == 2) {
        while (func_80034B44())
            ;
        func_80015CA0((void *)0x801C0000, (void *)0x800A0000);
    } else {
        while (func_80034B44())
            ;
        func_80033E34(D_80048D2C.lba, D_80048D2C.size, (void *)0x80180000, 0);
        while (func_80034B44())
            ;
        func_80015CA0((void *)0x80180000, (void *)0x800A0000);
    }
    if (D_80071E28 != 2) {
        if (D_800965EC == 2)
            D_80071E28 = 1;
        else
            D_80071E28 = 0;
        D_800730CC = D_8009A05C;
    }
    if (D_8009A05C != D_80071A5C || !D_8009A05C) {
        frame = func_800A460C(&D_80071E28, &D_800730CC, D_80095DDC, 0);
    } else {
        frame = func_800A460C(&D_80071E28, &D_800730CC, D_80095DDC, (void *)0x801B0000);
        D_80071A5C = 0;
        D_800965E8 = 0;
    }
    D_8009A05C = D_8009ABF6;
}

void flip_frame(void)
{
    if (D_80071A58) {
        frame++;
        frame = frame & 1;
        func_80013800();
        func_800444AC(&disp_env[frame]);
        PutDrawEnv(&draw_env[frame]);
        func_8004433C(&D_8007E7A0[frame]);
    }
}

void vsync_callback(void)
{
    switch (D_80095DD4) {
    case 0:
        break;
    case 1:
        flip_frame();
        break;
    case 2:
        func_800D8D78();
        break;
    case 3:
        func_800140A4();
        break;
    case 4:
        func_80022B5C();
        break;
    }
    if (!D_80062D98 && !D_80062D99) {
        /* The casts put the counter's address in a register, as the game does. */
        *(volatile unsigned int *)&D_8009D26C += 0x444;
        if (*(volatile unsigned int *)&D_8009D26C >> 16) {
            D_8009D264++;
            *(volatile unsigned int *)&D_8009D26C &= 0xFFFF;
        }
        D_8009D270 += 0x444;
        if (D_8009D270 >> 16) {
            if (!(D_8009D2E7 & 2)) {
                if (D_8009D268)
                    D_8009D268--;
            } else {
                D_8009D268++;
            }
            *(volatile unsigned int *)&D_8009D270 &= 0xFFFF;
        }
    }
    D_8007E768 = 1;
}

void func_8001171C(void)
{
    StopCallback();
    ResetCallback();
    func_80043938(0);
    func_80036298();
    D_80095DD4 = 0;
    VSyncCallback(vsync_callback);
    func_80043BA8(0);
    SetDispMask(0);
    InitGeom();
}

void init_display(void)
{
    SetDefDispEnv(&disp_env[0], 0, 232, 320, 240);
    SetDefDispEnv(&disp_env[1], 0, 0, 320, 240);
    func_80043814(&draw_env[0], 0, 8, 320, 224);
    func_80043814(&draw_env[1], 0, 240, 320, 224);
    draw_env[0].dtd = 1;
    draw_env[1].dtd = 1;
    draw_env[0].isbg = 0;
    draw_env[1].isbg = 0;
    func_800444AC(&disp_env[0]);
    PutDrawEnv(&draw_env[0]);
}

void func_80011860(void)
{
    if (D_800965EC != 5 && D_800965EC != 13) {
        if (D_800965EC != 2) {
            func_80033E34(D_80048D24.lba, D_80048D24.size, (void *)0x80180000, 0);
            while (func_80034B44())
                ;
            func_80015CA0((void *)0x80180000, (void *)0x800A0000);
        } else {
            while (func_80034B44())
                ;
            func_80015CA0((void *)0x801C0000, (void *)0x800A0000);
        }
    }
    func_800CF60C();
    func_800A16CC();
}

void func_80011920(void)
{
    D_800965E8 = 0;
    D_80071A5C = 0;
}

void load_sound_files(void)
{
    func_80033E34(instr_all_file.lba, instr_all_file.size, (void *)0x800F0000, 0);
    while (func_80034B44())
        ;
    func_80033E34(effect_all_file.lba, effect_all_file.size, (void *)0x801B0000, 0);
    while (func_80034B44())
        ;
    func_80033E34(instr_dat_file.lba, instr_dat_file.size, (void *)0x801BC800, 0);
    while (func_80034B44())
        ;
    snd_init((unsigned int *)0x800F0000, (unsigned int *)0x801BC800);
    snd_load_effects((unsigned int *)0x801B0000);
}

void func_800119E4(void)
{
    int i;

    /* Two stores go through a pointer: the game puts their addresses in a register. */
    if (game_mode == 3) {
        *(volatile int *)&D_8009D274.unk_00 = 2;
        D_8009D274.mode = 3;
    } else {
        *(volatile int *)&D_8009D274.unk_00 = 0;
        D_8009D274.mode = 1;
    }
    *(volatile short *)&D_8009D274.unk_06 = D_8009A05C;
    i = D_800965E0;
    D_8009D274.unk_0A = D_80074EB0[i].x >> 12;
    D_8009D274.unk_0C = D_80074EB0[i].y >> 12;
    D_8009D274.unk_0E = D_80074EB0[i].unk_66;
    D_8009D274.unk_10 = D_80074EB0[i].unk_2A;
    D_8009D274.unk_11 = D_8009C540;
    D_8009D274.unk_12 = D_8009AD2C;
}

void func_80011AEC(void)
{
    D_80071E28 = D_8009D274.unk_00;
    game_mode = D_8009D274.mode;
    if (!game_mode)
        game_mode = 1;
    D_8009A05C = D_8009D274.unk_06;
    D_8009AC58 = D_8009D274.unk_06;
    D_8009ABF8 = D_8009D274.unk_0A;
    D_8009ABFA = D_8009D274.unk_0C;
    D_8009AC16 = D_8009D274.unk_0E;
    D_8009AC18 = D_8009D274.unk_10;
    D_8009C540 = D_8009D274.unk_11;
    D_8009AD2C = D_8009D274.unk_12;
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_80011BB4);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80011C1C);

void func_80012840(int abr)
{
    unsigned short tpage = GetTPage(0, abr, 0, 0);

    func_80044A68(&D_8009A068[0].mode, 0, 0, tpage, 0);
    func_80044A68(&D_8009A068[1].mode, 0, 0, tpage, 0);
}

void func_800128B8(void)
{
    int i;
    int j;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            SetTile(&D_8009A068[i].tile[j]);
            func_80046848(&D_8009A068[i].tile[j], 1);
            func_80046870(&D_8009A068[i].tile[j], 1);
            set_rgb(&D_8009A068[i].tile[j], 0, 0, 0);
            set_wh(&D_8009A068[i].tile[j], 0xA0, 0xE0);
        }
    }
    set_xy(&D_8009A068[0].tile[0], 0, 0);
    set_xy(&D_8009A068[0].tile[1], 0xA0, 0);
    set_xy(&D_8009A068[1].tile[0], 0, 0);
    set_xy(&D_8009A068[1].tile[1], 0xA0, 0);
}

void copy_frame(void)
{
    MoveImage(&disp_env[frame].disp, disp_env[(frame + 1) & 1].disp.x, disp_env[(frame + 1) & 1].disp.y);
    while (DrawSync(1))
        ;
}

void func_80012A8C(int level)
{
    func_80044244(&D_8007E7A0[frame], 1);
    set_rgb(&D_8009A068[frame].tile[0], level, level, level);
    set_rgb(&D_8009A068[frame].tile[1], level, level, level);
    add_prim(&D_8007E7A0[frame], &D_8009A068[frame].tile[0]);
    add_prim(&D_8007E7A0[frame], &D_8009A068[frame].tile[1]);
    add_prim(&D_8007E7A0[frame], &D_8009A068[frame].mode);
}

/* Draws the fade tiles in colour (r, g, b) scaled by the fade level. */
void func_80012DB0(short r, short g, short b)
{
    /* The level is read through a pointer: the game keeps its address in a register. */
    func_80044244(&D_8007E7A0[frame], 1);
    set_rgb(&D_8009A068[frame].tile[0], *(volatile short *)&D_8009AC42 * r / 256,
            *(volatile short *)&D_8009AC42 * g / 256, *(volatile short *)&D_8009AC42 * b / 256);
    set_rgb(&D_8009A068[frame].tile[1], *(volatile short *)&D_8009AC42 * r / 256,
            *(volatile short *)&D_8009AC42 * g / 256, *(volatile short *)&D_8009AC42 * b / 256);
    add_prim(&D_8007E7A0[frame], &D_8009A068[frame].tile[0]);
    add_prim(&D_8007E7A0[frame], &D_8009A068[frame].tile[1]);
    add_prim(&D_8007E7A0[frame], &D_8009A068[frame].mode);
}

void func_800131B8(short r, short g, short b)
{
    func_80044244(&D_8007E7A0[frame], 1);
    set_rgb(&D_8009A068[frame].tile[0], r, g, b);
    set_rgb(&D_8009A068[frame].tile[1], r, g, b);
    add_prim(&D_8007E7A0[frame], &D_8009A068[frame].tile[0]);
    add_prim(&D_8007E7A0[frame], &D_8009A068[frame].tile[1]);
    add_prim(&D_8007E7A0[frame], &D_8009A068[frame].mode);
}

void func_800134F4(void)
{
    D_8009AC42 -= D_8009AC44;
    D_8009AC2C = 0;
    if (D_8009AC42 <= 0 || D_80114488 == 1) {
        D_8009AC40 = 0;
        D_8009AC42 = 0;
    }
}

void func_80013564(void)
{
    /* Through a pointer here, unlike func_800134F4: the game puts the address in a register. */
    *(volatile unsigned char *)&D_8009AC2C = 0;
    D_8009AC42 += D_8009AC44;
    if (D_8009AC42 >= 0x100)
        D_8009AC42 = 0xFF;
}

/* Moves from `from` towards `to`: step t of n. */
short func_800135C0(short from, short to, short t, short n)
{
    return from + (short)(to - from) * t / n;
}

void func_80013624(void)
{
    *(volatile unsigned char *)&D_8009AC2C = 0;
    D_8009AC42++;
    if (D_8009AC42 >= D_8009AC44) {
        D_8009AC4C = D_8009AC52;
        D_8009AC4E = D_8009AC54;
        D_8009AC50 = D_8009AC56;
        D_8009AC42 = D_8009AC44;
    }
    D_8009AC46 = func_800135C0(D_8009AC4C, D_8009AC52, D_8009AC42, D_8009AC44);
    D_8009AC48 = func_800135C0(D_8009AC4E, D_8009AC54, D_8009AC42, D_8009AC44);
    D_8009AC4A = func_800135C0(D_8009AC50, D_8009AC56, D_8009AC42, D_8009AC44);
    func_800131B8(D_8009AC46, D_8009AC48, D_8009AC4A);
}

void func_80013800(void)
{
    /* Through a pointer here, unlike func_80013624: the game puts the address in a register. */
    if (D_80071A58 == 3) {
        (*(volatile short *)&D_8009AC42)++;
        if (*(volatile short *)&D_8009AC42 == 0x22) {
            D_8009AC40 = 0;
            D_80095DD4 = 0;
            D_80071A58 = 0;
        }
        func_80012840(2);
        func_80012A8C(0x10);
    } else {
        (*(volatile short *)&D_8009AC42)++;
        if (*(volatile short *)&D_8009AC42 == 0x12) {
            D_8009AC40 = 0;
            D_80095DD4 = 0;
            D_80071A58 = 0;
        }
        func_80012840(2);
        func_80012A8C(0x20);
    }
}

void func_800138EC(void)
{
    /* The first fade variable each case reads goes through a pointer: the game puts its address in a register. */
    switch (*(volatile short *)&D_8009AC40) {
    case 0:
        *(volatile unsigned char *)&D_8009AC2C = 0;
        D_8009AC4C = 0;
        D_8009AC4E = 0;
        D_8009AC50 = 0;
        break;
    case 1:
        func_80012840(2);
        func_800134F4();
        func_80012DB0(0x100 - *(volatile short *)&D_8009AC46, 0x100 - D_8009AC48, 0x100 - D_8009AC4A);
        break;
    case 2:
        func_80012840(2);
        func_80013564();
        func_80012DB0(0x100 - *(volatile short *)&D_8009AC46, 0x100 - D_8009AC48, 0x100 - D_8009AC4A);
        break;
    case 7:
        func_80012840(2);
        func_800134F4();
        func_800131B8(0x100 - *(volatile short *)&D_8009AC46, 0x100 - D_8009AC48, 0x100 - D_8009AC4A);
        break;
    case 8:
        func_80012840(2);
        func_80013564();
        func_800131B8(0x100 - *(volatile short *)&D_8009AC46, 0x100 - D_8009AC48, 0x100 - D_8009AC4A);
        break;
    case 3:
        (*(volatile short *)&D_8009AC42)++;
        if (*(volatile short *)&D_8009AC42 == 0x22) {
            D_8009AC40 = 0;
            D_80095DD4 = 0;
            break;
        }
        func_80012840(2);
        func_80012A8C(0x10);
        break;
    case 13:
        (*(volatile short *)&D_8009AC42)++;
        if (*(volatile short *)&D_8009AC42 == 0x12) {
            D_8009AC40 = 0;
            D_80095DD4 = 0;
            break;
        }
        func_80012840(2);
        func_80012A8C(0x20);
        break;
    case 4:
        *(volatile unsigned char *)&D_8009AC2C = 1;
        break;
    case 5:
        func_80012840(1);
        func_800134F4();
        func_80012DB0(*(volatile short *)&D_8009AC46, D_8009AC48, D_8009AC4A);
        break;
    case 6:
        *(volatile unsigned char *)&D_8009AC2C = 0;
        func_80012840(1);
        func_80013564();
        func_80012DB0(D_8009AC46, D_8009AC48, D_8009AC4A);
        break;
    case 9:
        func_80012840(1);
        func_800134F4();
        func_800131B8(*(volatile short *)&D_8009AC46, D_8009AC48, D_8009AC4A);
        break;
    case 10:
        *(volatile unsigned char *)&D_8009AC2C = 0;
        func_80012840(1);
        func_80013564();
        func_800131B8(D_8009AC46, D_8009AC48, D_8009AC4A);
        break;
    case 11:
        func_80012840(1);
        func_80013624();
        break;
    case 12:
        func_80012840(2);
        func_80013624();
        break;
    }
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_80013C9C);

void func_800140A4(void)
{
    if (!(++D_8019DAA0 & 1)) {
        func_8004433C(D_8019D5E8);
        func_80013C9C();
    }
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_800140F4);
