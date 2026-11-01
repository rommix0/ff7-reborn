import random

from field import LOAD, HANDLERS, PRINTER, assemble, dat_raw, disassemble, lzs_pack, op_names, section_json, \
    section_raw
from kernel import encode
from unpack import lzs

names = [f"op_{op:02X}" for op in range(256)]
names[0x00], names[0x0F], names[0x10], names[0x12], names[0x14], names[0x28], names[0x40], names[0x80], names[0x95] = \
    "ret", "spcal", "skip", "back", "if", "kawai", "mes", "set", "inc"

# A script disassembles to a line per operation: its name, then its operand bytes, a jump's as a label named by its
# offset in the script (one may land at the script's end). spcal's lengths go by its sub-code; kawai's length byte
# is left out and worked out again.
code = bytes.fromhex("140102030009 400005 2805aabbcc 0ff601020304 1001 1202 1001")
lines = ["if #2 == var[1][03] L000E", "mes 00 05", "kawai AA BB CC", "L000E:", "spcal F6 01 02 03 04", "L0014:",
         "skip L0016", "L0016:", "back L0014", "skip L001A", "L001A:"]
assert disassemble(code, names) == lines and assemble(lines, names) == code

# Operands the handlers read through a bank nibble (bytes 1-3) show as a literal (#, bank 0) or a variable
# (var[bank][address]); the nibbles go back into their bytes. The if family puts its operator between its values;
# mes shows the string it opens. An operation these can't say exactly stays in hex.
code = bytes.fromhex("80122B07 14102B050001 0707 400005 950210 953210")
lines = ["set var[1][2B] var[2][07]", "if var[1][2B] == #5 L000A", "L000A:", "retto 07", "mes 00 05 ; Hello",
         "inc var[2][10]", "inc 32 10"]
names[0x07] = "retto"
assert disassemble(code, names, ["Hello"] * 6) == lines and assemble(lines, names) == code
assert assemble(["set var[1][2B] #-1", "if var[1][2B] >= var[2][03] L0000", "L0000:"], names) == \
    bytes.fromhex("80102BFF 14122B030401")
assert assemble(["kawai 01 02 03 04", "skip 05"], names) == bytes.fromhex("280601020304 1005")
# A saved variable the game's code shows the meaning of goes by its name, both ways.
names[0x16] = "if2"
assert disassemble(bytes.fromhex("162000002C010401"), names) == ["if2 story >= #300 L0008", "L0008:"]
assert assemble(["if2 story >= #300 L0008", "L0008:"], names) == bytes.fromhex("162000002C010401")
for wrong in (["mes 00"], ["mes 00 L0000", "L0000:"], ["skip L0200"] + ["mes 00 00"] * 90 + ["L0200:"]):
    try:
        assemble(wrong, names)
        raise AssertionError(wrong)
    except ValueError:
        pass

# The field module's debug names: each handler (table at 0x800E0228) shows its name through the debug printer.
image = bytearray(HANDLERS + 1024 + 0x300)
image[0x100:0x110] = b"".join(w.to_bytes(4, "little") for w in
                              (0x3C04800A, 0x24840200, 0x0C000000 | (PRINTER & 0x0FFFFFFF) >> 2, 0))
image[0x200:0x204] = b"req\0"
for op in range(256):
    image[HANDLERS + 4 * op:HANDLERS + 4 * op + 4] = (0x800A0100 if op < 2 else 0x800A0110).to_bytes(4, "little")
assert op_names(bytes(image))[:3] == ["req", "req_01", "op_02"]

# Section 1: a header, the entities' names, the music offsets, 32 script offsets per entity, the scripts, the
# dialogue (a count, offsets, the strings, each ending FF), then the music. A dump holds the names, scripts and
# strings; a build keeps the rest from the original, the music moved on by whole words.
def section(strings: list[bytes]) -> bytes:
    sec = bytearray(b"\x02\x05\x02\x01\0\0\x01\0" + bytes(24)) + b"one\0\0\0\0\0two\0\0\0\0\0" + bytes(4)
    table, scripts = len(sec), [bytes.fromhex("400005 00"), bytes.fromhex("00")]
    sec += bytes(128)
    at = len(sec)
    for e, slots in enumerate(([0] * 32, [0] + [1] * 31)):
        for s, k in enumerate(slots):
            script_at = at + sum(map(len, scripts[:k]))
            sec[table + 64 * e + 2 * s:table + 64 * e + 2 * s + 2] = script_at.to_bytes(2, "little")
    sec += b"".join(scripts)
    sec[4:6] = len(sec).to_bytes(2, "little")
    offsets = [2 + 2 * len(strings) + sum(map(len, strings[:i])) for i in range(len(strings))]
    sec += len(strings).to_bytes(2, "little") + b"".join(o.to_bytes(2, "little") for o in offsets) + b"".join(strings)
    sec += bytes(-len(sec) % 4)
    sec[0x20 + 16:0x24 + 16] = len(sec).to_bytes(4, "little")
    return bytes(sec + b"AKAO" + bytes(12))

wait = encode("there") + b"\xfe\xdd\xff\x05\xff"  # a code's argument can be FF; the next offset ends the string
original = section([wait, encode("Hi") + b"\xff"])
table = section_json(original, names)
assert table["entities"] == [{"name": "one", "scripts": [0] * 32}, {"name": "two", "scripts": [0] + [1] * 31}]
assert table["scripts"] == [["mes 00 05", "ret"], ["ret"]] and table["strings"] == ["there{FE DD}{FF}%", "Hi"]
assert section_raw(table, original, names) == original
table["strings"][1] = "Hello"
assert section_raw(table, original, names) == section([wait, encode("Hello") + b"\xff"])

# The DAT's header points at its seven sections in RAM; a new section 1 moves the rest along.
dat = b"".join((LOAD + at).to_bytes(4, "little") for at in (0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34)) + bytes(28)
moved = dat_raw(dat, bytes(12))
assert [int.from_bytes(moved[4 * i:4 * i + 4], "little") - LOAD for i in range(7)] == [0x1C, 0x28, 0x2C, 0x30, 0x34,
                                                                                          0x38, 0x3C]

# LZS: what unpack.lzs reads back, smaller when it repeats.
noise = random.Random(7)
for data in (bytes(noise.randrange(256) for _ in range(5000)), b"FINAL FANTASY " * 900 + bytes(3000), b"", b"ab"):
    assert lzs(lzs_pack(data)) == data
assert len(lzs_pack(b"FINAL FANTASY " * 900)) < 14 * 900 // 8  # a 2-byte reference covers up to 18 bytes
print("ok")
