#include "common.h"
#include "sound.h"

/* The game's boot file: it loads the sound driver's files and runs the main loop. Built with ASPSX 2.34 (`li` as
 * `ori`, indexed loads folded into `%lo`); GCC 2.7.2 and 2.6.3 both match so far. */

/* Where a file is on the disc. */
typedef struct DiscFile {
    unsigned int lba;
    unsigned int size;
} DiscFile;

extern DiscFile instr_all_file;
extern DiscFile effect_all_file;
extern DiscFile instr_dat_file;
extern DiscFile instr2_all_file;
extern DiscFile instr2_dat_file;
extern DiscFile disc_files[]; /* by number */
extern unsigned short D_800965E8;
extern unsigned short D_80071A5C;
extern unsigned int D_80071744;
extern unsigned int D_80095DD8;
extern void *D_800722C8;

void func_80033E34(unsigned int lba, unsigned int size, void *dest, void (*callback)(void)); /* starts a disc read */
int func_80034B44(void); /* a disc read is still running */
void func_80033DAC(unsigned int lba, int mode);
int DrawSync(int mode);
void copy_words(int *dst, int *src, int bytes);
void func_80015CA0(void *src, void *dest); /* unpacks a module */
void func_80015C3C(void *src, void *dest, int n);
void func_800140F4(void);
void func_800148A0(void);
void func_80044000(void *rect, unsigned int *data); /* VRAM upload */
void func_8001155C(void);
void StopCallback(void);
void ResetCallback(void);
void VSyncCallback(void (*callback)(void));
void SetDispMask(int mask);
void InitGeom(void);
void func_80043938(int mode);
void func_80036298(void);
void func_80043BA8(int mode);
/* Entry points of the modules run_overlay starts. */
void D_800A00CC(void);
void D_800A1158(void);

/* Shared with the VSync callback, hence volatile: the game reloads them and reads shorts through lhu. */
extern volatile short game_mode; /* 2 and 4 pick the module run_overlay starts next */
extern volatile short D_8009AC40;
extern volatile short D_8009AC42; /* a fade level, 0-255 */
extern volatile short D_8009AC44; /* and its step */
extern volatile short D_80114488;
extern unsigned char D_8009AC2C;
extern unsigned char D_8009C738[];
extern short D_80095DD4;


void play_effect_alone(unsigned short id)
{
    snd_request.id = 0xF1;
    snd_submit_request();
    snd_request.id = 0x20;
    snd_request.arg[0] = 0x40;
    snd_request.arg[1] = id;
    snd_submit_request();
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_800111E4);

void snd_load_bank2(void)
{
    func_80033E34(instr2_dat_file.lba, instr2_dat_file.size, (void *)0x800E0000, 0);
    while (func_80034B44())
        ;
    func_80033E34(instr2_all_file.lba, instr2_all_file.size, (void *)0x800A0000, 0);
    while (func_80034B44())
        ;
    load_instrument_bank2((unsigned int *)0x800A0000, (unsigned int *)0x800E0000);
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_800112E8);

INCLUDE_ASM("asm/nonmatchings/main/main", func_8001146C);

INCLUDE_ASM("asm/nonmatchings/main/main", func_8001155C);

void func_8001171C(void)
{
    StopCallback();
    ResetCallback();
    func_80043938(0);
    func_80036298();
    D_80095DD4 = 0;
    VSyncCallback(func_8001155C);
    func_80043BA8(0);
    SetDispMask(0);
    InitGeom();
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_80011784);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80011860);

void func_80011920(void)
{
    D_800965E8 = 0;
    D_80071A5C = 0;
}

void load_sound_files(void)
{
    func_80033E34(instr_all_file.lba, instr_all_file.size, (void *)0x800F0000, 0);
    while (func_80034B44())
        ;
    func_80033E34(effect_all_file.lba, effect_all_file.size, (void *)0x801B0000, 0);
    while (func_80034B44())
        ;
    func_80033E34(instr_dat_file.lba, instr_dat_file.size, (void *)0x801BC800, 0);
    while (func_80034B44())
        ;
    snd_init((unsigned int *)0x800F0000, (unsigned int *)0x801BC800);
    snd_load_effects((unsigned int *)0x801B0000);
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_800119E4);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80011AEC);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80011BB4);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80011C1C);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80012840);

INCLUDE_ASM("asm/nonmatchings/main/main", func_800128B8);

INCLUDE_ASM("asm/nonmatchings/main/main", func_800129D0);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80012A8C);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80012DB0);

INCLUDE_ASM("asm/nonmatchings/main/main", func_800131B8);

void func_800134F4(void)
{
    D_8009AC42 -= D_8009AC44;
    D_8009AC2C = 0;
    if (D_8009AC42 <= 0 || D_80114488 == 1) {
        D_8009AC40 = 0;
        D_8009AC42 = 0;
    }
}

void func_80013564(void)
{
    /* Through a pointer here, unlike func_800134F4: the game puts the address in a register. */
    *(volatile unsigned char *)&D_8009AC2C = 0;
    D_8009AC42 += D_8009AC44;
    if (D_8009AC42 >= 0x100)
        D_8009AC42 = 0xFF;
}

INCLUDE_ASM("asm/nonmatchings/main/main", func_800135C0);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80013624);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80013800);

INCLUDE_ASM("asm/nonmatchings/main/main", func_800138EC);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80013C9C);

INCLUDE_ASM("asm/nonmatchings/main/main", func_800140A4);

INCLUDE_ASM("asm/nonmatchings/main/main", func_800140F4);

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

INCLUDE_ASM("asm/nonmatchings/main/main", func_80014750);

INCLUDE_ASM("asm/nonmatchings/main/main", func_80014804);

INCLUDE_ASM("asm/nonmatchings/main/main", func_800148A0);

INCLUDE_ASM("asm/nonmatchings/main/main", func_800148B4);

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
