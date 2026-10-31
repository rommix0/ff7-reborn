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

#define MENU 0x801D0000u /* another load address: its modules come and go while one stays at START */
static int c_entry(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7) { return 0xC + a0 + a7; }
static const CodePatch c_code[] = {{MENU, (void *)c_entry}};
static Module c = {MENU, {0}, c_code, 0, 1, 0};

#define EFFECT 0x801B0000u /* two effect files open with the same 64 bytes; their code's hash tells them apart */
static int d_entry(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7) { return 0xD + a0 + a7; }
static int e_entry(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7) { return 0xE + a0 + a7; }
static const CodePatch d_code[] = {{EFFECT, (void *)d_entry}}, e_code[] = {{EFFECT, (void *)e_entry}};
static unsigned char d_image[128], e_image[128];
static Module d = {EFFECT, {0}, d_code, 0, 1, 0, sizeof d_image}, e = {EFFECT, {0}, e_code, 0, 1, 0, sizeof e_image};

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
        c.signature[i] = 3 * i;
    }
    module_register(&a);
    module_register(&b);
    module_register(&c);

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

    /* modules_at_other_addresses: with b still at START, a call into the menu's address reaches the menu */
    memcpy((void *)MENU, c.signature, sizeof c.signature);
    assert(module_call(MENU, 1, 0, 0, 0, 0, 0, 0, 2) == 0xC + 3);
    assert(module_call(START, 1, 0, 0, 0, 0, 0, 0, 2) == 0xB + 3);

    /* same_signature_told_apart_by_hash */
    d_image[100] = 1;
    e_image[100] = 2;
    d.hash = code_hash(d_image, sizeof d_image);
    e.hash = code_hash(e_image, sizeof e_image);
    module_register(&d);
    module_register(&e);
    memcpy((void *)EFFECT, e_image, sizeof e_image);
    assert(module_call(EFFECT, 1, 0, 0, 0, 0, 0, 0, 2) == 0xE + 3);
    memcpy((void *)EFFECT, d_image, sizeof d_image);
    assert(module_call(EFFECT, 1, 0, 0, 0, 0, 0, 0, 2) == 0xD + 3);
    puts("ok");
    return 0;
}
