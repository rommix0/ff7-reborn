# Final Fantasy VII (PS1, NTSC-U) knowledge base

Facts about the game's discs, code and data formats. Claims were checked against our own disc images; guesses say so,
and outside sources are named where a fact comes from them.

## 1. Target discs

| Disc | Image | Boot exe | Exe SHA1 |
|---|---|---|---|
| 1 | `Final Fantasy VII (USA) (Disc 1).bin` (MODE2/2352, 1 track) | `SCUS_941.63` | `a95e8b16b97071203b953bb81a33980509262f30` |
| 2 | `... (Disc 2).bin` | `SCUS_941.64` | same as disc 1 |
| 3 | `... (Disc 3).bin` | `SCUS_941.65` | same as disc 1 |

- The three boot executables are **byte-identical** (397,312 bytes); only the file name and `SYSTEM.CNF` differ.
  `INIT/KERNEL.BIN` and `FIELD/FIELD.BIN` are identical across discs too, so code recovery needs disc 1 plus any
  overlay that exists on a single disc.
- Main exe: loads at `0x80010000`, `$gp = 0x80062D44`.

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
| `MOVIE/` | 72 | 51 MDEC movies: 28 `.MOV` with interleaved XA audio (Form 2 sectors), 23 silent `.STR`; the rest are camera/sound data |
| `MINT/` | 2 | `DISKINFO.CNF`, `MOVIE_ID.BIN` |

Compression (all checked on disc 1 with `tools/unpack.py`):

- **Gzip overlays** (`BATTLE/*.X`, `FIELD/FIELD.BIN`, `WORLD/WORLD.BIN`, the mini-game `.BIN`s): `u32` decompressed
  size, a second `u32`, then a gzip stream. The size matches in every case.
- **Plain gzip:** `BATTLE/TITLE.BIN`, no header.
- **BIN-GZ archive** (`INIT/KERNEL.BIN`, `INIT/WINDOW.BIN`): each section is a 6-byte header (`u16` gzip length,
  `u16` decompressed length, `u16` section type; Gears calls the last two "unknown" and "file number") and its gzip
  data. A zero `u16` ends the archive.
  - `KERNEL.BIN`: 27 sections. Types 0–8 are the data tables; all 18 text sections are type 9.
  - `WINDOW.BIN`: 3 sections (33,312 + 32,800 + 1,302 bytes). Gears' offset table for it doesn't match the disc.
- **LZS** (`FIELD/*.DAT`, 703 of 787 files): `u32` **compressed** length (file size − 4; Gears says decompressed),
  then LZSS data with a 4 KiB window. All 703 decompress to a sane 7-pointer header. The other 84 `.DAT` files are
  2,048-byte uncompressed stubs, likely placeholder or cut maps.

**The three discs share one layout.** Of the 3,337 files on all three, 3,325 have the same sector, size and contents
on each. The rest:
- `MOVIE/`: each disc has its own set from sector 128,000 on (72, 79 and 48 files; 33 names on all three, 23 of them at
  the same sector).
- `SNOVA/` (17 files, sectors 127,101–127,660): disc 3 only; discs 1 and 2 have nothing there.
- `FIELD/TUNNEL_6.MIM`: same place and size, but disc 1's contents differ.
- `MINT/DISKINFO.CNF` starts `DISK0001`/`2`/`3` (the game's disc check, §7); `MINT/MOVIE_ID.BIN` differs too.

A movie name found on several discs has the same contents on each, so the PC port needs one copy of each movie.

**Movies** (`tools/movie.py` rewraps them for the PC port):
- Each video sector starts with the standard 32-byte STR header (`u16` `0x0160`, `u16` `0x8001`, sector index and
  count within the frame, `u32` frame number from 1, `u32` frame size, `u16` width and height); 2,016 bytes of frame
  data follow. A frame's data opens with 40 bytes of camera data, then the MDEC frame: `u16` code count, `0x3800`,
  `u16` qscale, version 1, then the libpress bitstream. All are 320×224 except `ENDING2E`/`ENDING3E` (640×192).
- `.MOV` files put an XA audio sector (stereo, 37.8 kHz, 4-bit) every 8th sector. `.STR` files are video only, all
  Form 1, so `tools/extract_disc.py` writes them as 2,048-byte sectors.
- 15 `.MOV`s set the emphasis bit in every audio sector (`CANON`, `FALLPL`, `GELNICA`, `HWINDJET`, `JAIROFAL`,
  `MAINPLR`, `NORTHMK`, `NRCRL`, `ONTRAIN`, `PARASHOT`, `PLREXP`, `RCKTFAIL`, `SOUTHMK`, `U_ROPEGO`, `U_ROPEIN`). Their
  audio was mastered with the CD's 50/15 µs pre-emphasis (IEC 60908): against the same movies on the PC release,
  their spectrum rises by that curve to within 1 dB, while unflagged movies match flat. The PS1 de-emphasizes them on
  playback (psx-spx; its exact filter is undocumented).
- The movie player (§7) calls libpress (`DecDCTvlc`, `DecDCTin`) and its reset loads the default quantization tables
  at `0x80051A50` (luma) and `0x80051A90` (chroma). Both tables are the same.
- Of the 40 bytes before each MDEC frame, the player copies all to `movie_camera` for the field; the last `u16` is a
  colour flag: 1 decodes in 24-bit colour from the next frame on, 0 in 16-bit. A movie without camera data has
  zeros and that 1.
- `MINT/MOVIE_ID.BIN` is the movie table, by number (54 entries on disc 1): 20 bytes each, `u32` sector, `u32` size,
  `u16` x, y (where the frame goes), `u16` w, h, `u16` last frame, `u16` type. Type 0 streams an STR or MOV, and w, h
  are its XA volume, left and right (255 for most, down to 152). Types 2 (looping) and 3 (once) are read into memory
  whole: a table of `u32` frame offsets, then a bare MDEC frame at each, no STR headers, no camera data. There are
  seven: `FSHIP2.BIN` and `FSHIP2N.BIN` (320×128, 45 frames, type 2), `OPENMV.BIN` (320×224 at y 8, 30 frames, type
  3), and 320×224 type 2 loops `WHITE2.BIN` (discs 2–3), `LAST4_2.BIN`, `LAST4_3.BIN` and `LASTMAP.BIN` (disc 3).
- Only the field opens movies. Its main loop waits `VSync(2)` (30 fps), or `VSync(3)` while a streamed movie plays;
  an in-memory movie (`D_800965E4` = 1) keeps 30 fps, and advances one frame per pass, since the field calls
  `cd_step` once per pass.
- Each movie fades the XA audio up (sound command `C9`, 30 ticks) when it opens and down 15 frames before its end. On
  disc 1, movie `0x26` (`HIWIND0`) slides to 0 and `0x31` (`GOLD7_2`) to `0x1800` instead of `0x7FFF`.
- The movies are 15 fps, and the CD streams 150 sectors a second, about 10 per frame. In most `.MOV`s the frames
  arrive a little late (0.6 s by the end of `ENDING2E`); in `SMK.STR` and `LSLMV.STR` they arrive early. The
  converter shows each frame at the next 15 fps tick or when its data has arrived, whichever is later.
- Audio usually runs past the last frame (median 3.2 s).

## 3. Code: what is recovered and what isn't

The main exe `SCUS_941.63` is split with splat and `make` rebuilds it byte-identical (SHA1-checked) from asm plus the
recovered C. Details in §7.

| Part | Range | Status |
|---|---|---|
| Boot code (`main.c`, `transition.c`, `loader.c`) | `0x8001117C`–`0x80014A00` | all 47 functions in C |
| `bitops.c` | `0x80014A00`–`0x80014A84` | 3 functions in C |
| AKAO sound driver (`sound_init.c`, `sound.c`) | `0x800293D0`–`0x80033A90` | 231 of 232 functions in C |
| CD layer (`cd.c`) | `0x80033A90`–`0x80034CF0` | all 38 functions in C |
| Movie player (`movie.c`) | `0x80034F3C`–`0x8003623C` | all 14 functions in C |
| PsyQ library block | `0x80036E84`–`0x80048D18` | 267 of 454 functions named (§8) |

Not recovered yet: the rest of the exe's game code (still asm) and every overlay: battle, field, world, menus,
mini-games, the 289 `MAGIC/*.BIN` effects, `DSCHANGE.X`, `ENDING.X`, `BATTLE/TITLE.BIN` (probably the
title screen).

### PC vs PS1

- The PC port (Eidos 1998, `FF7.EXE` 5.8 MB, x86) is a separate build from an earlier beta, so expect differences.
  Its code is the base for FFNx and ergonomy_joe's PC decomps (world map, coaster, chocobo race).
- **PC compiles all 13 menu modules into `FF7.EXE`**; PS1 loads each from a `.MNU` overlay (per Gears). The PC code
  is a logic reference for the PS1 menu overlays.
- PC music is MIDI (`DATA/MIDI/MIDI.LGP`, plus `AWE`, `XG`, `YGM` variants) through a separate sound library
  (ergonomy_joe ships it only as the binary `FF7SND.lib`). PC never had the AKAO sequencer, so PC-based reversing
  never covered the PS1 music/SFX engine.
- PC data lives in `.LGP` archives, PS1 data in loose files. Many formats inside are shared (field scripts,
  `SCENE.BIN`, `KERNEL.BIN` sections), so PC tools (Makou Reactor, ff7tk) document formats that also apply here.

## 4. Sound: the AKAO engine

The PS1 sound driver is Square's **AKAO** sequencer, linked into the main exe. Sequences are byte code that picks
instruments, sets pitch, volume, pan, ADSR and reverb, and plays notes on the 24 SPU voices. It reaches the SPU
**only through libspu**: no game code writes the SPU registers (`0x1F801C00`–`0x1F801DFF`) itself. Everything here
comes from our own disassembly of `SCUS_941.63`.

### Driver map

**Range: `0x800293D0`–`0x80033A90` (~42 KB).** Every game function that calls a named libspu function (§8) sits in
`0x8002988C`–`0x8002F848`. The call graph bounds the rest: graphics code (`AddPrim`, `GetClut`) ends at `0x800293D0`
and CD code (`CdInit`, `CdRead`, `CdReadSync`) starts at `0x80033A90`. Most of the driver's ~330 functions have no
direct caller; they're reached through pointer tables.

| Address | Our name | What it is (read from the code) |
|---|---|---|
| `0x8002DA7C` | (unnamed) | The only driver function called from all over the game (10 outside callers); a public entry point |
| `0x8002DF88` | `snd_run_command` | Runs one sound command immediately. Shorthands: `24`–`27` play 1–4 effects with consecutive ids (as `20`–`23`); `98`/`99` run `9A`+`9C` / `9B`+`9D`; `D8`–`DA` run `D0`–`D2` with `D4`–`D6`. The song commands do nothing here. Then `snd_command_handlers[cmd](entry)` |
| `0x8002E1A8` | `snd_flush_queue` | Runs every queued command: entries of 0x24 bytes at `snd_queue` (`0x80081DC8`), count at `snd_queue_count` (`0x80063010`) |
| `0x8002E23C` | (unnamed) | Calls a run of 8 libspu setters; probably the per-voice SPU update |
| `0x80030E7C` | `seq_step_channel` | Sequence interpreter for one channel (below) |
| `0x80049548` | `snd_command_handlers` | 256 handlers indexed by command byte. Live: `10`, `14`, `15`, `18`, `19`, `20`–`23`, `28`–`2B`, `30`, `34`, `80`–`82`, `90`, `92`, `9A`–`9D`, `A0`–`BD`, `C0`–`C2`, `C8`–`CA`, `D0`–`D2`, `D4`–`D6`, `E0`, `E4`, `F0`–`F5`, `F8`–`FA`. Everything else is the no-op `0x8002CF98` |
| `0x80049AA8` | `seq_cmd_handlers` | 96 handlers for sequence commands `0xA0`–`0xFF`. The code indexes it with the raw byte from base `0x80049828` |
| `0x80049C28` | `seq_note_lengths` | 11 `u16`: `0xC0C0, 0x6060, 0x3030, 0x1818, 0x0C0C, 0x0606, 0x0303, 0x2020, 0x1010, 0x0808, 0x0404` |
| `0x80075F28` | `instrument_table` | 128 × 64-byte instruments (= `INSTR.DAT` 8,192 bytes). Field `+0` is the SPU sample address (`0x1010` for entry 0, matching `INSTR.ALL`'s header) |

The command table is presumably what the field opcodes `AKAO`/`AKAO2` feed; command `0x10` is the first live
handler.

### Sequence format

Each channel has a state struct whose `+0x00` is the read pointer. Each step of `seq_step_channel`:
1. Read byte `b` and advance the pointer.
2. If `b >= 0xA0`, call `seq_cmd_handlers[b - 0xA0](channel, …)`. Repeat until a note, or until `b == 0xA0`, which
   ends the channel.
3. Otherwise `b` is a **note**: `pitch = b / 11`, `length = seq_note_lengths[b % 11]` (the low byte is the tick count:
   192 = whole, 96, 48, 24, 12, 6, 3; triplets 32, 16, 8, 4). Pitch 0–11 is a semitone in the current octave;
   `0x84`–`0x8E` (pitch 12) is a **tie**; `0x8F`–`0x9F` (pitch 13+) is a **rest**.
4. With channel flag `+0x38` bit 3 set, the semitone instead indexes a 5-byte-per-key map at `channel+0x14` and picks
   a per-key instrument from `instrument_table`: drum-kit mode *(name is our guess)*.

Argument lengths come from how far each handler moves the read pointer, confirmed by `tools/akao.py check`. **All 98
distinct songs (1,340 channels) and all 1,423 sound-effect streams on the three discs parse cleanly** with this
table, no unused command is hit, and about 95% of streams end exactly where the next one starts. Channels may fall
through into the next channel's bytes (shared tails), and some songs leave trailing bytes after the last channel.

| Args | Commands |
|---|---|
| 0 | `A0` end · `A6` `A7` octave ± · `B3` `B6` `BA` `BE` `C2`–`C7` · `C8` loop point · `CA` loop back (stops linear parsing) · `CB`–`CD` `D0` `D1` `D4`–`D7` `DB` `ED` `F3` `F5` `F9` |
| 1 | `A1`–`A3` `A5` octave · `A8` volume · `AA` `AC`–`AF` `B1` `B2` `B5` `B7` `B9` `BB` `BD` `BF` `C0` `C1` · `C9` repeat count · `CE` `CF` `D2` `D3` `D8`–`DA` `DC` `F2` `F6` `F8` |
| 2 | `A4` `A9` `AB` `B0` `BC` `DD`–`DF` `E8` `EA` `EC` · `EE` jump (stops linear parsing) · `F4` `F7` `FD` `FE` |
| 3 | `B4` `B8` `E9` `EB` `EF` `F0` `F1` |
| — | `E0`–`E7` `FA`–`FC` `FF`: unused; they go to `seq_cmd_unknown`, which just calls the `A0` handler |

- **Lengths rest on handler code alone** for `B7`, `BE`, `BF`, `CE`, `CF`, `DF`, `E9`, `EB`, `EF`, `F1` and `F3`,
  which never occur on the discs. Every other length is confirmed by real data.
- **Never used in songs:** `AC B0 B3 B7 BB BE BF C4 C5 CB CE CF D2–D7 D9 DA DB DF E9 EB EF F1 F3` and the unused
  `E0–E7 FA–FC FF`. `DA` (glide) appears only in sound effects. The player handles every command a song uses; `FE`
  has no audio effect.
- **Read in detail:** `A0`, `A1`, `A2`, `A5`–`A7`, `C8`–`CA`, `DC`, `E8`, `EE`, `F0` (also confirmed by the MIDI loop
  check); `A3`, `A4`, `A8`, `A9`, `AA`, `AB`, `C0`, `C1`, `D8`, `D9`, `DA`, `EC`, `ED` (volume, drums, pitch);
  `B4`–`B6`, `B8`–`BA`, `BC`, `BD`, `DD`, `DE`, `FD`, `FE` (LFOs and bars); `AD`–`AF`, `B1`, `B2`, `CC`, `CD`, `D0`,
  `D1` (articulation and envelopes). The rest are not studied yet.

**Basic commands**
- **`A5 n` / `A6` / `A7`:** set the octave (`+0x66`) / raise it by one / lower it by one, masked to 0–15.
- **`A3 n`, `A8 n`:** store a byte into a channel field and flag it for an SPU update; `A8` stores it sign-extended
  `<< 23` into `+0x44` (volume; see below).
- **`FD t n`: time signature** (`0x80033548`). Writes ticks per beat (`+0x5A`) and beats per bar (`+0x56`) into the
  song state and resets the beat and tick counters, so the bar starts where it stands. `30 04` (4/4) occurs in 132
  places; also 3/4, 2/4, 5/4–8/4 and 3/8–10/8 (`18 n`). Song 76 alternates three bars of 6/8 with one of 5/8; song
  71 runs through 6/4, 7/4 and 8/4; songs 1, 66 and 89 have none.
  - Most songs open with a 1–4 tick rest, then `FD` + `FE 01 00`, so their bar lines sit a few ticks after tick 0.
    Some open with a pickup bar (song 47: `FD 30 01`).
- **`FE lo hi`: bar number** (`0x80033588`), a `u16` at `+0x5E`; a counter for the game, usually `01 00` at bar 1.

**Articulation**
- **A tie after a rest** keys nothing on: neither the tie path nor the rest path sets the key-on mask, so `n r t` is
  a note and a longer rest (song 12's organ chords, channels 0–2, `n0/8 r/7 t/7`, stop at the rest). A pending `A4`
  bend is still spent on that tie, on the silent voice.
- **`CC` slur / `D0` full length** (`0x80032D44` / `0x80032D58`) set the note mode `+0x6E` to 1 / 4. At each note or
  tie, bit 1 takes bit 0, and a note with bit 1 set skips key-on: it takes over the sounding voice with a new pitch
  and no new attack, so `CC n1 n2` slurs n1 into n2.
  - Only the key-on mask is skipped (the slur branch `bnez .L80031454`). The new pitch, the restart of vibrato,
    tremolo and pan LFO (current settings, delay included) and the zeroed vibrato and bend offsets all still happen.
    So a long slurred line under a delayed vibrato restarts the delay on every note, and a depth change right before
    a slur takes effect (effect 274 raises its vibrato from `0x83` to `0xA3` that way). 53 songs and 176 effects
    slur with an LFO on; song 97 does it most (2,887 notes). The player does the same.
  - `CD` and `D1` (`0x80032D50`, `0x80032D64`) are empty. The mode ends in `seq_look_ahead` (`0x800318BC`), which runs
    before each note and scans to the next note, following `C9`/`CA`/`EE`/`F0` like the sequencer: `CB`, `CD`, `D1`
    or `DB` clear bits 0 and 2 and it keeps scanning; a rest, `A0` or an unused command clears them and stops. So
    `CC n1 n2 CD n3` holds n1 into n2, keys n2 off early, and starts n3 fresh.
  - `CC`/`CD` appear 1,601 times in 56 songs, and 12,039 of 156,161 notes are slurred. `D0`/`D1` appear 97 times in
    14 songs.
- **`AD`, `AE`, `AF`, `B1`, `B2`: the channel's envelope** (`0x80032E08`…`0x80032F98`): attack rate, decay rate,
  sustain level, sustain rate and release rate (`+0xFA`…`+0x102`), with a voice-update flag so they reach a sounding
  note. `load_channel_instrument` resets all five, and the three modes, from the instrument on every program change
  (the drum-kit path per note). `AD` is in 47 songs, `B1` in 35, `AF` in 7, `AE` in 5 and `B2` in 1: 476 overrides.
  The mode commands `B7`, `BB` and `BF` never occur.
  - The SPU waits a number of samples per envelope step that grows with the rate, up to 2^20 samples (about 24 s) at
    sustain rate `0x7F`, the "hold" most instruments use. The player keeps the wait already spent as a fraction of a
    step, which a new rate continues, so a mid-note change (effect 448's `B1 3E`) or a slow release after a held
    sustain plays out in full (release rate 12: about 180 ms). 50 songs and 193 effects change an envelope rate on a
    cell without a note.

**Extra voices and FM**
- **`F4 a b` / `F5`: layer** (`0x80031D6C` / `0x80031E98`). `F4` reserves a free SPU voice for the channel (`+0x24`,
  flag `0x100`), loads instrument `a` for the channel and `b` into that voice's channel slot, and every key-on keys on
  both. The envelope commands write both, and `seq_layer_voice_update` (`0x8002F738`) copies the main voice's pitch
  register (`+0xF8`) to the layer, vibrato included. `F5` frees the voice.
- **`F6 n` / `F7 t n`: layer balance** (`0x80031EEC` / `0x80031F30`): `+0xC6 = n << 8`. The main voice plays at
  `(127 − n) / 256` of its volume and the layer at `n / 256`; `F7` slides the balance to `n` over `t` ticks (0 = 256).
  Only song 10 uses it, channels 1–5: instruments 84 and 83, crossfaded.
- **`F8 n`: overlap** (`0x80033128`). Sets the release rate to `n` and, on first use, reserves a second voice for the
  channel (`+0x28`, flag `0x200`). Every key-on then toggles flag `0x400` in the voice update (`0x8002FA74`), so notes
  take turns between the two voices and each rings on in its release while the next starts, like a plucked string.
  With `0x200` set, a program change keeps the release rate (the update mask `0x1BB80` leaves it out), and so does a
  drum-kit note.
  - `F9` (`0x800331CC`) frees the second voice and takes the release rate from the instrument again.
  - 48 uses in 18 songs, almost all `F8 0C` (about a 2-second ring), e.g. song 96's plucked part (channels 11–13,
    instrument 21 with `AD 20` and `B1 33`). `F9` is used once, in song 2.
- **`C6` / `C7`: FM on / off** (`0x80032BB4` / `0x80032C20`) set and clear the channel's bit in the song's
  pitch-modulation mask (`+0x34`), which `snd_apply_fm_voices` hands to the SPU. Used in songs 53, 66 and 89. The
  SPU's rule (psx-spx): step = pitch register × (previous voice's output after its envelope, + 0x8000) >> 15, then
  anything over `0x3FFF` becomes `0x4000`. The cap comes after the modulation, so effect 270's FM voice (register
  `0x2B2E`) never steps past `0x4000`.

**Voice updates**
- **Pitch register: masked, not clamped.** Every voice update writes the pitch as `& 0x3FFF` (`0x8002F1D0`,
  `0x8002F23C` for songs; `0x8002F6AC`, `0x8002F728` for effects), and the setter at `0x800395C8` copies that straight
  into the voice, so a pitch past `0x3FFF` wraps round to a low one. The player does the same; the XM export keeps the
  unwrapped pitch.
  - Songs 2, 9, 53 and 72 and 87 effects have such notes. Song 2's channel 10 plays instrument 30, whose table starts
    at `0x6590` for octave 6, so its key-85 notes (`0x6BA0`) play at `0x2BA0`.
- **Key-off freezes the voice.** A key-off clears the voice's bit in the update mask (songs: `seq_song + 0xC`, set at
  key-on in `seq_step_channel`, cleared in `seq_tick`; effects: `0x80099FD4`), and `seq_update_voices` runs the voice
  update (`seq_channel_lfo` / `seq_effect_lfo`) and `snd_update_voice` only for voices in that mask. Through the
  release the SPU keeps the pitch, volume and pan it was last given: bends, volume slides and LFOs stop where the
  key-off found them. FM still applies, since the chip does that itself. The player does the same.
- **Song start** (`snd_start_song`): tempo `0xFFFF` (`D_8009A11E`) and accumulator 1 (`D_8009A124`), so before the
  first `E8` a song runs at about a tick per timer interrupt. Song 50 sets its tempo at tick 4, song 85 at tick 72.

**Reverb.** Most instruments have release rate 5, linear, about 1.5 ms, so notes mostly die at key-off; the tail you
hear in the game is SPU reverb.
- **Mode:** the song header's `u16` at `+8`. The song-start code (`snd_submit_request`) passes it to
  `snd_set_reverb_mode` (`0x80029AF0`), which acts only when it changes: it turns reverb off, calls
  `SpuSetReverbModeParam` with mask 1 and mode `| 0x100` (clear the work area), and turns it back on. 45 songs use 1
  (Room), 40 use 3 (Studio B, medium), and 13 use 4 (Studio C, large).
- **Presets** (libspu's tables in the exe): `spu_reverb_work_starts` (`0x8004AB5C`) is ten `u32`s, the work area
  start per mode in 8-byte units (Room `0xFB28`: 4,960 samples; Studio B `0xF6F8`: 9,248; Studio C `0xF204`: 14,320).
  `spu_reverb_presets` (`0x8004ABAC`) is ten 0x44-byte entries: a `u32` mask, then the 32 registers
  `0x1F801DC0`–`0x1F801DFE`. All three presets FF7 uses take `vLIN`/`vRIN` = `0x8000` (−1).
- **Depth:** `EA lo hi` (`seq_cmd_reverb_depth`, `0x80031BA0`) sets it to `(hi << 8 | lo) << 16`. It's zeroed at song
  start and used in 93 songs, mostly `EA 00 28` (`0x2800`). `seq_update_voices` takes the top half (`0x8009A146`),
  scales it by the game's sound setting `0x80062FB8` (0 at boot: unchanged) and pans it by `0x80062F70` (`0x40` at
  boot: both sides equal). `SpuSetReverbDepth` writes it as the reverb output volume (`vLOUT`/`vROUT`), so `0x2800`
  = 0.31.
- **Per channel:** `C2` turns a channel's reverb send on and `C3` off (1,876 and 59 uses). Song 42 turns it on for
  all 16 channels.
- **In the player** (`Reverb` in `tools/akao_synth.py`): psx-spx's formula on a copy of the work area at 22.05 kHz,
  resampled to and from the output rate; the sends are the voices' panned outputs, as on the SPU.
  - The SPU halves the rate on the way in and doubles it on the way out through a 39-tap half-band low-pass (checked
    against Mednafen). Ours is a windowed sinc (Kaiser, β 13, cut off at 11.025 kHz), within 0.1 dB of the hardware
    up to 12 kHz, every coefficient within 13/32768. Bright, aliasing voices such as effect 270's FM voice come out
    clean through the reverb, as in the game.
  - It works a stretch of up to 512 samples at a time with all reads before the writes, which matches the
    sample-by-sample formula while the stretch is shorter than the shortest write-to-read delay. Each filter's
    feedback from its own last output is computed as a filter. A test checks it against a plain per-sample version
    to within 1e-9. Cost: about 2% CPU for FF7's presets; Echo and Delay (4-sample delays) run slowly, but no song
    uses them.
  - Over 20 s, reverb adds 4–17% RMS: song 42 (Room) 0.086 → 0.089, song 96 (Studio B) 0.047 → 0.051, song 82
    (Studio C) 0.067 → 0.078.
  - Known gap: Room's two "diff" filters both write offset 0, so the SPU's left filter reads the right one's last
    output; ours uses its own. Nothing reaches the output from there (`vCOMB3`/`vCOMB4` are 0).
- **Aliasing in the voices themselves is real.** Above 44.1 kHz the SPU just skips samples, and the 4-point Gaussian
  is the only filtering (psx-spx); neither the driver nor Mednafen adds any. FM voices that swing up to the `0x4000`
  step limit alias on the console too, and their reverb send softens that.
- **Impulse responses** (`tools/reverb_ir.py`): the reverb is linear, so a click through it records each type for
  convolution reverbs. Levels are at full depth. Tails, trimmed where they fall below 24-bit: Room 1.65 s, Studio A
  2.00 s, Studio B 2.07 s, Studio C 4.76 s, Hall 4.02 s, Space Echo 8.15 s, Echo 11.87 s, Delay 0.74 s (a single
  repeat) and Pipe 3.94 s.

### Timing, loops and MIDI export

- **Clock:** `snd_init` (`0x8002988C`) sets up root counter 2 with target `0x43D1` and `snd_timer_tick`
  (`0x80030234`) as its interrupt. Counter 2 runs at system clock ÷ 8 (the driver's own catch-up code counts elapsed
  scanlines in units of 66), so the driver steps about **243.86 times a second**.
- **Tempo:** each step, `seq_tick` (`0x800308D4`) adds the song tempo (the `u16` from `E8 lo hi`, kept at
  `seq_song + 0x18`) to a 16-bit accumulator, and every carry is one sequence tick. So
  ticks/s = 243.86 × tempo / 65,536, and with 48 ticks per quarter note, **BPM = tempo × 0.004651**. The 96 songs that
  set a tempo span 56–191 BPM (median 106).
- **Per-channel counter:** `+0x56` holds `gate << 8 | length`, loaded from `seq_note_lengths` (`0x3030` = 48, 48), and
  `seq_tick` takes `0x101` off it each tick. Length 0 steps the channel to its next event; gate 0 keys the voice off.
  - For an ordinary note `seq_step_channel` subtracts `0x200`, so the voice keys off 2 ticks before the next event.
    Notes of 2 ticks or less never key off early, and a note keeps its full length when the next byte is a tie, or
    in slur or full-length mode.
- **Length commands:** `A2 n` makes the next note last `n` ticks and clears the fixed length. `DC d` sets a fixed
  length for every following note, equal to the last length + signed `d`, clamped to 1–255; `d` = 0 turns it off.
  Precedence: fixed length, then `A2`, then the length table.
- **Loop commands:**
  - **`C8`:** pushes a loop point (4 levels).
  - **`C9 n`:** jumps back until pass `n` (0 = 256), then pops. So effects 294 and 369 end after 256 passes, and
    effect 396 goes on to the notes after its repeat.
  - **`CA`:** jumps back forever.
  - **`F0 n lo hi`:** jumps by a signed 16-bit offset on pass `n` of the current repeat, for a different ending on the
    last pass.
  - **`EE lo hi`:** jumps by a signed 16-bit offset, counted from after its arguments; a backward jump is the song's
    endless loop.
  - **Look-ahead counts:** `seq_look_ahead` compares the raw `C9` count, so at a `C9 00` it always assumes another
    pass; our `look_ahead` does the same on purpose. The driver also compares `F0`/`F1` counts raw and follows `EF`
    and `F1`, where ours reads `F0 00` as 256 and stops at `EF`/`F1`. No song or effect uses `F0 00`, `EF` or `F1`.
- **Instrument commands:** `A1 n` selects instrument `n` and frees an `F4` layer's second voice. `F2 n` also changes
  instrument; both rescale a sounding note's pitch from the old instrument to the new one.
  - **`F2` plays from the loop point** (`0x80032274`): it sets the voice's sample start to SPU `0x76FE0` instead of the
    instrument's and keeps the instrument's loop address. `snd_init` uploads 32 bytes there from `0x8004A60C`: two
    silent ADPCM blocks, the second flagged end + repeat. So every note after `F2`, until the next `A1`, plays 56
    silent samples and jumps to the loop, skipping the sample's attack.
  - 21 songs use `F2` (42, 47, 48, 49, 71, 78, 80–82, 84, 85 and others), all on notes; no effect does. The export
    marks it `from_loop`; the player skips the 56 silent samples.

`python tools/akao.py midi <file|dir> <out>` writes one Standard MIDI File per song:
- **Layout:** format 1 at 48 PPQ, a conductor track with tempos plus `loopStart`/`loopEnd` markers, and one track per
  channel (MIDI channel = index mod 16).
- **Notes:** key = 12 × (octave + 1) + semitone, so octave 4 = middle C.
- **Controls:** program = AKAO instrument; CC7 = `A8`; CC10 = `AA`; velocity is fixed at 100.
- **Loops:** counted repeats are unrolled, and endless loops play once.
- **Left out:** vibrato/tremolo/pan LFOs, slides, ADSR, reverb, drum-kit mode and the other effect commands.

**Song loop.** Each channel loops on its own (`EE`/`CA` back to its own target), and in 38 songs the channels don't
all start looping at the same tick (song 7: 13 channels loop from tick 768, three from tick 1152). No command ties
them together; the song as a whole repeats from the **latest** channel loop start, every lcm(loop lengths) ticks (in
practice the longest loop): 5,376 ticks for song 7, 4,608 for song 74. `play_song` keeps the earlier channels looping
until that point and cuts everything there, and both exports use it.

**A loop's first pass can play differently.** After `CC` (slur) before a loop, every pass but the first slurs into
its note with no key-on, so the sample keeps running instead of restarting each time (effect 493, a drone). A bend
(`A4`) set before a loop fires on the first pass only (effect 244). The export compares each looping channel's
notes, slurs and bends over three passes; when the first differs and the next two match, the loop starts a pass
later. That moves 14 effects' loops and no song's.

**Check:** in all **98 songs, every looping channel repeats the same length**, or a length that divides it, such as a
one-bar drum loop; echo channels may start a few ticks late. This check is what confirmed `F0`'s meaning. No channel
runs past 4,000 bars.

### Channel defaults, volume and drum kits

- **Channel reset:** `channel_reset` (`0x80029BAC`) starts every channel on instrument 5 (via
  `load_channel_instrument`), octave 2, volume 100 (`+0x44` = `0x32000000`) and an `A3` multiplier of 120
  (`+0x2C` = `0x78`).
- **Voice volume** (`seq_channel_lfo`, `0x8002ED34`): (`A8` volume `<< 7`) × `A3` ÷ 128, plus a tremolo offset
  (`+0xD8`), scaled by the global master volume `0x80062F5E` ÷ 128. That's `0x7F` at boot (`0x800294A4` stores
  `0x7F0000` to `snd_master_volume`).
- **Output mode:** the word at `0x8009A104` (named `seq_song` in our symbols) picks how the volume is split. The
  sound-setting functions at `0x8002C7E8`, `0x8002C81C` and `0x8002C850` set it to 1, 4 and 2; boot sets 1.
  - **1, stereo:** left = volume × `snd_pan_left`[i] >> 15, right = volume × `snd_pan_right`[i] >> 15.
  - **4:** the same, but odd-numbered voices get their right side inverted (`~right`).
  - **Anything else (mono):** both sides = volume × `0x80049CC4` (entry 64 of the left table, 16,256) >> 15.
- **Pan tables:** i = (`AA` pan + pan-LFO offset `+0xDA`) & 0xFF. Entries 0–127 are linear: left 32,640 − 256·i, right
  128 + 256·i. The two always add up to 32,768, so the centre is −6 dB per side, not the −3 dB of equal-power panning.
  Entries 128–255 hold unrelated data (signed squares); only a pan LFO could reach them, and simulating all four
  pan-LFO songs keeps i within 5–114.
- **On the SPU:** a volume register r plays at r ÷ `0x4000` of full scale (psx-spx). So a voice plays at
  `A8` × `A3` × 127/128 ÷ 16,384 × the pan table entry ÷ 32,768.
- **`A3 n`:** the multiplier above. In songs, 1,317 of 2,108 uses are 127, and the rest go as low as 10.
- **`A9 n v` / `AB n p`:** slide volume or pan to `v`/`p` over `n` ticks (0 = 256). The per-tick step sits at
  `+0x48`/`+0xCA` and the counter at `+0x5C`/`+0x62`. `A8`/`AA` cancel a running slide. 266 of the slides fade in
  from volume 0.
- **`EC lo hi` / `ED`:** drum-kit mode on/off (channel flag bit 3). The map pointer (`+0x14`) is a signed offset from
  after the arguments. Each note's semitone picks a 5-byte entry: instrument, key (octave = key ÷ 12, same numbering
  as `A5`), `u16` volume (stored `<< 16` into `+0x44`, so the `A8` equivalent is the value `>> 7`), and pan (`<< 8`
  into `+0x60`). 4,585 notes as written (11,645 once loops are played out) are drum notes.
- **Envelope (ADSR):** when a note starts, the driver copies instrument bytes `+0x08`–`+0x0F` into the voice, and
  `snd_update_voice` hands them to libspu. The packing, checked against the register writes and psx-spx:

| Bytes | Setter | Register bits |
|---|---|---|
| `+0x08` attack rate, `+0x0D` attack mode | `SpuSetVoiceARAttr` (`0x80039850`) | ADSR bits 8–15 (rate = shift `<< 2` \| step) |
| `+0x09` decay shift | `SpuSetVoiceDR` (`0x8003973C`) | bits 4–7 |
| `+0x0A` sustain level | `SpuSetVoiceSL` (`0x800397C8`) | bits 0–3 |
| `+0x0B` sustain rate, `+0x0E` sustain mode | `SpuSetVoiceSRAttr` (`0x800398EC`) | bits 22–31 |
| `+0x0C` release shift, `+0x0F` release mode | `SpuSetVoiceRRAttr` (`0x800399D0`) | bits 16–21 |

- **Mode values:** 1 linear up, 3 linear down, 5 exponential up, 7 exponential down.
- **What the instruments use:** all 93 have sustain level 15. 75 hold at full level (sustain rate `0x7F`), and 18
  fade while held (sustain rate 52–65).

### Pitch: bends, transpose, detune

From `seq_step_channel`'s note and tie paths:
- **Key:** octave × 12 + semitone + transpose. The pitch comes from the instrument's table for that key, then the
  detune is applied.
- **`C0 s` / `C1 s` (`seq_cmd_transpose`, `_add`):** set or add a signed semitone transpose (`+0xCC`). Drum-kit notes
  ignore it.
- **`D8 d` / `D9 d` (`seq_cmd_detune`, `_add`):** set or add a signed detune (`+0xCE`). It scales the pitch by
  1 + d/128 when positive and 1 + d/256 when negative. Song values are mostly −2…+1 (≈ ±14 cents, chorus-style), plus
  −128 (exactly one octave down) and ±60.
- **`A4 n s` (`seq_cmd_bend`):** stores a slide length `n` (0 = 256, at `+0x68`) and a signed semitone amount `s`
  (`+0xD2`). 321 bends in the songs, from ±1 semitone over 4 ticks up to +65 over 8.
  - **Start:** the next **note or tie** starts it. Both reach the same check in `seq_step_channel` (`.L8003161C`,
    after a note's key-on): a non-zero `+0xD2` makes the target key the current key + `s`, and the pitch slides
    linearly *in frequency* to the target's pitch (transpose and detune included) over `n` ticks. The amount is then
    cleared, so later ties hold the new pitch.
  - **Examples:** most songs put `A4` before a tie (song 7 channel 16: a 2-tick note, `A4 04 01`, a 10-tick tie,
    repeated). Effects often put it before a note (effect 1: +15 and +12; effect 36: +32), and so do songs 9 and 66.
  - **Target key:** `+0xD0` (the note's key before transpose) += `s`, plus the transpose `+0xCC` *now*, so a `C1`
    between the note and the bend moves the target. Effect 36's last bend comes after a loop of `C1 02`, so it lands
    2 semitones higher than `s` above the held note (key 90). The export folds this into the bend's semitones.
  - **Effects skip the detune:** with flag `+0x54` set, the target pitch comes straight from the instrument's table,
    without the `+0xCE` detune scaling that songs apply.
  - **Glide:** a `DA` glide writes the same fields on a note, so a pending `A4` bend is lost.
- **`DA n` (`seq_cmd_glide`, `0x8003257C`):** a glide (portamento). No song uses it, only sound effects (43 uses).
  - **Setting it:** glide time `+0x6C` = n (0 = 256) and slur mode on (`+0x6E` = 1, as `CC`). It also clears the last
    key (`+0x6A`) and its transpose (`+0xD4`), so the first note after it doesn't glide from a note played before
    `DA` (14 effects have one).
  - **On each note** (`seq_step_channel`, `0x8003131C`): with a glide time and a last key (`+0x6A`, 0 = none), the
    note starts on the last key and bends to its own over the glide time, through the bend fields `+0x68`/`+0xD2`.
    The last key is then this note's.
  - **Ending:** the look-ahead clears the glide time wherever it ends slur mode. It runs before the glide check, so
    the note just before a rest doesn't glide.

### Sound effects

- **The data:** `SOUND/EFFECT.ALL`. Its table's entries 2·id and 2·id + 1 (`func_8002A6C4`) are an effect's two
  channels, as offsets from `0x1000` (`0xFFFF` = none): 721 effects, 702 with two channels and 19 with one, 1,423
  streams in all. 90 loop forever (ambient sounds).
- **Playing one:** the sound commands around `0x20`–`0x34` (handlers `0x8002B6AC` and `0x8002B8B4`) look up the id
  and hand both streams to `snd_start_effect`. It sets up a pair of effect channels (`0x80096608` + slot × `0x108`)
  with the command's pan and flag `+0x54` = 1.
- **Tempo:** effect channels tick on their own accumulator (`0x80099FE8`) at tempo `0x66A8` (`0x80099FE0`, set at
  boot and never changed): about 97.8 ticks a second, 122 BPM.
- **Commands:** none use `E8` (tempo). They use plenty of slurs, detune, bends, LFOs, envelope overrides, FM (755)
  and reverb sends (1,331 `C2`). There are 29 instruments, all in the boot bank.
- **Our export:** `effects/effect_NNNN.ff7song`, which the player opens like songs
  (`python tools/ff7player.py assets/ff7song/effects`). All 721 render with sound. Half end within 3 s; the longest
  one-shots (115–117) run 7,080 ticks (72 s).
- **Noise (`C4`/`C5`, `0x80032ABC`/`0x80032B30`):** set and clear the channel's bit in the noise mask (effects:
  `0x80099FEC`; songs: `+0x2C`), so the SPU plays its noise generator on that voice instead of the sample. 305 of
  the 721 effects use it (effect 36's first channel hisses, rising).
  - **Clock (`AC n`, `0x80032D6C`):** n sets it (0–63). With either top bit set, it adds n & 0x3F instead, wrapping
    at 64. Effects keep it at `0x80099FFA`, songs at `+0x52`; it starts at 0. Several `AC`s on one tick (effect 36
    adds 1, then sets 46) fold into one cell field in the driver's order: a set drops the earlier adds, and later
    adds go onto it.
  - **The generator** (psx-spx): a 16-bit register that shifts in the inverted parity of bits 15, 12, 11 and 10, at
    most once per 44.1 kHz sample. A timer of 0x20000 >> (clock >> 2) counts down by 4 + (clock & 3) each sample.
    From power-on it repeats every 65,535 shifts. One generator and one clock serve every voice. Sony's docs give
    only the 0–63 range; Mednafen times it differently but gives the same rate.
  - **Rates** (measured in the player, all matching the formula): clock 0 gives 1 shift a second, 32 gives 345, 46
    gives 4,134, 50 gives 8,269, and 56 and up give 44,100. Effect 36's noise runs at 50–62, then settles at 46
    (about 4 kHz of shifts, a dark hiss).
- **Effect-only commands:**

| Command | Action |
|---|---|
| `B0 a b` | Decay rate a, then sustain level b (calls the `AE` and `AF` handlers) |
| `B3` | The instrument's envelope back (all five rates and the three modes) |
| `BB n` | Sustain mode |
| `CB` | All off: vibrato, tremolo and pan LFO (flags `0x01`/`0x02`/`0x04`, offsets left as they are), noise, FM and its countdown, reverb, flags `0x10`/`0x20`, slur and full length |
| `D2 n` / `D3 n` | FM on now (`D2` only), then flipped after n + 1 ticks (0 = 257) by the countdown `+0xA6` in `seq_channel_slides`. `C7` stops the countdown |
| `D4` / `D5` | Flag `0x10`: the voice's pitch register becomes the previous channel's (`−0x108 + 0xF8`) plus its own vibrato and `+0x36`, instead of its own pitch |
| `D6` / `D7` | Flag `0x20`: the voice volume becomes the previous channel's pitch register × 2 × `A3` ÷ 128 instead of `A8` × `A3`, so it follows the other voice's pitch |
| `DB` | Glide off (`+0x6C` = 0). The look-ahead ends a glide there too |

- **`D6`/`D7` in use:** only effects 35 and 65, both with FM. The volume can pass the SPU's 15 bits: the setter at
  `0x800393C8` masks with `0x7FFF` without clamping, so it wraps round to a small negative volume. The player does
  the same.

### How the driver treats effects

Every command in `EFFECT.ALL` is handled. The three without a case need none: `A0` ends a stream, and `CD`/`D1` have
empty handlers because the look-ahead does their work.
- **Flag `+0x54`:** `A0`, `A1`, `AC`, `B4`, `B8`, `C2`–`C7`, `F2` and `F3` branch on it.
  - Most only pick the effect copy of a mask or setting: noise (`0x80099FEC`), reverb (`0x80099FF0`), FM
    (`0x80099FF4`) and the noise clock.
  - `B4`/`B8` take no delay: a non-zero first byte is the depth instead.
  - `A0` hands the voice back to the music without a key-off: it clears the channel from the effect masks
    (`0x80099FCC`, noise, reverb, FM) and sets the music channel's update flags. A note held to the end sounds on at
    its last volume until the music or the next effect takes the voice. Effect 643's last note fades only to volume
    1: its slide's final step to 0 comes on the `A0` tick, after the voice has left the update mask. The player
    keys everything off when a piece ends without a loop, since it has no music to take the voice back.
  - `C6` (`0x80032BB4`, also behind `D2`) sets an effect voice's FM bit only outside the mask `0x555555`, and an
    effect's first channel is always on an even voice, so FM on a first channel does nothing. 25 effects ask for it
    (270, 297, 302 and 447 among them); the player drops those `C6`s too.
  - `F3` sets the flag on a song and is never used.
- **`A1`/`F2` on a sounding note** (songs too): the voice's pitch (`+0x30`) is rescaled by the new instrument's C
  pitch over the old one's (`instrument_table` + 0x10), and the voice keeps its sample until the next key-on.
  Effects 86, 309, 412, 435 and 724 and songs 38 and 72 change instrument between a note and its tie. The player
  rescales the same way.
- **Voice update** (`seq_effect_lfo`, run on effect channels by `seq_update_voices`): vibrato, tremolo and pan LFO
  as for songs, but volume skips the master volume and uses the channel's own scale (`+0xC6` high byte ÷ 128), and
  pitch gets the channel's own scale (`+0x3D`). It steps the effect LFOs itself (songs do it in `seq_channel_lfo`),
  on every interrupt while the voice is keyed on.
- **Per-tick update** (`seq_effect_slides`, where songs run `seq_channel_slides`): the volume slide, the noise and
  FM countdowns (`+0xA4`/`+0xA6`), bends and LFO depth slides. It has no pan slide (`+0x60`/`+0x62`), so `AB` does
  nothing on an effect (effects 112, 113, 117; the export leaves their pan alone). It has no LFO delay countdown
  (`B4`/`B8` start effect LFOs with none) and no layer balance slide.
- **Set by the game at play time, not by the effect data:**
  - the pan: the play command's argument (the player uses the centre);
  - the volume and pitch scales: `func_8002BCCC` sets a pair's volume and `func_8002C2CC` the pitch scale. Boot sets
    them to `0x7F00` and 0 (`0x800294A4`'s loop over the effect channels), which the player plays: 127/128 and no
    change;
  - the reverb: the current song's (the player plays effects dry);
  - stopping the 90 that loop.
- **Left to context:** effect 419 plays noise before setting the clock, so in the game it uses whatever clock the
  last effect left. On its own it starts at 0.
- **Settled by ear:** effects 270, 297, 302 and 643 have no known driver-side cause for sounding odd, and only a
  recording from the game could reopen them.
  - Effect 302's tremolo (`B8 1F 02 00` on the cymbal, ±24% at 61 Hz) sounds wrong, but the driver's math and
    timing for it match ours as far as read, so it stays.
  - Effect 270 was checked step by step, its FM against psx-spx and Mednafen. Its commands come out as the driver
    plays them: a 58-semitone swoop and a +16 bend on the modulator, a 1-tick blip then a long note with a square
    vibrato on the FM voice, and their envelopes.

### Driver cross-check

Every driver function the player depends on is decompiled with Ghidra from `build/main.elf` (our matching build,
with our names) and compared with `tools/akao.py` and `tools/akao_synth.py`: `seq_tick`, `seq_step_channel`, the
look-ahead `func_800318BC`, the per-tick updates (`func_8002E478` songs, `seq_effect_slides` effects), the voice
updates (`func_8002ED34`, `seq_effect_lfo`), `seq_update_voices` and all 85 sequence command handlers.
- **Notes:** the note, tie and rest paths, the key-off gate (table lengths, `A2`/`DC`, slur and full length), the
  look-ahead, glides and drum kits all match: no note goes missing.
- **Argument lengths** match the driver's skip table (`0x800498A8`): `E9` and `EB` take 3 bytes (neither occurs on
  the discs).
- **`A1`** frees an `F4` layer, and **`A2`** sets the length `DC` counts from (no data hits either).
- **Slurred notes and bends:** a slurred note zeroes the bend offset (`+0x34`) but not its countdown (`+0x64`), so a
  running bend carries on from the new note at the same step. The player does the same.
- **Not modelled by the player:** `A8`/`AA` read signed, unmasked bytes (no data goes past `0x7F`); volume and pan
  slides keep 1/128 and 1/256 of a step where the export rounds per tick; `F4`'s voice allocation can fail when all
  24 voices are taken.

### Our song format and player

`tools/ff7song.py` and the player/editor `tools/ff7player.py` (spec: [`ff7song_format.md`](ff7song_format.md)) play
the driver's own behaviour.
- **Patterns are bars**, so any `FD` time signature works. Identical bars share a pattern: 81 of 98 songs reuse some.
  The 98 songs come to 9.8 MB of JSON plus a 1.7 MB bank (93 instruments).
- **Events sit at exact ticks**, and effects keep the driver's parameters.
- **Notes keep their full length across the loop end**, as the driver holds them. A channel whose loop opens with a
  tie keeps the previous pass's last note sounding; the exporter plays one extra pass to count those ties (87 notes
  in 21 songs). A note still held after that pass is a drone, tied on forever with `"len": null`: songs 66
  (channels 1–2) and 75 (channels 8 and 14).
- **Timing** (`tools/akao_synth.py`): 243.86 Hz interrupts and the tempo accumulator; note lengths, bends and LFO
  delays per tick; LFO steps per interrupt; the SPU ADSR at 44.1 kHz, worked out per stretch between interrupts.
  Songs start at the driver's tempo `0xFFFF` until their first `E8`.
- **Articulation:** early key-off, slurs, the channel's own envelope settings, and `F8` overlap (the previous note
  rings on in its release on a second voice).
- **Other driver features:** the layered second instrument with its balance (`F4`–`F7`), the pan LFO (`BC`/`BD`),
  LFO depth slides (`DD`/`DE`), FM (`C6`/`C7`), and SPU reverb with the song's preset, depth and sends (see
  "Reverb"). Song 82 gets instrument bank 2 (`"bank": 2`; see the sound files table).
- **Volume and pan:** the driver's own (see "Voice volume"): stereo mode, the master volume at boot, linear pan
  tables. Boot sets the SPU main volume to `0x3FFF` (full scale, in the common attributes at `0x8009C578`), so the
  console clips a loud mix just as the player does: 10 of 98 songs touch full scale in their first 15 s, at most
  0.031% of samples (song 92).
- **Interpolation:** the SPU's 4-point Gaussian (psx-spx). Each output sample weighs the sample before the position,
  the one at it and the two after, by one of 256 steps between samples.
  - **Weights:** psx-spx lists the hardware's 512-entry table but no formula, so we build our own curve rather than
    copy it: a Gaussian (σ 0.85 samples) under a (1 − (d/2)²)³ window, scaled so each step's four weights add up to
    `0x7F80` as the hardware's do. It comes within 17 of the table's 32,768ths, about −63 dB on white noise. (A
    degree-14 polynomial would match to ±1, but its coefficients would just be the table re-encoded.)
  - **Sound:** it smooths. A signal flipping every sample comes out at 0.4, where linear interpolation passes it
    whole, so high notes and samples pitched far up sound duller than with linear.
  - **Cost:** rendering takes 22–30% of real time on our machine, with no dropouts.

### LFOs: vibrato, tremolo, pan

Read from the command handlers, the note path at `0x80031518` and `seq_channel_lfo` (`0x8002ED34`).

| | Vibrato | Tremolo | Pan LFO |
|---|---|---|---|
| On | `B4 delay rate wave` (`seq_cmd_vibrato`, `0x80032614`) | `B8` (`0x80032804`) | `BC rate wave` (`0x8003298C`) |
| Depth | `B5 d` (`0x80032718`) | `B9` | `BD d` |
| Off | `B6` | `BA` | `BE` (`0x80032A98`) |
| Depth slide | `DD n d` | `DE` | none |
| Offset | `+0xD6` | `+0xD8` | `+0xDA` |

- **Effects:** a channel with flag `+0x54` set (by `snd_start_effect`, never by songs) reads `B4`/`B8`'s first byte
  as the depth instead of the delay.
- **Timing:**
  - Each note restarts the LFOs, and the commands act on the sounding note at once. An `on` restarts its LFO on the
    spot: delay, a step countdown of 1, the waveform's first step, and for vibrato the amount from the note's pitch
    (`+0x30`). `B5` recomputes the vibrato amount at the new depth and `B9`/`BD` store theirs, all keeping the LFO's
    place. An `off` clears the LFO's flag and zeroes its offset; `B6`/`BA` keep the depth.
  - Effect 302 switches vibrato on during a held note and swaps its depth every 24 ticks. Since only `on` restarts,
    the export marks each one with a `vibrato_on`/`tremolo_on`/`pan_lfo_on` field. 85 songs and 61 effects have a
    vibrato `on` in a cell without a note, 6 songs and 48 effects a tremolo one, and 4 songs a pan LFO one; only
    those under a held note change the sound.
  - The delay counts sequence ticks (`seq_channel_slides`). After it, the LFO reads its next waveform step every
    `rate` **timer interrupts** (≈ 243.86 Hz, whatever the tempo; 0 = 256). `seq_tick` runs the voice update
    (`seq_update_voices` → `seq_channel_lfo`) on every interrupt, before the tempo accumulator.
- **Waveforms (`lfo_waveforms`):** 16 pointers to s16 tables that loop back on the marker `0, 0, −n`.
  - 0/1 square ±; 2/3 square 0/+; 4/5 square 0/−; 6/7 sine, 39 steps per cycle (a 40-step sine missing its 4th
    step); 8/9 triangle, 16 steps; 10/11 sine, 20 steps; 12 noise; 13–15 reuse 9/8/9.
  - Even numbers (and 14) fade in over four cycles at ¼, ½ and ¾ scale, then repeat the full-scale one.
  - **12, noise:** 256 steps that repeat from the start (end marker `0, 0, −256`). It isn't a shape, so the exporter
    copies it from the user's executable into the songs that use it (`lfo_noise`). Song 76 runs it as vibrato on
    channels 1–4 (`B4 00 01 0C`, depth 60): the pitch jumps around −24…+45 cents every interrupt. Song 9 selects it
    at depth 0, so it's silent there. The XM export leaves it out.
- **Vibrato:** `+0x7C` = (d & 0x7F) × pitch × (1 if d's top bit is set, else 15/256) ÷ 128. The pitch offset
  `+0xD6` = `+0x7C` × step ÷ 65,536, doubled when positive, is added to the voice pitch.
  - **Usual setting:** `B4 60 01 06` + `B5 0A` (1,412 of the 1,556 `B5`s): after a half note, a 6.25 Hz sine fades
    in to about +8/−4 cents.
  - **Largest:** `B5 9F` in song 66, +3.6/−2.2 semitones at 0.25 Hz.
- **Tremolo:** volume offset `+0xD8` = voice volume × d/128 × step/32,768 (not doubled).
  - **Deeper than `0x80`** it takes the volume below zero, and the driver writes it unclamped. The SPU's volume is
    15-bit signed, so the wave plays inverted, not silent; the player does the same. Effect 643 slides its tremolo
    depth to `0xFF` on a square wave.
  - **Most common:** `B8 00 03 05` + `B9 78` (song 71) cuts the volume by 94% every other 3 interrupts, a 40 Hz buzz.
- **Pan LFO:** offset `+0xDA` = (d ÷ 2) × step/32,768 pan units.
- **Usage in the 98 songs:** vibrato in 93 (1,561 `B4`), tremolo in 11 (26 `B8`), pan LFO in 4, depth slides only in
  song 71.

### Tracker export (XM)

`python tools/xm.py <file|dir> <SOUND dir> <out>` writes FastTracker 2 modules that play the game's own samples. The
exporter is frozen.
- **Grid:** one XM tick = 2 AKAO ticks, so the XM BPM equals the song's BPM (56–191, inside XM's 32–255). Songs
  without bends (83) run at speed 1 (rows of 2 AKAO ticks), songs with bends (15) at speed 2 (rows of 4), since
  portamento only acts on a row's later XM ticks; their notes on the second XM tick get note delay `ED1`. Positions
  round to the grid (±1 AKAO tick), never lengths.
- **Patterns** hold whole bars of the song's `FD` time signatures, as many as fit in two bars of 4/4, and one starts
  at the loop. Once every channel is looping, the song repeats from any later point, so the loop moves to the next
  bar line (26 songs, each checked seamless: the moved stretch repeats exactly one period later). The endless loop
  is the XM restart position.
- **Instruments:** one 16-bit sample per AKAO instrument, with its loop. Bank 1, except song 82, which gets bank 2
  in slots 53 and up.
  - **Tuning:** relative note + finetune = 12·log2(44,100 · pitch[C] / 0x1000 / 8,363) − 12·(6 − base − 4), where
    `base` shifts songs whose octaves exceed XM's 8. One value per sample is enough: the pitch tables are
    equal-tempered to within 0.86 cents.
  - **Envelopes:** each instrument's ADSR is simulated with psx-spx's step rules at 44.1 kHz and sampled into an XM
    volume envelope, timed at the song's first BPM. Steady instruments get attack/decay, a sustain point, then the
    release curve. Fading ones put the sustain fade before the sustain point and turn the release into the fadeout
    (clamped to FT2's 0xFFF). Phases that move less than one XM volume step in 10 s count as flat (sustain `0x7F`
    steps once every 23.8 s).
- **Events:** each channel starts from the driver's defaults (instrument 5, octave 2, volume 100, `A3` 120).
  - Notes carry the voice volume 64 × `A8` × `A3` ÷ 127²; the global master volume and the pan law are left out.
    `AA` becomes `8xx` panning, slides become per-tick volume/pan values, and transpose is added to the key.
  - Drum-kit notes carry their entry's instrument, key, volume and pan. Every one of the 152,284 notes has a sample.
  - Detune: whole semitones go into the note, the rest into fine portamento `E1x`/`E2x` on the note's row (1/16
    semitone steps).
  - Bends: portamento `1xx`/`2xx` (16 units per semitone) on each row's second XM tick, stepping to where the
    driver's frequency-linear slide is by the end of that tick. A bend stops at the next note.
  - Tempo changes go on an extra control channel.
  - **Key-offs:** a note that doesn't run into the next one gets a key-off after the channel's other events, 2 ticks
    before the next note as the driver does. It is skipped where a note already starts in that row, and moves to the
    row's start where the effect column is taken, so bends keep their cells.
- **Vibrato and tremolo are baked per row:** each row gets the average of the driver's LFO over the timer
  interrupts it covers, so the 40 Hz tremolo comes out as its mean level instead of an aliased wobble. Vibrato
  becomes extra-fine portamento `X1x`/`X2x` (1/64 semitone), or `E1x`/`E2x` for steps over 15, in free effect cells
  only (a step whose cell is taken is made up on the next free row). Tremolo becomes absolute values in the volume
  column. `4xy`/`7xy` don't fit: FT2's smallest vibrato depth is ±1/8 semitone, twice the usual setting, its speed
  can't reach 6.25 Hz at most song tempos, and neither effect runs at speed 1.
- **Collisions:** a second note in a cell moves to the next row, and later volume or pan values in a row replace
  earlier ones. A pan change that clashes with another effect moves one row earlier. Still dropped across all 98
  songs: 14 notes (fast runs in songs that need speed 2), 43 detune steps on delayed notes, 13 detune steps blocked
  by pan, and the noise-waveform vibrato on 146 notes (song 76).
- **Not carried:** slurs (every note retriggers), the channel envelope overrides (an XM instrument has one fixed
  envelope), `F8` overlap and `F4` layers (an XM channel plays one note at a time), the pan LFO, LFO depth slides,
  LFO changes in the middle of a note, FM and reverb. The MIDI export doesn't carry bends, detune or LFOs.
- **Checks:** `python tools/test_xm.py assets/xm/*.xm` parses each file back and requires every byte to be accounted
  for. A listening check (2026-10-28, before vibrato/tremolo were added) found instruments, volume balance, drum
  kits, the song loop (song 7), bends and detune right.

### Sound files

Checked against our disc:

| File | Size | Layout |
|---|---|---|
| `SOUND/INSTR.ALL` | 483,232 | 16-byte header: `u32 spu_addr` (`0x1010`), `u32 length` (`0x75F90` = file size − 16), 8 zero bytes; then raw SPU ADPCM sample data uploaded to SPU RAM at `spu_addr` |
| `SOUND/INSTR.DAT` | 8,192 | 128 × 64-byte instruments, copied whole into `instrument_table` by `load_instrument_bank` (`0x800297A4`). Entry: `+0x00` `u32` SPU sample address, `+0x04` `u32` SPU loop address, `+0x08` 8 bytes of ADSR settings, `+0x10` 12 × `u32` SPU pitch for each semitone of octave 6 (other octaves shift it). 93 slots used |
| `SOUND/INSTR2.ALL` | 251,120 | Same header (`spu_addr 0x38560`, length `0x3D4E0`), second bank uploaded over the top part of bank 1's sample memory |
| `SOUND/INSTR2.DAT` | 8,192 | `load_instrument_bank2` (`0x80029818`) copies only the first 75 entries (4,800 bytes) to slot **53** onward (`instrument_table + 0xD40`). 6 used, becoming slots 53–58 |
| `SOUND/EFFECT.ALL` | 51,200 | Sound effects: a `0x1000`-byte table of 2,048 `u16` offsets (`0xFFFF` = empty) relative to `0x1000`, then 1,423 header-less command streams (same command set as music). Identical on all discs |
| `*.SND` (e.g. `MINI/ASERI2.SND`, `ENEMY6/FAN2.SND`) | small | One AKAO block (layout below) |

- **Bank 1** (`INSTR.DAT`, `INSTR.ALL`) is read by LBA from the exe's file table (`instr_dat_file`, `instr_all_file`
  in `src/main/main.c`) and loaded once at boot.
- **Bank 2:** only main-loop mode 15 loads it (`snd_load_bank2`, `0x80011274`: sectors `0x1E4` and `0x25F`).
  `INSTR2.ALL` uploads at SPU `0x38560`, exactly where bank 1's slot 53 starts, so from then on slots 53–127 hold
  only bank 2's six instruments. Song 82 (`FIELD/LASTMAP.DAT`) uses exactly those six plus low slots, while every
  other song that uses slots 53+ needs bank 1's instruments there. So song 82 is the bank-2 song (`BANK2_SONGS` in
  `tools/instruments.py`).

**Samples** are SPU-ADPCM (psx-spx: 16-byte blocks, 28 four-bit samples, 5 prediction filters):
- **Loops in bank 1:** bank 1 samples set no loop-start flags. Their blocks carry flag `0x02` and the last one `0x03`
  ("end, then repeat"), so the loop point comes only from `INSTR.DAT`'s loop address.
- **Converter:** `tools/instruments.py` turns both banks into 99 WAVs (64 looping, with `smpl` loop chunks) plus
  `instruments.csv`. Checks: no sample runs into the next one, no output clips, and a listening check (2026-10-28)
  found the WAVs right.
- **Playback rate:** WAVs play at 44,100 Hz, which is SPU pitch `0x1000`. The game plays semitone *s* of octave 6 at
  44,100 × `pitch[s]` / 0x1000 Hz; the CSV lists the rate for C.

**AKAO block** (in `.SND` files, field maps, `MINI/BGM*.LZS`, `MINI/CHOCOBO.DAT`, `MOVIE/STAFF*.BIN`):
- `0x00` `"AKAO"`.
- `0x04` `u16` song id.
- `0x06` `u16` data length (block = 16 + length).
- `0x08` `u16` reverb mode (libspu's type; see "Reverb").
- `0x0A` 6-byte BCD timestamp, e.g. `96 12 18 22 46 28` = 1996-12-18 22:46:28.
- `0x10` `u32` channel mask.
- `0x14` one `u16` per set bit. Each is relative to the byte after itself, so channel *i* starts at
  `0x14 + 2i + 2 + offset`.

Field maps embed their music as AKAO blocks: 613 of the 703 LZS field `.DAT`s contain the `"AKAO"` magic after
decompression, roughly 600 real sequences to test a sequence parser against.

Field script opcodes that drive sound, as named in Gears' opcode table (the bytes are facts; behaviour is still to be
confirmed in code): `0xDA AKAO2`, `0xF0 MUSIC`, `0xF1 SOUND`, `0xF2 AKAO`, `0xF3 MUSVT`, `0xF4 MUSVM`, `0xF5 MULCK`,
`0xF6 BMUSC`, `0xFC FMUSC`, `0xFD CMUSC`.

On PC, Gears says the PsyQ SEQ player was swapped for a MIDI player. Our `FF7.EXE` has the source paths
`lib\src\sound\midi1.cpp`, `sound.cpp`, `dx_snd.cpp`, `acm.cpp` and `creative\sfutils.cpp`. Nothing in the PC sound
path descends from AKAO, so the PC build is no reference for PS1 sound.

## 5. Engine architecture (Gears, PC-era research)

Gears is the Qhimm team's 2000s write-up of the engine, edited by Joshua "Halkun" Walker. It's an unfinished draft:
several chapters (most mini-games, the platform bug lists) are lorem-ipsum placeholders. What matters for PS1 code:

- **Kernel and modules.** The main exe is a resident kernel: threaded main loop, memory manager, CD loader and
  statically linked PsyQ libs. Field, battle, world, menu and mini-games swap in as overlays. The field module drives
  everything and its script can call any other module; battle can't open the menu.
- **Save map.** All persistent state is one 0x10F4-byte (4,340) block, copied wholesale to the memory card. Field
  scripts reach 5 banks in it, 8- or 16-bit wide, plus 256 bytes of non-saved temporaries. `SAVEMENU` previews only
  the first 80 bytes of each slot.
- **CD loading.** No BIOS file API: reads go by **sector number** through low-level calls, in chunks of up to 8 KiB,
  so the next module can preload while the current one runs. Fits `YAMADA.BIN` being an LBA/size table.
- **VRAM.** Double-buffered framebuffers on the left, CLUTs below them, then field textures and texture caches. Menu
  textures and the font stay resident on the right. 24-bit movies widen the display over the first two texture caches.
- **Menu module IDs** for the field `MENU` opcode (first argument always `0`): Party `0x09`; Form `0x07` (argument
  0 = make a party of 3, 1 = split into 3 groups, 2 = split into 2); Save `0x0E`; Name `0x06` (argument
  0–9 = Cloud … Chocobo); Shop `0x08` (argument = shop number). Other menus are reached only internally.
- **Menu assets.** Some `.MNU` overlays have TIM images appended; `PATYMENU` and `FORMMENU` share external TIMs on
  disc. `WINDOW.BIN` holds the resident window textures and font.
- **Debug rooms.** Field maps named after staff (Kitase, Nojima, Kyounen, …) jump to other scenes; useful entry
  points when tracing field code.

### Original PC source tree

The PC `FF7.EXE` still holds 100 source paths, left over from disabled memory-allocation tracing (Gears lists the
same set). They give the original module and file split, and the staff folders behind PS1 overlay names (e.g.
`battle\yama\` ↔ `INIT/YAMADA.BIN`), useful when naming recovered PS1 files:

| Folder | Files (examples) |
|---|---|
| `c:\ff7\field\src\` | `ad_app`, `ad_bk`, `ad_cdr`, `ad_data`, `ad_human`, `ad_obj`, `ad_tile`, `tutaddr` |
| `c:\ff7\src\battle\` | `battle`, `b3ddata`; `battle3d\` (`char`, `enemy`, `limitbrk`, `stage`, `mdl`, `lmd`); staff dirs `yama\`, `yasui\`, `myoshiok\` |
| `c:\ff7\src\menu\` | `english\loadmenu`, `btlmenu\english\callback` |
| `c:\ff7\src\wm\` | `wmdefine`, `wmfile` |
| `c:\ff7\src\main\` | `main`, `initpath` |
| `c:\ff7\{chocobo,condor,coaster,highway,snobo}\` | per mini-game: `ch_*`, `cd_*`, `psxdata*`, `memory`, `tmd` |
| `c:\lib\src\` | Eidos/Square port library: `graphics\` (DirectX + software renderer, `psx.cpp`, `psxgraph.cpp`), `sound\`, `file\`, `mem\`, `thread\`, `movie\`, `polygon\` |

- `snobo\tmd.cpp` and `highway\psxdata.cpp` suggest the PC mini-games wrap PS1-format data almost directly.
- To regenerate the list, extract `FF7.EXE` from the PC install disc and search it for strings starting with `C:\`.
  Error strings such as `ACCESS.CPP::SOUND_SIDE_SWITCH` survive too.

## 6. Tooling notes

- `tools/extract_disc.py`: stdlib ISO9660 reader for raw 2352-byte images, MODE2 (PS1) and MODE1 (PC). Files with
  any Form 2 sector (movies) are written as raw 2352-byte sectors so video players / jPSXdec keep the interleaved XA
  audio. `-l` lists LBA, size and SHA1 without writing anything. Extracting also writes `files.txt` (first sector,
  size, raw or not, path), the sector index the PC build reads through.
- `tools/unpack.py`: detects and decompresses gzip overlays, plain gzip, BIN-GZ archives and LZS.
  `tools/test_unpack.py` is the LZS self-check.
- `tools/movie.py`: each streamed movie becomes an AVI (MJPEG plus 16-bit PCM) and a `.cam` file (40 bytes per
  frame); each in-memory one a silent 30 fps AVI, found by matching file sizes against `MOVIE_ID.BIN`. The JPEGs hold the MDEC's own dequantized coefficients, with a quantization step per position that divides them
  exactly, so nothing is re-encoded. Checked on `MK8.STR` against ffmpeg's MDEC decoder: the planes agree to ±1
  (decoder rounding), and on `FSHIP2.BIN` and `OPENMV.BIN` too. Audio on `BRGNVL.MOV` is bit-identical to ffmpeg's XA decoder. Emphasized sectors go through a
  50/15 µs de-emphasis (bilinear, within 1 dB of the analog curve to 10 kHz), which brings those movies within about
  1 dB of their PC versions (up to 8 dB brighter before). `tools/test_movie.py` checks the coefficient code table
  and the 16-bit Huffman limit.
- Matching needs the original compiler era: PsyQ's gcc 2.x for mipsel. The open-source matching tools (splat,
  maspsx, m2c, asm-differ) are separate projects; check each license before vendoring. rabbitizer (MIPS/GTE
  disassembler) is MIT.
- References: psx-spx for the hardware (SPU, GPU, GTE, CD-ROM, MDEC; SPU ADPCM and register layouts); the Sony PsyQ
  SDK docs (`Libref.pdf`, `Filefrmt.pdf` for TIM/TMD/VAG/VAB). Which libraries the game links: §8.

## 7. Toolchain: what built the game, and what our SDK has

### What built the game (checked on `SCUS_941.63`)

- Linked PsyQ libraries carry CVS stamps from 1995–96: `libetc intr.c 1.73` (1995-11-10), `libcd bios.c 1.76`
  (1996-04-03), `libgpu sys.c 1.115` (1995-11-29), i.e. roughly PsyQ library 3.3–3.5.
- Game code shows the SN assembler's signature: indexed global access expands through `$at`
  (`lui $at,hi; addu $at,$at,$a0; lw $v0,lo($at)`), load-delay `nop`s are inserted, and branch delay slots are
  filled only from instructions *before* the branch, never copied from the branch target.
- Every file builds with `-O2`. Per file, as matched byte for byte (the `Makefile` sets these):

| File | Range | GCC | ASPSX | `-G` | maspsx extras |
|---|---|---|---|---|---|
| `main.c` | `0x8001117C`–`0x80013C9C` | 2.6.3 | 2.21 | 0 | `--expand-div` |
| `transition.c` | `0x80013C9C`–`0x800144D8` | 2.7.2 | 2.34 | 0 | |
| `loader.c` | `0x800144D8`–`0x80014A00` | 2.7.2 (2.6.3 also matches) | 2.34 | 8 | `--use-comm-section -G8` |
| `bitops.c` | `0x80014A00`–`0x80014A84` | 2.7.2 | any 2.21–2.79 (2.79 set) | 0 | |
| `sound_init.c` | `0x800293D0`–`0x80029A4C` | 2.6.3 | 2.21 | 8 | `--expand-div --use-comm-section -G8` |
| `sound.c` | `0x80029A50`–`0x80033A90` | 2.6.3 | 2.21 (or older) | 0 | `--expand-div` |
| `cd.c` | `0x80033A90`–`0x80034CF0` | 2.6.3 | 2.21 | 0 | `--expand-div` |
| `movie.c` | `0x80034F3C`–`0x8003623C` | 2.6.3 | 2.21 | 0 | `--expand-div` |

- **How the versions show:**
  - GCC 2.6.3 over 2.7.2: `func_800135C0` (2.7.2 keeps the quotient in another register); `seq_cmd_loop_point`
    reloads the loop level after storing it and `seq_cmd_jump` adds into a different register (2.7.2 gets both
    wrong). Every sound function matched also matches with 2.6.3.
  - GCC 2.7.2 over 2.6.3: `bitops.c` (2.6.3 matches one of its three functions); `transition.c` (under `main.c`'s
    toolchain a loop counter reset isn't duplicated as the game has it).
  - ASPSX 2.21: `li 1` is `ori` (2.34 and older), and an indexed access goes through `addiu $at` (before 2.30):
    `lui at; addiu at, at, %lo(sym); addu at, at, v1; lw v0, 0(at)`.
  - ASPSX 2.34: `li` is still `ori`, but an indexed load folds into `%lo`: `lui at; addu at, at, a0; lw v0,
    %lo(sym)(at)`.
  - `--expand-div`: every `/` carries ASPSX's divide-by-zero and overflow checks (`break 7`, `break 6`), which maspsx
    only adds with this flag.
  - `-G8`: small globals reached through `$gp`. In the driver, `0x800293D0`–`0x80029A4C` use `$gp`; from `0x80029A50`
    on, small globals like `0x80062F00` are reached with `lui`.
  - The ASPSX version makes no difference for simple leaf functions (`bitops.c`); it shows with `div`, `li`,
    indexed access or `$gp`.

### The PsyQ 4.4 SDK (runtime library 4.4, "DTL-S2350")

| Tool | Version | Runs on Win10 x64? |
|---|---|---|
| `pssn/bin/CC1PSX.EXE` | GCC **2.8.1** SN32 build 4.0.0010 | Yes (Win32) |
| `pssn/bin/ASPSX.EXE` | ASPSX 2.79 | Yes (needs absolute paths) |
| `pssn/bin/PSYLINK.EXE` | Psylink 2.73 (`/p` = raw binary at `/o` org) | Yes |
| `pssn/bin/DOS/CC1PSX.EXE` | GCC **2.6.3** (go32 DOS) | No, needs DOSBox |
| `pssn/bin/DOS/ASPSXD.EXE` | ASPSX 2.79D (DOS) | No, needs DOSBox |
| `psx/lib`, `zips/Lib44`, `zips/Lib45` | libs 4.4/4.5, stamps from 1997–98 | n/a |

- **Win32 `CC1PSX` → `ASPSX` → `PSYLINK /p`** on `80014A00`–`80014A80` gives the game's instruction selection and
  register allocation, but not its bytes: GCC 2.8.1 fills a loop branch's delay slot with a copy from the branch
  target where the game has a `nop`, and `-fno-delayed-branch` leaves every slot empty (ASPSX 2.79 doesn't fill
  them). It also omits an empty 8-byte stack frame the game has *(probably the compiler version)*. Useful for quick
  "same instructions?" checks only.
- **The DOS GCC 2.6.3** is the right version family but needs DOSBox; untested.
- **The libraries can't be used** for matching: they're two years newer than what's linked. Library code in the exe
  stays as bytes from the user's own disc, which also keeps Sony code out of the repo.
- **Headers** (`psx/include`, `LIBSPU.H`, …) and `Devrefs/Libref.pdf` give the library signatures and structs, for
  naming the library functions in the exe.

### Our build toolchain (open, runs in WSL/Linux)

`tools/setup_toolchain.sh` fetches everything into the git-ignored `toolchain/` folder; nothing is vendored.

| Piece | Source | License | Role |
|---|---|---|---|
| GCC 2.6.3 / 2.7.2 "Sony Playstation" `cc1`, `cpp` | decompals/old-gcc release `0.17`, SHA256-pinned in `tools/toolchain.sha256` | GPL (GCC) | Compiler |
| maspsx | mkst/maspsx at commit `4ffa8b51` | MIT | Rewrites GCC output the way SN's ASPSX would: delay slots, `$at` expansions, `nop`s |
| GNU binutils (`mipsel-linux-gnu-*`) | Ubuntu package `binutils-mipsel-linux-gnu` | GPL | Assemble, link, objdump |
| decomp-permuter | simonlindholm/decomp-permuter, cloned into `toolchain/` (ignored) with its own WSL venv | MIT | Tries random rewrites of one function and keeps those that compile closer to the game |

- **Pipeline:** `cpp` → `cc1 -O2 -G0` → `maspsx --aspsx-version=<per file>` → `as -march=r3000 -G0 -O1` → `ld` at
  the exact address → `objcopy -O binary`.
- **`tools/match.sh <file.c> <addr> [gcc]`** runs it and prints `MATCH` or diffs the disassembly against your exe.
  Per-file settings go in the environment: `ASPSX=2.21 MASPSXFLAGS=--expand-div tools/match.sh <file.c> <addr> 2.6.3`
  for a `sound.c` function. It links against `config/symbols.main*.txt`, splat's `build/undefined_*_auto.txt` and
  every symbol in the last full build, so a function that calls another or reads a global can be checked alone.
- **Link address:** `ld` aligns `.text` to 16 bytes, so link with an explicit section address and `SUBALIGN(4)`
  (`match.sh` does).
- **Preprocessor:** the period `cpp` is 32-bit and fails with "Value too large for defined data type" on any
  `#include` from a Windows drive under WSL (it can't `stat` the large inode numbers). The build uses the host
  `cpp -P -undef -nostdinc` instead, which doesn't change the generated code.
- **Permuter:** `tools/permuter.sh NAME SOURCE.c` sets up `build/perm/NAME`: the preprocessed C, the game's asm
  assembled as `target.o`, and a `compile.sh` with the file's flags (`DIR`, `GCC`, `ASPSX` and `MASPSXFLAGS` select
  them; the default is `sound.c`'s). Run it with the venv's `bin` on `PATH` (command in the script's header). Treat a
  find as a hint and rewrite it by hand in the file's style.
- **Full rebuild:** splat 0.50.1 splits `SCUS_941.63` per `config/main.yaml` and `make` relinks it, SHA1-identical to
  retail. All 47 boot functions, `bitops.c` and 231 of the sound driver's 232 functions are C; the rest is assembly.

**Main exe layout** (splat's guess, corrected by hand):

| File offset | VRAM | Contents |
|---|---|---|
| `0x0000` | n/a | PS-X EXE header (t_addr `0x80010000`, t_size `0x60800`, pc0 `0x800110C0`) |
| `0x0800`–`0x18B8` | `0x80010000` | read-only data (jump tables, strings such as `"battle.x"`) |
| `0x18B8`–`0x394FC` | `0x800110B8` | code. splat guessed `0x18AC`, but the 12 bytes there are the string `" (%08x)\n"` |
| `0x394FC`–`0x61000` | `0x80048CFC` | data. splat guessed `0x80048D18`, inside the sound files' disc table that opens it |

- **No padding between sections:** the retail link packs them back to back, so splat needs
  `ld_align_section_vram_end: False` and `ld_align_segment_vram_end: False`, or everything after the read-only data
  shifts by 8 bytes.
- **BSS size isn't set yet.** BSS isn't in the file, so it doesn't affect the hash.
- **A function's jump tables must sit in its `.rodata` subsegment** (`.rodata main/main` for `0x80010000`–`0x80010100`,
  a `.rodata` piece at `0x80010394` for `seq_look_ahead`'s `switch`), so splat moves each table into its function's
  `.s` file. Left in the plain `rodata` piece, the tables point at labels local to the C file and the link fails.

### Native build (the sound driver on the PC)

`make native` compiles `sound.c` and `sound_init.c` with the host GCC (64-bit, `-DNATIVE -DNON_MATCHING`) and links
them with `native/snd_native.c`: libspu and kernel stubs, a loader, and a song runner that prints each sequencer
tick's key-ons. `tools/native_check.py` plays songs on it and compares every channel's key-on ticks with
`tools/akao.py` (an F4 layer or F8 overlap voice counts as its channel's, since `akao.py` reports per channel). Songs 2
and 3 (`FIELD/MD1STIN.DAT`) match on every channel over 4000 ticks; a whole-disc run (every field file decompressed in
Python first) is slow and unfinished.

Rules for C that builds both ways and still matches:
- **A value that can hold an address is `long`** (`SoundCommand.arg`): 32 bits on the PS1, pointer-sized on the PC.
  `int` truncates the song and effect pointers.
- **The data stays in the user's exe.** The runner loads `SCUS_941.63` into `snd_ram` (2 MB standing for
  `0x80000000`), and `tools/native_syms.py` writes a linker script placing every data symbol the driver uses at its
  game address there. Code that treats neighbouring globals as one block (`seq_song` copied into `seq_song2`, the
  twelve words of effect state from `effect_voices`) works unchanged, and no game table enters the repo.
- **Objects that hold pointers are real C definitions** in `snd_native.c` (the channels, the command queue and
  request, the two handler tables, `lfo_waveforms`, the stream and effect-file pointers): a PC pointer doesn't fit in
  4 bytes of `snd_ram`. The game's address tables are translated at start-up via `funcs.c` (also from
  `native_syms.py`), which gives each driver function's game address.
- **Structs with pointers grow on the PC** (`SeqChannel` isn't `0x108` bytes there). The driver copies them as
  `sizeof(...) / 4` words, which still works.
- **A function still in assembly needs its C under `#ifdef NON_MATCHING`** (`snd_start_lasting_effect`); `NATIVE`
  turns `INCLUDE_ASM` off.
- **`-G8` tentative definitions stay common** (`-fcommon`), so they resolve into `snd_ram` as they resolve to the data
  section's labels on the PS1. The link is `-no-pie`, since those addresses are absolute.

### Native build: the CD layer

The PC build (`make pc`, below) compiles `cd.c` over `pc/cd_native.c`, a libcd that reads the extracted discs:
- **By sector, as the game does:** the exe's file tables give sectors, and `load_disc_table` reads sector `0x266`
  (`INIT/YAMADA.BIN`, 80 bytes, read as a whole sector). `files.txt` maps each sector to a file; past a file's end and
  outside every file a sector reads as zeros. Raw movie files give each sector's 2048 data bytes.
- **No disc swapping:** the drive always holds the disc the save is on (`0x8009D588`, disc 1 before there's a save),
  so `\MINT\DISKINFO.CNF` always matches it and `main` never loads `FIELD/DSCHANGE.X`. `cd.c` and `main.c` are
  unchanged; the PS1 build keeps its disc check.
- Reads finish at once and the drive is always ready, so `cd.c`'s retry paths don't run. The hand-written streaming
  decoder (`lzs_stream_start`/`lzs_stream_resume`) has a C version there: 9 sectors of `lzs_stream_src` per call,
  leaving `lzs_stream_src` where it stopped. `cd.c` points it at each new read; `ENDING.X` unpacks `MOVIE/OPENING.BIN`'s
  pictures from memory a frame at a time and relies on the saved position (without it the opening's background broke
  up after its first 9 sectors).
- The movie player's hooks do nothing on the PC yet: playing a movie also needs libpress and a renderer.
- `build/pc/test_cd` (`make pc-test`) checks it: on each disc as the save's, the disc check passes and reports that disc; all
  10,052 files read back byte for byte through `cd_read`/`cd_step`; and the 8,143 files that start with their packed
  length unpack the same streamed (`cd_read_lzs`), streamed from memory, and in memory (`lzs_decode`).
- Two header fixes it needed don't change the PS1 code: `snd_submit_request` has a prototype in `sound.h`, and
  `CdSearchFile` takes a `const char *`. `cd_read_disc_number`'s switch casts the found file to `long`, not `int`.

### PC build: the exe runs (milestone 1)

`make pc` builds `build/pc/ff7.exe`, a 32-bit Windows program (MinGW i686, called from WSL) that runs the whole boot
exe: the C from `src/main`, and every function still in assembly recompiled to C from the user's own `SCUS_941.63` by
`tools/recomp.py`. It boots through `ENDING.X`'s opening credits and the logo's movie (`OPENMV.BIN`, around frame
9000) to the title logo, on Start to the title menu (NEW GAME / Continue?, the menu modules at `0x801D0000`), and on
NEW GAME into the field module, which loads `MD1STIN` and plays the opening movie (`OPENINGE.MOV`, streamed).
`pc/sdl.c` shows it in an SDL2 window (640×480 at 4:3; the keyboard is pad 1; F12 saves `screenshot.bmp`), or
`ff7 [ASSETS] --frames N [--shot FILE.bmp]` runs without one and saves frame N.
- **Memory:** the PS1's RAM sits at its own address, `0x80000000` (with the 64 KB after it, for the stack words a
  call from the top of the stack reads), and the scratchpad at `0x1F800000`; the I/O window `0x1F801000`–`0x1F802FFF`
  goes to `pc/hw.c`. Of RAM's mirrors, `0xA0000000` folds back, and so does the first 64 KB from 0 (the field reads
  through a null pointer at the start of `MD1STIN`); the rest of KUSEG can't, as the host's own stack and heap may
  sit there. Game pointers need no translation, and C and recompiled code share the game's data. The link is
  `--large-address-aware --disable-dynamicbase`, and `tools/pc_syms.py` places each game symbol at its address.
- **Recompiler rules:** one C function per game function over a register context (`pc/recomp.h`); delay slots
  folded; a backward branch polls the hardware. Each function has a C-callable thunk taking eight ints (the last four
  are the PS1's stack words). Code pointers are native: a `lui`/`addiu` pair that builds a function's address gives
  the native function only when the address escapes as a code pointer (stored, passed, returned or called; libgte's
  `_patch_gte` uses one as a loop's end), and the code addresses in the game's data are patched at start: every
  aligned word in a data file's range that holds a function's address, whatever type splat gave it. (A `.word func`
  parse missed most of `snd_command_handlers`: `seq_cmd_scan_sizes` and `seq_cmd_sizes` are named at their C base,
  0xA0 entries before their tables, which puts a label inside the handler table, and splat emits the rest of it as
  bytes. Sound command 0xF0 then called a game address natively.) A game
  address that still turns up is looked up in the code tables. The point after a `jal setjmp` is an entry of its own:
  libetc's interrupt entry resumes there, through the BIOS's custom exit.
- **Modules:** the Makefile's `PC_MODULES` lists each with its file and load address (`ENDING.X` and the field's
  `FIELD.BIN` at `0x800A0000`, the 13 `MENU/*.MNU` at `0x801D0000`, all the same on every disc; `FIELD.BIN` is
  gzipped behind an 8-byte header, so the list names `FIELD.BIN.dec`, which `tools/unpack.py` makes). `make split`
  clears each `asm/<name>` (a stale file from an earlier split gets read as more functions) and splits it with a
  splat config from `tools/module_yaml.py`: a module is read-only data, then code from its first function to the last
  `jr $ra`, then data. The first function starts at its stack-frame setup or at the instructions GCC scheduled before
  it (two loads, in the field), which gives `ENDING.X`'s hand-found bounds exactly. `recomp.py` takes a module's functions
  from splat's `glabel`s, and its calls into the exe by the exe's symbols. Modules share load addresses, so a
  module's functions stay private to its file. `module_function` (`pc/dispatch.c`) picks the module whose first
  function's opening 64 bytes are in RAM and whose code has the address (a module at another address can still be
  in RAM), and patches its data's code pointers that still hold game addresses.
- **Code behind a BIOS stub:** splat runs a three-instruction stub (`jr $t2`) on into whatever follows when that has
  no symbol, so the function after it is never recompiled ("reached … which wasn't recompiled"). `format`,
  `_new_card` and `StopCARD` hid one each; they now have symbols.
- **What's emulated:** the BIOS calls the exe makes (56, checked by `tools/test_bios_coverage.py`) and the interrupt
  path (events, root counters, the custom exit; an event opened `EvMdINTR`, 0x1000, calls its function, and one
  opened `EvMdNOINTR`, 0x2000, is marked ready for `TestEvent`: the title menu waits that way on the memory cards,
  which answer "no card"); DMA, timers and vblank; a software GPU (VRAM, every primitive,
  textures, semi-transparency, masks; in 480i the field on screen changes each vblank, and `ENDING.X` uploads the other
  one line by line); the GTE (a command reads all its inputs before it writes any: OP and MVMVA on IR overwrite the
  IR they read, and a zero walkmesh normal from OP hit GCC's divide-by-zero `break` when the opening movie ends); the
  MDEC (`pc/mdec.c`, dequantizing as `tools/movie.py` does, so the in-memory movies
  run through libpress and `movie.c` as they are). Time is counted in polls of 32 R3000 cycles, about a short loop's pass, because the
  libraries' spin timeouts count passes (libetc's `VSync` gives up after 32768, ~1.7 frames) and recompiled code runs
  them far faster; after each vblank the host clock paces it to 59.94 Hz.
- **Waits in C:** the hardware moves only when something polls it. Recompiled loops do at every backward branch;
  decompiled C doesn't, so a C loop that waits on a flag only an interrupt changes has `HW_WAIT()` (`common.h`) as
  its body, which is `hw_poll()` under `NATIVE` and nothing on the PS1 (`snd_transfer_wait` hung the boot at frame
  ~680 without it; `movie_wait_frame` waits on the MDEC's last column the same way). `make pc-test` boots 900 frames
  to catch the next one.
- **Ports:** a register that's a port (MDEC0/1, GP0/1) takes a write as it is: merging a partial write with a read
  of it would take its data.
- **Streamed movies:** libcd's streaming is C in `pc/cd_native.c`. `CdRead2` starts reading the STR sectors from the
  drive's position at 150 a second of PS1 time; `StGetNext` waits for a frame's last sector and hands out its video
  sectors' data joined after the first one's header, in the ring `movie.c` gave `StSetRing`. libpress, the MDEC and
  `movie.c` run as they are, camera data included.
- **Not yet:** movie sound (the XA sectors are skipped) and SPU output, game controllers, L1/R1/L2/R2 keys. Waiting
  for the host's clock spins a core.
- `make pc-test` runs the recompiler on hand-assembled fixtures and C tests of the memory map, the C/recompiled
  bridge, the BIOS, modules, the hardware registers, MDEC, GPU, GTE, display and the CD layer.

### Matching rules for GCC 2.6.3/2.7.2 + maspsx

**Loop invariants (GCC 2.6.3's loop optimizer).** It moves a loop-invariant register out of a loop when
`threshold × savings × lifetime ≥ insn_count`:
- `threshold` is 26 here (measured) and drops by 3 for every register already moved; `insn_count` doubles for each
  register already moved out of an inner loop.
- `savings` counts the temporaries holding the same value; `lifetime` is in luids, and **every note that isn't a line
  number takes a luid** (deleted insns, loop and block notes).
- `-dL` prints each decision: compile the C and read the dump to see why an address was or wasn't hoisted.
- So a match can hinge on shape, not statements (see `main` below): a user variable deriving from an address
  lengthens its life and gets it hoisted; **a call without a prototype** makes GCC 2.6.3 put an implicit declaration
  in that block, which keeps the block's notes and lengthens lifetimes.

**Loop rotation.** GCC rotates a loop that starts with its exit test. To keep the game's unrotated form:
- make the test the `for` condition (`F8`), put it in its own block with a declaration (`F4`), use `goto`
  (`highest_bit`), or put the test inside a `for (;;)` with the `return` there (`seq_stream_end`);
- `seq_step_channel`'s command loop is a plain `do`/`while`.

**Register-held addresses.** GCC 2.6.3 reaches a plain scalar global with `lui`/`%lo`, but loads the address of a
struct field or a pointer expression (`*(T *)&x`) into a register, and folds later accesses to the same object back
to `lui`/`%lo`: within a function, the first one touched is the one in a register. In a loop those registers can be
hoisted. To steer it:
- Want the address in a register? Reach it through a pointer (`F8`/`F9`'s `effect_voices`) or a struct, ordered so
  the right field comes first (the save-data reset writes the disc number first).
- Don't want it? Use a plain global alias: `seq_song_flags` (`0x8009A13C`, `seq_song`'s `+0x38`) and the voice masks
  (`+0x04`, `+0x24`, `+0x28`) are reached as plain variables, since as struct members GCC computes the address once
  and the game doesn't. `VoiceSettings`' fields likewise each have an alias (a register holds the struct's address
  while the stores use their own).
- One symbol or two: GCC reaches one object's fields as offsets from another field's address only within one symbol.
  `snd_slow_slides` does that across the two song blocks, so `seq_songs[2]` (the same storage as `seq_song` and
  `seq_song2`) models them; the rest of `sound.c` uses the two names.
- An address the game computes once before a branch goes in a local there (`&seq_song` in sound command `14`).
- A plain global whose address the game holds for a load and a later store is reached through a pointer local: the
  CD layer's frame and retry counters (`int *last = &cd_last_vsync`), `&cd_request` in `cd_step`.
- Many of `main.c`'s older `*(volatile T *)&x` casts are probably this rule in disguise.

**Address forms of a big constant offset** (`transition.c`'s `ShardState` at `0x8019C000`):
- as a struct field (`shard_state->screen[i][j].vx`) GCC folds the whole address into one constant
  (`lhu v0, -2145789780(v1)`), which maspsx expands register first (`lui at; addu at, v1, at`), as in the game;
- through an extern symbol it expands as `addu at, at, v1` (8 words off);
- through a bare constant pointer (`((DVECTOR (*)[8])0x8019D8AC)[i][j]`) GCC keeps the base in a register for the
  offset-0 field and strength-reduces the loop. Where the game does show that register (`a3` for `.vx` in the setup's
  vertex grid), decay the field to a pointer first: `((SVECTOR (*)[8])shard_state->verts)[i][j]`.

**Aliasing.** GCC 2.x assumes a struct field (reached through an index or a pointer) can't alias a fixed scalar
global, so it moves accesses past each other:
- Where the game keeps the order, go through a plain pointer: `A0` reads the overlap voice and clears the channel flags
  through the same `unsigned int *` it used for `effect_voices` (through `ch->` the update mask loads early and the
  voice bit leaves `a2`); `seq_channel_slides`' noise flip (else the flags load rises above the store); `seq_tick`'s
  gate through an `unsigned short *` (else the mask loads rise above the gate store).
- Where the game reorders, use the field: `snd_resume_song`'s copies target `seq_song`'s fields, not the aliases.
- The opposite case: as separate globals, the save function lets GCC move a table load above the stores, which it
  doesn't do past a struct field, so the save-data block is one struct.
- `snd_submit_request` reads `snd_request.arg[n]`: the game reloads the fields after each queue store.

**`volatile`.** A `short` that the game loads with `lhu` and sign-extends with shifts, reloads after storing, and
reads in order (even twice) is `volatile` (probably shared with the VSync callback). `x++` on a volatile reloads `x`
after the store and discards it, as the game does. Applies to `game_mode`, `frame`, the fade level and step
(`0x8009AC42`/`0x8009AC44`), the save-data block, and `snd_transfer_busy` (the wait loop reloads it).

**Types and variable sharing** decide registers and shifts:
- One variable for two values: `snd_effect_pitch_shift_slide` (the length argument, then each channel's distance);
  `seq_tick` (the tempo scale and the phase); the bank loaders (the copy counter is the variable that held the transfer
  size, and the header pointer is stepped to the sample data before the call; a fresh counter or `header + 3` in the
  call gives `a0`/`v1` instead of `v1`/`a1`); `snd_start_effect` (the shifted pan goes back into the parameter; a
  separate local swaps the saved registers of the pan and the second stream).
- Exact types: `snd_start_effect`'s pan parameter is `unsigned short` (every caller casts); `vibrato_amount` (`+0x7C`)
  is a signed `short`, for the arithmetic shift; `B4`'s `amount` is `unsigned short`; `seq_cmd_octave_down` adds an
  `unsigned short` −1 (`0xFFFF`); `seq_step_channel`'s octave is an `unsigned short` filled from a byte, and it reads
  the look-ahead's result into the pitch variable; in `func_80014750` the entry size is an `int` taking an
  `unsigned short` result; `seq_channel_lfo` reads the master volume's top half through its own `unsigned short`
  symbol.

**Statement order** at function ends and in expressions:
- In `A1` and `F2` the voice-update `|=` comes after the last field copy.
- The SPU voice masks are one expression with the "free voices" mask repeated as the last argument; a local computed
  first schedules differently.
- `seq_tick`'s song 2 loop setup follows the game's order, or a register or the bit's constant moves.
- `snd_slow_slides`: slides decrement with `--`, the fade's step divides by `snd_fade_ticks` itself, and the pointer
  to song 2's masks is set at the top of each loop pass.

**`-G8` files** (`sound_init.c`, `loader.c`):
- maspsx emits `$gp` accesses only for small variables the same file defines, so the file gives them tentative
  definitions and `--use-comm-section` keeps them common: the linker resolves them to the data section's labels and
  adds no storage.
- GCC writes every function after the file's top-level `asm`, so a plain `INCLUDE_ASM` lands first. maspsx's
  workaround is each include inside a `__maspsx_include_asm_hack_` function (neither file needs it now).
  `-mno-gpopt` doesn't help: GCC forces it on when optimizing with `-G`.

**Switches.**
- A jump table runs to the highest listed case: `cd_cancel` lists states that do what `default` does, because the
  game's table runs to 18.
- `case -1: case 0:` compiles to a range check (`bgtz`, then `slti -1`): `cd_read_disc_number`'s `CdSearchFile` test.
- Code that falls into a function's final `return 0` must not end in its own return: `cd_check_disc` uses
  `break`-style switches and lets the success path fall out of the `if`. Likewise `movie_open`'s in-memory case ends
  in `break` and the function's `return buf`.
- Three cases in a row with no jump table compile to two `sltiu`s, upper bound first: `movie_stop`'s states 8–10.
  `x >= 8 && x < 11` instead folds to one range check.
- Identical stores ending both arms of an `if` are merged after them: `movie_next_frame` sets the frame's bottom edge
  in each arm, so the height loads twice.

**Inlining.** A non-static `inline` function is emitted and also expanded into later callers in the file, each copy
with its own locals: `movie_stop`, twice in `movie_play_frame` (its `RECT` sits above the caller's `CdlLOC`). To test
a caller alone, give the callee as `extern inline` in the prelude.

**Rounding up.** `(n + 3) / 4 * 4` on an unsigned gives `srl`/`sll`; `(n + 3) & ~3` gives a `li -4` and `and`.

**`.rodata` packing.** The link packs each file's `.rodata` back to back (`SUBALIGN(2)`), so a file whose last string
ends off a word boundary shifts everything after it. Size the string to the game's padded length:
`diskinfo_path[24]` in `cd.c`.

**Counting:** count recovered functions with a parser, not by eye; names with upper-case hex are easy to miss.

### Boot code (`0x8001117C`–`0x80014A00`, 47 functions, all C)

Before it sit the hand-written entry at `0x800110C0` (clears BSS, sets `$sp`/`$gp`, calls the main loop; asm segment
`18B8`) and `__main` (empty: GCC calls it first thing in `main`). `include/main.h` holds what the files share.

- **`main`** (`0x80011C1C`, 3 KB): sets up memory, the display and the boot files, then loops forever: load the sound
  files and the first module, then run modules by `game_mode` (a 16-case switch; most cases read a file to
  `0x80180000`, unpack it to `0x800A0000` and call it) until `D_8009ABF5` asks for 5, `0x0A` or `0x1A`. It has an
  unused local `"battle.x"`. The match hinges on the loop optimizer: the game rebuilds `&D_8009AC32` at the top of
  each pass and keeps `&D_8009ABF5` and `&D_8009ABF6` in registers set before the middle loop. That needs:
  - no `mode` pointer (a user variable deriving from `&D_8009AC32` gets it hoisted);
  - the middle loop as a `while (1)` whose four exits (disc refused, ending, game over, title) each set the state,
    shut the drivers down and `break` (GCC cross-jumps the copies and parks the blocks after the loop);
  - the sound requests through a block-scoped `SoundCommand *`;
  - most callees without a prototype. With prototypes the code is 5 luids short and the `&D_8009ABF5` temporary
    isn't moved.
- **Disc files:** `{lba, size}` pairs. The data section opens with the sound files (`instr_all_file`,
  `effect_all_file`, `instr_dat_file`, `instr2_all_file`, `instr2_dat_file`) and more; `disc_files` (`0x80048D84`) is
  a table by number, its first 10 entries reloaded from sector `0x266` by `load_disc_table`.
  - `read_disc_file(i, dest, callback)` starts a read through `cd_read(lba, size, dest, callback)`;
    `wait_disc_read(idle)` polls `cd_step`, calling `idle` while it waits.
  - `load_sound_files` puts INSTR.ALL at `0x800F0000`, EFFECT.ALL at `0x801B0000` and INSTR.DAT at `0x801BC800`, then
    calls `snd_init` and `snd_load_effects`; `snd_load_bank2` loads the second bank into `0x800A0000`/`0x800E0000`.
  - `load_boot_files` reads files 1 and 2 with read callbacks that walk their numbered entries
    (`func_80015B44`/`B50`/`B88`/`BC0`): file 1's entry 0 is an image for VRAM, file 2's entries go to the addresses
    in `D_80048DD4`. It and `func_800148A0` reach `D_80062F88` and `D_80063014` (the scratchpad pointer) through
    `$gp`.
- **Modules:** `run_overlay(file, entry)` reads the file to `0x801B0000`, unpacks it to `0x800A0000`
  (`func_80015CA0`) and calls `entry`. `func_800146A4` loops on `game_mode`: 4 starts file 3, 2 file 5.
- **Display:** two `DISPENV`s (`disp_env`, y 232 and 0) and two `DRAWENV`s (`draw_env`, y 8 and 240, `dtd` on).
  `init_display` sets them up, `flip_frame` toggles `frame` and puts the next pair and its ordering table,
  `copy_frame` copies the shown buffer to the other. `vsync_callback` runs what `D_80095DD4` picks (1 `flip_frame`),
  then advances the play-time clock: `D_8009D26C` gains `0x444` a frame, so its top half is one second at 60 Hz,
  counted in `D_8009D264`; `D_8009D270` drives the countdown `D_8009D268` the same way.
- **Screen fade:** `func_800138EC` runs it each frame, a switch on mode `D_8009AC40` (14 cases, written in the order
  0, 1, 2, 7, 8, 3, 13, 4, 5, 6, 9, 10, 11, 12, as the game lays them out). The modes pick a blend (`func_80012840`
  sets both draw modes' tpage), step the level `D_8009AC42` down or up (`func_800134F4`, `func_80013564`) or count it
  to an end, and draw.
  - `func_80013624` moves a colour from `D_8009AC4C`–`50` to `D_8009AC52`–`56` over `D_8009AC44` steps through
    `func_800135C0` (`from + (to - from) * t / n`).
  - The draw routines clear the frame's ordering table, colour both tiles of `D_8009A068[frame]` and link the tiles
    and the draw mode: `func_800131B8` in a colour, `func_80012DB0` in a colour scaled by the level
    (`level * c / 256`), `func_80012A8C` in grey. `func_800128B8` sets the tiles up (160x224 each, side by side).
- **libgpu macros:** colour, size and position stores only match written as libgpu's `setRGB0`/`setWH`/`setXY0`
  expand, through `&array[frame].tile[j]`: GCC then holds the tile's address (symbol plus offset) in a register and
  adds the field offset, where `array[frame].tile[j].r0` folds it into the `$at` form. `main.c` has them as
  `set_rgb`, `set_wh`, `set_xy`; linking is libgpu's 24-bit `next` field (the `0xFF000000`/`0xFFFFFF` masks), a
  bitfield in `add_prim`. `transition.c` uses the same macros for its quads' colour, texture coordinates and corners.
- **Save data:** one struct from `0x8009D274` to at least `0x8009D588` (`D_8009D274.disc` the disc number; two
  byte-pair counters at `0x8009D2A0`). `func_800119E4` saves its state, `func_80011AEC` restores it, `func_80011BB4`
  resets it. As a struct starting at the state, the first field (in a register) would be the wrong one.
- **Screen transition** (`transition.c`): `func_800140F4` gives it its own display, copies the screen in four strips
  with every pixel's semi-transparency bit set, and sets up a 10x7 grid of textured quads over an 11x8 vertex grid.
  `func_80013C9C` draws a frame: it alternates two quad grids, fades or re-pages them, rotates and scales the vertex
  grid with the GTE and links the projected quads. Its buffers sit back to back from `0x8019C000` (two quad grids, the
  current one, the vertex grid, the projected grid) as the struct `ShardState`; scalars past the grids are reached by
  symbol.

### CD layer (`0x80033A90`–`0x80034CF0`, 38 functions, all C)

`src/main/cd.c`. One request at a time, by sector: `cd_start_request(state, lba, size, dest, callback)` waits for
the current one, then queues the new one in `cd_request` (`0x80071A60`); `cd_step` runs that state's handler from
`cd_state_handlers` (`0x8004A634`, 21 entries) and returns the state, `CD_IDLE` (0) when done. Callers poll it.
- **Entry points:** `cd_read` (state 3), `cd_seek` (1), `cd_read_lzs` (11: reads 9 sectors at a time into
  `cd_sector_buf` and unpacks them as they arrive), and `_wait` versions that poll with `VSync(0)`. `cd_init` runs at
  boot, `cd_shutdown` on every way out of `main`'s loop.
- **States:** each request goes seek (`CdControlF(CdlSetloc)`), wait for `CdSync`, read, wait for `CdReadSync`, then
  17 (done) and 16 (go idle and call the callback). 19–20 pause the drive. 8–10 belong to the movie player (below);
  18 idles, but the next request pauses the drive first.
- **Errors:** a `CdlDiskError` retries the step; 16 in a row stop the drive (`cd_stop`, state 7) and loop
  `cd_error_sound(3)` (sound command 30, then a second's wait) until `cd_check_disc` returns 0. A step that waits
  3600 frames retries with the same sound.
- **Disc check:** `cd_check_disc` spins the disc up, checks `CdDiskReady` and the disc type, seeks sector 23
  (`SYSTEM.CNF`), then reads the disc number; `cd_read_disc_number` finds `\MINT\DISKINFO.CNF` with `CdSearchFile`
  and returns its eighth character minus `'0'`. `get_disc_number` wraps it; `main` compares it with the save's disc
  (`0x8009D588`) and runs `FIELD/DSCHANGE.X` when they differ.
- **`lzs_decode(src, dst)`:** the in-memory LZS decoder (`u32` packed size, flag bytes, 12-bit offsets into a 4 KiB
  window, lengths 3–18; window positions before the output read as zeros).
- **After it:** `0x80034CF0`–`0x80034F3C` is hand-written assembly (data words in `.text`, registers kept across
  calls): `lzs_stream_start`/`lzs_stream_resume`, the decoder `cd_read_lzs` feeds, and `offset_table_entry`
  (`table + table[i]`), which the movie player uses.
- `func_80033A90` sits here but isn't CD code: it clears the 0x500 bytes up to `0x8009D787` and resets play time.

### Movie player (`0x80034F3C`–`0x8003623C`, 14 functions, all C)

`src/main/movie.c`, built on libpress and libcd's streaming (`St*`). Its state is `movie` (`0x80095D88`).
- **Opening:** `movie_open(buf, id)` (called from outside the exe) refuses while the CD layer is busy, fades the XA
  audio up, and looks `id` up in `movie_table` (`MOVIE_ID.BIN`, loaded at boot by `movie_load_table`). It carves two
  run-level buffers, a column buffer and (type 0) a 36-sector ring out of `buf` and returns the end of them. Type 0
  seeks to the movie, types 2 and 3 read it whole; `movie_on_ready` then sets CD state 9.
- **Starting:** in state 9 `movie_poll_start` waits for 8 buffered sectors, then sets 10. The caller then runs
  `movie_start`, which decodes the first frame and sets 8.
- **Playing:** in state 8 every `cd_step` runs `movie_play_frame`. It sends the decoded frame to the MDEC into the
  buffer being drawn (`movie_target`), one 16-pixel column at a time (`movie_slice_done` copies each to VRAM and asks
  for the next). Meanwhile `movie_decode_next` takes the next frame, copies its camera block and colour flag, and
  VLC-decodes it into the other buffer. Then it waits for the last column. A stream that stalls is sought back and
  restarted (`movie_reread`).
- **Ending:** a frame number at or past the table's last frame, or one that doesn't advance, ends a streamed movie;
  type 3 ends after its last frame and type 2 loops. `movie_stop` pauses the drive, drops the callbacks and returns the
  CD layer to idle. If the screen was in 24-bit colour, it clears the 480 lines it used.
- **Colour depth:** a change in the frame's flag is held for one frame (`movie_depth_pending`), then applies to both
  the MDEC and the display (`isrgb24`); a 24-bit frame's columns are 24 halfwords wide.
- **Timeouts:** the waits count down from `0x800000` (splat shows it as `D_7FFFFF`, from `lui 0x80`; `addiu -1`).
- From `0x8003623C`: two data words and an unreferenced hand-written helper, then libspu (its `SPU:T/O` strings).

### Sound driver (`sound_init.c` `0x800293D0`–`0x80029A4C`, `sound.c` `0x80029A50`–`0x80033A90`)

Each file is its own splat segment. 231 of its 232 functions are C (`seq_cmd_slur_off`, `seq_cmd_full_length_off` and
the sound-command no-op `func_8002CF98` are empty in the game). Unrecovered ones are `INCLUDE_ASM`. **Parked:**
`snd_start_lasting_effect` (the kind 2 effect's start): the game keeps `&seq_channels[54]` in `s1` across the first
`channel_reset`; GCC treats it as a constant, rebuilds it for each use and addresses channel 55's fields from it. About
190,000 permuter tries didn't find the form.

**Structs** (`include/sound.h`, named from what the code does with them):
- `SeqChannel`, `0x108` bytes: read pointer, loop stack (`+0x04` starts, `+0xB8` level, `+0xBA` passes), drum map
  (`+0x14`), layer voice (`+0x24`), volume, pan, octave, length fields, glide, bend, transpose and detune, LFO depths
  and offsets, the noise and FM countdowns, the envelope (`+0xEC`–`+0x102`), flags (`+0x38`), `+0x54` set on effect
  channels (2 = kind 2), instrument number (`+0x58`), `vibrato_amount` (`+0x7C`), `key` (`+0xD0`). Pitch offset
  `bend_offset` (`+0x34`, its top half adds to the pitch register; step `bend_step` `+0x4C`, ticks `bend_count`
  `+0x64`). Effect `pitch_shift` `+0x3C` (a signed byte << 8, read as byte `+0x3D`; step `+0x40`, ticks `+0x5A`);
  `effect_ticks` (`+0x50`).
- `VoiceSettings` (`+0xDC`, `0x2C` bytes): what `snd_update_voice(voice, settings)` hands to libspu; first word the
  voice number. Its `update` (`+0xE0` in the channel) is libspu's voice attribute mask: `0x3` the two volumes, `0x10`
  pitch, `0x80` sample, `0x100`–`0x400` the envelope modes, `0x800`–`0x8000` the rates and sustain level, `0x10000`
  loop. `snd_direct_voice` (`0x8007EBE4`) is a standalone one for voices written without a channel.
- `SeqSong`, `0x60` bytes (the save slots' stride confirms it): voice masks `active_voices` (`+0x04`), `key_on_voices`
  (`+0x08`; alias `seq_song_key_on_voices`, effects' copy `effect_key_on_voices`), `key_off_voices` (`+0x10`),
  `paused_voices` (`+0x14`), tempo (`+0x18`), masks for layers, overlaps, noise, reverb and FM (`+0x24`–`+0x34`), flags
  (`+0x38`: `0x10` noise changed, `0x80` reverb depth changed), reverb mode (`+0x3C`), reverb depth (`+0x40`), their
  slide counters, `EF`'s recorded n (`+0x4C`), branch level (`+0x4E`, set by `92`), song id (`+0x4A`, 0 = none), the
  noise clock (`+0x52`), `pause_pending` (`+0x54`, set by `F3`), and the meter (`+0x56`–`+0x5E`: `bar_beat` `+0x58`,
  `beat_tick` `+0x5C`).
- `Instrument`: the 64-byte `INSTR.DAT` entry in `instrument_table`.
- `SoundCommand`: a 16-bit id, then 32-bit arguments from `+0x04` (`0x24` bytes, as queued).

**Globals:**
- `seq_channels` (`0x80096608`): every channel. 0–23 the song, 24–47 the second song block, slots 48–55 effects
  (four pairs at `0x30`, `0x32`, `0x34`, `0x36`), and **effect slots 48–55 are SPU voices 16–23**. A layer's spare voice
  keeps its envelope in its voice's entry, so the envelope commands write both.
- `seq_song`, and `seq_song2` (`0x8009A164`, right after it) owning channels 24–47 with its voices limited by
  `seq_song2_voices` (`0x80062F68`): the song fading out under `18`/`19`, and a waystation for `15` and `F4`.
- The effects' block: `effect_voices` (`0x80099FCC`) and the words after it follow `SeqSong`'s first fields
  (`effect_key_on_voices`, `effect_update_voices`, `effect_key_off_voices`, `effect_paused_voices`). Effect channels
  use their own masks and noise clock: `effect_noise_voices`, `effect_reverb_voices`, `effect_fm_voices`
  (`0x80099FEC`–`F4`), `effect_noise_clock` (`0x80099FFA`), and their own `effect_tempo`, `effect_tempo_phase`.
- `snd_flags` bits: 0 `SND_MUSIC_PAUSED`, 1 effects paused, 2 `SND_DOUBLE_SPEED`, and `SND_REVERB_SCALED`.
- Save slots `saved_song_channels[2][24]` (`0x8007EC10`), `saved_songs[2]` (`0x80083334`): slot 1 holds song 14,
  slot 0 any other. `song_data` (`0x80083580`), `snd_channel_fades[24]` (`0x8009C5A0`, 12 bytes each),
  `snd_fade_ticks` (`0x80062FC8`), `snd_request` (`0x8009A000`), `snd_queue_locked` (`0x80062F8C`),
  `seq_cmd_sizes` (`0x800499A8`), `seq_cmd_scan_sizes` (`0x800498A8`), the end stub `seq_end_stub` (`0x80049C40`).

**Sequence command handlers** (`seq_cmd_handlers[op - 0xA0]`) all take (channel, song, the channel's SPU voice bit).
`B0` and `CB` pass all three on; the reverb, noise and FM commands set or clear that bit in a voice mask. Every unused
command byte calls `seq_cmd_end`. Covered: octave, slur and full length, volume, pan, transpose, detune, lengths (`A2`,
`DC`), glide and bend, loops, repeats and jumps (`C8`, `C9`, `CA`, `EE`, `F0`, `F1`, `EF`), drum kit, tempo and its
slide, reverb depth and its slide, time signature and bar number, the tremolo and pan-LFO starts, the LFO depths, depth
slides and off commands, volume, pan and balance slides, noise (on, off, clock, timed), FM and reverb on/off, pitch and
volume follow, layer balance and layer off, overlap off, the envelope setters, `B0`, `B3`, `B4` (vibrato start), `CB`
(all off), `F3` (`func_800335CC`, sets the song's `+0x54`), `F4`/`F8` (the layer and overlap voice searches).
- `A0`: an ending effect sets the full update mask on the `seq_channels` entry its `+0x28` names, handing the voice
  back to that voice's music channel.
- `A1`, `F2` (instrument changes): rescale a sounding note's pitch by the new instrument's C pitch over the old one's,
  unless an effect holds the voice (`effect_voices`); with `F8` on they copy everything but the release (update
  `0x1BB80`). `F2` sets the sample to `0x76FE0`, the silent blocks. `load_channel_instrument` copies the sample and
  loop addresses and all eight envelope bytes and sets `0x1FF80`.
- `F9` restores the release rate from the instrument's `adsr[4]`.
- `F1` is `F0` that also pops the loop level; `EF` jumps when the song's `+0x4E` is non-zero and at least n, and
  records n at `+0x4C`.
- `DA` (glide) also turns slur mode on; `DB` only clears the glide ticks.
- `CE`/`CF` are the noise twins of `D2`/`D3`: noise on now (`CE` only) and flipped after n + 1 ticks. `DF` is the
  pan-LFO depth slide, twin of `DD` and `DE`.

**`seq_step_channel`** (`0x80030E7C`) runs a channel's next event:
- Calls handlers until a note, tie or rest (or `A0`, which stops it).
- A note byte is note × 11 + length (`seq_note_lengths`, a gate of key-off << 8 | ticks), then ties `0x84`–`0x8E` and
  rests `0x8F`–`0x99`. With `DC` the gate is length × `0x101`. The key-off comes 2 ticks early before a note or rest,
  unless the next note is a tie or the channel slurs (`CC`) or plays full length (`D0`); before a rest it always does
  when `DC` is on.
- Drums (`EC`): the note picks 5 bytes from the drum map: instrument (loaded only when it changes, release kept under
  `F8`), key, volume (2 bytes, << 16) and pan.
- Otherwise key = octave × 12 + note (kept in `key`) plus transpose; under `DA` the note starts from the last key and
  bends to its own over the glide's ticks.
- Key-on unless the note slurs; a layered channel (`F4`) keys its layer voice too.
- Pitch: the instrument's octave-6 pitch for the semitone, shifted by octave − 6, then detune (n/128 up, n/256 down).
  Vibrato's amount comes from the depth and this pitch; the three LFOs restart. `A4` (or a glide) sets `bend_step` so
  `bend_offset` reaches the new key's pitch in `bend_count` ticks.
- `seq_look_ahead` follows `C9`/`CA`/`EE`/`EF`/`F0`/`F1` on a copy of the loop level, by `seq_cmd_scan_sizes`.
- `seq_stream_end` (was `func_80031A70`) walks a stream by `seq_cmd_sizes` (0 for the commands that end one) and
  returns `CA` or `A0`.

**Sound commands** (`snd_command_handlers`; names follow the byte, `snd_cmd_D0`, until the meaning is known):
- **`snd_submit_request`** (was `func_8002DA7C`) is the game's way in: it reads `snd_request` and queues commands with
  the queue locked. Song starts (`10`, `14`, `15`, `18`, `19`) need data starting `AKAO`, then id, size and reverb
  mode; it returns 1 when that song already plays and -1 without the header. `24`–`27` queue `20`–`23` (effects with
  consecutive ids), `D8`–`DA` queue the tempo and pitch scale commands together, `98`/`99` resume or pause music and
  effects; anything else passes through. `snd_flush_queue` runs each queued command unless `snd_queue_locked` holds
  it; `snd_queue_next` gives the next free `SoundCommand` in `snd_queue`.
- **Songs** (`10`, `14`, `15`, `18`, `19`):
  - `10` a b id loads with `func_80029B78`, then resumes a saved song with that id (either slot, `snd_resume_song`)
    or starts it (`snd_start_song`); song 14 is saved first when it's playing.
  - `14` saves whatever plays (`snd_save_song`) before starting the new one.
  - `15` is `10` that swaps: the playing song goes through the second block into a save slot while the saved one
    resumes (or, with no saved match, it's saved and the new one starts).
  - `18`/`19` are `10`/`14` that cross-fade: `snd_fade_out_song` moves a playing song to the second block (channels
    0–23 to 24–47, `seq_song` to `seq_song2`, song data 12 KB up with every read, loop and drum-map pointer moved by
    `0x3000` and layer voices renumbered from 24) and fades it: `snd_channel_fades` takes channel n from `0x7F8000`
    to 0 over (24 − n) × `snd_fade_ticks` ticks (arg 3, 0 = 16).
  - `snd_start_song` reads `song_data`: a channel-mask word, then a 16-bit offset per channel (from after the
    offset). Every channel starts on instrument 20 at volume `0x3FFF0000`, centred; the song state starts over
    (tempo `0xFFFF`, `tempo_phase` 1).
  - `snd_resume_song` copies a save slot back field by field (the beat's tick count 2 back), then the channels.
    Channels the song doesn't hold step into the end stub; its voices rewrite everything. Unless a slide or the
    cross-fade runs, `snd_master_volume` restarts at 10 and slides back over 60 ticks.
  - `snd_load_song` copies the song into `song_data`; `snd_save_song` copies 24 channels and the `SeqSong` into a
    slot; `snd_keep_sounding_voices` runs first and keeps the voices still keyed on, or whose `F8` overlap voice is.
- **Effects** (`20`–`34`): look the id up with `func_8002A6C4` (its two streams), then `snd_stop_effect_pairs` (one to
  four pairs) and `snd_start_effect(pan, pair, first, second)`. `20`/`28` play on pair `0x34`, `29` on `0x32`, `2A` on
  `0x30`, `2B` on `0x36`; `21`–`23` play two, three or four effects together from `0x30` (or `0x32`) up; `30` hands
  its effect to `snd_start_lasting_effect`; `34` plays two caller-given streams, centred.
- **Effect controls `A0`–`B7`:** balance, balance slide, pan, pan slide, pitch shift, pitch-shift slide, four commands
  each, one per pair (slots 52, 50, 48, 54), via a shared worker (`snd_effect_balance` …) that sets both channels. A
  slide's length 0 means 1 tick. `B8`–`BD` apply the six to all four pairs (from slot 54 back, two channels a step).
- **Scales and volumes:** `D0`/`D1` and `D4`/`D5` set or slide `snd_tempo_scale` / `snd_pitch_scale` (a signed byte
  << 16), `D2`/`D6` are their from–to slides. `C0`/`C1`/`C2` set or slide `snd_master_volume`. `C8`/`C9`/`CA` set,
  slide and slide from–to `snd_cd_volume` (`0x80062FD4`, << 16); `C8` also calls `func_8002E428`.
  `snd_apply_cd_volume` writes its top half to both sides of a libspu `SpuCommonAttr` (mask `0x1C0`: CD volume
  left/right and CD reverb, here 0) and hands it to `0x80039034` *(probably `SpuSetCommonAttr`)*.
- **Others:** `80`/`81`/`82` store 1/2/4 in `seq_song`'s first word and call `func_8002A748` and `func_8002A798`.
  `90` stores a mute mask (`snd_muted_voices`: volume 0) and sets every music channel's volume-update bits. `92` sets
  the branch level. `E0` sets `snd_reverb_balance` (`0x80062F70`) and the reverb-changed flag; `E4` sets
  `snd_reverb_scale` and `SND_REVERB_SCALED` (every voice in reverb). `F0` (`snd_stop_songs`) runs `snd_stop_song` on
  both blocks; `F1` (`snd_stop_effects`) ends every effect but kind 2; `F2`/`F3` clear one word each (emptying the
  save slots); `FA` (`snd_stream_stop`) and `F8`/`F9` (`snd_stream_start`, taking the stream's voices from the
  effects with reverb off or on).
- **Pause:** `9B` gives the music's voices (not the effects' or the stream's) volume 0 and a held sustain through
  `snd_direct_voice`, moves `active_voices` to `paused_voices` and sets `SND_MUSIC_PAUSED`. `9A` rewrites volume and
  sustain, moves the masks back and resends the noise, reverb and FM voices. `9D`/`9C` do the same for the effects
  (bit 1), but a kind 2 effect on channels 54/55 keeps playing.
- **`F4`/`F5`:** `F4` runs `snd_keep_looping_effects` (an effect whose stream ends in `CA` stays), sets the effects
  aside (their 8 channels into `seq_channels[24..31]`, 12 words of their block after them), stops them and runs `B8`
  (`0x7F`) and `BC` (0) on every pair. `F5` copies them back and keys off the voices they no longer hold.

**Per-tick work:**
- **`snd_timer_tick`** runs the ticks due since the last call (scanlines ÷ 66, 1–8, doubled by `SND_DOUBLE_SPEED`),
  keeping the last four calls' times in `snd_tick_time[4]` and their sum in `snd_tick_time_total`.
- **`seq_tick`:** each block adds its tempo, scaled by `snd_tempo_scale` (top byte, read unsigned), to its phase; on a
  carry every channel's gate counts down, the key-off at the high byte and the next event (`seq_step_channel`) at the
  low one. Effects run only while not paused (except kind 2), counting `effect_ticks`. Then the first song's tempo,
  reverb and meter counters advance, a pending `F3` pause runs as `9B`, the queue flushes, the global slides run and
  voices key off. It reaches the first song through plain aliases (tempo, phase, slides, meter, pause flag) and the
  second through `seq_song2`'s fields.
- **`snd_slow_slides`** (every 4th tick, `snd_slow_slide_tick`): CD volume, the master volume (a fade to 0 stops the
  songs), the tempo and pitch scales (a pitch-scale step rewrites every music pitch); each effect's balance, pan and
  pitch-shift slides (a balance fade to 0 ends it); and the cross-fade: when channel n's fade ends, the old song's
  channel ends, the new song's rewrites everything, and the fade restarts as a fade-in.
- **`seq_channel_slides` / `seq_effect_slides`:** volume, balance and pan (music only), the vibrato and tremolo delays
  (music only), the timed noise and FM flips (on the song's masks or the effects'), the three LFO depth slides with
  output recomputed, and the `+0x34` pitch slide.
- **`seq_channel_lfo`** (music voice update): each LFO that's on and past its delay steps through its waveform.
  Volume is A8 × A3 (or the previous channel's pitch, `D6`) plus tremolo, × `snd_master_volume`, × the cross-fade,
  split by pan plus pan LFO through `snd_pan_left` / `snd_pan_right`. Pitch is the note's (or the previous channel's,
  `D4` mode) plus vibrato and `+0x34`, then scaled by `snd_pitch_scale`'s top byte read unsigned: × (128 + n) / 128
  below 128, × n / 256 from 128 up, so negative values lower the pitch.
- **`seq_effect_lfo`:** the same without LFO delays, master volume and cross-fade; the effect's balance scales its
  volume, pan places it, and its pitch shift scales the pitch as `snd_pitch_scale` does. Kind 2 skips both, centred.
- **`seq_update_voices`** (output pass): a changed reverb depth goes to the SPU scaled by `snd_reverb_scale` and split
  by `snd_reverb_balance`; a changed noise clock goes too. For both song blocks and the effects, each voice due an
  update runs its LFO update and writes what changed: layer channels through `seq_layer_voice_update` (main voice volume
  × (127 − n) / 256, layer n / 256, pitch copied), overlap channels to whichever voice their turn is on (flag `0x400`
  flips on each key-on; in the music block a busy overlap voice falls back to the channel's own). Then the due voices
  key on together.
- **`snd_update_voice`** hands each group with update bits set to libspu (pitch, volume, start, loop, then the
  envelope pairs: sustain, attack, release, decay with sustain level), clearing each group's bits until none are left.
- **Voice masks to the SPU:** `snd_apply_noise_voices`, `_reverb_voices` and `_fm_voices` build one mask from both
  song blocks and the effects' copy, after `snd_add_spare_voices` adds each channel's `F4` layer or `F8` overlap
  voice, and set it on and its complement off. `snd_key_off_voices` does the same with `key_off_voices`.
  `snd_refresh_effect_volumes` walks `effect_voices` from bit `0x10000` against slot 48; its twin walks
  `active_voices` from slot 0.
- **`snd_stop_song`:** every voice the song holds (layers and overlaps too) gets a 2-tick key-off and steps into
  `seq_end_stub`; the song's masks are cleared.

**Setup and helpers:**
- `snd_effect_streams`: an effect's two streams, `effect_data` + the 16-bit offsets 2·id and 2·id + 1 in
  `effect_offsets` (`0xFFFF` = none).
- `channel_reset`: instrument 5, octave 2, volume 100, multiplier 120, some twenty fields zeroed.
- `snd_set_reverb_mode`: on a change it stores the mode at `+0x3C`, turns reverb off, sets the mode with its work area
  cleared (`0x100`) through libspu's `SpuReverbAttr`, and turns it back on.
- **The sample stream** (`snd_stream_*`): sample data streams from main RAM through a double buffer at SPU
  `0x77000`–`0x78FFF`, on voice 16 (mono) or 16/17 (stereo, 2 KB each per half). `snd_stream_start` reads the header
  (total size, flags: bit 0 stereo, bit 1 loops, top half `snd_stream_pitch`; loop offset; a spare word), primes
  8 KB and stops the `0x30` pair. `snd_stream_voice_mono` / `snd_stream_voices_stereo` set the voices up through
  `snd_direct_voice`; `snd_stream_play_mono` / `_stereo` arm the first IRQ and key on. Each half's IRQ
  (`snd_stream_fill_*_a` / `_b`) copies the next 4 KB into the other half with `SpuWrite` and re-arms. At the end it
  restarts from `snd_stream_loop` (`snd_stream_loop_size` bytes) or `snd_stream_stop` keys the voices off.
- **`sound_init.c`:** the SPU transfer helpers (`snd_transfer_write`/`read`/`wait`, `snd_transfer_busy`), `snd_reset`
  (the boot state, `0x800294BC`), `snd_init`, `snd_load_effects` (copies EFFECT.ALL, `0xC800` bytes, into its
  buffer), `snd_quit`, and the bank loaders `load_instrument_bank` / `load_instrument_bank2` (all 128 instruments, or
  from 53 on).

## 8. Sony library map (main exe)

`tools/psyq_match.py` parses SN `.LIB` archives (`LIB` with `LNK` objects), masks each relocated field by its patch
type and searches the exe for each library module, falling back to single functions. `--ref-exe EXE NM_LIST` also
matches a linked library EXE named by an `nm` listing. Only names and addresses come out, into
`config/symbols.main.psyq.txt`, which splat reads; the rebuild stays byte-identical.

Sources, in priority order (on a rename the first wins, so the closest version goes first):

| Source | Where | Version stamps (libetc / libcd / libgpu) | Names alone |
|---|---|---|---|
| PsyQ **3.5** libs | `LIB35.ZIP` (1996-05) from a PsyQ SDK archive | 1.73 / 1.77 / 1.120 | 247 |
| PsyQ 4.4/4.5 libs | PsyQ 4.4 SDK (`psx/lib`, patches, `Lib44`, `Lib45`) | 1.75 / 1.86 / 1.140 | 154 |
| Net Yaroze resident library | `MON/LIBPS.EXE` on DTL-S3030/S3035, named by `PSX/LIB/LIBPS.A` on DTL-S3040 (`nm` listing) | 1.73 / 1.76 / 1.120 | 40 |
| **FF7 itself** | `SCUS_941.63` | 1.73 / 1.76 / 1.115 | |

Combined: **267 of 454** functions in the library block `0x80036E84`–`0x80048D18` named (58%): libgte 110, libgpu 40,
libapi 40, libspu 14, libcd 13, libetc 12, libc 14, libcard 10, libpress 5, Net-Yaroze-only 10. The libspu names
(`SpuSetReverb`, `SpuSetIRQCallback`, `SpuGetKeyStatus`, `SpuMallocWithStartAddr`) are anchors for the AKAO driver.

Rules that keep false names out:
- **Unique:** a hit occurs exactly once in the exe.
- **Distinctive:** a hit needs at least 10 words not touched by relocations, or no relocations and at least 4 words
  (BIOS call stubs carry a unique call number). This rejects e.g. `atof` at `0x800144F0` in game code.
- **Siblings:** smaller functions count only when a distinctive sibling from the same module puts the module at the
  same base.
- **Clashes:** two names for one address are dropped, unless both come from the same library: then it's an API renamed
  between versions (`InitPAD` → `InitPAD2`, `delete` → `erase`) and the first-listed source wins. Dropped example:
  `0x80048A7C`, a "call three functions" wrapper that fits both libsnd `SsInitHot` and libmcrd `MemCardInit`.
- **One name per address:** a name found at two addresses stays with the higher-priority source (3.5 `StartCARD` is
  a BIOS stub; 4.4's is a different wrapper).
- **Reference EXEs** have no relocation records, so jump targets, `lui` immediates, and offsets from a `lui`-loaded
  register or `$gp` are masked.

Review candidates (`--fuzzy 0.8`, not applied), with similarity scores. Fuzzy names need checking: of 20 guesses
from the 4.4 libs alone, 8 are confirmed by exact matches and 2 are wrong (`0x80038FEC` is not
`SpuStSetPreparationFinishedCallback`, `0x80043928` is not `ReadInitPadFlag`).
- **libspu** (1.00 but too small to pass alone): `_SpuDataCallback` `0x80036FD8`, `SpuSetNoiseVoice` `0x80037964`,
  `SpuSetReverbVoice` `0x800388C4`, `_SpuCallback` `0x80038C48`, `SpuSetTransferMode` `0x80038FB8`,
  `SpuSetTransferCallback` `0x80038FEC`, `SpuSetPitchLFOVoice` `0x80039010`; `_SpuIsInAllocateArea_` `0x80037D90` 0.92.
- **libcd:** `CdControl` `0x8003DE9C` 0.90, `CdControlF` `0x8003DFD4` 0.84, `CdMix` `0x8003E244` 0.89, `CdGetSector`
  `0x8003E268` 0.89, `CdLastPos` `0x8003DD08` 1.00, `CdPosToInt` `0x8003E3D4` 1.00, `CD_vol` `0x8003F398` 0.91,
  `CD_initvol` `0x8003F500` 0.85, `StSetRing` `0x8003DBA0` 1.00, `StClearRing` `0x80040898` 1.00, `StSetMask`
  `0x80040C88` 1.00, `CdGetToc` `0x80042144` 1.00, `CdGetDiskType` `0x8004076C` 0.81.
- **libgpu:** `ClearOTag` `0x8004418C` 0.85, `DrawOTag` `0x8004433C` 0.90, `SetDrawMode` `0x80044A68` 0.91,
  `SetSemiTrans` `0x80046848` 0.90, `SetShadeTex` `0x80046870` 0.90, `BreakDraw` `0x800484A8` 0.89.
- **libetc:** `GetVideoMode` `0x80043928` 1.00, `startIntrDMA` `0x8003D8F8` 0.81.

The other ~187 unnamed functions are probably from PsyQ 3.3/3.4 builds or are static helpers with no exported name;
the game's libgpu `sys.c 1.115` is older than every source we have.

**Game code ends just before `0x80036E84`.** Open question: are the last unnamed functions before it (`0x80036D98`,
`0x80036DE0`) game code or the start of libspu?
