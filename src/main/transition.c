#include "common.h"
#include "main.h"

/* A screen transition (0x80013C9C-0x800144D8): the screen breaks into a grid of quads that turn, shrink and fade.
 * Built like loader.c, GCC 2.7.2 and ASPSX 2.34 (an indexed load folds into `%lo`), but with -G0: its globals are
 * reached through `lui`/`%lo`. Its data lives in the overlay area at 0x8019xxxx; pointers to it are passed as integer
 * constants (they load with `lui`/`ori`), scalars there are reached by symbol. */

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

typedef struct SVECTOR {
    short vx, vy, vz, pad;
} SVECTOR;

typedef struct DVECTOR {
    short vx, vy;
} DVECTOR;

typedef struct CVECTOR {
    unsigned char r, g, b, cd;
} CVECTOR;

typedef struct MATRIX {
    short m[3][3];
    int t[3];
} MATRIX;

/* One frame's quads: the screen cut into 10x7 tiles of 32x32, and the ordering-table entry they're linked to. */
typedef struct Shards {
    unsigned int ot;
    POLY_FT4 poly[10][7];
} Shards;

/* libgpu's setRGB0, setUV4 and setXY4: written through a fresh pointer each time, as the game does. */
#define set_rgb(p, r, g, b) ((p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b))
#define set_uv4(p, _u0, _v0, _u1, _v1, _u2, _v2, _u3, _v3)                                                          \
    ((p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1), (p)->u2 = (_u2), (p)->v2 = (_v2),           \
     (p)->u3 = (_u3), (p)->v3 = (_v3))
#define set_xy4(p, _x0, _y0, _x1, _y1, _x2, _y2, _x3, _y3)                                                          \
    ((p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1), (p)->x2 = (_x2), (p)->y2 = (_y2),           \
     (p)->x3 = (_x3), (p)->y3 = (_y3))

#define shards ((Shards *)0x8019C000)                    /* two frames' worth, drawn alternately */
#define shard_verts ((SVECTOR (*)[8])0x8019D5EC)         /* the 11x8 grid corners */
#define shard_screen ((DVECTOR (*)[8])0x8019D8AC)        /* and where they land on screen */
#define shard_rot ((SVECTOR *)0x8019DA0C)
#define shard_scale ((int *)0x8019DA14)
#define shard_disp (*(DISPENV *)0x8019DA80)
#define shard_draw (*(DRAWENV *)0x8019DA24)

extern short D_8019DA0C, D_8019DA0E, D_8019DA10; /* shard_rot */
extern int D_8019DA14, D_8019DA18, D_8019DA1C;   /* shard_scale */
extern int D_8019DA94;                           /* which frame's quads */
extern int D_8019DA98;                           /* the scale's speed */
extern int D_8019DA9C;                           /* frames so far */
extern int D_8019DAA0;
extern int D_8019DAA4;                           /* brightness while fading out */
extern unsigned int *D_8019D5E8;
extern DVECTOR D_8019D8AC[11][8];                /* shard_screen, reached by symbol in func_80013C9C's last loop */
extern volatile short frame;
extern volatile short D_80095DD4;
extern RECT D_80062D44;
extern MATRIX D_80063028;

int VSync(int mode);
void func_800448D0(DISPENV *env);
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* probably SetDefDrawEnv */
void func_800444AC(DISPENV *env);                             /* probably PutDispEnv */
void PutDrawEnv(DRAWENV *env);
void func_8004433C(unsigned int *ot);                         /* probably DrawOTag */
void func_8004418C(unsigned int *ot, int n);
void MoveImage(RECT *rect, int x, int y);
void func_80044000(RECT *rect, unsigned int *data);
void func_80044064(RECT *rect, unsigned int *data);
unsigned short GetTPage(int tp, int abr, int x, int y);
void SetPolyFT4(POLY_FT4 *p);
void func_80046848(void *p, int on); /* probably SetSemiTrans */
void func_80046870(void *p, int on); /* probably SetShadeTex */
void AddPrim(void *ot, void *p);
void SetGeomOffset(int x, int y);
void SetGeomScreen(int h);
void RotMatrix(SVECTOR *r, MATRIX *m);
void ScaleMatrix(MATRIX *m, int *v);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
int RotTransPers(SVECTOR *v, DVECTOR *sxy, int *p, int *flag);
void copy_words(int *dst, int *src, int bytes);
void func_80013C9C(void);

#ifdef NON_MATCHING
/* Draws a frame of the transition. Same instructions as the game, but in the last loop the game adds the grid's address
 * to the index the other way round (`addu at, v0, at`): it reached the grid through an integer constant there, and as
 * one here GCC strength-reduces the loop, which the game doesn't. */
void func_80013C9C(void)
{
    Shards *s;
    CVECTOR c;
    int i;
    int j;
    int depth;
    int flag;
    int step;

    MoveImage(&D_80062D44, 0, 8);
    s = &shards[++D_8019DA94 & 1];
    D_8019D5E8 = (unsigned int *)s;
    if (D_8019DA9C++ >= 0x2F) {
        if (D_8019DAA4)
            D_8019DAA4--;
        c.cd = s->poly[0][0].code;
        c.r = D_8019DAA4;
        c.g = D_8019DAA4;
        c.b = D_8019DAA4;
        for (j = 0; j < 7; j++)
            for (i = 0; i < 10; i++)
                *(CVECTOR *)&s->poly[i][j].r0 = c;
        if (D_8019DA9C >= 0x4F)
            D_80095DD4 = 0;
    } else {
        for (j = 0; j < 7; j++)
            for (i = 0; i < 10; i++)
                s->poly[i][j].tpage = GetTPage(2, D_8019DA9C % 5 == 0 ? 3 : 0, (i * 32) & ~0x3F, 0);
    }
    RotMatrix(shard_rot, &D_80063028);
    ScaleMatrix(&D_80063028, shard_scale);
    SetRotMatrix(&D_80063028);
    SetTransMatrix(&D_80063028);
    D_8019DA10 -= 3;
    step = D_8019DA98;
    D_8019DA98 = step + 10;
    D_8019DA14 += step >> 4;
    D_8019DA18 += step >> 4;
    for (j = 0; j < 8; j++)
        for (i = 0; i < 11; i++)
            RotTransPers(&shard_verts[i][j], &shard_screen[i][j], &depth, &flag);
    func_8004418C(&s->ot, 1);
    for (j = 0; j < 7; j++) {
        for (i = 0; i < 10; i++) {
            set_xy4(&s->poly[i][j], D_8019D8AC[i][j].vx, D_8019D8AC[i][j].vy, D_8019D8AC[i + 1][j].vx,
                    D_8019D8AC[i + 1][j].vy, D_8019D8AC[i][j + 1].vx, D_8019D8AC[i][j + 1].vy,
                    D_8019D8AC[i + 1][j + 1].vx, D_8019D8AC[i + 1][j + 1].vy);
            AddPrim(&s->ot, &s->poly[i][j]);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/main/transition", func_80013C9C);
#endif

void func_800140A4(void)
{
    if (!(++D_8019DAA0 & 1)) {
        func_8004433C(D_8019D5E8);
        func_80013C9C();
    }
}

/* Starts the transition: its own display, the screen copied in four strips with every pixel's semi-transparency bit
 * set, then the quad grid set up and drawn once. */
void func_800140F4(void)
{
    RECT rect;
    RECT loaded;
    unsigned int *buf;
    unsigned int *prev;
    int i;
    int j;
    int top;
    Shards *s;

    D_80095DD4 = 0;
    VSync(0);
    func_800448D0(&shard_disp);
    top = shard_disp.screen.y > 16 ? 24 : 0;
    SetDefDispEnv(&shard_disp, 0, 232, 320, 240);
    func_80043814(&shard_draw, 0, 240, 320, 224);
    shard_disp.screen.y = top;
    shard_disp.isrgb24 = 0;
    shard_draw.dtd = 0;
    shard_draw.dfe = 1;
    shard_draw.isbg = 0;
    shard_draw.tpage = 0;
    func_800444AC(&shard_disp);
    PutDrawEnv(&shard_draw);
    SetGeomOffset(0x9F, 0x77);
    SetGeomScreen(0x1E0);
    D_8019DAA0 = 0;
    if (!frame) {
        rect.x = 0;
        rect.y = 8;
        rect.w = 320;
        rect.h = 224;
        MoveImage(&rect, 0, 240);
    }
    /* Each strip is read while the previous one, already in memory, gets its bits set and goes back. */
    for (i = 0; i < 4; i++) {
        loaded = rect;
        prev = buf;
        buf = (unsigned int *)0x801B8000;
        rect.x = 0;
        rect.y = i * 74 + 240;
        rect.w = 320;
        rect.h = 74;
        if (i & 1)
            buf = (unsigned int *)0x801B0000;
        DrawSync(0);
        if (i > 0) {
            for (j = 0; j < 320 * 74 / 2; j++)
                prev[j] |= 0x80008000;
            func_80044000(&loaded, prev);
        }
        if (i < 3)
            func_80044064(&rect, buf);
    }
    D_8019DA14 = 0x820;
    D_8019DA18 = 0x820;
    D_8019DA1C = 0x1000;
    D_8019DA98 = 4;
    D_8019DA0C = 0;
    D_8019DA0E = 0;
    D_8019DA10 = 0;
    D_8019DA9C = 0;
    D_8019DAA4 = 0x80;
    for (j = 0; j < 8; j++) {
        for (i = 0; i < 11; i++) {
            shard_verts[i][j].vx = i * 32 - 0xA0;
            shard_verts[i][j].vy = j * 32 - 0x70;
            shard_verts[i][j].vz = 0;
        }
    }
    s = shards;
    for (j = 0; j < 7; j++) {
        for (i = 0; i < 10; i++) {
            SetPolyFT4(&s->poly[i][j]);
            func_80046848(&s->poly[i][j], 1);
            func_80046870(&s->poly[i][j], 0);
            set_rgb(&s->poly[i][j], 0x80, 0x80, 0x80);
            set_uv4(&s->poly[i][j], (i * 32) & 0x3F, j * 32 + 8, ((i * 32) & 0x3F) + 0x1F, j * 32 + 8,
                    (i * 32) & 0x3F, j * 32 + 0x27, ((i * 32) & 0x3F) + 0x1F, j * 32 + 0x27);
        }
    }
    copy_words((int *)&shards[1], (int *)&shards[0], sizeof(Shards));
    func_80013C9C();
    D_80095DD4 = 3;
}
