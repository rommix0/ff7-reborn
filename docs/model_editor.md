# Field model editor (draft)

Goal: one self-contained app to open the field's 3D models (`FIELD/*.BCX` characters, each map's `.BSX`), change
their shape, colours, textures and animations, see them exactly as the PS1 draws them, and save them back into the
user's `assets/`. Like the other tools it reads the user's own discs, and an unedited model saves back byte for byte.

The format is decoded (`docs/knowledge_base.md`, `FIELD/*.BCX` and `.BSX`); every real model file on disc 1 decodes to
its last byte.

## What a model holds

- **Where models come from:**
  - **Characters:** the nine party characters are `.BCX` files (Cloud, Aeris, Barret, Tifa, Red XIII, Cid, Yuffie,
    Cait Sith, Vincent), with their skeleton, parts and first animations.
  - **Maps:** each map's `.BSX` holds its own models (everyone else) and adds animations to the characters it uses.
  - **The list:** the map's `.DAT` (section 6) lists which models it has.
- **Skeleton:** bones with a length and a parent. A part hangs on a bone, its vertices in the bone's own space.
- **Vertices:** `s16` x, y, z.
- **Polygons, the GPU's own eight kinds:**
  - Gouraud (a colour per vertex): triangles and quads, textured or not.
  - Flat (one colour): triangles and quads, textured or not.

  Each has a flag byte too: which texture word, semi-transparent or not, its blend mode.
- **Shading:**
  - **What's stored:** each colour is an RGB plus the index of a normal, out of a fixed table of 240 directions in
    `FIELD.BIN`.
  - **When it's lit:** once, as the map loads, the GTE lights every colour (`nccs`) from that normal. It uses the
    model's three lights (a colour and a direction each) and an ambient colour, all of which the map's `.BSX` gives
    per model.
  - **So:** the Gouraud picture is painted base colours, lit by the map's lights.
- **Textures:**
  - **UVs:** a table per part, which each polygon indexes.
  - **Texture words:** colour depth, texture page, palette row; some words place the texture in a per-model slot of
    VRAM.
  - **Images:** the `.BSX` carries its images as raw VRAM rectangles, palettes among them.
  - **`FIELD.TDB`:** 127 small 32×32 16-colour images and 34 palettes. Each model with a face id below 33 gets three
    of them in its slot (two side by side, one apart), replaceable at run time: swappable textures such as eyes and
    mouth, though the code doesn't name them.
- **Animations:**
  - per bone, three rotation angles (a byte each, 256 to a turn, applied as Ry·Rx·Rz)
  - each angle either fixed or a channel with a value per frame
  - optional offsets the same way
  - the scripts pick one by number (`anime`, `canim`, `dfanm` ...)
- **Not in the files:** shadows; the field draws them some other way.

## Two parts

### `tools/model.py`: the format, no UI

- **Decoding and encoding:** every model and animation, with nothing but the standard library. The editor and the
  command line both go through it.
- **`dump`/`build`:** to and from JSON, as `scene.py`, `field.py` and `world.py` do: exact edits and scripted
  changes, and the byte-for-byte test over every model on all three discs.
- **The limits, checked on every save, which says what doesn't fit and changes nothing:**
  - vertices and polygons per part
  - UVs within their texture page
  - texels within the palette
  - the map's models within their RAM area (the `.BSX` unpacks at `0x801B0000`)
  - each file within its room on the disc (`kernel.replace`)

### `tools/model_editor.py`: the app

`python tools/model_editor.py assets` opens a window built with Dear ImGui (`imgui-bundle`), with its 3D view drawn
by `moderngl`. Panels can be docked or moved:

- **Models:** the characters and each map's models, from the disc folders; open one, or several to compare.
- **3D view:**
  - orbit, pan, zoom; parts, bones and polygons shown or hidden
  - select vertices, polygons, parts or bones by clicking or boxing
  - move and rotate with on-screen handles (ImGuizmo, part of `imgui-bundle`), snapped to the game's units
- **Inspector:** the selection's fields as the game stores them (integers, flags, polygon type), editable.
- **Colours and light:**
  - **Brush:** sets vertex base colours, with a picker in the PS1's terms (0–255 per channel; `0x80` is neutral on
    a textured polygon), and flood fill per part.
  - **Normals:** pick each colour's normal from the game's 240 directions, or let the editor choose the nearest to
    the surface.
  - **Lights:** the map's three lights and ambient for this model, edited and seen at once.

  The view shows the colours lit, as the GTE lights them at load.
- **Texture page:** the page and palette the selection uses, with its UVs drawn over it:
  - drag UVs, snapped to whole texels
  - pick another palette
  - edit palette entries
  - replace the image from a PNG, cut down to 16 or 256 colours
  - the `FIELD.TDB` images for its swappable slots
- **Animation:**
  - the model's animations (a character's own from its `.BCX`, and each map's additions), a timeline per bone
  - play and scrub
  - pose bones with the rotate handle and set the frame
  - frames added or dropped, at the game's angle steps
- **PS1 view:** the same model drawn by the port's own software GPU and GTE (`pc/gpu.c`, `pc/gte.c` built as a
  small shared library, called through `ctypes`):
  - the GTE's rounding and vertex snapping
  - Gouraud blending in 15-bit colour with dithering
  - warping (affine) textures
  - semi-transparency
  - the ordering table's draw order

  The primitives are built the way the field's drawing code builds them (`func_800AE4DC`). Without
  the library (not built yet), this panel says so and the rest works. The packets are the game's: GT4 `3C`, GT3
  `34`, FT4 `2C`, FT3 `24`, F3 `20`, F4 `28`, G3 `30`, G4 `38` (`+2` when semi-transparent), with back faces
  culled by `nclip` and depth from the vertices' average.
- **Undo/redo:** every edit is a command on one stack.
- **Save:** goes through `model.py`'s encoder and limits into every disc folder, keeping `.orig` copies, as the other
  tools do.

**The 3D view's own shading:** shaders close to the PS1's look, so editing feels like the game:
- colours blended per vertex
- textures multiplied by colour / 128
- nearest texels, optional 15-bit colour and dither

The PS1 view is the exact one.

**Keeping it light:**
- The editor keeps what it shows in one place.
- It rebuilds its vertex buffers only when an edit changes them, never per frame.

## Goals, non-goals, anti-goals

- **Goals:**
  - every field model and animation viewable and editable in one app
  - exact saves
  - limits checked before anything is written
  - the PS1's own picture beside the editing view
- **Non-goals for now:**
  - new rendering features for the PC port (higher-resolution textures, real lighting): a port enhancement, later
  - battle and world map models: other formats; the field's first
  - modelling from scratch with sculpting or other heavy tools. glTF export/import can come later for that, out to
    Blender and back, but the editor doesn't depend on it.
- **Anti-goals:**
  - a save that changes anything the user didn't touch
  - game data in the repo
  - a format description copied from anyone else's notes (decoded from the code, as the rest is)

## Dependencies and constraints

- **`model.py`:** Python 3.10+, standard library only, MIT.
- **The editor:** `imgui-bundle` (MIT) and `moderngl` (MIT), with `numpy`, in a `tools/requirements-editor.txt` like
  the music player's; the other tools don't need them.
- **The PS1 view:** the host C compiler, building the shared library from `pc/` (a `make` target).
- **Checking:** every format claim is checked against all three discs and in the lab, as before.

## Milestones

1. **Formats:** decoded; written up in the knowledge base with `model.py`.
2. **`model.py dump`/`build`:** done. Byte for byte on all 721 model files; Cloud's shirt turned red showed in
   `MD1STIN`.
3. **Viewer:** done (`tools/model_editor.py`).
   - **What it does:** every party and map model, orbiting, animations, colours lit as the field lights them at
     load, textures from the map's images and the `FIELD.TDB` faces.
   - **The PS1 view:** `pc/gpu.c` drawing the field's own packets, back faces dropped and in the ordering table's
     order.
   - **`--shot`:** draws both views to a PNG without a window.
   - **Not exact:**
     - posing is in floating point, not the game's fixed-point GTE code
     - the editing view draws semi-transparent polygons opaque (the PS1 view blends them)
     - faces show each list's second image (the field loads a blank first)
4. **Editing:** vertex colours, vertices, UVs, bone poses and frames, undo, save. Lab check: an edit made in the
   editor shows in the game.
   - **Maps** (a tab of their own): each map's background drawn by `pc/gpu.c` as the field links its tiles, the
     walkmesh, exits, triggers, arrows and markers projected over it with the map's camera, and its characters
     where its start-up code puts them at a chosen point of the story, drawn into the same ordering table.
   - **Battle models** (in the Models tab): every party member (with any of their 16 weapons) and enemy, posed by
     the battle's animations; their colours come lit in the files.
   - **Battles** (a tab of their own): each battle's stage, party and enemies, drawn by `pc/gpu.c` in the battle's
     lists and order, played at 15 frames a second from the opening camera (or a setup camera), with shadows,
     idle animations and moving stages.
5. **Textures and palettes:** image replacement, palette editing; then glTF export/import if wanted.

## Open

- What you'd want to do first (recolour, reshape, new animations), which sets the order inside milestone 4.
- Whether a character's edit should apply everywhere (its `.BCX`) or to one map's copy; the files allow both.
- Which light direction pairs with which light colour (not traced yet).
- How much RAM a map's models may use (where the free pointer starts and what else shares it).
