import struct

from model import bcx_json, bcx_raw, bsx_json, bsx_raw, light, PACKET, pose

# A .BCX: u32 size, u32 header offset H; the parts' and animations' data from 8; at H a 0x24-byte header, then the
# bones (4 bytes), parts (32) and animations (16). This one: two bones, a part on the second with a Gouraud textured
# triangle and a Gouraud quad, and a two-frame animation turning the root by a channel.
vertices = b"".join(struct.pack("<4h", *v, 0) for v in ((0, 0, 0), (100, 0, 0), (0, 100, 0), (100, 100, -5)))
gt3 = bytes([0, 1, 2, 0]) + bytes.fromhex("80808001 7f7f7f02 10203003") + bytes([0, 1, 2, 7])
g4 = bytes([0, 1, 2, 3]) + bytes.fromhex("ff000004 00ff0005 0000ff06 40404007")
part_data = (bytes(4) + vertices + gt3 + g4 + struct.pack("<I", 0x3F0A0110) + struct.pack("<3H", 0x0000, 0x1F00, 0x1F1F)
             + bytes([0x10]))
part_data += bytes(-len(part_data) % 4)
anim_data = bytes(4) + bytes([1, 0, 0, 0, 255, 255, 255, 0, 0, 64, 128, 192, 255, 255, 255, 0]) + bytes([5, 6])
anim_data += bytes(-len(anim_data) % 4)
H = 8 + len(part_data) + len(anim_data)
header = (bytes([1, 0xFF, 2, 1, 1]) + bytes(0x11) + struct.pack("<3HI", 0x200, 8, 40, 0x80000000 + H + 0x24) + bytes(4))
bones = struct.pack("<hbB", 0, -1, 0) + struct.pack("<hbB", 100, 0, 1)
polys = 4 + 4 * 8
part = (bytes([1, 1, 4, 3, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1]) +
        struct.pack("<5H2I", polys, polys + 0x2C, polys + 0x28, polys + 0x32, PACKET["GT3"] + PACKET["G4"],
                    0x80000008, 0))
anim = struct.pack("<H4B3HI", 2, 2, 0, 0, 1, 20, 20, 20, 0x80000000 + 8 + len(part_data))
bcx = bytearray(struct.pack("<2I", 0, H) + part_data + anim_data + header + bones + part + anim)
bcx[0:4] = struct.pack("<I", len(bcx))

table = bcx_json(bytes(bcx))
assert table["scale"] == 512 and table["bones"] == [{"length": 0, "parent": -1}, {"length": 100, "parent": 0}]
assert table["parts"] == [{"bone": 1, "at": 8, "vertices": [[0, 0, 0], [100, 0, 0], [0, 100, 0], [100, 100, -5]],
                           "uvs": [[0, 0], [0, 31], [31, 31]], "textures": ["3F0A0110"],
                           "polygons": [{"kind": "GT3", "vertices": [0, 1, 2], "uv": [0, 1, 2, 7], "flags": 16,
                                         "colors": ["80808001", "7F7F7F02", "10203003"]},
                                        {"kind": "G4", "vertices": [0, 1, 2, 3],
                                         "colors": ["FF000004", "00FF0005", "0000FF06", "40404007"]}]}]
assert table["animations"] == [{"at": 8 + len(part_data), "frames": 2,
                                "bones": [{"flags": 1, "rotation": [0, 0, 0], "offset": [255, 255, 255]},
                                          {"flags": 0, "rotation": [64, 128, 192], "offset": [255, 255, 255]}],
                                "offsets": [], "fixed": [], "rotations": [[5, 6]]}]
assert bcx_raw(table) == bcx

# Edits: a part's data grows and moves what follows; parts that shared data keep sharing it unless one changes.
table["parts"][0]["vertices"].append([1, 2, 3])
grown = bcx_raw(table)
assert len(grown) == len(bcx) + 8 and bcx_json(grown)["parts"][0]["vertices"][-1] == [1, 2, 3]
assert bcx_json(grown)["animations"][0]["rotations"] == [[5, 6]]
for wrong in ({"bone": 5}, {"polygons": [{"kind": "G4", "vertices": [0, 1, 2, 9], "colors": ["00000000"] * 4}]},
              {"polygons": [{"kind": "H4", "vertices": [0, 1, 2], "colors": []}]}):
    try:
        bcx_raw({**table, "parts": [{**table["parts"][0], **wrong}]})
        raise AssertionError(wrong)
    except ValueError:
        pass
# Posing: a bone turns by its parent's turn and its own (Ry, then Rx, then Rz; a byte, 256 to a turn), and starts at
# its parent's start plus the parent's turn of (0, 0, its length). The root turned a quarter about y puts the child's
# start along x.
bones = [{"length": 0, "parent": -1}, {"length": 100, "parent": 0}]
anim = {"frames": 1, "bones": [{"flags": 1, "rotation": [0, 0, 0], "offset": [255] * 3},
                               {"flags": 0, "rotation": [0, 0, 0], "offset": [255] * 3}],
        "offsets": [], "fixed": [], "rotations": [[0]]}
assert [[round(x) for x in t] for _, t in pose(bones, anim, 0)] == [[0, 0, 0], [0, 0, 100]]
anim["bones"][0]["rotation"] = [0, 64, 0]  # rx is channel 0 (flags bit 0), 0 in its one frame; ry fixed at 64
assert [[round(x) for x in t] for _, t in pose(bones, anim, 0)][1] == [100, 0, 0]

# Lighting as the GTE does it once at load (nccs): per light max(0, L.N), times its colour, plus the ambient,
# times the stored colour; 255 in a light's colour is 4080/4096.
lights = {"ambient": "000000", "light_colors": ["FFFFFF", "000000", "000000"],
          "light_vectors": [[0, 0, 4096], [4096, 0, 0], [0, 4096, 0]]}
assert light("80808000", [0, 0, 4096], lights) == (127, 127, 127)
assert light("80808000", [0, 0, -4096], lights) == (0, 0, 0)
assert light("80808000", [0, 0, -4096], {**lights, "ambient": "808080"}) == (64, 64, 64)
print("ok")
