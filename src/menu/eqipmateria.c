/* EQIPMENU.MNU's second object, 0x801D1B94-0x801D82A4 (its jump tables at 0x801D0000, data from 0x801D83F8): the
 * Materia command's screen, module 3. The member's weapon and armor slots with their materia, the stock beside them
 * and the highlighted materia's page; Check lists the member's commands, magic, summons and enemy skills; Arrange
 * sorts the stock, exchanges materia with another member's, removes all of it or trashes one. GCC 2.7.2, ASPSX 2.21,
 * -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

extern PartyMember members[3];
extern DRAWENV results_draw[2];
extern int menu_member;
extern int D_80062D9C;
extern unsigned char D_8009D4F7; /* bit 0: the materia can't be changed */
extern unsigned short field_pad2; /* the second pad's PadState.held, read as a variable of its own */
extern unsigned short D_8009D78A[]; /* [0] bit n: character n can be in the party */

typedef struct SVECTOR {
    short vx, vy, vz, pad;
} SVECTOR;

typedef struct VECTOR {
    int vx, vy, vz, pad;
} VECTOR;

/* libgpu's textured triangle. */
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

/* A polygon as libgs's TMD reader returns it. */
typedef struct TMD_PRIM {
    unsigned int id;
    unsigned char rgb[4][4];
    unsigned short tpage, clut;
    unsigned char uv[4][2];
    SVECTOR x0, x1, x2, x3;
    SVECTOR n0, n1, n2, n3;
    SVECTOR *v_ofs, *n_ofs;
    unsigned short vert[4], norm[4];
} TMD_PRIM;

/* mtr_801D8070's model: one triangle's normals and vertices, and its primitive in each of two buffers. */
typedef struct MtrTri {
    SVECTOR n[3];
    SVECTOR v[3];
} MtrTri;

typedef struct MtrModel {
    int count;
    MtrTri tris[1];
    POLY_FT3 prims[2][1];
} MtrModel;

int func_80046D74(unsigned int *tmd, int obj); /* libgs: opens a TMD object, its polygon count */
TMD_PRIM *func_80046DB4(TMD_PRIM *p);          /* libgs: the next polygon */
void func_80046870(void *p, int tex);          /* libgpu: a primitive's shading-off bit */
void SetPolyFT3(void *p);

int func_8002603C(int materia); /* its icon's palette */
unsigned char *func_80021258(int materia); /* a materia's name */
void materia_own(unsigned int materia);
void party_effects_sum(void);
unsigned char *func_800256DC(int slot); /* the member's weapon slots */
unsigned char *armor_slots(int slot);
int func_8002542C(int materia); /* adds a materia to the stock */

extern unsigned char mtr_elements[9][10];
/* [8] To next level, [9] MASTER, [20] on the stats' names, [25] on the independent materia's (SHOPMENU has them from
 * [2] on) */
extern unsigned char mtr_texts[42][20];
/* 0 the slots and stock, 1 a slot picked, 3 the stock for the slot, 4-7 Check's commands, magic, summons and enemy
 * skills, 8 Arrange's menu, 9 Trash's stock, 10 Trash's question */
extern int mtr_state;
extern int mtr_exchanging; /* Exchange is up */
extern RECT mtr_windows[11];
extern unsigned char mtr_confirm[3][0x24]; /* Trash's question and its answers */
extern RECT mtr_command_window;
extern RECT mtr_list_window;
extern RECT mtr_info_window;
extern short mtr_command_widths[4]; /* the command window's, by its columns */
extern MateriaInfo *mtr_info;
extern MenuCursor mtr_cursors[16];
extern int mtr_member;
/* Exchange's: 0 picking a slot, 1 the stock, 2 a character's row (the whole set); the list's highlighted row, top
 * and scroll step, whether it is scrolling, and whether a first pick was made */
extern int mtr_exch_state;
extern int mtr_exch_row;
extern int mtr_exch_top;
extern int mtr_exch_step;
extern int mtr_exch_scrolling;
extern int mtr_exch_picked;
/* the first pick: 0 a slot, 1 the stock, 2 a set; its slot's column and row, and its character's place in the list */
extern int mtr_exch_kind;
extern int mtr_exch_col;
extern int mtr_exch_slot_row;
extern int mtr_exch_char;
extern unsigned char mtr_info_buf[0x80]; /* what mtr_info and materia_info point at */
extern unsigned char mtr_sephiroth_text[0x24];
extern int mtr_highlight; /* the highlighted materia's id, -1 for none */
extern Character *mtr_chars;
extern int mtr_record; /* the member's place in mtr_chars */
extern unsigned char mtr_prims[2][0x3500]; /* the stock's primitives, apart from the menu's */
extern ScrollBar mtr_bar;
extern int mtr_scroll; /* the frame count, which Enemy Skill's page scrolls its learned list by */
extern MemberCommand *mtr_command_list; /* the member's commands */
extern unsigned char spell_data[];
extern unsigned char D_80070EE4[]; /* the summons' part of it */
extern unsigned char D_800710A4[]; /* the enemy skills' */
extern unsigned char mtr_joined_count;
extern unsigned char mtr_joined[9]; /* Exchange's characters, 0xFF after */
extern SVECTOR mtr_model_rot;
extern VECTOR mtr_model_pos;

void mtr_draw_materia(int m, int scroll, DRAWENV *env);
void mtr_801D827C(int x, int y);

static void mtr_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

int mtr_negative(int x)
{
    return x < 0;
}

int mtr_abs(int x)
{
    return x < 0 ? -x : x;
}

/* SHOPMENU's shop_draw_changes. */
void mtr_draw_changes(void)
{
    int i, color;
    int rows;

    for (i = 0, rows = 0; i < 8; i++) {
        if (mtr_info->stats[i]) {
            if (mtr_negative(mtr_info->stats[i])) {
                func_8002708C(0xB4, rows * 11 + 0x9A, 0xE1, 7);
                color = 2;
            } else {
                func_8002708C(0xB4, rows * 11 + 0x9A, 0xB3, 7);
                color = 6;
            }
            func_80029114(0xBC, rows * 11 + 0x9A, mtr_abs(mtr_info->stats[i]), 2, color);
            func_80026A34(0, 1, 0x1F, 0);
            func_80026F44(0x78, rows * 11 + 0x98, mtr_texts[20 + i], 7);
            if (i >= 6)
                func_8002708C(0xCA, rows * 11 + 0x9A, 0xD3, 7);
            rows++;
        }
    }
    func_80026A34(0, 1, 0x1F, 0);
}

/* A materia's page (SHOPMENU keeps a copy, shop_draw_materia): its name, element and stars, its AP and the AP to the
 * next level, then by materia_info_kind its abilities, its bonus or, for Enemy Skill, the learned skills in a list that
 * scrolls (clipped to env) and a grid of all 24. The master materia show only their name. */
#ifdef NON_MATCHING
/* Same code, but 17 instructions short: the game keeps the Master Magic case's tests (GCC drops them here), loads 0xFF
 * afresh in the ability loops, and spills Enemy Skill's scroll differently. */
void mtr_draw_materia(int m, int scroll, DRAWENV *env)
{
    int id = m & 0xFF;
    int element, i, j, k, y, top, skills, color, pal;
    RECT r;

    if (m == -1)
        return;
    kind_text(8, 0xB, 8);
    element = materia_data[id].element;
    if (element < 9)
        func_80026F44(0x18, 0x72, mtr_elements[element], 6);
    func_80026F44(0x18, 0x64, func_80021258(id), 7);
    pal = func_8002603C(id);
    func_80028CA0(8, 0x63, 0x80, 0x20, 16, 16, (short)pal, 0);
    if (id != 0x30 && id != 0x49 && id != 0x5A && id != 0x2C) {
        for (i = 0; i < mtr_info->levels; i++) {
            if (i < mtr_info->level)
                func_80028CA0(0x7C + i * 15, 0x63, 0x90, 0x20, 16, 16, (short)pal, 0);
            else
                func_80028CA0(0x7C + i * 15, 0x63, 0x90, 0x30, 16, 16, (short)pal, 0);
        }
        func_80028E00(0x96, 0x7E, mtr_info->ap_to_next, 7, 7);
        if ((unsigned int)m >> 8 != 0xFFFFFF) {
            func_80028E00(0x8F, 0x72, (unsigned int)m >> 8, 8, 7);
            func_80026A34(0, 1, 0x1F, 0);
        } else {
            func_80026A34(0, 1, 0x1F, 0);
            func_80026F44(0x96, 0x72, mtr_texts[9], 7);
        }
        func_80026F44(0x3C, 0x7E, mtr_texts[8], 5);
        func_80026F44(0x70, 0x8C, mtr_texts[3], 5);
        func_80026F44(0x7C, 0x72, mtr_texts[7], 5);
        func_80026F44(8, 0x8C, mtr_texts[2], 5);
        func_80026F44(0x70, 0x8C, mtr_texts[3], 5);
        switch (materia_info_kind) {
        case 1:
            for (i = 0, y = 0x98; i < 5; i++) {
                if (mtr_info->lines[i].id != 0xFF) {
                    color = mtr_info->lines[i].value ? 7 : 0;
                    func_80026F44(0x18, y, kind_text(0, mtr_info->lines[i].id, 8), color);
                    y += 12;
                }
            }
            break;
        case 2:
            func_80026F44(0x14, 0x98, kind_text(1, mtr_info->lines[0].id, 8), 7);
            break;
        case 3:
        case 0x13:
            for (i = 0, y = 0x98; i < 5; i++) {
                if (mtr_info->lines[i].id != 0xFF) {
                    color = mtr_info->lines[i].value ? 7 : 0;
                    func_80026F44(0x18, y, kind_text(5, mtr_info->lines[i].id, 8), color);
                    y += 12;
                }
            }
            break;
        default:
            switch (id) {
            case 0:
                func_80026F44(0x12, 0x98, mtr_texts[29], 7);
                func_80029114(0x54, 0x9A, mtr_info->lines[0].value, 2, 6);
                func_8002708C(0x4C, 0x9A, 0xB3, 7);
                func_8002708C(0x62, 0x9A, 0xD3, 7);
                break;
            case 1:
                func_80026F44(0x12, 0x98, mtr_texts[28], 7);
                func_80029114(0x54, 0x9A, mtr_info->lines[0].value, 2, 6);
                func_8002708C(0x4C, 0x9A, 0xB3, 7);
                func_8002708C(0x62, 0x9A, 0xD3, 7);
                break;
            case 2:
                func_80026F44(0x12, 0x98, mtr_texts[15], 7);
                func_80029114(0x56, 0x9A, mtr_info->lines[0].value, 2, 6);
                func_8002708C(0x4E, 0x9A, 0xB3, 7);
                func_8002708C(0x64, 0x9A, 0xD3, 7);
                break;
            case 3:
                func_80026F44(0x12, 0x98, mtr_texts[17], 7);
                func_80029114(0x44, 0x9A, mtr_info->lines[0].value, 2, 6);
                func_8002708C(0x3C, 0x9A, 0xB3, 7);
                func_8002708C(0x52, 0x9A, 0xD3, 7);
                break;
            case 4:
                func_80026F44(0x12, 0x98, mtr_texts[18], 7);
                func_80029114(0x3C, 0x9A, mtr_info->lines[0].value, 2, 6);
                func_8002708C(0x34, 0x9A, 0xB3, 7);
                func_8002708C(0x4A, 0x9A, 0xD3, 7);
                break;
            case 16:
                func_80026F44(0x12, 0x98, mtr_texts[16], 7);
                func_80029114(0x44, 0x9A, mtr_info->lines[0].value, 3, 6);
                func_8002708C(0x3D, 0x9A, 0xB3, 7);
                func_8002708C(0x59, 0x9A, 0xD3, 7);
                break;
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
                func_80026F44(0x12, 0x98, mtr_texts[25 + id], 7);
                break;
            default:
                func_80026F44(0x12, 0x98, func_80021258(id), 7);
                break;
            }
            func_80026A34(0, 1, 0x1F, 0);
            break;
        }
        mtr_draw_changes();
        return;
    }
    switch (id) {
    case 0x2C:
        func_80026A34(0, 1, 0x1F, 0);
        setRECT(&r, 0, 5, 0x16C, 0xDB);
        func_80026A94(env, &r);
        skills = m >> 8;
        for (i = 0, y = 0; i < 7; i++, y += 12) {
            for (j = 0; j < 2; j++) {
                k = (i * 2 + j + scroll / 6 * 2) % 24;
                if (skills >> k & 1)
                    func_80026F44(j ? 0x7A : 0x1D, y - (scroll - scroll / 6 * 12 - 0x90), kind_text(2, k, 8), 7);
            }
        }
        setRECT(&r, 0x18, 0x8F, 0xB4, 0x49);
        func_80026A94(env, &r);
        setRECT(&r, 0x18, 0x8E, 0xB4, 0x4B);
        func_80027B84(&r);
        for (i = 0; i < 24; i++) {
            if (skills >> i & 1)
                func_80028CA0(i % 12 * 15 + 0x18, i / 12 * 13 + 0x73, 0x90, 0x20, 16, 16, (short)pal, 0);
            else
                func_80028CA0(i % 12 * 15 + 0x18, i / 12 * 13 + 0x73, 0x90, 0x30, 16, 16, (short)pal, 0);
        }
        func_80026A34(0, 1, 0x1F, 0);
        break;
    case 0x49:
        return;
    }
}
#else
INCLUDE_ASM("asm/menu/eqipmenu/nonmatchings/eqipmateria", mtr_draw_materia);
#endif

/* A spell's MP cost, by kind (0 magic, 1 summons, 2 enemy skills) the index-th of the member's, with the member's MP;
 * for magic and summons the uses All and Quadra Magic give it. */
void mtr_spell_info(int x, int y, int index, int member, int kind)
{
    MemberSpell *spells;

    x += 0xC;
    func_80028CA0(x + 3, y, 0, 0x10, 0x28, 0x10, 1, 0);
    switch (kind) {
    case 0:
        spells = members[member].spells;
        if (spells[index].id == 0xFF)
            goto mp;
        if (spells[index].uses) {
            func_80026A34(0, 1, 0x3F, 0);
            func_80026F44(x - 5, y + 0x27, kind_text(5, 0xE, 8), 7);
        }
        break;
    case 1:
        spells = members[member].spells + 56;
        break;
    case 2:
        spells = members[member].spells + 72;
        break;
    }
    if (spells[index].id == 0xFF)
        goto mp;
    if (kind != 2) {
        if (spells[index].quadra_uses) {
            func_80026A34(0, 1, 0x3F, 0);
            func_80026F44(x - 5, y + 0x1C, kind_text(5, 0xF, 8), 7);
            if (spells[index].quadra_uses != 0xFF)
                ((void (*)())func_80028E00)(x + 0x13, y + 0x1E, spells[index].quadra_uses, 2, 7, 0);
            else
                func_80028CA0(x + 0x13, y + 0x1E, 0x90, 0x60, 0x10, 8, 6, 0);
            func_80028CA0(x + 0x23, y + 0x1E, 0x78, 8, 8, 8, 7, 0);
        }
        if (spells[index].uses) {
            if (spells[index].uses != 0xFF)
                ((void (*)())func_80028E00)(x + 0x13, y + 0x2A, spells[index].uses, 2, 7, 0);
            else
                func_80028CA0(x + 0x13, y + 0x2A, 0x90, 0x60, 0x10, 8, 6, 0);
            func_80028CA0(x + 0x23, y + 0x2A, 0x78, 8, 8, 8, 7, 0);
        }
    }
    func_80028E00(x, y + 0x12, spells[index].mp, 3, 7);
mp:
    func_80028E00(x + 0x19, y + 0x12, members[member].mp, 3, 7);
    func_80028CA0(x + 0x15, y + 0x12, 0xD8, 0, 4, 8, 1, 0);
}

void mtr_swap_words(unsigned int *a, unsigned int *b)
{
    unsigned int t = *a;

    *a = *b;
    *b = t;
}

/* ITEMMENU's item_sort. */
int mtr_sort(void *base, unsigned int n, int (*cmp)(), void (*swap)())
{
    unsigned int los[64];
    unsigned int his[64];
    unsigned int hi, lo, i;
    unsigned int depth;

    if (n < 2)
        return 1;
    depth = 0;
    los[0] = 0;
    his[0] = n - 1;
    while (depth != -1) {
        lo = los[depth];
        i = lo + 1;
        n = hi = his[depth];
        while (i < hi) {
            while (i < hi && cmp(i, lo, &base) <= 0)
                i++;
            while (i <= hi && cmp(lo, hi, &base) <= 0)
                hi--;
            if (i < hi)
                swap(i++, hi--, &base);
        }
        if (cmp(lo, hi, &base) > 0)
            swap(lo, hi, &base);
        if (lo < hi && lo < --hi) {
            if (i < n && hi - lo < n - i) {
                mtr_swap_words(&hi, &n);
                mtr_swap_words(&lo, &i);
            }
            if (hi > lo) {
                los[depth] = lo;
                his[depth] = hi;
                depth++;
            }
        }
        if (n > i) {
            los[depth] = i;
            his[depth] = n;
            depth++;
        }
        if (depth >= 64)
            return 0;
        depth--;
    }
    return 1;
}

void mtr_swap_entries(unsigned int *a, unsigned int *b)
{
    unsigned int t = *a;

    *a = *b;
    *b = t;
}

/* By kind, then by key: 1 when a goes after b. */
int mtr_compare(unsigned char kind_a, unsigned char kind_b, unsigned int a, unsigned int b)
{
    if (kind_a == kind_b) {
        if (a == b)
            return 0;
        if (b < a)
            return -1;
        return 1;
    }
    if (kind_b < kind_a)
        return 1;
    return -1;
}

/* Arrange's order, as mtr_sort's cmp over the stock: by icon colour (an empty place last), then by AP and id. */
short mtr_by_kind(a, b, stock)
unsigned short a, b;
int **stock;
{
    short kind_a, kind_b;
    unsigned int key_a, key_b;

    if ((*stock)[a] == -1) {
        kind_a = 0x80;
        key_a = 0;
    } else {
        kind_a = func_8002603C((*stock)[a]);
        key_a = ((unsigned int)(*stock)[a] >> 8) + ((*stock)[a] << 24);
    }
    if ((*stock)[b] == -1) {
        kind_b = 0x80;
        key_b = 0;
    } else {
        kind_b = func_8002603C((*stock)[b]);
        key_b = ((unsigned int)(*stock)[b] >> 8) + ((*stock)[b] << 24);
    }
    return mtr_compare(kind_a, kind_b, key_a, key_b);
}

void mtr_swap_stock(a, b, stock)
unsigned short a, b;
unsigned int **stock;
{
    mtr_swap_entries(&(*stock)[a], &(*stock)[b]);
}

void mtr_arrange(int order)
{
    if (!order)
        mtr_sort(save_data.materia, 200, mtr_by_kind, mtr_swap_stock);
}

void mtr_801D2EB4(void)
{
}

/* By kind: 0 a command's description, 1 a materia's. */
unsigned char *mtr_text(int kind, int id)
{
    unsigned char *text;

    switch (kind) {
    case 1:
        text = kind_text(13, id, 0);
        break;
    case 0:
        text = kind_text(5, id, 0);
        break;
    }
    return text;
}

/* STATMENU's stat_commands, in the command window. */
void mtr_commands(void)
{
    RECT unused, win;
    PartyMember *m;
    int cols, col, row, shift, id, single;

    m = &members[mtr_member];
    cols = m->command_rows;
    for (col = 0; col < cols; col++) {
        for (row = 0; row < 4; row++) {
            id = m->commands[row + col * 4].id;
            single = cols < 2;
            if (id != 0xFF) {
                shift = single ? 0 : col > 0 ? 8 : 0;
                func_80026F44(mtr_command_window.x + 6 + col * 0x2C + shift, mtr_command_window.y + row * 12 + 5,
                              kind_text(5, id, 8), 7);
            }
        }
    }
    func_8001DE0C(&win, mtr_command_window.x, mtr_command_window.y, mtr_command_widths[cols - 1],
                  mtr_command_window.h);
    func_8001E040(&win);
}

/* Check's Magic: the member's spells three to a row, scrolling, with an arrow on those All or Quadra Magic reach the
 * whole party with, and the highlighted one's MP cost beside the list. */
void mtr_magic_list(void *table) /* unread */
{
    RECT clip, win;
    ScrollBar bar;
    int top;
    MemberSpell *spells;
    int rows, i, j, all;

    setRECT(&clip, 0, 5, 0x16C, 0xDB);
    func_80026A94(&results_draw[results_buf], &clip);
    bar.shown = 3;
    bar.total = 0x12;
    bar.top = mtr_cursors[5].top;
    bar.x = mtr_list_window.x + 0xEE;
    bar.y = mtr_list_window.y + 3;
    bar.w = 0xA;
    bar.h = 0x32;
    func_80028484(&bar);
    rows = 4;
    spells = members[mtr_member].spells;
    if (!mtr_cursors[5].scrolling)
        rows = 3;
    top = mtr_cursors[5].top * 3;
    for (j = 0; j < rows; j++) {
        for (i = 0; i < 3; i++) {
            if (spells[j * 3 + i + top].id != 0xFF)
                func_80026F44(mtr_list_window.x + 0x18 + i * 0x4E - 3,
                              mtr_list_window.y + 6 + j * 16 + mtr_cursors[5].scroll_step * 4,
                              kind_text(0, spells[j * 3 + i + top].id, 8), 7);
        }
    }
    for (j = 0; j < rows; j++) {
        for (i = 0; i < 3; i++) {
            if (spells[j * 3 + (i + top)].id == 0xFF)
                continue;
            if ((spells[j * 3 + (i + top)].uses || spells[j * 3 + (i + top)].quadra_uses)
                && (spells[j * 3 + (i + top)].targets & 0xC) == 0xC)
                all = 1;
            else
                all = 0;
            if (all)
                func_80028CA0(mtr_list_window.x + 0x44 + i * 0x4E + 5,
                              mtr_list_window.y + 8 + j * 16 + mtr_cursors[5].scroll_step * 4, 0x80, 8, 8, 8, 2, 0);
        }
    }
    clip.x = 0;
    clip.y = mtr_list_window.y + 3;
    clip.w = 0x16C;
    clip.h = mtr_list_window.h - 6;
    func_80026A94(&results_draw[results_buf], &clip);
    j = mtr_list_window.y;
    i = mtr_list_window.x + mtr_list_window.w;
    mtr_spell_info(i, j, mtr_cursors[5].col + mtr_cursors[5].row * 3 + mtr_cursors[5].top * 3, mtr_member, 0);
    setRECT(&clip, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x7F, &clip);
    func_8001E040(&mtr_list_window);
    func_8001DE0C(&win, i, j, mtr_info_window.w, mtr_info_window.h);
    func_8001E040(&win);
}

/* Check's Summon: one to a row. */
void mtr_summon_list(void *table) /* unread */
{
    RECT clip, win;
    ScrollBar bar;
    MemberSpell *spells;
    int top, rows, j, x;

    setRECT(&clip, 0, 5, 0x16C, 0xDB);
    func_80026A94(&results_draw[results_buf], &clip);
    bar.shown = 3;
    bar.total = 0x10;
    bar.top = mtr_cursors[6].top;
    bar.x = mtr_list_window.x + 0xEE;
    bar.y = mtr_list_window.y + 3;
    bar.w = 0xA;
    bar.h = 0x32;
    func_80028484(&bar);
    rows = 4;
    spells = members[mtr_member].spells + 56;
    if (!mtr_cursors[6].scrolling)
        rows = 3;
    top = mtr_cursors[6].top;
    for (j = 0; j < rows; j++) {
        if (spells[j + top].id != 0xFF)
            func_80026F44(mtr_list_window.x + 0x38, mtr_list_window.y + 6 + j * 16 + mtr_cursors[6].scroll_step * 4,
                          kind_text(1, spells[j + top].id, 8), 7);
    }
    clip.x = 0;
    clip.y = mtr_list_window.y + 3;
    clip.w = 0x16C;
    clip.h = mtr_list_window.h - 6;
    func_80026A94(&results_draw[results_buf], &clip);
    j = mtr_list_window.y;
    x = mtr_list_window.x + mtr_list_window.w;
    mtr_spell_info(x, j, mtr_cursors[6].col + mtr_cursors[6].row + mtr_cursors[6].top, mtr_member, 1);
    setRECT(&clip, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x7F, &clip);
    func_8001E040(&mtr_list_window);
    func_8001DE0C(&win, x, j, mtr_info_window.w, mtr_info_window.h);
    func_8001E040(&win);
}

/* Check's Enemy Skill: two to a row. */
void mtr_eskill_list(void *table) /* unread */
{
    RECT clip, win;
    ScrollBar bar;
    MemberSpell *spells;
    int top, rows, i, j;

    setRECT(&clip, 0, 5, 0x16C, 0xDB);
    func_80026A94(&results_draw[results_buf], &clip);
    bar.shown = 3;
    bar.total = 0xC;
    bar.top = mtr_cursors[7].top;
    bar.x = mtr_list_window.x + 0xEE;
    bar.y = mtr_list_window.y + 3;
    bar.w = 0xA;
    bar.h = 0x32;
    func_80028484(&bar);
    rows = 4;
    spells = members[mtr_member].spells + 72;
    if (!mtr_cursors[7].scrolling)
        rows = 3;
    top = mtr_cursors[7].top * 2;
    for (j = 0; j < rows; j++) {
        for (i = 0; i < 2; i++) {
            if (spells[j * 2 + i + top].id != 0xFF)
                func_80026F44(mtr_list_window.x + 0x18 + i * 0x78,
                              mtr_list_window.y + 6 + j * 16 + mtr_cursors[7].scroll_step * 4,
                              kind_text(2, spells[j * 2 + i + top].id, 8), 7);
        }
    }
    clip.x = 0;
    clip.y = mtr_list_window.y + 3;
    clip.w = 0x16C;
    clip.h = mtr_list_window.h - 6;
    func_80026A94(&results_draw[results_buf], &clip);
    j = mtr_list_window.y;
    i = mtr_list_window.x + mtr_list_window.w;
    mtr_spell_info(i, j, mtr_cursors[7].col + mtr_cursors[7].row * 2 + mtr_cursors[7].top * 2, mtr_member, 2);
    setRECT(&clip, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x7F, &clip);
    func_8001E040(&mtr_list_window);
    func_8001DE0C(&win, i, j, mtr_info_window.w, mtr_info_window.h);
    func_8001E040(&win);
}

/* MGICMENU's: All's uses count only on spells whose targets allow the whole party. */
void mtr_clear_all_uses(void)
{
    MemberSpell *s;
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 56; j++) {
            s = &members[i].spells[j];
            if (s->id != 0xFF && s->uses && (s->targets & 0xC) != 0xC)
                s->uses = 0;
        }
    }
}

/* STATMENU's stat_slots. */
void mtr_slots(int x, int y, unsigned char *slots, unsigned char *materia)
{
    RECT r;
    int i;

    for (i = 0; i < 8; i++) {
        if (slots[i] && (slots[i] == 3 || slots[i] == 7))
            func_80028CA0(x + i * 16 - 8, y, 0x70, 0x30, 16, 16, 1, 0);
    }
    for (i = 0; i < 8; i++) {
        if (!slots[i])
            continue;
        if (materia[i * 4] != 0xFF)
            func_80028CA0(x + i * 16, y, 0x80, 0x20, 16, 16, (short)func_8002603C(materia[i * 4]), 1);
        if (slots[i] >= 5)
            func_80028CA0(x + i * 16, y, 0x80, 0x30, 16, 16, 1, 0);
        else
            func_80028CA0(x + i * 16, y, 0x70, 0x20, 16, 16, 1, 0);
    }
    func_80026A34(0, 1, 0x3F, 0);
    setRECT(&r, x - 2, y, 0x80, 0xB);
    func_80027B84(&r);
    func_80026A34(0, 1, 0x1F, 0);
}

void mtr_init(void)
{
    int i;

    mtr_state = 1;
    mtr_member = menu_member;
    func_80026448(&mtr_cursors[0], 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 0);
    func_80026448(&mtr_cursors[1], 0, 0, 8, 2, 0, 0, 8, 2, 0, 0, 0, 0, 0);
    func_80026448(&mtr_cursors[3], 0, 0, 1, 10, 0, 0, 1, 200, 0, 0, 0, 0, 0);
    for (i = 0; i < 3; i++) {
        func_80020058(i);
        member_update(i);
    }
    party_effects_sum();
    mtr_801D827C(0x73, 0x9D);
    D_80062D9C = 1;
}

/* The hands by mtr_state (the one a step back blinking), the highlighted command's or spell's description, and
 * Arrange's menu and Trash's question. */
int mtr_draw_cursors(int frame)
{
    RECT tw;
    PartyMember *m;
    int i, y;

    m = &members[mtr_member];
    switch (mtr_state) {
    case 0:
        func_8001EB2C(0x8A, mtr_cursors[0].row * 24 + 0x22);
        break;
    case 1:
        func_8001EB2C(mtr_cursors[1].col * 16 + 0xBC, mtr_cursors[1].row * 24 + 0x22);
        break;
    case 3:
        if (frame & 2)
            func_8001EB2C(mtr_cursors[1].col * 16 + 0xBC, mtr_cursors[1].row * 24 + 0x22);
        func_8001EB2C(0xD0, mtr_cursors[3].row * 12 + 0x64);
        break;
    case 4:
        if (frame & 2)
            func_8001EB2C(0x8A, mtr_cursors[0].row * 24 + 0x22);
        func_8001EB2C(mtr_command_window.x + (mtr_cursors[4].col * 44 - 0x14) + (mtr_cursors[4].col ? 8 : 0),
                      mtr_command_window.y + 8 + mtr_cursors[4].row * 12);
        if (m->commands[mtr_cursors[4].row + mtr_cursors[4].col * 4].id != 0xFF)
            func_80026F44(0x10, mtr_windows[1].y + 6,
                          kind_text(5, m->commands[mtr_cursors[4].row + mtr_cursors[4].col * 4].id, 0), 7);
        break;
    case 5:
        if (frame & 2)
            func_8001EB2C(0x8A, mtr_cursors[0].row * 24 + 0x22);
        func_8001EB2C(mtr_list_window.x + mtr_cursors[5].col * 0x4E - 3,
                      mtr_list_window.y + 8 + mtr_cursors[5].row * 16);
        if (m->spells[mtr_cursors[5].col + mtr_cursors[5].row * 3 + mtr_cursors[5].top * 3].id != 0xFF)
            func_80026F44(
                0x10, mtr_windows[1].y + 6,
                kind_text(0, m->spells[mtr_cursors[5].col + mtr_cursors[5].row * 3 + mtr_cursors[5].top * 3].id, 0), 7);
        break;
    case 6:
        if (frame & 2)
            func_8001EB2C(0x8A, mtr_cursors[0].row * 24 + 0x22);
        func_8001EB2C(mtr_list_window.x + 0x1E, mtr_list_window.y + 8 + mtr_cursors[6].row * 16);
        if (m->spells[mtr_cursors[6].row + 56 + mtr_cursors[6].top].id != 0xFF)
            func_80026F44(0x10, mtr_windows[1].y + 6,
                          kind_text(1, m->spells[mtr_cursors[6].row + 56 + mtr_cursors[6].top].id, 0), 7);
        break;
    case 7:
        if (frame & 2)
            func_8001EB2C(0x8A, mtr_cursors[0].row * 24 + 0x22);
        func_8001EB2C(mtr_list_window.x + mtr_cursors[7].col * 0x78, mtr_list_window.y + 8 + mtr_cursors[7].row * 16);
        if (m->spells[mtr_cursors[7].col + 72 + mtr_cursors[7].row * 2 + mtr_cursors[7].top * 2].id != 0xFF)
            func_80026F44(0x10, mtr_windows[1].y + 6,
                          kind_text(2, m->spells[mtr_cursors[7].col + 72 + mtr_cursors[7].row * 2
                                                 + mtr_cursors[7].top * 2].id, 0),
                          7);
        break;
    case 9:
        func_8001EB2C(0xD0, mtr_cursors[9].row * 12 + 0x64);
    case 8:
        if (mtr_state != 9 || frame & 2)
            func_8001EB2C(mtr_windows[5].x - 0x12, mtr_windows[5].y + 5 + mtr_cursors[8].row * 12);
        for (i = 0; i < 4; i++)
            func_80026F44(mtr_windows[5].x + 8, mtr_windows[5].y + i * 12 + 4, mtr_texts[10 + i], 7);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x7F, &tw);
        func_8001E040(&mtr_windows[5]);
        break;
    case 10:
        if (frame & 2)
            func_8001EB2C(0xD0, mtr_cursors[9].row * 12 + 0x64);
        func_8001EB2C(mtr_windows[6].x + 0x20, mtr_windows[6].y + 0x18 + mtr_cursors[10].row * 12);
        func_80026F44(mtr_windows[6].x + 8, mtr_windows[6].y + 6, mtr_confirm[0], 7);
        func_80026F44(mtr_windows[6].x + 0x40, mtr_windows[6].y + 0x15, mtr_confirm[1], 7);
        func_80026F44(mtr_windows[6].x + 0x40, mtr_windows[6].y + 0x22, mtr_confirm[2], 7);
        func_8001E040(&mtr_windows[6]);
        break;
    }
}

/* The highlighted materia's description, and its id in mtr_highlight (-1 for none). */
int mtr_describe(void)
{
    mtr_highlight = -1;
    switch (mtr_state) {
    case 1:
        func_80026B5C(0x80);
        switch (mtr_cursors[1].row) {
        case 0:
            if (mtr_chars[mtr_record].weapon_materia[mtr_cursors[1].col] != -1)
                func_80026F44(0x10, mtr_windows[1].y + 6,
                              mtr_text(1, mtr_chars[mtr_record].weapon_materia[mtr_cursors[1].col] & 0xFF), 7);
            mtr_highlight = mtr_chars[mtr_record].weapon_materia[mtr_cursors[1].col] & 0xFF;
            break;
        case 1:
            if (mtr_chars[mtr_record].armor_materia[mtr_cursors[1].col] != -1)
                func_80026F44(0x10, mtr_windows[1].y + 6,
                              mtr_text(1, mtr_chars[mtr_record].armor_materia[mtr_cursors[1].col] & 0xFF), 7);
            mtr_highlight = mtr_chars[mtr_record].armor_materia[mtr_cursors[1].col] & 0xFF;
            break;
        }
        break;
    case 3:
        func_80026B5C(0x80);
        if (save_data.materia[mtr_cursors[3].row + mtr_cursors[3].top] != -1)
            func_80026F44(0x10, mtr_windows[1].y + 6,
                          mtr_text(1, save_data.materia[mtr_cursors[3].row + mtr_cursors[3].top] & 0xFF), 7);
        mtr_highlight = save_data.materia[mtr_cursors[3].row + mtr_cursors[3].top] & 0xFF;
        break;
    case 4:
        break;
    }
}

/* The materia under cursor n's page. */
void mtr_page(int n)
{
    materia_detail = 1;
    materia_own(save_data.materia[mtr_cursors[n].row + mtr_cursors[n].top]);
    mtr_draw_materia(save_data.materia[mtr_cursors[n].row + mtr_cursors[n].top], mtr_scroll,
                     &results_draw[results_buf]);
}

/* The stock under cursor n, ten (eleven while scrolling) names with their icons, in a primitive buffer of its own. */
void mtr_stock_list(int n)
{
    RECT clip;
    int rows, i, pal;

    func_800269D0();
    func_800269C0(mtr_prims[results_buf]);
    func_80026B5C(0xA);
    setRECT(&clip, 0, 5, 0x16C, 0xDB);
    func_80026A94(&results_draw[results_buf], &clip);
    rows = 10;
    if (mtr_cursors[n].scrolling)
        rows = 11;
    for (i = 0; i < rows; i++) {
        if (save_data.materia[i + mtr_cursors[n].top] != -1)
            func_80026F44(0xF6, i * 12 + 0x62 + mtr_cursors[n].scroll_step * 3,
                          func_80021258(save_data.materia[i + mtr_cursors[n].top] & 0xFF), 7);
    }
    mtr_bar.shown = 10;
    mtr_bar.total = 200;
    mtr_bar.top = mtr_cursors[n].top;
    mtr_bar.x = 0x160;
    mtr_bar.y = 0x61;
    mtr_bar.w = 10;
    mtr_bar.h = 0x79;
    func_80028484(&mtr_bar);
    for (i = 0; i < rows; i++) {
        pal = func_8002603C(save_data.materia[i + mtr_cursors[n].top] & 0xFF);
        if (save_data.materia[i + mtr_cursors[n].top] != -1)
            func_80028CA0(0xE8, i * 12 + 0x62 + mtr_cursors[n].scroll_step * 3, 0x80, 0x20, 16, 16, (short)pal, 0);
    }
    func_80026A34(0, 1, 0x3F, 0);
    setRECT(&clip, 0, 0x61, 0x16C, 0x79);
    func_80026A94(&results_draw[results_buf], &clip);
    func_800269E8();
}

/* Exchange's stock list. */
void mtr_exchange_stock(void)
{
    RECT clip;
    int rows, i, x, y, pal;

    func_800269D0();
    func_800269C0(mtr_prims[results_buf]);
    func_80026B5C(0xA);
    setRECT(&clip, 0, 5, 0x16C, 0xDB);
    func_80026A94(&results_draw[results_buf], &clip);
    x = 0xF6;
    rows = 10;
    y = 0x3B;
    if (mtr_cursors[11].scrolling)
        rows = 11;
    for (i = 0; i < rows; i++) {
        if (save_data.materia[i + mtr_cursors[11].top] != -1)
            func_80026F44(x, y + i * 16 + mtr_cursors[11].scroll_step * 4,
                          func_80021258(save_data.materia[i + mtr_cursors[11].top] & 0xFF), 7);
    }
    mtr_bar.shown = 10;
    mtr_bar.total = 200;
    mtr_bar.top = mtr_cursors[11].top;
    mtr_bar.x = 0x160;
    mtr_bar.y = 0x38;
    mtr_bar.w = 10;
    mtr_bar.h = 0xA2;
    func_80028484(&mtr_bar);
    for (i = 0; i < rows; i++) {
        pal = func_8002603C(save_data.materia[i + mtr_cursors[11].top] & 0xFF);
        if (save_data.materia[i + mtr_cursors[11].top] != -1)
            func_80028CA0(x - 16, y - 1 + i * 16 + mtr_cursors[11].scroll_step * 4, 0x80, 0x20, 16, 16, (short)pal, 0);
    }
    func_80026A34(0, 1, 0x3F, 0);
    setRECT(&clip, 0, 0x38, 0x16C, 0xA2);
    func_80026A94(&results_draw[results_buf], &clip);
    func_800269E8();
}

/* The right of the screen by mtr_state: a slot's materia page, the stock with the highlighted one's page, or Check's
 * commands with the list the highlighted command opens. */
int mtr_draw_panel(void)
{
    RECT unused;

    switch (mtr_state) {
    case 1:
        mtr_801D2EB4();
        func_80026B5C(0xA);
        materia_detail = 1;
        switch (mtr_cursors[1].row) {
        case 0:
            materia_own(mtr_chars[mtr_record].weapon_materia[mtr_cursors[1].col]);
            mtr_draw_materia(mtr_chars[mtr_record].weapon_materia[mtr_cursors[1].col], mtr_scroll,
                             &results_draw[results_buf]);
            break;
        case 1:
            materia_own(mtr_chars[mtr_record].armor_materia[mtr_cursors[1].col]);
            mtr_draw_materia(mtr_chars[mtr_record].armor_materia[mtr_cursors[1].col], mtr_scroll,
                             &results_draw[results_buf]);
            break;
        }
        func_80026A34(0, 1, 0x3F, 0);
        break;
    case 3:
        mtr_page(3);
        mtr_stock_list(3);
        break;
    case 0:
        if (!mtr_cursors[0].row)
            mtr_commands();
    case 8:
    case 9:
    case 10:
        if (mtr_cursors[0].row) {
            if (mtr_state != 0 && mtr_state != 8)
                mtr_page(9);
            mtr_stock_list(9);
        }
        break;
    case 4:
        mtr_commands();
        break;
    default:
        mtr_commands();
        switch (mtr_command_list[mtr_cursors[4].col * 4 + mtr_cursors[4].row].cursor) {
        case 1:
        case 8:
            mtr_magic_list(spell_data);
            break;
        case 2:
        case 9:
            mtr_summon_list(D_80070EE4);
            break;
        case 4:
            mtr_eskill_list(D_800710A4);
            break;
        }
        break;
    }
}

/* STATMENU's stat_portrait. */
void mtr_header(void)
{
    RECT tw;

    func_80020B68(0x3E, 0x13, mtr_member);
    func_8001CF3C(10, 0xE, 0x30, 0x30, 0, mtr_member * 48 + 0x38, 0x30, 0x30, mtr_member + 0xD, 0);
    setRECT(&tw, 0, 0, 0x100, 0x100);
    func_80026A34(0, 1, 0x7F, &tw);
}

/* Exchange's: swaps the materia of two pieces of equipment where both have a slot. */
void mtr_swap_slots(unsigned char *slots_a, unsigned char *slots_b, int *materia_a, int *materia_b)
{
    int i, t;

    for (i = 0; i < 8; i++) {
        if (slots_a[i] && slots_b[i]) {
            t = materia_a[i];
            materia_a[i] = materia_b[i];
            materia_b[i] = t;
        }
    }
}

/* Exchange's start: the joined characters to pick from, and its cursors. */
void mtr_exchange_open(void)
{
    int i, n;
    unsigned char *p, none;

    none = 0xFF;
    for (i = 8, p = &mtr_joined[8]; i >= 0; i--)
        *p-- = none;
    for (i = 0, n = 0; i < 9; i++) {
        if (D_8009D78A[0] >> i & 1)
            mtr_joined[n++] = i;
    }
    mtr_joined_count = n;
    func_80026448(&mtr_cursors[11], 0, 0, 1, 10, 0, 0, 1, 200, 0, 0, 0, 0, 0);
    func_80026448(&mtr_cursors[12], 0, 0, 8, 2, 0, 0, 8, 2, 0, 0, 0, 0, 0);
    func_80026448(&mtr_cursors[13], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 0, 0);
}

/* A materia's name and icon at x, y, and its stars at the top; returns its icon's palette. */
int mtr_draw_name(int x, int y, int m)
{
    RECT tw;
    int pal, i;

    func_80026F44(x + 0x10, y, func_80021258(m & 0xFF), 7);
    pal = func_8002603C(m & 0xFF);
    func_80028CA0(x, y - 1, 0x80, 0x20, 16, 16, (short)pal, 0);
    func_80026A34(0, 1, 0x3F, 0);
    materia_own(m);
    if ((m & 0xFF) != 0x30 && (m & 0xFF) != 0x49 && (m & 0xFF) != 0x5A && (m & 0xFF) != 0x2C) {
        for (i = 0; i < mtr_info->levels; i++) {
            if (i < mtr_info->level)
                func_80028CA0(0xAA + i * 15, 0xB, 0x90, 0x20, 16, 16, (short)pal, 0);
            else
                func_80028CA0(0xAA + i * 15, 0xB, 0x90, 0x30, 16, 16, (short)pal, 0);
        }
    }
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x7F, &tw);
    return pal;
}

/* Exchange: the joined characters' weapon and armor slots, four at a time and scrolling, the first pick blinking, the
 * stock, and the highlighted materia's description, name and stars. */
#ifdef NON_MATCHING
/* 3 instructions short of the game's 456: the game adds the rows' y offsets in another order and keeps one more
 * register. */
void mtr_exchange_draw(int frame)
{
    RECT win, clip;
    Character *c;
    int more, rows, i, y, d, n;

    more = mtr_joined_count - 4;
    c = &save_data.chars[mtr_joined[mtr_exch_top + mtr_exch_row]];
    if (mtr_exch_picked) {
        switch (mtr_exch_kind) {
        case 0:
            if (frame & 2) {
                d = mtr_exch_char - mtr_exch_top;
                if (d >= 0)
                    func_8001EB2C(mtr_exch_col * 16 + 0x29,
                                  mtr_exch_slot_row * 12 + (d * 42 + 0x44) + mtr_exch_step * 5);
            }
            break;
        case 2:
            if (frame & 2) {
                d = mtr_exch_char - mtr_exch_top;
                if (d >= 0)
                    func_8001EB2C(7, mtr_exch_slot_row * 12 + (d * 42 + 0x38) + mtr_exch_step * 5);
            }
            break;
        }
    }
    materia_detail = 1;
    switch (mtr_exch_state) {
    case 0:
        func_8001EB2C(mtr_cursors[12].col * 16 + 0x2B, mtr_cursors[12].row * 12 + (mtr_exch_row * 42 + 0x46));
        func_80026B5C(0x80);
        switch (mtr_cursors[12].row) {
        case 0:
            if (c->weapon_materia[mtr_cursors[12].col] != -1) {
                func_80026F44(0x10, 0x23, mtr_text(1, c->weapon_materia[mtr_cursors[12].col] & 0xFF), 7);
                mtr_draw_name(0x10, 0xB, c->weapon_materia[mtr_cursors[12].col]);
            }
            break;
        case 1:
            if (c->armor_materia[mtr_cursors[12].col] != -1) {
                func_80026F44(0x10, 0x23, mtr_text(1, c->armor_materia[mtr_cursors[12].col] & 0xFF), 7);
                mtr_draw_name(0x10, 0xB, c->armor_materia[mtr_cursors[12].col]);
            }
            break;
        }
        break;
    case 1:
        func_8001EB2C(mtr_windows[9].x - 0xB, mtr_windows[9].y + 8 + mtr_cursors[11].row * 16);
        if (save_data.materia[mtr_cursors[11].row + mtr_cursors[11].top] != -1) {
            func_80026F44(0x10, 0x23, mtr_text(1, save_data.materia[mtr_cursors[11].row + mtr_cursors[11].top] & 0xFF),
                          7);
            mtr_draw_name(0x10, 0xB, save_data.materia[mtr_cursors[11].row + mtr_cursors[11].top]);
        }
        break;
    case 2:
        func_8001EB2C(9, mtr_cursors[13].row * 12 + (mtr_exch_row * 42 + 0x3A));
        break;
    }
    mtr_exchange_stock();
    rows = 5;
    func_8001E040(&mtr_windows[7]);
    func_8001E040(&mtr_windows[8]);
    func_8001E040(&mtr_windows[9]);
    if (!mtr_exch_scrolling)
        rows = 4;
    setRECT(&clip, 0, 5, 0x16C, 0xDB);
    func_80026A94(&results_draw[results_buf], &clip);
    for (i = 0; i < rows; i++) {
        n = mtr_exch_top + i;
        y = i * 42 + mtr_exch_step * 5 + 0x35;
        if (mtr_joined[n] != 0xFF) {
            mtr_slots(0x43, y + 0xF, weapon_table[mtr_chars[mtr_joined[n]].weapon].slots,
                      (unsigned char *)mtr_chars[mtr_joined[n]].weapon_materia);
            mtr_slots(0x43, y + 0x1B, armor_table[mtr_chars[mtr_joined[n]].armor].slots,
                      (unsigned char *)mtr_chars[mtr_joined[n]].armor_materia);
            setRECT(&clip, 0, 0, 0xFF, 0xFF);
            func_80026A34(0, 1, 0x7F, &clip);
            func_80026F44(0x21, y + 4, save_data.chars[mtr_joined[n]].name, 7);
            func_80026F44(0x21, y + 0x10, mtr_texts[0], 5);
            func_80026F44(0x21, y + 0x1C, mtr_texts[1], 5);
        }
        func_8001DE0C(&win, 0, y, 0xCD, 0x2A);
        func_8001E040(&win);
    }
    setRECT(&clip, 0, 0x35, 0x16C, 0xA8);
    func_80026A94(&results_draw[results_buf], &clip);
    if (more > 0) {
        mtr_bar.shown = 4;
        mtr_bar.total = mtr_joined_count;
        mtr_bar.top = mtr_exch_top;
        mtr_bar.x = mtr_windows[10].x + 6;
        mtr_bar.y = 0x38;
        mtr_bar.w = 0xA;
        mtr_bar.h = 0xA2;
        func_80028484(&mtr_bar);
    }
    func_8001E040(&mtr_windows[10]);
}
#else
INCLUDE_ASM("asm/menu/eqipmenu/nonmatchings/eqipmateria", mtr_exchange_draw);
#endif

/* The character on Exchange's highlighted row, and the one the first pick was made on. */
#define EXCH_HERE save_data.chars[mtr_joined[mtr_exch_top + mtr_exch_row]]
#define EXCH_PICK save_data.chars[mtr_joined[mtr_exch_char]]

/* Exchange's buttons: ○ picks a slot, a set (a row's weapon and armor, or both) or a stock place, and on the second
 * pick swaps them; △ sends a slot's materia back to the stock; ✕ drops the first pick or leaves; the list scrolls
 * a character at a time over 8 frames. The armor's slot test reads the slot cursor's column of the main screen. */
#ifdef NON_MATCHING
/* 12 instructions short of the game's 1,047: the slot tests and the swaps' address sums come out in another order. */
void mtr_exchange_input(void)
{
    RECT unused[3];
    int last, more, m, t, row, col;
    int *first, *second;
    unsigned char *slots_a, *slots_b;

    last = 3;
    if (mtr_joined_count < 5)
        last = mtr_joined_count - 1;
    more = mtr_joined_count - 4;
    if (!mtr_exch_scrolling) {
        if (D_80062D7C & 0x40) {
            if (mtr_exch_picked)
                goto unpick;
            mtr_sound(4);
            mtr_exchanging = 0;
            mtr_state = 8;
            for (m = 0; m < 3; m++) {
                func_80020058(m);
                member_update(m);
            }
            party_effects_sum();
            return;
        }
        if (D_80062D7C & 0x10 && !mtr_exch_state) {
            switch (mtr_cursors[12].row) {
            case 0:
                if (!weapon_table[EXCH_HERE.weapon].slots[mtr_cursors[12].col])
                    goto buzz;
                first = (int *)&EXCH_HERE.weapon_materia[mtr_cursors[12].col];
                if (*first == -1)
                    goto buzz;
                m = func_8002542C(*first);
                if (m == -1)
                    goto stocked;
                goto buzz;
            case 1:
                if (!armor_table[EXCH_HERE.armor].slots[mtr_cursors[1].col])
                    goto buzz;
                first = (int *)&EXCH_HERE.armor_materia[mtr_cursors[12].col];
                if (*first == -1)
                    goto buzz;
                m = func_8002542C(*first);
                if (m != -1)
                    goto buzz;
            stocked:
                mtr_sound(0x1BF);
                *first = m;
                break;
            }
            return;
        }
        if (D_80062D7C & 0x20) {
            if (!mtr_exch_picked) {
                switch (mtr_exch_kind = mtr_exch_state) {
                case 0:
                    if (!mtr_cursors[12].row)
                        slots_a = weapon_table[EXCH_HERE.weapon].slots;
                    else
                        slots_a = armor_table[EXCH_HERE.armor].slots;
                    if (!slots_a[mtr_cursors[12].col])
                        goto buzz;
                    mtr_exch_col = mtr_cursors[12].col;
                    mtr_exch_picked = 1;
                    mtr_exch_slot_row = mtr_cursors[12].row;
                    mtr_exch_char = mtr_exch_top + mtr_exch_row;
                    mtr_sound(1);
                    break;
                case 2:
                    mtr_exch_col = 0;
                    mtr_exch_picked = 1;
                    mtr_exch_slot_row = mtr_cursors[13].row;
                    mtr_exch_char = mtr_exch_top + mtr_exch_row;
                    break;
                }
                return;
            }
            switch (mtr_exch_kind) {
            case 0:
                if (!mtr_exch_state) {
                    if (!mtr_cursors[12].row)
                        slots_a = weapon_table[EXCH_HERE.weapon].slots;
                    else
                        slots_a = armor_table[EXCH_HERE.armor].slots;
                    if (!slots_a[mtr_cursors[12].col])
                        goto buzz;
                }
                if (!mtr_exch_slot_row)
                    first = (int *)&EXCH_PICK.weapon_materia[mtr_exch_col];
                else
                    first = (int *)&EXCH_PICK.armor_materia[mtr_exch_col];
                switch (mtr_exch_state) {
                case 0:
                    if (!mtr_cursors[12].row)
                        second = (int *)&EXCH_HERE.weapon_materia[mtr_cursors[12].col];
                    else
                        second = (int *)&EXCH_HERE.armor_materia[mtr_cursors[12].col];
                    break;
                case 1:
                    second = &save_data.materia[mtr_cursors[11].row + mtr_cursors[11].top];
                    break;
                }
                t = *first;
                *first = *second;
                *second = t;
            unpick:
                mtr_exch_picked = 0;
                if (mtr_exch_state == 1)
                    mtr_exch_state = 0;
                break;
            case 2:
                switch (mtr_exch_slot_row) {
                case 0:
                    if (mtr_cursors[13].row)
                        goto buzz;
                    mtr_swap_slots(weapon_table[EXCH_PICK.weapon].slots, weapon_table[EXCH_HERE.weapon].slots,
                                   EXCH_PICK.weapon_materia, EXCH_HERE.weapon_materia);
                    mtr_swap_slots(armor_table[EXCH_PICK.armor].slots, armor_table[EXCH_HERE.armor].slots,
                                   EXCH_PICK.armor_materia, EXCH_HERE.armor_materia);
                    mtr_exch_picked = 0;
                    break;
                case 1:
                case 2:
                    if (!mtr_cursors[13].row) {
                    buzz:
                        mtr_sound(3);
                        break;
                    }
                    if (mtr_exch_slot_row == 1) {
                        first = (int *)EXCH_PICK.weapon_materia;
                        slots_a = weapon_table[EXCH_PICK.weapon].slots;
                    } else {
                        first = (int *)EXCH_PICK.armor_materia;
                        slots_a = armor_table[EXCH_PICK.armor].slots;
                    }
                    if (mtr_cursors[13].row == 1) {
                        second = (int *)EXCH_HERE.weapon_materia;
                        slots_b = weapon_table[EXCH_HERE.weapon].slots;
                    } else {
                        second = (int *)EXCH_HERE.armor_materia;
                        slots_b = armor_table[EXCH_HERE.armor].slots;
                    }
                    mtr_swap_slots(slots_a, slots_b, first, second);
                    mtr_exch_picked = 0;
                    break;
                }
                break;
            }
            return;
        }
        switch (mtr_exch_state) {
        case 0:
            row = mtr_cursors[12].row;
            col = mtr_cursors[12].col;
            func_800264A8(&mtr_cursors[12]);
            if (D_80062D7E & 0x2000 && col == mtr_cursors[12].col) {
                if (!mtr_exch_picked)
                    break;
                mtr_sound(1);
                mtr_exch_state = 1;
            } else if (D_80062D7E & 0x4000 && row == mtr_cursors[12].row) {
                if (mtr_exch_row < last) {
                    mtr_sound(1);
                    mtr_cursors[12].row = 0;
                    mtr_exch_row++;
                } else if (mtr_exch_row == 3 && mtr_exch_top < more) {
                    mtr_sound(1);
                    mtr_exch_scrolling = 1;
                    mtr_exch_step = -1;
                    mtr_cursors[12].row = 0;
                }
            } else if (D_80062D7E & 0x1000 && row == mtr_cursors[12].row) {
                if (mtr_exch_row) {
                    mtr_sound(1);
                    mtr_cursors[12].row = 1;
                    mtr_exch_row--;
                } else if (mtr_exch_top > 0) {
                    mtr_sound(1);
                    mtr_exch_scrolling = 2;
                    mtr_exch_step = -7;
                    mtr_cursors[12].row = 1;
                    mtr_exch_top--;
                }
            } else if (D_80062D7E & 0x8000 && col == mtr_cursors[12].col && !mtr_exch_picked) {
                mtr_sound(1);
                mtr_exch_state = 2;
                mtr_cursors[13].row = mtr_cursors[12].row + 1;
            }
            break;
        case 1:
            func_800264A8(&mtr_cursors[11]);
            if (D_80062D7C & 0x8000 && !mtr_cursors[11].scrolling) {
                mtr_sound(1);
                mtr_exch_state = 0;
            }
            break;
        case 2:
            row = mtr_cursors[13].row;
            func_800264A8(&mtr_cursors[13]);
            if (D_80062D7C & 0x2000 && mtr_cursors[13].row && !mtr_exch_picked) {
                mtr_sound(1);
                mtr_exch_state = 0;
                mtr_cursors[12].row = mtr_cursors[13].row - 1;
            } else if (D_80062D7E & 0x4000 && row == mtr_cursors[13].row) {
                if (mtr_exch_row < last) {
                    mtr_sound(1);
                    mtr_cursors[13].row = 0;
                    mtr_exch_row++;
                } else if (mtr_exch_row == 3 && mtr_exch_top < more) {
                    mtr_sound(1);
                    mtr_exch_scrolling = 1;
                    mtr_exch_step = -1;
                    mtr_cursors[13].row = 0;
                }
            } else if (D_80062D7E & 0x1000 && row == mtr_cursors[13].row) {
                if (mtr_exch_row) {
                    mtr_sound(1);
                    mtr_cursors[13].row = 2;
                    mtr_exch_row--;
                } else if (mtr_exch_top > 0) {
                    mtr_sound(1);
                    mtr_exch_scrolling = 2;
                    mtr_exch_step = -7;
                    mtr_cursors[13].row = 2;
                    mtr_exch_top--;
                }
            }
            break;
        }
    } else {
        switch (mtr_exch_scrolling) {
        case 2:
            if (++mtr_exch_step == 0) {
                mtr_exch_step = 0;
                mtr_exch_scrolling = 0;
            }
            break;
        case 1:
            if (--mtr_exch_step == -8) {
                mtr_exch_step = 0;
                mtr_exch_scrolling = 0;
                mtr_exch_top++;
            }
            break;
        }
    }
}
#else
INCLUDE_ASM("asm/menu/eqipmenu/nonmatchings/eqipmateria", mtr_exchange_input);
#endif

#undef EXCH_HERE
#undef EXCH_PICK

/* The screen each frame, by mtr_state (see its declaration); Exchange runs in place of the rest. A locked menu
 * (D_8009D4F7 bit 0) greys Arrange and keeps the slots as they are, with a caption on Sephiroth. */
#ifdef NON_MATCHING
/* 4 instructions short of the game's 1,260: Check's cursor loop is laid out differently, and the game re-reads the
 * pad between case 0's two tests. */
int mtr_frame(int frame)
{
    RECT tw, unused[6];
    int i, locked, col, row, dcol, drow, m;
    unsigned char *slots;

    mtr_scroll = frame;
    if (!(frame & 0xFF))
        mtr_801D7FEC();
    mtr_info = (MateriaInfo *)mtr_info_buf;
    materia_info = (MateriaInfo *)mtr_info_buf;
    locked = D_8009D4F7 & 1;
    /* the byte after the buffer, again and again: mtr_cursors[0]'s first */
    for (i = 0; i < 0x80; i++)
        mtr_info_buf[0x80] = 0xFF;
    mtr_command_list = members[mtr_member].commands;
    mtr_record = character_record[save_data.party[mtr_member]];
    mtr_chars = save_data.chars;
    func_800230C4(results_buf);
    if (!mtr_exchanging) {
        mtr_draw_cursors(mtr_scroll);
        mtr_describe();
        mtr_draw_panel();
        setRECT(&tw, 0, 0, 0xFF, 0xFF);
        func_80026A34(0, 1, 0x7F, &tw);
        mtr_header();
        func_80026B5C(0xA);
        func_80026F44(0xBA, 0x14, func_80021280(mtr_chars[mtr_record].weapon + 0x80), 7);
        func_80026F44(0xBA, 0x2C, func_80021280(mtr_chars[mtr_record].armor + 0x100), 7);
        func_80026F44(0x9A, 0x14, mtr_texts[0], 5);
        func_80026F44(0x9A, 0x2C, mtr_texts[1], 5);
        func_80026F44(0xA2, 0x21, mtr_texts[4], 7);
        func_80026F44(0xA2, 0x39, mtr_texts[5], locked ? 0 : 7);
        mtr_slots(0xD6, 0x20, weapon_table[mtr_chars[mtr_record].weapon].slots,
                  (unsigned char *)mtr_chars[mtr_record].weapon_materia);
        mtr_slots(0xD6, 0x38, armor_table[mtr_chars[mtr_record].armor].slots,
                  (unsigned char *)mtr_chars[mtr_record].armor_materia);
        setRECT(&tw, 0, 0, 0xFF, 0xFF);
        func_80026A34(0, 1, 0x7F, &tw);
        for (i = 0; i < 2; i++)
            func_8001E040(&mtr_windows[i]);
        switch (mtr_state) {
        case 1:
        case 3:
        case 8:
        case 9:
        two:
            func_8001E040(&mtr_windows[2]);
            func_8001E040(&mtr_windows[3]);
            break;
        default:
            if (mtr_cursors[0].row)
                goto two;
            func_8001E040(&mtr_windows[4]);
            break;
        }
    } else
        mtr_exchange_draw(frame);
    if (func_80023050())
        return;
    if (func_8001F6B4())
        return;
    switch (mtr_state) {
    case 0:
    case 1:
    case 8:
        if (D_80062D7E & 4) {
            do {
                if (--mtr_member < 0)
                    mtr_member = 2;
            } while (save_data.party[mtr_member] == 0xFF);
        } else if (D_80062D7E & 8) {
            do {
                if (++mtr_member >= 3)
                    mtr_member = 0;
            } while (save_data.party[mtr_member] == 0xFF);
        }
        break;
    }
    switch (mtr_state) {
    case 1:
        col = mtr_cursors[1].col;
        func_800264A8(&mtr_cursors[1]);
        break;
    case 4:
        col = mtr_cursors[4].col;
        row = mtr_cursors[4].row;
        func_800264A8(&mtr_cursors[4]);
        dcol = mtr_cursors[4].col - col;
        drow = mtr_cursors[4].row - row;
        /* on past the empty places, the way the cursor moved */
        for (;;) {
            if (!drow && !dcol) {
                if (!mtr_command_list[mtr_cursors[4].col * 4 + mtr_cursors[4].row].pad3)
                    break;
                mtr_cursors[4].row = 0;
                mtr_cursors[4].col = 0;
            }
            if (!mtr_command_list[mtr_cursors[4].col * 4 + mtr_cursors[4].row].pad3)
                break;
            mtr_cursors[4].row += drow;
            mtr_cursors[4].col += dcol;
            mtr_cursors[4].row &= 3;
            mtr_cursors[4].col %= members[mtr_member].command_rows;
        }
        break;
    case 11:
        break;
    default:
        func_800264A8(&mtr_cursors[mtr_state]);
        break;
    }
    switch (mtr_state) {
    case 0:
        if (D_80062D7C & 0x2040) {
            mtr_state = 1;
            mtr_cursors[1].row = mtr_cursors[0].row;
            break;
        }
        if (D_80062D7C & 0x20) {
            if (!mtr_cursors[0].row) {
                mtr_sound(1);
                mtr_state = 4;
                func_80026448(&mtr_cursors[4], 0, 0, members[mtr_member].command_rows, 4, 0, 0,
                              members[mtr_member].command_rows, 4, 0, 0, 1, 1, 0);
            } else {
                if (locked)
                    goto buzz;
                mtr_sound(1);
                mtr_state = 8;
                func_80026448(&mtr_cursors[8], 0, 0, 1, 4, 0, 0, 1, 4, 0, 0, 0, 1, 0);
                func_80026448(&mtr_cursors[9], 0, 0, 1, 10, 0, 0, 1, 200, 0, 0, 0, 0, 0);
            }
        }
        break;
    case 1:
        if (D_80062D7C & 0x8000 && col == mtr_cursors[1].col && !col) {
            mtr_state = 0;
            mtr_cursors[0].row = mtr_cursors[1].row;
        } else if (D_80062D7C & 0x20) {
            switch (mtr_cursors[1].row) {
            case 0:
                slots = func_800256DC(mtr_member);
                break;
            case 1:
                slots = armor_slots(mtr_member);
                break;
            }
            if (!slots[mtr_cursors[1].col])
                goto no_slot;
            if (locked)
                goto no_change;
            mtr_sound(1);
            mtr_state = 3;
        } else if (D_80062D7C & 0x10) {
            switch (mtr_cursors[1].row) {
            case 0:
                if (!func_800256DC(mtr_member)[mtr_cursors[1].col])
                    goto no_slot;
                if (locked)
                    goto no_change;
                if (mtr_chars[mtr_record].weapon_materia[mtr_cursors[1].col] == -1)
                    goto buzz;
                m = func_8002542C(mtr_chars[mtr_record].weapon_materia[mtr_cursors[1].col]);
                if (m != -1)
                    goto buzz;
                mtr_sound(0x1BF);
                mtr_chars[mtr_record].weapon_materia[mtr_cursors[1].col] = m;
                goto update;
            case 1:
                if (!armor_slots(mtr_member)[mtr_cursors[1].col])
                    goto no_slot;
                if (locked)
                    goto no_change;
                if (mtr_chars[mtr_record].armor_materia[mtr_cursors[1].col] == -1)
                    goto buzz;
                m = func_8002542C(mtr_chars[mtr_record].armor_materia[mtr_cursors[1].col]);
                if (m != -1)
                    goto buzz;
                mtr_sound(0x1BF);
                mtr_chars[mtr_record].armor_materia[mtr_cursors[1].col] = m;
                goto update;
            }
            break;
        no_slot:
            if (!locked)
                goto buzz;
        no_change:
            if (save_data.party[mtr_member] == 10)
                func_8001F6C0(mtr_sephiroth_text, 7);
            goto buzz;
        } else if (D_80062D7C & 0x40) {
            mtr_sound(4);
            func_8002305C(5, 0);
            func_8002120C(0);
            mtr_801D7FDC();
        } else if (D_80062D7C & 0x80) {
            mtr_sound(1);
            func_8002305C(4, 4);
            func_8002120C(4);
            menu_member = mtr_member;
            mtr_801D7FDC();
        }
        break;
    case 3:
        if (mtr_cursors[3].scrolling)
            break;
        if (D_80062D7C & 0x20) {
            mtr_sound(0x1BF);
            switch (mtr_cursors[1].row) {
            case 0:
                m = mtr_chars[mtr_record].weapon_materia[mtr_cursors[1].col];
                mtr_chars[mtr_record].weapon_materia[mtr_cursors[1].col] =
                    save_data.materia[mtr_cursors[3].top + mtr_cursors[3].row];
                mtr_state = 1;
                save_data.materia[mtr_cursors[3].top + mtr_cursors[3].row] = m;
                break;
            case 1:
                m = mtr_chars[mtr_record].armor_materia[mtr_cursors[1].col];
                mtr_state = 1;
                mtr_chars[mtr_record].armor_materia[mtr_cursors[1].col] =
                    save_data.materia[mtr_cursors[3].top + mtr_cursors[3].row];
                save_data.materia[mtr_cursors[3].top + mtr_cursors[3].row] = m;
                break;
            }
            goto update;
        }
        if (D_80062D7C & 0x40) {
            mtr_sound(4);
            mtr_state = 1;
        }
        break;
    case 4:
        if (D_80062D7C & 0x20) {
            mtr_sound(1);
            switch (mtr_command_list[mtr_cursors[4].col * 4 + mtr_cursors[4].row].cursor) {
            case 1:
            case 8:
                mtr_clear_all_uses();
                mtr_state = 5;
                func_80026448(&mtr_cursors[5], 0, 0, 3, 3, 0, 0, 3, 0x12, 0, 0, 2, 0, 0);
                break;
            case 2:
            case 9:
                mtr_state = 6;
                func_80026448(&mtr_cursors[6], 0, 0, 1, 3, 0, 0, 1, 0x10, 0, 0, 0, 0, 0);
                break;
            case 4:
                mtr_state = 7;
                func_80026448(&mtr_cursors[7], 0, 0, 2, 3, 0, 0, 2, 0xC, 0, 0, 2, 0, 0);
                break;
            }
        } else if (D_80062D7C & 0x40) {
            mtr_sound(4);
            mtr_state = 0;
        }
        break;
    case 5:
        if (mtr_cursors[5].scrolling)
            break;
        if (D_80062D7C & 0x40) {
            mtr_sound(4);
            mtr_state = 4;
        }
        break;
    case 6:
        if (mtr_cursors[6].scrolling)
            break;
        if (D_80062D7C & 0x40) {
            mtr_sound(4);
            mtr_state = 4;
        }
        break;
    case 7:
        if (mtr_cursors[7].scrolling)
            break;
        if (D_80062D7C & 0x40) {
            mtr_sound(4);
            mtr_state = 4;
        }
        break;
    case 8:
        if (D_80062D7C & 0x20) {
            switch (m = mtr_cursors[8].row) {
            case 0:
                mtr_sound(1);
                mtr_arrange(0);
                break;
            case 1:
                mtr_exchange_open();
                mtr_sound(1);
                mtr_state = 11;
                mtr_exchanging = m;
                break;
            case 2:
                /* Remove all: what the stock has room for */
                mtr_sound(0x1BF);
                for (i = 0; i < 8; i++) {
                    m = save_data.chars[character_record[save_data.party[mtr_member]]].weapon_materia[i];
                    if (m != -1 && func_8002542C(m) != m)
                        save_data.chars[character_record[save_data.party[mtr_member]]].weapon_materia[i] = -1;
                    m = save_data.chars[character_record[save_data.party[mtr_member]]].armor_materia[i];
                    if (m != -1 && func_8002542C(m) != m)
                        save_data.chars[character_record[save_data.party[mtr_member]]].armor_materia[i] = -1;
                }
            update:
                func_80020058(mtr_member);
                member_update(mtr_member);
                party_effects_sum();
                break;
            case 3:
                mtr_sound(1);
                mtr_state = 9;
                break;
            }
        } else if (D_80062D7C & 0x40) {
            mtr_sound(4);
            mtr_state = 0;
        }
        break;
    case 9:
        if (mtr_cursors[9].scrolling)
            break;
        if (D_80062D7C & 0x20) {
            if (save_data.materia[mtr_cursors[9].row + mtr_cursors[9].top] == -1)
                goto buzz;
            mtr_sound(1);
            func_80026448(&mtr_cursors[10], 0, 1, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 0);
            mtr_state = 10;
            break;
        buzz:
            mtr_sound(3);
        } else if (D_80062D7C & 0x40) {
            mtr_sound(4);
            mtr_state = 8;
        }
        break;
    case 10:
        if (D_80062D7C & 0x20) {
            if (!mtr_cursors[10].row) {
                mtr_sound(1);
                mtr_state = 9;
                save_data.materia[mtr_cursors[9].row + mtr_cursors[9].top] = -1;
            } else
                mtr_sound(1);
            mtr_state = 9;
        } else if (D_80062D7C & 0x40) {
            mtr_sound(4);
            mtr_state = 9;
        }
        break;
    case 11:
        mtr_exchange_input();
        break;
    }
}
#else
INCLUDE_ASM("asm/menu/eqipmenu/nonmatchings/eqipmateria", mtr_frame);
#endif

void mtr_801D7FDC(void)
{
    D_80062D9C = 0;
}

void mtr_801D7FEC(void)
{
}

/* L2 and R2 turn the unused model below, which spins on its own. */
void mtr_801D7FF4(void)
{
    if (field_pad2 & 8)
        mtr_model_pos.vy -= 0x80;
    if (field_pad2 & 2)
        mtr_model_pos.vy += 0x80;
    mtr_model_rot.vx += 0x20;
}

/* Unused: the first triangle of a TMD (libgs's TMD reader) as a textured triangle in each of two buffers. */
void mtr_801D8070(unsigned int *tmd, MtrModel *m)
{
    TMD_PRIM p;
    RECT unused;
    MtrTri *tri;
    POLY_FT3 *prim, *copy;
    int count, i;

    tri = m->tris;
    prim = m->prims[0];
    copy = m->prims[1];
    count = func_80046D74(tmd, 0);
    if (count >= 2)
        count = 1;
    for (i = 0; i < count; i++, tri++, prim++, copy++) {
        if (!func_80046DB4(&p))
            break;
        SetPolyFT3(prim);
        func_80046870(prim, 1);
        tri->n[0].vx = p.n0.vx;
        tri->n[0].vy = p.n0.vy;
        tri->n[0].vz = p.n0.vz;
        tri->n[1].vx = p.n1.vx;
        tri->n[1].vy = p.n1.vy;
        tri->n[1].vz = p.n1.vz;
        tri->n[2].vx = p.n2.vx;
        tri->n[2].vy = p.n2.vy;
        tri->n[2].vz = p.n2.vz;
        tri->v[0].vx = p.x0.vx;
        tri->v[0].vy = p.x0.vy;
        tri->v[0].vz = p.x0.vz;
        tri->v[1].vx = p.x1.vx;
        tri->v[1].vy = p.x1.vy;
        tri->v[1].vz = p.x1.vz;
        tri->v[2].vx = p.x2.vx;
        tri->v[2].vy = p.x2.vy;
        tri->v[2].vz = p.x2.vz;
        prim->clut = GetClut(0, 0x1FE);
        prim->tpage = GetTPage(1, 1, 0x340, 0);
        *copy = *prim;
    }
    m->count = i;
}

void mtr_801D827C(int x, int y)
{
}

void mtr_801D8284(void)
{
}

void mtr_801D828C(void)
{
}

void mtr_801D8294(void)
{
}

void mtr_801D829C(void)
{
}
