/* MINI/CHOCOBO.BIN, the chocobo race: its main loop, 0x800A02D0-0x800A1630. The game links four objects: this,
 * chocobo_init.c, chocobo_race.c and chocobo_figure.c, each built its own way (see the Makefile). */

#include "chocobo.h"

/* Runs calls on a stack at the top of the scratchpad, as the game does for its heavier per-frame work. */
#ifdef NATIVE
#define SetSpadStack(addr)
#define ResetSpadStack()
#else
#define SetSpadStack(addr)                                                                                             \
    __asm__ volatile("move $8, %0\nsw $29, 0($8)\naddiu $8, $8, -4\nmove $29, $8" : : "r"(addr) : "$8", "memory")
#define ResetSpadStack() __asm__ volatile("addiu $29, $29, 4\nlw $29, 0($29)" : : : "memory")
#endif

extern short D_800F5068, D_800F5090, D_800F5094; /* VSync counts, kept for a debug display */
extern int D_800F5078;
extern Figure *D_800F50A0[]; /* the figures to draw */

void chocobo_draw_props(void);
void chocobo_draw_prop_range(int from, int to);
void chocobo_orient_racers(void);

/* end of declarations */

/* The race: set up, then run frames until the race is over and its fade out is done; then rank whoever has not
 * finished by how far along they are. */
#ifdef NON_MATCHING
/* Register allocation and scheduling all through (the loops unroll as the game's: the racer loops by 3, the sort by
 * 4). */
static const RECT D_800A0000 = {0, 0, 0, 0};
/* 16 bytes the race copies to its stack and never reads. */
typedef struct Words4 {
    int x[4];
} Words4;
static const Words4 D_800A0008 = {{0, 0x190, 0x1E8, 0}};

void chocobo_main(void)
{
    RECT unused0 = D_800A0000;
    RECT unused1 = D_800A0000;
    Words4 unused2 = D_800A0008;
    int ids[6], keys[6];
    Segment *seg;
    DrawBuf *buf;
    Racer *r;
    int i, j, n, t, place;
    int max_draw, max_vsync, pause, frames;

    pause = 0;
    chocobo_init();
    D_800F5074 = &D_800B7A68[0];
    DrawSync(0);
    chocobo_init_state();
    chocobo_init_racers();
    chocobo_camera_follow_at(0, 0, 0);
    chocobo_init_sound();
    snd_request.id = 0xC0;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    frames = 0;
    if (!D_8009D490)
        chocobo_bet();
    chocobo_orient_racers();
    D_800B7514 = 0xFF;
    D_800B753C = 0;
    D_800F5070 = -16;
    /* in a race with the player, the player's chocobo (figure 0) keeps its stamina */
    for (i = 0; i < 6; i++) {
        if (!D_800B7A48 || D_800B75CC[i].chocobo) {
            D_800B75CC[i].stamina <<= 1;
            D_800B75CC[i].stamina_max <<= 1;
        }
    }
    snd_request.id = 0x10;
    snd_request.arg[0] = (long)D_80079F64;
    snd_submit_request();
    max_vsync = 0;
    max_draw = 0;
    D_800B7538 = 0;
    VSync(2);
    for (;;) {
        if (D_800F5048 > 3000) {
            D_800F5070 = 16;
            snd_request.id = 0xC1;
            snd_request.arg[0] = 60;
            snd_request.arg[1] = 0;
            snd_submit_request();
        }
        D_800F5078 = 0;
        D_800B66CC[0] ^= 1;
        buf = &D_800B7A68[0];
        if (D_800F5074 == buf)
            buf = &D_800B7A68[1];
        D_800F5074 = buf;
        func_80044244(D_800F5074->ot, 0x1000);
        D_800B66D0 = D_800F5074->ot;
        SetSpadStack(0x1F8003FC);
        chocobo_update_camera();
        ResetSpadStack();
        seg = &D_800B7500[D_800B75CC[D_800B753C].seg];
        if (seg->flags & 1) {
            func_8004418C(D_800F5074->ot_ground, 4);
            SetSpadStack(0x1F8003FC);
            chocobo_draw_ground();
            ResetSpadStack();
        }
        if (seg->flags & 2) {
            D_800F5074->fade.r0 = seg->r;
            D_800F5074->fade.g0 = seg->g;
            D_800F5074->fade.b0 = seg->b;
            addPrim(&D_800F5074->ot[10], &D_800F5074->fade);
        }
        SetSpadStack(0x1F8003FC);
        chocobo_update_racers();
        chocobo_draw_course();
        ResetSpadStack();
        chocobo_draw_props();
        for (i = 0; i < 6; i++) {
            if (!D_800B7564[i])
                continue;
            r = &D_800B75CC[i];
            *(int *)0x1F800000 = 3;
            chocobo_pose_figure(&D_800B1254->figures[r->rider], &D_800B7544, r->anim, (short)r->anim_time >> 8);
            *(int *)0x1F800000 = 3;
            chocobo_pose_figure(&D_800B1254->figures[r->chocobo], &D_800B7544, r->anim, (short)r->anim_time >> 8);
        }
        for (i = 0; i < 6; i++) {
            if (!D_800B7564[i])
                continue;
            D_800F50A0[D_800F5078++] = &D_800B1254->figures[D_800B75CC[i].rider];
            D_800F50A0[D_800F5078++] = &D_800B1254->figures[D_800B75CC[i].chocobo];
        }
        for (i = 0; i < D_800F5078; i++)
            chocobo_draw_figure(D_800F50A0[i]);
        if (++D_800F5040 == 60) {
            D_800B7A64 = -1;
            for (i = 0; i < 6; i++) {
                if (D_800B75CC[i].quick_start)
                    D_800B75CC[i].boost = 60;
                else
                    D_800B75CC[i].boost = 10;
            }
        }
        D_800F5080 = D_800B7534;
        if ((D_800F5080 & 0x800) && D_800B7478) {
            D_800F5070 = 16;
            snd_request.id = 0xC1;
            snd_request.arg[0] = 60;
            snd_request.arg[1] = 0;
            snd_submit_request();
        }
        D_800B7534 = func_8001C808();
        if (D_800F5080 != D_800B7534 && D_800B7A48 && (D_800B7534 & 0x100))
            D_800B75CC[0].autopilot = !D_800B75CC[0].autopilot;
        D_800F5090 = VSync(1);
        DrawSync(0);
        D_800F5094 = VSync(1);
        if (max_draw < D_800F5090)
            max_draw = D_800F5090;
        if (max_vsync < D_800F5094)
            max_vsync = D_800F5094;
        if (pause) {
            if (D_800B7534 & 8) {
                pause = !pause;
                frames++;
            }
        } else if (D_800B7534 & 2) {
            pause = 1;
            frames++;
        }
        if (D_800B7A48 && frames > 10) {
            D_800B75CC[0].stamina += D_800B75CC[0].stamina_max / 150;
            if (D_800B75CC[0].stamina_max < D_800B75CC[0].stamina)
                D_800B75CC[0].stamina = D_800B75CC[0].stamina_max;
            frames = 0;
        }
        if (D_800B7514 >= 0x100 && D_800F5070 > 0) {
            D_800F5070 = 0;
            break;
        }
        D_800B7660--;
        VSync(2);
        D_800B7538++;
        D_800F5068 = VSync(1);
        PutDrawEnv(&D_800F5074->draw);
        func_800444AC(&D_800F5074->disp);
        ClearImage(&D_800F5074->draw.clip, seg->r, seg->g, seg->b);
        if (seg->flags & 1)
            func_8004433C(D_800F5074->ot_ground);
        SetSpadStack(0x1F8003FC);
        chocobo_draw_hud();
        ResetSpadStack();
        func_8004433C(&D_800F5074->ot[0xFFF]);
        chocobo_fade();
    }
    n = 0;
    place = D_800B75CC[0].place > 0 ? D_800B75CC[0].place : 0;
    for (i = 1; i < 6; i++) {
        if (place < D_800B75CC[i].place)
            place = D_800B75CC[i].place;
    }
    place++;
    for (i = 0; i < 6; i++) {
        if (!D_800B75CC[i].place) {
            ids[n] = i;
            keys[n] = D_800B75CC[i].seg;
            n++;
        }
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < n - 1; j++) {
            if (keys[j] < keys[i]) {
                t = keys[j];
                keys[j] = keys[i];
                keys[i] = t;
                t = ids[j];
                ids[j] = ids[i];
                ids[i] = t;
            }
        }
    }
    for (i = 0; i < n; i++)
        D_800B75CC[ids[i]].place = place++;
    snd_request.id = 0xE4;
    snd_request.arg[0] = 0x40;
    snd_submit_request();
    if (!D_8009D490)
        chocobo_results();
    D_8009D4A1 = D_800B75CC[0].place - 1;
}
#else
INCLUDE_ASM("asm/overlay/chocobo/nonmatchings/chocobo", chocobo_main);
#endif

/* Draws the props of the segments in view, the range wrapping round the end of the course. */
void chocobo_draw_props(void)
{
    int n = D_800F5084->nsegments;
    int from = D_800B7598;
    int to = D_800F507C;
    from = (from + n) % n;
    to = (to + n) % n;

    PushMatrix();
    if (to < from) {
        chocobo_draw_prop_range(0, to);
        chocobo_draw_prop_range(from, n);
    } else {
        chocobo_draw_prop_range(from, to);
    }
    PopMatrix();
}

/* The props of segments from..to-1 that project in front of the camera: place their figure, pose it at the prop's
 * frame, queue it, then step the frame (looping or holding at the end). */
void chocobo_draw_prop_range(int from, int to)
{
    int i, k, n, a, flag;
    Prop *props, *p;
    Figure *figures, *o, *o2, *o3;
    char *base;
    unsigned short *anim;

    for (i = from; i < to; i++) {
        if (!D_800B7500[i].prop)
            continue;
        props = D_800F5084->props;
        p = &props[D_800B7500[i].prop - 1];
        k = p->figure;
        gte_ldv0_r(p);
        gte_rtps();
        figures = D_800B1254->figures;
        o = &figures[k];
        a = o->anims;
        base = o->base;
        anim = (unsigned short *)(base + a);
        gte_stflg(&flag);
        if (flag < 0)
            continue;
        o->x = p->pos.vx;
        o->y = p->pos.vy;
        o->z = p->pos.vz;
        o->rx = p->rx;
        o->ry = p->ry;
        o->rz = p->rz;
        PushMatrix();
        *(int *)0x1F800000 = 3;
        chocobo_pose_figure((o2 = D_800B1254->figures, &o2[k]), &D_800B7544, 0, p->frame);
        D_800F50A0[D_800F5078++] = (o3 = D_800B1254->figures, &o3[k]);
        PopMatrix();
        p->frame += p->step;
        if (p->frame >= (n = *anim)) {
            if (p->pos.pad & 2)
                p->frame = 0;
            else
                p->frame = n - 1;
        }
        if (p->frame < 0)
            p->frame = 0;
    }
}

/* Stands every racer's rider and chocobo up the same way. */
void chocobo_orient_racers(void)
{
    int i;
    Racer *r;
    Figure *p, *v;

    for (i = 0; i < 6; i++) {
        r = &D_800B75CC[i];
        v = D_800B1254->figures;
        p = &v[r->chocobo];
        v[r->rider].rx = p->rx = 0xC0;
        D_800B1254->figures[r->rider].ry = p->ry = 0;
        D_800B1254->figures[r->rider].rz = p->rz = 0x80;
    }
}
