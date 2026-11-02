"""Dump the field's 3D models (FIELD/*.BCX, the party, and each map's .BSX) to files people can edit, and build them
back.

  python tools/model.py dump "assets/Final Fantasy VII (USA) (Disc 1)" models
  python tools/model.py build models assets

A JSON file per model file. Bones (length, parent), parts (the bone they hang on, vertices, UVs, texture words,
polygons with their vertex indices, colours as RRGGBBNN where NN picks the normal the map's lights shine on, UV
indices and flags) and animations (per bone three angles, 256 to a turn, and three offsets, each fixed or a channel
with a value per frame). A .BSX adds per model its lights and where its bones, parts and animations go in the
model, and its texture block: VRAM rectangles as hex, and patches. "at" says where a part's or animation's data was:
the ones that shared data share it again, and new ones go last. build writes the files whose models changed into
every disc folder under assets, keeping the first originals as .orig, and their sizes into FIELD.BIN.
"""
import argparse
import json
import math
import re
import struct
from pathlib import Path

import field
import kernel
from unpack import lzs

KINDS = ["GT4", "GT3", "FT4", "FT3", "F3", "F4", "G3", "G4"]  # a part's polygons in this order (func_800AE4DC)
CORNERS = {"GT4": 4, "GT3": 3, "FT4": 4, "FT3": 3, "F3": 3, "F4": 4, "G3": 3, "G4": 4}
COLORS = {"GT4": 4, "GT3": 3, "FT4": 1, "FT3": 1, "F3": 1, "F4": 1, "G3": 3, "G4": 4}
TEXTURED = KINDS[:4]
PACKET = {"GT4": 0x34, "GT3": 0x28, "FT4": 0x28, "FT3": 0x20, "F3": 0x14, "F4": 0x18, "G3": 0x1C, "G4": 0x24}
PATCH_DATA = {1: 0x200, 2: 32}  # a FIELD.TDB image, a palette; types 0 and 3 carry their size
BCX_SIZES = 0x3F08C  # FIELD.BIN unpacked: a sector and size per .BCX (func_800ADC90)


def u16(d: bytes, at: int) -> int:
    return int.from_bytes(d[at:at + 2], "little")


def u32(d: bytes, at: int) -> int:
    return int.from_bytes(d[at:at + 4], "little")


def padding(d: bytes, end: int) -> dict:
    """The bytes that 4-align a part's or animation's data: zero, or left over from something else, kept."""
    pad = d[end:end + -end % 4]
    return {"pad": pad.hex()} if any(pad) else {}


def padded(data: bytes, thing: dict) -> bytes:
    pad = bytes.fromhex(thing.get("pad", ""))
    return data + (pad if len(pad) == -len(data) % 4 else bytes(-len(data) % 4))


def part_json(d: bytes, p: int) -> dict:
    off = u32(d, p + 0x18) - 0x80000000
    nv, nuv, ntw = d[p + 2], d[p + 3], d[p + 0xC]
    polys, uvs, words, flags = (u16(d, p + o) for o in (0xE, 0x10, 0x12, 0x14))
    at, textured, out = off + polys, 0, []
    for kind, count in zip(KINDS, d[p + 4:p + 0xC]):
        for _ in range(count):
            poly = {"kind": kind, "vertices": list(d[at:at + CORNERS[kind]])}
            colors = at + 4
            if kind in TEXTURED:
                uv = colors + 4 * COLORS[kind]
                poly["uv"] = list(d[uv:uv + 4])
                poly["flags"] = d[off + flags + textured]
                textured += 1
            poly["colors"] = [d[colors + 4 * i:colors + 4 * i + 4].hex().upper() for i in range(COLORS[kind])]
            out.append(poly)
            at += 4 + 4 * COLORS[kind] + 4 * (kind in TEXTURED)
    return {"bone": d[p + 1], "at": off,
            "vertices": [list(struct.unpack_from("<3h", d, off + 4 + 8 * i)) for i in range(nv)],
            "uvs": [[d[off + uvs + 2 * i], d[off + uvs + 2 * i + 1]] for i in range(nuv)],
            "textures": [f"{u32(d, off + words + 4 * i):08X}" for i in range(ntw)], "polygons": out,
            **padding(d, off + flags + textured)}


def part_raw(part: dict, bones: int) -> tuple[bytes, bytes]:
    """A part's data and its 32-byte header (the data's place left for the caller)."""
    nv, nuv, ntw = len(part["vertices"]), len(part["uvs"]), len(part["textures"])
    if not 0 <= part["bone"] < bones:
        raise ValueError(f"bone {part['bone']}: the model has {bones}")
    polys = sorted(part["polygons"], key=lambda poly: KINDS.index(poly["kind"]) if poly["kind"] in KINDS else -1)
    records, flags = bytearray(), bytearray()
    for poly in polys:
        kind = poly["kind"]
        if kind not in KINDS:
            raise ValueError(f"no polygon kind {kind}; there are {', '.join(KINDS)}")
        if len(poly["vertices"]) != CORNERS[kind] or len(poly["colors"]) != COLORS[kind]:
            raise ValueError(f"a {kind} has {CORNERS[kind]} vertices and {COLORS[kind]} colours")
        if max(poly["vertices"]) >= nv:
            raise ValueError(f"a {kind} uses vertex {max(poly['vertices'])}; the part has {nv}")
        records += bytes(poly["vertices"]).ljust(4, b"\0") + b"".join(bytes.fromhex(c) for c in poly["colors"])
        if kind in TEXTURED:
            if len(poly["uv"]) != 4:
                raise ValueError(f"a {kind} has 4 UV indices (a triangle's 4th unused)")
            if max(poly["uv"][:CORNERS[kind]]) >= nuv or poly["flags"] & 15 >= ntw:
                raise ValueError(f"a {kind} uses a UV or texture word the part hasn't")
            records += bytes(poly["uv"])
            flags.append(poly["flags"])
    counts = [sum(poly["kind"] == kind for poly in polys) for kind in KINDS]
    start = 4 + 8 * nv
    words = start + len(records)
    data = (bytes(4) + b"".join(struct.pack("<3hH", *v, 0) for v in part["vertices"]) + records
            + b"".join(struct.pack("<I", int(w, 16)) for w in part["textures"])
            + b"".join(bytes(uv) for uv in part["uvs"]) + flags)
    if max([nv, nuv, ntw, len(flags)] + counts) > 255 or len(data) > 0xFFFF:
        raise ValueError("a part takes at most 255 of each thing and 64 KB")
    head = (bytes([1, part["bone"], nv, nuv, *counts, ntw, len(flags)])
            + struct.pack("<5H", start, words + 4 * ntw, words, words + 4 * ntw + 2 * nuv,
                          sum(PACKET[k] * n for k, n in zip(KINDS, counts))))
    return padded(data, part), head


def anim_json(d: bytes, a: int) -> dict:
    frames, bones, moving, fixed, turning = u16(d, a), d[a + 2], d[a + 3], d[a + 4], d[a + 5]
    offsets, values, rotations = u16(d, a + 6), u16(d, a + 8), u16(d, a + 0xA)
    off = u32(d, a + 0xC) - 0x80000000
    return {"at": off, "frames": frames,
            "bones": [{"flags": d[off + 4 + 8 * b], "rotation": list(d[off + 5 + 8 * b:off + 8 + 8 * b]),
                       "offset": list(d[off + 8 + 8 * b:off + 11 + 8 * b])} for b in range(bones)],
            "offsets": [list(struct.unpack_from(f"<{frames}h", d, off + offsets + 2 * frames * c))
                        for c in range(moving)],
            "fixed": list(struct.unpack_from(f"<{fixed}h", d, off + values)),
            "rotations": [list(d[off + rotations + frames * c:off + rotations + frames * (c + 1)])
                          for c in range(turning)], **padding(d, off + rotations + frames * turning)}


def anim_raw(anim: dict) -> tuple[bytes, bytes]:
    frames, bones = anim["frames"], anim["bones"]
    moving, fixed, turning = len(anim["offsets"]), len(anim["fixed"]), len(anim["rotations"])
    if any(len(c) != frames for c in anim["offsets"] + anim["rotations"]):
        raise ValueError(f"a channel's frames differ from the animation's {frames}")
    for bone in bones:
        for k in range(3):
            r, t = bone["rotation"][k], bone["offset"][k]
            if bone["flags"] >> k & 1 and r >= turning or bone["flags"] >> 4 + k & 1 and t >= moving:
                raise ValueError("a bone uses a channel the animation hasn't")
            if not bone["flags"] >> 4 + k & 1 and t != 255 and t >= fixed:
                raise ValueError("a bone uses a fixed offset the animation hasn't")
    data = (bytes(4) + b"".join(bytes([b["flags"], *b["rotation"], *b["offset"], 0]) for b in bones)
            + b"".join(struct.pack(f"<{frames}h", *c) for c in anim["offsets"])
            + struct.pack(f"<{fixed}h", *anim["fixed"]) + b"".join(bytes(c) for c in anim["rotations"]))
    start = 4 + 8 * len(bones)
    head = struct.pack("<H4B3H", frames, len(bones), moving, fixed, turning, start, start + 2 * frames * moving,
                       start + 2 * frames * moving + 2 * fixed)
    return padded(data, anim), head


def angles(anim: dict | None, bone: int, frame: int) -> list[int]:
    """A bone's three angles in a frame (bytes, 256 to a turn); a bone the animation lacks keeps 0."""
    if anim is None or bone >= len(anim["bones"]):
        return [0, 0, 0]
    record = anim["bones"][bone]
    return [anim["rotations"][r][frame] if record["flags"] >> k & 1 else r for k, r in enumerate(record["rotation"])]


def rotation(rx: int, ry: int, rz: int) -> list[list[float]]:
    """Ry Rx Rz, as func_800AEE24 builds a bone's turn."""
    (sx, cx), (sy, cy), (sz, cz) = ((math.sin(a * math.pi / 128), math.cos(a * math.pi / 128)) for a in (rx, ry, rz))
    ry_ = [[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]
    rx_ = [[1, 0, 0], [0, cx, -sx], [0, sx, cx]]
    rz_ = [[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]]
    return matmul(matmul(ry_, rx_), rz_)


def matmul(a: list, b: list) -> list[list[float]]:
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def apply(m: list, v) -> list[float]:
    return [sum(m[i][k] * v[k] for k in range(3)) for i in range(3)]


def offsets(anim: dict | None, bone: int, frame: int) -> list[int]:
    """A bone's x, y, z offset in a frame: a moving channel's (flag bits 4-6), a fixed one, or 0 (index FF)."""
    if anim is None or bone >= len(anim["bones"]):
        return [0, 0, 0]
    record = anim["bones"][bone]
    return [anim["offsets"][t][frame] if record["flags"] >> 4 + k & 1 else 0 if t == 0xFF else anim["fixed"][t]
            for k, t in enumerate(record["offset"])]


def pose(bones: list, anim: dict | None, frame: int) -> list[tuple[list, list]]:
    """Each bone's turn and start in a frame: its parent's turn times its own, and its parent's start plus the
    parent's turn of (x, y, its length + z), x, y, z its offsets (func_800AEE24 applies them whenever the field
    draws a model). Vertices of a part on the bone go to turn x vertex + start."""
    out = []
    for i, bone in enumerate(bones):
        turn, start = ([[1, 0, 0], [0, 1, 0], [0, 0, 1]], [0, 0, 0]) if bone["parent"] < 0 else out[bone["parent"]]
        x, y, z = offsets(anim, i, frame)
        out.append((matmul(turn, rotation(*angles(anim, i, frame))),
                    [s + d for s, d in zip(start, apply(turn, (x, y, bone["length"] + z)))]))
    return out


def light(color: str, normal, lights: dict) -> tuple[int, int, int]:
    """A colour as the GTE lights it at load (nccs, func_800B0A48): the normal against each light, clamped at 0,
    times the light's colour, plus the ambient (bytes x 16, 4096 being 1), times the stored colour."""
    rgb = bytes.fromhex(color[:6])
    ambient = bytes.fromhex(lights["ambient"])
    colors = [bytes.fromhex(c) for c in lights["light_colors"]]
    strength = [max(0, sum(a * b for a, b in zip(v, normal)) >> 12) for v in lights["light_vectors"]]
    out = []
    for c in range(3):
        total = (ambient[c] << 16) + sum((col[c] << 4) * s for col, s in zip(colors, strength)) >> 12
        total = min(0x7FFF, max(0, total))
        out.append(min(255, rgb[c] * total >> 12))
    return tuple(out)


def model_json(d: bytes, at: int, bones: int, parts: int, anims: int) -> dict:
    p, a = at + 4 * bones, at + 4 * bones + 32 * parts
    return {"bones": [{"length": struct.unpack_from("<h", d, at + 4 * i)[0],
                       "parent": struct.unpack_from("<b", d, at + 4 * i + 2)[0]} for i in range(bones)],
            "parts": [part_json(d, p + 32 * i) for i in range(parts)],
            "animations": [anim_json(d, a + 16 * i) for i in range(anims)]}


class Data:
    """The data area from byte 8: each part's and animation's data, once per "at", in the order the originals sat;
    one edited away from what it shared, or new, goes after them."""

    def __init__(self):
        self.blobs, self.extra = {}, []

    def add(self, at, data: bytes) -> list:
        if at is not None and self.blobs.setdefault(at, data) == data:
            return ["at", at]
        self.extra.append(data)
        return ["extra", len(self.extra) - 1]

    def place(self) -> dict:
        where, at = {}, 8
        for key, data in [(("at", k), self.blobs[k]) for k in sorted(self.blobs)] + \
                         [(("extra", i), data) for i, data in enumerate(self.extra)]:
            where[key] = at
            at += len(data)
        return where

    def bytes(self) -> bytes:
        return b"".join([self.blobs[k] for k in sorted(self.blobs)] + self.extra)


def model_raw(model: dict, area: Data) -> list:
    """The model's bones, part headers and animation records, waiting for their data's place."""
    out = [b"".join(struct.pack("<hbB", b["length"], b["parent"], any(p["bone"] == i for p in model["parts"]))
                    for i, b in enumerate(model["bones"]))]
    for part in model["parts"]:
        data, head = part_raw(part, len(model["bones"]))
        out.append((head, area.add(part.get("at"), data)))
    for anim in model["animations"]:
        data, head = anim_raw(anim)
        out.append((head, area.add(anim.get("at"), data)))
    return out


def resolve(pieces: list, where: dict) -> bytes:
    out = bytearray()
    for piece in pieces:
        if isinstance(piece, bytes):
            out += piece
        else:
            head, key = piece
            out += head + struct.pack("<I", 0x80000000 | where[tuple(key)]) + (bytes(4) if len(head) == 0x18 else b"")
    return bytes(out)


def bcx_json(d: bytes) -> dict:
    h = u32(d, 4)
    return {"scale": u16(d, h + 0x16), **model_json(d, h + 0x24, d[h + 2], d[h + 3], d[h + 4])}


def bcx_raw(table: dict) -> bytes:
    area = Data()
    pieces = model_raw(table, area)
    where = area.place()
    h = 8 + len(area.bytes())
    bones, parts, anims = len(table["bones"]), len(table["parts"]), len(table["animations"])
    if max(bones, parts, anims) > 255:
        raise ValueError("a model takes at most 255 bones, parts and animations")
    header = (bytes([1, 0xFF, bones, parts, anims]) + bytes(0x11)
              + struct.pack("<3HI4x", table["scale"], 4 * bones, 4 * bones + 32 * parts, 0x80000000 + h + 0x24))
    body = area.bytes() + header + resolve(pieces, where)
    return struct.pack("<2I", 8 + len(body), h) + body


def rgb(d: bytes, at: int) -> str:
    return d[at:at + 3].hex().upper()


def textures_json(d: bytes, t: int) -> dict:
    count, patches = d[t + 4], d[t + 5] << 16 | u16(d, t + 6)
    out = {"images": []}
    for i in range(count):
        w, h, x, y, o = struct.unpack_from("<4HI", d, t + 8 + 12 * i)
        image = {"x": x, "y": y, "w": w, "h": h, "data": d[t + o:t + o + 2 * w * h].hex()}
        pad = d[t + o + 2 * w * h:t + o + 2 * w * h + (-2 * w * h) % 4]
        if any(pad):
            image["pad"] = pad.hex()
        out["images"].append(image)
    if not count:
        out["unused"] = d[t + 8:t + 20].hex()
    if patches:
        p = t + patches
        out["patch_word"] = u32(d, p + 4)
        out["patches"] = []
        for i in range(u32(d, p)):
            kind, o, size, dest, last = struct.unpack_from("<5I", d, p + 8 + 20 * i)
            out["patches"].append({"type": kind, "size": size, "dest": dest, "word": last,
                                   "data": d[p + o:p + o + PATCH_DATA.get(kind, size)].hex()})
    return out


def textures_raw(table: dict) -> bytes:
    images = table["images"]
    out = bytearray(8) + (bytes.fromhex(table["unused"]) if not images else bytes(12 * len(images)))
    for i, image in enumerate(images):
        data = bytes.fromhex(image["data"])
        if len(data) != 2 * image["w"] * image["h"]:
            raise ValueError(f"image {i}: {len(data)} bytes for {image['w']}x{image['h']}")
        out[8 + 12 * i:20 + 12 * i] = struct.pack("<4HI", image["w"], image["h"], image["x"], image["y"], len(out))
        out += data + (bytes.fromhex(image["pad"]) if "pad" in image else bytes(-len(data) % 4))
    patches = table.get("patches")
    size = len(out)
    if patches is not None:
        p = len(out)
        out += struct.pack("<2I", len(patches), table["patch_word"]) + bytes(20 * len(patches))
        for i, patch in enumerate(patches):
            data = bytes.fromhex(patch["data"])
            if len(data) != PATCH_DATA.get(patch["type"], patch["size"]):
                raise ValueError(f"patch {i}: {len(data)} bytes of data for its type and size")
            out[p + 8 + 20 * i:p + 28 + 20 * i] = struct.pack("<5I", patch["type"], len(out) - p, patch["size"],
                                                             patch["dest"], patch["word"])
            out += data + bytes(-len(data) % 4)
    at = size if patches is not None else 0  # a 24-bit offset to the patches, its high byte first
    out[0:8] = struct.pack("<IBBH", at or len(out), len(images), at >> 16, at & 0xFFFF)
    return bytes(out)


def bsx_json(d: bytes) -> dict:
    h = u32(d, 4)
    models = []
    for i in range(u32(d, h + 4)):
        e = h + 0x10 + 0x30 * i
        bones, parts, anims = d[e + 0x17], d[e + 0x23], d[e + 0x2F]
        models.append({"face": d[e], "bcx": d[e + 1], "scale": u16(d, e + 2), "ambient": rgb(d, e + 8),
                       "unknown_0B": d[e + 0xB],
                       "light_colors": [rgb(d, e + o) for o in (0x14, 0x20, 0x2C)],
                       "light_vectors": [list(struct.unpack_from("<3h", d, e + o)) for o in (0xC, 0x18, 0x24)],
                       "first_bone": struct.unpack_from("<b", d, e + 0x12)[0],
                       "first_part": struct.unpack_from("<b", d, e + 0x1E)[0],
                       "first_animation": struct.unpack_from("<b", d, e + 0x2A)[0],
                       **model_json(d, e + u32(d, e + 4), bones, parts, anims)})
    return {"unknown_0C": u32(d, h + 0xC), "models": models, "textures": textures_json(d, h + u32(d, h + 8))}


def bsx_raw(table: dict) -> bytes:
    area = Data()
    pieces = [model_raw(model, area) for model in table["models"]]
    where = area.place()
    h = 8 + len(area.bytes())
    n = len(table["models"])
    entries, blocks = bytearray(), bytearray()
    for i, (model, block) in enumerate(zip(table["models"], pieces)):
        counts = len(model["bones"]), len(model["parts"]), len(model["animations"])
        firsts = model["first_bone"], model["first_part"], model["first_animation"]
        if max(counts) > 127 or any(c and not 0 <= f <= 127 - c for f, c in zip(firsts, counts)):
            raise ValueError(f"model {i}: its bones, parts and animations must fit 0-127 from their first")
        ranges = [(f, f + c - 1) if c else (-1, -1) for f, c in zip(firsts, counts)]
        colors = [bytes.fromhex(c) for c in model["light_colors"]]
        vectors = [struct.pack("<3h", *v) for v in model["light_vectors"]]
        start = 0x10 + 0x30 * n + len(blocks)
        entries += (bytes([model["face"], model["bcx"]]) + struct.pack("<HI", model["scale"], start - 0x10 - 0x30 * i)
                    + bytes.fromhex(model["ambient"]) + bytes([model["unknown_0B"]]) + vectors[0]
                    + struct.pack("<2b", *ranges[0]) + colors[0] + bytes([counts[0]]) + vectors[1]
                    + struct.pack("<2b", *ranges[1]) + colors[1] + bytes([counts[1]]) + vectors[2]
                    + struct.pack("<2b", *ranges[2]) + colors[2] + bytes([counts[2]]))
        blocks += resolve(block, where)
    head = struct.pack("<4I", 0x80000000 + h, n, 0x10 + 0x30 * n + len(blocks), table["unknown_0C"])
    body = area.bytes() + head + entries + blocks + textures_raw(table["textures"])
    return struct.pack("<2I", 8 + len(body), h) + body


def files(disc: Path) -> list[Path]:
    """The .BCX and .BSX files that are models (75 .BSX are 2 KB stubs whose LZS length doesn't match)."""
    return [p for p in sorted((disc / "FIELD").glob("*.B[CS]X")) if u32(p.read_bytes(), 0) == p.stat().st_size - 4]


def to_json(name: str, d: bytes) -> dict:
    return bcx_json(d) if name.upper().endswith(".BCX") else bsx_json(d)


def to_raw(name: str, table: dict) -> bytes:
    return bcx_raw(table) if name.upper().endswith(".BCX") else bsx_raw(table)


def write_json(path: Path, table: dict):
    """One line per short list (a vertex, a polygon's colours), so a model reads and diffs line by line."""
    text = json.dumps(table, indent=1, ensure_ascii=False)
    text = re.sub(r"\[\s+([^\[\]{}]*?)\s+\]", lambda m: "[" + " ".join(m.group(1).split()) + "]", text)
    path.write_text(text + "\n", "utf-8")


def dump(disc: Path, out: Path):
    out.mkdir(parents=True, exist_ok=True)
    for path in files(disc):
        write_json(out / f"{path.name}.json", to_json(path.name, lzs(path.read_bytes())))


def build(folder: Path, assets: Path):
    for disc in kernel.discs(assets):
        restored = [o.stem for o in sorted((disc / "FIELD").glob("*.B[CS]X.orig"))]
        for name in restored:  # start from the originals: a dump says it all
            kernel.replace(disc, f"FIELD/{name}", (disc / f"FIELD/{name}.orig").read_bytes())
        image = bytearray(field.module(disc))
        changed = set(restored)
        for path in sorted(folder.glob("*.json")):
            name = path.name[:-5]
            packed = (disc / "FIELD" / name).read_bytes()
            old = lzs(packed)
            try:
                new = to_raw(name, json.loads(path.read_text("utf-8")))
            except ValueError as e:
                raise SystemExit(f"{path}: {e}")
            if new == old:
                continue
            start = next((i for i, (a, b) in enumerate(zip(old, new)) if a != b), min(len(old), len(new)))
            kernel.replace(disc, f"FIELD/{name}", field.repack(packed, new, start))
            changed.add(name)
        for name in changed:
            lba, size = next((int(r[0]), int(r[1])) for r in (line.split(maxsplit=3) for line in
                             (disc / "files.txt").read_text().splitlines()) if r[3] == f"FIELD/{name}")
            pairs = [BCX_SIZES + 8 * i for i in range(9)] if name.endswith(".BCX") else \
                [at + 0x10 for at in range(field.MAPS, len(image) - 0x18, 24)]
            for at in pairs:
                if u32(image, at) == lba:
                    image[at + 4:at + 8] = size.to_bytes(4, "little")
        field.repack_module(disc, "FIELD/FIELD.BIN", bytes(image))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("dump", help="the models from a disc folder into a folder of JSON files")
    d.add_argument("disc", type=Path)
    d.add_argument("out", type=Path)
    b = sub.add_parser("build", help="a folder of JSON files into every disc folder's model files")
    b.add_argument("dump", type=Path)
    b.add_argument("assets", type=Path)
    args = ap.parse_args()
    if args.command == "dump":
        dump(args.disc, args.out)
    else:
        build(args.dump, args.assets)


if __name__ == "__main__":
    main()
