/* SAVEMENU.MNU's third object, 0x801D2B58-0x801D4EC4 (its strings and jump tables at 0x801D03B8-0x801D0408): the
 * title's NEW GAME / Continue? choice and the load screen (load_run, module 16 in the exe's list), with the card
 * checks, file list cursor and file panel the save screen (savemenu.c) uses too. Continue picks a card, reads the 15
 * files' previews, then loads the chosen one if its checksum holds; an unformatted card offers to format it.
 * GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

#ifdef NATIVE
/* The game's own file and card calls: the PC build's recompiled exe names these apart from the host C library's. */
#define _card_info psx__card_info
#define _card_clear psx__card_clear
#define _card_load psx__card_load
#endif

int format(char *fs);
int _card_info(int port);
int _card_clear(int port);
int _card_load(int port);
int TestEvent(int event);
void func_8001DE24(RECT *r, int dx, int dy); /* moves it; takes shorts */
void func_8001DE40(RECT *dst, RECT *src);
void func_8001F710(void);
void func_80025ED4(void);
void func_80026B5C(int layer); /* empty */
void party_effects_sum(void);
int func_80023788(int t); /* the play time's hours */
int func_8002382C(int t); /* and minutes */

/* savecard.c's */
unsigned short card_checksum(unsigned short len, unsigned char *data);
void card_start(void);
void card_stop(void);
unsigned short card_list(int card);
SavePreview *card_preview(int slot);
int card_read_preview(int slot);
int card_read_save(int slot);

typedef struct CardStatus {
    unsigned char present;
    unsigned char error;
    unsigned char unformatted;
} CardStatus;

extern volatile unsigned char D_80062D99; /* nonzero stops the play time */
extern unsigned short D_80062F3C;         /* the files on the card, a bit each */
extern int card_events[8];
extern CardStatus card_status[2];
extern unsigned char D_80077F64[2][0x3400]; /* the menus' primitive buffers */
extern unsigned char save_prims[2][0x5000]; /* the portraits' image until it's in VRAM, then primitives */
extern int load_fade_level;
extern unsigned int load_state;
extern unsigned char save_texts[41][0x24];
extern unsigned char save_msgs[19][0x30];
extern RECT load_rects[4]; /* [0]-[2] a file's portraits, [3] the card choice */
extern unsigned char load_lv_text[];
extern unsigned char menu_labels[2][12]; /* "Time", "Gil" */
extern unsigned char load_colors[12];
extern int load_result; /* 1: NEW GAME */
extern int load_phase;  /* 0 fading in, 1 running, 2 fading out, -1 done */
extern int load_buf;
extern unsigned int *load_ot;
extern unsigned int load_ots[2][4];
extern MenuCursor load_cursors[8]; /* [0] the card, [1] the files, [6] format?, [7] NEW GAME / Continue? */
extern DRAWENV load_draw[2];
extern DISPENV load_disp[2];
extern int load_timer;
extern int load_listing; /* the card's files are still to be listed */
extern int load_file;    /* the next preview to read */
extern int D_801E3F14;
extern unsigned char load_vram3[0x600];
extern unsigned char save_vram[0x2800];
extern unsigned char save_vram2[0x2D00];

void load_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

int load_fade(int step)
{
    RECT tw;

    setlen(prim_next.tile, 3);
    setcode(prim_next.tile, 0x60);
    func_80046848(prim_next.tile, 1);
    prim_next.tile->x0 = 0;
    prim_next.tile->y0 = 0;
    prim_next.tile->w = 0x180;
    prim_next.tile->h = 0xE8;
    prim_next.tile->r0 = load_fade_level;
    prim_next.tile->g0 = load_fade_level;
    prim_next.tile->b0 = load_fade_level;
    AddPrim(prim_ot, prim_next.tile++);
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x5F, &tw);
    load_fade_level += step;
    if (load_fade_level < 0)
        load_fade_level = 0;
    if (load_fade_level >= 0x100)
        load_fade_level = 0xFF;
    return load_fade_level;
}

/* CNFGMENU's cnfg_sound_mode: the driver's output for the Sound setting. */
void load_sound_mode(int mode)
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

/* The exe's func_800264A8 with this module's beep and a scroll over eight frames, not four: the file lists'. */
void load_move_cursor(MenuCursor *c)
{
    unsigned short pad;

    if (!c->scrolling) {
        pad = field_pad1.repeat;
        if (pad & 0x1000) {
            c->row--;
            switch (c->wrap_y) {
            case 0:
                if (c->row >= 0) {
                    load_sound(1);
                    break;
                }
                c->row = 0;
                if (c->top > 0) {
                    c->top--;
                    c->scroll_step = -7;
                    c->scrolling = 1;
                    load_sound(1);
                }
                break;
            case 1:
            case 2:
                if (c->row < 0)
                    c->row = c->rows - 1;
                load_sound(1);
                break;
            }
        } else if (pad & 0x4000) {
            c->row++;
            switch (c->wrap_y) {
            case 0:
                if (c->row < c->rows) {
                    load_sound(1);
                    break;
                }
                c->row = c->rows - 1;
                if (c->top < c->total - c->rows) {
                    c->scroll_step = -1;
                    c->scrolling = 2;
                    load_sound(1);
                }
                break;
            case 1:
            case 2:
                if (c->row >= c->rows)
                    c->row = 0;
                load_sound(1);
                break;
            }
        } else if (pad & 0x8000) {
            switch (c->wrap_x) {
            case 0:
                if (--c->col >= 0)
                    load_sound(1);
                else
                    c->col = 0;
                break;
            case 1:
                if (--c->col < 0)
                    c->col = c->cols - 1;
                load_sound(1);
                break;
            case 2:
                if (!c->col && !c->row && !c->top)
                    break;
                if (--c->col < 0) {
                    c->col = c->cols - 1;
                    if (--c->row < 0) {
                        c->row = 0;
                        if (c->top > 0) {
                            c->top--;
                            c->scroll_step = -7;
                            c->scrolling = 1;
                        }
                    }
                }
                load_sound(1);
                break;
            }
        } else if (pad & 0x2000) {
            switch (c->wrap_x) {
            case 0:
                if (++c->col < c->cols)
                    load_sound(1);
                else
                    c->col = c->cols - 1;
                break;
            case 1:
                if (++c->col >= c->cols)
                    c->col = 0;
                load_sound(1);
                break;
            case 2:
                if (c->col == c->cols - 1 && c->row == c->rows - 1 && c->top == c->total - c->rows)
                    break;
                if (++c->col >= c->cols) {
                    c->col = 0;
                    if (c->row >= c->cols) /* the game's, to no effect */
                        c->col = 0;
                    if (++c->row >= c->rows) {
                        c->row = c->rows - 1;
                        if (c->top < c->total - c->rows) {
                            c->scroll_step = -1;
                            c->scrolling = 2;
                        }
                    }
                }
                load_sound(1);
                break;
            }
        } else if (pad & 8) {
            c->top += c->rows;
            if (c->top > c->total - c->rows)
                c->top = c->total - c->rows;
            else
                load_sound(1);
        } else if (pad & 4) {
            c->top -= c->rows;
            if (c->top < 0)
                c->top = 0;
            else
                load_sound(1);
        }
        return;
    }
    switch (c->scrolling) {
    case 1:
        if (++c->scroll_step == 0) {
            c->scrolling = 0;
            c->scroll_step = 0;
        }
        break;
    case 2:
        if (--c->scroll_step == -8) {
            c->scrolling = 0;
            c->scroll_step = 0;
            c->top++;
        }
        break;
    }
}

/* Takes any pending card event: the BIOS's, then the hardware's. */
void card_flush_sw(void)
{
    TestEvent(card_events[0]);
    TestEvent(card_events[1]);
    TestEvent(card_events[2]);
    TestEvent(card_events[3]);
}

void card_flush_hw(void)
{
    TestEvent(card_events[4]);
    TestEvent(card_events[5]);
    TestEvent(card_events[6]);
    TestEvent(card_events[7]);
}

/* Waits for a card operation: 0 done, 1 an error, 2 no card, 3 a new card. */
int card_wait_sw(void)
{
    for (;;) {
        if (TestEvent(card_events[0]) == 1)
            return 0;
        if (TestEvent(card_events[1]) == 1)
            return 1;
        if (TestEvent(card_events[2]) == 1)
            return 2;
        if (TestEvent(card_events[3]) == 1)
            return 3;
    }
}

int card_wait_hw(void)
{
    for (;;) {
        if (TestEvent(card_events[4]) == 1)
            return 0;
        if (TestEvent(card_events[5]) == 1)
            return 1;
        if (TestEvent(card_events[6]) == 1)
            return 2;
        if (TestEvent(card_events[7]) == 1)
            return 3;
    }
}

/* Whether a card is in the slot, and formatted: a card newly put in is read anew (_card_load). Declared int, though it
 * returns nothing: that keeps $v0 live at the end, as the game's code has it. */
int card_check(int card)
{
    int port = (card != 0) << 4;

    _card_info(port);
    switch (card_wait_sw()) {
    case 0:
        if (card_status[card].present)
            return;
        card_status[card].present = 1;
        break;
    case 1:
        card_status[card].error = 1;
        return;
    case 2:
        card_status[card].present = 0;
        card_status[card].error = 0;
        card_status[card].unformatted = 0;
        return;
    case 3:
        card_flush_hw();
        _card_clear(port);
        card_wait_hw();
        break;
    default:
        card_status[card].error = 1;
        return;
    }
    card_flush_sw();
    _card_load(port);
    switch (card_wait_sw()) {
    case 0:
        card_status[card].unformatted = 0;
        break;
    case 1:
        card_status[card].error = 1;
        break;
    case 2:
        card_status[card].present = 0;
        break;
    case 3:
        card_status[card].unformatted = 1;
        break;
    default:
        card_status[card].error = 1;
        break;
    }
}

/* Both slots, every 64 frames. */
void card_check_both(int frame)
{
    if (!(frame & 0x3F)) {
        card_check(0);
        card_check(1);
    }
}

/* A file's preview, tried 20 times: 0 read, 1 not. */
int card_load_preview(int card, int slot)
{
    int i, r;

    for (i = 0; i < 20; i++) {
        if (!card_read_preview(card ? slot | 0x10 : slot)) {
            r = 0;
            break;
        }
        r = 1;
    }
    return r;
}

/* A file's panel: the party's portraits, the leader's name and level, the place, time and gil, in the save's window
 * colours. */
void card_draw_preview(int unused, int y, int slot)
{
    SavePreview *p;
    int i, j;
    RECT r;
    RECT tw;

    p = card_preview(slot);
    func_80026F44(0xC0, y + 0x2E, p->place, 7);
    for (i = 0; i < 3; i++) {
        if (p->portraits[i] != 0xFF)
            func_8001D180(i * 0x34 + 0x16, y + 6, 0x30, 0x30, p->portraits[i] < 5 ? 0 : 0x30,
                          p->portraits[i] % 5 * 0x30, 0x30, 0x30, p->portraits[i], 0);
    }
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x7F, &tw);
    func_80028E00(func_80026B70(load_lv_text) + 0xC2, y + 0x1C, p->level, 2, 7);
    func_8002708C(0x152, y + 0xC, 0xD5, 7);
    func_80029114(0x144, y + 0xB, func_80023788(p->seconds), 2, 7);
    func_80029114(0x159, y + 0xB, func_8002382C(p->seconds), 2, 7);
    func_80028E00(0x135, y + 0x19, p->gil, 7, 7);
    setRECT(&tw, 0, 0, 0x100, 0x100);
    func_80026A34(0, 1, 0x7F, &tw);
    func_80026F44(0xBD, y + 0x1A, load_lv_text, 5);
    func_80026F44(0xB8, y + 8, p->name, 7);
    func_80026F44(0x11C, y + 9, menu_labels[0], 7);
    func_80026F44(0x11C, y + 0x17, menu_labels[1], 7);
    func_8001DEF0(p->colors);
    for (j = 0; j < 3; j++) {
        func_8001DE40(&r, &load_rects[j]);
        func_8001DE24(&r, 0, y);
        func_8001E040(&r);
    }
}

void load_init(void)
{
    load_result = 0;
    load_state = 7;
    func_8001DEF0(load_colors);
    func_80025D14(save_prims, 0x380, 0, 0, 0x1E0);
    DrawSync(0);
    func_80026448(&load_cursors[7], 0, 1, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 0);
    func_80025CD4(load_vram3);
    func_80025B8C(save_vram2);
    func_80025C14(save_vram);
    func_80025DF8();
    card_start();
}

#ifdef NON_MATCHING
/* 1 instruction short of the game's 1,122, the rest registers: the game keeps the file loop's row and y the other
 * way round, the Format? box's width in its own register, and format's result in $v1. */
int load_frame(int frame)
{
    RECT box;
    RECT tw;
    int i, rows;
    int w;
    unsigned char *text;
    int r, g, b;
    short err;
    int slot;

    if ((load_state < 2 || load_state == 7) && load_phase != 2 && load_phase != 0)
        card_check_both(frame);
    func_80026B5C(0x80);
    if (load_phase == 0) {
        if (!load_fade(-15))
            load_phase = 1;
    } else if (load_phase == 2) {
        if (load_fade(15) == 0xFF)
            load_phase = -1;
    }
    func_8001F6B4();
    switch (load_state) {
    case 0:
        func_8001EB2C(load_rects[3].x - 0x12, load_rects[3].y + 6 + load_cursors[0].row * 12);
        func_80026F44(10, 11, save_texts[1], 7);
        func_80026F44(load_rects[3].x + 0xC, load_rects[3].y + 5, save_texts[3], card_status[0].present ? 7 : 0);
        func_80026F44(load_rects[3].x + 0xC, load_rects[3].y + 0x11, save_texts[4], card_status[1].present ? 7 : 0);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x7F, &tw);
        func_8001E040(&load_rects[3]);
        break;
    case 1:
        if (!card_status[load_cursors[0].row].present) {
            load_state = 0;
            break;
        }
        rows = 4;
        func_800269D0();
        func_800269C0(save_prims[load_buf]);
        func_8001EB2C(8, load_cursors[1].row * 64 + 0x38);
        if (!load_cursors[1].scrolling)
            rows = 3;
        for (i = 0; i < rows; i++) {
            if (D_80062F3C >> (i + load_cursors[1].top) & 1) {
                func_8001DE70();
                card_draw_preview(0, i * 64 + load_cursors[1].scroll_step * 8 + 0x1D, i + load_cursors[1].top);
                func_8001DEB0();
            } else {
                func_80026F44(0x32, i * 64 + load_cursors[1].scroll_step * 8 + 0x37, save_texts[8], 6);
                func_8001DE40(&box, &load_rects[2]);
                func_8001DE24(&box, 0, i * 64 + load_cursors[1].scroll_step * 8 + 0x1D);
                func_8001E040(&box);
            }
        }
        func_80026B5C(0x80);
        setRECT(&tw, 0, 0x1D, 0x16C, 0xC3);
        func_80026A94(&load_draw[load_buf], &tw);
        func_80026F44(10, 11, save_texts[2], 7);
        func_80026F44(0xCE, 11, save_texts[9], 6);
        func_80026F44(func_80026B70(save_texts[9]) + 0xD0, 11,
                      save_texts[load_cursors[1].top + 13 + load_cursors[1].row], 7);
        func_8001DE0C(&box, 0xC8, 5, 0x4E, 0x18);
        func_8001E040(&box);
        func_800269E8();
        break;
    case 2:
    case 3:
        if (load_state == 2) {
            r = 0x40;
            g = 0x20;
            b = 0xA0;
        } else {
            r = 0xE0;
            g = 0x80;
            b = 0;
        }
        func_80026F44(10, 11, save_texts[12], 7);
        if (!load_listing) {
            func_800285AC(0x7A, 0x75, (load_file + 1) * 8, 8, r, g, b);
            setRECT(&tw, 0, 0, 0xFF, 0xFF);
            func_80026A34(0, 1, 0x3F, &tw);
        }
        func_8001DE0C(&box, 0x70, 0x6D, 0x8C, 0x18);
        func_8001E040(&box);
        break;
    case 4:
        if (load_phase == 2)
            break;
        w = func_80026B70(save_texts[6]) + 0x10;
        func_80026F44(0xBE - w / 2, 0x73, save_texts[6], 7);
        func_8001DE0C(&box, 0xB6 - w / 2, 0x6D, w, 0x18);
        func_8001E040(&box);
        break;
    case 6:
        if (frame & 2)
            func_8001EB2C(load_rects[3].x - 0x12, load_rects[3].y + 6 + load_cursors[0].row * 12);
        func_80026F44(load_rects[3].x + 0xC, load_rects[3].y + 5, save_texts[3], card_status[0].present ? 7 : 0);
        func_80026F44(load_rects[3].x + 0xC, load_rects[3].y + 0x11, save_texts[4], card_status[1].present ? 7 : 0);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x7F, &tw);
        func_8001E040(&load_rects[3]);
        func_80026F44(10, 11, save_msgs[2], 7);
        w = func_80026B70(save_msgs[3]) + 0x10;
        func_80026F44(0xBE - w / 2, load_rects[3].h + 0x63, save_msgs[3], 7);
        func_80026F44(0xE4 - w / 2, load_rects[3].h + 0x70, save_texts[34], 7);
        func_80026F44(0xE4 - w / 2, load_rects[3].h + 0x7C, save_texts[35], 7);
        func_8001EB2C(0xC8 - w / 2, load_rects[3].h + load_cursors[6].row * 12 + 0x73);
        func_8001DE0C(&box, 0xB6 - w / 2, load_rects[3].h + 0x5D, w, 0x30);
        func_8001E040(&box);
        break;
    case 7:
        func_8001EB2C(load_rects[3].x - 0x12, load_rects[3].y + 6 + load_cursors[7].row * 12);
        func_80026F44(load_rects[3].x + 8, load_rects[3].y + 6, save_texts[32], 7);
        func_80026F44(load_rects[3].x + 8, load_rects[3].y + 0x12, save_texts[10],
                      card_status[0].present || card_status[1].present ? 7 : 0);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x7F, &tw);
        SetPolyFT4(prim_next.ft4);
        prim_next.ft4->r0 = 0x60;
        prim_next.ft4->g0 = 0x60;
        prim_next.ft4->b0 = 0x60;
        prim_next.ft4->x0 = 0x6F;
        prim_next.ft4->y0 = 0x54;
        prim_next.ft4->x1 = 0x119;
        prim_next.ft4->y1 = 0x54;
        prim_next.ft4->x2 = 0x6F;
        prim_next.ft4->y2 = 0xEA;
        prim_next.ft4->x3 = 0x119;
        prim_next.ft4->y3 = 0xEA;
        prim_next.ft4->u0 = 0;
        prim_next.ft4->v0 = 0;
        prim_next.ft4->u1 = 0x96;
        prim_next.ft4->v1 = 0;
        prim_next.ft4->u2 = 0;
        prim_next.ft4->v2 = 0x96;
        prim_next.ft4->u3 = 0x96;
        prim_next.ft4->v3 = 0x96;
        prim_next.ft4->clut = GetClut(0, 0x1E0);
        prim_next.ft4->tpage = GetTPage(1, 0, 0x380, 0);
        AddPrim(prim_ot, prim_next.ft4);
        prim_next.ft4++;
        break;
    }
    if (load_state != 7) {
        func_80026F44(0x126, 0xB, save_texts[0], 7);
        func_8001DE0C(&box, 0x116, 5, 0x56, 0x18);
        func_8001E040(&box);
        func_8001DE0C(&box, 0, 5, 0x16C, 0x18);
        func_8001E040(&box);
    }
    if (func_8001F6B4() || load_phase != 1)
        return load_result;
    switch (load_state) {
    case 0:
        if (D_80062D7C & 0x20) {
            if (load_cursors[0].row < 2) {
                if (load_cursors[0].row < 0)
                    break;
                if (card_status[load_cursors[0].row].present) {
                    load_sound(1);
                    if (card_status[load_cursors[0].row].unformatted) {
                        load_state = 6;
                        func_80026448(&load_cursors[6], 0, 1, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 0);
                    } else {
                        load_timer = 10;
                        load_state = 2;
                        load_file = 0;
                        D_801E3F14 = 0;
                        D_80062F3C = 0;
                        load_listing = 1;
                        func_80026448(&load_cursors[1], 0, 0, 1, 3, 0, 0, 1, 15, 0, 0, 0, 0, 0);
                    }
                } else {
                    load_sound(3);
                    func_8001F6C0(save_msgs[5], 7);
                }
            }
        } else if (D_80062D7C & 0x40) {
            load_sound(4);
            load_state = 7;
        } else
            func_800264A8(&load_cursors[0]);
        break;
    case 1:
        i = load_cursors[1].scroll_step;
        load_move_cursor(&load_cursors[1]);
        if (load_cursors[1].scroll_step || i)
            break;
        if (D_80062D7C & 0x20) {
            if (D_80062F3C >> (load_cursors[1].row + load_cursors[1].top) & 1) {
                load_sound(1);
                load_state = 4;
                load_timer = 10;
            } else
                load_sound(3);
        } else if (D_80062D7C & 0x40) {
            load_sound(4);
            load_state = 0;
        }
        break;
    case 2:
        if (load_timer) {
            load_timer--;
            break;
        }
        if (load_listing) {
            load_timer = 0;
            load_listing = 0;
            D_80062F3C = card_list(load_cursors[0].row);
            break;
        }
        i = 0;
        if (D_80062F3C >> load_file & 1)
            i = card_load_preview(load_cursors[0].row, load_file);
        load_file++;
        if (i) {
            load_state = 0;
            func_8001F6C0(save_msgs[13], 2);
            load_sound(3);
        }
        if (load_file == 15) {
            load_file = 14;
            load_state = 3;
            load_timer = 10;
            load_sound(2);
        }
        break;
    case 3:
        if (!load_timer)
            load_state = 1;
        load_timer--;
        break;
    case 4:
        if (load_timer) {
            load_timer--;
            break;
        }
        D_80062D99 = 1;
        slot = load_cursors[1].row + load_cursors[1].top;
        if (load_cursors[0].row)
            slot |= 0x10;
        err = card_read_save(slot);
        if (!err) {
            if (save_data.preview.checksum != card_checksum(0x10F0, &save_data.preview.level)) {
                load_state = 1;
                load_sound(3);
                func_8001F6C0(save_texts[31], 0);
            } else {
                load_sound(0xD0);
                load_phase = 2;
                load_sound_mode(config & 3);
            }
        } else {
            load_state = 1;
            load_sound(3);
            func_8001F6C0(save_texts[11], err);
        }
        D_80062D99 = 0;
        break;
    case 6:
        func_800264A8(&load_cursors[6]);
        if (D_80062D7C & 0x20) {
            if (load_cursors[6].row) {
                load_state = 0;
                load_sound(4);
                break;
            }
            i = format(load_cursors[0].row ? "bu10:" : "bu00:");
            load_state = 0;
            if (i == 1) {
                card_status[load_cursors[0].row].unformatted = 0;
                func_8001F6C0(save_msgs[0], 7);
                load_sound(0xD0);
            } else {
                func_8001F6C0(save_msgs[1], 7);
                load_sound(3);
            }
        } else if (D_80062D7C & 0x40) {
            load_state = 0;
            load_sound(4);
        }
        break;
    case 7:
        if (D_80062D7C & 0x20) {
            switch (load_cursors[7].row) {
            case 0:
                load_sound(0xD0);
                load_result = 1;
                load_phase = 2;
                break;
            case 1:
                if (card_status[0].present || card_status[1].present) {
                    load_sound(1);
                    func_80026448(&load_cursors[0], 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 0);
                    load_state = 0;
                } else
                    load_sound(3);
                break;
            }
        } else
            func_800264A8(&load_cursors[7]);
        break;
    }
    return load_result;
}
#else
INCLUDE_ASM("asm/menu/savemenu/nonmatchings/saveload", load_frame);
#endif

void load_finish(void)
{
    func_80025ED4();
    func_80025CD4(D_800756F8);
    func_80025C94(load_vram3);
    func_80025BD0(save_vram2);
    func_80025C54(save_vram);
    card_stop();
    func_800444AC(&load_disp[0]);
    PutDrawEnv(&load_draw[0]);
}

/* NEW GAME / Continue?, and the load screen: 1 for NEW GAME. */
int load_run(void)
{
    int frame;
    int result;
    int i;

    func_80021044(load_draw, load_disp);
    load_phase = 0;
    frame = 0;
    load_init();
    load_buf = 0;
    for (;;) {
        func_8001CB48();
        func_800269C0(D_80077F64[load_buf]);
        load_ot = load_ots[load_buf];
        func_8004418C(load_ot, 1);
        func_80026A00(load_ot);
        func_8001F710();
        result = load_frame(frame);
        if (load_phase == -1)
            break;
        DrawSync(0);
        VSync(0);
        func_800444AC(&load_disp[load_buf]);
        PutDrawEnv(&load_draw[load_buf]);
        func_8004433C(load_ot);
        frame++;
        load_buf ^= 1;
    }
    load_finish();
    VSync(0);
    func_800444AC(&load_disp[1]);
    PutDrawEnv(&load_draw[1]);
    for (i = 0; i < 3; i++) {
        if (save_data.party[i] != 0xFF) {
            func_80020058(i);
            member_update(i);
        }
    }
    party_effects_sum();
    return result;
}
