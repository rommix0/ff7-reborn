/* MINI/SNOBO.BIN, the snowboard down to the Great Glacier: 0x800A0390-0x800BFD74 (its data follows to 0x800DD690,
 * then bss). One object: GCC 2.6.3 -O2 -G0 (no -fforce-addr: globals are reloaded by %hi/%lo every time), ASPSX 2.34
 * with its divide checks (2.21 can't fold an indexed global load into %lo). */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "gte.h"

int rand(void);
int abs(int x);
int rsin(int a);
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

/* Primitives written a word at a time: a colour word carries the GPU code in its top byte. */
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
typedef struct Sprite {
    unsigned int tag;
    unsigned int code; /* colour and command */
    unsigned int xy;
    unsigned int uvclut;
    unsigned int wh;
} Sprite;
typedef struct Fill {
    unsigned int tag;
    unsigned int code;
    unsigned int xy;
    unsigned int wh;
} Fill;
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

/* A piece of the slope (D_800C000C, 0x374 bytes): 7 rows of 7 points (the first row is the last of the piece before)
 * and their colours, then its ground as 72 triangles over them (6 by 6 quads, 2 each). The points' pads are a table of
 * their own: v[0-2].pad cut it into a grid of cells in x, v[3-5].pad in z, v[6].pad is the ground search's reach and
 * v[8 + n].pad the slope of face group n. */
typedef struct Piece {
    short kind;  /* the row of D_800BFF28 (its cells' handlers); 3: a fork */
    short pitch; /* the camera's pitch over it, / 3 */
    short bend;  /* it turns by this much over v[6].vx; its sign is the direction */
    short turn;  /* the angle it turns by */
    SVECTOR v[49];
    Rgb col[49];
    Face face[72];
} Piece;

/* A stretch of the course (D_800D37D4, 32 bytes each): its steps (each a piece), and per step the piece, flags, a
 * heading and where it starts. */
typedef struct Segment {
    short len;
    unsigned char pad02;
    unsigned char prev;    /* the stretch before, 0xFF none */
    unsigned char next[2]; /* at a fork: the branch, and the stretch it rejoins */
    short pad06;
    unsigned char *piece;  /* per step: its Piece in D_800C000C */
    unsigned char *flags;  /* per step: 0x80 the finish (and walls bounce), 8 fixed pitch, 4 ambience, 2 covered sky,
                            * 1 a jump */
    unsigned char *ahead;  /* per step: how many pieces to draw ahead */
    unsigned int *props;   /* per step: the kinds of the props on it, a byte each */
    short *angle;          /* per step: its heading */
    Pos *origin;           /* per step: where it starts */
} Segment;

/* The rider on his board (D_800DB1D4). */
typedef struct Board {
    int flags;            /* 0x00 bit 0: stopped (no steering, no motion) */
    short turn_target;    /* 0x04 */
    short turn_rate;      /* 0x06 */
    short turn_timer;     /* 0x08 */
    short carve_time;     /* 0x0A */
    unsigned short state; /* 0x0C low byte: its handler in D_800DB2E4 (read as a byte); bit 15: entered */
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

/* The course drawer's work, in the scratchpad (0x1F800000); the object update (snobo_update_objs) reuses it. The
 * points go in rows of 7; the *2 arrays are the last strip split in two. */
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
    short rejoin;       /* after a fork's branch, the stretch to carry on down; 0 none */
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

/* A place on the course being moved along (built in the scratchpad by snobo_move_along_course). */
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

/* The camera: its angles and the point it looks at (relative to D_8010284C). */
typedef struct Camera {
    SVECTOR ang;
    SVECTOR at;
} Camera;

/* One mark left along the course this frame (D_800FF784). */
typedef struct Mark {
    short seg;
    short idx;
    char pad04[4];
    Piece *slice;
    MATRIX m;
} Mark;

/* A model part queued for drawing (D_80101194, D_80102954 of them). */
typedef struct Queued {
    MATRIX m;
    short how;   /* 0: set the turn and the move, 1: the move only, 2: neither (a second part on the same matrix) */
    short model;
    int drawer;  /* which drawer (D_800DBA88) */
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
    void (*update)(Obj *o);
    int arg[4];           /* 0x38 each kind's own: a home position, a mode, a timer */
} Tmpl;

/* One of the 32 course objects (D_800FFDB0). */
struct Obj {
    unsigned char active;
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
    int drawer; /* D_800DBA88 */
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

/* A puff of snow thrown up by the board: eight in a ring (D_800FFC40, next at D_800FFD40). */
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

/* The scratchpad as the spray and the board's glow are drawn. */
typedef struct SprayWork {
    int otz;
    short sx, sy;
    char pad08[0x38];
    MATRIX view;
    MATRIX lw;
} SprayWork;
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

/* One joint of the rider's skeleton (D_800DB504, 19). */
typedef struct Joint {
    short m0[3][3];      /* the bone's direction, under the joint's world rotation */
    short pos_parent;    /* 0x12 the joint whose position this one adds to, or -1 */
    short m1[3][3];      /* 0x14 applied before the pose's rotation */
    short parent;        /* 0x26 the joint whose world rotation this one follows, or -1 */
    short m2[3][3];      /* 0x28 applied after it */
    short len;           /* 0x3A the bone's length */
    short no_bone;       /* 0x3C */
    short pad;
} Joint;

/* One drawn part of the rider (D_800DB9C4, 15): a model at a joint's position, turned like a joint. */
typedef struct SkelPart {
    short model;
    short drawer;
    short joint;     /* 0x04 rotation from this joint */
    short pos_joint; /* 0x06 position from this joint, or -1: the root */
} SkelPart;

/* The scratchpad as the skeleton code uses it. */
typedef struct SkelWork {
    RotMat world[19]; /* 0x000 */
    SVECTOR pos[19];  /* 0x17C */
    SVECTOR pts[15];  /* 0x214 */
    char pad28C[0x20];
    MATRIX view;      /* 0x2AC */
    MATRIX part;      /* 0x2CC */
    MATRIX view_model; /* 0x2EC */
    RotMat a;         /* 0x30C */
    RotMat b;         /* 0x320 */
    VECTOR v;         /* 0x334 */
    SVECTOR sv;       /* 0x344 */
} SkelWork;

/* The fades: a frame count and the level. */
typedef struct Fade {
    int count, level;
} Fade;

/* The sky's two gradient bands under the strip: their top (from the strip), height and colours. */
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

/* The trigger flags (D_80100F34): a byte a trigger, cleared by words. */
typedef union {
    unsigned char seen[604];
    int word[151];
} SeenFlags;

extern short D_8004BC98[]; /* libgte's rcossin_tbl: each angle's sine, then cosine */
extern unsigned char D_8009D456; /* the exe's: how far down the run got */
extern int D_800BFD74, D_800BFD78, D_800BFD7C, D_800BFD80, D_800BFD84, D_800BFD88; /* the files' sectors and sizes */
extern DRAWENV D_800BFD8C[2];
extern DISPENV D_800BFE44[2];
extern short D_800BFE6E;
extern short D_800BFE70[]; /* per stretch: its first step's number on the whole course */
extern short D_800BFE80[]; /* per stretch: its end step's number */
extern Camera D_800BFE90;  /* the camera's angles and offset at the start */
extern Camera D_800BFEA0;  /* the camera once the intro is over */
extern int D_800BFEB0, D_800BFEB4; /* the songs' offsets in the sound file */
extern RECT D_800BFEB8;
extern unsigned char D_800BFEC0[]; /* per stretch: what D_8009D456 becomes */
extern int D_800BFEC8[7];          /* the texture file's images */
extern unsigned int D_800BFEE4[][2]; /* per stretch, flat mode's two colours */
extern short D_800BFF1C[6];        /* the points the trackside props stand on */
extern unsigned char D_800BFF28[]; /* per piece kind, 4 x 4 grid cells: the handler for each */
extern short (*D_800BFF68[])(Locator *r);
extern unsigned char D_800BFF8C[]; /* per rider state: how the camera follows (1: along the motion, 2: the course) */
extern short D_800BFFAC[6];
extern SVECTOR D_800BFFB8[6];      /* three lines' ends */
extern unsigned char D_800BFFE8[12]; /* their colours, four bytes each */
extern SkyBand D_800BFFF4[2];
extern Piece D_800C000C[];
extern unsigned char D_800D067C[]; /* per step of the whole course: its Piece */
extern Pos D_800D123C[];           /* per step of the whole course: where it starts */
extern Segment D_800D37D4[];
extern short D_800D39E4[][58];     /* the rider's poses */
extern short D_800D4280[];
extern short D_800D42F4[];
extern short D_800D4368[];
extern short D_800D5758[58];
extern short D_800D6EE8[][3];      /* the poses' roots */
extern short D_800D6F5A[];
extern short D_800D6F60[];
extern short D_800D6F66[];
extern short D_800D71A6;           /* frames left in the pose blend; stops the state script while set */
extern short D_800D71A8;           /* the pose blended to, 0..0x74 */
extern int (*D_800D71AC[])(int t); /* the blends' easings */
extern MATRIX D_800D71BC;
extern unsigned char D_800D71D0[][64][4]; /* per rider state: its script's ops */
extern unsigned char D_800DB1D0;   /* the shadow is shown */
extern Board D_800DB1D4;
extern int (*D_800DB28C[14])(unsigned char *op); /* the script ops */
extern unsigned short D_800DB2C4[]; /* the buttons the script waits on */
extern unsigned short D_800DB2D4[]; /* the script's sounds */
extern void (*D_800DB2E4[])(Board *b, int enter, Scratch *s); /* the rider's states */
extern short D_800DB364[];          /* per state: how high he may be above the ground before the shadow shows */
extern unsigned char D_800DB3A4[];  /* the state after a landing, by state, speed and impact */
extern short D_800DB4C4[];          /* per state: its looping sound */
extern Joint D_800DB504[19];
extern SkelPart D_800DB9C4[15];
extern short D_800DBA3C[6];         /* the clock's digits' places */
extern unsigned short D_800DBA48[3];
extern int D_800DBA50[3];
extern unsigned short D_800DBA5C[3];
extern unsigned short D_800DBA64[]; /* BCD values, summed by the bits of a mask */
extern unsigned short D_800DBA84;   /* the board's glow's frame count */
extern void (*D_800DBA88[])(DrawWork *d, void *faces, int n); /* the drawers */
extern Tmpl D_800DBA98, D_800DBAE0, D_800DBB28, D_800DBB70;
extern Tmpl D_800DBBB8[2];
extern Tmpl D_800DBC48, D_800DBC90, D_800DBCD8;
extern short D_800DBD20[];          /* the pickups' worth */
extern Tmpl D_800DBD2C, D_800DBD74;
extern unsigned short D_800DBDBC[6]; /* the walkers' frames */
extern Tmpl D_800DBDC8, D_800DBE10, D_800DBE58;
extern short D_800DBEA0;           /* the frame of the last bounce sound */
extern short D_800DBEA2;           /* its pan */
extern Tmpl D_800DBEA8;
extern short D_800DBEF0[];
extern Tmpl D_800DBEF8;
extern int D_800DBF40[][3];
extern unsigned short D_800DBF70[];
extern unsigned short D_800DBF78[];
extern unsigned short D_800DBF7C[];
extern Tmpl D_800DBF84;
extern unsigned short D_800DBFCC;
extern Pos D_800DBFD0[39];         /* where the course's objects stand */
extern short D_800DC1A4[4][4][3];  /* the clusters' offsets */
extern short D_800DC204[5];        /* their x/z jitter */
extern short D_800DC20E;           /* the next jitter */
extern Pos D_800DC210, D_800DC21C, D_800DC228, D_800DC234, D_800DC240, D_800DC24C, D_800DC258, D_800DC264,
    D_800DC270, D_800DC27C, D_800DC288, D_800DC294, D_800DC2A0, D_800DC2AC, D_800DC2B8, D_800DC2C4, D_800DC2D0,
    D_800DC2DC, D_800DC2E8, D_800DC2F4, D_800DC300, D_800DC30C, D_800DC318, D_800DC324, D_800DC330, D_800DC33C,
    D_800DC348, D_800DC354, D_800DC360, D_800DC36C, D_800DC378, D_800DC384, D_800DC390, D_800DC39C, D_800DC3A8,
    D_800DC3B4, D_800DC3C0, D_800DC3CC, D_800DC3D8, D_800DC3E4, D_800DC3F0, D_800DC3FC, D_800DC408[2], D_800DC420,
    D_800DC42C, D_800DC438, D_800DC444, D_800DC450, D_800DC45C[2], D_800DC474, D_800DC480, D_800DC48C, D_800DC498,
    D_800DC4A4, D_800DC4B0, D_800DC4BC[2], D_800DC4D4, D_800DC4E0, D_800DC4EC, D_800DC4F8;
/* A thrown object's start: x, y, z, arg[0], arg[1], vel.x, vel.z, then f78 for some. */
extern int D_800DC504[8], D_800DC524[8], D_800DC544[8], D_800DC564[8], D_800DC584[8], D_800DC5A4[8], D_800DC5C4[8];
extern Pos D_800DC5E4, D_800DC5F0;
extern int D_800DC5FC[7];
extern Pos D_800DC618;
extern int D_800DC624[7];
extern Pos D_800DC640, D_800DC64C;
extern int D_800DC658[7], D_800DC674[7];
extern Pos D_800DC690, D_800DC69C, D_800DC6A8;
extern int D_800DC6B4[7], D_800DC6D0[7];
extern Pos D_800DC6EC, D_800DC6F8, D_800DC704, D_800DC710, D_800DC71C, D_800DC728;
extern int D_800DC734[7], D_800DC750[7];
extern Pos D_800DC76C;
extern Pos D_800DC778[38];
extern Pos D_800DC940, D_800DC94C, D_800DC958, D_800DC964, D_800DC970[2], D_800DC988, D_800DC994, D_800DC9A0[2],
    D_800DC9B8, D_800DC9C4, D_800DC9D0, D_800DC9DC, D_800DC9E8, D_800DC9F4, D_800DCA00, D_800DCA0C, D_800DCA18,
    D_800DCA24, D_800DCA30, D_800DCA3C, D_800DCA48[2], D_800DCA60, D_800DCA6C, D_800DCA78, D_800DCA84, D_800DCA90,
    D_800DCA9C, D_800DCAA8, D_800DCAB4, D_800DCAC0;
extern short D_800DCACC[12]; /* the jumps' height limits, by lane pair */
extern Pos D_800DCAE4, D_800DCAF0, D_800DCAFC;
/* Placings for snobo_spawn_kind: x, y, z and a lift, four words each. */
extern int D_800DCB08[], D_800DCB38[], D_800DCB58[], D_800DCB88[], D_800DCBA8[], D_800DCBC8[], D_800DCC08[],
    D_800DCC38[], D_800DCC58[], D_800DCC78[], D_800DCC98[], D_800DCCB8[], D_800DCCD8[], D_800DCD18[];
extern Pos D_800DCD38, D_800DCD44, D_800DCD50, D_800DCD5C, D_800DCD68, D_800DCD74, D_800DCD80, D_800DCD8C, D_800DCD98,
    D_800DCDA4, D_800DCDB0, D_800DCDBC;
extern void (*D_800DCDC8[])(int id, int first); /* the course's triggers, by step */
extern SVECTOR D_800DD72C, D_800DD734;
extern unsigned char D_800DD73C;  /* the ambience sound is on */
extern unsigned char D_800DD740;  /* the main loop runs */
extern unsigned char D_800DD744;  /* the rider is past the course's end */
extern void (*D_800DD748)(void);  /* the scene's step */
extern int D_800DD74C;            /* the buffer drawn into */
extern char D_800DD750[2][0xD000]; /* primitive buffers */
extern unsigned int D_800F7750[2][0x1000]; /* ordering tables */
extern Fade D_800FF750;
extern SVECTOR D_800FF770;
extern short D_800FF77C;
extern Mark D_800FF784[];
extern Camera D_800FFAF4;
extern SVECTOR D_800FFB04;          /* the pose blend: from root */
extern short D_800FFB0C[3];         /* to root (an array: separate shorts let GCC reorder the copies) */
extern short D_800FFB14[58];        /* from pose */
extern short D_800FFB88[58];        /* to pose */
extern int (*D_800FFBFC)(int t);    /* its easing */
extern short D_800FFC00;            /* its length */
extern short D_800FFC04;            /* the board's looping sound */
extern short D_800FFC08;            /* its pitch */
extern unsigned char D_800FFC0C[];  /* the script's loops: start */
extern unsigned char D_800FFC10[];  /* and count */
extern short D_800FFC14;            /* loop depth */
extern unsigned short D_800FFC18;   /* the script's pc */
/* Per trigger slot: armed, fired (an array: as a scalar GCC hoists a load above the store), the op to jump to. */
extern unsigned char D_800FFC1C[4], D_800FFC20[4], D_800FFC24[4];
extern unsigned char D_800FFC28;    /* the script's switch selector */
extern unsigned char D_800FFC2C;    /* its cue: a countdown the states read */
extern unsigned char D_800FFC30;
extern DR_MODE D_800FFC34;          /* the HUD's draw mode */
extern Spray D_800FFC40[8];
extern short D_800FFD40;
extern int D_800FFD44[9][3];        /* the spots the relative placers picked */
extern Obj D_800FFDB0[32];
extern short D_80100F30;            /* where the next free-object search starts */
extern SeenFlags D_80100F34;
extern short D_80101190;            /* the id of the object knocked last */
extern Queued D_80101194[];
extern unsigned int *D_80102794; /* this frame's ordering table (kept apart from D_80102798: typed as one struct, GCC
                                  * would hoist the reloads the game makes) */
extern void *D_80102798;         /* this frame's next primitive */
extern short D_8010279C, D_8010279E, D_801027A0, D_801027A2; /* the score's parts and total */
extern unsigned char D_801027A4;  /* the clock runs */
extern int D_801027A8;            /* frames the buttons have been held */
extern unsigned int D_801027AC;   /* buttons held */
extern unsigned int D_801027B0;   /* held last frame */
extern unsigned int D_801027B4;   /* auto-repeated */
extern unsigned int D_801027B8;   /* pressed */
extern short D_801027BC;          /* the course's heading under the rider */
extern unsigned short D_801027C0; /* pickups collected */
extern short D_801027C4;          /* the rider's step in his stretch */
extern short D_801027C8;          /* his face (lane) on the piece */
extern unsigned short D_801027CC; /* crashes */
extern MATRIX D_801027D0;
extern short D_801027F0;          /* frames to the next snow puff */
extern MATRIX D_801027F4;         /* the camera */
extern short D_80102814;          /* the run is scored */
extern short D_80102818;          /* the course's heading at the rider's step */
extern short D_8010281C;          /* the rider's step on the whole course */
extern MATRIX D_80102820;
extern SVECTOR D_80102840;        /* the pose's root */
extern short D_80102848;          /* paused */
extern Pos D_8010284C;            /* the drawing origin */
extern int D_8010285C;            /* the stack pointer kept while on the scratchpad's */
extern short D_80102860[58];      /* the pose's joint angles */
extern Segment *D_801028D4;       /* the rider's stretch */
extern Clock D_801028D8;          /* the run's time */
extern short D_801028DC;          /* the bend's share of the heading */
extern short D_801028E0;          /* the rider's stretch */
extern SVECTOR D_801028E4;        /* his place on the piece (x, z) */
extern unsigned char D_801028EC;  /* the surface under him: 8 ice, 4 a covered stretch */
extern short D_801028F0;
extern short D_801028F4;          /* the scene moves this frame */
extern VECTOR D_801028F8;         /* the rider's position, kept */
extern short D_80102908, D_8010290A, D_8010290C;
extern unsigned short D_80102910; /* the speed shown */
extern short D_80102954;          /* models queued this frame */
extern unsigned short D_80102958; /* frames */

void snobo_step_fade_in(void);
void snobo_step_intro(void);
void snobo_step_race(void);
void snobo_step_finish(void);
void snobo_step_close(void);
void snobo_step_fade_out(void);
void snobo_run_frame(int move, int draw);
void snobo_store_result(void);
void snobo_start_music(void);
void snobo_read_pad(void);
void snobo_load_music(void);
void snobo_load_textures(void);
void snobo_load_models(void);
void snobo_begin_frame(void);
void snobo_flip(void);
void snobo_save_rider(void);
void snobo_set_origin(void);
void snobo_ease_tilt(void);
void snobo_draw_hud(void);
void snobo_update_ambience(void);
void snobo_place_rider(int arg);
void snobo_init_run(int seg, int idx, int state);
void snobo_draw_course(int mode, int back, int count);
void snobo_add_mark(Work *w);
void snobo_set_slice_matrix(Work *w);
int snobo_project_slice(Work *w, int *sxy_in, unsigned int *sz_in, Rgb *col_in);
void snobo_draw_slice(Work *w);
void snobo_project_first_slice(Work *w);
void snobo_split_last_strip(Work *w);
void snobo_place_props(Work *w);
void snobo_draw_marks(Work *w);
void snobo_move_along_course(VECTOR *from, VECTOR *to);
void snobo_rider_enter_step(Locator *r, short step);
int snobo_find_ground_face(SVECTOR *p, Piece *pc, Locator *r);
int snobo_rider_step_forward(Locator *r);
int snobo_rider_enter_segment(Locator *r);
void snobo_bounce_player(Locator *r, VECTOR *from, VECTOR *to);
int snobo_deflect_velocity(Locator *r);
int snobo_plane_height(SVECTOR *p, SVECTOR *a, SVECTOR *b, SVECTOR *c);
short snobo_course_heading(SVECTOR *p, Piece *pc);
void snobo_normal_to_angles(SVECTOR *n, SVECTOR *rot);
void snobo_face_normal(int step, int face, SVECTOR *out, Scratch *sc);
void snobo_camera_target(SVECTOR *p, Piece *pc, Scratch *sc);
void snobo_camera_follow(Scratch *sc);
void snobo_camera_start_or_follow(Scratch *sc, int fixed);
void snobo_set_draw_origin(Scratch *sc);
void snobo_camera_start(Scratch *sc);
void snobo_camera_aim_at_player(Scratch *sc);
void snobo_draw_sky(void);
void snobo_flash_screen(int level, void *ot);
void snobo_fill_screen(unsigned int rgb, void *ot);
void snobo_check_score(void);
void snobo_score_run(void);
void snobo_run_events(void);
void snobo_pose_blend_start(int step, int frames, int ease);
void snobo_copy_pose(int *dst, int *src);
void snobo_pose_blend_step(void);
void snobo_rider_reset_sound(void);
void snobo_rider_frame(SVECTOR *pos, Scratch *s, int update, int draw);
void snobo_rider_shadow(Scratch *s);
int snobo_jump_state(Board *b);
void snobo_carve_turn(Board *b, int turn, Scratch *s);
int snobo_carve_too_hard(Board *b);
void snobo_spin_step(Board *b, Scratch *s);
void snobo_crash_slowdown(Board *b);
void snobo_pose_lerp(short *from, short *to, short *from_root, short *to_root, int t);
void snobo_face_motion(Board *b);
void snobo_rider_move(Board *b, Scratch *s);
void snobo_rider_fall(Board *b);
int snobo_angle_step(int to, int from, int max);
void snobo_rider_land(Board *b, int impact);
void snobo_board_sound(Board *b);
void snobo_pose_skeleton(MATRIX *m, MATRIX *view);
void snobo_init_hud_mode(void);
BcdTime snobo_time_tick(BcdTime t);
void snobo_draw_clock(unsigned int time, int x, int y);
void snobo_add_hud_sprite(Sprite *p, int uv, int w, int xy);
void snobo_draw_number(int n, int x, int y);
void snobo_draw_pulse_overlay(void);
unsigned short snobo_bcd_sum_mask(unsigned short mask);
unsigned short snobo_bcd_add(unsigned short a, unsigned short b);
void snobo_clear_spray(void);
void snobo_spawn_spray(Board *b);
void snobo_draw_board_glow(Board *b, MATRIX *view, GlowWork *w, int count);
void snobo_relocate_model(Model *m);
void *snobo_draw_queued(Model *m, unsigned int *ot, void *prim);
void snobo_draw_part(DrawWork *d);
void snobo_draw_g3(DrawWork *d, FaceG3 *f, int n);
void snobo_obj_knock(Obj *o);
void snobo_obj_fly(Obj *o);
void snobo_obj_expire(Obj *o);
void snobo_obj_sway(Obj *o);
void snobo_obj_expire_wobble(Obj *o);
void snobo_obj_wobble(Obj *o);
void snobo_obj_fall(Obj *o);
void snobo_obj_burst_expire(Obj *o);
void snobo_obj_fly_glow(Obj *o);
void snobo_obj_expire_glow(Obj *o);
void snobo_obj_spawn_turned(Obj *src, int dry, int angle, Scratch *t);
void snobo_obj_touch_expire(Obj *o);
void snobo_obj_pickup_rise(Obj *o);
void snobo_obj_topple(Obj *o);
void snobo_obj_topple_expire(Obj *o);
void snobo_obj_pace_turn(Obj *o);
void snobo_obj_walker_hit(Obj *o);
void snobo_obj_walker_expire(Obj *o);
void snobo_obj_walk(Obj *o);
void snobo_obj_walk_turn(Obj *o);
void snobo_obj_jump(Obj *o);
void snobo_obj_check_hit(Obj *o);
void snobo_obj_turn_back(Obj *o);
void snobo_spawn_thrown(int id, int rx, Pos *pos, Pos *vel);
void snobo_obj_landed(Obj *o);
void snobo_obj_bounce(Obj *o);
void snobo_obj_glide(Obj *o);
void snobo_place_e7_cluster(int id);
void snobo_board_tumble(void);
void snobo_clear_triggers(void);
void snobo_fire_trigger(int id);
void snobo_clear_objs(void);
void snobo_update_objs(Work *v);
void snobo_obj_wake_near_rider(Obj *o);
void snobo_obj_drop_timer(Obj *o);

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
 * searched round from D_80100F30; 0 when all 32 are taken. */
static inline Obj *obj_alloc(void)
{
    Obj *p;
    int n;
    int i = D_80100F30;

    for (n = 31; n != -1; n--) {
        p = &D_800FFDB0[i];
        if (!p->active) {
            D_80100F30 = (i + 1) & 31;
            return p;
        }
        i = (i + 1) & 31;
    }
    return 0;
}

/* A fresh object started from template t, at rest; 0 when the pool is full. id goes through a 16-bit parameter: the
 * copy that makes is in the game's code wherever the caller uses id afterwards. */
static inline Obj *obj_spawn(short id, Tmpl *t)
{
    Obj *o = obj_alloc();

    if (o) {
        o->active = 1;
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
        /* Some spawners set the new object's speed without checking: with the pool full, the PS1 writes it to low
         * RAM through 0. The PC hands them a spare. */
        static Obj spare;

        if (!o)
            return &spare;
    }
#endif
    return o;
}

/* The spawners for each template. Each takes what varies as parameters (set in the caller, id's copy would land after
 * the first constant's load), and stores in the order the game does. */

/* A model 0xF0 object (D_800DBCD8): arg[0] and its clut (shade) by kind. */
static inline void spawn_f0(short id, int shade, int arg, int ry, int x, int y, int z)
{
    D_800DBCD8.arg[0] = arg;
    D_800DBCD8.model = 0xF0;
    D_800DBCD8.shade = shade;
    D_800DBCD8.rot.vy = ry;
    D_800DBCD8.pos.x = x;
    D_800DBCD8.pos.y = y;
    D_800DBCD8.pos.z = z;
    obj_spawn(id, &D_800DBCD8);
}

/* At a course point, 0xA0000 above it. */
#define SPAWN_F0(id, ry, p, kind) spawn_f0(id, 0x2007C00 + ((kind) << 6), kind, ry, (p)->x, (p)->y - 0xA0000, (p)->z)

/* An object of D_800DBA98 (models 0xEA-0xEC) standing on the ground at x, y, z. */
static inline void spawn_bump(short id, int model, void (*update)(Obj *o), int ry, int x, int y, int z)
{
    D_800DBA98.model = model;
    D_800DBA98.rot.vy = ry;
    D_800DBA98.update = update;
    D_800DBA98.pos.x = x;
    D_800DBA98.ground = y;
    D_800DBA98.pos.y = y;
    D_800DBA98.pos.z = z;
    obj_spawn(id, &D_800DBA98);
}

/* At a course point, knocked over when hit. */
#define SPAWN_EA(id, model, ry, p) spawn_bump(id, model, snobo_obj_knock, ry, (p)->x, (p)->y, (p)->z)

/* Template t, its model and turn, at x, y, z. */
static inline void spawn_at(short id, Tmpl *t, int model, int ry, int x, int y, int z)
{
    t->model = model;
    t->rot.vy = ry;
    t->pos.x = x;
    t->pos.y = y;
    t->pos.z = z;
    obj_spawn(id, t);
}

/* Template t, turned, at x, y, z. */
static inline void spawn_turned(short id, Tmpl *t, int ry, int x, int y, int z)
{
    t->rot.vy = ry;
    t->pos.x = x;
    t->pos.y = y;
    t->pos.z = z;
    obj_spawn(id, t);
}

/* A thrown object from l: x, y, z, arg[0], arg[1], vel.x, vel.z. The game reads these through an int pointer, which
 * lets GCC hoist l[0] over the previous object's stores (a struct does not match the two-throw spawners). It doesn't
 * check for a full pool: the game writes through 0 then. */
static inline Obj *throw_obj(short id, Tmpl *t, int ry, int *l)
{
    Obj *p;

    t->rot.vy = ry;
    t->pos.x = l[0];
    t->pos.y = l[1];
    t->ground = l[1] + 0x600000;
    t->pos.z = l[2];
    t->arg[0] = l[3];
    t->arg[1] = l[4];
    p = obj_spawn(id, t);
    p->vel.x = l[5];
    p->vel.z = l[6];
    return p;
}

/* The gates' two templates: they differ only in the order they take the position in. */
static inline void spawn_gate_a(int id, int model, int ry, int x, int z, int y)
{
    D_800DBBB8[0].model = model;
    D_800DBBB8[0].rot.vy = ry;
    D_800DBBB8[0].pos.x = x;
    D_800DBBB8[0].pos.y = y;
    D_800DBBB8[0].pos.z = z;
    D_800DBBB8[0].ground = y + 0x400000;
    obj_spawn(id, &D_800DBBB8[0]);
}

static inline void spawn_gate_b(int id, int model, int ry, int x, int y, int z)
{
    D_800DBBB8[1].model = model;
    D_800DBBB8[1].rot.vy = ry;
    D_800DBBB8[1].pos.x = x;
    D_800DBBB8[1].pos.y = y;
    D_800DBBB8[1].pos.z = z;
    D_800DBBB8[1].ground = y + 0x400000;
    obj_spawn(id, &D_800DBBB8[1]);
}

#define GATE_A(id, model, ry, i) spawn_gate_a(id, model, ry, D_800DC778[i].x, D_800DC778[i].z, D_800DC778[i].y)
#define GATE_B(id, model, ry, i) spawn_gate_b(id, model, ry, D_800DC778[i].x, D_800DC778[i].y, D_800DC778[i].z)

/* At the end of a gated section: a jump off its lip once the rider is far enough above the ground. */
static inline void check_jump(int id)
{
    if (D_8010281C == id && (unsigned char)D_800DB1D4.state != 11 &&
        (D_800DB1D4.ground_y - D_800DB1D4.pos.vy) >> 12 > 0x240)
        snobo_board_tumble();
}

/* end of declarations */

void snobo_main(void)
{
    snd_request.id = 0xBA;
    snd_request.arg[0] = 0x40;
    snd_submit_request();
    snd_request.id = 0xB8;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0xBC;
    snd_request.arg[0] = 0;
    snd_submit_request();
    snobo_load_music();
    snobo_load_textures();
    snobo_load_models();
    SetGeomOffset(0, 0);
    SetGeomScreen(0xA0);
    snobo_init_run(0, 0, 0x10);
    snobo_rider_reset_sound();
    snobo_init_hud_mode();
    D_800DD74C = 0;
    D_800DD740 = 1;
    ClearImage(&D_800BFEB8, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);
    D_801027BC = 0;
    D_80102818 = 0;
    D_800FF750.count = 0;
    D_800DD748 = snobo_step_fade_in;
    while (D_800DD740) {
        snobo_read_pad();
        snobo_begin_frame();
        D_800DD748();
        /* the models are drawn on a stack in the scratchpad */
        D_8010285C = SetSp(0x1F8003FC);
        D_80102798 = snobo_draw_queued((void *)0x80180000, D_80102794, D_80102798);
        SetSp(D_8010285C);
        snobo_flip();
        cd_step();
        D_80102958++;
    }
    SetDispMask(1);
    snobo_start_music();
    snobo_store_result();
}

void snobo_step_fade_in(void)
{
    int c;

    D_800FF750.count += 4;
    c = *(unsigned char *)&D_800FF750.count;
    snobo_fill_screen(c << 16 | c << 8 | c, D_80102794);
    if (D_800FF750.count >= 0xFC) {
        D_800FF750.count = 0;
        D_800DD748 = snobo_step_intro;
    }
}

void snobo_step_intro(void)
{
    int n = ++D_800FF750.count;

    if (n < 0x80)
        snobo_flash_screen(0x100 - n * 2, D_80102794);
    if (n < 0x100) {
        D_800BFE90.ang.vx = (n * 96 >> 8) - 0x20;
        D_800BFE90.ang.vy = 0x1000 - (n * 2048 >> 8);
        D_800BFE90.ang.vz = 0;
        D_800BFE90.at.vx = 0;
        D_800BFE90.at.vy = (n * 64 >> 8) + 0x40;
        D_800BFE90.at.vz = (n * 224 >> 8) + 0x60;
    } else {
        D_800BFE90 = D_800BFEA0;
    }
    snobo_move_along_course(&D_800DB1D4.pos, &D_800DB1D4.pos);
    snobo_set_origin();
    snobo_camera_start(SCRATCH);
    snobo_draw_course(0, 1, 2);
    D_800FF770.vx = (D_800DB1D4.pos.vx >> 12) - D_8010284C.x;
    D_800FF770.vy = (D_800DB1D4.pos.vy >> 12) - D_8010284C.y;
    D_800FF770.vz = (D_800DB1D4.pos.vz >> 12) - D_8010284C.z;
    snobo_rider_frame(&D_800FF770, SCRATCH, 1, 1);
    snobo_draw_sky();
    snobo_draw_hud();
    if ((unsigned char)D_800DB1D4.state != 0x10 && D_800FF750.count > 0x100) {
        D_801027A4 = 1;
        D_800DD748 = snobo_step_race;
    }
}

void snobo_step_race(void)
{
    Piece *s;
    SVECTOR *prev;
    SVECTOR tri[3];
#ifndef NATIVE
    VECTOR unused; /* never used: the game's frame has room for it */
#endif
    int k, v;

    snobo_save_rider();
    /* Select pauses, unless R1 is held */
    if ((D_801027B8 & 0x800) && !(D_801027AC & 4)) {
        if (D_80102848 ^= 1)
            snd_request.id = 0x99;
        else
            snd_request.id = 0x98;
        snd_submit_request();
    }
    if ((D_801028F4 = !D_80102848)) {
        snobo_face_normal(D_801027C4, D_801027C8, &D_800DB1D4.normal, SCRATCH);
        snobo_rider_move(&D_800DB1D4, SCRATCH);
        snobo_move_along_course(&D_800DB1D4.pos, &D_801028F8);
        D_8010281C = D_801027C4 + D_800BFE70[D_801028E0];
        s = &D_800C000C[D_801028D4->piece[D_801027C4]];
        snobo_set_origin();
        snobo_place_rider(D_800BFE6E);
        /* heights at the slice's first row are taken from the slice before */
        prev = s->v;
        if (D_801027C4 > 0)
            prev = &D_800C000C[D_801028D4->piece[D_801027C4 - 1]].v[42];
        for (k = 0; k < 3; k++) {
            /* indexed as rows of bytes (lane * 4 + k), as the game does */
            v = ((unsigned char (*)[4])s->face)[D_801027C8][k];
            tri[k] = s->v[v];
            if (v < 7)
                tri[k].vy = prev[v].vy - prev[0].vy;
        }
        D_800DB1D4.ground_y = (snobo_plane_height(&D_801028E4, &tri[0], &tri[1], &tri[2]) +
                             D_801028D4->origin[D_801027C4].y) << 12;
        snobo_rider_fall(&D_800DB1D4);
        if (D_800DB1D4.air_time >= 9) {
            D_800DB1D4.ground_rot.vz = 0;
            D_800DB1D4.ground_rot.vx = 0;
        } else {
            snobo_normal_to_angles(&D_800DB1D4.normal, &D_800DB1D4.ground_rot);
        }
    }
    D_800DD72C.vx = (D_800DB1D4.pos.vx >> 12) - D_8010284C.x;
    D_800DD72C.vy = (D_800DB1D4.pos.vy >> 12) - D_8010284C.y;
    D_800DD72C.vz = (D_800DB1D4.pos.vz >> 12) - D_8010284C.z;
    snobo_draw_course(0, 1, D_801028D4->ahead[D_801027C4] + 1);
    if (D_801028F4)
        snobo_ease_tilt();
    D_8010285C = SetSp(0x1F8003FC);
    snobo_rider_frame(&D_800DD72C, SCRATCH, D_801028F4, 1);
    SetSp(D_8010285C);
    if (D_801028F4) {
        if (D_801027F0-- < 0 && D_800DB1D4.air_time < 4) {
            snobo_spawn_spray(&D_800DB1D4);
            D_801027F0 = 8;
        }
        snobo_draw_board_glow(&D_800DB1D4, &D_801027F4, (GlowWork *)WORK, D_801028F4);
    }
    snobo_draw_sky();
    snobo_draw_hud();
    snobo_check_score();
    snobo_update_ambience();
    if (D_801028D4->flags[D_801027C4] & 0x80) {
        snobo_set_draw_origin(SCRATCH);
        D_800DD748 = snobo_step_finish;
    }
}

void snobo_step_finish(void)
{
    D_801027B8 = 0;
    D_801027AC = 0;
    snobo_run_frame(1, 1);
    if (D_8010281C != D_800BFE80[D_801028E0])
        return;
    if (D_801027C8 >= 0x3C) {
        switch ((unsigned char)D_800DB1D4.state) {
        case 1:
        case 0x11:
        case 0x12:
            D_800DB1D4.state = 3;
        }
    }
    if (D_800DD744) {
        D_800FF750.level = 0;
        D_800FF750.count = 0;
        D_800DD748 = snobo_step_close;
    }
}

void snobo_step_close(void)
{
    D_800DB1D4.vy -= 0xC00;
    if ((unsigned char)D_800DB1D4.state >= 3 && (unsigned char)D_800DB1D4.state <= 6) {
        D_801027B8 = 0;
        D_801027AC |= 0x40;
    } else {
        D_801027B8 = 0;
        D_801027AC = 0;
    }
    snobo_run_frame(1, 1);
    if (++D_800FF750.count < 0x3D)
        return;
    snobo_flash_screen(D_800FF750.level += 2, D_80102794);
    if (D_800FF750.level >= 0xFC)
        D_800DD748 = snobo_step_fade_out;
}

void snobo_step_fade_out(void)
{
    int c;

    c = (D_800FF750.level -= 4) & 0xFF;
    snobo_fill_screen(c << 16 | c << 8 | c, D_80102794);
    if (D_800FF750.level <= 0)
        D_800DD740 = 0;
}

void snobo_run_frame(int move, int draw)
{
    Piece *s;
    SVECTOR *prev;
    SVECTOR tri[3];
    int k, v;

    snobo_save_rider();
    D_801028F4 = 1;
    if (move) {
        snobo_face_normal(D_801027C4, D_801027C8, &D_800DB1D4.normal, SCRATCH);
        snobo_rider_move(&D_800DB1D4, SCRATCH);
    }
    if (!D_800DD744) {
        snobo_move_along_course(&D_800DB1D4.pos, &D_801028F8);
        D_8010281C = D_801027C4 + D_800BFE70[D_801028E0];
    }
    s = &D_800C000C[D_801028D4->piece[D_801027C4]];
    snobo_camera_aim_at_player(SCRATCH);
    /* heights at the slice's first row are taken from the slice before */
    prev = s->v;
    if (D_801027C4 > 0)
        prev = &D_800C000C[D_801028D4->piece[D_801027C4 - 1]].v[42];
    for (k = 0; k < 3; k++) {
        /* indexed as rows of bytes (lane * 4 + k), as the game does */
        v = ((unsigned char (*)[4])s->face)[D_801027C8][k];
        tri[k] = s->v[v];
        if (v < 7)
            tri[k].vy = prev[v].vy - prev[0].vy;
    }
    if (!D_800DD744)
        D_800DB1D4.ground_y = (snobo_plane_height(&D_801028E4, &tri[0], &tri[1], &tri[2]) +
                             D_801028D4->origin[D_801027C4].y) << 12;
    else
        D_800DB1D4.ground_y = 0x7FFFFFFF;
    if (move)
        snobo_rider_fall(&D_800DB1D4);
    if (D_800DB1D4.air_time >= 9) {
        D_800DB1D4.ground_rot.vz = 0;
        D_800DB1D4.ground_rot.vx = 0;
    } else {
        snobo_normal_to_angles(&D_800DB1D4.normal, &D_800DB1D4.ground_rot);
    }
    D_800DD734.vx = (D_800DB1D4.pos.vx >> 12) - D_8010284C.x;
    D_800DD734.vy = (D_800DB1D4.pos.vy >> 12) - D_8010284C.y;
    D_800DD734.vz = (D_800DB1D4.pos.vz >> 12) - D_8010284C.z;
    if (draw)
        snobo_draw_course(0, 7 - (D_800BFE80[D_801028E0] - D_8010281C), 8);
    snobo_ease_tilt();
    D_8010285C = SetSp(0x1F8003FC);
    snobo_rider_frame(&D_800DD734, SCRATCH, D_801028F4, 0);
    SetSp(D_8010285C);
    snobo_draw_hud();
    snobo_update_ambience();
}

void snobo_store_result(void)
{
    D_8009D456 = D_800BFEC0[D_801028E0];
}

void snobo_play_effect(int channel, int sound, int volume)
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

void snobo_start_music(void)
{
    snd_request.id = 0x10;
    snd_request.arg[0] = D_800BFEB0 + 0x801FC000;
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

void snobo_read_pad(void)
{
    int repeat;

    D_801027B0 = D_801027AC;
    D_801027AC = func_8001C808(0);
    D_801027B8 = (D_801027B0 ^ D_801027AC) & D_801027AC;
    if (D_801027B0 != D_801027AC)
        D_801027A8 = 0;
    repeat = D_801027AC;
    if (D_801027A8 && D_801027A8 < 0x10)
        repeat = 0;
    D_801027B4 = repeat;
    D_801027A8++;
}

void snobo_load_music(void)
{
    cd_read_lzs(D_800BFD74, D_800BFD78, (void *)0x801FC000, 0);
    while (cd_step())
        ;
}

void snobo_load_textures(void)
{
    TIM_IMAGE tim;
    int i;

    cd_read_lzs(D_800BFD7C, D_800BFD80, (void *)0x80120000, 0);
    while (cd_step())
        ;
    for (i = 0; i < 7; i++) {
        OpenTIM((unsigned int *)(D_800BFEC8[i] + 0x80120000));
        ReadTIM(&tim);
        func_80044000(tim.prect, tim.paddr);
        if (tim.crect)
            func_80044000(tim.crect, tim.caddr);
    }
    DrawSync(0);
    DrawSync(0);
}

void snobo_load_models(void)
{
    cd_read_lzs(D_800BFD84, D_800BFD88, (void *)0x80180000, 0);
    while (cd_step())
        ;
    snobo_relocate_model((void *)0x80180000);
}

void snobo_begin_frame(void)
{
    D_800DD74C ^= 1;
    func_80044244(D_80102794 = D_800F7750[D_800DD74C], 0x1000);
    D_80102798 = D_800DD750[D_800DD74C];
}

void snobo_flip(void)
{
    DrawSync(0);
    VSync(D_80102848 ? 2 : 0);
    PutDrawEnv(&D_800BFD8C[D_800DD74C]);
    func_800444AC(&D_800BFE44[D_800DD74C]);
    func_8004433C(&D_800F7750[D_800DD74C][0xFFF]);
    D_800FF77C = 0;
}

void snobo_save_rider(void)
{
    D_801028F8 = D_800DB1D4.pos;
    D_80102908 = D_801027C4;
    D_8010290A = D_8010281C;
    D_8010290C = D_801027C8;
}

void snobo_restore_rider(void)
{
    D_800DB1D4.pos = D_801028F8;
    D_801027C4 = D_80102908;
    D_8010281C = D_8010290A;
    D_801027C8 = D_8010290C;
}

void snobo_set_origin(void)
{
    Pos *p = &D_801028D4->origin[D_801027C4];

    D_8010284C.x = p->x;
    D_8010284C.y = p->y;
    D_8010284C.z = p->z;
}

void snobo_ease_tilt(void)
{
    int d, step;

    d = D_800DB1D4.ground_rot.vz - D_800DB1D4.rot.vz;
    if (d) {
        step = (d < 0 ? -d : d) >> 3;
        if (!step)
            step = 1;
        if (d < 0)
            step = -step;
        D_800DB1D4.rot.vz += step;
    }
    d = D_800DB1D4.ground_rot.vx - D_800DB1D4.rot.vx;
    if (d) {
        step = (d < 0 ? -d : d) >> 2;
        if (!step)
            step = 1;
        if (d < 0)
            step = -step;
        D_800DB1D4.rot.vx += step;
    }
}

void snobo_draw_hud(void)
{
    Clock v;

    if (D_801028F4) {
        if (D_801027A4) {
            v.t = snobo_time_tick(D_801028D8.t);
            D_801028D8 = v;
        }
        if (D_80102958 & 1)
            D_80102910 = snobo_bcd_sum_mask(D_800DB1D4.speed * 150 / 2048);
    }
    snobo_draw_clock(D_801028D8.word, -0x98, -0x60);
    snobo_draw_number(D_80102910, 0x5C, -0x60);
    if (D_80102848)
        snobo_draw_pulse_overlay();
}

void snobo_update_ambience(void)
{
    unsigned char flag = D_801028D4->flags[D_801027C4] & 4;

    if (flag == D_800DD73C)
        return;
    D_800DD73C = flag;
    if (flag) {
        snd_request.arg[0] = 0x1E;
        snd_request.arg[1] = 1;
    } else {
        snd_request.arg[0] = 8;
        snd_request.arg[1] = 0x7F;
    }
    snd_request.id = 0xC1;
    snd_submit_request();
}

#ifdef NON_MATCHING
/* Scheduling: the game loads D_801027C4 and moves a0 above the matrix stores, so its stores come in
 * another order. */
void snobo_place_rider(int arg)
{
    Piece *s = &D_800C000C[D_801028D4->piece[D_801027C4]];

    rot_y(&D_801027D0, D_801027BC = snobo_course_heading(&D_801028E4, s) & 0xFFF);
    rot_y(&D_80102820, D_80102818 = D_801028D4->angle[D_801027C4]);
    snobo_camera_target(&D_801028E4, s, SCRATCH);
    if (arg)
        snobo_camera_start_or_follow(SCRATCH, arg);
    else
        snobo_camera_follow(SCRATCH);
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_place_rider);
#endif

#ifdef NON_MATCHING
/* Register choice and scheduling in the position block (D_8010284C, pos): the game keeps D_801028D4 in
 * a0. */
void snobo_init_run(int seg, int idx, int state)
{
    Piece *s;
    SVECTOR v;
    MATRIX m;

    D_801028D8.word = 0;
    D_801027A4 = 0;
    D_80102910 = 0;
    D_80102848 = 0;
    D_801027F0 = 0;
    D_801027CC = 0;
    D_801027C0 = 0;
    D_80102814 = 0;
    D_801028F0 = 0;
    D_800DD73C = 0;
    D_800DD744 = 0;
    D_801028E0 = seg;
    D_801028D4 = &D_800D37D4[seg];
    D_801027C4 = idx;
    s = &D_800C000C[D_801028D4->piece[idx]];
    D_800BFE90 = D_800BFEA0;
    D_800FFAF4.ang.vx = 0;
    D_800FFAF4.ang.vy = 0x800;
    D_800FFAF4.ang.vz = 0;
    /* the rider starts in the middle of the slice, turned with the course */
    rot_y(&m, D_801028D4->angle[idx]);
    v.vx = (s->v[0].pad + s->v[2].pad) >> 1;
    v.vy = (s->v[3].vy + s->v[45].vy) >> 1;
    v.vz = (s->v[3].pad + s->v[5].pad) >> 1;
    ApplyMatrixSV(&m, &v, &v);
    D_8010284C.x = D_801028D4->origin[idx].x;
    D_800DB1D4.pos.vx = (v.vx + D_8010284C.x) << 12;
    D_8010284C.y = D_801028D4->origin[idx].y;
    D_800DB1D4.pos.vy = (v.vy + D_8010284C.y) << 12;
    D_8010284C.z = D_801028D4->origin[idx].z;
    D_800DB1D4.pos.vz = (v.vz + D_8010284C.z) << 12;
    D_800DB1D4.vx = 0;
    D_800DB1D4.vy = 0;
    D_800DB1D4.vz = 0;
    D_800DB1D4.facing.vx = 0;
    D_800DB1D4.facing.vz = 0;
    D_800DB1D4.rot.vx = 0;
    D_800DB1D4.ground_rot.vx = 0;
    D_800DB1D4.rot.vy = 0;
    D_800DB1D4.ground_rot.vy = 0;
    D_800DB1D4.rot.vz = 0;
    D_800DB1D4.ground_rot.vz = 0;
    D_800DB1D4.state = state;
    D_800DB1D4.facing.vy = D_801028D4->angle[idx];
    v.vx = D_800DB1D4.pos.vx >> 12;
    v.vy = D_800DB1D4.pos.vy >> 12;
    v.vz = D_800DB1D4.pos.vz >> 12;
#ifdef NATIVE
    /* The game passes no locator: the search writes its depth and face wherever a2 points, here a small number, into
     * low RAM. The PC hands it the scratchpad's. */
    D_801027C8 = snobo_find_ground_face(&v, s, (Locator *)SCRATCH);
#else
    D_801027C8 = ((int (*)(SVECTOR *, Piece *))snobo_find_ground_face)(&v, s);
#endif
    snobo_clear_spray();
    snobo_clear_triggers();
    snobo_clear_objs();
    snd_request.id = 0x10;
    snd_request.arg[0] = D_800BFEB0 + 0x801FC000;
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
    snd_request.id = 0x10;
    snd_request.arg[0] = D_800BFEB4 + 0x801FC000;
    snd_submit_request();
    snd_request.id = 0xC2;
    snd_request.arg[0] = 0x12C;
    snd_request.arg[1] = 0;
    snd_request.arg[2] = 0x7F;
    snd_submit_request();
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_init_run);
#endif

#ifdef NON_MATCHING
/* The inner loop's exit test isn't repeated at its top as the game's is, the two idx = 0 are
 * cross-jumped into one, and the rejoin path keeps the value where the game reloads it. */
void snobo_draw_course(int mode, int back, int count)
{
    Work *w = WORK;
    short idx, seg, next, k;

    w->rejoin = 0;
    w->f00 = mode;
    w->m = D_801027F4;
    w->left = count;
    w->nmarks = 0;
    w->f32 = D_801028E0;
    w->f34 = D_801027C4;
    w->ot = D_80102794;
    w->prim = D_80102798;
    w->origin.x = D_8010284C.x;
    w->origin.y = D_8010284C.y;
    w->origin.z = D_8010284C.z;
    /* start BACK points behind the rider, in the stretch before if need be */
    k = D_801027C4 - back;
    idx = k;
    seg = D_801028E0;
    if (k < 0) {
        next = D_801028D4->prev;
        if (next == 0xFF) {
            idx = 0;
        } else {
            idx = k + D_800D37D4[next].len;
            seg = next;
            if (idx < 0)
                idx = 0;
        }
    }
    w->idx = idx;
    w->seg = seg;
    w->course = &D_800D37D4[w->seg];
    w->point = &w->course->origin[w->idx];
    snobo_set_slice_matrix(w);
    snobo_project_first_slice(w);
    do {
        snobo_draw_slice(w);
        snobo_add_mark(w);
        w->f03 = 0;
        while (w->left-- > 0) {
            if (w->slice->kind == 3) {
                /* a fork: carry on down the branch, then come back for the rest */
                w->seg = w->course->next[0];
                w->rejoin = w->course->next[1];
                w->idx = 0;
                w->course = &D_800D37D4[w->seg];
                w->point = w->course->origin;
                w->f30 = --w->left;
                w->f02 = 1;
            } else {
                if (++w->idx >= w->course->len)
                    break;
                w->f02 = 0;
                w->point++;
            }
            snobo_set_slice_matrix(w);
            if (w->f02)
                snobo_project_slice(w, w->sxy2, w->sz2, w->col2);
            else
                snobo_project_slice(w, &w->sxy[42], &w->sz[42], &w->col[42]);
            snobo_draw_slice(w);
            snobo_add_mark(w);
        }
        if (!w->rejoin)
            break;
        w->seg = w->rejoin;
        w->course = &D_800D37D4[w->seg];
        w->rejoin = 0;
        w->idx = 0;
        w->point = w->course->origin;
        w->left = w->f30;
        snobo_set_slice_matrix(w);
    } while (!snobo_project_slice(w, &w->sxy2[6], &w->sz2[6], &w->col2[6]));
    D_80102798 = w->prim;
    snobo_place_props(w);
    snobo_draw_marks(w);
    snobo_update_objs(w);
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_draw_course);
#endif

void snobo_add_mark(Work *w)
{
    Mark *m = &D_800FF784[w->nmarks];

    m->seg = w->seg;
    m->idx = w->idx;
    m->slice = w->slice;
    m->m = w->lm;
    w->nmarks++;
}

void snobo_set_slice_matrix(Work *w)
{
    rot_y(&w->rm, w->course->angle[w->idx]);
    w->rm.t[0] = w->point->x - w->origin.x;
    w->rm.t[1] = w->point->y - w->origin.y;
    w->rm.t[2] = w->point->z - w->origin.z;
    gte_CompMatrix(&w->m, &w->rm, &w->lm);
    gte_SetRotMatrix(&w->lm);
    gte_SetTransMatrix(&w->lm);
    w->slice = &D_800C000C[w->course->piece[w->idx]];
}

int snobo_project_slice(Work *w, int *sxy_in, unsigned int *sz_in, Rgb *col_in)
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
        snobo_split_last_strip(w);
    return 0;
}

#ifdef NON_MATCHING
/* Register allocation: the game keeps more values in s-registers (s6 for the GTE operands) and w in
 * a2. */
void snobo_draw_slice(Work *w)
{
    int i = 0, k, z;
    Tri t;
    int a, b, c;
    Tri *tri;

    if (!(w->f00 & 1)) {
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
        w->flat_col[0] = D_800BFEE4[w->seg][0];
        w->flat_col[1] = D_800BFEE4[w->seg][1];
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
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_draw_slice);
#endif

void snobo_project_first_slice(Work *w)
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
            next = &D_800D37D4[w->course->prev];
            src = &D_800C000C[next->piece[next->len - 1]].col[42];
        }
    } else {
        src = &D_800C000C[w->course->piece[prev]].col[42];
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
        snobo_split_last_strip(w);
}

void snobo_split_last_strip(Work *w)
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
/* The game stores the D_80102954 increment before computing the address, and keeps the prop base in t9
 * (one saved register fewer). */
void snobo_place_props(Work *w)
{
    Mark *m = D_800FF784;
    Queued *p;
    SVECTOR *v;
    unsigned int kinds;
    int n, k;

    n = w->nmarks;
    while (--n != -1) {
        w->lm = m->m;
        kinds = D_800D37D4[m->seg].props[m->idx];
        if (kinds) {
            p = &D_80101194[D_80102954++];
            p->m = w->lm;
            p->how = 0;
            p->model = kinds & 0xFF;
            p->drawer = 0;
            p->shade = 0;
            for (kinds >>= 8; kinds; kinds >>= 8) {
                p = &D_80101194[D_80102954++];
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
                v = &m->slice->v[D_800BFF1C[k]];
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
                p = &D_80101194[D_80102954++];
                p->m = w->lm;
                p->how = 0;
                p->model = 0xE6;
                p->drawer = 2;
                p->shade = 0;
            }
        }
        m++;
    }
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_place_props);
#endif

void snobo_draw_marks(Work *w)
{
    Mark *m = D_800FF784;
    short n = w->nmarks;

    while (--n != -1) {
        snobo_fire_trigger(D_800BFE70[m->seg] + m->idx);
        m++;
    }
}

/* Move along the course from `from` to `to` (20.12 fixed point), letting the grid cell of the piece under `to` decide
 * what happens; up to four tries. */
#ifdef NON_MATCHING
/* Register allocation: the game keeps j's zero in a0 through the first loop. */
void snobo_move_along_course(VECTOR *from, VECTOR *to)
{
    Locator *r = (Locator *)0x1F800000;
    SVECTOR *v;
    int i, j;

    r->tries = 0;
    r->seg = D_801028E0;
    r->step = D_801027C4;
    r->segp = D_801028D4;
    r->pos.vx = from->vx >> 12;
    r->pos.vz = from->vz >> 12;
    r->to.vx = to->vx >> 12;
    r->to.vz = to->vz >> 12;
    snobo_rider_enter_step(r, r->step);
    v = r->piece->v;
    for (i = 0; i < 3; i++)
        if (r->local.vx < v[i].pad)
            break;
    for (j = 0; j < 3; j++)
        if (v[j + 3].pad < r->local.vz)
            break;
    r->moved = D_800BFF68[D_800BFF28[r->piece->kind * 16 + j * 4 + i]](r);
    if (r->moved && ++r->tries < 4) {
        *from = *to;
        snobo_bounce_player(r, from, to);
        return;
    }
    D_801028E4.vx = r->local.vx;
    D_801028E4.vz = r->local.vz;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_move_along_course);
#endif

/* Turn the rider into the frame of piece `step` of its segment. */
inline void snobo_rider_enter_step(Locator *r, short step)
{
    short *cs;
    short s, c;
    Pos *o;

    r->piece = &D_800C000C[r->segp->piece[step]];
    cs = &D_8004BC98[(-r->segp->angle[step] & 0xFFF) * 2];
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
    o = &r->segp->origin[step];
    r->local.vx = r->pos.vx - o->x;
    r->local.vz = r->pos.vz - o->z;
    ApplyMatrixSV(&r->rot, &r->local, &r->local);
}

/* The face of the piece under p (local x, z), searched last to first; -1 if none. */
#ifdef NON_MATCHING
/* Three pseudos swap registers (reach, the face pointer and s2 get t1/t2/t0, the game t0/t1/t2). */
int snobo_find_ground_face(SVECTOR *p, Piece *pc, Locator *r)
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
        v = &pc->v[r->ground.v[0]];
        s0 = SXY(v);
        v = &pc->v[r->ground.v[2]];
        s2 = SXY(v);
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
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_find_ground_face);
#endif

/* Find the ground under the rider on its current piece; 1 if it has left it. */
int snobo_cell_stay(Locator *r)
{
    r->face = snobo_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0)
        return 1;
    D_801027C8 = r->face;
    D_801028EC = r->ground.attr;
    return 0;
}

/* Step the rider on to the next piece of its segment; 0 with D_800DD744 set at the segment's end, 1 if it isn't on
 * the ground there. */
#ifdef NON_MATCHING
/* &r->rot is computed before the rcossin_tbl base instead of after (one instruction). */
int snobo_rider_step_forward(Locator *r)
{
    short *cs;
    short s, c;
    Pos *o;
    short step = ++r->step;

    if (step >= r->segp->len) {
        D_800DD744 = 1;
        return 0;
    }
    r->piece = &D_800C000C[r->segp->piece[step]];
    cs = &D_8004BC98[(-r->segp->angle[step] & 0xFFF) * 2];
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
    o = &r->segp->origin[step];
    r->local.vx = r->pos.vx - o->x;
    r->local.vz = r->pos.vz - o->z;
    ApplyMatrixSV(&r->rot, &r->local, &r->local);
    /* never start behind the piece */
    if (r->local.vz > 0)
        r->local.vz = 0;
    r->face = snobo_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0)
        return 1;
    D_801027C4 = r->step;
    D_801027C8 = r->face;
    D_801028EC = r->ground.attr;
    return 0;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_rider_step_forward);
#endif

/* Step the rider back to the previous piece, into the previous segment at a segment's start; 1 if there is none or
 * it isn't on the ground there. */
#ifdef NON_MATCHING
/* The one-instruction difference of snobo_rider_step_forward, in both inlined copies of
 * snobo_rider_enter_step. */
int snobo_rider_step_back(Locator *r)
{
    short step = --r->step;

    if (step < 0) {
        if (r->segp->prev == 0xFF)
            return 1;
        r->segp = &D_800D37D4[r->seg = r->segp->prev];
        snobo_rider_enter_step(r, r->step = r->segp->len - 1);
        r->face = snobo_find_ground_face(&r->local, r->piece, r);
        if (r->face < 0)
            return 1;
        D_801028E0 = r->seg;
        D_801028D4 = r->segp;
    } else {
        snobo_rider_enter_step(r, step);
        r->face = snobo_find_ground_face(&r->local, r->piece, r);
        if (r->face < 0)
            return 1;
    }
    D_801027C4 = r->step;
    D_801027C8 = r->face;
    D_801028EC = r->ground.attr;
    return 0;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_rider_step_back);
#endif

/* As snobo_cell_stay, stepping on to the next piece when it has left this one. */
int snobo_cell_forward(Locator *r)
{
    r->face = snobo_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0)
        return snobo_rider_step_forward(r);
    D_801027C8 = r->face;
    D_801028EC = r->ground.attr;
    return 0;
}

int snobo_cell_take_left(Locator *r)
{
    r->seg = r->segp->next[0];
    return snobo_rider_enter_segment(r);
}

/* Put the rider at the start of its segment r->seg; 1 if it isn't on the ground there. */
int snobo_rider_enter_segment(Locator *r)
{
    short *cs;
    short s, c;
    Pos *o;

    r->segp = &D_800D37D4[r->seg];
    r->step = 0;
    r->piece = &D_800C000C[r->segp->piece[0]];
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
    r->face = snobo_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0)
        return 1;
    D_801028E0 = r->seg;
    D_801027C4 = r->step;
    D_801027C8 = r->face;
    D_801028D4 = r->segp;
    D_801028EC = r->ground.attr;
    return 0;
}

int snobo_cell_take_right(Locator *r)
{
    r->seg = r->segp->next[1];
    return snobo_rider_enter_segment(r);
}

/* As snobo_cell_stay, taking the course's left branch when it has left the piece. */
int snobo_cell_left_at_end(Locator *r)
{
    r->face = snobo_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0) {
        r->seg = r->segp->next[0];
        return snobo_rider_enter_segment(r);
    }
    D_801027C8 = r->face;
    D_801028EC = r->ground.attr;
    return 0;
}

/* As snobo_cell_stay, taking the course's right branch when it has left the piece. */
int snobo_cell_right_at_end(Locator *r)
{
    r->face = snobo_find_ground_face(&r->local, r->piece, r);
    if (r->face < 0) {
        r->seg = r->segp->next[1];
        return snobo_rider_enter_segment(r);
    }
    D_801027C8 = r->face;
    D_801028EC = r->ground.attr;
    return 0;
}

int snobo_cell_wall(void)
{
    return 1;
}

/* After a hit in snobo_move_along_course: bounce the player off the side it ran into, or stop it. */
#ifdef NON_MATCHING
/* The game keeps &D_800DB1D4.state in s0 though it reads it once, and loads D_801027BC once before the
 * +-0x200 branch. */
void snobo_bounce_player(Locator *r, VECTOR *from, VECTOR *to)
{
    int angle;

    if (D_801028D4->flags[D_801027C4] & 0x80) {
        snobo_deflect_velocity(r);
        return;
    }
    if ((unsigned char)D_800DB1D4.state != 2) {
        if (r->moved >= 2) {
            if (D_800DB1D4.speed > 0x80) {
                angle = (r->moved == 2 ? D_801027BC + 0x200 : D_801027BC - 0x200) & 0x7FF;
                snobo_rider_land(&D_800DB1D4, angle);
                return;
            }
        } else if (D_800DB1D4.speed > 0x80) {
            angle = snobo_deflect_velocity(r);
            if (angle > 0x800)
                angle = 0x1000 - angle;
            snobo_rider_land(&D_800DB1D4, angle);
            return;
        }
    }
    D_800DB1D4.vx = 0;
    D_800DB1D4.vz = 0;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_bounce_player);
#endif

/* Turn the player's velocity off the slope of the face under it (a wall?); returns the turn. */
int snobo_deflect_velocity(Locator *r)
{
    short *cs;
    short s, c;
    int dir, slope;

    r->segp = D_801028D4;
    r->step = D_801027C4;
    r->piece = &D_800C000C[r->segp->piece[r->step]];
    dir = ratan2(D_800DB1D4.vx, D_800DB1D4.vz) & 0xFFF;
    slope = (r->piece->v[(short)(D_801027C8 / 6) + 8].pad + r->segp->angle[r->step] + 0x800) & 0x7FF;
    slope = ((0x800 - (dir - slope)) * 2) & 0xFFF;
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
    ApplyMatrixLV(&r->rot, (VECTOR *)&D_800DB1D4.vx, (VECTOR *)&D_800DB1D4.vx);
    return slope;
}

/* The height at p's x, z of the plane through a, b, c. */
int snobo_plane_height(SVECTOR *p, SVECTOR *a, SVECTOR *b, SVECTOR *c)
{
    int x = ((a->vy - b->vy) * (a->vz - c->vz) - (a->vy - c->vy) * (a->vz - b->vz)) * (a->vx - p->vx);
    int ny = (a->vz - b->vz) * (a->vx - c->vx) - (a->vz - c->vz) * (a->vx - b->vx);
    int z = ((a->vx - b->vx) * (a->vy - c->vy) - (a->vx - c->vx) * (a->vy - b->vy)) * (a->vz - p->vz);

    if (ny)
        ny = (x + z) / ny;
    return ny + a->vy;
}

/* The course's heading at local point p on piece pc: the step's yaw plus the bend so far (D_801028DC). */
short snobo_course_heading(SVECTOR *p, Piece *pc)
{
    if (pc->bend > 0)
        D_801028DC = -ratan2(p->vz, pc->bend + p->vx);
    else if (pc->bend < 0)
        D_801028DC = ratan2(p->vz, pc->v[6].vx - p->vx - pc->bend);
    else
        D_801028DC = 0;
    return D_801028D4->angle[D_801027C4] + D_801028DC;
}

/* The rotation that stands a model up along normal n. */
void snobo_normal_to_angles(SVECTOR *n, SVECTOR *rot)
{
    rot->vx = ratan2(-n->vz, n->vy);
    rot->vy = 0;
    rot->vz = ratan2(n->vx, n->vy);
}

/* The normal of face `face` of the piece at `step`, turned by D_80102820 into out (sc->sv is its workspace). */
#ifdef NON_MATCHING
/* Load and multiply scheduling inside the cross products. */
void snobo_face_normal(int step, int face, SVECTOR *out, Scratch *sc)
{
    int k;
    Face *f;
    SVECTOR *v, *a, *b, *c;
    int abz, bcy, aby, bcz, bcx, abx;

    if (face < 0)
        return;
    k = D_801028D4->piece[step];
    f = &D_800C000C[k].face[face];
    v = D_800C000C[k].v;
    a = &v[f->v[0]];
    b = &v[f->v[1]];
    c = &v[f->v[2]];
    abz = b->vz - a->vz;
    bcy = c->vy - b->vy;
    aby = b->vy - a->vy;
    bcz = c->vz - b->vz;
    bcx = c->vx - b->vx;
    abx = b->vx - a->vx;
    sc->sv.vx = (abz * bcy - aby * bcz) >> 3;
    sc->sv.vy = (abz * bcx - abx * bcz) >> 3;
    sc->sv.vz = (aby * bcx - abx * bcy) >> 3;
    ApplyMatrixSV(&D_80102820, &sc->sv, out);
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_face_normal);
#endif

/* The camera's look-at point: the player, at a height blended across piece pc between its start and end heights
 * (by p, the player's local position), capped just above the player. */
#ifdef NON_MATCHING
/* The operand order of one addu in (y + v[48].vy) + (y + v[42].vy), and the loads before it. */
void snobo_camera_target(SVECTOR *p, Piece *pc, Scratch *sc)
{
    int bend = pc->bend;
    int y = D_801028D4->origin[D_801027C4].y;
    SVECTOR *pv = D_800C000C[D_801028D4->piece[D_801027C4]].v;
    int lo = ((y + (y + pv[6].vy)) >> 1) - D_8010284C.y;
    int hi = (((y + pv[48].vy) + (y + pv[42].vy)) >> 1) - D_8010284C.y;
    int turn, top;
    MATRIX *m;
    short *cs;
    short s, c;

    if (bend) {
        turn = D_801028DC;
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
        lo += (hi - lo) * (D_801028DC < 0 ? -D_801028DC : D_801028DC) / pc->turn;
    } else {
        sc->sv.vx = pc->v[3].vx;
        sc->sv.vz = p->vz;
        lo += (hi - lo) * p->vz / pc->v[5].pad;
    }
    top = lo;
    hi = (D_800DB1D4.pos.vy >> 12) - D_8010284C.y + 0x40;
    if (hi < top)
        top = hi;
    D_800FFAF4.at.vy = top;
    D_800FFAF4.at.vx = (D_800DB1D4.pos.vx >> 12) - D_8010284C.x;
    D_800FFAF4.at.vz = (D_800DB1D4.pos.vz >> 12) - D_8010284C.z;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_camera_target);
#endif

/* Point the camera at the player: turn towards its heading or the course's, pitch to the slope, and lift it when
 * the player would sit too high on the screen. */
#ifdef NON_MATCHING
/* Registers in the yaw block (the game's yaw in a0), where speed's conversion to short happens, the
 * pitch target's form. */
void snobo_camera_follow(Scratch *sc)
{
    int yaw, d, pitch;
    short speed;
    Segment *seg;
    SVECTOR *ang;

    if (D_801027C4 < 0)
        return;
    ang = &D_800FFAF4.ang;
    switch (D_800BFF8C[(unsigned char)D_800DB1D4.state]) {
    case 1:
        if (D_800DB1D4.speed > 0x80) {
            yaw = ratan2(-D_800DB1D4.vx, -D_800DB1D4.vz) & 0xFFF;
            goto turn;
        }
    case 2:
        yaw = D_801027BC;
    turn:
        d = (yaw - D_801027BC) & 0xFFF;
        if (d > 0x400 && d < 0xC00)
            yaw = (yaw + 0x800) & 0xFFF;
        d = (yaw - D_801027BC) & 0xFFF;
        if (d > 0x200 && d < 0x800)
            yaw = D_801027BC + 0x200;
        else if (d > 0x800 && d < 0xE00)
            yaw = D_801027BC - 0x200;
        speed = D_800DB1D4.speed >> 4;
        yaw = (0x800 - yaw) & 0xFFF;
        if (speed < 4)
            speed = 4;
        ang->vy = snobo_angle_step(yaw, ang->vy, speed);
    }
    seg = D_801028D4;
    if (seg->flags[D_801027C4] & 8)
        pitch = 0x40;
    else if (D_800DB1D4.air_time > 8)
        pitch = 0x80;
    else
        pitch = D_800C000C[seg->piece[D_801027C4]].pitch * 3;
    d = ang->vx;
    pitch = pitch + 0x40 - d;
    if (pitch) {
        if (pitch > 0)
            d += 4;
        else
            d -= 4;
        ang->vx = d;
    }
    ang->vz = 0;
    RotMatrix(&D_800FFAF4.ang, &D_801027F4);
    ApplyMatrixSV(&D_801027F4, &D_800FFAF4.at, &sc->sv);
    D_801027F4.t[0] = -sc->sv.vx;
    D_801027F4.t[1] = 0x80 - sc->sv.vy;
    D_801027F4.t[2] = 0x140 - sc->sv.vz;
    sc->sv.vx = (D_800DB1D4.pos.vx >> 12) - D_8010284C.x;
    sc->sv.vy = (D_800DB1D4.pos.vy >> 12) - D_8010284C.y;
    sc->sv.vz = (D_800DB1D4.pos.vz >> 12) - D_8010284C.z;
    SetGeomOffset(0, 0);
    SetGeomScreen(0xA0);
    SetRotMatrix(&D_801027F4);
    SetTransMatrix(&D_801027F4);
    gte_ldv0(&sc->sv);
    gte_rtps();
    gte_stsxy(&sc->u.sxy);
    if (sc->u.sxy.vy < 0)
        d = -sc->u.sxy.vy;
    else if (sc->u.sxy.vy > 0x40)
        d = (0x40 - sc->u.sxy.vy) * 2;
    else
        d = 0;
    D_801027F4.t[1] += d;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_camera_follow);
#endif

/* The camera before the start (fixed 1: the start view, held level while button 0 is down) or while riding: as
 * snobo_camera_follow, around the start angles and offset D_800BFE90. */
#ifdef NON_MATCHING
/* The game loads D_801027BC with lhu here (lh elsewhere); ours takes one more stack slot; registers. */
void snobo_camera_start_or_follow(Scratch *sc, int fixed)
{
    SVECTOR *ang;
    Camera *base;
    int yaw, d, pitch, speed;
    short lift, x;

    if (D_801027C4 < 0)
        return;
    ang = &D_800FFAF4.ang;
    base = &D_800BFE90;
    if (fixed == 1)
        D_800FFAF4.ang.vy = snobo_angle_step((D_800BFE90.ang.vy - D_801027BC) & 0xFFF, D_800FFAF4.ang.vy, 0x40);
    else {
        switch (D_800BFF8C[(unsigned char)D_800DB1D4.state]) {
        case 1:
            if (D_800DB1D4.speed > 0x80) {
                yaw = ratan2(-D_800DB1D4.vx, -D_800DB1D4.vz) & 0xFFF;
                goto turn;
            }
        case 2:
            yaw = D_801027BC;
        turn:
            d = (yaw - D_801027BC) & 0xFFF;
            if (d > 0x400 && d < 0xC00)
                yaw = (yaw + 0x800) & 0xFFF;
            d = (yaw - D_801027BC) & 0xFFF;
            if (d > 0x200 && d < 0x800)
                yaw = D_801027BC + 0x200;
            else if (d > 0x800 && d < 0xE00)
                yaw = D_801027BC - 0x200;
            speed = D_800DB1D4.speed >> 4;
            if (speed < 4)
                speed = 4;
            ang->vy = snobo_angle_step((0x800 - yaw) & 0xFFF, ang->vy, (short)speed);
        }
    }
    if (D_801028D4->flags[D_801027C4] & 8)
        pitch = 0x40;
    else if (D_800DB1D4.air_time > 8)
        pitch = 0x80;
    else
        pitch = D_800C000C[D_801028D4->piece[D_801027C4]].pitch * 3;
    pitch += base->ang.vx;
    if (fixed == 1 && D_801027AC & 1)
        ang->vx = pitch;
    else {
        x = ang->vx;
        d = (short)(pitch - x);
        if (d) {
            if (d > 0)
                x += 4;
            else
                x -= 4;
            ang->vx = x;
        }
    }
    ang->vz = base->ang.vz;
    RotMatrix(&D_800FFAF4.ang, &sc->m);
    ApplyMatrixSV(&sc->m, &D_800FFAF4.at, &sc->sv);
    sc->m.t[0] = base->at.vx - sc->sv.vx;
    sc->m.t[1] = base->at.vy - sc->sv.vy;
    sc->m.t[2] = base->at.vz - sc->sv.vz;
    sc->sv.vx = (D_800DB1D4.pos.vx >> 12) - D_8010284C.x;
    sc->sv.vy = (D_800DB1D4.pos.vy >> 12) - D_8010284C.y;
    sc->sv.vz = (D_800DB1D4.pos.vz >> 12) - D_8010284C.z;
    SetGeomOffset(0, 0);
    SetGeomScreen(0xA0);
    SetRotMatrix(&sc->m);
    SetTransMatrix(&sc->m);
    gte_ldv0(&sc->sv);
    gte_rtps();
    gte_stsxy(&sc->u.sxy);
    if (sc->u.sxy.vy < 0)
        lift = -sc->u.sxy.vy;
    else if (sc->u.sxy.vy > 0x40)
        lift = (0x40 - sc->u.sxy.vy) * 2;
    else
        lift = 0;
    sc->m.t[1] += lift;
    D_801027F4 = sc->m;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_camera_start_or_follow);
#endif

/* Set the drawing origin D_8010284C for the course's segment D_801028E0: a vertex of its start piece, picked by the
 * face under the player, turned by the start yaw. */
#ifdef NON_MATCHING
/* Scheduling: the game loads D_801027C8 after the first matrix store. */
void snobo_set_draw_origin(Scratch *sc)
{
    short step = D_800BFE80[D_801028E0];
    short *cs = &D_8004BC98[(D_80102818 & 0xFFF) * 2];
    short s = cs[0];
    short c = cs[1];
    short k;
    Piece *pc;
    SVECTOR *vert;
    Pos *o;

    sc->m.m[0][0] = c;
    sc->m.m[0][1] = 0;
    sc->m.m[1][0] = 0;
    sc->m.m[1][2] = 0;
    sc->m.m[2][1] = 0;
    sc->m.m[0][2] = s;
    sc->m.m[2][0] = -s;
    sc->m.m[2][2] = c;
    sc->m.m[1][1] = 0x1000;
    k = (short)(D_801027C8 >> 1);
    pc = &D_800C000C[D_800D067C[step]];
    vert = &pc->v[D_800BFFAC[(short)(k % 6)]];
    sc->sv.vx = vert->vx;
    sc->sv.vy = vert->vy;
    sc->sv.vz = vert->vz - 0x80;
    ApplyMatrix(&sc->m, &sc->sv, &sc->v);
    o = &D_800D123C[step];
    D_8010284C.x = sc->v.vx + o->x;
    D_8010284C.y = sc->v.vy + o->y;
    D_8010284C.z = sc->v.vz + o->z;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_set_draw_origin);
#endif

/* The camera at the start of the run: the fixed start angles and offset around the player. */
void snobo_camera_start(Scratch *sc)
{
    unsigned short yaw = D_80102818;

    D_800FFAF4.at.vx = (D_800DB1D4.pos.vx >> 12) - D_8010284C.x;
    D_800FFAF4.at.vy = (D_800DB1D4.pos.vy >> 12) - D_8010284C.y;
    D_800FFAF4.at.vz = (D_800DB1D4.pos.vz >> 12) - D_8010284C.z;
    D_800FFAF4.ang.vx = D_800BFE90.ang.vx;
    D_800FFAF4.ang.vy = (D_800BFE90.ang.vy - yaw) & 0xFFF;
    D_800FFAF4.ang.vz = D_800BFE90.ang.vz;
    RotMatrix(&D_800FFAF4.ang, &sc->m);
    ApplyMatrixSV(&sc->m, &D_800FFAF4.at, &sc->sv);
    sc->m.t[0] = D_800BFE90.at.vx - sc->sv.vx;
    sc->m.t[1] = D_800BFE90.at.vy - sc->sv.vy;
    sc->m.t[2] = D_800BFE90.at.vz - sc->sv.vz;
    D_801027F4 = sc->m;
}

/* Aim the camera from a fixed height straight at the player. */
void snobo_camera_aim_at_player(Scratch *sc)
{
    int x = (D_800DB1D4.pos.vx >> 12) - D_8010284C.x;
    int z = (D_800DB1D4.pos.vz >> 12) - D_8010284C.z;

    sc->sv.vx = ratan2((D_800DB1D4.pos.vy >> 12) - D_8010284C.y, SquareRoot0(x * x + z * z));
    sc->sv.vy = -ratan2(x, z);
    sc->sv.vz = 0;
    RotMatrix(&sc->sv, &sc->m);
    sc->m.t[0] = 0;
    sc->m.t[1] = 0x60;
    sc->m.t[2] = 0;
    D_801027F4 = sc->m;
}

/* Draw three lines (D_800BFFB8, D_800BFFE8) through m, all into one ordering table entry. */
#ifdef NON_MATCHING
/* The loop counter's clear lands one instruction earlier. */
void snobo_draw_lines(MATRIX *m)
{
    SVECTOR sv;
    int otz;
    LINE_F2 *p;
    SVECTOR *e;
    int i;

    gte_SetRotMatrix(m);
    gte_SetTransMatrix(m);
    p = (LINE_F2 *)D_80102798;
    e = D_800BFFB8;
    for (i = 0; i < 12;) {
        setlen(p, 3);
        setcode(p, 0x40);
        p->r0 = D_800BFFE8[i];
        p->g0 = D_800BFFE8[i + 1];
        p->b0 = D_800BFFE8[i + 2];
        sv.vx = e->vx;
        sv.vy = e->vy;
        sv.vz = e->vz;
        e++;
        gte_ldv0(&sv);
        gte_rtps();
        gte_stsxy(&p->x0);
        gte_stszotz(&otz);
        sv.vx = e->vx;
        sv.vy = e->vy;
        sv.vz = e->vz;
        e++;
        gte_ldv0(&sv);
        gte_rtps();
        gte_stsxy(&p->x1);
        gte_stszotz(&otz);
        i += 4;
        addPrim(D_80102794, p);
        p++;
    }
    D_80102798 = (unsigned int *)p;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_draw_lines);
#endif

/* Draw a blue-white triangle tri[0-2] on the course at the current step (into the second ordering table entry). */
void snobo_draw_marker_triangle(Scratch *sc, SVECTOR *tri)
{
    short *cs;
    short s, c;
    Pos *o;
    POLY_G3 *p;
    int yaw;
    MATRIX *m;

    sc->u.step = D_801027C4;
    yaw = D_801028D4->angle[sc->u.step];
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
    o = &D_801028D4->origin[sc->u.step];
    sc->m.t[0] = o->x - D_8010284C.x;
    sc->m.t[1] = o->y - D_8010284C.y;
    sc->m.t[2] = o->z - D_8010284C.z;
    m = &sc->m;
    gte_CompMatrix(&D_801027F4, m, m);
    gte_SetRotMatrix(m);
    gte_SetTransMatrix(m);
    p = (POLY_G3 *)D_80102798;
    *(unsigned int *)&p->tag = 0x06000000;
    *(unsigned int *)&p->r0 = 0x30F08080;
    *(unsigned int *)&p->r1 = 0x0080F080;
    *(unsigned int *)&p->r2 = 0x008080F0;
    gte_ldv3(&tri[0], &tri[1], &tri[2]);
    gte_rtpt();
    gte_stsxy3(&p->x0, &p->x1, &p->x2);
    AddPrim(D_80102794 + 1, p);
    D_80102798 = (unsigned int *)(p + 1);
}

/* The sky, in the farthest ordering table entry: one flat colour where the course is covered (flag 2), else a strip
 * of 64x96 sprites from the sky texture scrolled by the camera's yaw and pitch, and two gradients below it. */
#ifdef NON_MATCHING
/* GCC hoists y << 16 and spills; the game writes the sprite and tpage words through a second pointer. */
void snobo_draw_sky(void)
{
    unsigned int *p = D_80102798;
    unsigned int *ot = D_80102794 + 0xFFF;
    unsigned int *q;
    int x, y, u, col, i, row;
    SkyBand *band;

    if (D_801028D4->flags[D_801027C4] & 2) {
        p[0] = 0x03000000;
        p[1] = 0x6006203C;
        p[2] = 0xFF88FF60;
        p[3] = 0x00F00140;
        addPrim(ot, p);
        D_80102798 = p + 4;
        return;
    }
    x = D_800FFAF4.ang.vx;
    if (x >= 0x800)
        x |= 0xF000;
    y = -((short)x >> 2) - 0x40;
    q = p + 1;
    u = 0x400 - ((short)D_800FFAF4.ang.vy >> 2);
    col = (u & 0x3FF) >> 8;
    x = -((u & 0x3F) + 0xA0);
    u = u & 0x3FF & 0xC0;
    for (;;) {
        p[0] = 0x04000000;
        q[0] = 0x65000000;
        q[1] = y << 16 | (x & 0xFFFF);
        q[2] = (u & 0xFF) | 0x78000000;
        q[3] = 0x00600040;
        q += 5;
        addPrim(ot, p);
        p += 5;
        setlen(p, 1);
        q[0] = (func_80043CC0() == 1 || func_80043CC0() == 2 ? 0xE1000800 : 0xE1000200) | (col + 5);
        q += 2;
        x += 0x40;
        addPrim(ot, p);
        p += 2;
        if ((short)x >= 0xA0)
            break;
        u += 0x40;
        if (!(u & 0xFF))
            col = (col + 1) & 3;
    }
    y += 0x60;
    band = D_800BFFF4;
    for (i = 1; i != -1; i--) {
        p[0] = 0x08000000;
        p[3] = band->top;
        p[1] = band->top;
        p[7] = band->bottom;
        p[5] = band->bottom;
        row = (y + band->y) << 16;
        p[2] = row | 0xFF60;
        p[4] = row | 0xA0;
        row += band->h << 16;
        p[6] = row | 0xFF60;
        p[8] = row | 0xA0;
        band++;
        addPrim(ot, p);
        p += 9;
    }
    D_80102798 = p;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_draw_sky);
#endif

/* Brighten the whole screen by `level` (an additive grey TILE). */
void snobo_flash_screen(int level, void *ot)
{
    unsigned int *p;

    level |= level << 16 | level << 8;
    p = D_80102798;

    p[0] = 0x03000000;
    p[1] = level | 0x62000000;
    p[2] = 0xFF88FF60;
    p[3] = 0x00F00140;
    AddPrim(ot, p);
    p += 4;
    func_80044A68((DR_MODE *)p, 1, 0, getTPage(0, 1, 0, 0), 0);
    AddPrim(ot, p);
    D_80102798 = p + 3;
}

/* Fill the screen with colour rgb (a TILE). */
void snobo_fill_screen(unsigned int rgb, void *ot)
{
    unsigned int *p = D_80102798;

    p[0] = 0x03000000;
    p[1] = (rgb & 0xFFFFFF) | 0x61000000;
    p[2] = 0xFF88FF60;
    p[3] = 0x00F00140;
    AddPrim(ot, p);
    D_80102798 = p + 4;
}

void snobo_check_score(void)
{
    if (!D_80102814 && D_8010281C == 0x116)
        snobo_score_run();
}

/* The end of the run: score the three parts (D_801027CC, the time against two minutes, D_801027C0) and total them. */
void snobo_score_run(void)
{
    int n, c;

    n = 30 - (short)D_801027CC;
    if (n < 0)
        n = 0;
    D_8010279C = n;
    n = 30 - (((int)((D_801028D8.word >> 16 & 0xF) + (D_801028D8.word >> 20 & 0xF) * 10 + (D_801028D8.word >> 24 & 0xF) * 60 +
                     (D_801028D8.word >> 28) * 3600) - 120) >> 1);
    if (n > 30)
        n = 30;
    else if (n < 0)
        n = 0;
    c = (short)D_801027C0;
    D_8010279E = n;
    if (c > 40)
        c = 40;
    D_801027A0 = c;
    D_80102814 = 1;
    D_801028F0 = 1;
    D_801027A2 = D_8010279C + n + c;
}

/* Run the event script: jump to a trigger's op if one fired, then run ops until one asks to wait. */
#ifdef NON_MATCHING
/* The game's trigger loop isn't strength-reduced and its fired block sits before the loop. */
void snobo_run_events(void)
{
    int i;
    unsigned char *op;

    for (i = 0; i < 4; i++) {
        if (D_800FFC1C[i] && D_800FFC20[i]) {
            D_800FFC18 = D_800FFC24[i];
            D_800FFC20[i] = 0;
            D_800FFC1C[i] = 0;
            D_800D71A6 = 0;
            break;
        }
    }
    if (!D_800D71A6) {
        do {
            op = D_800D71D0[(unsigned char)D_800DB1D4.state][D_800FFC18];
            if (*op >= 14)
                break;
        } while (!D_800DB28C[*op](op));
    }
    snobo_pose_blend_step();
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_run_events);
#endif

int snobo_op_yield(void)
{
    return 1;
}

int snobo_op_pose(unsigned char *op)
{
    D_800D71A8 = op[1];
    snobo_pose_blend_start(0, op[2], op[3]);
    D_800FFC18++;
    return 1;
}

int snobo_op_loop(unsigned char *op)
{
    if (D_800FFC14 >= 3)
        return 1;
    D_800FFC14++;
    D_800FFC0C[D_800FFC14] = D_800FFC18 + 1;
    D_800FFC10[D_800FFC14] = op[1];
    D_800FFC18++;
    return 0;
}

int snobo_op_endloop(unsigned char *op)
{
    if (D_800FFC14 <= 0)
        return 1;
    if (D_800FFC10[D_800FFC14]) {
        D_800FFC10[D_800FFC14]--;
        D_800FFC18 = D_800FFC0C[D_800FFC14];
    } else {
        D_800FFC14--;
        D_800FFC18++;
    }
    return 0;
}

int snobo_op_goto(unsigned char *op)
{
    D_800FFC18 = op[1];
    return 0;
}

int snobo_op_arm_trigger(unsigned char *op)
{
    D_800FFC1C[op[1]] = 1;
    D_800FFC20[op[1]] = 0;
    D_800FFC24[op[1]] = op[2];
    D_800FFC18++;
    return 0;
}

int snobo_op_yield2(void)
{
    return 1;
}

int snobo_op_set_mode(unsigned char *op)
{
    D_800DB1D4.state = op[1];
    return 1;
}

int snobo_op_push(unsigned char *op)
{
    int n = op[1];
    unsigned short pc = D_800FFC18 + 1;

    D_800DB1D4.bounce = 0x1000;
    D_800DB1D4.air_time = 1;
    D_800DB1D4.vy = -n << 8;
    D_800FFC18 = pc;
    return 0;
}

int snobo_op_wait_button(unsigned char *op)
{
    unsigned short pad = D_801027AC;

    if (op[2])
        pad = ~pad;
    if (pad & D_800DB2C4[op[1]]) {
        D_800FFC18++;
        return 0;
    }
    return 1;
}

int snobo_op_switch(unsigned char *op)
{
    if (D_800FFC28)
        D_800FFC18 = op[D_800FFC28];
    else
        D_800FFC18++;
    return 0;
}

int snobo_op_disarm_trigger(unsigned char *op)
{
    D_800FFC1C[op[1]] = 0;
    D_800FFC18++;
    return 0;
}

int snobo_op_set_cue(unsigned char *op)
{
    unsigned char v = op[1];

    D_800FFC18++;
    D_800FFC2C = v;
    return 0;
}

int snobo_op_sound(unsigned char *op)
{
    snd_request.id = op[1] + 0x27;
    snd_request.arg[0] = op[3];
    snd_request.arg[1] = D_800DB2D4[op[2]];
    snd_submit_request();
    D_800FFC18++;
    return 0;
}

/* Starts a blend from the current pose to the pose step on from the last one (wrapping over the 117), over frames,
 * eased by ease. */
void snobo_pose_blend_start(int step, int frames, int ease)
{
    int i;
    int *dst, *src;

    if (step) {
        D_800D71A8 += step;
        if (D_800D71A8 < 0)
            D_800D71A8 = 0x74;
        if (D_800D71A8 > 0x74)
            D_800D71A8 = 0;
    }
    snobo_copy_pose((int *)D_800FFB14, (int *)D_80102860);
    dst = (int *)D_800FFB88;
    src = (int *)D_800D39E4[D_800D71A8];
    for (i = 28; i != -1; i--)
        *dst++ = *src++;
    D_800FFB04 = D_80102840;
    D_800FFB0C[0] = D_800D6EE8[D_800D71A8][0];
    D_800FFB0C[1] = D_800D6EE8[D_800D71A8][1];
    D_800FFB0C[2] = D_800D6EE8[D_800D71A8][2];
    D_800FFC00 = frames;
    D_800D71A6 = frames;
    D_800FFBFC = D_800D71AC[ease];
}

void snobo_copy_pose(int *dst, int *src)
{
    int i;

    for (i = 28; i != -1; i--)
        *dst++ = *src++;
}

/* Steps the pose blend one frame. */
void snobo_pose_blend_step(void)
{
    int t, i;
    short *from, *to, *dst;

    if (!D_800D71A6)
        return;
    D_800D71A6--;
    t = D_800FFBFC(((D_800FFC00 - D_800D71A6) << 12) / D_800FFC00);
    if (t < 0)
        t = 0;
    else if (t > 0x1000)
        t = 0x1000;
    from = D_800FFB14;
    to = D_800FFB88;
    dst = D_80102860;
    for (i = 0x38; i != -1; i--) {
        *dst++ = *from + ((*to - *from) * t >> 12);
        from++;
        to++;
    }
    from = (short *)&D_800FFB04;
    to = D_800FFB0C;
    dst = (short *)&D_80102840;
    for (i = 2; i != -1; i--) {
        *dst++ = *from + ((*to - *from) * t >> 12);
        from++;
        to++;
    }
}

int snobo_ease_linear(int a)
{
    return a;
}

int snobo_ease_in_out(int a)
{
    return (rsin((a >> 2) * 2 - 0x400) + 0x1000) >> 1;
}

int snobo_ease_in(int a)
{
    return 0x1000 - rsin((a >> 2) + 0x400);
}

int snobo_ease_out(int a)
{
    return -rsin((a >> 2) + 0x800);
}

void snobo_rider_reset_sound(void)
{
    D_800D71A6 = 0;
    D_800FFC04 = 0;
    D_800FFC08 = 0x7F;
}

/* The rider's frame: runs its state (entering it first if it just changed), then builds its matrix at pos and, if
 * draw, its shadow. */
#ifdef NON_MATCHING
/* The game builds &D_800DB1D4.state in a register for the state |= 0x8000 and the dispatch; GCC
 * hoists &D_800DB1D4 to the top instead. */
void snobo_rider_frame(SVECTOR *pos, Scratch *s, int update, int draw)
{
    Board *b = &D_800DB1D4;
    int i, enter;
    short *sc;
    short sn, c;

    if (update) {
        enter = 0;
        if (!((short)b->state & 0x8000)) {
            D_800FFC18 = 0;
            D_800FFC14 = 0;
            D_800FFC28 = 0;
            D_800FFC2C = 0;
            D_800D71A6 = 0;
            D_800DB1D4.flags = 0;
            for (i = 0; i < 4; i++) {
                D_800FFC1C[i] = 0;
                D_800FFC20[i] = 0;
            }
            enter = 1;
            D_800DB1D4.state_time = 0;
            b->state |= 0x8000;
        }
        D_800DB2E4[D_800DB1D4.state & 0xFF](&D_800DB1D4, enter, s);
        D_800DB1D4.state_time++;
        snobo_board_sound(&D_800DB1D4);
    }
    RotMatrixYXZ(&D_800DB1D4.rot, &s->m);
    sc = &D_8004BC98[(D_800DB1D4.facing.vy & 0xFFF) * 2];
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
    MulMatrix0(&s->m, &D_800D71BC, &D_800DB1D4.mat);
    D_800DB1D4.mat.t[0] = pos->vx;
    D_800DB1D4.mat.t[1] = pos->vy;
    D_800DB1D4.mat.t[2] = pos->vz;
    snobo_pose_skeleton(&D_800DB1D4.mat, &D_801027F4);
    if (draw)
        snobo_rider_shadow(s);
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_rider_frame);
#endif

/* The rider's shadow: shown once he is higher above the ground than his state allows (D_800DB364), until he lands;
 * queued as a draw item flat on the ground under him. */
void snobo_rider_shadow(Scratch *s)
{
    short h = (D_800DB1D4.ground_y - D_800DB1D4.pos.vy) >> 12;
    short *sc;
    short sn, c;
    Queued *d;

    if (D_800DB1D0) {
        if (h <= 0)
            D_800DB1D0 = 0;
    } else if (D_800DB364[D_800DB1D4.state & 0xFF] && D_800DB364[D_800DB1D4.state & 0xFF] < h) {
        D_800DB1D0 = 1;
    }
    if (!D_800DB1D0)
        return;
    RotMatrixYXZ(&D_800DB1D4.ground_rot, &s->m);
    sc = &D_8004BC98[(D_800DB1D4.facing.vy & 0xFFF) * 2];
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
    s->m.t[0] = D_800DB1D4.mat.t[0];
    s->m.t[1] = (D_800DB1D4.ground_y >> 12) - D_8010284C.y;
    s->m.t[2] = D_800DB1D4.mat.t[2];
    gte_CompMatrix(&D_801027F4, &s->m, &s->m);
    d = &D_80101194[D_80102954++];
    d->m = s->m;
    d->model = 0x110;
    d->drawer = 3;
    d->how = 0;
    d->shade = 2;
}

void snobo_state_script(Board *b, int enter, Scratch *s)
{
    if (enter) {
        D_800DB1D4.f90 = 0;
        D_800DB1D4.f8E = 0;
        D_800DB1D4.f8C = 0;
    }
    snobo_run_events();
}

void snobo_state_ride(Board *b, int enter, Scratch *s)
{
    b->normal.pad = 0;
    b->rot.vy = 0;
    D_800FFC28 = b->speed < 0x100;
    snobo_run_events();
    if (D_801027B8 & 0x40 && b->air_time < 8) {
        b->state = snobo_jump_state(b);
    } else if (D_801027AC & 0x8000) {
        if (b->speed <= 0x20)
            b->facing.vy -= 0x20;
        else
            b->state = 0x11;
    } else if (D_801027AC & 0x2000) {
        if (b->speed <= 0x20)
            b->facing.vy += 0x20;
        else
            b->state = 0x12;
    } else if (D_801027AC & 0x4080 && b->speed > 0x100) {
        b->state = 0x16;
    } else if (b->speed < 0x100 && D_801027AC & 0x1000) {
        b->state = 2;
    }
    snobo_face_motion(b);
}

/* Carving left: the lean builds toward 0x20, or 0x80 with a d-pad up/down, and blends the pose with it. */
void snobo_state_carve_left(Board *b, int enter, Scratch *s)
{
    int i, max, step;
    int *dst, *src;

    if (enter) {
        b->normal.pad = 0;
        b->lean = 0;
        b->carve_time = 0;
        dst = (int *)D_800FFB14;
        src = (int *)D_800D5758;
        for (i = 28; i != -1; i--)
            *dst++ = *src++;
        D_800FFB04 = D_80102840;
    }
    if (D_801027AC & 0x8000)
        max = D_801027AC & 0xC ? 0x80 : 0x20;
    else
        max = 0;
    step = D_801027AC & 0xC ? 2 : 1;
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
    snobo_pose_lerp(D_800FFB14, D_800D4368, (short *)&D_800FFB04, D_800D6F66, max << 7);
    if (b->lean > 0x20)
        b->carve_time += b->lean >> 5;
    else if (b->carve_time)
        b->carve_time--;
    snobo_carve_turn(b, -b->lean, s);
    snobo_face_motion(b);
    if (snobo_carve_too_hard(b))
        b->state = 0x17;
    else if (D_801027B8 & 0x40 && b->air_time < 8)
        b->state = snobo_jump_state(b);
    else if (!b->lean)
        b->state = D_801027AC & 0x2000 ? 0x12 : 1;
    else if (D_801027AC & 0x4080 && b->speed > 0x100)
        b->state = 0x16;
    else if (b->speed < 0x100 && D_801027AC & 0x1000)
        b->state = 2;
}

/* Carving right: the lean builds toward 0x20, or 0x80 with a d-pad up/down, and blends the pose with it. */
void snobo_state_carve_right(Board *b, int enter, Scratch *s)
{
    int i, max, step;
    int *dst, *src;

    if (enter) {
        b->normal.pad = 0;
        b->lean = 0;
        b->carve_time = 0;
        dst = (int *)D_800FFB14;
        src = (int *)D_800D5758;
        for (i = 28; i != -1; i--)
            *dst++ = *src++;
        D_800FFB04 = D_80102840;
    }
    if (D_801027AC & 0x2000)
        max = D_801027AC & 0xC ? 0x80 : 0x20;
    else
        max = 0;
    step = D_801027AC & 0xC ? 2 : 1;
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
    snobo_pose_lerp(D_800FFB14, D_800D42F4, (short *)&D_800FFB04, D_800D6F60, max << 7);
    if (b->lean > 0x20)
        b->carve_time += b->lean >> 5;
    else if (b->carve_time)
        b->carve_time--;
    snobo_carve_turn(b, b->lean, s);
    snobo_face_motion(b);
    if (snobo_carve_too_hard(b))
        b->state = 0x18;
    else if (D_801027B8 & 0x40 && b->air_time < 8)
        b->state = snobo_jump_state(b);
    else if (!b->lean)
        b->state = D_801027AC & 0x8000 ? 0x11 : 1;
    else if (D_801027AC & 0x4080 && b->speed > 0x100)
        b->state = 0x16;
    else if (b->speed < 0x100 && D_801027AC & 0x1000)
        b->state = 2;
}

int snobo_jump_state(Board *b)
{
    if (b->speed > 0x400) {
        if (D_801028EC & 8)
            return 6;
        if (D_801028D4->flags[D_801027C4] & 1)
            return 6;
    }
    if (D_801027AC & 0x8000)
        return 5;
    if (D_801027AC & 0x2000)
        return 4;
    return 3;
}

/* Turns the velocity by the carve angle (less at speed and late in a jump) and slows it by how hard the turn is. */
inline void snobo_carve_turn(Board *b, int turn, Scratch *s)
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

int snobo_carve_too_hard(Board *b)
{
    return 0x8C - (b->speed * b->speed >> 16) < b->lean;
}

void snobo_state_scripted(Board *b, int enter, Scratch *s)
{
    b->rot.vy = 0;
    snobo_run_events();
    snobo_face_motion(b);
}

void snobo_state_scripted2(Board *b, int enter, Scratch *s)
{
    b->rot.vy = 0;
    snobo_run_events();
    snobo_face_motion(b);
}

void snobo_state_wobble(Board *b, int enter, Scratch *s)
{
    int r;
    short *sc;
    short sn, c;

    if (enter) {
        b->turn_target = (unsigned char)b->state == 8 ? -0x18 : 0x18;
        b->rot.vy = 0;
        b->normal.pad = 0;
        b->vx >>= 1;
        b->vz >>= 1;
        D_801027CC++;
    }
    if (D_801027AC & 0x8000)
        r = -((rand() & 0xF) + 4);
    else if (D_801027AC & 0x2000)
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
    snobo_run_events();
    snobo_face_motion(b);
}

#ifdef NON_MATCHING
/* The game masks the turn with sltu/negu/and (a store-flag) where GCC branches; sin and cos swap
 * registers. */
void snobo_state_push(Board *b, int enter, Scratch *s)
{
    short r;
    short *sc;
    short sn, c;

    if (enter)
        b->normal.pad = 1;
    if (D_800FFC2C) {
        s->v.vx = 0;
        s->v.vy = 0;
        s->v.vz = -0x3000;
        sc = &D_8004BC98[(b->facing.vy & 0xFFF) * 2];
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
        ApplyMatrixLV(&s->m, &s->v, &s->v);
        b->vx += s->v.vx;
        b->vz += s->v.vz;
        D_800FFC2C--;
    }
    r = 0x20 - (b->speed >> 4);
    if (D_801027AC & 0x8000)
        r = -r;
    else if (!(D_801027AC & 0x2000))
        r = 0;
    if (r) {
        b->facing.vy += r;
        sc = &D_8004BC98[(r & 0xFFF) * 2];
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
        ApplyMatrixLV(&s->m, (VECTOR *)&b->vx, (VECTOR *)&b->vx);
    }
    D_800FFC28 = !(b->speed < 0x100 && D_801027AC & 0x1000);
    snobo_run_events();
    if ((unsigned char)b->state != 2)
        b->normal.pad = 0;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_state_push);
#endif

void snobo_state_stopped_steer(Board *b, int enter, Scratch *s)
{
    if (enter) {
        D_800FFC28 = D_800FFC30;
        b->vx = 0;
        b->vz = 0;
        b->flags |= 1;
    }
    snobo_run_events();
    if (D_800FFC2C)
        b->facing.vy = snobo_angle_step(D_801027BC, b->facing.vy, 0x10);
}

void snobo_state_stopped(Board *b, int enter, Scratch *s)
{
    if (enter) {
        b->vz = 0;
        b->vx = 0;
        b->flags |= 1;
    }
    D_800FFC28 = D_801027AC >> 12 & 1;
    snobo_run_events();
}

void snobo_state_crouch(Board *b, int enter, Scratch *s)
{
    int target, v;

    if (enter) {
        b->turn_target = 0;
        b->lean = 0;
        b->normal.pad = 4;
    }
    target = 0;
    v = b->lean;
    if (D_801027AC & 0x4080)
        target = (b->turn_target == 0) << 5;
    if (v < target) {
        v += 2;
    } else if (target < v) {
        v -= 2;
        if (v < 0)
            v = 0;
    }
    b->lean = v;
    snobo_pose_lerp(D_800D4280, D_800D4280 + 0x137C, D_800D6F5A, D_800D6F5A + 0x102, -rsin((v << 5) + 0x800));
    if (!b->air_time) {
        b->vx -= b->vx >> 5;
        b->vz -= b->vz >> 5;
        snobo_face_motion(b);
    }
    if (!b->lean) {
        b->state = 1;
        b->normal.pad = 0;
    }
}

void snobo_state_spin_left(Board *b, int enter, Scratch *s)
{
    if (enter) {
        b->turn_target = -0x80;
        b->turn_rate = 0;
        b->turn_timer = 0x40;
        b->rot.vy = 0;
        b->normal.pad = 0;
        D_801027CC++;
    }
    snobo_carve_turn(b, -b->lean, s);
    snobo_spin_step(b, s);
}

void snobo_state_spin_right(Board *b, int enter, Scratch *s)
{
    if (enter) {
        b->turn_target = 0x80;
        b->turn_rate = 0;
        b->turn_timer = 0x40;
        b->rot.vy = 0;
        b->normal.pad = 0;
        D_801027CC++;
    }
    snobo_carve_turn(b, -b->lean, s);
    snobo_spin_step(b, s);
}

void snobo_spin_step(Board *b, Scratch *s)
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
        D_800FFC20[1] = 1;
    }
    b->facing.vy += b->turn_rate;
    snobo_run_events();
}

void snobo_state_crash(Board *b, int enter, Scratch *s)
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
        D_801027CC += 2;
        D_800FFC30 = b->speed > 0x500 ? D_80102958 % 3 + 1 : 0;
    }
    if (D_800FFC2C)
        snobo_crash_slowdown(b);
    snobo_run_events();
    snobo_face_motion(b);
}

void snobo_state_tumble(Board *b, int enter, Scratch *s)
{
    if (enter) {
        b->rot.vy = 0;
        D_801027CC++;
    }
    snobo_crash_slowdown(b);
    snobo_run_events();
    snobo_face_motion(b);
}

void snobo_crash_slowdown(Board *b)
{
    if (b->air_time >= 5)
        return;
    b->vx -= b->vx >> 5;
    b->vz -= b->vz >> 5;
    if (b->speed < 0x180)
        b->normal.pad = 3;
    if ((!D_800FFC28 && b->speed < 0x100) || b->state_time > 0xF0) {
        b->normal.pad = 0;
        D_800FFC28 = 1;
    }
}

void snobo_state_wait(Board *b, int enter, Scratch *s)
{
    snobo_run_events();
    if (D_800FFC2C && D_801027AC & 0x1000)
        b->state = 2;
}

void snobo_state_none(Board *b, int enter, Scratch *s)
{
}

/* Sets the pose (and root) t/4096 of the way from one to another. */
void snobo_pose_lerp(short *from, short *to, short *from_root, short *to_root, int t)
{
    int i;
    short *dst;

    dst = D_80102860;
    for (i = 0x38; i != -1; i--) {
        *dst++ = *from + ((*to - *from) * t >> 12);
        from++;
        to++;
    }
    from = from_root;
    to = to_root;
    dst = (short *)&D_80102840;
    for (i = 2; i != -1; i--) {
        *dst++ = *from + ((*to - *from) * t >> 12);
        from++;
        to++;
    }
}

void snobo_face_motion(Board *b)
{
    if (b->speed >= 0x80)
        b->facing.vy = snobo_angle_step(ratan2(b->vx, b->vz) + 0x800, b->facing.vy, 0x10);
}

/* Moves the rider: the slope push (fading with speed when it pushes along), drag in the air, then the speed. */
#ifdef NON_MATCHING
/* The last block only: the game keeps vx >> 8 in v1 where GCC uses v0. */
void snobo_rider_move(Board *b, Scratch *s)
{
    int push, x;

    if (!(b->flags & 1)) {
        if (b->air_time <= 0) {
            push = b->normal.vx >> b->normal.pad;
            b->vx += (b->vx ^ push) < 0 ? push : push - push * b->speed / 2048;
            push = b->normal.vz >> b->normal.pad;
            b->vz += (b->vz ^ push) < 0 ? push : push - push * b->speed / 2048;
            if (!((unsigned char)b->state >= 3 && (unsigned char)b->state <= 6) && D_801028EC & 8) {
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
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_rider_move);
#endif

void snobo_rider_fall(Board *b)
{
    b->vy += b->bounce;
    b->pos.vy += b->vy;
    if (b->f1C >= 0 && b->ground_y < b->pos.vy) {
        b->pos.vy = b->ground_y;
        b->bounce = b->vy;
        b->vy = 0;
        b->air_time = 0;
        D_800FFC20[0] = 1;
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
int snobo_angle_step(int to, int from, int max)
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

/* After a landing of the given impact: the state to go to (D_800DB3A4, by state, speed and impact; 0xFE: a hard
 * landing that costs speed, 0xFF: stay). */
void snobo_rider_land(Board *b, int impact)
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
    next = D_800DB3A4[i];
    switch (next) {
    case 0xFF:
        break;
    case 0xFE:
        snobo_play_effect(3, 0x135, 0);
        b->vx -= b->vx >> 2;
        b->vz -= b->vz >> 2;
        break;
    case 8:
        if ((short)(D_801027C8 % 12) >= 7)
            next++;
    default:
        snobo_play_effect(3, 0xEF, 0);
        b->state = next;
    }
}

/* The board's looping sound for its state, and its pitch from the speed. */
void snobo_board_sound(Board *b)
{
    short loop = 0;
    short pitch;

    if (b->air_time < 9) {
        loop = D_800DB4C4[(unsigned char)b->state];
        if (loop == 0xED && D_801028EC & 4)
            loop = 0x105;
    }
    if (loop != D_800FFC04) {
        D_800FFC04 = loop;
        if (!loop) {
            snd_request.id = 0xA4;
            D_800FFC08 = 0;
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
    if (pitch != D_800FFC08) {
        D_800FFC08 = pitch;
        snd_request.id = 0xA0;
        snd_request.arg[0] = pitch;
        snd_submit_request();
    }
}

/* Poses the rider: each joint's world rotation from the pose angles (D_80102860) down the skeleton, the joint
 * positions from the bone lengths, then one draw item per part, placed by the model matrix m and seen by view. */
#ifdef NON_MATCHING
/* Register allocation and loop strength reduction are far off. The game's 0x50 frame (16 of args, 32 of
 * locals in a leaf) hints at an inlined helper with a 32-byte local. */
void snobo_pose_skeleton(MATRIX *m, MATRIX *view)
{
    SkelWork *sp = (SkelWork *)0x1F800000;
    short *rot = D_80102860;
    Joint *j = D_800DB504;
    SkelPart *part;
    RotMat *w = sp->world;
    SVECTOR *p;
    Queued *d;
    int i;
    short *sc;
    short sn, c;

    for (i = 18; i != -1; i--) {
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
    j = D_800DB504;
    w = sp->world;
    p = sp->pos;
    for (i = 18; i != -1; i--) {
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

    j = D_800DB504;
    p = sp->pos;
    for (i = 18; i != -1; i--) {
        if (j->pos_parent >= 0) {
            p->vx += sp->pos[j->pos_parent].vx;
            p->vy += sp->pos[j->pos_parent].vy;
            p->vz += sp->pos[j->pos_parent].vz;
        }
        j++;
        p++;
    }

    gte_SetRotMatrix(m);
    gte_ldv0(&D_80102840);
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
    part = D_800DB9C4;
    p = sp->pts;
    for (i = 14; i != -1; i--) {
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
        part++;
        p++;
    }

    p = sp->pts;
    part = D_800DB9C4;
    sp->view = *view;
    gte_MulMatrix0(&sp->view, m, &sp->view_model);
    for (i = 14; i != -1; i--) {
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
        d = &D_80101194[D_80102954++];
        d->m = sp->part;
        d->how = 0;
        d->model = part->model;
        d->shade = 0;
        d->drawer = part->drawer;
        part++;
    }
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_pose_skeleton);
#endif

/* The HUD's draw mode: texture page at (0x180, 0x100). */
void snobo_init_hud_mode(void)
{
    func_80044A68(&D_800FFC34, 1, 0, GetTPage(0, 0, 0x180, 0x100), 0);
}

/* One frame on the clock: 1/60 s is 0.0167, so 7, 6 and 1 into the lowest three digits. */
BcdTime snobo_time_tick(BcdTime t)
{
    unsigned int d;
    int carry;

    d = t.d0 + 7;
    if (d >= 10) {
        d = t.d0 - 3;
        carry = 7;
    } else {
        carry = 6;
    }
    t.d0 = d;
    d = t.d1 + carry;
    carry = 1;
    if (d >= 10) {
        d -= 10;
        carry = 2;
    }
    t.d1 = d;
    t.d2 += carry;
    if (t.d2 < 10)
        return t;
    t.d2 -= 10;
    t.d3++;
    if (t.d3 < 10)
        return t;
    t.d3 -= 10;
    t.d4++;
    if (t.d4 < 10)
        return t;
    t.d4 -= 10;
    t.d5++;
    if (t.d5 < 6)
        return t;
    t.d5 -= 6;
    t.d6++;
    if (t.d6 < 10)
        return t;
    t.d6 -= 10;
    t.d7++;
    if (t.d7 >= 10)
        t.d7 = 0;
    return t;
}

/* The race clock: three fixed pieces, then six digits of BCD time from the top byte down. */
void snobo_draw_clock(time, x, y)
unsigned int time;
unsigned short x;
int y;
{
    Sprite *p = D_80102798;
    int xy = y << 16 | x;
    int digits;
    int i;

    for (i = 0; i < 3; i++)
        snobo_add_hud_sprite(p++, D_800DBA48[i], D_800DBA50[i], xy + D_800DBA5C[i]);
    for (i = 0, digits = time >> 8; i < 6; i++, digits >>= 4)
        snobo_add_hud_sprite(p++, (digits & 0xF) * 8, 8, xy + D_800DBA3C[i]);
    *(DR_MODE *)p = D_800FFC34;
    addPrim(&D_80102794[1], p);
    D_80102798 = (DR_MODE *)p + 1;
}

void snobo_add_hud_sprite(p, uv, w, xy)
Sprite *p;
int uv;
unsigned short w;
int xy;
{
    p->tag = 0x04000000;
    p->code = 0x65000000;
    p->uvclut = uv | 0x78400000;
    p->wh = w | 0xB0000;
    p->xy = xy;
    addPrim(&D_80102794[1], p);
}

/* A three-digit BCD number without leading zeros, then two fixed pieces of the HUD around it. The digits' sprites
 * are written as words (a Sprite's), which keeps GCC reloading the ordering table after each. */
#ifdef NON_MATCHING
/* GCC hoists 0xB0008 and allocates registers differently. */
void snobo_draw_number(n, x, y)
unsigned short n;
unsigned short x;
int y;
{
    unsigned int *p = D_80102798;
    unsigned int *q;
    int xy = y << 16 | x;
    unsigned short shown = 0;
    short shift;
    int d;

    for (shift = 8; shift > 0; shift -= 4, xy += 8) {
        d = (n >> shift) & 0xF;
        if (!shown && !d)
            continue;
        q = p;
        p += 5;
        q[0] = 0x04000000;
        q[4] = 0x000B0008;
        q[1] = 0x65000000;
        q[2] = xy;
        q[3] = d * 8 | 0x78400000;
        shown = 1;
        addPrim(&D_80102794[1], q);
    }
    q = p;
    p += 5;
    q[0] = 0x04000000;
    q[4] = 0x000B0008;
    q[1] = 0x65000000;
    q[2] = xy;
    q[3] = (n & 0xF) * 8 | 0x78400000;
    addPrim(&D_80102794[1], q);
    q = p;
    p += 5;
    q[0] = 0x04000000;
    q[1] = 0x65000000;
    q[2] = xy + 0xA;
    q[3] = 0x78400C00;
    q[4] = 0x000B001E;
    addPrim(&D_80102794[1], q);
    q = p;
    p += 5;
    q[0] = 0x04000000;
    q[1] = 0x65000000;
    q[2] = xy - 0x38;
    q[3] = 0x78400C20;
    q[4] = 0x000B0024;
    addPrim(&D_80102794[1], q);
    *(DR_MODE *)p = D_800FFC34;
    addPrim(&D_80102794[1], p);
    D_80102798 = (DR_MODE *)p + 1;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_draw_number);
#endif

/* A pulsing sprite over a darkened screen. */
#ifdef NON_MATCHING
/* The game keeps the pulse value in two registers; one move lands elsewhere. */
void snobo_draw_pulse_overlay(void)
{
    union {
        Sprite *sprt;
        Fill *tile;
        DR_MODE *mode;
    } p;
    unsigned short t = (unsigned char)D_80102958 & 0x1F;
    unsigned int c = t;
    unsigned int shade;

    p.sprt = D_80102798;
    if (c > 16)
        c = 32 - t;
    shade = (c * 2 + 0x40) & 0xFF;
    p.sprt->tag = 0x04000000;
    p.sprt->code = 0x64000000 | shade << 16 | shade << 8 | shade;
    p.sprt->xy = 0xFFFAFFE5;
    p.sprt->uvclut = 0x78400C44;
    p.sprt->wh = 0x000D0036;
    AddPrim(D_80102794, p.sprt++);
    p.tile->tag = 0x03000000;
    p.tile->code = 0x62000000;
    p.tile->xy = 0xFF88FF60;
    p.tile->wh = 0x00F00140;
    AddPrim(D_80102794, p.tile++);
    *p.mode = D_800FFC34;
    AddPrim(D_80102794, p.mode++);
    D_80102798 = p.mode;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_draw_pulse_overlay);
#endif

/* The BCD sum of the entries of D_800DBA64 picked by the bits of mask. */
unsigned short snobo_bcd_sum_mask(unsigned short mask)
{
    unsigned short sum = 0;
    unsigned short *p = D_800DBA64;

    for (; mask; mask >>= 1, p++) {
        if (mask & 1)
            sum = snobo_bcd_add(sum, *p);
    }
    return sum;
}

/* a + b in four-digit BCD. */
unsigned short snobo_bcd_add(unsigned short a, unsigned short b)
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
void snobo_clear_spray(void)
{
    Spray *p;
    int i;

    D_800FFD40 = 0;
    p = D_800FFC40;
    for (i = 7; i != -1; i--) {
        p->used = 0;
        p++;
    }
}

/* At speed, a new puff of snow behind the board, in the next free ring slot (or the oldest). */
void snobo_spawn_spray(Board *b)
{
    Spray *p;
    int i;
    int n;
    int v;

    if (b->speed < 0x80)
        return;
    i = D_800FFD40;
    n = 8;
    while (D_800FFC40[i].used) {
        if (!--n)
            break;
        i = (i + 1) & 7;
    }
    p = &D_800FFC40[i];
    p->used = 1;
    D_800FFD40 = (i + 1) & 7;
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

/* Draws the live puffs as growing, fading quads; with move, also steps them. */
void snobo_draw_spray(Board *b, MATRIX *view, SprayWork *w, int move)
{
    POLY_FT4 *prim;
    Spray *p;
    short x0, x1, y0, y1;
    int y;
    int i;

    w->view = *view;
    w->lw.m[0][0] = 0x1000;
    w->lw.m[0][1] = 0;
    w->lw.m[0][2] = 0;
    w->lw.m[1][0] = 0;
    w->lw.m[1][1] = 0x1000;
    w->lw.m[1][2] = 0;
    w->lw.m[2][0] = 0;
    w->lw.m[2][1] = 0;
    w->lw.m[2][2] = 0x1000;
    w->lw.t[0] = (b->pos.vx >> 12) - D_8010284C.x;
    w->lw.t[1] = (b->pos.vy >> 12) - D_8010284C.y;
    w->lw.t[2] = (b->pos.vz >> 12) - D_8010284C.z;
    CompMatrix(&w->view, &w->lw, &w->lw);
    SetRotMatrix(&w->lw);
    SetTransMatrix(&w->lw);
    prim = D_80102798;
    p = D_800FFC40;
    for (i = 7; i != -1; p++, i--) {
        if (!p->used)
            continue;
        gte_ldv0(&p->x);
        gte_rtps();
        gte_stsxy(&w->sx);
        gte_stszotz(&w->otz);
        x0 = w->sx - p->w;
        x1 = w->sx + p->w;
        y = w->sy + p->rise;
        y0 = y - p->h;
        y1 = y;
        if (y0 < -0x78 || y1 > 0x78) {
            p->used = 0;
            continue;
        }
        *(int *)&prim->x0 = y0 << 16 | (unsigned short)x0;
        *(int *)&prim->x1 = y0 << 16 | (unsigned short)x1;
        *(int *)&prim->x2 = y1 << 16 | (unsigned short)x0;
        *(int *)&prim->x3 = y1 << 16 | (unsigned short)x1;
        prim->tag = 0x09000000;
        *(int *)&prim->r0 = 0x2E000000 | p->shade << 16 | p->shade << 8 | p->shade;
        *(int *)&prim->u0 = 0x78C08000;
        *(int *)&prim->u1 = getTPage(0, 1, 0x140, 0x100) << 16 | 0x801F;
        *(int *)&prim->u2 = 0x8F00;
        *(int *)&prim->u3 = 0x8F1F;
        addPrim(&D_80102794[1], prim);
        prim++;
        if (!move)
            continue;
        p->shade -= p->fade;
        if (p->shade < 8) {
            p->used = 0;
            continue;
        }
        p->x += p->dx;
        p->y += p->dy;
        p->z += p->dz;
        p->w += p->dw;
        p->h += p->dh;
        p->rise += 4;
    }
    D_80102798 = prim;
}

/* A glowing quad on the board, brighter with speed, its texture cycling on each counted frame. */
#ifdef NON_MATCHING
/* Scheduling: the game loads sx, sy before D_800DBA84, and the ordering table before the last uv
 * stores. */
void snobo_draw_board_glow(Board *b, MATRIX *view, GlowWork *w, int count)
{
    POLY_FT4 *prim;
    unsigned int c;
    int x, y;
    int l, r, t, bot;
    int v;
    unsigned short uv;

    if (count)
        D_800DBA84++;
    if (b->air_time >= 3 || (D_801028EC & 4))
        return;
    prim = D_80102798;
    prim->tag = 0x09000000;
    c = b->speed >> 3;
    if (c > 0xF0)
        c = 0xF0;
    *(int *)&prim->r0 = 0x2E000000 | c << 16 | c << 8 | c;
    w->v.vx = (b->pos.vx >> 12) - D_8010284C.x;
    w->v.vy = (b->pos.vy >> 12) - D_8010284C.y;
    w->v.vz = (b->pos.vz >> 12) - D_8010284C.z;
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
    v = ((D_800DBA84 << 4) & 0x60) + 0x80;
    uv = v << 8;
    *(int *)&prim->x0 = t << 16 | (unsigned short)l;
    *(int *)&prim->x1 = t << 16 | (unsigned short)r;
    *(int *)&prim->x2 = bot << 16 | (unsigned short)l;
    *(int *)&prim->x3 = bot << 16 | (unsigned short)r;
    *(int *)&prim->u0 = 0x78C00000 | uv;
    *(int *)&prim->u1 = (getTPage(0, 1, 0x140, 0x100) << 16 | 0x1F) | uv;
    *(int *)&prim->u2 = (unsigned short)((v + 0x1F) << 8);
    *(int *)&prim->u3 = (unsigned short)((v + 0x1F) << 8 | 0x1F);
    AddPrim(D_80102794 + 1, prim);
    D_80102798 = prim + 1;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_draw_board_glow);
#endif

void snobo_relocate_model(Model *m)
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
/* GCC hoists the constants 1 and -1 out of the loop into s4/s5; the game doesn't. */
void *snobo_draw_queued(Model *m, unsigned int *ot, void *prim)
{
    DrawWork *d = (DrawWork *)0x1F800000;
    Queued *q = D_80101194;
    int i;

    gte_SetFarColor(0xFF, 0xFF, 0xFF);
    d->model = m;
    d->ot = ot;
    d->prim = prim;
    i = D_80102954;
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
        snobo_draw_part(d);
        q++;
    }
    D_80102954 = 0;
    return d->prim;
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_draw_queued);
#endif

/* Projects the part's vertices, three at a time, to the scratchpad, then has its drawer draw its faces. */
void snobo_draw_part(DrawWork *d)
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
    D_800DBA88[(unsigned short)d->drawer](d, part->faces, part->nfaces);
}

/* Gouraud triangles facing the camera, linked by their average depth. */
#ifdef NON_MATCHING
/* Temporary registers only (the counter in t9, the game t2). */
void snobo_draw_g3(DrawWork *d, FaceG3 *f, int n)
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
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_draw_g3);
#endif

/* As snobo_draw_g3 with the colours depth-cued by the drawer's shade. */
void snobo_draw_g3_cued(DrawWork *d, FaceG3 *f, int n)
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
void snobo_draw_ft3(DrawWork *d, FaceFT3 *f, int n)
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

/* As snobo_draw_g3, semi-transparent, each triangle preceded by a draw mode with the drawer's blend. */
#ifdef NON_MATCHING
/* Temporary registers only (as snobo_draw_g3). */
void snobo_draw_g3_blend(DrawWork *d, FaceG3 *f, int n)
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
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_draw_g3_blend);
#endif

void snobo_obj_knock(Obj *o)
{
    if (!o->hit)
        return;
    D_80101190 = o->id;
    if (o->hit & 2) {
        o->vel.x = o->knock_x;
        o->vel.y = -0x28000;
        o->vel.z = o->knock_z;
        snobo_play_effect(3, 0xF0, 0);
        o->t.radius = 0;
        o->t.update = snobo_obj_fly;
    } else {
        o->t.update = snobo_obj_expire;
    }
}

void snobo_obj_fly(Obj *o)
{
    if (o->t.pos.y > o->t.ground) {
        o->active = 0;
        return;
    }
    o->t.rot.pad = 0;
    o->id = D_8010281C;
    o->t.pos.x += o->vel.x;
    o->t.pos.y += o->vel.y;
    o->t.pos.z += o->vel.z;
    o->vel.y += 0x1000;
    o->t.rot.vx -= 0x20;
    if (o->vel.y > 0)
        o->t8A++;
}

void snobo_obj_expire(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F)
        o->active = 0;
}

void snobo_obj_hop(Obj *o)
{
    if (o->hit) {
        D_80101190 = o->id;
        if (o->hit & 2) {
            o->vel.x = o->knock_x;
            o->vel.y = -0x28000;
            o->vel.z = o->knock_z;
            snobo_play_effect(3, 0xF0, 0);
            o->t.radius = 0;
            o->t.update = snobo_obj_fly;
        } else {
            o->t.update = snobo_obj_expire;
        }
        return;
    }
    if (o->t84-- < 0) {
        o->vel.y = -0x8000;
        o->t84 = (rand() & 0x1F) + 0x20;
    }
    o->t.pos.y += o->vel.y;
    o->vel.y += 0x1000;
    if (o->t.pos.y > o->t.ground) {
        o->t.pos.y = o->t.ground;
        o->vel.y = 0;
    }
}

void snobo_obj_sway_idle(Obj *o)
{
    if (o->hit) {
        if (o->hit & 2) {
            o->t.radius = 0;
            snobo_play_effect(3, 0xF0, 0);
            o->t.update = snobo_obj_sway;
        } else {
            o->t.update = snobo_obj_expire_wobble;
        }
    }
    if (o->t.model - 0xEA == 2)
        snobo_obj_wobble(o);
}

void snobo_obj_sway(Obj *o)
{
    o->t.rot.vx = -rsin(rsin(o->t84 << 5) >> 4) >> 3;
    o->t.rot.pad = 0;
    if (o->t84++ >= 0x79 && o->t8A++ >= 0x1F)
        o->active = 0;
}

void snobo_obj_expire_wobble(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F)
        o->active = 0;
    if (o->t.model - 0xEA == 2)
        snobo_obj_wobble(o);
}

void snobo_obj_wobble(Obj *o)
{
    o->t.rot.vz = rsin(rsin(o->t86++ << 5) >> 4) >> 3;
    o->t.rot.pad = 0;
}

void snobo_obj_touch(Obj *o)
{
    short t = o->t84;

    if (t) {
        o->t84 = t + 1;
        if (t >= 0x3D && o->t8A++ >= 0x1F)
            o->active = 0;
        return;
    }
    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        snobo_play_effect(2, 0x10C, 0);
    }
    o->t84 = 1;
}

void snobo_obj_burst3(Obj *o)
{
    Obj *p;

    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        snobo_play_effect(3, 0xF0, 0);
        o->vel.x = -0x24C0;
        o->vel.z = -0x3480;
        o->vel.y = -0x24000;
        o->t.radius = 0;
        o->t.update = snobo_obj_fall;
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
        if ((D_801028D8.word & 0xF0000) == 0x70000)
            snobo_spawn_thrown(o->id, D_800DB1D4.facing.vy + 0x800, &o->t.pos, &o->vel);
    } else {
        o->t.update = snobo_obj_burst_expire;
    }
}

void snobo_obj_fall(Obj *o)
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

void snobo_obj_burst_expire(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F)
        o->active = 0;
}

void snobo_obj_burst_glow(Obj *o)
{
    Obj *s;
    int t = o->t86++ & 0x1F;

    if (t > 0x10)
        t = 0x20 - t;
    o->t.shade = t << 7;
    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        snobo_play_effect(3, 0x189, 0);
        /* Two more pieces, turned either way, from a copy in the scratchpad. */
        s = (Obj *)0x1F800098;
        o->vel.y = -0x20000;
        o->t.update = snobo_obj_fly_glow;
        o->vel.x = o->knock_x >> 1;
        o->vel.z = o->knock_z >> 1;
        o->t.model++;
        *s = *o;
        s->t.model++;
        snobo_obj_spawn_turned(s, 0, -0x300, (Scratch *)(s + 1));
        snobo_obj_spawn_turned(s, 0x800, 0x300, (Scratch *)(s + 1));
    } else {
        o->t.update = snobo_obj_expire_glow;
    }
}

void snobo_obj_fly_glow(Obj *o)
{
    int t = o->t86++ & 0x1F;

    if (t > 0x10)
        t = 0x20 - t;
    o->t.shade = t << 7;
    if (o->t.ground < (o->t.pos.y += o->vel.y)) {
        o->active = 0;
        return;
    }
    o->t.rot.pad = 0;
    o->vel.y += 0x1000;
    o->t.pos.x += o->vel.x;
    o->t.pos.z += o->vel.z;
    o->t.rot.vx += 0x20;
    if (o->vel.y > 0x8000)
        o->t8A++;
}

void snobo_obj_expire_glow(Obj *o)
{
    int t;

    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F) {
        o->active = 0;
        return;
    }
    t = o->t86++ & 0x1F;
    if (t > 0x10)
        t = 0x20 - t;
    o->t.shade = t << 7;
}

/* A copy of src in a free slot, its heading turned by dry and its velocity by angle. */
void snobo_obj_spawn_turned(src, dry, angle, t)
Obj *src;
short dry;
int angle;
Scratch *t;
{
    Obj *p = obj_alloc();
    short *cs;
    short s, c;

    if (!p)
        return;
    *p = *src;
    p->t.rot.vy = dry + p->t.rot.vy;
    p->t.rot.pad = 0;
    cs = &D_8004BC98[(angle & 0xFFF) * 2];
    s = cs[0];
    c = cs[1];
    t->m.m[0][0] = c;
    t->m.m[0][1] = 0;
    t->m.m[0][2] = s;
    t->m.m[1][0] = 0;
    t->m.m[1][1] = 0x1000;
    t->m.m[1][2] = 0;
    t->m.m[2][0] = -s;
    t->m.m[2][1] = 0;
    t->m.m[2][2] = c;
    ApplyMatrixLV(&t->m, (VECTOR *)&src->vel, &t->v);
    p->vel.x = t->v.vx;
    p->vel.z = t->v.vz;
}

void snobo_obj_glow(Obj *o)
{
    int t = o->t86++ & 0x1F;

    if (t > 0x10)
        t = 0x20 - t;
    o->t.shade = t << 7;
    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        snobo_play_effect(3, 0x189, 0);
        o->vel.y = -0x20000;
        o->t.update = snobo_obj_fly_glow;
        o->vel.x = o->knock_x >> 1;
        o->vel.z = o->knock_z >> 1;
    } else {
        o->t.update = snobo_obj_expire_glow;
    }
}

void snobo_obj_touch2(Obj *o)
{
    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        snobo_play_effect(2, 0x10C, 0);
    }
    o->t.update = snobo_obj_touch_expire;
}

void snobo_obj_touch_expire(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F)
        o->active = 0;
}

void snobo_obj_pickup(Obj *o)
{
    short t;

    if (o->hit) {
        o->active = 0;
        snobo_play_effect(3, 0xF9, 0);
        D_801027C0 += D_800DBD20[o->t.arg[0]];
        return;
    }
    t = o->t84++;
    o->t.model = (t >> 3 & 3) + 0xF0;
    o->t.pos.y += rsin(t << 5) * 2;
    if (abs(D_80101190 - o->id) < 2)
        o->t.update = snobo_obj_pickup_rise;
}

void snobo_obj_pickup_rise(Obj *o)
{
    if (o->t88++ >= 0x79 && o->t8A++ >= 0x1F) {
        o->active = 0;
        return;
    }
    o->t.model = (o->t84++ >> 3 & 3) + 0xF0;
    o->t.pos.y += o->vel.y;
    o->vel.y -= 0x800;
}

void snobo_obj_topple_idle(Obj *o)
{
    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        o->t.update = snobo_obj_topple;
        snobo_play_effect(1, 0x10C, 0x7F);
    } else {
        o->t.update = snobo_obj_topple_expire;
    }
}

void snobo_obj_topple(Obj *o)
{
    o->t.rot.vx += rsin(o->t84++ >> 1) >> 2;
    o->t.rot.pad = 0;
    if (o->t.rot.vx > 0x480)
        o->t.update = snobo_obj_topple_expire;
}

void snobo_obj_topple_expire(Obj *o)
{
    if (o->t86++ >= 0x3D && o->t8A++ >= 0x1F)
        o->active = 0;
}

void snobo_obj_pace(Obj *o)
{
    if (o->hit) {
        if (o->hit & 2) {
            o->t.radius = 0;
            o->t.update = snobo_obj_walker_hit;
        } else {
            o->t.update = snobo_obj_walker_expire;
        }
    }
    o->t.pos.x += o->vel.x;
    o->t.pos.z += o->vel.z;
    o->t.frame = D_800DBDBC[(o->t.arg[3]++ >> 1) % 6];
    if (abs(o->t.arg[o->f78] - o->t.pos.x) <= 0x7FFF) {
        o->f78 ^= 1;
        o->t.arg[2] = 0x10;
        o->t.update = snobo_obj_pace_turn;
        o->vel.x = -o->vel.x;
        o->vel.z = -o->vel.z;
    }
}

void snobo_obj_pace_turn(Obj *o)
{
    if (o->hit) {
        if (o->hit & 2) {
            o->t.radius = 0;
            o->t.update = snobo_obj_walker_hit;
        } else {
            o->t.update = snobo_obj_walker_expire;
        }
    }
    o->t.rot.vy += o->f78 ? -0x80 : 0x80;
    o->t.rot.pad = 0;
    o->t.frame = D_800DBDBC[(o->t.arg[3]++ >> 1) % 6];
    if (!--o->t.arg[2])
        o->t.update = snobo_obj_pace;
}

void snobo_obj_walker_hit(Obj *o)
{
    if (o->hit & 2) {
        o->hit = 0;
        o->vel.x = o->knock_x;
        o->vel.y = -0x20000;
        o->vel.z = o->knock_z;
        snobo_play_effect(3, 0x135, 0);
        o->t.model = 0x105;
        o->t.frame = 0x10B;
        D_80101190 = o->id;
    }
    if (o->t.ground < (o->t.pos.y += o->vel.y)) {
        o->active = 0;
        return;
    }
    o->vel.y += 0x1000;
    o->t.pos.x += o->vel.x;
    o->t.pos.z += o->vel.z;
    o->t.rot.vy += 0x100;
    o->t.rot.pad = 0;
    o->id = D_8010281C;
}

void snobo_obj_walker_expire(Obj *o)
{
    if (o->t84++ >= 0x3D && o->t8A++ >= 0x1F)
        o->active = 0;
}

void snobo_obj_wait_player(Obj *o)
{
    if (D_800DB1D4.pos.vz >= o->t.arg[1])
        return;
    if (D_800DB1D4.speed > 0x200)
        o->t.update = snobo_obj_walk;
    else
        o->t.update = snobo_obj_check_hit;
}

void snobo_obj_walk(Obj *o)
{
    if (o->hit) {
        if (o->hit & 2) {
            o->t.radius = 0;
            o->t.update = snobo_obj_walker_hit;
        } else
            o->t.update = snobo_obj_walker_expire;
    }
    o->t.pos.x += o->vel.x;
    o->t.pos.z += o->vel.z;
    o->t.frame = D_800DBDBC[(o->t.arg[3]++ >> 1) % 6];
    if (abs(o->t.arg[0] - o->t.pos.x) > 0xFFFF)
        return;
    o->t.frame = 0x106;
    o->t.update = snobo_obj_walk_turn;
    o->t.arg[2] = 8;
}

void snobo_obj_walk_turn(Obj *o)
{
    int r;

    if (o->hit) {
        if (o->hit & 2) {
            o->t.radius = 0;
            o->t.update = snobo_obj_walker_hit;
        } else
            o->t.update = snobo_obj_walker_expire;
    }
    r = o->t.rot.vy;
    if (o->vel.x > 0)
        r -= 0x80;
    else
        r += 0x80;
    o->t.rot.vy = r;
    o->t.rot.pad = 0;
    o->t.frame = D_800DBDBC[(o->t.arg[3]++ >> 1) % 6];
    if (--o->t.arg[2])
        return;
    o->t.update = snobo_obj_jump;
    o->t.arg[3] = 8;
    o->t.arg[2] = o->t.pos.y;
}

void snobo_obj_jump(Obj *o)
{
    if (o->hit & 2) {
        o->t.radius = 0;
        o->t.update = snobo_obj_walker_hit;
        return;
    }
    if (o->t.arg[3] > 0) {
        o->t.frame = 0x106;
        if (o->hit & 1) {
            o->t.update = snobo_obj_walker_expire;
            return;
        }
    } else if (o->t.arg[3] < 0) {
        o->t.frame = 0x10B;
        o->t.pos.y += o->vel.y;
        o->vel.y += 0x1000;
        if (o->vel.y > 0 && o->t.pos.y >= o->t.arg[2]) {
            o->t.pos.y = o->t.arg[2];
            o->t.arg[3] = 0x10;
        }
    } else if (D_80101190 == o->id) {
        o->t.update = snobo_obj_turn_back;
        o->t.arg[2] = 0xE;
        o->t.arg[3] = 0;
        D_80101190 = 0;
    } else
        o->vel.y = -0x8000;
    o->t.arg[3]--;
}

void snobo_obj_check_hit(Obj *o)
{
    if (!o->hit)
        return;
    if (o->hit & 2) {
        o->t.radius = 0;
        o->t.update = snobo_obj_walker_hit;
    } else
        o->t.update = snobo_obj_walker_expire;
}

void snobo_obj_turn_back(Obj *o)
{
    int r, t;

    if (o->hit) {
        if (o->hit & 2) {
            o->t.radius = 0;
            o->t.update = snobo_obj_walker_hit;
        } else
            o->t.update = snobo_obj_walker_expire;
    }
    t = o->t.arg[2]--;
    if (t > 0) {
        r = o->t.rot.vy;
        if (o->vel.x > 0)
            r += 0x80;
        else
            r -= 0x80;
        o->t.rot.vy = r;
        o->t.rot.pad = 0;
        o->t.frame = D_800DBDBC[(o->t.arg[3]++ >> 1) % 6];
    } else if (t >= -0xB3)
        o->t.frame = 0x106;
    else {
        o->t.model = 0x105;
        o->t.arg[2] = 0xC;
        o->t.update = snobo_obj_walk_turn;
    }
}

void snobo_spawn_thrown(int id, int rx, Pos *pos, Pos *vel)
{
    Obj *o;

    D_800DBE10.rot.vy = rx;
    D_800DBE10.pos.x = pos->x;
    D_800DBE10.pos.y = pos->y;
    D_800DBE10.pos.z = pos->z;
    o = obj_spawn(id, &D_800DBE10);
    /* no check: a full pool writes through NULL */
    o->vel.x = vel->x;
    o->vel.y = vel->y - 0x1000;
    o->vel.z = vel->z;
}

void snobo_obj_fall_fade(Obj *o)
{
    if (o->t84++ < 8) {
        o->t.drawer = 1;
        o->t.shade = (8 - o->t84) << 7;
    } else
        o->t.drawer = 0;
    o->vel.y += 0x1000;
    o->t.pos.y += o->vel.y;
    if (o->t.ground < o->t.pos.y) {
        o->t.update = snobo_obj_landed;
        o->t84 = 0x3C;
    }
    if (!o->t86 && o->t.ground - 0x20000 < o->t.pos.y) {
        snobo_play_effect(3, 0x1AB, 0);
        o->t86 = 1;
    }
    if (o->hit & 2) {
        snobo_play_effect(2, 0xF0, 0);
        o->hit = 0;
        o->t.radius = 0;
    }
}

void snobo_obj_landed(Obj *o)
{
    if (o->t84) {
        o->t84--;
        if (o->hit & 2) {
            snobo_play_effect(2, 0xF0, 0);
            o->hit = 0;
            o->t.radius = 0;
        }
        return;
    }
    o->t.radius = 0;
    if (o->t8A++ >= 0x1F)
        o->active = 0;
}

void snobo_spawn_kind(int id, int rx, short kind, VECTOR *pos)
{
    if (kind & 2) {
        D_800DBE58.update = 0;
        D_800DBE58.radius = 0;
    } else
        D_800DBE58.update = snobo_obj_wake_near_rider;
    D_800DBE58.model = (kind & 1) + 0x10C;
    D_800DBE58.rot.vy = rx;
    D_800DBE58.pos.x = pos->vx;
    D_800DBE58.pos.y = pos->vy - pos->pad;
    D_800DBE58.ground = pos->vy + 0x20000;
    D_800DBE58.pos.z = pos->vz;
    D_800DBE58.arg[0] = kind;
    obj_spawn(id, &D_800DBE58);
}

#ifdef NON_MATCHING
/* Scheduling only: the game clears rot.pad right after the first four loads and adds y after the z
 * store. */
void snobo_obj_bounce(Obj *o)
{
    int t = o->t84--;
    short pan;

    if (!t) {
        if (o->t86) {
            o->active = 0;
            return;
        }
        o->t.radius = 0;
        o->t84 = (rand() & 0x1F) + 8;
        o->t.update = snobo_obj_drop_timer;
        o->t8A = -1;
        return;
    }
    if (t < 0x10)
        o->t8A++;
    if (o->hit) {
        o->hit = 0;
        o->t.radius = 0;
        o->t86 = 1;
        snobo_play_effect(2, 0xF0, 0);
    }
    o->t.rot.pad = 0;
    o->t.pos.x += o->vel.x;
    o->t.pos.z += o->vel.z;
    o->t.pos.y += o->vel.y;
    o->vel.y += 0x1000;
    o->t.rot.vz += -o->vel.x >> 11;
    if (o->t.ground >= o->t.pos.y)
        return;
    o->t.pos.y = o->t.ground;
    o->vel.y = -o->vel.y >> 1;
    if (o->t88++ >= 3)
        return;
    pan = (7 - (o->id - D_8010281C)) << 4;
    if (pan >= D_800DBEA2 || (short)D_80102958 - D_800DBEA0 >= 0xB) {
        D_800DBEA2 = pan;
        D_800DBEA0 = D_80102958;
        snobo_play_effect(4, 0x1AB, pan);
    }
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_obj_bounce);
#endif

void snobo_spawn_flyer(int id, int rx, short kind, Pos *pos)
{
    int dx = 0xC0000;
    Obj *o;

    if (kind & 1)
        dx = -0xC0000;
    D_800DBEA8.rot.vy = rx;
    D_800DBEA8.arg[0] = pos->x + dx;
    D_800DBEA8.arg[1] = pos->y - 0x400000;
    D_800DBEA8.arg[2] = pos->z + dx;
    D_800DBEA8.ground = pos->y - 0x80000;
    D_800DBEA8.arg[3] = kind & 2 ? 0x68 : 0x90;
    o = obj_spawn(id, &D_800DBEA8);
    dx = D_800DBEF0[kind] << 12;
    o->t8A = -1;
    o->vel.x = dx;
    o->vel.z = dx;
}

void snobo_spawn_launched(int id, short kind, Pos *pos)
{
    Obj *o;

    D_800DBEF8.rot.vy = D_800DBF70[kind];
    D_800DBEF8.arg[0] = pos->x;
    D_800DBEF8.arg[1] = pos->y - 0x80000;
    D_800DBEF8.arg[2] = pos->z;
    D_800DBEF8.arg[3] = kind;
    o = obj_spawn(id, &D_800DBEF8);
    o->vel.x = D_800DBF40[kind][0];
    o->vel.y = D_800DBF40[kind][1];
    o->vel.z = D_800DBF40[kind][2];
    o->t8A = -1;
}

void snobo_obj_glide_wait(Obj *o)
{
    int wait;

    if (o->id - D_8010281C >= 4)
        return;
    if (--o->t84 != -1) {
        o->t8A = -1;
        return;
    }
    o->t.radius = 0xA0;
    o->t.pos.x = o->t.arg[0];
    o->t.pos.y = o->t.arg[1];
    o->t.pos.z = o->t.arg[2];
    wait = D_800DBF78[o->t.arg[3] >> 1];
    o->t8A = 0;
    o->t.update = snobo_obj_glide;
    o->t84 = wait;
}

void snobo_obj_glide(Obj *o)
{
    int t;

    if (o->hit) {
        o->hit = 0;
        o->t.radius = 0;
        o->t86 = 1;
        snobo_play_effect(2, 0xF0, 0);
    }
    t = o->t84--;
    if (!t) {
        if (o->t86)
            o->active = 0;
        else {
            o->hit = 0;
            o->t.update = snobo_obj_glide_wait;
            o->t84 = (rand() & 0x1F) + 8;
        }
    } else if (t < 0x10)
        o->t8A++;
    o->t.pos.x += o->vel.x;
    o->t.pos.y += o->vel.y;
    o->t.pos.z += o->vel.z;
    o->t.rot.vx += D_800DBF7C[o->t.arg[3]];
    o->t.rot.pad = 0;
}

void snobo_place_f0_0(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0x800, &D_800DBFD0[0], 0);
}

void snobo_place_f0_2(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0xB42, &D_800DBFD0[2], 0);
}

void snobo_place_f0_3(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0xB73, &D_800DBFD0[3], 0);
}

void snobo_place_f0_4(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0x772, &D_800DBFD0[4], 0);
}

void snobo_place_ea_5_6(int id, int on)
{
    if (!on)
        return;
    SPAWN_EA(id, 0xEA, 0x43A, &D_800DBFD0[5]);
    SPAWN_EA(id, 0xEA, 0x43A, &D_800DBFD0[6]);
}

void snobo_place_f0_7(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0x41C, &D_800DBFD0[7], 0);
}

void snobo_place_f0_ea_10(int id, int on)
{
    if (!on)
        return;
    SPAWN_F0(id, 0x41D, &D_800DBFD0[10], 0);
    SPAWN_EA(id, 0xEA, 0x41D, &D_800DBFD0[11]);
    SPAWN_EA(id, 0xEA, 0x41D, &D_800DBFD0[12]);
}

void snobo_place_ea_f0_13(int id, int on)
{
    if (!on)
        return;
    SPAWN_EA(id, 0xEA, 0x48D, &D_800DBFD0[13]);
    SPAWN_EA(id, 0xEA, 0x48D, &D_800DBFD0[14]);
    SPAWN_F0(id, 0x48D, &D_800DBFD0[15], 0);
}

void snobo_place_f0_17(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0x8C4, &D_800DBFD0[17], 0);
}

void snobo_place_f0_18(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0xC00, &D_800DBFD0[18], 0);
}

void snobo_place_f0_21(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0x858, &D_800DBFD0[21], 0);
}

void snobo_place_eb_22(int id, int on)
{
    if (on)
        SPAWN_EA(id, 0xEB, 0xE3A, &D_800DBFD0[22]);
}

void snobo_place_eb_23(int id, int on)
{
    if (on)
        SPAWN_EA(id, 0xEB, 0xD1C, &D_800DBFD0[23]);
}

void snobo_place_eb_24(int id, int on)
{
    if (on)
        SPAWN_EA(id, 0xEB, 0xC00, &D_800DBFD0[24]);
}

void snobo_place_eb_25(int id, int on)
{
    if (on)
        SPAWN_EA(id, 0xEB, 0xAE5, &D_800DBFD0[25]);
}

void snobo_place_f0_26(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0x83A, &D_800DBFD0[26], 0);
}

void snobo_place_f0_27(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0xEE5, &D_800DBFD0[27], 0);
}

void snobo_place_f0_29_30(int id, int on)
{
    if (!on)
        return;
    SPAWN_F0(id, 0xA00, &D_800DBFD0[29], 0);
    SPAWN_F0(id, 0xA00, &D_800DBFD0[30], 1);
}

void snobo_place_f0_31(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0xA00, &D_800DBFD0[31], 2);
}

void snobo_place_f0_32(int id, int on)
{
    if (on)
        SPAWN_F0(id, 0xA00, &D_800DBFD0[32], 0);
}

void snobo_place_f0_a(int id, int on)
{
    if (on)
        spawn_f0(id, 0x2007C00, 0, 0xA00, D_800DBFD0[33].x, D_800DBFD0[33].y - 0xA0000, D_800DBFD0[33].z);
}

void snobo_place_f0_b(int id, int on)
{
    if (on)
        spawn_f0(id, 0x2007C40, 1, 0xA00, D_800DBFD0[34].x, D_800DBFD0[34].y - 0xA0000, D_800DBFD0[34].z);
}

void snobo_place_f0_c(int id, int on)
{
    if (on)
        spawn_f0(id, 0x2007C00, 0, 0xA00, D_800DBFD0[35].x, D_800DBFD0[35].y - 0xA0000, D_800DBFD0[35].z);
}

void snobo_place_pair_bf84(int id, int on)
{
    if (!on)
        return;
    spawn_at(id, &D_800DBF84, D_800DBFCC, 0xA00, D_800DBFD0[36].x, D_800DBFD0[36].y, D_800DBFD0[36].z);
    spawn_at(id, &D_800DBF84, D_800DBFCC, 0xA00, D_800DBFD0[37].x, D_800DBFD0[37].y, D_800DBFD0[37].z);
}

void snobo_place_f0_trigger(int id, int on)
{
    int d;
    unsigned short *m;

    if (on)
        spawn_f0(id, 0x2007C80, 2, 0xA00, D_800DBFD0[38].x, D_800DBFD0[38].y - 0xA0000, D_800DBFD0[38].z);
    if (id != D_8010281C)
        return;
    if ((unsigned short)D_801027C8 - 24 >= 24u)
        return;
    m = &D_800DB1D4.state;
    if ((unsigned char)*m == 10 || (unsigned char)*m == 12)
        return;
    d = (D_800DB1D4.ground_y - D_800DB1D4.pos.vy) >> 12;
    if (d < 0x50)
        return;
    if (d < 0x1C0)
        *m = 12;
    else if (d < 0x1C8)
        *m = 10;
    else
        return;
    snobo_play_effect(2, 0xF0, 0);
}

void snobo_place_bf84_rel_0(int id, int on)
{
    Pos *p;

    if (!on)
        return;
    p = &D_800D123C[id];
    D_800FFD44[0][0] = (p->x - 0x25A) << 12;
    D_800FFD44[0][1] = (p->y + 2) << 12;
    D_800FFD44[0][2] = (p->z - 0x1BA) << 12;
    spawn_at(id, &D_800DBF84, D_800DBFCC, 0xA00, D_800FFD44[0][0], D_800FFD44[0][1], D_800FFD44[0][2]);
}

void snobo_place_bf84_rel_1(int id, int on)
{
    Pos *p;

    if (!on)
        return;
    p = &D_800D123C[id];
    D_800FFD44[1][0] = (p->x + 0x6C1) << 12;
    D_800FFD44[1][1] = (p->y + 2) << 12;
    D_800FFD44[1][2] = (p->z - 0xADB) << 12;
    spawn_at(id, &D_800DBF84, D_800DBFCC, 0xA00, D_800FFD44[1][0], D_800FFD44[1][1], D_800FFD44[1][2]);
}

void snobo_place_f0_rel_2(int id, int on)
{
    Pos *p;

    if (!on)
        return;
    p = &D_800D123C[id];
    D_800FFD44[2][0] = (p->x + 0xF) << 12;
    D_800FFD44[2][1] = (p->y - 0x13D) << 12;
    D_800FFD44[2][2] = (p->z - 0x2C8) << 12;
    spawn_f0(id, 0x2007C00, 0, 0xA00, D_800FFD44[2][0], D_800FFD44[2][1] - 0xA0000, D_800FFD44[2][2]);
}

void snobo_place_f0_rel_3(int id, int on)
{
    Pos *p;

    if (!on)
        return;
    p = &D_800D123C[id];
    D_800FFD44[3][0] = (p->x + 0x5B7) << 12;
    D_800FFD44[3][1] = (p->y - 0x13D) << 12;
    D_800FFD44[3][2] = (p->z - 0x870) << 12;
    spawn_f0(id, 0x2007C00, 0, 0xA00, D_800FFD44[3][0], D_800FFD44[3][1] - 0xA0000, D_800FFD44[3][2]);
}

void snobo_place_f0_rel_4(int id, int on)
{
    Pos *p;

    if (!on)
        return;
    p = &D_800D123C[id];
    D_800FFD44[4][0] = (p->x - 0xA1) << 12;
    D_800FFD44[4][1] = (p->y - 0x17A) << 12;
    D_800FFD44[4][2] = (p->z - 0x377) << 12;
    spawn_f0(id, 0x2007C40, 1, 0xA00, D_800FFD44[4][0], D_800FFD44[4][1] - 0xA0000, D_800FFD44[4][2]);
}

void snobo_place_f0_rel_5(int id, int on)
{
    Pos *p;

    if (!on)
        return;
    p = &D_800D123C[id];
    D_800FFD44[5][0] = (p->x + 0x507) << 12;
    D_800FFD44[5][1] = (p->y - 0x17A) << 12;
    D_800FFD44[5][2] = (p->z - 0x91F) << 12;
    spawn_f0(id, 0x2007C40, 1, 0xA00, D_800FFD44[5][0], D_800FFD44[5][1] - 0xA0000, D_800FFD44[5][2]);
}

void snobo_place_f0_eb_6(int id, int on)
{
    Pos *p;

    if (!on)
        return;
    p = &D_800D123C[id];
    D_800FFD44[6][0] = (p->x + 0xD0) << 12;
    D_800FFD44[6][1] = (p->y - 0x133) << 12;
    D_800FFD44[6][2] = (p->z - 0x7AC) << 12;
    spawn_f0(id, 0x2007C40, 1, 0xA00, D_800FFD44[6][0], D_800FFD44[6][1] - 0xA0000, D_800FFD44[6][2]);
    D_800FFD44[6][1] = (p->y + 0x14D) << 12;
    spawn_at(id, &D_800DBAE0, 0xEB, 0xA00, D_800FFD44[6][0], D_800FFD44[6][1], D_800FFD44[6][2]);
}

void snobo_place_f0_eb_7(int id, int on)
{
    Pos *p;

    if (!on)
        return;
    p = &D_800D123C[id];
    D_800FFD44[7][0] = (p->x + 0xD0) << 12;
    D_800FFD44[7][1] = (p->y - 0x173) << 12;
    D_800FFD44[7][2] = (p->z - 0x7AC) << 12;
    spawn_f0(id, 0x2007C80, 2, 0xA00, D_800FFD44[7][0], D_800FFD44[7][1] - 0xA0000, D_800FFD44[7][2]);
    D_800FFD44[7][1] = (p->y + 0x14D) << 12;
    spawn_at(id, &D_800DBAE0, 0xEB, 0xA00, D_800FFD44[7][0], D_800FFD44[7][1], D_800FFD44[7][2]);
}

/* Four of D_800DBD2C's model around course object id, at offsets picked by id & 3, two sizes alternating; x and z
 * jitter by a shared table that cycles across calls. */
#ifdef NON_MATCHING
/* With the jitter in a local (the game reads it once) GCC hoists the template copy's end address into
 * s0; this reads it again instead: one load more, and the scheduling around it. */
void snobo_place_e7_cluster(int id)
{
    Pos *p = &D_800D123C[id];
    short *q = D_800DC1A4[id & 3][0];
    int i;

    for (i = 3; i != -1; i--) {
        D_800FFD44[8][0] = (q[0] + p->x + D_800DC204[D_800DC20E]) << 12;
        D_800FFD44[8][1] = (q[1] + p->y) << 12;
        D_800FFD44[8][2] = (q[2] + p->z + D_800DC204[D_800DC20E]) << 12;
        spawn_at(id, &D_800DBD2C, 0xE7, i & 1 ? 0xD80 : 0xE80, D_800FFD44[8][0], D_800FFD44[8][1], D_800FFD44[8][2]);
        if (++D_800DC20E >= 4)
            D_800DC20E = 0;
        q += 3;
    }
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_place_e7_cluster);
#endif

void snobo_place_f0_cluster_a(int id, int on)
{
    if (!on)
        return;
    spawn_f0(id, 0x2007C00, 0, 0x600, D_800DC210.x, D_800DC210.y - 0xA0000, D_800DC210.z);
    snobo_place_e7_cluster(id);
}

void snobo_place_f0_cluster_b(int id, int on)
{
    if (!on)
        return;
    spawn_f0(id, 0x2007C00, 0, 0x600, D_800DC21C.x, D_800DC21C.y - 0xA0000, D_800DC21C.z);
    snobo_place_e7_cluster(id);
}

void snobo_place_f0_cluster_c(int id, int on)
{
    if (!on)
        return;
    spawn_f0(id, 0x2007C00, 0, 0x600, D_800DC228.x, D_800DC228.y - 0xA0000, D_800DC228.z);
    snobo_place_e7_cluster(id);
}

void snobo_place_f0_cluster_d(int id, int on)
{
    if (!on)
        return;
    spawn_f0(id, 0x2007C00, 0, 0x600, D_800DC234.x, D_800DC234.y - 0xA0000, D_800DC234.z);
    snobo_place_e7_cluster(id);
}

void snobo_place_f0_cluster_e(int id, int on)
{
    if (!on)
        return;
    spawn_f0(id, 0x2007C00, 0, 0x600, D_800DC240.x, D_800DC240.y - 0xA0000, D_800DC240.z);
    snobo_place_e7_cluster(id);
}

void snobo_place_f0_cluster_f(int id, int on)
{
    if (!on)
        return;
    spawn_f0(id, 0x2007C00, 0, 0x600, D_800DC24C.x, D_800DC24C.y - 0xA0000, D_800DC24C.z);
    snobo_place_e7_cluster(id);
}

void snobo_place_ec_pair_f0_a(int id, int on)
{
    if (!on)
        return;
    spawn_bump(id, 0xEC, snobo_obj_hop, 0xA00, D_800DC258.x, D_800DC258.y, D_800DC258.z);
    spawn_bump(id, 0xEC, snobo_obj_hop, 0xA00, D_800DC264.x, D_800DC264.y, D_800DC264.z);
    spawn_f0(id, 0x2007C00, 0, 0xA00, D_800DC270.x, D_800DC270.y - 0xA0000, D_800DC270.z);
}

void snobo_place_ec(int id, int on)
{
    if (on)
        spawn_bump(id, 0xEC, snobo_obj_hop, 0xDC5, D_800DC27C.x, D_800DC27C.y, D_800DC27C.z);
}

void snobo_place_ec_pair_f0_b(int id, int on)
{
    if (!on)
        return;
    spawn_bump(id, 0xEC, snobo_obj_hop, 0xE45, D_800DC288.x, D_800DC288.y, D_800DC288.z);
    spawn_bump(id, 0xEC, snobo_obj_hop, 0xE45, D_800DC294.x, D_800DC294.y, D_800DC294.z);
    spawn_f0(id, 0x2007C40, 1, 0xE28, D_800DC2A0.x, D_800DC2A0.y - 0xA0000, D_800DC2A0.z);
}

void snobo_place_f0_late(int id, int on)
{
    if (!on || D_801027A2 < 0x3D)
        return;
    spawn_f0(id, 0x2007CC0, 3, 0xBE3, D_800DC2B8.x, D_800DC2B8.y - 0xA0000, D_800DC2B8.z);
}

void snobo_place_bc90_pair_a(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC2C4.x, D_800DC2C4.y, D_800DC2C4.z);
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC2D0.x, D_800DC2D0.y, D_800DC2D0.z);
}

void snobo_place_bc90_pair_b(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC390.x, D_800DC390.y, D_800DC390.z);
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC39C.x, D_800DC39C.y, D_800DC39C.z);
}

void snobo_place_bc90_pair_c(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC3A8.x, D_800DC3A8.y, D_800DC3A8.z);
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC3B4.x, D_800DC3B4.y, D_800DC3B4.z);
}

void snobo_spawn_pair_3c0(int kind, int on)
{
    if (!on)
        return;
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC3C0.x, D_800DC3C0.y, D_800DC3C0.z);
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC3CC.x, D_800DC3CC.y, D_800DC3CC.z);
}

void snobo_spawn_pair_3e4(int kind, int on)
{
    if (!on)
        return;
    snobo_spawn_launched((short)kind, 3, &D_800DC3FC);
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC3E4.x, D_800DC3E4.y, D_800DC3E4.z);
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC3F0.x, D_800DC3F0.y, D_800DC3F0.z);
}

void snobo_spawn_pair_480(int kind, int on)
{
    if (!on)
        return;
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC480.x, D_800DC480.y, D_800DC480.z);
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC48C.x, D_800DC48C.y, D_800DC48C.z);
}

void snobo_spawn_pair_498(int kind, int on)
{
    if (!on)
        return;
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC498.x, D_800DC498.y, D_800DC498.z);
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC4A4.x, D_800DC4A4.y, D_800DC4A4.z);
}

void snobo_spawn_one_4b0(int kind, int on)
{
    if (!on)
        return;
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC4B0.x, D_800DC4B0.y, D_800DC4B0.z);
    snobo_spawn_launched((short)kind, 3, &D_800DC4BC[0]);
    snobo_spawn_launched((short)kind, 2, &D_800DC4BC[1]);
}

void snobo_spawn_pair_4d4(int kind, int on)
{
    if (!on)
        return;
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC4D4.x, D_800DC4D4.y, D_800DC4D4.z);
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC4E0.x, D_800DC4E0.y, D_800DC4E0.z);
}

void snobo_spawn_pair_4ec(int kind, int on)
{
    if (!on)
        return;
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC4EC.x, D_800DC4EC.y, D_800DC4EC.z);
    spawn_turned(kind, &D_800DBC90, 0x238, D_800DC4F8.x, D_800DC4F8.y, D_800DC4F8.z);
}

void snobo_throw_504(int kind, int on)
{
    Obj *p;

    if (!on)
        return;
    p = throw_obj(kind, &D_800DBD74, 0xA38, D_800DC504);
    p->f78 = D_800DC504[7];
}

void snobo_throw_524(int kind, int on)
{
    Obj *p;

    if (!on)
        return;
    p = throw_obj(kind, &D_800DBD74, 0x1238, D_800DC524);
    p->f78 = D_800DC524[7];
}

void snobo_throw_544(int kind, int on)
{
    Obj *p;

    if (!on)
        return;
    p = throw_obj(kind, &D_800DBD74, 0xA38, D_800DC544);
    p->f78 = D_800DC544[7];
}

void snobo_throw_564(int kind, int on)
{
    Obj *p;

    if (!on)
        return;
    p = throw_obj(kind, &D_800DBD74, 0x1238, D_800DC564);
    p->f78 = D_800DC564[7];
}

void snobo_throw_584(int kind, int on)
{
    Obj *p;

    if (!on)
        return;
    p = throw_obj(kind, &D_800DBD74, 0xA38, D_800DC584);
    p->f78 = D_800DC584[7];
}

void snobo_throw_pair_5a4(int kind, int on)
{
    Obj *p;

    if (!on)
        return;
    p = throw_obj(kind, &D_800DBD74, 0xA38, D_800DC5A4);
    p->f78 = D_800DC5A4[7];
    p = throw_obj(kind, &D_800DBD74, 0x1238, D_800DC5C4);
    p->f78 = D_800DC5C4[7];
}

void snobo_throw_5fc(int kind, int on)
{
    if (!on)
        return;
    throw_obj(kind, &D_800DBDC8, 0xD8E, D_800DC5FC);
}

void snobo_throw_624(int kind, int on)
{
    if (!on)
        return;
    throw_obj(kind, &D_800DBDC8, 0x58E, D_800DC624);
}

void snobo_throw_pair_658(int kind, int on)
{
    if (!on)
        return;
    throw_obj(kind, &D_800DBDC8, 0xD8E, D_800DC658);
    throw_obj(kind, &D_800DBDC8, 0xD8E, D_800DC674);
}

void snobo_spawn_pair_69c(int kind, int on)
{
    if (!on)
        return;
    spawn_turned(kind, &D_800DBB70, 0x98E, D_800DC69C.x, D_800DC69C.y, D_800DC69C.z);
    spawn_turned(kind, &D_800DBB70, 0x98E, D_800DC6A8.x, D_800DC6A8.y, D_800DC6A8.z);
}

void snobo_throw_pair_6b4(int kind, int on)
{
    if (!on)
        return;
    throw_obj(kind, &D_800DBDC8, 0x58E, D_800DC6B4);
    throw_obj(kind, &D_800DBDC8, 0xD8E, D_800DC6D0);
}

void snobo_spawn_pair_71c(int kind, int on)
{
    if (!on)
        return;
    spawn_turned(kind, &D_800DBB70, 0x98E, D_800DC71C.x, D_800DC71C.y, D_800DC71C.z);
    spawn_turned(kind, &D_800DBB70, 0x98E, D_800DC728.x, D_800DC728.y, D_800DC728.z);
}

void snobo_throw_pair_734(int kind, int on)
{
    if (!on)
        return;
    throw_obj(kind, &D_800DBDC8, 0xD8E, D_800DC734);
    throw_obj(kind, &D_800DBDC8, 0x58E, D_800DC750);
}

#ifdef NON_MATCHING
/* The copy of the id is scheduled after the 0x2007CC0 constant, not in the branch delay slot. */
void snobo_spawn_late_76c(int kind, int on)
{
    if (!on || D_801027A2 <= 80)
        return;
    D_800DBCD8.arg[0] = 3;
    D_800DBCD8.shade = 0x2007CC0;
    spawn_at(kind, &D_800DBCD8, 0xF0, 0x955, D_800DC76C.x, D_800DC76C.y - 0xA0000, D_800DC76C.z);
}
#else
INCLUDE_ASM("asm/overlay/snobo/nonmatchings/snobo", snobo_spawn_late_76c);
#endif

void snobo_board_tumble(void)
{
    int v;

    D_800DB1D4.state = 11;
    v = D_800DB1D4.vy;
    if (v < 0)
        v = -v;
    if (v < 0x40000)
        v = 0x40000;
    D_800DB1D4.vy = v;
    snobo_play_effect(2, 0x10C, 0);
}

/* The gated sections (snobo_section_props_NN, NN the first of their gates in D_800DC778): with on, their gates go
 * up; then, in the rider's own section, the jump off its lip. */
void snobo_section_props_00(int kind, int on)
{
    if (on)
        GATE_A(kind, 0xF8, 0xD1C, 0);
    check_jump(kind);
}

void snobo_section_props_01(int kind, int on)
{
    if (on) {
        GATE_A(kind, 0xFB, 0xD1C, 1);
        GATE_A(kind, 0xFE, 0xD1C, 2);
    }
    check_jump(kind);
}

void snobo_section_props_03(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xF8, 0xD1C, 3);
        GATE_B(owner, 0xF9, 0xD1C, 4);
        GATE_B(owner, 0xFC, 0x151C, 5);
    }
    check_jump(owner);
}

void snobo_section_props_06(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xFB, 0x51C, 6);
        GATE_A(owner, 0xFE, 0x51C, 7);
    }
    check_jump(owner);
}

void snobo_section_props_08(int owner, int on)
{
    if (on)
    GATE_A(owner, 0xF8, 0x51C, 8);
    check_jump(owner);
}

void snobo_section_props_09(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xFB, 0x51C, 9);
        GATE_A(owner, 0xFE, 0x51C, 10);
    }
    check_jump(owner);
}

void snobo_section_props_11(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xF8, 0x51C, 11);
        GATE_B(owner, 0xFF, 0x51C, 12);
        GATE_B(owner, 0xF9, 0xD1C, 13);
    }
    check_jump(owner);
}

void snobo_section_props_14(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xFB, 0x51C, 14);
        GATE_A(owner, 0xFE, 0x51C, 15);
    }
    check_jump(owner);
}

void snobo_section_props_16(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xF8, 0x51C, 16);
        GATE_B(owner, 0xFC, 0x51C, 17);
        GATE_B(owner, 0xFF, 0xD1C, 18);
    }
    check_jump(owner);
}

void snobo_section_props_19(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xFB, 0x51C, 19);
        GATE_A(owner, 0xFE, 0x51C, 20);
    }
    check_jump(owner);
}

void snobo_section_props_21(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xF8, 0x51C, 21);
        GATE_B(owner, 0xFC, 0x51C, 22);
        GATE_B(owner, 0xFF, 0xD1C, 23);
    }
    check_jump(owner);
}

void snobo_section_props_24(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xFB, 0x51C, 24);
        GATE_A(owner, 0xFE, 0x51C, 25);
        GATE_B(owner, 0xF9, 0x51C, 26);
        GATE_B(owner, 0xFC, 0xD1C, 27);
    }
    check_jump(owner);
}

void snobo_section_props_28(int owner, int on)
{
    if (on)
    GATE_A(owner, 0xF8, 0xD1C, 28);
    check_jump(owner);
}

void snobo_section_props_29(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xFB, 0xD1C, 29);
        GATE_A(owner, 0xFE, 0xD1C, 30);
    }
    check_jump(owner);
}

void snobo_section_props_31(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xF8, 0xD1C, 31);
        GATE_B(owner, 0xFF, 0x151C, 32);
        GATE_B(owner, 0xFC, 0x151C, 33);
        GATE_B(owner, 0xFC, 0xD1C, 34);
        GATE_B(owner, 0xFF, 0xD1C, 35);
    }
    check_jump(owner);
}

void snobo_section_props_36(int owner, int on)
{
    if (on) {
        GATE_A(owner, 0xFB, 0xD1C, 36);
        GATE_A(owner, 0xFE, 0xD1C, 37);
    }
    check_jump(owner);
}

void snobo_trigger_flags_9928(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DC940.x, D_800DC940.y, D_800DC940.z);
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DC94C.x, D_800DC94C.y, D_800DC94C.z);
}

void snobo_trigger_flags_9B94(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DC958.x, D_800DC958.y, D_800DC958.z);
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DC964.x, D_800DC964.y, D_800DC964.z);
}

void snobo_trigger_flags_9E00(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DC988.x, D_800DC988.y, D_800DC988.z);
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DC994.x, D_800DC994.y, D_800DC994.z);
}

void snobo_trigger_flags_A06C(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DC9B8.x, D_800DC9B8.y, D_800DC9B8.z);
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DC9C4.x, D_800DC9C4.y, D_800DC9C4.z);
}

void snobo_trigger_flags_A2D8(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DC9D0.x, D_800DC9D0.y, D_800DC9D0.z);
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DC9DC.x, D_800DC9DC.y, D_800DC9DC.z);
}

void snobo_trigger_flags_A544(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DC9E8.x, D_800DC9E8.y, D_800DC9E8.z);
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DC9F4.x, D_800DC9F4.y, D_800DC9F4.z);
}

void snobo_trigger_flags_A7B0(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DCA18.x, D_800DCA18.y, D_800DCA18.z);
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DCA24.x, D_800DCA24.y, D_800DCA24.z);
}

void snobo_trigger_flags_AA1C(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DCA30.x, D_800DCA30.y, D_800DCA30.z);
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DCA3C.x, D_800DCA3C.y, D_800DCA3C.z);
}

void snobo_trigger_flags_AC88(int owner, int first)
{
    if (!first)
        return;
    snobo_spawn_flyer((short)owner, 0x600, 2, &D_800DCA6C);
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DCA78.x, D_800DCA78.y, D_800DCA78.z);
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DCA84.x, D_800DCA84.y, D_800DCA84.z);
}

void snobo_trigger_flags_AF30(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DCAB4.x, D_800DCAB4.y, D_800DCAB4.z);
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DCAC0.x, D_800DCAC0.y, D_800DCAC0.z);
}

void snobo_trigger_flags_B19C(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBC48, 0x101, 0xE00, D_800DCAE4.x, D_800DCAE4.y, D_800DCAE4.z);
    spawn_at(owner, &D_800DBC48, 0x102, 0xE00, D_800DCAF0.x, D_800DCAF0.y, D_800DCAF0.z);
}

void snobo_trigger_late_obj(int owner, int first)
{
    short id;

    if (!first)
        return;
    if (D_801027A2 <= 0x50)
        return;
    id = owner; /* copied here, past the guards, as the game does */
    D_800DBCD8.arg[0] = 3;
    D_800DBCD8.shade = 0x2007CC0;
    spawn_at(id, &D_800DBCD8, 0xF0, 0x785, D_800DCAFC.x, D_800DCAFC.y - 0xA0000, D_800DCAFC.z);
}

void snobo_trigger_bumps3_B580(int owner, int first)
{
    if (!first)
        return;
    spawn_bump(owner, 0xEC, snobo_obj_hop, 0x12AB, D_800DCD38.x, D_800DCD38.y, D_800DCD38.z);
    spawn_bump(owner, 0xEC, snobo_obj_hop, 0x12AB, D_800DCD44.x, D_800DCD44.y, D_800DCD44.z);
    spawn_bump(owner, 0xEC, snobo_obj_hop, 0x12AB, D_800DCD50.x, D_800DCD50.y, D_800DCD50.z);
}

void snobo_trigger_bumps2_B964(int owner, int first)
{
    if (!first)
        return;
    spawn_bump(owner, 0xEC, snobo_obj_hop, 0x12AB, D_800DCD5C.x, D_800DCD5C.y, D_800DCD5C.z);
    spawn_bump(owner, 0xEC, snobo_obj_hop, 0x12AB, D_800DCD68.x, D_800DCD68.y, D_800DCD68.z);
}

void snobo_trigger_bumps3_BC00(int owner, int first)
{
    if (!first)
        return;
    spawn_bump(owner, 0xEC, snobo_obj_hop, 0x12AB, D_800DCD74.x, D_800DCD74.y, D_800DCD74.z);
    spawn_bump(owner, 0xEC, snobo_obj_hop, 0x12AB, D_800DCD80.x, D_800DCD80.y, D_800DCD80.z);
    spawn_bump(owner, 0xEC, snobo_obj_hop, 0x12AB, D_800DCD8C.x, D_800DCD8C.y, D_800DCD8C.z);
}

void snobo_trigger_bumps2_BFE4(int owner, int first)
{
    if (!first)
        return;
    spawn_bump(owner, 0xEC, snobo_obj_hop, 0x12AB, D_800DCD98.x, D_800DCD98.y, D_800DCD98.z);
    spawn_bump(owner, 0xEC, snobo_obj_hop, 0x12AB, D_800DCDA4.x, D_800DCDA4.y, D_800DCDA4.z);
}

void snobo_trigger_pair_C280(int owner, int first)
{
    if (!first)
        return;
    spawn_at(owner, &D_800DBAE0, 0xEC, 0x2AB, D_800DCDB0.x, D_800DCDB0.y, D_800DCDB0.z);
    spawn_at(owner, &D_800DBAE0, 0xEC, 0x2AB, D_800DCDBC.x, D_800DCDBC.y, D_800DCDBC.z);
}

void snobo_clear_triggers(void)
{
    int i;
    int *p = D_80100F34.word;

    for (i = 150; i != -1; i--)
        *p++ = 0;
    D_80101190 = 0;
}

void snobo_fire_trigger(id)
short id;
{
    int first = 0;

    if (!D_80100F34.seen[id]) {
        first = 1;
        D_80100F34.seen[id] = 1;
    }
    if (D_800DCDC8[id])
        D_800DCDC8[id](id, first);
}

void snobo_clear_objs(void)
{
    int i;
    Obj *o = D_800FFDB0;

    for (i = 31; i != -1; i--) {
        o->active = 0;
        o++;
    }
    D_80100F30 = 0;
}

void snobo_update_objs(Work *v)
{
    Obj *o;
    Queued *e;
    int i, y, d, r, low;
    int *b = (int *)v->sz;

    v->idx = D_8010281C;
    v->f02 = D_801028F4;
    v->sz[0] = D_800DB1D4.pos.vx >> 12;
    v->sz[1] = D_800DB1D4.pos.vy >> 12;
    v->sz[2] = D_800DB1D4.pos.vz >> 12;
    for (i = 0, o = D_800FFDB0; i < 32; i++, o++) {
        if (!o->active)
            continue;
        if (o->id < v->idx) {
            o->active = 0;
            D_80100F30 = i;
            continue;
        }
        if (v->f02 && o->t.update) {
            o->t.update(o);
            if (!o->active)
                continue;
        }
        if (o->t8A & 1)
            continue;
        if (!o->t.rot.pad) {
            RotMatrixYXZ(&o->t.rot, &v->lm);
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

        e = &D_80101194[D_80102954++];
        e->m = v->lm;
        e->how = 0;
        e->model = o->t.model;
        e->drawer = o->t.drawer;
        e->shade = o->t.shade;
        if (o->t.frame) {
            Queued *e2 = &D_80101194[D_80102954++];
            e2->how = 2;
            e2->model = o->t.frame;
            e2->drawer = o->t.drawer;
            e2->shade = o->t.shade;
        }

        if (!o->t.radius)
            continue;
        if (o->id != v->idx)
            continue;
        switch ((unsigned char)D_800DB1D4.state) {
        case 10:
        case 12:
        case 14:
            if (D_800DB1D4.state_time < 8)
                continue;
        }
        y = o->t.pos.y >> 12;
        d = y - o->t.top;
        if (b[1] < d)
            continue;
        d = y + 0x100;
        if (d < b[1])
            continue;
        r = o->t.radius;
        d = y - o->t.low;
        low = b[1] < d;
        y = b[0] - (o->t.pos.x >> 12);
        d = ABS(y);
        if (r < d)
            continue;
        y = b[2] - (o->t.pos.z >> 12);
        y = ABS(y);
        if (r < y)
            continue;
        if (r < SquareRoot0(d * d + y * y))
            continue;
        o->hit |= 1;
        if (o->t.react[0] == 0xFF)
            continue;
        snobo_restore_rider();
        if (D_800DB1D4.speed > 0x100) {
            o->hit |= 2;
            o->knock_x = D_800DB1D4.vx;
            o->knock_z = D_800DB1D4.vz;
            low += 2;
        }
        if (o->t.react[low]) {
            d = o->t.react[low];
            if (d == 0x80) {
                if ((unsigned char)D_800DB1D4.state == 11)
                    d = 12;
                else if (D_800DB1D4.speed > 0x280)
                    d = 8;
                else
                    d = 12;
            }
            if (d == 8) {
                if ((short)(D_801027C8 % 12) >= 6)
                    d = 9;
                D_800DB1D4.vx -= D_800DB1D4.vx >> 1;
                D_800DB1D4.vz -= D_800DB1D4.vz >> 1;
            }
            D_800DB1D4.state = d;
        } else {
            D_800DB1D4.vz = 0;
            D_800DB1D4.vx = 0;
        }
    }
}

void snobo_obj_wake_near_rider(Obj *o)
{
    int dx, dz;

    o->t8A = -(o->id - D_8010281C >= 3);
    dx = (D_800DB1D4.pos.vx - o->t.pos.x) >> 12;
    dz = (D_800DB1D4.pos.vz - o->t.pos.z) >> 12;
    if (SquareRoot0(dx * dx + dz * dz) < D_800DB1D4.speed + 0x400) {
        o->t.update = snobo_obj_fall_fade;
        if (o->t.arg[0] & 1)
            o->t.radius = 0x60;
        else
            o->t.radius = 0x40;
        o->t.top = 0x100;
        o->t.low = 0x100;
    }
}

void snobo_obj_drop_timer(Obj *o)
{
    if (o->id - D_8010281C >= 4)
        return;
    if (--o->t84 != -1) {
        o->t8A = -1;
        return;
    }
    o->t.radius = 0xA0;
    o->t8A = 0;
    o->vel.y = 0x8000;
    o->t88 = 0;
    o->t84 = o->t.arg[3];
    o->t.pos.x = o->t.arg[0];
    o->t.pos.y = ((rand() & 0xF) << 16) + o->t.arg[1];
    o->t.update = snobo_obj_bounce;
    o->t.pos.z = o->t.arg[2];
}

void snobo_ev_spawn_BCECC(int id, int on)
{
    if (!on)
        return;
    spawn_at(id, &D_800DBB28, 0xED, 0xADC, D_800DBFD0[1].x, D_800DBFD0[1].y, D_800DBFD0[1].z);
}

void snobo_ev_spawn_BD008(int id, int on)
{
    if (!on)
        return;
    spawn_at(id, &D_800DBAE0, 0xEA, 0x726, D_800DBFD0[16].x, D_800DBFD0[16].y, D_800DBFD0[16].z);
}

void snobo_ev_spawn_BD144(int id, int on)
{
    if (!on)
        return;
    spawn_at(id, &D_800DBB28, 0xED, 0xF1E, D_800DBFD0[19].x, D_800DBFD0[19].y, D_800DBFD0[19].z);
}

void snobo_ev_nop_BD280(int id, int on)
{
}

void snobo_ev_spawn_BD288(int id, int on)
{
    if (!on)
        return;
    spawn_at(id, &D_800DBB28, 0xEF, 0xB90, D_800DBFD0[20].x, D_800DBFD0[20].y, D_800DBFD0[20].z);
}

void snobo_ev_nop_BD3C4(int id, int on)
{
}

void snobo_ev_spawn_BD3CC(int id, int on)
{
    if (!on)
        return;
    spawn_at(id, &D_800DBAE0, 0xEA, 0xE00, D_800DBFD0[28].x, D_800DBFD0[28].y, D_800DBFD0[28].z);
}

void snobo_ev_nop_BD508(int id, int on)
{
}

void snobo_ev_nop_BD510(int id, int on)
{
}

void snobo_ev_BD518(int id, int on)
{
    if (on)
        snobo_place_e7_cluster(id);
}

void snobo_ev_spawn_BD53C(int id, int on)
{
    if (!on)
        return;
    spawn_at(id, &D_800DBAE0, 0xEC, 0xF55, D_800DC2AC.x, D_800DC2AC.y, D_800DC2AC.z);
}

void snobo_ev_spawn_BD678(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC2DC.x, D_800DC2DC.y, D_800DC2DC.z);
}

void snobo_ev_spawn_BD7A8(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_launched((short)id, 0, &D_800DC2F4);
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC2E8.x, D_800DC2E8.y, D_800DC2E8.z);
}

void snobo_ev_spawn_BD908(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC300.x, D_800DC300.y, D_800DC300.z);
    snobo_spawn_launched((short)id, 1, &D_800DC30C);
}

void snobo_ev_spawn_BDA64(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC318.x, D_800DC318.y, D_800DC318.z);
    snobo_spawn_launched((short)id, 0, &D_800DC324);
}

void snobo_ev_spawn_BDBC0(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC330.x, D_800DC330.y, D_800DC330.z);
    snobo_spawn_launched((short)id, 1, &D_800DC33C);
}

void snobo_ev_spawn_BDD1C(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC348.x, D_800DC348.y, D_800DC348.z);
    snobo_spawn_launched((short)id, 0, &D_800DC354);
}

void snobo_ev_spawn_BDE78(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC360.x, D_800DC360.y, D_800DC360.z);
    snobo_spawn_launched((short)id, 1, &D_800DC36C);
}

void snobo_ev_BDFD4(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_launched((short)id, 0, &D_800DC378);
}

void snobo_ev_spawn_BE008(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC384.x, D_800DC384.y, D_800DC384.z);
}

void snobo_ev_BE138(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_launched((short)id, 2, &D_800DC3D8);
}

void snobo_ev_BE16C(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_launched((short)id, 3, &D_800DC408[0]);
    snobo_spawn_launched((short)id, 0, &D_800DC408[1]);
}

void snobo_ev_spawn_BE1C8(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC420.x, D_800DC420.y, D_800DC420.z);
}

void snobo_ev_spawn_BE2F8(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_launched((short)id, 1, &D_800DC438);
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC42C.x, D_800DC42C.y, D_800DC42C.z);
}

void snobo_ev_spawn_BE458(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC450.x, D_800DC450.y, D_800DC450.z);
    snobo_spawn_launched((short)id, 0, &D_800DC444);
}

void snobo_ev_BE5B4(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_launched((short)id, 2, &D_800DC45C[0]);
    snobo_spawn_launched((short)id, 1, &D_800DC45C[1]);
}

void snobo_ev_spawn_BE610(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBC90, 0x238, D_800DC474.x, D_800DC474.y, D_800DC474.z);
}

void snobo_ev_spawn_BE740(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBB70, 0x98E, D_800DC5E4.x, D_800DC5E4.y, D_800DC5E4.z);
}

void snobo_ev_spawn_BE870(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBB70, 0x98E, D_800DC5F0.x, D_800DC5F0.y, D_800DC5F0.z);
}

void snobo_ev_spawn_BE9A0(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBB70, 0x98E, D_800DC618.x, D_800DC618.y, D_800DC618.z);
}

void snobo_ev_spawn_BEAD0(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBB70, 0x98E, D_800DC640.x, D_800DC640.y, D_800DC640.z);
}

void snobo_ev_spawn_BEC00(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBB70, 0x98E, D_800DC64C.x, D_800DC64C.y, D_800DC64C.z);
}

void snobo_ev_spawn_BED30(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBB70, 0x98E, D_800DC690.x, D_800DC690.y, D_800DC690.z);
}

void snobo_ev_spawn_BEE60(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBB70, 0x98E, D_800DC6EC.x, D_800DC6EC.y, D_800DC6EC.z);
}

void snobo_ev_spawn_BEF90(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBB70, 0x98E, D_800DC6F8.x, D_800DC6F8.y, D_800DC6F8.z);
}

void snobo_ev_spawn_BF0C0(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBB70, 0x98E, D_800DC704.x, D_800DC704.y, D_800DC704.z);
}

void snobo_ev_spawn_BF1F0(int id, int on)
{
    if (!on)
        return;
    spawn_turned(id, &D_800DBB70, 0x98E, D_800DC710.x, D_800DC710.y, D_800DC710.z);
}

void snobo_ev_BF320(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_flyer((short)id, 0x600, 0, &D_800DC970[0]);
    snobo_spawn_flyer((short)id, 0x600, 1, &D_800DC970[1]);
}

void snobo_ev_BF384(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_flyer((short)id, 0x600, 0, &D_800DC9A0[0]);
    snobo_spawn_flyer((short)id, 0x600, 1, &D_800DC9A0[1]);
}

void snobo_ev_BF3E8(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_flyer((short)id, 0x600, 3, &D_800DCA00);
}

void snobo_ev_BF420(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_flyer((short)id, 0x600, 2, &D_800DCA0C);
}

void snobo_ev_BF458(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_flyer((short)id, 0x600, 1, &D_800DCA48[0]);
    snobo_spawn_flyer((short)id, 0x600, 0, &D_800DCA48[1]);
}

void snobo_ev_BF4BC(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_flyer((short)id, 0x600, 3, &D_800DCA60);
}

void snobo_ev_BF4F4(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_flyer((short)id, 0x600, 3, &D_800DCA90);
}

void snobo_ev_BF52C(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_flyer((short)id, 0x600, 2, &D_800DCA9C);
}

void snobo_ev_BF564(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_flyer((short)id, 0x600, 1, &D_800DCAA8);
}

void snobo_ev_jump_check_3C(int id, int on)
{
    int i, speed;

    if (id != D_8010281C)
        return;
    i = (D_801027C8 - 0x3C) >> 1;
    if (i < 0 || (D_800DB1D4.state & 0xFF) == 11)
        return;
    if ((D_800DB1D4.ground_y - D_800DB1D4.pos.vy) >> 12 > D_800DCACC[i]) {
        D_800DB1D4.state = 11;
        speed = D_800DB1D4.vy;
        if (speed < 0)
            speed = -speed;
        if (speed < 0x40000)
            speed = 0x40000;
        D_800DB1D4.vy = speed;
        snobo_play_effect(2, 0x10C, 0);
    }
}

void snobo_ev_jump_check_30(int id, int on)
{
    int i, speed;

    if (id != D_8010281C)
        return;
    i = (D_801027C8 - 0x30) >> 1;
    if (i < 0 || (D_800DB1D4.state & 0xFF) == 11)
        return;
    if ((D_800DB1D4.ground_y - D_800DB1D4.pos.vy) >> 12 > D_800DCACC[i]) {
        D_800DB1D4.state = 11;
        speed = D_800DB1D4.vy;
        if (speed < 0)
            speed = -speed;
        if (speed < 0x40000)
            speed = 0x40000;
        D_800DB1D4.vy = speed;
        snobo_play_effect(2, 0x10C, 0);
    }
}

void snobo_ev_sound_BF744(int id, int on)
{
    if (on)
        snobo_play_effect(4, 0xCD, 0);
}

void snobo_ev_kinds_BF770(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0, 0, (VECTOR *)(D_800DCB08));
    snobo_spawn_kind((short)id, 0xA00, 2, (VECTOR *)(D_800DCB08 + 4));
    snobo_spawn_kind((short)id, 0x400, 3, (VECTOR *)(D_800DCB08 + 8));
}

void snobo_ev_kinds_BF7E8(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0x800, 0, (VECTOR *)(D_800DCB38));
    snobo_spawn_kind((short)id, 0x300, 2, (VECTOR *)(D_800DCB38 + 4));
}

void snobo_ev_kinds_BF84C(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0xC00, 2, (VECTOR *)(D_800DCB58));
    snobo_spawn_kind((short)id, 0, 1, (VECTOR *)(D_800DCB58 + 4));
    snobo_spawn_kind((short)id, 0x700, 2, (VECTOR *)(D_800DCB58 + 8));
}

void snobo_ev_kinds_BF8C4(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0x600, 2, (VECTOR *)(D_800DCB88));
    snobo_spawn_kind((short)id, 0xC00, 2, (VECTOR *)(D_800DCB88 + 4));
}

void snobo_ev_kinds_BF928(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0x600, 2, (VECTOR *)(D_800DCBA8));
    snobo_spawn_kind((short)id, 0xC00, 2, (VECTOR *)(D_800DCBA8 + 4));
}

void snobo_ev_kinds_BF98C(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0xE00, 0, (VECTOR *)(D_800DCBC8));
    snobo_spawn_kind((short)id, 0x700, 0, (VECTOR *)(D_800DCBC8 + 4));
    snobo_spawn_kind((short)id, 0x100, 0, (VECTOR *)(D_800DCBC8 + 6));
    snobo_spawn_kind((short)id, 0x300, 0, (VECTOR *)(D_800DCBC8 + 8));
}

void snobo_ev_kinds_BFA18(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0, 2, (VECTOR *)(D_800DCC08));
    snobo_spawn_kind((short)id, 0x800, 2, (VECTOR *)(D_800DCC08 + 4));
    snobo_spawn_kind((short)id, 0x400, 3, (VECTOR *)(D_800DCC08 + 8));
}

void snobo_ev_kinds_BFA90(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0, 0, (VECTOR *)(D_800DCC38));
    snobo_spawn_kind((short)id, 0x100, 2, (VECTOR *)(D_800DCC38 + 4));
}

void snobo_ev_kinds_BFAF4(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0, 0, (VECTOR *)(D_800DCC58));
    snobo_spawn_kind((short)id, 0x500, 2, (VECTOR *)(D_800DCC58 + 4));
}

void snobo_ev_kinds_BFB58(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0, 0, (VECTOR *)(D_800DCC78));
    snobo_spawn_kind((short)id, 0, 3, (VECTOR *)(D_800DCC78 + 4));
}

void snobo_ev_kinds_BFBBC(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0, 2, (VECTOR *)(D_800DCC98));
    snobo_spawn_kind((short)id, 0xC00, 0, (VECTOR *)(D_800DCC98 + 4));
}

void snobo_ev_kinds_BFC20(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0x300, 2, (VECTOR *)(D_800DCCB8));
    snobo_spawn_kind((short)id, 0xF00, 3, (VECTOR *)(D_800DCCB8 + 4));
}

void snobo_ev_kinds_BFC84(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0, 0, (VECTOR *)(D_800DCCD8));
    snobo_spawn_kind((short)id, 0x400, 0, (VECTOR *)(D_800DCCD8 + 4));
    snobo_spawn_kind((short)id, 0x800, 2, (VECTOR *)(D_800DCCD8 + 8));
    snobo_spawn_kind((short)id, 0xC00, 2, (VECTOR *)(D_800DCCD8 + 12));
}

void snobo_ev_kinds_BFD10(int id, int on)
{
    if (!on)
        return;
    snobo_spawn_kind((short)id, 0, 1, (VECTOR *)(D_800DCD18));
    snobo_spawn_kind((short)id, 0x200, 2, (VECTOR *)(D_800DCD18 + 4));
}
