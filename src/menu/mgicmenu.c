/* MGICMENU.MNU, 0x801D0124-0x801D2930 (jump tables before it, data after it to 0x801D2B00): the Magic command's
 * module. A menu of Magic, Summon and Enemy-Skill (the last two once the save's flags show them) leads to the
 * member's spells in a scrolling grid, with the support materia linked to the one under the cursor and its MP cost.
 * Cure, Cure2, Cure3, Full Cure, Life and Life2 can be cast here: on one member, or on the party where the spell has
 * All's uses (left, right, L1 or R1 switch), for the field's own amounts. GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

extern PartyMember members[3];
extern unsigned char D_8009CBDC[]; /* the party: three character ids, 0xFF for an empty place */
extern unsigned char D_8009D2D3;   /* bit 0: Summon is shown, bit 1: Enemy-Skill */
extern int menu_member;
extern DRAWENV results_draw[2];

extern int mgic_state; /* 0 the menu, 1 the targets, 2 Magic, 3 Summon, 4 Enemy-Skill */
/* The menu's three names, "Added-Ability", the eight support names, "Nothing." and "Not enough MP." */
extern unsigned char mgic_text[14][20];
extern RECT mgic_windows[3];
extern RECT mgic_category_window;
extern RECT mgic_mp_window;
extern RECT mgic_title_window;
extern MenuCursor mgic_cursors[6];
extern int mgic_member;
extern int mgic_all; /* the spell goes on the whole party */
extern ScrollBar mgic_bar;

void mgic_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

void mgic_init(void)
{
    MemberSpell *s;
    int i, j;

    func_80026448(&mgic_cursors[0], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
    func_80026448(&mgic_cursors[1], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
    func_80026448(&mgic_cursors[2], 0, 0, 3, 7, 0, 0, 3, 0x12, 0, 0, 2, 0, 0);
    func_80026448(&mgic_cursors[3], 0, 0, 2, 7, 0, 0, 2, 8, 0, 0, 2, 0, 0);
    func_80026448(&mgic_cursors[4], 0, 0, 2, 7, 0, 0, 2, 0xC, 0, 0, 2, 0, 0);
    mgic_state = 0;
    mgic_member = menu_member;
    /* All's uses count only on spells whose targets allow the whole party. */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 56; j++) {
            s = &members[i].spells[j];
            if (s->id != 0xFF && s->uses && (s->targets & 0xC) != 0xC)
                s->uses = 0;
        }
    }
}

int mgic_hp_full(int slot)
{
    return members[slot].max_hp == members[slot].hp;
}

int mgic_mp_full(int slot)
{
    return members[slot].max_mp == members[slot].mp;
}

/* The member's name, HP and MP, and portrait. */
void mgic_header(void)
{
    RECT tw;

    func_80020B68(0x3E, 0x1C, mgic_member);
    func_8001CF3C(10, 0x17, 0x30, 0x30, 0, mgic_member * 48 + 0x38, 0x30, 0x30, mgic_member + 13, 0);
    setRECT(&tw, 0, 0, 0x100, 0x100);
    func_80026A34(0, 1, 0x7F, &tw);
}

#ifdef NON_MATCHING
/* 35 instructions short of the game's 2,342, all register choice and operand order: the game keeps `spells` on the
 * stack and the support labels' offset in a register where GCC does the opposite, adds the panel's offsets to their
 * constants before the window's x and y, and merges the two count calls of All and Quadra. Found on the way: the
 * grids index `top * 3 + j + i * 3` (with i * 3 next to top * 3 GCC factors them), the support colours are a
 * switch with the 7 tested in its default, the label is drawn in 7 whatever the colour, the heal amounts go through
 * `power` (the party-wide one divided by 3 in the loop), and the MP check and the "anyone to heal" tests keep the
 * failing branch last. */
void mgic_frame(int frame)
{
    RECT r, tw;
    MemberSpell *spells;
    int i, j, k, n, x, y, dx, dy, color, rows, any, power;

    if (mgic_state) {
        func_80026F44(mgic_title_window.x + 16, mgic_title_window.y + 6, mgic_text[mgic_cursors[0].row], 7);
        func_8001E040(&mgic_title_window);
    }
    func_800230C4(results_buf);
    switch (mgic_state) {
    case 0:
        func_8001EB2C(mgic_category_window.x - 10, mgic_category_window.y + mgic_cursors[0].row * 16 + 9);
        func_80026F44(mgic_category_window.x + 16, mgic_category_window.y + 6, mgic_text[0], 7);
        if (D_8009D2D3 & 1)
            func_80026F44(mgic_category_window.x + 16, mgic_category_window.y + 0x16, mgic_text[1], 7);
        if (D_8009D2D3 & 2)
            func_80026F44(mgic_category_window.x + 16, mgic_category_window.y + 0x26, mgic_text[2], 7);
        func_8001E040(&mgic_category_window);
        break;
    case 1:
    case 2:
        spells = members[mgic_member].spells;
        k = mgic_cursors[2].col + mgic_cursors[2].row * 3 + mgic_cursors[2].top * 3;
        if (mgic_state == 1) {
            x = 100;
            y = 50;
            func_8001EB2C(100, (mgic_all ? frame % 3 : mgic_cursors[1].row) * 56 + 0x4E);
            for (i = 0; i < 3; i++) {
                if (D_8009CBDC[i] == 0xFF)
                    continue;
                func_80020B68(x + 0x50, y + i * 56 + 10, i);
                func_8001CF3C(x + 0x16, y + i * 56 + 5, 0x30, 0x30, 0, i * 48 + 0x38, 0x30, 0x30, i + 13, 0);
                setRECT(&tw, 0, 0, 0x100, 0x100);
                func_80026A34(0, 1, 0x7F, &tw);
            }
            n = func_80026B70(kind_text(0, spells[k].id, 8));
            func_80026F44(x + 0x88 - n / 2, y - 14, kind_text(0, spells[k].id, 8), 7);
            func_8001DE0C(&r, x + 0x5B, y - 0x14, 0x5B, 0x18);
            func_8001E040(&r);
            func_8001DE0C(&r, x, y, 0xAA, 0xAB);
            func_8001E040(&r);
            if (frame & 2)
                func_8001EB2C(mgic_windows[2].x + mgic_cursors[2].col * 104 + 0x14,
                              mgic_windows[2].y + mgic_cursors[2].row * 16 + 0xB);
        } else {
            func_8001EB2C(mgic_windows[2].x + mgic_cursors[2].col * 104 + 0x14,
                          mgic_windows[2].y + mgic_cursors[2].row * 16 + 0xB);
        }
        if (spells[k].id != 0xFF)
            func_80026F44(16, mgic_windows[1].y + 6, kind_text(0, spells[k].id, 0), 7);
        func_80026F44(mgic_windows[0].x + 0x8E, mgic_windows[0].y + 10, mgic_text[3], 5);
        if (spells[k].id != 0xFF) {
            n = 0;
            for (i = 0; i < 8; i++) {
                switch (i) {
                case 0:
                    color = spells[k].uses ? 7 : 0;
                    break;
                case 1:
                    color = spells[k].quadra_uses ? 7 : 0;
                    break;
                default:
                    if (i == 7)
                        color = spells[k].flags >> 5 ? 7 : 0;
                    else
                        color = spells[k].flags >> (i - 2) & 1 ? 7 : 0;
                    break;
                }
                if (color) {
                    dx = n / 4 * 72;
                    dy = (n & 3) * 12;
                    func_80026F44(mgic_windows[0].x + dx + 0x95, mgic_windows[0].y + dy + 0x18, mgic_text[i + 4], 7);
                    if (i == 0) {
                        func_80028E00(mgic_windows[0].x + dx + 0xB8, mgic_windows[0].y + dy + 0x1A, spells[k].uses, 3, 7);
                        func_80028CA0(mgic_windows[0].x + dx + 0xCE, mgic_windows[0].y + dy + 0x1A, 0x78, 8, 8, 8, 7, 0);
                    } else if (i == 1) {
                        func_80028E00(mgic_windows[0].x + dx + 0xB8, mgic_windows[0].y + dy + 0x1A,
                                      spells[k].quadra_uses, 3, 7);
                        func_80028CA0(mgic_windows[0].x + dx + 0xCE, mgic_windows[0].y + dy + 0x1A, 0x78, 8, 8, 8, 7, 0);
                    } else if (i == 7) {
                        func_80028E00(mgic_windows[0].x + dx + 0xBD, mgic_windows[0].y + dy + 0x1A,
                                      spells[k].flags >> 5, 3, 7);
                    }
                    n++;
                }
                setRECT(&tw, 0, 0, 0xFF, 0xFF);
                func_80026A34(0, 1, 0x7F, &tw);
            }
            if (!n)
                func_80026F44(mgic_windows[0].x + 0x9A, mgic_windows[0].y + 0x1A, mgic_text[12], 7);
        }
        func_80028CA0(mgic_mp_window.x + 16, mgic_mp_window.y + 6, 0, 0x10, 0x28, 0x10, 1, 0);
        if (spells[k].id != 0xFF)
            func_80029114(mgic_mp_window.x + 0x18, mgic_mp_window.y + 0x24, spells[k].mp, 3, 7);
        setRECT(&tw, 0, 0, 0xFF, 0xFF);
        func_80026A34(0, 1, 0x7F, &tw);
        mgic_bar.shown = 7;
        mgic_bar.total = 0x12;
        mgic_bar.top = mgic_cursors[2].top;
        mgic_bar.x = 0x160;
        mgic_bar.y = mgic_windows[2].y + 4;
        mgic_bar.w = 10;
        mgic_bar.h = mgic_windows[2].h - 7;
        func_80028484(&mgic_bar);
        setRECT(&tw, 0, 5, 0x16C, 0xDB);
        func_80026A94(&results_draw[results_buf], &tw);
        rows = 7;
        if (mgic_cursors[2].scrolling)
            rows = 8;
        for (i = 0; i < rows; i++) {
            for (j = 0; j < 3; j++) {
                switch (spells[mgic_cursors[2].top * 3 + j + i * 3].id) {
                case 0:
                case 1:
                case 2:
                case 7:
                case 8:
                case 0x33:
                    color = 7;
                    break;
                default:
                    color = 0;
                    break;
                }
                if (spells[mgic_cursors[2].top * 3 + j + i * 3].id != 0xFF)
                    func_80026F44(j * 0x68 + 0x30, mgic_windows[2].y + i * 16 + 8 + mgic_cursors[2].scroll_step * 4,
                                  kind_text(0, spells[mgic_cursors[2].top * 3 + j + i * 3].id, 8), color);
            }
        }
        setRECT(&tw, 0, mgic_windows[2].y + 4, 0x16C, mgic_windows[2].h - 7);
        func_80026A94(&results_draw[results_buf], &tw);
        func_8001E040(&mgic_mp_window);
        break;
    case 3:
        func_8001EB2C(mgic_windows[2].x + mgic_cursors[3].col * 176, mgic_windows[2].y + mgic_cursors[3].row * 16 + 0xB);
        spells = &members[mgic_member].spells[56];
        k = mgic_cursors[3].col + mgic_cursors[3].row * 2 + mgic_cursors[3].top * 2;
        if (spells[k].id != 0xFF)
            func_80026F44(16, mgic_windows[1].y + 6, kind_text(1, k, 0), 7);
        func_80026F44(mgic_windows[0].x + 0x8E, mgic_windows[0].y + 10, mgic_text[3], 5);
        if (spells[k].id != 0xFF) {
            n = 0;
            for (i = 0; i < 8; i++) {
                switch (i) {
                case 0:
                    color = 0;
                    break;
                case 1:
                    color = spells[k].quadra_uses ? 7 : 0;
                    break;
                default:
                    if (i == 7)
                        color = spells[k].flags >> 5 ? 7 : 0;
                    else
                        color = spells[k].flags >> (i - 2) & 1 ? 7 : 0;
                    break;
                }
                if (color && i) {
                    dx = n / 4 * 72;
                    dy = (n & 3) * 12;
                    func_80026F44(mgic_windows[0].x + dx + 0x95, mgic_windows[0].y + dy + 0x18, mgic_text[i + 4], 7);
                    if (i == 1) {
                        func_80028E00(mgic_windows[0].x + dx + 0xB8, mgic_windows[0].y + dy + 0x1A,
                                      spells[k].quadra_uses, 3, 7);
                        func_80028CA0(mgic_windows[0].x + dx + 0xCE, mgic_windows[0].y + dy + 0x1A, 0x78, 8, 8, 8, 7, 0);
                    } else if (i == 7) {
                        func_80028E00(mgic_windows[0].x + dx + 0xBD, mgic_windows[0].y + dy + 0x1A,
                                      spells[k].flags >> 5, 3, 7);
                    }
                    n++;
                }
                setRECT(&tw, 0, 0, 0xFF, 0xFF);
                func_80026A34(0, 1, 0x7F, &tw);
            }
            if (!n)
                func_80026F44(mgic_windows[0].x + 0x9A, mgic_windows[0].y + 0x1A, mgic_text[12], 7);
        }
        func_80028CA0(mgic_mp_window.x + 16, mgic_mp_window.y + 6, 0, 0x10, 0x28, 0x10, 1, 0);
        if (spells[k].id != 0xFF)
            func_80029114(mgic_mp_window.x + 0x18, mgic_mp_window.y + 0x24, spells[k].mp, 3, 7);
        setRECT(&tw, 0, 0, 0xFF, 0xFF);
        func_80026A34(0, 1, 0x7F, &tw);
        mgic_bar.shown = 7;
        mgic_bar.total = 8;
        mgic_bar.top = mgic_cursors[3].top;
        mgic_bar.x = 0x160;
        mgic_bar.y = mgic_windows[2].y + 4;
        mgic_bar.w = 10;
        mgic_bar.h = mgic_windows[2].h - 7;
        func_80028484(&mgic_bar);
        setRECT(&tw, 0, 5, 0x16C, 0xDB);
        func_80026A94(&results_draw[results_buf], &tw);
        rows = 7;
        if (mgic_cursors[3].scrolling)
            rows = 8;
        for (i = 0; i < rows; i++) {
            for (j = 0; j < 2; j++) {
                if (spells[mgic_cursors[3].top * 2 + j + i * 2].id != 0xFF)
                    func_80026F44(j * 0xB0 + 0x1C, mgic_windows[2].y + i * 16 + 8 + mgic_cursors[3].scroll_step * 4,
                                  kind_text(1, spells[mgic_cursors[3].top * 2 + j + i * 2].id, 8), 7);
            }
        }
        setRECT(&tw, 0, mgic_windows[2].y + 4, 0x16C, mgic_windows[2].h - 7);
        func_80026A94(&results_draw[results_buf], &tw);
        func_8001E040(&mgic_mp_window);
        break;
    case 4:
        func_8001EB2C(mgic_windows[2].x + mgic_cursors[4].col * 176, mgic_windows[2].y + mgic_cursors[4].row * 16 + 0xB);
        spells = &members[mgic_member].spells[72];
        k = mgic_cursors[4].col + mgic_cursors[4].row * 2 + mgic_cursors[4].top * 2;
        if (spells[k].id != 0xFF)
            func_80026F44(16, mgic_windows[1].y + 6, kind_text(2, k, 0), 7);
        func_80026F44(mgic_windows[0].x + 0x8E, mgic_windows[0].y + 12, mgic_text[3], 5);
        if (spells[k].id != 0xFF)
            func_80026F44(mgic_windows[0].x + 0x9A, mgic_windows[0].y + 0x1A, mgic_text[12], 7);
        func_80028CA0(mgic_mp_window.x + 16, mgic_mp_window.y + 6, 0, 0x10, 0x28, 0x10, 1, 0);
        if (spells[k].id != 0xFF)
            func_80029114(mgic_mp_window.x + 0x18, mgic_mp_window.y + 0x24, spells[k].mp, 3, 7);
        setRECT(&tw, 0, 0, 0xFF, 0xFF);
        func_80026A34(0, 1, 0x7F, &tw);
        mgic_bar.shown = 7;
        mgic_bar.total = 0xC;
        mgic_bar.top = mgic_cursors[4].top;
        mgic_bar.x = 0x160;
        mgic_bar.y = mgic_windows[2].y + 4;
        mgic_bar.w = 10;
        mgic_bar.h = mgic_windows[2].h - 7;
        func_80028484(&mgic_bar);
        setRECT(&tw, 0, 5, 0x16C, 0xDB);
        func_80026A94(&results_draw[results_buf], &tw);
        rows = 7;
        if (mgic_cursors[4].scrolling)
            rows = 8;
        for (i = 0; i < rows; i++) {
            for (j = 0; j < 2; j++) {
                if (spells[mgic_cursors[4].top * 2 + j + i * 2].id != 0xFF)
                    func_80026F44(j * 0xB0 + 0x1C, mgic_windows[2].y + i * 16 + 8 + mgic_cursors[4].scroll_step * 4,
                                  kind_text(2, spells[mgic_cursors[4].top * 2 + j + i * 2].id, 8), 7);
            }
        }
        setRECT(&tw, 0, mgic_windows[2].y + 4, 0x16C, mgic_windows[2].h - 7);
        func_80026A94(&results_draw[results_buf], &tw);
        func_8001E040(&mgic_mp_window);
        break;
    }
    mgic_header();
    for (i = 0; i < 3; i++)
        func_8001E040(&mgic_windows[i]);
    func_800264A8(&mgic_cursors[mgic_state]);
    if (func_80023050())
        return;
    switch (mgic_state) {
    case 0:
        if (D_80062D7C & 0x20) {
            switch (mgic_cursors[0].row) {
            case 1:
                if (D_8009D2D3 & 1) {
                    mgic_state = 3;
                    mgic_sound(1);
                } else {
                    mgic_sound(3);
                }
                break;
            case 2:
                if (D_8009D2D3 & 2) {
                    mgic_state = 4;
                    mgic_sound(1);
                } else {
                    mgic_sound(3);
                }
                break;
            default:
                mgic_state = mgic_cursors[0].row + 2;
                mgic_sound(1);
                break;
            }
        } else if (D_80062D7E & 0x40) {
            mgic_sound(4);
            func_8002305C(5, 0);
            func_8002120C(0);
        } else if (D_80062D7E & 4) {
            mgic_sound(1);
            do {
                if (--mgic_member < 0)
                    mgic_member = 2;
            } while (D_8009CBDC[mgic_member] == 0xFF);
        } else if (D_80062D7E & 8) {
            mgic_sound(1);
            do {
                if (++mgic_member >= 3)
                    mgic_member = 0;
            } while (D_8009CBDC[mgic_member] == 0xFF);
        }
        break;
    case 1:
        k = mgic_cursors[2].col + mgic_cursors[2].row * 3 + mgic_cursors[2].top * 3;
        spells = members[mgic_member].spells;
        if (D_80062D7C & 0x20) {
            if (members[mgic_member].mp >= spells[k].mp) {
                if (!mgic_all) {
                    if (D_8009CBDC[mgic_cursors[1].row] == 0xFF) {
                        mgic_sound(3);
                        break;
                    }
                    /* Sephiroth (10, in the flashback) casts for nothing. */
                    switch (spells[k].id) {
                    case 0:
                        if (mgic_hp_full(mgic_cursors[1].row) || !members[mgic_cursors[1].row].hp) {
                            mgic_sound(3);
                            break;
                        }
                        if (D_8009CBDC[mgic_member] != 10)
                            func_80025988(mgic_member, spells[k].mp);
                        power = (members[mgic_member].stats[2] + D_8009C738[character_record[D_8009CBDC[mgic_member]]].level) * 6;
                        func_800258BC(mgic_cursors[1].row, power + 110);
                        mgic_sound(0x107);
                        break;
                    case 1:
                        if (mgic_hp_full(mgic_cursors[1].row) || !members[mgic_cursors[1].row].hp) {
                            mgic_sound(3);
                            break;
                        }
                        if (D_8009CBDC[mgic_member] != 10)
                            func_80025988(mgic_member, spells[k].mp);
                        power = (members[mgic_member].stats[2] + D_8009C738[character_record[D_8009CBDC[mgic_member]]].level) * 6;
                        func_800258BC(mgic_cursors[1].row, power + 660);
                        mgic_sound(0x107);
                        break;
                    case 2:
                        if (mgic_hp_full(mgic_cursors[1].row) || !members[mgic_cursors[1].row].hp) {
                            mgic_sound(3);
                            break;
                        }
                        if (D_8009CBDC[mgic_member] != 10)
                            func_80025988(mgic_member, spells[k].mp);
                        power = (members[mgic_member].stats[2] + D_8009C738[character_record[D_8009CBDC[mgic_member]]].level) * 6;
                        func_800258BC(mgic_cursors[1].row, power + 1760);
                        mgic_sound(0x107);
                        break;
                    case 0x33:
                        if (mgic_hp_full(mgic_cursors[1].row) || !members[mgic_cursors[1].row].hp)
                            break;
                        if (D_8009CBDC[mgic_member] != 10)
                            func_80025988(mgic_member, spells[k].mp);
                        func_800258BC(mgic_cursors[1].row, 10000);
                        mgic_sound(0x107);
                        break;
                    case 7:
                        if (members[mgic_cursors[1].row].hp) {
                            mgic_sound(3);
                            break;
                        }
                        if (D_8009CBDC[mgic_member] != 10)
                            func_80025988(mgic_member, spells[k].mp);
                        func_800258BC(mgic_cursors[1].row, members[mgic_cursors[1].row].max_hp / 4);
                        mgic_sound(0x107);
                        break;
                    case 8:
                        if (members[mgic_cursors[1].row].hp) {
                            mgic_sound(3);
                            break;
                        }
                        if (D_8009CBDC[mgic_member] != 10)
                            func_80025988(mgic_member, spells[k].mp);
                        func_800258BC(mgic_cursors[1].row, 10000);
                        mgic_sound(0x107);
                        break;
                    }
                } else {
                    switch (spells[k].id) {
                    case 0:
                        for (i = 0, any = 0; i < 3; i++) {
                            if (D_8009CBDC[i] != 0xFF && !mgic_hp_full(i))
                                any = 1;
                        }
                        if (any) {
                            if (D_8009CBDC[mgic_member] != 10)
                                func_80025988(mgic_member, spells[k].mp);
                            power = (members[mgic_member].stats[2] + D_8009C738[character_record[D_8009CBDC[mgic_member]]].level) * 12 + 220;
                            for (i = 0; i < 3; i++) {
                                if (D_8009CBDC[i] != 0xFF && members[i].hp)
                                    func_800258BC(i, power / 3);
                            }
                            mgic_sound(0x107);
                        } else {
                            mgic_sound(3);
                        }
                        break;
                    case 1:
                        for (i = 0, any = 0; i < 3; i++) {
                            if (D_8009CBDC[i] != 0xFF && !mgic_hp_full(i))
                                any = 1;
                        }
                        if (any) {
                            if (D_8009CBDC[mgic_member] != 10)
                                func_80025988(mgic_member, spells[k].mp);
                            power = (members[mgic_member].stats[2] + D_8009C738[character_record[D_8009CBDC[mgic_member]]].level) * 12 + 1320;
                            for (i = 0; i < 3; i++) {
                                if (D_8009CBDC[i] != 0xFF && members[i].hp)
                                    func_800258BC(i, power / 3);
                            }
                            mgic_sound(0x107);
                        } else {
                            mgic_sound(3);
                        }
                        break;
                    case 2:
                        for (i = 0, any = 0; i < 3; i++) {
                            if (D_8009CBDC[i] != 0xFF && !mgic_hp_full(i))
                                any = 1;
                        }
                        if (any) {
                            if (D_8009CBDC[mgic_member] != 10)
                                func_80025988(mgic_member, spells[k].mp);
                            power = (members[mgic_member].stats[2] + D_8009C738[character_record[D_8009CBDC[mgic_member]]].level) * 12 + 3520;
                            for (i = 0; i < 3; i++) {
                                if (D_8009CBDC[i] != 0xFF && members[i].hp)
                                    func_800258BC(i, power / 3);
                            }
                            mgic_sound(0x107);
                        } else {
                            mgic_sound(3);
                        }
                        break;
                    case 7:
                        for (i = 0, any = 0; i < 3; i++) {
                            if (D_8009CBDC[i] != 0xFF && !members[i].hp)
                                any = 1;
                        }
                        if (any) {
                            if (D_8009CBDC[mgic_member] != 10)
                                func_80025988(mgic_member, spells[k].mp);
                            /* An eighth of the max HP of the member the target cursor is on, for each. */
                            for (i = 0; i < 3; i++) {
                                if (D_8009CBDC[i] != 0xFF && !members[i].hp)
                                    func_800258BC(i, members[mgic_cursors[1].row].max_hp / 8);
                            }
                            mgic_sound(0x107);
                        } else {
                            mgic_sound(3);
                        }
                        break;
                    case 8:
                        for (i = 0, any = 0; i < 3; i++) {
                            if (D_8009CBDC[i] != 0xFF && !members[i].hp)
                                any = 1;
                        }
                        if (any) {
                            if (D_8009CBDC[mgic_member] != 10)
                                func_80025988(mgic_member, spells[k].mp);
                            for (i = 0; i < 3; i++) {
                                if (D_8009CBDC[i] != 0xFF && !members[i].hp)
                                    func_800258BC(i, 5000);
                            }
                            mgic_sound(0x107);
                        } else {
                            mgic_sound(3);
                        }
                        break;
                    }
                }
            } else {
                func_8001F6C0(mgic_text[13], 7);
                mgic_sound(3);
            }
        } else if (D_80062D7E & 0x40) {
            mgic_sound(4);
            mgic_state = 2;
        } else if (spells[k].uses && (D_80062D7E & 0xA00C)) {
            mgic_sound(1);
            mgic_all ^= 1;
        }
        break;
    case 2:
        if (D_80062D7C & 0x20) {
            k = mgic_cursors[2].col + mgic_cursors[2].row * 3 + mgic_cursors[2].top * 3;
            spells = members[mgic_member].spells;
            if (spells[k].id == 0xFF || !members[mgic_member].hp) {
                mgic_sound(3);
                break;
            }
            switch (spells[k].id) {
            case 0:
            case 1:
            case 2:
            case 7:
            case 8:
            case 0x33:
                mgic_all = 0;
                mgic_sound(1);
                mgic_state = 1;
                break;
            default:
                mgic_sound(3);
                break;
            }
            break;
        }
        /* fall through: ✕ goes back to the menu */
    case 3:
    case 4:
        if (D_80062D7E & 0x40) {
            mgic_state = 0;
            mgic_sound(4);
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/menu/mgicmenu/nonmatchings/mgicmenu", mgic_frame);
#endif

void mgic_801D2928(void)
{
}
