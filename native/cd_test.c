/* Checks the CD layer on the PC (cd.c over native/cd_native.c) against the extracted discs.
 *
 * usage: cd_test SCUS_941.63 ASSETS
 *
 * For each disc, as the save's disc: the disc check passes and reports that disc; every file reads back through
 * cd_read and cd_step byte for byte (the movies with XA audio are left to the movie player); and every LZS file
 * unpacked while it streams in (cd_read_lzs) matches the same file unpacked in memory (lzs_decode). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "sound.h"
#include "cd.h"
#include "cd_native.h"

#define RAM_BASE 0x80000000u
#define RAM(addr) (snd_ram + ((addr) - RAM_BASE))
#define SAVE_DISC 0x8009D588u

unsigned char snd_ram[0x200000];
SoundCommand snd_request;

int cd_read(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int cd_read_lzs(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int cd_step(void);
void cd_init(void);
void cd_stop(void);
int cd_check_disc(void);
int get_disc_number(void);
void lzs_decode(unsigned char *src, unsigned char *dst);

int snd_submit_request(void) { return 0; }
int VSync(int mode) { (void)mode; return 0; }

static int callbacks;
static void count_callback(void) { callbacks++; }

static unsigned char *read_file(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    unsigned char *data;

    if (!f) {
        perror(path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    rewind(f);
    data = malloc(*size + 4);
    if (fread(data, 1, *size, f) != (size_t)*size) {
        perror(path);
        exit(1);
    }
    fclose(f);
    return data;
}

int main(int argc, char **argv)
{
    static unsigned char got[0x2000000], want[0x400000];
    unsigned char *exe, *data;
    unsigned int load, length;
    const PcFile *files;
    char path[700];
    long size;
    int disc, count, i, failed = 0, read = 0, unpacked = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: cd_test SCUS_941.63 ASSETS\n");
        return 2;
    }
    exe = read_file(argv[1], &size);
    memcpy(&load, exe + 0x18, 4);
    memcpy(&length, exe + 0x1C, 4);
    memcpy(RAM(load), exe + 0x800, length);
    if (pc_cd_open(argv[2]) != 3) {
        fprintf(stderr, "%s: expected three extracted discs with files.txt\n", argv[2]);
        return 1;
    }
    for (disc = 1; disc <= 3; disc++) {
        *RAM(SAVE_DISC) = disc;
        cd_init();
        cd_stop();
        if (cd_check_disc() || get_disc_number() != disc || cd_request.disc != disc) {
            printf("disc %d: the disc check fails\n", disc);
            failed++;
        }
        files = pc_cd_files(disc, &count);
        for (i = 0; i < count; i++) {
            if (files[i].raw || files[i].size > sizeof(got))
                continue;
            snprintf(path, sizeof(path), "%s/%s", pc_cd_folder(disc), files[i].path);
            data = read_file(path, &size);
            callbacks = 0;
            cd_read(files[i].lba, files[i].size, got, count_callback);
            while (cd_step())
                ;
            if (memcmp(got, data, size) || callbacks != 1) {
                printf("disc %d: %s reads back wrong\n", disc, files[i].path);
                failed++;
            }
            read++;
            /* An LZS file starts with its packed length: the file's size minus that word. */
            if (size > 4 && !memcmp(data, &(unsigned int){size - 4}, 4)) {
                memset(got, 0, sizeof(want));
                memset(want, 0, sizeof(want));
                cd_read_lzs(files[i].lba, files[i].size, got, count_callback);
                while (cd_step())
                    ;
                lzs_decode(data, want);
                if (memcmp(got, want, sizeof(want)) || callbacks != 2) {
                    printf("disc %d: %s unpacks wrong while streaming\n", disc, files[i].path);
                    failed++;
                }
                unpacked++;
            }
            free(data);
        }
    }
    printf("%d files read, %d unpacked while streaming, %d failures\n", read, unpacked, failed);
    return failed != 0;
}
