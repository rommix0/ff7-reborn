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
import struct
from pathlib import Path


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
    return {"bones": bones, "parts": parts, "animations": anims, "tim": tim(s[texture])}


def files(disc: Path) -> list[Path]:
    """The battle's models: SEFFECT.LZS, the effects' own, isn't one."""
    return [p for p in sorted(disc.glob("ENEMY[1-6]/*.LZS")) if p.stem != "SEFFECT"]


def stage_json(d: bytes) -> dict:
    """A stage (STAGEn/STAGEnn.LZS, location nn): section 0 a type and a first section, the models (parts in world
    coordinates) and the TIM last. func_800B9568 draws a model in one of three lists: the far layer (16 coarse
    depths) first, then the backdrop (one list, the last linked drawn first), then the main ordering table with the
    actors. Type 1 (most stages) puts section 1 and 5 up to first in the backdrop, 2-4 in the far layer; type 5 the
    same with the backdrop's in the main table. Types 0, 2, 3 and 4 (a train, an elevator shaft, two with spinning
    parts) move some of their models each frame; they're drawn where they start (type 4's spinning ones 2500 up)."""
    s = sections(d)
    kind, first = s[0][0], s[0][1]
    out = {"type": kind, "models": [], "tim": tim(s[-1])}
    for k in range(1, len(s) - 1):
        match kind:
            case 1 | 5:
                far = 2 <= k <= 4
                layer = "far" if far else "backdrop" if kind == 1 and k < first else "main"
            case 0:
                layer = "far" if 2 <= k <= 4 else "main"
            case 3:
                layer = "backdrop" if k == 1 else "far" if k <= 4 else "main"
            case 4:
                layer = "far" if k <= 5 else "backdrop"
            case _:
                layer = "main"
        part = part_json(s[k], 0, 0)
        if kind == 4 and k >= 6:
            part["vertices"] = [[x, y - 2500, z] for x, y, z in part["vertices"]]
        out["models"].append({"section": k, "layer": layer, "part": part})
    return out


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
