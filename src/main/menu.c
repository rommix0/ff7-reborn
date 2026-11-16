/* The field menu's top level, 0x80023050-0x80025288: the command list, the party/time/gil screen with its Order swaps,
 * the main loop that runs a menu module each frame, and the calls that load an overlay and run it. GCC 2.7.2,
 * ASPSX 2.21, -G8, --expand-div. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "menu.h"

extern PartyMember members[3];

int command_window_h;
int menu_state; /* 0 in a module, 1 on the commands, 2 and 4 closing them for a module, 3 back, 5-6 to the commands */
int menu_return_step;
int menu_command_prev;
int menu_timer;
unsigned short menu_shown, menu_locked; /* a bit per command */
int menu_cursor_at; /* 0 the commands, 1 picking a member for one, 2 Order */
int menu_member;
int order_picked; /* Order's first pick is made */
int D_80062F90;
int menu_command;
int menu_phase; /* 0 sliding in, 1 up, 2 sliding out, -1 closed */
int order_first;
unsigned char D_80063024;
int menu_module_prev;
int menu_module;
int results_buf;
unsigned int *results_ot;
short results_slide;
short results_slide_step;

extern MenuCursor menu_cursors[3];
extern unsigned char command_names[10][12];
extern unsigned char command_kind[10];   /* 1 opens its module, 2 picks a member first */
extern unsigned char command_module[10];
extern void (*module_inits[])();
extern void (*module_frames[])(int frame);
extern int clock_units[6]; /* seconds in ten hours, an hour, ten minutes, a minute, ten seconds, a second */
extern unsigned char limit_colors[8][3]; /* a full limit bar flashes through them; [0] Fury, [2] Sadness */
extern unsigned char menu_labels[2][12];   /* "Time", "Gil" */
extern unsigned char member_labels[2][12]; /* "LV", "Limit level" */
extern unsigned char demo_text[];
extern int D_80049500[8];
extern unsigned short D_8009D78A[]; /* in the field scripts' variable banks */
extern unsigned char D_8009D44C[8];
/* The save from the party on, as one object: the game reaches the party from the address of the command bytes. */
extern struct {
    unsigned char party[4]; /* three character ids, 0xFF for an empty place */
    unsigned short inventory[320];
    unsigned char pad[0x444];
    unsigned char cmd_shown[2], cmd_locked[2]; /* a bit per command, in menu_shown and menu_locked */
} D_8009CBDC;
extern unsigned char D_8009D4F7;
extern unsigned char D_8009D5F0[]; /* the location's name */
extern unsigned short inventory[320];
extern unsigned char text_page_per_line;
extern int demo_input;
extern TILE fade_tile[2];
extern unsigned int menu_ots[2][20];
extern DRAWENV results_draw[2];
extern DISPENV results_disp[2];
extern unsigned char D_80077F64[2][0x3400];
extern int save_gil;
extern unsigned int D_8009D264, D_8009D26C; /* the play time: seconds, and a second's fraction (main.c) */

void func_8001C484(unsigned char *script);
unsigned int func_8001C808(void);
void func_8001CB48(void);
void func_8001F6E4(short placed, short x, short y);
void func_8001F710(void);
void func_8001FA28(unsigned short effect);
void func_80020058(int slot);
void func_80021044(DRAWENV *draw, DISPENV *disp);
void func_800211C4(int module);
void func_80025CD4(void *p);
void func_80026090(void);
void func_80026A00(unsigned int *ot);
void func_80026A94(DRAWENV *env, RECT *clip);
void func_8002708C(int x, int y, int glyph, int color);
void func_80029114(int x, int y, int n, int digits, int color);
void func_80044000(RECT *r, void *p);
void func_80044064(RECT *r, void *p);
void func_8004418C(unsigned int *ot, int n);
void func_8004433C(unsigned int *ot);
void func_800444AC(DISPENV *env);
void func_80046848(void *p, int semi);
void member_update(unsigned char slot);
void party_effects_sum(void);
int cd_step(void);
void SetTile(void *p);
void AddPrim(void *ot, void *p);
void PutDrawEnv(DRAWENV *env);
int DrawSync(int mode);
int VSync(int mode);
int MoveImage(RECT *r, int x, int y);
void func_801D027C(void);
void func_801D05C4(int arg);
void func_801D0704(int arg);
void func_801D131C(int arg);
void func_801D1774(void);
void func_801D1A6C(int arg);
void func_801D2D74(void);
void func_801D2E84(void);
void func_801D2F00(int arg);
void func_801D3018(int arg);
void func_801D3138(int arg);
void func_801D3228(void);
void func_801D4118(int arg);
void func_801D4CC0(void);

int func_80023050(void)
{
    return menu_state;
}

void func_8002305C(int state, int command)
{
    int prev = menu_command;

    menu_state = state;
    menu_command = command - 1;
    menu_command_prev = prev;
    switch (state) {
    case 1: /* nothing, but the game's switch has the case */
        return;
    case 2:
        menu_timer = 16;
        break;
    case 4:
        menu_timer = 16;
        break;
    case 5:
        menu_cursors[0].row = prev;
        menu_return_step = 0;
        menu_timer = 16;
        break;
    }
}

/* The command list's window: all ten while choosing, else one name sliding to or from its place. */
void func_800230C4(int buf)
{
    RECT clip;
    RECT win;
    int busy;
    int i;
    int y;
    int slide; /* the name's offset from its place in the list */

    busy = cd_step();
    clip.x = 0;
    clip.y = 5;
    clip.w = 0x16C;
    clip.h = 0xDB;
    func_80026A94(&results_draw[buf], &clip);
    y = 0;
    switch (menu_state) {
    case 2:
        if (menu_timer)
            menu_timer--;
        else if (!busy) {
            DATA_CALL(module_inits[menu_module], 0);
            menu_state = 3;
        }
        slide = menu_command * 12 * menu_timer / 16;
        func_80026F44(0x126, slide + 11, command_names[menu_command], 7);
        command_window_h = menu_timer * 108 / 16 + 24;
        break;
    case 4:
        if (menu_timer) {
            menu_timer--;
        } else {
            menu_state = 3;
            DATA_CALL0(module_inits[menu_module]);
        }
        func_80026F44(0x126, 11, command_names[menu_command_prev], 7);
        command_window_h = 24;
        break;
    case 3:
        if (menu_timer < 15)
            menu_timer++;
        else
            menu_state = 0;
    case 0:
        command_window_h = 24;
        func_80026F44(0x126, 11, command_names[menu_command], 7);
        break;
    case 1:
        command_window_h = 0x84;
        if (menu_phase != 1)
            y = results_slide * 5 - 100;
        for (i = 0; i < 10; i++) {
            if (menu_shown >> i & 1)
                func_80026F44(0x126, y + 11 + i * 12, command_names[i], (menu_locked >> i & 1) ? 0 : 7);
        }
        break;
    case 5:
        if (menu_timer) {
            menu_timer--;
            if (!menu_timer && menu_phase == 2) {
                menu_phase = -1;
                menu_state = 1;
            }
        } else {
            menu_state = 6;
        }
        func_80026F44(0x126, 11, command_names[menu_command_prev], 7);
        command_window_h = 24;
        break;
    case 6:
        if (menu_timer < 15)
            menu_timer++;
        else
            menu_state = 1;
        slide = menu_command_prev * 12 * menu_return_step / 16;
        func_80026F44(0x126, slide + 11, command_names[menu_command_prev], 7);
        command_window_h = menu_return_step * 108 / 16 + 24;
        menu_return_step++;
        break;
    }
    if (menu_phase == 1) {
        clip.x = 0x11A;
        clip.y = y + 8;
        clip.w = 0x56;
        clip.h = command_window_h - 6;
        func_80026A94(&results_draw[buf], &clip);
    }
    func_8001DE0C(&win, 0x116, y + 5, 0x56, command_window_h);
    func_8001E040(&win);
    if (menu_state != 0 && menu_state != 1) {
        TILE *tiles = fade_tile;
        int c = 0xFF - menu_timer * 16;

        SetTile(&tiles[results_buf]);
        func_80046848(&tiles[results_buf], 1);
        tiles[results_buf].x0 = 0;
        tiles[results_buf].y0 = 0;
        tiles[results_buf].w = 0x180;
        tiles[results_buf].h = 0xE0;
        tiles[results_buf].r0 = c;
        tiles[results_buf].g0 = c;
        tiles[results_buf].b0 = c;
        AddPrim(results_ot, &tiles[results_buf]);
    }
    clip.x = 0;
    clip.y = 0;
    clip.w = 0xFF;
    clip.h = 0xFF;
    func_80026A34(0, 1, 0x5F, &clip);
    clip.x = 0;
    clip.y = 5;
    clip.w = 0x16C;
    clip.h = 0xDB;
    func_80026A94(&results_draw[results_buf], &clip);
}

void func_8002368C(void)
{
    menu_cursor_at = 0;
    func_80026448(&menu_cursors[0], 0, 0, 1, 10, 0, 0, 1, 10, 0, 0, 0, 1, 0);
    func_80026448(&menu_cursors[1], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
    func_80026448(&menu_cursors[2], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
}

/* The play time's hours, minutes and seconds, each two digits, held at 99:59:59. */
int func_80023788(int t)
{
    if (t > 359999)
        t = 359999;
    return t / clock_units[0] * 10 + t % clock_units[0] / clock_units[1];
}

int func_8002382C(int t)
{
    if (t > 359999)
        t = 359999;
    t %= clock_units[0];
    t %= clock_units[1];
    return t / clock_units[2] * 10 + t % clock_units[2] / clock_units[3];
}

int func_80023940(int t)
{
    if (t > 359999)
        t = 359999;
    t %= clock_units[0];
    t %= clock_units[1];
    t %= clock_units[2];
    t %= clock_units[3];
    return t / clock_units[4] * 10 + t % clock_units[4] / clock_units[5];
}

void func_80023AC4(void)
{
    menu_phase = 2;
}

/* The party, time and gil screen behind the commands, and its input: a command, a member for it, or Order's swaps. */
void func_80023AD4(int frame)
{
    RECT win;
    RECT r;
    PartyMember swap;
    int i;
    int x;
    int y;
    unsigned char rec;

    if (D_80062F90)
        func_80026090();
    menu_shown = D_8009CBDC.cmd_shown[0];
    menu_shown |= D_8009CBDC.cmd_shown[1] << 8;
    menu_locked = D_8009CBDC.cmd_locked[0];
    menu_locked |= D_8009CBDC.cmd_locked[1] << 8;
    if (D_8009D4F7 & 1)
        menu_locked |= 0x41;
    if (func_80023050() == 1 && menu_phase == 1) {
        func_800264A8(&menu_cursors[menu_cursor_at]);
        switch (menu_cursor_at) {
        case 0:
            if ((field_pad1.pressed & 0x20) && (menu_shown >> menu_cursors[0].row & 1)
                && !(menu_locked >> menu_cursors[0].row & 1)) {
                if (menu_cursors[0].row == 5) {
                    menu_cursor_at = 2;
                    order_picked = 0;
                    break;
                }
                func_8001FA28(1);
                switch (command_kind[menu_cursors[0].row]) {
                case 0:
                    menu_cursor_at = menu_cursor_at;
                    break;
                case 1:
                    func_8002305C(2, command_module[menu_cursors[0].row]);
                    func_8002120C(command_module[menu_cursors[0].row]);
                    if (menu_cursors[0].row == 8)
                        func_8001FA28(0x1C3);
                    break;
                case 2:
                    menu_cursor_at = 1;
                    break;
                }
            } else if (field_pad1.pressed & 0x40) {
                func_8001FA28(4);
                results_slide_step = 3;
                menu_phase = 2;
            }
            break;
        case 1:
            if (field_pad1.pressed & 0x20) {
                if (D_8009CBDC.party[menu_cursors[1].row] == 0xFF) {
                    func_8001FA28(3);
                    break;
                }
                func_8001FA28(1);
                func_8002305C(2, command_module[menu_cursors[0].row]);
                func_8002120C(command_module[menu_cursors[0].row]);
                menu_cursor_at = 0;
                menu_member = menu_cursors[1].row;
            } else if (field_pad1.pressed & 0x40) {
                func_8001FA28(4);
                menu_cursor_at = 0;
            }
            break;
        case 2:
            if (field_pad1.pressed & 0x20) {
                if (!order_picked) {
                    func_8001FA28(1);
                    order_picked++;
                    order_first = menu_cursors[2].row;
                    break;
                }
                order_picked = 0;
                if (order_first == menu_cursors[2].row) {
                    if (D_8009CBDC.party[order_first] == 0xFF)
                        break;
                    D_8009C738[character_record[D_8009CBDC.party[order_first]]].row ^= 1;
                    func_8001FA28(1);
                    break;
                }
                func_8001FA28(1);
                {
                    unsigned char t = D_8009CBDC.party[order_first];

                    D_8009CBDC.party[order_first] = D_8009CBDC.party[menu_cursors[2].row];
                    D_8009CBDC.party[menu_cursors[2].row] = t;
                }
                swap = members[menu_cursors[2].row];
                members[menu_cursors[2].row] = members[order_first];
                members[order_first] = swap;
                /* Their palette rows, then their portraits through a spare corner of VRAM. */
                r.x = 0x100;
                r.w = 0x100;
                r.h = 1;
                r.y = menu_cursors[2].row + 0x1ED;
                func_80044064(&r, &D_800756F8[order_first * 0x200]);
                r.x = 0x100;
                r.w = 0x100;
                r.h = 1;
                r.y = order_first + 0x1ED;
                func_80044064(&r, &D_800756F8[menu_cursors[2].row * 0x200]);
                r.x = 0x3C0;
                r.w = 0x18;
                r.h = 0x30;
                r.y = order_first * 48 + 0x138;
                MoveImage(&r, 0x340, 0x100);
                DrawSync(0);
                r.x = 0x3C0;
                r.w = 0x18;
                r.h = 0x30;
                r.y = menu_cursors[2].row * 48 + 0x138;
                MoveImage(&r, 0x3C0, order_first * 48 + 0x138);
                DrawSync(0);
                r.x = 0x340;
                r.y = 0x100;
                r.w = 0x18;
                r.h = 0x30;
                MoveImage(&r, 0x3C0, menu_cursors[2].row * 48 + 0x138);
                DrawSync(0);
                func_80025C94(D_800756F8);
                DrawSync(0);
            } else if (field_pad1.pressed & 0x40) {
                func_8001FA28(4);
                if (order_picked)
                    order_picked--;
                else
                    menu_cursor_at = 0;
            }
            break;
        }
    }
    if (func_80023050() == 1 && menu_phase == 1) {
        switch (menu_cursor_at) {
        case 0:
            func_8001EB2C(0x10B, menu_cursors[0].row * 12 + 13);
            break;
        case 2:
            if ((frame & 2) && order_picked)
                func_8001EB2C(-4, order_first * 60 + 43);
        case 1:
            if (frame & 2)
                func_8001EB2C(0x10B, menu_cursors[0].row * 12 + 13);
            func_8001EB2C(0, menu_cursors[menu_cursor_at].row * 60 + 47);
            break;
        }
    }
    r.x = 0;
    r.y = 5;
    r.w = 0x16C;
    r.h = 0xDB;
    func_80026A94(&results_draw[results_buf], &r);
    func_800230C4(results_buf);

    y = 0x11C - results_slide * 84 / 20;
    func_80026F44(0xC0, y + 6, D_8009D5F0, 7);
    func_8001DE0C(&win, 0xB8, y, 0xB4, 0x18);
    func_8001E040(&win);

    x = 0x1CE - results_slide * 184 / 20;
    func_80026F44(x + 4, 0xAA, menu_labels[0], 7);
    func_80026F44(x + 5, 0xB8, menu_labels[1], 7);
    func_80028E00(x + 0x1F, 0xAC, func_80023788(D_8009D264), 2, 7);
    func_8002708C(x + 0x2C, 0xAD, 0xD5, D_8009D26C > 0x7FFF ? 0 : 7);
    func_80029114(x + 0x32, 0xAC, func_8002382C(D_8009D264), 2, 7);
    func_8002708C(x + 0x3E, 0xAD, 0xD5, 7);
    func_80029114(x + 0x43, 0xAC, func_80023940(D_8009D264), 2, 7);
    func_80028E00(x + 0xB, 0xBA, save_gil, 10, 7);
    r.x = 0;
    r.y = 0;
    r.w = 0x100;
    r.h = 0x100;
    func_80026A34(0, 1, 0x7F, &r);
    func_8001DE0C(&win, x, 0xA4, 0x56, 0x24);
    func_8001E040(&win);

    x = results_slide * 184 / 20 - 0xB8;
    y = 17;
    for (i = 0; i < 3; i++) {
        int top;
        int shift;
        int r0, g0, b0;

        if (D_8009CBDC.party[i] == 0xFF)
            continue;
        rec = character_record[D_8009CBDC.party[i]];
        top = i * 60 + 0x12;
        shift = ((D_8009C738[rec].row ^ 1) & 1) * 24;
        func_80020B68(x + 0x66, i * 60 + 0x1F, i);
        func_8001CF3C(x + shift + 0x16, i * 60 + 0x1D, 0x30, 0x30, 0, i * 48 + 0x38, 0x30, 0x30, i + 0xD, 0);
        func_800285AC(x + 0xCB, i * 60 + 0x2E, D_8009C738[rec].exp_bar, 6, 0x80, 0x20, 0x20);
        if (D_8009C738[rec].limit_bar == 0xFF) {
            r0 = limit_colors[frame >> 1 & 7][0];
            g0 = limit_colors[frame >> 1 & 7][1];
            b0 = limit_colors[frame >> 1 & 7][2];
        } else if (D_8009C738[rec].flags & 0x10) {
            r0 = limit_colors[0][0];
            g0 = limit_colors[0][1];
            b0 = limit_colors[0][2];
        } else if (D_8009C738[rec].flags & 0x20) {
            r0 = limit_colors[2][0];
            g0 = limit_colors[2][1];
            b0 = limit_colors[2][2];
        } else {
            r0 = 0x80;
            g0 = 0x20;
            b0 = 0x50;
        }
        func_800285AC(x + 0xCB, top + 0x31, D_8009C738[rec].limit_bar * 61 / 256, 6, r0, g0, b0);
        func_80028CA0(x + 0xCA, top + 0x1B, 0x88, 8, 0x40, 8, 7, 0);
        func_80028CA0(x + 0xCA, top + 0x30, 0x88, 8, 0x40, 8, 7, 0);
        func_80028E00(x + 0xFA, top + 0x27, D_8009C738[character_record[D_8009CBDC.party[i]]].limit_level, 1, 7);
        r.x = 0;
        r.y = 0;
        r.w = 0x100;
        r.h = 0x100;
        func_80026A34(0, 1, 0x3F, &r);
        func_80026F44(x + 0xBA, top + 0x10, member_labels[0], 7);
        func_80026F44(x + 0xBA, top + 0x25, member_labels[1], 7);
    }
    func_8001DE0C(&win, x, y, 0x12C, 0xBE);
    func_8001E040(&win);

    switch (menu_phase) {
    case 0:
        results_slide += results_slide_step;
        if (results_slide < 20) {
            results_slide = 20;
            menu_phase = 1;
        }
        break;
    case 2:
        results_slide += results_slide_step;
        if (results_slide > 100)
            menu_phase = -1;
        break;
    }
}

void func_80024A04(void)
{
    func_800444AC(&results_disp[0]);
    PutDrawEnv(&results_draw[1]);
}

/* The menu: runs the current module each frame until it slides shut. With a demo script, input comes from that. */
int func_80024A3C(unsigned char *demo)
{
    RECT unused; /* this and b, c: unused, but the game's frame has them */
    DMA_BUFFER(vram, 448 * 4);
    RECT win;
    RECT b, c;
    unsigned short i;
    int frame;
    int state;

    menu_state = 1;
    menu_phase = 0;
    menu_command_prev = 0;
    menu_command = 0;
    menu_module_prev = 0;
    menu_module = 0;
    text_page_per_line = 0;
    if (!demo) {
        func_8001F6E4(0, 0, 0);
        demo_input = 0;
    } else {
        func_8001C484(demo);
        demo_input = 1;
    }
    i = D_8009CBDC.cmd_shown[0]; /* through i, as the game has it in that register */
    if (i & 4)
        D_80063024 = 1;
    else
        D_80063024 = 0;
    for (i = 0; i < 3; i++) {
        if (D_8009CBDC.party[i] != 0xFF) {
            func_80020058(i);
            member_update(i);
            party_effects_sum();
        }
    }
    func_80021044(results_draw, results_disp);
    frame = 0;
    results_slide = 100;
    results_slide_step = -3;
    func_80025CD4(vram);
    func_80025C94(D_800756F8);
    menu_phase = 0;
    func_8002368C();
    results_buf = 0;
    do {
        func_8001CB48();
        func_800269C0(D_80077F64[results_buf]);
        results_ot = menu_ots[results_buf];
        func_8004418C(results_ot, 20);
        func_80026A00(results_ot);
        if (demo_input && menu_phase != -1 && (frame & 0x10)) {
            func_80026F44(0x28, 0xCE, demo_text, 4);
            func_8001DE0C(&win, 0x20, 0xC8, func_80026B70(demo_text) + 0x10, 0x18);
            func_8001E040(&win);
        }
        func_8001F710();
        state = func_80023050();
        DATA_CALL(module_frames[state == 2 || state == 4 || state == 5 ? menu_module_prev : menu_module], frame);
        DrawSync(0);
        VSync(0);
        func_800444AC(&results_disp[results_buf]);
        PutDrawEnv(&results_draw[results_buf]);
        func_8004433C(results_ot);
        frame++;
        results_buf ^= 1;
    } while (menu_phase != -1);
    func_80025C94(vram);
    func_80024A04();
    demo_input = 0;
    VSync(4);
    DMA_BUFFER_END(448 * 4);
    return 0;
}

/* Each loads a menu overlay, waits for it, and runs it. */
void func_80024D88(int arg)
{
    func_800211C4(13);
    while (cd_step())
        ;
    VSync(30);
    func_801D131C(arg);
}

void func_80024DD4(int arg)
{
    func_800211C4(14);
    while (cd_step())
        ;
    func_801D1A6C(arg);
}

void func_80024E18(int arg)
{
    func_800211C4(15);
    while (cd_step())
        ;
    func_801D4118(arg);
}

void func_80024E5C(void)
{
    func_800211C4(16);
    while (cd_step())
        ;
    func_801D4CC0();
}

void func_80024E94(void)
{
    func_800211C4(10);
    while (cd_step())
        ;
    func_801D1774();
}

void func_80024ECC(void)
{
    func_800211C4(1);
    while (cd_step())
        ;
    func_801D2D74();
}

void func_80024F04(void)
{
    func_800211C4(1);
    while (cd_step())
        ;
    func_801D2E84();
}

void func_80024F3C(int arg)
{
    func_800211C4(1);
    while (cd_step())
        ;
    func_801D2F00(arg);
}

void func_80024F80(int arg)
{
    func_800211C4(1);
    while (cd_step())
        ;
    func_801D3138(arg);
}

void func_80024FC4(int arg)
{
    func_800211C4(1);
    while (cd_step())
        ;
    func_801D3018(arg);
}

void func_80025008(void)
{
    func_800211C4(1);
    while (cd_step())
        ;
    func_801D3228();
}

void func_80025040(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (D_8009D78A[0] >> D_80049500[i] & 1)
            D_8009D44C[i] = D_8009C738[D_80049500[i]].level;
    }
}

void func_800250B4(void)
{
    func_800211C4(12);
    while (cd_step())
        ;
    func_801D027C();
}

void func_800250EC(int arg)
{
    func_800211C4(12);
    while (cd_step())
        ;
    func_801D05C4(arg);
}

void func_80025130(int arg)
{
    func_800211C4(12);
    while (cd_step())
        ;
    func_801D0704(arg);
}

/* A test entry with the buttons held at boot choosing a screen; with none, the menu with every command shown. */
void func_80025174(void)
{
    unsigned int held = func_8001C808();
    int i, j;

    if (held & 4) {
        func_80024E18(0);
    } else if (held & 8) {
        func_80024E5C();
        for (i = 0; i < 5; i++) {
            for (j = 0; j < 6; j++)
                inventory[319 - j - i * 6] = (j + 0x47) | 99 << 9;
        }
        func_80024E94();
    } else if (held & 1) {
        func_80024DD4(0);
    } else if (held & 2) {
        for (i = 0; i < 9; i++)
            func_80024D88(i);
        func_80024D88(100);
    } else {
        D_8009CBDC.cmd_shown[0] = 0xFF;
        D_8009CBDC.cmd_shown[1] = 0xFF;
        func_80024A3C(0);
    }
}
