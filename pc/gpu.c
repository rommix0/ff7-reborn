/* The PS1 GPU in software, from psx-spx's GPU pages: GP0's drawing and transfer commands into VRAM, GP1's display
 * settings, and GPUSTAT. Everything is drawn at once, so the GPU always reads as idle and ready. */
#include <string.h>
#include "gpu.h"

unsigned short vram[512][1024];

typedef struct Vertex {
    int x, y, r, g, b, u, v;
} Vertex;

/* How the primitive being drawn colours its pixels. */
static struct {
    int textured, raw, semi, shaded, dither, clut_x, clut_y;
} prim;

static struct {
    unsigned int e1, e2, e3, e4, e5;
    int page_x, page_y, abr, depth, dither, flip_x, flip_y;
    int win_mask_x, win_mask_y, win_off_x, win_off_y;
    int x1, y1, x2, y2, off_x, off_y;
    int set_mask, check_mask;
    int disp_x, disp_y, v1, v2, mode, disabled, dma_dir, irq;
    unsigned int info, field;
} g;

static unsigned int fifo[16];
static int fifo_n, fifo_need;
static struct {
    int x, y, w, h, i, active;
} upload, download;
static struct {
    int active, shaded, semi, count, have_color;
    unsigned int color; /* the next vertex's: each gets its own if shaded, else all share the command's */
    Vertex prev;
} polyline;

static const int dither_offset[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};

static int sign11(unsigned int v)
{
    return (int)(v << 21) >> 21;
}

static void set_draw_mode(unsigned int e1)
{
    g.e1 = e1 & 0x3FFF;
    g.page_x = (e1 & 0xF) * 64;
    g.page_y = (e1 >> 4 & 1) * 256;
    g.abr = e1 >> 5 & 3;
    g.depth = e1 >> 7 & 3;
    g.dither = e1 >> 9 & 1;
    g.flip_x = e1 >> 12 & 1;
    g.flip_y = e1 >> 13 & 1;
}

static void reset_drawing(void)
{
    set_draw_mode(0);
    g.e2 = g.e3 = g.e4 = g.e5 = 0;
    g.win_mask_x = g.win_mask_y = g.win_off_x = g.win_off_y = 0;
    g.x1 = g.y1 = g.x2 = g.y2 = g.off_x = g.off_y = 0;
    g.set_mask = g.check_mask = 0;
    fifo_n = 0;
    upload.active = download.active = polyline.active = 0;
}

static int to5(int c8, int offset)
{
    c8 += offset;
    return (c8 < 0 ? 0 : c8 > 255 ? 255 : c8) >> 3;
}

static int blend(int back, int front)
{
    int out = 0, shift, b, f, c;

    for (shift = 0; shift < 15; shift += 5) {
        b = back >> shift & 31;
        f = front >> shift & 31;
        switch (g.abr) {
        case 0:
            c = (b + f) >> 1;
            break;
        case 1:
            c = b + f;
            break;
        case 2:
            c = b - f;
            break;
        default:
            c = b + (f >> 2);
            break;
        }
        out |= (c < 0 ? 0 : c > 31 ? 31 : c) << shift;
    }
    return out;
}

/* One pixel through the drawing area, the mask check, semi-transparency and the mask bit. */
static void plot(int x, int y, int color, int semi)
{
    unsigned short *p;

    if (x < g.x1 || x > g.x2 || y < g.y1 || y > g.y2)
        return;
    p = &vram[y & 511][x & 1023];
    if (g.check_mask && *p & 0x8000)
        return;
    if (semi)
        color = (color & 0x8000) | blend(*p, color);
    *p = color | (g.set_mask ? 0x8000 : 0);
}

static int texel(int u, int v)
{
    int hw, index;

    u = ((u & ~g.win_mask_x) | (g.win_off_x & g.win_mask_x)) & 0xFF;
    v = ((v & ~g.win_mask_y) | (g.win_off_y & g.win_mask_y)) & 0xFF;
    switch (g.depth) {
    case 0:
        hw = vram[(g.page_y + v) & 511][(g.page_x + u / 4) & 1023];
        index = hw >> (u & 3) * 4 & 0xF;
        return vram[prim.clut_y & 511][(prim.clut_x + index) & 1023];
    case 1:
        hw = vram[(g.page_y + v) & 511][(g.page_x + u / 2) & 1023];
        index = hw >> (u & 1) * 8 & 0xFF;
        return vram[prim.clut_y & 511][(prim.clut_x + index) & 1023];
    default:
        return vram[(g.page_y + v) & 511][(g.page_x + u) & 1023];
    }
}

/* The colour a primitive gives pixel (x, y), from its colour and texture coordinate there; -1 if it's transparent. */
static void shade(int x, int y, int r, int gr, int b, int u, int v)
{
    int t, offset = prim.dither ? dither_offset[y & 3][x & 3] : 0;

    if (!prim.textured) {
        plot(x, y, to5(r, offset) | to5(gr, offset) << 5 | to5(b, offset) << 10, prim.semi);
        return;
    }
    t = texel(u, v);
    if (!t)
        return; /* colour 0000h is see-through */
    if (!prim.raw) /* modulated: 80h is full brightness */
        t = (t & 0x8000) | to5((t & 31) * r >> 4, offset) | to5((t >> 5 & 31) * gr >> 4, offset) << 5
            | to5((t >> 10 & 31) * b >> 4, offset) << 10;
    plot(x, y, t, prim.semi && t & 0x8000);
}

static long long edge(const Vertex *a, const Vertex *b, int x, int y)
{
    return (long long)(x - a->x) * (b->y - a->y) - (long long)(y - a->y) * (b->x - a->x);
}

/* With the vertices ordered so the area is positive, these are the edges whose own pixels the triangle keeps: the
 * top and left ones. The right and bottom edges belong to the neighbouring shape. */
static int keeps_edge(const Vertex *a, const Vertex *b)
{
    return b->y > a->y || (b->y == a->y && b->x < a->x);
}

static int oversized(const Vertex *a, const Vertex *b)
{
    return a->x - b->x > 1023 || b->x - a->x > 1023 || a->y - b->y > 511 || b->y - a->y > 511;
}

static int lerp(long long wa, int a, long long wb, int b, long long wc, int c, long long area)
{
    return (int)((wa * a + wb * b + wc * c) / area);
}

static void triangle(Vertex a, Vertex b, Vertex c)
{
    long long area = edge(&a, &b, c.x, c.y), wa, wb, wc;
    int x, y, x0, x1, y0, y1, keep_a, keep_b, keep_c;
    Vertex t;

    if (!area || oversized(&a, &b) || oversized(&b, &c) || oversized(&c, &a))
        return;
    if (area < 0) {
        t = b;
        b = c;
        c = t;
        area = -area;
    }
    keep_a = keeps_edge(&b, &c); /* the edge opposite each vertex */
    keep_b = keeps_edge(&c, &a);
    keep_c = keeps_edge(&a, &b);
    x0 = a.x < b.x ? (a.x < c.x ? a.x : c.x) : (b.x < c.x ? b.x : c.x);
    x1 = a.x > b.x ? (a.x > c.x ? a.x : c.x) : (b.x > c.x ? b.x : c.x);
    y0 = a.y < b.y ? (a.y < c.y ? a.y : c.y) : (b.y < c.y ? b.y : c.y);
    y1 = a.y > b.y ? (a.y > c.y ? a.y : c.y) : (b.y > c.y ? b.y : c.y);
    x0 = x0 < g.x1 ? g.x1 : x0;
    y0 = y0 < g.y1 ? g.y1 : y0;
    x1 = x1 > g.x2 ? g.x2 : x1;
    y1 = y1 > g.y2 ? g.y2 : y1;
    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            wa = edge(&b, &c, x, y);
            wb = edge(&c, &a, x, y);
            wc = edge(&a, &b, x, y);
            if (wa < 0 || wb < 0 || wc < 0 || (!wa && !keep_a) || (!wb && !keep_b) || (!wc && !keep_c))
                continue;
            shade(x, y, lerp(wa, a.r, wb, b.r, wc, c.r, area), lerp(wa, a.g, wb, b.g, wc, c.g, area),
                  lerp(wa, a.b, wb, b.b, wc, c.b, area), lerp(wa, a.u, wb, b.u, wc, c.u, area),
                  lerp(wa, a.v, wb, b.v, wc, c.v, area));
        }
    }
}

/* Lines include both ends, and are always dithered when dithering is on. */
static void line(Vertex a, Vertex b)
{
    int dx = b.x - a.x, dy = b.y - a.y, steps, i;

    if (oversized(&a, &b))
        return;
    steps = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    prim.textured = 0;
    prim.dither = g.dither;
    if (!steps) {
        shade(a.x, a.y, a.r, a.g, a.b, 0, 0);
        return;
    }
    for (i = 0; i <= steps; i++)
        shade(a.x + (dx * i * 2 + (dx < 0 ? -steps : steps)) / (2 * steps),
              a.y + (dy * i * 2 + (dy < 0 ? -steps : steps)) / (2 * steps),
              a.r + (b.r - a.r) * i / steps, a.g + (b.g - a.g) * i / steps, a.b + (b.b - a.b) * i / steps, 0, 0);
}

static Vertex vertex(unsigned int xy, unsigned int color)
{
    Vertex v;

    v.x = sign11(xy) + g.off_x;
    v.y = sign11(xy >> 16) + g.off_y;
    v.r = color & 0xFF;
    v.g = color >> 8 & 0xFF;
    v.b = color >> 16 & 0xFF;
    v.u = v.v = 0;
    return v;
}

static void polygon(const unsigned int *w)
{
    unsigned int op = w[0] >> 24, color = w[0];
    int quad = op & 8, textured = op & 4, shaded = op & 0x10, n = quad ? 4 : 3, i, k = 1;
    Vertex v[4];

    for (i = 0; i < n; i++) {
        if (shaded && i)
            color = w[k++];
        v[i] = vertex(w[k++], color);
        if (textured) {
            v[i].u = w[k] & 0xFF;
            v[i].v = w[k] >> 8 & 0xFF;
            if (i == 0) {
                prim.clut_x = (w[k] >> 16 & 0x3F) * 16;
                prim.clut_y = w[k] >> 22 & 0x1FF;
            } else if (i == 1) /* the texture page attribute sets the draw mode, except dithering and drawing to display */
                set_draw_mode((g.e1 & 0x600) | (w[k] >> 16 & 0x9FF));
            k++;
        }
    }
    prim.textured = textured;
    prim.raw = textured && op & 1;
    prim.semi = op & 2;
    prim.shaded = shaded;
    prim.dither = g.dither && (shaded || (textured && !prim.raw));
    triangle(v[0], v[1], v[2]);
    if (quad)
        triangle(v[1], v[2], v[3]);
}

static void rectangle(const unsigned int *w)
{
    static const int sizes[4] = {0, 1, 8, 16};
    unsigned int op = w[0] >> 24;
    int k = 2, width, height, x, y, u0 = 0, v0 = 0;
    Vertex at = vertex(w[1], w[0]);

    prim.textured = op & 4;
    prim.raw = prim.textured && op & 1;
    prim.semi = op & 2;
    prim.dither = 0;
    if (prim.textured) {
        u0 = w[2] & 0xFF;
        v0 = w[2] >> 8 & 0xFF;
        prim.clut_x = (w[2] >> 16 & 0x3F) * 16;
        prim.clut_y = w[2] >> 22 & 0x1FF;
        k = 3;
    }
    width = sizes[op >> 3 & 3] ? sizes[op >> 3 & 3] : (int)(w[k] & 0x3FF);
    height = sizes[op >> 3 & 3] ? sizes[op >> 3 & 3] : (int)(w[k] >> 16 & 0x1FF);
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            shade(at.x + x, at.y + y, at.r, at.g, at.b, u0 + (g.flip_x ? -x : x), v0 + (g.flip_y ? -y : y));
}

static void fill(const unsigned int *w)
{
    int x0 = w[1] & 0x3F0, y0 = w[1] >> 16 & 0x1FF, width = ((w[2] & 0x3FF) + 0xF) & ~0xF, height = w[2] >> 16 & 0x1FF;
    int color = (w[0] >> 3 & 31) | (w[0] >> 11 & 31) << 5 | (w[0] >> 19 & 31) << 10, x, y;

    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            vram[(y0 + y) & 511][(x0 + x) & 1023] = color;
}

/* Transfers and copies honour the mask settings, as rendering does. */
static void put_pixel(int x, int y, int color)
{
    unsigned short *p = &vram[y & 511][x & 1023];

    if (!(g.check_mask && *p & 0x8000))
        *p = color | (g.set_mask ? 0x8000 : 0);
}

static void copy(const unsigned int *w)
{
    static unsigned short row[1024];
    int sx = w[1] & 0x3FF, sy = w[1] >> 16 & 0x1FF, dx = w[2] & 0x3FF, dy = w[2] >> 16 & 0x1FF;
    int width = ((w[3] - 1) & 0x3FF) + 1, height = (((w[3] >> 16) - 1) & 0x1FF) + 1, x, y;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            row[x] = vram[(sy + y) & 511][(sx + x) & 1023];
        for (x = 0; x < width; x++)
            put_pixel(dx + x, dy + y, row[x]);
    }
}

static void start_transfer(const unsigned int *w, int is_upload)
{
    typeof(upload) *transfer = is_upload ? &upload : &download;

    transfer->x = w[1] & 0x3FF;
    transfer->y = w[1] >> 16 & 0x1FF;
    transfer->w = ((w[2] - 1) & 0x3FF) + 1;
    transfer->h = (((w[2] >> 16) - 1) & 0x1FF) + 1;
    transfer->i = 0;
    transfer->active = 1;
}

static void upload_halfword(int hw)
{
    if (upload.i < upload.w * upload.h)
        put_pixel(upload.x + upload.i % upload.w, upload.y + upload.i / upload.w, hw);
    if (++upload.i >= upload.w * upload.h)
        upload.active = 0;
}

static void settings(unsigned int w)
{
    switch (w >> 24) {
    case 0xE1:
        set_draw_mode(w);
        break;
    case 0xE2:
        g.e2 = w & 0xFFFFF;
        g.win_mask_x = (w & 31) * 8;
        g.win_mask_y = (w >> 5 & 31) * 8;
        g.win_off_x = (w >> 10 & 31) * 8;
        g.win_off_y = (w >> 15 & 31) * 8;
        break;
    case 0xE3:
        g.e3 = w & 0xFFFFF;
        g.x1 = w & 0x3FF;
        g.y1 = w >> 10 & 0x1FF;
        break;
    case 0xE4:
        g.e4 = w & 0xFFFFF;
        g.x2 = w & 0x3FF;
        g.y2 = w >> 10 & 0x1FF;
        break;
    case 0xE5:
        g.e5 = w & 0x3FFFFF;
        g.off_x = sign11(w);
        g.off_y = sign11(w >> 11);
        break;
    case 0xE6:
        g.set_mask = w & 1;
        g.check_mask = w >> 1 & 1;
        break;
    }
}

static int command_words(unsigned int op)
{
    int n;

    switch (op >> 5) {
    case 1:
        n = op & 8 ? 4 : 3;
        return 1 + n * (op & 4 ? 2 : 1) + (op & 0x10 ? n - 1 : 0);
    case 2:
        return op & 0x10 ? 4 : 3;
    case 3:
        return 2 + (op & 4 ? 1 : 0) + ((op >> 3 & 3) ? 0 : 1);
    case 4:
        return 4;
    case 5:
    case 6:
        return 3;
    }
    return op == 0x02 ? 3 : 1;
}

/* A polyline's words after its first: optional colour, then a vertex, until a 5xxx5xxxh terminator. */
static void polyline_word(unsigned int w)
{
    Vertex v;

    if ((w & 0xF000F000) == 0x50005000 && !polyline.have_color && polyline.count >= 2) {
        polyline.active = 0;
        return;
    }
    if (polyline.shaded && !polyline.have_color) {
        polyline.color = w;
        polyline.have_color = 1;
        return;
    }
    v = vertex(w, polyline.color);
    polyline.have_color = 0;
    prim.semi = polyline.semi;
    if (polyline.count++)
        line(polyline.prev, v);
    polyline.prev = v;
}

void gpu_gp0(unsigned int w)
{
    unsigned int op;

    if (upload.active) {
        upload_halfword(w & 0xFFFF);
        if (upload.active)
            upload_halfword(w >> 16);
        return;
    }
    if (polyline.active) {
        polyline_word(w);
        return;
    }
    fifo[fifo_n++] = w;
    op = fifo[0] >> 24;
    if (fifo_n == 1 && op >> 5 == 2 && op & 8) { /* a polyline: its first vertex keeps the command's colour */
        polyline.active = 1;
        polyline.shaded = op & 0x10;
        polyline.semi = op & 2;
        polyline.count = 0;
        polyline.color = w;
        polyline.have_color = polyline.shaded; /* the command word carried vertex 0's colour */
        fifo_n = 0;
        return;
    }
    if (fifo_n == 1)
        fifo_need = command_words(op);
    if (fifo_n < fifo_need)
        return;
    fifo_n = 0;
    switch (op >> 5) {
    case 0:
        if (op == 0x02)
            fill(fifo);
        else if (op == 0x1F)
            g.irq = 1;
        break;
    case 1:
        polygon(fifo);
        break;
    case 2:
        prim.semi = op & 2;
        line(vertex(fifo[1], fifo[0]), vertex(op & 0x10 ? fifo[3] : fifo[2], op & 0x10 ? fifo[2] : fifo[0]));
        break;
    case 3:
        rectangle(fifo);
        break;
    case 4:
        copy(fifo);
        break;
    case 5:
        start_transfer(fifo, 1);
        break;
    case 6:
        start_transfer(fifo, 0);
        break;
    case 7:
        settings(fifo[0]);
        break;
    }
}

void gpu_gp1(unsigned int w)
{
    switch (w >> 24 & 0x3F) {
    case 0x00:
        reset_drawing();
        g.disabled = 1;
        g.dma_dir = g.irq = 0;
        g.disp_x = g.disp_y = 0;
        g.v1 = 0x10;
        g.v2 = 0x100;
        g.mode = 0;
        break;
    case 0x01:
        fifo_n = 0;
        upload.active = download.active = polyline.active = 0;
        break;
    case 0x02:
        g.irq = 0;
        break;
    case 0x03:
        g.disabled = w & 1;
        break;
    case 0x04:
        g.dma_dir = w & 3;
        break;
    case 0x05:
        g.disp_x = w & 0x3FF;
        g.disp_y = w >> 10 & 0x1FF;
        break;
    case 0x07:
        g.v1 = w & 0x3FF;
        g.v2 = w >> 10 & 0x3FF;
        break;
    case 0x08:
        g.mode = w & 0xFF;
        break;
    default:
        if ((w >> 24 & 0x30) == 0x10) { /* 10h-1Fh: read an internal register through GP0 */
            switch (w & 7) {
            case 2:
                g.info = g.e2;
                break;
            case 3:
                g.info = g.e3;
                break;
            case 4:
                g.info = g.e4;
                break;
            case 5:
                g.info = g.e5;
                break;
            case 7:
                g.info = 2; /* GPU version */
                break;
            }
        }
        break;
    }
}

unsigned int gpu_read(void)
{
    unsigned int w = 0;
    int k;

    if (!download.active)
        return g.info;
    for (k = 0; k < 2; k++, download.i++)
        if (download.i < download.w * download.h)
            w |= (unsigned int)vram[(download.y + download.i / download.w) & 511]
                                   [(download.x + download.i % download.w) & 1023] << (16 * k);
    if (download.i >= download.w * download.h)
        download.active = 0;
    return w;
}

/* In 480i the even/odd bit is the field on screen, which changes each vblank; in 240-line mode it flips every
 * scanline, so each read sees the next. */
void gpu_vblank(void)
{
    g.field ^= 1;
}

unsigned int gpu_status(void)
{
    if ((g.mode & 0x24) != 0x24)
        g.field ^= 1;
    return (g.e1 & 0x7FF) | g.set_mask << 11 | g.check_mask << 12 | 1 << 13 | (g.e1 >> 11 & 1) << 15
        | (g.mode >> 6 & 1) << 16 | (g.mode & 3) << 17 | (g.mode >> 2 & 1) << 19 | (g.mode >> 3 & 1) << 20
        | (g.mode >> 4 & 1) << 21 | (g.mode >> 5 & 1) << 22 | g.disabled << 23 | g.irq << 24
        | (g.dma_dir == 1 || g.dma_dir == 2 || (g.dma_dir == 3 && download.active)) << 25 | 1 << 26
        | download.active << 27 | 1 << 28 | g.dma_dir << 29 | g.field << 31;
}

Display gpu_display(void)
{
    static const int widths[4] = {256, 320, 512, 640};
    Display d;

    d.x = g.disp_x;
    d.y = g.disp_y;
    d.w = g.mode & 0x40 ? 368 : widths[g.mode & 3];
    d.h = g.v2 > g.v1 ? g.v2 - g.v1 : 240;
    if ((g.mode & 0x24) == 0x24) /* 480 lines, interlaced */
        d.h *= 2;
    if (d.h > 512) /* the game sets the range: never more than VRAM holds */
        d.h = 512;
    d.rgb24 = g.mode >> 4 & 1;
    return d;
}
