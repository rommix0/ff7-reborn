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
rest of your `SCUS_941.63` and `FIELD/ENDING.X` recompiled to C at build time (never committed). It shows the first
screen so far. It needs [winlibs](https://winlibs.com/)' 32-bit MinGW GCC at `C:\winlibs\mingw32` (or set `PC_CC`
and `PC_NM`) and your three extracted discs in `assets/`:

```bash
sh tools/setup_sdl2.sh   # SDL2 2.30.12 (zlib licence) into toolchain/SDL2, checked against its pinned SHA-256
make split && make pc    # build/pc/ff7.exe, with SDL2.dll beside it
make pc-test             # the recompiler, the emulated hardware, and the CD layer reading every file on all discs
```

Run `build/pc/ff7.exe assets` from the project folder. Keys: arrows, Z/X/A/S (×, ○, □, △), Enter (Start), Backspace
(Select), F12 saves `screenshot.bmp`, Esc quits. `--frames N --shot FILE.bmp` runs without a window and saves frame N.

**Sony library names:** `config/symbols.main.psyq.txt` was generated by `tools/psyq_match.py`. It holds names
and addresses only. The sources were PsyQ 3.5 and 4.4/4.5 libraries plus Net Yaroze's resident library; see
`docs/knowledge_base.md` §8. To regenerate it, list the closest library version first:
`python tools/psyq_match.py <SCUS_941.63> <lib35>/*.LIB <sdk44>/*.LIB --ref-exe LIBPS.EXE libps_nm.txt`.

**Recovering a function:** add a `c` subsegment for it in `config/main.yaml`, write the C in `src/`, and add its
name to `config/symbols.main.txt`. Then `make split && make`.

## Layout

- `src/`: recovered C source, one folder per executable or overlay
- `native/`: what the PC build of the sound driver adds (libspu stubs, loader, song runner)
- `pc/`: the PC build's runtime (memory map, BIOS, hardware, MDEC, software GPU, GTE, CD, window)
- `config/`: the boot exe's splat config (`main.yaml`) and symbol names; the PC port's modules get theirs from
  `tools/module_yaml.py`
- `include/`: headers and the assembler macros splat expects
- `tools/`: extractor scripts and the build/match tooling
- `docs/knowledge_base.md`: what we know about the discs, overlays, sound engine and what is left to recover

## License

MIT (see `LICENSE`) for the code and docs in this repo. `include/include_asm.h` and `include/*.inc` are generated by splat (MIT). Final Fantasy VII and all its data belong to Square Enix and are not covered.
