/* NAMEMENU.MNU, 0x801D0014-0x801D14E8 (a jump table before it; namelevel.c's code, then data to 0x801DC078, after
 * it): the naming screen. The field runs name_run with a character's id (9, or 100 and up, for a chocobo): it fades
 * in, takes up to nine letters (six for a chocobo) from a grid with Space, Delete, Select and Default beside it, writes
 * the name to the save and fades out. Naming anyone but Cloud or a chocobo also brings them to the party's average
 * level, growing their stats as levelling up in battle does, which is how new members catch up. The Japanese
 * version's kana pages and a cheat check (names that fill the stock with items and materia) are still in it; nothing
 * here reaches the check. GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

extern struct {
    unsigned short items[320];
    int materia[256];
} inventory;
extern unsigned char D_8009D488[6][6];  /* the chocobos' names */
extern unsigned short D_8009D78A;       /* the joined characters, bit n for character n */
extern unsigned char D_80077F64[2][0x3400]; /* the menus' primitive buffers */

extern int name_fade_level;
extern unsigned char *name_portraits[10];
extern unsigned char name_cheats[6][16];
extern unsigned char name_defaults[12][12];
extern unsigned char name_buttons[5][8]; /* Space, Delete, Select, Default and Cancel, which isn't drawn */
extern unsigned char name_kana1[90]; /* the grid's pages, rows of ten */
extern unsigned char name_kana2[90];
extern unsigned char name_letters[70];
extern int name_state; /* 0 the grid, 1 the buttons */
extern int name_page;  /* 0 and 1 the kana, 2 the letters */
extern int name_who;
extern Character name_start_chars[12];
extern int name_cheat_magic[21];
extern int name_cheat_summons[16];
extern int name_cheat_commands[7];
extern int name_cheat_bahamuts[2];
extern unsigned int *name_ot;
extern unsigned int name_ots[2];
extern MenuCursor name_cursors[2];
extern unsigned char name_text[16];
extern unsigned char name_length;
extern DRAWENV name_draw[2];
extern DISPENV name_disp[2];
extern unsigned char name_vram[0x2800];
extern int name_buf;
extern int name_phase; /* 0 fading in, 1 running, 2 fading out, -1 done */

void name_catch_up(Character *c); /* namelevel.c */

void name_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

/* FORMMENU's form_fade. */
int name_fade(int step)
{
    RECT tw;

    setlen(prim_next.tile, 3);
    setcode(prim_next.tile, 0x60);
    func_80046848(prim_next.tile, 1);
    prim_next.tile->x0 = 0;
    prim_next.tile->y0 = 0;
    prim_next.tile->w = 0x180;
    prim_next.tile->h = 0xE8;
    prim_next.tile->r0 = name_fade_level;
    prim_next.tile->g0 = name_fade_level;
    prim_next.tile->b0 = name_fade_level;
    AddPrim(prim_ot, prim_next.tile++);
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x5F, &tw);
    name_fade_level += step;
    if (name_fade_level < 0)
        name_fade_level = 0;
    if (name_fade_level >= 0x100)
        name_fade_level = 0xFF;
    return name_fade_level;
}

/* The first cheat: 99 of every item and every materia, ids the game doesn't use made 0x1D. */
void name_cheat_stock(void)
{
    int i, m;

    for (i = 0; i < 320; i++)
        inventory.items[i] = i | 0xC600;
    for (i = 0; i < 256; i++) {
        m = i - i / 91 * 91;
        if (m != 0x2E && m != 0x2F && m != 0x2D && m != 0x26 && m != 0x3F && m != 0x42 && m != 0x43 && m != 0x11)
            inventory.materia[i] = m;
        else
            inventory.materia[i] = 0x1D;
    }
    inventory.materia[0] = 0x35;
    D_8009D78A = 0xFFFF;
}

/* Slots 6 and 7 hold Young Cloud and Sephiroth in the flashback: Cait Sith's and Vincent's records start afresh. */
void name_reset_char(int i)
{
    D_8009C738[i] = name_start_chars[i];
}

/* The default name. */
void name_load(void)
{
    int i;

    for (i = 0, name_length = 0; i < 15; i++) {
        name_text[i] = name_defaults[name_who][i];
        if (name_defaults[name_who][i] == 0xFF) {
            name_text[i + 1] = 0xFF;
            if (i >= 9)
                name_length = 8;
            return;
        }
        name_length++;
    }
}

void name_init(void)
{
    name_state = 0;
    name_page = 2;
    name_cursors[0].col = 0;
    name_cursors[0].row = 0;
    name_cursors[0].cols = 10;
    name_cursors[0].rows = 9;
    name_cursors[0].pad0 = 0;
    name_cursors[0].top = 0;
    name_cursors[0].pad4 = 10;
    name_cursors[0].total = 9;
    name_cursors[0].wrap_x = 0;
    name_cursors[0].wrap_y = 0;
    name_cursors[0].padE = 0;
    name_cursors[0].scroll_step = 0;
    name_cursors[0].scrolling = 0;
    name_cursors[1].col = 0;
    name_cursors[1].row = 0;
    name_cursors[1].cols = 1;
    name_cursors[1].rows = 4;
    name_cursors[1].pad0 = 0;
    name_cursors[1].top = 0;
    name_cursors[1].pad4 = 1;
    name_cursors[1].total = 4;
    name_cursors[1].wrap_x = 0;
    name_cursors[1].wrap_y = 1;
    name_cursors[1].padE = 0;
    name_cursors[1].scroll_step = 0;
    name_cursors[1].scrolling = 0;
    if (name_who == 6)
        name_reset_char(6);
    if (name_who == 7)
        name_reset_char(7);
    if (name_who >= 100)
        name_who = 9;
    func_80025C14(name_vram);
    func_80025D14(name_portraits[name_who], 0x340, 0x100, 0x180, 0);
    name_load();
}

int name_glyph(int page, int col, int row)
{
    int ch;

    switch (page) {
    case 0:
        ch = name_kana1[row * 10 + col];
        break;
    case 1:
        ch = name_kana2[row * 10 + col];
        break;
    case 2:
        ch = name_letters[row * 10 + col];
        break;
    }
    return ch;
}

/* At the last place a letter replaces the one there. */
void name_add(int ch)
{
    if (name_who != 9) {
        if (name_length == 8) {
            name_text[8] = ch;
            name_text[9] = 0xFF;
        } else {
            name_text[name_length++] = ch;
            name_text[name_length] = 0xFF;
        }
    } else if (name_length != 5) {
        name_text[name_length++] = ch;
        name_text[name_length] = 0xFF;
    } else {
        name_text[5] = ch;
        name_text[6] = 0xFF;
    }
}

void name_delete(void)
{
    unsigned char len;

    len = name_length;
    if (!len)
        return;
    if (name_who != 9) {
        if (len == 8 && name_text[len] != 0xFF)
            name_text[len] = 0xFF;
        else
            name_text[name_length = len - 1] = 0xFF;
    } else if (len != 5 || name_text[len] == 0xFF) {
        name_text[name_length = len - 1] = 0xFF;
    } else {
        name_text[len] = 0xFF;
    }
}

/* Whether the character's new name is cheat k, and if so its effect: the stock, or mastered sets of materia to try the
 * master materia trades (BGINMENU) with. */
int name_cheat(int k)
{
    int i, j;

    for (i = 0; i < 9; i++) {
        if (name_cheats[k][i] != D_8009C738[name_who].name[i])
            return;
        if (i != 8)
            continue;
        switch (k) {
        case 0:
            name_cheat_stock();
            break;
        case 1:
            for (j = 0; j < 21; j++)
                func_8002542C(name_cheat_magic[j] - 0x100);
            break;
        case 2:
            for (j = 0; j < 16; j++)
                func_8002542C(name_cheat_summons[j] - 0x100);
            break;
        case 3:
            for (j = 0; j < 7; j++)
                func_8002542C(name_cheat_commands[j] - 0x100);
            break;
        case 4:
            for (j = 0; j < 2; j++)
                func_8002542C(name_cheat_bahamuts[j] - 0x100);
            break;
        }
    }
}

void name_frame(int frame)
{
    RECT r, tw;
    int i, row, old;

    if (!name_phase) {
        if (!name_fade(-10))
            name_phase = 1;
    } else if (name_phase == 2) {
        if (name_fade(10) == 0xFF)
            name_phase = -1;
    }
    if (name_phase == 1) {
        old = name_cursors[name_state].col;
        func_800264A8(&name_cursors[name_state]);
        if (D_80062D7E & 4)
            name_sound(1);
        else if (D_80062D7E & 8)
            name_sound(1);
        if (name_page == 2 && name_cursors[0].row >= 7)
            name_cursors[0].row = 6;
        switch (name_state) {
        case 0:
            func_8001EB2C(name_cursors[0].col * 20 + 20, name_cursors[0].row * 16 + 100);
            if ((D_80062D7E & 0x2000) && old == name_cursors[name_state].col && !(field_pad1.held & 0x5000)) {
                name_sound(1);
                name_state = 1;
            } else if (D_80062D7E & 0x20) {
                name_sound(1);
                name_add(name_glyph(name_page, name_cursors[0].col, name_cursors[0].row));
            } else if (D_80062D7E & 0x40) {
                name_sound(4);
                name_delete();
            }
            break;
        case 1:
            func_8001EB2C(0x114, name_cursors[1].row * 16 + 0x9B);
            if ((D_80062D7E & 0x8000) && old == name_cursors[name_state].col) {
                name_sound(1);
                name_state = 0;
            }
            if (!(D_80062D7E & 0x20))
                break;
            name_sound(1);
            switch (name_cursors[name_state].row) {
            case 0:
                name_add(0);
                break;
            case 1:
                name_delete();
                break;
            case 2:
                if (!name_length)
                    break;
                if (name_who != 9) {
                    for (i = 0; i < 9; i++) {
                        D_8009C738[name_who].name[i] = name_text[i];
                        if (!name_who)
                            D_8009C738[6].name[i] = name_text[i];
                    }
                    D_8009C738[name_who].name[i] = 0xFF;
                } else {
                    for (i = 0; i < 6; i++)
                        D_8009D488[0][i] = name_text[i];
                    D_8009D488[0][i] = 0xFF;
                }
                name_phase = 2;
                break;
            case 3:
                name_load();
                break;
            }
            break;
        }
        if (D_80062D7E & 0x800) {
            name_state = 1;
            name_cursors[1].row = 2;
            name_sound(1);
        }
    }
    for (row = 0; row < 9; row++) {
        for (i = 0; i < 10; i++) {
            switch (name_page) {
            case 0:
                func_8002708C(i * 20 + 0x30, row * 14 + 0x62, name_kana1[row * 10 + i], 7);
                break;
            case 1:
                func_8002708C(i * 20 + 0x30, row * 14 + 0x62, name_kana2[row * 10 + i], 7);
                break;
            case 2:
                if (row < 7) {
                    func_80026C5C(i * 20 + 0x30, row * 16 + 0x62, 7, name_letters[row * 10 + i]);
                    setRECT(&tw, 0, 0, 0x100, 0x100);
                    func_80026A34(0, 1, 0x7E, &tw);
                }
                break;
            }
        }
    }
    setRECT(&tw, 0, 0, 0x100, 0x100);
    func_80026A34(0, 1, 0x7F, &tw);
    for (row = 0; row < 4; row++)
        func_80026F44(0x130, row * 16 + 0x9A, name_buttons[row], 7);
    if (name_who != 9) {
        for (i = 0; i < 9; i++) {
            if (i == name_length)
                func_8001D47C(i * 13 + 0x64, i * 13 + 0x6F, 0x42, frame & 4 ? 0xFFFF00 : 0x808000);
            else
                func_8001D47C(i * 13 + 0x64, i * 13 + 0x6F, 0x42, 0xFFFFFF);
        }
    } else {
        for (i = 0; i < 6; i++) {
            if (i == name_length)
                func_8001D47C(i * 13 + 0x64, i * 13 + 0x6F, 0x42, frame & 4 ? 0xFFFF00 : 0x808000);
            else
                func_8001D47C(i * 13 + 0x64, i * 13 + 0x6F, 0x42, 0xFFFFFF);
        }
    }
    for (i = 0; i < 9; i++) {
        if (name_text[i] == 0xFF)
            break;
        func_80026C5C(i * 13 + 0x64, 0x34, 7, name_text[i]);
    }
    func_80026F44(0x20, 0xE, kind_text(8, 10, 8), 7);
    func_8001D180(0x16, 0x24, 0x30, 0x30, 0, 0, 0x30, 0x30, 0, 0);
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x7F, &tw);
    func_8001DE0C(&r, 0, 5, 0x16C, 0x1B);
    func_8001E040(&r);
    func_8001DE0C(&r, 0, 0x20, 0x16C, 0x38);
    func_8001E040(&r);
    func_8001DE0C(&r, 0, 0x58, 0x11C, 0x88);
    func_8001E040(&r);
    func_8001DE0C(&r, 0x118, 0x58, 0x54, 0x88);
    func_8001E040(&r);
}

/* The named character's level and HP and MP; the screen's VRAM back as it was. */
void name_finish(void)
{
    if (name_who != 9 && name_who) {
        name_catch_up(&D_8009C738[character_record[name_who]]);
        D_8009C738[name_who].hp = D_8009C738[name_who].base_hp;
        D_8009C738[name_who].mp = D_8009C738[name_who].base_mp;
    }
    func_80025C54(name_vram);
}

void name_run(int who)
{
    int frame;

    func_80021044(name_draw, name_disp);
    name_who = who;
    name_phase = 0;
    frame = 0;
    name_init();
    name_buf = 0;
    for (;;) {
        func_8001CB48();
        func_800269C0(D_80077F64[name_buf]);
        name_ot = &name_ots[name_buf];
        func_8004418C(name_ot, 1);
        func_80026A00(name_ot);
        name_frame(frame);
        if (name_phase == -1)
            break;
        DrawSync(0);
        VSync(0);
        func_800444AC(&name_disp[name_buf]);
        PutDrawEnv(&name_draw[name_buf]);
        func_8004433C(name_ot);
        frame++;
        name_buf ^= 1;
    }
    VSync(0);
    name_finish();
    func_800444AC(&name_disp[0]);
    PutDrawEnv(&name_draw[0]);
    VSync(0);
    func_800444AC(&name_disp[1]);
    PutDrawEnv(&name_draw[1]);
}
