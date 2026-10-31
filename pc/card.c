/* The memory cards as image files (card.h). Layout per psx-spx "Memory Card Data Format": block 0 is the "MC" frame,
 * 15 directory frames (one per data block), the broken sector list and its replacement sectors, and a write-test
 * copy of the "MC" frame; blocks 1-15 hold the files. Every change is saved to a temporary file that then replaces
 * the image, so a crash mid-save can't leave half a card. */
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "card.h"

#define CARD_SIZE 0x20000
#define BLOCK 0x2000
#define FRAME 0x80
#define BLOCKS 16
#define LAST_SECTOR 0x3FF
#define IN_USE_FIRST 0x51
#define IN_USE_MIDDLE 0x52
#define IN_USE_LAST 0x53
#define FREE 0xA0
#define DELETED 0xA0 /* plus 1-3, as in use */
#define NAME_SIZE 21
#define FILES 16

static unsigned char cards[2][CARD_SIZE];
static int loaded[2];
static struct {
    int open, port, first, size, pos;
} files[FILES];
static struct {
    char pattern[NAME_SIZE + 1];
    int port, next;
} search;

static void seal(unsigned char *frame)
{
    unsigned char x = 0;
    int i;

    for (i = 0; i < FRAME - 1; i++)
        x ^= frame[i];
    frame[FRAME - 1] = x;
}

static void format(unsigned char *card)
{
    unsigned char *f;
    int n;

    memset(card, 0, BLOCK);
    card[0] = 'M';
    card[1] = 'C';
    seal(card);
    for (n = 1; n < BLOCKS; n++) {
        f = card + n * FRAME;
        f[0] = FREE;
        f[8] = f[9] = 0xFF;
        seal(f);
    }
    for (n = 16; n < 36; n++) {
        f = card + n * FRAME;
        memset(f, 0xFF, 4);
        f[8] = f[9] = 0xFF;
        seal(f);
    }
    memset(card + 36 * FRAME, 0xFF, 27 * FRAME); /* replacement sectors and unused frames */
    memcpy(card + 63 * FRAME, card, FRAME);
}

static const char *image_path(int port) { return port ? "memcard2.mcd" : "memcard1.mcd"; }

static unsigned char *card(int port)
{
    FILE *f;

    if (!loaded[port]) {
        loaded[port] = 1;
        f = fopen(image_path(port), "rb");
        if (!f || fread(cards[port], 1, CARD_SIZE, f) != CARD_SIZE)
            format(cards[port]);
        if (f)
            fclose(f);
    }
    return cards[port];
}

static void save(int port)
{
    char tmp[32];
    FILE *f;

    sprintf(tmp, "%s.tmp", image_path(port));
    f = fopen(tmp, "wb");
    if (!f) {
        perror(tmp);
        return;
    }
    fwrite(cards[port], 1, CARD_SIZE, f);
    if (fclose(f)) {
        perror(tmp);
        return;
    }
#ifdef _WIN32
    MoveFileExA(tmp, image_path(port), MOVEFILE_REPLACE_EXISTING);
#else
    rename(tmp, image_path(port));
#endif
}

/* "bu00:" is slot 1, "bu10:" slot 2; anything else (multitap cards, other devices) isn't a card here. */
static int device(const char *path, const char **name)
{
    if (strncmp(path, "bu", 2) || (path[2] != '0' && path[2] != '1') || path[3] != '0' || path[4] != ':')
        return -1;
    *name = path + 5;
    return path[2] - '0';
}

static unsigned char *entry(int port, int block) { return card(port) + block * FRAME; }

/* "?" matches one character, "*" the rest of the name. */
static int matches(const char *pattern, const char *name)
{
    for (; *pattern && *pattern != '*'; pattern++, name++) {
        if (*pattern != '?' && *pattern != *name)
            return 0;
        if (!*name)
            return 1;
    }
    return *pattern == '*' || !*name;
}

/* The first block of a file in use whose name matches. */
static int lookup(int port, const char *pattern, int from)
{
    int b;

    for (b = from; b < BLOCKS; b++) {
        if (entry(port, b)[0] == IN_USE_FIRST && matches(pattern, (char *)entry(port, b) + 0x0A))
            return b;
    }
    return 0;
}

int card_formatted(int port) { return card(port)[0] == 'M' && card(port)[1] == 'C'; }

static int create(int port, const char *name, int blocks)
{
    unsigned char *e;
    int b, n = 0, chosen[BLOCKS];

    if (blocks < 1 || strlen(name) >= NAME_SIZE || lookup(port, name, 1))
        return 0;
    for (b = 1; b < BLOCKS && n < blocks; b++)
        if (entry(port, b)[0] >= FREE)
            chosen[n++] = b;
    if (n < blocks)
        return 0;
    for (n = 0; n < blocks; n++) {
        e = entry(port, chosen[n]);
        memset(e, 0, FRAME);
        e[0] = !n ? IN_USE_FIRST : n == blocks - 1 ? IN_USE_LAST : IN_USE_MIDDLE;
        e[8] = n == blocks - 1 ? 0xFF : chosen[n + 1] - 1;
        e[9] = n == blocks - 1 ? 0xFF : 0;
        if (!n) {
            e[4] = blocks * BLOCK & 0xFF;
            e[5] = blocks * BLOCK >> 8 & 0xFF;
            e[6] = blocks * BLOCK >> 16;
            strcpy((char *)e + 0x0A, name);
        }
        seal(e);
    }
    save(port);
    return chosen[0];
}

int card_open(const char *path, unsigned int mode)
{
    const char *name;
    int port = device(path, &name), first, fd;

    if (port < 0 || !card_formatted(port))
        return -1;
    first = mode & 0x200 ? create(port, name, mode >> 16) : lookup(port, name, 1);
    if (!first)
        return -1;
    for (fd = 2; fd < FILES && files[fd].open; fd++)
        ;
    if (fd == FILES)
        return -1;
    files[fd].open = 1;
    files[fd].port = port;
    files[fd].first = first;
    files[fd].size = entry(port, first)[4] | entry(port, first)[5] << 8 | entry(port, first)[6] << 16;
    files[fd].pos = 0;
    return fd;
}

/* Where a file's byte lies on the card, following its blocks. */
static unsigned char *at(int fd, int pos)
{
    int b = files[fd].first, n;

    for (n = pos / BLOCK; n; n--)
        b = entry(files[fd].port, b)[8] + 1;
    return card(files[fd].port) + b * BLOCK + pos % BLOCK;
}

/* Reads and writes go a sector at a time and need whole sectors inside the file. */
static int transfer(int fd, unsigned char *buf, int len, int writing)
{
    int n;

    if (fd < 2 || fd >= FILES || !files[fd].open || len <= 0 || len % FRAME || files[fd].pos + len > files[fd].size)
        return -1;
    for (n = 0; n < len; n += FRAME, files[fd].pos += FRAME) {
        if (writing)
            memcpy(at(fd, files[fd].pos), buf + n, FRAME);
        else
            memcpy(buf + n, at(fd, files[fd].pos), FRAME);
    }
    if (writing)
        save(files[fd].port);
    return len;
}

int card_read(int fd, void *dst, int len) { return transfer(fd, dst, len, 0); }
int card_write(int fd, const void *src, int len) { return transfer(fd, (unsigned char *)src, len, 1); }

int card_close(int fd)
{
    if (fd < 2 || fd >= FILES || !files[fd].open)
        return -1;
    files[fd].open = 0;
    return fd;
}

/* firstfile/nextfile's direntry: name, attribute 50h, size, a next pointer the BIOS leaves unused, first sector. */
int card_find(const char *pattern, int first, unsigned char direntry[40])
{
    const char *name;
    unsigned char *e;
    int b, size;

    if (first) {
        search.port = device(pattern, &name);
        if (search.port < 0 || strlen(name) > NAME_SIZE)
            return 0;
        strcpy(search.pattern, name);
        search.next = 1;
    }
    if (search.port < 0 || !card_formatted(search.port) || !(b = lookup(search.port, search.pattern, search.next)))
        return 0;
    search.next = b + 1;
    e = entry(search.port, b);
    size = e[4] | e[5] << 8 | e[6] << 16;
    memset(direntry, 0, 40);
    strcpy((char *)direntry, (char *)e + 0x0A);
    direntry[0x14] = 0x50;
    direntry[0x18] = size & 0xFF;
    direntry[0x19] = size >> 8 & 0xFF;
    direntry[0x1A] = size >> 16;
    direntry[0x20] = b * (BLOCK / FRAME) & 0xFF;
    direntry[0x21] = b * (BLOCK / FRAME) >> 8;
    return 1;
}

/* A deleted file keeps its name and links, so the BIOS could undelete it. */
int card_delete(const char *path)
{
    const char *name;
    unsigned char *e;
    int port = device(path, &name), b, last;

    if (port < 0 || !card_formatted(port) || !(b = lookup(port, name, 1)))
        return 0;
    do {
        e = entry(port, b);
        last = e[8] == 0xFF;
        e[0] = DELETED + e[0] - IN_USE_FIRST + 1;
        seal(e);
        b = e[8] + 1;
    } while (!last);
    save(port);
    return 1;
}

int card_format(const char *device_name)
{
    const char *name;
    int port = device(device_name, &name);

    if (port < 0)
        return 0;
    format(card(port));
    save(port);
    return 1;
}

int card_write_sector(int port, int sector, const void *src)
{
    if (sector < 0 || sector > LAST_SECTOR)
        return 0;
    memcpy(card(port) + sector * FRAME, src, FRAME);
    save(port);
    return 1;
}
