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
from pathlib import Path

import field
import kernel
from unpack import lzs

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


def groups(packed: bytes) -> list[tuple[int, int]]:
    """Where each flag byte of an LZS stream is, and how much it has unpacked by then."""
    at, pos, out = 4, 0, []
    while at < len(packed):
        out.append((at, pos))
        flags = packed[at]
        at += 1
        for bit in range(8):
            if at >= len(packed):
                break
            if flags >> bit & 1:
                at, pos = at + 1, pos + 1
            else:
                at, pos = at + 2, pos + (packed[at + 1] & 15) + 3
    return out


def repack(packed: bytes, data: bytes, start: int) -> bytes:
    """data packed again from the last flag byte before start (what's before is unchanged), the rest kept as was:
    the original packer did better than ours, and the containers have little room to grow."""
    at, pos = max((g for g in groups(packed) if g[1] <= start), key=lambda g: g[1])
    body = packed[4:at] + field.lzs_pack(data, pos)[4:]
    return len(body).to_bytes(4, "little") + body


def section_at(image: bytes) -> int:
    return 4 + int.from_bytes(image[4 + 4 * SECTION:8 + 4 * SECTION], "little")  # offsets count from byte 4


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
            out = repack(packed, data[:at] + new + data[at + SIZE:], at)
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
