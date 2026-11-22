/* FIELD/ENDING.X, 0x800A0000-0x800A3934 (its data follows to 0x800AF414): main reads it in the field's place for the
 * SCEA logo at boot (end_logo), the opening credits before the title (end_credits(0)) and the ending's (end_credits(1)).
 * The credits are a script of 16-bit words: each frame runs its commands until one asks to be called again, then the
 * tasks it started (sprites shown in place, rolled up a letterboxed screen or moved, and a star field) draw. GCC 2.6.3,
 * ASPSX 2.21 with its divide checks, -G0. */

#include "common.h"
#include "main.h"
#include "menu.h"
#include "prims.h"
#include "cd.h"
#include "sound.h"
#include "gte.h"

typedef struct LINE_G2 {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    unsigned char r1, g1, b1, p1;
    short x1, y1;
} LINE_G2;

/* A node of a hierarchy: its matrix is its parent's times its own rotation, scale and place. */
typedef struct Node {
    struct Node *parent;
    struct Node *child;
    MATRIX m;
    VECTOR scale;
    SVECTOR rot;
    SVECTOR trans;
    int pad48;
} Node;

/* A sprite of the credits (0x88 bytes). */
typedef struct Sprite {
    unsigned short flags; /* 1 used, 2 animated, 4 looped, 8 fading, 0x10 moving, 0x8000 semi-transparent */
    short fade_left;
    short speed;
    unsigned short frame_time;
    unsigned short frame;
    unsigned char padA[2];
    void *anim;
    unsigned char r, g, b, pad13;
    unsigned char dr, dg, db, pad17; /* the colour's step while fading */
    unsigned char tr, tg, tb, pad1B; /* and where it ends */
    Node node;
    VECTOR pos; /* the place in 20.12 fixed point while moving */
    VECTOR target;
} Sprite;

/* A task run each frame, in a list kept in priority order. */
typedef struct Task {
    struct Task *prev, *next;
    void (*func)(struct Task *t);
    unsigned short id;
    unsigned char state; /* 2 added this frame, 4 running */
    unsigned char prio;
} Task;

/* libcd's sector header for streamed frames and libgpu's TIM description (layouts from Sony's Libref). */
typedef struct StHEADER {
    unsigned short id, type, secCount, nSectors;
    unsigned int frameCount, frameSize;
    unsigned short width, height;
    unsigned int dummy1, dummy2;
    CdlLOC loc;
} StHEADER;

typedef struct TIM_IMAGE {
    unsigned int mode;
    RECT *crect;
    unsigned int *caddr;
    RECT *prect;
    unsigned int *paddr;
} TIM_IMAGE;

/* An animation frame's header and pieces, as func_80036244 finds them. */
typedef struct SpriteFrame {
    unsigned char pad0, time, pad2[2];
    unsigned short y, x; /* the origin */
} SpriteFrame;

typedef struct SpritePart {
    unsigned char x, y, u, v;
    unsigned short w, h, tpage, clut;
} SpritePart;

extern short end_script_opening[];
extern short end_script_ending[];
extern int end_op_first; /* the command is starting, not being called again */
extern int end_op_more;  /* run another command this frame */
extern unsigned int end_files[]; /* sector and size, three times */
extern int end_wait_left;
extern SVECTOR end_streak[3]; /* a star's streak, from its place back along z */
extern int end_star_buffer;
extern int end_view_x, end_view_y, end_view_z;
extern int (*end_ops[30])(void);
extern unsigned int end_fade_ot[2];
extern TILE end_fade_tiles[2];
extern DR_MODE end_fade_modes[2];
extern unsigned int end_logo_ot[2];
extern DR_MODE end_logo_modes[2];
extern TILE end_logo_tiles[2];
extern void *end_movie_buf;
extern short *end_op_pc;
extern Sprite end_sprites[32];
extern Task end_sprite_task;
extern TILE end_bars_top[2], end_bars_bottom[2];
extern Node end_root;
extern Node end_stars[256];
extern LINE_G2 end_star_lines[2][256];
extern VECTOR end_up;
extern SVECTOR end_target;
extern unsigned int end_star_ot[2][256];
extern DRAWENV end_draw[2];
extern DISPENV end_disp[2];
extern unsigned int end_released1, end_released2;
extern Task end_task_head, end_task_tail;
extern unsigned int *end_cur_ot;
extern unsigned int end_pressed1, end_pressed2, end_held1, end_held2;
extern void *end_prims;
extern unsigned int end_ot[2];
extern int end_buffer;
extern int end_vsync;
extern int end_running;

extern StHEADER *movie_header;
extern DISPENV *movie_target, *movie_display;
extern SpriteFrame *D_8003623C;
extern SpritePart *D_80036240;

int printf(const char *fmt, ...);
void lzs_decode(unsigned char *src, unsigned char *dst);
void lzs_stream_start(void *buf, void *dest);
int lzs_stream_resume(void);
int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int cd_get_state(void);
void cd_init(void);
unsigned int *offset_table_entry(unsigned int *table, int i);
char *movie_open(char *buf, int id);
int movie_start(void);
void SetDispMask(int mask);
int func_800484A8(void); /* BreakDraw */
int func_80044908(void); /* GetODE: the field being drawn */
void func_80043938(int mode); /* ResetGraph */
void func_80043BA8(int level); /* SetGraphDebug */
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* SetDefDrawEnv */
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
void func_80044244(unsigned int *ot, int n); /* ClearOTag */
void func_80044000(RECT *r, void *p); /* LoadImage */
void func_80044A68(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw); /* SetDrawMode */
void OpenTIM(void *tim);
TIM_IMAGE *ReadTIM(TIM_IMAGE *t);
int IsIdleGPU(int max);
int VSync(int mode);
void SetTile(TILE *p);
void SetLineG2(LINE_G2 *p);
void StopCallback(void);
void ResetCallback(void);
void PadInit(int mode);
void InitGeom(void);
void func_80036298(void);
void SetGeomOffset(int x, int y);
void SetGeomScreen(int h);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
MATRIX *MulMatrix2(MATRIX *m0, MATRIX *m1);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
VECTOR *RotTrans(SVECTOR *v0, VECTOR *v1, long *flag);
unsigned long RotTransPers3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2, long *p,
                            long *flag);
int VectorNormal(VECTOR *v0, VECTOR *v1);
void srand(unsigned int seed);
int rand(void);
int func_80036244(void *anim, int frame); /* looks up an animation's frame: its pieces' count */

int end_flip(int mode);
void end_init_screen(int w, int h, int proj, unsigned char r, unsigned char g, unsigned char b);
void end_load_tim(unsigned int *tim, unsigned short *tpage, unsigned short *clut);
void end_node_init(Node *parent, Node *n);
void end_look_at(MATRIX *m, SVECTOR *eye, SVECTOR *at, VECTOR *up);
long end_node_update_yxz(Node *n);
void end_task_init(void);
void end_task_add(Task *t, int id, unsigned char prio, void (*func)(Task *t));
void end_task_run(void);
void end_task_remove(Task *t);
Task *end_task_find(short id);
void end_sprite_move(Sprite *s);
void end_sprite_fade(Sprite *s);
void end_sprite_animate(Sprite *s);
void *end_sprite_draw(unsigned int *ot, int unused, POLY_FT4 *p, Sprite *s);
void end_run_script(void);
void end_set_script(short *script);

/* libgpu's macros, which the fade's tile is set through (its buffer reloaded for each colour). */
#define setRGB0(p, r, g, b) ((p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b))
#define setXY0(p, x, y) ((p)->x0 = (x), (p)->y0 = (y))
#define setWH(p, _w, _h) ((p)->w = (_w), (p)->h = (_h))

/* The script's next word. */
#define ARG() (*end_op_pc++)

/* Uploads the field being drawn of the 640x480 logo at 0x80100000, a line at a time, into end_logo's odd. */
#define END_LOGO_UPLOAD()                                                                                              \
    {                                                                                                                  \
        odd = func_80044908() ^ 1;                                                                                     \
        src = (unsigned char *)0x80100000 + odd * 1280;                                                                \
        setRECT(&rect, 0, 0, 640, 1);                                                                                  \
        rect.y = odd;                                                                                                  \
        while (rect.y < 480) {                                                                                         \
            func_80044000(&rect, src);                                                                                 \
            DrawSync(0);                                                                                               \
            rect.y += 2;                                                                                               \
            src += 2560;                                                                                               \
        }                                                                                                              \
    }

/* Darkens the logo's band by a subtractive tile of (r, g, b). */
#define END_LOGO_FADE()                                                                                                \
    {                                                                                                                  \
        unsigned int *ot;                                                                                              \
        TILE *tile;                                                                                                    \
                                                                                                                       \
        ot = &end_logo_ot[buf];                                                                                        \
        func_80044244(ot, 1);                                                                                          \
        tile = &end_logo_tiles[buf];                                                                                   \
        SetTile(tile);                                                                                                 \
        func_80046848(tile, 1);                                                                                        \
        tile->x0 = 30;                                                                                                 \
        tile->y0 = 200;                                                                                                \
        tile->w = 580;                                                                                                 \
        tile->h = 74;                                                                                                  \
        tile->r0 = r;                                                                                                  \
        tile->g0 = g;                                                                                                  \
        tile->b0 = b;                                                                                                  \
        AddPrim(ot, tile);                                                                                             \
        func_80044A68(&end_logo_modes[buf], 1, 1, GetTPage(2, 2, 0, 0), 0);                                            \
        AddPrim(ot, &end_logo_modes[buf]);                                                                             \
        func_8004433C(ot);                                                                                             \
    }

/* The SCEA logo (STARTUP\SCEAP.LZS): faded in, held five seconds, faded out. */
void end_logo(void)
{
    RECT rect;
    CdlFILE file;
    int r, g, b, i, buf, ret, odd;
    unsigned char *src;

#ifdef NATIVE
    buf = 0; /* the game starts from whatever its register held */
#endif
    end_init_screen(640, 480, 512, 0, 0, 0);
    do {
        switch ((long)func_8003FB8C(&file, "\\STARTUP\\SCEAP.LZS;1")) {
        case -1:
        case 0:
            printf("scea file read error\n");
            return;
        }
        func_8003E100(CdlSetloc, &file.pos, 0);
        CdRead((file.size + 2047) / 2048, (void *)0x80180000, CdlModeSpeed);
        while ((ret = CdReadSync(1, 0)) > 0)
            ;
    } while (ret);
    lzs_decode((unsigned char *)0x80180000, (unsigned char *)0x80100000);
    SetDispMask(1);
    r = g = b = 254;
    do {
        buf = !buf;
        end_flip(0);
        END_LOGO_UPLOAD();
        END_LOGO_FADE();
        r -= 2;
        g -= 2;
        b -= 2;
    } while (r > 0);
    for (i = 0; i < 300; i++) {
        buf = !buf;
        end_flip(0);
        END_LOGO_UPLOAD();
    }
    while (r < 254) {
        buf = !buf;
        end_flip(0);
        END_LOGO_UPLOAD();
        END_LOGO_FADE();
        r += 2;
        g += 2;
        b += 2;
    }
    SetDispMask(0);
}

/* The credits: the opening's until a button fades them out, or the ending's, for good. */
void end_credits(int ending)
{
    unsigned char unused[256]; /* unused, but the game's frame has it */
    RECT rect;
    short b;

    for (;;) {
        end_init_screen(320, 240, 512, 0, 0, 0);
        if (ending)
            setRECT(&rect, 0, 0, 960, 480);
        else
            setRECT(&rect, 0, 0, 480, 480);
        /* A loop that runs once: GCC moves nothing across it, and the game loads the script's address after the
         * stores. */
        do {
            ClearImage(&rect, 0, 0, 0);
            while (DrawSync(1))
                ;
            end_task_init();
            end_vsync = 0;
            end_prims = (void *)0x801F0000;
        } while (0);
        end_set_script(ending ? end_script_ending : end_script_opening);
        end_running = 1;
        do {
            VSync(1);
            DrawSync(0);
            VSync(1);
            if ((end_buffer = end_flip(end_vsync)))
                end_prims = (void *)0x801F0000;
            rect.x = movie_target->disp.x;
            rect.y = movie_target->disp.y;
            if (!movie_display->isrgb24)
                rect.w = movie_target->disp.w;
            else
                rect.w = movie_target->disp.w * 3 / 2;
            rect.h = movie_target->disp.h;
            ClearImage(&rect, 0, 0, 0);
            DrawSync(0);
            VSync(1);
            if (cd_step() == CD_MOVIE_READY)
                movie_start();
            func_80044244(&end_ot[end_buffer], 1);
            end_cur_ot = &end_ot[end_buffer];
            end_run_script();
            end_task_run();
            func_8004433C(&end_ot[end_buffer]);
            VSync(1);
            if (!ending && end_pressed1 & 0x9F0)
                goto fade_out;
        } while (end_running);
    }
fade_out:
    snd_request.id = 0xC1;
    snd_request.arg[0] = 60;
    snd_request.arg[1] = 0;
    snd_submit_request();
    for (b = 0; b < 255; b += 4) {
        end_buffer = end_flip(end_vsync);
        rect.x = movie_target->disp.x;
        rect.y = movie_target->disp.y;
        rect.w = movie_target->disp.w;
        rect.h = movie_target->disp.h;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        if (cd_step() == CD_MOVIE_READY)
            movie_start();
        func_80044244(&end_ot[end_buffer], 1);
        end_cur_ot = &end_ot[end_buffer];
        end_task_run();
        func_8004433C(&end_ot[end_buffer]);
        func_80044244(&end_fade_ot[end_buffer], 1);
        SetTile(&end_fade_tiles[end_buffer]);
        func_80046848(&end_fade_tiles[end_buffer], 1);
        setRGB0(&end_fade_tiles[end_buffer], b, b, b);
        setXY0(&end_fade_tiles[end_buffer], 0, 0);
        setWH(&end_fade_tiles[end_buffer], 320, 240);
        AddPrim(&end_fade_ot[end_buffer], &end_fade_tiles[end_buffer]);
        func_80044A68(&end_fade_modes[end_buffer], 0, 0, GetTPage(2, 2, 0, 0), 0);
        AddPrim(&end_fade_ot[end_buffer], &end_fade_modes[end_buffer]);
        func_8004433C(&end_fade_ot[end_buffer]);
    }
    VSync(4);
    func_80043938(1);
}

/* Command 11's task: the sprites at a fixed place. */
void end_task_still(Task *t)
{
    unsigned char unused[256]; /* unused, but the game's frame has it */
    Sprite *s;
    int i;

    for (i = 0; i < 32; i++) {
        if (!(end_sprites[i].flags & 1))
            continue;
        s = &end_sprites[i];
        end_sprites[i].node.trans.vx = 40;
        end_sprites[i].node.trans.vy = 32;
        end_sprites[i].node.trans.vz = 0;
        end_sprite_animate(s);
        end_sprite_fade(s);
        end_prims = end_sprite_draw(end_cur_ot, 0, end_prims, s);
    }
}

/* Clears the sprites, for one of the three tasks that draw them. */
#define END_SPRITES_CLEAR()                                                                                            \
    {                                                                                                                  \
        int i;                                                                                                         \
                                                                                                                       \
        for (i = 0; i < 32; i++) {                                                                                     \
            end_sprites[i].flags = 0;                                                                                  \
            end_sprites[i].frame_time = 0;                                                                             \
            end_sprites[i].frame = 0;                                                                                  \
            end_sprites[i].anim = 0;                                                                                   \
            end_sprites[i].node.trans.vx = 0;                                                                          \
            end_sprites[i].node.trans.vy = 0;                                                                          \
            end_sprites[i].node.trans.vz = 0;                                                                          \
            end_sprites[i].r = 0;                                                                                      \
            end_sprites[i].g = 0;                                                                                      \
            end_sprites[i].b = 0;                                                                                      \
        }                                                                                                              \
    }

/* Command 11: sprites at a fixed place. */
int end_op_still(void)
{
    END_SPRITES_CLEAR();
    end_task_add(&end_sprite_task, 4, 0x80, end_task_still);
    return 1;
}

/* Command 13 (sprite, animation): starts a looped animation. */
int end_op_sprite(void)
{
    int i, anim;

    i = ARG();
    anim = ARG();
    end_sprites[i].flags = 7;
    end_sprites[i].frame_time = 0;
    end_sprites[i].frame = 0;
    end_sprites[i].anim = offset_table_entry((unsigned int *)0x800D0000, anim);
    end_sprites[i].node.trans.vx = 0;
    end_sprites[i].node.trans.vy = 0;
    end_sprites[i].node.trans.vz = 0;
    end_sprites[i].r = 0;
    end_sprites[i].g = 0;
    end_sprites[i].b = 0;
    return 1;
}

/* Command 15 (sprite, frames, brightness): fades a sprite to a grey, then runs the next command. */
int end_op_fade(void)
{
    int i, n, c;
    int dr, dg, db;

    i = ARG();
    n = ARG();
    c = ARG();
    dr = (c - end_sprites[i].r) / n;
    dg = (c - end_sprites[i].g) / n;
    db = (c - end_sprites[i].b) / n;
    end_sprites[i].fade_left = n;
    end_sprites[i].tr = c;
    end_sprites[i].tg = c;
    end_sprites[i].tb = c;
    end_sprites[i].flags |= 8;
    end_sprites[i].dr = dr;
    end_sprites[i].dg = dg;
    end_sprites[i].db = db;
    end_op_more = 1;
    return 1;
}

/* Command 17's task: the sprites rolled up between the letterbox's bars, each gone 16 lines above the top. */
void end_task_roll(Task *t)
{
    Sprite *s;
    int i;

    AddPrim(end_cur_ot, &end_bars_top[end_buffer]);
    AddPrim(end_cur_ot, &end_bars_bottom[end_buffer]);
    for (i = 0; i < 32; i++) {
        if (!(end_sprites[i].flags & 1))
            continue;
        s = &end_sprites[i];
        end_sprites[i].node.trans.vy--;
        if (end_sprites[i].node.trans.vy == -16)
            end_sprites[i].flags = 0;
        end_sprite_animate(s);
        end_sprite_fade(s);
        end_prims = end_sprite_draw(end_cur_ot, 0, end_prims, s);
    }
}

/* Command 17: the rolling credits. */
int end_op_roll(void)
{
    int i;

    END_SPRITES_CLEAR();
    end_task_add(&end_sprite_task, 4, 0x80, end_task_roll);
    for (i = 0; i < 2; i++) {
        SetTile(&end_bars_top[i]);
        SetTile(&end_bars_bottom[i]);
        end_bars_top[i].x0 = 0;
        end_bars_top[i].y0 = 0;
        end_bars_top[i].w = 320;
        end_bars_top[i].h = 40;
        end_bars_bottom[i].x0 = 0;
        end_bars_bottom[i].y0 = 200;
        end_bars_bottom[i].w = 320;
        end_bars_bottom[i].h = 40;
        end_bars_top[i].r0 = 0;
        end_bars_top[i].g0 = 0;
        end_bars_top[i].b0 = 0;
        end_bars_bottom[i].r0 = 0;
        end_bars_bottom[i].g0 = 0;
        end_bars_bottom[i].b0 = 0;
    }
    return 1;
}

/* Command 18 (animation, frame): a sprite in the first free place, at the bottom of the roll; called again until one
 * is free. */
int end_op_add(void)
{
    int i, anim, frame;

    anim = ARG();
    frame = ARG();
    for (i = 0; i < 32; i++) {
        if (end_sprites[i].flags & 1)
            continue;
        end_sprites[i].flags = 1;
        end_sprites[i].frame_time = 0;
        end_sprites[i].frame = frame;
        end_sprites[i].anim = offset_table_entry((unsigned int *)0x800D0000, anim);
        end_sprites[i].node.trans.vx = 24;
        end_sprites[i].node.trans.vy = 200;
        end_sprites[i].node.trans.vz = 0;
        end_sprites[i].r = 128;
        end_sprites[i].g = 128;
        end_sprites[i].b = 128;
        return 1;
    }
    return 0;
}

/* Command 20's task: the sprites moving. */
void end_task_move(Task *t)
{
    Sprite *s;
    int i;

    for (i = 0; i < 32; i++) {
        if (!(end_sprites[i].flags & 1))
            continue;
        s = &end_sprites[i];
        end_sprite_move(s);
        end_sprite_animate(s);
        end_sprite_fade(s);
        end_prims = end_sprite_draw(end_cur_ot, 0, end_prims, s);
    }
}

/* Command 20: moving sprites. */
int end_op_moving(void)
{
    int i;

    for (i = 0; i < 32; i++) {
        end_sprites[i].flags = 0;
        end_sprites[i].frame_time = 0;
        end_sprites[i].frame = 0;
        end_sprites[i].anim = 0;
        end_sprites[i].node.trans.vx = 0;
        end_sprites[i].node.trans.vy = 0;
        end_sprites[i].node.trans.vz = 0;
        end_sprites[i].pos.vx = 0;
        end_sprites[i].pos.vy = 0;
        end_sprites[i].pos.vz = 0;
        end_sprites[i].r = 0;
        end_sprites[i].g = 0;
        end_sprites[i].b = 0;
    }
    end_task_add(&end_sprite_task, 4, 0x80, end_task_move);
    return 1;
}

/* Command 21 (sprite, flags, animation, frame, x, y, z, r, g, b): sets a sprite up. */
int end_op_place(void)
{
    Sprite *s;
    int i, anim, frame;

    i = ARG();
    end_sprites[i].flags = ARG();
    anim = ARG();
    frame = ARG();
    end_sprites[i].frame_time = 0;
    end_sprites[i].frame = frame;
    end_sprites[i].anim = offset_table_entry((unsigned int *)0x800D0000, anim);
    s = &end_sprites[i];
    end_sprites[i].node.trans.vx = s->pos.vx = ARG();
    end_sprites[i].node.trans.vy = s->pos.vy = ARG();
    end_sprites[i].node.trans.vz = s->pos.vz = ARG();
    end_sprites[i].r = ARG();
    end_sprites[i].g = ARG();
    end_sprites[i].b = ARG();
    end_sprites[i].pos.vx <<= 12;
    end_sprites[i].pos.vy <<= 12;
    end_sprites[i].pos.vz <<= 12;
    return 1;
}

/* Command 23 (sprite, x, y, z, speed): moves a sprite to a place. */
int end_op_move(void)
{
    int i;

    i = ARG();
    end_sprites[i].flags |= 0x10;
    end_sprites[i].target.vx = ARG();
    end_sprites[i].target.vy = ARG();
    end_sprites[i].target.vz = ARG();
    end_sprites[i].speed = ARG();
    return 1;
}

/* Command 28: a star field, 256 stars in rows in front of the camera, coming at it. */
#ifdef NON_MATCHING
/* One instruction short: the game derives the star and its place from one load of the array's address and
 * steps both, where GCC loads it for each. */
int end_op_stars(void)
{
    Node *star;
    SVECTOR *pos;
    int i, k, x, y;

    srand(VSync(1));
    end_node_init(0, &end_root);
    end_root.trans.vx = 0;
    end_root.trans.vy = 0;
    end_root.trans.vz = -1024;
    x = -0x2000;
    y = -1005;
    for (i = 0, star = end_stars, pos = &end_stars[0].trans; i < 256; i++, star++, pos = &star->trans) {
        end_node_init(&end_root, star);
        k = rand();
        pos->vx = x;
        x += 0x400;
        pos->vy = y;
        pos->vz = k / 2 + 0x4000;
        if (x >= 0x2000) {
            x = -0x2000;
            y += 0x100;
        }
    }
    for (k = 0; k < 2; k++) {
        for (i = 0; i < 256; i++) {
            end_star_lines[k][i].r1 = 160;
            end_star_lines[k][i].g1 = 220;
            end_star_lines[k][i].b1 = 250;
            end_star_lines[k][i].r0 = 16;
            end_star_lines[k][i].g0 = 16;
            end_star_lines[k][i].b0 = 16;
            end_star_lines[k][i].r0 = end_star_lines[k][i].g0 = end_star_lines[k][i].b0 = 0;
            SetLineG2(&end_star_lines[k][i]);
        }
    }
    SetDispMask(1);
    return 1;
}
#else
INCLUDE_ASM("asm/overlay/ending/nonmatchings/ending", end_op_stars);
#endif

/* Command 29: the star field's frame, run for good. The pad moves a view that nothing reads. */
#ifdef NON_MATCHING
/* Same length: the target's four stores take their addresses in another order, and the depth and the
 * colour step swap registers. */
int end_op_stars_frame(void)
{
    unsigned int *ot;
    long fade, p, flag;
    unsigned long otz;
    int i, c;

    end_star_buffer ^= 1;
    if (end_held1 & 0x1000)
        end_view_z -= 8;
    else if (end_held1 & 0x4000)
        end_view_z += 8;
    else if (end_held1 & 0x2000)
        end_view_y += 8;
    else if (end_held1 & 0x8000)
        end_view_y -= 8;
    else if (end_held1 & 4)
        end_view_x -= 16;
    else if (end_held1 & 8)
        end_view_x += 16;
    end_target.vx = end_target.vy = end_target.vz = 0;
    end_target.vz = 0;
    end_up.vx = 0;
    end_up.vy = -0x1000;
    end_up.vz = 0;
    end_look_at(&end_root.m, &end_root.trans, &end_target, &end_up);
    ot = end_star_ot[end_star_buffer];
    func_8004418C(ot, 256);
    for (i = 0; i < 256; i++) {
        if (end_star_buffer)
            end_stars[i].trans.vz -= 32;
        else
            end_stars[i].trans.vz -= 128;
        if (end_stars[i].trans.vz < 0)
            end_stars[i].trans.vz = 0x4000;
        end_node_update_yxz(&end_stars[i]);
        SetRotMatrix(&end_stars[i].m);
        SetTransMatrix(&end_stars[i].m);
        otz = RotTransPers3(&end_streak[0], &end_streak[1], &end_streak[2],
                            (long *)&end_star_lines[end_star_buffer][i].x0,
                            (long *)&end_star_lines[end_star_buffer][i].x1, &fade, &p, &flag);
        fade = end_stars[i].trans.vz >> 6;
        c = 160 - fade;
        if (c < 0)
            end_star_lines[end_star_buffer][i].r1 = 0;
        else
            end_star_lines[end_star_buffer][i].r1 = c;
        c = 220 - fade;
        if (c < 0)
            end_star_lines[end_star_buffer][i].g1 = 0;
        else
            end_star_lines[end_star_buffer][i].g1 = c;
        c = 250 - fade;
        if (c < 0)
            end_star_lines[end_star_buffer][i].b1 = 0;
        else
            end_star_lines[end_star_buffer][i].b1 = c;
        AddPrim(&ot[otz >> 6], &end_star_lines[end_star_buffer][i]);
    }
    func_8004433C(ot);
    return 0;
}
#else
INCLUDE_ASM("asm/overlay/ending/nonmatchings/ending", end_op_stars_frame);
#endif

/* Runs the script's commands until one is to be called again next frame (it returns 0) and asks for no more. */
void end_run_script(void)
{
    short *pc;

    do {
        pc = end_op_pc;
        end_op_more = 0;
        end_op_pc = pc + 1;
        if (!DATA_CALL0(end_ops[*pc])) {
            end_op_pc = pc;
            end_op_first = 0;
        } else
            end_op_first = 1;
    } while (end_op_more);
}

void end_set_script(short *script)
{
    end_op_pc = script;
}

/* Command 0: stops the script. */
int end_op_stop(void)
{
    return 0;
}

/* Command 1 (file): starts reading one of three files to 0x800D0000. */
int end_op_read(void)
{
    int i;

    i = ARG();
    cd_read(end_files[i * 2], end_files[i * 2 + 1], (void *)0x800D0000, 0);
    return 1;
}

/* Command 2 (file): the same, unpacked as it's read. */
int end_op_read_lzs(void)
{
    int i;

    i = ARG();
    cd_read_lzs(end_files[i * 2], end_files[i * 2 + 1], (void *)0x800D0000, 0);
    return 1;
}

/* Command 3: waits for the read. */
int end_op_wait_read(void)
{
    return cd_get_state() == CD_IDLE;
}

/* Command 4 (picture): uploads one of the file's TIMs. */
int end_op_tim(void)
{
    unsigned short tpage, clut;

    end_load_tim(offset_table_entry((unsigned int *)0x800D0000, ARG()), &tpage, &clut);
    return 1;
}

/* Command 5 (picture): unpacks one of the file's packed TIMs to 0x80120000, a little each frame, and uploads it. */
int end_op_tim_lzs(void)
{
    unsigned short tpage, clut;
    int i;

    i = ARG();
    if (end_op_first)
        lzs_stream_start(offset_table_entry((unsigned int *)0x800D0000, i), (void *)0x80120000);
    if (lzs_stream_resume())
        return 0;
    end_load_tim((unsigned int *)0x80120000, &tpage, &clut);
    return 1;
}

/* Command 6 (movie): opens a movie. */
int end_op_movie(void)
{
    end_movie_buf = (void *)0x801A0000;
    movie_open((char *)0x801A0000, ARG());
    return 1;
}

/* Command 7: waits for the drive, as command 3. */
int end_op_wait_idle(void)
{
    return cd_get_state() == CD_IDLE;
}

/* Command 8 (width, height, projection, r, g, b): sets the screen up again. */
int end_op_screen(void)
{
    RECT unused; /* unused, but the game's frame has it */

#ifdef NATIVE
    {
        /* GCC on the PS1 reads the arguments left to right; the C standard leaves the order open. */
        int w = ARG(), h = ARG(), proj = ARG(), r = ARG(), g = ARG(), b = ARG();

        end_init_screen(w, h, proj, r, g, b);
    }
#else
    end_init_screen(ARG(), ARG(), ARG(), ARG(), ARG(), ARG());
#endif
    end_flip(0);
    return 1;
}

/* Command 9 (on): shows or hides the screen. */
int end_op_display(void)
{
    SetDispMask(ARG());
    return 1;
}

/* Command 10 (frames): waits. */
int end_op_wait(void)
{
    if (end_op_first)
        end_wait_left = ARG();
    else
        end_op_pc++;
    return --end_wait_left == 0;
}

/* Command 12: ends the sprites' task. */
int end_op_end_sprites(void)
{
    end_task_remove(end_task_find(4));
    return 1;
}

/* Command 14 (n): jumps back over the n words before it. */
int end_op_back(void)
{
    int n;

    n = *end_op_pc + 1;
    end_op_pc -= n;
    return 1;
}

/* Command 16 (mode): the VSync mode each frame waits with. */
int end_op_vsync(void)
{
    end_vsync = ARG();
    return 1;
}

/* Command 19: the credits are over (end_credits starts them again). */
int end_op_finish(void)
{
    end_running = 0;
    return 1;
}

/* Command 22 (sprite): removes a sprite. */
int end_op_remove(void)
{
    end_sprites[ARG()].flags = 0;
    return 1;
}

/* Command 24 (file): plays one of the file's songs. */
int end_op_music(void)
{
    snd_request.id = 0x10;
    snd_request.arg[0] = (long)offset_table_entry((unsigned int *)0x800D0000, ARG());
    snd_submit_request();
    return 1;
}

/* Command 26 (command, two arguments): a sound command. */
int end_op_sound(void)
{
    snd_request.id = ARG();
    snd_request.arg[0] = ARG();
    snd_request.arg[1] = ARG();
    snd_submit_request();
    return 1;
}

/* Command 25: waits for the movie to play. */
int end_op_wait_movie(void)
{
    return cd_get_state() == CD_MOVIE_PLAY;
}

/* Command 27 (frame): waits for the movie's frame. */
int end_op_wait_frame(void)
{
    int frame;

    frame = ARG();
    return movie_header->frameCount >= frame;
}

void end_init_system(void)
{
    StopCallback();
    ResetCallback();
    func_80043938(0);
    PadInit(0);
    InitGeom();
    func_80036298();
    cd_init();
}

void end_wait_gpu(void)
{
    int busy;

    while ((busy = func_800484A8()) == -1)
        VSync(0);
    if (busy)
        while (IsIdleGPU(1))
            ;
}

/* Sets the screen up: w x h, the other buffer below for less than 480 lines, the projection and background colour. */
void end_init_screen(int w, int h, int proj, unsigned char r, unsigned char g, unsigned char b)
{
    RECT rect;
    int y, busy;

    y = h == 480 ? 0 : 240;
    while ((busy = func_800484A8()) == -1)
        VSync(0);
    if (busy)
        while (IsIdleGPU(1))
            ;
    func_80043938(1);
    SetDispMask(0);
    func_80043BA8(0);
    SetGeomOffset(w / 2, h / 2);
    SetGeomScreen(proj);
    func_80043814(end_draw, 0, 0, w, h);
    SetDefDispEnv(end_disp, 0, y, w, h);
    func_80043814(&end_draw[1], 0, y, w, h);
    SetDefDispEnv(&end_disp[1], 0, 0, w, h);
    end_buffer = 1;
    end_disp[0].isrgb24 = end_disp[1].isrgb24 = 0;
    end_draw[0].isbg = end_draw[1].isbg = 0;
    end_draw[0].dfe = end_draw[1].dfe = 0;
    end_draw[0].dtd = end_draw[1].dtd = 0;
    end_draw[0].tpage = end_draw[1].tpage = 0;
    end_draw[0].r0 = r;
    end_draw[0].g0 = g;
    end_draw[0].b0 = b;
    end_draw[1].r0 = r;
    end_draw[1].g0 = g;
    end_draw[1].b0 = b;
    setRECT(&rect, 0, 0, w * 3 / 2, 480);
    ClearImage(&rect, 0, 0, 0);
    end_flip(0);
}

/* Shows what was drawn, swaps the buffers and reads the pads. Returns the buffer now drawn into. */
int end_flip(int mode)
{
    unsigned int pad, pad2, r1, r2;

    end_buffer ^= 1;
    DrawSync(0);
    VSync(mode);
    func_800444AC(&end_disp[end_buffer]);
    PutDrawEnv(&end_draw[end_buffer]);
    movie_display = &end_disp[end_buffer];
    movie_target = (DISPENV *)&end_draw[end_buffer];
    pad = func_8001C808();
    pad2 = pad >> 16;
    r1 = end_released1;
    r2 = end_released2;
    end_held1 = pad;
    end_held2 = pad2;
    end_released1 = ~pad;
    end_released2 = ~pad2;
    end_pressed1 = r1 & pad;
    end_pressed2 = r2 & pad2;
    return end_buffer;
}

/* Uploads a TIM's picture and palette and gives their texture page and CLUT. */
void end_load_tim(unsigned int *tim, unsigned short *tpage, unsigned short *clut)
{
    TIM_IMAGE t;

    OpenTIM(tim);
    ReadTIM(&t);
    if (t.paddr) {
        func_80044000(t.prect, t.paddr);
        *tpage = GetTPage(t.mode, 0, t.prect->x, t.prect->y);
    }
    if (t.caddr) {
        func_80044000(t.crect, t.caddr);
        *clut = GetClut(t.crect->x, t.crect->y);
    }
}

void end_node_init(Node *parent, Node *n)
{
#ifdef NATIVE
    if (parent) /* the game writes 0x00000004, kernel RAM the PC hasn't got */
#endif
        parent->child = n;
    n->parent = parent;
    n->scale.vx = n->scale.vy = n->scale.vz = n->scale.pad = 0x1000;
    n->rot.vx = n->rot.vy = n->rot.vz = n->rot.pad = n->trans.vx = n->trans.vy = n->trans.vz = n->trans.pad = 0;
}

/* A camera's matrix from its place and rotation. */
void end_view_matrix(MATRIX *m, SVECTOR *eye, SVECTOR *rot)
{
    VECTOR unused; /* unused, but the game's frame has it */

    RotMatrix(rot, m);
    gte_SetRotMatrix(m);
    gte_ldv0(eye);
    gte_rtv0();
    gte_stlvnl(m->t);
    m->t[0] = -m->t[0];
    m->t[1] = -m->t[1];
    m->t[2] = -m->t[2];
}

/* A camera's matrix from its place, what it looks at and which way is up. */
void end_look_at(MATRIX *m, SVECTOR *eye, SVECTOR *at, VECTOR *up)
{
    VECTOR v, x, y, z;

    v.vx = at->vx - eye->vx;
    v.vy = at->vy - eye->vy;
    v.vz = at->vz - eye->vz;
    VectorNormal(&v, &z);
    if (z.vz == up->vz)
        z.vz++;
    gte_ldopv1(&z);
    gte_ldopv2(up);
    gte_op12();
    gte_stlvnl(&v);
    VectorNormal(&v, &x);
    gte_ldopv1(&z);
    gte_ldopv2(&x);
    gte_op12();
    gte_stlvnl(&v);
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
    gte_SetRotMatrix(m);
    gte_ldv0(eye);
    gte_rtv0();
    gte_stlvnl(m->t);
    m->t[0] = -m->t[0];
    m->t[1] = -m->t[1];
    m->t[2] = -m->t[2];
}

/* The same, without negating the place. */
void end_look_at2(MATRIX *m, SVECTOR *eye, SVECTOR *at, VECTOR *up)
{
    VECTOR v, x, y, z;

    v.vx = at->vx - eye->vx;
    v.vy = at->vy - eye->vy;
    v.vz = at->vz - eye->vz;
    VectorNormal(&v, &z);
    if (z.vz == up->vz)
        z.vz++;
    gte_ldopv1(&z);
    gte_ldopv2(up);
    gte_op12();
    gte_stlvnl(&v);
    VectorNormal(&v, &x);
    gte_ldopv1(&z);
    gte_ldopv2(&x);
    gte_op12();
    gte_stlvnl(&v);
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
    gte_SetRotMatrix(m);
    gte_ldv0(eye);
    gte_rtv0();
    gte_stlvnl(m->t);
}

/* A node's matrix from its parent's, and its place on screen's terms; returns RotTrans's flag. */
long end_node_update(Node *n)
{
    long flag;

    RotMatrix(&n->rot, &n->m);
    n->m.t[0] = n->trans.vx;
    n->m.t[1] = n->trans.vy;
    n->m.t[2] = n->trans.vz;
    ScaleMatrix(&n->m, &n->scale);
    MulMatrix2(&n->parent->m, &n->m);
    SetRotMatrix(&n->parent->m);
    SetTransMatrix(&n->parent->m);
    RotTrans(&n->trans, (VECTOR *)n->m.t, &flag);
    return flag;
}

/* The same, rotating in Y, X, then Z. */
long end_node_update_yxz(Node *n)
{
    long flag;

    RotMatrixYXZ(&n->rot, &n->m);
    n->m.t[0] = n->trans.vx;
    n->m.t[1] = n->trans.vy;
    n->m.t[2] = n->trans.vz;
    ScaleMatrix(&n->m, &n->scale);
    MulMatrix2(&n->parent->m, &n->m);
    SetRotMatrix(&n->parent->m);
    SetTransMatrix(&n->parent->m);
    RotTrans(&n->trans, (VECTOR *)n->m.t, &flag);
    return flag;
}

/* A copy of end_move_step. */
int end_move_step2(Node *n, VECTOR *target, VECTOR *step, int speed)
{
    VECTOR d;

    d.vx = target->vx - n->trans.vx;
    d.vy = target->vy - n->trans.vy;
    d.vz = target->vz - n->trans.vz;
    VectorNormal(&d, step);
    if (d.vx >= -2 && d.vx < 2 && d.vy >= -2 && d.vy < 2 && d.vz >= -2 && d.vz < 2) {
        step->vx = d.vx << 12;
        step->vy = d.vy << 12;
        step->vz = d.vz << 12;
        return 1;
    }
    if (speed == 0x1000)
        return 0;
    step->vx = step->vx * speed / 4096;
    step->vy = step->vy * speed / 4096;
    step->vz = step->vz * speed / 4096;
    return 0;
}

void end_task_init(void)
{
    end_task_head.id = 0;
    end_task_head.state = 1;
    end_task_head.prio = 0xFF;
    end_task_head.prev = 0;
    end_task_head.next = &end_task_tail;
    end_task_tail.id = 1;
    end_task_tail.state = 1;
    end_task_tail.prio = 0;
    end_task_tail.prev = &end_task_head;
    end_task_tail.next = 0;
}

/* Links t in before at, as end_task_add does at either of its two places. */
#define END_TASK_INSERT()                                                                                              \
    {                                                                                                                  \
        t->id = id;                                                                                                    \
        t->func = func;                                                                                                \
        t->state = 2;                                                                                                  \
        t->prio = prio;                                                                                                \
        t->next = at;                                                                                                  \
        t->prev = at->prev;                                                                                            \
        at->prev = t;                                                                                                  \
        at = t->prev;                                                                                                  \
        at->next = t;                                                                                                  \
    }

/* Adds a task before the first one of lower priority. */
void end_task_add(Task *t, int id, unsigned char prio, void (*func)(Task *t))
{
    Task *at;

    at = &end_task_head;
    do {
        if (at->prio < prio) {
            END_TASK_INSERT();
            return;
        }
        at = at->next;
    } while (at->next);
    if (at->prio < prio)
        END_TASK_INSERT();
}

/* Runs the running tasks; the ones added this frame run from the next. */
void end_task_run(void)
{
    Task *t;

    for (t = end_task_head.next; t->next; t = t->next) {
        if (t->state == 4)
            t->func(t);
    }
    for (t = end_task_head.next; t->next; t = t->next) {
        if (t->state == 2)
            t->state = 4;
    }
}

void end_task_remove(Task *t)
{
    Task *prev, *next;

    prev = t->prev;
    next = t->next;
    prev->next = next;
    next->prev = prev;
}

void end_task_pause(Task *t)
{
    t->state = 8;
}

void end_task_resume(Task *t)
{
    t->state = 4;
}

void end_task_kill(Task *t)
{
    t->state = 0x10;
}

Task *end_task_find(short id)
{
    Task *t;

    for (t = end_task_head.next; t->next; t = t->next) {
        if (t->id == id)
            return t;
    }
    return 0;
}

int end_move_step(Node *n, VECTOR *target, VECTOR *step, int speed);

/* Moves a sprite towards its target, a step of its speed a frame. */
void end_sprite_move(Sprite *s)
{
    VECTOR step;

    if (!(s->flags & 0x10))
        return;
    if (end_move_step(&s->node, &s->target, &step, s->speed))
        s->flags ^= 0x10;
    s->pos.vx += step.vx;
    s->pos.vy += step.vy;
    s->pos.vz += step.vz;
    s->node.trans.vx = s->pos.vx / 4096;
    s->node.trans.vy = s->pos.vy / 4096;
    s->node.trans.vz = s->pos.vz / 4096;
}

void end_sprite_fade(Sprite *s)
{
    if (!(s->flags & 8))
        return;
    s->r += s->dr;
    s->g += s->dg;
    s->b += s->db;
    if (--s->fade_left)
        return;
    s->flags ^= 8;
    s->r = s->tr;
    s->g = s->tg;
    s->b = s->tb;
}

void end_sprite_animate(Sprite *s)
{
    unsigned short n;

    if (!(s->flags & 2))
        return;
    n = *(unsigned short *)s->anim;
    if (!s->frame_time) {
        s->frame++;
        if (s->frame >= n) {
            if (s->flags & 4)
                s->frame = 0;
            else
                s->frame--;
        }
        func_80036244(s->anim, s->frame);
        s->frame_time = D_8003623C->time;
    }
    s->frame_time--;
}

/* Draws a sprite's frame, a textured quad a piece, into ot from p; returns where the next primitive goes. */
void *end_sprite_draw(unsigned int *ot, int unused, POLY_FT4 *p, Sprite *s)
{
    SpriteFrame *frame;
    SpritePart *part;
    unsigned int n, i;
    unsigned short x, y, w, h;

    n = func_80036244(s->anim, s->frame);
    part = D_80036240;
    frame = D_8003623C;
    for (i = 0; i < n; p++, part++, i++) {
        x = part->x - frame->x + s->node.trans.vx;
        y = part->y - frame->y + s->node.trans.vy;
        w = part->w - 1;
        h = part->h - 1;
        p->r0 = s->r;
        p->g0 = s->g;
        p->b0 = s->b;
        p->x0 = x;
        p->y0 = y;
        p->x1 = x + w;
        p->y1 = y;
        p->x2 = x;
        p->y2 = y + h;
        p->x3 = x + w;
        p->y3 = y + h;
        p->u0 = part->u;
        p->v0 = part->v;
        p->u1 = part->u + w;
        p->v1 = part->v;
        p->u2 = part->u;
        p->v2 = part->v + h;
        p->u3 = part->u + w;
        p->v3 = part->v + h;
        p->tpage = part->tpage;
        p->clut = part->clut;
        SetPolyFT4(p);
        if (s->flags & 0x8000) {
            func_80046848(p, 1);
            p->tpage |= (s->flags & 0x6000) >> 8;
        }
        AddPrim(ot, p);
    }
    return p;
}

/* A step towards target in 20.12 fixed point: a unit vector times speed, or what's left when that's under 2. Returns
 * 1 when there. */
int end_move_step(Node *n, VECTOR *target, VECTOR *step, int speed)
{
    VECTOR d;
    int dx, dy, one;

    d.vx = dx = target->vx - n->trans.vx;
    d.vy = dy = target->vy - n->trans.vy;
    d.vz = target->vz - n->trans.vz;
    if (!dx) {
        one = dy < 0 ? -0x1000 : 0x1000;
        step->vx = 0;
        step->vy = one;
        step->vz = 0;
    } else if (!dy) {
        step->vx = dx < 0 ? -0x1000 : 0x1000;
        step->vy = 0;
        step->vz = 0;
    } else
        VectorNormal(&d, step);
    if (d.vx >= -2 && d.vx < 2 && d.vy >= -2 && d.vy < 2 && d.vz >= -2 && d.vz < 2) {
        step->vx = d.vx << 12;
        step->vy = d.vy << 12;
        step->vz = d.vz << 12;
        return 1;
    }
    if (speed == 0x1000)
        return 0;
    step->vx = step->vx * speed / 4096;
    step->vy = step->vy * speed / 4096;
    step->vz = step->vz * speed / 4096;
    return 0;
}
