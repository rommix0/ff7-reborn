/* The menus' text and widgets, 0x80026B64-0x800293D0: a line of text's width and drawing (12x12 glyphs from the
 * font at VRAM (896, 256)), the small 8x8 font, gauges, box outlines, sunken panels, raised buttons, scroll bars,
 * shaded bands and small digits. Where it starts is read off the code: these share their own $gp bytes, the sound
 * driver follows. GCC 2.7.2, ASPSX 2.21, -G8, and --expand-div for the gauge's divisions. */

#include "common.h"
#include "main.h"
#include "prims.h"

/* libgpu's getTPage. */
#define getTPage(tp, abr, x, y)                                                                                        \
    ((((tp) & 3) << 7) | (((abr) & 3) << 5) | (((y) & 0x100) >> 4) | (((x) & 0x3FF) >> 6) | (((y) & 0x200) << 2))

/* A gauge (HP, MP, a limit bar). */
typedef struct Gauge {
    short x, y, w, h;
    short value;
    short max;
    short mode;   /* 0: no second bar, 1: a green one, else a red one */
    short value2; /* the second bar's */
    unsigned char r, g, b;
} Gauge;

/* A list's scroll bar. */
typedef struct ScrollBar {
    short shown; /* rows on screen */
    short total; /* rows in the list */
    short top;   /* the first row shown */
    short x, y, w, h;
} ScrollBar;

/* Tentative definitions: the code reaches them through $gp, and the linker gives them the data section's labels. */
unsigned char text_max;           /* the most characters a line of menu text draws */
unsigned char text_page_per_line; /* nonzero: the caller sets the font's texture page back once a line */
void *prim_ot;

extern unsigned char *font_widths; /* by glyph: the left gap in bits 5-7 and the width below */

void func_80044A68(void *p, int dfe, int dtd, int tpage, RECT *tw); /* SetDrawMode */
void func_80026A34(int dfe, int dtd, int tpage, RECT *tw);           /* adds a drawing mode */

void func_80026B64(unsigned char max)
{
    text_max = max;
}

/* A line of menu text's width in pixels. */
int func_80026B70(unsigned char *text)
{
    int i, w, page;
    unsigned char c;

    w = 0;
    for (i = 0; i < text_max; i++) {
        if (!text)
            return w;
        if (*text == 0xFF)
            break;
        switch (*text) {
        case 0xFA:
            text++;
            page = 0xE7;
            break;
        case 0xFB:
            text++;
            page = 0x1B9;
            break;
        case 0xFC:
            text++;
            page = 0x2A0;
            break;
        case 0xFD:
            text++;
            page = 0x372;
            break;
        case 0xFE:
            text++;
            page = 0x444;
            break;
        default:
            page = 0;
        }
        c = font_widths[*text++ + page];
        w += c >> 5;
        w += c & 0x1F;
    }
    return w;
}

/* Draws one glyph at (x, y) in palette color; ch's low byte picks the font page for its high byte. Returns the x
 * after it. */
int func_80026C5C(int x, int y, int color, unsigned short ch)
{
    RECT tw;
    unsigned char *p;
    short page;
    int v;
    short pal_x, vram_dx;
    unsigned short u;

    pal_x = 0;
    vram_dx = 0;
    p = (unsigned char *)&ch;
    switch (*p) {
    case 0xF8:
        return x;
    case 0xFA:
        p++;
        v = 132;
        page = 0xE7;
        break;
    case 0xFB:
        p++;
        v = 0;
        pal_x = 16;
        page = 0x1B9;
        break;
    case 0xFC:
        p++;
        v = 132;
        pal_x = 16;
        page = 0x2A0;
        break;
    case 0xFD:
        p++;
        v = 132;
        page = 0x372;
        vram_dx = -64;
        break;
    case 0xFE:
        p++;
        v = 132;
        pal_x = 16;
        page = 0x444;
        vram_dx = -64;
        break;
    default:
        page = 0;
        v = 0;
    }
    ch = *p;
    u = ch % 21 * 12;
    v += (unsigned short)(ch / 21) * 12;
    x += font_widths[ch + page] >> 5;
    setlen(prim_next.sprt, 4);
    setcode(prim_next.sprt, 0x64);
    func_80046870(prim_next.sprt, 1);
    prim_next.sprt->x0 = x;
    prim_next.sprt->y0 = y;
    prim_next.sprt->u0 = u;
    prim_next.sprt->v0 = v;
    prim_next.sprt->w = 12;
    prim_next.sprt->h = 12;
    prim_next.sprt->clut = GetClut(pal_x + 0x100, color + 0x1F0);
    AddPrim(prim_ot, prim_next.sprt++);
    x += font_widths[ch + page] & 0x1F;
    if (!text_page_per_line) {
        setRECT(&tw, 0, 0, 255, 255);
        func_80044A68(prim_next.dr_mode, 0, 1, getTPage(0, 1, vram_dx + 896, 256), &tw);
        AddPrim(prim_ot, prim_next.dr_mode++);
    }
    return x;
}

/* Draws a line of menu text from (x, y). */
void func_80026F44(short x, short y, unsigned char *text, unsigned char color)
{
    RECT tw;
    short i;
    unsigned char c;
    int ch;

    if (!text)
        return;
    for (i = 0; i < text_max; i++) {
        c = *text;
        if (c == 0xFF)
            break;
        if ((c >= 0xFA && c <= 0xFE) || c == 0xF8) {
            ch = c | text[1] << 8;
            x = func_80026C5C(x, y, color, ch);
            text += 2;
        } else {
            text++;
            x = func_80026C5C(x, y, color, c);
        }
    }
    if (text_page_per_line) {
        setRECT(&tw, 0, 0, 255, 255);
        func_80044A68(prim_next.dr_mode, 0, 1, getTPage(0, 1, 896, 256), &tw);
        AddPrim(prim_ot, prim_next.dr_mode++);
    }
}

/* One 8x8 character from the small font; below 0x33, kana with a voicing mark drawn above. */
void func_8002708C(short x, short y, unsigned char c, unsigned char pal)
{
    int k, u, v;

    if (c < 0x29) {
        setlen(prim_next.sprt8, 3);
        setcode(prim_next.sprt8, 0x74);
        func_80046870(prim_next.sprt8, 1);
        prim_next.sprt8->x0 = x;
        prim_next.sprt8->y0 = y - 8;
        prim_next.sprt8->u0 = 0x88;
        prim_next.sprt8->v0 = 0x98;
        prim_next.sprt8->clut = GetClut(256, pal + 480);
        AddPrim(prim_ot, prim_next.sprt8++);
        k = c + 0x40;
        u = (k & 0xF) * 8 | 0x80;
        k >>= 4;
        k *= 8;
        v = k + 0x80;
    } else if (c >= 0xB4 && c <= 0xCD) {
        u = (c & 0xF) * 8 | 0x80;
        v = (c >> 4) * 8 | 0x80;
    } else if (c >= 0x29 && c <= 0x32) {
        setlen(prim_next.sprt8, 3);
        setcode(prim_next.sprt8, 0x74);
        func_80046870(prim_next.sprt8, 1);
        prim_next.sprt8->x0 = x;
        prim_next.sprt8->y0 = y - 8;
        prim_next.sprt8->u0 = 0x90;
        prim_next.sprt8->v0 = 0x98;
        prim_next.sprt8->clut = GetClut(256, pal + 480);
        AddPrim(prim_ot, prim_next.sprt8++);
        k = c + 0x17;
        u = (k & 0xF) * 8 | 0x80;
        k >>= 4;
        k *= 8;
        v = k + 0x80;
    } else {
        u = (c & 0xF) * 8 | 0x80;
        v = (c >> 4) * 8 | 0x80;
    }
    setlen(prim_next.sprt8, 3);
    setcode(prim_next.sprt8, 0x74);
    func_80046870(prim_next.sprt8, 1);
    prim_next.sprt8->x0 = x;
    prim_next.sprt8->y0 = y;
    prim_next.sprt8->u0 = u;
    prim_next.sprt8->v0 = v;
    prim_next.sprt8->clut = GetClut(256, pal + 480);
    AddPrim(prim_ot, prim_next.sprt8++);
}

/* A line in the small font, 8 pixels a character. */
void func_80027354(short x, short y, unsigned char *text, unsigned char pal)
{
    short i;
    int c;

    if (!text)
        return;
    for (i = 0; i < text_max; i++) {
        c = *text++;
        if (c == 0xFF)
            break;
        func_8002708C(x, y, c, pal);
        x += 8;
    }
}

/* A gauge: the bar in g's colour fading to grey, a second bar over it if mode is set, and a dark red backing. */
void func_80027408(Gauge *g)
{
    RECT unused; /* the frame has room for one */
    int len, w;
    short len2;
    short x, y, h;
    unsigned char r, gr, b;

    if (!g->max)
        return;
    len2 = g->w * g->value2 / g->max;
    len = g->w * g->value / g->max;
    x = g->x;
    y = g->y;
    h = g->h;
    w = g->w;
    SetPolyG4(prim_next.g4);
    prim_next.g4->x0 = x;
    prim_next.g4->y0 = y;
    prim_next.g4->x1 = x + len;
    prim_next.g4->y1 = y;
    prim_next.g4->x2 = x;
    prim_next.g4->y2 = y + h;
    prim_next.g4->x3 = x + len;
    prim_next.g4->y3 = y + h;
    r = g->r;
    gr = g->g;
    b = g->b;
    prim_next.g4->r0 = r;
    prim_next.g4->g0 = gr;
    prim_next.g4->b0 = b;
    prim_next.g4->r1 = 200;
    prim_next.g4->g1 = 200;
    prim_next.g4->b1 = 200;
    prim_next.g4->r2 = r;
    prim_next.g4->g2 = gr;
    prim_next.g4->b2 = b;
    prim_next.g4->r3 = 200;
    prim_next.g4->g3 = 200;
    prim_next.g4->b3 = 200;
    AddPrim(prim_ot, prim_next.g4++);
    if (g->mode) {
        if (g->mode == 1) {
            r = 0;
            gr = 200;
            b = 80;
        } else {
            r = 200;
            gr = 0;
            b = 0;
        }
        SetPolyG4(prim_next.g4);
        prim_next.g4->x0 = x;
        prim_next.g4->y0 = y;
        prim_next.g4->x1 = x + len2;
        prim_next.g4->y1 = y;
        prim_next.g4->x2 = x;
        prim_next.g4->y2 = y + h;
        prim_next.g4->x3 = x + len2;
        prim_next.g4->y3 = y + h;
        prim_next.g4->r0 = r;
        prim_next.g4->g0 = gr;
        prim_next.g4->b0 = b;
        prim_next.g4->r1 = r;
        prim_next.g4->g1 = gr;
        prim_next.g4->b1 = b;
        prim_next.g4->r2 = r;
        prim_next.g4->g2 = gr;
        prim_next.g4->b2 = b;
        prim_next.g4->r3 = r;
        prim_next.g4->g3 = gr;
        prim_next.g4->b3 = b;
        AddPrim(prim_ot, prim_next.g4++);
    }
    SetPolyG4(prim_next.g4);
    prim_next.g4->x0 = x;
    prim_next.g4->y0 = y;
    prim_next.g4->x1 = x + w;
    prim_next.g4->y1 = y;
    prim_next.g4->x2 = x;
    prim_next.g4->y2 = y + h + 1;
    prim_next.g4->x3 = x + w;
    prim_next.g4->y3 = y + h + 1;
    prim_next.g4->r0 = 80;
    prim_next.g4->g0 = 0;
    prim_next.g4->b0 = 0;
    prim_next.g4->r1 = 80;
    prim_next.g4->g1 = 0;
    prim_next.g4->b1 = 0;
    prim_next.g4->r2 = 0;
    prim_next.g4->g2 = 0;
    prim_next.g4->b2 = 0;
    prim_next.g4->r3 = 0;
    prim_next.g4->g3 = 0;
    prim_next.g4->b3 = 0;
    AddPrim(prim_ot, prim_next.g4++);
}

/* A box outline, a closed four-point line, in grey c. */
void func_80027990(int x, int y, int w, int h, int c)
{
    setlen(prim_next.line4, 6);
    setcode(prim_next.line4, 0x4C);
    prim_next.line4->pad = 0x55555555;
    prim_next.line4->x0 = x;
    prim_next.line4->y0 = y;
    prim_next.line4->x1 = x + w - 1;
    prim_next.line4->y1 = y;
    prim_next.line4->x2 = x + w - 1;
    prim_next.line4->y2 = y + h - 1;
    prim_next.line4->x3 = x;
    prim_next.line4->y3 = y + h - 1;
    prim_next.line4->r0 = c;
    prim_next.line4->g0 = c;
    prim_next.line4->b0 = c;
    AddPrim(prim_ot, prim_next.line4++);
    setlen(prim_next.line, 3);
    setcode(prim_next.line, 0x40);
    prim_next.line->x0 = x;
    prim_next.line->y0 = y;
    prim_next.line->x1 = x;
    prim_next.line->y1 = y + h - 1;
    prim_next.line->r0 = c;
    prim_next.line->g0 = c;
    prim_next.line->b0 = c;
    AddPrim(prim_ot, prim_next.line++);
}

/* A sunken panel: dark top and left edges, light bottom and right, over a semi-transparent fill. */
void func_80027B84(RECT *r)
{
    setlen(prim_next.line, 3);
    setcode(prim_next.line, 0x40);
    func_80046848(prim_next.line, 1);
    prim_next.line->x0 = r->x;
    prim_next.line->y0 = r->y;
    prim_next.line->x1 = r->x + r->w - 1;
    prim_next.line->y1 = r->y;
    prim_next.line->r0 = 0x10;
    prim_next.line->g0 = 0x10;
    prim_next.line->b0 = 0x10;
    AddPrim(prim_ot, prim_next.line++);
    setlen(prim_next.line, 3);
    setcode(prim_next.line, 0x40);
    func_80046848(prim_next.line, 1);
    prim_next.line->x0 = r->x;
    prim_next.line->y0 = r->y + r->h - 1;
    prim_next.line->x1 = r->x + r->w - 1;
    prim_next.line->y1 = r->y + r->h - 1;
    prim_next.line->r0 = 0x70;
    prim_next.line->g0 = 0x70;
    prim_next.line->b0 = 0x70;
    AddPrim(prim_ot, prim_next.line++);
    setlen(prim_next.line, 3);
    setcode(prim_next.line, 0x40);
    func_80046848(prim_next.line, 1);
    prim_next.line->x0 = r->x;
    prim_next.line->y0 = r->y;
    prim_next.line->x1 = r->x;
    prim_next.line->y1 = r->y + r->h - 1;
    prim_next.line->r0 = 0x20;
    prim_next.line->g0 = 0x20;
    prim_next.line->b0 = 0x20;
    AddPrim(prim_ot, prim_next.line++);
    setlen(prim_next.line, 3);
    setcode(prim_next.line, 0x40);
    func_80046848(prim_next.line, 1);
    prim_next.line->x0 = r->x + r->w - 1;
    prim_next.line->y0 = r->y;
    prim_next.line->x1 = r->x + r->w - 1;
    prim_next.line->y1 = r->y + r->h - 1;
    prim_next.line->r0 = 0x70;
    prim_next.line->g0 = 0x70;
    prim_next.line->b0 = 0x70;
    AddPrim(prim_ot, prim_next.line++);
    setlen(prim_next.tile, 3);
    setcode(prim_next.tile, 0x60);
    func_80046848(prim_next.tile, 1);
    prim_next.tile->x0 = r->x;
    prim_next.tile->y0 = r->y;
    prim_next.tile->w = r->w;
    prim_next.tile->h = r->h;
    prim_next.tile->r0 = 0x50;
    prim_next.tile->g0 = 0x50;
    prim_next.tile->b0 = 0x50;
    AddPrim(prim_ot, prim_next.tile++);
}

/* A raised button: light top and left edges, dark bottom and right, over a grey fill. */
void func_80028030(RECT *r)
{
    setlen(prim_next.line, 3);
    setcode(prim_next.line, 0x40);
    prim_next.line->x0 = r->x;
    prim_next.line->y0 = r->y;
    prim_next.line->x1 = r->x + r->w - 1;
    prim_next.line->y1 = r->y;
    prim_next.line->r0 = 0xF0;
    prim_next.line->g0 = 0xF0;
    prim_next.line->b0 = 0xF0;
    AddPrim(prim_ot, prim_next.line++);
    setlen(prim_next.line, 3);
    setcode(prim_next.line, 0x40);
    prim_next.line->x0 = r->x;
    prim_next.line->y0 = r->y + r->h - 1;
    prim_next.line->x1 = r->x + r->w - 1;
    prim_next.line->y1 = r->y + r->h - 1;
    prim_next.line->r0 = 0x40;
    prim_next.line->g0 = 0x40;
    prim_next.line->b0 = 0x40;
    AddPrim(prim_ot, prim_next.line++);
    setlen(prim_next.line, 3);
    setcode(prim_next.line, 0x40);
    prim_next.line->x0 = r->x;
    prim_next.line->y0 = r->y;
    prim_next.line->x1 = r->x;
    prim_next.line->y1 = r->y + r->h - 1;
    prim_next.line->r0 = 0xC8;
    prim_next.line->g0 = 0xC8;
    prim_next.line->b0 = 0xC8;
    AddPrim(prim_ot, prim_next.line++);
    setlen(prim_next.line, 3);
    setcode(prim_next.line, 0x40);
    prim_next.line->x0 = r->x + r->w - 1;
    prim_next.line->y0 = r->y;
    prim_next.line->x1 = r->x + r->w - 1;
    prim_next.line->y1 = r->y + r->h - 1;
    prim_next.line->r0 = 0x70;
    prim_next.line->g0 = 0x70;
    prim_next.line->b0 = 0x70;
    AddPrim(prim_ot, prim_next.line++);
    setlen(prim_next.tile, 3);
    setcode(prim_next.tile, 0x60);
    prim_next.tile->x0 = r->x;
    prim_next.tile->y0 = r->y;
    prim_next.tile->w = r->w;
    prim_next.tile->h = r->h;
    prim_next.tile->r0 = 0xA0;
    prim_next.tile->g0 = 0xA0;
    prim_next.tile->b0 = 0xA0;
    AddPrim(prim_ot, prim_next.tile++);
}

/* A scroll bar: the thumb's place and size are the shown part of the list, in a sunken track. */
void func_80028484(ScrollBar *s)
{
    RECT r;
    int top, len;

    top = s->h * s->top / s->total;
    len = s->h * s->shown / s->total;
    r.x = s->x;
    r.y = s->y + top;
    r.w = s->w - 1;
    r.h = len + 1;
    func_80028030(&r);
    r.x = s->x;
    r.y = s->y;
    r.w = s->w;
    r.h = s->h;
    func_80027B84(&r);
    func_80026A34(0, 1, 0x1F, 0);
}

/* A semi-transparent band in two halves, each from (r, g, b) at its top: to grey, then to black. */
void func_800285AC(short x, short y, short w, short h, unsigned char r, unsigned char g, unsigned char b)
{
    SetPolyG4(prim_next.g4);
    func_80046848(prim_next.g4, 1);
    prim_next.g4->x0 = x;
    prim_next.g4->y0 = y;
    prim_next.g4->x1 = x + w;
    prim_next.g4->y1 = y;
    prim_next.g4->x2 = x;
    prim_next.g4->y2 = y + h / 2;
    prim_next.g4->x3 = x + w;
    prim_next.g4->y3 = y + h / 2;
    prim_next.g4->r0 = r;
    prim_next.g4->g0 = g;
    prim_next.g4->b0 = b;
    prim_next.g4->r1 = r;
    prim_next.g4->g1 = g;
    prim_next.g4->b1 = b;
    prim_next.g4->r2 = 0x80;
    prim_next.g4->g2 = 0x80;
    prim_next.g4->b2 = 0x80;
    prim_next.g4->r3 = 0x80;
    prim_next.g4->g3 = 0x80;
    prim_next.g4->b3 = 0x80;
    AddPrim(prim_ot, prim_next.g4++);
    SetPolyG4(prim_next.g4);
    func_80046848(prim_next.g4, 1);
    prim_next.g4->x0 = x;
    prim_next.g4->y0 = y + h / 2;
    prim_next.g4->x1 = x + w;
    prim_next.g4->y1 = y + h / 2;
    prim_next.g4->x2 = x;
    prim_next.g4->y2 = y + h;
    prim_next.g4->x3 = x + w;
    prim_next.g4->y3 = y + h;
    prim_next.g4->r0 = r;
    prim_next.g4->g0 = g;
    prim_next.g4->b0 = b;
    prim_next.g4->r1 = r;
    prim_next.g4->g1 = g;
    prim_next.g4->b1 = b;
    prim_next.g4->r2 = 0;
    prim_next.g4->g2 = 0;
    prim_next.g4->b2 = 0;
    prim_next.g4->r3 = 0;
    prim_next.g4->g3 = 0;
    prim_next.g4->b3 = 0;
    AddPrim(prim_ot, prim_next.g4++);
}

/* A shine: white fading down over the top third and up over the bottom third, semi-transparent. */
void func_80028930(short x, short y, short w, short h)
{
    int third;

    SetPolyG4(prim_next.g4);
    func_80046848(prim_next.g4, 1);
    third = h;
    third /= 3;
    prim_next.g4->x0 = x;
    prim_next.g4->y0 = y;
    prim_next.g4->x1 = x + w;
    prim_next.g4->y1 = y;
    prim_next.g4->x2 = x;
    prim_next.g4->y2 = y + third;
    prim_next.g4->x3 = x + w;
    prim_next.g4->y3 = y + third;
    prim_next.g4->r0 = 0xFF;
    prim_next.g4->g0 = 0xFF;
    prim_next.g4->b0 = 0xFF;
    prim_next.g4->r1 = 0xFF;
    prim_next.g4->g1 = 0xFF;
    prim_next.g4->b1 = 0xFF;
    prim_next.g4->r2 = 0;
    prim_next.g4->g2 = 0;
    prim_next.g4->b2 = 0;
    prim_next.g4->r3 = 0;
    prim_next.g4->g3 = 0;
    prim_next.g4->b3 = 0;
    AddPrim(prim_ot, prim_next.g4++);
    SetPolyG4(prim_next.g4);
    func_80046848(prim_next.g4, 1);
    prim_next.g4->x0 = x;
    prim_next.g4->y0 = y + h - third;
    prim_next.g4->x1 = x + w;
    prim_next.g4->y1 = y + h - third;
    prim_next.g4->x2 = x;
    prim_next.g4->y2 = y + h - third + third;
    prim_next.g4->x3 = x + w;
    prim_next.g4->y3 = y + h - third + third;
    prim_next.g4->r0 = 0;
    prim_next.g4->g0 = 0;
    prim_next.g4->b0 = 0;
    prim_next.g4->r1 = 0;
    prim_next.g4->g1 = 0;
    prim_next.g4->b1 = 0;
    prim_next.g4->r2 = 0xFF;
    prim_next.g4->g2 = 0xFF;
    prim_next.g4->b2 = 0xFF;
    prim_next.g4->r3 = 0xFF;
    prim_next.g4->g3 = 0xFF;
    prim_next.g4->b3 = 0xFF;
    AddPrim(prim_ot, prim_next.g4++);
}

/* A w x h sprite from (u, v) of WINDOW.BIN's texture, palette row 480 + pal. */
void func_80028CA0(short x, short y, unsigned char u, unsigned char v, short w, short h, short pal, short semi)
{
    setlen(prim_next.sprt, 4);
    setcode(prim_next.sprt, 0x64);
    func_80046870(prim_next.sprt, 1);
    if (semi)
        func_80046848(prim_next.sprt, 1);
    prim_next.sprt->x0 = x;
    prim_next.sprt->y0 = y;
    prim_next.sprt->u0 = u;
    prim_next.sprt->v0 = v;
    prim_next.sprt->w = w;
    prim_next.sprt->h = h;
    prim_next.sprt->clut = GetClut(256, pal + 480);
    AddPrim(prim_ot, prim_next.sprt);
    prim_next.sprt++;
}

/* n in digits small digits from x, leading zeros left out; three digits stop at 999. */
void func_80028E00(short x, short y, unsigned int number, unsigned char digits, unsigned char pal)
{
    unsigned int div, q, n;
    short i;
    short leading;

    div = 1;
    leading = 1;
    for (i = 1; i < digits; i++)
        div *= 10;
    n = number;
    if (digits == 3 && n >= 1000)
        n = 999;
    for (i = 1; i < digits; i++) {
        setlen(prim_next.sprt, 4);
        setcode(prim_next.sprt, 0x64);
        func_80046870(prim_next.sprt, 1);
        q = n / div;
        prim_next.sprt->x0 = x + (i - 1) * 7;
        prim_next.sprt->y0 = y;
        prim_next.sprt->u0 = q * 8 + 0x88;
        prim_next.sprt->v0 = 0;
        prim_next.sprt->w = 7;
        prim_next.sprt->h = 8;
        prim_next.sprt->clut = GetClut(256, pal + 480);
        if (!leading || q) {
            AddPrim(prim_ot, prim_next.sprt);
            leading = 0;
            prim_next.sprt++;
        }
        n %= div;
        div /= 10;
    }
    setlen(prim_next.sprt, 4);
    setcode(prim_next.sprt, 0x64);
    func_80046870(prim_next.sprt, 1);
    prim_next.sprt->x0 = x + (digits - 1) * 7;
    prim_next.sprt->y0 = y;
    prim_next.sprt->u0 = n * 8 + 0x88;
    prim_next.sprt->v0 = 0;
    prim_next.sprt->w = 7;
    prim_next.sprt->h = 8;
    prim_next.sprt->clut = GetClut(256, pal + 480);
    AddPrim(prim_ot, prim_next.sprt);
    prim_next.sprt++;
}

/* The same with the leading zeros. */
#ifdef NON_MATCHING
/* Same code, but GCC gives n, div and i the saved registers in another order (s2, s0, s1 for the game's s0, s1, s2). */
void func_80029114(short x, short y, unsigned int n, unsigned char digits, unsigned char pal)
{
    unsigned int div, q;
    short i;

    div = 1;
    for (i = 1; i < digits; i++)
        div *= 10;
    for (i = 1; i < digits; i++) {
        setlen(prim_next.sprt, 4);
        setcode(prim_next.sprt, 0x64);
        func_80046870(prim_next.sprt, 1);
        q = n / div;
        n %= div;
        prim_next.sprt->x0 = x + (i - 1) * 7;
        prim_next.sprt->y0 = y;
        prim_next.sprt->u0 = q * 8 + 0x88;
        prim_next.sprt->v0 = 0;
        prim_next.sprt->w = 7;
        prim_next.sprt->h = 8;
        prim_next.sprt->clut = GetClut(256, pal + 480);
        AddPrim(prim_ot, prim_next.sprt);
        div /= 10;
        prim_next.sprt++;
    }
    setlen(prim_next.sprt, 4);
    setcode(prim_next.sprt, 0x64);
    func_80046870(prim_next.sprt, 1);
    prim_next.sprt->x0 = x + (digits - 1) * 7;
    prim_next.sprt->y0 = y;
    prim_next.sprt->u0 = n * 8 + 0x88;
    prim_next.sprt->v0 = 0;
    prim_next.sprt->w = 7;
    prim_next.sprt->h = 8;
    prim_next.sprt->clut = GetClut(256, pal + 480);
    AddPrim(prim_ot, prim_next.sprt);
    prim_next.sprt++;
}
#else
INCLUDE_ASM("asm/nonmatchings/main/menu_text", func_80029114);
#endif
