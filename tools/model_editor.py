"""The field viewer (the model editor to be): open the party's, each map's and the battle's models, turn them about,
play their animations, shaded and textured as the game draws them; each map as it starts, its background and
characters drawn by the port's GPU, with its walkmesh, exits, triggers, arrows and markers over it; each battle,
its stage, party and enemies as the battle sets them up, played from its opening camera, and any actor's action
(its script, the target's hurt and the attack's camera); and the world map's terrain as WORLD.BIN draws it round
any point, with an overview of the whole map to pick the point on.

  pip install -r tools/requirements-editor.txt
  python tools/model_editor.py assets
  python tools/model_editor.py assets --model MD1STIN.BSX:0 --anim 3 --frame 12 --shot cloud.png
  python tools/model_editor.py assets --model ENEMY6/CLOUD.LZS:3 --shot battle_cloud.png
  python tools/model_editor.py assets --battle 0x12C --party CLOUD --camera -1 --frame 30 --shot battle.png
  python tools/model_editor.py assets --battle 0x12C --action 0,20,4 --frame 10 --shot attack.png
  python tools/model_editor.py assets --map MD1STIN --story 1 --shot md1stin.png
  python tools/model_editor.py assets --world 0 --at 185636,123325 --view 1 --shot midgar.png
  python tools/model_editor.py assets --world 0 --at 169628,147243 --yaw 320 --view 2 --container 1       --models 3:352:1207:64,19:-800:600:1024 --shot junon.png

A character on a map (MD1STIN.BSX:0) has that map's lights, textures and animations added to its own; on its own
(CLOUD.BCX) it shows its colours unlit and untextured. A battle model (ENEMY6/CLOUD.LZS, :N its weapon N; enemies
listed with their names) comes lit and textured from its own file. Drag to turn the view, the wheel to zoom. A map
starts as its start-up code sets it at a point of the story (other saved variables 0): where its characters stand,
which tile groups show. The player and party members the code doesn't place aren't drawn. The Maps tab turns
layers, overlays, characters and groups on and off. Walkmesh edges are green, walls white, exits red, triggers yellow,
arrows cyan, markers magenta. The World tab draws the world map as the game does with its camera settled at a yaw and
view mode (L2/R2): sky, terrain, light zones, the party leader standing at the point (idle), and the other models
(vehicles, chocobos, ...) placed round it, with their shadows; the minimap isn't drawn.
"""
import argparse
import ctypes
import gzip
import struct
import zlib
from pathlib import Path
from types import SimpleNamespace

import moderngl
import numpy as np

import battle
import field
import model
import scene
import world
from unpack import bingz, lzs

NORMALS = 0x3F520  # FIELD.BIN unpacked: 240 normals, s16 x, y, z and a pad (D_800DF520)
FACES, MOUTHS = 0x3FCA4, 0x3FD94  # FIELD.BIN unpacked: per face id, 7 and 3 FIELD.TDB image numbers (func_800B1C7C)
BCX = ["CLOUD", "EARITH", "BALLET", "TIFA", "RED", "CID", "YUFI", "KETCY", "VINCENT"]  # .BCX numbers 1-9
VIEW = 512  # the view's size in pixels
DITHER = [-4, 0, -3, 1, 2, -2, 3, -1, -3, 1, -4, 0, 3, -1, 2, -2]  # the GPU's 4x4 dither, rows
FOV = np.radians(25)  # half the view's angle, both views
PACKETS = {"GT4": 0x3C, "GT3": 0x34, "FT4": 0x2C, "FT3": 0x24, "F3": 0x20, "F4": 0x28, "G3": 0x30, "G4": 0x38}
LIBRARY = Path(__file__).resolve().parent.parent / "build/editor"  # make editor

VERTEX_SHADER = """#version 330
uniform mat4 mvp;
in vec3 position;
in vec3 color;
in vec2 uv;
in ivec4 page;
in int depth;
out vec3 shade;
out vec2 warped;
noperspective out vec2 affine;
flat out ivec4 tex;
flat out int bits;
void main() {
    gl_Position = mvp * vec4(position, 1.0);
    shade = color;
    warped = uv;
    affine = uv;
    tex = page;
    bits = depth;
}
"""
FRAGMENT_SHADER = """#version 330
uniform bool ps1;
uniform float dither[16];
uniform usampler2D vram;
in vec3 shade;
in vec2 warped;
noperspective in vec2 affine;
flat in ivec4 tex;  // the texture page's x, y and the palette's x, y in VRAM
flat in int bits;  // -1 untextured, 0 4-bit, 1 8-bit, 2 15-bit
out vec4 frag;
uint at(int x, int y) { return texelFetch(vram, ivec2(x & 1023, y & 511), 0).r; }
void main() {
    vec3 c = shade * 255.0;
    if (bits >= 0) {  // the GPU's lookup; the PS1 maps textures without perspective
        ivec2 t = ivec2(floor(ps1 ? affine : warped)) & 255;
        uint texel;
        if (bits == 0) texel = at(tex.z + int(at(tex.x + t.x / 4, tex.y + t.y) >> uint((t.x & 3) * 4) & 15u), tex.w);
        else if (bits == 1)
            texel = at(tex.z + int(at(tex.x + t.x / 2, tex.y + t.y) >> uint((t.x & 1) * 8) & 255u), tex.w);
        else texel = at(tex.x + t.x, tex.y + t.y);
        if (texel == 0u) discard;
        vec3 rgb = vec3(texel & 31u, texel >> 5 & 31u, texel >> 10 & 31u) * 8.0;
        c = min(rgb * c / 128.0, 255.0);
    }
    if (ps1) {  // 15-bit colour, dithered as the GPU does
        ivec2 p = ivec2(gl_FragCoord.xy) & 3;
        c = floor(clamp(c + dither[p.y * 4 + p.x], 0.0, 255.0) / 8.0) * 8.0;
    }
    frag = vec4(c / 255.0, 1.0);
}
"""


def vram(disc: Path, bsx: dict | None, face: tuple[int, int, int] = (1, 1, 1),
         base: np.ndarray | None = None) -> np.ndarray:
    """VRAM as the field sets it up for a map's models (func_800AC35C), over base (the map's .MIM) if given: the
    .BSX's images and patches, then per model (its slot its place in the list) its face's FIELD.TDB palette and three
    images, the given entries of its lists. The field loads entry 0, the same blank (0x7E) for every face, and
    scripts swap in the others."""
    out = np.zeros((512, 1024), np.uint16) if base is None else base.copy()
    if bsx is None:
        return out

    def put(x: int, y: int, w: int, h: int, data: bytes):
        out[y:y + h, x:x + w] = np.frombuffer(data, "<u2").reshape(h, w)

    tdb = bytearray(lzs((disc / "FIELD/FIELD.TDB").read_bytes()))
    images, palettes = model.u32(tdb, 8), model.u32(tdb, 12)
    for image in bsx["textures"]["images"]:
        put(image["x"], image["y"], image["w"], image["h"], bytes.fromhex(image["data"]))
    for patch in bsx["textures"].get("patches", []):
        data = bytes.fromhex(patch["data"])
        if patch["type"] == 1:
            tdb[images + 0x200 * patch["dest"]:images + 0x200 * (patch["dest"] + 1)] = data
        elif patch["type"] == 2:
            tdb[palettes + 32 * patch["dest"]:palettes + 32 * (patch["dest"] + 1)] = data
    module = field.module(disc)
    for slot, m in enumerate(bsx["models"]):
        face_id = m["face"]
        if face_id >= 0x21:
            continue
        put(320, 480 + slot, 16, 1, bytes(tdb[palettes + 32 * face_id:palettes + 32 * face_id + 32]))
        for (table, size, x, y), k in zip(((FACES, 7, 0x300 + slot % 4 * 16, 0x100 + slot // 4 * 32),
                                           (FACES, 7, 0x308 + slot % 4 * 16, 0x100 + slot // 4 * 32),
                                           (MOUTHS, 3, 0x300 + slot % 8 * 8, 0x1A0 + slot // 8 * 32)), face):
            n = module[table + size * face_id + k]
            put(x, y, 8, 32, bytes(tdb[images + 0x200 * n:images + 0x200 * (n + 1)]))
    return out


def texture(word: str, slot: int) -> tuple[tuple[int, int, int, int], int, tuple[int, int]]:
    """A texture word: the page and palette in VRAM, the depth, and how far the model's slot moves its UVs. Kinds 0
    and 1 put the texture in the slot's place in one of two areas, and its palette in the slot's row; 2 leaves it."""
    w = int(word, 16)
    kind, depth = w & 0x3F, w >> 6 & 3
    shift = {0: (slot % 4 * 64, slot // 4 * 32), 1: (slot % 8 * 32, slot // 8 * 32)}.get(kind, (0, 0))
    page = ((w >> 8 & 15) * 64, (w >> 12 & 1) * 256, (w >> 16 & 0x3F) * 16,
            (w >> 22 & 0x1FF) + (slot if kind < 2 else 0))
    return page, depth, shift


class Model:
    """A model ready to draw: its bones and animations, and per part its triangles' corners, lit colours, UVs and
    textures."""

    def __init__(self, bones: list, parts: list, anims: list, lights: dict | None, normals: list, slot: int,
                 vram: np.ndarray):
        self.bones, self.parts, self.anims, self.vram = bones, parts, anims, vram
        rest = model.pose(bones, next((a for a in anims if a), None), 0)  # the field lights its colours once, at load
        self.corners, colors, uvs, pages, depths = [], [], [], [], []
        self.polygons = []  # per polygon its part, vertices and the GPU packet less its screen positions
        for index, part in enumerate(parts):
            turn = rest[part["bone"]][0]
            corners = []
            for poly in part["polygons"]:
                n = len(poly["vertices"])
                lit = []
                for i in range(n):
                    color = poly["colors"][min(i, len(poly["colors"]) - 1)]
                    if lights is None:
                        lit.append(tuple(bytes.fromhex(color[:6])))
                    else:
                        normal = [round(x) for x in model.apply(turn, normals[int(color[6:], 16)])]
                        lit.append(model.light(color, normal, lights))
                semi = 2 if poly.get("flags", 0) & 0x10 else 0  # semi-transparent
                code, attributes = PACKETS[poly["kind"]] + semi, []
                if "uv" in poly:
                    page, depth, shift = texture(part["textures"][poly["flags"] & 15], slot)
                    texels = [[a + b for a, b in zip(part["uvs"][i], shift)] for i in poly["uv"][:n]]
                    uv = [[c + 0.5 for c in t] for t in texels]
                    blend = poly["flags"] >> 5 & 3
                    attributes = [(t[0] & 255 | (t[1] & 255) << 8) for t in texels]
                    attributes[0] |= (page[3] << 6 | page[2] >> 4) << 16  # the palette
                    attributes[1] |= (page[0] // 64 | page[1] // 256 << 4 | blend << 5 | depth << 7) << 16  # the page
                else:
                    page, depth, uv = (0, 0, 0, 0), -1, [[0, 0]] * n
                shades = [r | g << 8 | b << 16 for r, g, b in lit]
                self.polygons.append((index, poly["vertices"], code, shades if poly["kind"][0] == "G" else shades[:1],
                                      attributes))
                for tri in ([(0, 1, 2), (1, 3, 2)] if n == 4 else [(0, 1, 2)]):
                    corners += [poly["vertices"][k] for k in tri]
                    colors += [lit[k] for k in tri]
                    uvs += [uv[k] for k in tri]
                    pages += [page] * 3
                    depths += [depth] * 3
            self.corners.append(np.array(corners, np.int32))
        self.vertices = [np.array(part["vertices"], np.float32).reshape(-1, 3) for part in parts]
        self.count = len(colors)
        self.colors = np.array(colors, np.float32).reshape(-1, 3) / 255
        self.uvs = np.array(uvs, np.float32).reshape(-1, 2)
        self.pages = np.array(pages, np.int32).reshape(-1, 4)
        self.depths = np.array(depths, np.int32)

    def posed(self, anim: dict | None, frame: int) -> list[np.ndarray]:
        """Each part's vertices where the frame puts them."""
        bones = model.pose(self.bones, anim, frame)
        return [verts @ np.array(bones[part["bone"]][0], np.float32).T + np.array(bones[part["bone"]][1], np.float32)
                for part, verts in zip(self.parts, self.vertices)]

    def positions(self, posed: list[np.ndarray]) -> np.ndarray:
        """The triangles' corners, in drawing order."""
        return np.concatenate([np.zeros((0, 3), np.float32)] + [v[c] for v, c in zip(posed, self.corners)])


def load(disc: Path, name: str) -> Model:
    """NAME.BCX, MAP.BSX:N for a map's model N, or a battle model (ENEMY6/CLOUD.LZS, :N for a party member's
    weapon N), its TIM in VRAM where the file says."""
    normals = [struct.unpack_from("<3h", field.module(disc), NORMALS + 8 * i) for i in range(240)]
    path, _, index = name.partition(":")
    if path.upper().endswith(".LZS"):
        table = battle.model_json(lzs((disc / path).read_bytes()), not Path(path).stem.startswith("ENEMY"),
                                  int(index or 0))
        out = vram(disc, None)
        for x, y, w, h, data in table["tim"]:
            out[y:y + h, x:x + w] = np.frombuffer(data, "<u2").reshape(h, w)
        return Model(table["bones"], table["parts"], table["animations"], None, normals, 0, out)
    if path.upper().endswith(".BCX"):
        table = model.bcx_json(lzs((disc / "FIELD" / path).read_bytes()))
        return Model(table["bones"], table["parts"], table["animations"], None, normals, 0, vram(disc, None))
    bsx = model.bsx_json(lzs((disc / "FIELD" / path).read_bytes()))
    slot = int(index or 0)
    entry = bsx["models"][slot]
    bones, parts, anims = entry["bones"], entry["parts"], list(entry["animations"])
    if entry["bcx"]:  # a character: its own model, and this map's animations from first_animation on
        own = model.bcx_json(lzs((disc / "FIELD" / f"{BCX[entry['bcx'] - 1]}.BCX").read_bytes()))
        bones, parts = own["bones"], own["parts"]
        anims = own["animations"] + [None] * max(0, entry["first_animation"] + len(anims) - len(own["animations"]))
        for i, anim in enumerate(entry["animations"]):
            anims[entry["first_animation"] + i] = anim
    return Model(bones, parts, anims, entry, normals, slot, vram(disc, bsx))


def names(disc: Path) -> list[str]:
    out = [f"{n}.BCX" for n in BCX]
    for path in model.files(disc):
        if path.suffix.upper() == ".BSX":
            d = lzs(path.read_bytes())
            out += [f"{path.name}:{i}" for i in range(model.u32(d, model.u32(d, 4) + 4))]
    return out + [f"{p.parent.name}/{p.name}" for p in battle.files(disc)]


def enemy_names(disc: Path) -> dict[str, str]:
    """ENEMYnnn: the name SCENE.BIN gives enemy nnn (battle 0x12C, the MP, reads ENEMY016)."""
    out = {}
    for raw in scene.read_scenes((disc / "BATTLE/SCENE.BIN").read_bytes()):
        for k in range(3):
            enemy = int.from_bytes(raw[2 * k:2 * k + 2], "little")
            if enemy != 0xFFFF:
                record = raw[scene.ENEMIES + k * scene.ENEMY_SIZE:]
                out.setdefault(f"ENEMY{enemy:03d}", scene.name_text(record[:32]).strip())
    return out


class FieldMap:
    """A map as the field draws it: its .MIM in VRAM (func_800A2D5C: a palette block LoadImage'd at (0, 480), one or
    two 8-bit page blocks, each a u32 length and x, y, w, h), its background tiles and what it places in 3D."""

    def __init__(self, disc: Path, name: str):
        self.name = name
        secs = field.split(lzs((disc / f"FIELD/{name}.DAT").read_bytes()))
        self.background, self.camera = field.background_json(secs[2]), field.camera_json(secs[3])
        self.walkmesh, self.map = field.walkmesh_json(secs[1]), field.map_json(secs[4])
        self.vram, mim, at = np.zeros((512, 1024), np.uint16), lzs((disc / f"FIELD/{name}.MIM").read_bytes()), 0
        while at + 12 <= len(mim) and field.u32(mim, at):
            x, y, w, h = struct.unpack_from("<4H", mim, at + 4)
            data = mim[at + 12:at + 12 + 2 * w * h].ljust(2 * w * h, b"\0")  # TUNNEL_6's ends early
            self.vram[y:y + h, x:x + w] = np.frombuffer(data, "<u2").reshape(h, w)
            at += field.u32(mim, at)
        self.groups = sorted({(t[-3], t[-1]) for items in self.background["layers"][1:] for run in items
                              if isinstance(run, list) for t in run if t[-3]})  # (group, state bit) pairs
        self.scripts = field.section_json(secs[0], field.op_names(field.module(disc)))

    def start(self, story: int) -> tuple[list[dict], set[tuple[int, int]]]:
        """The entities and background groups as the map sets them up: each entity's first script, in entity order,
        up to its first ret. char gives the entity the next model in section 7's list (D_8009C6C4 counts), pc makes it
        a party member, xyzi/xyi/xyz place it, dir turns it, visi shows or hides it, dfanm picks its animation;
        bgon/bgoff/bgclr set group bits, over the triggers' load state (odd kinds on). Saved variables are 0 but
        story, and set/set2 of a literal count; an if the values can't be known for goes the way 0 says. Returns the
        entities with a model, and the (group, state bit) pairs that show."""
        compare = {"==": lambda a, b: a == b, "!=": lambda a, b: a != b, ">": lambda a, b: a > b,
                   "<": lambda a, b: a < b, ">=": lambda a, b: a >= b, "<=": lambda a, b: a <= b,
                   "&": lambda a, b: a & b, "^": lambda a, b: a ^ b, "|": lambda a, b: a | b,
                   "bit": lambda a, b: a >> b & 1, "!bit": lambda a, b: not a >> b & 1}
        values, groups, models, out = {"story": story}, {}, 0, []
        for t in self.map["triggers"]:
            if t and t["kind"] % 2:
                groups[t["group"]] = groups.get(t["group"], 0) | 1 << t["bit"]

        def value(word: str) -> int:
            return int(word[1:]) if word.startswith("#") else values.get(word, 0)

        for entity in self.scripts["entities"]:
            lines = self.scripts["scripts"][entity["scripts"][0]]
            labels = {line[:-1]: i for i, line in enumerate(lines) if line.endswith(":")}
            me, i = {"name": entity["name"], "visible": True, "dir": 0, "anim": 0}, 0
            for _ in range(1000):  # a loop back, or the end, also stops it
                if i >= len(lines):
                    break
                words = lines[i].split(";")[0].split()
                i += 1
                if not words or words[0].endswith(":"):
                    continue
                op, args = words[0], words[1:]
                literal = all(a.startswith("#") for a in args)
                if op == "ret" or op in ("back", "lback"):
                    break
                if op in ("skip", "lskip"):
                    i = labels[args[0]]
                elif op in ("if", "lif", "if2", "lif2", "if2_18", "lif2_19") and len(args) == 4 \
                        and args[1] in compare and not compare[args[1]](value(args[0]), value(args[2])):
                    i = labels[args[3]]
                elif op in ("set", "set2") and len(args) == 2 and args[1].startswith("#"):
                    values[args[0]] = value(args[1])
                elif op == "char":
                    me["model"], models = models, models + 1
                elif op == "pc":
                    me["pc"] = int(args[0], 16)
                elif op in ("xyzi", "xyi", "xyz") and literal:
                    v = [(value(a) ^ 0x8000) - 0x8000 for a in args]  # the dump shows them unsigned
                    me["x"], me["y"] = v[:2]
                    me["z"] = v[2] if op != "xyi" else None  # xyi: the height from the triangle
                    me["triangle"] = v[-1] if op != "xyz" else None
                elif op == "dir" and literal:
                    me["dir"] = value(args[0])
                elif op == "visi":
                    me["visible"] = args[0] != "00"
                elif op == "dfanm":
                    me["anim"] = int(args[0], 16)
                elif op in ("bgon", "bgoff") and literal:
                    g, bit = value(args[0]), value(args[1])
                    groups[g] = groups.get(g, 0) | 1 << bit if op == "bgon" else groups.get(g, 0) & ~(1 << bit)
                elif op == "bgclr" and literal:
                    groups[value(args[0])] = 0
            if "model" in me:
                out.append(me)
        return out, {(g, s) for g, s in self.groups if groups.get(g, 0) & s}

    def characters(self, disc: Path, entities: list[dict]) -> tuple[np.ndarray, list]:
        """VRAM with the models' textures over the .MIM, and the packets of each entity placed and shown, as
        func_800AAB24 and func_800AEE24 draw a model: its vertices scaled at load ((v * scale) >> 12, the .BSX entry's
        scale), posed (animation as set, frame 0), turned by Rz(facing) Rx(-64) (the model's -y up, its -z ahead;
        facing 0 looks along -y) and moved to (x, y, z - 10), then the camera's RotTransPers. A model whose own
        point lands at a depth past 0xF00 << 2 isn't drawn."""
        bsx = model.bsx_json(lzs((disc / f"FIELD/{self.name}.BSX").read_bytes()))
        rotation = np.array(self.camera["rotation"]) / 4096
        out = []
        for e in entities:
            if not e["visible"] or "x" not in e:
                continue
            m = load(disc, f"{self.name}.BSX:{e['model']}")
            anim = m.anims[e["anim"]] if e["anim"] < len(m.anims) and m.anims[e["anim"]] else \
                next((a for a in m.anims if a), None)
            z = e["z"] if e["z"] is not None else self.height(e["triangle"], e["x"], e["y"])
            place = np.array([e["x"], e["y"], z - 10], float)
            turn = np.array(model.matmul(model.rotation(0, 0, e["dir"]), model.rotation(-64, 0, 0)))
            scale = bsx["models"][e["model"]]["scale"] / 4096
            if (rotation @ place + self.camera["translation"])[2] >= 0xF00 << 2:
                continue
            screen, depth = [], []
            for verts in m.posed(anim, 0):
                v = (verts * scale @ turn.T + place) @ rotation.T + self.camera["translation"]
                z = np.maximum(v[:, 2], 1)
                screen.append(np.round(v[:, :2] * self.camera["distance"] / z[:, None]).astype(int))
                depth.append(v[:, 2])
            out += polygon_packets(m, screen, depth)
        return vram(disc, bsx, base=self.vram), out

    def height(self, triangle: int, x: float, y: float) -> float:
        """The walkmesh triangle's height at (x, y), from its plane (func_800A8E34)."""
        a, b, c = (np.array(v[:3], float) for v in self.walkmesh["triangles"][triangle]["vertices"])
        n = np.cross(b - a, c - a)
        return a[2] - (n[0] * (x - a[0]) + n[1] * (y - a[1])) / n[2] if n[2] else a[2]


    def packets(self, layers: set[int], shown: set[tuple[int, int]]) -> list[tuple[int, list, list]]:
        """GPU packets as func_800A364C links them (slot, words, and each position word's index, x and y): layer 1
        and its page switches at
        0xFFF, layer 2 each at its depth with its own page, layers 3 and 4 at 0xFFF and 1. A tile in a group shows
        when one of its state bits is on. The field's sprites are raw-textured (code 65, 67 blended); its draw mode
        (func_80044A68, SetDrawMode with dither on) carries to the models drawn after."""
        out = []
        for layer, items in enumerate(self.background["layers"]):
            if layer not in layers:
                continue
            slot = (0xFFF, 0, 0xFFF, 1)[layer]
            for item in items:
                if isinstance(item, dict):
                    out.append((slot, [0xE1000200 | item["page"]], []))
                    continue
                for t in item:
                    if layer and t[-3] and (t[-3], t[-1]) not in shown:
                        continue
                    size, blend = (16 if layer < 2 else 32), (t[-2] if layer else 0)
                    words = [(0x65 | blend << 1) << 24, 0, t[4] << 16 | t[3] << 8 | t[2], size << 16 | size]
                    if layer == 1:
                        out.append((t[6], [0xE1000200 | t[5]] + words, [(2, t[0], t[1])]))
                    else:
                        out.append((slot, words, [(1, t[0], t[1])]))
        return out

    def project(self, points: np.ndarray) -> np.ndarray:
        """3D points to background pixels as func_800A41CC does: RotTransPers with the camera, offset 0."""
        v = points @ (np.array(self.camera["rotation"]) / 4096).T + np.array(self.camera["translation"])
        return v[:, :2] * self.camera["distance"] / np.maximum(v[:, 2:], 1)


def draw_line(rgb: np.ndarray, a: np.ndarray, b: np.ndarray, color: tuple[int, int, int]):
    if max(abs(a).max(), abs(b).max()) > 8192:  # a point near the camera projects far off the picture
        return
    n = int(max(abs(b - a))) + 1
    xy = np.round(a + (b - a) * np.linspace(0, 1, n)[:, None]).astype(int)
    keep = (xy[:, 0] >= 0) & (xy[:, 0] < rgb.shape[1]) & (xy[:, 1] >= 0) & (xy[:, 1] < rgb.shape[0])
    rgb[xy[keep, 1], xy[keep, 0]] = color


def overlay(rgb: np.ndarray, origin: tuple[int, int], fm: FieldMap, show: set[str]):
    """The walkmesh (green, a wall edge white), exits (red), triggers (yellow), arrows (cyan) and markers (magenta) on
    the background. The arrows and markers go where func_800ABFE8 projects them: an exit's line's middle in 3D
    (halved toward 0; skipped at x = y = 0), a marker's point."""
    o = np.array(origin)
    if "walkmesh" in show:
        for t in fm.walkmesh["triangles"]:
            xy = fm.project(np.array([v[:3] for v in t["vertices"]], float)) - o
            for k in range(3):
                draw_line(rgb, xy[k], xy[(k + 1) % 3], (255, 255, 255) if t["neighbours"][k] < 0 else (0, 200, 0))
    for key, color in (("exits", (255, 40, 40)), ("triggers", (255, 220, 0))):
        if key in show:
            for e in fm.map[key]:
                if e:
                    xy = fm.project(np.array(e["line"], float)) - o
                    for d in (-1, 0, 1):
                        draw_line(rgb, xy[0] + d, xy[1] + d, color)
    if "arrows" in show:
        for e, on in zip(fm.map["exits"], fm.map["arrows"]):
            middle = np.trunc(np.array(e["line"], float).sum(0) / 2) if e else np.zeros(3)
            if on and middle[:2].any():
                x, y = (fm.project(middle[None])[0] - o).astype(int)
                draw_line(rgb, np.array([x - 4, y - 6]), np.array([x, y]), (0, 255, 255))
                draw_line(rgb, np.array([x + 4, y - 6]), np.array([x, y]), (0, 255, 255))
    if "markers" in show:
        for *point, kind in fm.map["markers"]:
            if kind:
                x, y = (fm.project(np.array([point], float))[0] - o).astype(int)
                for d in range(-3, 4):
                    draw_line(rgb, np.array([x - 3, y + d]), np.array([x + 3, y + d]), (255, 0, 255))


class Renderer:
    """Draws a model into an offscreen picture the window shows (or a --shot saves)."""

    def __init__(self, ctx: moderngl.Context):
        self.ctx = ctx
        self.program = ctx.program(vertex_shader=VERTEX_SHADER, fragment_shader=FRAGMENT_SHADER)
        self.program["dither"].value = DITHER
        self.texture = ctx.texture((VIEW, VIEW), 4)
        self.fbo = ctx.framebuffer([self.texture], ctx.depth_renderbuffer((VIEW, VIEW)))
        self.size = 0

    def upload(self, m: Model):
        """Room for the model's triangles, made once per model; frames only write their positions into it."""
        self.size = m.count
        self.positions = self.ctx.buffer(reserve=max(1, m.count) * 12)
        fixed = [(m.colors, "3f", "color"), (m.uvs, "2f", "uv"), (m.pages, "4i", "page"), (m.depths, "i", "depth")]
        buffers = [(self.ctx.buffer(np.ascontiguousarray(a).tobytes() or bytes(16)), f, name) for a, f, name in fixed]
        self.vao = self.ctx.vertex_array(self.program, [(self.positions, "3f", "position")] + buffers)
        self.vram = self.ctx.texture((1024, 512), 1, m.vram.tobytes(), dtype="u2")
        self.vram.filter = (moderngl.NEAREST, moderngl.NEAREST)

    def draw(self, positions: np.ndarray, yaw: float, pitch: float, distance: float, center: np.ndarray, ps1: bool):
        self.positions.write(positions.astype(np.float32).tobytes())
        self.fbo.use()
        self.ctx.clear(0.12, 0.12, 0.14, depth=1.0)
        self.ctx.enable(moderngl.DEPTH_TEST)
        self.program["mvp"].write(camera(yaw, pitch, distance, center).T.astype(np.float32).tobytes())
        self.program["ps1"].value = ps1
        self.vram.use(0)
        self.program["vram"].value = 0
        if self.size:
            self.vao.render(vertices=self.size)

    def pixels(self) -> np.ndarray:
        return np.frombuffer(self.fbo.read(components=3), np.uint8).reshape(VIEW, VIEW, 3)[::-1]


def view(yaw: float, pitch: float, distance: float, center: np.ndarray) -> np.ndarray:
    """Looking at center from distance, turned by yaw and pitch; the PS1's y points down, so it's flipped."""
    cy, sy, cp, sp = np.cos(yaw), np.sin(yaw), np.cos(pitch), np.sin(pitch)
    eye = center + distance * np.array([sy * cp, -sp, cy * cp])
    forward = (center - eye) / np.linalg.norm(center - eye)
    right = np.cross(forward, [0, -1, 0])
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    out = np.identity(4)
    out[0, :3], out[1, :3], out[2, :3] = right, up, -forward
    out[:3, 3] = -out[:3, :3] @ eye
    return out


def camera(yaw: float, pitch: float, distance: float, center: np.ndarray) -> np.ndarray:
    near, far, f = distance / 50, distance * 10, 1 / np.tan(FOV)
    proj = np.array([[f, 0, 0, 0], [0, f, 0, 0], [0, 0, (far + near) / (near - far), 2 * far * near / (near - far)],
                     [0, 0, -1, 0]])
    return proj @ view(yaw, pitch, distance, center)


class Ps1View:
    """The model drawn by the port's own GPU (pc/gpu.c, built by make editor) as the field sends it: each polygon
    projected to a 320x240 screen, back faces dropped (nclip), the rest from far to near (the ordering table), as
    its own packet. The GPU does the PS1's Gouraud shading, dithering, unwarped textures and semi-transparency."""

    def __init__(self):
        name = next((n for n in ("ps1gpu.dll", "libps1gpu.so") if (LIBRARY / n).exists()), None)
        self.gpu = ctypes.CDLL(str(LIBRARY / name)) if name else None
        if self.gpu:
            words = (ctypes.c_uint16 * (512 * 1024)).in_dll(self.gpu, "vram")
            self.vram = np.ctypeslib.as_array(words).reshape(512, 1024)
            self.gpu.gte_read.restype = ctypes.c_uint  # the port's GTE, for the world map's terrain
            self.gpu.gte_write.argtypes = self.gpu.gte_write_ctrl.argtypes = [ctypes.c_int, ctypes.c_uint]
            self.gpu.gte_cmd.argtypes = [ctypes.c_uint]

    def upload(self, m: Model):
        if self.gpu:
            self.vram[:] = m.vram

    def draw(self, m: Model, posed: list[np.ndarray], camera: np.ndarray) -> np.ndarray | None:
        if not self.gpu:
            return None
        h = 120 / np.tan(FOV)
        screen, depth = [], []
        for verts in posed:
            v = verts @ camera[:3, :3].T + camera[:3, 3]
            z = np.maximum(-v[:, 2], 1e-3)
            screen.append(np.stack([np.round(160 + h * v[:, 0] / z), np.round(120 - h * v[:, 1] / z)], 1).astype(int))
            depth.append(-v[:, 2])
        # The ordering table: buckets by depth, far first; in a bucket the packet added last is drawn first (it's
        # put at the head of the bucket's list), so a part's Gouraud polygons go under its textured ones.
        packets = [(slot, i, words) for i, (slot, words, _) in enumerate(polygon_packets(m, screen, depth))]
        packets.sort(key=lambda p: (-p[0], -p[1]))
        words = [0xE1000200, 0xE3000000, 0xE4000000 | 239 << 10 | 319, 0xE5000000, 0xE6000000,
                 0x02241F1F, 0, 240 << 16 | 320] + [w for _, _, ws in packets for w in ws]
        self.gpu.gpu_gp0_words((ctypes.c_uint32 * len(words))(*words), len(words))
        return self.screen()

    def screen(self, top: int = 0, h: int = 240, w: int = 320) -> np.ndarray:
        """The 320x240 corner of VRAM (or w x h from row top), each 5-bit colour widened as the port's window does."""
        c = self.vram[top:top + h, :w].astype(np.uint32)
        c = np.stack([c & 31, c >> 5 & 31, c >> 10 & 31], 2)
        return (c << 3 | c >> 2).astype(np.uint8)

    def battle(self, vram_: np.ndarray, packets: list[list[int]], rows: tuple = (8, 165)) -> np.ndarray | None:
        """A battle's packets (Battle.packets) drawn as BATTLE.X's draw environment has it: dithered, clipped to rows
        8-165 (the menu covers the rest), or to the rows given (the Battles tab draws all 240, having no menu)."""
        if not self.gpu:
            return None
        self.vram[:] = vram_
        top, bottom = rows
        words = [0xE1000200, 0xE3000000 | top << 10, 0xE4000000 | bottom << 10 | 319, 0xE5000000, 0xE6000000,
                 0x02000000, 0, 240 << 16 | 320] + [w for p in packets for w in p]
        self.gpu.gpu_gp0_words((ctypes.c_uint32 * len(words))(*words), len(words))
        return self.screen()

    def world(self, vram_: np.ndarray, packets: list[list[int]], w: int = 320, h: int = 224, top: int = 8,
              dither: bool = True) -> np.ndarray | None:
        """Packets in drawing order as WORLD.BIN's draw environment has them (func_800A0D2C): a 320x224 picture at row
        8 of VRAM, dithered, on black (or a w x h one from row top)."""
        if not self.gpu:
            return None
        self.vram[:] = vram_
        words = [0xE1000000 | dither << 9, 0xE3000000 | top << 10, 0xE4000000 | (top + h - 1) << 10 | (w - 1),
                 0xE5000000 | top << 11, 0xE6000000, 0x02000000, top << 16, h << 16 | w]
        words += [x for p in packets for x in p]
        self.gpu.gpu_gp0_words((ctypes.c_uint32 * len(words))(*words), len(words))
        return self.screen(top, h, w)

    def background(self, fm: FieldMap, layers: set[int], shown: set[tuple[int, int]],
                   characters: tuple[np.ndarray, list] | None = None):
        """The whole background, drawn a screen at a time at (0, 0) of VRAM (no .MIM lies left of x 384), the
        packets from the far end of the ordering table, each slot's last linked first: so a page switch covers the
        layer's tiles before it in the file. Characters (VRAM with their textures, and their packets) are linked
        after the background, as func_800A364C runs before the models, so a tile at a model's slot covers it.
        Returns the picture and the background position of its corner."""
        if not self.gpu:
            return None, (0, 0)
        self.vram[:] = characters[0] if characters else fm.vram
        tile_packets = fm.packets(layers, shown)
        linked = tile_packets + (characters[1] if characters else [])
        packets = [p for _, p in sorted(enumerate(linked), key=lambda p: (-p[1][0], -p[0]))]
        tiles = [(x, y) for _, _, points in tile_packets for _, x, y in points] or [(0, 0)]
        x0, y0 = min(x for x, _ in tiles), min(y for _, y in tiles)
        x1, y1 = max(x for x, _ in tiles) + 32, max(y for _, y in tiles) + 32
        out = np.zeros((y1 - y0, x1 - x0, 3), np.uint8)
        for cy in range(y0, y1, 240):
            for cx in range(x0, x1, 320):
                words = [0xE1000200, 0xE3000000, 0xE4000000 | 239 << 10 | 319, 0xE5000000, 0x02000000, 0,
                         240 << 16 | 320]
                for _, w, points in packets:
                    if not points:
                        words += w
                        continue
                    xs, ys = [x for _, x, _ in points], [y for _, _, y in points]
                    if min(xs) < cx + 320 and max(xs) + 32 > cx and min(ys) < cy + 240 and max(ys) + 32 > cy:
                        w = list(w)
                        for at, x, y in points:
                            w[at] = ((y - cy) & 0x7FF) << 16 | (x - cx) & 0x7FF
                        words += w
                self.gpu.gpu_gp0_words((ctypes.c_uint32 * len(words))(*words), len(words))
                h, w = min(240, y1 - cy), min(320, x1 - cx)
                out[cy - y0:cy - y0 + h, cx - x0:cx - x0 + w] = self.screen()[:h, :w]
        return out, (x0, y0)


def frame_view(m: Model) -> tuple[np.ndarray, float]:
    points = m.positions(m.posed(next((a for a in m.anims if a), None), 0))
    if not len(points):
        return np.zeros(3), 1000.0
    low, high = points.min(axis=0), points.max(axis=0)
    return (low + high) / 2, float(np.linalg.norm(high - low)) * 1.4 + 1


def write_png(path: Path, rgb: np.ndarray):
    h, w = rgb.shape[:2]

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    raw = b"".join(b"\0" + row.tobytes() for row in rgb)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def shot(disc: Path, name: str, anim: int, frame: int, out: Path, yaw: float):
    """The editing view and, if the PS1 view's library is built, the PS1 view beside it (scaled to the same height)."""
    m = load(disc, name)
    renderer, ps1 = Renderer(moderngl.create_standalone_context()), Ps1View()
    renderer.upload(m)
    ps1.upload(m)
    center, distance = frame_view(m)
    a = m.anims[anim] if anim < len(m.anims) and m.anims[anim] else next((a for a in m.anims if a), None)
    posed = m.posed(a, frame % a["frames"] if a else 0)
    renderer.draw(m.positions(posed), yaw, 0.2, distance, center, True)
    picture = renderer.pixels()
    psx = ps1.draw(m, posed, view(yaw, 0.2, distance, center))
    if psx is not None:  # 320x240 shown 4:3 at the same height, nearest pixels
        rows = (np.arange(VIEW) * 240 // VIEW)[:, None]
        picture = np.concatenate([picture, psx[rows, np.arange(VIEW * 4 // 3) * 320 // (VIEW * 4 // 3)]], 1)
    write_png(out, np.ascontiguousarray(picture))


def polygon_packets(m: Model, screen: list[np.ndarray], depth: list[np.ndarray],
                    near: float = 1) -> list[tuple[int, list, list]]:
    """A model's polygons as the field links them (func_800AE4DC): back faces dropped (NCLIP), a quad at slot
    (SZ0 + SZ1 + SZ2 + SZ3) >> 4, a triangle at avsz3's (0x155 (SZ0 + SZ1 + SZ2)) >> 12; each its own packet, with
    where its position words are. A polygon with a corner as near as near is dropped."""
    out = []
    for part, corners, code, shades, attributes in m.polygons:
        xy, d = screen[part][corners], depth[part][corners]
        area = (xy[1, 0] - xy[0, 0]) * (xy[2, 1] - xy[0, 1]) - (xy[2, 0] - xy[0, 0]) * (xy[1, 1] - xy[0, 1])
        if d.min() <= near or area <= 0:
            continue
        words, points = [], []
        for i, (x, y) in enumerate(xy):
            if i < len(shades) and (i == 0 or code & 0x10):
                words.append(shades[i] | (code << 24 if i == 0 else 0))
            points.append((len(words), int(x), int(y)))
            words.append((int(y) & 0x7FF) << 16 | int(x) & 0x7FF)
            if attributes:
                words.append(attributes[i])
        z = int(d.sum())
        out.append((z >> 4 if len(corners) == 4 else 0x155 * z >> 12, words, points))
    return out


def map_shot(disc: Path, name: str, out: Path, story: int):
    """A map as it starts at that point of the story (its characters, its background groups), with the walkmesh,
    exits, triggers, arrows and markers over it."""
    fm = FieldMap(disc, name)
    entities, shown = fm.start(story)
    rgb, origin = Ps1View().background(fm, {0, 1, 2, 3}, shown, fm.characters(disc, entities))
    if rgb is None:
        raise SystemExit("the PS1 view's library isn't built: make editor")
    overlay(rgb, origin, fm, {"walkmesh", "exits", "triggers", "arrows", "markers"})
    write_png(out, rgb)


# The party's ENEMY6 files by character (kernel order; their other forms by a trailing digit), for their limits.
CHARACTERS = {"CLOUD": 0, "HICLOUD": 0, "BARRETT": 1, "TIFA": 2, "EARITH": 3, "RED13": 4, "YUFI": 5, "KETCY": 6,
              "VINSENT": 7, "CID": 8}


class Battle:
    """Battle number (SCENE.BIN's scene number // 4, setup number % 4) as BATTLE.X sets it up: its stage (location
    0-49 in STAGE1, 50-89 in STAGE2, D_800E7D98's files), the party (ENEMY6 files) and the enemies where placement
    puts them, each one's TIM loaded as func_800D2980 loads it. Loaded once; packets() draws a frame."""

    def __init__(self, disc: Path, number: int, party: tuple = ("CLOUD",)):
        self.table = scene.scene_json(scene.read_scenes((disc / "BATTLE/SCENE.BIN").read_bytes())[number // 4])
        self.setup = setup = self.table["setups"][number % 4]
        location = setup["location"]
        stage = battle.stage_json(lzs((disc / f"STAGE{1 if location < 50 else 2}/STAGE{location:02d}.LZS")
                                      .read_bytes()))
        self.vram = vram(disc, None)
        loads = [(None, None, stage)]
        self.module = battle.module(disc)
        self.kernel = [raw for _, raw in bingz((disc / "INIT/KERNEL.BIN").read_bytes())]
        self.limits = (disc / "MENU/LIMTMENU.MNU").read_bytes()
        exe = (disc / "SCUS_941.63").read_bytes()
        self.atan = list(struct.unpack_from("<1025h", exe, 0x8004FC98 - 0x80010000 + 0x800))  # libgte's ratan2
        self.camdat = lzs((disc / f"STAGE1/CAMDAT{battle.CAMDATS.get(setup['layout'], 0)}.LZS").read_bytes())
        places = battle.placement(self.module, setup, len(party))
        for slot, (name, place) in enumerate(zip(party, places)):
            raw = lzs((disc / f"ENEMY6/{name}.LZS").read_bytes())
            loads.append((slot, place, battle.moved(battle.model_json(raw, True), (0, 0x10, 0x11)[slot], 3 * slot),
                          battle.sections(raw)[1]))
        enemies = [(k, e) for k, e in enumerate(setup["formation"]) if e["enemy_id"] != 0xFFFF]
        for (k, enemy), place in zip(enemies, places[len(party):]):
            raw = lzs(next(disc.glob(f"ENEMY[1-6]/ENEMY{enemy['enemy_id']:03d}.LZS")).read_bytes())
            page = (0x13 if location == 0x4E else 0x12) + self.table["enemy_ids"].index(enemy["enemy_id"])
            loads.append((4 + k, place, battle.moved(battle.model_json(raw, False), page, 3 * (4 + k)),
                          battle.sections(raw)[1]))
        self.actors = []  # (actor, place, model, shadow size)
        self.settings = {}  # actor: section 1 (its action scripts and camera settings)
        for actor, place, loaded, *settings in loads:
            for x, y, w, h, data in loaded["tim"]:
                self.vram[y:y + h, x:x + w] = np.frombuffer(data, "<u2").reshape(h, w)
            if actor is not None:
                self.actors.append((actor, place, Model(loaded["bones"], loaded["parts"], loaded["animations"],
                                                         None, [], 0, self.vram), loaded["shadow"]))
                self.settings[actor] = settings[0]
        self.stage, self.opening, self.timeline = stage, [], []
        self.sections = {k: Model([{"length": 0, "parent": -1}], [part], [], None, [], 0, self.vram)
                         for k, part in stage["parts"].items()}

    def act(self, attacker: int, script: int, target: int, camera: int, variant: int, start: tuple):
        """Play an action from the battle's start, everyone at their places: attacker's action script number aimed
        at target, with attack camera number (-1 holds the camera) variant 0-2 from the layout's CAMDAT file, from the
        camera start (eye, target). The timeline then holds each battle frame's poses and camera."""
        actors = {}
        for actor, (place, facing), m, _ in self.actors:
            frames = [a["frames"] if a else 0 for a in m.anims] + [0] * 256
            actors[actor] = battle.Actor(battle.action_scripts(self.settings[actor], self.module), frames, place,
                                         facing, struct.unpack_from("<h", self.settings[actor], 4)[0],
                                         pos=list(place), facing=facing)
        actors[attacker].script = script
        self.fight = battle.Fight(actors, attacker, target, self.atan)
        self.channels = None
        if camera >= 0:
            eye, aim = battle.camdat(self.camdat, camera)[variant]
            settings = {k: battle.camera_settings(s1) for k, s1 in self.settings.items()}
            self.channels = (battle.Channel(eye, list(start[0])), battle.Channel(aim, list(start[1])), [],
                             battle.View([self.camera(k) for k in range(4)], 0, start, actors, settings, None,
                                         self.atan, attacker, target, [target]))
        self.timeline, self.shown, self.start = [], start, start

    def action_frame(self, frame: int) -> tuple[dict, tuple]:
        """Battle frame frame of the action: each actor's (animation, frame, place, facing) and the camera it's
        drawn from (the frame before's: func_800B9568 draws before the camera moves). Its camera script runs while
        the attacker's script does, then the camera halves its way back to the setup's camera 0."""
        models = {actor: m for actor, _, m, _ in self.actors}
        while len(self.timeline) <= frame:
            fight = self.fight
            battle.action_frame(fight)
            poses = {k: (a.anim, a.shown, list(a.pos), a.facing) for k, a in fight.actors.items()}

            def bone(k: int, n: int) -> list[int]:
                anim, at, pos, facing = poses[k]
                start = model.pose(models[k].bones, models[k].anims[anim], at)[n][1]
                turn = np.array(model.rotation(0, facing / 16, 0))
                return [round(v) for v in turn @ np.array(start) + pos]

            drawn = self.shown
            attacker = fight.actors[fight.attacker]
            if self.channels and attacker.script != attacker.idle:
                eye, aim, tasks, view = self.channels
                view.bone, view.current = bone, self.shown
                battle.camera_frame(eye, aim, tasks, view)
                self.shown = (list(eye.pos), list(aim.pos))
            elif self.channels:
                goal = self.camera(self.channels[3].chosen)
                self.shown = battle.returning(self.shown, goal) or goal
            self.timeline.append((poses, drawn))
        return self.timeline[frame]

    def camera(self, k: int, frame: int = 0) -> tuple[list[int], list[int]]:
        """The setup's camera k's eye and target, or (k -1) the opening's as battle frame frame is drawn: the camera
        after the frame before (func_800B9568 draws before the camera moves)."""
        if k < 0:
            at = max(frame - 1, 0)
            if at >= len(self.opening):
                self.opening = battle.opening(self.module, self.setup, at + 150)
            return self.opening[at]
        c = self.setup["cameras"][k]
        return [c["x"], c["y"], c["z"]], [c["target_x"], c["target_y"], c["target_z"]]

    def packets(self, eye, target, frame: int = 0, frames: dict | None = None,
                poses: dict | None = None) -> list[list[int]]:
        """The packets of battle frame frame seen from eye to target: the stage as stage_draws moves it, each actor
        in its battle stance (animation 0) at that frame (frames, if given, its frame by actor; poses, if given, its
        animation, frame, place and facing by actor, as action_frame plays them) with its shadow. In
        drawing order: the far layer, the backdrop (the last linked first), the shadows (subtracted: a draw mode of
        blend 2 before them, of 1 after, func_800B8360), then the main ordering table (avsz >> 2), the stage's
        linked before the actors'."""
        turn, shift = battle.look_at(eye, target)
        turn, shift = np.array(turn, np.float32) / 4096, np.array(shift, np.float32)
        linked = []  # (list, bucket, words): far 0, backdrop 1, shadows 2, main 3

        def link(m: Model, posed: list[np.ndarray], rank: int, down: int | None):
            screen, depth = [], []
            for verts in posed:
                v = verts @ turn.T + shift
                z = np.maximum(v[:, 2], 1)
                # the GTE's 512 projection about (160, 120), truncated, its screen x, y held to -1024..1023
                screen.append(np.clip(np.floor(np.stack([160 + 512 * v[:, 0] / z, 120 + 512 * v[:, 1] / z], 1)),
                                      -1024, 1023).astype(int))
                depth.append(v[:, 2])
            # rtpt's divide overflows nearer than H / 2: such polygons are dropped
            linked.extend((rank, 0 if down is None else otz >> down, words)
                          for otz, words, _ in polygon_packets(m, screen, depth, 256))

        ranks = {"far": 0, "backdrop": 1, "main": 3}
        for k, layer, down, (rx, ry, rz), position in battle.stage_draws(self.stage["type"], self.stage["first"],
                                                                           len(self.stage["parts"]) + 1, frame):
            m = self.sections[k]
            spin = np.array(model.rotation(rx / 16, ry / 16, rz / 16), np.float32)
            link(m, [m.vertices[0] @ spin.T + np.array(position, np.float32)], ranks[layer], down)
        for actor, ((x, y, z), facing), m, size in self.actors:
            anim = next((a for a in m.anims if a), None)
            at = ((frames or {}).get(actor, frame) % anim["frames"]) if anim else 0
            if poses:
                number, at, (x, y, z), facing = poses[actor]
                anim = m.anims[number] or anim
            place = np.array(model.rotation(0, facing / 16, 0), np.float32)
            link(m, [v @ place.T + np.array([x, y, z], np.float32) for v in m.posed(anim, at)], 3, 2)
            disc = Model([{"length": 0, "parent": -1}],
                         [battle.shadow(self.module, model.pose(m.bones, anim, at)[0][1], size)], [], None, [], 0,
                         self.vram)
            link(disc, [disc.vertices[0] @ place.T + np.array([x, 0, z], np.float32)], 2, None)
        order = sorted(range(len(linked)), key=lambda i: (linked[i][0], -linked[i][1], -i))
        out = [linked[i][2] for i in order]
        shadows = [i for i, k in enumerate(order) if linked[k][0] == 2]
        at = shadows[0] if shadows else next((i for i, k in enumerate(order) if linked[k][0] == 3), len(out))
        return out[:at] + [[0xE1000640]] + out[at:at + len(shadows)] + [[0xE1000620]] + out[at + len(shadows):]


def battle_shot(disc: Path, number: int, camera: int, frame: int, party: tuple, out: Path,
                action: list | None = None):
    b = Battle(disc, number, party)
    if action:
        b.act(*(action + [-1, 0][len(action) - 3:]), start=b.camera(max(camera, 0)))
        poses, eye_target = b.action_frame(frame)
        rgb = Ps1View().battle(b.vram, b.packets(*eye_target, frame, poses=poses))
    else:
        rgb = Ps1View().battle(b.vram, b.packets(*b.camera(camera, frame), frame))
    if rgb is None:
        raise SystemExit("the PS1 view's library isn't built: make editor")
    write_png(out, rgb)


# The world map's models seen in the lab, by id (0-2 the party leader; 32-40 WM3.BSZ's party, 41 and 42 chocobos),
# and the tint each takes (model 4 on spawning, the others by script: 19's 0x34B n, 13's 0x34C n + 5).
WORLD_MODELS = {3: "Highwind", 4: "chocobo (wild)", 7: "Tiny Bronco", 13: "submarine", 19: "chocobo (the party's)"}
WORLD_TINTS = {4: 0, 19: 4, 13: 6}
SHADOWED = 0x311B6F05  # the walkmesh types an entity casts a shadow on (func_800AAB18)


class WorldMap:
    """World map kind (0 the world, 2 and 3 the two that tile parts of it) at map_stage stage: its meshes, its
    container's textures in VRAM, light zones and models (container: WM0-WMA.TXZ, one a stage of the story, for the
    world; WMB, WMC for maps 2 and 3), the camera's tables and the light WORLD.BIN gives it (func_800AF110), the party
    leader (WM{leader}.BSZ: func_800B69A4 loads it at 0x8014A600) and WM3.BSZ's models (ids 32-42), their images in
    VRAM. view() draws it round a point as the game does, overview() all of it from above."""

    def __init__(self, disc: Path, kind: int, stage: int = 0, leader: int = 0, container: int = 0):
        self.kind = kind
        self.container = container = {2: 11, 3: 12}.get(kind, container)  # map kind + 9 (func_800A460C)
        self.meshes = world.map_meshes((disc / f"WORLD/WM{kind}.MAP").read_bytes(), kind, stage)
        image = lzs((disc / f"WORLD/WM{container:X}.TXZ").read_bytes())
        self.table, images = world.textures(image)
        at = world.section_at(image)
        self.zones = world.light_zones(world.scripts_json(image[at:at + world.SIZE])["code"])
        self.vram = np.zeros((512, 1024), np.uint16)
        for x, y, w, h, data in images:
            self.vram[y:y + h, x:x + w] = np.frombuffer(data, "<u2").reshape(h, w)
        self.module = gzip.decompress((disc / "WORLD/WORLD.BIN").read_bytes()[8:])
        exe = next(disc.glob("SCUS_941.6*")).read_bytes()
        self.sines = list(struct.unpack_from("<8192h", exe, 0x8004BC98 - 0x80010000 + 0x800))  # libgte's table
        bsz = lzs((disc / f"WORLD/WM{leader}.BSZ").read_bytes())
        wm3 = lzs((disc / "WORLD/WM3.BSZ").read_bytes())  # loaded at 0x8016D000 (func_800B6C84)
        sections = [image[world.section_at(image, 0):world.section_at(image, 1)]]
        self.models = {leader: world.world_models(bsz, 0x8014A600, 4 + struct.unpack_from("<I", bsz, 4)[0])[0]}
        container_models = world.world_models(sections[0], 0x8013A800, 0)
        self.models |= {i: container_models[k] for i, k in world.model_slots(self.module, container).items()}
        self.models |= {32 + k: m for k, m in enumerate(
            world.world_models(wm3, 0x8016D000, 4 + struct.unpack_from("<I", wm3, 4)[0]))}
        self.leader, self.lit = leader, set()
        for x, y, w, h, data in (world.model_images(bsz, 4 + struct.unpack_from("<I", bsz, 8)[0])
                                 + world.model_images(image, world.section_at(image, 1))
                                 + world.model_images(wm3, 4 + struct.unpack_from("<I", wm3, 8)[0])):
            self.vram[y:y + h, x:x + w] = np.frombuffer(data, "<u2").reshape(h, w)

    def word(self, address: int, n: int = 1, kind: str = "h") -> tuple:
        return struct.unpack_from(f"<{n}{kind}", self.module, address - 0x800A0000)

    def ground(self, x: int, z: int) -> int:
        """The height of the vertex nearest (x, z): the focus's y."""
        m = self.meshes.get((x >> 13, z >> 13))
        if m is None:
            return 0
        return min(m["vertices"], key=lambda v: abs(v[0] - (x & 0x1FFF)) + abs(v[2] - (z & 0x1FFF)))[1]

    def ground_type(self, x: int, z: int) -> int:
        """The walkmesh type of the triangle under (x, z): its type byte & 0x1F (an entity's +0x4A)."""
        m = self.meshes.get((x >> 13, z >> 13))
        lx, lz = x & 0x1FFF, z & 0x1FFF
        for t in m["triangles"] if m else []:
            (ax, _, az), (bx, _, bz), (cx, _, cz) = (m["vertices"][i] for i in t[:3])
            d = (bz - cz) * (ax - cx) + (cx - bx) * (az - cz)
            u = (bz - cz) * (lx - cx) + (cx - bx) * (lz - cz)
            v = (cz - az) * (lx - cx) + (ax - cx) * (lz - cz)
            if d and (u * d >= 0 and v * d >= 0 and (d - u - v) * d >= 0):
                return t[3] & 0x1F
        return 0

    def view(self, ps1: Ps1View, x: int, z: int, yaw: int, mode: int = 0, entities: list | None = None,
             settled: tuple | None = None, facing: int | None = None) -> np.ndarray | None:
        """The world round (x, z) as it draws a frame with the camera settled in view mode (0-3; the tilt and distance
        D_800C6628 and D_800C6638 give) at yaw (4096 a turn): the sky (not on map 2), then the terrain through the
        GTE, then (not on map 2) the party leader standing there facing away from the camera or by facing (frame 0
        of animation 0, as func_800AAB18 places the player) and the entities (model id, x from the point, y or None
        for the ground there, z from the point, facing, animation, frame, tint or None) as it places the others (the
        world's curve lowers them), each with its shadow on ground of a type in SHADOWED (models 3 and 11 anywhere),
        sized by model (func_800B5C7C); nothing past ordering table entry 2500 (DrawOTag starts there). Lit and
        coloured by the light zones at the point; H, the curve (5000) and (map 2) the fog as WORLD.BIN sets them
        (func_800A0D2C), the screen's centre row 120 in view 0 and 160 in the others (func_800A2088). Models are lit
        once, as the game lights them at load: the leader and WM3.BSZ's ids 32-40, not the container's. The game's
        camera eases toward those in integer steps and can stop a little short (tilt 1797 of 1800, centre row 157 of
        160), by how it got there: settled, (tilt, distance, centre row) as a lab dump holds them, stands in."""
        if not ps1.gpu:
            return None
        gte, height = ps1.gpu, self.ground(x, z)
        tilt, distance = self.word(0x800C6628 + 4 * mode, 1, "i")[0], self.word(0x800C6638 + 4 * mode, 1, "i")[0]
        centre = 160 if mode else 120
        if settled:
            tilt, distance, centre = settled
        _, rows = world.camera(self.sines, self.word(0x800BE1E8, 512, "H"), tilt, distance, height, yaw)
        rotation = [v for row in rows for v in row]
        count = (len(self.module) - (0x800C7938 - 0x800A0000)) // 8
        for i, m in self.models.items():  # once, as the game does at load
            if i not in self.lit and (i < 3 or 32 <= i < 0x29):
                world.light_model(gte, m, [self.word(0x800C7938 + 8 * k, 3) for k in range(count)])
            self.lit.add(i)
        light, top, bottom = world.zone_colours(self.zones, (x, height, z), self.word(0x800C6848, 9)[0:7:3],
                                                self.word(0x800C6768, 3, "B"), self.word(0x800C676C, 3, "B"))
        matrix = self.word(0x800C6828 if self.kind == 2 else 0x800C6808, 10, "H")
        for i in range(5):
            gte.gte_write_ctrl(8 + i, matrix[2 * i] | matrix[2 * i + 1] << 16)
            gte.gte_write_ctrl(16 + i, 0)
        gte.gte_write_ctrl(16, light[0] & 0xFFFF)
        gte.gte_write_ctrl(17, (light[1] & 0xFFFF) << 16)
        gte.gte_write_ctrl(19, light[2] & 0xFFFF)
        for i, c in enumerate((0x20, 0x20, 0x30) if self.kind == 2 else (0x40, 0x40, 0x40)):
            gte.gte_write_ctrl(13 + i, c << 4)
            gte.gte_write_ctrl(21 + i, 0)
        gte.gte_write_ctrl(24, 160 << 16)
        gte.gte_write_ctrl(25, centre << 16)
        gte.gte_write_ctrl(26, self.word(0x800C65EC, 1, "i")[0])
        sky = [] if self.kind == 2 else world.sky_packets(gte, self.sines, rotation, (0, 0, distance), height, yaw,
                                                          0, 5000, top, bottom, world.sky_scroll(0, 0, yaw, 5000)[0])
        terrain = [p for p in world.terrain_packets(gte, self.meshes, self.table, rotation, (0, 0, distance),
                                                    (x, height, z), 5000, 0x3E80 if self.kind == 2 else 0)
                   if p[0] <= 2500]
        packets = []
        if self.kind != 2:
            facing = yaw if facing is None else facing
            bones = world.pose(self.models[self.leader], rows, (0, 0, distance // 4), 0, 0,
                               (0xC0, facing >> 4 & 0xFF, 0))
            packets += [p for p in world.model_packets(gte, self.models[self.leader], bones) if p[0] <= 2500]
            shadows = [(self.leader, 0, height, 0, facing)]
            for ident, dx, y, dz, facing, anim, frame, tint in entities or []:
                m = self.models.get(ident)
                if m is None:
                    continue
                m = m if tint is None else world.tinted(self.module, m, tint)
                anim = min(anim, len(m["anims"]) - 1)
                if not m["anims"][anim]["frames"]:  # WM3.BSZ's model 1 has an empty animation 0
                    anim = next(k for k, a in enumerate(m["anims"]) if a["frames"])
                y = self.ground(x + dx, z + dz) if y is None else y
                t = world.entity_translation(gte, rotation, (0, 0, distance), height, 5000, (dx, y, dz))
                bones = world.pose(m, rows, t, anim, frame % m["anims"][anim]["frames"], (0xC0, facing >> 4 & 0xFF, 0))
                packets += [p for p in world.model_packets(gte, m, bones) if p[0] <= 2500]
                shadows.append((ident, dx, y, dz, facing))
            for ident, dx, y, dz, facing in shadows:
                if not (SHADOWED >> self.ground_type(x + dx, z + dz) & 1 or ident in (3, 11)):
                    continue
                across, along = {3: (200, 600), 11: (600, 1200)}.get(ident, (150, 150) if ident in (4, 19, 41, 42)
                                                                       else (100, 100))
                packets.append(world.shadow_packet(gte, self.sines, rotation, (0, 0, distance), (dx, dz), height,
                                                   y, facing + 0x800, (across, along), 0x20,
                                                   (0x18, 0x38, 15, 15)))
        packets += terrain  # linked after the models: drawn first where they share an entry (func_800A141C)
        order = sorted(range(len(packets)), key=lambda i: (-packets[i][0], -i))
        return ps1.world(self.vram, sky + [packets[i][1] for i in order])

    def overview(self, ps1: Ps1View) -> np.ndarray | None:
        """The whole map from above, 16 pixels a mesh (north up), its textures unlit, drawn by the port's GPU in two
        halves left of the textures in VRAM."""
        if not ps1.gpu:
            return None
        out = np.zeros((28 * 16, 36 * 16, 3), np.uint8)
        for half in (0, 1):
            packets = []
            for (mx, mz), m in self.meshes.items():
                if mx // 18 != half:
                    continue
                points = [((mz * 16 + (z >> 9)) & 0x7FF) << 16 | (mx % 18 * 16 + (x >> 9)) & 0x7FF
                          for x, _, z in m["vertices"]]
                for a, b, c, _, uv0, uv1, uv2, texture in m["triangles"]:
                    entry = self.table[texture & 0x1FF]
                    packets.append([0x34808080, points[a], uv0[0] | uv0[1] << 8 | (entry & 0xFFFF) << 16,
                                    0x808080, points[b], uv1[0] | uv1[1] << 8 | (entry >> 16) << 16,
                                    0x808080, points[c], uv2[0] | uv2[1] << 8])
            out[:, half * 288:half * 288 + 288] = ps1.world(self.vram, packets, 288, 448, 0, False)
        return out


def world_shot(disc: Path, kind: int, at: tuple[int, int], yaw: int, mode: int, out: Path, container: int = 0,
               models: list | None = None):
    """The world round at with models (id, x and z from at, facing) on the ground, the overview beside it (halved)
    with at marked."""
    wm, ps1 = WorldMap(disc, kind, container=container), Ps1View()
    picture = wm.view(ps1, at[0], at[1], yaw, mode, [(i, dx, None, dz, facing, 0, 0, WORLD_TINTS.get(i))
                                                     for i, dx, dz, facing in models or []])
    if picture is None:
        raise SystemExit("the PS1 view's library isn't built: make editor")
    small = wm.overview(ps1)[::2, ::2].copy()
    small[at[1] >> 10, :] = small[:, at[0] >> 10] = (255, 0, 255)
    write_png(out, np.concatenate([picture, small], 1))


def run(disc: Path, start: str | None, start_map: str | None = None, frames: int | None = None,
        start_battle: int | None = None, start_world: int | None = None):
    """The window; frames, if given, closes it after that many (to check it runs)."""
    from imgui_bundle import hello_imgui, imgui, immapp

    models, maps, map_ids_ = names(disc), [p.stem for p in field.maps(disc)], field.map_names(disc)
    enemies = enemy_names(disc)
    labels = {n: f"{n} {enemies.get(Path(n).stem, '')}".rstrip() for n in models}
    scenes = [scene.scene_json(raw) for raw in scene.read_scenes((disc / "BATTLE/SCENE.BIN").read_bytes())]
    battles = []  # the setups with a stage
    for number in range(4 * len(scenes)):
        table, setup = scenes[number // 4], scenes[number // 4]["setups"][number % 4]
        if setup["location"] < 90:
            who = [table["enemies"][table["enemy_ids"].index(e["enemy_id"])]["name"].strip()
                   for e in setup["formation"] if e["enemy_id"] != 0xFFFF]
            battles.append((number, f"{number:03X} {', '.join(who)} (stage {setup['location']:02d})"))
    party_files = {p.stem for p in (disc / "ENEMY6").glob("*.LZS") if not p.stem.startswith("ENEMY")} - {"SEFFECT"}
    state = {"choice": start or models[0], "frames": 0, "anim": 0, "frame": 0.0, "play": True, "yaw": 0.6,
             "pitch": 0.2, "zoom": 1.0, "ps1": True, "renderer": None, "model": None, "center": None,
             "distance": 1.0, "filter": "", "map_filter": "", "map_zoom": 2.0, "layers": {0, 1, 2, 3},
             "overlays": {"walkmesh", "exits", "triggers", "arrows", "markers"}, "vram": "model", "story": 0,
             "characters": True, "battle": battles[0][0] if start_battle is None else start_battle,
             "battle_filter": "", "camera": -1, "party": "CLOUD,BARRETT,TIFA", "battle_picture": None,
             "battle_frame": 0.0, "battle_play": True, "battle_drawn": None, "acting": False,
             "action": {"attacker": 0, "script": 20, "target": 4, "camera": -1, "variant": 0}, "command": 0,
             "commands": {},
             "world_kind": 0 if start_world is None else start_world, "world_stage": 0, "world": None,
             "world_at": [185636, 123325], "world_yaw": 0, "world_view": 0, "world_leader": 0,
             "world_container": 0, "world_entities": [],
             "world_drawn": None}

    def choose(name: str):
        state["choice"], state["model"] = name, load(disc, name)
        state["renderer"].upload(state["model"])
        state["ps1view"].upload(state["model"])
        state["center"], state["distance"] = frame_view(state["model"])
        state["anim"], state["frame"] = next((i for i, a in enumerate(state["model"].anims) if a), 0), 0.0

    def redraw():
        if state.get("picture"):
            state["picture"].release()
        state["picture"] = None

    def choose_map(name: str):
        state["map"] = FieldMap(disc, name)
        start_map_at(state["story"])

    def start_map_at(story: int):
        state["entities"], state["shown"] = state["map"].start(story)
        state["cast"] = state["map"].characters(disc, state["entities"])
        redraw()

    def maps_gui():
        imgui.begin_child("maps", imgui.ImVec2(220, 0))
        _, state["map_filter"] = imgui.input_text("##mapfilter", state["map_filter"])
        for name in maps:
            if state["map_filter"].upper() in name and imgui.selectable(name, name == state["map"].name)[0]:
                choose_map(name)
        imgui.end_child()
        imgui.same_line()
        fm = state["map"]
        if state["picture"] is None:  # drawn again only when something changes
            rgb, state["origin"] = state["ps1view"].background(fm, state["layers"], state["shown"],
                                                               state["cast"] if state["characters"] else None)
            state["vram"] = "map"
            if rgb is None:
                imgui.text_wrapped("The map view draws with the PS1 view's library. Build it with: make editor")
                return
            overlay(rgb, state["origin"], fm, state["overlays"])
            state["picture"] = state["renderer"].ctx.texture((rgb.shape[1], rgb.shape[0]), 3,
                                                             np.ascontiguousarray(rgb).tobytes())
            state["picture"].filter = (moderngl.NEAREST, moderngl.NEAREST)
        picture, zoom = state["picture"], state["map_zoom"]
        imgui.begin_child("background", imgui.ImVec2(-420, 0), 0, imgui.WindowFlags_.horizontal_scrollbar)
        imgui.image(imgui.ImTextureRef(picture.glo), imgui.ImVec2(picture.width * zoom, picture.height * zoom))
        if imgui.is_item_hovered():
            io = imgui.get_io()
            state["map_zoom"] = max(0.5, min(6.0, zoom * 1.1 ** io.mouse_wheel))
            at = (io.mouse_pos - imgui.get_item_rect_min()) / zoom
            state["cursor"] = (int(at.x) + state["origin"][0], int(at.y) + state["origin"][1])
        imgui.end_child()
        imgui.same_line()
        imgui.begin_child("map info", imgui.ImVec2(0, 0))
        changed = False
        imgui.text("layers")
        for layer in range(4):
            imgui.same_line()
            if imgui.checkbox(f"{layer + 1}", layer in state["layers"])[0]:
                state["layers"] ^= {layer}
                changed = True
        for key in ("walkmesh", "exits", "triggers", "arrows", "markers"):
            if imgui.checkbox(key, key in state["overlays"])[0]:
                state["overlays"] ^= {key}
                changed = True
        if imgui.checkbox("characters", state["characters"])[0]:
            state["characters"] = not state["characters"]
            changed = True
        imgui.set_next_item_width(120)
        story_changed, story = imgui.input_int("story", state["story"])
        if story_changed and story != state["story"]:
            state["story"] = max(0, story)
            start_map_at(state["story"])
        imgui.text(f"at {state['cursor']}" if state.get("cursor") else "")
        if fm.groups and imgui.collapsing_header("tile groups (state bits)"):
            for g, bit in fm.groups:
                if imgui.checkbox(f"group {g}: {bit:08b}", (g, bit) in state["shown"])[0]:
                    state["shown"] ^= {(g, bit)}
                    changed = True
        if imgui.collapsing_header("entities", imgui.TreeNodeFlags_.default_open):
            for e in state["entities"]:
                where = f"at {e['x']}, {e['y']}, facing {e['dir']}" if "x" in e else "not placed"
                party = f", party {e['pc']}" if "pc" in e else ""
                imgui.text(f"{e['name']}: model {e['model']}{party}, {where}{'' if e['visible'] else ', hidden'}")
        if imgui.collapsing_header("exits", imgui.TreeNodeFlags_.default_open):
            for i, e in enumerate(fm.map["exits"]):
                if e:
                    to = map_ids_[e["map"]] if e["map"] < len(map_ids_) else "?"
                    imgui.text(f"{i}: {to} ({e['map']}) at {e['x']}, {e['y']}, triangle {e['triangle']}")
        if imgui.collapsing_header("triggers"):
            for i, t in enumerate(fm.map["triggers"]):
                if t:
                    imgui.text(f"{i}: group {t['group']} bit {t['bit']}, kind {t['kind']}, sound {t['sound']}")
        imgui.end_child()
        if changed:
            redraw()

    def battles_gui():
        imgui.begin_child("battles", imgui.ImVec2(300, 0))
        _, state["battle_filter"] = imgui.input_text("##battlefilter", state["battle_filter"])
        for number, label in battles:
            chosen = number == state["battle"]
            if state["battle_filter"].upper() in label.upper() and imgui.selectable(label, chosen)[0]:
                state["battle"], state["camera"], state["battle_picture"] = number, -1, None
                state["battle_frame"], state["battle_play"], state["acting"] = 0.0, True, False
        imgui.end_child()
        imgui.same_line()
        table = scenes[state["battle"] // 4]
        setup = table["setups"][state["battle"] % 4]
        party = tuple(n for n in state["party"].upper().replace(" ", "").split(",") if n in party_files)[:3]
        if state["battle_picture"] is None:  # loaded again only when the battle or party changes
            state["battle_obj"] = Battle(disc, state["battle"], party)
            state["battle_picture"] = state["renderer"].ctx.texture((320, 240), 3)
            state["battle_picture"].filter = (moderngl.NEAREST, moderngl.NEAREST)
            state["battle_drawn"] = None
        if state["battle_play"]:  # 15 battle frames a second (VSync(4), func_800D8A88)
            state["battle_frame"] += imgui.get_io().delta_time * 15
        frame, b = int(state["battle_frame"]), state["battle_obj"]
        drawn = (frame, state["camera"], state["acting"])
        if state["battle_drawn"] != drawn or state["vram"] != "battle":
            if state["acting"]:
                poses, camera = b.action_frame(frame)
                rgb = state["ps1view"].battle(b.vram, b.packets(*camera, frame, poses=poses), (0, 239))
            else:
                rgb = state["ps1view"].battle(b.vram, b.packets(*b.camera(state["camera"], frame), frame), (0, 239))
            state["vram"], state["battle_drawn"] = "battle", drawn
            if rgb is None:
                imgui.text_wrapped("The battle view draws with the PS1 view's library. Build it with: make editor")
                return
            state["battle_picture"].write(np.ascontiguousarray(rgb).tobytes())
        imgui.image(imgui.ImTextureRef(state["battle_picture"].glo), imgui.ImVec2(640, 480))
        imgui.same_line()
        imgui.begin_child("battle info", imgui.ImVec2(0, 0))
        imgui.text(f"stage {setup['location']:02d}, layout {setup['layout']}, camera script {setup['camera_script']}")
        _, state["battle_play"] = imgui.checkbox("play", state["battle_play"])
        imgui.same_line()
        imgui.set_next_item_width(200)
        changed, f = imgui.slider_int("battle frame", frame, 0, 600)
        if changed:
            state["battle_frame"], state["battle_play"] = float(f), False
        if imgui.radio_button("opening", state["camera"] == -1):
            state["camera"] = -1
        for k, camera in enumerate(setup["cameras"]):
            if camera["x"] != -1 and imgui.radio_button(f"camera {k}", state["camera"] == k):
                state["camera"] = k
        imgui.set_next_item_width(200)
        entered, state["party"] = imgui.input_text("party", state["party"], imgui.InputTextFlags_.enter_returns_true)
        if entered:
            state["battle_picture"] = None
        imgui.text_wrapped(f"ENEMY6 files, up to 3: {', '.join(sorted(party_files))}")
        imgui.text("enemies")
        names, records = dict(enumerate(party)), {}
        for k, e in enumerate(setup["formation"]):
            if e["enemy_id"] != 0xFFFF:
                records[4 + k] = table["enemies"][table["enemy_ids"].index(e["enemy_id"])]
                names[4 + k] = records[4 + k]["name"].strip()
                imgui.text(f"{names[4 + k]} (ENEMY{e['enemy_id']:03d}) at {e['x']}, {e['y']}, {e['z']}")
        imgui.separator()
        # An action: its attacker's action script and attack camera; an enemy's attacks carry both (animation_n,
        # camera_n), the party's come from their commands (battle.party_commands: Cloud's Attack 20 holding the
        # camera, Ice 29 with camera 7, in the first battle).
        action, actors = state["action"], sorted(names)
        imgui.text("action")
        for key in ("attacker", "target"):
            imgui.set_next_item_width(200)
            at = actors.index(action[key]) if action[key] in actors else 0
            _, at = imgui.combo(key, at, [f"{k}: {names[k]}" for k in actors])
            action[key] = actors[at]
        for n in range(1, 17):
            enemy = records.get(action["attacker"])
            if not enemy or enemy[f"attack_{n}"] == 0xFFFF:
                continue
            attack = next((a for a in table["attacks"] if a["id"] == enemy[f"attack_{n}"]), None)
            camera = enemy[f"camera_{n}"] if enemy[f"camera_{n}"] != 0xFFFF else (
                attack["unknown_08"] | attack["unknown_09"] << 8 if attack else 0xFFFF)
            label = f"{attack['name'] if attack else enemy[f'attack_{n}']}: script {enemy[f'animation_{n}']}"
            if imgui.selectable(label, action["script"] == enemy[f"animation_{n}"])[0]:
                action["script"], action["camera"] = enemy[f"animation_{n}"], camera - 0x10000 * (camera > 0x7FFF)
        if action["attacker"] < len(party):  # the camera at one target; the one at all is shown after it
            stem, at_party = party[action["attacker"]], action["target"] < 4
            if (stem, at_party) not in state["commands"]:
                state["commands"][stem, at_party] = battle.party_commands(
                    b.module, b.kernel, b.limits, CHARACTERS.get(stem.rstrip("234")), at_party)
            commands = state["commands"][stem, at_party]
            state["command"] = min(state["command"], len(commands) - 1)
            imgui.set_next_item_width(200)
            _, state["command"] = imgui.combo("command", state["command"], [c[0] for c in commands])
            imgui.begin_list_box("##choices", imgui.ImVec2(280, 150))
            for k, (label, script, one, every) in enumerate(commands[state["command"]][1]):
                text = f"{label}: script {script}, camera {one}" + (f" (all {every})" if every != one else "")
                if imgui.selectable(f"{text}##{k}", action["script"] == script and action["camera"] == one)[0]:
                    action["script"], action["camera"] = script, one
            imgui.end_list_box()
        for key, top in (("script", 73), ("camera", 500), ("variant", 2)):
            imgui.set_next_item_width(200)
            _, action[key] = imgui.input_int(key, action[key])
            action[key] = max(-1 if key == "camera" else 0, min(action[key], top))
        if imgui.button("play action"):
            b.act(**action, start=b.camera(max(state["camera"], 0)))
            state["acting"], state["battle_frame"], state["battle_play"] = True, 0.0, True
        imgui.same_line()
        if imgui.button("stance"):
            state["acting"] = False
        imgui.end_child()

    def world_gui():
        wm = state["world"]
        wanted = (state["world_kind"], state["world_stage"], state["world_leader"], state["world_container"])
        if wm is None or state["world_loaded"] != wanted:
            wm = state["world"] = WorldMap(disc, *wanted)
            state["world_loaded"], state["world_drawn"] = wanted, None
            for key in ("world_picture", "world_overview"):
                state[key] = state["renderer"].ctx.texture((320, 224) if key == "world_picture" else (576, 448), 3)
                state[key].filter = (moderngl.NEAREST, moderngl.NEAREST)
            overview = wm.overview(state["ps1view"])
            if overview is None:
                imgui.text_wrapped("The world view draws with the PS1 view's library. Build it with: make editor")
                return
            state["world_overview"].write(np.ascontiguousarray(overview).tobytes())
        x, z = state["world_at"]
        entities = [(e["id"], e["dx"], None, e["dz"], e["facing"], 0, e["frame"], e["tint"])
                    for e in state["world_entities"]]
        drawn = (x, z, state["world_yaw"], state["world_view"], repr(entities))
        if state["world_drawn"] != drawn or state["vram"] != "world":
            rgb = wm.view(state["ps1view"], x, z, state["world_yaw"], state["world_view"], entities)
            state["vram"], state["world_drawn"] = "world", drawn
            state["world_picture"].write(np.ascontiguousarray(rgb).tobytes())
        imgui.image(imgui.ImTextureRef(state["world_picture"].glo), imgui.ImVec2(640, 448))
        imgui.same_line()
        imgui.begin_group()
        imgui.image(imgui.ImTextureRef(state["world_overview"].glo), imgui.ImVec2(576, 448))
        if imgui.is_item_hovered() and imgui.is_mouse_down(0):  # 16 pixels a mesh: 512 units a pixel
            at = imgui.get_io().mouse_pos - imgui.get_item_rect_min()
            state["world_at"] = [int(at.x) * 512 % 0x48000, int(at.y) * 512 % 0x38000]
        corner = imgui.get_item_rect_min()
        imgui.get_window_draw_list().add_circle(imgui.ImVec2(corner.x + x / 512, corner.y + z / 512), 5, 0xFFFF00FF)
        for kind, label in ((0, "world (WM0)"), (2, "map 2 (WM2)"), (3, "map 3 (WM3)")):
            if imgui.radio_button(label, state["world_kind"] == kind):
                state["world_kind"] = kind
            imgui.same_line()
        imgui.new_line()
        if state["world_kind"] == 0:
            imgui.set_next_item_width(200)
            _, state["world_stage"] = imgui.slider_int("map_stage", state["world_stage"], 0, 4)
        imgui.set_next_item_width(200)
        _, state["world_yaw"] = imgui.slider_int("yaw", state["world_yaw"], 0, 4095)
        imgui.set_next_item_width(200)
        _, state["world_view"] = imgui.slider_int("view (L2/R2)", state["world_view"], 0, 3)
        imgui.set_next_item_width(200)
        _, state["world_leader"] = imgui.slider_int("leader (WMn.BSZ)", state["world_leader"], 0, 2)
        if state["world_kind"] == 0:
            imgui.set_next_item_width(200)
            _, state["world_container"] = imgui.slider_int("container (WMn.TXZ)", state["world_container"], 0, 10)
        ids = sorted(i for i in wm.models if i != state["world_leader"])
        labels = [f"{i} {WORLD_MODELS.get(i, '')}".rstrip() for i in ids]
        for k, e in enumerate(state["world_entities"]):  # models round the point, as the scripts place them
            imgui.push_id(k)
            imgui.set_next_item_width(150)
            _, at = imgui.combo("##id", ids.index(e["id"]) if e["id"] in ids else 0, labels)
            if ids[at] != e["id"]:
                e["id"], e["tint"] = ids[at], WORLD_TINTS.get(ids[at])
            for key, low, high in (("dx", -8000, 8000), ("dz", -8000, 8000), ("facing", 0, 4095), ("frame", 0, 59)):
                imgui.same_line()
                imgui.set_next_item_width(70)
                _, e[key] = imgui.slider_int(f"##{key}", e[key], low, high)
            imgui.same_line()
            if imgui.button("x"):
                state["world_entities"].pop(k)
                imgui.pop_id()
                break
            imgui.pop_id()
        if ids and imgui.button("add a model (x, z from the point, facing, frame)"):
            state["world_entities"].append({"id": ids[0], "dx": 0, "dz": 1500, "facing": 0, "frame": 0,
                                            "tint": WORLD_TINTS.get(ids[0])})
        imgui.text(f"at {x}, {z} (mesh {x >> 13}, {z >> 13}; ground {wm.ground(x, z)})")
        imgui.end_group()

    def gui():
        if state["renderer"] is None:
            state["renderer"], state["ps1view"] = Renderer(moderngl.create_context()), Ps1View()
            state["screen"] = state["renderer"].ctx.texture((320, 240), 3)
            state["screen"].filter = (moderngl.NEAREST, moderngl.NEAREST)
            choose(state["choice"])
            choose_map(start_map or maps[0])
        if imgui.begin_tab_bar("views"):
            if imgui.begin_tab_item("Models")[0]:
                models_gui()
                imgui.end_tab_item()
            flags = imgui.TabItemFlags_.set_selected if start_map and state["frames"] == 0 else 0
            if imgui.begin_tab_item("Maps", None, flags)[0]:
                maps_gui()
                imgui.end_tab_item()
            flags = imgui.TabItemFlags_.set_selected if start_battle is not None and state["frames"] == 0 else 0
            if imgui.begin_tab_item("Battles", None, flags)[0]:
                battles_gui()
                imgui.end_tab_item()
            flags = imgui.TabItemFlags_.set_selected if start_world is not None and state["frames"] == 0 else 0
            if imgui.begin_tab_item("World", None, flags)[0]:
                world_gui()
                imgui.end_tab_item()
            imgui.end_tab_bar()
        state["frames"] += 1
        if frames and state["frames"] >= frames:
            hello_imgui.get_runner_params().app_shall_exit = True

    def models_gui():
        if state["vram"] != "model":
            state["ps1view"].upload(state["model"])
            state["vram"] = "model"
        imgui.begin_child("models", imgui.ImVec2(220, 0))
        _, state["filter"] = imgui.input_text("##filter", state["filter"])
        for name in models:
            if state["filter"].upper() in labels[name].upper() and imgui.selectable(
                    labels[name], name == state["choice"].partition(":")[0] or name == state["choice"])[0]:
                choose(name)
        imgui.end_child()
        imgui.same_line()
        imgui.begin_group()
        m = state["model"]
        anims = [i for i, a in enumerate(m.anims) if a]
        anim = m.anims[state["anim"]] if state["anim"] < len(m.anims) else None
        if anim and state["play"]:
            state["frame"] = (state["frame"] + imgui.get_io().delta_time * 30) % anim["frames"]
        frame = int(state["frame"]) if anim else 0
        posed = m.posed(anim, frame)
        state["renderer"].draw(m.positions(posed), state["yaw"], state["pitch"],
                               state["distance"] * state["zoom"], state["center"], state["ps1"])
        psx = state["ps1view"].draw(m, posed, view(state["yaw"], state["pitch"], state["distance"] * state["zoom"],
                                                   state["center"]))
        state["renderer"].ctx.screen.use()
        imgui.image(imgui.ImTextureRef(state["renderer"].texture.glo), imgui.ImVec2(VIEW, VIEW),
                    imgui.ImVec2(0, 1), imgui.ImVec2(1, 0))
        if imgui.is_item_hovered():
            io = imgui.get_io()
            if imgui.is_mouse_dragging(0):
                state["yaw"] += io.mouse_delta.x * 0.01
                state["pitch"] = max(-1.5, min(1.5, state["pitch"] + io.mouse_delta.y * 0.01))
            state["zoom"] = max(0.1, min(10.0, state["zoom"] * 0.9 ** io.mouse_wheel))
        imgui.same_line()
        imgui.begin_group()
        imgui.text("PS1 view (320x240, the port's GPU)")
        if psx is None:
            imgui.text_wrapped("Build it with: make editor")
        else:
            state["screen"].write(np.ascontiguousarray(psx).tobytes())
            imgui.image(imgui.ImTextureRef(state["screen"].glo), imgui.ImVec2(640, 480))
        imgui.end_group()
        if anims:
            if imgui.begin_combo("animation", f"{state['anim']}"):
                for i in anims:
                    if imgui.selectable(f"{i} ({m.anims[i]['frames']} frames)", i == state["anim"])[0]:
                        state["anim"], state["frame"] = i, 0.0
                imgui.end_combo()
            _, state["play"] = imgui.checkbox("play", state["play"])
            imgui.same_line()
            changed, f = imgui.slider_int("frame", frame, 0, max(0, anim["frames"] - 1) if anim else 0)
            if changed:
                state["frame"], state["play"] = float(f), False
        path, _, weapon = state["choice"].partition(":")
        if path.startswith("ENEMY6/") and not Path(path).stem.startswith("ENEMY"):  # a party member: the weapons
            imgui.set_next_item_width(120)
            changed, w = imgui.input_int("weapon", int(weapon or 0))
            if changed and 0 <= w < 16:
                choose(f"{path}:{w}" if w else path)
        _, state["ps1"] = imgui.checkbox("PS1 look (15-bit colour, dither, unwarped textures)", state["ps1"])
        imgui.text(f"{len(m.bones)} bones, {len(m.parts)} parts, {m.count // 3} triangles")
        imgui.end_group()

    params = hello_imgui.RunnerParams()
    params.app_window_params.window_title = "FF7 field viewer"
    params.app_window_params.window_geometry.size = (VIEW + 920, VIEW + 140)
    params.ini_folder_type = hello_imgui.IniFolderType.app_user_config_folder  # the window's layout, not in the repo
    params.callbacks.show_gui = gui
    immapp.run(params)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("assets", type=Path, help="the folder holding the extracted discs")
    ap.add_argument("--model", help="NAME.BCX, MAP.BSX:N or ENEMYn/NAME.LZS to start on (with --shot, the one to "
                                    "draw)")
    ap.add_argument("--anim", type=int, default=0)
    ap.add_argument("--frame", type=int, default=0, help="the animation's frame, or with --battle the battle's")
    ap.add_argument("--yaw", type=float, default=0.6)
    ap.add_argument("--map", help="a map (MD1STIN) to start on, or with --shot to draw")
    ap.add_argument("--story", type=int, default=0, help="with --map --shot, the story point the map starts at")
    ap.add_argument("--battle", type=lambda s: int(s, 0), help="a battle (0x12C) to start on, or with --shot to draw")
    ap.add_argument("--camera", type=int, default=0,
                    help="with --battle --shot, the setup's camera (0-3), or -1 its opening at battle frame --frame")
    ap.add_argument("--party", default="CLOUD", help="with --battle --shot, the party's ENEMY6 files (CLOUD,TIFA)")
    ap.add_argument("--action", help="with --battle --shot, an action from the start: attacker,script,target"
                    "[,attack camera[,variant]] (0,20,4: Cloud's Attack), drawn at its battle frame --frame")
    ap.add_argument("--world", type=int, help="a world map (0, 2, 3) to start on, or with --shot to draw")
    ap.add_argument("--at", default="185636,123325", help="with --world --shot, the point (x,z) to draw round")
    ap.add_argument("--view", type=int, default=0, help="with --world --shot, the camera's view mode (0-3)")
    ap.add_argument("--container", type=int, default=0, help="with --world 0 --shot, WMn.TXZ (a stage of the story)")
    ap.add_argument("--models", default="", help="with --world --shot, models id:x:z:facing from the point, by commas")
    ap.add_argument("--shot", type=Path, help="draw the model (or the map, battle or world) to this PNG and quit")
    args = ap.parse_args()
    disc = next(p for p in sorted(args.assets.iterdir()) if (p / "FIELD/FIELD.BIN").exists())
    if args.shot and args.world is not None:
        world_shot(disc, args.world, tuple(int(n) for n in args.at.split(",")), int(args.yaw), args.view, args.shot,
                   args.container, [tuple(int(v) for v in m.split(":")) for m in args.models.split(",") if m])
    elif args.shot and args.battle is not None:
        battle_shot(disc, args.battle, args.camera, args.frame, tuple(args.party.upper().split(",")), args.shot,
                    [int(v, 0) for v in args.action.split(",")] if args.action else None)
    elif args.shot and args.map:
        map_shot(disc, args.map.upper(), args.shot, args.story)
    elif args.shot:
        shot(disc, args.model or "CLOUD.BCX", args.anim, args.frame, args.shot, args.yaw)
    else:
        run(disc, args.model, args.map.upper() if args.map else None, start_battle=args.battle,
            start_world=args.world)


if __name__ == "__main__":
    main()
