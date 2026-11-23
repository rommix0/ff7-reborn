"""Align a function's jump tables in GCC's assembly as CHOCOBO.BIN's assembler did, in place.

usage: python tools/jtbl_align.py FILE.s

GCC 2.7.2 puts each jump table under `.align 3`. In CHOCOBO.BIN a function's first table only sits on a word
(jtbl_800A0034 is 4 mod 8), and the tables after it are 8-aligned from the first (sub_800ABABC's second table has a
zero word before it): the game's assembler aligned them within the function's own read-only data.
"""
import sys
from pathlib import Path

SIZES = {'.word': 4, '.half': 2, '.byte': 1}


def size(line: str) -> int:
    """The bytes one directive line of read-only data takes."""
    op, _, arg = line.strip().partition('\t')
    if op in SIZES:
        return SIZES[op] * len(arg.split(','))
    if op == '.space':
        return int(arg, 0)
    if op == '.ascii':
        return len(arg.strip('"').encode().decode('unicode_escape'))
    raise ValueError(f'unsized read-only data: {line!r}')


def align(lines: list[str]) -> list[str]:
    out = []
    off = None  # the offset in the current function's read-only data from its first jump table
    rdata = False
    for line in lines:
        s = line.strip()
        if s.startswith('.ent'):
            off = None
        if s == '.rdata':
            rdata = True
        elif s in ('.text', '.data', '.bss') or s.startswith('.section'):
            rdata = False
        elif rdata and s == '.align\t3':
            if off is None:
                off = 0
            pad = -off % 8
            out.append('\t.align\t2')
            out += ['\t.word\t0'] * (pad // 4)
            off += pad
            continue
        elif rdata and off is not None and s.startswith('.align'):
            off += -off % (1 << int(s.split('\t')[1]))
        elif rdata and off is not None and s and not s.endswith(':') and not s.startswith('#'):
            off += size(line)
        out.append(line)
    return out


def main():
    p = Path(sys.argv[1])
    p.write_bytes('\n'.join(align(p.read_bytes().decode().split('\n'))).encode())


if __name__ == '__main__':
    main()
