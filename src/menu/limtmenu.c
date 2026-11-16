/* LIMTMENU.MNU, 0x801D002C-0x801D1320 (two jump tables before it, data after it to 0x801D2414): the Limit command's
 * module. Set picks a limit level from the four (2 by 2) and asks to confirm; Check lists each level's limits and
 * shows one's help. limt_load, which the exe calls each time the menu opens, copies each party member's three limits
 * of their level into their battle data, with the limits' attack records, which live here. GCC 2.7.2, ASPSX 2.21,
 * -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

extern unsigned char D_8009CBDC[]; /* the party: three character ids, 0xFF for an empty place */
extern PartyMember members[3];
extern int menu_member;
extern unsigned char member_labels[2][12]; /* "LV", "Limit level" */

extern AttackData limt_records[];
extern unsigned char limt_level_names[4][36];
extern unsigned char limt_help_set[];
extern unsigned char limt_labels[2][36]; /* "Set", "Check" */
extern unsigned char limt_confirm[5][36];
extern unsigned char limt_set_text[];
extern unsigned char limt_help_check[];
extern int limt_state; /* 0 picking a level to set, 1 Set or Check, 2 picking a level to check, 3 one of its limits,
                        * 4 confirming */
extern RECT limt_windows[4];
extern unsigned char limt_colors[8][3]; /* the exe's limit_colors again */
extern MenuCursor limt_cursors[10];
extern int limt_member; /* the party place shown */

void func_80026B5C(int unused); /* empty in the exe */

static void limt_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

void limt_init(void)
{
    func_80026448(&limt_cursors[0], 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0);
    func_80026448(&limt_cursors[1], 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 1, 0, 0);
    limt_state = 1;
    limt_member = menu_member;
}

/* The member's face, name, limit bar and level, and Set and Check: the exe's party panel again. */
void limt_header(int frame)
{
    RECT tw;
    unsigned char rec;
    int r, g, b;

    func_80020B68(0x3E, 0xE, limt_member);
    func_8001CF3C(10, 9, 0x30, 0x30, 0, limt_member * 48 + 0x38, 0x30, 0x30, limt_member + 0xD, 0);
    func_80026F44(0xBA, 0xE, member_labels[1], 7);
    rec = character_record[D_8009CBDC[limt_member]];
    if (D_8009C738[rec].limit_bar == 0xFF) {
        r = limt_colors[frame >> 1 & 7][0];
        g = limt_colors[frame >> 1 & 7][1];
        b = limt_colors[frame >> 1 & 7][2];
    } else if (D_8009C738[rec].flags & 0x10) {
        r = limt_colors[0][0];
        g = limt_colors[0][1];
        b = limt_colors[0][2];
    } else if (D_8009C738[rec].flags & 0x20) {
        r = limt_colors[2][0];
        g = limt_colors[2][1];
        b = limt_colors[2][2];
    } else {
        r = 0x80;
        g = 0x20;
        b = 0x50;
    }
    func_800285AC(0xCB, 0x1A, D_8009C738[rec].limit_bar * 61 / 256, 6, r, g, b);
    func_80028CA0(0xCA, 0x19, 0x88, 8, 0x40, 8, 7, 0);
    func_80028E00(0xFA, 0x10, D_8009C738[character_record[D_8009CBDC[limt_member]]].limit_level, 1, 7);
    setRECT(&tw, 0, 0, 0x100, 0x100);
    func_80026A34(0, 1, 0x3F, &tw);
    func_80026F44(0xCA, 0x2B, limt_labels[0], 7);
    func_80026F44(0xFA, 0x2B, limt_labels[1], 7);
}

/* int, though it returns nothing: the game's has the return register live at its end. */
int limt_frame(int frame)
{
    RECT tw;
    unsigned char has[4]; /* each level: whether any of its limits is learned */
    int i, j, k, color, count;
    AttackData *records; /* the game's Set copies from the table through a register */

    records = limt_records;
    for (j = 0; j < 4; j++) {
        has[j] = 0;
        for (i = 0; i < 3; i++) {
            if (D_8009C738[character_record[D_8009CBDC[limt_member]]].limits >> (j * 3 + i) & 1)
                has[j] = 1;
        }
    }
    func_800230C4(results_buf);
    if (!func_80023050()) {
        switch (limt_state) {
        case 0:
            if (frame & 2)
                func_8001EB2C(limt_cursors[1].col * 48 + 0xB0, 0x2C);
            func_8001EB2C(limt_cursors[0].col * 166, limt_windows[1].y + 6 + limt_cursors[0].row * 64 + 32);
            func_80026F44(0x18, limt_windows[1].y + 6, limt_help_set, 7);
            break;
        case 1:
            func_8001EB2C(limt_cursors[1].col * 48 + 0xB0, 0x2C);
            break;
        case 2:
            if (frame & 2)
                func_8001EB2C(limt_cursors[1].col * 48 + 0xB0, 0x2C);
            func_8001EB2C(limt_cursors[2].col * 166, limt_windows[1].y + 6 + limt_cursors[2].row * 64 + 32);
            func_80026F44(0x18, limt_windows[1].y + 6, limt_help_check, 7);
            break;
        case 3:
            if (frame & 2)
                func_8001EB2C(limt_cursors[1].col * 48 + 0xB0, 0x2C);
            func_8001EB2C(limt_cursors[2].col * 166 + 0x10,
                          limt_windows[1].y + 4 + limt_cursors[2].row * 64 + 0x2D + limt_cursors[3].row * 16);
            func_80026F44(0x18, limt_windows[1].y + 6,
                          kind_text(3, limit_id(character_record[D_8009CBDC[limt_member]],
                                                limt_cursors[2].col * 3 + limt_cursors[2].row * 6 +
                                                    limt_cursors[3].row), 0), 7);
            break;
        case 4:
            func_8001EB2C(limt_windows[3].x + 0x32, limt_cursors[4].row * 12 + limt_windows[3].y + 0x38);
            func_80026F44(limt_windows[3].x + 8, limt_windows[3].y + 6, limt_confirm[0], 7);
            func_80026F44(limt_windows[3].x + 8, limt_windows[3].y + 0x16, limt_confirm[1], 7);
            func_80026F44(limt_windows[3].x + 8, limt_windows[3].y + 0x26, limt_confirm[2], 7);
            func_80026F44(limt_windows[3].x + 0x4C, limt_windows[3].y + 0x36, limt_confirm[3], 7);
            func_80026F44(limt_windows[3].x + 0x4C, limt_windows[3].y + 0x42, limt_confirm[4], 7);
            setRECT(&tw, 0, 0, 0x100, 0x100);
            func_80026A34(0, 1, 0x3F, &tw);
            func_8001E040(&limt_windows[3]);
            break;
        }
    }
    limt_header(frame);
    func_80026B5C(8);
    /* the four levels, each with the names of its limits learned, grey when it has none */
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            color = has[i * 2 + j] ? 7 : 0;
            func_80026F44(j * 166 + 0x20, limt_windows[2].y + 6 + i * 64 + 8, limt_level_names[i * 2 + j], color);
            for (k = 0; k < 3; k++) {
                if (D_8009C738[character_record[D_8009CBDC[limt_member]]].limits >> (i * 6 + j * 3 + k) & 1)
                    func_80026F44(j * 166 + 0x2D, limt_windows[2].y + 6 + i * 64 + 8 + k * 16 + 0xC,
                                  kind_text(3, limit_id(character_record[D_8009CBDC[limt_member]],
                                                        i * 6 + j * 3 + k), 8), color);
            }
        }
    }
    for (k = 0; k < 3; k++)
        func_8001E040(&limt_windows[k]);
    if (!func_80023050() && !func_8001F6B4()) {
        func_800264A8(&limt_cursors[limt_state]);
        switch (limt_state) {
        case 0:
            if (D_80062D7E & 0x40) {
                limt_state = 1;
                break;
            }
            if (!(D_80062D7C & 0x20))
                break;
            if (!has[limt_cursors[0].col + limt_cursors[0].row * 2]) {
                limt_sound(3);
                break;
            }
            func_80026448(&limt_cursors[4], 0, 1, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 0);
            limt_state = 4;
            break;
        case 1:
            if (D_80062D7E & 0x40) {
                limt_sound(4);
                func_8002305C(5, 0);
                func_8002120C(0);
            } else if (D_80062D7E & 0x20) {
                if (!limt_cursors[1].col) {
                    func_80026448(&limt_cursors[0], 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0);
                    limt_state = 0;
                } else {
                    func_80026448(&limt_cursors[2], 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0);
                    limt_state = 2;
                }
            } else if (D_80062D7E & 4) {
                limt_sound(1);
                do {
                    if (--limt_member < 0)
                        limt_member = 2;
                } while (D_8009CBDC[limt_member] == 0xFF);
            } else if (D_80062D7E & 8) {
                limt_sound(1);
                do {
                    if (++limt_member >= 3)
                        limt_member = 0;
                } while (D_8009CBDC[limt_member] == 0xFF);
            }
            break;
        case 2:
            if (D_80062D7E & 0x20) {
                if (has[limt_cursors[2].row * 2 + limt_cursors[2].col]) {
                    limt_state = 3;
                    for (i = 0, count = 0; i < 3; i++) {
                        if (D_8009C738[character_record[D_8009CBDC[limt_member]]].limits >>
                                ((limt_cursors[2].row * 2 + limt_cursors[2].col) * 3 + i) & 1)
                            count++;
                    }
                    func_80026448(&limt_cursors[3], 0, 0, 1, count, 0, 0, 1, count, 0, 0, 0, 1, 0);
                } else {
                    limt_sound(3);
                }
            } else if (D_80062D7E & 0x40) {
                limt_state = 1;
                limt_sound(4);
            }
            break;
        case 3:
            if (D_80062D7E & 0x40) {
                limt_state = 2;
                limt_sound(4);
            }
            break;
        case 4:
            if (D_80062D7C & 0x20) {
                if (limt_cursors[limt_state].row) {
                    limt_sound(1);
                    limt_state = 0;
                    break;
                }
                limt_sound(1);
                D_8009C738[character_record[D_8009CBDC[limt_member]]].limit_bar = 0;
                for (k = 0; k < 3; k++) {
                    members[limt_member].limits[k] = limit_id(character_record[D_8009CBDC[limt_member]],
                                                              limt_cursors[0].col * 3 + limt_cursors[0].row * 6 + k);
                    members[limt_member].limit_records[k] =
                        records[limit_id(character_record[D_8009CBDC[limt_member]],
                                         limt_cursors[0].col * 3 + limt_cursors[0].row * 6 + k)];
                }
                D_8009C738[character_record[D_8009CBDC[limt_member]]].limit_level =
                    limt_cursors[0].col + limt_cursors[0].row * 2 + 1;
                func_8001F6C0(limt_set_text, 7);
                limt_state = 1;
            } else if (D_80062D7C & 0x40) {
                limt_sound(4);
                limt_state = 0;
            }
            break;
        }
    }
}

void limt_801D11A0(void)
{
}

/* Each party member's three limits of their level, with their records, into their battle data. */
void limt_load(void)
{
    int place, i, rec, first;

    for (place = 0; place < 3; place++) {
        if (D_8009CBDC[place] == 0xFF)
            continue;
        rec = character_record[D_8009CBDC[place]];
        first = (D_8009C738[rec].limit_level - 1) * 3;
        for (i = 0; i < 3; i++) {
            members[place].limits[i] = limit_id(rec, first + i);
            members[place].limit_records[i] = limt_records[limit_id(rec, first + i)];
        }
    }
}
