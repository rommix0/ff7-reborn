/* gzip unpacking, 0x80015B44-0x80017238: archives of gzip members, modules, and the inflate itself (Huffman tables
 * from a fixed pool, the output written straight to its place, no window). -G8 and ASPSX 2.34 like the files before
 * it: the inflate's state lives in $gp variables. */

#include "common.h"
#include "main.h"

/* An archive entry: u16 size of the gzip data, u16 its unpacked size, u16 id, then the data; size 0 ends it. */
#define U16(p) ((p)[0] | (p)[1] << 8)

unsigned char *archive;      /* the next entry */
unsigned int inflate_out;    /* bytes written */
unsigned int inflate_in;     /* bytes read */
unsigned char *inflate_src;
unsigned char *inflate_dst;
unsigned int *huffman_pool;  /* 0x1000 words on gunzip's stack */
unsigned int bit_buf;
unsigned int bit_count;
unsigned int huffman_count;  /* table entries made for the current block */
unsigned int huffman_pool_used;
int lit_bits;                /* the literal table's lookup bits (9) */
int dist_bits;               /* the distance table's (6) */

/* The deflate format's tables. */
extern unsigned int code_length_order[];
extern unsigned short length_base[];
extern unsigned short length_extra[];
extern unsigned short dist_base[];
extern unsigned short dist_extra[];
extern unsigned short bit_masks[];

/* A table entry: e is the extra bits (or 16 a literal, 15 the end, 99 invalid, over 16 a subtable with e - 16
 * bits), b the bits this code takes. */
typedef struct Huffman {
    unsigned char e;
    unsigned char b;
    union {
        unsigned short n;
        struct Huffman *t;
    } v;
} Huffman;

#define NEXT_BYTE() (inflate_src[inflate_in++])
#define NEED_BITS(n) while (k < (n)) { b |= NEXT_BYTE() << k; k += 8; }
#define DUMP_BITS(n) { b >>= (n); k -= (n); }

int gunzip(unsigned char *src, unsigned char *dst);

void archive_open(unsigned char *p)
{
    archive = p;
}

int archive_entry_id(void)
{
    unsigned char *p = archive;
    int id = 0xFFFF;

    if (U16(p))
        id = U16(p + 4);
    return id;
}

int archive_entry_unpacked_size(void)
{
    unsigned char *p = archive;
    int size = 0;

    if (U16(p))
        size = U16(p + 2);
    return size;
}

/* Unpacks the next entry and moves past it if it came out at its size; -1 at the end. */
int archive_unpack_next(unsigned char *dst)
{
    int r = -1;
    unsigned short size;

    if ((size = U16(archive)) != 0) {
        r = gunzip(archive + 6, dst);
        if (r != U16(archive + 2))
            return r;
        archive += size + 6;
    }
    return r;
}

/* Unpacks the entry with an id; -1 if there's none. */
int archive_unpack_id(unsigned char *p, unsigned char *dst, int id)
{
    int r = -1;
    unsigned short size;

    while ((size = U16(p)) != 0) {
        if (U16(p + 4) == id) {
            r = gunzip(p + 6, dst);
            break;
        }
        p += size + 6;
    }
    return r;
}

/* A module: its code's size and its zeroed data's, then the gzip data. */
void unpack_module(void *src, void *dst)
{
    unsigned int *p = dst;
    unsigned int code = ((unsigned int *)src)[0];
    unsigned int bss = ((unsigned int *)src)[1];
    int i;

    gunzip((unsigned char *)src + 8, dst);
    bss /= 4;
    p += code / 4;
    for (i = 0; i < (int)bss; i++)
        *p++ = 0;
}

/* Starts over when the pool is used up: a block's tables are made, used and dropped before the next block's. */
void *huffman_alloc(unsigned int bytes)
{
    unsigned int used = huffman_pool_used;
    unsigned int end = used + (bytes + 3) / 4;
    void *p;

    if (end <= 0x1000) {
        huffman_pool_used = end;
        p = huffman_pool + used;
    } else {
        huffman_pool_used = 0;
        p = huffman_alloc(bytes);
    }
    return p;
}

int huffman_free(Huffman *t);

/* Builds the lookup tables for a code from its lengths b[0..n-1]: values below s are literals (or the end), the
 * rest take base d and extra bits e. The first table is looked up with *m bits. 0 for a complete code, 1 for an
 * incomplete one, 2 for lengths no code has, 3 out of memory. */
int huffman_build(unsigned int *b, unsigned int n, unsigned int s, unsigned short *d, unsigned short *e, Huffman **t,
                  int *m)
{
    unsigned int a;
    unsigned int c[17];
    int len;
    unsigned int f;
    int g;
    int h;
    unsigned int i;
    unsigned int j;
    int k;
    int l;
    unsigned int *p;
    Huffman *q;
    Huffman r;
    Huffman *u[16];
    unsigned int v[288];
    int w;
    unsigned int x[17];
    unsigned int *xp;
    int y;
    unsigned int z;

    for (len = 0; len < 17; len++)
        c[len] = 0;
    p = b;
    i = n;
    do {
        c[*p]++;
        p++;
    } while (--i);
    if (c[0] == n) {
        *t = 0;
        *m = 0;
        return 0;
    }

    l = *m;
    for (j = 1; j <= 16; j++)
        if (c[j])
            break;
    k = j;
    if ((unsigned int)l < j)
        l = j;
    for (i = 16; i; i--)
        if (c[i])
            break;
    g = i;
    if ((unsigned int)l > i)
        l = i;
    *m = l;

    for (y = 1 << j; j < i; j++, y <<= 1)
        if ((y -= c[j]) < 0)
            return 2;
    if ((y -= c[i]) < 0)
        return 2;
    c[i] += y;

    x[1] = j = 0;
    p = c + 1;
    xp = x + 2;
    while (--i)
        *xp++ = (j += *p++);

    p = b;
    i = 0;
    do {
        if ((j = *p++) != 0)
            v[x[j]++] = i;
    } while (++i < n);

    x[0] = i = 0;
    p = v;
    h = -1;
    w = -l;
    u[0] = 0;
    q = 0;
    z = 0;
    for (; k <= g; k++) {
        a = c[k];
        while (a--) {
            while (k > w + l) {
                h++;
                w += l;
                z = (z = g - w) > (unsigned int)l ? l : z;
                if ((f = 1 << (j = k - w)) > a + 1) {
                    f -= a + 1;
                    xp = c + k;
                    while (++j < z) {
                        if ((f <<= 1) <= *++xp)
                            break;
                        f -= *xp;
                    }
                }
                z = 1 << j;
                if ((q = huffman_alloc((z + 1) * sizeof(Huffman))) == 0) {
                    if (h)
                        huffman_free(u[0]);
                    return 3;
                }
                huffman_count += z + 1;
                *t = q + 1;
                *(t = &q->v.t) = 0;
                u[h] = ++q;
                if (h) {
                    x[h] = i;
                    r.b = l;
                    r.e = 16 + j;
                    r.v.t = q;
                    j = i >> (w - l);
                    u[h - 1][j] = r;
                }
            }

            r.b = k - w;
            if (p >= v + n) {
                r.e = 99;
            } else if (*p < s) {
                r.e = *p < 256 ? 16 : 15;
                r.v.n = *p++;
            } else {
                r.e = e[*p - s];
                r.v.n = d[*p++ - s];
            }

            f = 1 << (k - w);
            for (j = i >> w; j < z; j += f)
                q[j] = r;
            for (j = 1 << (k - 1); i & j; j >>= 1)
                i ^= j;
            i ^= j;
            while ((i & ((1 << w) - 1)) != x[h]) {
                h--;
                w -= l;
            }
        }
    }
    return y != 0 && g != 1;
}

/* The tables come from the pool, so there's nothing to give back. */
int huffman_free(Huffman *t)
{
    while (t)
        t = t[-1].v.t;
    return 0;
}

/* Decodes a block's codes with literal/length tables tl (bl bits) and distance tables td (bd bits). */
#ifdef NON_MATCHING
/* Same code but for one pair: the original sets the copy's counter in the branch's delay slot, after the shift. */
int inflate_codes(Huffman *tl, Huffman *td, int bl, int bd)
{
    unsigned int e;
    unsigned int n, d;
    unsigned int w;
    Huffman *t;
    unsigned int ml, md;
    unsigned int b;
    unsigned int k;

    b = bit_buf;
    k = bit_count;
    w = inflate_out;
    ml = bit_masks[bl];
    md = bit_masks[bd];
    for (;;) {
        NEED_BITS(bl)
        if ((e = (t = tl + (b & ml))->e) > 16)
            do {
                if (e == 99)
                    return 1;
                DUMP_BITS(t->b)
                e -= 16;
                NEED_BITS(e)
            } while ((e = (t = t->v.t + (b & bit_masks[e]))->e) > 16);
        DUMP_BITS(t->b)
        if (e == 16) {
            inflate_dst[w++] = t->v.n;
        } else {
            if (e == 15)
                break;
            NEED_BITS(e)
            n = t->v.n + (b & bit_masks[e]);
            DUMP_BITS(e)
            NEED_BITS(bd)
            if ((e = (t = td + (b & md))->e) > 16)
                do {
                    if (e == 99)
                        return 1;
                    DUMP_BITS(t->b)
                    e -= 16;
                    NEED_BITS(e)
                } while ((e = (t = t->v.t + (b & bit_masks[e]))->e) > 16);
            DUMP_BITS(t->b)
            NEED_BITS(e)
            d = w - t->v.n - (b & bit_masks[e]);
            DUMP_BITS(e)
            if (n) {
                e = n;
                do {
                    inflate_dst[w++] = inflate_dst[d++];
                } while (--e);
            }
        }
    }
    inflate_out = w;
    bit_buf = b;
    bit_count = k;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/main/inflate", inflate_codes);
#endif

int inflate_stored(void)
{
    unsigned int n;
    unsigned int w;
    unsigned int b;
    unsigned int k;

    b = bit_buf;
    k = bit_count;
    w = inflate_out;
    n = k & 7;
    DUMP_BITS(n)
    NEED_BITS(16)
    n = b & 0xFFFF;
    DUMP_BITS(16)
    NEED_BITS(16)
    if (n != (~b & 0xFFFF))
        return 1;
    DUMP_BITS(16)
    while (n--) {
        NEED_BITS(8)
        inflate_dst[w++] = b;
        DUMP_BITS(8)
    }
    inflate_out = w;
    bit_buf = b;
    bit_count = k;
    return 0;
}

int inflate_fixed(void)
{
    int i;
    Huffman *tl;
    Huffman *td;
    int bl;
    int bd;
    unsigned int l[288];

    for (i = 0; i < 144; i++)
        l[i] = 8;
    for (; i < 256; i++)
        l[i] = 9;
    for (; i < 280; i++)
        l[i] = 7;
    for (; i < 288; i++)
        l[i] = 8;
    bl = 7;
    if ((i = huffman_build(l, 288, 257, length_base, length_extra, &tl, &bl)) != 0)
        return i;
    for (i = 0; i < 30; i++)
        l[i] = 5;
    bd = 5;
    if ((i = huffman_build(l, 30, 0, dist_base, dist_extra, &td, &bd)) > 1) {
        huffman_free(tl);
        return i;
    }
    if (inflate_codes(tl, td, bl, bd))
        return 1;
    huffman_free(tl);
    huffman_free(td);
    return 0;
}

int inflate_dynamic(void)
{
    int i;
    unsigned int j;
    unsigned int l;
    unsigned int m;
    unsigned int n;
    Huffman *tl;
    Huffman *td;
    int bl;
    int bd;
    unsigned int nb;
    unsigned int nl;
    unsigned int nd;
    unsigned int ll[286 + 30];
    unsigned int b;
    unsigned int k;

    b = bit_buf;
    k = bit_count;
    NEED_BITS(5)
    nl = 257 + (b & 0x1F);
    DUMP_BITS(5)
    NEED_BITS(5)
    nd = 1 + (b & 0x1F);
    DUMP_BITS(5)
    NEED_BITS(4)
    nb = 4 + (b & 0xF);
    DUMP_BITS(4)
    if (nl > 286 || nd > 30)
        return 1;
    for (j = 0; j < nb; j++) {
        NEED_BITS(3)
        ll[code_length_order[j]] = b & 7;
        DUMP_BITS(3)
    }
    for (; j < 19; j++)
        ll[code_length_order[j]] = 0;

    bl = 7;
    if ((i = huffman_build(ll, 19, 19, 0, 0, &tl, &bl)) != 0) {
        if (i == 1)
            huffman_free(tl);
        return i;
    }

    n = nl + nd;
    m = bit_masks[bl];
    i = l = 0;
    while ((unsigned int)i < n) {
        NEED_BITS(bl)
        j = (td = tl + (b & m))->b;
        DUMP_BITS(j)
        j = td->v.n;
        if (j < 16) {
            ll[i++] = l = j;
        } else if (j == 16) {
            NEED_BITS(2)
            j = 3 + (b & 3);
            DUMP_BITS(2)
            if ((unsigned int)i + j > n)
                return 1;
            while (j--)
                ll[i++] = l;
        } else if (j == 17) {
            NEED_BITS(3)
            j = 3 + (b & 7);
            DUMP_BITS(3)
            if ((unsigned int)i + j > n)
                return 1;
            while (j--)
                ll[i++] = 0;
            l = 0;
        } else {
            NEED_BITS(7)
            j = 11 + (b & 0x7F);
            DUMP_BITS(7)
            if ((unsigned int)i + j > n)
                return 1;
            while (j--)
                ll[i++] = 0;
            l = 0;
        }
    }
    huffman_free(tl);
    bit_buf = b;
    bit_count = k;

    bl = lit_bits;
    if ((i = huffman_build(ll, nl, 257, length_base, length_extra, &tl, &bl)) != 0) {
        if (i == 1)
            return -1;
        return i;
    }
    bd = dist_bits;
    if ((i = huffman_build(ll + nl, nd, 0, dist_base, dist_extra, &td, &bd)) != 0) {
        if (i == 1)
            return -1;
        huffman_free(tl);
        return i;
    }
    if (inflate_codes(tl, td, bl, bd))
        return 1;
    huffman_free(tl);
    huffman_free(td);
    return 0;
}

int inflate_block(int *last)
{
    unsigned int t;
    unsigned int b;
    unsigned int k;

    b = bit_buf;
    k = bit_count;
    NEED_BITS(1)
    *last = b & 1;
    DUMP_BITS(1)
    NEED_BITS(2)
    t = b & 3;
    DUMP_BITS(2)
    bit_buf = b;
    bit_count = k;
    if (t == 2)
        return inflate_dynamic();
    if (t == 0)
        return inflate_stored();
    if (t == 1)
        return inflate_fixed();
    return 2;
}

int inflate(void)
{
    int last;
    int r;

    inflate_out = 0;
    bit_count = 0;
    bit_buf = 0;
    do {
        huffman_count = 0;
        if ((r = inflate_block(&last)) != 0)
            return r;
    } while (!last);
    /* give back the whole bytes read ahead */
    while (bit_count >= 8) {
        bit_count -= 8;
        inflate_in--;
    }
    return 0;
}

/* Unpacks a gzip member (no optional fields) to dst; its size, or -1. */
int gunzip(unsigned char *src, unsigned char *dst)
{
    unsigned char trailer[8];
    unsigned int pool[0x1000];
    int r;
    int i;
    int id1, id2;
    unsigned int size;

    huffman_pool = pool;
    inflate_src = src;
    inflate_dst = dst;
    inflate_in = 0;
    inflate_out = 0;
    id1 = NEXT_BYTE();
    id2 = NEXT_BYTE();
    if ((id1 != 0x1F) | (id2 != 0x8B))
        return -1;
    if (NEXT_BYTE() != 8)
        return -1;
    if (NEXT_BYTE() != 0)
        return -1;
    inflate_in = 10;
    r = inflate();
    if (r == 3 || r != 0)
        return -1;
    for (i = 0; i < 8; i++)
        trailer[i] = NEXT_BYTE();
    size = U16(trailer + 4) | U16(trailer + 6) << 16;
    if (size != inflate_out)
        return -1;
    return size;
}
