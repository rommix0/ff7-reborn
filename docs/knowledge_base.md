# Final Fantasy VII (PS1, NTSC-U) knowledge base

Facts gathered from our own discs. Every claim here was checked against the
disc images unless it's marked *(unverified)*.

## 1. Target discs

| Disc | Image | Boot exe | Exe SHA1 |
|---|---|---|---|
| 1 | `Final Fantasy VII (USA) (Disc 1).bin` (MODE2/2352, 1 track) | `SCUS_941.63` | `a95e8b16b97071203b953bb81a33980509262f30` |
| 2 | `... (Disc 2).bin` | `SCUS_941.64` | same as disc 1 |
| 3 | `... (Disc 3).bin` | `SCUS_941.65` | same as disc 1 |

- The three boot executables are **byte-identical** (397,312 bytes). Only the file name and `SYSTEM.CNF` differ.
  `INIT/KERNEL.BIN` and `FIELD/FIELD.BIN` are identical across discs too. Code recovery only needs disc 1 plus
  any overlay that exists on a single disc.
- Main exe: loads at `0x80010000`, `$gp = 0x80062D44`, BSS `0x3C930` bytes.

## 2. Disc layout (disc 1, 3,377 files)

| Dir | Files | What it is |
|---|---|---|
| `INIT/` | 3 | `KERNEL.BIN` (game tables + text, gzip-chunked), `WINDOW.BIN` (font/UI), `YAMADA.BIN` (80 bytes: LBA/size table of boot-time files) |
| `FIELD/` | 2374 | `FIELD.BIN` (field engine overlay, compressed), `*.DAT`/`*.MIM`/`*.BSX` per map, `DSCHANGE.X`, `ENDING.X` |
| `BATTLE/` | 7 | `BATTLE.X` (battle engine), `BATINI.X`, `BATRES.X`, `BROM.X`, `SCENE.BIN` (encounters), `TITLE.BIN`, `CO.BIN` |
| `ENEMY1..6/`, `STAGE1..2/` | ~430 | Battle models and backgrounds |
| `MAGIC/` | 318 (289 `.BIN`) | One code+data overlay per spell / summon / limit effect |
| `MENU/` | 26 | One overlay per menu screen (`*.MNU`) |
| `MINI/` | 38 | Mini-games: `CHOCOBO`, `JET` (coaster), `CONDOR`, `HIGHWAY` (bike), `SNOBO`/`SNOBO2` (snowboard), `SUBMAR` (submarine) |
| `WORLD/` | 43 | `WORLD.BIN` (world map overlay), `WM*.MAP/BOT/TXZ/EV` world data |
| `SOUND/` | 5 | AKAO sound banks, see §4 |
| `MOVIE/` | 72 | 28 are MDEC `.STR`/`.MOV` with interleaved XA audio (Form 2 sectors); the rest are camera/sound data |
| `MINT/` | 2 | `DISKINFO.CNF`, `MOVIE_ID.BIN` |

Compression (all checked on disc 1 with `tools/unpack.py`):

- **Gzip overlays.** Applies to `BATTLE/*.X`, `FIELD/FIELD.BIN`, `WORLD/WORLD.BIN` and the mini-game `.BIN`s. Layout:
  `u32` decompressed size, a second `u32` *(unknown)*, then a gzip stream. The decompressed size matches in every case.
- **Plain gzip.** `BATTLE/TITLE.BIN` has no header.
- **BIN-GZ archive.** Applies to `INIT/KERNEL.BIN` and `INIT/WINDOW.BIN`. Each section has a 6-byte header (`u16` gzip
  length, `u16` decompressed length, `u16` section type) followed by its gzip data; a zero `u16` ends the archive.
  Gears calls the second word "unknown" and the third a "file number"; the disc shows they are the size and a type.
  - `KERNEL.BIN`: 27 sections. Types 0–8 are the data tables, and all 18 text sections are type 9.
  - `WINDOW.BIN`: 3 sections (33,312 + 32,800 + 1,302 bytes). Gears' offset table for this file doesn't match our disc.
- **LZS.** `FIELD/*.DAT` (703 of 787 files): `u32` **compressed** length (Gears says decompressed; the disc says
  file size − 4), then LZSS data with a 4 KiB window. All 703 decompress to a sane 7-pointer header.
  - The other 84 `.DAT` files are 2,048-byte uncompressed stubs, likely placeholder or cut maps *(unverified)*.

## 3. Code: what is recovered and what isn't

Recovered code: see §7.

### PC vs PS1

- The PC port (`pc_discs`, Eidos 1998, `FF7.EXE` 5.8 MB, x86) is a separate build from an earlier beta. Its code
  is the base for FFNx and ergonomy_joe's PC decomps (world map, coaster, chocobo race).
- **The PC build compiles all 13 menu modules into `FF7.EXE`**, where PS1 loads each one from a `.MNU` overlay (per
  Gears). So the PC code is a logic reference for exactly the 9 PS1 menus nobody has recovered. Because the PC build
  comes from an earlier beta, expect differences.
- PC music is MIDI (`DATA/MIDI/MIDI.LGP`, plus `AWE`, `XG`, `YGM` variants) driven by a separate sound library
  (ergonomy_joe ships it only as the binary `FF7SND.lib`). PC never contained the PS1 AKAO sequencer, which is why
  older PC-based reversing never recovered the PS1 music/SFX engine.
- PC data lives in `.LGP` archives; PS1 data in loose files. Many formats inside are shared (field scripts,
  `SCENE.BIN`, `KERNEL.BIN` sections), so PC tools (Makou Reactor, ff7tk) document formats that also apply here.

## 4. Sound: the AKAO engine

The PS1 sound driver is Square's **AKAO** sequencer, linked into the main exe. It drives the SPU directly:
sequences are byte-code that pick instruments, set pitch/volume/pan/ADSR/reverb and play notes on the 24 SPU voices.

Sound files (checked against our disc):

| File | Size | Layout |
|---|---|---|
| `SOUND/INSTR.ALL` | 483,232 | 16-byte header: `u32 spu_addr` (`0x1010`), `u32 length` (`0x75F90` = file size − 16), 8 zero bytes; then raw SPU ADPCM sample data uploaded to SPU RAM at `spu_addr` |
| `SOUND/INSTR.DAT` | 8,192 | Instrument table copied into the driver; starts with SPU sample/loop addresses that point into `INSTR.ALL` |
| `SOUND/INSTR2.ALL` | 251,120 | Same header (`spu_addr 0x38560`, length `0x3D4E0`), second bank loaded later on top of the upper instrument slots |
| `SOUND/INSTR2.DAT` | 8,192 | Instrument table for the second bank |
| `SOUND/EFFECT.ALL` | 51,200 | Sound-effect sequences; starts with `FF FF FF FF` then a `u16` offset table |
| `*.SND` (e.g. `MINI/ASERI2.SND`, `ENEMY6/FAN2.SND`) | small | Standalone AKAO music sequence: magic `"AKAO"`, `u16` id, `u16` length, … |

Field maps embed their music as AKAO blocks: 613 of the 703 LZS field `.DAT`s contain the `"AKAO"` magic after
decompression. That gives us roughly 600 real sequences to test a sequence parser against.

Load order: `INSTR.DAT` and `INSTR.ALL` are read by LBA through the
`YAMADA.BIN` table and uploaded once at boot; `INSTR2.*` is loaded on demand into the upper instrument slots.

Field script opcodes that drive sound, as named in Gears' opcode table (the bytes are facts; behaviour is still
to be confirmed in code): `0xDA AKAO2`, `0xF0 MUSIC`, `0xF1 SOUND`, `0xF2 AKAO`, `0xF3 MUSVT`, `0xF4 MUSVM`,
`0xF5 MULCK`, `0xF6 BMUSC`, `0xFC FMUSC`, `0xFD CMUSC`.

On PC, Gears says the PsyQ SEQ player was swapped for a MIDI player. Our `FF7.EXE` has the source paths
`lib\src\sound\midi1.cpp`, `sound.cpp`, `dx_snd.cpp`, `acm.cpp` and `creative\sfutils.cpp`. Nothing in the PC sound
path descends from AKAO, so for PS1 sound the PC build is no reference at all.

## 5. Engine architecture (Gears, PC-era research)

Gears is the Qhimm team's 2000s write-up of the engine, edited by Joshua "Halkun" Walker; we have it at
`gears.pdf`. It's an unfinished draft: several chapters (most mini-games, the platform bug lists)
are lorem-ipsum placeholders. Points that matter for PS1 code recovery:

- **Kernel and modules.** A resident kernel (the main exe) runs a threaded main loop, memory manager and CD loader,
  plus statically linked PsyQ libs. Modules swap in as overlays: field, battle, world, menu, mini-games. The field
  module drives everything; its script can call any other module. Battle can't open the menu.
- **Save map.** All persistent game state sits in one 0x10F4-byte (4,340) block that is copied wholesale to the
  memory card. Field scripts reach 5 banks inside it, 8- or 16-bit wide, plus 256 bytes of non-saved temporaries.
  Menu `SAVEMENU` previews only the first 80 bytes of each slot.
- **CD loading.** The game avoids the slow BIOS file API. It reads by **sector number**, not filename, through
  low-level calls in chunks of up to 8 KiB, so it can preload the next module while the current one runs. This fits
  `YAMADA.BIN` being an LBA/size table.
- **VRAM.** Double-buffered framebuffers on the left, CLUTs below them, then field textures and texture caches.
  Menu textures and the font stay resident on the right. 24-bit movies widen the display over the first two
  texture caches.
- **Menu module IDs** for the field `MENU` opcode (first argument always `0`): Party `0x09`; Form `0x07` (argument
  0 = make a party of 3, 1 = split into 3 groups, 2 = split into 2); Save `0x0E`; Name `0x06` (argument
  0–9 = Cloud … Chocobo); Shop `0x08` (argument = shop number). The other menus are reached only internally.
- **Menu assets.** Some `.MNU` overlays have TIM images appended. `PATYMENU` and `FORMMENU` share external TIMs on
  disc. `WINDOW.BIN` holds the resident window textures and font.
- **Debug rooms.** Field maps named after staff (Kitase, Nojima, Kyounen, …) are debug rooms that jump to other
  scenes. They're useful entry points when tracing field code.

### Original PC source tree

Our PC `FF7.EXE` still contains 100 source paths, left over from disabled memory-allocation tracing. Gears lists
the same set. They give the original module and file split, and the staff folders behind PS1 overlay names (e.g.
`battle\yama\` ↔ `INIT/YAMADA.BIN`), which helps when naming recovered PS1 files:

| Folder | Files (examples) |
|---|---|
| `c:\ff7\field\src\` | `ad_app`, `ad_bk`, `ad_cdr`, `ad_data`, `ad_human`, `ad_obj`, `ad_tile`, `tutaddr` |
| `c:\ff7\src\battle\` | `battle`, `b3ddata`; `battle3d\` (`char`, `enemy`, `limitbrk`, `stage`, `mdl`, `lmd`); staff dirs `yama\`, `yasui\`, `myoshiok\` |
| `c:\ff7\src\menu\` | `english\loadmenu`, `btlmenu\english\callback` |
| `c:\ff7\src\wm\` | `wmdefine`, `wmfile` |
| `c:\ff7\src\main\` | `main`, `initpath` |
| `c:\ff7\{chocobo,condor,coaster,highway,snobo}\` | per mini-game: `ch_*`, `cd_*`, `psxdata*`, `memory`, `tmd` |
| `c:\lib\src\` | Eidos/Square port library: `graphics\` (DirectX + software renderer, `psx.cpp`, `psxgraph.cpp`), `sound\`, `file\`, `mem\`, `thread\`, `movie\`, `polygon\` |

`snobo\tmd.cpp` and `highway\psxdata.cpp` suggest the PC mini-games wrap PS1-format data almost directly.

To regenerate the list from your own copy, extract `FF7.EXE` from the PC install disc and search it for strings that
start with `C:\`. Error strings such as `ACCESS.CPP::SOUND_SIDE_SWITCH` also survive.

## 6. Tooling notes

- `tools/extract_disc.py`: stdlib ISO9660 reader for raw 2352-byte images. Handles MODE2 (PS1) and MODE1 (PC).
  Files with any Form 2 sector (movies) are written as raw 2352-byte sectors so video players / jPSXdec keep the
  interleaved XA audio. `-l` lists LBA, size and SHA1 without writing anything.
- `tools/unpack.py`: auto-detects and decompresses gzip overlays, plain gzip, BIN-GZ archives and LZS.
  `tools/test_unpack.py` is the LZS self-check.
- A matching decomp needs the original compiler era: PsyQ's gcc 2.x for mipsel. The open-source matching tools
  (splat, maspsx, m2c, asm-differ) are separate projects; check each one's license before vendoring it.
  rabbitizer (MIPS/GTE disassembler) is MIT.
- `psx-spx.txt`: hardware reference (SPU, GPU, GTE, CD-ROM, MDEC). Use for SPU ADPCM and register layouts.
- PsyQ SDK: Sony PsyQ docs (`Devrefs/Libref.pdf`, `Filefrmt.pdf` for TIM/TMD/VAG/VAB) and libs. For which
  libraries the game links, see §8.

## 7. Toolchain: what built the game, and what our SDK has

### What's in the game (checked on `SCUS_941.63`)

- Linked PsyQ libraries carry CVS stamps from 1995–96: `libetc intr.c 1.73` (1995-11-10), `libcd bios.c 1.76`
  (1996-04-03), `libgpu sys.c 1.115` (1995-11-29). That's roughly PsyQ library 3.3–3.5.
- Game code shows the SN assembler's signature. Indexed global access expands through `$at`
  (`lui $at,hi; addu $at,$at,$a0; lw $v0,lo($at)`), load-delay `nop`s are inserted, and branch delay slots are
  filled only from instructions *before* the branch, never copied from the branch target.

### What the PsyQ SDK has (PsyQ runtime library 4.4, "DTL-S2350")

| Tool | Version | Runs on Win10 x64? |
|---|---|---|
| `pssn/bin/CC1PSX.EXE` | GCC **2.8.1** SN32 build 4.0.0010 | Yes (Win32) |
| `pssn/bin/ASPSX.EXE` | ASPSX 2.79 | Yes (needs absolute paths) |
| `pssn/bin/PSYLINK.EXE` | Psylink 2.73 (`/p` = raw binary at `/o` org) | Yes |
| `pssn/bin/DOS/CC1PSX.EXE` | GCC **2.6.3** (go32 DOS) | No, needs DOSBox |
| `pssn/bin/DOS/ASPSXD.EXE` | ASPSX 2.79D (DOS) | No, needs DOSBox |
| `psx/lib`, `zips/Lib44`, `zips/Lib45` | libs 4.4/4.5, stamps from 1997–98 | n/a |

**Test result.** I wrote C for three back-to-back leaf functions at `80014A00`–`80014A80` from our own disassembly,
then built them with the Win32 `CC1PSX` → `ASPSX` → `PSYLINK /p`:

- Instruction selection and register allocation match the game.
- They are **not byte-identical**. GCC 2.8.1 fills a loop branch's delay slot by copying an instruction from the branch
  target; the game leaves a `nop` there. `-fno-delayed-branch` overshoots: then nothing fills any slot, and ASPSX 2.79
  doesn't fill them either.
- One function also has an empty 8-byte stack frame in the game that our build omits *(cause unknown, probably
  compiler version)*.

**Conclusions about the SDK:**

- The SDK's **libraries can't be used** for matching: they're two years newer than what's linked. Library code in
  the exe stays as bytes taken from the user's own disc, which also keeps Sony code out of the repo.
- The SDK's **Win32 compiler is the wrong version** (2.8.1) for byte-matching. It's still handy for quick "does
  this C produce the same instructions" checks.
- The SDK's **DOS GCC 2.6.3 is the right version family**, but it needs DOSBox to run, and it's untested so far.
- SDK headers (`psx/include`, `LIBSPU.H`, …) and `Devrefs/Libref.pdf` give the library function signatures and
  structs. Use them to name the library functions in the exe.

### Our build toolchain (open, runs in WSL/Linux)

`tools/setup_toolchain.sh` fetches everything into the git-ignored `toolchain/` folder. Nothing gets vendored.

| Piece | Source | License | Role |
|---|---|---|---|
| GCC 2.6.3 / 2.7.2 "Sony Playstation" `cc1`, `cpp` | decompals/old-gcc release `0.17`, SHA256-pinned in `tools/toolchain.sha256` | GPL (GCC) | Compiler |
| maspsx | mkst/maspsx at commit `4ffa8b51` | MIT | Rewrites GCC output the way SN's ASPSX would: delay slots, `$at` expansions, `nop`s |
| GNU binutils (`mipsel-linux-gnu-*`) | Ubuntu package `binutils-mipsel-linux-gnu` | GPL | Assemble, link, objdump |

Pipeline: `cpp` → `cc1 -O2 -G0` → `maspsx --aspsx-version=2.79` → `as -march=r3000 -G0 -O1` → `ld` at the exact address →
`objcopy -O binary`. `tools/match.sh <file.c> <addr> [gcc]` runs it and either prints `MATCH` or diffs the disassembly
against your exe.

**Proven:** `src/main/bitops.c` (3 functions, `80014A00`–`80014A84`, 132 bytes) is **byte-identical** to the game
with GCC 2.7.2. GCC 2.6.3 matches only one of the three, so that part of the exe was built with 2.7.2.

**Full rebuild:** splat 0.50.1 splits `SCUS_941.63` per `config/main.yaml`, and `make` relinks it. The result is
SHA1-identical to retail, with `bitops.c` compiled from C and the rest still assembly.

Main exe layout (found by splat, then corrected by hand):

| File offset | VRAM | Contents |
|---|---|---|
| `0x0000` | n/a | PS-X EXE header (t_addr `0x80010000`, t_size `0x60800`, pc0 `0x800110C0`) |
| `0x0800`–`0x18B8` | `0x80010000` | read-only data (jump tables, strings such as `"battle.x"`) |
| `0x18B8`–~`0x39518` | `0x800110B8` | code. splat guessed `0x18AC`, but the 12 bytes there are the string `" (%08x)\n"` |
| ~`0x39518`–`0x61000` | ~`0x80048D18` | data *(boundary estimated by splat)* |

- **No padding between sections.** The retail link packs them back to back, so splat needs
  `ld_align_section_vram_end: False` and `ld_align_segment_vram_end: False`. Otherwise every address after the
  read-only data shifts by 8 bytes.
- **BSS size isn't set yet.** BSS isn't stored in the file, so it doesn't affect the hash.

Lessons from getting it to match:
- **Link address:** `ld` aligns `.text` to 16 bytes, so link with an explicit section address and `SUBALIGN(4)`.
  `match.sh` does this.
- **Loop shape matters:** a `while` loop gets rotated by GCC, while the original `highest_bit` loop needed a
  `goto` to come out unrotated.
- **Assembler version:** `--aspsx-version` made no difference for these simple leaf functions (2.21–2.79 all
  match). Recheck it on functions with `div`, `li` or `$gp` access.

## 8. Sony library map (main exe)

`tools/psyq_match.py` parses SN `.LIB` archives (`LIB\x01` with `LNK\x02` objects) and masks each relocated field by
its patch type. It then searches the exe for each library module, falling back to single functions. Only
names and addresses come out. Output is `config/symbols.main.psyq.txt`, which splat reads; the rebuild stays
byte-identical.

Result against SDK libs 4.4/4.5 (`psx/lib`, `patches`, `zips/Lib44`, `zips/Lib45`):

| | Count |
|---|---|
| Functions in the library block `0x80036E84`–`0x80048ACC` | 444 |
| Named with confidence | **154** (libgte 83, libapi 36, libc 12, libcard 9, libetc 8, libgpu 2, libpress 2, libspu 2) |
| Review candidates (`--fuzzy 0.8`, not applied) | 20 |
| Unnamed | ~270, mostly libspu, libcd and libgpu, which changed between the game's 1996 libs and the SDK |

Rules that keep false names out:
- **Unique:** a hit must occur exactly once in the exe.
- **Distinctive:** a hit needs at least 10 words not touched by relocations, or no relocations at all and at least
  4 words (BIOS call stubs carry a unique call number).
- **Siblings:** smaller functions count only when a distinctive sibling from the same module puts the module at the
  same base.
- **Clashes:** an address claimed by two different names is dropped. Example: `0x80048A7C` is a "call three
  functions" wrapper that fits both libsnd `SsInitHot` and libmcrd `MemCardInit`.
- **Example false positive:** `atof` at `0x800144F0` in game code was one, rejected by the distinctive rule.

Review candidates (similarity, then name and address; confirm against `Libref.pdf` before using):
0.94 `SpuSetReverbDepth` `0x8003884C`, 0.94 `SpuSetIRQAddr` `0x80038BC4`, 0.92 `_SpuIsInAllocateArea_` `0x80037D90`,
0.88 `SpuInitMalloc` `0x80036FFC`, 0.92 `VSyncCallback` `0x8003D150`, 0.91 `CD_vol` `0x8003F398`,
0.85 `CD_initvol` `0x8003F500`, 0.86 `StSetStream` `0x80040A40`, 0.90 `SetDispMask` `0x80043D3C`,
0.89 `ClearImage` `0x80043F6C`, 0.84 `LoadImage` `0x80044000`, 0.84 `StoreImage` `0x80044064`,
0.90 `DrawOTag` `0x8004433C`, 0.83 `SetSemiTrans` `0x80046848`, 0.89 `BreakDraw` `0x800484A8`,
1.00 `SpuStSetPreparationFinishedCallback` `0x80038FEC`, `CdPosToInt` `0x8003E3D4`, `StClearRing` `0x80040898`,
`ReadInitPadFlag` `0x80043928`, `OpenTIM` `0x80046CFC`. The 1.00 ones are exact but too small to pass the
distinctive rule alone.

What would name the rest: the PsyQ 3.3–3.6 libraries the game actually linked. The CVS stamps in §7 point there.

**Game code ends just before `0x80036E84`.** The last unnamed functions before it (`0x80036D98`, `0x80036DE0`) still
need checking: game code, or the start of libspu?
