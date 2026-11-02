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
import math
import struct
from pathlib import Path
from typing import NamedTuple

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


class Zone(NamedTuple):
    """A light zone as the scripts' point_ commands set it (one of 16 at D_8010B178): where, its light colour and the
    sky's top and bottom colours (red, green, blue), and its outer and inner reach in 1024s."""
    x: int
    y: int
    z: int
    light: tuple
    top: tuple
    bottom: tuple
    outer: int
    inner: int


def zone_colours(zones: list[Zone], position: tuple, light: tuple, top: tuple,
                 bottom: tuple) -> tuple[tuple, tuple, tuple]:
    """Light 0's colour (the colour matrix's first column) and the sky gradient's top and bottom colours at position,
    as func_800AF3A4 sets them each frame from the base ones (D_800C6848's, D_800C6768, D_800C676C): within a zone's
    inner reach (x + y + z distances, round the world) light 0 is 7× the zone's light and the sky the zone's colours,
    fading back to the base ones at its outer reach past that. The last zone in reach wins."""
    out = light, top, bottom
    for zone in zones:
        dx, dz = abs(position[0] - zone.x), abs(position[2] - zone.z)
        dx, dz = 0x48000 - dx if dx > 0x23FFF else dx, 0x38000 - dz if dz > 0x1BFFF else dz
        near, reach = max(dx + abs(position[1] - zone.y) + dz - (zone.inner << 10), 0), zone.outer << 10
        if near < reach:
            far = reach - near
            out = (tuple(b * near // reach + c * far * 7 // reach for b, c in zip(light, zone.light)),
                   *(tuple((b * near + c * far) // reach for b, c in zip(base, mine))
                     for base, mine in ((top, zone.top), (bottom, zone.bottom))))
    return out


def light_zones(code: list[str]) -> list[Zone]:
    """The light zones a script set's code sets up, read straight through: system script 0 sets them with constants
    (point_select N, on; point_block and point_pos, x and z; point_rgb1, 2 and 3; point_range, outer and inner;
    func_800AF1A8 onward). Zones left off are points the scripts measure to, not lights."""
    zones, chosen, pushed = {}, None, []
    for line in code:
        if line.startswith("push #"):
            pushed.append(int(line[6:]))
            continue
        args, pushed = pushed, []
        if line == "point_select" and len(args) == 2:
            chosen = zones.setdefault(args[0], {"x": 0, "z": 0, "on": 0, "range": (0, 0),
                                                **{k: (0, 0, 0) for k in ("point_rgb1", "point_rgb2", "point_rgb3")}})
            chosen["on"] = chosen["on"] if args[1] == 0xFFFF else args[1]
        elif chosen is None or len(args) < 2:
            continue
        elif line == "point_block":
            chosen["x"], chosen["z"] = args[0] << 13 | chosen["x"] & 0x1FFF, args[1] << 13 | chosen["z"] & 0x1FFF
        elif line == "point_pos":
            chosen["x"] = chosen["x"] & ~0x1FFF | args[0] & 0x1FFF
            chosen["z"] = chosen["z"] & ~0x1FFF | args[1] & 0x1FFF
        elif line in ("point_rgb1", "point_rgb2", "point_rgb3") and len(args) == 3:
            chosen[line] = tuple(args)
        elif line == "point_range":
            chosen["range"] = tuple(args)
    return [Zone(z["x"], 0, z["z"], z["point_rgb1"], z["point_rgb2"], z["point_rgb3"], *z["range"])
            for _, z in sorted(zones.items()) if z["on"]]


def rot_matrix(sines: list[int], x: int, y: int, z: int) -> list[list[int]]:
    """libgte's RotMatrix in its own integer steps; sines is its table (D_8004BC98: sin, cos per 4096th of a turn)."""
    def sin_cos(angle: int) -> tuple[int, int]:
        i = 2 * (abs(angle) & 0xFFF)
        return -sines[i] if angle < 0 else sines[i], sines[i + 1]

    (sx, cx), (sy, cy), (sz, cz) = sin_cos(x), sin_cos(y), sin_cos(z)
    a, b = cz * -sy >> 12, sz * -sy >> 12
    return [[cz * cy >> 12, -(sz * cy) >> 12, sy],
            [(sz * cx >> 12) - (a * sx >> 12), (cz * cx >> 12) + (b * sx >> 12), -(cy * sx) >> 12],
            [(sz * sx >> 12) + (a * cx >> 12), (cz * sx >> 12) - (b * cx >> 12), cy * cx >> 12]]


def camera(sines: list[int], arctan: list[int], tilt: int, distance: int, height: int, yaw: int,
           roll: int = 0) -> tuple[int, list[list[int]]]:
    """The camera's pitch and rotation (D_800E56D8, D_800E5698) as func_800A1DF0 builds them: the eye tilt (4096 a
    turn) up from straight behind, distance away (a view's D_800C6628 and D_800C6638), over the focus at height (y)
    gives the pitch through arctan (WORLD.BIN's D_800BE1E8, 512 entries) plus 0x800; then RotMatrix(pitch, yaw, 0)
    turned by the roll."""
    tilted = rot_matrix(sines, tilt, 0, 0)
    eye_y, eye_z = tilted[1][2] * distance >> 12, tilted[2][2] * distance >> 12
    over, under = (eye_y * 16 - height) << 8, eye_z * 16
    t = abs(over) // abs(under) * (-1 if (over < 0) != (under < 0) else 1)  # C's division
    pitch = arctan[min(max(t, 0), 0x1FF)] + 0x800
    a, b = rot_matrix(sines, 0, 0, roll), rot_matrix(sines, pitch, yaw, 0)
    return pitch, [[sum(a[i][k] * b[k][j] for k in range(3)) >> 12 for j in range(3)] for i in range(3)]


def sky_scroll(scroll: int, last: int, yaw: int, curve: int) -> tuple[int, int]:
    """The clouds' scroll (D_8010AE44) and the yaw it was last moved for (D_8010AE48) after a frame at yaw
    (func_800AEA48): the turn since, the short way round, times curve / 10000 off the scroll (wrapping at 256)."""
    yaw = s16(yaw)
    yaw = yaw - 0x1000 if yaw - last > 0x800 else yaw + 0x1000 if last - yaw > 0x800 else yaw
    step = int((yaw - last) * curve / 10000)
    scroll -= step
    scroll = scroll + 256 if scroll < 0 else scroll - 256 if scroll > 255 else scroll
    return scroll, (last + int(step * 10000 / curve)) & 0xFFF


def sky_packets(gte, sines: list[int], rotation: list[int], translation: tuple, height: int, yaw: int, roll: int,
                curve: int, top: tuple, bottom: tuple, scroll: int = 0) -> list[list[int]]:
    """The sky as the world draws it behind everything (linked at the far entry, 2500), in drawing order: a draw mode
    without dither, the gradient (func_800AE638) from 24 rows above the screen down to the horizon in the top and
    bottom colours, dither back on, and the clouds (func_800AEA48): a strip of the texture at palette 0x7B0F, page
    0x39 (0xA9 on GPU types 1 and 2), grey 0x40, semi-transparent, 64 rows high, (curve >> 8) + 44 rows over the
    horizon, scrolled. The horizon is where a point 0x4000 ahead of the focus (turned by -yaw, at the camera's height)
    lands, plus 26 rows. Map 2 has no sky."""
    for i in range(4):
        gte.gte_write_ctrl(i, rotation[2 * i] & 0xFFFF | (rotation[2 * i + 1] & 0xFFFF) << 16)
    gte.gte_write_ctrl(4, rotation[8] & 0xFFFF)
    ahead = [sum(r * v for r, v in zip(row, (0, -height, -0x4000))) >> 12
             for row in rot_matrix(sines, 0, -yaw, 0)]  # func_800AE0BC
    for i, t in enumerate(translation):
        gte.gte_write_ctrl(5 + i, t)
    put_vector(gte, 0, ahead[0], -height, ahead[2])  # func_800A1FAC keeps the camera's height
    gte.gte_cmd(0x0480012)
    for i in range(3):
        gte.gte_write_ctrl(5 + i, gte.gte_read(25 + i))
    put_vector(gte, 0, 0, 0, 0)
    gte.gte_cmd(0x0180001)
    horizon = (gte.gte_read(14) >> 16) + 26  # D_8010B080, from the row as an unsigned 16 bits
    turn = rot_matrix(sines, 0, 0, roll)

    def place(x: int, y: int, tx: int, ty: int) -> int:  # MVMVA of RotMatrix(0, 0, roll) and (tx, ty)
        px, py = [((t << 12) + row[0] * x + row[1] * s16(y)) >> 12 for t, row in zip((tx, ty), turn)]
        return (py & 0xFFFF) << 16 | px & 0xFFFF

    def colour(rgb: tuple) -> int:
        return rgb[0] | rgb[1] << 8 | rgb[2] << 16

    up = -(horizon & 0xFFFF) - 24
    gradient = [0x38000000 | colour(top), place(-180, up, 160, horizon), colour(top), place(180, up, 160, horizon),
                colour(bottom), place(-180, 0, 160, horizon), colour(bottom), place(180, 0, 160, horizon)]
    row = horizon - ((max(curve, 1) >> 8) + 0x2C)
    xs = [-160, scroll - 0x160, scroll - 0x60, 160]
    points = [(place(x, row - 0x40, 160, 40), place(x, row, 160, 40)) for x in xs]

    def quad(left: int, right: int, u0: int, u1: int) -> list[int]:
        (a, b), (c, d) = points[left], points[right]
        return [0x2E404040, a, 0x7B0FC000 | u0 & 0xFF, c, 0x390000 | 0xC000 | u1 & 0xFF, b, 0xFF00 | u0 & 0xFF, d,
                0xFF00 | u1 & 0xFF]

    if scroll < 0xC0:
        clouds = [quad(0, 2, -0x40 - scroll, 0xFF), quad(2, 3, 0, ~scroll)]
    else:
        clouds = [quad(0, 1, -0x41 - scroll, 0xFF), quad(1, 2, 0, 0xFF), quad(2, 3, 0, ~scroll)]
    return [[0xE1000000, 0], gradient, [0xE1000200, 0]] + clouds[::-1]  # DR_MODEs: a mode and no window


# A part's polygon kinds in their order: (record bytes, corners, colours, textured, packet code)
KINDS_3D = {"GT4": (0x18, 4, 4, True, 0x3C), "GT3": (0x14, 3, 3, True, 0x34), "FT4": (0xC, 4, 1, True, 0x2C),
            "FT3": (0xC, 3, 1, True, 0x24), "F3": (8, 3, 1, False, 0x20), "F4": (8, 4, 1, False, 0x28),
            "G3": (0x10, 3, 3, False, 0x30), "G4": (0x14, 4, 4, False, 0x38)}
# The light func_800B6724 bakes a model in (func_800C31F0): a back colour, three grey lights and their directions.
MODEL_BACK = 0x40
MODEL_LIGHTS = ((0x6E, (-659, 411, 4034)), (0xAA, (1536, -3349, -1839)), (0x3C, (-2920, -1474, -2484)))
# The posing's sines and cosines per 256th of a turn: WORLD.BIN's D_800C7538 is this, truncated.
ANGLES = [(int(4096 * math.sin(k * math.pi / 128)), int(4096 * math.cos(k * math.pi / 128))) for k in range(256)]


def world_models(blob: bytes, base: int, at: int) -> list[dict]:
    """The models of a model section at offset at of blob, which loads at base (WM0-2.BSZ's section 0 for the party
    leader, at 0x8014A600; a container's section 0 for the rest, at 0x8013A800: every pointer in them is already a
    RAM address). The section: a count, a pointer to the first 0x24-byte header, a size. Scaled as func_800C1D58
    scales them at load (v · the header's scale >> 12, 0x200 for all: a length, a translation or a vertex)."""
    def at_(pointer: int) -> int:
        return pointer - base

    count, first = struct.unpack_from("<2I", blob, at)
    out = []
    for header in range(at_(first), at_(first) + 0x24 * count, 0x24):
        _, mode, n_bones, n_parts, n_anims, ax, ay, az = struct.unpack_from("<Bb6B", blob, header)
        scale, parts_at, anims_at, model_base = struct.unpack_from("<hHHI", blob, header + 0x16)
        bones_at = at_(model_base)
        bones = [(struct.unpack_from("<h", blob, bones_at + 4 * i)[0] * scale >> 12,
                  struct.unpack_from("<b", blob, bones_at + 4 * i + 2)[0], blob[bones_at + 4 * i + 3])
                 for i in range(n_bones)]
        anims = []
        for k in range(n_anims):
            frames, _, n16, n_static, n8, off16, off_static, off8, data = struct.unpack_from(
                "<H4B3HI", blob, bones_at + anims_at + 16 * k)
            d = at_(data)
            anims.append({"frames": frames, "records": [blob[d + 4 + 8 * i:d + 12 + 8 * i] for i in range(n_bones)],
                          "tracks": [[v * scale >> 12 for v in struct.unpack_from(f"<{frames}h", blob,
                                                                                    d + off16 + 2 * frames * i)]
                                     for i in range(n16)],
                          "statics": [v * scale >> 12 for v in struct.unpack_from(f"<{n_static}h", blob,
                                                                                   d + off_static)],
                          "angles": [list(blob[d + off8 + frames * i:d + off8 + frames * (i + 1)]) for i in range(n8)]})
        parts = []
        for k in range(n_parts):
            q = bones_at + parts_at + 0x20 * k
            on, bone, n_vertices = blob[q], blob[q + 1], blob[q + 2]
            counts = blob[q + 4:q + 12]
            polys_at, uvs_at, textures_at, bytes_at = struct.unpack_from("<4H", blob, q + 0xE)
            d = at_(struct.unpack_from("<I", blob, q + 0x18)[0])
            vertices = [tuple(c * scale >> 12 for c in struct.unpack_from("<3h", blob, d + 4 + 8 * i))
                        for i in range(n_vertices)]
            polygons, r, texture_byte = [], d + polys_at, d + bytes_at
            for (kind, (size, n, n_colours, textured, code)), many in zip(KINDS_3D.items(), counts):
                for _ in range(many):
                    words = struct.unpack_from(f"<{1 + n_colours + textured}I", blob, r)
                    polygon = {"kind": kind, "corners": [words[0] >> 8 * i & 0xFF for i in range(n)],
                               "colours": list(words[1:1 + n_colours])}
                    if textured:
                        polygon["uvs"] = [u16(blob, d + uvs_at + 2 * (words[-1] >> 8 * i & 0xFF)) for i in range(n)]
                        polygon["texture"] = blob[texture_byte]
                        texture_byte += 1
                    polygons.append(polygon)
                    r += size
            # As many texture words as the polygons name (model_packets takes the byte's low 4 bits): a part's table
            # can end the section (WMC.TXZ's).
            n_textures = max((p["texture"] & 0xF for p in polygons if "texture" in p), default=-1) + 1
            parts.append({"on": on, "bone": bone, "vertices": vertices, "polygons": polygons,
                          "textures": list(struct.unpack_from(f"<{n_textures}I", blob, d + textures_at))})
        out.append({"mode": mode, "angles": (ax, ay, az), "bones": bones, "anims": anims, "parts": parts})
    return out


def model_images(blob: bytes, at: int) -> list[tuple[int, int, int, int, bytes]]:
    """A model section's images as func_800C0808 loads them (WM0-2.BSZ's section 1): a count at 4, then per image w,
    h, x, y and the pixels' offset from the section's start."""
    out = []
    for i in range(blob[at + 4]):
        w, h, x, y, offset = struct.unpack_from("<4HI", blob, at + 8 + 12 * i)
        out.append((x, y, w, h, blob[at + offset:at + offset + 2 * w * h]))
    return out


def pose(model: dict, rotation: list[list[int]], translation: tuple, anim: int, frame: int,
         root: tuple | None = None) -> list[tuple[list[list[int]], list[int]]]:
    """Each bone's rotation and translation for frame of anim as func_800C1490 poses it under rotation and
    translation (the camera's for the model's place: func_800A36AC): the root turned by root's angles (256ths of a
    turn; func_800AAB18 gives x 0xC0, the facing >> 4) and a quarter turn about x, or (None, as the load-time
    lighting does) rotation and translation themselves; each bone turned y, x, z by its angles and moved along its
    own z by its length plus its translation. All in the GTE's integer steps."""
    def saturate(v: int) -> int:
        return max(-0x8000, min(0x7FFF, v))

    def turn(m: list[list[int]], c: list[int]) -> list[int]:  # MVMVA: (m · c) >> 12
        return [saturate(sum(m[r][i] * c[i] for i in range(3)) >> 12) for r in range(3)]

    def local(ax: int, ay: int, az: int) -> tuple:  # the columns of Ry · Rx · Rz
        (sx, cx), (sy, cy), (sz, cz) = ANGLES[ax], ANGLES[ay], ANGLES[az]
        p, q = s16(sx * sy >> 12), s16(sx * cy >> 12)
        return ([s16(sz * p + cz * cy >> 12), s16(sz * cx >> 12), s16(sz * q + cz * -sy >> 12)],
                [s16(cz * p + sz * -cy >> 12), s16(cz * cx >> 12), s16(cz * q + sz * sy >> 12)],
                [s16(cx * sy >> 12), -sx, s16(cx * cy >> 12)], (sx, cx, sy, cy))

    def columns(m: list[list[int]], cols: list) -> list[list[int]]:
        cols = [turn(m, c) for c in cols]
        return [[cols[c][r] for c in range(3)] for r in range(3)]

    if root is None:
        root_r, root_t = rotation, list(translation)
    else:
        c0, c1, _, (sx, cx, sy, cy) = local(*root)
        root_r = columns(rotation, [c0, [s16(-cx * sy >> 12), sx, s16(-cx * cy >> 12)], c1])
        root_t = [saturate(t) for t in translation]
    a = model["anims"][anim]
    out = []
    for (length, parent, _), record in zip(model["bones"], a["records"]):
        pr, pt = (root_r, root_t) if parent < 0 else out[parent]
        angles = [a["angles"][record[1 + k]][frame] if record[0] & 1 << k else record[1 + k] for k in range(3)]
        c0, c1, c2, _ = local(*angles)
        v = [0, 0, length]
        for k in range(3):
            index = record[4 + k]
            if record[0] & 0x10 << k:
                v[k] += a["tracks"][index][frame]
            elif index != 0xFF:
                v[k] += a["statics"][index]
        v = [s16(c) for c in v]
        out.append((columns(pr, [c0, c1, c2]),
                    [saturate(pt[k] + (sum(pr[k][i] * v[i] for i in range(3)) >> 12)) for k in range(3)]))
    return out


def set_matrix(gte, rotation: list[list[int]], translation: list[int] | None = None, at: int = 0):
    flat = [v for row in rotation for v in row]
    for i in range(4):
        gte.gte_write_ctrl(at + i, flat[2 * i] & 0xFFFF | (flat[2 * i + 1] & 0xFFFF) << 16)
    gte.gte_write_ctrl(at + 4, flat[8] & 0xFFFF)
    for i, t in enumerate(translation or []):
        gte.gte_write_ctrl(at + 5 + i, t)


def light_model(gte, model: dict, normals: list[tuple]):
    """Bakes the model's colours as func_800B6724 does once at load: posed at frame 0 of animation 0 under no
    camera, each part's colours through NCCS with its normal (WORLD.BIN's table D_800C7938, by the colour word's
    top byte, which stays), lights MODEL_LIGHTS turned with the part's bone, the back colour MODEL_BACK. A model
    whose animation 0 is empty (WM3.BSZ's model 1) is posed by its first with frames: the game reads past its data."""
    anim = next(k for k, a in enumerate(model["anims"]) if a["frames"])
    bones = pose(model, [[4096, 0, 0], [0, 4096, 0], [0, 0, 4096]], (0, 0, 0), anim, 0)
    set_matrix(gte, [[c << 4 for c, _ in MODEL_LIGHTS]] * 3, None, 16)
    for i in range(3):
        gte.gte_write_ctrl(13 + i, MODEL_BACK << 4)
    for part in model["parts"]:
        r = bones[part["bone"]][0]
        set_matrix(gte, [[sum(d[k] * r[k][j] for k in range(3)) >> 12 for j in range(3)] for _, d in MODEL_LIGHTS],
                   None, 8)
        for polygon in part["polygons"]:
            for i, colour in enumerate(polygon["colours"]):
                put_vector(gte, 0, *normals[colour >> 24])
                gte.gte_write(6, colour)
                gte.gte_cmd(0x108041B)
                polygon["colours"][i] = gte.gte_read(22)


def model_packets(gte, model: dict, bones: list, index: int = 0) -> list[tuple[int, list[int]]]:
    """The packets of a model's parts as func_800C0B48 draws them (mode -1: each part under its bone's matrix), with
    the ordering table bucket each is linked at: the vertices projected (rtpt in threes, then rtps), a polygon
    facing the camera (nclip > 0) linked at its average depth (avsz3; a quad's four depths over 16). Textures as
    func_800BFCAC builds them: a texture byte per polygon (texture word 0-15, 0x10 semi-transparent, 0x60 blend), the
    word's palette and page (kinds 0 and 1 move both by the model's index: the party leader is 0). For the port's
    GPU type (3; 1 and 2 pack the page otherwise)."""
    gte.gte_write_ctrl(29, 0x155)  # ZSF3, as InitGeom sets it
    out = []
    for part in model["parts"]:
        if not part["on"]:
            continue
        set_matrix(gte, *bones[part["bone"]])
        screen, vertices = [], part["vertices"]
        whole = len(vertices) - len(vertices) % 3
        for i in range(0, whole, 3):
            for k in range(3):
                put_vector(gte, 2 * k, *vertices[i + k])
            gte.gte_cmd(0x0280030)
            screen += [(gte.gte_read(12 + k), gte.gte_read(17 + k)) for k in range(3)]
        for v in vertices[whole:]:
            put_vector(gte, 0, *v)
            gte.gte_cmd(0x0180001)
            screen.append((gte.gte_read(14), gte.gte_read(19)))
        for polygon in part["polygons"]:
            corners = polygon["corners"]
            for k in range(3):
                gte.gte_write(12 + k, screen[corners[k]][0])
            gte.gte_cmd(0x1400006)
            facing = gte.gte_read(24)
            if not facing or facing & 0x80000000:
                continue
            if len(corners) == 4:
                depth = sum(screen[c][1] for c in corners) >> 4
            else:
                for k in range(3):
                    gte.gte_write(17 + k, screen[corners[k]][1])
                gte.gte_cmd(0x158002D)
                depth = gte.gte_read(7)
            size, n, n_colours, textured, code = KINDS_3D[polygon["kind"]]
            colours = [c & 0xFFFFFF for c in polygon["colours"]]
            uvs = [0] * n
            if textured:
                byte = polygon["texture"]
                t = part["textures"][byte & 0xF]
                kind = t & 0x3F
                du, dv = {0: (index % 4 * 64, index // 4 * 32), 1: (index % 8 * 32, index // 8 * 32)}.get(kind, (0, 0))
                uvs = [(uv + du) & 0xFF | ((uv >> 8) + dv & 0xFF) << 8 for uv in polygon["uvs"]]
                uvs[0] |= (((t >> 22 & 0x1FF) + (index if kind != 2 else 0)) << 6 | t >> 16 & 0x3F) << 16
                uvs[1] |= ((t & 0xC0) << 1 | byte & 0x60 | (t >> 12 & 1) << 4 | t >> 8 & 0xF) << 16
                code |= 2 if byte & 0x10 else 0
            words = []
            for k, c in enumerate(corners):
                if k < n_colours:
                    words.append(colours[k] | (code << 24 if k == 0 else 0))
                words.append(screen[c][0])
                if textured:
                    words.append(uvs[k])
            out.append((depth, words))
    return out


def shadow_packet(gte, sines: list[int], rotation: list[int], translation: tuple, offset: tuple, height: int,
                  ground: int, yaw: int, size: tuple, grey: int, rect: tuple) -> tuple[int, list[int]]:
    """An entity's shadow (func_800B59F4 as func_800B5C7C calls it): a quad size[0] by size[1] (half) at the ground
    under it, turned by its yaw under the camera, offset (x, z) from the focus at the camera's height; a
    subtracting texture (palette 0x7CC4, page 0x59; 0x129 on GPU types 1 and 2) over rect (u, v, w, h) in grey,
    linked at its nearest corner's depth >> 4. For a standing entity the game's grey is 0x20 and the size 100 by 100
    (both shrink as it rises: func_800B5C7C)."""
    camera_r = [rotation[0:3], rotation[3:6], rotation[6:9]]
    turn = rot_matrix(sines, 0, yaw, 0)
    turned = [[max(-0x8000, min(0x7FFF, sum(camera_r[i][k] * turn[k][j] for k in range(3)) >> 12)) for j in range(3)]
              for i in range(3)]  # MulRotMatrix0
    set_matrix(gte, camera_r, list(translation))
    put_vector(gte, 0, offset[0], -height, offset[1])  # func_800A1FAC
    gte.gte_cmd(0x0480012)
    set_matrix(gte, turned, [gte.gte_read(25 + i) for i in range(3)])
    corners = [(-size[0], ground, -size[1]), (size[0], ground, -size[1]), (-size[0], ground, size[1])]
    for k, v in enumerate(corners):
        put_vector(gte, 2 * k, *v)
    gte.gte_cmd(0x0280030)
    points = [gte.gte_read(12 + k) for k in range(3)]
    depths = [gte.gte_read(17 + k) for k in range(3)]
    put_vector(gte, 0, size[0], ground, size[1])
    gte.gte_cmd(0x0180001)
    points.append(gte.gte_read(14))
    depths.append(gte.gte_read(19))
    u, v, w, h = rect
    return min(depths) >> 4, [0x2E000000 | grey * 0x10101, points[0], 0x7CC40000 | v << 8 | u, points[1],
                              0x590000 | v << 8 | u + w, points[2], (v + h) << 8 | u, points[3], (v + h) << 8 | u + w]


SLOTS = 0x800C7114  # WORLD.BIN: per container (WM0-WMA, WMB, WMC), 32 bytes: each model id's 1-based header, or -1


def model_slots(module: bytes, container: int) -> dict[int, int]:
    """Which model each id 3-31 is in the container's section 0 (func_800B6B28 fills D_801159E8 from SLOTS's row):
    id: the header's index. Ids 0-2 are the party leader (WM0-WM2.BSZ), 32-42 WM3.BSZ's models 0-10."""
    row = module[SLOTS - 0x800A0000 + 32 * container:SLOTS - 0x800A0000 + 32 * container + 32]
    return {i: row[i] - 1 for i in range(3, 32) if 0 < row[i] < 0x80}


TINTS = 0x800C70DC  # WORLD.BIN: 7 tints, s16 r, g, b and a pad (func_800B624C)


def tinted(module: bytes, model: dict, k: int) -> dict:
    """The model with tint k added to every colour, each channel held to 0-255 (func_800C2450's gpl on both of its
    packet buffers; the top byte stays): spawning model 4, the wild chocobo, takes tint 0 (func_800A9334); command
    0x34B gives model 19, the party's chocobo, tint n (4 yellow) and 0x34C model 13, the submarine, n + 5."""
    tint = struct.unpack_from("<3h", module, TINTS - 0x800A0000 + 8 * k)

    def add(c: int) -> int:
        return c & 0xFF000000 | sum(max(0, min(255, (c >> 8 * i & 0xFF) + d)) << 8 * i for i, d in enumerate(tint))

    return model | {"parts": [part | {"polygons": [poly | {"colours": [add(c) for c in poly["colours"]]}
                                                    for poly in part["polygons"]]} for part in model["parts"]]}


def quarter(v: int) -> int:
    """C's division by 4, toward 0, as the place's quarters are taken."""
    return -(-v >> 2) if v < 0 else v >> 2


def curve_drop(gte, rotation: list[int], translation: tuple, height: int, dx: int, dz: int, curve: int) -> int:
    """How far the world's curve lowers an entity at (dx, dz) from the focus (func_800AA8F8): its point at the
    camera's height through rtps, then ((SZ >> 2) - curve)^2 >> 12 plus SX^2 >> 10 if the first is past the curve,
    over 4. The offsets are taken as s16."""
    flat = rotation
    rows = [flat[0:3], flat[3:6], flat[6:9]]
    shift = [t + (rows[k][1] * -height >> 12) for k, t in enumerate(translation)]  # RotTrans of (0, -H, 0)
    set_matrix(gte, rows, shift)
    put_vector(gte, 0, dx, 0, dz)
    gte.gte_cmd(0x0180001)  # rtps
    sx = s16(gte.gte_read(14))
    v = (gte.gte_read(19) >> 2) - curve
    return ((v * v >> 12) + (sx * sx >> 10) if v > 0 else 0) >> 2


def entity_translation(gte, rotation: list[int], translation: tuple, height: int, curve: int, offset: tuple,
                       lift: int = 0, player: bool = False) -> list[int]:
    """The translation an entity's model is posed under (func_800AAB18, func_800A36AC): its offset (x, y, z) from
    the focus (x and z wrapped round the world), y less the camera's height, plus its lift (+0x44) and less the
    curve's drop (neither for the player), each in quarters; turned by the camera (MVMVA, >> 12) and added to its
    translation in quarters."""
    dx, y, dz = offset
    dx = dx - 0x48000 if dx > 0x23FFF else dx + 0x48000 if dx < -0x24000 else dx
    dz = dz - 0x38000 if dz > 0x1BFFF else dz + 0x38000 if dz < -0x1C000 else dz
    drop = 0 if player else curve_drop(gte, rotation, translation, height, dx, dz, curve)
    place = [quarter(dx), quarter(y - height + (0 if player else lift) - drop), quarter(dz)]
    return [quarter(t) + (sum(rotation[3 * k + i] * s16(place[i]) for i in range(3)) >> 12)
            for k, t in enumerate(translation)]


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
