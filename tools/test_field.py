import random

from field import LOAD, HANDLERS, PRINTER, assemble, background_json, background_raw, camera_json, camera_raw, \
    dat_raw, disassemble, encounters_json, encounters_raw, lzs_pack, map_json, map_raw, models_json, models_raw, \
    op_names, section_json, section_raw, split, walkmesh_json, walkmesh_raw
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

# The DAT's header points at its seven sections in RAM, which lie back to back.
dat = dat_raw([bytes(12)] + [bytes(4)] * 6)
assert [int.from_bytes(dat[4 * i:4 * i + 4], "little") - LOAD for i in range(7)] == [0x1C, 0x28, 0x2C, 0x30, 0x34,
                                                                                        0x38, 0x3C]
assert split(dat) == [bytes(12)] + [bytes(4)] * 6

# Section 2, the walkmesh: a count, the triangles (three x, y, z, w), then each one's neighbour across each edge (-1
# a wall), padded to a word with what was there.
mesh = bytes.fromhex("0100 0000" "0a00 1400 f6ff 0000 0b00 1500 0000 0000 0c00 1600 0000 0000" "ffff 0000 ffff 7272")
assert walkmesh_json(mesh) == {"triangles": [{"vertices": [[10, 20, -10, 0], [11, 21, 0, 0], [12, 22, 0, 0]],
                                              "neighbours": [-1, 0, -1]}], "pad": 0x7272}
assert walkmesh_raw(walkmesh_json(mesh)) == mesh

# Section 3, the background: four run lists (a row's y, its first tile's index across all layers, the count; 7FFE
# the next texture page; 7FFF the end), then layer 1's tiles, the pages, layer 2's tiles (each with its own page and
# depth), layers 3 and 4's. Tiles after layer 1 have a group, a blend bit and the group's state bits to show in.
bg = {"layers": [[{"page": 0x1F}, [[0, 16, 32, 48, 0x7800], [16, 16, 48, 48, 0x7800]]],
                 [[[0, 32, 0, 16, 0x7801, 0x3E, 500, 3, 1, 2]]], [[[0, 0, 32, 64, 0x7802, 0, 0, 0]]], []]}
sec = background_raw(bg)
runs = bytes.fromhex("fe7f 0000 0000" "1000 0000 0200 ff7f" "2000 0200 0100 ff7f" "0000 0300 0100 ff7f ff7f")
head = 0x10 + len(runs)
assert [int.from_bytes(sec[4 * i:4 * i + 4], "little") for i in range(4)] == [head, head + 16, head + 18, head + 32]
assert sec[0x10:head] == runs and sec[head + 16:head + 18] == b"\x1f\0"
assert sec[head + 18:head + 32] == bytes.fromhex("0000 2000 00 10 0178 3e00 f401 83 02")
assert background_json(sec) == bg

# Section 4, the camera: a rotation (4.12), a word the field doesn't read, the translation, the screen offset, the
# projection distance, then bytes the field doesn't read.
cam = {"rotation": [[4096, 0, 0], [0, 4096, 0], [0, 0, -4096]], "pad": -4096, "translation": [-365, 3279, -27031],
       "offset": [0, 0], "distance": 386, "unread": "44 80"}
assert camera_json(camera_raw(cam)) == cam and len(camera_raw(cam)) == 40

# Section 5, the map: its name, how the pad turns, the camera's focus and scroll box, layers 3 and 4's wrap, start
# and parallax, 12 exits (a line, where it leads; map 7FFF unused), an arrow flag per exit, 12 triggers (a line and
# the background group bit it sets or clears; group FF unused) and 12 markers.
exit_ = {"line": [[1, 2, 3], [4, 5, 6]], "map": 117, "x": -10, "y": 20, "triangle": 7, "facing": 128}
trigger = {"line": [[0, 0, 0], [9, 9, 9]], "group": 1, "bit": 2, "kind": 5, "sound": 3}
m = {"name": "md1stin", "turn": 124, "focus": -16, "scroll": [-256, -256, 256, 256], "scroll_mode": 0,
     "layers": [{"wrap": [512, 256], "start": [0, 0], "parallax": [256, 128]}] * 2, "unread": " ".join(["00"] * 12),
     "exits": [exit_] + [None] * 11, "arrows": [1] * 12, "triggers": [None, trigger] + [None] * 10,
     "markers": [[-1086, 0, 5, 2]] + [[0, 0, 0, 0]] * 11}
sec = map_raw(m)
assert len(sec) == 740 and sec[:10] == b"md1stin\0\0\x7c" and map_json(sec) == m
assert sec[0x38:0x50] == bytes.fromhex("010002000300 040005000600 f6ff 1400 0700 7500 80808080")
assert sec[0x50 + 0x12:0x50 + 0x14] == b"\xff\x7f" and sec[0x158 + 12] == 0xFF
assert sec[0x168:0x178] == bytes.fromhex("000000000000 090009000900 01020503")
assert sec[0x224:0x234] == bytes.fromhex("c2fb0000 00000000 05000000 02000000")

# Section 6, two encounter tables: on, a rate (bigger, rarer), six battles with chances out of 64, four special ones.
tables = [{"on": 1, "rate": 128, "battles": [[301, 24], [302, 22], [303, 18], [0, 0], [0, 0], [0, 0]],
           "special": [[304, 4], [304, 4], [0, 0], [0, 0]]},
          {"on": 0, "rate": 0, "battles": [[0, 0]] * 6, "special": [[0, 0]] * 4}]
assert encounters_raw(tables)[:6] == bytes.fromhex("0180 2d61 2e59") and len(encounters_raw(tables)) == 48
assert encounters_json(encounters_raw(tables)) == tables
tables[0]["battles"][0][1] = 64  # a chance has 6 bits; the sixth battle takes what the others leave
for wrong in (tables, tables[:1]):
    try:
        encounters_raw(wrong)
        raise AssertionError(wrong)
    except ValueError:
        pass

# Section 7, the models: its size, a count, 8 bytes each, two of them filled in at load.
models = [{"face": 2, "bones": 22, "parts": 15, "animations": 4, "load": 1, "bcx": 3}]
assert models_raw(models) == bytes.fromhex("0c00 0100 02160f04 00010003") and models_json(models_raw(models)) == models

# LZS: what unpack.lzs reads back, smaller when it repeats.
noise = random.Random(7)
for data in (bytes(noise.randrange(256) for _ in range(5000)), b"FINAL FANTASY " * 900 + bytes(3000), b"", b"ab"):
    assert lzs(lzs_pack(data)) == data
assert len(lzs_pack(b"FINAL FANTASY " * 900)) < 14 * 900 // 8  # a 2-byte reference covers up to 18 bytes
print("ok")
