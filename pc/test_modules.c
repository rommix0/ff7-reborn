/* Modules sharing a load address: a call reaches the one whose code is in RAM, and its code pointers get patched. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "recomp.h"

Ctx ctx;
unsigned int cop0[32];
int bios_call(int table, int n) { (void)table; (void)n; assert(0); return 0; }
unsigned int io_read(unsigned int addr, int size) { (void)addr; (void)size; return 0; }
void io_write(unsigned int addr, unsigned int v, int size) { (void)addr; (void)v; (void)size; }

#define START 0x800A0000u
#define SLOT 0x800A0100u /* a word of the module's data holding a code address */

static int a_entry(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7) { return 0xA + a0 + a7; }
static int b_entry(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7) { return 0xB + a0 + a7; }

static const CodePatch a_code[] = {{START, (void *)a_entry}}, a_patches[] = {{SLOT, (void *)a_entry, START}};
static const CodePatch b_code[] = {{START, (void *)b_entry}}, b_patches[] = {{SLOT, (void *)b_entry, START}};
static Module a = {START, {0}, a_code, a_patches, 1, 1}, b = {START, {0}, b_code, b_patches, 1, 1};

/* What cd_read leaves in RAM: the module's code, and its data with the game's code addresses. */
static void load(const Module *m)
{
    memcpy((void *)START, m->signature, sizeof m->signature);
    mem_w32(SLOT, START);
}

int main(void)
{
    int i;

    assert(mem_map() == 0);
    for (i = 0; i < 64; i++) {
        a.signature[i] = i;
        b.signature[i] = 64 - i;
    }
    module_register(&a);
    module_register(&b);

    /* overlay_switch_by_content */
    load(&a);
    assert(module_call(START, 1, 0, 0, 0, 0, 0, 0, 2) == 0xA + 3);
    assert(mem_r32(SLOT) == (unsigned int)a_entry);
    load(&b);
    assert(module_call(START, 1, 0, 0, 0, 0, 0, 0, 2) == 0xB + 3);
    assert(mem_r32(SLOT) == (unsigned int)b_entry);

    /* a code pointer the game set since stays as it is, and so does a data pointer it put in that word */
    mem_w32(SLOT, (unsigned int)a_entry);
    module_call(START, 0, 0, 0, 0, 0, 0, 0, 0);
    assert(mem_r32(SLOT) == (unsigned int)a_entry);
    mem_w32(SLOT, 0x80100000);
    module_call(START, 0, 0, 0, 0, 0, 0, 0, 0);
    assert(mem_r32(SLOT) == 0x80100000);
    puts("ok");
    return 0;
}
