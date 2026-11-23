/* MINI/HIGHWAY.BIN, the motorcycle chase: 0x800A00D0-0x800B41C8 (its data follows to 0x800B4F4C). GCC 2.6.3, ASPSX
 * 2.21 with its divide checks, -G0 (no -fforce-addr: it reloads globals by %hi/%lo every time). Its scene engine
 * (frames, models, primitive pools, the hand-written drawers after the code) is JET.BIN's. Still asm without C: the
 * seven hand-written GTE drawers at the end. */

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
int csqrt(int a);
int SquareRoot0(int a);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixZYX(SVECTOR *r, MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);
int VectorNormal(VECTOR *v0, VECTOR *v1);
void SetLightMatrix(MATRIX *m);
void SetFogNearFar(int a, int b, int h);
void SetGeomOffset(int x, int y);
void SetGeomScreen(int h);
void SetBackColor(int r, int g, int b);
void SetFarColor(int r, int g, int b);
void func_80043938(int mode); /* ResetGraph */
void func_80044244(unsigned int *ot, int n); /* ClearOTagR */
void func_80044A68(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw); /* SetDrawMode */
void func_8004433C(unsigned int *ot); /* DrawOTag */
void func_800444AC(DISPENV *env); /* PutDispEnv */
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* SetDefDrawEnv */
void func_80044000(RECT *r, void *p); /* LoadImage */
int func_80043CC0(void); /* GetGraphType */
unsigned int func_8001C808(int pad); /* the pad's buttons (the exe's takes none; the module passes 1) */
void PutDrawEnv(DRAWENV *env);
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
void SetDispMask(int mask);
void SetPolyF3(void *p);
void SetPolyF4(void *p);
void SetPolyG3(void *p);
void SetPolyFT3(void *p);
void SetPolyGT3(void *p);
void SetPolyGT4(void *p);

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
#define getClut(x, y) ((y) << 6 | ((x) >> 4 & 0x3F))

/* libgte's inline macros. The scene engine's drawers use gte.h's (the pointer copied to $12 first); the model code
 * after them has the ones that work straight from the pointer's register (_r, as SUBMAR.BIN's), and single register
 * moves and commands. */
#ifdef NATIVE
#define gte_SetRotMatrix_r(r) gte_SetRotMatrix(r)
#define gte_SetTransMatrix_r(r) gte_SetTransMatrix(r)
#define gte_ldv0_r(v) gte_ldv0(v)
#define gte_stlvnl_r(v) gte_stlvnl(v)
#define gte_mtc2(reg, v) gte_write(reg, (v))
#define gte_mfc2(reg) ((int)gte_read(reg))
#define gte_mfc2_nop(reg) ((int)gte_read(reg))
#define gte_lwc2_m(reg, x) gte_write(reg, (x))
#define gte_op(cmd) gte_cmd((cmd) & 0x1FFFFFF)
#define gte_op_now(cmd) gte_cmd((cmd) & 0x1FFFFFF)
#define gte_rtps() gte_cmd(0x180001)
#define gte_rtpt() gte_cmd(0x280030)
#define gte_ldv3c(v)                                                                                                   \
    (gte_write(0, ((unsigned int *)(v))[0]), gte_write(1, ((unsigned int *)(v))[1]),                                   \
     gte_write(2, ((unsigned int *)(v))[2]), gte_write(3, ((unsigned int *)(v))[3]),                                   \
     gte_write(4, ((unsigned int *)(v))[4]), gte_write(5, ((unsigned int *)(v))[5]))
#define gte_stsxy2(p) (*(unsigned int *)(p) = gte_read(14))
#define gte_stsxy3c(p)                                                                                                 \
    (((unsigned int *)(p))[0] = gte_read(12), ((unsigned int *)(p))[1] = gte_read(13),                                 \
     ((unsigned int *)(p))[2] = gte_read(14))
#define gte_stszotz(p) (*(int *)(p) = (int)gte_read(19) >> 2)
#define gte_stsv(v)                                                                                                    \
    (((short *)(v))[0] = gte_read(9), ((short *)(v))[1] = gte_read(10), ((short *)(v))[2] = gte_read(11))
#define gte_st3(p)                                                                                                     \
    (((int *)(p))[0] = gte_read(12), ((int *)(p))[2] = gte_read(13), ((int *)(p))[4] = gte_read(14),                   \
     ((int *)(p))[1] = gte_read(17), ((int *)(p))[3] = gte_read(18), ((int *)(p))[5] = gte_read(19))
#define gte_st1(p) (((int *)(p))[0] = gte_read(14), ((int *)(p))[1] = gte_read(19))
#define gte_ldmac(r, g, b) (gte_write(25, (r)), gte_write(26, (g)), gte_write(27, (b)))
#define gte_ldir_rgb(p)                                                                                                \
    (gte_write(9, ((unsigned char *)(p))[0]), gte_write(10, ((unsigned char *)(p))[1]),                                \
     gte_write(11, ((unsigned char *)(p))[2]))
#define gte_ldir0(v) gte_write(8, (v))
#define gte_strgb(reg, p) (*(unsigned int *)(p) = gte_read(reg))
#else
#define gte_SetRotMatrix_r(r)                                                                                          \
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
#define gte_SetTransMatrix_r(r)                                                                                        \
    __asm__ volatile("lw $12, 20(%0)\n"                                                                                \
                     "lw $13, 24(%0)\n"                                                                                \
                     "ctc2 $12, $5\n"                                                                                  \
                     "lw $14, 28(%0)\n"                                                                                \
                     "ctc2 $13, $6\n"                                                                                  \
                     "ctc2 $14, $7"                                                                                    \
                     :                                                                                                 \
                     : "r"(r)                                                                                          \
                     : "$12", "$13", "$14")
#define gte_ldv0_r(v) __asm__ volatile("lwc2 $0, 0(%0)\nlwc2 $1, 4(%0)" : : "r"(v))
#define gte_stlvnl_r(v) __asm__ volatile("swc2 $25, 0(%0)\nswc2 $26, 4(%0)\nswc2 $27, 8(%0)" : : "r"(v) : "memory")
#define gte_mtc2(reg, v) __asm__ volatile("mtc2 %z0, $" #reg : : "rJ"(v))
#define gte_mfc2(reg)                                                                                                  \
    ({                                                                                                                 \
        int _v;                                                                                                        \
        __asm__ volatile("mfc2 %0, $" #reg : "=r"(_v));                                                                \
        _v;                                                                                                            \
    })
/* With the load delay filled. */
#define gte_mfc2_nop(reg)                                                                                              \
    ({                                                                                                                 \
        int _v;                                                                                                        \
        __asm__ volatile("mfc2 %0, $" #reg "\nnop" : "=r"(_v));                                                        \
        _v;                                                                                                            \
    })
#define gte_lwc2_m(reg, x) __asm__ volatile("lwc2 $" #reg ", %0" : : "m"(x))
#define gte_op(cmd) __asm__ volatile("nop\nnop\n.word " #cmd)
#define gte_op_now(cmd) __asm__ volatile(".word " #cmd)
#define gte_rtps() __asm__ volatile("nop\nnop\n.word 0x4A180001")
#define gte_rtpt() __asm__ volatile("nop\nnop\n.word 0x4A280030")
#define gte_ldv3c(v)                                                                                                   \
    __asm__ volatile("lwc2 $0, 0(%0)\nlwc2 $1, 4(%0)\nlwc2 $2, 8(%0)\nlwc2 $3, 12(%0)\nlwc2 $4, 16(%0)\n"              \
                     "lwc2 $5, 20(%0)"                                                                                 \
                     :                                                                                                 \
                     : "r"(v))
#define gte_stsxy2(p) __asm__ volatile("move $12, %0\nswc2 $14, 0($12)" : : "r"(p) : "$12", "memory")
#define gte_stsxy3c(p) __asm__ volatile("swc2 $12, 0(%0)\nswc2 $13, 4(%0)\nswc2 $14, 8(%0)" : : "r"(p) : "memory")
#define gte_stszotz(p)                                                                                                 \
    __asm__ volatile("move $12, %0\nmfc2 $13, $19\nnop\nsra $13, $13, 2\nsw $13, 0($12)"                               \
                     :                                                                                                 \
                     : "r"(p)                                                                                          \
                     : "$12", "$13", "memory")
#define gte_stsv(v)                                                                                                    \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "mfc2 $13, $9\n"                                                                                  \
                     "mfc2 $14, $10\n"                                                                                 \
                     "mfc2 $15, $11\n"                                                                                 \
                     "sh $13, 0($12)\n"                                                                                \
                     "sh $14, 2($12)\n"                                                                                \
                     "sh $15, 4($12)"                                                                                  \
                     :                                                                                                 \
                     : "r"(v)                                                                                          \
                     : "$12", "$13", "$14", "$15", "memory")
/* Three projected points: screen positions and depths, interleaved. */
#define gte_st3(p)                                                                                                     \
    __asm__ volatile("swc2 $12, 0(%0)\nswc2 $13, 8(%0)\nswc2 $14, 16(%0)\nswc2 $17, 4(%0)\nswc2 $18, 12(%0)\n"         \
                     "swc2 $19, 20(%0)"                                                                                \
                     :                                                                                                 \
                     : "r"(p)                                                                                          \
                     : "memory")
#define gte_st1(p) __asm__ volatile("swc2 $14, 0(%0)\nswc2 $19, 4(%0)" : : "r"(p) : "memory")
#define gte_ldmac(r, g, b) __asm__ volatile("mtc2 %0, $25\nmtc2 %1, $26\nmtc2 %2, $27" : : "r"(r), "r"(g), "r"(b))
#define gte_ldir_rgb(p)                                                                                                \
    __asm__ volatile("lbu $12, 0(%0)\nlbu $13, 1(%0)\nlbu $14, 2(%0)\nmtc2 $12, $9\nmtc2 $13, $10\nmtc2 $14, $11"      \
                     :                                                                                                 \
                     : "r"(p)                                                                                          \
                     : "$12", "$13", "$14")
#define gte_ldir0(v) __asm__ volatile("mtc2 %0, $8" : : "r"(v))
#define gte_strgb(reg, p) __asm__ volatile("swc2 $" #reg ", 0(%0)" : : "r"(p) : "memory")
#endif
/* gpf with the 12-bit shift, gpf and gpl without. */
#define gte_gpf12() gte_op(0x4B98003D)
#define gte_gpf0() gte_op(0x4B90003D)
#define gte_gpl0() gte_op(0x4BA0003E)
/* IR1-3 from MAC1-3 shifted down 12, then turned by the rotation (rtir): one column of the next matrix. */
#define gte_mac12_rtir()                                                                                               \
    {                                                                                                                  \
        gte_mtc2(9, gte_mfc2(25) >> 12 & 0xFFFF);                                                                      \
        gte_mtc2(10, gte_mfc2(26) >> 12 & 0xFFFF);                                                                     \
        gte_mtc2(11, gte_mfc2(27) >> 12 & 0xFFFF);                                                                     \
        gte_rtir();                                                                                                    \
    }

/* The scene engine (JET.BIN's). */

/* A shaded triangle as the hand-written drawer reads it (0x24 bytes). */
typedef struct Face {
    SVECTOR v[3];
    unsigned int c[3];
} Face;

/* A textured face as highway_draw_ft3 reads it (0x28 bytes). */
typedef struct Strip {
    unsigned char data[40];
} Strip;

/* A model's counts and bounding box as the model file has them (0x18 bytes). */
typedef struct ModelBox {
    short faces, strips;
    SVECTOR min, max;
    short x14, pad16;
} ModelBox;

/* A model of the scenery: shaded faces and textured strips (0x20 bytes). */
typedef struct Model {
    short total, x02;
    short count; /* faces */
    short strip_count;
    short x08, pad0A;
    Face *faces;
    Strip *strips;
    short max_x, min_x, max_z, min_z;
    short x1C, pad1E;
} Model;

/* What the hand-written drawers read. */
typedef struct DrawArgs {
    void *faces;
    void *prims;
    unsigned int *ot;
    Model *model;
} DrawArgs;

/* A coordinate frame of the scene: a rotation and translation relative to its parent, kept on its level's list. */
typedef struct Frame {
    Model *model;
    MATRIX m;
    struct Frame *parent;
    short id, slot;
    short level; /* the root's is 0, each child's its parent's + 1 */
    short pad2E;
    struct Frame *next, *prev;
} Frame;

/* A frame's primitives: the next free one of each kind, then the primitives. The engine (JET.BIN's too) has room for
 * eight kinds; HIGHWAY.BIN gives F3, F4, GT3 and GT4 none. */
typedef struct PrimPool {
    POLY_F3 *f3;
    POLY_F4 *f4;
    POLY_G3 *g3;
    POLY_G4 *g4;
    POLY_FT3 *ft3;
    POLY_FT4 *ft4;
    POLY_GT3 *gt3;
    POLY_GT4 *gt4;
    POLY_F3 f3_buf[0]; /* the zero-length arrays (a GNU extension) are needed for the match */
    POLY_F4 f4_buf[0];
    POLY_G3 g3_buf[2500];
    POLY_G4 g4_buf[2];
    POLY_FT3 ft3_buf[800];
    POLY_FT4 ft4_buf[800];
    POLY_GT3 gt3_buf[0];
    POLY_GT4 gt4_buf[0];
} PrimPool;

/* One of the two display buffers, drawn and shown in turn. */
typedef struct DrawBuf {
    DRAWENV draw;
    DISPENV disp;
    unsigned int ot_hud[10]; /* drawn last; the gauges and numbers go in [3], the fades in [1] */
    unsigned int ot[0x1000]; /* the scene */
    unsigned char x4098[0xFA0];
    unsigned int ot_far[20]; /* drawn first: the sky and what is far */
    PrimPool pool;
} DrawBuf;

/* The camera's matrices: the view, its translation, its rotation, its roll and the view without the roll; the roll's
 * angles and a road segment's corners about its middle, which the road drawer projects. */
typedef struct Camera {
    MATRIX view, trans, rot, roll, unrolled;
    SVECTOR roll_angles;
    SVECTOR corner[4];
} Camera;

/* The road. */

/* A segment of the road, 80 in a ring (0x144 bytes). */
typedef struct Segment {
    VECTOR pos;
    SVECTOR rot;
    MATRIX m; /* its turn, and its place from the camera */
    int x38;
    int curve;
    int turn; /* 1 left, 2 right */
    short width;
    short tex[10]; /* [0] the road's texture, [3] the rails'; [9] set once the course has run out (not drawn) */
    short x5A;
    SVECTOR view[4]; /* its corners in the camera's view: the road's left and right, then the rails' tops */
    unsigned char x7C[0x74];
    Frame *frames[20]; /* of what stands on it, drawn with it */
    unsigned char nframes;
} Segment;

/* A piece of the course: how it turns (1 left, 2 right), its curve, its length in segments, the roll and pitch it
 * ends at, its width and which roadside pattern it uses. */
typedef struct RoadCmd {
    unsigned char turn, curve;
    short len;
    short roll;
    short pitch;
    short width;
    short pattern;
} RoadCmd;

/* The texture half of a POLY_FT4: its corners' u/v, clut and tpage. */
typedef struct TexQuad {
    unsigned char u0, v0;
    unsigned short clut;
    unsigned char u1, v1;
    unsigned short tpage;
    unsigned char u2, v2;
    unsigned short pad1;
    unsigned char u3, v3;
    unsigned short pad2;
} TexQuad;

/* A TexQuad as the road drawer copies it, word by word. */
typedef struct QuadUV {
    unsigned int uv0, uv1, uv2, uv3;
} QuadUV;

/* A POLY_FT4 as the road drawer fills it, word by word. */
typedef struct QuadW {
    unsigned int tag, rgbc;
    unsigned int xy0, uv0; /* uv0 with the CLUT */
    unsigned int xy1, uv1; /* uv1 with the texture page */
    unsigned int xy2, uv2;
    unsigned int xy3, uv3;
} QuadW;

/* A segment's edge on screen: its four corners' places and depths (a quarter of SZ). */
typedef struct SegProj {
    int sxy[4];
    int sz[4];
} SegProj;

/* The scratchpad as the road code uses it. */
typedef struct ScratchPad {
    unsigned char x00[0x30];
    VECTOR side;             /* a sideways offset from the road's middle */
    VECTOR turned0, turned1; /* it turned by two segments' matrices */
    VECTOR at0, at1;         /* the points it gives on them */
    unsigned char x80[0x88];
    SVECTOR sub[11]; /* 0x108 the points a near segment is cut at, in the camera's view */
    int sub_sxy[11]; /* 0x160 those on screen */
    SegProj near;    /* 0x18C the segment's edge nearer the camera */
    unsigned char x1AC[0x20];
    SegProj far; /* 0x1CC its farther edge */
} ScratchPad;

/* The riders and their models. */

/* A rider's bike: its state, place and driving (0x140 bytes, inside its Rider). */
typedef struct Bike {
    int state;       /* 5: a free slot, out of the race; 6 scripted; 2 dying; 1 hit */
    int type;        /* 0 Cloud, 1 the truck, 2 a soldier, 10 the soldier on the truck */
    int rider_model; /* the rider's figure, then the bike's: slots in D_800BC6B0 and D_800BD658 (0x13 none) */
    int bike_model;
    int nparts;  /* frames drawn with a model (frame 0 is the root) */
    int nframes; /* frames owned */
    VECTOR pos;  /* across and along the road, 24.8 fixed point */
    int x28;
    int z_max;
    int x30, x34;
    int x38; /* the start line's z */
    int x3C, x40, x44;
    int part[4]; /* each part's model, as an offset into D_800BD558 */
    int x58, x5C;
    int front, rear;     /* how far ahead of and behind its origin the bike reaches */
    int front_r, rear_r; /* the radii of its front and rear for collisions */
    int x70, x74, x78, x7C, x80, x84;
    int hp, x8C;
    int ai; /* how a soldier drives: 10, 11, 12 */
    int x94, x98, x9C, xA0;
    int vx, xA8, vz;
    int xB0, xB4, xB8; /* the bike's animation restarted at xB4, frame xB8, when xB0 is set */
    int xBC, xC0, xC4; /* the rider's, the same */
    int radius;
    /* Moving along a path of points: path_pos's high half is the segment, its low half how far along it. */
    int path_on;
    SVECTOR *path;
    int path_end, path_pos;
    int xDC, xE0, xE4, xE8, xEC, xF0, xF4, xF8;
    int heading;
    int x100, x104, x108, x10C, x110;
    int pushed;
    int x118, x11C;
    int x120, x124, x128, x12C, x130, x134, x138, x13C;
} Bike;

/* One of the six riders: 0 Cloud, 1 the truck, 2-4 soldiers' bikes, 5 the soldier on the truck (0x180 bytes). */
typedef struct Rider {
    int x00, x04, x08, x0C;
    short x10, x12, x14, x16;
    Frame *frames[10];
    Bike bike;
} Rider;

/* A jointed model from the model file (0x24 bytes): whether it is drawn, its bones (their lengths are the first ints
 * at base; their matrices go to the work buffer, 0x20 bytes each, the parts' primitives after them), its parts (0x20
 * bytes each, at base + parts) and its animations (0x10 bytes each, at base + anims); where it stands and how it
 * turns (bytes 1-3 of the word at 0x04). */
typedef struct Figure {
    unsigned char on;
    signed char x01; /* -1: no root matrix */
    unsigned char nbones, nparts;
    unsigned char nanims;
    unsigned char rx, ry, rz;
    int x, y, z;
    short x14;
    short scale;
    unsigned short parts;
    unsigned short anims;
    char *base;
    char *work;
} Figure;

/* The model file's header, loaded at 0x801A0000. */
typedef struct FigureFile {
    unsigned char count;
    char x01[3];
    Figure *figures;
} FigureFile;

/* A figure's part (0x20 bytes): whether it is shown, its bone, its vertices' count (they follow a flag word at base),
 * how many primitives of each kind, its tables' offsets from base, and the primitives it builds (two buffers of size
 * bytes each). */
typedef struct Part {
    unsigned char visible, bone, nverts, x03;
    unsigned char n_gt4, n_gt3, n_ft4, n_ft3, n_f3, n_f4, n_g3, n_g4;
    short x0C;
    unsigned short polys, uvs, texinfo, flags, size;
    char *base;
    char *prims;
} Part;

/* A figure's animation (0x10 bytes): nframes frames of `groups` groups of `count` shorts at base + ofs_a (each
 * frame's), and n_b shorts at base + ofs_b (fixed), after a flag word at base. */
typedef struct Anim {
    unsigned short nframes;
    char x02;
    unsigned char groups, n_b;
    char x05;
    unsigned short ofs_a, ofs_b;
    unsigned short x0A;
    char *base;
} Anim;

/* A figure's place and animation as the game runs it (0xB4 bytes). */
typedef struct FigureState {
    MATRIX m;        /* where it is */
    int frames[20];  /* each animation's frame count, less one */
    char x70[0x28];
    int x98, x9C;
    int anim, frame;
    int xA8;
    short flash; /* a white flash fading out by 0x80 a frame */
    short xAE;
    short nparts;
    char xB2[2];
} FigureState;

/* A thing of the scenery (debris, sparks, signs): where it is, how it moves, and the frame it is drawn with; 100 of
 * them, linked through a free list of slots (0xDC bytes). */
typedef struct Thing {
    VECTOR pos;
    char pad10[8];
    SVECTOR rot;
    char pad20[8];
    int type;
    int model;
    char pad30[4];
    int started, timer;
    char pad3C[0x14];
    int x50, x54, x58, x5C;
    char pad60[0x18];
    int rider;
    char pad7C[0x2C];
    int xA8;
    char padAC[0x28];
    Frame *frame;
    short slot, active;
} Thing;

typedef struct Short3 {
    short x, y, z;
} Short3;

/* A projected vertex in the scratchpad: its screen position and depth. */
typedef struct ProjVert {
    int sxy, sz;
} ProjVert;

/* The scratchpad as the pose code uses it: what to do, the two tables of the current animation (shorts each frame,
 * fixed shorts), and the root's and the bones' matrices. */
typedef struct PoseScratch {
    int flags; /* 1: translations from the animation; 2: place the root from the figure's position and angles */
    int pad04[2];
    short *frame_values;
    short *fixed_values;
    int pad14[3];
    MATRIX m[1]; /* the root, then each bone */
} PoseScratch;

/* A polygon's colours in a part's polygon list: a word, then RGB(pad) per corner. */
typedef struct PolyRGB {
    int x00;
    unsigned char c[4][4];
} PolyRGB;

/* The HUD. */

/* One of the five gauges' values (10 bytes). */
typedef struct Gauge {
    unsigned short max, cur;
    unsigned short damage; /* taken in all */
    unsigned short x06;
    unsigned char flash; /* a timer */
    unsigned char icon;  /* which of the icon's cells */
} Gauge;

/* Where a gauge sits on screen and its pieces in the HUD texture: the frame, the bar, the icon (one cell per state). */
typedef struct GaugeBox {
    unsigned short x, y;
    unsigned short bar_x, bar_y;
    unsigned char icon_u[4];
    unsigned char icon_v[4];
    unsigned char w, h;
    unsigned char icon_size, icon_x;
} GaugeBox;

/* A key of a prop channel's list: the frame table, how many times to play it, then where the prop stands (dx, dy),
 * its flags and its turn. */
typedef struct PropKey {
    short table, repeat, dx, dy, flags, drot;
} PropKey;

/* The chase script: at each time, an event code. */
typedef struct Event {
    int time, code;
} Event;

/* What each course (D_800BC4D4) is made of: the prop file and its ten channels' keys, the road's pieces and roadside
 * patterns, and the script. */
typedef struct Course {
    unsigned char *props;
    PropKey *keys[10];
    RoadCmd *road;
    unsigned char *patterns;
    Event *script;
} Course;

extern const SVECTOR D_800A0008; /* no turn */
extern unsigned char D_8009D288[]; /* the field's variables */
extern unsigned char D_8009D3FB, D_8009D4F3, D_8009D4F4, D_8009D4F5, D_8009D4F6;

extern VECTOR D_800B41C8;             /* where the camera looks */
extern VECTOR D_800B41D8, D_800B41E8; /* the eye and its target */
extern VECTOR D_800B41F8, D_800B4208; /* the scripted target and eye */
extern int D_800B4218, D_800B421C;    /* how far back the left and right rails sort */
extern SVECTOR D_800B4220;
extern VECTOR D_800B4228;
extern unsigned int D_800B4238, D_800B423C, D_800B4240, D_800B4244, D_800B4248, D_800B424C;
extern int D_800B4250;
extern ModelBox *D_800B4254;
extern Face *D_800B4258;
extern Strip *D_800B425C;
extern Course D_800B4260[2]; /* by D_800BC4D4 */
extern int *D_800B42D0, *D_800B42D4; /* the path file: counts, and offsets from D_800B42D8 */
extern int D_800B42D8;
extern unsigned int *D_800B42EC[0x34]; /* the overlay's TIMs */
extern unsigned char D_800B43BC[];     /* which of the parts' two primitive buffers is built this frame */
extern unsigned int *D_800B43C0;       /* the ordering table the figures go in */
extern unsigned int D_800B43C8[]; /* by angle 0-255: sine in the low half, cosine in the high (0x1000 is 1) */
extern Rider D_800B4F4C[6];
extern int D_800B6D4C;
extern Frame D_800B6D50[400]; /* the frames */
extern unsigned char D_800BC4D4; /* which course */
extern int D_800BC4D8;
extern unsigned char D_800BC4DC;
extern int D_800BC4E0[4]; /* the first model of each swapped-in animation */
extern unsigned char D_800BC670;
extern int D_800BC674, D_800BC6AC; /* the view's side planes' far-side signs */
extern int D_800BC678;
extern SVECTOR *D_800BC67C; /* the path looked up last, D_800BD550 its points */
extern int D_800BC680;
extern unsigned short D_800BC684[11], D_801163BC[11]; /* the road's bands' tpages and cluts */
extern FigureState D_800BC6B0[];
extern Frame *D_800BD4C0, *D_800BD4C4, *D_800BD4C8, *D_800BD4CC; /* the sky's frames */
/* The tint handed to highway_tint_figure: x, y, z as little-endian shorts, then whether to tint every primitive. */
extern unsigned char D_800BD4D0[7];
extern int D_800BD534;
extern unsigned short D_800BD538[8], D_801163FC[8]; /* the rails' tpages and cluts */
extern int D_800BD550;
extern unsigned char D_800BD554; /* how many frame tables the prop file has */
extern int D_800BD558; /* how far back the riders sort */
extern MATRIX D_800BD55C; /* no turn, no move: the cut points are in the camera's view already */
extern unsigned char *D_800BD57C; /* the prop file */
extern int D_800BD580; /* the pad's buttons */
extern int D_800BD584;
extern int D_800BD588, D_800BD58C; /* the nearest soldier and its distance */
extern unsigned short D_800BD590;
extern unsigned char D_800BD594;
extern VECTOR D_800BD598[10]; /* the lamps' lights */
extern int D_800BD638, D_800BD63C, D_800BD640;
extern short D_800BD644[10];
extern Figure *D_800BD658; /* the model file's figures */
extern ModelBox *D_800BD7EC;
extern unsigned short D_800BD7F0[3], D_80116690[3]; /* the HUD's tpages and cluts */
extern int D_800BD818;
extern Model *D_800BD81C[181]; /* the models, by id */
extern unsigned char *D_800BDB00[]; /* the prop file's frame tables */
extern Anim *D_800BDF00;
extern TexQuad D_800BDF04[8]; /* a rail, by texture */
extern int D_800BDFA4, D_800BDFA8; /* the view's side planes' distances */
extern SVECTOR *D_800BDFAC; /* the camera's path */
extern int D_800BDFB0; /* Cloud's steering, 1-9 */
extern unsigned int D_800BDFB4; /* the score */
extern unsigned char D_800BDFB8; /* soldiers sent */
extern int D_800BDFBC; /* faces used */
extern unsigned int D_800BDFC0; /* the camera path's end */
extern int D_800BDFC4; /* the attack button */
extern int D_800BDFC8; /* the camera's speed along its path */
extern short D_800BDFCC[400]; /* each free frame's next */
extern unsigned char D_800BE2EC; /* the countdown to the next soldier */
extern int D_800BE2F0;
extern DrawBuf *D_800BE2F4;
extern SVECTOR D_800BE2F8;
extern int D_800BE300; /* the script's next event */
extern int D_800BE304; /* models used */
extern Frame D_800BE308[10]; /* each level's list's tail */
extern unsigned int D_800BE538; /* the camera's place on its path */
extern int D_800BE53C;
extern VECTOR D_800BE540; /* the camera's offset from its path */
extern int *D_800BE550;
extern short D_800BE554;
extern Segment D_800BE558[80];
extern unsigned short D_800C4A98;
extern int D_800C4A9C;
extern short D_800C4C30, D_800C4C34;
extern PropKey *D_800C4C38;
extern Face *D_800C4C3C;
extern int D_800C4C40, D_800C4C48; /* the view's side planes' normals' lengths */
extern RoadCmd *D_800C4C44; /* the course piece being built */
extern DrawBuf D_800C4C4C[2];
extern Model D_8010D30C[]; /* the model pool */
extern int D_8010EA2C; /* how far the camera lifts */
extern void *D_8010EA30;
extern MATRIX *D_8010EA34; /* the work matrix (scratchpad) */
extern short D_8010EA38; /* the first free frame */
extern TexQuad D_8010EA3C[8], D_8010EAE4[8]; /* a near rail's two halves */
extern int D_8010EADC;
extern unsigned char D_8010EAE0; /* the camera rolls */
extern Event *D_8010EB84; /* the script */
extern int D_8010EB88; /* segments made */
extern int D_8010EB8C;
extern Frame D_8010EB90; /* the root */
extern unsigned char D_8010EBC8;
extern unsigned char D_8010EBCC;
extern int D_8010EBD0; /* strips used */
extern int D_8010EBD4;
extern short D_8010EBD8;
extern Camera D_8010EBDC;
extern short D_8010FBDC[10];
extern TexQuad D_8010FBF0[11]; /* the road far off, by texture */
extern unsigned char D_8010FD30[10];
extern unsigned char D_8010FD3C;
extern int D_8010FD40; /* the nearest rider's distance */
extern unsigned char D_8010FD44[10];
extern PropKey *D_8010FD50[10];
extern int D_8010FD78; /* the camera's heading */
extern unsigned short D_8010FD7C;
extern short D_8010FD80[10], D_8010FD94[10];
extern VECTOR D_8010FDA8; /* where the road has got to */
extern int D_8010FDB8;
extern short D_8010FDBC;
extern char *D_8010FDC0;
extern TexQuad D_8010FDC4[]; /* the road nearer, cut in two */
extern Gauge D_80110044[5];
extern TexQuad D_80110078[]; /* the road nearest, cut in eight */
extern unsigned char D_80110A8C; /* the camera's mode */
extern DrawBuf *D_80110A90; /* the one being drawn */
extern int D_80110A94;
extern VECTOR D_80110A98, D_80110AAC; /* the view's side planes' normals */
extern Strip *D_80110AA8;
extern int D_80110ABC;
extern short D_80110AC0[100]; /* each free thing's next */
extern unsigned char D_80110B88; /* the chase is over */
extern unsigned short D_80110B8C;
extern unsigned int D_80110B90; /* the best score */
extern int D_80110B94, D_80110B98;
extern short D_80110B9C[10];
extern int D_80110BB0;
extern unsigned char *D_80110BB4;
extern ScratchPad *D_80110BB8;
extern int D_80110BBC;
extern unsigned char *D_80110BC0;
extern int D_80110BC4, D_80110BC8, D_80110BD0, D_80110BD8;
extern unsigned char D_80110BCC, D_80110BD4;
extern unsigned char D_80110BDC[10];
extern int D_80110BE8;
extern Thing D_80110BEC; /* the template highway_spawn_thing_at fills in */
extern unsigned char D_80110CC8, D_80110CCC, D_80110CD0;
extern int D_80110CD4;
extern GaugeBox D_80110CD8[5];
extern Thing D_80110D3C[100];
extern SVECTOR *D_8011632C[11]; /* &D_80110BB8->sub[i] */
extern DR_MODE D_80116358;
extern short D_80116364; /* the first free thing */
extern unsigned short D_80116368;
extern int *D_8011636C[11]; /* &D_80110BB8->sub_sxy[i] */
extern unsigned char D_8011639C;
extern int D_801163A0; /* the segment being built, the ring's head */
extern int D_801163A4, D_801163A8;
extern unsigned char D_801163AC, D_801163B0, D_801163B4, D_801163B8; /* which of the sky's frames are drawn */
extern unsigned char *D_801163E4; /* the course's pattern data */
extern Camera *D_801163E8;
extern int D_801163EC, D_801163F4;
extern int *D_801163F8;
extern short D_80116410[10];
extern int D_80116424; /* how far the view reaches */
extern int D_80116428;
extern short D_8011642C[10];
extern int D_80116440;
extern Frame D_80116444[10]; /* each level's list's head */
extern unsigned short D_80116674;
extern void *D_80116678;
extern short D_8011667C;
extern int D_80116680;
extern short D_80116684; /* things in use */
extern FigureFile *D_80116688;
extern int D_8011668C;
extern int D_800B3BF8; /* a word in the code, after highway_tint_part */

void highway_main(void);
void highway_vsync_count(void);
void highway_draw_road(DrawBuf *b);
void highway_draw_frame(DrawBuf *b, Frame *f, int z, int unused);
void highway_draw_sky_strips(DrawBuf *b, Frame *f, short y);
void highway_draw_sky_faces(DrawBuf *b, Frame *f, short y);
MATRIX *highway_view_matrix(DrawBuf *b, Frame *f, MATRIX *m);
void highway_draw_score(DrawBuf *b);
void highway_init(void);
void highway_init_gfx(void);
void highway_clear_buffer(void);
void highway_init_prims(PrimPool *pool);
void highway_reset_prims(PrimPool *pool);
void highway_init_frames(void);
void highway_frame_init(Frame *f, short slot);
Frame *highway_frame_new(short id, int x1, int x2, int x3, Frame *parent, int tx, int ty, int tz, short rx, short ry,
                         short rz);
void highway_frame_set(Frame *f, SVECTOR *r, VECTOR *t);
Frame *highway_frame_new_yxz(short id, Frame *parent, VECTOR *t, SVECTOR *r);
void highway_frame_free(Frame *f);
short highway_alloc_frame_slot(void);
void highway_free_frame_slot(short slot);
void highway_frame_link(Frame *f, Frame *parent);
void highway_frame_unlink(Frame *f);
void highway_init_model_pools(void);
Model *highway_make_model(int id);
Model *highway_alloc_model(void);
Face *highway_alloc_faces(int n);
Strip *highway_alloc_strips(int n);
void highway_load_textures(void);
void highway_upload_tim(unsigned int *tim);
void highway_setup_road_quads(void);
void highway_split_quad_v(TexQuad *src, TexQuad *top, TexQuad *bottom);
void highway_split_quad_grid(TexQuad *src, int i);
void highway_split_quad_h(TexQuad *src, int i);
void highway_init_road(void);
void highway_build_segment(void);
void highway_free_segment_frames(void);
void highway_road_point(int t, int side, VECTOR *out);
void highway_road_point_rot(int t, int side, VECTOR *out, SVECTOR *rot);
void highway_road_point_rot_fine(int t, int dx, VECTOR *pos, SVECTOR *rot);
void highway_place_props(void);
void highway_init_riders(void);
void highway_update_riders(int z, DrawBuf *b);
void highway_clear_rider(int i);
void highway_init_fixed_riders(void);
void highway_init_soldier(int i);
void highway_rider_stub(void);
void highway_keep_all_on_road(void);
void highway_keep_on_road(int i);
void highway_spawn_soldiers(void);
void highway_spawn_soldier(int i);
void highway_remove_rider(int i);
void highway_move_riders(void);
void highway_move_rider(int i);
void highway_move_soldier(int i);
void highway_place_riders(void);
void highway_place_rider(int i);
void highway_animate_riders(void);
void highway_animate_rider(int i);
void highway_swap_rider_models(void);
void highway_swap_rider_model(int i);
int highway_rider_angle(Rider *a, Rider *b);
int highway_rider_distance(Rider *a, Rider *b);
void highway_bike_ends(short n, SVECTOR *front, SVECTOR *rear, short *len);
void highway_collide_riders(void);
void highway_bump(short i, short j, int angle);
void highway_hit_rider(int n, int damage);
void highway_truck_hit(int unused, int angle);
void highway_start_rider_path(int n, int path);
void highway_drive_soldier(int n);
void highway_init_props(void);
void highway_step_prop(unsigned char ch, unsigned char *frame, short *dx, short *dy, unsigned short *flags,
                       short *drot);
void highway_prop_frame(unsigned char table, unsigned char frame, unsigned char *a, unsigned char *b, unsigned char *c);
void highway_init_camera(void);
void highway_update_camera(int z_base);
void highway_camera_stub(void);
void highway_start_camera_path(int path, int mode, int speed);
void highway_find_path(unsigned char i);
void highway_path_point(unsigned int pos, SVECTOR *path, VECTOR *out, unsigned char mirror);
void highway_clear_input(void);
void highway_read_input(void);
void highway_init_view_planes(void);
int highway_in_view(int *v);
int highway_in_view_sv(SVECTOR *v);
int highway_plane1_dist(int x, int y, int z);
int highway_plane2_dist(int x, int y, int z);
int highway_near_view(int *v, short margin);
int highway_near_plane1(int x, int y, int z, short near);
int highway_near_plane2(int x, int y, int z, short near);
void highway_init_matrices(void);
void highway_make_sky(void);
void highway_draw_sky(DrawBuf *b);
void highway_init_state(void);
void highway_load_files(void);
void highway_stop_sound(void);
void highway_start_sound(void);
void highway_play_sound(int a0, int channel, int v);
void highway_sound_b0(int arg, int which);
void highway_set_volume(int arg, int which);
void highway_set_pan(int arg, int which);
void highway_advance_road(void);
void highway_run_script(void);
unsigned char highway_fade(DrawBuf *buf, unsigned char state);
void highway_damage_gauge(unsigned char idx, short amount);
void highway_draw_gauges(DrawBuf *buf);
void highway_draw_gauge(DrawBuf *buf, int i, int flash, int state);
void highway_draw_number(DrawBuf *buf, int value, short x, short y, short pad);
void highway_draw_small_number(DrawBuf *buf, int value, short x, short y, short pad);
void highway_draw_sprite(DrawBuf *buf, short tex, short x, short y, short w, short h, unsigned char u, unsigned char v,
                         unsigned char uw, unsigned char vh, unsigned char semi);
void highway_init_figures(void);
void highway_update_figures(void);
void highway_init_things(void);
short highway_spawn_thing(Thing *src, short parent);
void highway_free_thing(Thing *t);
short highway_alloc_thing_slot(void);
void highway_free_thing_slot(short slot);
void highway_update_things(DrawBuf *b);
Thing *highway_spawn_thing_at(short x, short y, short z, short type, short model);
char *highway_setup_figure_prims(Figure *m, char *work, int a2);
char *highway_setup_part_prims(Part *p, char *prims, int relocate, int variant);
void highway_draw_figure(Figure *m);
void highway_project_part(Part *p);
void highway_pose_figure(Figure *m, MATRIX *root, int anim, int frame);
void highway_scale_figure(Figure *m, short scale, int force);
void highway_scale_part(Part *p, short scale, int force);
void highway_scale_anim(Anim *q, short scale, int force);
int highway_tint_figure(Figure *m, unsigned char *b);
void highway_tint_part(Part *p, short x, short y, short z);
void highway_project3(SVECTOR *v, int *xy);
POLY_G3 *highway_draw_g3_list_cued(DrawArgs *args);
POLY_G3 *highway_draw_g3_list(DrawArgs *args);
POLY_FT3 *highway_draw_ft3(Strip *src, POLY_FT3 *prim, unsigned int *ot);
POLY_FT3 *highway_draw_ft3_z(Strip *src, POLY_FT3 *prim, unsigned int *ot);

/* m = f's matrix, times its parent's when it has one. */
#define HW_VIEW_MATRIX(m, f)                                                                                           \
    {                                                                                                                  \
        (m)->m[0][0] = (f)->m.m[0][0];                                                                                 \
        (m)->m[0][1] = (f)->m.m[0][1];                                                                                 \
        (m)->m[0][2] = (f)->m.m[0][2];                                                                                 \
        (m)->m[1][0] = (f)->m.m[1][0];                                                                                 \
        (m)->m[1][1] = (f)->m.m[1][1];                                                                                 \
        (m)->m[1][2] = (f)->m.m[1][2];                                                                                 \
        (m)->m[2][0] = (f)->m.m[2][0];                                                                                 \
        (m)->m[2][1] = (f)->m.m[2][1];                                                                                 \
        (m)->m[2][2] = (f)->m.m[2][2];                                                                                 \
        (m)->t[0] = (f)->m.t[0];                                                                                       \
        (m)->t[1] = (f)->m.t[1];                                                                                       \
        (m)->t[2] = (f)->m.t[2];                                                                                       \
        if ((f)->parent != &D_8010EB90)                                                                                \
            CompMatrix(&(f)->parent->m, (m), (m));                                                                     \
    }

/* Puts the segment's corners on screen and in the camera's view; the GTE has its matrix. */
#define HW_PROJECT_SEG(s, proj)                                                                                        \
    {                                                                                                                  \
        D_801163E8->corner[0].vx = -((s)->width >> 1);                                                                 \
        D_801163E8->corner[1].vx = (s)->width >> 1;                                                                    \
        D_801163E8->corner[2].vx = -((s)->width >> 1) - 30;                                                            \
        D_801163E8->corner[3].vx = ((s)->width >> 1) + 30;                                                             \
        for (j = 0; j < 4; j++) {                                                                                      \
            gte_ldv0(&D_801163E8->corner[j]);                                                                          \
            gte_rtps();                                                                                                \
            gte_stsxy2(&(proj)->sxy[j]);                                                                               \
            gte_stszotz(&(proj)->sz[j]);                                                                               \
            gte_stsv(&(s)->view[j]);                                                                                   \
        }                                                                                                              \
    }

/* The segment's matrix in the camera's view, loaded into the GTE. */
#define HW_SEG_MATRIX(s)                                                                                               \
    {                                                                                                                  \
        (s)->m.t[0] = (s)->pos.vx - D_800B41C8.vx;                                                                     \
        (s)->m.t[1] = (s)->pos.vy - D_800B41C8.vy;                                                                     \
        (s)->m.t[2] = (s)->pos.vz - D_800B41C8.vz;                                                                     \
        gte_CompMatrix(&D_801163E8->view, &(s)->m, D_8010EA34);                                                        \
        gte_SetRotMatrix(D_8010EA34);                                                                                  \
        gte_SetTransMatrix(D_8010EA34);                                                                                \
    }

#define HW_MID(d, a, b)                                                                                                \
    ((d)->vx = ((a)->vx + (b)->vx) >> 1, (d)->vy = ((a)->vy + (b)->vy) >> 1, (d)->vz = ((a)->vz + (b)->vz) >> 1)

/* Puts the first n cut points on screen. */
#define HW_PROJECT_SUB(n)                                                                                              \
    for (j = 0; j < (n); j++) {                                                                                        \
        gte_ldv0(&D_80110BB8->sub[j]);                                                                                 \
        gte_rtps();                                                                                                    \
        gte_stsxy2(&D_80110BB8->sub_sxy[j]);                                                                           \
    }

#define HW_QUAD(t, a, b, c, d, ot)                                                                                     \
    {                                                                                                                  \
        q->rgbc = fog;                                                                                                 \
        q->uv0 = ((QuadUV *)(t))->uv0;                                                                                 \
        q->uv1 = ((QuadUV *)(t))->uv1;                                                                                 \
        q->uv2 = ((QuadUV *)(t))->uv2;                                                                                 \
        q->uv3 = ((QuadUV *)(t))->uv3;                                                                                 \
        q->xy0 = (a);                                                                                                  \
        q->xy1 = (b);                                                                                                  \
        q->xy2 = (c);                                                                                                  \
        q->xy3 = (d);                                                                                                  \
        addPrim(ot, q);                                                                                                \
        q++;                                                                                                           \
    }

#define HW_MAX4(a, b, c, d)                                                                                            \
    {                                                                                                                  \
        z = (a);                                                                                                       \
        if (z < (b))                                                                                                   \
            z = (b);                                                                                                   \
        if (z < (c))                                                                                                   \
            z = (c);                                                                                                   \
        if (z < (d))                                                                                                   \
            z = (d);                                                                                                   \
    }

#define SET_QUAD(q, left, right, top, bottom, tp, cl)                                                                  \
    ((q).u0 = (left), (q).v0 = (top), (q).u1 = (right), (q).v1 = (top), (q).u2 = (left), (q).v2 = (bottom),            \
     (q).u3 = (right), (q).v3 = (bottom), (q).tpage = (tp), (q).clut = (cl))

#define BAND(i, top, bottom)                                                                                           \
    (SET_QUAD(D_8010FBF0[i], 0, 0xFF, top, bottom, D_800BC684[i], D_801163BC[i]),                                      \
     highway_split_quad_grid(&D_8010FBF0[i], i), highway_split_quad_h(&D_8010FBF0[i], i))

/* Splits a quad's texture into a 4 x 2 grid of pieces, kept at 8i..8i+7: each piece's corners are its rectangle's
 * left/right u and top/bottom v. */
#define PIECE(left, right, top, bottom)                                                                                \
    (D_80110078[n].u0 = (left), D_80110078[n].v0 = (top), D_80110078[n].u1 = (right), D_80110078[n].v1 = (top),        \
     D_80110078[n].u2 = (left), D_80110078[n].v2 = (bottom), D_80110078[n].u3 = (right),                               \
     D_80110078[n].v3 = (bottom), D_80110078[n].clut = src->clut, D_80110078[n].tpage = src->tpage)

/* If end p (of racer i, radius rp) touches end q (of racer j, radius rq), the angle to push them apart along. */
#define HIT(p, q, rp, rq)                                                                                              \
    reach = a->rp + b->rq + a->radius + b->radius;                                                                     \
    if (SquareRoot0((q.vx - p.vx) * (q.vx - p.vx) + (q.vy - p.vy) * (q.vy - p.vy) + (q.vz - p.vz) * (q.vz - p.vz)) <   \
        reach) {                                                                                                       \
        angle = ratan2(q.vz - p.vz, q.vx - p.vx);                                                                      \
        turn = rot.vy - 0x800;                                                                                         \
        angle += turn;                                                                                                 \
    }

/* Steer by heading at `speed`, with drag. */
#define BIKE_MOVE(b, speed)                                                                                            \
    b->vx += rcos(b->heading) * (speed) >> 12;                                                                         \
    b->vz += rsin(b->heading) * (speed) >> 11;                                                                         \
    b->vx = b->vx * 8 / 10;                                                                                            \
    b->vz = b->vz * 8 / 10;                                                                                            \
    b->pos.vx += b->vx;                                                                                                \
    b->pos.vz += b->vz;

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* One list of quads / of triangles: the screen positions in, then linked in the ordering table by depth if it faces
 * the camera, unlinked if not. */
#define QUADS(T, step)                                                                                                 \
    for (i = 0; i < count; prim += sizeof(T)) {                                                                        \
        T *poly = (T *)prim;                                                                                           \
                                                                                                                       \
        w = *src;                                                                                                      \
        a = &pv[w & 0xFF];                                                                                             \
        b = &pv[(w & 0xFF00) >> 8];                                                                                    \
        c = &pv[w >> 16 & 0xFF];                                                                                       \
        d = &pv[w >> 24];                                                                                              \
        xa = a->sxy;                                                                                                   \
        xb = b->sxy;                                                                                                   \
        xc = c->sxy;                                                                                                   \
        gte_mtc2(12, xa);                                                                                              \
        gte_mtc2(14, xc);                                                                                              \
        gte_mtc2(13, xb);                                                                                              \
        i++;                                                                                                           \
        src += step;                                                                                                   \
        gte_op_now(0x4B400006);                                                                                        \
        tag = *(unsigned int *)poly;                                                                                   \
        *(int *)&poly->x0 = xa;                                                                                        \
        *(int *)&poly->x1 = xb;                                                                                        \
        *(int *)&poly->x2 = xc;                                                                                        \
        if (gte_mfc2_nop(24) <= 0) {                                                                                   \
            *(unsigned int *)poly = tag & hi;                                                                          \
        } else {                                                                                                       \
            *(int *)&poly->x3 = d->sxy;                                                                                \
            e = &ot[(a->sz + b->sz + c->sz + d->sz) / 16];                                                             \
            link = *e;                                                                                                 \
            *(unsigned int *)poly = (tag & hi) | (link & lo);                                                          \
            *e = (link & hi) | ((unsigned int)poly & lo);                                                              \
        }                                                                                                              \
    }

#define TRIS(T, step)                                                                                                  \
    for (i = 0; i < count; prim += sizeof(T)) {                                                                        \
        T *poly = (T *)prim;                                                                                           \
                                                                                                                       \
        w = *src;                                                                                                      \
        a = &pv[w & 0xFF];                                                                                             \
        b = &pv[(w & 0xFF00) >> 8];                                                                                    \
        c = &pv[w >> 16 & 0xFF];                                                                                       \
        xa = a->sxy;                                                                                                   \
        xb = b->sxy;                                                                                                   \
        xc = c->sxy;                                                                                                   \
        gte_mtc2(12, xa);                                                                                              \
        gte_mtc2(14, xc);                                                                                              \
        gte_mtc2(13, xb);                                                                                              \
        i++;                                                                                                           \
        src += step;                                                                                                   \
        gte_op_now(0x4B400006);                                                                                        \
        tag = *(unsigned int *)poly;                                                                                   \
        *(int *)&poly->x0 = xa;                                                                                        \
        *(int *)&poly->x1 = xb;                                                                                        \
        *(int *)&poly->x2 = xc;                                                                                        \
        if (gte_mfc2_nop(24) <= 0) {                                                                                   \
            *(unsigned int *)poly = tag & hi;                                                                          \
        } else {                                                                                                       \
            gte_lwc2_m(17, a->sz);                                                                                     \
            gte_lwc2_m(18, b->sz);                                                                                     \
            gte_lwc2_m(19, c->sz);                                                                                     \
            gte_op_now(0x4B58002D);                                                                                    \
            e = &ot[gte_mfc2_nop(7)];                                                                                  \
            link = *e;                                                                                                 \
            *(unsigned int *)poly = (tag & hi) | (link & lo);                                                          \
            *e = (link & hi) | ((unsigned int)poly & lo);                                                              \
        }                                                                                                              \
    }

/* One corner: colour c plus the tint, into the FIFO. */
#define TINT(c)                                                                                                        \
    {                                                                                                                  \
        gte_ldmac(r, g, b);                                                                                            \
        gte_ldir_rgb(c);                                                                                               \
        gte_gpl0();                                                                                                    \
    }

/* end of declarations */

void highway_main(void)
{
    int i = 0x174;

    D_800BE2F0 = 1;
    D_800BDFB4 = 0;
    D_80110B90 = 0;
    D_800BC4D4 = D_8009D3FB;
    D_80110B90 = D_8009D4F6 << 8;
    D_80110B90 += D_8009D4F5;
    highway_load_files();
    highway_init_gfx();
    highway_init();
    highway_start_sound();
    highway_play_sound(0xB3, 4, 0);
    highway_play_sound(0x12B, 3, 0);
    while (!D_80110B88) {
        RECT unused[3]; /* only widens the stack frame */

        D_800B43C0 = &D_80110A90->ot[D_800BD558 + 75];
        highway_read_input();
        highway_run_script();
        highway_advance_road();
        highway_update_riders(D_8011668C, D_80110A90);
        if (D_800BC4DC)
            D_800BC4DC = highway_fade(D_80110A90, D_800BC4DC);
        highway_draw_road(D_80110A90);
        highway_draw_sky(D_80110A90);
        highway_update_things(D_80110A90);
        if (D_800BC4D4 == 1)
            highway_draw_score(D_80110A90);
        else
            highway_draw_gauges(D_80110A90);
        highway_update_camera(D_8011668C);
        DrawSync(0);
        VSync(2);
        func_80043938(1);
        PutDrawEnv(&D_80110A90->draw);
        func_800444AC(&D_80110A90->disp);
        ClearImage(&D_80110A90->draw.clip, 0, 0, 0);
        func_8004433C(&D_80110A90->ot_far[19]);
        func_8004433C(&D_80110A90->ot[0xFFF]);
        func_8004433C(&D_80110A90->ot_hud[9]);
        D_800B43BC[0] ^= 1;
        D_80110A90 = D_80110A90 == &D_800C4C4C[0] ? &D_800C4C4C[1] : &D_800C4C4C[0];
        func_80044244(D_80110A90->ot, 0x1000);
        func_80044244(D_80110A90->ot_far, 20);
        func_80044244(D_80110A90->ot_hud, 10);
        highway_reset_prims(&D_80110A90->pool);
    }
    if (D_800BC4D4 == 1) {
        if (D_800BDFB4 > D_80110B90)
            D_80110B90 = D_800BDFB4;
        D_8009D4F4 = D_800BDFB4 >> 8;
        D_8009D4F6 = ((unsigned char *)&D_80110B90)[1];
        D_8009D4F5 = ((unsigned char *)&D_80110B90)[0];
        D_8009D4F3 = ((unsigned char *)&D_800BDFB4)[0];
    } else {
        D_8009D288[i++] = D_80110044[0].cur;
        D_8009D288[i++] = D_80110044[0].cur >> 8;
        D_8009D288[i++] = D_80110044[1].cur;
        D_8009D288[i++] = D_80110044[1].cur >> 8;
        D_8009D288[i++] = D_80110044[3].cur;
        D_8009D288[i++] = D_80110044[3].cur >> 8;
        D_8009D288[i++] = D_80110044[2].cur;
        D_8009D288[i++] = D_80110044[2].cur >> 8;
        D_8009D288[i++] = D_80110044[4].cur;
        D_8009D288[i] = D_80110044[4].cur >> 8;
    }
}

void highway_vsync_count(void)
{
    D_800B3BF8 = VSync(1);
}

#ifdef NON_MATCHING
/* Register allocation and block layout: the game reloads and spills more (about 270 instructions
 * longer), keeps two pointers into each primitive and shares one addPrim tail. */
void highway_draw_road(DrawBuf *b)
{
    SegProj *near, *far;
    QuadW *q;
    Segment *s;
    SVECTOR *p, *c;
    TexQuad *road, *rail, *rail_a, *rail_b, *sub;
    unsigned int *ot;
    SVECTOR **v;
    int **sxy;
    unsigned int fog;
    int i, j, k, n, z, depth, last, vis_l, vis_r;

    s = &D_800BE558[D_8010EB88 % 80];
    q = (QuadW *)b->pool.ft4;
    last = 0;
    HW_SEG_MATRIX(s);
    near = &D_80110BB8->near;
    HW_PROJECT_SEG(s, near);
    for (i = 1; i < 80; i++) {
        k = (D_8010EB88 + i) % 80;
        s = &D_800BE558[k];
        if (s->tex[9])
            continue;
        HW_SEG_MATRIX(s);
        far = &D_80110BB8->far;
        HW_PROJECT_SEG(s, far);
        fog = 0x80 - (far->sz[0] >> 5);
        fog = fog + (fog << 8) + (fog << 16) + 0x2C000000;
        for (n = 0; n < s->nframes; n++)
            highway_draw_frame(b, s->frames[n], 200, 0);
        vis_l = highway_in_view_sv(&s->view[2]);
        vis_r = highway_in_view_sv(&s->view[3]);
        if (SquareRoot0(s->view[0].vx * s->view[0].vx + s->view[0].vy * s->view[0].vy +
                        s->view[0].vz * s->view[0].vz) < 1000)
            vis_r = 1;
        /* the segment after the last one in view is still drawn, so that the road doesn't end too soon */
        if (vis_l | vis_r | (i - 1 == last)) {
            road = &D_8010FBF0[s->tex[0]];
            rail = &D_800BDF04[s->tex[3]];
            rail_a = &D_8010EA3C[s->tex[3]];
            rail_b = &D_8010EAE4[s->tex[3]];
            depth = (far->sz[0] + far->sz[1]) >> 1;
            if (depth > 1000) {
                HW_MAX4(near->sz[0], near->sz[1], far->sz[0], far->sz[1]);
                if (z < 4011)
                    HW_QUAD(road, near->sxy[0], near->sxy[1], far->sxy[0], far->sxy[1], &b->ot[z + 85]);
                HW_MAX4(near->sz[2], near->sz[0], far->sz[2], far->sz[0]);
                if (z < 0x1000 - D_800B4218)
                    HW_QUAD(rail, near->sxy[2], far->sxy[2], near->sxy[0], far->sxy[0], &b->ot[z + D_800B4218]);
                HW_MAX4(near->sz[3], near->sz[1], far->sz[3], far->sz[1]);
                if (z < 0x1000 - D_800B421C)
                    HW_QUAD(rail, near->sxy[3], far->sxy[3], near->sxy[1], far->sxy[1], &b->ot[z + D_800B421C]);
            } else if (depth < 300) {
                /* near: the road cut in eight, each rail in two, from the previous segment's corners and this one's;
                 * what is about to pass the camera is left out */
                gte_SetRotMatrix(&D_800BD55C);
                gte_SetTransMatrix(&D_800BD55C);
                k = k - 1 < 0 ? k + 79 : k - 1;
                HW_MAX4(near->sz[0], near->sz[1], far->sz[0], far->sz[1]);
                ot = &b->ot[z + 170];
                p = D_800BE558[k].view;
                c = s->view;
                v = D_8011632C;
                sxy = D_8011636C;
                if (z >= 41) {
                    HW_MID(v[0], &p[1], &p[0]);
                    HW_MID(v[1], &p[0], &c[0]);
                    HW_MID(v[2], &p[1], &c[1]);
                    HW_MID(v[3], v[1], v[2]);
                    HW_MID(v[4], v[0], &p[0]);
                    HW_MID(v[5], v[0], &p[1]);
                    HW_MID(v[6], v[1], v[3]);
                    HW_MID(v[7], v[2], v[3]);
                    HW_MID(v[8], &c[1], &c[0]);
                    HW_MID(v[9], &c[1], v[8]);
                    HW_MID(v[10], &c[0], v[8]);
                    HW_PROJECT_SUB(11);
                    sub = &D_80110078[s->tex[0] * 8];
                    HW_QUAD(&sub[0], near->sxy[0], *sxy[4], *sxy[1], *sxy[6], ot);
                    HW_QUAD(&sub[1], *sxy[4], *sxy[0], *sxy[6], *sxy[3], ot);
                    HW_QUAD(&sub[2], *sxy[0], *sxy[5], *sxy[3], *sxy[7], ot);
                    HW_QUAD(&sub[3], *sxy[5], near->sxy[1], *sxy[7], *sxy[2], ot);
                    HW_QUAD(&sub[4], *sxy[1], *sxy[6], far->sxy[0], *sxy[10], ot);
                    HW_QUAD(&sub[5], *sxy[6], *sxy[3], *sxy[10], *sxy[8], ot);
                    HW_QUAD(&sub[6], *sxy[3], *sxy[7], *sxy[8], *sxy[9], ot);
                    HW_QUAD(&sub[7], *sxy[7], *sxy[2], *sxy[9], far->sxy[1], ot);
                }
                HW_MAX4(near->sz[2], near->sz[0], far->sz[2], far->sz[0]);
                if (z >= 71) {
                    HW_MID(v[0], &p[0], &p[2]);
                    HW_MID(v[1], &c[0], &c[2]);
                    HW_PROJECT_SUB(2);
                    ot = &b->ot[z + D_800B4218];
                    HW_QUAD(rail_a, near->sxy[2], far->sxy[2], *sxy[0], *sxy[1], ot);
                    HW_QUAD(rail_b, *sxy[0], *sxy[1], near->sxy[0], far->sxy[0], ot);
                }
                /* the game tests corners 0 and 2 here but takes 1 and 3 */
                z = near->sz[3];
                if (z < near->sz[0])
                    z = near->sz[1];
                if (z < far->sz[2])
                    z = far->sz[3];
                if (z < far->sz[0])
                    z = far->sz[1];
                if (z >= 71) {
                    HW_MID(v[0], &p[1], &p[3]);
                    HW_MID(v[1], &c[1], &c[3]);
                    HW_PROJECT_SUB(2);
                    ot = &b->ot[z + D_800B421C];
                    HW_QUAD(rail_a, near->sxy[3], far->sxy[3], *sxy[0], *sxy[1], ot);
                    HW_QUAD(rail_b, *sxy[0], *sxy[1], near->sxy[1], far->sxy[1], ot);
                }
            } else {
                /* nearer: the road cut in two */
                gte_SetRotMatrix(&D_800BD55C);
                gte_SetTransMatrix(&D_800BD55C);
                k = k - 1 < 0 ? k + 79 : k - 1;
                HW_MAX4(near->sz[0], near->sz[1], far->sz[0], far->sz[1]);
                p = D_800BE558[k].view;
                c = s->view;
                v = D_8011632C;
                sxy = D_8011636C;
                HW_MID(v[0], &p[1], &p[0]);
                ot = &b->ot[z + 170];
                HW_MID(v[1], &c[0], &c[1]);
                HW_PROJECT_SUB(2);
                sub = &D_8010FDC4[s->tex[0] * 2];
                HW_QUAD(&sub[0], near->sxy[0], *sxy[0], far->sxy[0], *sxy[1], ot);
                HW_QUAD(&sub[1], *sxy[0], near->sxy[1], *sxy[1], far->sxy[1], ot);
                HW_MAX4(near->sz[2], near->sz[0], far->sz[2], far->sz[0]);
                if (z < 0x1000 - D_800B4218)
                    HW_QUAD(rail, near->sxy[2], far->sxy[2], near->sxy[0], far->sxy[0], &b->ot[z + D_800B4218]);
                HW_MAX4(near->sz[3], near->sz[1], far->sz[3], far->sz[1]);
                if (z < 0x1000 - D_800B421C)
                    HW_QUAD(rail, near->sxy[3], far->sxy[3], near->sxy[1], far->sxy[1], &b->ot[z + D_800B421C]);
            }
        }
        if (vis_l | vis_r)
            last = i;
        *near = *far;
    }
    b->pool.ft4 = (POLY_FT4 *)q;
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_draw_road);
#endif

/* Old-style, against an int prototype: callers pass z as an int and the function narrows it, as in the game. */
void highway_draw_frame(b, f, z, unused)
DrawBuf *b;
Frame *f;
short z;
int unused;
{
    int i, vis;
    Model *model;
    Strip *src;
    POLY_FT3 *prim;
    DrawArgs args;
    MATRIX *m = D_8010EA34;
    MATRIX *w;

    HW_VIEW_MATRIX(m, f);
    w = D_8010EA34;
    w->t[0] -= D_800B41C8.vx;
    w->t[1] -= D_800B41C8.vy;
    w->t[2] -= D_800B41C8.vz;
    gte_CompMatrix(&D_801163E8->view, D_8010EA34, D_8010EA34);
    gte_SetRotMatrix(D_8010EA34);
    gte_SetTransMatrix(D_8010EA34);
    vis = highway_near_view(D_8010EA34->t, f->model->x1C);
    vis = D_8010EA34->t[2] < D_80116424 + 1000 ? vis & 1 : 0;
    if (!vis)
        return;
    model = f->model;
    if (model->strip_count) {
        src = model->strips;
        prim = b->pool.ft3;
        for (i = 0; i < f->model->strip_count; i++) {
            prim = highway_draw_ft3_z(src, prim, &b->ot[z]);
            src++;
        }
        b->pool.ft3 = prim;
    } else {
        args.faces = model->faces;
        args.prims = b->pool.g3;
        args.ot = &b->ot[z];
        args.model = f->model;
        b->pool.g3 = highway_draw_g3_list_cued(&args);
    }
}

void highway_draw_sky_strips(DrawBuf *b, Frame *f, short y)
{
    int i;
    Strip *src;
    POLY_FT3 *prim;

    D_8010EA34->t[0] = 0;
    D_8010EA34->t[1] = y;
    D_8010EA34->t[2] = 0;
    if (!D_8010EAE0)
        gte_SetRotMatrix(&D_801163E8->unrolled);
    else
        gte_SetRotMatrix(&D_801163E8->view);
    gte_SetTransMatrix(D_8010EA34);
    prim = b->pool.ft3;
    src = f->model->strips;
    for (i = 0; i < f->model->strip_count; i++) {
        prim = highway_draw_ft3(src, prim, b->ot_far);
        src++;
    }
    b->pool.ft3 = prim;
}

void highway_draw_sky_faces(DrawBuf *b, Frame *f, short y)
{
    DrawArgs job;

    D_8010EA34->t[0] = 0;
    D_8010EA34->t[1] = y;
    D_8010EA34->t[2] = 0;
    if (!D_8010EAE0)
        gte_SetRotMatrix(&D_801163E8->unrolled);
    else
        gte_SetRotMatrix(&D_801163E8->view);
    gte_SetTransMatrix(D_8010EA34);
    job.faces = f->model->faces;
    job.prims = b->pool.g3;
    job.ot = b->ot_far;
    job.model = f->model;
    b->pool.g3 = highway_draw_g3_list(&job);
}

MATRIX *highway_view_matrix(DrawBuf *b, Frame *f, MATRIX *m)
{
    HW_VIEW_MATRIX(m, f);
    m->t[0] -= D_800B41C8.vx;
    m->t[1] -= D_800B41C8.vy;
    m->t[2] -= D_800B41C8.vz;
    gte_CompMatrix(&D_801163E8->view, m, m);
    gte_SetRotMatrix(m);
    gte_SetTransMatrix(m);
    if (highway_in_view(m->t))
        return m;
    return 0;
}

void highway_draw_score(DrawBuf *b)
{
    highway_draw_small_number(b, D_80110B90, 85, 24, 1);
    highway_draw_sprite(b, 2, 18, 20, 108, 20, 0, 32, 108, 20, 0);
    highway_draw_number(b, D_800BDFB4, 248, 26, 1);
    highway_draw_sprite(b, 1, 200, 20, 109, 27, 0, 0, 109, 27, 0);
    if (D_8011639C == 1)
        highway_draw_sprite(b, 1, 100, 80, 118, 58, 0, 60, 118, 58, 0);
    if (D_8011639C == 2)
        highway_draw_sprite(b, 1, 100, 80, 117, 56, 130, 60, 117, 56, 0);
    if (D_8011639C == 3)
        highway_draw_sprite(b, 1, 100, 80, 115, 62, 0, 129, 115, 62, 0);
}

void highway_init(void)
{
    int i;

    func_80044A68(&D_80116358, 0, 1, GetTPage(1, 1, 0x300, 0), 0);
    highway_init_frames();
    highway_init_matrices();
    highway_init_model_pools();
    for (i = 0; i < 181; i++)
        D_800BD81C[i] = highway_make_model(i);
    highway_clear_input();
    highway_init_riders();
    highway_init_road();
    highway_init_view_planes();
    highway_init_camera();
    highway_init_state();
    highway_make_sky();
    highway_init_figures();
    highway_init_things();
    D_801163E8->corner[0].vy = 0;
    D_801163E8->corner[0].vz = 0;
    D_801163E8->corner[1].vy = 0;
    D_801163E8->corner[1].vz = 0;
    D_801163E8->corner[2].vy = -90;
    D_801163E8->corner[2].vz = 0;
    D_801163E8->corner[3].vy = -90;
    D_801163E8->corner[3].vz = 0;
    for (i = 0; i < 80; i++) {
        D_800BE558[i].tex[3] = i % 8;
        highway_build_segment();
        D_8010EB88++;
    }
}

void highway_init_gfx(void)
{
    DrawBuf *b = D_800C4C4C;

    func_80043814(&b[0].draw, 0, 0, 320, 240);
    SetDefDispEnv(&b[0].disp, 0, 240, 320, 240);
    func_80043814(&D_800C4C4C[1].draw, 0, 240, 320, 240);
    SetDefDispEnv(&D_800C4C4C[1].disp, 0, 0, 320, 240);
    D_800C4C4C[0].draw.isbg = 0;
    D_800C4C4C[1].draw.isbg = 0;
    D_800C4C4C[0].draw.r0 = 0;
    D_800C4C4C[0].draw.g0 = 0;
    D_800C4C4C[0].draw.b0 = 0;
    D_800C4C4C[1].draw.r0 = 0;
    D_800C4C4C[1].draw.g0 = 0;
    D_800C4C4C[1].draw.b0 = 0;
    SetGeomOffset(160, 120);
    SetGeomScreen(300);
    SetDispMask(1);
    SetBackColor(128, 128, 128);
    SetFarColor(0, 0, 0);
    SetFogNearFar(6500, 11500, 300);
    highway_init_prims(&b[0].pool);
    highway_init_prims(&D_800C4C4C[1].pool);
    highway_reset_prims(&b[0].pool);
    highway_reset_prims(&D_800C4C4C[1].pool);
    func_80044244(b[0].ot, 0x1000);
    func_80044244(D_800C4C4C[1].ot, 0x1000);
    func_80044244(b[0].ot_far, 20);
    func_80044244(D_800C4C4C[1].ot_far, 20);
    func_80044244(b[0].ot_hud, 10);
    func_80044244(D_800C4C4C[1].ot_hud, 10);
    D_80110A90 = b;
}

void highway_clear_buffer(void)
{
    func_80044244(D_80110A90->ot, 0x1000);
    func_80044244(D_80110A90->ot_far, 20);
    func_80044244(D_80110A90->ot_hud, 10);
    highway_reset_prims(&D_80110A90->pool);
}

void highway_init_prims(PrimPool *pool)
{
    int i;

    for (i = 0; i < 0; i++)
        SetPolyF3(&pool->f3_buf[i]);
    for (i = 0; i < 0; i++)
        SetPolyF4(&pool->f4_buf[i]);
    for (i = 0; i < 2500; i++)
        SetPolyG3(&pool->g3_buf[i]);
    for (i = 0; i < 2; i++)
        SetPolyG4(&pool->g4_buf[i]);
    for (i = 0; i < 800; i++)
        SetPolyFT3(&pool->ft3_buf[i]);
    for (i = 0; i < 800; i++)
        SetPolyFT4(&pool->ft4_buf[i]);
    for (i = 0; i < 0; i++)
        SetPolyGT3(&pool->gt3_buf[i]);
    for (i = 0; i < 0; i++)
        SetPolyGT4(&pool->gt4_buf[i]);
}

void highway_reset_prims(PrimPool *pool)
{
    pool->f3 = pool->f3_buf;
    pool->f4 = pool->f4_buf;
    pool->g3 = pool->g3_buf;
    pool->g4 = pool->g4_buf;
    pool->ft3 = pool->ft3_buf;
    pool->ft4 = pool->ft4_buf;
    pool->gt3 = pool->gt3_buf;
    pool->gt4 = pool->gt4_buf;
}

void highway_init_frames(void)
{
    int i;

    highway_frame_init(&D_8010EB90, 0);
    D_8010EB90.level = 0;
    D_8010EA38 = 0;
    for (i = 0; i < 400; i++)
        D_800BDFCC[i] = i + 1;
    for (i = 0; i < 10; i++) {
        D_800BE308[i].prev = &D_80116444[i];
        D_80116444[i].next = &D_800BE308[i];
        D_800BE308[i].next = 0;
        D_80116444[i].prev = 0;
    }
}

void highway_frame_init(Frame *f, short slot)
{
    f->m.m[0][0] = 0x1000;
    f->m.m[1][1] = 0x1000;
    f->m.m[2][2] = 0x1000;
    f->m.t[0] = 0;
    f->m.t[1] = 0;
    f->m.t[2] = 0;
    f->m.m[0][1] = 0;
    f->m.m[0][2] = 0;
    f->m.m[1][0] = 0;
    f->m.m[1][2] = 0;
    f->m.m[2][0] = 0;
    f->m.m[2][1] = 0;
    f->parent = &D_8010EB90;
    f->slot = slot;
    f->next = 0;
    f->prev = 0;
}

Frame *highway_frame_new(short id, int x1, int x2, int x3, Frame *parent, int tx, int ty, int tz, short rx, short ry,
                         short rz)
{
    SVECTOR r;
    short slot = highway_alloc_frame_slot();
    Frame *f = &D_800B6D50[slot];

    highway_frame_link(f, parent);
    f->model = D_800BD81C[id];
    f->slot = slot;
    f->id = id;
    r.vx = rx;
    r.vy = ry;
    r.vz = rz;
    RotMatrix(&r, &f->m);
    f->m.t[0] = tx;
    f->m.t[1] = ty;
    f->m.t[2] = tz;
    return f;
}

void highway_frame_set(Frame *f, SVECTOR *r, VECTOR *t)
{
    RotMatrixYXZ(r, &f->m);
    f->m.t[0] = t->vx;
    f->m.t[1] = t->vy;
    f->m.t[2] = t->vz;
}

Frame *highway_frame_new_yxz(short id, Frame *parent, VECTOR *t, SVECTOR *r)
{
    SVECTOR unused; /* only widens the stack frame */
    short slot = highway_alloc_frame_slot();
    Frame *f = &D_800B6D50[slot];

    highway_frame_link(f, parent);
    f->model = D_800BD81C[id];
    f->id = id;
    f->slot = slot;
    RotMatrixYXZ(r, &f->m);
    f->m.t[0] = t->vx;
    f->m.t[1] = t->vy;
    f->m.t[2] = t->vz;
    return f;
}

void highway_frame_free(Frame *f)
{
    highway_frame_unlink(f);
    highway_free_frame_slot(f->slot);
}

short highway_alloc_frame_slot(void)
{
    short slot = D_8010EA38;

    D_8010EA38 = D_800BDFCC[slot];
    return slot;
}

void highway_free_frame_slot(short slot)
{
    D_800BDFCC[slot] = D_8010EA38;
    D_8010EA38 = slot;
}

void highway_frame_link(Frame *f, Frame *parent)
{
    short level;

    f->parent = parent;
    level = f->level = parent->level + 1;
    f->next = D_80116444[level].next;
    f->prev = f->next->prev;
    D_80116444[level].next->prev = f;
    D_80116444[level].next = f;
}

void highway_frame_unlink(Frame *f)
{
    f->next->prev = f->prev;
    f->prev->next = f->next;
}

void highway_init_model_pools(void)
{
    D_800BDFBC = 0;
    D_8010EBD0 = 0;
    D_800BE304 = 0;
    D_800C4C3C = D_800B4258;
    D_80110AA8 = D_800B425C;
    D_800BD7EC = D_800B4254;
}

Model *highway_make_model(int id)
{
    Model *m = highway_alloc_model();
    ModelBox *b = &D_800BD7EC[id];
    int faces = b->faces, strips = b->strips;

    m->min_x = b->min.vx;
    m->max_x = b->max.vx;
    m->min_z = b->min.vz;
    m->max_z = b->max.vz;
    m->x02 = 0;
    m->count = faces;
    m->strip_count = strips;
    m->x08 = 0;
    m->total = faces + strips;
    m->faces = highway_alloc_faces(faces);
    m->strips = highway_alloc_strips(strips);
    m->x1C = D_800BD7EC[id].x14;
    return m;
}

Model *highway_alloc_model(void)
{
    return &D_8010D30C[D_800BE304++];
}

Face *highway_alloc_faces(int n)
{
    int i = D_800BDFBC;

    D_800BDFBC += n;
    return &D_800C4C3C[i];
}

Strip *highway_alloc_strips(int n)
{
    int i = D_8010EBD0;

    D_8010EBD0 += n;
    return &D_80110AA8[i];
}

/* Uploads the overlay's TIMs, works out the tpages and cluts the drawing uses, and sets up the HUD's boxes. */
#ifdef NON_MATCHING
/* Only the gauge boxes' constant stores: the game reaches box 0 and box 4 through one register
 * and allocates the constants differently. */
void highway_load_textures(void)
{
    TIM_IMAGE tim;
    GaugeBox *box = D_80110CD8;
    int i;

    for (i = 0; i < 0x34; i++) {
        highway_upload_tim(D_800B42EC[i]);
        OpenTIM(D_800B42EC[i]);
        ReadTIM(&tim);
    }
    D_800BD7F0[0] = GetTPage(1, 1, 0x2C0, 0x100);
    D_80116690[0] = GetClut(0, 0x1FD);
    D_800BD7F0[1] = GetTPage(1, 1, 0x2C0, 0x100);
    D_80116690[1] = GetClut(0, 0x1FD);
    D_800BD7F0[2] = GetTPage(1, 1, 0x2C0, 0x100);
    D_80116690[2] = GetClut(0, 0x1FD);
    D_800BC684[0] = GetTPage(0, 1, 0x280, 0);
    D_801163BC[0] = GetClut(0, 0x1E0);
    D_800BC684[1] = GetTPage(0, 1, 0x280, 0x40);
    D_801163BC[1] = GetClut(0, 0x1E1);
    D_800BC684[2] = GetTPage(0, 1, 0x280, 0x80);
    D_801163BC[2] = GetClut(0x20, 0x1E0);
    D_800BC684[3] = GetTPage(0, 1, 0x280, 0xC0);
    D_801163BC[3] = GetClut(0x10, 0x1E0);
    D_800BC684[4] = GetTPage(1, 1, 0x2C0, 0x40);
    D_801163BC[4] = GetClut(0, 0x1FE);
    D_800BC684[5] = GetTPage(0, 1, 0x240, 0xC0);
    D_801163BC[5] = GetClut(0xF0, 0x1E3);
    D_800BC684[6] = GetTPage(0, 1, 0x240, 0x40);
    D_801163BC[6] = GetClut(0xF0, 0x1E2);
    D_800BC684[7] = GetTPage(0, 1, 0x240, 0x80);
    D_801163BC[7] = GetClut(0xF0, 0x1E4);
    D_800BC684[8] = GetTPage(0, 1, 0x2C0, 0);
    D_801163BC[8] = GetClut(0xF0, 0x1E1);
    D_800BC684[9] = GetTPage(0, 1, 0x2C0, 0xC0);
    D_801163BC[9] = GetClut(0xF0, 0x1E5);
    D_800BC684[10] = GetTPage(0, 1, 0x300, 0xC0);
    D_801163BC[10] = GetClut(0xF0, 0x1E0);
    D_801163FC[0] = GetClut(0x30, 0x1E0);
    D_800BD538[0] = GetTPage(0, 1, 0x300, 0);
    D_801163FC[1] = GetClut(0x40, 0x1E0);
    D_800BD538[1] = GetTPage(0, 1, 0x300, 0);
    D_801163FC[2] = GetClut(0x50, 0x1E0);
    D_800BD538[2] = GetTPage(0, 1, 0x300, 0);
    D_801163FC[3] = GetClut(0x60, 0x1E0);
    D_800BD538[3] = GetTPage(0, 1, 0x300, 0);
    D_801163FC[4] = GetClut(0x70, 0x1E0);
    D_800BD538[4] = GetTPage(0, 1, 0x300, 0);
    D_801163FC[5] = GetClut(0x80, 0x1E0);
    D_800BD538[5] = GetTPage(0, 1, 0x300, 0);
    D_801163FC[6] = GetClut(0x90, 0x1E0);
    D_800BD538[6] = GetTPage(0, 1, 0x300, 0);
    D_801163FC[7] = GetClut(0xA0, 0x1E0);
    D_800BD538[7] = GetTPage(0, 1, 0x300, 0);
    D_800C4A98 = GetClut(0, 0x1E5);
    D_8010FD7C = GetTPage(0, 1, 0x240, 0);
    D_80116674 = GetClut(0x30, 0x1E2);
    D_800BD590 = GetTPage(0, 1, 0x208, 0);
    D_80110CD8[4].x = 0xD9;
    D_80110CD8[4].y = 0x28;
    D_80110CD8[4].bar_x = 0xDC;
    D_80110CD8[4].bar_y = 0x31;
    D_80110CD8[4].icon_u[0] = 0;
    D_80110CD8[4].icon_u[1] = 0x5C;
    D_80110CD8[4].icon_u[2] = 0x6A;
    D_80110CD8[4].icon_u[3] = 0x78;
    D_80110CD8[4].icon_v[0] = 0;
    D_80110CD8[4].icon_v[1] = 0x2A;
    D_80110CD8[4].icon_v[2] = 0x2A;
    D_80110CD8[4].icon_v[3] = 0x2A;
    D_80110CD8[4].w = 0x54;
    D_80110CD8[4].h = 0x10;
    D_80110CD8[4].icon_size = 0x0E;
    D_80110CD8[4].icon_x = 0x45;
    D_80110CD8[2].x = 0xD9;
    D_80110CD8[2].y = 0x14;
    D_80110CD8[2].bar_x = 0xDC;
    D_80110CD8[2].bar_y = 0x1D;
    D_80110CD8[2].icon_u[0] = 0;
    D_80110CD8[2].icon_u[1] = 0x5C;
    D_80110CD8[2].icon_u[2] = 0x6A;
    D_80110CD8[2].icon_u[3] = 0x78;
    D_80110CD8[2].icon_v[0] = 0;
    D_80110CD8[2].icon_v[1] = 0x0E;
    D_80110CD8[2].icon_v[2] = 0x0E;
    D_80110CD8[2].icon_v[3] = 0x0E;
    D_80110CD8[2].w = 0x54;
    D_80110CD8[2].h = 0x10;
    D_80110CD8[2].icon_size = 0x0E;
    D_80110CD8[2].icon_x = 0x45;
    D_80110CD8[3].x = 0x14;
    D_80110CD8[3].y = 0x14;
    D_80110CD8[3].bar_x = 0x25;
    D_80110CD8[3].bar_y = 0x1D;
    D_80110CD8[3].icon_u[0] = 0;
    D_80110CD8[3].icon_u[1] = 0x5C;
    D_80110CD8[3].icon_u[2] = 0x6A;
    D_80110CD8[3].icon_u[3] = 0x78;
    D_80110CD8[3].icon_v[0] = 0x10;
    D_80110CD8[3].icon_v[1] = 0x1C;
    D_80110CD8[3].icon_v[2] = 0x1C;
    D_80110CD8[3].icon_v[3] = 0x1C;
    D_80110CD8[3].w = 0x54;
    D_80110CD8[3].h = 0x10;
    D_80110CD8[3].icon_size = 0x0E;
    D_80110CD8[3].icon_x = 1;
    D_80110CD8[1].x = 0x14;
    D_80110CD8[1].y = 0x28;
    D_80110CD8[1].bar_x = 0x25;
    D_80110CD8[1].bar_y = 0x31;
    D_80110CD8[1].icon_u[0] = 0;
    D_80110CD8[1].icon_u[1] = 0x5C;
    D_80110CD8[1].icon_u[2] = 0x6A;
    D_80110CD8[1].icon_u[3] = 0x78;
    D_80110CD8[1].icon_v[0] = 0x10;
    D_80110CD8[1].icon_v[1] = 0;
    D_80110CD8[1].icon_v[2] = 0;
    D_80110CD8[1].icon_v[3] = 0;
    D_80110CD8[1].w = 0x54;
    D_80110CD8[1].h = 0x10;
    D_80110CD8[1].icon_size = 0x0E;
    D_80110CD8[1].icon_x = 1;
    box->x = 0x74;
    box->y = 0xC8;
    box->bar_x = 0x8D;
    box->bar_y = 0xD9;
    box->icon_u[0] = 0;
    box->icon_u[1] = 0x5C;
    box->icon_u[2] = 0x72;
    box->icon_u[3] = 0x88;
    box->icon_v[0] = 0x20;
    box->icon_v[1] = 0x38;
    box->icon_v[2] = 0x38;
    box->icon_v[3] = 0x38;
    box->w = 0x5C;
    box->h = 0x18;
    box->icon_size = 0x16;
    box->icon_x = 1;
    D_80116368 = GetTPage(1, 1, 0x140, 0x100);
    D_80110B8C = GetClut(0, 0x1F4);
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_load_textures);
#endif

void highway_upload_tim(unsigned int *tim)
{
    TIM_IMAGE image;

    OpenTIM(tim);
    while (ReadTIM(&image)) {
        if (image.caddr)
            func_80044000(image.crect, image.caddr);
        if (image.paddr)
            func_80044000(image.prect, image.paddr);
    }
}

/* Sets up the textures' quads: eleven 256 x 64 bands, each also cut into halves and eighths, and eight 64 x 32
 * cells, each also cut into top and bottom halves. */
void highway_setup_road_quads(void)
{
    int i;

    BAND(0, 0, 0x3F);
    BAND(1, 0x40, 0x7F);
    BAND(2, 0x80, 0xBF);
    BAND(3, 0xC0, 0xFF);
    BAND(4, 0x40, 0x7F);
    BAND(5, 0xC0, 0xFF);
    BAND(6, 0x40, 0x7F);
    BAND(7, 0x80, 0xBF);
    BAND(8, 0, 0x3F);
    BAND(9, 0xC0, 0xFF);
    BAND(10, 0xC0, 0xFF);
    SET_QUAD(D_800BDF04[0], 0, 0x3F, 0, 0x1F, D_800BD538[0], D_801163FC[0]);
    SET_QUAD(D_800BDF04[1], 0x40, 0x7F, 0, 0x1F, D_800BD538[1], D_801163FC[1]);
    SET_QUAD(D_800BDF04[2], 0x80, 0xBF, 0, 0x1F, D_800BD538[2], D_801163FC[2]);
    SET_QUAD(D_800BDF04[3], 0xC0, 0xFF, 0, 0x1F, D_800BD538[3], D_801163FC[3]);
    SET_QUAD(D_800BDF04[4], 0, 0x3F, 0x20, 0x3F, D_800BD538[4], D_801163FC[4]);
    SET_QUAD(D_800BDF04[5], 0x40, 0x7F, 0x20, 0x3F, D_800BD538[5], D_801163FC[5]);
    SET_QUAD(D_800BDF04[6], 0x80, 0xBF, 0x20, 0x3F, D_800BD538[6], D_801163FC[6]);
    SET_QUAD(D_800BDF04[7], 0xC0, 0xFF, 0x20, 0x3F, D_800BD538[7], D_801163FC[7]);
    for (i = 0; i < 8; i++)
        highway_split_quad_v(&D_800BDF04[i], &D_8010EA3C[i], &D_8010EAE4[i]);
}
#undef BAND
#undef SET_QUAD

void highway_split_quad_v(TexQuad *src, TexQuad *top, TexQuad *bottom)
{
    unsigned char mid = (src->v0 + src->v2 - 1) >> 1;

    top->u0 = src->u0;
    top->v0 = src->v0;
    top->u1 = src->u1;
    top->v1 = src->v1;
    top->u2 = src->u2;
    top->v2 = mid;
    top->u3 = src->u3;
    top->v3 = mid;
    top->tpage = src->tpage;
    top->clut = src->clut;
    bottom->u0 = src->u0;
    bottom->v0 = mid;
    bottom->u1 = src->u1;
    bottom->v1 = mid;
    bottom->u2 = src->u2;
    bottom->v2 = src->v2;
    bottom->u3 = src->u3;
    bottom->v3 = src->v3;
    bottom->tpage = src->tpage;
    bottom->clut = src->clut;
}

void highway_split_quad_grid(TexQuad *src, int i)
{
    unsigned char mx, q1, q3, my;
    int n;

    mx = (src->u0 + src->u1 - 1) >> 1;
    q1 = (src->u0 + mx - 1) >> 1;
    q3 = (src->u1 + mx - 1) >> 1;
    my = (src->v0 + src->v2 - 1) >> 1;
    n = i * 8;
    PIECE(src->u0, q1, src->v0, my);
    n = i * 8 + 1;
    PIECE(q1 + 1, mx, src->v0, my);
    n = i * 8 + 2;
    PIECE(mx + 1, q3, src->v0, my);
    n = i * 8 + 3;
    PIECE(q3 + 1, src->u1, src->v0, my);
    n = i * 8 + 4;
    PIECE(src->u0, q1, my + 1, src->v2);
    n = i * 8 + 5;
    PIECE(q1 + 1, mx, my + 1, src->v2);
    n = i * 8 + 6;
    PIECE(mx + 1, q3, my + 1, src->v2);
    n = i * 8 + 7;
    PIECE(q3 + 1, src->u1, my + 1, src->v2);
}
#undef PIECE

/* Splits a quad's texture into a left and a right half, kept as the pair 2i, 2i+1. */
void highway_split_quad_h(TexQuad *src, int i)
{
    unsigned char mid = (src->u0 + src->u1 - 1) >> 1;

    D_8010FDC4[i * 2].u0 = src->u0;
    D_8010FDC4[i * 2].v0 = src->v0;
    D_8010FDC4[i * 2].u1 = mid;
    D_8010FDC4[i * 2].v1 = src->v1;
    D_8010FDC4[i * 2].u2 = src->u2;
    D_8010FDC4[i * 2].v2 = src->v2;
    D_8010FDC4[i * 2].u3 = mid;
    D_8010FDC4[i * 2].v3 = src->v3;
    D_8010FDC4[i * 2].clut = src->clut;
    D_8010FDC4[i * 2].tpage = src->tpage;
    D_8010FDC4[i * 2 + 1].u0 = mid;
    D_8010FDC4[i * 2 + 1].v0 = src->v0;
    D_8010FDC4[i * 2 + 1].u1 = src->u1;
    D_8010FDC4[i * 2 + 1].v1 = src->v1;
    D_8010FDC4[i * 2 + 1].u2 = mid;
    D_8010FDC4[i * 2 + 1].v2 = src->v2;
    D_8010FDC4[i * 2 + 1].u3 = src->u3;
    D_8010FDC4[i * 2 + 1].v3 = src->v3;
    D_8010FDC4[i * 2 + 1].clut = src->clut;
    D_8010FDC4[i * 2 + 1].tpage = src->tpage;
}

void highway_init_road(void)
{
    int i, j;

    highway_init_props();
    if (D_800BC4D4 == 0)
        D_800C4C44 = D_800B4260[0].road;
    if (D_800BC4D4 == 1)
        D_800C4C44 = D_800B4260[1].road;
    D_80110B98 = 0;
    D_801163EC = 0;
    D_8010FDA8.vx = 0;
    D_8010FDA8.vy = 0;
    D_8010FDA8.vz = 0;
    D_8010FD3C = 1;
    D_800BE554 = 0;
    D_801163F4 = 0;
    D_80110B98 = 0;
    D_80116440 = 0;
    D_80110BE8 = 0;
    D_80110BBC = 0;
    D_80110BB0 = 0;
    D_80110B94 = 0;
    D_8010EBD4 = 0;
    D_8010EB8C = 0;
    D_8010EADC = 0;
    if (D_800BC4D4 == 0)
        D_801163E4 = D_800B4260[0].patterns;
    if (D_800BC4D4 == 1)
        D_801163E4 = D_800B4260[1].patterns;
    D_80110BD4 = 0;
    D_80110CCC = 0;
    D_80110BCC = 0;
    D_80110CC8 = 0;
    for (i = 0; i < 80; i++) {
        for (j = 9; j >= 0; j--)
            D_800BE558[i].tex[j] = 0;
    }
    for (i = 0; i < 10; i++) {
        D_800BD598[i].vx = 0;
        D_800BD598[i].vy = 0;
        D_800BD598[i].vz = 0;
    }
    D_8010EBC8 = 0;
}

/* Builds the next road segment: starts the next course piece when one runs out, then steps the angles and position
 * on by one segment's length. */
#ifdef NON_MATCHING
/* Scheduling only: the game keeps the RoadCmd loads among the stores before them, and loads
 * D_800BE554 early. */
const SVECTOR D_800A0000 = {0, 0, 0x100}; /* the asm carries its own copy */
void highway_build_segment(void)
{
    SVECTOR dir = D_800A0000;
    SVECTOR step = D_800A0000;
    SVECTOR rot = D_800A0008;
    MATRIX m;
    RoadCmd *cmd;
    int curve, c, pitch, roll;

    if (D_8010FD3C) {
        cmd = D_800C4C44;
        D_80110BE8 = D_80116440;
        D_8010EBD4 = D_80110B94;
        D_8010EADC = 0;
        D_80110BB0 = 0;
        D_8010EBD8 = cmd->len;
        D_8011667C = cmd->width;
        D_80116440 = cmd->pitch;
        D_8010FDBC = cmd->pattern;
        D_80110B94 = cmd->roll;
        D_80110CD0 = cmd->turn;
        curve = cmd->curve;
        c = csqrt((curve * curve << 12) - 0x400);
        D_80110BBC = ((D_80116440 - D_80110BE8) << 16) / D_8010EBD8;
        D_8010EB8C = ((D_80110B94 - D_8010EBD4) << 16) / D_8010EBD8;
        D_801163F4 = ratan2(0x800, c) * 2;
        D_8010FD3C = 0;
        D_801163EC = 0;
        D_800BD584 = curve;
        D_80110BCC = 0;
        D_80110CC8 = 0;
        D_80110BD4 = D_801163E4[D_8010FDBC + 1];
        D_80110CCC = D_801163E4[D_8010FDBC + 1];
        D_80110BC0 = D_80110BB4 = D_801163E4 + (D_801163E4 + D_801163E4[0])[D_8010FDBC * 2 + 1];
        D_800BE554++;
        D_800C4C44++;
    }
    D_80110BCC++;
    D_80110BCC %= D_80110BD4;
    D_80110CC8++;
    D_80110CC8 %= D_80110BD4;
    D_800BE558[D_801163A0].tex[0] = D_80110BB4[D_80110BCC];
    if (D_8010EBC8 == 1)
        D_800BE558[D_801163A0].tex[9] = 1;
    if (D_800BE558[D_801163A0].tex[0] == 4)
        D_8010EBC8 = 1;
    if (D_80110CD0 == 1) {
        D_80110B98 += D_801163F4;
        D_801163EC += D_801163F4;
    }
    if (D_80110CD0 == 2) {
        D_80110B98 -= D_801163F4;
        D_801163EC += D_801163F4;
    }
    if (--D_8010EBD8 == 0)
        D_8010FD3C = 1;
    if (D_80110B98 < 0)
        D_80110B98 += 0x1000;
    D_80110B98 %= 0x1000;
    D_8010EADC += D_8010EB8C;
    D_80110BB0 += D_80110BBC;
    pitch = (D_80110BB0 >> 16) + D_80110BE8;
    roll = (D_8010EADC >> 16) + D_8010EBD4;
    rot.vx = pitch;
    rot.vy = D_80110B98;
    rot.vz = 0;
    RotMatrixYXZ(&rot, &m);
    ApplyMatrixSV(&m, &dir, &step);
    D_8010FDA8.vx += step.vx;
    D_8010FDA8.vy += step.vy;
    D_8010FDA8.vz += step.vz;
    D_800BE558[D_801163A0].pos.vx = D_8010FDA8.vx;
    D_800BE558[D_801163A0].pos.vy = D_8010FDA8.vy;
    D_800BE558[D_801163A0].pos.vz = D_8010FDA8.vz;
    D_800BE558[D_801163A0].curve = D_800BD584;
    D_800BE558[D_801163A0].turn = D_80110CD0;
    D_800BE558[D_801163A0].width = D_8011667C;
    D_800BE558[D_801163A0].rot.vx = 0;
    D_800BE558[D_801163A0].rot.vy = D_80110B98;
    D_800BE558[D_801163A0].rot.vz = roll;
    RotMatrix(&D_800BE558[D_801163A0].rot, &D_800BE558[D_801163A0].m);
    D_800BE558[D_801163A0].rot.vx = pitch;
    highway_place_props();
    D_801163A0 = (D_801163A0 + 1) % 80;
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_build_segment);
#endif

const SVECTOR D_800A0008 = {0, 0, 0};

void highway_free_segment_frames(void)
{
    int i;

    for (i = 0; i < D_800BE558[D_801163A0].nframes; i++)
        highway_frame_free(D_800BE558[D_801163A0].frames[i]);
    D_800BE558[D_801163A0].nframes = 0;
}

/* The point `side` across the road at `t` along it (segment t >> 8, 1/256ths into it). */
void highway_road_point(int t, int side, VECTOR *out)
{
    Segment *a, *b;
    int frac;

    D_80110BB8->side.vx = side << 18;
    D_80110BB8->side.vy = 0;
    D_80110BB8->side.vz = 0;
    frac = t & 0xFF;
    a = &D_800BE558[(t >> 8) % 80];
    b = &D_800BE558[((t >> 8) + 1) % 80];
    ApplyMatrixLV(&a->m, &D_80110BB8->side, &D_80110BB8->turned0);
    ApplyMatrixLV(&b->m, &D_80110BB8->side, &D_80110BB8->turned1);
    D_80110BB8->at0.vx = (D_80110BB8->turned0.vx >> 18) + a->pos.vx;
    D_80110BB8->at0.vy = (D_80110BB8->turned0.vy >> 18) + a->pos.vy;
    D_80110BB8->at0.vz = (D_80110BB8->turned0.vz >> 18) + a->pos.vz;
    D_80110BB8->at1.vx = (D_80110BB8->turned1.vx >> 18) + b->pos.vx;
    D_80110BB8->at1.vy = (D_80110BB8->turned1.vy >> 18) + b->pos.vy;
    D_80110BB8->at1.vz = (D_80110BB8->turned1.vz >> 18) + b->pos.vz;
    out->vx = ((D_80110BB8->at1.vx - D_80110BB8->at0.vx) * frac >> 8) + D_80110BB8->at0.vx;
    out->vy = ((D_80110BB8->at1.vy - D_80110BB8->at0.vy) * frac >> 8) + D_80110BB8->at0.vy;
    out->vz = ((D_80110BB8->at1.vz - D_80110BB8->at0.vz) * frac >> 8) + D_80110BB8->at0.vz;
}

/* highway_road_point's point, and the road's angles there; each angle is turned the short way round before blending. */
void highway_road_point_rot(int t, int side, VECTOR *out, SVECTOR *rot)
{
    Segment *a, *b;
    int frac;
    short ra, rb;

    D_80110BB8->side.vx = side << 18;
    D_80110BB8->side.vy = 0;
    D_80110BB8->side.vz = 0;
    frac = t & 0xFF;
    a = &D_800BE558[(t >> 8) % 80];
    b = &D_800BE558[((t >> 8) + 1) % 80];
    ApplyMatrixLV(&a->m, &D_80110BB8->side, &D_80110BB8->turned0);
    ApplyMatrixLV(&b->m, &D_80110BB8->side, &D_80110BB8->turned1);
    D_80110BB8->at0.vx = (D_80110BB8->turned0.vx >> 18) + a->pos.vx;
    D_80110BB8->at0.vy = (D_80110BB8->turned0.vy >> 18) + a->pos.vy;
    D_80110BB8->at0.vz = (D_80110BB8->turned0.vz >> 18) + a->pos.vz;
    D_80110BB8->at1.vx = (D_80110BB8->turned1.vx >> 18) + b->pos.vx;
    D_80110BB8->at1.vy = (D_80110BB8->turned1.vy >> 18) + b->pos.vy;
    D_80110BB8->at1.vz = (D_80110BB8->turned1.vz >> 18) + b->pos.vz;
    out->vx = ((D_80110BB8->at1.vx - D_80110BB8->at0.vx) * frac >> 8) + D_80110BB8->at0.vx;
    out->vy = ((D_80110BB8->at1.vy - D_80110BB8->at0.vy) * frac >> 8) + D_80110BB8->at0.vy;
    out->vz = ((D_80110BB8->at1.vz - D_80110BB8->at0.vz) * frac >> 8) + D_80110BB8->at0.vz;
    rot->vx = a->rot.vx;
    rot->vy = a->rot.vy;
    rot->vz = a->rot.vz;
    ra = a->rot.vx;
    rb = b->rot.vx;
    if (rb - a->rot.vx > 0x800)
        ra += 0x1000;
    if (ra - rb > 0x800)
        rb += 0x1000;
    rot->vx += (rb - ra) * frac >> 8;
    ra = a->rot.vy;
    rb = b->rot.vy;
    if (rb - a->rot.vy > 0x800)
        ra += 0x1000;
    if (ra - rb > 0x800)
        rb += 0x1000;
    rot->vy += (rb - ra) * frac >> 8;
    ra = a->rot.vz;
    rb = b->rot.vz;
    if (rb - a->rot.vz > 0x800)
        ra += 0x1000;
    if (ra - rb > 0x800)
        rb += 0x1000;
    rot->vz += (rb - ra) * frac >> 8;
}

/* The point dx to the side of the road at t (24.8 steps along it), and the road's heading there, both blended
 * between the two steps t lies between. */
void highway_road_point_rot_fine(int t, int dx, VECTOR *pos, SVECTOR *rot)
{
    Segment *a, *b;
    int frac = t & 0xFF;
    short ra, rb;

    a = &D_800BE558[(t >> 8) % 80];
    b = &D_800BE558[((t >> 8) + 1) % 80];
    D_80110BB8->side.vx = dx << 16;
    D_80110BB8->side.vy = 0;
    D_80110BB8->side.vz = 0;
    ApplyMatrixLV(&a->m, &D_80110BB8->side, &D_80110BB8->turned0);
    ApplyMatrixLV(&b->m, &D_80110BB8->side, &D_80110BB8->turned1);
    D_80110BB8->at0.vx = (D_80110BB8->turned0.vx >> 16) + a->pos.vx;
    D_80110BB8->at0.vy = (D_80110BB8->turned0.vy >> 16) + a->pos.vy;
    D_80110BB8->at0.vz = (D_80110BB8->turned0.vz >> 16) + a->pos.vz;
    D_80110BB8->at1.vx = (D_80110BB8->turned1.vx >> 16) + b->pos.vx;
    D_80110BB8->at1.vy = (D_80110BB8->turned1.vy >> 16) + b->pos.vy;
    D_80110BB8->at1.vz = (D_80110BB8->turned1.vz >> 16) + b->pos.vz;
    pos->vx = ((D_80110BB8->at1.vx - D_80110BB8->at0.vx) * frac >> 8) + D_80110BB8->at0.vx;
    pos->vy = ((D_80110BB8->at1.vy - D_80110BB8->at0.vy) * frac >> 8) + D_80110BB8->at0.vy;
    pos->vz = ((D_80110BB8->at1.vz - D_80110BB8->at0.vz) * frac >> 8) + D_80110BB8->at0.vz;
    rot->vx = a->rot.vx;
    rot->vy = a->rot.vy;
    rot->vz = 0;
    ra = a->rot.vx;
    rb = b->rot.vx;
    if (b->rot.vx - a->rot.vx > 0x800)
        ra += 0x1000;
    if (ra - b->rot.vx > 0x800)
        rb += 0x1000;
    rot->vx += (rb - ra) * frac >> 8;
    ra = a->rot.vy;
    rb = b->rot.vy;
    if (b->rot.vy - a->rot.vy > 0x800)
        ra += 0x1000;
    if (ra - b->rot.vy > 0x800)
        rb += 0x1000;
    rot->vy += (rb - ra) * frac >> 8;
}

/* Stands the road's props up around the camera's step: the ten slots highway_step_prop describes, the first six blended
 * along the road (a lamp also marks where its light goes), the rest placed by highway_road_point_rot. */
void highway_place_props(void)
{
    int n = 0;
    VECTOR pos;
    SVECTOR rot;
    SVECTOR unused = D_800A0008;
    unsigned char id;
    short dx, dy;
    unsigned short flags;
    short drot;
    int i;

    for (i = 0; i < 10; i++) {
        highway_step_prop(i, &id, &dx, &dy, &flags, &drot);
        if (!id) {
            if (flags & 2) {
                highway_road_point_rot_fine(D_801163A0 << 8, dx, &pos, &rot);
                D_800BD598[i].vx = pos.vx;
                D_800BD598[i].vy = pos.vy;
                D_800BD598[i].vz = pos.vz;
            }
        }
        if (!id)
            continue;
        if (i < 6) {
            highway_road_point_rot_fine(D_801163A0 << 8, dx, &pos, &rot);
            rot.vx = 0;
            rot.vz = 0;
            rot.vy += drot;
        } else {
            highway_road_point_rot(D_801163A0 << 8, dx, &pos, &rot);
            rot.vy += drot;
        }
        if (flags & 1)
            pos.vy = D_800BD598[i].vy - dy;
        else
            pos.vy = pos.vy - dy;
        D_800BE558[D_801163A0].frames[n++] = highway_frame_new_yxz(id, &D_8010EB90, &pos, &rot);
        D_800BE558[D_801163A0].nframes++;
    }
}

void highway_init_riders(void)
{
    D_800BE2EC = 0x78;
    D_800BC4E0[0] = 0x8E;
    D_800BC4E0[1] = 0x95;
    D_800BC4E0[2] = 0x94;
    D_800BDFB8 = 0;
    D_800BC4E0[3] = 0xB1;
    highway_clear_rider(0);
    highway_clear_rider(1);
    highway_clear_rider(2);
    highway_clear_rider(3);
    highway_clear_rider(4);
    highway_clear_rider(5);
    highway_init_fixed_riders();
}

void highway_update_riders(int z, DrawBuf *b)
{
    D_800BE2F4 = b;
    D_800BD818 = z;
    highway_spawn_soldiers();
    highway_move_riders();
    highway_place_riders();
    highway_keep_all_on_road();
    highway_animate_riders();
    highway_swap_rider_models();
    highway_update_figures();
    highway_collide_riders();
}

inline void highway_clear_rider(int i)
{
    Bike *r = &D_800B4F4C[i].bike;
    int n;

    D_800B4F4C[i].x00 = 0;
    D_800B4F4C[i].x04 = 0;
    D_800B4F4C[i].x08 = 0;
    D_800B4F4C[i].x10 = 0;
    D_800B4F4C[i].x12 = 0;
    D_800B4F4C[i].x14 = 0;
    for (n = 0x4F; n >= 0; n--)
        ((int *)r)[n] = 0;
    r->state = 5;
}

/* Sets up the three riders always on the road: Cloud (0), the truck (1) and the soldier on its back (5). */
#ifdef NON_MATCHING
/* Scheduling only: the order of the constant stores and which registers hold the constants. */
void highway_init_fixed_riders(void)
{
    Bike *r;

    highway_clear_rider(0);
    r = &D_800B4F4C[0].bike;
    r->state = 0;
    D_800B4F4C[0].bike.nframes = 4;
    D_800B4F4C[0].bike.nparts = 2;
    D_800B4F4C[0].bike.part[1] = 0x46;
    D_800B4F4C[0].bike.part[2] = 0x46;
    D_800B4F4C[0].bike.part[3] = 0x46;
    D_800B4F4C[0].bike.front = 0x14;
    D_800B4F4C[0].bike.rear = 0x64;
    D_800B4F4C[0].bike.front_r = 0x24;
    D_800B4F4C[0].bike.rear_r = 0x24;
    D_800B4F4C[0].bike.x7C = 0x30;
    D_800B4F4C[0].bike.x80 = 0x30;
    D_800B4F4C[0].bike.type = 0;
    D_800B4F4C[0].bike.rider_model = 1;
    D_800B4F4C[0].bike.bike_model = 0;
    D_800B4F4C[0].bike.pos.vx = 0;
    D_800B4F4C[0].bike.pos.vz = 0x3E800;
    D_800B4F4C[0].bike.z_max = 0xDAC00;
    D_800B4F4C[0].bike.x38 = 0x1F400;
    D_800B4F4C[0].bike.part[0] = 0;
    D_800B4F4C[0].bike.xA0 = 1;
    D_800B4F4C[0].frames[0] = highway_frame_new(0x61, 0, 0, 1, &D_8010EB90, 0, 0, 0, 0, 0, 0);
    D_800B4F4C[0].frames[1] = highway_frame_new(0x9B, 0, 0, 1, D_800B4F4C[0].frames[0], 0, -10, 0x55, 0, 0, 0);
    D_800B4F4C[0].frames[2] = highway_frame_new(0x95, 0, 0, 1, D_800B4F4C[0].frames[0], -0x50, 0, -0x28, 0, 0, 0);
    D_800B4F4C[0].frames[3] = highway_frame_new(0x61, 0, 0, 1, D_800B4F4C[0].frames[0], 0, 0, 0, 0, 0, 0);

    highway_clear_rider(1);
    r = &D_800B4F4C[1].bike;
    r->state = 0;
    D_800B4F4C[1].bike.type = 1;
    D_800B4F4C[1].bike.nframes = 3;
    D_800B4F4C[1].bike.nparts = 3;
    if (!D_800BC4D4) {
        D_800B4F4C[1].bike.rider_model = 2;
        D_800B4F4C[1].bike.bike_model = 0xD;
    } else {
        D_800B4F4C[1].bike.rider_model = 0x11;
        D_800B4F4C[1].bike.bike_model = 0x13;
    }
    r->pos.vz = 0x9C400;
    r->z_max = 0x9C400;
    r->x38 = 0x5DC00;
    r->part[1] = 0x46;
    r->xA0 = 1;
    r->x78 = 0x800;
    r->x7C = 0x1F4;
    r->front = -0x1E;
    r->rear = 0x64;
    r->pos.vx = 0;
    r->part[0] = 0;
    r->part[2] = 0x4B;
    r->front_r = 0x4B;
    r->rear_r = 0x4B;
    D_800B4F4C[1].frames[0] = highway_frame_new(0x61, 0, 0, 1, &D_8010EB90, 0, 0, 0, 0, 0, 0);
    D_800B4F4C[1].frames[1] = highway_frame_new(0x99, 0, 0, 1, D_800B4F4C[1].frames[0], 0, 0, 0, 0, 0, 0);
    D_800B4F4C[1].frames[2] = highway_frame_new(0x9A, 0, 0, 1, D_800B4F4C[1].frames[0], 0, 3, -6, 0, 0, 0);

    highway_clear_rider(5);
    D_800B4F4C[5].bike.state = 5;
    D_800B4F4C[5].bike.type = 10;
    D_800B4F4C[5].bike.nframes = 2;
    D_800B4F4C[5].bike.rider_model = 0xC;
    D_800B4F4C[5].bike.bike_model = 0x13;
    D_800B4F4C[5].bike.pos.vz = -0x1B58;
    D_800B4F4C[5].bike.part[1] = 0x46;
    D_800B4F4C[5].bike.part[2] = 0x46;
    D_800B4F4C[5].bike.front = -0x50;
    D_800B4F4C[5].bike.rear = 0x4B;
    D_800B4F4C[5].bike.front_r = 0x84;
    D_800B4F4C[5].bike.rear_r = 0xF0;
    D_800B4F4C[5].bike.x78 = 0x100;
    D_800B4F4C[5].bike.x80 = 0x1000;
    D_800B4F4C[5].bike.x7C = 0xFA;
    D_800B4F4C[5].bike.x84 = 0x1E;
    D_800B4F4C[5].bike.hp = 0x270F;
    D_800B4F4C[5].bike.x8C = 0x270F;
    D_800B4F4C[5].bike.nparts = 1;
    D_800B4F4C[5].bike.pos.vx = 0x3E800;
    D_800B4F4C[5].bike.z_max = 0x5DC00;
    D_800B4F4C[5].bike.x38 = 0x1F400;
    D_800B4F4C[5].bike.part[0] = 0;
    D_800B4F4C[5].bike.xA0 = 1;
    D_800B4F4C[5].frames[0] = highway_frame_new(0x61, 0, 0, 1, &D_8010EB90, 0, 0, 0, 0, 0, 0);
    D_800B4F4C[5].frames[1] = highway_frame_new(0x61, 0, 0, 1, D_800B4F4C[5].frames[0], 0, 0, 0, 0, 0, 0);
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_init_fixed_riders);
#endif

/* Puts a soldier's bike on the road: its frames and where it starts. */
#ifdef NON_MATCHING
/* Scheduling only: the order of the field stores and of the D_800BDFB8 increment; the game
 * hoists a3 = 1. */
void highway_init_soldier(int i)
{
    Bike *r = &D_800B4F4C[i].bike;

    r->z_max = 0xBB800;
    r->x38 = 0x1F400;
    r->part[1] = 0x46;
    r->part[2] = 0x46;
    r->xA0 = 1;
    r->rear = 0x64;
    r->front_r = 0x24;
    r->rear_r = 0x24;
    r->x78 = 0x400;
    r->x80 = 0x400;
    r->x7C = 0x1C2;
    r->type = 2;
    r->nframes = 3;
    r->nparts = 2;
    r->part[0] = 0;
    r->front = 0x14;
    r->x84 = 0x14;
    D_800BDFB8++;
    r->state = 5;
    D_800B4F4C[i].frames[0] = highway_frame_new(0x61, 0, 0, 1, &D_8010EB90, 0, 0, 0, 0, 0, 0);
    D_800B4F4C[i].frames[1] = highway_frame_new(0x9B, 0, 0, 1, D_800B4F4C[i].frames[0], 0, 0, 0x78, 0, 0, 0);
    D_800B4F4C[i].frames[2] = highway_frame_new(0x94, 0, 0, 1, D_800B4F4C[i].frames[0], 0, 0, 0, 0, 0, 0);
    switch (i) {
    case 2:
        r->pos.vx = -0xC800;
        r->pos.vz = -0xBB800;
        break;
    case 3:
        r->pos.vx = 0;
        r->pos.vz = -0xBB800;
        break;
    case 4:
        r->pos.vx = 0xC800;
        r->pos.vz = -0xBB800;
        break;
    }
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_init_soldier);
#endif

void highway_rider_stub(void)
{
}

/* Keeps every rider on the road. */
void highway_keep_all_on_road(void)
{
    highway_keep_on_road(0);
    highway_keep_on_road(1);
    if (D_800B4F4C[2].bike.state != 5)
        highway_keep_on_road(2);
    if (D_800B4F4C[3].bike.state != 5)
        highway_keep_on_road(3);
    if (D_800B4F4C[4].bike.state != 5)
        highway_keep_on_road(4);
    if (D_800B4F4C[5].bike.state != 5)
        highway_keep_on_road(5);
}

/* Keeps a rider between the road's edges (the truck, 5, keeps further in), and Cloud within his stretch of it. */
void highway_keep_on_road(int i)
{
    Bike *r = &D_800B4F4C[i].bike;
    int step = (((r->pos.vz >> 8) + D_8011668C) >> 8) % 80;
    int edge = ((D_800BE558[step].width >> 1) - 0x60) << 8;
    VECTOR unused[2];

    if (i != 5) {
        if (r->pos.vx > edge)
            r->pos.vx = edge;
        if (r->pos.vx < -edge)
            r->pos.vx = -edge;
    } else {
        if (r->pos.vx > edge - 0x7800)
            r->pos.vx = edge - 0x7800;
        if (r->pos.vx < -edge + 0x7800)
            r->pos.vx = -edge + 0x7800;
    }
    if (i == 0) {
        if (r->pos.vz > r->z_max)
            r->pos.vz = r->z_max;
        if (r->pos.vz < r->x38)
            r->pos.vz = r->x38;
    }
}

void highway_spawn_soldiers(void)
{
    if (!D_800BD594 && !D_800BE2EC) {
        if (rand() % 60 == 0 && D_800B4F4C[2].bike.state == 5)
            highway_spawn_soldier(2);
        if (rand() % 60 == 0 && D_800B4F4C[3].bike.state == 5)
            highway_spawn_soldier(3);
        if (rand() % 60 == 0 && D_800B4F4C[4].bike.state == 5)
            highway_spawn_soldier(4);
    }
    if (D_800BE2EC)
        D_800BE2EC--;
}

/* Sends a soldier's bike into slot i, with one of three riders. */
void highway_spawn_soldier(int i)
{
    Bike *r;
    int kind, anim;

    highway_clear_rider(i);
    highway_init_soldier(i);
    r = &D_800B4F4C[i].bike;
    kind = rand() % 3;
    switch (kind) {
    case 0:
        anim = i + 1;
        r->hp = 5;
        r->x8C = 5;
        break;
    case 1:
        anim = i + 7;
        r->hp = 0x3C;
        r->x8C = 0x3C;
        break;
    case 2:
        anim = i + 12;
        r->hp = 0x1E;
        r->x8C = 0x1E;
        break;
    default:
        anim = i + 1;
        r->hp = 0x28;
        r->x8C = 0x28;
        break;
    }
    r = &D_800B4F4C[i].bike;
    r->bike_model = i + 4;
    r->state = 0;
    r->rider_model = anim;
    r->ai = kind + 10;
}

/* Takes a soldier's bike off the road: hides its animations and frees its frames. */
inline void highway_remove_rider(int i)
{
    Bike *r = &D_800B4F4C[i].bike;
    int n;

    D_800BD658[r->rider_model].on = 0;
    D_800BD658[r->bike_model].on = 0;
    r->state = 5;
    for (n = 0; n < r->nframes; n++)
        highway_frame_free(D_800B4F4C[i].frames[n]);
}

void highway_move_riders(void)
{
    if (D_800B4F4C[0].bike.state != 5)
        highway_move_rider(0);
    if (D_800B4F4C[1].bike.state != 5)
        highway_move_rider(1);
    if (D_800B4F4C[5].bike.state != 5)
        highway_move_rider(5);
    if (D_800B4F4C[2].bike.state != 5)
        highway_move_soldier(2);
    if (D_800B4F4C[3].bike.state != 5)
        highway_move_soldier(3);
    if (D_800B4F4C[4].bike.state != 5)
        highway_move_soldier(4);
}

/* Moves Cloud (type 0: steered by D_800BDFB0's direction, or along a path while path_on is set), the truck (1) and the
 * soldier in the back (10): their drift, swerves and the speed they lose each frame. */
#ifdef NON_MATCHING
/* One delay slot: GCC fills the bgez of the abs() into radius with the store; the game leaves a nop. */
void highway_move_rider(int i)
{
    Bike *r = &D_800B4F4C[i].bike;
    int ang;
    VECTOR v, w;

    switch (r->type) {
    case 0:
        if (!r->path_on) {
            if (!r->x104) {
                if (D_800BDFC4 == 1)
                    r->x104 = 0x1D;
                if (D_800BDFC4 == 2)
                    r->x104 = -0x1D;
            }
            if (D_800BDFB0 == 1)
                ang = 0xA00;
            if (D_800BDFB0 == 2)
                ang = 0xC00;
            if (D_800BDFB0 == 3)
                ang = 0xE00;
            if (D_800BDFB0 == 4)
                ang = 0x800;
            if (D_800BDFB0 == 6)
                ang = 0;
            if (D_800BDFB0 == 7)
                ang = 0x600;
            if (D_800BDFB0 == 8)
                ang = 0x400;
            if (D_800BDFB0 == 9)
                ang = 0x200;
            if (r->state == 6) {
                D_800BC4D8 = 1;
                if (r->pos.vx > 0xB400) {
                    D_800BDFB0 = 4;
                    ang = 0x800;
                }
                if (r->pos.vx < 0xA500) {
                    D_800BDFB0 = 6;
                    ang = 0;
                }
            }
            if (D_800BDFB0) {
                r->vx += rcos(ang) * 1624 >> 12;
                r->vz += rsin(ang) * 1624 >> 11;
            }
            r->vx = r->vx * 8 / 10;
            r->vz = r->vz * 8 / 10;
            r->pos.vx += r->vx;
            r->pos.vz += r->vz;
            if (!D_800BDFB0) {
                if (r->x10C >= 0x400)
                    r->x10C -= 0xC00;
                if (r->x10C < -0x3FF)
                    r->x10C += 0xC00;
                if (r->x10C > -0x400 && r->x10C < 0x400)
                    r->x10C = 0;
            } else {
                r->x10C += rcos(ang) >> 1;
                if (r->x10C > 0x4400)
                    r->x10C = 0x4400;
                if (r->x10C < -0x4400)
                    r->x10C = -0x4400;
                r->radius = r->x10C >> 9;
                if (r->radius < 0)
                    r->radius = -r->radius;
            }
        } else {
            highway_path_point(r->path_pos, r->path, &v, 1);
            highway_path_point(r->path_pos + 0x80000, r->path, &w, 1);
            if (r->x44 < v.vx)
                r->x110 += r->x7C * 2;
            if (r->x44 > v.vx)
                r->x110 -= r->x7C * 2;
            if (r->x110 > 0x200)
                r->x110 = 0x200;
            if (r->x110 < -0x200)
                r->x110 = -0x200;
            r->pos.vx = (v.vx << 7) + r->x3C;
            r->pos.vz = (v.vz << 7) + r->x40;
            r->x44 = v.vx;
            r->path_pos += 0x10000;
            if (r->path_end < r->path_pos) {
                r->path_on = 0;
                r->state = 0;
                D_800B4F4C[0].bike.x108 = 0x49;
                D_800B4F4C[1].bike.heading = 0x31;
                D_800B4F4C[0].bike.nparts--;
            }
        }
        break;
    case 1:
        if (!r->path_on) {
            if (r->heading)
                r->x7C = 0;
            if (r->pos.vz > r->z_max) {
                r->xF4 = 1;
                r->xF8 = 0xC00;
            }
            if (r->pos.vz < r->x38) {
                r->xF4 = 1;
                r->xF8 = 0x400;
            }
            if (!r->xF4) {
                if (rand() % 4096 < r->x78) {
                    r->xF4 = 10;
                    r->xF8 = rand() % 4096;
                    r->vx = r->vx * 8 / 10;
                    r->vz = r->vz * 8 / 10;
                }
            } else {
                r->xF4--;
                r->vx += rcos(r->xF8) * r->x7C >> 12;
                r->vz += rsin(r->xF8) * r->x7C >> 11;
                r->pos.vx += r->vx;
                r->pos.vz += r->vz;
                r->x3C = r->pos.vx;
                r->x40 = r->pos.vz;
                r->vx = r->vx * 8 / 10;
                r->vz = r->vz * 8 / 10;
            }
        } else if (r->path_on == 1) {
            highway_path_point(r->path_pos, r->path, &v, 1);
            r->pos.vx = (v.vx << 7) + r->x3C;
            r->pos.vz = (v.vz << 7) + r->x40;
            r->path_pos += 0x10000;
            if (r->path_end < r->path_pos)
                r->path_on = 2;
        }
        break;
    case 10:
        if (!r->path_on) {
            if (rand() % 30 == 0) {
                if (!r->x10C)
                    highway_play_sound(0x12D, 2, 0);
                r->x10C = 3;
            }
            if (r->pos.vz > r->z_max) {
                r->xF8 = 1;
                r->heading = 0xC00;
            }
            if (r->pos.vz < r->x38) {
                r->xF8 = 1;
                r->heading = 0x400;
            }
            if (!r->xF8) {
                if (rand() % 4096 < r->x78) {
                    if (rand() % 4096 < r->x80)
                        r->heading = highway_rider_angle(&D_800B4F4C[i], &D_800B4F4C[1]);
                    else
                        r->heading = rand() % 4096;
                    r->xF8 = rand() % (r->x84 + 1);
                }
            } else {
                r->xF8--;
                r->vx += rcos(r->heading) * r->x7C >> 12;
                r->vz += rsin(r->heading) * r->x7C >> 11;
            }
            r->vx = r->vx * 8 / 10;
            r->vz = r->vz * 8 / 10;
            r->pos.vx += r->vx;
            r->pos.vz += r->vz;
            r->x3C = r->pos.vx;
            r->x40 = r->pos.vz;
        } else {
            if (r->path_on == 1) {
                highway_path_point(r->path_pos, r->path, &v, 1);
                r->pos.vx = (v.vx << 7) + r->x3C;
                r->pos.vz = (v.vz << 7) + r->x40;
                r->path_pos += 0x10000;
                if (r->path_end < r->path_pos)
                    r->path_on = 2;
            }
            if (r->x12C) {
                r->xC0 = 0;
                r->xBC = 1;
                r->xC4 = (r->x12C >> 2) % 11;
            }
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_move_rider);
#endif

void highway_move_soldier(int i)
{
    Bike *r = &D_800B4F4C[i].bike;
    char unused[0x48];

    if (r->state == 5)
        return;
    highway_drive_soldier(i);
    if (r->pos.vz > r->z_max)
        r->pos.vz = r->z_max;
}

void highway_place_riders(void)
{
    if (D_800B4F4C[0].bike.state != 5)
        highway_place_rider(0);
    if (D_800B4F4C[1].bike.state != 5)
        highway_place_rider(1);
    if (D_800B4F4C[2].bike.state != 5)
        highway_place_rider(2);
    if (D_800B4F4C[3].bike.state != 5)
        highway_place_rider(3);
    if (D_800B4F4C[4].bike.state != 5)
        highway_place_rider(4);
    if (D_800B4F4C[5].bike.state != 5)
        highway_place_rider(5);
}

/* Places a rider's root frame on the road for this frame and picks its animations: Cloud's lean and sword swing
 * (which hits the soldiers beside him), the truck's sway, a soldier's lean and its crash. */
#ifdef NON_MATCHING
/* Register choice for the constant 1 in two store groups, and one extra li in a delay slot. */
void highway_place_rider(int i)
{
    Bike *r = &D_800B4F4C[i].bike;
    Frame *f = D_800B4F4C[i].frames[0];
    MATRIX *m = &f->m;
    VECTOR pos, pos2;
    SVECTOR rot, rot2;
    unsigned char hit[8];
    VECTOR unused;
    unsigned char any, swing, h;
    short d;
    int j, ang, a1;
    Thing *e;

    switch (r->type) {
    case 0:
        r->xB0 = 0;
        highway_road_point_rot((r->pos.vz >> 8) + D_800BD818, r->pos.vx >> 8, &pos, &rot);
        if (D_800BD580 & 0x2000) {
            if (r->x110 < 0x200)
                r->x110 += r->x7C;
        }
        if (D_800BD580 & 0x8000) {
            if (r->x110 > -0x200)
                r->x110 -= r->x7C;
        }
        if (!D_800BDFB0) {
            if (r->x110 > r->x80)
                r->x110 -= r->x80;
            if (r->x110 < -r->x80)
                r->x110 += r->x80;
            if (r->x110 > -r->x80 && r->x110 < r->x80)
                r->x110 = 0;
        }
        if (r->x104) {
            r->xB0 = 1;
            if (r->x104 > 0) {
                r->x104--;
                r->xB4 = 2;
                r->xB8 = 0x1D - r->x104;
            }
            if (r->x104 < 0) {
                r->x104++;
                r->xB4 = 3;
                r->xB8 = r->x104 + 0x1D;
            }
            swing = r->x104 >= -22 && r->x104 < -10;
            if (r->x104 >= 11 && r->x104 < 20)
                swing = 1;
            if (swing) {
                any = 0;
                hit[2] = 0;
                hit[3] = 0;
                hit[4] = 0;
                highway_set_volume(D_80110BC4, 2);
                highway_play_sound(5, 2, 0xE);
                highway_sound_b0(0, 2);
                for (j = 2; j < 6; j++) {
                    if ((unsigned int)D_800B4F4C[j].bike.state < 2
                        && highway_rider_distance(&D_800B4F4C[0], &D_800B4F4C[j]) < 0xA0) {
                        ang = highway_rider_angle(&D_800B4F4C[0], &D_800B4F4C[j]);
                        if (ang < 0)
                            ang += 0x1000;
                        h = 0;
                        if (r->x104 < 0) {
                            h = ang >= 1 && ang < 0x384;
                            if (ang >= 0xE71 && ang < 0x1000)
                                h = 1;
                        }
                        if (r->x104 > 0) {
                            if (ang >= 0x47D && ang < 0x990)
                                h = 1;
                        }
                        if (h == 1) {
                            hit[j] = 1;
                            any = 1;
                        }
                    }
                }
                if (any == 1) {
                    highway_set_volume(D_80110BC4, 2);
                    highway_play_sound(0x12, 2, -0x1E);
                    highway_sound_b0(0, 2);
                    for (j = 2; j < 5; j++) {
                        if (hit[j] == 1)
                            highway_hit_rider(j, 0x32);
                    }
                }
            }
        }
        if (!r->x108) {
            d = r->x110;
            if (rand() % 10 == 0)
                r->xF4 = 1;
            if (rand() % 30 == 0) {
                if (!r->xF0)
                    highway_play_sound(0x12D, 2, 0);
                r->xF0 = 3;
            }
        } else {
            if (r->x108 >= 2)
                r->x108--;
            d = 0;
            r->xB0 = 1;
            r->xBC = 1;
            r->xB4 = 6;
            r->xC0 = 1;
            r->xB8 = 0x49 - r->x108;
            r->xC4 = 0x49 - r->x108;
        }
        rot.vz += d;
        if (d > -0x32 && d < 0x32)
            d = 0;
        a1 = (d >> 4) + (r->x10C >> 10) + 0x12;
        if (a1 < 1)
            a1 = 1;
        else if (a1 > 0x24)
            a1 = 0x24;
        if (d && !r->xBC && !r->x104) {
            r->xB0 = 1;
            r->xB4 = 1;
            r->xB8 = a1;
        }
        m->t[0] = pos.vx;
        m->t[1] = pos.vy;
        m->t[2] = pos.vz;
        RotMatrixYXZ(&rot, m);
        break;
    case 1:
        highway_road_point_rot((r->pos.vz >> 8) + D_800BD818, r->pos.vx >> 8, &pos, &rot);
        highway_road_point_rot((r->pos.vz >> 8) + D_800BD818 + 0xFA0, r->pos.vx >> 8, &pos2, &rot2);
        if (r->state == 1) {
            if (--r->x100 == -1)
                r->state = 0;
            r->xC0 = 3;
            r->xBC = 1;
            r->xC4 = 0x12 - r->x100;
        }
        if (r->heading >= 2) {
            if (r->heading >= 3)
                r->heading--;
            r->xBC = 1;
            r->xB0 = 1;
            r->xB4 = 1;
            r->xC0 = 2;
            r->xC4 = 0x31 - r->heading;
            r->xB8 = 0x31 - r->heading;
        }
        d = rot2.vy - rot.vy;
        if (d > 0x800)
            d -= 0x1000;
        if (d < -0x800)
            d += 0x1000;
        if (d < -10 || d > 10) {
            a1 = -(d >> 4) + 10;
            if (a1 < 1)
                a1 = 1;
            if (a1 > 0x12)
                a1 = 0x12;
            r->xBC = 1;
            r->xC0 = 1;
            r->xC4 = a1;
        }
        rot.vz += -d >> 1;
        m->t[0] = pos.vx;
        m->t[1] = pos.vy;
        m->t[2] = pos.vz;
        RotMatrixYXZ(&rot, m);
        break;
    case 10:
        highway_road_point_rot((r->pos.vz >> 8) + D_800BD818, r->pos.vx >> 8, &pos, &rot);
        f->m.t[0] = pos.vx;
        f->m.t[1] = pos.vy;
        f->m.t[2] = pos.vz;
        RotMatrixYXZ(&rot, m);
        break;
    case 2:
        highway_road_point_rot((r->pos.vz >> 8) + D_800BD818 + 0xFA0, r->pos.vx >> 8, &pos2, &rot2);
        highway_road_point_rot((r->pos.vz >> 8) + D_800BD818, r->pos.vx >> 8, &pos, &rot);
        d = (rot2.vy - rot.vy) >> 1;
        if (d > 0x800)
            d -= 0x1000;
        if (d < -0x800)
            d += 0x1000;
        a1 = (d >> 4) + 9;
        rot.vz += d << 1;
        if (a1 < 1)
            a1 = 1;
        if (a1 > 0x11)
            a1 = 0x11;
        if (d && !r->xB0) {
            r->xB0 = 1;
            r->xB4 = 1;
            r->xB8 = a1;
        }
        if (r->state == 1) {
            if (--r->x104 == -1)
                r->state = 0;
            r->xB4 = 2;
            r->xB0 = 1;
            r->xBC = 1;
            r->xC0 = 1;
            r->xB8 = 0x1C - r->x104;
            r->xC4 = 0x1C - r->x104;
        }
        if (r->state == 2) {
            if (--r->x108 == -1)
                highway_remove_rider(i);
            r->xB0 = 1;
            r->xBC = 1;
            r->xB4 = r->x118 + 3;
            r->xB8 = 0x3B - r->x108;
            r->xC4 = 0x3B - r->x108;
            r->xC0 = r->x118 + 2;
            r->x110 += 0x200;
            r->pos.vz -= r->x110;
            e = highway_spawn_thing_at(pos.vx, pos.vy, pos.vz, 2, 0xAA);
            e->rot.vx = rot.vx;
            e->rot.vy = rot.vy;
            e->rot.vz = rot.vz;
        }
        m->t[0] = pos.vx;
        m->t[1] = pos.vy;
        m->t[2] = pos.vz;
        RotMatrixYXZ(&rot, m);
        break;
    }
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_place_rider);
#endif

void highway_animate_riders(void)
{
    if (D_800B4F4C[0].bike.state != 5)
        highway_animate_rider(0);
    if (D_800B4F4C[1].bike.state != 5)
        highway_animate_rider(1);
    if (D_800B4F4C[2].bike.state != 5)
        highway_animate_rider(2);
    if (D_800B4F4C[3].bike.state != 5)
        highway_animate_rider(3);
    if (D_800B4F4C[4].bike.state != 5)
        highway_animate_rider(4);
    if (D_800B4F4C[5].bike.state != 5)
        highway_animate_rider(5);
}

/* Poses a rider for this frame: its parts' models, then its two animations (19 is none), each stepped on or
 * restarted, and whether each is on screen. */
void highway_animate_rider(int i)
{
    Bike *r = &D_800B4F4C[i].bike;
    int a1 = r->bike_model, a0 = r->rider_model;
    FigureState *an1 = &D_800BC6B0[a1], *an0 = &D_800BC6B0[a0];
    int k;
    VECTOR unused[2];

    for (k = 1; k < r->nparts; k++)
        highway_draw_frame(D_80110A90, D_800B4F4C[i].frames[k], r->part[k] + D_800BD558, 0);
    if (a0 != 0x13) {
        if (!r->xBC) {
            an0->frame++;
            an0->anim = 0;
            an0->frame = an0->frame % an0->frames[0];
        } else {
            an0->anim = r->xC0;
            an0->frame = r->xC4;
            r->xBC = 0;
        }
        highway_view_matrix(D_80110A90, D_800B4F4C[i].frames[0], &D_800BC6B0[a0].m);
        if (highway_near_view(D_800BC6B0[a0].m.t, D_800BD81C[129]->x1C))
            D_800BD658[a0].on = 1;
        else
            D_800BD658[a0].on = 0;
    }
    if (a1 != 0x13) {
        if (!r->xB0) {
            an1->frame++;
            an1->anim = 0;
            an1->frame = an1->frame % an1->frames[0];
        } else {
            an1->anim = r->xB4;
            an1->frame = r->xB8;
            r->xB0 = 0;
        }
        highway_view_matrix(D_80110A90, D_800B4F4C[i].frames[0], &D_800BC6B0[a1].m);
        /* the game tests the rider's figure's place here too */
        if (highway_near_view(D_800BC6B0[a0].m.t, D_800BD81C[129]->x1C))
            D_800BD658[a1].on = 1;
        else
            D_800BD658[a1].on = 0;
    }
}

void highway_swap_rider_models(void)
{
    if (D_800B4F4C[0].bike.state != 5)
        highway_swap_rider_model(0);
    if (D_800B4F4C[1].bike.state != 5)
        highway_swap_rider_model(1);
    if (D_800B4F4C[2].bike.state != 5)
        highway_swap_rider_model(2);
    if (D_800B4F4C[3].bike.state != 5)
        highway_swap_rider_model(3);
    if (D_800B4F4C[4].bike.state != 5)
        highway_swap_rider_model(4);
    if (D_800B4F4C[5].bike.state != 5)
        highway_swap_rider_model(5);
}

/* Steps the riders' swapped-in models: Cloud's sword swing (frame 3) and the turn of his wheel (frame 2), a soldier's
 * swing, the truck rider's. */
void highway_swap_rider_model(int i)
{
    Bike *r = &D_800B4F4C[i].bike;

    switch (r->type) {
    case 0:
        if (r->xF0) {
            r->xF0--;
            D_800B4F4C[i].frames[3]->model = D_800BD81C[D_800BC4E0[0] - r->xF0];
            highway_draw_frame(D_80110A90, D_800B4F4C[i].frames[3], r->part[3] + D_800BD558, 0);
        }
        if (r->xF4) {
            r->xF4 = 0;
            r->xF8 = (r->xF8 + 1) % 3;
            D_800B4F4C[i].frames[2]->model = D_800BD81C[D_800BC4E0[1] + r->xF8];
            highway_draw_frame(D_80110A90, D_800B4F4C[i].frames[2], r->part[2] + D_800BD558, 0);
        }
        break;
    case 2:
        if (r->xF0) {
            r->xF0--;
            D_800B4F4C[i].frames[2]->model = D_800BD81C[D_800BC4E0[2] - r->xF0];
            highway_draw_frame(D_80110A90, D_800B4F4C[i].frames[2], r->part[2] + D_800BD558, 0);
        }
        break;
    case 10:
        if (r->x10C) {
            r->x10C--;
            D_800B4F4C[i].frames[1]->model = D_800BD81C[D_800BC4E0[3] - r->x10C];
            highway_draw_frame(D_80110A90, D_800B4F4C[i].frames[1], r->part[2] + D_800BD558, 0);
        }
        break;
    }
}

/* The angle from a to b on the road plane. */
int highway_rider_angle(Rider *a, Rider *b)
{
    int bx = b->bike.pos.vx;

    return ratan2(b->bike.pos.vz - a->bike.pos.vz, bx - a->bike.pos.vx);
}

int highway_rider_distance(Rider *a, Rider *b)
{
    Frame *fa = a->frames[0], *fb = b->frames[0];
    int dx = fb->m.t[0] - fa->m.t[0], dy = fb->m.t[1] - fa->m.t[1], dz = fb->m.t[2] - fa->m.t[2];

    return SquareRoot0(dx * dx + dy * dy + dz * dz);
}

/* The world positions of a racer's front and rear. */
void highway_bike_ends(short n, SVECTOR *front, SVECTOR *rear, short *len)
{
    SVECTOR f, r;
    int fz = D_800B4F4C[n].bike.front, rz = D_800B4F4C[n].bike.rear;
    int unset_a, unset_b; /* never given values: the game reads garbage registers here */

    f.vx = 0;
    f.vy = 0;
    r.vx = 0;
    r.vy = 0;
    f.vz = fz;
    r.vz = rz;
    *len = abs(unset_a - unset_b) >> 2;
    ApplyMatrixSV(&D_800B4F4C[n].frames[0]->m, &f, front);
    ApplyMatrixSV(&D_800B4F4C[n].frames[0]->m, &r, rear);
    front->vx += D_800B4F4C[n].frames[0]->m.t[0];
    front->vy += D_800B4F4C[n].frames[0]->m.t[1];
    front->vz += D_800B4F4C[n].frames[0]->m.t[2];
    rear->vx += D_800B4F4C[n].frames[0]->m.t[0];
    rear->vy += D_800B4F4C[n].frames[0]->m.t[1];
    rear->vz += D_800B4F4C[n].frames[0]->m.t[2];
}

/* Collisions between every pair of racers still in the race. */
#ifdef NON_MATCHING
/* Register allocation only: s1 and s2 (the second rider and the angle) are swapped. */
void highway_collide_riders(void)
{
    VECTOR normal;
    SVECTOR rot, front_i, rear_i, front_j, rear_j;
    short len_i, len_j;
    short i, j;
    Bike *a, *b;
    int angle, reach, turn, n;

    for (i = 0, n = 6; i < n; i++) {
        a = &D_800B4F4C[i].bike;
        if (a->state == 5)
            continue;
        highway_bike_ends(i, &front_i, &rear_i, &len_i);
        for (j = i + 1; j < n; j++) {
            b = &D_800B4F4C[j].bike;
            if (b->state == 5)
                continue;
            highway_bike_ends(j, &front_j, &rear_j, &len_j);
            highway_road_point_rot(D_800BD818 + b->pos.vz, b->pos.vx, &normal, &rot);
            angle = 0xFFFF;
            HIT(front_i, front_j, front_r, front_r);
            if (angle == 0xFFFF) {
                HIT(rear_i, front_j, rear_r, front_r);
            }
            if (angle == 0xFFFF) {
                HIT(front_i, rear_j, front_r, rear_r);
            }
            if (angle == 0xFFFF) {
                HIT(rear_i, rear_j, rear_r, rear_r);
            }
            if (angle != 0xFFFF)
                highway_bump(i, j, angle);
        }
    }
}
#undef HIT
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_collide_riders);
#endif

/* Racers i and j collide along `angle`: push them apart and hurt whoever was rammed. */
void highway_bump(short i, short j, int angle)
{
    int sound = 0;
    int push_x, push_z, sin, cos;

    if (i == 0)
        sound = j != 1;
    if (i == 1 && j != i)
        sound = 1;
    if (sound == 1) {
        highway_play_sound(0x91, 2, 5);
        highway_set_volume(0x6F, 2);
        highway_sound_b0(0, 2);
    }
    angle %= 0x1000;
    if (angle < 0)
        angle += 0x1000;
    sin = rsin(angle) * 5000;
    push_z = sin >> 11;
    cos = rcos(angle) * 5000;
    push_x = cos >> 12;
    if (i != 0) {
        D_800B4F4C[i].bike.vx = push_x;
        D_800B4F4C[i].bike.vz = push_z;
    } else {
        D_800B4F4C[0].bike.vx = cos >> 13;
        D_800B4F4C[0].bike.vz = sin >> 12;
    }
    D_800B4F4C[j].bike.vx = -push_x;
    D_800B4F4C[j].bike.vz = -push_z;
    if (i >= 2 && j == 5) {
        D_800B4F4C[i].bike.vx = push_x * 2;
        D_800B4F4C[i].bike.vz = push_z * 2;
        D_800B4F4C[5].bike.vx = 0;
        D_800B4F4C[5].bike.vz = 0;
    }
    if (D_800B4F4C[j].bike.state == 2 && D_800B4F4C[i].bike.state != 2 && i >= 2 && j >= 2)
        highway_hit_rider(i, 15);
    if (D_800B4F4C[i].bike.state == 2 && D_800B4F4C[j].bike.state != 2 && i >= 2 && j >= 2)
        highway_hit_rider(j, 15);
    if (i == 1 && j != 0)
        highway_truck_hit(j, angle);
    if (i == 0) {
        if (j != 1)
            highway_damage_gauge(0, 0x40);
        if (D_800B4F4C[j].bike.state != 2 && j != 1)
            highway_hit_rider(j, 1);
    }
    if (j == 5)
        highway_hit_rider(i, 20);
}

/* Racer n takes a hit. */
void highway_hit_rider(int n, int damage)
{
    Rider *r;
    Bike *b = &D_800B4F4C[n].bike;
    int roll, dx;

    switch (b->type) {
    case 0:
        break;
    case 1:
        r = &D_800B4F4C[n];
        dx = D_800B4F4C[1].bike.pos.vx - r->bike.pos.vx;
        ratan2(D_800B4F4C[1].bike.pos.vz - r->bike.pos.vz, dx);
        b->state = 1;
        b->x100 = 0x12;
        break;
    case 2:
        if (b->hp <= 0) {
            D_800BDFB4 += 500;
            highway_play_sound(0x138, 1, 10);
            b->state = 2;
            roll = rand();
            b->x108 = 0x3B;
            b->x110 = 0;
            b->nparts--;
            b->x118 = roll % 3;
            if (D_800BDFB8 != 1 && D_80110A8C == 1)
                highway_start_camera_path(rand() % 2 + 3, 4, 40000);
            D_800BDFB8--;
        } else {
            b->hp -= damage;
            b->state = 1;
            b->x104 = 0x1C;
            if (D_800B4F4C[n].bike.bike_model != 0x13)
                D_800BC6B0[D_800B4F4C[n].bike.bike_model].flash = 0xFF;
        }
        break;
    }
}

/* Racer 1 was rammed at `angle` (the first argument is unused). */
void highway_truck_hit(int unused, int angle)
{
    if (D_800BDFB4)
        D_800BDFB4 -= 50;
    angle += 0x800;
    angle %= 0x1000;
    if (angle >= 0 && angle < 0x400)
        highway_damage_gauge(2, 400);
    if (angle >= 0x400 && angle < 0x800)
        highway_damage_gauge(3, 400);
    if (angle >= 0x800 && angle < 0xC00)
        highway_damage_gauge(1, 400);
    if (angle >= 0xC00 && angle < 0x1000)
        highway_damage_gauge(4, 400);
    D_800BC6B0[D_800B4F4C[1].bike.rider_model].flash = 0xFF;
}

/* Starts racer n along path `path`. */
void highway_start_rider_path(int n, int path)
{
    Bike *b = &D_800B4F4C[n].bike;

    highway_find_path(path);
    b->path_on = 1;
    b->path_pos = 0;
    b->path = D_800BC67C;
    b->path_end = ((D_800BD550 - 1) << 16) - 1;
}

/* Drives enemy racer n: knocked off the road, or by its ai's rule toward the truck (racer 1) or the player. */
#ifdef NON_MATCHING
/* Register allocation (the game keeps the base and n * 0x180 apart), the frames loop, and some
 * evaluation order. */
void highway_drive_soldier(int n)
{
    Bike *b = &D_800B4F4C[n].bike;
    Rider *r;
    int speed = 4000, to_truck, to_player, near, dx, k;

    if (b->pushed) {
        b->heading = 0xC00;
        BIKE_MOVE(b, speed);
        if (b->pos.vz < -5000) {
            D_800BD658[b->rider_model].on = 0;
            D_800BD658[b->bike_model].on = 0;
            b->state = 5;
            for (k = 0; k < b->nframes; k++)
                highway_frame_free(D_800B4F4C[n].frames[k]);
        }
        return;
    }
    switch (b->ai) {
    case 10: /* hangs by the truck, rams the player when close */
        r = &D_800B4F4C[n];
        speed = b->state != 1 ? 1000 : 0;
        dx = D_800B4F4C[1].bike.pos.vx - r->bike.pos.vx;
        to_truck = ratan2(D_800B4F4C[1].bike.pos.vz - r->bike.pos.vz, dx);
        dx = D_800B4F4C[0].bike.pos.vx - r->bike.pos.vx;
        to_player = ratan2(D_800B4F4C[0].bike.pos.vz - r->bike.pos.vz, dx);
        SquareRoot0((D_800B4F4C[1].frames[0]->m.t[0] - r->frames[0]->m.t[0]) *
                        (D_800B4F4C[1].frames[0]->m.t[0] - r->frames[0]->m.t[0]) +
                    (D_800B4F4C[1].frames[0]->m.t[1] - r->frames[0]->m.t[1]) *
                        (D_800B4F4C[1].frames[0]->m.t[1] - r->frames[0]->m.t[1]) +
                    (D_800B4F4C[1].frames[0]->m.t[2] - r->frames[0]->m.t[2]) *
                        (D_800B4F4C[1].frames[0]->m.t[2] - r->frames[0]->m.t[2]));
        near = SquareRoot0((D_800B4F4C[0].frames[0]->m.t[0] - r->frames[0]->m.t[0]) *
                               (D_800B4F4C[0].frames[0]->m.t[0] - r->frames[0]->m.t[0]) +
                           (D_800B4F4C[0].frames[0]->m.t[1] - r->frames[0]->m.t[1]) *
                               (D_800B4F4C[0].frames[0]->m.t[1] - r->frames[0]->m.t[1]) +
                           (D_800B4F4C[0].frames[0]->m.t[2] - r->frames[0]->m.t[2]) *
                               (D_800B4F4C[0].frames[0]->m.t[2] - r->frames[0]->m.t[2]));
        switch (D_800BDFB8) {
        case 1:
            b->heading = to_truck;
        case 2:
            b->heading = to_truck;
        case 3:
            speed = 1000;
            b->heading = to_truck;
        }
        if (near < 500 && D_800B4F4C[0].bike.x104) {
            speed = 1300;
            b->heading = to_player + 0x800;
        }
        break;
    case 11: /* follows the truck */
        speed = b->state != 1 ? 800 : 0;
        r = &D_800B4F4C[n];
        dx = D_800B4F4C[1].bike.pos.vx - r->bike.pos.vx;
        b->heading = ratan2(D_800B4F4C[1].bike.pos.vz - r->bike.pos.vz, dx);
        break;
    case 12: /* follows the truck, now and then veering at the player for a while */
        r = &D_800B4F4C[n];
        speed = b->state != 1 ? 800 : 0;
        dx = D_800B4F4C[1].bike.pos.vx - r->bike.pos.vx;
        to_truck = ratan2(D_800B4F4C[1].bike.pos.vz - r->bike.pos.vz, dx);
        dx = D_800B4F4C[0].bike.pos.vx - r->bike.pos.vx;
        to_player = ratan2(D_800B4F4C[0].bike.pos.vz - r->bike.pos.vz, dx);
        if (!b->x11C && rand() % 100 == 0)
            b->x11C = rand() % 90;
        if (!b->x11C) {
            b->heading = to_truck;
            break;
        }
        b->heading = to_player + 0x200;
        b->x11C--;
        break;
    default:
        goto clamp;
    }
    /* Behind the start line: drive straight up the road. */
    if (b->pos.vz < b->x38) {
        speed = 2000;
        b->heading = 0x400;
    }
    BIKE_MOVE(b, speed);
clamp:
    if (b->pos.vz > b->z_max)
        b->pos.vz = b->z_max;
}
#undef BIKE_MOVE
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_drive_soldier);
#endif

#ifdef NON_MATCHING
/* The game hoists the constant 1 into t3 and steps two pointers in the other order. */
void highway_init_props(void)
{
    int i;
    int n;

    if (!D_800BC4D4) {
        D_800BD57C = D_800B4260[0].props;
        D_8010FD50[0] = D_800B4260[0].keys[0];
        D_8010FD50[1] = D_800B4260[0].keys[1];
        D_8010FD50[2] = D_800B4260[0].keys[2];
        D_8010FD50[3] = D_800B4260[0].keys[3];
        D_8010FD50[4] = D_800B4260[0].keys[4];
        D_8010FD50[5] = D_800B4260[0].keys[5];
        D_8010FD50[6] = D_800B4260[0].keys[6];
        D_8010FD50[7] = D_800B4260[0].keys[7];
        D_8010FD50[8] = D_800B4260[0].keys[8];
        D_8010FD50[9] = D_800B4260[0].keys[9];
    } else {
        D_800BD57C = D_800B4260[1].props;
        D_8010FD50[0] = D_800B4260[1].keys[0];
        D_8010FD50[1] = D_800B4260[1].keys[1];
        D_8010FD50[2] = D_800B4260[1].keys[2];
        D_8010FD50[3] = D_800B4260[1].keys[3];
        D_8010FD50[4] = D_800B4260[1].keys[4];
        D_8010FD50[5] = D_800B4260[1].keys[5];
        D_8010FD50[6] = D_800B4260[1].keys[6];
        D_8010FD50[7] = D_800B4260[1].keys[7];
        D_8010FD50[8] = D_800B4260[1].keys[8];
        D_8010FD50[9] = D_800B4260[1].keys[9];
    }
    D_800BD554 = D_800BD57C[0];
    n = D_800BD554 + 1;
    /* The file: a count, that many frame counts, then the frame tables' 16-bit little-endian offsets. */
    for (i = 0; i < D_800BD554; i++) {
        if (D_800BC4D4 == 0)
            D_800BDB00[i] = D_800B4260[0].props + (D_800BD57C + n)[i * 2] + ((D_800BD57C + n)[i * 2 + 1] << 8);
        if (D_800BC4D4 == 1)
            D_800BDB00[i] = D_800B4260[1].props + (D_800BD57C + n)[i * 2] + ((D_800BD57C + n)[i * 2 + 1] << 8);
    }
    for (i = 0; i < 10; i++) {
        D_8010FD80[i] = 0;
        D_8010FD44[i] = 1;
    }
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_init_props);
#endif

#ifdef NON_MATCHING
/* The game loads the key index twice (lh and lhu); its frame is 8 bytes smaller. */
void highway_step_prop(unsigned char ch, unsigned char *frame, short *dx, short *dy, unsigned short *flags,
                       short *drot)
{
    PropKey *key;
    unsigned char a, b, c;
    unsigned char n;

    if (D_8010FD44[ch]) {
        key = &D_8010FD50[ch][D_8010FD80[ch]];
        D_8010FD80[ch]++;
        D_80116410[ch] = key->table;
        D_800BD644[ch] = key->repeat;
        D_8010FD94[ch] = key->dx;
        D_8011642C[ch] = key->dy;
        D_80110B9C[ch] = key->flags;
        D_8010FBDC[ch] = key->drot;
        D_800C4C38 = key;
        n = D_800BD57C[key->table + 1];
        D_80110BDC[ch] = 0;
        D_8010FD44[ch] = 0;
        D_8010FD30[ch] = n;
    }
    highway_prop_frame(D_80116410[ch], D_80110BDC[ch], &a, &b, &c);
    D_80110BDC[ch]++;
    if (D_80110BDC[ch] == D_8010FD30[ch]) {
        D_80110BDC[ch] = 0;
        if (--D_800BD644[ch] == 0)
            D_8010FD44[ch] = 1;
    }
    *frame = a;
    *dx = D_8010FD94[ch];
    *dy = D_8011642C[ch];
    *flags = D_80110B9C[ch];
    *drot = D_8010FBDC[ch];
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_step_prop);
#endif

/* A prop frame is three bytes; callers pass a third out for the last one, which nothing reads. */
void highway_prop_frame(unsigned char table, unsigned char frame, unsigned char *a, unsigned char *b, unsigned char *c)
{
    unsigned char *e = &D_800BDB00[table][frame * 3];

    *a = e[0];
    *b = e[1];
}

void highway_init_camera(void)
{
    D_80110A8C = 1;
    D_800B41D8.vz = -2000;
    D_800B41D8.vx = 0;
    D_800B41D8.vy = 0;
    D_800B41E8.vx = 0;
    D_800B41E8.vy = 0;
    D_800B41E8.vz = 0;
    D_800B41F8.vx = 0;
    D_800B41F8.vy = 0;
    D_800B41F8.vz = 0;
    D_800B4208.vx = 0;
    D_800B4208.vy = 0;
    D_800B4208.vz = -500;
    D_8010EA2C = 0;
    D_801163A8 = 0;
    D_800BE2F8.vx = 0;
    D_800BE2F8.vy = 0;
    D_800BE2F8.vz = 0;
    D_801163F8 = D_800B42D0;
    D_800BE550 = D_800B42D4;
}

/* Pans and fades the engine sounds by where the racers are around the camera, then places the camera for its mode. */
#ifdef NON_MATCHING
/* The game reloads &D_800B4F4C[0].bike.pos.vz with lui each time where GCC keeps it in s2 (one
 * more saved register), and some scheduling in modes 1 and 4. */
void highway_update_camera(int z_base)
{
    VECTOR unused_a, unused_b;
    SVECTOR unused_c;
    VECTOR dir, unit, unused_d;
    SVECTOR rot, ground, eye_off, look_off;
    VECTOR unused_e;
    VECTOR d[6], n[6];
    int dist[6], vol[6];
    int pan, k, lift, pz;
    Frame *f;
    Bike *b;

    d[0].vx = D_800B4F4C[0].frames[0]->m.t[0] - D_800B41D8.vx;
    d[0].vy = D_800B4F4C[0].frames[0]->m.t[1] - D_800B41D8.vy;
    d[0].vz = D_800B4F4C[0].frames[0]->m.t[2] - D_800B41D8.vz;
    dist[0] = SquareRoot0(d[0].vx * d[0].vx + d[0].vy * d[0].vy + d[0].vz * d[0].vz);
    vol[0] = dist[0] >> 5;
    VectorNormal(&d[0], &n[0]);
    pan = ratan2(n[0].vz, n[0].vx);
    pan = D_8010FD78 + (0x400 - pan);
    if (pan > 0x800)
        pan -= 0x1000;
    if (pan < -0x800)
        pan += 0x1000;
    pan >>= 2;
    pan += 0x40;
    if (pan > 0x7F)
        pan = 0x7F;
    if (pan < 0)
        pan = 0;
    if (vol[0] > 0x5A)
        vol[0] = 0x5A;
    if (vol[0] < 0)
        vol[0] = 0;
    D_80110BC4 = 0x7F - vol[0];
    D_80110BD8 = 0x7F - vol[0];
    highway_set_pan(pan, 2);
    highway_set_pan(pan, 4);
    highway_set_volume(D_80110BC4, 1);
    highway_set_volume(D_80110BC4, 4);
    /* The nearest other racer gets channel 3. */
    D_800BD58C = 0x7FFFFFFF;
    D_800BD588 = 0;
    D_8010FD40 = dist[0];
    for (k = 1; k < 6; k++) {
        f = KSEG0(D_800B4F4C[k].frames[0]); /* null for a racer not out yet: the game reads low RAM */
        b = &D_800B4F4C[k].bike;
        d[k].vx = f->m.t[0] - D_800B41D8.vx;
        d[k].vy = f->m.t[1] - D_800B41D8.vy;
        d[k].vz = f->m.t[2] - D_800B41D8.vz;
        dist[k] = SquareRoot0(d[k].vx * d[k].vx + d[k].vy * d[k].vy + d[k].vz * d[k].vz);
        vol[k] = dist[k] >> 5;
        if (dist[k] < D_8010FD40 && k > 0 && b->state != 5)
            D_8010FD40 = dist[k];
        if (dist[k] < D_800BD58C && k >= 2 && b->state != 5) {
            D_800BD588 = k;
            D_800BD58C = dist[k];
        }
    }
    if (D_800BD588) {
        VectorNormal(&d[D_800BD588], &n[D_800BD588]);
        pan = ratan2(n[D_800BD588].vz, n[D_800BD588].vx);
    pan = D_8010FD78 + (0x400 - pan);
        if (pan > 0x800)
            pan -= 0x1000;
        if (pan < -0x800)
            pan += 0x1000;
        pan >>= 2;
    pan += 0x40;
        if (pan > 0x7F)
            pan = 0x7F;
        if (pan < 0)
            pan = 0;
        if (vol[D_800BD588] > 0x5A)
            vol[D_800BD588] = 0x5A;
        if (vol[D_800BD588] < 0)
            vol[D_800BD588] = 0;
        D_80110BD0 = 0x7F - vol[D_800BD588];
        highway_set_pan(pan, 3);
        highway_set_volume(D_80110BD0, 3);
    } else
        highway_set_volume(0, 3);

    /* Lift the camera when a racer is close (D_8010FD40 is the nearest one's distance) and late in the course. */
    D_8010EA2C = 0;
    if (D_8010FD40 < 800)
        D_8010EA2C = (800 - D_8010FD40) >> 2;
    pz = D_800B4F4C[0].bike.pos.vz;
    lift = (pz - 350000) >> 11;
    if (lift < 0)
        lift = 0;
    if (D_8010EA2C < lift)
        D_8010EA2C = lift;

    switch (D_80110A8C) {
    case 1: /* behind the player */
        eye_off.vz = pz >> 9;
        eye_off.vy = 85;
        look_off.vy = 120;
        look_off.vz = pz >> 8;
        look_off.vx = D_800B4F4C[0].bike.pos.vx >> 8;
        eye_off.vx = -(D_800B4F4C[0].bike.pos.vx >> 9) * 4 / 3;
        highway_road_point_rot(z_base + eye_off.vz, eye_off.vx, &D_800B41D8, &ground);
        highway_road_point(z_base + look_off.vz, look_off.vx, &D_800B41E8);
        D_800B4218 = 150;
        D_800B421C = 150;
        D_800BD558 = 0;
        D_800B41D8.vy -= eye_off.vy;
        D_800B41E8.vy = lift * 2 + (D_800B41E8.vy - look_off.vy);
        break;
    case 4: /* swinging along the camera path */
        highway_path_point(D_800BE538, D_800BDFAC, &D_800BE540, 1);
        eye_off.vy = 85;
        look_off.vy = 120;
        look_off.vx = D_800B4F4C[0].bike.pos.vx >> 8;
        look_off.vz = D_800B4F4C[0].bike.pos.vz >> 8;
        eye_off.vz = (D_800B4F4C[0].bike.pos.vz >> 9) + D_800BE540.vz;
        eye_off.vx = -(D_800B4F4C[0].bike.pos.vx >> 9) * 4 / 3 + D_800BE540.vx;
        highway_road_point_rot(z_base + eye_off.vz, eye_off.vx, &D_800B41D8, &ground);
        highway_road_point(z_base + look_off.vz, look_off.vx, &D_800B41E8);
        D_800B4218 = 200;
        D_800B421C = 200;
        D_800B41D8.vy -= eye_off.vy;
        D_800B41D8.vy -= D_800BE540.vy;
        D_800B41E8.vy = lift * 2 + (D_800B41E8.vy - look_off.vy);
        if (D_800BE540.vx > 350)
            D_800B421C = 40;
        if (D_800BE540.vx < -350)
            D_800B4218 = 40;
        D_800BE538 += D_800BDFC8;
        if (D_800BE538 > D_800BDFC0)
            D_80110A8C = 1;
        if (D_800BC4D4 == 1) {
            if (D_800B41D8.vy >= -2899 && D_8011639C == 2)
                D_800BD558 = 300;
            else
                D_800BD558 = 0;
        }
        break;
    case 5: /* a fixed point */
        highway_road_point_rot(D_801163A8, D_800BE2F8.vx + D_800B4208.vx, &D_800B41D8, &ground);
        highway_road_point(z_base + (D_800B4F4C[0].bike.pos.vz >> 8), D_800B4F4C[0].bike.pos.vx >> 8, &D_800B41E8);
        D_800B4218 = 200;
        D_800B421C = 200;
        D_800B41D8.vy -= D_800BE2F8.vy;
        D_800BE540.vx = D_800B4208.vx;
        D_800BE540.vy = D_800B4208.vy;
        D_800BE540.vz = D_800B4208.vz;
        if (D_800BE540.vx > 350)
            D_800B421C = 40;
        if (D_800BE540.vx < -350)
            D_800B4218 = 40;
        break;
    case 0: /* the scripted view: eye from D_800B4208, target from D_800B41F8 */
        highway_road_point_rot(z_base + D_800B4208.vz, D_800B4208.vx, &D_800B41D8, &ground);
        highway_road_point(z_base + D_800B41F8.vz + (D_800B4F4C[0].bike.pos.vz >> 8), D_800B41F8.vx, &D_800B41E8);
        D_800B4218 = 200;
        D_800B421C = 200;
        D_800B41E8.vy += D_800B41F8.vy;
        D_800B41D8.vy += D_800B4208.vy;
        D_800BE540.vx = D_800B4208.vx;
        D_800BE540.vy = D_800B4208.vy;
        D_800BE540.vz = D_800B4208.vz;
        if (D_800B4208.vx > 350)
            D_800B421C = 40;
        if (D_800B4208.vx < -350)
            D_800B4218 = 40;
        break;
    }

    dir.vx = D_800B41E8.vx - D_800B41D8.vx;
    dir.vy = D_800B41E8.vy - (D_800B41D8.vy - D_8010EA2C);
    dir.vz = D_800B41E8.vz - D_800B41D8.vz;
    D_800B41C8.vx = D_800B41E8.vx;
    D_800B41C8.vy = D_800B41E8.vy - (D_8010EA2C >> 1);
    D_800B41C8.vz = D_800B41E8.vz;
    if (SquareRoot0(dir.vx * dir.vx + dir.vy * dir.vy + dir.vz * dir.vz) > 0x1000) {
        VectorNormal(&dir, &unit);
        rot.vx = ratan2(unit.vy, SquareRoot0(unit.vx * unit.vx + unit.vz * unit.vz));
        rot.vy = ratan2(unit.vz, unit.vx) - 0x400;
    } else {
        rot.vx = ratan2(dir.vy, SquareRoot0(dir.vx * dir.vx + dir.vz * dir.vz));
        rot.vy = ratan2(dir.vz, dir.vx) - 0x400;
    }
    rot.vz = 0;
    D_801163E8->trans.t[0] = dir.vx;
    D_801163E8->trans.t[1] = dir.vy;
    D_801163E8->trans.t[2] = dir.vz;
    D_801163E8->roll_angles.vz = -ground.vz;
    D_8010EAE0 = D_801163E8->roll_angles.vz != 0;
    RotMatrix(&D_801163E8->roll_angles, &D_801163E8->roll);
    RotMatrix(&rot, &D_801163E8->rot);
    CompMatrix(&D_801163E8->rot, &D_801163E8->trans, &D_801163E8->view);
    CompMatrix(&D_801163E8->rot, &D_801163E8->trans, &D_801163E8->unrolled);
    CompMatrix(&D_801163E8->roll, &D_801163E8->view, &D_801163E8->view);
    D_8010FD78 = rot.vy;
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_update_camera);
#endif

void highway_camera_stub(void)
{
}

/* Puts the camera in `mode` on path `path`, moving `speed` along it a frame (16.16 points). */
void highway_start_camera_path(int path, int mode, int speed)
{
    D_80110A8C = mode;
    highway_find_path(path);
    D_800BE538 = 0;
    D_800BDFC8 = speed;
    D_800BDFAC = D_800BC67C;
    D_800BDFC0 = ((D_800BD550 - 1) << 16) - 1;
}

/* Looks up path i: D_800BD550 its point count, D_800BC67C its points. */
void highway_find_path(unsigned char i)
{
    D_800BD550 = D_800B42D0[i];
    D_800BC67C = (SVECTOR *)(D_800B42D4[i] + D_800B42D8);
}

/* pos: the segment in the high half, how far along it (0x10000 is the end) in the low half. */
void highway_path_point(unsigned int pos, SVECTOR *path, VECTOR *out, unsigned char mirror)
{
    int i = pos >> 16;
    unsigned int t = pos & 0xFFFF;
    SVECTOR a, b;
    VECTOR d;

    a.vx = path[i].vx;
    a.vy = path[i].vy;
    a.vz = path[i].vz;
    b.vx = path[i + 1].vx;
    b.vy = path[i + 1].vy;
    b.vz = path[i + 1].vz;
    d.vx = b.vx - a.vx;
    d.vy = b.vy - a.vy;
    d.vz = b.vz - a.vz;
    d.vx = t * d.vx;
    d.vy = t * d.vy;
    d.vz = t * d.vz;
    d.vx >>= 16;
    d.vy >>= 16;
    d.vz >>= 16;
    if (!mirror) {
        out->vx = -path[i].vx - d.vx;
        out->vy = -path[i].vy - d.vy;
        out->vz = -path[i].vz - d.vz;
    } else {
        out->vx = -path[i].vx - d.vx;
        out->vy = path[i].vy + d.vy;
        out->vz = path[i].vz + d.vz;
    }
}

void highway_clear_input(void)
{
    D_80110ABC = 0;
    D_800BDFB0 = 0;
    D_800BDFC4 = 0;
    D_800BC4D8 = 0;
}

void highway_read_input(void)
{
    int stop;
    int unused[4]; /* never touched, but the frame has room for it */

    if (!D_800BC4D8) {
        D_800BD580 = func_8001C808(1);
        stop = 0;
    } else {
        D_800BD580 = 0;
        stop = 0;
    }
    if (stop)
        return;
    D_800BDFB0 = 0;
    D_800BDFC4 = 0;
    if (D_800BD580 & 0x8000)
        D_800BDFB0 = 4;
    if (D_800BD580 & 0x2000)
        D_800BDFB0 = 6;
    if (D_800BD580 & 0x1000) {
        D_800BDFB0 = 8;
        if (D_800BD580 & 0x8000)
            D_800BDFB0 = 7;
        if (D_800BD580 & 0x2000)
            D_800BDFB0 = 9;
    }
    if (D_800BD580 & 0x4000) {
        D_800BDFB0 = 2;
        if (D_800BD580 & 0x8000)
            D_800BDFB0 = 1;
        if (D_800BD580 & 0x2000)
            D_800BDFB0 = 3;
    }
    if (D_800BD580 & 0x80)
        D_800BDFC4 = 1;
    if (D_800BD580 & 0x20)
        D_800BDFC4 = 2;
}

void highway_init_view_planes(void)
{
    VECTOR a, b, c, d;
    VECTOR top_left = {-160, 120, 300};
    VECTOR top_right = {160, 120, 300};
    VECTOR bottom_left = {-160, -120, 300};
    VECTOR bottom_right = {160, -120, 300};

    a.vx = bottom_left.vx >> 2;
    a.vy = bottom_left.vy >> 2;
    a.vz = bottom_left.vz >> 2;
    b.vx = top_left.vx >> 2;
    b.vy = top_left.vy >> 2;
    b.vz = top_left.vz >> 2;
    c.vx = bottom_right.vx >> 2;
    c.vy = bottom_right.vy >> 2;
    c.vz = bottom_right.vz >> 2;
    d.vx = top_right.vx >> 2;
    d.vy = top_right.vy >> 2;
    d.vz = top_right.vz >> 2;
    OuterProduct0(&a, &b, &D_80110A98);
    OuterProduct0(&c, &d, &D_80110AAC);
    D_800BDFA4 = -((bottom_left.vx >> 2) * D_80110A98.vx) - (bottom_left.vy >> 2) * D_80110A98.vy -
                 (bottom_left.vz >> 2) * D_80110A98.vz;
    D_800BDFA8 = -((bottom_right.vx >> 2) * D_80110AAC.vx) - (bottom_right.vy >> 2) * D_80110AAC.vy -
                 (bottom_right.vz >> 2) * D_80110AAC.vz;
    D_800BC674 = (bottom_right.vx >> 2) * D_80110A98.vx + (bottom_right.vy >> 2) * D_80110A98.vy +
                 (bottom_right.vz >> 2) * D_80110A98.vz + D_800BDFA4;
    D_800BC6AC = (bottom_left.vx >> 2) * D_80110AAC.vx + (bottom_left.vy >> 2) * D_80110AAC.vy +
                 (bottom_left.vz >> 2) * D_80110AAC.vz + D_800BDFA8;
    D_800C4C40 = SquareRoot0(D_80110A98.vx * D_80110A98.vx + D_80110A98.vy * D_80110A98.vy +
                             D_80110A98.vz * D_80110A98.vz);
    D_800C4C48 = SquareRoot0(D_80110AAC.vx * D_80110AAC.vx + D_80110AAC.vy * D_80110AAC.vy +
                             D_80110AAC.vz * D_80110AAC.vz);
}

#ifdef NON_MATCHING
/* Register choice only: the two sides' registers (t2/a2) are swapped. */
int highway_in_view(int *v)
{
    int side2 = 0, side1 = 0;
    int x = v[0] >> 2, y = v[1] >> 2, z = v[2] >> 2;
    int d1 = x * D_80110A98.vx + y * D_80110A98.vy + z * D_80110A98.vz + D_800BDFA4;
    int d2 = x * D_80110AAC.vx + y * D_80110AAC.vy + z * D_80110AAC.vz + D_800BDFA8;

    if (d1 > 0 && D_800BC674 > 0)
        side1 = 1;
    if (d1 < 0 && D_800BC674 < 0)
        side1 = 1;
    if (d2 > 0 && D_800BC6AC > 0)
        side2 = 1;
    if (d2 < 0 && D_800BC6AC < 0)
        side2 = 1;
    return side2 & side1;
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_in_view);
#endif

#ifdef NON_MATCHING
/* Register choice only: the two sides' registers (t2/a2) are swapped. */
int highway_in_view_sv(SVECTOR *v)
{
    int side2 = 0, side1 = 0;
    int x = v->vx >> 2, y = v->vy >> 2, z = v->vz >> 2;
    int d1 = x * D_80110A98.vx + y * D_80110A98.vy + z * D_80110A98.vz + D_800BDFA4;
    int d2 = x * D_80110AAC.vx + y * D_80110AAC.vy + z * D_80110AAC.vz + D_800BDFA8;

    if (d1 > 0 && D_800BC674 > 0)
        side1 = 1;
    if (d1 < 0 && D_800BC674 < 0)
        side1 = 1;
    if (d2 > 0 && D_800BC6AC > 0)
        side2 = 1;
    if (d2 < 0 && D_800BC6AC < 0)
        side2 = 1;
    return side2 & side1;
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_in_view_sv);
#endif

int highway_plane1_dist(int x, int y, int z)
{
    return (x >> 2) * D_80110A98.vx + (y >> 2) * D_80110A98.vy + (z >> 2) * D_80110A98.vz + D_800BDFA4;
}

int highway_plane2_dist(int x, int y, int z)
{
    return (x >> 2) * D_80110AAC.vx + (y >> 2) * D_80110AAC.vy + (z >> 2) * D_80110AAC.vz + D_800BDFA8;
}

/* Like highway_in_view, but a point less than `margin` (scaled) from a plane's wrong side still counts. */
#ifdef NON_MATCHING
/* Register choice in the two divides, and an early copy of margin. */
int highway_near_view(int *v, short margin)
{
    int d;
    int side1 = 0, side2 = 0;

    d = (v[0] >> 2) * D_80110A98.vx + (v[1] >> 2) * D_80110A98.vy + (v[2] >> 2) * D_80110A98.vz + D_800BDFA4;
    if (D_800BC674 > 0 && d >= 0)
        side1 = 1;
    if (D_800BC674 < 0 && d <= 0)
        side1 = 1;
    if (!side1 && abs(d) / D_800C4C40 < margin)
        side1 = 1;
    d = (v[0] >> 2) * D_80110AAC.vx + (v[1] >> 2) * D_80110AAC.vy + (v[2] >> 2) * D_80110AAC.vz + D_800BDFA8;
    if (D_800BC6AC > 0 && d >= 0)
        side2 = 1;
    if (D_800BC6AC < 0 && d <= 0)
        side2 = 1;
    if (!side2 && abs(d) / D_800C4C48 < margin)
        side2 = 1;
    return side1 & side2;
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_near_view);
#endif

/* Whether (x, y, z) is on the plane's kept side, or within near of it. */
int highway_near_plane1(int x, int y, int z, short near)
{
    int hit = 0;
    int dist = (x >> 2) * D_80110A98.vx + (y >> 2) * D_80110A98.vy + (z >> 2) * D_80110A98.vz + D_800BDFA4;

    if (D_800BC674 > 0 && dist >= 0)
        hit = 1;
    if (D_800BC674 < 0 && dist <= 0)
        hit = 1;
    if (!hit && ABS(dist) / D_800C4C40 < near)
        hit = 1;
    return hit;
}

/* The same test against the second plane. */
int highway_near_plane2(int x, int y, int z, short near)
{
    int hit = 0;
    int dist = (x >> 2) * D_80110AAC.vx + (y >> 2) * D_80110AAC.vy + (z >> 2) * D_80110AAC.vz + D_800BDFA8;

    if (D_800BC6AC > 0 && dist >= 0)
        hit = 1;
    if (D_800BC6AC < 0 && dist <= 0)
        hit = 1;
    if (!hit && ABS(dist) / D_800C4C48 < near)
        hit = 1;
    return hit;
}

void highway_init_matrices(void)
{
    D_801163E8 = &D_8010EBDC;
    D_80110BB8 = (ScratchPad *)0x1F800000;
    D_8010EA30 = (void *)0x1F800000;
    D_80116678 = (void *)0x1F8000A8;
    D_8010EA34 = (MATRIX *)0x1F8000C8;
    D_8011632C[0] = (SVECTOR *)0x1F800108;
    D_8011632C[1] = (SVECTOR *)0x1F800110;
    D_8011632C[2] = (SVECTOR *)0x1F800118;
    D_8011632C[3] = (SVECTOR *)0x1F800120;
    D_8011632C[4] = (SVECTOR *)0x1F800128;
    D_8011632C[5] = (SVECTOR *)0x1F800130;
    D_8011632C[6] = (SVECTOR *)0x1F800138;
    D_8011632C[7] = (SVECTOR *)0x1F800140;
    D_8011632C[8] = (SVECTOR *)0x1F800148;
    D_8011632C[9] = (SVECTOR *)0x1F800150;
    D_8011632C[10] = (SVECTOR *)0x1F800158;
    D_8011636C[0] = (int *)0x1F800160;
    D_8011636C[1] = (int *)0x1F800164;
    D_8011636C[2] = (int *)0x1F800168;
    D_8011636C[3] = (int *)0x1F80016C;
    D_8011636C[4] = (int *)0x1F800170;
    D_8011636C[5] = (int *)0x1F800174;
    D_8011636C[6] = (int *)0x1F800178;
    D_8011636C[7] = (int *)0x1F80017C;
    D_8011636C[8] = (int *)0x1F800180;
    D_8011636C[9] = (int *)0x1F800184;
    D_8011636C[10] = (int *)0x1F800188;
    RotMatrix(&D_800B4220, &D_801163E8->view);
    RotMatrix(&D_800B4220, &D_801163E8->trans);
    RotMatrix(&D_800B4220, &D_801163E8->rot);
    TransMatrix(&D_801163E8->view, &D_800B4228);
    TransMatrix(&D_801163E8->trans, &D_800B4228);
    TransMatrix(&D_801163E8->rot, &D_800B4228);
    TransMatrix(&D_801163E8->roll, &D_800B4228);
    RotMatrix(&D_800B4220, &D_800BD55C);
    TransMatrix(&D_800BD55C, &D_800B4228);
}

void highway_make_sky(void)
{
    D_800BD4C0 = highway_frame_new(0xAE, 0, 0, 1, &D_8010EB90, 0, 0, 0, 0, 0, 0);
    D_800BD4C4 = highway_frame_new(0xA5, 0, 0, 1, &D_8010EB90, 0, 0, 0, 0, 0, 0);
    D_800BD4C8 = highway_frame_new(0xA3, 0, 0, 1, &D_8010EB90, 0, 0, 0, 0, 0, 0);
    D_800BD4CC = highway_frame_new(0xA7, 0, 0, 1, &D_8010EB90, 0, 0, 0, 0, 0, 0);
}

void highway_draw_sky(DrawBuf *b)
{
    if (D_801163AC == 1)
        highway_draw_sky_strips(b, D_800BD4C0, -500);
    if (D_801163B0 == 1)
        highway_draw_sky_strips(b, D_800BD4C4, -300);
    if (D_801163B4 == 1)
        highway_draw_sky_strips(b, D_800BD4C8, 0);
    if (D_801163B8 == 1)
        highway_draw_sky_faces(b, D_800BD4CC, -500);
}

void highway_init_state(void)
{
    MATRIX light = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    int i;

    SetLightMatrix(&light);
    SetFogNearFar(D_800BD63C, D_80116424, 300);
    D_800BD63C = 0x1964;
    D_80116424 = 0x2CEC;
    D_800BD640 = 0x41A0;
    D_80116428 = 0x4F4C;
    D_800BD534 = 0x7A;
    D_800BD638 = 0x80;
    D_80110B88 = 0;
    D_8010EBCC = 0;
    D_800BD594 = 0;
    D_80110CD4 = 0;
    D_8010EB88 = 0;
    D_8010FDB8 = 0;
    D_800BC680 = 0;
    D_801163A4 = 0;
    D_80110BC4 = 0;
    D_80110BC8 = 0;
    D_80110BD0 = 0;
    D_80110BD8 = 0;
    D_800C4C30 = 0;
    D_800C4C34 = 0;
    D_80110044[0].max = 0xFFFF;
    D_80110044[2].max = 0xFFFF;
    D_80110044[4].max = 0xFFFF;
    D_80110044[1].max = 0xFFFF;
    D_80110044[3].max = 0xFFFF;
    for (i = 0; i < 5; i++) {
        D_80110044[i].cur = D_80110044[i].max;
        D_80110044[i].damage = 0;
        D_80110044[i].x06 = 0;
        D_80110044[i].flash = 0;
        D_80110044[i].icon = 1;
    }
    D_80116680 = 0;
    D_800BC678 = 0;
    D_801163AC = 0;
    D_801163B0 = 0;
    D_801163B4 = 0;
    D_801163B8 = 0;
    D_8010EAE0 = 0;
    D_800BC670 = 0;
    D_800BC4DC = 1;
    D_800BD558 = 0;
    if (D_800BC4D4 == 0)
        D_8010EB84 = D_800B4260[0].script;
    if (D_800BC4D4 == 1)
        D_8010EB84 = D_800B4260[1].script;
}

void highway_load_files(void)
{
    cd_read(D_800B4238, D_800B423C, (void *)0x80120000, 0);
    while (cd_step())
        ;
    highway_load_textures();
    highway_setup_road_quads();
    cd_read(D_800B4240, D_800B4244, (void *)0x801A0000, 0);
    while (cd_step())
        ;
    cd_read(D_800B4248, D_800B424C, (void *)0x80120000, 0);
    while (cd_step())
        ;
}

void highway_stop_sound(void)
{
    snd_request.id = 0xC1;
    snd_request.arg[0] = 0xF0;
    snd_request.arg[1] = 0;
    snd_submit_request();
    snd_request.id = 0xB9;
    snd_request.arg[0] = 0xF0;
    snd_request.arg[1] = 0;
    snd_submit_request();
}

void highway_start_sound(void)
{
    snd_request.id = 0xC0;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0xB8;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0x10;
    snd_request.arg[0] = D_800B4250;
    snd_submit_request();
}

void highway_play_sound(int a0, int channel, int v)
{
    int busy = 0;

    if (channel == 1)
        busy = D_800C4C30 > 0;
    if (channel == 2 && D_800C4C34 > 0)
        busy = 1;
    if (channel == 1 && v < 0) {
        snd_request.id = 0x28;
        snd_request.arg[0] = 0;
        snd_request.arg[1] = a0;
        busy = 0;
        snd_submit_request();
    }
    if (channel == 2 && v < 0) {
        snd_request.id = 0x29;
        snd_request.arg[0] = 0;
        snd_request.arg[1] = a0;
        busy = 0;
        snd_submit_request();
    }
    if (busy != 1) {
        if (channel == 1)
            snd_request.id = 0x28;
        if (channel == 2)
            snd_request.id = 0x29;
        if (channel == 3)
            snd_request.id = 0x2A;
        if (channel == 4)
            snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = a0;
        snd_submit_request();
    }
    if (!busy) {
        if (channel == 1)
            D_800C4C30 = v;
        if (channel == 2)
            D_800C4C34 = v;
    }
    if (channel == 1 && v < 0)
        D_800C4C30 = -v;
    if (channel == 2 && v < 0)
        D_800C4C34 = -v;
}

void highway_sound_b0(int arg, int which)
{
    if (which == 1)
        snd_request.id = 0xB0;
    if (which == 2)
        snd_request.id = 0xB1;
    if (which == 3)
        snd_request.id = 0xB2;
    if (which == 4)
        snd_request.id = 0xB3;
    snd_request.arg[0] = arg;
    snd_submit_request();
}

/* Old-style, against an int prototype: callers pass the narrow parameters as ints and the function narrows them. */
void highway_set_volume(arg, which)
int arg;
unsigned char which;
{
    if (which == 1)
        snd_request.id = 0xA0;
    if (which == 2)
        snd_request.id = 0xA1;
    if (which == 3)
        snd_request.id = 0xA2;
    if (which == 4)
        snd_request.id = 0xA3;
    snd_request.arg[0] = arg;
    snd_submit_request();
}

/* Old-style, against an int prototype: callers pass the narrow parameters as ints and the function narrows them. */
void highway_set_pan(arg, which)
short arg;
unsigned char which;
{
    if (which == 1)
        snd_request.id = 0xA8;
    if (which == 2)
        snd_request.id = 0xA9;
    if (which == 3)
        snd_request.id = 0xAA;
    if (which == 4)
        snd_request.id = 0xAB;
    snd_request.arg[0] = arg;
    snd_submit_request();
}

void highway_advance_road(void)
{
    int i;

    D_80110CD4 += D_800BD534;
    D_8010FDB8 += D_800BD534;
    D_800BC680 = D_8010FDB8 >> 8;
    D_8010FDB8 &= 0xFF;
    D_8010EB88 += D_800BC680;
    D_8011668C = D_80110CD4 + 0x2300;
    for (i = 0; i < D_800BC680; i++) {
        highway_free_segment_frames();
        highway_build_segment();
    }
}

/* Runs the script's due event, then the two timed sequences (sound cues and speed changes) once started. */
#ifdef NON_MATCHING
/* Case 20 only: the game keeps &D_800B4F4C[5].bike.state in s0 and writes rider 1's as -0x600(s0);
 * GCC writes both directly. */
void highway_run_script(void)
{
    Event *ev;

    if (D_800C4C30 > 0)
        D_800C4C30--;
    if (D_800C4C34 > 0)
        D_800C4C34--;
    ev = &D_8010EB84[D_800BE300];
    if (ev->time < D_8010EB88 - 45) {
        switch (ev->code) {
        case 18:
            highway_start_camera_path(5, 4, 0x27100);
            break;
        case 5:
            D_800BE2F8.vx = 90;
            D_800BE2F8.vy = 180;
            D_80110A8C = 5;
            D_801163A8 = D_8011668C + 0x29E0;
            break;
        case 19:
            D_800BE2F8.vx = 90;
            D_800BE2F8.vy = 180;
            D_80110A8C = 5;
            D_801163A8 = D_8011668C + 0x14F0;
            break;
        case 6:
            D_80110A8C = 1;
            break;
        case 11:
            D_801163AC = 0;
            break;
        case 10:
            D_801163AC = 1;
            break;
        case 13:
            D_801163B0 = 0;
            break;
        case 12:
            D_801163B0 = 1;
            break;
        case 17:
            D_801163B4 = 0;
            break;
        case 16:
            D_801163B4 = 1;
            break;
        case 26:
            D_801163B8 = 0;
            break;
        case 25:
            D_801163B8 = 1;
            break;
        case 8:
            D_800B4F4C[2].bike.pushed = 1;
            D_800B4F4C[3].bike.pushed = 1;
            D_800B4F4C[4].bike.pushed = 1;
            D_800BD594 = 1;
            break;
        case 14:
            snd_request.id = 0x2A;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0;
            snd_submit_request();
            snd_request.id = 0x2A;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0x126;
            snd_submit_request();
            D_800B4F4C[5].bike.state = 0;
            break;
        case 20:
            D_800B4F4C[0].bike.x108 = 0;
            D_800B4F4C[0].bike.x3C = -0xC800;
            D_800B4F4C[0].bike.x40 = 0x38400;
            highway_start_rider_path(0, 7);
            D_800B4F4C[5].bike.state = 6;
            D_800B4F4C[5].bike.x3C = 0xAA00;
            D_800B4F4C[5].bike.x40 = 0x1F400;
            highway_start_rider_path(5, 6);
            D_800B4F4C[1].bike.state = 6;
            D_800B4F4C[1].bike.x3C = -0xB400;
            D_800B4F4C[1].bike.x40 = 0x5DC00;
            highway_start_rider_path(1, 8);
            highway_start_camera_path(2, 4, 0x10000);
            D_800BC4D8 = 1;
            D_80116680 = 1;
            D_800C4A9C = 0;
            break;
        case 21:
            if (D_800BC678)
                break;
            D_800B4F4C[0].bike.nparts--;
            D_800B4F4C[0].bike.x108 = 0x4A;
            D_800B4F4C[1].bike.heading = 0x31;
            highway_start_camera_path(9, 4, 0x10000);
            D_800BC4D8 = 1;
            D_800BC678 = 1;
            D_800C4A9C = 0;
            D_8011639C = 2;
            break;
        case 22:
            D_8011639C = 1;
            break;
        case 23:
            D_8011639C = 3;
            break;
        case 24:
            D_8011639C = 0;
            break;
        case 28:
            D_800B4F4C[0].bike.state = 6;
            break;
        }
        D_800BE300++;
    }
    if (D_800BC678) {
        if (D_800C4A9C >= 6) {
            if (D_800BD534 > 0)
                D_800BD534 -= 10;
            if (D_800BD534 < 0)
                D_800BD534 = 0;
        }
        if (D_800C4A9C == 0x109) {
            snd_request.id = 0xC1;
            snd_request.arg[0] = 0xF0;
            snd_request.arg[1] = 0;
            snd_submit_request();
            snd_request.id = 0xB9;
            snd_request.arg[0] = 0xF0;
            snd_request.arg[1] = 0;
            snd_submit_request();
        }
        if (D_800C4A9C == 5) {
            snd_request.id = 0xA2;
            snd_request.arg[0] = 0x7F;
            snd_submit_request();
            snd_request.id = 0xA3;
            snd_request.arg[0] = 0x7F;
            snd_submit_request();
            snd_request.id = 0xB2;
            snd_request.arg[0] = 1;
            snd_submit_request();
            snd_request.id = 0xB3;
            snd_request.arg[0] = 1;
            snd_submit_request();
            snd_request.id = 0x2A;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0x241;
            snd_submit_request();
            snd_request.id = 0x2B;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0x241;
            snd_submit_request();
            D_800B4F4C[0].bike.pos.vz -= 0x2800;
            D_800B4F4C[1].bike.pos.vz += 0x6400;
        }
        if (D_800C4A9C == 0x19) {
            snd_request.id = 0x2A;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0;
            snd_submit_request();
            snd_request.id = 0x2B;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0;
            snd_submit_request();
            snd_request.id = 0xA2;
            snd_request.arg[0] = 0;
            snd_submit_request();
            snd_request.id = 0xA3;
            snd_request.arg[0] = 0;
            snd_submit_request();
        }
        if (D_800C4A9C == 0x14A)
            D_800BC4DC = 2;
        if (D_800C4A9C == 0x190)
            D_80110B88 = 1;
        D_800C4A9C++;
    }
    if (D_80116680) {
        if (D_800C4A9C >= 0xDD)
            D_800B4F4C[5].bike.x12C++;
        if (D_800C4A9C >= 0xB5) {
            if (D_800BD534 > 0)
                D_800BD534 -= 3;
            if (D_800BD534 < 0)
                D_800BD534 = 0;
        }
        if (D_800C4A9C == 0x13B) {
            snd_request.id = 0xC1;
            snd_request.arg[0] = 0xF0;
            snd_request.arg[1] = 0;
            snd_submit_request();
            snd_request.id = 0xB9;
            snd_request.arg[0] = 0xF0;
            snd_request.arg[1] = 0;
            snd_submit_request();
        }
        if (D_800C4A9C == 0xB8) {
            snd_request.id = 0xA2;
            snd_request.arg[0] = 0x7F;
            snd_submit_request();
            snd_request.id = 0xA3;
            snd_request.arg[0] = 0x7F;
            snd_submit_request();
            snd_request.id = 0xB2;
            snd_request.arg[0] = 1;
            snd_submit_request();
            snd_request.id = 0xB3;
            snd_request.arg[0] = 1;
            snd_submit_request();
            snd_request.id = 0x2A;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0x241;
            snd_submit_request();
            snd_request.id = 0x2B;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0x241;
            snd_submit_request();
        }
        if (D_800C4A9C == 0xCB) {
            snd_request.id = 0x2A;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0;
            snd_submit_request();
            snd_request.id = 0x2B;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0;
            snd_submit_request();
            snd_request.id = 0xA2;
            snd_request.arg[0] = 0;
            snd_submit_request();
            snd_request.id = 0xA3;
            snd_request.arg[0] = 0;
            snd_submit_request();
        }
        if (D_800C4A9C == 0x17C)
            D_800BC4DC = 2;
        if (D_800C4A9C == 0x1C2)
            D_80110B88 = 1;
        D_800C4A9C++;
    }
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_run_script);
#endif

/* The screen fade: 1 starts a fade in from black, 2 a fade out; 3 and 4 step them. Returns the new state. */
unsigned char highway_fade(DrawBuf *buf, unsigned char state)
{
    POLY_G4 *g;
    POLY_FT4 *ft;

    switch (state) {
    case 1:
        D_800BC670 = 0xFC;
        state = 3;
        break;
    case 2:
        D_800BC670 = 0;
        state = 4;
        break;
    case 3:
        D_800BC670 -= 4;
        if (!D_800BC670)
            state = 0;
        break;
    case 4:
        D_800BC670 = D_800BC670 < 0xF0 ? D_800BC670 + 4 : 0xFF;
        break;
    }
    g = buf->pool.g4;
    ft = buf->pool.ft4;
    g->r0 = D_800BC670;
    g->g0 = D_800BC670;
    g->b0 = D_800BC670;
    g->r1 = D_800BC670;
    g->g1 = D_800BC670;
    g->b1 = D_800BC670;
    g->r2 = D_800BC670;
    g->g2 = D_800BC670;
    g->b2 = D_800BC670;
    g->r3 = D_800BC670;
    g->g3 = D_800BC670;
    g->x0 = 0;
    g->y0 = 0;
    g->x1 = 320;
    g->y1 = 0;
    g->x2 = 0;
    g->y2 = 240;
    g->x3 = 320;
    g->y3 = 240;
    g->b3 = D_800BC670;
    func_80046848(g, 1);
    addPrim(&buf->ot_hud[1], g);
    g++;
    ft->x0 = 0;
    ft->y0 = 0;
    ft->x1 = 0;
    ft->y1 = 0;
    ft->x2 = 0;
    ft->y2 = 0;
    ft->x3 = 0;
    ft->y3 = 0;
    ft->r0 = 0;
    ft->g0 = 0;
    ft->b0 = 0;
    ft->u0 = 0;
    ft->v0 = 0;
    ft->u1 = 0;
    ft->v1 = 0;
    ft->u2 = 0;
    ft->v2 = 0;
    ft->u3 = 0;
    ft->v3 = 0;
    ft->tpage = GetTPage(1, 2, 320, 224);
    ft->clut = GetClut(16, 0x1E4);
    addPrim(&buf->ot_hud[1], ft);
    ft++;
    buf->pool.ft4 = ft;
    buf->pool.g4 = g;
    return state;
}

void highway_damage_gauge(unsigned char idx, short amount)
{
    unsigned char i, found;
    int next;

    if (D_80110044[2].cur < 30 && idx == 2)
        amount = 0;
    D_80110044[idx].flash = 0x70;
    D_80110044[idx].damage += amount;
    if (amount < D_80110044[idx].cur)
        D_80110044[idx].cur -= amount;
    else
        D_80110044[idx].cur = 0;
    found = 0;
    if (D_80110044[idx].cur)
        return;
    for (i = 0; i < 4; i++) {
        next = (idx + i) % 4 + 1;
        if (D_80110044[(unsigned char)next].cur) {
            idx = next;
            found = 1;
        }
    }
    if (found == 1) {
        D_80110044[idx].flash = 0x70;
        D_80110044[idx].damage += amount;
        if (amount < D_80110044[idx].cur)
            D_80110044[idx].cur -= amount;
        else
            D_80110044[idx].cur = 0;
    }
}

void highway_draw_gauges(DrawBuf *buf)
{
    int i;

    for (i = 0; i < 5; i++) {
        if (rand() % 30 == 0 && D_80110044[i].icon == 1 && D_80110044[i].cur > 10)
            D_80110044[i].icon = 3;
        if (D_80110044[i].icon >= 2)
            D_80110044[i].icon--;
        highway_draw_gauge(buf, i, D_80110044[i].flash, D_80110044[i].icon);
        if (D_80110044[i].flash)
            D_80110044[i].flash -= 8;
    }
}

/* One gauge: its bar (red-shifted by flash), its state's icon (grey once empty) and its frame. */
/* Old-style, against an int prototype: the caller passes i and state as ints and the function narrows them. */
#ifdef NON_MATCHING
/* Register allocation only: flash * 2 lands in v1 instead of t4, so r/g/b move from t7/t6/t5 to
 * t6/t5/t4. */
void highway_draw_gauge(buf, i, flash, state)
DrawBuf *buf;
short i;
int flash;
unsigned char state;
{
    POLY_FT4 *p;
    int len;
    unsigned char r, g, b;
    int red, green;

    if (D_80110044[i].max) {
        len = (D_80110044[i].cur << 6) / D_80110044[i].max - 1;
        if (len < 0)
            len = 0;
    } else
        len = 0;
    green = 0x80 - flash;
    red = flash * 2;
    if (!D_80110044[i].cur) {
        r = 0x30;
        g = 0x30;
        b = 0x30;
    } else {
        r = 0x80;
        g = 0x80;
        b = 0x80;
    }
    p = buf->pool.ft4;
    p->x0 = D_80110CD8[i].bar_x;
    p->y0 = D_80110CD8[i].bar_y;
    p->x1 = D_80110CD8[i].bar_x + len;
    p->y1 = D_80110CD8[i].bar_y;
    p->x2 = D_80110CD8[i].bar_x;
    p->y2 = D_80110CD8[i].bar_y + 4;
    p->x3 = D_80110CD8[i].bar_x + len;
    p->y3 = D_80110CD8[i].bar_y + 4;
    p->r0 = red;
    p->g0 = green;
    p->b0 = 0;
    p->u0 = 0;
    p->v0 = 0x38;
    p->u1 = 0;
    p->v1 = 0x38;
    p->u2 = 0;
    p->v2 = 0x3D;
    p->u3 = 0;
    p->v3 = 0x3D;
    p->tpage = D_80116368;
    p->clut = D_80110B8C;
    addPrim(&buf->ot_hud[3], p);
    p++;
    p->x0 = D_80110CD8[i].icon_x + D_80110CD8[i].x;
    p->y0 = D_80110CD8[i].y + 1;
    p->x1 = D_80110CD8[i].icon_x + D_80110CD8[i].x + D_80110CD8[i].icon_size;
    p->y1 = D_80110CD8[i].y + 1;
    p->x2 = D_80110CD8[i].icon_x + D_80110CD8[i].x;
    p->y2 = D_80110CD8[i].y + (D_80110CD8[i].icon_size + 1);
    p->x3 = D_80110CD8[i].icon_x + D_80110CD8[i].x + D_80110CD8[i].icon_size;
    p->y3 = D_80110CD8[i].y + (D_80110CD8[i].icon_size + 1);
    p->r0 = r;
    p->g0 = g;
    p->b0 = b;
    p->u0 = D_80110CD8[i].icon_u[state];
    p->v0 = D_80110CD8[i].icon_v[state];
    p->u1 = D_80110CD8[i].icon_u[state] + D_80110CD8[i].icon_size;
    p->v1 = D_80110CD8[i].icon_v[state];
    p->u2 = D_80110CD8[i].icon_u[state];
    p->v2 = D_80110CD8[i].icon_v[state] + D_80110CD8[i].icon_size;
    p->u3 = D_80110CD8[i].icon_u[state] + D_80110CD8[i].icon_size;
    p->v3 = D_80110CD8[i].icon_v[state] + D_80110CD8[i].icon_size;
    p->tpage = D_80116368;
    p->clut = D_80110B8C;
    addPrim(&buf->ot_hud[3], p);
    p++;
    p->x0 = D_80110CD8[i].x;
    p->y0 = D_80110CD8[i].y;
    p->x1 = D_80110CD8[i].w + D_80110CD8[i].x;
    p->y1 = D_80110CD8[i].y;
    p->x2 = D_80110CD8[i].x;
    p->y2 = D_80110CD8[i].h + D_80110CD8[i].y;
    p->x3 = D_80110CD8[i].w + D_80110CD8[i].x;
    p->y3 = D_80110CD8[i].h + D_80110CD8[i].y;
    p->r0 = 0x80;
    p->g0 = 0x80;
    p->b0 = 0x80;
    p->u0 = D_80110CD8[i].icon_u[0];
    p->v0 = D_80110CD8[i].icon_v[0];
    p->u1 = D_80110CD8[i].icon_u[0] + D_80110CD8[i].w;
    p->v1 = D_80110CD8[i].icon_v[0];
    p->u2 = D_80110CD8[i].icon_u[0];
    p->v2 = D_80110CD8[i].icon_v[0] + D_80110CD8[i].h;
    p->u3 = D_80110CD8[i].icon_u[0] + D_80110CD8[i].w;
    p->v3 = D_80110CD8[i].icon_v[0] + D_80110CD8[i].h;
    p->tpage = D_80116368;
    p->clut = D_80110B8C;
    addPrim(&buf->ot_hud[3], p);
    p++;
    buf->pool.ft4 = p;
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_draw_gauge);
#endif

/* A five-digit number from the font texture; pad keeps the leading zeros. */
void highway_draw_number(DrawBuf *buf, int value, short x, short y, short pad)
{
    POLY_FT4 *p = buf->pool.ft4;
    int place = 10000;
    unsigned char lead = 1;
    int rest = value + 1;
    int i, digit;

    for (i = 0; i < 5; i++) {
        digit = 0;
        while (place < rest) {
            rest -= place;
            digit++;
        }
        if (digit)
            lead = 0;
        if (value == 0 && place == 1)
            lead = 0;
        if (pad == 1 || digit != 0 || !lead) {
            p->x0 = x + i * 12;
            p->y0 = y;
            p->x1 = x + 16 + i * 12;
            p->y1 = y;
            p->x2 = x + i * 12;
            p->y2 = y + 16;
            p->x3 = x + 16 + i * 12;
            p->y3 = y + 16;
            p->r0 = 0x80;
            p->g0 = 0x80;
            p->b0 = 0x80;
            p->u0 = digit * 16;
            p->v0 = 200;
            p->u1 = digit * 16 + 15;
            p->v1 = 200;
            p->u2 = digit * 16;
            p->v2 = 215;
            p->u3 = digit * 16 + 15;
            p->v3 = 215;
            p->tpage = D_800BD7F0[0];
            p->clut = D_80116690[0];
            func_80046848(p, 0);
            addPrim(&buf->ot_hud[3], p);
            p++;
        }
        place /= 10;
    }
    buf->pool.ft4 = p;
}

/* The same with the small font; pad keeps the leading zeros. */
void highway_draw_small_number(DrawBuf *buf, int value, short x, short y, short pad)
{
    POLY_FT4 *p = buf->pool.ft4;
    int place = 10000;
    unsigned char lead = 1;
    int rest = value + 1;
    int i, digit;

    for (i = 0; i < 5; i++) {
        digit = 0;
        while (place < rest) {
            rest -= place;
            digit++;
        }
        if (digit)
            lead = 0;
        if (value == 0 && place == 1)
            lead = 0;
        if (pad == 1 || digit != 0 || !lead) {
            p->x0 = x + i * 8;
            p->y0 = y;
            p->x1 = x + 8 + i * 8;
            p->y1 = y;
            p->x2 = x + i * 8;
            p->y2 = y + 16;
            p->x3 = x + 8 + i * 8;
            p->y3 = y + 16;
            p->r0 = 0x80;
            p->g0 = 0x80;
            p->b0 = 0x80;
            p->u0 = digit * 8;
            p->v0 = 224;
            p->u1 = digit * 8 + 8;
            p->v1 = 224;
            p->u2 = digit * 8;
            p->v2 = 239;
            p->u3 = digit * 8 + 8;
            p->v3 = 239;
            p->tpage = D_800BD7F0[0];
            p->clut = D_80116690[0];
            func_80046848(p, 0);
            addPrim(&buf->ot_hud[3], p);
            p++;
        }
        place /= 10;
    }
    buf->pool.ft4 = p;
}

/* A textured sprite: tex picks the tpage and CLUT. */
void highway_draw_sprite(DrawBuf *buf, short tex, short x, short y, short w, short h, unsigned char u, unsigned char v,
                         unsigned char uw, unsigned char vh, unsigned char semi)
{
    POLY_FT4 *p = buf->pool.ft4;

    p->x0 = x;
    p->y0 = y;
    p->x1 = x + w;
    p->y1 = y;
    p->x2 = x;
    p->y2 = y + h;
    p->x3 = x + w;
    p->y3 = y + h;
    p->r0 = 0x80;
    p->g0 = 0x80;
    p->b0 = 0x80;
    p->u0 = u;
    p->v0 = v;
    p->u1 = u + uw;
    p->v1 = v;
    p->u2 = u;
    p->v2 = v + vh;
    p->u3 = u + uw;
    p->v3 = v + vh;
    p->tpage = D_800BD7F0[tex];
    p->clut = D_80116690[tex];
    func_80046848(p, semi);
    addPrim(&buf->ot_hud[3], p);
    p++;
    buf->pool.ft4 = p;
}

void highway_init_figures(void)
{
    int i, j, k, n;

    D_800B6D4C = 0;
    D_800BE53C = 0;
    D_80110A94 = 0;
    D_8010FDC0 = (char *)0x801C0000;
    D_80116688 = (FigureFile *)0x801A0000;
    D_800BD658 = ((FigureFile *)0x801A0000)->figures;
    for (i = 0; i < D_80116688->count; i++) {
        D_8010FDC0 = highway_setup_figure_prims(&D_800BD658[i], D_8010FDC0, i);
        D_800BD658[i].on = 0;
        D_800BC6B0[i].nparts = D_800BD658[i].nanims - 1;
        D_800BD658[i].rx = 0x40;
        D_800BD658[i].ry = 0;
        D_800BD658[i].rz = 0x80;
    }
    for (i = 0; i < D_80116688->count; i++) {
        for (k = 0; k < 20; k++)
            D_800BC6B0[i].frames[k] = 0;
        n = D_800BD658[i].nanims;
        for (j = 0; j < n; j++) {
            D_800BDF00 = (Anim *)(D_800BD658[i].base + D_800BD658[i].anims) + j;
            D_800BC6B0[i].frames[j] = D_800BDF00->nframes - 1;
        }
        D_800BC6B0[i].m.t[0] = 0;
        D_800BC6B0[i].m.t[1] = 0;
        D_800BC6B0[i].m.t[2] = 0;
        D_800BC6B0[i].x98 = 0;
        D_800BC6B0[i].x9C = 0;
        D_800BC6B0[i].anim = 0;
        D_800BC6B0[i].frame = 0;
        D_800BC6B0[i].xA8 = 0;
        D_800BC6B0[i].flash = 0;
        D_800BC6B0[i].xAE = 0;
    }
}

void highway_update_figures(void)
{
    int i;
    FigureState *m;

    m = D_800BC6B0;
    for (i = 0; i < D_80116688->count; i++, m++) {
        *(int *)0x1F800000 = 3;
        highway_pose_figure(&D_800BD658[i], &m->m, m->anim, m->frame);
    }
    m = D_800BC6B0;
    for (i = 0; i < D_80116688->count; i++, m++) {
        if (m->xAE == 1) {
            D_800BD4D0[0] = m->flash;
            D_800BD4D0[1] = m->flash >> 8;
            D_800BD4D0[2] = m->flash;
            D_800BD4D0[3] = m->flash >> 8;
            D_800BD4D0[4] = m->flash;
            D_800BD4D0[5] = m->flash >> 8;
            D_800BD4D0[6] = 1;
            highway_tint_figure(&D_800BD658[i], D_800BD4D0);
            m->xAE = 0;
        }
        if (m->flash > 0) {
            m->flash -= 0x80;
            D_800BD4D0[0] = m->flash;
            D_800BD4D0[1] = m->flash >> 8;
            D_800BD4D0[2] = m->flash;
            D_800BD4D0[3] = m->flash >> 8;
            D_800BD4D0[4] = m->flash;
            D_800BD4D0[5] = m->flash >> 8;
            D_800BD4D0[6] = 1;
            highway_tint_figure(&D_800BD658[i], D_800BD4D0);
            if (m->flash < 0)
                m->flash = 0;
            m->xAE = 1;
        }
    }
    for (i = 0; i < D_80116688->count; i++)
        highway_draw_figure(&D_800BD658[i]);
}

void highway_init_things(void)
{
    int i;

    for (i = 0; i < 100; i++) {
        D_80110D3C[i].slot = -1;
        D_80110D3C[i].active = 0;
    }
    D_80116364 = 0;
    for (i = 0; i < 100; i++)
        D_80110AC0[i] = i + 1;
}

short highway_spawn_thing(Thing *src, short parent)
{
    short slot, model;

    if (D_80116684 < 99) {
        D_80116684++;
        slot = highway_alloc_thing_slot();
        D_80110D3C[slot] = *src;
        model = src->model;
        D_80110D3C[slot].active = 1;
        D_80110D3C[slot].slot = slot;
        if (!parent)
            D_80110D3C[slot].frame = highway_frame_new(model, 0, 0, 1, &D_8010EB90, src->pos.vx, src->pos.vy,
                                                       src->pos.vz, src->rot.vx, src->rot.vy, src->rot.vz);
        else
            D_80110D3C[slot].frame = highway_frame_new(model, 0, 0, 1, D_80110D3C[parent].frame, src->pos.vx,
                                                       src->pos.vy, src->pos.vz, src->rot.vx, src->rot.vy, src->rot.vz);
    }
    return slot;
}

inline void highway_free_thing(Thing *t)
{
    if (t->slot == -1)
        return;
    D_80116684--;
    highway_frame_free(t->frame);
    highway_free_thing_slot(t->slot);
    t->slot = -1;
    t->active = 0;
}

short highway_alloc_thing_slot(void)
{
    short slot = D_80116364;

    D_80116364 = D_80110AC0[slot];
    return slot;
}

void highway_free_thing_slot(short slot)
{
    short next = D_80116364;

    D_80116364 = slot;
    D_80110AC0[slot] = next;
}

void highway_update_things(DrawBuf *b)
{
    int i;
    short layer;
    int unused[4]; /* never used, but the game keeps its stack room */
    Thing *t;
    Bike *r;
    unsigned char order, hidden;

    for (i = 0; i < 100; i++) {
        t = &D_80110D3C[i];
        layer = 0;
        if (!t->active)
            continue;
        order = 2;
        hidden = 0;
        switch (t->type) {
        case 1:
            r = &D_800B4F4C[t->rider].bike;
            if (!t->started) {
                t->started = 1;
                t->timer = 0;
                t->x50 = 0;
                t->x54 = 0;
            }
            highway_road_point_rot(D_8011668C + t->x50 + (r->pos.vz >> 8), r->pos.vx >> 8, &t->pos, &t->rot);
            t->timer++;
            t->x50 -= 40;
            t->x54 += rand() % 20;
            t->pos.vy -= t->x54;
            if (t->timer > 10)
                highway_free_thing(t);
            break;
        case 2:
            if (!t->started) {
                t->started = 1;
                t->timer = 0;
                t->x50 = 0;
                t->x54 = 0;
                t->x58 = 0;
            }
            t->x58 = (t->x58 + 1) % 3;
            t->x5C = t->x58 + 0xAA;
            t->frame->model = D_800BD81C[t->x5C];
            if (++t->timer >= 3)
                highway_free_thing(t);
            break;
        case 3:
            if (!t->started) {
                t->started = 1;
                t->timer = 0;
                t->x50 = 0;
                t->x54 = 0;
                t->x58 = 0;
            }
            t->x58 = (t->x58 + 1) % 3;
            t->x5C = t->x58 + 0xB2;
            t->frame->model = D_800BD81C[t->x5C];
            if (++t->timer >= 2)
                highway_free_thing(t);
            break;
        case 4:
            if (!t->started) {
                t->started = 1;
                t->timer = 0;
                t->x50 = 0;
                t->x54 = -30;
                t->x58 = 0;
            }
            t->x58++;
            t->rot.vy += 0x100;
            t->pos.vy += t->x54;
            t->x54++;
            if (++t->timer > 60)
                highway_free_thing(t);
            break;
        }
        t->frame->m.t[0] = t->pos.vx;
        t->frame->m.t[1] = t->pos.vy;
        t->frame->m.t[2] = t->pos.vz;
        if (order == 0)
            RotMatrixYXZ(&t->rot, &t->frame->m);
        if (order == 1)
            RotMatrixZYX(&t->rot, &t->frame->m);
        if (order)
            RotMatrix(&t->rot, &t->frame->m);
        if (!hidden)
            highway_draw_frame(b, t->frame, layer, 0);
    }
}

Thing *highway_spawn_thing_at(short x, short y, short z, short type, short model)
{
    D_80110BEC.pos.vx = x;
    D_80110BEC.pos.vy = y;
    D_80110BEC.pos.vz = z;
    D_80110BEC.type = type;
    D_80110BEC.started = 1;
    D_80110BEC.xA8 = 0;
    D_80110BEC.model = model;
    return &D_80110D3C[highway_spawn_thing(&D_80110BEC, 0)];
}

char *highway_setup_figure_prims(Figure *m, char *work, int a2)
{
    unsigned int i;
    Part *parts;

    m->work = work;
    work += m->nbones * 32;
    parts = (Part *)(m->base + m->parts);
    for (i = 0; i < m->nparts; i++)
        work = highway_setup_part_prims(&parts[i], work, 0, a2);
    highway_scale_figure(m, m->scale, 0);
    return work;
}

/* Sets up a part's primitives in both buffers from its polygons: colours, texture coordinates, CLUT and texture page
 * (moved to texture variant `variant`), and the kinds; the vertices are filled each frame. */
char *highway_setup_part_prims(Part *p, char *prims, int relocate, int variant)
{
    unsigned int buf;
    int du4, dv4, du8, dv8;
    unsigned int i, n;
    unsigned int *src;
    unsigned char *flags;
    char *out;
    int du, dv, dy;
    int *texinfo;
    unsigned short *uvs;

    texinfo = (int *)(p->base + p->texinfo);
    uvs = (unsigned short *)(p->base + p->uvs);

    if (relocate)
        p->base = (char *)p + 0x20;
    p->prims = prims;
    du4 = variant % 4 << 6;
    dv4 = variant / 4 << 5;
    du8 = variant % 8 << 5;
    dv8 = variant / 8 << 5;
    for (buf = 0; buf < 2; buf++) {
        out = prims;
        flags = (unsigned char *)(p->base + p->flags);
        if (buf)
            out += p->size;
        src = (unsigned int *)(p->base + p->polys);
        n = p->n_gt4;
        for (i = 0; i < n; i++, out += sizeof(POLY_GT4), src += 6) {
            POLY_GT4 *poly = (POLY_GT4 *)out;
            unsigned int uv, info;
            unsigned char flag;

            *(unsigned int *)&poly->r0 = src[1];
            *(unsigned int *)&poly->r1 = src[2];
            *(unsigned int *)&poly->r2 = src[3];
            *(unsigned int *)&poly->r3 = src[4];
            uv = src[5];
            *(unsigned short *)&poly->u0 = uvs[uv & 0xFF];
            *(unsigned short *)&poly->u1 = uvs[(uv & 0xFF00) >> 8];
            *(unsigned short *)&poly->u2 = uvs[uv >> 16 & 0xFF];
            *(unsigned short *)&poly->u3 = uvs[uv >> 24];
            flag = *flags++;
            info = texinfo[flag & 0xF];
            dy = (info & 0x3F) != 2 ? variant : 0;
            poly->clut = getClut((info & 0x3F0000) >> 12, ((info & 0x7FC00000) >> 22) + dy);
            poly->tpage = getTPage((info & 0xC0) >> 6, flag >> 5 & 3, (info & 0xF00) >> 2, (info >> 12 & 1) << 8);
            if ((info & 0x3F) == 0)
                du = du4, dv = dv4;
            else if ((info & 0x3F) == 1)
                du = du8, dv = dv8;
            else
                du = dv = 0;
            setlen(poly, 12);
            poly->code = 0x3C;
            poly->u0 += du;
            poly->v0 += dv;
            poly->u1 += du;
            poly->v1 += dv;
            poly->u2 += du;
            poly->v2 += dv;
            poly->u3 += du;
            poly->v3 += dv;
            if (flag & 0x10)
                poly->code = 0x3E;
        }
        n = p->n_gt3;
        for (i = 0; i < n; i++, out += sizeof(POLY_GT3), src += 5) {
            POLY_GT3 *poly = (POLY_GT3 *)out;
            unsigned int uv, info;
            unsigned char flag;

            *(unsigned int *)&poly->r0 = src[1];
            *(unsigned int *)&poly->r1 = src[2];
            *(unsigned int *)&poly->r2 = src[3];
            uv = src[4];
            *(unsigned short *)&poly->u0 = uvs[uv & 0xFF];
            *(unsigned short *)&poly->u1 = uvs[(uv & 0xFF00) >> 8];
            *(unsigned short *)&poly->u2 = uvs[uv >> 16 & 0xFF];
            flag = *flags++;
            info = texinfo[flag & 0xF];
            dy = (info & 0x3F) != 2 ? variant : 0;
            poly->clut = getClut((info & 0x3F0000) >> 12, ((info & 0x7FC00000) >> 22) + dy);
            poly->tpage = getTPage((info & 0xC0) >> 6, flag >> 5 & 3, (info & 0xF00) >> 2, (info >> 12 & 1) << 8);
            if ((info & 0x3F) == 0)
                du = du4, dv = dv4;
            else if ((info & 0x3F) == 1)
                du = du8, dv = dv8;
            else
                du = dv = 0;
            setlen(poly, 9);
            poly->code = 0x34;
            poly->u0 += du;
            poly->v0 += dv;
            poly->u1 += du;
            poly->v1 += dv;
            poly->u2 += du;
            poly->v2 += dv;
            if (flag & 0x10)
                poly->code = 0x36;
        }
        n = p->n_ft4;
        for (i = 0; i < n; i++, out += sizeof(POLY_FT4), src += 3) {
            POLY_FT4 *poly = (POLY_FT4 *)out;
            unsigned int uv, info;
            unsigned char flag;

            *(unsigned int *)&poly->r0 = src[1];
            uv = src[2];
            *(unsigned short *)&poly->u0 = uvs[uv & 0xFF];
            *(unsigned short *)&poly->u1 = uvs[(uv & 0xFF00) >> 8];
            *(unsigned short *)&poly->u2 = uvs[uv >> 16 & 0xFF];
            *(unsigned short *)&poly->u3 = uvs[uv >> 24];
            flag = *flags++;
            info = texinfo[flag & 0xF];
            dy = (info & 0x3F) != 2 ? variant : 0;
            poly->clut = getClut((info & 0x3F0000) >> 12, ((info & 0x7FC00000) >> 22) + dy);
            poly->tpage = getTPage((info & 0xC0) >> 6, flag >> 5 & 3, (info & 0xF00) >> 2, (info >> 12 & 1) << 8);
            if ((info & 0x3F) == 0)
                du = du4, dv = dv4;
            else if ((info & 0x3F) == 1)
                du = du8, dv = dv8;
            else
                du = dv = 0;
            setlen(poly, 9);
            poly->code = 0x2C;
            poly->u0 += du;
            poly->v0 += dv;
            poly->u1 += du;
            poly->v1 += dv;
            poly->u2 += du;
            poly->v2 += dv;
            poly->u3 += du;
            poly->v3 += dv;
            if (flag & 0x10)
                poly->code = 0x2E;
        }
        n = p->n_ft3;
        for (i = 0; i < n; i++, out += sizeof(POLY_FT3), src += 3) {
            POLY_FT3 *poly = (POLY_FT3 *)out;
            unsigned int uv, info;
            unsigned char flag;

            *(unsigned int *)&poly->r0 = src[1];
            uv = src[2];
            *(unsigned short *)&poly->u0 = uvs[uv & 0xFF];
            *(unsigned short *)&poly->u1 = uvs[(uv & 0xFF00) >> 8];
            *(unsigned short *)&poly->u2 = uvs[uv >> 16 & 0xFF];
            flag = *flags++;
            info = texinfo[flag & 0xF];
            dy = (info & 0x3F) != 2 ? variant : 0;
            poly->clut = getClut((info & 0x3F0000) >> 12, ((info & 0x7FC00000) >> 22) + dy);
            poly->tpage = getTPage((info & 0xC0) >> 6, flag >> 5 & 3, (info & 0xF00) >> 2, (info >> 12 & 1) << 8);
            if ((info & 0x3F) == 0)
                du = du4, dv = dv4;
            else if ((info & 0x3F) == 1)
                du = du8, dv = dv8;
            else
                du = dv = 0;
            setlen(poly, 7);
            poly->code = 0x24;
            poly->u0 += du;
            poly->v0 += dv;
            poly->u1 += du;
            poly->v1 += dv;
            poly->u2 += du;
            poly->v2 += dv;
            if (flag & 0x10)
                poly->code = 0x26;
        }
        n = p->n_f3;
        for (i = 0; i < n; i++, out += sizeof(POLY_F3), src += 2) {
            POLY_F3 *poly = (POLY_F3 *)out;

            *(unsigned int *)&poly->r0 = src[1];
            setlen(poly, 4);
            poly->code = 0x20;
        }
        n = p->n_f4;
        for (i = 0; i < n; i++, out += sizeof(POLY_F4), src += 2) {
            POLY_F4 *poly = (POLY_F4 *)out;

            *(unsigned int *)&poly->r0 = src[1];
            setlen(poly, 5);
            poly->code = 0x28;
        }
        n = p->n_g3;
        for (i = 0; i < n; i++, out += sizeof(POLY_G3), src += 4) {
            POLY_G3 *poly = (POLY_G3 *)out;

            *(unsigned int *)&poly->r0 = src[1];
            *(unsigned int *)&poly->r1 = src[2];
            *(unsigned int *)&poly->r2 = src[3];
            setlen(poly, 6);
            poly->code = 0x30;
        }
        n = p->n_g4;
        for (i = 0; i < n; i++, out += sizeof(POLY_G4), src += 5) {
            POLY_G4 *poly = (POLY_G4 *)out;

            *(unsigned int *)&poly->r0 = src[1];
            *(unsigned int *)&poly->r1 = src[2];
            *(unsigned int *)&poly->r2 = src[3];
            *(unsigned int *)&poly->r3 = src[4];
            setlen(poly, 8);
            poly->code = 0x38;
        }
    }
    return prims + p->size * 2;
}

/* Draws a figure's shown parts: each with its bone's matrix, put under the figure's root matrix (work's first) unless
 * x01 is -1. */
#ifdef NON_MATCHING
/* Hand-written: the game's frame is 0x3C bytes (GCC rounds frames to 8) and its CompMatrix is
 * interleaved by hand. The C does the same with gte_CompMatrix. */
void highway_draw_figure(Figure *m)
{
    MATRIX *scratch = (MATRIX *)0x1F800000;
    unsigned int w = *(unsigned int *)m;
    unsigned int pw, i;
    Part *part;
    MATRIX *root, *bone;

    if (!(w & 0xFF))
        return;
    part = (Part *)(m->parts + m->base);
    root = (MATRIX *)m->work;
    for (i = 0; i < w >> 24; i++, part++) {
        pw = *(unsigned int *)part;
        if (!(pw & 0xFF))
            continue;
        if ((signed char)(w >> 8) != -1) {
            /* The game reaches the bone as (pw & 0xFFFF) >> 3 here but (pw & 0xFF00) >> 3 below. */
            bone = (MATRIX *)((char *)root + ((pw & 0xFFFF) >> 3));
            gte_CompMatrix(root, bone, scratch);
            gte_SetRotMatrix_r(scratch);
            gte_SetTransMatrix_r(scratch);
        } else {
            bone = (MATRIX *)((char *)root + ((pw & 0xFF00) >> 3));
            gte_SetRotMatrix_r(bone);
            gte_SetTransMatrix_r(bone);
        }
        highway_project_part(part);
    }
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_draw_figure);
#endif

/* Projects a part's vertices to the scratchpad, then for each of its polygons in this frame's buffer: puts in the
 * screen positions and, if it faces the camera, links it in the ordering table by depth (quads by their four
 * depths' sum / 16, triangles by avsz3); a polygon turned away is unlinked. */
#ifdef NON_MATCHING
/* Register allocation (the game keeps 0xFF000000/0xFFFFFF in s-registers), where the loop
 * increments and the next polygon word's load sit, and 0x48 bytes of unused frame. */
void highway_project_part(Part *p)
{
    ProjVert *pv = (ProjVert *)0x1F800008;
    ProjVert *out = (ProjVert *)0x1F800008;
    unsigned int *ot = D_800B43C0;
    unsigned int hi = 0xFF000000, lo = 0xFFFFFF;
    unsigned int i, n, count, counts, w, tag, link, *e;
    int xa, xb, xc;
    unsigned int *src;
    char *prim;
    ProjVert *a, *b, *c, *d;

    n = p->nverts;
    count = p->nverts / 3;
    src = (unsigned int *)(p->base + 4);
    for (i = 0; i < count; i++, src += 6, out += 3) {
        gte_ldv3c(src);
        gte_rtpt();
        gte_st3(out);
    }
    for (i = count * 3; i < n; i++, src += 2, out++) {
        gte_ldv0_r(src);
        gte_rtps();
        gte_st1(out);
    }
    prim = p->prims;
    if (D_800B43BC[0])
        prim += p->size;
    counts = *(unsigned int *)&p->n_gt4;
    count = counts & 0xFF;
    QUADS(POLY_GT4, 6)
    count = (counts & 0xFF00) >> 8;
    TRIS(POLY_GT3, 5)
    count = counts >> 16 & 0xFF;
    QUADS(POLY_FT4, 3)
    count = counts >> 24;
    TRIS(POLY_FT3, 3)
    counts = *(unsigned int *)&p->n_f3;
    count = counts & 0xFF;
    TRIS(POLY_F3, 2)
    count = (counts & 0xFF00) >> 8;
    QUADS(POLY_F4, 2)
    count = counts >> 16 & 0xFF;
    TRIS(POLY_G3, 4)
    count = counts >> 24;
    QUADS(POLY_G4, 5)
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_project_part);
#endif

/* Poses a figure at frame `frame` of its animation `anim`, under `root`: each bone's matrix, from its
 * parent's, its length and its angles and offsets (fixed, or that frame's), into the scratchpad and, for the bones
 * that are drawn, the figure's work buffer. Angles index the sine table, so 256 make a turn. */
#ifdef NON_MATCHING
/* Hand-written in GCC's calling style: `sub` (GCC emits subu/negu), GTE hazard slots filled with
 * work, 0x1F800022 built with ori/sll/ori and a stray nop. The C is for the PC port. */
void highway_pose_figure(Figure *m, MATRIX *root, int anim, int frame)
{
    PoseScratch *sp = (PoseScratch *)0x1F800000;
    MATRIX *w;
    MATRIX *out;
    MATRIX *parent;
    Anim *q;
    unsigned char *frame_angles;
    unsigned int *rec;
    unsigned int sa, sb, sc, ha, hb, hc, word, bone, extra, i0, i1;
    int c0, c1, c2, a, b, c;
    int i, n, count;

    if (!m->on)
        return;
    gte_SetRotMatrix_r(root);
    gte_SetTransMatrix_r(root);
    w = &sp->m[0];
    if (sp->flags & 2) {
        /* The root: the figure's position (a VECTOR at 0x08) through root, and its angles (bytes 1-3 of the word at
         * 0x04) turned by root's rotation, one column at a time. */
        gte_ldlv0((char *)m + 8);
        gte_rt();
        word = *(unsigned int *)((char *)m + 4);
        gte_stlvl(&w->t);
        sa = D_800B43C8[word >> 8 & 0xFF];
        sb = D_800B43C8[word >> 16 & 0xFF];
        sc = D_800B43C8[word >> 24];
        ha = sa >> 16;
        hb = sb >> 16;
        hc = sc >> 16;
        gte_mtc2(8, -((int)sa >> 16));
        gte_mtc2(9, sb & 0xFFFF);
        gte_mtc2(10, hb & 0xFFFF);
        gte_gpf12();
        gte_mtc2(9, gte_mfc2(25) & 0xFFFF);
        gte_mtc2(10, sa & 0xFFFF);
        gte_mtc2(11, gte_mfc2(26) & 0xFFFF);
        gte_rtir();
        c0 = gte_mfc2(9);
        c1 = gte_mfc2(10);
        c2 = gte_mfc2(11);
        gte_mtc2(8, sa);
        gte_mtc2(9, sb);
        gte_mtc2(10, hb);
        w->m[0][1] = c0;
        w->m[1][1] = c1;
        gte_gpf12();
        w->m[2][1] = c2;
        a = gte_mfc2(25) & 0xFFFF;
        b = gte_mfc2(26) & 0xFFFF;
        gte_mtc2(8, sc);
        gte_mtc2(9, a);
        gte_mtc2(10, ha);
        gte_mtc2(11, b);
        c = -sb;
        gte_gpf0();
        gte_mtc2(8, hc);
        gte_mtc2(9, hb);
        gte_mtc2(10, 0);
        gte_mtc2(11, c & 0xFFFF);
        gte_gpl0();
        gte_mac12_rtir()
        c0 = gte_mfc2(9);
        c1 = gte_mfc2(10);
        c2 = gte_mfc2(11);
        gte_mtc2(8, hc);
        gte_mtc2(9, a);
        gte_mtc2(10, ha);
        gte_mtc2(11, b);
        w->m[0][0] = c0;
        w->m[1][0] = c1;
        gte_gpf0();
        w->m[2][0] = c2;
        gte_mtc2(8, -(short)sc);
        gte_mtc2(9, hb);
        gte_mtc2(10, 0);
        gte_mtc2(11, c & 0xFFFF);
        gte_gpl0();
        gte_mac12_rtir()
        w->m[0][2] = gte_mfc2(9);
        w->m[1][2] = gte_mfc2(10);
        w->m[2][2] = gte_mfc2(11);
    } else
        *w = *root;
    q = (Anim *)(m->anims + m->base) + anim;
    sp->frame_values = (short *)(q->ofs_a + q->base) + frame;
    sp->fixed_values = (short *)(q->ofs_b + q->base);
    frame_angles = (unsigned char *)(*(unsigned short *)&q->x0A + q->base) + frame;
    n = m->nbones;
    count = q->nframes;
    rec = (unsigned int *)(q->base + 4);
    for (i = 0; i < n; i++, rec += 2) {
        /* A bone's word: its length, its parent (-1: the root) and whether it is drawn (its matrix kept). */
        bone = ((unsigned int *)m->base)[i];
        out = &sp->m[i + 1];
        parent = &sp->m[(signed char)(bone >> 16) + 1];
        w = (int)bone >> 24 ? (MATRIX *)(m->work + i * 32) : 0;
        out->t[2] = (short)bone;
        gte_SetRotMatrix_r(parent);
        gte_SetTransMatrix_r(parent);
        /* Its record: three angles, each fixed or (bits 0-2) an index into the frame's angle bytes. */
        word = rec[0];
        a = word >> 8 & 0xFF;
        b = word >> 16 & 0xFF;
        c = word >> 24;
        gte_mtc2(8, count);
        gte_mtc2(9, a);
        gte_mtc2(10, b);
        gte_mtc2(11, c);
        gte_gpf0();
        if (word & 1)
            a = frame_angles[gte_mfc2(25)];
        if (word & 2)
            b = frame_angles[gte_mfc2(26)];
        if (word & 4)
            c = frame_angles[gte_mfc2(27)];
        sa = D_800B43C8[a];
        sb = D_800B43C8[b];
        sc = D_800B43C8[c];
        ha = sa >> 16;
        hb = sb >> 16;
        hc = sc >> 16;
        gte_mtc2(8, ha);
        gte_mtc2(9, sb & 0xFFFF);
        gte_mtc2(10, hb);
        gte_gpf12();
        gte_mtc2(9, gte_mfc2(25) & 0xFFFF);
        gte_mtc2(10, -sa & 0xFFFF);
        gte_mtc2(11, gte_mfc2(26) & 0xFFFF);
        gte_rtir();
        out->t[0] = 0;
        out->t[1] = 0;
        c0 = gte_mfc2(9);
        c1 = gte_mfc2(10);
        c2 = gte_mfc2(11);
        gte_mtc2(8, sa);
        gte_mtc2(9, sb & 0xFFFF);
        gte_mtc2(10, hb);
        out->m[0][2] = c0;
        out->m[1][2] = c1;
        gte_gpf12();
        out->m[2][2] = c2;
        if (w)
            w->m[0][2] = c0;
        i0 = gte_mfc2(25) & 0xFFFF;
        i1 = gte_mfc2(26) & 0xFFFF;
        gte_mtc2(8, sc);
        gte_mtc2(9, i0);
        gte_mtc2(10, ha);
        gte_mtc2(11, i1);
        gte_gpf0();
        if (w)
            w->m[1][2] = c1;
        gte_mtc2(8, hc);
        gte_mtc2(9, hb);
        gte_mtc2(10, 0);
        gte_mtc2(11, -sb & 0xFFFF);
        gte_gpl0();
        if (w)
            w->m[2][2] = c2;
        gte_mac12_rtir()
        c0 = gte_mfc2(9);
        c1 = gte_mfc2(10);
        c2 = gte_mfc2(11);
        gte_mtc2(8, hc);
        gte_mtc2(9, i0);
        gte_mtc2(10, ha);
        gte_mtc2(11, i1);
        out->m[0][0] = c0;
        out->m[1][0] = c1;
        gte_gpf0();
        out->m[2][0] = c2;
        if (w)
            w->m[0][0] = c0;
        gte_mtc2(8, sc);
        gte_mtc2(9, -(short)hb & 0xFFFF);
        gte_mtc2(10, 0);
        gte_mtc2(11, sb & 0xFFFF);
        gte_gpl0();
        if (w)
            w->m[1][0] = c1;
        gte_mac12_rtir()
        if (w)
            w->m[2][0] = c2;
        c0 = gte_mfc2(9);
        c1 = gte_mfc2(10);
        c2 = gte_mfc2(11);
        /* Its offsets: each from the frame's values (bits 4-6), the fixed ones, or (0xFF) none; z adds to the
         * length, x and y replace. */
        if (sp->flags & 1) {
            gte_mtc2(8, count);
            extra = rec[1];
            if (word & 0x40) {
                gte_mtc2(9, (extra & 0xFF0000) >> 16);
                gte_gpf0();
                out->t[2] += sp->frame_values[gte_mfc2(25)];
            } else if ((extra & 0xFF0000) != 0xFF0000)
                out->t[2] += sp->fixed_values[(extra & 0xFF0000) >> 16];
            if (word & 0x10) {
                gte_mtc2(9, extra & 0xFF);
                gte_gpf0();
                out->t[0] = sp->frame_values[gte_mfc2(25)];
            } else if ((extra & 0xFF) != 0xFF)
                out->t[0] = sp->fixed_values[extra & 0xFF];
            if (word & 0x20) {
                gte_mtc2(9, (extra & 0xFF00) >> 8);
                gte_gpf0();
                out->t[1] = sp->frame_values[gte_mfc2(25)];
            } else if ((extra & 0xFF00) != 0xFF00)
                out->t[1] = sp->fixed_values[(extra & 0xFF00) >> 8];
        }
        /* The place: the offsets through the parent's matrix. */
        gte_ldlv0(&out->t);
        out->m[0][1] = c0;
        out->m[1][1] = c1;
        gte_rt();
        out->m[2][1] = c2;
        if (w) {
            w->m[0][1] = c0;
            w->m[1][1] = c1;
            w->m[2][1] = c2;
        }
        gte_stlvl(&out->t);
        if (w)
            gte_stlvl(&w->t);
    }
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_pose_figure);
#endif

/* Scales a whole figure by scale (0x1000 is 1): its parts' vertices, its bones' lengths and its animations. */
void highway_scale_figure(Figure *m, short scale, int force)
{
    MATRIX *mat = (MATRIX *)0x1F800000;
    SVECTOR *in = (SVECTOR *)0x1F800020;
    VECTOR *out = (VECTOR *)0x1F800028;
    unsigned int i;
    unsigned int n;
    Part *parts;
    Anim *pieces;
    short *len;

    n = m->nparts;
    parts = (Part *)(m->parts + m->base);
    for (i = 0; i < n; i++)
        highway_scale_part(&parts[i], scale, force);
    mat->m[0][0] = scale;
    mat->m[1][1] = scale;
    mat->m[2][2] = scale;
    mat->t[0] = mat->t[1] = mat->t[2] = 0;
    mat->m[0][1] = mat->m[0][2] = mat->m[1][0] = mat->m[1][2] = mat->m[2][0] = mat->m[2][1] = 0;
    gte_SetRotMatrix_r(mat);
    gte_SetTransMatrix_r(mat);
    /* The lengths are ints; their low halves are turned three at a time, the rest one by one. */
    n = m->nbones / 3;
    len = (short *)m->base;
    for (i = 0; i < n; i++) {
        in->vx = len[i * 6];
        in->vy = len[i * 6 + 2];
        in->vz = len[i * 6 + 4];
        gte_ldv0_r(in);
        gte_rt();
        gte_stlvnl_r(out);
        len[i * 6] = out->vx;
        len[i * 6 + 2] = out->vy;
        len[i * 6 + 4] = out->vz;
    }
    for (i = n * 3; i < m->nbones; i++) {
        in->vx = len[i * 2];
        gte_ldv0_r(in);
        gte_rt();
        gte_stlvnl_r(out);
        len[i * 2] = out->vx;
    }
    n = m->nanims;
    pieces = (Anim *)(m->anims + m->base);
    for (i = 0; i < n; i++)
        highway_scale_anim(&pieces[i], scale, force);
}

/* Scales a part's vertices by scale (0x1000 is 1), once: the flag word before them says it is done, unless forced. */
void highway_scale_part(Part *p, short scale, int force)
{
    MATRIX *m = (MATRIX *)0x1F800000;
    VECTOR *out = (VECTOR *)0x1F800020;
    SVECTOR *v;
    unsigned int i, n;

    if ((*(int *)p->base & 1) && !force)
        return;
    m->m[0][0] = scale;
    m->m[1][1] = scale;
    m->m[2][2] = scale;
    m->t[0] = m->t[1] = m->t[2] = 0;
    m->m[0][1] = m->m[0][2] = m->m[1][0] = m->m[1][2] = m->m[2][0] = m->m[2][1] = 0;
    gte_SetRotMatrix_r(m);
    gte_SetTransMatrix_r(m);
    n = p->nverts;
    v = (SVECTOR *)(p->base + 4);
    for (i = 0; i < n; i++) {
        gte_ldv0_r(&v[i]);
        gte_rt();
        gte_stlvnl_r(out);
        v[i].vx = out->vx;
        v[i].vy = out->vy;
        v[i].vz = out->vz;
    }
    *(int *)p->base |= 1;
}

/* Scales an animation's coordinates (shorts, by threes where they can) by scale (0x1000 is 1), once: the flag word
 * before them says it is done, unless forced. */
#ifdef NON_MATCHING
/* Register choice only: the outer counter and the group pointer (a2/t3), and the second
 * section's counter and offset (a1/a2), are swapped. */
void highway_scale_anim(Anim *q, short scale, int force)
{
    MATRIX *m = (MATRIX *)0x1F800000;
    SVECTOR *in = (SVECTOR *)0x1F800000;
    VECTOR *out = (VECTOR *)0x1F800008;
    unsigned int g, i, n, n3, count;
    Short3 *v;
    short *s;

    if (*(int *)q->base && !force)
        return;
    m->m[0][0] = scale;
    m->m[1][1] = scale;
    m->m[2][2] = scale;
    m->t[0] = m->t[1] = m->t[2] = 0;
    m->m[0][1] = m->m[0][2] = m->m[1][0] = m->m[1][2] = m->m[2][0] = m->m[2][1] = 0;
    gte_SetRotMatrix_r(m);
    gte_SetTransMatrix_r(m);
    n = q->groups;
    count = q->nframes;
    for (g = 0; g < n; g++) {
        s = (short *)(q->base + q->ofs_a) + g * count;
        v = (Short3 *)s;
        n3 = count / 3;
        for (i = 0; i < n3; i++) {
            in->vx = v[i].x;
            in->vy = v[i].y;
            in->vz = v[i].z;
            gte_ldv0_r(in);
            gte_rt();
            gte_stlvnl_r(out);
            v[i].x = out->vx;
            v[i].y = out->vy;
            v[i].z = out->vz;
        }
        for (i = n3 * 3; i < count; i++) {
            in->vx = s[i];
            gte_ldv0_r(in);
            gte_rt();
            gte_stlvnl_r(out);
            s[i] = out->vx;
        }
    }
    n = q->n_b;
    n3 = n / 3;
    for (i = 0; i < n3; i++) {
        v = (Short3 *)(q->base + q->ofs_b);
        in->vx = v[i].x;
        in->vy = v[i].y;
        in->vz = v[i].z;
        gte_ldv0_r(in);
        gte_rt();
        gte_stlvnl_r(out);
        v[i].x = out->vx;
        v[i].y = out->vy;
        v[i].z = out->vz;
    }
    for (i = n3 * 3; i < n; i++) {
        s = (short *)(q->base + q->ofs_b);
        in->vx = s[i];
        gte_ldv0_r(in);
        gte_rt();
        gte_stlvnl_r(out);
        s[i] = out->vx;
    }
    *(int *)q->base = 1;
}
#else
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_scale_anim);
#endif

int highway_tint_figure(Figure *m, unsigned char *b)
{
    SVECTOR unused; /* never used, but the game keeps its stack room */
    unsigned int i;
    unsigned int n = m->nparts;
    Part *parts = (Part *)(m->base + m->parts);
    short x = b[1] << 8 | b[0], y = b[3] << 8 | b[2], z = b[5] << 8 | b[4];

    *(int *)0x1F800200 = b[6];
    for (i = 0; i < n; i++)
        highway_tint_part(&parts[i], x, y, z);
    return 1;
}

/* Tints a part's primitives in the buffer being drawn: each corner's colour from its polygon plus (x, y, z) (an RGB
 * offset), keeping the primitives' kinds. With bit 0 of the scratchpad flag clear, only linked primitives. */
void highway_tint_part(Part *p, short x, short y, short z)
{
    char *out = p->prims;
    PolyRGB *src = (PolyRGB *)(p->base + p->polys);
    int r, g, b;
    unsigned int i, n, counts;
    unsigned char code;

    if (D_800B43BC[0])
        out += p->size;
    gte_ldir0(16);
    r = x << 4;
    g = y << 4;
    b = z << 4;
    if (*(int *)0x1F800200 & 1) {
        counts = *(unsigned int *)&p->n_gt4;
        n = counts & 0xFF;
        for (i = 0; i < n; i++, out += sizeof(POLY_GT4), src = (PolyRGB *)((char *)src + 0x18)) {
            POLY_GT4 *poly = (POLY_GT4 *)out;

            code = poly->code;
            TINT(src->c[0]);
            TINT(src->c[1]);
            TINT(src->c[2]);
            gte_strgb(20, &poly->r0);
            gte_strgb(21, &poly->r1);
            gte_strgb(22, &poly->r2);
            TINT(src->c[3]);
            gte_strgb(22, &poly->r3);
            poly->code = code;
        }
        n = (counts & 0xFF00) >> 8;
        for (i = 0; i < n; i++, out += sizeof(POLY_GT3), src = (PolyRGB *)((char *)src + 0x14)) {
            POLY_GT3 *poly = (POLY_GT3 *)out;

            code = poly->code;
            TINT(src->c[0]);
            TINT(src->c[1]);
            TINT(src->c[2]);
            gte_strgb(20, &poly->r0);
            gte_strgb(21, &poly->r1);
            gte_strgb(22, &poly->r2);
            poly->code = code;
        }
        n = counts >> 16 & 0xFF;
        for (i = 0; i < n; i++, out += sizeof(POLY_FT4), src = (PolyRGB *)((char *)src + 0xC)) {
            POLY_FT4 *poly = (POLY_FT4 *)out;

            code = poly->code;
            TINT(src->c[0]);
            gte_strgb(22, &poly->r0);
            poly->code = code;
        }
        n = counts >> 24;
        for (i = 0; i < n; i++, out += sizeof(POLY_FT3), src = (PolyRGB *)((char *)src + 0xC)) {
            POLY_FT3 *poly = (POLY_FT3 *)out;

            code = poly->code;
            TINT(src->c[0]);
            gte_strgb(22, &poly->r0);
            poly->code = code;
        }
        counts = *(unsigned int *)&p->n_f3;
        n = counts & 0xFF;
        for (i = 0; i < n; i++, out += sizeof(POLY_F3), src = (PolyRGB *)((char *)src + 8)) {
            POLY_F3 *poly = (POLY_F3 *)out;

            code = poly->code;
            TINT(src->c[0]);
            gte_strgb(22, &poly->r0);
            poly->code = code;
        }
        n = (counts & 0xFF00) >> 8;
        for (i = 0; i < n; i++, out += sizeof(POLY_F4), src = (PolyRGB *)((char *)src + 8)) {
            POLY_F4 *poly = (POLY_F4 *)out;

            code = poly->code;
            TINT(src->c[0]);
            gte_strgb(22, &poly->r0);
            poly->code = code;
        }
        n = counts >> 16 & 0xFF;
        for (i = 0; i < n; i++, out += sizeof(POLY_G3), src = (PolyRGB *)((char *)src + 0x10)) {
            POLY_G3 *poly = (POLY_G3 *)out;

            code = poly->code;
            TINT(src->c[0]);
            TINT(src->c[1]);
            TINT(src->c[2]);
            gte_strgb(20, &poly->r0);
            gte_strgb(21, &poly->r1);
            gte_strgb(22, &poly->r2);
            poly->code = code;
        }
        n = counts >> 24;
        for (i = 0; i < n; i++, out += sizeof(POLY_G4), src = (PolyRGB *)((char *)src + 0x14)) {
            POLY_G4 *poly = (POLY_G4 *)out;

            code = poly->code;
            TINT(src->c[0]);
            TINT(src->c[1]);
            TINT(src->c[2]);
            gte_strgb(20, &poly->r0);
            gte_strgb(21, &poly->r1);
            gte_strgb(22, &poly->r2);
            TINT(src->c[3]);
            gte_strgb(22, &poly->r3);
            poly->code = code;
        }
        return;
    }
    /* Only the primitives linked into an ordering table this frame (a tag's address is 0 otherwise). */
    counts = *(unsigned int *)&p->n_gt4;
    n = counts & 0xFF;
    for (i = 0; i < n; i++, out += sizeof(POLY_GT4), src = (PolyRGB *)((char *)src + 0x18)) {
        POLY_GT4 *poly = (POLY_GT4 *)out;

        if (!(poly->tag << 8))
            continue;
        code = poly->code;
        TINT(src->c[0]);
        TINT(src->c[1]);
        TINT(src->c[2]);
        gte_strgb(20, &poly->r0);
        gte_strgb(21, &poly->r1);
        gte_strgb(22, &poly->r2);
        TINT(src->c[3]);
        gte_strgb(22, &poly->r3);
        poly->code = code;
    }
    n = (counts & 0xFF00) >> 8;
    for (i = 0; i < n; i++, out += sizeof(POLY_GT3), src = (PolyRGB *)((char *)src + 0x14)) {
        POLY_GT3 *poly = (POLY_GT3 *)out;

        if (!(poly->tag << 8))
            continue;
        code = poly->code;
        TINT(src->c[0]);
        TINT(src->c[1]);
        TINT(src->c[2]);
        gte_strgb(20, &poly->r0);
        gte_strgb(21, &poly->r1);
        gte_strgb(22, &poly->r2);
        poly->code = code;
    }
    n = counts >> 16 & 0xFF;
    for (i = 0; i < n; i++, out += sizeof(POLY_FT4), src = (PolyRGB *)((char *)src + 0xC)) {
        POLY_FT4 *poly = (POLY_FT4 *)out;

        if (!(poly->tag << 8))
            continue;
        code = poly->code;
        TINT(src->c[0]);
        gte_strgb(22, &poly->r0);
        poly->code = code;
    }
    n = counts >> 24;
    for (i = 0; i < n; i++, out += sizeof(POLY_FT3), src = (PolyRGB *)((char *)src + 0xC)) {
        POLY_FT3 *poly = (POLY_FT3 *)out;

        if (!(poly->tag << 8))
            continue;
        code = poly->code;
        TINT(src->c[0]);
        gte_strgb(22, &poly->r0);
        poly->code = code;
    }
    counts = *(unsigned int *)&p->n_f3;
    n = counts & 0xFF;
    for (i = 0; i < n; i++, out += sizeof(POLY_F3), src = (PolyRGB *)((char *)src + 8)) {
        POLY_F3 *poly = (POLY_F3 *)out;

        if (!(poly->tag << 8))
            continue;
        code = poly->code;
        TINT(src->c[0]);
        gte_strgb(22, &poly->r0);
        poly->code = code;
    }
    n = (counts & 0xFF00) >> 8;
    for (i = 0; i < n; i++, out += sizeof(POLY_F4), src = (PolyRGB *)((char *)src + 8)) {
        POLY_F4 *poly = (POLY_F4 *)out;

        if (!(poly->tag << 8))
            continue;
        code = poly->code;
        TINT(src->c[0]);
        gte_strgb(22, &poly->r0);
        poly->code = code;
    }
    n = counts >> 16 & 0xFF;
    for (i = 0; i < n; i++, out += sizeof(POLY_G3), src = (PolyRGB *)((char *)src + 0x10)) {
        POLY_G3 *poly = (POLY_G3 *)out;

        if (!(poly->tag << 8))
            continue;
        code = poly->code;
        TINT(src->c[0]);
        TINT(src->c[1]);
        TINT(src->c[2]);
        gte_strgb(20, &poly->r0);
        gte_strgb(21, &poly->r1);
        gte_strgb(22, &poly->r2);
        poly->code = code;
    }
    n = counts >> 24;
    for (i = 0; i < n; i++, out += sizeof(POLY_G4), src = (PolyRGB *)((char *)src + 0x14)) {
        POLY_G4 *poly = (POLY_G4 *)out;

        if (!(poly->tag << 8))
            continue;
        code = poly->code;
        TINT(src->c[0]);
        TINT(src->c[1]);
        TINT(src->c[2]);
        gte_strgb(20, &poly->r0);
        gte_strgb(21, &poly->r1);
        gte_strgb(22, &poly->r2);
        TINT(src->c[3]);
        gte_strgb(22, &poly->r3);
        poly->code = code;
    }
}

/* D_800B3BF8: a word the game keeps here in the code, before the hand-written GTE routines (highway_vsync_count sets
 * it). */
#ifdef NATIVE
int D_800B3BF8;
#else
__asm__(".word 0");
#endif

/* For each 0x24-byte Face of a DrawArgs: rtps per vertex, nclip, skipping back faces and OTZ outside
 * 1..0x1000; a depth-cued POLY_G3 (dpcs) linked at ot[OTZ]. Returns the next primitive. */
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_draw_g3_list_cued);

/* As highway_draw_g3_list_cued without depth cueing or depth: every primitive goes into the one OT entry. */
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_draw_g3_list);

/* One triangle: rtpt, nclip; if it faces the camera, a grey semi-transparent POLY_GT3 into the fixed
 * OT entry. Returns prim + 0x28, or prim unchanged. */
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_draw_gt3);

/* As highway_draw_gt3 with a POLY_FT3 (0x20 bytes). */
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_draw_ft3);

/* As highway_draw_gt3, linked at ot[OTZ] (avsz3) and skipped when OTZ <= 0. */
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_draw_gt3_z);

/* As highway_draw_ft3, linked at ot[OTZ] and skipped when OTZ <= 0. */
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_draw_ft3_z);

/* Turns and projects three points: their screen positions to xy[0..2]. */
void highway_project3(SVECTOR *v, int *xy)
{
    gte_ldv3c(v);
    gte_rtpt();
    gte_stsxy3c(xy);
}

/* If the largest of the ints at +0x10, +0x14, +0x50 and +0x54 is <= 0x1000, copies four UV words
 * into a POLY_FT4. */
INCLUDE_ASM("asm/overlay/highway/nonmatchings/highway", highway_set_ft4_uvs_near);
