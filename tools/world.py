"""Dump the world map's event scripts to files people can edit, and build them back.

  python tools/world.py dump "assets/Final Fantasy VII (USA) (Disc 1)" world
  python tools/world.py build world assets

The scripts sit in each WORLD/WM*.TXZ (the loose WM*.EV files are older copies the game doesn't read): one set in
WM0-WMA and their S versions, one each in WMB and WMC, dumped as WM0.json, WMB.json and WMC.json. A file lists the
scripts (a key: "system N", an entity's function, or a map block's, and the label where it starts) and the code, a
line per operation: operators (add, eq, set, ...), pushes (#5, byte[0A3], word.1[4], bit.3[F]: a constant, or a
bit, byte or u16 of the save data, a scratch area 1 or 2, or special value 3; saved bytes and u16s by their field
names), jumps to labels, run N (function N of the entity on the stack), and commands as op_3XX. build writes every
container whose scripts changed into every disc folder under assets, keeping the first originals as .orig, and the
containers' sizes into WORLD.BIN. A container is packed again only from its scripts on, and must fit the sectors it
had: there's room for a few dozen packed bytes more in the tightest (WM1.TXZ).
"""
import argparse
import gzip
import json
import struct
from pathlib import Path

import field
import kernel
from unpack import lzs

BLOCK, MESH = 0xB800, 8192  # a map file's blocks (16 meshes each); a mesh's side (the world: 36 by 28, wrapping)
# map_stage's swaps (func_800A54F0): world block -> (the stage it's swapped from, the block of the file that shows)
SWAPS = {0x32: (1, 0x3F), 0x29: (2, 0x40), 0x2A: (2, 0x41), 0x3C: (3, 0x42), 0x2F: (4, 0x43), 0x30: (4, 0x44)}
SECTION, CODE, SIZE = 4, 0x400, 0x7000  # the container's section func_800A0D2C copies; the code after the table
SETS = {"WMB": "WMB", "WMC": "WMC"}  # the rest carry WM0's
SIZES = [0x273E4, 0x2744C]  # WORLD.BIN unpacked: a sector and size per WM0-WMC.TXZ, then per WM0S-WMCS
OPERATORS = {0x15: "neg", 0x17: "not", 0x30: "mul", 0x40: "add", 0x41: "sub", 0x50: "shl", 0x51: "shr",
             0x60: "lt", 0x61: "gt", 0x62: "le", 0x63: "ge", 0x70: "eq", 0x71: "ne", 0x80: "and", 0xA0: "or",
             0xB0: "land", 0xC0: "lor", 0xE0: "set"}  # func_800ABFC0
# 18-1B measure from the current entity: to a point, an entity, a position 344 saved; the angle to a point.
MEASURES = {0x18: "point_distance", 0x19: "entity_distance", 0x1A: "saved_pos_distance", 0x1B: "point_angle"}
# Commands (func_800AC700) by what their handlers do; most act on the current entity, and the wait_ ones run again
# each frame until what they wait on is done.
COMMANDS = {
    0x300: "spawn_entity", 0x302: "set_player", 0x303: "set_speed", 0x304: "set_direction", 0x305: "set_wait",
    0x306: "wait_move", 0x307: "set_player_control", 0x308: "set_block", 0x309: "set_block_pos",
    0x30A: "set_height_step", 0x30B: "set_height_offset", 0x30C: "board", 0x30E: "play_animation",
    0x310: "point_select", 0x311: "point_block", 0x312: "point_pos", 0x313: "point_rgb1", 0x314: "point_range",
    0x315: "point_rgb2", 0x316: "point_rgb3", 0x317: "battle", 0x318: "enter_field", 0x319: "set_view",
    0x31C: "camera_mode", 0x31D: "play_sound", 0x31F: "camera_turn_speed", 0x321: "face_point", 0x324: "set_window",
    0x325: "message", 0x326: "ask", 0x327: "wait_answer", 0x328: "set_move_direction", 0x329: "camera_tilt_speed",
    0x32A: "camera_zoom_speed", 0x32B: "set_encounters", 0x32D: "close_window", 0x32E: "wait_window",
    0x32F: "set_camera_yaw", 0x330: "select_entity", 0x333: "face_entity", 0x334: "wait_run", 0x336: "set_speed_x16",
    0x33A: "set_vertical_speed", 0x33B: "fade_out", 0x33D: "enter_field_now", 0x33E: "play_music",
    0x344: "save_pos", 0x345: "face_saved_pos", 0x347: "move_to_entity", 0x348: "fade_in", 0x349: "map_stage",
    0x34A: "spawn_effect", 0x34D: "effect_emitter", 0x34E: "effect_emitter_stop", 0x34F: "set_y",
    0x351: "music_volume", 0x352: "screen_shake", 0x353: "restore_player_pos", 0x354: "set_active", 0x355: "set_timer",
}
NAMES = {**OPERATORS, **MEASURES, **COMMANDS, 0x100: "reset", 0x200: "goto", 0x201: "ifnot", 0x203: "ret"}
# Special values (area 3, jtbl_800A0268), whatever the push's kind: the current entity's block and place in it, its
# direction and model id, the last field exit, the view (as set_view takes it), the player's model id, the last
# answer (-1 while the window is open), the random table's next byte.
SPECIALS = {0: "block_x", 1: "block_z", 2: "x", 3: "z", 4: "direction", 5: "exit_kind", 6: "exit_key",
            7: "view_mode", 8: "player_id", 0xC: "answer", 0xF: "entity_id", 0x10: "random"}
OPCODES = {name: op for op, name in NAMES.items()}
KINDS = {0x114: "bit", 0x118: "byte", 0x11C: "word"}  # func_800ABB24; 0x110 is the value itself
BYTE_BANKS, WORD_BANKS = [1, 3, 11, 13, 15], [2, 4, 12, 14, 7]  # the field's banks for each 256 bytes of save data


def u16(data: bytes, at: int) -> int:
    return int.from_bytes(data[at:at + 2], "little")


def operand(op: int, value: int) -> str:
    kind, bank = op & ~3, op & 3
    if op == 0x110:
        return f"#{value}"
    if kind not in KINDS:
        return ""
    if bank == 0 and kind != 0x114 and value < 0x500:
        key = ((BYTE_BANKS if kind == 0x118 else WORD_BANKS)[value >> 8], value & 0xFF)
        if key in field.VARS:
            return field.VARS[key]
    if bank == 3 and value in SPECIALS:
        return f"{KINDS[kind]}.{SPECIALS[value]}"
    return f"{KINDS[kind]}{f'.{bank}' if bank else ''}[{value:X}]"


def disassemble(code: bytes, starts: set | None = None) -> list[str]:
    """A line per operation, with a label wherever a jump lands or a script starts."""
    words = [u16(code, 2 * i) for i in range(len(code) // 2)]
    end = max((i + 1 for i, w in enumerate(words) if w), default=0)
    ops, at = [], 0
    while at < end:
        size = 2 if 0x101 <= words[at] <= 0x201 else 1
        ops.append((at, words[at:at + size]))
        at += size
    labels = {op[1] for _, op in ops if op[0] in (0x200, 0x201)} | (starts or set())
    return [line for at, op in ops for line in disassemble_one(at, op, labels)]


def disassemble_one(at: int, op: list[int], labels: set) -> list[str]:
    head = [f"L{at:04X}:"] if at in labels else []
    w = op[0]
    if w in (0x200, 0x201):
        return head + [f"{NAMES[w]} L{op[1]:04X}"]
    if 0x101 <= w <= 0x1FF:
        text = operand(w, op[1])
        return head + [f"push {text}" if text else f"push_{w:03X} {op[1]:04X}"]
    if 0x204 <= w <= 0x2FF:
        return head + [f"run {w - 0x204}"]
    return head + [NAMES.get(w, f"op_{w:03X}")]


def push_value(text: str) -> tuple[int, int]:
    if text.startswith("#"):
        return 0x110, int(text[1:]) & 0xFFFF
    if text in field.NAMED:
        bank, address = field.NAMED[text]
        kind = 0x118 if bank in BYTE_BANKS else 0x11C
        return kind, (BYTE_BANKS if kind == 0x118 else WORD_BANKS).index(bank) << 8 | address
    name, _, rest = text.partition("[")
    name, _, bank = name.partition(".")
    kind = next((k for k, n in KINDS.items() if n == name), None)
    special = next((i for i, n in SPECIALS.items() if n == bank), None)
    if kind is not None and special is not None and not rest:
        return kind | 3, special
    if kind is None or not rest.endswith("]") or bank not in ("", "1", "2", "3"):
        raise ValueError(f"{text} isn't #value, a variable's name, bit/byte/word[.bank][address] or a special")
    return kind | int(bank or 0), int(rest[:-1], 16)


def assemble(lines: list[str], labels: dict | None = None) -> bytes:
    """Words from disassemble()'s lines; labels, if given, gets where each label is."""
    words, jumps = [], []
    labels = {} if labels is None else labels
    for line in lines:
        line = line.split(";")[0].strip()
        if not line:
            continue
        if line.endswith(":"):
            labels[line[:-1]] = len(words)
            continue
        name, *args = line.split()
        try:
            if name in ("goto", "ifnot") and len(args) == 1:
                jumps.append((len(words) + 1, args[0], line))
                words += [OPCODES[name], 0]
            elif name == "push" and len(args) == 1:
                words += push_value(args[0])
            elif name.startswith("push_") and len(args) == 1:
                words += [int(name[5:], 16), int(args[0], 16)]
            elif name == "run" and len(args) == 1:
                words.append(0x204 + int(args[0]))
            elif name in OPCODES and not args:
                words.append(OPCODES[name])
            elif name.startswith("op_") and not args:
                words.append(int(name[3:], 16))
            else:
                raise ValueError(f"no operation {name} with {len(args)} operands")
        except ValueError as e:
            raise ValueError(f"{line!r}: {e}")
    for at, label, line in jumps:
        if label not in labels:
            raise ValueError(f"{line!r}: no label {label}")
        words[at] = labels[label]
    if not all(0 <= w < 0x10000 for w in words):
        raise ValueError("a word out of 0-FFFF")
    return b"".join(w.to_bytes(2, "little") for w in words)


def what(key: int) -> str:
    if key < 0x4000:
        return f"system {key}"
    if key < 0x8000:
        return f"entity {key >> 8 & 0x3F:02X} function {key & 0xFF}"
    block = key >> 4 & 0x3FF
    return f"block {block % 36},{block // 36} type {(key & 0xF) + 3}"  # func_800ABA78


def scripts_json(section: bytes) -> dict:
    """The table (func_800AB6E4 searches entries 1-255 by key) and the code from 0x400."""
    entries = [(u16(section, 4 * i), u16(section, 4 * i + 2)) for i in range(1, CODE // 4)]
    return {"scripts": [{"key": f"{key:04X}", "_what": what(key), "at": f"L{at:04X}"}
                        for key, at in entries if key != 0xFFFF],
            "code": disassemble(section[CODE:], {at for key, at in entries if key != 0xFFFF})}


def scripts_raw(table: dict) -> bytes:
    labels = {}
    code = assemble(table["code"], labels)
    entries = []
    for script in table["scripts"]:
        if script["at"] not in labels:
            raise ValueError(f"script {script['key']}: no label {script['at']} in the code")
        entries.append((int(script["key"], 16), labels[script["at"]]))
    if len(entries) > CODE // 4 - 1:
        raise ValueError(f"{len(entries)} scripts; the table has room for {CODE // 4 - 1}")
    if len(code) > SIZE - CODE:
        raise ValueError(f"the code is {len(code)} bytes; there's room for {SIZE - CODE}")
    head = bytes(4) + b"".join(k.to_bytes(2, "little") + a.to_bytes(2, "little") for k, a in sorted(entries))
    return head + bytes.fromhex("ffff0000") * (CODE // 4 - 1 - len(entries)) + code.ljust(SIZE - CODE, b"\0")


def section_at(image: bytes, k: int = SECTION) -> int:
    return 4 + int.from_bytes(image[4 + 4 * k:8 + 4 * k], "little")  # offsets count from byte 4


def s16(x: int) -> int:
    return ((x & 0xFFFF) ^ 0x8000) - 0x8000


def textures(image: bytes) -> tuple[list[int], list[tuple[int, int, int, int, bytes]]]:
    """A container's section 2 as func_800A0D2C loads it: 512 texture words (a palette in the low half, a page in the
    high) for D_800BD148, then images for LoadImage (u32 length, x, y, w, h, the pixels), a 0 length after the last."""
    at = section_at(image, 2)
    table, images = list(struct.unpack_from("<512I", image, at)), []
    at += 0x800
    while length := struct.unpack_from("<I", image, at)[0]:
        x, y, w, h = struct.unpack_from("<4h", image, at + 4)
        images.append((x, y, w, h, image[at + 12:at + 12 + 2 * w * h]))
        at += length & ~3
    return table, images


def mesh(data: bytes) -> dict:
    """An unpacked mesh as func_800A5D00 reads it: u16 triangle and vertex counts, the triangles (3 corners, a byte, 3
    UVs, a u16 whose low 9 bits pick the texture), the vertices and their normals (s16 x, y, z, pad)."""
    count, vertices = struct.unpack_from("<2H", data)
    triangles = []
    for at in range(4, 4 + 12 * count, 12):
        a, b, c, byte, u0, v0, u1, v1, u2, v2, texture = struct.unpack_from("<10BH", data, at)
        triangles.append((a, b, c, byte, (u0, v0), (u1, v1), (u2, v2), texture))
    at = 4 + 12 * count
    return {"triangles": triangles,
            "vertices": [struct.unpack_from("<3h", data, at + 8 * i) for i in range(vertices)],
            "normals": [struct.unpack_from("<3h", data, at + 8 * (vertices + i)) for i in range(vertices)]}


def file_block(kind: int, block: int, stage: int) -> int:
    """The block of WM{kind}.MAP that shows world block block (9 by 7, row by row) at map_stage stage
    (func_800A54F0). Map 0 is the world, six of its blocks swapped for the six after them as the story goes on; maps 2
    and 3 tile it with 3 by 4 and 2 by 2 blocks of their own (map 3's reach only some of it: past that, a number out
    of the file)."""
    row, col = divmod(block, 9)
    if kind == 0:
        need, swap = SWAPS.get(block, (99, block))
        return swap if stage >= need else block
    if kind == 2:
        row = row + 2 if row < 2 else row - 6 if row >= 6 else row - 2
        return row * 3 + (col if col < 3 else col - 6 if col >= 6 else col - 3)
    if kind == 3:
        row = row if row < 3 else row - 7
        row = row + 2 if row < 0 else row - 2 if row >= 2 else row
        col = col if col < 4 else col - 9
        return row * 2 + (col + 2 if col < 0 else col - 2 if col >= 2 else col)
    return block


def map_meshes(data: bytes, kind: int, stage: int = 0) -> dict[tuple[int, int], dict]:
    """Each world mesh (x 0-35, z 0-27) from WM{kind}.MAP's data at map_stage stage. A block is 16 offsets (its
    meshes 4 by 4, row by row), each to a u32 length and the mesh, LZS."""
    out, blocks = {}, {}
    for number in range(63):
        k = file_block(kind, number, stage)
        if not 0 <= k < len(data) // BLOCK:
            continue
        if k not in blocks:
            raw = data[k * BLOCK:(k + 1) * BLOCK]
            starts = struct.unpack_from("<16I", raw)
            blocks[k] = [mesh(lzs(raw[at:at + 4 + struct.unpack_from("<I", raw, at)[0]])) for at in starts]
        for i, m in enumerate(blocks[k]):
            out[(number % 9 * 4 + i % 4, number // 9 * 4 + i // 4)] = m
    return out


def zone_colour(zones: list[tuple], position: tuple, base: tuple) -> tuple[int, int, int]:
    """Light 0's colour (the colour matrix's first column) at position as func_800AF3A4 sets it each frame: a zone
    (x, y, z, its red, green and blue, and its outer and inner reach in 1024s; the scripts' point_ commands set 16 at
    D_8010B178) within its inner reach (x + y + z distances, round the world) gives 7× its colour, fading to the base
    colour (D_800C6848's) at its outer reach past that. The last zone in reach wins."""
    out = base
    for x, y, z, red, green, blue, outer, inner in zones:
        dx, dz = abs(position[0] - x), abs(position[2] - z)
        dx, dz = 0x48000 - dx if dx > 0x23FFF else dx, 0x38000 - dz if dz > 0x1BFFF else dz
        near, reach = max(dx + abs(position[1] - y) + dz - (inner << 10), 0), outer << 10
        if near < reach:
            out = tuple(b * near // reach + c * (reach - near) * 7 // reach for b, c in zip(base, (red, green, blue)))
    return out


def put_vector(gte, reg: int, x: int, y: int, z: int):
    gte.gte_write(reg, (y & 0xFFFF) << 16 | x & 0xFFFF)
    gte.gte_write(reg + 1, z & 0xFFFF)


def terrain_packets(gte, meshes: dict, table: list[int], rotation: list[int], translation: tuple, focus: tuple,
                    curve: int, fog: int = 0, code: int = 0x34) -> list[tuple[int, list[int]]]:
    """The terrain round the focus (x, y, z) as func_800A71E8 draws it, through gte (the port's, pc/gte.c, with H,
    the offset and the lights set): its ordering table bucket and GT3 packet (less the tag) per triangle drawn. The
    rotation is the camera's (D_800E5698), the translation where it puts the focus (D_800E56B8's); curve
    (D_800E5678) bends the far terrain down and fog (D_800BD138, map 2 only) darkens it by palette rows."""
    for i in range(4):
        gte.gte_write_ctrl(i, rotation[2 * i] & 0xFFFF | (rotation[2 * i + 1] & 0xFFFF) << 16)
    gte.gte_write_ctrl(4, rotation[8] & 0xFFFF)
    out = []
    for dz in range(-2, 3):  # the 5 by 5 meshes round the focus's (func_800A63FC)
        for dx in range(-2, 3):
            m = meshes.get((((focus[0] >> 13) + dx) % 36, ((focus[2] >> 13) + dz) % 28))
            if m is None:
                continue
            # func_800A1FAC: the mesh's corner from the focus, at the focus's height, turned and moved as the focus is
            for i, t in enumerate(translation):
                gte.gte_write_ctrl(5 + i, t)
            put_vector(gte, 0, dx * MESH - (focus[0] & 0x1FFF), -focus[1], dz * MESH - (focus[2] & 0x1FFF))
            gte.gte_cmd(0x0480012)
            moved = [gte.gte_read(25 + i) for i in range(3)]
            for i, t in enumerate(moved):
                gte.gte_write_ctrl(5 + i, t)
            # func_800BCA78: the world curves away, each vertex dropping by the square of how far past the curve it is
            vertices = []
            for x, y, z in m["vertices"]:
                put_vector(gte, 0, x, y, z)
                gte.gte_cmd(0x0180001)
                past = (gte.gte_read(19) >> 2) - curve
                if past > 0:
                    drop = (past >> 6) + (abs(s16(gte.gte_read(14)) - 160) >> 5)
                    y = s16(y - drop * drop)
                vertices.append((x, y, z))
            # func_800BCB2C: projected again, three at a time
            screen = []
            for i in range(0, len(vertices), 3):
                group = vertices[i:i + 3] + [(0, 0, 0)] * (i + 3 - len(vertices))
                for k, v in enumerate(group):
                    put_vector(gte, 2 * k, *v)
                gte.gte_cmd(0x0280030)
                screen += [(gte.gte_read(12 + k), gte.gte_read(17 + k) & 0xFFFF) for k in range(3)]
            # func_800BCBE8: a triangle with some corner on screen across and some down, that faces the camera
            for a, b, c, _, uv0, uv1, uv2, texture in m["triangles"]:
                points = [screen[k][0] for k in (a, b, c)]
                if not any(p & 0xFFFF < 320 for p in points) or not any(p >> 16 < 240 for p in points):
                    continue
                for k, p in enumerate(points):
                    gte.gte_write(12 + k, p)
                gte.gte_cmd(0x1400006)
                if gte.gte_read(24) & 0x80000000:
                    continue
                far = max(screen[k][1] for k in (a, b, c))
                gte.gte_write(6, code << 24)
                for k, n in enumerate((a, b, c)):
                    put_vector(gte, 2 * k, *m["normals"][n])
                gte.gte_cmd(0x0D80420)
                rgb = [gte.gte_read(20 + k) for k in range(3)]
                entry = table[texture & 0x1FF]
                clut = entry + (min((far - fog) >> 3 & 0xFFC0, 0x3C0) if fog and far > fog else 0)
                out.append((far >> 4, [rgb[0], points[0], uv0[0] | uv0[1] << 8 | (clut & 0xFFFF) << 16,
                                       rgb[1], points[1], uv1[0] | uv1[1] << 8 | (entry >> 16) << 16,
                                       rgb[2], points[2], uv2[0] | uv2[1] << 8 | texture << 16]))
    return out


def dump(disc: Path, out: Path):
    out.mkdir(parents=True, exist_ok=True)
    for name in ("WM0", "WMB", "WMC"):
        image = lzs((disc / f"WORLD/{name}.TXZ").read_bytes())
        at = section_at(image)
        table = scripts_json(image[at:at + SIZE])
        (out / f"{name}.json").write_text(json.dumps(table, indent=1, ensure_ascii=False) + "\n", "utf-8")


def build(folder: Path, assets: Path):
    sections = {}
    for path in sorted(folder.glob("*.json")):
        try:
            sections[path.stem] = scripts_raw(json.loads(path.read_text("utf-8")))
        except ValueError as e:
            raise SystemExit(f"{path}: {e}")
    for disc in kernel.discs(assets):
        for original in sorted((disc / "WORLD").glob("*.orig")):  # start from the originals: a dump says it all
            kernel.replace(disc, f"WORLD/{original.stem}", original.read_bytes())
        image = bytearray(gzip.decompress((disc / "WORLD/WORLD.BIN").read_bytes()[8:]))
        for path in sorted((disc / "WORLD").glob("WM*.TXZ")):
            new = sections.get(SETS.get(path.stem[:3], "WM0"))
            packed = path.read_bytes()
            data = lzs(packed)
            at = section_at(data)
            if new is None or data[at:at + SIZE] == new:
                continue
            out = field.repack(packed, data[:at] + new + data[at + SIZE:], at)
            lba = kernel.replace(disc, f"WORLD/{path.name}", out)
            entry = next(t + 8 * i for t in SIZES for i in range(13) if int.from_bytes(image[t + 8 * i:t + 8 * i + 4],
                                                                                        "little") == lba)
            # The CD layer only reads it as a count of sectors (cd.c), and WORLD.BIN doesn't pack back as small as
            # it was, so it changes only if that count does.
            if (len(out) + 0x7FF) >> 11 != (int.from_bytes(image[entry + 4:entry + 8], "little") + 0x7FF) >> 11:
                image[entry + 4:entry + 8] = len(out).to_bytes(4, "little")
        field.repack_module(disc, "WORLD/WORLD.BIN", bytes(image))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("dump", help="the scripts from a disc folder into a folder of JSON files")
    d.add_argument("disc", type=Path)
    d.add_argument("out", type=Path)
    b = sub.add_parser("build", help="a folder of JSON files into every disc folder's world map containers")
    b.add_argument("dump", type=Path)
    b.add_argument("assets", type=Path)
    args = ap.parse_args()
    if args.command == "dump":
        dump(args.disc, args.out)
    else:
        build(args.dump, args.assets)


if __name__ == "__main__":
    main()
