/* The materia's AP, 0x80017238-0x8001786C: AP from a battle into each member's weapon and armor materia, and the
 * party's support materia sums the field reads. -G8 like text.c, but assembled by ASPSX 2.21 (indexed loads go
 * through `addiu $at`). */

#include "common.h"
#include "main.h"

PartyMember *cur_member;      /* the member being worked on */
unsigned char materia_detail; /* nonzero: materia_level also writes the level, levels and AP to next */
int materia_levels;           /* how many levels the materia materia_level last read has */
unsigned char D_80062E5C;
extern short materia_levelled[]; /* each member's materia that levelled up: weapon slots bits 0-7, armor 8-15 */
/* Sized, so GCC takes its address for a cheap gp one and writes each byte through its own lui, as the game does;
 * the save byte is unsized, so its address stays in a register. */
extern unsigned char party_effects[4];
extern unsigned char D_8009D302[];

/* The armor's record (0x24 bytes). */
typedef struct {
    unsigned char pad0[0x11];
    unsigned char growth; /* as PartyMember.weapon_growth */
} Armor;

int materia_level(unsigned char id, int ap);
void materia_add(unsigned int materia);
Armor *armor_data(int id);
PartyMember *party_member(int slot);

int materia_split(unsigned int materia, int *ap, unsigned char *id)
{
    *id = materia;
    *ap = materia >> 8;
    return materia_level(*id, *ap);
}

/* The slot's growth multiplies the AP; empty slots, materia 0x2C and mastered materia get none. A materia reaching its
 * last level is mastered and puts a new copy in the inventory. */
#ifdef NON_MATCHING
/* Same code, but GCC keeps &materia_levelled[slot] in a register through both loops where the game works it out each
 * time, and so spills the slot instead of the AP. An unused stack local anywhere in the function gives the game's
 * registers, with the frame 8 bytes too big. */
void materia_gain_ap(short slot, unsigned short ap)
{
    unsigned int *m;
    int i, level, new_level;
    int m_ap;
    unsigned char id;
    Armor *armor;

    materia_detail = 0;
    materia_levelled[slot] = 0;
    if (D_8009CBDC[slot] == 0xFF)
        return;
    cur_member = party_member(slot);
    m = D_8009C738[character_record[D_8009CBDC[slot]]].weapon_materia;
    for (i = 0; i < 8; i++, m++) {
        level = materia_split(*m, &m_ap, &id);
        if (id == 0xFF || id == 0x2C || m_ap == 0xFFFFFF)
            continue;
        switch (cur_member->weapon_growth) {
        case 0:
            break;
        case 1:
        default:
            m_ap += ap;
            break;
        case 2:
            m_ap += ap * 2;
            break;
        case 3:
            m_ap += ap * 3;
            break;
        }
        *m = (m_ap & 0xFFFFFF) << 8 | id;
        new_level = materia_split(*m, &m_ap, &id);
        if (level < new_level) {
            materia_levelled[slot] |= 1 << i;
            if (new_level == materia_levels) {
                materia_add(id);
                *m = id | 0xFFFFFF00;
            }
        }
    }
    armor = armor_data(D_8009C738[character_record[D_8009CBDC[slot]]].armor);
    m = D_8009C738[character_record[D_8009CBDC[slot]]].armor_materia;
    for (i = 0; i < 8; i++, m++) {
        level = materia_split(*m, &m_ap, &id);
        if (id == 0xFF || id == 0x2C || m_ap == 0xFFFFFF)
            continue;
        switch (armor->growth) {
        case 0:
            break;
        case 1:
        default:
            m_ap += ap;
            break;
        case 2:
            m_ap += ap * 2;
            break;
        case 3:
            m_ap += ap * 3;
            break;
        }
        *m = (m_ap & 0xFFFFFF) << 8 | id;
        new_level = materia_split(*m, &m_ap, &id);
        if (level < new_level) {
            materia_levelled[slot] |= 1 << (i + 8);
            if (new_level == materia_levels) {
                materia_add(id);
                *m = id | 0xFFFFFF00;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/main/materia_ap", materia_gain_ap);
#endif

/* Each of the four starts at 16 (the third at 0) and takes the members' sums, the second less encounter_less (2 if that
 * goes below 0), capped at 32 (the last at 0x55). */
void party_effects_sum(void)
{
    int i, add1, less1, n;

    add1 = 0;
    less1 = 0;
    party_effects[0] = 16;
    party_effects[1] = 16;
    party_effects[2] = 0;
    party_effects[3] = 16;
    D_8009D302[0] &= 0x7F;
    for (i = 0; i < 3; i++) {
        if (D_8009CBDC[i] == 0xFF)
            continue;
        cur_member = party_member(i);
        party_effects[0] += cur_member->effects[0];
        add1 += cur_member->effects[1];
        less1 += cur_member->encounter_less;
        party_effects[2] += cur_member->effects[2];
        party_effects[3] += cur_member->effects[3];
        if (cur_member->flags23 & 1)
            D_8009D302[0] |= 0x80;
    }
    n = add1 + 16;
    party_effects[1] = n < less1 ? 2 : n - less1;
    if (party_effects[0] > 32)
        party_effects[0] = 32;
    if (party_effects[1] > 32)
        party_effects[1] = 32;
    if (party_effects[2] > 32)
        party_effects[2] = 32;
    if (party_effects[3] > 0x55)
        party_effects[3] = 0x55;
    if (D_80062E5C)
        party_effects[3] |= 0x80;
}
