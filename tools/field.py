"""Dump the field maps (FIELD/*.DAT) to files people can edit, and build them back.

  python tools/field.py dump "assets/Final Fantasy VII (USA) (Disc 1)" field
  python tools/field.py build field assets

A JSON file per map: its entities (a name and, for each of their 32 script slots, which script runs), the scripts a
line per operation (the field module's own name for it, then its operand bytes in hex, jumps to labels), and the
dialogue in the text format tools/kernel.py describes. Operations are named from the disc's FIELD.BIN, op_XX where
it has no name. Then the other sections:
  walkmesh    triangles (three x, y, z, w; z the height, w unread) and the neighbour across each edge (-1 a wall)
  background  four layers of runs of tiles (one row each) and texture-page switches, each switch the page of the
              tiles before it (the field draws a layer's list from its end); a tile is
              layer 1: x, y, u, v, palette
              layer 2: x, y, u, v, palette, page, depth, group, blended, state bits
              layers 3 and 4: x, y, u, v, palette, group, blended, state bits
  camera      the rotation, translation, screen offset and projection distance
  map         its name, the pad's turn, the camera's focus and scroll box, layers 3 and 4's scrolling, 12 exits,
              their arrows, 12 triggers (a line that sets or clears a background group's bit) and 12 markers
  encounters  two tables: on, the rate, six battles and four special ones, each with its chance out of 64
  models      each model's face, bones, parts, animations, whether it's loaded and its .BCX
A section left out of a file stays as it was. build writes the maps that differ from the originals into every disc
folder under assets, keeping the first originals as .orig, and FIELD.BIN's table of where each map lies. A map
unpacks to at most 77,852 bytes (the background is unpacked right after it).
"""
import argparse
import gzip
import json
import re
from pathlib import Path

import kernel
from unpack import lzs

LOAD, ROOM = 0x80114FE4, 0x80128000 - 0x80114FE4  # where a map unpacks, and the .MIM's buffer after it
MODULE, HANDLERS, PRINTER, MAPS = 0x800A0000, 0x40228, 0x800BEAD4, 0x3A5B8  # FIELD.BIN unpacked
FIELD_ARGS = dict.fromkeys(range(0xFA, 0xFF), 1)  # func_800D5C9C: FA-FD a glyph page, FE a sub-code
# Bytes each operation takes, from how far its handler moves the script on (0: not used, spcal and kawai vary).
LENGTHS = [
    1, 3, 3, 3, 3, 3, 3, 2, 2, 15, 6, 6, 0, 0, 2, 0,  # 00
    2, 3, 2, 3, 6, 7, 8, 9, 8, 9, 0, 0, 0, 0, 0, 0,  # 10
    11, 2, 5, 3, 3, 9, 2, 2, 0, 1, 2, 2, 5, 7, 2, 10,  # 20
    4, 4, 4, 2, 2, 4, 5, 8, 6, 6, 6, 4, 1, 1, 1, 1,  # 30
    3, 5, 6, 2, 0, 5, 0, 5, 7, 4, 2, 2, 0, 5, 0, 5,  # 40
    10, 6, 4, 2, 2, 3, 7, 7, 5, 5, 5, 7, 8, 10, 8, 1,  # 50
    10, 2, 5, 6, 6, 1, 9, 1, 9, 2, 7, 9, 1, 4, 3, 6,  # 60
    4, 2, 3, 4, 4, 8, 4, 5, 4, 5, 3, 3, 3, 3, 2, 3,  # 70
    4, 5, 4, 4, 4, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4,  # 80
    5, 4, 5, 4, 5, 3, 3, 3, 3, 3, 4, 5, 6, 7, 7, 11,  # 90
    2, 2, 3, 3, 2, 11, 9, 9, 6, 6, 2, 4, 1, 6, 3, 3,  # A0
    5, 5, 4, 3, 6, 6, 2, 4, 5, 4, 3, 5, 5, 4, 0, 2,  # B0
    11, 8, 15, 12, 1, 3, 3, 2, 2, 2, 4, 3, 3, 3, 2, 2,  # C0
    13, 2, 2, 16, 10, 10, 4, 4, 3, 1, 15, 2, 4, 1, 1, 11,  # D0
    4, 4, 3, 3, 3, 5, 5, 5, 7, 10, 10, 5, 5, 8, 8, 11,  # E0
    2, 5, 14, 2, 2, 2, 2, 4, 2, 1, 3, 2, 2, 6, 3, 1,  # F0
]
SPCAL = {0xF5: 3, 0xF6: 6, 0xF7: 4, 0xF8: 4, 0xF9: 2, 0xFA: 2, 0xFB: 3, 0xFC: 3, 0xFD: 4, 0xFE: 2, 0xFF: 2}
# Jumps: where the operand sits and its size. Forward ones count from the operand, backward ones from the operation.
FORWARD = {0x10: (1, 1), 0x11: (1, 2), 0x14: (5, 1), 0x15: (5, 2), 0x16: (7, 1), 0x17: (7, 2), 0x18: (7, 1),
           0x19: (7, 2), 0x30: (3, 1), 0x31: (3, 1), 0x32: (3, 1), 0xCB: (2, 1), 0xCC: (2, 1)}
BACKWARD = {0x12: (1, 1), 0x13: (1, 2)}
JUMPS = FORWARD | BACKWARD
# Operands a handler reads or writes through func_800BEE10/BF908/BF3AC/C0248 (offset, bytes, bank nibble: 1 is
# byte 1's high one, 2 its low one, up to 6 for byte 3); with the nibble 0 the operand is the value itself, else an
# address in that bank. The if family's come from its compare helpers; ask's (48) from two of its three calls; move,
# cmove and fmove (A8, A9, AD) set theirs in branch delay slots.
SPEC = {
    0x09: ((4, 2, 1), (6, 2, 2), (8, 1, 3), (9, 2, 4), (11, 2, 5), (13, 1, 6)), 0x14: ((2, 1, 1), (3, 1, 2)),
    0x15: ((2, 1, 1), (3, 1, 2)), 0x16: ((2, 2, 1), (4, 2, 2)), 0x17: ((2, 2, 1), (4, 2, 2)),
    0x18: ((2, 2, 1), (4, 2, 2)), 0x19: ((2, 2, 1), (4, 2, 2)), 0x23: ((2, 1, 2),),
    0x25: ((4, 1, 1), (5, 1, 2), (6, 1, 3), (7, 2, 4)), 0x2C: ((3, 2, 1),), 0x37: ((3, 2, 1), (5, 2, 2)),
    0x38: ((3, 1, 1), (4, 1, 2), (5, 1, 4)), 0x39: ((2, 2, 1), (4, 2, 2)), 0x3A: ((2, 2, 1), (4, 2, 2)),
    0x3B: ((2, 1, 1), (3, 1, 2)), 0x41: ((3, 1, 1),), 0x42: ((3, 1, 1),), 0x45: ((3, 2, 2),), 0x47: ((3, 2, 2),),
    0x48: ((6, 1, 2),), 0x49: ((3, 1, 2),), 0x4D: ((3, 2, 2),), 0x4F: ((3, 2, 2),),
    0x56: ((3, 1, 1), (4, 1, 2), (5, 1, 3), (6, 1, 4)), 0x57: ((3, 1, 1), (4, 1, 2), (5, 1, 3), (6, 1, 4)),
    0x58: ((2, 2, 1), (4, 1, 2)), 0x59: ((2, 2, 1), (4, 1, 2)), 0x5A: ((2, 2, 1), (4, 1, 2)),
    0x5B: ((3, 1, 1), (4, 1, 2), (5, 1, 3), (6, 1, 4)), 0x5C: ((3, 1, 1), (4, 1, 2), (5, 1, 3), (6, 1, 4)),
    0x5D: ((4, 1, 1), (5, 1, 2), (6, 1, 3), (7, 1, 4), (9, 1, 6)),
    0x5E: ((4, 1, 1), (5, 1, 2), (6, 1, 3), (7, 1, 4)), 0x62: ((2, 2, 2),), 0x63: ((2, 2, 2),),
    0x64: ((2, 2, 1), (4, 2, 2)), 0x66: ((3, 2, 1), (5, 2, 2), (7, 2, 4)), 0x68: ((3, 2, 1), (5, 2, 2), (7, 2, 4)),
    0x6B: ((3, 1, 1), (4, 1, 2), (5, 1, 4)), 0x6E: ((2, 1, 2),), 0x6F: ((2, 2, 2),), 0x70: ((2, 2, 2),),
    0x73: ((3, 1, 2),), 0x74: ((3, 1, 2),), 0x75: ((4, 1, 1), (5, 1, 2), (6, 1, 3), (7, 1, 4)),
    0x76: ((2, 1, 1), (3, 1, 2)), 0x77: ((2, 1, 1), (3, 2, 2)), 0x78: ((2, 1, 1), (3, 1, 2)),
    0x79: ((2, 1, 1), (3, 2, 2)), 0x7A: ((2, 1, 2),), 0x7B: ((2, 1, 2),), 0x7C: ((2, 1, 2),), 0x7D: ((2, 1, 2),),
    0x7F: ((2, 1, 2),), 0x80: ((2, 1, 1), (3, 1, 2)), 0x81: ((2, 1, 1), (3, 2, 2)), 0x82: ((2, 1, 1), (3, 1, 2)),
    0x83: ((2, 1, 1), (3, 1, 2)), 0x84: ((2, 1, 1), (3, 1, 2)), 0x85: ((2, 1, 1), (3, 1, 2)),
    0x86: ((2, 1, 1), (3, 2, 2)), 0x87: ((2, 1, 1), (3, 1, 2)), 0x88: ((2, 1, 1), (3, 2, 2)),
    0x89: ((2, 1, 1), (3, 1, 2)), 0x8A: ((2, 1, 1), (3, 2, 2)), 0x8B: ((2, 1, 1), (3, 1, 2)),
    0x8C: ((2, 1, 1), (3, 2, 2)), 0x8D: ((2, 1, 1), (3, 1, 2)), 0x8E: ((2, 1, 1), (3, 2, 2)),
    0x8F: ((2, 1, 1), (3, 1, 2)), 0x90: ((2, 1, 1), (3, 2, 2)), 0x91: ((2, 1, 1), (3, 1, 2)),
    0x92: ((2, 1, 1), (3, 2, 2)), 0x93: ((2, 1, 1), (3, 1, 2)), 0x94: ((2, 1, 1), (3, 2, 2)), 0x95: ((2, 1, 2),),
    0x96: ((2, 1, 2),), 0x97: ((2, 1, 2),), 0x98: ((2, 1, 2),), 0x99: ((2, 1, 2),), 0x9A: ((2, 1, 1), (3, 1, 2)),
    0x9B: ((2, 1, 1), (3, 2, 2)), 0x9C: ((3, 1, 1), (4, 1, 2), (5, 1, 4)), 0x9D: ((3, 2, 2), (5, 1, 4)),
    0x9E: ((3, 2, 2), (5, 1, 4)), 0x9F: ((5, 2, 2), (7, 2, 3), (9, 1, 4), (10, 1, 6)),
    0xA5: ((3, 2, 1), (5, 2, 2), (7, 2, 3), (9, 2, 4)), 0xA6: ((3, 2, 1), (5, 2, 2), (7, 2, 3)),
    0xA7: ((3, 2, 1), (5, 2, 2), (7, 2, 3)), 0xA8: ((2, 2, 1), (4, 2, 2)), 0xA9: ((2, 2, 1), (4, 2, 2)),
    0xAD: ((2, 2, 1), (4, 2, 2)), 0xB2: ((2, 2, 2),), 0xB3: ((2, 1, 2),), 0xB4: ((2, 1, 2),), 0xB5: ((2, 2, 2),),
    0xB7: ((3, 1, 2),), 0xB8: ((3, 1, 1), (4, 1, 2)), 0xB9: ((3, 1, 2),), 0xBD: ((2, 2, 2),),
    0xC0: ((3, 2, 1), (5, 2, 2), (7, 2, 3), (9, 2, 4)), 0xC1: ((4, 1, 1), (5, 1, 2), (6, 1, 3), (7, 1, 4)),
    0xC2: ((3, 2, 1), (5, 2, 2), (7, 2, 3), (9, 2, 4)), 0xC3: ((4, 2, 1), (6, 2, 2), (8, 2, 3), (10, 2, 4)),
    0xC5: ((2, 1, 2),), 0xC6: ((2, 1, 2),),
    0xD3: ((4, 2, 1), (6, 2, 2), (8, 2, 3), (10, 2, 4), (12, 2, 5), (14, 2, 6)),
    0xD4: ((3, 2, 1), (5, 2, 2), (7, 2, 3), (9, 1, 4)), 0xD5: ((3, 2, 1), (5, 2, 2), (7, 2, 3), (9, 1, 4)),
    0xD6: ((2, 2, 2),), 0xD7: ((2, 2, 2),), 0xDA: ((5, 2, 1), (7, 2, 2), (9, 2, 3), (11, 2, 4), (13, 2, 6)),
    0xDF: ((6, 1, 1), (7, 1, 2), (8, 1, 3), (9, 1, 4), (10, 1, 6)), 0xE0: ((2, 1, 1), (3, 1, 2)),
    0xE1: ((2, 1, 1), (3, 1, 2)), 0xE2: ((2, 1, 2),), 0xE3: ((2, 1, 2),), 0xE4: ((2, 1, 2),),
    0xE5: ((2, 1, 1), (3, 1, 2)), 0xE6: ((2, 1, 1), (3, 1, 2)), 0xE7: ((2, 1, 1), (3, 1, 2)),
    0xE8: ((3, 1, 1), (4, 1, 2), (5, 1, 4)), 0xE9: ((4, 1, 1), (5, 1, 2), (6, 1, 3), (7, 1, 4), (8, 1, 5)),
    0xEA: ((4, 1, 1), (5, 1, 2), (6, 1, 3), (7, 1, 4), (8, 1, 5)), 0xED: ((5, 1, 1), (6, 1, 2), (7, 1, 4)),
    0xEE: ((5, 1, 1), (6, 1, 2), (7, 1, 4)), 0xEF: ((6, 1, 1), (7, 1, 2), (8, 1, 3), (9, 1, 4), (10, 1, 6)),
    0xF1: ((2, 2, 1), (4, 1, 2)), 0xF2: ((5, 1, 1), (6, 2, 2), (8, 2, 3), (10, 2, 4), (12, 2, 6)),
    0xF7: ((2, 1, 1), (3, 1, 2)), 0xFE: ((2, 1, 2),),
}
COMPARE = ["==", "!=", ">", "<", ">=", "<=", "&", "^", "|", "bit", "!bit"]  # the if family's operator byte
COMPARE_AT = {0x14: 4, 0x15: 4, 0x16: 6, 0x17: 6, 0x18: 6, 0x19: 6}
TEXT_AT = {0x40: 2, 0x43: 1, 0x48: 3}  # mes, mpnam, ask: the string they show
# Saved variables by (bank, address), named where the game's code shows what they mean. Banks 1/2 are 0x8009D288,
# 3/4 +0x100, 11/12 +0x200, 13/14 +0x300, 15/7 +0x400 (the first of each pair reads a byte, the second a u16 at the
# same address); 5/6 (0x80075E24) is each map's scratch space and isn't saved.
VARS = {
    (2, 0x00): "story",  # set only to constants, 0 to 1999; the world map picks its version by it
    (1, 0x10): "play_hours", (1, 0x11): "play_minutes", (1, 0x12): "play_seconds",  # func_800BB3A8
    (1, 0x14): "timer_hours", (1, 0x15): "timer_minutes", (1, 0x16): "timer_seconds",  # the countdown, same place
    (2, 0x18): "battles",  # main adds 1 each battle
    (2, 0x1C): "menu_shown", (2, 0x1E): "menu_locked",  # a bit per main menu entry, Item (0) to Save (9)
    (2, 0x70): "battle_points",  # the arena's low u16 of a u32 it draws as "Current Battle Points"
    (1, 0x74): "arena_round",
    (3, 0x09): "party1", (3, 0x0A): "party2", (3, 0x0B): "party3",  # character ids, FF empty; party1 is led
    (3, 0x73): "bike_mode",  # 1: the motorbike game keeps a score
    (4, 0x62): "speed_square_score",  # what the mini-game returns
    (3, 0xCE): "snowboard_exit",  # where the snowboard run ended, a bit per way out
    (11, 0x09): "chocobo_own",  # not 0: the player's race chocobo comes from the bank, not random
    (11, 0x17): "chocobo_class",  # 0-3: the rivals' stat ranges
    (11, 0x19): "chocobo_place",  # the last race's place, from 0
    (12, 0x6B): "bike_score", (12, 0x6D): "bike_high_score",
    (11, 0x7C): "snowboard_best1", (11, 0x7D): "snowboard_best2", (11, 0x7E): "snowboard_best3",
    (11, 0x7F): "snowboard_course",
    (13, 0x00): "disc",  # dskcg sets it; main swaps discs when it differs
    (13, 0x1F): "materia_full",  # smtra: 1 when all 200 slots were taken
    (14, 0x5E): "submarine_result",  # what the mini-game returns
    (15, 0x6F): "owns_all",  # a menu check: 1 when every item or materia in its list is owned
}
NAMED = {name: key for key, name in VARS.items()}
MODEL = ("face", "bones", "parts", "animations", "load", "bcx")  # section 7's entry, less two bytes the loader fills


def u16(data: bytes, at: int) -> int:
    return int.from_bytes(data[at:at + 2], "little")


def u32(data: bytes, at: int) -> int:
    return int.from_bytes(data[at:at + 4], "little")


def op_names(image: bytes) -> list[str]:
    """Each operation's name as the field module's debug trace prints it (its handler passes it to the printer)."""
    call = 0x0C000000 | (PRINTER & 0x0FFFFFFF) >> 2
    names = []
    for op in range(256):
        handler = u32(image, HANDLERS + 4 * op) - MODULE
        words = [u32(image, handler + 4 * i) for i in range(64)]
        if 0x03E00008 in words:  # jr ra: the handler ends there
            words = words[:words.index(0x03E00008) + 2]
        words = words[:words.index(call) + 2] if call in words else []
        name = ""
        for i, w in enumerate(words[:-1]):  # the first lui a0 / addiu a0, a0 pair (the addiu can be in the delay slot)
            lo = words[i + 2] if words[i + 1] == call and i + 2 < len(words) else words[i + 1]
            if w >> 16 == 0x3C04 and lo >> 16 == 0x2484:
                at = ((w & 0xFFFF) << 16) + ((lo & 0xFFFF) ^ 0x8000) - 0x8000 - MODULE
                name = image[at:image.index(0, at)].decode("latin-1")
                break
        if not name or "?" in name:
            name = f"op_{op:02X}"
        elif name in names:
            name = f"{name}_{op:02X}"
        names.append(name)
    return names


def length(code: bytes, at: int) -> int:
    op = code[at]
    if op == 0x0F:
        return SPCAL.get(code[at + 1], 2)
    if op == 0x28:
        return code[at + 1]
    return LENGTHS[op] or 1


def is_byte(token: str) -> bool:
    return len(token) == 2 and all(c in "0123456789abcdefABCDEF" for c in token)


def tokens(op: bytes, label: str | None, plain: bool = False) -> list[str]:
    """An operation's operands as text: SPEC's values as #literal or var[bank][address] (their bank bytes left out),
    the if family's operator by its sign and between its values, a jump's label, the rest (all, if plain) as hex."""
    fields = {} if plain else {at: (size, n) for at, size, n in SPEC.get(op[0], ())}
    banks = {(n + 1) // 2 for size, n in fields.values()}
    jump = JUMPS[op[0]] if label else None
    out, at = [], 2 if op[0] == 0x28 else 1  # kawai's length byte is worked out again
    while at < len(op):
        if jump and at == jump[0]:
            out.append(label)
            at += jump[1]
        elif at in banks:
            at += 1
        elif at in fields:
            size, n = fields[at]
            bank = op[(n + 1) // 2] >> 4 if n % 2 else op[(n + 1) // 2] & 15
            value = int.from_bytes(op[at:at + size], "little")
            out.append(VARS.get((bank, value), f"var[{bank}][{value:02X}]") if bank else f"#{value}")
            at += size
        elif not plain and COMPARE_AT.get(op[0]) == at and op[at] < len(COMPARE):
            out.append(COMPARE[op[at]])
            at += 1
        else:
            out.append(f"{op[at]:02X}")
            at += 1
    if len(out) > 2 and out[2] in COMPARE:
        out[1:3] = out[2], out[1]
    return out


def operands(op: int, words: list[str]) -> tuple[bytearray, str | None]:
    """Operand bytes from tokens(): a line with no #, var or operator is all hex bytes; a jump's label comes back
    with its bytes left zero."""
    nice = any(w.startswith(("#", "var[")) or w in COMPARE or w in NAMED for w in words)
    fields = {at: (size, n) for at, size, n in SPEC.get(op, ())} if nice else {}
    banks = {(n + 1) // 2 for size, n in fields.values()}
    if nice and len(words) > 2 and words[1] in COMPARE:
        words = [words[0], words[2], words[1]] + words[3:]
    out, label, at = bytearray(), None, 2 if op == 0x28 else 1
    pending = iter(words)
    while True:
        if at in banks:
            out.append(0)
            at += 1
            continue
        word = next(pending, None)
        if word is None:
            return out, label
        if op in JUMPS and at == JUMPS[op][0] and not is_byte(word):
            label = word
            out += bytes(JUMPS[op][1])
            at += JUMPS[op][1]
        elif at in fields:
            size, n = fields[at]
            if word.startswith("#"):
                bank, value = 0, int(word[1:])
            elif word in NAMED:
                bank, value = NAMED[word]
            elif word.startswith("var[") and word.endswith("]") and "][" in word:
                bank, value = int(word[4:word.index("]")]), int(word[word.index("][") + 2:-1], 16)
            else:
                raise ValueError(f"{word} isn't #value, var[bank][address] or a variable's name")
            if not 0 <= bank < 16:
                raise ValueError(f"{word}: banks go 0 to 15")
            out[(n + 1) // 2 - 1] |= bank << 4 if n % 2 else bank
            out += (value & (1 << 8 * size) - 1).to_bytes(size, "little")
            at += size
        elif nice and COMPARE_AT.get(op) == at and word in COMPARE:
            out.append(COMPARE.index(word))
            at += 1
        elif is_byte(word):
            out.append(int(word, 16))
            at += 1
        else:
            raise ValueError(f"{word} isn't a byte here")


def disassemble(code: bytes, names: list[str], strings: list | None = None) -> list[str]:
    ops, at = [], 0
    while at < len(code):
        ops.append((at, code[at:at + length(code, at)]))
        at += len(ops[-1][1])
    starts = {at for at, _ in ops} | {len(code)}
    targets = {}
    for at, op in ops:
        if op[0] in JUMPS:
            where, size = JUMPS[op[0]]
            value = int.from_bytes(op[where:where + size], "little")
            target = at + where + value if op[0] in FORWARD else at - value
            if target in starts:
                targets[at] = target
    labels = set(targets.values())
    lines = []
    for at, op in ops:
        if at in labels:
            lines.append(f"L{at:04X}:")
        label = f"L{targets[at]:04X}" if at in targets else None
        words = tokens(op, label)
        raw = bytearray(op)
        if label:
            where, size = JUMPS[op[0]]
            raw[where:where + size] = bytes(size)
        if bytes(operands(op[0], words)[0]) != raw[2 if op[0] == 0x28 else 1:]:  # what that can't say stays hex
            words = tokens(op, label, plain=True)
        line = " ".join([names[op[0]]] + words)
        if strings and op[0] in TEXT_AT and op[TEXT_AT[op[0]]] < len(strings):
            text = strings[op[TEXT_AT[op[0]]]]
            line += f" ; {strings[text] if isinstance(text, int) else text}"
        lines.append(line)
    if len(code) in labels:
        lines.append(f"L{len(code):04X}:")
    return lines


def assemble(lines: list[str], names: list[str]) -> bytes:
    opcodes = {name: op for op, name in enumerate(names)}
    code, labels, jumps = bytearray(), {}, []
    for line in lines:
        line = line.split(";")[0].strip()  # a comment: mes shows its string there
        if line.endswith(":"):
            labels[line[:-1]] = len(code)
            continue
        name, *words = line.split()
        if name not in opcodes:
            raise ValueError(f"{line!r}: no operation {name}")
        op, at = opcodes[name], len(code)
        try:
            data, label = operands(op, words)
        except ValueError as e:
            raise ValueError(f"{line!r}: {e}")
        code.append(op)
        if op == 0x28:
            code.append(len(data) + 2)
        if label:
            jumps.append((at, label, line))
        code += data
        if op != 0x28 and len(code) - at != length(code, at):
            raise ValueError(f"{line!r}: {name} takes {length(code, at) - 1} operand bytes")
    for at, label, line in jumps:
        if label not in labels:
            raise ValueError(f"{line!r}: no label {label}")
        where, size = JUMPS[code[at]]
        value = labels[label] - at - where if code[at] in FORWARD else at - labels[label]
        if not 0 <= value < 1 << 8 * size:
            raise ValueError(f"{line!r}: {label} is out of this jump's reach")
        code[at + where:at + where + size] = value.to_bytes(size, "little")
    return bytes(code)


def dialogue(sec: bytes) -> tuple[list[bytes], int, int, int | None]:
    """The strings (each to the next one's offset, the last to its FF), and where their table, the first of them and
    what follows them start. A pointer with no room for a count behind it has no table; no pointer, no rest."""
    n, start = sec[2], u16(sec, 4)
    end = min([u32(sec, 0x20 + 8 * n + 4 * i) for i in range(u16(sec, 6))] + [len(sec)])
    if not start:
        return [], 0, 0, None
    if end - start < 2:
        return [], start, start, start
    offsets = [u16(sec, start + 2 + 2 * i) for i in range(u16(sec, start))]
    later = sorted(set(offsets))
    strings = []
    for o in offsets:
        after = [x for x in later if x > o]
        ff = sec.find(b"\xff", start + o, end)
        strings.append(sec[start + o:start + after[0]] if after else sec[start + o:ff + 1 if ff >= 0 else end])
    first = start + min(offsets, default=2)
    return strings, start, first, max([first] + [start + o + len(s) for o, s in zip(offsets, strings)])


def section_json(sec: bytes, names: list[str]) -> dict:
    n, strings_at, music = sec[2], u16(sec, 4), u16(sec, 6)
    table = 0x20 + 8 * n + 4 * music
    slots = [[u16(sec, table + 64 * e + 2 * s) for s in range(32)] for e in range(n)]
    starts = sorted({at for entity in slots for at in entity})
    music_at = [u32(sec, 0x20 + 8 * n + 4 * i) for i in range(music)]
    code_end = min([strings_at] + music_at) if strings_at else min(music_at + [len(sec)])
    strings, start = dialogue(sec)[:2]
    offsets = [u16(sec, start + 2 + 2 * i) for i in range(len(strings))]
    texts = [offsets.index(o) if offsets.index(o) < i else  # a string shared with an earlier one
             kernel.decode(s[:-1] if s.endswith(b"\xff") else s, FIELD_ARGS)
             for i, (o, s) in enumerate(zip(offsets, strings))]
    out = {"entities": [{"name": sec[0x20 + 8 * e:0x28 + 8 * e].rstrip(b"\0").decode("latin-1"),
                         "scripts": [starts.index(at) for at in slots[e]]} for e in range(n)],
           "scripts": [disassemble(sec[at:end], names, texts) for at, end in zip(starts, starts[1:] + [code_end])],
           "strings": texts}
    unterminated = [i for i, s in enumerate(strings) if not s.endswith(b"\xff")]
    if unterminated:
        out["unterminated"] = unterminated
    return out


def section_raw(table: dict, original: bytes, names: list[str]) -> bytes:
    """Section 1 from a dump; what the dump doesn't hold (the header, what follows the dialogue, the music) comes from
    the original, moved on by whole words so the music stays aligned."""
    n, music = len(table["entities"]), u16(original, 6)
    old_strings, old_start, old_first, old_rest = dialogue(original)
    old_n = original[2]
    out = bytearray(original[:0x20])
    out[2] = n
    for e in table["entities"]:
        out += e["name"].encode("latin-1").ljust(8, b"\0")
    out += original[0x20 + 8 * old_n:0x20 + 8 * old_n + 4 * music]
    table_at = len(out)
    out += bytes(64 * n)
    script_at = []
    for lines in table["scripts"]:
        script_at.append(len(out))
        out += assemble(lines, names)
    for e, entity in enumerate(table["entities"]):
        for s, k in enumerate(entity["scripts"]):
            out[table_at + 64 * e + 2 * s:table_at + 64 * e + 2 * s + 2] = script_at[k].to_bytes(2, "little")
    if old_rest is None:  # no dialogue table
        if table["strings"]:
            raise ValueError("this map has no dialogue table to add strings to")
        old_rest = min([u32(original, 0x20 + 8 * old_n + 4 * i) for i in range(music)] + [len(original)])
        out[4:6] = bytes(2)
    elif not old_strings and old_rest == old_start:  # a pointer with no table behind it
        out[4:6] = len(out).to_bytes(2, "little")
    else:
        start = len(out)
        out[4:6] = start.to_bytes(2, "little")
        count = len(table["strings"])
        out += count.to_bytes(2, "little") + bytes(2 * count) + original[old_start + 2 + 2 * len(old_strings):old_first]
        unterminated = set(table.get("unterminated", []))
        for i, text in enumerate(table["strings"]):
            if isinstance(text, int):
                at = out[start + 2 + 2 * text:start + 4 + 2 * text]
            else:
                at = (len(out) - start).to_bytes(2, "little")
                out += kernel.encode(text) + (b"" if i in unterminated else b"\xff")
            out[start + 2 + 2 * i:start + 4 + 2 * i] = at
    out += bytes(-(len(out) - old_rest) % 4)
    shift = len(out) - old_rest
    out += original[old_rest:]
    for i in range(music):
        at = 0x20 + 8 * n + 4 * i
        out[at:at + 4] = (u32(original, 0x20 + 8 * old_n + 4 * i) + shift).to_bytes(4, "little")
    return bytes(out)


def s16(data: bytes, at: int) -> int:
    return int.from_bytes(data[at:at + 2], "little", signed=True)


def walkmesh_json(sec: bytes) -> dict:
    n = u16(sec, 0)
    out = {"triangles": [{"vertices": [[s16(sec, 4 + 24 * t + 8 * v + 2 * c) for c in range(4)] for v in range(3)],
                          "neighbours": [s16(sec, 4 + 24 * n + 6 * t + 2 * k) for k in range(3)]} for t in range(n)]}
    if n % 2:  # the section is padded to a word with whatever was there
        out["pad"] = u16(sec, 4 + 30 * n)
    return out


def walkmesh_raw(mesh: dict) -> bytes:
    tris = mesh["triangles"]
    if len(tris) > 512:  # idlck's bits
        raise ValueError("walkmesh: at most 512 triangles")
    out = len(tris).to_bytes(2, "little") + bytes(2)
    out += b"".join(c.to_bytes(2, "little", signed=True) for t in tris for v in t["vertices"] for c in v)
    out += b"".join(k.to_bytes(2, "little", signed=True) for t in tris for k in t["neighbours"])
    return out + mesh.get("pad", 0).to_bytes(2, "little") if len(tris) % 2 else out


def background_json(sec: bytes) -> dict:
    tiles, pages = [u32(sec, 4 * i) for i in range(4)], u32(sec, 4)
    at, next_tile, layers = 0x10, [tiles[0], tiles[2], tiles[3], tiles[3]], []
    for layer in range(4):
        items = []
        for at in range(at, len(sec), 6):
            y, n = s16(sec, at), s16(sec, at + 2)
            if y == 0x7FFF:
                break
            if y == 0x7FFE:  # the next texture page; layers 3 and 4 have none of their own
                items.append({"page": u16(sec, pages + 2 * n)})
                continue
            run, size = [], (8, 14, 10, 10)[layer]
            for t in range(next_tile[layer], next_tile[layer] + size * s16(sec, at + 4), size):
                b = sec[t:t + size]
                tile = [s16(b, 0), s16(b, 2), b[4], b[5], u16(b, 6)]
                if layer == 1:
                    tile += [u16(b, 8), u16(b, 10)]  # its page, and its place in the ordering table
                if layer:
                    tile += [b[size - 2] & 0x3F, b[size - 2] >> 7, b[size - 1]]  # group, blended, state bits
                run.append(tile)
            next_tile[layer] += size * len(run)
            if layer == 2:
                next_tile[3] = next_tile[2]
            items.append(run)
        at += 2
        layers.append(items)
    out = {"layers": layers}
    if next_tile[3] < len(sec):
        out["pad"] = u16(sec, next_tile[3])
    return out


def background_raw(bg: dict) -> bytes:
    if len(bg["layers"]) != 4:
        raise ValueError("background: four layers")
    runs, pages, tables, index = b"", b"", [b"", b"", b""], 0  # tables: layer 1's, layer 2's, layers 3 and 4's
    for layer, items in enumerate(bg["layers"]):
        for item in items:
            if isinstance(item, dict):
                if layer == 1:
                    raise ValueError("background: layer 2's tiles carry their own pages")
                runs += b"\xfe\x7f" + (len(pages) // 2).to_bytes(2, "little") + bytes(2)
                pages += item["page"].to_bytes(2, "little")
                continue
            if any(tile[1] != item[0][1] for tile in item):
                raise ValueError("background: a run's tiles share a row")
            runs += b"".join(v.to_bytes(2, "little", signed=True) for v in (item[0][1], index, len(item)))
            index += len(item)  # the tiles' place in the field's state bytes, across all four layers
            for tile in item:
                b = b"".join(v.to_bytes(2, "little", signed=True) for v in tile[:2]) + bytes(tile[2:4])
                b += tile[4].to_bytes(2, "little")
                if layer == 1:
                    b += tile[5].to_bytes(2, "little") + tile[6].to_bytes(2, "little")
                if layer:
                    group, blend, state = tile[-3:]
                    b += bytes([blend << 7 | group, state])
                tables[min(layer, 2)] += b
        runs += b"\xff\x7f"
    offsets = [0x10 + len(runs)]
    for size in (len(tables[0]), len(pages), len(tables[1])):
        offsets.append(offsets[-1] + size)
    out = b"".join(o.to_bytes(4, "little") for o in offsets) + runs + tables[0] + pages + tables[1] + tables[2]
    return out + bg["pad"].to_bytes(2, "little") if "pad" in bg else out


def camera_json(sec: bytes) -> dict:
    return {"rotation": [[s16(sec, 6 * r + 2 * c) for c in range(3)] for r in range(3)], "pad": s16(sec, 0x12),
            "translation": [int.from_bytes(sec[0x14 + 4 * i:0x18 + 4 * i], "little", signed=True) for i in range(3)],
            "offset": [s16(sec, 0x20), s16(sec, 0x22)], "distance": s16(sec, 0x24), "unread": sec[0x26:].hex(" ")}


def camera_raw(cam: dict) -> bytes:
    out = b"".join(c.to_bytes(2, "little", signed=True) for row in cam["rotation"] for c in row)
    out += cam["pad"].to_bytes(2, "little", signed=True)
    out += b"".join(t.to_bytes(4, "little", signed=True) for t in cam["translation"])
    out += b"".join(v.to_bytes(2, "little", signed=True) for v in cam["offset"] + [cam["distance"]])
    return out + bytes.fromhex(cam["unread"])


def map_json(sec: bytes) -> dict:
    def line(at: int) -> list[list[int]]:
        return [[s16(sec, at + 6 * e + 2 * c) for c in range(3)] for e in range(2)]

    exits, triggers = [], []
    for at in range(0x38, 0x158, 0x18):
        b = sec[at:at + 0x18]
        if u16(b, 0x12) == 0x7FFF:
            exits.append(None)
            continue
        exits.append({"line": line(at), "map": u16(b, 0x12), "x": s16(b, 0xC), "y": s16(b, 0xE),
                      "triangle": u16(b, 0x10), "facing": b[0x14]} |
                     ({"unread": list(b[0x15:0x18])} if b[0x15:0x18] != bytes([b[0x14]] * 3) else {}))
    for at in range(0x158, 0x218, 0x10):
        b = sec[at:at + 0x10]
        triggers.append(None if b[0xC] == 0xFF else
                        {"line": line(at), "group": b[0xC], "bit": b[0xD], "kind": b[0xE], "sound": b[0xF]})
    return {"name": sec[:8].rstrip(b"\0").decode("latin-1"), "turn": sec[9], "focus": s16(sec, 0xA),
            "scroll": [s16(sec, 0xC + 2 * i) for i in range(4)], "scroll_mode": sec[0x14],
            "layers": [{"wrap": [s16(sec, 0x18 + 4 * k), s16(sec, 0x1A + 4 * k)],
                        "start": [s16(sec, 0x20 + 4 * k), s16(sec, 0x22 + 4 * k)],
                        "parallax": [s16(sec, 0x28 + 4 * k), s16(sec, 0x2A + 4 * k)]} for k in range(2)],
            "unread": (sec[8:9] + sec[0x15:0x18] + sec[0x30:0x38]).hex(" "),
            "exits": exits, "arrows": list(sec[0x218:0x224]), "triggers": triggers,
            "markers": [[s16(sec, at + 4 * i) for i in range(3)] + [u32(sec, at + 12)]
                        for at in range(0x224, 0x2E4, 16)]}


def map_raw(m: dict) -> bytes:
    def coords(*values: int) -> bytes:
        return b"".join(v.to_bytes(2, "little", signed=True) for v in values)

    if not len(m["exits"]) == len(m["triggers"]) == len(m["arrows"]) == len(m["markers"]) == 12:
        raise ValueError("map: 12 exits, triggers, arrows and markers")
    unread = bytes.fromhex(m["unread"])
    out = m["name"].encode("latin-1").ljust(8, b"\0") + unread[:1] + bytes([m["turn"]]) + coords(m["focus"])
    out += coords(*m["scroll"]) + bytes([m["scroll_mode"]]) + unread[1:4]
    out += b"".join(coords(*layer[key]) for key in ("wrap", "start", "parallax") for layer in m["layers"])
    out += unread[4:]
    for e in m["exits"]:
        if e is None:
            out += bytes(0x12) + b"\xff\x7f" + bytes(4)
            continue
        out += coords(*e["line"][0], *e["line"][1], e["x"], e["y"]) + e["triangle"].to_bytes(2, "little")
        out += e["map"].to_bytes(2, "little") + bytes([e["facing"]] + e.get("unread", [e["facing"]] * 3))
    for t in m["triggers"]:
        out += bytes(12) + b"\xff" + bytes(3) if t is None else \
            coords(*t["line"][0], *t["line"][1]) + bytes([t["group"], t["bit"], t["kind"], t["sound"]])
    out += bytes(m["arrows"])
    return out + b"".join(coords(x) + bytes(2) + coords(y) + bytes(2) + coords(z) + bytes(2) +
                          kind.to_bytes(4, "little") for x, y, z, kind in m["markers"])


def encounters_json(sec: bytes) -> list[dict]:
    out = []
    for t in range(2):
        w = [u16(sec, 24 * t + 2 * i) for i in range(11)]
        out.append({"on": w[0] & 1, "rate": w[0] >> 8, "battles": [[x & 0x3FF, x >> 10] for x in w[1:7]],
                    "special": [[x & 0x3FF, x >> 10] for x in w[7:11]]})
    return out


def encounters_raw(tables: list[dict]) -> bytes:
    out = b""
    for t in tables:
        pairs = t["battles"] + t["special"]
        if len(t["battles"]) != 6 or len(t["special"]) != 4 or len(tables) != 2:
            raise ValueError("encounters: two tables of 6 battles and 4 special ones")
        if not all(0 <= b < 1024 and 0 <= c < 64 for b, c in pairs) or not 0 <= t["rate"] < 256:
            raise ValueError("encounters: battles 0-1023, chances 0-63, rate 0-255")
        words = [t["rate"] << 8 | t["on"]] + [c << 10 | b for b, c in pairs] + [0]
        out += b"".join(w.to_bytes(2, "little") for w in words)
    return out


def models_json(sec: bytes) -> list[dict]:
    return [dict(zip(MODEL, (b[0], b[1], b[2], b[3], b[5], b[7])))
            for b in (sec[at:at + 8] for at in range(4, 4 + 8 * u16(sec, 2), 8))]


def models_raw(models: list[dict]) -> bytes:
    out = (4 + 8 * len(models)).to_bytes(2, "little") + len(models).to_bytes(2, "little")
    for m in models:
        face, bones, parts, anims, load, bcx = (m[k] for k in MODEL)
        out += bytes([face, bones, parts, anims, 0, load, 0, bcx])  # +4 the loader's slot, filled at load
    return out


def sections(dat: bytes) -> list[int]:
    return [u32(dat, 4 * i) - LOAD for i in range(7)] + [len(dat)]


def dat_raw(secs: list[bytes]) -> bytes:
    at, header = 4 * len(secs), b""
    for sec in secs:
        header += (LOAD + at).to_bytes(4, "little")
        at += len(sec)
    return header + b"".join(secs)


def lzs_pack(data: bytes, start: int = 0) -> bytes:
    """LZSS as unpack.lzs reads it: a flag byte per eight items (1 a literal), a reference 12 bits of window position
    and 4 of length less 3. Greedy, the longest match in the window. From start on, the bytes before it already
    in the window (a stream cut at a flag byte goes on from there)."""
    out, chains, i = bytearray(), {}, start
    for k in range(max(0, start - (4096 - 18)), start):
        chains.setdefault(data[k:k + 3], []).append(k)
    while i < len(data):
        flags = len(out)
        out.append(0)
        for bit in range(8):
            if i >= len(data):
                break
            best, at = 0, 0
            for j in reversed(chains.get(data[i:i + 3], [])[-256:]):
                if i - j > 4096 - 18:
                    break
                n = 3
                while n < 18 and i + n < len(data) and data[j + n] == data[i + n]:
                    n += 1
                if n > best:
                    best, at = n, j
                    if n == 18:
                        break
            if best >= 3:
                where = (4096 - 18 + at) & 0xFFF
                out += bytes([where & 0xFF, (where >> 4) & 0xF0 | (best - 3)])
            else:
                best = 1
                out[flags] |= 1 << bit
                out.append(data[i])
            for k in range(i, i + best):
                chains.setdefault(data[k:k + 3], []).append(k)
            i += best
    return len(out).to_bytes(4, "little") + bytes(out)


def groups(packed: bytes) -> list[tuple[int, int]]:
    """Where each flag byte of an LZS stream is, and how much it has unpacked by then."""
    at, pos, out = 4, 0, []
    while at < len(packed):
        out.append((at, pos))
        flags = packed[at]
        at += 1
        for bit in range(8):
            if at >= len(packed):
                break
            if flags >> bit & 1:
                at, pos = at + 1, pos + 1
            else:
                at, pos = at + 2, pos + (packed[at + 1] & 15) + 3
    return out


def repack(packed: bytes, data: bytes, start: int) -> bytes:
    """data packed again from the last flag byte before start (what's before it is unchanged), the stream up to it
    kept as it was: the original packer did better than ours, and files on the disc have little room to grow."""
    at, pos = max((g for g in groups(packed) if g[1] <= start), key=lambda g: g[1])
    body = packed[4:at] + lzs_pack(data, pos)[4:]
    return len(body).to_bytes(4, "little") + body


def maps(disc: Path) -> list[Path]:
    return sorted(p for p in (disc / "FIELD").glob("*.DAT") if p.stat().st_size != 2048)  # 2 KB stubs aren't maps


def module(disc: Path) -> bytes:
    return gzip.decompress((disc / "FIELD/FIELD.BIN").read_bytes()[8:])


PARTS = {"walkmesh": (1, walkmesh_json, walkmesh_raw), "background": (2, background_json, background_raw),
         "camera": (3, camera_json, camera_raw), "map": (4, map_json, map_raw),
         "encounters": (5, encounters_json, encounters_raw), "models": (6, models_json, models_raw)}


def split(dat: bytes) -> list[bytes]:
    at = sections(dat)
    return [dat[at[i]:at[i + 1]] for i in range(7)]


def write_json(path: Path, table: dict):
    """A list of numbers on one line (a battle and its chance, a tile), so a map reads and diffs line by line."""
    text = json.dumps(table, indent=1, ensure_ascii=False)
    text = re.sub(r"\[\s+([-\d,\s]*?)\s+\]", lambda m: "[" + " ".join(m.group(1).split()) + "]", text)
    path.write_text(text + "\n", "utf-8")


def dump(disc: Path, out: Path):
    names = op_names(module(disc))
    out.mkdir(parents=True, exist_ok=True)
    for path in maps(disc):
        secs = split(lzs(path.read_bytes()))
        table = section_json(secs[0], names) | {key: to_json(secs[i]) for key, (i, to_json, _) in PARTS.items()}
        write_json(out / f"{path.stem}.json", table)


def build(folder: Path, assets: Path):
    for disc in kernel.discs(assets):
        changed = []
        for original in sorted((disc / "FIELD").glob("*.DAT.orig")):  # start from the originals: a dump says it all
            changed.append(f"FIELD/{original.stem}")
            kernel.replace(disc, changed[-1], original.read_bytes())
        image = bytearray(module(disc))
        names = op_names(image)
        for path in sorted(folder.glob("*.json")):
            name = f"FIELD/{path.stem}.DAT"
            dat = lzs((disc / name).read_bytes())
            secs, table = split(dat), json.loads(path.read_text("utf-8"))
            try:  # a section the file leaves out stays as it was
                secs[0] = section_raw(table, secs[0], names)
                for key, (i, _, to_raw) in PARTS.items():
                    if key in table:
                        secs[i] = to_raw(table[key])
            except (ValueError, KeyError, TypeError) as e:
                raise SystemExit(f"{path}: {e!r}")
            new = dat_raw(secs)
            if new == dat:
                continue
            if len(new) > ROOM:
                raise SystemExit(f"{path}: the map unpacks to {len(new)} bytes; there's room for {ROOM}")
            kernel.replace(disc, name, lzs_pack(new))
            changed.append(name)
        sizes = {r[3]: (int(r[0]), int(r[1])) for r in (line.split(maxsplit=3) for line in
                                                       (disc / "files.txt").read_text().splitlines())}
        for name in changed:
            lba, size = sizes[name]
            entry = next(at for at in range(MAPS, len(image), 24) if u32(image, at) == lba)  # map: .DAT, .MIM, .BSX
            image[entry + 4:entry + 8] = size.to_bytes(4, "little")
        repack_module(disc, "FIELD/FIELD.BIN", bytes(image))


def repack_module(disc: Path, name: str, image: bytes):
    """A gzip module (8-byte header) with a new image, if it changed: the original again if the image is the
    original's, else packed and padded to the original's size, which the exe reads it by (gzip stops at the end of
    its stream). Several tools patch FIELD.BIN, each only its own entries."""
    original = disc / f"{name}.orig"
    old = (original if original.exists() else disc / name).read_bytes()
    if image == gzip.decompress(old[8:]):
        if old != (disc / name).read_bytes():
            kernel.replace(disc, name, old)
        return
    if image == gzip.decompress((disc / name).read_bytes()[8:]):
        return
    packed = old[:8] + gzip.compress(image, 9, mtime=0)
    if len(packed) > len(old):
        raise SystemExit(f"{disc / name} packs to {len(packed)} bytes, more than its {len(old)}")
    kernel.replace(disc, name, packed.ljust(len(old), b"\0"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("dump", help="every map's scripts and dialogue from a disc folder into a folder of files")
    d.add_argument("disc", type=Path)
    d.add_argument("out", type=Path)
    b = sub.add_parser("build", help="a dump folder into every disc folder's maps")
    b.add_argument("dump", type=Path)
    b.add_argument("assets", type=Path)
    args = ap.parse_args()
    if args.command == "dump":
        dump(args.disc, args.out)
    else:
        build(args.dump, args.assets)


if __name__ == "__main__":
    main()
