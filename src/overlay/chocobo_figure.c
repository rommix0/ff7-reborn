/* MINI/CHOCOBO.BIN's figure code, 0x800AD91C-0x800B1250: HIGHWAY.BIN's (highway.c has the same functions),
 * built with its older toolchain. */

#include "chocobo.h"

int func_80043CC0(void); /* GetGraphType */

#define getTPage(tp, abr, x, y)                                                                                        \
    (func_80043CC0() == 1 || func_80043CC0() == 2                                                                      \
         ? (((tp) & 3) << 9) | (((abr) & 3) << 7) | (((y) & 0x300) >> 3) | (((x) & 0x3FF) >> 6)                        \
         : (((tp) & 3) << 7) | (((abr) & 3) << 5) | (((y) & 0x100) >> 4) | (((x) & 0x3FF) >> 6) | (((y) & 0x200) << 2))
#define getClut(x, y) ((y) << 6 | ((x) >> 4 & 0x3F))

/* libgte's inline macros (chocobo.h has the shared ones): the ones that work straight from the pointer's register (_r,
 * as SUBMAR.BIN's), and single register moves and commands. */
#ifdef NATIVE
#define gte_SetRotMatrix_r(r) gte_SetRotMatrix(r)
#define gte_SetTransMatrix_r(r) gte_SetTransMatrix(r)
#define gte_stlvnl_r(v) gte_stlvnl(v)
#define gte_mtc2(reg, v) gte_write(reg, (v))
#define gte_mfc2(reg) ((int)gte_read(reg))
#define gte_mfc2_nop(reg) ((int)gte_read(reg))
#define gte_lwc2_m(reg, x) gte_write(reg, (x))
#define gte_op(cmd) gte_cmd((cmd) & 0x1FFFFFF)
#define gte_op_now(cmd) gte_cmd((cmd) & 0x1FFFFFF)
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

extern unsigned int D_800B67B8[]; /* by angle 0-255: sine in the low half, cosine in the high (0x1000 is 1) */

char *chocobo_setup_part_prims(Part *p, char *prims, int relocate, int variant);
void chocobo_scale_figure(Figure *m, short scale, int force);
void chocobo_project_part(Part *p);
void chocobo_scale_part(Part *p, short scale, int force);
void chocobo_scale_anim(Anim *q, short scale, int force);
void chocobo_tint_part(Part *p, short x, short y, short z);

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

#define SEMI_SHADE(T)                                                                                                  \
    for (j = 0; j < n; prim += sizeof(T), j++) {                                                                       \
        if (on)                                                                                                        \
            ((T *)prim)->code |= 2;                                                                                    \
        else                                                                                                           \
            ((T *)prim)->code &= ~2;                                                                                   \
        if (on)                                                                                                        \
            ((T *)prim)->code |= 1;                                                                                    \
        else                                                                                                           \
            ((T *)prim)->code &= ~1;                                                                                   \
    }

/* end of declarations */

char *chocobo_setup_figure_prims(Figure *m, char *work, int a2)
{
    unsigned int i;
    Part *parts;

    m->work = work;
    work += m->nbones * 32;
    parts = (Part *)(m->base + m->parts);
    for (i = 0; i < m->nparts; i++)
        work = chocobo_setup_part_prims(&parts[i], work, 0, a2);
    chocobo_scale_figure(m, m->scale, 0);
    return work;
}

/* Sets up a part's primitives in both buffers from its polygons: colours, texture coordinates, CLUT and texture page
 * (moved to texture variant `variant`), and the kinds; the vertices are filled each frame. */
char *chocobo_setup_part_prims(Part *p, char *prims, int relocate, int variant)
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
void chocobo_draw_figure(Figure *m)
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
        chocobo_project_part(part);
    }
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_figure", chocobo_draw_figure);
#endif

/* Projects a part's vertices to the scratchpad, then for each of its polygons in this frame's buffer: puts in the
 * screen positions and, if it faces the camera, links it in the ordering table by depth (quads by their four
 * depths' sum / 16, triangles by avsz3); a polygon turned away is unlinked. */
#ifdef NON_MATCHING
/* Register allocation (the game keeps 0xFF000000/0xFFFFFF in s-registers), where the loop
 * increments and the next polygon word's load sit, and 0x48 bytes of unused frame. */
void chocobo_project_part(Part *p)
{
    ProjVert *pv = (ProjVert *)0x1F800008;
    ProjVert *out = (ProjVert *)0x1F800008;
    unsigned int *ot = D_800B66D0;
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
    if (D_800B66CC[0])
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
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_figure", chocobo_project_part);
#endif

/* Poses a figure at frame `frame` of its animation `anim`, under `root`: each bone's matrix, from its
 * parent's, its length and its angles and offsets (fixed, or that frame's), into the scratchpad and, for the bones
 * that are drawn, the figure's work buffer. Angles index the sine table, so 256 make a turn. */
#ifdef NON_MATCHING
/* Hand-written in GCC's calling style: `sub` (GCC emits subu/negu), GTE hazard slots filled with
 * work, 0x1F800022 built with ori/sll/ori and a stray nop. The C is for the PC port. */
void chocobo_pose_figure(Figure *m, MATRIX *root, int anim, int frame)
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
        sa = D_800B67B8[word >> 8 & 0xFF];
        sb = D_800B67B8[word >> 16 & 0xFF];
        sc = D_800B67B8[word >> 24];
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
        sa = D_800B67B8[a];
        sb = D_800B67B8[b];
        sc = D_800B67B8[c];
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
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_figure", chocobo_pose_figure);
#endif

/* Scales a whole figure by scale (0x1000 is 1): its parts' vertices, its bones' lengths and its animations. */
void chocobo_scale_figure(Figure *m, short scale, int force)
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
        chocobo_scale_part(&parts[i], scale, force);
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
        chocobo_scale_anim(&pieces[i], scale, force);
}

/* Scales a part's vertices by scale (0x1000 is 1), once: the flag word before them says it is done, unless forced. */
void chocobo_scale_part(Part *p, short scale, int force)
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
void chocobo_scale_anim(Anim *q, short scale, int force)
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
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_figure", chocobo_scale_anim);
#endif

int chocobo_tint_figure(Figure *m, unsigned char *b)
{
    SVECTOR unused; /* never used, but the game keeps its stack room */
    unsigned int i;
    unsigned int n = m->nparts;
    Part *parts = (Part *)(m->base + m->parts);
    short x = b[1] << 8 | b[0], y = b[3] << 8 | b[2], z = b[5] << 8 | b[4];

    *(int *)0x1F800200 = b[6];
    for (i = 0; i < n; i++)
        chocobo_tint_part(&parts[i], x, y, z);
    return 1;
}

/* Tints a part's primitives in the buffer being drawn: each corner's colour from its polygon plus (x, y, z) (an RGB
 * offset), keeping the primitives' kinds. With bit 0 of the scratchpad flag clear, only linked primitives. */
void chocobo_tint_part(Part *p, short x, short y, short z)
{
    char *out = p->prims;
    PolyRGB *src = (PolyRGB *)(p->base + p->polys);
    int r, g, b;
    unsigned int i, n, counts;
    unsigned char code;

    if (D_800B66CC[0])
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

/* Turns semi-transparency and plain texturing (no shading) on or off (*b) for every primitive of a figure, in both
 * buffers. */
int chocobo_set_figure_semitrans(Figure *m, unsigned char *b)
{
    Part *parts = (Part *)(m->parts + m->base);
    Part *part;
    int on = *b;
    unsigned int i, buf, j, n;
    char *prim;

    for (i = 0; i < m->nparts; i++) {
        part = &parts[i];
        for (buf = 0; buf < 2; buf++) {
            prim = part->prims;
            if (buf)
                prim += part->size;
            n = part->n_gt4;
            SEMI_SHADE(POLY_GT4)
            n = part->n_gt3;
            SEMI_SHADE(POLY_GT3)
            n = part->n_ft4;
            SEMI_SHADE(POLY_FT4)
            n = part->n_ft3;
            SEMI_SHADE(POLY_FT3)
            n = part->n_f3;
            SEMI_SHADE(POLY_F3)
            n = part->n_f4;
            SEMI_SHADE(POLY_F4)
            n = part->n_g3;
            SEMI_SHADE(POLY_G3)
            n = part->n_g4;
            SEMI_SHADE(POLY_G4)
        }
    }
    return 1;
}
