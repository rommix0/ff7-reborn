/* SAVEMENU.MNU, 0x801D0408-0x801D1950 (its strings and jump tables at 0x801D0000-0x801D0050): the save screen, the
 * module's first object. The main menu's Save runs it a frame at a time (save_init, save_frame, save_finish) over its
 * own screen; a save point's field request runs it alone (save_run), with its own buffers and a fade. Pick a card,
 * read its 15 files' previews (savecard.c, saveload.c), pick a file, answer Overwrite?, and it's written; an
 * unformatted card offers to format it. The module's other two objects are savecard.c and saveload.c.
 * GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

int format(char *fs);
void func_8001DE24(RECT *r, int dx, int dy); /* moves it; takes shorts */
void func_8001DE40(RECT *dst, RECT *src);
void func_8001F710(void);
void func_80026B5C(int layer); /* empty */

/* savecard.c's and saveload.c's */
void card_start(void);
void card_stop(void);
unsigned short card_list(int card);
short card_save(int slot);
void card_check_both(int frame);
int card_load_preview(int card, int slot);
void card_draw_preview(int unused, int y, int slot);
void load_move_cursor(MenuCursor *c);

typedef struct CardStatus {
    unsigned char present;
    unsigned char error;
    unsigned char unformatted;
} CardStatus;

extern unsigned short D_80062F3C; /* the files on the card, a bit each */
extern DRAWENV results_draw[2];
extern CardStatus card_status[2];
extern unsigned char D_80077F64[2][0x3400]; /* the menus' primitive buffers */
extern int save_fade_level;
extern RECT save_card_rect;
extern RECT save_confirm_rect;
extern unsigned char save_prims[2][0x5000];
extern unsigned char save_title[];
extern RECT save_empty_rect;
extern RECT save_title_rect;
extern unsigned char save_texts[41][0x24];
extern unsigned char save_msgs[19][0x30];
extern int D_801E36A0;
extern int save_timer;
extern int save_listing; /* the card's files are still to be listed */
extern int save_file;    /* the next preview to read */
extern int save_phase;   /* 0 fading in, 1 running, 2 fading out, -1 done; save_run's only */
extern int save_buf;
extern int save_alone; /* run by save_run, not from the main menu */
extern DRAWENV save_draw[2];
extern DISPENV save_disp[2];
extern MenuCursor save_cursors[8]; /* [0] the card, [1] the files, [6] format?, [7] Overwrite? */
extern unsigned int save_state;
extern unsigned int *save_ot;
extern unsigned int save_ots[2];
extern int save_settle; /* counts down the first 240 frames, which pick another copy of "No Memory card." */
extern unsigned char save_vram[0x2800];
extern unsigned char save_vram2[0x2D00];

void save_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

int save_fade(int step)
{
    RECT tw;

    setlen(prim_next.tile, 3);
    setcode(prim_next.tile, 0x60);
    func_80046848(prim_next.tile, 1);
    prim_next.tile->x0 = 0;
    prim_next.tile->y0 = 0;
    prim_next.tile->w = 0x180;
    prim_next.tile->h = 0xE8;
    prim_next.tile->r0 = save_fade_level;
    prim_next.tile->g0 = save_fade_level;
    prim_next.tile->b0 = save_fade_level;
    AddPrim(prim_ot, prim_next.tile++);
    setRECT(&tw, 0, 0, 0xFF, 0xFF);
    func_80026A34(0, 1, 0x5F, &tw);
    save_fade_level += step;
    if (save_fade_level < 0)
        save_fade_level = 0;
    if (save_fade_level >= 0x100)
        save_fade_level = 0xFF;
    return save_fade_level;
}

void save_init(unsigned char alone)
{
    save_settle = 0xF0;
    save_alone = alone;
    save_state = 0;
    func_80026448(&save_cursors[0], 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 0);
    func_80025B8C(save_vram2);
    func_80025C14(save_vram);
    func_80025DF8();
    card_start();
}

void save_finish(void)
{
    func_80025BD0(save_vram2);
    func_80025C54(save_vram);
    card_stop();
}

/* Declared int, though it returns nothing: save_run hands on whatever is left in $v0. */
#ifdef NON_MATCHING
/* 1 instruction short of the game's 1,073, the rest registers and order: the file loop's y sums the other way, and
 * the game keeps format's result in $v1. */
int save_frame(int frame)
{
    RECT box;
    RECT tw;
    int i, rows;
    int w;
    int r, g, b;
    int slot;

    if (!save_alone)
        func_800230C4(results_buf);
    else if (save_phase == 0) {
        if (!save_fade(-15))
            save_phase = 1;
    } else if (save_phase == 2) {
        if (save_fade(15) == 0xFF)
            save_phase = -1;
    }
    if (!func_80023050() || (save_alone && save_phase == 1)) {
        if (!func_8001F6B4()) {
            if (save_state < 2)
                card_check_both(frame);
            if (save_settle)
                save_settle--;
        }
    }
    func_80026B5C(0x80);
    switch (save_state) {
    case 0:
        func_8001EB2C(save_card_rect.x - 0x12, save_card_rect.y + 6 + save_cursors[0].row * 12);
        func_80026F44(10, 11, save_texts[1], 7);
        func_80026F44(save_card_rect.x + 0xC, save_card_rect.y + 5, save_texts[3], card_status[0].present ? 7 : 0);
        func_80026F44(save_card_rect.x + 0xC, save_card_rect.y + 0x11, save_texts[4], card_status[1].present ? 7 : 0);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x7F, &tw);
        func_8001E040(&save_card_rect);
        break;
    case 7:
        func_8001EB2C(save_confirm_rect.x + 0x16, save_confirm_rect.y + 0x15 + save_cursors[7].row * 12);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x7F, &tw);
        func_80026F44(save_confirm_rect.x + 10, save_confirm_rect.y + 6, save_texts[33], 7);
        func_80026F44(save_confirm_rect.x + 0x30, save_confirm_rect.y + 0x13, save_texts[34], 7);
        func_80026F44(save_confirm_rect.x + 0x30, save_confirm_rect.y + 0x1F, save_texts[35], 7);
        func_8001E040(&save_confirm_rect);
        /* and the file list behind it */
    case 1:
        if (!card_status[save_cursors[0].row].present) {
            save_state = 0;
            break;
        }
        func_800269D0();
        func_800269C0(save_prims[!save_alone ? results_buf : save_buf]);
        if (save_state != 7 || frame & 2)
            func_8001EB2C(8, save_cursors[1].row * 64 + 0x38);
        rows = 4;
        if (!save_cursors[1].scrolling)
            rows = 3;
        for (i = 0; i < rows; i++) {
            if (D_80062F3C >> (i + save_cursors[1].top) & 1) {
                func_8001DE70();
                card_draw_preview(0, i * 64 + save_cursors[1].scroll_step * 8 + 0x1D, i + save_cursors[1].top);
                func_8001DEB0();
            } else {
                func_80026F44(0x32, i * 64 + save_cursors[1].scroll_step * 8 + 0x37, save_texts[8], 6);
                func_8001DE40(&box, &save_empty_rect);
                func_8001DE24(&box, 0, i * 64 + save_cursors[1].scroll_step * 8 + 0x1D);
                func_8001E040(&box);
            }
        }
        func_80026B5C(0x80);
        setRECT(&tw, 0, 0x1D, 0x16C, 0xC3);
        if (!save_alone)
            func_80026A94(&results_draw[results_buf], &tw);
        else
            func_80026A94(&save_draw[save_buf], &tw);
        func_80026F44(10, 11, save_texts[2], 7);
        func_80026F44(0xCE, 11, save_texts[9], 6);
        func_80026F44(func_80026B70(save_texts[9]) + 0xD0, 11,
                      save_texts[save_cursors[1].top + 13 + save_cursors[1].row], 7);
        func_8001DE0C(&box, 0xC8, 5, 0x4E, 0x18);
        func_8001E040(&box);
        func_800269E8();
        break;
    case 2:
    case 3:
        if (save_state == 2) {
            r = 0x40;
            g = 0x20;
            b = 0xA0;
        } else {
            r = 0xE0;
            g = 0x80;
            b = 0;
        }
        func_80026F44(10, 11, save_texts[12], 7);
        if (!save_listing) {
            func_800285AC(0x7A, 0x75, (save_file + 1) * 8, 8, r, g, b);
            setRECT(&tw, 0, 0, 0xFF, 0xFF);
            func_80026A34(0, 1, 0x3F, &tw);
        }
        func_8001DE0C(&box, 0x70, 0x6D, 0x8C, 0x18);
        func_8001E040(&box);
        break;
    case 4:
        w = func_80026B70(save_texts[7]) + 0x10;
        func_80026F44(0xBE - w / 2, 0x73, save_texts[7], 7);
        func_8001DE0C(&box, 0xB6 - w / 2, 0x6D, w, 0x18);
        func_8001E040(&box);
        break;
    case 6:
        if (frame & 2)
            func_8001EB2C(save_card_rect.x - 0x12, save_card_rect.y + 6 + save_cursors[0].row * 12);
        func_80026F44(save_card_rect.x + 0xC, save_card_rect.y + 5, save_texts[3], card_status[0].present ? 7 : 0);
        func_80026F44(save_card_rect.x + 0xC, save_card_rect.y + 0x11, save_texts[4], card_status[1].present ? 7 : 0);
        setRECT(&tw, 0, 0, 0x100, 0x100);
        func_80026A34(0, 1, 0x7F, &tw);
        func_8001E040(&save_card_rect);
        func_80026F44(10, 11, save_msgs[2], 7);
        w = func_80026B70(save_msgs[3]) + 0x10;
        func_80026F44(0xBE - w / 2, save_card_rect.h + 0x63, save_msgs[3], 7);
        func_80026F44(0xE4 - w / 2, save_card_rect.h + 0x70, save_texts[34], 7);
        func_80026F44(0xE4 - w / 2, save_card_rect.h + 0x7C, save_texts[35], 7);
        func_8001EB2C(0xC8 - w / 2, save_card_rect.h + save_cursors[6].row * 12 + 0x73);
        func_8001DE0C(&box, 0xB6 - w / 2, save_card_rect.h + 0x5D, w, 0x30);
        func_8001E040(&box);
        break;
    }
    if (save_alone) {
        func_80026B5C(0x80);
        func_80026F44(0x126, 0xB, save_title, 7);
        func_8001E040(&save_title_rect);
    }
    func_8001DE0C(&box, 0, 5, 0x16C, 0x18);
    func_8001E040(&box);
    if (!((!save_alone && !func_80023050()) || (save_alone && save_phase == 1)))
        return;
    if (func_8001F6B4())
        return;
    switch (save_state) {
    case 0:
        if (D_80062D7C & 0x20) {
            if (card_status[save_cursors[0].row].present) {
                save_sound(1);
                if (card_status[save_cursors[0].row].unformatted) {
                    save_state = 6;
                    func_80026448(&save_cursors[6], 0, 1, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 0);
                } else {
                    save_state = 2;
                    save_timer = 0x3C;
                    save_file = 0;
                    D_801E36A0 = 0;
                    D_80062F3C = 0;
                    save_listing = 1;
                    func_80026448(&save_cursors[1], 0, 0, 1, 3, 0, 0, 1, 15, 0, 0, 0, 0, 0);
                }
            } else {
                save_sound(3);
                if (!save_settle)
                    func_8001F6C0(save_msgs[5], 7);
                else
                    func_8001F6C0(save_msgs[4], 7);
            }
        } else {
            func_800264A8(&save_cursors[0]);
            if (save_alone) {
                if (D_80062D7C & 0x40) {
                    save_sound(4);
                    save_phase = 2;
                }
            } else if (D_80062D7C & 0x40) {
                save_sound(4);
                save_finish();
                func_8002305C(5, 0);
                func_8002120C(0);
            }
        }
        break;
    case 1:
        i = save_cursors[1].scroll_step;
        load_move_cursor(&save_cursors[1]);
        if (save_cursors[1].scroll_step || i)
            break;
        if (D_80062D7C & 0x20) {
            save_state = 7;
            func_80026448(&save_cursors[7], 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 0);
            save_sound(1);
        } else if (D_80062D7C & 0x40) {
            save_sound(4);
            save_state = 0;
        }
        break;
    case 2:
        if (save_timer) {
            save_timer--;
            break;
        }
        if (save_listing) {
            save_timer = 0;
            save_listing = 0;
            D_80062F3C = card_list(save_cursors[0].row);
            break;
        }
        i = 0;
        if (D_80062F3C >> save_file & 1)
            i = card_load_preview(save_cursors[0].row, save_file);
        save_file++;
        if (i) {
            save_state = 0;
            func_8001F6C0(save_msgs[13], 2);
        }
        if (save_file == 15) {
            save_file = 14;
            save_state = 3;
            save_timer = 10;
            save_sound(2);
        }
        break;
    case 3:
        if (!save_timer)
            save_state = 1;
        save_timer--;
        break;
    case 4:
        if (save_timer) {
            save_timer--;
            break;
        }
        save_state = 1;
        slot = save_cursors[1].row + save_cursors[1].top;
        if (save_cursors[0].row)
            slot |= 0x10;
        if (!card_save(slot)) {
            save_sound(0xD0);
            func_8001F6C0(save_texts[28], 7);
            D_80062F3C |= 1 << (save_cursors[1].row + save_cursors[1].top);
        } else {
            save_sound(3);
            func_8001F6C0(save_msgs[8], 7);
        }
        break;
    case 6:
        func_800264A8(&save_cursors[6]);
        if (D_80062D7C & 0x20) {
            if (save_cursors[6].row) {
                save_state = 0;
                save_sound(4);
                break;
            }
            i = format(save_cursors[0].row ? "bu10:" : "bu00:");
            save_state = 0;
            if (i == 1) {
                card_status[save_cursors[0].row].unformatted = 0;
                func_8001F6C0(save_msgs[0], 7);
                save_sound(0xD0);
            } else {
                func_8001F6C0(save_msgs[1], 7);
                save_sound(3);
            }
        } else if (D_80062D7C & 0x40) {
            save_state = 0;
            save_sound(4);
        }
        break;
    case 7:
        if (D_80062D7C & 0x20) {
            switch (save_cursors[7].row) {
            case 0:
                save_sound(1);
                save_state = 4;
                save_timer = 10;
                break;
            case 1:
                save_sound(4);
                save_state = 1;
                break;
            }
        } else if (D_80062D7C & 0x40) {
            save_state = 1;
            save_sound(4);
        } else
            func_800264A8(&save_cursors[7]);
        break;
    }
}
#else
INCLUDE_ASM("asm/menu/savemenu/nonmatchings/savemenu", save_frame);
#endif

/* A save point's save screen. */
int save_run(void)
{
    int frame;
    int result;

    func_80021044(save_draw, save_disp);
    save_phase = 0;
    frame = 0;
    save_init(1);
    save_buf = 0;
    for (;;) {
        func_8001CB48();
        func_800269C0(D_80077F64[save_buf]);
        save_ot = &save_ots[save_buf];
        func_8004418C(save_ot, 1);
        func_80026A00(save_ot);
        func_8001F710();
        result = save_frame(frame);
        if (save_phase == -1)
            break;
        DrawSync(0);
        VSync(0);
        func_800444AC(&save_disp[save_buf]);
        PutDrawEnv(&save_draw[save_buf]);
        func_8004433C(save_ot);
        frame++;
        save_buf ^= 1;
    }
    save_finish();
    VSync(0);
    func_800444AC(&save_disp[0]);
    PutDrawEnv(&save_draw[0]);
    VSync(0);
    func_800444AC(&save_disp[1]);
    PutDrawEnv(&save_draw[1]);
    return result;
}
