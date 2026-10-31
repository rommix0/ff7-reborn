"""What one change to the lab's input did to RAM: the byte runs that differ between two dumps (pc/lab.c, --dump-at).

usage: python tools/lab_diff.py BASE.bin OTHER.bin [--noise SAME_INPUT.bin] [--gap N]

BASE and OTHER come from runs that differ only in their --input script, dumped at the same frame. The PC build is
deterministic but for a few words (host addresses the game keeps), so a third dump from a run with BASE's own input
names those, and they're left out. Runs closer than --gap bytes print as one.
"""
import argparse
from pathlib import Path

RAM = 0x80000000


def diff_runs(base: bytes, other: bytes, noise: bytes | None = None, gap: int = 4) -> list[tuple[int, int]]:
    """(offset, length) of each run of differing bytes, without the ones that differ in noise too."""
    runs = []
    for i, (a, b) in enumerate(zip(base, other)):
        if a == b or noise and noise[i] != a:
            continue
        if runs and i - (runs[-1][0] + runs[-1][1]) < gap:
            runs[-1] = (runs[-1][0], i + 1 - runs[-1][0])
        else:
            runs.append((i, 1))
    return runs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("base", type=Path)
    ap.add_argument("other", type=Path)
    ap.add_argument("--noise", type=Path)
    ap.add_argument("--gap", type=int, default=4)
    args = ap.parse_args()
    base, other = args.base.read_bytes(), args.other.read_bytes()
    noise = args.noise.read_bytes() if args.noise else None
    for at, n in diff_runs(base, other, noise, args.gap):
        print(f"{RAM + at:08X} {n:5}  {base[at:at + min(n, 16)].hex(' ')}  ->  {other[at:at + min(n, 16)].hex(' ')}")


if __name__ == "__main__":
    main()
