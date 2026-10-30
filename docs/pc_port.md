# PC port strategy

Goal: FF7 playable on the PC from the user's own extracted discs, with no disc swapping, while the PS1 build stays
byte-identical. Decompiling every module to matching C first would take years before anything plays, so the game runs
first and decompilation keeps replacing parts of it.

## What's left to run

The main exe is mostly C (boot, sound driver, CD layer, movie player). Outside it, nothing is recovered:

| Module | Unpacked |
|---|---|
| Field (`FIELD/FIELD.BIN`) | 258 KiB |
| Battle (`BATTLE/BATTLE.X`, `BATINI.X`, `BATRES.X`) | 350 KiB |
| Title (`BATTLE/TITLE.BIN`), ending (`FIELD/ENDING.X`) | 201 KiB |
| World map (`WORLD/WORLD.BIN`) | 160 KiB |
| Menus (`MENU/*.MNU`, 13) | 473 KiB |
| Mini-games (`MINI/*.BIN`, 22) and magic effects (`MAGIC/*.BIN`, 289) | ~47 MB, mostly graphics |

## The plan

1. **One memory model for every kind of code.** The PC build is a 32-bit program with the PS1's 2 MB of RAM mapped
   at `0x80000000` and the scratchpad at `0x1F800000` (checked on Windows with MinGW, large-address-aware, and on
   Linux with `gcc -m32`). A game address is then a real pointer: the matching C compiles unchanged, literal
   addresses like `(void *)0x800E0000` work, structs keep their PS1 layout, and code from the other two kinds below
   shares data with it. This replaces the current native builds' `snd_ram` translation.

2. **Static recompilation for everything still in assembly.** A tool (`tools/recomp.py`, on rabbitizer, already a
   dependency) turns each game function into a C function over a register context: delay slots folded, loads and
   stores straight to the mapped RAM, `jal` to known addresses as direct calls, jump tables from splat, and other
   `jr`/`jalr` through an address table. Overlays share load addresses (the field and the disc-change screen both run at `0x800A0000`), so
   the table is per module, and the game's module loader switches it. Overlays get splat configs like the exe's.
   - The output is a translation of the game's code, so it's generated at build time from the user's files and
     git-ignored, like `assets/`.
   - **Decompiled C replaces recompiled code one function at a time.** Thunks generated from the C prototypes bridge
     the two calling styles. Decompilation stays matching, and is prioritised by what the port or modding needs, not
     by what's needed to play.

3. **Hardware where the PS1's behaviour is the point, libraries where the PC should differ.**
   - Emulated, so Sony's library code runs as compiled: the **GPU** (GP0/GP1 commands and DMA into 1024×512 VRAM),
     the **GTE** (COP2, for libgte and inline use alike), the **MDEC** (done: the in-memory movies run through libpress
     unchanged), the **SPU** (24 ADPCM voices, ADSR, noise, reverb), root counters and the VSync interrupt. Behaviour
     comes from psx-spx; no emulator code is copied (most are GPL).
   - Replaced at the library: **libcd** (done: extracted files, the save's disc always inserted), **libcd's streaming**
     (the STR sectors read from the extracted files, so movies run through libpress, the MDEC and `movie.c` as they
     are, camera data included; their XA audio decoded in C), **libcard**
     (memory cards as save files), **pads** (PC controllers and keyboard), and the BIOS calls (events and callbacks, run in C).
   - The 187 unnamed library functions only need names where a library is replaced.

4. **Drawing: a software GPU first.** Rasterise the GPU commands into VRAM exactly (dithering, semi-transparency,
   texture windows, 24-bit display for movies) and show the display area in a window. It's exact, it can be checked
   against a reference emulator's frames, and the GPU-command boundary leaves room for a hardware renderer with
   upscaling later.

5. **Audio:** the SPU mixed at 44.1 kHz into a fixed ring buffer read by the audio callback (no allocation or locking
   there); movie audio decoded from the STR's XA sectors, at each movie's `MOVIE_ID.BIN` volume.

## Milestones

1. **The exe boots on the PC:** the whole exe (C plus recompiled asm) runs to its first screen. *Done: it reaches
   `ENDING.X`'s first screen in an SDL2 window. See the knowledge base, "PC build: the exe runs".*
2. **New game:** title, the opening movie, the first field with walking, music and sound. *So far: the boot runs
   through the credits and the title menu, and NEW GAME plays the opening movie (silent so far).*
3. **Field complete:** dialogue, menus, saving to files.
4. **Battle, world map, mini-games:** with the magic effects.
5. **The whole game,** across all three discs' content with no swaps.

Each stage is checked against the PS1: the PS1 build stays byte-identical, recompiled code is compared with decompiled
code where both exist (the sound driver's key-on check is the model), and screens against a reference emulator.

## Not now

Upscaling, widescreen and 60 fps; macOS (no 32-bit builds); other regions. Never: game data or recompiled game code
in the repo.

## Open

- SDL2 (zlib licence) is the port's one outside dependency, chosen over DirectX 9 so the build can go cross-platform:
  the window is in; audio output and controllers go through it next.
