#ifndef PRIMS_H
#define PRIMS_H

/* The game's drawing primitives and the menus' pointer into the primitive buffer. */

/* libgpu's primitives. */
typedef struct POLY_FT4 {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    unsigned char u0, v0;
    unsigned short clut;
    short x1, y1;
    unsigned char u1, v1;
    unsigned short tpage;
    short x2, y2;
    unsigned char u2, v2;
    unsigned short pad1;
    short x3, y3;
    unsigned char u3, v3;
    unsigned short pad2;
} POLY_FT4;

typedef struct TILE_1 {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
} TILE_1;

typedef struct LINE_F2 {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
} LINE_F2;

typedef struct TILE {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    short w, h;
} TILE;

typedef struct SPRT {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    unsigned char u0, v0;
    unsigned short clut;
    short w, h;
} SPRT;

typedef struct POLY_G4 {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    unsigned char r1, g1, b1, pad1;
    short x1, y1;
    unsigned char r2, g2, b2, pad2;
    short x2, y2;
    unsigned char r3, g3, b3, pad3;
    short x3, y3;
} POLY_G4;

typedef struct DR_ENV {
    unsigned int tag;
    unsigned int code[15];
} DR_ENV;

/* Where the next primitive goes, as whichever kind is being built. A union, not a plain pointer: the game reloads it
 * after every store to a primitive, which GCC does only for an aggregate. */
typedef union {
    POLY_FT4 *ft4;
    TILE_1 *tile1;
    LINE_F2 *line;
    TILE *tile;
    SPRT *sprt;
    POLY_G4 *g4;
    DR_ENV *dr_env;
} PrimPtr;

extern PrimPtr prim_next;
extern void *prim_ot; /* the ordering table being drawn into */

void SetPolyFT4(void *p);
void SetTile1(void *p);
void SetLineF2(void *p);
void func_80046848(void *p, int on); /* SetSemiTrans */
void func_80046870(void *p, int on); /* SetShadeTex */
unsigned short GetClut(int x, int y);
unsigned short GetTPage(int tp, int abr, int x, int y);
void AddPrim(void *ot, void *p);


/* libgpu's macros. */
#define setlen(p, len) (((unsigned char *)(p))[3] = (len))
#define setcode(p, c) ((p)->code = (c))
#define setRECT(r, _x, _y, _w, _h) ((r)->x = (_x), (r)->y = (_y), (r)->w = (_w), (r)->h = (_h))

#endif
