/* MINI/JET.BIN, 0x800A0450-0x800A8310 (its data follows, then hand-written GTE routines to 0x800A8918). GCC 2.6.3,
 * ASPSX 2.21 with its divide checks, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "gte.h"

/* libgpu's primitives that only JET.BIN uses (layouts from Sony's Libref). */
typedef struct POLY_F3 {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0, x1, y1, x2, y2;
} POLY_F3;

typedef struct POLY_F4 {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0, x1, y1, x2, y2, x3, y3;
} POLY_F4;

typedef struct POLY_G3 {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    unsigned char r1, g1, b1, pad1;
    short x1, y1;
    unsigned char r2, g2, b2, pad2;
    short x2, y2;
} POLY_G3;

typedef struct POLY_FT3 {
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
} POLY_FT3;

typedef struct POLY_GT3 {
    unsigned int tag;
    unsigned char r0, g0, b0, code;
    short x0, y0;
    unsigned char u0, v0;
    unsigned short clut;
    unsigned char r1, g1, b1, p1;
    short x1, y1;
    unsigned char u1, v1;
    unsigned short tpage;
    unsigned char r2, g2, b2, p2;
    short x2, y2;
    unsigned char u2, v2;
    unsigned short pad2;
} POLY_GT3;

/* A frame's primitives, one array of each kind, and the next free one in each. */
typedef struct JetPrims {
    void *next[9];
    POLY_F3 f3[1];
    POLY_F4 f4[1];
    POLY_G3 g3[1600]; /* the track's faces */
    POLY_G4 g4[30];
    POLY_FT3 ft3[1];
    POLY_FT4 ft4[300]; /* sprites and the HUD */
    POLY_GT3 gt3[1];
    POLY_GT4 gt4[1];
    LINE_F2 lf2[1];
} JetPrims;

/* A frame's buffer (two, drawn and shown in turn). */
typedef struct JetBuffer {
    DRAWENV draw;
    DISPENV disp;
    unsigned int ot[0x1000];   /* 0x70 the 3D scene */
    unsigned char pad4070[0x28];
    unsigned int hud_ot[0xB4]; /* 0x4098 */
    JetPrims prims;            /* 0x4368 */
} JetBuffer;

/* A part of the track's scenery, made of shaded triangles and of strips (0x20 bytes). */
typedef struct Strip {
    unsigned char data[40];
} Strip;

typedef struct Model {
    short total;
    short x2;
    short count; /* faces */
    short strip_count;
    short x8;
    short padA;
    struct Face *faces;
    Strip *strips;
    short max_x, min_x, max_z, min_z; /* 0x14 */
    unsigned char pad1C[4];
} Model;

/* A node of the scene: its matrix is its parent's times its own. Nodes are kept in lists by depth. */
typedef struct Obj {
    Model *model;
    MATRIX m;
    struct Obj *parent; /* 0x24 */
    short id;
    short slot;
    short level;
    short pad2E;
    struct Obj *next, *prev; /* 0x30 */
} Obj;

/* What the hand-written drawers read. */
typedef struct DrawArgs {
    void *faces;
    void *prims;
    unsigned int *ot;
    Model *model;
} DrawArgs;

/* A shaded triangle as the hand-written drawer reads it. */
typedef struct Face {
    SVECTOR v[3];
    unsigned int c[3];
} Face;

/* A list of what the track streams in and out: each entry's neighbours, 0xFFFF for none. */
typedef struct Link {
    unsigned short prev;
    unsigned short next;
} Link;


/* libgpu's TIM description (layout from Sony's Libref). */
typedef struct TIM_IMAGE {
    unsigned int mode;
    RECT *crect;
    unsigned int *caddr;
    RECT *prect;
    unsigned int *paddr;
} TIM_IMAGE;

/* A file read by sector: its first sector and its size. */
typedef struct JetFile {
    unsigned int lba;
    unsigned int size;
} JetFile;

/* What a target does: its kind picks its behaviour in jet_update_targets, its parameters come from the track file
 * (0xA0 bytes). Some fields mean different things for different kinds. */
typedef struct JetInfo {
    int kind;
    int hit;
    int model;
    int active; /* 0x0C cleared to remove it; some kinds count it down */
    int fresh;  /* 0x10 set by the spawner: the kind sets itself up */
    int time;
    int anim;  /* 0x18 which path of the track file's it follows */
    int speed; /* 0x1C along the path, in 16.16 */
    int x20, x24;
    int frame; /* 0x28 */
    int a, b, c;
    int x38[6];
    int params[20]; /* 0x50 by enum JetParam */
} JetInfo;

/* A target of the ride, as the stream places it: a copy of a template the track file holds (0x13C bytes). */
typedef struct Target {
    VECTOR pos;
    unsigned char pad10[8];
    SVECTOR rot; /* 0x18 */
    unsigned char pad20[8];
    JetInfo info; /* 0x28 */
    int count;    /* 0xC8 the path's keys */
    SVECTOR *keys;
    unsigned char padD0[4];
    Obj *obj; /* 0xD4 */
    short slot;
    short state;
    SVECTOR box[8];      /* 0xDC the centres of its bounding box's faces */
    unsigned int sxy[8]; /* 0x11C */
} Target;

/* Where the track file places a target (0x60 bytes). */
typedef struct JetPlace {
    short kind;
    short pad2;
    short model;
    short pad6;
    int anim;
    int speed;
    int params[20];
} JetPlace;

/* A target's parameters. */
enum JetParam {
    P_SCORE = 0,
    P_TYPE = 10, /* how it is hit: 1-5 */
    P_SPIN = 11,
    P_X12 = 12,
    P_LIFE = 13,
    P_PAN = 18 /* where its sound plays */
};

/* A model's bounding box. */
typedef struct JetBox {
    short faces;
    short strips;
    SVECTOR min, max;
} JetBox;

extern int jet_vol_a2;
extern int jet_vol_a3;
extern VECTOR jet_cam_pos;
extern VECTOR jet_left_plane, jet_right_plane; /* the view's left and right planes */
extern int jet_plane_l_side;
extern unsigned short jet_gun_lx;
extern unsigned short jet_gun_lx2;
extern unsigned short jet_gun_ly;
extern int jet_plane_r_side;
extern unsigned short jet_gun_ly2;
extern unsigned short jet_gun_rx;
extern unsigned short jet_gun_rx2;
extern unsigned short jet_gun_ry;
extern unsigned short jet_gun_ry2;
extern int jet_fog_near;
extern int jet_fog_far;
extern unsigned short jet_light_head;
extern int jet_plane_l_d;
extern unsigned short jet_face_head;
extern int jet_plane_r_d;
extern Obj *jet_bonus_obj;
extern SVECTOR jet_bonus_rot;
extern unsigned char jet_bonus_blink;
extern short jet_bonus_time;
extern short jet_bonus_score;
extern short jet_heat;
extern unsigned int jet_stream_t, jet_stream_prev;
extern unsigned short *jet_face_adds, *jet_face_drops, *jet_light_adds, *jet_light_drops;
extern unsigned char jet_stream_first;
extern unsigned int jet_stream_segments;
extern Target jet_targets[100];
extern int jet_placed_segment, jet_placed, jet_path_count;
extern JetBox *jet_boxes;
extern int jet_plane_l_len, jet_plane_r_len;
extern int jet_model_used;
extern Model jet_model_pool[];
extern Strip *jet_strip_pool;
extern int jet_strip_used;
extern Target jet_template;
extern void *jet_path_keys;
extern unsigned short jet_face_tail, jet_light_tail;
extern int jet_pad_dir, D_800A8A7C, jet_start_held;
extern unsigned char jet_fire_wait, jet_laser_frame;
extern VECTOR jet_debug_offset;
extern VECTOR D_800A83C8, jet_track_pos;
extern SVECTOR jet_cam_tilt;
extern MATRIX jet_tilt;
extern int *jet_part_offsets, *jet_part_size;
extern unsigned char jet_aim_mode;
extern int jet_gun_sound;
extern JetFile jet_files[4];
extern unsigned int *jet_tims[];
extern void *jet_parts[17]; /* the track file's parts, as its table gives them */
extern unsigned short jet_fade_tpage, jet_fade_clut;
extern int jet_x8A84;
extern void *D_80110BB8, *D_800D16D4, *D_800D1A38, *D_800D1A3C, *D_800A891C, *D_800A8920;
extern unsigned int jet_track_t; /* 16.16 along the track */
extern unsigned char jet_hit_side;
extern int jet_vol_engine, jet_vol_x8334;
extern unsigned short jet_tpages[12], jet_cluts[12];
extern short jet_bonus_model;
extern int jet_score;
extern unsigned char jet_paused;
extern int jet_segment;
extern Obj jet_root;
extern int jet_last_segment;
extern unsigned char jet_firing;
extern SVECTOR *jet_rail_l;
extern unsigned short jet_faces_shown, jet_lights_shown;
extern DR_MODE jet_draw_mode;
extern Link jet_light_links[9000];
extern short jet_aim_x;
extern short jet_aim_y;
extern unsigned char jet_visible;
extern volatile int jet_vblanks;
extern unsigned char jet_done;
extern Link jet_face_links[12000];
extern SVECTOR *jet_rail_r;
extern SVECTOR *jet_rail_rot;
extern int jet_speed;
extern JetBuffer *jet_buf;
extern void *jet_part;
extern short jet_target_head;
extern short jet_node_head;
extern int jet_face_used;
extern short jet_target_count;
extern MATRIX *jet_view;
extern MATRIX *jet_work;
extern JetBuffer jet_bufs[2];
extern MATRIX jet_cam;
extern Obj jet_nodes[200];
extern Obj jet_level_heads[10];
extern Obj jet_level_tails[10];
extern Model *jet_models[];
extern short jet_target_next[];
extern short jet_node_next[200];
extern Face *jet_face_pool;

int VSync(int mode);
void func_80043938(int mode); /* ResetGraph */
void func_80044244(unsigned int *ot, int n); /* ClearOTagR */
void func_80044A68(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw); /* SetDrawMode */
void SetFogNearFar(int a, int b, int h);
int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
void OpenTIM(unsigned int *tim);
TIM_IMAGE *ReadTIM(TIM_IMAGE *t);
void func_80044000(RECT *r, void *p); /* LoadImage */
int rsin(int a);
void PutDrawEnv(DRAWENV *env);
void func_8004433C(unsigned int *ot); /* DrawOTag */
void func_800444AC(DISPENV *env); /* PutDispEnv */
unsigned int func_8001C808(int pad); /* the pad's buttons (the exe's takes none; the module passes 1) */
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* SetDefDrawEnv */
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
void SetDispMask(int mask);
void SetGeomOffset(int x, int y);
void SetGeomScreen(int h);
void SetBackColor(int r, int g, int b);
void SetFarColor(int r, int g, int b);
void SetPolyF3(void *p);
void SetPolyF4(void *p);
void SetPolyG3(void *p);
void SetPolyFT3(void *p);
void SetPolyGT3(void *p);
void SetPolyGT4(void *p);
int SquareRoot0(int a);
int rand(void);
int ratan2(int y, int x);
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixZYX(SVECTOR *r, MATRIX *m);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);
int VectorNormal(VECTOR *v0, VECTOR *v1);

/* The hand-written routines after the data. The drawers hand back the next free primitive as a 24-bit address
 * (KUSEG, which the PS1 maps onto RAM); the PC's C reaches RAM only at 0x80000000. */
#ifdef NATIVE
#define KSEG0(p) ((void *)((unsigned int)(p) | 0x80000000))
#else
#define KSEG0(p) (p)
#endif
void jet_project9(SVECTOR *v, unsigned int *sxy);
void jet_project3(SVECTOR *v, unsigned int *sxy);
void *jet_draw_model(DrawArgs *a);
void *jet_draw_model_far(DrawArgs *a);
void *jet_draw_face(Face *f, void *prims, unsigned int *ot, Face *same);
void *jet_draw_light(SVECTOR *v, void *prims, unsigned int *ot, SVECTOR *w);

void jet_draw_target(JetBuffer *buf, Obj *o, short z, int unused, Target *t);
void jet_draw_guns(JetBuffer *buf, Obj *o, short z, int unused, Target *t);
void jet_draw_icon(JetBuffer *buf, Obj *o, short z, int unused, Target *t);
void jet_draw_faces(void);
void jet_draw_lights(void);
void jet_set_view(void);
void jet_draw_heat(void);
void jet_draw_bonus(JetBuffer *buf, short a, int b, int c, int d);
void jet_draw_number(int value, short x, short y, short zeros, unsigned short v);
void jet_draw_sprite(short id, int x, int y, int w, int h, int u, int v, int tw, int th, unsigned char semi);
void jet_set_tpage(void);
void jet_init(void);
void jet_load(void);
void jet_load_tims(void);
void jet_upload_tim(unsigned int *image);
void jet_init_stream(void);
void jet_init_targets(void);
void jet_init_planes(void);
void jet_init_models(void);
Model *jet_make_model(int id);
void jet_init_gfx(void);
void jet_init_nodes(void);
Model *jet_alloc_model(void);
Face *jet_alloc_faces(int n);
Strip *jet_alloc_strips(int n);
void jet_init_prims(JetPrims *p);
void jet_start_sound(void);
void jet_stop_sound(void);
void jet_sound_hit(short pan);
void jet_sound_gun(unsigned char on);
void jet_set_volumes(void);
void jet_init_track(void);
void jet_move_camera(void);
void jet_select_part(unsigned char part, int unused);
void jet_read_pad(void);
void jet_stream(int step);
void jet_stream_in(int step);
void jet_add_face(unsigned short id);
void jet_drop_face(unsigned short id);
void jet_add_light(unsigned short id);
void jet_drop_light(unsigned short id);
void jet_stream_out(int step);
short jet_alloc_target(void);
short jet_spawn(Target *tmpl, short parent);
void jet_place(short x, short y, short z, short kind, short model);
void jet_free_target(short i);
void jet_update_targets(JetBuffer *buf);
void jet_hit(Target *t);
void jet_despawn(Target *t);
void jet_select_path(unsigned char i, unsigned char set);
void jet_path_point(unsigned int t, SVECTOR *keys, VECTOR *out, unsigned char flip);
void jet_draw_aim(JetBuffer *buf);
void jet_draw_lasers(void);
void jet_place_targets(void);
void jet_place_still(short x, short y, short z, short kind, short model);
void jet_destroy(Target *t);
int jet_in_view(int *v);
void jet_reset_prims(JetPrims *p);
void jet_node_init(Obj *o, short slot);
Obj *jet_node_new(short id, int a1, int a2, int a3, Obj *parent, int x, int y, int z, short rx, short ry, short rz);
void jet_node_free(Obj *o);
short jet_alloc_node(void);
void jet_free_node(short i);
void jet_node_link(Obj *o, Obj *parent);
void jet_node_unlink(Obj *o);

/* Copies a node's matrix to the work matrix. */
#define JET_COPY_MATRIX(d, s)                                                                                          \
    {                                                                                                                  \
        (d)->m[0][0] = (s)->m[0][0];                                                                                   \
        (d)->m[0][1] = (s)->m[0][1];                                                                                   \
        (d)->m[0][2] = (s)->m[0][2];                                                                                   \
        (d)->m[1][0] = (s)->m[1][0];                                                                                   \
        (d)->m[1][1] = (s)->m[1][1];                                                                                   \
        (d)->m[1][2] = (s)->m[1][2];                                                                                   \
        (d)->m[2][0] = (s)->m[2][0];                                                                                   \
        (d)->m[2][1] = (s)->m[2][1];                                                                                   \
        (d)->m[2][2] = (s)->m[2][2];                                                                                   \
        (d)->t[0] = (s)->t[0];                                                                                         \
        (d)->t[1] = (s)->t[1];                                                                                         \
        (d)->t[2] = (s)->t[2];                                                                                         \
    }

/* The node's matrix in the camera's view: its parent's, then the camera's. */
#define JET_VIEW_MATRIX(o)                                                                                             \
    {                                                                                                                  \
        MATRIX *m = jet_work;                                                                                     \
        MATRIX *w, *cam;                                                                                               \
                                                                                                                       \
        JET_COPY_MATRIX(m, &(o)->m);                                                                                   \
        if ((o)->parent != &jet_root) {                                                                              \
            CompMatrix(&(o)->parent->m, m, m);                                                                         \
        }                                                                                                              \
        w = jet_work;                                                                                             \
        w->t[0] -= jet_cam_pos.vx;                                                                                      \
        w->t[1] -= jet_cam_pos.vy;                                                                                      \
        w->t[2] -= jet_cam_pos.vz;                                                                                      \
        cam = &jet_cam;                                                                                             \
        gte_CompMatrix(cam, jet_work, jet_work);                                                             \
        gte_SetRotMatrix(jet_work);                                                                               \
        gte_SetTransMatrix(jet_work);                                                                             \
    }

unsigned short jet_main(void)
{
    jet_init();
    func_80044A68(&jet_draw_mode, 0, 1, GetTPage(1, 1, 0x300, 0), 0);
    jet_rail_rot = jet_parts[7];
    jet_select_part(0, 0);
    jet_rail_l = jet_part;
    jet_select_part(1, 0);
    jet_rail_r = jet_part;
    jet_start_sound();
    SetFogNearFar(jet_fog_near, jet_fog_far, 0x100);
    jet_bonus_obj = jet_node_new(0x1E, 0, 0, 1, &jet_root, 0x4B0, 0x32, 0xBB8, 0, 0x3E8, 0);
    for (;;) {
        RECT unused;

        if (jet_segment * 4 > jet_last_segment - 0x10 || jet_done == 1) {
            break;
        }
        jet_read_pad();
        if (!jet_paused) {
            jet_move_camera();
            jet_stream_in(jet_speed);
            jet_set_view();
            jet_draw_lights();
            jet_draw_faces();
            jet_draw_bonus(jet_buf, jet_bonus_model, 5, 0x28, 0);
            jet_stream_out(jet_speed);
            jet_update_targets(jet_buf);
            jet_draw_number(jet_score, 0xF4, 0xC8, 0, 0);
            jet_draw_sprite(7, 0xCC, 0xC8, 0x27, 0x11, 0, 0, 0x27, 0x11, 0);
            jet_draw_sprite(0xB, 0x12, 0x56, 0xC, 0x8C, 0, 0x70, 0xC, 0x8C, 0);
            jet_draw_heat();
            if (jet_speed < 0x4000) {
                jet_vol_a2 = 0;
            } else {
                jet_vol_a2 = 0x7F;
            }
        } else {
            jet_draw_sprite(9, 0xCA, 0xC0, 0x60, 0x20, 0, 0x50, 0x60, 0x20, 0);
            jet_vol_a2 = 0;
            jet_vol_a3 = 0;
        }
        jet_set_volumes();
        jet_set_tpage();
        jet_draw_sprite(0xA, 0xC8, 0xC0, 0x6F, 0x1F, 0, 0x30, 0x70, 0x20, 0);
        DrawSync(0);
        VSync(0);
        func_80043938(1);
        PutDrawEnv(&jet_buf->draw);
        func_800444AC(&jet_buf->disp);
        ClearImage(&jet_buf->draw.clip, 0, 0, 0);
        if (jet_visible) {
            func_8004433C(&jet_buf->ot[0xFFF]);
            func_8004433C(&jet_buf->hud_ot[0xB3]);
        }
        jet_vblanks = 0;
        jet_buf = jet_buf == &jet_bufs[0] ? &jet_bufs[1] : &jet_bufs[0];
        func_80044244(jet_buf->ot, 0x1000);
        func_80044244(jet_buf->hud_ot, 0xB4);
        jet_reset_prims(&jet_buf->prims);
    }
    snd_request.id = 0xB8;
    snd_request.arg[0] = 0;
    snd_submit_request();
    return jet_score;
}

#ifdef NON_MATCHING
/* Same length: GCC narrows the loads of each point's x to halfwords; the game loads the words. */
void jet_draw_target(JetBuffer *buf, Obj *o, short z, int unused, Target *t)
{
    DrawArgs args;
    short x[8], y[8];
    short min_x, max_x, min_y, max_y, i;

    JET_VIEW_MATRIX(o);
    args.faces = o->model->faces;
    args.prims = buf->prims.next[2];
    args.ot = &buf->ot[z];
    args.model = o->model;
    buf->prims.next[2] = KSEG0(jet_draw_model(&args));
    jet_project9(t->box, t->sxy);
    y[0] = (t->sxy[0] & 0xFFFF0000) >> 16;
    min_y = y[0];
    max_y = min_y;
    x[0] = t->sxy[0];
    min_x = x[0];
    max_x = min_x;
    for (i = 1; i < 6; i++) {
        y[i] = (t->sxy[i] & 0xFFFF0000) >> 16;
        x[i] = t->sxy[i];
        if (x[i] < min_x) {
            min_x = x[i];
        }
        if (max_x < x[i]) {
            max_x = x[i];
        }
        if (y[i] < min_y) {
            min_y = y[i];
        }
        if (max_y < y[i]) {
            max_y = y[i];
        }
    }
    if (jet_in_view(jet_work->t)) {
        t->info.hit = 0;
        if (jet_aim_x < max_x && min_x < jet_aim_x && jet_aim_y < max_y && min_y < jet_aim_y && jet_firing == 1) {
            t->info.hit = 1;
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_draw_target);
#endif

#ifdef NON_MATCHING
/* Register choice: the last SetRotMatrix's load goes to t1 here, a3 in the game. */
void jet_draw_guns(JetBuffer *buf, Obj *o, short z, int unused, Target *t)
{
    DrawArgs args;
    short x[8], y[8];
    unsigned int sxy[12];

    JET_VIEW_MATRIX(o);
    args.faces = o->model->faces;
    args.prims = buf->prims.next[2];
    args.ot = &buf->hud_ot[z];
    args.model = o->model;
    buf->prims.next[2] = KSEG0(jet_draw_model(&args));
    jet_project3((SVECTOR *)jet_models[79]->faces, sxy);
    jet_gun_lx = sxy[1];
    jet_gun_ly = sxy[1] >> 16;
    jet_gun_lx2 = sxy[2];
    jet_gun_ly2 = sxy[2] >> 16;
    jet_project3((SVECTOR *)((char *)jet_models[79]->faces + 0x24), sxy);
    jet_gun_rx = sxy[1];
    jet_gun_ry = sxy[1] >> 16;
    jet_gun_rx2 = sxy[2];
    jet_gun_ry2 = sxy[2] >> 16;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_draw_guns);
#endif

void jet_draw_icon(JetBuffer *buf, Obj *o, short z, int unused, Target *t)
{
    DrawArgs args;
    MATRIX *m;

    m = jet_work;
    JET_COPY_MATRIX(m, &o->m);
    gte_SetRotMatrix(jet_work);
    gte_SetTransMatrix(jet_work);
    args.faces = o->model->faces;
    args.prims = buf->prims.next[2];
    args.ot = &buf->hud_ot[z];
    args.model = o->model;
    buf->prims.next[2] = KSEG0(jet_draw_model_far(&args));
}

void jet_draw_faces(void)
{
    void *p = jet_buf->prims.next[2];
    Face *faces = jet_parts[10];
    Link *links;
    unsigned short i;

    if (jet_faces_shown) {
        i = jet_face_head;
        links = jet_face_links;
        do {
            p = KSEG0(jet_draw_face(&faces[i], p, jet_buf->ot, &faces[i]));
            i = links[i].next;
        } while (i != 0xFFFF);
    }
    jet_buf->prims.next[2] = p;
}

void jet_draw_lights(void)
{
    void *p = jet_buf->prims.next[5];
    unsigned short i = jet_light_head;
    SVECTOR *a, *b;
    Link *links;

    do {
        a = jet_rail_l;
        b = jet_rail_r;
        p = KSEG0(jet_draw_light(&a[i], p, jet_buf->ot, &b[i]));
        links = jet_light_links;
        i = links[i].next;
    } while (i != 0xFFFF);
    jet_buf->prims.next[5] = p;
}

void jet_set_view(void)
{
    MATRIX *m = jet_view;
    MATRIX *cam = &jet_cam;

    m->t[0] = -jet_cam_pos.vx;
    m->t[1] = -jet_cam_pos.vy;
    m->t[2] = -jet_cam_pos.vz;
    gte_CompMatrix(cam, jet_view, jet_work);
    gte_SetRotMatrix(jet_work);
    gte_SetTransMatrix(jet_work);
}

void jet_track_point(unsigned int t, int offset, VECTOR *pos, SVECTOR *rot)
{
    VECTOR pa, pb, mid, mid1, mid0, across, along, cross, n, da, db;
    SVECTOR *rots, *rails_a, *rails_b, *r0, *r1, *a0, *a1, *b0, *b1;
    int i = t >> 16;
    int frac = t & 0xFFFF;
    int dx, dy, dz, x;

    rots = jet_rail_rot;
    r0 = &rots[i];
    r1 = &rots[i + 1];
    dx = r1->vx - r0->vx;
    dy = r0->vy - r1->vy;
    dz = r1->vz - r0->vz;
    if (dx > 0x800) {
        dx -= 0x1000;
    }
    if (dy > 0x800) {
        dy -= 0x1000;
    }
    if (dz > 0x800) {
        dz -= 0x1000;
    }
    if (dx < -0x800) {
        dx += 0x1000;
    }
    if (dy < -0x800) {
        dy += 0x1000;
    }
    if (dz < -0x800) {
        dz += 0x1000;
    }
    dx *= frac;
    dy *= frac;
    dz *= frac;
    rails_a = jet_rail_l;
    a0 = &rails_a[i];
    a1 = &rails_a[i + 1];
    da.vx = (a1->vx - a0->vx) * frac;
    da.vy = (a1->vy - a0->vy) * frac;
    da.vz = (a1->vz - a0->vz) * frac;
    rails_b = jet_rail_r;
    b0 = &rails_b[i];
    b1 = &rails_b[i + 1];
    pa.vx = a0->vx + (da.vx >> 16);
    pa.vy = a0->vy + (da.vy >> 16);
    pa.vz = a0->vz + (da.vz >> 16);
    db.vx = (b1->vx - b0->vx) * frac;
    db.vy = (b1->vy - b0->vy) * frac;
    db.vz = (b1->vz - b0->vz) * frac;
    pb.vx = b0->vx + (db.vx >> 16);
    pb.vy = b0->vy + (db.vy >> 16);
    pb.vz = b0->vz + (db.vz >> 16);
    mid.vx = (pb.vx + pa.vx) >> 1;
    mid.vy = (pb.vy + pa.vy) >> 1;
    mid.vz = (pb.vz + pa.vz) >> 1;
    mid0.vx = (b0->vx + a0->vx) >> 1;
    mid0.vy = (b0->vy + a0->vy) >> 1;
    mid0.vz = (b0->vz + a0->vz) >> 1;
    mid1.vx = (b1->vx + a1->vx) >> 1;
    mid1.vy = (b1->vy + a1->vy) >> 1;
    mid1.vz = (b1->vz + a1->vz) >> 1;
    along.vx = mid1.vx - mid0.vx;
    along.vy = mid1.vy - mid0.vy;
    along.vz = mid1.vz - mid0.vz;
    across.vx = pb.vx - pa.vx;
    across.vy = pb.vy - pa.vy;
    across.vz = pb.vz - pa.vz;
    OuterProduct0(&along, &across, &cross);
    VectorNormal(&cross, &n);
    x = n.vx;
    pos->vx = (short)mid.vx + (x * offset >> 12);
    x = n.vy;
    pos->vy = (short)mid.vy + (x * offset >> 12);
    x = n.vz;
    pos->vz = (short)mid.vz + (x * offset >> 12);
    dx >>= 16;
    rot->vx = r0->vx + dx;
    dy >>= 16;
    rot->vy = dy - r0->vy;
    dz >>= 16;
    rot->vz = r0->vz + dz;
}

#ifdef NON_MATCHING
/* Register choice and the order of the quad's stores. */
void jet_draw_heat(void)
{
    POLY_G4 *p = jet_buf->prims.next[3];
    int h = jet_heat;

    p->y0 = 0xDC - h;
    p->y1 = 0xDC - h;
    p->y2 = 0xDC;
    p->y3 = 0xDC;
    p->r0 = 0x80 - h;
    p->r1 = 0x80 - h;
    p->x1 = 0x1C;
    p->x3 = 0x1C;
    p->x0 = 0x14;
    p->x2 = 0x14;
    p->g0 = h;
    p->b0 = 0;
    p->g1 = h;
    p->b1 = 0;
    p->r2 = 0x80;
    p->g2 = 0;
    p->b2 = 0;
    p->r3 = 0x80;
    p->g3 = 0;
    p->b3 = 0;
    func_80046848(p, 0);
    addPrim(&jet_buf->hud_ot[1], p);
    p++;
    jet_buf->prims.next[3] = p;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_draw_heat);
#endif

#ifdef NON_MATCHING
/* One instruction short: the game reaches jet_bonus_obj through its address from the first use. */
void jet_draw_bonus(JetBuffer *buf, short id, int dx, int dy, int dz)
{
    Target *t;

    if (id == 0 || id == 0x5B) {
        return;
    }
    jet_bonus_obj->model = jet_models[id];
    jet_bonus_rot.vx += dx;
    jet_bonus_rot.vy += dy;
    jet_bonus_rot.vz += dz;
    if (jet_bonus_blink == 1) {
        RotMatrix(&jet_bonus_rot, &jet_bonus_obj->m);
        jet_draw_icon(buf, jet_bonus_obj, 0, 0, t);
        jet_draw_number(jet_bonus_score, 0xDC, 0xA0, 0, 0x18);
    }
    if (--jet_bonus_time < 0x32) {
        if (jet_bonus_blink == 0) {
            jet_bonus_blink = 1;
        } else {
            jet_bonus_blink = 0;
        }
    }
    if (jet_bonus_time == 0) {
        jet_bonus_model = 0;
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_draw_bonus);
#endif

#ifdef NON_MATCHING
/* Five instructions short: the game spills the left edge and the caller-saved values around SetSemiTrans. */
void jet_draw_number(int value, short x, short y, short zeros, unsigned short v)
{
    POLY_FT4 *p = jet_buf->prims.next[5];
    int div = 1000;
    unsigned char blank = 1;
    int n = value + 1;
    int i, digit;

    for (i = 0; i < 4; i++) {
        digit = 0;
        while (n > div) {
            n -= div;
            digit++;
        }
        if (digit) {
            blank = 0;
        }
        if (!value && div == 1) {
            blank = 0;
        }
        if (zeros == 1 || digit || !blank) {
            p->x0 = x + i * 14;
            p->y0 = y;
            p->x1 = x + i * 14 + 16;
            p->y1 = y;
            p->x2 = x + i * 14;
            p->y2 = y + 16;
            p->x3 = x + i * 14 + 16;
            p->y3 = y + 16;
            p->r0 = 0x80;
            p->g0 = 0x80;
            p->b0 = 0x80;
            p->u0 = digit * 16 + 0x30;
            p->v0 = v;
            p->u1 = digit * 16 + 0x40;
            p->v1 = v;
            p->u2 = digit * 16 + 0x30;
            p->v2 = v + 18;
            p->u3 = digit * 16 + 0x40;
            p->v3 = v + 18;
            p->tpage = jet_tpages[8];
            p->clut = jet_cluts[8];
            func_80046848(p, 1);
            addPrim(&jet_buf->hud_ot[1], p);
            p++;
        }
        div /= 10;
    }
    jet_buf->prims.next[5] = p;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_draw_number);
#endif

#ifdef NON_MATCHING
/* Four instructions short: the game keeps x and copies the sums into the parameters' registers. */
void jet_draw_sprite(short id, int x, int y, int w, int h, int u, int v, int tw, int th, unsigned char semi)
{
    POLY_FT4 *p = jet_buf->prims.next[5];

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
    p->u1 = u + tw;
    p->v1 = v;
    p->u2 = u;
    p->v2 = v + th;
    p->u3 = u + tw;
    p->v3 = v + th;
    p->tpage = jet_tpages[id];
    p->clut = jet_cluts[id];
    func_80046848(p, semi);
    addPrim(&jet_buf->hud_ot[1], p);
    p++;
    jet_buf->prims.next[5] = p;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_draw_sprite);
#endif

void jet_set_tpage(void)
{
    POLY_FT4 *p = jet_buf->prims.next[5];

    p->x0 = 0;
    p->y0 = 0;
    p->x1 = 0;
    p->y1 = 0;
    p->x2 = 0;
    p->y2 = 0;
    p->x3 = 0;
    p->y3 = 0;
    p->r0 = 0x80;
    p->g0 = 0x80;
    p->b0 = 0x80;
    p->u0 = 0;
    p->v0 = 0;
    p->u1 = 0;
    p->v1 = 0;
    p->u2 = 0;
    p->v2 = 0;
    p->u3 = 0;
    p->v3 = 0;
    p->tpage = jet_tpages[5];
    p->clut = jet_cluts[5];
    func_80046848(p, 0);
    addPrim(&jet_buf->ot[0xFFF], p);
    p++;
    p->x0 = 0;
    p->y0 = 0;
    p->x1 = 0;
    p->y1 = 0;
    p->x2 = 0;
    p->y2 = 0;
    p->x3 = 0;
    p->y3 = 0;
    p->r0 = 0x80;
    p->g0 = 0x80;
    p->b0 = 0x80;
    p->u0 = 0;
    p->v0 = 0;
    p->u1 = 0;
    p->v1 = 0;
    p->u2 = 0;
    p->v2 = 0;
    p->u3 = 0;
    p->v3 = 0;
    p->tpage = jet_tpages[5];
    p->clut = jet_cluts[5];
    func_80046848(p, 0);
    addPrim(&jet_buf->ot[2], p);
    p++;
    jet_buf->prims.next[5] = p;
}

#ifdef NON_MATCHING
/* One instruction short: the game clears the loop's counter before jet_init_stream. */
void jet_init(void)
{
    int i;

    jet_x8A84 = 0;
    D_80110BB8 = (void *)0x1F800000;
    D_800D16D4 = (void *)0x1F800000;
    jet_paused = 0;
    jet_work = (MATRIX *)0x1F800010;
    jet_view = (MATRIX *)0x1F800030;
    D_800D1A38 = (void *)0x1F800050;
    D_800D1A3C = (void *)0x1F800058;
    D_800A891C = (void *)0x1F800060;
    D_800A8920 = (void *)0x1F800064;
    jet_view->m[0][0] = 0x1000;
    jet_view->m[0][1] = 0;
    jet_view->m[0][2] = 0;
    jet_view->m[1][0] = 0;
    jet_view->m[1][1] = 0x1000;
    jet_view->m[1][2] = 0;
    jet_view->m[2][0] = 0;
    jet_view->m[2][1] = 0;
    jet_view->m[2][2] = 0x1000;
    jet_init_gfx();
    jet_load();
    jet_x8A84 = 0x99;
    jet_init_stream();
    jet_init_nodes();
    jet_init_models();
    jet_init_track();
    jet_init_targets();
    jet_init_planes();
    for (i = 0; i < 100; i++) {
        jet_models[i] = jet_make_model(i);
    }
    jet_speed = 10000;
    jet_fog_near = 0x28AA;
    jet_fog_far = 0x37DC;
    jet_segment = 0;
    jet_track_t = 0;
    jet_score = 0;
    jet_visible = 0;
    jet_done = 0;
    jet_bonus_model = 0;
    jet_bonus_blink = 0;
    jet_bonus_rot.vx = 0;
    jet_bonus_rot.vy = 0;
    jet_bonus_rot.vz = 0;
    jet_hit_side = 0;
    jet_vol_engine = 0x7F;
    jet_vol_x8334 = 0x7F;
    jet_vol_a2 = 0;
    jet_vol_a3 = 0;
    jet_bonus_time = 0;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_init);
#endif

void jet_load(void)
{
    RECT unused = {0, 0, 320, 200};

    cd_read(jet_files[0].lba, jet_files[0].size, jet_tims, 0);
    while (cd_step()) {
    }
    cd_read(jet_files[1].lba, jet_files[1].size, (void *)0x800F0000, 0);
    while (cd_step()) {
    }
    jet_load_tims();
    cd_read(jet_files[2].lba, jet_files[2].size, jet_parts, 0);
    while (cd_step()) {
    }
    cd_read_lzs(jet_files[3].lba, jet_files[3].size, (void *)0x800F0000, 0);
    while (cd_step()) {
    }
}

void jet_load_tims(void)
{
    TIM_IMAGE tim;
    unsigned int *image;
    int i;

    for (i = 0; i < 9; i++) {
        image = jet_tims[i];
        jet_upload_tim(image);
        OpenTIM(image);
        ReadTIM(&tim);
    }
    jet_fade_tpage = GetTPage(0, 2, 0x280, 0);
    jet_fade_clut = GetClut(0, 0x1E0);
    jet_tpages[0] = GetTPage(0, 1, 0x280, 0);
    jet_cluts[0] = GetClut(0, 0x1E0);
    jet_tpages[1] = GetTPage(1, 1, 0x2C0, 0);
    jet_cluts[1] = GetClut(0, 0x1E1);
    jet_tpages[2] = GetTPage(1, 1, 0x2D0, 0);
    jet_cluts[2] = GetClut(0, 0x1E1);
    jet_tpages[3] = GetTPage(1, 1, 0x2E0, 0);
    jet_cluts[3] = GetClut(0, 0x1E1);
    jet_tpages[4] = GetTPage(0, 1, 0x280, 0x100);
    jet_cluts[4] = GetClut(0, 0x1FF);
    jet_tpages[5] = GetTPage(0, 1, 0x280, 0x100);
    jet_cluts[5] = GetClut(0, 0x1FE);
    jet_tpages[6] = GetTPage(0, 1, 0x300, 0);
    jet_cluts[6] = GetClut(0x10, 0x1E0);
    jet_tpages[7] = GetTPage(0, 1, 0x240, 0);
    jet_cluts[7] = GetClut(0x40, 0x1E0);
    jet_tpages[8] = GetTPage(0, 1, 0x240, 0x18);
    jet_cluts[8] = GetClut(0x30, 0x1E0);
    jet_tpages[9] = GetTPage(0, 1, 0x240, 0x50);
    jet_cluts[9] = GetClut(0x50, 0x1E0);
    jet_tpages[10] = GetTPage(0, 1, 0x240, 0x30);
    jet_cluts[10] = GetClut(0x20, 0x1E0);
    jet_tpages[11] = GetTPage(0, 1, 0x240, 0);
    jet_cluts[11] = GetClut(0x60, 0x1E0);
}

void jet_upload_tim(unsigned int *image)
{
    TIM_IMAGE tim;

    OpenTIM(image);
    while (ReadTIM(&tim)) {
        if (tim.caddr) {
            func_80044000(tim.crect, tim.caddr);
        }
        if (tim.paddr) {
            func_80044000(tim.prect, tim.paddr);
        }
    }
}

void jet_start_sound(void)
{
    snd_request.id = 0x10;
    snd_request.arg[0] = (long)jet_parts[0];
    snd_submit_request();
    snd_request.id = 0xC0;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0xB8;
    snd_request.arg[0] = 0x7F;
    snd_submit_request();
    snd_request.id = 0xBC;
    snd_request.arg[0] = 0;
    snd_submit_request();
    snd_request.id = 0xA2;
    snd_request.arg[0] = 0;
    snd_submit_request();
    snd_request.id = 0x2A;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = 0x177;
    snd_submit_request();
}

void jet_stop_sound(void)
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

void jet_sound_hit(short pan)
{
    int side = (jet_hit_side + 1) & 1;

    jet_hit_side = side;

    if (!side) {
        snd_request.id = 0xB0;
        snd_request.arg[0] = 0;
        snd_submit_request();
        snd_request.id = 0x28;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = pan;
        snd_submit_request();
    }
    if (jet_hit_side == 1) {
        snd_request.id = 0xB1;
        snd_request.arg[0] = 0;
        snd_submit_request();
        snd_request.id = 0x29;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = pan;
        snd_submit_request();
    }
}

void jet_sound_gun(unsigned char on)
{
    if (jet_gun_sound == 0 && on) {
        snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 0x22B;
        snd_submit_request();
    }
    if (on) {
        jet_vol_a3 = on;
        snd_request.id = 0xB3;
        snd_request.arg[0] = on;
        snd_submit_request();
        jet_gun_sound = on;
    } else {
        snd_request.id = 0x2B;
        snd_request.arg[0] = 0x40;
        snd_request.arg[1] = 0;
        snd_submit_request();
        jet_gun_sound = 0;
    }
}

void jet_set_volumes(void)
{
    snd_request.id = 0xA2;
    snd_request.arg[0] = jet_vol_a2;
    snd_submit_request();
    snd_request.id = 0xA3;
    snd_request.arg[0] = jet_vol_a3;
    snd_submit_request();
}

void jet_init_track(void)
{
    D_800A83C8.vx = 0;
    D_800A83C8.vy = -0x1B76;
    D_800A83C8.vz = 0xC8;
    jet_part_size = jet_parts[6];
    jet_part_offsets = jet_parts[5];
    jet_select_part(0, 3);
    jet_aim_mode = 1;
}

void jet_move_camera(void)
{
    VECTOR pos;
    SVECTOR rot, view;
    int pitch;

    jet_track_point(jet_track_t, -100, &pos, &rot);
    jet_track_t += jet_speed;
    jet_track_pos.vx = pos.vx;
    jet_track_pos.vy = pos.vy;
    jet_track_pos.vz = pos.vz;
    if (rot.vx < 0) {
        rot.vx += 0x1000;
    }
    jet_cam_tilt.vz = -rot.vz;
    pitch = rsin(rot.vx) / 15;
    if (pitch > 0 && jet_speed > 0xA7F8) {
        jet_speed -= pitch;
    }
    if (pitch < 0 && jet_speed < 0x1D4C0) {
        jet_speed -= pitch;
    }
    jet_cam_pos.vx = pos.vx;
    jet_cam_pos.vy = pos.vy;
    jet_cam_pos.vz = pos.vz;
    view.vx = -rot.vx;
    view.vy = -rot.vy;
    view.vz = 0;
    RotMatrix(&view, &jet_cam);
    RotMatrix(&jet_cam_tilt, &jet_tilt);
    CompMatrix(&jet_tilt, &jet_cam, &jet_cam);
}

void jet_select_part(unsigned char part, int unused)
{
    int offset = jet_part_offsets[part];
    void *base = jet_parts[4];

    jet_part = offset + (char *)base;
    jet_last_segment = *jet_part_size;
}

void jet_nothing(void)
{
}

#ifdef NON_MATCHING
/* The cooldown's decrement: the game loads it once for the test and the store. */
void jet_read_pad(void)
{
    unsigned int pad = func_8001C808(1);

    if (!jet_paused) {
        jet_pad_dir = 0;
        D_800A8A7C = 0;
        if (pad & 0x8000) {
            jet_pad_dir = 4;
        }
        if (pad & 0x2000) {
            jet_pad_dir = 6;
        }
        if (pad & 0x1000) {
            jet_pad_dir = 8;
            if (pad & 0x8000) {
                jet_pad_dir = 7;
            }
            if (pad & 0x2000) {
                jet_pad_dir = 9;
            }
        }
        if (pad & 0x4000) {
            jet_pad_dir = 2;
            if (pad & 0x8000) {
                jet_pad_dir = 1;
            }
            if (pad & 0x2000) {
                jet_pad_dir = 3;
            }
        }
        if (jet_aim_mode == 1) {
            if (pad & 0x4000) {
                jet_aim_y += 5;
            }
            if (pad & 0x1000) {
                jet_aim_y -= 5;
            }
            if (pad & 0x8000) {
                jet_aim_x -= 5;
            }
            if (pad & 0x2000) {
                jet_aim_x += 5;
            }
            jet_firing = 0;
            if (pad & 0x20) {
                jet_sound_gun(jet_heat);
                if (jet_heat > 8) {
                    jet_heat--;
                }
                if (!jet_fire_wait--) {
                    jet_fire_wait = 1;
                    jet_firing = 1;
                    jet_laser_frame = (unsigned char)(jet_laser_frame + 3) % 15;
                }
            } else {
                jet_sound_gun(0);
                if (jet_heat < 0x80) {
                    jet_heat++;
                }
            }
            if (jet_aim_x > 0x140) {
                jet_aim_x = 0x140;
            }
            if (jet_aim_x < 0) {
                jet_aim_x = 0;
            }
            if (jet_aim_y > 0xF0) {
                jet_aim_y = 0xF0;
            }
            if (jet_aim_y < 0) {
                jet_aim_y = 0;
            }
        }
        if (!jet_aim_mode) {
            if (pad & 0x4000) {
                jet_fog_far -= 10;
            }
            if (pad & 0x1000) {
                jet_fog_far += 10;
            }
            if (pad & 0x8000) {
                jet_fog_near -= 10;
            }
            if (pad & 0x2000) {
                jet_fog_near += 10;
            }
            if (pad & 0x40) {
                jet_debug_offset.vz -= 100;
            }
            if (pad & 0x10) {
                jet_debug_offset.vz += 100;
            }
            if (pad & 0x80) {
                jet_debug_offset.vx -= 100;
            }
            if (pad & 0x20) {
                jet_debug_offset.vx += 100;
            }
            if (pad & 0x8) {
                jet_debug_offset.vy -= 100;
            }
            if (pad & 0x2) {
                jet_debug_offset.vy += 100;
            }
            if (pad & 0x4) {
                jet_speed += 0x400;
            }
            if (pad & 0x1) {
                if (jet_speed >= 0x400) {
                    jet_speed -= 0x400;
                }
            }
            if (pad & 0x800) {
                jet_speed = 0;
            }
        }
    }
    if (pad & 0x800) {
        jet_start_held++;
    } else {
        jet_start_held = 0;
    }
    if (jet_start_held == 1) {
        if (jet_paused == 1) {
            jet_paused = 0;
        } else {
            jet_paused = 1;
        }
        jet_sound_hit(0x3B);
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_read_pad);
#endif

void jet_init_stream(void)
{
    int i;

    jet_stream_t = 0xFFFE;
    jet_stream_prev = 0;
    jet_face_adds = jet_parts[8];
    jet_face_drops = jet_parts[9];
    jet_light_adds = jet_parts[2];
    jet_light_drops = jet_parts[3];
    for (i = 0; i < 12000; i++) {
        jet_face_links[i].prev = 0xFFFF;
        jet_face_links[i].next = 0xFFFF;
    }
    jet_faces_shown = 0;
    for (i = 0; i < 9000; i++) {
        jet_light_links[i].prev = 0xFFFF;
        jet_light_links[i].next = 0xFFFF;
    }
    jet_lights_shown = 0;
    jet_stream_first = 1;
}

#ifdef NON_MATCHING
/* One addition's operands the other way round. */
void jet_stream(int step)
{
    unsigned int from = jet_stream_t;
    unsigned int to = from + step;
    unsigned int seg, n, i;

    jet_stream_prev = from;
    jet_stream_t = to;
    seg = from >> 18;
    n = (to >> 18) - seg;
    jet_segment += n;
    for (i = 0; i < n + jet_stream_first; i++) {
        for (;;) {
            unsigned short c = *jet_face_adds++;
            if (c == 0xFFFF) {
                break;
            }
            jet_add_face(c);
        }
        for (;;) {
            unsigned short c = *jet_face_drops++;
            if (c == 0xFFFF) {
                break;
            }
            jet_drop_face(c);
        }
    }
    for (i = 0; i < n; i++) {
        for (;;) {
            unsigned short c = *jet_light_adds++;
            if (c == 0xFFFF) {
                break;
            }
            jet_add_light(c);
        }
        for (;;) {
            unsigned short c = *jet_light_drops++;
            if (c == 0xFFFF) {
                break;
            }
            jet_drop_light(c);
        }
    }
    if (jet_stream_first == 1) {
        jet_stream_first = 0;
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_stream);
#endif

#ifdef NON_MATCHING
/* Register choice and the order of the first loads. */
void jet_stream_in(int step)
{
    unsigned int from = jet_stream_t;
    unsigned int to = from + step;
    unsigned int i;

    jet_stream_prev = from;
    jet_stream_t = to;
    jet_stream_segments = (to >> 18) - (from >> 18);
    jet_segment += jet_stream_segments;
    for (i = 0; i < jet_stream_segments + jet_stream_first; i++) {
        for (;;) {
            unsigned short c = *jet_face_adds++;

            if (c == 0xFFFF) {
                break;
            }
            jet_add_face(c);
        }
    }
    for (i = 0; i < jet_stream_segments; i++) {
        for (;;) {
            unsigned short c = *jet_light_adds++;

            if (c == 0xFFFF) {
                break;
            }
            jet_add_light(c);
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_stream_in);
#endif

#ifdef NON_MATCHING
/* The two loads of the loop's bound in each other's registers. */
void jet_stream_out(int step)
{
    unsigned int i;

    for (i = 0; i < jet_stream_segments + jet_stream_first; i++) {
        for (;;) {
            unsigned short c = *jet_face_drops++;

            if (c == 0xFFFF) {
                break;
            }
            jet_drop_face(c);
        }
    }
    for (i = 0; i < jet_stream_segments; i++) {
        for (;;) {
            unsigned short c = *jet_light_drops++;

            if (c == 0xFFFF) {
                break;
            }
            jet_drop_light(c);
        }
    }
    if (jet_stream_first == 1) {
        jet_stream_first = 0;
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_stream_out);
#endif

#ifdef NON_MATCHING
/* Two instructions short: register choice and where the count's store goes. */
void jet_add_face(unsigned short id)
{
    Link *l = &jet_face_links[id];
    unsigned short n = jet_faces_shown;
    unsigned short tail;

    if (n == 0) {
        jet_face_head = id;
        jet_face_tail = id;
        jet_faces_shown = 1;
    } else {
        tail = jet_face_tail;
        l->prev = tail;
        jet_face_links[tail].next = id;
        jet_face_tail = id;
        jet_faces_shown = n + 1;
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_add_face);
#endif

void jet_drop_face(unsigned short id)
{
    unsigned short prev = jet_face_links[id].prev;
    unsigned short next = jet_face_links[id].next;

    if (prev != 0xFFFF) {
        jet_face_links[prev].next = next;
    } else {
        jet_face_head = next;
    }
    if (next != 0xFFFF) {
        jet_face_links[next].prev = prev;
    } else {
        jet_face_tail = prev;
    }
    jet_face_links[id].prev = 0xFFFF;
    jet_face_links[id].next = 0xFFFF;
    jet_faces_shown--;
}

#ifdef NON_MATCHING
/* Two instructions short: register choice. */
void jet_add_light(unsigned short id)
{
    unsigned short n = jet_lights_shown;
    unsigned short tail;

    if (n == 0) {
        jet_light_links[id].prev = 0xFFFF;
        jet_light_links[id].next = 0xFFFF;
        jet_light_head = id;
        jet_light_tail = id;
        jet_lights_shown = 1;
    } else {
        tail = jet_light_tail;
        jet_light_links[id].next = 0xFFFF;
        jet_light_links[id].prev = tail;
        jet_light_links[tail].next = id;
        jet_light_tail = id;
        jet_lights_shown = n + 1;
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_add_light);
#endif

void jet_drop_light(unsigned short id)
{
    unsigned short prev = jet_light_links[id].prev;
    unsigned short next = jet_light_links[id].next;

    if (prev != 0xFFFF) {
        jet_light_links[prev].next = next;
    } else {
        jet_light_head = next;
    }
    if (next != 0xFFFF) {
        jet_light_links[next].prev = prev;
    } else {
        jet_light_tail = prev;
    }
    jet_lights_shown--;
}

void jet_init_targets(void)
{
    int i;

    for (i = 0; i < 100; i++) {
        jet_targets[i].slot = -1;
        jet_targets[i].state = 0;
    }
    jet_target_head = 0;
    for (i = 0; i < 100; i++) {
        jet_target_next[i] = i + 1;
    }
    jet_heat = 0x80;
    jet_aim_x = 0xA0;
    jet_firing = 0;
    jet_fire_wait = 0;
    jet_aim_y = 0x78;
    jet_placed_segment = 0;
    jet_placed = 0;
    jet_target_count = 0;
}

#ifdef NON_MATCHING
/* Two instructions short: the index is kept in a0, and the tables' loads come in another order. */
inline void jet_select_path(unsigned char i, unsigned char set)
{
    if (set == 0) {
        jet_path_keys = (char *)jet_parts[11] + ((int *)jet_parts[12])[i];
        jet_path_count = ((int *)jet_parts[13])[i];
    }
    if (set == 1) {
        jet_path_keys = (char *)jet_parts[4] + ((int *)jet_parts[5])[i];
        jet_path_count = ((int *)jet_parts[6])[i];
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_select_path);
#endif

#ifdef NON_MATCHING
/* Three instructions short: the game stores the product before shifting it, and multiplies the other way round. */
void jet_path_point(unsigned int t, SVECTOR *keys, VECTOR *out, unsigned char flip)
{
    SVECTOR k[2];
    VECTOR d;
    SVECTOR *p = &keys[t >> 16];
    int frac = t & 0xFFFF;

    k[0].vx = p[0].vx;
    k[0].vy = p[0].vy;
    k[0].vz = p[0].vz;
    k[1].vx = p[1].vx;
    k[1].vy = p[1].vy;
    k[1].vz = p[1].vz;
    d.vx = k[1].vx - k[0].vx;
    d.vy = k[1].vy - k[0].vy;
    d.vz = k[1].vz - k[0].vz;
    d.vx *= frac;
    d.vx >>= 16;
    d.vy *= frac;
    d.vy >>= 16;
    d.vz *= frac;
    d.vz >>= 16;
    if (!flip) {
        out->vx = p->vx + d.vx;
        out->vy = -p->vy - d.vy;
        out->vz = -p->vz - d.vz;
    } else {
        out->vx = p->vx + d.vx;
        out->vy = p->vy + d.vy;
        out->vz = p->vz + d.vz;
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_path_point);
#endif

void jet_draw_aim(JetBuffer *buf)
{
    POLY_FT4 *p = buf->prims.next[5];

    p->x0 = jet_aim_x - 16;
    p->y0 = jet_aim_y - 16;
    p->x1 = jet_aim_x + 16;
    p->y1 = jet_aim_y - 16;
    p->x2 = jet_aim_x - 16;
    p->y2 = jet_aim_y + 16;
    p->x3 = jet_aim_x + 16;
    p->y3 = jet_aim_y + 16;
    p->r0 = 0x80;
    p->g0 = 0x80;
    p->b0 = 0x80;
    p->u0 = 0;
    p->v0 = 0;
    p->u1 = 0x40;
    p->v1 = 0;
    p->u2 = 0;
    p->v2 = 0x40;
    p->u3 = 0x40;
    p->v3 = 0x40;
    p->tpage = jet_tpages[0];
    p->clut = jet_cluts[0];
    func_80046848(p, 0);
    addPrim(&buf->ot[1], p);
    p++;
    buf->prims.next[5] = p;
}

#ifdef NON_MATCHING
/* Two additions' operands the other way round. */
void jet_draw_lasers(void)
{
    POLY_FT4 *p;
    short w;

    if (jet_firing == 1) {
        p = jet_buf->prims.next[5];
        w = jet_heat >> 3;
        p->x0 = jet_gun_lx + w;
        p->y0 = jet_gun_ly;
        p->x1 = jet_aim_x;
        p->y1 = jet_aim_y;
        p->x2 = jet_gun_lx - w;
        p->y2 = jet_gun_ly;
        p->x3 = jet_aim_x;
        p->y3 = jet_aim_y;
        p->r0 = 0x80;
        p->g0 = 0x80;
        p->b0 = 0x80;
        p->u0 = 0x20 - jet_laser_frame;
        p->v0 = 0;
        p->u1 = 0x20 - jet_laser_frame;
        p->v1 = 0x40;
        p->u2 = 0x10 - jet_laser_frame;
        p->v2 = 0;
        p->u3 = 0x10 - jet_laser_frame;
        p->v3 = 0x40;
        p->tpage = jet_tpages[1];
        p->clut = jet_cluts[1];
        func_80046848(p, 1);
        addPrim(&jet_buf->ot[1], p);
        p++;
        p->x0 = jet_gun_rx + w;
        p->y0 = jet_gun_ry;
        p->x1 = jet_aim_x;
        p->y1 = jet_aim_y;
        p->x2 = jet_gun_rx - w;
        p->y2 = jet_gun_ry;
        p->x3 = jet_aim_x;
        p->y3 = jet_aim_y;
        p->r0 = 0x80;
        p->g0 = 0x80;
        p->b0 = 0x80;
        p->u0 = 0x20 - jet_laser_frame;
        p->v0 = 0;
        p->u1 = 0x20 - jet_laser_frame;
        p->v1 = 0x40;
        p->u2 = 0x10 - jet_laser_frame;
        p->v2 = 0;
        p->u3 = 0x10 - jet_laser_frame;
        p->v3 = 0x40;
        p->tpage = jet_tpages[1];
        p->clut = jet_cluts[1];
        func_80046848(p, 1);
        addPrim(&jet_buf->ot[1], p);
        p++;
        jet_buf->prims.next[5] = p;
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_draw_lasers);
#endif

#ifdef NON_MATCHING
/* Two instructions short: register choice. */
short jet_spawn(Target *tmpl, short parent)
{
    short slot;
    Target *t;
    int model;
    JetBox *b;
    short cx, cy, cz;
    VECTOR unused;

    if (jet_target_count < 99) {
        jet_target_count++;
        slot = jet_alloc_target();
        jet_targets[slot] = *tmpl;
        t = &jet_targets[slot];
        model = tmpl->info.model;
        t->state = 1;
        t->slot = slot;
        t->obj = jet_node_new(model, 0, 0, 1, parent ? jet_targets[parent].obj : &jet_root, tmpl->pos.vx,
                              tmpl->pos.vy, tmpl->pos.vz, tmpl->rot.vx, tmpl->rot.vy, tmpl->rot.vz);
        b = &jet_boxes[(short)model];
        t = &jet_targets[slot];
        cx = (b->max.vx + b->min.vx) >> 1;
        cy = (b->max.vy + b->min.vy) >> 1;
        cz = (b->max.vz + b->min.vz) >> 1;
        t->box[0].vx = cx;
        t->box[0].vy = cy;
        t->box[0].vz = b->max.vz;
        t->box[1].vx = cx;
        t->box[1].vy = cy;
        t->box[1].vz = b->min.vz;
        t->box[2].vx = cx;
        t->box[2].vy = b->max.vy;
        t->box[2].vz = cz;
        t->box[3].vx = cx;
        t->box[3].vy = b->min.vy;
        t->box[3].vz = cz;
        t->box[4].vx = b->max.vx;
        t->box[4].vy = cy;
        t->box[4].vz = cz;
        t->box[5].vx = b->min.vx;
        t->box[5].vy = cy;
        t->box[5].vz = cz;
    }
    return slot;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_spawn);
#endif


inline void jet_despawn(Target *t)
{
    if (t->slot != -1) {
        jet_target_count--;
        jet_node_free(t->obj);
        jet_free_target(t->slot);
        t->slot = -1;
        t->state = 0;
    }
}

short jet_alloc_target(void)
{
    short i = jet_target_head;

    jet_target_head = jet_target_next[i];
    return i;
}

void jet_free_target(short i)
{
    jet_target_next[i] = jet_target_head;
    jet_target_head = i;
}

#ifdef NON_MATCHING
/* One instruction longer: GCC finds the template's address from the path's. */
void jet_place_targets(void)
{
    int seg, i, k;
    unsigned char *count;
    JetPlace *place;

    for (seg = jet_placed_segment; seg < jet_segment; seg++) {
        count = (unsigned char *)jet_parts[15] + seg;
        for (i = 0; i < *count; i++) {
            place = jet_parts[14];
            for (k = 0; k < 20; k++) {
                jet_template.info.params[k] = place[jet_placed].params[k];
            }
            jet_template.info.anim = place[jet_placed].anim;
            jet_template.info.speed = place[jet_placed].speed;
            jet_place(0, 0, 0, place[jet_placed].kind, place[jet_placed].model);
            jet_placed++;
        }
    }
    jet_placed_segment = jet_segment;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_place_targets);
#endif

inline void jet_place(short x, short y, short z, short kind, short model)
{
    jet_template.pos.vx = x;
    jet_template.pos.vy = y;
    jet_template.pos.vz = z;
    jet_template.info.kind = kind;
    jet_template.info.fresh = 1;
    jet_template.info.hit = 0;
    jet_template.info.model = model;
    jet_spawn(&jet_template, 0);
}

inline void jet_place_still(short x, short y, short z, short kind, short model)
{
    jet_template.pos.vx = x;
    jet_template.pos.vy = y;
    jet_template.pos.vz = z;
    jet_template.info.kind = kind;
    jet_template.info.fresh = 1;
    jet_template.info.hit = 0;
    jet_template.info.params[P_X12] = 0;
    jet_template.info.model = model;
    jet_spawn(&jet_template, 0);
}

/* A fade over the whole screen: a shaded quad of colour c, drawn through the subtractive mode a dummy sprite sets. */
#define JET_FADE(c)                                                                                                    \
    {                                                                                                                  \
        POLY_G4 *g = buf->prims.next[3];                                                                               \
        POLY_FT4 *f;                                                                                                   \
        unsigned char v = (c);                                                                                         \
                                                                                                                       \
        g->x0 = 0;                                                                                                     \
        g->y0 = 0;                                                                                                     \
        g->x1 = 320;                                                                                                   \
        g->y1 = 0;                                                                                                     \
        g->x2 = 0;                                                                                                     \
        g->y2 = 240;                                                                                                   \
        g->x3 = 320;                                                                                                   \
        g->y3 = 240;                                                                                                   \
        g->r0 = v;                                                                                                     \
        g->g0 = v;                                                                                                     \
        g->b0 = v;                                                                                                     \
        g->r1 = v;                                                                                                     \
        g->g1 = v;                                                                                                     \
        g->b1 = v;                                                                                                     \
        g->r2 = v;                                                                                                     \
        g->g2 = v;                                                                                                     \
        g->b2 = v;                                                                                                     \
        g->r3 = v;                                                                                                     \
        g->g3 = v;                                                                                                     \
        g->b3 = v;                                                                                                     \
        func_80046848(g, 1);                                                                                           \
        addPrim(&buf->hud_ot[0], g);                                                                                   \
        g++;                                                                                                           \
        buf->prims.next[3] = g;                                                                                        \
        f = buf->prims.next[5];                                                                                        \
        f->r0 = 0;                                                                                                     \
        f->g0 = 0;                                                                                                     \
        f->b0 = 0;                                                                                                     \
        f->x0 = 0;                                                                                                     \
        f->y0 = 0;                                                                                                     \
        f->x1 = 0;                                                                                                     \
        f->y1 = 0;                                                                                                     \
        f->x2 = 0;                                                                                                     \
        f->y2 = 0;                                                                                                     \
        f->x3 = 0;                                                                                                     \
        f->y3 = 0;                                                                                                     \
        f->tpage = jet_fade_tpage;                                                                                         \
        f->clut = jet_fade_clut;                                                                                          \
        func_80046848(f, 0);                                                                                           \
        addPrim(&buf->hud_ot[1], f);                                                                                   \
        f++;                                                                                                           \
        buf->prims.next[5] = f;                                                                                        \
    }

#ifdef NON_MATCHING
/* Eighteen instructions short: GCC merges the kinds' shared tails (the despawn, the hit check) into other cases than the game's, and the registers follow. */
void jet_update_targets(JetBuffer *buf)
{
    static const unsigned char rot_order[8] = {0};
    int i, k;
    Target *t;
    JetInfo *in;
    unsigned char view;
    short z;
    VECTOR ahead, unused[2], p;
    SVECTOR r;
    int x, y, dx, dy, dz;

    jet_place_targets();
    jet_draw_aim(buf);
    jet_draw_lasers();
    for (i = 0; i < 100; i++) {
        t = &jet_targets[i];
        in = &t->info;
        z = 0;
        if (t->state == 0) {
            continue;
        }
        view = 0;
        switch (in->kind) {
        case 100:
            if (in->fresh == 1) {
                jet_select_path(in->anim, 0);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->time = 0;
                in->hit = 0;
                in->params[P_TYPE] = 1;
                in->frame = 0;
                in->a = t->pos.vx;
                in->b = t->pos.vy;
                in->c = t->pos.vz;
                in->params[P_SCORE] = 0;
            } else {
                in->time++;
                in->frame++;
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            k = in->frame;
            jet_track_point(jet_track_t + 0x2FFFD, -100, &p, &r);
            {
                int sx = in->a, sy, sz;

                sx += k * (p.vx - sx) >> 7;
                sy = in->b;
                sy += k * (p.vy - sy) >> 7;
                sz = in->c;
                sz += k * (p.vz - sz) >> 7;
                t->pos.vx = sx;
                t->pos.vy = sy;
                t->pos.vz = sz;
            }
            jet_track_point(jet_track_t + 0x3FFFC, -100, &p, &r);
            dx = t->pos.vx - p.vx;
            dy = t->pos.vy - p.vy;
            dz = t->pos.vz - p.vz;
            SquareRoot0(dx * dx + dy * dy + dz * dz);
            if (in->time > 0x80) {
                if (jet_score > 5) {
                    jet_score -= 5;
                } else {
                    jet_score = 0;
                }
                in->active = 0;
            }
            if (in->hit) {
                jet_hit(t);
            }
            break;
        case 17:
            if (in->fresh == 1) {
                if (in->params[17]) {
                    jet_sound_hit(in->params[17]);
                }
                jet_select_path(in->anim, 0);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->time = 0;
                in->hit = 0;
                in->frame = 0;
                in->a = (t->count - 2) << 16;
            } else {
                in->time++;
            }
            if (in->params[3] < jet_segment) {
                in->frame += in->speed;
            }
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            if (in->hit) {
                jet_hit(t);
            }
            if (in->frame < in->a) {
                jet_path_point(in->frame, t->keys, &t->pos, 0);
                t->rot.vx = 0;
                t->rot.vy = 0;
                t->rot.vz = 0;
            }
            break;
        case 0:
            if (in->fresh == 1) {
                if (in->params[17]) {
                    jet_sound_hit(in->params[17]);
                }
                jet_select_path(in->anim, 0);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->time = 0;
                in->hit = 0;
                in->frame = 0;
                in->a = (t->count - 1) << 16;
            } else {
                in->time++;
            }
            in->frame += in->speed;
            if (in->params[1] == 1) {
                in->frame %= in->a;
            }
            if (in->frame > in->a) {
                in->active = 0;
            }
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            jet_path_point(in->frame, t->keys, &t->pos, 0);
            t->rot.vx += in->params[3];
            t->rot.vy += in->params[4];
            t->rot.vz += in->params[5];
            if (in->hit) {
                jet_hit(t);
            }
            break;
        case 1:
            if (in->fresh == 1) {
                if (in->params[17]) {
                    jet_sound_hit(in->params[17]);
                }
                jet_select_path(in->anim, 0);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->time = 0;
                in->hit = 0;
                in->frame = 0;
                in->a = (t->count - 1) << 16;
            } else {
                in->time++;
            }
            in->frame += in->speed;
            if (in->params[1] == 1) {
                in->frame %= in->a;
            }
            if (in->frame > in->a) {
                in->active = 0;
            }
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            jet_path_point(in->frame, t->keys, &t->pos, 0);
            jet_path_point((in->frame + 0x10000) % ((t->count - 1) << 16), t->keys, &ahead, 0);
            dx = ahead.vx - t->pos.vx;
            dz = ahead.vz - t->pos.vz;
            dy = ahead.vy - t->pos.vy;
            t->rot.vx = ratan2(dy, SquareRoot0(dx * dx + dz * dz));
            t->rot.vy = -ratan2(dz, dx) - 0x400;
            t->rot.vz = 0;
            if (in->hit) {
                jet_hit(t);
            }
            break;
        case 10:
            if (in->fresh == 1) {
                if (in->params[17]) {
                    jet_sound_hit(in->params[17]);
                }
                jet_select_path(0, 1);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->time = 0;
                in->hit = 0;
                in->frame = 0;
                in->a = (t->count - 1) << 16;
            } else {
                in->time++;
            }
            in->frame += in->speed;
            if (in->params[1] == 1) {
                in->frame %= in->a;
            }
            if (in->frame > in->a) {
                in->active = 0;
            }
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            jet_track_point(jet_track_t + 0x3FFFC, 10, &t->pos, &t->rot);
            view = 1;
            if (in->hit) {
                jet_hit(t);
            }
            break;
        case 4:
            if (in->fresh == 1) {
                if (in->params[17]) {
                    jet_sound_hit(in->params[17]);
                }
                jet_select_path(in->anim, 0);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->hit = 0;
                jet_path_point(0, t->keys, &t->pos, 0);
            }
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            if (in->params[3] < jet_segment) {
                in->a += 4;
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            t->pos.vy += in->a;
            if (t->pos.vy > 0) {
                in->active = 0;
            }
            if (in->hit) {
                jet_hit(t);
            }
            break;
        case 5:
            if (in->fresh == 1) {
                if (in->params[17]) {
                    jet_sound_hit(in->params[17]);
                }
                jet_select_path(in->anim, 0);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->hit = 0;
                in->frame = 0;
                in->a = (t->count - 1) << 16;
                t->rot.vx = in->params[3];
                t->rot.vy = in->params[4];
                t->rot.vz = in->params[5];
            }
            in->frame += in->speed;
            if (in->params[1] == 1) {
                in->frame %= in->a;
            }
            if (in->frame > in->a) {
                in->active = 0;
            }
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            t->rot.vx += in->params[6];
            t->rot.vy += in->params[7];
            t->rot.vz += in->params[8];
            if (in->params[P_TYPE] == 5) {
                t->obj->model = (&jet_models[91])[in->params[14]];
            }
            if (in->params[14] == 1) {
                in->params[14] = 0;
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            jet_path_point(in->frame, t->keys, &t->pos, 0);
            if (!in->hit) {
                break;
            }
            if (in->params[P_TYPE] != 5 || jet_speed < 0x4015) {
                jet_hit(t);
            }
            if (in->params[P_TYPE] == 5) {
                in->params[14] = 1;
            }
            break;
        case 2:
            if (in->fresh == 1) {
                if (in->params[17]) {
                    jet_sound_hit(in->params[17]);
                }
                jet_select_path(in->anim, 0);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->hit = 0;
                in->frame = rand() % in->params[3] * 2 - in->params[3] - 1;
                in->a = 0;
                in->b = 0;
                in->c = (t->count - 1) << 16;
                t->rot.vx = 0;
                t->rot.vy = 0;
                t->rot.vz = 0;
                jet_path_point(0, t->keys, &t->pos, 0);
            }
            in->b += in->speed;
            if (in->b > in->c) {
                in->active = 0;
            }
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            if (in->a > in->frame) {
                in->a -= 5;
            }
            if (in->a < in->frame) {
                in->a += 5;
            }
            t->rot.vx = in->a;
            t->rot.vy += in->params[4];
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            jet_path_point(in->b, t->keys, &t->pos, 0);
            if (in->hit) {
                jet_hit(t);
            }
            break;
        case 230:
            if (in->fresh == 1) {
                in->fresh = 0;
                in->active = 1;
                in->hit = 0;
            }
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            if (in->hit) {
                jet_hit(t);
            }
            break;
        case 7:
        case 13:
            if (in->fresh == 1) {
                jet_select_path(in->anim, 0);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->hit = 0;
                t->rot.vx = in->params[3];
                t->rot.vy = in->params[4];
                t->rot.vz = 0;
                jet_path_point(0, t->keys, &t->pos, 0);
                in->frame = 0;
            }
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            if (in->params[5] < jet_segment && in->frame++ < in->params[7]) {
                t->rot.vx += in->params[6];
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            if (in->hit) {
                jet_hit(t);
            }
            break;
        case 11:
            jet_sound_hit(0x8E);
            for (k = 0; k < in->params[3]; k++) {
                jet_place_still(0x3446, -10000, 0x20CB, 12, 0x2A);
            }
            jet_despawn(t);
            /* fall through */
        case 12:
            if (in->fresh == 1) {
                in->hit = 0;
                in->active = 100;
                in->fresh = 0;
                in->time = 0;
                in->frame = rand() % 60 - 30;
                in->a = -rand() % 200;
                in->b = rand() % 60 - 30;
            } else {
                in->time++;
            }
            in->a++;
            t->pos.vx += in->frame;
            t->pos.vy += in->a;
            t->rot.vx += 10;
            t->rot.vy += 400;
            t->rot.vz += 200;
            t->pos.vz += in->b;
            if (--in->active == 0) {
                jet_despawn(t);
            }
            break;
        case 8:
            if (in->fresh == 1) {
                jet_select_path(in->anim, 0);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->hit = 0;
                in->frame = 0;
                jet_path_point(0, t->keys, &t->pos, 0);
            }
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            t->pos.vy -= in->params[3];
            if ((in->params[3] -= in->params[4]) < 0) {
                jet_sound_hit(0x98);
                for (k = 0; k < 20; k++) {
                    x = t->pos.vx;
                    y = t->pos.vy;
                    dz = t->pos.vz;
                    jet_place_still(x, y, dz, 9, rand() % 3 + 0x44);
                }
                in->active = 0;
            }
            break;
        case 9:
            if (in->fresh == 1) {
                in->hit = 0;
                in->active = 100;
                in->fresh = 0;
                in->time = 0;
                in->frame = rand() % 60 - 30;
                in->a = rand() % 60 - 30;
                in->b = rand() % 60 - 30;
            } else {
                in->time++;
            }
            t->pos.vx += in->frame;
            t->pos.vy += in->a;
            t->rot.vx += 10;
            t->rot.vy += 400;
            t->rot.vz += 200;
            t->pos.vz += in->b;
            if (--in->active == 0) {
                jet_despawn(t);
            }
            break;
        case 14:
            if (in->fresh == 1) {
                jet_sound_hit(10);
                jet_select_path(in->anim, 0);
                t->keys = jet_path_keys;
                t->count = jet_path_count;
                in->fresh = 0;
                in->active = 1;
                in->hit = 0;
                in->time = 0;
                in->frame = -70;
                jet_path_point(0, t->keys, &t->pos, 0);
            }
            in->frame++;
            if (++in->time == 5) {
                for (k = 0; k < in->params[3]; k++) {
                    x = t->pos.vx + rand() % 100 - 50;
                    y = t->pos.vy + 500;
                    dz = t->pos.vz + rand() % 100 - 50;
                    jet_place_still(x, y, dz, 15, 0x2A);
                }
                jet_place_still(t->pos.vx, t->pos.vy, t->pos.vz, 16, 0x29);
            }
            if (in->frame < 0) {
                t->rot.vy += 20;
            }
            if (in->frame == 80) {
                in->active = 0;
            }
            t->pos.vy += in->frame;
            if (in->params[2] < jet_segment) {
                in->active = 0;
            }
            if (!in->active) {
                jet_despawn(t);
                break;
            }
            if (in->hit) {
                jet_hit(t);
            }
            break;
        case 15:
            if (in->fresh == 1) {
                in->hit = 0;
                in->active = 200;
                in->fresh = 0;
                in->time = 0;
                in->frame = rand() % 80 - 40;
                in->a = rand() % 80 - 40;
                in->b = -(rand() % 40 + 40);
            } else {
                in->time++;
            }
            in->b++;
            t->rot.vx += 480;
            t->rot.vy += 40;
            t->rot.vz += 610;
            t->pos.vx += in->frame;
            t->pos.vy += in->b;
            t->pos.vz += in->a;
            if (--in->active == 0) {
                jet_despawn(t);
            }
            break;
        case 16:
            if (in->fresh == 1) {
                in->active = 200;
                in->hit = 0;
                in->fresh = 0;
                in->time = 0;
                in->frame = -60;
            } else {
                in->time++;
            }
            if (in->time > 20) {
                t->pos.vy += in->frame;
                in->frame++;
            }
            if (--in->active == 0) {
                jet_despawn(t);
            }
            break;
        case 3:
            t->pos.vx = jet_track_pos.vx;
            t->pos.vy = jet_track_pos.vy - 2500;
            t->pos.vz = jet_track_pos.vz;
            z = 1000;
            t->rot.vx = 0;
            t->rot.vy = 0;
            t->rot.vz = 0;
            break;
        case 201:
            if (in->fresh == 1) {
                in->active = 20;
                in->fresh = 0;
                in->time = 0;
                in->hit = 0;
            } else {
                in->time++;
            }
            jet_place_still(t->pos.vx, t->pos.vy, t->pos.vz, 0xCA, 0x2A);
            if (--in->active == 0) {
                jet_despawn(t);
            }
            break;
        case 202:
            if (in->fresh == 1) {
                in->hit = 0;
                in->active = 50;
                in->fresh = 0;
                in->time = 0;
                in->frame = rand() % 20 - 10;
                in->a = rand() % 40 - 20;
                in->b = rand() % 20 - 10;
            } else {
                in->time++;
            }
            t->pos.vx += in->frame;
            t->pos.vy += in->a;
            t->rot.vx += 10;
            t->rot.vy += 100;
            t->rot.vz += 20;
            t->pos.vz += in->b;
            if (--in->active == 0) {
                jet_despawn(t);
            }
            break;
        case 203:
            if (in->fresh == 1) {
                in->hit = 0;
                in->active = 50;
                in->fresh = 0;
                in->time = 0;
                in->frame = rand() % 200 - 100;
                in->a = rand() % 200 - 100;
                in->b = rand() % 200 - 100;
            } else {
                in->time++;
            }
            t->pos.vx += in->frame;
            t->pos.vy += in->a;
            t->rot.vx += 0;
            t->rot.vy += 300;
            t->rot.vz += 0;
            t->pos.vz += in->b;
            if (--in->active == 0) {
                jet_despawn(t);
            }
            break;
        case 255:
            if (in->fresh == 1) {
                in->fresh = 0;
                in->time = 0;
            } else {
                in->time++;
            }
            if (in->params[2] < jet_speed) {
                jet_speed -= in->params[0];
            }
            if (jet_speed < 0) {
                jet_speed = 0;
                jet_place_still(0, 0, 0, 0xFD, 0x1D);
            }
            if (in->params[1] < in->time) {
                jet_despawn(t);
            }
            break;
        case 254:
            if (in->fresh == 1) {
                in->fresh = 0;
                in->time = 0;
                in->frame = 0;
                in->a = VSync(-1);
                in->b = 0;
            } else {
                in->time++;
            }
            if (in->frame == 0) {
                jet_speed = 0;
            }
            if (in->frame == 1) {
                jet_speed += in->params[1];
                in->b++;
            }
            if (in->params[0] < VSync(-1) - in->a) {
                in->frame = 1;
            }
            if (in->params[2] < in->b) {
                jet_despawn(t);
            }
            break;
        case 252:
            if (in->fresh == 1) {
                in->fresh = 0;
                in->time = 0;
                jet_place_still(0, 0, 0, 3, 0x3B);
                jet_speed = 0;
                jet_visible = 1;
            } else {
                in->time++;
            }
            JET_FADE(~(in->time * 2));
            if (in->time >= 126) {
                jet_despawn(t);
                jet_speed = 0x4000;
            }
            break;
        case 253:
            if (in->fresh == 1) {
                in->fresh = 0;
                in->time = 0;
                jet_speed = 0;
                jet_stop_sound();
            } else {
                in->time++;
            }
            JET_FADE(in->time * 2);
            if (in->time >= 128) {
                jet_despawn(t);
                jet_speed = 0x4000;
                jet_visible = 0;
                jet_done = 1;
            }
            break;
        case 250:
            if (jet_score < in->params[P_SCORE]) {
                jet_template.info.params[0] = 300;
                jet_template.info.params[1] = 400;
                jet_template.info.params[2] = 0;
                jet_place_still(t->pos.vx, t->pos.vy, t->pos.vz, 0xFF, 0x1E);
            }
            jet_despawn(t);
            break;
        }
        t->obj->m.t[0] = t->pos.vx;
        t->obj->m.t[1] = t->pos.vy;
        t->obj->m.t[2] = t->pos.vz;
        if (rot_order[0] == 0) {
            RotMatrixYXZ(&t->rot, &t->obj->m);
        }
        if (rot_order[0] == 1) {
            RotMatrixZYX(&t->rot, &t->obj->m);
        }
        if (rot_order[0] == 2) {
            RotMatrix(&t->rot, &t->obj->m);
        }
        if (view == 0) {
            jet_draw_target(buf, t->obj, z, 0, t);
        }
        if (view == 1) {
            jet_draw_guns(buf, t->obj, z, 0, t);
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_update_targets);
#endif

#ifdef NON_MATCHING
/* The inlined jet_place_still loads the place as halfwords; the game loads words. */
void jet_hit(Target *t)
{
    unsigned char damage = jet_heat >> 5;

    if (damage == 0) {
        damage = 1;
    }
    t->info.params[P_LIFE] -= damage;
    if (t->info.params[P_LIFE] < 0) {
        jet_destroy(t);
    } else {
        jet_place_still(t->pos.vx, t->pos.vy, t->pos.vz, 0xCA, 0x3F);
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_hit);
#endif

#ifdef NON_MATCHING
/* Register choice in the debris loops. */
void jet_destroy(Target *t)
{
    JetInfo *in = &t->info;
    int i, x, y, z;

    if (in->params[P_TYPE] == 1) {
        jet_score += in->params[P_SCORE];
        jet_sound_hit(in->params[P_PAN]);
        in->active = 0;
        for (i = 0; i < 3; i++) {
            x = t->pos.vx;
            y = t->pos.vy;
            z = t->pos.vz;
            jet_place_still(x, y, z, 0xCA, rand() % 3 + 0x3F);
        }
        jet_bonus_time = 100;
        jet_bonus_blink = 1;
        jet_bonus_rot.vx = 0;
        jet_bonus_rot.vy = 0;
        jet_bonus_rot.vz = 0;
        jet_bonus_score = in->params[P_SCORE];
        jet_bonus_model = t->obj->id;
    }
    if (in->params[P_TYPE] == 2) {
        jet_score += in->params[P_SCORE];
        jet_sound_hit(in->params[P_PAN]);
        in->active = 0;
        for (i = 0; i < 3; i++) {
            x = t->pos.vx;
            y = t->pos.vy;
            z = t->pos.vz;
            jet_place_still(x, y, z, 0xCB, rand() % 3 + 0x3C);
        }
        jet_bonus_time = 100;
        jet_bonus_blink = 1;
        jet_bonus_rot.vx = 0;
        jet_bonus_rot.vy = 0;
        jet_bonus_rot.vz = 0;
        jet_bonus_score = in->params[P_SCORE];
        jet_bonus_model = t->obj->id;
    }
    if (in->params[P_TYPE] == 3) {
        jet_score += in->params[P_SCORE];
        t->rot.vx += in->params[P_SPIN];
    }
    if (in->params[P_TYPE] == 4) {
        jet_score += in->params[P_SCORE];
        jet_sound_hit(in->params[P_PAN]);
        in->active = 0;
        jet_bonus_time = 100;
        jet_bonus_blink = 1;
        jet_bonus_rot.vx = 0;
        jet_bonus_rot.vy = 0;
        jet_bonus_rot.vz = 0;
        jet_bonus_score = in->params[P_SCORE];
        jet_bonus_model = t->obj->id;
    }
    if (in->params[P_TYPE] == 5) {
        jet_score += in->params[P_SCORE];
        jet_sound_hit(in->params[P_PAN]);
        in->active = 0;
        for (i = 0; i < 100; i++) {
            x = t->pos.vx;
            y = t->pos.vy;
            z = t->pos.vz;
            jet_place_still(x, y, z, 0xCB, rand() % 3 + 0x3F);
        }
        jet_bonus_time = 100;
        jet_bonus_blink = 1;
        jet_bonus_rot.vx = 0;
        jet_bonus_rot.vy = 0;
        jet_bonus_rot.vz = 0;
        jet_bonus_score = in->params[P_SCORE];
        jet_bonus_model = t->obj->id;
    }
    if (jet_score >= 10000) {
        jet_score = 9999;
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_destroy);
#endif

void jet_init_planes(void)
{
    VECTOR tl4, bl4, tr4, br4;
    VECTOR bl = {-160, 120, 256};
    VECTOR br = {160, 120, 256};
    VECTOR tl = {-160, -120, 256};
    VECTOR tr = {160, -120, 256};
    int ax, ay, az, bx, by, bz;

    tl4.vx = tl.vx >> 2;
    tl4.vy = tl.vy >> 2;
    tl4.vz = tl.vz >> 2;
    bl4.vx = bl.vx >> 2;
    bl4.vy = bl.vy >> 2;
    bl4.vz = bl.vz >> 2;
    tr4.vx = tr.vx >> 2;
    tr4.vy = tr.vy >> 2;
    tr4.vz = tr.vz >> 2;
    br4.vx = br.vx >> 2;
    br4.vy = br.vy >> 2;
    br4.vz = br.vz >> 2;
    OuterProduct0(&tl4, &bl4, &jet_left_plane);
    OuterProduct0(&tr4, &br4, &jet_right_plane);
    ax = jet_left_plane.vx;
    ay = jet_left_plane.vy;
    az = jet_left_plane.vz;
    bx = jet_right_plane.vx;
    by = jet_right_plane.vy;
    bz = jet_right_plane.vz;
    jet_plane_l_d = -(ax * (tl.vx >> 2)) - ay * (tl.vy >> 2) - az * (tl.vz >> 2);
    jet_plane_r_d = -(bx * (tr.vx >> 2)) - by * (tr.vy >> 2) - bz * (tr.vz >> 2);
    jet_plane_l_side = ax * (tr.vx >> 2) + ay * (tr.vy >> 2) + az * (tr.vz >> 2) + jet_plane_l_d;
    jet_plane_r_side = bx * (tl.vx >> 2) + by * (tl.vy >> 2) + bz * (tl.vz >> 2) + jet_plane_r_d;
    jet_plane_l_len = SquareRoot0(ax * ax + ay * ay + az * az);
    jet_plane_r_len = SquareRoot0(jet_right_plane.vx * jet_right_plane.vx + jet_right_plane.vy * jet_right_plane.vy + jet_right_plane.vz * jet_right_plane.vz);
}


#ifdef NON_MATCHING
/* Register choice: the two flags swap registers. */
int jet_in_view(int *v)
{
    int ax = jet_left_plane.vx, ay = jet_left_plane.vy, az = jet_left_plane.vz;
    int bx = jet_right_plane.vx, by = jet_right_plane.vy, bz = jet_right_plane.vz;
    int x = v[0] >> 2;
    int y = v[1] >> 2;
    int z = v[2] >> 2;
    int d1 = ax * x + ay * y + az * z + jet_plane_l_d;
    int d2 = bx * x + by * y + bz * z + jet_plane_r_d;
    int a = 0;
    int b = 0;

    if (d1 > 0 && jet_plane_l_side > 0) {
        a = 1;
    }
    if (d1 < 0 && jet_plane_l_side < 0) {
        a = 1;
    }
    if (d2 > 0 && jet_plane_r_side > 0) {
        b = 1;
    }
    if (d2 < 0 && jet_plane_r_side < 0) {
        b = 1;
    }
    return a & b;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_in_view);
#endif

#ifdef NON_MATCHING
/* Register choice: the two flags swap registers. */
int jet_in_view_s(SVECTOR *v)
{
    int ax = jet_left_plane.vx, ay = jet_left_plane.vy, az = jet_left_plane.vz;
    int bx = jet_right_plane.vx, by = jet_right_plane.vy, bz = jet_right_plane.vz;
    int x = v->vx >> 2;
    int y = v->vy >> 2;
    int z = v->vz >> 2;
    int d1 = ax * x + ay * y + az * z + jet_plane_l_d;
    int d2 = bx * x + by * y + bz * z + jet_plane_r_d;
    int a = 0;
    int b = 0;

    if (d1 > 0 && jet_plane_l_side > 0) {
        a = 1;
    }
    if (d1 < 0 && jet_plane_l_side < 0) {
        a = 1;
    }
    if (d2 > 0 && jet_plane_r_side > 0) {
        b = 1;
    }
    if (d2 < 0 && jet_plane_r_side < 0) {
        b = 1;
    }
    return a & b;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_in_view_s);
#endif

inline int jet_plane_l(int x, int y, int z)
{
    int ax = jet_left_plane.vx;
    int ay = jet_left_plane.vy;
    int az = jet_left_plane.vz;

    return ax * (x >> 2) + ay * (y >> 2) + az * (z >> 2) + jet_plane_l_d;
}

inline int jet_plane_r(int x, int y, int z)
{
    int bx = jet_right_plane.vx;
    int by = jet_right_plane.vy;
    int bz = jet_right_plane.vz;

    return bx * (x >> 2) + by * (y >> 2) + bz * (z >> 2) + jet_plane_r_d;
}

#ifdef NON_MATCHING
/* Register choice. */
int jet_near_view(int *v, short margin)
{
    int d;
    int a = 0;
    int b = 0;

    d = jet_plane_l(v[0], v[1], v[2]);
    if (jet_plane_l_side > 0 && d >= 0) {
        a = 1;
    }
    if (jet_plane_l_side < 0 && d <= 0) {
        a = 1;
    }
    if (!a && (d < 0 ? -d : d) / jet_plane_l_len < margin) {
        a = 1;
    }
    d = jet_plane_r(v[0], v[1], v[2]);
    if (jet_plane_r_side > 0 && d >= 0) {
        b = 1;
    }
    if (jet_plane_r_side < 0 && d <= 0) {
        b = 1;
    }
    if (!b && (d < 0 ? -d : d) / jet_plane_r_len < margin) {
        b = 1;
    }
    return a & b;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_near_view);
#endif

int jet_near_l(int x, int y, int z, short margin)
{
    int d = jet_plane_l(x, y, z);
    int in = 0;

    if (jet_plane_l_side > 0 && d >= 0) {
        in = 1;
    }
    if (jet_plane_l_side < 0 && d <= 0) {
        in = 1;
    }
    if (!in && (d < 0 ? -d : d) / jet_plane_l_len < margin) {
        in = 1;
    }
    return in;
}

int jet_near_r(int x, int y, int z, short margin)
{
    int d = jet_plane_r(x, y, z);
    int in = 0;

    if (jet_plane_r_side > 0 && d >= 0) {
        in = 1;
    }
    if (jet_plane_r_side < 0 && d <= 0) {
        in = 1;
    }
    if (!in && (d < 0 ? -d : d) / jet_plane_r_len < margin) {
        in = 1;
    }
    return in;
}

void jet_init_models(void)
{
    jet_face_used = 0;
    jet_strip_used = 0;
    jet_model_used = 0;
    jet_face_pool = jet_parts[10];
    jet_strip_pool = jet_parts[16];
    jet_boxes = jet_parts[1];
}

#ifdef NON_MATCHING
/* Two instructions longer: register choice. */
Model *jet_make_model(int id)
{
    Model *m = jet_alloc_model();
    JetBox *b = &jet_boxes[id];
    short faces = b->faces;
    short strips = b->strips;

    m->min_x = b->min.vx;
    m->max_x = b->max.vx;
    m->min_z = b->min.vz;
    m->max_z = b->max.vz;
    m->x2 = 0;
    m->count = faces;
    m->strip_count = strips;
    m->x8 = 0;
    m->total = faces + strips;
    m->faces = jet_alloc_faces(faces);
    m->strips = jet_alloc_strips(strips);
    return m;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_make_model);
#endif

Model *jet_alloc_model(void)
{
    int i = jet_model_used;

    jet_model_used = i + 1;
    return &jet_model_pool[i];
}

Face *jet_alloc_faces(int n)
{
    Face *f;
    int i = jet_face_used;

    jet_face_used = i + n;
    f = jet_face_pool;
    return &f[i];
}

Strip *jet_alloc_strips(int n)
{
    Strip *s;
    int i = jet_strip_used;

    jet_strip_used = i + n;
    s = jet_strip_pool;
    return &s[i];
}

#ifdef NON_MATCHING
/* Three instructions longer: GCC reaches the second buffer's colours from the first buffer's address. */
void jet_init_gfx(void)
{
    func_80043814(&jet_bufs[0].draw, 0, 0, 320, 240);
    SetDefDispEnv(&jet_bufs[0].disp, 0, 240, 320, 240);
    func_80043814(&jet_bufs[1].draw, 0, 240, 320, 240);
    SetDefDispEnv(&jet_bufs[1].disp, 0, 0, 320, 240);
    jet_bufs[0].draw.isbg = 0;
    jet_bufs[1].draw.isbg = 0;
    jet_bufs[0].draw.r0 = 0;
    jet_bufs[0].draw.g0 = 0;
    jet_bufs[0].draw.b0 = 8;
    jet_bufs[1].draw.r0 = 0;
    jet_bufs[1].draw.g0 = 0;
    jet_bufs[1].draw.b0 = 8;
    SetGeomOffset(160, 160);
    SetGeomScreen(256);
    SetDispMask(1);
    SetBackColor(128, 128, 128);
    SetFarColor(0, 0, 8);
    jet_init_prims(&jet_bufs[0].prims);
    jet_init_prims(&jet_bufs[1].prims);
    jet_reset_prims(&jet_bufs[0].prims);
    jet_reset_prims(&jet_bufs[1].prims);
    func_80044244(jet_bufs[0].ot, 0x1000);
    func_80044244(jet_bufs[1].ot, 0x1000);
    func_80044244(jet_bufs[0].hud_ot, 0xB4);
    func_80044244(jet_bufs[1].hud_ot, 0xB4);
    jet_buf = &jet_bufs[0];
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_init_gfx);
#endif

void jet_clear_frame(void)
{
    func_80044244(jet_buf->ot, 0x1000);
    func_80044244(jet_buf->hud_ot, 0xB4);
    jet_reset_prims(&jet_buf->prims);
}

void jet_init_prims(JetPrims *p)
{
    int i;

    for (i = 0; i < 1; i++) {
        SetPolyF3(&p->f3[i]);
    }
    for (i = 0; i < 1; i++) {
        SetPolyF4(&p->f4[i]);
    }
    for (i = 0; i < 1600; i++) {
        SetPolyG3(&p->g3[i]);
    }
    for (i = 0; i < 30; i++) {
        SetPolyG4(&p->g4[i]);
    }
    for (i = 0; i < 1; i++) {
        SetPolyFT3(&p->ft3[i]);
    }
    for (i = 0; i < 300; i++) {
        SetPolyFT4(&p->ft4[i]);
    }
    for (i = 0; i < 1; i++) {
        SetPolyGT3(&p->gt3[i]);
    }
    for (i = 0; i < 1; i++) {
        SetPolyGT4(&p->gt4[i]);
    }
    for (i = 0; i < 1; i++) {
        SetLineF2(&p->lf2[i]);
    }
}


void jet_reset_prims(JetPrims *p)
{
    p->next[0] = p->f3;
    p->next[1] = p->f4;
    p->next[2] = p->g3;
    p->next[3] = p->g4;
    p->next[4] = p->ft3;
    p->next[5] = p->ft4;
    p->next[6] = p->gt3;
    p->next[7] = p->gt4;
    p->next[8] = p->lf2;
}

#ifdef NON_MATCHING
/* Four instructions longer: the game steps the two lists' pointers by 56 from their next fields. */
void jet_init_nodes(void)
{
    Obj *tail, *head;
    int i;

    jet_node_init(&jet_root, 0);
    jet_root.level = 0;
    jet_node_head = 0;
    for (i = 0; i < 200; i++) {
        jet_node_next[i] = i + 1;
    }
    tail = jet_level_tails;
    head = jet_level_heads;
    for (i = 0; i < 10; i++) {
        tail->next = 0;
        tail->prev = head;
        head->next = tail;
        head->prev = 0;
        head++;
        tail++;
    }
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_init_nodes);
#endif

void jet_node_init(Obj *o, short slot)
{
    o->m.m[0][0] = 0x1000;
    o->m.m[1][1] = 0x1000;
    o->m.m[2][2] = 0x1000;
    o->m.t[0] = 0;
    o->m.t[1] = 0;
    o->m.t[2] = 0;
    o->m.m[0][1] = 0;
    o->m.m[0][2] = 0;
    o->m.m[1][0] = 0;
    o->m.m[1][2] = 0;
    o->m.m[2][0] = 0;
    o->m.m[2][1] = 0;
    o->parent = &jet_root;
    o->slot = slot;
    o->next = 0;
    o->prev = 0;
}

#ifdef NON_MATCHING
/* One instruction longer: register choice. */
Obj *jet_node_new(short id, int a1, int a2, int a3, Obj *parent, int x, int y, int z, short rx, short ry, short rz)
{
    SVECTOR rot;
    short slot = jet_alloc_node();
    Obj *o = &jet_nodes[slot];

    jet_node_link(o, parent);
    o->id = id;
    o->slot = slot;
    o->model = jet_models[id];
    rot.vx = rx;
    rot.vy = ry;
    rot.vz = rz;
    RotMatrix(&rot, &o->m);
    o->m.t[0] = x;
    o->m.t[1] = y;
    o->m.t[2] = z;
    return o;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_node_new);
#endif

void jet_node_free(Obj *o)
{
    jet_node_unlink(o);
    jet_free_node(o->slot);
}

short jet_alloc_node(void)
{
    short i = jet_node_head;

    jet_node_head = jet_node_next[i];
    return i;
}

void jet_free_node(short i)
{
    jet_node_next[i] = jet_node_head;
    jet_node_head = i;
}

#ifdef NON_MATCHING
/* One instruction short: the game computes the list head's address last. */
void jet_node_link(Obj *o, Obj *parent)
{
    Obj *head;

    o->parent = parent;
    o->level = parent->level + 1;
    head = &jet_level_heads[o->level];
    o->next = head->next;
    o->prev = o->next->prev;
    head->next->prev = o;
    head->next = o;
}
#else
INCLUDE_ASM("asm/overlay/jet/nonmatchings/jet", jet_node_link);
#endif

void jet_node_unlink(Obj *o)
{
    o->next->prev = o->prev;
    o->prev->next = o->next;
}
