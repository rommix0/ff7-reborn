/* The party, 0x8001FA28-0x800211B8: a few sound requests, the equipment records' lookups, a member's stats worked
 * out from the character and equipment, the menus' HP/MP blocks and their screen setup. GCC 2.7.2, ASPSX 2.21, -G8. */

#include "common.h"
#include "main.h"
#include "sound.h"

extern unsigned char D_8009CBDC[]; /* the party: three character ids, 0xFF for an empty place */

/* A bar as the menus draw it: value against max, w pixels at most. */
typedef struct Bar {
    short x, y, w, h;
    short value, max;
    short padC;
    short trail; /* a second length drawn behind value */
    unsigned char r, g, b;
} Bar;

extern PartyMember members[3];
extern unsigned char D_80049350[], D_80049344[]; /* "Sadness", "Fury" */
extern unsigned char D_800493A0[], D_80049390[], D_80049398[]; /* "LV", "HP", "MP" */

AccessoryData *func_80025774(int id); /* accessory_table[id] */
void func_80026A34(int dfe, int dtd, int tpage, RECT *tw);
void func_80026F44(int x, int y, unsigned char *text, int color);
void func_80027354(int x, int y, unsigned char *text, int color);
void func_80027408(Bar *bar);
void func_80028CA0(short x, short y, int u, int v, int w, int h, int pal, int semi);
void func_80028E00(int x, int y, int n, int digits, int color);
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* probably SetDefDrawEnv */
void func_800444AC(DISPENV *env);                             /* probably PutDispEnv */
int func_80043CC0(void);
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
void PutDrawEnv(DRAWENV *env);
void SetDispMask(int mask);
int VSync(int mode);

void func_8001FCDC(int slot, int stat, int add);
void func_8001FE6C(int slot, int mode, int elements);
void func_8001FF50(int slot, int elements);
void func_8001FF8C(int slot, int status);
void func_8001FFD4(int slot, int status);
void func_8002001C(int slot, int statuses);

/* Sound command 0x30 (a lasting effect), as cd_error_sound sends it. */
void func_8001FA28(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

/* Sound commands 0x28 and 0x29, centred. */
void func_8001FA68(unsigned short effect)
{
    snd_request.id = 0x28;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

void func_8001FAAC(unsigned short effect)
{
    snd_request.id = 0x29;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

void func_8001FAF0(void)
{
}

/* Where an item can be used, by item number: items, then weapons from 0x80, armor from 0x100, accessories from
 * 0x120. */
int func_8001FAF8(int item)
{
    if (item < 0x80)
        return item_table[item].usable;
    if (item < 0x100)
        return weapon_table[item - 0x80].usable;
    if (item < 0x120)
        return armor_table[item - 0x100].usable;
    return accessory_table[item - 0x120].usable;
}

/* The party member's weapon power (k 0) or armor defense (1); 2 and 3 are 0. It looks character_record up twice,
 * which only works while that maps the party's ids to themselves. */
int func_8001FBAC(int slot, int k)
{
    int id = character_record[D_8009CBDC[slot]];
    int v;

    switch (k) {
    case 0:
        v = weapon_table[D_8009C738[character_record[id]].weapon].power;
        break;
    case 1:
        v = armor_table[D_8009C738[character_record[id]].armor].defense;
        break;
    case 2:
        v = 0;
        break;
    case 3:
        v = 0;
        break;
    }
    return v;
}

void func_8001FCDC(int slot, int stat, int add)
{
    switch (stat) {
    case 0:
        members[slot].primary[0] += add;
        break;
    case 1:
        members[slot].primary[1] += add;
        break;
    case 2:
        members[slot].primary[2] += add;
        break;
    case 3:
        members[slot].primary[3] += add;
        break;
    case 4:
        members[slot].primary[4] += add;
        break;
    case 5:
        members[slot].primary[5] += add;
        break;
    }
}

/* An armor or accessory's elements, by its element_mode. */
void func_8001FE6C(int slot, int mode, int elements)
{
    switch (mode) {
    case 0:
        members[slot].elements[3] |= elements;
        break;
    case 1:
        members[slot].elements[2] |= elements;
        break;
    case 2:
        members[slot].elements[1] |= elements;
        break;
    }
}

void func_8001FF50(int slot, int elements)
{
    members[slot].elements[0] |= elements;
}

void func_8001FF8C(int slot, int status)
{
    if (status <= 64)
        members[slot].statuses[0] |= 1 << status;
}

void func_8001FFD4(int slot, int status)
{
    if (status <= 64)
        members[slot].statuses[1] |= 1 << status;
}

void func_8002001C(int slot, int statuses)
{
    members[slot].statuses[1] |= statuses;
}

/* The party member's HP, MP, stats, elements and statuses from the character and its equipment. */
void func_80020058(int slot)
{
    ArmorData *armor;
    AccessoryData *acc;
    int rec, i, str, dex, vit, spr, mag, luck;

    if (D_8009CBDC[slot] == 0xFF)
        return;
    rec = character_record[D_8009CBDC[slot]];
    members[slot].elements[0] = 0;
    members[slot].elements[1] = 0;
    members[slot].elements[2] = 0;
    members[slot].elements[3] = 0;
    members[slot].statuses[0] = 0;
    members[slot].statuses[1] = 0;
    members[slot].weapon = weapon_table[D_8009C738[rec].weapon];
    members[slot].hp = D_8009C738[rec].hp;
    members[slot].mp = D_8009C738[rec].mp;
    members[slot].max_hp = D_8009C738[rec].base_hp;
    members[slot].max_mp = D_8009C738[rec].base_mp;
    members[slot].primary[0] = 0;
    members[slot].primary[4] = 0;
    members[slot].primary[1] = 0;
    members[slot].primary[3] = 0;
    members[slot].primary[2] = 0;
    members[slot].primary[5] = 0;
    for (i = 0; i < 4; i++)
        func_8001FCDC(slot, members[slot].weapon.stat[i], members[slot].weapon.stat_add[i]);
    func_8001FF50(slot, members[slot].weapon.elements);
    func_8001FF8C(slot, members[slot].weapon.status);
    armor = armor_data(D_8009C738[rec].armor);
    for (i = 0; i < 4; i++)
        func_8001FCDC(slot, armor->stat[i], armor->stat_add[i]);
    func_8001FE6C(slot, armor->element_mode, armor->elements);
    func_8001FFD4(slot, armor->status);
    if (D_8009C738[rec].accessory != 0xFF) {
        acc = func_80025774(D_8009C738[rec].accessory);
        for (i = 0; i < 2; i++)
            func_8001FCDC(slot, acc->stat[i], acc->stat_add[i]);
        func_8001FE6C(slot, acc->element_mode, acc->elements);
        func_8002001C(slot, acc->statuses);
    }
    str = D_8009C738[rec].stats[0] + D_8009C738[rec].stat_bonus[0] + members[slot].primary[0];
    dex = D_8009C738[rec].stats[4] + D_8009C738[rec].stat_bonus[4] + members[slot].primary[4];
    vit = D_8009C738[rec].stats[1] + D_8009C738[rec].stat_bonus[1] + members[slot].primary[1];
    spr = D_8009C738[rec].stats[3] + D_8009C738[rec].stat_bonus[3] + members[slot].primary[3];
    mag = D_8009C738[rec].stats[2] + D_8009C738[rec].stat_bonus[2] + members[slot].primary[2];
    luck = D_8009C738[rec].stats[5] + D_8009C738[rec].stat_bonus[5] + members[slot].primary[5];
    /* The Curse Ring, by its item number: its record has room for two of these. */
    if (D_8009C738[rec].accessory + 0x120 == 0x139) {
        str += 15;
        dex += 15;
        vit += 15;
        spr += 15;
        mag += 15;
        luck += 10;
    }
    if (str > 255)
        str = 255;
    if (dex > 255)
        dex = 255;
    if (vit > 255)
        vit = 255;
    if (spr > 255)
        spr = 255;
    if (mag > 255)
        mag = 255;
    if (luck > 255)
        luck = 255;
    members[slot].primary[0] = str;
    members[slot].primary[4] = dex;
    members[slot].primary[1] = vit;
    members[slot].primary[3] = spr;
    members[slot].primary[2] = mag;
    members[slot].primary[5] = luck;
}

/* A character's name, level, HP and MP block at (x, y), from the save: grey when dead, yellow when low. */
void func_800206E4(int x, int y, int rec)
{
    RECT tw;
    Bar bar;
    int dead, hp_color, mp_color;

    bar.x = x + 18;
    bar.y = y + 31;
    bar.w = 60;
    bar.h = 1;
    bar.value = D_8009C738[rec].hp;
    bar.trail = D_8009C738[rec].hp;
    bar.max = D_8009C738[rec].max_hp;
    bar.padC = 0;
    bar.r = 0;
    bar.g = 128;
    bar.b = 255;
    func_80027408(&bar);
    bar.x = x + 18;
    bar.y = y + 41;
    bar.w = 60;
    bar.h = 1;
    bar.value = D_8009C738[rec].mp;
    bar.trail = D_8009C738[rec].mp;
    bar.max = D_8009C738[rec].max_mp;
    bar.padC = 0;
    bar.r = 0;
    bar.g = 255;
    bar.b = 128;
    func_80027408(&bar);
    dead = !D_8009C738[rec].hp;
    hp_color = 6;
    if (D_8009C738[rec].max_hp / 4 < D_8009C738[rec].hp)
        hp_color = 7;
    mp_color = 6;
    if (D_8009C738[rec].max_mp / 4 < D_8009C738[rec].mp)
        mp_color = 7;
    if (D_8009C738[rec].flags & 0x10)
        func_80026F44(x + 36, y + 11, D_80049350, 3);
    if (D_8009C738[rec].flags & 0x20)
        func_80026F44(x + 36, y + 11, D_80049344, 3);
    func_80026F44(x, y, D_8009C738[rec].name, dead ? 2 : 7);
    func_80027354(x, y + 13, D_800493A0, 5);
    func_80028E00(x + 18, y + 13, D_8009C738[rec].level, 2, dead ? 2 : 7);
    func_80027354(x, y + 24, D_80049390, 5);
    func_80028E00(x + 18, y + 23, D_8009C738[rec].hp, 4, dead ? 2 : hp_color);
    func_80028E00(x + 50, y + 23, D_8009C738[rec].max_hp, 4, dead ? 2 : 7);
    func_80027354(x, y + 34, D_80049398, 5);
    func_80028E00(x + 18, y + 33, D_8009C738[rec].mp, 4, dead ? 2 : mp_color);
    func_80028E00(x + 50, y + 33, D_8009C738[rec].max_mp, 4, dead ? 2 : 7);
    func_80028CA0(x + 46, y + 33, 216, 0, 4, 8, 1, 0);
    func_80028CA0(x + 46, y + 23, 216, 0, 4, 8, 1, 0);
    tw.x = 0;
    tw.y = 0;
    tw.w = 256;
    tw.h = 256;
    func_80026A34(0, 1, 0x7F, &tw);
}

/* The same for a party member, its HP and MP as the battle has them. */
void func_80020B68(int x, int y, int slot)
{
    RECT tw;
    Bar bar;
    int rec, dead, hp_color, mp_color;

    rec = character_record[D_8009CBDC[slot]];
    bar.x = x + 18;
    bar.y = y + 31;
    bar.w = 60;
    bar.h = 1;
    bar.value = members[slot].hp;
    bar.trail = members[slot].hp;
    bar.max = members[slot].max_hp;
    bar.padC = 0;
    bar.r = 0;
    bar.g = 128;
    bar.b = 255;
    func_80027408(&bar);
    bar.x = x + 18;
    bar.y = y + 41;
    bar.w = 60;
    bar.h = 1;
    bar.value = members[slot].mp;
    bar.trail = members[slot].mp;
    bar.max = members[slot].max_mp;
    bar.padC = 0;
    bar.r = 0;
    bar.g = 255;
    bar.b = 128;
    func_80027408(&bar);
    dead = !members[slot].hp;
    hp_color = members[slot].max_hp / 4 < members[slot].hp ? 7 : 6;
    mp_color = members[slot].max_mp / 4 < members[slot].mp ? 7 : 6;
    if (D_8009C738[rec].flags & 0x10)
        func_80026F44(x + 36, y + 11, D_80049350, 3);
    if (D_8009C738[rec].flags & 0x20)
        func_80026F44(x + 36, y + 11, D_80049344, 3);
    func_80026F44(x, y, D_8009C738[rec].name, dead ? 2 : 7);
    func_80027354(x, y + 13, D_800493A0, 5);
    func_80028E00(x + 18, y + 13, D_8009C738[rec].level, 2, dead ? 2 : 7);
    func_80027354(x, y + 24, D_80049390, 5);
    func_80028E00(x + 18, y + 23, members[slot].hp, 4, dead ? 2 : hp_color);
    func_80028E00(x + 50, y + 23, members[slot].max_hp, 4, dead ? 2 : 7);
    func_80027354(x, y + 34, D_80049398, 5);
    func_80028E00(x + 18, y + 33, members[slot].mp, 4, dead ? 2 : mp_color);
    func_80028E00(x + 50, y + 33, members[slot].max_mp, 4, dead ? 2 : 7);
    func_80028CA0(x + 46, y + 33, 216, 0, 4, 8, 1, 0);
    func_80028CA0(x + 46, y + 23, 216, 0, 4, 8, 1, 0);
    tw.x = 0;
    tw.y = 0;
    tw.w = 256;
    tw.h = 256;
    func_80026A34(0, 1, 0x7F, &tw);
}

/* The menus' double buffer, 364 pixels wide: cleared once over both halves first. */
void func_80021044(DRAWENV *draw, DISPENV *disp)
{
    VSync(0);
    func_80043814(&draw[0], 0, 0, 384, 472);
    draw[0].dfe = 1;
    draw[0].isbg = 1;
    PutDrawEnv(&draw[0]);
    VSync(0);
    func_80043814(&draw[0], 0, 8, 384, 224);
    func_80043814(&draw[1], 0, 240, 384, 224);
    SetDefDispEnv(&disp[0], 0, 232, 364, 240);
    SetDefDispEnv(&disp[1], 0, 0, 364, 240);
    draw[0].isbg = draw[1].isbg = 1;
    draw[0].dfe = draw[1].dfe = 1;
    draw[0].dtd = draw[1].dtd = 1;
    draw[0].r0 = 0;
    draw[0].g0 = 0;
    draw[0].b0 = 0;
    draw[1].r0 = 0;
    draw[1].g0 = 0;
    draw[1].b0 = 0;
    draw[0].tpage = draw[1].tpage = func_80043CC0() == 1 || func_80043CC0() == 2 ? 0xAF : 0x3F;
    VSync(0);
    func_800444AC(&disp[0]);
    PutDrawEnv(&draw[0]);
    SetDispMask(1);
}
