import struct

from field import lzs_pack, repack
from unpack import lzs
from world import (BLOCK, CODE, SIZE, assemble, disassemble, file_block, map_meshes, mesh, scripts_json, scripts_raw,
                   terrain_packets, textures, zone_colour)


def words(*ws: int) -> bytes:
    return b"".join(w.to_bytes(2, "little") for w in ws)


# The code is u16 words: operators (00-FF) and commands (300-) take nothing inline, a push (101-1FF) its value,
# goto/ifnot (200/201) a word number from the code's start. A push's type says what the value is: 110 itself, 114
# a bit, 118 a byte, 11C a u16 at that address; the low two bits pick the save data (0), a scratch area (1, 2) or
# a special value (3). Saved bytes and u16s take the field's names.
code = words(0x203, 0x100, 0x110, 300, 0x11C, 0, 0x70, 0x201, 11, 0x119, 0x0A, 0x317, 0x200, 1, 0x203, 0x20D, 0x331)
lines = ["L0000:", "ret", "L0001:", "reset", "push #300", "push story", "eq", "ifnot L000B", "push byte.1[A]",
         "L000B:", "battle", "goto L0001", "ret", "run 9", "op_331"]
assert disassemble(code, {0}) == lines and assemble(lines) == code
assert disassemble(words(0x11B, 0x9, 0x118, 0x10, 0x112, 7, 0x16)) == ["push byte.3[9]", "push play_hours",
                                                                         "push_112 0007", "op_016"]
assert assemble(["push word[10]", "push bit[523]", "push_112 7"]) == words(0x11C, 0x10, 0x114, 0x523, 0x112, 7)
# Commands, the measuring operators and the special values the code shows the meaning of go by name.
named = words(0x11B, 8, 0x330, 0x117, 0x15, 0x110, 50, 0x110, 0, 0x318, 0x19)
assert disassemble(named) == ["push byte.player_id", "select_entity", "push bit.3[15]", "push #50", "push #0",
                              "enter_field", "entity_distance"]
assert assemble(disassemble(named)) == named
for wrong in (["goto L0005"], ["push"], ["push word.4[1]"], ["fly"]):
    try:
        assemble(wrong)
        raise AssertionError(wrong)
    except ValueError:
        pass

# A section: 256 table entries of a u16 key and a word number (entry 0 and the unused FFFF 0000 read nothing), the
# code from 0x400, zeros to 0x7000. Keys: below 4000 a system script, 4000 an entity's function, 8000 a map block's.
section = bytearray(SIZE)
section[4:16] = words(0x0001, 1, 0x4E03, 1, 0x8000 | (5 + 36 * 2) << 4 | 1, 0)
section[16:0x400] = words(0xFFFF, 0) * 252
section[CODE:CODE + len(code)] = code
table = scripts_json(bytes(section))
assert table["scripts"] == [{"key": "0001", "_what": "system 1", "at": "L0001"},
                            {"key": "4E03", "_what": "entity 0E function 3", "at": "L0001"},
                            {"key": "84D1", "_what": "block 5,2 type 4", "at": "L0000"}]
assert table["code"] == lines and scripts_raw(table) == section
table["code"][4] = "push #1"
assert scripts_raw(table)[CODE + 6:CODE + 8] == words(1)

# A container keeps its packed bytes up to the flag byte before what changed; the rest is packed again.
data = bytes(range(256)) * 40
new = data[:6000] + b"changed" + data[6007:]
packed = repack(lzs_pack(data), new, 6000)
assert lzs(packed) == new and packed[4:900] == lzs_pack(data)[4:900]

# A map file: blocks of 0xB800 bytes, each 16 offsets (one per mesh, 4 by 4, row by row) to a u32 length and an
# LZS mesh. A mesh: u16 triangle and vertex counts, the triangles (corners, a byte, 3 UVs, a u16 whose low 9 bits
# pick the texture), the vertices and as many normals (s16 x, y, z, pad). Here vertex 1 of each mesh carries its
# block's number and its own as x and z.
def block(number: int) -> bytes:
    out = bytearray(64)
    for i in range(16):
        raw = words(1, 3) + bytes([0, 1, 2, 0x23, 1, 2, 3, 4, 5, 6]) + words(0x2238)
        raw += struct.pack("<12h", 0, 0, 0, 0, number, -5, i, 0, 0, 0, 8192, 0) + struct.pack("<4h", 0, -4096, 0, 0) * 3
        struct.pack_into("<I", out, 4 * i, len(out))
        packed = lzs_pack(raw)
        out += packed + bytes(-len(packed) % 4)
    return bytes(out) + bytes(BLOCK - len(out))


one = mesh(lzs(block(7)[64:]))
assert one["triangles"] == [(0, 1, 2, 0x23, (1, 2), (3, 4), (5, 6), 0x2238)]
assert one["vertices"] == [(0, 0, 0), (7, -5, 0), (0, 0, 8192)] and one["normals"] == [(0, -4096, 0)] * 3
# The world is 9 by 7 blocks (36 by 28 meshes). map_stage swaps six of them for the six after them as the story
# goes on (func_800A54F0): 0x32 from stage 1, 0x29 and 0x2A from 2, 0x3C from 3, 0x2F and 0x30 from 4.
world = b"".join(block(n) for n in range(69))
assert [file_block(0, b, 2) for b in (0x29, 0x2A, 0x32, 0x3C, 0x2F, 0)] == [0x40, 0x41, 0x3F, 0x3C, 0x2F, 0]
assert [file_block(0, 0x30, s) for s in range(5)] == [0x30, 0x30, 0x30, 0x30, 0x44]
assert [file_block(0, b, 1) for b in (0x29, 0x2A, 0x32)] == [0x29, 0x2A, 0x3F]
assert [file_block(0, 0x3C, s) for s in (2, 3)] == [0x3C, 0x42]
meshes = map_meshes(world, 0, 0)
assert len(meshes) == 36 * 28 and meshes[(22, 15)]["vertices"][1][::2] == (32, 14)  # block 3·9 + 5, mesh 3·4 + 2
assert [map_meshes(world, 0, s)[(12, 20)]["vertices"][1][0] for s in (3, 4)] == [0x30, 0x44]
# Maps 2 and 3 tile the world with 3 by 4 and 2 by 2 blocks of their own; map 3's reach only some of it.
assert [file_block(2, b, 0) for b in (0, 3, 18, 26, 62)] == [6, 6, 0, 2, 2]
assert [file_block(3, b, 0) for b in (0, 1, 9, 10, 20, 8)] == [0, 1, 2, 3, 0, 1] and file_block(3, 36, 0) < 0
assert len(map_meshes(world[:4 * BLOCK], 3, 0)) < 36 * 28

# A container's section 2 is the textures: 512 words (a palette in the low half, a page in the high) and images to
# load (u32 length, x, y, w, h, the pixels), a 0 length after the last.
section2 = struct.pack("<512I", *range(512)) + struct.pack("<I4h2H", 16, 1, 2, 2, 1, 5, 6) + bytes(4)
table, images = textures(struct.pack("<4I", 3, 12, 12, 12) + section2)
assert table == list(range(512)) and images == [(1, 2, 2, 1, struct.pack("<2H", 5, 6))]


# Light zones (func_800AF3A4): one within its inner reach (here 1·1024, by x + y + z distances, wrapping round the
# world) sets light 0's colour to 7× its own; between that and its outer reach (2·1024 further) the colour fades to
# the base one; past it, the base. The last zone in reach wins.
base = (1774, 1774, 1774)
zone = (1000, 0, 0, 10, 20, 30, 2, 1)
assert [zone_colour([zone], (x, 0, 0), base) for x in (1500, 1000 + 1024 + 1024, 1000 + 3072, 0x48000 - 24)] == [
    (70, 140, 210), (1774 // 2 + 35, 887 + 70, 887 + 105), base, (70, 140, 210)]
assert zone_colour([zone, (0, 0, 0, 1, 1, 1, 2, 1)], (500, 0, 0), base) == (7, 7, 7)


def s16(x: int) -> int:
    return ((x & 0xFFFF) ^ 0x8000) - 0x8000


# The terrain as func_800A71E8 draws a mesh, with a GTE that projects (x, y, z) to (x, y) at depth z + 1000 and
# lights a corner by its normal's y: the vertices go to the scratchpad, each at depth (SZ >> 2) past the curve (5000
# here) dropped by ((that >> 6) + (|SX - 160| >> 5))², and are projected again; a triangle goes when some corner is
# on screen across and some down, and it faces the camera (nclip >= 0). Its bucket is its farthest corner >> 4, its
# packet the lit colours with the packet's code, and its texture's palette and page.
class Gte:
    def __init__(self):
        self.data, self.ctrl = [0] * 32, [0] * 32

    def gte_write(self, reg: int, x: int):
        self.data[reg] = x & 0xFFFFFFFF

    def gte_read(self, reg: int) -> int:
        return self.data[reg]

    def gte_write_ctrl(self, reg: int, x: int):
        self.ctrl[reg] = x & 0xFFFFFFFF

    def gte_cmd(self, op: int):
        def vector(i):
            return [s16(self.data[2 * i]), s16(self.data[2 * i] >> 16), s16(self.data[2 * i + 1])]
        if op == 0x0480012:  # the rotation (identity here) of V0 plus the translation
            self.data[25:28] = [(c + t) & 0xFFFFFFFF for c, t in zip(vector(0), self.ctrl[5:8])]
        elif op in (0x0180001, 0x0280030):
            for x, y, z in [vector(i) for i in ((0,) if op == 0x0180001 else (0, 1, 2))]:
                self.data[12:15] = self.data[13:15] + [(y & 0xFFFF) << 16 | x & 0xFFFF]
                self.data[16:20] = self.data[17:20] + [z + 1000]
        elif op == 0x1400006:
            (x0, y0), (x1, y1), (x2, y2) = [(s16(s), s16(s >> 16)) for s in self.data[12:15]]
            self.data[24] = ((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)) & 0xFFFFFFFF
        elif op == 0x0D80420:
            self.data[20:23] = [self.data[6] & 0xFF000000 | -vector(i)[1] >> 5 & 0xFF for i in range(3)]


# Vertex 2 is at depth 31000: (7750 - 5000) >> 6 = 42 and |0 - 160| >> 5 = 5, so it drops 47² = 2209, to y -2159:
# off screen down, as vertex 1 is across. The first triangle faces away.
flat = {"triangles": [(0, 1, 2, 0, (0, 0), (8, 0), (0, 8), 0x205), (0, 2, 1, 0, (0, 0), (0, 8), (8, 0), 5)],
        "vertices": [(0, 0, 3000), (500, 0, 3000), (0, 50, 30000)],
        "normals": [(0, -4096, 0), (0, -2048, 0), (0, 0, 0)]}
tex = [0] * 512
tex[5] = 0x0017798F
packets = terrain_packets(Gte(), {(0, 0): flat}, tex, rotation=[4096, 0, 0, 0, 4096, 0, 0, 0, 4096],
                          translation=(0, 0, 0), focus=(0, 0, 0), curve=5000)
assert packets == [(1937, [0x34000080, 0, 0x798F0000, 0x34000000, 0xF7910000, 0x00170800, 0x34000040, 500,
                           0x00050008])]
print("ok")
