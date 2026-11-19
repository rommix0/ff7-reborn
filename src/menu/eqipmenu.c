/* EQIPMENU.MNU's first object, 0x801D0124-0x801D1B94 (its data at 0x801D82A4 and 0x801D88F8): the Equip command's
 * screen, module 4 (the module table's entry 15 points here too). The top lists the member's weapon, armor and
 * accessory; ○ on one lists what in the inventory that member can equip there, with its slots, growth and description,
 * and the stats panel shows Attack to Magic def% now and with the highlighted one on. ○ again swaps it in: the old one
 * goes back to the inventory, and materia in slots the new one hasn't goes back to the stock. △ takes the accessory
 * off; L1 and R1 change the member; □ (or ✕ after a change) goes on to the Materia screen when that command is shown.
 * GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

void func_80025380(); /* takes an unsigned short, but the game passes its items unextended */
unsigned short func_80025288(); /* the same */
void func_800254D8(void);
void materia_add(int materia);
unsigned char *armor_slots(int slot);
void party_effects_sum(void);
void func_8001F6AC(void);
void func_80026B5C(int layer);

extern DRAWENV results_draw[2];
extern int menu_member;
extern short D_80062FA4;
extern unsigned char D_80063024; /* the Materia command is shown */
extern unsigned char D_8009D4F7; /* bit 0: the equipment can't be changed */

extern unsigned char eqip_texts[25][12]; /* the slots', the stats', Slot, Growth and the growths' */
extern RECT eqip_windows[5];
extern unsigned char eqip_list[48]; /* what the member can equip in the slot, the kind's numbering; 0xFF after */
extern MenuCursor eqip_cursors[2]; /* the slot, the list */
extern int eqip_member;
extern int eqip_state;   /* 0 picking the slot, 1 picking from the list */
extern int eqip_changed; /* a weapon or armor was changed: ✕ goes on to the Materia screen */
extern ScrollBar eqip_bar;
extern int eqip_count;

static void eqip_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

void eqip_801D0164(void)
{
    func_8001F6AC();
}

/* SHOPMENU's shop_draw_slots. */
void eqip_slots(int x, int y, unsigned char *slots)
{
    int i;
    RECT r;

    for (i = 0; i < 8; i++) {
        if (slots[i] && (slots[i] == 3 || slots[i] == 7))
            func_80028CA0(x + i * 16 - 8, y, 0x70, 0x30, 16, 16, 1, 0);
    }
    for (i = 0; i < 8; i++) {
        if (slots[i]) {
            if (slots[i] >= 5)
                func_80028CA0(x + i * 16, y, 0x80, 0x30, 16, 16, 1, 0);
            else
                func_80028CA0(x + i * 16, y, 0x70, 0x20, 16, 16, 1, 0);
        }
    }
    r.x = x - 2;
    r.y = y;
    r.w = 0x80;
    r.h = 0xB;
    func_80027B84(&r);
    func_80026A34(0, 1, 0x1F, 0);
}

/* Lists what of kind (0 weapons, 1 armor, 2 accessories) in the inventory the member can equip. */
void eqip_fill_list(int kind)
{
    int i, n, id;

    switch (kind) {
    case 0:
        for (i = 0, n = 0; i < 320; i++) {
            if (save_data.items[i] == 0xFFFF)
                continue;
            id = save_data.items[i] & 0x1FF;
            if (id >= 0x80 && id < 0x100) {
                id -= 0x80;
                if (weapon_table[id].equip >> save_data.party[eqip_member] & 1)
                    eqip_list[n++] = id;
            }
        }
        break;
    case 1:
        for (i = 0, n = 0; i < 320; i++) {
            if (save_data.items[i] == 0xFFFF)
                continue;
            id = save_data.items[i] & 0x1FF;
            if (id >= 0x100 && id < 0x120) {
                id -= 0x100;
                if (armor_table[id].equip >> save_data.party[eqip_member] & 1)
                    eqip_list[n++] = id;
            }
        }
        break;
    case 2:
        for (i = 0, n = 0; i < 320; i++) {
            if (save_data.items[i] == 0xFFFF)
                continue;
            id = save_data.items[i] & 0x1FF;
            if (id >= 0x120 && id < 0x160) {
                id -= 0x120;
                if (accessory_table[id].equip >> save_data.party[eqip_member] & 1)
                    eqip_list[n++] = id;
            }
        }
        break;
    }
    eqip_count = n;
    for (; n < 48; n++)
        eqip_list[n] = 0xFF;
}

void eqip_init(void)
{
    eqip_state = 0;
    eqip_changed = 0;
    eqip_member = menu_member;
    func_80026448(&eqip_cursors[0], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
    func_80026448(&eqip_cursors[1], 0, 0, 1, 8, 0, 0, 1, 0x10, 0, 0, 0, 0, 0);
}

void eqip_801D064C(void)
{
}

/* Attack, Attack%, Defense, Defense%, Magic atk (always 0), Magic def and Magic def% with what the member has on; while
 * picking from the list, beside them the same with the highlighted one on, coloured by whether each goes up or down. */
#ifdef NON_MATCHING
/* 5 instructions short of the game's 351: the game keeps the current Attack in a saved register and spills one more
 * of the current values. */
void eqip_stats(int x, int y)
{
    int i, character, rec, weapon, armor;
    int attack, hit, defense, defense_pct, magic, mdefense, mdefense_pct;
    int new_attack, new_hit, new_defense, new_defense_pct, new_magic, new_mdefense, new_mdefense_pct;
    int color;

    character = save_data.party[eqip_member];
    for (i = 0; i < 7; i++)
        func_80026F44(x, y + i * 12 - 3, eqip_texts[10 + i], 5);
    for (i = 0; i < 7; i++)
        func_8002708C(x + 0x74, y + i * 12, 0xDA, 5);
    rec = character_record[character];
    new_attack = weapon_table[save_data.chars[rec].weapon].power;
    new_hit = weapon_table[save_data.chars[rec].weapon].hit;
    new_defense = armor_table[save_data.chars[rec].armor].defense;
    new_defense_pct = armor_table[save_data.chars[rec].armor].defense_pct;
    new_mdefense = armor_table[save_data.chars[rec].armor].mdefense;
    new_mdefense_pct = armor_table[save_data.chars[rec].armor].mdefense_pct;
    new_magic = 0;
    func_80028E00(x + 0x58, y, new_attack, 3, 7);
    func_80028E00(x + 0x58, y + 0xC, new_hit, 3, 7);
    func_80028E00(x + 0x58, y + 0x18, new_defense, 3, 7);
    func_80028E00(x + 0x58, y + 0x24, new_defense_pct, 3, 7);
    func_80028E00(x + 0x58, y + 0x30, new_magic, 3, 7);
    func_80028E00(x + 0x58, y + 0x3C, new_mdefense, 3, 7);
    func_80028E00(x + 0x58, y + 0x48, new_mdefense_pct, 3, 7);
    attack = new_attack;
    hit = new_hit;
    defense = new_defense;
    defense_pct = new_defense_pct;
    magic = new_magic;
    mdefense = new_mdefense;
    mdefense_pct = new_mdefense_pct;
    if (!eqip_state)
        return;
    if (eqip_cursors[0].row != 2) {
        switch (eqip_cursors[0].row) {
        case 0:
            weapon = eqip_list[eqip_cursors[1].row + eqip_cursors[1].top];
            new_attack = weapon_table[weapon].power;
            new_hit = weapon_table[weapon].hit;
            break;
        case 1:
            armor = eqip_list[eqip_cursors[1].row + eqip_cursors[1].top];
            new_defense = armor_table[armor].defense;
            new_defense_pct = armor_table[armor].defense_pct;
            new_mdefense = armor_table[armor].mdefense;
            new_mdefense_pct = armor_table[armor].mdefense_pct;
            break;
        }
    }
    if (attack < new_attack)
        color = 6;
    else if (new_attack < attack)
        color = 2;
    else
        color = 7;
    func_80028E00(x + 0x82, y, new_attack, 3, color);
    if (hit < new_hit)
        color = 6;
    else if (new_hit < hit)
        color = 2;
    else
        color = 7;
    func_80028E00(x + 0x82, y + 0xC, new_hit, 3, color);
    if (defense < new_defense)
        color = 6;
    else if (new_defense < defense)
        color = 2;
    else
        color = 7;
    func_80028E00(x + 0x82, y + 0x18, new_defense, 3, color);
    if (defense_pct < new_defense_pct)
        color = 6;
    else if (new_defense_pct < defense_pct)
        color = 2;
    else
        color = 7;
    func_80028E00(x + 0x82, y + 0x24, new_defense_pct, 3, color);
    if (magic < new_magic)
        color = 6;
    else if (new_magic < magic)
        color = 2;
    else
        color = 7;
    func_80028E00(x + 0x82, y + 0x30, new_magic, 3, color);
    if (mdefense < new_mdefense)
        color = 6;
    else if (new_mdefense < mdefense)
        color = 2;
    else
        color = 7;
    func_80028E00(x + 0x82, y + 0x3C, new_mdefense, 3, color);
    if (mdefense_pct < new_mdefense_pct)
        color = 6;
    else if (new_mdefense_pct < mdefense_pct)
        color = 2;
    else
        color = 7;
    func_80028E00(x + 0x82, y + 0x48, new_mdefense_pct, 3, color);
}
#else
INCLUDE_ASM("asm/menu/eqipmenu/nonmatchings/eqipmenu", eqip_stats);
#endif

/* The screen each frame (see the top of the file). */
#ifdef NON_MATCHING
/* The game's length (1,007); frame and the characters' table, and the list cursor's base and growth, swap
 * registers. */
int eqip_frame(int frame)
{
    RECT unused, r;
    Character *chars, *c; /* chars: the game's code reads the table through a register */
    int rec, i, rows, id, growth, old;
    unsigned char *slots, *text;

    func_800230C4(results_buf);
    chars = save_data.chars;
    rec = character_record[save_data.party[eqip_member]];
    if (eqip_state == 1) {
        if (eqip_count > 8) {
            eqip_bar.shown = 8;
            eqip_bar.total = eqip_count;
            eqip_bar.top = eqip_cursors[1].top;
            eqip_bar.x = 0x160;
            eqip_bar.y = 0x55;
            eqip_bar.w = 0xA;
            eqip_bar.h = 0x85;
            func_80028484(&eqip_bar);
        }
        func_80026B5C(9);
        rows = 8;
        if (eqip_count < 9)
            rows = eqip_count;
        if (eqip_cursors[1].scrolling)
            rows++;
        for (i = 0; i < rows; i++) {
            switch (eqip_cursors[0].row) {
            case 0:
                id = eqip_list[i + eqip_cursors[1].top] + 0x80;
                break;
            case 1:
                id = eqip_list[i + eqip_cursors[1].top] + 0x100;
                break;
            case 2:
                id = eqip_list[i + eqip_cursors[1].top] + 0x120;
                break;
            }
            func_80026F44(0xF6, i * 16 + 0x5A + eqip_cursors[1].scroll_step * 4, func_80021280(id), 7);
        }
    }
    setRECT(&r, 0xB2, 0x55, 0xBA, 0x85);
    func_80026A94(&results_draw[results_buf], &r);
    switch (eqip_state) {
    case 0:
        func_8001EB2C(0x7C, eqip_cursors[0].row * 16 + 0xD);
        if (eqip_cursors[0].row == 2)
            break;
        func_80026F44(0x10, eqip_windows[2].y + 0xA, eqip_texts[17], 5);
        func_80026F44(0x10, eqip_windows[2].y + 0x1E, eqip_texts[18], 5);
        switch (eqip_cursors[0].row) {
        case 0:
            id = save_data.chars[character_record[save_data.party[eqip_member]]].weapon;
            eqip_slots(0x5C, eqip_windows[2].y + 0xA, weapon_table[id].slots);
            growth = weapon_table[id].growth;
            break;
        case 1:
            id = save_data.chars[character_record[save_data.party[eqip_member]]].armor;
            eqip_slots(0x5C, eqip_windows[2].y + 0xA, armor_table[id].slots);
            growth = armor_table[id].growth;
            break;
        }
        if (growth >= 4)
            growth = 4;
        else if (growth < 0)
            growth = 4;
        text = eqip_texts[19 + growth];
        func_80026F44(0x92 - func_80026B70(text) / 2, eqip_windows[2].y + 0x1E, text, 7);
        break;
    case 1:
        if (frame & 2)
            func_8001EB2C(0x7C, eqip_cursors[0].row * 16 + 0xD);
        func_8001EB2C(0xDD, eqip_cursors[1].row * 16 + 0x5C);
        if (eqip_cursors[0].row == 2)
            break;
        func_80026F44(0x10, eqip_windows[2].y + 0xA, eqip_texts[17], 5);
        func_80026F44(0x10, eqip_windows[2].y + 0x1E, eqip_texts[18], 5);
        switch (eqip_cursors[0].row) {
        case 0:
            id = eqip_list[eqip_cursors[1].row + eqip_cursors[1].top];
            eqip_slots(0x5C, eqip_windows[2].y + 0xA, weapon_table[id].slots);
            growth = weapon_table[id].growth;
            break;
        case 1:
            id = eqip_list[eqip_cursors[1].row + eqip_cursors[1].top];
            eqip_slots(0x5C, eqip_windows[2].y + 0xA, armor_table[id].slots);
            growth = armor_table[id].growth;
            break;
        }
        if (growth >= 4)
            growth = 4;
        else if (growth < 0)
            growth = 4;
        text = eqip_texts[19 + growth];
        func_80026F44(0x92 - func_80026B70(text) / 2, eqip_windows[2].y + 0x1E, text, 7);
        break;
    }
    func_80026B5C(8);
    for (i = 0; i < 3; i++)
        func_80026F44(0x96, i * 16 + 0xB, eqip_texts[i], 5);
    eqip_stats(0x20, eqip_windows[3].y + 0xC);
    setRECT(&r, 0, 0, 0x100, 0x100);
    func_80026A34(0, 1, 0x7F, &r);
    func_80020B68(0x42, 0xD, eqip_member);
    func_8001CF3C(10, 9, 0x30, 0x30, 0, eqip_member * 48 + 0x38, 0x30, 0x30, eqip_member + 0xD, 0);
    setRECT(&r, 0, 0, 0x100, 0x100);
    func_80026A34(0, 1, 0x7F, &r);
    func_80026B5C(10);
    c = &chars[rec];
    func_80026F44(0xB6, 0xB, func_80021280(c->weapon + 0x80), 7);
    func_80026F44(0xB6, 0x1B, func_80021280(c->armor + 0x100), 7);
    if (c->accessory != 0xFF)
        func_80026F44(0xB6, 0x2B, func_80021280(c->accessory + 0x120), 7);
    if (eqip_state == 1) {
        switch (eqip_cursors[0].row) {
        case 0:
            func_80026F44(0x18, eqip_windows[1].y + 6,
                          kind_text(4, eqip_list[eqip_cursors[1].top + eqip_cursors[1].row] + 0x80, 0), 7);
            break;
        case 1:
            func_80026F44(0x18, eqip_windows[1].y + 6,
                          kind_text(4, eqip_list[eqip_cursors[1].top + eqip_cursors[1].row] + 0x100, 0), 7);
            break;
        case 2:
            func_80026F44(0x18, eqip_windows[1].y + 6,
                          kind_text(4, eqip_list[eqip_cursors[1].top + eqip_cursors[1].row] + 0x120, 0), 7);
            break;
        }
    } else {
        switch (eqip_cursors[0].row) {
        case 0:
            func_80026F44(
                0x18, eqip_windows[1].y + 6,
                kind_text(4, save_data.chars[character_record[save_data.party[eqip_member]]].weapon + 0x80, 0), 7);
            break;
        case 1:
            func_80026F44(
                0x18, eqip_windows[1].y + 6,
                kind_text(4, save_data.chars[character_record[save_data.party[eqip_member]]].armor + 0x100, 0), 7);
            break;
        case 2:
            if (save_data.chars[character_record[save_data.party[eqip_member]]].accessory != 0xFF)
                func_80026F44(
                    0x18, eqip_windows[1].y + 6,
                    kind_text(4, save_data.chars[character_record[save_data.party[eqip_member]]].accessory + 0x120, 0),
                    7);
            break;
        }
    }
    for (rows = 0; rows < 5; rows++)
        func_8001E040(&eqip_windows[rows]);
    if (func_80023050())
        return;
    func_800264A8(&eqip_cursors[eqip_state]);
    switch (eqip_state) {
    case 0:
        if (D_80062D7E & 4) {
            do {
                if (--eqip_member < 0)
                    eqip_member = 2;
            } while (save_data.party[eqip_member] == 0xFF);
        } else if (D_80062D7E & 8) {
            do {
                if (++eqip_member >= 3)
                    eqip_member = 0;
            } while (save_data.party[eqip_member] == 0xFF);
        }
        if (D_80062D7E & 0x40) {
            if (eqip_changed && (D_80062FA4 || D_80063024))
                goto materia;
            eqip_sound(4);
            func_8002305C(5, 0);
            func_8002120C(0);
        } else if (D_80062D7C & 0x20) {
            eqip_fill_list(eqip_cursors[0].row);
            if (!eqip_count || D_8009D4F7 & 1) {
                eqip_sound(3);
                break;
            }
            eqip_sound(1);
            eqip_state = 1;
            rows = 8;
            if (eqip_count < 9)
                rows = eqip_count;
            func_80026448(&eqip_cursors[1], 0, 0, 1, rows, 0, 0, 1, eqip_count, 0, 0, 0, 0, 0);
        } else if (D_80062D7E & 0x80 && (D_80062FA4 || D_80063024)) {
        materia:
            eqip_sound(1);
            func_8002305C(4, 3);
            func_8002120C(3);
            menu_member = eqip_member;
        } else if (D_80062D7E & 0x10 && !(D_8009D4F7 & 1) && eqip_cursors[0].row == 2) {
            eqip_sound(4);
            c = &chars[rec];
            if (c->accessory != 0xFF)
                func_80025380(c->accessory + 0x120 | 0x200);
            c->accessory = 0xFF;
            goto update;
        }
        break;
    case 1:
        if (D_80062D7C & 0x20) {
            eqip_sound(0x1BF);
            eqip_state = 0;
            id = eqip_list[eqip_cursors[1].row + eqip_cursors[1].top];
            switch (eqip_cursors[0].row) {
            case 0:
                old = chars[rec].weapon;
                chars[rec].weapon = id;
                slots = weapon_table[id].slots;
                id += 0x80;
                eqip_changed = 1;
                old += 0x80;
                for (i = 0; i < 8; i++) {
                    if (!slots[i] && chars[rec].weapon_materia[i] != -1) {
                        func_800254D8();
                        materia_add(chars[rec].weapon_materia[i]);
                        func_800254D8();
                        chars[rec].weapon_materia[i] = -1;
                    }
                }
                func_80025380(old | 0x200);
                break;
            case 1:
                old = chars[rec].armor;
                chars[rec].armor = id;
                id += 0x100;
                eqip_changed = 1;
                old += 0x100;
                slots = armor_slots(eqip_member);
                for (i = 0; i < 8; i++) {
                    if (!slots[i] && chars[rec].armor_materia[i] != -1) {
                        func_800254D8();
                        materia_add(chars[rec].armor_materia[i]);
                        func_800254D8();
                        chars[rec].armor_materia[i] = -1;
                    }
                }
                func_80025380(old | 0x200);
                break;
            case 2:
                old = chars[rec].accessory;
                chars[rec].accessory = id;
                if (old != 0xFF)
                    func_80025380(old + 0x120 | 0x200);
                id += 0x120;
                break;
            }
            func_80025288(id | 0x200);
        update:
            func_80020058(eqip_member);
            member_update(eqip_member);
            party_effects_sum();
        } else if (D_80062D7C & 0x40)
            eqip_state = 0;
        break;
    }
}
#else
INCLUDE_ASM("asm/menu/eqipmenu/nonmatchings/eqipmenu", eqip_frame);
#endif

void eqip_801D1B8C(void)
{
}
