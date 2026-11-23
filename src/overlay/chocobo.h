/* MINI/CHOCOBO.BIN, the chocobo race: what its four objects (chocobo.c, chocobo_init.c, chocobo_race.c and
 * chocobo_figure.c) share. */

#ifndef CHOCOBO_H
#define CHOCOBO_H

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "gte.h"

int rand(void);
void srand(unsigned int seed);
int rsin(int a);
int rcos(int a);
int VectorNormal(VECTOR *v0, VECTOR *v1);
void PushMatrix(void);
void PopMatrix(void);
int VSync(int mode);
void PutDrawEnv(DRAWENV *env);
void func_80044000(RECT *r, void *p);        /* LoadImage */
void func_80044244(unsigned int *ot, int n); /* ClearOTagR */
void func_8004418C(unsigned int *ot, int n); /* ClearOTag */
void func_8004433C(unsigned int *ot);        /* DrawOTag */
void func_800444AC(DISPENV *env);            /* PutDispEnv */
void func_800442DC(void *p);                 /* DrawPrim */
void func_800269C0(void *buf);               /* the menu text's primitive buffer */
void func_80026A00(unsigned int *ot);        /* the menu text's ordering table */
unsigned int func_8001C808(void);            /* the pad's buttons: held in the low half, pressed in the high */

/* libgte's inline macros used by more than one object, as highway.c has them. */
#ifdef NATIVE
unsigned int gte_read_ctrl(int reg);
#define gte_ldv0_r(v) gte_ldv0(v)
#define gte_ldv3c(v)                                                                                                   \
    (gte_write(0, ((unsigned int *)(v))[0]), gte_write(1, ((unsigned int *)(v))[1]),                                   \
     gte_write(2, ((unsigned int *)(v))[2]), gte_write(3, ((unsigned int *)(v))[3]),                                   \
     gte_write(4, ((unsigned int *)(v))[4]), gte_write(5, ((unsigned int *)(v))[5]))
#define gte_rtps() gte_cmd(0x180001)
#define gte_rtpt() gte_cmd(0x280030)
#define gte_stflg(p) (*(int *)(p) = gte_read_ctrl(31))
#else
#define gte_ldv0_r(v) __asm__ volatile("lwc2 $0, 0(%0)\nlwc2 $1, 4(%0)" : : "r"(v))
#define gte_ldv3c(v)                                                                                                   \
    __asm__ volatile("lwc2 $0, 0(%0)\nlwc2 $1, 4(%0)\nlwc2 $2, 8(%0)\nlwc2 $3, 12(%0)\nlwc2 $4, 16(%0)\n"              \
                     "lwc2 $5, 20(%0)"                                                                                 \
                     :                                                                                                 \
                     : "r"(v))
#define gte_rtps() __asm__ volatile("nop\nnop\n.word 0x4A180001")
#define gte_rtpt() __asm__ volatile("nop\nnop\n.word 0x4A280030")
#define gte_stflg(p) __asm__ volatile("cfc2 $12, $31\nnop\nsw $12, 0(%0)" : : "r"(p) : "$12", "memory")
#endif

/* The figures: jointed models (the chocobos, their riders and the props), as HIGHWAY.BIN's. */

/* A figure from the figure file (0x24 bytes): whether it is drawn, its bones, parts and animations (at base + parts
 * and base + anims), where it stands, how it turns (256 to a turn) and its scale. */
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

/* The figure file's header. */
typedef struct FigureFile {
    unsigned char count;
    char x01[3];
    Figure *figures;
} FigureFile;

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

/* The course: the file at 0x80110000. */

/* A segment of the course (0x18 bytes): its two edges across the track, then what happens on it. */
typedef struct Segment {
    SVECTOR left, right;   /* left.pad: a course sound to start (1-based); right.pad: a prop to set off */
    signed char shot;      /* a camera shot to start (1-based) */
    unsigned char prop;    /* the prop standing on it (1-based) */
    unsigned char ahead;   /* how many segments on from it are drawn */
    signed char ground;    /* bits a racer's ground_mask picks to slow it; bit 7: cull back faces */
    unsigned char r, g, b; /* the background */
    unsigned char flags;   /* 1: draw the ground; 2: tint the screen with the background */
} Segment;

/* A shaded triangle of the scenery (0x1C bytes): the corners' pads hold the second and third colours. */
typedef struct CourseTri {
    SVECTOR v[3];
    unsigned int c0; /* and the code */
} CourseTri;

/* A shaded quad of the scenery (0x28 bytes). */
typedef struct CourseQuad {
    SVECTOR v[4];
    unsigned int c3;
    unsigned int c0; /* and the code */
} CourseQuad;

/* A billboard of the scenery (0x16 bytes): its foot (pad: the clut), texture page, width, height and corners' uvs. */
typedef struct CourseSprite {
    SVECTOR pos;
    unsigned short tpage;
    short size;
    short rise;
    unsigned short uv1, uv3, uv0, uv2;
} CourseSprite;

/* A shaded triangle of the ground (0x24 bytes): its corners and their colours (the first with the code). */
typedef struct GroundTri {
    SVECTOR v[3];
    unsigned int c0, c1, c2;
} GroundTri;

/* Per segment: the lane each of four lines keeps, and how much faster (/1024) it runs there. */
typedef struct CourseLine {
    unsigned char lane[4];
    signed char speed[4];
} CourseLine;

/* A camera shot (2-aligned: copied with lwl/lwr). */
typedef struct Shot {
    signed char kind; /* 0 or -1 follow, 1 fixed, 2 pan alongside, 3 orbit, 10 orbit the winner */
    signed char mode; /* kind 1: look at the watched racer; kind 2: its speed x0, /2, x1, x2 */
    short height;     /* kind 2's */
    SVECTOR eye;      /* kind 3: vx, vz and pad divisors and vy a height; pad: the first segment drawn or started at */
    SVECTOR at;       /* pad: the last segment drawn */
} Shot;

/* A prop along the course (0x10 bytes): where it stands, how it animates and which figure draws it. */
typedef struct Prop {
    SVECTOR pos; /* pad: 1 lit from the start, 2 loops its animation */
    short step, frame;
    unsigned char rx, ry, rz;
    unsigned char figure;
} Prop;

/* A sound the course starts at a segment: snd_request's id and first six arguments. */
typedef struct CourseSound {
    unsigned short id;
    unsigned short arg[6];
    short pad;
} CourseSound;

typedef struct Course {
    int nsegments;
    volatile int spurt; /* the segment the racers spurt from (reloaded on every read) */
    Segment *segments;
    int x0C;
    CourseQuad *quads;
    int x14;
    CourseTri *tris;
    short *seg_first; /* each segment's first command */
    int *cmds;        /* kind << 16 | index; a negative one starts the next segment, -1 ends */
    short nground;
    short pad26;
    GroundTri *ground;
    int x2C;
    CourseSprite *sprites;
    CourseLine *lines;
    Shot *shots;
    unsigned int nprops;
    Prop *props;
    CourseSound *sounds;
} Course;

/* A racer (0xA4 bytes, ints in it: 4-aligned, so a copy is lw/sw). Six race; [6] is the camera's record, a copy of the
 * racer it follows. */
typedef struct Racer {
    short seg, next; /* the segment it is on and the next */
    short speed;
    short ai; /* how it races, 0-3 (and its tint) */
    short x08;
    short y; /* its height, eased */
    int x0C;
    SVECTOR pos;
    VECTOR frac;   /* x and z in 1/4096, carried into pos */
    SVECTOR ahead; /* where it runs to: on its lane, at the next segment */
    SVECTOR rot;   /* vy: its heading */
    SVECTOR aim;   /* vy: the heading it turns to */
    SVECTOR goal;  /* where it runs to past the finish: the start line */
    short boost;   /* frames of dash left */
    short lane_avg;
    short trait; /* 1: slower when not leading, 2: dashes on a stumble */
    short pace;  /* the speed it aims at */
    short x50;
    short line; /* which of the segment's four lines it keeps to */
    short cruise;
    short run, sprint; /* its top speeds running and dashing */
    short accel;       /* percent added to pace */
    short x5C;
    short x5E; /* percent of pace - cruise taken each segment */
    short autopilot;
    short skill;
    short x64;
    short steer; /* how far it moves across a segment */
    int stamina, stamina_max;
    short place;     /* where it finished, 0 while running */
    short anim_time; /* the animation's frame, 8.8 */
    short x74;
    short anim_speed;
    short anim_last; /* the animation playing */
    short x7A;
    short lane;     /* across the course: 0 the right edge, 256 the left */
    short finished; /* -1 past the line */
    short anim;     /* the animation it wants: 0 run, 1 dash, 5 tired */
    short rank;     /* its place while running */
    short stuck;    /* frames without passing a segment */
    unsigned short ground_mask;
    unsigned char name[8];
    short rider, chocobo; /* figures (the chocobo's index is also its colour) */
    short bump;           /* the bump sound's timer */
    short spurt_lane;
    short spurt; /* the segment it spurts from */
    short spurt_dash;
    short x9C, x9E;
    short quick_start;
    short steady; /* cruise at its run speed */
} Racer;

/* The display buffers. */

/* A slot of a buffer's primitive pool. */
typedef union Prim {
    POLY_G3 g3;
    POLY_G4 g4;
    POLY_FT3 ft3;
    POLY_FT4 ft4;
    SPRT sprt;
    SPRT_16 sprt16;
} Prim;

/* One of the two display buffers (0x1EAE0 bytes). */
typedef struct DrawBuf {
    unsigned int ot[0x1004]; /* 0x1000 for the scene, cleared in reverse; [0x1000] the menu text's */
    Prim prims[2500];        /* taken in turn, D_800F5034 the next */
    POLY_F4 markers[6];      /* the racers on the course map */
    POLY_FT4 map;
    char x1C768[0x18];
    POLY_F4 stamina_bar;
    char x1C798[0x28];
    POLY_FT4 stamina_frame;
    unsigned int ot_ground[4]; /* drawn before the scene */
    POLY_G3 ground[249];
    char x1E334[0x1C];
    POLY_F4 fade; /* the whole screen */
    POLY_FT4 cards[5][3];
    POLY_FT4 card_shadows[5][3];
    POLY_FT4 card_items[5][3];
    DRAWENV draw;
    DISPENV disp;
} DrawBuf;

extern unsigned char D_8009D490, D_8009D49E, D_8009D4A1; /* the race's game variables */
extern unsigned char D_80079F64[], D_8007AF64[], D_8007BF64[], D_8007C764[]; /* the exe's sound buffers */
extern unsigned char *D_800B1250; /* a tint for chocobo_tint_figure: x, y, z as little-endian shorts, then a flag */
extern FigureFile *D_800B1254;
extern unsigned char D_800B66CC[]; /* which of the parts' two primitive buffers is built this frame */
extern unsigned int *D_800B66D0;   /* the ordering table the figures go in */
extern int D_800B7478;  /* someone has finished second */
extern int D_800B747C;  /* the gated prizes are allowed */
extern int D_800B74FC;  /* the course's segments */
extern Segment *D_800B7500;
extern int D_800B7514;  /* the fade: 0 (none) to 0x100 */
extern unsigned int D_800B7534; /* the pad this frame */
extern int D_800B7538;  /* frames */
extern int D_800B753C;  /* the racer the camera watches */
extern MATRIX D_800B7544; /* the view */
extern int D_800B7564[6]; /* the racers on screen */
extern int D_800B7594;  /* the winner */
extern int D_800B7598, D_800F507C; /* the first and last segments drawn */
extern int D_800B759C;  /* set: the camera jumps to its target */
extern Racer D_800B75CC[7];
extern short D_800B7660;
extern int D_800B7A48;  /* the player races (-1); 0 a demo */
extern int D_800B7A64;  /* the race has started */
extern DrawBuf D_800B7A68[2];
extern int D_800F5040;  /* frames since the start */
extern int D_800F5044;
extern int D_800F5048;
extern int D_800F5050;  /* divides a racer's speed into its animation's */
extern Shot D_800F5054; /* the shot running */
extern int D_800F5070;  /* the fade's step */
extern DrawBuf *D_800F5074; /* the buffer being built */
extern unsigned int D_800F5080; /* the pad last frame */
extern Course *D_800F5084;
extern int D_800F5088;  /* the racer the camera copied */
extern int D_800F508C;
extern int D_800F5098;  /* the race's rank */
extern int D_800F5124;  /* the follow camera's preset */

void chocobo_init_state(void);
void chocobo_init_sound(void);
void chocobo_init(void);
void chocobo_draw_ground(void);
void chocobo_draw_course(void);
void chocobo_init_racers(void);
void chocobo_update_racers(void);
void chocobo_camera_follow_at(int i, int speed, int seg);
void chocobo_update_camera(void);
void chocobo_draw_hud(void);
void chocobo_load_cluts(void);
void chocobo_bet(void);
void chocobo_results(void);
void chocobo_fade(void);
char *chocobo_setup_figure_prims(Figure *m, char *work, int a2);
void chocobo_draw_figure(Figure *m);
void chocobo_pose_figure(Figure *m, MATRIX *root, int anim, int frame);
int chocobo_tint_figure(Figure *m, unsigned char *b);
int chocobo_set_figure_semitrans(Figure *m, unsigned char *b);

#endif
