#include "common.h"
#include "sound.h"
#include "main.h"

/* The game's boot file (0x8001117C-0x800144D8): it boots the sound driver, sets up the display and runs the main loop.
 * Assembled by ASPSX 2.21 (indexed loads go through `addiu $at`); GCC 2.7.2 and 2.6.3 both match so far. */

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
extern unsigned short D_80071A5C;
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
extern unsigned char D_8009A068[];
extern int D_8019DAA0;
extern unsigned int *D_8019D5E8;

/* Shared with the VSync callback, hence volatile: the game reloads them and reads shorts through lhu. */
extern volatile short D_8009AC40;
extern volatile short D_8009AC42; /* a fade level, 0-255 */
extern volatile short D_8009AC44; /* and its step */
extern volatile short D_80114488;
extern unsigned char D_8009AC2C;
extern short D_80095DD4;

void func_8001155C(void);
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

void play_effect_alone(unsigned short id)
{
    snd_request.id = 0xF1;
    snd_submit_request();
    snd_request.id = 0x20;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = id;
    snd_submit_request();
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_800111E4);

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

INCLUDE_ASM("asm/nonmatchings/main/main", func_800112E8);

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

INCLUDE_ASM("asm/nonmatchings/main/main", func_8001155C);

void func_8001171C(void)
{
    StopCallback();
    ResetCallback();
    func_80043938(0);
    func_80036298();
    D_80095DD4 = 0;
    VSyncCallback(func_8001155C);
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

    func_80044A68(D_8009A068, 0, 0, tpage, 0);
    func_80044A68(D_8009A068 + 0x30, 0, 0, tpage, 0);
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_800128B8);

void copy_frame(void)
{
    MoveImage(&disp_env[frame].disp, disp_env[(frame + 1) & 1].disp.x, disp_env[(frame + 1) & 1].disp.y);
    while (DrawSync(1))
        ;
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_80012A8C);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80012DB0);

INCLUDE_ASM("asm/nonmatchings/main/main", func_800131B8);

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

INCLUDE_ASM("asm/nonmatchings/main/main", func_800135C0);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80013624);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80013800);

INCLUDE_ASM("asm/nonmatchings/main/main", func_800138EC);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80013C9C);

void func_800140A4(void)
{
    if (!(++D_8019DAA0 & 1)) {
        func_8004433C(D_8019D5E8);
        func_80013C9C();
    }
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_800140F4);
