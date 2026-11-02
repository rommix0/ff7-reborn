import math
import struct

import model
from battle import (OPENINGS, PARTY_FACING, PARTY_THREE, PARTY_TWO, SHADOW, Actor, Channel, Fight, View,
                    action_frame, action_scripts, animation, camera_frame, look_at, model_json, moved, opening,
                    placement, ratan2, rcos, returning, shadow, square_root0, stage_draws, stage_json)


def stream(fields: list[tuple[int, int]]) -> bytes:
    """Fields of (bits, value) packed MSB first."""
    bits = "".join(format(v & (1 << n) - 1, f"0{n}b") for n, v in fields)
    bits += "0" * (-len(bits) % 8)
    return bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8))


# An animation: u16 frames, u16 the bitstream's length, u8 shift, the bitstream (MSB first). Frame 0 is absolute: the
# root's x, y, z in 16 bits each, then each bone's rotation x, y, z in 12 - shift bits, shifted up. Each later frame
# adds deltas: the root's a flag then 16 or 7 bits; a rotation's a flag (0: none), then 3 bits k: 0 is -(1 << shift),
# 7 is 12 - shift bits shifted, else k bits pushed away from 0 by 1 << (k - 1), shifted.
first = [(16, 10), (16, -555), (16, 224), (10, 100), (10, -1), (10, 3)]
second = [(1, 0), (7, -3), (1, 1), (16, 1000), (1, 0), (7, 0),
          (1, 1), (3, 0), (1, 0), (1, 1), (3, 2), (2, -2)]
third = [(1, 0), (7, 0)] * 3 + [(1, 1), (3, 7), (10, 5), (1, 0), (1, 1), (3, 1), (1, 0)]
body = stream(first + second + third)
anim = struct.pack("<HHB", 3, len(body), 2) + body
assert animation(anim, 1) == [((10, -555, 224), [(400, -4, 12)]),
                              ((7, 445, 224), [(396, -4, 12 + (-2 - 2) * 4)]),
                              ((7, 445, 224), [(416, -4, 0)])]

def packed(pieces: list[bytes]) -> bytes:
    at, offsets = 4 + 4 * len(pieces), []
    for piece in pieces:
        offsets.append(at)
        at += len(piece)
    return struct.pack(f"<I{len(pieces)}I", len(pieces), *offsets) + b"".join(pieces)


# An enemy file: the skeleton (a root, a bone of length -50 carrying a part, a bone on it of length -30; the part a
# textured triangle on page 0x25 with palette (0, 480), and a semi-transparent Gouraud quad), settings (no weapon),
# one animation, a TIM.
vertices = struct.pack("<16h", 0, 0, 0, 0, 10, 0, 0, 0, 0, 10, 0, 0, 10, 10, 0, 0)
part = (struct.pack("<I", 32) + vertices + struct.pack("<hH", 1, 0x25) + struct.pack("<3h2B2BH4B", 0, 8, 16, 0x80, 2,
        1, 2, 0x7800, 3, 4, 5, 6) + struct.pack("<I", 0) + struct.pack("<hh", 0, 0) + struct.pack("<I", 1)
        + struct.pack("<4h", 0, 8, 16, 24) + bytes.fromhex("1020303A 405060FF 708090FF A0B0C0FF"))
skeleton = (struct.pack("<HH", 2, 0) + struct.pack("<hhI", 0, 0, 0) + struct.pack("<hhI", 0, -50, 28)
            + struct.pack("<hhI", 1, -30, 0) + part)
still = stream([(16, 0), (16, -100), (16, 0)] + [(12, 0), (12, 1024), (12, 0)] + [(12, 0)] * 6)
tim = struct.pack("<2I", 0x10, 8) + struct.pack("<I4H", 12 + 32, 0, 480, 16, 1) + bytes(32) + struct.pack(
    "<I4H", 12 + 4, 320, 0, 1, 2) + bytes(4)
settings = bytes(6) + struct.pack("<3h", 2560, 1600, -150)  # no weapon; the shadow's size across, along, offset
table = model_json(packed([skeleton, settings, struct.pack("<HHB", 1, len(still), 0) + still, tim]), False)
assert table["shadow"] == (2560, 1600, -150)
assert table["bones"] == [{"length": 0, "parent": -1}, {"length": 0, "parent": 0}, {"length": -50, "parent": 1}]
assert table["parts"] == [{"bone": 1, "vertices": [[0, 0, 0], [10, 0, 0], [0, 10, 0], [10, 10, 0]],
                           "uvs": [[1, 2], [3, 4], [5, 6]], "textures": ["78000502"],
                           "polygons": [{"kind": "FT3", "vertices": [0, 1, 2], "uv": [0, 1, 2, 0], "flags": 0x30,
                                         "colors": ["80808000"]},
                                        {"kind": "G4", "vertices": [0, 1, 2, 3], "flags": 0x10,
                                         "colors": ["1020303A", "405060FF", "708090FF", "A0B0C0FF"]}]}]
assert table["tim"] == [(0, 480, 16, 1, bytes(32)), (320, 0, 1, 2, bytes(4))]
# Posed as the field poses: the root at its animated place, turned a quarter about y (1024 of 4096); a bone at its
# parent's start plus the parent's turn of (0, 0, the parent's length).
(turn, start), (_, child), (_, grandchild) = model.pose(table["bones"], table["animations"][0], 0)
assert [round(x) for x in start] == [0, -100, 0] and [round(x) for x in child] == [0, -100, 0]
assert [round(x) for x in grandchild] == [-50, -100, 0]
assert [round(x, 6) for x in model.apply(turn, (0, 0, 1))] == [1, 0, 0]

# Loaded as enemy model 0, actor 4: the image a page word of 0x12 on (128 right, 256 down), the palette 12 rows down.
loaded = moved(table, 0x12, 12)
assert loaded["parts"][0]["textures"] == [f"{0x78000502 + (2 << 8) + (1 << 12) + (12 << 22):08X}"]
assert loaded["tim"] == [(0, 492, 16, 1, bytes(32)), (448, 256, 1, 2, bytes(4))]


# A stage: type and first in section 0, then parts in world coordinates, the TIM last. Type 1 draws section 1 and
# 5 up to first behind everything (the backdrop), 2-4 in the far layer and first on in the main ordering table with
# the actors; type 5 sorts the backdrop's sections with the main ones.
stage = stage_json(packed([bytes([1, 6]) + bytes(6)] + [part] * 6 + [tim]))
assert (stage["type"], stage["first"], list(stage["parts"])) == (1, 6, [1, 2, 3, 4, 5, 6])
assert stage["parts"][1] == table["parts"][0] | {"bone": 0} and stage["tim"] == table["tim"]
still = (0, 0, 0)
assert [d[:3] for d in stage_draws(1, 6, 7, 0)] == [(1, "backdrop", None), (2, "far", 10), (3, "far", 10),
                                                   (4, "far", 10), (5, "backdrop", None), (6, "main", 2)]
assert [d[1] for d in stage_draws(5, 6, 7, 0)] == ["main", "far", "far", "far", "main", "main"]
# The moving ones, by battle frame: type 3's parts turn about y 1, 2, 4 a frame (already in frame 0); type 2's walls
# drop 100 a frame over 125 and its two cars bob 25 a frame in opposite phase (16 frames each way); type 0's carriage
# jolts 40 every 4 frames (120 every 32) and its scenery scrolls 1000 a frame in z, wrapping as a s16.
assert [d[3] for d in stage_draws(3, 0, 8, 9)][4:] == [(0, 10, 0), (0, 20, 0), (0, 40, 0)]
elevator = [stage_draws(2, 0, 11, frame) for frame in (0, 1, 17, 126)]
assert [e[4][4][1] for e in elevator] == [0, -100, -1700, -100] and elevator[1][-1][4] == (0, 12400, 0)
assert [(e[0][4][1], e[2][4][1]) for e in elevator[1:3]] == [(-25, 25), (375, -375)]
assert [stage_draws(0, 0, 12, frame)[0][4][1] for frame in (0, 1, 2, 17)] == [0, -40, 0, -120]
assert [stage_draws(0, 0, 12, frame)[8][4][2] for frame in (0, 1, 2, 3)] == [30000, 31000, 32000, -32536]

# A shadow: BATTLE.X's disc scaled by the model's size across and along (4096 being 1), at its root's x and z plus
# the offset, flat on the floor; an offset of -1 puts it at the actor's own place.
module = bytearray(0x50000)
module[SHADOW - 0x800A0000:SHADOW - 0x800A0000 + len(part)] = part
assert shadow(module, (5, -300, 7), (8192, 4096, 3))["vertices"] == [[5, 0, 10], [25, 0, 10], [5, 0, 10],
                                                                       [25, 0, 10]]
assert shadow(module, (5, -300, 7), (4096, 4096, -1))["vertices"][1] == [10, 0, 0]

# Places: the party's from BATTLE.X's tables by layout (one member at x 0, two from their own table), facing from
# another (0 in layout 2); enemies at their formation places, turned half round on the far side (z < 0; z >= 0 in
# layouts 3, 5, 6, 7; never in layout 1).
for layout in range(9):
    struct.pack_into("<9h", module, PARTY_THREE - 0x800A0000 + 18 * layout, -10, 1, 20, 0, 2, 21, 10, 3, 22)
    struct.pack_into("<6h", module, PARTY_TWO - 0x800A0000 + 12 * layout, -5, 4, 30, 5, 5, 31)
    struct.pack_into("<3H", module, PARTY_FACING - 0x800A0000 + 6 * layout, 1024, 2048, 3072)
enemies = [{"enemy_id": 16, "x": -500, "y": 0, "z": -1500}, {"enemy_id": 16, "x": 500, "y": 0, "z": 300},
           {"enemy_id": 0xFFFF, "x": -1, "y": -1, "z": -1}]
assert placement(module, {"layout": 0, "formation": enemies}, 1) == [
    ((0, 1, 20), 1024), ((-500, 0, -1500), 2048), ((500, 0, 300), 0)]
assert placement(module, {"layout": 3, "formation": enemies}, 2) == [
    ((-5, 4, 30), 1024), ((5, 5, 31), 2048), ((-500, 0, -1500), 0), ((500, 0, 300), 2048)]
assert [f for _, f in placement(module, {"layout": 2, "formation": enemies}, 3)] == [0, 0, 0, 2048, 0]
assert [f for _, f in placement(module, {"layout": 1, "formation": enemies}, 3)][3:] == [0, 0]

# The camera, in the GTE's integers: looking along +z from (0, 0, -1000), up -y, so a point above the target is up
# the screen (view y < 0). VectorNormal runs long, the op's >> 12 short: z (0, 0, 1000) -> 4095; x (4095, 0, 0) ->
# 4102; y (0, 4095 * 4102 >> 12 = 4100, 0) -> 4100. (Checked against the game's own matrices in three lab dumps.)
rows, t = look_at((0, 0, -1000), (0, 0, 0))
assert rows == [[4102, 0, 0], [0, 4100, 0], [0, 0, 4095]] and t == [0, 0, 1000]

# The opening: camera_script n's eye and target scripts from BATTLE.X's tables. Each battle frame the target's runs,
# then the eye's, then their eases: set a point (F9, FA), ease to the setup's camera over 3 frames (E2), wait 3 (F5,
# F4), end (FF). An ease's first step is in the frame it starts (f 0: no move), then f 1 of 3 on rcos's curve
# ((rcos(682 + 2048) + 4096) = 2044 of 8192 of the way), then the exact place.
assert [rcos(a) for a in (0, 1024, 2048, 2730, 4096)] == [4096, 0, -4096, -2052, 4096]
eye_code = bytes([0xF9]) + struct.pack("<3h", 1000, 0, 0) + bytes([0xE2, 3, 0xF5, 3, 0xF4, 0xFF])
target_code = bytes([0xFA]) + struct.pack("<3h", 0, 0, 0) + bytes([0xE2, 3, 0xF5, 3, 0xF4, 0xFF])
for table_, code, at in zip(OPENINGS, (eye_code, target_code), (0x40000, 0x40100)):
    struct.pack_into("<I", module, table_ - 0x800A0000 + 4 * 2, 0x800A0000 + at)
    module[at:at + len(code)] = code
setup = {"camera_script": 2, "cameras": [{"x": 2000, "y": 0, "z": 0, "target_x": 0, "target_y": 0, "target_z": 100}]}
assert opening(module, setup, 5) == [([1000, 0, 0], [0, 0, 0]), ([1249, 0, 0], [0, 0, 24]),
                                     ([2000, 0, 0], [0, 0, 100]), ([2000, 0, 0], [0, 0, 100]),
                                     ([2000, 0, 0], [0, 0, 100])]

# libgte's ratan2 by a table of 1025 arctangents (here from the formula; the game's is the exe's), and SquareRoot0's
# table of roots: close, not exact (1200 * 1200 gives 1193).
atan = [round(math.atan(i / 1024) * 2048 / math.pi) for i in range(1025)]
assert [ratan2(atan, y, x) for y, x in ((0, 0), (-500, -3200), (3, 4), (-3, -4))] == [0, -1947, 419, -1629]
assert [square_root0(v * v) for v in (0, 1, 100, 1200, 4000)] == [0, 1, 99, 1193, 3998]

# Action scripts: section 1's pointers from 0x68 up to the first script; BATTLE.X's own for 0x2E-0x3B.
s1 = bytearray(0x68) + struct.pack("<2I", 0x70, 0x73) + bytes([0, 0xFE, 0xC0, 0x14, 0xEE])
scripts = action_scripts(bytes(s1), module)
assert len(scripts) == 0x3C and scripts[1][:2] == bytes([0x14, 0xEE]) and scripts[0x2E] == module[0x4EAA0:]

# Cloud's Attack in the first battle (script 20), as the lab frames show it: he turns to the MP (FC), plays 0x1A,
# runs at it over 4 frames to its size (350) plus 1200 short of it (D1: each step the distance over 4, C division:
# (-65, -417), 0 in y) playing 0x1B, swings (0x1E, 0x1C; the blow lands 2 frames after F7 1: the MP turns to him and
# starts its hurt script, 1), goes home (FA), plays 0x1D, turns back (E5) and ends (EE: back to script 0, animation
# 0). Every animation 2 frames here.
attack = bytes.fromhex("fcf0d8001a001ad1b00400000 4f01bf7011e1cfaf01de5ee".replace(" ", ""))
cloud = Actor([b"\x00\xfe\xc0"] + [b""] * 19 + [attack], [2] * 64, (0, 0, 1700), 0, 400, script=20,
              pos=[0, 0, 1700])
mp = Actor([b"\x00\xfe\xc0", b"\x01\xe5\xee"], [2] * 64, (-500, 0, -1500), 2048, 350, pos=[-500, 0, -1500],
           facing=2048)
fight = Fight({0: cloud, 4: mp}, 0, 4, atan)
trace = []
for _ in range(14):
    action_frame(fight)
    trace.append((cloud.script, cloud.anim, cloud.shown, tuple(cloud.pos), cloud.facing, mp.script, mp.facing))
assert trace[0] == (20, 0x1A, 0, (0, 0, 1700), 101, 0, 2048) and mp.toward == -1947
assert [t[3] for t in trace[2:7]] == [(-65, 0, 1283), (-130, 0, 866), (-195, 0, 449), (-260, 0, 32), (-260, 0, 32)]
assert [t[1] for t in trace[2:10]] == [0x1B, 0x1B, 0x1E, 0x1E, 0x1C, 0x1C, 0x1D, 0x1D]
assert [t[5:] for t in trace[5:10]] == [(0, 2048), (1, 63589), (1, 63589), (1, 63589), (0, 2048)]
assert trace[8][3:5] == ((0, 0, 1700), 101) and trace[10][:5] == (0, 0, 0, (0, 0, 1700), 0)

# The camera, played by an attack's scripts: the target follows the attacker's camera bone 0 (E8: no y adjust) for
# 2 frames, the eye takes bone 0 plus an offset now (F0: scaled by 256ths, then waits a frame), then eases
# straight (DB) to a point over 2 frames (E6), starting in its own frame: each step the distance by SquareRoot0 (229
# for 230), given its sign, plus 1, in 16ths over n; then the point.
settings = {0: {"bones": bytes([3] * 16), "scale": (512, 256, 256), "points": [], "adjust": 300}}
view = View([([0, 0, 0], [0, 0, 0])], 0, ([0, 0, 0], [0, 0, 0]), {0: cloud}, settings,
            lambda k, n: [10 * n, -400, 1700], atan, 0, 4, [4])
eye = Channel(bytes([0xF0, 0]) + struct.pack("<3h", 100, -50, 0) + bytes([0xDB, 0xE6]) + struct.pack("<3h", 0, 0, 0)
              + bytes([2, 0xFF]), [5, 5, 5])
aim = Channel(bytes([0xE8, 0]) + struct.pack("<3h", 0, 0, 0) + bytes([2, 0xFF]), [0, 0, 0])
tasks, shots = [], []
for _ in range(4):
    camera_frame(eye, aim, tasks, view)
    shots.append((list(eye.pos), list(aim.pos)))
assert shots[0] == ([230, -450, 1700], [30, -400, 1700]) and shots[1][1] == [30, -400, 1700]
assert shots[1][0] == [116, -225, 851] and shots[2][0] == shots[3][0] == [0, 0, 0]
# Afterwards the camera halves its way back to the setup's camera each frame (C division), and stops when every half
# is 0.
assert returning(([10, -10, 3], [0, 0, 1]), ([0, 0, 0], [0, 0, 0])) == ([5, -5, 2], [0, 0, 1])
assert returning(([1, 0, 0], [0, 0, 1]), ([0, 0, 0], [0, 0, 0])) is None
print("ok")
