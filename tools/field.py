"""Dump the field maps' scripts and dialogue (FIELD/*.DAT) to files people can edit, and build them back.

  python tools/field.py dump "assets/Final Fantasy VII (USA) (Disc 1)" field
  python tools/field.py build field assets

A JSON file per map: its entities (a name and, for each of their 32 script slots, which script runs), the scripts a
line per operation (the field module's own name for it, then its operand bytes in hex, jumps to labels), and the
dialogue in the text format tools/kernel.py describes. Operations are named from the disc's FIELD.BIN, op_XX where
it has no name. build writes the maps that differ from the originals into every disc folder under assets, keeping
the first originals as .orig, and FIELD.BIN's table of where each map lies; the rest of a map comes from its
original. A map unpacks to at most 77,852 bytes (the background is unpacked right after it).
"""
import argparse
import gzip
import json
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
            out.append(f"var[{bank}][{value:02X}]" if bank else f"#{value}")
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
    nice = any(w.startswith(("#", "var[")) or w in COMPARE for w in words)
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
            elif word.startswith("var[") and word.endswith("]") and "][" in word:
                bank, value = int(word[4:word.index("]")]), int(word[word.index("][") + 2:-1], 16)
            else:
                raise ValueError(f"{word} isn't #value or var[bank][address]")
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


def sections(dat: bytes) -> list[int]:
    return [u32(dat, 4 * i) - LOAD for i in range(7)] + [len(dat)]


def dat_raw(dat: bytes, sec1: bytes) -> bytes:
    at = sections(dat)
    shift = len(sec1) - (at[1] - at[0])
    header = b"".join((LOAD + o + (shift if i else 0)).to_bytes(4, "little") for i, o in enumerate(at[:7]))
    return header + sec1 + dat[at[1]:]


def lzs_pack(data: bytes) -> bytes:
    """LZSS as unpack.lzs reads it: a flag byte per eight items (1 a literal), a reference 12 bits of window position
    and 4 of length less 3. Greedy, the longest match in the window."""
    out, chains, i = bytearray(), {}, 0
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


def maps(disc: Path) -> list[Path]:
    return sorted(p for p in (disc / "FIELD").glob("*.DAT") if p.stat().st_size != 2048)  # 2 KB stubs aren't maps


def module(disc: Path) -> bytes:
    return gzip.decompress((disc / "FIELD/FIELD.BIN").read_bytes()[8:])


def dump(disc: Path, out: Path):
    names = op_names(module(disc))
    out.mkdir(parents=True, exist_ok=True)
    for path in maps(disc):
        dat = lzs(path.read_bytes())
        at = sections(dat)
        table = section_json(dat[at[0]:at[1]], names)
        (out / f"{path.stem}.json").write_text(json.dumps(table, indent=1, ensure_ascii=False) + "\n", "utf-8")


def build(folder: Path, assets: Path):
    for disc in kernel.discs(assets):
        for original in sorted((disc / "FIELD").glob("*.orig")):  # start from the originals: a dump says it all
            kernel.replace(disc, f"FIELD/{original.stem}", original.read_bytes())
        image = bytearray(module(disc))
        names = op_names(image)
        for path in sorted(folder.glob("*.json")):
            name = f"FIELD/{path.stem}.DAT"
            dat = lzs((disc / name).read_bytes())
            at = sections(dat)
            try:
                sec1 = section_raw(json.loads(path.read_text("utf-8")), dat[at[0]:at[1]], names)
            except ValueError as e:
                raise SystemExit(f"{path}: {e}")
            if sec1 == dat[at[0]:at[1]]:
                continue
            new = dat_raw(dat, sec1)
            if len(new) > ROOM:
                raise SystemExit(f"{path}: the map unpacks to {len(new)} bytes; there's room for {ROOM}")
            data = lzs_pack(new)
            lba = kernel.replace(disc, name, data)
            entry = next(at for at in range(MAPS, len(image), 24) if u32(image, at) == lba)  # map: .DAT, .MIM, .BSX
            image[entry + 4:entry + 8] = len(data).to_bytes(4, "little")
        if bytes(image) == module(disc):
            continue
        old = (disc / "FIELD/FIELD.BIN").read_bytes()
        packed = old[:8] + gzip.compress(bytes(image), 9, mtime=0)
        if len(packed) > len(old):  # the exe reads it by its original size
            raise SystemExit(f"{disc}: FIELD.BIN packs to {len(packed)} bytes, more than its {len(old)}")
        kernel.replace(disc, "FIELD/FIELD.BIN", packed.ljust(len(old), b"\0"))


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
