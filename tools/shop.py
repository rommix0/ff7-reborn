"""Dump the shops (MENU/SHOPMENU.MNU) to a file people can edit, and build them back.

  python tools/shop.py dump "assets/Final Fantasy VII (USA) (Disc 1)" shops.json
  python tools/shop.py build shops.json assets

The file holds the shops' titles and the clerks' two sets of lines, the 80 shops (a title, a set of lines, and up to
10 things on sale: an "item" by its inventory number, 0-127 items, 128-255 weapons, 256-287 armor, 288-319
accessories, or a "materia"), and the price of every item and materia. "_name" and "_title" are shown from
KERNEL.BIN and the titles; build ignores them. build writes SHOPMENU.MNU into every disc folder under assets, keeping
the first original as .orig. Text is in the format tools/kernel.py describes, up to its slot's size.
"""
import argparse
import json
from pathlib import Path

import kernel
from unpack import bingz

# The module runs from 0x801D0000 as the file is (func_801D0FE0 picks a shop, func_801D11A4 lists it).
TITLES, TITLE_SIZE, TITLE_COUNT = 0x42C8, 20, 9
GREETINGS, GREETING_SIZE, GREETING_LINES = 0x437C, 46, 10
SHOPS, SHOP_SIZE, SHOP_COUNT, STOCK = 0x4714, 84, 80, 10
PRICES, MATERIA_PRICES = 0x6854, 0x6854 + 0x600  # a u32 per inventory number, then per materia (func_801D10B0)
KINDS = ["item", "materia"]


def u32(data: bytes, at: int) -> int:
    return int.from_bytes(data[at:at + 4], "little")


def text(data: bytes, at: int, size: int) -> str:
    return kernel.decode(data[at:at + size].split(b"\xff")[0])


def slot(line: str, size: int) -> bytes:
    """A string in a fixed slot: FF after it unless it fills the slot, then zeros."""
    raw = kernel.encode(line)
    if len(raw) > size:
        raise ValueError(f"{line!r} is {len(raw)} bytes; its slot has {size}")
    return (raw + b"\xff" if len(raw) < size else raw).ljust(size, b"\0")


def shops_json(mnu: bytes, names: dict) -> dict:
    titles = [text(mnu, TITLES + TITLE_SIZE * i, TITLE_SIZE) for i in range(TITLE_COUNT)]
    shops = []
    for n in range(SHOP_COUNT):
        at = SHOPS + SHOP_SIZE * n
        stock = []
        for i in range(int.from_bytes(mnu[at + 2:at + 4], "little")):
            kind, thing = KINDS[mnu[at + 4 + 8 * i]], u32(mnu, at + 8 + 8 * i)
            stock.append({kind: thing, "_name": names["items" if kind == "item" else "materia"][thing]})
        shops.append({"title": mnu[at], "_title": titles[mnu[at]], "greeting": mnu[at + 1], "stock": stock})
    return {
        "titles": titles,
        "greetings": [[text(mnu, GREETINGS + GREETING_SIZE * (GREETING_LINES * k + i), GREETING_SIZE)
                       for i in range(GREETING_LINES)] for k in range(2)],
        "shops": shops,
        "item_prices": [{"_name": name, "price": u32(mnu, PRICES + 4 * i)} for i, name in enumerate(names["items"])],
        "materia_prices": [{"_name": name, "price": u32(mnu, MATERIA_PRICES + 4 * i)}
                           for i, name in enumerate(names["materia"])],
    }


def shops_raw(table: dict, mnu: bytes) -> bytes:
    out = bytearray(mnu)
    for i, title in enumerate(table["titles"]):
        out[TITLES + TITLE_SIZE * i:TITLES + TITLE_SIZE * (i + 1)] = slot(title, TITLE_SIZE)
    for k, lines in enumerate(table["greetings"]):
        for i, line in enumerate(lines):
            at = GREETINGS + GREETING_SIZE * (GREETING_LINES * k + i)
            out[at:at + GREETING_SIZE] = slot(line, GREETING_SIZE)
    for n, shop in enumerate(table["shops"]):
        if len(shop["stock"]) > STOCK:
            raise ValueError(f"shop {n} sells {len(shop['stock'])} things; a shop has room for {STOCK}")
        record = bytearray([shop["title"], shop["greeting"]]) + len(shop["stock"]).to_bytes(2, "little")
        for thing in shop["stock"]:
            kind = next((k for k in KINDS if k in thing), None)
            if kind is None:
                raise ValueError(f"shop {n}: {thing} isn't an item or a materia")
            record += KINDS.index(kind).to_bytes(4, "little") + thing[kind].to_bytes(4, "little")
        out[SHOPS + SHOP_SIZE * n:SHOPS + SHOP_SIZE * (n + 1)] = record.ljust(SHOP_SIZE, b"\0")
    for at, prices in ((PRICES, table["item_prices"]), (MATERIA_PRICES, table["materia_prices"])):
        for i, entry in enumerate(prices):
            out[at + 4 * i:at + 4 * i + 4] = entry["price"].to_bytes(4, "little")
    return bytes(out)


def names(disc: Path) -> dict:
    sections = [raw for _, raw in bingz((disc / "INIT/KERNEL.BIN").read_bytes())]
    read = lambda name: [kernel.decode(s) for s in kernel.read_text(sections[kernel.NAMES.index(name)])]
    items = read("item_names") + read("weapon_names") + read("armor_names") + read("accessory_names")
    return {"items": items, "materia": read("materia_names")}


def dump(disc: Path, out: Path):
    table = shops_json((disc / "MENU/SHOPMENU.MNU").read_bytes(), names(disc))
    out.write_text(json.dumps(table, indent=1, ensure_ascii=False) + "\n", "utf-8")


def build(path: Path, assets: Path):
    table = json.loads(path.read_text("utf-8"))
    for disc in kernel.discs(assets):
        original = disc / "MENU/SHOPMENU.MNU.orig"
        mnu = (original if original.exists() else disc / "MENU/SHOPMENU.MNU").read_bytes()
        try:
            kernel.replace(disc, "MENU/SHOPMENU.MNU", shops_raw(table, mnu))
        except ValueError as e:
            raise SystemExit(f"{path}: {e}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("dump", help="SHOPMENU.MNU from a disc folder into a JSON file")
    d.add_argument("disc", type=Path)
    d.add_argument("out", type=Path)
    b = sub.add_parser("build", help="a JSON file into every disc folder's SHOPMENU.MNU")
    b.add_argument("dump", type=Path)
    b.add_argument("assets", type=Path)
    args = ap.parse_args()
    if args.command == "dump":
        dump(args.disc, args.out)
    else:
        build(args.dump, args.assets)


if __name__ == "__main__":
    main()
