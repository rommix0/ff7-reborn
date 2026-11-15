/* Small drawing helpers the menus share, 0x8001CDA4-0x8001D6A8: each builds one primitive at prim_next, adds it to
 * prim_ot and steps prim_next past it. GCC 2.7.2, ASPSX 2.21 and -G8, like pad.c before it. */

#include "common.h"

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

/* Where the next primitive goes, as whichever kind is being built. A union, not a plain pointer: the game reloads it
 * after every store to a primitive, which GCC does only for an aggregate. */
typedef union {
    POLY_FT4 *ft4;
    TILE_1 *tile;
    LINE_F2 *line;
} PrimPtr;

extern PrimPtr prim_next;
extern void *prim_ot; /* the ordering table the menus draw into */

void SetPolyFT4(void *p);
void SetTile1(void *p);
void SetLineF2(void *p);
void func_80046848(void *p, int on); /* SetSemiTrans */
void func_80046870(void *p, int on); /* SetShadeTex */
unsigned short GetClut(int x, int y);
unsigned short GetTPage(int tp, int abr, int x, int y);
void AddPrim(void *ot, void *p);

/* A 64x64 quad of the 128x128 texture at VRAM (832, 0); nothing in the game calls it. */
void func_8001CDA4(void)
{
    SetPolyFT4(prim_next.ft4);
    func_80046870(prim_next.ft4, 1);
    prim_next.ft4->x0 = 0;
    prim_next.ft4->y0 = 5;
    prim_next.ft4->x1 = 64;
    prim_next.ft4->y1 = 5;
    prim_next.ft4->x2 = 0;
    prim_next.ft4->y2 = 69;
    prim_next.ft4->x3 = 64;
    prim_next.ft4->y3 = 69;
    prim_next.ft4->u0 = 0;
    prim_next.ft4->v0 = 0;
    prim_next.ft4->u1 = 128;
    prim_next.ft4->v1 = 0;
    prim_next.ft4->u2 = 0;
    prim_next.ft4->v2 = 128;
    prim_next.ft4->u3 = 128;
    prim_next.ft4->v3 = 128;
    prim_next.ft4->clut = GetClut(0, 510);
    prim_next.ft4->tpage = GetTPage(1, 0, 832, 0);
    AddPrim(prim_ot, prim_next.ft4);
    prim_next.ft4++;
}

/* A w x h sprite from WINDOW.BIN's texture at VRAM (960, 256), palette row 480 + pal. */
void func_8001CF3C(short x, short y, short w, short h, unsigned short u, unsigned short v, unsigned short tw,
                   unsigned short th, short pal, short semi)
{
    SetPolyFT4(prim_next.ft4);
    func_80046870(prim_next.ft4, 1);
    if (semi)
        func_80046848(prim_next.ft4, 1);
    prim_next.ft4->x0 = x;
    prim_next.ft4->y0 = y;
    prim_next.ft4->x1 = x + w;
    prim_next.ft4->y1 = y;
    prim_next.ft4->x2 = x;
    prim_next.ft4->y2 = y + h;
    prim_next.ft4->x3 = x + w;
    prim_next.ft4->y3 = y + h;
    prim_next.ft4->u0 = u;
    prim_next.ft4->v0 = v;
    prim_next.ft4->u1 = u + tw;
    prim_next.ft4->v1 = v;
    prim_next.ft4->u2 = u;
    prim_next.ft4->v2 = v + th;
    prim_next.ft4->u3 = u + tw;
    prim_next.ft4->v3 = v + th;
    prim_next.ft4->clut = GetClut(256, pal + 480);
    prim_next.ft4->tpage = GetTPage(1, 0, 960, 256);
    AddPrim(prim_ot, prim_next.ft4);
    prim_next.ft4++;
}

/* The same from the texture at VRAM (832, 256), palette (384, pal). */
void func_8001D180(short x, short y, short w, short h, unsigned short u, unsigned short v, unsigned short tw,
                   unsigned short th, short pal, short semi)
{
    SetPolyFT4(prim_next.ft4);
    func_80046870(prim_next.ft4, 1);
    if (semi)
        func_80046848(prim_next.ft4, 1);
    prim_next.ft4->x0 = x;
    prim_next.ft4->y0 = y;
    prim_next.ft4->x1 = x + w;
    prim_next.ft4->y1 = y;
    prim_next.ft4->x2 = x;
    prim_next.ft4->y2 = y + h;
    prim_next.ft4->x3 = x + w;
    prim_next.ft4->y3 = y + h;
    prim_next.ft4->u0 = u;
    prim_next.ft4->v0 = v;
    prim_next.ft4->u1 = u + tw;
    prim_next.ft4->v1 = v;
    prim_next.ft4->u2 = u;
    prim_next.ft4->v2 = v + th;
    prim_next.ft4->u3 = u + tw;
    prim_next.ft4->v3 = v + th;
    prim_next.ft4->clut = GetClut(384, pal);
    prim_next.ft4->tpage = GetTPage(1, 0, 832, 256);
    AddPrim(prim_ot, prim_next.ft4);
    prim_next.ft4++;
}

/* A yellow dot; unused. */
void func_8001D3C0(short x, short y)
{
    SetTile1(prim_next.tile);
    prim_next.tile->x0 = x;
    prim_next.tile->y0 = y;
    prim_next.tile->r0 = 255;
    prim_next.tile->g0 = 255;
    prim_next.tile->b0 = 0;
    AddPrim(prim_ot, prim_next.tile);
    prim_next.tile++;
}

/* A level line from x0 to x1 at y in colour 0xRRGGBB. */
void func_8001D47C(short x0, short x1, short y, int rgb)
{
    SetLineF2(prim_next.line);
    prim_next.line->r0 = rgb >> 16;
    prim_next.line->g0 = rgb >> 8;
    prim_next.line->b0 = rgb;
    prim_next.line->x0 = x0;
    prim_next.line->y0 = y;
    prim_next.line->x1 = x1;
    prim_next.line->y1 = y;
    AddPrim(prim_ot, prim_next.line);
    prim_next.line++;
}

/* A line, yellow when lit and grey when not; unused. */
void func_8001D56C(short x0, short y0, short x1, short y1, short lit)
{
    if (lit) {
        SetLineF2(prim_next.line);
        prim_next.line->r0 = 255;
        prim_next.line->g0 = 255;
        prim_next.line->b0 = 0;
    } else {
        SetLineF2(prim_next.line);
        prim_next.line->r0 = 128;
        prim_next.line->g0 = 128;
        prim_next.line->b0 = 128;
    }
    prim_next.line->x0 = x0;
    prim_next.line->y0 = y0;
    prim_next.line->x1 = x1;
    prim_next.line->y1 = y1;
    AddPrim(prim_ot, prim_next.line);
    prim_next.line++;
}
