/* MINI/SNOBO2.BIN, the Gold Saucer snowboard: 0x800A0448-0x800B8260 (its data follows to 0x800E18F4, then bss). A
 * reworked copy of SNOBO.BIN (snobo.c): the course, rider and object code are SNOBO's adjusted to new offsets, around
 * the Gold Saucer's own course choice, ghosts, scores and results screens. One object: GCC 2.6.3 -O2 -G0 (no
 * -fforce-addr), ASPSX 2.34 with its divide checks; the jump tables match without realignment. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "gte.h"

int rand(void);
int abs(int x);
int rsin(int a);
int rcos(int a);
int ratan2(int y, int x);
int SquareRoot0(int a);
int VectorNormal(VECTOR *v0, VECTOR *v1);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void SetGeomOffset(int x, int y);
void SetGeomScreen(int h);
void SetDispMask(int mask);
void PutDrawEnv(DRAWENV *env);
unsigned short GetTPage(int tp, int abr, int x, int y);
void func_80044244(unsigned int *ot, int n);                            /* ClearOTagR */
void func_8004433C(unsigned int *ot);                                   /* DrawOTag */
void func_800444AC(DISPENV *env);                                       /* PutDispEnv */
void func_80044000(RECT *r, void *p);                                   /* LoadImage */
void func_80044A68(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw); /* SetDrawMode */
int func_80043CC0(void);                                                /* GetGraphType */
unsigned int func_8001C808(int pad); /* the pad's buttons (the exe's takes none; the module passes 0) */
int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));

/* The exe's SetSp: the models are drawn on a stack in the scratchpad. The PC runs them on its own stack. */
#ifdef NATIVE
static inline int SetSp(int sp)
{
    return sp;
}
#else
int SetSp(int sp);
#endif

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

#define getTPage(tp, abr, x, y)                                                                                        \
    (func_80043CC0() == 1 || func_80043CC0() == 2                                                                      \
         ? (((tp) & 3) << 9) | (((abr) & 3) << 7) | (((y) & 0x300) >> 3) | (((x) & 0x3FF) >> 6)                        \
         : (((tp) & 3) << 7) | (((abr) & 3) << 5) | (((y) & 0x100) >> 4) | (((x) & 0x3FF) >> 6) | (((y) & 0x200) << 2))

/* The singleton form (x copied, then negated in place) is what gives the game's abs code. */
#define ABS(x) ((x) < 0 ? -(x) : (x))

/* libgte's DMPSX macros as this module has them: each operand copied to $12-$14 first, one statement per register
 * (the game's copies interleave with computing the operands); the $12 copy also takes $15. */
#ifdef NATIVE
/* The PC moves the words through memcpy: the game reads shorts and bytes as words. */
static inline unsigned int gte_word(const void *p, int i)
{
    unsigned int v;

    __builtin_memcpy(&v, (const char *)p + i * 4, 4);
    return v;
}
static inline void gte_put(void *p, int i, unsigned int v)
{
    __builtin_memcpy((char *)p + i * 4, &v, 4);
}
#define gte_rtps() gte_cmd(0x180001)
#define gte_rtpt() gte_cmd(0x280030)
#define gte_nclip() gte_cmd(0x1400006)
#define gte_avsz3() gte_cmd(0x158002D)
#define gte_dpct() gte_cmd(0xF8002A)
#define gte_ldv3(a, b, c)                                                                                              \
    (gte_write(0, gte_word(a, 0)), gte_write(1, gte_word(a, 1)), gte_write(2, gte_word(b, 0)),                         \
     gte_write(3, gte_word(b, 1)), gte_write(4, gte_word(c, 0)), gte_write(5, gte_word(c, 1)))
#define gte_ldsxy3(a, b, c) (gte_write(12, (a)), gte_write(14, (c)), gte_write(13, (b)))
#define gte_ldsz3(a, b, c) (gte_write(17, (a)), gte_write(18, (b)), gte_write(19, (c)))
#define gte_ldrgb3(a, b, c)                                                                                            \
    (gte_write(20, gte_word(a, 0)), gte_write(21, gte_word(b, 0)), gte_write(22, gte_word(c, 0)),                      \
     gte_write(6, gte_word(c, 0)))
#define gte_ldir0(v) gte_write(8, (v))
#define gte_stsxy(p) gte_put(p, 0, gte_read(14))
#define gte_stsxy3(a, b, c) (gte_put(a, 0, gte_read(12)), gte_put(b, 0, gte_read(13)), gte_put(c, 0, gte_read(14)))
#define gte_stsz3c(p) (gte_put(p, 0, gte_read(17)), gte_put(p, 1, gte_read(18)), gte_put(p, 2, gte_read(19)))
#define gte_stszotz(p) gte_put(p, 0, (int)gte_read(19) >> 2)
#define gte_stopz(p) gte_put(p, 0, gte_read(24))
#define gte_stotz(p) gte_put(p, 0, gte_read(7))
#define gte_strgb3(a, b, c) (gte_put(a, 0, gte_read(20)), gte_put(b, 0, gte_read(21)), gte_put(c, 0, gte_read(22)))
#define gte_SetFarColor(r, g, b)                                                                                       \
    (gte_write_ctrl(21, (r) << 4), gte_write_ctrl(22, (g) << 4), gte_write_ctrl(23, (b) << 4))
#else
#define gte_mov12(v) __asm__ volatile("move $12, %0" : : "r"(v) : "$12", "$15")
#define gte_mov13(v) __asm__ volatile("move $13, %0" : : "r"(v) : "$13")
#define gte_mov14(v) __asm__ volatile("move $14, %0" : : "r"(v) : "$14")
#define gte_rtps() __asm__ volatile("nop\nnop\n.word 0x4A180001")
#define gte_rtpt() __asm__ volatile("nop\nnop\n.word 0x4A280030")
#define gte_nclip() __asm__ volatile("nop\nnop\n.word 0x4B400006")
#define gte_avsz3() __asm__ volatile("nop\nnop\n.word 0x4B58002D")
#define gte_dpct() __asm__ volatile("nop\nnop\n.word 0x4AF8002A")
#define gte_ldv3(a, b, c)                                                                                              \
    {                                                                                                                  \
        gte_mov12(a);                                                                                                  \
        gte_mov13(b);                                                                                                  \
        gte_mov14(c);                                                                                                  \
        __asm__ volatile("lwc2 $0, 0($12)\nlwc2 $1, 4($12)\nlwc2 $2, 0($13)\nlwc2 $3, 4($13)\nlwc2 $4, 0($14)\n"       \
                         "lwc2 $5, 4($14)");                                                                           \
    }
#define gte_ldsxy3(a, b, c)                                                                                            \
    {                                                                                                                  \
        gte_mov12(a);                                                                                                  \
        gte_mov13(b);                                                                                                  \
        gte_mov14(c);                                                                                                  \
        __asm__ volatile("mtc2 $12, $12\nmtc2 $14, $14\nmtc2 $13, $13");                                               \
    }
#define gte_ldsz3(a, b, c)                                                                                             \
    {                                                                                                                  \
        gte_mov12(a);                                                                                                  \
        gte_mov13(b);                                                                                                  \
        gte_mov14(c);                                                                                                  \
        __asm__ volatile("mtc2 $12, $17\nmtc2 $13, $18\nmtc2 $14, $19");                                               \
    }
#define gte_ldrgb3(a, b, c)                                                                                            \
    {                                                                                                                  \
        gte_mov12(a);                                                                                                  \
        gte_mov13(b);                                                                                                  \
        gte_mov14(c);                                                                                                  \
        __asm__ volatile("lwc2 $20, 0($12)\nlwc2 $21, 0($13)\nlwc2 $22, 0($14)\nlwc2 $6, 0($14)");                     \
    }
#define gte_ldir0(v)                                                                                                   \
    {                                                                                                                  \
        gte_mov12(v);                                                                                                  \
        __asm__ volatile("mtc2 $12, $8");                                                                              \
    }
#define gte_stsxy(p) __asm__ volatile("move $12, %0\nswc2 $14, 0($12)" : : "r"(p) : "$12", "memory")
#define gte_stsxy3(a, b, c)                                                                                            \
    {                                                                                                                  \
        gte_mov12(a);                                                                                                  \
        gte_mov13(b);                                                                                                  \
        gte_mov14(c);                                                                                                  \
        __asm__ volatile("swc2 $12, 0($12)\nswc2 $13, 0($13)\nswc2 $14, 0($14)" : : : "memory");                       \
    }
#define gte_stsz3c(p)                                                                                                  \
    {                                                                                                                  \
        gte_mov12(p);                                                                                                  \
        __asm__ volatile("swc2 $17, 0($12)\nswc2 $18, 4($12)\nswc2 $19, 8($12)" : : : "memory");                       \
    }
#define gte_stszotz(p)                                                                                                 \
    __asm__ volatile("move $12, %0\nmfc2 $13, $19\nnop\nsra $13, $13, 2\nsw $13, 0($12)"                               \
                     :                                                                                                 \
                     : "r"(p)                                                                                          \
                     : "$12", "$13", "memory")
#define gte_stopz(p) __asm__ volatile("move $12, %0\nswc2 $24, 0($12)" : : "r"(p) : "$12", "memory")
#define gte_stotz(p) __asm__ volatile("move $12, %0\nswc2 $7, 0($12)" : : "r"(p) : "$12", "memory")
#define gte_strgb3(a, b, c)                                                                                            \
    {                                                                                                                  \
        gte_mov12(a);                                                                                                  \
        gte_mov13(b);                                                                                                  \
        gte_mov14(c);                                                                                                  \
        __asm__ volatile("swc2 $20, 0($12)\nswc2 $21, 0($13)\nswc2 $22, 0($14)" : : : "memory");                       \
    }
#define gte_SetFarColor(r, g, b)                                                                                       \
    {                                                                                                                  \
        gte_mov12(r);                                                                                                  \
        gte_mov13(g);                                                                                                  \
        gte_mov14(b);                                                                                                  \
        __asm__ volatile("sll $12, $12, 4\nsll $13, $13, 4\nsll $14, $14, 4\nctc2 $12, $21\nctc2 $13, $22\n"           \
                         "ctc2 $14, $23");                                                                             \
    }
#endif

/* r3 = r1 * r2, the rotations only (libgte's MulMatrix0, inline). */
#define gte_MulMatrix0(r1, r2, r3)                                                                                     \
    {                                                                                                                  \
        gte_SetRotMatrix(r1);                                                                                          \
        gte_ldclmv(r2);                                                                                                \
        gte_rtir();                                                                                                    \
        gte_stclmv(r3);                                                                                                \
        gte_ldclmv((char *)(r2) + 2);                                                                                  \
        gte_rtir();                                                                                                    \
        gte_stclmv((char *)(r3) + 2);                                                                                  \
        gte_ldclmv((char *)(r2) + 4);                                                                                  \
        gte_rtir();                                                                                                    \
        gte_stclmv((char *)(r3) + 4);                                                                                  \
    }

/* gte_stszotz as snobo2_draw_lines has it: its register choice wants $14 and $15 taken too. */
#ifdef NATIVE
#define gte_stszotz4(p) gte_stszotz(p)
#else
#define gte_stszotz4(p)                                                                                                \
    __asm__ volatile("move $12, %0\nmfc2 $13, $19\nnop\nsra $13, $13, 2\nsw $13, 0($12)"                               \
                     :                                                                                                 \
                     : "r"(p)                                                                                          \
                     : "$12", "$13", "$14", "$15", "memory")
#endif

/* libgte's screen point and colour (layouts from Sony's Libref). */
typedef struct DVECTOR {
    short vx, vy;
} DVECTOR;
typedef struct CVECTOR {
    unsigned char r, g, b, cd;
} CVECTOR;

/* A screen point and a colour as the course drawer moves them, a word at a time. */
typedef union {
    unsigned int word;
    DVECTOR v;
} Sxy;
typedef union {
    unsigned int word;
    CVECTOR c;
} Rgb;

/* Primitives written a word at a time (libgpu's layouts, but the game stores whole words): a colour word carries the
 * GPU code in its top byte. */
typedef struct FlatTri {
    unsigned int tag;
    unsigned int c0;
    int xy0, xy1, xy2;
} FlatTri;
typedef struct ShadedTri {
    unsigned int tag;
    unsigned int c0;
    int xy0;
    unsigned int c1;
    int xy1;
    unsigned int c2;
    int xy2;
} ShadedTri;
typedef struct ShadedQuad {
    unsigned int tag;
    unsigned int c0;
    int xy0;
    unsigned int c1;
    int xy1;
    unsigned int c2;
    int xy2;
    unsigned int c3;
    int xy3;
} ShadedQuad;
typedef struct TexTri {
    unsigned int tag;
    unsigned int code, xy0, uv0, xy1, uv1, xy2, uv2;
} TexTri;
typedef struct Tile { /* a TILE */
    unsigned int tag;
    unsigned int code;
    unsigned int xy;
    unsigned int wh;
} Tile;
typedef struct Dot { /* a TILE_1 */
    unsigned int tag;
    unsigned int code;
    unsigned int xy;
} Dot;
typedef struct Line4 { /* a LINE_F4 */
    unsigned int tag;
    unsigned int code;
    unsigned int xy[4];
    unsigned int end; /* 0x55555555 */
} Line4;
typedef struct BlendMode { /* a DR_TPAGE */
    unsigned int tag;
    unsigned int code;
} BlendMode;

/* A point of the course, an object's place (20.12 or whole units): a VECTOR without its pad. */
typedef struct Pos {
    int x, y, z;
} Pos;

/* A ground triangle of a piece: its vertices and its surface (copied as a word). */
typedef struct Face {
    unsigned char v[3] __attribute__((aligned(4)));
    unsigned char attr;
} Face;

/* A piece of the slope (D_800B85D8, 0x374 bytes): 7 rows of 7 points (the first row is the last of the piece before)
 * and their colours, then its ground as 72 triangles over them (6 by 6 quads, 2 each). The points' pads are a table of
 * their own: v[0-2].pad cut it into a grid of cells in x, v[3-5].pad in z, v[6].pad is the ground search's reach and
 * v[8 + n].pad the slope of face group n. */
typedef struct Piece {
    short kind;  /* the row of D_800B848C (its cells' handlers); 3: a fork */
    short pitch; /* the camera's pitch over it, / 3 */
    short bend;  /* it turns by this much over v[6].vx; its sign is the direction */
    short turn;  /* the angle it turns by */
    SVECTOR v[49];
    Rgb col[49];
    Face face[72];
} Piece;

/* A stretch of the course (D_800C824C, 32 bytes each): its steps (each a piece), and per step the piece, flags, a
 * heading and where it starts. */
typedef struct Segment {
    short len;
    unsigned char closed;  /* a fork's branch won't enter it (stretches 2 and 4, until the course's best score is 70) */
    unsigned char prev;    /* the stretch before, 0xFF none */
    unsigned char next[2]; /* at a fork: the branch, and the stretch it rejoins */
    short pad06;
    unsigned char *piece;  /* per step: its Piece in D_800B85D8 */
    unsigned char *flags;  /* per step: 0x80 the finish (and walls bounce), 0x20 holds the rider to the ground, 0x10
                            * no board glow, 8 fixed pitch, 4 ambience, 2 covered sky, 1 a jump */
    unsigned char *ahead;  /* per step: how many pieces to draw ahead */
    unsigned int *props;   /* per step: the kinds of the props on it, a byte each */
    short *angle;          /* per step: its heading */
    Pos *origin;           /* per step: where it starts */
} Segment;

/* The rider on his board (D_800DDE64). */
typedef struct Board {
    int flags;            /* 0x00 bit 0: stopped (no steering, no motion) */
    short turn_target;    /* 0x04 */
    short turn_rate;      /* 0x06 */
    short turn_timer;     /* 0x08 */
    short carve_time;     /* 0x0A */
    unsigned short state; /* 0x0C low byte: its handler in D_800DDF74 (read as a byte); bit 15: entered */
    short state_time;     /* 0x0E frames in the state */
    short lean;           /* 0x10 the lean or crouch */
    short air_time;       /* 0x12 frames off the ground */
    short speed;          /* 0x14 */
    short pad16;
    int ground_y;         /* 0x18 the ground's height under him, << 12; 0x7FFFFFFF off the course */
    int f1C;              /* 0x1C < 0: no ground */
    int f20;
    VECTOR pos;           /* 0x24 << 12; y grows downwards */
    int vx, vy, vz;       /* 0x34 velocity, << 12 */
    int bounce;           /* 0x40 the landing's impact; gravity while airborne */
    VECTOR dir;           /* 0x44 the velocity, normalised */
    char pad54[8];
    SVECTOR ground_rot;   /* 0x5C the turn that stands him on the ground under him; his shadow's */
    SVECTOR rot;          /* 0x64 his tilt, easing toward ground_rot; vy: the lean */
    SVECTOR normal;       /* 0x6C the ground's normal; pad: the slope's push is normal >> pad */
    SVECTOR facing;       /* 0x74 vy: his heading */
    char pad7C[0x10];
    short f8C, f8E, f90;  /* cleared as a state script starts */
    char pad92[6];
    MATRIX mat;           /* 0x98 the board's place */
} Board;

/* The course drawer's work, in the scratchpad (0x1F800000); the object update (snobo2_update_objs) reuses it and
 * hands it to each object. The points go in rows of 7; the *2 arrays are the last strip split in two. */
typedef struct Work {
    unsigned char f00;  /* bit 0: flat */
    unsigned char pad01;
    unsigned char f02;  /* in the course drawer: on a fork's branch; in the object update: the objects move */
    unsigned char f03;
    short f04;          /* the quad along the row, flat mode */
    char pad06[6];
    unsigned int flat_col[2]; /* flat mode's two colours */
    int opz;
    Pos origin;         /* the drawing origin */
    short idx;          /* the step drawn; in the object update, the rider's step on the course */
    short seg;
    unsigned short rejoin; /* after a fork's branch, the stretch to carry on down; 0 none */
    char pad2A[4];
    short left;
    short f30;
    short f32;
    short f34;
    short nmarks;
    Segment *course;
    Piece *slice;
    Pos *point;
    MATRIX lm; /* the piece's place in view */
    MATRIX rm; /* the piece's place on the course */
    MATRIX m;  /* the view */
    Rgb col[49];
    Rgb col2[13];
    int sxy[51];
    int sxy2[13];
    unsigned int sz[51]; /* the object update keeps the rider's place (whole units) in its first three */
    unsigned int sz2[13];
    unsigned int *ot;
    char *prim;
    SVECTOR sv;
} Work;

/* The scratchpad as the rider, camera and object code use it. */
typedef struct Scratch {
    union {
        SVECTOR sxy;
        int step;
    } u;
    SVECTOR pad08;
    SVECTOR sv;
    SVECTOR pad18;
    VECTOR v;
    VECTOR pad30;
    MATRIX m;
    MATRIX m2;
} Scratch;

#define WORK ((Work *)0x1F800000)
#define SCRATCH ((Scratch *)0x1F800000)

/* A place on the course being moved along (built in the scratchpad by snobo2_move_along_course). */
typedef struct Locator {
    short seg;
    short step;
    short face;
    short moved; /* what the grid cell's handler returned */
    short tries;
    short pad0A;
    int opz;
    Segment *segp;
    MATRIX rot;
    SVECTOR local;
    Piece *piece;
    VECTOR pos;
    VECTOR to;
    Face ground;
} Locator;

/* The scratchpad while the finish is timed: a locator, then the rider's distance along the piece before and after
 * the frame that crossed the line. */
typedef struct FinishWork {
    Locator loc;
    int before; /* 0x64 */
    int after;
    int dist;
} FinishWork;

#define FINISH ((FinishWork *)0x1F800000)

/* The camera: its angles and the point it looks at (relative to D_80106A60). */
typedef struct Camera {
    SVECTOR ang;
    SVECTOR at;
} Camera;

/* One mark left along the course this frame (D_80103994). */
typedef struct Mark {
    short seg;
    short idx;
    char pad04[4];
    Piece *slice;
    MATRIX m;
} Mark;

/* A model part queued for drawing (D_801053B8, D_80105384 of them). */
typedef struct Queued {
    MATRIX m;
    short how;   /* 0: set the turn and the move, 1: the move only, 2: neither (a second part on the same matrix) */
    short model;
    int drawer;  /* which drawer (D_800DFA40) */
    int shade;   /* each drawer's own: depth cue, clut or blend */
} Queued;

typedef struct Obj Obj;

/* What an object starts as (0x48 bytes): each spawner fills in a few fields of one of these templates in .data and
 * copies it whole into a free Obj. */
typedef struct Tmpl {
    short model;
    short frame;          /* a second model drawn on the same matrix, 0 none */
    short scale;          /* the rotation matrix's left shift */
    short pad06;
    int drawer;           /* 0x08 */
    int shade;            /* 0x0C */
    short radius;         /* 0x10 of the hit cylinder (0: no hits) */
    short top;
    short low;
    unsigned char react[4]; /* 0x16 the rider's state on a hit, by high/low and slow/fast; 0xFF none, 0 a stop */
    SVECTOR rot;          /* 0x1A rot.pad: the matrix is built (Obj.mat) */
    short pad22;
    Pos pos;              /* 0x24 20.12 */
    int ground;           /* 0x30 */
    void (*update)(Obj *o); /* also handed the course drawer's Work, which two of them use (Update2) */
    int arg[4];           /* 0x38 each kind's own: a home position, a mode, a timer, another Obj */
} Tmpl;

/* One of the 32 course objects (D_80103F70). */
struct Obj {
    unsigned char active; /* 2: kept however far behind the rider, and hit-tested anywhere */
    unsigned char hit;    /* 1: touched, 2: touched at speed */
    short id;             /* the course event that spawned it; it goes once the rider is past it */
    Tmpl t;
    MATRIX mat;           /* 0x4C */
    Pos vel;              /* 0x6C */
    int f78;
    int knock_x, knock_z; /* 0x7C the rider's velocity when he hit it */
    short t84, t86, t88;  /* timers */
    short t8A;            /* counts the fade out; -1 hidden */
};

/* An update as the object loop calls it: with the course drawer's work. */
typedef void (*Update2)(Obj *o, Work *w);

/* A place and its ground (Tmpl.pos, ground) or a velocity and f78 (Obj.vel, f78): the game copies each pair as one
 * 16-byte block. */
typedef struct Quad {
    int w[4];
} Quad;
#ifdef NATIVE
#define COPY_QUAD(dst, src) __builtin_memcpy(dst, src, sizeof(Quad))
#else
#define COPY_QUAD(dst, src) (*(Quad *)(dst) = *(Quad *)(src))
#endif

/* Where a course event's spawner puts its object: a turn, then an offset from the event's step's start (D_800C5A8C,
 * whole units), then 1-4 values of the spawner's own (a kind, a model, a timer, a height ...). The spawners take
 * records of the size they read; the events pass them from .data, several in a row where a step has more. */
typedef struct Place {
    short ry, x, y, z;
} Place;
typedef struct Place1 {
    short ry, x, y, z;
    short arg[1];
} Place1;
typedef struct Place2 {
    short ry, x, y, z;
    short arg[2];
} Place2;
typedef struct Place3 {
    short ry, x, y, z;
    short arg[3];
} Place3;
typedef struct Place4 {
    short ry, x, y, z;
    short arg[4];
} Place4;

/* A model as loaded: a header, then its parts, whose three pointers are stored as offsets from the first part. */
typedef struct ModelPart {
    unsigned int *verts; /* SVECTORs */
    int nverts;
    unsigned int *p08;
    int t0C;
    unsigned int *faces; /* for its drawer */
    int nfaces;
    int t18;
} ModelPart;

typedef struct Model {
    int t00;
    int flags; /* top bit: offsets already turned into pointers */
    int n;
    ModelPart part[0]; /* n of them */
} Model;

/* The scratchpad while a model is drawn: its part's screen positions and depths, the polygon being sorted and where
 * primitives go. */
typedef struct DrawWork {
    Model *model;
    int part;
    unsigned int *ot;
    int drawer; /* D_800DFA40 */
    int shade;
    int sxy[84];
    int sz[84];
    char pad2B4[8];
    int z;
    char pad2C0[0xC];
    void *prim;
} DrawWork;

/* A gouraud triangle as a model stores it: colours as in a POLY_G3, then its corners. */
typedef struct FaceG3 {
    int t00;
    unsigned int c0, c1, c2;
    unsigned short i0, i1, i2;
    short pad;
} FaceG3;

/* A flat textured triangle as a model stores it. */
typedef struct FaceFT3 {
    int t00;
    unsigned short uv0, clut;
    unsigned int uv1; /* and tpage */
    unsigned int uv2;
    int t10;
    unsigned short i0, i1, i2;
    short pad;
} FaceFT3;

/* The race time in BCD, ten-thousandths of a second up to tens of minutes. */
typedef struct BcdTime {
    unsigned d0 : 4, d1 : 4, d2 : 4, d3 : 4, d4 : 4, d5 : 4, d6 : 4, d7 : 4;
} BcdTime;

/* The race clock: from bit 16 seconds, tens of seconds, minutes, hours. */
typedef union Clock {
    unsigned int word;
    BcdTime t;
} Clock;

/* A recorded run (D_800B83A4, 4): slot 0 records the rider, the others are the courses' kept best runs, played back
 * beside him. A record is 15 bytes: x, y, z as 24-bit big-endian 12.12 values, then the turn's x, y, z as 16-bit
 * big-endian angles. */
typedef struct Ghost {
    short on;
    short frame; /* the record written or shown; -1 not started */
    short count; /* records kept */
    short pad06;
    unsigned char *rec;
    unsigned int time;
} Ghost;

/* The scene steps' work (D_80103954): snobo.c's Fade (count, level) grown by the results screen's state. One struct:
 * the game reuses its base register across the fields. */
typedef struct SceneWork {
    int count;            /* frames in the step, or the fade's level */
    int record;           /* a new best time */
    int choice;           /* on the results screen: 0 go on, 1 quit */
    int tick;             /* the record banner's frames */
    int place;            /* the run's place in the best times */
    int f14;
    unsigned int held, pressed; /* the buttons, kept from the scene */
} SceneWork;

/* A puff of snow thrown up by the board: eight in a ring (D_80103E6C, next at D_80103F6C). */
typedef struct Spray {
    int used;
    short x, y, z;       /* loaded as an SVECTOR */
    unsigned short rise; /* how far above x, y, z it is drawn */
    short dx, dy, dz;
    short pad12;
    short shade, fade;
    short w, dw;
    short h, dh;
} Spray;

/* The scratchpad as the board's glow is drawn. */
typedef struct GlowWork {
    short sx, sy;
    int otz;
    char pad08[8];
    SVECTOR v;
    char pad18[0x28];
    MATRIX m;
} GlowWork;

/* A rotation without the translation: the first 20 bytes of a MATRIX. */
typedef struct RotMat {
    short m[3][3];
    short pad;
} RotMat;

/* One joint of a rider's skeleton (20 bytes): SNOBO's Joint with its three matrices kept elsewhere. */
typedef struct Joint {
    RotMat *m0;       /* the bone's direction, under the joint's world rotation */
    RotMat *m1;       /* applied before the pose's rotation */
    RotMat *m2;       /* applied after it */
    short pos_parent; /* 0x0C the joint whose position this one adds to, or -1 */
    short parent;     /* 0x0E the joint whose world rotation this one follows, or -1 */
    short len;        /* 0x10 the bone's length */
    short no_bone;    /* 0x12 */
} Joint;

/* One drawn part of a rider: a model at a joint's position, turned like a joint; a negative model ends the list. */
typedef struct SkelPart {
    short model;
    short drawer;
    short joint;     /* 0x04 rotation from this joint */
    short pos_joint; /* 0x06 position from this joint, or -1: the root */
} SkelPart;

/* The scratchpad as the skeleton code uses it: up to 21 joints. */
typedef struct SkelWork {
    RotMat world[21];  /* 0x000 */
    SVECTOR pos[21];   /* 0x1A4 */
    SVECTOR pts[21];   /* 0x24C */
    MATRIX view;       /* 0x2F4 */
    MATRIX part;       /* 0x314 */
    MATRIX view_model; /* 0x334 */
    RotMat a;          /* 0x354 */
    RotMat b;          /* 0x368 */
    VECTOR v;          /* 0x37C */
    SVECTOR sv;        /* 0x38C */
    Joint *joints;     /* 0x394 */
    SkelPart *parts;   /* 0x398 */
    int pad39C;
    short count;       /* 0x3A0 joints */
} SkelWork;

/* A rider's animation set (D_800DDDBC, one per rider; D_80103E5C the current one): SNOBO's fixed pose, root and script
 * tables behind pointers. */
typedef struct Rig {
    short nangle;               /* 0x00 joint angles in a pose */
    short npose;                /* 0x02 words in a pose */
    int *poses;                 /* 0x04 npose words each */
    short (*roots)[3];          /* 0x08 per pose: its root */
    unsigned char (*script)[4]; /* 0x0C per rider state, 64 ops: its script */
    struct {
        short *pose;
        short *root;
    } key[5]; /* 0x10 a carve's start (its root unused), then upright, carving left, right, crouched */
} Rig;

/* The sky's gradient bands under the strip: their top (from the strip), height and colours. */
typedef struct SkyBand {
    short y;
    short h;
    unsigned int top;
    unsigned int bottom;
} SkyBand;

/* A triangle's 3 point numbers, read a word at a time. */
typedef union {
    unsigned int word;
    unsigned char pt[4];
} Tri;

/* The trigger flags (D_801050F4): a byte a trigger, cleared by words. */
typedef union {
    unsigned char seen[640];
    int word[160];
} SeenFlags;

/* A menu (D_800DF61C): its options in a column, centred on x. */
typedef struct Menu {
    short x, y;
    short count;
    short w;
    short pulse; /* the highlight pulses with the frame count >> this */
    short pad0A;
    char **item;
} Menu;

/* A character of the menu font (D_800DF7C4, from 0x10): its sprite's texture place and size; the low byte of the size
 * is its width. */
typedef struct Glyph {
    unsigned int uvclut;
    union {
        unsigned int word;
        unsigned char w;
    } wh;
} Glyph;

/* A framed window as the menus and screens draw it, in the scratchpad (0x1F800000): its place and size, and the colour
 * and points of the next line or dot. The results screen keeps its rows shown and their colour at the start. */
typedef struct Window {
    union {
        unsigned int rgb; /* the window drawer's next line or dot */
        int rows;         /* the results screen's rows shown */
    } u;
    unsigned int text_rgb; /* the results screen's rows' colour */
    char pad08[0x78];
    RECT box;     /* 0x80 */
    DVECTOR pt[4]; /* 0x88 */
} Window;

#define WIN ((Window *)0x1F800000)

/* The slope's height under depth z (both << 12) where the sliding objects run. */
#define SLOPE_Y(z) (((((z) >> 12) - 45122) * 22629 / 84531 + 68150) << 12)

/* A reread of x the game makes right after storing it (or after an early return), where GCC would reuse the value. */
#define REREAD(x) (*(volatile int *)&(x))

extern short D_8004BC98[]; /* libgte's rcossin_tbl: each angle's sine, then cosine */
extern unsigned char D_8009D4F8[]; /* the exe's: the minigame's result, three words then three bytes */
extern unsigned char D_8009D507; /* the exe's */
extern char D_800A034C[]; /* the clock's icon */
extern char D_800A0350[]; /* the record's icon */
extern char D_800A03C0[]; /* the cursor */
extern char D_800A0418[]; /* "PTS." */
extern int D_800B8260; /* the sound file's sector and size */
extern int D_800B8264; /* its size */
extern int D_800B8268; /* the texture file's sector and size */
extern int D_800B826C; /* its size */
extern int D_800B8270; /* the model file's sector and size */
extern int D_800B8274; /* its size */
extern DRAWENV D_800B8278[2]; /* the two buffers' draw environments */
extern DISPENV D_800B8330[2]; /* and display environments */
extern short D_800B835C[]; /* per stretch: its first step's number on the whole course */
extern int D_800B8378; /* the song's offset in the sound file */
extern int D_800B837C; /* the ambience's offset in the sound file */
extern int D_800B8380; /* the finish's sound's offset in the sound file */
extern Camera D_800B8384; /* the camera's angles and offset at the start */
extern Camera D_800B8394; /* the camera once the intro is over */
extern Ghost D_800B83A4[4]; /* the recorded runs: [0] the one being recorded, [course] the kept ones */
extern short D_800B83E4; /* how the ghost is drawn: 1, 2 a model pair of D_800B8438, 3 and up animated */
extern RECT D_800B83E8; /* cleared at the start */
extern int D_800B83F0[18]; /* its images (every one has a clut) */
extern short D_800B8438[2][2]; /* the ghost's model and second model, by D_800B83E4 */
extern short D_800B8440[4]; /* the animated ghost's second models */
extern unsigned int D_800B8448[][2]; /* per stretch, flat mode's two colours */
extern short D_800B8480[6]; /* the points the trackside props stand on */
extern unsigned char D_800B848C[]; /* per piece kind, 4 x 4 grid cells: the handler for each */
extern short (*D_800B84CC[])(Locator *r); /* the grid cells' handlers */
extern unsigned char D_800B84F0[]; /* per rider state: how the camera follows (1: along the motion, 2: the course) */
extern SVECTOR D_800B8510[6]; /* three lines' ends */
extern unsigned char D_800B8540[12]; /* their colours, four bytes each */
extern SkyBand D_800B854C[3]; /* the sky's gradient bands */
/* Per course, from 1 (the game's bases lie in the sky bands): the times that grade the run; its par in seconds. */
extern unsigned int D_800B8558[][8];
extern short D_800B856E[];
extern Piece D_800B85D8[]; /* the course's pieces */
extern Pos D_800C5A8C[]; /* per step of the whole course: where it starts */
extern Segment D_800C824C[]; /* the course's stretches */
extern short D_800D1D8E; /* frames left in the pose blend */
extern short D_800D1D90; /* the pose blended to */
extern int (*D_800D1D94[])(int t); /* the blends' easings */
extern MATRIX D_800D1DA4;
extern unsigned char D_800D1DB6; /* the shadow is shown */
extern Rig D_800DDDBC[]; /* per rider */
extern Board D_800DDE64; /* the rider */
extern int (*D_800DDF1C[14])(unsigned char *op); /* the script ops */
extern unsigned short D_800DDF54[]; /* the buttons the script waits on */
extern unsigned short D_800DDF64[]; /* the script's sounds */
extern void (*D_800DDF74[])(Board *b, int enter, Scratch *s); /* the rider's states */
extern short D_800DDFF4[]; /* per state: how high he may be above the ground before the shadow shows */
extern unsigned char D_800DE034[]; /* after a landing, by state, speed and impact: the next state */
extern short D_800DE154[]; /* per state: its looping sound */
extern short D_800DF404[]; /* per rider: its joints */
extern Joint *D_800DF40C[]; /* per rider: its skeleton */
extern SkelPart *D_800DF5A0[]; /* per rider: its parts */
extern char D_800DF5AC[]; /* time templates: drawn into, and blanks */
extern char D_800DF5B8[];
extern char D_800DF5C4[];
extern char D_800DF5D0[];
extern char D_800DF5DC[]; /* an icon, three digits and a sign */
extern Menu D_800DF61C[]; /* the pause and results menus */
extern unsigned int D_800DF68C[][4]; /* HUD pictures: a sprite's xy, uv and clut, size, then its texture page */
extern char *D_800DF74C[5]; /* the rows' names */
extern char *D_800DF760[5]; /* the rows' "  0/30" texts, the points written in */
extern short D_800DF774[5]; /* the rows' y */
extern char D_800DF780[]; /* "COURSE A RECORD" */
extern char D_800DF790[]; /* "00'00"000": a record written in */
extern char D_800DF79C[]; /* "--'--"---": no record */
extern char *D_800DF7A8[5]; /* "1ST." ... "5TH." */
extern Glyph D_800DF7C4[]; /* the menu font */
extern unsigned short D_800DFA1C[]; /* BCD values, summed by the bits of a mask */
extern unsigned short D_800DFA3C; /* the board's glow's frame count */
extern void (*D_800DFA40[])(DrawWork *d, void *faces, int n); /* the drawers */
extern Tmpl D_800DFA50;
extern Tmpl D_800DFA98;
extern Tmpl D_800DFAE0;
extern Tmpl D_800DFB28;
extern Tmpl D_800DFB70;
extern unsigned short D_800DFBB8[4]; /* the models it cycles through (right after its template, D_800DFB70) */
extern Tmpl D_800DFBC0;
extern Tmpl D_800DFC08;
extern Tmpl D_800DFC50;
extern Tmpl D_800DFC98;
extern Tmpl D_800DFCE0;
extern Tmpl D_800DFD28;
extern Tmpl D_800DFD70;
extern Tmpl D_800DFDB8;
extern Tmpl D_800DFE00;
extern Tmpl D_800DFE48;
extern short D_800DFE90[6]; /* the swinging pair's partner's frames */
extern Tmpl D_800DFE9C;
extern Tmpl D_800DFEE4;
extern unsigned short D_800DFF2C[]; /* the pickups' worth */
extern Tmpl D_800DFF38;
extern Tmpl D_800DFF80;
extern Tmpl D_800DFFC8;
extern Tmpl D_800E0010;
extern Tmpl D_800E0058;
extern Tmpl D_800E00A0;
extern Tmpl D_800E00E8;
extern Tmpl D_800E0130;
extern Tmpl D_800E0178;
extern Place1 D_800E01C0;
extern Place2 D_800E01CC;
extern Place1 D_800E01D8;
extern Place1 D_800E01E4;
extern Place1 D_800E01F0;
extern Place1 D_800E01FC;
extern Place1 D_800E0208;
extern Place1 D_800E0214;
extern Place1 D_800E0220;
extern Place1 D_800E022C;
extern Place1 D_800E0238;
extern Place1 D_800E0244;
extern Place1 D_800E0250;
extern Place1 D_800E025C;
extern Place1 D_800E0268;
extern Place1 D_800E0274;
extern Place1 D_800E0280;
extern Place1 D_800E028C;
extern Place1 D_800E0298;
extern Place1 D_800E02A4;
extern Place1 D_800E02B0;
extern Place1 D_800E02BC[2];
extern Place1 D_800E02D0;
extern Place1 D_800E02DC;
extern Place1 D_800E02E8;
extern Place1 D_800E02F4;
extern Place1 D_800E0300;
extern Place1 D_800E030C;
extern Place1 D_800E0314;
extern Place1 D_800E0320;
extern Place1 D_800E0328;
extern Place1 D_800E0334;
extern Place1 D_800E0340;
extern Place2 D_800E034C;
extern Place1 D_800E0358;
extern Place2 D_800E0364;
extern Place1 D_800E0370;
extern Place1 D_800E037C;
extern Place1 D_800E0388;
extern Place1 D_800E0394[5];
extern Place1 D_800E03C8[6];
extern Place D_800E0404;
extern Place1 D_800E040C;
extern Place D_800E0418;
extern Place1 D_800E0420;
extern Place1 D_800E042C;
extern Place1 D_800E0438;
extern Place1 D_800E0444;
extern Place2 D_800E0450;
extern Place1 D_800E045C;
extern Place2 D_800E0468;
extern Place1 D_800E0474;
extern Place2 D_800E0480;
extern Place1 D_800E048C;
extern Place1 D_800E0498;
extern Place1 D_800E04A4[2];
extern Place1 D_800E04B8[2];
extern Place1 D_800E04CC[2];
extern Place1 D_800E04E0;
extern Place1 D_800E04EC;
extern Place2 D_800E04F8;
extern Place2 D_800E0504;
extern Place2 D_800E0510;
extern Place1 D_800E051C;
extern Place D_800E0528;
extern Place1 D_800E0530;
extern Place D_800E053C;
extern Place1 D_800E0544;
extern Place D_800E0550;
extern Place D_800E0558;
extern Place D_800E0560;
extern Place D_800E0568;
extern Place1 D_800E0570;
extern Place D_800E057C;
extern Place D_800E0584;
extern Place D_800E058C[2];
extern Place1 D_800E059C;
extern Place D_800E05A8;
extern Place D_800E05B0;
extern Place D_800E05B8;
extern Place D_800E05C0;
extern Place D_800E05C8;
extern Place1 D_800E05D0;
extern Place D_800E05DC;
extern Place D_800E05E4;
extern Place D_800E05EC;
extern Place D_800E05F4;
extern Place1 D_800E05FC;
extern Place D_800E0608;
extern Place D_800E0610[2];
extern Place1 D_800E0620;
extern Place D_800E062C[2];
extern Place D_800E063C;
extern Place D_800E0644[2];
extern Place D_800E0654[2];
extern Place D_800E0664;
extern Place D_800E066C[2];
extern Place1 D_800E067C;
extern Place D_800E0688;
extern Place D_800E0690;
extern Place1 D_800E0698;
extern Place D_800E06A4[3];
extern Place1 D_800E06BC;
extern Place D_800E06C8[3];
extern Place1 D_800E06E0;
extern Place D_800E06EC[3];
extern Place1 D_800E0704;
extern Place1 D_800E0710;
extern Place1 D_800E071C;
extern Place1 D_800E0728;
extern Place1 D_800E0734[2];
extern Place3 D_800E0748[2];
extern Place3 D_800E0764[2];
extern Place3 D_800E0780[2];
extern Place3 D_800E079C[2];
extern Place3 D_800E07B8;
extern Place3 D_800E07C8[2];
extern Place1 D_800E07E4;
extern Place1 D_800E07F0;
extern Place1 D_800E07FC[2];
extern Place1 D_800E0810;
extern Place1 D_800E081C;
extern Place1 D_800E0828;
extern Place1 D_800E0834;
extern Place1 D_800E0840;
extern Place1 D_800E084C;
extern Place1 D_800E0858;
extern Place1 D_800E0864;
extern Place1 D_800E0870[2];
extern Place1 D_800E0884;
extern Place1 D_800E0890;
extern Place1 D_800E089C;
extern Place1 D_800E08A8;
extern Place1 D_800E08B4;
extern Place1 D_800E08C0;
extern Place1 D_800E08CC;
extern Place1 D_800E08D8[2];
extern Place1 D_800E08EC[2];
extern Place1 D_800E0900;
extern Place1 D_800E090C;
extern Place1 D_800E0918;
extern Place1 D_800E0924;
extern Place1 D_800E0930;
extern Place1 D_800E093C;
extern Place1 D_800E0948[2];
extern Place1 D_800E095C;
extern Place1 D_800E0968;
extern Place1 D_800E0974;
extern Place1 D_800E0980;
extern Place1 D_800E098C;
extern Place1 D_800E0998;
extern Place1 D_800E09A4;
extern Place1 D_800E09B0;
extern Place1 D_800E09BC;
extern Place1 D_800E09C8;
extern Place1 D_800E09D4;
extern Place1 D_800E09E0;
extern Place1 D_800E09EC;
extern Place1 D_800E09F8;
extern Place1 D_800E0A04;
extern Place1 D_800E0A10;
extern Place1 D_800E0A1C;
extern Place1 D_800E0A28;
extern Place1 D_800E0A34;
extern Place1 D_800E0A40;
extern Place1 D_800E0A4C;
extern Place1 D_800E0A58;
extern Place1 D_800E0A64;
extern Place1 D_800E0A70[2];
extern Place1 D_800E0A84;
extern Place2 D_800E0A90;
extern Place2 D_800E0A9C;
extern Place2 D_800E0AA8;
extern Place1 D_800E0AB4;
extern Place1 D_800E0AC0[2];
extern Place1 D_800E0AD4[2];
extern Place1 D_800E0AE8[2];
extern Place1 D_800E0AFC[2];
extern Place1 D_800E0B10;
extern Place1 D_800E0B1C;
extern Place1 D_800E0B28;
extern Place1 D_800E0B34;
extern Place1 D_800E0B40;
extern Place1 D_800E0B4C;
extern Place1 D_800E0B58;
extern Place3 D_800E0B64;
extern Place3 D_800E0B74;
extern Place3 D_800E0B84;
extern Place3 D_800E0B94;
extern Place3 D_800E0BA4;
extern Place3 D_800E0BB4;
extern Place3 D_800E0BC4;
extern Place3 D_800E0BD4;
extern Place3 D_800E0BE4;
extern Place3 D_800E0BF4;
extern Place3 D_800E0C04;
extern Place3 D_800E0C14;
extern Place3 D_800E0C24;
extern Place3 D_800E0C34;
extern Place3 D_800E0C44;
extern Place3 D_800E0C54;
extern Place3 D_800E0C64;
extern Place3 D_800E0C74;
extern Place3 D_800E0C84;
extern Place1 D_800E0C94;
extern Place3 D_800E0CA0;
extern Place3 D_800E0CB0;
extern Place3 D_800E0CC0;
extern Place3 D_800E0CD0;
extern Place1 D_800E0CE0;
extern Place1 D_800E0CEC;
extern Place1 D_800E0CF8;
extern Place1 D_800E0D04;
extern Place1 D_800E0D10;
extern Place1 D_800E0D1C;
extern Place1 D_800E0D28;
extern Place1 D_800E0D34;
extern Place1 D_800E0D40;
extern Place1 D_800E0D4C;
extern Place1 D_800E0D58;
extern Place1 D_800E0D64;
extern Place1 D_800E0D70;
extern Place1 D_800E0D7C;
extern Place1 D_800E0D88;
extern Place1 D_800E0D94;
extern Place1 D_800E0DA0[2];
extern Place1 D_800E0DB4;
extern Place1 D_800E0DC0;
extern Place1 D_800E0DCC;
extern Place1 D_800E0DD8;
extern Place1 D_800E0DE4;
extern Place1 D_800E0DF0;
extern Place1 D_800E0DFC;
extern Place1 D_800E0E08;
extern Place1 D_800E0E14;
extern Place1 D_800E0E20;
extern Place4 D_800E0E2C;
extern Place1 D_800E0E3C;
extern Place4 D_800E0E48;
extern Place4 D_800E0E58[2];
extern Place1 D_800E0E78;
extern Place4 D_800E0E84;
extern Place4 D_800E0E94[2];
extern Place1 D_800E0EB4;
extern Place4 D_800E0EC0;
extern Place1 D_800E0ED0;
extern Place4 D_800E0EDC;
extern Place1 D_800E0EEC;
extern Place2 D_800E0EF8;
extern void (*D_800E0F04[])(int id, int first); /* the course's triggers, by step */
extern SVECTOR D_800E18F4; /* where the race step draws the rider */
extern short D_800E18FC; /* the pause menu's cursor */
extern SVECTOR D_800E1904; /* where the scene draws him */
extern SVECTOR D_800E1910; /* where the still scene draws him */
extern SVECTOR D_800E1918[3]; /* the ground triangle under the rider */
extern SVECTOR *D_800E1930; /* the piece's first row, as the piece before has it */
extern unsigned char D_800E1944; /* the main loop runs */
extern unsigned char D_800E1948; /* the run was finished */
extern void (*D_800E194C)(void); /* the scene's step */
extern int D_800E1950; /* the buffer drawn into */
extern char D_800E1954[2][0xD000]; /* primitive buffers */
extern unsigned int D_800FB954[2][0x1000]; /* ordering tables */
extern SceneWork D_80103954; /* the scene steps' work */
extern SVECTOR D_80103974; /* where the intro draws him */
extern Clock D_8010397C; /* the run's time, kept */
extern short D_80103980; /* the pause menu's page */
extern short D_80103984; /* with D_801053A4: the course is drawn flat */
extern short D_8010398C;
extern Mark D_80103994[]; /* this frame's marks */
extern Camera D_80103D04; /* the camera */
extern SVECTOR D_80103D14; /* the pose blend: from root */
extern short D_80103D1C[3]; /* to root */
extern short D_80103D24[]; /* from pose */
extern short D_80103DA4[]; /* to pose */
extern int (*D_80103E24)(int t);  /* its easing */
extern short D_80103E28; /* its length */
extern short D_80103E2C; /* the board's looping sound */
extern short D_80103E30; /* its pitch */
extern unsigned char D_80103E34[]; /* the script's loops: start */
extern unsigned char D_80103E38[]; /* and count */
extern short D_80103E3C; /* loop depth */
extern short D_80103E40; /* the script's pc */
extern unsigned char D_80103E44[4]; /* per trigger slot: armed */
extern unsigned char D_80103E48[4]; /* fired (an array: as a scalar GCC hoists a load above the store) */
extern unsigned char D_80103E4C[4]; /* the op to jump to */
extern unsigned char D_80103E50; /* the script's switch selector */
extern unsigned char D_80103E54; /* its cue */
extern unsigned char D_80103E58;
extern Rig *D_80103E5C; /* the rider's */
extern DR_MODE D_80103E60; /* the HUD's draw mode */
extern Spray D_80103E6C[8]; /* the snow puffs */
extern short D_80103F6C; /* the next one */
extern Obj D_80103F70[32]; /* the course objects */
extern short D_801050F0; /* where the next free-object search starts */
extern SeenFlags D_801050F4; /* the triggers seen */
extern short D_80105374; /* the id of the object knocked last (snobo.c's D_80101190) */
extern short D_80105378; /* cleared with the objects */
extern unsigned short D_8010537C; /* frames (the HUD and menus read it signed) */
extern unsigned int *D_80105380; /* this frame's ordering table */
extern short D_80105384; /* models queued this frame */
extern short D_80105388; /* pickup 4 taken: the slope pulls twice as hard */
extern void *D_8010538C; /* this frame's next primitive */
extern short D_80105390[4]; /* the score's parts and total (an array: the game takes its last element's address) */
extern unsigned char D_80105398; /* the clock runs (set once the rider reaches the course's start, with D_80106A28) */
extern int D_8010539C; /* frames the buttons have been held */
extern unsigned int D_801053A0; /* buttons held */
extern short D_801053A4; /* a timed run (pickup 3 starts one): most spawners stay off */
extern unsigned int D_801053A8; /* held last frame */
extern unsigned int D_801053AC; /* auto-repeated */
extern unsigned int D_801053B0; /* buttons pressed */
extern short D_801053B4; /* the course's heading under the rider */
extern Queued D_801053B8[]; /* the models queued this frame */
extern short D_801069B8; /* the rider (from D_8009D507): D_800DDDBC's index */
extern unsigned short D_801069BC; /* pickups collected */
extern unsigned int D_801069C0[3]; /* per course: the best time */
extern short D_801069CC; /* the rider's step in his stretch */
extern short D_801069D0; /* the rider's face (lane) on the piece */
extern short D_801069D4; /* crashes */
extern MATRIX D_801069D8; /* the rider's turn about y */
extern short D_801069F8; /* frames to the next snow puff */
extern MATRIX D_801069FC; /* the camera */
extern short D_80106A28; /* the course, 1-3 (its ghost's slot, its records' row + 1); 0 none */
extern short D_80106A2C; /* the course's heading at the rider's step */
extern short D_80106A30; /* the rider's step on the whole course */
extern MATRIX D_80106A34; /* his turn back */
extern SVECTOR D_80106A54; /* the pose's root */
extern short D_80106A5C; /* paused */
extern Pos D_80106A60; /* the drawing origin */
extern int D_80106A70; /* the stack pointer kept while on the scratchpad's */
extern short D_80106A74[]; /* the pose's joint angles */
extern unsigned char D_80106AF4[3]; /* per course: the best score */
extern Segment *D_80106AF8; /* the rider's stretch */
extern Clock D_80106AFC; /* the run's time */
extern short D_80106B00; /* the bend's share of the heading */
extern short D_80106B04; /* the rider's stretch */
extern unsigned int D_80106B08[3][5]; /* per course: the five best times, 0 none */
extern SVECTOR D_80106B44; /* his place on the piece (x, z) */
extern unsigned char D_80106B4C; /* the surface under him */
extern short D_80106B50; /* the ghost moves */
extern VECTOR D_80106B54; /* his position, kept */
extern short D_80106B64; /* the rider's steps, kept */
extern short D_80106B66;
extern short D_80106B68;
extern short D_80106B6C; /* the speed shown */

void snobo2_step_start(void);
void snobo2_step_fade_in(void);
void snobo2_step_intro(void);
void snobo2_step_race(void);
void snobo2_pause_menu(void);
void snobo2_step_finish(void);
void snobo2_step_finish_wait(void);
void snobo2_step_score(void);
void snobo2_step_time(void);
void snobo2_step_results(void);
void snobo2_step_retry(void);
void snobo2_step_quit(void);
void snobo2_draw_scene(void);
void snobo2_draw_scene_still(void);
void snobo2_rider_update(void);
void snobo2_play_effect(int channel, int sound, int volume);
void snobo2_start_music(void);
void snobo2_fade_music(void);
void snobo2_start_ambience(void);
void snobo2_read_pad(void);
void snobo2_load_music(void);
void snobo2_load_textures(void);
void snobo2_load_models(void);
void snobo2_load_records(void);
void snobo2_store_records(void);
void snobo2_keep_best(void);
int snobo2_rank_time(void);
void snobo2_begin_frame(void);
void snobo2_flip(void);
void snobo2_save_rider(void);
void snobo2_set_origin(void);
void snobo2_ease_tilt(void);
void snobo2_draw_hud(void);
void snobo2_speed_sound(void);
void snobo2_place_rider(int arg);
void snobo2_keep_ghost(void);
void snobo2_record_ghost(void);
void snobo2_draw_ghost(void);
void snobo2_init_run(int seg, int idx, int state);
void snobo2_draw_course(int back, int count);
void snobo2_add_mark(Work *w);
void snobo2_set_slice_matrix(Work *w);
int snobo2_project_slice(Work *w, int *sxy_in, unsigned int *sz_in, Rgb *col_in);
void snobo2_draw_slice(Work *w);
void snobo2_project_first_slice(Work *w);
void snobo2_split_last_strip(Work *w);
void snobo2_place_props(Work *w);
void snobo2_draw_marks(Work *w);
void snobo2_move_along_course(VECTOR *from, VECTOR *to);
void snobo2_rider_enter_step(Locator *r, short step);
int snobo2_find_ground_face(SVECTOR *p, Piece *pc, Locator *r);
int snobo2_rider_enter_segment(Locator *r);
void snobo2_bounce_player(Locator *r, VECTOR *from, VECTOR *to);
int snobo2_deflect_velocity(Locator *r);
int snobo2_plane_height(SVECTOR *p, SVECTOR *a, SVECTOR *b, SVECTOR *c);
short snobo2_course_heading(SVECTOR *p, Piece *pc);
void snobo2_normal_to_angles(SVECTOR *n, SVECTOR *rot);
void snobo2_face_normal(int step, int face, SVECTOR *out, Scratch *sc);
void snobo2_camera_target(SVECTOR *p, Piece *pc, Scratch *sc);
void snobo2_camera_follow(Scratch *sc);
void snobo2_camera_start_or_follow(Scratch *sc, int fixed);
void snobo2_camera_start(Scratch *sc);
void snobo2_camera_intro(Scratch *sc);
void snobo2_draw_sky(void);
void snobo2_darken_screen(int level, void *ot);
void snobo2_flash_screen(int level, void *ot);
void snobo2_fill_screen(unsigned int rgb, void *ot);
void snobo2_score_run(void);
void snobo2_time_finish(void);
int snobo2_rank(void);
void snobo2_restart_run(void);
void snobo2_pose_blend_start(int frames, int ease);
void snobo2_copy_pose(int *dst, int *src);
void snobo2_pose_blend_step(void);
void snobo2_rider_reset_sound(void);
void snobo2_fire_script_trigger(int slot);
void snobo2_rider_frame(SVECTOR *pos, Scratch *s, int update, int draw);
void snobo2_rider_shadow(Scratch *s);
int snobo2_jump_state(Board *b);
void snobo2_carve_turn(Board *b, int turn, Scratch *s);
int snobo2_carve_too_hard(Board *b);
void snobo2_spin_step(Board *b, Scratch *s);
void snobo2_crash_slowdown(Board *b);
void snobo2_pose_lerp(short *from, short *to, short *from_root, short *to_root, int t);
void snobo2_face_motion(Board *b);
void snobo2_rider_move(Board *b, Scratch *s);
void snobo2_rider_fall(Board *b);
int snobo2_angle_step(int to, int from, int max);
void snobo2_rider_land(Board *b, int impact);
void snobo2_board_sound(Board *b);
void snobo2_pose_skeleton(MATRIX *m, MATRIX *view);
void snobo2_init_hud_mode(void);
Clock snobo2_time_add(Clock c, unsigned short step);
void snobo2_draw_time_and_record(unsigned int time, short x, short y);
void snobo2_time_text(char *s, unsigned int time, int base);
void snobo2_draw_number(unsigned short n, short x, short y);
void snobo2_draw_menu(int n, int sel);
void snobo2_draw_pulse_box_a(int t);
void snobo2_draw_pulse_box_b(int t);
void snobo2_draw_pulse_box_c(int t);
void snobo2_draw_hud_pic(int n);
void snobo2_draw_results(Window *w, int t, unsigned int *ot);
void snobo2_draw_records(int course, int sel, int t);
void snobo2_draw_text(char *s, int x, int y, unsigned int rgb);
void snobo2_draw_window(Window *w, int scale, unsigned int *ot);
Line4 *snobo2_add_window_line(Window *w, Line4 *p, unsigned int *ot);
Dot *snobo2_add_window_dot(Window *w, Dot *p, unsigned int *ot);
unsigned short snobo2_bcd_sum_mask(unsigned short mask);
unsigned short snobo2_bcd_add(unsigned short a, unsigned short b);
void snobo2_clear_spray(void);
void snobo2_spawn_spray(Board *b);
void snobo2_draw_board_glow(Board *b, MATRIX *view, GlowWork *w, int count);
void snobo2_relocate_model(Model *m);
void *snobo2_draw_queued(Model *m, unsigned int *ot, void *prim);
void snobo2_draw_part(DrawWork *d);
void snobo2_obj_fly(Obj *o);
void snobo2_obj_sway(Obj *o);
void snobo2_obj_wobble(Obj *o);
void snobo2_obj_slide(Obj *o);
void snobo2_obj_slider_hit(Obj *o);
void snobo2_obj_slider_fly(Obj *o);
void snobo2_obj_fall_tip(Obj *o);
void snobo2_obj_tumble_down(Obj *o);
void snobo2_obj_run_ahead_a(Obj *o);
void snobo2_obj_spin_out_a(Obj *o);
void snobo2_obj_run_across(Obj *o);
void snobo2_obj_drop_item(Obj *o);
void snobo2_obj_fly_land(Obj *o);
void snobo2_obj_run_ahead_c(Obj *o);
void snobo2_obj_spin_out_c(Obj *o);
void snobo2_obj_fall(Obj *o);
void snobo2_spawn_thrower(Obj *o);
void snobo2_obj_blink(Obj *o);
void snobo2_obj_thrower_wait(Obj *o);
void snobo2_obj_thrower(Obj *o);
void snobo2_obj_thrower_fade(Obj *o);
void snobo2_obj_thrown(Obj *o);
void snobo2_obj_dropped(Obj *o);
void snobo2_spawn_burst_flyer(Obj *o);
void snobo2_spawn_spinner(Obj *o, int dir);
void snobo2_obj_roll_downhill(Obj *o, Work *w);
short snobo2_roll_frame(int i);
void snobo2_obj_fly_spin(Obj *o);
void snobo2_obj_hop(Obj *o);
void snobo2_obj_hopper_knock(Obj *o);
void snobo2_obj_hopper_fly(Obj *o);
void snobo2_obj_pickup_rise(Obj *o);
void snobo2_spawn_flash_a(); /* snobo2_obj_pickup calls it with no argument */
void snobo2_obj_flash_a(Obj *o);
void snobo2_spawn_flash_b(); /* snobo2_obj_pickup calls it with no argument */
void snobo2_obj_flash_b(Obj *o);
void snobo2_obj_topple_back(Obj *o);
void snobo2_obj_topple(Obj *o);
void snobo2_obj_fall_back(Obj *o);
void snobo2_obj_fall_forward(Obj *o);
void snobo2_obj_wake_near_rider(Obj *o);
void snobo2_obj_drop(Obj *o);
void snobo2_obj_landed(Obj *o);
int snobo2_obj_floater_touch(Obj *o);
void snobo2_obj_knocked_spin(Obj *o);
void snobo2_board_tumble(void);
void snobo2_clear_triggers(void);
void snobo2_fire_trigger(int id);
void snobo2_clear_objs(void);
void snobo2_update_objs(Work *v);
void snobo2_obj_expire(Obj *o);

/* m = a turn about y by angle a (the game builds it inline from rcossin_tbl). */
static inline void rot_y(MATRIX *m, int a)
{
    short *t = &D_8004BC98[(a & 0xFFF) * 2];
    short sn = t[0], cs = t[1];

    m->m[0][0] = cs;
    m->m[0][1] = 0;
    m->m[0][2] = sn;
    m->m[1][0] = 0;
    m->m[1][1] = 0x1000;
    m->m[1][2] = 0;
    m->m[2][0] = -sn;
    m->m[2][1] = 0;
    m->m[2][2] = cs;
}

/* A point's x, z as a screen point word. */
#define SXY(v) ((v)->vx << 16 | (unsigned short)(v)->vz)

/* The game inlines the object spawning into every spawner (no copy of it stands alone). The next free object,
 * searched round from D_801050F0; 0 when all 32 are taken. */
static inline Obj *obj_alloc(void)
{
    Obj *p;
    int n;
    int i = D_801050F0;

    for (n = 31; n != -1; n--) {
        p = &D_80103F70[i];
        if (!p->active) {
            D_801050F0 = (i + 1) & 31;
            return p;
        }
        i = (i + 1) & 31;
    }
    return 0;
}

/* A fresh object started from template t, at rest, active as given (1, or 2 to keep it whatever the rider's step); 0
 * when the pool is full. id goes through a 16-bit parameter: the copy that makes is in the game's code wherever the
 * caller uses id afterwards. active is a parameter so its constant loads first, where the game's does. */
static inline Obj *obj_spawn(short id, Tmpl *t, int active)
{
    Obj *o = obj_alloc();

    if (o) {
        o->active = active;
        o->hit = 0;
        o->id = id;
        o->t = *t;
        o->vel.x = 0;
        o->vel.y = 0;
        o->vel.z = 0;
        o->f78 = 0;
        o->t84 = 0;
        o->t86 = 0;
        o->t88 = 0;
        o->t8A = 0;
    }
#ifdef NATIVE
    {
        /* Some spawners set the new object's fields without checking: with the pool full, the PS1 writes them to low
         * RAM through 0. The PC hands them a spare. */
        static Obj spare;

        if (!o)
            return &spare;
    }
#endif
    return o;
}

/* At the end of a gated section: a jump off its lip once the rider is far enough above the ground. */
static inline void check_jump(int id)
{
    if (D_80106A30 == id && (unsigned char)D_800DDE64.state != 11 &&
        (D_800DDE64.ground_y - D_800DDE64.pos.vy) >> 12 > 0x240)
        snobo2_board_tumble();
}

/* snobo2_rider_enter_step as snobo2_time_finish inlines it twice: its matrix stores come in another order. */
static inline void finish_enter_step(Locator *r, short step)
{
    short *cs;
    short s, c;
    Pos *o;
    MATRIX *m = &r->rot;

    r->piece = &D_800B85D8[r->segp->piece[step]];
    cs = &D_8004BC98[(-r->segp->angle[step] & 0xFFF) * 2];
    s = cs[0];
    c = cs[1];
    m->m[0][1] = 0;
    m->m[1][0] = 0;
    m->m[1][1] = 0x1000;
    m->m[1][2] = 0;
    m->m[2][1] = 0;
    m->m[0][0] = c;
    m->m[0][2] = s;
    m->m[2][0] = -s;
    m->m[2][2] = c;
    o = &r->segp->origin[step];
    r->local.vx = r->pos.vx - o->x;
    r->local.vz = r->pos.vz - o->z;
    ApplyMatrixSV(&r->rot, &r->local, &r->local);
}

/* end of declarations */

void snobo2_main(void)
{
    snobo2_load_music();
    snobo2_load_textures();
    snobo2_load_models();
    snobo2_load_records();
    D_800E1950 = 0;
    SetGeomOffset(0, 0);
    SetGeomScreen(0xA0);
    ClearImage(&D_800B83E8, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);
    D_800E1944 = 1;
    D_80103984 = 1;
    D_800E194C = snobo2_step_start;
    switch (D_8009D507) {
    case 2:
        D_801069B8 = 1;
        break;
    case 8:
        D_801069B8 = 2;
        break;
    default:
        D_801069B8 = 0;
        break;
    }
    while (D_800E1944) {
        snobo2_read_pad();
        snobo2_begin_frame();
        D_800E194C();
        /* the models are drawn on a stack in the scratchpad */
        D_80106A70 = SetSp(0x1F8003FC);
        D_8010538C = snobo2_draw_queued((void *)0x80178000, D_80105380, D_8010538C);
        SetSp(D_80106A70);
        snobo2_flip();
        cd_step();
        D_8010537C++;
    }
    snobo2_store_records();
    snobo2_start_music();
}

void snobo2_step_start(void)
{
    snobo2_start_music();
    snobo2_start_ambience();
    snobo2_init_run(0, 0, 0x10);
    snobo2_fill_screen(0, D_80105380);
    D_800E194C = snobo2_step_fade_in;
}

void snobo2_step_fade_in(void)
{
    int c = *(unsigned char *)&D_80103954.count;

    snobo2_fill_screen(c << 16 | c << 8 | c, D_80105380);
    D_80103954.count += 4;
    if (D_80103954.count >= 0x100) {
        D_80103954.count = 0;
        D_800E194C = snobo2_step_intro;
    }
}

void snobo2_step_intro(void)
{
    int n;

    if (D_801053B0 & 0x800) {
        /* start skips the intro */
        snobo2_fire_script_trigger(0);
        D_80103954.count = 0xFF;
    }
    n = ++D_80103954.count;
    if (n < 0x80)
        snobo2_flash_screen(0x100 - n * 2, D_80105380);
    if (n < 0x100) {
        D_800B8384.ang.vx = (n * 96 >> 8) - 0x20;
        D_800B8384.ang.vy = 0x1000 - (n * 2048 >> 8);
        D_800B8384.ang.vz = 0;
        D_800B8384.at.vx = 0;
        D_800B8384.at.vy = (n * 64 >> 8) + 0x40;
        D_800B8384.at.vz = (n * 224 >> 8) + 0x60;
    } else {
        D_800B8384 = D_800B8394;
    }
    snobo2_move_along_course(&D_800DDE64.pos, &D_800DDE64.pos);
    snobo2_set_origin();
    snobo2_camera_start((void *)0x1F800000);
    snobo2_draw_course(1, 2);
    D_80103974.vx = (D_800DDE64.pos.vx >> 12) - D_80106A60.x;
    D_80103974.vy = (D_800DDE64.pos.vy >> 12) - D_80106A60.y;
    D_80103974.vz = (D_800DDE64.pos.vz >> 12) - D_80106A60.z;
    snobo2_rider_frame(&D_80103974, (void *)0x1F800000, 1, 1);
    snobo2_draw_sky();
    snobo2_draw_hud();
    if ((unsigned char)D_800DDE64.state != 0x10 && D_80103954.count > 0x100) {
        D_800E194C = snobo2_step_race;
        D_80103954.count = 0;
    }
}

void snobo2_step_race(void)
{
    int i;

    snobo2_save_rider();
    snobo2_pause_menu();
    if ((D_80106B50 = !D_80106A5C))
        snobo2_rider_update();
    D_800E18F4.vx = (D_800DDE64.pos.vx >> 12) - D_80106A60.x;
    D_800E18F4.vy = (D_800DDE64.pos.vy >> 12) - D_80106A60.y;
    D_800E18F4.vz = (D_800DDE64.pos.vz >> 12) - D_80106A60.z;
    snobo2_draw_course(1, D_80106AF8->ahead[D_801069CC] + 1);
    if (D_80106B50)
        snobo2_ease_tilt();
    D_80106A70 = SetSp(0x1F8003FC);
    snobo2_rider_frame(&D_800E18F4, (void *)0x1F800000, D_80106B50, 1);
    SetSp(D_80106A70);
    if (D_80106B50) {
        if (D_801069F8-- < 0 && D_800DDE64.air_time < 4) {
            snobo2_spawn_spray(&D_800DDE64);
            D_801069F8 = 8;
        }
        if (!(D_801053A4 & D_80103984))
            snobo2_draw_board_glow(&D_800DDE64, &D_801069FC, (void *)0x1F800000, D_80106B50);
    }
    if (D_801053A4) {
        snobo2_speed_sound();
        snobo2_record_ghost();
        snobo2_draw_ghost();
    }
    if (D_80106AF8->flags[D_801069CC] & 0x80) {
        D_80105398 = 0;
        D_80103954.count = 0;
        D_80103954.record = D_801053A4 && D_801069C0[i = D_80106A28 - 1] && D_80106AFC.word < D_801069C0[i];
        snobo2_time_finish();
        D_80103954.tick = 0x10;
        D_800E1948 = 1;
        D_800E194C = snobo2_step_finish;
        snd_request.id = 0xD8;
        snd_request.arg[0] = 0;
        snd_submit_request();
        snd_request.id = 0x10;
        snd_request.arg[0] = D_800B8380 + 0x801F8000;
        snd_submit_request();
    }
    snobo2_draw_sky();
    snobo2_draw_hud();
}

/* The pause menu: start opens it, each page offers a menu of 1 + max entries. */
void snobo2_pause_menu(void)
{
    short menu, max;

#ifdef NATIVE
    menu = max = 0; /* the game has no page outside 0-4 */
#endif

    if (!D_80106A5C) {
        if (!(D_801053B0 & 0x800))
            return;
        D_80106A5C = 1;
        if (D_801053A4) {
            D_80103980 = 1;
            D_800E18FC = 1;
        } else {
            D_80103980 = 0;
            D_800E18FC = 0;
        }
        snd_request.id = 0x99;
        snd_submit_request();
        return;
    }
    if (D_801053B0 & 0x1000)
        D_800E18FC--;
    if (D_801053B0 & 0x4000)
        D_800E18FC++;
    switch (D_80103980) {
    case 0:
        menu = 1;
        max = 2;
        break;
    case 1:
        menu = 2;
        max = 3;
        break;
    case 2:
        if (D_801069C0[0] > 0x53FFFF) {
            menu = 3;
            max = 2;
        } else {
            menu = 4;
            max = 3;
        }
        break;
    case 3:
        menu = 5;
        max = 1;
        break;
    case 4:
        menu = 6;
        max = 1;
        break;
    }
    if (D_800E18FC < 0)
        D_800E18FC = 0;
    if (D_800E18FC > max)
        D_800E18FC = max;
    snobo2_draw_menu(menu, D_800E18FC);
    switch (D_80103980) {
    case 0:
    case 1:
        if (!(D_801053B0 & 0x820))
            break;
        if (D_80103980)
            D_800E18FC--;
        switch (D_800E18FC) {
        case 0:
            D_80106A5C = 0;
            break;
        case -1:
            D_80103980 = 4;
            D_800E18FC = 0;
            break;
        case 1:
            D_800E194C = snobo2_step_retry;
            D_80106A5C = 0;
            break;
        case 2:
            D_800E194C = snobo2_step_quit;
            D_80106A5C = 0;
            break;
        }
        break;
    case 2:
        D_800B83E4 = D_800E18FC;
        if (D_801053B0 & 0x820)
            D_80106A5C = 0;
        break;
    case 3:
        D_80103984 = D_800E18FC;
        if (D_801053B0 & 0x820)
            D_80106A5C = 0;
        break;
    case 4:
        if (!(D_801053B0 & 0x820))
            break;
        if (D_800E18FC) {
            D_80103980 = 3;
            D_800E18FC = D_80103984;
        } else {
            D_80103980 = 2;
            D_800E18FC = D_800B83E4;
        }
        break;
    }
    if (D_80106A5C)
        return;
    snd_request.id = 0x98;
    snd_submit_request();
}

void snobo2_step_finish(void)
{
    switch ((unsigned char)D_800DDE64.state) {
    case 1:
    case 2:
    case 0x11:
    case 0x12:
    case 0x16:
        D_800DDE64.state = 0x1C;
        break;
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x19:
        D_800DDE64.state = 0;
        break;
    }
    if (D_80103954.record)
        snobo2_draw_pulse_box_b(D_80103954.tick++);
    D_801053B0 = 0;
    D_801053A0 = 0;
    snobo2_draw_scene();
    D_80103954.count++;
    if (!(unsigned char)D_800DDE64.state) {
        D_80103D04.ang.pad = 0;
        snobo2_score_run();
        D_800E194C = snobo2_step_finish_wait;
    }
}

void snobo2_step_finish_wait(void)
{
    if (D_80103954.record)
        snobo2_draw_pulse_box_b(D_80103954.tick++);
    D_801053B0 = 0;
    D_801053A0 = 0;
    snobo2_draw_scene_still();
    if (D_80103954.count++ > 0xF0) {
        D_80103954.count = 0;
        if (D_801053A4)
            D_80103954.place = snobo2_rank_time();
        else
            snobo2_keep_best();
        D_800E194C = D_801053A4 ? snobo2_step_time : snobo2_step_score;
    }
}

void snobo2_step_score(void)
{
    int s, state;

    snobo2_draw_results((void *)0x1F800000, D_80103954.count, D_80105380 + 1);
    D_801053B0 = 0;
    D_801053A0 = 0;
    snobo2_draw_scene_still();
    state = D_80103954.count++;
    if (state < 0x50)
        return;
    s = D_80105390[3];
    if (s < 100) {
        if (s < 70) {
            if (s < 50) {
                if (s >= 30)
                    state = 0x1D;
                else
                    state = 0x1F;
            } else {
                state = 0;
            }
        } else {
            state = 0x1E;
        }
    } else {
        state = 0x1B;
    }
    D_800DDE64.state = state;
    D_80103954.choice = 0;
    D_80103954.f14 = snobo2_rank();
    D_800E194C = snobo2_step_results;
}

void snobo2_step_time(void)
{
    int s, state;

    snobo2_draw_records(D_80106A28 - 1, D_80103954.place, D_80103954.count);
    if (D_80103954.record && !D_80103954.count)
        snobo2_play_effect(4, 0x1B9, 0x7F);
    D_801053B0 = 0;
    D_801053A0 = 0;
    snobo2_draw_scene_still();
    if (++D_80103954.count < 0x25)
        return;
    if (!D_80103954.record) {
        s = D_80105390[1];
        if (s < 30) {
            if (s >= 15)
                state = 0x1D;
            else
                state = 0x1F;
        } else {
            state = 0;
        }
    } else {
        state = 0x1E;
    }
    snd_request.id = 0xA7;
    D_800DDE64.state = state;
    snd_request.arg[0] = 0x12C;
    snd_request.arg[1] = 0;
    snd_submit_request();
    D_80103954.choice = 0;
    D_80103954.f14 = snobo2_rank();
    D_800E194C = snobo2_step_results;
}

void snobo2_step_results(void)
{
    if (D_80103954.choice) {
        if (D_801053B0 & 0x1000)
            D_80103954.choice = 0;
    } else if (D_801053B0 & 0x4000) {
        D_80103954.choice = 1;
    }
    snobo2_draw_menu(0, D_80103954.choice);
    snobo2_draw_hud_pic(D_80103954.f14);
    if (D_801053A4)
        snobo2_draw_records(D_80106A28 - 1, D_80103954.place, D_80103954.count);
    else
        snobo2_draw_results((void *)0x1F800000, D_80103954.count, D_80105380 + 1);
    D_80103954.held = D_801053A0;
    D_80103954.pressed = D_801053B0;
    D_801053B0 = 0;
    D_801053A0 = 0;
    snobo2_draw_scene_still();
    D_801053B0 = D_80103954.pressed;
    D_801053A0 = D_80103954.held;
    if (D_801053B0 & 0x860) {
        D_80103954.count = 0;
        D_800E194C = D_80103954.choice ? snobo2_step_quit : snobo2_step_retry;
    }
}

void snobo2_step_retry(void)
{
    int n;

    if (!D_80103954.count)
        snobo2_fade_music();
    n = D_80103954.count += 8;
    if (n < 0x100) {
        snobo2_flash_screen(n, D_80105380);
        D_801053B0 = 0;
        D_801053A0 = 0;
        D_80106B50 = 0;
        snobo2_draw_scene();
        return;
    }
    if (D_800E1948 && D_801053A4 && D_800B83A4[0].frame < 0x1BB0 && D_80106AFC.word < D_800B83A4[D_80106A28].time)
        snobo2_keep_ghost();
    snobo2_fill_screen(0xFFFFFF, D_80105380);
    D_800E194C = snobo2_restart_run;
}

void snobo2_step_quit(void)
{
    int n;

    if (!D_80103954.count)
        snobo2_fade_music();
    n = D_80103954.count += 8;
    if (n < 0x100) {
        snobo2_darken_screen(n, D_80105380);
        D_801053B0 = 0;
        D_801053A0 = 0;
        D_80106B50 = 0;
        snobo2_draw_scene();
    } else {
        snobo2_fill_screen(0, D_80105380);
        D_800E1944 = 0;
    }
}

void snobo2_draw_scene(void)
{
    snobo2_save_rider();
    if (D_80106B50)
        snobo2_rider_update();
    D_800E1904.vx = (D_800DDE64.pos.vx >> 12) - D_80106A60.x;
    D_800E1904.vy = (D_800DDE64.pos.vy >> 12) - D_80106A60.y;
    D_800E1904.vz = (D_800DDE64.pos.vz >> 12) - D_80106A60.z;
    snobo2_draw_course(1, D_80106AF8->ahead[D_801069CC] + 1);
    if (D_80106B50)
        snobo2_ease_tilt();
    D_80106A70 = SetSp(0x1F8003FC);
    snobo2_rider_frame(&D_800E1904, (void *)0x1F800000, D_80106B50, 1);
    SetSp(D_80106A70);
    snobo2_draw_sky();
    snobo2_draw_hud();
    if (D_801053A4) {
        snobo2_record_ghost();
        snobo2_draw_ghost();
    }
}

void snobo2_draw_scene_still(void)
{
    if (D_80106B50)
        snobo2_rider_update();
    D_800E1910.vx = (D_800DDE64.pos.vx >> 12) - D_80106A60.x;
    D_800E1910.vy = (D_800DDE64.pos.vy >> 12) - D_80106A60.y;
    D_800E1910.vz = (D_800DDE64.pos.vz >> 12) - D_80106A60.z;
    snobo2_camera_intro((void *)0x1F800000);
    snobo2_draw_course(1, D_80106AF8->ahead[D_801069CC] + 1);
    if (D_80106B50)
        snobo2_ease_tilt();
    D_80106A70 = SetSp(0x1F8003FC);
    snobo2_rider_frame(&D_800E1910, (void *)0x1F800000, D_80106B50, 1);
    SetSp(D_80106A70);
    snobo2_draw_sky();
    snobo2_draw_hud();
    if (D_801053A4)
        snobo2_draw_ghost();
}

void snobo2_rider_update(void)
{
    Piece *s;
    int k, v;

    snobo2_face_normal(D_801069CC, D_801069D0, &D_800DDE64.normal, (void *)0x1F800000);
    snobo2_rider_move(&D_800DDE64, (void *)0x1F800000);
    snobo2_move_along_course(&D_800DDE64.pos, &D_80106B54);
    D_80106A30 = D_801069CC + D_800B835C[D_80106B04];
    s = &D_800B85D8[D_80106AF8->piece[D_801069CC]];
    snobo2_set_origin();
    snobo2_place_rider(0);
    /* heights at the piece's first row are taken from the piece before */
    if (D_801069CC > 0)
        D_800E1930 = &D_800B85D8[D_80106AF8->piece[D_801069CC - 1]].v[42];
    else
        D_800E1930 = s->v;
    for (k = 0; k < 3; k++) {
        /* indexed as rows of bytes (lane * 4 + k), as the game does */
        v = ((unsigned char (*)[4])s->face)[D_801069D0][k];
        D_800E1918[k] = s->v[v];
        if (v < 7)
            D_800E1918[k].vy = D_800E1930[v].vy - D_800E1930[0].vy;
    }
    D_800DDE64.ground_y = (snobo2_plane_height(&D_80106B44, &D_800E1918[0], &D_800E1918[1], &D_800E1918[2]) +
                           D_80106AF8->origin[D_801069CC].y) << 12;
    snobo2_rider_fall(&D_800DDE64);
    if (D_800DDE64.air_time >= 9) {
        D_800DDE64.ground_rot.vz = 0;
        D_800DDE64.ground_rot.vx = 0;
    } else {
        snobo2_normal_to_angles(&D_800DDE64.normal, &D_800DDE64.ground_rot);
    }
}

void snobo2_play_effect(int channel, int sound, int volume)
{
    if (volume) {
        snd_request.id = channel + 0x9F;
        snd_request.arg[0] = volume;
        snd_submit_request();
    }
    snd_request.id = channel + 0x27;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = sound;
    snd_submit_request();
}

void snobo2_start_music(void)
{
    snd_request.id = 0x10;
    snd_request.arg[0] = D_800B8378 + 0x801F8000;
    snd_submit_request();
    snd_request.id = 0xC0;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0xD8;
    snd_request.arg[0] = 0;
    snd_submit_request();
    snd_request.id = 0x23;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0;
    snd_request.arg[2] = 0;
    snd_request.arg[3] = 0;
    snd_request.arg[4] = 0;
    snd_submit_request();
    snd_request.id = 0xB8;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0xBC;
    snd_request.arg[0] = 0;
    snd_submit_request();
}

void snobo2_fade_music(void)
{
    snd_request.id = 0xB9;
    snd_request.arg[0] = 0x1E;
    snd_request.arg[1] = 0;
    snd_submit_request();
    snd_request.id = 0xC1;
    snd_request.arg[0] = 0x1E;
    snd_request.arg[1] = 0;
    snd_submit_request();
}

void snobo2_start_ambience(void)
{
    snd_request.id = 0x10;
    snd_request.arg[0] = D_800B837C + 0x801F8000;
    snd_submit_request();
    snd_request.id = 0xC2;
    snd_request.arg[0] = 0x12C;
    snd_request.arg[1] = 0;
    snd_request.arg[2] = 0x7F;
    snd_submit_request();
}

void snobo2_read_pad(void)
{
    int repeat;

    D_801053A8 = D_801053A0;
    D_801053A0 = func_8001C808(0);
    D_801053B0 = (D_801053A8 ^ D_801053A0) & D_801053A0;
    if (D_801053A8 != D_801053A0)
        D_8010539C = 0;
    repeat = D_801053A0;
    if (D_8010539C && D_8010539C < 0x10)
        repeat = 0;
    D_801053AC = repeat;
    D_8010539C++;
}

void snobo2_load_music(void)
{
    cd_read_lzs(D_800B8260, D_800B8264, (void *)0x801F8000, 0);
    while (cd_step())
        ;
}

void snobo2_load_textures(void)
{
    TIM_IMAGE tim;
    int i;

    cd_read_lzs(D_800B8268, D_800B826C, (void *)0x80120000, 0);
    while (cd_step())
        ;
    for (i = 0; i < 18; i++) {
        OpenTIM((unsigned int *)(D_800B83F0[i] + 0x80120000));
        ReadTIM(&tim);
        func_80044000(tim.prect, tim.paddr);
        func_80044000(tim.crect, tim.caddr);
    }
    DrawSync(0);
}

void snobo2_load_models(void)
{
    cd_read_lzs(D_800B8270, D_800B8274, (void *)0x80178000, 0);
    while (cd_step())
        ;
    snobo2_relocate_model((void *)0x80178000);
}

#ifdef NON_MATCHING
/* The second loop: GCC keeps &D_80106B08[i] in a register where the game rebuilds it from i * 20 + 4. */
void snobo2_load_records(void)
{
    unsigned char *p = D_8009D4F8;
    union {
        unsigned int w;
        unsigned char b[4];
    } u;
    int i, j;

    for (i = 0; i < 3; i++) {
        u.b[0] = p[i * 4];
        u.b[1] = p[i * 4 + 1];
        u.b[2] = p[i * 4 + 2];
        u.b[3] = p[i * 4 + 3];
        D_801069C0[i] = u.w;
        D_80106AF4[i] = p[12 + i];
    }
    for (i = 0; i < 3; i++) {
        D_80106B08[i][0] = D_801069C0[i];
        for (j = 1; j < 5; j++)
            D_80106B08[i][j] = 0;
    }
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_load_records);
#endif

void snobo2_store_records(void)
{
    unsigned char *p = D_8009D4F8;
    int i;

    for (i = 0; i < 3; i++) {
        unsigned int w = D_801069C0[i];

        p[i * 4] = w;
        p[i * 4 + 1] = w >> 8;
        p[i * 4 + 2] = w >> 16;
        p[i * 4 + 3] = w >> 24;
        p[12 + i] = D_80106AF4[i];
    }
}

#ifdef NON_MATCHING
/* GCC keeps &D_801069C0[i] in a register (la + addu); the game keeps i * 4 and indexes the symbol. */
void snobo2_keep_best(void)
{
    int i = D_80106A28 - 1;

    if (!D_801069C0[i] || D_80106AFC.word < D_801069C0[i])
        D_801069C0[i] = D_80106AFC.word;
    if (D_80106AF4[i] < D_80105390[3])
        D_80106AF4[i] = D_80105390[3];
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_keep_best);
#endif

/* The run's place in its course's five best times (5: none), entered there. */
int snobo2_rank_time(void)
{
    unsigned int t = D_80106AFC.word;
    unsigned int *row = D_80106B08[D_80106A28 - 1];
    int i, j;

    for (i = 0; i < 5; i++)
        if (!row[i] || t < row[i])
            break;
    if (i < 5) {
        for (j = 3; j >= i; j--)
            row[j + 1] = row[j];
        row[i] = t;
        if (!i) {
            int c = D_80106A28 - 1;

            D_801069C0[c] = t;
        }
    }
    return i;
}

void snobo2_begin_frame(void)
{
    D_800E1950 ^= 1;
    func_80044244(D_80105380 = D_800FB954[D_800E1950], 0x1000);
    D_8010538C = D_800E1954[D_800E1950];
}

void snobo2_flip(void)
{
    DrawSync(0);
    VSync(D_80106A5C ? 2 : 0);
    PutDrawEnv(&D_800B8278[D_800E1950]);
    func_800444AC(&D_800B8330[D_800E1950]);
    func_8004433C(&D_800FB954[D_800E1950][0xFFF]);
    D_8010398C = 0;
}

void snobo2_save_rider(void)
{
    D_80106B54 = D_800DDE64.pos;
    D_80106B64 = D_801069CC;
    D_80106B66 = D_80106A30;
    D_80106B68 = D_801069D0;
}

void snobo2_restore_rider(void)
{
    D_800DDE64.pos = D_80106B54;
    D_801069CC = D_80106B64;
    D_80106A30 = D_80106B66;
    D_801069D0 = D_80106B68;
}

void snobo2_set_origin(void)
{
    Pos *p = &D_80106AF8->origin[D_801069CC];

    D_80106A60.x = p->x;
    D_80106A60.y = p->y;
    D_80106A60.z = p->z;
}

void snobo2_ease_tilt(void)
{
    int d, step;

    d = D_800DDE64.ground_rot.vz - D_800DDE64.rot.vz;
    if (d) {
        step = (d < 0 ? -d : d) >> 3;
        if (!step)
            step = 1;
        if (d < 0)
            step = -step;
        D_800DDE64.rot.vz += step;
    }
    d = D_800DDE64.ground_rot.vx - D_800DDE64.rot.vx;
    if (d) {
        step = (d < 0 ? -d : d) >> 2;
        if (!step)
            step = 1;
        if (d < 0)
            step = -step;
        D_800DDE64.rot.vx += step;
    }
}

void snobo2_draw_hud(void)
{
    Clock v, was;

    if (D_80106B50) {
        if (D_80105398) {
            D_8010397C = was = D_80106AFC;
            v = snobo2_time_add(was, 0x167);
            D_80106AFC = v;
        }
        if (D_8010537C & 1)
            D_80106B6C = snobo2_bcd_sum_mask(D_800DDE64.speed * 150 / 2048);
    }
    snobo2_draw_time_and_record(D_80106AFC.word, -0x98, -0x60);
    snobo2_draw_number(D_80106B6C, 0x32, -0x60);
}

void snobo2_speed_sound(void)
{
    if (D_8010537C & 0xF)
        return;
    snd_request.id = 0xD0;
    snd_request.arg[0] = D_800DDE64.speed >> 5;
    snd_submit_request();
}

#ifdef NON_MATCHING
/* Scheduling of the two inline rot_y stores (as snobo.c's twin). */
void snobo2_place_rider(int arg)
{
    Piece *s = &D_800B85D8[D_80106AF8->piece[D_801069CC]];

    rot_y(&D_801069D8, D_801053B4 = snobo2_course_heading(&D_80106B44, s) & 0xFFF);
    rot_y(&D_80106A34, D_80106A2C = D_80106AF8->angle[D_801069CC]);
    snobo2_camera_target(&D_80106B44, s, (void *)0x1F800000);
    if (arg)
        snobo2_camera_start_or_follow((void *)0x1F800000, arg);
    else
        snobo2_camera_follow((void *)0x1F800000);
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_place_rider);
#endif

#ifdef NON_MATCHING
/* The frame count is copied through a0 (lh) after the time's load; the game uses v1 (lhu) and stores it before the
 * loop count. */
void snobo2_keep_ghost(void)
{
    Ghost *g = &D_800B83A4[D_80106A28];
    int *dst = (int *)g->rec;
    int *src;
    int n;

    g->on = 1;
    n = D_800B83A4[0].frame;
    g->count = n;
    n = 0x67FF;
    g->time = D_80106AFC.word;
    src = (int *)D_800B83A4[0].rec;
    for (; n != -1; n--)
        *dst++ = *src++;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_keep_ghost);
#endif

/* Records the rider's frame: his position and tilt, big-endian. */
void snobo2_record_ghost(void)
{
    unsigned char *p;
    int n;

    if (!D_80106A28 || !D_80106B50)
        return;
    n = D_800B83A4[0].frame;
    if (n >= 0x1BB0)
        return;
    p = (unsigned char *)D_800B83A4[0].rec + n * 15;
    D_800B83A4[0].frame = n + 1;
    n = D_800DDE64.pos.vx;
    p[0] = n >> 24;
    p[1] = n >> 16;
    p[2] = n >> 8;
    n = D_800DDE64.pos.vy;
    p[3] = n >> 24;
    p[4] = n >> 16;
    p[5] = n >> 8;
    n = D_800DDE64.pos.vz;
    p[6] = n >> 24;
    p[7] = n >> 16;
    p[8] = n >> 8;
    n = D_800DDE64.rot.vx;
    p[9] = n >> 8;
    p[10] = n;
    n = D_800DDE64.facing.vy;
    p[11] = n >> 8;
    p[12] = n;
    n = D_800DDE64.rot.vz;
    p[13] = n >> 8;
    p[14] = n;
}

/* Queue the ghost of a recorded run at its current record. */
#ifdef NON_MATCHING
/* Register choice and every queue push (the game stores D_80105384 + 1 before working out the slot); the game's frame
 * also has 8 bytes it never uses. */
void snobo2_draw_ghost(void)
{
    Scratch *s;
    Ghost *g;
    unsigned char *rec;
    Queued *p;
    int f, x;
    short k;

    if (!D_80106A28)
        return;
    g = &D_800B83A4[D_80106A28];
    if (!g->on)
        return;
    f = g->frame;
    s = SCRATCH;
    if (D_80106B50 && f + 1 < g->count)
        g->frame = ++f;
    if (f < 0 || !D_800B83E4)
        return;
    rec = &g->rec[f * 15];
    s->sv.vy = 0;
    s->sv.vx = rec[10] | rec[9] << 8;
    s->sv.vz = rec[14] | rec[13] << 8;
    RotMatrixYXZ(&s->sv, &s->m);
    rot_y(&s->m2, rec[11] << 8 | rec[12]);
    MulMatrix0(&s->m, &s->m2, &s->m);
    x = rec[0] << 24 | rec[1] << 16 | rec[2] << 8;
    s->m.t[0] = (x >> 12) - D_80106A60.x;
    x = rec[3] << 24 | rec[4] << 16 | rec[5] << 8;
    s->m.t[1] = (x >> 12) - D_80106A60.y;
    x = rec[6] << 24 | rec[7] << 16 | rec[8] << 8;
    s->m.t[2] = (x >> 12) - D_80106A60.z;
    CompMatrix(&D_801069FC, &s->m, &s->m);
    k = D_800B83E4;
    if (k < 3) {
        p = &D_801053B8[D_80105384++];
        p->m = s->m;
        p->how = 0;
        p->model = D_800B8438[k - 1][0];
        p->drawer = 3;
        p->shade = 0;
        p = &D_801053B8[D_80105384++];
        p->how = 2;
        p->model = D_800B8438[k - 1][1];
        p->drawer = 3;
        p->shade = 0;
    } else {
        p = &D_801053B8[D_80105384++];
        p->m = s->m;
        p->how = 0;
        p->model = 0x118;
        p->drawer = 3;
        p->shade = 0;
        p = &D_801053B8[D_80105384++];
        p->how = 2;
        p->model = D_800B8440[(D_8010537C & 6) >> 1];
        p->drawer = 3;
        p->shade = 0;
    }
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_ghost);
#endif

void snobo2_init_run(int seg, int idx, int state)
{
    Piece *s;
    SVECTOR v;
    MATRIX m;

    D_80106AFC.word = 0;
    D_8010397C.word = 0;
    D_801053A4 = 0;
    D_80105398 = 0;
    D_80106B6C = 0;
    D_80106A5C = 0;
    D_801069F8 = 0;
    D_801069D4 = 0;
    D_801069BC = 0;
    D_80105388 = 0;
    D_80106A28 = 0;
    D_80106B04 = seg;
    D_80106AF8 = &D_800C824C[seg];
    D_801069CC = idx;
    s = &D_800B85D8[D_80106AF8->piece[idx]];
    D_800B8384 = D_800B8394;
    D_80103D04.ang.vx = 0;
    D_80103D04.ang.vy = 0x800;
    D_80103D04.ang.vz = 0;
    /* the rider starts in the middle of the slice, turned with the course */
    rot_y(&m, D_80106AF8->angle[idx]);
    v.vx = (s->v[0].pad + s->v[2].pad) >> 1;
    v.vy = (s->v[3].vy + s->v[45].vy) >> 1;
    v.vz = (s->v[3].pad + s->v[5].pad) >> 1;
    ApplyMatrixSV(&m, &v, &v);
    D_80106A60.x = D_80106AF8->origin[idx].x;
    D_80106A60.y = D_80106AF8->origin[idx].y;
    D_80106A60.z = D_80106AF8->origin[idx].z;
    D_800DDE64.pos.vx = (v.vx + D_80106A60.x) << 12;
    D_800DDE64.pos.vy = (v.vy + D_80106A60.y) << 12;
    D_800DDE64.pos.vz = (v.vz + D_80106A60.z) << 12;
    D_800DDE64.vx = 0;
    D_800DDE64.vy = 0;
    D_800DDE64.vz = 0;
    D_800DDE64.facing.vx = 0;
    D_800DDE64.facing.vy = D_80106AF8->angle[idx];
    D_800DDE64.facing.vz = 0;
    D_800DDE64.rot.vx = 0;
    D_800DDE64.ground_rot.vx = 0;
    D_800DDE64.rot.vy = 0;
    D_800DDE64.ground_rot.vy = 0;
    D_800DDE64.rot.vz = 0;
    D_800DDE64.ground_rot.vz = 0;
    D_800DDE64.state = state;
    D_800DDE64.speed = 0;
    v.vx = D_800DDE64.pos.vx >> 12;
    v.vy = D_800DDE64.pos.vy >> 12;
    v.vz = D_800DDE64.pos.vz >> 12;
#ifdef NATIVE
    /* The game passes no locator: the search writes its depth and face wherever a2 points. The PC hands it the
     * scratchpad's. */
    D_801069D0 = snobo2_find_ground_face(&v, s, (Locator *)0x1F800000);
#else
    D_801069D0 = ((int (*)(SVECTOR *, Piece *))snobo2_find_ground_face)(&v, s);
#endif
    snobo2_clear_spray();
    snobo2_clear_triggers();
    snobo2_clear_objs();
    snobo2_rider_reset_sound();
    snobo2_init_hud_mode();
    D_800B83A4[1].frame = -1;
    D_800B83A4[2].frame = -1;
    D_800B83A4[3].frame = -1;
    D_800B83A4[0].frame = 0;
    D_800B83A4[0].count = 0;
    D_801053B4 = 0;
    D_80106A2C = 0;
    D_800E1948 = 0;
    D_80103954.count = 0;
    D_800C824C[2].closed = D_80106AF4[0] < 70;
    D_800C824C[4].closed = D_80106AF4[1] < 70;
}

#ifdef NON_MATCHING
/* As snobo.c's twin: the two idx = 0 are cross-jumped into one, the inner loop's exit test isn't repeated at its top,
 * the fork branch's stores are scheduled differently. */
void snobo2_draw_course(int back, int count)
{
    Work *w = WORK;
    short idx, seg, next, k;

    w->rejoin = 0;
    w->f00 = D_801053A4 & D_80103984;
    w->m = D_801069FC;
    w->left = count;
    w->nmarks = 0;
    w->f32 = D_80106B04;
    w->f34 = D_801069CC;
    w->ot = D_80105380;
    w->prim = D_8010538C;
    w->origin.x = D_80106A60.x;
    w->origin.y = D_80106A60.y;
    w->origin.z = D_80106A60.z;
    /* start BACK points behind the rider, in the stretch before if need be */
    k = D_801069CC - back;
    idx = k;
    seg = D_80106B04;
    if (k < 0) {
        next = D_80106AF8->prev;
        if (next == 0xFF) {
            idx = 0;
        } else {
            idx = k + D_800C824C[next].len;
            seg = next;
            if (idx < 0)
                idx = 0;
        }
    }
    w->idx = idx;
    w->seg = seg;
    w->course = &D_800C824C[w->seg];
    w->point = &w->course->origin[w->idx];
    snobo2_set_slice_matrix(w);
    snobo2_project_first_slice(w);
    do {
        snobo2_draw_slice(w);
        snobo2_add_mark(w);
        w->f03 = 0;
        while (w->left-- > 0) {
            if (w->slice->kind == 3) {
                /* a fork: carry on down the branch, then come back for the rest */
                w->seg = w->course->next[0];
                w->rejoin = w->course->next[1];
                w->idx = 0;
                w->course = &D_800C824C[w->seg];
                w->point = w->course->origin;
                w->f30 = --w->left;
                w->f02 = 1;
            } else {
                if (++w->idx >= w->course->len)
                    break;
                w->f02 = 0;
                w->point++;
            }
            snobo2_set_slice_matrix(w);
            if (w->f02)
                snobo2_project_slice(w, w->sxy2, w->sz2, w->col2);
            else
                snobo2_project_slice(w, &w->sxy[42], &w->sz[42], &w->col[42]);
            snobo2_draw_slice(w);
            snobo2_add_mark(w);
        }
        seg = w->rejoin;
        if (!seg)
            break;
        w->seg = seg;
        w->course = &D_800C824C[w->seg];
        w->rejoin = 0;
        w->idx = 0;
        w->point = w->course->origin;
        w->left = w->f30;
        snobo2_set_slice_matrix(w);
    } while (!snobo2_project_slice(w, &w->sxy2[6], &w->sz2[6], &w->col2[6]));
    D_8010538C = w->prim;
    snobo2_place_props(w);
    snobo2_draw_marks(w);
    snobo2_update_objs(w);
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_course);
#endif

void snobo2_add_mark(Work *w)
{
    Mark *m = &D_80103994[w->nmarks];

    m->seg = w->seg;
    m->idx = w->idx;
    m->slice = w->slice;
    m->m = w->lm;
    w->nmarks++;
}

void snobo2_set_slice_matrix(Work *w)
{
    rot_y(&w->rm, w->course->angle[w->idx]);
    w->rm.t[0] = w->point->x - w->origin.x;
    w->rm.t[1] = w->point->y - w->origin.y;
    w->rm.t[2] = w->point->z - w->origin.z;
    gte_CompMatrix(&w->m, &w->rm, &w->lm);
    gte_SetRotMatrix(&w->lm);
    gte_SetTransMatrix(&w->lm);
    w->slice = &D_800B85D8[w->course->piece[w->idx]];
}

int snobo2_project_slice(Work *w, int *sxy_in, unsigned int *sz_in, Rgb *col_in)
{
    int *sxy = w->sxy;
    unsigned int *sz = w->sz;
    Rgb *col = w->col;
    SVECTOR *v;
    Rgb *c;
    int i;

    /* the first row is the last one drawn */
    i = 7;
    while (i--) {
        *sxy++ = *sxy_in++;
        *sz++ = *sz_in++;
        *col++ = *col_in++;
    }
    v = &w->slice->v[7];
    for (i = 42; i > 0; i -= 3, v += 3, sxy += 3, sz += 3) {
        gte_ldv3(v, v + 1, v + 2);
        gte_rtpt();
        gte_stsxy3(sxy, sxy + 1, sxy + 2);
        gte_stsz3c(sz);
    }
    c = &w->slice->col[7];
    i = 42;
    while (i--)
        *col++ = *c++;
    if (w->slice->kind == 3)
        snobo2_split_last_strip(w);
    return 0;
}

#ifdef NON_MATCHING
/* Register allocation, as snobo.c's twin: the game keeps the masks and counters in s0-s6 and loads every GTE operand
 * through s6. */
void snobo2_draw_slice(Work *w)
{
    int i = 0, k, z;
    Tri t;
    int a, b, c;
    Tri *tri;

    if (!w->f00) {
        if (w->sz[45] < 0x1000) {
            /* near: the shape's own triangles */
            ShadedTri *p = (ShadedTri *)w->prim;
    
            tri = (Tri *)w->slice->face;
            k = 72;
            while (k--) {
                t = *tri++;
                a = t.pt[0];
                b = t.pt[1];
                c = t.pt[2];
                gte_ldsxy3(w->sxy[a], w->sxy[b], w->sxy[c]);
                gte_nclip();
                gte_stopz(&w->opz);
                if (w->opz < 0)
                    continue;
                z = w->sz[a];
                if (z < w->sz[b])
                    z = w->sz[b];
                if (z < w->sz[c])
                    z = w->sz[c];
                z >>= 2;
                if (z < 1 || z > 0xFFF)
                    continue;
                p->tag = 0x06000000;
                p->c0 = w->col[a].word;
                p->xy0 = w->sxy[a];
                p->c1 = w->col[b].word;
                p->xy1 = w->sxy[b];
                p->c2 = w->col[c].word;
                p->xy2 = w->sxy[c];
                addPrim(&w->ot[z], p);
                p++;
            }
            w->prim = (char *)p;
        } else {
            /* far: a quad for each two triangles */
            ShadedQuad *p = (ShadedQuad *)w->prim;
    
            for (i = 0, k = 0; i < 41; i++) {
                gte_ldsxy3(w->sxy[i], w->sxy[i + 1], w->sxy[i + 7]);
                gte_nclip();
                gte_stopz(&w->opz);
                if (w->opz >= 0) {
                    z = w->sz[i];
                    if (z < w->sz[i + 1])
                        z = w->sz[i + 1];
                    if (z < w->sz[i + 7])
                        z = w->sz[i + 7];
                    if (z < w->sz[i + 8])
                        z = w->sz[i + 8];
                    z >>= 2;
                    if (z >= 1 && z <= 0xFFF) {
                        p->tag = 0x08000000;
                        p->c0 = w->col[i].word | 0x08000000;
                        p->xy0 = w->sxy[i];
                        p->c1 = w->col[i + 1].word;
                        p->xy1 = w->sxy[i + 1];
                        p->c2 = w->col[i + 7].word;
                        p->xy2 = w->sxy[i + 7];
                        p->c3 = w->col[i + 8].word;
                        p->xy3 = w->sxy[i + 8];
                        addPrim(&w->ot[z], p);
                        p++;
                    }
                }
                /* the row's last point starts no quad */
                if (++k >= 6) {
                    k = 0;
                    i++;
                }
            }
            w->prim = (char *)p;
        }
    } else {
        /* flat: two flat triangles a quad, in the stretch's two colours by turns */
        FlatTri *p;

        w->f04 = 0;
        w->flat_col[0] = D_800B8448[w->seg][0];
        w->flat_col[1] = D_800B8448[w->seg][1];
        p = (FlatTri *)w->prim;
        for (; i < 42; i++) {
            if (++w->f04 >= 7) {
                w->f04 = 0;
                continue;
            }
            z = w->sz[i];
            if (z < w->sz[i + 1])
                z = w->sz[i + 1];
            if (z < w->sz[i + 7])
                z = w->sz[i + 7];
            if (z < w->sz[i + 8])
                z = w->sz[i + 8];
            z >>= 2;
            if (z < 1 || z > 0xFFF)
                continue;
            gte_ldsxy3(w->sxy[i], w->sxy[i + 1], w->sxy[i + 7]);
            gte_nclip();
            gte_stopz(&w->opz);
            if (w->opz > 0) {
                p->tag = 0x04000000;
                p->c0 = w->flat_col[i & 1];
                p->xy0 = w->sxy[i];
                p->xy1 = w->sxy[i + 1];
                p->xy2 = w->sxy[i + 7];
                addPrim(&w->ot[z], p);
                p++;
            }
            gte_ldsxy3(w->sxy[i + 1], w->sxy[i + 8], w->sxy[i + 7]);
            gte_nclip();
            gte_stopz(&w->opz);
            if (w->opz > 0) {
                p->tag = 0x04000000;
                p->c0 = w->flat_col[i & 1];
                p->xy0 = w->sxy[i + 1];
                p->xy1 = w->sxy[i + 8];
                p->xy2 = w->sxy[i + 7];
                addPrim(&w->ot[z], p);
                p++;
            }
        }
        w->prim = (char *)p;
    }
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_slice);
#endif

void snobo2_project_first_slice(Work *w)
{
    SVECTOR *v = w->slice->v;
    int *sxy = w->sxy;
    unsigned int *sz = w->sz;
    Rgb *col, *src;
    Segment *next;
    int i, n, prev;

    for (i = 49; i > 0; i -= 3, v += 3, sxy += 3, sz += 3) {
        gte_ldv3(v, v + 1, v + 2);
        gte_rtpt();
        gte_stsxy3(sxy, sxy + 1, sxy + 2);
        gte_stsz3c(sz);
    }
    /* the first row's colours are the last row of the slice before */
    prev = w->idx - 1;
    if (prev < 0) {
        if (w->course->prev == 0xFF) {
            src = w->slice->col;
        } else {
            next = &D_800C824C[w->course->prev];
            src = &D_800B85D8[next->piece[next->len - 1]].col[42];
        }
    } else {
        src = &D_800B85D8[w->course->piece[prev]].col[42];
    }
    col = w->col;
    n = 7;
    while (n--)
        *col++ = *src++;
    src = &w->slice->col[7];
    n = 42;
    while (n--)
        *col++ = *src++;
    if (w->slice->kind == 3)
        snobo2_split_last_strip(w);
}

void snobo2_split_last_strip(Work *w)
{
    int p0, p1;
    Sxy pm;
    Rgb c0, c1, cm;
    unsigned int z0, z1;
    int i, j;

    /* the last strip gets a point between each two */
    j = 0;
    for (i = 42; i < 48; i++) {
        p0 = w->sxy[i];
        p1 = w->sxy[i + 1];
        w->sxy2[j] = p0;
        pm.v.vx = ((short)p0 + (short)p1) >> 1;
        pm.v.vy = ((p0 >> 16) + (p1 >> 16)) >> 1;
        w->sxy2[j + 1] = pm.word;
        z0 = w->sz[i];
        z1 = w->sz[i + 1];
        w->sz2[j] = z0;
        w->sz2[j + 1] = (z0 + z1) >> 1;
        c0 = w->col[i];
        c1 = w->col[i + 1];
        cm.c.r = (c0.c.r + c1.c.r) >> 1;
        cm.c.g = (c0.c.g + c1.c.g) >> 1;
        cm.c.b = (c0.c.b + c1.c.b) >> 1;
        cm.c.cd = c0.c.cd;
        w->col2[j] = c0;
        w->col2[j + 1] = cm;
        j += 2;
    }
    w->sxy2[j] = w->sxy[i];
    w->sz2[j] = w->sz[i];
    w->col2[j] = w->col[i];
}

#ifdef NON_MATCHING
/* As snobo.c's twin: the game stores the D_80105384 increment before computing the slot, which frees a register. */
void snobo2_place_props(Work *w)
{
    Mark *m = D_80103994;
    Queued *p;
    SVECTOR *v;
    unsigned int kinds;
    int n, k;

    n = w->nmarks;
    while (--n != -1) {
        w->lm = m->m;
        kinds = D_800C824C[m->seg].props[m->idx];
        if (kinds) {
            p = &D_801053B8[D_80105384++];
            p->m = w->lm;
            p->how = 0;
            p->model = kinds & 0xFF;
            p->drawer = 0;
            p->shade = 0;
            for (kinds >>= 8; kinds; kinds >>= 8) {
                p = &D_801053B8[D_80105384++];
                p->how = 2;
                p->model = kinds & 0xFF;
                p->drawer = 0;
                p->shade = 0;
            }
        } else {
            w->rm.t[0] = w->lm.t[0];
            w->rm.t[1] = w->lm.t[1];
            w->rm.t[2] = w->lm.t[2];
            for (k = 0; k < 6; k++) {
                v = &m->slice->v[D_800B8480[k]];
                w->sv.vx = v->vx;
                w->sv.vy = v->vy;
                w->sv.vz = v->vz;
                gte_SetRotMatrix(&w->lm);
                gte_ldv0(&w->sv);
                gte_rtv0();
                gte_stlvnl(w->lm.t);
                w->lm.t[0] += w->rm.t[0];
                w->lm.t[1] += w->rm.t[1];
                w->lm.t[2] += w->rm.t[2];
                p = &D_801053B8[D_80105384++];
                p->m = w->lm;
                p->how = 0;
                p->model = 0xAF;
                p->drawer = 2;
                p->shade = 0;
            }
        }
        m++;
    }
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_place_props);
#endif

void snobo2_draw_marks(Work *w)
{
    Mark *m = D_80103994;
    short n = w->nmarks;

    while (--n != -1) {
        snobo2_fire_trigger(D_800B835C[m->seg] + m->idx);
        m++;
    }
}

/* Move along the course from `from` to `to` (20.12 fixed point), letting the grid cell of the piece under `to` decide
 * what happens; up to four tries. */
#ifdef NON_MATCHING
/* i and j (the cell's column and row) swap a2/a0. */
void snobo2_move_along_course(VECTOR *from, VECTOR *to)
{
    Locator *r = (Locator *)0x1F800000;
    SVECTOR *v;
    int i, j;

    r->tries = 0;
    r->seg = D_80106B04;
    r->step = D_801069CC;
    r->segp = D_80106AF8;
    r->pos.vx = from->vx >> 12;
    r->pos.vz = from->vz >> 12;
    r->to.vx = to->vx >> 12;
    j = to->vz >> 12;
    r->to.vz = j;
    snobo2_rider_enter_step(r, r->step);
    v = r->piece->v;
    for (i = 0; i < 3; i++)
        if (r->local.vx < v[i].pad)
            break;
    for (j = 0; j < 3; j++)
        if (v[j + 3].pad < r->local.vz)
            break;
    r->moved = D_800B84CC[D_800B848C[r->piece->kind * 16 + j * 4 + i]](r);
    if (r->moved && ++r->tries < 4) {
        *from = *to;
        snobo2_bounce_player(r, from, to);
        return;
    }
    D_80106B44.vx = r->local.vx;
    D_80106B44.vz = r->local.vz;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_move_along_course);
#endif

/* Turn the rider into the frame of piece `step` of its segment. */
inline void snobo2_rider_enter_step(Locator *r, short step)
{
    short *cs;
    short s, c;
    Pos *o;

    r->piece = &D_800B85D8[r->segp->piece[step]];
    cs = &D_8004BC98[(-r->segp->angle[step] & 0xFFF) * 2];
    s = cs[0];
    c = cs[1];
    r->rot.m[0][1] = 0;
    r->rot.m[1][0] = 0;
    r->rot.m[1][2] = 0;
    r->rot.m[2][1] = 0;
    r->rot.m[0][2] = s;
    r->rot.m[1][1] = 0x1000;
    r->rot.m[0][0] = c;
    r->rot.m[2][0] = -s;
    r->rot.m[2][2] = c;
    o = &r->segp->origin[step];
    r->local.vx = r->pos.vx - o->x;
    r->local.vz = r->pos.vz - o->z;
    ApplyMatrixSV(&r->rot, &r->local, &r->local);
}

/* The face of the piece under p (local x, z), searched last to first; -1 if none. */
#ifdef NON_MATCHING
/* The two corner words (s0/s2) are worked out in the other order. */
int snobo2_find_ground_face(SVECTOR *p, Piece *pc, Locator *r)
{
    int i;
    int reach = pc->v[6].pad;
    unsigned int sp = (unsigned short)p->vx << 16 | (unsigned short)p->vz;
    unsigned int s0, s1, s2;
    int d;
    SVECTOR *v;

    for (i = 71; i != -1; i--) {
        d = pc->v[pc->face[i].v[0]].vx - p->vx;
        if (ABS(d) > reach)
            continue;
        d = pc->v[pc->face[i].v[0]].vz - p->vz;
        if (ABS(d) > reach)
            continue;
        d = pc->v[pc->face[i].v[1]].vx - p->vx;
        if (ABS(d) > reach)
            continue;
        d = pc->v[pc->face[i].v[1]].vz - p->vz;
        if (ABS(d) > reach)
            continue;
        d = pc->v[pc->face[i].v[2]].vx - p->vx;
        if (ABS(d) > reach)
            continue;
        d = pc->v[pc->face[i].v[2]].vz - p->vz;
        if (ABS(d) > reach)
            continue;
        r->ground = pc->face[i];
        v = &pc->v[r->ground.v[2]];
        s2 = SXY(v);
        v = &pc->v[r->ground.v[0]];
        s0 = SXY(v);
        gte_ldsxy3(sp, s2, s0);
        gte_nclip();
        gte_stopz(&r->opz);
        if (r->opz < 0)
            continue;
        v = &pc->v[r->ground.v[1]];
        s1 = SXY(v);
        gte_ldsxy3(sp, s0, s1);
        gte_nclip();
        gte_stopz(&r->opz);
        if (r->opz < 0)
            continue;
        gte_ldsxy3(sp, s1, s2);
        gte_nclip();
        gte_stopz(&r->opz);
        if (r->opz >= 0)
            break;
    }
    return i;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_find_ground_face);
#endif

int snobo2_cell_stay(Locator *r)
{
    r->face = snobo2_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0)
        return 1;
    D_801069D0 = r->face;
    D_80106B4C = r->ground.attr;
    return 0;
}

/* Step the rider on to the next piece of its segment; 2 at the segment's end, 1 if it isn't on the ground there. */
int snobo2_rider_step_forward(Locator *r)
{
    short step = ++r->step;

    if (step >= r->segp->len)
        return 2;
    snobo2_rider_enter_step(r, step);
    /* never start behind the piece */
    if (r->local.vz > 0)
        r->local.vz = 0;
    r->face = snobo2_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0)
        return 1;
    D_801069CC = r->step;
    D_801069D0 = r->face;
    D_80106B4C = r->ground.attr;
    return 0;
}

/* Step the rider back to the previous piece, into the previous segment at a segment's start; 1 if there is none or
 * it isn't on the ground there. */
int snobo2_rider_step_back(Locator *r)
{
    short step = --r->step;

    if (step < 0) {
        if (r->segp->prev == 0xFF)
            return 1;
        r->segp = &D_800C824C[r->seg = r->segp->prev];
        snobo2_rider_enter_step(r, r->step = r->segp->len - 1);
        r->face = snobo2_find_ground_face(&r->local, r->piece, r);
        if (r->face < 0)
            return 1;
        D_80106B04 = r->seg;
        D_80106AF8 = r->segp;
    } else {
        snobo2_rider_enter_step(r, step);
        r->face = snobo2_find_ground_face(&r->local, r->piece, r);
        if (r->face < 0)
            return 1;
    }
    D_801069CC = r->step;
    D_801069D0 = r->face;
    D_80106B4C = r->ground.attr;
    return 0;
}

/* As the stay cell, stepping on to the next piece when it has left this one. */
int snobo2_cell_forward(Locator *r)
{
    r->face = snobo2_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0)
        return snobo2_rider_step_forward(r);
    D_801069D0 = r->face;
    D_80106B4C = r->ground.attr;
    return 0;
}

/* Take the left branch, unless it is closed. */
int snobo2_cell_take_left(Locator *r)
{
    if (D_800C824C[r->segp->next[0]].closed)
        return 3;
    r->seg = r->segp->next[0];
    return snobo2_rider_enter_segment(r);
}

/* Put the rider at the start of its segment r->seg; 1 if it isn't on the ground there. */
int snobo2_rider_enter_segment(Locator *r)
{
    short *cs;
    short s, c;
    Pos *o;

    r->segp = &D_800C824C[r->seg];
    r->step = 0;
    r->piece = &D_800B85D8[r->segp->piece[0]];
    cs = &D_8004BC98[(-r->segp->angle[0] & 0xFFF) * 2];
    s = cs[0];
    c = cs[1];
    r->rot.m[1][1] = 0x1000;
    r->rot.m[0][1] = 0;
    r->rot.m[1][0] = 0;
    r->rot.m[1][2] = 0;
    r->rot.m[2][1] = 0;
    r->rot.m[0][2] = s;
    r->rot.m[0][0] = c;
    r->rot.m[2][0] = -s;
    r->rot.m[2][2] = c;
    o = r->segp->origin;
    r->local.vx = r->pos.vx - o->x;
    r->local.vz = r->pos.vz - o->z;
    ApplyMatrixSV(&r->rot, &r->local, &r->local);
    r->face = snobo2_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0)
        return 1;
    D_80106B04 = r->seg;
    D_801069CC = r->step;
    D_801069D0 = r->face;
    D_80106AF8 = r->segp;
    D_80106B4C = r->ground.attr;
    return 0;
}

/* Take the right branch, unless it is closed. */
int snobo2_cell_take_right(Locator *r)
{
    if (D_800C824C[r->segp->next[1]].closed)
        return 4;
    r->seg = r->segp->next[1];
    return snobo2_rider_enter_segment(r);
}

/* As the stay cell, taking the left branch when it has left the piece (3 if that branch is closed). */
int snobo2_cell_left_at_end(Locator *r)
{
    r->face = snobo2_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0) {
        if (D_800C824C[r->segp->next[0]].closed)
            return 3;
        r->seg = r->segp->next[0];
        return snobo2_rider_enter_segment(r);
    }
    D_801069D0 = r->face;
    D_80106B4C = r->ground.attr;
    return 0;
}

/* As the stay cell, taking the right branch when it has right the piece (4 if that branch is closed). */
int snobo2_cell_right_at_end(Locator *r)
{
    r->face = snobo2_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0) {
        if (D_800C824C[r->segp->next[1]].closed)
            return 4;
        r->seg = r->segp->next[1];
        return snobo2_rider_enter_segment(r);
    }
    D_801069D0 = r->face;
    D_80106B4C = r->ground.attr;
    return 0;
}

int snobo2_cell_wall(void)
{
    return 1;
}

/* After a hit on a stretch's walls: bounce the rider off them at speed, else stop him. */
void snobo2_bounce_player(Locator *r, VECTOR *from, VECTOR *to)
{
    int angle;

    if ((unsigned char)D_800DDE64.state == 2 || D_800DDE64.speed < 0x80) {
        D_800DDE64.vx = 0;
        D_800DDE64.vz = 0;
        return;
    }
    angle = snobo2_deflect_velocity(r);
    if (angle > 0x800)
        angle = 0x1000 - angle;
    snobo2_rider_land(&D_800DDE64, angle);
}

/* Turn the rider's velocity off the wall he hit (r->moved: 1 the piece's own, 2-4 D_801053B4's); returns the turn. */
int snobo2_deflect_velocity(Locator *r)
{
    short *cs;
    short s, c;
    int dir, slope;

#ifdef NATIVE
    slope = 0; /* the game turns off walls only (moved 1-4) */
#endif

    r->segp = D_80106AF8;
    r->step = D_801069CC;
    r->piece = &D_800B85D8[r->segp->piece[r->step]];
    dir = ratan2(D_800DDE64.vx, D_800DDE64.vz) & 0xFFF;
    switch (r->moved) {
    case 1:
        slope = r->piece->v[(short)(D_801069D0 / 6) + 8].pad + r->segp->angle[r->step] + 0x800;
        break;
    case 2:
    case 3:
    case 4:
        slope = D_801053B4 + 0x400;
    }
    slope = ((0x800 - (dir - (slope & 0x7FF))) * 2) & 0xFFF;
    cs = &D_8004BC98[slope * 2];
    s = cs[0];
    c = cs[1];
    r->rot.m[0][1] = 0;
    r->rot.m[1][0] = 0;
    r->rot.m[1][1] = 0x1000;
    r->rot.m[1][2] = 0;
    r->rot.m[2][1] = 0;
    r->rot.m[0][2] = s;
    r->rot.m[0][0] = c;
    r->rot.m[2][2] = c;
    r->rot.m[2][0] = -s;
    ApplyMatrixLV(&r->rot, (VECTOR *)&D_800DDE64.vx, (VECTOR *)&D_800DDE64.vx);
    return slope;
}

/* The height at p's x, z of the plane through a, b, c. */
int snobo2_plane_height(SVECTOR *p, SVECTOR *a, SVECTOR *b, SVECTOR *c)
{
    int x = ((a->vy - b->vy) * (a->vz - c->vz) - (a->vy - c->vy) * (a->vz - b->vz)) * (a->vx - p->vx);
    int ny = (a->vz - b->vz) * (a->vx - c->vx) - (a->vz - c->vz) * (a->vx - b->vx);
    int z = ((a->vx - b->vx) * (a->vy - c->vy) - (a->vx - c->vx) * (a->vy - b->vy)) * (a->vz - p->vz);

    if (ny)
        ny = (x + z) / ny;
    return ny + a->vy;
}

/* The course's heading at local point p on piece pc: the step's yaw plus the bend so far (D_80106B00). */
short snobo2_course_heading(SVECTOR *p, Piece *pc)
{
    if (pc->bend > 0)
        D_80106B00 = -ratan2(p->vz, pc->bend + p->vx);
    else if (pc->bend < 0)
        D_80106B00 = ratan2(p->vz, pc->v[6].vx - p->vx - pc->bend);
    else
        D_80106B00 = 0;
    return D_80106AF8->angle[D_801069CC] + D_80106B00;
}

/* The rotation that stands a model up along normal n. */
void snobo2_normal_to_angles(SVECTOR *n, SVECTOR *rot)
{
    rot->vx = ratan2(-n->vz, n->vy);
    rot->vy = 0;
    rot->vz = ratan2(n->vx, n->vy);
}

/* The normal of face `face` of the piece at `step`, turned by D_80106A34 into out (sc->sv is its workspace). */
void snobo2_face_normal(int step, int face, SVECTOR *out, Scratch *sc)
{
    int k;
    Face *f;
    SVECTOR *v, *a, *b, *c;
    int abz, bcy, aby, bcz, bcx, abx;

    if (face < 0)
        return;
    k = D_80106AF8->piece[step];
    f = &D_800B85D8[k].face[face];
    v = D_800B85D8[k].v;
    a = &v[f->v[0]];
    b = &v[f->v[1]];
    c = &v[f->v[2]];
    aby = b->vy - a->vy;
    abz = b->vz - a->vz;
    bcy = c->vy - b->vy;
    bcz = c->vz - b->vz;
    abx = b->vx - a->vx;
    bcx = c->vx - b->vx;
    sc->sv.vx = (abz * bcy - aby * bcz) >> 3;
    sc->sv.vy = (abz * bcx - abx * bcz) >> 3;
    sc->sv.vz = (aby * bcx - abx * bcy) >> 3;
    ApplyMatrixSV(&D_80106A34, &sc->sv, out);
}

/* The camera's look-at point: the player, at a height blended across piece pc between its start and end heights
 * (by p, the player's local position), capped just above the player. */
void snobo2_camera_target(SVECTOR *p, Piece *pc, Scratch *sc)
{
    int bend = pc->bend;
    int y = D_80106AF8->origin[D_801069CC].y;
    SVECTOR *pv = D_800B85D8[D_80106AF8->piece[D_801069CC]].v;
    int lo = ((y + (y + pv[6].vy)) >> 1) - D_80106A60.y;
    int end_r = y + pv[48].vy;
    int end_l = y + pv[42].vy;
    int hi = ((end_l + end_r) >> 1) - D_80106A60.y;
    int turn, top;
    MATRIX *m;
    short *cs;
    short s, c;

    if (bend) {
        turn = D_80106B00;
        cs = &D_8004BC98[(turn & 0xFFF) * 2];
        sc->sv.vy = 0;
        sc->sv.vz = 0;
        s = cs[0];
        c = cs[1];
        sc->m.m[0][1] = 0;
        sc->m.m[1][0] = 0;
        sc->m.m[1][1] = 0x1000;
        sc->m.m[1][2] = 0;
        sc->m.m[2][1] = 0;
        sc->m.m[0][2] = s;
        sc->m.m[0][0] = c;
        sc->m.m[2][2] = c;
        sc->m.m[2][0] = -s;
        m = &sc->m;
        if (bend > 0) {
            sc->sv.vx = bend + pc->v[3].vx;
            ApplyMatrixSV(m, &sc->sv, &sc->sv);
            sc->sv.vx -= bend;
        } else {
            bend = pc->v[3].vx - bend;
            sc->sv.vx = -bend;
            ApplyMatrixSV(m, &sc->sv, &sc->sv);
            sc->sv.vx += bend + pc->v[3].vx;
        }
        lo += (hi - lo) * (D_80106B00 < 0 ? -D_80106B00 : D_80106B00) / pc->turn;
    } else {
        sc->sv.vx = pc->v[3].vx;
        sc->sv.vz = p->vz;
        lo += (hi - lo) * p->vz / pc->v[5].pad;
    }
    top = lo;
    hi = (D_800DDE64.pos.vy >> 12) - D_80106A60.y + 0x40;
    if (hi < top)
        top = hi;
    D_80103D04.at.vy = top;
    D_80103D04.at.vx = (D_800DDE64.pos.vx >> 12) - D_80106A60.x;
    D_80103D04.at.vz = (D_800DDE64.pos.vz >> 12) - D_80106A60.z;
}

/* Point the camera at the player: turn towards its heading or the course's, pitch to the slope, and lift it when
 * the player would sit too high on the screen. */
#ifdef NON_MATCHING
/* The game re-truncates speed >> 4 to a short (sll/sra) where GCC folds it. */
void snobo2_camera_follow(Scratch *sc)
{
    int yaw, d, pitch;
    Segment *seg;
    SVECTOR *ang;

    if (D_801069CC < 0)
        return;
    ang = &D_80103D04.ang;
    switch (D_800B84F0[(unsigned char)D_800DDE64.state]) {
    case 1:
        if (D_800DDE64.speed > 0x80) {
            yaw = ratan2(-D_800DDE64.vx, -D_800DDE64.vz) & 0xFFF;
            goto turn;
        }
    case 2:
        yaw = D_801053B4;
    turn:
        d = (yaw - D_801053B4) & 0xFFF;
        if (d > 0x400 && d < 0xC00)
            yaw = (yaw + 0x800) & 0xFFF;
        d = (yaw - D_801053B4) & 0xFFF;
        if (d > 0x200 && d < 0x800)
            yaw = D_801053B4 + 0x200;
        else if (d > 0x800 && d < 0xE00)
            yaw = D_801053B4 - 0x200;
        d = (short)(D_800DDE64.speed >> 4);
        yaw = (0x800 - yaw) & 0xFFF;
        if (d < 4)
            d = 4;
        ang->vy = snobo2_angle_step(yaw, ang->vy, d);
    }
    seg = D_80106AF8;
    if (seg->flags[D_801069CC] & 8)
        pitch = 0x20;
    else if (D_800DDE64.air_time > 8)
        pitch = 0x80;
    else
        pitch = D_800B85D8[seg->piece[D_801069CC]].pitch * 3;
    pitch += 0x40;
    pitch -= ang->vx;
    if (pitch)
        ang->vx += pitch > 0 ? 4 : -4;
    ang->vz = 0;
    RotMatrix(&D_80103D04.ang, &D_801069FC);
    ApplyMatrixSV(&D_801069FC, &D_80103D04.at, &sc->sv);
    D_801069FC.t[0] = -sc->sv.vx;
    D_801069FC.t[1] = 0x80 - sc->sv.vy;
    D_801069FC.t[2] = 0x140 - sc->sv.vz;
    sc->sv.vx = (D_800DDE64.pos.vx >> 12) - D_80106A60.x;
    sc->sv.vy = (D_800DDE64.pos.vy >> 12) - D_80106A60.y;
    sc->sv.vz = (D_800DDE64.pos.vz >> 12) - D_80106A60.z;
    SetGeomOffset(0, 0);
    SetGeomScreen(0xA0);
    SetRotMatrix(&D_801069FC);
    SetTransMatrix(&D_801069FC);
    gte_ldv0(&sc->sv);
    gte_rtps();
    gte_stsxy(&sc->u.sxy);
    if (sc->u.sxy.vy < 0)
        pitch = -sc->u.sxy.vy;
    else if (sc->u.sxy.vy > 0x40)
        pitch = (0x40 - sc->u.sxy.vy) * 2;
    else
        pitch = 0;
    D_801069FC.t[1] += pitch;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_camera_follow);
#endif

/* The camera before the start (fixed 1: the start view, held level while button 0 is down) or while riding: as
 * snobo2_camera_follow, around the start angles and offset D_800B8384. */
#ifdef NON_MATCHING
/* Frame 56 vs 40 (stack slots for the short locals), lhu vs lh for the heading, the yaw block's registers. */
void snobo2_camera_start_or_follow(Scratch *sc, int fixed)
{
    SVECTOR *ang;
    Camera *base;
    int d, speed;
    unsigned short yaw;
    short pitch;
    short lift;

    if (D_801069CC < 0)
        return;
    ang = &D_80103D04.ang;
    base = &D_800B8384;
    if (fixed == 1)
        D_80103D04.ang.vy = snobo2_angle_step((D_800B8384.ang.vy - D_801053B4) & 0xFFF, D_80103D04.ang.vy, 0x40);
    else {
        switch (D_800B84F0[(unsigned char)D_800DDE64.state]) {
        case 1:
            if (D_800DDE64.speed > 0x80) {
                yaw = ratan2(-D_800DDE64.vx, -D_800DDE64.vz) & 0xFFF;
                goto turn;
            }
        case 2:
            yaw = D_801053B4;
        turn:
            d = (yaw - D_801053B4) & 0xFFF;
            if (d > 0x400 && d < 0xC00)
                yaw = (yaw + 0x800) & 0xFFF;
            d = (yaw - D_801053B4) & 0xFFF;
            if (d > 0x200 && d < 0x800)
                yaw = D_801053B4 + 0x200;
            else if (d > 0x800 && d < 0xE00)
                yaw = D_801053B4 - 0x200;
            speed = D_800DDE64.speed >> 4;
            if (speed < 4)
                speed = 4;
            ang->vy = snobo2_angle_step((0x800 - yaw) & 0xFFF, ang->vy, (short)speed);
        }
    }
    if (D_80106AF8->flags[D_801069CC] & 8)
        pitch = 0x20;
    else if (D_800DDE64.air_time > 8)
        pitch = 0x80;
    else
        pitch = D_800B85D8[D_80106AF8->piece[D_801069CC]].pitch * 3;
    pitch += base->ang.vx;
    if (fixed == 1 && D_801053A0 & 1)
        ang->vx = pitch;
    else {
        d = (short)(pitch - ang->vx);
        if (d) {
            if (d > 0)
                ang->vx += 4;
            else
                ang->vx -= 4;
        }
    }
    ang->vz = base->ang.vz;
    RotMatrix(&D_80103D04.ang, &sc->m);
    ApplyMatrixSV(&sc->m, &D_80103D04.at, &sc->sv);
    sc->m.t[0] = base->at.vx - sc->sv.vx;
    sc->m.t[1] = base->at.vy - sc->sv.vy;
    sc->m.t[2] = base->at.vz - sc->sv.vz;
    sc->sv.vx = (D_800DDE64.pos.vx >> 12) - D_80106A60.x;
    sc->sv.vy = (D_800DDE64.pos.vy >> 12) - D_80106A60.y;
    sc->sv.vz = (D_800DDE64.pos.vz >> 12) - D_80106A60.z;
    SetGeomOffset(0, 0);
    SetGeomScreen(0xA0);
    SetRotMatrix(&sc->m);
    SetTransMatrix(&sc->m);
    gte_ldv0(&sc->sv);
    gte_rtps();
    gte_stsxy(&sc->u.sxy);
    lift = sc->u.sxy.vy;
    if (lift < 0)
        lift = -lift;
    else if (lift > 0x40)
        lift = (0x40 - lift) * 2;
    else
        lift = 0;
    sc->m.t[1] += lift;
    D_801069FC = sc->m;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_camera_start_or_follow);
#endif

/* The camera at the start of the run: the fixed start angles and offset around the player. */
void snobo2_camera_start(Scratch *sc)
{
    unsigned short yaw = D_80106A2C;

    D_80103D04.at.vx = (D_800DDE64.pos.vx >> 12) - D_80106A60.x;
    D_80103D04.at.vy = (D_800DDE64.pos.vy >> 12) - D_80106A60.y;
    D_80103D04.at.vz = (D_800DDE64.pos.vz >> 12) - D_80106A60.z;
    D_80103D04.ang.vx = D_800B8384.ang.vx;
    D_80103D04.ang.vy = (D_800B8384.ang.vy - yaw) & 0xFFF;
    D_80103D04.ang.vz = D_800B8384.ang.vz;
    RotMatrix(&D_80103D04.ang, &sc->m);
    ApplyMatrixSV(&sc->m, &D_80103D04.at, &sc->sv);
    sc->m.t[0] = D_800B8384.at.vx - sc->sv.vx;
    sc->m.t[1] = D_800B8384.at.vy - sc->sv.vy;
    sc->m.t[2] = D_800B8384.at.vz - sc->sv.vz;
    D_801069FC = sc->m;
}

/* The intro's camera: swing round from the start yaw ever faster and close in on the start offset, then place it. */
void snobo2_camera_intro(Scratch *sc)
{
    short n = D_800B8384.ang.pad;

    if (n < 0x40)
        D_800B8384.ang.pad = ++n;
    D_800B8384.ang.vy -= n >> 4;
    if (D_800B8384.at.vy > 0x30)
        D_800B8384.at.vy--;
    if (D_800B8384.at.vz > 0x100)
        D_800B8384.at.vz--;
    snobo2_camera_start(sc);
}

/* Draw three lines (D_800B8510, D_800B8540) through m, all into one ordering table entry. */
#ifdef NON_MATCHING
/* The loop counter's clear lands two instructions early (as snobo.c's twin). */
void snobo2_draw_lines(MATRIX *m)
{
    SVECTOR sv;
    int otz;
    LINE_F2 *p;
    SVECTOR *e;
    int i;

    gte_SetRotMatrix(m);
    gte_SetTransMatrix(m);
    p = (LINE_F2 *)D_8010538C;
    e = D_800B8510;
    for (i = 0; i < 12;) {
        setlen(p, 3);
        setcode(p, 0x40);
        p->r0 = D_800B8540[i];
        p->g0 = D_800B8540[i + 1];
        p->b0 = D_800B8540[i + 2];
        sv.vx = e->vx;
        sv.vy = e->vy;
        sv.vz = e->vz;
        e++;
        gte_ldv0(&sv);
        gte_rtps();
        gte_stsxy(&p->x0);
        gte_stszotz4(&otz);
        sv.vx = e->vx;
        sv.vy = e->vy;
        sv.vz = e->vz;
        e++;
        gte_ldv0(&sv);
        gte_rtps();
        gte_stsxy(&p->x1);
        gte_stszotz4(&otz);
        i += 4;
        addPrim(D_80105380, p);
        p++;
    }
    D_8010538C = (unsigned int *)p;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_lines);
#endif

/* Draw a blue-white triangle tri[0-2] on the course at the current step (into the second ordering table entry). */
void snobo2_draw_marker_triangle(Scratch *sc, SVECTOR *tri)
{
    short *cs;
    short s, c;
    Pos *o;
    POLY_G3 *p;
    int yaw;
    MATRIX *m;

    sc->u.step = D_801069CC;
    yaw = D_80106AF8->angle[sc->u.step];
    cs = &D_8004BC98[(yaw & 0xFFF) * 2];
    s = cs[0];
    c = cs[1];
    sc->m.m[1][1] = 0x1000;
    sc->m.m[0][1] = 0;
    sc->m.m[1][0] = 0;
    sc->m.m[1][2] = 0;
    sc->m.m[2][1] = 0;
    sc->m.m[0][2] = s;
    sc->m.m[2][0] = -s;
    sc->m.m[0][0] = c;
    sc->m.m[2][2] = c;
    o = &D_80106AF8->origin[sc->u.step];
    sc->m.t[0] = o->x - D_80106A60.x;
    sc->m.t[1] = o->y - D_80106A60.y;
    sc->m.t[2] = o->z - D_80106A60.z;
    m = &sc->m;
    gte_CompMatrix(&D_801069FC, m, m);
    gte_SetRotMatrix(m);
    gte_SetTransMatrix(m);
    p = (POLY_G3 *)D_8010538C;
    *(unsigned int *)&p->tag = 0x06000000;
    *(unsigned int *)&p->r0 = 0x30F08080;
    *(unsigned int *)&p->r1 = 0x0080F080;
    *(unsigned int *)&p->r2 = 0x008080F0;
    gte_ldv3(&tri[0], &tri[1], &tri[2]);
    gte_rtpt();
    gte_stsxy3(&p->x0, &p->x1, &p->x2);
    AddPrim(D_80105380 + 1, p);
    D_8010538C = (unsigned int *)(p + 1);
}

/* The sky, in the farthest ordering table entry: one flat colour where the course is covered (flag 2), else three
 * gradient bands moved by the camera's pitch and a flat colour above them. */
#ifdef NON_MATCHING
/* Two loop pointer inits (band + 2, p + 32) are swapped around the pitch load. */
void snobo2_draw_sky(void)
{
    unsigned int *p = D_8010538C;
    unsigned int *ot = D_80105380 + 0xFFF;
    int y, row, i;
    unsigned int col;
    SkyBand *band;

    if (D_80106AF8->flags[D_801069CC] & 2) {
        p[0] = 0x03000000;
        p[1] = 0x60F0A050;
        p[2] = 0xFF88FF60;
        p[3] = 0x00F00140;
        addPrim(ot, p);
        D_8010538C = p + 4;
        return;
    }
    y = -((short)D_80103D04.ang.vx >> 2) + 0x20;
    band = D_800B854C;
    for (i = 2; i != -1; i--) {
        p[0] = 0x08000000;
        p[1] = p[3] = band->top;
        p[5] = p[7] = band->bottom;
        row = y + band->y;
        p[2] = row << 16 | 0xFF60;
        p[4] = row << 16 | 0xA0;
        col = (row + band->h) << 16;
        p[6] = col | 0xFF60;
        p[8] = col | 0xA0;
        band++;
        addPrim(ot, p);
        p += 9;
    }
    if (row >= -0x77) {
        p[0] = 0x03000000;
        p[1] = 0x60D75C0F;
        p[2] = 0xFF88FF60;
        p[3] = -(row + 0x78) << 16 | 0x140;
        addPrim(ot, p);
        p += 4;
    }
    D_8010538C = p;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_sky);
#endif

/* Darken the whole screen by `level` (a subtractive grey TILE). */
void snobo2_darken_screen(int level, void *ot)
{
    unsigned int *p;

    level |= level << 16 | level << 8;
    p = D_8010538C;

    p[0] = 0x03000000;
    p[1] = level | 0x62000000;
    p[2] = 0xFF88FF60;
    p[3] = 0x00F00140;
    AddPrim(ot, p);
    p += 4;
    func_80044A68((DR_MODE *)p, 1, 0, getTPage(0, 2, 0, 0), 0);
    AddPrim(ot, p);
    D_8010538C = p + 3;
}

/* Brighten the whole screen by `level` (an additive grey TILE). */
void snobo2_flash_screen(int level, void *ot)
{
    unsigned int *p;

    level |= level << 16 | level << 8;
    p = D_8010538C;

    p[0] = 0x03000000;
    p[1] = level | 0x62000000;
    p[2] = 0xFF88FF60;
    p[3] = 0x00F00140;
    AddPrim(ot, p);
    p += 4;
    func_80044A68((DR_MODE *)p, 1, 0, getTPage(0, 1, 0, 0), 0);
    AddPrim(ot, p);
    D_8010538C = p + 3;
}

/* Fill the screen with colour rgb (a TILE). */
void snobo2_fill_screen(unsigned int rgb, void *ot)
{
    unsigned int *p = D_8010538C;

    p[0] = 0x03000000;
    p[1] = (rgb & 0xFFFFFF) | 0x61000000;
    p[2] = 0xFF88FF60;
    p[3] = 0x00F00140;
    AddPrim(ot, p);
    D_8010538C = p + 4;
}

/* The end of the run: score the three parts (crashes, the time against the course's par, pickups) and total them. */
#ifdef NON_MATCHING
/* The first part's value lands in v1; the game uses a0. */
void snobo2_score_run(void)
{
    int n, c;

    c = 30 - (short)(D_801069D4 / 3);
    if (c < 0)
        c = 0;
    D_80105390[0] = c;
    n = 30 - (((int)((D_80106AFC.word >> 16 & 0xF) + (D_80106AFC.word >> 20 & 0xF) * 10 +
                     (D_80106AFC.word >> 24 & 0xF) * 60 + (D_80106AFC.word >> 28) * 3600) -
               D_800B856E[D_80106A28]) >> 1);
    if (n > 30)
        n = 30;
    else if (n < 0)
        n = 0;
    c = (short)D_801069BC;
    D_80105390[1] = n;
    if (c > 40)
        c = 40;
    D_80105390[2] = c;
    D_80105390[3] = D_80105390[0] + n + c;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_score_run);
#endif

/* The run's time at the line: last frame's clock plus the share of this frame (60ths of a second, in
 * ten-thousandths) before the rider crossed it, measured along the piece. */
void snobo2_time_finish(void)
{
    FinishWork *w = FINISH;
    int d;

    w->loc.seg = D_80106B04;
    w->loc.step = D_801069CC;
    w->loc.segp = D_80106AF8;
    w->loc.pos.vx = D_80106B54.vx >> 12;
    w->loc.pos.vz = D_80106B54.vz >> 12;
    finish_enter_step(&w->loc, w->loc.step);
    w->before = w->loc.local.vz;
    w->loc.pos.vx = D_800DDE64.pos.vx >> 12;
    w->loc.pos.vz = D_800DDE64.pos.vz >> 12;
    finish_enter_step(&w->loc, w->loc.step);
    d = w->loc.local.vz;
    w->after = d;
    d = w->before - d;
    if (d < 0)
        d = -d;
    w->dist = d;
    if (d) {
        w->before = snobo2_bcd_sum_mask(w->before * 167 / d);
        D_80106AFC = snobo2_time_add(D_8010397C, w->before);
    }
    D_80106AFC.word &= ~0xF;
}

int snobo2_rank(void)
{
    unsigned int *p;
    unsigned int v;
    int i;

    if (!D_801053A4) {
        v = D_80105390[3];
        if (v >= 100)
            return 8;
        if (v >= 70)
            return 9;
        if (v >= 30)
            return 10;
        return 11;
    }
    v = D_80106AFC.word;
    p = D_800B8558[D_80106A28];
    for (i = 0; v >= *p; i++)
        p++;
    return i;
}

void snobo2_restart_run(void)
{
    snobo2_start_music();
    snobo2_start_ambience();
    snobo2_init_run(0, 0, 0x10);
    snobo2_fill_screen(0xFFFFFF, D_80105380);
    D_800E194C = snobo2_step_intro;
}

void snobo2_run_events(void)
{
    int i;
    unsigned char *op;

    goto search; /* the game has the fired slot's handling above the search */
found:
    D_80103E40 = D_80103E4C[i];
    D_80103E48[i] = 0;
    D_80103E44[i] = 0;
    D_800D1D8E = 0;
    goto run;
search:
    for (i = 0; i < 4; i++)
        if (D_80103E44[i] && D_80103E48[i])
            goto found;
run:
    if (!D_800D1D8E) {
        while (1) {
            op = D_80103E5C->script[(unsigned char)D_800DDE64.state * 64 + D_80103E40];
            if (*op >= 14)
                break;
            if (D_800DDF1C[*op](op))
                break;
        }
    }
    snobo2_pose_blend_step();
}

int snobo2_op_yield(void)
{
    return 1;
}

int snobo2_op_pose(unsigned char *op)
{
    D_800D1D90 = op[1];
    snobo2_pose_blend_start(op[2], op[3]);
    D_80103E40++;
    return 1;
}

int snobo2_op_loop(unsigned char *op)
{
    if (D_80103E3C >= 3)
        return 1;
    D_80103E3C++;
    D_80103E34[D_80103E3C] = D_80103E40 + 1;
    D_80103E38[D_80103E3C] = op[1];
    D_80103E40++;
    return 0;
}

int snobo2_op_endloop(unsigned char *op)
{
    if (D_80103E3C <= 0)
        return 1;
    if (D_80103E38[D_80103E3C]) {
        D_80103E38[D_80103E3C]--;
        D_80103E40 = D_80103E34[D_80103E3C];
    } else {
        D_80103E3C--;
        D_80103E40++;
    }
    return 0;
}

int snobo2_op_goto(unsigned char *op)
{
    D_80103E40 = op[1];
    return 0;
}

int snobo2_op_arm_trigger(unsigned char *op)
{
    D_80103E44[op[1]] = 1;
    D_80103E48[op[1]] = 0;
    D_80103E4C[op[1]] = op[2];
    D_80103E40++;
    return 0;
}

int snobo2_op_yield2(void)
{
    return 1;
}

int snobo2_op_set_mode(unsigned char *op)
{
    D_800DDE64.state = op[1];
    return 1;
}

int snobo2_op_push(unsigned char *op)
{
    int n = op[1];
    unsigned short pc = D_80103E40 + 1;
    D_800DDE64.bounce = 0x1000;
    D_800DDE64.air_time = 1;
    D_800DDE64.vy = -n << 9;
    D_80103E40 = pc;
    return 0;
}

int snobo2_op_wait_button(unsigned char *op)
{
    unsigned short pad = D_801053A0;
    if (op[2])
        pad = ~pad;
    if (pad & D_800DDF54[op[1]]) {
        D_80103E40++;
        return 0;
    }
    return 1;
}

int snobo2_op_switch(unsigned char *op)
{
    if (D_80103E50)
        D_80103E40 = op[D_80103E50];
    else
        D_80103E40++;
    return 0;
}

int snobo2_op_disarm_trigger(unsigned char *op)
{
    D_80103E44[op[1]] = 0;
    D_80103E40++;
    return 0;
}

int snobo2_op_set_cue(unsigned char *op)
{
    unsigned char v = op[1];
    D_80103E40++;
    D_80103E54 = v;
    return 0;
}

int snobo2_op_sound(unsigned char *op)
{
    snd_request.id = op[1] + 0x27;
    snd_request.arg[0] = op[3];
    snd_request.arg[1] = D_800DDF64[op[2]];
    snd_submit_request();
    D_80103E40++;
    return 0;
}

void snobo2_pose_blend_start(int frames, int ease)
{
    int i;
    int *dst, *src;
    short *root;

    snobo2_copy_pose((int *)D_80103D24, (int *)D_80106A74);
    src = D_80103E5C->poses + D_800D1D90 * D_80103E5C->npose;
    dst = (int *)D_80103DA4;
    i = D_80103E5C->npose;
    while (i--)
        *dst++ = *src++;
    D_80103D14 = D_80106A54;
    root = D_80103E5C->roots[D_800D1D90];
    D_80103D1C[0] = root[0];
    D_80103D1C[1] = root[1];
    D_80103D1C[2] = root[2];
    D_80103E28 = frames;
    D_800D1D8E = frames;
    D_80103E24 = D_800D1D94[ease];
}

void snobo2_copy_pose(int *dst, int *src)
{
    int i;

    i = D_80103E5C->npose;
    while (i--)
        *dst++ = *src++;
}

void snobo2_pose_blend_step(void)
{
    int t, i;
    short *from, *to, *dst;

    if (!D_800D1D8E)
        return;
    D_800D1D8E--;
    t = D_80103E24(((D_80103E28 - D_800D1D8E) << 12) / D_80103E28);
    if (t < 0)
        t = 0;
    else if (t > 0x1000)
        t = 0x1000;
    from = D_80103D24;
    to = D_80103DA4;
    dst = D_80106A74;
    i = D_80103E5C->nangle;
    while (i--) {
        *dst++ = *from + ((*to - *from) * t >> 12);
        from++;
        to++;
    }
    from = (short *)&D_80103D14;
    to = D_80103D1C;
    dst = (short *)&D_80106A54;
    for (i = 2; i != -1; i--) {
        *dst++ = *from + ((*to - *from) * t >> 12);
        from++;
        to++;
    }
}

int snobo2_ease_linear(int a)
{
    return a;
}

int snobo2_ease_in_out(int a)
{
    return (rsin((a >> 2) * 2 - 0x400) + 0x1000) >> 1;
}

int snobo2_ease_in(int a)
{
    return 0x1000 - rsin((a >> 2) + 0x400);
}

int snobo2_ease_out(int a)
{
    return -rsin((a >> 2) + 0x800);
}

void snobo2_rider_reset_sound(void)
{
    D_800D1D8E = 0;
    D_80103E2C = 0;
    D_80103E30 = 0x7F;
}

void snobo2_fire_script_trigger(int slot)
{
    D_80103E48[slot] = 1;
}

#ifdef NON_MATCHING
/* The game zeroes state_time by its own address and keeps &state in a register; GCC reaches state through &state_time
 * (as snobo.c's twin). */
void snobo2_rider_frame(SVECTOR *pos, Scratch *s, int update, int draw)
{
    unsigned short *st; /* set at each use: the game builds &state there */
    int i, enter;
    short *sc;
    short sn, c;

    D_80103E5C = &D_800DDDBC[D_801069B8];
    if (update) {
        enter = 0;
        if (!((short)D_800DDE64.state & 0x8000)) {
            D_80103E40 = 0;
            D_80103E3C = 0;
            D_80103E50 = 0;
            D_80103E54 = 0;
            D_800D1D8E = 0;
            D_800DDE64.flags = 0;
            for (i = 0; i < 4; i++) {
                D_80103E44[i] = 0;
                D_80103E48[i] = 0;
            }
            enter = 1;
            D_800DDE64.state_time = 0;
            st = &D_800DDE64.state;
            *st |= 0x8000;
        }
        st = &D_800DDE64.state;
        D_800DDF74[*st & 0xFF](&D_800DDE64, enter, s);
        D_800DDE64.state_time++;
        snobo2_board_sound(&D_800DDE64);
    }
    RotMatrixYXZ(&D_800DDE64.rot, &s->m);
    sc = &D_8004BC98[(D_800DDE64.facing.vy & 0xFFF) * 2];
    sn = sc[0];
    c = sc[1];
    s->m2.m[0][1] = 0;
    s->m2.m[1][0] = 0;
    s->m2.m[1][1] = 0x1000;
    s->m2.m[1][2] = 0;
    s->m2.m[2][1] = 0;
    s->m2.m[0][2] = sn;
    s->m2.m[0][0] = c;
    s->m2.m[2][0] = -sn;
    s->m2.m[2][2] = c;
    MulMatrix0(&s->m, &s->m2, &s->m);
    MulMatrix0(&s->m, &D_800D1DA4, &D_800DDE64.mat);
    D_800DDE64.mat.t[0] = pos->vx;
    D_800DDE64.mat.t[1] = pos->vy;
    D_800DDE64.mat.t[2] = pos->vz;
    snobo2_pose_skeleton(&D_800DDE64.mat, &D_801069FC);
    if (draw)
        snobo2_rider_shadow(s);
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_rider_frame);
#endif

void snobo2_rider_shadow(Scratch *s)
{
    short h = (D_800DDE64.ground_y - D_800DDE64.pos.vy) >> 12;
    short *sc;
    short sn, c;
    Queued *d;

    if (D_800D1DB6) {
        if (h <= 0)
            D_800D1DB6 = 0;
    } else if (D_800DDFF4[D_800DDE64.state & 0xFF] && D_800DDFF4[D_800DDE64.state & 0xFF] < h) {
        D_800D1DB6 = 1;
    }
    if (!D_800D1DB6)
        return;
    RotMatrixYXZ(&D_800DDE64.ground_rot, &s->m);
    sc = &D_8004BC98[(D_800DDE64.facing.vy & 0xFFF) * 2];
    sn = sc[0];
    c = sc[1];
    s->m.m[0][1] = 0;
    s->m.m[1][0] = 0;
    s->m.m[1][1] = 0x1000;
    s->m.m[1][2] = 0;
    s->m.m[2][1] = 0;
    s->m.m[0][2] = sn;
    s->m.m[0][0] = c;
    s->m.m[2][0] = -sn;
    s->m.m[2][2] = c;
    s->m.t[0] = D_800DDE64.mat.t[0];
    s->m.t[1] = (D_800DDE64.ground_y >> 12) - D_80106A60.y;
    s->m.t[2] = D_800DDE64.mat.t[2];
    gte_CompMatrix(&D_801069FC, &s->m, &s->m);
    d = &D_801053B8[D_80105384++];
    d->m = s->m;
    d->model = 0xAD;
    d->drawer = 3;
    d->how = 0;
    d->shade = 2;
}

void snobo2_state_script(Board *b, int enter, Scratch *s)
{
    if (enter) {
        D_800DDE64.f90 = 0;
        D_800DDE64.f8E = 0;
        D_800DDE64.f8C = 0;
    }
    snobo2_run_events();
}

void snobo2_state_ride(Board *b, int enter, Scratch *s)
{
    b->rot.vy = 0;
    b->normal.pad = 0;
    D_80103E50 = b->speed < 0x100;
    snobo2_run_events();
    if (D_801053B0 & 0x40 && b->air_time < 8) {
        b->state = snobo2_jump_state(b);
    } else if (D_801053A0 & 0x8000) {
        if (b->speed <= 0x20)
            b->facing.vy -= 0x20;
        else
            b->state = 0x11;
    } else if (D_801053A0 & 0x2000) {
        if (b->speed <= 0x20)
            b->facing.vy += 0x20;
        else
            b->state = 0x12;
    } else if (D_801053A0 & 0x4080 && b->speed > 0x100) {
        b->state = 0x16;
    } else if (b->speed < 0x100 && D_801053A0 & 0x1000) {
        b->state = 2;
    }
    snobo2_face_motion(b);
}

void snobo2_state_carve_left(Board *b, int enter, Scratch *s)
{
    int i, max, step;
    int *dst, *src;

    if (enter) {
        b->lean = 0;
        b->carve_time = 0;
        b->normal.pad = 0;
        dst = (int *)D_80103D24;
        src = (int *)D_80103E5C->key[0].pose;
        i = D_80103E5C->npose;
        while (i--)
            *dst++ = *src++;
        D_80103D14 = D_80106A54;
    }
    if (D_801053A0 & 0x8000)
        max = D_801053A0 & 0xC ? 0x80 : 0x20;
    else
        max = 0;
    step = D_801053A0 & 0xC ? 2 : 1;
    if (!max)
        step <<= 1;
    if (b->lean < max) {
        b->lean += step;
        if (b->lean > max)
            b->lean = max;
    } else if (b->lean > max) {
        b->lean -= step;
        if (b->lean < 0)
            b->lean = 0;
    }
    max = b->lean;
    if (max > 0x20) {
        b->rot.vy = (0x20 - max) << 3;
        max = 0x20;
    } else {
        b->rot.vy = 0;
    }
    snobo2_pose_lerp(D_80103D24, D_80103E5C->key[2].pose, (short *)&D_80103D14, D_80103E5C->key[2].root, max << 7);
    if (b->lean > 0x20)
        b->carve_time += b->lean >> 5;
    else if (b->carve_time)
        b->carve_time--;
    snobo2_carve_turn(b, -b->lean, s);
    snobo2_face_motion(b);
    if (snobo2_carve_too_hard(b))
        b->state = 0x17;
    else if (D_801053B0 & 0x40 && b->air_time < 8)
        b->state = snobo2_jump_state(b);
    else if (!b->lean)
        b->state = D_801053A0 & 0x2000 ? 0x12 : 1;
    else if (D_801053A0 & 0x4080 && b->speed > 0x100)
        b->state = 0x16;
    else if (b->speed < 0x100 && D_801053A0 & 0x1000)
        b->state = 2;
}

void snobo2_state_carve_right(Board *b, int enter, Scratch *s)
{
    int i, max, step;
    int *dst, *src;

    if (enter) {
        b->lean = 0;
        b->carve_time = 0;
        b->normal.pad = 0;
        dst = (int *)D_80103D24;
        src = (int *)D_80103E5C->key[0].pose;
        i = D_80103E5C->npose;
        while (i--)
            *dst++ = *src++;
        D_80103D14 = D_80106A54;
    }
    if (D_801053A0 & 0x2000)
        max = D_801053A0 & 0xC ? 0x80 : 0x20;
    else
        max = 0;
    step = D_801053A0 & 0xC ? 2 : 1;
    if (!max)
        step <<= 1;
    if (b->lean < max) {
        b->lean += step;
        if (b->lean > max)
            b->lean = max;
    } else if (b->lean > max) {
        b->lean -= step;
        if (b->lean < 0)
            b->lean = 0;
    }
    max = b->lean;
    if (max > 0x20) {
        b->rot.vy = (max - 0x20) << 3;
        max = 0x20;
    } else {
        b->rot.vy = 0;
    }
    snobo2_pose_lerp(D_80103D24, D_80103E5C->key[3].pose, (short *)&D_80103D14, D_80103E5C->key[3].root, max << 7);
    if (b->lean > 0x20)
        b->carve_time += b->lean >> 5;
    else if (b->carve_time)
        b->carve_time--;
    snobo2_carve_turn(b, b->lean, s);
    snobo2_face_motion(b);
    if (snobo2_carve_too_hard(b))
        b->state = 0x18;
    else if (D_801053B0 & 0x40 && b->air_time < 8)
        b->state = snobo2_jump_state(b);
    else if (!b->lean)
        b->state = D_801053A0 & 0x8000 ? 0x11 : 1;
    else if (D_801053A0 & 0x4080 && b->speed > 0x100)
        b->state = 0x16;
    else if (b->speed < 0x100 && D_801053A0 & 0x1000)
        b->state = 2;
}

int snobo2_jump_state(Board *b)
{
    if (b->speed > 0x400) {
        if (D_80106B4C & 8)
            return 6;
        if (D_80106AF8->flags[D_801069CC] & 1)
            return 6;
    }
    if (D_801053A0 & 0x8000)
        return 5;
    if (D_801053A0 & 0x2000)
        return 4;
    return 3;
}

/* The carving states inline it, but the game keeps a copy too. */
inline void snobo2_carve_turn(Board *b, int turn, Scratch *s)
{
    int drag;
    short *sc;
    short sn, c;

    if (b->air_time >= 25)
        return;
    drag = b->speed * b->speed >> 15;
    turn = turn * (0x180 - drag) / 0x180;
    if (b->air_time >= 9)
        turn = turn * (24 - b->air_time) >> 4;
    sc = &D_8004BC98[(turn >> 2 & 0xFFF) * 2];
    sn = sc[0];
    c = sc[1];
    s->m.m[0][1] = 0;
    s->m.m[1][0] = 0;
    s->m.m[1][1] = 0x1000;
    s->m.m[1][2] = 0;
    s->m.m[2][1] = 0;
    s->m.m[0][2] = sn;
    s->m.m[0][0] = c;
    s->m.m[2][2] = c;
    s->m.m[2][0] = -sn;
    ApplyMatrixLV(&s->m, (VECTOR *)&b->vx, &s->v);
    drag = (0x4000 - (drag * (turn < 0 ? -turn : turn) >> 4)) >> 2;
    b->vx = drag * s->v.vx >> 12;
    b->vz = drag * s->v.vz >> 12;
}

int snobo2_carve_too_hard(Board *b)
{
    if (b->air_time)
        return 0;
    return 0x8C - (b->speed * b->speed >> 16) < b->lean;
}

void snobo2_state_scripted(Board *b, int enter, Scratch *s)
{
    b->rot.vy = 0;
    snobo2_run_events();
    snobo2_face_motion(b);
}

void snobo2_state_scripted2(Board *b, int enter, Scratch *s)
{
    b->rot.vy = 0;
    snobo2_run_events();
    snobo2_face_motion(b);
}

void snobo2_state_wobble(Board *b, int enter, Scratch *s)
{
    int r;
    short *sc;
    short sn, c;

    if (enter) {
        b->turn_target = (unsigned char)b->state == 8 ? -0x18 : 0x18;
        b->rot.vy = 0;
        b->normal.pad = 0;
        b->vx /= 3;
        b->vz /= 3;
        D_801069D4++;
    }
    if (D_801053A0 & 0x8000)
        r = -((rand() & 0xF) + 4);
    else if (D_801053A0 & 0x2000)
        r = (rand() & 0xF) + 4;
    else
        r = 0;
    sc = &D_8004BC98[((unsigned short)(r + b->turn_target) & 0xFFF) * 2];
    sn = sc[0];
    c = sc[1];
    s->m.m[0][1] = 0;
    s->m.m[1][0] = 0;
    s->m.m[1][1] = 0x1000;
    s->m.m[1][2] = 0;
    s->m.m[2][1] = 0;
    s->m.m[0][2] = sn;
    s->m.m[0][0] = c;
    s->m.m[2][2] = c;
    s->m.m[2][0] = -sn;
    ApplyMatrixLV(&s->m, (VECTOR *)&b->vx, (VECTOR *)&b->vx);
    if (b->turn_target)
        b->turn_target += b->turn_target > 0 ? -2 : 2;
    snobo2_run_events();
    snobo2_face_motion(b);
}

#ifdef NON_MATCHING
/* The game masks the turn with a set-flag (sltu/negu/and) where GCC branches (as snobo.c's twin). */
void snobo2_state_push(Board *b, int enter, Scratch *s)
{
    short r;

    if (enter)
        b->normal.pad = 1;
    if (D_80103E54) {
        s->v.vx = 0;
        s->v.vy = 0;
        s->v.vz = -0x3000;
        rot_y(&s->m, (unsigned short)b->facing.vy);
        ApplyMatrixLV(&s->m, &s->v, &s->v);
        b->vx += s->v.vx;
        b->vz += s->v.vz;
        D_80103E54--;
    }
    r = 0x20 - (b->speed >> 4);
    if (D_801053A0 & 0x8000)
        r = -r;
    else
        r &= -((D_801053A0 & 0x2000) != 0);
    if (r) {
        b->facing.vy += r;
        rot_y(&s->m, (unsigned short)r);
        ApplyMatrixLV(&s->m, (VECTOR *)&b->vx, (VECTOR *)&b->vx);
    }
    D_80103E50 = !(b->speed < 0x100 && D_801053A0 & 0x1000);
    snobo2_run_events();
    if ((unsigned char)b->state != 2)
        b->normal.pad = 0;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_state_push);
#endif

void snobo2_state_stopped_steer(Board *b, int enter, Scratch *s)
{
    if (enter) {
        D_80103E50 = D_80103E58;
        b->vx = 0;
        b->vz = 0;
        b->flags |= 1;
    }
    snobo2_run_events();
    if (D_80103E54)
        b->facing.vy = snobo2_angle_step(D_801053B4, b->facing.vy, 0x10);
}

void snobo2_state_stopped(Board *b, int enter, Scratch *s)
{
    if (enter) {
        b->vz = 0;
        b->vx = 0;
        b->flags |= 1;
    }
    D_80103E50 = D_801053A0 >> 12 & 1;
    snobo2_run_events();
}

void snobo2_state_crouch(Board *b, int enter, Scratch *s)
{
    int target, v;

    if (enter) {
        b->turn_target = 0;
        b->lean = 0;
        b->normal.pad = 4;
    }
    target = 0;
    v = b->lean;
    if (D_801053A0 & 0x4080)
        target = (b->turn_target == 0) << 5;
    if (v < target) {
        v += 2;
    } else if (target < v) {
        v -= 2;
        if (v < 0)
            v = 0;
    }
    b->lean = v;
    snobo2_pose_lerp(D_80103E5C->key[1].pose, D_80103E5C->key[4].pose, D_80103E5C->key[1].root, D_80103E5C->key[4].root,
                 -rsin((v << 5) + 0x800));
    if (!b->air_time) {
        b->vx -= b->vx >> 5;
        b->vz -= b->vz >> 5;
        snobo2_face_motion(b);
    }
    if (!b->lean) {
        b->state = 1;
        b->normal.pad = 0;
    }
}

void snobo2_state_spin_left(Board *b, int enter, Scratch *s)
{
    if (enter) {
        b->turn_target = -0x80;
        b->turn_rate = 0;
        b->turn_timer = 0x40;
        b->rot.vy = 0;
        b->normal.pad = 0;
        D_801069D4++;
    }
    snobo2_carve_turn(b, -b->lean, s);
    snobo2_spin_step(b, s);
}

void snobo2_state_spin_right(Board *b, int enter, Scratch *s)
{
    if (enter) {
        b->turn_target = 0x80;
        b->turn_rate = 0;
        b->turn_timer = 0x40;
        b->rot.vy = 0;
        b->normal.pad = 0;
        D_801069D4++;
    }
    snobo2_carve_turn(b, -b->lean, s);
    snobo2_spin_step(b, s);
}

void snobo2_spin_step(Board *b, Scratch *s)
{
    if (b->speed < 0x200)
        b->turn_timer = 0;
    if (b->turn_timer) {
        if (b->turn_target != b->turn_rate)
            b->turn_rate += b->turn_target >> 3;
        b->turn_timer--;
    } else if (b->turn_rate) {
        b->turn_rate -= b->turn_target >> 1;
    } else {
        D_80103E48[1] = 1;
    }
    b->facing.vy += b->turn_rate;
    snobo2_run_events();
}

void snobo2_state_crash(Board *b, int enter, Scratch *s)
{
    if (enter) {
        if ((unsigned char)b->state == 12) {
            b->vx = -b->vx;
            b->vz = -b->vz;
        }
        b->rot.vy = 0;
        b->normal.pad = 0;
        b->vx >>= 1;
        b->vz >>= 1;
        D_801069D4 += 2;
        D_80103E58 = b->speed > 0x500 ? D_8010537C % 3 + 1 : 0;
    }
    if (D_80103E54)
        snobo2_crash_slowdown(b);
    snobo2_run_events();
    snobo2_face_motion(b);
}

void snobo2_state_tumble(Board *b, int enter, Scratch *s)
{
    if (enter) {
        b->rot.vy = 0;
        D_801069D4++;
    }
    snobo2_crash_slowdown(b);
    snobo2_run_events();
    snobo2_face_motion(b);
}

void snobo2_crash_slowdown(Board *b)
{
    if (b->air_time >= 5)
        return;
    b->vx -= b->vx >> 5;
    b->vz -= b->vz >> 5;
    if (b->speed < 0x180)
        b->normal.pad = 3;
    if ((!D_80103E50 && b->speed < 0x100) || b->state_time > 0xF0) {
        b->normal.pad = 0;
        D_80103E50 = 1;
    }
}

void snobo2_state_wait(Board *b, int enter, Scratch *s)
{
    snobo2_run_events();
    if (D_80103E54 && D_801053A0 & 0x1000)
        b->state = 2;
}

void snobo2_state_scripted_stopped(Board *b, int enter, Scratch *s)
{
    snobo2_run_events();
    b->flags |= 1;
}

void snobo2_state_turn_back(Board *b, int enter, Scratch *s)
{
    int d;
    short *yaw;

    if (enter)
        b->turn_rate = 0;
    if (!D_80103E50) {
        b->vx -= b->vx >> 4;
        b->vz -= b->vz >> 4;
        if (b->turn_rate < 0x40)
            b->turn_rate += 8;
        yaw = &b->facing.vy; /* through a pointer the game reads the heading after the store */
        *yaw -= b->turn_rate;
        d = *yaw - D_80106A2C;
        if (d < 0)
            d = -d;
        if (((d + 0x800) & 0xFFF) < 0x40) {
            D_80103E50 = 1;
            b->vz = 0;
            b->vx = 0;
            b->flags |= 1;
        }
    }
    snobo2_run_events();
}

/* Sets the pose (and root) t/4096 of the way from one to another. */
void snobo2_pose_lerp(short *from, short *to, short *from_root, short *to_root, int t)
{
    int i;
    short *dst;

    dst = D_80106A74;
    i = D_80103E5C->nangle;
    for (i--; i != -1; i--) {
        *dst++ = *from + ((*to - *from) * t >> 12);
        from++;
        to++;
    }
    from = from_root;
    to = to_root;
    dst = &D_80106A54.vx;
    for (i = 2; i != -1; i--) {
        *dst++ = *from + ((*to - *from) * t >> 12);
        from++;
        to++;
    }
}

/* Turns him toward where he is going once he is moving. */
void snobo2_face_motion(Board *b)
{
    if (b->speed >= 0x80)
        b->facing.vy = snobo2_angle_step(ratan2(b->vx, b->vz) + 0x800, b->facing.vy, 0x10);
}

#ifdef NON_MATCHING
/* The last block: the game keeps vx >> 8 in v1, GCC in v0 (as snobo.c's twin). */
void snobo2_rider_move(Board *b, Scratch *s)
{
    int push, px, pz, x;

    if (!(b->flags & 1)) {
        if (b->air_time <= 0) {
            push = b->normal.vx >> b->normal.pad;
            px = (b->vx ^ push) < 0 ? push : push - push * b->speed / 2048;
            push = b->normal.vz >> b->normal.pad;
            pz = (b->vz ^ push) < 0 ? push : push - push * b->speed / 2048;
            if (D_80105388) {
                px <<= 1;
                pz <<= 1;
            }
            b->vx += px;
            b->vz += pz;
            if (!((unsigned char)b->state >= 3 && (unsigned char)b->state <= 6) && D_80106B4C & 8) {
                b->bounce = 0;
                b->vy = b->speed;
            }
        } else {
            push = 16 - (b->air_time >> 2);
            if (push < 6)
                push = 6;
            b->vx -= b->vx >> push;
            b->vz -= b->vz >> push;
        }
        s->v.vx = b->vx;
        s->v.vy = 0;
        s->v.vz = b->vz;
        VectorNormal(&s->v, &b->dir);
        b->pos.vx += b->vx;
        b->pos.vz += b->vz;
    }
    x = (b->vx >> 8) * (b->vx >> 8) + (b->vz >> 8) * (b->vz >> 8);
    if (x)
        b->speed = SquareRoot0(x);
    else
        b->speed = 0;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_rider_move);
#endif

/* Gravity, and the landing. Steps flagged 0x20 hold him to the ground while he is on it. */
void snobo2_rider_fall(Board *b)
{
    b->vy += b->bounce;
    b->pos.vy += b->vy;
    if (D_80106AF8->flags[D_801069CC] & 0x20 && !b->air_time)
        b->pos.vy = b->ground_y;
    if (b->f1C >= 0 && b->pos.vy > b->ground_y) {
        b->pos.vy = b->ground_y;
        b->bounce = b->vy;
        b->vy = 0;
        b->air_time = 0;
        D_80103E48[0] = 1;
        if (b->bounce > 0x1000)
            b->bounce -= 0x1000;
        else
            b->bounce = 0x1000;
    } else {
        b->bounce = 0x1000;
        b->air_time++;
    }
}

/* Steps angle from toward to, at most max, the short way round. */
int snobo2_angle_step(int to, int from, int max)
{
    int d = (to & 0xFFF) - (from & 0xFFF);

    if (d < 0)
        d += 0x1000;
    if (d < 0x800) {
        if (max < d)
            d = max;
    } else {
        d -= 0x1000;
        if (d < -max)
            d = -max;
    }
    return (from + d) & 0xFFF;
}

/* After a landing of the given impact: the state to go to (0xFE: a hard landing that costs speed, 0xFF: stay). */
void snobo2_rider_land(Board *b, int impact)
{
    unsigned short next;
    int i = (unsigned char)b->state * 9;

    if (b->speed > 0x200)
        i++;
    if (b->speed > 0x500)
        i++;
    if (impact > 0x200)
        i += 3;
    if (impact > 0x400)
        i += 3;
    next = D_800DE034[i];
    switch (next) {
    case 0xFF:
        break;
    case 0xFE:
        snobo2_play_effect(3, 0x135, 0);
        b->vx -= b->vx >> 2;
        b->vz -= b->vz >> 2;
        break;
    case 8:
        if ((short)(D_801069D0 % 12) >= 7)
            next++;
    default:
        snobo2_play_effect(3, 0xEF, 0);
        b->state = next;
    }
}

/* The board's looping sound for its state, and its pitch from the speed. */
void snobo2_board_sound(Board *b)
{
    short loop = 0;
    short pitch;

    if (b->air_time < 9) {
        loop = D_800DE154[(unsigned char)b->state];
        if (D_800DE154[(unsigned char)b->state] == 0xED && D_80106B4C & 4)
            loop = 0x105;
    }
    if (loop != D_80103E2C) {
        D_80103E2C = loop;
        if (!loop) {
            snd_request.id = 0xA4;
            D_80103E30 = 0;
            snd_request.arg[0] = 1;
            snd_request.arg[1] = 0;
        } else {
            snd_request.id = 0x28;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = loop;
        }
        snd_submit_request();
    }
    if (!loop)
        return;
    pitch = b->speed >> 3;
    if (pitch >= 0x80)
        pitch = 0x7F;
    if (pitch != D_80103E30) {
        snd_request.id = 0xA0;
        D_80103E30 = pitch;
        snd_request.arg[0] = pitch;
        snd_submit_request();
    }
}

/* Poses the rider: each joint's world rotation from the pose angles down the skeleton, the joint positions from the
 * bone lengths, then one draw item per part, placed by the model matrix m and seen by view. SNOBO's
 * snobo_pose_skeleton with the skeleton picked per rider. */
#ifdef NON_MATCHING
/* The game's frame has 32 more bytes of dead locals, and register allocation differs throughout (as snobo.c's twin). */
void snobo2_pose_skeleton(MATRIX *m, MATRIX *view)
{
    SkelWork *sp = (SkelWork *)0x1F800000;
    short *rot = D_80106A74;
    Joint *j;
    SkelPart *part;
    RotMat *w = sp->world;
    SVECTOR *p;
    Queued *d;
    int i;
    short *sc;
    short sn, c;

    i = sp->count = D_800DF404[D_801069B8];
    j = sp->joints = D_800DF40C[D_801069B8];
    sp->parts = D_800DF5A0[D_801069B8];
    for (i--; i != -1; i--) {
        sc = &D_8004BC98[(rot[2] & 0xFFF) * 2];
        sn = sc[0];
        c = sc[1];
        sp->a.m[0][2] = 0;
        sp->a.m[1][2] = 0;
        sp->a.m[2][0] = 0;
        sp->a.m[2][1] = 0;
        sp->a.m[2][2] = 0x1000;
        sp->a.m[0][0] = c;
        sp->a.m[0][1] = -sn;
        sp->a.m[1][0] = sn;
        sp->a.m[1][1] = c;
        sc = &D_8004BC98[(rot[0] & 0xFFF) * 2];
        sn = sc[0];
        c = sc[1];
        sp->b.m[0][0] = 0x1000;
        sp->b.m[0][1] = 0;
        sp->b.m[0][2] = 0;
        sp->b.m[1][0] = 0;
        sp->b.m[2][0] = 0;
        sp->b.m[1][1] = c;
        sp->b.m[1][2] = -sn;
        sp->b.m[2][1] = sn;
        sp->b.m[2][2] = c;
        gte_MulMatrix0(&sp->a, &sp->b, &sp->a);
        sc = &D_8004BC98[(rot[1] & 0xFFF) * 2];
        sn = sc[0];
        c = sc[1];
        sp->b.m[0][1] = 0;
        sp->b.m[1][0] = 0;
        sp->b.m[1][1] = 0x1000;
        sp->b.m[1][2] = 0;
        sp->b.m[2][1] = 0;
        sp->b.m[0][2] = sn;
        sp->b.m[0][0] = c;
        sp->b.m[2][0] = -sn;
        sp->b.m[2][2] = c;
        gte_MulMatrix0(&sp->a, &sp->b, &sp->a);
        gte_MulMatrix0(j->m1, &sp->a, &sp->a);
        gte_MulMatrix0(&sp->a, j->m2, w);
        w++;
        rot += 3;
        j++;
    }

    sp->sv.vy = 0;
    sp->sv.vx = 0;
    sp->view.t[2] = 0;
    sp->view.t[1] = 0;
    sp->view.t[0] = 0;
    gte_SetTransMatrix(&sp->view);
    j = sp->joints;
    w = sp->world;
    p = sp->pos;
    i = sp->count;
    for (i--; i != -1; i--) {
        if (j->parent >= 0)
            gte_MulMatrix0(&sp->world[j->parent], w, w);
        if (!j->no_bone) {
            gte_MulMatrix0(w, j->m0, &sp->view);
            gte_SetRotMatrix(&sp->view);
            sp->sv.vz = j->len;
            gte_ldv0(&sp->sv);
            gte_rt();
            gte_stlvnl(&sp->v);
            p->vx = sp->v.vx >> 3;
            p->vy = sp->v.vy >> 3;
            p->vz = sp->v.vz >> 3;
        }
        j++;
        w++;
        p++;
    }

    j = sp->joints;
    p = sp->pos;
    i = sp->count;
    for (i--; i != -1; i--) {
        if (j->pos_parent >= 0) {
            p->vx += sp->pos[j->pos_parent].vx;
            p->vy += sp->pos[j->pos_parent].vy;
            p->vz += sp->pos[j->pos_parent].vz;
        }
        j++;
        p++;
    }

    gte_SetRotMatrix(m);
    gte_ldv0(&D_80106A54);
    gte_rtv0();
    gte_stlvnl(&sp->v);
    sp->v.vy -= 0x371;
    /* the model's y and z axes point the other way from the skeleton's */
    sp->a.m[0][0] = m->m[0][0];
    sp->a.m[1][0] = m->m[1][0];
    sp->a.m[2][0] = m->m[2][0];
    sp->a.m[0][1] = -m->m[0][1];
    sp->a.m[1][1] = -m->m[1][1];
    sp->a.m[2][1] = -m->m[2][1];
    sp->a.m[0][2] = -m->m[0][2];
    sp->a.m[1][2] = -m->m[1][2];
    sp->a.m[2][2] = -m->m[2][2];
    gte_SetRotMatrix(&sp->a);
    sp->view.t[0] = (sp->v.vx >> 4) + m->t[0];
    sp->view.t[1] = (sp->v.vy >> 4) + m->t[1];
    sp->view.t[2] = (sp->v.vz >> 4) + m->t[2];
    gte_SetTransMatrix(&sp->view);
    p = sp->pts;
    for (part = sp->parts; part->model >= 0; part++) {
        if (part->pos_joint >= 0) {
            gte_ldv0(&sp->pos[part->pos_joint]);
            gte_rt();
            gte_stlvnl(&sp->v);
            p->vx = sp->v.vx;
            p->vy = sp->v.vy;
            p->vz = sp->v.vz;
        } else {
            p->vx = sp->view.t[0];
            p->vy = sp->view.t[1];
            p->vz = sp->view.t[2];
        }
        p++;
    }

    p = sp->pts;
    sp->view = *view;
    gte_MulMatrix0(&sp->view, m, &sp->view_model);
    for (part = sp->parts; part->model >= 0; part++) {
        gte_SetRotMatrix(&sp->view);
        gte_SetTransMatrix(&sp->view);
        gte_ldv0(p);
        gte_rt();
        gte_stlvnl(sp->part.t);
        w = &sp->world[part->joint];
        sp->part.m[0][0] = w->m[0][0];
        sp->part.m[0][1] = -w->m[0][1];
        sp->part.m[0][2] = -w->m[0][2];
        sp->part.m[1][0] = -w->m[1][0];
        sp->part.m[1][1] = w->m[1][1];
        sp->part.m[1][2] = w->m[1][2];
        sp->part.m[2][0] = -w->m[2][0];
        sp->part.m[2][1] = w->m[2][1];
        sp->part.m[2][2] = w->m[2][2];
        gte_MulMatrix0(&sp->view_model, &sp->part, &sp->part);
        p++;
        d = &D_801053B8[D_80105384++];
        d->m = sp->part;
        d->how = 0;
        d->model = part->model;
        d->shade = 0;
        d->drawer = part->drawer;
    }
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_pose_skeleton);
#endif

/* The HUD's draw mode: texture page at (0x180, 0x100). */
void snobo2_init_hud_mode(void)
{
    func_80044A68(&D_80103E60, 1, 0, GetTPage(0, 0, 0x180, 0x100), 0);
}

/* Adds step (three BCD digits, to the lowest three of the time) to c; the clock stops at all nines. */
Clock snobo2_time_add(Clock c, unsigned short step)
{
    unsigned short d;

    d = c.t.d0 + (step & 0xF);
    step >>= 4;
    if (d >= 10) {
        d -= 10;
        step++;
    }
    c.t.d0 = d;
    d = c.t.d1 + (step & 0xF);
    step >>= 4;
    if (d >= 10) {
        d -= 10;
        step++;
    }
    c.t.d1 = d;
    c.t.d2 += step;
    if (c.t.d2 < 10)
        return c;
    c.t.d2 -= 10;
    c.t.d3++;
    if (c.t.d3 < 10)
        return c;
    c.t.d3 -= 10;
    c.t.d4++;
    if (c.t.d4 < 10)
        return c;
    c.t.d4 -= 10;
    c.t.d5++;
    if (c.t.d5 < 6)
        return c;
    c.t.d5 -= 6;
    c.t.d6++;
    if (c.t.d6 < 10)
        return c;
    c.t.d6 -= 10;
    c.t.d7++;
    if (c.t.d7 >= 10)
        c.word = 0x99999999;
    return c;
}

/* The time and the course's record at x, y. */
#ifdef NON_MATCHING
/* Register allocation of x and y (the game keeps raw and sign-extended copies of both, and x + 0x24 twice). */
void snobo2_draw_time_and_record(unsigned int time, short x, short y)
{
    char *s;

    snobo2_draw_text(D_800A034C, x, y, 0x808080);
    snobo2_draw_text(D_800A0350, x, y + 14, 0x808080);
    if (D_80106A28) {
        snobo2_time_text(D_800DF5AC, time, 0x10);
        s = D_800DF5AC;
    } else {
        s = D_800DF5B8;
    }
    snobo2_draw_text(s, x + 0x24, y, 0x808080);
    time = D_801069C0[D_80106A28 - 1];
    if (D_80106A28 && time) {
        snobo2_time_text(D_800DF5C4, time, 0x30);
        s = D_800DF5C4;
    } else {
        s = D_800DF5D0;
    }
    snobo2_draw_text(s, x + 0x24, y + 14, 0x808080);
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_time_and_record);
#endif

/* Writes time's digits (from the minutes down to the thousandths) into a "00'00"000" template, as characters base
 * up. */
inline void snobo2_time_text(char *s, unsigned int time, int base)
{
    s[0] = base + (time >> 28);
    s[1] = base + (time >> 24 & 0xF);
    s[3] = base + (time >> 20 & 0xF);
    s[4] = base + (time >> 16 & 0xF);
    s[6] = base + (time >> 12 & 0xF);
    s[7] = base + (time >> 8 & 0xF);
    s[8] = base + (time >> 4 & 0xF);
}

/* Draws n (three BCD digits, leading zeros blank) at x, y. */
void snobo2_draw_number(unsigned short n, short x, short y)
{
    short i;
    int shift;

    for (i = 1, shift = 8; i < 4; i++, shift -= 4)
        D_800DF5DC[i] = (n >> shift & 0xF) + 0x10;
    for (i = 1; i < 3; i++) {
        if (D_800DF5DC[i] != 0x10)
            break;
        D_800DF5DC[i] = 0x40;
    }
    snobo2_draw_text(D_800DF5DC, x, y, 0x808080);
}

/* Draws menu n with option sel highlighted. */
void snobo2_draw_menu(int n, int sel)
{
    Menu *m = &D_800DF61C[n];
    int c, y, x;
    unsigned int hi;
    int i;

    c = (short)D_8010537C << m->pulse;
    c &= 0x1F;
    y = m->y;
    x = m->x - (m->w >> 1);
    if (c > 16)
        c = 32 - c;
    c = c * 2 + 0x60;
    hi = c | (c << 16 | c << 8);
    for (i = 0; i < m->count; i++, y += 12)
        snobo2_draw_text(m->item[i], x, y, i == sel ? hi : 0x505050);
    snobo2_draw_text(D_800A03C0, x - 9, sel * 12 + m->y, hi);
    WIN->box.x = -(m->w + 0x20) / 2;
    WIN->box.y = m->y - 6;
    WIN->box.w = m->w + 0x20;
    WIN->box.h = m->count * 12 + 8;
    snobo2_draw_window(WIN, 0x100, D_80105380 + 1);
}

/* A pulsing highlight over a HUD box, frame t: a lighter sprite and its blend mode. */
#ifdef NON_MATCHING
/* v0/v1 swapped in the colour's last two ors. */
void snobo2_draw_pulse_box_a(int t)
{
    unsigned int *p;
    int c = t & 0xF;
    unsigned int rgb;

    if (c > 8)
        c = 16 - c;
    c = c * 16 + 0x40;
    p = D_8010538C;
    rgb = c | (c << 16 | c << 8);
    p[0] = 0x04000000;
    p[1] = rgb | 0x66000000;
    p[2] = 0xFFC8FFA2;
    p[3] = 0x79C00000;
    p[4] = 0x003100BC;
    addPrim(&D_80105380[1], p);
    p += 5;
    p[0] = 0x01000000;
    p[1] = 0xE1000498;
    addPrim(&D_80105380[1], p);
    D_8010538C = p + 2;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_pulse_box_a);
#endif

/* A pulsing highlight over a HUD box, frame t: a lighter sprite and its blend mode. */
#ifdef NON_MATCHING
/* v0/v1 swapped in the colour's last two ors. */
void snobo2_draw_pulse_box_b(int t)
{
    unsigned int *p;
    int c = t & 0xF;
    unsigned int rgb;

    if (c > 8)
        c = 16 - c;
    c = c * 16 + 0x40;
    p = D_8010538C;
    rgb = c | (c << 16 | c << 8);
    p[0] = 0x04000000;
    p[1] = rgb | 0x66000000;
    p[2] = 0xFFCBFF92;
    p[3] = 0x79C03200;
    p[4] = 0x004A00E6;
    addPrim(&D_80105380[1], p);
    p += 5;
    p[0] = 0x01000000;
    p[1] = 0xE1000498;
    addPrim(&D_80105380[1], p);
    D_8010538C = p + 2;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_pulse_box_b);
#endif

/* A pulsing highlight over a HUD box, frame t: a lighter sprite and its blend mode. */
#ifdef NON_MATCHING
/* v0/v1 swapped in the colour's last two ors. */
void snobo2_draw_pulse_box_c(int t)
{
    unsigned int *p;
    int c = t & 0xF;
    unsigned int rgb;

    if (c > 8)
        c = 16 - c;
    c = c * 16 + 0x40;
    p = D_8010538C;
    rgb = c | (c << 16 | c << 8);
    p[0] = 0x04000000;
    p[1] = rgb | 0x66000000;
    p[2] = 0xFFC6FFA6;
    p[3] = 0x7A408000;
    p[4] = 0x002400B4;
    addPrim(&D_80105380[1], p);
    p += 5;
    p[0] = 0x01000000;
    p[1] = 0xE1000498;
    addPrim(&D_80105380[1], p);
    D_8010538C = p + 2;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_pulse_box_c);
#endif

/* Draws HUD picture n: a sprite and its texture page, as words (the game writes them through a plain pointer: GCC
 * then reloads the ordering table after each store). */
void snobo2_draw_hud_pic(int n)
{
    unsigned int *p = D_8010538C;
    unsigned int *h = D_800DF68C[n];

    p[0] = 0x04000000;
    p[1] = 0x65000000;
    p[2] = h[0];
    p[3] = h[1];
    p[4] = h[2];
    addPrim(&D_80105380[2], p);
    p += 5;
    p[0] = 0x01000000;
    p[1] = h[3];
    addPrim(&D_80105380[2], p);
    D_8010538C = p + 2;
}

/* The results window, t frames after it opened: it grows for 16 frames, then a row shows every 16. */
#ifdef NON_MATCHING
/* The quotient of n / 10 lands in v0 where the game uses v1 (both divisions). */
void snobo2_draw_results(Window *w, int t, unsigned int *ot)
{
    int i, n, scale, c;

    if (t >= 16) {
        w->u.rows = ((t - 16) >> 4) + 1;
        if (w->u.rows >= 6)
            w->u.rows = 5;
        for (i = 0; i < w->u.rows; i++) {
            char *s = D_800DF760[i];

            n = i >= 4 ? D_80106AF4[D_80106A28 - 1] : D_80105390[i];
            if (n >= 100) {
                *(int *)s = 0x2F303031; /* "100/" */
            } else {
                s[0] = 0x40;
                if (n / 10)
                    s[1] = n / 10 + '0';
                else
                    s[1] = 0x40;
                s[2] = n % 10 + '0';
            }
        }
        for (i = 0; i < w->u.rows; i++) {
            if (i == 3) {
                c = (short)D_8010537C;
                c &= 0x1F;
                if (c > 16)
                    c = 32 - c;
                c = c * 2 + 0x60;
                c |= c << 16 | c << 8;
                w->text_rgb = c;
            } else {
                w->text_rgb = 0x787878;
            }
            snobo2_draw_text(D_800DF74C[i], -0x45, D_800DF774[i], w->text_rgb);
            snobo2_draw_text(D_800DF760[i], -7, D_800DF774[i], w->text_rgb);
            snobo2_draw_text(D_800A0418, 0x2C, D_800DF774[i], w->text_rgb);
        }
    }
    w->box.x = -0x4D;
    w->box.y = -0x36;
    w->box.w = 0x99;
    w->box.h = 0x4C;
    scale = t << 4;
    if (scale > 0x100)
        scale = 0x100;
    if (scale)
        snobo2_draw_window(w, scale, ot);
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_results);
#endif

/* The course's records window, t frames after it opened: it grows for 16 frames, then a row shows every 4. */
void snobo2_draw_records(int course, int sel, int t)
{
    int rows = (t - 16) >> 2;
    int c, i;
    unsigned int hi, rgb;
    char *text;

    if (rows >= 6)
        rows = 5;
    if (rows > 0) {
        c = (short)D_8010537C;
        c &= 0x1F;
        if (c > 16)
            c = 32 - c;
        c = c * 2 + 0x60;
        D_800DF780[7] = course + 'A';
        hi = c | (c << 16 | c << 8);
        snobo2_draw_text(D_800DF780, -0x32, -0x3A, 0x787878);
        for (i = 0; i < rows; i++) {
            rgb = 0x787878;
            if (i == sel)
                rgb = hi;
            snobo2_draw_text(D_800DF7A8[i], -0x30, i * 14 - 0x2A, rgb);
            if (D_80106B08[course][i]) {
                snobo2_time_text(D_800DF790, D_80106B08[course][i], '0');
                text = D_800DF790;
            } else {
                text = D_800DF79C;
            }
            snobo2_draw_text(text, -0x10, i * 14 - 0x2A, rgb);
        }
    }
    i = t << 4;
    WIN->box.x = -0x3E;
    WIN->box.y = -0x40;
    WIN->box.w = 0x7C;
    WIN->box.h = 0x5C;
    if (i > 0x100)
        i = 0x100;
    if (i)
        snobo2_draw_window(WIN, i, D_80105380 + 1);
}

#ifdef NON_MATCHING
/* The sprite tag constant 0x04000000 is hoisted out of the loop; the game loads it for each glyph. */
void snobo2_draw_text(char *s, int x, int y, unsigned int rgb)
{
    unsigned int *p = D_8010538C;
    Glyph *g;
    unsigned char c;

    while ((c = *s++)) {
        if (c == ' ') {
            x += 4;
            continue;
        }
        p[0] = 0x04000000;
        g = (Glyph *)((char *)D_800DF7C4 + (c - 0x10) * 8);
        p[1] = rgb | 0x64000000;
        p[2] = y << 16 | (unsigned short)x;
        p[3] = g->uvclut;
        p[4] = g->wh.word;
        addPrim(&D_80105380[1], p);
        x = x + g->wh.w - 1;
        p += 5;
    }
    *(DR_MODE *)p = D_80103E60;
    addPrim(&D_80105380[1], p);
    p += 3;
    D_8010538C = p;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_text);
#endif

/* The window at its place, opened to scale / 0x100 about its centre: a light bevelled frame with a dark inner edge and
 * a half-transparent fill. */
#ifdef NON_MATCHING
/* Register allocation and spill slots: the game keeps r, r - 1 and l - 1 on the stack and ot in fp. */
void snobo2_draw_window(Window *w, int scale, unsigned int *ot)
{
    void *p = D_8010538C;
    Tile *t;
    short l, r, top, b;

    l = w->box.x + (w->box.w >> 1) - (w->box.w * scale >> 9);
    w->u.rgb = 0xE0E0E0;
    r = l + (w->box.w * scale >> 8);
    w->pt[0].vx = l;
    w->pt[1].vx = r - 1;
    w->pt[2].vx = r - 1;
    w->pt[3].vx = l - 1;
    top = w->box.y + (w->box.h >> 1) - (w->box.h * scale >> 9);
    b = top + (w->box.h * scale >> 8);
    w->pt[0].vy = top - 2;
    w->pt[1].vy = top - 2;
    w->pt[2].vy = top - 1;
    w->pt[3].vy = top - 1;
    p = snobo2_add_window_line(w, p, ot);
    w->pt[0].vx = r;
    w->pt[0].vy = top - 1;
    w->pt[1].vx = r;
    w->pt[1].vy = b - 1;
    w->pt[2].vx = r + 1;
    w->pt[2].vy = b - 1;
    w->pt[3].vx = r + 1;
    w->pt[3].vy = top;
    p = snobo2_add_window_line(w, p, ot);
    w->pt[0].vx = r;
    w->pt[0].vy = b;
    w->pt[1].vx = l;
    w->pt[1].vy = b;
    w->pt[2].vx = l;
    w->pt[2].vy = b + 1;
    w->pt[3].vx = r - 1;
    w->pt[3].vy = b + 1;
    p = snobo2_add_window_line(w, p, ot);
    w->pt[0].vx = l - 1;
    w->pt[0].vy = b;
    w->pt[1].vx = l - 1;
    w->pt[1].vy = top;
    w->pt[2].vx = l - 2;
    w->pt[2].vy = top;
    w->pt[3].vx = l - 2;
    w->pt[3].vy = b - 1;
    p = snobo2_add_window_line(w, p, ot);
    w->pt[0].vx = l;
    w->pt[0].vy = top;
    p = snobo2_add_window_dot(w, p, ot);
    w->pt[0].vx = r - 1;
    p = snobo2_add_window_dot(w, p, ot);
    w->pt[0].vy = b - 1;
    p = snobo2_add_window_dot(w, p, ot);
    w->pt[0].vx = l;
    p = snobo2_add_window_dot(w, p, ot);
    w->u.rgb = 0x202020;
    w->pt[0].vx = l;
    w->pt[0].vy = b - 2;
    w->pt[1].vx = l;
    w->pt[1].vy = top + 1;
    w->pt[2].vx = l + 1;
    w->pt[2].vy = top;
    w->pt[3].vx = r - 2;
    w->pt[3].vy = top;
    p = snobo2_add_window_line(w, p, ot);
    w->pt[0].vx = l + 2;
    w->pt[0].vy = b + 2;
    w->pt[1].vx = r - 1;
    w->pt[1].vy = b + 2;
    w->pt[2].vx = r + 2;
    w->pt[2].vy = b - 1;
    w->pt[3].vx = r + 2;
    w->pt[3].vy = top + 2;
    p = snobo2_add_window_line(w, p, ot);
    w->pt[0].vx = l + 1;
    w->pt[0].vy = top + 1;
    t = (Tile *)snobo2_add_window_dot(w, p, ot);
    t->tag = 0x03000000;
    t->code = 0x62402020;
    t->xy = (top + 1) << 16 | (unsigned short)(l + 1);
    t->wh = ((short)(b - 1) - (short)top) << 16 | (unsigned short)(r - 1 - l);
    addPrim(ot, t);
    D_8010538C = t + 1;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_window);
#endif

/* A line through pt[0-3], half-transparent, in the window's colour; returns the next free primitive. */
Line4 *snobo2_add_window_line(Window *w, Line4 *p, unsigned int *ot)
{
    p->tag = 0x06000000;
    p->code = w->u.rgb | 0x4C000000;
    p->xy[0] = w->pt[0].vy << 16 | (unsigned short)w->pt[0].vx;
    p->xy[1] = w->pt[1].vy << 16 | (unsigned short)w->pt[1].vx;
    p->xy[2] = w->pt[2].vy << 16 | (unsigned short)w->pt[2].vx;
    p->xy[3] = w->pt[3].vy << 16 | (unsigned short)w->pt[3].vx;
    p->end = 0x55555555;
    addPrim(ot, p);
    return p + 1;
}

/* A dot at pt[0] in the window's colour; returns the next free primitive. */
Dot *snobo2_add_window_dot(Window *w, Dot *p, unsigned int *ot)
{
    p->tag = 0x02000000;
    p->code = w->u.rgb | 0x68000000;
    p->xy = w->pt[0].vy << 16 | (unsigned short)w->pt[0].vx;
    addPrim(ot, p);
    return p + 1;
}

/* The BCD sum of the entries of D_800DFA1C picked by the bits of mask. */
unsigned short snobo2_bcd_sum_mask(unsigned short mask)
{
    unsigned short sum = 0;
    unsigned short *p = D_800DFA1C;

    for (; mask; mask >>= 1, p++) {
        if (mask & 1)
            sum = snobo2_bcd_add(sum, *p);
    }
    return sum;
}

/* a + b in four-digit BCD. */
unsigned short snobo2_bcd_add(unsigned short a, unsigned short b)
{
    unsigned short sum = 0;
    unsigned short d;
    unsigned short carry = 0;
    short sh;

    for (sh = 0; sh < 13; sh += 4) {
        carry += ((a >> sh) & 0xF) + ((b >> sh) & 0xF);
        d = carry;
        if (d >= 10) {
            d = carry - 10;
            carry = 1;
        } else {
            carry = 0;
        }
        sum |= d << sh;
    }
    return sum;
}

/* Clears the spray ring. */
void snobo2_clear_spray(void)
{
    Spray *p;
    int i;

    D_80103F6C = 0;
    p = D_80103E6C;
    for (i = 7; i != -1; i--) {
        p->used = 0;
        p++;
    }
}

/* At speed, a new puff of snow behind the board, in the next free ring slot (or the oldest). */
void snobo2_spawn_spray(Board *b)
{
    Spray *p;
    int i;
    int n;
    int v;

    if (b->speed < 0x80)
        return;
    i = D_80103F6C;
    n = 8;
    while (D_80103E6C[i].used) {
        if (!--n)
            break;
        i = (i + 1) & 7;
    }
    p = &D_80103E6C[i];
    p->used = 1;
    D_80103F6C = (i + 1) & 7;
    v = (b->bounce >> 12) + 0x80;
    if (v > 0xF0)
        v = 0xF0;
    p->shade = v;
    p->fade = 0x10;
    p->w = 0x10;
    p->dw = (b->speed >> 8) + (b->bounce >> 16);
    p->h = 0x10;
    p->dh = (b->speed >> 7) + (b->bounce >> 16);
    p->dx = -(b->vx >> 14);
    p->dy = -(b->speed >> 7) * 3 / 2;
    p->dz = -(b->vz >> 14);
    p->x = 0;
    p->y = 0;
    p->z = 0;
    p->rise = 8;
}

/* A glowing quad on the board, brighter with speed, its texture cycling on each counted frame; not where the step's
 * flag 0x10 is set, nor in the air. */
#ifdef NON_MATCHING
/* Scheduling and registers of the corner and uv words (where snobo.c's twin fails too). */
void snobo2_draw_board_glow(Board *b, MATRIX *view, GlowWork *w, int count)
{
    POLY_FT4 *prim;
    unsigned int c;
    int x, y;
    int l, r, t, bot;
    unsigned short uv;

    if (D_80106AF8->flags[D_801069CC] & 0x10)
        return;
    if (count)
        D_800DFA3C++;
    if (b->air_time >= 3)
        return;
    prim = D_8010538C;
    prim->tag = 0x09000000;
    c = b->speed >> 3;
    if (c > 0xF0)
        c = 0xF0;
    *(int *)&prim->r0 = 0x2E000000 | c << 16 | c << 8 | c;
    w->v.vx = (b->pos.vx >> 12) - D_80106A60.x;
    w->v.vy = (b->pos.vy >> 12) - D_80106A60.y;
    w->v.vz = (b->pos.vz >> 12) - D_80106A60.z;
    ApplyMatrix(view, &w->v, (VECTOR *)w->m.t);
    w->m.t[0] += view->t[0];
    w->m.t[1] += view->t[1];
    w->m.t[2] += view->t[2];
    SetTransMatrix(&w->m);
    w->v.vz = 0;
    w->v.vy = 0;
    w->v.vx = 0;
    gte_ldv0(&w->v);
    gte_rtps();
    gte_stsxy(&w->sx);
    gte_stszotz(&w->otz);
    x = w->sx;
    y = w->sy;
    l = x - 0x20;
    r = x + 0x20;
    t = y - 0x28;
    bot = y + 0x18;
    y = ((D_800DFA3C << 4) & 0x60) + 0x80;
    uv = y << 8;
    *(int *)&prim->x0 = t << 16 | (unsigned short)l;
    *(int *)&prim->x1 = t << 16 | (unsigned short)r;
    *(int *)&prim->x2 = bot << 16 | (unsigned short)l;
    *(int *)&prim->x3 = bot << 16 | (unsigned short)r;
    *(int *)&prim->u0 = 0x78C00000 | uv;
    *(int *)&prim->u1 = (getTPage(0, 1, 0x140, 0x100) << 16 | 0x1F) | uv;
    *(int *)&prim->u2 = (unsigned short)((y + 0x1F) << 8);
    *(int *)&prim->u3 = (unsigned short)((y + 0x1F) << 8 | 0x1F);
    addPrim(D_80105380 + 1, prim);
    D_8010538C = prim + 1;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_board_glow);
#endif

/* Turns a loaded model's part offsets into pointers, once. */
void snobo2_relocate_model(Model *m)
{
    ModelPart *p;
    unsigned int base;
    int i;

    if (m->flags < 0)
        return;
    p = m->part;
    base = (unsigned int)p >> 2;
    for (i = m->n; i-- != 0; p++) {
        p->verts += base;
        p->p08 += base;
        p->faces += base;
    }
    m->flags |= 0x80000000;
}

/* Draws everything queued from model m into ot; returns the next free primitive. */
#ifdef NON_MATCHING
/* GCC hoists the loop's 1 and -1 into s4/s5 (as snobo.c's twin). */
void *snobo2_draw_queued(Model *m, unsigned int *ot, void *prim)
{
    DrawWork *d = (DrawWork *)0x1F800000;
    Queued *q = D_801053B8;
    int i;

    gte_SetFarColor(0xFF, 0xFF, 0xFF);
    d->model = m;
    d->ot = ot;
    d->prim = prim;
    i = D_80105384;
    while (--i != -1) {
        d->part = q->model;
        d->drawer = q->drawer;
        d->shade = q->shade;
        switch (q->how) {
        case 0:
            gte_SetRotMatrix(&q->m);
        case 1:
            gte_SetTransMatrix(&q->m);
        }
        snobo2_draw_part(d);
        q++;
    }
    D_80105384 = 0;
    return d->prim;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_queued);
#endif

/* Projects the part's vertices, three at a time, to the scratchpad, then has its drawer draw its faces. */
void snobo2_draw_part(DrawWork *d)
{
    ModelPart *part = (ModelPart *)((char *)d->model + d->part * sizeof(ModelPart) + sizeof(Model));
    SVECTOR *v = (SVECTOR *)part->verts;
    int n = part->nverts;
    int *sxy = d->sxy;
    int *sz = d->sz;

    if (n >= 0x55)
        return;
    gte_ldv3(v, v + 1, v + 2);
    gte_rtpt();
    gte_stsxy3(sxy, sxy + 1, sxy + 2);
    sxy += 3;
    gte_stsz3c(sz);
    while ((n -= 3) > 0) {
        v += 3;
        sz += 3;
        gte_ldv3(v, v + 1, v + 2);
        gte_rtpt();
        gte_stsxy3(sxy, sxy + 1, sxy + 2);
        sxy += 3;
        gte_stsz3c(sz);
    }
    D_800DFA40[(unsigned short)d->drawer](d, part->faces, part->nfaces);
}

/* Gouraud triangles facing the camera, linked by their average depth. */
#ifdef NON_MATCHING
/* Temporaries only: the counter in t9, the game's in t2 (as snobo.c's twin). */
void snobo2_draw_g3(DrawWork *d, FaceG3 *f, int n)
{
    ShadedTri *p = d->prim;
    int *sxy = d->sxy;
    int *sz = d->sz;
    int a, b, c;

    while (n-- != 0) {
        a = f->i0;
        b = f->i1;
        c = f->i2;
        gte_ldsxy3(sxy[a], sxy[b], sxy[c]);
        gte_nclip();
        gte_stopz(&d->z);
        if (d->z > 0) {
            gte_ldsz3(sz[a], sz[b], sz[c]);
            gte_avsz3();
            gte_stotz(&d->z);
            if (d->z > 0 && d->z < 0x1000) {
                p->tag = 0x06000000;
                p->xy0 = sxy[a];
                p->xy1 = sxy[b];
                p->xy2 = sxy[c];
                p->c0 = f->c0;
                p->c1 = f->c1;
                p->c2 = f->c2;
                addPrim(d->ot + d->z, p);
                p++;
            }
        }
        f++;
    }
    d->prim = p;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_g3);
#endif

/* As snobo2_draw_g3 with the colours depth-cued by the drawer's shade. */
void snobo2_draw_g3_cued(DrawWork *d, FaceG3 *f, int n)
{
    ShadedTri *p = d->prim;
    int *sxy = d->sxy;
    int *sz = d->sz;
    int shade = d->shade;
    int a, b, c;

    while (n-- != 0) {
        a = f->i0;
        b = f->i1;
        c = f->i2;
        gte_ldsxy3(sxy[a], sxy[b], sxy[c]);
        gte_nclip();
        gte_stopz(&d->z);
        if (d->z > 0) {
            gte_ldsz3(sz[a], sz[b], sz[c]);
            gte_avsz3();
            gte_stotz(&d->z);
            if (d->z > 0 && d->z < 0x1000) {
                p->tag = 0x06000000;
                p->xy0 = sxy[a];
                p->xy1 = sxy[b];
                p->xy2 = sxy[c];
                gte_ldrgb3(&f->c2, &f->c1, &f->c0);
                gte_ldir0(shade);
                gte_dpct();
                gte_strgb3(&p->c2, &p->c1, &p->c0);
                addPrim(d->ot + d->z, p);
                p++;
            }
        }
        f++;
    }
    d->prim = p;
}

/* Textured triangles facing the camera, in the drawer's colour and clut (or the first face's). */
void snobo2_draw_ft3(DrawWork *d, FaceFT3 *f, int n)
{
    TexTri *p;
    int *sxy = d->sxy;
    int *sz = d->sz;
    unsigned int code = (d->shade & 0xFFFF0000) | 0x25000000;
    unsigned int clut = d->shade << 16;
    int a, b, c;

    if (!clut)
        clut = f->clut << 16;
    p = d->prim;
    while (--n != -1) {
        a = f->i0;
        b = f->i1;
        c = f->i2;
        gte_ldsxy3(sxy[a], sxy[b], sxy[c]);
        gte_nclip();
        gte_stopz(&d->z);
        if (d->z > 0) {
            gte_ldsz3(sz[a], sz[b], sz[c]);
            gte_avsz3();
            gte_stotz(&d->z);
            if (d->z > 0 && d->z < 0x1000) {
                p->tag = 0x07000000;
                p->code = code;
                p->xy0 = sxy[a];
                p->xy1 = sxy[b];
                p->xy2 = sxy[c];
                p->uv0 = f->uv0 | clut;
                p->uv1 = f->uv1;
                p->uv2 = f->uv2;
                addPrim(d->ot + d->z, p);
                p++;
            }
        }
        f++;
    }
    d->prim = p;
}

/* As snobo2_draw_g3, semi-transparent, each triangle preceded by a draw mode with the drawer's blend. */
#ifdef NON_MATCHING
/* Temporaries only: the counter in t9, the game's in t2. */
void snobo2_draw_g3_blend(DrawWork *d, FaceG3 *f, int n)
{
    union {
        ShadedTri *g3;
        BlendMode *tp;
    } p;
    int *sxy = d->sxy;
    int *sz = d->sz;
    unsigned int mode = 0xE1000200 | (d->shade & 3) << 5;
    int a, b, c;

    p.g3 = d->prim;
    while (n-- != 0) {
        a = f->i0;
        b = f->i1;
        c = f->i2;
        gte_ldsxy3(sxy[a], sxy[b], sxy[c]);
        gte_nclip();
        gte_stopz(&d->z);
        if (d->z > 0) {
            gte_ldsz3(sz[a], sz[b], sz[c]);
            gte_avsz3();
            gte_stotz(&d->z);
            if (d->z > 0 && d->z < 0x1000) {
                p.g3->tag = 0x06000000;
                p.g3->xy0 = sxy[a];
                p.g3->xy1 = sxy[b];
                p.g3->xy2 = sxy[c];
                p.g3->c0 = f->c0 | 0x02000000;
                p.g3->c1 = f->c1;
                p.g3->c2 = f->c2;
                addPrim(d->ot + d->z, p.g3);
                p.g3++;
                p.tp->tag = 0x01000000;
                p.tp->code = mode;
                addPrim(d->ot + d->z, p.tp);
                p.tp++;
            }
        }
        f++;
    }
    d->prim = p.g3;
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_draw_g3_blend);
#endif

/* A prop of template D_800DFA98 placed by pl about event id's point, standing on the ground there. */
void snobo2_place_prop(short id, Place1 *pl)
{
    Pos *p;
    int y;

    if (D_801053A4)
        return;
    D_800DFA98.rot.vy = pl->ry;
    p = &D_800C5A8C[id];
    D_800DFA98.pos.x = (pl->x + p->x) << 12;
    y = (pl->y + p->y) << 12;
    D_800DFA98.pos.y = y;
    D_800DFA98.pos.z = (pl->z + p->z) << 12;
    D_800DFA98.ground = y;
    D_800DFA98.model = pl->arg[0] + 0xB2;
    obj_spawn(id, &D_800DFA98, 1);
}

/* Hit: at speed it flies off the way the rider went, else it fades out. */
void snobo2_obj_knock(Obj *o)
{
    if (!o->hit)
        return;
    D_80105374 = o->id;
    if (o->hit & 2) {
        o->vel.x = o->knock_x;
        o->vel.y = -0x28000;
        o->vel.z = o->knock_z;
        snobo2_play_effect(3, 0xF0, 0);
        o->t.radius = 0;
        o->t.update = snobo2_obj_fly;
    } else {
        o->t.update = snobo2_obj_expire;
    }
}

/* Flies, tumbling, until it drops below its ground; fades once falling. */
void snobo2_obj_fly(Obj *o)
{
    if (o->t.pos.y > o->t.ground) {
        o->active = 0;
        return;
    }
    o->t.rot.pad = 0;
    o->id = D_80106A30;
    o->t.pos.x += o->vel.x;
    o->t.pos.y += o->vel.y;
    o->t.pos.z += o->vel.z;
    o->vel.y += 0x1000;
    o->t.rot.vx -= 0x20;
    if (o->vel.y > 0)
        o->t8A++;
}

/* A swaying prop of template D_800DFAE0 placed by pl about event id's point; arg[0] keeps its model number (2 also
 * wobbles). */
void snobo2_place_sway(short id, Place1 *pl)
{
    Pos *p;

    if (D_801053A4)
        return;
    D_800DFAE0.arg[0] = pl->arg[0];
    D_800DFAE0.model = pl->arg[0] + 0xB2;
    D_800DFAE0.rot.vy = pl->ry;
    p = &D_800C5A8C[id];
    D_800DFAE0.pos.x = (pl->x + p->x) << 12;
    D_800DFAE0.pos.y = (pl->y + p->y) << 12;
    D_800DFAE0.pos.z = (pl->z + p->z) << 12;
    obj_spawn(id, &D_800DFAE0, 1);
}

/* Waits to be hit (swaying sideways if arg[0] is 2): at speed it sways over, else it fades out. */
void snobo2_obj_sway_idle(Obj *o)
{
    if (o->hit) {
        if (o->hit & 2) {
            o->t.radius = 0;
            snobo2_play_effect(3, 0xF0, 0);
            o->t.update = snobo2_obj_sway;
        } else {
            o->t.update = snobo2_obj_expire;
        }
    }
    if (o->t.arg[0] == 2)
        snobo2_obj_wobble(o);
}

/* Sways back and forth after a hit, then fades out. */
void snobo2_obj_sway(Obj *o)
{
    o->t.rot.vx = -rsin(rsin(o->t84 << 5) >> 4) >> 3;
    o->t.rot.pad = 0;
    if (o->t84++ >= 0x79 && o->t8A++ >= 0x1F)
        o->active = 0;
}

/* Wobbles sideways. */
void snobo2_obj_wobble(Obj *o)
{
    o->t.rot.vz = rsin(rsin(o->t86++ << 5) >> 4) >> 3;
    o->t.rot.pad = 0;
}

/* A slider of template D_800DFB28 (first byte 2) placed by pl about event id's point, hidden until the rider is near;
 * it comes from the side its turn faces away from. No check for a full pool: the game writes through 0 then. */
void snobo2_place_slider(short id, Place1 *pl)
{
    Pos *p;
    Obj *o;

    if (D_801053A4)
        return;
    D_800DFB28.rot.vy = pl->ry;
    p = &D_800C5A8C[id];
    D_800DFB28.pos.x = (pl->x + p->x) << 12;
    D_800DFB28.pos.y = (pl->y + p->y) << 12;
    D_800DFB28.pos.z = (pl->z + p->z) << 12;
    o = obj_spawn(id, &D_800DFB28, 2);
    o->t8A = -1;
    o->t84 = !(pl->ry & 0x800);
}

/* Once the rider is near, starts sliding across the course at half his forward speed or more (from the left or the
 * right by t84). */
void snobo2_obj_slider_wait(Obj *o)
{
    int v;

    if (o->t.pos.z - D_800DDE64.pos.vz > 0x700000)
        return;
    v = D_800DDE64.vz >> 1;
    if (v < 0x8000)
        v = 0x8000;
    o->vel.z = v;
    if (o->t84)
        v = -v;
    o->t.radius = 0x40;
    o->vel.x = v;
    o->t8A = 0;
    o->t.update = snobo2_obj_slide;
}

/* Slides across the course on its slope (a straight line in z, y) until it reaches the far side, where it turns into
 * models 0xD3/0xD4 and moves on; gone once the rider is well past, knocked away when hit. */
void snobo2_obj_slide(Obj *o)
{
    int v;

    if (o->hit) {
        snobo2_obj_slider_hit(o);
        return;
    }
    v = (D_800DDE64.pos.vz - o->t.pos.z) >> 12;
    if (v > 0x200) {
        o->active = 0;
        return;
    }
    o->t.pos.x += o->vel.x;
    o->t.pos.z += o->vel.z;
    o->t.pos.y = (((o->t.pos.z >> 12) - 45122) * 22629 / 84531 + 68150) << 12;
    v = (unsigned int)o->t.pos.x >> 12;
    if (o->t84) {
        if (v > 0xF0363)
            return;
        o->t.rot.pad = 0;
        o->vel.y = -0x10000;
        o->t.rot.vy += 0x80;
        if (v > 0xEFDE5)
            return;
    } else {
        if (v <= 0xF0364)
            return;
        o->t.rot.pad = 0;
        o->vel.y = -0x10000;
        o->t.rot.vy -= 0x80;
        if (v <= 0xF08E5)
            return;
    }
    o->t.update = snobo2_obj_tumble_down;
    o->t.model = 0xD3;
    o->t.frame = 0xD4;
}

/* Hit: knocked away at three quarters of the rider's speed, with a sound. */
void snobo2_obj_slider_hit(Obj *o)
{
    o->t.radius = 0;
    o->vel.x = o->knock_x * 3 / 4;
    o->vel.y = -0x20000;
    o->vel.z = o->knock_z * 3 / 4;
    o->t.update = snobo2_obj_slider_fly;
    snobo2_play_effect(3, 0x135, 0);
}

/* Knocked away: flies, tumbling, over the slope (a straight line in z, y); falling back onto it, it turns into model
 * 0xE2 and drops on, spawning a D_800DFB70 where it lands (under the course event of its z) with a sound by how far
 * ahead of the rider it is. Gone once the rider is well past. */
#ifdef NON_MATCHING
/* Registers only: vel.y lands in a0 after the multiply (the game's a2, before it). */
void snobo2_obj_slider_fly(Obj *o)
{
    int y, ground, vy, d;

    if ((D_800DDE64.pos.vz - o->t.pos.z) >> 12 > 0x200) {
        o->active = 0;
        return;
    }
    o->t.model = 0xD3;
    o->t.pos.z += o->vel.z;
    ground = (((o->t.pos.z >> 12) - 45122) * 22629 / 84531 + 68150) << 12;
    o->t.frame = 0xD4;
    vy = o->vel.y + 0x1000;
    o->vel.y = vy;
    y = vy + o->t.pos.y;
    o->t.pos.x += o->vel.x;
    if (vy > 0 && y > ground - 0x40000) {
        y = ground;
        o->t.model = 0xE2;
        o->t.rot.vy = 0x200;
        o->t.update = snobo2_obj_fall_tip;
        o->vel.y = -0x20000;
        o->t.frame = 0;
        o->t84 = 0;
        o->vel.x >>= 1;
        o->vel.z >>= 1;
        D_800DFB70.pos.x = o->t.pos.x;
        D_800DFB70.pos.y = y;
        D_800DFB70.pos.z = o->t.pos.z;
        obj_spawn(((o->t.pos.z >> 12) - 45122) / 1483 + 0x8E, &D_800DFB70, 1);
        d = o->t.pos.z - D_800DDE64.pos.vz;
        if (d >> 12 > 0 && d >> 12 < 0x1000)
            snobo2_play_effect(4, 0x1AB, 0x7F - (d >> 17));
    }
    o->t.pos.y = y;
    if (o->t86++ < 0x40) {
        o->t.rot.pad = 0;
        o->t.rot.vx -= 0x20;
    }
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_obj_slider_fly);
#endif

/* Falls and tips forward, fading out after a while. */
void snobo2_obj_fall_tip(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F) {
        o->active = 0;
        return;
    }
    o->t.rot.pad = 0;
    o->t.pos.x += o->vel.x;
    o->vel.y += 0x1000;
    o->t.pos.y = o->vel.y + o->t.pos.y;
    o->t.pos.z += o->vel.z;
    o->t.rot.vx -= 0x200;
}

/* Cycles through four models; on a hit it fades out (with a sound if at speed). */
void snobo2_obj_cycle_models(Obj *o)
{
    if (o->hit) {
        if (o->hit & 2) {
            o->t.radius = 0;
            snobo2_play_effect(3, 0x135, 0);
        }
        o->t84 = 0;
        o->t.update = snobo2_obj_expire;
    }
    o->t.model = D_800DFBB8[(o->t84++ >> 2) & 3];
}

void snobo2_obj_tumble_down(Obj *o)
{
    int z, y, ny;

    if ((D_800DDE64.pos.vz - o->t.pos.z) >> 12 > 0x200) {
        o->active = 0;
        return;
    }
    z = REREAD(o->t.pos.z) + o->vel.z;
    y = SLOPE_Y(z);
    o->t.pos.x += o->vel.x;
    o->t.pos.z = z;
    ny = (o->vel.y += 0x1000) + o->t.pos.y;
    if (o->vel.y > 0 && ny > y - 0x40000) {
        o->active = 0;
        return;
    }
    o->t.pos.y = ny;
    o->t.rot.pad = 0;
    o->t.rot.vx -= 0x40;
}

void snobo2_place_runner_a(short id, Place2 *r)
{
    Pos *p;
    Obj *o;

    if (D_801053A4)
        return;
    D_800DFBC0.rot.vy = r->ry;
    p = &D_800C5A8C[id];
    D_800DFBC0.pos.x = (r->x + p->x) << 12;
    D_800DFBC0.pos.y = (r->y + p->y) << 12;
    D_800DFBC0.pos.z = (r->z + p->z) << 12;
    o = obj_spawn(id, &D_800DFBC0, 2);
    o->t84 = r->arg[0];
    o->t.arg[0] = (r->arg[1] << 12) + o->t.pos.z;
    o->t8A = -1;
}

void snobo2_obj_wait_close_a(Obj *o)
{
    if (o->t.pos.z - D_800DDE64.pos.vz <= 0x200000) {
        o->t8A = 0;
        o->vel.z = D_800DDE64.vz;
        o->t.radius = 0x40;
        o->t.update = snobo2_obj_run_ahead_a;
    }
}

void snobo2_obj_run_ahead_a(Obj *o)
{
    int rz, v, z;

    if (o->hit) {
        o->t.radius = 0;
        o->vel.x = o->knock_x * 3 / 4;
        o->vel.y = -0x20000;
        o->vel.z = o->knock_z * 3 / 4;
        o->t.update = snobo2_obj_slider_fly;
        snobo2_play_effect(3, 0x135, 0);
        return;
    }
    rz = D_800DDE64.pos.vz;
    v = o->vel.z;
    z = rz < o->t.pos.z ? v - 0x400 : v + 0x800;
    if (z < 0x4000)
        z = 0x4000;
    o->vel.z = z;
    z = o->t.pos.z + o->vel.z;
    v = rz - z;
    if (D_800DDE64.speed > 0x200) {
        if (v > 0x80000) {
            z = rz - 0x80000;
            if (o->vel.z < D_800DDE64.vz)
                o->vel.z = D_800DDE64.vz;
        } else if (v < -0x1000000) {
            z = rz + 0x1000000;
        }
    }
    o->t.pos.z = z;
    o->t.pos.y = SLOPE_Y(z);
    o->t.pos.x += rcos(o->t84 += 0x10) << 4;
    if (o->t.arg[0] < z) {
        o->t86 = 0;
        o->t.update = snobo2_obj_spin_out_a;
    }
}

void snobo2_obj_spin_out_a(Obj *o)
{
    int c, z, y;

    if (o->hit) {
        o->t.radius = 0;
        o->vel.x = o->knock_x * 3 / 4;
        o->vel.y = -0x20000;
        o->vel.z = o->knock_z * 3 / 4;
        o->t.update = snobo2_obj_slider_fly;
        snobo2_play_effect(3, 0x135, 0);
        return;
    }
    c = rcos(o->t84 += 0x10);
    o->t.rot.pad = 0;
    z = o->t.pos.z + o->vel.z;
    y = SLOPE_Y(z);
    o->t.pos.x += c << 3;
    o->t.pos.z = z;
    o->t.rot.vy += (o->t88 += 8);
    o->t.pos.y = y;
    if (o->t86++ >= 0x1F) {
        o->vel.y = -0x20000;
        o->t.radius = 0;
        o->t.update = snobo2_obj_slider_fly;
    }
}

void snobo2_place_crosser(short id, Place *r)
{
    Pos *p;
    Obj *o;

    if (D_801053A4)
        return;
    D_800DFC08.rot.vy = r->ry;
    p = &D_800C5A8C[id];
    D_800DFC08.pos.x = (r->x + p->x) << 12;
    D_800DFC08.pos.y = (r->y + p->y) << 12;
    D_800DFC08.pos.z = (r->z + p->z) << 12;
    o = obj_spawn(id, &D_800DFC08, 2);
    o->t8A = -1;
    /* Turned round, it runs the other way (snobo2_obj_wait_close_b). */
    o->t84 = !(r->ry & 0x800);
}

void snobo2_obj_wait_close_b(Obj *o)
{
    int v;

    if (o->t.pos.z - D_800DDE64.pos.vz <= 0x700000) {
        v = D_800DDE64.vz >> 1;
        if (v <= 0x7FFF)
            v = 0x8000;
        o->vel.z = v;
        if (o->t84)
            v = -v;
        o->t.radius = 0x40;
        o->vel.x = v;
        o->t8A = 0;
        o->t.update = snobo2_obj_run_across;
    }
}

void snobo2_obj_run_across(Obj *o)
{
    int z, y, x;

    if (o->hit) {
        snobo2_obj_drop_item(o);
        return;
    }
    x = (D_800DDE64.pos.vz - o->t.pos.z) >> 12;
    if (x > 0x200) {
        o->active = 0;
        return;
    }
    z = REREAD(o->t.pos.z) + o->vel.z;
    y = SLOPE_Y(z);
    o->t.pos.z = z;
    o->t.pos.x += o->vel.x;
    x = (unsigned)REREAD(o->t.pos.x) >> 12;
    o->t.pos.y = y;
    if (o->t84) {
        if (x > 0xF0363)
            return;
        o->t.rot.pad = 0;
        o->vel.y = -0x10000;
        o->t.rot.vy += 0x80;
        if (x > 0xEFDE5)
            return;
    } else {
        if (x <= 0xF0364)
            return;
        o->t.rot.pad = 0;
        o->vel.y = -0x10000;
        o->t.rot.vy -= 0x80;
        if (x <= 0xF08E5)
            return;
    }
    o->t.update = snobo2_obj_tumble_down;
}

void snobo2_obj_drop_item(Obj *o)
{
    Obj *p;

    D_800DFC50.pos.x = o->t.pos.x;
    D_800DFC50.pos.y = o->t.pos.y;
    D_800DFC50.pos.z = o->t.pos.z;
    D_800DFC50.rot.vx = o->t.rot.vx;
    D_800DFC50.rot.vy = o->t.rot.vy;
    p = obj_spawn(D_80106A30, &D_800DFC50, 1);
    p->vel.x = o->vel.x * 4 / 5;
    p->vel.y = -0x22000;
    p->vel.z = o->vel.z * 4 / 5;
    o->hit = 0;
    o->vel.x = o->knock_x * 3 / 4;
    o->vel.y = -0x20000;
    o->vel.z = o->knock_z * 3 / 4;
    o->t.model = 0xC9;
    o->t.frame = 0xD0;
    o->t.radius = 0;
    o->t.update = snobo2_obj_fly_land;
    snobo2_play_effect(3, 0x135, 0);
}

void snobo2_obj_fly_land(Obj *o)
{
    int *rz = &D_800DDE64.pos.vz;
    int z, y, ny, d;

    if ((*rz - o->t.pos.z) >> 12 > 0x200) {
        o->active = 0;
        return;
    }
    z = REREAD(o->t.pos.z) + o->vel.z;
    y = SLOPE_Y(z);
    o->t.pos.x += o->vel.x;
    o->t.pos.z = z;
    ny = (o->vel.y += 0x1000) + o->t.pos.y;
    if (o->vel.y > 0 && ny > y - 0x40000) {
        /* Lands: it stays on the slope, its id from how far down it is. */
        o->active = 1;
        o->t.model = 0xD5;
        o->t.frame = 0;
        o->t.radius = 0x40;
        o->t.rot.vx = -0xAA;
        o->t.update = snobo2_obj_cycle_models;
        o->t.pos.y = y;
        o->t.rot.vy = 0;
        o->t.rot.pad = 0;
        o->t.low = 0x40;
        o->t.top = 0x40;
        o->t.react[2] = o->t.react[3] = 0xA;
        o->id = ((o->t.pos.z >> 12) - 45122) / 1483 + 0x8E;
        d = o->t.pos.z - *rz;
        /* A thud, fainter the further ahead of the rider it lands. */
        if (d >> 12 > 0 && d >> 12 < 0x1000)
            snobo2_play_effect(4, 0x1AB, 0x7F - (d >> 17));
        return;
    }
    o->t.pos.y = ny;
    if (o->t86++ < 0x40) {
        o->t.rot.pad = 0;
        o->t.rot.vx -= 0x20;
    }
}

void snobo2_obj_fall_spin(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F)
        o->active = 0;
    o->t.rot.pad = 0;
    o->t.pos.x += o->vel.x;
    o->t.pos.y = (o->vel.y += 0x1000) + o->t.pos.y;
    o->t.pos.z += o->vel.z;
    o->t.rot.vx -= 0x200;
}

void snobo2_place_runner_c(short id, Place2 *r)
{
    Pos *p;
    Obj *o;

    if (D_801053A4)
        return;
    D_800DFC98.rot.vy = r->ry;
    p = &D_800C5A8C[id];
    D_800DFC98.pos.x = (r->x + p->x) << 12;
    D_800DFC98.pos.y = (r->y + p->y) << 12;
    D_800DFC98.pos.z = (r->z + p->z) << 12;
    o = obj_spawn(id, &D_800DFC98, 2);
    o->t84 = r->arg[0];
    o->t.arg[0] = (r->arg[1] << 12) + o->t.pos.z;
    o->t8A = -1;
}

void snobo2_obj_wait_close_c(Obj *o)
{
    if (o->t.pos.z - D_800DDE64.pos.vz <= 0x200000) {
        o->t8A = 0;
        o->vel.z = D_800DDE64.vz;
        o->t.radius = 0x40;
        o->t.update = snobo2_obj_run_ahead_c;
    }
}

void snobo2_obj_run_ahead_c(Obj *o)
{
    int rz, v, z;

    if (o->hit) {
        snobo2_obj_drop_item(o);
        return;
    }
    rz = D_800DDE64.pos.vz;
    v = o->vel.z;
    z = rz < o->t.pos.z ? v - 0x400 : v + 0x800;
    if (z < 0x4000)
        z = 0x4000;
    o->vel.z = z;
    z = o->t.pos.z + o->vel.z;
    v = rz - z;
    if (D_800DDE64.speed > 0x200) {
        if (v > 0x80000) {
            z = rz - 0x80000;
            if (o->vel.z < D_800DDE64.vz)
                o->vel.z = D_800DDE64.vz;
        } else if (v < -0x1000000) {
            z = rz + 0x1000000;
        }
    }
    o->t.pos.z = z;
    o->t.pos.y = SLOPE_Y(z);
    o->t.pos.x += rcos(o->t84 += 0x20) << 5;
    if (o->t.arg[0] < z) {
        o->t86 = 0;
        o->t.update = snobo2_obj_spin_out_c;
    }
}

void snobo2_obj_spin_out_c(Obj *o)
{
    int c, z, y;

    if (o->hit) {
        o->t.radius = 0;
        o->vel.x = o->knock_x * 3 / 4;
        o->vel.y = -0x20000;
        o->vel.z = o->knock_z * 3 / 4;
        o->t.update = snobo2_obj_slider_fly;
        snobo2_play_effect(3, 0x135, 0);
        return;
    }
    c = rcos(o->t84 += 0x10);
    o->t.rot.pad = 0;
    z = o->t.pos.z + o->vel.z;
    y = SLOPE_Y(z);
    o->t.pos.x += c << 3;
    o->t.pos.z = z;
    o->t.rot.vy += (o->t88 += 8);
    o->t.pos.y = y;
    if (o->t86++ >= 0x1F) {
        o->vel.y = -0x20000;
        o->t.radius = 0;
        o->t.update = snobo2_obj_fly_land;
    }
}

void snobo2_place_crate(short id, Place1 *r)
{
    Pos *p;

    D_800DFCE0.arg[0] = r->arg[0];
    if (D_801053A4)
        return;
    D_800DFCE0.rot.vy = r->ry;
    p = &D_800C5A8C[id];
    D_800DFCE0.pos.x = (r->x + p->x) << 12;
    D_800DFCE0.pos.y = (r->y + p->y) << 12;
    D_800DFCE0.pos.z = (r->z + p->z) << 12;
    obj_spawn(id, &D_800DFCE0, 1);
}

void snobo2_obj_burst3(Obj *o)
{
    Obj *p;

    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        snobo2_play_effect(3, 0xF0, 0);
        o->vel.x = -0x1260;
        o->vel.z = -0x1A40;
        o->vel.y = -0x24000;
        o->t.radius = 0;
        o->t.update = snobo2_obj_fall;
        o->t.model++;
        /* Two more pieces flying off to the sides. */
        if (!(p = obj_alloc()))
            return;
        *p = *o;
        p->vel.x = -o->vel.z;
        o->vel.y = -0x24800;
        p->vel.z = o->vel.x;
        p->t.model = o->t.model + 1;
        if (!(p = obj_alloc()))
            return;
        *p = *o;
        p->vel.x = o->vel.z;
        o->vel.y = -0x23800;
        p->vel.z = -o->vel.x;
        p->t.model = o->t.model + 2;
        /* What was inside. */
        switch (o->t.arg[0]) {
        case 1:
            snobo2_spawn_thrower(o);
            break;
        case 2:
            snobo2_spawn_burst_flyer(o);
            break;
        case 3:
            snobo2_spawn_spinner(o, 0);
            break;
        case 4:
            snobo2_spawn_spinner(o, 1);
            break;
        }
    } else {
        o->t.update = snobo2_obj_expire;
    }
}

void snobo2_obj_fall(Obj *o)
{
    if (o->t84++ >= 0x29 && o->t8A++ >= 0x1F) {
        o->active = 0;
        return;
    }
    o->t.pos.x += o->vel.x;
    o->t.pos.y += o->vel.y;
    o->t.pos.z += o->vel.z;
    o->vel.y += 0x1000;
}

void snobo2_spawn_thrower(Obj *o)
{
    Obj *thrower, *p;

    /* A thrower, a blinker on its spot and the two things it throws at the rider (snobo2_obj_thrower_wait); it
     * keeps the three in arg[0-2]. */
    D_800DFD28.model = 0xD8;
    D_800DFD28.frame = 0xD9;
    D_800DFD28.pos.x = o->t.pos.x;
    D_800DFD28.pos.y = o->t.pos.y;
    D_800DFD28.pos.z = o->t.pos.z;
    D_800DFD28.ground = D_800DFD28.pos.y + 0x600000;
    D_800DFD28.update = snobo2_obj_thrower_wait;
    thrower = obj_spawn(o->id, &D_800DFD28, 1);
    D_800DFD28.model = 0;
    D_800DFD28.frame = 0;
    D_800DFD28.update = snobo2_obj_blink;
    thrower->t.arg[0] = (int)obj_spawn(o->id, &D_800DFD28, 1);
    D_800DFD28.model = 0xDC;
    D_800DFD28.frame = 0;
    D_800DFD28.pos.x = o->t.pos.x;
    D_800DFD28.pos.y = o->t.pos.y - 0x22000;
    D_800DFD28.pos.z = o->t.pos.z - 0x80000;
    D_800DFD28.update = 0;
    thrower->t.arg[1] = (int)(p = obj_spawn(o->id, &D_800DFD28, 1));
    p->vel.y = -0x18000;
    D_800DFD28.model = 0xDD;
    D_800DFD28.frame = 0;
    D_800DFD28.update = 0;
    thrower->t.arg[2] = (int)(p = obj_spawn(o->id, &D_800DFD28, 1));
    p->vel.y = -0x1E000;
}

void snobo2_obj_blink(Obj *o)
{
    int t = o->t84++;

    /* Blinks before it goes. */
    if (t >= 0x3D) {
        if (t & 4)
            o->t.model = 0xDE;
        else
            o->t.model = 0;
    }
}

void snobo2_obj_thrower_wait(Obj *o)
{
    Obj *p;

    if (o->t84++ >= 0x79) {
        o->t.model = 0xDA;
        o->t.frame = 0xDB;
        o->t.update = snobo2_obj_thrower;
        o->t84 = 0;
        o->t.radius = 0x40;
        o->t.top = 0x80;
        p = (Obj *)o->t.arg[1];
        p->t.update = snobo2_obj_thrown;
        p->t.radius = 0xC0;
        p->t.top = 0x40;
        p->t.react[0] = p->t.react[1] = p->t.react[2] = p->t.react[3] = 0xC;
        p->vel.x = (D_800DDE64.pos.vx - p->t.pos.x) >> 4;
        p->vel.z = (D_800DDE64.pos.vz - p->t.pos.z) >> 4;
        p = (Obj *)o->t.arg[2];
        p->t.update = snobo2_obj_dropped;
        p->vel.x = (0x10 - (rand() & 0x1F)) << 11;
    }
}

void snobo2_obj_thrower(Obj *o)
{
    if (o->hit) {
        if (o->hit & 2) {
            o->vel.y = -0x20000;
            o->t.model = 0xCA;
            o->t.frame = 0xD0;
            o->hit = 0;
            o->t.radius = 0;
            o->vel.x = o->knock_x;
            o->vel.z = o->knock_z;
            ((Obj *)o->t.arg[0])->active = 0;
            o->t.update = snobo2_obj_fly_land;
            snobo2_play_effect(3, 0x135, 0);
        } else {
            o->t84 = 0;
            o->t.update = snobo2_obj_thrower_fade;
        }
        return;
    }
    if (++o->t84 == 0x3D) {
        o->t.model = 0xD8;
        o->t.frame = 0xD9;
    }
}

void snobo2_obj_thrower_fade(Obj *o)
{
    Obj *p;

    if (o->t84++ >= 0x3D) {
        p = (Obj *)o->t.arg[0];
        if (o->t8A++ >= 0x1F) {
            o->active = 0;
            p->active = 0;
        } else {
            p->t8A = o->t8A;
        }
    }
}

void snobo2_obj_thrown(Obj *o)
{
    int y, ny;

    o->t.pos.z += o->vel.z;
    y = SLOPE_Y(REREAD(o->t.pos.z));
    o->t.pos.x += o->vel.x;
    ny = (o->vel.y += 0x1000) + o->t.pos.y;
    if (ny > y) {
        ny = y;
        o->t.radius = 0;
        o->t.update = snobo2_obj_expire;
    }
    o->t.pos.y = ny;
    if (o->t86++ < 0x15) {
        o->t.rot.pad = 0;
        o->t.rot.vx += 0x60;
    }
    if (o->hit) {
        o->t.radius = 0;
        o->hit = 0;
        snobo2_play_effect(3, 0xF0, 0);
    }
}

void snobo2_obj_dropped(Obj *o)
{
    int y, ny;

    o->t.pos.z -= 0x4000;
    y = SLOPE_Y(REREAD(o->t.pos.z)) + 0x20;
    o->t.pos.x += o->vel.x;
    ny = (o->vel.y += 0x1000) + o->t.pos.y;
    if (ny > y) {
        ny = y;
        o->t.update = snobo2_obj_expire;
    }
    o->t.pos.y = ny;
}

void snobo2_spawn_burst_flyer(Obj *o)
{
    Obj *p;

    COPY_QUAD(&D_800DFD70.pos, &o->t.pos);
    p = obj_spawn(D_80106A30, &D_800DFD70, 1);
    COPY_QUAD(&p->vel, &o->vel);
}

void snobo2_spawn_spinner(Obj *o, int dir)
{
    Obj *p;

    D_800DFDB8.arg[0] = dir ? 0x20 : -0x20; /* its turn a frame (snobo2_obj_roll_downhill) */
    COPY_QUAD(&D_800DFDB8.pos, &o->t.pos);
    p = obj_spawn(D_80106A30, &D_800DFDB8, 1);
    COPY_QUAD(&p->vel, &o->vel);
}

void snobo2_obj_spinner_fly(Obj *o)
{
    int z, y, ny;

    if ((D_800DDE64.pos.vz - o->t.pos.z) >> 12 > 0x200) {
        o->active = 0;
        return;
    }
    z = REREAD(o->t.pos.z) + o->vel.z;
    y = SLOPE_Y(z);
    o->t.pos.x += o->vel.x;
    o->t.pos.z = z;
    ny = (o->vel.y += 0x1000) + o->t.pos.y;
    if (o->vel.y > 0 && ny > y - 0x40000) {
        o->active = 1;
        o->t.model = 0xDB;
        o->t.frame = 0xDA;
        o->t.radius = 0x40;
        o->t.low = 0x40;
        o->t.top = 0x40;
        o->t.react[2] = o->t.react[3] = 0xA;
        o->t.pos.y = y;
        o->t.rot.vx = 0;
        o->t.rot.vy = 0;
        o->t.rot.pad = 0;
        o->t86 = 0;
        o->t84 = 0;
        o->t.update = (void (*)(Obj *))snobo2_obj_roll_downhill;
        snobo2_play_effect(3, 0x38, 0);
        return;
    }
    o->t.pos.y = ny;
    if (o->t86++ < 0x40) {
        o->t.rot.pad = 0;
        o->t.rot.vx -= 0x40;
    }
}

#ifdef NON_MATCHING
/* Scheduling: the game keeps the turned angle in a3 and reloads pos.x for the range test before storing pos.z. */
void snobo2_obj_roll_downhill(Obj *o, Work *w)
{
    int ry;

    if (o->hit) {
        if (o->hit & 2) {
            o->vel.y = -0x20000;
            o->t.model = 0xCA;
            o->t.frame = 0xD0;
            o->hit = 0;
            o->t.radius = 0;
            o->t.update = snobo2_obj_fly_land;
            o->vel.x = o->knock_x;
            o->vel.z = o->knock_z;
            snobo2_play_effect(3, 0x135, 0);
        } else {
            o->t84 = 0;
            o->t.update = snobo2_obj_expire;
        }
        return;
    }
    if (o->t86) {
        if (o->t.rot.vx != 0x400) {
            o->t.rot.vx += 0x80;
            o->t.rot.pad = 0;
        }
        return;
    }
    if (o->t84++ < 0xF)
        return;
    o->t.rot.pad = 0;
    ry = (short)(o->t.rot.vy + o->t.arg[0]);
    o->t.rot.vy = ry;
    rot_y(&w->lm, ry);
    w->sv.vx = 0;
    w->sv.vz = -0xC00;
    ApplyMatrix(&w->lm, &w->sv, (VECTOR *)w->lm.t);
    o->t.pos.x += w->lm.t[0] << 4;
    o->t.pos.z += w->lm.t[2] << 4;
    /* the slope's height under it */
    o->t.pos.y = (((o->t.pos.z >> 12) - 0xB042) * 22629 / 84531 + 0x10A36) << 12;
    if ((unsigned)o->t.pos.x >> 12 < 0xEFDA0 || (unsigned)o->t.pos.x >> 12 > 0xF0900) {
        o->t86 = 1;
        /* the game sets these and overwrites the model at once */
        o->t.model = 0xE3;
        o->t.frame = 0xE4;
    }
    o->t.model = 0xC9;
    o->t.frame = snobo2_roll_frame(o->t84 >> 1);
}
#else
INCLUDE_ASM("asm/overlay/snobo2/nonmatchings/snobo2", snobo2_obj_roll_downhill);
#endif

void snobo2_place_swing_pair(short id, Place4 *ev)
{
    Pos *p;
    Obj *o;

    if (D_801053A4)
        return;
    if (ev->arg[3]) {
        D_800DFE00.model = 0xE8;
        D_800DFE00.radius = 0xA0;
        D_800DFE00.top = 0x100;
        D_800DFE00.low = 0xE0;
    } else {
        D_800DFE00.model = 0xE9;
        D_800DFE00.radius = 0x60;
        D_800DFE00.top = 0xC0;
        D_800DFE00.low = 0xA0;
    }
    D_800DFE00.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800DFE00.pos.x = (ev->x + p->x) << 12;
    D_800DFE00.pos.y = (ev->y + p->y) << 12;
    D_800DFE00.pos.z = (ev->z + p->z) << 12;
    /* neither spawn checks for a full pool: the game writes through 0 then */
    o = obj_spawn(id, &D_800DFE00, 1);
    o->t.pos.y += ev->arg[3] ? -0x80000 : -0x40000;
    D_800DFE48.pos.y = o->t.pos.y + (ev->arg[3] ? -0x80000 : -0x40000);
    D_800DFE48.rot.vy = ev->ry;
    o->t.arg[0] = (int)obj_spawn(id, &D_800DFE48, 1);
    o->t.arg[1] = ev->arg[1];
    o->t.arg[2] = ev->arg[2];
    o->t86 = ev->arg[0];
}

/* A frame of the snow-ball's roll (D_800DFE90's six); the game also inlines it. */
inline short snobo2_roll_frame(int i)
{
    return D_800DFE90[i % 6];
}

void snobo2_obj_swing_pair(Obj *o, Work *w)
{
    Obj *s = (Obj *)o->t.arg[0];
    int sh;

    if (o->t84) {
        if (o->t86++ >= 0x3D) {
            o->t8A++;
            s->t8A++;
            if (o->t8A >= 0x1F) {
                o->active = 0;
                s->active = 0;
                return;
            }
        }
    } else if (o->hit) {
        D_80105374 = o->id;
        if (o->hit & 2) {
            o->t.radius = 0;
            w->sv.vx = o->knock_x >> 12;
            w->sv.vz = o->knock_z >> 12;
            rot_y(&w->lm, 0x100);
            ApplyMatrix(&w->lm, &w->sv, (VECTOR *)w->lm.t);
            o->vel.x = w->lm.t[0] << 12;
            o->vel.y = -0x28000;
            o->vel.z = w->lm.t[2] << 12;
            o->t.update = snobo2_obj_fly_spin;
            rot_y(&w->lm, 0xF00);
            ApplyMatrix(&w->lm, &w->sv, (VECTOR *)w->lm.t);
            s->vel.x = w->lm.t[0] << 12;
            s->vel.y = -0x28000;
            s->vel.z = w->lm.t[2] << 12;
            s->t.model = 0xCA;
            s->t.update = snobo2_obj_fly_spin;
            s->t.frame = 0xD0;
            snobo2_play_effect(3, 0x135, 0);
        } else
            o->t84 = 1;
    }
    w->sv.vx = 0;
    w->sv.vy = 0;
    sh = o->t.arg[1];
    w->sv.vz = rcos(o->t86++ << sh);
    o->t.rot.vz = rsin(o->t86 << sh);
    o->t.rot.pad = 0;
    rot_y(&w->lm, (unsigned short)o->t.rot.vy + 0x400);
    ApplyMatrixSV(&w->lm, &w->sv, &w->sv);
    sh = o->t.arg[2];
    s->t.pos.x = o->t.pos.x += w->sv.vx << sh;
    s->t.pos.z = o->t.pos.z += w->sv.vz << sh;
    s->t.frame = snobo2_roll_frame((short)(s->t86++ >> 2));
}

void snobo2_obj_fly_spin(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F) {
        o->active = 0;
        return;
    }
    o->t.pos.x += o->vel.x;
    o->t.rot.pad = 0;
    o->id = D_80106A30;
    o->t.pos.y += o->vel.y += 0x1000;
    o->t.pos.z += o->vel.z;
    o->t.rot.vx += 0x80;
}

void snobo2_place_hopper(short id, Place1 *ev)
{
    Pos *p;

    if (D_801053A4)
        return;
    D_800DFE9C.model = ev->arg[0] + 0xE6;
    D_800DFE9C.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800DFE9C.pos.x = (ev->x + p->x) << 12;
    D_800DFE9C.pos.y = (ev->y + p->y) << 12;
    D_800DFE9C.pos.z = (ev->z + p->z) << 12;
    D_800DFE9C.ground = D_800DFE9C.pos.y;
    obj_spawn(id, &D_800DFE9C, 1);
}

void snobo2_obj_hop_wait(Obj *o)
{
    if (o->hit) {
        if (o->hit & 2) {
            snobo2_obj_hopper_knock(o);
            return;
        }
        o->t.update = snobo2_obj_expire;
    }
    if (o->t84++ >= 0x10) {
        o->vel.y = -0x10000;
        o->t.update = snobo2_obj_hop;
    }
}

void snobo2_obj_hop(Obj *o)
{
    if (o->hit & 2) {
        snobo2_obj_hopper_knock(o);
        return;
    }
    o->t.pos.y += o->vel.y += 0x1000;
    if (o->t.pos.y > o->t.ground) {
        o->t.frame = 0xE5;
        o->t84 = 0;
        o->t.update = snobo2_obj_hop_wait;
        o->t.pos.y = o->t.ground;
    } else
        o->t.frame = 0xD0;
}

void snobo2_obj_hopper_knock(Obj *o)
{
    o->active = 2;
    o->vel.y = -0x20000;
    o->t84 = 0;
    o->t.radius = 0;
    o->t.update = snobo2_obj_hopper_fly;
    o->vel.x = o->knock_x;
    o->vel.z = o->knock_z;
    snobo2_play_effect(3, 0x135, 0);
}

void snobo2_obj_hopper_fly(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F) {
        o->active = 0;
        return;
    }
    o->t.rot.pad = 0;
    o->t.pos.x += o->vel.x;
    o->t.pos.y += o->vel.y += 0x1000;
    o->t.pos.z += o->vel.z;
    o->t.rot.vx += 0x80;
}

void snobo2_place_pickup(short id, Place1 *ev)
{
    Pos *p;

    if (ev->arg[0] != 4 && D_801053A4)
        return;
    D_800DFEE4.arg[0] = ev->arg[0];
    D_800DFEE4.shade = ((ev->arg[0] + 0x1F0) << 6 & 0xFFFF) | 0x2000000;
    D_800DFEE4.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800DFEE4.pos.x = (ev->x + p->x) << 12;
    D_800DFEE4.pos.y = (ev->y + p->y) << 12;
    D_800DFEE4.pos.z = (ev->z + p->z) << 12;
    D_800DFEE4.pos.y -= 0xC0000;
    obj_spawn(id, &D_800DFEE4, 1);
}

void snobo2_obj_pickup(Obj *o)
{
    short t;

    if (o->hit) {
        o->active = 0;
        snobo2_play_effect(3, 0xF9, 0);
        switch (o->t.arg[0]) {
        case 3:
#ifdef NATIVE
            snobo2_spawn_flash_a(0); /* the PS1 passes whatever a0 holds; the flash never needs its id */
#else
            snobo2_spawn_flash_a();
#endif
            D_801053A4 = 1;
            break;
        case 4:
#ifdef NATIVE
            snobo2_spawn_flash_b(0); /* the PS1 passes whatever a0 holds; the flash never needs its id */
#else
            snobo2_spawn_flash_b();
#endif
            D_80105388 = 1;
            break;
        default:
            D_801069BC += D_800DFF2C[o->t.arg[0]];
        }
        return;
    }
    t = o->t84++;
    o->t.model = (t >> 3 & 3) + 0xB8;
    o->t.pos.y += rsin(t << 5) * 2;
    if (D_80105374 && abs(D_80105374 - o->id) < 2) {
        o->t.update = snobo2_obj_pickup_rise;
        D_80105374 = 0;
    }
}

void snobo2_obj_pickup_rise(Obj *o)
{
    if (o->t88++ >= 0x3D && o->t8A++ >= 0x1F) {
        o->active = 0;
        return;
    }
    o->t.model = (o->t84++ >> 3 & 3) + 0xB8;
    o->t.pos.y += o->vel.y;
    o->vel.y -= 0x800;
}

void snobo2_spawn_flash_a(int id)
{
    D_800DFA50.update = snobo2_obj_flash_a;
    obj_spawn(id, &D_800DFA50, 2);
}

void snobo2_obj_flash_a(Obj *o)
{
    snobo2_draw_pulse_box_a(o->t84);
    if (o->t84++ >= 0x3D)
        o->active = 0;
}

void snobo2_spawn_flash_b(int id)
{
    D_800DFA50.update = snobo2_obj_flash_b;
    obj_spawn(id, &D_800DFA50, 2);
}

void snobo2_obj_flash_b(Obj *o)
{
    snobo2_draw_pulse_box_c(o->t84);
    if (o->t84++ >= 0x3D)
        o->active = 0;
}

void snobo2_place_topple_back(short id, Place1 *ev)
{
    Pos *p;

    if (D_801053A4)
        return;
    D_800DFF38.model = ev->arg[0] + 0xAE;
    D_800DFF38.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800DFF38.pos.x = (ev->x + p->x) << 12;
    D_800DFF38.pos.y = (ev->y + p->y) << 12;
    D_800DFF38.pos.z = (ev->z + p->z) << 12;
    obj_spawn(id, &D_800DFF38, 1);
}

void snobo2_obj_topple_idle_back(Obj *o)
{
    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        o->t.update = snobo2_obj_topple_back;
        snobo2_play_effect(1, 0x10C, 0x7F);
    } else {
        o->t.update = snobo2_obj_expire;
    }
}

void snobo2_obj_topple_back(Obj *o)
{
    o->t.rot.vx -= rsin(o->t86++ >> 1) >> 2;
    o->t.rot.pad = 0;
    if (o->t.rot.vx < -0x480)
        o->t.update = snobo2_obj_expire;
}

void snobo2_place_topple(short id, Place1 *ev)
{
    Pos *p;

    if (D_801053A4)
        return;
    D_800DFF80.model = ev->arg[0] + 0xAE;
    D_800DFF80.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800DFF80.pos.x = (ev->x + p->x) << 12;
    D_800DFF80.pos.y = (ev->y + p->y) << 12;
    D_800DFF80.pos.z = (ev->z + p->z) << 12;
    obj_spawn(id, &D_800DFF80, 1);
}

void snobo2_obj_topple_idle(Obj *o)
{
    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        o->t.update = snobo2_obj_topple;
        snobo2_play_effect(1, 0x10C, 0x7F);
    } else {
        o->t.update = snobo2_obj_expire;
    }
}

void snobo2_obj_topple(Obj *o)
{
    o->t.rot.vx += rsin(o->t86++ >> 1) >> 2;
    o->t.rot.pad = 0;
    if (o->t.rot.vx > 0x480)
        o->t.update = snobo2_obj_expire;
}

void snobo2_place_model(short id, Place2 *ev)
{
    Pos *p;

    D_800DFFC8.model = ev->arg[0];
    D_800DFFC8.frame = ev->arg[1];
    D_800DFFC8.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800DFFC8.pos.x = (ev->x + p->x) << 12;
    D_800DFFC8.pos.y = (ev->y + p->y) << 12;
    D_800DFFC8.pos.z = (ev->z + p->z) << 12;
    obj_spawn(id, &D_800DFFC8, 1);
}

void snobo2_place_f0(short id, Place *ev)
{
    Pos *p;

    if (D_801053A4)
        return;
    D_800E0010.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800E0010.pos.x = (ev->x + p->x) << 12;
    D_800E0010.pos.y = (ev->y + p->y) << 12;
    D_800E0010.pos.z = (ev->z + p->z) << 12;
    obj_spawn(id, &D_800E0010, 1);
}

void snobo2_place_f2(short id, Place1 *ev)
{
    Pos *p;

    if (D_801053A4)
        return;
    if (ev->arg[0]) {
        D_800E0058.model = 0xF2;
        D_800E0058.radius = 0x100;
    } else {
        D_800E0058.model = 0xF3;
        D_800E0058.radius = 0xC0;
    }
    D_800E0058.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800E0058.pos.x = (ev->x + p->x) << 12;
    D_800E0058.pos.y = (ev->y + p->y) << 12;
    D_800E0058.pos.z = (ev->z + p->z) << 12;
    obj_spawn(id, &D_800E0058, 1);
}

void snobo2_obj_touch2(Obj *o)
{
    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        snobo2_play_effect(2, 0x10C, 0);
    }
    o->t.update = snobo2_obj_expire;
}

void snobo2_place_leaner(short id, Place *ev)
{
    Pos *p;

    if (D_801053A4)
        return;
    D_800E00A0.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800E00A0.pos.x = (ev->x + p->x) << 12;
    D_800E00A0.pos.y = (ev->y + p->y) << 12;
    D_800E00A0.pos.z = (ev->z + p->z) << 12;
    obj_spawn(id, &D_800E00A0, 1);
}

void snobo2_obj_lean_idle(Obj *o)
{
    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        snobo2_play_effect(2, 0x10C, 0);
        o->t.update = D_801053B4 & 0x800 ? snobo2_obj_fall_back : snobo2_obj_fall_forward;
    } else
        o->t.update = snobo2_obj_expire;
}

void snobo2_obj_fall_back(Obj *o)
{
    o->t.rot.vx -= o->t84 += 8;
    o->t.rot.pad = 0;
    if (o->t.rot.vx < -0x500) {
        o->t84 = 0;
        o->t.update = snobo2_obj_expire;
    }
}

void snobo2_obj_fall_forward(Obj *o)
{
    o->t.rot.vx += o->t84 += 8;
    o->t.rot.pad = 0;
    if (o->t.rot.vx > 0x500) {
        o->t84 = 0;
        o->t.update = snobo2_obj_expire;
    }
}

void snobo2_place_drop(short id, Place3 *ev)
{
    Pos *p;

    if (D_801053A4)
        return;
    D_800E00E8.update = ev->arg[2] ? 0 : snobo2_obj_wake_near_rider;
    D_800E00E8.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800E00E8.pos.x = (ev->x + p->x) << 12;
    D_800E00E8.pos.y = (ev->y + p->y) << 12;
    D_800E00E8.pos.z = (ev->z + p->z) << 12;
    D_800E00E8.arg[0] = ev->arg[1];
    D_800E00E8.model = ev->arg[1] ? 0xF1 : 0xF6;
    D_800E00E8.ground = D_800E00E8.pos.y;
    D_800E00E8.pos.y -= ev->arg[0] << 12;
    obj_spawn(id, &D_800E00E8, 1);
}

void snobo2_obj_wake_near_rider(Obj *o)
{
    int dx, dz;

    o->t8A = -(o->id - D_80106A30 >= 3);
    dx = (D_800DDE64.pos.vx - o->t.pos.x) >> 12;
    dz = (D_800DDE64.pos.vz - o->t.pos.z) >> 12;
    if (SquareRoot0(dx * dx + dz * dz) < D_800DDE64.speed + 0x400) {
        o->t.update = snobo2_obj_drop;
        if (o->t.arg[0])
            o->t.radius = 0x180;
        else
            o->t.radius = 0x40;
        o->t.top = 0x100;
        o->t.low = 0x100;
    }
}

void snobo2_obj_drop(Obj *o)
{
    o->vel.y += 0x1000;
    o->t.pos.y += o->vel.y;
    if (o->t.pos.y > o->t.ground) {
        o->t.update = snobo2_obj_landed;
        o->t84 = 0x3C;
    }
    if (!o->t86 && o->t.pos.y > o->t.ground - 0x20000) {
        snobo2_play_effect(3, 0x1AB, 0);
        o->t86 = 1;
    }
    if (o->hit & 2) {
        snobo2_play_effect(2, 0xF0, 0);
        o->hit = 0;
        o->t.radius = 0;
    }
}

void snobo2_obj_landed(Obj *o)
{
    if (o->t84) {
        o->t84--;
        if (o->hit & 2) {
            snobo2_play_effect(2, 0xF0, 0);
            o->hit = 0;
            o->t.radius = 0;
        }
        return;
    }
    o->t.radius = 0;
    if (o->t8A++ >= 0x1F)
        o->active = 0;
}

void snobo2_place_circler(short id, Place1 *ev)
{
    Pos *p;
    Obj *o;

    if (D_801053A4)
        return;
    D_800E0130.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800E0130.pos.x = (ev->x + p->x) << 12;
    D_800E0130.pos.y = (ev->y + p->y) << 12;
    D_800E0130.pos.z = (ev->z + p->z) << 12;
    /* no check for a full pool: the game writes through 0 then */
    o = obj_spawn(id, &D_800E0130, 1);
    o->t88 = ev->arg[0];
    o->t.arg[0] = o->t.pos.x;
    o->t.arg[1] = o->t.pos.z;
    o->t.arg[2] = o->t.pos.y - 0x60000;
}

void snobo2_obj_circle(Obj *o)
{
    int a;

    if (snobo2_obj_floater_touch(o))
        return;
    a = (o->t86 << 5) + o->t88;
    o->t.pos.x = rsin(a) * 448 + o->t.arg[0];
    o->t.pos.z = rcos(a) * 448 + o->t.arg[1];
    o->t.pos.y = 0;
    o->t.rot.pad = 0;
    o->t.rot.vy += 0x20;
    o->t.pos.y = (rsin((o->t86 << 6) + o->t88) << 6) + o->t.arg[2];
    o->t86++;
}

/* The touch check of the floating pickups (also kept on its own): 1 once it is knocked away or gone. */
inline int snobo2_obj_floater_touch(Obj *o)
{
    short t = o->t84;

    if (t) {
        o->t84 = t + 1;
        if (t >= 0x3D && o->t8A++ >= 0x1F) {
            o->active = 0;
            return 1;
        }
        return 0;
    }
    if (o->hit) {
        if (o->hit & 2) {
            o->vel.x = o->knock_x;
            o->vel.z = o->knock_z;
            o->t84 = 0;
            o->t.radius = 0;
            o->t.update = snobo2_obj_knocked_spin;
            snobo2_play_effect(2, 0x10C, 0);
            return 1;
        }
        o->t84 = 1;
    }
    return 0;
}

/* Knocked away: flies off rising (y grows downwards), spinning faster each frame. */
void snobo2_obj_knocked_spin(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F)
        o->active = 0;
    o->t.rot.pad = 0;
    o->t.pos.x += o->vel.x;
    o->t.pos.z += o->vel.z;
    o->t.pos.y += o->vel.y -= 0xC00;
    o->t.rot.vy += o->t.arg[3];
    o->t.arg[3] += o->t.arg[3] < 0 ? -4 : 4;
}

/* A floating pickup (snobo2_obj_floater) at an event's placing. */
void snobo2_place_floater(short id, Place3 *ev)
{
    Obj *o;
    Pos *p;

    if (D_801053A4)
        return;
    D_800E0178.rot.vy = ev->ry;
    p = &D_800C5A8C[id];
    D_800E0178.pos.x = (ev->x + p->x) << 12;
    D_800E0178.pos.y = (ev->y + p->y) << 12;
    D_800E0178.pos.z = (ev->z + p->z) << 12;
    o = obj_spawn(id, &D_800E0178, 1);
    /* no check: a full pool writes through 0 */
    o->t88 = ev->arg[2];
    o->t.arg[3] = ev->arg[1];
    o->t.arg[2] = o->t.pos.y - (ev->arg[0] << 12);
}

/* A floating pickup: bobs and spins until it is touched. */
void snobo2_obj_floater(Obj *o)
{
    if (snobo2_obj_floater_touch(o))
        return;
    o->t.pos.y = (rsin((o->t86++ << 5) + o->t88) << 5) + o->t.arg[2];
    o->t.rot.pad = 0;
    o->t.rot.vy += o->t.arg[3];
}

void snobo2_board_tumble(void)
{
    int v;

    D_800DDE64.state = 11;
    v = D_800DDE64.vy;
    if (v < 0)
        v = -v;
    if (v < 0x40000)
        v = 0x40000;
    D_800DDE64.vy = v;
    snobo2_play_effect(2, 0x10C, 0);
}

void snobo2_clear_triggers(void)
{
    int i;
    int *p = D_801050F4.word;

    for (i = 159; i != -1; i--)
        *p++ = 0;
    D_80105374 = 0;
}

void snobo2_fire_trigger(id)
short id;
{
    int first = 0;

    if (!D_801050F4.seen[id]) {
        first = 1;
        D_801050F4.seen[id] = 1;
    }
    if (D_800E0F04[id])
        D_800E0F04[id](id, first);
}

void snobo2_clear_objs(void)
{
    int i;
    Obj *o = D_80103F70;

    for (i = 31; i != -1; i--) {
        o->active = 0;
        o++;
    }
    D_801050F0 = 0;
    D_80105378 = 0;
}

/* Moves, queues for drawing and hit-tests the course objects. */
void snobo2_update_objs(Work *v)
{
    Obj *o;
    Queued *e;
    int i, y, d, r, low;
    int *b = (int *)v->sz;

    v->idx = D_80106A30;
    v->f02 = D_80106B50;
    v->sz[0] = D_800DDE64.pos.vx >> 12;
    v->sz[1] = D_800DDE64.pos.vy >> 12;
    v->sz[2] = D_800DDE64.pos.vz >> 12;
    for (i = 0, o = D_80103F70; i < 32; i++, o++) {
        if (!o->active)
            continue;
        if (!(o->active & 2) && o->id < v->idx) {
            o->active = 0;
            D_801050F0 = i;
            continue;
        }
        if (v->f02 && o->t.update) {
            ((Update2)o->t.update)(o, v);
            if (!o->active)
                continue;
        }
        if (o->t8A & 1)
            continue;
        if (!o->t.rot.pad) {
            RotMatrix(&o->t.rot, &v->lm);
            y = o->t.scale;
            if (y) {
                short *p = v->lm.m[0];
                for (d = 8; d != -1; d--) {
                    *p = *p << y;
                    p++;
                }
            }
            o->mat = v->lm;
            o->t.rot.pad = 1;
        } else {
            v->lm = o->mat;
        }
        v->lm.t[0] = (o->t.pos.x >> 12) - v->origin.x;
        v->lm.t[1] = (o->t.pos.y >> 12) - v->origin.y;
        v->lm.t[2] = (o->t.pos.z >> 12) - v->origin.z;
        gte_SetRotMatrix(&v->m);
        gte_ldclmv(&v->lm.m[0][0]);
        gte_rtir();
        gte_stclmv(&v->lm.m[0][0]);
        gte_ldclmv(&v->lm.m[0][1]);
        gte_rtir();
        gte_stclmv(&v->lm.m[0][1]);
        gte_ldclmv(&v->lm.m[0][2]);
        gte_rtir();
        gte_stclmv(&v->lm.m[0][2]);
        gte_SetTransMatrix(&v->m);
        gte_ldlv0(v->lm.t);
        gte_rt();
        gte_stlvl(v->lm.t);

        if (o->t.model) {
            e = &D_801053B8[D_80105384++];
            e->m = v->lm;
            e->how = 0;
            e->model = o->t.model;
            e->drawer = o->t.drawer;
            e->shade = o->t.shade;
            if (o->t.frame) {
                Queued *e2 = &D_801053B8[D_80105384++];
                e2->how = 2;
                e2->model = o->t.frame;
                e2->drawer = o->t.drawer;
                e2->shade = o->t.shade;
            }
        }

        if (!o->t.radius)
            continue;
        if (!(o->active & 2) && o->id != v->idx)
            continue;
        switch ((unsigned char)D_800DDE64.state) {
        case 10:
        case 12:
        case 14:
            if (D_800DDE64.state_time < 8)
                continue;
        }
        y = o->t.pos.y >> 12;
        d = y - o->t.top;
        if (b[1] < d)
            continue;
        d = y + 0x100;
        if (d < b[1])
            continue;
        /* The cylinder test sits in a block of its own (a do-while(0)): its loop depth is what gives the radius,
         * not the object pointer, the first saved register, as the game has it. */
        do {
            r = o->t.radius;
            d = y - o->t.low;
            low = b[1] < d;
            y = b[0] - (o->t.pos.x >> 12);
            d = ABS(y);
            if (r < d)
                goto next;
            y = b[2] - (o->t.pos.z >> 12);
            y = ABS(y);
            if (r < y)
                goto next;
            if (r < SquareRoot0(d * d + y * y))
                goto next;
        } while (0);
        o->hit |= 1;
        if (o->t.react[0] == 0xFF)
            continue;
        snobo2_restore_rider();
        if (D_800DDE64.speed > 0x100) {
            o->hit |= 2;
            o->knock_x = D_800DDE64.vx;
            o->knock_z = D_800DDE64.vz;
            low += 2;
        }
        if (o->t.react[low]) {
            d = o->t.react[low];
            if (d == 0x80) {
                /* in the air already (states 11-15): the big tumble */
                if ((unsigned char)D_800DDE64.state >= 11 && (unsigned char)D_800DDE64.state < 16)
                    d = 12;
                else if (D_800DDE64.speed > 0x280)
                    d = 8;
                else
                    d = 12;
            } else if (d == 0x81) {
                d = 12;
                D_800DDE64.vx >>= 2;
                D_800DDE64.vz >>= 2;
            }
            if (d == 8) {
                if ((short)(D_801069D0 % 12) >= 6)
                    d = 9;
                D_800DDE64.vx -= D_800DDE64.vx >> 1;
                D_800DDE64.vz -= D_800DDE64.vz >> 1;
            }
            D_800DDE64.state = d;
        } else {
            D_800DDE64.vz = 0;
            D_800DDE64.vx = 0;
        }
    next:;
    }
}

void snobo2_obj_expire(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F)
        o->active = 0;
}

void snobo2_ev_B4414(int id, int on)
{
    if (!on || D_80106AF4[2] < 0x46)
        return;
    snobo2_place_pickup((short)id, &D_800E01C0);
}

/* The extra one goes up only for a time under 1:05; the rider's arrival marks the stretch once. */
void snobo2_ev_timed_B4458(int id, int on)
{
    if (on) {
        snobo2_place_model((short)id, &D_800E01CC);
        if (D_801053A4 && D_80106B08[0][0] && D_80106B08[0][0] < 0x1050000)
            snobo2_place_pickup((short)id, &D_800E01D8);
    }
    if (!D_80105398 && D_80106A30 == id) {
        D_80106A28 = 1;
        D_80105398 = 1;
    }
}

void snobo2_ev_B4524(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E01E4);
}

void snobo2_ev_B4554(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E01F0);
}

void snobo2_ev_B4584(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E01FC);
}

void snobo2_ev_B45B4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0208);
}

void snobo2_ev_B45E4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0214);
}

void snobo2_ev_B4614(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0220);
}

void snobo2_ev_B4644(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E022C);
}

void snobo2_ev_B4674(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0238);
}

void snobo2_ev_B46A4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0244);
}

void snobo2_ev_B46D4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0250);
}

void snobo2_ev_B4704(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E025C);
}

void snobo2_ev_B4734(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0268);
}

void snobo2_ev_B4764(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0274);
}

void snobo2_ev_B4794(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0280);
}

void snobo2_ev_B47C4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E028C);
}

void snobo2_ev_B47F4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0298);
}

void snobo2_ev_B4824(int id, int on)
{
    if (!on)
        return;
    snobo2_place_hopper((short)id, &D_800E02A4);
}

void snobo2_ev_B4854(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E02B0);
}

void snobo2_ev_B4884(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E02D0);
    snobo2_place_prop((short)id, &D_800E02BC[0]);
    snobo2_place_prop((short)id, &D_800E02BC[1]);
}

void snobo2_ev_B48E8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E02DC);
}

void snobo2_ev_B4918(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E02E8);
}

void snobo2_ev_B4948(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E02F4);
}

/* In the lanes of a jump's lip: a big or a small jump by his height above the ground. */
void snobo2_ev_lip_B4978(int id, int on)
{
    int h, st;

    if (on)
        snobo2_place_pickup((short)id, &D_800E0300);
    if (id != D_80106A30)
        return;
    if (D_801069D0 < 0x18 || D_801069D0 >= 0x30)
        return;
    if ((unsigned char)D_800DDE64.state == 10 || (unsigned char)D_800DDE64.state == 12)
        return;
    h = (D_800DDE64.ground_y - D_800DDE64.pos.vy) >> 12;
    if (h < 0x100)
        return;
    if (h < 0x1C0)
        st = 12;
    else if (h < 0x1C8)
        st = 10;
    else
        return;
    D_800DDE64.state = st;
    snobo2_play_effect(2, 0xF0, 0);
}

void snobo2_ev_B4A54(int id, int on)
{
    if (!on)
        return;
    snobo2_place_slider((short)id, &D_800E030C);
    snobo2_place_topple_back((short)id, &D_800E0314);
}

void snobo2_ev_B4AA0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_slider((short)id, &D_800E0320);
    snobo2_place_topple_back((short)id, &D_800E0328);
}

void snobo2_ev_B4AEC(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0334);
}

void snobo2_ev_B4B1C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crate((short)id, &D_800E0340);
    snobo2_place_runner_a((short)id, &D_800E034C);
}

void snobo2_ev_B4B68(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crate((short)id, &D_800E0358);
    snobo2_place_runner_a((short)id, &D_800E0364);
}

void snobo2_ev_B4BB4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0370);
}

void snobo2_ev_B4BE4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E037C);
}

void snobo2_ev_B4C14(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0388);
}

void snobo2_ev_B4C44(int id, int on)
{
    int i;

    if (on) {
        for (i = 0; i < 5; i++)
            snobo2_place_topple_back((short)id, &D_800E0394[i]);
    }
}

void snobo2_ev_B4CA4(int id, int on)
{
    int i;

    if (on) {
        for (i = 0; i < 6; i++)
            snobo2_place_topple_back((short)id, &D_800E03C8[i]);
    }
}

void snobo2_ev_B4D04(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crosser((short)id, &D_800E0404);
    snobo2_place_topple_back((short)id, &D_800E040C);
}

void snobo2_ev_B4D50(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crosser((short)id, &D_800E0418);
    snobo2_place_topple_back((short)id, &D_800E0420);
}

void snobo2_ev_B4D9C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E042C);
}

void snobo2_ev_B4DCC(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0438);
}

void snobo2_ev_B4DFC(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crate((short)id, &D_800E0444);
    snobo2_place_runner_c((short)id, &D_800E0450);
}

void snobo2_ev_B4E48(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crate((short)id, &D_800E045C);
    snobo2_place_runner_c((short)id, &D_800E0468);
}

void snobo2_ev_B4E94(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crate((short)id, &D_800E0474);
    snobo2_place_runner_c((short)id, &D_800E0480);
}

void snobo2_ev_B4EE0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E048C);
}

void snobo2_ev_B4F10(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crate((short)id, &D_800E0498);
}

void snobo2_ev_B4F40(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crate((short)id, &D_800E04A4[0]);
    snobo2_place_crate((short)id, &D_800E04A4[1]);
}

void snobo2_ev_B4F94(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crate((short)id, &D_800E04B8[0]);
    snobo2_place_crate((short)id, &D_800E04B8[1]);
}

void snobo2_ev_B4FE8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_crate((short)id, &D_800E04CC[0]);
    snobo2_place_crate((short)id, &D_800E04CC[1]);
}

/* In the lanes of a jump's lip: a big or a small jump by his height above the ground. */
void snobo2_ev_lip_B503C(int id, int on)
{
    int h, st;

    if (on)
        snobo2_place_pickup((short)id, &D_800E04E0);
    if (id != D_80106A30)
        return;
    if (D_801069D0 < 0x18 || D_801069D0 >= 0x30)
        return;
    if ((unsigned char)D_800DDE64.state == 10 || (unsigned char)D_800DDE64.state == 12)
        return;
    h = (D_800DDE64.ground_y - D_800DDE64.pos.vy) >> 12;
    if (h < 0x100)
        return;
    if (h < 0x1C0)
        st = 12;
    else if (h < 0x1C8)
        st = 10;
    else
        return;
    D_800DDE64.state = st;
    snobo2_play_effect(2, 0xF0, 0);
}

void snobo2_ev_B5118(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E04EC);
}

void snobo2_ev_B5148(int id, int on)
{
    if (!on)
        return;
    snobo2_place_model((short)id, &D_800E04F8);
}

void snobo2_ev_B5178(int id, int on)
{
    if (!on || !D_800C824C[2].closed)
        return;
    snobo2_place_model((short)id, &D_800E0504);
}

/* The extra one goes up only for a time under 1:20; the rider's arrival marks the stretch once. */
void snobo2_ev_timed_B51B8(int id, int on)
{
    if (on) {
        snobo2_place_model((short)id, &D_800E0510);
        snobo2_place_pickup((short)id, &D_800E051C);
        snobo2_place_f0((short)id, &D_800E0528);
        if (D_801053A4 && D_80106B08[1][0] && D_80106B08[1][0] < 0x1200000)
            snobo2_place_pickup((short)id, &D_800E0530);
    }
    if (!D_80105398 && D_80106A30 == id) {
        D_80106A28 = 2;
        D_80105398 = 1;
    }
}

void snobo2_ev_B52A4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f0((short)id, &D_800E053C);
    snobo2_place_pickup((short)id, &D_800E0544);
}

void snobo2_ev_B52F0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f0((short)id, &D_800E0550);
}

void snobo2_ev_B5320(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f0((short)id, &D_800E0558);
}

void snobo2_ev_B5350(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f0((short)id, &D_800E0560);
}

void snobo2_ev_B5380(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f0((short)id, &D_800E0568);
    snobo2_place_pickup((short)id, &D_800E0570);
}

void snobo2_ev_B53CC(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f0((short)id, &D_800E057C);
}

void snobo2_ev_B53FC(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f0((short)id, &D_800E0584);
}

void snobo2_ev_B542C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f0((short)id, &D_800E058C[0]);
    snobo2_place_f0((short)id, &D_800E058C[1]);
}

void snobo2_ev_B5480(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E059C);
}

void snobo2_ev_jump_B54B0(int id, int on)
{
    if (on)
    snobo2_place_leaner((short)id, &D_800E05A8);
    check_jump(id);
}

void snobo2_ev_jump_B5544(int id, int on)
{
    if (on)
    snobo2_place_leaner((short)id, &D_800E05B0);
    check_jump(id);
}

void snobo2_ev_jump_B55D8(int id, int on)
{
    if (on)
    snobo2_place_leaner((short)id, &D_800E05B8);
    check_jump(id);
}

void snobo2_ev_jump_B566C(int id, int on)
{
    if (on)
    snobo2_place_leaner((short)id, &D_800E05C0);
    check_jump(id);
}

void snobo2_ev_jump_B5700(int id, int on)
{
    if (on)
    snobo2_place_leaner((short)id, &D_800E05C8);
    check_jump(id);
}

void snobo2_ev_jump_B5794(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E05DC);
        snobo2_place_pickup((short)id, &D_800E05D0);
    }
    check_jump(id);
}

void snobo2_ev_jump_B5844(int id, int on)
{
    if (on)
    snobo2_place_leaner((short)id, &D_800E05E4);
    check_jump(id);
}

void snobo2_ev_jump_B58D8(int id, int on)
{
    if (on)
    snobo2_place_leaner((short)id, &D_800E05EC);
    check_jump(id);
}

void snobo2_ev_jump_B596C(int id, int on)
{
    if (on)
    snobo2_place_leaner((short)id, &D_800E05F4);
    check_jump(id);
}

void snobo2_ev_B5A00(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E05FC);
}

void snobo2_ev_jump_B5A30(int id, int on)
{
    if (on)
    snobo2_place_leaner((short)id, &D_800E0608);
    check_jump(id);
}

void snobo2_ev_jump_B5AC4(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E0610[0]);
        snobo2_place_leaner((short)id, &D_800E0610[1]);
    }
    check_jump(id);
}

void snobo2_ev_jump_B5B7C(int id, int on)
{
    if (on)
    snobo2_place_pickup((short)id, &D_800E0620);
    check_jump(id);
}

void snobo2_ev_jump_B5C10(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E062C[0]);
        snobo2_place_leaner((short)id, &D_800E062C[1]);
    }
    check_jump(id);
}

void snobo2_section_props_B5CC8(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E063C);
    }
    check_jump(id);
}

void snobo2_section_props_B5D5C(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E0644[0]);
        snobo2_place_leaner((short)id, &D_800E0644[1]);
    }
    check_jump(id);
}

void snobo2_section_props_B5E14(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E0654[0]);
        snobo2_place_leaner((short)id, &D_800E0654[1]);
    }
    check_jump(id);
}

void snobo2_section_props_B5ECC(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E0664);
    }
    check_jump(id);
}

void snobo2_section_props_B5F60(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E066C[0]);
        snobo2_place_leaner((short)id, &D_800E066C[1]);
        snobo2_place_pickup((short)id, &D_800E067C);
    }
    check_jump(id);
}

void snobo2_section_props_B6028(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E0688);
    }
    check_jump(id);
}

void snobo2_section_props_B60BC(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E0690);
    }
    check_jump(id);
}

void snobo2_ev_B6150(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0698);
}

void snobo2_section_props_B6180(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E06A4[0]);
        snobo2_place_leaner((short)id, &D_800E06A4[1]);
        snobo2_place_leaner((short)id, &D_800E06A4[2]);
    }
    check_jump(id);
}

void snobo2_section_props_B6244(int id, int on)
{
    check_jump(id);
}

void snobo2_section_props_B62AC(int id, int on)
{
    if (on) {
        snobo2_place_pickup((short)id, &D_800E06BC);
    }
    check_jump(id);
}

void snobo2_section_props_B6340(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E06C8[0]);
        snobo2_place_leaner((short)id, &D_800E06C8[1]);
        snobo2_place_leaner((short)id, &D_800E06C8[2]);
    }
    check_jump(id);
}

void snobo2_section_props_B6404(int id, int on)
{
    check_jump(id);
}

void snobo2_section_props_B646C(int id, int on)
{
    if (on) {
        snobo2_place_pickup((short)id, &D_800E06E0);
    }
    check_jump(id);
}

void snobo2_section_props_B6500(int id, int on)
{
    if (on) {
        snobo2_place_leaner((short)id, &D_800E06EC[0]);
        snobo2_place_leaner((short)id, &D_800E06EC[1]);
        snobo2_place_leaner((short)id, &D_800E06EC[2]);
    }
    check_jump(id);
}

void snobo2_ev_B65C4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0704);
}

void snobo2_ev_B65F4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0710);
}

void snobo2_ev_B6624(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E071C);
}

void snobo2_ev_B6654(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0728);
    snobo2_place_circler((short)id, &D_800E0734[0]);
    snobo2_place_circler((short)id, &D_800E0734[1]);
}

void snobo2_ev_B66B8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_floater((short)id, &D_800E0748[0]);
    snobo2_place_floater((short)id, &D_800E0748[1]);
}

void snobo2_ev_B670C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_floater((short)id, &D_800E0764[0]);
    snobo2_place_floater((short)id, &D_800E0764[1]);
}

void snobo2_ev_B6760(int id, int on)
{
    if (!on)
        return;
    snobo2_place_floater((short)id, &D_800E0780[0]);
    snobo2_place_floater((short)id, &D_800E0780[1]);
}

void snobo2_ev_B67B4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_floater((short)id, &D_800E079C[0]);
    snobo2_place_floater((short)id, &D_800E079C[1]);
}

void snobo2_ev_B6808(int id, int on)
{
    if (!on)
        return;
    snobo2_place_floater((short)id, &D_800E07B8);
}

void snobo2_ev_B6838(int id, int on)
{
    if (!on)
        return;
    snobo2_place_floater((short)id, &D_800E07C8[0]);
    snobo2_place_floater((short)id, &D_800E07C8[1]);
}

void snobo2_ev_B688C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_hopper((short)id, &D_800E07E4);
    snobo2_place_pickup((short)id, &D_800E07F0);
    snobo2_place_prop((short)id, &D_800E07FC[0]);
    snobo2_place_prop((short)id, &D_800E07FC[1]);
}

void snobo2_ev_B6900(int id, int on)
{
    if (!on)
        return;
    snobo2_place_sway((short)id, &D_800E0810);
}

void snobo2_ev_B6930(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E081C);
}

void snobo2_ev_B6960(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0828);
}

void snobo2_ev_B6990(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E0834);
}

void snobo2_ev_B69C0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0840);
    snobo2_place_prop((short)id, &D_800E084C);
}

void snobo2_ev_B6A0C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E0858);
}

void snobo2_ev_B6A3C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0864);
    snobo2_place_prop((short)id, &D_800E0870[0]);
    snobo2_place_prop((short)id, &D_800E0870[1]);
}

void snobo2_ev_B6AA0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E0884);
}

void snobo2_ev_B6AD0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E0890);
}

void snobo2_ev_B6B00(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E089C);
}

void snobo2_ev_B6B30(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E08A8);
}

void snobo2_ev_B6B60(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E08B4);
}

void snobo2_ev_B6B90(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E08C0);
}

void snobo2_ev_B6BC0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E08CC);
}

void snobo2_ev_B6BF0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E08D8[0]);
    snobo2_place_topple((short)id, &D_800E08D8[1]);
}

void snobo2_ev_B6C44(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E08EC[0]);
    snobo2_place_topple((short)id, &D_800E08EC[1]);
    snobo2_place_pickup((short)id, &D_800E0900);
}

void snobo2_ev_B6CA8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E090C);
}

void snobo2_ev_B6CD8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E0918);
}

void snobo2_ev_B6D08(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E0924);
}

void snobo2_ev_B6D38(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E0930);
}

void snobo2_ev_B6D68(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E093C);
}

void snobo2_ev_B6D98(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E0948[0]);
    snobo2_place_topple((short)id, &D_800E0948[1]);
}

void snobo2_ev_B6DEC(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E095C);
}

void snobo2_ev_B6E1C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E0968);
}

void snobo2_ev_B6E4C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_topple((short)id, &D_800E0974);
}

void snobo2_ev_B6E7C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0980);
    snobo2_place_sway((short)id, &D_800E098C);
}

void snobo2_ev_B6EC8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0998);
}

void snobo2_ev_B6EF8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_hopper((short)id, &D_800E09A4);
}

void snobo2_ev_B6F28(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E09B0);
}

void snobo2_ev_B6F58(int id, int on)
{
    if (!on)
        return;
    snobo2_place_hopper((short)id, &D_800E09BC);
}

void snobo2_ev_B6F88(int id, int on)
{
    if (!on)
        return;
    snobo2_place_sway((short)id, &D_800E09C8);
}

void snobo2_ev_B6FB8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_hopper((short)id, &D_800E09D4);
}

void snobo2_ev_B6FE8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E09E0);
}

void snobo2_ev_B7018(int id, int on)
{
    if (!on)
        return;
    snobo2_place_hopper((short)id, &D_800E09EC);
}

void snobo2_ev_B7048(int id, int on)
{
    if (!on)
        return;
    snobo2_place_sway((short)id, &D_800E09F8);
}

void snobo2_ev_B7078(int id, int on)
{
    if (!on)
        return;
    snobo2_place_hopper((short)id, &D_800E0A04);
}

void snobo2_ev_B70A8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0A10);
}

void snobo2_ev_B70D8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_hopper((short)id, &D_800E0A1C);
}

void snobo2_ev_B7108(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0A28);
}

void snobo2_ev_B7138(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0A34);
}

void snobo2_ev_B7168(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0A40);
}

void snobo2_ev_B7198(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0A4C);
}

void snobo2_ev_B71C8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_hopper((short)id, &D_800E0A58);
}

void snobo2_ev_B71F8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E0A64);
}

void snobo2_ev_B7228(int id, int on)
{
    if (!on)
        return;
    snobo2_place_sway((short)id, &D_800E0A70[0]);
    snobo2_place_sway((short)id, &D_800E0A70[1]);
}

void snobo2_ev_B727C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0A84);
}

void snobo2_ev_B72AC(int id, int on)
{
    if (!on)
        return;
    snobo2_place_model((short)id, &D_800E0A90);
}

void snobo2_ev_B72DC(int id, int on)
{
    if (on && D_800C824C[4].closed)
        snobo2_place_model((short)id, &D_800E0A9C);
}

void snobo2_ev_B731C(int id, int on)
{
    if (on) {
        snobo2_place_model((short)id, &D_800E0AA8);
        if (D_801053A4 && D_80106B08[2][0] && D_80106B08[2][0] < 0x1350000)
            snobo2_place_pickup((short)id, &D_800E0AB4);
    }
    if (!D_80105398 && D_80106A30 == id) {
        D_80106A28 = 3;
        D_80105398 = 1;
    }
}

void snobo2_ev_B73E8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0AC0[0]);
    snobo2_place_pickup((short)id, &D_800E0AC0[1]);
}

void snobo2_ev_B743C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0AD4[0]);
    snobo2_place_pickup((short)id, &D_800E0AD4[1]);
}

void snobo2_ev_B7490(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0AE8[0]);
    snobo2_place_pickup((short)id, &D_800E0AE8[1]);
}

void snobo2_ev_B74E4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0AFC[0]);
    snobo2_place_pickup((short)id, &D_800E0AFC[1]);
}

void snobo2_ev_B7538(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0B10);
}

void snobo2_ev_B7568(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0B1C);
}

void snobo2_ev_B7598(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0B28);
}

void snobo2_ev_B75C8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0B34);
}

void snobo2_ev_B75F8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0B40);
}

void snobo2_ev_B7628(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E0B4C);
}

void snobo2_ev_B7658(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0B58);
}

void snobo2_ev_B7688(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0B64);
}

void snobo2_ev_B76B8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0B74);
}

void snobo2_ev_B76E8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0B84);
}

void snobo2_ev_B7718(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0B94);
}

void snobo2_ev_B7748(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0BA4);
}

void snobo2_ev_B7778(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0BB4);
}

void snobo2_ev_B77A8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0BC4);
}

void snobo2_ev_B77D8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0BD4);
}

void snobo2_ev_B7808(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0BE4);
}

void snobo2_ev_B7838(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0BF4);
}

void snobo2_ev_B7868(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0C04);
}

void snobo2_ev_B7898(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0C14);
}

void snobo2_ev_B78C8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0C24);
}

void snobo2_ev_B78F8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0C34);
}

void snobo2_ev_B7928(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0C44);
}

void snobo2_ev_B7958(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0C54);
}

void snobo2_ev_B7988(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0C64);
}

void snobo2_ev_B79B8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0C74);
}

void snobo2_ev_B79E8(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0C84);
    snobo2_place_pickup((short)id, &D_800E0C94);
}

void snobo2_ev_B7A34(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0CA0);
}

void snobo2_ev_B7A64(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0CB0);
}

void snobo2_ev_B7A94(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0CC0);
}

void snobo2_ev_B7AC4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_drop((short)id, &D_800E0CD0);
    snobo2_place_pickup((short)id, &D_800E0CE0);
}

void snobo2_ev_B7B10(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0CEC);
}

void snobo2_ev_B7B40(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0CF8);
}

void snobo2_ev_nop_B7B70(int id, int on)
{
}

void snobo2_ev_B7B78(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0D04);
    snobo2_place_pickup((short)id, &D_800E0D10);
}

void snobo2_ev_B7BC4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0D1C);
}

void snobo2_ev_B7BF4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0D28);
}

void snobo2_ev_B7C24(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0D34);
}

void snobo2_ev_B7C54(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0D40);
}

void snobo2_ev_nop_B7C84(int id, int on)
{
}

void snobo2_ev_nop_B7C8C(int id, int on)
{
}

void snobo2_ev_B7C94(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0D4C);
}

void snobo2_ev_nop_B7CC4(int id, int on)
{
}

void snobo2_ev_nop_B7CCC(int id, int on)
{
}

void snobo2_ev_B7CD4(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0D58);
}

void snobo2_ev_B7D04(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0D64);
    snobo2_place_pickup((short)id, &D_800E0D70);
}

void snobo2_ev_nop_B7D50(int id, int on)
{
}

void snobo2_ev_nop_B7D58(int id, int on)
{
}

void snobo2_ev_B7D60(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0D7C);
}

void snobo2_ev_B7D90(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0D88);
    snobo2_place_pickup((short)id, &D_800E0D94);
}

void snobo2_ev_B7DDC(int id, int on)
{
    if (!on)
        return;
    snobo2_place_f2((short)id, &D_800E0DA0[0]);
    snobo2_place_f2((short)id, &D_800E0DA0[1]);
}

void snobo2_ev_B7E30(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E0DB4);
}

void snobo2_ev_B7E60(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0DC0);
}

void snobo2_ev_B7E90(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E0DCC);
}

void snobo2_ev_B7EC0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E0DD8);
}

void snobo2_ev_B7EF0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0DE4);
}

void snobo2_ev_B7F20(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E0DF0);
}

void snobo2_ev_B7F50(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E0DFC);
}

void snobo2_ev_B7F80(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0E08);
}

void snobo2_ev_B7FB0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_prop((short)id, &D_800E0E14);
}

void snobo2_ev_B7FE0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0E20);
}

void snobo2_ev_B8010(int id, int on)
{
    if (!on)
        return;
    snobo2_place_swing_pair((short)id, &D_800E0E2C);
    snobo2_place_pickup((short)id, &D_800E0E3C);
}

void snobo2_ev_B805C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_swing_pair((short)id, &D_800E0E48);
}

void snobo2_ev_B808C(int id, int on)
{
    if (!on)
        return;
    snobo2_place_swing_pair((short)id, &D_800E0E58[0]);
    snobo2_place_swing_pair((short)id, &D_800E0E58[1]);
    snobo2_place_pickup((short)id, &D_800E0E78);
}

void snobo2_ev_B80F0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_swing_pair((short)id, &D_800E0E84);
}

void snobo2_ev_B8120(int id, int on)
{
    if (!on)
        return;
    snobo2_place_swing_pair((short)id, &D_800E0E94[0]);
    snobo2_place_swing_pair((short)id, &D_800E0E94[1]);
    snobo2_place_pickup((short)id, &D_800E0EB4);
}

void snobo2_ev_B8184(int id, int on)
{
    if (!on)
        return;
    snobo2_place_swing_pair((short)id, &D_800E0EC0);
    snobo2_place_pickup((short)id, &D_800E0ED0);
}

void snobo2_ev_B81D0(int id, int on)
{
    if (!on)
        return;
    snobo2_place_swing_pair((short)id, &D_800E0EDC);
}

void snobo2_ev_B8200(int id, int on)
{
    if (!on)
        return;
    snobo2_place_pickup((short)id, &D_800E0EEC);
}

void snobo2_ev_B8230(int id, int on)
{
    if (!on)
        return;
    snobo2_place_model((short)id, &D_800E0EF8);
}
