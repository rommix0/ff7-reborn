#ifndef MAIN_H
#define MAIN_H

/* Where a file is on the disc. */
typedef struct DiscFile {
    unsigned int lba;
    unsigned int size;
} DiscFile;

extern DiscFile disc_files[]; /* by number */
/* A file's place and size as call arguments. The game takes each field's address on its own (two registers where
 * one would do), as `*&` makes GCC do. */
#define DISC_FILE(f) *&(f).lba, *&(f).size

/* A character in the save (0x8009C738, nine of them). */
typedef struct Character {
    unsigned char id;
    unsigned char level;
    unsigned char stats[6];      /* Str, Vit, Mag, Spr, Dex, Luck */
    unsigned char stat_bonus[6]; /* what sources have added to them */
    unsigned char limit_level;
    unsigned char limit_bar; /* 0xFF when full */
    unsigned char name[12];
    unsigned char weapon;
    unsigned char armor;
    unsigned char accessory; /* 0xFF for none */
    unsigned char flags;     /* 0x10 Sadness, 0x20 Fury */
    unsigned char row; /* bit 0 clear: the back row */
    unsigned char exp_bar; /* the Status screen's bar */
    unsigned short limits; /* learned, a bit each by limit_id's slot */
    unsigned short kills;
    unsigned short limit_uses[3]; /* by limit level */
    unsigned short hp;
    unsigned short base_hp; /* before materia */
    unsigned short mp;
    unsigned short base_mp;
    unsigned char pad34[4];
    unsigned short max_hp;
    unsigned short max_mp;
    unsigned int exp;
    unsigned int weapon_materia[8]; /* the materia's id in the low byte, its AP above (0xFFFFFF once mastered) */
    unsigned int armor_materia[8];
    unsigned int exp_to_next;
} Character;
extern Character D_8009C738[];
/* The party (D_8009CBDC) is declared in the files that read it: menu.c needs it as part of a larger struct. */
extern int character_record[];     /* each character id's place in D_8009C738 */

/* What the results screen counts from and to, set up before it starts. */
typedef struct {
    unsigned int exp[4];       /* as counted so far */
    unsigned int exp_end[4];   /* with the battle's EXP */
    unsigned int levels[4][12]; /* the EXP of each level from the one the member had */
    unsigned char frac[4];     /* the count's fractions */
    unsigned char level[4];    /* the level the member had */
} ExpTally;
extern ExpTally exp_tally;

extern unsigned short config; /* the Config menu's settings: bits 2-3 the controller mapping, 10-12 Magic order */
/* The save's settings around the Config menu's (config is its own symbol too). */
extern struct {
    unsigned char battle_speed;         /* 0 fastest, 0xFF slowest */
    unsigned char battle_message_speed;
    unsigned short config;              /* bits 0-1 Sound, 2-3 Controller, 4-5 Cursor, 6-7 ATB, 8-9 Camera angle,
                                         * 10-12 Magic order */
    unsigned char button_map[16];       /* by bit, the bit it becomes */
    unsigned char message_speed;
} settings;

/* A menu's cursor as func_800264A8 moves it. */
typedef struct MenuCursor {
    short pad0;
    short top;   /* the first row shown */
    short pad4;
    short total; /* rows in all */
    short scrolling; /* 1 up a row, 2 down, over four frames */
    signed char col;
    signed char row;
    signed char cols, rows; /* shown */
    unsigned char padE;
    signed char scroll_step; /* how far through the scroll */
    signed char wrap_x, wrap_y; /* 0 stop at the edge (wrap_y 0 scrolls the list), 1 wrap, 2 on to the next row */
} MenuCursor;

/* A spell or command a member has, as the menus and battle list it. */
typedef struct MemberSpell {
    unsigned char id; /* 0xFF for none */
    unsigned char mp;
    unsigned char uses; /* All's uses on a spell, a summon's uses */
    unsigned char quadra;
    unsigned char quadra_uses;
    unsigned char targets; /* from spell_data */
    unsigned char pad6;
    unsigned char flags; /* support materia's spread bits below; bits 5-7 MP Turbo's level, MP cost up a tenth each */
} MemberSpell;

typedef struct MemberCommand {
    unsigned char id; /* 0xFF for none */
    unsigned char cursor;  /* from command_data */
    unsigned char targets; /* from command_data */
    unsigned char pad3;
    unsigned char uses;
    unsigned char flags; /* support materia's spread bits */
} MemberCommand;

/* An attack's record, as KERNEL.BIN's attacks and LIMTMENU's limits have it (28 bytes). */
typedef struct AttackData {
    unsigned char pad0[4];
    unsigned short mp_cost;
    unsigned char pad6[2];
    unsigned short camera[2]; /* one target, all */
    unsigned int pad0C[4];
} AttackData;

/* A support effect a paired materia gives (Counter, Added Effect and so on). */
typedef struct MemberSupport {
    unsigned char kind;
    unsigned char value;
    unsigned char param;
} MemberSupport;

/* KERNEL.BIN's equipment records. A stat id in stat[] is an index into PartyMember.primary, 0xFF for none; usable is
 * where the item can be used, sold or thrown, as func_8001FAF8 reads it for any item number. */
typedef struct WeaponData {
    unsigned char pad0[4];
    unsigned char power;
    unsigned char status; /* the status it inflicts, a bit number */
    unsigned char growth; /* its materia's AP: 0 none, 1 normal, 2 double, 3 triple */
    unsigned char pad7;
    unsigned char hit; /* Attack% */
    unsigned char pad9[5];
    unsigned short equip; /* who can equip it, a bit per character */
    unsigned short elements;
    unsigned char pad12[2];
    unsigned char stat[4];
    unsigned char stat_add[4];
    unsigned char slots[8]; /* two by two: linked or not */
    unsigned char pad24[6];
    unsigned short usable;
} WeaponData;
extern WeaponData weapon_table[];

typedef struct ArmorData {
    unsigned char pad0;
    unsigned char element_mode; /* which of PartyMember.elements[1]-[3] its elements go in */
    unsigned char defense;
    unsigned char mdefense; /* Magic def */
    unsigned char defense_pct;  /* Defense%, the member's Dex / 4 added */
    unsigned char mdefense_pct; /* Magic def% */
    unsigned char status; /* the status it guards against, a bit number */
    unsigned char pad7[2];
    unsigned char slots[8];
    unsigned char growth; /* as WeaponData.growth */
    unsigned short equip; /* as WeaponData.equip */
    unsigned short elements;
    unsigned char pad16[2];
    unsigned char stat[4];
    unsigned char stat_add[4];
    unsigned short usable;
    unsigned char pad22[2];
} ArmorData;
extern ArmorData armor_table[];
ArmorData *armor_data(int id);

typedef struct AccessoryData {
    unsigned char stat[2];
    unsigned char stat_add[2];
    unsigned char element_mode;
    unsigned char special;
    unsigned short elements;
    unsigned int statuses;
    unsigned short equip; /* as WeaponData.equip */
    unsigned short usable;
} AccessoryData;
extern AccessoryData accessory_table[];

typedef struct ItemData {
    unsigned char pad0[10];
    unsigned short usable;
    unsigned char pad0C[16];
} ItemData;
extern ItemData item_table[];

/* A party member as worked out from the character, equipment and materia (0x8009D84C, three of them). */
typedef struct PartyMember {
    unsigned char misc[2];    /* member_bonus[10] and [11] */
    unsigned char primary[6]; /* Str, Vit, Mag, Spr, Dex, Luck */
    short stats[4];
    short hp;
    short max_hp;
    short mp;
    short max_mp;
    unsigned short atb;       /* the battle's ATB at the start */
    unsigned short limit_bar; /* the battle's, << 8 */
    unsigned short pad1C[2];
    unsigned char limit_level;
    unsigned char command_rows; /* the battle menu's rows of commands */
    unsigned char pad22;
    unsigned char flags23; /* 8: HP and MP swapped */
    MemberSupport support[8];
    unsigned short elements[4]; /* bits of D_800491B0: [0] the weapon's, [1]-[3] the armor's by the support's level */
    unsigned int statuses[2];   /* the weapon's, the armor's */
    MemberCommand commands[16];
    unsigned char limits[8]; /* the limit level's three, ids less 0x80 (0xFF for none) */
    AttackData limit_records[3];
    MemberSpell spells[96]; /* 56 magic, 16 summons, 24 enemy skills */
    WeaponData weapon;
    unsigned char pad434[3];
    unsigned char encounter_less; /* taken from effects[1]'s sum */
    unsigned char pad438[4];
    unsigned char effects[4]; /* support materia the field reads, summed over the party; [1] the encounter rate */
} PartyMember;
extern PartyMember *cur_member;      /* the member being worked on */
extern unsigned char materia_detail; /* nonzero: materia_level also writes the level, levels and AP to next */
PartyMember *party_member(int slot);
int materia_level(unsigned char id, int ap);

/* What the battle's results screen shows of each member. */
typedef struct {
    unsigned int to_next;
    unsigned int exp;
    unsigned char bar; /* 0-61 */
    unsigned char level;
    unsigned short new_limits; /* limits learned, a bit each by limit_id's slot */
} TallyShown;
extern TallyShown tally_shown[];

/* The items won, four at most. */
typedef struct {
    short id; /* the inventory's numbering, -1 for none */
    short count;
    unsigned short won; /* taken across on the items screen */
} Spoil;
extern Spoil spoils[];

/* A materia's record in KERNEL.BIN (0x800730D0, 20 bytes). */
typedef struct MateriaData {
    unsigned short ap[4]; /* AP for each level up, in hundreds; 0xFFFF past the last */
    unsigned char stats;  /* the stat changes it makes */
    unsigned char status[3];
    unsigned char element;
    unsigned char type;     /* low 4 bits: what kind of materia (5 support), high 4 bits: a sub-kind */
    unsigned char attrs[6]; /* what it does, by kind */
} MateriaData;
extern MateriaData materia_data[];

/* The member's battle commands as they're being made. */
typedef struct {
    unsigned char id;
    unsigned char level;
    unsigned char flags;
} CommandSlot;
extern CommandSlot command_slots[]; /* 16 */

/* The member's spells as they're being made. */
typedef struct {
    unsigned char id;
    unsigned char all;
    unsigned char quadra;
    unsigned char quadra_uses;
    unsigned char flags; /* as MemberSpell.flags */
} SpellSlot;
extern SpellSlot spell_slots[]; /* 56 */
extern unsigned char summon_uses[];        /* 16, 0xFF unlimited */
extern unsigned char summon_quadra[];      /* 16 */
extern unsigned char summon_quadra_uses[]; /* 16 */

/* What the materia menu's detail page shows of one materia (materia_info). */
typedef struct MateriaInfo {
    unsigned char level;
    unsigned char levels;
    unsigned char pad2[2];
    int ap_to_next;
    unsigned char pad8[2];
    short stats[8]; /* as the stat changes table */
    struct {
        unsigned char id;
        unsigned char value;
    } lines[5];
} MateriaInfo;
extern MateriaInfo *materia_info;
extern unsigned char materia_info_kind; /* which layout the page uses */

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
