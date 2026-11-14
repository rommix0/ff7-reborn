/* A party member's stats and battle lists as the materia build them, 0x8001AC9C-0x8001C0EC: a materia's level, the
 * percentages and caps on the stats, the magic, summon and command materia, and the command and spell lists the
 * battle menu reads. GCC 2.6.3, ASPSX 2.21 and -G8 like the other materia files. */

#include "common.h"
#include "main.h"

PartyMember *cur_member;
unsigned char materia_detail;
MateriaInfo *materia_info;
unsigned char materia_info_kind;
int materia_levels;
int ap_to_next; /* the detail page's AP to the next level, 0 at the last */
int support_count;
unsigned char magic_times;
unsigned char summon_times;
short indep_bonus[4];
/* indep_bonus[1]-[3] by names of their own: the game loads them through $gp, which ASPSX 2.21 (as maspsx models it)
 * does for a common symbol only at its own address. */
short encounter_bonus;
short indep_bonus2;
short preemptive;
short enemy_away;
unsigned char preemptive_mastered;
unsigned char command_count; /* command_slots in use */
unsigned char spell_count;   /* spell_slots in use */
int commands_shown;          /* the member's commands, for the menu's rows */

extern short member_bonus[]; /* percent on the stats, then caps; [8] and [9] max HP and MP */
extern short bonus_hpmp[];   /* member_bonus + 8 */
extern short bonus_stats[];  /* added to the stats */
/* The member before materia: [0] and [1] its first bytes, [2]-[7] the primary stats, [8]-[11] from func_8001FBAC,
 * [12] and [13] max HP and MP. */
extern short member_base[];

/* KERNEL.BIN's command and spell records. */
typedef struct {
    unsigned char cursor;
    unsigned char targets;
    unsigned char pad2[6];
} CommandData;
extern CommandData command_data[];
typedef struct {
    unsigned char pad0[4];
    unsigned char mp; /* the low byte */
    unsigned char pad5[7];
    unsigned char targets;
    unsigned char pad13[15];
} SpellData;
extern SpellData spell_data[];

int func_8001FBAC(unsigned char slot, int k);
int func_8001B834(unsigned char id);
void func_8001B8A8(void);
int func_8001BA54(int id);
void func_8001BB30(unsigned char id, unsigned char place);
void func_8001BCE8(unsigned char id);
void func_8001BD50(unsigned char id, unsigned char slot, unsigned char spell);

/* The level a materia's AP reaches; on the detail page also its levels and the AP to the next. */
int materia_level(unsigned char id, int ap)
{
    MateriaInfo *info;
    int i, level = 1;

    for (i = 3; i >= 0; i--) {
        if (materia_data[id].ap[i] != 0xFFFF && ap >= materia_data[id].ap[i] * 100) {
            level = i + 2;
            break;
        }
    }
    materia_levels = 1;
    for (i = 0; i < 4; i++) {
        if (materia_data[id].ap[i] != 0xFFFF)
            materia_levels++;
    }
    if (materia_detail) {
        if (materia_data[id].ap[level - 1] == 0xFFFF || level == materia_levels)
            ap_to_next = 0;
        else
            ap_to_next = materia_data[id].ap[level - 1] * 100 - ap;
        info = materia_info;
        info->level = level;
        info->levels = materia_levels;
        info->ap_to_next = ap_to_next;
    }
    return level;
}

/* The independent materia's caps, then their bonuses on the member. */
void func_8001AE08(void)
{
    if (indep_bonus[0] > 32)
        indep_bonus[0] = 32;
    if (indep_bonus2 > 32)
        indep_bonus2 = 32;
    if (encounter_bonus > 255)
        encounter_bonus = 255;
    if (enemy_away > 255)
        enemy_away = 255;
    cur_member->effects[0] += indep_bonus[0];
    cur_member->effects[1] += encounter_bonus;
    cur_member->encounter_less += enemy_away;
    cur_member->effects[2] += indep_bonus2;
    cur_member->effects[3] += preemptive;
}

/* The materia's percentages and additions on the member's stats, capped, then written to the member. */
void func_8001AEE4(void)
{
    int i;

    for (i = 0; i < 11; i++) {
        if (member_bonus[i] > 100)
            member_bonus[i] = 100;
    }
    if (member_bonus[10] > 32)
        member_bonus[10] = 32;
    for (i = 0; i < 6; i++) {
        if (!(bonus_stats[i] & 0x8000))
            bonus_stats[i] &= 0xFF;
    }
    member_base[2] = member_base[2] + member_base[2] * member_bonus[0] / 100 + bonus_stats[0];
    member_base[3] = member_base[3] + member_base[3] * member_bonus[1] / 100 + bonus_stats[1];
    member_base[4] = member_base[4] + member_base[4] * member_bonus[2] / 100 + bonus_stats[2];
    member_base[5] = member_base[5] + member_base[5] * member_bonus[3] / 100 + bonus_stats[3];
    member_base[6] = member_base[6] + member_base[6] * member_bonus[4] / 100 + bonus_stats[4];
    member_base[7] = member_base[7] + member_base[7] * member_bonus[5] / 100 + bonus_stats[5];
    member_base[12] = member_base[12] + member_base[12] * bonus_hpmp[0] / 100;
    member_base[8] = member_base[8] + member_base[8] * member_bonus[6] / 100;
    member_base[9] = member_base[9] + member_base[9] * member_bonus[7] / 100;
    member_base[13] = member_base[13] + member_base[13] * bonus_hpmp[1] / 100;
    if (member_base[2] > 255)
        member_base[2] = 255;
    if (member_base[3] > 255)
        member_base[3] = 255;
    if (member_base[4] > 255)
        member_base[4] = 255;
    if (member_base[5] > 255)
        member_base[5] = 255;
    if (member_base[6] > 255)
        member_base[6] = 255;
    if (member_base[7] > 255)
        member_base[7] = 255;
    if (member_base[12] > 9999)
        member_base[12] = 9999;
    if (member_base[13] > 999)
        member_base[13] = 999;
    if (member_base[2] < 0)
        member_base[2] = 0;
    if (member_base[3] < 0)
        member_base[3] = 0;
    if (member_base[4] < 0)
        member_base[4] = 0;
    if (member_base[5] < 0)
        member_base[5] = 0;
    if (member_base[6] < 0)
        member_base[6] = 0;
    if (member_base[7] < 0)
        member_base[7] = 0;
    if (member_base[12] < 0)
        member_base[12] = 0;
    if (member_base[13] < 0)
        member_base[13] = 0;
    cur_member->stats[2] = member_base[10];
    cur_member->stats[3] = member_base[11];
    cur_member->primary[0] = member_base[2];
    cur_member->primary[1] = member_base[3];
    cur_member->primary[2] = member_base[4];
    cur_member->primary[3] = member_base[5];
    cur_member->primary[4] = member_base[6];
    cur_member->primary[5] = member_base[7];
    cur_member->max_hp = member_base[12];
    cur_member->max_mp = member_base[13];
    cur_member->misc[0] = member_bonus[10];
    cur_member->misc[1] = member_bonus[11];
}

/* Summon materia. */
void func_8001B4A0(unsigned char id, int ap)
{
    MateriaInfo *info;
    int level = materia_level(id, ap);
    int summon = materia_data[id].attrs[0];

    if (!materia_detail) {
        if (summon_uses[summon - 0x38] != 0xFF)
            summon_uses[summon - 0x38] += level;
        func_8001BD50(summon - 0x38, summon, summon);
        func_8001B834(3);
        if (!summon_times)
            summon_times = 1;
    } else {
        info = materia_info;
        info->lines[0].id = summon - 0x38;
        info->lines[0].value = level;
        materia_info_kind = 2;
    }
}

/* The summons' uses and Quadra Magic onto the member. */
void func_8001B570(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        cur_member->spells[0x38 + i].uses = summon_uses[i];
        cur_member->spells[0x38 + i].quadra = summon_quadra[i];
        cur_member->spells[0x38 + i].quadra_uses = summon_quadra_uses[i];
    }
}

/* Magic materia: its spells up to its level. */
void func_8001B5E4(unsigned char id, int ap)
{
    MateriaInfo *info;
    int i, level = materia_level(id, ap);

    if (!materia_detail) {
        for (i = level - 1; i >= 0; i--) {
            if (materia_data[id].attrs[i] != 0xFF)
                func_8001BCE8(materia_data[id].attrs[i]);
        }
        func_8001B834(2);
        if (!magic_times)
            magic_times = 1;
    } else {
        info = materia_info;
        for (i = 0; i < 5; i++) {
            info->lines[i].id = materia_data[id].attrs[i];
            info->lines[i].value = 0;
        }
        for (i = 0; i < level; i++)
            info->lines[i].value = 1;
        materia_info_kind = 1;
    }
}

/* Command materia: the best command the level reaches. */
void func_8001B704(unsigned char id, int ap)
{
    MateriaInfo *info;
    int i, level = materia_level(id, ap);

    if (!materia_detail) {
        for (i = level - 1; i >= 0; i--) {
            if (materia_data[id].attrs[i] != 0xFF)
                break;
        }
        func_8001B834(materia_data[id].attrs[i]);
    } else {
        info = materia_info;
        for (i = 0; i < 5; i++) {
            info->lines[i].id = materia_data[id].attrs[i];
            info->lines[i].value = 0;
        }
        info->lines[info->levels == level ? level - 2 : level - 1].value = 1;
        materia_info_kind = 3;
    }
}

/* A command's slot on the list, added if it isn't there. */
int func_8001B834(unsigned char id)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (command_slots[i].id == id)
            return i;
    }
    command_slots[command_count++].id = id;
    return command_count - 1;
}

/* Mug takes Steal's place. */
void func_8001B8A8(void)
{
    int i, mug = 0;

    for (i = 0; i < 16; i++) {
        if (command_slots[i].id == 0x11) {
            mug = 1;
            break;
        }
    }
    for (i = 0; i < 16; i++) {
        if (mug == 1 && command_slots[i].id == 5)
            command_slots[i].id = 0xFF;
    }
}

/* The command list onto the member, W-Magic and W-Summon in place of Magic and Summon. */
void func_8001B944(void)
{
    int i;

    commands_shown = 0;
    support_count = 0;
    func_8001B8A8();
    for (i = 0; i < 16; i++) {
        if (command_slots[i].id == 0xFF)
            continue;
        if (command_slots[i].id == 2 && magic_times == 2)
            command_slots[i].id = 0x15;
        if (command_slots[i].id == 3 && summon_times == 2)
            command_slots[i].id = 0x16;
        func_8001BB30(command_slots[i].id, func_8001BA54(command_slots[i].id));
        commands_shown++;
    }
    cur_member->command_rows = (commands_shown - 1) / 4 + 1;
}

/* A command's place in the battle menu: Attack's, Magic's, Summon's or Item's, or the first free one after. A
 * command with none of its own takes Magic's or Summon's place while that's free. Attack's five are tested one by one
 * (joined with ||, GCC merges the neighbouring ids into range checks). With no place free the game falls off the end
 * with 0 in $v0. */
int func_8001BA54(int id)
{
    int i;

    if (id == 0x18)
        return 0;
    if (id == 0x19)
        return 0;
    if (id == 0x1A)
        return 0;
    if (id == 0x1B)
        return 0;
    if (id == 1)
        return 0;
    if (id == 4 || id == 0x17)
        return 3;
    if (id == 2 || id == 0x15)
        return 1;
    if (id == 3 || id == 0x16)
        return 2;
    if (!magic_times) {
        magic_times = 1;
        return 1;
    }
    if (!summon_times) {
        summon_times = 1;
        return 2;
    }
    for (i = 4; i < 16; i++) {
        if (cur_member->commands[i].id == 0xFF)
            return i;
    }
#ifdef NATIVE
    return 0; /* what the game's $v0 holds here */
#endif
}

void func_8001BB30(unsigned char id, unsigned char place)
{
    int i;

    cur_member->commands[place].id = id;
    cur_member->commands[place].pad3 = 0;
    cur_member->commands[place].cursor = command_data[id].cursor;
    cur_member->commands[place].targets = command_data[id].targets;
    for (i = 0; i < 16; i++) {
        if (id == command_slots[i].id) {
            cur_member->commands[place].uses = command_slots[i].level;
            cur_member->commands[place].flags = command_slots[i].flags;
            break;
        }
    }
}

/* The spell list onto the member. */
void func_8001BC18(void)
{
    int i;

    for (i = 0; i < 56; i++) {
        func_8001BD50(spell_slots[i].id, i, spell_slots[i].id);
        cur_member->spells[i].uses = spell_slots[i].all;
        cur_member->spells[i].quadra = spell_slots[i].quadra;
        cur_member->spells[i].quadra_uses = spell_slots[i].quadra_uses;
        cur_member->spells[i].flags = spell_slots[i].flags;
    }
}

/* A spell on the list, added if it isn't there. */
void func_8001BCE8(unsigned char id)
{
    int i;

    for (i = 0; i < 56; i++) {
        if (spell_slots[i].id == id)
            return;
    }
    spell_slots[spell_count++].id = id;
}

void func_8001BD50(unsigned char id, unsigned char slot, unsigned char spell)
{
    cur_member->spells[slot].id = id;
    cur_member->spells[slot].mp = spell_data[spell].mp;
    cur_member->spells[slot].targets = spell_data[spell].targets;
}

/* Everything the materia build, cleared before the member is worked out again. The spell list's Quadra Magic fields
 * go through a pointer and the support effects through one chained assignment: that's how the game addresses them. */
void func_8001BDB0(unsigned char slot, PartyMember *m)
{
    int i;
    SpellSlot *p;

    command_count = 0;
    spell_count = 0;
    summon_times = 0;
    magic_times = 0;
    for (i = 0; i < 56; i++) {
        spell_slots[i].id = 0xFF;
        p = &spell_slots[i];
        p->quadra_uses = 0;
        p->quadra = 0;
        spell_slots[i].all = 0;
        spell_slots[i].flags = 0;
    }
    for (i = 0; i < 96; i++) {
        m->spells[i].id = 0xFF;
        m->spells[i].quadra_uses = 0;
        m->spells[i].quadra = 0;
        m->spells[i].uses = 0;
        m->spells[i].flags = 0;
    }
    for (i = 0; i < 16; i++) {
        summon_quadra_uses[i] = 0;
        summon_quadra[i] = 0;
        summon_uses[i] = 0;
    }
    for (i = 0; i < 12; i++)
        member_bonus[i] = 0;
    for (i = 0; i < 6; i++)
        bonus_stats[i] = 0;
    for (i = 0; i < 16; i++) {
        command_slots[i].id = 0xFF;
        command_slots[i].level = 0;
        command_slots[i].flags = 0;
    }
    for (i = 0; i < 16; i++) {
        m->commands[i].id = 0xFF;
        m->commands[i].pad3 = 1;
        m->commands[i].flags = 0;
    }
    for (i = 0; i < 8; i++)
        cur_member->support[i].kind = cur_member->support[i].value = cur_member->support[i].param = 0;
    member_base[2] = cur_member->primary[0];
    member_base[3] = cur_member->primary[1];
    member_base[4] = cur_member->primary[2];
    member_base[5] = cur_member->primary[3];
    member_base[6] = cur_member->primary[4];
    member_base[7] = cur_member->primary[5];
    member_base[12] = cur_member->max_hp;
    member_base[13] = cur_member->max_mp;
    member_base[0] = cur_member->misc[0];
    member_base[1] = cur_member->misc[1];
    cur_member->flags23 = 0;
    member_base[8] = func_8001FBAC(slot, 0);
    member_base[9] = func_8001FBAC(slot, 1);
    member_base[10] = func_8001FBAC(slot, 2);
    member_base[11] = func_8001FBAC(slot, 3);
    func_8001B834(1);
    func_8001B834(4);
    for (i = 0; i < 4; i++)
        indep_bonus[i] = 0;
    enemy_away = 0;
    cur_member->effects[0] = 0;
    cur_member->effects[1] = 0;
    cur_member->encounter_less = 0;
    cur_member->effects[2] = 0;
    cur_member->effects[3] = 0;
    preemptive_mastered = 0;
}
