/* SAVEMENU.MNU's card layer, 0x801D1950-0x801D2B58 (its strings at 0x801D0050-0x801D03B8): the module's second
 * object, between the save screen (savemenu.c) and the load screen (saveload.c), which both use it. The game's
 * saves are BASCUS-94163FF7-S01 to S15 on either card. A file is one block: the card's 0x200-byte header (title,
 * icon), then 0x10F4 bytes of the save's data from 0x8009C6E4, whose first 0x54 bytes (SavePreview) are what the
 * file lists show. GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "menu.h"

#ifdef NATIVE
/* The game's own file and card calls: the PC build's recompiled exe names these apart from the host C library's. */
#define open psx_open
#define read psx_read
#define write psx_write
#define close psx_close
#define _bu_init psx__bu_init
#define _card_auto psx__card_auto
#endif

struct DIRENTRY {
    char name[20];
    int attr;
    int size;
    struct DIRENTRY *next;
    int head;
    char system[4];
};

int sprintf(char *buf, const char *fmt, ...);
void *memcpy(void *dst, const void *src, unsigned int n);
int open(char *name, int mode);
int read(int fd, void *buf, int n);
int write(int fd, void *buf, int n);
int close(int fd);
int delete(char *name);
struct DIRENTRY *firstfile(char *name, struct DIRENTRY *dir);
struct DIRENTRY *nextfile(struct DIRENTRY *dir);
void EnterCriticalSection(void);
void ExitCriticalSection(void);
int OpenEvent(unsigned int desc, int spec, int mode, unsigned int (*handler)(void));
int EnableEvent(int event);
int TestEvent(int event);
void func_800489F0(int shared); /* InitCARD */
void func_80048A44(void);       /* StartCARD */
void ChangeClearPAD(int on);
void _bu_init(void);
int _card_auto(int on);
int func_80023788(int t); /* the play time's hours */
int func_8002382C(int t); /* and minutes */

/* The card's file header: the title frame, then the icon's frames. */
typedef struct CardHeader {
    unsigned char magic[2]; /* "SC" */
    unsigned char icon_flag;
    unsigned char blocks;
    unsigned char title[64]; /* Shift-JIS */
    unsigned char pad44[28];
    unsigned char clut[32];
    unsigned char icon[3][128];
} CardHeader;

typedef struct CardIcon {
    unsigned char clut[32];
    unsigned char pad20[12];
    unsigned char image[128];
    unsigned char padAC[0x34A];
} CardIcon;

extern volatile unsigned char D_80062D99;   /* nonzero stops the play time */
extern int D_80062DCC;             /* the card events are open */
extern unsigned int D_8009D264;    /* the play time in seconds */
extern int card_events[8];         /* D_8009A024: the BIOS's and the hardware's, each done, error, new card, timeout */
extern PartyMember members[3];
extern unsigned int save_gil;
extern unsigned char card_sjis[]; /* Shift-JIS for the font's characters, two bytes each */
extern CardIcon card_icons[15];
extern char *card_names[15];
extern int card_text_max;
extern unsigned char *card_titles[15]; /* "ＦＦ７／ＳＡＶＥ０１／００：００" and so on */
extern SavePreview card_previews[15];
extern int card_slot;
extern CardHeader card_header;
extern unsigned char card_buf[0x2000];
extern unsigned char card_status[2][3];
extern int card_len;

INCLUDE_RODATA("asm/menu/savemenu/nonmatchings/savecard", card_name_text);

/* CRC-16 (polynomial 0x1021, from 0xFFFF, inverted): the save's first word, over the rest. */
unsigned short card_checksum(unsigned short len, unsigned char *data)
{
    int crc = 0xFFFF;
    unsigned short i, j;

    for (i = 0; i < len; i++) {
        crc ^= data[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = crc << 1 ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return ~crc;
}

void card_start(void)
{
    int i;

    if (!D_80062DCC) {
        EnterCriticalSection();
        card_events[0] = OpenEvent(0xF4000001, 4, 0x2000, 0);
        card_events[1] = OpenEvent(0xF4000001, 0x8000, 0x2000, 0);
        card_events[2] = OpenEvent(0xF4000001, 0x100, 0x2000, 0);
        card_events[3] = OpenEvent(0xF4000001, 0x2000, 0x2000, 0);
        card_events[4] = OpenEvent(0xF0000011, 4, 0x2000, 0);
        card_events[5] = OpenEvent(0xF0000011, 0x8000, 0x2000, 0);
        card_events[6] = OpenEvent(0xF0000011, 0x100, 0x2000, 0);
        card_events[7] = OpenEvent(0xF0000011, 0x2000, 0x2000, 0);
        func_800489F0(1);
        func_80048A44();
        ChangeClearPAD(0);
        _bu_init();
        _card_auto(0);
        for (i = 0; i < 8; i++)
            EnableEvent(card_events[i]);
        ExitCriticalSection();
        D_80062DCC = 1;
    }
    for (i = 0; i < 2; i++) {
        card_status[i][0] = 0;
        card_status[i][1] = 0;
        card_status[i][2] = 0;
    }
}

void card_stop(void)
{
}

int card_test(int unused, int event)
{
    return TestEvent(card_events[event]);
}

int card_same_name(char *a, char *b)
{
    do {
        if (*a++ != *b++)
            return 0;
    } while (*a || *b);
    return 1;
}

/* Bit n set: the card has save n+1. */
unsigned short card_list(int card)
{
    DMA_BUFFER(dir, sizeof(struct DIRENTRY));
    struct DIRENTRY *d;
    int found = 0;
    int i;

    for (i = 0; i < 100; i++) {
        d = firstfile(card ? "bu10:*" : "bu00:*", (struct DIRENTRY *)dir);
        if (d)
            break;
    }
    if (d) {
        while (d) {
            for (i = 0; i < 15; i++) {
                if (card_same_name(d->name, card_names[i]))
                    found |= 1 << i;
            }
            d = nextfile(d);
        }
    } else
        found = 0;
    DMA_BUFFER_END(sizeof(struct DIRENTRY));
    return found;
}

SavePreview *card_preview(int slot)
{
    return &card_previews[slot];
}

/* The files' paths, by card; arrays, not literals, so card_read_save's asm can reach them by name. */
const char card_path1[] = "bu10:%s";
const char card_path0[] = "bu00:%s";

/* Slot: the save's number (0-14), plus 0x10 for the second card. 0 read, 1 no file, 2 the read failed. */
int card_read_preview(int slot)
{
    struct DIRENTRY unused;
    DMA_BUFFER(path, 32);
    int fd, n, retry;

    card_len = 0x280;
    if (slot & 0x10)
        sprintf(path, card_path1, card_names[slot & 0xF]);
    else
        sprintf(path, card_path0, card_names[slot & 0xF]);
    fd = open(path, 1);
    if (fd == -1) {
        DMA_BUFFER_END(32);
        return 1;
    }
    retry = 30;
    while ((n = read(fd, card_buf, card_len)) != card_len) {
        retry--;
        if (n != -1)
            card_len -= n;
        if (!retry) {
            close(fd);
            DMA_BUFFER_END(32);
            return 2;
        }
    }
    close(fd);
    memcpy(&card_previews[slot & 0xF], card_buf + 0x200, sizeof(SavePreview));
    DMA_BUFFER_END(32);
    return 0;
}

/* The same for the whole save, into the game; its window colours take effect at once. */
#ifdef NON_MATCHING
/* Same length; the game gives the file and the retry count each other's registers. */
int card_read_save(int slot)
{
    struct DIRENTRY unused;
    DMA_BUFFER(path, 32);
    int fd, n, retry;

    card_len = 0x2000;
    if (slot & 0x10)
        sprintf(path, card_path1, card_names[slot & 0xF]);
    else
        sprintf(path, card_path0, card_names[slot & 0xF]);
    fd = open(path, 1);
    if (fd == -1) {
        DMA_BUFFER_END(32);
        return 1;
    }
    retry = 30;
    while ((n = read(fd, card_buf, card_len)) != card_len) {
        retry++; /* up from 30: in effect no limit */
        if (n != -1)
            card_len -= n;
        if (!retry) {
            close(fd);
            DMA_BUFFER_END(32);
            return 2;
        }
    }
    close(fd);
    memcpy(&save_data, card_buf + 0x200, 0x10F4);
    for (n = 0; n < 12; n++)
        D_80049208[n] = save_data.preview.colors[n];
    DMA_BUFFER_END(32);
    return 0;
}
#else
INCLUDE_ASM("asm/menu/savemenu/nonmatchings/savecard", card_read_save);
#endif

/* Unused: Shift-JIS lead bytes, the second from the second kanji level only. */
int card_lead_byte2(unsigned char c)
{
    if ((c >= 0x99 && c <= 0x9F) || (c >= 0xE1 && c <= 0xFC))
        return 1;
    return 0;
}

int card_lead_byte(unsigned char c)
{
    if ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC))
        return 1;
    return 0;
}

void card_copy_title(unsigned char *dst, unsigned char *src)
{
    int i;

    for (i = 0; i < 64; i++)
        *dst++ = *src++;
}

INCLUDE_RODATA("asm/menu/savemenu/nonmatchings/savecard", card_title_text);

void card_set_text_max(int n)
{
    card_text_max = n;
}

/* Game text up to its 0xFF end, at most card_text_max bytes. */
void card_copy_text(unsigned char *dst, unsigned char *src)
{
    int unused[1];
    int i;

    for (i = 0; i < card_text_max; i++) {
        *dst = *src;
        if (*src == 0xFF)
            break;
        dst++;
        src++;
    }
}

/* The preview from the game: the party's first member, the window colours, gil, time and place. */
void card_fill_preview(void)
{
    int i;
    unsigned char c;

    for (i = 0; i < 3; i++)
        save_data.preview.portraits[i] = save_data.party[i];
    card_set_text_max(16);
    for (i = 0; i < 3; i++) {
        c = save_data.party[i];
        if (c != 0xFF) {
            card_copy_text(save_data.preview.name, save_data.chars[c].name);
            save_data.preview.level = save_data.chars[c].level;
            save_data.preview.hp = members[i].hp;
            save_data.preview.max_hp = members[i].max_hp;
            save_data.preview.mp = members[i].mp;
            save_data.preview.max_mp = members[i].max_mp;
            break;
        }
    }
    for (i = 0; i < 12; i++)
        save_data.preview.colors[i] = D_80049208[i];
    save_data.preview.gil = save_gil;
    save_data.preview.seconds = D_8009D264;
    card_set_text_max(24);
    card_copy_text(save_data.preview.place, save_data.place);
}

/* Writes the save to path under title (with the play time put in). 0 saved, 1 the file couldn't be made, 2 nor
 * opened, 3 the write failed. An old file is deleted first, and a short write is retried from the buffer's start. */
short card_write_save(char *path, unsigned char *title)
{
    unsigned char unused[0x200];
    int i, fd, n, retry;
    unsigned char t, c;
    unsigned char *p;
    int k;

    card_fill_preview();
    card_len = 0x2000;
    card_header.magic[0] = 'S';
    card_header.magic[1] = 'C';
    card_header.icon_flag = 0x11;
    card_header.blocks = 1;
    card_copy_title(card_header.title, title);
    for (k = 27, p = card_header.pad44 + 27; k >= 0; k--)
        *p-- = 0;
    t = func_80023788(save_data.preview.seconds);
    c = (unsigned char)(t / 10) * 2 + 0x20;
    card_header.title[22] = card_sjis[c];
    card_header.title[23] = card_sjis[c + 1];
    c = (unsigned char)(t % 10) * 2 + 0x20;
    card_header.title[24] = card_sjis[c];
    card_header.title[25] = card_sjis[c + 1];
    t = func_8002382C(save_data.preview.seconds);
    c = (unsigned char)(t / 10) * 2 + 0x20;
    card_header.title[28] = card_sjis[c];
    card_header.title[29] = card_sjis[c + 1];
    c = (unsigned char)(t % 10) * 2 + 0x20;
    card_header.title[30] = card_sjis[c];
    card_header.title[31] = card_sjis[c + 1];
    memcpy(card_header.clut, card_icons[card_slot].clut, 32);
    memcpy(card_header.icon, card_icons[card_slot].image, 128);
    memcpy(card_buf, &card_header, 0x200);
    D_80062D99 = 1;
    save_data.preview.checksum = card_checksum(0x10F0, &save_data.preview.level);
    memcpy(card_buf + 0x200, &save_data, 0x10F4);
    D_80062D99 = 0;
    for (i = 0; i < 10; i++) {
        fd = open(path, 1);
        if (fd != -1) {
            delete(path);
            close(fd);
            break;
        }
    }
    retry = 30;
    while ((fd = open(path, card_header.blocks << 16 | 0x200)) == -1) {
        if (!--retry)
            return 1;
    }
    close(fd);
    retry = 30;
    while ((fd = open(path, 2)) == -1) {
        if (!--retry)
            return 2;
    }
    retry = 30;
    while ((n = write(fd, card_buf, card_len)) != card_len) {
        retry--;
        if (n != -1)
            card_len -= n;
        if (!retry) {
            close(fd);
            return 3;
        }
    }
    close(fd);
    return 0;
}

short card_save(int slot)
{
    DMA_BUFFER(path, 64);
    short r;

    if (slot & 0x10)
        sprintf(path, card_path1, card_names[slot & 0xF]);
    else
        sprintf(path, card_path0, card_names[slot & 0xF]);
    slot &= 0xF;
    card_slot = slot;
    r = card_write_save(path, card_titles[slot]);
    if (!r)
        card_previews[slot] = save_data.preview;
    DMA_BUFFER_END(64);
    return r;
}
