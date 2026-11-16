/* STATMENU.MNU, 0x801D0000-0x801D1968 (data after it to 0x801D5E20): the Status command's module. Three pages, ○
 * stepping through them: the member's stats, equipment and battle commands; the elements their equipment attacks
 * with, halves, voids, absorbs; the statuses it inflicts and guards against. L1 and R1 change the member. GCC 2.7.2,
 * ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

extern unsigned char D_8009CBDC[]; /* the party: three character ids, 0xFF for an empty place */
extern PartyMember members[3];
extern int menu_member;

extern int stat_state; /* the page: 0 the stats, 1 the elements, 2 the statuses */
extern unsigned char stat_element_names[9][10];
extern unsigned char stat_status_names[27][20];
extern unsigned char stat_labels[10][15];
extern unsigned char stat_primary_names[6][15];
extern unsigned char stat_derived_names[7][15];
extern unsigned char stat_equip_names[3][15]; /* "Wpn.", "Arm.", "Acc." */
extern unsigned char stat_limit_label[];
extern RECT stat_windows[1];
extern short stat_command_widths[4]; /* the command window's, by its columns */
extern RECT stat_command_windows[8];
extern unsigned char stat_colors[8][3]; /* the exe's limit_colors again */
extern MenuCursor stat_cursors[3];
extern int stat_member; /* the party place shown */
extern unsigned char stat_prims[2][0x2000]; /* the status page's second part, which outgrows the menu's buffer */

static void stat_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

void stat_init(void)
{
    RECT unused; /* unused, but the game's frame has it */

    func_80026448(&stat_cursors[0], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
    func_80026448(&stat_cursors[1], 0, 0, 1, 3, 0, 0, 1, 9, 0, 0, 0, 0, 0);
    stat_state = 0;
    stat_member = menu_member;
}

void stat_portrait(void)
{
    RECT tw;

    func_80020B68(0x3E, 0xE, stat_member);
    func_8001CF3C(10, 9, 0x30, 0x30, 0, stat_member * 48 + 0x38, 0x30, 0x30, stat_member + 0xD, 0);
    setRECT(&tw, 0, 0, 0x100, 0x100);
    func_80026A34(0, 1, 0x7F, &tw);
}

/* The statuses' icons, two rows of 13, lit for those which (0 the weapon, 1 the armor) has. The icons are two-byte
 * characters in a text of the exe's. */
void stat_statuses(int x, int y, int which)
{
    RECT unused[2]; /* unused, but the game's frame has them */
    int i, has, px, py, ch;
    unsigned char *icons;

    for (i = 0; i < 27; i++) {
        if (i == 19)
            continue;
        icons = kind_text(8, 11, 8);
        switch (which) {
        case 0:
            has = members[stat_member].statuses[0] & 1 << i;
            break;
        case 1:
            has = members[stat_member].statuses[1] & 1 << i;
            break;
        }
        if (i != 26) {
            px = x + 0x6A + i % 13 * 14 - (i >= 20 ? 14 : 0);
            py = y + which * 32 + (i / 13 ? 16 : 0);
            ch = icons[i * 2 + 0x13] << 8 | icons[i * 2 + 0x12];
            func_80026C5C(px, py, has ? 7 : 0, ch);
        } else {
            ch = icons[0x47] << 8 | icons[0x46];
            func_80026C5C(x + 0x112, y + which * 32 + 16, has ? 7 : 0, ch);
        }
    }
}

/* The elements' icons in a row, lit for those in which of the member's element sets. */
void stat_elements(int x, int y, int which)
{
    RECT unused[2]; /* unused, but the game's frame has them */
    int i, has, ch;
    unsigned char *icons;

    for (i = 0; i < 9; i++) {
        icons = kind_text(8, 11, 8);
        switch (which) {
        case 0:
            has = members[stat_member].elements[0] & 1 << i;
            break;
        case 1:
            has = members[stat_member].elements[1] & 1 << i;
            break;
        case 2:
            has = members[stat_member].elements[2] & 1 << i;
            break;
        case 3:
            has = members[stat_member].elements[3] & 1 << i;
            break;
        }
        ch = icons[i * 2 + 1] << 8 | icons[i * 2];
        func_80026C5C(x + 0x6A + i * 14, y + which * 16, has ? 7 : 0, ch);
    }
}

/* The elements or statuses page: each set's names, wrapping at the screen's edge, lit for those it has. */
void stat_page(int x, int y)
{
    int set, i, bit, has, px, lines; /* lines: how many times the names wrapped */

    switch (stat_state) {
    case 1:
        func_80026F44(x - 16, y - 16, stat_labels[0], 5);
        func_80026F44(x, y, stat_labels[2], 5);
        func_80026F44(x, y + 0x22, stat_labels[4], 5);
        func_80026F44(x, y + 0x44, stat_labels[5], 5);
        func_80026F44(x, y + 0x66, stat_labels[6], 5);
        for (set = 0; set < 4; set++) {
            px = x + 0x2D;
            for (i = 0, lines = 0; i < 9; i++) {
                bit = 1 << i;
                switch (set) {
                case 0:
                    has = members[stat_member].elements[0] & bit;
                    break;
                case 1:
                    has = members[stat_member].elements[1] & bit;
                    break;
                case 2:
                    has = members[stat_member].elements[2] & bit;
                    break;
                case 3:
                    has = members[stat_member].elements[3] & bit;
                    break;
                }
                if (func_80026B70(stat_element_names[i]) + px >= 0x15F) {
                    lines++;
                    px = x + 0x2D;
                }
                func_80026F44(px, y + set * 0x22 + lines * 14, stat_element_names[i], has ? 7 : 0);
                px += 10 + func_80026B70(stat_element_names[i]);
            }
        }
        break;
    case 2:
        x -= 16;
        y -= 2;
        func_80026F44(x - 14, y - 14, stat_labels[1], 5);
        func_80026F44(x - 10, y, stat_labels[2], 5);
        func_80026F44(x - 10, y + 0x48, stat_labels[3], 5);
        for (set = 0; set < 2; set++) {
            px = x + 0x23;
            if (set == 1) {
                func_800269D0();
                func_800269C0(stat_prims[results_buf]);
            }
            for (i = 0, lines = 0; i < 27; i++) {
                if (i == 19)
                    continue;
                bit = 1 << i;
                switch (set) {
                case 0:
                    has = members[stat_member].statuses[0] & bit;
                    break;
                case 1:
                    has = members[stat_member].statuses[1] & bit;
                    break;
                }
                if (func_80026B70(stat_status_names[i]) + px >= 0x15F) {
                    lines++;
                    px = x + 0x23;
                }
                func_80026F44(px, y + set * 0x48 + lines * 14, stat_status_names[i], has ? 7 : 0);
                px += 6 + func_80026B70(stat_status_names[i]);
            }
        }
        func_800269E8();
        break;
    }
}

/* A piece of equipment's materia slots, the links between them, and the materia in them (four bytes each, the
 * id first: Character.weapon_materia's words). */
void stat_slots(int x, int y, unsigned char *slots, unsigned char *materia)
{
    RECT r;
    int i;

    for (i = 0; i < 8; i++) {
        if (slots[i] && (slots[i] == 3 || slots[i] == 7)) /* the zero test is the game's own */
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

void stat_equipment(int x, int y)
{
    RECT unused; /* unused, but the game's frame has it */
    Character *chars, *c; /* chars: the game's code reads the table through a register */

    chars = D_8009C738;
    c = &chars[character_record[D_8009CBDC[stat_member]]];
    func_80026F44(x + 0x38, y, func_80021280(c->weapon + 0x80), 7);
    func_80026F44(x + 0x38, y + 0x20, func_80021280(c->armor + 0x100), 7);
    if (c->accessory != 0xFF)
        func_80026F44(x + 0x38, y + 0x40, func_80021280(c->accessory + 0x120), 7);
    func_80026F44(x + 0x10, y, stat_equip_names[0], 5);
    func_80026F44(x + 0x10, y + 0x20, stat_equip_names[1], 5);
    func_80026F44(x + 0x10, y + 0x40, stat_equip_names[2], 5);
    stat_slots(x + 0x30, y + 0x10, weapon_table[c->weapon].slots, (unsigned char *)c->weapon_materia);
    stat_slots(x + 0x30, y + 0x30, armor_table[c->armor].slots, (unsigned char *)c->armor_materia);
}

/* The six primary stats, then the seven worked out from them and the equipment. */
void stat_stats(int x, int y)
{
    RECT tw;
    int i, id, defense_pct;
    unsigned short attack, hit, defense, magic, mdefense, mdefense_pct;

    id = D_8009CBDC[stat_member];
    for (i = 0; i < 6; i++)
        func_80026F44(x, y + i * 12 - 2, stat_primary_names[i], 5);
    func_80028E00(x + 0x4A, y, members[stat_member].primary[0], 5, 7);
    func_80028E00(x + 0x4A, y + 0xC, members[stat_member].primary[4], 5, 7);
    func_80028E00(x + 0x4A, y + 0x18, members[stat_member].primary[1], 5, 7);
    func_80028E00(x + 0x4A, y + 0x24, members[stat_member].primary[2], 5, 7);
    func_80028E00(x + 0x4A, y + 0x30, members[stat_member].primary[3], 5, 7);
    func_80028E00(x + 0x4A, y + 0x3C, members[stat_member].primary[5], 5, 7);
    y += 0x4D;
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x3F, &tw);
    for (i = 0; i < 7; i++)
        func_80026F44(x, y + i * 12 - 2, stat_derived_names[i], 5);
    attack = members[stat_member].stats[0];
    hit = weapon_table[D_8009C738[character_record[id]].weapon].hit;
    defense = members[stat_member].stats[1];
    defense_pct = members[stat_member].primary[4] / 4 + armor_table[D_8009C738[character_record[id]].armor].defense_pct;
    magic = members[stat_member].stats[2];
    mdefense = members[stat_member].stats[3];
    mdefense_pct = armor_table[D_8009C738[character_record[id]].armor].mdefense_pct;
    func_80028E00(x + 0x4A, y, attack, 5, 7);
    func_80028E00(x + 0x4A, y + 0xC, hit, 5, 7);
    func_80028E00(x + 0x4A, y + 0x18, defense, 5, 7);
    func_80028E00(x + 0x4A, y + 0x24, defense_pct, 5, 7);
    func_80028E00(x + 0x4A, y + 0x30, magic, 5, 7);
    func_80028E00(x + 0x4A, y + 0x3C, mdefense, 5, 7);
    func_80028E00(x + 0x4A, y + 0x48, mdefense_pct, 5, 7);
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x3F, &tw);
}

/* The level, EXP, next level, limit bar and level: the exe's party panel again. */
void stat_header(int frame)
{
    RECT tw;
    int rec, bar, r, g, b; /* bar: rec again, which the game's bar code reads as a name of its own */

    rec = character_record[D_8009CBDC[stat_member]];
    bar = rec;
    if (D_8009C738[bar].limit_bar == 0xFF) {
        r = stat_colors[frame >> 1 & 7][0];
        g = stat_colors[frame >> 1 & 7][1];
        b = stat_colors[frame >> 1 & 7][2];
    } else if (D_8009C738[bar].flags & 0x10) {
        r = stat_colors[0][0];
        g = stat_colors[0][1];
        b = stat_colors[0][2];
    } else if (D_8009C738[bar].flags & 0x20) {
        r = stat_colors[2][0];
        g = stat_colors[2][1];
        b = stat_colors[2][2];
    } else {
        r = 0x80;
        g = 0x20;
        b = 0x50;
    }
    func_800285AC(0xFD, 0x33, D_8009C738[bar].limit_bar * 61 / 256, 6, r, g, b);
    func_80026F44(0x94, 0xB, kind_text(8, 9, 8), 7);
    func_80026F44(0x94, 0x24, kind_text(8, 0x2C, 8), 7);
    func_80026F44(0x94, 0x30, stat_limit_label, 7);
    func_800285AC(0xCE, 0x1B, D_8009C738[rec].exp_bar, 6, 0x80, 0x20, 0x20);
    func_80028CA0(0xCD, 0x1A, 0x88, 8, 0x40, 8, 7, 0);
    func_80028CA0(0x10A, 0xD, 0xF8, 8, 8, 8, 7, 0);
    func_80028CA0(0x132, 0x26, 0xF8, 8, 8, 8, 7, 0);
    func_80028E00(0xC4, 0xD, D_8009C738[rec].exp, 10, 7);
    func_80028E00(0xEC, 0x26, D_8009C738[rec].exp_to_next, 10, 7);
    func_80028E00(0xD4, 0x32, D_8009C738[rec].limit_level, 1, 7);
    func_80028CA0(0xFC, 0x32, 0x88, 8, 0x40, 8, 7, 0);
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x3F, &tw);
}

/* The member's battle commands in their window, four to a column. */
void stat_commands(int x, int y)
{
    RECT tw, win;
    PartyMember *m;
    int cols, col, row, shift, id, single;

    m = &members[stat_member];
    cols = m->command_rows;
    for (col = 0; col < cols; col++) {
        for (row = 0; row < 4; row++) {
            id = m->commands[row + col * 4].id;
            single = cols < 2;
            if (id != 0xFF) {
                shift = single ? 0 : col > 0 ? 8 : 0; /* the columns after the first, a little right */
                func_80026F44(x + 6 + col * 0x2C + shift, y + row * 12 + 5, kind_text(5, id, 8), 7);
            }
        }
    }
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x7F, &tw);
    func_8001DE0C(&win, x, y, stat_command_widths[cols - 1], stat_command_windows[0].h);
    func_8001E040(&win);
}

void stat_frame(int frame)
{
    RECT unused; /* unused, but the game's frame has it */
    RECT tw;
    int i;

    func_800230C4(results_buf);
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x7F, &tw);
    stat_portrait();
    switch (stat_state) {
    case 0:
        stat_commands(0xB2, 0x45);
        stat_equipment(0xAA, 0x8D);
        stat_stats(0x24, 0x3D);
        stat_header(frame);
        break;
    case 1:
    case 2:
        stat_page(0x26, 0x4D);
        stat_header(frame);
        break;
    }
    for (i = 0; i < 1; i++)
        func_8001E040(&stat_windows[i]);
    if (func_80023050())
        return;
    switch (stat_state) {
    case 0:
        if (D_80062D7C & 0x20) {
            stat_sound(1);
            stat_state = 1;
        } else if (D_80062D7E & 0x40) {
            stat_sound(4);
            func_8002305C(5, 0);
            func_8002120C(0);
        }
        break;
    case 1:
        if (D_80062D7C & 0x20) {
            stat_sound(1);
            stat_state = 2;
        } else if (D_80062D7E & 0x40) {
            stat_sound(4);
            func_8002305C(5, 0);
            func_8002120C(0);
        }
        break;
    case 2:
        if (D_80062D7C & 0x20) {
            stat_sound(1);
            stat_state = 0;
        } else if (D_80062D7E & 0x40) {
            stat_sound(4);
            func_8002305C(5, 0);
            func_8002120C(0);
        }
        break;
    }
    if (D_80062D7E & 4) {
        stat_sound(1);
        do {
            if (--stat_member < 0)
                stat_member = 2;
        } while (D_8009CBDC[stat_member] == 0xFF);
    } else if (D_80062D7E & 8) {
        stat_sound(1);
        do {
            if (++stat_member >= 3)
                stat_member = 0;
        } while (D_8009CBDC[stat_member] == 0xFF);
    }
}

void stat_801D1960(void)
{
}
