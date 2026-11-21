/* NAMEMENU.MNU, 0x801D14E8-0x801D1E78: name_catch_up, which levels a character the naming screen has named up to
 * the party, and the level-up arithmetic it runs: the EXP curve, then for each level the stat, HP and MP gains from
 * KERNEL.BIN's curves and tables. Its own object in the module: ASPSX 2.34 folds indexed loads into `%lo`, which the
 * naming screen's 2.21 doesn't. GCC 2.7.2, -G0. */

#include "common.h"
#include "main.h"
#include "levelup.h"

extern unsigned char D_8009D28A;  /* a field variable */
extern unsigned short D_8009D78A; /* the joined characters, bit n for character n */

unsigned char rand_byte(void);
void name_unpack(Growth *w, Character *c);
void name_pack(Growth *w, Character *c);
void name_growth_init(Growth *w);
void name_level_to(Growth *w);
unsigned int name_exp_for(Growth *w, int level);
void name_stat_gain(Growth *w, int stat);
void name_hp_gain(Growth *w);
void name_mp_gain(Growth *w);

/* Levels c to the average of the other joined characters (or a fixed level), as many level-ups as that takes. */
void name_catch_up(Character *c)
{
    Growth w;
    int n, i, j, level;
    unsigned char join;

    level = 0; /* the others' levels summed, then averaged */
    n = 0;
    join = 0;
    if (c->id < 9)
        join = kernel_characters[c->id].join;
    for (i = 0; i < 9; i++) {
        if (!(D_8009D78A >> i & 1))
            continue;
        for (j = 0; j < 9; j++) {
            if (D_8009C738[j].id == i) {
                if (i != c->id) {
                    n++;
                    level += D_8009C738[j].level;
                }
                break;
            }
        }
    }
    if (!n) {
        n = 1;
        level = c->level;
    }
    level /= n;
    if (join & 1)
        level = D_8009D28A;
    else
        level += (signed char)join / 2;
    if (level <= 0)
        level = 1;
    name_unpack(&w, c);
    if (join & 1) {
        w.exp = 0;
        w.level = 1;
        for (i = 7; i >= 0; i--)
            w.base[i] = 0;
    }
    name_growth_init(&w);
    w.gain = name_exp_for(&w, level - 1);
    name_level_to(&w);
    name_pack(&w, c);
}

void name_unpack(Growth *w, Character *c)
{
    w->id = c->id;
    w->level = c->level;
    w->base[0] = c->stats[0];
    w->base[1] = c->stats[1];
    w->base[2] = c->stats[2];
    w->base[3] = c->stats[3];
    w->base[4] = c->stats[4];
    w->base[5] = c->stats[5];
    w->base[6] = c->base_hp;
    w->base[7] = c->base_mp;
    w->exp = c->exp;
}

void name_pack(Growth *w, Character *c)
{
    c->level = w->level;
    c->stats[0] = w->stats[0];
    c->stats[1] = w->stats[1];
    c->stats[2] = w->stats[2];
    c->stats[3] = w->stats[3];
    c->stats[4] = w->stats[4];
    c->stats[5] = w->stats[5];
    c->base_hp = w->stats[6];
    c->base_mp = w->stats[7];
    c->exp = w->total;
    c->exp_to_next = w->next - w->total;
    c->exp_bar = 0;
    if (w->next != w->reached)
        c->exp_bar = (w->total - w->reached) * 61 / (w->next - w->reached);
    if (w->level >= 99) {
        c->exp_to_next = 0;
        c->exp_bar = 61;
    }
}

void name_growth_init(Growth *w)
{
    int i;

    w->kernel = &kernel_characters[w->id];
    for (i = 0; i < 8; i++) {
        if (!w->base[i])
            w->base[i] = 1;
        w->stats[i] = w->base[i];
    }
    for (i = 15; i >= 0; i--)
        w->levels[i] = 0;
}

void name_level_to(Growth *w)
{
    unsigned int exp;
    int n, i, level;

    n = 0;
    w->total = w->exp + w->gain;
    if (w->total > 999999999)
        w->total = 999999999;
    w->reached = w->start = name_exp_for(w, w->level - 1);
    level = w->level;
    while (level < 99) {
        exp = name_exp_for(w, level);
        w->next = exp;
        if (n < 16)
            w->levels[n++] = exp;
        if (w->total < exp)
            break;
        for (i = 0; i < 6; i++)
            name_stat_gain(w, i);
        name_hp_gain(w);
        name_mp_gain(w);
        w->level++;
        w->reached = exp;
        level = w->level;
    }
}

/* The EXP that level + 1 begins at; also sets w's bracket for level. */
unsigned int name_exp_for(Growth *w, int level)
{
    unsigned int exp;
    int i;

    exp = 0;
    w->bracket = 0;
    if (level) {
        if (level < 61)
            w->bracket = (level - 1) / 10;
        else
            w->bracket = (level - 61) / 20 + 6;
    }
    for (i = 1; i <= level; i++)
        exp += kernel_curves[w->kernel->curves[8]][w->bracket].gradient * i * i / 10;
    return exp;
}

/* Which of a gain table's twelve entries: by how far under the curve, with a little chance. */
int name_gain_index(int under)
{
    int i;

    i = under + (rand_byte() & 7) + 1;
    if (i < 0)
        i = 0;
    else if (i >= sizeof kernel_stat_gains)
        i = sizeof kernel_stat_gains - 1;
    return i;
}

int name_stat_curve(Growth *w, int level, int stat)
{
    int curve;

    curve = w->kernel->curves[stat];
    return kernel_curves[curve][w->bracket].gradient * level / 100
        + kernel_curves[curve][w->bracket].base;
}

int name_hp_curve(Growth *w, int level)
{
    int curve;

    curve = w->kernel->curves[6];
    return kernel_curves[curve][w->bracket].gradient * level
        + kernel_curves[curve][w->bracket].base * 40;
}

int name_mp_curve(Growth *w, int level)
{
    int curve;

    curve = w->kernel->curves[7];
    return kernel_curves[curve][w->bracket].gradient * level / 10
        + kernel_curves[curve][w->bracket].base * 2;
}

void name_stat_gain(Growth *w, int stat)
{
    int v;

    v = w->stats[stat]
        + kernel_stat_gains[name_gain_index(name_stat_curve(w, w->level + 1, stat) - w->stats[stat])];
    if (v > 100)
        v = 100;
    w->stats[stat] = v;
}

void name_hp_gain(Growth *w)
{
    int now, next, v;

    now = name_hp_curve(w, w->level);
    next = name_hp_curve(w, w->level + 1);
    v = w->stats[6]
        + (next - now) * kernel_hp_gains[name_gain_index(now * 100 / w->stats[6] - 100)] / 100;
    if (v > 9999)
        v = 9999;
    w->stats[6] = v;
}

void name_mp_gain(Growth *w)
{
    int now, next, v;

    now = name_mp_curve(w, w->level);
    next = name_mp_curve(w, w->level + 1);
    v = w->stats[7]
        + (next - now) * kernel_mp_gains[name_gain_index(now * 100 / w->stats[7] - 100)] / 100;
    if (v > 999)
        v = 999;
    w->stats[7] = v;
}
