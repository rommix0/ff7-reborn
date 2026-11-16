/* ITEMMENU.MNU, 0x801D01E8-0x801D3260 (jump tables before it, data after it to 0x801D4020): the Item command's module.
 * Use lists the inventory and uses an item on a member or, for a Megalixir or Tent, the party; Arrange sorts it (seven
 * orders, a quicksort with its own stack) or swaps two entries by hand (Customize); Key Items lists the key items. A
 * limit manual teaches its owner the fourth level's limit once the others are learned, and the owner (or anyone else)
 * answers in a line of its own. The rest are field calls the exe makes through the field's menu requests: take the
 * party's best 48 materia away and give them back (types 15 and 16), strip a leaving character (17), and put one
 * character's equipment aside and back (18 and 19). GCC 2.7.2, ASPSX 2.21, -G0. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"
#include "menu.h"

extern PartyMember members[3];
extern unsigned char D_8009D2C8[]; /* the key items, a bit each */
extern unsigned char D_8009D5E8;   /* bit 1: the Save Crystal can be used */
extern unsigned short menu_locked;
extern DRAWENV results_draw[2];

void func_80023AC4(void);
unsigned short func_80025288(); /* takes an unsigned short, but the game passes its item numbers unextended */
unsigned short func_80025310(); /* the same */
void func_80026B5C(int layer); /* empty */

extern unsigned short item_name_order[]; /* by item id: its place by name */
extern unsigned char item_materia_rank[]; /* by materia id: which are taken first */
extern unsigned char item_tim[];
extern int item_state;
extern unsigned char item_keys[64];
extern MenuCursor item_cursors[6];
extern unsigned char item_comment[80];
extern int item_kept[48];
/* What the owner says when given a limit manual: learned, not ready, and anyone else's answer, by manual; then Cait
 * Sith's, who has none. */
extern unsigned char item_comments[25][34];
extern unsigned char item_menu_text[3][12];
extern unsigned char item_arrange_text[8][12];
extern RECT item_windows[3];
extern RECT item_arrange_window;
extern unsigned char item_manual_owners[8]; /* by manual: its character */
extern int item_swap_phase; /* Customize: 1 once the first item is picked */
extern int item_swap_col;
extern int item_swap_first;
extern ScrollBar item_bar;
extern int item_wait; /* frames before the target can be picked; nothing here sets it */

void item_sound(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

/* The item's icon: items, then the weapons by whose they are, armor and accessories. The frame passes a fourth
 * argument (0) that it never reads. */
void item_icon(int x, int y, int item, int unused)
{
    unsigned int kind;
    int u, v;

    if (item < 0x80)
        kind = 0;
    else if (item < 0x100) {
        item -= 0x80;
        if (item < 0x10)
            kind = 1;
        else if (item < 0x20)
            kind = 3;
        else if (item < 0x30)
            kind = 2;
        else if (item < 0x3E)
            kind = 5;
        else if (item < 0x49)
            kind = 4;
        else if (item < 0x57)
            kind = 9;
        else if (item < 0x65)
            kind = 6;
        else if (item < 0x72)
            kind = 7;
        else
            kind = 8;
    } else if (item < 0x120)
        kind = 10;
    else
        kind = 11;
    u = (kind & 1) * 16 | 0x60;
    v = (kind >> 1) * 16 + 0x70;
    func_80028CA0(x, y, u, v, 16, 16, 1, 0);
}

void item_list_keys(void)
{
    int i, n;
    unsigned char *p;

    for (i = 0, n = 0, p = item_keys; i < 64; i++) {
        if (D_8009D2C8[i / 8] >> (i & 7) & 1) {
            *p++ = i;
            n++;
        }
    }
    for (; n < 64; n++)
        item_keys[n] = 0xFF;
}

void item_swap_words(unsigned int *a, unsigned int *b)
{
    unsigned int t = *a;

    *a = *b;
    *b = t;
}

/* Sorts n entries of base with a quicksort that keeps its own stack (64 ranges, else it gives up and returns 0):
 * cmp and swap take two indexes and &base. */
int item_sort(void *base, unsigned int n, int (*cmp)(), void (*swap)())
{
    unsigned int los[64];
    unsigned int his[64];
    unsigned int hi, lo, i;
    unsigned int depth;

    if (n < 2)
        return 1;
    depth = 0;
    los[0] = 0;
    his[0] = n - 1;
    while (depth != -1) {
        lo = los[depth];
        i = lo + 1;
        n = hi = his[depth];
        while (i < hi) {
            while (i < hi && cmp(i, lo, &base) <= 0)
                i++;
            while (i <= hi && cmp(lo, hi, &base) <= 0)
                hi--;
            if (i < hi)
                swap(i++, hi--, &base);
        }
        if (cmp(lo, hi, &base) > 0)
            swap(lo, hi, &base);
        if (lo < hi && lo < --hi) {
            if (i < n && hi - lo < n - i) {
                item_swap_words(&hi, &n);
                item_swap_words(&lo, &i);
            }
            if (hi > lo) {
                los[depth] = lo;
                his[depth] = hi;
                depth++;
            }
        }
        if (n > i) {
            los[depth] = i;
            his[depth] = n;
            depth++;
        }
        if (depth >= 64)
            return 0;
        depth--;
    }
    return 1;
}

void item_swap_halves(unsigned short *a, unsigned short *b)
{
    unsigned short t = *a;

    *a = *b;
    *b = t;
}

int item_sign(int x)
{
    if (x)
        return x < 0 ? -1 : 1;
    return 0;
}

/* Arrange's orders, as item_sort's cmp over the inventory: an empty entry goes last. */
int item_by_type(a, b, items)
short a, b;
unsigned short **items;
{
    return item_sign(((*items)[a] & 0x1FF) - ((*items)[b] & 0x1FF));
}

int item_by_most(a, b, items)
short a, b;
unsigned short **items;
{
    unsigned short x;
    short na, nb;

    if ((x = (*items)[a]) == 0xFFFF)
        na = 0;
    else
        na = x >> 9;
    if ((x = (*items)[b]) == 0xFFFF)
        nb = 0;
    else
        nb = x >> 9;
    return item_sign(nb - na);
}

int item_by_least(a, b, items)
short a, b;
unsigned short **items;
{
    unsigned short x;
    int na, nb;

    if ((x = (*items)[a]) == 0xFFFF)
        na = 20000;
    else
        na = x >> 9;
    if ((x = (*items)[b]) == 0xFFFF)
        nb = 20000;
    else
        nb = x >> 9;
    return item_sign(na - nb);
}

int item_by_name(a, b, items)
short a, b;
unsigned short **items;
{
    unsigned short x;
    short na, nb;

    if ((x = (*items)[a]) == 0xFFFF)
        na = 20000;
    else
        na = item_name_order[x & 0x1FF];
    if ((x = (*items)[b]) == 0xFFFF)
        nb = 20000;
    else
        nb = item_name_order[x & 0x1FF];
    return item_sign(na - nb);
}

int item_by_field(a, b, items)
short a, b;
unsigned short **items;
{
    unsigned short x;
    int na, nb;

    if ((x = (*items)[a]) == 0xFFFF)
        na = 0;
    else
        na = func_8001FAF8(x & 0x1FF) & 4 ? 1 : 2;
    if ((x = (*items)[b]) == 0xFFFF)
        nb = 0;
    else
        nb = func_8001FAF8(x & 0x1FF) & 4 ? 1 : 2;
    return item_sign(nb - na);
}

int item_by_battle(a, b, items)
short a, b;
unsigned short **items;
{
    unsigned short x;
    int na, nb;

    if ((x = (*items)[a]) == 0xFFFF)
        na = 0;
    else
        na = func_8001FAF8(x & 0x1FF) & 2 ? 1 : 2;
    if ((x = (*items)[b]) == 0xFFFF)
        nb = 0;
    else
        nb = func_8001FAF8(x & 0x1FF) & 2 ? 1 : 2;
    return item_sign(nb - na);
}

int item_by_throw(a, b, items)
short a, b;
unsigned short **items;
{
    unsigned short x;
    int na, nb;

    if ((x = (*items)[a]) == 0xFFFF)
        na = 0;
    else
        na = func_8001FAF8(x & 0x1FF) & 8 ? 1 : 2;
    if ((x = (*items)[b]) == 0xFFFF)
        nb = 0;
    else
        nb = func_8001FAF8(x & 0x1FF) & 8 ? 1 : 2;
    return item_sign(nb - na);
}

void item_swap_items(a, b, items)
short a, b;
unsigned short **items;
{
    item_swap_halves(&(*items)[a], &(*items)[b]);
}

void item_arrange(int order)
{
    switch (order) {
    case 0:
        break;
    case 1:
        item_sort(save_data.items, 320, item_by_field, item_swap_items);
        break;
    case 2:
        item_sort(save_data.items, 320, item_by_battle, item_swap_items);
        break;
    case 3:
        item_sort(save_data.items, 320, item_by_throw, item_swap_items);
        break;
    case 4:
        item_sort(save_data.items, 320, item_by_type, item_swap_items);
        break;
    case 5:
        item_sort(save_data.items, 320, item_by_name, item_swap_items);
        break;
    case 6:
        item_sort(save_data.items, 320, item_by_most, item_swap_items);
        break;
    case 7:
        item_sort(save_data.items, 320, item_by_least, item_swap_items);
        break;
    }
}

void item_init(void)
{
    item_state = 1;
    func_80026448(&item_cursors[0], 0, 0, 3, 1, 0, 0, 3, 1, 0, 0, 1, 0, 0);
    func_80026448(&item_cursors[1], 0, 0, 1, 10, 0, 0, 1, 320, 0, 0, 0, 0, 0);
    func_80026448(&item_cursors[2], 0, 0, 1, 3, 0, 0, 1, 3, 0, 0, 0, 1, 0);
    item_list_keys();
}

int item_hp_full(int slot)
{
    return members[slot].max_hp == members[slot].hp;
}

int item_mp_full(int slot)
{
    return members[slot].max_mp == members[slot].mp;
}

/* Whether the character has learned every limit there is but the tenth (a limit manual's test). */
int item_limits_learned(int c)
{
    int i;
    int learned;

    for (i = 0, learned = 0; i < 10; i++) {
        if (limit_id(c, i) != 0x7F)
            learned |= 1 << i;
    }
    learned &= ~0x200;
    return save_data.chars[c].limits == learned;
}

/* func_8001FAF8's flags, with a Tent (0x46) unusable while the menu is locked from saving and the Save Crystal
 * (0x62) until the field allows it. */
int item_usable(int item)
{
    int usable = func_8001FAF8(item);

    switch (item) {
    case 0x62:
        if (!(D_8009D5E8 & 2))
            usable |= 4;
        break;
    case 0x46:
        if (menu_locked & 0x200)
            usable |= 4;
        break;
    }
    return usable;
}

void item_set_comment(unsigned char *text)
{
    int i;

    for (i = 0; i < 80; i++)
        item_comment[i] = *text++;
}

/* item_cursors: [0] Use, Arrange, Key Items; [1] the items; [2] the party; [3] the key items; [4] Arrange's orders;
 * [5] the items again, for Customize. item_state picks which one moves. The int it returns is never read. */
#ifdef NON_MATCHING
/* The game's 1,723 instructions, 22 lines of them in another order: the item list's count shift and the call before
 * it, the key list's index, the Earth Harp's two constants and Customize's first pick load in another order or
 * register (searched by hand and with the permuter). Found on the way: the item cases come in the game's order, not
 * the item numbers'; the Megalixir's "nobody to heal" buzz comes after its work, which is the copy GCC keeps for every
 * case's buzz; the hand for a party-wide item is a call of its own; a row's y is `i * 16 + 58 + step * 4` (any other
 * grouping strength-reduces the 58 into the counter); the take-item calls are unprototyped (the game passes the item
 * unextended); the list's entry is a temporary of its own in each loop; and the key list's index is set in two
 * steps. */
int item_frame(int frame)
{
    RECT box;
    RECT tw;
    int i, j;
    int n;
    int item;
    int list;
    int rows;
    int color;
    int who;
    int id;
    int y;
    unsigned short x;
    unsigned short kept;

    func_800230C4(results_buf);
    if (item_state == 2) {
        if (!item_wait) {
            /* Megalixir and Tent work on the whole party: the hand goes round it */
            id = save_data.items[item_cursors[1].row + item_cursors[1].top] & 0x1FF;
            if (id == 6 || id == 0x46)
                func_8001EB2C(0, frame % 3 * 56 + 75);
            else
                func_8001EB2C(0, item_cursors[2].row * 56 + 75);
        }
        if (frame & 2)
            func_8001EB2C(169, item_cursors[1].row * 16 + 60);
        if (item_wait)
            item_wait--;
    }
    func_80026B5C(0x80);
    switch (item_state) {
    case 0:
        func_8001EB2C(item_cursors[0].col * 56 + 8, 12);
        break;
    case 1:
        if (frame & 2)
            func_8001EB2C(item_cursors[0].col * 56 + 8, 12);
        func_8001EB2C(169, item_cursors[1].row * 16 + 60);
        item = item_cursors[1].row + item_cursors[1].top;
        if (save_data.items[item] != 0xFFFF)
            func_80026F44(16, 35, kind_text(4, save_data.items[item] & 0x1FF, 0), 7);
        break;
    case 2:
        if (frame & 2)
            func_8001EB2C(item_cursors[0].col * 56 + 8, 12);
        item = item_cursors[1].row + item_cursors[1].top;
        if (save_data.items[item] != 0xFFFF)
            func_80026F44(16, 35, kind_text(4, save_data.items[item] & 0x1FF, 0), 7);
        break;
    case 3:
        if (frame & 2)
            func_8001EB2C(item_cursors[0].col * 56 + 8, 12);
        func_8001EB2C(item_cursors[3].col * 166 + 3, item_cursors[3].row * 16 + 60);
        item = (item_cursors[3].row + item_cursors[3].top) * 2 + item_cursors[3].col;
        if (item_keys[item] != 0xFF)
            func_80026F44(16, 35, kind_text(14, item_keys[item], 0), 7);
        break;
    case 4:
        if (frame & 2)
            func_8001EB2C(item_cursors[0].col * 56 + 8, 12);
        func_8001EB2C(item_arrange_window.x - 18, item_arrange_window.y + 8 + item_cursors[4].row * 12);
        for (j = 0; j < 8; j++)
            func_80026F44(item_arrange_window.x + 8, item_arrange_window.y + 6 + j * 12, item_arrange_text[j], 7);
        setRECT(&tw, 0, 0, 256, 256);
        func_80026A34(0, 1, 0x7F, &tw);
        func_8001E040(&item_arrange_window);
        break;
    case 5:
        if (frame & 2)
            func_8001EB2C(item_cursors[0].col * 56 + 8, 12);
        item = item_cursors[5].row + item_cursors[5].top;
        if (save_data.items[item] != 0xFFFF)
            func_80026F44(16, 35, kind_text(4, save_data.items[item] & 0x1FF, 0), 7);
        break;
    }
    func_80026B5C(8);
    setRECT(&tw, 0, 0, 256, 256);
    func_80026A34(0, 1, 0x7F, &tw);
    if (item_cursors[0].col != 2) {
        /* the party: name, level, HP and MP, and portrait */
        for (j = 0; j < 3; j++) {
            if (save_data.party[j] == 0xFF)
                continue;
            func_80020B68(80, j * 56 + 59, j);
            func_8001CF3C(22, j * 56 + 54, 48, 48, 0, j * 48 + 56, 48, 48, j + 13, 0);
            setRECT(&tw, 0, 0, 256, 256);
            func_80026A34(0, 1, 0x7F, &tw);
        }
        func_8001DE0C(&box, 0, 50, 170, 171);
        func_8001E040(&box);
    }
    for (i = 0; i < 3; i++)
        func_80026F44(i * 56 + 34, 13, item_menu_text[i], 7);
    setRECT(&tw, 0, 0, 364, 224);
    func_80026A94(&results_draw[results_buf], &tw);
    if (item_cursors[0].col != 2) {
        if (item_state == 5) {
            /* the first of the two being swapped */
            if (item_swap_phase && (frame & 2)) {
                y = (item_swap_first - item_cursors[5].top) * 16 + item_cursors[5].scroll_step * 4;
                if (y >= -11 && y < 260)
                    func_8001EB2C(165, y + 56);
            }
            func_8001EB2C(169, item_cursors[5].row * 16 + 60);
            list = 5;
        } else
            list = 1;
        item_bar.shown = 10;
        item_bar.total = 320;
        item_bar.top = item_cursors[list].top;
        item_bar.x = 352;
        item_bar.y = 53;
        item_bar.w = 10;
        item_bar.h = 165;
        func_80028484(&item_bar);
        rows = 10;
        if (item_cursors[list].scrolling)
            rows = 11;
        func_80026B5C(9);
        for (i = 0; i < rows; i++) {
            n = item_cursors[list].top + i;
            if (save_data.items[n] == 0xFFFF)
                continue;
            item = save_data.items[n] & 0x1FF;
            color = item_usable(item) & 4 ? 0 : 7;
            func_80026F44(214, i * 16 + 58 + item_cursors[list].scroll_step * 4, kind_text(4, item, 8), color);
        }
        for (i = 0; i < rows; i++) {
            n = item_cursors[list].top + i;
            x = save_data.items[n];
            n = x;
            if (n == 0xFFFF)
                continue;
            item = x & 0x1FF;
            n = x >> 9;
            color = item_usable(item) & 4 ? 0 : 7;
            item_icon(196, i * 16 + 56 + item_cursors[list].scroll_step * 4, item, 0);
            func_8002708C(319, i * 16 + 60 + item_cursors[list].scroll_step * 4, 0xD5, color);
            func_80028E00(320, i * 16 + 59 + item_cursors[list].scroll_step * 4, n, 3, color);
        }
    } else {
        item_bar.shown = 10;
        item_bar.total = 32;
        item_bar.top = item_cursors[3].top;
        item_bar.x = 352;
        item_bar.y = 53;
        item_bar.w = 10;
        item_bar.h = 165;
        func_80028484(&item_bar);
        func_80026B5C(9);
        for (i = 0; i < 12; i++) {
            n = item_cursors[3].top + i;
            n *= 2;
            for (j = 0; j < 2; j++) {
                item = n + j;
                if (item_keys[item] != 0xFF)
                    func_80026F44(j * 166 + 32, i * 16 + 58 + item_cursors[3].scroll_step * 4,
                                  kind_text(14, item_keys[item], 8), 7);
            }
        }
    }
    setRECT(&tw, 0, 53, 364, 165);
    func_80026A94(&results_draw[results_buf], &tw);
    for (j = 0; j < 3; j++)
        func_8001E040(&item_windows[j]);
    if (func_80023050())
        return;
    func_800264A8(&item_cursors[item_state]);
    switch (item_state) {
    case 0:
        if (D_80062D7C & 0x20) {
            item_sound(1);
            switch (item_cursors[0].col) {
            case 0:
                item_state = 1;
                break;
            case 1:
                func_80026448(&item_cursors[4], 0, 0, 1, 8, 0, 0, 1, 8, 0, 0, 0, 1, 0);
                item_state = 4;
                break;
            case 2:
                func_80026448(&item_cursors[3], 0, 0, 2, 10, 0, 0, 2, 32, 0, 0, 2, 0, 0);
                item_state = 3;
                break;
            }
        } else if (D_80062D7E & 0x40) {
            item_sound(4);
            func_8002305C(5, 0);
            func_8002120C(0);
        }
        break;
    case 1:
        if (item_cursors[1].scrolling)
            break;
        if (D_80062D7C & 0x20) {
            item = item_cursors[1].row + item_cursors[1].top;
            if (save_data.items[item] == 0xFFFF) {
                item_sound(3);
                break;
            }
            item = save_data.items[item] & 0x1FF;
            if (item_usable(item) & 4) {
                item_sound(3);
                break;
            }
            switch (item) {
            case 0x67: /* Earth Harp: records 6 and 7 become Cait Sith's and Vincent's at level 1. The kernel never lets
                        * it be used here, so this doesn't run. */
                item_sound(0x107);
                save_data.chars[6].id = 6;
                save_data.chars[6].level = 1;
                save_data.chars[6].limit_level = 1;
                save_data.chars[6].limit_bar = 0xFF;
                save_data.chars[6].exp = 0xFFFFFF;
                save_data.chars[7].level = 1;
                save_data.chars[7].id = 7;
                save_data.chars[7].limit_level = 1;
                save_data.chars[7].limit_bar = 0xFF;
                save_data.chars[7].exp = 0xFFFFFF;
                break;
            case 0x62: /* Save Crystal */
                item_sound(0x107);
                D_8009D5E8 |= 1;
                func_8002305C(5, 0);
                func_8002120C(0);
                func_80023AC4();
                break;
            default:
                item_sound(1);
                item_wait = 0;
                item_state = 2;
                break;
            }
        } else if (D_80062D7C & 0x40) {
            item_sound(4);
            item_state = 0;
        }
        break;
    case 2:
        if (item_wait)
            break;
        if (D_80062D7C & 0x20) {
            item = save_data.items[item_cursors[1].row + item_cursors[1].top] & 0x1FF;
            who = save_data.party[item_cursors[2].row];
            if (who == 0xFF && item != 6 && item != 0x46) {
                item_sound(3);
                break;
            }
            switch (item) {
            case 0xD: /* Hyper: Sadness goes, else Fury comes */
                if (save_data.chars[who].flags & 0x20) {
                    item_sound(3);
                    break;
                }
                if (save_data.chars[who].flags & 0x10)
                    save_data.chars[who].flags &= ~0x10;
                else
                    save_data.chars[who].flags |= 0x20;
                item_sound(0x107);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 0xE: /* Tranquilizer: Fury goes, else Sadness comes */
                if (save_data.chars[who].flags & 0x20)
                    save_data.chars[who].flags &= ~0x20;
                else if (save_data.chars[who].flags & 0x10) {
                    item_sound(3);
                    break;
                } else
                    save_data.chars[who].flags |= 0x10;
                item_sound(0x107);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 0x57: /* the limit manuals: the fourth level's limit, for their owner once the rest are learned */
            case 0x58:
            case 0x59:
            case 0x5A:
            case 0x5B:
            case 0x5C:
            case 0x5D:
            case 0x5E:
                if (who == item_manual_owners[item - 0x57]) {
                    if (item_limits_learned(who)) {
                        item_sound(0x180);
                        save_data.chars[item_manual_owners[item - 0x57]].limits |= 0x200;
                        func_80025288(item | 1 << 9);
                        if (func_80025310(item) == 0xFFFF)
                            item_state = 1;
                        item_set_comment(item_comments[(item - 0x57) * 3]);
                        func_8001F6C0(item_comment, 7);
                        break;
                    }
                    item_set_comment(item_comments[(item - 0x57) * 3 + 1]);
                    func_8001F6C0(item_comment, 7);
                    item_sound(3);
                } else if (who == 6) { /* Cait Sith has no manual */
                    item_set_comment(item_comments[24]);
                    func_8001F6C0(item_comment, 7);
                    item_sound(3);
                } else {
                    item_set_comment(item_comments[(who < 6 ? who : who - 1) * 3 + 2]);
                    func_8001F6C0(item_comment, 7);
                    item_sound(3);
                }
                break;
            case 0x47: /* the sources: a stat's bonus up one, to 255 */
            case 0x48:
            case 0x49:
            case 0x4A:
            case 0x4B:
            case 0x4C:
                switch (item) {
                case 0x47:
                    if (save_data.chars[who].stat_bonus[0] >= 255) {
                        item_sound(3);
                        return;
                    }
                    save_data.chars[who].stat_bonus[0]++;
                    break;
                case 0x48:
                    if (save_data.chars[who].stat_bonus[1] >= 255) {
                        item_sound(3);
                        return;
                    }
                    save_data.chars[who].stat_bonus[1]++;
                    break;
                case 0x49:
                    if (save_data.chars[who].stat_bonus[2] >= 255) {
                        item_sound(3);
                        return;
                    }
                    save_data.chars[who].stat_bonus[2]++;
                    break;
                case 0x4A:
                    if (save_data.chars[who].stat_bonus[3] >= 255) {
                        item_sound(3);
                        return;
                    }
                    save_data.chars[who].stat_bonus[3]++;
                    break;
                case 0x4B:
                    if (save_data.chars[who].stat_bonus[4] >= 255) {
                        item_sound(3);
                        return;
                    }
                    save_data.chars[who].stat_bonus[4]++;
                    break;
                case 0x4C:
                    if (save_data.chars[who].stat_bonus[5] >= 255) {
                        item_sound(3);
                        return;
                    }
                    save_data.chars[who].stat_bonus[5]++;
                    break;
                }
                item_sound(0x107);
                func_80020058(item_cursors[2].row);
                member_update(item_cursors[2].row);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 0: /* Potion */
                if (item_hp_full(item_cursors[2].row) || !members[item_cursors[2].row].hp) {
                    item_sound(3);
                    break;
                }
                item_sound(0x107);
                func_800258BC(item_cursors[2].row, 100);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 1: /* Hi-Potion */
                if (item_hp_full(item_cursors[2].row) || !members[item_cursors[2].row].hp) {
                    item_sound(3);
                    break;
                }
                item_sound(0x107);
                func_800258BC(item_cursors[2].row, 500);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 3: /* Ether */
                if (item_mp_full(item_cursors[2].row) || !members[item_cursors[2].row].hp) {
                    item_sound(3);
                    break;
                }
                item_sound(0x107);
                func_80025A44(item_cursors[2].row, 100);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 4: /* Turbo Ether */
                if (item_mp_full(item_cursors[2].row) || !members[item_cursors[2].row].hp) {
                    item_sound(3);
                    break;
                }
                item_sound(0x107);
                func_80025A44(item_cursors[2].row, 10000);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 7: /* Phoenix Down */
                if (members[item_cursors[2].row].hp) {
                    item_sound(3);
                    break;
                }
                item_sound(0x107);
                func_800258BC(item_cursors[2].row, members[item_cursors[2].row].max_hp / 4);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 0x46: /* Tent */
                for (j = 0, n = 0; j < 3; j++) {
                    if (save_data.party[j] != 0xFF && (!item_hp_full(j) || !item_mp_full(j)))
                        n = 1;
                }
                if (!n) {
                    item_sound(3);
                    break;
                }
                for (j = 0; j < 3; j++) {
                    if (members[j].hp && save_data.party[j] != 0xFF) {
                        func_800258BC(j, 10000);
                        func_80025A44(j, 10000);
                    }
                }
                item_sound(0x107);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 2: /* X-Potion */
                if (item_hp_full(item_cursors[2].row) || !members[item_cursors[2].row].hp) {
                    item_sound(3);
                    break;
                }
                item_sound(0x107);
                func_800258BC(item_cursors[2].row, 10000);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 5: /* Elixir */
                if (item_hp_full(item_cursors[2].row) && item_mp_full(item_cursors[2].row)
                    || !members[item_cursors[2].row].hp) {
                    item_sound(3);
                    break;
                }
                item_sound(0x107);
                func_800258BC(item_cursors[2].row, 10000);
                func_80025A44(item_cursors[2].row, 10000);
                func_80025288(item | 1 << 9);
                if (func_80025310(item) == 0xFFFF)
                    item_state = 1;
                break;
            case 6: /* Megalixir */
                for (j = 0, n = 0; j < 3; j++) {
                    if (save_data.party[j] != 0xFF && (!item_hp_full(j) || !item_mp_full(j)))
                        n = 1;
                }
                if (n) {
                    for (j = 0; j < 3; j++) {
                        if (members[j].hp && save_data.party[j] != 0xFF) {
                            func_800258BC(j, 10000);
                            func_80025A44(j, 10000);
                        }
                    }
                    item_sound(0x107);
                    func_80025288(item | 1 << 9);
                    if (func_80025310(item) == 0xFFFF)
                        item_state = 1;
                } else
                    item_sound(3);
                break;
            }
        } else if (D_80062D7C & 0x40) {
            item_sound(4);
            item_state = 1;
        }
        break;
    case 3:
        if (D_80062D7C & 0x40) {
            item_sound(4);
            item_state = 0;
        }
        break;
    case 4:
        if (D_80062D7C & 0x20) {
            item_sound(1);
            if (!item_cursors[4].row) { /* Customize */
                func_80026448(&item_cursors[5], 0, 0, 1, 10, 0, 0, 1, 320, 0, 0, 0, 0, 0);
                item_swap_phase = 0;
                item_swap_col = 0;
                item_swap_first = 0;
                item_state = 5;
            } else {
                item_arrange(item_cursors[4].row);
                item_state = 0;
            }
        } else if (D_80062D7C & 0x40) {
            item_sound(4);
            item_state = 0;
        }
        break;
    case 5:
        if (D_80062D7C & 0x20) {
            switch (item_swap_phase) {
            case 0:
                item_sound(1);
                item_swap_col = item_cursors[5].col;
                item_swap_phase++;
                item_swap_first = item_cursors[5].top + item_cursors[5].row;
                break;
            case 1:
                item_sound(1);
                kept = save_data.items[item_swap_first];
                save_data.items[item_swap_first] = save_data.items[item_cursors[5].row + item_cursors[5].top];
                item_swap_phase = 0;
                save_data.items[item_cursors[5].row + item_cursors[5].top] = kept;
                break;
            }
        } else if (D_80062D7C & 0x40) {
            item_sound(4);
            item_state = 0;
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/menu/itemmenu/nonmatchings/itemmenu", item_frame);
#endif

void item_801D296C(void)
{
}

/* The materia field events take: the 48 the party has with the highest item_materia_rank. */
void item_replace_rank(int m, int rank)
{
    int i;

    for (i = 0; i < 48; i++) {
        if (item_materia_rank[(unsigned char)item_kept[i]] == rank) {
            item_kept[i] = m;
            return;
        }
    }
}

int item_lowest_rank(void)
{
    int i;
    int low = 0xFF;
    int rank;

    for (i = 0; i < 48; i++) {
        rank = item_materia_rank[(unsigned char)item_kept[i]];
        if (rank < low)
            low = rank;
    }
    return low;
}

void item_keep(int *m)
{
    int i;
    int low;

    if (*m == -1)
        return;
    for (i = 0; i < 48; i++) {
        if (item_kept[i] == -1) {
            item_kept[i] = *m;
            return;
        }
    }
    low = item_lowest_rank();
    if (item_materia_rank[*m & 0xFF] >= low)
        item_replace_rank(*m, low);
}

/* Puts a materia in a free slot of a joined character's weapon or armor, the last character first; 1 if none has
 * one. */
#ifdef NON_MATCHING
/* Same code but for two registers swapped: the slot counter and the pointer to the materia (searched by hand and with
 * the permuter). */
int item_give_back(int m)
{
    int c, j;

    for (c = 8; c >= 0; c--) {
        if (!(save_data.joined >> c & 1))
            continue;
        for (j = 0; j < 8; j++) {
            if (save_data.chars[c].weapon_materia[j] == -1 && weapon_table[save_data.chars[c].weapon].slots[j]) {
                save_data.chars[c].weapon_materia[j] = m;
                return 0;
            }
        }
        for (j = 0; j < 8; j++) {
            if (save_data.chars[c].armor_materia[j] == -1 && armor_table[save_data.chars[c].armor].slots[j]) {
                save_data.chars[c].armor_materia[j] = m;
                return 0;
            }
        }
    }
    return 1;
}
#else
INCLUDE_ASM("asm/menu/itemmenu/nonmatchings/itemmenu", item_give_back);
#endif

/* Takes a materia away: from a joined character's weapon or armor, or else the stock. */
void item_remove(int m)
{
    int i, j;

    for (i = 0; i < 9; i++) {
        if (!(save_data.joined >> i & 1))
            continue;
        for (j = 0; j < 8; j++) {
            if (save_data.chars[i].weapon_materia[j] == m) {
                save_data.chars[i].weapon_materia[j] = -1;
                return;
            }
        }
        for (j = 0; j < 8; j++) {
            if (save_data.chars[i].armor_materia[j] == m) {
                save_data.chars[i].armor_materia[j] = -1;
                return;
            }
        }
    }
    for (j = 0; j < 200; j++) {
        if (save_data.materia[j] == m) {
            save_data.materia[j] = -1;
            return;
        }
    }
}

void item_take_kept(void)
{
    int i;

    for (i = 0; i < 48; i++) {
        if (item_kept[i] != -1)
            item_remove(item_kept[i]);
    }
    for (i = 0; i < 48; i++)
        save_data.taken.materia[i] = item_kept[i];
}

/* Field menu type 15: takes the party's best 48 materia away. */
#ifdef NON_MATCHING
/* Same code but for two registers swapped: the inner counter and the pointer to the materia (searched by hand and
 * with the permuter; the stock's loop shares the inner counter's register). */
void item_take_materia(void)
{
    int i, j, k;

    for (k = 47; k >= 0; k--)
        item_kept[k] = -1;
    for (i = 0; i < 9; i++) {
        if (!(save_data.joined >> i & 1))
            continue;
        for (j = 0; j < 8; j++)
            item_keep(&save_data.chars[i].weapon_materia[j]);
        for (j = 0; j < 8; j++)
            item_keep(&save_data.chars[i].armor_materia[j]);
    }
    for (i = 0; i < 200; i++)
        item_keep(&save_data.materia[i]);
    item_take_kept();
}
#else
INCLUDE_ASM("asm/menu/itemmenu/nonmatchings/itemmenu", item_take_materia);
#endif

/* Type 16: gives them back, into free slots first, the rest to the stock. */
void item_return_materia(void)
{
    int i;

    for (i = 0; i < 48; i++) {
        if (save_data.taken.materia[i] != -1 && item_give_back(save_data.taken.materia[i]))
            func_8002542C(save_data.taken.materia[i]);
    }
}

/* Type 17: the character's materia to the stock and accessory to the items, as when one leaves the party. */
void item_strip(int c)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (save_data.chars[c].weapon_materia[i] != -1) {
            func_8002542C(save_data.chars[c].weapon_materia[i]);
            save_data.chars[c].weapon_materia[i] = -1;
        }
    }
    for (i = 0; i < 8; i++) {
        if (save_data.chars[c].armor_materia[i] != -1) {
            func_8002542C(save_data.chars[c].armor_materia[i]);
            save_data.chars[c].armor_materia[i] = -1;
        }
    }
    if (save_data.chars[c].accessory != 0xFF) {
        func_80025380(save_data.chars[c].accessory + 0x120 | 1 << 9);
        save_data.chars[c].accessory = 0xFF;
    }
}

/* Type 18: puts the party, the character's equipment and materia and the stock's first three materia aside, leaving
 * the character weapon 0 and no materia. */
void item_stash(int c)
{
    int i;
    struct Stash *stash = &save_data.taken.stash;

    for (i = 0; i < 3; i++)
        stash->party[i] = save_data.party[i];
    stash->weapon = save_data.chars[c].weapon;
    stash->armor = save_data.chars[c].armor;
    for (i = 0; i < 3; i++) {
        stash->materia[i] = save_data.materia[i];
        save_data.materia[i] = -1;
    }
    for (i = 0; i < 8; i++) {
        stash->weapon_materia[i] = save_data.chars[c].weapon_materia[i];
        save_data.chars[c].weapon_materia[i] = -1;
        stash->armor_materia[i] = save_data.chars[c].armor_materia[i];
        save_data.chars[c].armor_materia[i] = -1;
    }
    save_data.chars[c].weapon = 0;
}

/* Type 19: puts them back. */
void item_unstash(int c)
{
    int i;
    struct Stash *stash = &save_data.taken.stash;

    for (i = 0; i < 3; i++)
        save_data.party[i] = stash->party[i];
    save_data.chars[c].weapon = stash->weapon;
    save_data.chars[c].armor = stash->armor;
    for (i = 0; i < 3; i++)
        save_data.materia[i] = stash->materia[i];
    for (i = 0; i < 8; i++) {
        save_data.chars[c].weapon_materia[i] = stash->weapon_materia[i];
        save_data.chars[c].armor_materia[i] = stash->armor_materia[i];
    }
}

/* The icons' image into VRAM; the exe calls it for a new game. */
void item_load_tim(void)
{
    func_80025D14(item_tim, 0x3F0, 0x120, 0x110, 0x1E0);
}

