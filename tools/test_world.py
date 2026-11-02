import math
import struct

from field import lzs_pack, repack
from unpack import lzs
from world import (BLOCK, CODE, SIZE, assemble, camera, disassemble, file_block, light_zones, map_meshes, mesh,
                   scripts_json, scripts_raw, sky_packets, sky_scroll, terrain_packets, textures, Zone,
                   zone_colours, world_models, pose, light_model, model_packets, shadow_packet,
                   model_images, SLOTS, TINTS, entity_translation, model_slots, quarter, tinted)


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
# world) sets light 0's colour to 7× its own and the sky's top and bottom to its own; between that and its outer
# reach (2·1024 further) they fade to the base ones; past it, the base. The last zone in reach wins.
base = ((1774, 1774, 1774), (0, 0, 255), (250, 255, 255))
zone = Zone(1000, 0, 0, (10, 20, 30), (200, 100, 0), (0, 0, 40), 2, 1)
assert [zone_colours([zone], (x, 0, 0), *base) for x in (1500, 1000 + 1024 + 1024, 1000 + 3072, 0x48000 - 24)] == [
    ((70, 140, 210), (200, 100, 0), (0, 0, 40)), ((922, 957, 992), (100, 50, 127), (125, 127, 147)), base,
    ((70, 140, 210), (200, 100, 0), (0, 0, 40))]
other = Zone(0, 0, 0, (1, 1, 1), (1, 1, 1), (1, 1, 1), 2, 1)
assert zone_colours([zone, other], (500, 0, 0), *base) == ((7, 7, 7), (1, 1, 1), (1, 1, 1))


# The zones are what the point_ commands set (func_800AF1A8 onward, each popping its arguments in push order):
# point_select N, on; point_block and point_pos (x, z: high and low 13 bits); point_rgb1, 2 and 3 (red, green, blue:
# light, sky top, sky bottom); point_range (outer, inner). A select of -1 leaves on as it was; zones left off aren't
# lights.
code = ["reset", "push #1", "push #1", "point_select", "reset", "push #22", "push #14", "point_block", "reset",
        "push #5381", "push #5345", "point_pos", "reset", "push #60", "push #90", "push #100", "point_rgb1", "reset",
        "push #12", "push #10", "point_range", "push #0", "push #0", "push #65", "point_rgb2", "push #145",
        "push #255", "push #190", "point_rgb3", "reset", "push #3", "push #0", "point_select", "push #9", "push #9",
        "point_block"]
assert light_zones(code) == [Zone(22 * 8192 + 5381, 0, 14 * 8192 + 5345, (60, 90, 100), (0, 0, 65), (145, 255, 190),
                                  12, 10)]

# The camera (func_800A1DF0): a view's tilt and distance (D_800C6628, D_800C6638), the eye that tilt puts the
# distance away, and its height over the focus's give the pitch through the arctangent table D_800BE1E8; the
# rotation is RotMatrix(pitch, yaw, 0) after the roll. RotMatrix's sines here are round(4096 sin).
sines = [v for i in range(4096) for v in (round(4096 * math.sin(i * math.pi / 2048)),
                                          round(4096 * math.cos(i * math.pi / 2048)))]
arctan = list(range(512))
# Tilted 45°, 1000 away: the eye is at y -708, z 707. t = (16·-708 - height) · 256 / (16·707), held to 0-511.
assert camera(sines, arctan, 512, 1000, 0, 0) == (0x800, [[4096, 0, 0], [0, -4096, 0], [0, 0, -4096]])
assert [camera(sines, arctan, 512, 1000, h, 0)[0] for h in (-20000, -100000)] == [0x800 + 196, 0x800 + 511]
assert camera(sines, arctan, 512, 1000, 0, 1024)[1][0] == [0, 0, 4096]
assert camera(sines, arctan, 512, 1000, 0, 0, 1024)[1] == [[0, 4096, 0], [4096, 0, 0], [0, 0, -4096]]


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
        elif op == 0x158002D:  # avsz3: the average here
            self.data[7] = sum(self.data[17:20]) // 3
        elif op == 0x108041B:  # nccs: the normal's x as red
            self.data[22] = self.data[6] & 0xFF000000 | vector(0)[0] & 0xFF


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


# The sky (func_800AE638, func_800AEA48, linked by func_800A141C): the horizon is where a point 0x4000 ahead of the
# focus lands, plus 26 rows (here the stand-in GTE puts it at row 0). The gradient (no dither) runs from 24 rows above
# the screen to it; the clouds, a strip 64 rows high 63 rows above it ((5000 >> 8) + 44), scrolled: two quads (u from
# 0xC0 - scroll and from 0) or, from scroll 0xC0, three. Drawn last linked first.
def xy(x: int, y: int) -> int:
    return (y & 0xFFFF) << 16 | x & 0xFFFF


identity = [4096, 0, 0, 0, 4096, 0, 0, 0, 4096]
sky = sky_packets(Gte(), sines, identity, (0, 0, 0), 0, 0, 0, 5000, (0, 0, 65), (145, 255, 190))
assert sky == [[0xE1000000, 0], [0x38410000, xy(-20, -24), 0x410000, xy(340, -24), 0xBEFF91, xy(-20, 26), 0xBEFF91,
                                 xy(340, 26)], [0xE1000200, 0],
               [0x2E404040, xy(64, -61), 0x7B0FC000, xy(320, -61), 0x39C0FF, xy(64, 3), 0xFF00, xy(320, 3), 0xFFFF],
               [0x2E404040, xy(0, -61), 0x7B0FC0C0, xy(64, -61), 0x39C0FF, xy(0, 3), 0xFFC0, xy(64, 3), 0xFFFF]]
clouds = sky_packets(Gte(), sines, identity, (0, 0, 0), 0, 0, 0, 5000, (0, 0, 0), (0, 0, 0), 200)[3:]
assert [(p[1] & 0xFFFF, p[2] & 0xFF, p[4] & 0xFF) for p in clouds] == [(264, 0, 55), (8, 0, 255), (0, 247, 255)]
# The scroll follows the yaw: (yaw - last) · curve / 10000 off the scroll (wrapping at 256), last moved by as much.
assert sky_scroll(0, 0, 100, 5000) == (206, 100) and sky_scroll(0, 0, 4000, 5000) == (48, 4000)

# A model section (WM0.BSZ's section 0, a container's section 0): a count, a pointer to the first 0x24-byte header and
# a size; every pointer is already where the file loads (here 0x80100000). A header: on, mode, bone, part and
# animation counts, root angles, the load scale at 0x16 (0x200: a length or vertex v becomes v · 0x200 >> 12), the
# parts' and animations' offsets from the base pointer at 0x1C; the bones (s16 length, s8 parent, a byte) at the base.
# An animation: frames, then counts and offsets of its s16 tracks, s16 statics and byte (angle) tracks, and its data:
# a word, then per bone a byte of flags (bits 0-2: angle x, y, z from a track; 4-6: translation x, y, z from one),
# 3 angles (256ths) or track numbers, 3 translation numbers (0xFF none). A part: on, bone, vertex count, 0, its counts
# of GT4, GT3, FT4, FT3, F3, F4, G3, G4, its polygons', UVs', textures' and texture bytes' offsets, the packet size
# and its data: a word, the vertices, the polygons (corners, then colours: RGB and a normal number).
BASE = 0x80100000
blob = bytearray(0x200)
struct.pack_into("<3I", blob, 0, 1, BASE + 12, 0x200)
struct.pack_into("<4B4b", blob, 12, 1, 0xFF, 1, 1, 1, 0, 0, 0)
struct.pack_into("<hHHI", blob, 12 + 0x16, 0x200, 0x10, 0x40, BASE + 0x60)
struct.pack_into("<hbB", blob, 0x60, -80, -1, 1)  # one bone, -80 long
struct.pack_into("<H4B3HI", blob, 0x60 + 0x40, 1, 1, 0, 1, 0, 0, 0x10, 0, BASE + 0x100)
struct.pack_into("<I8B", blob, 0x100, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0)  # bone 0: z from static 0
struct.pack_into("<h", blob, 0x110, 160)
struct.pack_into("<4B8B2x5HI", blob, 0x60 + 0x10, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0, 28, 0, 0, 0, 0x1C, BASE + 0x180)
struct.pack_into("<I12h4B3I", blob, 0x180, 0, 0, 0, 0, 0, 80, 0, 0, 0, 0, 80, 0, 0, 0, 1, 2, 0,
                 0x05332211, 0x05332211, 0x05665544)
cloud = world_models(bytes(blob), BASE, 0)[0]
assert cloud["bones"] == [(-10, -1, 1)] and cloud["parts"][0]["vertices"] == [(0, 0, 0), (10, 0, 0), (0, 10, 0)]
assert cloud["anims"][0]["statics"] == [20] and cloud["parts"][0]["polygons"][0]["kind"] == "G3"
# Posed (func_800C1490): the root turned by its angles (x 0 here) and then 90° about x, so the bone runs along y; a
# bone's translation is its length plus its animation's along its own z.
assert pose(cloud, [[4096, 0, 0], [0, 4096, 0], [0, 0, 4096]], (0, 0, 1000), 0, 0, (0, 0, 0)) == [
    ([[4096, 0, 0], [0, 0, 4096], [0, -4096, 0]], [0, 10, 1000])]
# Lit once at load (func_800B6724): each colour through NCCS with its normal (here a stand-in that gives the normal's
# x as red), the normal number kept.
light_model(Gte(), cloud, [(7, 0, 0)] * 256)
assert cloud["parts"][0]["polygons"][0]["colours"] == [0x05000007] * 3
# Drawn (func_800C0B48): the part's vertices projected, a polygon facing the camera linked at its average depth.
drawn = model_packets(Gte(), cloud, [([[4096, 0, 0], [0, 4096, 0], [0, 0, 4096]], [0, 0, 0])])
assert drawn == [(1000, [0x30000007, xy(0, 0), 0x000007, xy(10, 0), 0x000007, xy(0, 10)])]
# The shadow (func_800B5C7C, func_800B59F4): a quad of the given half sizes at the ground, turned by the yaw, a
# subtracting texture (palette 0x7CC4, page 0x59) over the rectangle given, linked at its nearest corner.
assert shadow_packet(Gte(), sines, identity, (0, 0, 0), (0, 0), 0, 307, 0, (100, 100), 0x20, (0x18, 0x38, 15, 15)) \
    == (56, [0x2E202020, xy(-100, 307), 0x7CC43818, xy(100, 307), 0x593827, xy(-100, 307), 0x4718, xy(100, 307),
             0x4727])
# A model section's images (WM0.BSZ's section 1, func_800C0808): a count at 4, then w, h, x, y and an offset from
# the section's start per image.
images = bytes(4) + bytes([1, 0, 0, 0]) + struct.pack("<4HI", 2, 1, 768, 256, 20) + struct.pack("<2H", 7, 8)
assert model_images(images, 0) == [(768, 256, 2, 1, struct.pack("<2H", 7, 8))]

# The other models: a container's row of SLOTS gives each id 3-31 its header, 1-based (-1: none).
module = bytearray(0x30000)
module[SLOTS - 0x800A0000 + 32 + 3:SLOTS - 0x800A0000 + 32 + 6] = bytes([3, 0xFF, 1])
assert model_slots(bytes(module), 1) == {3: 2, 5: 0}
# A tint (TINTS: s16 r, g, b) added to every colour, each channel held to 0-255, the top byte kept.
struct.pack_into("<3h", module, TINTS - 0x800A0000 + 8 * 4, 69, 17, -147)
assert tinted(bytes(module), cloud, 4)["parts"][0]["polygons"][0]["colours"] == [0x0500114C] * 3
# An entity's place (func_800AAB18, func_800A36AC): its offset from the focus (wrapped round the world: 0x30000 across
# is -0x18000), y less the camera's height plus its lift, less the curve's drop, all in quarters toward 0, turned by
# the camera and added to its translation in quarters. The drop (func_800AA8F8): with the stand-in, the point at
# depth 31000 is (7750 - 5000)^2 >> 12 = 1846 past the curve, plus 100^2 >> 10 = 9, over 4: 463. The player has
# no lift and no drop.
assert [quarter(v) for v in (-5, 5, -463)] == [-1, 1, -115]
assert entity_translation(Gte(), identity, (0, 0, 4000), 50, 5000, (100, 50, 30000)) == [25, -115, 8500]
assert entity_translation(Gte(), identity, (0, 0, 4000), 50, 5000, (0x30000, 90, 0), 8, True) == [-0x6000, 10, 1000]
print("ok")
