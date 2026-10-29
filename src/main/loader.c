#include "common.h"
#include "main.h"

/* Disc reads and module starts (0x800144D8-0x80014A00). Assembled by ASPSX 2.34, unlike main.c: an indexed load folds
 * into `%lo` (`lui at; addu at, at, a0; lw v0, %lo(sym)(at)`). func_800148A0/func_800148B4 reach small globals through
 * $gp, so the file is -G8; it builds as -G0 until they are C, since -G8 puts INCLUDE_ASM out of order. */

extern unsigned int D_80071744;
extern unsigned int D_80095DD8;
extern void *D_800722C8;
extern unsigned char D_8009C738[];

void func_80033DAC(unsigned int lba, int mode);
void copy_words(int *dst, int *src, int bytes);
void func_80015C3C(void *src, void *dest, int n);
void func_800148A0(void);
void func_80044000(void *rect, unsigned int *data); /* VRAM upload */
/* Entry points of the modules run_overlay starts. */
void D_800A00CC(void);
void D_800A1158(void);

unsigned int disc_file_lba(int i)
{
    return disc_files[i].lba;
}

void func_800144F0(unsigned int lba)
{
    func_80033DAC(lba, 0);
}

void func_80014510(int i)
{
    func_800144F0(disc_files[i].lba);
}

void func_80014540(void)
{
    func_80033E34(D_80071744, D_80095DD8, D_800722C8, 0);
}

void read_disc_file(int i, void *dest, void (*callback)(void))
{
    func_80033E34(disc_files[i].lba, disc_files[i].size, dest, callback);
}

void wait_disc_read(void (*idle)(void))
{
    while (func_80034B44()) {
        if (idle)
            idle();
    }
}

void func_80014608(void)
{
}

void load_disc_table(void)
{
    int sector[0x800 / 4];

    func_80033E34(0x266, 0x800, sector, 0);
    wait_disc_read(0);
    copy_words((int *)disc_files, sector, 0x50);
}

void run_overlay(int file, void (*entry)(void))
{
    read_disc_file(file, (void *)0x801B0000, 0);
    wait_disc_read(0);
    func_80015CA0((void *)0x801B0000, (void *)0x800A0000);
    entry();
}

void func_800146A4(void)
{
    int running = -1;

    do {
        switch (game_mode) {
        case 4:
            wait_disc_read(0);
            run_overlay(3, D_800A00CC);
            break;
        case 2:
            func_800140F4();
            run_overlay(5, D_800A1158);
            break;
        default:
            running = 0;
        }
    } while (running);
}

INCLUDE_ASM("asm/nonmatchings/main/loader", func_80014750);

INCLUDE_ASM("asm/nonmatchings/main/loader", func_80014804);

INCLUDE_ASM("asm/nonmatchings/main/loader", func_800148A0);

INCLUDE_ASM("asm/nonmatchings/main/loader", func_800148B4);

void func_80014934(void)
{
    func_800148A0();
    read_disc_file(2, (void *)0x801B0000, 0);
    wait_disc_read(0);
    func_80015C3C((void *)0x801B0000, D_8009C738, 3);
}

void load_tim(unsigned int *tim)
{
    if (tim[1] & 8) {
        func_80044000(tim + 3, tim + 5);
        tim += tim[2] >> 2;
    }
    func_80044000(tim + 3, tim + 5);
}

void func_800149E0(void)
{
    DrawSync(0);
}
