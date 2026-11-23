/* MINI/CHOCOBO.BIN's race, 0x800A2984-0x800AD91C: the course, the racers, the camera, the HUD, the betting board and
 * the results. */

#include "chocobo.h"

int abs(int x);
int SquareRoot0(int a);
void OuterProduct12(VECTOR *v0, VECTOR *v1, VECTOR *v2);
MATRIX *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
MATRIX *MulMatrix2(MATRIX *m0, MATRIX *m1);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
VECTOR *RotTrans(SVECTOR *v0, VECTOR *v1, long *flag);
void func_80044AC0(DR_ENV *dr, DRAWENV *env);                     /* SetDrawEnv */
void func_80026A34(int dfe, int dtd, int tpage, RECT *tw);       /* the menu text's drawing mode */
void func_80026F44(int x, int y, unsigned char *text, int color); /* the menu's text */
void func_80028E00(int x, int y, int n, int digits, int color);  /* the menu's numbers */

/* libgte's inline macros only the race uses. */
#ifdef NATIVE
#define gte_nclip() gte_cmd(0x1400006)
#define gte_cfc2_nop(reg) ((int)gte_read_ctrl(reg))
#define gte_stsxy(p) (*(unsigned int *)(p) = gte_read(14))
#define gte_stopz(p) (*(int *)(p) = gte_read(24))
#define gte_stszotz(p) (*(int *)(p) = (int)gte_read(19) >> 2)
#define gte_stsz3c(p) (((int *)(p))[0] = gte_read(17), ((int *)(p))[1] = gte_read(18), ((int *)(p))[2] = gte_read(19))
#define gte_stsz4c(p)                                                                                                  \
    (((int *)(p))[0] = gte_read(16), ((int *)(p))[1] = gte_read(17), ((int *)(p))[2] = gte_read(18),                   \
     ((int *)(p))[3] = gte_read(19))
#define gte_stsxy3_g3(p)                                                                                               \
    (((unsigned int *)(p))[2] = gte_read(12), ((unsigned int *)(p))[4] = gte_read(13),                                 \
     ((unsigned int *)(p))[6] = gte_read(14))
#else
#define gte_nclip() __asm__ volatile("nop\nnop\n.word 0x4B400006")
/* With the load delay filled. */
#define gte_cfc2_nop(reg)                                                                                              \
    ({                                                                                                                 \
        int _v;                                                                                                        \
        __asm__ volatile("cfc2 %0, $" #reg "\nnop" : "=r"(_v));                                                        \
        _v;                                                                                                            \
    })
#define gte_stsxy(p) __asm__ volatile("swc2 $14, 0(%0)" : : "r"(p) : "memory")
#define gte_stopz(p) __asm__ volatile("swc2 $24, 0(%0)" : : "r"(p) : "memory")
/* Unlike highway.c's, straight from the pointer's register. */
#define gte_stszotz(p)                                                                                                 \
    __asm__ volatile("mfc2 $12, $19\nnop\nsra $12, $12, 2\nsw $12, 0(%0)" : : "r"(p) : "$12", "memory")
#define gte_stsz3c(p) __asm__ volatile("swc2 $17, 0(%0)\nswc2 $18, 4(%0)\nswc2 $19, 8(%0)" : : "r"(p) : "memory")
#define gte_stsz4c(p)                                                                                                  \
    __asm__ volatile("swc2 $16, 0(%0)\nswc2 $17, 4(%0)\nswc2 $18, 8(%0)\nswc2 $19, 12(%0)" : : "r"(p) : "memory")
/* The three screen positions into a POLY_G3 (or the first three of a POLY_G4). */
#define gte_stsxy3_g3(p) __asm__ volatile("swc2 $12, 8(%0)\nswc2 $13, 16(%0)\nswc2 $14, 24(%0)" : : "r"(p) : "memory")
#endif

/* libgpu's setlen through the tag's bitfield, as the game writes it (prims.h's byte store folds the offset into the
 * address instead). */
#ifdef NATIVE
#define setlen_tag(p, l) setlen(p, l)
#else
#define setlen_tag(p, l) (((P_TAG *)(p))->len = (l))
#endif

/* What a segment's command list draws (signed: an unsigned switch is built as a linear compare chain). */
enum Kind { KIND_END = -1, KIND_TRI, KIND_QUAD, KIND_SPRITE = 0x21 };

/* A colour built in a register from a course triangle's packed pads (a 4-aligned bitfield, as the game's and/or). */
typedef struct RgbBits {
    unsigned rg : 16;
    unsigned b : 8;
    unsigned pad : 8;
} RgbBits;

/* A screen position; 4-aligned, so GCC keeps it in a register like the game. */
typedef union XY {
    struct {
        short x, y;
    } s;
    long l;
} XY;

/* A rival class's ranges (8 bytes): sprint and run speeds from min up, by rand() % range. */
typedef struct RivalClass {
    short sprint_min, sprint_range;
    short run_min, run_range;
} RivalClass;

/* A follow camera's offsets. */
typedef struct CamPreset {
    short dy;
    short look_dy;
    short shift; /* the distance behind the racer as a shift of rsin/rcos */
} CamPreset;

typedef struct Colour {
    unsigned char r, g, b, pad;
} Colour;

/* A racer's place in the standings. */
typedef struct Standing {
    int id;
    int key;
} Standing;

/* One prize in a prize table: what it is, whether only some races may give it, and whether it is the special one
 * (at most one per race). */
typedef struct Prize {
    unsigned char item;
    unsigned char gated;
    unsigned char pad;
    unsigned char special;
} Prize;

/* One of the 15 cards of the prize board, 5 columns of 3. */
typedef struct Card {
    short state;         /* 0 at rest, 1/2 turned to a side, 3-6 turning */
    unsigned char width; /* turning: 0 (edge on) to 16 */
    unsigned char lit;   /* its prize shows */
    unsigned char back;  /* which side is drawn: 0 or 0xFF */
    unsigned char kind;  /* 1 picked (turn to side 0), 2 face down (turn to 0xFF), 0xFF done */
    unsigned char item;
    unsigned char prize; /* which of the three prizes, 0-2 */
} Card;

/* A pair of chocobos placed first and second (lower index first) and the card it wins. */
typedef struct Ticket {
    short first, second;
    short col, row;
} Ticket;

extern char D_80077F64[], D_80078F64[]; /* the menu text's primitive buffers */
extern unsigned char D_8009D488[0x40]; /* the race's game variables */
extern unsigned char D_8009D48C[2], D_8009D492[0x12], D_8009D498; /* the player's chocobo's name's end, its stats */
extern unsigned char D_8009D4A4; /* the prize won */
extern unsigned int save_gil;
extern RECT D_800B14A0; /* the whole screen */
extern RivalClass D_800B12E8[];
extern SVECTOR D_800B1348; /* where the camera wants to be */
extern SVECTOR D_800B1350; /* where it wants to look */
extern int D_800B1358;     /* a new shot */
extern CamPreset D_800B135C[];
extern Colour D_800B1370[6]; /* the racers' colours */
extern Colour D_800B1388[];  /* by the player's chocobo's colour */
extern int D_800B13A0[5];    /* a standings sprite's u / 32 by animation while running */
extern int D_800B13B4[];     /* ... by finishing place */
extern SPRT D_800B13D0, D_800B13E4; /* the course map's frame */
extern POLY_FT4 D_800B13F8, D_800B1420;
extern int D_800B1448[6]; /* the standings' rows' y */
extern unsigned int D_800B1460[4][4]; /* by rank: the price of 0-3 cards */
extern int D_800B14A8, D_800B14AC, D_800B14B0; /* the prize game's racer's turn (debug) */
extern POLY_F4 D_800B14B4; /* the whole screen, subtractive */
extern int D_800B14CC[6];  /* by place: the results' line's y */
extern Ticket D_800B14E4[16];
extern SPRT D_800B155C, D_800B1570;
extern POLY_FT3 D_800B1584; /* only sets the texture page */
extern int D_800B15A4;      /* the prize game's racer's animation frame, 8.8 */
extern POLY_FT4 D_800B15A8[]; /* the prize board's backgrounds, 10 quads per rank */
extern POLY_FT4 D_800B1BE8[]; /* the results' backgrounds, 9 quads per rank */
extern unsigned int D_800B2188[48];   /* the race's six CLUT rows */
extern unsigned int D_800B2228[5][8]; /* the player's chocobo's row by colour, and row 1's replacement */
extern unsigned char D_800B22C8[7];   /* the second player's chocobo's name */
extern Prize D_800B22D0[], D_800B232C[], D_800B2380[], D_800B23C0[]; /* by rank, each after its count */
extern unsigned char D_800B23EC[][16]; /* the prizes' names */
extern unsigned char D_800B256C[43][8]; /* the rivals' names */
extern unsigned short D_800B26CC[0x2001]; /* arccos by x + 0x1000, x from -0x1000 to 0x1000 */
extern int D_800B733C; /* the last racer */
extern SVECTOR D_800B7340; /* the camera's eye, eased toward D_800B1348 */
extern SVECTOR D_800B7348; /* what it looks at, eased toward D_800B1350 */
extern SPRT D_800B7354[6]; /* the standings' 32x32 sprites */
extern SPRT D_800B73CC[6]; /* the standings' 24x24 sprites, one per colour */
extern int D_800B7444, D_800B7448, D_800B744C; /* the prize game's camera: z, y, x */
extern Prize *D_800B7458; /* the rank's prizes */
extern int D_800B745C[3]; /* the three prizes on offer, in table order */
extern int D_800B7468;             /* frames, for the prize cursor's blink */
extern int D_800B746C;             /* the prize cursor is shown */
extern int D_800B7470, D_800B7474; /* the prize cursor's column and row; on the results, the winning ticket's */
extern Card D_800B7480[5][3];
extern int D_800B75A8;    /* the watched racer's ground height, eased */
extern int D_800B75B4[6]; /* each racer's depth on screen */
extern short D_800B7614, D_800B762C;
extern short D_800B7A28;
extern short D_800F5028, D_800F509C; /* the shot and racer chocobo_set_shot parks before the start */
extern unsigned int *D_800F502C; /* the gil */
extern int D_800F5034; /* the next free primitive */

void chocobo_draw_segments(int first, int last);
void chocobo_use_item(int i, int item);
void chocobo_update_racer(short i);
void chocobo_move_racer(short i);
void chocobo_camera_follow(int i);
void chocobo_camera_move(int follow);
void chocobo_set_shot(short shot, short racer);
void chocobo_look_at(MATRIX *m, SVECTOR *eye, SVECTOR *at);
void chocobo_update_shot(void);
void chocobo_apply_camera(void);
void chocobo_trigger_prop(short i);
int chocobo_yaw_from_dir(int x, int z);
int chocobo_yaw_between(SVECTOR *from, SVECTOR *to);
void chocobo_debug_variables(void);
void chocobo_draw_number(unsigned int n, int x, int y);
void chocobo_draw_prize_racer(int n);
void chocobo_deal_prizes(void);
void chocobo_draw_cards(int active);
void chocobo_draw_prizes(void);
void chocobo_draw_text(unsigned char *text, int unused, int x, int y);

/* end of declarations */

/* Draws the ground: up to 249 of the course's triangles that face the camera. */
void chocobo_draw_ground(void)
{
    int i, n;
    GroundTri *t;
    POLY_G3 *p;
    VECTOR unused; /* the game keeps 16 bytes of stack below opz */
    int opz;
    GroundTri *tris;
    DrawBuf *buf;

    n = 0;
    for (i = 0; i < D_800F5084->nground; i++) {
        tris = D_800F5084->ground;
        t = &tris[i];
        gte_ldv3c(t);
        gte_rtpt();
        buf = D_800F5074;
        p = &buf->ground[n];
        *(unsigned int *)&p->r0 = t->c0;
        setlen(p, 6);
        setcode(p, 0x30);
        if (gte_cfc2_nop(31) < 0)
            continue;
        gte_nclip();
        *(unsigned int *)&p->r1 = t->c1;
        *(unsigned int *)&p->r2 = t->c2;
        gte_stopz(&opz);
        if (opz < 0)
            continue;
        if (n >= 249)
            return;
        gte_stsxy3_g3(p);
        addPrim(&D_800F5074->ot_ground[1], p);
        n++;
    }
}

/* Draws the segments in view, the range wrapping round the end of the course. */
void chocobo_draw_course(void)
{
    int n = D_800F5084->nsegments;
    int from = D_800B7598;
    int to = D_800F507C;

    from = (from + n) % n;
    to = (to + n) % n;
    D_800F5034 = 0;
    if (to < from) {
        chocobo_draw_segments(0, to);
        chocobo_draw_segments(from, n);
    } else {
        chocobo_draw_segments(from, to);
    }
}

/* Draws segments first..last from their command lists: shaded triangles and quads (back faces culled where the
 * segment says) and billboards, into the ordering table, up to 2495 primitives a frame. */
#ifdef NON_MATCHING
/* Loop.c moves &D_800F5084 and &D_800F5074 out of the loop (the game reloads them by %hi/%lo each time), which
 * shifts the registers from the s-registers on; lb against lbu for the cull flag. */
void chocobo_draw_segments(int first, int last)
{
    int z[4];
    int opz;
    int seg = first, left = last - first;
    int n = D_800F5034;
    int *cmd = &D_800F5084->cmds[D_800F5084->seg_first[first]];
    int c = *cmd;
    int cull; /* set by the list's first segment marker */
    XY sxy, top;
    int added, maxz, w, x, z1, z2, z3;
    RgbBits c1, c2;
    XY right;
    CourseTri *t;
    CourseQuad *q;
    POLY_G3 *g3;
    POLY_G4 *g4;
    POLY_FT4 *ft4;

    while (c != -1) {
        if (c < 0) {
            if (--left < 0)
                break;
            seg++;
            cull = D_800B7500[seg].ground & 0x80;
        }
        switch ((enum Kind)((unsigned)c >> 16 & 0xFF)) {
        case KIND_TRI:
            t = &D_800F5084->tris[(short)c];
            gte_ldv3c(t);
            gte_rtpt();
            c1.rg = t->v[0].pad;
            c1.b = t->v[1].pad;
            g3 = &D_800F5074->prims[n].g3;
            *(RgbBits *)&g3->r1 = c1;
            added = 0;
            if (gte_cfc2_nop(31) < 0)
                goto next;
            gte_nclip();
            *(unsigned int *)&g3->r0 = t->c0;
            gte_stopz(&opz);
            if (cull && opz <= 0)
                goto next;
            gte_stsz3c(z);
            setlen(g3, 6);
            gte_stsxy3_g3(g3);
            maxz = z[0];
            z1 = z[1];
            z2 = z[2];
            if (maxz < z1)
                maxz = z1;
            if (maxz < z2)
                maxz = z2;
            maxz /= 4;
            /* The game sets the first colour again before the third: dead, but in the code. */
            c1.rg = t->v[0].pad;
            c1.b = t->v[1].pad;
            c1.rg = t->v[2].pad;
            c1.b = t->v[1].pad >> 8;
            *(RgbBits *)&g3->r2 = c1;
            addPrim(&D_800F5074->ot[maxz], g3);
            added = 1;
            break;
        case KIND_QUAD:
            q = &D_800F5084->quads[(short)c];
            gte_ldv3c(q);
            gte_rtpt();
            g4 = &D_800F5074->prims[n].g4;
            *(unsigned int *)&g4->r0 = q->c0;
            setlen(g4, 8);
            added = 0;
            if (gte_cfc2_nop(31) < 0)
                goto next;
            gte_nclip();
            *(unsigned int *)&g4->r3 = q->c3;
            gte_stopz(&opz);
            if (cull && opz < 0)
                goto next;
            gte_stsxy3_g3(g4);
            gte_ldv0_r(&q->v[3]);
            gte_rtps();
            c2.b = q->v[1].pad >> 8;
            c2.rg = q->v[2].pad;
            *(RgbBits *)&g4->r1 = c2;
            if (gte_cfc2_nop(31) < 0)
                goto next;
            gte_stsxy(&g4->x3);
            gte_stsz4c(z);
            maxz = z[0];
            z1 = z[1];
            z2 = z[2];
            z3 = z[3];
            if (maxz < z1)
                maxz = z1;
            if (maxz < z2)
                maxz = z2;
            if (maxz < z3)
                maxz = z3;
            maxz /= 4;
            added = 1;
            c2.rg = q->v[3].pad;
            c2.b = ((unsigned char *)&q->c3)[3];
            *(RgbBits *)&g4->r2 = c2;
            addPrim(&D_800F5074->ot[maxz], g4);
            break;
        case KIND_SPRITE: {
            CourseSprite spr;
            int otz;

            spr = D_800F5084->sprites[(short)c];
            gte_ldv0_r(&spr.pos);
            gte_rtps();
            ft4 = &D_800F5074->prims[n].ft4;
            if (gte_cfc2_nop(31) < 0)
                goto hidden;
            gte_stszotz(&otz);
            if (otz > 4000)
                goto hidden;
            gte_stsxy(&sxy);
            {
                x = sxy.s.x;
                if (x < -20 || sxy.s.y < -20 || x > 350 || sxy.s.y > 270)
                    goto hidden;
                w = spr.size * 270 / otz;
                ft4->x0 = sxy.s.x = x - w / 2;
                right.s.x = sxy.s.x + w;
                ft4->x1 = right.s.x;
                ft4->y0 = sxy.s.y;
                ft4->y1 = sxy.s.y;
                ft4->x2 = sxy.s.x;
                ft4->x3 = right.s.x;
                spr.pos.vy -= spr.rise * 4;
                gte_ldv0_r(&spr.pos);
                gte_rtps();
                *(unsigned short *)&ft4->u0 = spr.uv0;
                *(unsigned short *)&ft4->u1 = spr.uv1;
                gte_stsxy(&top);
                ft4->y2 = ft4->y3 = top.s.y;
                *(unsigned short *)&ft4->u2 = spr.uv2;
                *(unsigned int *)&ft4->r0 = 0x2E808080;
                *(unsigned short *)&ft4->u3 = spr.uv3;
                ft4->clut = spr.pos.pad;
                ft4->tpage = spr.tpage;
                setlen(ft4, 9);
                added = 1;
                addPrim(&D_800F5074->ot[otz], ft4);
            }
            break;
        hidden:
            added = 0;
            break;
        }
        default:
            goto test;
        }
    next:
        n += added;
    test:
        if (n >= 2495)
            break;
        c = *++cmd;
    }
    D_800F5034 = n;
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_draw_segments);
#endif

/* Applies item (1-13) to racer i: speed and stamina boosts and cuts, flags, timers. */
void chocobo_use_item(int i, int item)
{
    Racer *r = &D_800B75CC[i];

    switch (item) {
    case 1:
        r->spurt = D_800F5084->spurt + 20;
        break;
    case 2: {
        int v = r->stamina_max;

        v += v * 4 / 3;
        r->stamina_max = v;
        r->stamina = v;
        break;
    }
    case 3:
        r->spurt = D_800F5084->spurt - 20;
        break;
    case 4:
        r->trait = 2;
        break;
    case 5:
        r->spurt_dash = 0;
        break;
    case 6:
        r->x64 = 100;
        r->line = 0;
        r->ai = 3;
        break;
    case 7:
        r->x9C = -1;
        break;
    case 8:
        r->stamina = r->stamina_max /= 2;
        break;
    case 9:
        r->ground_mask &= 2;
        break;
    case 10:
        r->ground_mask &= 1;
        break;
    case 11:
        r->quick_start = -1;
        if (rand() & 1)
            r->stamina_max /= 2;
        break;
    case 12:
        r->steady = -1;
        break;
    case 13:
        r->line = 3;
        break;
    }
}

/* Sets up the racers: shuffles their lanes, gives each its stats (the player's from the field, the rivals' from
 * their class), places it on the start line, picks the rivals' names, and tints their chocobos. */
#ifdef NON_MATCHING
/* Register allocation in the first loops; one more hoisted address (&D_800B66CC) in the last loop (an s-register
 * more: the frame is 8 bytes bigger); the switches' tails are cross-jumped in ours. */
void chocobo_init_racers(void)
{
    VECTOR v;
    int order[43];
    short i;
    Racer *r;
    int j, k, lane, t, n;
    unsigned char *name;
    Figure *m;

    for (i = 0; i < 6; i++) {
        r = &D_800B75CC[i];
        r->lane = i * 224 / 6 + 16;
    }
    for (i = 0; i < 40; i++) {
        j = rand() % 6;
        t = rand() % 6;
        lane = D_800B75CC[j].lane;
        D_800B75CC[j].lane = D_800B75CC[t].lane;
        D_800B75CC[t].lane = lane;
    }

    for (i = 0; i < 6; i++) {
        r = &D_800B75CC[i];
        if (D_800B7A48 && i == 0) {
            r->sprint = D_8009D492[0] + (D_8009D492[1] << 8);
            r->run = D_8009D492[2] + (D_8009D492[3] << 8);
            r->x5E = D_8009D492[4];
            r->skill = D_8009D492[5];
            r->ai = rand() % 4;
            r->autopilot = 1;
            r->stamina = r->stamina_max = D_8009D492[0x10] + (D_8009D492[0x11] << 8);
            r->trait = D_8009D492[8];
            r->x5C = r->x64 / 10;
            switch (D_8009D492[7]) {
            case 1:
                r->ground_mask = 1;
                break;
            case 2:
                r->ground_mask = 2;
                break;
            case 3:
            case 4:
                r->ground_mask = 0;
                break;
            default:
                r->ground_mask = 3;
                break;
            }
            r->x64 = D_8009D498;
        } else {
            r->sprint = D_800B12E8[D_8009D492[13]].sprint_min + rand() % D_800B12E8[D_8009D492[13]].sprint_range;
            r->run = D_800B12E8[D_8009D492[13]].run_min + rand() % D_800B12E8[D_8009D492[13]].run_range;
            r->x5E = 50;
            r->skill = 100;
            r->ai = rand() % 4;
            r->autopilot = 1;
            r->stamina = r->stamina_max = D_800B12E8[D_8009D492[13]].sprint_min + rand() % 300;
            r->trait = (rand() & 7) == 0 ? 2 : 0;
            r->ground_mask = 3;
            r->x5C = r->x64 / 10;
            r->x64 = (rand() & 1) * 50 + 50;
        }
        if (r->x64 < 40) {
            r->line = 3;
            r->steer = (40 - r->x64) / 10;
        } else if (r->x64 < 60) {
            r->line = 2;
            r->steer = (60 - r->x64) / 5;
        } else if (r->x64 < 80) {
            r->line = 1;
            r->steer = (80 - r->x64) / 5;
        } else {
            r->line = 0;
            r->steer = (110 - r->x64) / 5;
        }
        r->x5C = r->x64 / 8;
        r->rank = i;
        r->speed = r->run / 2;
        r->stuck = 0;
        r->quick_start = 0;
        r->x9E = 0;
        r->x9C = 0;
        r->steady = 0;
        lane = r->lane;
        r->spurt_lane = lane;
        r->pace = r->run;
        r->seg = 10;
        r->next = 11;
        r->spurt_dash = -1;
        r->spurt = D_800F5084->spurt;
        r->pos.vx = (D_800B7500[r->seg].left.vx * r->lane + D_800B7500[r->seg].right.vx * (256 - lane)) / 256;
        r->pos.vy = (D_800B7500[r->seg].left.vy * r->lane + D_800B7500[r->seg].right.vy * (256 - lane)) / 256;
        r->pos.vz = (D_800B7500[r->seg].left.vz * r->lane + D_800B7500[r->seg].right.vz * (256 - lane)) / 256;
        r->goal.vx = r->pos.vx;
        r->goal.vy = r->pos.vy;
        r->goal.vz = r->pos.vz;
        r->ahead.vx = (D_800B7500[r->next].left.vx * r->lane + D_800B7500[r->next].right.vx * (256 - lane)) / 256;
        r->ahead.vy = (D_800B7500[r->next].left.vy * r->lane + D_800B7500[r->next].right.vy * (256 - lane)) / 256;
        r->ahead.vz = (D_800B7500[r->next].left.vz * r->lane + D_800B7500[r->next].right.vz * (256 - lane)) / 256;
        r->rot.vx = 0;
        r->rot.vy = 0;
        r->rot.vz = 0;
        r->aim.vx = 0;
        r->aim.vy = 0;
        r->aim.vz = 0;
        r->rot.vy = chocobo_yaw_between(&D_800B7500[10].left, &D_800B7500[11].left);
        r->anim_speed = r->speed / D_800F5050;
        r->place = 0;
        r->anim_time = 0;
        r->finished = 0;
        r->anim = 0;
        v.vx = r->ahead.vx - r->pos.vx;
        v.vy = r->ahead.vy - r->pos.vy;
        v.vz = r->ahead.vz - r->pos.vz;
        VectorNormal(&v, &v);
        r->rot.vy = r->aim.vy = chocobo_yaw_from_dir((short)v.vx, (short)v.vz);
    }

    if (D_8009D49E) {
        /* The second player's chocobo starts as the first's, in its own place, faster and fitter. */
        SVECTOR pos = D_800B75CC[1].pos;
        SVECTOR to = D_800B75CC[1].ahead;
        SVECTOR spare; /* only widens the stack frame */

        D_800B75CC[1] = D_800B75CC[0];
        D_800B75CC[1].pos = pos;
        D_800B75CC[1].goal = pos;
        D_800B75CC[1].ahead = to;
        D_800B75CC[1].stamina_max += D_800B75CC[1].stamina_max / 4;
        D_800B75CC[1].stamina += D_800B75CC[1].stamina / 4;
        D_800B75CC[1].ai = 3;
        D_800B75CC[1].x5E = 100;
        D_800B75CC[1].x64 = 100;
        D_800B75CC[1].rider = 2;
        D_800B75CC[1].ground_mask = 0;
        D_800B75CC[1].rider = 9;
        D_800B75CC[1].chocobo = 1;
        D_800B75CC[1].run += D_800B75CC[1].run / 10;
        D_800B75CC[1].sprint += D_800B75CC[1].sprint / 10;
    }

    for (k = 42; k >= 0; k--)
        order[k] = k;
    for (k = 0; k < 200; k++) {
        j = rand() % 43;
        t = rand() % 43;
        lane = order[j];
        order[j] = order[t];
        order[t] = lane;
    }

    for (i = 0; i < 6; i++) {
        if (D_8009D49E && i == 1) {
            for (n = 0; n < 7; n++)
                D_800B75CC[i].name[n] = D_800B22C8[n];
            continue;
        }
        name = D_800B256C[order[i]];
        D_800B75CC[i].name[0] = name[0];
        D_800B75CC[i].name[1] = name[1];
        D_800B75CC[i].name[2] = name[2];
        D_800B75CC[i].name[3] = name[3];
        D_800B75CC[i].name[4] = name[4];
        D_800B75CC[i].name[5] = name[5];
        D_800B75CC[i].name[6] = 0xFF;
        if (D_800B7A48 && i != 0) {
            chocobo_use_item(i, 0);
            chocobo_use_item(i, 0);
        }
    }
    if (D_800B7A48) {
        D_800B75CC[0].name[6] = 0xFF;
        *(int *)&D_800B75CC[0].name[0] = *(int *)D_8009D488;
        *(unsigned short *)&D_800B75CC[0].name[4] = *(unsigned short *)D_8009D48C;
    }

    for (i = 0; i < 6; i++) {
        r = &D_800B75CC[i];
        m = &D_800B1254->figures[r->rider];
        if (D_8009D49E && i == 1)
            continue;
        if (D_800B7A48 && i == 0)
            continue;
        switch (r->ai) {
        case 0:
            D_800B1250[0] = 0;
            D_800B1250[1] = 0;
            D_800B1250[2] = 0;
            D_800B1250[3] = 0;
            D_800B1250[4] = 0;
            break;
        case 1:
            D_800B1250[0] = 0x40;
            D_800B1250[1] = 0;
            D_800B1250[2] = 0x40;
            D_800B1250[3] = 0;
            D_800B1250[4] = 0;
            break;
        case 2:
            D_800B1250[0] = 0x40;
            D_800B1250[1] = 0;
            D_800B1250[2] = 0x10;
            D_800B1250[3] = 0;
            D_800B1250[4] = 0;
            break;
        case 3:
            D_800B1250[0] = 0;
            D_800B1250[1] = 0;
            D_800B1250[2] = 0x10;
            D_800B1250[3] = 0;
            D_800B1250[4] = 0x40;
            break;
        default:
            goto tint;
        }
        D_800B1250[5] = 0;
        D_800B1250[6] = 1;
    tint:
        D_800B66CC[0] = 0;
        chocobo_tint_figure(m, D_800B1250);
        D_800B66CC[0] = 1;
        chocobo_tint_figure(m, D_800B1250);
    }
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_init_racers);
#endif

/* Ranks the racers by how far along they are into rank, updates each, then moves the camera. */
void chocobo_update_racers(void)
{
    int key[6], who[6];
    int i, j, t;

    for (i = 0; i < 6; i++) {
        key[i] = D_800B75CC[i].seg;
        who[i] = i;
    }
    for (i = 0; i < 6; i++) {
        for (j = i; j < 6; j++) {
            if (key[i] < key[j]) {
                t = key[i];
                key[i] = key[j];
                key[j] = t;
                t = who[i];
                who[i] = who[j];
                who[j] = t;
            }
        }
    }
    for (i = 0; i < 6; i++)
        D_800B75CC[who[i]].rank = i;
    D_800B733C = who[5];
    for (i = 0; i < 6; i++)
        chocobo_update_racer(i);
    if (D_800B7A48)
        chocobo_camera_move(1);
    else
        chocobo_camera_move(0);
}

/* Updates racer i: moves it (or, once stopped, picks its animation by its place), runs its animation, bumps it
 * against the others (with the bump sound), then projects it and places its two figures when on screen. */
#ifdef NON_MATCHING
/* Only the scheduling of the animation-length test (one block of about 20 instructions). */
void chocobo_update_racer(short i)
{
    Racer *r = &D_800B75CC[i];
    Racer *o;
    int j, dist, push, push2, ang;
    struct {
        short x;
        short y;
    } sxy;
    VECTOR v;
    int flag, z;

    if (!D_800B7A64) {
        r->anim = 3;
        r->anim_speed = 0x100;
        r->speed = 10;
    } else if (r->speed) {
        chocobo_move_racer(i);
        r->anim_speed = r->speed / D_800F5050;
    } else {
        D_800F5048++;
        if (D_8009D49E && r->chocobo == 1) {
            switch (r->place) {
            case 1:
                r->anim = 2;
                break;
            case 2:
                r->anim = 3;
                break;
            default:
                r->anim = 4;
            }
        } else {
            switch (r->place) {
            case 1:
                r->anim = 2;
                break;
            case 6:
                r->anim = 4;
                break;
            default:
                r->anim = 3;
            }
        }
        r->anim_speed = 0x100;
    }
    r->anim_time += r->anim_speed;
    r->x74 += r->anim_speed;
    if (r->anim_time >> 8 >= ((Anim *)(D_800B1254->figures[r->chocobo].base +
                                       D_800B1254->figures[r->chocobo].anims))[r->anim].nframes) {
        if (r->anim_last != r->anim) {
            r->anim_last = r->anim;
            r->anim_time = 0;
        } else {
            r->anim_time &= 0xFF;
        }
    }

    for (j = 0; j < 6; j++) {
        if (j == i)
            continue;
        o = &D_800B75CC[j];
        dist = r->pos.vx - o->pos.vx;
        dist = SquareRoot0(dist * dist + (r->pos.vz - o->pos.vz) * (r->pos.vz - o->pos.vz));
        if (dist < 150) {
            if (o->finished) {
                o->speed = r->speed;
                r->speed = 10;
            }
            if (!o->speed || !r->speed)
                continue;
            v.vx = r->pos.vx - o->pos.vx;
            v.vy = 0;
            v.vz = r->pos.vz - o->pos.vz;
            push = 0x80000 / o->speed;
            VectorNormal(&v, &v);
            push2 = 0x80000 / r->speed;
            ang = (short)chocobo_yaw_from_dir((short)v.vx, (short)v.vz);
            if (!r->chocobo && r->bump < 0) {
                r->bump = 30;
                snd_request.arg[0] = 0x7F - (D_800B75B4[i] >> 5);
                snd_request.id = 0xA3;
                snd_submit_request();
                snd_request.id = 0x2B;
                snd_request.arg[0] = r->lane / 2;
                snd_request.arg[1] = (rand() & 1) + 0x114;
                snd_submit_request();
            }
            if (push) {
                r->pos.vx += rsin(ang) / push;
                r->pos.vz += -rcos(ang) / push;
                r->speed -= r->speed / 8;
                if (r->speed < 100)
                    r->speed = 100;
            }
            if (push2) {
                o->pos.vx -= rsin(ang) / push2;
                o->pos.vz -= -rcos(ang) / push2;
            }
        }
        if (r->trait == 2 && dist < 170)
            rand();
    }

    gte_ldv0_r(&r->pos);
    gte_rtps();
    D_800B7564[i] = 0;
    gte_stflg(&flag);
    if (flag < 0)
        return;
    gte_stsxy(&sxy);
    gte_stszotz(&z);
    if (z >= 2200 || sxy.x <= -20 || sxy.x >= 350 || sxy.y <= 0 || sxy.y >= 300)
        return;
    D_800B1254->figures[r->rider].ry = D_800B1254->figures[r->chocobo].ry = -(r->rot.vy >> 4);
    D_800B1254->figures[r->rider].x = D_800B1254->figures[r->chocobo].x = r->pos.vx;
    D_800B1254->figures[r->rider].y = D_800B1254->figures[r->chocobo].y = r->pos.vy;
    D_800B1254->figures[r->rider].z = D_800B1254->figures[r->chocobo].z = r->pos.vz;
    D_800B7564[i] = -1;
    D_800B75B4[i] = z;
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_update_racer);
#endif

/* Moves racer i along the course. On passing a segment: the AI's (or the pad's) pace and lane, stamina and dashes,
 * the ground; stuck for 16 frames: two segments on; past the finish: its place. Then turns and moves it, and keeps it
 * at the ground's height. */
#ifdef NON_MATCHING
/* Register allocation (r in s2, not s1; one callee-saved register fewer, and the game's frame has 8 more bytes of
 * locals), D_800B7500 kept in a register in the ahead block where the game reloads it, some load order, and srl
 * against sra for the last halving. */
void chocobo_move_racer(short i)
{
    Racer *r = &D_800B75CC[i];
    Segment *s;
    VECTOR a, b, n, d, far;
    int k, end, seg, w, speed;

    if (r->finished) {
        r->speed -= (r->place + 1) * 80 + r->speed / 64;
        if (r->speed < 100)
            r->speed = 0;
        else {
            r->anim = 0;
            r->boost = 0;
            if (r->trait == 2) {
                r->anim_time = 0;
                r->anim = 1;
                r->speed -= 50;
            }
        }
    }
    speed = r->speed;
    s = &D_800B7500[r->next];
    a.vx = s->right.vx - s->left.vx;
    a.vy = 0;
    a.vz = s->right.vz - s->left.vz;
    b.vx = r->pos.vx - s->left.vx;
    b.vy = 0;
    b.vz = r->pos.vz - s->left.vz;
    OuterProduct12(&a, &b, &a);
    if (a.vy >= 0) {
        r->stuck = 0;
        for (k = r->seg - 5, end = r->seg + 20; k < end; k++) {
            seg = (k + D_800B74FC) % D_800B74FC;
            s = &D_800B7500[seg];
            a.vx = s->right.vx - s->left.vx;
            a.vy = 0;
            a.vz = s->right.vz - s->left.vz;
            b.vx = r->pos.vx - s->left.vx;
            b.vy = 0;
            b.vz = r->pos.vz - s->left.vz;
            OuterProduct12(&a, &b, &a);
            if (a.vy < 0)
                break;
        }
        r->seg = r->next;
        r->next = (short)(seg + D_800B74FC) % D_800B74FC;
        if (i || !D_800B7A48 || r->autopilot) {
            if (rand() < 5000) {
                switch (r->ai) {
                case 3:
                    if (r->spurt < r->seg)
                        r->lane = (r->lane * 7 + r->spurt_lane) / 8;
                    /* fall through */
                case 0:
                    for (k = 0; k < 6; k++) {
                        if (!D_800B75CC[k].rank)
                            break;
                    }
                    if (k == i)
                        break;
                    if (D_800B75CC[k].rank - r->rank >= 31)
                        r->accel = 10;
                    else if (r->run * 5 / 4 < r->pace)
                        r->accel = -5;
                    else if (r->pace < r->run)
                        r->accel = 3;
                    else
                        r->accel = 0;
                    break;
                case 1:
                    if (D_800B733C == i) {
                        if (!r->boost)
                            r->boost = rand() & 0x28;
                    } else if (r->run * 5 / 4 < r->pace)
                        r->accel = -5;
                    else if (r->pace < r->run * 9 / 10)
                        r->accel = 3;
                    else
                        r->accel = 0;
                    break;
                case 2:
                    if (r->stamina > r->stamina_max / 3) {
                        r->accel = 20;
                        if (r->seg < r->spurt && r->spurt_dash)
                            r->boost = 10;
                    } else if (r->run * 10 / 9 < r->pace)
                        r->accel = -10;
                    else
                        r->accel = 2;
                    break;
                }
                if (r->spurt < r->seg && r->ai != 2) {
                    if (r->spurt_dash)
                        r->boost = 50;
                    if (r->boost)
                        D_800F5044 = -1;
                    r->lane = (r->lane * 7 + r->spurt_lane) / 8;
                }
            }
            r->lane_avg = (r->lane_avg + r->lane) / 2;
        } else {
            if (r->spurt < r->seg && r->boost)
                D_800F5044 = -1;
            if (r->stamina > 0 && !r->finished) {
                if (D_800B7534 & 0x80)
                    r->accel = 2;
                if (D_800B7534 & 0x40)
                    r->accel = -2;
                if (D_800B7534 & 0x20 && r->stamina > 0)
                    r->boost = 2;
            }
        }
        if ((rand() & 0x3F) < r->skill && !r->boost) {
            r->pace += r->pace * r->accel / 100;
            k = r->run * 5 / 4;
            if (k < r->pace)
                r->pace = k;
            if (r->stamina < 0)
                r->pace = r->run;
            if (r->run < r->pace)
                r->stamina -= (r->pace - r->run) / 128;
            if (r->accel > 0)
                r->accel--;
            else if (r->accel < 0)
                r->accel++;
        }
        if (r->boost) {
            if (--r->boost < 0)
                r->boost = 0;
            r->stamina -= r->stamina_max / 300;
            if (r->stamina <= 0) {
                r->boost = 0;
                r->stamina = 0;
            }
            if (!r->boost)
                r->anim = 0;
            else
                r->anim = 1;
            if (r->quick_start && D_800F5040 < 150)
                r->cruise = 0x157C;
            else
                r->cruise = r->sprint;
        } else if (r->steady)
            r->cruise = r->run;
        else
            r->cruise += (r->pace - r->cruise) * r->x5E / 100;
        if (r->rank >= 2 && r->trait == 1)
            r->cruise = r->cruise * 9 / 10;
        if (!r->autopilot && D_800B7A48) {
            if ((rand() & 0x3F) < r->skill) {
                if (D_800B7534 & 0x8000)
                    r->lane += r->steer + r->skill / 32;
                if (D_800B7534 & 0x2000)
                    r->lane -= r->steer + r->skill / 32;
            }
            /* A stumble when the pad is touched, worse for a clumsy racer; now and then it costs stamina. */
            if ((unsigned short)D_800B7534 && r->skill < (rand() & 0x1F)) {
                k = 25 - r->skill / 2;
                r->lane += ((rand() & 3) - 1) * k;
                if (r->stamina > 0 && !(rand() & 7)) {
                    r->stamina -= r->stamina_max / 500;
                    if (r->trait == 2)
                        r->boost = 10;
                    else
                        r->speed -= r->speed / 4;
                }
            }
        } else {
            /* Steer for its line, then away from racers close by. */
            k = D_800F5084->lines[r->seg].lane[r->line] - r->lane;
            if (k < 0) {
                if (r->steer < -k)
                    k = r->x50 - r->steer;
            } else if (r->steer < k)
                k = r->steer + r->x50;
            for (seg = 0; seg < 6; seg++) {
                if (seg == i)
                    continue;
                end = r->seg - D_800B75CC[seg].seg;
                w = end < 0 ? -end : end;
                if (w >= 20)
                    continue;
                end = r->lane - D_800B75CC[seg].lane;
                w = end < 0 ? -end : end;
                if ((w < 16 && !(rand() & 0x1F)) || r->finished)
                    k += ((rand() & 7) - 3) * 16;
            }
            r->lane += k;
        }
        if (!r->finished) {
            r->speed = r->cruise + D_800F5084->lines[r->seg].speed[r->line] * r->cruise / 1024;
            if (r->stamina < 6) {
                r->speed -= r->speed / 4;
                r->anim = 5;
            } else if (D_800B733C == i)
                r->speed += r->speed / 8;
        }
        if (!r->boost && r->rank < 3) {
            if (r->ai != 2) {
                if (r->seg < r->spurt)
                    r->speed -= r->speed / (r->rank + 7);
            } else if (r->seg < r->spurt)
                r->speed -= r->speed / (r->rank + 8);
        }
        k = r->ground_mask & D_800B7500[r->seg].ground;
        if (k)
            r->speed -= r->speed * k / 4;
        if (r->lane > 216)
            r->lane = 216;
        else if (r->lane < 48)
            r->lane = 48;
        s = &D_800B7500[r->next];
        w = 0x100 - r->lane;
        r->ahead.vx = (s->left.vx * r->lane + s->right.vx * w) / 256;
        r->ahead.vy = (s->left.vy * r->lane + s->right.vy * w) / 256;
        r->ahead.vz = (s->left.vz * r->lane + s->right.vz * w) / 256;
        s = &D_800B7500[(r->next + 1) % D_800B74FC];
        far.vx = (s->left.vx * r->lane + s->right.vx * w) / 256;
        far.vy = (s->left.vy * r->lane + s->right.vy * w) / 256;
        far.vz = (s->left.vz * r->lane + s->right.vz * w) / 256;
        d.vx = far.vx - r->pos.vx;
        d.vy = 0;
        d.vz = far.vz - r->pos.vz;
        VectorNormal(&d, &d);
        r->aim.vy = chocobo_yaw_from_dir((short)d.vx, (short)d.vz);
        if (D_800B7500[r->seg].shot) {
            chocobo_set_shot(D_800B7500[r->seg].shot, i);
            D_800B7500[r->seg - 1].shot = 0;
            D_800B7500[r->seg].shot = 0;
            D_800B7500[r->seg + 1].shot = 0;
        }
        if (D_800B7500[r->seg].right.pad)
            chocobo_trigger_prop(D_800B7500[r->seg].right.pad);
    } else {
        w = 0x100 - r->lane;
        if (++r->stuck >= 16 && D_800B7A64 && !r->finished) {
            r->seg += 2;
            r->next += 2;
            r->pos.vx = (D_800B7500[r->seg].left.vx * r->lane + D_800B7500[r->seg].right.vx * w) / 256;
            r->pos.vy = (D_800B7500[r->seg].left.vy * r->lane + D_800B7500[r->seg].right.vy * w) / 256;
            r->pos.vz = (D_800B7500[r->seg].left.vz * r->lane + D_800B7500[r->seg].right.vz * w) / 256;
            r->ahead.vx = (D_800B7500[r->next].left.vx * r->lane + D_800B7500[r->next].right.vx * w) / 256;
            r->ahead.vy = (D_800B7500[r->next].left.vy * r->lane + D_800B7500[r->next].right.vy * w) / 256;
            r->ahead.vz = (D_800B7500[r->next].left.vz * r->lane + D_800B7500[r->next].right.vz * w) / 256;
            r->stuck = 0;
            r->speed = r->cruise = r->pace = r->run;
            d.vx = r->ahead.vx - r->pos.vx;
            d.vy = 0;
            d.vz = r->ahead.vz - r->pos.vz;
            VectorNormal(&d, &d);
            r->rot.vy = r->aim.vy = chocobo_yaw_from_dir((short)d.vx, (short)d.vz);
        }
    }
    if (r->seg > D_800B74FC - 10 && !r->finished) {
        r->finished = -1;
        r->ahead.vx = r->goal.vx;
        r->ahead.vy = r->goal.vy;
        r->ahead.vz = r->goal.vz;
        k = 1;
        for (seg = 0; seg < 6; seg++) {
            if (D_800B75CC[seg].place >= k)
                k = D_800B75CC[seg].place + 1;
        }
        r->place = k;
        if (k == 1) {
            D_800B7594 = i;
            if (!D_800B7A48) {
                snd_request.id = 0x10;
                snd_request.arg[0] = (long)D_8007BF64;
                snd_submit_request();
            }
        }
        if (k == 2)
            D_800B7478 = -1;
        if (D_800B7A48 && !i) {
            if (D_800B75CC[0].place == 1)
                snd_request.arg[0] = (long)D_8007BF64;
            else
                snd_request.arg[0] = (long)D_8007C764;
            snd_request.id = 0x10;
            snd_submit_request();
        }
    }
    if (r->rot.vy - r->aim.vy > 3000)
        r->rot.vy -= 0x1000;
    if (r->rot.vy - r->aim.vy < -3000)
        r->rot.vy += 0x1000;
    r->rot.vy = (r->rot.vy * 3 + r->aim.vy) / 4;
    if (speed) {
        r->frac.vx += rsin(r->rot.vy) * speed / 64;
        r->frac.vz += -rcos(r->rot.vy) * speed / 64;
        r->pos.vx += r->frac.vx / 4096;
        r->pos.vz += r->frac.vz / 4096;
        r->frac.vx &= 0x80000FFF;
        r->frac.vz &= 0x80000FFF;
    }
    /* The ground's height under it, from the plane of its segment. */
    a.vx = D_800B7500[r->next].right.vx - D_800B7500[r->next].left.vx;
    a.vy = D_800B7500[r->next].right.vy - D_800B7500[r->next].left.vy;
    a.vz = D_800B7500[r->next].right.vz - D_800B7500[r->next].left.vz;
    b.vx = D_800B7500[r->next].left.vx - D_800B7500[r->seg].left.vx;
    b.vy = D_800B7500[r->next].left.vy - D_800B7500[r->seg].left.vy;
    b.vz = D_800B7500[r->next].left.vz - D_800B7500[r->seg].left.vz;
    OuterProduct12(&b, &a, &n);
    if (n.vy)
        k = ((D_800B7500[r->seg].left.vx - r->pos.vx) * n.vx + (D_800B7500[r->seg].left.vz - r->pos.vz) * n.vz +
             n.vy * D_800B7500[r->seg].left.vy) / n.vy;
    else
        k = r->pos.vy;
    r->y = r->pos.vy = (r->y + k) / 2;
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_move_racer);
#endif

/* The camera follows racer i: a copy of it, put on the middle line of its segment and the next. */
void chocobo_camera_follow(int i)
{
    Racer *r;
    int lane;
    VECTOR d;

    if (D_800F5088 == i)
        return;
    D_800F5088 = i;
    r = &D_800B75CC[6];
    *r = D_800B75CC[i];
    r->lane = lane = 0x7F;
    r->pos.vx = (D_800B7500[r->seg].left.vx * r->lane + D_800B7500[r->seg].right.vx * (0x100 - lane)) / 256;
    r->pos.vy = (D_800B7500[r->seg].left.vy * r->lane + D_800B7500[r->seg].right.vy * (0x100 - lane)) / 256;
    r->pos.vz = (D_800B7500[r->seg].left.vz * r->lane + D_800B7500[r->seg].right.vz * (0x100 - lane)) / 256;
    r->ahead.vx = (D_800B7500[r->next].left.vx * r->lane + D_800B7500[r->next].right.vx * (0x100 - lane)) / 256;
    r->ahead.vy = (D_800B7500[r->next].left.vy * r->lane + D_800B7500[r->next].right.vy * (0x100 - lane)) / 256;
    r->ahead.vz = (D_800B7500[r->next].left.vz * r->lane + D_800B7500[r->next].right.vz * (0x100 - lane)) / 256;
    d.vx = r->ahead.vx - r->pos.vx;
    d.vy = 0;
    d.vz = r->ahead.vz - r->pos.vz;
    VectorNormal(&d, &d);
    r->aim.vy = chocobo_yaw_from_dir((short)d.vx, (short)d.vz);
}

/* The camera follows racer i from segment seg on, at the given speed. */
void chocobo_camera_follow_at(int i, int speed, int seg)
{
    Racer *r;
    int lane, next;
    VECTOR d;

    r = &D_800B75CC[6];
    D_800F5088 = i;
    *r = D_800B75CC[i];
    next = (seg + 1 + D_800B74FC) % D_800B74FC;
    r->lane = lane = 0x7F;
    r->seg = seg;
    r->speed = speed;
    r->next = next;
    r->pos.vx = (D_800B7500[seg].left.vx * r->lane + D_800B7500[seg].right.vx * (0x100 - lane)) / 256;
    r->pos.vy = (D_800B7500[seg].left.vy * r->lane + D_800B7500[seg].right.vy * (0x100 - lane)) / 256;
    r->pos.vz = (D_800B7500[seg].left.vz * r->lane + D_800B7500[seg].right.vz * (0x100 - lane)) / 256;
    r->ahead.vx = (D_800B7500[next].left.vx * r->lane + D_800B7500[next].right.vx * (0x100 - lane)) / 256;
    r->ahead.vy = (D_800B7500[next].left.vy * r->lane + D_800B7500[next].right.vy * (0x100 - lane)) / 256;
    r->ahead.vz = (D_800B7500[next].left.vz * r->lane + D_800B7500[next].right.vz * (0x100 - lane)) / 256;
    d.vx = r->ahead.vx - r->pos.vx;
    d.vy = 0;
    d.vz = r->ahead.vz - r->pos.vz;
    VectorNormal(&d, &d);
    r->aim.vy = chocobo_yaw_from_dir((short)d.vx, (short)d.vz);
}

/* Moves the camera's record along the course after the racer it follows: turns it to the segment after next, keeps it
 * at the ground's height and, while following, matches the racer's speed. Passing a segment sets off its prop and
 * (in a race with the player) its sound. */
#ifdef NON_MATCHING
/* Register allocation and load placement: the segment search's counters, D_800B7500/D_800B74FC kept in a register in
 * the ahead block where the game reloads them, srl against sra for the last halving, and where the forced
 * &D_800B7500 is set up. */
void chocobo_camera_move(int follow)
{
    Racer *r = &D_800B75CC[6];
    Racer *t;
    Segment *s;
    VECTOR a, b, n, d, far;
    int i, end, seg, w, ev, speed;

    s = &D_800B7500[r->next];
    a.vx = s->right.vx - s->left.vx;
    a.vy = 0;
    a.vz = s->right.vz - s->left.vz;
    b.vx = r->pos.vx - s->left.vx;
    b.vy = 0;
    b.vz = r->pos.vz - s->left.vz;
    OuterProduct12(&a, &b, &a);
    /* Past the next segment's line: find the segment it is on now. */
    if (a.vy >= 0) {
        D_800B7A28 = 0;
        for (i = r->seg, end = i + 60; i < end; i++) {
            seg = (i + D_800B74FC) % D_800B74FC;
            s = &D_800B7500[seg];
            a.vx = s->right.vx - s->left.vx;
            a.vy = 0;
            a.vz = s->right.vz - s->left.vz;
            b.vx = r->pos.vx - s->left.vx;
            b.vy = 0;
            b.vz = r->pos.vz - s->left.vz;
            OuterProduct12(&a, &b, &a);
            if (a.vy < 0)
                break;
        }
        r->seg = r->next;
        r->next = (short)(seg + D_800B74FC) % D_800B74FC;
        s = &D_800B7500[r->next];
        w = 0x100 - r->lane;
        r->ahead.vx = (s->left.vx * r->lane + s->right.vx * w) / 256;
        r->ahead.vy = (s->left.vy * r->lane + s->right.vy * w) / 256;
        r->ahead.vz = (s->left.vz * r->lane + s->right.vz * w) / 256;
        s = &D_800B7500[(r->next + 1) % D_800B74FC];
        far.vx = (s->left.vx * r->lane + s->right.vx * w) / 256;
        far.vy = (s->left.vy * r->lane + s->right.vy * w) / 256;
        far.vz = (s->left.vz * r->lane + s->right.vz * w) / 256;
        d.vx = far.vx - r->pos.vx;
        d.vy = 0;
        d.vz = far.vz - r->pos.vz;
        VectorNormal(&d, &d);
        r->aim.vy = chocobo_yaw_from_dir((short)d.vx, (short)d.vz);
        ev = D_800B7500[r->seg].right.pad;
        if (ev)
            chocobo_trigger_prop(ev);
        if (D_800B7A48 && (ev = D_800B7500[r->seg].left.pad)) {
            ev--;
            snd_request.id = D_800F5084->sounds[ev].id;
            snd_request.arg[0] = D_800F5084->sounds[ev].arg[0];
            snd_request.arg[1] = D_800F5084->sounds[ev].arg[1];
            snd_request.arg[2] = D_800F5084->sounds[ev].arg[2];
            snd_request.arg[3] = D_800F5084->sounds[ev].arg[3];
            snd_request.arg[4] = D_800F5084->sounds[ev].arg[4];
            snd_request.arg[5] = D_800F5084->sounds[ev].arg[5];
            snd_submit_request();
        }
    }
    /* Turn the short way round towards aim, an eighth at a time. */
    if (r->rot.vy - r->aim.vy > 3000)
        r->rot.vy -= 0x1000;
    if (r->rot.vy - r->aim.vy < -3000)
        r->rot.vy += 0x1000;
    r->rot.vy = (r->rot.vy * 7 + r->aim.vy) / 8;
    r->frac.vx += rsin(r->rot.vy) * r->speed / 64;
    r->frac.vz += -rcos(r->rot.vy) * r->speed / 64;
    r->pos.vx += r->frac.vx / 4096;
    r->pos.vz += r->frac.vz / 4096;
    r->frac.vx &= 0x80000FFF;
    r->frac.vz &= 0x80000FFF;
    /* The ground's height under the camera, from the plane of its segment. */
    a.vx = D_800B7500[r->next].right.vx - D_800B7500[r->next].left.vx;
    a.vy = D_800B7500[r->next].right.vy - D_800B7500[r->next].left.vy;
    a.vz = D_800B7500[r->next].right.vz - D_800B7500[r->next].left.vz;
    b.vx = D_800B7500[r->next].left.vx - D_800B7500[r->seg].left.vx;
    b.vy = D_800B7500[r->next].left.vy - D_800B7500[r->seg].left.vy;
    b.vz = D_800B7500[r->next].left.vz - D_800B7500[r->seg].left.vz;
    OuterProduct12(&b, &a, &n);
    if (n.vy) {
        i = ((D_800B7500[D_800B75CC[D_800B753C].seg].left.vx - r->pos.vx) * n.vx +
             (D_800B7500[D_800B75CC[D_800B753C].seg].left.vz - r->pos.vz) * n.vz +
             n.vy * D_800B7500[D_800B75CC[D_800B753C].seg].left.vy) / n.vy;
        D_800B75A8 = (D_800B75A8 * 7 + i) / 8;
        i = ((D_800B7500[r->seg].left.vx - r->pos.vx) * n.vx + (D_800B7500[r->seg].left.vz - r->pos.vz) * n.vz +
             n.vy * D_800B7500[r->seg].left.vy) / n.vy;
    } else
        i = r->pos.vy;
    r->y = r->pos.vy = (r->y + i) / 2;
    if (!follow || !D_800B7A64)
        return;
    t = &D_800B75CC[D_800B753C];
    seg = t->seg - r->seg;
    speed = t->speed;
    r->speed = speed;
    if (t->finished)
        return;
    /* Fall back when ahead, catch up when far behind. */
    if (seg <= 0)
        r->speed = 0;
    else if (seg >= 3)
        r->speed = speed + seg * 150;
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_camera_move);
#endif

/* Starts camera shot `shot` (1-based) on racer `racer`; before the race it only parks them. */
void chocobo_set_shot(short shot, short racer)
{
    if (D_800B7A48 == -1) {
        D_800F5054.kind = -1;
        D_800F5028 = racer;
        D_800F509C = shot;
        return;
    }
    D_800B1358 = -1;
    if (shot == -1)
        return;
    D_800F508C = D_800B753C = racer;
    D_800F5054 = D_800F5084->shots[shot - 1];
}

/* A camera's matrix from its place and what it looks at, world up. */
void chocobo_look_at(MATRIX *m, SVECTOR *eye, SVECTOR *at)
{
    VECTOR v;
    VECTOR x, z;
    VECTOR y = {0, 0x1000, 0, 0};

    v.vx = at->vx - eye->vx;
    v.vy = at->vy - eye->vy;
    v.vz = at->vz - eye->vz;
    VectorNormal(&v, &z);
    OuterProduct12(&z, &y, &v);
    VectorNormal(&v, &x);
    OuterProduct12(&z, &x, &v);
    VectorNormal(&v, &y);
    m->m[0][0] = x.vx;
    m->m[0][1] = x.vy;
    m->m[0][2] = x.vz;
    m->m[1][0] = y.vx;
    m->m[1][1] = y.vy;
    m->m[1][2] = y.vz;
    m->m[2][0] = z.vx;
    m->m[2][1] = z.vy;
    m->m[2][2] = z.vz;
    ApplyMatrix(m, eye, &v);
    m->t[0] = -v.vx;
    m->t[1] = -v.vy;
    m->t[2] = -v.vz;
}

/* Puts the camera's record on the racer it watches (a pan restarts it at the shot's segment, at a speed by the
 * shot's mode, once per shot); once the race is over, orbits the winner. */
void chocobo_update_shot(void)
{
    int b;

    if (D_800F5054.kind == 2) {
        switch (D_800F5054.mode) {
        case 0:
            b = 0;
            break;
        case 1:
            b = D_800B75CC[D_800B753C].speed / 2;
            break;
        case 2:
            b = D_800B75CC[D_800B753C].speed;
            break;
        case 3:
            b = D_800B75CC[D_800B753C].speed * 2;
            break;
        }
        if (D_800B1358) {
            chocobo_camera_follow_at(D_800B753C, b, D_800F5054.eye.pad);
            D_800B1358 = 0;
        }
    } else
        chocobo_camera_follow(D_800B753C);
    if ((D_800F5048 && !D_800B7A48) || (D_800B7A48 && D_800B75CC[0].finished)) {
        D_800F5054.kind = 10;
        D_800B759C = -1;
        D_800B753C = D_800B7594;
    }
}

/* Each frame: the camera's eye and look-at targets and the segments to draw, by the shot's kind; then the camera. */
#ifdef NON_MATCHING
/* The segment wrap: the game adds len + d before the segment, GCC folds any one expression to (seg + d) + len, so k
 * is a temporary here and gets its own register. In the pan the game reaches the records through &D_800B79A4. */
void chocobo_update_camera(void)
{
    Racer *me;
    int k;
    SVECTOR *p;

    chocobo_update_shot();
    switch (D_800F5054.kind) {
    case -1:
    default:
        me = &D_800B75CC[6];
        k = D_800B74FC - 10;
        D_800B7598 = (me->seg + k) % D_800B74FC;
        D_800F507C = (D_800B7598 + D_800B7500[me->seg].ahead) % D_800B74FC;
        D_800B1348.vy = D_800B135C[D_800F5124].dy + D_800B75A8;
        D_800B1348.vx = (D_800B1348.vx + D_800B75CC[6].pos.vx -
                         (rsin(D_800B75CC[6].rot.vy) >> D_800B135C[D_800F5124].shift)) / 2;
        D_800B1348.vz = (D_800B1348.vz + D_800B75CC[6].pos.vz -
                         (-rcos(D_800B75CC[6].rot.vy) >> D_800B135C[D_800F5124].shift)) / 2;
        D_800B1350.vy = (D_800B1350.vy * 3 + D_800B75CC[D_800B753C].pos.vy + D_800B135C[D_800F5124].look_dy) / 4;
        D_800B1350.vx = (D_800B75CC[D_800B753C].pos.vx * 7 + D_800B75CC[6].pos.vx) / 8;
        D_800B1350.vz = (D_800B75CC[D_800B753C].pos.vz * 7 + D_800B75CC[6].pos.vz) / 8;
        break;
    case 1:
        D_800B1348 = D_800F5054.eye;
        if (D_800F5054.mode) {
            k = D_800B74FC - 60;
            D_800B7598 = (D_800B75CC[D_800B753C].seg + k) % D_800B74FC;
            D_800F507C = (D_800B7598 + 120) % D_800B74FC;
            p = &D_800B75CC[D_800B753C].pos;
            D_800B1350.vx = p->vx;
            D_800B1350.vy = p->vy;
            D_800B1350.vz = p->vz;
        } else {
            D_800B1350 = D_800F5054.at;
            D_800B7598 = D_800F5054.eye.pad;
            D_800F507C = D_800F5054.at.pad;
        }
        break;
    case 2:
        me = &D_800B75CC[6];
        if (D_800B75CC[D_800B753C].seg < me->seg) {
            k = D_800B74FC - 100;
            D_800B7598 = (D_800B75CC[D_800B753C].seg + k) % D_800B74FC;
            D_800F507C = me->seg + 10;
        } else {
            k = D_800B74FC + 100;
            D_800F507C = (D_800B75CC[D_800B753C].seg + k) % D_800B74FC;
            D_800B7598 = me->seg - 15;
        }
        D_800B1348.vy = D_800B75CC[6].pos.vy + D_800F5054.height;
        D_800B1348.vx = D_800B75CC[6].pos.vx - (rsin(D_800B75CC[6].rot.vy) >> D_800B135C[D_800F5124].shift);
        D_800B1348.vz = D_800B75CC[6].pos.vz - (-rcos(D_800B75CC[6].rot.vy) >> D_800B135C[D_800F5124].shift);
        D_800B1350.vx = (D_800B75CC[D_800B753C].pos.vx + D_800B75CC[6].pos.vx) / 2;
        D_800B1350.vy = (D_800B75CC[D_800B753C].pos.vy + D_800B75CC[6].pos.vy) / 2 + 200;
        D_800B1350.vz = (D_800B75CC[D_800B753C].pos.vz + D_800B75CC[6].pos.vz) / 2;
        break;
    case 3:
        k = D_800B74FC - 60;
        D_800B7598 = (D_800B75CC[D_800B753C].seg + k) % D_800B74FC;
        D_800F507C = (D_800B7598 + 120) % D_800B74FC;
        p = &D_800B75CC[D_800B753C].pos;
        D_800B1348.vx = p->vx;
        D_800B1348.vy = p->vy;
        D_800B1348.vz = p->vz;
        D_800B1348.vy += D_800F5054.eye.vy + rcos(D_800B7538 * 16) / D_800F5054.eye.vx;
        D_800B1348.vx -= rcos(D_800B7538 * 16) / D_800F5054.eye.vz;
        D_800B1348.vz -= rsin(D_800B7538 * 16) / D_800F5054.eye.pad;
        p = &D_800B75CC[D_800B753C].pos;
        D_800B1350.vx = p->vx;
        D_800B1350.vy = p->vy;
        D_800B1350.vz = p->vz;
        break;
    case 10:
        D_800F507C = 70;
        p = &D_800B75CC[D_800B753C].pos;
        D_800B1348.vx = p->vx;
        D_800B1348.vy = p->vy;
        D_800B1348.vz = p->vz;
        D_800B7598 = D_800B74FC - 20;
        D_800B1348.vy += 800;
        D_800B1348.vx -= rcos(D_800B7538 * 16) / 6;
        D_800B1348.vz -= rsin(D_800B7538 * 16) / 6;
        p = &D_800B75CC[D_800B753C].pos;
        D_800B1350.vx = p->vx;
        D_800B1350.vy = p->vy;
        D_800B1350.vy += 200;
        D_800B1350.vz = p->vz;
        break;
    }
    chocobo_apply_camera();
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_update_camera);
#endif

/* Eases the camera's eye and look-at toward their targets (or jumps there), builds the view and loads it into the
 * GTE. */
void chocobo_apply_camera(void)
{
    VECTOR pos = {0, 0, 0, 0};
    MATRIX look;
    SVECTOR rot = {0, 0, 0, 0};
    long flag;

    if (D_800B759C) {
        D_800B7340 = D_800B1348;
        D_800B7348 = D_800B1350;
        D_800B759C = 0;
    }
    /* height eases slower than the ground plane */
    D_800B7348.vx = (D_800B7348.vx * 3 + D_800B1350.vx) / 4;
    D_800B7348.vy = (D_800B7348.vy * 7 + D_800B1350.vy) / 8;
    D_800B7348.vz = (D_800B7348.vz * 3 + D_800B1350.vz) / 4;
    D_800B7340.vx = (D_800B7340.vx * 3 + D_800B1348.vx) / 4;
    D_800B7340.vy = (D_800B7340.vy * 7 + D_800B1348.vy) / 8;
    D_800B7340.vz = (D_800B7340.vz * 3 + D_800B1348.vz) / 4;
    chocobo_look_at(&look, &D_800B7340, &D_800B7348);
    RotMatrixYXZ(&rot, &D_800B7544);
    TransMatrix(&D_800B7544, &pos);
    MulMatrix2(&look, &D_800B7544);
    SetRotMatrix(&look);
    SetTransMatrix(&look);
    RotTrans((SVECTOR *)&pos, (VECTOR *)D_800B7544.t, &flag);
    SetRotMatrix(&D_800B7544);
    SetTransMatrix(&D_800B7544);
}

/* Sets prop i's animation going forward, or (i - 0x80) backward. */
void chocobo_trigger_prop(short i)
{
    if (i > 0x80)
        D_800F5084->props[i - 0x80].step = -1;
    else
        D_800F5084->props[i].step = 1;
}

/* A heading (0-0xFFF) from a unit direction's x (by the arccos table) and the sign of z. Its callers pass shorts, but
 * it takes ints: chocobo_yaw_between inlines it, and the game keeps it too. */
inline int chocobo_yaw_from_dir(int x, int z)
{
    int i;

    if ((short)x >= 0x1000)
        return 0x400;
    if ((short)x < -0xFFF)
        return 0xC00;
    i = (x + 0x1000) & 0x1FFF;
    if ((short)z > 0)
        return (D_800B26CC[i] + 0x400) & 0xFFF;
    return (D_800B26CC[0x2000 - i] + 0xC00) & 0xFFF;
}

/* The heading from one point to another. */
int chocobo_yaw_between(SVECTOR *from, SVECTOR *to)
{
    VECTOR d;

    d.vx = to->vx - from->vx;
    d.vy = to->vy - from->vy;
    d.vz = to->vz - from->vz;
    VectorNormal(&d, &d);
    return chocobo_yaw_from_dir(d.vx, d.vz);
}

/* The race's HUD: every 16 frames sorts the standings and rebuilds their sprites; then queues the course map's frame,
 * the standings, each running racer's marker on the map, the map, and the player's stamina frame and bar. */
#ifdef NON_MATCHING
/* The sprite rebuild loop's induction variables: the game steps the colour words' pointer itself and keeps the
 * element pointer for setlen; ours does the reverse and keeps 4 in s3, so the s-registers shift from there on. */
void chocobo_draw_hud(void)
{
    Standing order[6];
    Standing tmp;
    unsigned int i, j;
    int k;
    int n, y;
    Racer *r;
    POLY_F4 *m;

    if (!(D_800B7538 & 0xF)) {
        for (i = 0; i < 6; i++) {
            if (D_800B75CC[i].finished)
                order[i].key = D_800B75CC[i].place;
            else
                order[i].key = D_800B75CC[i].rank + 10;
            order[i].id = i;
        }
        for (i = 0; i < 6; i++) {
            for (j = 0; j < 5; j++) {
                if (order[j].key > order[j + 1].key) {
                    tmp = order[j];
                    order[j] = order[j + 1];
                    order[j + 1] = tmp;
                }
            }
        }
        for (i = 0; i < 6; i++) {
            r = &D_800B75CC[order[i].id];
            if (D_800B7A48) {
                D_800B7354[i].x0 = i * 40 + 8;
                D_800B73CC[i].x0 = i * 40 + 16;
                y = D_800B1448[i];
                D_800B73CC[i].y0 = y;
                D_800B7354[i].y0 = y + 22;
            } else {
                D_800B7354[i].x0 = i * 50 + 16;
                D_800B73CC[i].x0 = i * 50 + 24;
                y = D_800B1448[i];
                D_800B73CC[i].y0 = y + 140;
                D_800B7354[i].y0 = y + 162;
            }
            D_800B73CC[i].v0 = 199;
            D_800B73CC[i].u0 = r->chocobo * 24 + 48;
            if (r->finished)
                D_800B7354[i].u0 = D_800B13B4[r->place] << 5;
            else
                D_800B7354[i].u0 = D_800B13A0[r->anim] << 5;
            D_800B7354[i].v0 = 223;
            D_800B73CC[i].clut = 0x3D67;
            D_800B7354[i].clut = (r->chocobo + 250) << 6 | 0x27;
            D_800B73CC[i].h = 24;
            D_800B73CC[i].w = 24;
            D_800B7354[i].h = 32;
            D_800B7354[i].w = 32;
            *(unsigned int *)&D_800B73CC[i].r0 = 0x64808080;
            *(unsigned int *)&D_800B7354[i].r0 = 0x64808080;
            setlen(&D_800B73CC[i], 4);
            setlen(&D_800B7354[i], 4);
        }
    }
    if (D_800B7A48) {
        if (D_800B762C)
            D_800F5074->prims[D_800F5034].ft4 = D_800B13F8;
        else
            D_800F5074->prims[D_800F5034].ft4 = D_800B1420;
        addPrim(&D_800F5074->ot[2], &D_800F5074->prims[D_800F5034]);
        D_800F5034++;
    }
    for (k = 0; k < 6; k++) {
        if (D_800B7478) {
            D_800F5074->prims[D_800F5034].sprt = D_800B13D0;
            if (D_800B7538 & 8)
                D_800F5074->prims[D_800F5034].sprt.clut = 0x3CE7;
            else
                D_800F5074->prims[D_800F5034].sprt.clut = 0x3D27;
            if (!D_800B7A48) {
                D_800F5074->prims[D_800F5034].sprt.y0 += 140;
                D_800F5074->prims[D_800F5034].sprt.x0 += 8;
            }
            addPrim(&D_800F5074->ot[3], &D_800F5074->prims[D_800F5034]);
            D_800F5034++;
            D_800F5074->prims[D_800F5034].sprt = D_800B13E4;
            if (D_800B7538 & 8)
                D_800F5074->prims[D_800F5034].sprt.clut = 0x3CE7;
            else
                D_800F5074->prims[D_800F5034].sprt.clut = 0x3D27;
            if (!D_800B7A48) {
                D_800F5074->prims[D_800F5034].sprt.y0 += 140;
                D_800F5074->prims[D_800F5034].sprt.x0 += 18;
            }
            addPrim(&D_800F5074->ot[3], &D_800F5074->prims[D_800F5034]);
            D_800F5034++;
        }
        if (D_800B7A64) {
            D_800F5074->prims[D_800F5034].sprt = D_800B73CC[k];
            addPrim(&D_800F5074->ot[3], &D_800F5074->prims[D_800F5034]);
            D_800F5034++;
            D_800F5074->prims[D_800F5034].sprt = D_800B7354[k];
            addPrim(&D_800F5074->ot[3], &D_800F5074->prims[D_800F5034]);
            D_800F5034++;
        }
        r = &D_800B75CC[k];
        if (r->finished)
            continue;
        m = &D_800F5074->markers[k];
        n = r->pos.vx / 650;
        m->x2 = m->x0 = n + 282;
        m->x3 = m->x1 = n + 285;
        n = r->pos.vz / 610;
        m->y1 = m->y0 = n + 40;
        m->y3 = m->y2 = n + 43;
        if (k == D_800B753C && (D_800B7538 & 8)) {
            D_800F5074->markers[k].r0 = D_800B1370[k].r;
            D_800F5074->markers[k].g0 = D_800B1370[k].g;
            D_800F5074->markers[k].b0 = D_800B1370[k].b;
        } else {
            D_800F5074->markers[k].r0 = D_800B1370[k].r >> 1;
            D_800F5074->markers[k].g0 = D_800B1370[k].g >> 1;
            D_800F5074->markers[k].b0 = D_800B1370[k].b >> 1;
        }
        /* the watched racer's marker draws over the others */
        if (k == D_800B753C)
            addPrim(&D_800F5074->ot[1], &D_800F5074->markers[k]);
        else
            addPrim(&D_800F5074->ot[2], &D_800F5074->markers[k]);
    }
    addPrim(&D_800F5074->ot[3], &D_800F5074->map);
    if (!D_800B7A48)
        return;
    if (D_800B7614 && (D_800B7538 & 4))
        D_800F5074->stamina_frame.clut = 0x3CA7;
    else
        D_800F5074->stamina_frame.clut = 0x2064;
    addPrim(&D_800F5074->ot[2], &D_800F5074->stamina_frame);
    y = 208 - D_800B75CC[0].stamina * 68 / D_800B75CC[0].stamina_max;
    if (y > 209)
        y = 209;
    D_800F5074->stamina_bar.y1 = y;
    D_800F5074->stamina_bar.y0 = y;
    D_800F5074->stamina_bar.r0 = 255;
    D_800F5074->stamina_bar.g0 = 20;
    D_800F5074->stamina_bar.b0 = 20;
    addPrim(&D_800F5074->ot[3], &D_800F5074->stamina_bar);
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_draw_hud);
#endif

/* Loads the racers' six CLUT rows, the player's chocobo's (and the second player's) in their own colours, and sets
 * the course map's racer colours to match. A colour above 4 copies from an unset pointer, as the game does. */
#ifdef NON_MATCHING
/* One instruction in row 1's copy: the game loads its first word by its own address (D_800B2248) and the rest from
 * D_800B2228's; ours loads all eight from the base. */
void chocobo_load_cluts(void)
{
    RECT rect = {0x270, 250, 16, 6};
    unsigned int cluts[48];
    unsigned int *row;
    unsigned int i;

    for (i = 0; i < 48; i++)
        cluts[i] = D_800B2188[i];
    if (D_800B7A48) {
        switch (D_8009D488[0x11]) {
        case 1:
            row = D_800B2228[3];
            break;
        case 2:
            row = D_800B2228[2];
            break;
        case 3:
            row = D_800B2228[1];
            break;
        case 0:
        case 4:
            row = D_800B2228[4];
            break;
        }
        for (i = 0; i < 8; i++)
            cluts[i] = row[i];
    }
    if (D_8009D488[0x16]) {
        for (i = 8; i < 16; i++)
            cluts[i] = D_800B2228[1][i - 8];
    }
    func_80044000(&rect, cluts);
    if (D_800B7A48)
        D_800B1370[0] = D_800B1388[D_8009D488[0x11]];
    if (D_8009D488[0x16]) {
        D_800B1370[1].b = 0x80;
        D_800B1370[1].g = 0x80;
        D_800B1370[1].r = 0x80;
    }
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_load_cluts);
#endif

/* A debug screen: edit the race's variables until select is pressed. */
#ifdef NON_MATCHING
/* The order of two hoisted base registers in the prologue, and a nop the game leaves in the select test's delay
 * slot where ours fills it. */
void chocobo_debug_variables(void)
{
    RECT rect;
    int cursor = 0;
    int i;
    unsigned int *pad;

    for (;;) {
        func_80044244((D_800F5074 = D_800B7A68)->ot, 1);
        VSync(3);
        rect.x = 0;
        rect.y = 0;
        rect.w = 320;
        rect.h = 480;
        ClearImage(&rect, 0, 0, 0);
        func_80026A00(D_800F5074->ot);
        func_800269C0(D_80077F64);
        pad = &D_800B7534; /* the game reads the buttons through a register for three tests */
        for (i = 0; i < 64; i++) {
            if (cursor == i)
                func_80028E00(i / 16 * 32, (i & 15) * 12 + 16, D_8009D488[i], 4, 2);
            else
                func_80028E00(i / 16 * 32, (i & 15) * 12 + 16, D_8009D488[i], 4, 7);
        }
        D_800B7534 = func_8001C808();
        if (D_800B7534 != D_800F5080) {
            if (D_800B7534 & 0x8000) {
                if (--cursor < 0)
                    cursor = 63;
            }
            if (D_800B7534 & 0x2000) {
                if (++cursor >= 64)
                    cursor = 0;
            }
            if (*pad & 0x20)
                D_8009D488[cursor]++;
            if (*pad & 0x40)
                D_8009D488[cursor]--;
        }
        if (*pad & 0x100)
            break;
        rect.x = 0;
        rect.y = 0;
        rect.w = 256;
        rect.h = 256;
        D_800F5080 = D_800B7534;
        func_80026A34(0, 1, GetTPage(0, 1, 960, 256), &rect);
        func_8004433C(D_800F5074->ot);
    }
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_debug_variables);
#endif

/* The betting before a race the player watches: the board of 15 tickets (each a pair of racers, 5x3, a prize behind
 * each), the HUD, the shown racer's figure, stats and the gil; left/right show another racer; the d-pad moves the
 * cursor; a button buys a ticket into one of 3 slots (the oldest replaced when full), given back if the gil can't pay
 * for one more; start fades out and pays for the tickets. */
#ifdef NON_MATCHING
/* &D_800B66CC is CSE'd into s0 across the buffer swap where the game reaches it by %hi/%lo at each of its three uses:
 * one instruction more, every branch after it 4 bytes on, and `sound = 0` scheduled earlier. */
/* The prize game's HUD; the last one marks the racer shown. */
const SPRT D_800A00DC[11] = {
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 14, 14, 0, 0, 0x79A4, 168, 24},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 191, 10, 0, 32, 0x79A4, 80, 24},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 194, 40, 136, 32, 0x79A4, 64, 9},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 194, 63, 136, 48, 0x79A4, 64, 9},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 194, 86, 152, 64, 0x79A4, 32, 9},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 11, 155, 216, 64, 0x79A4, 32, 12},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 295, 48, 216, 48, 0x79A4, 24, 12},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 290, 218, 216, 32, 0x79A4, 32, 12},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 256, 192, 184, 240, 0x79E4, 26, 16},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 276, 10, 80, 32, 0x7AE4, 28, 32},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 286, 181, 0, 208, 0x79E4, 20, 24},
};

void chocobo_bet(void)
{
    int sel[3];
    int slot, count, sound, n, i;

    snd_request.id = 0x10;
    snd_request.arg[0] = (long)D_8007AF64;
    snd_submit_request();
    count = 0;
    ClearImage(&D_800B14A0, 0, 0, 0);
    D_800B7444 = 900;
    D_800B7448 = 240;
    D_800B744C = 320;
    chocobo_deal_prizes();
    slot = 0;
    D_800B7470 = D_800B7474 = 0;
    D_800B7468 = 0;
    D_800B746C = sel[0] = sel[1] = sel[2] = -1;
    sound = 0;
    for (;;) {
        D_800B66CC[0] ^= 1;
        D_800F5074 = D_800F5074 == D_800B7A68 ? &D_800B7A68[1] : D_800B7A68;
        func_80044244(D_800F5074->ot, 0x1000);
        func_80044244(&D_800F5074->ot[0x1000], 1);
        func_80026A00(&D_800F5074->ot[0x1000]);
        func_800269C0(D_800B66CC[0] ? D_80078F64 : D_80077F64);
        D_800F5034 = 0;
        D_800B66D0 = D_800F5074->ot;
        chocobo_draw_cards(1);
        if (D_800B7514 >= 0x100 && D_800F5070 > 0) {
            D_800F5070 = 0;
            *D_800F502C -= D_800B1460[D_800F5098][count];
            return;
        }
        D_800B7538++;
        D_800B7468++;
        D_800B7534 = func_8001C808();
        if (D_800B7534 != D_800F5080) {
            if (D_800B7534 & 0x800)
                D_800F5070 = 0x10;
            D_800B7468 = 0;
            if (D_800B7534 & 4) {
                if (--D_800B753C < 0)
                    D_800B753C = 5;
                sound = -1;
            }
            if (D_800B7534 & 9) {
                if (++D_800B753C >= 6)
                    D_800B753C = 0;
                sound = -1;
            }
            if (D_800B7534 & 0x120 && !D_800B7A48) {
                sound = -1;
                /* the card under the cursor: put back if picked, else picked into a free slot, else in place of the
                 * oldest pick */
                for (n = 0; n < 3; n++) {
                    if (++slot >= 3)
                        slot = 0;
                    if (sel[slot] == D_800B7470 * 10 + D_800B7474) {
                        D_800B7480[D_800B7470][D_800B7474].kind = 2;
                        sel[slot] = -1;
                        break;
                    }
                }
                if (n == 3) {
                    for (n = 0; n < 3; n++) {
                        if (++slot >= 3)
                            slot = 0;
                        if (sel[slot] == -1 || sel[slot] == D_800B7470 * 10 + D_800B7474) {
                            D_800B7480[D_800B7470][D_800B7474].kind = 1;
                            sel[slot] = D_800B7470 * 10 + D_800B7474;
                            break;
                        }
                    }
                    if (n == 3) {
                        if (++slot >= 3)
                            slot = 0;
                        D_800B7480[sel[slot] / 10][sel[slot] % 10].kind = 2;
                        D_800B7480[D_800B7470][D_800B7474].kind = 1;
                        sel[slot] = D_800B7470 * 10 + D_800B7474;
                    }
                }
                count = 0;
                for (n = 0; n < 3; n++)
                    if (sel[n] != -1)
                        count++;
                /* not enough gil for one more: take it back */
                if (*D_800F502C - D_800B1460[D_800F5098][count] > *D_800F502C) {
                    count--;
                    D_800B7480[D_800B7470][D_800B7474].kind = 2;
                    sel[slot] = -1;
                }
            }
        }
        if (!D_800B7A48 && D_800F5080 != D_800B7534) {
            if (D_800B7534 & 0x8000) {
                if (--D_800B7470 < 0)
                    D_800B7470 = 4;
                sound = -1;
            }
            if (D_800B7534 & 0x2000) {
                if (++D_800B7470 >= 5)
                    D_800B7470 = 0;
                sound = -1;
            }
            if (D_800B7534 & 0x1000) {
                if (--D_800B7474 < 0)
                    D_800B7474 = 2;
                sound = -1;
            }
            if (D_800B7534 & 0x4000) {
                if (++D_800B7474 >= 3)
                    D_800B7474 = 0;
                sound = -1;
            }
        }
        D_800F5080 = D_800B7534;
        VSync(1);
        DrawSync(0);
        VSync(1);
        if (sound) {
            snd_request.id = 0x20;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 1;
            snd_submit_request();
        }
        VSync(2);
        PutDrawEnv(&D_800F5074->draw);
        func_800444AC(&D_800F5074->disp);
        ClearImage(&D_800F5074->draw.clip, 0, 0, 0);
        D_800B1584.tpage = GetTPage(1, 0, 448, 256);
        func_800442DC(&D_800B1584);
        func_800442DC(&D_800B155C);
        D_800B1584.tpage = GetTPage(1, 0, 576, 256);
        func_800442DC(&D_800B1584);
        func_800442DC(&D_800B1570);
        chocobo_draw_prize_racer(D_800B753C);
        for (i = 0; i < 10; i++) {
            D_800F5074->prims[D_800F5034].ft4 = D_800B15A8[D_800F5098 * 10 + i];
            addPrim(&D_800F5074->ot[0xFA0], &D_800F5074->prims[D_800F5034]);
            D_800F5034++;
        }
        D_800B1584.tpage = GetTPage(0, 0, 640, 256);
        D_800F5074->prims[D_800F5034].ft3 = D_800B1584;
        addPrim(&D_800F5074->ot[0x83], &D_800F5074->prims[D_800F5034]);
        D_800F5034++;
        for (i = 0; i < 10; i++) {
            D_800F5074->prims[D_800F5034].sprt = D_800A00DC[i];
            addPrim(&D_800F5074->ot[0x82], &D_800F5074->prims[D_800F5034]);
            D_800F5034++;
        }
        D_800F5074->prims[D_800F5034].sprt = D_800A00DC[i];
        D_800F5074->prims[D_800F5034].sprt.u0 = D_800B753C << 5;
        addPrim(&D_800F5074->ot[0x82], &D_800F5074->prims[D_800F5034]);
        D_800F5034++;
        chocobo_draw_number(*D_800F502C - D_800B1460[D_800F5098][count], 276, 214);
        chocobo_draw_number((short)(D_800B75CC[D_800B753C].sprint / 34), 284, 43);
        chocobo_draw_number(D_800B75CC[D_800B753C].stamina / 10, 284, 66);
        sound = 0;
        chocobo_draw_prizes();
        chocobo_draw_text(D_800B75CC[D_800B753C].name, 6, 240, 88);
        func_8004433C(&D_800F5074->ot[0xFFF]);
        func_8004433C(&D_800F5074->ot[0x1000]);
        chocobo_fade();
    }
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_bet);
#endif

/* Draws n right-aligned at (x, y) in 16x16 digits (blanks for leading zeros, a single 0 for 0), after a POLY_FT3
 * that sets their texture page. */
void chocobo_draw_number(unsigned int n, int x, int y)
{
    SPRT_16 digits[11];
    SPRT_16 *s;
    int i;
    int digit;

    D_800B1584.tpage = GetTPage(0, 0, 640, 256);
    D_800F5074->prims[D_800F5034].ft3 = D_800B1584;
    addPrim(&D_800F5074->ot[0x3D], &D_800F5074->prims[D_800F5034]);
    D_800F5034++;
    if (!n) {
        s = digits;
        s->u0 = 8;
        s->v0 = 240;
        s->clut = 0x79E4;
        setlen(s, 3);
        s->x0 = x;
        s->y0 = y;
        *(unsigned int *)&s->r0 = 0x7C808080;
        D_800F5074->prims[D_800F5034].sprt16 = *s;
        addPrim(&D_800F5074->ot[0x3C], &D_800F5074->prims[D_800F5034]);
        D_800F5034++;
        return;
    }
    for (i = 0; i < 11; i++) {
        digits[i].x0 = x - i * 12;
        digits[i].y0 = y;
        digit = n % 10;
        if (n)
            digits[i].u0 = digit * 16 + 8;
        else
            digits[i].u0 = 240;
        digits[i].v0 = 240;
        digits[i].clut = 0x79E4;
        setlen(&digits[i], 3);
        *(unsigned int *)&digits[i].r0 = 0x7C808080;
        D_800F5074->prims[D_800F5034].sprt16 = digits[i];
        addPrim(&D_800F5074->ot[0x3C], &D_800F5074->prims[D_800F5034]);
        D_800F5034++;
        n /= 10;
    }
}

/* Draws racer n's chocobo and rider under a fixed camera, running its animation; with bit 1 of the pad (debug) the
 * pad turns them and moves the camera. */
void chocobo_draw_prize_racer(int n)
{
    MATRIX m;
    SVECTOR rot;
    VECTOR pos;
    Racer *r = &D_800B75CC[n];
    Figure *body;
    char *anims;
    int anim;
    unsigned int pressed;

    rot.vz = 0x800;
    rot.vx = 0;
    rot.vy = 0;
    RotMatrix(&rot, &m);
    pos.vx = D_800B744C;
    pos.vy = D_800B7448;
    pos.vz = D_800B7444;
    TransMatrix(&m, &pos);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    PushMatrix();
    anim = r->trait == 2;
    body = &D_800B1254->figures[r->chocobo];
    anims = body->base + body->anims;
    if (D_800B7534 & 2) {
        pressed = D_800B7534 >> 16;
        if (pressed & 0x8000)
            D_800B14AC++;
        if (pressed & 0x2000)
            D_800B14AC--;
        if (pressed & 0x1000)
            D_800B14A8++;
        if (pressed & 0x4000)
            D_800B14A8--;
        if (pressed & 4)
            D_800B14B0++;
        if (pressed & 1)
            D_800B14B0--;
        if (pressed & 8) {
            D_800B7444 -= 20;
            if (D_800B7444 < 150)
                D_800B7444 = 150;
        }
        if (pressed & 2) {
            D_800B7444 += 20;
            if (D_800B7444 > 0x800)
                D_800B7444 = 0x800;
        }
        if (pressed & 0x80) {
            D_800B744C -= 20;
            if (D_800B744C < -0x400)
                D_800B744C = -0x400;
        }
        if (pressed & 0x20) {
            D_800B744C += 20;
            if (D_800B744C > 0x400)
                D_800B744C = 0x400;
        }
        if (pressed & 0x10) {
            D_800B7448 -= 20;
            if (D_800B7448 < -0x400)
                D_800B7448 = -0x400;
        }
        if (pressed & 0x40) {
            D_800B7448 += 20;
            if (D_800B7448 > 0x400)
                D_800B7448 = 0x400;
        }
        if (pressed & 0x100) {
            D_800B7444 = 900;
            D_800B7448 = 240;
            D_800B744C = 320;
        }
    } else if (D_800B7538 & 2) {
        D_800B14AC++;
    }
    D_800B1254->figures[r->rider].rx = body->rx = -D_800B14A8;
    D_800B1254->figures[r->rider].ry = body->ry = -D_800B14AC;
    D_800B1254->figures[r->rider].rz = body->rz = -D_800B14B0;
    D_800B15A4 += 0x100;
    if (D_800B15A4 >> 8 >= *(unsigned short *)(anims + anim * 16))
        D_800B15A4 &= 0xFF;
    *(int *)0x1F800000 = 3;
    chocobo_pose_figure(body, &m, anim, D_800B15A4 >> 8);
    chocobo_draw_figure(body);
    *(int *)0x1F800000 = 3;
    chocobo_pose_figure(&D_800B1254->figures[r->rider], &m, anim, D_800B15A4 >> 8);
    chocobo_draw_figure(&D_800B1254->figures[r->rider]);
    PopMatrix();
}

/* Picks three different prizes at random from the rank's table (gated ones only if allowed, at most one special,
 * which goes last), sorts them, shuffles 7, 5 and 3 cards of them over the board's tickets, and resets the cards. */
void chocobo_deal_prizes(void)
{
    int ids[16];
    int prizes[16];
    int special, got, count, r, i, j, k, t, a;
    Prize *p;

    switch (D_800F5098) {
    case 0:
        D_800B7458 = D_800B23C0;
        break;
    case 1:
        D_800B7458 = D_800B2380;
        break;
    case 2:
        D_800B7458 = D_800B232C;
        break;
    case 3:
        D_800B7458 = D_800B22D0;
        break;
    }
    i = 0;
    special = 0;
    count = *(int *)D_800B7458;
    D_800B7458++;
    D_800B745C[0] = D_800B745C[1] = D_800B745C[2] = -1;
    got = -1;
    /* the odd loop shape and pointer sum are the forms that give the game's operand order */
    do {
        r = rand() % count;
        p = (Prize *)(r * 4 + (int)D_800B7458);
        if (p->item == D_800B745C[0] || p->item == D_800B745C[1] || p->item == D_800B745C[2])
            continue;
        if (p->gated) {
            if (special && p->special)
                continue;
            if (D_800B747C) {
                D_800B745C[i] = p->item;
                i++;
            }
            if (D_800B7458[r].special) {
                got = D_800B7458[r].item;
                special = -1;
            }
        } else {
            if (special && p->special)
                continue;
            D_800B745C[i] = p->item;
            i++;
            if (p->special) {
                got = p->item;
                special = -1;
            }
        }
        if (i >= 3)
            break;
    } while (i != 3);
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 2; j++) {
            if (D_800B745C[j] > D_800B745C[j + 1]) {
                t = D_800B745C[j];
                D_800B745C[j] = D_800B745C[j + 1];
                D_800B745C[j + 1] = t;
            }
        }
    }
    /* the special prize goes last */
    if (got != -1) {
        if (D_800B745C[0] == got) {
            D_800B745C[0] = D_800B745C[2];
            D_800B745C[2] = got;
        } else if (D_800B745C[1] == got) {
            D_800B745C[1] = D_800B745C[2];
            D_800B745C[2] = got;
        }
    }
    for (i = 0; i < 7; i++) {
        ids[i] = D_800B745C[0];
        prizes[i] = 0;
    }
    for (i = 7; i < 12; i++) {
        ids[i] = D_800B745C[1];
        prizes[i] = 1;
    }
    for (i = 12; i < 15; i++) {
        ids[i] = D_800B745C[2];
        prizes[i] = 2;
    }
    for (i = 0; i < 100; i++) {
        a = rand() % 15;
        j = rand() % 15;
        t = ids[a];
        ids[a] = ids[j];
        ids[j] = t;
        t = prizes[a];
        prizes[a] = prizes[j];
        prizes[j] = t;
    }
    for (k = 0; k < 3; k++) {
        for (i = 0; i < 5; i++) {
            D_800B7480[i][k].state = 0;
            D_800B7480[i][k].width = 0;
            D_800B7480[i][k].lit = 0;
            D_800B7480[i][k].item = ids[k * 5 + i];
            D_800B7480[i][k].prize = prizes[k * 5 + i];
            D_800B7480[i][k].kind = 2;
            setlen_tag(&D_800B7A68[0].cards[i][k], 9);
            setlen_tag(&D_800B7A68[1].cards[i][k], 9);
        }
    }
    D_800F502C = &save_gil;
    D_800B753C = 0;
}

/* The results' sprites, which chocobo_results copies out. Its rodata comes before chocobo_draw_cards' jump tables,
 * so splat leaves it out of chocobo_results' asm. */
const SPRT D_800A01B8[11] = {
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 11, 155, 216, 64, 0x79A4, 32, 12},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 187, 36, 176, 112, 0x7AA4, 32, 26},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 187, 68, 176, 143, 0x7AA4, 32, 24},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 195, 108, 208, 112, 0x79E4, 28, 16},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 195, 139, 208, 128, 0x79E4, 28, 16},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 195, 168, 176, 176, 0x79E4, 28, 16},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 195, 198, 176, 192, 0x79E4, 28, 16},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 20, 10, 0, 64, 0x79A4, 144, 24},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 220, 37, 0, 88, 0x7A24, 24, 16},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 220, 70, 32, 88, 0x7A24, 16, 16},
    {0x4000000, 0x80, 0x80, 0x80, 0x66, 232, 15, 192, 216, 0x7D24, 12, 10},
};

/* Steps each card's turn (by its kind and state) and draws it: the card (the side its turn shows), its shadow and,
 * when lit, its prize; the cursor's card blinks. active: 1 in the betting, 0 on the results. */
void chocobo_draw_cards(int active)
{
    int i, k;

    for (k = 0; k < 3; k++) {
        for (i = 0; i < 5; i++) {
            switch (D_800B7480[i][k].kind) {
            case 0:
                break;
            case 1:
                switch (D_800B7480[i][k].state) {
                case 0:
                    D_800B7480[i][k].width = 0;
                    D_800B7480[i][k].state = 4;
                    break;
                case 1:
                    D_800B7480[i][k].kind = 0xFF;
                    break;
                case 2:
                    D_800B7480[i][k].width = 16;
                    D_800B7480[i][k].state = 5;
                    break;
                case 3:
                    D_800B7480[i][k].state = 4;
                    break;
                case 6:
                    D_800B7480[i][k].state = 5;
                    break;
                }
                break;
            case 2:
                switch (D_800B7480[i][k].state) {
                case 0:
                    D_800B7480[i][k].width = 0;
                    D_800B7480[i][k].state = 6;
                    break;
                case 1:
                    D_800B7480[i][k].width = 16;
                    D_800B7480[i][k].state = 3;
                    break;
                case 2:
                    D_800B7480[i][k].kind = 0xFF;
                    break;
                case 4:
                    D_800B7480[i][k].state = 3;
                    break;
                case 5:
                    D_800B7480[i][k].state = 6;
                    break;
                case 6:
                    break;
                }
                break;
            }
            switch (D_800B7480[i][k].state) {
            case 3:
                D_800B7480[i][k].back = 0;
                D_800B7480[i][k].width -= 2;
                if (!D_800B7480[i][k].width)
                    D_800B7480[i][k].state = 0;
                break;
            case 4:
                D_800B7480[i][k].back = 0;
                D_800B7480[i][k].width += 2;
                if (D_800B7480[i][k].width >= 16)
                    D_800B7480[i][k].state = 1;
                break;
            case 5:
                D_800B7480[i][k].back = 0xFF;
                D_800B7480[i][k].width -= 2;
                if (!D_800B7480[i][k].width)
                    D_800B7480[i][k].state = 0;
                break;
            case 6:
                D_800B7480[i][k].back = 0xFF;
                D_800B7480[i][k].width += 2;
                if (D_800B7480[i][k].width >= 16)
                    D_800B7480[i][k].state = 2;
                break;
            }
            D_800F5074->cards[i][k].x0 = D_800F5074->cards[i][k].x2 = i * 34 - (D_800B7480[i][k].width - 23);
            D_800F5074->cards[i][k].x1 = D_800F5074->cards[i][k].x3 = i * 34 + (D_800B7480[i][k].width + 23);
            if (D_800B7A48 && active) {
                *(unsigned int *)&D_800F5074->cards[i][k].r0 = 0x2E808080;
                if (!D_800B14E4[k * 5 + i].first || !D_800B14E4[k * 5 + i].second)
                    D_800B7480[i][k].kind = 1;
            } else if (i == D_800B7470 && k == D_800B7474 && D_800B746C) {
                D_800F5074->cards[i][k].r0 = D_800F5074->cards[i][k].b0 = D_800F5074->cards[i][k].g0 =
                    D_800B7468 & 8 ? 0xB0 : 0x50;
                D_800F5074->cards[i][k].code = 0x2E;
            } else {
                *(unsigned int *)&D_800F5074->cards[i][k].r0 = 0x2E808080;
            }
            D_800F5074->cards[i][k].u0 = D_800F5074->cards[i][k].u2 = 0x40;
            D_800F5074->cards[i][k].u1 = D_800F5074->cards[i][k].u3 = 0x62;
            if (!D_800B7480[i][k].back) {
                D_800F5074->cards[i][k].tpage = GetTPage(1, 1, 576, 256);
                D_800F5074->cards[i][k].clut = GetClut(320, 490);
                D_800F5074->cards[i][k].v0 = D_800F5074->cards[i][k].v1 = 0;
                D_800F5074->cards[i][k].v2 = D_800F5074->cards[i][k].v3 = 0x22;
                D_800F5074->cards[i][k].y0 = k * 34 - (D_800B7480[i][k].width >> 2) + 45;
                D_800F5074->cards[i][k].y1 = (D_800B7480[i][k].width >> 2) + k * 34 + 37;
                D_800F5074->cards[i][k].y2 = (D_800B7480[i][k].width >> 2) + k * 34 + 71;
                D_800F5074->cards[i][k].y3 = k * 34 - (D_800B7480[i][k].width >> 2) + 79;
            } else {
                D_800F5074->cards[i][k].tpage = GetTPage(1, 1, 576, 256);
                D_800F5074->cards[i][k].clut = GetClut(320, 491);
                D_800F5074->cards[i][k].v0 = D_800F5074->cards[i][k].v1 = 0x22;
                D_800F5074->cards[i][k].v2 = D_800F5074->cards[i][k].v3 = 0x44;
                D_800F5074->cards[i][k].y0 = (D_800B7480[i][k].width >> 2) + k * 34 + 37;
                D_800F5074->cards[i][k].y1 = k * 34 - (D_800B7480[i][k].width >> 2) + 45;
                D_800F5074->cards[i][k].y2 = k * 34 - (D_800B7480[i][k].width >> 2) + 79;
                D_800F5074->cards[i][k].y3 = (D_800B7480[i][k].width >> 2) + k * 34 + 71;
            }
            D_800F5074->card_shadows[i][k] = D_800F5074->cards[i][k];
            D_800F5074->card_shadows[i][k].tpage = GetTPage(0, 0, 640, 256);
            D_800F5074->card_shadows[i][k].clut = 0x7A24;
            D_800F5074->card_shadows[i][k].u0 = D_800F5074->card_shadows[i][k].u2 = i * 34;
            D_800F5074->card_shadows[i][k].u1 = D_800F5074->card_shadows[i][k].u3 = i * 34 + 34;
            D_800F5074->card_shadows[i][k].v0 = D_800F5074->card_shadows[i][k].v1 = k * 34 + 104;
            D_800F5074->card_shadows[i][k].v2 = D_800F5074->card_shadows[i][k].v3 = k * 34 + 138;
            if (D_800B7480[i][k].lit) {
                D_800F5074->card_items[i][k] = D_800F5074->cards[i][k];
                D_800F5074->card_items[i][k].tpage = GetTPage(0, 0, 640, 256);
                D_800F5074->card_items[i][k].clut = 0x7A64;
                D_800F5074->card_items[i][k].u0 = D_800F5074->card_items[i][k].u2 = 0xD0;
                D_800F5074->card_items[i][k].u1 = D_800F5074->card_items[i][k].u3 = 0xF2;
                D_800F5074->card_items[i][k].v0 = D_800F5074->card_items[i][k].v1 = D_800B7480[i][k].prize * 34 - 0x70;
                D_800F5074->card_items[i][k].v2 = D_800F5074->card_items[i][k].v3 = D_800B7480[i][k].prize * 34 - 0x4E;
                *(unsigned int *)&D_800F5074->card_items[i][k].r0 = 0x2E808080;
                addPrim(&D_800F5074->ot[0x3EA], &D_800F5074->card_items[i][k]);
            }
            addPrim(&D_800F5074->ot[0x3EB], &D_800F5074->card_shadows[i][k]);
            addPrim(&D_800F5074->ot[0x3EC], &D_800F5074->cards[i][k]);
        }
    }
}

/* The results: plays the sounds, finds the first two places and their ticket's card, then each frame draws the
 * places (names and portraits), the background, the HUD, the ticket's two numbers and the board (lit and dimmed at
 * frames 10, 30 and 110, the prize given at 115) until a button after 0xA0 frames fades it out. */
#ifdef NON_MATCHING
/* Register allocation: GCC hoists the addPrim mask out of the loop (the game rebuilds it in the racer loop), so the
 * line's y pointer (fp in the game) is spilled; &D_800B66CC is CSE'd into s0 (the game reaches it by %hi/%lo each
 * time); the 11-sprite unrolled loop is scheduled differently. */
void chocobo_results(void)
{
    int i, j, k, l;
    int first, second, winner, frame;
    Card *card;

    D_800B746C = -1;
    frame = 0;
    snd_request.id = 0xC1;
    snd_request.arg[0] = snd_request.arg[1] = 0x1E;
    snd_submit_request();
    ClearImage(&D_800B14A0, 0, 0, 0);
    chocobo_init_sound();
    D_800F5070 = -0x10;
    D_800B7514 = 0xFF;
    snd_request.id = 0x10;
    snd_request.arg[0] = 0;
    snd_submit_request();
    for (i = 0; i < 6; i++) {
        if (D_800B75CC[i].place == 1)
            winner = first = i;
        if (D_800B75CC[i].place == 2)
            second = i;
    }
    if (second < first) {
        i = first;
        first = second;
        second = i;
    }
    for (i = 0; i < 16; i++)
        if (first == D_800B14E4[i].first && second == D_800B14E4[i].second)
            break;
    if (i == 16)
        i = 0;
    D_800B7470 = D_800B14E4[i].col;
    D_800B7474 = D_800B14E4[i].row;
    for (;;) {
        D_800B66CC[0] ^= 1;
        D_800F5074 = D_800F5074 == D_800B7A68 ? &D_800B7A68[1] : D_800B7A68;
        func_80044244(D_800F5074->ot, 0x1000);
        func_80044244(&D_800F5074->ot[0x1000], 1);
        func_80026A00(&D_800F5074->ot[0x1000]);
        func_800269C0(D_800B66CC[0] ? D_80078F64 : D_80077F64);
        D_800F5034 = 0;
        D_800B66D0 = D_800F5074->ot;
        chocobo_draw_cards(0);
        for (i = 0; i < 6; i++) {
            for (j = 0; j < 6; j++) {
                if (D_800B75CC[j].place == i + 1) {
                    chocobo_draw_text(D_800B75CC[j].name, 7, 0xDC, D_800B14CC[i]);
                    D_800F5074->prims[D_800F5034].ft4.x0 = D_800F5074->prims[D_800F5034].ft4.x2 = 0x12A;
                    *(int *)&D_800F5074->prims[D_800F5034].ft4.r0 = 0x2E808080;
                    D_800F5074->prims[D_800F5034].ft4.x1 = D_800F5074->prims[D_800F5034].ft4.x3 = 0x13A;
                    D_800F5074->prims[D_800F5034].ft4.y0 = D_800F5074->prims[D_800F5034].ft4.y1 = D_800B14CC[i];
                    D_800F5074->prims[D_800F5034].ft4.y2 = D_800F5074->prims[D_800F5034].ft4.y3 = D_800B14CC[i] + 0x10;
                    D_800F5074->prims[D_800F5034].ft4.u0 = D_800F5074->prims[D_800F5034].ft4.u2 =
                        D_800B75CC[j].chocobo * 16 + 0x30;
                    D_800F5074->prims[D_800F5034].ft4.u1 = D_800F5074->prims[D_800F5034].ft4.u3 =
                        D_800B75CC[j].chocobo * 16 + 0x40;
                    D_800F5074->prims[D_800F5034].ft4.v0 = D_800F5074->prims[D_800F5034].ft4.v1 = 0x58;
                    D_800F5074->prims[D_800F5034].ft4.v2 = D_800F5074->prims[D_800F5034].ft4.v3 = 0x68;
                    D_800F5074->prims[D_800F5034].ft4.clut = 0x7A24;
                    D_800F5074->prims[D_800F5034].ft4.tpage = GetTPage(0, 0, 0x280, 0x100);
                    setlen(&D_800F5074->prims[D_800F5034], 9);
                    addPrim(&D_800F5074->ot[0xFA0], &D_800F5074->prims[D_800F5034]);
                    D_800F5034++;
                }
            }
        }
        if (frame == 10) {
            snd_request.id = 0x28;
            snd_request.arg[0] = 0x40;
            snd_request.arg[1] = 0x84;
            snd_submit_request();
            for (k = 0; k < 3; k++)
                for (l = 0; l < 5; l++)
                    D_800B7480[l][k].kind = D_800B7480[l][k].back ? 1 : 2;
        }
        if (frame == 30 && D_800B7A48 && winner != D_800B75CC[0].chocobo)
            for (k = 0; k < 3; k++)
                for (l = 0; l < 5; l++)
                    D_800B7480[l][k].kind = 1;
        if (frame == 0x6E)
            for (k = 0; k < 3; k++)
                for (l = 0; l < 5; l++)
                    D_800B7480[l][k].kind = D_800B7480[l][k].back ? 1 : 2;
        if (frame == 0x73) {
            for (k = 0; k < 3; k++)
                for (l = 0; l < 5; l++)
                    D_800B7480[l][k].lit = 0xFF;
            card = &D_800B7480[0][D_800B7470 * 3 + D_800B7474];
            if (card->back)
                D_8009D4A4 = card->item;
            D_8009D4A1 = D_800B75CC[0].place - 1;
        }
        frame++;
        D_800B7538++;
        D_800B7468 += 2;
        D_800B7534 = func_8001C808();
        if (D_800B7534 != D_800F5080 && (D_800B7534 & 0x8F0) && frame >= 0xA0)
            D_800F5070 = 0x10;
        if (D_800B7514 >= 0x100 && D_800F5070 > 0) {
            D_800F5070 = 0;
            snd_request.id = 0xC1;
            snd_request.arg[0] = snd_request.arg[1] = 0x1E;
            snd_submit_request();
            return;
        }
        VSync(1);
        DrawSync(0);
        VSync(1);
        VSync(2);
        PutDrawEnv(&D_800F5074->draw);
        func_800444AC(&D_800F5074->disp);
        ClearImage(&D_800F5074->draw.clip, 0, 0, 0);
        D_800B1584.tpage = GetTPage(1, 0, 0x1C0, 0x100);
        func_800442DC(&D_800B1584);
        func_800442DC(&D_800B155C);
        D_800B1584.tpage = GetTPage(1, 0, 0x240, 0x100);
        func_800442DC(&D_800B1584);
        func_800442DC(&D_800B1570);
        for (i = 0; i < 9; i++) {
            D_800F5074->prims[D_800F5034].ft4 = D_800B1BE8[D_800F5098 * 9 + i];
            addPrim(&D_800F5074->ot[0xFA0], &D_800F5074->prims[D_800F5034]);
            D_800F5034++;
        }
        D_800B1584.tpage = GetTPage(0, 0, 0x280, 0x100);
        D_800B1584.code = 0x24;
        D_800F5074->prims[D_800F5034].ft3 = D_800B1584;
        addPrim(&D_800F5074->ot[0x1F], &D_800F5074->prims[D_800F5034]);
        D_800F5034++;
        for (i = 0; i < 11; i++) {
            D_800F5074->prims[D_800F5034].sprt = D_800A01B8[i];
            addPrim(&D_800F5074->ot[0x1E], &D_800F5074->prims[D_800F5034]);
            D_800F5034++;
        }
        /* The ticket: the two chocobos' numbers. */
        *(int *)&D_800F5074->prims[D_800F5034].sprt.r0 = 0x64808080;
        setlen(&D_800F5074->prims[D_800F5034], 4);
        D_800F5074->prims[D_800F5034].sprt.x0 = 0xD5;
        D_800F5074->prims[D_800F5034].sprt.y0 = 0xA;
        D_800F5074->prims[D_800F5034].sprt.v0 = 0xD0;
        D_800F5074->prims[D_800F5034].sprt.u0 = first << 5;
        D_800F5074->prims[D_800F5034].sprt.clut = 0x7D24;
        D_800F5074->prims[D_800F5034].sprt.w = 0x14;
        D_800F5074->prims[D_800F5034].sprt.h = 0x18;
        addPrim(&D_800F5074->ot[0x1E], &D_800F5074->prims[D_800F5034]);
        D_800F5034++;
        *(int *)&D_800F5074->prims[D_800F5034].sprt.r0 = 0x64808080;
        setlen(&D_800F5074->prims[D_800F5034], 4);
        D_800F5074->prims[D_800F5034].sprt.x0 = 0xF3;
        D_800F5074->prims[D_800F5034].sprt.y0 = 0xA;
        D_800F5074->prims[D_800F5034].sprt.v0 = 0xD0;
        D_800F5074->prims[D_800F5034].sprt.u0 = second << 5;
        D_800F5074->prims[D_800F5034].sprt.clut = 0x7D24;
        D_800F5074->prims[D_800F5034].sprt.w = 0x14;
        D_800F5074->prims[D_800F5034].sprt.h = 0x18;
        addPrim(&D_800F5074->ot[0x1E], &D_800F5074->prims[D_800F5034]);
        D_800F5034++;
        chocobo_draw_prizes();
        func_8004433C(&D_800F5074->ot[0xFFF]);
        func_8004433C(&D_800F5074->ot[0x1000]);
        chocobo_fade();
    }
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo_race", chocobo_results);
#endif

/* Lists the three prizes, each name beside its icon. */
void chocobo_draw_prizes(void)
{
    int i;

    chocobo_draw_text(D_800B23EC[D_800B745C[0]], 0x10, 0x25, 0xAC);
    chocobo_draw_text(D_800B23EC[D_800B745C[1]], 0x10, 0x25, 0xC0);
    chocobo_draw_text(D_800B23EC[D_800B745C[2]], 0x10, 0x25, 0xD4);
    for (i = 0; i < 3; i++) {
        D_800F5074->prims[D_800F5034].ft4.x0 = D_800F5074->prims[D_800F5034].ft4.x2 = 0xF;
        D_800F5074->prims[D_800F5034].ft4.y0 = D_800F5074->prims[D_800F5034].ft4.y1 = i * 0x15 + 0xA9;
        D_800F5074->prims[D_800F5034].ft4.x1 = D_800F5074->prims[D_800F5034].ft4.x3 = 0x1F;
        D_800F5074->prims[D_800F5034].ft4.y2 = D_800F5074->prims[D_800F5034].ft4.y3 = i * 0x15 + 0xB9;
        D_800F5074->prims[D_800F5034].ft4.tpage = GetTPage(0, 0, 0x280, 0x100);
        D_800F5074->prims[D_800F5034].ft4.clut = 0x7A64;
        D_800F5074->prims[D_800F5034].ft4.u0 = D_800F5074->prims[D_800F5034].ft4.u2 = 0xD8;
        D_800F5074->prims[D_800F5034].ft4.u1 = D_800F5074->prims[D_800F5034].ft4.u3 = 0xE8;
        D_800F5074->prims[D_800F5034].ft4.v0 = D_800F5074->prims[D_800F5034].ft4.v1 = i * 0x22 + 0x98;
        D_800F5074->prims[D_800F5034].ft4.v2 = D_800F5074->prims[D_800F5034].ft4.v3 = i * 0x22 + 0xA8;
        *(int *)&D_800F5074->prims[D_800F5034].ft4.r0 = 0x2E808080;
        setlen(&D_800F5074->prims[D_800F5034], 9);
        addPrim(&D_800F5074->ot[2], &D_800F5074->prims[D_800F5034]);
        D_800F5034++;
    }
}

/* Draws text at (x, y) in the menu's colour 7; the second argument is unused. */
void chocobo_draw_text(unsigned char *text, int unused, int x, int y)
{
    func_80026F44(x, y, text, 7);
}

/* Darkens the screen by the fade (a subtractive full-screen quad drawn at once, under a copy of the buffer's drawing
 * environment set to subtract), then moves the fade on by its step, clamped to 0..0x100 (stopping at 0). */
void chocobo_fade(void)
{
    DRAWENV env;
    DR_ENV dr;

    if (D_800B7514) {
        env = D_800F5074->draw;
        env.tpage = GetTPage(0, 2, 0, 0);
        env.isbg = 0;
        func_80044AC0(&dr, &env);
        func_800442DC(&dr);
        D_800B14B4.r0 = D_800B14B4.g0 = D_800B14B4.b0 = D_800B7514;
        func_800442DC(&D_800B14B4);
    }
    D_800B7514 += D_800F5070;
    if (D_800B7514 < 0) {
        D_800B7514 = 0;
        D_800F5070 = 0;
    } else if (D_800B7514 > 0x100)
        D_800B7514 = 0x100;
}
