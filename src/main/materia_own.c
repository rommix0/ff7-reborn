/* Each materia's own effect, 0x800197B8-0x8001AC9C: what a materia does by itself on a party member (stats, commands,
 * spells, the independent materia's bonuses), or with materia_detail set, what the materia menu's detail page shows of
 * it. GCC 2.6.3, ASPSX 2.21 and -G8 like materia.c. */

#include "common.h"
#include "main.h"

PartyMember *cur_member;
unsigned char materia_detail;
MateriaInfo *materia_info;
unsigned char materia_info_kind;
unsigned char preemptive_mastered;
int support_count;
unsigned char magic_times;  /* 2 with W-Magic */
unsigned char summon_times; /* 2 with W-Summon */
short indep_bonus[3]; /* the independent materia's bonuses by attrs[0]; 3 reaches preemptive */
short preemptive;     /* the chance of a pre-emptive strike */
short enemy_away;

extern short stat_changes[][8]; /* by MateriaData.stats: Str, Vit, Mag, Spr, Dex, Luck, max HP and MP percent */
extern short member_bonus[]; /* by attrs[0]; [8] and [9] max HP and MP percent, [12]-[17] the stats */
/* The same table from [8] and from [12]: the game reaches these parts as symbols of their own. */
extern short bonus_hpmp[];
extern short bonus_stats[];
extern unsigned char summon_uses[]; /* 16 */

int command_find(unsigned char id);
int func_8001B834(unsigned char id);
void func_8001BD50(unsigned char a, unsigned char b, unsigned char c);
void func_8001BCE8(unsigned char id);
void func_8001B4A0(unsigned char id, int ap);
void func_8001B5E4(unsigned char id, int ap);
void func_8001B704(unsigned char id, int ap);

void materia_stat_changes(unsigned char id);
void func_8001A5B4(unsigned char sub, unsigned char id, int ap);
void func_8001A980(unsigned char sub, unsigned char id, int ap);
void func_80019D1C(unsigned char sub, unsigned char id, int ap);
void func_80019F90(unsigned char sub, unsigned char id, int ap);
void func_8001A384(unsigned char id, int ap);
void func_8001A174(unsigned char sub, unsigned char id, int ap);
void func_8001A3B8(unsigned char sub, unsigned char id, int ap);
void func_8001A440(void);
void func_8001A4A8(void);
void func_8001A518(void);
void func_80019D74(unsigned char level, unsigned char id);
void func_80019DA0(unsigned char level, unsigned char id);
void func_80019E4C(unsigned char level);
void func_80019E84(unsigned char level, unsigned char id);
void func_8001A1C8(unsigned char id, int ap);
void func_8001A280(unsigned char id, int ap);
void func_8001A684(unsigned char id, int ap);
void func_8001A780(unsigned char id, int ap);
void func_8001A874(unsigned char id, int ap);
void func_8001A9CC(unsigned char id, int ap);
void func_8001AB1C(unsigned char id, int ap);

/* A materia's own effect, by its kind (the type's low 4 bits). */
#ifdef NON_MATCHING
/* Same code, but the game keeps materia_info in $a3 for the loop, not $a2. */
void materia_own(unsigned int materia)
{
    unsigned char i, id, sub;
    unsigned int m;
    int ap;

    if (materia_detail) {
        for (i = 0; i < 8; i++)
            materia_info->stats[i] = 0;
    }
    ap = materia >> 8;
    m = materia;
    id = m;
    if (id == 0xFF)
        return;
    materia_stat_changes(id);
    sub = materia_data[id].type >> 4;
    switch (materia_data[id].type & 0xF) {
    case 0:
        func_8001A5B4(sub, m, ap);
        break;
    case 1:
        func_8001A980(sub, m, ap);
        break;
    case 2:
        func_80019D1C(sub, m, ap);
        break;
    case 3:
        func_80019F90(sub, m, ap);
        break;
    case 4:
        func_8001A384(m, ap);
        break;
    case 5:
        func_8001A174(sub, m, ap);
        break;
    case 6:
        func_8001B704(m, ap);
        break;
    case 7:
        func_8001A3B8(sub, m, ap);
        break;
    case 8:
        func_8001A440();
        break;
    case 9:
        func_8001B5E4(m, ap);
        break;
    case 0xA:
        func_8001A4A8();
        break;
    case 0xB:
        func_8001B4A0(m, ap);
        break;
    case 0xC:
        func_8001A518();
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/main/materia_own", materia_own);
#endif

/* The stat changes the materia makes: added to the member's, or shown on the detail page. */
void materia_stat_changes(unsigned char id)
{
    if (!materia_detail) {
        if (materia_data[id].stats != 0xFF) {
            bonus_stats[0] += stat_changes[materia_data[id].stats][0];
            bonus_stats[1] += stat_changes[materia_data[id].stats][1];
            bonus_stats[2] += stat_changes[materia_data[id].stats][2];
            bonus_stats[3] += stat_changes[materia_data[id].stats][3];
            bonus_stats[4] += stat_changes[materia_data[id].stats][4];
            bonus_stats[5] += stat_changes[materia_data[id].stats][5];
            bonus_hpmp[0] += stat_changes[materia_data[id].stats][6];
            bonus_hpmp[1] += stat_changes[materia_data[id].stats][7];
        }
    } else if (materia_data[id].stats != 0xFF) {
        materia_info->stats[0] = stat_changes[materia_data[id].stats][0];
        materia_info->stats[1] = stat_changes[materia_data[id].stats][1];
        materia_info->stats[2] = stat_changes[materia_data[id].stats][2];
        materia_info->stats[3] = stat_changes[materia_data[id].stats][3];
        materia_info->stats[4] = stat_changes[materia_data[id].stats][4];
        materia_info->stats[5] = stat_changes[materia_data[id].stats][5];
        materia_info->stats[6] = stat_changes[materia_data[id].stats][6];
        materia_info->stats[7] = stat_changes[materia_data[id].stats][7];
    }
}

/* Command materia. */
void func_80019D1C(unsigned char sub, unsigned char id, int ap)
{
    int level = materia_level(id, ap);

    if (sub == 1)
        func_80019E84(level, id);
}

void func_80019D74(unsigned char level, unsigned char id)
{
    if (id == 0xB)
        func_80019E4C(level);
}

/* Counter Attack. */
void func_80019DA0(unsigned char level, unsigned char id)
{
    if (!materia_detail) {
        if (support_count != 8) {
            cur_member->support[support_count].kind = 9;
            cur_member->support[support_count].value = 9;
            cur_member->support[support_count++].param = materia_data[id].attrs[level];
        }
    } else
        materia_info_kind = 0x12;
}

/* Long Range. */
void func_80019E4C(unsigned char level)
{
    if (!materia_detail)
        cur_member->flags23 |= 4;
    else
        materia_info_kind = 0x12;
}

/* A command that takes the place of Attack, the best one the level reaches. */
void func_80019E84(unsigned char level, unsigned char id)
{
    int i = 0;
    MateriaInfo *info;

    if (!materia_detail) {
        for (i = level - 1; i > 0; i--) {
            if (materia_data[id].attrs[i] != 0xFF)
                break;
        }
        command_slots[0].id = materia_data[id].attrs[i];
    } else {
        info = materia_info;
        for (i = 0; i < 5; i++) {
            info->lines[i].id = materia_data[id].attrs[i];
            info->lines[i].value = 0;
        }
        info->lines[info->levels == level ? level - 2 : level - 1].value = 1;
        materia_info_kind = 0x13;
    }
}

/* W-Magic, W-Summon, W-Item. */
void func_80019F90(unsigned char sub, unsigned char id, int ap)
{
    MateriaInfo *info;
    int level = materia_level(id, ap);
    int k;

    if (!materia_detail) {
        switch (materia_data[id].attrs[0]) {
        case 0x15:
            magic_times = 2;
            k = command_find(2);
            if (k == -1)
                command_slots[func_8001B834(2)].level = level;
            else {
                command_slots[k].id = 2;
                command_slots[k].level = level;
            }
            break;
        case 0x16:
            summon_times = 2;
            k = command_find(3);
            if (k == -1)
                command_slots[func_8001B834(3)].level = level;
            else {
                command_slots[k].id = 3;
                command_slots[k].level = level;
            }
            break;
        case 0x17:
            k = command_find(4);
            command_slots[k].id = materia_data[id].attrs[0];
            command_slots[k].level = level;
            break;
        }
    } else {
        materia_info_kind = 0xE;
        info = materia_info;
        info->lines[0].id = materia_data[id].attrs[0];
        info->lines[0].value = level;
    }
}

/* Support materia: only the detail page. */
void func_8001A174(unsigned char sub, unsigned char id, int ap)
{
    switch (sub) {
    case 2:
        func_8001A280(id, ap);
        break;
    case 3:
        func_8001A1C8(id, ap);
        break;
    }
}

void func_8001A1C8(unsigned char id, int ap)
{
    MateriaInfo *info;
    int level = materia_level(id, ap);

    if (materia_detail) {
        info = materia_info;
        info->lines[0].id = materia_data[id].attrs[0];
        info->lines[0].value = level;
        switch (materia_data[id].attrs[0]) {
        case 0x57:
            materia_info_kind = 0x10;
            break;
        case 0x51:
        case 0x63:
            materia_info_kind = 0xF;
            break;
        }
    }
}

void func_8001A280(unsigned char id, int ap)
{
    MateriaInfo *info;
    int level = materia_level(id, ap);

    if (materia_detail) {
        info = materia_info;
        info->lines[0].id = materia_data[id].attrs[0];
        info->lines[0].value = level * 10;
        switch (materia_data[id].attrs[0]) {
        case 0x64:
            materia_info_kind = 0x10;
            break;
        case 0x58:
            materia_info_kind = 0x10;
            break;
        case 0x59:
            materia_info_kind = 0x10;
            break;
        case 0x5A:
            materia_info_kind = 0x10;
            break;
        case 0x56:
            materia_info_kind = 0x10;
            break;
        case 0x5C:
            materia_info_kind = 0x10;
            break;
        case 0x5D:
            materia_info_kind = 0x10;
            break;
        case 0x5E:
            materia_info_kind = 0x10;
            break;
        case 0x5F:
            materia_info_kind = 0x10;
            break;
        case 0x54:
        case 0x55:
            materia_info_kind = 0xF;
            break;
        }
    }
}

void func_8001A384(unsigned char id, int ap)
{
    materia_level(id, ap);
    if (materia_detail)
        materia_info_kind = 0xB;
}

/* Enemy Skill: a bit of the AP for each skill learnt. */
void func_8001A3B8(unsigned char sub, unsigned char id, int ap)
{
    int i, learnt;

    if (!materia_detail) {
        learnt = ap & 0xFFFFFF;
        for (i = 0; i < 24; i++) {
            if (learnt & 1)
                func_8001BD50(i, i + 0x48, i + 0x48);
            learnt >>= 1;
        }
        func_8001B834(0xD);
    } else
        materia_info_kind = 8;
}

/* Master Command. */
void func_8001A440(void)
{
    if (!materia_detail) {
        func_8001B834(5);
        func_8001B834(6);
        func_8001B834(7);
        func_8001B834(9);
        func_8001B834(0xA);
        func_8001B834(0xB);
        func_8001B834(0xC);
    } else
        materia_info_kind = 5;
}

/* Master Magic. */
void func_8001A4A8(void)
{
    int i;

    if (!materia_detail) {
        for (i = 0; i < 56; i++)
            func_8001BCE8(i);
        func_8001B834(2);
        if (!magic_times)
            magic_times = 1;
    } else
        materia_info_kind = 6;
}

/* Master Summon: every summon, without a limit on its uses. */
void func_8001A518(void)
{
    int i;

    if (!materia_detail) {
        for (i = 0; i < 16; i++)
            summon_uses[i] = 0xFF;
        for (i = 0x38; i < 0x48; i++)
            func_8001BD50(i - 0x38, i, i);
        func_8001B834(3);
        if (!summon_times)
            summon_times = 1;
    } else
        materia_info_kind = 7;
}

/* Independent materia. */
void func_8001A5B4(unsigned char sub, unsigned char id, int ap)
{
    int level = materia_level(id, ap);

    switch (sub) {
    case 0:
        func_8001A684(id, ap);
        break;
    case 2:
        func_8001A780(id, ap);
        break;
    case 4:
        func_8001A874(id, ap);
        break;
    case 3:
        func_80019D74(level, id);
        break;
    }
}

void func_8001A684(unsigned char id, int ap)
{
    materia_level(id, ap);
    if (!materia_detail) {
        if (materia_data[id].attrs[0] == 0x62)
            cur_member->flags23 |= 8;
        if (materia_data[id].attrs[0] == 0xC)
            cur_member->flags23 |= 1;
    } else {
        if (materia_data[id].attrs[0] == 0x62)
            materia_info_kind = 0x11;
        if (materia_data[id].attrs[0] == 0xC)
            materia_info_kind = 0x11;
    }
}

void func_8001A780(unsigned char id, int ap)
{
    MateriaInfo *info;
    int level = materia_level(id, ap);

    if (id == 0xD) {
        func_80019DA0(level, id);
        return;
    }
    if (!materia_detail)
        member_bonus[materia_data[id].attrs[0]] += materia_data[id].attrs[level];
    else {
        info = materia_info;
        info->lines[0].id = materia_data[id].attrs[0];
        info->lines[0].value = materia_data[id].attrs[level];
        materia_info_kind = 4;
    }
}

void func_8001A874(unsigned char id, int ap)
{
    MateriaInfo *info;
    int i, bonus;

    for (i = materia_level(id, ap); i > 0; i--) {
        if (materia_data[id].attrs[i] != 0xFF) {
            bonus = materia_data[id].attrs[i];
            break;
        }
    }
    if (!materia_detail)
        member_bonus[materia_data[id].attrs[0]] += bonus;
    else {
        info = materia_info;
        info->lines[0].id = materia_data[id].attrs[0];
        info->lines[0].value = bonus;
        materia_info_kind = 0xD;
    }
}

void func_8001A980(unsigned char sub, unsigned char id, int ap)
{
    switch (sub) {
    case 4:
        func_8001AB1C(id, ap);
        break;
    case 2:
        func_8001A9CC(id, ap);
        break;
    }
}

void func_8001A9CC(unsigned char id, int ap)
{
    MateriaInfo *info;
    int i, level, bonus;

    level = materia_level(id, ap);
    for (i = level; i > 0; i--) {
        if (materia_data[id].attrs[i] != 0xFF) {
            bonus = materia_data[id].attrs[i];
            break;
        }
    }
    if (!materia_detail) {
        indep_bonus[materia_data[id].attrs[0]] += bonus;
        if (id == 0xA) {
            if (preemptive > 0x55)
                preemptive = 0x55;
            if (level == 5)
                preemptive_mastered = 1;
        }
    } else {
        info = materia_info;
        info->lines[0].id = materia_data[id].attrs[0];
        info->lines[0].value = level;
        materia_info_kind = 0xA;
    }
}

void func_8001AB1C(unsigned char id, int ap)
{
    MateriaInfo *info;
    int i, level, bonus, away;

    level = materia_level(id, ap);
    if (id == 7) {
        for (i = level; i > 0; i--) {
            if (materia_data[id].attrs[i] != 0xFF) {
                away = materia_data[id].attrs[i];
                break;
            }
        }
    } else {
        for (i = level; i > 0; i--) {
            if (materia_data[id].attrs[i] != 0xFF) {
                bonus = materia_data[id].attrs[i];
                break;
            }
        }
    }
    if (!materia_detail) {
        if (id == 7)
            enemy_away += away;
        else
            indep_bonus[materia_data[id].attrs[0]] += bonus;
    } else {
        info = materia_info;
        info->lines[0].id = materia_data[id].attrs[0];
        info->lines[0].value = bonus;
        materia_info_kind = 9;
    }
}
