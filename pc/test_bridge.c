/* Calls between recompiled code and C, and code pointers kept as native addresses. usage: test_bridge SCUS_941.63 */
#include <assert.h>
#include <stdio.h>
#include "memory.h"
#include "recomp.h"

Ctx ctx;
unsigned int cop0[32];
void hw_poll(void) {}
void recomp_missing(unsigned int addr) { fprintf(stderr, "missing %08X\n", addr); assert(0); }
void recomp_syscall(unsigned int code) { (void)code; }
int bios_call(int table, int n) { (void)table; (void)n; assert(0); return 0; }
void recomp_break(unsigned int code) { (void)code; }
void gte_cmd(unsigned int op) { (void)op; }
unsigned int gte_read(int reg) { return reg; }
void gte_write(int reg, unsigned int v) { (void)reg; (void)v; }
unsigned int gte_read_ctrl(int reg) { return reg; }
void gte_write_ctrl(int reg, unsigned int v) { (void)reg; (void)v; }
unsigned int io_read(unsigned int addr, int size) { (void)addr; (void)size; return 0; }
void io_write(unsigned int addr, unsigned int v, int size) { (void)addr; (void)v; (void)size; }

/* build/pc/gen/test_bridge.c: cd_read and cd_state_done recompiled, with their tables. */
extern const CodePatch bridge_code[], bridge_patches[];
extern const int bridge_code_count, bridge_patch_count;
int cd_read(int lba, int size, int dest, int callback, int a4, int a5, int a6, int a7);
int cd_state_done(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7);

static int seen[5];

/* Native, so the recompiled cd_read calls it the C way; its fifth argument comes off the PS1 stack. */
int cd_start_request(int mode, unsigned int lba, unsigned int size, void *dest, void (*callback)(void))
{
    seen[0] = mode;
    seen[1] = lba;
    seen[2] = size;
    seen[3] = (int)dest;
    seen[4] = (int)callback;
    return 0;
}

int main(int argc, char **argv)
{
    unsigned int sp = 0x801FFF00;

    assert(argc == 2 && mem_map() == 0 && exe_load(argv[1]));
    ctx.r[29] = sp;
    assert(cd_read(0x1234, 0x800, 0x80100000, 0x80020000, 0, 0, 0, 0) == 0);
    assert(seen[0] == 3 && seen[1] == 0x1234 && seen[2] == 0x800 && seen[3] == 0x80100000 && seen[4] == 0x80020000);
    assert(ctx.r[29] == sp);

    /* code_pointer_roundtrip: the state table now holds native functions, and either form of address calls them */
    patch_code_pointers(bridge_patches, bridge_patch_count);
    assert(mem_r32(0x8004A678) == (unsigned int)cd_state_done);
    code_register(bridge_code, bridge_code_count);
    assert(code_lookup(0x80034430) == (void *)cd_state_done);
    mem_w32(0x80071A60, 0);
    call_indirect(0x80034430);
    assert(mem_r32(0x80071A60) == 16);
    mem_w32(0x80071A60, 0);
    call_indirect(mem_r32(0x8004A678));
    assert(mem_r32(0x80071A60) == 16);
    puts("ok");
    return 0;
}
