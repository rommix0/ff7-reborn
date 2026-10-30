"""Linker glue for the PC port: places every game symbol the objects use but don't define at its game address.

usage: python tools/pc_syms.py OUT.ld "CC [LDFLAGS]" OBJ... [--nm NM] [--stubs OUT.c]

In the 32-bit PC build the PS1's RAM sits at its own address, so a game variable needs no translation: the linker just
needs to know where it is. Names come from config/symbols.main.txt and config/symbols.main.psyq.txt, or from splat's
D_XXXXXXXX / func_XXXXXXXX form. 32-bit MinGW objects prefix C names with an underscore that the linker leaves out
of its messages, so each symbol is written both ways; only the one in use matters.

Only what a trial link leaves undefined gets an address: the game's libc names (printf, memcpy) and the host's
(MinGW's main calls __main, which the game also has) must stay the host's. With --nm, the objects' common symbols
(the -G8 files' tentative definitions) are placed too. With --stubs, a function at 0x800A0000 or above, which lives
in a module loaded at run time, gets a C stub that finds it there (module_call in pc/dispatch.c).
"""
import argparse
import re
import shlex
import subprocess
from pathlib import Path

MODULES = 0x800A0000


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", type=Path)
    ap.add_argument("cc")
    ap.add_argument("objs", nargs="+")
    ap.add_argument("--nm")
    ap.add_argument("--stubs", type=Path)
    args = ap.parse_args()
    addrs = {}
    for path in ("config/symbols.main.psyq.txt", "config/symbols.main.txt"):
        for line in Path(path).read_text().splitlines():
            if m := re.match(r"(\w+) = (0x[0-9A-Fa-f]+);", line):
                addrs[m[1]] = int(m[2], 16)

    def address(name):
        if m := re.fullmatch(r"(?:D|func)_([0-9A-F]{8})", name):
            return int(m[1], 16)
        return addrs.get(name)

    # A relative output path: from WSL, a Windows linker can't write to a /mnt/... path.
    trial_out = args.out.with_suffix(".trial")
    trial = subprocess.run([*shlex.split(args.cc), *args.objs, "-o", str(trial_out)], capture_output=True, text=True)
    for leftover in args.out.parent.glob(trial_out.name + "*"):
        leftover.unlink()
    wanted = set(re.findall(r"undefined reference to `([^']+)'", trial.stdout + trial.stderr))
    if args.nm:
        for line in subprocess.run([args.nm, *args.objs], capture_output=True, text=True).stdout.splitlines():
            parts = line.split()
            if len(parts) == 3 and parts[1] == "C":
                wanted.add(parts[2][1:] if parts[2].startswith("_") and address(parts[2][1:]) else parts[2])
    lines, modules = [], []
    for name in sorted(wanted):
        addr = address(name)
        if addr is None:
            continue
        if addr >= MODULES and name.startswith("func_"):
            modules.append((name, addr))
        else:
            lines.append(f"{name} = {addr:#010x};\n_{name} = {addr:#010x};\n")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("".join(lines))
    if args.stubs:
        args.stubs.parent.mkdir(parents=True, exist_ok=True)
        args.stubs.write_text("int module_call(unsigned int addr, int a0, int a1, int a2, int a3, int a4, int a5, int a6,"
                              " int a7);\n" + "".join(
                                  f"int {n}(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7)\n"
                                  f"{{\n    return module_call({a:#010x}, a0, a1, a2, a3, a4, a5, a6, a7);\n}}\n"
                                  for n, a in modules))


if __name__ == "__main__":
    main()
