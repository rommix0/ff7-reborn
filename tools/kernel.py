"""Dump INIT/KERNEL.BIN to files people can edit, and build it back: the game's tables as JSON, its text as one line
per string.

  python tools/kernel.py dump "assets/Final Fantasy VII (USA) (Disc 1)" kernel
  python tools/kernel.py build kernel assets

build writes the new KERNEL.BIN into every disc folder under assets (all three discs carry the same one) and puts its
size in YAMADA.BIN and files.txt, keeping the first original of each as .orig (copy those back to undo); its table of
where each battle scene sits is made from that folder's SCENE.BIN (tools/scene.py builds that). In the text, {XX ...}
is any
byte by its hex: control codes with their arguments ({EB 00 05} is an item's name, {F8 02} a colour) and the few
characters a text file would show ambiguously ({5B} is "{"). A table's "_name" comes from the names file and build
ignores it.
"""
import argparse
import gzip
import json
import re
from pathlib import Path

from unpack import bingz

NAMES = ["commands", "attacks", "battle_and_growth", "new_game", "items", "weapons", "armor", "accessories", "materia",
         "command_help", "attack_help", "item_help", "weapon_help", "armor_help", "accessory_help", "materia_help",
         "key_item_help", "command_names", "attack_names", "item_names", "weapon_names", "armor_names",
         "accessory_names", "materia_names", "key_item_names", "battle_text", "summon_attack_names"]
TEXT = 9  # the section type of every text section; the tables' types are their numbers
# The help texts go through func_800150E4 and the battle text through func_80014E74, which unpack F9 references; the
# names are copied as they are (func_80014D58).
PACKED = set(range(9, 17)) | {25}
# Each table loads whole to a fixed address, so its size can't change.
SIZES = [256, 3584, 3988, 2876, 3584, 5632, 1152, 512, 1920]
# Tables of records: section -> (record size, names section, [(offset, size, field)]). Bytes no field covers are
# dumped as unknown_XX. A field is named once the game shows it (docs/knowledge_base.md, KERNEL.BIN): stat_N is a
# stat (0 Strength, 1 Vitality, 2 Magic, 3 Spirit, 4 Dexterity, 5 Luck, FF none) that bonus_N adds to; equip_mask
# has a bit per character; restrictions is clear where allowed (1 sell, 2 use in battle, 4 use in the menu, 8 throw).
STATS = [(0x14 + i, 1, f"stat_{i + 1}") for i in range(4)] + [(0x18 + i, 1, f"bonus_{i + 1}") for i in range(4)]
LAYOUTS = {
    0: (8, 17, []),
    1: (28, 18, [(0x04, 2, "mp_cost")]),
    4: (28, 19, [(0x0A, 2, "restrictions"), (0x0F, 1, "power")]),
    5: (44, 20, [(0x04, 1, "attack"), (0x06, 1, "growth"), (0x08, 1, "hit"), (0x0E, 2, "equip_mask"), *STATS,
                 *[(0x1C + i, 1, f"slot_{i + 1}") for i in range(8)], (0x2A, 2, "restrictions")]),
    6: (36, 21, [(0x02, 1, "defense"), (0x03, 1, "magic_defense"), (0x04, 1, "defense_pct"),
                 (0x05, 1, "magic_defense_pct"), *[(0x09 + i, 1, f"slot_{i + 1}") for i in range(8)],
                 (0x11, 1, "growth"), (0x12, 2, "equip_mask"), *[(o + 4, n, f) for o, n, f in STATS],
                 (0x20, 2, "restrictions")]),
    7: (16, 22, [(0x00, 1, "stat_1"), (0x01, 1, "stat_2"), (0x02, 1, "bonus_1"), (0x03, 1, "bonus_2"),
                 (0x0C, 2, "equip_mask"), (0x0E, 2, "restrictions")]),
    8: (20, 23, [(2 * i, 2, f"ap_level_{i + 2}") for i in range(4)]),
}
# func_80014C80 hands each text section the next piece of the exe's 0x80063690-0x8006947C with no check; the u16
# offset table after it is what an overflow would hit.
TEXT_SPACE = 0x8006947C - 0x80063690
ARGS = {**dict.fromkeys(range(0xEA, 0xF2), 2), 0xF8: 1, **dict.fromkeys(range(0xFA, 0xFF), 1)}
# Past ASCII the US text (kernel, scenes, every field's dialogue) uses only … “ ” ‘ ’ ·. The other bytes show up only in
# the dummy enemies' and attacks' names, text in another font, so they stay hex rather than read as accented letters.
GLYPHS = {0xA9, 0xB2, 0xB3, 0xB4, 0xB5, 0xC1}


def decode(text: bytes, args: dict | None = None) -> str:
    """A string's bytes (no FF) as text: Mac OS Roman less 0x20 (past ASCII only GLYPHS), codes from 0xE0 as {XX ...}
    with their arguments (ARGS unless given; field text has its own)."""
    args = ARGS if args is None else args
    out, i = [], 0
    while i < len(text):
        c = text[i]
        n = 1 + args.get(c, 0)
        char = bytes([c + 0x20]).decode("mac_roman") if c < 0x5F or c in GLYPHS else ""
        out.append(char if char and char.isprintable() and char != "{" else
                   "{" + " ".join(f"{b:02X}" for b in text[i:i + n]) + "}")
        i += n if c >= 0xE0 else 1
    return "".join(out)


def encode(text: str) -> bytes:
    out = bytearray()
    for token, char in re.findall(r"\{([0-9A-Fa-f ]+)\}|(.)", text, re.S):
        if token:
            out += bytes.fromhex(token)
            continue
        code = char.encode("mac_roman")[0] - 0x20
        if code < 0:
            raise ValueError(f"{char!r} has no glyph")
        out.append(code)
    return bytes(out)


def expand(section: bytes, at: int) -> bytes:
    """The string at an offset, as func_800150E4 unpacks it: F9 b copies (b >> 6) * 2 + 4 bytes of the section from
    (b & 0x3F) + 1 bytes before the F9."""
    out = bytearray()
    while section[at] != 0xFF:
        if section[at] == 0xF9:
            src = at - 1 - (section[at + 1] & 0x3F)
            out += section[src:src + (section[at + 1] >> 6) * 2 + 4]
            at += 2
        else:
            n = 3 if 0xEA <= section[at] <= 0xF1 else 1
            out += section[at:at + n]
            at += n
    # A copy can bring the FF that ended an earlier string (the battle text's do), and the text stops there.
    i = 0
    while i < len(out) and out[i] != 0xFF:
        i += 3 if 0xEA <= out[i] <= 0xF1 else 1
    return bytes(out[:i])


def read_text(section: bytes) -> list[bytes]:
    # Section 9's first offset is 0x3F, a byte into its own table, so the count rounds up.
    count = (int.from_bytes(section[:2], "little") + 1) // 2
    return [expand(section, int.from_bytes(section[2 * k:2 * k + 2], "little")) for k in range(count)]


def pack_text(strings: list[bytes], compress: bool = True) -> bytes:
    """A text section, repeats as F9 references if compress. A reference starts and ends between codes (the unpacker
    copies a code's arguments without looking at them), copies only bytes that are text themselves, and never follows
    an insert in its string: the battle text's are filled in first (func_80014E74), which moves what comes after."""
    raw = bytearray(2 * len(strings))
    copyable = bytearray(len(raw))
    at = {}
    for k, text in enumerate(strings):
        if text not in at:
            at[text] = len(raw)
            bounds, i = {len(text)}, 0
            while i < len(text):
                bounds.add(i)
                i += 3 if 0xEA <= text[i] <= 0xF1 else 1
            i, inserted = 0, False
            while i < len(text):
                inserted = inserted or i in bounds and 0xEA <= text[i] <= 0xF1
                ref = None
                if compress and not inserted and i in bounds:
                    ref = next(((length, d) for length in (10, 8, 6, 4) if i + length in bounds
                                for d in range(1, min(64, len(raw)) + 1)
                                if all(copyable[len(raw) - d:len(raw) - d + length])
                                and raw[len(raw) - d:len(raw) - d + length] == text[i:i + length]), None)
                if ref:
                    length, d = ref
                    raw += bytes([0xF9, (length - 4) // 2 << 6 | d - 1])
                    copyable += b"\0\0"
                    i += length
                else:
                    raw.append(text[i])
                    copyable.append(1)
                    i += 1
            raw.append(0xFF)
            copyable.append(0)
        raw[2 * k:2 * k + 2] = at[text].to_bytes(2, "little")
    return bytes(raw)


def pack_kernel(sections: list[tuple[int, bytes]]) -> bytes:
    out = bytearray()
    for kind, raw in sections:
        gz = gzip.compress(raw, 9, mtime=0)
        out += len(gz).to_bytes(2, "little") + len(raw).to_bytes(2, "little") + kind.to_bytes(2, "little") + gz
    return bytes(out + b"\0\0")


def fields_json(data: bytes, fields: list, skip: range = range(0)) -> dict:
    """A record's named fields (little-endian; a fourth item True for signed), then each byte no field covers as
    unknown_XX."""
    record, covered = {}, set(skip)
    for offset, length, field, *signed in fields:
        record[field] = int.from_bytes(data[offset:offset + length], "little", signed=bool(signed))
        covered.update(range(offset, offset + length))
    record.update((f"unknown_{i:02X}", data[i]) for i in range(len(data)) if i not in covered)
    return record


def fields_raw(record: dict, size: int, fields: list) -> bytearray:
    data = bytearray(size)
    for offset, length, field, *signed in fields:
        data[offset:offset + length] = record[field].to_bytes(length, "little", signed=bool(signed))
    for key, value in record.items():
        if key.startswith("unknown_"):
            data[int(key[8:], 16)] = value
    return data


def dump(disc: Path, out: Path):
    sections = [raw for _, raw in bingz((disc / "INIT/KERNEL.BIN").read_bytes())]
    out.mkdir(parents=True, exist_ok=True)
    for n, raw in enumerate(sections):
        path = out / f"{n:02d}_{NAMES[n]}"
        if n >= TEXT:
            path.with_suffix(".txt").write_text("".join(decode(s) + "\n" for s in read_text(raw)), "utf-8")
            continue
        if n not in LAYOUTS:
            table = {"bytes": [raw[i:i + 16].hex(" ") for i in range(0, len(raw), 16)]}
        else:
            size, names, fields = LAYOUTS[n]
            labels = [decode(s) for s in read_text(sections[names])]
            table = [{"_name": labels[k], **fields_json(raw[k * size:(k + 1) * size], fields)}
                     for k in range(len(raw) // size)]
        path.with_suffix(".json").write_text(json.dumps(table, indent=1, ensure_ascii=False) + "\n", "utf-8")


def read_dump(folder: Path) -> list[tuple[int, bytes]]:
    sections = []
    for n, name in enumerate(NAMES):
        path = folder / f"{n:02d}_{name}"
        if n >= TEXT:
            lines = path.with_suffix(".txt").read_text("utf-8").split("\n")[:-1]
            strings = []
            for line_number, line in enumerate(lines, 1):
                try:
                    strings.append(encode(line))
                except ValueError as e:
                    raise SystemExit(f"{path.with_suffix('.txt')}:{line_number}: {e}")
            sections.append((TEXT, pack_text(strings, n in PACKED)))
            continue
        table = json.loads(path.with_suffix(".json").read_text("utf-8"))
        if n not in LAYOUTS:
            raw = bytes.fromhex("".join(table["bytes"]))
        else:
            size, _, fields = LAYOUTS[n]
            raw = b"".join(fields_raw(record, size, fields) for record in table)
        if len(raw) != SIZES[n]:
            raise SystemExit(f"{path.with_suffix('.json')}: {len(raw)} bytes, the game's table is {SIZES[n]}")
        sections.append((n, bytes(raw)))
    text = sum(len(raw) for kind, raw in sections if kind == TEXT)
    if text > TEXT_SPACE:
        raise SystemExit(f"the text is {text} bytes; the game has room for {TEXT_SPACE}")
    return sections


def scene_table(scene_bin: bytes) -> bytes:
    """Section 2's 64 bytes at 0xF1C: the first scene of each 8 KB block of SCENE.BIN, FF after the last."""
    starts, first = [], 0
    for block in range(0, len(scene_bin), 8192):
        starts.append(first)
        first += sum(scene_bin[block + 4 * i:block + 4 * i + 4] != b"\xff" * 4 for i in range(16))
    return bytes(starts).ljust(64, b"\xff")


def backup(disc: Path, name: str):
    original = disc / (name + ".orig")
    if not original.exists():
        original.write_bytes((disc / name).read_bytes())


def replace(disc: Path, name: str, data: bytes) -> int:
    """Writes a disc file, keeping its first original, and its size in files.txt (the PC port maps sectors to files
    by it); on the disc it can grow into the free sectors before the next file. Returns its sector."""
    files = (disc / "files.txt").read_text().splitlines()
    rows = [line.split(maxsplit=3) for line in files]
    lba = next(int(r[0]) for r in rows if r[3] == name)
    room = (min(int(r[0]) for r in rows if int(r[0]) > lba) - lba) * 2048
    if len(data) > room:
        raise SystemExit(f"{disc / name}: {len(data)} bytes; the disc has room for {room}")
    backup(disc, name)
    backup(disc, "files.txt")
    (disc / name).write_bytes(data)
    (disc / "files.txt").write_text("".join((f"{r[0]} {len(data)} {r[2]} {r[3]}" if r[3] == name else line) + "\n"
                                            for r, line in zip(rows, files)))
    print(f"{disc / name}: {len(data)} bytes")
    return lba


def install(disc: Path, sections: list[tuple[int, bytes]]):
    """KERNEL.BIN into a disc folder, its scene table made from the SCENE.BIN there (BATINI.X finds a scene's block
    by it), and its size in YAMADA.BIN too (the game reads it by that sector and size)."""
    battle = bytearray(sections[2][1])
    battle[0xF1C:0xF5C] = scene_table((disc / "BATTLE/SCENE.BIN").read_bytes())
    kernel = pack_kernel(sections[:2] + [(2, bytes(battle))] + sections[3:])
    lba = replace(disc, "INIT/KERNEL.BIN", kernel)
    backup(disc, "INIT/YAMADA.BIN")
    yamada = bytearray((disc / "INIT/YAMADA.BIN").read_bytes())
    entry = next(i for i in range(0, len(yamada), 8) if int.from_bytes(yamada[i:i + 4], "little") == lba)
    yamada[entry + 4:entry + 8] = len(kernel).to_bytes(4, "little")
    (disc / "INIT/YAMADA.BIN").write_bytes(yamada)


def discs(assets: Path) -> list[Path]:
    return sorted(p for p in assets.iterdir() if (p / "INIT/KERNEL.BIN").exists())


def build(folder: Path, assets: Path):
    sections = read_dump(folder)
    for disc in discs(assets):
        install(disc, sections)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("dump", help="KERNEL.BIN from a disc folder into a folder of files")
    d.add_argument("disc", type=Path)
    d.add_argument("out", type=Path)
    b = sub.add_parser("build", help="a dump folder into every disc folder's KERNEL.BIN")
    b.add_argument("dump", type=Path)
    b.add_argument("assets", type=Path)
    args = ap.parse_args()
    if args.command == "dump":
        dump(args.disc, args.out)
    else:
        build(args.dump, args.assets)


if __name__ == "__main__":
    main()
