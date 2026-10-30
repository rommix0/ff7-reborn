"""MIPS R3000 code to C for the PC port (docs/pc_port.md): one C function per game function, over pc/recomp.h's Ctx.

usage: python tools/recomp.py --image FILE [--base ADDR] --symbols NM_LIST [--native NM_LIST] [--asm DIR]
                              [--only NAME,...] -o OUT.c

FILE is a PS-X EXE (its header gives the load address) or a raw module loaded at --base. NM_LIST is `nm -n` output:
its T/t symbols are the functions, and each ends where the next begins. --native lists the functions the PC build has
in C, which aren't recompiled. --asm is splat's asm folder, whose jtbl_ blocks give the jump tables; for a module it
also gives the functions (its glabels), and NM_LIST the exe's, which the module calls. The output is a
translation of the game's code: generate it from the user's own files and never commit it.

Each instruction becomes C in order. A branch reads its condition, runs its delay slot, then jumps; a backward branch
calls hw_poll() first, so interrupts get in. R3000 load-delay slots and add/addi overflow traps aren't emulated:
GCC's code never depends on them.
"""
import argparse
import re
from pathlib import Path

import rabbitizer

rabbitizer.config.pseudos_enablePseudos = False

# Names a thunk mustn't take, since the host C library has them: the game's own libc (printf, memcpy) and the host's
# helpers (MinGW's main calls __main, and the game has one). Those thunks get a psx_ prefix.
HOST = set("""printf sprintf vsprintf puts putchar getchar gets memcpy memset memmove memcmp memchr strlen strcpy strncpy
strcat strncat strcmp strncmp strchr strrchr strstr strtok strtol strtoul atoi atol atof abs labs rand srand malloc free
calloc realloc exit abort qsort bsearch toupper tolower isalpha isdigit isspace isupper islower isxdigit isalnum ispunct
isprint iscntrl isgraph setjmp longjmp bcopy bzero bcmp index rindex open close read write lseek ioctl main""".split())
ARGS = ("(int)ctx.r[4], (int)ctx.r[5], (int)ctx.r[6], (int)ctx.r[7], (int)mem_r32(ctx.r[29] + 0x10u), "
        "(int)mem_r32(ctx.r[29] + 0x14u), (int)mem_r32(ctx.r[29] + 0x18u), (int)mem_r32(ctx.r[29] + 0x1Cu)")


MODULES = 0x800A0000  # code from here up is loaded at run time (field, battle, ending...)


def thunk(name):
    return f"psx_{name}" if name.startswith("_") or name in HOST else name


def reg(n):
    return "0u" if n == 0 else f"ctx.r[{n}]"


def put(n, expr):
    # A write to $zero is dropped, but a load from I/O still has to happen.
    return f"(void)({expr});" if n == 0 else f"ctx.r[{n}] = {expr};"


def recompile(image: bytes, base: int, funcs: dict[int, str], native: set[str], jtbls: dict[int, list[int]],
              emit: set[int] | None = None, tag: str | None = None, data_words=(), prefix: str = "") -> str:
    """C for every function in funcs (address -> name) that isn't native, or only those in emit.

    Each gets r_<ADDR> for recompiled callers and a thunk under its own name for C. With a tag, also dispatch.c's
    tables: <tag>_code (address -> native function) and <tag>_patches (from data_words, (RAM address, function
    name) pairs: the game's data words that hold code addresses)."""
    starts = sorted(funcs)
    end_of = {a: (starts[i + 1] if i + 1 < len(starts) else base + len(image)) for i, a in enumerate(starts)}
    native_at = {a for a in starts if funcs[a] in native}
    own = [a for a in starts if base <= a < base + len(image)]  # the rest (a module's calls into the exe) are linked
    chosen = [a for a in own if a not in native_at and (emit is None or a in emit)]
    names = {a: funcs[a] if a in native_at else prefix + thunk(funcs[a]) for a in starts}
    # Modules share addresses, so their functions stay inside their own file.
    local = set(own) if base >= MODULES else set()
    scope = {a: "static " if a in local else "" for a in starts}
    # A longjmp lands just after its setjmp's jal, on the frame setjmp saw: that point is entered as itself, the
    # function's body again from that label (libetc's interrupt entry is one).
    setjmp_at = {a for a in starts if funcs[a] == "setjmp"}
    resumes = {a: [p + 8 for p in range(a, end_of[a], 4) if image[p - base + 3] >> 2 == 3
                   and (0x80000000 | (int.from_bytes(image[p - base:p - base + 4], "little") & 0x3FFFFFF) << 2)
                   in setjmp_at] for a in chosen}
    out = ['#include "recomp.h"\n\n']
    out += [f"{scope[a]}void r_{a:08X}(void);\n" for a in starts if a not in native_at]
    out += [f"{scope[a]}void r_{e:08X}(void);\n" for a in chosen for e in resumes[a]]
    out += [f"{scope[a]}int {names[a]}(int, int, int, int, int, int, int, int);\n" for a in starts]
    for a in chosen:
        for e in [None] + resumes[a]:
            out.append("\n" + scope[a] + function(image, base, a, end_of[a], funcs, native_at, names,
                                                  jtbls.get(a, []), e))
        out.append(f"""
{scope[a]}int {names[a]}(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{{
    unsigned int sp = ctx.r[29];

    ctx.r[29] = sp - 0x20;
    mem_w32(sp - 0x10, a4);
    mem_w32(sp - 0xC, a5);
    mem_w32(sp - 0x8, a6);
    mem_w32(sp - 0x4, a7);
    ctx.r[4] = a0;
    ctx.r[5] = a1;
    ctx.r[6] = a2;
    ctx.r[7] = a3;
    r_{a:08X}();
    ctx.r[29] = sp;
    return ctx.r[2];
}}
""")
    if tag:
        linked = set(chosen) | (native_at & set(own))
        # A resume entry takes no arguments: it runs on the registers the BIOS put back.
        entries = sorted([(a, names[a]) for a in linked] + [(e, f"r_{e:08X}") for a in chosen for e in resumes[a]])
        code = [f"    {{{a:#010x}u, (void *){name}}},\n" for a, name in entries]
        by_name = {n: a for a, n in funcs.items()}
        patches = [f"    {{{addr:#010x}u, (void *){names[by_name[n]]}, {by_name[n]:#010x}u}},\n" for addr, n in data_words
                   if n in by_name and (by_name[n] in linked or by_name[n] not in own)]
        for label, rows in (("code", code), ("patches", patches)):
            out.append(f"\nconst CodePatch {tag}_{label}[] = {{\n{''.join(rows) or '    {0, 0},\n'}}};\n")
            out.append(f"const int {tag}_{'code' if label == 'code' else 'patch'}_count = {len(rows)};\n")
        if base >= MODULES:
            first = own[0] - base
            signature = ", ".join(str(b) for b in image[first:first + 64])
            out.append(f"\nconst Module {tag}_module = {{{own[0]:#010x}u, {{{signature}}}, {tag}_code, {tag}_patches, "
                       f"{len(code)}, {len(patches)}}};\n")
    return "".join(out)


def code_pointers(image: bytes, base: int, data_asm: list[str], funcs: dict[int, str]) -> list[tuple[int, str]]:
    """The data's words that hold a function's address, as (address, function name), whatever type splat gave them:
    a table it cut at a label inside it comes out as bytes. Each data file's address range is scanned word by word."""
    found = []
    for text in data_asm:
        addrs = [int(a, 16) for a in re.findall(r"/\* \w+ ([0-9A-F]{8}) ", text)]
        if not addrs:
            continue
        lo, hi = max(min(addrs), base), min(max(addrs) + 4, base + len(image))
        for a in range(lo + 3 & ~3, hi - 3, 4):
            word = int.from_bytes(image[a - base:a - base + 4], "little")
            if word in funcs:
                found.append((a, funcs[word]))
    return found


def function(image, base, start, end, funcs, native_at, names, table, entry=None):
    words = [int.from_bytes(image[p - base:p - base + 4], "little") for p in range(start, end, 4)]
    instrs = [rabbitizer.Instruction(w, vram=start + 4 * i) for i, w in enumerate(words)]
    inside = range(start, end)
    labels = {t for t in table if t in inside} | ({entry} if entry else set())
    for i, ins in enumerate(instrs):
        if ins.isBranch() or ins.getOpcodeName() == "j":
            target = ins.getBranchVramGeneric() if ins.isBranch() else ins.getInstrIndexAsVram()
            if target in inside:
                labels.add(target)
    # A delay slot that is itself a branch target is emitted twice: once with its branch and once on its own.
    for i, ins in enumerate(instrs):
        if ins.hasDelaySlot() and start + 4 * (i + 1) in labels:
            labels.add(start + 4 * (i + 2))

    # A lui/addiu (or ori) pair that builds a function's address gives its native address instead: the PC build's
    # code pointers are all native, so C can call what the game stores.
    function_at = {}
    luis = {}
    for i, (ins, raw) in enumerate(zip(instrs, words)):
        if start + 4 * i in labels:
            luis = {}
        op, rs, rt = raw >> 26, raw >> 21 & 31, raw >> 16 & 31
        if op in (0x09, 0x0D) and rs in luis:
            value = (luis[rs] + (raw & 0xFFFF) - (0x10000 if op == 0x09 and raw & 0x8000 else 0)) & 0xFFFFFFFF \
                if op == 0x09 else luis[rs] | raw & 0xFFFF
            if value in funcs:
                function_at[i] = value
        for written in ([raw >> 11 & 31] if ins.modifiesRd() else []) + ([rt] if ins.modifiesRt() else []):
            luis.pop(written, None)
        if op == 0x0F:
            luis[rt] = (raw & 0xFFFF) << 16

    def escapes(i, reg):
        """Whether the address built into reg leaves as a code pointer: stored, passed to a call, returned or called.
        Anything else (a loop's end, a compare) keeps the game's address."""
        if i and instrs[i - 1].getOpcodeName() in ("jal", "jalr") and 4 <= reg <= 7:
            return True  # built in a call's delay slot: an argument
        for k, (ins, raw) in enumerate(zip(instrs[i + 1:i + 24], words[i + 1:i + 24]), i + 1):
            name, rs, rt = ins.getOpcodeName(), raw >> 21 & 31, raw >> 16 & 31
            if name == "sw" and rt == reg or name in ("jalr", "jr") and rs == reg:
                return True
            if name in ("jal", "jalr", "j") and 4 <= reg <= 7:
                return True
            if name == "jr":  # returning: in v0/v1, or stored by the delay slot
                slot = words[k + 1] if k + 1 < len(words) else 0
                return reg in (2, 3) or slot >> 26 == 0x2B and slot >> 16 & 31 == reg
            if ins.readsRs() and rs == reg or ins.readsRt() and rt == reg:
                return False
            if ins.modifiesRd() and raw >> 11 & 31 == reg or ins.modifiesRt() and rt == reg:
                return False
        return False

    function_at = {i: v for i, v in function_at.items() if escapes(i, words[i] >> 16 & 31)}

    def jump(target, pc):
        if target in inside:
            return ("hw_poll(); " if target <= pc else "") + f"goto L_{target:08X};"
        return call(target) + " return;"

    def call(target):
        if target in native_at:
            return f"ctx.r[2] = (unsigned int){names[target]}({ARGS});"
        if target in funcs:
            return f"r_{target:08X}();"
        if target >= MODULES:  # a module loaded at run time: dispatch.c finds it
            return f"call_indirect({target:#010x});"
        return f"recomp_missing({target:#010x});"

    lines = [f"void r_{start:08X}(void) /* {funcs[start]} */\n{{\n" if not entry else
             f"void r_{entry:08X}(void) /* {funcs[start]}, from its setjmp */\n{{\n    goto L_{entry:08X};\n"]
    i = 0
    while i < len(instrs):
        pc = start + 4 * i
        if pc in labels:
            lines.append(f"L_{pc:08X}:;\n")
        ins = instrs[i]
        if not ins.hasDelaySlot():
            if i in function_at:
                lines.append(f"    {put(words[i] >> 16 & 31, f'(unsigned int){names[function_at[i]]}')}\n")
            else:
                lines.append(f"    {simple(ins, words[i], pc)}\n")
            i += 1
            continue
        slot = ""
        if i + 1 < len(instrs):
            slot = (put(words[i + 1] >> 16 & 31, f"(unsigned int){names[function_at[i + 1]]}") if i + 1 in function_at
                    else simple(instrs[i + 1], words[i + 1], pc + 4))
        lines.append(f"    {{ {control(ins, words[i], pc, slot, jump, call, table, inside)} }}\n")
        if pc + 4 in labels:
            lines.append(f"    goto L_{pc + 8:08X};\nL_{pc + 4:08X}:;\n    {slot}\n")
        i += 2
    lines.append("}\n")
    return "".join(lines)


def control(ins, raw, pc, slot, jump, call, table, inside):
    """A branch or jump with its delay slot already translated."""
    name, rs, rt = ins.getOpcodeName(), raw >> 21 & 31, raw >> 16 & 31
    link = f"ctx.r[31] = {pc + 8:#010x}u; " if name in ("bltzal", "bgezal", "jal") else ""
    cond = {
        "beq": f"{reg(rs)} == {reg(rt)}", "bne": f"{reg(rs)} != {reg(rt)}",
        "blez": f"(int){reg(rs)} <= 0", "bgtz": f"(int){reg(rs)} > 0",
        "bltz": f"(int){reg(rs)} < 0", "bgez": f"(int){reg(rs)} >= 0",
        "bltzal": f"(int){reg(rs)} < 0", "bgezal": f"(int){reg(rs)} >= 0",
    }.get(name)
    if cond:
        return f"int c = {cond}; {link}{slot} if (c) {{ {jump(ins.getBranchVramGeneric(), pc)} }}"
    if name == "j":
        return f"{slot} {jump(ins.getInstrIndexAsVram(), pc)}"
    if name == "jal":
        return f"{link}{slot} {call(ins.getInstrIndexAsVram())}"
    if name == "jalr":
        rd = raw >> 11 & 31
        return f"unsigned int t = {reg(rs)}; {put(rd, f'{pc + 8:#010x}u') if rd else ''} {slot} call_indirect(t);"
    if name == "jr" and rs == 31:
        return f"{slot} return;"
    if name == "jr":
        cases = "".join(f"case {t:#010x}u: goto L_{t:08X}; " for t in sorted(set(table)) if t in inside)
        return f"unsigned int t = {reg(rs)}; {slot} switch (t) {{ {cases}default: call_indirect(t); return; }}"
    raise ValueError(f"{pc:08X}: no translation for {name}")


def simple(ins, raw, pc):
    """Any instruction without a delay slot."""
    name, rs, rt, rd, sa = ins.getOpcodeName(), raw >> 21 & 31, raw >> 16 & 31, raw >> 11 & 31, raw >> 6 & 31
    imm = raw & 0xFFFF
    simm = imm - 0x10000 if imm & 0x8000 else imm
    addr = f"{reg(rs)} + {simm & 0xFFFFFFFF:#x}u"
    op = raw >> 26
    if op == 0x12:  # COP2: the GTE
        sub, cd = raw >> 21 & 31, raw >> 11 & 31
        if raw & 1 << 25:
            return f"gte_cmd({raw & 0x1FFFFFF:#x});"
        return {0: put(rt, f"gte_read({cd})"), 2: put(rt, f"gte_read_ctrl({cd})"),
                4: f"gte_write({cd}, {reg(rt)});", 6: f"gte_write_ctrl({cd}, {reg(rt)});"}[sub]
    if op == 0x32:
        return f"gte_write({rt}, mem_r32({addr}));"
    if op == 0x3A:
        return f"mem_w32({addr}, gte_read({rt}));"
    if op == 0x10:  # COP0
        sub, cd = raw >> 21 & 31, raw >> 11 & 31
        if sub == 0:
            return put(rt, f"cop0[{cd}]")
        if sub == 4:
            return f"cop0[{cd}] = {reg(rt)};"
        if raw & 0x3F == 0x10:  # rfe: pop the interrupt-enable/mode stack
            return "cop0[12] = (cop0[12] & ~0xFu) | (cop0[12] >> 2 & 0xFu);"
    a, b = reg(rs), reg(rt)
    match name:
        case "nop":
            return ";"
        case "sll":
            return put(rd, f"{b} << {sa}")
        case "srl":
            return put(rd, f"{b} >> {sa}")
        case "sra":
            return put(rd, f"(unsigned int)((int){b} >> {sa})")
        case "sllv":
            return put(rd, f"{b} << ({a} & 31)")
        case "srlv":
            return put(rd, f"{b} >> ({a} & 31)")
        case "srav":
            return put(rd, f"(unsigned int)((int){b} >> ({a} & 31))")
        case "add" | "addu":
            return put(rd, f"{a} + {b}")
        case "sub" | "subu":
            return put(rd, f"{a} - {b}")
        case "and":
            return put(rd, f"{a} & {b}")
        case "or":
            return put(rd, f"{a} | {b}")
        case "xor":
            return put(rd, f"{a} ^ {b}")
        case "nor":
            return put(rd, f"~({a} | {b})")
        case "slt":
            return put(rd, f"(int){a} < (int){b}")
        case "sltu":
            return put(rd, f"{a} < {b}")
        case "addi" | "addiu":
            return put(rt, f"{a} + {simm & 0xFFFFFFFF:#x}u")
        case "slti":
            return put(rt, f"(int){a} < {simm}")
        case "sltiu":
            return put(rt, f"{a} < {simm & 0xFFFFFFFF:#x}u")
        case "andi":
            return put(rt, f"{a} & {imm:#x}u")
        case "ori":
            return put(rt, f"{a} | {imm:#x}u")
        case "xori":
            return put(rt, f"{a} ^ {imm:#x}u")
        case "lui":
            return put(rt, f"{imm << 16:#010x}u")
        case "mfhi":
            return put(rd, "ctx.hi")
        case "mflo":
            return put(rd, "ctx.lo")
        case "mthi":
            return f"ctx.hi = {a};"
        case "mtlo":
            return f"ctx.lo = {a};"
        case "mult":
            return (f"{{ long long p = (long long)(int){a} * (int){b}; "
                    "ctx.lo = (unsigned int)p; ctx.hi = (unsigned int)(p >> 32); }")
        case "multu":
            return (f"{{ unsigned long long p = (unsigned long long){a} * {b}; "
                    "ctx.lo = (unsigned int)p; ctx.hi = (unsigned int)(p >> 32); }")
        case "div":
            return f"op_div((int){a}, (int){b});"
        case "divu":
            return f"op_divu({a}, {b});"
        case "lb":
            return put(rt, f"(unsigned int)(signed char)mem_r8({addr})")
        case "lbu":
            return put(rt, f"mem_r8({addr})")
        case "lh":
            return put(rt, f"(unsigned int)(short)mem_r16({addr})")
        case "lhu":
            return put(rt, f"mem_r16({addr})")
        case "lw":
            return put(rt, f"mem_r32({addr})")
        case "lwl":
            return put(rt, f"op_lwl({b}, {addr})")
        case "lwr":
            return put(rt, f"op_lwr({b}, {addr})")
        case "sb":
            return f"mem_w8({addr}, {b});"
        case "sh":
            return f"mem_w16({addr}, {b});"
        case "sw":
            return f"mem_w32({addr}, {b});"
        case "swl":
            return f"op_swl({b}, {addr});"
        case "swr":
            return f"op_swr({b}, {addr});"
        case "syscall":
            return f"recomp_syscall({raw >> 6 & 0xFFFFF:#x});"
        case "break":
            return f"recomp_break({raw >> 6 & 0xFFFFF:#x});"
    # Data in the text section (hand-written code keeps words there): reaching it is a bug.
    return f"recomp_missing({pc:#010x}); /* {raw:08X} */"


# Not functions: GCC's per-file markers and splat's segment bounds, which share addresses with real functions.
MARKER = re.compile(r"gcc2_compiled\.|__gnu_compiled_c|\w+_(VRAM|START|END|SIZE)")


def read_symbols(path, lo, hi):
    funcs = {}
    for line in Path(path).read_text().splitlines():
        parts = line.split()
        if (len(parts) == 3 and parts[1] in "Tt" and lo <= int(parts[0], 16) < hi
                and not MARKER.fullmatch(parts[2])):
            funcs.setdefault(int(parts[0], 16), parts[2])
    return funcs


def read_jump_tables(asm, funcs):
    starts = sorted(funcs)
    tables = {}
    for path in Path(asm).rglob("*.s"):
        inside = False
        for line in path.read_text().splitlines():
            if line.startswith("dlabel jtbl_"):
                inside = True
            elif line.startswith("enddlabel"):
                inside = False
            elif inside and (m := re.search(r"\.word \.L([0-9A-F]{8})", line)):
                target = int(m[1], 16)
                owner = max((s for s in starts if s <= target), default=None)
                if owner is not None:
                    tables.setdefault(owner, []).append(target)
    return tables


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--image", type=Path, required=True)
    ap.add_argument("--base", type=lambda s: int(s, 16))
    ap.add_argument("--symbols", type=Path, required=True)
    ap.add_argument("--native", type=Path)
    ap.add_argument("--asm", type=Path)
    ap.add_argument("--only")
    ap.add_argument("--tag", help="also write the code table and patch list as <tag>_code and <tag>_patches")
    ap.add_argument("--thunk-prefix", default="", help="for tests that link the C version of the same functions")
    ap.add_argument("--rename", action="append", default=[], metavar="GAME=PC",
                    help="a function the PC build compiles under another name (main=game_main)")
    ap.add_argument("-o", "--out", type=Path, required=True)
    args = ap.parse_args()
    data = args.image.read_bytes()
    base = args.base
    image, image_base = data, base  # all of it, for the code pointers in its data
    # splat's segment bounds keep the rodata before the exe's code and the data after it out of the function list.
    bounds = {parts[2]: int(parts[0], 16) for parts in map(str.split, args.symbols.read_text().splitlines())
              if len(parts) == 3}
    lo, hi = bounds["main_TEXT_START"], bounds["main_TEXT_END"]
    funcs = read_symbols(args.symbols, lo, hi)
    if data.startswith(b"PS-X EXE"):
        base, size = int.from_bytes(data[0x18:0x1C], "little"), int.from_bytes(data[0x1C:0x20], "little")
        image, image_base = data[0x800:], base
        data, base = data[0x800 + lo - base:0x800 + hi - base], lo
    else:
        # A module, which has no ELF: its functions are splat's glabels and its code ends at the last instruction.
        # It calls the exe's functions by the exe's symbols.
        code = "".join(p.read_text() for p in args.asm.rglob("*.s") if "data" not in p.parts)
        funcs.update((int(m[2], 16), m[1]) for m in re.finditer(r"^glabel (\w+)\n\s*/\* \w+ ([0-9A-F]{8}) ", code, re.M))
        end = max(int(a, 16) for a in re.findall(r"/\* \w+ ([0-9A-F]{8}) [0-9A-F]{8} \*/ ", code)) + 4
        data = data[:end - base]
    renames = dict(r.split("=") for r in args.rename)
    funcs = {a: renames.get(n, n) for a, n in funcs.items()}
    # Anything the PC build defines, code or data, isn't recompiled.
    native = set()
    if args.native:
        native = {line.split()[-1].lstrip("_") for line in args.native.read_text().splitlines()
                  if len(line.split()) == 3 and line.split()[1] in "TDBCR"}
    jtbls = read_jump_tables(args.asm, funcs) if args.asm else {}
    emit = None
    if args.only:
        names = set(args.only.split(","))
        emit = {a for a, n in funcs.items() if n in names}
    data_words = []
    if args.asm:
        # The exe's asm folder holds the modules' too: each patches only its own data.
        data_asm = [p.read_text() for p in sorted(args.asm.rglob("*.s")) if "data" in p.parts]
        data_words = [(a, n) for a, n in code_pointers(image, image_base, data_asm, funcs)
                      if (a >= MODULES) == (base >= MODULES)]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(recompile(data, base, funcs, native, jtbls, emit, args.tag, data_words, args.thunk_prefix))


if __name__ == "__main__":
    main()
