import struct
from pathlib import Path

from kernel import encode
from save import BLOCK, DATA, crc, save_json, save_raw, saves, set_fields

# The save menu's checksum (func_801D1950): CRC-16 from 0xFFFF with 0x1021, each byte into the high end, inverted.
assert crc(b"123456789") == 0xD64E

# A save block: the 0x200-byte title frame, then the game's data from +0x200 (RAM 0x8009C6E4): its checksum word, the
# file list's preview at +4, the characters' records from +0x54 (0x84 bytes each), the party at +0x4F8, item words
# at +0x4FC (number in the low 9 bits, count in the top 7), gil, seconds, module, field, disc and place.
names = [f"Thing{i}" for i in range(320)]
names[5], names[6], names[7] = "", "Twin", "Twin"  # a name that's blank or shared is shown as its number
block = bytearray(BLOCK)
block[:2] = b"SC"
data = memoryview(block)[0x200:0x200 + DATA]
cloud = 0x54
data[cloud + 1] = 7
data[cloud + 0x10:cloud + 0x1C] = (encode("Cloud") + b"\xff\x01\x02").ljust(12, b"\xff")  # leftovers after the end
struct.pack_into("<4H", data, cloud + 0x2C, 90, 95, 10, 12)
struct.pack_into("<2HI", data, cloud + 0x38, 100, 11, 1234)
data[0x4F8:0x4FB] = bytes([0, 3, 0xFF])
struct.pack_into("<320H", data, 0x4FC, 0 | 3 << 9, 6 | 1 << 9, *[0xFFFF] * 318)
struct.pack_into("<2I", data, 0xB7C, 500, 3600)
struct.pack_into("<2H", data, 0xB94, 1, 116)
data[0xEA4] = 1
data[0xF0C:0xF24] = (encode("Platform") + b"\xff").ljust(24, b"\0")
del data

table = save_json(bytes(block), names)
assert table["party"] == ["cloud", "aeris", None] and table["gil"] == 500 and table["seconds"] == 3600
assert table["module"] == 1 and table["field"] == 116 and table["disc"] == 1 and table["place"] == "Platform"
assert table["characters"]["cloud"] == {"level": 7, "name": "Cloud", "hp": 90, "hp_base": 95, "mp": 10, "mp_base": 12,
                                        "hp_max": 100, "mp_max": 11, "exp": 1234}
assert table["items"][:3] == [["Thing0", 3], ["#6", 1], None]

# Building writes the checksum and the preview from the data: the leader's level, name, HP and MP, the party, gil,
# seconds and place. A built save reads back the same and builds back byte for byte.
raw = save_raw(table, names)
d = raw[0x200:0x200 + DATA]
assert struct.unpack_from("<I", d, 0)[0] == crc(d[4:])
assert d[4] == 7 and d[5:8] == bytes([0, 3, 0xFF]) and d[8:0x0E] == encode("Cloud") + b"\xff"
assert struct.unpack_from("<4H2I", d, 0x18) == (90, 100, 10, 11, 500, 3600)
assert d[0x28:0x31] == encode("Platform") + b"\xff"
assert d[cloud + 0x16:cloud + 0x18] == b"\x01\x02"  # leftovers stay
assert save_raw(save_json(raw, names), names) == raw

# set: quick changes by name; an item count goes on its slot, or the first empty one; 0 empties it. The preview
# follows a new leader.
data = save_json(raw, names)
set_fields(data, ["gil=999999", "level.cloud=50", "name.cloud=Zack", "item.Thing0=0", "item.Thing9=99", "item.#6=2",
                  "party=tifa,cloud", "field=200", "seconds=7200"], names)
d = save_raw(data, names)[0x200:0x200 + DATA]
assert struct.unpack_from("<I", d, 0xB7C)[0] == 999999 and struct.unpack_from("<I", d, 0x20)[0] == 999999
assert d[cloud + 1] == 50 and d[cloud + 0x10:cloud + 0x15] == encode("Zack") + b"\xff"
assert struct.unpack_from("<3H", d, 0x4FC) == (9 | 99 << 9, 6 | 2 << 9, 0xFFFF)  # Thing0's slot, emptied first
assert d[0x4F8:0x4FB] == bytes([2, 0, 0xFF]) and d[4] == 0  # Tifa leads: her level, 0 here
assert struct.unpack_from("<H", d, 0xB96)[0] == 200 and struct.unpack_from("<I", d, 0x24)[0] == 7200
for wrong in (["gold=5"], ["item.Nothing=1"], ["party=zack"], ["level.cloud=x"], ["name.cloud=" + "a" * 13]):
    try:
        set_fields(save_json(raw, names), wrong, names)
        raise AssertionError(wrong)
    except ValueError:
        pass

# A card's saves: its directory's first blocks (0x51) named BASCUS-94163FF7-Snn. The user's cards, if they're here,
# all build back byte for byte.
cards = sorted((Path(__file__).parents[2] / "saved_games").glob("*.mcr"))
for card in cards:
    d = card.read_bytes()
    for name, at in saves(d).items():
        assert name.startswith("S") and save_raw(save_json(d[at:at + BLOCK], names), names) == d[at:at + BLOCK], card
print("ok", len(cards), "cards")
