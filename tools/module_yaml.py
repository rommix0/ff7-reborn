"""The splat config for a module the PC port recompiles (docs/pc_port.md), on stdout.

usage: python tools/module_yaml.py FILE NAME VRAM

FILE is the module as the game loads it (raw, at VRAM). Modules lay out as read-only data, code, then data: the code
runs from the first function (its stack-frame setup, and any instructions GCC put before it) to the last `jr $ra`'s
delay slot. Its asm goes to asm/NAME, and calls
into the exe take their names from the exe's symbol files.
"""
import argparse
import hashlib
from pathlib import Path

import rabbitizer

PROLOGUE = 0x27BD  # addiu $sp, $sp, -n
JR_RA = 0x03E00008
# The R3000's primary opcodes, with the GTE's cop2, lwc2 and swc2: rabbitizer still decodes R4000 ones like daddi.
R3000_OPCODES = {*range(0x00, 0x11), 0x12, *range(0x20, 0x27), *range(0x28, 0x2C), 0x2E, 0x32, 0x3A}


def is_code(word: int, before: int) -> bool:
    """Whether a word before the first stack-frame setup can be code too, given the word before it: a module may open
    with a leaf function (an effect file's entry), and GCC may put a load ahead of the setup. A zero is a nop only in a
    delay slot; pointers and words GCC never emits (R4000 opcodes, writes to $zero, shifts of $zero, jumps out of RAM)
    are the header or data before the code."""
    if not word:
        prev = rabbitizer.Instruction(before)
        return prev.hasDelaySlot() or prev.doesLoad()
    ins = rabbitizer.Instruction(word)
    shifts_zero = ins.getOpcodeName() in ("sll", "srl", "sra") and not word >> 16 & 0x1F
    jumps_out = ins.isJumpWithAddress() and (word & 0x3FFFFFF) << 2 >= 0x200000
    return not 0x80000000 <= word < 0x80200000 and word >> 26 in R3000_OPCODES and ins.isValid() \
        and not ins.outputsToGprZero() and not shifts_zero and not jumps_out


def code_bounds(image: bytes) -> tuple[int, int]:
    """Where the code starts and ends in a module."""
    words = [int.from_bytes(image[i:i + 4], "little") for i in range(0, len(image) & ~3, 4)]
    start = next(i for i, w in enumerate(words) if w >> 16 == PROLOGUE and w & 0x8000)
    while start and is_code(words[start - 1], words[start - 2] if start > 1 else 0):
        start -= 1
    # BATRES.X ends on a jr $ra whose delay slot is its zeroed BSS, past the file
    end = min(max(i for i, w in enumerate(words) if w == JR_RA) + 2, len(words))
    return start * 4, end * 4


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", type=Path)
    ap.add_argument("name")
    ap.add_argument("vram", type=lambda s: int(s, 16))
    args = ap.parse_args()
    image = args.file.read_bytes()
    start, end = code_bounds(image)
    rodata = "      - [0, rodata]\n" if start else ""
    data = f"      - [{end:#x}, data]\n" if end < len(image) else ""
    print(f"""name: {args.file.name}
sha1: {hashlib.sha1(image).hexdigest()}
options:
  basename: {args.name}
  target_path: {args.file.as_posix()}
  base_path: ../..
  platform: psx
  compiler: PSYQ

  asm_path: asm/{args.name}
  src_path: src/{args.name}
  build_path: build/{args.name}
  ld_script_path: build/{args.name}.ld
  undefined_funcs_auto_path: build/{args.name}_undefined_funcs_auto.txt
  undefined_syms_auto_path: build/{args.name}_undefined_syms_auto.txt
  symbol_addrs_path:
    - config/symbols.main.txt
    - config/symbols.main.psyq.txt

  find_file_boundaries: False
  gp_value: 0x80062D44
  o_as_suffix: True
  use_legacy_include_asm: False
  generate_asm_macros_files: False
  section_order: [".rodata", ".text", ".data", ".bss"]
  string_encoding: ASCII
  data_string_encoding: ASCII
  rodata_string_guesser_level: 2
  data_string_guesser_level: 2

segments:
  - name: {args.name}
    type: code
    start: 0
    vram: 0x{args.vram:X}
    subsegments:
{rodata}      - [{start:#x}, asm]
{data}  - [0x{len(image):X}]""")


if __name__ == "__main__":
    main()
