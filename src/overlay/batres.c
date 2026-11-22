/* BATTLE/BATRES.X, 0x801B0000-0x801B193C: the battle's results. When BATTLE.X ends a battle it reads this over
 * BATINI.X. It tallies the spoils, levels the party (the same level-up code as NAMEMENU's namelevel.c) and
 * counts kills and limit uses towards new limits. GCC 2.7.2, ASPSX 2.34 with its divide checks, -G0. */

#include "common.h"
#include "main.h"
#include "levelup.h"
#include "battle.h"

/* The battle's winnings and what the results screens show of them (0x8009D7D8): the exe's battle_won, tally_shown and
 * spoils, which the game here reaches from one base. */
extern struct {
    int exp, ap, gil;
    TallyShown shown[3];
    Spoil spoils[4];
} results;
extern unsigned char D_8009CBDC[]; /* the party: three character ids, 0xFF for an empty place */

extern unsigned short D_80163B80, D_800FA6B8;
extern unsigned short D_8009D78A; /* the joined characters, bit n for character n */
extern volatile short D_80095DD4;
extern unsigned char party_effects[4];
extern struct {
    unsigned short items[320]; /* id in the low 9 bits, count above, 0xFFFF for none */
} inventory;
extern PartyMember members[3];

int count_bits(unsigned int x);
void func_800A6000(int actor, int a, int b);
void func_800A7254(int a, int actor, int b, int c);
void func_800B1060(int a);
void func_80014540(void);
void func_800A3354(void);
void func_800A31A0(int a, int b, int c, int d);
void func_800A56B0(short a);
void func_800DCF94(int a);
int func_800B0F04(BattleEnemy *enemy, int a, int b, int c);
void func_800254D8(void);
void func_800A4ACC(int member, int ap);
void func_800BB944(void);
void func_80022DE4(void);
void res_801B0C74(Character *c, int bits);
void res_share_exp(int who);
void res_clear(void);
void res_store_party(int hp, int mp);
unsigned short res_learn_limits(Character *c, int kills, int uses);

extern unsigned char D_80163790[3]; /* the party's character ids in the battle */
extern unsigned short D_8016376A;
extern Character D_80167938;        /* a character's record kept through the battle, put back after */
extern unsigned char D_8009D58A[4]; /* a count in the save, not word-aligned */

int limit_id(unsigned int character, int slot);
unsigned char rand_byte(void);
void exp_tally_start(int hidden);
void res_level_up(Character *c, unsigned int gain, int slot);
void res_unpack(Growth *w, Character *c);
void res_pack(Growth *w, Character *c);
void res_growth_init(Growth *w);
void res_level_to(Growth *w);
unsigned int res_exp_for(Growth *w, int level);
void res_stat_gain(Growth *w, int stat);
void res_hp_gain(Growth *w);
void res_mp_gain(Growth *w);

/* The module's entry, run by BATTLE.X when a battle ends: the spoils, EXP and AP into the party, the party's state
 * back into the save, and the next mode (4, the results screens, unless the battle was escaped or lost). */
void res_finish(void)
{
    Character *c;
    PartyMember *pm;
    int flags, next, who, hp, mp, wait, kinds, alive, n, busy, i, k, item, diff;
    int *saved, row;
    BattleItem *held;
    unsigned short *dst;
    int id;
    BattleEnemy *e;
    unsigned int gain;

    flags = battle.outcome;
    who = D_8009D78A;
    wait = 0;
    kinds = 0;
    next = 0;
    hp = 0;
    mp = 0;
    for (i = 0; i < 10; i++) {
        if (battle.actors[i].present != -1)
            func_800A6000(i, 6, 6);
    }
    if (battle.flags & 4) {
        row = stage_hp.stage;
        for (i = 0; i < 6; i++)
            stage_hp.hp[row][i] = battle.actors[i + 4].hp;
    }
    if (battle_data.given) {
        n = count_bits(battle_data.given);
        alive = 0;
        for (i = 0; i < 4; i++) {
            if (battle.actors[i].present != -1) {
                hp += battle.actors[i].hp;
                mp += battle.actors[i].mp;
                alive++;
            }
        }
        hp /= n;
        mp /= n;
        if (!alive)
            battle_data.given = 0;
    }
    if (D_8016376A & 0x40)
        flags |= 0x40;
    if (D_8016376A & 0x80)
        flags |= 0x80;
    if (flags & 0x40)
        flags &= ~0x20;
    if (flags & 0x20)
        flags &= ~4;
    if (flags & 2) {
        flags &= ~8;
        if (battle_data.left & 0xF) {
            flags &= ~2;
            flags |= 8;
        }
    }
    res_clear();
    for (i = 0; i < 10; i++)
        func_800A7254(0, i, 4, 0);
    for (i = 0; i < 3; i++) {
        places[i].x8 &= 0x1831;
        places[i].xC = places[i].x8;
    }
    if (!(flags & 8)) {
        if (flags & 2) {
            if (!(flags & 0x40)) {
                wait = 30;
                func_800B1060(8);
                D_80163B80 = 1;
            }
        } else if (flags & 4) {
            if (!(flags & 0x40)) {
                wait = 8;
                for (i = 0; i < 3; i++)
                    battle.actors[i].state16 = 14;
            }
        } else if (scene.setup.flags & 8 && !(D_8016376A & 0x100)) {
            for (i = 0; i < 3; i++)
                battle.actors[i].state16 = 12;
            wait = 49;
            D_800FA6B8 = 1;
        }
    }
    busy = D_800FA6B8 | D_80163B80;
    if (!busy)
        func_80014540();
    for (i = 0; i < wait; i++)
        func_800A3354();
    while (D_800FA6B8 | D_80163B80)
        func_800A3354();
    if (busy)
        func_80014540();
    if (!(battle.flags & 8))
        func_800A31A0(0, 0, 0, 0);
    if (flags & 2)
        *(short *)&D_800707BE |= 1;
    else if (flags & 0xC)
        *(short *)&D_800707BE |= 8;
    for (i = 0; i < 6; i++) {
        if (battle.actors[i + 4].present != -1 && !(battle_data.left >> (i + 4) & 1))
            break;
    }
    if (i == 6)
        *(short *)&D_800707BE |= 8;
    for (i = 0; i < 16; i++) {
        func_800A3354();
        func_800A56B0(10);
        for (k = 0; k < 3; k++)
            func_800A56B0(k);
    }
    func_800DCF94(-1);
    held = battle_items;
    for (i = 0; i < 320; i++) {
        inventory.items[i] = 0xFFFF;
        if (held[i].id != 0xFFFF)
            inventory.items[i] = (held[i].id & 0x1FF) | (held[i].count << 9 & 0xFE00);
    }
    if (!(flags & 0xDE) && !(battle.flags & 0x20)) {
        next = 4;
        for (i = 0; i < 6; i++) {
            id = scene.formation[i].id;
            if (id == -1)
                continue;
            e = &battle_data.enemies[id];
            if (!(battle.actors[i + 4].status & 0x4001) || battle_data.left >> (i + 4) & 1)
                continue;
            results.exp += battle.actors[i + 4].exp;
            results.gil += battle.actors[i + 4].gil;
            results.ap += e->ap;
            if (units.slots[i + 4].flags1 & 0x11)
                continue;
            item = func_800B0F04(e, 0, 0, 0x100);
            if ((item & 0xFFFF) == 0xFFFF)
                continue;
            for (k = 0; k < kinds; k++) {
                if (results.spoils[k].id == (item & 0xFFFF)) {
                    results.spoils[k].count++;
                    break;
                }
            }
            if (k == kinds) {
                kinds = k + 1;
                results.spoils[k].id = item;
                results.spoils[k].count = 1;
            }
        }
        if (party_effects[0])
            results.gil = results.gil * party_effects[0] >> 4;
        func_800254D8();
        for (i = 0; i < 3; i++) {
            for (k = 0; k < 9; k++) {
                if (D_8009CBDC[i] != D_8009C738[k].id)
                    continue;
                gain = 0;
                who &= ~(1 << D_8009CBDC[i]);
                c = &D_8009C738[k];
                pm = &members[i];
                if (!(battle.actors[i].status & 0x4001)) {
                    func_800A4ACC(i, results.ap);
                    gain = results.exp;
                    if (pm->misc[0])
                        gain = gain * pm->misc[0] >> 4;
                }
                res_level_up(c, gain, i);
                results.shown[i].new_limits = res_learn_limits(c, units.members[i].kills, units.members[i].uses);
                diff = units.members[i].materia ^ units.members[i].materia2;
                if (diff)
                    res_801B0C74(c, diff);
            }
        }
        res_share_exp(who);
    }
    res_store_party(hp, mp);
    func_800BB944();
    D_80095DD4 = next;
    if (D_80095DD4 == 4)
        func_80022DE4();
}

/* Writes the party's HP, MP, limit bars and Sadness or Fury back to their characters, adding the battle's AP to the
 * count at D_8009D58A; members in battle_data.given get hp and mp instead, and 7777 HP becomes 1. */
#ifdef NON_MATCHING
/* Same length: the record copy at the end keeps its source and end in registers set up before the search, and
 * its destination as a pointer stepped with the search, where GCC sets them up when it finds the record. */
void res_store_party(int hp, int mp)
{
    Character *c;
    unsigned int n, k;
    int m, h, mm, s, id, i;

    for (k = 0; k < 4; k++)
        ((unsigned char *)&n)[k] = D_8009D58A[k];
    n += results.ap;
    for (k = 0; k < 4; k++)
        D_8009D58A[k] = ((unsigned char *)&n)[k];
    for (m = 0; m < 3; m++) {
        h = battle.actors[m].hp;
        mm = battle.actors[m].mp;
        id = D_80163790[m];
        if (battle_data.given >> m & 1) {
            h = hp;
            mm = mp;
        }
        if (h == 7777)
            h = 1;
        for (i = 0; i < 9; i++) {
            c = &D_8009C738[i];
            if (id == c->id) {
                c->hp = h;
                c->mp = mm;
                c->limit_bar = units.members[m].limit_bar;
                c->flags = battle.actors[m].status & 0x30;
                if (battle.flags & 0x10 && !c->id) {
                    for (s = 0; s < 12; s++) {
                        if (limit_id(id, s) != 0x7F)
                            c->limits |= 1 << s;
                    }
                    c->limit_level = 4;
                    c->limit_bar = 0;
                }
                break;
            }
        }
    }
    if (D_8016376A & 0x40) {
        for (i = 0; i < 9; i++) {
            if (D_80167938.id == D_8009C738[i].id) {
                D_8009C738[i] = D_80167938;
                break;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/overlay/batres/nonmatchings/batres", res_store_party);
#endif


/* ORs bits << 8 into the AP of each of c's materia of kind 7. */
void res_801B0C74(Character *c, int bits)
{
    int i;

    bits <<= 8;
    for (i = 0; i < 8; i++) {
        if ((c->weapon_materia[i] & 0xFF) != 0xFF && (materia_data[c->weapon_materia[i] & 0xFF].type & 0xF) == 7)
            c->weapon_materia[i] |= bits;
        if ((c->armor_materia[i] & 0xFF) != 0xFF && (materia_data[c->armor_materia[i] & 0xFF].type & 0xF) == 7)
            c->armor_materia[i] |= bits;
    }
}

/* Half the battle's EXP to each character in who (a bit by id), keeping their HP and MP as far below the maximum. */
void res_share_exp(int who)
{
    Character *c;
    int i, hp, mp;

    for (i = 0; i < 9; i++) {
        if (D_8009C738[i].id < 9 && who >> D_8009C738[i].id & 1) {
            c = &D_8009C738[i];
            hp = c->base_hp;
            mp = c->base_mp;
            res_level_up(c, results.exp / 2, -1);
            if (c->hp) {
                if (hp < c->base_hp)
                    c->hp += c->base_hp - hp;
                if (mp < c->base_mp)
                    c->mp += c->base_mp - mp;
            }
        }
    }
}

/* Clears what the results screens show and starts the EXP count, hiding its empty places. */
void res_clear(void)
{
    int i, hidden;

    hidden = 0;
    results.exp = 0;
    results.ap = 0;
    results.gil = 0;
    for (i = 0; i < 4; i++) {
        results.spoils[i].id = -1;
        results.spoils[i].won = 0;
    }
    for (i = 0; i < 3; i++) {
        results.shown[i].new_limits = 0;
        results.shown[i].level = 0;
    }
    for (i = 0; i < 3; i++) {
        if (D_8009CBDC[i] == 0xFF)
            hidden |= 1 << i;
    }
    exp_tally_start(hidden);
}

/* Levels c with gain EXP; slot, unless -1, is its place on the results screen, whose count it sets up. */
void res_level_up(Character *c, unsigned int gain, int slot)
{
    Growth w;
    int i;

    res_unpack(&w, c);
    if (slot != -1) {
        exp_tally.frac[slot] = 0;
        exp_tally.level[slot] = w.level;
    }
    res_growth_init(&w);
    w.gain = gain;
    res_level_to(&w);
    if (slot != -1) {
        exp_tally.exp_end[slot] = w.total;
        exp_tally.exp[slot] = w.exp;
        exp_tally.levels[slot][0] = w.start;
        for (i = 0; i < 11; i++)
            exp_tally.levels[slot][i + 1] = w.levels[i];
    }
    res_pack(&w, c);
}

void res_unpack(Growth *w, Character *c)
{
    w->id = c->id;
    w->level = c->level;
    w->base[0] = c->stats[0];
    w->base[1] = c->stats[1];
    w->base[2] = c->stats[2];
    w->base[3] = c->stats[3];
    w->base[4] = c->stats[4];
    w->base[5] = c->stats[5];
    w->base[6] = c->base_hp;
    w->base[7] = c->base_mp;
    w->exp = c->exp;
}

void res_pack(Growth *w, Character *c)
{
    c->level = w->level;
    c->stats[0] = w->stats[0];
    c->stats[1] = w->stats[1];
    c->stats[2] = w->stats[2];
    c->stats[3] = w->stats[3];
    c->stats[4] = w->stats[4];
    c->stats[5] = w->stats[5];
    c->base_hp = w->stats[6];
    c->base_mp = w->stats[7];
    c->exp = w->total;
    c->exp_to_next = w->next - w->total;
    c->exp_bar = 0;
    if (w->next != w->reached)
        c->exp_bar = (w->total - w->reached) * 61 / (w->next - w->reached);
    if (w->level >= 99) {
        c->exp_to_next = 0;
        c->exp_bar = 61;
    }
}

void res_growth_init(Growth *w)
{
    int i;

    w->kernel = &kernel_characters[w->id];
    for (i = 0; i < 8; i++) {
        if (!w->base[i])
            w->base[i] = 1;
        w->stats[i] = w->base[i];
    }
    for (i = 15; i >= 0; i--)
        w->levels[i] = 0;
}

void res_level_to(Growth *w)
{
    unsigned int exp;
    int n, i, level;

    n = 0;
    w->total = w->exp + w->gain;
    if (w->total > 999999999)
        w->total = 999999999;
    w->reached = w->start = res_exp_for(w, w->level - 1);
    level = w->level;
    while (level < 99) {
        exp = res_exp_for(w, level);
        w->next = exp;
        if (n < 16)
            w->levels[n++] = exp;
        if (w->total < exp)
            break;
        for (i = 0; i < 6; i++)
            res_stat_gain(w, i);
        res_hp_gain(w);
        res_mp_gain(w);
        w->level++;
        w->reached = exp;
        level = w->level;
    }
}

/* The EXP that level + 1 begins at; also sets w's bracket for level. */
unsigned int res_exp_for(Growth *w, int level)
{
    unsigned int exp;
    int i;

    exp = 0;
    w->bracket = 0;
    if (level) {
        if (level < 61)
            w->bracket = (level - 1) / 10;
        else
            w->bracket = (level - 61) / 20 + 6;
    }
    for (i = 1; i <= level; i++)
        exp += kernel_curves[w->kernel->curves[8]][w->bracket].gradient * i * i / 10;
    return exp;
}

/* Which of a gain table's twelve entries: by how far under the curve, with a little chance. */
int res_gain_index(int under)
{
    int i;

    i = under + (rand_byte() & 7) + 1;
    if (i < 0)
        i = 0;
    else if (i >= sizeof kernel_stat_gains)
        i = sizeof kernel_stat_gains - 1;
    return i;
}

int res_stat_curve(Growth *w, int level, int stat)
{
    int curve;

    curve = w->kernel->curves[stat];
    return kernel_curves[curve][w->bracket].gradient * level / 100
        + kernel_curves[curve][w->bracket].base;
}

int res_hp_curve(Growth *w, int level)
{
    int curve;

    curve = w->kernel->curves[6];
    return kernel_curves[curve][w->bracket].gradient * level
        + kernel_curves[curve][w->bracket].base * 40;
}

int res_mp_curve(Growth *w, int level)
{
    int curve;

    curve = w->kernel->curves[7];
    return kernel_curves[curve][w->bracket].gradient * level / 10
        + kernel_curves[curve][w->bracket].base * 2;
}

void res_stat_gain(Growth *w, int stat)
{
    int v;

    v = w->stats[stat]
        + kernel_stat_gains[res_gain_index(res_stat_curve(w, w->level + 1, stat) - w->stats[stat])];
    if (v > 100)
        v = 100;
    w->stats[stat] = v;
}

void res_hp_gain(Growth *w)
{
    int now, next, v;

    now = res_hp_curve(w, w->level);
    next = res_hp_curve(w, w->level + 1);
    v = w->stats[6]
        + (next - now) * kernel_hp_gains[res_gain_index(now * 100 / w->stats[6] - 100)] / 100;
    if (v > 9999)
        v = 9999;
    w->stats[6] = v;
}

void res_mp_gain(Growth *w)
{
    int now, next, v;

    now = res_mp_curve(w, w->level);
    next = res_mp_curve(w, w->level + 1);
    v = w->stats[7]
        + (next - now) * kernel_mp_gains[res_gain_index(now * 100 / w->stats[7] - 100)] / 100;
    if (v > 999)
        v = 999;
    w->stats[7] = v;
}

/* Adds a battle's kills and uses of the current limit level to c's, and learns the limits they reach: kills learn
 * slots 3 and 6, uses the current level's next two slots. Returns the limits newly learned. */
#ifdef NON_MATCHING
/* Same code but for register choice: the game keeps the limits in a3 and the counters in a1 and t0. */
unsigned short res_learn_limits(Character *c, int kills, int uses)
{
    int learned, old, total, n, level, id, i;
    unsigned short *need;

    total = c->kills;
    old = learned = c->limits;
    level = (c->limit_level - 1) & 3;
    id = c->id;
    total += kills;
    if (total > 0xFFFF)
        total = 0xFFFF;
    if (id < 9) {
        kills = 8;
        for (i = 0; i < 2; i++) {
            if (!(learned & kills) && kernel_characters[id].kills[i] != 0xFFFF
                && total >= kernel_characters[id].kills[i])
                learned |= kills;
            kills <<= 3;
        }
    }
    if (level < 3) {
        n = c->limit_uses[level];
        n += uses;
        if (n > 0xFFFF)
            n = 0xFFFF;
        if (id < 9) {
            uses = 2;
            need = kernel_characters[id].uses[level];
            for (i = 0; i < level; i++)
                uses <<= 3;
            for (i = 0; i < 2; i++) {
                if (!(learned & uses) && need[i] != 0xFFFF && n >= need[i])
                    learned |= uses;
                uses <<= 1;
            }
        }
        c->limit_uses[level] = n;
    }
    c->kills = total;
    c->limits = learned;
    return old ^ learned;
}
#else
INCLUDE_ASM("asm/overlay/batres/nonmatchings/batres", res_learn_limits);
#endif
