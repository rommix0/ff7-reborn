/* The memory card files: what the save menu does to them, and the image that ends up on disk. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "card.h"

#define SAVE "bu00:BASCUS-94163FF7-S01"
#define BLOCK 0x2000

static unsigned char image[0x20000], data[2 * BLOCK], back[2 * BLOCK];

static void load(const char *path)
{
    FILE *f = fopen(path, "rb");

    assert(f && fread(image, 1, sizeof image, f) == sizeof image);
    fclose(f);
}

static int frame_ok(const unsigned char *frame)
{
    unsigned char x = 0;
    int i;

    for (i = 0; i < 0x7F; i++)
        x ^= frame[i];
    return x == frame[0x7F];
}

static unsigned int u32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24; }

int main(void)
{
    unsigned char entry[40];
    int fd, i;

    remove("memcard1.mcd");
    remove("memcard2.mcd");
    for (i = 0; i < (int)sizeof data; i++)
        data[i] = i * 7 + (i >> 8);

    /* blank_card_is_formatted_and_empty */
    assert(card_formatted(0));
    assert(!card_find("bu00:*", 1, entry));

    /* save_menu_write: create (one block), close, open for writing, write it all, close */
    fd = card_open(SAVE, 0x10200);
    assert(fd >= 0 && card_close(fd) == fd);
    assert(card_open(SAVE, 0x10200) == -1); /* exists */
    fd = card_open(SAVE, 2);
    assert(card_write(fd, data, BLOCK) == BLOCK);
    assert(card_write(fd, data, 0x80) == -1); /* past its end */
    card_close(fd);

    /* save_menu_read: the header first, then all of it */
    fd = card_open(SAVE, 1);
    assert(card_read(fd, back, 0x280) == 0x280 && !memcmp(back, data, 0x280));
    assert(card_read(fd, back, 0x40) == -1); /* not whole sectors */
    card_close(fd);
    fd = card_open(SAVE, 1);
    assert(card_read(fd, back, BLOCK) == BLOCK && !memcmp(back, data, BLOCK));
    card_close(fd);
    assert(card_open("bu00:BASCUS-94163FF7-S02", 1) == -1);

    /* image_on_disk: the header frame, the file's directory frame, its data in block 1 */
    load("memcard1.mcd");
    assert(image[0] == 'M' && image[1] == 'C' && frame_ok(image) && image[0x7F] == 0x0E);
    assert(u32(image + 0x80) == 0x51 && u32(image + 0x84) == BLOCK && image[0x88] == 0xFF && image[0x89] == 0xFF);
    assert(!strcmp((char *)image + 0x8A, SAVE + 5) && frame_ok(image + 0x80));
    assert(u32(image + 0x100) == 0xA0 && frame_ok(image + 0x100));
    assert(u32(image + 16 * 0x80) == 0xFFFFFFFF && frame_ok(image + 16 * 0x80)); /* no broken sectors */
    assert(!memcmp(image + 63 * 0x80, image, 0x80));
    assert(!memcmp(image + BLOCK, data, BLOCK));

    /* firstfile_lists_it: name, attribute, size, first sector; a pattern that matches nothing */
    assert(card_find("bu00:BASCUS*", 1, entry));
    assert(!strcmp((char *)entry, SAVE + 5) && u32(entry + 0x14) == 0x50 && u32(entry + 0x18) == BLOCK);
    assert(u32(entry + 0x20) == 0x40);
    assert(!card_find("bu00:*", 0, entry));
    assert(!card_find("bu00:BASCUS-94163FF7-S0?X", 1, entry));
    assert(card_find("bu00:BASCUS-94163FF7-S0?", 1, entry));

    /* two_blocks_linked: the next file takes blocks 2 and 3 */
    fd = card_open("bu00:TWO", 0x20200);
    card_close(fd);
    fd = card_open("bu00:TWO", 2);
    assert(card_write(fd, data, 2 * BLOCK) == 2 * BLOCK);
    card_close(fd);
    load("memcard1.mcd");
    assert(u32(image + 0x100) == 0x51 && image[0x108] == 2 && u32(image + 0x180) == 0x53 && image[0x188] == 0xFF);
    assert(!memcmp(image + 2 * BLOCK, data, 2 * BLOCK));
    fd = card_open("bu00:TWO", 1);
    assert(card_read(fd, back, 0x80) == 0x80 && card_read(fd, back + 0x80, 2 * BLOCK - 0x80) == 2 * BLOCK - 0x80);
    assert(!memcmp(back, data, 2 * BLOCK));
    card_close(fd);

    /* delete_frees_and_creating_reuses: the save menu deletes the old file before creating the new one */
    assert(card_delete(SAVE) && !card_delete(SAVE));
    load("memcard1.mcd");
    assert(u32(image + 0x80) == 0xA1 && frame_ok(image + 0x80));
    assert(card_open(SAVE, 1) == -1);
    fd = card_open(SAVE, 0x10200);
    card_close(fd);
    load("memcard1.mcd");
    assert(u32(image + 0x80) == 0x51);

    /* full_card: 12 blocks are left, so 13 don't fit */
    assert(card_open("bu00:BIG", 0xD0200) == -1);
    fd = card_open("bu00:BIG", 0xC0200);
    assert(fd >= 0);
    card_close(fd);
    assert(card_open("bu00:ONE", 0x10200) == -1);

    /* format_clears_the_directory, and slot 2 is a card of its own */
    assert(card_format("bu00:"));
    assert(!card_find("bu00:*", 1, entry));
    load("memcard1.mcd");
    assert(u32(image + 0x80) == 0xA0 && image[0x88] == 0xFF);
    fd = card_open("bu10:SLOT2", 0x10200);
    card_close(fd);
    assert(card_find("bu10:*", 1, entry) && !card_find("bu00:*", 1, entry));

    /* write_sector: _card_clear's raw write of the write-test sector */
    assert(card_write_sector(0, 0x3F, data) && !card_write_sector(0, 0x401, data));
    load("memcard1.mcd");
    assert(!memcmp(image + 0x3F * 0x80, data, 0x80));

    remove("memcard1.mcd");
    remove("memcard2.mcd");
    puts("ok");
    return 0;
}
