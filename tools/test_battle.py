import struct

import model
from battle import animation, model_json


def stream(fields: list[tuple[int, int]]) -> bytes:
    """Fields of (bits, value) packed MSB first."""
    bits = "".join(format(v & (1 << n) - 1, f"0{n}b") for n, v in fields)
    bits += "0" * (-len(bits) % 8)
    return bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8))


# An animation: u16 frames, u16 the bitstream's length, u8 shift, the bitstream (MSB first). Frame 0 is absolute: the
# root's x, y, z in 16 bits each, then each bone's rotation x, y, z in 12 - shift bits, shifted up. Each later frame
# adds deltas: the root's a flag then 16 or 7 bits; a rotation's a flag (0: none), then 3 bits k: 0 is -(1 << shift),
# 7 is 12 - shift bits shifted, else k bits pushed away from 0 by 1 << (k - 1), shifted.
first = [(16, 10), (16, -555), (16, 224), (10, 100), (10, -1), (10, 3)]
second = [(1, 0), (7, -3), (1, 1), (16, 1000), (1, 0), (7, 0),
          (1, 1), (3, 0), (1, 0), (1, 1), (3, 2), (2, -2)]
third = [(1, 0), (7, 0)] * 3 + [(1, 1), (3, 7), (10, 5), (1, 0), (1, 1), (3, 1), (1, 0)]
body = stream(first + second + third)
anim = struct.pack("<HHB", 3, len(body), 2) + body
assert animation(anim, 1) == [((10, -555, 224), [(400, -4, 12)]),
                              ((7, 445, 224), [(396, -4, 12 + (-2 - 2) * 4)]),
                              ((7, 445, 224), [(416, -4, 0)])]

# An enemy file: the skeleton (a root, a bone of length -50 carrying a part, a bone on it of length -30; the part a
# textured triangle on page 0x25 with palette (0, 480), and a semi-transparent Gouraud quad), settings (no weapon),
# one animation, a TIM.
vertices = struct.pack("<16h", 0, 0, 0, 0, 10, 0, 0, 0, 0, 10, 0, 0, 10, 10, 0, 0)
part = (struct.pack("<I", 32) + vertices + struct.pack("<hH", 1, 0x25) + struct.pack("<3h2B2BH4B", 0, 8, 16, 0x80, 2,
        1, 2, 0x7800, 3, 4, 5, 6) + struct.pack("<I", 0) + struct.pack("<hh", 0, 0) + struct.pack("<I", 1)
        + struct.pack("<4h", 0, 8, 16, 24) + bytes.fromhex("1020303A 405060FF 708090FF A0B0C0FF"))
skeleton = (struct.pack("<HH", 2, 0) + struct.pack("<hhI", 0, 0, 0) + struct.pack("<hhI", 0, -50, 28)
            + struct.pack("<hhI", 1, -30, 0) + part)
still = stream([(16, 0), (16, -100), (16, 0)] + [(12, 0), (12, 1024), (12, 0)] + [(12, 0)] * 6)
tim = struct.pack("<2I", 0x10, 8) + struct.pack("<I4H", 12 + 32, 0, 480, 16, 1) + bytes(32) + struct.pack(
    "<I4H", 12 + 4, 320, 0, 1, 2) + bytes(4)
pieces = [skeleton, bytes(4), struct.pack("<HHB", 1, len(still), 0) + still, tim]
at = 4 + 4 * len(pieces)
offsets = []
for piece in pieces:
    offsets.append(at)
    at += len(piece)
table = model_json(struct.pack("<I4I", 4, *offsets) + b"".join(pieces), False)
assert table["bones"] == [{"length": 0, "parent": -1}, {"length": 0, "parent": 0}, {"length": -50, "parent": 1}]
assert table["parts"] == [{"bone": 1, "vertices": [[0, 0, 0], [10, 0, 0], [0, 10, 0], [10, 10, 0]],
                           "uvs": [[1, 2], [3, 4], [5, 6]], "textures": ["78000502"],
                           "polygons": [{"kind": "FT3", "vertices": [0, 1, 2], "uv": [0, 1, 2, 0], "flags": 0x30,
                                         "colors": ["80808000"]},
                                        {"kind": "G4", "vertices": [0, 1, 2, 3], "flags": 0x10,
                                         "colors": ["1020303A", "405060FF", "708090FF", "A0B0C0FF"]}]}]
assert table["tim"] == [(0, 480, 16, 1, bytes(32)), (320, 0, 1, 2, bytes(4))]
# Posed as the field poses: the root at its animated place, turned a quarter about y (1024 of 4096); a bone at its
# parent's start plus the parent's turn of (0, 0, the parent's length).
(turn, start), (_, child), (_, grandchild) = model.pose(table["bones"], table["animations"][0], 0)
assert [round(x) for x in start] == [0, -100, 0] and [round(x) for x in child] == [0, -100, 0]
assert [round(x) for x in grandchild] == [-50, -100, 0]
assert [round(x, 6) for x in model.apply(turn, (0, 0, 1))] == [1, 0, 0]
print("ok")
