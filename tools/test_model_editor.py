import numpy as np

from model_editor import Model, texture

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
print("ok")
