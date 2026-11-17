/* FORMMENU.MNU, 0x801D0198-0x801D1CF8 (unread strings before it, data after it to 0x801DEA74): the PHS. The party on
 * the left, the other joined characters in a 3 by 3 grid; ○ on one and then another swaps them, the same one twice
 * shows its equipment and experience. The main menu runs it through form_init and form_frame (module 9); the field
 * opens it with form_run, which loads it as module 14, fades in and out and runs its own loop, also for splitting the
 * party into groups at the end of the game. GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

short func_8002603C(int materia); /* its icon's palette: an int, but declared short where this file was built */

/* The save around the party: menu.c's view of it, with the PHS's bits. */
extern struct {
    unsigned char party[4]; /* three character ids, 0xFF for an empty place */
    unsigned char pad4[0xBA8];
    unsigned short locked; /* +0xBAC, bit n: character n can't be moved */
    unsigned short joined; /* bit n: character n has joined */
} D_8009CBDC;

extern unsigned char D_80077F64[2][0x3400]; /* the menus' primitive buffers */

extern int form_fade_level;
extern unsigned char form_text[6][38];
extern unsigned char form_equip_names[4][22]; /* "Wpn.", "Arm.", "Acc.", "Limit level:" */
extern int form_state_prev;
extern int form_state; /* 0 the party, 1 the others, 2 one's equipment and experience */
extern RECT form_title_window;
extern RECT form_windows[4];
extern int form_needed; /* the party's size to leave with */
extern unsigned int *form_ot;
extern unsigned int form_ots[2];
extern MenuCursor form_cursors[2];
extern DRAWENV form_draw[2];
extern DISPENV form_disp[2];
extern unsigned char form_reserve[9]; /* the others' ids, in the grid's order; 0xFF for none */
extern int form_picked;  /* a first character is picked */
extern int form_fading;  /* run from the field: fades in and out */
extern int form_shown;   /* the character whose equipment is shown */
extern int form_split;   /* form_run's argument when splitting the party, else 0 */
extern unsigned char form_first_id, form_second_id;
extern unsigned char *form_first, *form_second; /* where the picked ones are */
extern unsigned char form_vram_faces[0x5000];
extern unsigned char form_vram_portraits[0x5A00];
extern unsigned char form_vram_saved[0xC00];
extern int form_first_state; /* the list the first one was picked from */
extern int form_buf;
extern int form_phase; /* 0 fading in, 1 running, 2 fading out, -1 done */
extern int form_first_col, form_first_row;

void func_80026B5C(int unused); /* empty in the exe */

void form_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

/* Darkens the screen by the fade's level, then steps it; returns the new level. */
int form_fade(int step)
{
    RECT tw;

    setlen(prim_next.tile, 3);
    setcode(prim_next.tile, 0x60);
    func_80046848(prim_next.tile, 1);
    prim_next.tile->x0 = 0;
    prim_next.tile->y0 = 0;
    prim_next.tile->w = 0x180;
    prim_next.tile->h = 0xE8;
    prim_next.tile->r0 = form_fade_level;
    prim_next.tile->g0 = form_fade_level;
    prim_next.tile->b0 = form_fade_level;
    AddPrim(prim_ot, prim_next.tile++);
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x5F, &tw);
    form_fade_level += step;
    if (form_fade_level < 0)
        form_fade_level = 0;
    if (form_fade_level >= 0x100)
        form_fade_level = 0xFF;
    return form_fade_level;
}

/* STATMENU's stat_slots again. */
void form_slots(int x, int y, unsigned char *slots, unsigned char *materia)
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
            func_80028CA0(x + i * 16, y, 0x80, 0x20, 16, 16, func_8002603C(materia[i * 4]), 1);
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

void form_equipment(int x, int y)
{
    Character *chars, *c; /* chars: the game's code reads the table through a register */

    chars = D_8009C738;
    c = &chars[form_shown];
    func_80026F44(x + 0x38, y, func_80021280(c->weapon + 0x80), 7);
    func_80026F44(x + 0x38, y + 0x20, func_80021280(c->armor + 0x100), 7);
    if (c->accessory != 0xFF)
        func_80026F44(x + 0x38, y + 0x40, func_80021280(c->accessory + 0x120), 7);
    func_80026F44(x + 0x10, y, form_equip_names[0], 5);
    func_80026F44(x + 0x10, y + 0x20, form_equip_names[1], 5);
    func_80026F44(x + 0x10, y + 0x40, form_equip_names[2], 5);
    form_slots(x + 0x30, y + 0x10, weapon_table[c->weapon].slots, (unsigned char *)c->weapon_materia);
    form_slots(x + 0x30, y + 0x30, armor_table[c->armor].slots, (unsigned char *)c->armor_materia);
}

/* The shown character's experience: its bar, the total and what the next level needs, right-aligned labels. */
#ifdef NON_MATCHING
/* Same code but for the order of three instructions around the first label's kind_text call: the game works out
 * its x before the call's arguments, which no form of the expression gives here. */
void form_member(int frame)
{
    RECT tw;
    int id, x, w;

    x = 0x20;
    id = form_shown;
    func_800285AC(0x111, 0x6C, D_8009C738[id].exp_bar, 6, 0x80, 0x20, 0x20);
    func_80028CA0(0x110, 0x6B, 0x88, 8, 0x40, 8, 7, 0);
    func_80028CA0(0x15A, 0x5E, 0xF8, 8, 8, 8, 7, 0);
    func_80028CA0(0x15A, 0x77, 0xF8, 8, 8, 8, 7, 0);
    func_80028E00(0x114, 0x5E, D_8009C738[id].exp, 10, 7);
    func_80028E00(0x114, 0x77, D_8009C738[id].exp_to_next, 10, 7);
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x3F, &tw);
    w = func_80026B70(kind_text(8, 9, 8)) - 0xD6;
    w = x - w;
    func_80026F44(w, 0x5C, kind_text(8, 9, 8), 7);
    w = func_80026B70(kind_text(8, 0x2C, 8)) - 0xD6;
    x -= w;
    func_80026F44(x, 0x75, kind_text(8, 0x2C, 8), 7);
}
#else
INCLUDE_ASM("asm/menu/formmenu/nonmatchings/formmenu", form_member);
#endif

/* The first pick: the character under the cursor, unless it can't be moved. */
void form_pick_party(void)
{
    unsigned char *grid, *p; /* grid: the game's code reads the grid through a register */

    form_first_state = form_state;
    switch (form_state) {
    case 0:
        if (D_8009CBDC.locked >> D_8009CBDC.party[form_cursors[0].row] & 1) {
            form_sound(3);
            return;
        }
        form_first_id = D_8009CBDC.party[form_cursors[0].row];
        form_first = &D_8009CBDC.party[form_cursors[0].row];
        form_first_row = form_cursors[0].row;
        break;
    case 1:
        grid = form_reserve;
        p = &grid[form_cursors[1].row * 3 + form_cursors[1].col];
        if (D_8009CBDC.locked >> *p & 1) {
            form_sound(3);
            return;
        }
        form_first_id = *p;
        form_first = p;
        form_first_col = form_cursors[1].col;
        form_first_row = form_cursors[1].row;
        break;
    }
    form_picked = 1;
}

/* The second pick: swaps the two, or shows the character picked twice. */
void form_pick_swap(void)
{
    unsigned char *grid, *p;
    unsigned char id;
    int i;

    switch (form_state) {
    case 0:
        if (D_8009CBDC.locked >> D_8009CBDC.party[form_cursors[0].row] & 1) {
            form_sound(3);
            return;
        }
        form_second_id = D_8009CBDC.party[form_cursors[0].row];
        form_second = &D_8009CBDC.party[form_cursors[0].row];
        break;
    case 1:
        grid = form_reserve;
        p = &grid[form_cursors[1].row * 3 + form_cursors[1].col];
        if (D_8009CBDC.locked >> *p & 1) {
            form_sound(3);
            return;
        }
        form_second_id = *p;
        form_second = p;
        break;
    }
    form_picked = 0;
    if (*form_first == *form_second && *form_first != 0xFF) {
        form_state_prev = form_state;
        form_state = 2;
        form_shown = *form_second;
    }
    id = *form_first;
    *form_first = *form_second;
    *form_second = id;
    for (i = 0; i < 3; i++) {
        if (D_8009CBDC.party[i] != 0xFF) {
            func_80020058(i);
            member_update(i);
        }
    }
}

void form_pick(void)
{
    switch (form_picked) {
    case 0:
        form_pick_party();
        break;
    case 1:
        form_pick_swap();
        break;
    }
}

/* The module's init (also the main menu's): every character's battle data worked out, the portraits into VRAM, the
 * others into the grid. */
#ifdef NON_MATCHING
/* Same code but for which of v0 and v1 gets the grid and 0xFF in the two loops at the end. */
void form_init(int fading)
{
    RECT unused; /* unused, but the game's frame has it */
    int i;
    unsigned char first;
    unsigned char *p;

    first = D_8009CBDC.party[0];
    for (i = 0; i < 9; i++) {
        D_8009CBDC.party[0] = i;
        func_80020058(0);
        member_update(0);
    }
    D_8009CBDC.party[0] = first;
    func_80020058(0);
    member_update(0);
    form_fading = fading;
    form_picked = 0;
    if (fading)
        func_80025CD4(form_vram_saved);
    DrawSync(0);
    func_80025B8C(form_vram_portraits);
    DrawSync(0);
    func_80025C14(form_vram_faces);
    DrawSync(0);
    func_80025DF8();
    func_80026448(&form_cursors[0], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
    func_80026448(&form_cursors[1], 0, 0, 3, 3, 0, 0, 3, 3, 0, 0, 0, 0, 0);
    form_state = 0;
    p = form_reserve;
    for (i = 8; i >= 0; i--)
        p[i] = 0xFF;
    for (i = 0, p = form_reserve; i < 9; i++) {
        if (D_8009CBDC.party[0] != i && D_8009CBDC.party[1] != i && D_8009CBDC.party[2] != i &&
            D_8009CBDC.joined >> i & 1)
            *p++ = i;
    }
}
#else
INCLUDE_ASM("asm/menu/formmenu/nonmatchings/formmenu", form_init);
#endif

/* Whether the party has the size to leave with. */
#ifdef NON_MATCHING
/* Same code but for the end: GCC returns the comparison with xor and sltiu, the game branches to 0 or 1. */
int form_ready(void)
{
    int i, count;

    for (i = 0, count = 0; i < 3; i++) {
        if (D_8009CBDC.party[i] != 0xFF)
            count++;
    }
    return count == (form_fading ? form_needed : 3);
}
#else
INCLUDE_ASM("asm/menu/formmenu/nonmatchings/formmenu", form_ready);
#endif

void form_frame(int frame)
{
    RECT unused, tw; /* unused: the game's frame has it */
    int i, j, k, id, cancelled;

    cancelled = 0;
    if (form_fading) {
        if (!form_phase) {
            if (!form_fade(-15))
                form_phase = 1;
        } else if (form_phase == 2) {
            if (form_fade(15) == 0xFF)
                form_phase = -1;
        }
    }
    if ((form_fading && form_phase == 1) || (!func_80023050() && !form_fading)) {
        id = form_cursors[form_state].col;
        func_800264A8(&form_cursors[form_state]);
        switch (form_state) {
        case 0:
            if (D_80062D7C & 0x20) {
                form_sound(1);
                form_pick();
            } else if (D_80062D7C & 0x2000) {
                form_state = 1;
            } else if ((D_80062D7C & 0x40) && form_picked) {
                form_sound(4);
                form_picked = 0;
                cancelled = 1;
            }
            break;
        case 1:
            if (D_80062D7C & 0x20) {
                form_sound(1);
                form_pick();
            } else if (!id && (D_80062D7C & 0x8000)) {
                form_state = 0;
            }
            if ((D_80062D7C & 0x40) && form_picked) {
                form_picked = 0;
                cancelled = 1;
            }
            break;
        case 2:
            if (D_80062D7C & 0x60) {
                form_sound(4);
                cancelled = 1;
                form_state = form_state_prev;
            }
            break;
        }
        if (form_fading) {
            if (((D_80062D7C & 0x800) || ((D_80062D7C & 0x40) && !form_picked)) && !cancelled) {
                if (form_ready()) {
                    form_sound(1);
                    form_phase = 2;
                } else {
                    form_sound(3);
                }
            }
        } else if ((D_80062D7C & 0x40) && !form_picked && !cancelled) {
            if (!form_ready()) {
                form_sound(3);
            } else {
                DrawSync(0);
                func_800260DC();
                D_80062F90 = 1;
                func_8002305C(5, 0);
                func_8002120C(0);
                func_80025BD0(form_vram_portraits);
                DrawSync(0);
                func_80025C54(form_vram_faces);
                DrawSync(0);
                func_80025C94(D_800756F8);
                DrawSync(0);
                form_sound(4);
            }
        }
    }
    if (form_fading) {
        func_80026B5C(0x80);
        func_80026F44(0x126, 0xB, form_text[0], 7);
        func_8001E040(&form_title_window);
        if (frame & 0x80)
            func_80026F44(0x10, 0xB, form_text[form_needed + 1], 7);
        else if (form_split) {
            if (form_split < 6)
                func_80026F44(0x10, 0xB, form_text[1], 7);
            else
                func_80026F44(0x10, 0xB, form_text[2], 7);
        } else {
            func_80026F44(0x10, 0xB, form_text[5], 7);
        }
    } else {
        func_800230C4(results_buf);
        func_80026F44(0x10, 0xB, form_text[4], 7);
    }
    if (form_picked) {
        switch (form_first_state) {
        case 0:
            if (frame & 2)
                func_8001EB2C(-3, form_windows[1].y + form_first_row * 64 + 0x1D);
            break;
        case 1:
            if (frame & 2)
                func_8001EB2C(form_first_col * 48 + form_windows[3].x + 9,
                              form_first_row * 46 + form_windows[3].y + 0x19);
            break;
        }
    }
    switch (form_state) {
    case 0:
        func_8001EB2C(0, form_windows[1].y + form_cursors[0].row * 64 + 0x20);
        break;
    case 1:
        func_8001EB2C(form_cursors[1].col * 48 + form_windows[3].x + 0xC,
                      form_cursors[1].row * 46 + form_windows[3].y + 0x1C);
        id = form_reserve[form_cursors[1].row * 3 + form_cursors[1].col];
        if (id == 0xFF)
            break;
        func_800206E4(form_windows[2].x + 0x52, form_windows[2].y + 8, id);
        if (func_80023050() == 5)
            break;
        func_8001D180(form_windows[2].x + 0x18, form_windows[2].y + 4, 0x30, 0x30, id < 5 ? 0 : 0x30, id % 5 * 48,
                      0x30, 0x30, id, 0);
        setRECT(&tw, 0, 0, 0xFF, 0xFF);
        func_80026A34(0, 1, 0x7F, &tw);
        break;
    case 2:
        if (form_state_prev == 1) {
            id = form_reserve[form_cursors[1].row * 3 + form_cursors[1].col];
            func_800206E4(form_windows[2].x + 0x52, form_windows[2].y + 8, id);
            if (func_80023050() != 5) {
                func_8001D180(form_windows[2].x + 0x18, form_windows[2].y + 4, 0x30, 0x30, id < 5 ? 0 : 0x30,
                              id % 5 * 48, 0x30, 0x30, id, 0);
                setRECT(&tw, 0, 0, 0xFF, 0xFF);
                func_80026A34(0, 1, 0x7F, &tw);
            }
        } else if (frame & 2) {
            func_8001EB2C(0, form_windows[1].y + form_cursors[0].row * 64 + 0x20);
        }
        form_equipment(0xB4, 0x8B);
        form_member(frame);
        break;
    }
    if (form_state != 2) {
        for (j = 0; j < 3; j++) {
            for (i = 0; i < 3; i++) {
                id = form_reserve[j * 3 + i];
                if (id == 0xFF || func_80023050() == 5)
                    continue;
                func_8001D180(form_windows[3].x + 0x1C + i * 48, form_windows[3].y + 6 + j * 46, 0x28, 0x28,
                              id < 5 ? 0 : 0x30, id % 5 * 48, 0x30, 0x30, id, 0);
                setRECT(&tw, 0, 0, 0xFF, 0xFF);
                func_80026A34(0, 1, 0x7F, &tw);
            }
        }
    }
    for (k = 0; k < 3; k++) {
        if (D_8009CBDC.party[k] == 0xFF)
            continue;
        id = character_record[D_8009CBDC.party[k]];
        func_80020B68(0x54, form_windows[1].y + 0xC + k * 64, k);
        if (func_80023050() == 5)
            continue;
        func_8001D180(0x14, form_windows[1].y + 8 + k * 64, 0x30, 0x30, id < 5 ? 0 : 0x30, id % 5 * 48, 0x30, 0x30,
                      id, 0);
        setRECT(&tw, 0, 0, 0xFF, 0xFF);
        func_80026A34(0, 1, 0x7F, &tw);
    }
    for (k = 0; k < 4; k++)
        func_8001E040(&form_windows[k]);
}

/* The VRAM as it was before the module. */
void form_restore(void)
{
    func_80025CD4(D_800756F8);
    func_80025C94(form_vram_saved);
    func_80025BD0(form_vram_portraits);
    func_80025C54(form_vram_faces);
}

/* The field's PHS. needed is the party's size to leave with, 0 for three; from 4 the party is being split (4 to 7
 * leave with three, 8 with two; the help says two groups below 6, three from 6). */
void form_run(int needed)
{
    unsigned char vram[0x600];
    int frame, i;

    form_needed = needed;
    form_split = 0;
    if (!needed) {
        form_needed = 3;
    } else if (needed < 4) {
        form_needed = needed;
    } else {
        form_split = needed;
        needed -= 4;
        if (needed >= 0) {
            if (needed < 4)
                form_needed = 3;
            else if (needed == 4)
                form_needed = 2;
        }
    }
    func_80025CD4(vram);
    func_80025C94(D_800756F8);
    frame = 0;
    func_80021044(form_draw, form_disp);
    form_phase = 0;
    form_init(1);
    form_buf = 0;
    for (;;) {
        func_8001CB48();
        func_800269C0(D_80077F64[form_buf]);
        form_ot = &form_ots[form_buf];
        func_8004418C(form_ot, 1);
        func_80026A00(form_ot);
        form_frame(frame);
        if (form_phase == -1)
            break;
        DrawSync(0);
        VSync(0);
        func_800444AC(&form_disp[form_buf]);
        PutDrawEnv(&form_draw[form_buf]);
        func_8004433C(form_ot);
        frame++;
        form_buf ^= 1;
    }
    form_restore();
    func_80025C94(vram);
    for (i = 0; i < 3; i++) {
        if (D_8009CBDC.party[i] != 0xFF) {
            func_80020058(i);
            member_update(i);
        }
    }
    VSync(0);
    func_800444AC(&form_disp[0]);
    PutDrawEnv(&form_draw[0]);
    VSync(0);
    func_800444AC(&form_disp[1]);
    PutDrawEnv(&form_draw[1]);
}
