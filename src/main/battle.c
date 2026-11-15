/* The battle's helpers in the main exe, 0x800155A4-0x80015B44: its "Data Error" halt, the results screen's EXP count,
 * the spoils into the inventory, and a character's limits. -G8 and ASPSX 2.34 like text.c; the one division is
 * assembled with its divide-by-zero check (--expand-div). */

#include "common.h"
#include "main.h"

int battle_error;  /* the "Data Error" code, -1 for none; 0x100 on once the battle has halted for it */
int D_80062F60;
int tally_frames;  /* frames the EXP count has run */
int tally_done;    /* a bit for each member whose count has finished */
int tally_hidden;  /* a bit for each member whose level isn't shown */

extern volatile unsigned short menu_pad1;  /* pad.c's; its first field is the buttons held */
extern unsigned char D_8009CBDC[];         /* the party: three characters, 0xFF for an empty place */

/* The battle's, at 0x800F83A4. */
typedef struct {
    unsigned char pad0[0x22];
    unsigned short flags;
} BattleState;
extern BattleState D_800F83A4;

void func_800A3278(void);
void func_800B6D6C(void);

/* What the results screen counts from and to, set up before it starts. */
typedef struct {
    unsigned int exp[4];       /* as counted so far */
    unsigned int exp_end[4];   /* with the battle's EXP */
    unsigned int levels[4][12]; /* the EXP of each level from the one the member had */
    unsigned char frac[4];     /* the count's fractions */
    unsigned char level[4];    /* the level the member had */
} ExpTally;
extern ExpTally exp_tally;

extern struct {
    unsigned short items[320]; /* id in the low 9 bits, count above, 0xFFFF for none */
    int materia[256];          /* -1 for none */
} inventory;

/* KERNEL.BIN section 2 begins with a record for each character. */
typedef struct {
    unsigned char pad0[12];
    unsigned char limits[12]; /* limit ids, 0x80 on; 0xFF for none */
    unsigned char pad18[32];
} KernelCharacter;
extern KernelCharacter kernel_characters[];

void battle_set_error(int code)
{
    battle_error = code;
}

/* Halts the battle on an error, once, until the buttons 0x10C are held together. */
void battle_error_halt(void)
{
    if (battle_error < 0 || D_800F83A4.flags & 8)
        return;
    battle_error |= 0x100;
    D_80062F60 = 0;
    while (menu_pad1 != 0x10C) {
        func_800A3278();
        func_800B6D6C();
    }
    D_800F83A4.flags |= 8;
}

void exp_tally_start(int hidden)
{
    tally_frames = 0;
    tally_done = 0;
    tally_hidden = hidden;
}

/* A frame of the results screen's EXP count (all of it at once with skip); nonzero while a member is still counting.
 * Each frame adds a 256th of the member's current level to its EXP. */
#ifdef NON_MATCHING
/* Same code, but the original sets up its pointers into exp_tally in another order (from levels, not the start). */
int exp_tally_step(int skip)
{
    int m, i, n, id;
    unsigned int prev, next, cur;

    if (skip)
        tally_done |= 7;
    for (m = 0; m < 3; m++) {
        id = D_8009CBDC[m];
        if (id == 0xFF) {
            tally_done |= 1 << m;
            continue;
        }
        for (i = 0; i < 9; i++) {
            if (D_8009C738[i].id != id)
                continue;
            prev = exp_tally.levels[m][0];
            next = exp_tally.levels[m][1];
            cur = exp_tally.exp[m];
            for (n = 1; n < 11;) {
                if (cur < next)
                    break;
                prev = next;
                next = exp_tally.levels[m][++n];
            }
            if (n == 11)
                exp_tally.exp[m] = exp_tally.exp_end[m];
            if (tally_frames) {
                if (exp_tally.exp_end[m] > exp_tally.exp[m]) {
                    if (exp_tally.exp[m] < next) {
                        unsigned int step = exp_tally.frac[m] + (next - prev);

                        exp_tally.exp[m] += step >> 8;
                        exp_tally.frac[m] = step;
                        if (exp_tally.exp[m] < next)
                            goto shown;
                    }
                    exp_tally.exp[m] = next;
                    exp_tally.frac[m] = 0;
                } else {
                    tally_done |= 1 << m;
                    exp_tally.exp[m] = exp_tally.exp_end[m];
                }
            }
        shown:
            if (tally_done >> m & 1 || exp_tally.level[m] + n - 1 >= 99) {
                tally_shown[m].level = D_8009C738[i].level;
                tally_shown[m].exp = D_8009C738[i].exp;
                tally_shown[m].to_next = D_8009C738[i].exp_to_next;
                tally_shown[m].bar = D_8009C738[i].exp_bar;
            } else {
                tally_shown[m].level = exp_tally.level[m] + n - 1;
                tally_shown[m].exp = exp_tally.exp[m];
                tally_shown[m].to_next = next - exp_tally.exp[m];
                tally_shown[m].bar = 0;
                if (next != prev)
                    tally_shown[m].bar = (exp_tally.exp[m] - prev) * 61 / (next - prev);
            }
            break;
        }
        if (tally_hidden >> m & 1)
            tally_shown[m].level = 0;
    }
    tally_frames++;
    return tally_done != 7;
}
#else
INCLUDE_ASM("asm/nonmatchings/main/battle", exp_tally_step);
#endif

/* Items join a stack of their own (99 at most) or the first empty place; materia takes an empty place each. */
void add_spoils(void)
{
    int k, i, count, free;
    short id;

    for (k = 0; k < 4; k++) {
        if (!spoils[k].won)
            continue;
        id = spoils[k].id;
        if (id == -1)
            continue;
        count = spoils[k].count;
        free = -1;
        if (id < 0x180) {
            for (i = 0; i < 320; i++) {
                if ((inventory.items[i] & 0x1FF) == id) {
                    count += inventory.items[i] >> 9;
                    if (count >= 100)
                        count = 99;
                    inventory.items[i] = id | count << 9;
                    break;
                }
                if (inventory.items[i] == 0xFFFF && free == -1)
                    free = i;
            }
            if (i == 320 && free != -1)
                inventory.items[free] = id | count << 9;
        } else {
            for (i = 0; i < 256; i++) {
                if (inventory.materia[i] == -1) {
                    inventory.materia[i] = id - 0x180;
                    if (--count == 0)
                        break;
                }
            }
        }
    }
}

/* The limit in a character's slot, less 0x80 (0x7F for none, or for no such character). */
#ifdef NON_MATCHING
/* Same code but for one add's operand order (slot + row in the original), and a 48-byte frame it doesn't use. */
int limit_id(unsigned int character, int slot)
{
    int id = 0x7F;

    if (character < 9)
        id = kernel_characters[character].limits[slot] - 0x80;
    return id;
}
#else
INCLUDE_ASM("asm/nonmatchings/main/battle", limit_id);
#endif
