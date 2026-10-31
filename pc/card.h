#ifndef PC_CARD_H
#define PC_CARD_H

/* The memory cards as files: slot 1 is memcard1.mcd and slot 2 memcard2.mcd in the working directory, each a raw
 * 128 KB card image (psx-spx, "Memory Card Data Format"), the format emulators use. A missing image is a freshly
 * formatted card, written out on its first change. These are the BIOS's file functions for "bu00:" and "bu10:"
 * (psx-spx, "BIOS File Functions"), synchronous, in whole 128-byte sectors. */
int card_formatted(int port);                           /* port 0 or 1: the image starts with the "MC" frame */
int card_open(const char *path, unsigned int mode);     /* fd, or -1; mode bit 9 creates (bits 16-31 blocks) */
int card_read(int fd, void *dst, int len);              /* bytes read, or -1 */
int card_write(int fd, const void *src, int len);       /* bytes written, or -1 */
int card_close(int fd);                                 /* fd, or -1 */
int card_find(const char *pattern, int first, unsigned char direntry[40]); /* 1 found, 0 none: firstfile/nextfile */
int card_delete(const char *path);                      /* 1 or 0 */
int card_format(const char *device);                    /* 1 or 0 */
int card_write_sector(int port, int sector, const void *src); /* 1 or 0 */

#endif
