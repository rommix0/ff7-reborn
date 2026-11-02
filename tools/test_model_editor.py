import numpy as np

from model_editor import FieldMap, Model, Ps1View, overlay, texture

# A texture word: kind (0, 1: in the model's slot of one of two areas, the palette in the slot's row; 2: where it
# is), depth, texture page, palette x / 16 and y.
assert texture("78141C01", 3) == ((768, 256, 320, 483), 0, (96, 0))
assert texture("78141C00", 5) == ((768, 256, 320, 485), 0, (64, 32))
assert texture("00451182", 5) == ((64, 256, 80, 1), 2, (0, 0))

# Polygons become the GPU packets the field sends, less their screen positions: a Gouraud textured triangle's
# colours, its UVs (moved by the slot), the palette with the first and the page (and its blend) with the second; a
# flat quad's one colour.
part = {"bone": 0, "vertices": [[0, 0, 0], [10, 0, 0], [0, 10, 0], [10, 10, 0]], "uvs": [[0, 0], [7, 0], [0, 7]],
        "textures": ["78141C01"],
        "polygons": [{"kind": "GT3", "vertices": [0, 1, 2], "uv": [0, 1, 2, 0], "flags": 0x30,
                      "colors": ["80808000", "40404000", "20202000"]},
                     {"kind": "F4", "vertices": [0, 1, 2, 3], "colors": ["FF000000"]}]}
m = Model([{"length": 0, "parent": -1}], [part], [], None, [], 1, np.zeros((512, 1024), np.uint16))
gt3, f4 = m.polygons
assert gt3 == (0, [0, 1, 2], 0x36, [0x808080, 0x404040, 0x202020],
               [(481 << 6 | 20) << 16 | 32, (12 | 1 << 4 | 1 << 5) << 16 | 39, 7 << 8 | 32])
assert f4 == (0, [0, 1, 2, 3], 0x28, [0x0000FF], [])
assert m.count == 3 + 6

# A map's background as the field links it: layer 1 and its page switches at the far end of the ordering table,
# layer 2 at each tile's depth with its own page; a tile in a group only when its state bit is shown; blended tiles
# as code 67.
fm = FieldMap.__new__(FieldMap)
red, blue = 8 | 2 << 7, 9 | 2 << 7  # 15-bit pages at x 512 and 576
fm.background = {"layers": [[[[0, 0, 0, 0, 0]], {"page": red}, [[16, 0, 0, 0, 0]], {"page": blue}],
                            [[[0, 16, 0, 0, 0, blue, 7, 0, 1, 0], [16, 16, 0, 0, 0, blue, 7, 3, 0, 4]]], [], []]}
fm.groups = [(3, 4)]
fm.vram = np.zeros((512, 1024), np.uint16)
fm.vram[:, 512:576], fm.vram[:, 576:640] = 0x001F, 0x7C00
packets = fm.packets({0, 1, 2, 3}, set())
assert [(slot, points) for slot, _, points in packets] == [(0xFFF, [(1, 0, 0)]), (0xFFF, []), (0xFFF, [(1, 16, 0)]),
                                                            (0xFFF, []), (7, [(2, 0, 16)])]
assert packets[4][1] == [0xE1000200 | blue, 0x67000000, 0, 0, 16 << 16 | 16]
assert len(fm.packets({0, 1}, {(3, 4)})) == 6

# How a map starts: each entity's first script up to its first ret. char takes the next model; xyzi places it (the
# dump shows coordinates unsigned); an if on story goes as the story given says; visi 00 hides. Group bits come from
# odd-kind triggers and bgon.
fm.scripts = {"entities": [{"name": "dir", "scripts": [0]}, {"name": "guard", "scripts": [1]},
                           {"name": "view", "scripts": [2]}],
              "scripts": [["bgon #2 #1", "ret"],
                          ["char 05", "if2 story < #3 L0010", "xyzi #100 #65436 #5 #2", "dir #64", "L0010:",
                           "visi 00", "ret", "xyzi #9 #9 #9 #9"],
                          ["ret"]]}
fm.map = {"triggers": [{"group": 3, "bit": 0, "kind": 5}] + [None] * 11}
fm.groups = [(2, 2), (3, 1), (3, 2)]
entities, shown = fm.start(0)
assert entities == [{"name": "guard", "visible": False, "dir": 64, "anim": 0, "model": 0, "x": 100, "y": -100,
                     "z": 5, "triangle": 2}] and shown == {(2, 2), (3, 1)}
assert "x" not in fm.start(3)[0][0]

# Drawn from the far end, each slot's last linked first, a page switch covers the tiles before it in the list.
ps1 = Ps1View()
if ps1.gpu:
    rgb, origin = ps1.background(fm, {0}, set())
    assert origin == (0, 0) and tuple(rgb[8, 8]) == (255, 0, 0) and tuple(rgb[8, 24]) == (0, 0, 255)

# 3D to background pixels: the camera's rotation (4.12) and translation, then x and y times H over the depth.
fm.camera = {"rotation": [[4096, 0, 0], [0, 4096, 0], [0, 0, 4096]], "translation": [0, 0, 1000], "distance": 500}
assert fm.project(np.array([[100.0, 50.0, 0.0]])).tolist() == [[50.0, 25.0]]

# An exit's arrow at its line's middle in 3D, projected (func_800ABFE8), not halfway between the projected ends; a
# marker that's on (kind not 0) at its point.
fm.camera["distance"] = 1000
fm.map = {"exits": [{"line": [[0, 0, 0], [60, 0, 3000]]}] + [None] * 11, "arrows": [1] + [0] * 11,
          "triggers": [None] * 12, "markers": [[0, -20, 0, 1]] + [[0, 0, 0, 0]] * 11}
rgb = np.zeros((64, 64, 3), np.uint8)
overlay(rgb, (-32, -32), fm, {"arrows", "markers"})
assert tuple(rgb[32, 44]) == (0, 255, 255) and not rgb[32, 39:42].any()  # (30, 0, 1500) lands at x 12
assert tuple(rgb[12, 32]) == (255, 0, 255)
print("ok")
