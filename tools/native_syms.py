"""Link glue for the native sound driver (native/snd_native.c), from config/symbols.main.txt and the native objects.

usage: python tools/native_syms.py OUT_DIR DRIVER.o... -- OTHER.o...

Writes OUT_DIR/ram.ld, which puts every game data symbol the objects need but don't define at its game address in
snd_ram, and OUT_DIR/funcs.c, the game address of each driver function (for the game's handler tables).
"""
import re
import subprocess
import sys
from pathlib import Path

RAM_BASE = 0x80000000


def main():
    out = Path(sys.argv[1])
    split = sys.argv.index("--")
    driver, others = sys.argv[2:split], sys.argv[split + 1:]
    addrs, funcs = {}, set()
    for line in Path("config/symbols.main.txt").read_text().splitlines():
        if m := re.match(r"(\w+) = (0x[0-9A-Fa-f]+);", line):
            addrs[m[1]] = int(m[2], 16)
            if "type:func" in line:
                funcs.add(m[1])

    def address(name):
        if m := re.fullmatch(r"(?:D|func)_([0-9A-F]{8})", name):
            return int(m[1], 16)
        return addrs.get(name)

    defined, wanted, driver_funcs = set(), set(), []
    for obj in driver + others:
        for line in subprocess.run(["nm", obj], capture_output=True, text=True, check=True).stdout.splitlines():
            kind, name = line.split()[-2:]
            if kind in "UC":
                wanted.add(name)
            else:
                defined.add(name)
                if kind == "T" and obj in driver:
                    driver_funcs.append(name)
    data = sorted(n for n in wanted - defined if address(n) is not None and n not in funcs)
    (out / "ram.ld").write_text("".join(f"{n} = snd_ram + {address(n) - RAM_BASE:#x};\n" for n in data))
    (out / "funcs.c").write_text(
        "".join(f"extern char {n}[];\n" for n in driver_funcs)
        + "const struct { unsigned int addr; void *func; } game_funcs[] = {\n"
        + "".join(f"    {{{address(n):#x}, {n}}},\n" for n in driver_funcs)
        + f"}};\nconst int game_func_count = {len(driver_funcs)};\n")


if __name__ == "__main__":
    main()
