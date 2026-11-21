#ifndef LEVELUP_H
#define LEVELUP_H

/* Levelling up, which NAMEMENU.MNU (namelevel.c) and BATRES.X (batres.c) each carry a copy of: the EXP curve, then for
 * each level the stat, HP and MP gains from KERNEL.BIN's curves and tables. */

/* KERNEL.BIN section 2 as loaded: a record for each character, then the tables levelling up reads. */
typedef struct {
    unsigned char curves[9]; /* Str, Vit, Mag, Spr, Dex, Luck, HP, MP, EXP: rows of the curves below */
    unsigned char pad9;
    unsigned char join; /* bit 0: joins at the level in D_8009D28A; else at the party's average plus join / 2 */
    unsigned char padB;
    unsigned char limits[12];  /* limit ids, 0x80 on; 0xFF for none */
    unsigned short kills[2];   /* the kills that learn limit slots 3 and 6, 0xFFFF for never */
    unsigned short uses[3][2]; /* by limit level, the uses that learn its next two slots */
    unsigned char pad28[16];
} KernelCharacter;
extern KernelCharacter kernel_characters[9];
extern unsigned char kernel_stat_gains[12]; /* a stat's rise, by how far under its curve it is */
extern unsigned char kernel_hp_gains[12];   /* percentages of the HP curve's rise */
extern unsigned char kernel_mp_gains[12];
extern struct {
    unsigned char gradient;
    signed char base;
} kernel_curves[64][8]; /* by the level's bracket */

/* A character as it levels. */
typedef struct {
    int bracket; /* where the level falls on the curves: tens to 60, then twenties */
    unsigned char id, level;
    unsigned short pad6;
    unsigned short base[8];  /* Str, Vit, Mag, Spr, Dex, Luck, HP, MP as they were */
    unsigned short stats[8]; /* and as they grow */
    KernelCharacter *kernel;
    unsigned int exp;
    unsigned int total; /* exp plus gain */
    unsigned int gain;
    unsigned int start;      /* the EXP of the level it had */
    unsigned int levels[16]; /* each new level's EXP */
    unsigned int reached;    /* the EXP of the level reached */
    unsigned int next;       /* and of the one after */
} Growth;

#endif
