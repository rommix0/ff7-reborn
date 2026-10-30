"""The splat config for a module the PC port recompiles (docs/pc_port.md), on stdout.

usage: python tools/module_yaml.py FILE NAME VRAM

FILE is the module as the game loads it (raw, at VRAM). Modules lay out as read-only data, code, then data: the code
runs from the first function's stack-frame setup to the last `jr $ra`'s delay slot. Its asm goes to asm/NAME, and calls
into the exe take their names from the exe's symbol files.
"""
import argparse
import hashlib
from pathlib import Path

PROLOGUE = 0x27BD  # addiu $sp, $sp, -n
JR_RA = 0x03E00008


def code_bounds(image: bytes) -> tuple[int, int]:
    """Where the code starts and ends in a module."""
    words = [int.from_bytes(image[i:i + 4], "little") for i in range(0, len(image) & ~3, 4)]
    start = next(i for i, w in enumerate(words) if w >> 16 == PROLOGUE and w & 0x8000)
    end = max(i for i, w in enumerate(words) if w == JR_RA) + 2
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
      - [{end:#x}, data]
  - [0x{len(image):X}]""")


if __name__ == "__main__":
    main()
