"""The battle's models (ENEMY*/*.LZS: the party and the enemies), read as BATTLE.X reads them, into the field
models' shapes (tools/model.py) so the viewer draws them the same way.

A file is a u32 count and that many section offsets. Section 0 is the skeleton and its parts, 1 the settings and
action scripts, then animations (4 bytes: an empty slot). The party's files (the ones not named ENEMYnnn) have 52
body animations from 2, each one's weapon animation 52 on, then a TIM and 16 weapon models (func_800B5C1C,
func_800CD400). An enemy's TIM is last; one with a weapon (section 1 byte 2 bit 7) has 16 body and 16 weapon
animations and the weapon just before the TIM (func_800B54B8).
"""
import gzip
import math
import re
import struct
from dataclasses import dataclass, field
from pathlib import Path

import kernel


def sections(d: bytes) -> list[bytes]:
    n = struct.unpack_from("<I", d)[0]
    at = list(struct.unpack_from(f"<{n}I", d, 4)) + [len(d)]
    return [d[at[k]:at[k + 1]] for k in range(n)]


def animation(d: bytes, bones: int) -> list[tuple[tuple[int, int, int], list[tuple[int, int, int]]]]:
    """Each frame's root position and per bone rotation x, y, z (4096 a turn), as func_800D376C decodes them: frame 0
    absolute, the rest deltas, from a bitstream read MSB first."""
    frames, length, shift = struct.unpack_from("<HHB", d)
    at = 0

    def bits(n: int) -> int:
        nonlocal at
        v = 0
        for _ in range(n):
            v = v << 1 | d[5 + at // 8] >> 7 - at % 8 & 1
            at += 1
        return v - (1 << n) if v >> n - 1 else v

    def rotation_delta() -> int:
        if not bits(1):
            return 0
        k = bits(3) & 7
        if k == 0:
            return -(1 << shift)
        if k == 7:
            return bits(12 - shift) << shift
        v = bits(k)
        return v + (-(1 << k - 1) if v < 0 else 1 << k - 1) << shift

    root = [bits(16) for _ in range(3)]
    turns = [[bits(12 - shift) << shift for _ in range(3)] for _ in range(bones)]
    out = [(tuple(root), [tuple(t) for t in turns])]
    for _ in range(1, frames):
        root = [r + bits(16 if bits(1) else 7) for r in root]
        turns = [[(a + rotation_delta() + 0x8000 & 0xFFFF) - 0x8000 for a in t] for t in turns]
        out.append((tuple(root), [tuple(t) for t in turns]))
    return out


def part_json(d: bytes, at: int, bone: int) -> dict:
    """A part as func_800D29D4 reads it: a u32 size and the vertices (s16 x, y, z, pad), then textured triangles
    (s16 count, the texture page; 16 bytes each), textured quads (u32 count; 20), Gouraud triangles (s16 count, s16;
    20) and Gouraud quads (u32 count; 24). Corners are byte offsets into the vertices; a textured polygon has its
    own UVs and palette and a grey, a Gouraud one ready-made GPU colour words. As a field part: each (page, palette)
    a texture word (kind 2, where it is), flags its index, blend 1 (the renderer forces it) and bit 4 when
    semi-transparent (a Gouraud one's too, by its code byte)."""
    size = struct.unpack_from("<I", d, at)[0]
    vertices = [list(struct.unpack_from("<3h", d, at + 4 + i)) for i in range(0, size, 8)]
    at += 4 + size
    part = {"bone": bone, "vertices": vertices, "uvs": [], "textures": [], "polygons": []}

    def textured(kind: str, corners: list, uvs: list, clut: int, grey: int, code: int):
        word = (2 | (page >> 7 & 3) << 6 | (page & 15) << 8 | (page >> 4 & 1) << 12 | (clut & 0x3F) << 16
                | (clut >> 6 & 0x1FF) << 22)
        name = f"{word:08X}"
        if name not in part["textures"]:
            part["textures"].append(name)
        first = len(part["uvs"])
        part["polygons"].append({"kind": kind, "vertices": [c // 8 for c in corners],
                                 "uv": list(range(first, first + len(uvs))) + [0] * (4 - len(uvs)),
                                 "flags": part["textures"].index(name) | 0x20 | (0x10 if code & 2 else 0),
                                 "colors": [f"{grey:02X}" * 3 + "00"]})
        part["uvs"] += uvs

    count, page = struct.unpack_from("<hH", d, at)
    at += 4
    for _ in range(count):
        a, b, c, grey, code, u0, v0, clut, u1, v1, u2, v2 = struct.unpack_from("<3h2B2BH4B", d, at)
        textured("FT3", [a, b, c], [[u0, v0], [u1, v1], [u2, v2]], clut, grey, code)
        at += 16
    count = struct.unpack_from("<I", d, at)[0]
    at += 4
    for _ in range(count):
        *corners, u0, v0, clut, u1, v1, u2, v2, u3, v3, grey, code = struct.unpack_from("<4h2BH8B", d, at)
        textured("FT4", corners, [[u0, v0], [u1, v1], [u2, v2], [u3, v3]], clut, grey, code)
        at += 20
    for kind, n, head, size in (("G3", 3, "<h2x", 20), ("G4", 4, "<I", 24)):
        count = struct.unpack_from(head, d, at)[0]
        at += 4
        for _ in range(count):
            corners = struct.unpack_from(f"<{n}h", d, at)
            first = at + 8
            colors = [d[first + 4 * i:first + 4 * i + 4].hex().upper() for i in range(n)]
            part["polygons"].append({"kind": kind, "vertices": [c // 8 for c in corners], "colors": colors,
                                     "flags": 0x10 if int(colors[0][6:], 16) & 2 else 0})
            at += size
    return part


def skeleton(d: bytes) -> tuple[list, list]:
    """Section 0 or a weapon (func_800C76C8): u16 count, u16, then count + 1 bones of s16 parent, s16 length, u32
    the offset of its part (0: none). Bone 0, the root, has no parent; a bone starts at its parent's start plus the
    parent's turn of (0, 0, the parent's length). As field bones: each with its parent's length."""
    count = struct.unpack_from("<H", d)[0]
    entries = [struct.unpack_from("<hhI", d, 4 + 8 * i) for i in range(count + 1)]
    bones = [{"length": 0 if i == 0 else entries[parent][1], "parent": -1 if i == 0 else parent}
             for i, (parent, _, _) in enumerate(entries)]
    parts = [part_json(d, offset & 0x7FFFFFFF, i) for i, (_, _, offset) in enumerate(entries) if offset]
    return bones, parts


def tim(d: bytes) -> list[tuple[int, int, int, int, bytes]]:
    """A TIM's blocks to load (x, y, w, h in VRAM words, data): its palettes, if it has them, then its image."""
    flags, at, out = struct.unpack_from("<I", d, 4)[0], 8, []
    for _ in range(2 if flags & 8 else 1):
        length, x, y, w, h = struct.unpack_from("<I4H", d, at)
        out.append((x, y, w, h, d[at + 12:at + 12 + 2 * w * h]))
        at += length
    return out


def frames_json(body: list, weapon: list | None) -> dict:
    """Decoded frames as a field animation: every angle its own channel (in field units, 256 a turn), the roots'
    positions offsets. The weapon's bones follow the body's, its root on the actor's root, as func_800B4E30 sets it."""
    anim = {"frames": len(body), "bones": [], "rotations": [], "offsets": [], "fixed": []}
    for frames in (body, weapon) if weapon else (body,):
        frames = [frames[min(f, len(frames) - 1)] for f in range(len(body))]
        for b in range(len(frames[0][1])):
            turning = len(anim["rotations"])
            anim["rotations"] += [[f[1][b][k] / 16 for f in frames] for k in range(3)]
            record = {"flags": 7, "rotation": [turning, turning + 1, turning + 2], "offset": [0xFF] * 3}
            if b == 0:
                moving = len(anim["offsets"])
                anim["offsets"] += [[f[0][k] for f in frames] for k in range(3)]
                record |= {"flags": 0x77, "offset": [moving, moving + 1, moving + 2]}
            anim["bones"].append(record)
    return anim


def model_json(d: bytes, party: bool, weapon: int = 0) -> dict:
    """A battle model with weapon (a party member's 0-15) as field bones, parts and animations, and its TIM."""
    s = sections(d)
    armed = s[1][2] & 0x80
    if party:
        texture, weapons, body, plus = len(s) - 17, s[len(s) - 16:], range(2, min(54, len(s) - 17)), 52
    else:
        texture, weapons, plus = len(s) - 1, s[-2:-1], 16
        body = range(2, 18) if armed else range(2, len(s) - 1)
    bones, parts = skeleton(s[0])
    weapon_bones = None
    if armed and len(weapons[weapon]) > 4:
        extra, extra_parts = skeleton(weapons[weapon])
        weapon_bones = len(extra)
        parts += [p | {"bone": p["bone"] + len(bones)} for p in extra_parts]
        bones += [b | {"parent": b["parent"] + len(bones) if b["parent"] >= 0 else -1} for b in extra]
    anims = []
    for k in body:
        if len(s[k]) <= 4:
            anims.append(None)
            continue
        held = s[k + plus] if weapon_bones and k + plus < texture and len(s[k + plus]) > 4 else None
        anims.append(frames_json(animation(s[k], len(bones) - (weapon_bones or 0)),
                                 animation(held, weapon_bones) if held else None))
    # the shadow's size across, along and its z offset from the root (section 1 +6, +8, +0xA; func_800B4E30)
    return {"bones": bones, "parts": parts, "animations": anims, "tim": tim(s[texture]),
            "shadow": struct.unpack_from("<3h", s[1], 6)}


def files(disc: Path) -> list[Path]:
    """The battle's models: SEFFECT.LZS, the effects' own, isn't one."""
    return [p for p in sorted(disc.glob("ENEMY[1-6]/*.LZS")) if p.stem != "SEFFECT"]


def stage_json(d: bytes) -> dict:
    """A stage (STAGEn/STAGEnn.LZS, location nn): section 0 a type and a first section, the models (parts in world
    coordinates) by section, and the TIM last. stage_draws says where each is drawn."""
    s = sections(d)
    return {"type": s[0][0], "first": s[0][1], "parts": {k: part_json(s[k], 0, 0) for k in range(1, len(s) - 1)},
            "tim": tim(s[-1])}


def s16(v: int) -> int:
    return (v + 0x8000 & 0xFFFF) - 0x8000


def stage_draws(kind: int, first: int, count: int, frame: int) -> list[tuple[int, str, int | None, tuple, tuple]]:
    """What func_800B9568 draws of a stage with count sections in battle frame frame (15 a second, from 0): each a
    section, its list, the shift from avsz to its slot (None: the backdrop's one list), a rotation x, y, z (4096 a
    turn) and a position. The lists are drawn in order: the far layer (>> 10, or >> 11), the backdrop (the last
    linked first), the shadows, the main ordering table (>> 2). Type 1 (most stages) puts section 1 and 5 up to
    first in the backdrop, 2-4 in the far layer, the rest on the main table; type 5 the same with the backdrop's on
    the main table. The others move entries each frame (func_800BA11C, func_800BA24C, func_800BA2BC), counted from
    the battle's first frame; a value changed after drawing shows the frame after."""
    still = (0, 0, 0)
    main, far = ("main", 2), ("far", 10)
    match kind:
        case 1 | 5:
            back = ("backdrop", None) if kind == 1 else main
            return ([(1, *back, still, still)] + [(k, *far, still, still) for k in (2, 3, 4)]
                    + [(k, *back, still, still) for k in range(5, first)]
                    + [(k, *main, still, still) for k in range(first, count)])
        case 0:  # a train: the carriage jolts, sections 8-10 drawn 8 times scroll by in z
            n = frame - 1
            jolt = 0 if frame == 0 else -40 * (n % 4 == 0) - 80 * (n % 32 == 16)
            out = [(k, *main, still, (0, jolt, 0)) for k in (1, 5, 6, 7, 11)] + [(k, *far, still, still)
                                                                                 for k in (4, 3, 2)]
            for entry in range(2, 10):
                reset = s16(30000 - 10000 * (entry - 2))
                # func_800B430C starts entries 2-7 at their reset, 8 at 9's and leaves 9 at 0
                z = reset if entry < 8 else s16(30000 - 70000) if entry == 8 else 0
                for _ in range(frame):  # drawn, then: back to reset at reset + 10000, on 1000
                    z = s16((reset if z == s16(reset + 10000) else z) + 1000)
                out += [(k, *main, still, (0, 0, z)) for k in (8, 9, 10)]
            return out
        case 2:  # an elevator shaft: the walls scroll up, the cars bob
            wall = 0 if frame == 0 else -100 * ((frame - 1) % 125 + 1)
            bob = []
            for counter in (0, 16):
                y = 0
                for _ in range(frame):
                    y += -25 if counter < 16 else 25
                    counter = counter - 1 & 31
                bob.append(y)
            return ([(k, *main, still, (0, bob[0], 0)) for k in (1, 7)] + [(k, *main, still, (0, bob[1], 0))
                                                                         for k in (2, 8)]
                    + [(k, *main, still, (0, wall, 0)) for k in (3, 4, 5, 9, 10)] + [(6, *main, still, still)]
                    + [(k, *main, still, (0, wall + 12500, 0)) for k in (3, 4, 5, 9, 10)])
        case 3:  # three parts spin about y
            return ([(1, "backdrop", None, still, still)] + [(k, *far, still, still) for k in (2, 3, 4)]
                    + [(k, *main, (0, (frame + 1) * turn, 0), still) for k, turn in ((5, 1), (6, 2), (7, 4))])
        case 4:  # three rings spin about z, 2500 up
            return ([(k, "far", 11, still, still) for k in (1, 5)] + [(k, *far, still, still) for k in (2, 3, 4)]
                    + [(k, "backdrop", None, (0, 0, (frame + 1) * turn), (0, -2500, 0))
                       for k, turn in ((6, 8), (7, 6), (8, 4))])
    return [(k, *main, still, still) for k in range(1, count)]


def moved(table: dict, page: int, rows: int) -> dict:
    """A model's parts and TIM where func_800D2980 loads them: the image moved by a texture page word's worth (party
    slot 0, 1, 2: 0, 0x10, 0x11; enemy model j: 0x12 + j), the palettes down rows (3 an actor, func_800C62F4)."""
    dx, dy = (page & 15) * 64, (page >> 4 & 1) * 256
    blocks = [(x, y + rows, w, h, data) for x, y, w, h, data in table["tim"][:-1]]
    x, y, w, h, data = table["tim"][-1]
    parts = []
    for part in table["parts"]:
        words = []
        for word in part["textures"]:
            w_ = int(word, 16)
            w_ += (page & 15) << 8 | (page >> 4 & 1) << 12 | rows << 22
            words.append(f"{w_:08X}")
        parts.append(part | {"textures": words})
    return table | {"parts": parts, "tim": blocks + [(x + dx, y + dy, w, h, data)]}


def module(disc: Path) -> bytes:
    """BATTLE.X as it runs at 0x800A0000 (gzip after an 8-byte header)."""
    return gzip.decompress((disc / "BATTLE/BATTLE.X").read_bytes()[8:])


SHADOW = 0x800EA294  # BATTLE.X's shadow: a disc of radius 900 at y 0, 16 Gouraud triangles, grey 200 in the middle


def shadow(module: bytes, root: tuple, size: tuple) -> dict:
    """An actor's shadow as func_800BA598 draws it: the disc scaled by the model's size across and along, at its
    root's x and z (plus the offset; at the actor's own place if that's -1), on the floor. Drawn subtracted."""
    part = part_json(module, SHADOW - 0x800A0000, 0)
    across, along, offset = size
    x, z = (0, 0) if offset == -1 else (root[0], root[2] + offset)
    return part | {"vertices": [[x + vx * across / 4096, 0, z + vz * along / 4096] for vx, _, vz in part["vertices"]]}


PARTY_THREE, PARTY_TWO, PARTY_FACING = 0x800E8E84, 0x800E8F28, 0x800E8F94  # BATTLE.X tables, per layout


def placement(module: bytes, setup: dict, members: int) -> list[tuple[tuple[int, int, int], int]]:
    """Where the party (members of them) and then the setup's enemies stand, and their turn about y (4096 a turn).
    The party's places come from BATTLE.X's tables by the setup's layout (func_800B4794: one member takes the
    three's first at x 0, two their own table; front row); its facing from another (func_800B46B4; 0 in layout
    2). Enemies stand at their formation places, facing the party: turned half round when on the far side (z < 0,
    or z >= 0 in layouts 3, 5, 6 and 7), not in layout 1 (func_800B5138)."""
    layout = setup["layout"]

    def s16s(address: int, n: int) -> tuple:
        return struct.unpack_from(f"<{n}h", module, address - 0x800A0000)

    out = []
    for slot in range(members):
        if members == 2:
            x, y, z = s16s(PARTY_TWO + layout * 12 + slot * 6, 3)
        else:
            x, y, z = s16s(PARTY_THREE + layout * 18 + slot * 6, 3)
            x = 0 if members == 1 else x
        facing = 0 if layout == 2 else s16s(PARTY_FACING + layout * 6 + slot * 2, 1)[0] & 0xFFFF
        out.append(((x, y, z), facing))
    for enemy in setup["formation"]:
        if enemy["enemy_id"] == 0xFFFF:
            continue
        z = enemy["z"]
        far = z >= 0 if layout in (3, 5, 6, 7) else z < 0
        out.append(((enemy["x"], enemy["y"], z), 2048 if far and layout != 1 else 0))
    return out


def normal(v: list[int]) -> list[int]:
    """libgte's VectorNormal: v scaled to about 4096 long, as the GTE does it (sqr, the leading zeros of the sum
    rounded down to even, a 1 / root from a table of 192 entries, floor(32768 / sqrt(64 + i)), then gpf and a
    shift). Its results run a little long (4102 for 4096)."""
    v = [(c + 0x8000 & 0xFFFF) - 0x8000 for c in v]
    total = sum(c * c for c in v)
    zeros = 32 - total.bit_length() & ~1
    top = total << zeros - 24 if zeros >= 24 else total >> 24 - zeros
    return [math.isqrt((1 << 30) // top) * c >> (31 - zeros >> 1) for c in v]


def square_root0(v: int) -> int:
    """libgte's SquareRoot0: the leading zeros rounded down to even, then a table of 192 roots, floor(512 sqrt(64 +
    i)), shifted back. Close, not exact: 1200 * 1200 gives 1193."""
    if not v:
        return 0
    zeros = 32 - v.bit_length() & ~1
    top = v << zeros - 24 if zeros >= 24 else v >> 24 - zeros
    return math.isqrt(top << 18) << (31 - zeros >> 1) >> 12


def straight(p: int, a: int, b: int, n: int) -> int:
    """A straight ease's step from a to b over n frames (func_800C0480): the distance by SquareRoot0, given its
    sign, plus 1 in 16ths, over n; added to p in 16ths."""
    d = square_root0((b - a) * (b - a))
    return (p * 16 + cdiv(((d if b >= a else -d) + 1) * 16, n)) >> 4


def look_at(eye, target) -> tuple[list[list[int]], list[int]]:
    """The battle camera (func_800D85B0), in the GTE's integers: rows x, y, z (4096 being 1) with z towards the
    target (its z nudged off 0), x = z x up, y = z x x (op, the up vector (0, -4096, 0), each >> 12), so view space is
    x right, y down, z ahead; and the translation -(R eye >> 12)."""
    def op(d, ir):
        return [d[1] * ir[2] - d[2] * ir[1] >> 12, d[2] * ir[0] - d[0] * ir[2] >> 12, d[0] * ir[1] - d[1] * ir[0] >> 12]

    z = normal([t - e for t, e in zip(target, eye)])
    z[2] += z[2] == 0
    x = normal(op(z, [0, -4096, 0]))
    rows = [x, normal(op(z, x)), z]
    return rows, [-(sum(r[k] * eye[k] for k in range(3)) >> 12) for r in rows]


OPENINGS = 0x800E9E5C, 0x800E9FF8  # BATTLE.X's eye and target scripts by a setup's camera_script (103)


def rcos(a: int) -> int:
    """libgte's rcos, from its quarter-wave table, round(4096 sin(i pi / 2048)) for i 0-1024."""
    q, r = divmod(a & 4095, 1024)
    sin = [round(4096 * math.sin(i * math.pi / 2048)) for i in (r, 1024 - r)]
    return [sin[1], -sin[0], -sin[1], sin[0]][q]


def opening(module: bytes, setup: dict, ticks: int) -> list[tuple[list[int], list[int]]]:
    """The camera's eye and target after each of the first ticks battle frames: the setup's camera_script (BATTLE.X's
    own pair of scripts, camera -2) played by camera_frame from the setup's camera 0. They set a point (F9, FA), take
    the current camera (D9), choose a setup camera (DD), ease to it (E2) or to a point (E6) over n frames, set a wait
    (F5), wait (F4) and end (FF)."""
    cameras = [([c["x"], c["y"], c["z"]], [c["target_x"], c["target_y"], c["target_z"]])
               for c in setup["cameras"]]
    codes = [module[struct.unpack_from("<I", module, table - 0x800A0000 + 4 * setup["camera_script"])[0]
                    - 0x800A0000:] for table in OPENINGS]
    eye, target = (Channel(codes[k], list(cameras[0][k])) for k in (0, 1))
    view = View(cameras, 0, cameras[0], {}, {}, None, [], 0, 0, [])
    tasks, out = [], []
    for _ in range(ticks):
        camera_frame(eye, target, tasks, view)
        out.append((list(eye.pos), list(target.pos)))
        view.current = out[-1]
    return out


# Action scripts (func_800C7C4C): BATTLE.X's own scripts for script numbers 0x2E-0x3B (jtbl_800A0888; 0x33 keeps the
# model's), and the operand bytes of the opcodes the player passes over (effects, sounds, flags).
BUILT_IN = {0x2E: 0x800EEAA0, 0x2F: 0x800EEAB0, 0x30: 0x800EEAB8, 0x31: 0x800EEAE0, 0x32: 0x800EEAEC,
            0x34: 0x800EEAD8, 0x35: 0x800EEB1C, 0x36: 0x800EEB10, 0x37: 0x800EEB04, 0x38: 0x800EEAF8,
            0x39: 0x800EEAD0, 0x3A: 0x800EEAC8, 0x3B: 0x800EEACC}
OPERANDS = {0x90: 3, 0x91: 1, 0x94: 5, 0x96: 2, 0x97: 2, 0x98: 1, 0x9D: 1, 0xA0: 1, 0xA1: 2, 0xA2: 1, 0xA3: 1,
            0xA8: 2, 0xAB: 4, 0xAC: 1, 0xAD: 5, 0xAF: 1, 0xB5: 11, 0xB9: 1, 0xBC: 1, 0xBD: 4, 0xBE: 1, 0xBF: 2,
            0xC2: 1, 0xC4: 3, 0xC6: 1, 0xC7: 3, 0xCB: 8, 0xD4: 3, 0xD6: 1, 0xD7: 2, 0xD8: 3, 0xDA: 1, 0xDB: 4, 0xDC: 3,
            0xDD: 2, 0xDE: 2, 0xE7: 1, 0xF5: 1, 0xF7: 1, 0xF8: 1}
ANIMATES = {0xBB, 0xC0, 0xCD, 0xD2, 0xD3, 0xD9, 0xEF}  # the default case: like a byte below 0x8E
# The runs at the target: where the distance (s16) and the frame count sit after the opcode, and its length.
RUNS = {0xD1: (0, 4, 5), 0x99: (1, 5, 6), 0xE9: (0, 2, 3), 0xD5: (0, 7, 8), 0xCF: (0, 7, 8)}


def action_scripts(s1: bytes, module: bytes) -> list[bytes]:
    """A model's action scripts by number (section 1's pointers from 0x68 up to the first script: 74 for the party,
    32 for an enemy), BATTLE.X's own in place of 0x2E-0x3B."""
    first = struct.unpack_from("<I", s1, 0x68)[0]
    scripts = [s1[p:] for p in struct.unpack_from(f"<{(first - 0x68) // 4}I", s1, 0x68)]
    scripts += [b""] * (0x3C - len(scripts))
    for n, address in BUILT_IN.items():
        scripts[n] = module[address - 0x800A0000:]
    return scripts


# A party command's choices (func_800A1798 runs the command's program from D_800A0098): Magic, Summon and E.Skill
# make an attack id of the choice from 0, 0x38 and 0x48 (func_800A73C0, func_800A73D8, func_800A76AC); the W- commands
# run the same programs and Mime, Change and Defend play no script of their own.
SPELLS = {2: range(0x00, 0x38), 3: range(0x38, 0x48), 13: range(0x48, 0x60)}
ITEM, LIMIT = 4, 20
LIMITS = 0x1324  # LIMTMENU.MNU's records of the characters' own limits (ids 0x80 up; func_801D11A8 copies them)


def party_commands(module: bytes, sections: list[bytes], limits: bytes, character: int | None,
                   at_party: bool) -> list[tuple[str, int, list[tuple[str, int, int, int, int]]]]:
    """A party member's commands as the battle plays them: each command's number and choices as (name, action script,
    camera at one target, at all (-1 holds it), the attack the menu queues: a spell's place in its command, an item's
    number, a limit's id, else 0). The script is BATTLE.X's first byte of 4 a command at 0x800A0000; the cameras
    the command record's +4 and +6, or, where the choice has a record (a spell's, an item's, a limit's), its +8 and
    +0xA (func_800A79CC; an item's +8 for both, func_800A80DC). A limit plays 0x3C on by the character's slots filled
    before it (kernel section 2's 12 at +0xC + character * 0x38; none without a character); an item 0x21 at the
    party, 0x20 at the enemies."""
    names = [[re.sub(r"\{F8 ..\}", "", kernel.decode(t)) for t in kernel.read_text(sections[k])]
             for k in (17, 18, 19)]

    def cameras(record: bytes, at: int) -> tuple[int, int]:
        return tuple(-1 if c == 0xFFFF else c for c in struct.unpack_from("<2H", record, at))

    listed = []
    for command, name in enumerate(names[0][:28]):
        script = module[4 * command]
        if command in SPELLS:
            choices = [(names[1][a], script, *cameras(sections[1], 28 * a + 8), a - SPELLS[command].start)
                       for a in SPELLS[command] if names[1][a]]
        elif command == ITEM:
            choices = [(n, 0x20 + at_party, *cameras(sections[4], 28 * k + 8)[:1] * 2, k)
                       for k, n in enumerate(names[2]) if n]
        elif command == LIMIT:
            slots = [a for a in sections[2][0xC + 0x38 * character:0xC + 0x38 * character + 12]
                     if a != 0xFF] if character is not None else []
            choices = [(names[1][a], 0x3C + k, *cameras(limits, LIMITS + 28 * (a - 0x80) + 8), a)
                       for k, a in enumerate(slots)]
        elif script:
            choices = [(name, script, *cameras(sections[0], 8 * command + 4), 0)]
        else:
            continue
        if name and choices and command not in (21, 22, 23):
            listed.append((name, command, choices))
    return listed


@dataclass
class Actor:
    """An actor as the action scripts move it (its record at D_801518E4 + actor * 0xB9C)."""
    scripts: list[bytes]
    frames: list[int]  # each animation's frame count
    home: tuple  # x, y, z (D_80163C80)
    base: int  # the facing the battle gives it (+0x18), 4096 a turn
    size: int  # section 1 +4 (+0x12)
    idle: int = 0  # the script it goes back to (D_80163784)
    script: int = 0
    pc: int = 0
    run: bool = True  # its animation has just ended, so the script runs (+0x3B)
    hold: int = 0  # F4's wait (+0x3D)
    anim: int = 0
    next: int = 0  # the frame its next step shows
    shown: int = 0
    elapsed: int = 0  # steps since its animation started (+0x36 of its 0x74-byte record)
    pos: list = field(default_factory=list)
    facing: int = 0
    toward: int = 0  # FC's angle from its attacker toward it (+0x2C of its 0x74-byte record)


@dataclass
class Fight:
    """What the action scripts share: the actors by number, the attacker (D_801590CC) and its target (D_800F99E8),
    the exe's arctangents (ratan2), the move tasks running, the blows on their way (frames to go) and A9's counter
    (D_800FAFD8)."""
    actors: dict
    attacker: int
    target: int
    atan: list
    tasks: list = field(default_factory=list)
    blows: list = field(default_factory=list)
    count: int = 0


HURT = (5, 1)  # the scripts the first battle's lab frames start on a hit: the party's, an enemy's


def step(a: Actor):
    """The animation's next frame (func_800CD400): the script runs again once it has shown the last."""
    a.shown, a.next, a.elapsed = a.next, (a.next + 1) % (a.frames[a.anim] or 1), a.elapsed + 1
    a.run = a.next == 0


def ratan2(atan: list[int], y: int, x: int) -> int:
    """libgte's ratan2, by its table of 1025 arctangents (the exe's D_8004FC98, 4096 a turn)."""
    if not x and not y:
        return 0
    ax, ay = abs(x), abs(y)
    if ay < ax:
        r = atan[ay // (ax >> 10) if ay & 0x7FE00000 else (ay << 10) // ax]
    else:
        r = 0x400 - atan[ax // (ay >> 10) if ax & 0x7FE00000 else (ax << 10) // ay]
    r = 0x800 - r if x < 0 else r
    return -r if y < 0 else r


def rsin(a: int) -> int:
    return rcos(a - 1024)


def cdiv(a: int, b: int) -> int:
    """C's division, toward 0."""
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def action_frame(fight: Fight):
    """One battle frame: each actor's action script runs if its animation has ended, then its animation steps a frame
    (func_800C7C4C), then the move tasks the scripts started (D_80163B48's) step; the game poses the actors after that,
    and its camera follows those poses. A byte below 0x8E plays that animation and the script waits for its end; scripts
    turn to the target (FC) and back (E5, F9, B4, 95), go home (FA), step along the facing (ED, E4: 516; B1, B0: 258),
    run at the target to its size plus a distance short of it over n frames (D1, 99, E9, CC, D0, D5, CF), slide (C8),
    wait (F4 n, F3) and end (EE, FF back to the idle script; FE C0 loops it). A blow (F7 n, BE n) lands n + 1 frames on:
    the target turns to face the attacker and starts its hurt script, as the battle does on a hit (outside the scripts;
    HURT, as the lab frames show it). Effects, sounds and flags are passed over: 9E, which waits for effects, never
    does."""
    for k in sorted(fight.actors):
        if not fight.actors[k].run or act(fight, k):
            step(fight.actors[k])
    for blow in list(fight.blows):
        blow[0] -= 1
        if blow[0]:
            continue
        fight.blows.remove(blow)
        t = fight.actors[fight.target]
        t.script, t.pc, t.hold, t.run = HURT[fight.target >= 4], 0, 0, True
        t.facing = fight.actors[fight.attacker].facing - 0x800 & 0xFFFF
    for task in list(fight.tasks):
        if not task[1]:
            fight.tasks.remove(task)
            continue
        a = fight.actors[task[0]]
        a.pos = [s16(p + d) for p, d in zip(a.pos, task[2])]
        task[1] -= 1


def act(fight: Fight, k: int) -> bool:
    """Run actor k's script until it plays an animation (True: the animation steps this frame) or waits (False)."""
    a, t, atan, tasks, target = fight.actors[k], fight.actors[fight.target], fight.atan, fight.tasks, fight.target
    while True:
        code = a.scripts[a.script]
        op = code[a.pc]
        a.pc += 1
        args = code[a.pc:]
        if op < 0x8E or op in ANIMATES:
            a.anim, a.next, a.elapsed = op, 0, 0
            return True
        if op in (0xA7, 0xB6):  # another animation, as far in as the last had gone
            a.anim, a.next, a.pc = args[0], 0, a.pc + 1
            elapsed = a.elapsed
            for _ in range(elapsed):
                step(a)
            a.elapsed = elapsed
            return True
        if op == 0xA9:  # its first operand is never read; it starts 0-15 frames in, by a counter
            a.anim, a.next, a.elapsed, a.pc = args[1], 0, 0, a.pc + 2
            fight.count = fight.count + 3 & 0xFF
            for _ in range(fight.count & 15):
                step(a)
            return True
        if op == 0xFE and not a.hold and args[0] == 0xC0:
            a.anim, a.next, a.elapsed = code[0], 0, 0
            a.script, a.pc, a.run = a.idle, 0, True
        elif op in (0xEE, 0xFF):
            a.script, a.pc, a.hold, a.run = a.idle, 0, 0, True
        elif op in (0xF1, 0x9E):
            a.pc -= op == 0xF1
            return False
        elif op == 0xF4:
            a.hold, a.pc = args[0], a.pc + 1
        elif op == 0xF3 and a.hold:
            a.hold, a.pc = a.hold - 1, a.pc - 1
            return False
        elif op == 0xFC and fight.attacker != target:
            d = ratan2(atan, t.pos[0] - a.pos[0], t.pos[2] - a.pos[2])
            a.facing, t.toward = d + 0x800 & 0xFFFF, d
        elif op in (0xE5, 0xF9, 0xB4, 0x95, 0xBA):
            a.facing, a.pc = a.base, a.pc + 2 * (op == 0xBA)
        elif op == 0xFA:
            a.pos = list(a.home)
        elif op == 0xFD:
            a.pos, a.pc = list(struct.unpack_from("<3h", args)), a.pc + 6
        elif op in (0xED, 0xE4, 0xB1, 0xB0):
            angle, stride = a.facing + (0x800 if op in (0xE4, 0xB0) else 0), 0x204 if op in (0xED, 0xE4) else 0x102
            a.pos[0] -= rsin(angle) * stride >> 12
            a.pos[2] -= rcos(angle) * stride >> 12
        elif op in (0x9A, 0xFB):
            d, y = struct.unpack_from("<2h", args)
            a.pc += 4
            angle, d = 0x800 - t.toward, d + t.size
            a.pos = [t.pos[0] - (rsin(angle) * d >> 12), y, t.pos[2] + (rcos(angle) * d >> 12)]
        elif op in RUNS or op in (0xCC, 0xD0):
            # ponytail: one target only (D_801031F0 clear); D0, D5 and CF have curves of their own, run straight here
            if op == 0xCC:
                d, n, length = 0, args[0], 1
            elif op == 0xD0:
                d, n, length = struct.unpack_from("<h", args)[0], 5 if args[2] < 4 else 8, 3
            else:
                at, count, length = RUNS[op]
                d, n = struct.unpack_from("<h", args, at)[0], args[count]
            a.pc += length
            angle, d = 0x800 - t.toward, d + t.size
            x = s16(t.pos[0] - (rsin(angle) * d >> 12))
            z = s16(t.pos[2] + (rcos(angle) * d >> 12))
            move = (0, 0, 0) if target == k else (cdiv(x - a.pos[0], n), cdiv(t.pos[1] - a.pos[1], n) if k < 4
                                                  else 0, cdiv(z - a.pos[2], n))
            tasks.append([k, n, move])
        elif op == 0xC8:
            dx, dz, n = struct.unpack_from("<2hB", args)
            a.pc += 5
            tasks.append([k, n, (dx, 0, dz)])
        elif op == 0xCE and k >= 4:
            a.pc = code.index(0xCD, a.pc) + 1
        elif op in (0xF7, 0xBE):
            fight.blows.append([args[0] + 2])
            a.pc += 1
        elif op == 0xC1:
            a.pc = code.index(0xC9) + 1
        elif op != 0xFE:
            a.pc += OPERANDS.get(op, 0)


CAMDATS = {2: 1, 3: 2, 4: 2, 5: 2, 6: 2, 7: 2}  # by layout; the others read CAMDAT0 (D_800E7D50)


def returning(current: tuple, goal: tuple) -> tuple | None:
    """The camera's way back after an attack (func_800C1104): each of the eye's and target's numbers moves half the
    way (C division) to the setup camera's; None once every half is 0."""
    halves = [[cdiv(g - c, 2) for c, g in zip(cs, gs)] for cs, gs in zip(current, goal)]
    if not any(any(h) for h in halves):
        return None
    return tuple([c + h for c, h in zip(cs, hs)] for cs, hs in zip(current, halves))


def camdat(d: bytes, number: int) -> list[tuple[bytes, bytes]]:
    """An attack camera's three variants, each an eye and a target script, from a CAMDAT file (loaded at
    0x801A0000, its pointers already absolute; the eye and target tables hold camera n's at n * 3 + channel)."""
    tables = struct.unpack_from("<2I", d)
    return [tuple(d[struct.unpack_from("<I", d, table - 0x801A0000 + 4 * (number * 3 + ch))[0] - 0x801A0000:]
                  for table in tables) for ch in range(3)]


@dataclass
class Channel:
    """A camera channel's eye or target (D_80151844, D_801518A4 + channel * 14)."""
    code: bytes
    pos: list
    pc: int = 0
    wait: int = 0
    linear: bool = False  # eases go straight, not on rcos's curve
    turn: bool = False  # the eye's offsets from an actor turn with it


@dataclass
class View:
    """What the camera scripts read: the setup's cameras (eye, target) and the one chosen (D_80163B3C), the camera
    shown, the actors (with each one's camera settings, from section 1: bones +0x12, offset scales +0x44, points
    +0x4C, y adjust +0x64), bone(actor, n) its bone n's world position, the attacker, the target (0xF all those
    hit) and those hit (D_801517F0)."""
    cameras: list
    chosen: int
    current: tuple
    actors: dict
    settings: dict
    bone: object
    atan: list  # the exe's arctangents (ratan2)
    attacker: int
    target: int
    hits: list
    ground: bool = False  # the target's D8 sends its later eases to bones to y 0 (D_800F4B00)


def camera_settings(s1: bytes) -> dict:
    """An actor's camera settings from its model's section 1."""
    points = struct.unpack_from("<12h", s1, 0x4C)
    return {"bones": s1[0x12:0x22], "scale": struct.unpack_from("<3H", s1, 0x44),
            "points": [(points[0:3], points[3:6]), (points[6:9], points[9:12])],
            "adjust": struct.unpack_from("<h", s1, 0x64)[0]}


def midpoint(view: View) -> list[int]:
    """func_800D54EC: the middle of the box round those hit, on the floor."""
    xs, zs = ([view.actors[k].pos[i] for k in view.hits] for i in (0, 2))
    return [(min(xs) + max(xs)) // 2, 0, (min(zs) + max(zs)) // 2]


def bone_point(view: View, actor: int, n: int, offset: tuple, eye: bool, toward: bool, turn: bool,
               lift: bool = True) -> list[int]:
    """Bone n (by the actor's camera bone table) of an actor plus an offset: an eye's scaled by the model's offset
    scales (256 being 1) and, when turn, turned about y with its facing or (toward) FC's angle (func_800C0DD8); a
    target's y raised by the model's y adjust (func_800C0314), but not a following target's (its task works it out
    and then writes over it)."""
    if actor == 0xF:
        return [p + o for p, o in zip(midpoint(view), offset)]
    a, s = view.actors[actor], view.settings[actor]
    at = view.bone(actor, s["bones"][n])
    if not eye:
        lift = 0 if not lift or a.script == 1 and s["adjust"] > 0 else s["adjust"]
        return [at[0] + offset[0], at[1] + offset[1] + lift, at[2] + offset[2]]
    x, y, z = (o * k >> 8 for o, k in zip(offset, s["scale"]))
    if turn:
        r = square_root0(x * x + z * z)
        angle = ratan2(view.atan, x, z) + (a.toward if toward else a.facing)
        x, z = rsin(angle) * r >> 12, rcos(angle) * r >> 12
    return [at[0] + x, at[1] + y, at[2] + z]


def camera_frame(eye: Channel, target: Channel, tasks: list, view: View):
    """One battle frame of a camera channel (func_800BC81C): the target's script, then the eye's, then the tasks
    they started (func_800BC630). Besides the opening's opcodes: ease to a bone of an actor plus an offset (eye D8,
    E3, E4, E5; target D8, E3, E4, E5), follow one for n frames (eye E7, E9, EB; target E8, EA, EC), set one now and
    wait for the next frame (eye EF, F0, F7; target F0, F8, F9), ease to the target's camera point (E0), circle the
    target (eye F8). An opcode a channel doesn't have ends its frame."""
    for k, ch in ((1, target), (0, eye)):
        while camera_op(k, ch, eye, target, tasks, view):
            pass
    for task in list(tasks):
        kind, ch = task[0], task[1]
        if kind == "ease":
            _, ch, a, b, n, f, linear = task
            if n - f <= 1:
                ch.pos = list(b)
                tasks.remove(task)
                continue
            if linear:
                ch.pos = [straight(p, x, y, n) for p, x, y in zip(ch.pos, a, b)]
            else:
                w = rcos(int((f << 11) / n) + 0x800) + 4096
                ch.pos = [s16(x + int(w * (y - x) / 8192)) for x, y in zip(a, b)]
            task[5] += 1
        elif kind == "follow":
            _, ch, k, actor, n, offset, toward, frames = task
            ch.pos = bone_point(view, actor, n, offset, k == 0, toward, eye.turn, False)
            task[7] -= 1
            if not task[7]:
                tasks.remove(task)
        else:  # orbit: angle step, radius, angle, radius step, y step, frames
            step, r, angle, grow, rise, n = task[2:]
            angle += step
            ch.pos = [target.pos[0] + (rsin(angle) * r >> 9), ch.pos[1] + rise, target.pos[2] + (rcos(angle) * r >> 9)]
            task[3:] = [r + grow, angle, grow, rise, n - 1]
            if n == 1:
                tasks.remove(task)


def camera_op(k: int, ch: Channel, eye: Channel, target: Channel, tasks: list, view: View) -> bool:
    """One opcode of channel k (0 the eye, 1 the target); False when its frame ends."""
    op = ch.code[ch.pc]
    ch.pc += 1
    args = ch.code[ch.pc:]

    def ease(to: list, n: int):
        tasks[:] = [t for t in tasks if t[1] is not ch]
        tasks.append(["ease", ch, list(ch.pos), list(to), n, 0, ch.linear])


    who = {"A": view.attacker, "T": view.target}
    eye_ops = {0xD8: ("ease", "L", 9), 0xE3: ("ease", "n", 9), 0xE4: ("ease", "A", 8), 0xE5: ("ease", "T", 8),
               0xE7: ("follow", "A", 8), 0xE9: ("follow", "T", 8), 0xEB: ("follow", "n", 9),
               0xEF: ("set", "n", 8), 0xF0: ("set", "A", 7), 0xF7: ("set", "T", 7)}
    target_ops = {0xD8: ("ease", "L", 9), 0xE3: ("ease", "n", 9), 0xE4: ("ease", "A", 8), 0xE5: ("ease", "T", 8),
                  0xE8: ("follow", "A", 8), 0xEA: ("follow", "T", 8), 0xEC: ("follow", "n", 9),
                  0xF0: ("set", "n", 8), 0xF8: ("set", "T", 7), 0xF9: ("set", "A", 7)}
    actor_ops = target_ops if k else eye_ops
    if op in actor_ops:
        kind, whom, length = actor_ops[op]
        ch.pc += length
        if whom in "Ln":
            actor = view.hits[args[0]] if whom == "L" else args[0]
            args = args[1:]
        else:
            actor = who[whom]
        n, offset = args[0], struct.unpack_from("<3h", args, 1)
        toward = whom == "T" or (whom == "L" and k == 0)
        if k and op == 0xD8:
            view.ground = True
        at = bone_point(view, actor, n, offset, k == 0, toward, eye.turn)
        if kind == "ease":
            ease([at[0], 0, at[2]] if k and view.ground else at, args[7])
        elif kind == "follow":
            tasks[:] = [t for t in tasks if t[1] is not ch]
            tasks.append(["follow", ch, k, actor, n, offset, toward, args[7]])
        else:
            ch.pos = at
            return False
    elif op == (0xF9, 0xFA)[k]:
        ch.pos, ch.pc = list(struct.unpack_from("<3h", args)), ch.pc + 6
    elif op == 0xE6:
        ease(list(struct.unpack_from("<3h", args)), args[6])
        ch.pc += 7
    elif op == 0xE1:
        ch.pos = list(view.cameras[view.chosen][k])
    elif op == 0xE2:
        ease(view.cameras[view.chosen][k], args[0])
        ch.pc += 1
    elif op == 0xE0:
        p, n = args[0], args[1]
        ch.pc += 2
        a = view.actors[view.target]
        x, y, z = view.settings[view.target]["points"][p][k]
        ease([x + a.pos[0], y + (a.pos[1] if p else 0), z + a.pos[2]], n)
    elif op == 0xDD:
        view.chosen, ch.pc = args[0], ch.pc + 1
    elif op == 0xDE:
        view.target = 3 if args[0] else view.target
        ch.pc += 1
    elif op == 0xDF:
        view.target = 0xF
    elif op == 0xD9:
        ch.pos = list(view.current[k])
    elif op in ((0xDA, 0xDB), (0xDB, 0xDC))[k]:
        ch.linear = op != (0xDA, 0xDB)[k]
    elif op in (0xDC, 0xF1) and not k:
        ch.turn = op == 0xDC
    elif op == 0xF8 and not k:
        step, r, angle, grow, rise, n = struct.unpack_from("<6h", args)
        ch.pc += 12
        tasks.append(["orbit", ch, step, r, angle, grow, rise, n])
    elif op in (0xD5, 0xD6, 0xD7) and not k:  # sounds and effects
        ch.pc += {0xD5: 2, 0xD6: 0, 0xD7: 2}[op]
    elif op == 0xF5:
        ch.wait, ch.pc = args[0], ch.pc + 1
    elif op == 0xF4:
        if not ch.wait:
            return True
        ch.wait, ch.pc = ch.wait - 1, ch.pc - 1
        return False
    elif op == 0xFE:
        if not ch.wait and args[0] == 0xC0:
            ch.pc = 0
    else:  # FF holds; any other opcode ends the frame
        ch.pc -= op == 0xFF
        return False
    return True
