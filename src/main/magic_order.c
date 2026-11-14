/* The Config menu's Magic order, 0x8001C0EC-0x8001C3CC. GCC 2.7.2, ASPSX 2.34 and -G8, unlike the materia files
 * before it. */

#include "common.h"
#include "main.h"

extern PartyMember members[3];
extern unsigned char magic_places[];  /* each spell's place: its row's kind in bits 5-7, its place in the row below */
extern unsigned char magic_rows[][4]; /* by Magic order, each kind's row */

/* Re-sorts a member's 56 spells by kind, the setting's three kinds in its order and then the rest, three to a menu
 * line; a line with none of its spells is left out. Spell 25 and summon 15 then lose their Quadra Magic uses. */
void func_8001C0EC(int slot)
{
    unsigned char rows[4][24];
    unsigned char have[56];
    unsigned char where[56];
    MemberSpell saved[56];
    MemberSpell *spells = members[slot].spells;
    int order = (config >> 10) & 7;
    int i, k, m, n;

    for (i = 0; i < 56; i++) {
        saved[i] = spells[i];
        if (spells[i].id != 0xFF)
            where[spells[i].id] = i; /* where each spell was */
        spells[i].id = 0xFF;
    }
    for (i = 0; i < 4; i++)
        for (k = 0; k < 24; k++)
            rows[i][k] = 0xFF;
    for (i = 0; i < 54; i++)
        rows[magic_rows[order][magic_places[i] >> 5]][magic_places[i] & 0x1F] = i;
    for (i = 0; i < 56; i++)
        have[i] = 0xFF;
    for (i = 0; i < 56; i++)
        if (saved[i].id != 0xFF)
            have[saved[i].id] = saved[i].id; /* spells the member doesn't have stay 0xFF */
    for (i = 0; i < 4; i++)
        for (k = 0; k < 24; k++)
            if (rows[i][k] != 0xFF)
                rows[i][k] = have[rows[i][k]];
    n = 0;
    for (i = 0; i < 4; i++)
        for (k = 0; k < 24; k += 3) {
            unsigned char *line = &rows[i][k];
            if ((line[0] & line[1] & line[2]) == 0xFF)
                continue;
            for (m = 0; m < 3; m++) {
                if (line[m] != 0xFF)
                    spells[n] = saved[where[line[m]]];
                n++;
            }
        }
    for (i = 0; i < 72; i++) {
        int id = spells[i].id;
        if (id == 0xFF)
            continue;
        if (i >= 56)
            id += 56;
        if (id == 25 || id == 71)
            spells[i].quadra_uses = 0;
    }
}

void func_8001C3C4(void)
{
}
