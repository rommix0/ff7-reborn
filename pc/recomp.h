#ifndef PC_RECOMP_H
#define PC_RECOMP_H

/* What code from tools/recomp.py runs on: the R3000's registers, its memory through memory.h, and the hooks the rest
 * of the PC build provides. */
#include "memory.h"

typedef struct Ctx {
    unsigned int r[32], hi, lo;
} Ctx;

extern Ctx ctx;
extern unsigned int cop0[32]; /* system control: SR is 12, CAUSE 13, EPC 14 */

/* A game code address and the native function for it. tools/recomp.py writes a table of these for the functions it
 * recompiles, and a list of the code pointers in the game's data, which patch_code_pointers rewrites: in the PC build
 * every code pointer the game holds is a native address, and C can call it. */
typedef struct CodePatch {
    unsigned int addr;
    void *func;
    unsigned int game; /* in a patch list: the code address the word holds in the game's data */
} CodePatch;

void code_register(const CodePatch *table, int n); /* sorted by addr */
void *code_lookup(unsigned int addr);
void patch_code_pointers(const CodePatch *p, int n);
/* A module (an overlay loaded at 0x800A0000 or above, several to an address) as tools/recomp.py writes it: the one in
 * RAM is the one whose first function's opening bytes match its signature. */
typedef struct Module {
    unsigned int start; /* its first function */
    unsigned char signature[64];
    const CodePatch *code, *patches;
    int code_count, patch_count;
} Module;

void module_register(const Module *m);
void *module_function(unsigned int addr); /* the loaded module's function at addr, or 0 */
int module_call(unsigned int addr, int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7);
void pc_vblank(void);                     /* the PC build's once-a-frame hook (main.c) */

/* The BIOS in C (bios.c). call_indirect sends the BIOS's three entry points, 0xA0, 0xB0 and 0xC0, to bios_call with
 * the function number in $t1. irq_raise sets an interrupt's bit in I_STAT and, when the CPU accepts interrupts, runs
 * the game's handlers on a saved copy of the registers; irq_check delivers one that had to wait. */
#define BIOS_A0 0xA0u
#define BIOS_B0 0xB0u
#define BIOS_C0 0xC0u
int bios_call(int table, int n);
void irq_raise(unsigned int bit);
void irq_check(void);
extern unsigned int i_stat, i_mask;
extern unsigned short pad_buttons; /* active low, in the PS1's bit order */

void hw_poll(void);                       /* at every backward branch: lets interrupts in */
void call_indirect(unsigned int addr);    /* jalr, and jr through a register that isn't a jump table */
void recomp_missing(unsigned int addr);   /* a jal whose target wasn't recompiled */
void recomp_syscall(unsigned int code);
void recomp_break(unsigned int code);
void gte_cmd(unsigned int op);
unsigned int gte_read(int reg);
void gte_write(int reg, unsigned int v);
unsigned int gte_read_ctrl(int reg);
void gte_write_ctrl(int reg, unsigned int v);

/* The unaligned loads and stores, little-endian (psx-spx): LWR/SWR handle the low end of the word, LWL/SWL the high
 * end, so a pair reaches the four bytes from any address. */
static inline unsigned int op_lwl(unsigned int rt, unsigned int addr)
{
    int shift = (addr & 3) * 8;

    return (rt & (0x00FFFFFFu >> shift)) | mem_r32(addr & ~3u) << (24 - shift);
}

static inline unsigned int op_lwr(unsigned int rt, unsigned int addr)
{
    int shift = (addr & 3) * 8;

    return (rt & ~(0xFFFFFFFFu >> shift)) | mem_r32(addr & ~3u) >> shift;
}

static inline void op_swl(unsigned int rt, unsigned int addr)
{
    int shift = (addr & 3) * 8;
    unsigned int word = mem_r32(addr & ~3u);

    mem_w32(addr & ~3u, (word & ~(0xFFFFFFFFu >> (24 - shift))) | rt >> (24 - shift));
}

static inline void op_swr(unsigned int rt, unsigned int addr)
{
    int shift = (addr & 3) * 8;
    unsigned int word = mem_r32(addr & ~3u);

    mem_w32(addr & ~3u, (word & ~(0xFFFFFFFFu << shift)) | rt << shift);
}

/* DIV and DIVU never trap: dividing by zero or 0x80000000 by -1 gives these fixed results. */
static inline void op_div(int n, int d)
{
    if (!d) {
        ctx.lo = n >= 0 ? 0xFFFFFFFFu : 1;
        ctx.hi = n;
    } else if ((unsigned int)n == 0x80000000u && d == -1) {
        ctx.lo = 0x80000000u;
        ctx.hi = 0;
    } else {
        ctx.lo = n / d;
        ctx.hi = n % d;
    }
}

static inline void op_divu(unsigned int n, unsigned int d)
{
    ctx.lo = d ? n / d : 0xFFFFFFFFu;
    ctx.hi = d ? n % d : n;
}

#endif
