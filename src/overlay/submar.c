/* MINI/SUBMAR.BIN, the submarine, 0x800A00BC-0x800AD330 (its data follows to 0x800B2E38). GCC 2.6.3, ASPSX 2.21 with
 * its divide checks, -G0 -fforce-addr. Still asm without C: the two copies each of libgte's ratan2 and of a vector
 * normalise. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "gte.h"

int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int rand(void);
void srand(unsigned int seed);

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
/* The camera's move between views: its angles, distance and height from, to and the difference, and how far along
 * it is (0x1000 is there). */
typedef struct CamMove {
    SVECTOR from, to, delta;
    int dist_from, dist_to, dist_delta;
    int height_from, height_to, height_delta;
    int t, step, active;
} CamMove;
extern CamMove D_801BA30C;
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
extern unsigned int D_800AE3C8, D_800AE3F0; /* the quit menu's two choices' colours */
extern unsigned int D_801BCF08[20], D_801BCF58[8]; /* the HUD's ordering tables */
typedef struct OtChain {
    unsigned int *first, *last;
} OtChain;
extern OtChain D_800AE5A4[];
typedef struct PathNode {
    int x, y, z;
    int x0C, radius, speed;
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
extern VECTOR D_800B5F3C; /* the up vector the subs' side axis is crossed with */
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
/* A variable the game's code keeps in a given register (the PC build lets its compiler choose). */
#ifdef NATIVE
#define FIXED_REG(r)
#else
#define FIXED_REG(r) asm(r)
#endif
/* Single GTE register moves and commands, one asm statement each, as SUBMAR.BIN's own code is built from them (the
 * command words whole: 0x4A486012 is rtv0, 0x4A48E012 rtv1, 0x4A496012 rtv2). */
#ifdef NATIVE
unsigned int gte_read_ctrl(int reg);
#define gte_ctc2(reg, v) gte_write_ctrl(reg, (v))
#define gte_mtc2(reg, v) gte_write(reg, (v))
#define gte_mfc2(reg) ((int)gte_read(reg))
#define gte_cfc2(reg) ((int)gte_read_ctrl(reg))
#define gte_op(cmd) gte_cmd((cmd) & 0x1FFFFFF)
#define gte_ldlvl_m(v) gte_ldlvl(v)
#define gte_ctc2_02(a, b) (gte_write_ctrl(0, (a)), gte_write_ctrl(2, (b)))
#define gte_mfc2_late(reg) ((int)gte_read(reg))
#define gte_stlvnl_m(p) (((int *)(p))[0] = gte_read(25), ((int *)(p))[1] = gte_read(26), ((int *)(p))[2] = gte_read(27))
#define gte_lwc2_m(reg, x) gte_write(reg, (x))
#define gte_stmac(a, b, c) ((a) = gte_read(25), (b) = gte_read(26), (c) = gte_read(27))
#define gte_stlzc(a) ((a) = gte_read(31))
#define gte_ldv1(v) (gte_write(2, ((unsigned int *)(v))[0]), gte_write(3, ((unsigned int *)(v))[1]))
#define gte_op_now(cmd) gte_cmd((cmd) & 0x1FFFFFF)
#define gte_rtpt() gte_cmd(0x280030)
#define gte_stsxy0(p) (*(unsigned int *)(p) = gte_read(12))
#define gte_stsxy1(p) (*(unsigned int *)(p) = gte_read(13))
#define gte_rtps() gte_cmd(0x180001)
#define gte_stir3(a, b, c) ((a) = gte_read(9), (b) = gte_read(10), (c) = gte_read(11))
#define gte_stir2(a, b) ((a) = gte_read(9), (b) = gte_read(10))
#define gte_ldsv(v)                                                                                                    \
    (gte_write(9, ((unsigned short *)(v))[0]), gte_write(10, ((unsigned short *)(v))[1]),                              \
     gte_write(11, ((unsigned short *)(v))[2]))
/* IR1-3 as V0's or V1's or V2's three shorts. */
#define gte_ir_to_v(n, x, y, z) (gte_write(2 * (n), ((x) & 0xFFFF) | (y) << 16), gte_write(2 * (n) + 1, (z) & 0xFFFF))
#define gte_rot_v12()                                                                                                  \
    {                                                                                                                  \
        unsigned int x = gte_read(9), y = gte_read(10), z = gte_read(11);                                              \
        gte_cmd(0x48E012);                                                                                             \
        gte_ir_to_v(0, x, y, z);                                                                                       \
        x = gte_read(9), y = gte_read(10), z = gte_read(11);                                                           \
        gte_cmd(0x496012);                                                                                             \
        gte_ir_to_v(1, x, y, z);                                                                                       \
        gte_ir_to_v(2, gte_read(9), gte_read(10), gte_read(11));                                                       \
    }
#define gte_store_cols(m)                                                                                              \
    {                                                                                                                  \
        short *c = (short *)(m);                                                                                       \
        c[0] = gte_read(9), c[3] = gte_read(10), c[6] = gte_read(11);                                                  \
        gte_cmd(0x48E012);                                                                                             \
        c[1] = gte_read(9), c[4] = gte_read(10), c[7] = gte_read(11);                                                  \
        gte_cmd(0x496012);                                                                                             \
        c[2] = gte_read(9), c[5] = gte_read(10), c[8] = gte_read(11);                                                  \
    }
#define gte_rot_v12_b() gte_rot_v12()
#define gte_store_svecs_b(v) gte_store_svecs(v)
#define gte_store_svecs(v)                                                                                             \
    {                                                                                                                  \
        short *c = (short *)(v);                                                                                       \
        c[0] = gte_read(9), c[1] = gte_read(10), c[2] = gte_read(11);                                                  \
        gte_cmd(0x48E012);                                                                                             \
        c[4] = gte_read(9), c[5] = gte_read(10), c[6] = gte_read(11);                                                  \
        gte_cmd(0x496012);                                                                                             \
        c[8] = gte_read(9), c[9] = gte_read(10), c[10] = gte_read(11);                                                 \
    }
#else
#define gte_ctc2(reg, v) __asm__ volatile("ctc2 %z0, $" #reg : : "rJ"(v))
#define gte_mtc2(reg, v) __asm__ volatile("mtc2 %z0, $" #reg : : "rJ"(v))
#define gte_mfc2(reg)                                                                                                  \
    ({                                                                                                                 \
        int _v;                                                                                                        \
        __asm__ volatile("mfc2 %0, $" #reg : "=r"(_v));                                                                \
        _v;                                                                                                            \
    })
#define gte_cfc2(reg)                                                                                                  \
    ({                                                                                                                 \
        int _v;                                                                                                        \
        __asm__ volatile("cfc2 %0, $" #reg : "=r"(_v));                                                                \
        _v;                                                                                                            \
    })
#define gte_op(cmd) __asm__ volatile("nop\nnop\n.word " #cmd)
#define gte_ldlvl_m(v)                                                                                                 \
    __asm__ volatile("lwc2 $9, %0\nlwc2 $10, %1\nlwc2 $11, %2"                                                       \
                     :                                                                                                 \
                     : "m"(((int *)(v))[0]), "m"(((int *)(v))[1]), "m"(((int *)(v))[2]))
#define gte_ctc2_02(a, b) __asm__ volatile("ctc2 %0, $0\nctc2 %1, $2" : : "r"(a), "r"(b))
#define gte_mfc2_late(reg)                                                                                             \
    ({                                                                                                                 \
        int _v;                                                                                                        \
        __asm__ volatile("nop\nnop\nmfc2 %0, $" #reg : "=r"(_v));                                                      \
        _v;                                                                                                            \
    })
#define gte_stlvnl_m(p)                                                                                                \
    __asm__ volatile("swc2 $25, %0\nswc2 $26, %1\nswc2 $27, %2"                                                      \
                     : "=m"(((int *)(p))[0]), "=m"(((int *)(p))[1]), "=m"(((int *)(p))[2]))
#define gte_lwc2_m(reg, x) __asm__ volatile("lwc2 $" #reg ", %0" : : "m"(x))
#define gte_stmac(a, b, c) __asm__ volatile("mfc2 %0, $25\nmfc2 %1, $26\nmfc2 %2, $27" : "=r"(a), "=r"(b), "=r"(c))
#define gte_stlzc(a) __asm__ volatile("mfc2 %0, $31\nnop" : "=r"(a))
#undef gte_ldv0
#define gte_ldv0(v) __asm__ volatile("lwc2 $0, 0(%0)\nlwc2 $1, 4(%0)" : : "r"(v))
#define gte_ldv1(v) __asm__ volatile("lwc2 $2, 0(%0)\nlwc2 $3, 4(%0)" : : "r"(v))
#define gte_op_now(cmd) __asm__ volatile(".word " #cmd)
#define gte_rtpt() __asm__ volatile("nop\nnop\n.word 0x4A280030")
#define gte_stsxy0(p) __asm__ volatile("swc2 $12, %0" : "=m"(*(unsigned int *)(p)))
#define gte_stsxy1(p) __asm__ volatile("swc2 $13, %0" : "=m"(*(unsigned int *)(p)))
#define gte_rtps() __asm__ volatile("nop\nnop\n.word 0x4A180001\nnop")
#define gte_stir3(a, b, c) __asm__ volatile("mfc2 %0, $9\nmfc2 %1, $10\nmfc2 %2, $11" : "=r"(a), "=r"(b), "=r"(c))
#define gte_stir2(a, b) __asm__ volatile("mfc2 %0, $9\nmfc2 %1, $10" : "=r"(a), "=r"(b))
#define gte_ldsv(v)                                                                                                    \
    __asm__ volatile("lhu $12, 0(%0)\nlhu $13, 2(%0)\nlhu $14, 4(%0)\nmtc2 $12, $9\nmtc2 $13, $10\nmtc2 $14, $11"     \
                     :                                                                                                 \
                     : "r"(v)                                                                                          \
                     : "$12", "$13", "$14")
/* V0 is turned already: turn V1 and V2, and put the three results back in V0-V2 (the next matrix's columns). */
#define GTE_ROT_V12(w)                                                                                                 \
    __asm__ volatile("mfc2 $12, $9\nmfc2 $13, $10\nmfc2 $14, $11\n" w ".word 0x4A48E012\n"                        \
                     "andi $12, $12, 0xFFFF\nsll $13, $13, 16\nor $12, $12, $13\nandi $14, $14, 0xFFFF\n"              \
                     "mtc2 $12, $0\nmtc2 $14, $1\n"                                                                    \
                     "mfc2 $12, $9\nmfc2 $13, $10\nmfc2 $14, $11\n" w ".word 0x4A496012\n"                        \
                     "andi $12, $12, 0xFFFF\nsll $13, $13, 16\nor $12, $12, $13\nandi $14, $14, 0xFFFF\n"              \
                     "mtc2 $12, $2\nmtc2 $14, $3\n"                                                                    \
                     "mfc2 $12, $9\nmfc2 $13, $10\nmfc2 $14, $11\n"                                                    \
                     "andi $12, $12, 0xFFFF\nsll $13, $13, 16\nor $12, $12, $13\nandi $14, $14, 0xFFFF\n"              \
                     "mtc2 $12, $4\nmtc2 $14, $5" : :)
/* V0 is turned already: store the three results as m's columns, turning V1 and V2 between. */
#define GTE_STORE_COLS(m, w)                                                                                           \
    __asm__ volatile("mfc2 $12, $9\nmfc2 $13, $10\nmfc2 $14, $11\n" w ".word 0x4A48E012\n"                        \
                     "sh $12, 0(%0)\nsh $13, 6(%0)\nsh $14, 12(%0)\n"                                                  \
                     "mfc2 $12, $9\nmfc2 $13, $10\nmfc2 $14, $11\n" w ".word 0x4A496012\n"                        \
                     "sh $12, 2(%0)\nsh $13, 8(%0)\nsh $14, 14(%0)\n"                                                  \
                     "mfc2 $12, $9\nmfc2 $13, $10\nmfc2 $14, $11\n"                                                    \
                     "sh $12, 4(%0)\nsh $13, 10(%0)\nsh $14, 16(%0)"                                                   \
                     :                                                                                                 \
                     : "r"(m)                                                                                          \
                     : "$12", "$13", "$14", "memory")
/* The same as three SVECTORs. */
#define GTE_STORE_SVECS(v, w)                                                                                          \
    __asm__ volatile("mfc2 $12, $9\nmfc2 $13, $10\nmfc2 $14, $11\n" w ".word 0x4A48E012\n"                        \
                     "sh $12, 0(%0)\nsh $13, 2(%0)\nsh $14, 4(%0)\n"                                                   \
                     "mfc2 $12, $9\nmfc2 $13, $10\nmfc2 $14, $11\n" w ".word 0x4A496012\n"                        \
                     "sh $12, 8(%0)\nsh $13, 10(%0)\nsh $14, 12(%0)\n"                                                 \
                     "mfc2 $12, $9\nmfc2 $13, $10\nmfc2 $14, $11\n"                                                    \
                     "sh $12, 16(%0)\nsh $13, 18(%0)\nsh $14, 20(%0)"                                                  \
                     :                                                                                                 \
                     : "r"(v)                                                                                          \
                     : "$12", "$13", "$14", "memory")
/* The second source file's copies of them leave the GTE no gap before the commands. */
#define gte_rot_v12() GTE_ROT_V12("nop\nnop\n")
#define gte_rot_v12_b() GTE_ROT_V12("")
#define gte_store_cols(m) GTE_STORE_COLS(m, "nop\nnop\n")
#define gte_store_svecs(v) GTE_STORE_SVECS(v, "nop\nnop\n")
#define gte_store_svecs_b(v) GTE_STORE_SVECS(v, "")
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
    SVECTOR dir; /* its heading as a unit vector */
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
    SVECTOR to_player;
    void *kind;
    PathNode *node;
    int node_dist;
    unsigned int tubes[4];
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

extern unsigned short D_800ADD10[6];
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
    SVECTOR up;
    short roll[3];
    short x3A;
    short tilt[3];
    short x42;
    short cam[3];
    short x4A;
    short pitch2, heading2;
    short x50;
    char x52[0xA];
    short pitch3;
    short heading3;
    char x60[4];
    SVECTOR dir;
    int view;
    int view_toggle;
    int dist;
    char x78[0x10];
    SVECTOR ahead;
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
    SVECTOR axes[3];
    SVECTOR x40;
    SVECTOR x48;
    SVECTOR to_player;
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
void submar_init_hud(void);
void sub_800A832C(void);
void submar_probe_floor(void);
void sub_800A32C8(void);
void sub_800A36F4(void);
void sub_800A9130(void);
void *sub_800A1F9C(void);
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

/* A polyline of two segments, as the sub markers use it (libgpu's LINE_F3 with its colour and code as one word). */
typedef struct LineF3 {
    unsigned int tag;
    unsigned int rgbc;
    short x0, y0, x1, y1, x2, y2;
    unsigned int end;
} LineF3;
extern unsigned short D_800AD8F4[8];
/* setaddr as the marker code writes it: the length byte read on its own, the address cut to 24 bits by shifts. */
#ifdef NATIVE
#define setaddr_sh(p, a) setaddr(p, a)
#else
#define setaddr_sh(p, a) (*(unsigned int *)(p) = ((P_TAG *)(p))->len << 24 | (unsigned int)(a) << 8 >> 8)
#endif

/* Each sub's marker: two lines boxing where it shows on screen, coloured by its state. */

/* A line primitive with its points as words (LINE_F3 or, without the last two words, LINE_F2). */
typedef struct LineXY {
    unsigned int tag;
    unsigned int rgbc;
    int xy0, xy1, xy2;
    unsigned int end;
} LineXY;
extern unsigned char D_801C2880[2][0x1440];

/* Each sub's depth line: from the sea floor under it down to it, in steps of 64, shaded by depth and coloured by its
 * state. */

extern unsigned char D_801C5100[2][0x300];

/* The player's depth line: from the sea floor under the sub down to it, the first step what takes it to a multiple of
 * 64 and the rest 64 at most, shaded by depth. */

extern unsigned short D_800AD48C[0xC0], D_800AD60C[0xC0];
extern SVECTOR D_800AE8EC, D_800AE8FC;

/* The torpedo's line to the player: its offset, its length, and how far it lies along the player's two axes. */

int submar_ratan2(int y, int x);
int submar_normalize(VECTOR *v);
extern VECTOR D_800AD410, D_800AD430;

/* Each sub turns to face along its path: its heading and pitch from the direction to its next node, the side axis
 * from that and the up vector (or a fallback when they line up). */

int submar_ratan2_b(int y, int x);
int submar_normalize_b(VECTOR *v);

/* Each torpedo turns to face along its course, as sub_800A32C8 turns the subs. */

/* A corner of a shaded triangle: where it is, and its colour. */
typedef struct ShadeVert {
    SVECTOR v;
    unsigned int rgb;
} ShadeVert;
/* libgpu's POLY_G3 with each colour and point as one word. */
typedef struct PolyG3W {
    unsigned int tag;
    unsigned int rgbc0;
    int xy0;
    unsigned int rgb1;
    int xy1;
    unsigned int rgb2;
    int xy2;
} PolyG3W;
#ifdef NATIVE
#define gte_ldv2(v) (gte_write(4, ((unsigned int *)(v))[0]), gte_write(5, ((unsigned int *)(v))[1]))
#define gte_ldv3c(v)                                                                                                   \
    (gte_write(0, ((unsigned int *)(v))[0]), gte_write(1, ((unsigned int *)(v))[1]),                                  \
     gte_write(2, ((unsigned int *)(v))[2]), gte_write(3, ((unsigned int *)(v))[3]),                                  \
     gte_write(4, ((unsigned int *)(v))[4]), gte_write(5, ((unsigned int *)(v))[5]))
#define gte_strgb2(p) (*(unsigned int *)(p) = gte_read(22))
#else
#define gte_ldv2(v) __asm__ volatile("lwc2 $4, 0(%0)\nlwc2 $5, 4(%0)" : : "r"(v))
/* libgte's gte_ldv3c: three SVECTORs in a row to V0-V2. */
#define gte_ldv3c(v)                                                                                                   \
    __asm__ volatile("lwc2 $0, 0(%0)\nlwc2 $1, 4(%0)\nlwc2 $2, 8(%0)\nlwc2 $3, 12(%0)\nlwc2 $4, 16(%0)\n"              \
                     "lwc2 $5, 20(%0)"                                                                                 \
                     :                                                                                                 \
                     : "r"(v))
#define gte_strgb2(p) __asm__ volatile("swc2 $22, %0" : "=m"(*(unsigned int *)(p)))
#endif

/* The work area the sea floor's triangles are drawn from, in the scratchpad. */
typedef struct DrawScratch {
    char x00[0xC];
    unsigned int code; /* the primitives' code, in the top byte */
    char x10[0x30];
    unsigned int *ot;
    PolyG3W *prim;
    int x48;
    int z;
    char x50[0x18];
    int near; /* screen depths below this are cut */
    char x6C[0x188];
    int clip;     /* a bit for each point nearer than near */
    int depth[3]; /* the points' depths, the clipped first */
} DrawScratch;
void sub_800ACF6C(ShadeVert *a, ShadeVert *b, ShadeVert *c);
void sub_800ACCA8(ShadeVert *a, ShadeVert *b, ShadeVert *c);
#ifdef NATIVE
#define gte_stsxy3r(a, b, c) ((a) = gte_read(12), (b) = gte_read(13), (c) = gte_read(14))
#define gte_ldv3(a, b, c)                                                                                              \
    (gte_ldv0(a), gte_write(2, ((unsigned int *)(b))[0]), gte_write(3, ((unsigned int *)(b))[1]),                      \
     gte_write(4, ((unsigned int *)(c))[0]), gte_write(5, ((unsigned int *)(c))[1]))
#else
#define gte_stsxy3r(a, b, c) __asm__ volatile("mfc2 %0, $12\nmfc2 %1, $13\nmfc2 %2, $14" : "=r"(a), "=r"(b), "=r"(c))
#define gte_ldv3(a, b, c)                                                                                              \
    __asm__ volatile("lwc2 $0, 0(%0)\nlwc2 $1, 4(%0)\nlwc2 $2, 0(%1)\nlwc2 $3, 4(%1)\nlwc2 $4, 0(%2)\nlwc2 $5, 4(%2)" \
                     :                                                                                                 \
                     : "r"(a), "r"(b), "r"(c))
#endif

/* The clip's work area in the scratchpad: the primitive code, the ordering table and the next primitive, the depth to
 * sort at, the near plane, the three corners' screen depths and the two new corners' colours. */
#define CLIP_CODE (*(unsigned int *)0x1F80000C)
#define CLIP_OT (*(unsigned int **)0x1F800040)
#define CLIP_PRIM (*(PolyG3W **)0x1F800044)
#define CLIP_OTZ (*(int *)0x1F80004C)
#define CLIP_NEAR (*(int *)0x1F800068)
#define CLIP_SZ ((int *)0x1F8001F8)
#define CLIP_RGB1 (*(unsigned int *)0x1F80020C)
#define CLIP_RGB2 (*(unsigned int *)0x1F800210)
#ifdef NATIVE
#define gte_ldrgb_8(v) gte_write(6, ((unsigned int *)(v))[2])
#else
/* RGB from a corner's colour word through $13, as the clip code loads it. */
#define gte_ldrgb_8(v) __asm__ volatile("lw $13, 8(%0)\nmtc2 $13, $6" : : "r"(v) : "$13")
#endif

/* The scratchpad as the model drawing code keeps its state there. */
#define CUT_CODE ((unsigned int *)0x1F80000C)  /* the primitives' code and flags */
#define CUT_OT ((unsigned int **)0x1F800040)   /* the ordering table */
#define CUT_PRIM ((PolyG3W **)0x1F800044)      /* the next primitive */
#define CUT_OTZ ((int *)0x1F80004C)            /* the entry this model goes in */
#define CUT_ZA ((int *)0x1F800068)             /* the clipped vertex's depth */
#define CUT_ZB ((int *)0x1F8001F8)             /* the other edge's far vertex's depth */
#define CUT_NEAR ((int *)0x1F8001FC)           /* the near plane */
#define CUT_NEAR2 ((int *)0x1F800200)          /* the near plane for the second edge */
#define CUT_T ((int *)0x1F800204)              /* where along the edge the plane cuts (4.12) */
#define CUT_RGB ((unsigned int *)0x1F800214)   /* the first cut point's shaded colour */

/* sub_800AC2FC's work area in the scratchpad. */
typedef struct FloorWork {
    short col0, col1, row0, row1; /* the cells in view */
    unsigned int line_rgbc;
    unsigned int tri_code;
    int flag;
    char x14[0xC];
    int sxy0, sxy1, sxy2;
    char x2C[0x14];
    unsigned int *ot;
    PolyG3W *tri;
    LineXY *line;
    int z;
    char x50[0x18];
    int half_height;
    int x6C;
    unsigned int shades[17];
    ShadeVert v[4];
    char xE4[0x90];
    unsigned short row_h[64];
} FloorWork;
#define FLOOR_WORK ((FloorWork *)0x1F800000)
extern unsigned char D_8011E96C[2][0xBE50], D_800C5F4C[2][0x18000];
extern unsigned int D_800AEAE0, D_800AEAE4[17];
extern int D_800AEB28, D_800AEB2C;
void sub_800ACA40(ShadeVert *a, ShadeVert *b, ShadeVert *c);
#ifdef NATIVE
#define gte_ldv3_m(a, b, c)                                                                                            \
    (gte_write(0, ((unsigned int *)(a))[0]), gte_write(1, ((unsigned int *)(a))[1]),                                   \
     gte_write(2, ((unsigned int *)(b))[0]), gte_write(3, ((unsigned int *)(b))[1]),                                   \
     gte_write(4, ((unsigned int *)(c))[0]), gte_write(5, ((unsigned int *)(c))[1]))
#else
#define gte_ldv3_m(a, b, c)                                                                                            \
    __asm__ volatile("lwc2 $0, %0\nlwc2 $1, %1\nlwc2 $2, %2\nlwc2 $3, %3\nlwc2 $4, %4\nlwc2 $5, %5"                    \
                     :                                                                                                 \
                     : "m"(((int *)(a))[0]), "m"(((int *)(a))[1]), "m"(((int *)(b))[0]), "m"(((int *)(b))[1]),        \
                       "m"(((int *)(c))[0]), "m"(((int *)(c))[1]))
#endif

/* The sea floor's lighting and fog: the camera, the light's direction, the fog colour, and where the fog starts and is
 * whole. */
typedef struct FloorEnv {
    SVECTOR cam;
    SVECTOR light;
    unsigned char r, g, b, pad;
    int far;
    int near;
} FloorEnv;
/* libgpu's POLY_F4 with its colour and points as words. */
typedef struct PolyF4W {
    unsigned int tag;
    unsigned int rgbc;
    int xy0, xy1, xy2, xy3;
} PolyF4W;
/* The work area the sea floor is drawn from, in the scratchpad. */
typedef struct FloorScratch {
    SVECTOR v[4]; /* the quad being drawn */
    char x20[0x20];
    int sxy[2];     /* a wall's last two screen points */
    int sxy_new[2]; /* and the next two */
    char x50[0x10];
    int sz[2]; /* the last two points' depths */
    char x68[0x18];
    int nclip;
    int x84;
    int otz;
    unsigned int code;                /* the quads' code, in the top byte */
    unsigned int normal_xy, normal_z; /* the face's normal, as V0 takes it */
    char x98[0x14];
    short wall_x, wall_z;
    FloorEnv env;
    unsigned short h, h_down, h_left, h_right, h_up, top; /* the cell's heights; top, a wall's */
    char xD8[4];
    unsigned int *ot;
    char xE0[4];
    short col_hi, col_lo, row_hi, row_lo; /* the cells in view */
    char xEC[8];
    int dist;
    int xF8;
    unsigned int side; /* the wall being drawn, 0-3 */
    int draw;
    unsigned int fog[17]; /* the colour for each 64 units of depth */
} FloorScratch;
extern FloorEnv D_800AD470;
extern short D_800AD468, D_800AD46A, D_800AD46C, D_800AD46E;
extern short D_800AE958, D_800AE95C;
extern unsigned int D_800AD890[17];
extern short D_800AD78C[0x81];
extern unsigned char D_8015730C[2][0x30000];
#ifdef NATIVE
#define gte_nop()
#define gte_ldv0_m(v) gte_ldv0(v)
#define gte_ldv1_m(v) gte_ldv1(v)
#define gte_ldv2_m(v) (gte_write(4, ((unsigned int *)(v))[0]), gte_write(5, ((unsigned int *)(v))[1]))
#define gte_stsxy2(p) (*(unsigned int *)(p) = gte_read(14))
#define gte_stotz(p) (*(unsigned int *)(p) = gte_read(7))
#define gte_stmac0(p) (*(unsigned int *)(p) = gte_read(24))
#define gte_stsz12(p) (((unsigned int *)(p))[0] = gte_read(17), ((unsigned int *)(p))[1] = gte_read(18))
#else
#define gte_nop() __asm__ volatile("nop")
#define gte_ldv0_m(v) __asm__ volatile("lwc2 $0, %0\nlwc2 $1, %1" : : "m"(((int *)(v))[0]), "m"(((int *)(v))[1]))
#define gte_ldv1_m(v) __asm__ volatile("lwc2 $2, %0\nlwc2 $3, %1" : : "m"(((int *)(v))[0]), "m"(((int *)(v))[1]))
#define gte_ldv2_m(v) __asm__ volatile("lwc2 $4, %0\nlwc2 $5, %1" : : "m"(((int *)(v))[0]), "m"(((int *)(v))[1]))
#define gte_stsxy2(p) __asm__ volatile("swc2 $14, %0" : "=m"(*(unsigned int *)(p)))
#define gte_stotz(p) __asm__ volatile("swc2 $7, %0" : "=m"(*(unsigned int *)(p)))
#define gte_stmac0(p) __asm__ volatile("swc2 $24, %0" : "=m"(*(unsigned int *)(p)))
#define gte_stsz12(p)                                                                                                  \
    __asm__ volatile("swc2 $17, %0\nswc2 $18, %1" : "=m"(((unsigned int *)(p))[0]), "=m"(((unsigned int *)(p))[1]))
#endif

extern int D_800AD8D4, D_800AD8D8, D_800AEA1C;
extern SVECTOR D_800AE8EC, D_800AE8FC;

void sub_800A9AD4(SVECTOR *angle, SVECTOR *axes);
#ifdef NATIVE
#define gte_ldL1(a, b) (gte_write_ctrl(8, (a)), gte_write_ctrl(9, (b)))
#define gte_stmac1(x) ((x) = gte_read(25))
#else
/* The light matrix's first row, its last element with L21 in the second word. */
#define gte_ldL1(a, b) __asm__ volatile("ctc2 %0, $8\nctc2 %1, $9" : : "r"(a), "r"(b))
/* MAC1, a slot after the command before it and one after the read. */
#define gte_stmac1(x) __asm__ volatile("nop\nmfc2 %0, $25\nnop" : "=r"(x))
#endif

void sub_800A9CB0(Torpedo *t);

/* The player's position and travel direction as the camera and the HUD's map read them. */
/* The GTE's error flag, and a zero the game tests instead of it. */
#ifdef NATIVE
#define gte_stflg(f, z) ((f) = gte_read_ctrl(31), (z) = 0)
#else
#define gte_stflg(f, z) __asm__ volatile("nop\ncfc2 %0, $31\nnop\nmove %1, $0" : "=r"(f), "=r"(z))
#endif

/* libgpu's POLY_G4 with its colours and points as words. */
typedef struct PolyG4W {
    unsigned int tag;
    unsigned int rgbc0;
    int xy0;
    unsigned int rgb1;
    int xy1;
    unsigned int rgb2;
    int xy2;
    unsigned int rgb3;
    int xy3;
} PolyG4W;
/* sub_800AA714's work area in the scratchpad: a torpedo's trail as a strip of five quads. */
typedef struct TrailWork {
    SVECTOR pts[12]; /* each of the six trail points either side of the width */
    int sxy[6];      /* two batches of three points on screen */
    unsigned int *ot;
    int otz[2];
    int flag[2];
    unsigned int code;
    int x90;
    PolyG4W *prim;
    unsigned int *colors; /* the trail's six colours, front to back */
    unsigned int pals[5][4];
    unsigned int trail_colors[6];
    SVECTOR width;
} TrailWork;
extern unsigned int D_800AE994[20], D_800AE9E4[6];
extern unsigned char D_801BFA18[2][0x1734];
#ifdef NATIVE
#define gte_stsxy3_m(a, b, c) ((a) = gte_read(12), (b) = gte_read(13), (c) = gte_read(14))
#define gte_stsz3(a, b, c) ((a) = gte_read(17), (b) = gte_read(18), (c) = gte_read(19))
#else
#define gte_stsxy3_m(a, b, c) __asm__ volatile("swc2 $12, %0\nswc2 $13, %1\nswc2 $14, %2" : "=m"(a), "=m"(b), "=m"(c))
#define gte_stsz3(a, b, c) __asm__ volatile("mfc2 %0, $17\nmfc2 %1, $18\nmfc2 %2, $19" : "=r"(a), "=r"(b), "=r"(c))
#endif
/* The farthest of the last three points' depths, a quarter of it as the OT entry. */
#define TRAIL_OTZ(n)                                                                                                   \
    {                                                                                                                  \
        gte_stsz3(z0, z1, z2);                                                                                         \
        if (z0 < z1) {                                                                                                 \
            z0 = z1;                                                                                                   \
        }                                                                                                              \
        if (z0 < z2) {                                                                                                 \
            z0 = z2;                                                                                                   \
        }                                                                                                              \
        w->otz[n] = z0 / 4;                                                                                            \
    }
/* A trail point either side of the width, as points n and n + 1. */
#define TRAIL_PAIR(n)                                                                                                  \
    {                                                                                                                  \
        w->pts[n].vx = x + wx;                                                                                         \
        w->pts[n].vy = y + wy;                                                                                         \
        w->pts[n].vz = z + wz;                                                                                         \
        w->pts[n + 1].vx = x - wx;                                                                                     \
        w->pts[n + 1].vy = y - wy;                                                                                     \
        w->pts[n + 1].vz = z - wz;                                                                                     \
    }
/* One quad of the strip: screen points a, b, c, d, shaded from colour k to k + 1. */
#define TRAIL_QUAD(z, a, b, c, d, k)                                                                                   \
    {                                                                                                                  \
        p = w->prim;                                                                                                   \
        xy0 = w->sxy[a];                                                                                               \
        cp = w->colors;                                                                                                \
        xy1 = w->sxy[b];                                                                                               \
        c0 = cp[k] | w->code;                                                                                          \
        p->xy0 = xy0;                                                                                                  \
        p->xy1 = xy1;                                                                                                  \
        p->rgbc0 = c0;                                                                                                 \
        p->rgb1 = c0;                                                                                                  \
        c1 = cp[k + 1];                                                                                                \
        xy0 = w->sxy[c];                                                                                               \
        xy1 = w->sxy[d];                                                                                               \
        p->rgb2 = c1;                                                                                                  \
        p->xy2 = xy0;                                                                                                  \
        p->rgb3 = c1;                                                                                                  \
        p->xy3 = xy1;                                                                                                  \
        ot = &w->ot[w->otz[z]];                                                                                        \
        p->tag = (*ot << 8 >> 8) | 0x08000000;                                                                         \
        *ot = (unsigned int)p << 8 >> 8;                                                                               \
        w->prim = p + 1;                                                                                               \
    }

/* sub_800AB518's work area in the scratchpad. */
typedef struct MarkWork {
    int x00[8];
    unsigned int *ot;
    SVECTOR *pt;         /* the cell being drawn: its centre, its number in pad */
    SVECTOR *verts;      /* the marker model's six vertices */
    unsigned int *faces; /* and its eight faces */
    int n;               /* cells left */
    unsigned int face;
    unsigned int code;
    int x3C;
    int px, py, pz; /* the player */
    int ring;       /* the sonar ping's radius */
    int ring_width;
    int dist;
    int range;
    int shade;
    SVECTOR vbuf[6];
    unsigned int fbuf[8];
} MarkWork;
/* libgpu's POLY_F3 with its colour and points as words. */
typedef struct PolyF3W {
    unsigned int tag;
    unsigned int rgbc;
    int xy0, xy1, xy2;
} PolyF3W;
extern int D_800AEA0C, D_800AEA1C, D_800B1E68;
extern unsigned int D_800AEA20[12], D_800AEA50[8], D_800AEA70[8];
extern unsigned char D_800F786C[2][0x13880];
#ifdef NATIVE
#define gte_stsxy3_f3(p)                                                                                              \
    (((int *)(p))[2] = gte_read(12), ((int *)(p))[3] = gte_read(13), ((int *)(p))[4] = gte_read(14))
#else
/* libgte's gte_stsxy3_f3: the three points into a POLY_F3. */
#define gte_stsxy3_f3(p) __asm__ volatile("swc2 $12, 8(%0)\nswc2 $13, 12(%0)\nswc2 $14, 16(%0)" : : "r"(p) : "memory")
#endif

/* A model: its vertices, its faces (three vertex numbers, one a byte) and each face's three colours. */
typedef struct Model {
    SVECTOR *verts;
    unsigned int *faces;
    unsigned int (*colors)[3];
    int n;
} Model;
/* sub_800ABD4C's work area in the scratchpad. */
typedef struct ModelWork {
    MATRIX m;
    unsigned int *ot;
    SVECTOR *verts;
    unsigned int *faces;
    unsigned int (*colors)[3];
    int n;
    unsigned int code;
} ModelWork;
#define MODEL_WORK ((ModelWork *)0x1F800000)
#ifdef NATIVE
#define gte_stsxy0_g3(p) ((p)->xy0 = gte_read(12))
#define gte_stsxy12_g3(p) ((p)->xy1 = gte_read(13), (p)->xy2 = gte_read(14))
#else
/* POLY_G3's points from SXY0-2, at their offsets from the primitive. */
#define gte_stsxy0_g3(p) __asm__ volatile("swc2 $12, 8(%0)" : : "r"(p) : "memory")
#define gte_stsxy12_g3(p) __asm__ volatile("swc2 $13, 16(%0)\nswc2 $14, 24(%0)" : : "r"(p) : "memory")
#endif

/* A point the player's sub is checked at, as an offset from it, and where it falls on the sea floor. */
typedef struct Probe {
    int vx, vy, vz;
    int x0C;
    int h; /* its depth */
    int cell;
    unsigned int alert; /* the alert it raises */
    int x1C;
} Probe;

/* The floor check's work area in the scratchpad. */
typedef struct ProbeWork {
    int x, y, z;
    int x0C;
    SVECTOR fwd, side, up;
    int gear;
    unsigned int alerts;
    Probe hull[12]; /* around the hull */
    Probe ahead[6]; /* along its heading: a length ahead, then this frame's move */
} ProbeWork;
#define PROBE_WORK ((ProbeWork *)0x1F800000)

void func_80044A68(void *p, int dfe, int dtd, int tpage, RECT *tw); /* SetDrawMode */
void func_80046A9C(void *p0, void *p1);                             /* MargePrim */
#define termPrim(p) setaddr(p, 0xFFFFFFFF)

/* The HUD's primitives. Icons are a draw mode merged with a sprite; text is a run of glyph sprites, linked one to the
 * next. */
typedef struct IconText {
    ModeSprite icon;
    DR_MODE mode;
    SPRT text[7];
} IconText;
extern IconText D_800AD9FC, D_800ADAB4;
extern SPRT D_800ADB6C[4], D_800ADBBC[5], D_800ADC20[5], D_800ADC84[3];
/* A bar shaded from one colour to another under its own draw mode, then its icon. */
typedef struct Gauge {
    DR_MODE mode;
    POLY_G4 bar;
    ModeSprite icon;
} Gauge;
extern Gauge D_800ADCC0;
/* A meter: two sprites, a textured quad that slides along it, and one on top. */
typedef struct Meter {
    SPRT face[2];
    POLY_GT4 slider;
    SPRT cap;
} Meter;
extern Meter D_800ADD1C, D_800ADD8C;
extern SPRT D_800ADDFC[2], D_800ADE24[2], D_800ADE4C[2], D_800ADE74[8];
extern SPRT D_800ADF00, D_800ADF14;
extern DR_MODE D_800ADF28;
extern ModeSprite D_800ADF34;
extern SPRT D_800ADF50[15], D_800AE07C[17], D_800AE1D0[9], D_800AE284[6], D_800AE2FC[5], D_800AE360[8];
extern SPRT D_800AE400[11], D_800AE4DC[10];

/* end of declarations */

#ifdef NON_MATCHING
/* Register choice throughout: the game's frame saves eight callee-saved registers and keeps 8 bytes on the stack where
 * GCC here uses a ninth register. */
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
    submar_init_hud();
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
    submar_probe_floor();
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
        func_8004433C(D_801BCF58);
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
            submar_probe_floor();
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
        func_8004418C(D_801BCF58, 8);
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
                D_800ADF34.sprt.r0 = 0;
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
            D_800ADF34.sprt.r0 = (1200 - D_800AD38C.countdown) / 15;
        } else {
            D_800ADF34.sprt.r0 = (900 - D_800AD38C.countdown) * 80 / 900;
        }
        D_800ADF34.sprt.x0 = -0xA0 - D_800AD3C8.x;
        D_800ADF34.sprt.y0 = -0x96 - D_800AD3C8.y;
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
    D_801BA30C.from.vx = D_800AE914;
    D_801BA30C.from.vy = D_800AE916;
    D_801BA30C.from.vz = D_800AE918;
    D_801BA30C.to.vx = -0x200;
    D_801BA30C.to.vy = D_800AE916;
    D_801BA30C.to.vz = D_800AE918;
    D_801BA30C.delta.vx = -0x200 - D_800AE914;
    D_801BA30C.delta.vy = 0x1000;
    D_801BA30C.delta.vz = 0;
    D_801BA30C.dist_from = D_800AE944;
    D_801BA30C.dist_to = 0x2800;
    D_801BA30C.dist_delta = 0x2800 - D_800AE944;
    D_801BA30C.height_from = D_800AD40C;
    D_801BA30C.height_to = 0x200;
    D_801BA30C.height_delta = 0x200 - D_800AD40C;
    D_801BA30C.t = 0;
    D_801BA30C.step = 0x10;
    D_801BA30C.active = 1;
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
    D_800B5F3C.vx = D_800AD420;
    D_800B5F3C.vy = D_800AD424;
    D_800B5F3C.vz = D_800AD428;
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

void sub_800A1DA0(SVECTOR *angle, MATRIX *m)
{
    int x, cx, sx, y, cy, sy, z, cz, sz;

    x = ((int *)D_8004BC98)[(angle->vx + 0x1000) & 0xFFF];
    cx = (unsigned int)x >> 16;
    sx = x & 0xFFFF;
    gte_ctc2(0, 0x1000);
    gte_ctc2(1, 0);
    x = -sx;
    gte_ctc2(2, x << 16 | cx);
    x = sx << 16;
    gte_ctc2(3, x);
    gte_ctc2(4, cx);
    y = ((int *)D_8004BC98)[(angle->vy + 0x1000) & 0xFFF];
    cy = (unsigned int)y >> 16;
    sy = y & 0xFFFF;
    y = -sy;
    gte_mtc2(0, cy);
    gte_mtc2(1, y);
    gte_op(0x4A486012);
    gte_mtc2(2, 0x10000000);
    gte_mtc2(3, 0);
    gte_mtc2(4, sy);
    gte_mtc2(5, cy);
    gte_rot_v12();
    z = ((int *)D_8004BC98)[(angle->vz + 0x1000) & 0xFFF];
    cz = (unsigned int)z >> 16;
    sz = z & 0xFFFF;
    z = -sz;
    gte_ctc2(0, z << 16 | cz);
    z = sz << 16;
    gte_ctc2(1, z);
    gte_ctc2(2, cz);
    gte_ctc2(3, 0);
    gte_ctc2(4, 0x1000);
    gte_op(0x4A486012);
    gte_store_cols(m);
}

/* Draws the sea floor in view: each raised cell's top, lit by the light and fogged by its distance, then the walls
 * down to its lower neighbours in steps of 64. Returns the next free primitive. */
#ifdef NON_MATCHING
/* Register choice grows the frame, and GCC places the column loops' tests at the other end. */
void *sub_800A1F9C(void)
{
    FloorScratch *sc;
    PolyF4W *p;
    Cell *cell;
    int i, j, k, row, col, x, y, z, a, b, c, sum, lz, sh, dist, flag, f, d, near;
    unsigned int step, ytop, side, top;
    unsigned char r, g, bl;
    short wx, wz;

    sc = (FloorScratch *)0x1F800000;
    sc->code = 0x2A000000;
    p = (PolyF4W *)D_8015730C[D_800AD3B8];
    sc->ot = D_801B730C[D_800AD3B8];
    /* Word by word, the camera's halves among them. */
    ((int *)&sc->env)[0] = ((int *)&D_800AD470)[0];
    ((int *)&sc->env)[1] = ((int *)&D_800AD470)[1];
    ((int *)&sc->env)[2] = ((int *)&D_800AD470)[2];
    ((int *)&sc->env)[3] = ((int *)&D_800AD470)[3];
    ((int *)&sc->env)[4] = ((int *)&D_800AD470)[4];
    ((int *)&sc->env)[5] = ((int *)&D_800AD470)[5];
    ((int *)&sc->env)[6] = ((int *)&D_800AD470)[6];
    for (i = 0; i < 17; i++) {
        sc->fog[i] = D_800AD890[i];
    }
    sc->normal_xy = 0x10000000;
    sc->normal_z = 0;
    row = (0x800 - D_800AE95C) / 64;
    col = (D_800AE958 + 0x800) / 64;
    sc->row_hi = row + D_800AD46E;
    if (sc->row_hi >= 64) {
        sc->row_hi = 63;
    }
    if (sc->row_hi < 0) {
        sc->row_hi = 0;
    }
    sc->row_lo = row + D_800AD46C;
    if (sc->row_lo >= 64) {
        sc->row_lo = 63;
    }
    if (sc->row_lo < 0) {
        sc->row_lo = 0;
    }
    sc->col_hi = col + D_800AD46A;
    if (sc->col_hi >= 64) {
        sc->col_hi = 63;
    }
    if (sc->col_hi < 0) {
        sc->col_hi = 0;
    }
    sc->col_lo = col + D_800AD468;
    if (sc->col_lo >= 64) {
        sc->col_lo = 63;
    }
    if (sc->col_lo < 0) {
        sc->col_lo = 0;
    }
    gte_SetBackColor(0, 0, 0);

    /* The tops. */
    for (j = sc->row_hi; j >= sc->row_lo; j--) {
        z = 0x800 - j * 64;
        sc->v[0].vz = sc->v[1].vz = z;
        sc->v[2].vz = sc->v[3].vz = z - 0x40;
        for (k = sc->col_hi; k >= sc->col_lo; k--) {
            cell = &D_800B5F4C[j * 64 + k];
            if ((sc->h = cell->h) == cell->base) {
                continue;
            }
            gte_mtc2(6, sc->code | sc->fog[cell->target / 64]);
            sc->v[0].vx = sc->v[2].vx = k * 64 - 0x800;
            sc->v[1].vx = sc->v[3].vx = k * 64 - 0x7C0;
            sc->v[0].vy = sc->v[1].vy = sc->v[2].vy = sc->v[3].vy = sc->h;
            x = sc->v[0].vx - sc->env.cam.vx + 0x20;
            y = sc->v[0].vy - sc->env.cam.vy;
            z = sc->v[0].vz - sc->env.cam.vz - 0x20;
            gte_mtc2(9, x);
            gte_mtc2(10, y);
            gte_mtc2(11, z);
            gte_op(0x4AA00428);
            gte_nop();
            gte_stmac(a, b, c);
            sum = a + b + c;
            gte_mtc2(30, sum);
            gte_mtc2(9, x);
            gte_mtc2(10, y);
            gte_mtc2(11, z);
            gte_stlzc(lz);
            lz = lz >> 1 << 1;
            sh = (31 - lz) / 2;
            if (lz == 32) {
                sum = 0;
            }
            if (lz < 24) {
                sum >>= 24 - lz;
            } else {
                sum <<= lz - 24;
            }
            gte_mtc2(8, D_800AD60C[sum - 0x40]);
            dist = D_800AD48C[sum - 0x40] << sh >> 12;
            sc->dist = dist;
            if (sc->env.far < dist) {
                continue;
            }
            gte_op_now(0x4B90003D);
            gte_ldv3c(sc->v);
            gte_stmac(x, y, z);
            gte_op_now(0x4A280030);
            gte_ctc2(8, (x >> sh) & 0xFFFF | (y >> sh) << 16);
            gte_ctc2(9, (z >> sh) & 0xFFFF);
            flag = gte_cfc2(31);
            gte_stsxy0(&p->xy0);
            gte_stsxy1(&p->xy1);
            gte_stsxy2(&p->xy2);
            gte_op_now(0x4B400006);
            if (flag < 0) {
                continue;
            }
            gte_ldv0_m(&sc->v[3]);
            gte_ldv1(&sc->env.light);
            d = gte_mfc2(24);
            gte_nop();
            gte_op_now(0x4A180001);
            if (d <= 0) {
                continue;
            }
            gte_ctc2(16, 0);
            gte_ctc2(17, 0);
            gte_ctc2(19, 0);
            flag = gte_cfc2(31);
            gte_stsxy2(&p->xy3);
            if (flag < 0) {
                continue;
            }
            gte_op_now(0x4B68002E);
            gte_ldv0_m(&sc->normal_xy);
            d = gte_mfc2(7);
            gte_stotz(&sc->otz);
            if (d >= 0x600) {
                continue;
            }
            gte_op_now(0x4A4AE012);
            f = 0;
            r = sc->env.r;
            bl = sc->env.b;
            g = sc->env.g;
            d = gte_mfc2(25);
            near = sc->env.near;
            if (d >= near) {
                f = ((d - near) << 7) / (0x1000 - near);
                if (f > 0x80) {
                    f = 0x80;
                }
                f = D_800AD78C[f] * (sc->env.far - sc->dist) / sc->env.far;
            }
            if (f > 0) {
                gte_mtc2(9, r);
                gte_mtc2(10, g);
                gte_mtc2(11, bl);
                gte_mtc2(8, f);
                gte_op(0x4B90003D);
                gte_nop();
                gte_stmac(a, b, c);
                gte_ctc2(16, a >> 8);
                gte_ctc2(17, b >> 8 << 16);
                gte_ctc2(19, c >> 8);
            }
            gte_op(0x4B08041B);
            p->tag = (sc->ot[sc->otz] << 8 >> 8) | 0x05000000;
            sc->ot[sc->otz] = (unsigned int)p << 8 >> 8;
            p++;
            gte_strgb2(&p[-1].rgbc);
        }
    }

    /* The walls, where a neighbour is lower. */
    for (j = sc->row_hi; j >= sc->row_lo; j--) {
        sc->wall_z = 0x800 - j * 64;
        for (k = sc->col_hi; k >= sc->col_lo; k--) {
            cell = &D_800B5F4C[j * 64 + k];
            /* The six heights and flags as three words (Cell is only short-aligned). */
            ((unsigned int *)&sc->h)[0] = ((unsigned int *)cell)[0];
            ((unsigned int *)&sc->h)[1] = ((unsigned int *)cell)[1];
            ((unsigned int *)&sc->h)[2] = ((unsigned int *)cell)[2];
            if ((sc->h = cell->h) == cell->base) {
                continue;
            }
            gte_mtc2(6, sc->code | sc->fog[cell->target >> 6]);
            gte_SetBackColor(0, 0, 0);
            sc->wall_x = k * 64 - 0x800;
            sc->side = 0;
            do {
                side = sc->side;
                sc->draw = 0;
                sc->side = side + 1;
                switch (side) {
                case 0:
                    top = sc->h_down;
                    if (sc->h < top) {
                        wx = sc->wall_x;
                        sc->top = top;
                        wz = sc->wall_z;
                        sc->normal_xy = 0;
                        sc->normal_z = 0x1000;
                        sc->draw = 1;
                        sc->v[0].vx = wx;
                        sc->v[2].vx = wx;
                        sc->v[1].vx = wx + 0x40;
                        sc->v[3].vx = wx + 0x40;
                        sc->v[0].vz = wz - 0x40;
                        sc->v[1].vz = wz - 0x40;
                        sc->v[2].vz = wz - 0x40;
                        sc->v[3].vz = wz - 0x40;
                    }
                    break;
                case 1:
                    top = sc->h_up;
                    if (sc->h < top) {
                        sc->normal_z = 0xF000;
                        wx = sc->wall_x;
                        wz = sc->wall_z;
                        sc->top = top;
                        sc->normal_xy = 0;
                        sc->draw = 1;
                        sc->v[1].vx = wx;
                        sc->v[3].vx = wx;
                        sc->v[0].vx = wx + 0x40;
                        sc->v[2].vx = wx + 0x40;
                        sc->v[0].vz = wz;
                        sc->v[1].vz = wz;
                        sc->v[2].vz = wz;
                        sc->v[3].vz = wz;
                    }
                    break;
                case 2:
                    top = sc->h_left;
                    if (sc->h < top) {
                        wz = sc->wall_z;
                        sc->top = top;
                        wx = sc->wall_x;
                        sc->normal_xy = 0x1000;
                        sc->normal_z = 0;
                        sc->draw = 1;
                        sc->v[0].vx = wx;
                        sc->v[1].vx = wx;
                        sc->v[2].vx = wx;
                        sc->v[3].vx = wx;
                        sc->v[0].vz = wz;
                        sc->v[2].vz = wz;
                        sc->v[1].vz = wz - 0x40;
                        sc->v[3].vz = wz - 0x40;
                    }
                    break;
                case 3:
                    top = sc->h_right;
                    if (sc->h < top) {
                        sc->normal_xy = 0xF000;
                        wx = sc->wall_x;
                        sc->top = top;
                        wz = sc->wall_z;
                        sc->normal_z = 0;
                        sc->draw = 1;
                        sc->v[0].vx = wx + 0x40;
                        sc->v[1].vx = wx + 0x40;
                        sc->v[2].vx = wx + 0x40;
                        sc->v[3].vx = wx + 0x40;
                        sc->v[1].vz = wz;
                        sc->v[3].vz = wz;
                        sc->v[0].vz = wz - 0x40;
                        sc->v[2].vz = wz - 0x40;
                    }
                    break;
                }
                if (!sc->draw) {
                    continue;
                }
                ytop = (unsigned short)sc->h;
                sc->v[0].vy = sc->v[1].vy = ytop;
                gte_ldv0_m(&sc->v[0]);
                gte_ldv1_m(&sc->v[1]);
                gte_op(0x4A280030);
                step = 0x40 - (ytop & 0x3F);
                if ((unsigned short)sc->top < ytop + step) {
                    step = (unsigned short)sc->top - ytop;
                }
                sc->v[2].vy = ytop;
                sc->v[0].vy = sc->v[1].vy = ytop + step;
                gte_stsxy0(&sc->sxy[0]);
                gte_stsxy1(&sc->sxy[1]);
                gte_stsz12(sc->sz);
                do {
                    x = (sc->v[0].vx + sc->v[1].vx) / 2;
                    y = (sc->v[0].vy + sc->v[2].vy) / 2;
                    z = (sc->v[0].vz + sc->v[1].vz) / 2;
                    x -= sc->env.cam.vx;
                    y -= sc->env.cam.vy;
                    z -= sc->env.cam.vz;
                    gte_mtc2(9, x);
                    gte_mtc2(10, y);
                    gte_mtc2(11, z);
                    gte_ldv0_m(&sc->v[0]);
                    gte_op_now(0x4AA00428);
                    gte_ldv1_m(&sc->v[1]);
                    gte_ldv2_m(&sc->v[1]);
                    gte_nop();
                    gte_stmac(a, b, c);
                    sum = a + b + c;
                    gte_mtc2(30, sum);
                    gte_mtc2(9, x);
                    gte_mtc2(10, y);
                    gte_mtc2(11, z);
                    gte_stlzc(lz);
                    lz = lz >> 1 << 1;
                    sh = (31 - lz) / 2;
                    if (lz == 32) {
                        sum = 0;
                    }
                    if (lz < 24) {
                        sum >>= 24 - lz;
                    } else {
                        sum <<= lz - 24;
                    }
                    gte_mtc2(8, D_800AD60C[sum - 0x40]);
                    gte_op(0x4B90003D);
                    dist = D_800AD48C[sum - 0x40] << sh >> 12;
                    sc->dist = dist;
                    if (sc->env.far < dist) {
                        ytop += step;
                        step = sc->top - ytop;
                        if (step > 0x40) {
                            step = 0x40;
                        }
                        continue;
                    }
                    gte_stmac(x, y, z);
                    gte_op_now(0x4A280030);
                    gte_ctc2(8, (x >> sh) & 0xFFFF | (y >> sh) << 16);
                    gte_ctc2(9, (z >> sh) & 0xFFFF);
                    ytop += step;
                    a = sc->sz[0];
                    b = sc->sz[1];
                    step = sc->top - ytop;
                    if (step > 0x40) {
                        step = 0x40;
                    }
                    sc->v[2].vy = ytop;
                    sc->v[0].vy = sc->v[1].vy = ytop + step;
                    flag = gte_cfc2(31);
                    gte_stsxy0(&sc->sxy_new[0]);
                    gte_stsxy1(&sc->sxy_new[1]);
                    gte_mtc2(16, a);
                    gte_mtc2(19, b);
                    gte_stsz12(sc->sz);
                    gte_op_now(0x4B68002E);
                    x = sc->sxy_new[0];
                    a = sc->sxy[0];
                    b = sc->sxy[1];
                    y = sc->sxy_new[1];
                    sc->sxy[0] = x;
                    sc->sxy[1] = y;
                    if (flag < 0) {
                        continue;
                    }
                    gte_nop();
                    gte_mtc2(14, a);
                    d = gte_mfc2(7);
                    if (d >= 0x600) {
                        continue;
                    }
                    sc->otz = d;
                    gte_op_now(0x4B400006);
                    p->xy0 = a;
                    p->xy1 = b;
                    p->xy2 = sc->sxy_new[0];
                    p->xy3 = sc->sxy_new[1];
                    gte_stmac0(&sc->nclip);
                    gte_mtc2(15, b);
                    gte_op(0x4B400006);
                    gte_nop();
                    gte_ldv1(&sc->env.light);
                    d = gte_mfc2(24);
                    if (sc->nclip >= 0 && d <= 0) {
                        continue;
                    }
                    f = 0;
                    gte_op(0x4A4AE012);
                    r = sc->env.r;
                    bl = sc->env.b;
                    g = sc->env.g;
                    gte_ldv0_m(&sc->normal_xy);
                    gte_ctc2(16, 0);
                    gte_ctc2(17, 0);
                    gte_ctc2(19, 0);
                    d = gte_mfc2(25);
                    near = sc->env.near;
                    if (d >= near) {
                        f = ((d - near) << 7) / (0x1000 - near);
                        if (f > 0x80) {
                            f = 0x80;
                        }
                        f = D_800AD78C[f] * (D_800AD470.far - sc->dist) / sc->env.far;
                    }
                    if (f > 0) {
                        gte_mtc2(9, r);
                        gte_mtc2(10, g);
                        gte_mtc2(11, bl);
                        gte_mtc2(8, f);
                        gte_op(0x4B90003D);
                        gte_stmac(a, b, c);
                        gte_ctc2(16, a >> 8);
                        gte_ctc2(17, b >> 8 << 16);
                        gte_ctc2(19, c >> 8);
                    }
                    gte_op(0x4B08041B);
                    p->tag = (sc->ot[sc->otz] << 8 >> 8) | 0x05000000;
                    sc->ot[sc->otz] = (unsigned int)p << 8 >> 8;
                    p++;
                    gte_strgb2(&p[-1].rgbc);
                } while (ytop < (unsigned short)sc->top);
            } while (sc->side < 4);
        }
    }
    return p;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A1F9C);
#endif

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


#ifdef NON_MATCHING
/* Register choice for the GTE operands. */
void sub_800A32C8(void)
{
    register int t4 FIXED_REG("$12");
    register int t5 FIXED_REG("$13");
    register int t6 FIXED_REG("$14");
    Sub *s;
    VECTOR *v;
    int len;

    v = (VECTOR *)0x1F800000;
    s = D_800B08EC;
    do {
        if (!(s->flags & 3)) {
            continue;
        }
        v[0].vx = (s->node->x - s->x) >> 12;
        v[0].vy = (s->node->y - s->y) >> 12;
        v[0].vz = (s->node->z - s->z) >> 12;
        submar_normalize(&v[0]);
        gte_ctc2(0, v[0].vx);
        gte_ctc2(2, v[0].vy);
        gte_ctc2(4, v[0].vz);
        gte_ldlvl(&D_800B5F3C);
        gte_op(0x4B78000C);
        gte_stlvnl_m(&v[1]);
        if (!submar_normalize(&v[1])) {
            gte_ldlvl(&D_800AD430);
            gte_op(0x4B78000C);
            gte_stlvnl_m(&v[1]);
            if (!submar_normalize(&v[1])) {
                gte_ldlvl(&D_800AD410);
                gte_op(0x4B78000C);
                gte_stlvnl_m(&v[1]);
                submar_normalize(&v[1]);
            }
        }
        gte_lwc2_m(9, v[0].vx);
        gte_mtc2(10, 0);
        gte_lwc2_m(11, v[0].vz);
        gte_op(0x4AA00428);
        t4 = gte_mfc2(25);
        t5 = gte_mfc2(27);
        t4 += t5;
        gte_mtc2(30, t4);
        t6 = gte_mfc2_late(31);
        t6 = t6 >> 1 << 1;
        t5 = (31 - t6) / 2;
        if (t6 == 32) {
            t4 = 0;
        }
        if (t6 < 24) {
            t4 >>= 24 - t6;
        } else {
            t4 <<= t6 - 24;
        }
        len = D_800AD48C[t4 - 0x40] << t5 >> 12;
        s->ax = -submar_ratan2(v[0].vy, len);
        s->ay = -submar_ratan2(v[1].vz, v[1].vx);
        s->az = 0;
    } while (++s < &D_800B08EC[12]);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A32C8);
#endif

/* Copies of libgte's ratan2 and of a vector normalise built from GTE register moves; a second pair sits at
 * 0x800A9914, so the module seems to be two source files that each have them. ratan2 divides without the checks
 * the rest of the module has. Kept as asm. */
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_ratan2);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_normalize);
/* Moves the subs: each turns toward its wanted pitch and heading and goes along them at a speed easing to its
 * node's; a sunk one sinks to the floor. Then the distance to its node, its line to the player (how far, ahead,
 * aside, above), the next node, torpedoes at the player, and its alert state and tubes. */
#ifdef NON_MATCHING
/* Hoisted loop constants in other registers; the trail copies come out as lwl/lwr, and the second state switch loses
 * its empty cases. */
void sub_800A36F4(void)
{
    Sub *s;
    int cur, d, spd, x, y, z, i;
    short *a, *b, *tbl;
    int sb, cb, sum, lz, sh;
    unsigned int v;
    int k, j;

    tbl = D_8004BC98;
    s = D_800B08EC;
    do {
        if (s->flags & 3) {
            d = (s->ax + 0x1000) & 0xFFF;
            cur = (s->rx + 0x1000) & 0xFFF;
            d -= cur;
            if (d > 0x800) {
                d -= 0x1000;
            }
            if (d < -0x800) {
                d += 0x1000;
            }
            if (d > 0) {
                d = 4;
            } else if (d < 0) {
                d = -4;
            }
            cur += d;
            if (cur > 0x800) {
                cur -= 0x1000;
            }
            if (cur < -0x800) {
                cur += 0x1000;
            }
            s->rx = cur;
            d = (s->ay + 0x1000) & 0xFFF;
            cur = (s->ry + 0x1000) & 0xFFF;
            d -= cur;
            if (d > 0x800) {
                d -= 0x1000;
            }
            if (d < -0x800) {
                d += 0x1000;
            }
            if (d > 0) {
                d = 8;
            } else if (d < 0) {
                d = -8;
            }
            cur += d;
            if (cur > 0x800) {
                cur -= 0x1000;
            }
            if (cur < -0x800) {
                cur += 0x1000;
            }
            s->ry = cur;
            a = &tbl[((s->rx + 0x1000) & 0xFFF) * 2];
            b = &tbl[((s->ry + 0x1000) & 0xFFF) * 2];
            sb = a[1] * b[0];
            cb = a[1] * b[1];
            s->dir.vy = -a[0];
            s->dir.vx = sb >> 12;
            s->dir.vz = cb >> 12;
        }
        if (s->flags & 1) {
            spd = s->x2C;
            x = s->x + (s->dir.vx * spd >> 12);
            z = s->z + (s->dir.vz * spd >> 12);
            y = s->y + (s->dir.vy * spd >> 12);
            if ((unsigned int)(x + 0x800000) > 0x1000000 || (unsigned int)y > 0x400000 ||
                (unsigned int)(z + 0x800000) > 0x1000000) {
                s->flags = (s->flags & ~1) | 2;
                continue;
            }
            s->x = x;
            x >>= 12;
            s->z = z;
            z >>= 12;
            s->y = y;
            x = (x + 0x800) / 64;
            if (x < 0) {
                x = 0;
            }
            if (x >= 64) {
                x = 63;
            }
            z = (0x800 - z) / 64;
            if (z < 0) {
                z = 0;
            }
            if (z >= 64) {
                z = 63;
            }
            s->cell = z * 64 + x;
            d = s->x30 - spd;
            if (d < -8 || d > 8) {
                d /= 8;
            } else if (d > 0) {
                d = 1;
            } else if (d < 0) {
                d = -1;
            }
            s->x2C = spd + d;
        } else if (s->flags & 0x20) {
            if (s->y < D_800B5F4C[(0x800 - (s->z >> 12)) / 64 * 64 + ((s->x >> 12) + 0x800) / 64].base << 12) {
                s->y += 0x800;
                if (s->rx >= -0x2FF) {
                    s->rx -= 4;
                }
                if (s->rz >= -0x2FF) {
                    s->rz -= 4;
                    s->x += s->dir.vx / 4;
                    s->z += s->dir.vz / 4;
                }
            } else {
                s->flags = 0;
                D_800AD8E0++;
                D_800AD8E4--;
            }
        }
        if (s->flags & 1) {
            x = (s->node->x - s->x) >> 12;
            y = (s->node->y - s->y) >> 12;
            z = (s->node->z - s->z) >> 12;
            gte_mtc2(9, x);
            gte_mtc2(10, y);
            gte_mtc2(11, z);
            gte_op(0x4AA00428);
            gte_stmac(sum, lz, sh);
            sum += lz;
            sum += sh;
            gte_mtc2(30, sum);
            lz = gte_mfc2_late(31);
            lz = lz >> 1 << 1;
            sh = (31 - lz) / 2;
            if (lz == 32) {
                sum = 0;
            }
            if (lz < 24) {
                sum >>= 24 - lz;
            } else {
                sum <<= lz - 24;
            }
            s->node_dist = D_800AD48C[sum - 0x40] << sh >> 12;
        }
        if (s->flags & 3) {
            x = (s->x - D_800AE8D0.x) >> 12;
            y = (s->y - D_800AE8D0.y) >> 12;
            z = (s->z - D_800AE8D0.z) >> 12;
            gte_mtc2(9, x);
            gte_mtc2(10, y);
            gte_mtc2(11, z);
            gte_op(0x4AA00428);
            s->to_player.vx = x;
            s->to_player.vy = y;
            s->to_player.vz = z;
            gte_stmac(sum, lz, sh);
            sum += lz;
            sum += sh;
            gte_mtc2(30, sum);
            gte_mtc2(9, x);
            gte_mtc2(10, y);
            gte_mtc2(11, z);
            gte_stlzc(lz);
            lz = lz >> 1 << 1;
            sh = (31 - lz) / 2;
            if (lz == 32) {
                sum = 0;
            }
            if (lz < 24) {
                sum >>= 24 - lz;
            } else {
                sum <<= lz - 24;
            }
            gte_mtc2(8, D_800AD60C[sum - 0x40]);
            sum = D_800AD48C[sum - 0x40];
            gte_op_now(0x4B90003D);
            s->dist = sum << sh >> 12;
            gte_stmac(x, y, z);
            x >>= sh;
            y >>= sh;
            z >>= sh;
            gte_ctc2(8, (x & 0xFFFF) | y << 16);
            gte_ctc2(9, (z & 0xFFFF) | -x << 16);
            gte_ctc2(10, (-y & 0xFFFF) | -z << 16);
            gte_ldv0(&D_800AE8FC);
            gte_op(0x4A4A6012);
            gte_ldv1(&D_800AE8EC);
            gte_ldv2(&s->dir);
            x = gte_mfc2(25);
            gte_op_now(0x4A4AE012);
            if (x < D_800AD8D4) {
                s->flags &= ~4;
            } else {
                s->flags |= 4;
            }
            s->angle = x;
            x = gte_mfc2(25);
            gte_op_now(0x4A4B6012);
            s->side = x;
            x = gte_mfc2(26);
            if (x < D_800AD8D8) {
                s->flags &= ~8;
            } else {
                s->flags |= 8;
            }
        }
        if (!(s->flags & 3)) {
            continue;
        }
        if (s->value <= 0) {
            s->flags = 0x20;
            s->state = 0;
            s->x3C = 0;
            s->value = 0;
            D_800AD8E4++;
            continue;
        }
        if (s->node_dist < s->node->radius) {
            s->node = (PathNode *)s->node->next;
            s->x30 = s->node->speed;
        }
        if ((s->flags & 8) && D_800AD8F0 && s->state) {
            k = 0;
            do {
                if (s->tubes[k] == 0x20000) {
                    for (i = 16; i < 32; i++) {
                        if (D_800F5F6C[i].flags != 1) {
                            continue;
                        }
                        if (s->dist >= 0x200) {
                            break;
                        }
                        D_800F5F6C[i].pos.vx = s->x;
                        D_800F5F6C[i].pos.vy = s->y;
                        D_800F5F6C[i].pos.vz = s->z;
                        D_800F5F6C[i].trail_buf[0].vx = s->x >> 12;
                        D_800F5F6C[i].trail_buf[0].vy = s->y >> 12;
                        D_800F5F6C[i].trail_buf[0].vz = s->z >> 12;
                        D_800F5F6C[i].vel.vx = D_800AE8D0.x;
                        D_800F5F6C[i].vel.vy = D_800AE8D0.y;
                        D_800F5F6C[i].vel.vz = D_800AE8D0.z;
                        D_800F5F6C[i].rot.vx = s->rx;
                        D_800F5F6C[i].rot.vy = s->ry;
                        D_800F5F6C[i].rot.vz = s->rz;
                        D_800F5F6C[i].x70 = D_800B0E5C.x1024 / 2;
                        D_800F5F6C[i].x74 = D_800B0E5C.x1028;
                        D_800F5F6C[i].flags = 2;
                        D_800F5F6C[i].target = (Sub *)&D_800AE8D0;
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
                        s->tubes[k] = 0;
                        break;
                    }
                    break;
                }
            } while (++k < 4);
        }
        if (s->dist <= D_800AEA10) {
            s->flags |= 0x10;
            if (D_800AEA0C) {
                d = D_800AEA1C - s->dist;
                if (d >= 0 && d <= D_800AEA14) {
                    switch (s->state) {
                    case 0:
                        s->state = 0x100;
                        s->x3C = 0x3C;
                        break;
                    case 0x200:
                        s->state = 0x400;
                        s->x3C = 0x2D;
                        break;
                    case 0x400:
                        s->state = 0x400;
                        s->x3C = 0x2D;
                        break;
                    case 0x800:
                        s->x3C = 0x2D;
                        break;
                    }
                } else {
                    switch (s->state) {
                    case 0x200:
                        break;
                    case 0:
                        break;
                    case 0x100:
                        s->state = 0x200;
                        s->x3C = 0x2D;
                        break;
                    }
                }
            }
        } else {
            s->state = 0;
            s->x3C = 0;
            s->flags &= ~0x10;
        }
        if (--s->x3C < 0) {
            switch (s->state) {
            case 0x800:
                s->state = 0x400;
                s->x3C = 0x2D;
                break;
            case 0x400:
                s->state = 0x200;
                s->x3C = 0x2D;
                break;
            case 0x200:
            case 0x100:
                s->state = 0;
            case 0:
            default:
                s->x3C = 0;
            }
        }
        j = 0;
        do {
            v = s->tubes[j];
            if (v & 0x10000) {
                v = (v & 0xFFFF) - 1;
                if (!v) {
                    v = 0x20000;
                } else {
                    v = (v & 0xFFFF) | 0x10000;
                }
            } else if (!(v & 0x20000)) {
                v = 0x10060;
            }
            s->tubes[j] = v;
        } while (++j < 4);
    } while (++s < &D_800B08EC[12]);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A36F4);
#endif

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


#ifdef NON_MATCHING
/* Same code, but GCC here bases the primitive's induction register on the second point rather than the third, and
 * register choice. */
void sub_800A4624(void)
{
    Sub *s;
    unsigned int *ot;
    LineXY *p;
    unsigned int y, top, c, shade;
    int x, z, z1, z2;

    ot = D_801B730C[D_800AD3B8];
    p = (LineXY *)D_801C2880[D_800AD3B8];
    s = D_800B08EC;
    do {
        if (!(s->flags & 3) || !(s->flags & 0x10) || !(s->state & 0xF00)) {
            continue;
        }
        z = (unsigned int)s->z >> 12;
        top = D_800B5F4C[s->cell].base;
        y = s->y >> 12;
        gte_mtc2(1, z);
        gte_mtc2(3, z);
        gte_mtc2(5, z);
        x = s->x >> 12;
        if (y >= top) {
            continue;
        }
        x &= 0xFFFF;
        do {
            gte_mtc2(0, top << 16 | x);
            top -= 0x40;
            if (y >= top) {
                top = y;
            }
            gte_mtc2(2, top << 16 | x);
            gte_mtc2(4, top << 16 | x);
            gte_rtpt();
            shade = (top >> 2) & 0xFF;
            if (s->angle <= 0) {
                c = ((shade >> 1) + (shade >> 2)) << 16;
            } else {
                switch (s->state) {
                case 0x800:
                    c = (shade >> 1) + (shade >> 2);
                    break;
                case 0x400:
                    c = shade >> 1;
                    c |= c << 8;
                    break;
                case 0x200:
                    c = shade >> 2;
                    c |= c << 8;
                    break;
                case 0x100:
                    c = (shade >> 1) << 8;
                    break;
                default:
                    c = shade;
                }
            }
            if (top != y) {
                p->rgbc = c | 0x4A000000;
                p->end = 0x55555555;
            } else {
                p->rgbc = c | 0x42000000;
            }
            if (gte_cfc2(31) < 0) {
                continue;
            }
            if (top != y) {
                gte_stsxy0(&p->xy0);
                c = gte_mfc2(13);
                p->xy1 = c;
                p->xy2 = (c >> 16 << 16) | (((short)c + 2) & 0xFFFF);
                z1 = gte_mfc2(17);
                z2 = gte_mfc2(18);
                z = z2;
                if (z1 < z2) {
                    z = z1;
                }
                z >>= 2;
                if (z >= 0x600) {
                    break;
                }
                p->tag = (ot[z] << 8 >> 8) | 0x05000000;
                ot[z] = (unsigned int)p << 8 >> 8;
                p++;
            } else {
                gte_stsxy0(&p->xy0);
                gte_stsxy1(&p->xy1);
                z1 = gte_mfc2(17);
                z2 = gte_mfc2(18);
                z = z2;
                if (z1 < z2) {
                    z = z1;
                }
                z >>= 2;
                if (z >= 0x600) {
                    break;
                }
                p->tag = (ot[z] << 8 >> 8) | 0x03000000;
                ot[z] = (unsigned int)p << 8 >> 8;
                p = (LineXY *)&p->xy2;
            }
        } while (y < top);
    } while (++s < &D_800B08EC[12]);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A4624);
#endif

#ifdef NON_MATCHING
/* Register choice: GCC here keeps the next primitive's address in a register of its own. */
void sub_800A4914(void)
{
    Sub *s;
    LineF3 *p, *prev;
    unsigned int c;
    int sxy, x, y, h;
    short i;
    unsigned int end, tag;

    prev = p = (LineF3 *)D_801C5700[D_800AD3B8];
    s = D_800B08EC;
    end = 0x55555555;
    tag = 0x05000000;
    do {
        if (!s->state) {
            continue;
        }
        c = (s->x >> 12) & 0xFFFF | (s->y >> 12) << 16;
        gte_mtc2(0, c);
        c = (s->z >> 12) & 0xFFFF;
        gte_mtc2(1, c);
        gte_rtps();
        switch (s->state) {
        case 0x800:
            c = 0xC0;
            break;
        case 0x400:
            c = 0x8080;
            break;
        case 0x200:
            c = 0x4040;
            break;
        case 0x100:
            c = 0x8000;
            break;
        }
        if (s->angle <= 0) {
            c = 0xC00000;
        }
        c |= 0x4A000000;
        p[0].rgbc = c;
        p[1].rgbc = c;
        p[0].end = end;
        p[1].end = end;
        if (gte_cfc2(31) < 0) {
            continue;
        }
        sxy = gte_mfc2(14);
        x = (short)sxy;
        y = sxy >> 16;
        i = s->dist / 64;
        if (i >= 8) {
            i = 7;
        }
        if (i < 0) {
            i = 0;
        }
        h = D_800AD8F4[i];
        p->tag = tag;
        p->x0 = x - h;
        p->y0 = y - h;
        p->y1 = y - h;
        p->x1 = x + h;
        p->x2 = x + h;
        p->y2 = y + h;
        setaddr_sh(prev, p);
        prev = p;
        p = prev + 1;
        p->x0 = x - h;
        p->y0 = y - h;
        p->x1 = x - h;
        p->y1 = y + h;
        p->x2 = x + h;
        p->y2 = y + h;
        p->tag = tag;
        setaddr_sh(prev, p);
        prev = p;
        p = prev + 1;
    } while (++s < &D_800B08EC[12]);
    setaddr(prev, 0xFFFFFFFF);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A4914);
#endif

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


#ifdef NON_MATCHING
/* Register choice and order in the readout's links: the game loads every tag before it stores the
 * palettes and keeps the loop's masks in s0 and s1. Everything else matches. */
void submar_init_hud(void)
{
    unsigned short clut = D_800AD904.clut;
    SPRT *p;
    int tpage;
    int i;

    func_8004418C(D_801BCF08, 20);
    func_8004418C(D_801BCF58, 8);
    for (i = 0; i < 4; i++) {
        func_80044A68(&D_800AD97C[i].mode, 0, 0,
                      getTPage(D_800AD904.mode, 1, D_800AD904.image.x, D_800AD904.image.y), 0);
        D_800AD97C[i].sprt.clut = clut + 3;
        func_80046A9C(&D_800AD97C[i].mode, &D_800AD97C[i].sprt);
    }
    D_800AD97C[1].sprt.clut = D_800AD97C[0].sprt.clut = clut + 1;

    func_80044A68(&D_800AD9FC.icon.mode, 0, 0, getTPage(D_800AD904.mode, 1, D_800AD904.image.x, D_800AD904.image.y),
                  0);
    D_800AD9FC.icon.sprt.clut = clut;
    func_80046A9C(&D_800AD9FC.icon.mode, &D_800AD9FC.icon.sprt);
    func_80044A68(&D_800AD9FC.mode, 0, 0, getTPage(D_800AD904.mode, 0, D_800AD904.image.x, D_800AD904.image.y), 0);
    setaddr(&D_800AD9FC.mode, &D_800AD9FC.text[0]);
    for (i = 0; i < 6; i++) {
        D_800AD9FC.text[i].clut = clut;
        D_800AD9FC.text[i + 1].clut = clut;
        setaddr(&D_800AD9FC.text[i], &D_800AD9FC.text[i + 1]);
    }
    setaddr(&D_800AD9FC.icon, &D_800AD9FC.mode);

    func_80044A68(&D_800ADAB4.icon.mode, 0, 0, getTPage(D_800AD904.mode, 1, D_800AD904.image.x, D_800AD904.image.y),
                  0);
    D_800ADAB4.icon.sprt.clut = clut;
    func_80046A9C(&D_800ADAB4.icon.mode, &D_800ADAB4.icon.sprt);
    func_80044A68(&D_800ADAB4.mode, 0, 0, getTPage(D_800AD904.mode, 0, D_800AD904.image.x, D_800AD904.image.y), 0);
    setaddr(&D_800ADAB4.mode, &D_800ADAB4.text[0]);
    for (i = 0; i < 6; i++) {
        D_800ADAB4.text[i].clut = clut;
        D_800ADAB4.text[i + 1].clut = clut;
        setaddr(&D_800ADAB4.text[i], &D_800ADAB4.text[i + 1]);
    }
    D_800ADB6C[0].clut = clut;
    D_800ADB6C[1].clut = clut;
    D_800ADB6C[2].clut = clut;
    D_800ADB6C[3].clut = clut;
    D_800ADBBC[0].clut = clut;
    setaddr(&D_800ADAB4.icon, &D_800ADAB4.mode);
    setaddr(&D_800ADB6C[0], &D_800ADB6C[1]);
    setaddr(&D_800ADB6C[1], &D_800ADB6C[2]);
    setaddr(&D_800ADB6C[2], &D_800ADB6C[3]);
    termPrim(&D_800ADB6C[3]);
    setaddr(&D_800ADBBC[0], &D_800ADBBC[1]);
    for (i = 0, p = D_800ADBBC; i < 3; i++, p++) {
        p[1].clut = clut;
        setaddr(&D_800ADBBC[i + 1], &D_800ADBBC[i + 2]);
    }
    D_800ADBBC[4].clut = clut;
    D_800ADC20[0].clut = clut;
    termPrim(&D_800ADBBC[4]);
    setaddr(&D_800ADC20[0], &D_800ADC20[1]);
    for (i = 0, p = D_800ADC20; i < 3; i++, p++) {
        p[1].clut = clut;
        setaddr(&D_800ADC20[i + 1], &D_800ADC20[i + 2]);
    }
    D_800ADC20[4].clut = clut;
    D_800ADC84[0].clut = clut;
    D_800ADC84[1].clut = clut;
    D_800ADC84[2].clut = clut;
    termPrim(&D_800ADC20[4]);
    setaddr(&D_800ADC84[0], &D_800ADC84[2]);
    setaddr(&D_800ADC84[1], &D_800ADC84[2]);
    termPrim(&D_800ADC84[2]);

    func_80044A68(&D_800ADCC0.mode, 0, 0, getTPage(D_800AD904.mode, 1, D_800AD904.image.x, D_800AD904.image.y), 0);
    func_80044A68(&D_800ADCC0.icon.mode, 0, 0, getTPage(D_800AD904.mode, 0, D_800AD904.image.x, D_800AD904.image.y),
                  0);
    D_800ADCC0.icon.sprt.clut = clut;
    func_80046A9C(&D_800ADCC0.mode, &D_800ADCC0.bar);
    func_80046A9C(&D_800ADCC0.icon.mode, &D_800ADCC0.icon.sprt);
    setaddr(&D_800ADCC0.mode, &D_800ADCC0.icon);
    termPrim(&D_800ADCC0.icon);

    tpage = getTPage(D_800AD904.mode, 0, D_800AD904.image.x, D_800AD904.image.y);
    D_800ADD1C.slider.tpage = tpage;
    D_800ADD1C.face[0].clut = clut + 2;
    D_800ADD1C.face[1].clut = clut;
    D_800ADD1C.slider.clut = clut + 1;
    func_80046A9C(&D_800ADD1C.face[0], &D_800ADD1C.face[1]);
    setaddr(&D_800ADD1C.face[0], &D_800ADD1C.slider);
    termPrim(&D_800ADD1C.slider);
    tpage = getTPage(D_800AD904.mode, 0, D_800AD904.image.x, D_800AD904.image.y);
    D_800ADD8C.slider.tpage = tpage;
    D_800ADD8C.face[0].clut = clut + 2;
    D_800ADD8C.face[1].clut = clut;
    D_800ADD8C.slider.clut = clut + 1;
    D_800ADD8C.cap.clut = clut;
    func_80046A9C(&D_800ADD8C.face[0], &D_800ADD8C.face[1]);
    setaddr(&D_800ADD8C.face[0], &D_800ADD8C.slider);
    setaddr(&D_800ADD8C.slider, &D_800ADD8C.cap);
    termPrim(&D_800ADD8C.cap);
    D_800ADDFC[0].clut = clut;
    D_800ADDFC[1].clut = clut;
    func_80046A9C(&D_800ADDFC[0], &D_800ADDFC[1]);
    termPrim(&D_800ADDFC[0]);
    D_800ADE24[0].clut = clut;
    D_800ADE24[1].clut = clut;
    func_80046A9C(&D_800ADE24[0], &D_800ADE24[1]);
    termPrim(&D_800ADE24[0]);
    D_800ADE4C[0].clut = clut;
    D_800ADE4C[1].clut = clut;
    func_80046A9C(&D_800ADE4C[0], &D_800ADE4C[1]);
    termPrim(&D_800ADE4C[0]);

    for (i = 3; i >= 0; i--) {
        D_800ADE74[i].clut = clut;
    }
    for (i = 4; i < 8; i++) {
        D_800ADE74[i].clut = clut + 3;
    }
    for (i = 0; i < 7; i++) {
        setaddr(&D_800ADE74[i], &D_800ADE74[i + 1]);
    }
    D_800ADF14.clut = clut + 4;
    D_800ADF14.u0 = 0;
    D_800ADF14.v0 = 0x60;
    termPrim(&D_800ADF00);
    termPrim(&D_800ADF14);
    for (i = 0; i < 5; i++) {
        D_800AE284[i].clut = clut + 7;
        setaddr(&D_800AE284[i], &D_800AE284[i + 1]);
    }
    D_800AE284[5].clut = clut + 7;
    termPrim(&D_800AE284[5]);

    func_80044A68(&D_800ADF28, 0, 0, getTPage(D_800AD904.mode, 1, D_800AD904.image.x, D_800AD904.image.y), 0);
    addPrim(&D_801BCF08[14], &D_800ADF28);
    func_80044A68(&D_800ADF34.mode, 0, 0, getTPage(D_800AD91C.mode, 1, D_800AD91C.image.x, D_800AD91C.image.y),
                  0);
    func_80046A9C(&D_800ADF34.mode, &D_800ADF34.sprt);
    termPrim(&D_800ADF34);
    for (i = 0; i < 14; i++) {
        D_800ADF50[i].clut = clut + 5;
        setaddr(&D_800ADF50[i], &D_800ADF50[i + 1]);
    }
    D_800ADF50[14].clut = clut + 5;
    termPrim(&D_800ADF50[14]);
    for (i = 0; i < 16; i++) {
        D_800AE07C[i].clut = clut + 5;
        setaddr(&D_800AE07C[i], &D_800AE07C[i + 1]);
    }
    D_800AE07C[16].clut = clut + 5;
    termPrim(&D_800AE07C[16]);
    for (i = 0; i < 8; i++) {
        D_800AE1D0[i].clut = clut + 5;
        setaddr(&D_800AE1D0[i], &D_800AE1D0[i + 1]);
    }
    D_800AE1D0[8].clut = clut + 5;
    termPrim(&D_800AE1D0[8]);
    for (i = 0; i < 4; i++) {
        D_800AE2FC[i].clut = clut + 5;
        setaddr(&D_800AE2FC[i], &D_800AE2FC[i + 1]);
    }
    D_800AE2FC[4].clut = clut + 5;
    termPrim(&D_800AE2FC[4]);
    for (i = 0; i < 7; i++) {
        D_800AE360[i].clut = clut + 5;
        setaddr(&D_800AE360[i], &D_800AE360[i + 1]);
    }
    D_800AE360[7].clut = clut + 5;
    termPrim(&D_800AE360[7]);
    for (i = 0; i < 9; i++) {
        D_800AE4DC[i].clut = clut + 5;
        setaddr(&D_800AE4DC[i], &D_800AE4DC[i + 1]);
    }
    D_800AE4DC[9].clut = clut + 5;
    termPrim(&D_800AE4DC[9]);
    for (i = 0; i < 10; i++) {
        D_800AE400[i].clut = clut + 5;
        setaddr(&D_800AE400[i], &D_800AE400[i + 1]);
    }
    D_800AE400[10].clut = clut + 5;
    termPrim(&D_800AE400[10]);
    D_800AE400[9].u0 = D_800AD8DC / 10 << 4;
    D_800AE400[10].u0 = D_800AD8DC % 10 << 4;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_init_hud);
#endif
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
    D_800AE4DC[5].u0 = c << 4;
    n -= d * 10000;
    d = n / 1000;
    c = d;
    if (d == 0 && lead == 1) {
        c = 12;
    } else {
        lead = 0;
    }
    D_800AE4DC[6].u0 = c << 4;
    n -= d * 1000;
    d = n / 100;
    c = d;
    if (d == 0 && lead == 1) {
        c = 12;
    } else {
        lead = 0;
    }
    D_800AE4DC[7].u0 = c << 4;
    n -= d * 100;
    d = n / 10;
    c = d;
    if (d == 0 && lead == 1) {
        c = 12;
    } else {
        lead = 0;
    }
    D_800AE4DC[8].u0 = c << 4;
    n -= d * 10;
    D_800AE4DC[9].u0 = n << 4;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_set_score);
#endif

#ifdef NON_MATCHING
/* Register choice and the stores' order. */
void submar_set_count(void)
{
    int n = D_800AD8E0 + D_800AD8E4;

    D_800AE400[6].u0 = n / 10 << 4;
    D_800AE400[7].u0 = n % 10 << 4;
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
        addPrim(D_801BCF58, &D_800AD97C[0]);
        n = 1;
    }
    if (flags & 0x400) {
        addPrim(&D_801BCF58[1], &D_800AD97C[1]);
        n += 2;
    }
    if (flags & 0x4000) {
        addPrim(&D_801BCF58[2], &D_800AD97C[2]);
        n += 3;
    }
    if (flags & 0x8000) {
        addPrim(&D_801BCF58[2], &D_800AD97C[3]);
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
    D_800ADBBC[0].y0 = n - 0x73;
    D_800ADBBC[1].y0 = D_800ADBBC[2].y0 = D_800ADBBC[3].y0 = D_800ADBBC[4].y0 = n - 0x70;
    d = y / 10;
    D_800ADBBC[4].u0 = (y - d * 10) * 4 + 200;
    y = d / 10;
    D_800ADBBC[3].u0 = (d - y * 10) * 4 + 200;
    d = y / 10;
    D_800ADBBC[2].u0 = (y - d * 10) * 4 + 200;
    D_800ADBBC[1].u0 = (d - d / 10 * 10) * 4 + 200;
    a = (D_800AE97C << 6) / (D_800B0E5C.player.speed_scale * 2);
    n = D_800AE97C * 200 / (D_800B0E5C.player.speed_scale * 2);
    D_800ADC20[0].y0 = -0x33 - a;
    D_800ADC20[1].y0 = D_800ADC20[2].y0 = D_800ADC20[3].y0 = D_800ADC20[4].y0 = -0x30 - a;
    if (n > 0) {
        D_800ADC20[1].u0 = 0xF0;
    } else if (n == 0) {
        D_800ADC20[1].u0 = 0xF8;
    } else {
        D_800ADC20[1].u0 = 0xF4;
        n = -n;
    }
    d = n / 10;
    D_800ADC20[4].u0 = (n - d * 10) * 4 + 200;
    n = d;
    d = n / 10;
    D_800ADC20[3].u0 = (n - d * 10) * 4 + 200;
    n = d;
    a = ((D_800AE8E6 + 0xC00) & 0xFFF) / 16;
    d = n / 10;
    D_800ADC20[2].u0 = (n - d * 10) * 4 + 200;
    a = (a + 0xCB) & 0xFF;
    n = 0x100 - a;
    if (n >= 0x7D) {
        D_800ADC84[0].w = 0x7D;
        D_800ADC84[0].u0 = a;
        setaddr(&D_800ADC84[0], &D_800ADC84[2]);
    } else {
        D_800ADC84[1].x0 = n - 0x3E;
        D_800ADC84[1].w = 0x7D - n;
        D_800ADC84[0].u0 = a;
        D_800ADC84[0].w = n;
        D_800ADC84[1].u0 = 0;
        setaddr(&D_800ADC84[0], &D_800ADC84[1]);
    }
    color = D_800AE8E4 ? 0x66808080 : 0x66C0C0A0;
    c = D_800AE8E4 ? 0x66808080 : 0x64606060;
    *(unsigned int *)&D_800ADB6C[0].r0 = c;
    *(unsigned int *)&D_800ADB6C[3].r0 = color;
    D_800ADB6C[3].y0 = -0x30 - D_800AE8E4 / 16;
    n = D_800AE8E0;
    if (n < 0) {
        n = 0;
    }
    a = n * 127 / 16384;
    if (a == 0 && n > 0) {
        a = 1;
    }
    D_800ADCC0.bar.x1 = D_800ADCC0.bar.x3 = a - 0x3F;
    a = n * 255 / 16384;
    *(unsigned int *)&D_800ADCC0.bar.r1 = *(unsigned int *)&D_800ADCC0.bar.r3 = a << 8 | (0xFF - a);
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
    D_800ADD1C.slider.x0 = D_800ADD1C.slider.x2 = a + 0x52;
    D_800ADD1C.slider.x1 = D_800ADD1C.slider.x3 = a + 0x5A;
    w = D_800ADD10[(unsigned short)f];
    D_800ADD1C.slider.u0 = D_800ADD1C.slider.u2 = w + a;
    D_800ADD1C.slider.u1 = D_800ADD1C.slider.u3 = w + a + 8;
    D_800ADD1C.slider.v0 = D_800ADD1C.slider.v1 = w >> 8;
    D_800ADD1C.slider.v2 = D_800ADD1C.slider.v3 = (w >> 8) + 0x1A;
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
    D_800ADD8C.slider.x0 = D_800ADD8C.slider.x2 = a - 0x7E;
    D_800ADD8C.slider.x1 = D_800ADD8C.slider.x3 = a - 0x7A;
    D_800ADD8C.slider.u0 = D_800ADD8C.slider.u2 = n + 0x70;
    D_800ADD8C.slider.u1 = D_800ADD8C.slider.u3 = n + 0x74;
    a = D_800AD950 + 1;
    if (a >= 0x20) {
        a = 0;
    }
    D_800AD950 = a;
    D_800ADDFC[1].u0 = a / 8 * 16 + 0x50;
    a = D_800AD954 + 1;
    if (a >= 0x20) {
        a = 0;
    }
    D_800AD954 = a;
    D_800ADE24[1].v0 = a / 8 * 24;
    a = D_800AD958 + 1;
    if (a >= 0x10) {
        a = 0;
    }
    D_800AD958 = a;
    D_800ADE4C[1].v0 = a / 4 * 24 + 0x81;
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
        *(unsigned int *)&D_800ADE74[i + 4].r0 = color;
        D_800ADE74[i + 4].v0 = v;
    }
    n = D_800AD3A8;
    if (n < 0) {
        n = 0;
    }
    n /= 60;
    d = n / 600;
    n -= d * 600;
    D_800AE284[2].u0 = d * 8 + 0x78;
    d = n / 60;
    n -= d * 60;
    D_800AE284[3].u0 = d * 8 + 0x78;
    d = n / 10;
    D_800AE284[4].u0 = d * 8 + 0x78;
    n -= d * 10;
    D_800AE284[5].u0 = n * 8 + 0x78;
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
        D_801BA30C.active = 1;
        D_800AE8D0.view = view;
    } else if (D_800AD45C & 8) {
        D_800AE8D0.view_toggle ^= 1;
        if (D_800AE8D0.view_toggle == 1) {
            view = 4;
        }
        submar_change_view(view);
        D_801BA30C.active = 1;
    }
    if (D_801BA30C.active) {
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
    D_801BA30C.from.vx = r;
    D_801BA30C.to.vx = v->rx;
    d = ((v->rx + 0x1000) & 0xFFF) - ((r + 0x1000) & 0xFFF);
    if (d > 0x800) {
        d -= 0x1000;
    }
    if (d < -0x800) {
        d += 0x1000;
    }
    D_801BA30C.delta.vx = d;
    r = D_800AE916;
    D_801BA30C.from.vy = r;
    D_801BA30C.to.vy = v->ry;
    d = ((v->ry + 0x1000) & 0xFFF) - ((r + 0x1000) & 0xFFF);
    if (d > 0x800) {
        d -= 0x1000;
    }
    if (d < -0x800) {
        d += 0x1000;
    }
    D_801BA30C.delta.vy = d;
    r = D_800AE918;
    D_801BA30C.from.vz = r;
    D_801BA30C.to.vz = v->rz;
    d = ((v->rz + 0x1000) & 0xFFF) - ((r + 0x1000) & 0xFFF);
    if (d > 0x800) {
        d -= 0x1000;
    }
    if (d < -0x800) {
        d += 0x1000;
    }
    D_801BA30C.delta.vz = d;
    D_801BA30C.dist_from = D_800AE944;
    D_801BA30C.dist_to = v->dist;
    D_801BA30C.dist_delta = v->dist - D_800AE944;
    D_801BA30C.height_from = D_800AD40C;
    D_801BA30C.height_to = v->height;
    D_801BA30C.height_delta = v->height - D_800AD40C;
    D_801BA30C.t = 0;
    D_801BA30C.step = 0x100;
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

void submar_move_camera(void)
{
    int t;
    int a, b, c, d, e;

    t = D_801BA30C.t;
    if (t < 0x1000) {
        gte_mtc2(8, t);
        gte_ldsv(&D_801BA30C.delta);
        gte_op(0x4B98003D);
        gte_stir3(a, b, c);
        gte_mtc2(9, D_801BA30C.dist_delta);
        gte_mtc2(10, D_801BA30C.height_delta);
        gte_op(0x4B98003D);
        D_800AE914 = D_801BA30C.from.vx + a;
        D_800AE916 = D_801BA30C.from.vy + b;
        D_800AE918 = D_801BA30C.from.vz + c;
        gte_stir2(a, b);
        d = D_801BA30C.dist_from;
        D_800AE944 = d + a;
        e = D_801BA30C.height_from;
        D_800AD40C = e + b;
        D_801BA30C.t = t + D_801BA30C.step;
    } else {
        D_800AE914 = D_801BA30C.to.vx;
        D_800AE916 = D_801BA30C.to.vy;
        D_800AE918 = D_801BA30C.to.vz;
        D_800AE944 = D_801BA30C.dist_to;
        D_800AD40C = D_801BA30C.height_to;
        D_801BA30C.t = 0;
        D_801BA30C.active = 0;
    }
}

void sub_800A8124(SVECTOR *angle, SVECTOR *axes)
{
    register int t4 FIXED_REG("$12");
    register int t5 FIXED_REG("$13");
    register int t6 FIXED_REG("$14");

    t4 = ((int *)D_8004BC98)[(angle->vx + 0x1000) & 0xFFF];
    t5 = (unsigned int)t4 >> 16;
    t6 = t4 & 0xFFFF;
    t4 = 0x1000;
    gte_ctc2(0, t4);
    gte_ctc2(1, 0);
    t4 = -t6;
    t4 <<= 16;
    t4 |= t5;
    gte_ctc2(2, t4);
    t4 = t6 << 16;
    gte_ctc2(3, t4);
    gte_ctc2(4, t5);
    t4 = ((int *)D_8004BC98)[(angle->vz + 0x1000) & 0xFFF];
    t5 = (unsigned int)t4 >> 16;
    t6 = t4 & 0xFFFF;
    t4 = t6 << 16;
    t4 = t5 | t4;
    gte_mtc2(0, t4);
    gte_mtc2(1, 0);
    gte_op(0x4A486012);
    t6 = -t6;
    t5 <<= 16;
    t4 = t6 & 0xFFFF;
    t4 = t5 | t4;
    gte_mtc2(2, t4);
    gte_mtc2(3, 0);
    t4 = 0x1000;
    gte_mtc2(4, 0);
    gte_mtc2(5, t4);
    gte_rot_v12();
    t4 = ((int *)D_8004BC98)[(angle->vy + 0x1000) & 0xFFF];
    t5 = (unsigned int)t4 >> 16;
    t6 = t4 & 0xFFFF;
    gte_ctc2(0, t5);
    gte_ctc2(1, t6);
    t4 = 0x1000;
    gte_ctc2(2, t4);
    t6 = -t6;
    t6 &= 0xFFFF;
    gte_ctc2(3, t6);
    gte_ctc2(4, t5);
    gte_op(0x4A486012);
    gte_store_svecs(axes);
}

/* Moves the player along its heading at its speed, within the sea's bounds, between the floor and the mission's
 * depth; finds the point ahead where its heading meets the surface, its cell, a mine in that cell, and steps the
 * torpedo tubes' reloading. */
#ifdef NON_MATCHING
/* The game reaches the player's x through the register holding &pitch, and register choice. */
void sub_800A832C(void)
{
    int csa, sa, x, y, z, floor, depth, dx, dy, dz, t, s, flag, err, col, row, n, i;
    unsigned int v, hi;
    short ca;
    Cell *c;

    sub_800A8124((SVECTOR *)&D_800AE8D0.pitch, &D_800AE8D0.fwd);
    csa = ((int *)D_8004BC98)[(D_800AE8D0.pitch3 + 0x1000) & 0xFFF];
    sa = csa & 0xFFFF;
    ca = (unsigned int)csa >> 16;
    csa = ((int *)D_8004BC98)[(D_800AE8D0.heading3 + 0x1000) & 0xFFF];

    dx = ca * (csa << 16 >> 16);
    D_800AE8D0.dir.vy = -sa;
    D_800AE8D0.alerts = 0;
    x = (D_800AE8D0.up.vx * D_800AE8D0.speed >> 12) + D_800AE8D0.x;
    dz = ca * (csa >> 16);
    y = (D_800AE8D0.up.vy * D_800AE8D0.speed >> 12) + D_800AE8D0.y;
    z = (D_800AE8D0.up.vz * D_800AE8D0.speed >> 12) + D_800AE8D0.z;
    D_800AE8D0.dir.vx = dx >> 12;
    D_800AE8D0.dir.vz = dz >> 12;
    floor = D_800B5F4C[(0x800 - (z >> 12)) / 64 * 64 + ((x >> 12) + 0x800) / 64].base << 12;
    depth = D_800B0E5C.floor_depth;
    if ((unsigned int)(x + 0x800000) > 0x1000000 || (unsigned int)(z + 0x800000) > 0x1000000) {
        D_800AE8D0.alerts |= 0x200;
    } else if (y >= floor) {
        if (D_800AE8D0.pitch < 0) {
            if (D_800AE8D0.pitch < -0xF) {
                D_800AE8D0.pitch += 0x10;
                D_800AE8D0.pitch2 += 0x10;
            } else {
                D_800AE8D0.pitch = 0;
                D_800AE8D0.pitch2 = 0;
            }
        }
        D_800AE8D0.alerts |= 0x200;
    } else {
        if (y < depth) {
            if (D_800AE8D0.pitch > 0) {
                if (D_800AE8D0.pitch >= 0x10) {
                    D_800AE8D0.pitch -= 0x10;
                    D_800AE8D0.pitch2 -= 0x10;
                } else {
                    D_800AE8D0.pitch = 0;
                    D_800AE8D0.pitch2 = 0;
                }
            }
            D_800AE8D0.x = x;
            D_800AE8D0.y = depth;
        } else {
            D_800AE8D0.x = x;
            D_800AE8D0.y = y;
        }
        D_800AE8D0.z = z;
    }
    x = D_800AE8D0.x;
    z = D_800AE8D0.z;
    y = D_800AE8D0.y;
    D_800AD470.cam.vz = z >> 12;
    D_800AD470.cam.vx = x >> 12;
    D_800AD470.cam.vy = y >> 12;
    D_800AD470.light.vy = D_800AE8D0.dir.vy;
    D_800AD470.light.vz = D_800AE8D0.dir.vz;
    D_800AD470.light.vx = D_800AE8D0.dir.vx;
    gte_mtc2(9, D_800AE8D0.dir.vx);
    gte_mtc2(10, D_800AE8D0.dir.vy);
    gte_mtc2(11, D_800AE8D0.dir.vz);
    dz = 0;
    dx = 0;
    /* How far along the heading the surface is, the ahead point there unless it's too far or behind. */
    dy = (0x410000 - y) >> 12;
    t = -D_800AE8D0.pitch3 % 4096;
    if (t >= 1 && t < 0x400) {
        s = D_8004BC98[((t + 0x1000) & 0xFFF) * 2];
        if (s) {
            t = (0x410000 - y) / s;
            if (t < 0x7FFF) {
                gte_mtc2(8, t);
                gte_op(0x4B98003D);
                gte_stflg(flag, err);
                if (!err) {
                    gte_stmac(dx, dy, dz);
                }
            }
        }
    }
    x = D_800AE8D0.x >> 12;
    dx += x;
    y = D_800AE8D0.y >> 12;
    dy += y;
    z = D_800AE8D0.z >> 12;
    D_800AE8D0.ahead.vx = dx;
    D_800AE8D0.ahead.vy = dy;
    D_800AE8D0.ahead.vz = dz + z;
    col = (x + 0x800) / 64;
    if (col < 0) {
        col = 0;
    }
    if (col >= 0x40) {
        col = 0x3F;
    }
    row = (0x800 - z) / 64;
    if (row < 0) {
        row = 0;
    }
    if (row >= 0x40) {
        row = 0x3F;
    }
    D_800AE8D0.cell = row * 64 + col;
    c = &D_800B5F4C[D_800AE8D0.cell];
    /* A mine just above the player's depth in its cell goes off. */
    if (c->flags & 0x2000) {
        if (!(c->flags & 0x8000) && y - c->target >= -0x3F && y - c->target < 0) {
            c->flags |= 0x8080;
            snd_request.id = 0x2B;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0x229;
            D_800AE8D0.x10 -= 0x800;
            D_800AD95C.amp += D_800AD95C.amp_start * 6;
            snd_submit_request();
        }
    }
    /* Tubes: 0 empty (starts reloading), 0x1xxxx reloading for xxxx more frames, 0x20000 loaded. */
    for (i = 0; i < 4; i++) {
        v = D_800AE984[i];
        hi = v >> 16 << 16;
        switch (hi) {
        case 0:
            v = D_800AD934 | 0x10000;
            break;
        case 0x10000:
            n = (v & 0xFFFF) - 1;
            if (n > 0) {
                v = (n & 0xFFFF) | hi;
            } else {
                v = 0x20000;
            }
            break;
        }
        D_800AE984[i] = v;
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A832C);
#endif

#ifdef NON_MATCHING
/* Register choice and order: the first block's copies go out in another order, the clamps' second halves
 * keep their result in the input's register, and the last loop's step fills two delay slots the game leaves empty. */
void submar_probe_floor(void)
{
    ProbeWork *w = PROBE_WORK;
    Probe *p;
    int x, z;
    int gear;
    int col, row;
    int ux, uy, uz;
    int vx, vy, vz;

    w->x = D_800AE8D0.x;
    w->y = D_800AE8D0.y;
    w->z = D_800AE8D0.z;
    /* The axes go over as words, each one's second word first. */
    ((int *)&w->fwd)[1] = ((int *)&D_800AE8D0.fwd)[1];
    ((int *)&w->fwd)[0] = ((int *)&D_800AE8D0.fwd)[0];
    ((int *)&w->side)[1] = ((int *)&D_800AE8D0.side)[1];
    ((int *)&w->side)[0] = ((int *)&D_800AE8D0.side)[0];
    ((int *)&w->up)[1] = ((int *)&D_800AE8D0.up)[1];
    ((int *)&w->up)[0] = ((int *)&D_800AE8D0.up)[0];
    w->gear = D_800AE8D0.gear;
    w->alerts = 0;
    ux = w->up.vx;
    uy = w->up.vy;
    uz = w->up.vz;
    vx = w->side.vx;
    vy = w->side.vy;
    vz = w->side.vz;
    setVector(&w->hull[0], ux + vx, uy + vy, uz + vz);
    setVector(&w->hull[1], ux - vx, uy - vy, uz - vz);
    setVector(&w->hull[2], -ux - vx, -uy - vy, -uz - vz);
    setVector(&w->hull[3], -ux + vx, -uy + vy, -uz + vz);
    setVector(&w->hull[4], ux, uy, uz);
    setVector(&w->hull[5], ux * 2, uy * 2, uz * 2);
    setVector(&w->hull[6], -ux, -uy, -uz);
    setVector(&w->hull[7], -ux * 2, -uy * 2, -uz * 2);
    vx = w->fwd.vx;
    vy = w->fwd.vy;
    vz = w->fwd.vz;
    setVector(&w->hull[8], ux + vx, uy + vy, uz + vz);
    setVector(&w->hull[9], ux - vx, uy - vy, uz - vz);
    setVector(&w->hull[10], -ux + vx, -uy + vy, -uz + vz);
    setVector(&w->hull[11], -ux - vx, -uy - vy, -uz - vz);
    if (w->gear >= 0) {
        setVector(&w->ahead[0], ux * 32, uy * 32, uz * 32);
        setVector(&w->ahead[1], (ux + vx) * 32, (uy + vy) * 32, (uz + vz) * 32);
        setVector(&w->ahead[2], (ux - vx) * 32, (uy - vy) * 32, (uz - vz) * 32);
    } else {
        setVector(&w->ahead[0], -(ux * 32), -(uy * 32), -(uz * 32));
        setVector(&w->ahead[1], -((ux + vx) * 32), -((uy + vy) * 32), -((uz + vz) * 32));
        setVector(&w->ahead[2], -((ux - vx) * 32), -((uy - vy) * 32), -((uz - vz) * 32));
    }
    gear = D_800AE8D0.gear;
    setVector(&w->ahead[3], ux * gear >> 12, uy * gear >> 12, uz * gear >> 12);
    setVector(&w->ahead[4], (ux + vx) * gear >> 12, (uy + vy) * gear >> 12, (uz + vz) * gear >> 12);
    setVector(&w->ahead[5], (ux - vx) * gear >> 12, (uy - vy) * gear >> 12, (uz - vz) * gear >> 12);
    w->hull[0].alert = w->hull[1].alert = w->hull[2].alert = w->hull[3].alert = 0x400;
    w->hull[4].alert = w->hull[5].alert = w->hull[6].alert = w->hull[7].alert = w->hull[8].alert = w->hull[9].alert =
        w->hull[10].alert = w->hull[11].alert = 0x500;
    w->ahead[3].alert = w->ahead[0].alert = 1;
    w->ahead[4].alert = w->ahead[1].alert = 8;
    w->ahead[5].alert = w->ahead[2].alert = 4;

    /* Each point's cell and depth. */
    p = w->hull;
    do {
        x = ((p->vx << 6) + w->x) >> 12;
        z = ((p->vz << 6) + w->z) >> 12;
        x = (x + 0x800) / 64;
        col = x < 0 ? 0 : x;
        x = col < 64 ? col : 63;
        z = (0x800 - z) / 64;
        row = z < 0 ? 0 : z;
        z = row < 64 ? row : 63;
        p->cell = z * 64 + x;
        p->h = ((p->vy << 6) + w->y) >> 12;
    } while (++p <= &w->hull[11]);
    /* The corners touching a cell on its way up, and any point under the floor. */
    p = w->hull;
    do {
        if ((D_800B5F4C[p->cell].flags & 0x2000) && (D_800B5F4C[p->cell].flags & 0x1000) &&
            !(D_800B5F4C[p->cell].flags & 0x8000) && p->h - D_800B5F4C[p->cell].target > -0x40 &&
            p->h - D_800B5F4C[p->cell].target < 0) {
            w->alerts |= p->alert & 0x400;
        }
        if (D_800B5F4C[p->cell].base < p->h) {
            w->alerts |= p->alert & 0x100;
        }
    } while (++p <= &w->hull[11]);
    /* The points ahead off the floor's edge or under it. */
    p = w->ahead;
    do {
        x = (p->vx + w->x) >> 12;
        z = (p->vz + w->z) >> 12;
        x = (x + 0x800) / 64;
        if (x < 0) {
            w->alerts |= p->alert;
            x = 0;
        }
        if (x >= 64) {
            w->alerts |= p->alert;
            x = 63;
        }
        z = (0x800 - z) / 64;
        if (z < 0) {
            w->alerts |= p->alert;
            z = 0;
        }
        if (z >= 64) {
            w->alerts |= p->alert;
            z = 63;
        }
        p->cell = z * 64 + x;
        p->h = (p->vy + w->y) >> 12;
        if (D_800B5F4C[p->cell].base < p->h && D_800B5F4C[p->cell].base < w->y >> 12) {
            w->alerts |= p->alert;
        }
    } while (++p <= &w->ahead[5]);
    D_800AE8D0.alerts |= w->alerts;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_probe_floor);
#endif
#ifdef NON_MATCHING
/* Register choice and the order of the first loads. */
void sub_800A9130(void)
{
    LineXY *p;
    unsigned int *ot;
    unsigned int c;
    int z, top, y, x, step, z1, z2;

    p = (LineXY *)D_801C5100[D_800AD3B8];
    ot = D_801B730C[D_800AD3B8];
    z = (unsigned int)D_800AE8D8 >> 12;
    gte_mtc2(1, z);
    gte_mtc2(3, z);
    gte_mtc2(5, z);
    top = D_800B5F4C[D_800AE960].base;
    y = D_800AE8D4 >> 12;
    step = top % 64;
    if (top - step < y) {
        step = top - y;
    }
    x = D_800AE8D0.x >> 12;
    if (y >= top) {
        return;
    }
    x &= 0xFFFF;
    do {
        gte_mtc2(0, top << 16 | x);
        top -= step;
        step = top - y;
        if (step > 0x40) {
            step = 0x40;
        }
        if (y >= top) {
            top = y;
        }
        gte_mtc2(2, top << 16 | x);
        gte_mtc2(4, top << 16 | x);
        gte_rtpt();
        if (top != y) {
            p->rgbc = (top / 4 << 8 & 0xFF00) | 0x4A000000;
            p->end = 0x55555555;
            if (gte_cfc2(31) < 0) {
                continue;
            }
            gte_stsxy0(&p->xy0);
            c = gte_mfc2(13);
            p->xy1 = c;
            p->xy2 = (c >> 16 << 16) | (((short)c + 2) & 0xFFFF);
            z1 = gte_mfc2(17);
            z2 = gte_mfc2(18);
            z = z2;
            if (z1 < z2) {
                z = z1;
            }
            z >>= 2;
            if (z >= 0x600) {
                break;
            }
            p->tag = (ot[z] << 8 >> 8) | 0x05000000;
            ot[z] = (unsigned int)p << 8 >> 8;
            p++;
        } else {
            p->rgbc = (top / 4 << 8 & 0xFF00) | 0x42000000;
            if (gte_cfc2(31) < 0) {
                continue;
            }
            gte_stsxy0(&p->xy0);
            gte_stsxy1(&p->xy1);
            z1 = gte_mfc2(17);
            z2 = gte_mfc2(18);
            z = z2;
            if (z1 < z2) {
                z = z1;
            }
            z >>= 2;
            if (z >= 0x600) {
                break;
            }
            p->tag = (ot[z] << 8 >> 8) | 0x03000000;
            ot[z] = (unsigned int)p << 8 >> 8;
            p = (LineXY *)&p->xy2;
        }
    } while (y < top);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A9130);
#endif

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
    D_801BA30C.from.vx = rx - 0x200;
    D_801BA30C.from.vy = D_801BA30C.to.vy = ry;
    D_801BA30C.delta.vx = 0x200;
    D_801BA30C.delta.vy = 0x1000;
    D_801BA30C.dist_delta = dist - 0x800;
    D_801BA30C.height_delta = height - 0x800;
    D_801BA30C.step = 0x20;
    D_801BA30C.active = 1;
    D_801BA30C.to.vx = rx;
    D_801BA30C.from.vz = D_801BA30C.to.vz = rz;
    D_801BA30C.delta.vz = 0;
    D_801BA30C.dist_from = 0x800;
    D_801BA30C.dist_to = dist;
    D_801BA30C.height_from = 0x800;
    D_801BA30C.height_to = height;
    D_801BA30C.t = 0;
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

#ifdef NON_MATCHING
/* Register choice for the GTE operands, as sub_800A32C8. */
void sub_800A96C8(void)
{
    register int t4 FIXED_REG("$12");
    register int t5 FIXED_REG("$13");
    register int t6 FIXED_REG("$14");
    Torpedo *t;
    VECTOR *v;
    int len;

    v = (VECTOR *)0x1F800000;
    t = D_800F5F6C;
    do {
        v[0].vx = (t->vel.vx - t->pos.vx) >> 12;
        v[0].vy = (t->vel.vy - t->pos.vy) >> 12;
        v[0].vz = (t->vel.vz - t->pos.vz) >> 12;
        t->x64 = submar_normalize_b(&v[0]);
        gte_ctc2(0, v[0].vx);
        gte_ctc2(2, v[0].vy);
        gte_ctc2(4, v[0].vz);
        gte_ldlvl(&D_800B5F3C);
        gte_op(0x4B78000C);
        gte_stlvnl_m(&v[1]);
        if (!submar_normalize_b(&v[1])) {
            gte_ldlvl(&D_800AD430);
            gte_op(0x4B78000C);
            gte_stlvnl_m(&v[1]);
            if (!submar_normalize_b(&v[1])) {
                gte_ldlvl(&D_800AD410);
                gte_op(0x4B78000C);
                gte_stlvnl_m(&v[1]);
                submar_normalize_b(&v[1]);
            }
        }
        gte_lwc2_m(9, v[0].vx);
        gte_mtc2(10, 0);
        gte_lwc2_m(11, v[0].vz);
        gte_op(0x4AA00428);
        t4 = gte_mfc2(25);
        t5 = gte_mfc2(27);
        t4 += t5;
        gte_mtc2(30, t4);
        t6 = gte_mfc2_late(31);
        t6 = t6 >> 1 << 1;
        t5 = (31 - t6) / 2;
        if (t6 == 32) {
            t4 = 0;
        }
        if (t6 < 24) {
            t4 >>= 24 - t6;
        } else {
            t4 <<= t6 - 24;
        }
        len = D_800AD48C[t4 - 0x40] << t5 >> 12;
        t->x40.vx = -submar_ratan2_b(v[0].vy, len);
        t->x40.vy = -submar_ratan2_b(v[1].vz, v[1].vx);
        t->x40.vz = 0;
    } while (++t < &D_800F5F6C[32]);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A96C8);
#endif

/* The second copies of submar_ratan2 and submar_normalize. */
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_ratan2_b);
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", submar_normalize_b);
#ifdef NON_MATCHING
/* Matches but for the last angle's 0x1000, which GCC here takes from the register the first one left it in. */
void sub_800A9AD4(SVECTOR *angle, SVECTOR *axes)
{
    int x, cx, sx, y, cy, sy, z, cz, sz, t;

    x = ((int *)D_8004BC98)[(angle->vx + 0x1000) & 0xFFF];
    gte_ctc2(1, 0);
    cx = (unsigned int)x >> 16;
    sx = x & 0xFFFF;
    gte_ctc2(0, 0x1000);
    x = -sx;
    gte_ctc2(2, x << 16 | cx);
    x = sx << 16;
    gte_ctc2(3, x);
    gte_ctc2(4, cx);
    z = ((int *)D_8004BC98)[(angle->vz + 0x1000) & 0xFFF];
    gte_mtc2(1, 0);
    cz = (unsigned int)z >> 16;
    sz = z & 0xFFFF;
    gte_mtc2(0, cz | sz << 16);
    gte_op(0x4A486012);
    sz = -sz;
    cz <<= 16;
    sz &= 0xFFFF;
    z = cz | sz;
    gte_mtc2(2, z);
    gte_mtc2(3, 0);
    gte_mtc2(4, 0);
    gte_mtc2(5, 0x1000);
    gte_rot_v12_b();
    y = ((int *)D_8004BC98)[(angle->vy + 0x1000) & 0xFFF];
    cy = (unsigned int)y >> 16;
    sy = y & 0xFFFF;
    gte_ctc2(0, cy);
    gte_ctc2(1, sy);
    t = 0x1000;
    gte_ctc2(2, t);
    t = -sy;
    sy = t & 0xFFFF;
    gte_ctc2(3, sy);
    gte_ctc2(4, cy);
    gte_op(0x4A486012);
    gte_store_svecs_b(axes);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A9AD4);
#endif

/* Steers a torpedo: turns it a fraction of the way to its wanted heading and pitch, spins it, rebuilds its axes, and
 * updates how well it points along its course, the cell it is over and its speed. */
#ifdef NON_MATCHING
/* GCC loads the second cosine signed where the game zero-extends it, and register choice. */
void sub_800A9CB0(Torpedo *t)
{
    int want, cur, step;
    short *a, *b;
    int sb, cb, speed, d;

    step = 0;
    want = (t->x40.vx + 0x1000) & 0xFFF;
    cur = (t->rot.vx + 0x1000) & 0xFFF;
    want -= cur;
    if (want > 0x800) {
        want -= 0x1000;
    }
    if (want < -0x800) {
        want += 0x1000;
    }
    if (want > 0) {
        if (!(step = want >> 4) && !(step = want >> 3) && !(step = want >> 2) && !(step = want / 2)) {
            step = want;
        }
    } else if (want < 0) {
        if (!(step = want / 16) && !(step = want / 8) && !(step = want / 4) && !(step = want / 2)) {
            step = want;
        }
    }
    cur += step;
    if (cur > 0x800) {
        cur -= 0x1000;
    }
    if (cur < -0x800) {
        cur += 0x1000;
    }
    t->rot.vx = cur;
    want = (t->x40.vy + 0x1000) & 0xFFF;
    cur = (t->rot.vy + 0x1000) & 0xFFF;
    want -= cur;
    if (want > 0x800) {
        want -= 0x1000;
    } else if (want < -0x800) {
        want += 0x1000;
    }
    if (want > 0) {
        if (!(step = want >> 4) && !(step = want >> 3) && !(step = want >> 2) && !(step = want / 2)) {
            step = want;
        }
    } else if (want < 0) {
        if (!(step = want / 16) && !(step = want / 8) && !(step = want / 4) && !(step = want / 2)) {
            step = want;
        }
    }
    cur += step;
    if (cur > 0x800) {
        cur -= 0x1000;
    }
    if (cur < -0x800) {
        cur += 0x1000;
    }
    t->rot.vy = cur;
    t->rot.vz = (t->rot.vz + 0x100) & 0xFFF;
    sub_800A9AD4(&t->rot, t->axes);
    a = &D_8004BC98[((t->x40.vx + 0x1000) & 0xFFF) * 2];
    b = &D_8004BC98[((t->x40.vy + 0x1000) & 0xFFF) * 2];
    sb = a[1] * b[0];
    cb = a[1] * b[1];
    t->x48.vy = -a[0];
    t->x48.vx = sb >> 12;
    t->x48.vz = cb >> 12;
    gte_ldL1((unsigned short)t->axes[2].vx | t->axes[2].vy << 16, (unsigned short)t->axes[2].vz);
    gte_ldv0(&t->x48);
    gte_op(0x4A4A6012);
    gte_stmac1(t->x68);
    t->x80 = ((t->pos.vx >> 12) + 0x800) / 64 + (0x800 - (t->pos.vz >> 12)) / 64 * 64;
    speed = t->x70;
    d = t->x6C - speed;
    if (d > 4) {
        speed += d / 4;
    } else if (d > 0) {
        speed++;
    } else if (d < 0) {
        speed--;
    }
    t->x70 = speed;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A9CB0);
#endif

#ifdef NON_MATCHING
/* Register choice. */
void sub_800A9FD0(Torpedo *t)
{
    int dx, dy, dz, sum, lz, sh, len, nx, ny, nz;

    dx = (t->pos.vx - D_800AE8D0.x) >> 12;
    dy = (t->pos.vy - D_800AE8D4) >> 12;
    dz = (t->pos.vz - D_800AE8D8) >> 12;
    gte_mtc2(9, dx);
    gte_mtc2(10, dy);
    gte_mtc2(11, dz);
    gte_op(0x4AA00428);
    t->to_player.vx = dx;
    t->to_player.vy = dy;
    t->to_player.vz = dz;
    gte_stmac(sum, lz, sh);
    sum += lz;
    sum += sh;
    gte_mtc2(30, sum);
    gte_mtc2(9, dx);
    gte_mtc2(10, dy);
    gte_mtc2(11, dz);
    gte_stlzc(lz);
    lz = lz >> 1 << 1;
    sh = (31 - lz) / 2;
    if (lz == 32) {
        sum = 0;
    }
    if (lz < 24) {
        sum >>= 24 - lz;
    } else {
        sum <<= lz - 24;
    }
    gte_mtc2(8, D_800AD60C[sum - 0x40]);
    len = D_800AD48C[sum - 0x40];
    gte_op_now(0x4B90003D);
    t->dist = len << sh >> 12;
    gte_stmac(nx, ny, nz);
    nx >>= sh;
    ny >>= sh;
    nz >>= sh;
    gte_ctc2(8, (nx & 0xFFFF) | ny << 16);
    gte_ctc2(9, (nz & 0xFFFF) | -nx << 16);
    gte_ctc2(10, (-ny & 0xFFFF) | -nz << 16);
    gte_ldv0(&D_800AE8FC);
    gte_op(0x4A4A6012);
    gte_ldv1(&D_800AE8EC);
    nx = gte_mfc2(25);
    gte_op_now(0x4A4AE012);
    t->angle = nx;
    t->side = gte_mfc2(25);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800A9FD0);
#endif

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


/* Moves the torpedoes: steers them, runs them along their heading, drops one out of the sea, leaves trail points,
 * sets off a mine one passes over or ends it on the floor, then steps its state: 2 running (hits its target when
 * close, loses a dead or far one), 4 lost, 8/0x10/0x20 exploding (mine, spent, hit) for a while, then 1 free. */
#ifdef NON_MATCHING
/* Register choice and one more nop. */
void sub_800AA1D0(void)
{
    Torpedo *t;
    SVECTOR *p;
    Cell *c;
    int x, y, z, dx, dy, dz;

    sub_800A96C8();
    t = D_800F5F6C;
    do {
        if (t->flags & 2) {
            sub_800A9CB0(t);
        }
        x = t->pos.vx;
        y = t->pos.vy;
        z = t->pos.vz;
        if (t->flags & 6) {
            dx = t->axes[2].vx * t->x70;
            dy = t->axes[2].vy * t->x70;
            dz = t->axes[2].vz * t->x70;
            x += dx >> 12;
            y += dy >> 12;
            z += dz >> 12;
        }
        if ((unsigned int)(x + 0x800000) > 0x1000000 || (unsigned int)(z + 0x800000) > 0x1000000 ||
            (unsigned int)y > 0x400000) {
            t->flags = 1;
            t->x74 = 0;
            t->target = 0;
        } else {
            t->pos.vx = x;
            t->pos.vy = y;
            t->pos.vz = z;
        }
        if (t->flags & 6) {
            if (--t->x88 <= 0) {
                x = t->pos.vx;
                p = t->trail[4];
                y = t->pos.vy;
                p->vx = x >> 12;
                p->vy = y >> 12;
                p->vz = t->pos.vz >> 12;
                t->trail[4] = t->trail[3];
                t->trail[3] = t->trail[2];
                t->trail[2] = t->trail[1];
                t->trail[1] = t->trail[0];
                t->trail[0] = p;
                t->x88 = D_800AEA08;
            }
        }
        if (t->flags & 6) {
            sub_800A9FD0(t);
            if (t->flags & 6) {
                c = &D_800B5F4C[t->x80];
                if (c->flags & 0x2000) {
                    if (!(c->flags & 0x8000) && (t->pos.vy >> 12) - c->target >= -0x3F &&
                        (t->pos.vy >> 12) - c->target < 0) {
                        t->flags = 8;
                        t->x74 = 12;
                        t->target = 0;
                        D_800B5F4C[t->x80].flags |= 0x8080;
                        snd_request.id = 0x2B;
                        snd_request.arg[0] = 0x40;
                        snd_request.arg[1] = 0x229;
                        snd_submit_request();
                    }
                }
                if (D_800B5F4C[t->x80].base - (t->pos.vy >> 12) < 0) {
                    t->flags = 8;
                    t->x74 = 12;
                    t->target = 0;
                }
            }
        }
        switch (t->flags) {
        case 2:
            if (t->x74 > 0) {
                if (t->x64 < t->x78) {
                    if (t->target) {
                        snd_request.id = 0x2B;
                        snd_request.arg[0] = 0x40;
                        snd_request.arg[1] = 0x22A;
                        snd_submit_request();
                        if (t->target == (Sub *)&D_800AE8D0) {
                            D_800AD95C.amp += D_800AD95C.amp_start;
                        }
                        t->target->value -= 0x100;
                        t->target = 0;
                    }
                    t->flags = 0x20;
                    t->x74 = 12;
                } else if (t->x68 < 0x800 || (t->target && t->target->value <= 0)) {
                    t->flags = 4;
                    t->target = 0;
                    if (t->x74 >= 0x20) {
                        t->x74 = 0x20;
                    }
                    /* The player's own torpedoes are the first 16. */
                    if (t < &D_800F5F6C[16]) {
                        D_800AE9FC++;
                    }
                } else {
                    t->vel.vx = t->target->x;
                    t->vel.vy = t->target->y;
                    t->vel.vz = t->target->z;
                }
                t->x74--;
            } else {
                t->flags = 0x10;
                t->target = 0;
                t->x74 = 12;
                if (t < &D_800F5F6C[16]) {
                    D_800AE9FC++;
                }
            }
            break;
        case 4:
            if (t->x74 > 0) {
                if (t->x64 < t->x78) {
                    t->flags = 0x10;
                    t->x74 = 12;
                }
                t->x74--;
            } else {
                t->flags = 0x10;
                t->x74 = 12;
            }
            break;
        case 8:
        case 0x10:
        case 0x20:
            if (--t->x74 <= 0) {
                t->flags = 1;
                t->x74 = 0;
                t->target = 0;
            }
            break;
        }
    } while (++t < &D_800F5F6C[32]);
    gte_SetRotMatrix(&D_800F5F4C);
    gte_SetTransMatrix(&D_800F5F4C);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800AA1D0);
#endif

/* Draws each torpedo's trail: its position and the five earlier ones, widened either side and joined as Gouraud
 * quads in the colours of its state. */
#ifdef NON_MATCHING
/* GCC makes the torpedo pointer an induction variable; the game keeps the index and the offset. */
void sub_800AA714(void)
{
    TrailWork *w;
    Torpedo *t;
    PolyG4W *p;
    SVECTOR *v;
    unsigned int *ot, *cp, *src, *dst, i, f;
    int x, y, z, z0, z1, z2, fl, wx, wy, wz, xy0, xy1;
    unsigned int c0, c1;

    w = (TrailWork *)0x1F800000;
    w->code = 0x3A000000;
    w->prim = (PolyG4W *)D_801BFA18[D_800AD3B8];
    w->ot = D_801B730C[D_800AD3B8];
    /* One word more than the palettes; the trail's colours copied next take it back. */
    dst = w->pals[0];
    src = D_800AE994;
    i = 0;
    do {
        *dst++ = *src++;
    } while (i++ < 20);
    dst = w->trail_colors;
    src = D_800AE9E4;
    i = 0;
    do {
        *dst++ = *src++;
    } while (i++ < 6);
    w->colors = w->trail_colors;
    gte_SetRotMatrix(&D_800F5F4C);
    gte_SetTransMatrix(&D_800F5F4C);
    for (i = 0; i < 32; i++) {
        f = D_800F5F6C[i].flags;
        if (f == 1) {
            continue;
        }
        if (f == 2) {
            cp = w->colors;
            cp[1] = cp[0] = w->pals[0][0];
        } else if (f == 4) {
            cp = w->colors;
            cp[1] = cp[0] = w->pals[1][0];
        } else if (f == 0x20) {
            cp = w->colors;
            cp[1] = cp[0] = w->pals[3][0];
        } else if (f == 8) {
            cp = w->colors;
            cp[1] = cp[0] = w->pals[4][0];
        } else {
            cp = w->colors;
            cp[1] = cp[0] = w->pals[2][0];
        }
        t = &D_800F5F6C[i];
        t = &D_800F5F6C[i];
        wx = (t->axes)->vx >> 10;
        wy = (t->axes)->vy >> 10;
        wz = (t->axes)->vz >> 10;
        w->width.vx = wx;
        w->width.vy = wy;
        w->width.vz = wz;
        x = t->pos.vx >> 12;
        y = t->pos.vy >> 12;
        z = t->pos.vz >> 12;
        TRAIL_PAIR(0);
        v = t->trail[0];
        x = v->vx;
        y = v->vy;
        z = v->vz;
        TRAIL_PAIR(2);
        gte_ldv3c(&w->pts[0]);
        v = t->trail[1];
        gte_op_now(0x4A280030);
        x = v->vx;
        y = v->vy;
        z = v->vz;
        TRAIL_PAIR(4);
        v = t->trail[2];
        x = v->vx;
        y = v->vy;
        z = v->vz;
        TRAIL_PAIR(6);
        fl = gte_cfc2(31);
        gte_stsxy3_m(w->sxy[0], w->sxy[1], w->sxy[2]);
        w->flag[0] = fl;
        TRAIL_OTZ(0);
        gte_ldv3c(&w->pts[3]);
        v = t->trail[3];
        gte_op_now(0x4A280030);
        wx = w->width.vx;
        wy = w->width.vy;
        wz = w->width.vz;
        x = v->vx;
        y = v->vy;
        z = v->vz;
        TRAIL_PAIR(8);
        v = t->trail[4];
        x = v->vx;
        y = v->vy;
        z = v->vz;
        TRAIL_PAIR(10);
        fl = gte_cfc2(31);
        gte_stsxy3_m(w->sxy[3], w->sxy[4], w->sxy[5]);
        w->flag[1] = fl;
        TRAIL_OTZ(1);
        gte_ldv3c(&w->pts[6]);
        gte_op(0x4A280030);
        if (w->flag[0] >= 0 && w->otz[0] < 0x600) {
            TRAIL_QUAD(0, 0, 1, 2, 3, 0);
        }
        if (w->flag[1] >= 0 && w->otz[0] < 0x600) {
            TRAIL_QUAD(0, 2, 3, 4, 5, 1);
        }
        fl = gte_cfc2(31);
        gte_stsxy3_m(w->sxy[0], w->sxy[1], w->sxy[2]);
        w->flag[0] = fl;
        TRAIL_OTZ(0);
        gte_ldv3c(&w->pts[9]);
        gte_op(0x4A280030);
        if (w->flag[1] >= 0 && w->flag[0] >= 0 && w->otz[1] < 0x600) {
            TRAIL_QUAD(1, 4, 5, 0, 1, 2);
        }
        fl = gte_cfc2(31);
        gte_stsxy3_m(w->sxy[3], w->sxy[4], w->sxy[5]);
        w->flag[1] = fl;
        TRAIL_OTZ(1);
        if (w->flag[0] >= 0 && w->otz[0] < 0x600) {
            TRAIL_QUAD(0, 0, 1, 2, 3, 3);
        }
        if (w->flag[1] >= 0 && w->otz[1] < 0x600) {
            TRAIL_QUAD(1, 2, 3, 4, 5, 4);
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800AA714);
#endif

#ifdef NON_MATCHING
/* Which addresses GCC strength-reduces into their own registers. */
void submar_init_torpedoes(void)
{
    unsigned int i;

    for (i = 0; i < 32; i++) {
        setVector(&D_800F5F6C[i].pos, 0, 0, 0);
        setVector(&D_800F5F6C[i].vel, 0, 0, 0);
        setVector(&D_800F5F6C[i].rot, 0, 0, 0);
        setVector(&D_800F5F6C[i].axes[2], 0, 0, 0x1000);
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

/* Draws a marker on each listed cell the sonar has found: the ping's ring, moving out to its range, shows the cells
 * it passes (and marks them found), shaded by distance and by how far behind the ring's front they are. */
#ifdef NON_MATCHING
/* GCC lifts the shade's address out of the loop, which moves every register after it. */
void sub_800AB518(void)
{
    MarkWork *w;
    PolyF3W *p;
    SVECTOR *v;
    Cell *c;
    unsigned int f, *ot, *dst, *src, i;
    int x, y, z, vx, vy, vz, sum, lz, sh, r, mac0, otz, flag;

    w = (MarkWork *)0x1F800000;
    w->faces = w->fbuf;
    w->pt = D_801BCF78;
    w->verts = w->vbuf;
    w->ring = 1;
    p = (PolyF3W *)D_800F786C[D_800AD3B8];
    w->ot = D_801B730C[D_800AD3B8];
    w->n = D_800B1E68;
    w->px = D_800AE8D0.x >> 12;
    w->py = D_800AE8D4 >> 12;
    w->pz = D_800AE8D8 >> 12;
    w->ring_width = D_800AEA14;
    w->range = D_800AEA10;
    if (D_800AEA0C) {
        r = D_800AEA1C + D_800AEA18;
        if (D_800AEA10 + D_800AEA14 < r) {
            r = 1;
            D_800AEA0C = 0;
        }
        D_800AEA1C = r;
        w->ring = r;
    }
    w->code = 0x22000000;
    dst = (unsigned int *)w->verts;
    src = D_800AEA20;
    for (i = 0; i < 12; i++) {
        *dst++ = *src++;
    }
    dst = w->faces;
    src = D_800AEA50;
    for (i = 0; i < 8; i++) {
        *dst++ = *src++;
    }
    do {
        v = w->pt;
        x = v->vx;
        y = v->vy;
        z = v->vz;
        sum = x - w->px;
        lz = y - w->py;
        sh = z - w->pz;
        gte_mtc2(9, sum);
        gte_mtc2(10, lz);
        gte_mtc2(11, sh);
        gte_op(0x4AA00428);
        c = &D_800B5F4C[v->pad];
        if (!(c->flags & 0x2000)) {
            goto next;
        }
        if (c->flags & 0x8000) {
            w->dist = 0;
            w->shade = (c->flags & 0xFF) << 5;
        } else {
            gte_stmac(sum, lz, sh);
            sum += lz;
            sum += sh;
            gte_mtc2(30, sum);
            lz = gte_mfc2_late(31);
            lz = lz >> 1 << 1;
            sh = (31 - lz) / 2;
            if (lz == 32) {
                sum = 0;
            }
            if (lz < 24) {
                sum >>= 24 - lz;
            } else {
                sum <<= lz - 24;
            }
            w->dist = D_800AD48C[sum - 0x40] << sh >> 12;
            if (w->dist >= w->range) {
                goto next;
            }
            r = w->ring - w->dist;
            if (r < 0 || w->ring_width < r) {
                goto next;
            }
            D_800B5F4C[w->pt->pad].flags |= 0x1000;
            w->shade = ((w->range - w->dist) << 12) / w->range * (w->ring_width - (w->ring - w->dist)) /
                       w->ring_width;
        }
        for (w->face = 0; w->face < 8; w->face++) {
            f = w->faces[w->face];
            v = &w->verts[f & 0xFF];
            vx = v->vx + x;
            vy = v->vy + y;
            vz = v->vz + z;
            gte_mtc2(0, (vx & 0xFFFF) | vy << 16);
            gte_mtc2(1, vz);
            v = &w->verts[f >> 8 & 0xFF];
            vx = v->vx + x;
            vy = v->vy + y;
            vz = v->vz + z;
            gte_mtc2(2, (vx & 0xFFFF) | vy << 16);
            gte_mtc2(3, vz);
            v = &w->verts[f >> 16 & 0xFF];
            vx = v->vx + x;
            vy = v->vy + y;
            vz = v->vz + z;
            gte_mtc2(4, (vx & 0xFFFF) | vy << 16);
            gte_mtc2(5, vz);
            gte_op(0x4A280030);
            gte_mtc2(6, w->code | D_800AEA70[w->face]);
            gte_mtc2(8, 0);
            gte_lwc2_m(9, w->shade);
            gte_lwc2_m(10, w->shade);
            gte_lwc2_m(11, w->shade);
            flag = gte_cfc2(31);
            gte_op_now(0x4B400006);
            if (flag < 0) {
                break;
            }
            gte_stsxy3_f3(p);
            mac0 = gte_mfc2(24);
            gte_op_now(0x4B58002D);
            if (mac0 <= 0) {
                continue;
            }
            otz = gte_mfc2(7);
            gte_op_now(0x4A680029);
            if (otz >= 0x600) {
                continue;
            }
            ot = &w->ot[otz];
            p->tag = (*ot << 8 >> 8) | 0x04000000;
            *ot = (unsigned int)p << 8 >> 8;
            p++;
            gte_strgb2(&p[-1].rgbc);
        }
    next:
        w->pt++;
    } while (--w->n);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800AB518);
#endif

#ifdef NON_MATCHING
/* Register choice for the second angle's sine and cosine. */
void sub_800ABA8C(SVECTOR *angle, MATRIX *m)
{
    int x, cx, sx, y, cy, sy, z, cz, sz;

    x = ((int *)D_8004BC98)[(angle->vx + 0x1000) & 0xFFF];
    cx = (unsigned int)x >> 16;
    sx = x & 0xFFFF;
    gte_ctc2(0, 0x1000);
    gte_ctc2(1, 0);
    x = -sx;
    gte_ctc2(2, x << 16 | cx);
    x = sx << 16;
    gte_ctc2(3, x);
    gte_ctc2(4, cx);
    z = ((int *)D_8004BC98)[(angle->vz + 0x1000) & 0xFFF];
    sz = z & 0xFFFF;
    cz = (unsigned int)z >> 16;
    gte_mtc2(0, cz | sz << 16);
    gte_mtc2(1, 0);
    sz = -sz;
    cz <<= 16;
    sz &= 0xFFFF;
    z = cz | sz;
    gte_mtc2(2, z);
    gte_mtc2(3, 0);
    gte_mtc2(4, 0);
    gte_mtc2(5, 0x1000);
    gte_op(0x4A486012);
    gte_rot_v12();
    y = ((int *)D_8004BC98)[(angle->vy + 0x1000) & 0xFFF];
    cy = (unsigned int)y >> 16;
    sy = y & 0xFFFF;
    gte_ctc2(0, cy);
    gte_ctc2(1, sy);
    gte_ctc2(2, 0x1000);
    gte_ctc2(3, -sy & 0xFFFF);
    gte_ctc2(4, cy);
    gte_op(0x4A486012);
    gte_rot_v12();
    gte_SetRotMatrix(&D_800F5F4C);
    gte_op(0x4A486012);
    gte_store_cols(m);
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800ABA8C);
#endif

/* Draws a model at D_800AEA90 turned by the angles at D_800AEA98, its faces Gouraud-shaded triangles; returns the next
 * free primitive. */
#ifdef NON_MATCHING
/* Register choice inside the face loop. */
void *sub_800ABD4C(void *prims, void *kind)
{
    Model *model = kind;
    ModelWork *w;
    PolyG3W *p;
    unsigned int f, *ot, *col;
    SVECTOR *a, *b, *c;
    int x, cx, sx, y, cy, sy, z, cz, sz, i, flag, otz, xa, xb, xc, ya, yb, yc, za, zb, zc, vx, vy, vz;

    p = prims;
    w = MODEL_WORK;
    x = ((int *)D_8004BC98)[(D_800AEA98.vx + 0x1000) & 0xFFF];
    cx = (unsigned int)x >> 16;
    sx = x & 0xFFFF;
    gte_ctc2(0, 0x1000);
    gte_ctc2(1, 0);
    x = -sx;
    gte_ctc2(2, x << 16 | cx);
    x = sx << 16;
    gte_ctc2(3, x);
    gte_ctc2(4, cx);
    z = ((int *)D_8004BC98)[(D_800AEA98.vz + 0x1000) & 0xFFF];
    sz = z & 0xFFFF;
    cz = (unsigned int)z >> 16;
    gte_mtc2(0, cz | sz << 16);
    gte_mtc2(1, 0);
    sz = -sz;
    cz <<= 16;
    sz &= 0xFFFF;
    z = cz | sz;
    gte_mtc2(2, z);
    gte_mtc2(3, 0);
    gte_mtc2(4, 0);
    gte_mtc2(5, 0x1000);
    gte_op(0x4A486012);
    gte_rot_v12();
    y = ((int *)D_8004BC98)[(D_800AEA98.vy + 0x1000) & 0xFFF];
    sy = y & 0xFFFF;
    cy = (unsigned int)y >> 16;
    gte_ctc2(0, cy);
    gte_ctc2(1, sy);
    gte_ctc2(2, 0x1000);
    gte_ctc2(3, -sy & 0xFFFF);
    gte_ctc2(4, cy);
    gte_op(0x4A486012);
    gte_rot_v12();
    {
    register int t4 FIXED_REG("$12");
    register int t5 FIXED_REG("$13");
    register int t6 FIXED_REG("$14");
        gte_SetRotMatrix(&D_800F5F4C);
        gte_op(0x4A486012);
        t4 = gte_mfc2(9);
        t5 = gte_mfc2(10);
        t6 = gte_mfc2(11);
        gte_op(0x4A48E012);
        w->m.m[0][0] = t4;
        w->m.m[1][0] = t5;
        w->m.m[2][0] = t6;
        t4 = gte_mfc2(9);
        t5 = gte_mfc2(10);
        t6 = gte_mfc2(11);
        gte_op(0x4A496012);
        w->m.m[0][1] = t4;
        w->m.m[1][1] = t5;
        w->m.m[2][1] = t6;
        t4 = gte_mfc2(9);
        t5 = gte_mfc2(10);
        t6 = gte_mfc2(11);
        w->m.m[0][2] = t4;
        w->m.m[1][2] = t5;
        w->m.m[2][2] = t6;
        gte_SetTransMatrix(&D_800F5F4C);
        gte_ldv0(&D_800AEA90);
        gte_op(0x4A480012);
        gte_SetRotMatrix(&w->m);
        t4 = gte_mfc2(25);
        t5 = gte_mfc2(26);
        t6 = gte_mfc2(27);
        gte_ctc2(5, t4);
        gte_ctc2(6, t5);
        gte_ctc2(7, t6);
    }
    w->ot = D_801B730C[D_800AD3B8];
    w->verts = model->verts;
    w->faces = model->faces;
    w->colors = model->colors;
    w->code = 0x30000000;
    w->n = model->n;
    f = w->faces[0];
    a = &w->verts[f & 0xFF];
    vx = (unsigned short)a->vx;
    vy = a->vy;
    vx |= vy << 16;
    vz = a->vz;
    gte_mtc2(0, vx);
    gte_mtc2(1, vz);
    a = &w->verts[f >> 8 & 0xFF];
    vx = (unsigned short)a->vx;
    vy = a->vy;
    vx |= vy << 16;
    vz = a->vz;
    gte_mtc2(2, vx);
    gte_mtc2(3, vz);
    a = &w->verts[f >> 16 & 0xFF];
    vx = (unsigned short)a->vx;
    vy = a->vy;
    vx |= vy << 16;
    vz = a->vz;
    gte_mtc2(4, vx);
    gte_mtc2(5, vz);
    __asm__ volatile("nop");
    i = 1;
    do {
        /* The next face's vertices go in while this one is checked. */
        gte_op_now(0x4A280030);
        f = w->faces[i];
        a = &w->verts[f & 0xFF];
        b = &w->verts[f >> 8 & 0xFF];
        c = &w->verts[f >> 16 & 0xFF];
        xa = (unsigned short)a->vx;
        xb = (unsigned short)b->vx;
        ya = a->vy;
        xc = (unsigned short)c->vx;
        yb = b->vy;
        xa |= ya << 16;
        xb |= yb << 16;
        yc = c->vy;
        zb = b->vz;
        zc = c->vz;
        xc |= yc << 16;
        za = a->vz;
        flag = gte_cfc2(31);
        gte_op_now(0x4B400006);
        gte_mtc2(0, xa);
        gte_mtc2(1, za);
        gte_mtc2(2, xb);
        gte_mtc2(3, zb);
        gte_mtc2(4, xc);
        gte_mtc2(5, zc);
        if (flag < 0 || gte_mfc2(24) <= 0) {
            continue;
        }
        gte_op_now(0x4B58002D);
        col = w->colors[i - 1];
        p->rgbc0 = w->code | col[0];
        p->rgb1 = col[1];
        p->rgb2 = col[2];
        otz = gte_mfc2(7);
        gte_stsxy0_g3(p);
        if (otz >= 0x600) {
            continue;
        }
        gte_stsxy12_g3(p);
        ot = &w->ot[otz];
        p->tag = (*ot << 8 >> 8) | 0x06000000;
        *ot = (unsigned int)p << 8 >> 8;
        p++;
    } while (++i < w->n);
    return p;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800ABD4C);
#endif

/* The sea floor: a grid of 63x63 cells around the player, each two triangles shaded by depth under a wire line; cells
 * out of view keep only the line. Counts what it drew. */
#ifdef NON_MATCHING
/* The game keeps its state in the scratchpad and saves only s0-s4; GCC here keeps more in registers. */
void sub_800AC2FC(void)
{
    FloorWork *w;
    Cell *cell;
    unsigned short *h;
    LineXY *p;
    PolyG3W *t;
    unsigned int *ot;
    unsigned int code;
    unsigned int row, col;
    int c, r, i, flag, z, z2, z3, sxy0, sxy1, sxy2;

    w = FLOOR_WORK;
    w->tri = (PolyG3W *)D_8011E96C[D_800AD3B8];
    w->line = (LineXY *)D_800C5F4C[D_800AD3B8];
    w->ot = D_801B730C[D_800AD3B8];
    w->line_rgbc = D_800AEAE0 | 0x4A000000;
    code = 0x30000000;
    if (D_800AEB30) {
        code = 0x32000000;
    }
    w->tri_code = code;
    c = (0x7E0 - D_800AE95C) / 64;
    r = (D_800AE958 + 0x7E0) / 64;

    if ((w->row0 = c + D_800AD46C) >= 0x40) {
        w->row0 = 0x3F;
    }
    if (w->row0 < 0) {
        w->row0 = 0;
    }
    if ((w->row1 = c + D_800AD46E) >= 0x40) {
        w->row1 = 0x3F;
    }
    if (w->row1 < 0) {
        w->row1 = 0;
    }
    if ((w->col0 = r + D_800AD468) >= 0x40) {
        w->col0 = 0x40;
    }
    if (w->col0 < 0) {
        w->col0 = 0;
    }
    if ((w->col1 = r + D_800AD46A) >= 0x40) {
        w->col1 = 0x40;
    }
    if (w->col1 < 0) {
        w->col1 = 0;
    }
    i = 0;
    do {
        w->shades[i] = D_800AEAE4[i];
    } while (++i < 17);
    i = 0;
    do {
        w->row_h[i] = D_800B5F4C[i].base;
    } while (++i < 64);
    w->half_height = D_800AD40C / 2;
    row = 1;
    cell = &D_800B5F4C[64];
    w->v[0].v.vz = 0x7E0;
    w->v[1].v.vz = 0x7E0;
    w->v[2].v.vz = 0x7A0;
    w->v[3].v.vz = 0x7A0;
    do {
        h = &w->row_h[1];
        col = 1;
        w->v[0].v.vx = -0x7E0;
        w->v[2].v.vx = -0x7E0;
        w->v[1].v.vx = -0x7A0;
        w->v[3].v.vx = -0x7A0;
        w->v[0].v.vy = w->row_h[0];
        w->v[1].v.vy = w->row_h[1];
        w->v[2].v.vy = w->row_h[0] = cell[0].base;
        w->v[3].v.vy = w->row_h[1] = cell[1].base;
        cell++;
        do {
            gte_ldv3_m(&w->v[0], &w->v[1], &w->v[2]);
            if (row < w->row0 || w->row1 < row || col < w->col0 || w->col1 < col) {
                gte_op_now(0x4A280030);
                *(int *)&w->v[0].v = *(int *)&w->v[1].v;
                *(int *)&w->v[2].v = *(int *)&w->v[3].v;
                w->v[1].v.vx += 0x40;
                w->v[3].v.vx += 0x40;
                w->v[1].v.vy = h[1];
                w->v[3].v.vy = h[1] = cell[1].base;
                cell++;
                h++;
                flag = gte_cfc2(31);
                p = w->line;
                if (flag < 0) {
                    continue;
                }
                sxy0 = gte_mfc2(12);
                sxy1 = gte_mfc2(13);
                sxy2 = gte_mfc2(14);
                z = gte_mfc2(17);
                z2 = gte_mfc2(18);
                z3 = gte_mfc2(19);
                gte_op_now(0x4B400006);
                p->xy0 = sxy2;
                p->xy1 = sxy0;
                p->xy2 = sxy1;
                p->rgbc = w->line_rgbc;
                if (z < z2) {
                    z = z2;
                }
                if (z < z3) {
                    z = z3;
                }
                z >>= 2;
                if (z >= 0x600) {
                    continue;
                }
                p->end = 0x55555555;
                if (gte_mfc2(24) <= 0) {
                    continue;
                }
                ot = &w->ot[z];
                p->tag = (*ot << 8 >> 8) | 0x05000000;
                *ot = (unsigned int)p << 8 >> 8;
                w->line = p + 1;
                continue;
            }
            gte_op_now(0x4A280030);
            w->v[0].rgb = w->shades[(unsigned short)w->v[0].v.vy >> 6];
            w->v[1].rgb = w->shades[(unsigned short)w->v[1].v.vy >> 6];
            w->v[2].rgb = w->shades[(unsigned short)w->v[2].v.vy >> 6];
            w->v[3].rgb = w->shades[(unsigned short)w->v[3].v.vy >> 6];
            flag = gte_cfc2(31);
            z = gte_mfc2(17);
            z2 = gte_mfc2(18);
            z3 = gte_mfc2(19);
            sxy0 = gte_mfc2(12);
            sxy1 = gte_mfc2(13);
            sxy2 = gte_mfc2(14);
            gte_op_now(0x4B400006);
            p = w->line;
            w->flag = flag;
            p->rgbc = w->line_rgbc;
            p->xy0 = sxy2;
            p->xy1 = sxy0;
            p->xy2 = sxy1;
            w->sxy0 = sxy0;
            if (gte_mfc2(24) > 0) {
                if (z < z2) {
                    z = z2;
                }
                if (z < z3) {
                    z = z3;
                }
                z >>= 2;
                w->z = z;
                p->end = 0x55555555;
                if (z >= 0x600) {
                    goto next;
                }
                w->sxy1 = sxy1;
                w->sxy2 = sxy2;
                if (w->flag < 0) {
                    sub_800ACA40(&w->v[0], &w->v[1], &w->v[2]);
                } else {
                    ot = &w->ot[w->z];
                    p = w->line;
                    p->tag = (*ot << 8 >> 8) | 0x05000000;
                    *ot = (unsigned int)p << 8 >> 8;
                    w->line = p + 1;
                    t = w->tri;
                    t->rgbc0 = w->tri_code | w->v[0].rgb;
                    t->rgb1 = w->v[1].rgb;
                    t->xy0 = w->sxy0;
                    t->xy1 = w->sxy1;
                    t->rgb2 = w->v[2].rgb;
                    t->xy2 = w->sxy2;
                    t->tag = (*ot << 8 >> 8) | 0x06000000;
                    *ot = (unsigned int)t << 8 >> 8;
                    w->tri = t + 1;
                }
            }
            sub_800ACA40(&w->v[1], &w->v[3], &w->v[2]);
        next:
            *(int *)&w->v[0].v = *(int *)&w->v[1].v;
            *(int *)&w->v[2].v = *(int *)&w->v[3].v;
            w->v[1].v.vx += 0x40;
            w->v[3].v.vx += 0x40;
            w->v[1].v.vy = h[1];
            w->v[3].v.vy = h[1] = cell[1].base;
            cell++;
            h++;
        } while (++col < 0x40);
        row++;
        w->v[1].v.vz = w->v[0].v.vz -= 0x40;
        w->v[3].v.vz = w->v[2].v.vz -= 0x40;
    } while (row < 0x40);
    D_800AEB28 = w->tri - (PolyG3W *)D_8011E96C[D_800AD3B8];
    D_800AEB2C = (unsigned char *)w->line - D_800C5F4C[D_800AD3B8];
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800AC2FC);
#endif

/* Draws a sea floor triangle: whole when it's all in front, or through the clippers when one or two points are too
 * near. */
#ifdef NON_MATCHING
/* Register choice: the scratch base, the points and the first depth land elsewhere. */
void sub_800ACA40(ShadeVert *a, ShadeVert *b, ShadeVert *c)
{
    DrawScratch *sc;
    PolyG3W *p;
    unsigned int *ot;
    int flag, z0, z1, z2, near, clip, z, xy0, xy1, xy2;

    sc = (DrawScratch *)0x1F800000;
    gte_ldv3(a, b, c);
    gte_op_now(0x4A280030);
    p = sc->prim;
    p->rgbc0 = sc->code | a->rgb;
    p->rgb1 = b->rgb;
    p->rgb2 = c->rgb;
    flag = gte_cfc2(31);
    z0 = gte_mfc2(17);
    z1 = gte_mfc2(18);
    z2 = gte_mfc2(19);
    gte_op_now(0x4B400006);
    if (flag < 0 && (flag & 0x7FF9EFFF)) {
        return;
    }
    near = sc->near;
    clip = z0 < near;
    if (z1 < near) {
        clip |= 2;
    }
    if (z2 < near) {
        clip |= 4;
    }
    if (clip == 7) {
        return;
    }
    z = z0;
    if (z0 < z1) {
        z = z1;
    }
    if (z < z2) {
        z = z2;
    }
    z >>= 2;
    sc->z = z;
    if (z >= 0x600) {
        return;
    }
    sc->clip = clip;
    switch (clip) {
    case 3:
    case 4:
        sc->depth[0] = z2;
        sc->depth[1] = z0;
        sc->depth[2] = z1;
        break;
    case 2:
    case 5:
        sc->depth[0] = z1;
        sc->depth[1] = z2;
        sc->depth[2] = z0;
        break;
    case 1:
    case 6:
        sc->depth[0] = z0;
        sc->depth[1] = z1;
        sc->depth[2] = z2;
        break;
    case 0:
        break;
    case 7:
        return;
    }
    if (gte_mfc2(24) <= 0) {
        return;
    }
    switch (sc->clip) {
    case 0:
        p = sc->prim;
        gte_stsxy3r(xy0, xy1, xy2);
        p->xy0 = xy0;
        p->xy1 = xy1;
        p->xy2 = xy2;
        ot = &sc->ot[sc->z];
        p->tag = (*ot << 8 >> 8) | 0x06000000;
        *ot = (unsigned int)p << 8 >> 8;
        p++;
        sc->prim = p;
        break;
    case 1:
        sub_800ACF6C(a, b, c);
        break;
    case 2:
        sub_800ACF6C(b, c, a);
        break;
    case 3:
        sub_800ACCA8(c, a, b);
        break;
    case 4:
        sub_800ACF6C(c, a, b);
        break;
    case 5:
        sub_800ACCA8(b, c, a);
        break;
    case 6:
        sub_800ACCA8(a, b, c);
        break;
    case 7:
        break;
    }
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800ACA40);
#endif

/* Clips a triangle whose first corner is behind the near plane and the other two in front: the edges to them meet the
 * plane where the depths say, with the colours mixed to match, and the triangle drawn is the first corner's with those
 * two points. */
#ifdef NON_MATCHING
/* Register choice for the two colours. */
void sub_800ACCA8(ShadeVert *v0, ShadeVert *v1, ShadeVert *v2)
{
    int z0, num, t, x, y, z;
    unsigned int c1, c2;
    PolyG3W *p;
    unsigned int *ot;

    z0 = CLIP_SZ[0];
    num = (z0 - CLIP_NEAR) << 12;
    t = num / (z0 - CLIP_SZ[1]);
    if ((unsigned int)t > 0x1000) {
        return;
    }
    gte_mtc2(8, t);
    x = v1->v.vx;
    x -= v0->v.vx;
    y = v1->v.vy;
    y -= v0->v.vy;
    z = v1->v.vz;
    z -= v0->v.vz;
    gte_mtc2(9, x);
    gte_mtc2(10, y);
    gte_mtc2(11, z);
    gte_op(0x4B98003D);
    gte_stir3(x, y, z);
    x += v0->v.vx;
    y += v0->v.vy;
    z += v0->v.vz;
    x = y << 16 | (x & 0xFFFF);
    gte_mtc2(2, x);
    gte_mtc2(3, z);
    c1 = v1->rgb;
    x = (c1 & 0xFF) << 4;
    y = c1 >> 4 & 0xFF0;
    z = c1 >> 12 & 0xFF0;
    gte_SetFarColor(x, y, z);
    gte_ldrgb_8(v0);
    gte_op(0x4A780010);
    c1 = gte_mfc2(22);
    t = num / (z0 - CLIP_SZ[2]);
    CLIP_RGB1 = c1;
    if ((unsigned int)t > 0x1000) {
        return;
    }
    gte_mtc2(8, t);
    x = v2->v.vx;
    x -= v0->v.vx;
    y = v2->v.vy;
    y -= v0->v.vy;
    z = v2->v.vz;
    z -= v0->v.vz;
    gte_mtc2(9, x);
    gte_mtc2(10, y);
    gte_mtc2(11, z);
    gte_op(0x4B98003D);
    gte_stir3(x, y, z);
    x += v0->v.vx;
    y += v0->v.vy;
    z += v0->v.vz;
    x = y << 16 | (x & 0xFFFF);
    gte_mtc2(4, x);
    gte_mtc2(5, z);
    c2 = v2->rgb;
    x = (c2 & 0xFF) << 4;
    y = c2 >> 4 & 0xFF0;
    z = c2 >> 12 & 0xFF0;
    gte_SetFarColor(x, y, z);
    gte_ldrgb_8(v0);
    gte_op(0x4A780010);
    gte_ldv0(v0);
    c2 = gte_mfc2(22);
    CLIP_RGB2 = c2;
    gte_op_now(0x4A280030);
    p = CLIP_PRIM;
    p->rgbc0 = CLIP_CODE | v0->rgb;
    p->rgb1 = c1;
    p->rgb2 = c2;
    p->xy0 = gte_mfc2(12);
    p->xy1 = gte_mfc2(13);
    p->xy2 = gte_mfc2(14);
    ot = CLIP_OTZ + CLIP_OT;
    p->tag = (*ot << 8 >> 8) | 0x06000000;
    *ot = (unsigned int)p << 8 >> 8;
    CLIP_PRIM = p + 1;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800ACCA8);
#endif

/* Draws what's left of triangle a b c in front of the near plane when only a is behind it: a's two edges are cut
 * at the plane, the new points shaded from a's colour by how far along they are, and the quad drawn as two
 * Gouraud triangles. */
#ifdef NON_MATCHING
/* GCC here keeps more values in saved registers (s0-s5 against s0-s2). */
void sub_800ACF6C(ShadeVert *a, ShadeVert *b, ShadeVert *c)
{
    PolyG3W *p;
    unsigned int *ot;
    int t, dx, dy, dz, rgb;

    t = (*CUT_NEAR - *CUT_ZA << 12) / (*CUT_NEAR - *CUT_ZB);
    if ((unsigned int)t > 0x1000) {
        return;
    }
    gte_mtc2(8, t);
    *CUT_T = t;
    dx = a->v.vx - b->v.vx;
    dy = a->v.vy - b->v.vy;
    dz = a->v.vz - b->v.vz;
    gte_mtc2(9, dx);
    gte_mtc2(10, dy);
    gte_mtc2(11, dz);
    gte_op(0x4B98003D);
    gte_ldv1(&b->v);
    gte_ldv2(&c->v);
    gte_stir3(dx, dy, dz);
    dx += b->v.vx;
    dy += b->v.vy;
    dz += b->v.vz;
    gte_mtc2(0, (dx & 0xFFFF) | dy << 16);
    gte_mtc2(1, dz);
    gte_rtpt();
    p = *CUT_PRIM;
    rgb = a->rgb;
    gte_ctc2(21, (rgb & 0xFF) << 4);
    gte_ctc2(22, rgb >> 4 & 0xFF0);
    gte_ctc2(23, rgb >> 12 & 0xFF0);
    gte_stsxy3r(p->xy0, p->xy1, p->xy2);
    rgb = b->rgb;
    gte_mtc2(6, rgb);
    p->rgb1 = rgb;
    gte_mtc2(8, *CUT_T);
    gte_op(0x4A780010);
    p->rgb2 = c->rgb;
    rgb = gte_mfc2(22);
    *CUT_RGB = rgb;
    p->rgbc0 = *CUT_CODE | rgb << 8 >> 8;
    ot = &(*CUT_OT)[*CUT_OTZ];
    p->tag = *ot << 8 >> 8 | 0x06000000;
    *ot = (unsigned int)p << 8 >> 8;
    t = (*CUT_NEAR2 - *CUT_ZA << 12) / (*CUT_NEAR2 - *CUT_ZB);
    *CUT_PRIM = ++p;
    if ((unsigned int)t > 0x1000) {
        return;
    }
    gte_mtc2(8, t);
    *CUT_T = t;
    dx = a->v.vx - c->v.vx;
    dy = a->v.vy - c->v.vy;
    dz = a->v.vz - c->v.vz;
    gte_mtc2(9, dx);
    gte_mtc2(10, dy);
    gte_mtc2(11, dz);
    gte_op(0x4B98003D);
    gte_stir3(dx, dy, dz);
    dx += c->v.vx;
    dy += c->v.vy;
    dz += c->v.vz;
    gte_mtc2(4, (dx & 0xFFFF) | dy << 16);
    gte_mtc2(5, dz);
    gte_ldv1(&c->v);
    gte_rtpt();
    rgb = a->rgb;
    gte_ctc2(21, (rgb & 0xFF) << 4);
    gte_ctc2(22, rgb >> 4 & 0xFF0);
    gte_ctc2(23, rgb >> 12 & 0xFF0);
    gte_stsxy3r(p->xy0, p->xy1, p->xy2);
    rgb = c->rgb;
    gte_mtc2(6, rgb);
    p->rgb1 = rgb;
    gte_mtc2(8, *CUT_T);
    gte_op(0x4A780010);
    p->rgbc0 = *CUT_CODE | *CUT_RGB << 8 >> 8;
    p->rgb2 = gte_mfc2(22);
    ot = &(*CUT_OT)[*CUT_OTZ];
    p->tag = *ot << 8 >> 8 | 0x06000000;
    *ot = (unsigned int)p << 8 >> 8;
    *CUT_PRIM = p + 1;
}
#else
INCLUDE_ASM("asm/overlay/submar/nonmatchings/submar", sub_800ACF6C);
#endif

