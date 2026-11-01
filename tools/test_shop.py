from kernel import encode
from shop import GREETINGS, PRICES, SHOPS, TITLES, shops_json, shops_raw

# SHOPMENU.MNU keeps 9 shop titles (20 bytes), two sets of 10 clerk lines (46), 80 shops of 84 bytes (a title, a
# greeting set, a count, 10 entries of a u16 kind (0 inventory, 1 materia), 2 zero bytes, a u32 id), then a u32 price
# per inventory index from 0x6854 and per materia from 0x600 on. A dump names the stock and prices; build ignores
# the names.
mnu = bytearray(0x7058)
mnu[TITLES:TITLES + 20] = encode("Item Shop").ljust(19, b"\0") + b"\0"
mnu[TITLES + 9:TITLES + 10] = b"\xff"
mnu[GREETINGS + 46:GREETINGS + 46 + 9] = encode("Welcome!") + b"\xff"
mnu[GREETINGS + 9 * 46:GREETINGS + 10 * 46] = encode(" " * 46)
mnu[SHOPS:SHOPS + 20] = bytes.fromhex("0000 0200 0000 0000 0101 0000 0100 0000 3100 0000")
mnu[PRICES:PRICES + 8] = (50).to_bytes(4, "little") + (300).to_bytes(4, "little")
mnu[PRICES + 0x600 + 4 * 49:PRICES + 0x604 + 4 * 49] = (8000).to_bytes(4, "little")
names = {"items": [f"item{i}" for i in range(320)], "materia": [f"materia{i}" for i in range(96)]}
table = shops_json(bytes(mnu), names)
assert table["titles"][0] == "Item Shop" and table["greetings"][0][1] == "Welcome!"
assert table["greetings"][0][9] == " " * 46 and len(table["shops"]) == 80
assert table["shops"][0] == {"title": 0, "_title": "Item Shop", "greeting": 0,
                             "stock": [{"item": 257, "_name": "item257"}, {"materia": 49, "_name": "materia49"}]}
assert table["item_prices"][1] == {"_name": "item1", "price": 300} and len(table["item_prices"]) == 320
assert table["materia_prices"][49] == {"_name": "materia49", "price": 8000} and len(table["materia_prices"]) == 96
assert shops_raw(table, bytes(mnu)) == mnu

table["titles"][0] = "Bargains"
table["shops"][0]["stock"] = [{"materia": 49}, {"item": 5}, {"item": 300}]
table["item_prices"][5]["price"] = 7
edited = shops_raw(table, bytes(mnu))
assert edited[TITLES:TITLES + 9] == encode("Bargains") + b"\xff"
assert edited[SHOPS:SHOPS + 28] == bytes.fromhex("0000 0300 0100 0000 3100 0000 0000 0000 0500 0000"
                                                  "0000 0000 2C01 0000")
assert edited[PRICES + 20:PRICES + 24] == (7).to_bytes(4, "little")
for key, value in (("titles", ["x" * 21] + table["titles"][1:]),
                   ("shops", [{"title": 0, "greeting": 0, "stock": [{"item": 1}] * 11}] + table["shops"][1:]),
                   ("shops", [{"title": 0, "greeting": 0, "stock": [{"weapon": 1}]}] + table["shops"][1:])):
    try:
        shops_raw({**table, key: value}, bytes(mnu))
        raise AssertionError(key)
    except ValueError:
        pass
print("ok")
