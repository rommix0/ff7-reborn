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

The PS1 sound driver is Square's **AKAO** sequencer, linked into the main exe. Sequences are byte code that picks
instruments, sets pitch, volume, pan, ADSR and reverb, and plays notes on the 24 SPU voices. It reaches the SPU
**only through libspu**: no game code writes the SPU registers (`0x1F801C00`–`0x1F801DFF`) itself.

Everything below comes from our own disassembly.

### Driver map (our analysis of `SCUS_941.63`)

How it was found:
- **libspu callers.** Every game function that calls a named libspu function (§8) sits in `0x8002988C`–`0x8002F848`.
- **Boundaries from the call graph.** The function before `0x800293D0` is graphics code (`AddPrim`, `GetClut`), and
  at `0x80033A90` CD code begins (`CdInit`, `CdRead`, `CdReadSync`).
- **Pointer tables.** Most of the driver's ~330 functions have no direct caller; they're reached through tables.

**Range: `0x800293D0`–`0x80033A90` (~42 KB).**

| Address | Our name | What it is (verified by reading the code) |
|---|---|---|
| `0x8002DA7C` | (unnamed) | The only driver function called from all over the game (10 outside callers); a public entry point |
| `0x8002DF88` | `snd_run_command` | Runs one sound command immediately: remaps `0x9C`/`0x9D`/`0xD5`/`0xD6`, then `snd_command_handlers[cmd](entry)` |
| `0x8002E1A8` | `snd_flush_queue` | Runs every queued command: entries of 0x24 bytes at `snd_queue` (`0x80081DC8`), count at `snd_queue_count` (`0x80063010`) |
| `0x8002E23C` | (unnamed) | Calls a run of 8 libspu setters; probably the per-voice SPU update |
| `0x80030E7C` | `seq_step_channel` | Sequence interpreter for one channel (below) |
| `0x80049548` | `snd_command_handlers` | 256 handlers indexed by command byte. Live: `10`, `14`, `15`, `18`, `19`, `20`–`23`, `28`–`2B`, `30`, `34`, `80`–`82`, `90`, `92`, `9A`–`9D`, `A0`–`BD`, `C0`–`C2`, `C8`–`CA`, `D0`–`D2`, `D4`–`D6`, `E0`, `E4`, `F0`–`F5`, `F8`–`FA`. Everything else is the no-op `0x8002CF98` |
| `0x80049AA8` | `seq_cmd_handlers` | 96 handlers for sequence commands `0xA0`–`0xFF`. The code indexes it with the raw byte from base `0x80049828` |
| `0x80049C28` | `seq_note_lengths` | 11 `u16`: `0xC0C0, 0x6060, 0x3030, 0x1818, 0x0C0C, 0x0606, 0x0303, 0x2020, 0x1010, 0x0808, 0x0404` |
| `0x80075F28` | `instrument_table` | 128 × 64-byte instruments (= `INSTR.DAT` 8,192 bytes). Field `+0` is the SPU sample address (`0x1010` for entry 0, matching `INSTR.ALL`'s header) |

The command table is presumably what the field opcodes `AKAO`/`AKAO2` feed; command `0x10` is the first live
handler. *(unverified)*

### Sequence format (from `seq_step_channel`)

Per channel, a state struct whose offset `0x00` is the read pointer into the sequence. Each step:
1. Read byte `b`, advance the pointer.
2. If `b >= 0xA0`, call `seq_cmd_handlers[b - 0xA0](channel, …)`. Repeat until a note, or until `b == 0xA0`, which
   ends the channel.
3. Otherwise `b` is a **note**: `pitch = b / 11`, `length = seq_note_lengths[b % 11]` (the low byte is used as the tick
   count: 192 = whole, 96, 48, 24, 12, 6, 3; triplets 32, 16, 8, 4).
   - `pitch` 0–11 is a semitone in the current octave.
   - `b` `0x84`–`0x8E` (pitch 12) is a **tie**.
   - `b` `0x8F`–`0x9F` (pitch 13+) is a **rest**.
4. When a channel flag (`+0x38` bit 3) is set, the semitone instead indexes a 5-byte-per-key map at `channel+0x14`
   and picks a per-key instrument from `instrument_table`. That's drum-kit mode *(name is our guess)*.

Commands read so far:
- **`A0`:** end of channel. Unused bytes `E0`–`E7`, `FA`–`FC` and `FF` all go to `seq_cmd_unknown`, which just calls the `A0` handler.
- **`A5`, `A6`, `A7`:** `A5 n` sets the octave (`+0x66`); `A6` raises it by one and `A7` lowers it, both masked to 0–15.
- **`A3 n`, `A8 n`:** store a byte into a channel field and flag it for an SPU update; `A8` stores it sign-extended
  `<< 23` into `+0x44` (a volume-like value).
- **`CC`:** sets `+0x6E = 1`.

Argument lengths come from how far each handler moves the read pointer, then were confirmed by
`tools/akao.py check`. **All 98 distinct songs (1,340 channels) and all 1,423 sound-effect streams on the three
discs parse cleanly** with this table. No unused command is hit, and about 95% of streams end exactly where the next one
starts:

| Args | Commands |
|---|---|
| 0 | `A0` end · `A6` `A7` octave ± · `B3` `B6` `BA` `BE` `C2`–`C7` · `C8` loop point · `CA` loop back (stops linear parsing) · `CB`–`CD` `D0` `D1` `D4`–`D7` `DB` `ED` `F3` `F5` `F9` |
| 1 | `A1`–`A3` `A5` octave · `A8` volume · `AA` `AC`–`AF` `B1` `B2` `B5` `B7` `B9` `BB` `BD` `BF` `C0` `C1` · `C9` repeat count · `CE` `CF` `D2` `D3` `D8`–`DA` `DC` `F2` `F6` `F8` |
| 2 | `A4` `A9` `AB` `B0` `BC` `DD`–`DF` `E8`–`EC` · `EE` jump (stops linear parsing) · `F4` `F7` `FD` `FE` |
| 3 | `B4` `B8` `EF` `F0` `F1` |
| — | `E0`–`E7` `FA`–`FC` `FF`: unused; the driver treats them as `A0` |

- **Lengths confirmed by real data** for every command except `B7`, `BE`, `BF`, `CE`, `CF`, `DF`, `E9`, `EB`, `EF`, `F1`
  and `F3`. None of those occur on the discs, so their lengths rest on the handler code alone.
- **Structure quirks:**
  - Channels may fall through into the next channel's bytes (shared tails).
  - Some songs leave trailing bytes after the last channel.
- **Behaviour notes:**
  - **Read in detail and confirmed by the MIDI loop check (below):** `A0`, `A1`, `A2`, `A5`–`A7`, `C8`–`CA`, `DC`, `E8`,
    `EE` and `F0`.
  - **Read but not fully confirmed:** `A8`/`AA` store volume- and pan-like values.
  - **Not studied yet:** the rest.

### Timing, loops and MIDI export

- **Clock:** `snd_init` (`0x8002988C`) sets up root counter 2 with target `0x43D1` and `snd_timer_tick`
  (`0x80030234`) as its interrupt. Counter 2 runs at system clock ÷ 8, so the driver steps about **243.86 times a
  second**. The ÷8 is confirmed by the driver's own catch-up code, which counts elapsed scanlines in units of 66.
- **Tempo:** each step, `seq_tick` (`0x800308D4`) adds the song tempo (the `u16` from `E8 lo hi`, kept at
  `seq_song + 0x18`) to a 16-bit accumulator, and every carry is one sequence tick. So
  ticks/s = 243.86 × tempo / 65,536, and with 48 ticks per quarter note, **BPM = tempo × 0.004651**. Across the
  96 songs that set a tempo this gives 56–191 BPM (median 106).
- **Per-channel counter:** `+0x56` holds `gate << 8 | length`, and each tick subtracts 1 from both bytes. Length 0
  steps the channel to its next event; gate 0 releases the note. Notes that aren't tied get a gate 2 ticks shorter
  than the length.
- **Length commands:**
  - **`A2 n`:** the next note lasts `n` ticks, and the fixed length is cleared.
  - **`DC d`:** sets a fixed length for every following note, equal to the last length + signed `d`, clamped to 1–255.
    `d` = 0 turns it off.
  - **Precedence:** fixed length, then `A2`, then the length table.
- **Loop commands:**
  - **`C8`:** pushes a loop point (4 levels).
  - **`C9 n`:** jumps back until pass `n` (0 = 256), then pops.
  - **`CA`:** jumps back forever.
  - **`F0 n lo hi`:** jumps by a signed 16-bit offset on pass `n` of the current repeat, for a different ending on the
    last pass.
  - **`EE lo hi`:** jumps by a signed 16-bit offset, counted from after its arguments; a backward jump is the song's
    endless loop.
- **Instrument commands:** `A1 n` selects instrument `n`. `F2 n` also changes instrument, rescaling the playing pitch
  from the old instrument to the new one.

`python tools/akao.py midi <file|dir> <out>` writes one Standard MIDI File per song:
- **Layout:** format 1 at 48 PPQ, a conductor track with tempos plus `loopStart`/`loopEnd` markers, and one track per
  channel (MIDI channel = index mod 16).
- **Notes:** key = 12 × (octave + 1) + semitone, so octave 4 = middle C.
- **Controls:** program = AKAO instrument; CC7 = `A8`; CC10 = `AA`; velocity is fixed at 100.
- **Loops:** counted repeats are unrolled, and endless loops play once.
- **Left out:** vibrato/tremolo/pan LFOs, slides, ADSR, reverb, drum-kit mode and the other effect commands.

Check: in all **98 songs, every looping channel repeats the same length**, or a length that divides it, such as a
one-bar drum loop. Echo channels may start a few ticks late. Before `F0` was handled, 5 songs failed this check,
which is how its meaning was confirmed. No channel runs past 4,000 bars.

Sound files (checked against our disc):

| File | Size | Layout |
|---|---|---|
| `SOUND/INSTR.ALL` | 483,232 | 16-byte header: `u32 spu_addr` (`0x1010`), `u32 length` (`0x75F90` = file size − 16), 8 zero bytes; then raw SPU ADPCM sample data uploaded to SPU RAM at `spu_addr` |
| `SOUND/INSTR.DAT` | 8,192 | 128 × 64-byte instruments, copied whole into `instrument_table` by `load_instrument_bank` (`0x800297A4`). Entry: `+0x00` `u32` SPU sample address, `+0x04` `u32` SPU loop address, `+0x08` 8 bytes of ADSR settings, `+0x10` 12 × `u32` SPU pitch for each semitone of octave 6 (other octaves shift it). 93 slots used |
| `SOUND/INSTR2.ALL` | 251,120 | Same header (`spu_addr 0x38560`, length `0x3D4E0`), second bank uploaded over the top part of bank 1's sample memory |
| `SOUND/INSTR2.DAT` | 8,192 | `load_instrument_bank2` (`0x80029818`) copies only the first 75 entries (4,800 bytes) to slot **53** onward (`instrument_table + 0xD40`). 6 used, becoming slots 53–58 |
| `SOUND/EFFECT.ALL` | 51,200 | Sound effects: a `0x1000`-byte table of 2,048 `u16` offsets (`0xFFFF` = empty) relative to `0x1000`, then 1,423 header-less command streams (same command set as music). Identical on all discs |
| `*.SND` (e.g. `MINI/ASERI2.SND`, `ENEMY6/FAN2.SND`) | small | One AKAO block (layout below) |

**Samples** are SPU-ADPCM (psx-spx: 16-byte blocks, 28 four-bit samples, 5 prediction filters):
- **Loops in bank 1:** bank 1 samples set no loop-start flags. Their blocks carry flag `0x02` and the last one `0x03`
  ("end, then repeat"), so the loop point comes only from `INSTR.DAT`'s loop address.
- **Converter:** `tools/instruments.py` turns both banks into 99 WAVs (64 looping, with `smpl` loop chunks) plus
  `instruments.csv`. Checks: no sample runs into the next one, no output clips, and a listening check (2026-10-28) found the WAVs sound right.
- **Playback rate:** WAVs play at 44,100 Hz, which is SPU pitch `0x1000`. The game plays semitone *s* of octave 6 at
  44,100 × `pitch[s]` / 0x1000 Hz; the CSV lists the rate for C.

**AKAO block** (in `.SND` files, field maps, `MINI/BGM*.LZS`, `MINI/CHOCOBO.DAT`, `MOVIE/STAFF*.BIN`):
- `0x00` `"AKAO"`.
- `0x04` `u16` song id.
- `0x06` `u16` data length (block = 16 + length).
- `0x08` `u16` *(unknown)*.
- `0x0A` 6-byte BCD timestamp, e.g. `96 12 18 22 46 28` = 1996-12-18 22:46:28.
- `0x10` `u32` channel mask.
- `0x14` one `u16` per set bit. Each is relative to the byte after itself, so channel *i* starts at
  `0x14 + 2i + 2 + offset`.

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

`tools/psyq_match.py` parses SN `.LIB` archives (`LIB` with `LNK` objects) and masks each relocated field by
its patch type. It then searches the exe for each library module, falling back to single functions. With
`--ref-exe EXE NM_LIST` it also matches a linked library EXE named by an `nm` listing. Only names and addresses
come out. Output is `config/symbols.main.psyq.txt`, which splat reads; the rebuild stays byte-identical.

Sources, in priority order (on a rename the first one wins, so the closest version goes first):

| Source | Where | Version stamps (libetc / libcd / libgpu) | Names alone |
|---|---|---|---|
| PsyQ **3.5** libs | user archive `PSYQ_SDK.zip` → `psyq/lib/old_libs/LIB35.ZIP` (1996-05) | 1.73 / 1.77 / 1.120 | 247 |
| PsyQ 4.4/4.5 libs | PsyQ SDK (`psx/lib`, `patches`, `zips/Lib44`, `zips/Lib45`) | 1.75 / 1.86 / 1.140 | 154 |
| Net Yaroze resident library | `MON/LIBPS.EXE` on DTL-S3030/S3035, named by `PSX/LIB/LIBPS.A` on DTL-S3040 (`nm` listing) | 1.73 / 1.76 / 1.120 | 40 |
| **FF7 itself** | `SCUS_941.63` | 1.73 / 1.76 / 1.115 | |

Combined: **267 of 454** functions in the library block `0x80036E84`–`0x80048D18` named (58%). By library: libgte 110,
libgpu 40, libapi 40, libspu 14, libcd 13, libetc 12, libc 14, libcard 10, libpress 5, Net-Yaroze-only 10. The libspu
names include `SpuSetReverb`, `SpuSetIRQCallback`, `SpuGetKeyStatus` and `SpuMallocWithStartAddr`, which are useful
anchors for finding the AKAO driver.

Rules that keep false names out:
- **Unique:** a hit must occur exactly once in the exe.
- **Distinctive:** a hit needs at least 10 words not touched by relocations, or no relocations at all and at least
  4 words (BIOS call stubs carry a unique call number).
- **Siblings:** smaller functions count only when a distinctive sibling from the same module puts the module at the
  same base.
- **Clashes:** two names for one address are dropped, unless both come from the same library. Then it's an API
  renamed between versions (`InitPAD` → `InitPAD2`, `delete` → `erase`) and the first-listed source wins. Example of
  a dropped clash: `0x80048A7C` is a "call three functions" wrapper that fits both libsnd `SsInitHot` and libmcrd
  `MemCardInit`.
- **One name per address:** a name found at two addresses stays with the higher-priority source. 3.5 `StartCARD` is
  a BIOS stub; 4.4's `StartCARD` is a different wrapper.
- **Reference EXEs** have no relocation records, so jump targets, `lui` immediates, and offsets from a `lui`-loaded
  register or `$gp` are masked.
- **Example false positive:** `atof` at `0x800144F0` in game code was one, rejected by the distinctive rule.

Review candidates (`--fuzzy 0.8`, not applied). The earlier 4.4-only list was a good test of this policy: 8 of its
20 guesses are now confirmed by exact matches, but 2 were **wrong** (`0x80038FEC` is `SpuSetTransferCallback`, not
`SpuStSetPreparationFinishedCallback`; `0x80043928` is `GetVideoMode`, not `ReadInitPadFlag`). Still open, with
similarity scores:
- **libspu, 1.00 but too small to pass alone:** `_SpuDataCallback` `0x80036FD8`, `SpuSetNoiseVoice` `0x80037964`,
  `SpuSetReverbVoice` `0x800388C4`, `_SpuCallback` `0x80038C48`, `SpuSetTransferMode` `0x80038FB8`,
  `SpuSetTransferCallback` `0x80038FEC`, `SpuSetPitchLFOVoice` `0x80039010`. Also `_SpuIsInAllocateArea_` `0x80037D90`
  at 0.92.
- **libcd:** `CdControl` `0x8003DE9C` 0.90, `CdControlF` `0x8003DFD4` 0.84, `CdMix` `0x8003E244` 0.89, `CdGetSector`
  `0x8003E268` 0.89, `CdLastPos` `0x8003DD08` 1.00, `CdPosToInt` `0x8003E3D4` 1.00, `CD_vol` `0x8003F398` 0.91,
  `CD_initvol` `0x8003F500` 0.85, `StSetRing` `0x8003DBA0` 1.00, `StClearRing` `0x80040898` 1.00, `StSetMask`
  `0x80040C88` 1.00, `CdGetToc` `0x80042144` 1.00, `CdGetDiskType` `0x8004076C` 0.81.
- **libgpu:** `ClearOTag` `0x8004418C` 0.85, `DrawOTag` `0x8004433C` 0.90, `SetDrawMode` `0x80044A68` 0.91,
  `SetSemiTrans` `0x80046848` 0.90, `SetShadeTex` `0x80046870` 0.90, `BreakDraw` `0x800484A8` 0.89.
- **libetc:** `GetVideoMode` `0x80043928` 1.00, `startIntrDMA` `0x8003D8F8` 0.81.

The remaining ~187 unnamed functions are probably from PsyQ 3.3/3.4 builds or are internal (static) helpers with no
exported name. The game's libgpu `sys.c 1.115` is older than every source we have.

**Game code ends just before `0x80036E84`.** The last unnamed functions before it (`0x80036D98`, `0x80036DE0`) still
need checking: game code, or the start of libspu?
