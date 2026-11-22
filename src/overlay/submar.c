/* MINI/SUBMAR.BIN, the submarine, 0x800A00BC-0x800AD330 (its data follows to 0x800B2E38). GCC 2.6.3, ASPSX 2.21 with
 * its divide checks, -G0 -fforce-addr. Partly recovered: the functions still INCLUDE_ASM without C are mostly built
 * from GTE register macros, not tried yet. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "gte.h"

int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));

/* libgte's inline macros as SUBMAR.BIN has them (inline_c.h's: no copy of the pointer first, unlike JET.BIN's). */
#ifdef NATIVE
#define gte_SetLightMatrix(r)                                                                                          \
    (gte_write_ctrl(8, ((unsigned int *)(r))[0]), gte_write_ctrl(9, ((unsigned int *)(r))[1]),                         \
     gte_write_ctrl(10, ((unsigned int *)(r))[2]), gte_write_ctrl(11, ((unsigned int *)(r))[3]),                       \
     gte_write_ctrl(12, ((unsigned int *)(r))[4]))
#define gte_SetColorMatrix(r)                                                                                          \
    (gte_write_ctrl(16, ((unsigned int *)(r))[0]), gte_write_ctrl(17, ((unsigned int *)(r))[1]),                       \
     gte_write_ctrl(18, ((unsigned int *)(r))[2]), gte_write_ctrl(19, ((unsigned int *)(r))[3]),                       \
     gte_write_ctrl(20, ((unsigned int *)(r))[4]))
#define gte_SetBackColor(r, g, b) (gte_write_ctrl(13, (r)), gte_write_ctrl(14, (g)), gte_write_ctrl(15, (b)))
#define gte_SetFarColor(r, g, b) (gte_write_ctrl(21, (r)), gte_write_ctrl(22, (g)), gte_write_ctrl(23, (b)))
#else
#undef gte_SetRotMatrix
#undef gte_SetTransMatrix
#define gte_SetRotMatrix(r)                                                                                            \
    __asm__ volatile("lw $12, 0(%0)\n"                                                                                 \
                     "lw $13, 4(%0)\n"                                                                                 \
                     "ctc2 $12, $0\n"                                                                                  \
                     "ctc2 $13, $1\n"                                                                                  \
                     "lw $12, 8(%0)\n"                                                                                 \
                     "lw $13, 12(%0)\n"                                                                                \
                     "lw $14, 16(%0)\n"                                                                                \
                     "ctc2 $12, $2\n"                                                                                  \
                     "ctc2 $13, $3\n"                                                                                  \
                     "ctc2 $14, $4"                                                                                    \
                     :                                                                                                 \
                     : "r"(r)                                                                                          \
                     : "$12", "$13", "$14")
#define gte_SetTransMatrix(r)                                                                                          \
    __asm__ volatile("lw $12, 20(%0)\n"                                                                                \
                     "lw $13, 24(%0)\n"                                                                                \
                     "ctc2 $12, $5\n"                                                                                  \
                     "lw $14, 28(%0)\n"                                                                                \
                     "ctc2 $13, $6\n"                                                                                  \
                     "ctc2 $14, $7"                                                                                    \
                     :                                                                                                 \
                     : "r"(r)                                                                                          \
                     : "$12", "$13", "$14")
#define gte_SetLightMatrix(r)                                                                                          \
    __asm__ volatile("lw $12, 0(%0)\n"                                                                                 \
                     "lw $13, 4(%0)\n"                                                                                 \
                     "ctc2 $12, $8\n"                                                                                  \
                     "ctc2 $13, $9\n"                                                                                  \
                     "lw $12, 8(%0)\n"                                                                                 \
                     "lw $13, 12(%0)\n"                                                                                \
                     "lw $14, 16(%0)\n"                                                                                \
                     "ctc2 $12, $10\n"                                                                                 \
                     "ctc2 $13, $11\n"                                                                                 \
                     "ctc2 $14, $12"                                                                                   \
                     :                                                                                                 \
                     : "r"(r)                                                                                          \
                     : "$12", "$13", "$14")
#define gte_SetColorMatrix(r)                                                                                          \
    __asm__ volatile("lw $12, 0(%0)\n"                                                                                 \
                     "lw $13, 4(%0)\n"                                                                                 \
                     "ctc2 $12, $16\n"                                                                                 \
                     "ctc2 $13, $17\n"                                                                                 \
                     "lw $12, 8(%0)\n"                                                                                 \
                     "lw $13, 12(%0)\n"                                                                                \
                     "lw $14, 16(%0)\n"                                                                                \
                     "ctc2 $12, $18\n"                                                                                 \
                     "ctc2 $13, $19\n"                                                                                 \
                     "ctc2 $14, $20"                                                                                   \
                     :                                                                                                 \
                     : "r"(r)                                                                                          \
                     : "$12", "$13", "$14")
#define gte_SetBackColor(r, g, b) __asm__ volatile("ctc2 %0, $13\nctc2 %1, $14\nctc2 %2, $15" : : "r"(r), "r"(g), "r"(b))
#define gte_SetFarColor(r, g, b) __asm__ volatile("ctc2 %0, $21\nctc2 %1, $22\nctc2 %2, $23" : : "r"(r), "r"(g), "r"(b))
#endif
extern short D_800AE914, D_800AE916, D_800AE918;
extern int D_800AE944, D_800AD40C;
extern short D_801BA30C, D_801BA30E, D_801BA310, D_801BA314, D_801BA316, D_801BA318, D_801BA31C, D_801BA31E;
extern short D_801BA320;
extern int D_801BA324, D_801BA328, D_801BA32C, D_801BA330, D_801BA334, D_801BA338, D_801BA33C, D_801BA340;
extern int D_801BA344;
extern short D_800AD3B8, D_800AD3BA, D_800AD3BC;
extern unsigned int D_801B7310[2][0x600];
extern unsigned int D_800AD330, D_800AD334, D_800AD338, D_800AD33C;
extern unsigned char D_800B365C[], D_800B565C[];
typedef struct SubTex {
    int mode;
    RECT image;
    RECT clut_rect;
    unsigned short tpage;
    unsigned short clut;
} SubTex;
/* A TIM block (CLUT or pixels): its length, where it goes in VRAM, then its data. */
typedef struct TimBlock {
    unsigned int len;
    RECT rect;
} TimBlock;
int func_80043CC0(void);                /* GetGraphType */
void func_80044000(RECT *r, void *p); /* LoadImage */
/* libgpu's getTPage and getClut. */
#define getTPage(tp, abr, x, y)                                                                                        \
    (func_80043CC0() == 1 || func_80043CC0() == 2                                                                      \
         ? (((tp) & 3) << 9) | (((abr) & 3) << 7) | (((y) & 0x300) >> 3) | (((x) & 0x3FF) >> 6)                       \
         : (((tp) & 3) << 7) | (((abr) & 3) << 5) | (((y) & 0x100) >> 4) | (((x) & 0x3FF) >> 6) | (((y) & 0x200) << 2))
#define getClut(x, y) ((y) << 6 | ((x) >> 4 & 0x3F))
extern unsigned int D_800AD340, D_800AD344, D_800AD348, D_800AD34C;
extern SubTex D_800AD904, D_800AD91C;
extern int D_800AD8E0, D_800AD8E4;
extern unsigned char D_800AE484, D_800AE498;
extern unsigned int D_800AE3C8, D_800AE3F0;
extern unsigned int D_801BCF08[];
typedef struct OtChain {
    unsigned int *first, *last;
} OtChain;
extern OtChain D_800AE5A4[];
typedef struct PathNode {
    int x, y, z;
    int pad[3];
    unsigned int next;
    int pad2;
} PathNode;
typedef struct Trail {
    int x, y, z;
    char pad[0x80];
    SVECTOR *trail[5];
} Trail;
void submar_load_tim(TimBlock *tim, SubTex *tex);

typedef struct SubFile {
    unsigned int lba, size;
} SubFile;
extern SubFile D_800AD350[];
void submar_init_paths(PathNode *n);
extern DRAWENV D_800B5E5C[2];
extern DISPENV D_800B5F14[2];
extern int D_800AD420, D_800AD424, D_800AD428;
extern int D_800B5F3C, D_800B5F40, D_800B5F44;
extern MATRIX D_800AD3EC;
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* SetDefDrawEnv */
int rand(void);
void srand(unsigned int seed);
unsigned int func_8001C808(); /* the pad's buttons */
extern unsigned short D_800AD440[5], D_800AD44C[5];
extern unsigned short D_800AD458, D_800AD45A, D_800AD45C, D_800AD45E, D_800AD460, D_800AD462, D_800AD464, D_800AD466;

#ifdef NATIVE
#define gte_ldlvl(v)                                                                                                   \
    (gte_write(9, ((unsigned int *)(v))[0]), gte_write(10, ((unsigned int *)(v))[1]),                                  \
     gte_write(11, ((unsigned int *)(v))[2]))
#else
#undef gte_rtir
#undef gte_stlvnl
#define gte_ldlvl(v) __asm__ volatile("lwc2 $9, 0(%0)\nlwc2 $10, 4(%0)\nlwc2 $11, 8(%0)" : : "r"(v))
#define gte_rtir() __asm__ volatile("nop\nnop\n.word 0x4A49E012")
#define gte_stlvnl(v) __asm__ volatile("swc2 $25, 0(%0)\nswc2 $26, 4(%0)\nswc2 $27, 8(%0)" : : "r"(v) : "memory")
#endif
/* The scratchpad's first words, a work area. */
#define SCRATCH_ANGLE ((SVECTOR *)0x1F800000)
#define SCRATCH_VX ((int *)0x1F800008)
#define SCRATCH_VY ((int *)0x1F80000C)
#define SCRATCH_VZ ((int *)0x1F800010)
extern short D_800AE90C, D_800AE90E, D_800AE910, D_800AE91C, D_800AE91E, D_800AE920;
extern int D_800AE968;
extern int D_800AE8D4, D_800AE8D8;
extern MATRIX D_800F5F4C;
void sub_800A1DA0(SVECTOR *angle, MATRIX *m);

/* A cell of the 64x64 sea floor: its height, its neighbours' (to draw it without looking them up), flags and the
 * height it eases towards. */
typedef struct Cell {
    unsigned short h;
    unsigned short h_down;  /* the next row's */
    unsigned short h_left;  /* the previous column's */
    unsigned short h_right; /* the next column's */
    unsigned short h_up;    /* the previous row's */
    unsigned short flags;
    unsigned short target;
    unsigned short base;
} Cell;
extern Cell D_800B5F4C[0x1000];
extern int D_800AE960;

/* A submarine or other thing in the water (12 of them, the player's first). */
typedef struct Sub {
    int x, y, z;
    int x0C;
    int value;
    int x14, x18;
    short rx, ry, rz;
    short x22;
    short ax, ay, az;
    short x2A;
    int x2C, x30;
    unsigned int flags;
    unsigned int state;
    int x3C;
    int dist;
    int angle;
    int side;
    char x4C[8];
    void *kind;
    PathNode *node;
    char x5C[0x14];
    int cell;
} Sub;
/* A sub's record in the mission file. */
typedef struct SubRecord {
    int x, y, z;
    int x0C;
    int value;
    short rx, ry, rz;
    short x1A;
    int x1C, x20;
    unsigned int flags;
    int kind;
    int node;
} SubRecord;
extern Sub D_800B08EC[12];
extern unsigned char D_800AEAA0[][16];
extern int D_800AD8DC;
extern volatile int D_800AD8E8;
void submar_init_subs(SubRecord *r);

extern OtChain D_800AE644[];

extern short D_8004BC98[]; /* libgte's rcossin_tbl: each angle's sine, then cosine */
/* A draw mode and a sprite, linked into one primitive. */
typedef struct ModeSprite {
    DR_MODE mode;
    SPRT sprt;
} ModeSprite;
extern ModeSprite D_800AD97C[4];
extern unsigned int D_800AE96C;
extern int D_800AD93C;
extern int D_800AE8E0, D_800AD3B4, D_800AE9FC;
extern int D_800AD934, D_800AD3A8, D_800AE97C;
extern SPRT D_800AE540[5];
/* A shake: two angles turning, scaled by an amplitude that decays. */
typedef struct Shake {
    int start;
    unsigned short angle0_start, angle1_start;
    int amp_start;
    int decay;
    unsigned short step0;
    short angle0;
    unsigned short step1;
    short angle1;
    int amp;
    short out0, out1;
} Shake;
extern Shake D_800AD95C;

extern SPRT D_800ADAE0[28];
extern unsigned short D_800ADD10[6];
extern POLY_GT4 D_800ADD44, D_800ADDB4;
extern SPRT D_800ADE10, D_800ADE38, D_800ADE60;
extern SPRT D_800ADEC4[4];
extern SPRT D_800AE2AC[4];
extern unsigned int D_800AE984[4];
extern short D_800AE8E4, D_800AE8E6;
extern int D_800AD944, D_800AD948, D_800AD94C, D_800AD950, D_800AD954, D_800AD958;

/* The player's sub. */
typedef struct Player {
    int x, y, z;
    int x0C;
    int x10;
    short pitch, heading;
    short x18;
    short x1A;
    SVECTOR fwd;
    SVECTOR side;
    char x2C[8];
    short roll[3];
    short x3A;
    short tilt[3];
    short x42;
    short cam[3];
    short x4A;
    short pitch2, heading2;
    short x50;
    char x52[0xC];
    short heading3;
    char x60[0xC];
    int view;
    int view_toggle;
    int dist;
    char x78[0x18];
    int cell;
    int x94;
    int flip;
    unsigned int alerts;
    int x_a0;
    int x_a4;
    int gear;
    int speed;
    int x_b0;
    unsigned int tubes[4];
} Player;
extern Player D_800AE8D0;
/* A torpedo (32 of them). */
typedef struct Torpedo {
    VECTOR pos;
    VECTOR vel;
    SVECTOR rot;
    char x28[0x10];
    SVECTOR x38;
    SVECTOR x40;
    SVECTOR x48;
    char x50[8];
    int dist;
    int side;
    int angle;
    int x64, x68, x6C, x70, x74, x78;
    unsigned int flags;
    int x80;
    Sub *target;
    int x88;
    SVECTOR *trail[5];
    SVECTOR trail_buf[5];
} Torpedo;
extern Torpedo D_800F5F6C[32];

extern SVECTOR D_801BCF78[];

extern SVECTOR D_800AEA90, D_800AEA98;
extern unsigned char D_8013660C[2][0x10680];
void *sub_800ABD4C(void *p, void *kind);

/* A camera view: its angles, distances and the fields it sets. */
typedef struct View {
    short rx, ry, rz;
    short x06;
    int dist;
    int height;
    int x10;
    int flip;
    int x18, x1C, x20, x24, x28;
    int x2C;
} View;
extern View D_800AE7E0[];
extern int D_800AE964;
extern int D_800AEA00, D_800AEA04, D_800AEA10, D_800AEA14, D_800AEA18;

extern int D_800AE684[];
extern short D_800AE6F0[], D_800AE734[], D_800AE778[], D_800AE7BC[];
void submar_fire(void);
void submar_move_camera(void);
void submar_change_view(int n);

extern int D_800AEA08;

/* The player's record in the mission file. */
typedef struct PlayerRecord {
    int x, y, z;
    int x0C;
    int x10;
    short pitch, heading;
    short x18;
    short x1A;
    int speed_scale;
    int view;
    int kind;
} PlayerRecord;

/* The mini-game's mode (0-5) and what it returns. */
typedef struct Game {
    unsigned char mode;
    unsigned char x01;
    unsigned short result;
} Game;
extern Game D_800AD380;
extern int D_800AD384; /* how it ended: 1 won, 2 sunk, 3 out of time, 5 quit */
extern int D_800AD388; /* 1 paused, 2 ending, 4 the quit prompt, 8 quit chosen */
/* The clock, in vblanks. */
typedef struct Clock {
    int start;
    int prev;
    int now;
    int delta;
    int secs;
    int mins;
    int elapsed;
    int left;
    int limit;
    int countdown;
} Clock;
extern Clock D_800AD38C;
extern unsigned short D_800AD3C0[2], D_800AD3C4[2];
typedef struct Shift {
    short x, y;
} Shift;
extern Shift D_800AD3C8;
extern unsigned int D_801B730C[2][0x600];
extern unsigned int D_801C5700[2][0x90];
extern unsigned char D_801BA348[2][0x15E0];
extern int D_800AD8F0, D_800AEA0C, D_800AEB30;
extern SPRT D_800ADF40;
void func_800444AC(DISPENV *env);            /* PutDispEnv */
void PutDrawEnv(DRAWENV *env);
void func_80044244(unsigned int *ot, int n); /* ClearOTagR */
void func_8004433C(unsigned int *ot);        /* DrawOTag */
void func_8004418C(unsigned int *ot, int n); /* ClearOTag */
int DrawSync(int mode);
int VSync(int mode);
void SetDispMask(int mask);
void submar_camera_to_result(void);
void submar_ot_span(void);
void submar_load_sound(unsigned char mode);
void submar_load_tims(void);
void submar_load_mission(unsigned char n);
void submar_init_gfx(void);
void submar_read_pad(void);
void submar_set_view(void);
void submar_init_floor(void);
void submar_raise_cells(short n);
void submar_update_floor(void);
void submar_lock_target(void);
void submar_draw_subs(void);
void submar_set_score(void);
void submar_set_count(void);
void submar_set_quit_choice(int on);
void submar_show_layer(unsigned int i, int mode);
void submar_show_message(int n);
void submar_show_result(int n);
void submar_show_alerts(void);
void submar_update_hud(void);
void submar_update_shake(void);
void submar_sonar_torpedoes(void);
void submar_sonar_subs(void);
void submar_control(void);
void submar_init_player(PlayerRecord *r);
void submar_init_torpedoes(void);
void submar_list_cells(int max);
void sub_800A4D90(void);
void sub_800A832C(void);
void sub_800A8944(void);
void sub_800A32C8(void);
void sub_800A36F4(void);
void sub_800A9130(void);
void sub_800A1F9C(void);
void sub_800AB518(void);
void sub_800A4624(void);
void sub_800A4914(void);
void sub_800AA1D0(void);
void sub_800AA714(void);
void sub_800AC2FC(void);
#ifdef NATIVE
#define gte_SetGeomScreen(h) gte_write_ctrl(26, (h))
#else
#define gte_SetGeomScreen(h) __asm__ volatile("ctc2 %0, $26" : : "r"(h))
#endif

/* The mission file, read whole: the floor's heights, the settings, the player's and the subs' records, the
 * paths. */
typedef struct Mission {
    unsigned char floor[0x1000];
    int time_limit;
    int floor_offset;
    int floor_depth;
    int drops;
    int x1010;
    int x1014, x1018, x101C, x1020, x1024, x1028;
    PlayerRecord player;
    SubRecord subs[12];
    char x1294[0x56C];
} Mission;
extern Mission D_800B0E5C;
extern PathNode D_800B265C[128]; /* read with the mission file, after it */

/* end of declarations */

#ifdef NON_MATCHING
/* Register choice throughout: the game's frame saves eight callee-saved registers and keeps 8 bytes on the\nstack where GCC here uses a ninth register. */
unsigned short submar_main(unsigned char mode)
{
    int end, e, n, quit;

    SetDispMask(0);
    if (mode >= 6) {
        D_800AD380.mode = 5;
    } else {
        D_800AD380.mode = mode;
    }
    D_800AD380.result = 0;
    D_800AD388 = 0;
    submar_load_tims();
    submar_load_sound(D_800AD380.mode);
    if (D_800AD380.mode == 0) {
        snd_request.id = 0xC2;
        snd_request.arg[0] = 0x3FC;
        snd_request.arg[1] = 0x7E;
        snd_request.arg[2] = 1;
        snd_submit_request();
        snd_request.id = 0x10;
        snd_request.arg[0] = (long)D_800B365C;
    } else {
        snd_request.id = 0x10;
        snd_request.arg[0] = (long)D_800B565C;
    }
    snd_submit_request();
    submar_init_gfx();
    submar_load_mission(D_800AD380.mode);
    submar_init_player(&D_800B0E5C.player);
    sub_800A4D90();
    D_800AD3B8 = 0;
    submar_init_floor();
    if (D_800B0E5C.drops > 500) {
        D_800B0E5C.drops = 500;
    }
    submar_raise_cells(D_800B0E5C.drops);
    submar_init_torpedoes();
    submar_list_cells(D_800B0E5C.drops);
    func_800444AC(&D_800B5F14[D_800AD3B8]);
    PutDrawEnv(&D_800B5E5C[D_800AD3B8]);
    func_80044244(D_801B730C[D_800AD3B8], 0x600);
    sub_800A832C();
    sub_800A8944();
    submar_set_view();
    sub_800A32C8();
    sub_800A36F4();
    D_801C5700[D_800AD3B8][0] = 0xFFFFFF;
    submar_update_hud();
    D_800AD8F0 = 0;
    D_800AD38C.start = D_800AD38C.prev = VSync(-1);
    D_800AD38C.limit = D_800B0E5C.time_limit;
    D_800AD38C.countdown = D_800AD380.mode ? 1200 : 900;
    while (1) {
        func_8004433C(&D_801B730C[D_800AD3B8][D_800AD3BA]);
        func_8004433C(D_801C5700[D_800AD3B8]);
        func_8004433C(&D_801BCF08[20]);
        func_8004433C(D_801BCF08);
        D_800AD3B8 = (D_800AD3B8 ^ 1) & 1;
        func_80044244(D_801B730C[D_800AD3B8], 0x600);
        submar_read_pad();
        if (D_800AD388 & 2) {
            D_800AD45A = 0;
            D_800AD45E = 0;
            D_800AD458 = 0;
            D_800AD45C = 0;
        } else {
            quit = (D_800AD388 & 1) ? 0 : !D_800AEA0C;
            if (!D_800B0E5C.x1010 && !(D_800AD458 & 0x20)) {
                quit = 0;
            }
            if (quit) {
                snd_request.id = 0x28;
                snd_request.arg[0] = 0x40;
                D_800AEA0C = 1;
                snd_request.arg[1] = 0x195;
                snd_submit_request();
                D_800AD944 = 0x10;
            }
            if (D_800AD45C & 0x800) {
                e = 0x99;
                n = 4;
                D_800AD388 ^= 1;
                if (!(D_800AD388 & 1)) {
                    e = 0x98;
                    n = 0;
                }
                submar_show_layer(15, D_800AD388 & 1);
                submar_show_message(n);
                snd_request.id = e;
                snd_submit_request();
                D_800AD388 &= ~0xC;
            }
        }
        if (D_800AD380.mode && (D_800AD388 & 1) && (D_800AD45C & 0x100)) {
            e = 0;
            D_800AD388 = (D_800AD388 ^ 4) & ~8;
            submar_set_quit_choice(0);
            n = 5;
            if (!(D_800AD388 & 4)) {
                e = 1;
                n = 4;
            }
            submar_show_layer(15, e);
            submar_show_message(n);
        }
        e = D_800AD388 & 5;
        if (e == 5 && (D_800AD45C & 0xA020)) {
            snd_request.id = 0x30;
            snd_request.arg[0] = 1;
            snd_submit_request();
            if (D_800AD45C & 0x8000) {
                D_800AD388 |= 8;
            } else if (D_800AD45C & 0x2000) {
                D_800AD388 &= ~8;
            } else if (D_800AD45C & 0x20) {
                if (D_800AD388 & 8) {
                    D_800AD384 = e;
                    break;
                }
                D_800AD388 &= ~0xC;
                submar_show_layer(15, 1);
                submar_show_message(4);
            }
            submar_set_quit_choice(D_800AD388 & 8);
        }
        if (!(D_800AD388 & 1)) {
            submar_control();
        }
        submar_set_view();
        gte_SetGeomScreen(D_800AD40C);
        sub_800A9130();
        D_800AEA98.vy = D_800AE8D0.heading;
        D_800AEA98.vz = D_800AE8D0.x18;
        D_800AEA90.vx = D_800AE8D0.x >> 12;
        D_800AEA90.vy = D_800AE8D0.y >> 12;
        D_800AEA90.vz = D_800AE8D0.z >> 12;
        D_800AEA98.vx = D_800AE8D0.pitch;
        sub_800ABD4C(D_801BA348[D_800AD3B8], (void *)D_800AE8D0.x_b0);
        if (!(D_800AD388 & 3)) {
            sub_800A832C();
            sub_800A8944();
        }
        gte_SetRotMatrix(&D_800F5F4C);
        gte_SetTransMatrix(&D_800F5F4C);
        if (!(D_800AD388 & 1)) {
            submar_update_floor();
        }
        sub_800A1F9C();
        sub_800AB518();
        sub_800A32C8();
        gte_SetRotMatrix(&D_800F5F4C);
        gte_SetTransMatrix(&D_800F5F4C);
        if (!(D_800AD388 & 3)) {
            sub_800A36F4();
        }
        submar_lock_target();
        sub_800A4624();
        submar_draw_subs();
        D_801C5700[D_800AD3B8][0] = 0xFFFFFF;
        if (!(D_800AD388 & 2)) {
            sub_800A4914();
        }
        if (!(D_800AD388 & 3)) {
            sub_800AA1D0();
        }
        sub_800AA714();
        sub_800AC2FC();
        submar_ot_span();
        if (!(D_800AD388 & 1)) {
            submar_update_shake();
        }
        D_800AD3C8.x = D_800AD95C.out0;
        D_800AD3C8.y = D_800AD95C.out1;
        D_800B5E5C[D_800AD3B8].ofs[0] = D_800AD3C0[D_800AD3B8] + D_800AD3C8.x;
        D_800B5E5C[D_800AD3B8].ofs[1] = D_800AD3C4[D_800AD3B8] + D_800AD3C8.y;
        if (!(D_800AD388 & 1)) {
            if (!(D_800AD388 & 2)) {
                submar_sonar_torpedoes();
                submar_sonar_subs();
            }
            submar_update_hud();
        }
        func_8004418C(&D_801BCF08[20], 8);
        if (!(D_800AD388 & 2)) {
            submar_show_alerts();
        }
        if (!D_800AD380.mode) {
            if (D_800AD38C.secs == 16) {
                snd_request.id = 0xC1;
                snd_request.arg[0] = 0x1E;
                snd_request.arg[1] = 1;
                snd_submit_request();
            }
            if (D_800AD38C.secs == 17) {
                snd_request.id = 0x10;
                snd_request.arg[0] = (long)D_800B565C;
                snd_submit_request();
            }
        }
        if (!(D_800AD388 & 2) && D_800AD38C.secs >= 18) {
            snd_request.id = 0xC1;
            snd_request.arg[0] = 8;
            snd_request.arg[1] = D_800AD93C;
            snd_submit_request();
        }
        if (D_800AD38C.secs == 17) {
            D_800AD8F0 = 1;
        }
        e = D_800AD38C.elapsed;
        if ((unsigned int)e < 0xF0 && (unsigned int)(e / 16) < 14) {
            submar_show_layer(e / 16, 1);
        }
        if (DrawSync(1)) {
            DrawSync(0);
        }
        VSync(2);
        func_800444AC(&D_800B5F14[D_800AD3B8]);
        PutDrawEnv(&D_800B5E5C[D_800AD3B8]);
        SetDispMask(1);
        n = VSync(-1);
        e = n - D_800AD38C.prev;
        D_800AD38C.now = n;
        if (D_800AD388 & 1) {
            e = 0;
        }
        D_800AD38C.delta = e;
        D_800AD38C.elapsed += e;
        if (!(D_800AD388 & 2)) {
            D_800AD38C.left = D_800AD38C.limit - D_800AD38C.elapsed;
        }
        D_800AD38C.prev = n;
        D_800AD38C.secs = D_800AD38C.elapsed / 60;
        D_800AD38C.mins = D_800AD38C.elapsed / 3600;
        if (!(D_800AD388 & 2)) {
            end = 0;
            if (!D_800AD380.mode) {
                end = (D_800AE8D0.x10 < 1) * 2;
                if (D_800AD38C.limit < D_800AD38C.elapsed) {
                    end = 3;
                }
                if (!(D_800B08EC[0].flags & 3)) {
                    end = 1;
                }
                if (D_800AD8E0 + D_800AD8E4 >= D_800AD8DC) {
                    end = 1;
                }
                if (D_800AD8E0 >= D_800AD8DC) {
                    end = 1;
                }
            } else {
                if (D_800AE8D0.x10 <= 0) {
                    end = 2;
                }
                if (D_800AD38C.limit < D_800AD38C.elapsed) {
                    end = 3;
                }
                if (D_800AD8E0 + D_800AD8E4 >= D_800AD8DC) {
                    end = 1;
                }
            }
            D_800AD384 = end;
            if (end == 1) {
                D_800AD3B4 = D_800AD38C.left;
            } else {
                D_800AD3B4 = 0;
            }
            switch (end) {
            case 2:
                D_800AD95C.decay = -0x10;
                D_800AD95C.amp += D_800AD95C.amp_start * 2;
            case 3:
                D_800ADF40.r0 = 0;
                submar_show_layer(19, 1);
            case 1:
                submar_show_message(end);
                submar_camera_to_result();
                D_800AD8F0 = 0;
                D_800AEB30 = 1;
                snd_request.id = 0xB9;
                snd_request.arg[0] = 300;
                snd_request.arg[1] = 0;
                snd_submit_request();
                snd_request.id = 0xC1;
                snd_request.arg[0] = 300;
                snd_request.arg[1] = 0;
                snd_submit_request();
                D_800AD388 |= 2;
                break;
            }
            continue;
        }
        D_800AD38C.countdown -= e;
        if (!D_800AD380.mode) {
            if (D_800AD384 == 3 && D_800AD38C.countdown < 0x1C3) {
                submar_show_message(2);
            }
        } else {
            if (D_800AD38C.countdown < 0x259) {
                submar_set_count();
                submar_show_message(7);
            }
            if (D_800AD38C.countdown < 0x1C3) {
                submar_set_score();
                submar_show_result(6);
            }
        }
        if (D_800AD380.mode) {
            D_800ADF40.r0 = (1200 - D_800AD38C.countdown) / 15;
        } else {
            D_800ADF40.r0 = (900 - D_800AD38C.countdown) * 80 / 900;
        }
        D_800ADF40.x0 = -0xA0 - D_800AD3C8.x;
        D_800ADF40.y0 = -0x96 - D_800AD3C8.y;
        if (D_800AD38C.countdown <= 0) {
            break;
        }
    }
    SetDispMask(0);
    if (D_800AD384 == 5) {
        VSync(30);
        snd_request.id = 0xB8;
        snd_request.arg[0] = 0;
        snd_submit_request();
        snd_request.id = 0xC0;
        snd_request.arg[0] = 0;
        snd_submit_request();
        snd_request.id = 0x98;
        snd_submit_request();
        VSync(30);
    }
    snd_request.id = 0xF1;
    snd_submit_request();
    snd_request.id = 0xF0;
    snd_submit_request();
    snd_request.id = 0xB8;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0xBA;
    snd_request.arg[0] = 0x40;
    snd_submit_request();
    snd_request.id = 0xC0;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    VSync(15);
    if (!D_800AD380.mode) {
        if (D_800AD384 == 1) {
            D_800AD380.result = 1;
        } else {
            D_800AD380.result = 0;
        }
    } else if (D_800AD384 != 1) {
        D_800AD380.result = 0;
    } else {
        D_800AD380.result = 1;
    }
    return D_800AD380.result;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_main);
#endif

#ifdef NON_MATCHING
/* The game keeps each load below the stores written before it; GCC here moves the loads up. */
void submar_camera_to_result(void)
{
    D_801BA30C = D_800AE914;
    D_801BA30E = D_800AE916;
    D_801BA310 = D_800AE918;
    D_801BA314 = -0x200;
    D_801BA316 = D_800AE916;
    D_801BA318 = D_800AE918;
    D_801BA31C = -0x200 - D_800AE914;
    D_801BA31E = 0x1000;
    D_801BA320 = 0;
    D_801BA324 = D_800AE944;
    D_801BA328 = 0x2800;
    D_801BA32C = 0x2800 - D_800AE944;
    D_801BA330 = D_800AD40C;
    D_801BA334 = 0x200;
    D_801BA338 = 0x200 - D_800AD40C;
    D_801BA33C = 0;
    D_801BA340 = 0x10;
    D_801BA344 = 1;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_camera_to_result);
#endif

#ifdef NON_MATCHING
/* The first scan's decrement fills the game's delay slot only there, and register choice. */
void submar_ot_span(void)
{
    unsigned int *p;
    int i;
    int top = 0, bottom = 0;

    for (p = &D_801B7310[D_800AD3B8][0x5FE], i = 0x5FF; p > &D_801B7310[D_800AD3B8][-1]; p--, i--) {
        if (*p != ((unsigned int)p << 8 >> 8) - 4) {
            top = i;
            break;
        }
    }
    if (D_801B7310[D_800AD3B8][0] == ((unsigned int)&D_801B7310[D_800AD3B8][-1] << 8 >> 8)) {
        for (p = &D_801B7310[D_800AD3B8][1], i = 2; p < &D_801B7310[D_800AD3B8][0x5FE]; p++, i++) {
            if (*p != ((unsigned int)p << 8 >> 8) - 4) {
                bottom = i;
                p[-1] = (unsigned int)&D_801B7310[D_800AD3B8][-1] << 8 >> 8;
                break;
            }
        }
    }
    D_800AD3BC = bottom;
    D_800AD3BA = top;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_ot_span);
#endif



void submar_load_sound(unsigned char mode)
{
    cd_read(D_800AD330, D_800AD334, D_800B365C, 0);
    while (cd_step()) {
    }
    cd_read(D_800AD338, D_800AD33C, D_800B565C, 0);
    while (cd_step()) {
    }
    snd_request.id = 0xC0;
    snd_request.arg[0] = 1;
    snd_submit_request();
    snd_request.id = 0xA0;
    snd_request.arg[0] = 0x7E;
    snd_submit_request();
    snd_request.id = 0xA1;
    snd_request.arg[0] = 1;
    snd_submit_request();
    snd_request.id = 0xA2;
    snd_request.arg[0] = 1;
    snd_submit_request();
    snd_request.id = 0xA3;
    snd_request.arg[0] = 0x7E;
    snd_submit_request();
    snd_request.id = 0x29;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0x238;
    snd_submit_request();
    snd_request.id = 0x2A;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0x230;
    snd_submit_request();
}

void submar_load_tim(TimBlock *tim, SubTex *tex)
{
    int mode;
    RECT r;

    if (*(unsigned short *)tim != 0x10) {
        return;
    }
    mode = ((unsigned int *)tim)[1] & 7;
    tex->mode = mode;
    if (((unsigned int *)tim)[1] & 8) {
        tim = (TimBlock *)((unsigned int *)tim + 2);
        if (tim->len >= 5) {
            r.x = tim->rect.x;
            r.y = tim->rect.y;
            r.w = 0x100;
            r.h = 1;
            func_80044000(&r, tim + 1);
            tex->clut = getClut(tim->rect.x, tim->rect.y);
            tex->clut_rect.x = r.x;
            tex->clut_rect.y = r.y;
            tex->clut_rect.w = r.w;
            tex->clut_rect.h = r.h;
        }
        tim = (TimBlock *)((char *)tim + tim->len);
    } else {
        tim = (TimBlock *)((unsigned int *)tim + 2);
    }
    if (tim->len >= 5) {
        func_80044000(&tim->rect, tim + 1);
        tex->tpage = getTPage(mode, 1, tim->rect.x, tim->rect.y);
        tex->image.x = tim->rect.x;
        tex->image.y = tim->rect.y;
        tex->image.w = tim->rect.w;
        tex->image.h = tim->rect.h;
    }
}


void submar_load_tims(void)
{
    cd_read_lzs(D_800AD340, D_800AD344, (void *)0x80100000, 0);
    while (cd_step()) {
    }
    submar_load_tim((TimBlock *)0x80100000, &D_800AD91C);
    cd_read_lzs(D_800AD348, D_800AD34C, (void *)0x80100000, 0);
    while (cd_step()) {
    }
    submar_load_tim((TimBlock *)0x80100000, &D_800AD904);
}

#ifdef NON_MATCHING
/* The game computes the file table entry's address twice; register choice. */
void submar_load_mission(unsigned char n)
{
    unsigned int a, b;

    if (n < 6) {
        cd_read(D_800AD350[n].lba, D_800AD350[n].size, &D_800B0E5C, 0);
        while (cd_step()) {
        }
    }
    submar_init_paths(D_800B265C);
    submar_init_subs(D_800B0E5C.subs);
    a = D_800B0E5C.floor_depth;
    if (a >= 16) {
        a = 0;
    }
    D_800B0E5C.floor_depth = a << 18;
    b = D_800B0E5C.floor_offset;
    if (b >= 16) {
        b = 0;
    }
    D_800B0E5C.floor_offset = b;
    D_800B0E5C.x101C = (D_800B0E5C.x1014 << 12) / D_800B0E5C.x1018;
    D_800B0E5C.x1028 = (D_800B0E5C.x1020 << 12) / D_800B0E5C.x1024;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_load_mission);
#endif

#ifdef NON_MATCHING
/* Register choice: the two tpage addresses and the zeros the back and far colours take. */
void submar_init_gfx(void)
{
    RECT r;

    r.x = 0;
    r.y = 0;
    r.w = 0x140;
    r.h = 0x1E0;
    ClearImage(&r, 0, 0, 0);
    SetDefDispEnv(&D_800B5F14[0], 0, 0, 0x140, 0xF0);
    func_80043814(&D_800B5E5C[0], 0, 0xF0, 0x140, 0xF0);
    SetDefDispEnv(&D_800B5F14[1], 0, 0xF0, 0x140, 0xF0);
    func_80043814(&D_800B5E5C[1], 0, 0, 0x140, 0xF0);
    D_800B5E5C[0].ofs[0] = D_800B5E5C[1].ofs[0] = 0xA0;
    D_800B5E5C[0].ofs[1] = 0x186;
    D_800B5E5C[1].ofs[1] = 0x96;
    D_800B5E5C[0].tpage = D_800B5E5C[1].tpage = getTPage(0, 1, 0x200, 0);
    D_800B5E5C[0].tw.w = D_800B5E5C[1].tw.w = 0xFF;
    D_800B5E5C[0].tw.h = D_800B5E5C[1].tw.h = 0xFF;
    D_800B5E5C[0].tw.x = D_800B5E5C[1].tw.x = 0;
    D_800B5E5C[0].tw.y = D_800B5E5C[1].tw.y = 0;
    D_800B5E5C[0].dtd = D_800B5E5C[1].dtd = 0;
    D_800B5E5C[0].isbg = D_800B5E5C[1].isbg = 1;
    D_800B5E5C[0].r0 = D_800B5E5C[1].r0 = 0;
    D_800B5E5C[0].g0 = D_800B5E5C[1].g0 = 0;
    D_800B5E5C[0].b0 = D_800B5E5C[1].b0 = 0;
    srand(rand());
    D_800B5F3C = D_800AD420;
    D_800B5F40 = D_800AD424;
    D_800B5F44 = D_800AD428;
    gte_SetRotMatrix(&D_800AD3EC);
    gte_SetTransMatrix(&D_800AD3EC);
    gte_SetLightMatrix(&D_800AD3EC);
    gte_SetBackColor(0, 0, 0);
    gte_SetColorMatrix(&D_800AD3EC);
    gte_SetFarColor(0, 0, 0);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_init_gfx);
#endif

#ifdef NON_MATCHING
/* Register choice in the key-repeat loop. */
void submar_read_pad(void)
{
    unsigned int pad, pressed, held;
    unsigned int i;

    pad = func_8001C808();
    D_800AD458 = pad;
    D_800AD460 = pad >> 16;
    pressed = pad & ~D_800AD45A;
    held = D_800AD45A & pad;
    D_800AD45A = pad;
    D_800AD45C = pressed;
    for (i = 0; i < 5; i++) {
        if (D_800AD44C[i] & pressed) {
            D_800AD440[i] = 10;
        }
        if (D_800AD44C[i] & held) {
            if (D_800AD440[i] == 0) {
                pressed |= D_800AD44C[i];
                D_800AD440[i] = 1;
            } else {
                D_800AD440[i]--;
            }
        }
    }
    D_800AD45E = pressed;
    D_800AD464 = D_800AD460 & ~D_800AD462;
    D_800AD462 = D_800AD460;
    D_800AD466 = 0;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_read_pad);
#endif

#ifdef NON_MATCHING
/* Register choice: the scratchpad's second and third words swap registers. */
void submar_set_view(void)
{
    SCRATCH_ANGLE->vx = -D_800AE914 - D_800AE90C;
    if (!D_800AE968) {
        SCRATCH_ANGLE->vx -= D_800AE91C;
    }
    SCRATCH_ANGLE->vy = -D_800AE91E - D_800AE916 - D_800AE90E;
    SCRATCH_ANGLE->vz = -D_800AE920 - D_800AE918 - D_800AE910;
    sub_800A1DA0(SCRATCH_ANGLE, &D_800F5F4C);
    *SCRATCH_VX = D_800AE8D0.x >> 12;
    *SCRATCH_VY = D_800AE8D4 >> 12;
    *SCRATCH_VZ = D_800AE8D8 >> 12;
    gte_SetRotMatrix(&D_800F5F4C);
    gte_ldlvl(SCRATCH_VX);
    gte_rtir();
    gte_stlvnl(SCRATCH_VX);
    D_800F5F4C.t[0] = -*SCRATCH_VX;
    D_800F5F4C.t[1] = -*SCRATCH_VY;
    D_800F5F4C.t[2] = D_800AE944 - *SCRATCH_VZ;
    gte_SetTransMatrix(&D_800F5F4C);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_set_view);
#endif

INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A1DA0);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A1F9C);
void submar_init_floor(void)
{
    unsigned char *src = D_800B0E5C.floor;
    Cell *c;
    int h, i, off;

    off = D_800B0E5C.floor_offset - 0x30;
    c = D_800B5F4C;
    do {
        h = *src++ + off;
        if (h < 0) {
            h = 0;
        }
        if (h > 16) {
            h = 16;
        }
        c->target = c->h = c->base = h << 6;
        c->flags = 0;
    } while (++c < &D_800B5F4C[0x1000]);
    for (i = 0, c = D_800B5F4C; i < 0x1000; i++, c++) {
        if (i % 64 != 63) {
            c->h_right = c[1].h;
        } else {
            c->h_right = 0x3C0;
        }
        if (i % 64 != 0) {
            c->h_left = c[-1].h;
        } else {
            c->h_left = 0x3C0;
        }
        if (i / 64 != 63) {
            c->h_down = c[64].h;
        } else {
            c->h_down = 0x3C0;
        }
        if (i / 64 != 0) {
            c->h_up = c[-64].h;
        } else {
            c->h_up = 0x3C0;
        }
    }
}


#ifdef NON_MATCHING
/* Register choice: two registers. */
void submar_raise_cells(short n)
{
    Cell *c;
    int i, found, floor, row, col, t, dr, dc;

    floor = D_800B0E5C.floor_depth / 4096;
    row = D_800AE960 / 64;
    col = D_800AE960 % 64;
    if (n > 500) {
        n = 500;
    }
    while (n > 0) {
        found = 0;
        for (i = 0; i < 0x1000; i++) {
            c = &D_800B5F4C[i];
            if (c->flags & 0x2000) {
                continue;
            }
            found = 1;
            dr = i / 64 - row;
            dc = i % 64 - col;
            if ((unsigned int)(dr + 2) < 5 && (unsigned int)(dc + 2) < 5) {
                continue;
            }
            if (floor >= c->base) {
                continue;
            }
            if ((short)rand() >= 0x7F) {
                continue;
            }
            c->flags = 0x2000;
            t = 0x400 - rand() * 8 / 0x7FFF * 64;
            if (t < 0) {
                t = 0;
            }
            if (c->base < t) {
                t = c->base;
            }
            c->target = t;
            if (--n <= 0) {
                break;
            }
        }
        if (!found) {
            break;
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_raise_cells);
#endif


#ifdef NON_MATCHING
/* GCC here reaches the four neighbours from one pointer; the game keeps a pointer for each. */
void submar_update_floor(void)
{
    unsigned int i;
    int h, target, d, flags, t;

    for (i = 0; i < 0x1000; i++) {
        h = D_800B5F4C[i].h;
        flags = D_800B5F4C[i].flags;
        target = D_800B5F4C[i].base;
        if (flags & 0x1000) {
            target = D_800B5F4C[i].target;
        }
        if (h != target) {
            if (h < target) {
                if (target - h >= 3) {
                    h += 2;
                } else {
                    h++;
                }
            } else {
                d = h - target;
                if (d >= 5) {
                    h -= d / 4;
                } else {
                    h--;
                }
            }
            D_800B5F4C[i].h = h;
            if (i % 64 != 0) {
                D_800B5F4C[i - 1].h_right = h;
            }
            if (i % 64 != 63) {
                D_800B5F4C[i + 1].h_left = h;
            }
            if (i / 64 != 63) {
                D_800B5F4C[i + 64].h_up = h;
            }
            if (i / 64 != 0) {
                D_800B5F4C[i - 64].h_down = h;
            }
        } else {
            D_800B5F4C[i].flags = flags & ~0x1000;
        }
        flags = D_800B5F4C[i].flags;
        if (flags & 0x8000) {
            t = flags & 0xFF;
            flags &= 0xFF00;
            t--;
            if (t < 0) {
                t = 0;
                flags = 0;
                D_800B5F4C[i].target = D_800B5F4C[i].base;
            }
            D_800B5F4C[i].flags = flags | t;
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_update_floor);
#endif


INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A32C8);
/* Copies of libgte's ratan2 and of a vector normalise built from GTE register moves; a second pair sits at
 * 0x800A9914, so the module seems to be two source files that each have them. ratan2 divides without the checks
 * the rest of the module has. Kept as asm. */
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_ratan2);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_normalize);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A36F4);
#ifdef NON_MATCHING
/* The game tests the first sub at fixed addresses before its search loop. */
void submar_lock_target(void)
{
    Sub *s, *best;
    int i, d;

    s = D_800B08EC;
    while (!(s->state & 0x800)) {
        s++;
        if (s >= &D_800B08EC[12]) {
            break;
        }
    }
    if (s < &D_800B08EC[12]) {
        if (s->flags & 0x10) {
            if (s->flags & 4) {
                return;
            }
            s->state = 0x200;
            s->x3C = 0x2D;
        } else {
            s->state = 0;
            s->x3C = 0;
        }
    }
    d = 0x7FFFFFFF;
    best = 0;
    for (i = 0; i < 12; i++) {
        if ((D_800B08EC[i].flags & 0x14) == 0x14 && (D_800B08EC[i].state & 0x400) && D_800B08EC[i].dist < d) {
            d = D_800B08EC[i].dist;
            best = &D_800B08EC[i];
        }
    }
    if (best) {
        best->state = 0x800;
        best->x3C = 0x2D;
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_lock_target);
#endif


void submar_draw_subs(void)
{
    Sub *s;
    void *p = D_8013660C[D_800AD3B8];

    s = D_800B08EC;
    do {
        if ((s->flags & 0x20) || ((s->flags & 3) && (s->flags & 0x10) && (s->state & 0xF00))) {
            D_800AEA90.vx = s->x >> 12;
            D_800AEA90.vy = s->y >> 12;
            D_800AEA90.vz = s->z >> 12;
            D_800AEA98.vx = s->rx;
            D_800AEA98.vy = s->ry;
            D_800AEA98.vz = s->rz;
            p = sub_800ABD4C(p, s->kind);
        }
    } while (++s < &D_800B08EC[12]);
    gte_SetRotMatrix(&D_800F5F4C);
    gte_SetTransMatrix(&D_800F5F4C);
}


INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A4624);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A4914);
void submar_init_paths(PathNode *n)
{
    unsigned int i;

    do {
        n->x <<= 18;
        n->y <<= 18;
        n->z <<= 18;
        i = n->next;
        if (i > 128) {
            i = 0;
        }
        n->next = (unsigned int)&D_800B265C[i];
    } while (++n < &D_800B265C[128]);
}


void submar_init_subs(SubRecord *r)
{
    Sub *s;
    int col, row;

    D_800AD8DC = 0;
    D_800AD8E0 = 0;
    D_800AD8E4 = 0;
    D_800AD8E8 = 0;
    s = D_800B08EC;
    do {
        s->x = r->x << 18;
        s->y = r->y << 18;
        s->z = r->z << 18;
        col = s->x >> 12;
        row = s->z >> 12;
        col = (col + 0x800) / 64;
        if (col < 0) {
            col = 0;
        }
        if (col >= 64) {
            col = 63;
        }
        row = (0x800 - row) / 64;
        if (row < 0) {
            row = 0;
        }
        if (row >= 64) {
            row = 63;
        }
        s->cell = row * 64 + col;
        s->value = r->value;
        s->rx = r->rx;
        s->ry = r->ry;
        s->rz = r->rz;
        s->x2C = r->x1C;
        s->x30 = r->x20;
        s->flags = r->flags;
        if (r->flags & 1) {
            s->node = &D_800B265C[r->node];
            s->kind = D_800AEAA0[r->kind];
            D_800AD8DC++;
            D_800AD8E8 += r->value;
        }
        r++;
    } while (++s < &D_800B08EC[12]);
}


INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A4D90);
#ifdef NON_MATCHING
/* Instruction order and register choice in the digits. */
void submar_set_score(void)
{
    Sub *s;
    int n, d, c, lead;

    n = 0;
    for (s = D_800B08EC; s < &D_800B08EC[12]; s++) {
        if (s->flags & 3) {
            n += s->value;
        }
    }
    n = D_800AD8E8 - n + D_800AE8E0 + D_800AD3B4 - D_800AE9FC * 100;
    if (n < 0) {
        n = 0;
    }
    if (n > 99999) {
        n = 99999;
    }
    lead = 1;
    d = n / 10000;
    c = d;
    if (d == 0 && lead == 1) {
        c = 12;
    } else {
        lead = 0;
    }
    D_800AE540[0].u0 = c << 4;
    n -= d * 10000;
    d = n / 1000;
    c = d;
    if (d == 0 && lead == 1) {
        c = 12;
    } else {
        lead = 0;
    }
    D_800AE540[1].u0 = c << 4;
    n -= d * 1000;
    d = n / 100;
    c = d;
    if (d == 0 && lead == 1) {
        c = 12;
    } else {
        lead = 0;
    }
    D_800AE540[2].u0 = c << 4;
    n -= d * 100;
    d = n / 10;
    c = d;
    if (d == 0 && lead == 1) {
        c = 12;
    } else {
        lead = 0;
    }
    D_800AE540[3].u0 = c << 4;
    n -= d * 10;
    D_800AE540[4].u0 = n << 4;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_set_score);
#endif

#ifdef NON_MATCHING
/* Register choice and the stores' order. */
void submar_set_count(void)
{
    int n = D_800AD8E0 + D_800AD8E4;

    D_800AE484 = n / 10 << 4;
    D_800AE498 = n % 10 << 4;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_set_count);
#endif

void submar_set_quit_choice(int on)
{
    unsigned int a, b;

    a = 0x64808080;
    b = 0x64202020;
    if (!on) {
        a = 0x64202020;
        b = 0x64808080;
    }
    D_800AE3C8 = a;
    D_800AE3F0 = b;
}


#ifdef NON_MATCHING
/* The link's word is loaded before its new address is computed here, after in the game. */
void submar_show_layer(unsigned int i, int mode)
{
    unsigned int *first, *last;

    if (i < 20) {
        setaddr(&D_801BCF08[i], &D_801BCF08[i + 1]);
        if (mode == 1) {
            first = D_800AE5A4[i].first;
            last = D_800AE5A4[i].last;
            if (first && last) {
                addPrims(&D_801BCF08[i], first, last);
            }
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_show_layer);
#endif


#ifdef NON_MATCHING
/* The link's word is loaded before its new address is computed here, after in the game. */
void submar_show_message(int n)
{
    unsigned int *first, *last;

    setaddr(&D_801BCF08[16], &D_801BCF08[17]);
    if (n) {
        first = D_800AE644[n].first;
        last = D_800AE644[n].last;
        if (first && last) {
            addPrims(&D_801BCF08[16], first, last);
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_show_message);
#endif

#ifdef NON_MATCHING
/* The link's word is loaded before its new address is computed here, after in the game. */
void submar_show_result(int n)
{
    unsigned int *first, *last;

    setaddr(&D_801BCF08[17], &D_801BCF08[18]);
    if (n) {
        first = D_800AE644[n].first;
        last = D_800AE644[n].last;
        if (first && last) {
            addPrims(&D_801BCF08[17], first, last);
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_show_result);
#endif

#ifdef NON_MATCHING
/* One delay slot is filled from the other branch. */
void submar_show_alerts(void)
{
    unsigned int flags = D_800AE96C;
    int n = 0;

    if (flags & 0x300) {
        D_800AD97C[0].sprt.clut = D_800AD904.clut + 3;
        if (flags & 0x200) {
            D_800AD97C[0].sprt.clut = D_800AD904.clut + 1;
        }
        addPrim(&D_801BCF08[20], &D_800AD97C[0]);
        n = 1;
    }
    if (flags & 0x400) {
        addPrim(&D_801BCF08[21], &D_800AD97C[1]);
        n += 2;
    }
    if (flags & 0x4000) {
        addPrim(&D_801BCF08[22], &D_800AD97C[2]);
        n += 3;
    }
    if (flags & 0x8000) {
        addPrim(&D_801BCF08[22], &D_800AD97C[3]);
        n += 5;
    }
    if (n) {
        n <<= 3;
        if (n >= 0x80) {
            n = 0x7F;
        }
    } else {
        n = 1;
    }
    D_800AD93C = n;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_show_alerts);
#endif


#ifdef NON_MATCHING
/* The game keeps each load below the stores written before it; GCC here moves the loads up. */
void submar_update_hud(void)
{
    int y, n, d, a, f, i, v;
    unsigned int c, color;
    unsigned short w;

    y = D_800AE8D4 >> 12;
    n = y * 11 / 100;
    D_800ADAE0[11].y0 = n - 0x73;
    D_800ADAE0[12].y0 = D_800ADAE0[13].y0 = D_800ADAE0[14].y0 = D_800ADAE0[15].y0 = n - 0x70;
    d = y / 10;
    D_800ADAE0[15].u0 = (y - d * 10) * 4 + 200;
    y = d / 10;
    D_800ADAE0[14].u0 = (d - y * 10) * 4 + 200;
    d = y / 10;
    D_800ADAE0[13].u0 = (y - d * 10) * 4 + 200;
    D_800ADAE0[12].u0 = (d - d / 10 * 10) * 4 + 200;
    a = (D_800AE97C << 6) / (D_800B0E5C.player.speed_scale * 2);
    n = D_800AE97C * 200 / (D_800B0E5C.player.speed_scale * 2);
    D_800ADAE0[16].y0 = -0x33 - a;
    D_800ADAE0[17].y0 = D_800ADAE0[18].y0 = D_800ADAE0[19].y0 = D_800ADAE0[20].y0 = -0x30 - a;
    if (n > 0) {
        D_800ADAE0[17].u0 = 0xF0;
    } else if (n == 0) {
        D_800ADAE0[17].u0 = 0xF8;
    } else {
        D_800ADAE0[17].u0 = 0xF4;
        n = -n;
    }
    d = n / 10;
    D_800ADAE0[20].u0 = (n - d * 10) * 4 + 200;
    n = d;
    d = n / 10;
    D_800ADAE0[19].u0 = (n - d * 10) * 4 + 200;
    n = d;
    a = ((D_800AE8E6 + 0xC00) & 0xFFF) / 16;
    d = n / 10;
    D_800ADAE0[18].u0 = (n - d * 10) * 4 + 200;
    a = (a + 0xCB) & 0xFF;
    n = 0x100 - a;
    if (n >= 0x7D) {
        D_800ADAE0[21].w = 0x7D;
        D_800ADAE0[21].u0 = a;
        setaddr(&D_800ADAE0[21], &D_800ADAE0[23]);
    } else {
        D_800ADAE0[22].x0 = n - 0x3E;
        D_800ADAE0[22].w = 0x7D - n;
        D_800ADAE0[21].u0 = a;
        D_800ADAE0[21].w = n;
        D_800ADAE0[22].u0 = 0;
        setaddr(&D_800ADAE0[21], &D_800ADAE0[22]);
    }
    color = D_800AE8E4 ? 0x66808080 : 0x66C0C0A0;
    c = D_800AE8E4 ? 0x66808080 : 0x64606060;
    *(unsigned int *)&D_800ADAE0[7].r0 = c;
    *(unsigned int *)&D_800ADAE0[10].r0 = color;
    D_800ADAE0[10].y0 = -0x30 - D_800AE8E4 / 16;
    n = D_800AE8E0;
    if (n < 0) {
        n = 0;
    }
    a = n * 127 / 16384;
    if (a == 0 && n > 0) {
        a = 1;
    }
    D_800ADAE0[25].x0 = D_800ADAE0[26].x0 = a - 0x3F;
    a = n * 255 / 16384;
    *(unsigned int *)&D_800ADAE0[25].r0 = *(unsigned int *)&D_800ADAE0[26].r0 = a << 8 | (0xFF - a);
    a = D_800AD948 + 1;
    f = D_800AD94C;
    if (a >= 40) {
        f++;
        a = 0;
        if ((unsigned short)f >= 6) {
            f = 0;
        }
    }
    D_800AD94C = (unsigned short)f;
    D_800AD948 = a;
    D_800ADD44.x0 = D_800ADD44.x2 = a + 0x52;
    D_800ADD44.x1 = D_800ADD44.x3 = a + 0x5A;
    w = D_800ADD10[(unsigned short)f];
    D_800ADD44.u0 = D_800ADD44.u2 = w + a;
    D_800ADD44.u1 = D_800ADD44.u3 = w + a + 8;
    D_800ADD44.v0 = D_800ADD44.v1 = w >> 8;
    D_800ADD44.v2 = D_800ADD44.v3 = (w >> 8) + 0x1A;
    n = D_800AD944 + 1;
    if (n >= 0x7C) {
        n = 0x56;
    }
    D_800AD944 = n;
    a = n;
    if (a >= 0x29) {
        a -= 0x29;
        if (a >= 0x29) {
            a -= 0x29;
            if (a >= 0x29) {
                a -= 0x29;
            }
        }
    }
    D_800ADDB4.x0 = D_800ADDB4.x2 = a - 0x7E;
    D_800ADDB4.x1 = D_800ADDB4.x3 = a - 0x7A;
    D_800ADDB4.u0 = D_800ADDB4.u2 = n + 0x70;
    D_800ADDB4.u1 = D_800ADDB4.u3 = n + 0x74;
    a = D_800AD950 + 1;
    if (a >= 0x20) {
        a = 0;
    }
    D_800AD950 = a;
    D_800ADE10.u0 = a / 8 * 16 + 0x50;
    a = D_800AD954 + 1;
    if (a >= 0x20) {
        a = 0;
    }
    D_800AD954 = a;
    D_800ADE38.v0 = a / 8 * 24;
    a = D_800AD958 + 1;
    if (a >= 0x10) {
        a = 0;
    }
    D_800AD958 = a;
    D_800ADE60.v0 = a / 4 * 24 + 0x81;
    for (i = 0; i < 4; i++) {
        v = 0x20;
        color = 0x66000000;
        f = D_800AE984[i];
        if (f & 0x10000) {
            c = (f & 0xFFFF) * 255 / D_800AD934;
            v = 0x18;
            c = (0xFF - c) & 0xFF;
            color = c | c << 8 | c << 16 | 0x66000000;
        } else if (f & 0x20000) {
            color = 0x66FFFFFF;
            v = 0x10;
        }
        *(unsigned int *)&D_800ADEC4[i].r0 = color;
        D_800ADEC4[i].v0 = v;
    }
    n = D_800AD3A8;
    if (n < 0) {
        n = 0;
    }
    n /= 60;
    d = n / 600;
    n -= d * 600;
    D_800AE2AC[0].u0 = d * 8 + 0x78;
    d = n / 60;
    n -= d * 60;
    D_800AE2AC[1].u0 = d * 8 + 0x78;
    d = n / 10;
    D_800AE2AC[2].u0 = d * 8 + 0x78;
    n -= d * 10;
    D_800AE2AC[3].u0 = n * 8 + 0x78;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_update_hud);
#endif

#ifdef NON_MATCHING
/* The multiplies' operand order and register choice. */
void submar_update_shake(void)
{
    if (D_800AD95C.start) {
        if (!D_800AD95C.amp) {
            D_800AD95C.angle0 = D_800AD95C.angle0_start;
            D_800AD95C.angle1 = D_800AD95C.angle1_start;
        }
        D_800AD95C.start = 0;
        D_800AD95C.amp = D_800AD95C.amp_start;
    } else {
        D_800AD95C.angle0 = (D_800AD95C.angle0 + D_800AD95C.step0) & 0xFFF;
        D_800AD95C.angle1 = (D_800AD95C.angle1 + D_800AD95C.step1) & 0xFFF;
    }
    D_800AD95C.out0 = D_800AD95C.amp * D_8004BC98[D_800AD95C.angle0 * 2] / 4096 >> 8;
    D_800AD95C.out1 = D_800AD95C.amp * D_8004BC98[D_800AD95C.angle1 * 2] / 4096 >> 8;
    D_800AD95C.amp += D_800AD95C.decay;
    if (D_800AD95C.amp < 0) {
        D_800AD95C.amp = 0;
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_update_shake);
#endif

#ifdef NON_MATCHING
/* Register choice around the absolute value and the table read. */
void submar_sonar_torpedoes(void)
{
    Torpedo *t;
    int i, n, d, a, c, pan, vol;

    n = 0;
    vol = 0;
    pan = 0;
    for (i = 0, t = D_800F5F6C; i < 32; i++, t++) {
        if (t->flags & 6) {
            if (t >= &D_800F5F6C[16]) {
                D_800AE8D0.alerts |= 0x8000;
            }
            d = t->dist;
            d = 0x100 - d;
            if (d >= 0) {
                n++;
                a = t->angle;
                if (a < -0x800 && (t->flags & 2)) {
                    D_800AE8D0.alerts |= 0x2000;
                }
                if (a < 0) {
                    a = -a;
                }
                c = ((int *)D_8004BC98)[a / 4 & 0xFFF] >> 17;
                if (t->side < 0) {
                    c = -c;
                }
                pan += c * d;
                vol += d * d;
            }
        }
    }
    if (n) {
        pan = pan / 256 / n;
        vol = vol / 256 / n;
        pan /= 32;
        vol /= 2;
        if (pan < -0x40) {
            pan = -0x40;
        }
        if (pan >= 0x40) {
            pan = 0x3F;
        }
        if (vol >= 0x80) {
            vol = 0x7F;
        }
        if (!vol) {
            vol = 1;
        }
        snd_request.id = 0xA6;
        snd_request.arg[0] = 4;
        snd_request.arg[1] = vol;
        snd_submit_request();
        snd_request.id = 0xAE;
        snd_request.arg[0] = 4;
        snd_request.arg[1] = pan + 0x40;
    } else {
        snd_request.id = 0xA6;
        snd_request.arg[0] = 8;
        snd_request.arg[1] = 1;
        snd_submit_request();
        snd_request.id = 0xAE;
        snd_request.arg[0] = 2;
        snd_request.arg[1] = 0x40;
    }
    snd_submit_request();
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_sonar_torpedoes);
#endif


#ifdef NON_MATCHING
/* Register choice around the absolute value and the table read. */
void submar_sonar_subs(void)
{
    Sub *t;
    int n, d, a, c, pan, vol;

    n = 0;
    vol = 0;
    pan = 0;
    for (t = D_800B08EC; t < &D_800B08EC[12]; t++) {
        if (t->flags & 3) {
            if (t->state & 0xF00) {
                D_800AE8D0.alerts |= 0x4000;
            }
            d = t->dist;
            d = 0x100 - d;
            if (d >= 0) {
                n++;
                a = t->angle;
                if (a < -0x800 && (t->flags & 8)) {
                    D_800AE8D0.alerts |= 0x1000;
                }
                if (a < 0) {
                    a = -a;
                }
                c = ((int *)D_8004BC98)[a / 4 & 0xFFF] >> 17;
                if (t->side < 0) {
                    c = -c;
                }
                pan += c * d;
                vol += d * d;
            }
        }
    }
    if (n) {
        pan = pan / 256 / n;
        vol = vol / 256 / n;
        pan /= 32;
        vol /= 2;
        if (pan < -0x40) {
            pan = -0x40;
        }
        if (pan >= 0x40) {
            pan = 0x3F;
        }
    }
    vol += 0x18;
    if (vol >= 0x80) {
        vol = 0x7F;
    }
    snd_request.id = 0xA5;
    snd_request.arg[0] = 4;
    snd_request.arg[1] = vol;
    snd_submit_request();
    snd_request.id = 0xAD;
    snd_request.arg[0] = 4;
    snd_request.arg[1] = pan + 0x40;
    snd_submit_request();
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_sonar_subs);
#endif

#ifdef NON_MATCHING
/* Register choice. */
void submar_control(void)
{
    unsigned short b;
    int g, y, view;
    short pitch, heading, rx, ry, rz;

    b = D_800AD45E;
    g = D_800AE8D0.gear;
    if (b & 0x10) {
        g++;
    }
    if (b & 0x40) {
        g--;
    }
    if (g >= 0x12) {
        g = 0x11;
    }
    if (g <= 0) {
        g = 1;
    }
    D_800AE8D0.gear = g;
    D_800AE8D0.speed = D_800AE684[g];
    if (b & 0x80) {
        submar_fire();
    }
    b = D_800AD458;
    g = D_800AE8D0.gear;
    if ((b & 0x10) && g == 0x11) {
        g = 0x12;
    }
    if ((b & 0x40) && g == 1) {
        g = 0;
    }
    D_800AE8D0.gear = g;
    D_800AE8D0.speed = D_800AE684[g];
    if (b & 0x2000) {
        D_800AE8D0.x_a4 = 1;
    } else if (b & 0x8000) {
        D_800AE8D0.x_a4 = 2;
    } else if (D_800AE8D0.alerts & 1) {
        if (!D_800AE8D0.x_a0) {
            D_800AE8D0.x_a0 = 1;
            if ((D_800AE8D0.alerts & 0xC) == 4) {
                D_800AE8D0.x_a4 = 1;
                b |= 0x2000;
            } else if ((D_800AE8D0.alerts & 0xC) == 8) {
                b |= 0x8000;
                D_800AE8D0.x_a4 = 2;
            } else if (D_800AE8D0.x_a4 == 1) {
                b |= 0x2000;
            } else if (D_800AE8D0.x_a4 == 2) {
                b |= 0x8000;
            }
        } else if (D_800AE8D0.x_a4 == 1) {
            b |= 0x2000;
        } else if (D_800AE8D0.x_a4 == 2) {
            b |= 0x8000;
        }
    } else {
        D_800AE8D0.x_a0 = 0;
    }
    y = D_800AE8D0.y;
    if (b & 4) {
        y -= 0x1000;
    }
    if (b & 1) {
        y += 0x1000;
    }
    if (y < D_800B0E5C.floor_depth) {
        y = D_800B0E5C.floor_depth;
    }
    if (y > 0x400000) {
        y = 0x400000;
    }
    if (y < D_800B5F4C[D_800AE8D0.cell].base << 12) {
        D_800AE8D0.y = y;
    }
    pitch = D_800AE8D0.pitch;
    heading = D_800AE8D0.heading;
    if (b & 0x1000) {
        pitch -= 0x10;
    }
    if (b & 0x4000) {
        pitch += 0x10;
    }
    if (b & 0x2000) {
        heading += 0x20;
    }
    if (b & 0x8000) {
        heading -= 0x20;
    }
    if (pitch > 0x200) {
        pitch = 0x200;
    }
    if (pitch < -0x200) {
        pitch = -0x200;
    }
    if (heading > 0x800) {
        heading -= 0x1000;
    }
    if (heading < -0x800) {
        heading += 0x1000;
    }
    D_800AE8D0.pitch = pitch;
    D_800AE8D0.heading = heading;
    pitch = D_800AE8D0.pitch2;
    heading = D_800AE8D0.heading2;
    if (b & 0x1000) {
        pitch -= 0x10;
    }
    if (b & 0x4000) {
        pitch += 0x10;
    }
    if (b & 0x2000) {
        heading += 0x20;
    }
    if (b & 0x8000) {
        heading -= 0x20;
    }
    if (pitch > 0x200) {
        pitch = 0x200;
    }
    if (pitch < -0x200) {
        pitch = -0x200;
    }
    if (heading > 0x800) {
        heading -= 0x1000;
    }
    if (heading < -0x800) {
        heading += 0x1000;
    }
    D_800AE8D0.pitch2 = pitch;
    D_800AE8D0.heading2 = heading;
    heading = D_800AE8D0.heading3;
    if (b & 0x2000) {
        heading += 0x20;
    }
    if (b & 0x8000) {
        heading -= 0x20;
    }
    if (heading > 0x800) {
        heading -= 0x1000;
    }
    if (heading < -0x800) {
        heading += 0x1000;
    }
    D_800AE8D0.heading3 = heading;
    rx = D_800AE8D0.roll[0];
    ry = D_800AE8D0.roll[1];
    rz = D_800AE8D0.roll[2];
    if (!D_800AE8D0.x94 || D_800AE8D0.gear == 9) {
        b &= 0xFFF;
    }
    if (b & 0x1000) {
        rx++;
    } else if (b & 0x4000) {
        rx--;
    } else {
        if (rx > 0) {
            rx--;
        }
        if (rx < 0) {
            rx++;
        }
    }
    if (b & 0x2000) {
        ry--;
        rz++;
    } else if (b & 0x8000) {
        ry++;
        rz--;
    } else {
        if (ry > 0) {
            ry--;
        }
        if (ry < 0) {
            ry++;
        }
        if (rz > 0) {
            rz--;
        }
        if (rz < 0) {
            rz++;
        }
    }
    if (rx < -0x10) {
        rx = -0x10;
    }
    if (ry < -0x10) {
        ry = -0x10;
    }
    if (rz < -0x10) {
        rz = -0x10;
    }
    if (rx > 0x10) {
        rx = 0x10;
    }
    if (ry > 0x10) {
        ry = 0x10;
    }
    if (rz > 0x10) {
        rz = 0x10;
    }
    view = D_800AE8D0.view;
    D_800AE8D0.roll[0] = rx;
    D_800AE8D0.roll[1] = ry;
    D_800AE8D0.roll[2] = rz;
    D_800AE8D0.tilt[0] = D_800AE6F0[rx];
    D_800AE8D0.tilt[1] = D_800AE734[ry];
    D_800AE8D0.tilt[2] = D_800AE778[rz];
    D_800AE8D0.x18 = D_800AE7BC[rz];
    if (D_800AD45C & 2) {
        view++;
        if (view >= 4) {
            view = 0;
        }
        submar_change_view(view);
        D_801BA344 = 1;
        D_800AE8D0.view = view;
    } else if (D_800AD45C & 8) {
        D_800AE8D0.view_toggle ^= 1;
        if (D_800AE8D0.view_toggle == 1) {
            view = 4;
        }
        submar_change_view(view);
        D_801BA344 = 1;
    }
    if (D_801BA344) {
        submar_move_camera();
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_control);
#endif

#ifdef NON_MATCHING
/* The order of the view's loads and the camera's stores. */
void submar_change_view(int n)
{
    View *v;
    int r, d;

    r = D_800AE914;
    if (!D_800AE968) {
        if (D_800AE7E0[n].flip) {
            r += D_800AE91C;
        }
    } else if (!D_800AE7E0[n].flip) {
        r -= D_800AE91C;
    }
    v = &D_800AE7E0[n];
    D_801BA30C = r;
    D_801BA314 = v->rx;
    d = ((v->rx + 0x1000) & 0xFFF) - ((r + 0x1000) & 0xFFF);
    if (d > 0x800) {
        d -= 0x1000;
    }
    if (d < -0x800) {
        d += 0x1000;
    }
    D_801BA31C = d;
    r = D_800AE916;
    D_801BA30E = r;
    D_801BA316 = v->ry;
    d = ((v->ry + 0x1000) & 0xFFF) - ((r + 0x1000) & 0xFFF);
    if (d > 0x800) {
        d -= 0x1000;
    }
    if (d < -0x800) {
        d += 0x1000;
    }
    D_801BA31E = d;
    r = D_800AE918;
    D_801BA310 = r;
    D_801BA318 = v->rz;
    d = ((v->rz + 0x1000) & 0xFFF) - ((r + 0x1000) & 0xFFF);
    if (d > 0x800) {
        d -= 0x1000;
    }
    if (d < -0x800) {
        d += 0x1000;
    }
    D_801BA320 = d;
    D_801BA324 = D_800AE944;
    D_801BA328 = v->dist;
    D_801BA32C = v->dist - D_800AE944;
    D_801BA330 = D_800AD40C;
    D_801BA334 = v->height;
    D_801BA338 = v->height - D_800AD40C;
    D_801BA33C = 0;
    D_801BA340 = 0x100;
    D_800AE964 = v->x10;
    D_800AE968 = v->flip;
    D_800AEA00 = v->x18;
    D_800AEA04 = v->x1C;
    D_800AEA10 = v->x20;
    D_800AEA14 = v->x24;
    D_800AEA18 = v->x28;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_change_view);
#endif

INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_move_camera);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A8124);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A832C);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A8944);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A9130);
#ifdef NON_MATCHING
/* Register choice. */
void submar_init_player(PlayerRecord *r)
{
    View *v;
    int x, z, col, row, i, dist, height;
    short rx, ry, rz;

    D_800AE8D0.x = x = r->x << 18;
    D_800AE8D0.y = r->y << 18;
    D_800AE8D0.z = z = r->z << 18;
    D_800AE8D0.x10 = r->x10;
    D_800AE8D0.pitch = D_800AE8D0.pitch2 = r->pitch;
    D_800AE8D0.heading = D_800AE8D0.heading2 = r->heading;
    D_800AE8D0.x18 = D_800AE8D0.x50 = r->x18;
    D_800AE8D0.heading3 = r->heading;
    D_800AE8D0.view_toggle = 0;
    D_800AE8D0.view = r->view;
    v = &D_800AE7E0[r->view];
    D_800AE8D0.cam[0] = rx = v->rx;
    D_800AE8D0.cam[1] = ry = v->ry;
    D_800AE8D0.cam[2] = rz = v->rz;
    D_800AE8D0.dist = dist = v->dist;
    D_800AE8D0.x94 = v->x10;
    D_800AD40C = height = v->height;
    D_800AEA00 = v->x18;
    D_800AEA04 = v->x1C;
    D_800AEA10 = v->x20;
    D_800AEA14 = v->x24;
    D_800AEA18 = v->x28;
    col = ((x >> 12) + 0x800) / 64;
    row = (0x800 - (z >> 12)) / 64;
    D_800AE8D0.cell = row * 64 + col;
    D_800AE8D0.speed = D_800AE684[D_800AE8D0.gear];
    D_801BA30C = rx - 0x200;
    D_801BA30E = D_801BA316 = ry;
    D_801BA31C = 0x200;
    D_801BA31E = 0x1000;
    D_801BA32C = dist - 0x800;
    D_801BA338 = height - 0x800;
    D_801BA340 = 0x20;
    D_801BA344 = 1;
    D_801BA314 = rx;
    D_801BA310 = D_801BA318 = rz;
    D_801BA320 = 0;
    D_801BA324 = 0x800;
    D_801BA328 = dist;
    D_801BA330 = 0x800;
    D_801BA334 = height;
    D_801BA33C = 0;
    D_800AE8D0.x_b0 = (int)D_800AEAA0[r->kind];
    for (i = 0; i < 19; i++) {
        D_800AE684[i] = r->speed_scale * D_800AE684[i] / 16;
    }
    D_800AE8D0.alerts = 0;
    D_800AE8D0.x_a4 = 1;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_init_player);
#endif

INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A96C8);
/* The second copies of submar_ratan2 and submar_normalize. */
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_ratan2_b);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_normalize_b);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A9AD4);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A9CB0);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A9FD0);
void submar_push_trail(Trail *t)
{
    int x = t->x >> 12;
    SVECTOR *s = t->trail[4];
    int y = t->y >> 12;

    s->vx = x;
    s->vz = t->z >> 12;
    s->vy = y;
    t->trail[4] = t->trail[3];
    t->trail[3] = t->trail[2];
    t->trail[2] = t->trail[1];
    t->trail[1] = t->trail[0];
    t->trail[0] = s;
}


INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800AA1D0);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800AA714);
#ifdef NON_MATCHING
/* Which addresses GCC strength-reduces into their own registers. */
void submar_init_torpedoes(void)
{
    unsigned int i;

    for (i = 0; i < 32; i++) {
        setVector(&D_800F5F6C[i].pos, 0, 0, 0);
        setVector(&D_800F5F6C[i].vel, 0, 0, 0);
        setVector(&D_800F5F6C[i].rot, 0, 0, 0);
        setVector(&D_800F5F6C[i].x38, 0, 0, 0x1000);
        setVector(&D_800F5F6C[i].x40, 0, 0, 0);
        setVector(&D_800F5F6C[i].x48, 0, 0, 0x1000);
        D_800F5F6C[i].x78 = 0x20;
        D_800F5F6C[i].x64 = 0;
        D_800F5F6C[i].x68 = 0x1000;
        D_800F5F6C[i].x70 = 0;
        D_800F5F6C[i].x74 = 0;
        D_800F5F6C[i].flags = 1;
        D_800F5F6C[i].x80 = 0;
        D_800F5F6C[i].target = 0;
    }
    for (i = 0; i < 16; i++) {
        D_800F5F6C[i].x6C = D_800B0E5C.x1018;
    }
    for (i = 16; i < 32; i++) {
        D_800F5F6C[i].x6C = D_800B0E5C.x1024;
    }
    D_800AE9FC = 0;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_init_torpedoes);
#endif


#ifdef NON_MATCHING
/* Register choice; the game uses one more callee-saved register. */
void submar_fire(void)
{
    Sub *s;
    int tube, i, x, y, z;

    i = 0;
    while (D_800AE8D0.tubes[i] != 0x20000) {
        i++;
        if (i >= 4) {
            break;
        }
    }
    if (i >= 4) {
        return;
    }
    tube = i;
    i = 0;
    while (D_800F5F6C[i].flags != 1) {
        i++;
        if (i >= 16) {
            break;
        }
    }
    if (i >= 16) {
        return;
    }
    for (s = D_800B08EC; s < &D_800B08EC[12]; s++) {
        if (s->state & 0x800) {
            x = D_800AE8D0.fwd.vx * D_800AEA00;
            y = D_800AE8D0.fwd.vy * D_800AEA00;
            z = D_800AE8D0.fwd.vz * D_800AEA00;
            if (tube & 1) {
                x = -x;
                y = -y;
                z = -z;
            }
            x += D_800AE8D0.side.vx * D_800AEA04 + D_800AE8D0.x;
            y += D_800AE8D0.side.vy * D_800AEA04 + D_800AE8D0.y;
            z += D_800AE8D0.side.vz * D_800AEA04 + D_800AE8D0.z;
            D_800F5F6C[i].trail_buf[0].vx = x >> 12;
            D_800F5F6C[i].pos.vx = x;
            D_800F5F6C[i].pos.vy = y;
            D_800F5F6C[i].trail_buf[0].vy = y >> 12;
            D_800F5F6C[i].pos.vz = z;
            D_800F5F6C[i].trail_buf[0].vz = z >> 12;
            D_800F5F6C[i].vel.vx = s->x;
            D_800F5F6C[i].vel.vy = s->y;
            D_800F5F6C[i].vel.vz = s->z;
            D_800F5F6C[i].rot.vx = D_800AE8D0.pitch;
            D_800F5F6C[i].rot.vy = D_800AE8D0.heading;
            D_800F5F6C[i].rot.vz = D_800AE8D0.x18;
            D_800F5F6C[i].x70 = D_800B0E5C.x1018 / 2 < D_800AE8D0.speed ? D_800AE8D0.speed : D_800B0E5C.x1018 / 2;
            D_800F5F6C[i].x74 = D_800B0E5C.x101C;
            D_800F5F6C[i].flags = 2;
            D_800AE8D0.tubes[tube] = 0;
            D_800F5F6C[i].target = s;
            D_800F5F6C[i].x88 = D_800AEA08;
            D_800F5F6C[i].trail[0] = &D_800F5F6C[i].trail_buf[0];
            D_800F5F6C[i].trail[1] = &D_800F5F6C[i].trail_buf[1];
            D_800F5F6C[i].trail[2] = &D_800F5F6C[i].trail_buf[2];
            D_800F5F6C[i].trail[3] = &D_800F5F6C[i].trail_buf[3];
            D_800F5F6C[i].trail[4] = &D_800F5F6C[i].trail_buf[4];
            D_800F5F6C[i].trail_buf[1] = D_800F5F6C[i].trail_buf[0];
            D_800F5F6C[i].trail_buf[2] = D_800F5F6C[i].trail_buf[0];
            D_800F5F6C[i].trail_buf[3] = D_800F5F6C[i].trail_buf[0];
            D_800F5F6C[i].trail_buf[4] = D_800F5F6C[i].trail_buf[0];
            snd_request.id = 0x2B;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0x1D2;
            snd_submit_request();
            return;
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_fire);
#endif


#ifdef NON_MATCHING
/* GCC here splits the output pointer in two. */
void submar_list_cells(int max)
{
    SVECTOR *p = D_801BCF78;
    Cell *c = D_800B5F4C;
    int i, n = 0;

    for (i = 0; i < 0x1000; i++, c++) {
        if (c->flags & 0x2000) {
            n++;
            p->vx = (i % 64) * 64 - 0x7E0;
            p->vz = 0x7E0 - i / 64 * 64;
            p->pad = i;
            p->vy = c->target - 0x20;
            p++;
            if (n > max) {
                break;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_list_cells);
#endif

INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800AB518);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800ABA8C);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800ABD4C);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800AC2FC);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800ACA40);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800ACCA8);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800ACF6C);
