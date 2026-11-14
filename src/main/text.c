/* The kernel's text, 0x80014C70-0x800155A4: its sections in memory, the strings' inserts and packed copies, and the
 * names of things (docs/knowledge_base.md, KERNEL.BIN). Built like random.c before it, -G8 and ASPSX 2.34: the
 * section count and the bytes used are reached through $gp. */

int text_sections;                   /* sections loaded so far */
int text_used;                       /* bytes of text_buffer they take */
extern unsigned short text_offsets[]; /* each section's place in text_buffer */
extern unsigned char text_buffer[];
extern unsigned char unpack_buffer[256];
extern unsigned char actor_name[];
extern unsigned char no_text[];

/* The kernel's tables don't keep each kind of thing apart: spells, summons and enemy skills share one list of spell
 * records, and the inventory numbers items, weapons, armor, accessories and materia one after another. */
extern int attack_kind_starts[];         /* magic, summons, enemy skills: where each starts among the spells */
extern unsigned short inventory_starts[]; /* where each kind of the inventory starts, then the end */
extern unsigned char inventory_kinds[];   /* the kind each of them is to kind_text */
extern unsigned char kind_name_offsets[]; /* kind_text's kinds 0-3: where each starts in its name section */
extern unsigned char kind_sections[];     /* each kind's name section, 0xFF for one that isn't the kernel's */

extern unsigned char D_800708D4[]; /* +0x10 of the first spell record */
extern unsigned char D_800722DC[]; /* +0x10 of the first item record */

typedef struct {
    unsigned char id;
    unsigned char pad1[15];
    unsigned char name[12];
    unsigned char pad1C[0x84 - 0x1C];
} Character;
extern Character D_8009C738[]; /* the save's nine characters */

/* The battle's, loaded with it. */
typedef struct {
    short enemy; /* which of the scene's three enemies */
    unsigned char pad2[14];
} FormationSlot;
typedef struct {
    signed char character;
    unsigned char pad1[15];
} PartySlot;
typedef struct {
    unsigned char name[32];
    unsigned char pad20[184 - 32];
} Enemy;
typedef struct {
    unsigned char pad0[0xF];
    unsigned char letter; /* the one that tells two of the same enemy apart, 0xFF for none */
    unsigned char pad10[0x19];
    unsigned char flags;
    unsigned char pad2A[0x12];
    unsigned short u3C;
    unsigned char pad3E[6];
} ActorInfo;
typedef struct {
    int pad0;
    int flags;
    unsigned char pad8[0x28];
    int hp;
    unsigned char pad34[0x34];
} BattleActor;
extern FormationSlot D_80163658[];
extern PartySlot D_801636B8[];
extern Enemy D_800F5F44[];
extern ActorInfo D_800F5BB8[];
extern BattleActor D_800F83E0[];
extern unsigned char D_800F652C[]; /* 32-byte names */
extern int D_800F7ED0;             /* the font's "A" */
extern int D_800FAFD0;             /* the font's "0" */

unsigned char *kind_text(int kind, int index, int names);
unsigned char *text_unpack(unsigned char *src, unsigned char *start);
unsigned char *battle_text(int index);
unsigned char *battle_text_unpacked(int index);
unsigned char *func_800A5F90(int index);
void func_800A5E0C(unsigned char *dst, unsigned char *format, unsigned short *args);

void text_reset(void)
{
    text_sections = 0;
    text_used = 0;
}

/* The next section's room. Nothing checks it against the buffer's end. */
unsigned char *text_alloc(int size)
{
    int used = text_used;

    text_offsets[text_sections++] = used;
    text_used = used + size;
    return text_buffer + used;
}

/* Byte +0x10 of a spell's (kind 0-2) or an item's (kind 4) record, -1 for 0xFF or anything else. */
int attack_byte_10(int kind, int index)
{
    unsigned char v = 0xFF;
    int ret = -1;

    switch (kind) {
    case 0:
    case 1:
    case 2:
        v = D_800708D4[(attack_kind_starts[kind] + index) * 28];
        break;
    case 4:
        if (index < 0x80)
            v = D_800722DC[index * 28];
        break;
    }
    if (v != 0xFF)
        ret = v;
    return ret;
}

/* Copies a string without its 0xFF, at most n + 1 bytes (n -1 for no limit); returns where it stopped. */
unsigned char *text_copy(unsigned char *dst, unsigned char *src, int n)
{
    unsigned char c;

    while ((c = *src) != 0xFF) {
        *dst = c;
        src++;
        dst++;
        if (n-- == 0)
            break;
    }
    return dst;
}

/* String index of a loaded section (0 is section 9, the first with text). */
unsigned char *text_string(int section, int index, int first)
{
    unsigned char *p = text_buffer + text_offsets[section + first];

    return p + ((unsigned short *)p)[index];
}

unsigned char *text_copy_string(int section, int index, unsigned char *dst)
{
    return text_copy(dst, text_string(section, index, 0), -1);
}

unsigned char *text_copy_character_name(int id, unsigned char *dst)
{
    int i;

    for (i = 0; i < 9; i++) {
        if (D_8009C738[i].id == id) {
            dst = text_copy(dst, D_8009C738[i].name, 12);
            break;
        }
    }
    return dst;
}

/* Copies text to dst with its inserts (0xEA-0xF1 and a big-endian u16) filled in, up to and with its 0xFF. A packed
 * copy (0xF9 and its byte) stays as it is, for text_unpack. */
unsigned char *text_expand(unsigned char *dst, unsigned char *text)
{
    unsigned char *out = dst;
    int pos = 0; /* an index, not a pointer: the loop optimizer makes it one, after the digits' address */
    unsigned char c;
    unsigned short arg;
    unsigned int digits[16];
    int n;

    c = 0;
    while (c != 0xFF) {
        c = text[pos++];
        if (c >= 0xEA && c <= 0xF1) {
            arg = text[pos++] << 8;
            arg |= text[pos++];
            switch (c) {
            case 0xEA:
                out = text_copy_character_name(arg, out);
                break;
            case 0xEB:
                out = text_copy(out, kind_text(4, arg, 8), -1);
                break;
            case 0xEC:
                n = 0;
                do {
                    digits[n++] = arg % 10;
                    arg /= 10;
                } while (arg != 0 && n < sizeof digits / sizeof *digits);
                for (; n > 0; n--)
                    *out++ = digits[n - 1] + D_800FAFD0;
                break;
            case 0xED: /* 0-2 the party, 4 on the enemies */
                if (arg < 3)
                    out = text_copy_character_name(D_801636B8[arg].character, out);
                else if (arg >= 4)
                    out = text_copy(out, D_800F5F44[D_80163658[arg - 4].enemy].name, 32);
                break;
            case 0xEE:
                out = text_copy_string(9, arg, out);
                break;
            case 0xEF:
                if (arg < 26)
                    *out++ = arg + D_800F7ED0;
                break;
            case 0xF0:
                out = text_copy_string(16, arg, out);
                break;
            case 0xF1:
                out = text_copy_string(arg >> 8, arg & 0xFF, out);
                break;
            }
        } else {
            *out++ = c;
            if (c == 0xF9)
                *out++ = text[pos++];
        }
    }
    return dst;
}

/* Unpacks src into unpack_buffer: 0xF9 b copies (b >> 6) * 2 + 4 bytes from (b & 0x3F) + 1 bytes before the 0xF9,
 * counted in start, the string as it was packed (src itself, or before text_expand moved it). */
unsigned char *text_unpack(unsigned char *src, unsigned char *start)
{
    unsigned int n = 0;
    int pos = 0;
    unsigned char *buf = unpack_buffer;
    unsigned char *out = buf;
    unsigned char c;
    int i, len, back;

    do {
        c = *src++;
        pos++;
        if (c == 0xF9) {
            do { /* the original's form (a macro, by the look of it); plain statements order the registers otherwise */
                c = *src++;
                pos++;
            } while (0);
            back = pos - ((c & 0x3F) + 3);
            len = (c >> 6) * 2 + 4;
            for (i = 0; i < len; i++) {
                c = start[back++];
                out++;
                buf[n++] = c;
            }
        } else if (c >= 0xEA && c <= 0xF1) {
            *out++ = c;
            *out++ = *src++;
            pos += 2;
            n += 3;
            *out++ = *src++;
        } else {
            *out++ = c;
            n++;
        }
    } while (c != 0xFF && n < sizeof unpack_buffer);
    return unpack_buffer;
}

unsigned char *battle_text(int index)
{
    return text_string(16, index, 0);
}

unsigned char *battle_text_unpacked(int index)
{
    unsigned char *text = battle_text(index);

    return text_unpack(text, text);
}

/* The name (names 8) or the help text (names 0) of a thing by kind: 0-2 magic, summons and enemy skills, 3 the
 * attacks from 0x80, 4 the inventory's numbering, 5 commands, 10-14 weapons, armor, accessories, materia and key items;
 * 6-9 the battle's. */
unsigned char *kind_text(int kind, int index, int names)
{
    unsigned char *text = no_text;
    unsigned char buf[256];
    unsigned short args[2];
    unsigned char *out;
    unsigned char *s;
    int section;
    unsigned int i;
    int actor;

    if (kind == 4) {
        for (i = 0; i < 5; i++) {
            if (index < inventory_starts[i + 1]) {
                kind = inventory_kinds[i];
                index -= inventory_starts[i];
                break;
            }
        }
    }
    if (kind == 3 && index == 0x7F)
        index = 0xFF;
    if (index != 0xFF) {
        if (kind >= 0 && kind < 4 && index + kind_name_offsets[kind] < 0xE0)
            index += kind_name_offsets[kind];
        section = kind_sections[kind];
        if (section != 0xFF) {
            text = text_string(section + names, index, 0);
            if (!names)
                text = text_unpack(text, text);
        } else {
            switch (kind) {
            case 6:
                text = text_string(index < 16 ? 17 : 9, index, 0);
                break;
            case 7: /* an enemy on the field, with its letter and what the battle shows of it */
                if (index < 6) {
                    actor = index + 4;
                    out = actor_name;
                    out = text_copy(out, D_800F5F44[D_80163658[index].enemy].name, 32);
                    if (D_800F5BB8[actor].letter != 0xFF)
                        *out++ = D_800F5BB8[actor].letter + D_800F7ED0;
                    if (D_800F83E0[actor].flags & 0x40)
                        out = text_copy(out, battle_text_unpacked(0x71), -1);
                    if (D_800F5BB8[actor].flags & 0x40) {
                        args[0] = D_800F5BB8[actor].u3C;
                        args[1] = D_800F83E0[actor].hp;
                        out = text_copy(out, battle_text_unpacked(0x7F), -1);
                        func_800A5E0C(out, battle_text_unpacked(0x72), args);
                        text_expand(buf, out);
                        out = text_copy(out, buf, -1);
                    }
                    *out = 0xFF;
                    text = actor_name;
                }
                break;
            case 8:
                if (index >= 0x100)
                    s = func_800A5F90(index - 0x100);
                else
                    s = battle_text(index);
                text = text_unpack(text_expand(buf, s), s);
                break;
            case 9:
                text = D_800F652C + index * 32;
                break;
            }
        }
    }
    return text;
}
