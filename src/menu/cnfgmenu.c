/* CNFGMENU.MNU, 0x801D0040-0x801D1AA8 (two jump tables before it, data after it to 0x801D2538): the Config command's
 * module. A list of settings changed with left and right: the window colour (its four corners, each red, green and
 * blue), Sound, the controller (Customize remaps the buttons), Cursor, ATB, the three speeds, Camera angle, Magic
 * order. The window colours are edited on a copy, kept when ✕ leaves the colour editor. GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

extern int cnfg_state; /* 0 the list, 1 picking a corner, 2 its red, green, blue, 3 Customize */
extern unsigned char cnfg_glyphs[12][2]; /* each pad bit's button: its glyph and palette */
extern unsigned char cnfg_buttons[12];   /* the buttons in the order Customize lists them */
extern unsigned char cnfg_text[39][48];
extern unsigned char cnfg_button_names[12][48]; /* what each bit does; 9 and 10 (no button) hold the prompts */
extern unsigned char cnfg_press_start[48];      /* cnfg_button_names[9] */
extern unsigned char cnfg_press_end[48];        /* cnfg_button_names[10] */
extern unsigned char cnfg_rgb_names[3][6];      /* in the small font */
extern unsigned char cnfg_orders[6][3];         /* each Magic order's kinds, in turn */
extern RECT cnfg_windows[2];
extern unsigned short cnfg_held, cnfg_prev, cnfg_pressed; /* the pad as read, unmapped: Customize reads it */
extern int cnfg_needed[3]; /* the bits Customize must leave mapped: Menu, OK, Cancel */
extern MenuCursor cnfg_cursors[4];
extern int cnfg_remap_row; /* Customize's row, from 1; 0 until START */
extern unsigned char cnfg_colors[12];

void cnfg_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

/* The driver's output for the Sound setting. */
void cnfg_sound_mode(int mode)
{
    switch (mode) {
    case 0:
        snd_request.id = 0x81;
        snd_request.arg[0] = 0x81;
        snd_request.arg[1] = 0x81;
        break;
    case 1:
        snd_request.id = 0x80;
        snd_request.arg[0] = 0x80;
        snd_request.arg[1] = 0x80;
        break;
    case 2:
        snd_request.id = 0x82;
        snd_request.arg[0] = 0x82;
        snd_request.arg[1] = 0x82;
        break;
    }
    snd_submit_request();
}

/* The lowest of the first 12 bits set, 0xFF for none. */
unsigned char cnfg_first_bit(unsigned short bits)
{
    int i;

    for (i = 0; i < 12; i++) {
        if (bits >> i & 1)
            return i;
    }
    return 0xFF;
}

/* An old-style definition: cnfg_frame passes the coordinates without extending them to short, which a prototype would
 * make it do. */
void cnfg_button(x, y, bit)
short x, y;
int bit;
{
    RECT unused; /* unused, but the game's frame has it */
    unsigned char glyph, pal;

    glyph = cnfg_glyphs[bit][0];
    pal = cnfg_glyphs[bit][1];
    func_80028CA0(x, y, (glyph & 3) * 16 + 0x60, (glyph >> 2) * 16 + 0x40, 16, 16, pal, 0);
}

/* The settings and their values, the one chosen white. */
void cnfg_list(void)
{
    RECT r;
    int i, y, w;

    y = cnfg_windows[1].y + 0x21;
    func_80026F44(0x28, cnfg_windows[1].y + 0xE, cnfg_text[0], 5);
    for (i = 1; i < 9; i++)
        func_80026F44(0x28, y + (i - 1) * 18, cnfg_text[i], 5);
    func_80026F44(0x28, y + (i - 1) * 18, cnfg_text[33], 5);
    for (i = 0; i < 2; i++)
        func_80026F44(0xA5 + i * 0x41, y, cnfg_text[15 + i], (settings.config & 3) == i ? 7 : 0);
    for (i = 0; i < 2; i++)
        func_80026F44(0xA5 + i * 0x41, y + 0x12, cnfg_text[18 + i], (settings.config >> 2 & 3) == i ? 7 : 0);
    for (i = 0; i < 2; i++)
        func_80026F44(0xA5 + i * 0x41, y + 0x24, cnfg_text[20 + i], (settings.config >> 4 & 3) == i ? 7 : 0);
    func_80026F44(0xA5, y + 0x36, cnfg_text[22], (settings.config >> 6 & 3) == 0 ? 7 : 0);
    w = func_80026B70(cnfg_text[22]);
    func_80026F44(w + 0xAF, y + 0x36, cnfg_text[23], (settings.config >> 6 & 3) == 1 ? 7 : 0);
    w += func_80026B70(cnfg_text[23]);
    func_80026F44(w + 0xB9, y + 0x36, cnfg_text[24], (settings.config >> 6 & 3) == 2 ? 7 : 0);
    for (i = 0; i < 2; i++)
        func_80026F44(0xA5 + i * 0x41, y + 0x7E, cnfg_text[25 + i], (settings.config >> 8 & 3) == i ? 7 : 0);
    for (i = 0; i < 3; i++)
        func_80026F44(0xBD + i * 0x34, y + 0x90, cnfg_text[34 + cnfg_orders[settings.config >> 10 & 7][i]], 7);
    func_80026F44(0x95, y + 0x90, cnfg_text[38], 7);
    r.x = settings.battle_speed / 2 + 0xB8;
    r.y = y + 0x48;
    r.w = 8;
    r.h = 0xB;
    func_80028030(&r);
    r.x = settings.battle_message_speed / 2 + 0xB8;
    r.y = y + 0x5A;
    r.w = 8;
    r.h = 0xB;
    func_80028030(&r);
    r.x = settings.message_speed / 2 + 0xB8;
    r.y = y + 0x6C;
    r.w = 8;
    r.h = 0xB;
    func_80028030(&r);
    for (i = 0; i < 3; i++) {
        func_80026F44(0x9D, y + 0x48 + i * 18 + 2, cnfg_text[29], 7);
        func_80026F44(0x144, y + 0x48 + i * 18 + 2, cnfg_text[28], 7);
    }
    for (i = 0; i < 3; i++) {
        r.x = 0xB8;
        r.y = y + 0x48 + i * 18;
        r.w = 0x88;
        r.h = 0xB;
        func_80027B84(&r);
    }
    func_80029114(0xAD, y + 0x92, (settings.config >> 10 & 7) + 1, 1, 7);
    setRECT(&r, 0, 0, 0x100, 0x100);
    func_80026A34(0, 1, 0x1F, &r);
}

void cnfg_init(void)
{
    RECT unused; /* unused, but the game's frame has it */
    int i;

    func_80026448(&cnfg_cursors[0], 0, 0, 1, 10, 0, 0, 1, 10, 0, 0, 0, 1, 0);
    func_80026448(&cnfg_cursors[1], 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0);
    func_80026448(&cnfg_cursors[2], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
    func_80026448(&cnfg_cursors[3], 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0);
    cnfg_state = 0;
    for (i = 0; i < 12; i++)
        cnfg_colors[i] = D_80049208[i];
}

#ifdef NON_MATCHING
/* Same code but for the saved registers: the game gives the colour editor's index s0 and still puts the drawing's x
 * in s3 before y; any split of those roles among the locals swaps one pair or the other (searched by hand and with the
 * permuter). Found on the way: ATB's row steps its value before the click sound, the others after it; Sound's right
 * steps it inside the expression; the drawing reads settings.config, not config, so GCC keeps its address in a
 * register; Customize's two prompts are symbols of their own, or GCC shares one address for both. */
void cnfg_frame(int frame)
{
    RECT r, tw;
    int i, y, k, found; /* k: an x as drawn, or the colour being changed */
    unsigned short mode;

    cnfg_held = func_8001C808();
    cnfg_pressed = cnfg_held & (cnfg_prev ^ cnfg_held);
    func_800230C4(results_buf);
    switch (cnfg_state) {
    case 0:
        func_80026F44(0x10, 0xB, kind_text(8, cnfg_cursors[0].row + 0x37, 8), 7);
        if (!cnfg_cursors[0].row)
            func_8001EB2C(0xC, cnfg_windows[1].y + 0x10);
        else
            func_8001EB2C(0xC, cnfg_windows[1].y + 0x23 + (cnfg_cursors[0].row - 1) * 18);
        break;
    case 1:
        if (frame & 2) {
            if (!cnfg_cursors[0].row)
                func_8001EB2C(0xC, cnfg_windows[1].y + 0x10);
            else
                func_8001EB2C(0xC, cnfg_windows[1].y + 0x25 + (cnfg_cursors[0].row - 1) * 18);
        }
        func_8001EB2C(cnfg_cursors[1].col * 61 + 0x8D, cnfg_windows[1].y + 4 + cnfg_cursors[1].row * 18);
        break;
    case 2:
        if (frame & 2) {
            func_8001EB2C(0xC, cnfg_windows[1].y + 0x10 + cnfg_cursors[0].row * 18);
            func_8001EB2C(cnfg_cursors[1].col * 61 + 0x8D, cnfg_windows[1].y + 4 + cnfg_cursors[1].row * 19);
        }
        func_8001EB2C(0x93, cnfg_windows[1].y + 0x24 + cnfg_cursors[2].row * 12);
        k = cnfg_windows[1].x + 0xAD;
        y = cnfg_windows[1].y + 0x23;
        func_8002708C(k, y, cnfg_rgb_names[0][0], 2);
        func_8002708C(k, y + 12, cnfg_rgb_names[1][0], 4);
        func_8002708C(k, y + 24, cnfg_rgb_names[2][0], 1);
        for (i = 0; i < 3; i++) {
            func_80029114(k + 0x10, y, cnfg_colors[cnfg_cursors[1].col * 3 + cnfg_cursors[1].row * 6 + i],
                          3, 7);
            y += 12;
            tw.x = cnfg_colors[cnfg_cursors[1].col * 3 + cnfg_cursors[1].row * 6 + i] / 2 + 0xD5;
            tw.y = cnfg_windows[1].y + 0x21 + i * 12;
            tw.w = 8;
            tw.h = 0xB;
            func_80028030(&tw);
            tw.x = 0xD5;
            tw.y = cnfg_windows[1].y + 0x21 + i * 12;
            tw.w = 0x88;
            tw.h = 0xB;
            func_80027B84(&tw);
        }
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x1F, &tw);
        func_8001DE0C(&r, cnfg_windows[1].x + 0xA5, cnfg_windows[1].y + 0x1D, 0xC0, 0x2B);
        func_8001E040(&r);
        func_8001DE0C(&r, cnfg_windows[1].x + 0xF8, cnfg_windows[1].y + 7, 0x1A, 0x12);
        func_8001DF24(&r, cnfg_colors[cnfg_cursors[1].col * 3 + cnfg_cursors[1].row * 6],
                      cnfg_colors[cnfg_cursors[1].col * 3 + cnfg_cursors[1].row * 6 + 1],
                      cnfg_colors[cnfg_cursors[1].col * 3 + cnfg_cursors[1].row * 6 + 2]);
        func_8001DE0C(&r, cnfg_windows[1].x + 0xF5, cnfg_windows[1].y + 4, 0x20, 0x18);
        func_8001E040(&r);
        break;
    case 3:
        if (frame & 2)
            func_8001EB2C(0xC, cnfg_windows[1].y + 0xA + cnfg_cursors[0].row * 21);
        if (!cnfg_remap_row)
            func_80026F44(0x10, 0xB, cnfg_press_start, 7);
        else
            func_80026F44(0x10, 0xB, cnfg_press_end, 7);
        k = cnfg_windows[1].x + 0x8C;
        y = cnfg_windows[1].y + 3;
        if (cnfg_remap_row)
            func_8001EB2C(cnfg_windows[1].x + 0x78, y + 0xA + (cnfg_remap_row - 1) * 18);
        for (i = 0; i < 10; i++)
            func_8002708C(k + 0x22, y + 0xA + i * 18, 0xDA, 5);
        func_8001DE0C(&r, k, y, 0x78, 0xBB);
        y += 6;
        k += 0xA;
        for (i = 0; i < 10; i++) {
            cnfg_button(k, y + i * 18, cnfg_buttons[i]);
            cnfg_button(k + 0x2C, y + i * 18, settings.button_map[cnfg_buttons[i]]);
            setRECT(&tw, 0, 0, 0x100, 0x100);
            func_80026A34(0, 1, 0x1F, &tw);
            func_80026F44(k + 0x3E, y + 4 + i * 18 - 2, cnfg_button_names[settings.button_map[cnfg_buttons[i]]], 6);
        }
        func_8001E040(&r);
        break;
    }
    cnfg_list();
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x1F, &tw);
    /* the window colour's sample, in the colours being edited */
    func_8001DE70();
    func_8001DEF0(cnfg_colors);
    func_8001DE0C(&r, cnfg_windows[1].x + 0xA5, cnfg_windows[1].y + 4, 0x40, 0x18);
    func_8001E040(&r);
    func_8001DEB0();
    for (i = 0; i < 2; i++)
        func_8001E040(&cnfg_windows[i]);
    if (!func_80023050()) {
        func_800264A8(&cnfg_cursors[cnfg_state]);
        switch (cnfg_state) {
        case 0:
            if (D_80062D7E & 0x40) {
                cnfg_sound(4);
                func_8002305C(5, 0);
                func_8002120C(0);
                break;
            }
            switch (cnfg_cursors[0].row) {
            case 0:
                if (D_80062D7C & 0x20) {
                    cnfg_state = 1;
                    cnfg_sound(1);
                }
                break;
            case 1:
                mode = settings.config & 3;
                if ((D_80062D7E & 0x2000) && mode != 1) {
                    cnfg_sound(1);
                    settings.config = (settings.config & ~3) | ++mode;
                    cnfg_sound_mode(mode);
                }
                if ((D_80062D7E & 0x8000) && mode) {
                    cnfg_sound(1);
                    mode--;
                    settings.config = (settings.config & ~3) | mode;
                    cnfg_sound_mode(mode);
                }
                break;
            case 2:
                mode = settings.config >> 2 & 3;
                if (D_80062D7C & 0x20) {
                    if (mode == 1) {
                        cnfg_sound(1);
                        cnfg_state = 3;
                        cnfg_remap_row = 0;
                    }
                    break;
                }
                if ((D_80062D7E & 0x2000) && mode != 1) {
                    cnfg_sound(1);
                    mode++;
                    settings.config = (settings.config & ~0xC) | mode << 2;
                }
                if ((D_80062D7E & 0x8000) && mode) {
                    cnfg_sound(1);
                    mode--;
                    settings.config = (settings.config & ~0xC) | mode << 2;
                }
                break;
            case 3:
                mode = settings.config >> 4 & 3;
                if ((D_80062D7E & 0x2000) && mode != 1) {
                    cnfg_sound(1);
                    mode++;
                    settings.config = (settings.config & ~0x30) | mode << 4;
                }
                if ((D_80062D7E & 0x8000) && mode) {
                    cnfg_sound(1);
                    mode--;
                    settings.config = (settings.config & ~0x30) | mode << 4;
                }
                break;
            case 4:
                mode = settings.config >> 6 & 3;
                if ((D_80062D7E & 0x2000) && mode != 2) {
                    mode++;
                    cnfg_sound(1);
                    settings.config = (settings.config & ~0xC0) | mode << 6;
                }
                if ((D_80062D7E & 0x8000) && mode) {
                    cnfg_sound(1);
                    mode--;
                    settings.config = (settings.config & ~0xC0) | mode << 6;
                }
                break;
            case 5:
                if ((field_pad1.held & 0x2000) && settings.battle_speed != 0xFF) {
                    if (frame & 4)
                        cnfg_sound(1);
                    settings.battle_speed++;
                }
                if ((field_pad1.held & 0x8000) && settings.battle_speed) {
                    if (frame & 4)
                        cnfg_sound(1);
                    settings.battle_speed--;
                }
                break;
            case 6:
                if ((field_pad1.held & 0x2000) && settings.battle_message_speed != 0xFF) {
                    if (frame & 4)
                        cnfg_sound(1);
                    settings.battle_message_speed++;
                }
                if ((field_pad1.held & 0x8000) && settings.battle_message_speed) {
                    if (frame & 4)
                        cnfg_sound(1);
                    settings.battle_message_speed--;
                }
                break;
            case 7:
                if ((field_pad1.held & 0x2000) && settings.message_speed != 0xFF) {
                    if (frame & 4)
                        cnfg_sound(1);
                    settings.message_speed++;
                }
                if ((field_pad1.held & 0x8000) && settings.message_speed) {
                    if (frame & 4)
                        cnfg_sound(1);
                    settings.message_speed--;
                }
                break;
            case 8:
                mode = settings.config >> 8 & 3;
                if ((D_80062D7E & 0x2000) && mode != 1) {
                    cnfg_sound(1);
                    mode++;
                    settings.config = (settings.config & ~0x300) | mode << 8;
                }
                if ((D_80062D7E & 0x8000) && mode) {
                    cnfg_sound(1);
                    mode--;
                    settings.config = (settings.config & ~0x300) | mode << 8;
                }
                break;
            case 9:
                mode = settings.config >> 10 & 7;
                if ((D_80062D7E & 0x2000) && mode != 5) {
                    cnfg_sound(1);
                    mode++;
                    settings.config = (settings.config & ~0x1C00) | mode << 10;
                }
                if ((D_80062D7E & 0x8000) && mode) {
                    cnfg_sound(1);
                    mode--;
                    settings.config = (settings.config & ~0x1C00) | mode << 10;
                }
                break;
            }
            break;
        case 1:
            if (D_80062D7C & 0x40) {
                for (i = 0; i < 12; i++)
                    D_80049208[i] = cnfg_colors[i];
                cnfg_sound(4);
                cnfg_state = 0;
            } else if (D_80062D7C & 0x20) {
                cnfg_state = 2;
                cnfg_sound(1);
            }
            break;
        case 2:
            k = cnfg_cursors[1].col * 3 + cnfg_cursors[1].row * 6 + cnfg_cursors[2].row;
            if (D_80062D7C & 0x40) {
                cnfg_sound(4);
                cnfg_state = 1;
            } else if (field_pad1.held & 0x2000) {
                if (cnfg_colors[k] != 0xFF) {
                    if (frame & 4)
                        cnfg_sound(1);
                    cnfg_colors[k]++;
                }
            } else if (field_pad1.held & 0x8000) {
                if (cnfg_colors[k]) {
                    if (frame & 4)
                        cnfg_sound(1);
                    cnfg_colors[k]--;
                }
            }
            break;
        case 3:
            if ((cnfg_pressed & 0x40) && !cnfg_remap_row) {
                cnfg_sound(4);
                cnfg_state = 0;
                break;
            }
            if (!cnfg_remap_row) {
                if (cnfg_pressed & 0x800)
                    cnfg_remap_row = 1;
                break;
            }
            if (!cnfg_pressed)
                break;
            if (cnfg_first_bit(cnfg_pressed) != 0xFF) {
                settings.button_map[cnfg_buttons[cnfg_remap_row - 1]] = cnfg_first_bit(cnfg_pressed);
                break;
            }
            if ((D_80062D7E & 0x1000) && cnfg_remap_row != 1) {
                cnfg_sound(1);
                cnfg_remap_row--;
                break;
            }
            if ((D_80062D7E & 0x4000) && cnfg_remap_row != 10) {
                cnfg_sound(1);
                cnfg_remap_row++;
                break;
            }
            if (D_80062D7E & 0xA000) {
                /* leaving needs Menu, OK and Cancel each on some button */
                found = 0;
                for (mode = 0; mode < 3; mode++) {
                    for (i = 0; i < 16; i++) {
                        if (cnfg_needed[mode] == settings.button_map[i])
                            found |= 1 << mode;
                    }
                }
                if (found == 7) {
                    cnfg_sound(1);
                    cnfg_remap_row = 0;
                    cnfg_state = 0;
                } else {
                    cnfg_sound(3);
                }
            }
            break;
        case 4: /* none: the game's switch has it */
            break;
        }
    }
    cnfg_prev = cnfg_held;
}
#else
INCLUDE_ASM("asm/menu/cnfgmenu/nonmatchings/cnfgmenu", cnfg_frame);
#endif

void cnfg_801D1AA0(void)
{
}
