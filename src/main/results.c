/* The battle's results screens, 0x800211B8-0x80023050: EXP and AP counted into the party with level ups, materia
 * and limits learned, then gil and the items won; and before them the menus' module loading. GCC 2.7.2, ASPSX 2.21,
 * -G8. */

#include "common.h"
#include "main.h"
#include "prims.h"

extern PartyMember members[3];

/* A menu's cursor as func_800264A8 moves it. */
typedef struct MenuCursor {
    short pad0;
    short top;   /* the first row shown */
    short pad4;
    short total; /* rows in all */
    short scrolling;
    unsigned char col;
    signed char row;
    unsigned char cols, rows; /* shown */
    unsigned char padE;
    unsigned char scroll_dir;
    unsigned char wrap_x, wrap_y;
} MenuCursor;

/* A member's level ups on the EXP screen, each message shown for its timer's frames. */
typedef struct LevelUp {
    unsigned char level;   /* as last drawn */
    unsigned char materia; /* the next slot to look at for a materia that levelled */
    unsigned char materia_timer;
    unsigned char level_timer;
    unsigned char limit; /* the next limit slot to look at */
    unsigned char limit_timer;
} LevelUp;

int menu_module_prev;
int menu_module;
short results_phase; /* 0-1 EXP and AP, 2-3 gil and items, 4 fading out, 5 done */
unsigned char results_counting;
void *module_dest;
int results_items; /* any items won */
unsigned int *results_ot;
short results_slide; /* the EXP screen's windows slide in from 100 to 20 */
short results_fade;
short results_drawn;
unsigned char results_leaving;
unsigned char mastered_count; /* materia mastered in the battle, mastered[] */
unsigned char mastered_shown;
unsigned char mastered_next;
unsigned char mastered_timer;
int results_buf;
unsigned char D_80062F98;
unsigned char results_frame;
short results_slide_step;
short results_fade_step;

extern DiscFile menu_files[];
extern unsigned char mastered[];
extern LevelUp level_ups[3];
extern MenuCursor spoils_cursor;
extern unsigned char spoils_cursor_y[3];
extern TILE fade_tile[2];
extern unsigned int results_ots[2][20];
extern DRAWENV results_draw[2];
extern DISPENV results_disp[2];
extern unsigned char D_80077F64[2][0x3400];
extern unsigned char D_800756F8[];
extern short D_80095DD4;
extern unsigned char D_80062DFD;
extern unsigned char D_80049338[]; /* "LEVEL UP" */
extern struct {
    int gil;
    unsigned int clock[4]; /* text_draw.c's D_8009D264 */
} save_gil;
extern struct {
    int exp, ap, gil;
} battle_won;
extern unsigned short materia_levelled[];
extern struct {
    unsigned short held, prev, pressed, repeat;
} field_pad1;

unsigned char *kind_text(int kind, int index, int names);
int exp_tally_step(int skip);
void add_spoils(void);
int limit_id(unsigned int character, int slot);
void func_8001CB48(void);
void func_8001CF3C(short x, short y, short w, short h, unsigned short u, unsigned short v, unsigned short tw,
                   unsigned short th, short pal, short semi);
void func_8001DE0C(RECT *r, int x, int y, int w, int h);
void func_8001E040(RECT *r);
void func_8001FA28(unsigned short effect);
void func_8001FA68(unsigned short effect);
void func_8001FAAC(unsigned short effect);
void func_80021044(DRAWENV *draw, DISPENV *disp);
void func_80025C94(void *p);
short func_8002603C(int materia); /* its icon's palette */
void func_800264A8(MenuCursor *cursor);
void func_800269C0(void *buf);
void func_80026A00(unsigned int *ot);
void func_80026A34(int dfe, int dtd, int tpage, RECT *tw);
void func_80026F44(int x, int y, unsigned char *text, int color);
void func_800285AC(int x, int y, int value, int a3, int a4, int a5, int a6);
void func_80028CA0(short x, short y, int u, int v, int w, int h, int pal, int semi);
void func_80028E00(int x, int y, int n, int digits, int color);
void func_80043938(int mode);
void func_8004418C(unsigned int *ot, int n);
void func_8004433C(unsigned int *ot); /* probably DrawOTag */
void func_800444AC(DISPENV *env);     /* probably PutDispEnv */
void SetTile(void *p);
void PutDrawEnv(DRAWENV *env);
void SetDispMask(int mask);
int DrawSync(int mode);
int VSync(int mode);
int cd_read(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int cd_step(void);
unsigned short GetTPage(int tp, int abr, int x, int y);

void func_800211B8(void *dest)
{
    module_dest = dest;
}

#ifdef NON_MATCHING
void func_800211C4(int module)
{
    DiscFile *files = menu_files;

    cd_read(files[module].lba, files[module].size, module_dest, 0);
    cd_step();
}
#else
INCLUDE_ASM("asm/nonmatchings/main/results", func_800211C4);
#endif

/* Loads a menu module unless it's 0 or the one wanted is already in (3 and 4 share theirs). */
void func_8002120C(int module)
{
    int prev = menu_module;

    menu_module = module;
    menu_module_prev = prev;
    if (module && !((prev == 3 || prev == 4) && (module == 3 || module == 4)))
        func_800211C4(module);
}

unsigned char *func_80021258(int materia)
{
    return kind_text(13, materia, 8);
}

unsigned char *func_80021280(int item)
{
    return kind_text(4, item, 8);
}

/* Gil and the items won: take them across one at a time or all at once, then leave by the bottom row. */
#ifdef NON_MATCHING
void func_800212A8(void)
{
    RECT tw;
    RECT r;
    RECT unused;
    short i;
    char old, row;
    int delta, x;
    short y;

    if (results_phase == 2 && (field_pad1.repeat & 0x820) && !results_fade && spoils_cursor.row == 5) {
        func_8001FA28(1);
        if (battle_won.gil) {
            save_gil.gil += battle_won.gil;
            battle_won.gil = 0;
        }
        results_phase = 3;
        results_fade_step = -results_fade_step;
    }
    results_fade += results_fade_step;
    if (results_fade <= 0 && results_phase != 3) {
        results_phase = 2;
        results_fade = 0;
        if (results_leaving)
            results_leaving = 0;
        if (results_items) {
            old = spoils_cursor.row;
            func_800264A8(&spoils_cursor);
            row = spoils_cursor.row;
            if (row != 0 && row != 5) {
                delta = row - old;
                for (;;) {
                    if (spoils[spoils_cursor.row - 1].id != -1)
                        break;
                    spoils_cursor.row += delta;
                    if (spoils_cursor.row == 0 || spoils_cursor.row == 5)
                        break;
                }
            }
            if (field_pad1.pressed & (field_pad1.pressed & 0x820)) {
                if (!spoils_cursor.row) {
                    func_8001FA28(1);
                    for (i = 0; i < 4; i++)
                        spoils[i].won = 1;
                    spoils_cursor.row = 5;
                } else if (spoils_cursor.row != 5) {
                    func_8001FA28(1);
                    spoils[spoils_cursor.row - 1].won ^= 1;
                }
            }
        }
        if (battle_won.gil) {
            battle_won.gil--;
            save_gil.gil++;
            if (results_frame & 2)
                func_8001FA68(0x2C);
        }
    } else {
        TILE *tiles = fade_tile;

        SetTile(&tiles[results_buf]);
        func_80046848(&tiles[results_buf], 1);
        tiles[results_buf].x0 = 0;
        tiles[results_buf].y0 = 0;
        tiles[results_buf].w = 384;
        tiles[results_buf].h = 224;
        tiles[results_buf].r0 = results_fade;
        tiles[results_buf].g0 = results_fade;
        tiles[results_buf].b0 = results_fade;
        AddPrim(results_ot, &tiles[results_buf]);
        tw.x = 0;
        tw.y = 0;
        tw.w = 256;
        tw.h = 256;
        func_80026A34(0, 1, 0x5F, &tw);
        if (results_fade >= 256 && results_phase == 3) {
            results_fade = 255;
            results_phase = 4;
        }
    }
    func_80028CA0(336, 40, 0xF8, 16, 8, 8, 7, 0);
    func_80028E00(266, 40, save_gil.gil, 10, 7);
    func_80028CA0(160, 40, 0xF8, 16, 8, 8, 7, 0);
    func_80028E00(111, 40, battle_won.gil, 7, 7);
    if (results_items) {
        if (!spoils_cursor.row) {
            y = spoils_cursor_y[0];
            x = 0;
        } else if (spoils_cursor.row == 5) {
            y = spoils_cursor_y[2];
            x = 0;
        } else {
            x = 20;
            y = spoils_cursor_y[1] + (spoils_cursor.row - 1) * 32;
        }
        func_80028CA0(x, y, 0xE0, 8, 24, 16, 1, 1);
        for (i = 0; i < 4; i++) {
            if (spoils[i].id == -1)
                continue;
            func_80028E00(156, i * 32 + 85, spoils[i].count, 2, spoils[i].won ? 0 : 7);
            if (spoils[i].won)
                func_80028E00(332, i * 32 + 85, spoils[i].count, 2, 7);
        }
    }
    tw.x = 0;
    tw.y = 0;
    tw.w = 256;
    tw.h = 256;
    func_80026A34(0, 1, 0x5F, &tw);
    func_80026F44(32, 38, kind_text(8, 0x18, 8), 7);
    func_80026F44(200, 38, kind_text(8, 0x19, 8), 7);
    if (results_items) {
        func_80026F44(16, 12, kind_text(8, 0x15, 8), 7);
        func_80026F44(24, 62, kind_text(8, 0x1A, 8), 7);
        func_80026F44(192, 62, kind_text(8, 0x1C, 8), 7);
        func_80026F44(24, 202, kind_text(8, 0x1B, 8), 7);
        for (i = 0; i < 4; i++) {
            if (spoils[i].id == -1)
                continue;
            func_80026F44(48, i * 32 + 83, kind_text(4, spoils[i].id, 8), spoils[i].won ? 0 : 7);
            if (spoils[i].won)
                func_80026F44(224, i * 32 + 83, kind_text(4, spoils[i].id, 8), 7);
        }
    } else {
        func_80026F44(16, 12, kind_text(8, 0x16, 8), 7);
        func_80026F44(24, 62, kind_text(8, 0x1D, 8), 7);
    }
    func_8001DE0C(&r, 0, 0, 364, 32);
    func_8001E040(&r);
    func_8001DE0C(&r, 0, 32, 184, 24);
    func_8001E040(&r);
    func_8001DE0C(&r, 176, 32, 188, 24);
    func_8001E040(&r);
    if (results_items) {
        func_8001DE0C(&r, 0, 56, 184, 168);
        func_8001E040(&r);
        func_8001DE0C(&r, 176, 56, 188, 168);
    } else {
        func_8001DE0C(&r, 0, 56, 364, 168);
    }
    func_8001E040(&r);
}
#else
INCLUDE_ASM("asm/nonmatchings/main/results", func_800212A8);
#endif

/* "LEVEL UP". */
void func_80021BAC(int x, int y)
{
    RECT tw;
    RECT r;

    tw.x = 0;
    tw.y = 0;
    tw.w = 256;
    tw.h = 256;
    func_80026A34(0, 1, 0x7F, &tw);
    func_80026F44(x + 8, y + 6, D_80049338, 6);
    func_8001DE0C(&r, x, y, 80, 20);
    func_8001E040(&r);
}

/* A materia's level up. */
void func_80021C4C(int x, int y, int materia)
{
    RECT tw;
    RECT r;

    func_80028CA0(x + 8, y + 5, 0x80, 0x20, 16, 16, func_8002603C(materia), 0);
    tw.x = 0;
    tw.y = 0;
    tw.w = 256;
    tw.h = 256;
    func_80026A34(0, 1, 0x7F, &tw);
    func_80026F44(x + 24, y + 5, func_80021258(materia), 7);
    func_80026F44(x + 56, y + 18, D_80049338, 6);
    func_8001DE0C(&r, x, y, 130, 32);
    func_8001E040(&r);
}

/* A materia mastered, as mastered_shown. */
void func_80021D5C(int x, int y)
{
    RECT tw;
    RECT r;

    func_80028CA0(x + 8, y + 5, 0x80, 0x20, 16, 16, func_8002603C(mastered_shown), 0);
    tw.x = 0;
    tw.y = 0;
    tw.w = 256;
    tw.h = 256;
    func_80026A34(0, 1, 0x7F, &tw);
    func_80026F44(x + 32, y + 18, kind_text(8, 0x25, 8), 7);
    func_80026F44(x + 24, y + 5, func_80021258(mastered_shown), 7);
    func_8001DE0C(&r, x, y, 130, 35);
    func_8001E040(&r);
}

/* A limit learned. */
void func_80021E70(int x, int y, int slot, int limit)
{
    RECT tw; /* unused, but the game's has it on the stack */
    RECT r;

    func_80026F44(x + 47, y + 18, kind_text(8, 0x24, 8), 7);
    func_80026F44(x + 8, y + 5, kind_text(3, limit_id(character_record[D_8009CBDC[slot]], limit), 8), 6);
    func_8001DE0C(&r, x, y, 130, 35);
    func_8001E040(&r);
}

/* EXP and AP counted into the party, with each member's level ups, materia levels and limits learned. */
#ifdef NON_MATCHING
void func_80021F58(void)
{
    RECT tw;
    RECT r;
    int busy, top, exp_x, ap_x, exp_y, ap_y, color;
    short i, my;
    unsigned char rec;

    if (results_counting)
        busy = exp_tally_step(field_pad1.pressed);
    top = 32 - results_slide * 32 / 20;
    exp_x = 184 - results_slide * 184 / 20;
    ap_x = results_slide * 176 / 20;
    exp_y = 32;
    ap_y = 32;
    if (results_counting) {
        if (!mastered_timer && mastered_next < mastered_count) {
            func_8001FAAC(0x1E1);
            mastered_timer = 150;
            mastered_shown = mastered[mastered_next];
            mastered_next++;
        }
        if (mastered_timer) {
            func_80021D5C(0x75, top + 12);
            mastered_timer--;
        }
    }
    func_80028CA0(exp_x + 158, exp_y + 8, 0xF8, 8, 8, 8, 7, 0);
    func_80028E00(exp_x + 95, exp_y + 8, battle_won.exp, 9, 7);
    func_80028CA0(ap_x + 158, ap_y + 8, 0xF8, 8, 8, 8, 7, 0);
    func_80028E00(ap_x + 95, ap_y + 8, battle_won.ap, 9, 7);
    tw.x = 0;
    tw.y = 0;
    tw.w = 256;
    tw.h = 256;
    func_80026A34(0, 1, GetTPage(0, 1, 960, 256), &tw);
    func_80026F44(16, top + 12, kind_text(8, 0x10, 8), 7);
    func_80026F44(exp_x + 32, exp_y + 6, kind_text(8, 0x13, 8), 7);
    func_80026F44(ap_x + 48, ap_y + 6, kind_text(8, 0x14, 8), 7);
    func_8001DE0C(&r, 0, top, 364, 32);
    func_8001E040(&r);
    func_8001DE0C(&r, exp_x, exp_y, 184, 24);
    func_8001E040(&r);
    func_8001DE0C(&r, ap_x, ap_y, 188, 24);
    func_8001E040(&r);
    for (i = 0; i < 3; i++) {
        my = (i * 56 + 56) * results_slide / 20;
        rec = character_record[D_8009CBDC[i]];
        if (D_8009CBDC[i] != 0xFF && tally_shown[i].level) {
            if (level_ups[i].level != tally_shown[i].level) {
                func_8001FA28(0x180);
                level_ups[i].level_timer = 80;
            }
            if (level_ups[i].level_timer) {
                func_80021BAC(36, my + 30);
                level_ups[i].level_timer--;
            }
            level_ups[i].level = tally_shown[i].level;
            if (results_counting) {
                if (!level_ups[i].materia_timer) {
                materia:
                    if (level_ups[i].materia != 16) {
                        if (!(materia_levelled[i] >> level_ups[i].materia & 1)) {
                            level_ups[i].materia++;
                            goto materia;
                        }
                        func_8001FAAC(0x1E1);
                        level_ups[i].materia_timer = 80;
                        materia_levelled[i] &= ~(1 << level_ups[i].materia);
                        level_ups[i].materia++;
                    }
                }
                if (!level_ups[i].limit_timer && !level_ups[i].materia_timer) {
                limit:
                    if (level_ups[i].limit != 10) {
                        if (!(tally_shown[i].new_limits >> level_ups[i].limit & 1)) {
                            level_ups[i].limit++;
                            goto limit;
                        }
                        func_8001FAAC(0x1E1);
                        level_ups[i].limit_timer = 80;
                        level_ups[i].limit++;
                    }
                }
            }
            /* The slot already stepped past it; the weapon's slots run on into the armor's. */
            if (level_ups[i].materia_timer) {
                func_80021C4C(0x75, my + 4, (unsigned char)D_8009C738[rec].weapon_materia[level_ups[i].materia - 1]);
                level_ups[i].materia_timer--;
            }
            if (level_ups[i].limit_timer) {
                func_80021E70(0x75, my + 4, i, level_ups[i].limit - 1);
                level_ups[i].limit_timer--;
            }
            color = !members[i].hp ? 2 : members[i].hp < members[i].max_hp / 4 ? 6 : 7;
            func_80026F44(107, my + 34, kind_text(8, 0x2B, 8), color);
            func_80026F44(184, my + 34, kind_text(8, 0x2C, 8), color);
            func_80026F44(226, my + 9, kind_text(8, 9, 8), color);
            func_80026F44(88, my + 11, D_8009C738[rec].name, color);
            func_800285AC(281, my + 25, tally_shown[i].bar, 6, 0x80, 0x20, 0x20);
            func_80028CA0(280, my + 24, 0x88, 8, 64, 8, 7, 0);
            func_80028CA0(338, my + 12, 0xF8, 8, 8, 8, color, 0);
            func_80028CA0(338, my + 36, 0xF8, 8, 8, 8, color, 0);
            func_80028E00(267, my + 12, tally_shown[i].exp, 10, color);
            func_80028E00(267, my + 36, tally_shown[i].to_next, 10, color);
            func_80028E00(145, my + 36, tally_shown[i].level, 2, color);
            tw.x = 0;
            tw.y = 0;
            tw.w = 256;
            tw.h = 256;
            func_80026A34(0, 1, 0x3F, &tw);
            func_8001CF3C(22, my + 4, 48, 48, 0, i * 48 + 56, 48, 48, i + 13, 0);
            tw.x = 0;
            tw.y = 0;
            tw.w = 256;
            tw.h = 256;
            func_80026A34(0, 1, 0x7F, &tw);
        }
        func_8001DE0C(&r, 0, my, 364, 56);
        func_8001E040(&r);
    }
    if (results_slide == 20 && results_counting && field_pad1.pressed && !busy) {
        results_leaving = 1;
        results_fade_step = -6;
        results_phase = 1;
        results_fade = 255;
        results_slide_step = -results_slide_step;
    }
    results_slide += results_slide_step;
    if (!results_phase && results_slide < 20) {
        results_slide = 20;
        if (field_pad1.pressed) {
            results_counting = 1;
            for (i = 0; i < 4; i++)
                spoils[i].won = 0;
        }
    }
    if (results_slide > 100)
        results_slide = 100;
    if (results_counting && busy && (results_frame & 2))
        func_8001FA68(0x2D);
}
#else
INCLUDE_ASM("asm/nonmatchings/main/results", func_80021F58);
#endif

void func_80022FE0(void);

/* A frame of the results screens. */
void func_80022B5C(void)
{
    RECT tw;

    if (DrawSync(1))
        return;
    func_800444AC(&results_disp[results_buf]);
    PutDrawEnv(&results_draw[results_buf]);
    if (!results_drawn) {
        results_drawn = 1;
        func_80043938(1);
        func_80025C94(D_800756F8);
    } else {
        func_8004433C(results_ot);
    }
    results_buf ^= 1;
    if (results_phase == 5) {
        D_80062F98 = 0xFF;
        func_80022FE0();
        D_80095DD4 = 0;
    }
    func_8001CB48();
    results_ot = results_ots[results_buf];
    func_8004418C(results_ot, 20);
    func_800269C0(D_80077F64[results_buf]);
    func_80026A00(results_ot);
    switch (results_phase) {
    case 0:
        func_80021F58();
        break;
    case 1:
        func_80021F58();
    case 2:
    case 3:
        func_800212A8();
        break;
    case 4: {
        TILE *tiles = fade_tile;

        SetTile(&tiles[results_buf]);
        func_80046848(&tiles[results_buf], 1);
        tiles[results_buf].x0 = 0;
        tiles[results_buf].y0 = 0;
        tiles[results_buf].w = 384;
        tiles[results_buf].h = 224;
        tiles[results_buf].r0 = results_fade;
        tiles[results_buf].g0 = results_fade;
        tiles[results_buf].b0 = results_fade;
        AddPrim(results_ot, &tiles[results_buf]);
        tw.x = 0;
        tw.y = 0;
        tw.w = 256;
        tw.h = 256;
        func_80026A34(0, 1, 0x5F, &tw);
        results_phase = 5;
        break;
    }
    }
    results_frame++;
}

/* The results screens' start. */
#ifdef NON_MATCHING
void func_80022DE4(void)
{
    RECT unused;
    int i;

    results_drawn = 0;
    D_80062DFD = 1;
    exp_tally_step(0);
    mastered_next = 0;
    mastered_timer = 0;
    for (i = 0; i < 3; i++) {
        level_ups[i].level = tally_shown[i].level;
        level_ups[i].materia = 0;
        level_ups[i].materia_timer = 0;
        level_ups[i].level_timer = 0;
        level_ups[i].limit = 0;
        level_ups[i].limit_timer = 0;
    }
    results_slide = 100;
    results_buf = 0;
    results_phase = 0;
    results_counting = 0;
    D_80062F98 = 0;
    results_slide_step = -3;
    func_80021044(results_draw, results_disp);
    results_items = 0;
    results_ot = results_ots[results_buf];
    for (i = 0; i < 4; i++)
        if (spoils[i].id != -1)
            results_items = 1;
    spoils_cursor.rows = 6;
    spoils_cursor.pad4 = 1;
    spoils_cursor.col = 0;
    spoils_cursor.pad0 = 0;
    spoils_cursor.top = 0;
    spoils_cursor.cols = 1;
    spoils_cursor.total = 6;
    spoils_cursor.wrap_x = 0;
    spoils_cursor.wrap_y = 1;
    spoils_cursor.padE = 0;
    spoils_cursor.scroll_dir = 0;
    spoils_cursor.scrolling = 0;
    spoils_cursor.row = results_items ? 0 : 5;
    SetDispMask(1);
    VSync(40);
}
#else
INCLUDE_ASM("asm/nonmatchings/main/results", func_80022DE4);
#endif

/* The results screens' end: the spoils into the inventory. */
void func_80022FE0(void)
{
    add_spoils();
    func_800444AC(&results_disp[results_buf]);
    PutDrawEnv(&results_draw[results_buf]);
}
