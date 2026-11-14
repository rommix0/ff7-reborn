#ifndef MAIN_H
#define MAIN_H

/* Where a file is on the disc. */
typedef struct DiscFile {
    unsigned int lba;
    unsigned int size;
} DiscFile;

extern DiscFile disc_files[]; /* by number */

/* A character in the save (0x8009C738, nine of them). */
typedef struct Character {
    unsigned char id;
    unsigned char level;
    unsigned char pad2[14];
    unsigned char name[12];
    unsigned char weapon;
    unsigned char armor;
    unsigned char pad1E[3];
    unsigned char exp_bar; /* the Status screen's bar */
    unsigned char pad22[10];
    unsigned short hp;
    unsigned char pad2E[2];
    unsigned short mp;
    unsigned char pad32[6];
    unsigned short max_hp;
    unsigned short max_mp;
    unsigned int exp;
    unsigned int weapon_materia[8]; /* the materia's id in the low byte, its AP above (0xFFFFFF once mastered) */
    unsigned int armor_materia[8];
    unsigned int exp_to_next;
} Character;
extern Character D_8009C738[];
extern unsigned char D_8009CBDC[]; /* the party: three character ids, 0xFF for an empty place */
extern int character_record[];     /* each character id's place in D_8009C738 */

/* A spell or command a member has, as the menus and battle list it. */
typedef struct MemberSpell {
    unsigned char id; /* 0xFF for none */
    unsigned char mp;
    unsigned char uses;
    unsigned char pad3[4];
    unsigned char flags; /* support materia's spread bits below; bits 5-7 MP Turbo's level, MP cost up a tenth each */
} MemberSpell;

typedef struct MemberCommand {
    unsigned char id; /* 0xFF for none */
    unsigned char pad1[3];
    unsigned char uses;
    unsigned char pad5;
} MemberCommand;

/* A support effect a paired materia gives (Counter, Added Effect and so on). */
typedef struct MemberSupport {
    unsigned char kind;
    unsigned char value;
    unsigned char param;
} MemberSupport;

/* A party member as worked out from the character, equipment and materia (0x8009D84C, three of them). */
typedef struct PartyMember {
    unsigned char pad0[2];
    unsigned char stat_bonus[4]; /* added to stats[] */
    unsigned char pad6[2];
    short stats[4];
    short hp;
    short max_hp;
    short mp;
    short max_mp;
    unsigned char pad18[0xB];
    unsigned char flags23; /* 8: HP and MP swapped */
    MemberSupport support[8];
    unsigned short elements[4]; /* bits of D_800491B0: [0] the weapon's, [1]-[3] the armor's by the support's level */
    unsigned int statuses[2];   /* the weapon's, the armor's */
    MemberCommand commands[16];
    unsigned char padAC[0x5C];
    MemberSpell spells[72];
    unsigned char pad348[0xC6];
    unsigned char weapon_growth; /* the weapon's materia AP: 0 none, 1 normal, 2 double, 3 triple */
    unsigned char pad40F[0x15];
    unsigned char weapon_pairs[8]; /* the weapon's slots, two by two: linked or not */
    unsigned char pad42C[0xB];
    unsigned char encounter_less; /* taken from effects[1]'s sum */
    unsigned char pad438[4];
    unsigned char effects[4]; /* support materia the field reads, summed over the party; [1] the encounter rate */
} PartyMember;
extern PartyMember *cur_member;      /* the member being worked on */
extern unsigned char materia_detail; /* nonzero: materia_level also writes the level, levels and AP to next */
PartyMember *party_member(int slot);
int materia_level(unsigned char id, int ap);

/* libgpu's display and drawing environments (layouts from Sony's Libref). */
typedef struct RECT {
    short x, y, w, h;
} RECT;

typedef struct DISPENV {
    RECT disp;
    RECT screen;
    unsigned char isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct DRAWENV {
    RECT clip;
    short ofs[2];
    RECT tw;
    unsigned short tpage;
    unsigned char dtd, dfe, isbg, r0, g0, b0;
    unsigned int dr_env[16];
} DRAWENV;

/* Picks what runs next (func_800146A4). Shared with the VSync callback, hence volatile: the game reloads it and reads
 * it through lhu. */
extern volatile short game_mode;
/* Written twice in a row by func_800111E4, so volatile. */
extern volatile short D_800707BE;

/* The CD layer (cd.c): start a request, then poll cd_step until it returns 0. */
int cd_read(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
int cd_seek(unsigned int lba, void (*callback)(void));
int cd_step(void);
void read_disc_file(int i, void *dest, void (*callback)(void));
void wait_disc_read(void (*idle)(void));
void unpack_module(void *src, void *dest); /* unpacks a module */
void func_800140F4(void);
int DrawSync(int mode);
void ClearImage(RECT *rect, int r, int g, int b);

#endif
