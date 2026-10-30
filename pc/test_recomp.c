/* Differential test of the recompiler: functions recompiled from the user's exe against our matching C for them,
 * built for the PC. usage: test_recomp ASSETS */
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "recomp.h"

Ctx ctx;
unsigned int cop0[32];
void hw_poll(void) {}
void call_indirect(unsigned int addr) { fprintf(stderr, "indirect call to %08X\n", addr); exit(1); }
void recomp_missing(unsigned int addr) { fprintf(stderr, "missing %08X\n", addr); exit(1); }
void recomp_syscall(unsigned int code) { (void)code; }
void recomp_break(unsigned int code) { (void)code; }
void gte_cmd(unsigned int op) { (void)op; }
unsigned int gte_read(int reg) { return reg; }
void gte_write(int reg, unsigned int v) { (void)reg; (void)v; }
unsigned int gte_read_ctrl(int reg) { return reg; }
void gte_write_ctrl(int reg, unsigned int v) { (void)reg; (void)v; }
unsigned int io_read(unsigned int addr, int size) { (void)addr; (void)size; return 0; }
void io_write(unsigned int addr, unsigned int v, int size) { (void)addr; (void)v; (void)size; }

void copy_words(int *dst, int *src, int bytes);
int highest_bit(unsigned int x);
int count_bits(unsigned int x);
void lzs_decode(unsigned char *src, unsigned char *dst);
void r_80014A00(void);
void r_80014A38(void);
void r_80014A58(void);
void r_80034BB0(void);

static unsigned int recompiled(void (*f)(void), unsigned int a0, unsigned int a1, unsigned int a2)
{
    ctx.r[4] = a0;
    ctx.r[5] = a1;
    ctx.r[6] = a2;
    ctx.r[29] = 0x801FFF00;
    f();
    return ctx.r[2];
}

static unsigned int random_word(void)
{
    return (unsigned int)rand() << 30 ^ (unsigned int)rand() << 15 ^ rand();
}

static int check_lzs_folder(const char *folder)
{
    static unsigned char want[0x200000], got[0x200000];
    char path[1024];
    struct dirent *entry;
    DIR *dir = opendir(folder);
    unsigned char *data;
    unsigned int packed;
    FILE *f;
    long size;
    int checked = 0;

    if (!dir)
        return 0;
    while ((entry = readdir(dir))) {
        snprintf(path, sizeof(path), "%s/%s", folder, entry->d_name);
        if (!strstr(entry->d_name, ".DAT") || !(f = fopen(path, "rb")))
            continue;
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        rewind(f);
        data = malloc(size);
        assert(fread(data, 1, size, f) == (size_t)size);
        fclose(f);
        memcpy(&packed, data, 4);
        if (size > 4 && packed == size - 4) {
            memset(want, 0, sizeof(want));
            memset(got, 0, sizeof(got));
            lzs_decode(data, want);
            recompiled(r_80034BB0, (unsigned int)data, (unsigned int)got, 0);
            if (memcmp(want, got, sizeof(want))) {
                fprintf(stderr, "%s unpacks differently\n", path);
                exit(1);
            }
            checked++;
        }
        free(data);
    }
    closedir(dir);
    return checked;
}

int main(int argc, char **argv)
{
    int src[64], want[64], got[64];
    char folder[1024];
    struct dirent *entry;
    DIR *dir;
    unsigned int x;
    int i, n, lzs = 0;

    assert(argc == 2 && mem_map() == 0);
    srand(1);
    for (i = 0; i < 10000; i++) {
        x = random_word() >> (i % 32);
        assert(recompiled(r_80014A38, x, 0, 0) == (unsigned int)highest_bit(x));
        assert(recompiled(r_80014A58, x, 0, 0) == (unsigned int)count_bits(x));
    }
    for (i = 0; i < 1000; i++) {
        for (n = 0; n < 64; n++)
            src[n] = random_word();
        memset(want, 0, sizeof(want));
        memset(got, 0, sizeof(got));
        n = rand() % 65 * 4;
        copy_words(want, src, n);
        recompiled(r_80014A00, (unsigned int)got, (unsigned int)src, n);
        assert(!memcmp(want, got, sizeof(want)));
    }
    assert((dir = opendir(argv[1])));
    while ((entry = readdir(dir))) {
        snprintf(folder, sizeof(folder), "%s/%s/FIELD", argv[1], entry->d_name);
        if (entry->d_name[0] != '.')
            lzs += check_lzs_folder(folder);
    }
    closedir(dir);
    assert(lzs > 2000);
    printf("%d LZS files\nok\n", lzs);
    return 0;
}
