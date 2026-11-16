/* BGINMENU.MNU, 0x801D0000-0x801D07F0 (its data follows to 0x801D0898): module 12, which nothing in the exe opens. Its
 * screen is CHNGMENU's, title and all; after it come materia helpers that nothing here calls: count a character's
 * materia, check for or take away a materia, and trade a mastered set (all the magic, all the summons or seven
 * commands) for its master materia, or Bahamut and Neo Bahamut for Bahamut ZERO. GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "menu.h"

extern int bgin_active; /* the cursor in use; -1 until bgin_init */
extern RECT bgin_windows[2];
extern unsigned char bgin_title[];
extern unsigned char bgin_magic_set[];
extern unsigned char bgin_summon_set[];
extern unsigned char bgin_command_set[];
extern unsigned char bgin_bahamut_set[];
extern MenuCursor bgin_cursors[2];

void bgin_init(void)
{
    RECT unused; /* unused, but the game's frame has it */

    func_80026448(&bgin_cursors[0], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
    func_80026448(&bgin_cursors[1], 0, 0, 1, 3, 0, 0, 1, 9, 0, 0, 0, 0, 0);
    bgin_active = 0;
}

void bgin_frame(void)
{
    RECT unused[2]; /* unused, but the game's frame has them */
    int i;

    func_800230C4(results_buf);
    if (!bgin_active)
        func_8001EB2C(0, bgin_windows[1].y + bgin_cursors[0].row * 64 + 32);
    func_80026F44(16, 11, bgin_title, 7);
    for (i = 0; i < 2; i++)
        func_8001E040(&bgin_windows[i]);
    func_800264A8(&bgin_cursors[bgin_active]);
    if (D_80062D7E & 0x40) {
        func_8002305C(5, 0);
        func_8002120C(0);
    }
}

void bgin_801D01BC(void)
{
}

int bgin_materia_count(int c)
{
    int i;
    int count;

    for (i = 0, count = 0; i < 8; i++) {
        if (save_data.chars[c].weapon_materia[i] != -1)
            count++;
    }
    for (i = 0; i < 8; i++) {
        if (save_data.chars[c].armor_materia[i] != -1)
            count++;
    }
    return count;
}

int bgin_read16(unsigned char *p)
{
    return p[0] | p[1] << 8;
}

void bgin_write16(unsigned char *p, unsigned short value)
{
    p[0] = value;
    p[1] = value >> 8;
}

/* The first five characters' HP times a field variable's fraction of 0xFFFF, at least 1. */
void bgin_scale_hp(void)
{
    int i;
    int hp;

    for (i = 0; i < 5; i++) {
        hp = save_data.chars[i].hp * bgin_read16(&save_data.hp_scale[i * 2]) / 0xFFFF;
        if (hp <= 0)
            hp = 1;
        save_data.chars[i].hp = hp;
    }
}

/* Takes one mastered `id` away: from a joined character's weapon or armor, or else the stock. */
void bgin_take_mastered(int id)
{
    int i, j;

    id |= 0xFFFFFF00;
    for (i = 0; i < 9; i++) {
        if (!(save_data.joined >> i & 1))
            continue;
        for (j = 0; j < 8; j++) {
            if (save_data.chars[i].weapon_materia[j] == id) {
                save_data.chars[i].weapon_materia[j] = -1;
                return;
            }
        }
        for (j = 0; j < 8; j++) {
            if (save_data.chars[i].armor_materia[j] == id) {
                save_data.chars[i].armor_materia[j] = -1;
                return;
            }
        }
    }
    for (j = 0; j < 200; j++) {
        if (save_data.materia[j] == id) {
            save_data.materia[j] = -1;
            return;
        }
    }
}

/* Whether a joined character or the stock has `id` mastered. */
int bgin_has_mastered(int id)
{
    unsigned int m, ap;
    int i, j;

    for (i = 0; i < 9; i++) {
        if (!(save_data.joined >> i & 1))
            continue;
        for (j = 0; j < 8; j++) {
            m = save_data.chars[i].weapon_materia[j];
            ap = m >> 8;
            if (ap == 0xFFFFFF && (m & 0xFF) == id)
                return 1;
        }
        for (j = 0; j < 8; j++) {
            m = save_data.chars[i].armor_materia[j];
            ap = m >> 8;
            if (ap == 0xFFFFFF && (m & 0xFF) == id)
                return 1;
        }
    }
    for (j = 0; j < 200; j++) {
        m = save_data.materia[j];
        ap = m >> 8;
        if (ap == 0xFFFFFF && (m & 0xFF) == id)
            return 1;
    }
    return 0;
}

/* Whether a joined character or the stock has `id` at all. */
int bgin_has_materia(int id)
{
    int i, j;

    for (i = 0; i < 9; i++) {
        if (!(save_data.joined >> i & 1))
            continue;
        for (j = 0; j < 8; j++) {
            if ((save_data.chars[i].weapon_materia[j] & 0xFF) == id)
                return 1;
        }
        for (j = 0; j < 8; j++) {
            if ((save_data.chars[i].armor_materia[j] & 0xFF) == id)
                return 1;
        }
    }
    for (j = 0; j < 200; j++) {
        if ((save_data.materia[j] & 0xFF) == id)
            return 1;
    }
    return 0;
}

/* save_data.result: 1 when the party has a trade's materia (all of `set` mastered, or both Bahamuts for set 3). */
void bgin_check_set(int set)
{
    int i;

    save_data.result = 0;
    switch (set) {
    case 0:
        for (i = 0; i < 21; i++) {
            if (!bgin_has_mastered(bgin_magic_set[i]))
                return;
        }
        break;
    case 1:
        for (i = 0; i < 16; i++) {
            if (!bgin_has_mastered(bgin_summon_set[i]))
                return;
        }
        break;
    case 2:
        for (i = 0; i < 7; i++) {
            if (!bgin_has_mastered(bgin_command_set[i]))
                return;
        }
        break;
    case 3:
        for (i = 0; i < 2; i++) {
            if (!bgin_has_materia(bgin_bahamut_set[i]))
                return;
        }
        break;
    }
    save_data.result = 1;
}

/* Takes a mastered set away for its master materia (Master Magic, Master Summon, Master Command); set 3 keeps the
 * Bahamuts and adds Bahamut ZERO. */
void bgin_trade_set(int set)
{
    int i;

    switch (set) {
    case 0:
        for (i = 0; i < 21; i++)
            bgin_take_mastered(bgin_magic_set[i]);
        func_8002542C(0x49);
        break;
    case 1:
        for (i = 0; i < 16; i++)
            bgin_take_mastered(bgin_summon_set[i]);
        func_8002542C(0x5A);
        break;
    case 2:
        for (i = 0; i < 7; i++)
            bgin_take_mastered(bgin_command_set[i]);
        func_8002542C(0x30);
        break;
    case 3:
        func_8002542C(0x58);
        break;
    }
}
