/* Text and windows, 0x8001D6A8-0x8001FA28: the dialogue text (12x12 glyphs from the font at VRAM (896, 256), the pad's
 * button icons, and codes for colour, blinking and spacing), window frames and backgrounds from WINDOW.BIN's texture
 * at (960, 256), the countdown and number displays, the field's dialogue windows and the demo's caption window.
 * GCC 2.7.2, ASPSX 2.21, -G8, and --expand-div for the digits' divisions. */

#include "common.h"
#include "main.h"
#include "prims.h"

/* A field dialogue window. */
typedef struct Window {
    unsigned char *text;
    short x, y;
    short w, h;           /* its size open */
    short open_w, open_h; /* and while it opens */
    short scroll;         /* the text's y */
    short shown;          /* characters of it to show */
    short count;          /* text_count after drawing */
    short lines;          /* text_lines after drawing */
    unsigned char more;   /* the text runs past the window */
    unsigned char flags;  /* bit 0: no frame, 1: background semi-transparent, 2: text semi-transparent */
    unsigned char arrow;  /* nonzero: the arrow at arrow_x, arrow_y */
    unsigned char extra;  /* 1: the countdown at extra_x, extra_y; 2: number there */
    unsigned char pal;    /* passed to the number display, which ignores it */
    unsigned char digits; /* number's */
    unsigned char pad1E[2];
    int number;
    short arrow_x, arrow_y;
    short extra_x, extra_y;
    short visible;
    short pad2E;
} Window;

short text_color;   /* palette 0-7 */
short text_blink;   /* nonzero: the text blinks */
short text_shimmer; /* nonzero: the palette cycles along the line */
short text_frame;   /* counts frames, for both */
short text_left;    /* characters still to show */
int text_semi;      /* nonzero: drawn semi-transparent */
int text_fixed;     /* nonzero: every glyph 13 pixels wide */
int text_lines;     /* lines drawn */
int text_count;     /* bytes of text read */
int window_semi;    /* nonzero: window backgrounds are semi-transparent */

/* The caption window the demo script shows. */
unsigned char caption_state;  /* 0: closed, 1: opening, 2: open, 3: closing */
unsigned char caption_color;
int caption_timer;            /* frames before a button may close it */
unsigned char *caption_text;
unsigned char caption_placed; /* nonzero: at caption_x, caption_y and closed only by a button */
unsigned char caption_step;   /* how far open, 1-3 */
short caption_x;
short caption_y;

extern unsigned char *D_800707C0; /* the font's widths: by glyph, the left gap in bits 5-7 and the width below */
extern unsigned char D_80049208[12]; /* the window's four corner colours */
extern unsigned char D_800696F0[12]; /* and a saved copy */
extern int D_80049214[4];            /* 600, 60, 10, 1: a time's digits in seconds */
extern int D_80049224[8];            /* 10000000 down to 1: a number's digits */

extern unsigned char D_80062DFD; /* set each time the dialogue windows draw */
extern unsigned char D_80049384[12]; /* the caption window's corner colours */
extern unsigned char D_80077F64[2][0x3400]; /* the primitive buffers, one per frame */
/* The game clock: the play time and the countdown the field can start, in seconds, and the fractions main.c adds to
 * them each frame. Volatile, as main.c has it. */
extern volatile struct {
    int play;
    int countdown;
    unsigned int play_frac;
    unsigned int countdown_frac;
} D_8009D264;

void func_80026A34(int dfe, int dtd, int tpage, RECT *tw); /* adds a drawing mode */
void func_80026A00(void *ot);                              /* sets prim_ot */
void func_800269C0(void *buf);                             /* sets prim_next */
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* probably SetDefDrawEnv */
void func_80044AC0(DR_ENV *dr, DRAWENV *env);                 /* probably SetDrawEnv */
int func_80026B70(unsigned char *text);                         /* text's width in pixels */
void func_80026F44(int x, int y, unsigned char *text, int color); /* draws a line of text */
int func_8001C498(void); /* pad 1's buttons; pad.c returns them as unsigned short */

/* Draws text from (x, y), wrapping before right; returns the y it ends on. 0xFA-0xFD switch to another page of the
 * font for the next glyph, 0xFE begins a code, 0xE7 is a new line and 0xF6-0xF9 are the pad's ○ △ □ ✕. */
#ifdef NON_MATCHING
/* Same code, but GCC gives the saved registers to the locals in another order. The loop is a goto loop: as a for
 * loop, GCC hoists right's sign extension out of it, where the game extends it at each use. */
short func_8001D6A8(short x, short y, short right, unsigned char *text)
{
    RECT tw;
    short i, k;
    unsigned char c;
    short ch, row, col;
    short pal_x, page, clut_y, u;
    int v, index;

    k = 0;
    i = 0;
loop:
    {
        c = *text;
        if (!text_left || c == 0xFF || c == 0xE8 || c == 0xE9)
            goto done;
        if (c == 0xE7) {
            x = 8;
            y += 16;
            text++;
            text_lines++;
            text_count++;
            goto next;
        }
        pal_x = 0;
        switch (c) {
        case 0xFA:
            text++;
            v = 132;
            page = 0xE7;
            text_count++;
            break;
        case 0xFB:
            text++;
            v = 0;
            pal_x = 16;
            page = 0x1B9;
            text_count++;
            break;
        case 0xFC:
            text++;
            v = 132;
            pal_x = 16;
            page = 0x2A0;
            text_count++;
            break;
        case 0xFD:
            text++;
            v = 132;
            page = 0x372;
            text_count++;
            break;
        case 0xFE:
            text_count++;
            text++;
            if (*text < 0xD2) {
                v = 132;
                pal_x = 16;
                page = 0x444;
                break;
            }
            text_count++;
            if (*text < 0xDA) {
                text_color = *text - 0xD2;
                text++;
                goto next;
            }
            if (*text == 0xDA) {
                text_blink ^= 1;
                text++;
                goto next;
            }
            if (*text == 0xDB) {
                text_shimmer ^= 1;
                text++;
                goto next;
            }
            if (*text == 0xE9) {
                text_fixed ^= 1;
                text++;
                goto next;
            }
            break;
        default:
            if (c >= 0xF6 && c <= 0xF9) {
                setRECT(&tw, 0, 0, 256, 256);
                func_80026A34(0, 1, GetTPage(0, 1, 896, 256), &tw);
                setlen(prim_next.sprt, 4);
                setcode(prim_next.sprt, 0x64);
                func_80046870(prim_next.sprt, 1);
                if (text_semi)
                    func_80046848(prim_next.sprt, 1);
                prim_next.sprt->x0 = x;
                prim_next.sprt->y0 = y - 2;
                switch (*text - 0xF6) {
                case 0:
                    prim_next.sprt->u0 = 0x60;
                    prim_next.sprt->v0 = 0x40;
                    break;
                case 1:
                    prim_next.sprt->u0 = 0x90;
                    prim_next.sprt->v0 = 0x40;
                    break;
                case 2:
                    prim_next.sprt->u0 = 0x80;
                    prim_next.sprt->v0 = 0x40;
                    break;
                case 3:
                    prim_next.sprt->u0 = 0x70;
                    prim_next.sprt->v0 = 0x40;
                    break;
                }
                prim_next.sprt->w = 16;
                prim_next.sprt->h = 16;
                prim_next.sprt->clut = GetClut(256, 490);
                AddPrim(prim_ot, prim_next.sprt++);
                text++;
                setRECT(&tw, 0, 0, 256, 256);
                text_left--;
                func_80026A34(0, 1, GetTPage(0, 1, 960, 256), &tw);
                text_count++;
                x += 16;
                goto next;
            }
            page = 0;
            v = 0;
        }
        if (!text_blink) {
            if (!text_shimmer)
                clut_y = text_color + 0x1F0;
            else
                clut_y = ((text_frame >> 2) - k) & 7 | 0x1F0;
        } else if (text_shimmer)
            clut_y = ((text_frame >> 2) - k) & 7 | 0x1F0;
        else if (text_frame >> 2 & 1)
            clut_y = text_color + 0x1F0;
        else if (text_color)
            clut_y = 0x1F0;
        else {
            x += pal_x;
            goto next;
        }
        ch = *text;
        col = ch % 21;
        row = ch / 21;
        u = col * 12;
        v += row * 12;
        index = ch + page;
        if (x + ((D_800707C0[index] >> 5) + (D_800707C0[index] & 0x1F)) > right) {
            x = 8;
            y += 16;
            text_lines++;
        }
        if (!text_fixed)
            x += D_800707C0[index] >> 5;
        setlen(prim_next.sprt, 4);
        setcode(prim_next.sprt, 0x64);
        func_80046870(prim_next.sprt, 1);
        if (text_semi)
            func_80046848(prim_next.sprt, 1);
        prim_next.sprt->x0 = x;
        prim_next.sprt->y0 = y;
        prim_next.sprt->u0 = u;
        prim_next.sprt->v0 = v;
        prim_next.sprt->w = 12;
        prim_next.sprt->h = 12;
        prim_next.sprt->clut = GetClut(pal_x | 0x100, clut_y);
        AddPrim(prim_ot, prim_next.sprt++);
        if (!text_fixed)
            x += D_800707C0[index] & 0x1F;
        else
            x += 13;
        text++;
        text_left--;
        text_count++;
    }
next:
    i++;
    k++;
    if (i < 1024)
        goto loop;
done:
    setRECT(&tw, 0, 0, 256, 256);
    func_80026A34(0, 1, GetTPage(0, 1, 896, 256), &tw);
    return y;
}
#else
INCLUDE_ASM("asm/nonmatchings/main/text_draw", func_8001D6A8);
#endif

void func_8001DE0C(RECT *r, int x, int y, int w, int h)
{
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
}

void func_8001DE24(RECT *r, short dx, short dy)
{
    r->x += dx;
    r->y += dy;
}

void func_8001DE40(RECT *dst, RECT *src)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->w = src->w;
    dst->h = src->h;
}

/* Saves the window colours. */
void func_8001DE70(void)
{
    int i;

    for (i = 0; i < 12; i++)
        D_800696F0[i] = D_80049208[i];
}

/* Restores them. */
void func_8001DEB0(void)
{
    int i;

    for (i = 0; i < 12; i++)
        D_80049208[i] = D_800696F0[i];
}

/* Sets them. */
void func_8001DEF0(unsigned char *colors)
{
    int i;

    for (i = 0; i < 12; i++)
        D_80049208[i] = *colors++;
}

/* A plain rectangle in one colour. */
void func_8001DF24(RECT *r, unsigned char red, unsigned char green, unsigned char blue)
{
    setlen(prim_next.tile, 3);
    setcode(prim_next.tile, 0x60);
    func_80046870(prim_next.tile, 1);
    prim_next.tile->x0 = r->x;
    prim_next.tile->y0 = r->y;
    prim_next.tile->w = r->w;
    prim_next.tile->h = r->h;
    prim_next.tile->r0 = red;
    prim_next.tile->g0 = green;
    prim_next.tile->b0 = blue;
    AddPrim(prim_ot, prim_next.tile++);
}

/* A window: its frame from WINDOW.BIN's texture, edges tiled through a texture window, and its background shaded
 * from the four corner colours. */
void func_8001E040(RECT *r)
{
    RECT tw;
    int x, y, w, h;

    if (r->w > 8) {
        setlen(prim_next.sprt, 4);
        setcode(prim_next.sprt, 0x64);
        func_80046870(prim_next.sprt, 1);
        prim_next.sprt->x0 = r->x + 4;
        prim_next.sprt->y0 = r->y;
        prim_next.sprt->w = r->w - 8;
        prim_next.sprt->h = 4;
        prim_next.sprt->u0 = 0;
        prim_next.sprt->v0 = 0;
        prim_next.sprt->clut = GetClut(256, 480);
        AddPrim(prim_ot, prim_next.sprt++);
        setRECT(&tw, 0, 224, 16, 16);
        func_80026A34(0, 1, GetTPage(0, 1, 960, 256), &tw);
        setlen(prim_next.sprt, 4);
        setcode(prim_next.sprt, 0x64);
        func_80046870(prim_next.sprt, 1);
        prim_next.sprt->x0 = r->x + 4;
        prim_next.sprt->y0 = r->y + r->h - 4;
        prim_next.sprt->w = r->w - 8;
        prim_next.sprt->h = 4;
        prim_next.sprt->u0 = 0;
        prim_next.sprt->v0 = 12;
        prim_next.sprt->clut = GetClut(256, 480);
        AddPrim(prim_ot, prim_next.sprt++);
        setRECT(&tw, 16, 232, 16, 16);
        func_80026A34(0, 1, GetTPage(0, 1, 960, 256), &tw);
    }
    if (r->h > 8) {
        setlen(prim_next.sprt, 4);
        setcode(prim_next.sprt, 0x64);
        func_80046870(prim_next.sprt, 1);
        prim_next.sprt->x0 = r->x + r->w - 4;
        prim_next.sprt->y0 = r->y + 4;
        prim_next.sprt->w = 4;
        prim_next.sprt->h = r->h - 8;
        prim_next.sprt->u0 = 12;
        prim_next.sprt->v0 = 0;
        prim_next.sprt->clut = GetClut(256, 480);
        AddPrim(prim_ot, prim_next.sprt++);
        setRECT(&tw, 16, 240, 16, 16);
        func_80026A34(0, 1, GetTPage(0, 1, 960, 256), &tw);
        setlen(prim_next.sprt, 4);
        setcode(prim_next.sprt, 0x64);
        func_80046870(prim_next.sprt, 1);
        prim_next.sprt->x0 = r->x;
        prim_next.sprt->y0 = r->y + 4;
        prim_next.sprt->w = 4;
        prim_next.sprt->h = r->h - 8;
        prim_next.sprt->u0 = 0;
        prim_next.sprt->v0 = 0;
        prim_next.sprt->clut = GetClut(256, 480);
        AddPrim(prim_ot, prim_next.sprt++);
        setRECT(&tw, 0, 240, 16, 16);
        func_80026A34(0, 1, GetTPage(0, 1, 960, 256), &tw);
    }
    setlen(prim_next.sprt, 4);
    setcode(prim_next.sprt, 0x64);
    func_80046870(prim_next.sprt, 1);
    prim_next.sprt->x0 = r->x;
    prim_next.sprt->y0 = r->y;
    prim_next.sprt->u0 = 0;
    prim_next.sprt->v0 = 232;
    prim_next.sprt->w = 4;
    prim_next.sprt->h = 4;
    prim_next.sprt->clut = GetClut(256, 480);
    AddPrim(prim_ot, prim_next.sprt);
    prim_next.sprt++;
    setlen(prim_next.sprt, 4);
    setcode(prim_next.sprt, 0x64);
    func_80046870(prim_next.sprt, 1);
    prim_next.sprt->x0 = r->x + r->w - 4;
    prim_next.sprt->y0 = r->y;
    prim_next.sprt->u0 = 12;
    prim_next.sprt->v0 = 232;
    prim_next.sprt->w = 4;
    prim_next.sprt->h = 4;
    prim_next.sprt->clut = GetClut(256, 480);
    AddPrim(prim_ot, prim_next.sprt);
    prim_next.sprt++;
    setlen(prim_next.sprt, 4);
    setcode(prim_next.sprt, 0x64);
    func_80046870(prim_next.sprt, 1);
    prim_next.sprt->x0 = r->x;
    prim_next.sprt->y0 = r->y + r->h - 4;
    prim_next.sprt->u0 = 16;
    prim_next.sprt->v0 = 228;
    prim_next.sprt->w = 4;
    prim_next.sprt->h = 4;
    prim_next.sprt->clut = GetClut(256, 480);
    AddPrim(prim_ot, prim_next.sprt);
    prim_next.sprt++;
    setlen(prim_next.sprt, 4);
    setcode(prim_next.sprt, 0x64);
    func_80046870(prim_next.sprt, 1);
    prim_next.sprt->x0 = r->x + r->w - 4;
    prim_next.sprt->y0 = r->y + r->h - 4;
    prim_next.sprt->u0 = 28;
    prim_next.sprt->v0 = 228;
    prim_next.sprt->w = 4;
    prim_next.sprt->h = 4;
    prim_next.sprt->clut = GetClut(256, 480);
    AddPrim(prim_ot, prim_next.sprt);
    prim_next.sprt++;
    setRECT(&tw, 0, 0, 256, 256);
    func_80026A34(0, 1, GetTPage(0, 1, 960, 256), &tw);
    if (r->w > 6 && r->h > 6) {
        x = r->x + 3;
        y = r->y + 3;
        w = r->w - 6;
        h = r->h - 6;
        setlen(prim_next.g4, 8);
        setcode(prim_next.g4, 0x38);
        if (window_semi)
            func_80046848(prim_next.g4, 1);
        prim_next.g4->x0 = x;
        prim_next.g4->y0 = y;
        prim_next.g4->x1 = x + w;
        prim_next.g4->y1 = y;
        prim_next.g4->x2 = x;
        prim_next.g4->y2 = y + h;
        prim_next.g4->x3 = x + w;
        prim_next.g4->y3 = y + h;
        prim_next.g4->r0 = D_80049208[0];
        prim_next.g4->g0 = D_80049208[1];
        prim_next.g4->b0 = D_80049208[2];
        prim_next.g4->r1 = D_80049208[3];
        prim_next.g4->g1 = D_80049208[4];
        prim_next.g4->b1 = D_80049208[5];
        prim_next.g4->r2 = D_80049208[6];
        prim_next.g4->g2 = D_80049208[7];
        prim_next.g4->b2 = D_80049208[8];
        prim_next.g4->r3 = D_80049208[9];
        prim_next.g4->g3 = D_80049208[10];
        prim_next.g4->b3 = D_80049208[11];
        AddPrim(prim_ot, prim_next.g4++);
        setRECT(&tw, 96, 224, 32, 32);
        func_80026A34(0, 1, GetTPage(0, 0, 960, 256), &tw);
    }
}

/* A 24x16 sprite drawn half-transparent (additive). */
void func_8001EB2C(short x, short y)
{
    RECT tw;

    setlen(prim_next.sprt, 4);
    setcode(prim_next.sprt, 0x64);
    func_80046848(prim_next.sprt, 1);
    func_80046870(prim_next.sprt, 1);
    prim_next.sprt->x0 = x;
    prim_next.sprt->y0 = y;
    prim_next.sprt->u0 = 224;
    prim_next.sprt->v0 = 8;
    prim_next.sprt->w = 24;
    prim_next.sprt->h = 16;
    prim_next.sprt->clut = GetClut(256, 481);
    AddPrim(prim_ot, prim_next.sprt++);
    setRECT(&tw, 0, 0, 255, 255);
    func_80026A34(0, 1, GetTPage(0, 2, 960, 256), &tw);
}

/* A time as MM:SS from a count of seconds, the colon blinking with the seconds; 99:59 at most. */
void func_8001EC70(int x, int y, int time)
{
    RECT tw;
    int i, digit;

    setlen(prim_next.sprt, 4);
    setcode(prim_next.sprt, 0x64);
    func_80046870(prim_next.sprt, 1);
    prim_next.sprt->x0 = x + 32;
    prim_next.sprt->y0 = y;
    prim_next.sprt->u0 = time & 1 ? 168 : 160;
    prim_next.sprt->v0 = 80;
    prim_next.sprt->w = 8;
    prim_next.sprt->h = 21;
    prim_next.sprt->clut = GetClut(256, 492);
    AddPrim(prim_ot, prim_next.sprt++);
    if (time >= 6000)
        time = 5999;
    for (i = 0; i < 4; i++) {
        digit = time / D_80049214[i];
        setlen(prim_next.sprt, 4);
        setcode(prim_next.sprt, 0x64);
        func_80046870(prim_next.sprt, 1);
        prim_next.sprt->x0 = x;
        prim_next.sprt->y0 = y;
        prim_next.sprt->u0 = digit % 5 * 16 + 176;
        prim_next.sprt->v0 = digit < 5 ? 80 : 104;
        prim_next.sprt->w = 16;
        prim_next.sprt->h = 21;
        prim_next.sprt->clut = GetClut(256, 492);
        AddPrim(prim_ot, prim_next.sprt++);
        if (i == 1)
            x += 8;
        x += 16;
        time %= D_80049214[i];
    }
    setRECT(&tw, 0, 0, 255, 255);
    func_80026A34(0, 1, GetTPage(0, 1, 960, 256), &tw);
}

/* An 8-digit number in the timer's digits, showing only its last `digits` digits; pal goes unused. */
void func_8001EF84(int x, int y, int n, int digits, int pal)
{
    RECT tw;
    int i, digit;

    for (i = 0; i < 8; i++) {
        digit = n / D_80049224[i];
        setlen(prim_next.sprt, 4);
        setcode(prim_next.sprt, 0x64);
        func_80046870(prim_next.sprt, 1);
        prim_next.sprt->x0 = x;
        prim_next.sprt->y0 = y;
        prim_next.sprt->u0 = digit % 5 * 16 + 176;
        prim_next.sprt->v0 = digit < 5 ? 80 : 104;
        prim_next.sprt->w = 16;
        prim_next.sprt->h = 21;
        prim_next.sprt->clut = GetClut(256, 492);
        if (digits >= 8 - i) {
            AddPrim(prim_ot, prim_next.sprt++);
            x += 16;
        }
        n %= D_80049224[i];
    }
    setRECT(&tw, 0, 0, 255, 255);
    func_80026A34(0, 1, GetTPage(0, 1, 960, 256), &tw);
}

/* Draws the field's dialogue windows into buffer buf (0 or 1) and ot: each window's text, clipped to the window as it
 * opens, then its frame. */
#ifdef NON_MATCHING
/* Same code but for one register: the game keeps &frame in a saved register from before the loop (loop
 * optimization), which GCC does here only with r set outside the if, and then it picks other registers. */
void func_8001F1BC(Window *windows, short count, void *ot, short buf)
{
    DRAWENV env;
    RECT frame;
    RECT *r;
    short i;

    D_80062DFD = 1;
    text_frame++;
    func_80026A00(ot);
    func_800269C0(D_80077F64[buf]);
    for (i = 0; i < count; i++) {
        if (windows[i].visible) {
            r = &frame;
            text_fixed = 0;
            text_lines = 0;
            text_count = 0;
            if (windows[i].arrow)
                func_8001EB2C(windows[i].arrow_x, windows[i].arrow_y);
            switch (windows[i].extra) {
            case 1:
                func_8001EC70(windows[i].extra_x, windows[i].extra_y, D_8009D264.countdown);
                break;
            case 2:
                func_8001EF84(windows[i].extra_x, windows[i].extra_y, windows[i].number, windows[i].digits,
                              windows[i].pal);
                break;
            }
            text_semi = windows[i].flags >> 2 & 1;
            text_left = windows[i].shown;
            text_color = 7;
            text_shimmer = 0;
            text_blink = 0;
            if (func_8001D6A8(8, windows[i].scroll + 6, windows[i].w - 3, windows[i].text) + 16 > windows[i].h - 3)
                windows[i].more = 1;
            else
                windows[i].more = 0;
            func_80043814(&env, windows[i].x, windows[i].y + (buf ? 8 : 240), windows[i].w, windows[i].h);
            env.isbg = 0;
            env.dfe = 1;
            env.clip.x = windows[i].x + 3 + (windows[i].w >> 1) - (windows[i].open_w >> 1);
            env.clip.y = windows[i].y + (buf ? 8 : 240) + 3 + (windows[i].h >> 1) - (windows[i].open_h >> 1);
            env.clip.w = windows[i].open_w - 6;
            env.clip.h = windows[i].open_h - 6;
            env.tpage = 0x5F;
            func_80044AC0(prim_next.dr_env, &env);
            AddPrim(ot, prim_next.dr_env);
            prim_next.dr_env++;
            if (windows[i].flags & 2)
                window_semi = 1;
            else
                window_semi = 0;
            if (!(windows[i].flags & 1)) {
                /* w and h against open_w and open_h, both halves at once */
                if (*(int *)&windows[i].w != *(int *)&windows[i].open_w)
                    func_8001DE0C(r, (windows[i].w >> 1) - (windows[i].open_w >> 1),
                                  (windows[i].h >> 1) - (windows[i].open_h >> 1), windows[i].open_w, windows[i].open_h);
                else
                    func_8001DE0C(r, 0, 0, windows[i].w, windows[i].h);
                func_8001E040(r);
            }
            func_80043814(&env, windows[i].x, windows[i].y + (buf ? 8 : 240), windows[i].w, windows[i].h);
            env.isbg = 0;
            env.dfe = 1;
            env.tpage = 0x5F;
            func_80044AC0(prim_next.dr_env, &env);
            AddPrim(ot, prim_next.dr_env);
            window_semi = 0;
            prim_next.dr_env++;
            windows[i].count = text_count;
            windows[i].lines = text_lines;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/main/text_draw", func_8001F1BC);
#endif

void func_8001F6AC(void)
{
}

unsigned char func_8001F6B4(void)
{
    return caption_state;
}

/* Opens the caption window on text. */
void func_8001F6C0(unsigned char *text, int color)
{
    caption_state = 1;
    caption_color = color;
    caption_timer = 40;
    caption_text = text;
    caption_step = 1;
}

/* Places the caption window at (x, y), or with placed 0 closes it. */
void func_8001F6E4(short placed, short x, short y)
{
    caption_placed = placed;
    if (placed) {
        caption_x = x;
        caption_y = y;
    } else
        caption_state = 0;
}

/* Draws the caption window and steps it: three frames to open, open until a button after 20 frames (or after 40 on
 * its own unless placed), three frames to close. */
void func_8001F710(void)
{
    RECT r;
    int w, x, y, ww, hh;

    if (!caption_state)
        return;
    w = func_80026B70(caption_text);
    switch (caption_state) {
    case 1:
        ww = w / 3 * caption_step;
        hh = caption_step * 8;
        if (!caption_placed) {
            x = 180;
            y = 104;
            func_8001DE0C(&r, x - ww / 2, y - hh / 2, ww, hh);
        } else {
            x = caption_x;
            y = caption_y;
            func_8001DE0C(&r, x + (w + 16) / 2 - ww / 2, y + 12 - hh / 2, ww, hh);
        }
        if (++caption_step == 3) {
            caption_step = 2;
            caption_state = 2;
        }
        break;
    case 2:
        if (!caption_placed) {
            x = 180;
            y = 104;
            func_80026F44(x - w / 2, y - 6, caption_text, caption_color);
            func_8001DE0C(&r, x - w / 2 - 8, y - 12, w + 16, 24);
        } else {
            x = caption_x;
            y = caption_y;
            func_80026F44(x + 8, y + 6, caption_text, caption_color);
            func_8001DE0C(&r, x, y, w + 16, 24);
        }
        if (caption_timer)
            caption_timer--;
        if (caption_timer < 20 && !cd_step()) {
            if (!caption_placed && !caption_timer || func_8001C498())
                caption_state = 3;
        }
        break;
    case 3:
        ww = w / 3 * caption_step;
        hh = caption_step * 8;
        if (!caption_placed) {
            x = 180;
            y = 104;
            func_8001DE0C(&r, x - ww / 2, y - hh / 2, ww, hh);
        } else {
            x = caption_x;
            y = caption_y;
            func_8001DE0C(&r, x + (w + 16) / 2 - ww / 2, y + 12 - hh / 2, ww, hh);
        }
        if (--caption_step == 0)
            caption_state = 0;
        break;
    }
    func_8001DE70();
    func_8001DEF0(D_80049384);
    func_8001E040(&r);
    func_8001DEB0();
}
