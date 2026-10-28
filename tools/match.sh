#!/bin/bash
# Compile one C file with the period toolchain and compare the code to the game exe at an address.
# usage: tools/match.sh src/main/bitops.c 80014A00 [gcc-version]   (CFLAGS / EXE env vars override defaults)
set -eu
cd "$(dirname "$0")/.."
src=$1 org=$2 gcc=${3:-2.7.2}
exe=${EXE:-"assets/Final Fantasy VII (USA) (Disc 1)/SCUS_941.63"}
w=build/match
mkdir -p $w

toolchain/gcc-$gcc/cpp -P -undef -D__GNUC__=2 -Iinclude "$src" > $w/f.i
toolchain/gcc-$gcc/cc1 -quiet ${CFLAGS:--O2 -G0} $w/f.i -o $w/f.s
python3 toolchain/maspsx/maspsx.py --aspsx-version=2.79 < $w/f.s > $w/f.ms
mipsel-linux-gnu-as -EL -march=r3000 -mtune=r3000 -no-pad-sections -O1 -G0 $w/f.ms -o $w/f.o
# ld aligns .text to 16 bytes by default; most game functions aren't, so pin the address exactly.
echo "SECTIONS { .text 0x$org : SUBALIGN(4) { *(.text) } }" > $w/f.ld
mipsel-linux-gnu-ld -EL -T $w/f.ld -e 0 -o $w/f.elf $w/f.o
mipsel-linux-gnu-objcopy -O binary -j .text $w/f.elf $w/ours.bin

size=$(stat -c %s $w/ours.bin)
# Main exe: 0x800-byte PS-X EXE header, then text loaded at 0x80010000.
tail -c +$((0x$org - 0x80010000 + 0x800 + 1)) "$exe" | head -c "$size" > $w/game.bin
if cmp -s $w/ours.bin $w/game.bin; then
    echo "MATCH $src @ $org ($size bytes)"
    exit 0
fi
dis() { mipsel-linux-gnu-objdump -D -b binary -m mips:3000 -EL --adjust-vma=0x$org "$1" | tail -n +8; }
diff <(dis $w/game.bin) <(dis $w/ours.bin) | sed 's/^</game/; s/^>/ours/'
exit 1
