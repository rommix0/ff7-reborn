/* MINI/CONDOR.BIN, Fort Condor: 0x800A037C-0x800BE7D8 (its data follows to 0x800BE810, then bss). One object: GCC
 * 2.6.3 -O2 -G0 (no -fforce-addr: globals are reloaded by %hi/%lo every time), ASPSX 2.21 with its divide checks. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "gte.h"

int rand(void);
int rsin(int a);
int rcos(int a);
void PushMatrix(void);
void PopMatrix(void);
void InitGeom(void);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
MATRIX *MulRotMatrix(MATRIX *m);
MATRIX *MulMatrix2(MATRIX *m0, MATRIX *m1);
VECTOR *RotTrans(SVECTOR *v0, VECTOR *v1, long *flag);
long RotTransPers(SVECTOR *v, long *sxy, long *p, long *flag);
long RotAverageNclip3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2, long *p, long *otz,
                      long *flag);
long RotAverageNclip4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1, long *sxy2,
                      long *sxy3, long *p, long *otz, long *flag);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void SetGeomOffset(int x, int y);
void SetGeomScreen(int h);
int VSync(int mode);
void PutDrawEnv(DRAWENV *env);
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
void SetDispMask(int mask);
void SetSprt16(void *p);
void SetPolyF3(void *p);
void SetPolyF4(void *p);
void SetPolyG3(void *p);
void SetPolyGT3(void *p);
void SetLineF4(void *p);
void bcopy(const void *src, void *dest, int n);
int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
void func_80043814(DRAWENV *env, int x, int y, int w, int h);           /* SetDefDrawEnv */
void func_80044000(RECT *r, void *p);                                   /* LoadImage */
void func_80044064(RECT *r, void *p);                                   /* StoreImage */
void func_80044244(unsigned int *ot, int n);                            /* ClearOTagR */
void func_8004433C(unsigned int *ot);                                   /* DrawOTag */
void func_800444AC(DISPENV *env);                                       /* PutDispEnv */
void func_800449FC(DR_OFFSET *p, short *ofs);                           /* SetDrawOffset */
void func_80044A68(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw); /* SetDrawMode */
unsigned int func_8001C808(void); /* the pad's buttons: held in the low half, pressed in the high */

/* libgpu's TIM description (layout from Sony's Libref). */
typedef struct TIM_IMAGE {
    unsigned int mode;
    RECT *crect;
    unsigned int *caddr;
    RECT *prect;
    unsigned int *paddr;
} TIM_IMAGE;
void OpenTIM(unsigned int *tim);
TIM_IMAGE *ReadTIM(TIM_IMAGE *t);

#define setUV0(p, _u0, _v0) ((p)->u0 = (_u0), (p)->v0 = (_v0))

/* libgte's inline macros, straight from the pointers' registers. */
#ifdef NATIVE
unsigned int gte_read_ctrl(int reg);
#define gte_ldv0_r(v) gte_ldv0(v)
#define gte_stlvnl_r(v) gte_stlvnl(v)
#define gte_stflg_r(p) (*(long *)(p) = gte_read_ctrl(31))
#define gte_SetRotMatrix_r(r) gte_SetRotMatrix(r)
#define gte_SetTransMatrix_r(r) gte_SetTransMatrix(r)
#define gte_ldv3(a, b, c)                                                                                              \
    (gte_write(0, ((unsigned int *)(a))[0]), gte_write(1, ((unsigned int *)(a))[1]),                                   \
     gte_write(2, ((unsigned int *)(b))[0]), gte_write(3, ((unsigned int *)(b))[1]),                                   \
     gte_write(4, ((unsigned int *)(c))[0]), gte_write(5, ((unsigned int *)(c))[1]))
#define gte_rtpt() gte_cmd(0x280030)
#define gte_nclip() gte_cmd(0x1400006)
#define gte_avsz3() gte_cmd(0x158002D)
#define gte_stopz(p) (*(long *)(p) = gte_read(24))
#define gte_stotz(p) (*(long *)(p) = gte_read(7))
#define gte_stsxy3(a, b, c)                                                                                            \
    (*(unsigned int *)(a) = gte_read(12), *(unsigned int *)(b) = gte_read(13), *(unsigned int *)(c) = gte_read(14))
#else
#define gte_ldv0_r(v) __asm__ volatile("lwc2 $0, 0(%0)\nlwc2 $1, 4(%0)" : : "r"(v))
#define gte_stlvnl_r(v) __asm__ volatile("swc2 $25, 0(%0)\nswc2 $26, 4(%0)\nswc2 $27, 8(%0)" : : "r"(v) : "memory")
#define gte_stflg_r(p) __asm__ volatile("cfc2 $12, $31\nnop\nsw $12, 0(%0)" : : "r"(p) : "$12", "memory")
#define gte_SetRotMatrix_r(r)                                                                                          \
    __asm__ volatile("lw $12, 0(%0)\nlw $13, 4(%0)\nctc2 $12, $0\nctc2 $13, $1\nlw $12, 8(%0)\nlw $13, 12(%0)\n"       \
                     "lw $14, 16(%0)\nctc2 $12, $2\nctc2 $13, $3\nctc2 $14, $4"                                        \
                     :                                                                                                 \
                     : "r"(r)                                                                                          \
                     : "$12", "$13", "$14")
#define gte_SetTransMatrix_r(r)                                                                                        \
    __asm__ volatile("lw $12, 20(%0)\nlw $13, 24(%0)\nctc2 $12, $5\nlw $14, 28(%0)\nctc2 $13, $6\nctc2 $14, $7"        \
                     :                                                                                                 \
                     : "r"(r)                                                                                          \
                     : "$12", "$13", "$14")
#define gte_ldv3(a, b, c)                                                                                              \
    __asm__ volatile("lwc2 $0, 0(%0)\nlwc2 $1, 4(%0)\nlwc2 $2, 0(%1)\nlwc2 $3, 4(%1)\nlwc2 $4, 0(%2)\nlwc2 $5, 4(%2)"  \
                     :                                                                                                 \
                     : "r"(a), "r"(b), "r"(c))
#define gte_rtpt() __asm__ volatile("nop\nnop\n.word 0x4A280030")
#define gte_nclip() __asm__ volatile("nop\nnop\n.word 0x4B400006")
#define gte_avsz3() __asm__ volatile("nop\nnop\n.word 0x4B58002D")
#define gte_stopz(p) __asm__ volatile("swc2 $24, 0(%0)" : : "r"(p) : "memory")
#define gte_stotz(p) __asm__ volatile("swc2 $7, 0(%0)" : : "r"(p) : "memory")
#define gte_stsxy3(a, b, c)                                                                                            \
    __asm__ volatile("swc2 $12, 0(%0)\nswc2 $13, 0(%1)\nswc2 $14, 0(%2)" : : "r"(a), "r"(b), "r"(c) : "memory")
#endif

/* Files the module loads to fixed places, reached through their addresses. */
#define CONDOR_DATA ((unsigned char *)0x80180000) /* the battle data: kinds, paths, scripts, stages, damage table */
#define CONDOR_SONGS ((int *)0x80188000)          /* the sound file: its songs' offsets, then the songs */
#define CAMERA ((Camera *)0x8018FF80)             /* the field's camera */
#define IMG_FRAME ((unsigned int *)0x80190040)    /* the texture image: the empty card frame at its row 0xE0 */
#define IMG_DIGITS ((unsigned int (*)[32])0x80198080) /* the 16-row digits, a row of 32 words */
#define FONT ((unsigned int (*)[32])0x80198880)   /* 8-row 4-bit glyphs, a row of 32 words; from 0x80, 8 rows down */
#define IMG_PORTRAITS ((unsigned int *)0x80199040) /* six per column of 16 rows, 8 words wide */
#define PICTURES ((unsigned int *)0x801A8040)     /* the unit pictures, 64 halfwords by 32 rows each */
#define MODELS ((unsigned char *)0x801B0000)      /* the units' models: per kind a table of its frames' offsets */
#define SCRATCH ((Scratch *)0x1F800000)

/* The battle data's halfword at OFS: the offsets of its tables (0 the kinds' 32-byte records, 4 the paths, 6 the
 * scripts, 8 the player's starting units, 0xA the stages' waves, 0x18 the damage percents, 0x1C a heal), and the
 * scripts' steps. */
#define DATA_HALF(ofs) (*(unsigned short *)(CONDOR_DATA + (ofs)))

/* The camera as the field file stores it. */
typedef struct Camera {
    MATRIX m;
    int pad;
    short h; /* screen distance */
} Camera;

/* The scratchpad the model drawer works in. */
typedef struct Scratch {
    unsigned int *ot;
    long opz;
    long otz;
} Scratch;

/* A unit (0x78 bytes): 0-19 the player's, 20-39 the enemy's, 40-68 shots and blasts, 67-69 the fort and the two
 * beside it. The scene's models are drawn at scene, turned by rot and scaled by scale; pos is its place on the
 * field. */
typedef struct Unit {
    unsigned short flags; /* 0: a free slot; 4 engaged, 8 walking, 0x10 timed (or hit this blow), 0x20 off its route,
                           * 0x40 being healed */
    unsigned char mode;   /* what it is doing: 0 idle, 1 seek, 2 fight, 3 chase, 4 hit, 5 heal, 0xA dying */
    unsigned char order;  /* the order given to it */
    unsigned char act;    /* 1 moving, 2 acting, 4 healing, 0x80 recoiling */
    unsigned char fade;   /* its models' red while it dies; 0xFF once dead and counted; not a target while set */
    unsigned short kind;  /* its entry in the data, model and script files; below 0x10 the player's */
    unsigned short slot;  /* its model buffer */
    unsigned short cell;  /* the field cell it stands in, -1 none */
    unsigned short x0C;
    unsigned short fall; /* 8.8, added to scene.vy while it falls */
    unsigned char hp, max_hp;
    unsigned char power;
    unsigned char role; /* 1 soldier, 2 archer, 3 and 4 towers, 5 healer, 6 bomber, 7 attacker */
    unsigned char reach;
    unsigned char x15;
    unsigned char sight;
    unsigned char x17;
    unsigned short act_wait; /* frames to its next step */
    unsigned short rate;
    unsigned short timer; /* while flags has 0x10 */
    unsigned short cooldown;
    unsigned char ai;
    unsigned char number; /* on its card */
    unsigned char w, h;   /* its box: x +-w/2, y up to h above */
    unsigned short path;  /* path << 8 | point */
    unsigned short tx, ty; /* where it walks to, a shot's target; 0, 0 when it stands */
    unsigned char pose;    /* which of its kind's scripts runs */
    unsigned char frame;   /* its model's frame; a shot's stage */
    unsigned char hold;    /* frames left on it */
    unsigned char phase;   /* of its fight */
    unsigned char damage;  /* the hit it is taking */
    unsigned char x2F;
    unsigned short script, script_pos; /* the running script and the step in it, byte offsets into the data */
    unsigned short dir;
    unsigned short heading; /* drawn; follows dir */
    unsigned short flash;   /* 0x200: fading back */
    unsigned char sub_x, sub_y; /* 1/256 position fractions */
    unsigned char act_time, rest_time;
    unsigned short wait;
    unsigned char target, foe; /* units, 0xFF none */
    unsigned char clear;       /* the directions it last slid along */
    unsigned char type;        /* its row and column in the damage table */
    int link;                  /* a unit it belongs to, 0xFF none */
    SVECTOR pos;
    SVECTOR scene;
    SVECTOR vel; /* 8.8 steps */
    SVECTOR rot;
    VECTOR scale;
} Unit;

/* A triangular cell of the field (0x4C bytes): its corners, its neighbours, the corners on screen and their box, and
 * the turn it gives a unit standing on it. */
typedef struct Cell {
    SVECTOR v[4];   /* v[0] is not projected */
    short next[3];  /* the cells across its sides, -1 for none (the file's colour word, byte-swapped) */
    short pad26;
    SVECTOR scr[3]; /* v[1..3] on screen */
    short x0, x1, y0, y1; /* their box, biased by 0x4000 */
    short rot_x, rot_z;
} Cell;

/* A unit found by a search and its number. */
typedef struct Found {
    Unit *unit;
    short idx;
} Found;

/* A unit of the unit list (0x2C bytes): where its picture slides from, to and back. */
typedef struct Pick {
    SVECTOR pos;
    SVECTOR home;
    SVECTOR to;
    Unit *unit;
    short x1C;
    short index;
    short x20, x22;
    unsigned short step_x, step_y; /* 1/256ths a frame */
    unsigned char frac_x, frac_y;
    unsigned char pad2A[2];
} Pick;

/* A number floating over a unit (a hit or a heal): its age (0 unused; bit 15 the second palette), value, start. */
typedef struct Popup {
    short age;
    short value;
    short x, y;
} Popup;

/* An explosion or a burst of fire (16 bytes, 8 of them): where, its animation step (0xFF: free), the hit it deals. */
typedef struct Blast {
    short x, y, z;
    short pad6;
    unsigned char step;
    unsigned char timer; /* frames left on this step */
    unsigned char image; /* its cell of the 4x2 sheet */
    unsigned char power;
    unsigned char type; /* 7: a burst of fire (condor_draw_burst), else an explosion (condor_update_blast) */
    unsigned char padD[3];
} Blast;

/* A step of an explosion: how it blends, how many frames it lasts, which image it shows. */
typedef struct BlastStep {
    short mode, frames, image;
} BlastStep;

/* A unit waiting in a stage's wave (8 bytes): its kind, where it comes in (16-pixel cells), when. */
typedef struct Spawn {
    unsigned char kind, x, y, x03;
    unsigned short time;
    unsigned char x06, x07;
} Spawn;

/* A player's unit kept between battles (8 bytes); the list ends with x -1. */
typedef struct Kept {
    short x, y;
    unsigned short kind;
    short dir;
} Kept;

/* A 32-byte record of the battle data: a kind's score and the numbers on its card. */
typedef struct Record {
    unsigned short score;
    unsigned char x02; /* shown in the card's first row */
    unsigned char x03[2];
    unsigned char x05; /* its second row */
    unsigned char x06;
    unsigned char x07; /* its third row */
    unsigned char x08[0x18];
} Record;

/* A wall as the field file stores it: corners with y and z the other way round, then its colour byte-swapped. */
typedef struct WallRec {
    short p[4][3];
    unsigned short rgb[3];
    unsigned char pad[2];
} WallRec;

/* A face of a model in MODELS (12 bytes): its corners' colours, then their vertices. */
typedef struct ModelFace {
    unsigned char rgb[3][3];
    unsigned char v[3];
} ModelFace;

/* A face of a unit's model as drawn (0x50 bytes): a primitive for each display buffer and its corners. */
typedef struct UnitFace {
    POLY_G3 prim[2];
    SVECTOR v[3];
} UnitFace;

/* A unit's built model (0x1044 bytes). */
typedef struct UnitModel {
    int count;
    UnitFace faces[52];
} UnitModel;

/* A unit's model queued to be recoloured (12 bytes); count's bit 15 asks for the colours' blue-grey version. */
typedef struct ShadeJob {
    UnitFace *faces;
    ModelFace *src;
    short count;
} ShadeJob;

/* A face of a tower's model as drawn, lit (0x68 bytes). */
typedef struct TowerFace {
    POLY_GT3 prim[2];
    SVECTOR v[3];
} TowerFace;

/* A tower's built model (0x1524 bytes). */
typedef struct TowerModel {
    int count;
    TowerFace faces[52];
} TowerModel;

/* A tower's model queued to be drawn this frame. */
typedef struct TowerJob {
    TowerFace *faces;
    short count;
} TowerJob;

/* A flat quad drawn after its draw mode: one primitive per frame, its corners. */
typedef struct Quad {
    DR_MODE mode;
    POLY_F4 poly[2];
    SVECTOR v[4];
} Quad;

/* A draw-offset change and the offset it sets. */
typedef struct DrawOffset {
    DR_OFFSET dr;
    short ofs[2];
} DrawOffset;

/* One of the two display buffers (0x69BC bytes), built and shown in turn. */
typedef struct DrawBuf {
    DRAWENV draw;
    DISPENV disp;
    unsigned int ot[0x1000]; /* reverse-cleared, drawn from ot[0xFFF] */
    SPRT_16 sprt[300];       /* the sprite blocks' cells */
    SPRT_16 map[294];        /* the battlefield, 14 rows of 21; their colour does the fades */
    DR_MODE mode[8];         /* the map's texture pages */
    DR_MODE line_mode;
    DR_MODE x65FC;
    DrawOffset offs[6];
    unsigned char x6668[0x120];
    Quad shade;      /* a subtractive grey quad */
    POLY_F3 mark[4]; /* the chosen cell and its three neighbours */
    POLY_FT4 blast[8];
    POLY_FT4 panel; /* the map panel */
    LINE_F2 line;   /* the fade line */
    unsigned char x69AC[0x10];
} DrawBuf;

/* A ground primitive: a flat quad and a word after it. */
typedef struct Ground {
    POLY_F4 poly;
    int x18;
} Ground;

/* A block of w by h 16x16 sprites from the buffers' pool, drawn as one picture at x, y less the camera (0x1C bytes). */
typedef struct SpriteBlock {
    unsigned short first; /* its first sprite in DrawBuf.sprt */
    unsigned short w, h;  /* in cells */
    unsigned short x, y;
    short x0A;
    short *move; /* when set, added to x, y each draw */
    DR_MODE mode;
} SpriteBlock;

extern unsigned char D_8009C6E4[]; /* the save's data */
extern unsigned char D_8009D331, D_8009D33B;
extern unsigned int save_gil;
extern DiscFile D_800BE7D8[7]; /* the module's own files */
extern Kept D_800BE814[21];
extern Spawn *D_800BE8BC; /* the record a unit is brought in from */
extern Unit D_800BE8C0[70];
extern UnitModel D_800C0990[70];
extern TowerModel D_80107C28[3];
/* The popups' blocks 59-74 run on over D_8010C33C's first cells: the game's array is 70 long. */
extern SpriteBlock D_8010BB94[];
extern Cell D_8010C33C[];
extern Ground D_80115B3C[2][512];
extern Blast D_8011CB3C[8];
extern DrawBuf D_8011CBBC[2];
extern DrawBuf *D_80129F34;            /* the one being built */
extern short D_80129F38;               /* the next free sprite of the pool */
extern unsigned int D_80129F3C[0x200]; /* an image being built for VRAM: the hp numbers, the unit card's text */
extern unsigned int D_8012A73C[0x200]; /* the counters' image, 10 words by 32 rows */
extern RECT D_8012AF3C, D_8012AF44, D_8012AF4C;
extern short D_8012AF54; /* counts the two-digit counters' changes */
extern short D_8012AF58;
extern unsigned int D_8012AF5C[8][32]; /* the two-digit counters' image */
extern Found D_8012B75C[32]; /* a search's finds */
extern short D_8012B85C;
extern Found D_8012B860[32]; /* the hurt ones of a search */
extern short D_8012B960;
extern Found D_8012B964[32]; /* the ones of the wanted type */
extern short D_8012BA64;
extern unsigned char D_8012BA68[32]; /* the next number for each kind */
extern Pick D_8012BA88[32];
extern short D_8012C008, D_8012C00C, D_8012C010, D_8012C014;
extern unsigned short D_8012C018;
extern ShadeJob D_8012C01C[70];
extern int D_8012C364;
extern TowerJob D_8012C368[4];
extern int D_8012C388; /* the ground height (its low half read for the z's) */
extern unsigned int D_8012C390, D_8012C394, D_8012C398, D_8012C39C; /* the pad: held, last frame's, with repeat,
                                                                      * pressed */
extern int D_8012C3A0;       /* the d-pad bits the cursor walk fakes */
extern short D_8012C3A4[2];  /* the cursor */
extern short D_8012C3AC[2];  /* the target cursor, where a unit is sent */
extern short D_8012C3BC[2];
extern short D_8012C3C4[2];  /* the cursor before a snap */
extern int D_8012C3CC, D_8012C3D0, D_8012C3D4, D_8012C3D8, D_8012C3DC, D_8012C3E4;
extern int D_8012C3E8; /* 1 picking, 2 the menu, 3 targeting */
extern int D_8012C3EC; /* the command mode */
extern int D_8012C3F0; /* the buffer being built (0/1) */
extern int D_8012C3F8;
extern int D_8012C3FC; /* the chosen cell, or -1 */
extern int D_8012C400;
extern int D_8012C404; /* 0: a new battle, 2: one carried on; 1 placing */
extern int D_8012C408; /* the battle's state; 4 won */
extern short D_8012C40C; /* the stage */
extern short D_8012C410, D_8012C414, D_8012C418;
extern short D_8012C41C; /* the selected unit, -1 none */
extern short D_8012C420, D_8012C424, D_8012C428, D_8012C42C;
extern short D_8012C430;                 /* digits given */
extern unsigned short D_8012C434, D_8012C438; /* the camera's x, y */
extern unsigned short D_8012C43C, D_8012C440, D_8012C444, D_8012C448;
extern short D_8012C44C, D_8012C450;
extern unsigned short D_8012C454; /* the cursor's speed-up */
extern short D_8012C458, D_8012C45C;
extern unsigned short D_8012C460;
extern short D_8012C464;
extern short D_8012C468; /* the shop's turn, 0-0xFFF; the aim */
extern short D_8012C46C; /* its spin */
extern short D_8012C470; /* the item in front */
extern short D_8012C474;
extern short D_8012C478; /* the fort's level (1-3), 0 for none */
extern short D_8012C47C, D_8012C480, D_8012C484, D_8012C488, D_8012C48C; /* the map fade */
extern short D_8012C490; /* a unit is dying */
extern short D_8012C494; /* which */
extern short D_8012C498, D_8012C49C, D_8012C4A0;
extern short D_8012C4AC, D_8012C4B0, D_8012C4B4, D_8012C4B8, D_8012C4BC, D_8012C4C0, D_8012C4C4;
extern short D_8012C4C8; /* set: units are drawn grey-blue */
extern unsigned short D_8012C4CC, D_8012C4D0;
extern short D_8012C4D4, D_8012C4D8, D_8012C4DC, D_8012C4E0, D_8012C4E4, D_8012C4E8;
extern unsigned short D_8012C4EC;
extern short D_8012C4F0, D_8012C4F4, D_8012C4F8, D_8012C4FC;
extern short D_8012C500; /* the shop's items */
extern short D_8012C504; /* the turn between two items */
extern int D_8012C508;   /* the player's units */
extern int D_8012C50C;   /* the enemy's */
extern int D_8012C510;   /* the next enemy of the wave */
extern int D_8012C514;
extern int D_8012C518; /* how many cells */
extern int D_8012C51C, D_8012C524, D_8012C528, D_8012C52C, D_8012C530;
extern unsigned int D_8012C534, D_8012C538; /* a unit's price, the money */
extern unsigned int D_8012C53C;             /* the number being cut into digits */
extern unsigned int D_8012C540;             /* the deepest the scenery reached */
extern int D_8012C544, D_8012C548; /* the step toward an aim point (0x100 on the long axis) or a turned step */
extern int D_8012C54C, D_8012C550; /* the cursor walk's copy of it */
extern int D_8012C554, D_8012C558; /* |dx|, |dy| */
extern int D_8012C55C, D_8012C560; /* the walk's 1/256 accumulators */
extern int D_8012C564, D_8012C568; /* dx, dy were negative */
extern unsigned char D_8012C56C, D_8012C570, D_8012C574, D_8012C578; /* the menu's shape */
extern unsigned char D_8012C57C;    /* the order given */
extern unsigned char D_8012C580[];  /* the shop's items' kinds */
/* A unit's place noted while condor_find_detour tries detours: sub_x, sub_y, pos and scene. */
extern unsigned char D_8012C58C, D_8012C590;
extern SVECTOR D_8012C594, D_8012C59C;
extern short D_8012C5A4, D_8012C5A8, D_8012C5AC;
extern int D_8012C5B0; /* the stage's clock, in 60ths of a second */
extern unsigned short D_8012C5B4, D_8012C5B8, D_8012C5BC, D_8012C5C0, D_8012C5C4, D_8012C5C8;
extern short D_8012C5CC, D_8012C5D0, D_8012C5D4;
extern unsigned short D_8012C5DC;
extern short D_8012C5E0, D_8012C5E4;
extern Popup D_8012C5FC[16];
extern MATRIX D_8012C67C; /* the view */
extern MATRIX D_8012C69C;
extern SVECTOR D_8012C6DC, D_8012C6E4;
extern VECTOR D_8012C6EC; /* the view's offset */
extern int D_8012C6FC, D_8012C700, D_8012C704;
extern short D_8012C70C, D_8012C70E, D_8012C710;
extern int D_8012C714, D_8012C718, D_8012C71C;
extern SVECTOR D_8012C724, D_8012C72C;
extern VECTOR D_8012C734, D_8012C744;
extern SVECTOR D_8012C754;
extern MATRIX *D_8012C75C;
extern unsigned char D_8012C760[24]; /* by kind: how far its model drops */
/* The offsets of the parts of the file loaded at 0x801B0000: the game reads the first by its symbol, the file
 * itself through the constant (MODELS). */
extern unsigned int D_801B0000[12];

void condor_draw_quad(unsigned int *ot, int which);
void condor_draw_units(unsigned int *ot, int frame);
void condor_draw_fort_screen(unsigned int *ot, int frame);
void condor_draw_fade_line(void);
void condor_draw_ground(unsigned int *ot, int frame);
short condor_find_units_at(short *pos);
short condor_find_touching_units(Unit *u, short first, short n);
short condor_find_units_in_reach(Unit *u, short first, short n);
void condor_count_units_in_reach(Unit *u, short first, short n);
short condor_unit_overlaps(Unit *u, short i);
void condor_update_popups(void);
void condor_draw_counter_digits(int slot, int value);
void condor_init_block(unsigned short g, unsigned short w, unsigned short h, unsigned short u, unsigned short v,
                       unsigned short tp, unsigned short semi, unsigned short tx, unsigned short ty, unsigned short cx,
                       unsigned short cy, short *move);
void condor_put_block(unsigned short g, unsigned short x, short y);
void condor_set_block_row(unsigned short g, unsigned short row);
void condor_load_two_numbers(void);
void condor_load_unit_card(); /* called with one argument and with two: the second is whatever is in a1 */
unsigned char condor_next_digit(unsigned int place);
void condor_load_score_counters(Record *rec);
void condor_load_unit_info(short kind);
void condor_draw_hp(void);
int condor_draw_background(unsigned int *ot);
void condor_fade_out_start(void);
void condor_fade_step_queued(void);
void condor_update_colours(void);
void condor_play_sfx(short pan, int effect);
void condor_run_unit_list(void);
int condor_reload_map_image(void);
void condor_free_scroll(void);
void condor_yes_no(void);
int condor_mode_move_cursor(void);
int condor_mode_walk_cursor(void);
int condor_opening(void);
void condor_play_death_cry(Unit *u);
short condor_fight_step(int a, int b);
void condor_fight_end(int a, int b);
void condor_cursor_walk_step(void);
void condor_aim(int x0, int y0, int x1, int y1);
short condor_square_angle(SVECTOR *v);
void condor_angle_to_square(unsigned short angle);
void condor_update_command(void);
short condor_angle_between(SVECTOR *a, SVECTOR *b);
short condor_outside_cell(Cell *t, short x, short y);
short condor_find_cell(Unit *u);
short condor_update_cell(Unit *u);
short condor_turn_at_edge(Unit *u);
short condor_clear_dirs(Unit *a, Unit *b, short want);
short condor_slide_unit(Unit *u);
void condor_open_unit_list(void);
int condor_open_unit_menu(void);
int condor_check_placement(void);
void condor_clear_selection(void);
void condor_switch_screen(void);
int condor_read_pad(MATRIX *m, MATRIX *unused);
void condor_menu_layout(void);
void condor_move_cursor(int btn);
void condor_move_point(int btn, short *pos);
void condor_scroll_camera(int buttons);
void condor_build_tower_model(Unit *u, short i);
void condor_build_unit_model(Unit *u);
void condor_next_waypoint(Unit *u);
int condor_start_script(Unit *u);
short condor_place_is_free(short kind, unsigned short *pos);
short condor_ground_is_free(short kind, unsigned short *pos);
int condor_init_unit(Unit *u, Spawn *s, int slot, unsigned short *pos);
void condor_place_player_units(void);
void condor_spawn_enemy(void);
void condor_do_unit_order(void);
void condor_order_move(void);
void condor_walk_to_target(Unit *u);
void condor_give_order(int i);
int condor_open_shop(void);
int condor_buy_unit(int kind);
void condor_shop_update(void);
void condor_aim_tower(void);
void condor_aim_unit(void);
void condor_drop_selected(void);
void condor_take_hit(Unit *u);
int condor_area_attack(Unit *u);
int condor_attack(Unit *u);
void condor_update_blast(POLY_FT4 *p, Blast *b);
void condor_draw_burst(POLY_FT4 *poly, Blast *b);
int condor_start_burst(Unit *o);
int condor_step_shot(Unit *o);
void condor_update_shots(void);
void condor_turn_step(int angle, short len);
void condor_hit_by(Unit *by, Unit *o);
short condor_fire_shot(Unit *src, int n);
void condor_place_fort(void);
void condor_spawn_fort(void);
int condor_spawn_at(Unit *src, int kind);
void condor_init_prims(DrawBuf *g, int a1, int a2, short db);
void condor_cell_box(Cell *t);
void condor_build_cells(unsigned char *src, Cell *dest);
void condor_init_field(int x, int y, int unused);
void condor_load_data(void);
int condor_load_tim(unsigned int off, int x, int y, int cx, int cy);
int condor_load_background(int x, int y, int cx, int cy);
void condor_read_file(int i, int room, void *dest);
void condor_read_file_lzs(int i, int room, void *dest);
short condor_role_walks(Unit *o);
void condor_tint_model_red(Unit *o);
void condor_animate_troops(void);
int condor_update_troops(void);
void condor_tick_timer(Unit *u);
void condor_heal_unit(Unit *u);
void condor_step_unit(Unit *u, int i);
void condor_ranged_fight(Unit *u, int i);
void condor_run_anim_script(Unit *u);
void condor_face_target(Unit *u);
void condor_update_unit(Unit *u, int i);
short condor_find_detour(Unit *u);
void condor_step_or_detour(Unit *u);
void condor_after_step(Unit *u);
void condor_move_unit(Unit *u);
void condor_apply_velocity(Unit *u);
void condor_bump_foe(Unit *u, int i);
short condor_pick_target(Unit *u);
short condor_update_attacker(Unit *u, int i);
void condor_update_tower(Unit *u, int i);
void condor_set_pose(Unit *u);
void condor_update_soldier(Unit *u, int i);
short condor_fire_at(Unit *u, int i);
short condor_shoot_target(Unit *u);
short condor_shoot_any(Unit *u);
short condor_update_shooter(Unit *u);
void condor_update_archer(Unit *u, int i);
void condor_heal_target(Unit *u);
void condor_update_healer(Unit *u, int i);
void condor_update_bomber(Unit *u, int i);
void condor_main(void);
void condor_end_round(short how);
void condor_init_battle(void);
void condor_read_done(void);
void condor_stop_sounds(void);
void condor_play_end_music(void);
void condor_flip_buffers(void);
void condor_update_frame(void);
void condor_draw_unit_panel(short x, short y);
int condor_draw_result_rows(void);
void condor_draw_frame(void);
void condor_draw_banner(short mode);
void condor_draw_block_at_y(short i, short y);
void condor_draw_block_at(short i, short x, short y);
void condor_draw_flip_pair(int y);
void condor_draw_hud(void);
void condor_init_blocks(void);
void condor_update_chosen_cell(void);

/* Sets the GTE to unit U's place: its turn and position in the scene (M1), then its heading and scale on top (M2).
 * TURN is a zeroed vector whose y holds the heading; ZERO a zero vector. */
#define SET_UNIT_MATRIX(u, m1, m2, turn, zero, flag)                                                                   \
    {                                                                                                                  \
        RotMatrix(&(u)->rot, m1);                                                                                      \
        gte_ldv0_r(&(u)->scene);                                                                                       \
        gte_rt();                                                                                                      \
        gte_stlvnl_r((m1)->t);                                                                                         \
        gte_stflg_r(flag);                                                                                             \
        gte_SetRotMatrix_r(m1);                                                                                        \
        gte_SetTransMatrix_r(m1);                                                                                      \
        RotMatrix(turn, m2);                                                                                           \
        MulRotMatrix(m2);                                                                                              \
        ScaleMatrix(m2, &(u)->scale);                                                                                  \
        gte_ldv0_r(zero);                                                                                              \
        gte_rt();                                                                                                      \
        gte_stlvnl_r((m2)->t);                                                                                         \
        gte_stflg_r(flag);                                                                                             \
        gte_SetRotMatrix_r(m2);                                                                                        \
        gte_SetTransMatrix_r(m2);                                                                                      \
    }

/* Projects unit model MDL's faces that face the camera into OT as frame FRAME's primitives. */
#define DRAW_MODEL(mdl, ot, frame)                                                                                     \
    {                                                                                                                  \
        UnitFace *_t = (mdl)->faces;                                                                                   \
        int _n, _i;                                                                                                    \
        SCRATCH->ot = ot;                                                                                              \
        _n = (mdl)->count;                                                                                             \
        for (_i = 0; _i < _n; _i++, _t++) {                                                                            \
            gte_ldv3(&_t->v[0], &_t->v[1], &_t->v[2]);                                                                 \
            gte_rtpt();                                                                                                \
            gte_nclip();                                                                                               \
            gte_stopz(&SCRATCH->opz);                                                                                  \
            if (SCRATCH->opz > 0) {                                                                                    \
                gte_stsxy3(&_t->prim[frame].x0, &_t->prim[frame].x1, &_t->prim[frame].x2);                             \
                gte_avsz3();                                                                                           \
                gte_stotz(&SCRATCH->otz);                                                                              \
                addPrim(SCRATCH->ot + SCRATCH->otz, &_t->prim[frame]);                                                 \
            }                                                                                                          \
        }                                                                                                              \
    }

/* Projects tower model MDL into OT as frame FRAME's primitives. */
#define DRAW_TOWER(mdl, ot, frame)                                                                                     \
    {                                                                                                                  \
        TowerFace *_t = (mdl)->faces;                                                                                  \
        int _n = (mdl)->count;                                                                                         \
        int _i;                                                                                                        \
        for (_i = 0; _i < _n; _i++, _t++) {                                                                            \
            if (RotAverageNclip3(&_t->v[0], &_t->v[1], &_t->v[2], (long *)&_t->prim[frame].x0,                         \
                                 (long *)&_t->prim[frame].x1, (long *)&_t->prim[frame].x2, &p, &otz, &flag2) > 0)      \
                AddPrim(ot + otz, &_t->prim[frame]);                                                                   \
        }                                                                                                              \
    }

/* Copies digit D (8 rows) into column COL of the two-digit image. */
static inline void put_digit(int col, int d)
{
    int i;

    for (i = 0; i < 8; i++)
        D_80129F3C[i * 2 + col] = FONT[i][d];
}

/* Copies glyph CH (8 rows, W words apart) into the counters' image at word AT, unless it is blank (0xFF). W must be a
 * char: an int doesn't match. */
static inline void put_glyph(unsigned char at, unsigned char ch, unsigned char w)
{
    int r, c;

    if (ch == 0xFF)
        return;
    c = ch;
    if (c >= 0x80)
        c = (c & 0x7F) + 0x100;
    for (r = 0; r < 8; r++)
        D_8012A73C[at + r * w] = FONT[r][c];
}

/* The same into the unit card's text image, glyphs from 0x80 at +0x80. */
static inline void put_card_glyph(unsigned char at, unsigned char ch, unsigned char w)
{
    int r, c;

    if (ch == 0xFF)
        return;
    c = ch;
    if (c >= 0x80)
        c = (c & 0x7F) + 0x80;
    for (r = 0; r < 8; r++)
        D_80129F3C[at + r * w] = FONT[r][c];
}

/* A sound command: id, then its two arguments (a pan of 0x40 is the middle). */
#define CONDOR_SFX(cmd, pan, sfx)                                                                                      \
    (snd_request.id = (cmd), snd_request.arg[0] = (pan), snd_request.arg[1] = (sfx), snd_submit_request())

/* Both buffers have reached the fade's end: go on to the queued state. */
#define FADE_BUFFER_DONE()                                                                                             \
    do {                                                                                                               \
        if (!D_8012C3F0)                                                                                               \
            D_8012C480 = 0;                                                                                            \
        else                                                                                                           \
            D_8012C484 = 0;                                                                                            \
        if (!(D_8012C480 | D_8012C484)) {                                                                              \
            D_8012C488 = D_8012C47C;                                                                                   \
            D_8012C47C = 0;                                                                                            \
        }                                                                                                              \
    } while (0)

/* One band of 16x16 background sprites: rows from vstart below vend, columns from ustart below uend. */
#define CONDOR_BAND(vstart, ystart, ustart, xstart, uend)                                                              \
    do {                                                                                                               \
        for (v = vstart, y = ystart; v < vend; v += 16, y += 16) {                                                     \
            vv = v;                                                                                                    \
            if (v == 0x1F0)                                                                                            \
                vv = 0x1EF;                                                                                            \
            for (u = ustart, x = xstart; u < uend; u += 16, x += 16, p++) {                                            \
                p->x0 = x;                                                                                             \
                p->u0 = u;                                                                                             \
                p->v0 = vv;                                                                                            \
                p->y0 = y;                                                                                             \
                addPrim(ot + 0xFFF, p);                                                                                \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

/* Moves pos a step's worth (step/256, sign in the top byte) toward to, keeping the fraction. */
#define CONDOR_SLIDE(pos, to, step, frac)                                                                              \
    do {                                                                                                               \
        if (pos == to)                                                                                                 \
            step = 0;                                                                                                  \
        else {                                                                                                         \
            frac = s = step + frac;                                                                                    \
            d = s >> 8;                                                                                                \
            if (d) {                                                                                                   \
                if (d >= 0x80)                                                                                         \
                    d |= 0xFF00;                                                                                       \
                pos += d;                                                                                              \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

/* Slides every picked unit 8 steps; how many are still moving. */
#define CONDOR_SLIDE_ALL()                                                                                             \
    do {                                                                                                               \
        e = D_8012BA88;                                                                                                \
        left = D_8012C008;                                                                                             \
        for (i = 0; i < D_8012C008; i++, e++) {                                                                        \
            if (!(e->step_x | e->step_y)) {                                                                            \
                left--;                                                                                                \
                continue;                                                                                              \
            }                                                                                                          \
            for (k = 0; k < 8; k++) {                                                                                  \
                CONDOR_SLIDE(e->pos.vx, e->to.vx, e->step_x, e->frac_x);                                               \
                CONDOR_SLIDE(e->pos.vy, e->to.vy, e->step_y, e->frac_y);                                               \
            }                                                                                                          \
            u = e->unit;                                                                                               \
            u->scene.vx = e->pos.vx;                                                                                   \
            u->scene.vy = e->pos.vy;                                                                                   \
        }                                                                                                              \
    } while (0)

/* Fills the sound request that starts song k of the sound file (its offsets rounded up to a word). */
#define condor_song_request(k)                                                                                         \
    {                                                                                                                  \
        int ofs = CONDOR_SONGS[k];                                                                                     \
        if (ofs & 3) {                                                                                                 \
            ofs &= ~3;                                                                                                 \
            ofs += 4;                                                                                                  \
        }                                                                                                              \
        snd_request.id = 0x10;                                                                                         \
        snd_request.arg[0] = ofs + (int)CONDOR_SONGS;                                                                  \
    }

/* Back to the map, the selected unit let go. */
#define DESELECT()                                                                                                     \
    {                                                                                                                  \
        D_8012C3E8 = 1;                                                                                                \
        D_8012C480 = D_8012C484 = D_8012C47C = 1;                                                                      \
        D_8012C41C = -1;                                                                                               \
        D_8012C428 = 1;                                                                                                \
    }

/* A free popup, or 0xFF. */
static inline short free_popup(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (!D_8012C5FC[i].age)
            return i;
    }
    return 0xFF;
}

#define MIN3(a, b, c) ((a) > (b) ? ((b) > (c) ? (c) : (b)) : ((a) > (c) ? (c) : (a)))
#define MAX3(a, b, c) ((a) < (b) ? ((b) < (c) ? (c) : (b)) : ((a) < (c) ? (c) : (a)))
/* The game tests bit 15 here and takes the value either way (a dead andi in the asm). */
#define SX(v) ((v) & 0x8000 ? (v) : (v))

/* Whether a falling shot has hit the ground or reached its target spot. */
static inline short condor_landed(Unit *o)
{
    short r = 0;

    if (o->ty < o->scene.vy + 0x200) {
        r = 1;
    } else {
        if (o->tx == o->pos.vx)
            o->vel.vx = 0;
        if (o->ty == o->pos.vy)
            o->vel.vy = 0;
        if (!(o->vel.vx | o->vel.vy))
            r = 1;
    }
    return r;
}

/* Whether angle a lies outside the arc from b to c (the shorter way round). */
static inline int condor_outside_arc(short a, short b, short c)
{
    short hi, lo;

    if (b >= c) {
        hi = b;
        lo = c;
    } else {
        hi = c;
        lo = b;
    }
    if (hi - lo < 0x800) {
        if (hi < a || a < lo)
            return 1;
    } else if (a < hi && lo < a) {
        return 1;
    }
    return 0;
}

/* Cell i if (x, y) is inside it, else -1. */
static inline short condor_on_cell(short i, short x, short y)
{
    short bx = x + 0x4000;
    short by = y + 0x4000;
    Cell *t = &D_8010C33C[i];

    if (bx < t->x0 || t->x1 < bx || by < t->y0 || t->y1 < by)
        return -1;
    if (condor_outside_cell(t, x, y))
        return -1;
    return i;
}

/* Whether spans ending at a and b, a reaching back sa and b back sb, overlap. */
static inline int condor_overlap(short a, short b, int sa, int sb)
{
    return a >= b ? a - sa < b : b - sb < a;
}

/* Draws a frame with every map tile on the CLUT at row clut_y, at brightness shade. */
static inline void condor_draw_shaded(int clut_y, int shade)
{
    DrawBuf *f = D_80129F34;
    unsigned short clut = GetClut(0, clut_y);
    SPRT_16 *s;
    int i, j;

    for (s = f->map, i = 0; i < 14; i++) {
        for (j = 0; j < 21; j++) {
            s->clut = clut;
            s->r0 = s->g0 = s->b0 = shade;
            s++;
        }
    }
    condor_flip_buffers();
    condor_draw_frame();
}

/* Starts the cursor flying from where it is to (tx, ty). */
static inline void condor_fly_cursor(int fx, int fy, int tx, int ty)
{
    condor_aim(fx, fy, tx, ty);
    D_8012C4C0 = 1;
    D_8012C4C4 = 0x10;
    D_8012C54C = D_8012C544;
    D_8012C550 = D_8012C548;
}

/* Moves the cursor with the d-pad, faster the longer it is held. */
static inline void condor_cursor_repeat(void)
{
    D_8012C454++;
    if (D_8012C454 > 16)
        D_8012C454 = 16;
    condor_move_cursor(D_8012C390);
    if (D_8012C454 >= 3) {
        condor_move_cursor(D_8012C390);
        if (D_8012C454 >= 4) {
            condor_move_cursor(D_8012C390);
            condor_move_cursor(D_8012C390);
        }
    }
}

/* Puts the cursor on the selected unit, scrolling the view towards it the way the d-pad would. */
static inline void condor_snap_cursor(void)
{
    int btn = (D_800BE8C0[D_8012C41C].pos.vx < D_8012C3C4[0]) << 15;

    if (D_8012C3C4[0] < D_800BE8C0[D_8012C41C].pos.vx)
        btn |= 0x2000;
    if (D_800BE8C0[D_8012C41C].pos.vy < D_8012C3C4[1])
        btn |= 0x1000;
    if (D_8012C3C4[1] < D_800BE8C0[D_8012C41C].pos.vy)
        btn |= 0x4000;
    D_8012C3C4[0] = D_8012C3A4[0];
    D_8012C3C4[1] = D_8012C3A4[1];
    if (btn)
        condor_move_cursor(btn);
    D_8012C3A4[0] = D_800BE8C0[D_8012C41C].pos.vx;
    D_8012C3A4[1] = D_800BE8C0[D_8012C41C].pos.vy;
}

/* Puts the unit back where condor_update_unit noted it. */
static inline void condor_restore_place(Unit *u)
{
    u->sub_x = D_8012C58C;
    u->sub_y = D_8012C590;
    u->pos.vx = D_8012C594.vx;
    u->pos.vy = D_8012C594.vy;
    u->pos.vz = D_8012C594.vz;
    u->scene.vx = D_8012C59C.vx;
    u->scene.vy = D_8012C59C.vy;
    u->scene.vz = D_8012C59C.vz;
}

/* Brings up unit i's portrait panel (a unit arrived, or met its first foe). */
static inline void condor_show_unit(short i, int look, int pose)
{
    D_8012C5CC = look;
    D_8012C5D0 = 0;
    D_8012C4E0 = i;
    D_8012AF58 = pose;
    condor_load_unit_card((short)D_800BE8C0[i].kind);
    D_8012C4E4 = 0;
    D_8012C4F8 = -8;
    D_8012C4F0 = -8;
    D_8012C4FC = 1;
    D_8012C4F4 = 1;
    condor_play_sfx(2, 1);
    condor_play_sfx(0, 2);
    condor_play_sfx(0, 3);
    condor_play_sfx(0, 4);
    D_8012C42C = 1;
    D_8012C5D0 = 0;
    D_8012C4B8 = -0x20;
    condor_fade_out_start();
    D_8012C3EC = 0x11;
}

/* The unit lost its target: back to standing. */
static inline void condor_drop_target(Unit *u)
{
    u->flags = 1;
    u->mode = 0;
    u->act = 0;
    u->tx = 0;
    u->ty = 0;
    u->phase = 0;
    u->damage = 0;
    u->wait = 0;
    u->foe = 0xFF;
    u->target = 0xFF;
}

/* From its noted place, two steps turned by turn; the route cell it lands in, or -1. */
static inline short condor_try_turn(Unit *u, int turn)
{
    condor_restore_place(u);
    condor_angle_to_square((u->dir + turn) & 0xFFF);
    u->vel.vx = D_8012C544;
    u->vel.vy = D_8012C548;
    condor_apply_velocity(u);
    condor_apply_velocity(u);
    return condor_update_cell(u);
}

/* The unit's route cell if it still stands in it, else -1. */
static inline short condor_same_cell(Unit *u)
{
    Cell *r;
    short cell;
    short x, y;

    x = u->scene.vx + 0x4000;
    y = u->scene.vy + 0x4000;
    cell = u->cell;
    r = &D_8010C33C[cell];
    if (x < r->x0 || r->x1 < x || y < r->y0 || r->y1 < y)
        return -1;
    if (condor_outside_cell(r, u->scene.vx, u->scene.vy))
        return -1;
    return cell;
}

/* Every few frames (staggered by unit), a search of the player's side in the unit's sight; the first found, or -1. */
static inline short condor_look(Unit *u, Unit *probe)
{
    short r;

    if (!u->wait) {
        u->wait = (u - D_800BE8C0 + 20) & 7;
        if (u->foe != 0xFF)
            return -1;
        probe->pos.vx = u->pos.vx;
        probe->pos.vy = u->pos.vy - u->sight * 4;
        probe->pos.vz = u->pos.vz;
        probe->w = probe->h = u->sight << 4;
        r = condor_find_units_in_reach(probe, 0, 20);
        if (r != -1)
            return r;
    } else
        u->wait--;
    return -1;
}

/* Of the n found, the most hurt (listing the hurt ones in D_8012B860); -1 if none is hurt. */
static inline short condor_most_hurt(Found *h, short n)
{
    short most, best;
    short k;
    int j;
    Unit *p;
    int hurt;

    best = -1;
    if (!n)
        return -1;
    D_8012B960 = 0;
    most = 0;
    for (k = 0; k < n; k++, h++) {
        p = h->unit;
        hurt = p->max_hp - p->hp;
        if (hurt) {
            j = D_8012B960;
            D_8012B960 = j + 1;
            D_8012B860[j].unit = p;
            D_8012B860[j].idx = h->idx;
            if (hurt >= most) {
                most = p->max_hp - p->hp;
                best = h->idx;
            }
        }
    }
    return best;
}

/* Follows the unit's target (mode 3): walks after it, or gives up; the target, -1, or -2 when it gave up. */
static inline short condor_chase(Unit *u)
{
    short t = u->target;

    if (t != -1) { /* always: target is a byte */
        if (D_800BE8C0[t].flags) {
            D_8012C3AC[0] = D_800BE8C0[t].pos.vx;
            D_8012C3AC[1] = D_800BE8C0[t].pos.vy;
            condor_walk_to_target(u);
            return t;
        }
        condor_drop_target(u);
        return -2;
    }
    return -1;
}

/* Follows the unit's target (modes 3 and 5): walks after it, or gives up when it is gone. */
static inline void condor_follow(Unit *u)
{
    short t = u->target;

    if (t == -1) /* never: target is a byte */
        return;
    if (u->mode != 3 && u->mode != 5)
        return;
    if (D_800BE8C0[t].flags) {
        D_8012C3AC[0] = D_800BE8C0[t].pos.vx;
        D_8012C3AC[1] = D_800BE8C0[t].pos.vy;
        condor_walk_to_target(u);
    } else
        condor_drop_target(u);
}

/* The sprite blocks, as the game inlines their drawing (each inlined copy leaves 16 bytes of frame, as the callers'
 * frames show). */

/* Draws block i: applies its motion, then queues its sprites and its draw mode. i is reused for the first sprite's
 * index, which keeps the block's address out of reach of integrate's constant folding. */
static inline void condor_draw_block(unsigned short i)
{
    unsigned int *ot = D_80129F34->ot;
    SpriteBlock *b = &D_8010BB94[i];
    short *move;
    SPRT_16 *p;
    int row, col;

    if ((move = b->move)) {
        b->x += move[0];
        b->y += move[1];
    }
    i = b->first;
    p = &D_80129F34->sprt[i];
    for (row = 0; row < b->h * 16; row += 16) {
        for (col = 0; col < b->w * 16; col += 16) {
            p->x0 = b->x + col - D_8012C434;
            p->y0 = b->y + row - D_8012C438;
            addPrim(ot, p);
            p++;
        }
    }
    AddPrim(ot, &b->mode);
}

/* The same after moving block id to (x, y), unless x is 0xFFFF. The drawers after condor_stop_sounds have this shape:
 * the block index as an int and the block's fields reloaded (condor_draw_block's form doesn't match there). */
static inline void condor_draw_block_to(int id, unsigned short x, unsigned short y)
{
    SpriteBlock *g;
    unsigned int *ot;
    SPRT_16 *s;
    int i, j;

    ot = D_80129F34->ot;
    g = &D_8010BB94[id];
    if (x != 0xFFFF) {
        g->x = x;
        g->y = y;
    }
    if (g->move) {
        g->x += g->move[0];
        g->y += g->move[1];
    }
    s = &D_80129F34->sprt[g->first];
    for (j = 0; j < g->h * 16; j += 16) {
        for (i = 0; i < g->w * 16; i += 16) {
            s->x0 = g->x + i - D_8012C434;
            s->y0 = g->y + j - D_8012C438;
            addPrim(ot, s);
            s++;
        }
    }
    AddPrim(ot, &g->mode);
}

/* Sets the texture corner of each of block i's cells in both frames' pools: u,v for the top-left cell, 16 on per cell.
 */
static inline void condor_set_block_uv(unsigned short i, unsigned char u, unsigned char v)
{
    SpriteBlock *b = &D_8010BB94[i];
    SPRT_16 *p0 = &D_8011CBBC[0].sprt[b->first];
    SPRT_16 *p1 = &D_8011CBBC[1].sprt[b->first];
    int row, col;

    for (row = 0; row < b->h * 16; row += 16) {
        for (col = 0; col < b->w * 16; col += 16) {
            p0->u0 = p1->u0 = u + col;
            p0->v0 = p1->v0 = v + row;
            p0++;
            p1++;
        }
    }
}

/* The same with an int block and short offsets, as condor_draw_result_rows, condor_draw_frame and condor_draw_banner
 * need. */
static inline void condor_set_block_uvs(int id, short du, short dv)
{
    SpriteBlock *g;
    SPRT_16 *s0, *s1;
    int i, j;

    g = &D_8010BB94[id];
    s0 = &D_8011CBBC[0].sprt[g->first];
    s1 = &D_8011CBBC[1].sprt[g->first];
    for (j = 0; j < g->h * 16; j += 16) {
        for (i = 0; i < g->w * 16; i += 16) {
            s0->u0 = s1->u0 = i + du;
            s0->v0 = s1->v0 = j + dv;
            s0++;
            s1++;
        }
    }
}

/* Points every cell of block i in both frames' pools at the CLUT at cx,cy. */
static inline void condor_set_block_clut(unsigned short i, int cx, int cy)
{
    SpriteBlock *b = &D_8010BB94[i];
    SPRT_16 *p0 = &D_8011CBBC[0].sprt[b->first];
    SPRT_16 *p1 = &D_8011CBBC[1].sprt[b->first];
    int n = b->w * b->h;
    unsigned short clut = GetClut(cx, cy);
    int j;

    for (j = 0; j < n; j++) {
        p0->clut = p1->clut = clut;
        p0++;
        p1++;
    }
}

/* Links in, at ot, the change to the draw offset d holds. */
static inline void condor_put_offset(DrawOffset *d, unsigned int *ot)
{
    func_800449FC(&d->dr, d->ofs);
    AddPrim(ot, d);
}

/* end of declarations */

/* Projects the quad (primitive WHICH) (corners at -32, -8 and D_8012C4D4, D_8012C4D8, all at depth D_8012C388, doubled,
 * scaled by D_8012C388 about D_8012C4CC - 0x100, D_8012C4D0 - 0x200) into OT after its draw mode. */
void condor_draw_quad(unsigned int *ot, int which)
{
    SVECTOR rot;
    SVECTOR pos;
    VECTOR scale;
    MATRIX m;
    long flag, p, otz;
    DrawBuf *buf;
    Quad *quad;
    POLY_F4 *poly;

    pos.vx = D_8012C4CC - 0x100;
    pos.vy = D_8012C4D0 - 0x200;
    rot.vx = 0;
    rot.vy = 0;
    rot.vz = 0;
    pos.vz = 0;
    scale.vx = D_8012C388;
    scale.vy = D_8012C388;
    scale.vz = D_8012C388;
    PushMatrix();
    buf = D_80129F34;
    quad = &buf->shade;
    quad->v[0].vx = quad->v[2].vx = -0x20;
    quad->v[1].vx = quad->v[3].vx = D_8012C4D4 * 2;
    quad->v[0].vy = quad->v[1].vy = -8;
    quad->v[2].vy = quad->v[3].vy = D_8012C4D8 * 2;
    quad->v[0].vz = quad->v[1].vz = quad->v[2].vz = quad->v[3].vz = D_8012C388 * 2;
    RotMatrix(&rot, &m);
    ScaleMatrix(&m, &scale);
    RotTrans(&pos, (VECTOR *)m.t, &flag);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    poly = &quad->poly[which];
    RotAverageNclip4(&quad->v[0], &quad->v[1], &quad->v[2], &quad->v[3], (long *)&poly->x0,
                     (long *)&poly->x1, (long *)&poly->x2, (long *)&poly->x3, &p, &otz, &flag);
    AddPrim(ot + otz, poly);
    AddPrim(ot + otz, &quad->mode);
    PopMatrix();
}

/* Draws the units into OT as frame FRAME's primitives: the soldiers on screen (the chosen one, when it is a special
 * one, as D_80107C28), the scenery, and the fort's three parts (two of them special models); in mode 15 the bar. */
#ifdef NON_MATCHING
/* Register allocation and stack layout; the control flow and the GTE sequence are the game's. The game turns the
 * camera through whatever pointer its register held; this has a local matrix for it. */
void condor_draw_units(unsigned int *ot, int frame)
{
    MATRIX m1, m2;
    MATRIX cam_m, *cam = &cam_m; /* the game uses whatever pointer the register held */
    SVECTOR turn, zero;
    long flag;
    long p, otz, flag2;
    Unit *u;
    short i;

    PushMatrix();
    D_8012C6DC.vx = 0;
    D_8012C6DC.vy = 0;
    D_8012C6DC.vz = 0;
    D_8012C6E4.vx = 0;
    D_8012C6E4.vy = 0;
    D_8012C6E4.vz = 0;
    D_8012C6EC.vx = 0;
    D_8012C6EC.vy = 0;
    D_8012C6EC.vz = 0;
    turn.vx = 0;
    turn.vy = 0;
    turn.vz = 0;
    zero.vx = 0;
    zero.vy = 0;
    zero.vz = 0;
    RotMatrix(&D_8012C6DC, cam);
    TransMatrix(cam, &D_8012C6EC);
    SetRotMatrix(cam);
    SetTransMatrix(cam);
    for (i = 0, u = D_800BE8C0; i < 40; i++, u++) {
        if (!u->flags)
            continue;
        if (u->pos.vy > D_8012C444 + 0xF0 || u->pos.vy < D_8012C444)
            continue;
        if (u->pos.vx > D_8012C440 + 0x140 || u->pos.vx < D_8012C434)
            continue;
        PushMatrix();
        turn.vy = u->heading;
        SET_UNIT_MATRIX(u, &m1, &m2, &turn, &zero, &flag);
        if (u->fade && D_8012C494 == i)
            DRAW_TOWER(&D_80107C28[0], ot, frame)
        else
            DRAW_MODEL(&D_800C0990[i], ot, frame)
        PopMatrix();
    }
    for (i = 40; i < 52; i++) {
        u = &D_800BE8C0[i];
        if (!u->flags)
            continue;
        PushMatrix();
        turn.vy = u->heading;
        SET_UNIT_MATRIX(u, &m1, &m2, &turn, &zero, &flag);
        DRAW_MODEL(&D_800C0990[i], ot, frame)
        PopMatrix();
    }
    for (i = 0x43; i < 0x46; i++) {
        u = &D_800BE8C0[i];
        if (!u->flags)
            continue;
        PushMatrix();
        turn.vy = u->heading;
        SET_UNIT_MATRIX(u, &m1, &m2, &turn, &zero, &flag);
        if (i == 0x43)
            DRAW_TOWER(&D_80107C28[2], ot, frame)
        else if (i == 0x44)
            DRAW_TOWER(&D_80107C28[1], ot, frame)
        else
            DRAW_MODEL(&D_800C0990[i], ot, frame)
        PopMatrix();
    }
    if (D_8012C3EC == 15)
        condor_draw_quad(ot, frame);
    PopMatrix();
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_draw_units);
#endif

/* In mode 7 (the fort screen): draws the scenery units 52-62 into OT as frame FRAME's primitives, noting the deepest
 * in D_8012C540, then for fort level D_8012C478 its parts (units 64-66) where the level's layout places them, and the
 * map panel. */
#ifdef NON_MATCHING
/* Register allocation and frame size, as condor_draw_units (the camera matrix too). */
void condor_draw_fort_screen(unsigned int *ot, int frame)
{
    MATRIX m1, m2;
    MATRIX cam_m, *cam = &cam_m; /* the game uses whatever pointer the register held */
    SVECTOR turn, zero, lift;
    /* Per level, each part's presence and place. */
    short layout[3][3][3] = {
        { { 2, 0x22, 0x15 }, { 1, 0x0C, 0x15 }, { 0, 0, 0 } },
        { { 1, 0x0C, 0x15 }, { 0, 0, 0 }, { 2, 0x22, 0x18 } },
        { { 0, 0, 0 }, { 2, 0x22, 0x15 }, { 1, 0x0C, 0x18 } },
    };
    long flag;
    POLY_FT4 *map;
    Unit *u;
    short *part;
    short i;
    int k;

    if (D_8012C3EC != 7)
        return;
    PushMatrix();
    D_8012C6DC.vx = 0;
    D_8012C6DC.vy = 0;
    D_8012C6DC.vz = 0;
    D_8012C6E4.vx = 0;
    D_8012C6E4.vy = 0;
    D_8012C6E4.vz = 0;
    D_8012C6EC.vx = 0;
    D_8012C6EC.vy = 0;
    D_8012C6EC.vz = 0;
    turn.vx = 0;
    turn.vy = 0;
    turn.vz = 0;
    zero.vx = 0;
    zero.vy = 0;
    zero.vz = 0;
    RotMatrix(&D_8012C6DC, cam);
    TransMatrix(cam, &D_8012C6EC);
    SetRotMatrix(cam);
    SetTransMatrix(cam);
    lift.vx = 0;
    lift.vy = 0;
    lift.vz = D_8012C754.vz;
    PushMatrix();
    RotMatrix(&D_8012C72C, D_8012C75C);
    ScaleMatrix(D_8012C75C, &D_8012C744);
    gte_ldv0_r(&lift);
    gte_rt();
    gte_stlvnl_r(D_8012C75C->t);
    gte_stflg_r(&flag);
    gte_SetRotMatrix_r(D_8012C75C);
    gte_SetTransMatrix_r(D_8012C75C);
    for (i = 0x34; i < 0x3F; i++) {
        u = &D_800BE8C0[i];
        if (!u->flags)
            continue;
        PushMatrix();
        turn.vy = u->heading;
        SET_UNIT_MATRIX(u, &m1, &m2, &turn, &zero, &flag);
        DRAW_MODEL(&D_800C0990[i], ot, frame)
        if (D_8012C540 < SCRATCH->otz)
            D_8012C540 = SCRATCH->otz;
        PopMatrix();
    }
    PopMatrix();
    if (D_8012C478) {
        for (i = 0x40, k = 0; i < 0x43; i++) {
            u = &D_800BE8C0[i];
            part = layout[D_8012C478 - 1][k++];
            if (!u->flags || !part[0])
                continue;
            u->scene.vx = part[1];
            u->scene.vy = part[2];
            PushMatrix();
            turn.vy = u->heading;
            SET_UNIT_MATRIX(u, &m1, &m2, &turn, &zero, &flag);
            DRAW_MODEL(&D_800C0990[i], ot, frame)
            PopMatrix();
        }
    }
    PopMatrix();
    map = &D_80129F34->panel;
    map->u0 = 0;
    map->v0 = 0x30;
    map->u1 = 0x5F;
    map->v1 = 0x30;
    map->u2 = 0;
    map->v2 = 0x6F;
    map->u3 = 0x5F;
    map->v3 = 0x6F;
    map->x0 = 0xA0;
    map->y0 = 0x88;
    map->x1 = 0xFF;
    map->y1 = 0x88;
    map->x2 = 0xA0;
    map->y2 = 0xC7;
    map->x3 = 0xFF;
    map->y3 = 0xC7;
    AddPrim(D_80129F34->ot, map);
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_draw_fort_screen);
#endif

/* While D_8012C498 counts down (by 2 a frame), draws a line across the screen in that shade of red, at 0x116 or
 * at D_8012C530 - 0x18A when D_8012C404 isn't 1, over a draw mode for page 0x340. */
void condor_draw_fade_line(void)
{
    if (!D_8012C498)
        return;
    D_8012C498 -= 2;
    D_80129F34->line.x0 = -0x60;
    D_80129F34->line.x1 = 0x200;
    if (D_8012C404 == 1)
        D_80129F34->line.y0 = D_80129F34->line.y1 = 0x116;
    else
        D_80129F34->line.y0 = D_80129F34->line.y1 = D_8012C530 - 0x18A;
    D_80129F34->line.r0 = D_8012C498;
    D_80129F34->line.g0 = 0;
    D_80129F34->line.b0 = 0;
    AddPrim(&D_80129F34->ot[0xFFE], &D_80129F34->line);
    func_80044A68(&D_80129F34->line_mode, 0, 0, GetTPage(0, 1, 0x340, 0), 0);
    AddPrim(&D_80129F34->ot[0xFFE], &D_80129F34->line_mode);
}

/* Projects the field's cells into OT as frame FRAME's ground quads (those facing the camera and in depth), then the
 * chosen cell and its neighbours as the buffer's marks. */
void condor_draw_ground(unsigned int *ot, int frame)
{
    Ground *g = D_80115B3C[frame];
    Cell *c = D_8010C33C;
    DrawBuf *buf;
    POLY_F3 *m;
    long p, otz, flag;
    short k;
    int i;

    for (i = 0; i < D_8012C518; i++, c++, g++) {
        if (RotAverageNclip4(&c->v[1], &c->v[2], &c->v[3], &c->v[1], (long *)&g->poly.x0, (long *)&g->poly.x1,
                             (long *)&g->poly.x2, (long *)&g->poly.x3, &p, &otz, &flag) > 0 &&
            otz > 0 && otz < 0x1000)
            AddPrim(ot, &g->poly);
    }
    if (D_8012C3FC == -1)
        return;
    c = &D_8010C33C[D_8012C3FC];
    buf = D_80129F34;
    m = &buf->mark[0];
    RotAverageNclip3(&c->v[1], &c->v[2], &c->v[3], (long *)&m->x0, (long *)&m->x1, (long *)&m->x2, &p, &otz, &flag);
    AddPrim(ot, m);
    k = D_8010C33C[D_8012C3FC].next[0];
    m = &buf->mark[1];
    if (k != -1) {
        c = &D_8010C33C[k];
        RotAverageNclip3(&c->v[1], &c->v[2], &c->v[3], (long *)&m->x0, (long *)&m->x1, (long *)&m->x2, &p, &otz,
                         &flag);
        AddPrim(ot, m);
    }
    k = D_8010C33C[D_8012C3FC].next[1];
    m = &buf->mark[2];
    if (k != -1) {
        c = &D_8010C33C[k];
        RotAverageNclip3(&c->v[1], &c->v[2], &c->v[3], (long *)&m->x0, (long *)&m->x1, (long *)&m->x2, &p, &otz,
                         &flag);
        AddPrim(ot, m);
    }
    k = D_8010C33C[D_8012C3FC].next[2];
    m = &buf->mark[3];
    if (k != -1) {
        c = &D_8010C33C[k];
        RotAverageNclip3(&c->v[1], &c->v[2], &c->v[3], (long *)&m->x0, (long *)&m->x1, (long *)&m->x2, &p, &otz,
                         &flag);
        AddPrim(ot, m);
    }
}

/* Lists in D_8012B75C (marked 1) the live, targetable units within 13 of POS across and from 10 above to 14 below
 * it, and gives the index of the nearest, by the smaller of the x and y distances, or -1. */
#ifdef NON_MATCHING
/* The game loads the point's x and y once into two registers each and hoists x + 13 and y + 14. */
short condor_find_units_at(short *pos)
{
    Unit *o;
    short i;
    short best;
    short ret;
    short dx, dy;
    short adx, ady;
    int px, py;

    px = pos[0];
    py = pos[1];
    D_8012B85C = 0;
    best = 0x4000;
    ret = -1;
    for (o = D_800BE8C0, i = 0; i < 40; o++, i++) {
        if (o->flags && !o->fade && px - 13 < o->pos.vx && o->pos.vx < px + 13 &&
            py - 10 < o->pos.vy && o->pos.vy < py + 14) {
            D_8012B75C[D_8012B85C].idx = 1;
            D_8012B75C[D_8012B85C].unit = o;
            D_8012B85C++;
            dx = o->pos.vx - px;
            adx = dx;
            if (dx < 0)
                adx = -dx;
            dy = o->pos.vy - py;
            ady = dy;
            if (dy < 0)
                ady = -dy;
            if (adx > ady)
                adx = ady;
            if (adx < best) {
                best = adx;
                ret = i;
            }
        }
    }
    return ret;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_find_units_at);
#endif

/* As condor_find_units_in_reach, but the boxes meet as bodies do (each its own height, half the widths apart) and U
 * itself is left out. */
short condor_find_touching_units(Unit *u, short first, short n)
{
    Unit *o;
    short i;
    short best;
    short ret;
    short dx, dy;
    short adx, ady;
    int w;

    D_8012B85C = 0;
    best = 0x4000;
    ret = -1;
    o = &D_800BE8C0[first];
    for (i = first; i < n; i++, o++) {
        if (!o->flags)
            continue;
        if (o->pos.vy < u->pos.vy ? o->pos.vy < u->pos.vy - u->h : u->pos.vy < o->pos.vy - o->h)
            continue;
        w = (o->w + u->w) >> 1;
        if (o->pos.vx < u->pos.vx ? o->pos.vx < u->pos.vx - w : u->pos.vx < o->pos.vx - w)
            continue;
        if (o == u)
            continue;
        if (o->fade)
            continue;
        D_8012B75C[D_8012B85C].unit = o;
        D_8012B75C[D_8012B85C].idx = i;
        D_8012B85C++;
        dx = o->pos.vx - u->pos.vx;
        adx = dx;
        if (dx < 0)
            adx = -dx;
        dy = o->pos.vy - u->pos.vy;
        ady = dy;
        if (dy < 0)
            ady = -dy;
        if (adx > ady)
            adx = ady;
        if (adx < best) {
            best = adx;
            ret = i;
        }
    }
    return ret;
}

/* Lists in D_8012B75C the live, targetable units I..N-1 whose boxes meet U's (U's height both ways, the full widths
 * apart) and gives the index of the nearest, by the smaller of the x and y distances, or -1. */
short condor_find_units_in_reach(Unit *u, short first, short n)
{
    Unit *o;
    short i;
    short best = 0x4000;
    short ret;
    short dx, dy;
    short adx, ady;
    int w;
    short h;

    D_8012B85C = 0;
    h = u->h;
    o = &D_800BE8C0[first];
    ret = -1;
    for (i = first; i < n; i++, o++) {
        if (!o->flags)
            continue;
        if (o->pos.vy < u->pos.vy ? o->pos.vy < u->pos.vy - h : u->pos.vy < o->pos.vy - h)
            continue;
        w = o->w + u->w;
        if (o->pos.vx < u->pos.vx ? o->pos.vx < u->pos.vx - w : u->pos.vx < o->pos.vx - w)
            continue;
        if (o->fade)
            continue;
        D_8012B75C[D_8012B85C].unit = o;
        D_8012B75C[D_8012B85C].idx = i;
        D_8012B85C++;
        dx = o->pos.vx - u->pos.vx;
        adx = dx;
        if (dx < 0)
            adx = -dx;
        dy = o->pos.vy - u->pos.vy;
        ady = dy;
        if (dy < 0)
            ady = -dy;
        if (adx > ady)
            adx = ady;
        if (adx < best) {
            best = adx;
            ret = i;
        }
    }
    return ret;
}

/* Counts into D_8012B85C the live units I..N-1, other than U, in U's reach: a box 28 wide and 22 tall. */
void condor_count_units_in_reach(Unit *u, short first, short n)
{
    Unit *o;
    short i;
    short hit;
    int half;

    D_8012B85C = 0;
    o = &D_800BE8C0[first];
    for (i = first; i < n; o++, i++) {
        if (o->flags) {
            half = (o->w + 28) >> 1;
            hit = 0;
            if (o->pos.vx < u->pos.vx)
                hit = o->pos.vx >= u->pos.vx - half;
            else if (u->pos.vx >= o->pos.vx - half)
                hit = 1;
            if (hit) {
                hit = 0;
                if (o->pos.vy < u->pos.vy)
                    hit = o->pos.vy >= u->pos.vy - 22;
                else if (u->pos.vy >= o->pos.vy - o->h)
                    hit = 1;
                if (hit && o != u)
                    D_8012B85C++;
            }
        }
    }
}

/* Index I if unit I is a live unit other than U whose box overlaps U's, else -1. */
short condor_unit_overlaps(Unit *u, short i)
{
    Unit *o = &D_800BE8C0[i];
    short ret = -1;
    short hit;
    int half;

    if (o->flags) {
        half = (o->w + u->w) >> 1;
        hit = 0;
        if (o->pos.vx < u->pos.vx)
            hit = o->pos.vx >= u->pos.vx - half;
        else if (u->pos.vx >= o->pos.vx - half)
            hit = 1;
        if (hit) {
            hit = 0;
            if (o->pos.vy < u->pos.vy)
                hit = o->pos.vy >= u->pos.vy - u->h;
            else if (u->pos.vy >= o->pos.vy - o->h)
                hit = 1;
            if (hit && o != u)
                ret = i;
        }
    }
    return ret;
}

/* Ages the 16 popups (sprite groups 0x3B on): each takes its palette on its second frame, rises half a pixel a
 * frame and goes after 36. */
void condor_update_popups(void)
{
    int i;
    short age;

    for (i = 0; i < 16; i++) {
        if (!D_8012C5FC[i].age)
            continue;
        D_8012C5FC[i].age++;
        age = D_8012C5FC[i].age & 0x7FFF;
        if (age == 2) {
            if (!(D_8012C5FC[i].age & 0x8000))
                condor_set_block_row(i + 0x3B, 0);
            else
                condor_set_block_row(i + 0x3B, 4);
        }
        if (age >= 36) {
            D_8012C5FC[i].age = 0;
            continue;
        }
        D_8010BB94[i + 0x3B].x = D_8012C5FC[i].x;
        D_8010BB94[i + 0x3B].y = D_8012C5FC[i].y - (age >> 1);
        condor_put_block(i + 0x3B, 0xFFFF, 0);
    }
}

/* Draws VALUE's last two digits into counter SLOT, the tens blank below 10. */
void condor_draw_counter_digits(int slot, int value)
{
    int tens = value % 100 / 10;
    unsigned int *p = D_8012AF5C[0] + slot * 2;
    int ones;
    int r;

    if (tens || value >= 100) {
        for (r = 0; r < 8; r++, p += 32)
            *p = FONT[r][tens];
    } else {
        for (r = 0; r < 8; r++, p += 32)
            *p = 0;
    }
    ones = value % 10;
    p = D_8012AF5C[0] + slot * 2;
    for (r = 0; r < 8; r++, p += 32)
        p[1] = FONT[r][ones];
    D_8012AF54++;
}

/* Sets up group G: W x H cells taken from the next free ones, their texture from U, V on the page at TX, TY (TP, and
 * SEMI - 1 as the blend when SEMI is set, which also makes them semi-transparent), their CLUT at CX, CY (rows from
 * 0x100 are 64 to the left), SHIFT added to where they are drawn. */
void condor_init_block(unsigned short g, unsigned short w, unsigned short h, unsigned short u, unsigned short v,
                  unsigned short tp, unsigned short semi, unsigned short tx, unsigned short ty, unsigned short cx,
                  unsigned short cy, short *move)
{
    SpriteBlock *grp;
    unsigned short abr;
    unsigned short clut;
    int row, col;

    if (!semi)
        abr = 0;
    else
        abr = semi - 1;
    grp = &D_8010BB94[g];
    grp->w = w;
    grp->h = h;
    grp->x = 0;
    grp->y = 0;
    grp->move = move;
    grp->first = D_80129F38;
    if (cy >= 0x100)
        clut = GetClut(cx - 0x40, cy - 0x100);
    else
        clut = GetClut(cx, cy);
    func_80044A68(&grp->mode, 0, 0, GetTPage(tp, abr, tx, ty), 0);
    for (row = 0; row < grp->h; row++) {
        for (col = 0; col < grp->w; col++) {
            if (semi) {
                func_80046848(&D_8011CBBC[0].sprt[D_80129F38], 1);
                func_80046848(&D_8011CBBC[1].sprt[D_80129F38], 1);
            }
            D_8011CBBC[0].sprt[D_80129F38].clut = clut;
            D_8011CBBC[1].sprt[D_80129F38].clut = clut;
            setUV0(&D_8011CBBC[0].sprt[D_80129F38], u + col * 16, v + row * 16);
            setUV0(&D_8011CBBC[1].sprt[D_80129F38], u + col * 16, v + row * 16);
            D_80129F38++;
        }
    }
}

/* Puts group G's cells in the buffer being drawn, at X, Y unless X is 0xFFFF (then where it last was), shifted by its
 * SHIFT, less the camera. */
void condor_put_block(unsigned short g, unsigned short x, short y)
{
    unsigned int *ot = D_80129F34->ot;
    SpriteBlock *grp = &D_8010BB94[g];
    SPRT_16 *p;
    short *move;
    int cx, cy;

    if (x != 0xFFFF) {
        grp->x = x;
        grp->y = y;
    }
    move = grp->move;
    if (move) {
        grp->x += move[0];
        grp->y += move[1];
    }
    p = &D_80129F34->sprt[grp->first];
    for (cy = 0; cy < grp->h * 16; cy += 16) {
        for (cx = 0; cx < grp->w * 16; cx += 16, p++) {
            p->x0 = grp->x + cx - D_8012C434;
            p->y0 = grp->y + cy - D_8012C438;
            addPrim(ot, p);
        }
    }
    AddPrim(ot, &grp->mode);
}

/* Points every cell of group G at CLUT row 0xF0 + ROW (x 0x380), in both buffers. */
void condor_set_block_row(unsigned short g, unsigned short row)
{
    SpriteBlock *grp = &D_8010BB94[g];
    int n = grp->w * grp->h;
    SPRT_16 *a = &D_8011CBBC[0].sprt[grp->first];
    SPRT_16 *b = &D_8011CBBC[1].sprt[grp->first];
    unsigned short clut = GetClut(0x380, row + 0xF0);
    int i;

    for (i = 0; i < n; a++, b++, i++) {
        b->clut = clut;
        a->clut = clut;
    }
}

/* Draws D_8012C508 and D_8012C50C as two digits each (a leading zero blank) into VRAM at 0x3C8, 0x10 and 0x3C8,
 * 0x28. */
void condor_load_two_numbers(void)
{
    unsigned char digit[2];
    int i;
    int d;

    for (i = 15; i >= 0; i--)
        D_80129F3C[i] = 0;
    D_8012C430 = 0;
    D_8012C53C = D_8012C508;
    digit[0] = condor_next_digit(10);
    digit[1] = condor_next_digit(1);
    d = digit[0];
    if (d != 0xFF)
        put_digit(0, d);
    d = digit[1];
    if (d != 0xFF)
        put_digit(1, d);
    setRECT(&D_8012AF3C, 0x3C8, 0x10, 4, 8);
    func_80044000(&D_8012AF3C, D_80129F3C);
    DrawSync(0);

    for (i = 31; i >= 0; i--)
        D_80129F3C[i] = 0;
    D_8012C430 = 0;
    D_8012C53C = D_8012C50C;
    digit[0] = condor_next_digit(10);
    digit[1] = condor_next_digit(1);
    d = digit[0];
    if (d != 0xFF)
        put_digit(0, d);
    d = digit[1];
    if (d != 0xFF)
        put_digit(1, d);
    setRECT(&D_8012AF3C, 0x3C8, 0x28, 4, 8);
    func_80044000(&D_8012AF3C, D_80129F3C);
    DrawSync(0);
}

/* Loads the unit card at 0x3AA, 3 in VRAM: the empty frame for WHO -1, else WHO's portrait and two digits (blank for
 * UNIT -1; else unit UNIT's number, or D_8012BA68[WHO] when D_8012C3EC is 7). */
#ifdef NON_MATCHING
/* The game keeps the image bases in registers and steps byte offsets where GCC folds them. */
void condor_load_unit_card(who, unit)
short who, unit;
{
    unsigned char digit[2];
    short col, blk;
    int at, from;
    int n = 0;
    int row, c;

    if (who == -1) {
        for (row = 0; row < 16; row++) {
            from = 0x1C00 + row * 32;
            for (c = 0; c < 11; c++)
                D_80129F3C[n++] = IMG_FRAME[from + c];
        }
    } else {
        if (unit == -1) {
            digit[1] = digit[0] = 11;
        } else {
            D_8012C53C = D_800BE8C0[unit].number;
            if (D_8012C3EC == 7)
                D_8012C53C = D_8012BA68[who];
            D_8012C430 = 0;
            digit[0] = condor_next_digit(10);
            digit[1] = condor_next_digit(1);
            if (digit[0] == 0xFF)
                digit[0] = 0;
        }
        col = who / 6;
        blk = who % 6;
        at = col * 8 + blk * 512;
        for (row = 0; row < 16; row++) {
            from = at + row * 32;
            for (c = 0; c < 8; c++)
                D_80129F3C[n++] = IMG_PORTRAITS[from + c];
            D_80129F3C[n++] = IMG_DIGITS[row][digit[0]];
            D_80129F3C[n++] = IMG_DIGITS[row][digit[1]];
            D_80129F3C[n++] = 0;
        }
    }
    setRECT(&D_8012AF4C, 0x3AA, 3, 0x16, 0x10);
    func_80044000(&D_8012AF4C, D_80129F3C);
    DrawSync(0);
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_load_unit_card);
#endif

/* One decimal digit of D_8012C53C at PLACE, taken off it; 0xFF (blank) for a leading zero. D_8012C430 counts the
 * digits given so far. Callers after it in the file get it inlined. */
inline unsigned char condor_next_digit(unsigned int place)
{
    unsigned int digit;

    if (!D_8012C53C) {
        if (!D_8012C430)
            return place != 1 ? 0xFF : 0;
        return 0;
    }
    digit = D_8012C53C / place;
    if (!digit) {
        if (!D_8012C430)
            return 0xFF;
        return 0;
    }
    D_8012C430++;
    D_8012C53C -= digit * place;
    return digit;
}

/* Draws D_8012C538 as seven digits and REC's score as four, each followed by glyphs 0x8B 0x8C, into the counters'
 * image (rows 1 and 12) and loads it at 0x3E0, 0xC0. */
void condor_load_score_counters(Record *rec)
{
    unsigned char text[9];
    int i;

    for (i = 0x1FF; i >= 0; i--)
        D_8012A73C[i] = 0;
    D_8012C430 = 0;
    D_8012C53C = D_8012C538;
    text[0] = condor_next_digit(1000000);
    text[1] = condor_next_digit(100000);
    text[2] = condor_next_digit(10000);
    text[3] = condor_next_digit(1000);
    text[4] = condor_next_digit(100);
    text[5] = condor_next_digit(10);
    text[6] = condor_next_digit(1);
    text[7] = 0x8B;
    text[8] = 0x8C;
    for (i = 0; i < 9; i++)
        put_glyph(i + 10, text[i], 10);
    D_8012C534 = rec->score;
    D_8012C53C = rec->score;
    D_8012C430 = 0;
    text[0] = condor_next_digit(1000);
    text[1] = condor_next_digit(100);
    text[2] = condor_next_digit(10);
    text[3] = condor_next_digit(1);
    text[4] = 0x8B;
    text[5] = 0x8C;
    for (i = 0; i < 6; i++)
        put_glyph(i + 0x7B, text[i], 10);
    setRECT(&D_8012AF3C, 0x3E0, 0xC0, 0x14, 0x20);
    func_80044000(&D_8012AF3C, D_8012A73C);
    DrawSync(0);
}

/* Shows the card of unit kind KIND: its picture (from the image at 0x801A8040) at 0x3C0, 0xC0 in VRAM, its three
 * numbers (record fields x02, x05, x07) at 0x3D8, 0xC0, then the score counters. */
void condor_load_unit_info(short kind)
{
    unsigned char text[9];
    unsigned char pic[28] = {
        0x0A, 0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x04, 0x00, 0x06, 0x05, 0x00, 0x05, 0x02,
        0x05, 0x04, 0x05, 0x06, 0x0A, 0x06, 0x0A, 0x06, 0x0A, 0x06, 0x0A, 0x00, 0x0A, 0x02,
    };
    unsigned int *src;
    Record *rec;
    int i;

    src = PICTURES + (unsigned short)(pic[kind * 2] << 1 | pic[kind * 2 + 1] << 9);
    setRECT(&D_8012AF44, 0x3C0, 0xC0, 0x40, 0x20);
    func_80044000(&D_8012AF44, src);
    DrawSync(0);
    for (i = 0x1FF; i >= 0; i--)
        D_80129F3C[i] = 0;
    rec = (Record *)(CONDOR_DATA + *(unsigned short *)CONDOR_DATA);
    rec += kind;
    D_8012C53C = rec->x02;
    D_8012C430 = 0;
    text[0] = condor_next_digit(100);
    text[1] = condor_next_digit(10);
    text[2] = condor_next_digit(1);
    for (i = 0; i < 3; i++)
        put_card_glyph(i, text[i], 6);
    D_8012C4AC = 0;
    D_8012C53C = rec->x05;
    if (!D_8012C53C) {
        text[0] = 0xFF;
        text[1] = 0x0E;
        for (i = 0; i < 2; i++)
            put_card_glyph(i + 0x60, text[i], 6);
        D_8012C4B0 = 0;
    } else {
        D_8012C430 = 0;
        text[0] = condor_next_digit(100);
        text[1] = condor_next_digit(10);
        text[2] = condor_next_digit(1);
        for (i = 0; i < 3; i++)
            put_card_glyph(i + 0x60, text[i], 6);
        /* the number's indent: its digits are right-aligned */
        D_8012C4B0 = 8;
        if (text[1] != 0xFF)
            D_8012C4B0 = 4;
        if (text[0] != 0xFF)
            D_8012C4B0 = 0;
    }
    D_8012C53C = rec->x07;
    if (D_8012C53C < 2) {
        text[0] = 0xFF;
        text[1] = 0xFF;
        if (!D_8012C53C)
            text[2] = 0x0E;
        else
            text[2] = 1;
        for (i = 0; i < 3; i++)
            put_card_glyph(i + 0xC0, text[i], 6);
    } else {
        text[0] = 1;
        text[1] = 0x0D;
        text[2] = D_8012C53C;
        for (i = 0; i < 3; i++)
            put_card_glyph(i + 0xC0, text[i], 6);
    }
    setRECT(&D_8012AF3C, 0x3D8, 0xC0, 0xC, 0x30);
    func_80044000(&D_8012AF3C, D_80129F3C);
    DrawSync(0);
    condor_load_score_counters(rec);
}

/* Draws the selected unit's "hp/max" (8x8 glyphs from the font the menu loads at 0x80198880) into VRAM. */
#ifdef NON_MATCHING
/* The digit step: the game divides by a register (divu), and its blank tests branch the other way round. */
void condor_draw_hp(void)
{
    unsigned char digits[10];
    int i, row, glyph;

    digits[0] = 0xFF;
    digits[1] = 0xFF;
    digits[2] = 0xFF;
    D_8012C430 = 0;
    D_8012C53C = D_800BE8C0[D_8012C41C].hp;
    digits[3] = condor_next_digit(100);
    digits[4] = condor_next_digit(10);
    digits[5] = condor_next_digit(1);
    digits[6] = 12;
    D_8012C430 = 0;
    D_8012C53C = D_800BE8C0[D_8012C41C].max_hp;
    digits[7] = condor_next_digit(100);
    digits[8] = condor_next_digit(10);
    digits[9] = condor_next_digit(1);
    for (i = 0x1FF; i >= 0; i--)
        D_80129F3C[i] = 0;
    for (i = 0; i < 10; i++) {
        glyph = digits[i];
        if (glyph == 0xFF)
            continue;
        for (row = 0; row < 8; row++)
            D_80129F3C[(unsigned char)i + row * 16] = ((unsigned int(*)[32])0x80198880)[row][glyph];
    }
    D_8012AF3C.x = 0x3C0;
    D_8012AF3C.y = 0xD0;
    D_8012AF3C.w = 0x20;
    D_8012AF3C.h = 0x10;
    func_80044000(&D_8012AF3C, D_80129F3C);
    DrawSync(0);
    condor_load_unit_card((short)D_800BE8C0[D_8012C41C].kind);
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_draw_hp);
#endif

/* Draws the scrolled background: the 256-wide picture wraps, so up to four bands, each after the draw mode of its
 * texture page. */
#ifdef NON_MATCHING
/* The game reads D_8012C444 twice and masks vtop with `and -16`; register allocation follows from that. */
int condor_draw_background(unsigned int *ot)
{
    SPRT_16 *p = D_80129F34->map;
    int mode = (D_8012C444 >> 8) * 2;
    int vtop, vend, xfine, yfine;
    int u, v, x, y, vv;

    vend = 0x100 - (D_8012C444 & 0xF0);
    xfine = D_8012C440 & 0xF;
    yfine = D_8012C444 & 0xF;
    vtop = D_8012C444 & ~0xF;
    if (vend >= 0xE0)
        vend = 0xE0;
    vend += vtop;
    CONDOR_BAND(vtop, -yfine, D_8012C440 & 0xFF0, -xfine, 0x100);
    AddPrim(ot + 0xFFF, &D_80129F34->mode[mode++]);
    CONDOR_BAND(vtop, -yfine, 0, 0x100 - D_8012C440, D_8012C440 + 0x40);
    AddPrim(ot + 0xFFF, &D_80129F34->mode[mode++]);
    vend = (vtop & 0xF0) - 0x20;
    if (vend > 0) {
        if (vend >= 0xE0)
            vend = 0xE0;
        CONDOR_BAND(0, 0x100 - (unsigned char)D_8012C444, D_8012C440 & 0xFF0, -xfine, 0x100);
        AddPrim(ot + 0xFFF, &D_80129F34->mode[mode++]);
        CONDOR_BAND(0, 0x100 - (unsigned char)D_8012C444, 0, 0x100 - D_8012C440, D_8012C440 + 0x40);
        AddPrim(ot + 0xFFF, &D_80129F34->mode[mode]);
    }
    return 1;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_draw_background);
#endif

void condor_fade_out_start(void)
{
    if (D_8011CBBC[D_8012C3F0].map[0].r0 < 0x51)
        D_8012C488 = 3;
    else
        D_8012C488 = 1;
    D_8012C47C = 2;
    D_8012C480 = D_8012C484 = 1;
}

/* Fades the map in this frame's buffer: 1 darkens it to 0x50, 3 brightens it back to 0x80, 2 just finishes. */
void condor_fade_step_queued(void)
{
    SPRT_16 *p = D_8011CBBC[D_8012C3F0].map;
    int i;

    if (D_8012C488 == 1) {
        if (p->r0 > 0x50) {
            for (i = 0; i < 0x126; p++, i++) {
                p->r0 -= 0x10;
                p->g0 -= 0x10;
                p->b0 -= 0x10;
            }
        } else
            FADE_BUFFER_DONE();
    }
    if (D_8012C488 == 3) {
        if (p->r0 < 0x50) {
            for (i = 0; i < 0x126; p++, i++) {
                p->r0 += 0x10;
                p->g0 += 0x10;
                p->b0 += 0x10;
            }
        } else
            FADE_BUFFER_DONE();
    }
    if (D_8012C488 == 2)
        FADE_BUFFER_DONE();
}

/* Runs the queued triangle recolourings, then this frame's step of the map fade (D_8012C47C). */
void condor_update_colours(void)
{
    UnitFace *shade;
    ModelFace *colors;
    POLY_G3 *p;
    SPRT_16 *t;
    int i, j, n, flag, k;
    short m;

    if (D_8012C364) {
        for (i = 0; i < D_8012C364; i++) {
            shade = D_8012C01C[i].faces;
            n = D_8012C01C[i].count;
            colors = D_8012C01C[i].src;
            flag = n & 0x8000;
            n &= 0x7FFF;
            if (!flag) {
                for (j = 0; j < n; j++, shade++, colors++) {
                    p = &shade->prim[D_8012C3F0];
                    p->r0 = colors->rgb[0][0];
                    p->g0 = colors->rgb[0][1];
                    p->b0 = colors->rgb[0][2];
                    p->r1 = colors->rgb[1][0];
                    p->g1 = colors->rgb[1][1];
                    p->b1 = colors->rgb[1][2];
                    p->r2 = colors->rgb[2][0];
                    p->g2 = colors->rgb[2][1];
                    p->b2 = colors->rgb[2][2];
                }
            } else {
                for (j = 0; j < n; shade++, colors++, j++) {
                    p = &shade->prim[D_8012C3F0];
                    m = colors->rgb[0][0];
                    if (m < colors->rgb[0][1])
                        m = colors->rgb[0][1];
                    if (m < colors->rgb[0][2])
                        m = colors->rgb[0][2];
                    p->b0 = m;
                    p->r0 = p->g0 = m >> 1;
                    m = colors->rgb[1][0];
                    if (m < colors->rgb[1][1])
                        m = colors->rgb[1][1];
                    if (m < colors->rgb[1][2])
                        m = colors->rgb[1][2];
                    p->b1 = m;
                    p->r1 = p->g1 = m >> 1;
                    m = colors->rgb[2][0];
                    if (m < colors->rgb[2][1])
                        m = colors->rgb[2][1];
                    if (m < colors->rgb[2][2])
                        m = colors->rgb[2][2];
                    p->b2 = m;
                    p->r2 = p->g2 = m >> 1;
                }
            }
        }
        D_8012C364 = 0;
    }
    switch (D_8012C47C) {
    case 1:
        t = D_8011CBBC[D_8012C3F0].map;
        m = t->r0;
        if (m < 0x80) {
            for (k = 0; k < 0x126; k++, t++) {
                t->r0 += 0x10;
                t->g0 += 0x10;
                t->b0 += 0x10;
            }
        } else
            FADE_BUFFER_DONE();
        break;
    case 2:
        condor_fade_step_queued();
        break;
    case 3:
        t = D_8011CBBC[D_8012C3F0].map;
        if (t->r0 > 0x30) {
            for (k = 0; k < 0x126; k++, t++) {
                t->r0 -= 0x10;
                t->g0 -= 0x10;
                t->b0 -= 0x10;
            }
        } else
            FADE_BUFFER_DONE();
        break;
    }
}

void condor_play_sfx(short pan, int effect)
{
    snd_request.id = effect + 0x27;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = pan;
    snd_submit_request();
}

/* The camera list: slide the units in, let up/down (L2/R2) pick one, circle or cross slides them home. */
#ifdef NON_MATCHING
/* Register allocation in the 8-step slide: the fraction's store and the step's copy are ordered differently. */
void condor_run_unit_list(void)
{
    Pick *e;
    Unit *u;
    short i, left;
    unsigned short k, s, d;

    switch (D_8012C42C) {
    case 1:
        CONDOR_SLIDE_ALL();
        if (!left)
            D_8012C42C = 2;
        break;
    case 2:
        if (D_8012C398 & 0x2000) {
            CONDOR_SFX(0x2B, 0x40, 1);
            if (++D_8012C00C >= D_8012C008)
                D_8012C00C = 0;
            D_8012C41C = D_8012BA88[D_8012C00C].unit - D_800BE8C0;
            condor_load_unit_card((short)D_800BE8C0[D_8012C41C].kind);
            condor_draw_hp();
        }
        if (D_8012C398 & 0x8000) {
            CONDOR_SFX(0x2B, 0x40, 1);
            if (--D_8012C00C < 0)
                D_8012C00C = D_8012C008 - 1;
            D_8012C41C = D_8012BA88[D_8012C00C].unit - D_800BE8C0;
            condor_load_unit_card((short)D_800BE8C0[D_8012C41C].kind);
            condor_draw_hp();
        }
        if (D_8012C39C & 0x60) {
            if (D_8012C39C & 0x20) {
                D_8012C010 = 1;
                CONDOR_SFX(0x2B, 0x40, 1);
            } else {
                D_8012C010 = 2;
                CONDOR_SFX(0x2B, 0x40, 4);
            }
            for (i = 0, e = D_8012BA88; i < D_8012C008; i++, e++) {
                u = e->unit;
                condor_aim(u->scene.vx, u->scene.vy, e->home.vx, e->home.vy);
                e->step_x = D_8012C544;
                e->step_y = D_8012C548;
                e->frac_y = 0;
                e->frac_x = 0;
                e->to.vx = e->home.vx;
                e->to.vy = e->home.vy;
                e->to.vz = e->home.vz;
                u->scene.vz = e->home.vz;
            }
            D_8012C42C = 3;
        } else
            for (i = 0; i < D_8012C008; i++)
                ; /* its body is gone, the loop left behind */
        break;
    case 3:
        CONDOR_SLIDE_ALL();
        if (left)
            break;
        for (i = 0, e = D_8012BA88; i < D_8012C008; i++, e++) {
            u = e->unit;
            u->scene.vx = e->home.vx;
            u->scene.vy = e->home.vy;
            u->scene.vz = e->home.vz;
            u->rot.vy = e->x20;
            u->dir = e->x22;
        }
        if (D_8012C010 == 1)
            D_8012C41C = D_8012BA88[D_8012C00C].index;
        else
            D_8012C41C = D_8012C018;
        condor_load_unit_card((short)D_800BE8C0[D_8012C41C].kind);
        condor_draw_hp();
        D_8012C3EC = 0;
        D_8012C42C = 0;
        if (D_8012C3E8 == 1) {
            if (D_8012C010 == 1 && (unsigned short)D_800BE8C0[D_8012C41C].kind < 16)
                condor_open_unit_menu();
            else {
                D_8012C47C = 1;
                D_8012C480 = D_8012C484 = 1;
            }
        }
        if (D_8012C3E8 == 3) {
            if (D_8012C010 == 1)
                D_8012C420 = D_8012C41C;
            D_8012C41C = D_8012C018;
        }
        D_8012C014++;
        break;
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_run_unit_list);
#endif

/* Puts back the map's own picture and palette and redraws three frames with it (L1 pressed). */
#ifdef NON_MATCHING
/* The 0x80 constant and the row counter swap registers; GetClut's a0 is set after the buffer pointer's load. */
int condor_reload_map_image(void)
{
    SPRT_16 *p;
    unsigned short clut;
    int x, y;

    D_8012C42C = 1;
    if (!(D_8012C39C & 0x100))
        return 0;
    CONDOR_SFX(0x2B, 0x40, 1);
    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    D_8012AF3C.x = 0x140;
    D_8012AF3C.w = 0xA0;
    D_8012AF3C.y = 0;
    D_8012AF3C.h = 0xDC;
    D_8012C434 = D_8012C458;
    D_8012C438 = D_8012C45C;
    func_80044000(&D_8012AF3C, (void *)0x80150220);
    DrawSync(0);
    D_8012AF3C.x = 0;
    D_8012AF3C.y = 0x1F0;
    D_8012AF3C.w = 0x100;
    D_8012AF3C.h = 1;
    func_80044000(&D_8012AF3C, (void *)0x80150014);
    DrawSync(0);
    D_8012C3EC = 0;

    p = (SPRT_16 *)D_80129F34;
    clut = GetClut(0, 0x1F0);
    p += 0x533;
    for (y = 0; y < 14; y++)
        for (x = 0; x < 21; p++, x++) {
            p->clut = clut;
            p->r0 = p->g0 = p->b0 = 0x80;
        }
    condor_flip_buffers();
    condor_draw_frame();
    p = (SPRT_16 *)D_80129F34;
    clut = GetClut(0, 0x1F0);
    p += 0x533;
    for (y = 0; y < 14; y++)
        for (x = 0; x < 21; p++, x++) {
            p->clut = clut;
            p->r0 = p->g0 = p->b0 = 0x80;
        }
    condor_flip_buffers();
    condor_draw_frame();
    p = (SPRT_16 *)D_80129F34;
    clut = GetClut(0, 0x1F0);
    p += 0x533;
    for (y = 0; y < 14; y++)
        for (x = 0; x < 21; p++, x++) {
            p->clut = clut;
            p->r0 = p->g0 = p->b0 = 0x80;
        }
    condor_flip_buffers();
    condor_draw_frame();
    VSync(0);
    SetDispMask(1);
    D_8012C42C = 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_reload_map_image);
#endif

/* Free scrolling of the map: the d-pad moves it (twice as far with square held), start leaves. */
#ifdef NON_MATCHING
/* One instruction: the game sets s0 = 1 in the delay slot before the start test. */
void condor_free_scroll(void)
{
    D_8012C42C = 1;
    if (D_8012C390 & 0xF000) {
        condor_scroll_camera(D_8012C390);
        condor_scroll_camera(D_8012C390);
        condor_scroll_camera(D_8012C390);
        condor_scroll_camera(D_8012C390);
        if (D_8012C390 & 0x80) {
            condor_scroll_camera(D_8012C390);
            condor_scroll_camera(D_8012C390);
            condor_scroll_camera(D_8012C390);
            condor_scroll_camera(D_8012C390);
        }
    }
    if (D_8012C39C & 0x800) {
        CONDOR_SFX(0x2B, 0x40, 1);
        if (D_8012C40C != D_8012C410)
            D_8012C408 = 1;
        else {
            D_8012C3EC = 0;
            D_8012C42C = 0;
            D_8012C488 = D_8012C48C;
            if (D_8012C488 == 1) {
                D_8012C47C = 1;
                D_8012C480 = D_8012C484 = 1;
            } else {
                if (D_8011CBBC[D_8012C3F0].map[0].r0 < 0x51)
                    D_8012C488 = 3;
                else
                    D_8012C488 = 1;
                D_8012C47C = 2;
                D_8012C480 = D_8012C484 = 1;
            }
            condor_load_two_numbers();
            condor_clear_selection();
            D_8012C428 = 1;
            D_8012C3A4[0] = D_8012C44C + D_8012C434;
            D_8012C3A4[1] = D_8012C450 + D_8012C438;
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_free_scroll);
#endif

/* The two-choice prompt: up/down move between the rows (0 and 0x10), circle takes the row, cross backs out. */
void condor_yes_no(void)
{
    if (D_8012C39C & 0x20) {
        if (!D_8012C4F4) {
            CONDOR_SFX(0x2B, 0x40, 1);
            D_8012C408 = 3;
        } else {
            CONDOR_SFX(0x2B, 0x40, 4);
            D_8012C47C = 1;
            D_8012C480 = D_8012C484 = 1;
            D_8012C3EC = 0;
            D_8012C42C = 0;
            D_8012AF58 = 12;
        }
    }
    if (D_8012C39C & 0x40) {
        CONDOR_SFX(0x2B, 0x40, 4);
        D_8012C47C = 1;
        D_8012C480 = D_8012C484 = 1;
        D_8012C3EC = 0;
        D_8012C42C = 0;
        D_8012AF58 = 12;
    }
    if (D_8012C39C & 0x1000) {
        CONDOR_SFX(0x2B, 0x40, 1);
        D_8012C4F4 -= 0x10;
        if (D_8012C4F4 < 0)
            D_8012C4F4 = 0x10;
    }
    if (D_8012C39C & 0x4000) {
        CONDOR_SFX(0x2B, 0x40, 1);
        D_8012C4F4 += 0x10;
        if (D_8012C4F4 > 0x10)
            D_8012C4F4 = 0;
    }
    D_8010BB94[6].y = D_8012C438 + (D_8012C4F4 + 0x4C);
}

int condor_mode_move_cursor(void)
{
    int i;

    if (D_8012C4C0 < D_8012C4C4)
        D_8012C4C0++;
    else
        D_8012C4C0 = D_8012C4C4;
    for (i = 0; i < D_8012C4C0; i++) {
        condor_cursor_walk_step();
        if (!D_8012C3EC) {
            D_8012C41C = D_8012C4E0;
            condor_menu_layout();
            condor_draw_hp();
            if (!D_8012C5CC) {
                D_8012C41C = -1;
                D_8012C428 = 1;
                D_8012C41C = condor_find_units_at(D_8012C3A4);
                condor_open_unit_menu();
            } else if (D_8012C5CC == 1) {
                D_8012C3EC = 5;
                D_800BE8C0[D_8012C424].phase = 1;
                D_8012C3E8 = 1;
            } else {
                D_8012C3EC = 0;
                D_8012C5CC = 0;
                D_8012C42C = 0;
                D_8012C3E8 = 2;
            }
            return 0;
        }
    }
    return 0;
}

int condor_mode_walk_cursor(void)
{
    int i;

    if (D_8012C5C4) {
        D_8010BB94[10].x = -8;
        D_8010BB94[10].y = -0x20 - (D_8012C5C4 >> 2) % 3;
    } else {
        if (D_8012C4C0 < D_8012C4C4)
            D_8012C4C0++;
        else
            D_8012C4C0 = D_8012C4C4;
        for (i = 0; i < D_8012C4C0; i++) {
            if (D_8012C57C == 4)
                D_8012C3EC = 0;
            else
                condor_cursor_walk_step();
            if (!D_8012C3EC) {
                D_8012C47C = 1;
                D_8012C480 = D_8012C484 = 1;
                D_8012C42C = 0;
                D_8012C41C = -1;
                D_8012C428 = 1;
                D_8012C41C = condor_find_units_at(D_8012C3A4);
                condor_draw_hp();
                break;
            }
        }
    }
    return 0;
}

/* The opening: the map fades up from black, then (unless skipped) the banner slides, then the view scrolls down until
 * the cursor is at the bottom. */
int condor_opening(void)
{
    int i;

    D_8012C42C = 1;
    if (!D_8012C5D0) {
        if (D_8012C43C < 0x80) {
            for (i = 0; i < 0x126; i++) {
                (D_8011CBBC[D_8012C3F0].map + i)->r0 = D_8012C43C;
                (D_8011CBBC[D_8012C3F0].map + i)->g0 = D_8012C43C;
                (D_8011CBBC[D_8012C3F0].map + i)->b0 = D_8012C43C;
            }
            D_8012C43C += 2;
        } else {
            D_8012C43C = 0x80;
            if (D_8011CBBC[0].map[0].clut == D_8011CBBC[1].map[0].clut) {
                D_8012C47C = 1;
                D_8012C480 = D_8012C484 = 1;
                D_8012C5D0++;
            }
        }
    } else if (D_8012C5D0 == 1) {
        if (D_8012C404 == 1 || (D_8012C4B8 -= 2) == -24)
            D_8012C5D0++;
    } else {
        condor_move_point(0x4000, D_8012C3A4);
        condor_move_point(0x4000, D_8012C3A4);
        condor_move_point(0x4000, D_8012C3A4);
        if (D_8012C438 >= 0x148) {
            D_8012C3EC = 0;
            D_8012C42C = 0;
        }
    }
}

/* A unit's death cry, panned by where it stands. */
#ifdef NON_MATCHING
/* s0 and s1 swapped between the sound command (kept across the call) and the unit's x. */
void condor_play_death_cry(Unit *u)
{
    unsigned short x;
    int cmd, sfx;

    switch (u->kind) {
    case 19:
        cmd = 0x2A;
        x = u->pos.vx;
        snd_request.id = cmd;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 0;
        snd_submit_request();
        sfx = 0xE3;
        break;
    case 4:
        cmd = 0x2A;
        x = u->pos.vx;
        snd_request.id = cmd;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 0;
        snd_submit_request();
        sfx = 0xE4;
        break;
    case 12:
    case 13:
    case 18:
        cmd = 0x2A;
        x = u->pos.vx;
        snd_request.id = cmd;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 0;
        snd_submit_request();
        sfx = 0xE1;
        break;
    case 17:
        cmd = 0x2A;
        x = u->pos.vx;
        snd_request.id = cmd;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 0;
        snd_submit_request();
        sfx = 0xE8;
        break;
    default:
        cmd = 0x2A;
        x = u->pos.vx;
        snd_request.id = cmd;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 0;
        snd_submit_request();
        sfx = 0xE2;
        break;
    }
    snd_request.id = cmd;
    snd_request.arg[1] = sfx;
    snd_request.arg[0] = (x & 0x1FF) >> 2;
    snd_submit_request();
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_play_death_cry);
#endif

/* One step of unit a's attack on unit b: 0 while it goes on, 1 when it is over, 2 when b has died. */
#ifdef NON_MATCHING
/* The switch value lives in s0 in the game; the damage table's address is built from the data symbol's %hi first;
 * the free popup search's register; the kill path's order. */
short condor_fight_step(int a, int b)
{
    Unit *u = &D_800BE8C0[a];
    Unit *t = &D_800BE8C0[b];
    Unit *e, *shot;
    short k, i, hp, pct;
    int damage;
    unsigned char *table;

    switch (u->phase) {
    case 1:
        u->pose = 2;
        u->act |= 2;
        condor_start_script(u);
        u->phase = 2;
        condor_play_death_cry(u);
        u->link = 0xFF;
        k = condor_spawn_at(u, u->kind);
        if (k == -1)
            break;
        e = &D_800BE8C0[k];
        u->link = k;
        e->pose = 7;
        e->rate = 1;
        e->act_wait = 1;
        e->type = a;
        if (!condor_start_script(e))
            e->flags = 0;
        return 0;
    case 2:
        if (u->hold == 0xFF) {
            u->pose = 0;
            condor_start_script(u);
            u->phase = 3;
            if (!t->flags || t->fade)
                u->phase = 5;
            if (D_8012C3EC != 5)
                return 0;
            if (u->link == 0xFF)
                break;
            D_800BE8C0[u->link].flags = 0;
            return 0;
        }
        if (D_8012C3EC != 5)
            break;
        if (u->link == 0xFF)
            break;
        shot = &D_800BE8C0[u->link];
        if (shot->flags)
            condor_run_anim_script(shot);
        return 0;
    case 3:
        i = free_popup();
        if (i != 0xFF) {
            D_8012C5FC[i].age = 1;
            table = CONDOR_DATA + DATA_HALF(0x18);
            pct = table[u->type * 20 + t->type];
            damage = (unsigned char)(pct ? u->power * pct / 100 : 0);
            D_8012C5FC[i].value = (unsigned char)damage;
            D_8012C5FC[i].x = t->pos.vx;
            D_8012C5FC[i].y = t->pos.vy;
            condor_draw_counter_digits(i, D_8012C5FC[i].value);
        }
        t->act |= 2;
        if (t->pose == 2 || t->pose == 3) {
            u->phase = 5;
            break;
        }
        t->pose = 3;
        condor_start_script(t);
        u->phase = 4;
        break;
    case 4:
        if (t->hold != 0xFF && t->pose == 3)
            return 0;
        if (!(t->pose == 2 || t->pose == 3)) {
            t->pose = 0;
            condor_start_script(t);
        }
        u->phase = 5;
        break;
    case 5:
        u->phase = 0;
        u->damage = 120;
        u->pose = 0;
        condor_start_script(u);
        table = CONDOR_DATA + DATA_HALF(0x18);
        pct = table[u->type * 20 + t->type];
        damage = (unsigned char)(pct ? u->power * pct / 100 : 0);
        hp = t->hp - (unsigned char)damage;
        if (hp < 0)
            hp = 0;
        t->hp = hp;
        if (D_8012C41C != -1)
            condor_draw_hp();
        if (!t->flags || t->fade)
            return 2;
        if (hp) {
            if (!(t->pose == 2 || t->pose == 3)) {
                t->pose = 0;
                condor_start_script(t);
            }
            return 1;
        }
        if (t->kind >= 16) {
            /* an enemy beaten: the banner for it */
            if (D_8012C41C == -1) {
                D_8012C41C = u - D_800BE8C0;
                condor_draw_hp();
                D_8012C41C = -1;
            }
            k = u - D_800BE8C0;
            D_8012C5CC = 11;
            D_8012C5D0 = 0;
            D_8012C4E0 = k;
            D_8012AF58 = 10;
            condor_load_unit_card((short)D_800BE8C0[k].kind);
            D_8012C4E4 = 0;
            D_8012C4F8 = -8;
            D_8012C4F0 = -8;
            D_8012C4FC = 1;
            D_8012C4F4 = 1;
            condor_play_sfx(0, 1);
            condor_play_sfx(0, 2);
            condor_play_sfx(0, 3);
            condor_play_sfx(0, 4);
            D_8012C42C = 1;
            D_8012C5D0 = 0;
            D_8012C4B8 = -32;
            condor_fade_out_start();
            D_8012C3EC = 17;
        }
        if (&D_800BE8C0[u->foe] == t) {
            u->foe = 0xFF;
            u->ty = 0;
            u->tx = 0;
            u->vel.vy = 0;
            u->vel.vx = 0;
            u->act &= 0xFC;
        }
        if (t->fade)
            return 2;
        t->fade = 0xFF;
        if (t->kind < 16)
            D_8012C508--;
        else
            D_8012C50C--;
        condor_load_two_numbers();
        return 2;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_fight_step);
#endif

void condor_fight_end(int a, int b)
{
    D_8012C3EC = 0;
    D_8012C42C = 0;
    D_800BE8C0[a].phase = 0;
    D_800BE8C0[b].phase = 0;
    if (!D_8012C4BC) {
        D_8012C41C = D_8012C424;
        if (D_800BE8C0[D_8012C424].flags && !D_800BE8C0[D_8012C424].fade) {
            D_8012C428 = 0;
            if (condor_role_walks(&D_800BE8C0[D_8012C424])) {
                condor_open_unit_menu();
                D_8012C4BC = 0;
                return;
            }
        }
    } else {
        D_8012C3E8 = 1;
        D_8012C41C = -1;
    }
    D_8012C47C = 1;
    D_8012C480 = D_8012C484 = 1;
    D_8012C4BC = 0;
}

/* One frame of the cursor walk: d-pad presses for each axis whose 1/256ths overflowed. */
void condor_cursor_walk_step(void)
{
    D_8012C3A0 = 0;
    D_8012C55C += D_8012C54C;
    D_8012C560 += D_8012C550;
    if (!D_8012C54C)
        D_8012C554 = 0;
    else if (D_8012C55C & 0xFF00) {
        if (D_8012C564)
            D_8012C3A0 = 0x8000;
        else
            D_8012C3A0 = 0x2000;
        if (D_8012C554)
            D_8012C554--;
        else {
            D_8012C55C = 0;
            D_8012C54C = 0;
        }
    }
    if (!D_8012C550)
        D_8012C558 = 0;
    else if (D_8012C560 & 0xFF00) {
        if (D_8012C568)
            D_8012C3A0 |= 0x1000;
        else
            D_8012C3A0 |= 0x4000;
        if (D_8012C558)
            D_8012C558--;
        else {
            D_8012C560 = 0;
            D_8012C550 = 0;
        }
    }
    D_8012C55C &= 0xFF;
    D_8012C560 &= 0xFF;
    condor_move_cursor(D_8012C3A0);
    if (!((D_8012C554 | D_8012C558) & 0xFFFF))
        D_8012C3EC = 0;
}

/* The step from (x0, y0) toward (x1, y1), for the cursor walk and the units' and shots' aims: per frame along each
 * axis, in 1/256ths, the long axis a whole 0x100, the short one in proportion (at least 1). Non-static inline: the
 * callers after it have it inlined. */
inline void condor_aim(int x0, int y0, int x1, int y1)
{
    D_8012C560 = 0;
    D_8012C55C = 0;
    D_8012C568 = 0;
    D_8012C564 = 0;
    D_8012C554 = x1 - x0;
    if (D_8012C554 < 0) {
        D_8012C564 = 1;
        D_8012C554 = -D_8012C554;
    }
    D_8012C558 = y1 - y0;
    if (D_8012C558 < 0) {
        D_8012C568 = 1;
        D_8012C558 = -D_8012C558;
    }
    if (D_8012C554 >= D_8012C558) {
        D_8012C544 = 0x100;
        if ((D_8012C558 << 8) == 0)
            D_8012C548 = 1;
        else
            D_8012C548 = (D_8012C558 << 8) / D_8012C554;
        if (!D_8012C548)
            D_8012C548 = 1;
    } else {
        D_8012C548 = 0x100;
        if ((D_8012C554 << 8) == 0)
            D_8012C544 = 1;
        else
            D_8012C544 = (D_8012C554 << 8) / D_8012C558;
        if (!D_8012C544)
            D_8012C544 = 1;
    }
    if (D_8012C564)
        D_8012C544 = -D_8012C544;
    if (D_8012C568)
        D_8012C548 = -D_8012C548;
}

/* The inverse of condor_angle_to_square: a point on the square of half-side 0x100 back to its angle. */
#ifdef NON_MATCHING
/* The game ORs the two quadrant flags and sign-extends (this adds them: the same value); the angle sits in v1. */
short condor_square_angle(SVECTOR *v)
{
    short x, y, angle;
    short xneg, yneg;
    xneg = yneg = 0;
    x = v->vx;
    if (x < 0) {
        x = -x;
        xneg = 1;
    }
    y = v->vy;
    if (y < 0) {
        y = -y;
        yneg = 2;
    }
    switch (xneg + yneg) {
    case 0:
        if (x == 0x100)
            angle = (y << 1) + 0xC00;
        else
            angle = 0x1000 - (x << 1);
        break;
    case 1:
        if (y == 0x100)
            angle = (x << 1);
        else
            angle = 0x400 - (y << 1);
        break;
    case 2:
        if (y == 0x100)
            angle = (x << 1) + 0x800;
        else
            angle = 0xC00 - (y << 1);
        break;
    case 3:
        if (x == 0x100)
            angle = (y << 1) + 0x400;
        else
            angle = 0x800 - (x << 1);
        break;
    default:
        angle = 0;
    }
    return angle;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_square_angle);
#endif

/* An angle (0-0xFFF) to a point on the square of half-side 0x100, by its eighth. */
void condor_angle_to_square(unsigned short angle)
{
    int t = (angle & 0x1FF) >> 1;

    switch (angle & 0xE00) {
    case 0x000:
        D_8012C544 = -t;
        D_8012C548 = 0x100;
        break;
    case 0x200:
        D_8012C544 = -0x100;
        D_8012C548 = 0xFF - t;
        break;
    case 0x400:
        D_8012C544 = -0x100;
        D_8012C548 = -t;
        break;
    case 0x600:
        D_8012C544 = t - 0xFF;
        D_8012C548 = -0x100;
        break;
    case 0x800:
        D_8012C544 = t;
        D_8012C548 = -0x100;
        break;
    case 0xA00:
        D_8012C544 = 0x100;
        D_8012C548 = t - 0xFF;
        break;
    case 0xC00:
        D_8012C544 = 0x100;
        D_8012C548 = t;
        break;
    case 0xE00:
        D_8012C544 = 0xFF - t;
        D_8012C548 = 0x100;
        break;
    }
}

/* One frame of the current command mode (D_8012C3EC). */
void condor_update_command(void)
{
    int unit, target, attacker, i;
    short done;
    short result;

    switch (D_8012C3EC) {
    case 1:
        D_8012C3EC = 0;
        break;
    case 2:
        condor_mode_move_cursor();
        break;
    case 3:
        condor_mode_walk_cursor();
        break;
    case 4:
        condor_opening();
        break;
    case 5:
        unit = D_8012C424;
        D_8012C5CC = 0;
        target = D_800BE8C0[unit].foe;
        if (condor_role_walks(&D_800BE8C0[unit]))
            result = condor_fight_step(unit, target);
        else
            result = 1;
        if (result == 2)
            condor_fight_end(unit, target);
        if (result == 1) {
            D_8012C3EC = 6;
            D_800BE8C0[D_800BE8C0[D_8012C424].foe].phase = 1;
        }
        break;
    case 6:
        attacker = D_8012C424;
        unit = D_800BE8C0[attacker].foe;
        if (condor_role_walks(&D_800BE8C0[attacker])) {
            result = condor_fight_step(unit, attacker);
            if (result == 2)
                condor_fight_end(unit, attacker);
            if (result != 1)
                break;
        }
        condor_fight_end(unit, attacker);
        break;
    case 7:
        condor_shop_update();
        break;
    case 8:
        condor_aim_tower();
        break;
    case 9:
        condor_free_scroll();
        break;
    case 10:
        condor_yes_no();
        break;
    case 11:
        done = ++D_8012C5D0 > 600;
        if (++D_8012C4B8 >= 0x40) {
            D_8012C4B8 = 0x40;
            if (D_8012C39C & 0x60)
                done++;
        }
        if (done)
            D_8012C408 = 4;
        break;
    case 12:
        if (D_8012C5D0 < 360) {
            D_8012C5D0++;
            if (++D_8012C4B8 >= 0x40)
                D_8012C4B8 = 0x40;
        } else
            D_8012C408 = 5;
        break;
    case 13:
        if (D_8012C4C0 < D_8012C4C4)
            D_8012C4C0++;
        else
            D_8012C4C0 = D_8012C4C4;
        for (i = 0; i < D_8012C4C0; i++)
            condor_cursor_walk_step();
        break;
    case 14:
        condor_reload_map_image();
        break;
    case 16:
        condor_aim_unit();
        break;
    case 17:
        D_8012C4B8 += 4;
        if (D_8012C4B8 >= D_8012C4DC) {
            D_8012C4B8 = D_8012C4DC;
            D_8012C3EC = 0;
        }
        break;
    }
}

/* The angle from a to b on the cursor's square, starting the cursor walk between them on the way. */
short condor_angle_between(SVECTOR *a, SVECTOR *b)
{
    SVECTOR step;

    condor_aim(a->vx, a->vy + 0x4000, b->vx, b->vy + 0x4000);
    step.vx = D_8012C544;
    step.vy = D_8012C548;
    step.vz = 0;
    return condor_square_angle(&step);
}

short condor_outside_cell(Cell *t, short x, short y)
{
    SVECTOR pt;

    pt.vx = x;
    pt.vy = y;
    pt.vz = 0;
    D_8012C5A4 = condor_angle_between(&t->scr[0], &pt);
    D_8012C5A8 = condor_angle_between(&t->scr[0], &t->scr[1]);
    D_8012C5AC = condor_angle_between(&t->scr[0], &t->scr[2]) + 8;
    if (condor_outside_arc(D_8012C5A4, D_8012C5A8, D_8012C5AC))
        return 1;
    D_8012C5A4 = condor_angle_between(&t->scr[1], &pt);
    D_8012C5A8 = condor_angle_between(&t->scr[1], &t->scr[2]);
    D_8012C5AC = condor_angle_between(&t->scr[1], &t->scr[0]);
    return condor_outside_arc(D_8012C5A4, D_8012C5A8, D_8012C5AC);
}

short condor_find_cell(Unit *u)
{
    Cell *t = D_8010C33C;
    short i = 0;
    short x = u->scene.vx;
    short y = u->scene.vy;
    short bx = x + 0x4000;
    short by = y + 0x4000;

    for (; i < D_8012C518; i++, t++) {
        if (bx < t->x0 || t->x1 < bx || by < t->y0 || t->y1 < by)
            continue;
        if (condor_outside_cell(t, x, y))
            continue;
        u->rot.vx = t->rot_x;
        u->rot.vz = t->rot_z;
        return i;
    }
    return -1;
}

short condor_update_cell(Unit *u)
{
    short i;

    if ((i = condor_on_cell(D_8010C33C[u->cell].next[0], u->scene.vx, u->scene.vy)) != -1)
        return i;
    if ((i = condor_on_cell(D_8010C33C[u->cell].next[1], u->scene.vx, u->scene.vy)) != -1)
        return i;
    if ((i = condor_on_cell(D_8010C33C[u->cell].next[2], u->scene.vx, u->scene.vy)) != -1)
        return i;
    return condor_find_cell(u);
}

#ifdef NON_MATCHING
/* GCC turns the open-edge test into a store-flag sequence where the game branches, and sign-extends the angle once
 * more than the game. */
short condor_turn_at_edge(Unit *u)
{
    SVECTOR here, there;
    Cell *t = &D_8010C33C[u->cell];
    short angle;
    short edge;
    int turn;

    here.vx = u->scene.vx;
    here.vy = u->scene.vy;
    here.vz = 0;
    there.vx = D_8012C59C.vx;
    there.vy = D_8012C59C.vy;
    there.vz = 0;
    if (t->next[0] == -1)
        edge = 1;
    else if (t->next[1] == -1)
        edge = 2;
    else if (t->next[2] != -1)
        edge = 0;
    else
        edge = 3;
    switch (edge) {
    case 1:
        angle = condor_angle_between(&t->scr[0], &t->scr[1]);
        break;
    case 2:
        angle = condor_angle_between(&t->scr[1], &t->scr[2]);
        break;
    case 3:
        angle = condor_angle_between(&t->scr[2], &t->scr[0]);
        break;
    }
    if (!edge)
        return 0;
    turn = 0x1000 - (condor_square_angle(&u->vel) & 0xFFF);
    if (((turn + angle + 0x400) & 0xFFF) >= 0x800)
        angle = ((angle + 0x800) & 0xFFF) - 0x40;
    else
        angle += 0x40;
    condor_angle_to_square(angle);
    u->vel.vx = D_8012C544;
    u->vel.vy = D_8012C548;
    return 1;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_turn_at_edge);
#endif

/* Which of the directions in want (1 up, 2 down, 4 left, 8 right) a can step without bumping into b. */
#ifdef NON_MATCHING
/* Register allocation only (the game's ay/bx/nx in t4/t3/t2), and two delay-slot fills. */
short condor_clear_dirs(Unit *a, Unit *b, short want)
{
    int ny = want & 3;
    int nx = want & 0xC;
    short ax = a->pos.vx;
    short ay = a->pos.vy;
    short bx = b->pos.vx;
    short by = b->pos.vy;
    int w = (unsigned)(a->w + b->w) >> 1;
    int ad = a->h;
    int bd = b->h;

    switch (want) {
    case 1:
        if (ay > by && by == ay - ad && condor_overlap(ax, bx, w, w))
            ny = 0;
        else
            ny = 1;
        break;
    case 2:
        if (ay < by && ay == by - bd && condor_overlap(ax, bx, w, w))
            ny = 0;
        else
            ny = 2;
        break;
    case 4:
        if (ax > bx && bx == ax - w && condor_overlap(ay, by, ad, bd))
            ny = 0;
        else
            ny = 4;
        break;
    case 8:
        if (ax < bx && ax == bx - w && condor_overlap(ay, by, ad, bd))
            ny = 0;
        else
            ny = 8;
        break;
    case 5:
        if (ay > by && by == ay - ad && condor_overlap(ax, bx, w, w))
            ny = 0;
        else
            ny = 1;
        if (ax > bx && bx == ax - w && condor_overlap(ay, by, ad, bd))
            nx = 0;
        else
            nx = 4;
        break;
    case 9:
        if (ay > by && by == ay - ad && condor_overlap(ax, bx, w, w))
            ny = 0;
        else
            ny = 1;
        if (ax < bx && ax == bx - w && condor_overlap(ay, by, ad, bd))
            nx = 0;
        else
            nx = 8;
        break;
    case 6:
        if (ay < by && ay == by - bd && condor_overlap(ax, bx, w, w))
            ny = 0;
        else
            ny = 2;
        if (ax > bx && bx == ax - w && condor_overlap(ay, by, ad, bd))
            nx = 0;
        else
            nx = 4;
        break;
    case 10:
        if (ay < by && ay == by - bd && condor_overlap(ax, bx, w, w))
            ny = 0;
        else
            ny = 2;
        if (ax < bx && ax == bx - w && condor_overlap(ay, by, ad, bd))
            nx = 0;
        else
            nx = 8;
        break;
    }
    return ny | nx;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_clear_dirs);
#endif

/* Slide u along whatever other units leave clear; whether it moved (0x80: free to move as it likes). */
#ifdef NON_MATCHING
/* The return-0 tests: GCC threads them to the epilogue, the game jumps to a shared `return 0` block. */
short condor_slide_unit(Unit *u)
{
    short hits = 0;
    short clear = 0;
    int want = 0;
    short i;

    if (u->vel.vy)
        want = u->vel.vy < 0 ? 1 : 2;
    if (u->vel.vx)
        want |= u->vel.vx < 0 ? 4 : 8;
    for (i = 0; i < D_8012B85C; i++) {
        clear |= condor_clear_dirs(u, D_8012B75C[i].unit, want);
        if (clear)
            hits++;
    }
    if (!clear && !hits) {
        if (!(u->flags & 8))
            return 0x80;
        clear = u->clear;
    }
    if ((clear & 3) && (clear & 0xC))
        return 0;
    if ((short)want == clear)
        return 0;
    i = 1; /* now whether it moved */
    switch (clear) {
    case 1:
    case 2:
        if (u->vel.vy < 0) {
            u->pos.vy--;
            u->scene.vy--;
        }
        if (u->vel.vy > 0) {
            u->pos.vy++;
            u->scene.vy++;
        }
        break;
    case 4:
    case 8:
        if (u->vel.vx < 0) {
            u->pos.vx--;
            u->scene.vx--;
        }
        if (u->vel.vx > 0) {
            u->pos.vx++;
            u->scene.vx++;
        }
        break;
    case 0:
    case 3:
    case 5:
    case 6:
    case 7:
    default:
        i = 0;
        break;
    }
    if (i) {
        u->clear = clear;
        u->flags |= 8;
    }
    return i;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_slide_unit);
#endif

/* Opens the unit list: lays the units out as icons in a panel by the cursor, then runs it until it closes. */
#ifdef NON_MATCHING
/* The entry and unit pointers swap t4/t5; the tail's stores of D_8012C47C/484/480 come after the new index. */
void condor_open_unit_list(void)
{
    Found *r = D_8012B75C;
    Pick *e = D_8012BA88;
    short count;
    short half;
    short left;
    short top;
    short x, y, x0;
    short i;
    Unit *u;

    D_8012C014 = 0;
    D_8012C010 = 0;
    D_8012C00C = 0;
    D_8012C008 = count = D_8012B85C;
    if (count >= 8) {
        half = 0x60;
        D_8012C4D4 = 0x100;
        D_8012C4D8 = (short)(count / 7) * 32 + 0x30;
    } else {
        half = count / 2 * 32;
        D_8012C4D4 = count * 32 + 0x20;
        if (!(count & 1)) {
            half -= 0x10;
            D_8012C4D4 = count * 32 + 0x10;
        }
        D_8012C4D8 = 0x30;
    }
    if (D_8012C3E8 != 3) {
        left = D_8012C3A4[0];
        top = D_8012C3A4[1];
    } else {
        left = D_8012C3AC[0];
        top = D_8012C3AC[1];
    }
    /* Keep the panel on screen. */
    if (left - D_8012C434 - half <= 0x40)
        left = half + D_8012C434 + 0x40;
    else if (left + D_8012C4D4 - D_8012C434 - half >= 0x110)
        left = half + (D_8012C434 + 0x140 - D_8012C4D4) - 8;
    x = left - (short)(half + 0x100);
    y = top - 0x1F0;
    D_8012C4CC = left - half - 0x10;
    D_8012C4D0 = top - 0x10;
    x0 = x;
    for (i = 0; i < D_8012C008; e++, r++, i++) {
        u = r->unit;
        if (u - D_800BE8C0 == D_8012C41C)
            D_8012C00C = i;
        e->pos.vx = e->home.vx = u->scene.vx;
        e->pos.vy = e->home.vy = u->scene.vy;
        e->pos.vz = e->home.vz = u->scene.vz;
        e->index = u - D_800BE8C0;
        e->unit = r->unit;
        e->x20 = u->rot.vy;
        e->x22 = u->dir;
        condor_aim(u->pos.vx, u->pos.vy, x + 0x100, y + 0x200);
        e->frac_y = 0;
        e->frac_x = 0;
        e->to.vx = x;
        x += 0x20;
        e->step_x = D_8012C544;
        e->step_y = D_8012C548;
        e->to.vy = y;
        u->scene.vz = D_8012C388 - 0x50;
        u->rot.vy = 0;
        if (i == 6 || i == 13) {
            x = x0;
            y += 0x20;
        }
    }
    D_8012C3EC = 15;
    D_8012C42C = 1;
    if (D_8011CBBC[D_8012C3F0].map[0].r0 > 80)
        D_8012C488 = 1;
    else
        D_8012C488 = 3;
    D_8012C47C = 2;
    D_8012C484 = 1;
    D_8012C480 = 1;
    D_8012C018 = D_8012C41C;
    D_8012C41C = D_8012BA88[D_8012C00C].unit - D_800BE8C0;
    condor_load_unit_card((short)D_800BE8C0[D_8012C41C].kind);
    condor_draw_hp();
    while (!D_8012C014) {
        condor_read_pad(&D_8012C67C, &D_8012C69C);
        condor_flip_buffers();
        condor_run_unit_list();
        condor_animate_troops();
        condor_draw_frame();
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_open_unit_list);
#endif

int condor_open_unit_menu(void)
{
    if (D_8012C404 == 1)
        return 1;
    if (D_8012C41C == -1)
        return 0;
    condor_draw_hp();
    D_8012C3E8 = 2;
    D_8012C4E4 = 0;
    D_8012C4F8 = -8;
    D_8012C4F0 = -8;
    D_8012C4FC = 1;
    D_8012C4F4 = 1;
    if (D_8011CBBC[D_8012C3F0].map[0].r0 > 80)
        D_8012C488 = 1;
    else
        D_8012C488 = 3;
    D_8012C47C = 2;
    D_8012C484 = 1;
    D_8012C480 = 1;
    condor_menu_layout();
    return 0;
}

int condor_check_placement(void)
{
    SVECTOR pos;

    if (D_8012C3EC || D_8012C5CC || D_8012C41C != -1 || D_8012C508 >= 20)
        return 1;
    if (D_8012C404 == 1) {
        if (D_8012C3A4[1] >= 0x2A0)
            return 1;
    } else if (D_8012C3A4[1] >= D_8012C530) {
        return 1;
    }
    pos.vz = -40;
    pos.vx = D_8012C3A4[0];
    pos.vy = 0x400 - D_8012C3A4[1];
    if (condor_place_is_free(0, (unsigned short *)&pos) == 1)
        return 1;
    D_8012C5C0 = 1;
    return 0;
}

void condor_clear_selection(void)
{
    D_8012C41C = -1;
    D_8012C524 = 0x100;
    D_8012C528 = 0;
}

void condor_switch_screen(void)
{
    DMA_BUFFER(black, 512);
    short i;

    for (i = 0; i < 256; i++)
        ((short *)black)[i] = 0;
    snd_request.id = 0x2B;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 1;
    snd_submit_request();
    snd_request.id = 0x28;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0;
    snd_submit_request();
    snd_request.id = 0x29;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0;
    snd_submit_request();
    snd_request.id = 0x2A;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0;
    snd_submit_request();

    /* Keep the battlefield screen and its CLUT row in RAM, then black the row out. */
    D_8012AF3C.x = 0x140;
    D_8012AF3C.y = 0;
    D_8012AF3C.w = 0xA0;
    D_8012AF3C.h = 0xDC;
    D_8012C458 = D_8012C434;
    D_8012C45C = D_8012C438;
    func_80044064(&D_8012AF3C, (void *)0x80150220);
    DrawSync(0);
    D_8012AF3C.x = 0;
    D_8012AF3C.y = 0x1F0;
    D_8012AF3C.w = 0x100;
    D_8012AF3C.h = 1;
    func_80044064(&D_8012AF3C, (void *)0x80150014);
    DrawSync(0);
    D_8012AF3C.x = 0;
    D_8012AF3C.y = 0x1F0;
    D_8012AF3C.w = 0x100;
    D_8012AF3C.h = 1;
    func_80044000(&D_8012AF3C, black);
    DrawSync(0);
    condor_draw_shaded(0x1F8, 0x80);
    condor_draw_shaded(0x1F8, 0x80);
    condor_draw_shaded(0x1F8, 0x80);

    D_8012C3EC = 14;
    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    D_8012C438 = 6;
    D_8012C434 = 0;
    D_8012AF3C.x = 0x140;
    D_8012AF3C.y = 0;
    D_8012AF3C.w = 0xA0;
    D_8012AF3C.h = 0xDC;
    func_80044000(&D_8012AF3C, (void *)0x80168220);
    DrawSync(0);
    D_8012AF3C.x = 0;
    D_8012AF3C.y = 0x1F0;
    D_8012AF3C.w = 0x100;
    D_8012AF3C.h = 1;
    func_80044000(&D_8012AF3C, (void *)0x80168014);
    DrawSync(0);
    condor_draw_shaded(0x1F0, 0xA0);
    condor_draw_shaded(0x1F0, 0xA0);
    condor_draw_shaded(0x1F0, 0xA0);
    VSync(0);
    SetDispMask(1);
    DMA_BUFFER_END(512);
}

/* One frame of the battle's input: reads the pad and acts on it by mode (D_8012C3E8). */
#ifdef NON_MATCHING
/* The cursor snaps: the game keeps &D_8012C3A4/&D_8012C3C4 in registers and reaches [1] as 2(reg); some store
 * orders (the 0x50 block's fade stores, case 3's aim operands), and D_8012C57C's stores shared at a join. */
int condor_read_pad(MATRIX *m, MATRIX *unused)
{
    unsigned int held;
    int ret;
    SVECTOR pos;
    Unit *u;

    held = func_8001C808();
    D_8012C390 = held;
    D_8012C39C = (held ^ D_8012C394) & held;
    D_8012C394 = held;
    D_8012C398 = 0;
    ret = 0;
    if (D_8012C408)
        ret = D_8012C408;
    if (held & 0xF00F) {
        if (D_8012C39C & 0xF00F) {
            D_8012C4E8 = 10;
            D_8012C398 = D_8012C39C;
        }
        if (--D_8012C4E8 < 0) {
            D_8012C4E8 = 3;
            D_8012C398 = held & 0xF00F;
        }
    } else {
        D_8012C4E8 = 10;
    }
    if (D_8012C3EC) {
        condor_update_command();
        goto done;
    }

    if (D_8012C5CC) {
        if (D_8012C39C & 0xA0) {
            if (D_8012C39C & 0x20)
                D_8012C4BC = 0;
            else
                D_8012C4BC = 1;
            u = &D_800BE8C0[D_8012C4E0];
            condor_aim(D_8012C3A4[0], D_8012C3A4[1], u->pos.vx, u->pos.vy);
            D_8012C3EC = 2;
            D_8012C4C0 = 1;
            D_8012C4C4 = 0x10;
            D_8012C54C = D_8012C544;
            D_8012C550 = D_8012C548;
        }
        if (D_8012C39C & 0x50) {
            D_8012C47C = 1;
            D_8012C484 = 1;
            D_8012C480 = 1;
            D_8012C42C = 0;
            D_8012C5CC = 0;
            condor_load_unit_card((short)D_800BE8C0[D_8012C41C].kind);
        }
        goto done;
    }

    if (D_8012C39C & 0x20) {
        switch (D_8012C3E8) {
        case 3:
            if (D_8012C5C0) {
                D_8012C5C0 = 0;
                CONDOR_SFX(0x2B, 0x40, 1);
                D_8012C010 = 0;
                condor_order_move();
                if (D_8012C010 == 2)
                    break;
                D_8012C41C = -1;
                D_8012C42C = 1;
                D_8012C5C4 = 30;
                D_8010BB94[10].x = 0xFFF8;
                D_8010BB94[10].y = 0xFFE7;
                D_8012C524 = 0x100;
                D_8012C528 = 0;
                D_8012C3EC = 3;
                D_8012C3E8 = 1;
                condor_fly_cursor(D_8012C3AC[0], D_8012C3AC[1], D_8012C3A4[0], D_8012C3A4[1]);
            } else {
                CONDOR_SFX(0x2B, 0x40, 3);
            }
            break;
        case 2:
            CONDOR_SFX(0x2B, 0x40, 1);
            CONDOR_SFX(0x28, 0x40, 0);
            CONDOR_SFX(0x29, 0x40, 0);
            CONDOR_SFX(0x2A, 0x40, 0);
            condor_do_unit_order();
            break;
        case 1:
            if (D_8012C41C != -1) {
                CONDOR_SFX(0x2B, 0x40, 1);
                condor_find_units_at(D_8012C3A4);
                if (!D_8012B85C)
                    break;
                if (D_8012B85C == 1) {
                    if (D_800BE8C0[D_8012C41C].kind < 16 && D_8012C404 != 1 && D_8012C41C != -1) {
                        condor_draw_hp();
                        D_8012C3E8 = 2;
                        D_8012C4E4 = 0;
                        D_8012C4F8 = -8;
                        D_8012C4F0 = -8;
                        D_8012C4FC = 1;
                        D_8012C4F4 = 1;
                        if (D_8011CBBC[D_8012C3F0].map[0].r0 > 80)
                            D_8012C488 = 1;
                        else
                            D_8012C488 = 3;
                        D_8012C47C = 2;
                        D_8012C484 = 1;
                        D_8012C480 = 1;
                        condor_menu_layout();
                    }
                } else {
                    condor_open_unit_list();
                }
            } else {
                D_8012C498 = 0xC0;
                if (D_8012C5C0)
                    D_8012C464++;
                else
                    CONDOR_SFX(0x2B, 0x40, 3);
            }
            break;
        }
    }

    if (D_8012C39C & 0x40) {
        if (D_8012C404 == 1) {
            CONDOR_SFX(0x2B, 0x40, 1);
            D_8012C3EC = 10;
            D_8012C42C = 1;
            D_8012C41C = -1;
            if (D_8011CBBC[D_8012C3F0].map[0].r0 > 80)
                D_8012C488 = 1;
            else
                D_8012C488 = 3;
            D_8012C47C = 2;
            D_8012C484 = 1;
            D_8012C480 = 1;
            D_8012AF3C.x = 0x3C0;
            D_8012AF3C.y = 0xE0;
            D_8012AF3C.w = 0x40;
            D_8012AF3C.h = D_8012C4F4 = 0x10;
            D_8010BB94[6].x = D_8012C434 + 0x30;
            D_8010BB94[6].y = D_8012C438 + 0x5C;
            func_80044000(&D_8012AF3C, (void *)0x80196840);
            condor_load_unit_card(-1, 0);
        } else {
            switch (D_8012C3E8) {
            case 2:
                CONDOR_SFX(0x2B, 0x40, 4);
                D_8012C3E8 = 1;
                D_8012C47C = 1;
                D_8012C484 = 1;
                D_8012C480 = 1;
                break;
            case 3:
                CONDOR_SFX(0x2B, 0x40, 4);
                D_8012C3EC = 2;
                condor_fly_cursor(D_8012C3AC[0], D_8012C3AC[1], D_8012C3A4[0], D_8012C3A4[1]);
                break;
            }
        }
    }

    if (D_8012C3E8 < 2) {
        /* Picking a unit with the cursor. */
        D_8012C5C0 = 0;
        if (D_8012C390 & 0xF000) {
            condor_cursor_repeat();
            if (D_8012C390 & 0x80)
                condor_cursor_repeat();
            condor_check_placement();
            D_8012C41C = -1;
            D_8012C524 = 0x100;
            D_8012C528 = 0;
            D_8012C428 = 1;
        } else {
            D_8012C454 = 0;
            if (D_8012C428 == 1) {
                D_8012C428 = 0;
                D_8012C41C = condor_find_units_at(D_8012C3A4);
                condor_draw_hp();
                D_8012C3C4[1] = D_8012C3A4[1];
                D_8012C3C4[0] = D_8012C3A4[0];
                if (D_8012C41C != -1) {
                    if (!D_800BE8C0[D_8012C41C].flags)
                        D_8012C41C = -1;
                    if (D_8012C41C != -1) {
                        if (D_8012C41C >= 20)
                            D_8012C51C = 1;
                        else
                            D_8012C51C = 0;
                        condor_snap_cursor();
                    }
                }
            }
            condor_check_placement();
            if (D_8012C41C != -1) {
                D_8012C3C4[0] = D_8012C3A4[0];
                D_8012C3C4[1] = D_8012C3A4[1];
                condor_snap_cursor();
            }
        }
    } else if (D_8012C3E8 == 2) {
        /* Choosing from the unit's menu. */
        if (D_8012C398 & 0x1000) {
            CONDOR_SFX(0x2B, 0x40, 1);
            if (!D_8012C4E4) {
                D_8012C4E4 = D_8012C56C;
                D_8012C4F4 = D_8012C4FC + D_8012C56C * 16;
            }
            D_8012C4E4--;
            D_8012C4F4 -= 0x10;
        }
        if (D_8012C398 & 0x4000) {
            CONDOR_SFX(0x2B, 0x40, 1);
            if (D_8012C4E4 == D_8012C56C - 1) {
                D_8012C4E4 = -1;
                D_8012C4F4 = D_8012C4FC - 0x10;
            }
            D_8012C4E4++;
            D_8012C4F4 += 0x10;
        }
    } else {
        /* Picking a target for the selected unit. */
        D_8012C5C0 = 0;
        if (D_8012C390 & 0xF000) {
            condor_cursor_repeat();
            if (D_8012C390 & 0x80)
                condor_cursor_repeat();
            D_8012C57C = 3;
        } else {
            D_8012C454 = 0;
            D_8012C420 = condor_find_units_at(D_8012C3AC);
            if (D_8012C420 == -1) {
                D_8012C57C = 3;
            } else {
                D_8012C3AC[0] = D_800BE8C0[D_8012C420].pos.vx;
                D_8012C3AC[1] = D_800BE8C0[D_8012C420].pos.vy;
                if (D_8012C41C == D_8012C420)
                    D_8012C57C = 4;
                else if (D_800BE8C0[D_8012C420].kind >= 16)
                    D_8012C57C = 0;
                else if (D_800BE8C0[D_8012C41C].kind == 12)
                    D_8012C57C = 2;
                else
                    D_8012C57C = 3;
            }
        }
        if (!D_8012C3EC) {
            pos.vz = -40;
            pos.vx = D_8012C3AC[0];
            pos.vy = 0x400 - D_8012C3AC[1];
            if (condor_ground_is_free(0, (unsigned short *)&pos) != 1)
                D_8012C5C0 = 1;
        }
    }

    if ((D_8012C39C & 0x800) && !D_8012C3EC && D_8012C404 != 1) {
        CONDOR_SFX(0x2B, 0x40, 1);
        D_8012C3EC = 9;
        D_8012C47C = 3;
        D_8012C484 = 1;
        D_8012C480 = 1;
        D_8012C48C = D_8012C488;
        D_8012C410 = D_8012C40C;
        condor_load_two_numbers();
        CONDOR_SFX(0x28, 0x40, 0);
        CONDOR_SFX(0x29, 0x40, 0);
        CONDOR_SFX(0x2A, 0x40, 0);
        D_8012C44C = D_8012C3A4[0] - D_8012C434;
        D_8012C450 = D_8012C3A4[1] - D_8012C438;
        D_8012C3A4[0] = D_8012C434 + 0xA0;
        D_8012C3A4[1] = D_8012C438 + 0x68;
    }
    if ((D_8012C39C & 0x100) && D_8012C3E8 < 2 && !D_8012C3EC && D_8012C404 != 1)
        condor_switch_screen();
    if (D_8012C39C & 1) {
        CONDOR_SFX(0x2B, 0x40, 1);
        if (D_8012C5D4 >= 2)
            D_8012C5D4--;
    }
    if (D_8012C39C & 2) {
        CONDOR_SFX(0x2B, 0x40, 1);
        if (D_8012C5D4 < 4)
            D_8012C5D4++;
    }
    if (D_8012C39C & 4) {
        CONDOR_SFX(0x2B, 0x40, 1);
        if (D_8012C5D4 >= 2)
            D_8012C5D4--;
    }
    if (D_8012C39C & 8) {
        CONDOR_SFX(0x2B, 0x40, 1);
        if (D_8012C5D4 < 4)
            D_8012C5D4++;
    }
done:
    SetRotMatrix(m);
    SetTransMatrix(m);
    return ret;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_read_pad);
#endif

void condor_menu_layout(void)
{
    switch (D_800BE8C0[D_8012C41C].role) {
    case 0:
        D_8012C56C = 1;
        D_8012C570 = 2;
        break;
    case 3:
    case 4:
        D_8012C56C = 2;
        D_8012C570 = 5;
        D_8012C574 = 2;
        break;
    case 1:
    case 2:
    case 5:
        D_8012C56C = 1;
        D_8012C570 = 3;
        break;
    case 6:
        D_8012C56C = 2;
        D_8012C570 = 3;
        D_8012C574 = 0;
        break;
    case 7:
    default:
        D_8012C56C = 0;
        break;
    }
}

void condor_move_cursor(int btn)
{
    short *cursor = D_8012C3A4;

    if (!D_8012C5CC)
        cursor = D_8012C3AC;
    if (D_8012C3E8 != 3 && !D_8012C3EC)
        cursor = D_8012C3A4;
    condor_move_point(btn, cursor);
}

void condor_move_point(int btn, short *pos)
{
    short p[2];
    int clamped;

    p[0] = pos[0] - D_8012C434;
    p[1] = pos[1] - D_8012C438;
    if (btn & 0x1000) {
        clamped = 0;
        if (p[1] >= 0x69) {
            clamped = 1;
        } else {
            D_8012C438--;
            if (D_8012C438 > 0x8000) {
                D_8012C438 = 0;
                clamped = 1;
            } else {
                D_8012C6EC.vy++;
            }
        }
        if (clamped && --p[1] < 0)
            p[1] = 0;
    }
    if (btn & 0x4000) {
        clamped = 0;
        if (p[1] < 0x68) {
            clamped = 1;
        } else {
            D_8012C438++;
            if (D_8012C438 > 0x324) {
                D_8012C438 = 0x324;
                clamped = 1;
            } else {
                D_8012C6EC.vy--;
            }
        }
        if (clamped && ++p[1] >= 0xCC)
            p[1] = 0xCC;
    }
    if (btn & 0x8000) {
        clamped = 0;
        if (p[0] >= 0xA1) {
            clamped = 1;
        } else {
            D_8012C434--;
            if (D_8012C434 > 0x8000) {
                clamped = 1;
                D_8012C434 = 0;
                D_8012C6EC.vx &= ~7;
            } else {
                D_8012C6EC.vx++;
            }
        }
        if (clamped && --p[0] < 0)
            p[0] = 0;
    }
    if (btn & 0x2000) {
        clamped = 0;
        if (p[0] < 0xA0) {
            clamped = 1;
        } else {
            D_8012C434++;
            if (D_8012C434 > 0xC0) {
                clamped = 1;
                D_8012C434 = 0xC0;
                D_8012C6EC.vx &= ~7;
            } else {
                D_8012C6EC.vx--;
            }
        }
        if (clamped && ++p[0] >= 0x140)
            p[0] = 0x140;
    }
    pos[0] = p[0] + D_8012C434;
    pos[1] = p[1] + D_8012C438;
}

#ifdef NON_MATCHING
/* In the x clamp the game loads D_8012C6EC after the store to D_8012C434. */
void condor_scroll_camera(int buttons)
{
    if (buttons & 0x1000) {
        D_8012C438--;
        if (D_8012C438 > 0x8000) {
            D_8012C438 = 0;
        } else {
            int *p = &D_8012C6EC.vy;
            (*p)++;
        }
    }
    if (buttons & 0x4000) {
        D_8012C438++;
        if (D_8012C438 > 0x324) {
            D_8012C438 = 0x324;
        } else {
            int *p = &D_8012C6EC.vy;
            (*p)--;
        }
    }
    if (buttons & 0x8000) {
        D_8012C434--;
        if (D_8012C434 > 0x8000) {
            int *p = &D_8012C6EC.vx;
            D_8012C434 = 0;
            *p &= ~7;
        } else {
            int *p = &D_8012C6EC.vx;
            (*p)++;
        }
    }
    if (buttons & 0x2000) {
        D_8012C434++;
        if (D_8012C434 > 0xC0) {
            int *p = &D_8012C6EC.vx;
            D_8012C434 = 0xC0;
            *p &= ~7;
        } else {
            int *p = &D_8012C6EC.vx;
            (*p)--;
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_scroll_camera);
#endif

/* Builds tower i's model from unit u's frame: tower 0 in the unit's red, the others a dim red. */
#ifdef NON_MATCHING
/* Register allocation only, and the operand order of one addu in the frame lookup. */
void condor_build_tower_model(Unit *u, short i)
{
    unsigned char *models = MODELS;
    unsigned char *frame, *p, *verts, *v;
    ModelFace *src;
    TowerFace *dst;
    POLY_GT3 *prim;
    int count, n;

    dst = D_80107C28[i].faces;
    p = models + (*(int *)(models + u->frame * 4 + (((unsigned short *)models)[u->kind] & ~3)) >> 1 << 1);
    frame = p;
    count = *p;
    p += 2;
    verts = *(unsigned short *)p + frame;
    p += 2;
    D_8012C368[i].faces = dst;
    D_80107C28[i].count = count;
    D_8012C368[i].count = count;
    src = (ModelFace *)p;
    for (n = 0; n < count; n++, src++, dst++) {
        if (!i) {
            prim = &dst->prim[D_8012C3F0];
            prim->r0 = prim->r1 = prim->r2 = u->fade;
            prim->g0 = prim->b0 = prim->g1 = prim->b1 = prim->g2 = prim->b2 = 0;
        } else {
            dst->prim[0].r0 = dst->prim[0].r1 = dst->prim[0].r2 = 0x40;
            dst->prim[0].g0 = dst->prim[0].b0 = dst->prim[0].g1 =
                dst->prim[0].b1 = dst->prim[0].g2 = dst->prim[0].b2 = 0;
            dst->prim[1].r0 = dst->prim[1].r1 = dst->prim[1].r2 = 0x40;
            dst->prim[1].g0 = dst->prim[1].b0 = dst->prim[1].g1 =
                dst->prim[1].b1 = dst->prim[1].g2 = dst->prim[1].b2 = 0;
        }
        v = verts + src->v[0] * 6;
        dst->v[0].vx = ((short *)v)[0];
        dst->v[0].vy = ((short *)v)[1];
        dst->v[0].vz = ((short *)v)[2];
        v = verts + src->v[1] * 6;
        dst->v[1].vx = ((short *)v)[0];
        dst->v[1].vy = ((short *)v)[1];
        dst->v[1].vz = ((short *)v)[2];
        v = verts + src->v[2] * 6;
        dst->v[2].vx = ((short *)v)[0];
        dst->v[2].vy = ((short *)v)[1];
        dst->v[2].vz = ((short *)v)[2];
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_build_tower_model);
#endif

/* Builds unit u's model from its frame and queues it. */
#ifdef NON_MATCHING
/* Register allocation; in grey-blue the game reloads the brighter channel instead of keeping it. */
void condor_build_unit_model(Unit *u)
{
    unsigned char *models = MODELS;
    unsigned char *frame, *p, *verts, *v;
    ModelFace *src;
    UnitFace *dst;
    POLY_G3 *prim;
    int count, n, c;
    int slot = u->slot;

    p = models + (*(int *)(models + u->frame * 4 + (((unsigned short *)models)[u->kind] & ~3)) >> 1 << 1);
    frame = p;
    dst = D_800C0990[slot].faces;
    count = *p;
    p += 2;
    verts = *(unsigned short *)p + frame;
    src = (ModelFace *)(p + 2);
    D_8012C01C[D_8012C364].faces = dst;
    D_800C0990[slot].count = count;
    D_8012C01C[D_8012C364].src = src;
    D_8012C01C[D_8012C364].count = count;
    if (D_8012C4C8)
        D_8012C01C[D_8012C364].count = count | 0x8000;
    D_8012C364++;
    for (n = 0; n < count; n++, src++, dst++) {
        prim = &dst->prim[D_8012C3F0];
        if (!D_8012C4C8) {
            prim->r0 = src->rgb[0][0];
            prim->g0 = src->rgb[0][1];
            prim->b0 = src->rgb[0][2];
            prim->r1 = src->rgb[1][0];
            prim->g1 = src->rgb[1][1];
            prim->b1 = src->rgb[1][2];
            prim->r2 = src->rgb[2][0];
            prim->g2 = src->rgb[2][1];
            prim->b2 = src->rgb[2][2];
        } else {
            /* Grey-blue: each corner's brightest channel. */
            c = src->rgb[0][0];
            if (c < src->rgb[0][1])
                c = src->rgb[0][1];
            if (c < src->rgb[0][2])
                c = src->rgb[0][2];
            prim->r0 = prim->g0 = c >> 1;
            prim->b0 = c;
            c = src->rgb[1][0];
            if (c < src->rgb[1][1])
                c = src->rgb[1][1];
            if (c < src->rgb[1][2])
                c = src->rgb[1][2];
            prim->r1 = prim->g1 = c >> 1;
            prim->b1 = c;
            c = src->rgb[2][0];
            if (c < src->rgb[2][1])
                c = src->rgb[2][1];
            if (c < src->rgb[2][2])
                c = src->rgb[2][2];
            prim->r2 = prim->g2 = c >> 1;
            prim->b2 = c;
        }
        v = verts + src->v[0] * 6;
        dst->v[0].vx = ((short *)v)[0];
        dst->v[0].vy = ((short *)v)[1];
        dst->v[0].vz = ((short *)v)[2];
        v = verts + src->v[1] * 6;
        dst->v[1].vx = ((short *)v)[0];
        dst->v[1].vy = ((short *)v)[1];
        dst->v[1].vz = ((short *)v)[2];
        v = verts + src->v[2] * 6;
        dst->v[2].vx = ((short *)v)[0];
        dst->v[2].vy = ((short *)v)[1];
        dst->v[2].vz = ((short *)v)[2];
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_build_unit_model);
#endif

/* Sends unit u on to the next point of its path (u->path: the path, the point); the player's units march
 * straight ahead, a unit at its path's end stops facing down. */
#ifdef NON_MATCHING
/* The order of the two path loads, and the position/target loads scheduled differently. */
void condor_next_waypoint(Unit *u)
{
    unsigned char *scripts, *path;
    unsigned short pt;
    int dx, dy;

    u->sub_x = 0;
    u->sub_y = 0;
    u->vel.vx = 0;
    u->vel.vy = 0;
    u->vel.vz = 0;
    if (u->kind < 16) {
        u->vel.vy = 0x100;
        u->tx = 0;
        u->ty = 0;
        u->path = 0;
        u->dir = condor_square_angle(&u->vel);
    } else {
        scripts = CONDOR_DATA;
        path = scripts + ((unsigned short *)(scripts + *(unsigned short *)(scripts + 4)))[u->path >> 8];
        pt = ((unsigned short *)path)[u->path & 0xFF];
        if ((pt & 0xFF) != 0xFF) {
            u->path++;
            u->tx = (pt & 0xFF) << 4;
            u->ty = 0x400 - ((pt >> 8) << 4);
            dx = u->tx - u->pos.vx;
            dy = u->ty - u->pos.vy;
            D_8012C560 = 0;
            D_8012C55C = 0;
            D_8012C568 = 0;
            D_8012C564 = 0;
            D_8012C554 = dx;
            if (dx < 0) {
                D_8012C564 = 1;
                D_8012C554 = -dx;
            }
            D_8012C558 = dy;
            if (dy < 0) {
                D_8012C568 = 1;
                D_8012C558 = -dy;
            }
            if (D_8012C554 >= D_8012C558) {
                D_8012C544 = 0x100;
                if ((D_8012C558 << 8) == 0)
                    D_8012C548 = 1;
                else
                    D_8012C548 = (D_8012C558 << 8) / D_8012C554;
                if (!D_8012C548)
                    D_8012C548 = 1;
            } else {
                D_8012C548 = 0x100;
                if ((D_8012C554 << 8) == 0)
                    D_8012C544 = 1;
                else
                    D_8012C544 = (D_8012C554 << 8) / D_8012C558;
                if (!D_8012C544)
                    D_8012C544 = 1;
            }
            if (D_8012C564)
                D_8012C544 = -D_8012C544;
            if (D_8012C568)
                D_8012C548 = -D_8012C548;
            u->vel.vx = D_8012C544;
            u->vel.vy = D_8012C548;
            u->dir = condor_square_angle(&u->vel);
        } else {
            u->dir = 0x800;
            u->act &= 0xFE;
        }
    }
    u->heading = u->flash = 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_next_waypoint);
#endif

/* Starts the unit's script: its first step's frame (none with 0x80 set), and how long it shows. */
#ifdef NON_MATCHING
/* The load order of kind and pose in the table lookup; the return value in a0 where the game has v1. */
int condor_start_script(Unit *u)
{
    unsigned short step;
    int frame, ret;
    unsigned char *scripts = CONDOR_DATA;

    u->frame = 0;
    u->hold = 0;
    u->phase = 0;
    u->damage = 0;
    u->x2F = 0;
    u->script_pos = 0;
    u->script = *(unsigned short *)(CONDOR_DATA + ((unsigned short *)CONDOR_DATA)[3] + u->kind * 16 + u->pose * 2);
    if ((u->script & 0xFF) == 0xFF)
        return 0;
    step = *(unsigned short *)(scripts + (unsigned short)(u->script + u->script_pos));
    if (step == 0xFFFF) {
        u->hold = 0xFF;
        condor_build_unit_model(u);
        ret = 0;
    } else {
        if (step == 0xFFFE) {
            u->script_pos = 0;
            step = *(unsigned short *)(scripts + u->script);
        }
        frame = step & 0xFF;
        if (step & 0x80)
            frame = 0;
        u->frame = frame;
        u->hold = step >> 8;
        if (!(step & 0x80))
            condor_build_unit_model(u);
        u->script_pos += 2;
        ret = step;
    }
    return ret;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_start_script);
#endif

/* Whether a unit of this kind fits at pos: no unit in its way and condor_find_cell finds nothing. */
#ifdef NON_MATCHING
/* The kind description pointer's loads are scheduled later. */
short condor_place_is_free(short kind, unsigned short *pos)
{
    Unit probe;
    Unit *p = &probe;
    unsigned char *scripts = CONDOR_DATA;
    unsigned char *desc;

    desc = scripts + *(unsigned short *)scripts + kind * 32;
    p->pos.vx = pos[0];
    p->pos.vy = 0x400 - pos[1];
    p->pos.vz = D_8012C388;
    p->scene.vx = pos[0] - 0x100;
    p->scene.vy = 0x200 - pos[1];
    p->scene.vz = D_8012C388;
    p->w = desc[13];
    p->h = desc[14];
    condor_count_units_in_reach(p, 0, 39);
    if (!D_8012B85C)
        return condor_find_cell(p) == -1;
    return 1;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_place_is_free);
#endif

/* Whether a unit of this kind fits at pos as far as condor_find_cell goes (no check against the other units). */
#ifdef NON_MATCHING
/* The kind description pointer's loads are scheduled later. */
short condor_ground_is_free(short kind, unsigned short *pos)
{
    Unit probe;
    
    unsigned char *scripts = CONDOR_DATA;
    unsigned char *desc;

    probe.pos.vx = pos[0];
    probe.pos.vy = 0x400 - pos[1];
    probe.pos.vz = D_8012C388;
    desc = scripts + *(unsigned short *)scripts + kind * 32;
    probe.scene.vx = pos[0] - 0x100;
    probe.scene.vy = 0x200 - pos[1];
    probe.scene.vz = D_8012C388;
    probe.w = desc[13];
    probe.h = desc[14];
    return condor_find_cell(&probe) == -1;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_ground_is_free);
#endif

/* Brings unit u in as described by s at pos, in slot (0x80: check the ground even for an enemy). Returns 1 when
 * the place is taken. */
#ifdef NON_MATCHING
/* s2/s3 swapped (the spawn pointer and slot & 0x7F), the store order in the cell block, and D_8012C388's address
 * kept in a register. */
int condor_init_unit(Unit *u, Spawn *s, int slot, unsigned short *pos)
{
    unsigned char *scripts = CONDOR_DATA;
    unsigned char *desc;
    int n, q;
    Cell *r;

    desc = scripts + *(unsigned short *)scripts;
    desc += s->kind * 32;
    u->pos.vx = pos[0];
    u->pos.vy = 0x400 - pos[1];
    u->pos.vz = D_8012C388;
    u->scene.vx = pos[0] - 0x100;
    u->scene.vy = 0x200 - pos[1];
    u->scene.vz = D_8012C388;
    u->w = desc[13];
    u->h = desc[14];
    u->rot.vx = 0;
    u->rot.vy = 0;
    u->rot.vz = 0;
    n = slot & 0x7F;
    if (n < 40 || (slot & 0x80)) {
        condor_count_units_in_reach(u, 0, 39);
        if (D_8012B85C)
            return 1;
        if ((short)(u->cell = condor_find_cell(u)) == -1)
            return 1;
        r = &D_8010C33C[u->cell];
        u->rot.vx = r->rot_x;
        u->scene.vz = D_8012C388;
        u->rot.vz = r->rot_z;
    }
    q = D_8012C388 / D_8012C448;
    u->mode = 0;
    u->order = 0;
    u->act = 0;
    u->fade = 0;
    u->slot = n;
    u->flags = 1;
    u->scene.vz = D_8012C388;
    u->scale.vz = u->scale.vy = u->scale.vx = q;
    u->kind = s->kind;
    u->x0C = s->time;
    u->fall = *(unsigned short *)desc;
    u->hp = desc[2];
    u->max_hp = desc[2];
    u->power = desc[5];
    u->role = desc[6];
    u->reach = desc[7] << 4;
    u->sight = desc[10];
    u->x17 = desc[11];
    u->act_wait = desc[12] + n + 1;
    u->cooldown = 0;
    u->ai = s->x06;
    u->timer = 0;
    u->number = D_8012BA68[u->kind];
    u->rate = desc[12];
    u->path = s->x03 << 8;
    u->flash = rand() & 0x3FF;
    u->pose = 0;
    condor_start_script(u);
    u->hold += n & 7;
    u->act_time = desc[15];
    u->rest_time = desc[16];
    u->target = 0xFF;
    u->foe = 0xFF;
    u->type = u->kind;
    u->wait = desc[16];
    condor_next_waypoint(u);
    if (u->tx == u->pos.vx && u->ty == u->pos.vy)
        condor_next_waypoint(u);
    if (u - D_800BE8C0 >= 20)
        u->fall = 0;
    return 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_init_unit);
#endif

/* Places the player's units at the start of a battle: the stage's own, or those kept from the last one. */
#ifdef NON_MATCHING
/* The game's frame is 8 bytes bigger (an unused local before pos) and it loads D_8012C508 late. */
void condor_place_player_units(void)
{
    Unit *u;
    Spawn *s;
    Kept *k;
    unsigned short pos[2];
    int n, r;

    if (!D_8012C404) {
        u = D_800BE8C0;
        for (s = (Spawn *)(CONDOR_DATA + *(unsigned short *)(CONDOR_DATA + 8)); s->kind != 0xFF; s++) {
            D_800BE8BC->kind = s->kind;
            D_800BE8BC->x = s->x;
            D_800BE8BC->y = s->y;
            D_800BE8BC->x03 = s->x03;
            D_800BE8BC->time = s->time;
            D_800BE8BC->x06 = s->x06;
            pos[0] = D_800BE8BC->x << 4;
            pos[1] = D_800BE8BC->y << 4;
            if ((short)condor_init_unit(u, D_800BE8BC, D_8012C508, pos))
                continue;
            n = D_8012BA68[u->kind];
            u->number = n;
            n++;
            if ((unsigned char)n >= 100)
                n = 1;
            D_8012BA68[u->kind] = n;
            u++;
            D_8012C508++;
        }
    }
    if (D_8012C404 == 2) {
        u = D_800BE8C0;
        for (k = D_800BE814; k->x != -1; k++) {
            D_800BE8BC->kind = k->kind;
            D_800BE8BC->x = 0;
            D_800BE8BC->y = 0;
            D_800BE8BC->x03 = 0;
            D_800BE8BC->x06 = 0;
            D_800BE8BC->time = 0;
            pos[0] = k->x;
            pos[1] = k->y;
            r = condor_init_unit(u, D_800BE8BC, D_8012C508, pos);
            if (u->flags)
                u->heading = u->dir = k->dir;
            if ((short)r)
                continue;
            n = D_8012BA68[u->kind];
            u->number = n;
            n++;
            if ((unsigned char)n >= 100)
                n = 1;
            D_8012BA68[u->kind] = n;
            u++;
            D_8012C508++;
        }
    }
    condor_load_two_numbers();
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_place_player_units);
#endif

/* Brings in the stage's next enemy once the clock reaches its time and there is room for it. */
void condor_spawn_enemy(void)
{
    unsigned char *scripts;
    Spawn *s;
    int secs, i, r;
    unsigned short pos[2];
    Unit *u, *p;
    int n;
    unsigned short k;

    if (D_8012C404 == 1)
        return;
    scripts = CONDOR_DATA;
    s = (Spawn *)(scripts + ((unsigned short *)scripts)[5 + D_8012C40C]);
    s += D_8012C510;
    if (s->kind == 0xFF)
        return;
    if (D_8012C50C >= 20)
        return;
    secs = D_8012C5B0 += D_8012C5D4;
    if (secs)
        secs /= 60;
    D_8012C5E4 = secs;
    D_8012C5E0 = s->time;
    if (secs < s->time)
        return;
    for (i = 20, p = &D_800BE8C0[20]; i < 40; i++, p++) {
        if (!p->flags) {
            r = i;
            goto found;
        }
    }
    r = 0xFFFF;
found:
    k = r;
    if (k == 0xFFFF)
        return;
    D_800BE8BC->kind = s->kind;
    D_800BE8BC->x = s->x;
    D_800BE8BC->y = s->y;
    D_800BE8BC->x03 = s->x03;
    D_800BE8BC->time = s->time;
    D_800BE8BC->x06 = s->x06;
    pos[0] = D_800BE8BC->x << 4;
    pos[1] = D_800BE8BC->y << 4;
    u = &D_800BE8C0[k];
    if ((short)condor_init_unit(u, D_800BE8BC, k, pos))
        return;
    D_8012C50C++;
    condor_load_two_numbers();
    n = D_8012BA68[u->kind];
    u->number = n;
    n++;
    if ((unsigned char)n >= 100)
        n = 1;
    D_8012BA68[u->kind] = n;
    D_8012C510++;
}

/* Carries out the order chosen for the selected unit. */
void condor_do_unit_order(void)
{
    unsigned char order;
    Unit *u;

    if (!D_8012C56C)
        D_8012C570 = 7;
    order = D_8012C570;
    if (D_8012C4E4 == 1)
        order = D_8012C574;
    if (D_8012C4E4 == 2)
        order = D_8012C578;
    u = &D_800BE8C0[D_8012C41C];
    switch (order) {
    case 0:
        if (!(u->path & 0x80)) {
            u->mode = 10;
            u->vel.vx = u->vel.vy = u->tx = u->ty = u->dir = 0;
            DESELECT();
            break;
        }
        snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 3;
        snd_submit_request();
        break;
    case 1:
        DESELECT();
        break;
    case 2:
        condor_drop_selected();
        DESELECT();
        break;
    case 3:
        D_8012C3E8 = 3;
        D_8012C57C = 3;
        D_8012C3AC[0] = D_8012C3A4[0];
        D_8012C3AC[1] = D_8012C3A4[1];
        break;
    case 4:
        DESELECT();
        break;
    case 5:
        D_8012C470 = 0;
        D_8012C460 = D_800BE8C0[D_8012C41C].heading;
        D_8012C468 = (D_8012C460 + 0x200) & 0xFFF;
        D_8012C5DC = u->reach;
        D_8012C3EC = 0x10;
        break;
    default:
        DESELECT();
        break;
    }
}

/* Gives the selected unit the chosen order; only order 3 is given from here. */
void condor_order_move(void)
{
    unsigned char order;

    if (!D_8012C56C)
        D_8012C570 = 7;
    order = D_8012C570;
    if (D_8012C4E4 == 1)
        order = D_8012C574;
    if (D_8012C4E4 == 2)
        order = D_8012C578;
    switch (order) {
    case 0:
    case 1:
    case 2:
        break;
    case 3:
        condor_give_order(D_8012C41C);
        break;
    case 4:
    case 5:
    case 6:
        break;
    }
}

/* Sends the unit straight at D_8012C3AC: its step on the longer axis is 1, on the other the ratio (never 0). */
#ifdef NON_MATCHING
/* The target y store is scheduled after the zeroing of D_8012C55C-568. */
void condor_walk_to_target(Unit *u)
{
    int x, y, dx, dy;

    u->vel.vx = 0;
    u->vel.vy = 0;
    u->vel.vz = 0;
    x = u->pos.vx;
    y = u->pos.vy;
    u->tx = D_8012C3AC[0];
    u->ty = D_8012C3AC[1];
    dx = u->tx - x;
    dy = u->ty - y;
    D_8012C560 = 0;
    D_8012C55C = 0;
    D_8012C568 = 0;
    D_8012C564 = 0;
    D_8012C554 = dx;
    if (dx < 0) {
        D_8012C564 = 1;
        D_8012C554 = -dx;
    }
    D_8012C558 = dy;
    if (dy < 0) {
        D_8012C568 = 1;
        D_8012C558 = -dy;
    }
    if (D_8012C554 >= D_8012C558) {
        D_8012C544 = 0x100;
        if ((D_8012C558 << 8) == 0)
            D_8012C548 = 1;
        else
            D_8012C548 = (D_8012C558 << 8) / D_8012C554;
        if (!D_8012C548)
            D_8012C548 = 1;
    } else {
        D_8012C548 = 0x100;
        if ((D_8012C554 << 8) == 0)
            D_8012C544 = 1;
        else
            D_8012C544 = (D_8012C554 << 8) / D_8012C558;
        if (!D_8012C544)
            D_8012C544 = 1;
    }
    if (D_8012C564)
        D_8012C544 = -D_8012C544;
    if (D_8012C568)
        D_8012C548 = -D_8012C548;
    u->vel.vx = D_8012C544;
    u->vel.vy = D_8012C548;
    u->dir = condor_square_angle(&u->vel);
    u->flash = 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_walk_to_target);
#endif

/* Gives unit i the order in D_8012C57C. */
void condor_give_order(int i)
{
    Unit *u = &D_800BE8C0[i];

    u->order = D_8012C57C;
    D_8012C010 = 0;
    switch (D_8012C57C) {
    case 0:
    case 1:
        condor_find_units_at(D_8012C3AC);
        if (D_8012B85C >= 2)
            condor_open_unit_list();
        if (D_8012C010 == 2)
            break;
        u->mode = 3;
        u->act = 3;
        u->target = D_8012C420;
        condor_walk_to_target(u);
        break;
    case 2:
        condor_find_units_at(D_8012C3AC);
        if (D_8012B85C >= 2)
            condor_open_unit_list();
        if (D_8012C010 == 2)
            break;
        u->mode = 5;
        u->act = 5;
        u->target = D_8012C420;
        condor_walk_to_target(u);
        break;
    case 3:
        u->mode = 1;
        u->act = 1;
        u->target = D_8012C420;
        condor_walk_to_target(u);
        u->foe = 0xFF;
        u->phase = 0;
        u->damage = 0;
        u->x2F = 0;
        u->target = 0xFF;
        u->flags &= 0xD3;
        break;
    case 4:
        u->mode = 0;
        u->act = 0;
        u->tx = 0;
        u->ty = 0;
        u->target = 0xFF;
        break;
    case 5:
        u->mode = 6;
        u->act = 9;
        condor_walk_to_target(u);
        u->target = 0xFF;
        break;
    case 6:
        u->mode = 7;
        u->act = 0x11;
        condor_walk_to_target(u);
        u->target = 0xFF;
        break;
    }
}

/* Opens the shop: the units for sale (the list by D_8012C418) set on a ring, and the three shop units 64-66.
 * Returns 1 when there is no room for it. */
#ifdef NON_MATCHING
/* Frame 8 bytes bigger and one more saved register in the game; the shop units' block is reached through constants
 * instead of a register. */
int condor_open_shop(void)
{
    unsigned short pos[3];
    char list0[11] = { 1, 2, 3, 4, 12, 13, 5, 7, 0, 0, 0 };
    char list1[11] = { 1, 2, 3, 4, 12, 13, 5, 7, 8, 0, 0 };
    char list2[11] = { 1, 2, 3, 4, 12, 13, 5, 6, 7, 0, 0 };
    char list3[11] = { 1, 2, 3, 4, 12, 13, 5, 6, 7, 8, 0 };
    char *list;
    Unit *u;
    short i, k;
    int n, slot, q;

    switch (D_8012C418 & 3) {
    case 0:
        list = list0;
        D_8012C500 = 8;
        D_8012C504 = 0x200;
        break;
    case 1:
        list = list1;
        D_8012C500 = 9;
        D_8012C504 = 0x1C7;
        break;
    case 2:
        list = list2;
        D_8012C500 = 9;
        D_8012C504 = 0x1C7;
        break;
    default:
        list = list3;
        D_8012C500 = 10;
        D_8012C504 = 0x199;
        break;
    }
    for (i = 0; i < 11; i++)
        D_8012C580[i] = *list++;
    D_8012C42C = 1;
    n = 0;
    slot = 52;
    u = &D_800BE8C0[52];
    if (D_8011CBBC[D_8012C3F0].map[0].r0 > 0x50)
        D_8012C488 = 1;
    else
        D_8012C488 = 3;
    D_8012C47C = 2;
    D_8012C480 = D_8012C484 = 1;
    D_8012C72C.vx = 0xB8;
    D_8012C734.vz = D_8012C388 * 2;
    D_8012C470 = 0;
    D_8012C468 = 0;
    D_8012C46C = 0;
    D_8012C4A0 = 0;
    D_8012C49C = 0;
    D_8012C478 = 0;
    D_8012C724.vx = 0;
    D_8012C724.vy = 0;
    D_8012C724.vz = 0;
    D_8012C734.vx = 0;
    D_8012C734.vy = 0;
    D_8012C72C.vy = 0;
    D_8012C72C.vz = 0;
    D_8012C744.vz = D_8012C744.vy = D_8012C744.vx = D_8012C388;
    D_800BE8BC->kind = D_8012C580[(short)D_8012C4EC];
    D_800BE8BC->x03 = 0;
    D_800BE8BC->time = 0;
    pos[0] = D_8012C3A4[0];
    pos[1] = 0x400 - D_8012C3A4[1];
    pos[2] = -40;
    if ((short)condor_init_unit(u, D_800BE8BC, 0xB4, pos) == 1) {
        snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 3;
        snd_submit_request();
        D_800BE8C0[52].flags = 0;
        return 1;
    }
    snd_request.id = 0x2B;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 1;
    snd_submit_request();
    D_800BE8C0[52].vel.vy = 0;
    D_800BE8C0[52].vel.vx = 0;
    D_800BE8C0[52].rot.vz = 0;
    D_800BE8C0[52].rot.vx = 0;
    D_800BE8C0[52].frame = 0;
    /* Greyed out when it costs more than there is. */
    D_8012C4C8 = D_8012C538 < DATA_HALF(DATA_HALF(0) + D_800BE8BC->kind * 32);
    condor_build_unit_model(u);
    D_8012C754.vx = u->scene.vx;
    D_8012C754.vy = u->scene.vy;
    D_8012C754.vz = u->scene.vz - 0x118;
    D_8012C4C8 = 0;
    slot++;
    condor_turn_step(-(n * D_8012C504) & 0xFFF, 0x8C);
    D_8012C3EC = 7;
    u->scene.vy = 0;
    u->scene.vx = D_8012C544;
    u->scene.vz = -D_8012C548;
    condor_load_unit_card(D_800BE8BC->kind, D_8012C474 + 1);
    n++;
    condor_load_unit_info(D_800BE8BC->kind);
    u++;
    for (i = 1; i < D_8012C500; i++, u++) {
        k = i + D_8012C4EC;
        if (k >= D_8012C500)
            k -= D_8012C500;
        D_800BE8BC->kind = D_8012C580[k];
        condor_init_unit(u, D_800BE8BC, slot & 0xFFFF, pos);
        u->frame = 0;
        u->vel.vy = 0;
        u->vel.vx = 0;
        u->rot.vz = 0;
        u->rot.vx = 0;
        D_8012C4C8 = D_8012C538 < DATA_HALF(DATA_HALF(0) + D_800BE8BC->kind * 32);
        condor_build_unit_model(u);
        D_8012C4C8 = 0;
        condor_turn_step(-(n * D_8012C504) & 0xFFF, 0x8C);
        slot++;
        n++;
        u->scene.vz = -D_8012C548;
        u->scene.vy = 0;
        u->scene.vx = D_8012C544;
    }
    D_800BE8BC->kind = 0x11;
    condor_init_unit(&D_800BE8C0[66], D_800BE8BC, 66, pos);
    q = D_8012C388 / D_8012C448;
    D_800BE8C0[66].scene.vy = 0x18;
    D_800BE8C0[66].scene.vz = -0x40;
    D_800BE8C0[66].vel.vy = 0;
    D_800BE8C0[66].vel.vx = 0;
    D_800BE8C0[66].rot.vz = 0;
    D_800BE8C0[66].rot.vx = 0;
    D_800BE8C0[66].frame = 0;
    D_800BE8C0[66].scene.vx = 12;
    D_800BE8C0[66].scale.vz = D_800BE8C0[66].scale.vy = D_800BE8C0[66].scale.vx = q / 2;
    D_800BE8BC->kind = 0x12;
    condor_init_unit(&D_800BE8C0[65], D_800BE8BC, 65, pos);
    q = D_8012C388 / D_8012C448;
    D_800BE8C0[65].scene.vy = 0x18;
    D_800BE8C0[65].scene.vz = -0x40;
    D_800BE8C0[65].vel.vy = 0;
    D_800BE8C0[65].vel.vx = 0;
    D_800BE8C0[65].rot.vz = 0;
    D_800BE8C0[65].rot.vx = 0;
    D_800BE8C0[65].frame = 0;
    D_800BE8C0[65].scene.vx = 0x22;
    D_800BE8C0[65].scale.vz = D_800BE8C0[65].scale.vy = D_800BE8C0[65].scale.vx = q / 2;
    D_800BE8BC->kind = 0x13;
    condor_init_unit(&D_800BE8C0[64], D_800BE8BC, 64, pos);
    q = D_8012C388 / D_8012C448;
    D_800BE8C0[64].scene.vx = 0x38;
    D_800BE8C0[64].scene.vy = 0x18;
    D_800BE8C0[64].scene.vz = -0x40;
    D_800BE8C0[64].vel.vy = 0;
    D_800BE8C0[64].vel.vx = 0;
    D_800BE8C0[64].rot.vz = 0;
    D_800BE8C0[64].rot.vx = 0;
    D_800BE8C0[64].frame = 0;
    D_8012C3EC = 7;
    D_800BE8C0[64].scale.vz = D_800BE8C0[64].scale.vy = D_800BE8C0[64].scale.vx = q / 2;
    return 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_open_shop);
#endif

/* Buys a unit of this kind at the cursor. Returns 1 when the place is taken. Towers (roles 3 and 4) go on to
 * aiming. */
#ifdef NON_MATCHING
/* The position stores and the kind's description loads are scheduled differently. With no free slot the game reads
 * u's fields from whatever s0 held; this reads unit 20's. */
int condor_buy_unit(int kind)
{
    Unit *u, *p;
    unsigned short pos[3];
    int i, n;
    unsigned short r;
    unsigned short slot;

    for (i = 0, p = D_800BE8C0; i < 20; i++, p++) {
        if (!p->flags) {
            r = i;
            goto found;
        }
    }
    r = 0xFFFF;
found:
    slot = r;
    u = p; /* with no free slot the game goes on with u unset (whatever its register held) */
    if (slot != 0xFFFF) {
        D_8012C3EC = 8;
        D_8012C474 = r;
        D_800BE8BC->kind = kind;
        D_800BE8BC->x03 = 0;
        D_800BE8BC->time = 0;
        pos[2] = 40;
        pos[0] = D_8012C3A4[0];
        pos[1] = 0x400 - D_8012C3A4[1];
        u = &D_800BE8C0[slot];
        if ((short)condor_init_unit(u, D_800BE8BC, slot, pos) == 1) {
            snd_request.id = 0x2B;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 3;
            snd_submit_request();
            u->flags = 0;
            D_8012C3EC = 0;
            return 1;
        }
        snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 1;
        snd_submit_request();
        D_8012C538 -= D_8012C534;
        u->vel.vy = 0;
        u->vel.vx = 0;
        D_8012C508++;
        n = D_8012BA68[u->kind];
        u->number = n;
        n++;
        if ((unsigned char)n >= 100)
            n = 1;
        D_8012BA68[u->kind] = n;
        D_8012C41C = r;
        condor_draw_hp();
    }
    if (u->role == 3 || u->role == 4) {
        D_8012C3EC = 8;
        D_8012C468 = 0x200;
        D_8012C470 = 0;
        D_8012C5DC = u->reach;
        return 0;
    }
    D_8012C480 = D_8012C484 = D_8012C47C = 1;
    D_8012C42C = 0;
    for (i = 0, p = (&D_800BE8C0[52]); i < D_8012C500; i++, p++)
        p->flags = 0;
    if (D_8012C404 == 1)
        D_8012AF58 = 12;
    D_8012C3EC = 0;
    return 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_buy_unit);
#endif

/* The shop: left and right spin the ring of units for sale, which settles on the nearest one when let go; 0x20
 * buys the one in front, 0x40 leaves. */
#ifdef NON_MATCHING
/* Structure right; scheduling and register allocation far off. */
void condor_shop_update(void)
{
    Unit *p;
    short i, i0, i1, a, b, item;
    int d;

    D_8012C4A0 += 0x10;
    D_8012C49C += 0x20;
    for (i = 0, p = (&D_800BE8C0[52]); i < D_8012C500; i++, p++)
        p->rot.vy = D_8012C49C;
    (&D_800BE8C0[52])[12].rot.vy = (&D_800BE8C0[52])[13].rot.vy = (&D_800BE8C0[52])[14].rot.vy = -D_8012C4A0;
    if (D_8012C390 & 0x8000)
        D_8012C46C = -0x30;
    if (D_8012C390 & 0x2000)
        D_8012C46C = 0x30;
    d = D_8012C46C;
    if (d) {
        /* Let go: spin on to the next item's place, or back to this one's. */
        if (!(D_8012C390 & 0xA000)) {
            if (d > 0) {
                a = D_8012C468 & 0xFFF;
                if (!a)
                    i0 = 0;
                else
                    i0 = a / D_8012C504;
                if (a == D_8012C500)
                    i0 = 0;
                b = (a + D_8012C46C) & 0xFFF;
                if (!b)
                    i1 = 0;
                else
                    i1 = b / D_8012C504;
                if (b == D_8012C500)
                    i1 = 0;
                if (i0 == i1) {
                    D_8012C468 += D_8012C46C;
                    D_8012C72C.vy += D_8012C46C;
                } else {
                    D_8012C46C = (i0 + 1) * D_8012C504 - a;
                    D_8012C468 += D_8012C46C;
                    D_8012C72C.vy += D_8012C46C;
                    D_8012C46C = 0;
                }
            } else {
                a = D_8012C468 & 0xFFF;
                if (!a)
                    i0 = 0;
                else
                    i0 = a / D_8012C504;
                if (a == D_8012C500)
                    i0 = 0;
                b = (a + D_8012C46C) & 0xFFF;
                if (!b)
                    i1 = 0;
                else
                    i1 = b / D_8012C504;
                if (b == D_8012C500)
                    i1 = 0;
                if (i0 == i1) {
                    D_8012C468 += D_8012C46C;
                    D_8012C72C.vy += D_8012C46C;
                } else if (!i0) {
                    D_8012C468 = 0;
                    D_8012C72C.vy = 0;
                    D_8012C46C = 0;
                } else {
                    D_8012C46C = -(a % D_8012C504);
                    D_8012C468 += D_8012C46C;
                    D_8012C72C.vy += D_8012C46C;
                    D_8012C46C = 0;
                }
            }
        } else {
            D_8012C468 += d;
            D_8012C72C.vy += d;
        }
    }
    item = (D_8012C468 & 0xFFF) / D_8012C504;
    if (item == D_8012C500)
        item = 0;
    if (item != D_8012C470) {
        snd_request.id = 0x28;
        snd_request.arg[0] = 0x40;
        D_8012C470 = item;
        snd_request.arg[1] = 1;
        snd_submit_request();
        item += D_8012C4EC;
        if (item >= D_8012C500)
            item -= D_8012C500;
        condor_load_unit_card(D_8012C580[item], D_8012C474 + 1);
        condor_load_unit_info(D_8012C580[item]);
    }
    if (!(D_8012C390 & 0xA000)) {
        D_800BE8C0[52 + item].rot.vy = 0;
        if (D_8012C580[item] < 5 && D_8012C580[item] >= 2)
            D_8012C478 = D_8012C580[item] - 1;
        else
            D_8012C478 = 0;
    } else {
        D_8012C478 = 0;
    }
    if (D_8012C39C & 0x20) {
        if (D_8012C580[item] == 9 || D_8012C538 < D_8012C534) {
            snd_request.id = 0x2B;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 3;
            snd_submit_request();
        } else {
            for (i = 0, p = (&D_800BE8C0[52]); i < D_8012C500; i++, p++)
                p->flags = 0;
            if (D_8012C404 == 1)
                D_8012AF58 = 12;
            condor_buy_unit(D_8012C580[item]);
            condor_load_two_numbers();
        }
    }
    if (D_8012C39C & 0x40) {
        snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 4;
        snd_submit_request();
        for (i = 0, p = (&D_800BE8C0[52]); i < D_8012C500; i++, p++)
            p->flags = 0;
        if (D_8012C404 == 1)
            D_8012AF58 = 12;
        D_8012C480 = D_8012C484 = D_8012C47C = 1;
        D_8012C42C = 0;
        D_8012C3EC = 0;
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_shop_update);
#endif

/* Aiming the selected unit: left and right turn it; 0x20 fires, 0x40 fires its last shot and gives it up. */
#ifdef NON_MATCHING
/* The counter decrement keeps the loaded value in v0 instead of a0. */
void condor_aim_tower(void)
{
    Unit *u = &D_800BE8C0[D_8012C41C];
    int n;

    if (D_8012C390 & 0x8000) {
        D_8012C468 -= 0x20;
        if (D_8012C468 < 0)
            D_8012C468 = 0;
    }
    if (D_8012C390 & 0x2000) {
        D_8012C468 += 0x20;
        if (D_8012C468 > 0x400)
            D_8012C468 = 0x400;
    }
    D_8012C70E = u->heading = u->dir = D_8012C468 - 0x200;
    if (D_8012C39C & 0x20) {
        snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 1;
        snd_submit_request();
        D_8012C480 = D_8012C484 = D_8012C47C = 1;
        D_8012C3EC = 0;
        D_8012C42C = 0;
    }
    if (D_8012C39C & 0x40) {
        snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 4;
        snd_submit_request();
        D_8012C480 = D_8012C484 = D_8012C47C = 1;
        D_8012C3EC = 0;
        D_8012C42C = 0;
        D_8012C538 += D_8012C534;
        D_800BE8C0[D_8012C41C].flags = 0;
        D_8012C508--;
        n = D_8012BA68[u->kind] - 1;
        n = n ? n : 99;
        D_8012BA68[u->kind] = n;
        condor_load_two_numbers();
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_aim_tower);
#endif

/* Aiming the selected unit: left and right turn it; 0x20 confirms the aim, 0x40 cancels it; either deselects it. */
void condor_aim_unit(void)
{
    Unit *u = &D_800BE8C0[D_8012C41C];

    if (D_8012C390 & 0x8000) {
        D_8012C468 -= 0x20;
        if (D_8012C468 < 0)
            D_8012C468 = 0;
    }
    if (D_8012C390 & 0x2000) {
        D_8012C468 += 0x20;
        if (D_8012C468 > 0x400)
            D_8012C468 = 0x400;
    }
    D_8012C70E = u->heading = u->dir = D_8012C468 - 0x200;
    if (D_8012C39C & 0x20) {
        snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 1;
        snd_submit_request();
        D_8012C480 = D_8012C484 = D_8012C47C = 1;
        D_8012C3EC = 0;
        D_8012C42C = 0;
        u->reach = D_8012C5DC;
        D_8012C3E8 = 1;
        D_8012C480 = D_8012C484 = D_8012C47C = 1;
        D_8012C41C = -1;
        D_8012C428 = 1;
    }
    if (D_8012C39C & 0x40) {
        snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 4;
        snd_submit_request();
        D_8012C480 = D_8012C484 = D_8012C47C = 1;
        D_8012C3EC = 0;
        D_8012C42C = 0;
        D_8012C3E8 = 1;
        D_8012C480 = D_8012C484 = D_8012C47C = 1;
        D_8012C41C = -1;
        D_8012C428 = 1;
        u->heading = u->dir = D_8012C460;
    }
}

/* Gives up the selected unit: it fades out and stops counting. */
void condor_drop_selected(void)
{
    D_800BE8C0[D_8012C41C].fade = 0xFF;
    D_8012C508--;
    condor_load_two_numbers();
}

/* Unit u takes its hit (u->damage): a damage number, then its death or a hit sound placed by where it stands. */
#ifdef NON_MATCHING
/* s0 and s2 swapped between the damage and the hp. */
void condor_take_hit(Unit *u)
{
    int damage = u->damage;
    short n, hp;
    int i, r;
    unsigned int pan;

    u->pose = 3;
    condor_start_script(u);
    for (i = 0; i < 16; i++) {
        if (!D_8012C5FC[i].age) {
            r = i;
            goto found;
        }
    }
    r = 0xFF;
found:
    n = r;
    if (n != 0xFF) {
        D_8012C5FC[n].age = 1;
        D_8012C5FC[n].value = damage;
        D_8012C5FC[n].x = u->pos.vx;
        D_8012C5FC[n].y = u->pos.vy;
        condor_draw_counter_digits(n, damage);
    }
    hp = u->hp - damage;
    if (hp < 0)
        hp = 0;
    u->hp = hp;
    if (D_8012C41C != -1)
        condor_draw_hp();
    if (!hp) {
        if (u->fade)
            return;
        u->fade = 0xFF;
        if (u->kind < 16)
            D_8012C508--;
        else
            D_8012C50C--;
        condor_load_two_numbers();
        return;
    }
    pan = u->pos.vx & 0x1FF;
    snd_request.id = 0x2A;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0;
    snd_submit_request();
    snd_request.id = 0x2A;
    snd_request.arg[1] = 0xE1;
    snd_request.arg[0] = pan >> 2;
    snd_submit_request();
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_take_hit);
#endif

/* Unit u's blow lands on the units in its reach: during frames 4 and 5 the first one is hit even if it was hit
 * already, otherwise each one not yet hit and not recovering is. Returns whether the first was hit. */
#ifdef NON_MATCHING
/* GCC hoists the 0x80180000 constant out of the loop (s3); the game rebuilds it each time. */
int condor_area_attack(Unit *u)
{
    short i;
    Unit *t;
    unsigned char *table;
    int pct;
    unsigned char d;

    condor_find_touching_units(u, 20, 40);
    if (!D_8012B85C)
        return 0;
    for (i = 0; i < D_8012B85C; i++) {
        t = D_8012B75C[i].unit;
        switch (u->frame) {
        case 4:
        case 5:
            t->flags |= 0x10;
            if (!t->timer)
                t->timer = 40;
            t->act_wait = 0;
            table = CONDOR_DATA + *(unsigned short *)(CONDOR_DATA + 0x18);
            pct = table[u->type * 20 + t->type];
            d = pct ? u->power * pct / 100 : 0;
            t->damage = d;
            condor_take_hit(t);
            return 1;
        }
        if (!(t->flags & 0x10) && !t->timer) {
            t->flags |= 0x10;
            t->timer = 40;
            t->act_wait = 0;
            table = CONDOR_DATA + *(unsigned short *)(CONDOR_DATA + 0x18);
            pct = table[u->type * 20 + t->type];
            d = pct ? u->power * pct / 100 : 0;
            t->damage = d;
            condor_take_hit(t);
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_area_attack);
#endif

/* Unit u hits the first unit in its reach not already hit: the damage is u's power scaled by the type table
 * (percent). Returns whether it hit one. */
int condor_attack(Unit *u)
{
    short i;
    Unit *t;
    unsigned char *table;
    int pct;
    unsigned char d;

    condor_find_touching_units(u, 0, 20);
    if (!D_8012B85C)
        return 0;
    for (i = 0; i < D_8012B85C; i++) {
        t = D_8012B75C[i].unit;
        if (t->flags & 0x10)
            continue;
        t->flags |= 0x10;
        if (!t->timer)
            t->timer = 0x28;
        t->act_wait = 0;
        table = CONDOR_DATA + *(unsigned short *)(CONDOR_DATA + 0x18);
        pct = table[u->type * 20 + t->type];
        d = pct ? u->power * pct / 100 : 0;
        t->damage = d;
        condor_take_hit(t);
        return 1;
    }
    return 0;
}

/* Animates and draws an explosion; at step 12 it hits the units under it. */
#ifdef NON_MATCHING
/* GCC keeps the step table's address in a register across two loads. */
void condor_update_blast(POLY_FT4 *p, Blast *b)
{
    Unit probe;
    BlastStep steps[12] = {
        { 4, 1, 0 }, { 1, 1, 0 }, { 0, 1, 0 }, { 0, 2, 1 }, { 0, 2, 2 }, { 0, 2, 3 },
        { 0, 4, 4 }, { 0, 6, 5 }, { 0, 6, 6 }, { 1, 6, 7 }, { 4, 6, 7 }, { 4, 1, 7 },
    };
    short mode;

    if (b->step >= 0x40) {
        b->step = 0xFF;
        b->timer = 0;
    } else if (b->step < 11) {
        if (!b->timer) {
            b->step++;
            b->timer = steps[b->step].frames - 1;
            b->image = steps[b->step].image;
            p->clut = GetClut(0x340, 0xF1);
        } else {
            b->timer--;
        }
        mode = steps[b->step].mode;
        if (mode == 0)
            func_80046848(p, 0);
        if (mode == 1) {
            func_80046848(p, 1);
            p->tpage = GetTPage(0, 1, 0x340, 0);
        }
        if (mode == 4) {
            func_80046848(p, 1);
            p->tpage = GetTPage(0, 3, 0x340, 0);
        }
        p->u0 = (b->image & 3) << 6;
        p->v0 = (b->image & 4) << 3;
        p->u1 = ((b->image & 3) << 6) + 63;
        p->v1 = (b->image & 4) << 3;
        p->u2 = (b->image & 3) << 6;
        p->v2 = ((b->image & 4) << 3) + 31;
        p->u3 = ((b->image & 3) << 6) + 63;
        p->v3 = ((b->image & 4) << 3) + 31;
        p->x0 = b->x + 0x80;
        p->y0 = b->y + 0x68;
        p->x1 = b->x + 0xC0;
        p->y1 = b->y + 0x68;
        p->x2 = b->x + 0x80;
        p->y2 = b->y + 0x88;
        p->x3 = b->x + 0xC0;
        p->y3 = b->y + 0x88;
        AddPrim(D_80129F34->ot, p);
    } else {
        b->step++;
    }
    if (b->step == 12) {
        probe.pos.vx = b->x + 0x100;
        probe.pos.vy = b->y + 0x210;
        probe.w = 0x40;
        probe.h = 0x20;
        probe.pos.vz = b->z;
        probe.power = b->power;
        probe.type = b->type;
        condor_area_attack(&probe);
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_update_blast);
#endif

/* Animates and draws a burst; at frame 10 it hurts what stands there. */
#ifdef NON_MATCHING
/* The first animation read: the game folds sp + 0x88 into each index; GCC keeps sp + 136 in a register. */
void condor_draw_burst(POLY_FT4 *poly, Blast *b)
{
    Unit o;
    /* each frame: semi-transparency (0 none, 1 additive, 4 the other), time, cell */
    short anim[12][3] = {
        { 4, 1, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 2, 1 }, { 4, 2, 2 }, { 4, 2, 3 },
        { 4, 4, 4 }, { 4, 6, 5 }, { 4, 6, 6 }, { 4, 3, 7 }, { 4, 2, 7 }, { 4, 1, 7 },
    };
    short mode;

    if (b->step >= 0x40) {
        b->step = 0xFF;
        b->timer = 0;
    } else if (b->step < 11) {
        if (!b->timer) {
            b->step++;
            b->timer = anim[b->step][1] - 1;
            b->image = anim[b->step][2];
            poly->clut = GetClut(0x340, 0xF3);
        } else {
            b->timer--;
        }
        mode = anim[b->step][0];
        if (mode == 0)
            func_80046848(poly, 0);
        if (mode == 1) {
            func_80046848(poly, 1);
            poly->tpage = GetTPage(0, 1, 0x340, 0);
        }
        if (mode == 4) {
            func_80046848(poly, 1);
            poly->tpage = GetTPage(0, 3, 0x340, 0);
        }
        poly->u0 = (b->image & 3) << 6;
        poly->v0 = ((b->image & 4) << 3) + 0x40;
        poly->u1 = ((b->image & 3) << 6) + 0x3F;
        poly->v1 = ((b->image & 4) << 3) + 0x40;
        poly->u2 = (b->image & 3) << 6;
        poly->v2 = ((b->image & 4) << 3) + 0x5F;
        poly->u3 = ((b->image & 3) << 6) + 0x3F;
        poly->v3 = ((b->image & 4) << 3) + 0x5F;
        poly->x0 = b->x + 0x80;
        poly->y0 = b->y + 0x68;
        poly->x1 = b->x + 0xC0;
        poly->y1 = b->y + 0x68;
        poly->x2 = b->x + 0x80;
        poly->y2 = b->y + 0x88;
        poly->x3 = b->x + 0xC0;
        poly->y3 = b->y + 0x88;
        AddPrim(D_80129F34->ot, poly);
    } else {
        b->step++;
    }
    if (b->step == 10) {
        o.pos.vx = b->x + 0x100;
        o.pos.vy = b->y + 0x210;
        o.w = 0x20;
        o.h = 0x20;
        o.pos.vz = b->z;
        o.power = b->power;
        o.type = b->type;
        condor_area_attack(&o);
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_draw_burst);
#endif

/* Starts a burst where o stands, with its sound panned by o's place; 0 if all eight are busy. */
#ifdef NON_MATCHING
/* Only the sound request: the game reaches snd_request's id and two arguments as separate symbols, scheduled
 * freely; through the struct GCC keeps &snd_request in a register. */
int condor_start_burst(Unit *o)
{
    Blast *b = D_8011CB3C;
    short i;
    unsigned short pan;

    for (i = 0; i < 8; i++, b++) {
        if (b->step == 0xFF) {
            pan = o->pos.vx;
            snd_request.id = 0x29;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0;
            snd_submit_request();
            snd_request.id = 0x29;
            snd_request.arg[1] = 0xF;
            snd_request.arg[0] = (pan & 0x1FF) >> 2;
            snd_submit_request();
            D_8011CB3C[i].step = 0;
            D_8011CB3C[i].x = o->scene.vx;
            D_8011CB3C[i].y = o->scene.vy;
            D_8011CB3C[i].z = o->scene.vz;
            D_8011CB3C[i].power = o->power;
            D_8011CB3C[i].type = o->type;
            return 1;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_start_burst);
#endif

/* Moves a shot by its stage (frame), then ends it when its time is up or it lands. Declared int though it returns
 * nothing: the game keeps v0 live at the end (unfilled delay slots). */
int condor_step_shot(Unit *o)
{
    int hit;

    condor_apply_velocity(o);
    switch (o->frame) {
    case 0:
    case 1:
        o->rot.vx += 0x80;
        condor_area_attack(o);
        break;
    case 2:
    case 3:
        o->rot.vx += 0x200;
        o->scene.vy = ((short)o->fall >> 8) + o->scene.vy;
        o->fall += 0x14;
        break;
    case 4:
    case 5:
        condor_apply_velocity(o);
        condor_apply_velocity(o);
        o->rot.vx = 0x200;
        if (o->hp >= 4) {
            if (o->type == 4)
                hit = condor_area_attack(o);
            else
                hit = condor_attack(o);
            if ((short)hit)
                o->hp = 2;
        }
        o->hp--;
        if (!o->hp)
            o->flags = 0;
        break;
    }
    switch (o->frame) {
    case 0:
    case 1:
        o->hp--;
        if (!o->hp) {
            o->flags = 0;
            o->fade = 0;
            snd_request.id = 0x29;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0;
            snd_submit_request();
        }
        break;
    case 2:
    case 3:
        if (condor_landed(o)) {
            o->flags = 0;
            o->fade = 0;
            condor_start_burst(o);
        }
        break;
    }
}

/* Steps the units in slots 40-69 whose timer runs out. */
#ifdef NON_MATCHING
/* The game reads D_8012C3EC as if it were volatile (after the loop's branch); making it volatile breaks the
 * functions that read it plainly. */
void condor_update_shots(void)
{
    Unit *o;
    Unit *t;
    Unit *u;
    short i;

    if (D_8012C3E8 < 2 && !D_8012C42C && D_8012C3EC != 7) {
        o = &D_800BE8C0[40];
        for (i = 40; i < 70; o++, i++) {
            if (!o->flags)
                continue;
            o->act_wait--;
            if (o->act_wait)
                continue;
            o->act_wait = o->rate;
            if (o->kind == 0x18) {
                condor_step_shot(o);
                continue;
            }
            if (o->pose == 7) {
                t = &D_800BE8C0[o->type];
                if (t->fade)
                    o->flags = 0;
                if (t->pose != 2)
                    o->flags = 0;
            }
            if (o->kind == 0xB && (short)condor_find_touching_units(o, 0x14, 0x28) != -1) {
                o->flags = 0;
                o->fade = 0;
                condor_start_burst(o);
                u = &D_800BE8C0[o->foe];
                if (u->flags && u->kind == 0xD)
                    u->path &= 0x7F;
            }
            condor_run_anim_script(o);
            if (o->hold == 0xFF) {
                o->flags = 0;
                o->fade = 0;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_update_shots);
#endif

/* Turns a step of len by angle (0-0xFFF) into D_8012C544, D_8012C548. Non-static inline: condor_fire_shot and
 * condor_place_fort have it inlined. */
inline void condor_turn_step(int angle, short len)
{
    int s = rsin(angle & 0x3FF) * len >> 12;
    int c = rcos(angle & 0x3FF) * len >> 12;

    switch (angle & 0xC00) {
    case 0:
        D_8012C544 = -s;
        D_8012C548 = c;
        break;
    case 0x400:
        D_8012C544 = -c;
        D_8012C548 = -s;
        break;
    case 0x800:
        D_8012C544 = s;
        D_8012C548 = -c;
        break;
    case 0xC00:
        D_8012C544 = c;
        D_8012C548 = s;
        break;
    default:
        D_8012C544 = s;
        D_8012C548 = c;
    }
}

/* What a unit does when it hits o, by the hitter's kind; each plays its sound panned by o's place. */
#ifdef NON_MATCHING
/* The sound request's form and the scheduling of its stores against the unit's field stores. */
void condor_hit_by(Unit *by, Unit *o)
{
    unsigned short pan;

    switch (by->kind) {
    case 4:
    case 0x13:
        o->frame = 4;
        o->w = o->h = 2;
        o->pos.vy -= 0x10;
        o->scene.vy -= 0x10;
        pan = o->pos.vx;
        snd_request.id = 0x2A;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 0;
        snd_submit_request();
        snd_request.id = 0x2A;
        snd_request.arg[1] = 0xE4;
        snd_request.arg[0] = (pan & 0x1FF) >> 2;
        snd_submit_request();
        break;
    case 5:
        o->frame = 0;
        snd_request.id = 0x29;
        snd_request.arg[1] = 0xF3;
        snd_request.arg[0] = (o->pos.vx & 0x1FF) >> 2;
        snd_submit_request();
        break;
    case 6:
        o->frame = 0;
        condor_fire_shot(o, 0);
        condor_fire_shot(o, 1);
        snd_request.id = 0x29;
        snd_request.arg[1] = 0xE6;
        snd_request.arg[0] = (o->pos.vx & 0x1FF) >> 2;
        snd_submit_request();
        break;
    case 7:
        o->frame = 2;
        o->pos.vy -= 0x28;
        o->scene.vy -= 0x28;
        snd_request.id = 0x29;
        snd_request.arg[0] = (o->pos.vx & 0x1FF) >> 2;
        snd_request.arg[1] = 0xE7;
        snd_submit_request();
        break;
    case 8:
        o->frame = 3;
        o->pos.vy -= 0x28;
        o->scene.vy -= 0x28;
        snd_request.id = 0x29;
        snd_request.arg[0] = (o->pos.vx & 0x1FF) >> 2;
        snd_request.arg[1] = 0xE7;
        snd_submit_request();
        break;
    case 0x18:
        if (!o->frame) {
            o->pos.vx -= 0x10;
            o->scene.vx -= 0x10;
        } else {
            o->pos.vx += 0x10;
            o->scene.vx += 0x10;
        }
        o->frame = 0;
        break;
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_hit_by);
#endif

/* Fires a shot of kind n (0 or 1: kind 0x18) from src into the first free slot from 40 on; returns the slot or -1. */
#ifdef NON_MATCHING
/* Frame 72 against 56 (16 bytes of unused locals in the game); the data base isn't kept in a register; a range test
 * folded; a0/a1 swapped in the fall loop. */
short condor_fire_shot(Unit *src, int n)
{
    unsigned short spot[4];
    Unit *o;
    Unit *p;
    unsigned short slot;
    unsigned short op;
    int wait;
    int stage;
    unsigned char *scripts;
    short d;
    short k;
    int acc;
    int i;
    int kind;

    p = &D_800BE8C0[40];
    for (i = 40; i < 69; i++, p++) {
        if (!p->flags) {
            slot = i;
            goto found;
        }
    }
    slot = 0xFFFF;
found:
    if (slot != 0xFFFF) {
    kind = n;
    if ((short)n < 2)
        kind = 0x18;
    D_800BE8BC->kind = kind;
    D_800BE8BC->time = D_800BE8BC->x03 = 0;
    D_800BE8BC->x = D_800BE8BC->y = 0;
    spot[0] = D_800BE8BC->x;
    spot[1] = D_800BE8BC->y;
    o = &D_800BE8C0[slot];
    condor_init_unit(o, D_800BE8BC, slot, spot);
    o->pos.vx = src->pos.vx;
    o->pos.vy = src->pos.vy;
    o->pos.vz = src->pos.vz;
    o->scene.vx = src->scene.vx;
    o->scene.vy = src->scene.vy;
    o->scene.vz = src->scene.vz;
    o->dir = src->dir;
    o->pos.vz -= 0x14;
    o->scene.vz -= 0x14;
    o->power = src->power;
    o->act_wait = 1;
    o->rate = 1;
    o->type = src->kind;
    if ((short)n < 0x19) {
        if ((short)n < 2) {
            o->frame = n;
            o->hp = src->hp;
            o->type = src->type;
        } else {
            o->hp = src->reach;
        }
        condor_hit_by(src, o);
        scripts = CONDOR_DATA;
        op = *(unsigned short *)(scripts + (unsigned short)(o->script + o->script_pos));
        if (op == 0xFFFF) {
            o->hold = 0xFF;
            condor_build_unit_model(o);
        } else {
            if (op == 0xFFFE) {
                o->script_pos = 0;
                op = *(unsigned short *)(scripts + o->script);
            }
            wait = op & 0x80;
            stage = op & 0xFF;
            if (wait)
                stage = 0;
            o->frame = stage;
            o->hold = op >> 8;
            if (!wait)
                condor_build_unit_model(o);
            o->script_pos += 2;
        }
    }
    o->max_hp = o->hp;
    if (o->frame < 4 && o->frame >= 2)
        condor_turn_step(o->dir, src->reach);
    else
        condor_turn_step(o->dir, 100);
    o->tx = o->pos.vx + D_8012C544;
    o->ty = o->pos.vy + D_8012C548;
    condor_aim(o->pos.vx, o->pos.vy, o->tx, o->ty);
    o->vel.vx = D_8012C544;
    o->vel.vy = D_8012C548;
    if (o->frame < 4)
    if (o->frame >= 2) {
        /* the fall speed that lands the shot on its target: d * 2 - 0x14 * (d / 2) */
        d = o->vel.vx == 0x100 ? o->pos.vx - o->tx : o->pos.vy - o->ty;
        if (d < 0)
            d = -d;
        acc = 0;
        for (k = 0; k < d / 2; k++)
            acc += 0x14;
        o->fall = d * 2 - acc;
    }
    return (short)slot;
    }
    return -1;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_fire_shot);
#endif

/* Puts the t (slot 69) where the followed unit stands and aims it; in states 8 and 0x10 also places the condor-side
 * unit for it (slot 68 for kinds 5 and 6, 67 for the rest). */
void condor_place_fort(void)
{
    Unit *t;
    Unit *s;
    int ang;
    int st;

    if (D_8012C41C != -1) {
        t = &D_800BE8C0[69];
        s = &D_800BE8C0[D_8012C41C];
        t->pos.vx = s->pos.vx;
        t->pos.vy = s->pos.vy;
        t->pos.vz = s->pos.vz;
        t->scene.vx = s->scene.vx;
        t->scene.vy = s->scene.vy;
        t->scene.vz = s->scene.vz;
        ang = s->dir;
        t->pos.vz -= 10;
        t->scene.vz -= 10;
        t->dir = ang;
        t->scene.vy -= D_8012C760[s->kind];
        t->rot.vx = 0;
        t->rot.vy = 0;
        t->rot.vz = ang;
        t->heading = 0;
        if (!D_8012C3EC) {
            condor_turn_step(ang, 100);
            t->tx = t->pos.vx + D_8012C544;
            t->ty = t->pos.vy + D_8012C548;
            condor_aim(t->pos.vx, t->pos.vy, t->tx, t->ty);
            t->vel.vx = D_8012C544;
            t->vel.vy = D_8012C548;
        }
    } else {
        D_800BE8C0[69].scene.vx = 0x400;
        D_800BE8C0[69].pos.vx = 0x400;
        D_800BE8C0[69].rot.vx = 0;
        D_800BE8C0[69].rot.vy = 0;
        D_800BE8C0[69].rot.vz = 0;
        D_800BE8C0[68].scene.vx = 0x400;
        D_800BE8C0[68].pos.vx = 0x400;
        D_800BE8C0[68].rot.vx = 0;
        D_800BE8C0[68].rot.vy = 0;
        D_800BE8C0[68].rot.vz = 0;
        D_800BE8C0[67].scene.vx = 0x400;
        D_800BE8C0[67].pos.vx = 0x400;
        D_800BE8C0[67].rot.vx = 0;
        D_800BE8C0[67].rot.vy = 0;
        D_800BE8C0[67].rot.vz = 0;
    }
    st = D_8012C3EC;
    if (st != 8 && st != 0x10)
        return;
    s = &D_800BE8C0[D_8012C41C];
    switch (s->kind) {
    case 5:
        t = &D_800BE8C0[68];
        D_800BE8C0[68].frame = 0;
        condor_build_tower_model(t, 1);
        D_800BE8C0[68].scale.vz = D_8012C388 / D_8012C448;
        break;
    case 6:
        t = &D_800BE8C0[68];
        D_800BE8C0[68].frame = 0;
        condor_build_tower_model(t, 1);
        D_800BE8C0[68].scale.vz = D_8012C388 / D_8012C448 * 100 / 75;
        break;
    default:
    case 7:
        t = &D_800BE8C0[67];
        D_800BE8C0[67].frame = 1;
        condor_build_tower_model(t, 2);
        break;
    }
    t->pos.vx = s->pos.vx;
    t->pos.vy = s->pos.vy;
    t->pos.vz = s->pos.vz;
    t->scene.vx = s->scene.vx;
    t->scene.vy = s->scene.vy;
    t->scene.vz = s->scene.vz;
    t->dir = s->dir;
    t->pos.vz -= 10;
    t->rot.vy = 0;
    t->rot.vz = 0;
    t->heading = 0;
    t->scene.vz -= 10;
    t->rot.vx = 0x400;
    if (t - D_800BE8C0 == 68) {
        t->rot.vy = s->dir;
        return;
    }
    condor_turn_step(s->heading, D_8012C5DC);
    t->scene.vx = s->scene.vx + D_8012C544;
    t->scene.vy = s->scene.vy + D_8012C548 - 0x28;
    if (s->kind == 8)
        t->scale.vx = D_8012C388 / D_8012C448 * 2;
    else
        t->scale.vx = D_8012C388 / D_8012C448;
}

/* The fort and the two units beside it at the top of the table (slots 67-69). */
#ifdef NON_MATCHING
/* The game stores unit 68's frame by absolute address; GCC reaches it through the unit pointer. */
void condor_spawn_fort(void)
{
    unsigned short spot[4];
    Unit *o = &D_800BE8C0[69];

    D_800BE8BC->time = D_800BE8BC->x03 = 0;
    D_800BE8BC->x = D_800BE8BC->y = 0;
    spot[0] = spot[1] = spot[2] = 0;
    D_800BE8BC->kind = 0x1A;
    condor_init_unit(o, D_800BE8BC, 69, spot);
    o--;
    D_800BE8BC->kind = 0x1B;
    condor_init_unit(o, D_800BE8BC, 68, spot);
    D_800BE8C0[68].frame = 0;
    condor_build_tower_model(o, 1);
    o--;
    D_800BE8BC->kind = 0x1B;
    condor_init_unit(o, D_800BE8BC, 67, spot);
    D_800BE8C0[67].frame = 1;
    condor_build_tower_model(o, 2);
    D_800BE8C0[69].scene.vx = 0x400;
    D_800BE8C0[69].pos.vx = 0x400;
    D_800BE8C0[69].rot.vx = 0;
    D_800BE8C0[69].rot.vy = 0;
    D_800BE8C0[69].rot.vz = 0;
    D_800BE8C0[68].scene.vx = 0x400;
    D_800BE8C0[68].pos.vx = 0x400;
    D_800BE8C0[68].rot.vx = 0;
    D_800BE8C0[68].rot.vy = 0;
    D_800BE8C0[68].rot.vz = 0;
    D_800BE8C0[67].scene.vx = 0x400;
    D_800BE8C0[67].pos.vx = 0x400;
    D_800BE8C0[67].rot.vx = 0;
    D_800BE8C0[67].rot.vy = 0;
    D_800BE8C0[67].rot.vz = 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_spawn_fort);
#endif

/* Puts a unit of kind in the first free slot from 40 on, where src stands; returns the slot or -1. */
int condor_spawn_at(Unit *src, int kind)
{
    unsigned short spot[4];
    Unit *o;
    Unit *p;
    unsigned short n;
    int i;
    int ret;

    p = &D_800BE8C0[40];
    for (i = 40; i < 69; i++, p++) {
        if (!p->flags) {
            n = i;
            goto found;
        }
    }
    n = 0xFFFF;
found:
    if (n == 0xFFFF) {
        ret = -1;
    } else {
        D_800BE8BC->kind = kind;
        D_800BE8BC->time = D_800BE8BC->x03 = 0;
        D_800BE8BC->x = D_800BE8BC->y = 0;
        spot[0] = D_800BE8BC->x;
        spot[1] = D_800BE8BC->y;
        o = &D_800BE8C0[n];
        condor_init_unit(o, D_800BE8BC, n, spot);
        o->pos.vx = src->pos.vx;
        o->pos.vy = src->pos.vy;
        o->pos.vz = src->pos.vz;
        o->scene.vx = src->scene.vx;
        o->scene.vy = src->scene.vy;
        o->scene.vz = src->scene.vz;
        o->dir = src->dir;
        ret = (short)n;
    }
    return ret;
}

/* Sets up frame db's primitives in g, and clears the units and bursts (a1 and a2 go unused). */
void condor_init_prims(DrawBuf *g, int a1, int a2, short db)
{
    SPRT_16 *s;
    POLY_GT3 *t;
    POLY_FT4 *f;
    POLY_F4 *q;
    POLY_F3 *d;
    Unit *o;
    UnitFace *sh;
    TowerFace *m;
    Blast *b;
    Quad *sp;
    int i, j;

    D_8012C434 = 0x60;
    D_8012C438 = 0;
    D_8012C43C = 0;
    func_80044A68(&g->mode[0], 0, 0, GetTPage(1, 0, 0x140, 0), 0);
    func_80044A68(&g->mode[1], 0, 0, GetTPage(1, 0, 0x1C0, 0), 0);
    func_80044A68(&g->mode[2], 0, 0, GetTPage(1, 0, 0x140, 0x100), 0);
    func_80044A68(&g->mode[3], 0, 0, GetTPage(1, 0, 0x1C0, 0x100), 0);
    func_80044A68(&g->mode[4], 0, 0, GetTPage(1, 0, 0x240, 0), 0);
    func_80044A68(&g->mode[5], 0, 0, GetTPage(1, 0, 0x2C0, 0), 0);
    func_80044A68(&g->mode[6], 0, 0, GetTPage(1, 0, 0x240, 0x100), 0);
    func_80044A68(&g->mode[7], 0, 0, GetTPage(1, 0, 0x2C0, 0x100), 0);
    s = g->map;
    for (j = 0; j < 14; j++) {
        for (i = 0; i < 21; s++, i++) {
            SetSprt16(s);
            func_80046848(s, 0);
            s->clut = GetClut(0, 0x1F0);
            s->u0 = 0;
            s->v0 = 0;
            s->r0 = 0;
            s->g0 = 0;
            s->b0 = 0;
        }
    }
    o = D_800BE8C0;
    for (j = 0; j < 70; o++, j++) {
        sh = D_800C0990[j].faces;
        for (i = 0; i < 52; i++, sh++) {
            SetPolyG3(&sh->prim[db]);
            func_80046848(&sh->prim[db], 0);
        }
        o->flags = 0;
    }
    for (j = 0; j < 3; j++) {
        m = D_80107C28[j].faces;
        for (i = 0; i < 52; i++, m++) {
            t = &m->prim[db];
            SetPolyGT3(t);
            func_80046848(t, 1);
            t->u0 = t->u1 = t->u2 = 0x20;
            t->v0 = t->v1 = t->v2 = 0x10;
            t->tpage = GetTPage(0, 1, 0x340, 0);
            t->clut = GetClut(0x340, 0xF1);
        }
    }
    s = g->sprt;
    for (i = 0; i < 300; s++, i++) {
        SetSprt16(s);
        func_80046870(s, 1);
        s->r0 = 0xFF;
        s->g0 = 0xFF;
        s->b0 = 0xFF;
    }
    f = g->blast;
    b = D_8011CB3C;
    for (i = 0; i < 8; f++, i++, b++) {
        b->step = 0xFF;
        b->timer = 0;
        SetPolyFT4(f);
        func_80046870(f, 0);
        func_80046848(f, 0);
        f->r0 = 0x80;
        f->g0 = 0x80;
        f->b0 = 0x80;
        f->tpage = GetTPage(0, 0, 0x340, 0);
        f->clut = GetClut(0x340, 0xF1);
        f->u0 = 0;
        f->v0 = 0;
        f->u1 = 0x3F;
        f->v1 = 0;
        f->u2 = 0;
        f->v2 = 0x20;
        f->u3 = 0x3F;
        f->v3 = 0x20;
    }
    f = &g->panel;
    SetPolyFT4(f);
    func_80046870(f, 0);
    func_80046848(f, 1);
    g->panel.r0 = 0x80;
    g->panel.g0 = 0x80;
    g->panel.b0 = 0x80;
    g->panel.tpage = GetTPage(0, 0, 0x380, 0);
    g->panel.clut = GetClut(0x380, 0xF2);
    g->panel.u0 = 0;
    g->panel.v0 = 0x30;
    g->panel.u1 = 0x5F;
    g->panel.v1 = 0x30;
    g->panel.u2 = 0;
    g->panel.v2 = 0x70;
    g->panel.u3 = 0x5F;
    g->panel.v3 = 0x70;
    sp = &g->shade;
    q = &sp->poly[db];
    SetPolyF4(q);
    func_80046870(q, 0);
    func_80046848(q, 1);
    q->r0 = 0x40;
    q->g0 = 0x40;
    q->b0 = 0x40;
    g->shade.v[0].vx = -0x20;
    g->shade.v[0].vy = -8;
    g->shade.v[0].vz = D_8012C388;
    g->shade.v[1].vx = 0x20;
    g->shade.v[1].vy = -8;
    g->shade.v[1].vz = D_8012C388;
    g->shade.v[2].vx = -0x20;
    g->shade.v[2].vy = 8;
    g->shade.v[2].vz = D_8012C388;
    g->shade.v[3].vx = 0x20;
    g->shade.v[3].vy = 8;
    g->shade.v[3].vz = D_8012C388;
    func_80044A68(&sp->mode, 0, 0, GetTPage(0, 2, 0x350, 0x10), 0);
    for (i = 15; i >= 0; i--)
        D_8012C5FC[i].age = 0;
    SetLineF2(&g->line);
    func_80046870(&g->line, 0);
    func_80046848(&g->line, 1);
    g->line.r0 = 0;
    g->line.g0 = 0;
    g->line.b0 = 0;
    g->line.x0 = 0;
    g->line.x1 = 0x200;
    g->line.y1 = 0x200;
    g->line.y0 = 0x200;
    for (i = 0; i < 4; i++) {
        d = &g->mark[i];
        SetPolyF3(d);
        func_80046870(d, 0);
        func_80046848(d, 1);
        switch (i) {
        case 0:
            g->mark[0].r0 = 0xC0;
            g->mark[0].g0 = 0xC0;
            g->mark[0].b0 = 0xC0;
            break;
        case 1:
            g->mark[1].r0 = 0x60;
            g->mark[1].g0 = 0x10;
            g->mark[1].b0 = 0x10;
            break;
        case 2:
            g->mark[2].r0 = 0x10;
            g->mark[2].g0 = 0x60;
            g->mark[2].b0 = 0x10;
            break;
        case 3:
            g->mark[3].r0 = 0x10;
            g->mark[3].g0 = 0x60;
            g->mark[3].b0 = 0x60;
            break;
        }
    }
}

#ifdef NON_MATCHING
/* The game has a dead `andi 0x8000` after each corner load, a 0x88 frame with six zeroed shorts, single-store
 * min/max. */
void condor_cell_box(Cell *t)
{
    short s0[4], s1[4];
    unsigned char rest[0x40];
    short a, b, c;
    int v;

    s0[0] = 0;
    s0[1] = 0;
    s0[2] = 0;
    s1[0] = 0;
    s1[1] = 0;
    s1[2] = 0;
    /* biased so the box works on off-screen corners too */
    v = t->scr[0].vx;
    a = SX(v) + 0x4000;
    v = t->scr[1].vx;
    b = SX(v) + 0x4000;
    v = t->scr[2].vx;
    c = SX(v) + 0x4000;
    t->x0 = MIN3(a, b, c);
    t->x1 = MAX3(a, b, c);
    v = t->scr[0].vy;
    a = SX(v) + 0x4000;
    v = t->scr[1].vy;
    b = SX(v) + 0x4000;
    v = t->scr[2].vy;
    c = SX(v) + 0x4000;
    t->y0 = MIN3(a, b, c);
    t->y1 = MAX3(a, b, c);
    t->rot_x = 0x100;
    t->rot_z = 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_cell_box);
#endif

/* Builds the walls at dest from the field file part at src: 0x4C-byte walls first, an outline (LINE_F4) for each at
 * +0x9800 and a copy of each outline at +0xD000 for the other frame. */
#ifdef NON_MATCHING
/* Two addiu swapped in the prologue (the pointer induction setup). */
void condor_build_cells(unsigned char *src, Cell *dest)
{
    long sxy;
    long p, flag;
    LINE_F4 *line = (LINE_F4 *)((char *)dest + 0x9800);
    LINE_F4 *line2 = (LINE_F4 *)((char *)dest + 0xD000);
    WallRec *rec;
    short i;
    unsigned int c;

    D_8012C434 = 0x60;
    D_8012C438 = 0x198;

    D_8012C518 = src[0] + (src[1] << 8);
    rec = (WallRec *)(src + 0x10);

    for (i = 0; i < D_8012C518; i++, rec++, dest++, line++) {
        SetLineF4(line);
        func_80046870(line, 1);
        dest->v[0].vx = rec->p[0][0];
        dest->v[0].vy = rec->p[0][1];
        dest->v[0].vz = rec->p[0][2];
        dest->v[1].vx = rec->p[1][0];
        dest->v[1].vy = rec->p[1][1];
        dest->v[1].vz = rec->p[1][2];
        dest->v[2].vx = rec->p[2][0];
        dest->v[2].vy = rec->p[2][1];
        dest->v[2].vz = rec->p[2][2];
        dest->v[3].vx = rec->p[3][0];
        dest->v[3].vy = rec->p[3][1];
        dest->v[3].vz = rec->p[3][2];
        dest->v[0].vy = rec->p[0][2];
        dest->v[1].vy = rec->p[1][2];
        dest->v[2].vy = rec->p[2][2];
        dest->v[3].vy = rec->p[3][2];
        dest->v[0].vz = -rec->p[0][1];
        dest->v[1].vz = -rec->p[1][1];
        dest->v[2].vz = -rec->p[2][1];
        dest->v[3].vz = -rec->p[3][1];
        RotTransPers(&dest->v[1], &sxy, &p, &flag);
        dest->scr[0].vx = (short)sxy - D_8012C6EC.vx - 0xA0;
        dest->scr[0].vy = (sxy >> 16) - D_8012C6EC.vy - 0x78;
        RotTransPers(&dest->v[2], &sxy, &p, &flag);
        dest->scr[1].vx = (short)sxy - D_8012C6EC.vx - 0xA0;
        dest->scr[1].vy = (sxy >> 16) - D_8012C6EC.vy - 0x78;
        RotTransPers(&dest->v[3], &sxy, &p, &flag);
        dest->scr[2].vx = (short)sxy - D_8012C6EC.vx - 0xA0;
        dest->scr[2].vy = (sxy >> 16) - D_8012C6EC.vy - 0x78;
        dest->next[0] = rec->rgb[0];
        dest->next[1] = rec->rgb[1];
        dest->next[2] = rec->rgb[2];
        c = (unsigned short)dest->next[0];
        dest->next[0] = c >> 8 | c << 8;
        c = (unsigned short)dest->next[1];
        dest->next[1] = c >> 8 | c << 8;
        c = (unsigned short)dest->next[2];
        dest->next[2] = c >> 8 | c << 8;
        condor_cell_box(dest);
        line->r0 = 0x80;
        line->g0 = 0;
        line->b0 = 0;
        *line2++ = *line;
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_build_cells);
#endif

/* Sets up the view from the field file (centred at x, y) and loads the field's pictures and data. */
void condor_init_field(int x, int y, int unused)
{
    unsigned int off;

    D_8012C390 = 0;
    InitGeom();
    SetGeomOffset(x, y);
    condor_read_file_lzs(0, 0x80, (void *)0x801B0000);
    off = D_801B0000[0];
    if (off & 3) {
        off &= ~3;
        off += 4; /* each part starts on a word */
    }
    bcopy((void *)(off + 0x801B0000), CAMERA, 0x80);
    D_8012C67C = CAMERA->m;
    D_8012C388 = CAMERA->h;
    SetGeomScreen(D_8012C388);
    SetRotMatrix(&D_8012C67C);
    SetTransMatrix(&D_8012C67C);
    D_8012C6DC.vx = 0x46;
    D_8012C6DC.vy = 0;
    D_8012C6DC.vz = 0;
    D_8012C6EC.vx = 0;
    D_8012C6EC.vy = 0;
    D_8012C6EC.vz = 0;
    RotMatrix(&D_8012C6DC, &D_8012C69C);
    TransMatrix(&D_8012C69C, &D_8012C6EC);
    MulMatrix2(&D_8012C69C, &D_8012C67C);
    SetRotMatrix(&D_8012C67C);
    SetTransMatrix(&D_8012C67C);
    condor_load_background(0x140, 0, 0, 0x1F0);
    condor_read_file_lzs(3, 0x40000, (void *)0x801B0000);
    bcopy((void *)0x801B0000, (void *)0x80188000, 0x8000);
    condor_load_tim(D_801B0000[5], 0x380, 0, 0x380, 0xF0);
    condor_load_tim(D_801B0000[6], 0x3C0, 0, 0x3C0, 0xF0);
    condor_load_tim(D_801B0000[7], 0x340, 0, 0x340, 0xF0);
    off = D_801B0000[8];
    if (off & 3) {
        off &= ~3;
        off += 4;
    }
    bcopy((void *)(off + 0x801B0000), (void *)0x80190000, 0x8000);
    off = D_801B0000[9];
    if (off & 3) {
        off &= ~3;
        off += 4;
    }
    bcopy((void *)(off + 0x801B0000), (void *)0x80198000, 0x8000);
    off = D_801B0000[10];
    if (off & 3) {
        off &= ~3;
        off += 4;
    }
    bcopy((void *)(off + 0x801B0000), (void *)0x801A0000, 0x8000);
    off = D_801B0000[11];
    if (off & 3) {
        off &= ~3;
        off += 4;
    }
    bcopy((void *)(off + 0x801B0000), (void *)0x801A8000, 0x8000);
}

void condor_load_data(void)
{
    unsigned char init[24] = {
        0x00, 0x28, 0x28, 0x2C, 0x28, 0x14, 0x1C, 0x2C, 0x2C, 0x00, 0x00, 0x00,
        0x28, 0x28, 0x14, 0x14, 0x30, 0x26, 0x1E, 0x28, 0x14, 0x14, 0x14, 0x14,
    };
    unsigned int i;
    unsigned int off;

    for (i = 0; i < 24; i++)
        D_8012C760[i] = init[i];
    condor_read_file(4, 0x10000, (void *)0x80180000);
    condor_read_file_lzs(0, 0x40000, (void *)0x801B0000);
    off = D_801B0000[2];
    if (off & 3) {
        off &= ~3;
        off += 4; /* each part starts on a word */
    }
    condor_build_cells((void *)(off + 0x801B0000), D_8010C33C);
    off = D_801B0000[3];
    if (off & 3) {
        off &= ~3;
        off += 4;
    }
    bcopy((void *)(off + 0x801B0000), (void *)0x801B0000, 0x40000);
}

/* Uploads the TIM at off in the file buffer at 0x801B0000: its pixels to (x, y), its CLUT to (cx, cy). Returns the
 * pixel block's size, w low and h high. */
int condor_load_tim(unsigned int off, int x, int y, int cx, int cy)
{
    TIM_IMAGE tim;
    int size = 0;

    OpenTIM((unsigned int *)((off >> 2 << 2) + 0x801B0000));
    ReadTIM(&tim);
    if (tim.paddr) {
        D_8012AF3C.x = x;
        D_8012AF3C.y = y;
        D_8012AF3C.w = tim.prect->w;
        D_8012AF3C.h = tim.prect->h;
        func_80044000(&D_8012AF3C, tim.paddr);
        size = tim.prect->w | tim.prect->h << 16;
    }
    if (tim.caddr) {
        D_8012AF3C.x = cx;
        D_8012AF3C.y = cy;
        D_8012AF3C.w = tim.crect->w;
        D_8012AF3C.h = tim.crect->h;
        func_80044000(&D_8012AF3C, tim.caddr);
    }
    DrawSync(0);
    return size;
}

/* Loads the background: two 256-colour CLUTs at (cx, cy) and (cx, cy + 8), a 512x512 8-bit picture from (x, y), and
 * one more CLUT at (0, 497). */
int condor_load_background(int x, int y, int cx, int cy)
{
    TIM_IMAGE tim; /* unused, but the frame has it */

    condor_read_file_lzs(1, 0x48000, (void *)0x801B0000);
    setRECT(&D_8012AF3C, cx, cy, 0x100, 1);
    func_80044000(&D_8012AF3C, (void *)0x801B0014);
    DrawSync(0);
    setRECT(&D_8012AF3C, cx, cy + 8, 0x100, 1);
    func_80044000(&D_8012AF3C, (void *)0x801B0014);
    DrawSync(0);
    setRECT(&D_8012AF3C, x, y, 0x100, 0x200);
    func_80044000(&D_8012AF3C, (void *)0x801B0220);
    DrawSync(0);
    condor_read_file_lzs(2, 0x48000, (void *)0x801B0000);
    setRECT(&D_8012AF3C, x + 0x100, y, 0x100, 0x200);
    func_80044000(&D_8012AF3C, (void *)0x801B0000);
    DrawSync(0);
    condor_read_file_lzs(5, 0x130000, (void *)0x80168000);
    setRECT(&D_8012AF3C, 0, 0x1F1, 0x100, 1);
    func_80044000(&D_8012AF3C, (void *)0x80168014);
    DrawSync(0);
    return 0;
}

void condor_read_file(int i, int room, void *dest)
{
    cd_read(DISC_FILE(D_800BE7D8[i]), dest, condor_read_done);
    while (cd_step())
        ;
}

void condor_read_file_lzs(int i, int room, void *dest)
{
    cd_read_lzs(DISC_FILE(D_800BE7D8[i]), dest, condor_read_done);
    while (cd_step())
        ;
}

/* Whether the unit walks (roles 1, 2 and 5-7), and so bumps into foes. Non-static inline, returning short as
 * its callers read it: the callers after it have it inlined. */
inline short condor_role_walks(Unit *o)
{
    int r = 0;

    switch (o->role) {
    case 1:
    case 2:
    case 5:
    case 6:
    case 7:
        r = 1;
    }
    return r;
}

/* Tints the dying unit's model red by its fade. */
void condor_tint_model_red(Unit *o)
{
    TowerFace *m = D_8012C368[0].faces;
    int n = D_8012C368[0].count;
    int i;

    for (i = 0; i < n; i++, m++) {
        (D_8012C3F0 + m->prim)->r0 = o->fade;
        (D_8012C3F0 + m->prim)->g0 = 0;
        (D_8012C3F0 + m->prim)->b0 = 0;
        (D_8012C3F0 + m->prim)->r1 = o->fade;
        (D_8012C3F0 + m->prim)->g1 = 0;
        (D_8012C3F0 + m->prim)->b1 = 0;
        (D_8012C3F0 + m->prim)->r2 = o->fade;
        (D_8012C3F0 + m->prim)->g2 = 0;
        (D_8012C3F0 + m->prim)->b2 = 0;
    }
}

void condor_animate_troops(void)
{
    Unit *o = D_800BE8C0;
    int i;

    for (i = 0; i < 40; i++, o++) {
        if (o->flags)
            condor_run_anim_script(o);
    }
}

/* Steps the units in slots 0-39: one dying unit at a time flashes and goes; the others act on their timers. Also
 * tracks the lowest y of slots of kind 0x10 and up and the highest of the rest. */
#ifdef NON_MATCHING
/* Only the sound request, as condor_start_burst. */
int condor_update_troops(void)
{
    Unit *o;
    int lo = 0x400;
    int hi = 0x1E0;
    int i;
    int y;
    short was;

    for (o = D_800BE8C0, i = 0; i < 40; o++, i++) {
        if (!o->flags)
            continue;
        if (!o->fade) {
            if (o->flags & 0x10)
                condor_tick_timer(o);
            if (!o->fade)
                goto alive;
        }
        if (!D_8012C490) {
            if (o->fade != 0xFF)
                continue;
            o->fade = 0x50;
            snd_request.id = 0x2A;
            snd_request.arg[1] = 0x16;
            snd_request.arg[0] = (o->pos.vx & 0x1FF) >> 2;
            snd_submit_request();
            condor_build_tower_model(o, 0);
            D_8012C490 = 1;
            D_8012C494 = i;
            continue;
        }
        if (D_8012C494 != i)
            continue;
        if (o->fade != 0xFF) {
            o->fade -= 2;
            if (!o->fade) {
                o->fade = 0;
                o->flags = 0;
                D_8012C494 = -1;
                D_8012C490 = 0;
                if (o->kind == 0x10 && !D_8012C414)
                    D_8012C414 = 1;
                continue;
            }
            condor_tint_model_red(o);
        } else {
            o->fade = 0x3C;
        }
        continue;
    alive:
        if (o->kind >= 0x10) {
            if (o->pos.vy <= lo)
                lo = o->pos.vy;
        } else {
            y = (short)(o->pos.vy + (o->sight << 4));
            if (y >= hi)
                hi = y;
        }
        if (!(o->flags & 4)) {
            if (D_8012C3E8 < 2 && !D_8012C42C) {
                o->act_wait -= D_8012C5D4;
                if (!o->act_wait || o->act_wait > 0xFEFF) {
                    o->act_wait = o->rate;
                    condor_step_unit(o, i);
                }
            }
        } else {
            if (D_8012C3E8 < 2 && !D_8012C5CC && condor_role_walks(o))
                condor_ranged_fight(o, i);
        }
        condor_run_anim_script(o);
    }
    was = D_8012C52C;
    if (lo <= 0x1C0)
        D_8012C52C = 0x60;
    else
        D_8012C52C = 0x60 - (lo - 0x1C0) / 6;
    if (D_8012C52C < 0x58 && was >= 0x58) {
        snd_request.id = 0x28;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 0;
        snd_submit_request();
    }
    D_8012C530 = hi;
    if (hi >= 0x3A0)
        D_8012C530 = 0x3A0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_update_troops);
#endif

void condor_tick_timer(Unit *u)
{
    if (u->timer) {
        u->timer--;
        if (u->timer == 0) {
            u->flags &= 0xEF;
            if (u->pose != 2) {
                u->pose = 1;
                condor_start_script(u);
            }
        }
    }
}

/* Heals the unit, shows the number and plays the sound. */
void condor_heal_unit(Unit *u)
{
    unsigned short heal;
    short slot;
    short hp;

    heal = DATA_HALF(0x1C);
    u->flags &= 0xBF;
    slot = free_popup();
    if (slot != 0xFF) {
        D_8012C5FC[slot].age = -0x7FFF;
        D_8012C5FC[slot].value = heal;
        D_8012C5FC[slot].x = u->pos.vx;
        D_8012C5FC[slot].y = u->pos.vy;
        condor_draw_counter_digits(slot, (short)heal);
    }
    snd_request.id = 0x2B;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 1;
    snd_submit_request();
    hp = u->hp;
    hp += heal;
    if (hp >= u->max_hp) {
        u->flags &= 0xBF;
        hp = u->max_hp;
    }
    u->hp = hp;
    if (D_8012C41C != -1)
        condor_draw_hp();
}

void condor_step_unit(Unit *u, int i)
{
    unsigned short flags = u->flags;

    if (flags & 0x40)
        condor_heal_unit(u);
    if (!(flags & 0x50))
        condor_update_unit(u, i);
}

/* A ranged unit's fight step: drops a foe it can no longer reach, else takes aim and fires when the shot is free. */
#ifdef NON_MATCHING
/* `== 1` on condor_fight_step's result: the value and constant registers swapped. */
void condor_ranged_fight(Unit *u, int i)
{
    if (condor_unit_overlaps(u, u->foe) != -1) {
        if (!u->phase) {
            if (!D_800BE8C0[D_800BE8C0[i].foe].phase)
                u->phase = 1;
        } else if (!D_8012C3EC) {
            if (condor_fight_step(i, D_800BE8C0[i].foe) == 1) {
                u->phase = 0;
                u->damage = 0x78;
            }
        }
        return;
    }
    u->flags = 1;
    u->foe = 0xFF;
    u->phase = 0;
    u->damage = 0;
    u->x2F = 0;
    if (u->kind < 0x10) {
        u->dir = 0;
        return;
    }
    if (u->path & 0xFF)
        u->path--;
    condor_next_waypoint(u);
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_ranged_fight);
#endif

#ifdef NON_MATCHING
/* The game keeps 0x80180000 in a register and masks the script word with andi 0xFFFF in its compares. */
void condor_run_anim_script(Unit *u)
{
    unsigned short flash;
    int cmd;

    flash = u->flash & 0x1FF;
    if (u->hold != 0xFF) {
        if (u->hold == 0) {
            cmd = DATA_HALF((unsigned short)(u->script + u->script_pos));
            if ((unsigned short)cmd == 0xFFFF) {
                u->hold = 0xFF;
                condor_build_unit_model(u);
                cmd = 0;
            } else {
                if ((unsigned short)cmd == 0xFFFE) {
                    u->script_pos = 0;
                    cmd = DATA_HALF(u->script);
                }
                u->frame = cmd & 0x80 ? 0 : cmd & 0xFF;
                u->hold = (unsigned short)cmd >> 8;
                if (!(cmd & 0x80))
                    condor_build_unit_model(u);
                u->script_pos += 2;
            }
            if (cmd & 0x80) {
                if (!(u->flash & 0x200)) {
                    flash += 0x10;
                    u->flash = flash;
                    if (flash >= 0x1FF)
                        u->flash = 0x3FF;
                } else {
                    flash -= 0x10;
                    u->flash = flash | 0x200;
                    if (flash > 0xFEFF)
                        u->flash = 0;
                }
            }
        } else
            u->hold--;
    }
    u->heading = u->dir;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_run_anim_script);
#endif

/* Turns the unit toward its walk target (a tower just turns). */
void condor_face_target(Unit *u)
{
    if (u->kind >= 0x10) {
        if (u->path & 0xFF)
            u->path--;
        condor_next_waypoint(u);
    } else {
        u->vel.vx = 0;
        u->vel.vy = 0;
        u->vel.vz = 0;
        if (u->tx | u->ty) {
            condor_aim(u->pos.vx, u->pos.vy, u->tx, u->ty);
            u->vel.vx = D_8012C544;
            u->vel.vy = D_8012C548;
        }
        u->dir = condor_square_angle(&u->vel);
    }
    u->flags &= 0xF7;
}

/* Runs one unit's step: notes where it stands (for condor_find_detour's detours), then its kind's handler. */
void condor_update_unit(Unit *u, int i)
{
    int spare[4]; /* never used, but the game's frame has room for it */

    D_8012C58C = u->sub_x;
    D_8012C590 = u->sub_y;
    D_8012C594.vx = u->pos.vx;
    D_8012C594.vy = u->pos.vy;
    D_8012C594.vz = u->pos.vz;
    D_8012C59C.vx = u->scene.vx;
    D_8012C59C.vy = u->scene.vy;
    D_8012C59C.vz = u->scene.vz;
    if (u->foe == 0xFF) {
        switch (u->role) {
        case 0:
            break;
        case 1:
            condor_update_soldier(u, i);
            break;
        case 2:
            condor_update_archer(u, i);
            break;
        case 3:
        case 4:
            condor_update_tower(u, i);
            break;
        case 5:
            condor_update_healer(u, i);
            break;
        case 6:
            condor_update_bomber(u, i);
            break;
        case 7:
            condor_update_attacker(u, i);
            break;
        }
    } else
        u->act &= 0xFD;
}

/* The unit walked off the route: tries six turns, nearest first; the cell the first good one reaches, else 0. */
short condor_find_detour(Unit *u)
{
    short cell;

    if ((cell = condor_try_turn(u, -0x80)) != -1)
        return cell;
    if ((cell = condor_try_turn(u, 0x80)) != -1)
        return cell;
    if ((cell = condor_try_turn(u, -0x420)) != -1)
        return cell;
    if ((cell = condor_try_turn(u, 0x420)) != -1)
        return cell;
    if ((cell = condor_try_turn(u, -0x720)) != -1)
        return cell;
    if ((cell = condor_try_turn(u, 0x720)) != -1)
        return cell;
    return 0;
}

/* Moves the unit two steps; if that left the route, tries detours, and failing those puts it back. */
#ifdef NON_MATCHING
/* The three paths into the last condor_apply_velocity call: the game sets a0 in each one's delay slot and leaves a nop
 * after the jal. */
void condor_step_or_detour(Unit *u)
{
    Cell *r;
    short cell;

    condor_apply_velocity(u);
    condor_apply_velocity(u);
    cell = condor_update_cell(u);
    if (cell != -1) {
        u->cell = cell;
        r = &D_8010C33C[u->cell];
        u->rot.vx = r->rot_x;
        u->rot.vz = r->rot_z;
        u->scene.vz = D_8012C388;
        condor_restore_place(u);
    } else {
        u->flags |= 0x20;
        condor_restore_place(u);
        if (!condor_turn_at_edge(u) && !condor_find_detour(u)) {
            condor_restore_place(u);
            return;
        }
    }
    condor_apply_velocity(u);
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_step_or_detour);
#endif

/* After a step: keeps the unit's route cell up to date (or finds its way back), stops it at its target, and on
 * arrival lets the next unit move and shows its panel. */
#ifdef NON_MATCHING
/* Register choice for the cell pointer (v0 against a0) and where D_8012C388's load sits in the cell blocks. */
void condor_after_step(Unit *u)
{
    Cell *r;
    short cell;

    cell = condor_same_cell(u);
    if (u->flags & 0x20) {
        if (cell != u->cell) {
            cell = condor_update_cell(u);
            if (cell == -1)
                condor_step_or_detour(u);
            else {
                u->cell = cell;
                r = &D_8010C33C[u->cell];
                u->rot.vx = r->rot_x;
                u->rot.vz = r->rot_z;
                u->scene.vz = D_8012C388;
            }
        } else {
            condor_face_target(u);
            condor_apply_velocity(u);
            cell = condor_update_cell(u);
            if (cell == -1)
                condor_step_or_detour(u);
            else {
                u->cell = cell;
                r = &D_8010C33C[u->cell];
                u->rot.vx = r->rot_x;
                u->rot.vz = r->rot_z;
                u->scene.vz = D_8012C388;
                u->flags &= 0xDF;
            }
        }
    } else if (cell != u->cell) {
        cell = condor_update_cell(u);
        if (cell == -1)
            condor_step_or_detour(u);
        else {
            u->cell = cell;
            r = &D_8010C33C[u->cell];
            u->rot.vx = r->rot_x;
            u->rot.vz = r->rot_z;
            u->scene.vz = D_8012C388;
        }
    }
    if (u->tx == u->pos.vx)
        u->vel.vx = 0;
    if (u->ty == u->pos.vy)
        u->vel.vy = 0;
    if (u->vel.vx | u->vel.vy)
        return;
    u->ty = 0;
    u->tx = 0;
    if (u->kind >= 0x10) {
        condor_next_waypoint(u);
        return;
    }
    u->mode = 0;
    u->vel.vx = 0;
    u->vel.vy = 0;
    u->vel.vz = 0;
    u->act &= 0xFE;
    if (D_8012C41C == -1) {
        D_8012C41C = u - D_800BE8C0;
        condor_draw_hp();
        D_8012C41C = -1;
    }
    condor_show_unit(u - D_800BE8C0, 4, 3);
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_after_step);
#endif

void condor_move_unit(Unit *u)
{
    if (u->vel.vx | u->vel.vy) {
        condor_apply_velocity(u);
        condor_after_step(u);
    }
}

/* Steps the unit by its 8.8 velocity, carrying the fractions. */
void condor_apply_velocity(Unit *u)
{
    unsigned short d;

    d = u->vel.vx + u->sub_x;
    u->sub_x = d;
    d >>= 8;
    if (d) {
        if (d >= 0x80)
            d |= 0xFF00;
        u->pos.vx += d;
        u->scene.vx += d;
    }
    d = u->vel.vy + u->sub_y;
    u->sub_y = d;
    d >>= 8;
    if (d) {
        if (d >= 0x80)
            d |= 0xFF00;
        u->pos.vy += d;
        u->scene.vy += d;
    }
}

/* Bumps unit i into the nearest foe: both turn to face each other and fight; the first clash shows the
 * attacker's panel. */
#ifdef NON_MATCHING
/* One register: the panel's unit index lands in v1, the game's in a1. */
void condor_bump_foe(Unit *u, int i)
{
    Unit *o;
    short hit = -1;
    int engaged;

    if (u->kind < 0x10) {
        if (condor_role_walks(u))
            hit = condor_find_touching_units(u, 20, 40);
    } else
        hit = condor_find_touching_units(u, 0, 20);
    if (hit == -1)
        return;
    o = &D_800BE8C0[hit];
    engaged = (u->flags | o->flags) & 4;
    if (condor_role_walks(u)) {
        u->flags |= 4;
        condor_aim(u->pos.vx, u->pos.vy, o->pos.vx, o->pos.vy);
        u->vel.vx = D_8012C544;
        u->vel.vy = D_8012C548;
        u->dir = condor_square_angle(&u->vel);
        if (u->foe == 0xFF)
            u->foe = hit;
    }
    if (condor_role_walks(o)) {
        o->flags |= 4;
        condor_aim(o->pos.vx, o->pos.vy, u->pos.vx, u->pos.vy);
        o->vel.vx = D_8012C544;
        o->vel.vy = D_8012C548;
        o->dir = condor_square_angle(&o->vel);
        if (o->foe == 0xFF)
            o->foe = i;
    }
    if (u->kind < 0x10) {
        D_8012C424 = i;
        D_8012C3AC[0] = u->pos.vx;
        hit = u - D_800BE8C0;
        D_8012C3AC[1] = u->pos.vy;
    } else {
        D_8012C424 = hit;
        D_8012C3AC[0] = o->pos.vx;
        hit = o - D_800BE8C0;
        D_8012C3AC[1] = o->pos.vy;
    }
    if (engaged)
        return;
    condor_show_unit(hit, 1, 0);
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_bump_foe);
#endif

/* An attacker's choice of target, by its AI: 1 and 4 the first in sight, 2 the most hurt in sight, 3 the most hurt
 * of the type it hunts (0x11 hunts type 3, 0x12 type 4, 0x13 type 2). -1: none. */
#ifdef NON_MATCHING
/* Register allocation throughout (the result in t4 in the game, the list walks in t0/a3/t5/t6); the game keeps the
 * hurt value twice. */
short condor_pick_target(Unit *u)
{
    Unit probe;
    int spare[8]; /* never used, but the game's frame has room for it */
    Found *h;
    short found, best;
    short want, n, k, count;
    Unit *p;

    switch (u->ai) {
    case 1:
        found = condor_look(u, &probe);
        break;
    case 2:
        found = condor_look(u, &probe);
        if (found == -1)
            break;
        best = condor_most_hurt(D_8012B75C, D_8012B85C);
        if (best != -1)
            found = best;
        break;
    case 3:
        found = condor_look(u, &probe);
        if (found == -1)
            break;
        count = D_8012B85C;
        if (!count)
            n = -1;
        else {
            h = D_8012B75C;
            D_8012BA64 = 0;
            n = -1;
            switch (u->kind) {
            case 0x11:
                want = 3;
                break;
            case 0x12:
                want = 4;
                break;
            case 0x13:
                want = 2;
                break;
            default:
                want = 0;
                break;
            }
            for (k = 0; k < count; k++, h++) {
                p = h->unit;
                if (p->kind == want) {
                    n = D_8012BA64 + 1;
                    D_8012B964[D_8012BA64].unit = p;
                    D_8012B964[D_8012BA64].idx = want; /* sic: the type, not the unit */
                    D_8012BA64 = n;
                }
            }
        }
        if (n != -1)
            best = condor_most_hurt(D_8012B964, D_8012BA64);
        else
            best = condor_most_hurt(D_8012B75C, D_8012B85C);
        if (best != -1)
            found = best;
        break;
    case 4:
        if (D_8012C50C == 1) {
            u->act = 1;
            u->mode = 1;
            u->target = 0xFF;
            u->foe = 0xFF;
            u->ai = 0;
            u->path = (u->path >> 8 & 0xF) == 6 ? 0x202 : 0xA02;
            condor_next_waypoint(u);
            found = -1;
            break;
        }
        condor_bump_foe(u, u - D_800BE8C0);
        found = condor_look(u, &probe);
        break;
    default:
        found = -1;
        break;
    }
    return found;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_pick_target);
#endif

/* Kind 7 (the attackers' main force, and type 0x13 shooters): pick a target, chase it, fight; reaching the
 * gate cells (0xCC-0xCF) tells the game the fort is under attack. */
short condor_update_attacker(Unit *u, int i)
{
    short changed;
    short t;
    int self;

    if (u->kind == 0x13 && condor_update_shooter(u) == 1)
        return 0;
    changed = 0;
    switch (u->mode) {
    case 0:
        u->mode = 1;
        u->act = 1;
        if (u->pose) {
            changed = 1;
            u->pose = 0;
            u->act = 0;
        }
        break;
    case 1:
        t = -1;
        if (!u->cooldown)
            t = condor_pick_target(u);
        else
            u->cooldown--;
        if (t == -1) {
            if (u->pose != 1) {
                changed++;
                u->pose = 1;
                u->act |= 1;
            }
            break;
        }
        u->target = t;
        u->mode = 3;
        u->act |= 2;
        D_8012C3AC[0] = D_800BE8C0[t].pos.vx;
        D_8012C3AC[1] = D_800BE8C0[t].pos.vy;
        condor_walk_to_target(u);
        break;
    case 2:
        u->act = 2;
        break;
    case 3:
        if (u->flags & 0x20) {
            if (u->target != 0xFF) {
                u->target = 0xFF;
                u->mode = 1;
                u->cooldown = u->rate;
                if (u->path & 0xFF)
                    u->path--;
                condor_next_waypoint(u);
                if (u->ty >= u->pos.vy)
                    condor_next_waypoint(u);
            }
        } else if (condor_chase(u) == -2) {
            if (u->path & 0xFF)
                u->path--;
            condor_next_waypoint(u);
            if (u->ty >= u->pos.vy)
                condor_next_waypoint(u);
        }
        break;
    }
    if (changed)
        condor_start_script(u);
    self = u - D_800BE8C0;
    if (u->tx | u->ty)
        condor_move_unit(u);
    if (condor_role_walks(u))
        condor_bump_foe(u, self);
    if (u->cell - 0xCC < 4U && !D_8012C414)
        D_8012C414 = 2;
    return 0;
}

/* Kinds 3 and 4 (towers): wait, fire a shot (a unit of type 0x18) and wait for it to land, rest. */
void condor_update_tower(Unit *u, int i)
{
    unsigned short changed = 0;
    short shot;

    switch (u->mode) {
    case 0:
        if (u->wait)
            u->wait--;
        else {
            changed = 1;
            u->mode = 2;
            u->pose = 2;
            u->act |= 2;
        }
        break;
    case 2:
        if (u->hold != 0xFF)
            break;
        if (!u->damage) {
            shot = condor_fire_shot(u, 0x18);
            if (shot != -1)
                u->damage = shot;
        } else if (!D_800BE8C0[u->damage].flags) {
            changed = 1;
            u->mode = 4;
            u->pose = 5;
            u->damage = 0;
            u->wait = u->act_time;
            u->act &= 0xFD;
        }
        break;
    case 4:
        if (u->wait)
            u->wait--;
        else {
            changed = 1;
            u->mode = 0;
            u->pose = 0;
            u->damage = 0;
            u->wait = u->rest_time;
        }
        break;
    }
    if (changed)
        condor_start_script(u);
}

/* Sets the pose that goes with the unit's mode. */
void condor_set_pose(Unit *u)
{
    short changed = 0;

    switch (u->mode) {
    case 0:
        if (u->pose) {
            changed++;
            u->pose = 0;
            u->act = 0;
            u->foe = 0xFF;
            u->vel.vy = 0;
            u->vel.vx = 0;
            u->ty = 0;
            u->tx = 0;
        }
        break;
    case 1:
        if (u->pose != 1) {
            changed++;
            u->pose = 1;
            u->act |= 1;
        }
        break;
    case 2:
        break;
    case 3:
        if (u->pose != 1) {
            changed++;
            u->pose = 1;
            u->act |= 1;
        }
        condor_follow(u);
        break;
    case 4:
        if (u->pose != 6) {
            changed++;
            u->pose = 6;
            u->act |= 4;
        }
        break;
    case 5:
        if (u->pose != 1) {
            changed++;
            u->pose = 1;
            u->act |= 1;
        }
        condor_follow(u);
        break;
    }
    if (changed)
        condor_start_script(u);
}

/* Kind 1 (foot soldiers): state, walk, bump. */
void condor_update_soldier(Unit *u, int i)
{
    int self;

    condor_set_pose(u);
    self = u - D_800BE8C0;
    if (u->tx | u->ty)
        condor_move_unit(u);
    if (condor_role_walks(u))
        condor_bump_foe(u, self);
}

/* Fires a shot (a unit of type 0x18) from the unit at unit i; 1 if one was free. */
short condor_fire_at(Unit *u, int i)
{
    Unit *t;
    short shot;

    shot = condor_fire_shot(u, 0x18);
    if (shot != -1) {
        t = &D_800BE8C0[i];
        u->damage = shot;
        condor_aim(u->pos.vx, u->pos.vy, t->pos.vx, t->pos.vy);
        u->vel.vx = D_8012C544;
        u->vel.vy = D_8012C548;
        u->dir = condor_square_angle(&u->vel);
        D_800BE8C0[shot].vel.vx = D_8012C544;
        D_800BE8C0[shot].vel.vy = D_8012C548;
        D_800BE8C0[shot].rot.vx = 0x400;
        D_800BE8C0[shot].rot.vy = u->dir;
        u->vel.vy = 0;
        u->vel.vx = 0;
        u->ty = 0;
        u->tx = 0;
        return 1;
    }
    return 0;
}

/* A shooter with a target: fires at it if it is in reach; 1 if it fired. */
short condor_shoot_target(Unit *u)
{
    Unit probe;
    short k;

    probe.pos.vx = u->pos.vx;
    probe.pos.vy = u->pos.vy;
    probe.pos.vz = u->pos.vz;
    probe.w = probe.h = u->reach;
    if (u - D_800BE8C0 < 20)
        condor_find_units_in_reach(&probe, 20, 40);
    else
        condor_find_units_in_reach(&probe, 0, 20);
    if (D_8012B85C) {
        for (k = 0; k < D_8012B85C; k++) {
            if (D_8012B75C[k].unit == &D_800BE8C0[u->target] && condor_fire_at(u, u->target) == 1) {
                u->pose = 2;
                u->act |= 0x80;
                u->wait = u->act_time;
                condor_start_script(u);
                return 1;
            }
        }
    }
    u->wait = u->rest_time;
    return 0;
}

/* A shooter without a target: fires at the first foe in reach; 1 if it fired. */
#ifdef NON_MATCHING
/* GCC hoists &D_800BE8C0 into s2 (one more saved register); the game recomputes it in the loop. */
short condor_shoot_any(Unit *u)
{
    Unit probe;
    short k;

    probe.pos.vx = u->pos.vx;
    probe.pos.vy = u->pos.vy;
    probe.pos.vz = u->pos.vz;
    probe.w = probe.h = u->reach;
    if (u - D_800BE8C0 < 20)
        condor_find_units_in_reach(&probe, 20, 40);
    else
        condor_find_units_in_reach(&probe, 0, 20);
    if (D_8012B85C) {
        for (k = 0; k < D_8012B85C; k++) {
            if (condor_fire_at(u, D_8012B75C[k].unit - D_800BE8C0) == 1) {
                u->pose = 2;
                u->act |= 0x80;
                u->wait = u->act_time;
                condor_start_script(u);
                return 1;
            }
        }
    }
    u->wait = u->rest_time;
    return 0;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_shoot_any);
#endif

/* A shooter's turn: recoil after a shot, chase or shoot its target, or look for one; 1 if it is busy shooting. */
short condor_update_shooter(Unit *u)
{
    short busy = 0;

    if (u->act & 0x80) {
        if (u->wait)
            u->wait--;
        else {
            u->act &= 0x7F;
            condor_face_target(u);
        }
        busy++;
    } else if (u->target != 0xFF) {
        if (u->wait)
            u->wait--;
        else if (condor_shoot_target(u))
            busy = 1;
        else {
            u->tx = D_800BE8C0[u->target].pos.vx;
            u->ty = D_800BE8C0[u->target].pos.vy;
            condor_face_target(u);
        }
    } else if (u->wait)
        u->wait--;
    else if ((u - D_800BE8C0 >= 20 || !u->mode) && condor_shoot_any(u) == 1)
        busy = 1;
    return busy;
}

/* Kind 2 (archers): shoot if able, else state, walk, bump. */
void condor_update_archer(Unit *u, int i)
{
    int self;

    if (condor_update_shooter(u))
        return;
    condor_set_pose(u);
    self = u - D_800BE8C0;
    if (u->tx | u->ty)
        condor_move_unit(u);
    if (condor_role_walks(u))
        condor_bump_foe(u, self);
}

/* A healer at work: heals its target while it stands next to it and is hurt. */
void condor_heal_target(Unit *u)
{
    Unit *p;
    short healed;
    short k;

    if (!(u->act & 4))
        return;
    if (!D_800BE8C0[u->target].flags) {
        u->act = 0;
        u->vel.vy = 0;
        u->vel.vx = 0;
        u->ty = 0;
        u->tx = 0;
        D_8012B85C = 0;
    }
    condor_find_touching_units(u, 0, 0x13);
    healed = 0;
    if (D_8012B85C) {
        for (k = 0; k < D_8012B85C; k++) {
            p = D_8012B75C[k].unit;
            if (p != &D_800BE8C0[u->target])
                continue;
            u->mode = 4;
            u->vel.vy = 0;
            u->vel.vx = 0;
            u->ty = 0;
            u->tx = 0;
            u->act &= 0xFE;
            if (p->hp < p->max_hp) {
                p->flags |= 0x40;
                u->act_wait = u->act_time * u->rate;
                healed++;
            } else
                u->act = 0;
        }
    }
    if (!healed && !(u->act & 1)) {
        u->act = 0;
        u->mode = 0;
        condor_start_script(u);
    }
}

/* Kind 5 (healers): heal, then state, walk, bump. */
void condor_update_healer(Unit *u, int i)
{
    int self;

    condor_heal_target(u);
    condor_set_pose(u);
    self = u - D_800BE8C0;
    if (u->tx | u->ty)
        condor_move_unit(u);
    if (condor_role_walks(u))
        condor_bump_foe(u, self);
}

/* Kind 6: while dying (mode 0xA) plays its last pose and leaves a blast behind; else state, walk, bump. */
void condor_update_bomber(Unit *u, int i)
{
    short blast;
    int self;

    if (u->mode == 0xA) {
        if (u->pose != 6) {
            u->pose = 6;
            condor_start_script(u);
            return;
        }
        if (u->hold != 0xFF)
            return;
        u->pose = 0;
        condor_start_script(u);
        u->mode = 0;
        blast = condor_fire_shot(u, 0xB);
        if (blast == -1)
            return;
        u->path = 0x80;
        D_800BE8C0[blast].rot.vx = 0x180;
        D_800BE8C0[blast].foe = u - D_800BE8C0;
        return;
    }
    condor_set_pose(u);
    self = u - D_800BE8C0;
    if (u->tx | u->ty)
        condor_move_unit(u);
    if (condor_role_walks(u))
        condor_bump_foe(u, self);
}

/* Fort Condor's entry (the game calls it to start the battle): runs rounds until the battle is decided, then writes
 * the outcome, the round and the gil back to the save. */
void condor_main(void)
{
    unsigned char *save = D_8009C6E4;
    int running;
    int ofs;

    D_8012C448 = 10;
    D_8012C404 = 1;
    D_8012C40C = D_8009D331;
    if (D_8012C40C >= 7)
        D_8012C40C = 0;
    D_8012C408 = 0;
    D_8012C418 = D_8009D33B;
    D_8012C538 = save_gil;
    condor_init_field(0xA0, 0x78, 1);
    condor_load_data();
    condor_init_battle();
    /* the battle song: CONDOR's sound file at 0x80188000 starts with its songs' offsets, each rounded up to a word */
    ofs = ((int *)0x80188000)[3];
    if (ofs & 3) {
        ofs &= ~3;
        ofs += 4;
    }
    snd_request.id = 0x10;
    snd_request.arg[0] = ofs + 0x80188000;
    snd_submit_request();
    running = 1;
    do {
        condor_end_round(condor_read_pad(&D_8012C67C, &D_8012C69C));
        if (D_8012C408 >= 4)
            running = 0;
    } while (running);
    DrawSync(0);
    SetDispMask(0);
    save[0xC4E] = D_8012C508;
    save[0xC53] = D_8012C50C;
    save[0xC4F] = D_8012C510 - D_8012C50C;
    save[0xC50] = D_8012C408 == 4;
    *(int *)(save + 0xB7C) = D_8012C538;
}

/* Ends a round of the battle by how it ended: 0 the result screen, 1 a retry, 2 a switch of songs, 3 the board
 * kept for later. */
#ifdef NON_MATCHING
/* The song request: the game writes the argument by snd_request.arg[0]'s own address while it keeps &snd_request in
 * a register. */
void condor_end_round(short how)
{
    short k;
    Kept *slot;
    Unit *p;

    switch (how) {
    case 0:
        condor_update_frame();
        break;
    case 1:
        DrawSync(0);
        snd_request.id = 0xF0;
        snd_submit_request();
        SetDispMask(0);
        while (func_8001C808())
            VSync(0);
        condor_init_battle();
        if (D_8012C404 == 1)
            condor_song_request(0) else condor_song_request(3);
        snd_submit_request();
        break;
    case 2:
        DrawSync(0);
        snd_request.id = 0xF0;
        snd_submit_request();
        SetDispMask(0);
        if (D_8012C404 == 1)
            D_8012C404 = 0;
        else
            D_8012C404 = 1;
        condor_init_battle();
        if (D_8012C404 == 1)
            condor_song_request(3) else condor_song_request(0);
        snd_submit_request();
        D_8012AF58 = 0xC;
        break;
    case 3:
        DrawSync(0);
        snd_request.id = 0xF0;
        snd_submit_request();
        SetDispMask(0);
        p = D_800BE8C0;
        slot = D_800BE814;
        for (k = 0; k < 20; k++, p++) {
            if (p->flags) {
                slot->x = p->pos.vx;
                slot->y = 0x400 - p->pos.vy;
                slot->kind = p->kind;
                slot->dir = p->dir;
                slot++;
            }
        }
        slot->x = -1;
        condor_song_request(0);
        snd_submit_request();
        condor_init_battle();
        condor_song_request(0);
        snd_submit_request();
        break;
    default:
        DrawSync(0);
        snd_request.id = 0xF0;
        snd_submit_request();
        break;
    }
    DrawSync(0);
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_end_round);
#endif

#ifdef NON_MATCHING
/* The stores of 0x100 to D_8012C704/D_8012C524: GCC moves them next to the D_8012C70C store, the game keeps 0x100
 * in a1; the register choice follows. */
void condor_init_battle(void)
{
    short i;
    unsigned short tpage, clut;
    int per;

    if (D_8012C408 == 3)
        D_8012C404 = 2;
    D_8012C5E0 = 0;
    D_8012C3E4 = 0;
    D_8012C3CC = 0;
    D_8012C5BC = 0;
    D_8012C5B8 = 0;
    D_8012C5B4 = 0;
    D_8012C5B0 = 0;
    D_8012C408 = 0;
    D_8012C414 = 0;
    D_8012C3F8 = 0;
    D_8012C514 = 0;
    D_8012C5C4 = 0;
    D_8012C5D4 = 2;
    D_8012C490 = 0;
    D_8012C464 = 0;
    D_8012C364 = 0;
    D_8012C484 = 0;
    D_8012C480 = 0;
    D_8012C47C = 0;
    D_8012C488 = 0;
    D_8012C5DC = 0;
    D_8012AF54 = 0;
    D_8012AF58 = -1;
    D_8012C454 = 0;
    D_8012C510 = 0;
    D_8012C50C = 0;
    D_8012C508 = 0;
    D_80129F38 = 0;
    D_8012C498 = 0;
    D_8012C52C = 0;
    D_8012C6DC.vx = 0;
    D_8012C6DC.vy = 0;
    D_8012C6DC.vz = 0;
    D_8012C6E4.vx = 0;
    per = D_8012C388 / D_8012C448;
    D_8012C6E4.vy = 0;
    D_8012C6E4.vz = 0;
    D_8012C6EC.vx = 0;
    D_8012C6EC.vy = 0;
    D_8012C6EC.vz = 0;
    D_8012C6FC = 0;
    D_8012C700 = 0;
    D_8012C704 = 0x100;
    D_8012C70E = 0;
    D_8012C710 = 0;
    D_8012C5D0 = 0;
    D_8012C5CC = 0;
    D_8012C394 = 0;
    D_8012C42C = 0;
    D_8012C4B4 = -1;
    D_8012C420 = -1;
    D_8012C41C = -1;
    D_8012C428 = 0;
    D_8012C400 = 0;
    D_8012C524 = 0x100;
    D_8012C528 = 0;
    D_8012C3FC = 0;
    D_8012C3DC = 0;
    D_8012C70C = 0x100;
    D_8012C4E8 = 10;
    D_8012C3E8 = 1;
    D_8012C4B8 = 0x60;
    D_8012C3EC = 4;
    D_8012C714 = per;
    D_8012C718 = per;
    D_8012C71C = per;
    D_8012C3D8 = 0;
    D_8012C5C0 = 0;
    D_8012C4EC = 0;
    D_8012C4C8 = 0;
    for (i = 0; i < 32; i++)
        D_8012BA68[i] = 1;

    func_80043814(&D_8011CBBC[0].draw, 0, 0, 0x140, 0x1E0);
    D_8011CBBC[0].draw.isbg = 1;
    D_8011CBBC[0].draw.r0 = 0;
    D_8011CBBC[0].draw.g0 = 0;
    D_8011CBBC[0].draw.b0 = 0;
    PutDrawEnv(&D_8011CBBC[0].draw);
    DrawSync(0);
    func_80043814(&D_8011CBBC[1].draw, 0, 0, 0x140, 0x1E0);
    D_8011CBBC[1].draw.isbg = 1;
    D_8011CBBC[1].draw.r0 = 0;
    D_8011CBBC[1].draw.g0 = 0;
    D_8011CBBC[1].draw.b0 = 0;
    PutDrawEnv(&D_8011CBBC[1].draw);
    DrawSync(0);

    /* the 480-line clear done, each buffer draws 220 lines of its own half and shows 240 */
    D_8011CBBC[0].draw.isbg = 0;
    D_8011CBBC[1].draw.isbg = 0;
    func_80043814(&D_8011CBBC[0].draw, 0, 8, 0x140, 0xDC);
    SetDefDispEnv(&D_8011CBBC[0].disp, 0, 0xF0, 0x140, 0xF0);
    func_80043814(&D_8011CBBC[1].draw, 0, 0xF8, 0x140, 0xDC);
    SetDefDispEnv(&D_8011CBBC[1].disp, 0, 0, 0x140, 0xF0);
    D_8011CBBC[0].disp.screen.x = 0;
    D_8011CBBC[0].disp.screen.y = 8;
    D_8011CBBC[0].disp.screen.w = 0x100;
    D_8011CBBC[0].disp.screen.h = 0xD8;
    D_8011CBBC[1].disp.screen.x = 0;
    D_8011CBBC[1].disp.screen.y = 8;
    D_8011CBBC[1].disp.screen.w = 0x100;
    D_8011CBBC[1].disp.screen.h = 0xD8;
    tpage = GetTPage(0, 0, 0x3C0, 0);
    clut = GetClut(0x3C0, 0xF0);
    D_80129F34 = &D_8011CBBC[1];
    condor_init_prims(&D_8011CBBC[0], tpage, clut, 0);
    condor_init_prims(&D_8011CBBC[1], tpage, clut, 1);
    condor_place_player_units();
    condor_init_blocks();
    condor_spawn_fort();
    if (D_80129F34 == &D_8011CBBC[0]) {
        D_80129F34 = &D_8011CBBC[1];
        D_8012C3F0 = 1;
    } else {
        D_80129F34 = &D_8011CBBC[0];
        D_8012C3F0 = 0;
    }
    func_80044244(D_80129F34->ot, 0x1000);
    SetDispMask(1);
    if (D_8012C404 == 1)
        D_8012AF58 = 12;
    else
        D_8012AF58 = 1;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_init_battle);
#endif

void condor_read_done(void)
{
}

void condor_stop_sounds(void)
{
    snd_request.id = 0x28;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0;
    snd_submit_request();
    snd_request.id = 0x29;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0;
    snd_submit_request();
    snd_request.id = 0x2A;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0;
    snd_submit_request();
    snd_request.id = 0x2B;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0;
    snd_submit_request();
    D_8012C41C = -1;
    D_8012C42C = 1;
    D_8012C5D0 = 0;
    D_8012C4B8 = -0x20;
    if (D_8011CBBC[D_8012C3F0].map[0].r0 > 0x50)
        D_8012C488 = 1;
    else
        D_8012C488 = 3;
    D_8012C47C = 2;
    D_8012C484 = 1;
    D_8012C480 = 1;
}

void condor_play_end_music(void)
{
    int off, off2;
    char *base;

    condor_stop_sounds();
    D_8012AF58 = 7;
    if (D_8012C40C == 6) {
        base = (char *)0x80188000;
        off = CONDOR_SONGS[2];
        if (off & 3) {
            off &= ~3;
            off += 4;
        }
        snd_request.id = 0x10;
        snd_request.arg[0] = (long)(base + off);
    } else {
        off2 = CONDOR_SONGS[4];
        if (off2 & 3) {
            off2 &= ~3;
            off2 += 4;
        }
        snd_request.id = 0x10;
        snd_request.arg[0] = off2 + 0x80188000;
    }
    snd_submit_request();
    D_8012C3EC = 11;
}

void condor_flip_buffers(void)
{
    if (D_8012AF54) {
        D_8012AF3C.x = 0x3C0;
        D_8012AF3C.y = 0xB0;
        D_8012AF3C.w = 0x40;
        D_8012AF54 = 0;
        D_8012AF3C.h = 8;
        func_80044000(&D_8012AF3C, D_8012AF5C);
    }
    if (D_8012AF58 != -1) {
        D_8012AF3C.x = 0x3C0;
        D_8012AF3C.y = 0xE0;
        D_8012AF3C.w = 0x40;
        D_8012AF3C.h = 0x10;
        if (D_8012AF58 < 16)
            func_80044000(&D_8012AF3C, (void *)((D_8012AF58 << 11) + 0x80190040));
        else
            func_80044000(&D_8012AF3C, (void *)((D_8012AF58 & 0xF) << 11 | 0x801A0040));
        D_8012AF58 = -1;
    }
    D_8012C3D0 = VSync(1);
    DrawSync(0);
    D_8012C3D4 = VSync(0);
    PutDrawEnv(D_8012C3CC & 1 ? &D_8011CBBC[0].draw : &D_8011CBBC[1].draw);
    func_800444AC(D_8012C3CC & 1 ? &D_8011CBBC[0].disp : &D_8011CBBC[1].disp);
    if (D_8012C400 == 2 || D_8012C3EC == 14)
        ClearImage(D_8012C3CC & 1 ? &D_8011CBBC[0].draw.clip : &D_8011CBBC[1].draw.clip, 0, 0, 0x30);
    func_8004433C(&D_80129F34->ot[0xFFF]);
    D_8012C3CC++;
    D_8012C5BC++;
    if (D_8012C400)
        condor_update_chosen_cell();
    if (D_80129F34 == &D_8011CBBC[0]) {
        D_80129F34 = &D_8011CBBC[1];
        D_8012C3F0 = 1;
    } else {
        D_80129F34 = &D_8011CBBC[0];
        D_8012C3F0 = 0;
    }
    func_80044244(D_80129F34->ot, 0x1000);
    condor_update_colours();
}

void condor_update_frame(void)
{
    int i;
    Unit *u;
    char *base;
    int off;

    condor_flip_buffers();
    switch (D_8012C3EC) {
    case 2:
    case 3:
    case 7:
    case 8:
    case 16:
        break;
    default:
        if (!(D_8012C42C | D_8012C3EC) && D_8012C3E8 < 2) {
            D_8012C5B8++;
            if (D_8012C5B8 >= 3600) {
                D_8012C5B8 = 0;
                D_8012C5B4++;
            }
            condor_spawn_enemy();
            if (D_8012C464) {
                D_8012C464 = 0;
                if (!D_8012C3EC && !D_8012C5CC) {
                    if (D_8012C508 < 21) {
                        snd_request.id = 0x29;
                        snd_request.arg[0] = 0x40;
                        snd_request.arg[1] = 0;
                        snd_submit_request();
                        if ((short)condor_open_shop() == 1) {
                            D_8012C47C = 1;
                            D_8012C484 = 1;
                            D_8012C480 = 1;
                            D_8012C42C = 0;
                        }
                    } else {
                        snd_request.id = 0x2B;
                        snd_request.arg[0] = 0x40;
                        snd_request.arg[1] = 3;
                        snd_submit_request();
                    }
                }
            }
        }
        if (D_8012C3EC != 9 && !D_8012C5CC) {
            if (D_8012C404 != 1) {
                condor_update_troops();
                condor_update_shots();
            } else {
                for (i = 0, u = D_800BE8C0; i < 40; i++, u++) {
                    if (u->flags)
                        condor_run_anim_script(u);
                }
            }
        }
    }
    if (!D_8012C3EC) {
        if (!D_8012C5CC && D_8012C404 != 1) {
            if (!D_8012C50C && !D_8012C414)
                D_8012C414 = 1;
            if (!D_8012C508 && !D_8012C414 && D_8012C538 < 400)
                D_8012C414 = 2;
        }
        if (!D_8012C3EC && !D_8012C5CC) {
            if (D_8012C414 == 1) {
                condor_stop_sounds();
                base = (char *)0x80188000;
                off = CONDOR_SONGS[1];
                D_8012AF58 = 2;
                if (off & 3) {
                    off &= ~3;
                    off += 4;
                }
                snd_request.id = 0x10;
                snd_request.arg[0] = (long)(base + off);
                snd_submit_request();
                D_8012C3EC = 12;
            }
            if (D_8012C414 == 2)
                condor_play_end_music();
        }
    }
    condor_draw_frame();
}

void condor_draw_unit_panel(short x, short y)
{
    unsigned short lit;

    lit = 0xFFFF - D_800BE8C0[D_8012C41C].act;
    condor_draw_block_to(2, x, y + 2);
    condor_draw_block_to(3, x + 10, y + 0x15);
    condor_set_block_clut(22, 0x380, 0xF0 | (lit & 1));
    condor_draw_block_to(22, x, y + 0x20);
    lit >>= 1;
    condor_set_block_clut(23, 0x380, 0xF0 | (lit & 1));
    condor_draw_block_to(23, x + 0x10, y + 0x20);
    condor_set_block_clut(24, 0x380, 0xF0 | (lit >> 1 & 1));
    condor_draw_block_to(24, x + 0x20, y + 0x20);
    condor_set_block_clut(4, 0x380, (unsigned short)(D_8012C51C + 2) + 0xF0);
    condor_draw_block_to(4, x, y);
}

/* Grid 6 and condor_draw_unit_panel's panel, then D_8012C56C (0-3) rows: a picture picked by D_8012C570/574/578 and a
 * mark. */
int condor_draw_result_rows(void)
{
    if (!D_8012C56C) {
        condor_draw_unit_panel(-0x28, 0x10);
        return 0;
    }
    condor_draw_block_to(6, D_8012C4F0 + 0x38, D_8012C4F4 + 0x10);
    condor_draw_unit_panel(-0x28, 0x10);
    condor_set_block_uvs(11, -0x40, D_8012C570 * 16);
    condor_draw_block_to(11, 0x40, 0x10);
    condor_set_block_clut(14, 0x380, (unsigned short)(D_8012C51C + 2) + 0xF0);
    condor_draw_block_to(14, 0x38, 0x10);
    if (D_8012C56C == 1)
        return 0;
    condor_set_block_uvs(12, -0x40, D_8012C574 * 16);
    condor_draw_block_to(12, 0x40, 0x20);
    condor_set_block_clut(15, 0x380, (unsigned short)(D_8012C51C + 2) + 0xF0);
    condor_draw_block_to(15, 0x38, 0x20);
    if (D_8012C56C == 2)
        return 0;
    condor_set_block_uvs(13, -0x40, D_8012C578 * 16);
    condor_draw_block_to(13, 0x40, 0x30);
    condor_set_block_clut(16, 0x380, (unsigned short)(D_8012C51C + 2) + 0xF0);
    condor_draw_block_to(16, 0x38, 0x30);
    return 0;
}

#ifdef NON_MATCHING
/* Only the D_8012C3EC == 9 block: the game keeps &block 44's x in a2 and the camera in t4/t3; everything after it
 * is one instruction later. */
void condor_draw_frame(void)
{
    SpriteBlock *g;
    Blast *m;
    POLY_FT4 *p;
    short dx;
    unsigned int *ot;
    SPRT_16 *s;
    unsigned short cx, cy, ox;
    int oy;
    int i, j;
    short k;

    D_800BE8C0[69].scene.vx = 0x400;
    D_800BE8C0[69].pos.vx = 0x400;
    D_800BE8C0[69].rot.vx = 0;
    D_800BE8C0[69].rot.vy = 0;
    D_800BE8C0[69].rot.vz = 0;
    D_800BE8C0[68].scene.vx = 0x400;
    D_800BE8C0[68].pos.vx = 0x400;
    D_800BE8C0[68].rot.vx = 0;
    D_800BE8C0[68].rot.vy = 0;
    D_800BE8C0[68].rot.vz = 0;
    D_800BE8C0[67].scene.vx = 0x400;
    D_800BE8C0[67].pos.vx = 0x400;
    D_800BE8C0[67].rot.vx = 0;
    D_800BE8C0[67].rot.vy = 0;
    D_800BE8C0[67].rot.vz = 0;
    if (D_8012C3F0)
        D_8012C5C8 = 8;
    else
        D_8012C5C8 = 0xF8;

    /* the clamps are overridden: the point ends up pinned near the screen's top right */
    D_8012C3BC[0] = D_8012C3A4[0];
    D_8012C3BC[1] = D_8012C3A4[1];
    if (D_8012C3BC[0] - D_8012C434 < 0x28)
        D_8012C3BC[0] = D_8012C434 + 0x28;
    if (D_8012C3BC[0] - D_8012C434 > 0xC8)
        D_8012C3BC[0] = D_8012C434 + 0xC8;
    if ((short)D_8012C3A4[1] - D_8012C438 >= 0xA0)
        D_8012C3BC[1] = D_8012C3A4[1] - 0x70;
    D_8012C3BC[0] = D_8012C434 + 0xC6;
    D_8012C3BC[1] = D_8012C438 - 0xE;

    if (D_8012C514 >= 16)
        D_8012C514 = -1;
    D_8012C514++;
    if (D_8012C5C4) {
        D_8012C5C4--;
        condor_draw_block_to(10, 0xFFFF, 0);
    }

    switch (D_8012C3EC) {
    case 11:
        if (D_8012C4B8 == 0x40)
            condor_draw_flip_pair(0x4F);
        condor_draw_banner(D_8012C4B8);
        break;
    case 4:
        if (D_8012C404 != 1)
            condor_draw_banner(D_8012C4B8);
        break;
    case 12:
        condor_draw_banner(D_8012C4B8);
        break;
    }

    if (D_8012C3EC == 9) {
        if (D_8012C5BC & 0x20) {
            g = &D_8010BB94[44];
            cx = D_8012C434;
            cy = D_8012C438;
            ot = D_80129F34->ot;
            D_8010BB94[44].x = cx + 0x88;
            D_8010BB94[44].y = cy + 0x68;
            if (D_8010BB94[44].move) {
                D_8010BB94[44].x += D_8010BB94[44].move[0];
                D_8010BB94[44].y += D_8010BB94[44].move[1];
            }
            g = &D_8010BB94[44];
            s = &D_80129F34->sprt[g->first];
            for (j = 0; j < g->h * 16; j += 16) {
                for (i = 0; i < g->w * 16; i += 16) {
                    s->x0 = g->x + i - cx;
                    s->y0 = g->y + j - cy;
                    addPrim(ot, s);
                    s++;
                }
            }
            AddPrim(ot, &g->mode);
        }
    } else {
        condor_set_block_uvs(1, 0x20, 0x30);
        switch (D_8012C3E8) {
        case 0:
            break;
        case 1:
            if (D_8012C414 || D_8012C3EC == 10 || D_8012C3EC == 14)
                break;
            if (D_8012C41C != -1) {
                if (!D_800BE8C0[D_8012C41C].flags || D_800BE8C0[D_8012C41C].fade) {
                    D_8012C41C = -1;
                    break;
                }
                if (D_8012C5CC)
                    break;
                if (D_8012C41C >= 20)
                    D_8012C51C = 1;
                else
                    D_8012C51C = 0;
                condor_draw_unit_panel(-0x28, 0x10);
                condor_place_fort();
                break;
            }
            if (D_8012C3EC == 5 || D_8012C3EC == 6) {
                condor_place_fort();
                break;
            }
            if (!D_8012C5C0) {
                D_8010BB94[5].move = (short *)D_8012C3A4;
                condor_draw_block_to(5, -4, -0x20);
            }
            condor_draw_block_to(1, -4, -0x20);
            break;
        case 2:
            if (D_8012C414)
                break;
            if (D_8012C41C >= 20)
                D_8012C51C = 1;
            else
                D_8012C51C = 0;
            condor_place_fort();
            condor_draw_result_rows();
            break;
        case 3:
            if (D_8012C3EC != 15) {
                if (!D_8012C5C0) {
                    D_8010BB94[5].move = (short *)D_8012C3AC;
                    condor_draw_block_to(5, -4, -0x20);
                }
                condor_set_block_uvs(9, -0x70, D_8012C57C * 16);
                condor_draw_block_to(9, 0xE, -0x20);
                condor_draw_block_to(46, -4, -0x20);
                break;
            }
            if (D_8012C41C == -1)
                break;
            if (D_8012C41C >= 20)
                D_8012C51C = 1;
            else
                D_8012C51C = 0;
            condor_draw_unit_panel(-0x28, 0x10);
            condor_place_fort();
            break;
        }
    }

    condor_draw_hud();
    condor_update_popups();
    condor_draw_fade_line();
    D_8012C444 = D_8012C438;
    D_8012C440 = D_8012C434;
    switch (D_8012C400) {
    case 0:
    case 1:
        condor_draw_background(D_80129F34->ot);
        break;
    case 3:
        condor_draw_background(D_80129F34->ot);
        /* fall through */
    case 2:
        condor_draw_ground(&D_80129F34->ot[0xFFE], D_8012C3F0);
        break;
    }
    ox = -0x60;
    oy = 0x188;

    /* each layer of the ordering table is drawn with its own draw offset */
    dx = -(D_8012C440 + ox);
    D_80129F34->offs[4].ofs[0] = dx;
    D_80129F34->offs[4].ofs[1] = oy - D_8012C444 + D_8012C5C8;
    condor_put_offset(&D_80129F34->offs[4], &D_80129F34->ot[2]);
    condor_draw_units(D_80129F34->ot, D_8012C3F0);
    D_8012C540 = 0;
    condor_draw_fort_screen(D_80129F34->ot, D_8012C3F0);

    D_80129F34->offs[0].ofs[0] = 0;
    D_80129F34->offs[0].ofs[1] = D_8012C5C8;
    condor_put_offset(&D_80129F34->offs[0], &D_80129F34->ot[0xFFF]);

    dx = -(D_8012C440 + ox);
    D_80129F34->offs[1].ofs[0] = dx;
    D_80129F34->offs[1].ofs[1] = oy - D_8012C444 + D_8012C5C8;
    condor_put_offset(&D_80129F34->offs[1], &D_80129F34->ot[0xFFE]);

    dx = -(D_8012C440 + ox);
    D_80129F34->offs[2].ofs[0] = dx;
    D_80129F34->offs[2].ofs[1] = oy - D_8012C444 + D_8012C5C8;
    condor_put_offset(&D_80129F34->offs[2], &D_80129F34->ot[2]);

    D_80129F34->offs[3].ofs[0] = 0;
    D_80129F34->offs[3].ofs[1] = D_8012C5C8;
    condor_put_offset(&D_80129F34->offs[3], &D_80129F34->ot[0]);

    D_80129F34->offs[5].ofs[0] = 0;
    D_80129F34->offs[5].ofs[1] = D_8012C5C8 - 8;
    condor_put_offset(&D_80129F34->offs[5], &D_80129F34->ot[0x20 + D_8012C540]);

    p = D_80129F34->blast;
    m = D_8011CB3C;
    for (k = 0; k < 8; m++, k++, p++) {
        if (m->step == 0xFF)
            continue;
        if (m->type == 7)
            condor_draw_burst(p, m);
        else
            condor_update_blast(p, m);
    }
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_draw_frame);
#endif

#ifdef NON_MATCHING
/* Only the prologue's load order: the game loads D_8012C434 first and D_8012C438/D_8012C4B8 last. */
void condor_draw_banner(short mode)
{
    unsigned short top;

    D_8012C4DC = mode;
    D_8010BB94[17].x = D_8012C434 + 0x20;
    D_8010BB94[18].x = D_8012C434 + 0x20;
    D_8010BB94[18].w = 16;
    D_8010BB94[17].w = 16;
    D_8010BB94[17].h = 2;
    D_8010BB94[18].h = 1;
    top = D_8012C438 + D_8012C4B8;
    D_8010BB94[17].y = top - 8;
    D_8010BB94[18].y = top + 6;
    condor_set_block_uvs(17, 0, -0x78);
    condor_set_block_uvs(18, 0, -0x20);
    condor_draw_block_to(18, 0xFFFF, 0);
    condor_draw_block_to(17, 0xFFFF, 0);
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_draw_banner);
#endif

void condor_draw_block_at_y(short i, short y)
{
    D_8010BB94[i].y = D_8012C438 + y;
    condor_draw_block(i);
}

void condor_draw_block_at(short i, short x, short y)
{
    D_8010BB94[i].x = D_8012C434 + x;
    D_8010BB94[i].y = D_8012C438 + y;
    condor_draw_block(i);
}

/* The two halves of a picture that flips with bit 4 of D_8012C5BC, placed side by side at height y. */
void condor_draw_flip_pair(int y)
{
    if (D_8012C5BC >> 4 & 1)
        condor_set_block_uv(21, -16, -0x58);
    else
        condor_set_block_uv(21, -16, -0x48);
    if (D_8012C5BC >> 4 & 1)
        condor_set_block_uv(51, -32, -0x48);
    else
        condor_set_block_uv(51, -32, -0x58);
    D_8010BB94[21].x = D_8012C434 + 0x88;
    D_8010BB94[21].y = y + D_8012C438;
    condor_draw_block(21);
    D_8010BB94[51].x = D_8012C434 + 0x98;
    D_8010BB94[51].y = y + D_8012C438;
    condor_draw_block(51);
}

#ifdef NON_MATCHING
/* The plain inlined block draws get the block's address as base + i * 28 where the game loads the constant; some
 * scheduling (the fade stores before the unit field load, the w/h stores of blocks 17 and 18, block 36's uv loop). */
void condor_draw_hud(void)
{
    short h, rest;

    if (D_8012C404 != 1 && D_8012C3EC != 14) {
        if (D_8012C52C >= 0x58 && !D_8012C414 && D_8012C514 & 8) {
            if (!(D_8012C514 & 7)) {
                snd_request.id = 0x28;
                snd_request.arg[0] = 0x40;
                snd_request.arg[1] = 0xDE;
                snd_submit_request();
            }
            D_8010BB94[30].x = D_8012C434 + 0xC;
            D_8010BB94[30].y = D_8012C438 + 0x53;
            condor_draw_block(30);
        }
        D_8010BB94[27].x = D_8012C434 + 0xC;
        D_8010BB94[27].y = D_8012C438 + 0x50;
        condor_draw_block(27);
        D_8010BB94[28].x = D_8012C434 + 0x10;
        D_8010BB94[28].y = D_8012C438 + 0x42;
        condor_draw_block(28);
        D_8010BB94[29].x = D_8012C434 + 0x10;
        D_8010BB94[29].y = D_8012C438 + 0xB5;
        condor_draw_block(29);

        /* A gauge of D_8012C52C pixels: blocks 31-36 stacked, 16 a block, the top one showing only what is left. */
        D_8010BB94[31].x = D_8012C434 + 0xC;
        D_8010BB94[32].x = D_8010BB94[31].x;
        D_8010BB94[33].x = D_8010BB94[31].x;
        D_8010BB94[34].x = D_8010BB94[31].x;
        D_8010BB94[35].x = D_8010BB94[31].x;
        D_8010BB94[36].x = D_8010BB94[31].x;
        h = D_8012C52C;
        if ((short)D_8012C52C >= 16) {
            condor_set_block_uv(31, 16, 0x20);
            condor_draw_block_at_y(31, 0xA3);
            condor_set_block_uv(36, 16, 0x10);
            condor_draw_block_at_y(36, 0xB3 - h);
        } else {
            if ((short)D_8012C52C >= 2) {
                condor_set_block_uv(31, 16, 0x2F);
                condor_draw_block_at_y(31, 0xB2);
            }
            condor_set_block_uv(36, 16, h);
            condor_draw_block_at_y(36, 0xA3);
        }
        h -= 16;
        if (h >= 16)
            condor_draw_block_at_y(32, 0xB3 - h);
        rest = h - 16;
        if (rest >= 16)
            condor_draw_block_at_y(33, 0xB3 - rest);
        rest = h - 32;
        if (rest >= 16)
            condor_draw_block_at_y(34, 0xB3 - rest);
        rest = h - 48;
        if (rest >= 16)
            condor_draw_block_at_y(35, 0xB3 - rest);

        if (D_8012C5D4 >= 2)
            condor_set_block_clut(53, 0x380, 0xF0);
        else
            condor_set_block_clut(53, 0x380, 0xF2);
        if (D_8012C5D4 >= 3)
            condor_set_block_clut(54, 0x380, 0xF0);
        else
            condor_set_block_clut(54, 0x380, 0xF2);
        if (D_8012C5D4 >= 4)
            condor_set_block_clut(55, 0x380, 0xF0);
        else
            condor_set_block_clut(55, 0x380, 0xF2);
        condor_draw_block_at(56, 0x118, 0x88);
        condor_draw_block_at(52, 0x120, 0xB0);
        condor_draw_block_at(53, 0x120, 0xA8);
        condor_draw_block_at(54, 0x120, 0xA0);
        condor_draw_block_at(55, 0x120, 0x98);
        condor_draw_block_at(57, 0x118, 0xB8);
    }

    if (D_8012C5CC) {
        if (!D_8012C42C) {
            D_8012C42C++;
            if (D_8011CBBC[D_8012C3F0].map[0].r0 > 0x50)
                D_8012C488 = 1;
            else
                D_8012C488 = 3;
            D_8012C47C = 2;
            D_8012C484 = 1;
            D_8012C480 = 1;
        }
        if (D_8012C3EC != 0x11)
            condor_draw_flip_pair(0x3F);
        D_8010BB94[19].x = D_8012C434 + 0x20;
        D_8010BB94[19].y = D_8012C438 + D_8012C4B8 - 0x18;
        D_8010BB94[20].x = D_8010BB94[19].x;
        D_8010BB94[20].y = D_8012C438 + D_8012C4B8 - 0x15;
        condor_draw_block(20);
        condor_draw_block(19);
        condor_draw_banner(0x30);
        if (++D_8012C5D0 >= 0x1E0) {
            D_8012C47C = 1;
            D_8012C484 = 1;
            D_8012C480 = 1;
            D_8012C5CC = 0;
            D_8012C42C = 0;
            condor_load_unit_card((short)D_800BE8C0[D_8012C41C].kind);
        }
    }

    if (D_8012C3EC == 7) {
        D_8010BB94[17].x = D_8012C434 + 0x68;
        D_8010BB94[17].y = D_8012C438 + 0x10;
        D_8010BB94[18].x = D_8012C434 + 0x88;
        D_8010BB94[18].y = D_8012C438 + 0x15;
        D_8010BB94[17].w = 7;
        D_8010BB94[18].w = 5;
        D_8010BB94[17].h = 2;
        D_8010BB94[18].h = 2;
        condor_set_block_uv(17, 0, -0x38);
        condor_set_block_uv(18, -0x80, -0x40);
        condor_draw_block(18);
        condor_draw_block(17);
        D_8010BB94[20].x = D_8012C434 + 0x40;
        D_8010BB94[20].y = D_8012C438 + 0x8D;
        condor_draw_block(20);
        D_8010BB94[39].x = D_8012C434 - D_8012C4AC + 0x84;
        D_8010BB94[39].y = D_8012C438 + 0x9F;
        condor_draw_block(39);
        D_8010BB94[40].x = D_8012C434 - D_8012C4B0 + 0x88;
        D_8010BB94[40].y = D_8012C438 + 0xAC;
        condor_draw_block(40);
        D_8010BB94[41].x = D_8012C434 + 0x84;
        D_8010BB94[41].y = D_8012C438 + 0xB9;
        condor_draw_block(41);
        D_8010BB94[48].x = D_8012C434 + 0xA6;
        D_8010BB94[48].y = D_8012C438 + 0xAA;
        condor_draw_block(48);
        D_8010BB94[37].x = D_8012C434 + 0x40;
        D_8010BB94[37].y = D_8012C438 + 0x88;
        condor_draw_block(37);
        D_8010BB94[38].x = D_8012C434 + 0x40;
        D_8010BB94[38].y = D_8012C438 + 0xA8;
        condor_draw_block(38);
        /* Frames 0, 1, 2, 1 of a four-step cycle. */
        h = D_8012C5BC >> 3 & 3;
        if (h == 3)
            h = 1;
        condor_set_block_uv(58, 0x60, h << 5 | 0x10);
        D_8010BB94[58].x = D_8012C434 + 0x90;
        D_8010BB94[58].y = D_8012C438 + 0x64;
        condor_draw_block(58);
        return;
    }
    if (D_8012C404 != 1)
        return;
    if (D_8012C3EC == 10) {
        D_8010BB94[6].move = 0;
        condor_draw_block(6);
        D_8010BB94[6].move = D_8012C3BC;
        D_8010BB94[42].x = D_8012C434 + 0x38;
        D_8010BB94[42].y = D_8012C438 + 0x4B;
        D_8010BB94[43].x = D_8012C434 + 0x40;
        D_8010BB94[43].y = D_8012C438 + 0x59;
        condor_draw_block(42);
        condor_draw_block(43);
        D_8010BB94[49].move = 0;
        D_8010BB94[49].x = D_8012C434 + 0x1C;
        D_8010BB94[49].y = D_8012C438 + 0x40;
        condor_draw_block(49);
        condor_draw_banner(0x30);
        D_8012C4B8 = D_8012C4DC;
        return;
    }
    condor_draw_flip_pair(0xC0);
    if (D_8012C464 && D_8012C3EC == 7)
        return;
    condor_draw_banner(0xB0);
    D_8012C4B8 = D_8012C4DC;
}
#else
INCLUDE_ASM("asm/overlay/condor/nonmatchings/condor", condor_draw_hud);
#endif

void condor_init_blocks(void)
{
    int i;

    D_8012C3A4[0] = D_8012C434 + 0x98;
    D_8012C3A4[1] = D_8012C438 + 0x60;
    condor_init_block(1, 2, 2, 0, 0x30, 0, 0, 0x3c0, 0, 0x3c0, 0xf6, D_8012C3A4);
    condor_init_block(2, 6, 1, 0xa0, 0, 0, 0, 0x380, 0, 0x380, 0xf7, D_8012C3BC);
    condor_init_block(3, 6, 1, 0, 0xd0, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, D_8012C3BC);
    condor_init_block(4, 6, 3, 0, 0, 0, 1, 0x380, 0, 0x380, 0xf2, D_8012C3BC);
    condor_init_block(5, 2, 2, 0x40, 0x30, 0, 0, 0x3c0, 0, 0x3c0, 0xf6, D_8012C3A4);
    condor_init_block(6, 1, 1, 0xd0, 0x78, 0, 0, 0x380, 0, 0x380, 0xf0, D_8012C3BC);
    condor_init_block(7, 1, 1, 0, 0x30, 0, 0, 0x3c0, 0, 0x3c0, 0xf6, D_8012C3AC);
    condor_init_block(9, 2, 1, 0xa0, 8, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, D_8012C3AC);
    condor_init_block(0xa, 1, 2, 0x20, 0x50, 0, 0, 0x3c0, 0, 0x3c0, 0xf6, D_8012C3AC);
    condor_init_block(0xb, 3, 1, 0, 0, 0, 0, 0x3c0, 0, 0x3c0, 0xf7, D_8012C3BC);
    condor_init_block(0xc, 3, 1, 0, 0, 0, 0, 0x3c0, 0, 0x3c0, 0xf7, D_8012C3BC);
    condor_init_block(0xd, 3, 1, 0, 0, 0, 0, 0x3c0, 0, 0x3c0, 0xf7, D_8012C3BC);
    condor_init_block(0xe, 4, 1, 0x60, 0, 0, 1, 0x380, 0, 0x380, 0xf2, D_8012C3BC);
    condor_init_block(0xf, 4, 1, 0x60, 0, 0, 1, 0x380, 0, 0x380, 0xf2, D_8012C3BC);
    condor_init_block(0x10, 4, 1, 0x60, 0, 0, 1, 0x380, 0, 0x380, 0xf2, D_8012C3BC);
    condor_init_block(0x11, 0x10, 2, 0, 0x88, 0, 1, 0x380, 0, 0x380, 0xf2, 0);
    condor_init_block(0x12, 0x10, 1, 0, 0xe0, 0, 0, 0x3c0, 0, 0x3c0, 0xf7, 0);
    condor_init_block(0x13, 8, 2, 0, 0x70, 0, 1, 0x380, 0, 0x380, 0xf2, 0);
    condor_init_block(0x14, 6, 1, 0xa0, 0, 0, 0, 0x380, 0, 0x380, 0xf7, 0);
    condor_init_block(0x15, 1, 1, 0xf0, 0x70, 0, 0, 0x380, 0, 0x380, 0xf5, 0);
    condor_init_block(0x16, 1, 1, 0x80, 0x78, 0, 0, 0x380, 0, 0x380, 0xf0, D_8012C3BC);
    condor_init_block(0x17, 1, 1, 0x90, 0x78, 0, 0, 0x380, 0, 0x380, 0xf0, D_8012C3BC);
    condor_init_block(0x18, 1, 1, 0xa0, 0x78, 0, 0, 0x380, 0, 0x380, 0xf0, D_8012C3BC);
    condor_init_block(0x19, 1, 1, 0xb0, 0x78, 0, 0, 0x380, 0, 0x380, 0xf0, D_8012C3BC);
    condor_init_block(0x1a, 1, 1, 0xc0, 0x78, 0, 0, 0x380, 0, 0x380, 0xf0, D_8012C3BC);
    condor_init_block(0x1b, 1, 7, 0, 0x10, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x1c, 2, 1, 0x20, 0x10, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x1d, 2, 1, 0x20, 0x20, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x1e, 3, 1, 0x20, 0, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x1f, 1, 1, 0x10, 0x20, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x20, 1, 1, 0x10, 0x18, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x21, 1, 1, 0x10, 0x18, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x22, 1, 1, 0x10, 0x18, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x23, 1, 1, 0x10, 0x18, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x24, 1, 1, 0x10, 0x10, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x25, 6, 2, 0, 0xa8, 0, 1, 0x380, 0, 0x380, 0xf2, 0);
    condor_init_block(0x26, 6, 2, 0x60, 0xa8, 0, 1, 0x380, 0, 0x380, 0xf2, 0);
    condor_init_block(0x27, 2, 1, 0x60, 0xc0, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x28, 2, 1, 0x60, 0xd0, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x29, 2, 1, 0x60, 0xe0, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x2a, 3, 1, 0xa0, 0, 0, 0, 0x380, 0, 0x380, 0xf7, 0);
    condor_init_block(0x2b, 3, 1, 0xd0, 0, 0, 0, 0x380, 0, 0x380, 0xf7, 0);
    condor_init_block(0x2c, 3, 1, 0, 0x80, 0, 0, 0x3c0, 0, 0x3c0, 0xf7, 0);
    condor_init_block(0x2d, 4, 2, 0, 0x90, 0, 1, 0x3c0, 0, 0x3c0, 0x1f2, 0);
    condor_init_block(0x2e, 2, 2, 0x20, 0x50, 0, 0, 0x3c0, 0, 0x3c0, 0xf6, D_8012C3AC);
    condor_init_block(0x2f, 6, 4, 0, 0x30, 0, 1, 0x380, 0, 0x380, 0xf2, 0);
    condor_init_block(0x30, 5, 2, 0, 0xc0, 0, 0, 0x3c0, 0, 0x3c0, 0xf0, 0);
    condor_init_block(0x31, 6, 3, 0x60, 0x40, 0, 1, 0x380, 0, 0x380, 0xf2, 0);
    condor_init_block(0x32, 1, 1, 0, 0xe0, 0, 0, 0x380, 0, 0x380, 0xf0, 0);
    condor_init_block(0x33, 1, 1, 0xe0, 0x70, 0, 0, 0x380, 0, 0x380, 0xf5, 0);
    condor_init_block(0x34, 1, 1, 0xf0, 0x78, 0, 0, 0x380, 0, 0x380, 0xf0, 0);
    condor_init_block(0x35, 1, 1, 0xf0, 0x78, 0, 0, 0x380, 0, 0x380, 0xf0, 0);
    condor_init_block(0x36, 1, 1, 0xf0, 0x78, 0, 0, 0x380, 0, 0x380, 0xf0, 0);
    condor_init_block(0x37, 1, 1, 0xf0, 0x78, 0, 0, 0x380, 0, 0x380, 0xf0, 0);
    condor_init_block(0x38, 2, 1, 0xe0, 0x50, 0, 0, 0x380, 0, 0x380, 0xf0, 0);
    condor_init_block(0x39, 2, 1, 0xe0, 0x60, 0, 0, 0x380, 0, 0x380, 0xf0, 0);
    condor_init_block(0x3a, 2, 2, 0x60, 0x10, 0, 0, 0x3c0, 0, 0x3c0, 0xf6, 0);
    for (i = 0; i < 16; i++)
        condor_init_block(i + 0x3B, 1, 1, i * 16, 0xB0, 0, 0, 0x3C0, 0, 0x3C0, 0xF0, 0);
}

void condor_update_chosen_cell(void)
{
    if (D_8012C3E8 > 0 && D_8012C41C != -1)
        D_8012C3FC = D_800BE8C0[D_8012C41C].cell;
    else
        D_8012C3FC = -1;
}
