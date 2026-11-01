"""The field model editor (so far a viewer): open the party's and each map's models, turn them about, play their
animations, shaded and textured as the game draws them.

  pip install -r tools/requirements-editor.txt
  python tools/model_editor.py assets
  python tools/model_editor.py assets --model MD1STIN.BSX:0 --anim 3 --frame 12 --shot cloud.png

A character on a map (MD1STIN.BSX:0) has that map's lights, textures and animations added to its own; on its own
(CLOUD.BCX) it shows its colours unlit and untextured. Drag to turn the view, the wheel to zoom.
"""
import argparse
import ctypes
import struct
import zlib
from pathlib import Path

import moderngl
import numpy as np

import field
import model
from unpack import lzs

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


def vram(disc: Path, bsx: dict | None, face: tuple[int, int, int] = (1, 1, 1)) -> np.ndarray:
    """VRAM as the field sets it up for a map's models (func_800AC35C): the .BSX's images and patches, then per model
    (its slot its place in the list) its face's FIELD.TDB palette and three images, the given entries of its lists.
    The field loads entry 0, the same blank (0x7E) for every face, and scripts swap in the others."""
    out = np.zeros((512, 1024), np.uint16)
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
                code, attributes = PACKETS[poly["kind"]], []
                if "uv" in poly:
                    page, depth, shift = texture(part["textures"][poly["flags"] & 15], slot)
                    texels = [[a + b for a, b in zip(part["uvs"][i], shift)] for i in poly["uv"][:n]]
                    uv = [[c + 0.5 for c in t] for t in texels]
                    blend = poly["flags"] >> 5 & 3
                    code += 2 if poly["flags"] & 0x10 else 0  # semi-transparent
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
    """NAME.BCX, or MAP.BSX:N for a map's model N."""
    normals = [struct.unpack_from("<3h", field.module(disc), NORMALS + 8 * i) for i in range(240)]
    path, _, index = name.partition(":")
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
    return out


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
        packets = []
        for part, corners, code, shades, attributes in m.polygons:
            xy, d = screen[part][corners], depth[part][corners]
            area = (xy[1, 0] - xy[0, 0]) * (xy[2, 1] - xy[0, 1]) - (xy[2, 0] - xy[0, 0]) * (xy[1, 1] - xy[0, 1])
            if d.min() <= 1 or area <= 0:
                continue
            words = []
            for i, (x, y) in enumerate(xy):
                if i < len(shades) and (i == 0 or code & 0x10):
                    words.append(shades[i] | (code << 24 if i == 0 else 0))
                words.append((int(y) & 0x7FF) << 16 | int(x) & 0x7FF)
                if attributes:
                    words.append(attributes[i])
            packets.append((int(d.mean()) >> 2, len(packets), words))
        # The ordering table: buckets by depth, far first; in a bucket the packet added last is drawn first (it's
        # put at the head of the bucket's list), so a part's Gouraud polygons go under its textured ones.
        packets.sort(key=lambda p: (-p[0], -p[1]))
        words = [0xE1000200, 0xE3000000, 0xE4000000 | 239 << 10 | 319, 0xE5000000, 0xE6000000,
                 0x02241F1F, 0, 240 << 16 | 320] + [w for _, _, ws in packets for w in ws]
        self.gpu.gpu_gp0_words((ctypes.c_uint32 * len(words))(*words), len(words))
        c = self.vram[:240, :320].astype(np.uint32)
        return np.stack([(c & 31) << 3, (c >> 5 & 31) << 3, (c >> 10 & 31) << 3], 2).astype(np.uint8)


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


def run(disc: Path, start: str | None, frames: int | None = None):
    """The window; frames, if given, closes it after that many (to check it runs)."""
    from imgui_bundle import hello_imgui, imgui, immapp

    models = names(disc)
    state = {"choice": start or models[0], "frames": 0, "anim": 0, "frame": 0.0, "play": True, "yaw": 0.6,
             "pitch": 0.2, "zoom": 1.0, "ps1": True, "renderer": None, "model": None, "center": None,
             "distance": 1.0, "filter": ""}

    def choose(name: str):
        state["choice"], state["model"] = name, load(disc, name)
        state["renderer"].upload(state["model"])
        state["ps1view"].upload(state["model"])
        state["center"], state["distance"] = frame_view(state["model"])
        state["anim"], state["frame"] = next((i for i, a in enumerate(state["model"].anims) if a), 0), 0.0

    def gui():
        if state["renderer"] is None:
            state["renderer"], state["ps1view"] = Renderer(moderngl.create_context()), Ps1View()
            state["screen"] = state["renderer"].ctx.texture((320, 240), 3)
            state["screen"].filter = (moderngl.NEAREST, moderngl.NEAREST)
            choose(state["choice"])
        imgui.begin_child("models", imgui.ImVec2(220, 0))
        _, state["filter"] = imgui.input_text("##filter", state["filter"])
        for name in models:
            if state["filter"].upper() in name.upper() and imgui.selectable(name, name == state["choice"])[0]:
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
        _, state["ps1"] = imgui.checkbox("PS1 look (15-bit colour, dither, unwarped textures)", state["ps1"])
        imgui.text(f"{len(m.bones)} bones, {len(m.parts)} parts, {m.count // 3} triangles")
        imgui.end_group()
        state["frames"] += 1
        if frames and state["frames"] >= frames:
            hello_imgui.get_runner_params().app_shall_exit = True

    params = hello_imgui.RunnerParams()
    params.app_window_params.window_title = "FF7 field models"
    params.app_window_params.window_geometry.size = (VIEW + 920, VIEW + 140)
    params.ini_folder_type = hello_imgui.IniFolderType.app_user_config_folder  # the window's layout, not in the repo
    params.callbacks.show_gui = gui
    immapp.run(params)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("assets", type=Path, help="the folder holding the extracted discs")
    ap.add_argument("--model", help="NAME.BCX or MAP.BSX:N to start on (with --shot, the one to draw)")
    ap.add_argument("--anim", type=int, default=0)
    ap.add_argument("--frame", type=int, default=0)
    ap.add_argument("--yaw", type=float, default=0.6)
    ap.add_argument("--shot", type=Path, help="draw the model to this PNG and quit")
    args = ap.parse_args()
    disc = next(p for p in sorted(args.assets.iterdir()) if (p / "FIELD/FIELD.BIN").exists())
    if args.shot:
        shot(disc, args.model or "CLOUD.BCX", args.anim, args.frame, args.shot, args.yaw)
    else:
        run(disc, args.model)


if __name__ == "__main__":
    main()
