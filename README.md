# FF7 Reborn (WIP)

Code recovery for **Final Fantasy VII (PS1, NTSC-U, SCUS-94163/4/5)**. The original source code is lost; this repo rebuilds it from the retail discs, plus scripts so owners of a legit copy can extract the game data themselves.

This project was made possible with the assistance of Claude.

I have nothing against people using agentic coders, so hand-made and agent assisted contributions are welcome. Just make sure the code works before you do a pull request.

No game data is in this repo. `assets/` and any disc images are git-ignored. Bring your own discs.

## Extract your discs

Needs Python 3.10+, no other dependencies.

```bash
python tools/extract_disc.py "Final Fantasy VII (USA) (Disc 1).bin"
```

Files land in `assets/<image name>/`, with `files.txt` listing where each one sits on the disc (the PC build reads by
sector, as the game does). Add `-l` to list files with LBA, size and SHA1 instead. Your disc 1
`SCUS_941.63` should hash to `a95e8b16b97071203b953bb81a33980509262f30`.

Then decompress what you need (overlays, `KERNEL.BIN`, field maps). The format is detected per file:

```bash
python tools/unpack.py "assets/Final Fantasy VII (USA) (Disc 1)/BATTLE/BATTLE.X"
```

Mod the game's data: `INIT/KERNEL.BIN` holds the item, weapon, armor, accessory, materia, spell and command tables
and their names, help texts and battle messages. Dump it to files you can edit (the tables as JSON, the text one line
per string), then build it back into every disc's folder for the PC build:

```bash
python tools/kernel.py dump "assets/Final Fantasy VII (USA) (Disc 1)" kernel
python tools/kernel.py build kernel assets
```

`BATTLE/SCENE.BIN` holds the 256 battle scenes: the enemies (names, stats, HP, MP, EXP, gil,
AP, drops, steals, morphs, elements), their attacks and AI, and each battle's setup (area, layout, escape, cameras,
and which enemy stands where).
It works the same way, a JSON file per scene; the AI scripts come as a list of lines, one operation each (`push8 0x1`,
`jump_if_false L0029`, `attack`, `text ...`), and can be rewritten freely within the scene's room:

```bash
python tools/scene.py dump "assets/Final Fantasy VII (USA) (Disc 1)" scene
python tools/scene.py build scene assets
```

`build` keeps the first originals as `.orig` (copy them back to undo) and refuses data that won't fit in the game's
memory or on the disc; the scene build remakes `KERNEL.BIN` too, which knows where each scene is. Fields not worked
out yet are `unknown_XX`, and their values can still be changed; `docs/knowledge_base.md` (Text and `KERNEL.BIN`,
`BATTLE/SCENE.BIN`) says what each named one does and how the text's `{XX}` codes work.

The field maps (`FIELD/*.DAT`) hold the story: each map's scripts and dialogue. They dump to a JSON file per map:
its entities, their scripts a line per operation (named as the game's own debug trace names them, values as
`#12` or a variable `var[bank][address]`, or its name where the game's code shows it (`story`, `party1`,
`menu_locked`, ...), other operands as hex bytes, jumps to labels, e.g.
`if2 story == #0 L002D`, `batle #300`, `mes 03 1E ; ...` with the string shown) and its
dialogue. The rest of the map follows: its walkmesh (triangles and their neighbours), background tiles (four
layers, a line per tile), camera, exits and triggers (`"exits"`: a line on the floor and the map, position, triangle
and facing it leads to), random encounters (two tables of six battles and four special ones, each with its chance
out of 64, and a rate: bigger is rarer) and the models it loads. `tools/field.py`'s header lists each tile's numbers.
A section taken out of a file stays as it was. `build` writes only the maps that differ from the originals (into every
disc folder, keeping `.orig` copies) and moves `FIELD.BIN`'s table of where each map lies to match. A longer line
may need its window made wider: the `wsize` before its `mes`.

```bash
python tools/field.py dump "assets/Final Fantasy VII (USA) (Disc 1)" field
python tools/field.py build field assets
```

The field's 3D models (`FIELD/*.BCX`, the party, and each map's `.BSX`): bones, parts (vertices, polygons with their
colours, UVs and texture words), animations, and per map each model's lights and textures, a JSON file per model file.
A colour is `RRGGBBNN`, NN the normal the map's lights shine on (the GTE lights each colour once as the map loads):

```bash
python tools/model.py dump "assets/Final Fantasy VII (USA) (Disc 1)" models
python tools/model.py build models assets
```

View them with `tools/model_editor.py` (the start of the editor `docs/model_editor.md` plans; it needs
imgui-bundle, moderngl and numpy): pick a party member, any map's model or any battle model (the party with each of
their 16 weapons, every enemy by name), turn it about, play its animations. Beside
the view, a PS1 view draws it with the PC port's own GPU (`make editor` builds that), the way the game sends it. Its
Maps tab draws a map as it starts at a point of the story (`--story`): its background and the characters its
start-up scripts place, with that GPU, the walkmesh, exits (with where they lead), triggers, arrows and markers
over it, layers and tile groups (doors, lights, animations) to turn on and off:

```bash
pip install -r tools/requirements-editor.txt
make editor
python tools/model_editor.py assets
python tools/model_editor.py assets --model MD1STIN.BSX:0 --anim 3 --frame 12 --shot cloud.png
python tools/model_editor.py assets --model ENEMY6/CLOUD.LZS:3 --shot battle_cloud.png
python tools/model_editor.py assets --map MD1STIN --story 1 --shot md1stin.png
```

The shops (`MENU/SHOPMENU.MNU`): what each of the 80 sells (by inventory number or materia, names shown), the price
of every item and materia, the shops' titles and the clerks' lines, in one JSON file:

```bash
python tools/shop.py dump "assets/Final Fantasy VII (USA) (Disc 1)" shops.json
python tools/shop.py build shops.json assets
```

The world map's event scripts (inside `WORLD/WM*.TXZ`): what happens where on the world map, the scripted battles
among it. A JSON file per set (the main world, `WMB`, `WMC`) lists the scripts by key and the code a line per
operation, as `scene.py`'s AI does (`push story`, `push #1000`, `ge`, `ifnot L0026`, `push #324`, `battle`,
`push #49`, `message`, `push byte.player_id`, `select_entity`; commands not worked out yet are `op_3XX`):

```bash
python tools/world.py dump "assets/Final Fantasy VII (USA) (Disc 1)" world
python tools/world.py build world assets
```

The containers are packed back to back, so an edit can grow a set's packed size by only a few dozen bytes.

Disassemble the music and sound effects (AKAO sequences):

```bash
python tools/akao.py dump "assets/Final Fantasy VII (USA) (Disc 1)/MINI/SENSUI.SND"
python tools/akao.py dump "assets/Final Fantasy VII (USA) (Disc 1)/SOUND/EFFECT.ALL" 5
python tools/akao.py check assets
python tools/akao.py midi assets assets/midi
```

`check` parses every song and sound effect it finds and reports anything that doesn't parse (~3 minutes for three
discs). `midi` writes each distinct song as `song_<id>.mid`, with tempo and `loopStart`/`loopEnd` markers.

Export the songs in our own format and play and edit them in the music player, which emulates the game's sound
driver (time signatures, slurs, vibrato, tremolo, bends, envelopes, reverb and loops as the game plays them). The songs hold no samples; they
share one instrument bank file. The format is described in `docs/ff7song_format.md`. The player needs numpy and
sounddevice; the window is tkinter.

```bash
python tools/ff7song.py assets "assets/Final Fantasy VII (USA) (Disc 1)/SOUND" assets/ff7song
pip install -r tools/requirements-player.txt
python tools/ff7player.py assets/ff7song
```

In the player:

- **Playback:** Space plays and pauses. **Tempo** (25–400%) and **Transpose** (±24 semitones) change playback only,
  not the song; **Reset** puts them back. **Reverb** switches the SPU reverb on and off. Double-click a bar in the order list to jump there; click a channel
  heading to mute it.
- **Cells:** click a cell to select it, double-click to edit its note and effects, and press Delete to clear it.
- **Bars:** the buttons under the order list insert, duplicate or delete bars, change a time signature, and move
  the loop. Bars that share a pattern (shown as `×2` and so on) change together until **Own copy** splits one off.
- **Saving:** Ctrl+S writes the `.ff7song` back.

The export also writes the game's 721 sound effects to `assets/ff7song/effects`. Open them in the same player:

```bash
python tools/ff7player.py assets/ff7song/effects
```

Record the game's nine SPU reverb types as impulse-response WAVs for a convolution reverb (24-bit stereo, 44.1 kHz;
a centred one per type, plus left-in and right-in files for true stereo; needs numpy):

```bash
python tools/reverb_ir.py "assets/Final Fantasy VII (USA) (Disc 1)/SCUS_941.63"
```

Convert the instrument samples to WAV (with loop points) and a CSV of their settings:

```bash
python tools/instruments.py "assets/Final Fantasy VII (USA) (Disc 1)/SOUND"
```

Rewrap the movies for the PC port: an AVI per movie (MJPEG video, PCM audio), plus a `.cam` file with each
frame's camera data for the streamed ones, in `assets/movies`. The video keeps the PS1's own DCT coefficients, so nothing is re-encoded:

```bash
python tools/movie.py assets
```

## Build toolchain (Linux or WSL)

```bash
sudo apt install binutils-mipsel-linux-gnu python3 python3-venv curl git make
sh tools/setup_toolchain.sh
make split   # splat: cut your SCUS_941.63 into assembly (asm/, git-ignored)
make         # rebuild the exe from asm + recovered C and check the SHA1
```

`make` ends with `build/SCUS_941.63: OK` when the rebuild is byte-identical. To test one C file on its own
against the original, run `bash tools/match.sh src/main/bitops.c 80014A00`.

**The sound driver on the PC:** `make native` builds the same C with the host GCC into `build/native/snd_native`,
which loads its data from your `SCUS_941.63`. `python tools/native_check.py <disc folder> <field file or folder>`
plays each song on it and checks every channel's notes against `tools/akao.py` (`gcc` is the only extra package).

**The PC build (work in progress):** the whole boot exe runs as a 32-bit Windows program: the recovered C, plus the
rest of your `SCUS_941.63` and the game's modules (field, battle, world map, menus, spell effects, ...) recompiled to C
at build time (never committed). So far it boots through the title menu into NEW GAME's opening movie and the first
field, with sound, and runs the menus, saving, battles, the world map and the mini-games. It needs
[winlibs](https://winlibs.com/)' 32-bit MinGW GCC at `C:\winlibs\mingw32` (or set `PC_CC` and `PC_NM`) and your three
extracted discs in `assets/`:

```bash
sh tools/setup_sdl2.sh   # SDL2 2.30.12 (zlib licence) into toolchain/SDL2, checked against its pinned SHA-256
make split && make pc    # build/pc/ff7.exe, with SDL2.dll beside it
make pc-test             # the recompiler, the emulated hardware, and the CD layer reading every file on all discs
```

Run `build/pc/ff7.exe assets` from the project folder. Keys: arrows, Z/X/A/S (×, ○, □, △), Q/W (L1, R1), 1/2 (L2,
R2), Enter (Start), Backspace (Select), F12 saves `screenshot.bmp`, Esc quits; a game controller works too.
`--frames N --shot FILE.bmp` runs without a window and saves frame N.

**Saves** go to memory card images in the working directory, `memcard1.mcd` (slot 1) and `memcard2.mcd` (slot 2):
raw 128 KB cards, the format PS1 emulators use, so saves move between them. A missing image is an empty formatted
card.

**Reversing lab** (`pc/lab.c`): `--trace FILE` logs what the game does frame by frame (its mode, the field, what the
field module asks for, the pad, and each file a CD read starts); `--input FILE` plays the pad from a script (lines of
a frame and the buttons held from it); `--field N` warps to field N once the first field is up (`0x41` is the debug
room); `--minigame N` then starts mini-game N (0 bike, 1 chocobo race, 2 snowboard, 3 Fort Condor, 4 submarine,
5 Speed Square, 6 Gold Saucer snowboard); `--code "80AAAAAA VVVV"` holds a GameShark code every frame (16-bit `80`, 8-bit `30`; repeatable);
`--dump-at N` or F11 dumps RAM, VRAM and SPU RAM to `dump_*.bin`; `--fast` runs frames as fast as the PC can
(about 7× real time without a window). The build is deterministic, so
`python tools/lab_diff.py A_ram.bin B_ram.bin --noise A2_ram.bin` shows what two scripts did differently (A2 is a
second run of A's script).

**Sony library names:** `config/symbols.main.psyq.txt` was generated by `tools/psyq_match.py`. It holds names
and addresses only. The sources were PsyQ 3.5 and 4.4/4.5 libraries plus Net Yaroze's resident library; see
`docs/knowledge_base.md` §8. To regenerate it, list the closest library version first:
`python tools/psyq_match.py <SCUS_941.63> <lib35>/*.LIB <sdk44>/*.LIB --ref-exe LIBPS.EXE libps_nm.txt`.

**Recovering a function:** add a `c` subsegment for it in `config/main.yaml`, write the C in `src/`, and add its
name to `config/symbols.main.txt`. Then `make split && make`.

## Layout

- `src/`: recovered C source, one folder per executable or overlay
- `native/`: what the PC build of the sound driver adds (libspu stubs, loader, song runner)
- `pc/`: the PC build's runtime (memory map, BIOS, hardware, MDEC, SPU, software GPU, GTE, CD, window and sound)
- `config/`: the boot exe's splat config (`main.yaml`) and symbol names; the PC port's modules get theirs from
  `tools/module_yaml.py`
- `include/`: headers and the assembler macros splat expects
- `tools/`: extractor scripts and the build/match tooling
- `docs/knowledge_base.md`: what we know about the discs, overlays, sound engine and what is left to recover

## License

MIT (see `LICENSE`) for the code and docs in this repo. `include/include_asm.h` and `include/*.inc` are generated by splat (MIT). Final Fantasy VII and all its data belong to Square Enix and are not covered.
