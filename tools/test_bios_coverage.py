"""Every BIOS function the exe calls has an entry in pc/bios.c's table.

usage: python tools/test_bios_coverage.py SCUS_941.63

The exe reaches the BIOS through stubs (li $t2, 0xA0/0xB0/0xC0; jr $t2; li $t1, N, in some order) and a few inline
calls (the same with jalr $t2). This fails on a (table, N) pair bios.c doesn't handle.
"""
import re
import struct
import sys
from pathlib import Path

LI_T2, LI_T1, JR_T2, JALR_T2 = 0x240A, 0x2409, 0x01400008, 0x0140F809  # addiu $t2/$t1, $zero, imm; jr/jalr $t2


def bios_calls(exe: bytes) -> set[tuple[int, int]]:
    words = struct.unpack_from(f"<{(len(exe) - 0x800) // 4}I", exe, 0x800)
    calls = set()
    for i in range(len(words) - 2):
        w = words[i:i + 3]
        table = [x & 0xFFFF for x in w if x >> 16 == LI_T2]
        number = [x & 0xFFFF for x in w if x >> 16 == LI_T1]
        if (JR_T2 in w or JALR_T2 in w) and table and number and table[0] in (0xA0, 0xB0, 0xC0):
            calls.add((table[0], number[0]))
    return calls


def main():
    calls = bios_calls(Path(sys.argv[1]).read_bytes())
    handled = {(int(t, 16), int(n, 16)) for t, n in re.findall(r"\{(0x[AB0C]0), (0x[0-9A-F]+),",
                                                                Path("pc/bios.c").read_text())}
    assert len(calls) == 56, len(calls)
    missing = sorted(calls - handled)
    assert not missing, [f"{t:X}({n:02X}h)" for t, n in missing]
    print("ok")


if __name__ == "__main__":
    main()
