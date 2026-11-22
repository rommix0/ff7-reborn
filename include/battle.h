/* BATTLE.X's globals that BATINI.X (the battle's setup) and BATRES.X (its results) both read. */

#ifndef BATTLE_H
#define BATTLE_H

#include "main.h"

/* An actor (0x68 bytes): the party's three, then the enemies. */
typedef struct {
    unsigned int status; /* 1 dead, 0x4001 out of the fight, 0x10 Sadness, 0x20 Fury */
    unsigned int flags4; /* 2 and 0x40 the side and row, 8 a party member */
    signed char present; /* -1 for an empty place; a member's character id, an enemy's record */
    unsigned char level;
    unsigned char padA[2];
    unsigned char xC; /* an enemy's number among its kind */
    unsigned char attack, mattack, defense_pct;
    unsigned char x10, x11, x12, x13;
    unsigned char dex, luck, state16;
    unsigned char pad17[9];
    unsigned short defense, mdefense;
    unsigned short id; /* an enemy's ID */
    unsigned char pad26[2];
    unsigned short mp, max_mp;
    unsigned int hp, max_hp;
    unsigned char pad34[0x10];
    unsigned int x44;
    unsigned char pad48[4];
    unsigned char x4C, x4D, row, x4F;
    unsigned short x50, x52;
    unsigned char pad54[2];
    unsigned char x56;
    unsigned char pad57;
    int gil, exp;
    unsigned char pad60[8];
} BattleActor;

/* BATTLE.X's state at 0x800F83A4, its actors from 0x3C. */
typedef struct {
    unsigned char pad0[4];
    unsigned char layout;
    unsigned char pad5[5];
    unsigned short present; /* the places with an actor, a bit each */
    unsigned char padC[0xC];
    unsigned short party; /* the members, a bit each */
    unsigned short enemies;
    unsigned char pad1C[6];
    unsigned short outcome; /* how the battle ended, read by res_finish */
    unsigned char pad24[4];
    unsigned short id; /* the battle */
    unsigned char pad2A[2];
    unsigned short flags; /* 4 a battle in stages, 0x10 Cloud learns all his limits, 0x20 no results screens */
    unsigned char pad2E[2];
    unsigned short stage;
    unsigned char pad32[0xA];
    BattleActor actors[10];
} BattleState;
extern BattleState battle;

/* A place's battle record (0x44 bytes, 0x800F5BB8), then the members' and their extras. */
typedef struct {
    unsigned char pad0[4];
    unsigned short atb;
    unsigned char pad6[2];
    unsigned short x8;
    unsigned char padA[2];
    unsigned char xC, special, padE, xF;
    unsigned char x10[16]; /* [2] 0xFF with the accessory that gives 0x200000 */
    unsigned char x20[8];
    unsigned char pad28;
    unsigned char flags1; /* 2, 8; 0x11 drops nothing */
    unsigned char pad2A[0xA];
    unsigned int x34; /* statuses */
    void *enemy;
    unsigned short hp, mp;
    unsigned char pad40[4];
} BattleSlot;

typedef struct {
    Character *character;
    unsigned char uses; /* of the limit level's limits */
    unsigned char limit_level; /* 0-3, 0xFF none */
    unsigned char x6;
    unsigned char kills;
    unsigned short limit_bar, limit_bar2;
    unsigned short mp, hp;
    unsigned short max_mp, max_hp;
    unsigned short mp_cap, hp_cap;
    unsigned char pad18[4];
    unsigned int limit_need;
    unsigned int statuses; /* from the accessory */
    unsigned int materia, materia2; /* res_finish ORs their difference into kind 7 materia */
    unsigned char pad2C[8];
} BattleMember;

typedef struct {
    unsigned char weapon0, weapon1, weapon2, hit, crit, weapon7;
    unsigned char pad6[2];
    unsigned short x8[3];
    unsigned short elements, x10, x12;
    unsigned int statuses;
} BattleExtra;

extern struct {
    BattleSlot slots[10];
    BattleMember members[3];
    BattleExtra extra[3];
} units;

/* A scene's enemy record as BATTLE.X keeps it (0xB8 bytes). */
typedef struct {
    unsigned char pad0[0x20];
    unsigned char level, dex, luck, defense_pct, attack, defense, mattack, mdefense;
    unsigned char pad28[0x6C];
    unsigned short attacks[3];
    unsigned char pad9A[2];
    unsigned short mp;
    unsigned short ap;
    unsigned char padA0[2];
    unsigned char xA2;
    unsigned char padA3;
    unsigned int hp;
    int exp, gil;
    unsigned int immune;
    unsigned char padB4[4];
} BattleEnemy;

typedef struct {
    unsigned char pad0[0xC];
    unsigned char targets;
    unsigned char padD[15];
} BattleAttack;

/* BATTLE.X's copy of the scene at 0x800F5F44, and what follows it. */
extern struct {
    BattleEnemy enemies[3];
    BattleAttack attacks[32];
    unsigned short attack_ids[32];
    unsigned char names[32][32];
    unsigned char pad9E8[0x278];
    unsigned char setup_ai[0x200];
    unsigned char enemy_ai[0x1000];
    unsigned char pad1E60[0x24];
    unsigned short type; /* the battle type, from the setup's layout */
    unsigned char pad1E86[2];
    unsigned short left; /* enemies that have left the battle, from bit 4 */
    unsigned char pad1E8A[4];
    unsigned short given; /* members whose HP and MP res_store_party is given */
} battle_data;

/* The scene's own records as SCENE.BIN has them (0x8016360C). */
extern struct {
    short enemy_ids[4];
    struct {
        unsigned short area, next, escape;
        unsigned char pad6[2];
        unsigned short battles[4];
        unsigned short flags;
        unsigned char layout, camera;
    } setup;
    short cameras[4][6];
    struct {
        short id; /* -1 for none */
        short x, y, z;
        unsigned short row, cover;
        unsigned int flags;
    } formation[6];
} scene;

/* After the scene: a record per place (0x801636B8). */
extern struct {
    signed char id;
    unsigned char x1;
    unsigned char pad2[4];
    unsigned short x6;
    unsigned int x8, xC;
} places[10];

/* The enemies' HP kept between the stages of a battle. */
extern struct {
    unsigned int dead; /* enemies killed in earlier stages, six bits a stage */
    int stage;
    unsigned int hp[4][6];
} stage_hp;

/* The battle's copy of the inventory's items. */
typedef struct {
    unsigned short id;
    unsigned char count, kind, targets, pad5;
} BattleItem;
extern BattleItem battle_items[320];

#endif
