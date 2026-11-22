/* BATTLE/BATINI.X, 0x801B0000-0x801B27B4 (a jump table and four small tables before the code, from 0x50): the
 * battle's setup, run at 0x801B0000 straight after BATTLE.X is read. It reads the battle's scene from SCENE.BIN,
 * builds the party's actors from the save and the enemies' from the scene, copies the items, sets the battle type's
 * row and side flags and the ATB start values. GCC 2.7.2, ASPSX 2.34 with its divide checks, -G0. */

#include "common.h"
#include "main.h"
#include "battle.h"

typedef struct {
    unsigned char attack, x1, targets, x3, x4, x5;
} EnemyCommand;
extern EnemyCommand enemy_commands[6][16];

typedef struct {
    unsigned char a, b;
    signed char c;
    unsigned char pad3[5];
} BattleQueue;
extern BattleQueue D_800F6934[64];
extern BattleQueue D_800F6B34[10];
extern struct {
    signed char a;
    unsigned char pad1[7];
} D_800F6B86[2];

extern unsigned char battle_items_rows;

extern struct {
    unsigned short items[320];
} inventory;
extern PartyMember members[3];
extern unsigned char D_8009CBDC[];
extern Character D_80167938;
extern unsigned short D_8016375A, D_8016376A, D_8016376C, D_8016376E, D_80163770, D_80163772;
extern unsigned char D_800F7DD4[3];
extern unsigned short D_800F7DA6, D_800F7DA8, D_800F7DAA, D_800F7DB2, D_800F7DB6, D_800F7DE8;
extern unsigned char D_800F9DA0;
extern int D_800F89F0[6][32];
extern int D_800FAFD0, D_800F7ED0, D_801620A8;
extern unsigned int D_80062F88;
extern unsigned char D_80083184[64]; /* KERNEL.BIN: the first scene of each 8 KB block of SCENE.BIN */
extern struct {
    unsigned char pad0[0x28];
    unsigned int limit_need[4];
} kernel_limits[9];

typedef struct {
    unsigned char cursor, targets;
    unsigned char pad2[6];
} Command;
extern Command command_data[];
typedef struct {
    unsigned char pad0[0xC];
    unsigned char targets;
    unsigned char padD[15];
} Spell;
extern Spell spell_data[];

int VSync(int mode);
void rand_seed(int seed);
unsigned char rand_byte(void);
int rand_below(int n);
void party_effects_sum(void);
unsigned char *battle_text_unpacked(int id);
void battle_set_error(int code);
int limit_id(unsigned int character, int slot);
int disc_file_lba(int file);
int cd_read(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
void gunzip(void *src, void *dest);
void copy_words(void *dest, void *src, int size);
void func_800A3278(void);
void func_800A283C(void);
void func_800AD480(void);
void func_800A71F4(void);
void func_800DCF94(int a);
void func_800A55BC(void);
void func_800AE954(int actor);
void func_800A4540(void);
void func_800A61D4(void);
void func_800A4480(void);
void func_800A5BC8(int member, int a);
void func_800A2894(void);
void func_800A7254(int a, int actor, int b, int c);
void func_800A6000(int actor, int a, int b);
void func_800B108C(int actor);
void func_800B1060(int a);
unsigned int func_800B2F50(void);
void func_800A4BA4(int member);
void func_800A4E80(int member);
void func_800A3354(void);
void func_80020058(int slot);
void member_update(int slot);

void ini_party(void);
void ini_party_again(void);
void ini_801B1120(void);
void ini_member_commands(int member);
void ini_clear_commands(int member);
void ini_limits(int character, int learned, unsigned char *limits);
unsigned int ini_char_materia(Character *c);
void ini_accessory(int member, int accessory);
int ini_801B1734(int member);
void ini_copy_stats(PartyMember *pm, BattleMember *rec, BattleActor *actor);
void ini_layout(void);
void ini_items(void);
void ini_enemies(void);
void ini_801B2308(void);
void ini_load_scene(int id, void (*idle)(void));
int ini_scene_block(int scene);
int ini_boost(int value);
void ini_set_speed(int speed);
void ini_atb(void);

/* The module's entry (BATTLE.X calls it through the table at 0x801B0000): sets up battle id, its party, enemies,
 * items, layout and ATB, and for a battle in stages the enemies' HP from the stage before. */
#ifdef NON_MATCHING
/* Same length: the actors' base for the present bits stays in another register, and the stage loop's ~0x18 is
 * loaded once before the loop. */
void ini_start(int id)
{
    BattleActor *actor;
    unsigned char *text;
    int i, n, bit;

    rand_seed(VSync(-1));
    VSync(-1);
    for (i = 0; i < 3; i++) {
        func_80020058(i);
        member_update(i);
    }
    party_effects_sum();
    text = battle_text_unpacked(0x7E);
    D_800FAFD0 = text[0];
    D_800F7ED0 = text[1];
    func_800A3278();
    func_800A283C();
    func_800AD480();
    for (i = 0; i < 64; i++)
        D_800F6934[i].a = 0xFF;
    for (i = 0; i < 10; i++)
        D_800F6B34[i].a = 0xFF;
    for (i = 0; i < 2; i++)
        D_800F6B86[i].a = -1;
    func_800A71F4();
    D_801620A8 = -1;
    func_800DCF94(-1);
    for (i = 0; i < 10; i++) {
        battle.actors[i].present = -1;
        battle.actors[i].x13 = 0x10;
    }
    func_800A55BC();
    ini_party();
    ini_items();
    ini_load_scene(id, 0);
    ini_enemies();
    ini_set_speed(settings.battle_speed);
    D_800F7DAA = (settings.config & 0xC0) >> 6;
    actor = battle.actors;
    for (i = 0; i < 10; i++, actor++) {
        func_800AE954(i);
        if (actor->present != -1)
            battle.present |= 1 << i;
    }
    battle.layout = scene.setup.layout;
    battle.id = id;
    ini_layout();
    func_800A4540();
    ini_801B1120();
    ini_801B2308();
    func_800A61D4();
    func_800A4540();
    ini_atb();
    func_800A4480();
    D_800F7DE8 |= 1;
    for (i = 0; i < 3; i++)
        func_800A5BC8(i, 1);
    if (battle.flags & 8) {
        ini_set_speed(0x80);
        D_800F7DAA = 0;
        for (i = 0; i < 3; i++)
            ini_clear_commands(i);
    } else
        func_800A2894();
    if (!(battle.flags & 4))
        return;
    if (!(D_80062F88 & 4)) {
        D_80062F88 |= 4;
        stage_hp.dead = -1;
    }
    n = battle.stage;
    stage_hp.stage = n;
    bit = 1 << n * 6;
    for (i = 0; i < 6; i++) {
        if (stage_hp.dead & bit)
            stage_hp.dead &= ~bit;
        else if (battle.actors[i + 4].present != -1) {
            battle.actors[i + 4].hp = stage_hp.hp[n][i];
            if (!battle.actors[i + 4].hp) {
                battle.actors[i + 4].status |= 1;
                battle.actors[i + 4].x44 |= 1;
                battle.actors[i + 4].flags4 &= ~0x18;
            }
        }
        bit <<= 1;
    }
    func_800A4540();
}
#else
INCLUDE_ASM("asm/overlay/batini/nonmatchings/batini", ini_start);
#endif

/* The next stage of a battle in stages: its enemies, the party again if D_8016376C. */
#ifdef NON_MATCHING
/* Same length: the actors' base for the present bits stays in another register. */
void ini_next_stage(int id)
{
    BattleActor *actor;
    int i, n;

    n = 4;
    if (D_8016376C) {
        n = 0;
        func_800A7254(0, 0, 0xF, 0);
        func_800A7254(0, 0, 0xE, 0);
    }
    for (i = 0; i < 64; i++) {
        if (D_800F6934[i].c >= n)
            D_800F6934[i].c = -1;
    }
    for (i = 0; i < 3; i++)
        units.members[i].x6 = 0;
    if (D_8016376C)
        ini_party_again();
    ini_load_scene(id, func_800A3354);
    ini_enemies();
    battle.present = 0;
    actor = battle.actors;
    for (i = 0; i < 10; i++, actor++) {
        func_800AE954(i);
        if (actor->present != -1)
            battle.present |= 1 << i;
    }
    battle.layout = scene.setup.layout;
    battle.id = id;
    ini_layout();
    func_800A4540();
    ini_801B2308();
    func_800A4540();
    for (i = 4; i < 10; i++) {
        units.slots[i].atb = ((rand_below(64) & 0xFF) + 0x80) << 8;
        func_800B108C(i);
    }
}
#else
INCLUDE_ASM("asm/overlay/batini/nonmatchings/batini", ini_next_stage);
#endif

/* The ATB start values: from each actor's speed, then by the battle type. */
void ini_atb(void)
{
    int atb[10];
    int mask, top, i, v;

    mask = D_8016375A;
    top = 0;
    for (i = 0; i < 10; i++) {
        units.slots[i].atb = 0;
        v = 0;
        if (mask >> i & 1) {
            v = (func_800B2F50() & 0xFFFF) >> 1;
            if (top < v)
                top = v;
        }
        atb[i] = v;
    }
    for (i = 0; i < 10; i++) {
        if (!(mask >> i & 1))
            continue;
        switch (battle_data.type) {
        case 0:
        case 5:
            atb[i] += 0xE000;
            atb[i] -= top;
            break;
        case 2:
        case 4:
            if (i < 4)
                atb[i] = 0;
            else {
                atb[i] += 0xF000;
                atb[i] -= top;
            }
            break;
        default:
            if (i < 4)
                atb[i] = 0xFFFE;
            else
                atb[i] >>= 3;
            break;
        }
        if (battle.flags & 8) {
            if (i < 3)
                atb[i] = 0xFFFE;
            else
                atb[i] = 0;
        }
        units.slots[i].atb = atb[i];
    }
    for (i = 0; i < 3; i++)
        members[i].atb = units.slots[i].atb;
}

/* The battle speed from the setting (0 fastest, 0xFF slowest). */
void ini_set_speed(int speed)
{
    D_800F7DA6 = 0x10000 / ((speed * 480 / 256 + 120) * 2);
}

/* The party's actors and battle records from the save and the members' worked-out stats, their limits, and the
 * party's average Dexterity (D_800F7DA8, plus 50). */
#ifdef NON_MATCHING
/* Same length: the bit's 1 is loaded a few instructions early, and the average's dex - 1 goes through another
 * register. */
void ini_party(void)
{
    BattleSlot *slot;
    BattleExtra *extra;
    BattleMember *rec;
    PartyMember *pm;
    BattleActor *actor;
    Character *c;
    ArmorData *armor;
    unsigned char *w;
    int m, dex, n, i, k, id, bit, v, level;

    dex = 0;
    n = 0;
    battle.party = 0;
    for (m = 0; m < 3; m++) {
        id = D_8009CBDC[m];
        places[m].id = -1;
        slot = &units.slots[m];
        rec = &units.members[m];
        extra = &units.extra[m];
        pm = &members[m];
        actor = &battle.actors[m];
        if (id == 0xFF)
            continue;
        for (i = 0; i < 9; i++) {
            c = &D_8009C738[i];
            if (D_8016376A & 0x40)
                D_80167938 = *c;
            if (c->id != id)
                continue;
            slot->xC = 0xFF;
            slot->flags1 = 1;
            bit = 1;
            slot->xF = 0xFF;
            places[m].id = id;
            actor->present = id;
            actor->xC = id + 0x10;
            actor->level = c->level;
            actor->x56 = 8;
            actor->x11 = 5;
            actor->state16 = 0;
            actor->flags4 = 8;
            rec->character = c;
            battle.party |= bit << m;
            if (!(c->row & 1))
                actor->flags4 |= 0x40;
            actor->hp = pm->hp;
            actor->mp = pm->mp;
            rec->hp = actor->hp;
            rec->mp = actor->mp;
            ini_copy_stats(pm, rec, actor);
            actor->x12 = 0x10;
            actor->status = c->flags & 0x30;
            actor->x44 = 0;
            slot->x34 = pm->statuses[1];
            w = (unsigned char *)&pm->weapon;
            extra->weapon0 = w[0];
            extra->weapon1 = w[1];
            extra->weapon2 = w[2];
            extra->hit = w[8];
            extra->crit = w[0x27];
            extra->weapon7 = w[7];
            extra->elements = pm->weapon.elements | pm->elements[0];
            extra->x10 = *(unsigned short *)&w[0xC];
            extra->x12 = *(unsigned short *)&w[0x28];
            extra->statuses = pm->statuses[0];
            armor = &armor_table[c->armor];
            actor->defense_pct = armor->defense_pct;
            actor->x4D = armor->mdefense_pct;
            ini_accessory(m, c->accessory);
            actor->x50 = 0;
            actor->x52 = 0xFFFF;
            for (k = 0; k < 3; k++) {
                v = (w + k)[0x24];
                if (w[0xB] & bit)
                    v |= 0x100;
                extra->x8[k] = v;
                bit <<= 1;
            }
            slot->flags1 &= ~2;
            if (pm->flags23 & 4)
                extra->weapon0 &= ~0x20;
            if (!(extra->weapon0 & 0x20))
                slot->flags1 |= 2;
            slot->atb = 0;
            pm->pad22 = 0;
            pm->atb = 0;
            pm->pad1C[0] = 0;
            pm->pad1C[1] = 0;
            pm->limit_bar = 0;
            pm->limit_level = 1;
            rec->limit_level = 0xFF;
            if (id < 9) {
                level = c->limit_level - 1;
                if ((unsigned int)level < 4) {
                    rec->limit_bar2 = rec->limit_bar = c->limit_bar;
                    rec->limit_level = level;
                    rec->limit_need = kernel_limits[id].limit_need[level];
                    pm->limit_bar = rec->limit_bar << 8;
                    pm->limit_level = c->limit_level;
                    ini_limits(id, c->limits, pm->limits);
                    if (rec->limit_need)
                        goto ok;
                }
                battle_set_error(0x26);
            }
        ok:
            actor->flags4 |= 8;
            if (!actor->hp)
                actor->status |= 1;
            ini_member_commands(m);
            func_800A4BA4(m);
            if (rec->limit_bar == 0xFF) {
                func_800A4E80(m);
                units.slots[m].x8 &= ~1;
            }
            if (actor->status)
                func_800B108C(m);
            rec->materia2 = rec->materia = ini_char_materia(c);
            n++;
            dex += c->stats[4];
            break;
        }
    }
    if (n) {
        id = dex - 1; /* in id's register, as the game has it */
        D_800F7DA8 = (id + n) / n + 50;
    }
}
#else
INCLUDE_ASM("asm/overlay/batini/nonmatchings/batini", ini_party);
#endif

/* The party's actors again for a battle's next stage: level, HP, MP and the battle's records from the save. */
void ini_party_again(void)
{
    BattleSlot *slot;
    BattleExtra *extra;
    PartyMember *pm;
    BattleActor *actor;
    BattleMember *rec;
    Character *c;
    int m, i, id;

    for (m = 0; m < 3; m++) {
        slot = &units.slots[m];
        rec = &units.members[m];
        extra = &units.extra[m];
        pm = &members[m];
        actor = &battle.actors[m];
        id = D_8009CBDC[m];
        if (id == 0xFF)
            continue;
        for (i = 0; i < 9; i++) {
            c = &D_8009C738[i];
            if (c->id != id)
                continue;
            actor->level = c->level;
            actor->hp = c->hp;
            actor->mp = c->mp;
            slot->hp = actor->hp;
            slot->mp = actor->mp;
            ini_copy_stats(pm, rec, actor);
            slot->x34 = pm->statuses[1];
            extra->elements = pm->weapon.elements | pm->elements[0];
            extra->statuses = pm->statuses[0];
            extra->hit = pm->weapon.hit;
            extra->weapon0 = pm->weapon.pad0[0];
            slot->flags1 &= ~2;
            if (pm->flags23 & 4)
                extra->weapon0 &= ~0x20;
            if (!(extra->weapon0 & 0x20))
                slot->flags1 |= 2;
            ini_accessory(m, c->accessory);
            ini_member_commands(m);
            func_800A4BA4(m);
            if (!ini_801B1734(m))
                func_800B108C(m);
            break;
        }
    }
}

/* Runs func_800A6000 for each member in the battle that isn't dead. */
void ini_801B1120(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (places[i].id != -1 && !(battle.actors[i].status & 1))
            func_800A6000(i, 0, 0);
    }
}

/* The member's commands' targets for the battle, the menu's rows, and which spells it can cast. */
void ini_member_commands(int member)
{
    PartyMember *pm;
    int i, k, id, targets;

    pm = &members[member];
    pm->command_rows = 1;
    for (i = 0; i < 16; i++) {
        targets = 0xFF;
        id = pm->commands[i].id;
        k = id; /* the game tests the id through two registers */
        if (k != 0xFF) {
            targets = command_data[k].targets;
            if (targets == 0xFF)
                targets = units.extra[member].weapon0;
            if (k < 0x1C) {
                if (id >= 0x18)
                    pm->commands[i].uses = 0xFF;
            }
            if (pm->commands[i].cursor == 7) {
                if (units.slots[member].flags1 & 2)
                    pm->commands[i].cursor = 0;
                if (pm->commands[i].uses && pm->commands[i].id != 0x19)
                    targets |= 0xC;
                if (pm->commands[i].id == 5 || pm->commands[i].id == 0x11) {
                    targets |= 0x10;
                    if (pm->commands[i].uses)
                        pm->commands[i].cursor = 0;
                }
            }
            pm->command_rows = i / 4 + 1;
        }
        pm->commands[i].targets = targets;
    }
    for (k = 0; k < 96; k++) {
        id = pm->spells[k].id;
        if (id == 0xFF)
            continue;
        if (k >= 72)
            id += 0x48;
        else if (k >= 56)
            id += 0x38;
        if (k < 56 && !(spell_data[id].targets & 8))
            pm->spells[k].uses = 0;
    }
}

/* Clears a member's commands but the first. */
void ini_clear_commands(int member)
{
    PartyMember *pm;
    int i;

    pm = &members[member];
    pm->command_rows = 1;
    for (i = 1; i < 4; i++) {
        pm->commands[i].id = 0xFF;
        pm->commands[i].cursor = 0;
        pm->commands[i].targets = 0;
        pm->commands[i].pad3 = 3;
        pm->commands[i].uses = 0;
        pm->commands[i].flags = 0;
    }
}

/* Fills the member's learned limits of its level (limits[3]-[5]) and their count (limits[6]). */
void ini_limits(int character, int learned, unsigned char *limits)
{
    int n, k, s;

    n = 0;
    for (k = 0; k < 3; k++) {
        if (limits[k] != 0xFF) {
            for (s = 0; s < 12; s++) {
                if (limit_id(character, s) == limits[k])
                    break;
            }
            if (s == 12)
                battle_set_error(0x26);
            else if (learned >> s & 1) {
                n++;
                limits[k + 3] = limits[k * 0x1C + 0x14];
            }
        }
    }
    limits[7] = 0;
    limits[6] = n;
}

/* A materia's bits for the battle: the AP above a kind 7 materia's id, with the top bit set. */
unsigned int ini_materia(unsigned int materia)
{
    unsigned int id, r;

    id = materia & 0xFF;
    r = 0;
    if (id != 0xFF && (materia_data[id].type & 0xF) == 7)
        r = materia >> 8 | 0x80000000;
    return r;
}

/* The kind 7 bits of all of a character's materia. */
unsigned int ini_char_materia(Character *c)
{
    unsigned int r;
    int i;

    r = 0;
    for (i = 0; i < 8; i++) {
        r |= ini_materia(c->weapon_materia[i]);
        r |= ini_materia(c->armor_materia[i]);
    }
    return r;
}

/* The accessory's special effect: statuses it gives for the battle, kept in the member's record. */
void ini_accessory(int member, int accessory)
{
    BattleMember *rec;
    BattleSlot *slot;
    BattleActor *actor;
    unsigned int s;

    slot = &units.slots[member];
    rec = &units.members[member];
    actor = &battle.actors[member];
    actor->status &= ~rec->statuses;
    slot->x34 &= ~rec->statuses;
    rec->statuses = 0;
    slot->special = 0xFF;
    if (accessory == 0xFF)
        return;
    slot->special = accessory_table[accessory].special;
    switch (slot->special) {
    case 0:
        actor->status |= 0x100;
        slot->x34 |= 0x100;
        rec->statuses |= 0x100;
        break;
    case 1:
        s = 0x800000;
        actor->status |= s;
        slot->x34 |= s;
        rec->statuses |= s;
        break;
    case 2:
        s = 0x200000;
        actor->status |= s;
        slot->x34 |= s;
        rec->statuses |= s;
        slot->x10[2] = 0xFF;
        break;
    case 3:
        s = 0x40000;
        actor->status |= s;
        slot->x34 |= s;
        rec->statuses |= s;
        break;
    case 6:
        s = 0x30000;
        actor->status |= s;
        rec->statuses |= s;
        break;
    }
}

/* The statuses the battle starts the member with (D_800F9DA0), as messages; returns 1 if any. */
int ini_801B1734(int member)
{
    static const unsigned int statuses[4] = { 0, 0x1000, 8, 0x800 };
    static const int messages[4] = { 0, 10, 0x27, 10 };
    unsigned int immune;
    int i, any;

    immune = units.slots[member].x34;
    battle.actors[member].status &= ~0x200000;
    any = 0;
    if (units.slots[member].flags1 & 8)
        immune |= 0x800;
    if (D_800F9DA0 & 0x10) {
        battle.actors[member].hp += battle.actors[member].max_hp >> 1;
        if (battle.actors[member].max_hp < battle.actors[member].hp)
            battle.actors[member].hp = battle.actors[member].max_hp;
        func_800A7254(2, member, 0x17, 0);
    }
    for (i = 0; i < 4; i++) {
        if (D_800F9DA0 >> i & 1) {
            battle.actors[member].status |= statuses[i] & ~immune;
            func_800A7254(2, member, 0x17, messages[i]);
            any = 1;
        }
    }
    return any;
}

/* The member's stats into its actor and record. */
void ini_copy_stats(PartyMember *pm, BattleMember *rec, BattleActor *actor)
{
    actor->dex = pm->primary[4];
    actor->luck = pm->primary[5];
    actor->max_hp = pm->max_hp;
    actor->max_mp = pm->max_mp;
    actor->attack = pm->stats[0];
    actor->mattack = pm->stats[2];
    actor->defense = pm->stats[1];
    actor->mdefense = pm->stats[3];
    if (!actor->attack)
        actor->attack = 1;
    rec->max_hp = actor->max_hp;
    rec->max_mp = actor->max_mp;
    if (pm->flags23 & 8) {
        rec->hp_cap = 999;
        rec->mp_cap = 9999;
    } else {
        rec->hp_cap = 9999;
        rec->mp_cap = 999;
    }
}

/* The battle type's sides and rows: which actors face back (flag 2) and which are in the back row (0x80). */
#ifdef NON_MATCHING
/* Same length: the rows and the two cases' temporaries take other registers. */
void ini_layout(void)
{
    static const unsigned char effects[8] = { 0xFF, 0x32, 0x33, 0x34, 0x35, 0xFF, 0x48, 0x07 }; /* by battle type */
    unsigned short side[3];
    int enemies, party, mask, rows, effect, t, i;
    unsigned int f, b;

    enemies = battle.enemies;
    party = battle.party;
    mask = 5;
    if (scene.setup.layout == 7)
        mask = -6;
    effect = effects[battle_data.type];
    if (effect != 0xFF && battle.id != 0x3D6)
        func_800B1060(effect);
    side[2] = side[1] = side[0] = rows = 0;
    switch (battle_data.type) {
    case 0:
        rows = enemies;
    case 1:
        side[0] = party;
        side[1] = enemies;
        break;
    case 2:
        rows = party;
        side[0] = enemies;
        side[1] = rows;
        break;
    case 4:
        side[1] = party;
        for (i = 0; i < 6; i++) {
            if (enemies >> (i + 4) & 1)
                side[battle.actors[i + 4].flags4 & 2] |= 1 << (i + 4);
        }
        t = side[2] | (party & 2);
        rows = t;
        if (battle.id == 0x3D6)
            rows = t & ~party;
        break;
    default:
        t = party & ~mask;
        side[0] = party & mask;
        side[1] = enemies;
        side[2] = t;
        rows = t;
        for (i = 0; i < 6; i++) {
            if (enemies >> (i + 4) & 1 && scene.formation[i].z >= 0)
                rows |= 1 << (i + 4);
        }
        break;
    }
    for (i = 0; i < 10; i++) {
        battle.actors[i].flags4 &= ~0x82;
        if (side[2] >> i & 1)
            battle.actors[i].flags4 |= 2;
        if ((unsigned short)rows >> i & 1)
            battle.actors[i].flags4 |= 0x80;
    }
    for (i = 0; i < 3; i++) {
        f = battle.actors[i].flags4;
        b = f >> 6;
        b &= 1;
        switch (battle_data.type) {
        case 0:
        case 1:
            break;
        case 2:
            b = !b;
            battle.actors[i].flags4 = f ^ 0x40;
            break;
        default:
            battle.actors[i].flags4 &= ~0x40;
            b = 0;
            break;
        }
        places[i].x6 = b;
    }
    D_8016376E = side[0];
    D_80163770 = side[1];
    D_80163772 = side[2];
}
#else
INCLUDE_ASM("asm/overlay/batini/nonmatchings/batini", ini_layout);
#endif

/* The inventory's items for the battle's Item menu: each one's count, kind and targets. */
void ini_items(void)
{
    BattleItem *it;
    int i, last, n, id, count, kind, targets;

    last = 0;
    for (i = 0; i < 320; i++) {
        it = &battle_items[i];
        id = inventory.items[i];
        count = 0;
        kind = 0;
        targets = 0xB;
        if (id != 0xFFFF) {
            count = inventory.items[i] >> 9;
            id &= 0x1FF;
            if (id < 0x80) {
                targets = item_table[id].usable & 0xB;
                kind = item_table[id].pad0C[0];
            } else if (id < 0x100) {
                targets = weapon_table[id - 0x80].usable & 0xB;
                kind = weapon_table[id - 0x80].pad0[0];
            } else if (id < 0x120) {
                targets = armor_table[id - 0x100].usable & 0xB;
                kind = 3;
            } else if (id < 0x140) {
                targets = accessory_table[id - 0x120].usable & 0xB;
                kind = 3;
            }
            last = i + 1;
        }
        it->id = id;
        it->count = count;
        it->kind = kind;
        it->targets = targets;
    }
    n = (last + 1) / 2;
    if (n < 3)
        n = 3;
    battle_items_rows = n;
}

/* The enemies' actors from the formation and the scene's records, their numbering among their kind and their
 * commands from the scene's attacks. */
#ifdef NON_MATCHING
/* Same length: the command pointer and the attack id swap registers, and the last loop steps its pointer elsewhere. */
void ini_enemies(void)
{
    BattleSlot *slot;
    BattleActor *actor;
    BattleEnemy *e;
    EnemyCommand *c;
    int i, k, j, n, t;
    unsigned int att;
    short id;

    battle.enemies = 0;
    for (i = 0; i < 3; i++)
        D_800F7DD4[i] = 0;
    for (i = 0; i < 6; i++) {
        for (k = 0; k < 32; k++)
            D_800F89F0[i][k] = 0;
    }
    for (i = 0; i < 6; i++) {
        slot = &units.slots[i + 4];
        actor = &battle.actors[i + 4];
        id = scene.formation[i].id;
        actor->present = -1;
        actor->id = 0xFFFF;
        actor->flags4 = 0;
        actor->status = 0;
        actor->x4F = 0xFF;
        D_800F6B34[i + 4].a = 0xFF;
        for (k = 0; k < 16; k++)
            slot->x10[k] = 0;
        for (k = 0; k < 8; k++)
            slot->x20[k] = 0;
        if (id == -1)
            continue;
        actor->id = id;
        for (k = 0; k < 3; k++) {
            if (scene.enemy_ids[k] == id)
                break;
        }
        D_800F7DD4[k]++;
        id = k;
        e = &battle_data.enemies[id];
        scene.formation[i].id = id;
        actor->present = id;
        actor->hp = actor->max_hp = e->hp;
        actor->mp = actor->max_mp = e->mp;
        actor->attack = e->attack;
        actor->mattack = e->mattack;
        actor->defense = e->defense * 2;
        actor->mdefense = e->mdefense * 2;
        actor->defense_pct = e->defense_pct;
        actor->dex = e->dex;
        actor->luck = e->luck;
        actor->level = e->level;
        actor->x12 = e->xA2;
        actor->gil = e->gil;
        actor->exp = e->exp;
        actor->x11 = 1;
        actor->x4C = 1;
        actor->x56 = 2;
        actor->x10 = 0;
        actor->status = 0;
        actor->x44 = 0;
        actor->x50 = 0;
        actor->x52 = 0xFFFF;
        actor->flags4 = scene.formation[i].flags & 0x1F;
        actor->row = scene.formation[i].row;
        slot->enemy = e;
        slot->special = 0xFF;
        slot->xC = 0xFF;
        slot->xF = 0xFF;
        slot->x34 = ~e->immune;
        battle.enemies |= 1 << (i + 4);
        actor->xC = 0;
        for (k = 0; k < i; k++) {
            if (battle.actors[k + 4].id == actor->id)
                actor->xC++;
        }
        n = 3;
        for (k = 0; k < n; k++) {
            c = &enemy_commands[i][k];
            att = e->attacks[k];
            c->attack = 0xFF;
            c->x1 = 0;
            c->targets = 0;
            c->x3 = 3;
            att &= 0xFFFF;
            if (att == 0xFFFF)
                continue;
            for (j = 0; j < 32; j++) {
                if (battle_data.attack_ids[j] == att) {
                    t = battle_data.attacks[j].targets;
                    if (t)
                        t ^= 2;
                    c->attack = j;
                    c->targets = t;
                    c->x3 = 0;
                    break;
                }
            }
        }
        for (k = n; k < 16; k++) {
            c = &enemy_commands[i][k];
            c->attack = 0xFF;
            c->x1 = 0;
            c->targets = 0;
            c->x3 = 3;
        }
        slot->flags1 = 0;
    }
    for (i = 0; i < 6; i++) {
        if (D_800F7DD4[scene.formation[i].id] >= 2)
            units.slots[i + 4].xF = battle.actors[i + 4].xC;
    }
}
#else
INCLUDE_ASM("asm/overlay/batini/nonmatchings/batini", ini_enemies);
#endif

/* The enemies' flags back into the formation, their places' x1 and their statuses into x44. */
void ini_801B2308(void)
{
    int i;

    for (i = 0; i < 6; i++) {
        if (scene.formation[i].id != -1)
            func_800A6000(i + 4, 0, 0);
    }
    for (i = 0; i < 6; i++) {
        scene.formation[i].flags = battle.actors[i + 4].flags4;
        places[i + 4].x1 = battle.actors[i + 4].x10;
        battle.actors[i + 4].x44 = battle.actors[i + 4].status;
    }
}

/* Reads battle id's scene from SCENE.BIN (four battles a scene, packed scenes in 8 KB blocks) into BATTLE.X's
 * records, and sets the battle type from the setup's layout. */
void ini_load_scene(int id, void (*idle)(void))
{
    static const unsigned char types[12] = { 0, 1, 2, 3, 4, 3, 3, 3, 5, 0x6E, 0x64, 0x62 }; /* by layout */
    unsigned char unused[0x100]; /* unused, but the game's frame has it */
    unsigned char buf[0x1E80];
    unsigned int *dir;
    unsigned short *escape;
    int n, block, k, i;

    dir = (unsigned int *)0x801C0000;
    n = id / 4;
    block = ini_scene_block(n);
    cd_read(disc_file_lba(7) + block * 4, 0x2000, (void *)0x801C0000, 0);
    k = n - D_80083184[block];
    wait_disc_read(idle);
    k = dir[k];
    gunzip(dir + k, buf);
    k = id - n * 4;
    copy_words(&scene, buf, 8);
    copy_words(&scene.setup, buf + 8 + k * 20, 0x14);
    copy_words(scene.cameras, buf + 0x58 + k * 48, 0x30);
    copy_words(scene.formation, buf + 0x118 + k * 96, 0x60);
    copy_words(battle_data.enemies, buf + 0x298, 0x228);
    copy_words(battle_data.attacks, buf + 0x4C0, 0x380);
    copy_words(battle_data.attack_ids, buf + 0x840, 0x40);
    copy_words(battle_data.names, buf + 0x880, 0x400);
    copy_words(battle_data.setup_ai, buf + 0xC80, 0x200);
    copy_words(battle_data.enemy_ai, buf + 0xE80, 0x1000);
    if (D_8016376A & 4 && scene.setup.flags & 0x10) {
        if (!scene.setup.layout)
            scene.setup.layout = 1;
    }
    battle_data.type = types[scene.setup.layout];
    if (D_8016376A & 0x40) {
        scene.setup.area = 0x25;
        scene.setup.flags |= 4;
        scene.setup.camera = (rand_byte() & 3) + 0x60;
        scene.setup.escape = 1;
        for (i = 0; i < 3; i++) {
            battle_data.enemies[i].hp <<= 1;
            battle_data.enemies[i].attack = ini_boost(battle_data.enemies[i].attack);
            battle_data.enemies[i].mattack = ini_boost(battle_data.enemies[i].mattack);
        }
    } else if (D_8016376A & 8)
        scene.setup.flags &= ~4;
    if (!(scene.setup.flags & 4))
        D_8016376A |= 8;
    escape = &D_800F7DB2;
    *escape = scene.setup.escape;
    if (battle_data.type == 1 || battle_data.type == 3)
        *escape = 1;
    D_800F7DB6 = D_800F7DB2;
}

/* Which 8 KB block of SCENE.BIN holds a scene. */
int ini_scene_block(int scene)
{
    unsigned int i;

    for (i = 1; i < 64; i++) {
        if (scene < D_80083184[i])
            break;
    }
    return i - 1;
}

/* value * 1.25, at most 255. */
int ini_boost(int value)
{
    value = value * 125 / 100;
    if (value >= 256)
        value = 255;
    return value;
}
