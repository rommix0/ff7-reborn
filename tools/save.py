"""Read and change FF7 saves on a memory card (a raw 128 KB image: the emulators' .mcr, the PC build's memcard1.mcd),
for tests that start anywhere in the story (docs/knowledge_base.md, "Memory cards").

  python tools/save.py dump CARD OUT           each save on the card as OUT/Snn.json
  python tools/save.py build FOLDER CARD       Snn.json files back over the card's saves
  python tools/save.py set CARD S01 gil=999999 level.cloud=50 party=cloud,tifa,barret item.Potion=99 field=116

A save is one 8 KB block: the 0x200-byte title frame, then 0x10F4 bytes that are the game's RAM from 0x8009C6E4. The
JSON names what's been checked against the game (party, characters, items, gil, seconds, module, field, disc,
place) and keeps the rest as hex ("data", "frame"); the named fields win. Building works out the checksum and the
preview the file list shows. Items are by name (from the disc's KERNEL.BIN; --assets) or "#n" by number.
"""
import argparse
import json
import struct
from pathlib import Path

import kernel
from model import write_json
from unpack import bingz

BLOCK, DATA, FRAME = 0x2000, 0x10F4, 0x200
CHARACTERS = ["cloud", "barret", "tifa", "aeris", "red", "yuffie", "cait", "vincent", "cid"]  # the records' order
RECORD, RECORDS, PARTY, ITEMS = 0x84, 0x54, 0x4F8, 0x4FC
# A record's fields: offset, struct format. The name is 12 bytes of game text.
STATS = {"level": (1, "B"), "hp": (0x2C, "H"), "hp_base": (0x2E, "H"), "mp": (0x30, "H"), "mp_base": (0x32, "H"),
         "hp_max": (0x38, "H"), "mp_max": (0x3A, "H"), "exp": (0x3C, "I")}
NAME = (0x10, 12)
# The save's own fields, and its game text.
FIELDS = {"gil": (0xB7C, "I"), "seconds": (0xB80, "I"), "module": (0xB94, "H"), "field": (0xB96, "H"),
          "disc": (0xEA4, "B")}
PLACE = (0xF0C, 24)


def crc(data: bytes) -> int:
    """The save menu's checksum (func_801D1950)."""
    r = 0xFFFF
    for b in data:
        r ^= b << 8
        for _ in range(8):
            r = (r << 1 ^ 0x1021 if r & 0x8000 else r << 1) & 0xFFFF
    return ~r & 0xFFFF


def saves(card: bytes) -> dict[str, int]:
    """Each FF7 save's name (S01-S15) and where its block starts: the directory's first blocks (state 0x51)."""
    out = {}
    for i in range(1, 16):
        frame = card[0x80 * i:0x80 * (i + 1)]
        name = frame[0x0A:0x1E].split(b"\0")[0].decode("ascii", "replace")
        if frame[0] == 0x51 and name.startswith("BASCUS-94163FF7-"):
            out[name[16:]] = BLOCK * i
    return out


def item_names(assets: Path) -> list[str]:
    """The 320 things an item word numbers: items, weapons, armour, accessories, by the game's own names."""
    discs = kernel.discs(assets) if assets.is_dir() else []
    if not discs:
        return [""] * 320
    sections = bingz((discs[0] / "INIT/KERNEL.BIN").read_bytes())
    return [kernel.decode(s) for name in ("item_names", "weapon_names", "armor_names", "accessory_names")
            for s in kernel.read_text(sections[kernel.NAMES.index(name)][1])]


def text(b: bytes) -> str:
    return kernel.decode(b.split(b"\xff")[0])


def put_text(data: bytearray, at: int, size: int, value: str):
    """Game text and its end, over what was there: the bytes after the end stay (the game leaves leftovers)."""
    b = kernel.encode(value)
    if len(b) > size:
        raise ValueError(f"{value!r} is longer than {size} characters")
    b = (b + b"\xff")[:size]  # a name filling its field has no end
    data[at:at + len(b)] = b


def item_label(word: int, names: list[str]) -> list | None:
    if word == 0xFFFF:
        return None
    number = word & 0x1FF
    name = names[number] if number < len(names) else ""
    return [name if name and names.count(name) == 1 else f"#{number}", word >> 9]


def item_number(label: str, names: list[str]) -> int:
    if label.startswith("#"):
        return int(label[1:])
    if names.count(label) != 1:
        raise ValueError(f"no item called {label!r}")
    return names.index(label)


def save_json(block: bytes, names: list[str]) -> dict:
    data = block[FRAME:FRAME + DATA]
    table = {key: struct.unpack_from("<" + f, data, at)[0] for key, (at, f) in FIELDS.items()}
    table["place"] = text(data[PLACE[0]:PLACE[0] + PLACE[1]])
    table["party"] = [None if c == 0xFF else CHARACTERS[c] if c < 9 else c for c in data[PARTY:PARTY + 3]]
    table["characters"] = {}
    for i, who in enumerate(CHARACTERS):
        record = data[RECORDS + RECORD * i:RECORDS + RECORD * (i + 1)]
        stats = {key: struct.unpack_from("<" + f, record, at)[0] for key, (at, f) in STATS.items()}
        table["characters"][who] = {"level": stats.pop("level"), "name": text(record[NAME[0]:NAME[0] + NAME[1]]),
                                    **stats}
    table["items"] = [item_label(w, names) for w in struct.unpack_from("<320H", data, ITEMS)]
    table["frame"] = block[:FRAME].hex()
    table["data"] = data.hex()
    if any(block[FRAME + DATA:]):
        table["tail"] = block[FRAME + DATA:].hex()
    return table


def save_raw(table: dict, names: list[str]) -> bytes:
    data = bytearray.fromhex(table["data"])
    for key, (at, f) in FIELDS.items():
        struct.pack_into("<" + f, data, at, table[key])
    put_text(data, *PLACE, table["place"])
    data[PARTY:PARTY + 3] = bytes(0xFF if c is None else CHARACTERS.index(c) if isinstance(c, str) else c
                                  for c in table["party"])
    for i, who in enumerate(CHARACTERS):
        record = RECORDS + RECORD * i
        for key, (at, f) in STATS.items():
            struct.pack_into("<" + f, data, record + at, table["characters"][who][key])
        put_text(data, record + NAME[0], NAME[1], table["characters"][who]["name"])
    for i, item in enumerate(table["items"]):
        struct.pack_into("<H", data, ITEMS + 2 * i, 0xFFFF if item is None else
                         item_number(item[0], names) | item[1] << 9)
    # The file list's preview: the leader's level, name, HP and MP, the party, gil, seconds, the place.
    lead = data[PARTY]
    if lead < 9:
        record = data[RECORDS + RECORD * lead:RECORDS + RECORD * (lead + 1)]
        data[4] = record[1]
        put_text(data, 8, 16, text(record[NAME[0]:NAME[0] + NAME[1]]))
        data[0x18:0x20] = record[0x2C:0x2E] + record[0x38:0x3A] + record[0x30:0x32] + record[0x3A:0x3C]
    data[5:8] = data[PARTY:PARTY + 3]
    data[0x20:0x28] = data[0xB7C:0xB84]
    put_text(data, 0x28, 32, table["place"])
    struct.pack_into("<I", data, 0, crc(data[4:]))
    tail = bytes.fromhex(table.get("tail", "")) or bytes(BLOCK - FRAME - DATA)
    return bytes.fromhex(table["frame"]) + bytes(data) + tail


def set_fields(table: dict, assignments: list[str], names: list[str]):
    """key=value changes: gil, seconds, module, field, disc, place; party=a,b,c; level.who, name.who (and the other
    stats); item.NAME=count (0 empties its slot)."""
    for assignment in assignments:
        key, _, value = assignment.partition("=")
        stat, _, who = key.partition(".")
        try:
            if key in FIELDS:
                table[key] = int(value, 0)
            elif key == "place":
                table[key] = value
            elif key == "party":
                party = [None if not c else c if c in CHARACTERS else int(c) for c in value.split(",")]
                if len(party) > 3 or any(isinstance(c, int) and not 0 <= c < 256 for c in party):
                    raise ValueError
                table["party"] = (party + [None] * 3)[:3]
            elif who in CHARACTERS and stat == "name":
                if len(kernel.encode(value)) > NAME[1]:
                    raise ValueError
                table["characters"][who]["name"] = value
            elif who in CHARACTERS and stat in STATS:
                table["characters"][who][stat] = int(value, 0)
            elif stat == "item" and who:
                number, count = item_number(who, names), int(value, 0)
                slots = table["items"]
                at = next((i for i, it in enumerate(slots) if it and item_number(it[0], names) == number),
                          next((i for i, it in enumerate(slots) if it is None), None))
                if at is None or not 0 <= count < 128:
                    raise ValueError
                slots[at] = item_label(number | count << 9, names) if count else None
            else:
                raise ValueError
        except ValueError:
            raise ValueError(f"can't set {assignment!r}") from None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--assets", type=Path, default=Path("assets"), help="where the discs are, for item names")
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("dump")
    d.add_argument("card", type=Path)
    d.add_argument("out", type=Path)
    b = sub.add_parser("build")
    b.add_argument("folder", type=Path)
    b.add_argument("card", type=Path)
    s = sub.add_parser("set")
    s.add_argument("card", type=Path)
    s.add_argument("save")
    s.add_argument("changes", nargs="+")
    args = ap.parse_args()
    names = item_names(args.assets)
    card = bytearray(args.card.read_bytes())
    found = saves(card)
    if args.command == "dump":
        args.out.mkdir(parents=True, exist_ok=True)
        for name, at in found.items():
            write_json(args.out / f"{name}.json", save_json(card[at:at + BLOCK], names))
        return
    tables = ({p.stem: json.loads(p.read_text("utf-8")) for p in sorted(args.folder.glob("S*.json"))}
              if args.command == "build" else {})
    if args.command == "set":
        if args.save not in found:
            raise SystemExit(f"{args.card}: no save {args.save} (it has {', '.join(found) or 'none'})")
        tables[args.save] = save_json(card[found[args.save]:found[args.save] + BLOCK], names)
        try:
            set_fields(tables[args.save], args.changes, names)
        except ValueError as e:
            raise SystemExit(str(e))
    for name, table in tables.items():
        if name not in found:
            raise SystemExit(f"{args.card}: no save {name} to write over")
        card[found[name]:found[name] + BLOCK] = save_raw(table, names)
    args.card.write_bytes(card)


if __name__ == "__main__":
    main()
