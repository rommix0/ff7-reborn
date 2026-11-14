/* Support materia, 0x8001786C-0x800197B8: a party member worked out again from the character and equipment, and what
 * each support materia does to the materia in the slot linked with it (All, Quadra Magic, MP Turbo, Elemental, Added
 * Effect, the commands' extras). GCC 2.6.3, ASPSX 2.21 and -G8 like materia_ap.c. */

#include "common.h"
#include "main.h"

PartyMember *cur_member;
unsigned char materia_detail;
extern unsigned int member_materia[16]; /* the weapon's eight, then the armor's */

unsigned char *armor_slots(int slot);
void func_8001BDB0(int slot, PartyMember *m);
void materia_own(unsigned int materia);
void materia_pair(unsigned char a, unsigned char b, unsigned int m1, unsigned int m2, unsigned char x, unsigned char y,
                  unsigned char armor);
void func_8001BC18(void);
void func_8001B944(void);
void func_8001B570(void);
void func_8001AEE4(void);
void func_8001AE08(void);
void func_80019690(unsigned int materia);
void func_8001C0EC(int slot);
extern short D_80069548[];

/* The member's spells as they're being made. */
typedef struct {
    unsigned char id;
    unsigned char all;
    unsigned char quadra;
    unsigned char quadra_uses;
    unsigned char flags; /* as MemberSpell.flags */
} SpellSlot;
extern unsigned char summon_quadra[];      /* 16 */
extern unsigned char summon_quadra_uses[]; /* 16 */
extern SpellSlot spell_slots[];     /* 56 */

int support_count; /* member's support[] in use */
int support_kind;
short support_sub;
unsigned char support_spread;
extern unsigned char support_params[]; /* the support materia's attrs[1-5], by its level */

void pair_support(unsigned int m1, unsigned int m2, unsigned char x, unsigned char y, unsigned char armor);
void support_apply(unsigned int support, unsigned int paired, unsigned char x, unsigned char armor);
void func_80018390(unsigned char support, unsigned char level, unsigned int paired);
void func_8001840C(unsigned char support, unsigned char level, unsigned int paired);
void func_800184C0(unsigned char support, unsigned char level, unsigned int paired);
void support_add(unsigned char level, unsigned char value);
void func_80018630(unsigned char level, unsigned char id, int ap);
void func_80018834(unsigned char level, unsigned char id, int ap);
void func_80018934(unsigned char level, unsigned char id, int ap);
void func_80018A04(unsigned char level, unsigned char id, int ap);
void func_80018AB0(unsigned char level);
void func_80018B14(unsigned char level);
void func_80018BB8(unsigned char level, unsigned char id, int ap);
void func_80018C94(unsigned char level, unsigned int paired);
void func_80018D4C(unsigned int paired);
void support_elemental(unsigned char level, unsigned int paired, unsigned char armor);
void support_added_effect(unsigned char level, unsigned int paired, unsigned char armor);
void func_800191A0(unsigned char support, unsigned char level, unsigned int paired);
void func_80019440(unsigned char support, unsigned char level, unsigned int paired);
void func_80018ECC(unsigned char id, int ap);
void func_80018FC0(void);
void func_800190E8(unsigned char id, int ap);
void func_80019064(void);
void func_80018E18(unsigned char id, int ap);
void func_80018E90(void);
void func_80019254(unsigned char level, unsigned char id, int ap);
void func_80019338(unsigned char level);
void func_8001937C(unsigned char level, unsigned char id);
void func_800193F4(unsigned char level);
void func_800194BC(unsigned char level);
void func_80019544(unsigned char level, unsigned char id, int ap);
void func_8001BB30(int a, int b);
int command_find(unsigned char id);
int spell_find(unsigned char id);
extern unsigned short D_800491B0[]; /* a bit for each of 32 elements */

/* MP Turbo's level in a flags byte goes up by the support's level, to 5 at most. */
#define TURBO_RAISE(f, level)                                                                                    \
    {                                                                                                            \
        unsigned char x_ = (f);                                                                                  \
        int n_ = (x_ >> 5) + (level);                                                                            \
                                                                                                                 \
        if (n_ > 5)                                                                                              \
            n_ = 5;                                                                                              \
        (f) = (x_ & 0x1F) | (n_ & 7) << 5;                                                                       \
    }

/* The game writes this out wherever a support effect goes on the member's list. Both macros are bare blocks: in
 * do-while (0) they don't match. */
#define SUPPORT_ADD(v, level)                                                                                    \
    {                                                                                                            \
        cur_member->support[support_count].kind = support_kind + support_sub;                                    \
        cur_member->support[support_count].value = (v);                                                          \
        cur_member->support[support_count++].param = support_params[(level) - 1];                                \
    }

/* Works the member out again: the materia's own effects, then each linked pair's, then the stats, HP and MP. */
#ifdef NON_MATCHING
/* Same code, but the game copies armor_slots' result after setting up party_member's argument, not before. */
void member_update(unsigned char slot)
{
    int i;
    unsigned char *pairs;
    unsigned int *m;
    unsigned int m1, m2;
    unsigned char a, b;

    if (D_8009CBDC[slot] == 0xFF)
        return;
    pairs = armor_slots(slot);
    func_8001BDB0(slot, cur_member = party_member(slot));
    materia_detail = 0;
    m = D_8009C738[character_record[D_8009CBDC[slot]]].weapon_materia;
    for (i = 0; i < 8; i++)
        member_materia[i] = *m++;
    m = D_8009C738[character_record[D_8009CBDC[slot]]].armor_materia;
    for (i = 8; i < 16; i++)
        member_materia[i] = *m++;
    for (i = 0; i < 16; i++)
        materia_own(member_materia[i]);
    m = member_materia;
    for (i = 0; i < 4; i++) {
        m1 = *m++;
        m2 = *m++;
        materia_pair(cur_member->weapon_pairs[i * 2], cur_member->weapon_pairs[i * 2 + 1], m1, m2, 0, 0, 0);
    }
    m = &member_materia[8];
    for (i = 0; i < 4; i++) {
        a = *pairs++;
        b = *pairs++;
        m1 = *m++;
        m2 = *m++;
        materia_pair(a, b, m1, m2, 0, 0, 1);
    }
    func_8001BC18();
    func_8001B944();
    func_8001B570();
    func_8001AEE4();
    D_80069548[0] = cur_member->stat_bonus[0] + D_80069548[0];
    D_80069548[1] = cur_member->stat_bonus[1] + D_80069548[1];
    D_80069548[2] = cur_member->stat_bonus[2] + D_80069548[2];
    D_80069548[3] = cur_member->stat_bonus[3] + D_80069548[3];
    if (D_80069548[0] > 255)
        D_80069548[0] = 255;
    if (D_80069548[1] > 255)
        D_80069548[1] = 255;
    if (D_80069548[2] > 255)
        D_80069548[2] = 255;
    if (D_80069548[3] > 255)
        D_80069548[3] = 255;
    if (D_80069548[0] < 0)
        D_80069548[0] = 0;
    if (D_80069548[1] < 0)
        D_80069548[1] = 0;
    if (D_80069548[2] < 0)
        D_80069548[2] = 0;
    if (D_80069548[3] < 0)
        D_80069548[3] = 0;
    cur_member->stats[0] = D_80069548[0];
    cur_member->stats[1] = D_80069548[1];
    cur_member->stats[2] = D_80069548[2];
    cur_member->stats[3] = D_80069548[3];
    func_8001AE08();
    for (i = 0; i < 16; i++)
        func_80019690(member_materia[i]);
    func_8001C0EC(slot);
    for (i = 0; i < 72; i++) {
        int n;

        if (!(cur_member->spells[i].flags & 0xE0))
            continue;
        n = cur_member->spells[i].mp;
        n += n * ((cur_member->spells[i].flags & 0xE0) >> 5) / 10 + 1;
        if (n > 255)
            n = 255;
        cur_member->spells[i].mp = n;
    }
    if (cur_member->flags23 & 8) {
        i = cur_member->max_hp;
        cur_member->max_hp = cur_member->max_mp;
        cur_member->max_mp = i;
    }
    if (cur_member->max_hp < 10)
        cur_member->max_hp = 10;
    if (cur_member->max_mp < 10)
        cur_member->max_mp = 10;
    if (cur_member->max_hp < cur_member->hp) {
        cur_member->hp = cur_member->max_hp;
        D_8009C738[character_record[D_8009CBDC[slot]]].hp = cur_member->hp;
    }
    if (cur_member->max_mp < cur_member->mp) {
        cur_member->mp = cur_member->max_mp;
        D_8009C738[character_record[D_8009CBDC[slot]]].mp = cur_member->mp;
    }
    D_8009C738[character_record[D_8009CBDC[slot]]].max_hp = cur_member->max_hp;
    D_8009C738[character_record[D_8009CBDC[slot]]].max_mp = cur_member->max_mp;
}
#else
INCLUDE_ASM("asm/nonmatchings/main/materia", member_update);
#endif

/* A pair of linked slots: slot codes 2 and 3, or 6 and 7. */
void materia_pair(unsigned char a, unsigned char b, unsigned int m1, unsigned int m2, unsigned char x, unsigned char y,
                  unsigned char armor)
{
    if (a == 2 && b == 3)
        pair_support(m1, m2, x, y, armor);
    if (a == 6 && b == 7)
        pair_support(m1, m2, x, y, armor);
}

/* Applies a support materia to the materia it's paired with, whichever side it's on. */
void pair_support(unsigned int m1, unsigned int m2, unsigned char x, unsigned char y, unsigned char armor)
{
    unsigned char type[2];
    unsigned char id[2];

    id[0] = m1;
    type[0] = materia_data[id[0]].type & 0xF;
    id[1] = m2;
    type[1] = materia_data[id[1]].type & 0xF;
    if (id[0] != 0xFF && id[1] != 0xFF && (type[0] == 5 || type[1] == 5) && !(type[0] == 5 && type[1] == 5)) {
        if (type[0] == 5)
            support_apply(m1, m2, y, armor);
        else
            support_apply(m2, m1, x, armor);
    }
}

#ifdef NON_MATCHING
/* Same code, but the game copies the support materia through one more register at the start. */
void support_apply(unsigned int support, unsigned int paired, unsigned char x, unsigned char armor)
{
    int i;
    unsigned char level = materia_level(support, support >> 8);

    for (i = 0; i < 5; i++)
        support_params[i] = materia_data[(unsigned char)support].attrs[i + 1];
    switch (materia_data[(unsigned char)support].attrs[0]) {
    case 0x63:
        func_800191A0(support, level, paired);
        break;
    case 0x51:
        func_80019440(support, level, paired);
        break;
    case 0x59:
        support_spread = 1;
        func_80018D4C(paired);
        break;
    case 0x5A:
        support_spread = 2;
        func_80018D4C(paired);
        break;
    case 0x58:
        func_80018C94(level, paired);
        break;
    case 0x5D:
        support_spread = 8;
        func_80018D4C(paired);
        break;
    case 0x5C:
        support_spread = 0x10;
        func_80018D4C(paired);
        break;
    case 0x57:
        support_kind = 1;
        func_800184C0(support, level, paired);
        break;
    case 0x56:
        support_kind = 4;
        func_800184C0(support, level, paired);
        break;
    case 0x55:
        support_kind = 7;
        func_8001840C(support, level, paired);
        break;
    case 0x54:
        support_kind = 8;
        func_80018390(support, level, paired);
        break;
    case 0x5E:
        support_elemental(level, paired, armor);
        break;
    case 0x5F:
        support_added_effect(level, paired, armor);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/main/materia", support_apply);
#endif

/* Elemental: the weapon attacks with the paired materia's element; the armor resists it, more with each level. */
void support_elemental(unsigned char level, unsigned int paired, unsigned char armor)
{
    unsigned short bit;
    unsigned char element = materia_data[(unsigned char)paired].element;

    if (element >= 32)
        return;
    bit = D_800491B0[element];
    if (!armor) {
        cur_member->elements[0] = bit | cur_member->elements[0];
        return;
    }
    switch (level) {
    case 1:
        cur_member->elements[1] = bit | cur_member->elements[1];
        break;
    case 2:
        cur_member->elements[2] = bit | cur_member->elements[2];
        break;
    case 3:
    default:
        cur_member->elements[3] = bit | cur_member->elements[3];
        break;
    }
}

/* Added Effect: the weapon inflicts the paired materia's statuses; the armor guards against them. */
void support_added_effect(unsigned char level, unsigned int paired, unsigned char armor)
{
    unsigned char id = paired;
    int bits = materia_data[id].status[0] + (materia_data[id].status[1] << 8) + (materia_data[id].status[2] << 16);

    if (!armor)
        cur_member->statuses[0] = bits | cur_member->statuses[0];
    else
        cur_member->statuses[1] = bits | cur_member->statuses[1];
}

void func_80018390(unsigned char support, unsigned char level, unsigned int paired)
{
    unsigned char id = paired;
    int ap = paired >> 8;
    int type = materia_data[id].type;

    support_sub = 0;
    switch (type & 0xF) {
    case 6:
        func_80018630(level, id, ap);
        break;
    case 8:
        support_add(level, 0xFD);
        break;
    }
}

void func_8001840C(unsigned char support, unsigned char level, unsigned int paired)
{
    unsigned char id = paired;
    int ap = paired >> 8;
    int type = materia_data[id].type;

    support_sub = 0;
    switch (type & 0xF) {
    case 9:
        func_80018834(level, id, ap);
        break;
    case 0xA:
        support_add(level, 0xFE);
        break;
    case 0xB:
        func_80018934(level, id, ap);
        break;
    case 0xC:
        support_add(level, 0xFF);
        break;
    }
}

void func_800184C0(unsigned char support, unsigned char level, unsigned int paired)
{
    unsigned char id = paired;
    int ap = paired >> 8;
    int type = materia_data[id].type;

    support_sub = 0;
    switch (type & 0xF) {
    case 6:
        support_sub = 1;
        func_80018630(level, id, ap);
        break;
    case 8:
        support_sub = 1;
        support_add(level, 0xFD);
        break;
    case 9:
        func_80018834(level, id, ap);
        break;
    case 0xA:
        support_add(level, 0xFE);
        break;
    case 0xB:
        func_80018934(level, id, ap);
        break;
    case 0xC:
        support_add(level, 0xFF);
        break;
    }
}

void support_add(unsigned char level, unsigned char value)
{
    if (support_count != 8)
        SUPPORT_ADD(value, level);
}

void func_80018630(unsigned char level, unsigned char id, int ap)
{
    unsigned char t;

    materia_level(id, ap);
    t = materia_data[id].attrs[0];
    switch (t) {
    case 5:
    case 6:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        if (command_find(t) != -1 && support_count != 8)
            SUPPORT_ADD(materia_data[id].attrs[0], level);
    }
    t = materia_data[id].attrs[1];
    if ((t == 7 || t == 0x11) && command_find(t) != -1 && support_count != 8)
        SUPPORT_ADD(materia_data[id].attrs[1], level);
}

void func_80018834(unsigned char level, unsigned char id, int ap)
{
    int i;

    for (i = materia_level(id, ap); i > 0; i--) {
        if (materia_data[id].attrs[i - 1] == 0xFF)
            continue;
        if (support_count != 8)
            SUPPORT_ADD(materia_data[id].attrs[i - 1], level);
        break;
    }
}

void func_80018934(unsigned char level, unsigned char id, int ap)
{
    materia_level(id, ap);
    if (support_count != 8)
        SUPPORT_ADD(materia_data[id].attrs[0], level);
}

/* MP Turbo on a magic materia: each of its spells (up to its level) costs more and does more, five steps at most. */
void func_80018A04(unsigned char level, unsigned char id, int ap)
{
    int i;

    for (i = materia_level(id, ap); i > 0; i--)
        TURBO_RAISE(cur_member->spells[materia_data[id].attrs[i - 1]].flags, level);
}

/* ...on a summon materia: every summon. */
void func_80018AB0(unsigned char level)
{
    int i;

    for (i = 0; i < 16; i++)
        TURBO_RAISE(cur_member->spells[56 + i].flags, level);
}

void func_80018B14(unsigned char level)
{
    int i, k;

    for (i = 0; i < 56; i++) {
        k = spell_find(i);
        if (k == -1)
            continue;
        TURBO_RAISE(spell_slots[k].flags, level);
    }
}

void func_80018BB8(unsigned char level, unsigned char id, int ap)
{
    int i, k;

    for (i = materia_level(id, ap); i > 0; i--) {
        k = spell_find(materia_data[id].attrs[i - 1]);
        if (k == -1)
            continue;
        TURBO_RAISE(spell_slots[k].flags, level);
    }
}

void func_80018C94(unsigned char level, unsigned int paired)
{
    unsigned char id = paired;
    int ap = paired >> 8;

    switch (materia_data[id].type & 0xF) {
    case 9:
        func_80018BB8(level, id, ap);
        break;
    case 0xA:
        func_80018B14(level);
        break;
    case 0xB:
        func_80018A04(level, id, ap);
        break;
    case 0xC:
        func_80018AB0(level);
        break;
    }
}

void func_80018D4C(unsigned int paired)
{
    unsigned char id = paired;
    int ap = paired >> 8;

    switch (materia_data[id].type & 0xF) {
    case 6:
        func_80018ECC(id, ap);
        break;
    case 8:
        func_80018FC0();
        break;
    case 9:
        func_800190E8(id, ap);
        break;
    case 0xA:
        func_80019064();
        break;
    case 0xB:
        func_80018E18(id, ap);
        break;
    case 0xC:
        func_80018E90();
        break;
    }
}

/* The spread bits on each spell of a magic materia, up to its level. */
void func_80018E18(unsigned char id, int ap)
{
    int i;

    for (i = materia_level(id, ap); i > 0; i--)
        cur_member->spells[materia_data[id].attrs[i - 1]].flags |= support_spread;
}

/* ...on every summon. */
void func_80018E90(void)
{
    int i;

    for (i = 0; i < 16; i++)
        cur_member->spells[56 + i].flags |= support_spread;
}

void func_80018ECC(unsigned char id, int ap)
{
    int k;
    unsigned char t;

    k = command_find(materia_data[id].attrs[0]);
    if (k != -1)
        command_slots[k].flags |= support_spread;
    t = materia_data[id].attrs[1];
    if (t == 7 || t == 0x11) {
        k = command_find(t);
        if (k != -1)
            command_slots[k].flags |= support_spread;
    }
}

void func_80018FC0(void)
{
    int i, k;

    for (i = 0; i < 16; i++) {
        k = command_find(command_slots[i].id);
        if (k != -1)
            command_slots[k].flags |= support_spread;
    }
}

void func_80019064(void)
{
    int i, k;

    for (i = 0; i < 56; i++) {
        k = spell_find(i);
        if (k != -1)
            spell_slots[k].flags |= support_spread;
    }
}

void func_800190E8(unsigned char id, int ap)
{
    int i, k;

    for (i = materia_level(id, ap); i > 0; i--) {
        k = spell_find(materia_data[id].attrs[i - 1]);
        if (k != -1)
            spell_slots[k].flags |= support_spread;
    }
}

/* Quadra Magic. */
void func_800191A0(unsigned char support, unsigned char level, unsigned int paired)
{
    unsigned char id = paired;
    int ap = paired >> 8;

    switch (materia_data[id].type & 0xF) {
    case 9:
        func_80019254(level, id, ap);
        break;
    case 0xA:
        func_80019338(level);
        break;
    case 0xB:
        func_8001937C(level, id);
        break;
    case 0xC:
        func_800193F4(level);
        break;
    }
}

void func_80019254(unsigned char level, unsigned char id, int ap)
{
    int i, k;

    for (i = materia_level(id, ap); i > 0; i--) {
        k = spell_find(materia_data[id].attrs[i - 1]);
        if (k != -1) {
            spell_slots[k].quadra++;
            spell_slots[k].quadra_uses += level;
        }
    }
}

void func_80019338(unsigned char level)
{
    int i;

    for (i = 0; i < 56; i++) {
        spell_slots[i].quadra += 1;
        spell_slots[i].quadra_uses = spell_slots[i].quadra_uses + level;
    }
}

void func_8001937C(unsigned char level, unsigned char id)
{
    int k = materia_data[id].attrs[0] - 56;

    summon_quadra[k]++;
    summon_quadra_uses[k] += level;
}

void func_800193F4(unsigned char level)
{
    int i;

    for (i = 0; i < 16; i++) {
        summon_quadra[i]++;
        summon_quadra_uses[i] += level;
    }
}

/* All. */
void func_80019440(unsigned char support, unsigned char level, unsigned int paired)
{
    unsigned char id = paired;
    int ap = paired >> 8;

    switch (materia_data[id].type & 0xF) {
    case 9:
        func_80019544(level, id, ap);
        break;
    case 0xA:
        func_800194BC(level);
        break;
    }
}

void func_800194BC(unsigned char level)
{
    int i, k;

    for (i = 0; i < 56; i++) {
        k = spell_find(i);
        if (k != -1)
            spell_slots[k].all += level;
    }
}

void func_80019544(unsigned char level, unsigned char id, int ap)
{
    int i, k;

    for (i = materia_level(id, ap); i > 0; i--) {
        k = spell_find(materia_data[id].attrs[i - 1]);
        if (k != -1)
            spell_slots[k].all += level;
    }
}

int command_find(unsigned char id)
{
    int i;

    for (i = 0; i < 16; i++) {
        if (command_slots[i].id == id)
            return i;
    }
    return -1;
}

int spell_find(unsigned char id)
{
    int i;

    for (i = 0; i < 56; i++) {
        if (spell_slots[i].id == id)
            return i;
    }
    return -1;
}

/* A materia of kind 4 adds its level to every spell's and some commands' uses. */
void func_80019690(unsigned int materia)
{
    int i, level;
    unsigned char id = materia;
    int ap = materia >> 8;

    if ((materia_data[id].type & 0xF) != 4)
        return;
    level = materia_level(id, ap);
    for (i = 0; i < 56; i++) {
        if (cur_member->spells[i].id != 0xFF)
            cur_member->spells[i].uses += level;
    }
    for (i = 0; i < 16; i++) {
        if (cur_member->commands[i].id == 0xFF)
            continue;
        switch (cur_member->commands[i].id) {
        case 5:
        case 6:
        case 9:
        case 0xA:
        case 0xB:
        case 0x11:
            cur_member->commands[i].uses += level;
            break;
        }
    }
    if (cur_member->commands[0].id != 0x1A)
        func_8001BB30(0x18, 0);
}
