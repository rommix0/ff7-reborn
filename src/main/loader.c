#include "common.h"
#include "main.h"

/* Disc reads and module starts (0x800144D8-0x80014A00). Assembled by ASPSX 2.34, unlike main.c: an indexed load folds
 * into `%lo` (`lui at; addu at, at, a0; lw v0, %lo(sym)(at)`). Compiled with -G8: like sound_init.c, it defines its
 * small globals as tentative definitions, left common, so the linker resolves them to the data section's labels. */

int D_80062F88;
void *D_80063014; /* the scratchpad */

extern unsigned int D_80071744;
extern unsigned int D_80095DD8;
extern void *D_800722C8;
extern unsigned char D_8009C738[];
extern unsigned char *D_800707C0;
extern unsigned char D_80063048[];
extern void *D_80048DD4[]; /* where func_80014750 puts each entry of its file */

void func_80033DAC(unsigned int lba, int mode);
void copy_words(int *dst, int *src, int bytes);
void func_80015C3C(void *src, void *dest, int n);
void func_800148A0(void);
void func_80014C70(void);
void *func_80014C80(unsigned short size);
/* A file of numbered entries: start reading it, the next entry's number (0xFFFF at the end) and size, copy it out. */
void func_80015B44(void *file);
unsigned short func_80015B50(void);
unsigned short func_80015B88(void);
void func_80015BC0(void *dest);
void load_tim(unsigned int *tim);
void func_800149E0(void);
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

/* Read callback for file 2: each entry goes to its place in D_80048DD4, entry 9 to new memory of its size. */
void func_80014750(void)
{
    unsigned short id;
    int size;

    func_80015B44((void *)0x801B0000);
    while ((id = func_80015B50()) != 0xFFFF) {
        size = func_80015B88();
        if (id == 9)
            func_80015BC0(func_80014C80(size));
        else if (D_80048DD4[id])
            func_80015BC0(D_80048DD4[id]);
    }
}

/* Read callback for file 1: entry 0 is an image for VRAM, entry 1 goes to D_80063048. */
void func_80014804(void)
{
    unsigned short id;

    func_80015B44((void *)0x801B0000);
    while ((id = func_80015B50()) != 0xFFFF) {
        switch (id) {
        case 0:
            func_80015BC0((void *)0x801C0000);
            func_800149E0();
            load_tim((unsigned int *)0x801C0000);
            break;
        case 1:
            func_80015BC0(D_80063048);
            break;
        }
    }
}

void func_800148A0(void)
{
    D_80062F88 = 0;
    D_800707BE = 0;
}

int load_boot_files(void)
{
    func_800148A0();
    D_80063014 = (void *)0x1F800000;
    D_800707C0 = D_80063048;
    load_disc_table();
    func_80014C70();
    read_disc_file(1, (void *)0x801B0000, func_80014804);
    wait_disc_read(0);
    read_disc_file(2, (void *)0x801B0000, func_80014750);
    wait_disc_read(0);
    return 1;
}

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
