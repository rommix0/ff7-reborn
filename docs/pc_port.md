# PC port strategy

Goal: FF7 playable on the PC from the user's own extracted discs, with no disc swapping, while the PS1 build stays
byte-identical. Decompiling every module to matching C first would take years before anything plays, so the game runs
first and decompilation keeps replacing parts of it.

## What's left to run

The main exe is mostly C (boot, sound driver, CD layer, movie player), and so are all 13 menu modules
(`MENU/*.MNU`: the field menu and its screens, the shops and the save screens; 20 functions parked as asm), the
game over screen in `FIELD/DSCHANGE.X` (its disc prompt parked), the battle's setup and results, `BATTLE/BATINI.X`
and `BATRES.X` (7 functions parked), the logo and credits in `FIELD/ENDING.X` (its star field parked) and the Speed
Square, `MINI/JET.BIN` (29 functions parked), and most of the submarine, `MINI/SUBMAR.BIN` (12 of its functions; 46 parked).
Outside them, nothing is recovered:

| Module | Unpacked |
|---|---|
| Field (`FIELD/FIELD.BIN`) | 258 KiB |
| Battle (`BATTLE/BATTLE.X`) | 334 KiB |
| Title (`BATTLE/TITLE.BIN`) | 140 KiB |
| World map (`WORLD/WORLD.BIN`) | 160 KiB |
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
   - The menus are the first whole modules done this way: the PC build compiles their C in place of the recompiled
     code, with the parked functions still recompiled. Their C, the parked functions' non-matching C and the fully
     recompiled modules give the same traces, screens and save data on scripted runs through each module's screens.
   - A call into a module's recompiled code passes a0-a3 and the ten stack words a MIPS function can read (`JET.BIN`
     has functions of 11 arguments). Decompiled C that keeps a pointer from the game's hand-written GTE code sets
     bit 31 on it (`KSEG0`): those routines return 24-bit addresses, which the PS1 reads through RAM's mirror at 0,
     and only recompiled code folds that mirror.
   - The Speed Square with `jet.c` gives the same traces as fully recompiled, and its ride starts a frame sooner: the
     start is timed in vblanks (`VSync(-1)`), PS1 time advances with polls, and recompiled loops poll on every
     backward branch where native C doesn't, so a frame of C passes less PS1 time.
   - The submarine with `submar.c` gives the same traces, screens and VRAM as fully recompiled; only its clock's
     vblank counts differ, for the same reason. Built with its parked C (`-DNON_MATCHING`) it gives the same
     traces; the screens differ only in where the raised cells come up, which moves with how much native code runs
     (sets of functions that never call `rand` move it the same way).
   - C that calls `rand` or `srand` gets the game's own (`common.h` maps them to the recompiled `psx_rand` and
     `psx_srand`), not the host C library's.

3. **Hardware where the PS1's behaviour is the point, libraries where the PC should differ.**
   - Emulated, so Sony's library code runs as compiled: the **GPU** (GP0/GP1 commands and DMA into 1024×512 VRAM),
     the **GTE** (COP2, for libgte and inline use alike), the **MDEC** (done: the in-memory movies run through libpress
     unchanged), the **SPU** (done: 24 ADPCM voices, ADSR, sweeps, noise, reverb, its interrupt, CD input), root counters and the VSync interrupt. Behaviour
     comes from psx-spx; no emulator code is copied (most are GPL).
   - Replaced at the library: **libcd** (done: extracted files, the save's disc always inserted), **libcd's streaming**
     (the STR sectors read from the extracted files, so movies run through libpress, the MDEC and `movie.c` as they
     are, camera data included; their XA audio decoded in C), **libcard**
     (done: memory cards as raw card image files, at the BIOS's file functions), **pads** (PC controllers and keyboard), and the BIOS calls (events and callbacks, run in C).
   - The 187 unnamed library functions only need names where a library is replaced.

4. **Drawing: a software GPU first.** Rasterise the GPU commands into VRAM exactly (dithering, semi-transparency,
   texture windows, 24-bit display for movies) and show the display area in a window. It's exact, it can be checked
   against a reference emulator's frames, and the GPU-command boundary leaves room for a hardware renderer with
   upscaling later.

5. **Audio:** the SPU mixed at 44.1 kHz into a fixed ring buffer read by the audio callback (no allocation or locking
   there); movie audio decoded from the STR's XA sectors, at each movie's `MOVIE_ID.BIN` volume. *Done.*

## Milestones

1. **The exe boots on the PC:** the whole exe (C plus recompiled asm) runs to its first screen. *Done: it reaches
   `ENDING.X`'s first screen in an SDL2 window. See the knowledge base, "PC build: the exe runs".*
2. **New game:** title, the opening movie, the first field with walking, music and sound. *So far: the boot runs
   through the credits and the title menu, and NEW GAME plays the opening movie, with its sound, into the first field; music and effects play.*
3. **Field complete:** dialogue, menus, saving to files. *So far: the field menu and all ten of its screens work, and saving and loading
   (Continue) go through the game's own save menu to `memcard1.mcd`.*
4. **Battle, world map, mini-games:** with the magic effects. *So far: the battle modules and all 289 effect files
   are recompiled, and the first battle plays through to its results and back to the field. The world map runs:
   walking, the map display, the camera, its menu, random battles (escaped, or lost through the game over screen and
   back to the title), and walking into a field. All seven mini-games start, draw and take the pad.*
5. **The whole game,** across all three discs' content with no swaps. *So far: saves from a playthrough, one per
   story stop on all three discs, load and play: walking, fields, the world maps and battles.*

Each stage is checked against the PS1: the PS1 build stays byte-identical, recompiled code is compared with decompiled
code where both exist (the sound driver's key-on check is the model), and screens against a reference emulator.

## Not now

Upscaling, widescreen and 60 fps; macOS (no 32-bit builds); other regions. Never: game data or recompiled game code
in the repo.

## Open

- SDL2 (zlib licence) is the port's one outside dependency, chosen over DirectX 9 so the build can go cross-platform:
  the window, sound and controllers are in.
