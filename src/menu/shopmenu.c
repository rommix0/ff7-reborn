/* SHOPMENU.MNU, 0x801D0084-0x801D42C8 (two jump tables before it, data after it to 0x801DF1C4): the shops. The field
 * asks for shop N (menu request 8) and the exe calls shop_run, which runs the shop's own frame loop until it closes.
 * Buy lists the shop's stock with its price and who can equip it (Attack or Defense now and with it); a weapon or
 * armor shows its slots and growth while choosing how many. Sell picks items (at half price; ones that can't be sold are
 * greyed) or a materia from the stock, sold for its AP or 70 times its price once mastered (see shop_materia_value), with
 * the materia menu's page for it. The shop's titles, clerk lines, stock and prices are data here (tools/shop.py).
 * GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

extern unsigned char D_80077F64[2][0x3400]; /* the menus' primitive buffers */
extern struct {
    unsigned short items[320];
    int materia[200];
} inventory;
extern unsigned int save_gil;
extern unsigned char materia_detail;
extern unsigned char text_page_per_line;

unsigned short func_80025310(); /* takes an unsigned short, but the game passes its item numbers unextended */
unsigned short func_80025288(); /* the same */
void func_80025380(); /* the same */
unsigned int func_80025B10(unsigned int n); /* takes gil */
void func_80025B48(unsigned int n); /* gives gil */
int func_8001FAF8(int item); /* where it can't be used: bit 0 sold */
void materia_own(unsigned int materia);
void func_80026B5C(int layer); /* empty */
int func_8002603C(int materia); /* its icon's palette */
void func_80026B64(unsigned char max);
void func_80027990(int x, int y, int w, int h, int c); /* a highlight */
unsigned char *func_80021258(int materia); /* a materia's name */

typedef struct ShopEntry {
    short kind; /* 0 an inventory number, 1 a materia */
    short pad2;
    int id;
} ShopEntry;

typedef struct Shop {
    unsigned char title;
    unsigned char line_set;
    short count;
    ShopEntry stock[10];
} Shop;

extern unsigned char shop_titles[9][20];
extern unsigned char shop_lines[2][10][46]; /* the clerk's, by line set; the second in dialect, which no shop uses */
extern Shop shops[80];
extern unsigned char shop_elements[9][10];
/* The materia menu's texts (from EQIPMENU): [6] To next level, [7] MASTER, [18] on the stats' names, [23] on the
 * independent materia's. */
extern unsigned char shop_texts[40][20];
extern int shop_fade_level;
extern unsigned char shop_growth[4][22]; /* None, Normal, Double, Triple; [4] is shop_letters' bytes */
extern unsigned char shop_letters[2][2]; /* A and D */
extern unsigned int shop_price_table[];
extern RECT shop_windows[16]; /* [15] the title's */
extern MateriaInfo *shop_info;
extern struct {
    unsigned int items[384];
    unsigned int materia[128];
} *shop_prices;
extern int shop_id;
extern int shop_title;
extern int shop_line_set;
extern int D_801D70F0;
extern int shop_phase; /* -1 done */
extern int shop_buf;
extern unsigned int *shop_ot;
extern unsigned int shop_ots[2];
extern DRAWENV shop_draw[2];
extern DISPENV shop_disp[2];
extern MenuCursor shop_cursors[10];
extern ScrollBar shop_bar;
extern unsigned char shop_vram[0x2800];
extern unsigned char shop_vram2[0x2D00];
extern unsigned char shop_vram3[0x600];
extern int shop_state;
extern unsigned char shop_menu_text[10][20];
extern MateriaInfo shop_info_buf;
extern int shop_quantity;
extern int shop_slide; /* the Sell box slides in over 10 frames */

int shop_frame(int frame);

void shop_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

/* ITEMMENU's item_icon, with its unread fourth argument. */
void shop_icon(int x, int y, int item, int unused)
{
    unsigned int kind;
    int u, v;

    if (item < 0x80)
        kind = 0;
    else if (item < 0x100) {
        item -= 0x80;
        if (item < 0x10)
            kind = 1;
        else if (item < 0x20)
            kind = 3;
        else if (item < 0x30)
            kind = 2;
        else if (item < 0x3E)
            kind = 5;
        else if (item < 0x49)
            kind = 4;
        else if (item < 0x57)
            kind = 9;
        else if (item < 0x65)
            kind = 6;
        else if (item < 0x72)
            kind = 7;
        else
            kind = 8;
    } else if (item < 0x120)
        kind = 10;
    else
        kind = 11;
    u = (kind & 1) * 16 | 0x60;
    v = (kind >> 1) * 16 + 0x70;
    func_80028CA0(x, y, u, v, 16, 16, 1, 0);
}

int shop_negative(int x)
{
    return x < 0;
}

int shop_abs(int x)
{
    return x < 0 ? -x : x;
}

/* What equipping the item would change, a row for each stat that does: an arrow, by how much, and its name. */
void shop_draw_changes(void)
{
    int i, color;
    int rows;

    for (i = 0, rows = 0; i < 8; i++) {
        if (shop_info->stats[i]) {
            if (shop_negative(shop_info->stats[i])) {
                func_8002708C(0xB4, rows * 11 + 0x9A, 0xE1, 7);
                color = 2;
            } else {
                func_8002708C(0xB4, rows * 11 + 0x9A, 0xB3, 7);
                color = 6;
            }
            func_80029114(0xBC, rows * 11 + 0x9A, shop_abs(shop_info->stats[i]), 2, color);
            func_80026A34(0, 1, 0x1F, 0);
            func_80026F44(0x78, rows * 11 + 0x98, shop_texts[18 + i], 7);
            if (i >= 6)
                func_8002708C(0xCA, rows * 11 + 0x9A, 0xD3, 7);
            rows++;
        }
    }
    func_80026A34(0, 1, 0x1F, 0);
}

/* A materia's page (the materia menu's, kept here): its name, element and stars, its AP and the AP to the next level,
 * then by materia_info_kind its abilities, its bonus or, for Enemy Skill, the learned skills in a list that scrolls
 * (clipped to env) and a grid of all 24. The master materia show only their name. */
#ifdef NON_MATCHING
/* Same code, but 17 instructions short: the game keeps the Master Magic case's tests (GCC drops them here), loads 0xFF
 * afresh in the ability loops, and spills Enemy Skill's scroll differently. */
void shop_draw_materia(int m, int scroll, DRAWENV *env)
{
    int id = m & 0xFF;
    int element, i, j, k, y, top, skills, color, pal;
    RECT r;

    if (m == -1)
        return;
    kind_text(8, 0xB, 8);
    element = materia_data[id].element;
    if (element < 9)
        func_80026F44(0x18, 0x72, shop_elements[element], 6);
    func_80026F44(0x18, 0x64, func_80021258(id), 7);
    pal = func_8002603C(id);
    func_80028CA0(8, 0x63, 0x80, 0x20, 16, 16, (short)pal, 0);
    if (id != 0x30 && id != 0x49 && id != 0x5A && id != 0x2C) {
        for (i = 0; i < shop_info->levels; i++) {
            if (i < shop_info->level)
                func_80028CA0(0x7C + i * 15, 0x63, 0x90, 0x20, 16, 16, (short)pal, 0);
            else
                func_80028CA0(0x7C + i * 15, 0x63, 0x90, 0x30, 16, 16, (short)pal, 0);
        }
        func_80028E00(0x96, 0x7E, shop_info->ap_to_next, 7, 7);
        if ((unsigned int)m >> 8 != 0xFFFFFF) {
            func_80028E00(0x8F, 0x72, (unsigned int)m >> 8, 8, 7);
            func_80026A34(0, 1, 0x1F, 0);
        } else {
            func_80026A34(0, 1, 0x1F, 0);
            func_80026F44(0x96, 0x72, shop_texts[7], 7);
        }
        func_80026F44(0x3C, 0x7E, shop_texts[6], 5);
        func_80026F44(0x70, 0x8C, shop_texts[1], 5);
        func_80026F44(0x7C, 0x72, shop_texts[5], 5);
        func_80026F44(8, 0x8C, shop_texts[0], 5);
        func_80026F44(0x70, 0x8C, shop_texts[1], 5);
        switch (materia_info_kind) {
        case 1:
            for (i = 0, y = 0x98; i < 5; i++) {
                if (shop_info->lines[i].id != 0xFF) {
                    color = shop_info->lines[i].value ? 7 : 0;
                    func_80026F44(0x18, y, kind_text(0, shop_info->lines[i].id, 8), color);
                    y += 12;
                }
            }
            break;
        case 2:
            func_80026F44(0x14, 0x98, kind_text(1, shop_info->lines[0].id, 8), 7);
            break;
        case 3:
        case 0x13:
            for (i = 0, y = 0x98; i < 5; i++) {
                if (shop_info->lines[i].id != 0xFF) {
                    color = shop_info->lines[i].value ? 7 : 0;
                    func_80026F44(0x18, y, kind_text(5, shop_info->lines[i].id, 8), color);
                    y += 12;
                }
            }
            break;
        default:
            switch (id) {
            case 0:
                func_80026F44(0x12, 0x98, shop_texts[27], 7);
                func_80029114(0x54, 0x9A, shop_info->lines[0].value, 2, 6);
                func_8002708C(0x4C, 0x9A, 0xB3, 7);
                func_8002708C(0x62, 0x9A, 0xD3, 7);
                break;
            case 1:
                func_80026F44(0x12, 0x98, shop_texts[26], 7);
                func_80029114(0x54, 0x9A, shop_info->lines[0].value, 2, 6);
                func_8002708C(0x4C, 0x9A, 0xB3, 7);
                func_8002708C(0x62, 0x9A, 0xD3, 7);
                break;
            case 2:
                func_80026F44(0x12, 0x98, shop_texts[13], 7);
                func_80029114(0x56, 0x9A, shop_info->lines[0].value, 2, 6);
                func_8002708C(0x4E, 0x9A, 0xB3, 7);
                func_8002708C(0x64, 0x9A, 0xD3, 7);
                break;
            case 3:
                func_80026F44(0x12, 0x98, shop_texts[15], 7);
                func_80029114(0x44, 0x9A, shop_info->lines[0].value, 2, 6);
                func_8002708C(0x3C, 0x9A, 0xB3, 7);
                func_8002708C(0x52, 0x9A, 0xD3, 7);
                break;
            case 4:
                func_80026F44(0x12, 0x98, shop_texts[16], 7);
                func_80029114(0x3C, 0x9A, shop_info->lines[0].value, 2, 6);
                func_8002708C(0x34, 0x9A, 0xB3, 7);
                func_8002708C(0x4A, 0x9A, 0xD3, 7);
                break;
            case 16:
                func_80026F44(0x12, 0x98, shop_texts[14], 7);
                func_80029114(0x44, 0x9A, shop_info->lines[0].value, 3, 6);
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
                func_80026F44(0x12, 0x98, shop_texts[23 + id], 7);
                break;
            default:
                func_80026F44(0x12, 0x98, func_80021258(id), 7);
                break;
            }
            func_80026A34(0, 1, 0x1F, 0);
            break;
        }
        shop_draw_changes();
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
INCLUDE_ASM("asm/menu/shopmenu/nonmatchings/shopmenu", shop_draw_materia);
#endif

/* NAMEMENU's name_fade. */
int shop_fade(int step)
{
    RECT tw;

    setlen(prim_next.tile, 3);
    setcode(prim_next.tile, 0x60);
    func_80046848(prim_next.tile, 1);
    prim_next.tile->x0 = 0;
    prim_next.tile->y0 = 0;
    prim_next.tile->w = 0x180;
    prim_next.tile->h = 0xE8;
    prim_next.tile->r0 = shop_fade_level;
    prim_next.tile->g0 = shop_fade_level;
    prim_next.tile->b0 = shop_fade_level;
    AddPrim(prim_ot, prim_next.tile++);
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x5F, &tw);
    shop_fade_level += step;
    if (shop_fade_level < 0)
        shop_fade_level = 0;
    if (shop_fade_level >= 0x100)
        shop_fade_level = 0xFF;
    return shop_fade_level;
}

/* A weapon's or armor's materia slots (0 none, 1-4 a slot, 5-7 one with growth; 3 and 7 linked to the next) on a
 * panel. */
void shop_draw_slots(int x, int y, unsigned char *slots)
{
    int i;
    RECT r;
    unsigned char *s;

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

/* How many of materia `id` the stock holds. */
int shop_stock_count(int id)
{
    int i, n;

    if (id == 0xFF)
        return 0;
    for (i = 0, n = 0; i < 200; i++) {
        if ((save_data.materia[i] & 0xFF) == id)
            n++;
    }
    return n;
}

void shop_open(int n)
{
    text_page_per_line = 1;
    shop_id = n;
    shop_line_set = shops[n].line_set;
    shop_title = shops[n].title;
    D_801D70F0 = 0;
}

/* What a materia sells for: its AP, or 70 times its price once mastered; one priced 1 sells for 1. */
int shop_materia_value(int m)
{
    int price;

    if (m == -1)
        return 0;
    price = shop_prices->materia[m & 0xFF];
    if (price == 1)
        return 1;
    m = (unsigned int)m >> 8;
    if (m == 0xFFFFFF)
        m = price * 70;
    return m;
}

/* The same for a mastered one. */
int shop_master_value(int m)
{
    int price;

    if (m == -1)
        return 0;
    price = shop_prices->materia[m & 0xFF];
    if (price == 1)
        return 1;
    return price * 70;
}

/* The shop's title centred on its panel. The assignment to w keeps GCC from folding 0x3B into x, as the game has it. */
void shop_draw_title(void)
{
    int w;

    func_80026B5C(0x80);
    func_80026F44(shop_windows[15].x - (w = func_80026B70(shop_titles[shop_title]) / 2 - 0x3B), 0xB,
                  shop_titles[shop_title], 7);
    func_8001E040(&shop_windows[15]);
}

/* Each joined character's portrait (the party's blinking) with Attack and Defense now and with the highlighted weapon
 * or armor on, the new value coloured by whether it goes up or down. */
#ifdef NON_MATCHING
/* Same length; the game loads the highlighted entry's id before its kind and keeps the index in another register. */
void shop_draw_party(int frame)
{
    int i, x, y, member, color, id;
    int attack, new_attack, defense, new_defense;
    RECT tw;

    for (i = 0; i < 3; i++) {
        member = save_data.party[i];
        if (member == 0xFF)
            continue;
        color = frame & 0x7F;
        x = member % 3 * 114 + 14;
        y = member / 3 * 25 + 0x91;
        if (color >= 0x40)
            color = 0x7F - color;
        func_80027990(x, y, 0x18, 0x18, color * 4);
    }
    func_80026B64(2);
    for (i = 0; i < 9; i++) {
        if (!(save_data.joined >> i & 1))
            continue;
        x = i % 3 * 114 + 14;
        y = i / 3 * 25 + 0x91;
        func_8001D180(x, y, 0x18, 0x18, i < 5 ? 0 : 0x30, i % 5 * 48, 0x30, 0x30, i, 0);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x3F, &tw);
        new_attack = weapon_table[save_data.chars[i].weapon].power;
        new_defense = armor_table[save_data.chars[i].armor].defense;
        id = shops[shop_id].stock[shop_cursors[1].top + shop_cursors[1].row].id;
        defense = new_defense;
        attack = new_attack;
        switch (shops[shop_id].stock[shop_cursors[1].top + shop_cursors[1].row].kind) {
        case 0:
            if (id < 0x80)
                continue;
            if (id < 0x100) {
                if (!(weapon_table[id - 0x80].equip >> i & 1))
                    continue;
                new_attack = weapon_table[id - 0x80].power;
            } else if (id < 0x120) {
                if (!(armor_table[id - 0x100].equip >> i & 1))
                    continue;
                new_defense = armor_table[id - 0x100].defense;
            } else if (id < 0x140)
                continue;
            break;
        case 1:
            continue;
        case 2:
            continue;
        }
        if (attack < new_attack)
            color = 6;
        else if (new_attack < attack)
            color = 2;
        else
            color = 7;
        func_80026F44(x + 0x1D, y + 0xE, shop_letters[1], 5);
        func_80026F44(x + 0x1D, y + 2, shop_letters[0], 5);
        func_80028E00(x + 0x30, y + 4, attack, 3, 7);
        func_8002708C(x + 0x45, y + 4, 0xDA, 5);
        func_80028E00(x + 0x4D, y + 4, new_attack, 3, color);
        if (defense < new_defense)
            color = 6;
        else if (new_defense < defense)
            color = 2;
        else
            color = 7;
        func_80028E00(x + 0x30, y + 0x10, defense, 3, 7);
        func_8002708C(x + 0x45, y + 0x10, 0xDA, 5);
        func_80028E00(x + 0x4D, y + 0x10, new_defense, 3, color);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x3F, &tw);
    }
    func_80026B64(0x80);
}
#else
INCLUDE_ASM("asm/menu/shopmenu/nonmatchings/shopmenu", shop_draw_party);
#endif

/* How many of the highlighted thing on sale the party has in its stock. */
int shop_owned(void)
{
    int kind = shops[shop_id].stock[shop_cursors[1].row + shop_cursors[1].top].kind;
    int id = shops[shop_id].stock[shop_cursors[1].row + shop_cursors[1].top].id;
    int n;

    switch (kind) {
    case 0:
        n = func_80025310(id);
        if (n == 0xFFFF) {
            n = 0;
            break;
        }
        n >>= 9;
        break;
    case 1:
        n = shop_stock_count(id & 0xFF);
        break;
    case 2:
        n = 0;
        break;
    }
    return n;
}

/* How many of materia `id` the joined characters have on. */
int shop_equipped_materia(int id)
{
    int i, j, n;

    if (id == 0xFF)
        return 0;
    n = 0;
    for (i = 0; i < 9; i++) {
        if (!(save_data.joined >> i & 1))
            continue;
        for (j = 0; j < 8; j++) {
            if ((save_data.chars[i].weapon_materia[j] & 0xFF) == (id & 0xFF))
                n++;
        }
        for (j = 0; j < 8; j++) {
            if ((save_data.chars[i].armor_materia[j] & 0xFF) == (id & 0xFF))
                n++;
        }
    }
    return n;
}

/* How many of the highlighted thing on sale the joined characters have on. */
int shop_equipped(void)
{
    int i, j, n;
    int kind, id;

    n = 0;
    id = shops[shop_id].stock[shop_cursors[1].row + shop_cursors[1].top].id;
    for (i = 0; i < 9; i++) {
        if (!(save_data.joined >> i & 1))
            continue;
        kind = shops[shop_id].stock[shop_cursors[1].row + shop_cursors[1].top].kind;
        switch (kind) {
        case 0:
            if (id < 0x80)
                break;
            if (id < 0x100) {
                if (save_data.chars[i].weapon + 0x80 == id)
                    n++;
            } else if (id < 0x120) {
                if (save_data.chars[i].armor + 0x100 == id)
                    n++;
            } else if (id < 0x140) {
                if (save_data.chars[i].accessory + 0x120 == id)
                    n++;
            }
            break;
        case 1:
            for (j = 0; j < 8; j++) {
                if ((save_data.chars[i].weapon_materia[j] & 0xFF) == (id & 0xFF))
                    n++;
            }
            for (j = 0; j < 8; j++) {
                if ((save_data.chars[i].armor_materia[j] & 0xFF) == (id & 0xFF))
                    n++;
            }
            break;
        case 2:
            break;
        }
    }
    return n;
}

/* How many of `item` the joined characters have on. The accessory test reads the armor. */
int shop_equipped_item(int item)
{
    int i, n;

    for (n = 0, i = 0; i < 9; i++) {
        if (!(save_data.joined >> i & 1))
            continue;
        if (item < 0x80)
            continue;
        if (item < 0x100) {
            if (save_data.chars[i].weapon + 0x80 == item)
                n++;
        } else if (item < 0x120) {
            if (save_data.chars[i].armor + 0x100 == item)
                n++;
        } else if (item < 0x140) {
            if (save_data.chars[i].armor + 0x120 == item)
                n++;
        }
    }
    return n;
}

void shop_init(void)
{
    RECT unused;

    shop_prices = (void *)shop_price_table;
    func_80026448(&shop_cursors[0], 0, 0, 3, 1, 0, 0, 3, 1, 0, 0, 1, 0, 0);
    func_80026448(&shop_cursors[2], 0, 0, 2, 10, 0, 0, 2, 0xA0, 0, 0, 2, 0, 0);
    func_80026448(&shop_cursors[3], 0, 0, 1, 10, 0, 0, 1, 0xC8, 0, 0, 0, 0, 0);
    func_80025CD4(shop_vram3);
    DrawSync(0);
    func_80025B8C(shop_vram2);
    DrawSync(0);
    func_80025C14(shop_vram);
    DrawSync(0);
    func_80025DF8();
    shop_state = 0;
}

/* How many more of the highlighted thing the party can hold (99 in all). */
int shop_room(void)
{
    int on = shop_equipped();

    return 99 - shop_owned() - on;
}

/* Whether the stock has a free place. */
int shop_has_space(void)
{
    int i, space;

    space = 0;
    for (i = 0; i < 200; i++) {
        if (save_data.materia[i] == -1)
            space = 1;
    }
    return space;
}

/* The shop each frame, by shop_state: 0 Buy, Sell or Exit; 1 the stock (with who can equip it); 4 how many to buy;
 * 6 sell items or materia; 2 the inventory to sell from; 5 how many to sell; 3 the materia stock to sell from, with the
 * materia's page. Items sell for half their price, materia for shop_materia_value. Triangle backs out; Exit closes the
 * shop at once, triangle on the first menu fades it out. */
#ifdef NON_MATCHING
/* 6 instructions short of the game's 2,297: the game spills more (the Sell box's x, the inventory row and the row
 * count) and keeps other registers and loads in the selling paths. */
int shop_frame(int frame)
{
    int i, j, n, x, rows, owned, equipped, count, id, kind, growth, color;
    unsigned char *text;
    RECT box, tw;

    if (!shop_phase) {
        if (!shop_fade(-10))
            shop_phase = 1;
    } else if (shop_phase == 2) {
        if (shop_fade(10) == 0xFF)
            shop_phase = -1;
    }
    shop_info = &shop_info_buf;
    materia_info = &shop_info_buf;
    shop_draw_title();
    switch (shop_state) {
    case 6:
        func_80026B5C(0x80);
        x = shop_cursors[6].col * 40 + 6;
        func_8001EB2C(shop_windows[10].x + x - 0x13, shop_windows[10].y + 8);
        func_80026F44(shop_windows[10].x + 0xC, shop_windows[10].y + 6, shop_menu_text[6], 7);
        func_8001E040(&shop_windows[10]);
        if (frame & 2)
            func_8001EB2C(shop_windows[7].x + shop_cursors[0].col * 30 - 0x13, shop_windows[7].y + 8);
        func_80026F44(shop_windows[7].x + 8, shop_windows[7].y + 6, shop_menu_text[0], 7);
        func_8001E040(&shop_windows[7]);
        func_8001E040(&shop_windows[8]);
        func_80026F44(0xC, 0xB, shop_lines[shop_line_set][3], 7);
        break;
    case 0:
        func_80026B5C(0x80);
        func_80026F44(0xC, 0xB, shop_lines[shop_line_set][0], 7);
        func_8001EB2C(shop_windows[7].x + shop_cursors[0].col * 30 - 0x13, shop_windows[7].y + 8);
        func_80026F44(shop_windows[7].x + 8, shop_windows[7].y + 6, shop_menu_text[0], 7);
        func_8001E040(&shop_windows[7]);
        break;
    case 3:
        owned = shop_stock_count(save_data.materia[shop_cursors[3].row + shop_cursors[3].top] & 0xFF);
        equipped = shop_equipped_materia(save_data.materia[shop_cursors[3].row + shop_cursors[3].top] & 0xFF);
        func_8001EB2C(0xD0, shop_cursors[3].row * 12 + 0x64);
        func_80026F44(0xC, 0xB, shop_lines[shop_line_set][3], 7);
        if (save_data.materia[shop_cursors[3].row + shop_cursors[3].top] != -1)
            func_80026F44(0x10, shop_windows[12].y + 6,
                          kind_text(0xD, save_data.materia[shop_cursors[3].row + shop_cursors[3].top] & 0xFF, 0), 7);
        func_80026B5C(0xA);
        materia_detail = 1;
        materia_own(save_data.materia[shop_cursors[3].row + shop_cursors[3].top]);
        shop_draw_materia(save_data.materia[shop_cursors[3].row + shop_cursors[3].top], frame, &shop_draw[shop_buf]);
        setRECT(&tw, 0, 5, 0x16C, 0xDB);
        func_80026A94(&shop_draw[shop_buf], &tw);
        rows = shop_cursors[3].scrolling ? 11 : 10;
        for (i = 0; i < rows; i++) {
            if (save_data.materia[shop_cursors[3].top + i] != -1)
                func_80026F44(0xF6, i * 12 + 0x62 + shop_cursors[3].scroll_step * 3,
                              func_80021258(save_data.materia[shop_cursors[3].top + i] & 0xFF), 7);
        }
        shop_bar.shown = 10;
        shop_bar.total = 200;
        shop_bar.top = shop_cursors[3].top;
        shop_bar.x = 0x160;
        shop_bar.y = 0x61;
        shop_bar.w = 10;
        shop_bar.h = 0x79;
        func_80028484(&shop_bar);
        for (i = 0; i < rows; i++) {
            color = func_8002603C(save_data.materia[shop_cursors[3].top + i] & 0xFF);
            if (save_data.materia[shop_cursors[3].top + i] != -1)
                func_80028CA0(0xE8, i * 12 + 0x62 + shop_cursors[3].scroll_step * 3, 0x80, 0x20, 16, 16, (short)color,
                              0);
        }
        setRECT(&tw, 0, 0x61, 0x16C, 0x79);
        func_80026A94(&shop_draw[shop_buf], &tw);
        func_80028E00(shop_windows[6].x + 0x23, shop_windows[6].y - 4,
                      shop_materia_value(save_data.materia[shop_cursors[3].row + shop_cursors[3].top]) + save_gil, 10, 7);
        func_80028E00(0x7C, shop_windows[6].y - 1,
                      shop_master_value(save_data.materia[shop_cursors[3].row + shop_cursors[3].top]), 10, 7);
        func_80028E00(0x7C, shop_windows[6].y - 0xD,
                      shop_materia_value(save_data.materia[shop_cursors[3].row + shop_cursors[3].top]), 10, 7);
        func_80028E00(shop_windows[6].x + 0x23, shop_windows[6].y + 0xB, save_gil, 10, 7);
        func_80028E00(0x23, shop_windows[6].y + 0xB, owned, 8, 7);
        func_80028E00(0x8A, shop_windows[6].y + 0xB, equipped, 8, 7);
        setRECT(&tw, 0, 0, 0xFF, 0xFF);
        func_80026A34(0, 1, 0x3F, &tw);
        func_80026F44(shop_windows[6].x + 0x12, shop_windows[6].y + 9, shop_menu_text[1], 5);
        func_80026F44(0x12, shop_windows[6].y + 9, shop_menu_text[2], 5);
        func_80026F44(0x71, shop_windows[6].y + 9, shop_menu_text[3], 5);
        func_80026F44(shop_windows[6].x - 0x26, shop_windows[6].y - 6, shop_menu_text[7], 5);
        func_80026F44(0x12, shop_windows[6].y - 3, shop_menu_text[8], 5);
        func_80026F44(0x12, shop_windows[6].y - 0xF, shop_menu_text[9], 5);
        func_8001E040(&shop_windows[11]);
        func_8001E040(&shop_windows[12]);
        func_8001E040(&shop_windows[13]);
        func_8001E040(&shop_windows[14]);
        break;
    case 4:
        id = shops[shop_id].stock[shop_cursors[1].row + shop_cursors[1].top].id;
        shop_windows[9].y = shop_windows[2].y + (shop_cursors[1].row * 16 + 6);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_8001DE0C(&box, shop_windows[9].x + (shop_windows[9].w - 0xE6), shop_windows[9].y + shop_windows[9].h - 3,
                      0xE6, 0x30);
        func_80028E00(shop_windows[9].x + 0x28, shop_windows[9].y + 0xA, shop_quantity, 10, 7);
        func_80028E00(shop_windows[9].x + 0x28, shop_windows[9].y + 0x17,
                      shop_prices->items[id & 0x1FF] * shop_quantity, 10, 7);
        func_80026A34(0, 1, 0x3F, &tw);
        func_80026F44(shop_windows[9].x + 6, shop_windows[9].y + 8, shop_menu_text[5], 5);
        func_80026F44(shop_windows[9].x + 6, shop_windows[9].y + 0x15, shop_menu_text[4], 5);
        func_8001E040(&shop_windows[9]);
        if (id >= 0x80 && id < 0x100)
            kind = 1;
        else
            kind = (id >= 0x100 && id < 0x120) * 2;
        if (kind) {
            func_80026F44(box.x + 0x10, box.y + 9, kind_text(8, 0x28, 8), 5);
            func_80026F44(box.x + 0x10, box.y + 0x1D, kind_text(8, 0x29, 8), 5);
            switch (kind) {
            case 1:
                shop_draw_slots(box.x + 0x5C, box.y + 9, weapon_table[id - 0x80].slots);
                growth = weapon_table[id - 0x80].growth;
                break;
            case 2:
                shop_draw_slots(box.x + 0x5C, box.y + 9, armor_table[id - 0x100].slots);
                growth = armor_table[id - 0x100].growth;
                break;
            }
            if (growth >= 4)
                growth = 4;
            else if (growth < 0)
                growth = 4;
            text = shop_growth[growth];
            func_80026F44(box.x - (j = func_80026B70(text) / 2 - 0x92), box.y + 0x1D, text, 7);
            func_80026A34(0, 1, 0x3F, &tw);
            func_8001E040(&box);
        }
        /* fall through */
    case 1:
        func_80026B5C(0x80);
        owned = shop_owned();
        equipped = shop_equipped();
        switch (shop_state) {
        case 1:
            func_80026F44(0xC, 0xB, shop_lines[shop_line_set][1], 7);
            break;
        case 4:
            func_80026F44(0xC, 0xB, shop_lines[shop_line_set][2], 7);
            break;
        }
        func_80028E00(shop_windows[6].x + 0x23, shop_windows[6].y + 0x16, save_gil, 10, 7);
        func_80028E00(shop_windows[6].x + 0x23, shop_windows[6].y + 0x38, owned, 10, 7);
        func_80028E00(shop_windows[6].x + 0x23, shop_windows[6].y + 0x50, equipped, 10, 7);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x3F, &tw);
        func_80026F44(shop_windows[6].x + 0x12, shop_windows[6].y + 0xA, shop_menu_text[1], 5);
        func_80026F44(shop_windows[6].x + 0x12, shop_windows[6].y + 0x2C, shop_menu_text[2], 5);
        func_80026F44(shop_windows[6].x + 0x12, shop_windows[6].y + 0x44, shop_menu_text[3], 5);
        id = shops[shop_id].stock[shop_cursors[1].row + shop_cursors[1].top].id;
        switch (shops[shop_id].stock[shop_cursors[1].row + shop_cursors[1].top].kind) {
        case 0:
            func_80026F44(0x10, shop_windows[1].y + 6, kind_text(4, id, 0), 7);
            break;
        case 1:
            func_80026F44(0x10, shop_windows[1].y + 6, kind_text(0xD, id & 0xFF, 0), 7);
            break;
        case 2:
            break;
        }
        setRECT(&tw, 0, 5, 0x16C, 0xDB);
        func_80026A94(&shop_draw[shop_buf], &tw);
        rows = shop_cursors[1].scrolling ? 6 : 5;
        for (i = 0; i < rows; i++) {
            n = shop_cursors[1].top + i;
            if (n >= shops[shop_id].count)
                break;
            id = shops[shop_id].stock[n].id;
            x = shop_cursors[1].scroll_step * 4 + shop_windows[2].y + i * 16 + 8;
            switch (shops[shop_id].stock[n].kind) {
            case 0:
                func_80026F44(0x2C, x, kind_text(4, id, 8), 7);
                shop_icon(0x1A, x - 3, id, 0);
                func_80028E00(0x9C, x, shop_prices->items[id & 0x1FF], 10, 7);
                break;
            case 1:
                func_80026F44(0x28, x, kind_text(0xD, id & 0xFF, 8), 7);
                func_80028CA0(0x1A, x, 0x80, 0x20, 16, 16, (short)func_8002603C(id & 0xFF), 0);
                func_80028E00(0x9C, x, shop_prices->materia[id & 0xFF], 10, 7);
                break;
            case 2:
                break;
            }
            setRECT(&tw, 0, 0, 0x100, 0x100);
            func_80026A34(0, 1, 0x3F, &tw);
        }
        if (shops[shop_id].count >= 6) {
            shop_bar.shown = 5;
            shop_bar.total = shops[shop_id].count;
            shop_bar.top = shop_cursors[1].top;
            shop_bar.x = shop_windows[2].x + shop_windows[2].w - 0xD;
            shop_bar.y = shop_windows[2].y + 3;
            shop_bar.w = 10;
            shop_bar.h = shop_windows[2].h - 6;
            func_80028484(&shop_bar);
        }
        setRECT(&tw, shop_windows[2].x, shop_windows[2].y + 4, shop_windows[2].w, shop_windows[2].h - 7);
        func_80026A94(&shop_draw[shop_buf], &tw);
        if (shop_state != 4 || frame & 2)
            func_8001EB2C(0, shop_cursors[1].row * 16 + 0xB + shop_windows[2].y);
        shop_draw_party(frame);
        func_8001E040(&shop_windows[1]);
        func_8001E040(&shop_windows[3]);
        func_8001E040(&shop_windows[6]);
        func_8001E040(&shop_windows[2]);
        break;
    case 5:
        if (!shop_cursors[2].col)
            x = shop_slide * 36 + 0xB6;
        else
            x = -(shop_slide * 36);
        owned = inventory.items[shop_cursors[2].top * 2 + (shop_cursors[2].row * 2 + shop_cursors[2].col)] >> 9;
        equipped = shop_equipped_item(
            inventory.items[shop_cursors[2].top * 2 + (shop_cursors[2].row * 2 + shop_cursors[2].col)] & 0x1FF);
        func_80026F44(0xC, 0xB, shop_lines[shop_line_set][4], 7);
        id = inventory.items[shop_cursors[2].top * 2 + (shop_cursors[2].row * 2 + shop_cursors[2].col)] & 0x1FF;
        func_80026F44(x + 0x12, 0x3A, shop_menu_text[5], 5);
        func_80026F44(x + 0x12, 0x46, shop_menu_text[4], 5);
        func_80026F44(x + 0x12, 0x52, shop_menu_text[7], 5);
        func_80026F44(x + 0x42, 0x85, shop_menu_text[1], 5);
        func_80026F44(x + 0x42, 0xA7, shop_menu_text[2], 5);
        func_80026F44(x + 0x42, 0xBF, shop_menu_text[3], 5);
        func_80028E00(x + 0x53, 0x3C, shop_quantity, 10, 7);
        func_80028E00(x + 0x53, 0x48, (shop_prices->items[id] >> 1) * shop_quantity, 10, 7);
        func_80028E00(x + 0x53, 0x60, save_gil + (shop_prices->items[id] >> 1) * shop_quantity, 10, 7);
        func_80028E00(x + 0x53, 0x93, save_gil, 10, 7);
        func_80028E00(x + 0x53, 0xB5, owned, 10, 7);
        func_80028E00(x + 0x53, 0xCD, equipped, 10, 7);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x3F, &tw);
        func_8001DE0C(&box, x, 0x32, 0xAA, 0xAB);
        func_8001E040(&box);
        if (shop_slide)
            shop_slide--;
        /* fall through */
    case 2:
        if (shop_state == 5) {
            if (frame & 2)
                func_8001EB2C(shop_cursors[2].col * 166 + 3, shop_cursors[2].row * 16 + 0x3C);
        } else {
            func_80026F44(0xC, 0xB, shop_lines[shop_line_set][3], 7);
            func_8001EB2C(shop_cursors[2].col * 166 + 3, shop_cursors[2].row * 16 + 0x3C);
        }
        n = (shop_cursors[2].row + shop_cursors[2].top) * 2 + shop_cursors[2].col;
        if (inventory.items[n] != 0xFFFF)
            func_80026F44(0x10, 0x23, kind_text(4, inventory.items[n] & 0x1FF, 0), 7);
        setRECT(&tw, 0, 5, 0x16C, 0xDB);
        func_80026A94(&shop_draw[shop_buf], &tw);
        shop_bar.shown = 10;
        shop_bar.total = 0xA0;
        shop_bar.top = shop_cursors[2].top;
        shop_bar.x = 0x160;
        shop_bar.y = 0x35;
        shop_bar.w = 10;
        shop_bar.h = 0xA5;
        func_80028484(&shop_bar);
        func_80026B5C(9);
        rows = !shop_cursors[2].scrolling ? 10 : 11;
        for (i = 0; i < rows; i++) {
            for (j = 0; j < 2; j++) {
                n = (shop_cursors[2].top + i) * 2 + j;
                if (inventory.items[n] != 0xFFFF) {
                    id = inventory.items[n] & 0x1FF;
                    color = func_8001FAF8(id) & 1 ? 0 : 7;
                    func_80026F44(0x2C + j * 166, i * 16 + 0x3A + shop_cursors[2].scroll_step * 4, kind_text(4, id, 8),
                                  color);
                }
            }
        }
        for (i = 0; i < rows; i++) {
            for (j = 0; j < 2; j++) {
                n = (shop_cursors[2].top + i) * 2 + j;
                if (inventory.items[n] != 0xFFFF) {
                    count = inventory.items[n] >> 9;
                    id = inventory.items[n] & 0x1FF;
                    color = func_8001FAF8(id) & 1 ? 0 : 7;
                    shop_icon(0x1A + j * 166, i * 16 + 0x38 + shop_cursors[2].scroll_step * 4, id, 0);
                    func_8002708C(0x91 + j * 166, i * 16 + 0x3C + shop_cursors[2].scroll_step * 4, 0xD5, color);
                    func_80028E00(0x92 + j * 166, i * 16 + 0x3B + shop_cursors[2].scroll_step * 4, count, 3, color);
                }
            }
        }
        setRECT(&tw, 0, 0x35, 0x16C, 0xA5);
        func_80026A94(&shop_draw[shop_buf], &tw);
        func_8001E040(&shop_windows[1]);
        func_8001E040(&shop_windows[8]);
        break;
    }
    setRECT(&tw, 0, 0, 0x100, 0x100);
    func_80026A34(0, 1, 0x3F, &tw);
    func_8001E040(&shop_windows[0]);
    if (shop_phase != 1)
        return;
    func_800264A8(&shop_cursors[shop_state]);
    switch (shop_state) {
    case 0:
        if (D_80062D7C & 0x20) {
            switch (shop_cursors[0].col) {
            case 0:
                shop_sound(1);
                n = 5;
                if (shops[shop_id].count < 6)
                    n = shops[shop_id].count;
                func_80026448(&shop_cursors[1], 0, 0, 1, n, 0, 0, 1, shops[shop_id].count, 0, 0, 0, 0, 0);
                shop_state = 1;
                break;
            case 1:
                func_80026448(&shop_cursors[6], 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 1, 0, 0);
                shop_state = 6;
                break;
            case 2:
                shop_sound(1);
                shop_phase = -1;
                break;
            }
        } else if (D_80062D7C & 0x40) {
            shop_phase = 2;
            shop_sound(4);
        }
        break;
    case 1:
        if (D_80062D7C & 0x20) {
            id = shops[shop_id].stock[shop_cursors[1].row + shop_cursors[1].top].id;
            switch (shops[shop_id].stock[shop_cursors[1].row + shop_cursors[1].top].kind) {
            case 0:
                if (save_gil < shop_prices->items[id] || shop_room() <= 0) {
                    shop_sound(3);
                    break;
                }
                shop_sound(1);
                shop_state = 4;
                shop_quantity = 1;
                break;
            case 1:
                id &= 0xFF;
                if (save_gil < shop_prices->materia[id] || !shop_has_space()) {
                    shop_sound(3);
                    break;
                }
                func_8002542C(id);
                save_gil -= shop_prices->materia[id];
                shop_sound(0x108);
                break;
            case 2:
                break;
            }
        } else if (D_80062D7C & 0x40) {
            shop_state = 0;
            shop_sound(4);
        }
        break;
    case 2:
        if (D_80062D7C & 0x20) {
            id = inventory.items[shop_cursors[2].top * 2 + (shop_cursors[2].row * 2 + shop_cursors[2].col)];
            if (id == 0xFFFF || func_8001FAF8(id & 0x1FF) & 1) {
                shop_sound(3);
                break;
            }
            shop_sound(1);
            shop_slide = 10;
            shop_quantity = 1;
            shop_state = 5;
        } else if (D_80062D7C & 0x40) {
            shop_state = 6;
            shop_sound(4);
        }
        break;
    case 3:
        if (shop_cursors[3].scrolling)
            break;
        if (D_80062D7C & 0x20) {
            if (save_data.materia[shop_cursors[3].row + shop_cursors[3].top] == -1) {
                shop_sound(3);
                break;
            }
            shop_sound(0x108);
            save_gil += shop_materia_value(save_data.materia[shop_cursors[3].row + shop_cursors[3].top]);
            save_data.materia[shop_cursors[3].row + shop_cursors[3].top] = -1;
        } else if (D_80062D7C & 0x40) {
            shop_state = 6;
            shop_sound(4);
        }
        break;
    case 4:
        id = shops[shop_id].stock[shop_cursors[1].row + shop_cursors[1].top].id;
        if (D_80062D7C & 0x20) {
            func_80025B10(shop_prices->items[id] * shop_quantity);
            shop_sound(0x108);
            func_80025380(id | shop_quantity << 9);
            shop_state = 1;
        } else if (D_80062D7E & 0x2000 && shop_quantity < shop_room()) {
            if (save_gil >= shop_prices->items[id] * (shop_quantity + 1))
                shop_quantity++;
        } else if (D_80062D7E & 0x1000 && shop_quantity + 10 < shop_room()) {
            if (save_gil >= shop_prices->items[id] * (shop_quantity + 10))
                shop_quantity += 10;
        } else if (D_80062D7E & 0x8000) {
            if (--shop_quantity <= 0)
                shop_quantity = 1;
        } else if (D_80062D7E & 0x4000) {
            if ((shop_quantity -= 10) <= 0)
                shop_quantity = 1;
        } else if (D_80062D7C & 0x40) {
            shop_state = 1;
            shop_sound(4);
        }
        break;
    case 5:
        if (shop_slide)
            break;
        count = inventory.items[shop_cursors[2].top * 2 + (shop_cursors[2].row * 2 + shop_cursors[2].col)] >> 9;
        id = inventory.items[shop_cursors[2].top * 2 + (shop_cursors[2].row * 2 + shop_cursors[2].col)] & 0x1FF;
        if (D_80062D7C & 0x20) {
            func_80025288(id | shop_quantity << 9);
            func_80025B48((shop_prices->items[id] >> 1) * shop_quantity);
            shop_sound(0x108);
            shop_state = 2;
        } else if (D_80062D7C & 0x40) {
            shop_state = 2;
            shop_sound(4);
        } else if (D_80062D7E & 0x2000 && shop_quantity < count) {
            shop_quantity++;
        } else if (D_80062D7E & 0x1000 && shop_quantity + 10 < count) {
            shop_quantity += 10;
        } else if (D_80062D7E & 0x8000) {
            if (--shop_quantity <= 0)
                shop_quantity = 1;
        } else if (D_80062D7E & 0x4000) {
            if ((shop_quantity -= 10) <= 0)
                shop_quantity = 1;
        }
        break;
    case 6:
        if (D_80062D7C & 0x20) {
            shop_sound(1);
            if (shop_cursors[6].col) {
                func_80026448(&shop_cursors[3], 0, 0, 1, 10, 0, 0, 1, 0xC8, 0, 0, 0, 0, 0);
                shop_state = 3;
            } else
                shop_state = 2;
        } else if (D_80062D7C & 0x40) {
            shop_state = 0;
            shop_sound(4);
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/menu/shopmenu/nonmatchings/shopmenu", shop_frame);
#endif

/* The screen's VRAM back as it was. */
void shop_finish(void)
{
    func_80025C94(shop_vram3);
    func_80025BD0(shop_vram2);
    func_80025C54(shop_vram);
    VSync(0);
    func_800444AC(&shop_disp[0]);
    PutDrawEnv(&shop_draw[1]);
}

/* The field's call: runs shop n until it closes. */
void shop_run(int n)
{
    int frame;

    func_80021044(shop_draw, shop_disp);
    shop_open(n);
    shop_phase = 0;
    frame = 0;
    shop_init();
    shop_buf = 0;
    for (;;) {
        func_8001CB48();
        func_800269C0(D_80077F64[shop_buf]);
        shop_ot = &shop_ots[shop_buf];
        func_8004418C(shop_ot, 1);
        func_80026A00(shop_ot);
        shop_frame(frame);
        if (shop_phase == -1)
            break;
        DrawSync(0);
        VSync(0);
        func_800444AC(&shop_disp[shop_buf]);
        PutDrawEnv(&shop_draw[shop_buf]);
        func_8004433C(shop_ot);
        frame++;
        shop_buf ^= 1;
    }
    shop_finish();
    VSync(0);
    func_800444AC(&shop_disp[1]);
    PutDrawEnv(&shop_draw[0]);
}
