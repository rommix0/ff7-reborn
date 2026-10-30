/* Calls through a code address. Recompiled code and C both hold native function pointers; a PS1 code address that
 * still turns up (one built at run time, say) is looked up in the recompiler's tables. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp.h"

typedef int (*NativeCall)(int, int, int, int, int, int, int, int);

static struct {
    const CodePatch *table;
    int n;
} tables[64];
static int table_count;

void code_register(const CodePatch *table, int n)
{
    if (table_count == 64) {
        fputs("too many code tables\n", stderr);
        exit(1);
    }
    tables[table_count].table = table;
    tables[table_count++].n = n;
}

static void *find(const CodePatch *table, int n, unsigned int addr)
{
    int lo, hi, mid;

    for (lo = 0, hi = n - 1; lo <= hi;) {
        mid = (lo + hi) / 2;
        if (table[mid].addr == addr)
            return table[mid].func;
        if (table[mid].addr < addr)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return 0;
}

void *code_lookup(unsigned int addr)
{
    int t;
    void *f;

    for (t = table_count - 1; t >= 0; t--) {
        if ((f = find(tables[t].table, tables[t].n, addr)))
            return f;
    }
    return 0;
}

void patch_code_pointers(const CodePatch *p, int n)
{
    for (; n > 0; n--, p++)
        mem_w32(p->addr, (unsigned int)p->func);
}

/* A game code address as a native function: the exe's, then a loaded module's. */
static void *code_at(unsigned int addr)
{
    void *f = code_lookup(addr);

    if (!f)
        f = module_function(addr);
    if (!f) {
        fprintf(stderr, "no module for %08X\n", addr);
        exit(1);
    }
    return f;
}

static const Module *modules[64];
static int module_count;

void module_register(const Module *m)
{
    if (module_count == 64) {
        fputs("too many modules\n", stderr);
        exit(1);
    }
    modules[module_count++] = m;
}

/* The game loads a module by reading it over whatever was there, so which one is loaded is read off RAM each time.
 * Its data comes in with game code addresses: a word still holding the one it was loaded with gets the native
 * function, while anything the game has put there since stays. ponytail: a signature compare and a patch pass per
 * call; cache the last match if it shows. */
void *module_function(unsigned int addr)
{
    const Module *m;
    const CodePatch *p;
    int i;

    for (i = 0; i < module_count; i++) {
        m = modules[i];
        if (memcmp((const void *)m->start, m->signature, sizeof m->signature))
            continue;
        for (p = m->patches; p < m->patches + m->patch_count; p++) {
            if (mem_r32(p->addr) == p->game)
                mem_w32(p->addr, (unsigned int)p->func);
        }
        return find(m->code, m->code_count, addr);
    }
    return 0;
}

/* C's calls into modules, through the stubs tools/pc_syms.py writes. */
int module_call(unsigned int addr, int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
    return ((NativeCall)code_at(addr))(a0, a1, a2, a3, a4, a5, a6, a7);
}

/* The callee gets a0-a3 and the four words above the caller's $sp, as a MIPS function would find them. */
void call_indirect(unsigned int addr)
{
    unsigned int sp = ctx.r[29];
    void *f;

    if (addr == BIOS_A0 || addr == BIOS_B0 || addr == BIOS_C0) {
        ctx.r[2] = bios_call(addr, ctx.r[9]);
        return;
    }
    f = addr >= RAM_START && addr < RAM_START + RAM_SIZE ? code_at(addr) : (void *)addr;
    ctx.r[2] = ((NativeCall)f)(ctx.r[4], ctx.r[5], ctx.r[6], ctx.r[7], mem_r32(sp + 0x10), mem_r32(sp + 0x14),
                               mem_r32(sp + 0x18), mem_r32(sp + 0x1C));
}
