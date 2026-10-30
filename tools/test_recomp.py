"""Checks tools/recomp.py on hand-assembled fixtures: each is recompiled, built with a 32-bit gcc and run.

usage (WSL or Linux): toolchain/venv/bin/python tools/test_recomp.py
"""
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from recomp import code_pointers, recompile

ZERO, V0, V1, A0, A1, T0, RA = 0, 2, 3, 4, 5, 8, 31
BASE = 0x80100000


def r_type(funct, rs=0, rt=0, rd=0, sa=0):
    return rs << 21 | rt << 16 | rd << 11 | sa << 6 | funct


def i_type(op, rs, rt, imm):
    return op << 26 | rs << 21 | rt << 16 | imm & 0xFFFF


def addiu(rt, rs, imm):
    return i_type(9, rs, rt, imm)


NOP = 0
JR_RA = r_type(8, rs=RA)


def run(words, body, funcs=None):
    """Recompile words at BASE as function f, then run body (C) in a main that prints its results."""
    c = recompile(b"".join(w.to_bytes(4, "little") for w in words), BASE, funcs or {BASE: "f"}, set(), {})
    harness = f"""#include <stdio.h>
#include "recomp.h"
Ctx ctx;
unsigned int cop0[32];
int polls;
void hw_poll(void) {{ polls++; }}
unsigned int indirect;
void call_indirect(unsigned int addr) {{ indirect = addr; }}
void recomp_missing(unsigned int addr) {{ (void)addr; }}
void recomp_syscall(unsigned int code) {{ (void)code; }}
void recomp_break(unsigned int code) {{ (void)code; }}
void gte_cmd(unsigned int op) {{ (void)op; }}
unsigned int gte_read(int reg) {{ return reg; }}
void gte_write(int reg, unsigned int v) {{ (void)reg; (void)v; }}
unsigned int gte_read_ctrl(int reg) {{ return reg; }}
void gte_write_ctrl(int reg, unsigned int v) {{ (void)reg; (void)v; }}
unsigned int io_read(unsigned int addr, int size) {{ (void)addr; (void)size; return 0; }}
void io_write(unsigned int addr, unsigned int v, int size) {{ (void)addr; (void)v; (void)size; }}
{c}
int main(void)
{{
{body}
    return 0;
}}
"""
    with tempfile.TemporaryDirectory() as tmp:
        src, exe = Path(tmp, "t.c"), Path(tmp, "t")
        src.write_text(harness)
        subprocess.run(["gcc", "-m32", "-O1", "-w", "-Ipc", str(src), "-o", str(exe)], check=True)
        return subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.split()


def call(regs):
    return "".join(f"    ctx.r[{n}] = {v};\n" for n, v in regs.items()) + "    r_80100000();\n"


def delay_slot_runs_on_both_paths():
    words = [i_type(4, A0, ZERO, 2), addiu(V0, ZERO, 5), addiu(V0, V0, 1), JR_RA, NOP]  # beq a0, zero, L
    for a0, v0 in ((0, 5), (1, 6)):
        assert run(words, call({A0: a0}) + '    printf("%u", ctx.r[2]);') == [str(v0)]


def branch_reads_condition_before_slot():
    words = [i_type(4, A0, ZERO, 2), addiu(A0, A0, 1), addiu(V1, ZERO, 7), JR_RA, NOP]
    assert run(words, call({A0: 0}) + '    printf("%u %u", ctx.r[3], ctx.r[4]);') == ["0", "1"]


def jr_ra_returns():
    words = [JR_RA, addiu(V0, ZERO, 3), addiu(V0, ZERO, 9)]
    assert run(words, call({}) + '    printf("%u", ctx.r[2]);') == ["3"]


def div_by_zero_values():
    div, divu = r_type(0x1A, A0, A1), r_type(0x1B, A0, A1)
    tail = [r_type(0x12, rd=V0), r_type(0x10, rd=V1), JR_RA, NOP]  # mflo v0, mfhi v1
    show = '    printf("%x %x", ctx.r[2], ctx.r[3]);'
    assert run([div] + tail, call({A0: 5, A1: 0}) + show) == ["ffffffff", "5"]
    assert run([div] + tail, call({A0: -5, A1: 0}) + show) == ["1", "fffffffb"]
    assert run([div] + tail, call({A0: 0x80000000, A1: -1}) + show) == ["80000000", "0"]
    assert run([divu] + tail, call({A0: 5, A1: 0}) + show) == ["ffffffff", "5"]


def unaligned_pair_order():
    # lwr v0, 1(a0); lwl v0, 4(a0) loads bytes 1-4; swr a1, 1(a0); swl a1, 4(a0) stores them
    words = [i_type(0x26, A0, V0, 1), i_type(0x22, A0, V0, 4), i_type(0x2E, A0, A1, 1), i_type(0x2A, A0, A1, 4),
             JR_RA, NOP]
    body = ("    static unsigned char b[8] = {10, 11, 12, 13, 14, 15, 16, 17};\n"
            + call({A0: "(unsigned int)b", A1: "0xAABBCCDD"})
            + '    printf("%x %u %x %x %x %x %u", ctx.r[2], b[0], b[1], b[2], b[3], b[4], b[5]);')
    assert run(words, body) == ["e0d0c0b", "10", "dd", "cc", "bb", "aa", "15"]  # bytes 0x0B-0x0E


def backward_branch_polls():
    # L: addiu a0, a0, -1; bne a0, zero, L; nop
    words = [addiu(A0, A0, -1), i_type(5, A0, ZERO, -2), NOP, JR_RA, NOP]
    assert run(words, call({A0: 3}) + '    printf("%u %d", ctx.r[4], polls);') == ["0", "2"]


def bltzal_links():
    words = [i_type(1, A0, 0x10, 2), NOP, addiu(V0, ZERO, 1), JR_RA, NOP]  # bltzal a0, +2
    body = call({A0: -1, RA: 0}) + '    printf("%x %u", ctx.r[31], ctx.r[2]);'
    assert run(words, body) == [f"{BASE + 8:x}", "0"]


def mult_is_signed_64_bit():
    words = [r_type(0x18, A0, A1), r_type(0x12, rd=V0), r_type(0x10, rd=V1), JR_RA, NOP]
    body = call({A0: -3, A1: 0x40000000}) + '    printf("%x %x", ctx.r[2], ctx.r[3]);'
    assert run(words, body) == ["40000000", "ffffffff"]


def function_address_becomes_native():
    # lui v0, %hi(g); addiu v0, v0, %lo(g) where g is a function: the value is g's native thunk, as C would take it
    g = BASE + 0x8010
    words = [i_type(0xF, 0, V0, (g + 0x8000) >> 16), addiu(V0, V0, g & 0xFFFF), JR_RA, NOP] + [NOP] * 0x2000
    words[(g - BASE) // 4:(g - BASE) // 4 + 2] = [JR_RA, NOP]
    body = call({}) + '    printf("%d", ctx.r[2] == (unsigned int)g);'
    assert run(words, body, {BASE: "f", g: "g"}) == ["1"]


def function_address_kept_for_compares():
    # lui t1, %hi(g); addiu t1, t1, %lo(g); loop bne t0, t1: a copy loop's end, not a code pointer (libgte's
    # _patch_gte copies words up to PadInit's address)
    g = BASE + 0x8010
    T1 = 9
    words = [i_type(0xF, 0, T1, (g + 0x8000) >> 16), addiu(T1, T1, g & 0xFFFF), addiu(T0, ZERO, 0),
             i_type(5, T0, T1, 1), NOP, JR_RA, NOP] + [NOP] * 0x2000
    words[(g - BASE) // 4:(g - BASE) // 4 + 2] = [JR_RA, NOP]
    body = call({}) + f'    printf("%x", ctx.r[9]);'
    assert run(words, body, {BASE: "f", g: "g"}) == [f"{g:x}"]


def stored_function_address_becomes_native():
    # lui t1, %hi(g); addiu t1, t1, %lo(g); sw t1, 0(a0): a callback stored for later
    g = BASE + 0x8010
    T1 = 9
    words = [i_type(0xF, 0, T1, (g + 0x8000) >> 16), addiu(T1, T1, g & 0xFFFF), i_type(0x2B, A0, T1, 0),
             JR_RA, NOP] + [NOP] * 0x2000
    words[(g - BASE) // 4:(g - BASE) // 4 + 2] = [JR_RA, NOP]
    body = "    static unsigned int slot;\n" + call({A0: "(unsigned int)&slot"}) + '    printf("%d", slot == (unsigned int)g);'
    assert run(words, body, {BASE: "f", g: "g"}) == ["1"]


def address_stored_in_return_slot_becomes_native():
    # lui t1, %hi(g); addiu t1, t1, %lo(g); jr ra; sw t1, 0(a0): stored by the return's delay slot
    g = BASE + 0x8010
    T1 = 9
    words = [i_type(0xF, 0, T1, (g + 0x8000) >> 16), addiu(T1, T1, g & 0xFFFF), JR_RA, i_type(0x2B, A0, T1, 0)] \
        + [NOP] * 0x2000
    words[(g - BASE) // 4:(g - BASE) // 4 + 2] = [JR_RA, NOP]
    body = "    static unsigned int slot;\n" + call({A0: "(unsigned int)&slot"}) + '    printf("%d", slot == (unsigned int)g);'
    assert run(words, body, {BASE: "f", g: "g"}) == ["1"]


def module_call_goes_indirect():
    # jal 0x800A0030: a module loaded at run time, found through call_indirect
    words = [0x0C000000 | (0x800A0030 & 0x3FFFFFF) >> 2, NOP, JR_RA, NOP]
    assert run(words, call({}) + '    printf("%x", indirect);') == ["800a0030"]


def setjmp_return_is_an_entry():
    # jal setjmp; nop; addiu v0, v0, 5: an interrupt (the BIOS's custom exit, libetc's) resumes after the jal with
    # v0 = 1, on the frame setjmp saw, so that point gets an entry of its own
    s = BASE + 0x8010
    words = [0x0C000000 | (s & 0x3FFFFFF) >> 2, NOP, addiu(V0, V0, 5), JR_RA, NOP] + [NOP] * 0x2000
    words[(s - BASE) // 4:(s - BASE) // 4 + 2] = [JR_RA, addiu(V0, ZERO, 0)]
    body = call({}) + '    printf("%u ", ctx.r[2]);\n    ctx.r[2] = 1;\n    r_80100008();\n    printf("%u", ctx.r[2]);'
    assert run(words, body, {BASE: "f", s: "setjmp"}) == ["5", "6"]


def module_calls_exe_and_keeps_its_names_private():
    # A module at 0x800A0000 calls exe function e (recompiled elsewhere: only declared here) and records its signature
    m, e = 0x800A0000, 0x80020000
    words = [0x0C000000 | (e & 0x3FFFFFF) >> 2, NOP, JR_RA, NOP]
    image = b"".join(w.to_bytes(4, "little") for w in words) + bytes(64)
    c = recompile(image, m, {m: "func_800A0000", e: "e"}, set(), {}, tag="mod")
    assert "static void r_800A0000(void)" in c and "\nvoid r_80020000(void);" in c
    assert "void r_80020000(void)\n{" not in c and "static int func_800A0000(" in c
    assert f"const Module mod_module = {{{m:#010x}u, {{{', '.join(str(b) for b in image[:64])}}}" in c


def code_pointers_found_whatever_splat_typed():
    # A table splat cut at a label inside it comes out as bytes: its function addresses are still code pointers.
    # Only words inside a data file's range count, and only exact function starts.
    g = BASE + 0x40
    image = bytearray(0x100)
    image[0x80:0x84] = g.to_bytes(4, "little")  # in the data file's range, as .byte lines
    image[0x84:0x88] = (g + 4).to_bytes(4, "little")  # inside g, not its start
    image[0xC0:0xC4] = g.to_bytes(4, "little")  # past the data file
    data_asm = "".join(f"    /* {a - BASE:X} {a:08X} */ .byte 0x{image[a - BASE]:02X}\n" for a in range(BASE + 0x80, BASE + 0x88))
    assert code_pointers(bytes(image), BASE, [data_asm], {BASE: "f", g: "g"}) == [(BASE + 0x80, "g")]


if __name__ == "__main__":
    for test in (delay_slot_runs_on_both_paths, branch_reads_condition_before_slot, jr_ra_returns,
                 div_by_zero_values, unaligned_pair_order, backward_branch_polls, bltzal_links,
                 mult_is_signed_64_bit, function_address_becomes_native,
                 module_call_goes_indirect, function_address_kept_for_compares,
                 stored_function_address_becomes_native, module_calls_exe_and_keeps_its_names_private,
                 setjmp_return_is_an_entry, address_stored_in_return_slot_becomes_native,
                 code_pointers_found_whatever_splat_typed):
        test()
    print("ok")
