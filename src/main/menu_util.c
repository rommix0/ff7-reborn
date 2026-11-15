/* The menus' helpers, 0x80025288-0x80026B64: the inventory's items and materia, a party member's records, HP, MP and
 * gil, the faces and palettes kept in VRAM, a new game's settings, the cursors, and where primitives go. Where it ends
 * is read off the code (the text measuring and drawing after it share their own globals), not proven. GCC 2.7.2,
 * ASPSX 2.21, -G8. */

#include "common.h"
#include "main.h"
#include "prims.h"
#include "sound.h"

/* The save's inventory, from the items on. The game reaches the materia flags from the materia's address, so they're
 * one object here. */
extern struct {
    unsigned short items[320]; /* id in the low 9 bits, count above, 0xFFFF for none */
    int materia[200];          /* id in the low byte, AP above; -1 for none */
    unsigned char pad[0x153];
    unsigned char materia_got; /* 1: one with icon palette 10 was added, 2: materia 0x2C was */
} inventory;

/* The save's settings around the Config menu's. */
extern struct {
    unsigned char battle_speed;
    unsigned char battle_message_speed;
    unsigned short config;
    unsigned char button_map[16];
    unsigned char message_speed;
} settings;

extern unsigned char D_8009CBDC[];  /* the party: three character ids, 0xFF for an empty place */
extern PartyMember members[3];
extern struct {
    unsigned int gil;
    unsigned int seconds; /* the play time */
    unsigned int pad;
    unsigned int fraction;
} save_gil;
extern unsigned char mastered[];
extern unsigned char materia_keep[]; /* by materia id: the lowest is dropped when the materia is full */
extern unsigned char icon_kinds[];   /* by materia type: which of icon_palettes */
extern unsigned char icon_palettes[];
extern DiscFile face_files[9];
extern unsigned char D_800756F8[];
extern struct {
    unsigned short held, prev, pressed, repeat;
} field_pad1;

unsigned char mastered_count;
int D_80062F90;
void *prim_saved;
void *ot_saved;
void *prim_ot;

typedef struct TIM_IMAGE {
    unsigned int mode;
    RECT *crect;
    unsigned int *caddr;
    RECT *prect;
    unsigned int *paddr;
} TIM_IMAGE;

void func_8001C434(void);
void func_8001FA28(unsigned short effect);
void func_80020058(int slot);
void func_800211C4(int module);
void func_80025008(void);
void member_update(unsigned char slot);
void party_effects_sum(void);
int cd_read_wait(unsigned int lba, unsigned int size, void *dest, void (*callback)(void));
void func_801D11A8(void);
void func_80043814(DRAWENV *env, int x, int y, int w, int h); /* SetDefDrawEnv */
void func_80044000(RECT *r, void *p);                         /* LoadImage */
void func_80044064(RECT *r, void *p);                         /* StoreImage */
void func_80044A68(void *p, int dfe, int dtd, int tpage, RECT *tw); /* SetDrawMode */
void func_80044AC0(void *p, DRAWENV *env);                    /* SetDrawEnv */
int MoveImage(RECT *r, int x, int y);
int func_800484A8(void);
int IsIdleGPU(int max);
int VSync(int mode);
void OpenTIM(void *tim);
TIM_IMAGE *ReadTIM(TIM_IMAGE *t);
void func_800254E4(unsigned char materia);
int func_8002603C(unsigned char materia);
void func_80025CD4(void *p);
void func_80025C14(void *p);
void func_80025C54(void *p);
void func_80025D14(void *tim, int x, int y, int clut_x, int clut_y);
void func_80026090(void);
void func_800260DC(void);
void func_80026408(unsigned short effect);

/* Takes up to item's count of its id from the inventory; returns what was taken, 0xFFFF for none. */
unsigned short func_80025288(unsigned short item)
{
    int id = item & 0x1FF;
    int count = item >> 9;
    unsigned short taken = 0xFFFF;
    int i;
    unsigned short have;

    for (i = 0; i < 320; i++) {
        have = inventory.items[i];
        if (have == 0xFFFF || id != (have & 0x1FF))
            continue;
        /* The test goes through taken first: that puts taken in the register the game has it in. */
        if ((taken = count < have >> 9)) {
            taken = item | count << 9; /* item already holds count: the game ors it in again */
            inventory.items[i] = ((have >> 9) - count) << 9 | id;
        } else {
            taken = have;
            inventory.items[i] = 0xFFFF;
        }
        break;
    }
    return taken;
}

/* The inventory's entry for an item, 0xFFFF for none. */
unsigned short func_80025310(unsigned short item)
{
    int id = item & 0x1FF;
    unsigned short none = 0xFFFF;
    int i;
    unsigned short have;

    for (i = 0; i < 320; i++) {
        have = inventory.items[i];
        if (have != 0xFFFF && id == (have & 0x1FF))
            return have;
    }
    return none;
}

void func_80025360(void)
{
    func_8001FA28(0x19F);
}

/* Adds an item: to its entry, 99 at most, else to the first free one. */
void func_80025380(unsigned short item)
{
    int id = item & 0x1FF;
    int count = item >> 9;
    int i;
    unsigned short have;

    for (i = 0; i < 320; i++) {
        have = inventory.items[i];
        if (have != 0xFFFF && id == (have & 0x1FF)) {
            count += have >> 9;
            if (count >= 100)
                count = 99;
            inventory.items[i] = count << 9 | id;
            return;
        }
    }
    for (i = 0; i < 320; i++) {
        if (inventory.items[i] == 0xFFFF) {
            inventory.items[i] = item;
            return;
        }
    }
}

/* Adds a materia to the first free place; returns -1, or the materia when there's none. */
int func_8002542C(int materia)
{
    unsigned char id = materia;
    int i;

    for (i = 0; i < 200; i++) {
        if (inventory.materia[i] == -1) {
            inventory.materia[i] = materia;
            if (func_8002603C(id) == 10)
                inventory.materia_got |= 1;
            if (id == 0x2C)
                inventory.materia_got |= 2;
            return -1;
        }
    }
    return materia;
}

void func_800254D8(void)
{
    mastered_count = 0;
}

void func_800254E4(unsigned char materia)
{
    mastered[mastered_count] = materia;
    mastered_count++;
}

/* As func_8002542C, but also lists it as mastered; when the materia is full it takes the place of the one
 * materia_keep values least. */
void materia_add(int materia)
{
    unsigned char id = materia;
    int i;
    unsigned int least;
    int drop;

    for (i = 0; i < 200; i++) {
        if (inventory.materia[i] == -1) {
            inventory.materia[i] = materia;
            if (func_8002603C(id) == 10)
                inventory.materia_got |= 1;
            if (id == 0x2C)
                inventory.materia_got |= 2;
            func_800254E4(id);
            return;
        }
    }
    drop = -1;
    least = 0xFF;
    for (i = 0; i < 200; i++) {
        if (materia_keep[inventory.materia[i] & 0xFF] < least) {
            least = materia_keep[inventory.materia[i] & 0xFF];
            drop = inventory.materia[i];
        }
    }
    for (i = 0; i < 200; i++) {
        if (inventory.materia[i] == drop) {
            inventory.materia[i] = materia;
            return;
        }
    }
}

void func_80025648(void)
{
}

void func_80025650(void)
{
}

int func_80025658(void)
{
    return D_8009C738[0].level;
}

/* The equipment's materia slots; for an empty place the game hands back 0xFF. */
unsigned char *armor_slots(int slot)
{
    int id = D_8009CBDC[slot];

    if (id != 0xFF)
        return armor_table[D_8009C738[character_record[id]].armor].slots;
    return (unsigned char *)0xFF;
}

unsigned char *func_800256DC(int slot)
{
    int id = D_8009CBDC[slot];

    if (id != 0xFF)
        return weapon_table[D_8009C738[character_record[id]].weapon].slots;
    return (unsigned char *)0xFF;
}

ArmorData *armor_data(int id)
{
    return &armor_table[id];
}

AccessoryData *func_80025774(int id)
{
    return &accessory_table[id];
}

PartyMember *party_member(int slot)
{
    if (D_8009CBDC[slot] != 0xFF)
        return &members[slot];
    return (PartyMember *)0xFF;
}

void func_800257C4(void)
{
}

unsigned char *func_800257CC(int id)
{
    return D_8009C738[character_record[id]].name;
}

/* A member's HP down by n, to 0 at least, in the member and the character. */
void func_80025800(int slot, int n)
{
    int id = D_8009CBDC[slot];
    int rec;
    int hp;

    if (id == 0xFF)
        return;
    rec = character_record[id];
    if ((hp = members[slot].hp - n) < 0)
        members[slot].hp = 0;
    else
        members[slot].hp = hp;
    D_8009C738[rec].hp = members[slot].hp;
}

/* A member's HP up by n, to the most at most. */
void func_800258BC(int slot, int n)
{
    int id = D_8009CBDC[slot];
    int rec;

    if (id == 0xFF)
        return;
    rec = character_record[id];
    members[slot].hp += n;
    if (members[slot].hp > members[slot].max_hp)
        members[slot].hp = members[slot].max_hp;
    D_8009C738[rec].hp = members[slot].hp;
}

void func_80025988(int slot, int n)
{
    int id = D_8009CBDC[slot];
    int rec;
    int mp;

    if (id == 0xFF)
        return;
    rec = character_record[id];
    if ((mp = members[slot].mp - n) < 0)
        members[slot].mp = 0;
    else
        members[slot].mp = mp;
    D_8009C738[rec].mp = members[slot].mp;
}

void func_80025A44(int slot, int n)
{
    int id = D_8009CBDC[slot];
    int rec;

    if (id == 0xFF)
        return;
    rec = character_record[id];
    members[slot].mp += n;
    if (members[slot].mp > members[slot].max_mp)
        members[slot].mp = members[slot].max_mp;
    D_8009C738[rec].mp = members[slot].mp;
}

/* Takes up to n gil; returns what was taken. */
unsigned int func_80025B10(unsigned int n)
{
    unsigned int gil = save_gil.gil;

    if (gil < n) {
        save_gil.gil = 0;
        return gil;
    }
    save_gil.gil = gil - n;
    return n;
}

void func_80025B48(unsigned int n)
{
    if (save_gil.gil + n < save_gil.gil)
        save_gil.gil = 0xFFFFFFFF;
    else
        save_gil.gil += n;
}

unsigned int func_80025B7C(void)
{
    return save_gil.gil;
}

/* Saving and restoring VRAM areas the menus draw over: the faces, the palettes, the window palettes. */
void func_80025B8C(void *p)
{
    RECT r;

    setRECT(&r, 0x340, 0x184, 0x30, 0x78);
    func_80044064(&r, p);
}

void func_80025BD0(void *p)
{
    RECT r;

    setRECT(&r, 0x340, 0x184, 0x30, 0x78);
    func_80044000(&r, p);
}

void func_80025C14(void *p)
{
    RECT r;

    setRECT(&r, 0x180, 0, 0x100, 9);
    func_80044064(&r, p);
}

void func_80025C54(void *p)
{
    RECT r;

    setRECT(&r, 0x180, 0, 0x100, 9);
    func_80044000(&r, p);
}

void func_80025C94(void *p)
{
    RECT r;

    setRECT(&r, 0x100, 0x1ED, 0x100, 3);
    func_80044000(&r, p);
}

void func_80025CD4(void *p)
{
    RECT r;

    setRECT(&r, 0x100, 0x1ED, 0x100, 3);
    func_80044064(&r, p);
}

/* Loads a TIM's image and palette to VRAM at the given places. */
void func_80025D14(void *tim, int x, int y, int clut_x, int clut_y)
{
    TIM_IMAGE t;

    OpenTIM(tim);
    while (ReadTIM(&t)) {
        if (t.caddr) {
            t.crect->x = clut_x;
            t.crect->y = clut_y;
            func_80044000(t.crect, t.caddr);
            DrawSync(0);
        }
        if (t.paddr) {
            t.prect->x = x;
            t.prect->y = y;
            func_80044000(t.prect, t.paddr);
            DrawSync(0);
        }
    }
}

/* All nine faces to VRAM: five to a column from (0x340, 0x100), each with its palette on row i at x 0x180. */
void func_80025DF8(void)
{
    RECT unused; /* the frame has room for one */
    DMA_BUFFER(tim, 0x1000);
    int i;

    for (i = 0; i < 9; i++) {
        cd_read_wait(DISC_FILE(face_files[i]), tim, 0);
        func_80025D14(tim, i / 5 * 24 + 0x340, i % 5 * 48 + 0x100, 0x180, i);
        DrawSync(0);
    }
    DMA_BUFFER_END(0x1000);
}

/* The party's faces and palettes to where the menus draw them from. */
void func_80025ED4(void)
{
    RECT r;
    int i;
    int id;

    DrawSync(0);
    for (i = 0; i < 3; i++) {
        id = D_8009CBDC[i];
        if (id == -1)
            continue;
        setRECT(&r, 0x340, 0x100, 24, 48);
        if (id >= 5)
            r.x = 0x358;
        else
            r.x = 0x340;
        r.y += id % 5 * 48;
        MoveImage(&r, 0x3C0, 0x138 + i * 48);
        DrawSync(0);
        setRECT(&r, 0x180, id, 0x100, 1);
        MoveImage(&r, 0x100, 0x1ED + i);
        DrawSync(0);
    }
    func_80025CD4(D_800756F8);
    DrawSync(0);
}

void func_80026034(void)
{
}

/* A materia's icon palette, by its type. */
int func_8002603C(unsigned char materia)
{
    return icon_palettes[icon_kinds[materia_data[materia].type & 0xF]];
}

void func_80026090(void)
{
    while (cd_step())
        ;
    func_800211C4(7);
    while (cd_step())
        ;
    func_801D11A8();
    D_80062F90 = 0;
}

/* Into the menus: works out the party and loads their faces, keeping VRAM's palettes. */
void func_800260DC(void)
{
    RECT r;
    DMA_BUFFER(cluts, 0x1200);
    DMA_BUFFER(tim, 0x1000);
    int i;
    int id;

    while (cd_step())
        ;
    DrawSync(0);
    func_80025C14(cluts);
    DrawSync(0);
    for (i = 0; i < 3; i++) {
        id = D_8009CBDC[i];
        if (id == 0xFF)
            continue;
        func_80020058(i);
        member_update(i);
        party_effects_sum();
        cd_read_wait(DISC_FILE(face_files[id]), tim, 0);
        func_80025D14(tim, 0x3C0, 0x138 + i * 48, 0x180, i);
        DrawSync(0);
    }
    setRECT(&r, 0x180, 0, 0x100, 3);
    func_80044064(&r, D_800756F8);
    DrawSync(0);
    func_80025C54(cluts);
    DrawSync(0);
    VSync(6);
    while (func_800484A8() == -1)
        ;
    while (IsIdleGPU(1))
        ;
    DMA_BUFFER_END(0x1200 + 0x1000);
}

/* A new game: the menus set up, then the settings' defaults. */
void func_80026258(void)
{
    int i;
    unsigned char *p;

    D_80062F90 = 0;
    func_8001C434();
    func_80026090();
    func_80025008();
    func_800260DC();
    settings.battle_speed = 0x80;
    settings.battle_message_speed = 0x80;
    settings.message_speed = 0x80;
    settings.config = 0x41;
    for (i = 15, p = &settings.button_map[15]; i >= 0; i--)
        *p-- = i;
}

/* The Cat's Bell, which the field and world map call as the party walks: 3 HP, then HP's low bit cleared unless it's
 * full. The character's copy is indexed by the character id, not its record. */
void func_800262D8(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (D_8009CBDC[i] == 0xFF)
            continue;
        if (D_8009C738[character_record[D_8009CBDC[i]]].accessory + 0x120 != 0x13B)
            continue;
        func_800258BC(i, 3);
        if (members[i].hp == members[i].max_hp)
            continue;
        members[i].hp &= ~1;
        D_8009C738[D_8009CBDC[i]].hp &= ~1;
    }
}

/* Plays a menu sound effect. */
void func_80026408(unsigned short effect)
{
    snd_request.id = 0x30;
    snd_request.arg[0] = effect;
    snd_request.arg[1] = effect;
    snd_submit_request();
}

/* Sets up a cursor. */
void func_80026448(MenuCursor *c, int col, int row, int cols, int rows, int pad0, int top, int pad4, int total,
                   int padE, int scroll_step, int wrap_x, int wrap_y, unsigned short scrolling)
{
    c->col = col;
    c->row = row;
    c->cols = cols;
    c->rows = rows;
    c->pad0 = pad0;
    c->top = top;
    c->pad4 = pad4;
    c->total = total;
    c->padE = padE;
    c->scroll_step = scroll_step;
    c->wrap_x = wrap_x;
    c->wrap_y = wrap_y;
    c->scrolling = scrolling;
}

/* Moves a cursor by the pad, with a beep: up and down by row, scrolling the list when wrap_y is 0; left and right by
 * column, on to the next row when wrap_x is 2; R1/L1 a page. A scroll runs over four frames. */
void func_800264A8(MenuCursor *c)
{
    unsigned short pad;

    if (!c->scrolling) {
        pad = field_pad1.repeat;
        if (pad & 0x1000) {
            c->row--;
            switch (c->wrap_y) {
            case 0:
                if (c->row >= 0) {
                    func_80026408(1);
                    break;
                }
                c->row = 0;
                if (c->top > 0) {
                    c->top--;
                    c->scroll_step = -3;
                    c->scrolling = 1;
                    func_80026408(1);
                }
                break;
            case 1:
            case 2:
                if (c->row < 0)
                    c->row = c->rows - 1;
                func_80026408(1);
                break;
            }
        } else if (pad & 0x4000) {
            c->row++;
            switch (c->wrap_y) {
            case 0:
                if (c->row < c->rows) {
                    func_80026408(1);
                    break;
                }
                c->row = c->rows - 1;
                if (c->top < c->total - c->rows) {
                    c->scroll_step = -1;
                    c->scrolling = 2;
                    func_80026408(1);
                }
                break;
            case 1:
            case 2:
                if (c->row >= c->rows)
                    c->row = 0;
                func_80026408(1);
                break;
            }
        } else if (pad & 0x8000) {
            switch (c->wrap_x) {
            case 0:
                if (--c->col >= 0)
                    func_80026408(1);
                else
                    c->col = 0;
                break;
            case 1:
                if (--c->col < 0)
                    c->col = c->cols - 1;
                func_80026408(1);
                break;
            case 2:
                if (!c->col && !c->row && !c->top)
                    break;
                if (--c->col < 0) {
                    c->col = c->cols - 1;
                    if (--c->row < 0) {
                        c->row = 0;
                        if (c->top > 0) {
                            c->top--;
                            c->scroll_step = -3;
                            c->scrolling = 1;
                        }
                    }
                }
                func_80026408(1);
                break;
            }
        } else if (pad & 0x2000) {
            switch (c->wrap_x) {
            case 0:
                if (++c->col < c->cols)
                    func_80026408(1);
                else
                    c->col = c->cols - 1;
                break;
            case 1:
                if (++c->col >= c->cols)
                    c->col = 0;
                func_80026408(1);
                break;
            case 2:
                if (c->col == c->cols - 1 && c->row == c->rows - 1 && c->top == c->total - c->rows)
                    break;
                if (++c->col >= c->cols) {
                    c->col = 0;
                    if (c->row >= c->cols) /* the game's, to no effect */
                        c->col = 0;
                    if (++c->row >= c->rows) {
                        c->row = c->rows - 1;
                        if (c->top < c->total - c->rows) {
                            c->scroll_step = -1;
                            c->scrolling = 2;
                        }
                    }
                }
                func_80026408(1);
                break;
            }
        } else if (pad & 8) {
            c->top += c->rows;
            if (c->top > c->total - c->rows)
                c->top = c->total - c->rows;
            else
                func_80026408(1);
        } else if (pad & 4) {
            c->top -= c->rows;
            if (c->top < 0)
                c->top = 0;
            else
                func_80026408(1);
        }
        return;
    }
    switch (c->scrolling) {
    case 1:
        if (++c->scroll_step == 0) {
            c->scrolling = 0;
            c->scroll_step = 0;
        }
        break;
    case 2:
        if (--c->scroll_step == -4) {
            c->scrolling = 0;
            c->scroll_step = 0;
            c->top++;
        }
        break;
    }
}

/* Where primitives go: the buffer and ordering table, and a saved copy of each. */
void func_800269C0(void *buf)
{
    prim_next.ft4 = buf;
}

void func_800269D0(void)
{
    prim_saved = prim_next.ft4;
}

void func_800269E8(void)
{
    prim_next.ft4 = prim_saved;
}

void func_80026A00(void *ot)
{
    prim_ot = ot;
}

void func_80026A0C(void)
{
    ot_saved = prim_ot;
}

void func_80026A20(void)
{
    prim_ot = ot_saved;
}

void func_80026A34(int dfe, int dtd, int tpage, RECT *tw)
{
    func_80044A68(prim_next.dr_mode, dfe, dtd, tpage, tw);
    AddPrim(prim_ot, prim_next.dr_mode++);
}

/* A drawing environment like env's, clipped to clip (its x and y from env's offset). */
void func_80026A94(DRAWENV *env, RECT *clip)
{
    DRAWENV e;

    func_80043814(&e, env->ofs[0], env->ofs[1], env->clip.w, env->clip.h);
    e.dfe = 1;
    e.isbg = 0;
    e.clip.x += clip->x;
    e.clip.y += clip->y;
    e.clip.w = clip->w;
    e.clip.h = clip->h;
    e.tpage = 0x3F;
    func_80044AC0(prim_next.dr_env, &e);
    AddPrim(prim_ot, prim_next.dr_env);
    prim_next.dr_env++;
}

void func_80026B5C(void)
{
}
