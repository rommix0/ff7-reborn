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
| `0x8002DF88` | `snd_run_command` | Runs one sound command immediately. Shorthands: `24`–`27` play 1–4 effects with consecutive ids (as `20`–`23`); `98`/`99` run `9A`+`9C` / `9B`+`9D`; `D8`–`DA` run `D0`–`D2` with `D4`–`D6`. The song commands do nothing here. Then `snd_command_handlers[cmd](entry)` |
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
| 2 | `A4` `A9` `AB` `B0` `BC` `DD`–`DF` `E8` `EA` `EC` · `EE` jump (stops linear parsing) · `F4` `F7` `FD` `FE` |
| 3 | `B4` `B8` `E9` `EB` `EF` `F0` `F1` |
| — | `E0`–`E7` `FA`–`FC` `FF`: unused; the driver treats them as `A0` |

- **Lengths confirmed by real data** for every command except `B7`, `BE`, `BF`, `CE`, `CF`, `DF`, `E9`, `EB`, `EF`, `F1`
  and `F3`. None of those occur on the discs, so their lengths rest on the handler code alone.
- **Structure quirks:**
  - Channels may fall through into the next channel's bytes (shared tails).
  - Some songs leave trailing bytes after the last channel.
- **Behaviour notes:**
  - **Read in detail and confirmed by the MIDI loop check (below):** `A0`, `A1`, `A2`, `A5`–`A7`, `C8`–`CA`, `DC`, `E8`,
    `EE` and `F0`.
  - **Read in detail (volume, drums and pitch, below):** `A3`, `A4`, `A8`, `A9`, `AA`, `AB`, `C0`, `C1`, `D8`, `D9`,
    `DA`, `EC` and `ED`.
  - **Read in detail (LFOs and bars, below):** `B4`–`B6`, `B8`–`BA`, `BC`, `BD`, `DD`, `DE`, `FD` and `FE`.
  - **Read in detail (articulation and envelopes, below):** `AD`–`AF`, `B1`, `B2`, `CC`, `CD`, `D0` and `D1`.
  - **Not studied yet:** the rest.
- **`FD t n`: time signature** (`0x80033548`). It writes ticks per beat (`+0x5A`) and beats per bar (`+0x56`) into
  the song state and resets the beat and tick counters, so the bar starts where it stands.
  - **In the songs:** `30 04` (4/4) in 132 places, also 3/4, 2/4, 5/4–8/4 and 3/8–10/8 (`18 n`). Song 76
    alternates three bars of 6/8 with one of 5/8; song 71 runs through 6/4, 7/4 and 8/4. Songs 1, 66 and 89 have
    none.
  - **Lead-ins:** most songs open with a 1–4 tick rest, then `FD` + `FE 01 00`, so their bar lines sit a few ticks
    after tick 0. Some open with a pickup bar (song 47: `FD 30 01`).
- **`FE lo hi`: bar number** (`0x80033588`), a `u16` at `+0x5E`. Usually `01 00` at the start of bar 1.
- **Key-off timing.** `seq_note_lengths` (`0x80049C28`) holds `u16`s: the key-off countdown in the high byte and
  the length in the low byte (`0x3030` = 48, 48). `seq_tick` takes `0x101` off `+0x56` each tick. When the low byte
  runs out, the channel steps; when the high byte does, the voice keys off.
  - **Early key-off:** for an ordinary note, `seq_step_channel` subtracts `0x200`, so the voice keys off 2 ticks
    before the next event. Notes of 2 ticks or less never key off early.
  - **Held to the end:** a note keeps its full length when the next byte is a tie, or in slur or full-length mode.
  - **A tie after a rest** keys nothing on: neither the tie path nor the rest path sets the key-on mask, so the note
    released at the rest stays released and `n r t` is a note and a longer rest. A pending `A4` bend is still
    spent on that tie, on the silent voice.
    - **Player bug (fixed):** the parser added the tie to the last note across the rest, so the note sounded
      through both. Song 12's organ chords (channels 0–2, `n0/8 r/7 t/7`) rang on through every gap, against the
      game, where they stop at the rest.
- **`CC` slur and `D0` full length** (`0x80032D44`, `0x80032D58`) set the note mode `+0x6E` to 1 and 4.
  - **Mode bits:** at each note or tie, bit 1 takes bit 0, and a note with bit 1 set skips key-on. It takes over
    the sounding voice with a new pitch and no new attack. So `CC n1 n2` slurs n1 into n2.
  - **Everything else in the note setup still runs:** in `seq_step_channel` the slur branch (`bnez .L80031454`)
    skips only the key-on mask. The new pitch, the restart of vibrato, tremolo and pan LFO (with their current
    settings, delay included), and the zeroed vibrato and bend offsets all still happen.
    - **So:** a long slurred line under a delayed vibrato starts the delay again on every note.
    - **Before:** the player kept the old LFOs running, which missed depth changes made right before a slur
      (effect 274 raises its vibrato from `0x83` to `0xA3` that way).
    - **Reach:** 53 songs and 176 effects slur with an LFO on. Song 97 does it most (2,887 notes).
  - **Ending the mode:** `CD` and `D1` (`0x80032D50`, `0x80032D64`) are empty. The look-ahead `seq_look_ahead`
    (`0x800318BC`) runs before each note and scans to the next note, following `C9`/`CA`/`EE`/`F0` like the
    sequencer.
    - **Clear and keep scanning:** `CB`, `CD`, `D1` or `DB` clear bits 0 and 2.
    - **Clear and stop:** a rest, `A0`, or an unused command.
    - So `CC n1 n2 CD n3` holds n1 into n2, keys n2 off early, and starts n3 fresh.
  - **In the songs:** `CC`/`CD` appear 1,601 times in 56 songs, and 12,039 of 156,161 notes are slurred. `D0`/`D1`
    appear 97 times in 14 songs.
- **`AD`, `AE`, `AF`, `B1`, `B2`: the channel's envelope** (`0x80032E08`…`0x80032F98`).
  - **What they set:** attack rate, decay rate, sustain level, sustain rate and release rate (`+0xFA`…`+0x102`),
    with a voice-update flag so they reach a sounding note.
  - **Reset:** `load_channel_instrument` copies all five, and the three modes, from the instrument on every program
    change (the drum-kit path does the same per note).
  - **In the songs:** `AD` in 47 songs, `B1` in 35, `AF` in 7, `AE` in 5 and `B2` in 1: 476 overrides in all.
  - **Mode commands:** `B7`, `BB` and `BF` (the envelope modes) never occur.
  - **Rate changes mid-step (player bug, fixed):** the SPU waits a number of samples per envelope step that grows
    with the rate, up to 2^20 samples (about 24 s) at sustain rate `0x7F`, the "hold" most instruments use. The
    player carried the samples already waited over to the next rate as they were, so a faster rate cashed them in
    at once.
    - **Where it showed:** effect 448's `B1 3E` mid-note (6 down every 16 samples) dropped the note to silence
      in one tick, halfway through. Any release slower than a few milliseconds, after such a sustain, was cut off
      too: release rate 12 lasted 0 ms instead of 180.
    - **Now:** the carry is kept as a fraction of a step, which a new rate continues.
    - **Reach:** 50 songs and 193 effects change an envelope rate on a cell without a note, and any song with a
      slow release (`B2`, the `F8` ring) after a held sustain.
- **Song start** (`snd_start_song`): tempo `0xFFFF` (`D_8009A11E`) and accumulator 1 (`D_8009A124`).
  - **Before the first `E8`:** a song runs at about a tick per timer interrupt. Song 50 sets its tempo at tick 4,
    and song 85 at tick 72.
  - **Player bug this caused:** the player used to start at tempo 0, so those two songs never got past tick 0.
- **Reverb.** The PS1's notes mostly die at key-off: most instruments have release rate 5, linear, about 1.5 ms.
  The tail you hear in the game is SPU reverb.
  - **Mode:** the song header's `u16` at `+8`. The song-start code (`func_8002DA7C`) passes it to
    `snd_set_reverb_mode` (`0x80029AF0`), which sets libspu's reverb type. 45 songs use 1 (Room), 40 use 3
    (Studio B, medium), and 13 use 4 (Studio C, large).
  - **Setting the mode:** only when it changes. It calls `SpuSetReverbModeParam` with mask 1 and mode
    `| 0x100` (clear the work area), turning reverb off around it.
  - **Presets:** libspu's tables in the executable. `spu_reverb_work_starts` (`0x8004AB5C`) is ten `u32`s, the work
    area start per mode in 8-byte units (Room `0xFB28`: 4,960 samples; Studio B `0xF6F8`: 9,248; Studio C `0xF204`:
    14,320). `spu_reverb_presets` (`0x8004ABAC`) is ten 0x44-byte entries: a `u32` mask, then the 32 registers
    `0x1F801DC0`–`0x1F801DFE`. All three presets FF7 uses take `vLIN`/`vRIN` = `0x8000` (−1).
  - **Depth:** `EA lo hi` (`seq_cmd_reverb_depth`, `0x80031BA0`) sets it to `(hi << 8 | lo) << 16`. It's zeroed at
    song start and used in 93 songs, mostly `EA 00 28` (`0x2800`).
  - **Depth to the SPU:** `seq_update_voices` takes the top half (`0x8009A146`), scales it by the game's sound setting
    `0x80062FB8` (0 at boot: unchanged) and pans it by `0x80062F70` (`0x40` at boot: both sides equal). Then
    `SpuSetReverbDepth` writes it as the reverb output volume (`vLOUT`/`vROUT`), so `0x2800` = 0.31.
  - **In the player** (`Reverb` in `tools/akao_synth.py`): psx-spx's formula on a copy of the work area, at
    22.05 kHz, resampled to and from the output rate. The sends are the voices' panned outputs, as on the SPU.
    - **Resampling:** the SPU halves the rate on the way in and doubles it on the way out through a 39-tap
      half-band low-pass (Mednafen's `spu_reverb.inc`, read to confirm). We use our own windowed sinc (Kaiser,
      β 13, cut off at 11.025 kHz). Its response matches the hardware's within 0.1 dB up to 12 kHz, and every
      coefficient is within 13/32768.
      - **Before:** a 2-sample average going in and straight-line interpolation coming out. That let through
        aliases only 4 dB down above 12.5 kHz, where the hardware's are 17 dB down at 12.5 kHz and 38 dB down at
        14 kHz.
      - **Where it shows:** bright, aliasing voices such as effect 270's FM voice sound much cleaner through the
        reverb, as they do in the game.
    - **Aliasing in the voices themselves is real.** Above 44.1 kHz the SPU just skips samples, and the 4-point
      Gaussian is the only filtering (psx-spx). Neither the driver nor Mednafen adds any. FM voices that swing up
      to the `0x4000` step limit alias on the console too, and in the game their reverb send softens that.
    - **Speed:** it works a stretch of up to 512 samples at a time. All reads come before the writes, which
      matches the sample-by-sample formula while the stretch is shorter than the shortest delay from a write to a
      read. Each filter's feedback from its own last output is computed as a filter. A test checks it against a
      plain per-sample version to within 1e-9.
    - **Cost:** about 2% CPU for the presets FF7 uses. Echo and Delay have 4-sample delays, so they run slowly,
      but no song uses them.
    - **What it changes:** over 20 s, reverb adds 4–17% RMS: song 42 (Room) 0.086 → 0.089, song 96 (Studio B)
      0.047 → 0.051, song 82 (Studio C) 0.067 → 0.078.
    - **Known gap:** Room's two "diff" filters both write offset 0, so the SPU's left filter reads the right
      one's last output. Ours uses its own. Nothing reaches the output from there (`vCOMB3`/`vCOMB4` are 0).
  - **Impulse responses** (`tools/reverb_ir.py`): the reverb is linear, so a click through it records each type
    for convolution reverbs. Levels are at full depth. The tails, trimmed where they fall below 24-bit:
    Room 1.65 s, Studio A 2.00 s, Studio B 2.07 s, Studio C 4.76 s, Hall 4.02 s, Space Echo 8.15 s,
    Echo 11.87 s, Delay 0.74 s (a single repeat) and Pipe 3.94 s.
  - **Per channel:** `C2` turns a channel's reverb send on and `C3` turns it off (1,876 and 59 uses). Song 42
    turns it on for all 16 channels.
- **`F4 a b` / `F5`: layer** (`0x80031D6C` / `0x80031E98`).
  - **Second voice:** `F4` reserves a free SPU voice for the channel (`+0x24`, flag `0x100`). It loads instrument
    `a` for the channel and instrument `b` into that voice's channel slot. Every key-on keys on both voices.
  - **Shared settings:** the envelope commands write both. `seq_layer_voice_update` (`0x8002F738`) copies the main
    voice's pitch register (`+0xF8`) to the layer, so the layer plays the main voice's pitch, vibrato included.
  - **`F5`** frees the voice.
- **`F6 n` / `F7 t n`: layer balance** (`0x80031EEC` / `0x80031F30`): `+0xC6 = n << 8`. In layer mode the main voice
  plays at `(127 − n) / 256` of its volume and the layer at `n / 256`. `F7` slides the balance to `n` over `t`
  ticks (0 = 256).
  - **In the songs:** only song 10, channels 1–5: instruments 84 and 83, crossfaded.
- **`C6` / `C7`: FM on / off** (`0x80032BB4` / `0x80032C20`).
  - **Effect:** they set and clear the channel's bit in the song's pitch-modulation mask (`+0x34`), which
    `snd_apply_fm_voices` hands to the SPU. The voice's sample step then follows the previous voice's output: step ×
    (1 + output).
  - **In the songs:** 53, 66 and 89.
  - **The SPU's rule** (psx-spx): step = pitch register × (previous voice's output after its envelope, + 0x8000)
    >> 15, then anything over `0x3FFF` becomes `0x4000`. So the cap comes after the modulation. We used to cap
    first, which let effect 270's FM voice (register `0x2B2E`) swing up to `0x565C`, far past what the chip
    plays.
- **Pitch register: masked, not clamped.** Every voice update writes the pitch as `& 0x3FFF` (`0x8002F1D0`,
  `0x8002F23C` for songs, `0x8002F6AC`, `0x8002F728` for effects). The setter at `0x800395C8` copies that straight
  into the voice, so a pitch past `0x3FFF` wraps round to a low one; the player now does the same.
  - **Songs:** 2, 9, 53 and 72 have notes past it. Song 2's channel 10 plays instrument 30, whose own table
    starts at `0x6590` for octave 6, so its key-85 notes (`0x6BA0`) play at `0x2BA0`. That's 6.6 semitones below
    the `0x3FFF` we used to clamp to.
  - **Effects:** 87 have such notes.
  - **XM export:** it still gives those notes their unwrapped pitch.
- **Key-off freezes the voice.** A key-off also clears the voice's bit in the update mask (songs: `seq_song + 0xC`,
  set at key-on in `seq_step_channel`, cleared in `seq_tick`; effects: `0x80099FD4`). `seq_update_voices` only runs
  the voice update (`seq_channel_lfo` / `seq_effect_lfo`) and `snd_update_voice` for voices in that mask.
  - **So:** through the release the SPU keeps the pitch, volume and pan it was last given. Bends, volume slides
    and LFOs stop where the key-off found them, and the LFOs stop stepping. FM still applies, since the chip does
    that itself.
  - **Before:** the player kept updating released voices. Effect 447's cymbal keys off mid-swoop, and we kept
    bending it towards a pitch that would have wrapped. Its release is over by then, so it made no audible
    difference there, but slower releases under a bend, slide or LFO now hold still as on the console.
- **Opcode audit (98 songs).**
  - **Handled:** every command a song uses is handled.
  - **No audio effect:** `FE` is a bar counter for the game.
  - **Never used in songs:** `AC B0 B3 B7 BB BE BF C4 C5 CB CE CF D2–D7 D9 DA DB DF E9 EB EF F1 F3` and the unused
    `E0–E7 FA–FC FF`. `DA` (glide) appears only in sound effects.
- **`F8 n`: overlap** (`0x80033128`).
  - **Release rate:** sets the channel's release rate to `n`.
  - **Second voice:** on first use, reserves a free SPU voice for the channel (`+0x28`, flag `0x200`). From then
    on, every key-on toggles flag `0x400` in the voice update (`0x8002FA74`), so notes take turns between the two
    voices. Each note rings on in its release while the next one starts, like a plucked string. With `0x200`
    set, a program change keeps the release rate (the update mask `0x1BB80` leaves it out), and so does a
    drum-kit note.
  - **`F9` ends it** (`0x800331CC`): frees the second voice and takes the release rate from the instrument again.
  - **In the songs:** 48 uses in 18 songs, almost all `F8 0C` (about a 2-second ring). Song 96's plucked part
    (channels 11–13, instrument 21 with `AD 20` and `B1 33`) is one. `F9` is used once, in song 2.

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
  - **`C9 n`:** jumps back until pass `n` (0 = 256), then pops. We used to read `C9 00` as endless: effects 294
    and 369 now end after their 256 passes, and effect 396 goes on to the notes after its repeat.
    - **The note look-ahead** (`func_800318BC`) compares the raw count instead, so at a `C9 00` it always
      assumes another pass. `look_ahead` does the same on purpose. It also compares `F0`/`F1` counts raw and
      follows `EF` and `F1`, where ours reads `F0 00` as 256 and stops at `EF`/`F1`. No song or effect uses
      `F0 00`, `EF` or `F1`, so that difference is never heard.
  - **`CA`:** jumps back forever.
  - **`F0 n lo hi`:** jumps by a signed 16-bit offset on pass `n` of the current repeat, for a different ending on the
    last pass.
  - **`EE lo hi`:** jumps by a signed 16-bit offset, counted from after its arguments; a backward jump is the song's
    endless loop.
- **Instrument commands:** `A1 n` selects instrument `n`, and frees an `F4` layer's second voice. `F2 n` also
  changes instrument; both rescale a sounding note's pitch from the old instrument to the new one.
  - **`F2` plays from the loop point** (`0x80032274`): it sets the voice's sample start to SPU `0x76FE0` instead of
    the instrument's, and keeps the instrument's loop address. `snd_init` uploads 32 bytes there from `0x8004A60C`:
    two silent ADPCM blocks, the second flagged end + repeat. So every note after `F2` plays 56 silent samples and
    jumps to the loop: the attack part of the sample is skipped. It lasts until the next `A1`.
  - **In the data:** 21 songs use `F2` (42, 47, 48, 49, 71, 78, 80–82, 84, 85 and others), all on notes; no effect
    does. We used to treat it as `A1`, which played their attacks. The export marks it `from_loop`; the player
    skips the 56 silent samples.

`python tools/akao.py midi <file|dir> <out>` writes one Standard MIDI File per song:
- **Layout:** format 1 at 48 PPQ, a conductor track with tempos plus `loopStart`/`loopEnd` markers, and one track per
  channel (MIDI channel = index mod 16).
- **Notes:** key = 12 × (octave + 1) + semitone, so octave 4 = middle C.
- **Controls:** program = AKAO instrument; CC7 = `A8`; CC10 = `AA`; velocity is fixed at 100.
- **Loops:** counted repeats are unrolled, and endless loops play once.
- **Left out:** vibrato/tremolo/pan LFOs, slides, ADSR, reverb, drum-kit mode and the other effect commands.

**Song loop.** Each channel loops on its own (`EE`/`CA` back to its own target), and in 38 songs the channels don't
all start looping at the same tick. Song 7 is an example: 13 channels loop from tick 768, three from tick 1152.
The song as a whole repeats from the **latest** channel loop start, every lcm(loop lengths) ticks (in practice the
longest loop). `play_song` keeps the earlier channels looping until that point and cuts everything there, and both
exports use it. No command is involved; it's how the data is laid out. Before this, exports looped from the
earliest start to the latest end, which added extra bars: song 7 had 5,760 ticks per loop instead of 5,376, and
song 74 had 6,147 instead of 4,608.

**A loop's first pass can play differently.** After `CC` (slur) before a loop, every pass but the first slurs into
its note with no key-on, so the sample keeps running instead of restarting each time (effect 493, a drone). A bend
(`A4`) set before a loop fires on the first pass only (effect 244). The export compares each looping channel's
notes, slurs and bends over three passes; when the first differs and the next two match, the loop starts a pass
later. That moves 14 effects' loops and no song's.

Check: in all **98 songs, every looping channel repeats the same length**, or a length that divides it, such as a
one-bar drum loop. Echo channels may start a few ticks late. Before `F0` was handled, 5 songs failed this check,
which is how its meaning was confirmed. No channel runs past 4,000 bars.

### Channel defaults, volume and drum kits

- **Channel reset.** `channel_reset` (`0x80029BAC`) starts every channel on instrument 5 (via
  `load_channel_instrument`), octave 2, volume 100 (`+0x44` = `0x32000000`) and an `A3` multiplier of 120
  (`+0x2C` = `0x78`).
- **Voice volume.** Computed in `seq_channel_lfo` (`0x8002ED34`):
  - **Base:** (`A8` volume `<< 7`) × `A3` ÷ 128, plus a tremolo offset (`+0xD8`).
  - **Master volume:** scaled by the global master volume `0x80062F5E` ÷ 128. It's `0x7F` at boot (`0x800294A4`
    stores `0x7F0000` to `snd_master_volume`).
  - **Output mode:** the word at `0x8009A104` (named `seq_song` in our symbols) picks how the volume is split. The
    sound-setting functions at `0x8002C7E8`, `0x8002C81C` and `0x8002C850` set it to 1, 4 and 2, and boot sets 1.
    - **1, stereo:** left = volume × `snd_pan_left`[i] >> 15, right = volume × `snd_pan_right`[i] >> 15.
    - **4:** the same, but odd-numbered voices get their right side inverted (`~right`).
    - **Anything else (mono):** both sides = volume × `0x80049CC4` (entry 64 of the left table, 16,256) >> 15.
  - **Pan tables:** i = (`AA` pan + pan-LFO offset `+0xDA`) & 0xFF. Entries 0–127 are linear: left
    32,640 − 256·i, right 128 + 256·i. The two always add up to 32,768, so the centre is −6 dB per side, not the
    −3 dB of equal-power panning. Entries 128–255 hold unrelated data (signed squares). Only a pan LFO could reach
    them, and simulating all four pan-LFO songs keeps i within 5–114.
  - **On the SPU:** a volume register r plays at r ÷ `0x4000` of full scale (psx-spx). So a voice plays at
    `A8` × `A3` × 127/128 ÷ 16,384 × the pan table entry ÷ 32,768.
- **`A3 n`:** the multiplier above. In songs, 1,317 of 2,108 uses are 127, and the rest go as low as 10.
- **`A9 n v` / `AB n p`:** slide volume or pan to `v`/`p` over `n` ticks (0 = 256). The per-tick step sits at
  `+0x48`/`+0xCA` and the counter at `+0x5C`/`+0x62`. `A8`/`AA` cancel a running slide. 266 of the slides fade in
  from volume 0.
- **`EC lo hi` / `ED`:** drum-kit mode on/off (channel flag bit 3). The map pointer (`+0x14`) is a signed offset from
  after the arguments.
  - **Map lookup:** each note's semitone picks a 5-byte entry: instrument, key (octave = key ÷ 12, same numbering
    as `A5`), `u16` volume (stored `<< 16` into `+0x44`, so the `A8` equivalent is the value `>> 7`), and pan
    (`<< 8` into `+0x60`).
  - **Scale:** 4,585 notes as written (11,645 once loops are played out) are drum notes.
- **Envelope (ADSR).**
  - **When it's sent:** when a note starts, the driver copies instrument bytes `+0x08`–`+0x0F` into the voice, and
    `snd_update_voice` hands them to libspu.
  - **How the bytes pack**, checked against the register writes and psx-spx:

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

Read from `seq_step_channel`'s note and tie paths:
- **Key:** octave × 12 + semitone + transpose. The pitch comes from the instrument's table for that key, then the
  detune is applied.
- **`C0 s` / `C1 s` (`seq_cmd_transpose`, `_add`):** set or add a signed semitone transpose (`+0xCC`). Drum-kit
  notes ignore it.
- **`D8 d` / `D9 d` (`seq_cmd_detune`, `_add`):** set or add a signed detune (`+0xCE`). It scales the pitch by
  1 + d/128 when positive and 1 + d/256 when negative.
  - **Values in the songs:** mostly −2…+1 (≈ ±14 cents, chorus-style), plus −128 (exactly one octave down) and ±60.
- **`A4 n s` (`seq_cmd_bend`):** stores a slide length `n` (0 = 256, at `+0x68`) and a signed semitone amount `s`
  (`+0xD2`).
  - **Start:** the next **note or tie** starts the bend. Both reach the same check in `seq_step_channel` (`.L8003161C`,
    after a note's key-on): a non-zero `+0xD2` makes the target key the current key + `s`, and the pitch slides
    linearly *in frequency* to the target's pitch (transpose and detune included) over `n` ticks.
  - **Songs and effects:** most songs put `A4` before a tie. Effects often put it before a note (effect 1: +15 and
    +12; effect 36: +32), and so do songs 9 and 66. We used to start bends only on ties, which dropped those.
  - **Glide:** a `DA` glide writes the same fields on a note, so a pending `A4` bend is lost.
  - **Target key:** `+0xD0` (the note's key before transpose) += `s`, plus the transpose `+0xCC` *now*. A `C1`
    between the note and the bend therefore moves the target. Effect 36's last bend comes after a loop of `C1 02`, so
    it lands 2 semitones higher than `s` above the held note (key 90, where we had 88). The export folds this into the
    bend's semitones.
  - **Effects skip the detune:** with flag `+0x54` set, the target pitch comes straight from the instrument's table,
    without the `+0xCE` detune scaling that songs apply.
  - **One-shot:** the amount is then cleared, and later ties hold the new pitch.
  - **Song 7 channel 16:** a 2-tick note, `A4 04 01`, a 10-tick tie, repeated.
  - **Scale:** 321 bends in the songs, from ±1 semitone over 4 ticks up to +65 over 8.
- **`DA n` (`seq_cmd_glide`, `0x8003257C`):** a glide (portamento). No song uses it, only sound effects (43 uses).
  - **Setting it:** the glide time `+0x6C` = n (0 = 256), and slur mode on (`+0x6E` = 1, as `CC`). It also clears the
    last key (`+0x6A`) and its transpose (`+0xD4`), so the first note after it doesn't glide. 14 effects play a
    note before `DA`, and we used to glide their first note from it.
  - **On each note** (`seq_step_channel`, `0x8003131C`): with a glide time and a last key (`+0x6A`, 0 = none), the
    note starts on the last key and bends to its own over the glide time, through the bend fields `+0x68`/`+0xD2`.
    The last key is then this note's.
  - **Ending:** the look-ahead clears the glide time wherever it ends slur mode. It runs before the glide check, so
    the note just before a rest doesn't glide.

### Sound effects

- **The data:** `SOUND/EFFECT.ALL`. Its table's entries 2·id and 2·id + 1 (`func_8002A6C4`) are an effect's two
  channels, as offsets from `0x1000` (`0xFFFF` = none). That gives 721 effects: 702 with two channels and 19 with
  one, the 1,423 streams. 90 loop forever (ambient sounds).
- **Playing one:** the sound commands around `0x20`–`0x34` (we read the handlers at `0x8002B6AC` and
  `0x8002B8B4`) look up the id and hand both streams to `func_8002A094`. It sets up a pair of effect channels
  (`0x80096608` + slot × `0x108`): the pan from the command, and flag `+0x54` = 1.
- **Flag `+0x54`:** `B4`/`B8` (vibrato/tremolo on) take no delay, and a non-zero first byte becomes the depth
  instead.
- **Tempo:** effect channels tick on their own accumulator (`0x80099FE8`), at tempo `0x66A8` (`0x80099FE0`, set
  at boot and never changed): about 97.8 ticks a second, 122 BPM.
- **Noise (`C4`/`C5`, `0x80032ABC`/`0x80032B30`):** they set and clear the channel's bit in the noise mask
  (effects: `0x80099FEC`; songs: `+0x2C`). The SPU then plays its noise generator on that voice instead of the
  sample. 305 of the 721 effects use it (effect 36's first channel hisses, rising).
  - **Clock (`AC n`, `0x80032D6C`):** n sets it (0–63). With either top bit set, it adds n & 0x3F instead, wrapping at
    64. Effects keep it at `0x80099FFA` and songs at `+0x52`; it starts at 0.
  - **One tick, several `AC`s** (effect 36 adds 1, then sets 46): the export folds them into one cell field in the
    driver's order. A set drops the earlier adds, and adds after it go onto it.
  - **The generator** (psx-spx): a 16-bit register that shifts in the inverted parity of bits 15, 12, 11 and 10, at
    most once per 44.1 kHz sample. A timer of 0x20000 >> (clock >> 2) counts down by 4 + (clock & 3) each sample.
    From power-on it repeats every 65,535 shifts. One generator and one clock serve every voice.
  - **Checked:** Sony's docs only give the 0–63 range. Mednafen (GPL, read only to confirm) times it differently,
    but its rate works out the same: a divider every `0x4000 >> shift` samples feeds a counter that shifts on each
    8. Measured in the player: clock 0 gives 1 shift a second, 32 gives 345, 46 gives 4,134, 50 gives 8,269, and 56
    and up give 44,100. All match the formula. Effect 36's noise runs at 50–62, then settles at 46 (about 4 kHz of
    shifts, a dark hiss).
- **Effect-only commands:**
  - **`B0 a b`:** decay rate a, then sustain level b (it calls the `AE` and `AF` handlers).
  - **`B3`:** the instrument's envelope back (all five rates and the three modes).
  - **`BB n`:** sustain mode.
  - **`CB`:** all off: vibrato, tremolo and pan LFO (flags `0x01`/`0x02`/`0x04`, offsets left as they are), noise,
    FM (and its countdown), reverb, flags `0x10`/`0x20`, slur and full length. We used to leave the LFOs on
    (effects 17, 102 and 551).
  - **`D2 n` / `D3 n`:** FM on now (`D2` only), and flipped after n + 1 ticks (0 = 257) by the countdown `+0xA6` in
    `seq_channel_slides`. `C7` stops the countdown.
  - **`D4`/`D5`:** flag `0x10`. The voice's pitch register becomes the previous channel's (`−0x108 + 0xF8`) plus its
    own vibrato and `+0x36`, instead of its own pitch.
  - **`D6`/`D7`:** flag `0x20`. The voice volume becomes the previous channel's pitch register × 2 × `A3` ÷ 128
    instead of `A8` × `A3`, so the volume follows the other voice's pitch. Only effects 35 and 65 use it, both with
    FM. It can pass the SPU's 15-bit volume. The setter at `0x800393C8` masks with `0x7FFF` and doesn't clamp, so
    the register wraps round to a small negative volume. The player does the same.
  - **`DB`:** glide off (`+0x6C` = 0). The look-ahead ends a glide there too.
- **Commands:** none use `E8` (tempo). They use plenty of slurs, detune, bends, LFOs, envelope overrides, FM
  (755) and reverb sends (1,331 `C2`). There are 29 instruments, all in the boot bank.
- **Our export:** `effects/effect_NNNN.ff7song`, which the player opens like songs
  (`python tools/ff7player.py assets/ff7song/effects`). All 721 render with sound. Half end within 3 s, and the
  longest one-shots (115–117) run 7,080 ticks (72 s).
- **Audit (everything effects do differently):**
  - **Commands:** every command in `EFFECT.ALL` is handled. The three without a case need none: `A0` ends a
    stream, and `CD`/`D1` have empty handlers because the look-ahead does their work.
  - **Handlers that branch on the effect flag `+0x54`:** `A0`, `A1`, `AC`, `B4`, `B8`, `C2`–`C7`, `F2`, `F3`.
    - Most only pick the effect copy of a mask or setting: noise (`0x80099FEC`), reverb (`0x80099FF0`), FM
      (`0x80099FF4`) and the noise clock.
    - `B4`/`B8` take a depth instead of a delay (done).
    - `A0` hands the voice back to the music. It keys nothing off: it clears the channel from the effect masks
      (`0x80099FCC`, noise, reverb, FM) and sets the music channel's update flags. A note held to the end
      sounds on at its last volume until the music or the next effect takes the voice. Effect 643's last note
      fades to volume 1 that way: its slide's final step to 0 comes on the `A0` tick, after the voice has left
      the update mask. The player keys everything off when a piece ends without a loop, since it has no music to
      take the voice back.
    - `F3` sets the flag on the song and is never used.
  - **`A1`/`F2` on a sounding note:** the voice's pitch (`+0x30`) is rescaled by the new instrument's C pitch over
    the old one's (`instrument_table` + 0x10). The voice keeps its sample until the next key-on. Effects 86, 309,
    412, 435 and 724 change instrument between a note and its tie, and so do songs 38 and 72. The player now
    rescales.
  - **Their own voice update** (`seq_effect_lfo`, run on effect channels by `seq_update_voices`):
    - Vibrato, tremolo and pan LFO match the songs' version.
    - Volume skips the master volume and uses the channel's own scale (`+0xC6` high byte ÷ 128).
    - Pitch gets the channel's own scale (`+0x3D`).
    - The game sets both scales with sound commands (`func_8002BCCC` sets a pair's volume, `func_8002C2CC` the
      pitch scale), not from the effect data. At boot they're `0x7F00` and 0 (`0x800294A4`'s loop over the effect
      channels), which is what the player plays: 127/128 and no change.
  - **Their own per-tick update** (`seq_effect_slides`, where songs run `seq_channel_slides`):
    - It keeps the volume slide, the noise and FM countdowns (`+0xA4`/`+0xA6`), bends and LFO depth slides.
    - It has no pan slide (`+0x60`/`+0x62`), so `AB` does nothing on an effect (effects 112, 113, 117; the
      export now leaves their pan alone).
    - It has no LFO delay countdown, which effects never need since `B4`/`B8` start them with no delay, and no
      layer balance slide.
    - `seq_effect_lfo` steps the effect LFOs itself (songs do it in `seq_channel_lfo`), on every interrupt while
      the voice is keyed on.
  - **Effect 302's tremolo** (`B8 1F 02 00` on the cymbal, ±24% at 61 Hz) sounds wrong to the ear: without it the
    effect sounds right. The driver's math and timing for it match ours as far as we've read, so it stays. Marked
    settled, along with effects 270, 297 and 643, when the listening audit closed: none has a known driver-side
    cause, and only a recording from the game could reopen them.
  - **Effect 270** was checked step by step against all of the above and against FM in psx-spx and Mednafen. Its
    commands come out as the driver plays them: a 58-semitone swoop and a +16 bend on the modulator, a 1-tick blip
    then a long note with a square vibrato on the FM voice, and their envelopes.
  - **Set by the game at play time, not by the effect:**
    - the pan (the play command's argument; the player uses the centre);
    - the volume and pitch scales above;
    - the reverb (the current song's; the player plays effects dry);
    - stopping the 90 that loop.
  - **Left to context:**
    - Effect 419 plays noise before setting the clock, so in the game it uses whatever clock the last effect
      left. On its own it starts at 0.
    - 25 effects turn FM on for their first channel. The driver ignores it: `C6` (`0x80032BB4`, also behind `D2`)
      sets an effect voice's FM bit only outside the mask `0x555555`, and an effect's first channel is always
      on an even voice. So they never had a modulator in the game either (effects 270, 297, 302, 447 among
      them). The player now drops those `C6`s.

### Audit against the decompiled driver

Ghidra decompiled every driver function the player depends on from `build/main.elf` (our matching build, so the
same code at the same addresses with our names): `seq_tick`, `seq_step_channel`, the look-ahead `func_800318BC`,
the per-tick updates (`func_8002E478` songs, `seq_effect_slides` effects), the voice updates (`func_8002ED34`,
`seq_effect_lfo`), `seq_update_voices` and all 85 sequence command handlers. Each was compared with `tools/akao.py`
and `tools/akao_synth.py`.
- **Notes:** the note, tie and rest paths, the key-off gate (table lengths, `A2`/`DC`, slur and full-length), the
  look-ahead, glides and drum kits all match: no note goes missing.
- **Argument lengths:** checked against the driver's own skip table (`0x800498A8`). `E9` and `EB` take 3 bytes,
  not 2 (neither occurs on the discs).
- **Fixed:** `F2` plays from the loop point (21 songs), `DA` clears the last key (14 effects), `CB` turns the
  LFOs off (3 effects), `C9 00` is 256 passes (3 effects), FM is ignored on an effect's first channel (25
  effects, no audible change), `A1` frees an `F4` layer and `A2` sets the length `DC` counts from (no data hits
  either).
- **Slurred notes and bends:** a slurred note zeroes the bend offset (`+0x34`) but not its countdown (`+0x64`),
  so a running bend carries on from the new note at the same step. The player used to stop it.
- **Left as they are:** `A8`/`AA` read signed and unmasked bytes (no data goes past `0x7F`); volume and pan
  slides keep 1/128 and 1/256 of a step where the export rounds per tick; F4's voice allocation can fail when all
  24 voices are taken.

### Our song format and player

`tools/ff7song.py` and the player/editor `tools/ff7player.py` (spec: [`ff7song_format.md`](ff7song_format.md)) replace
the XM's compromises with the driver's own behaviour.
- **Patterns are bars**, so any `FD` time signature works. Identical bars share a pattern: 81 of 98 songs reuse
  some. The 98 songs come to 9.8 MB of JSON plus a 1.7 MB bank (93 instruments).
- **Events sit at exact ticks**, and effects keep the driver's parameters.
- **Notes keep their full length across the loop end**, as the driver holds them:
  - **Tied across the loop:** a channel whose loop opens with a tie keeps the last note of the previous pass
    sounding. The exporter plays one extra pass to count those ties: 87 notes in 21 songs. Before this fix they
    were cut right at the loop point.
  - **Drones:** a note still held after that whole extra pass is tied on forever and gets `"len": null`. That's
    songs 66 (channels 1–2) and 75 (channels 8 and 14).
- **The player runs the driver's timing** (`tools/akao_synth.py`):
  - 243.86 Hz interrupts and the tempo accumulator.
  - Per tick: note lengths, bends and LFO delays.
  - Per interrupt: LFO steps.
  - The SPU ADSR at 44.1 kHz, worked out per stretch between interrupts.
  - **Speed and output:** numpy mixes 8–12× faster than real time. The output matches the earlier browser version
    (commit 72514b8, the one checked by ear) to within 0.01 in peak and RMS.
- **Articulation and envelopes follow the driver:** early key-off, slurs, the channel's own envelope settings, and
  `F8` overlap (the previous note rings on in its release on a second voice).
- **Instrument bank 2:** song 82 gets it (`"bank": 2`). See the sound files table.
- **Tempo:** songs start at the driver's tempo `0xFFFF` until their first `E8`.
- **Other driver features:** the layered second instrument with its balance (`F4`–`F7`), the pan LFO (`BC`/`BD`),
  LFO depth slides (`DD`/`DE`), and FM (`C6`/`C7`).
- **Reverb:** SPU reverb with the song's preset, depth and sends (see "Reverb").
- **Volume and pan:** the driver's own (see "Voice volume"): stereo mode, the master volume at boot, linear pan
  tables. Boot sets the SPU main volume to `0x3FFF` (full scale, in the common attributes at `0x8009C578`), so the
  console clips a loud mix just as the player does: 10 of 98 songs touch full scale in their first 15 s, at most
  0.031% of samples (song 92). Before this, the player used equal-power panning and half the level, so songs now
  play 2.7 dB louder at the centre and 5.8 dB louder panned hard.
- **Interpolation:** the SPU's 4-point Gaussian (psx-spx): each output sample weighs the sample before the
  position, the one at it and the two after, by one of 256 steps between samples.
  - **The weights:** psx-spx lists the hardware's 512-entry table but gives no formula, so we build our own curve
    instead of copying it. A Gaussian (σ 0.85 samples) under a (1 − (d/2)²)³ window, scaled so each step's four
    weights add up to `0x7F80` as the hardware's do. It comes within 17 of the table's 32,768ths, about −63 dB on
    white noise. A degree-14 polynomial would match it to ±1, but its coefficients would just be the table
    re-encoded.
  - **Sound:** it smooths. A signal flipping every sample comes out at 0.4, where linear interpolation passes it
    whole, so high notes and samples pitched far up sound duller than before.
  - **Cost:** 4 taps instead of 2, so rendering takes 22–30% of real time on our machine, up from 15–18%. The
    player has no dropouts.

### LFOs: vibrato, tremolo, pan

Read from the command handlers, the note path at `0x80031518` and `seq_channel_lfo` (`0x8002ED34`):
- **Commands:**
  - `B4 delay rate wave` turns vibrato on (`seq_cmd_vibrato`), `B5 d` sets its depth, `B6` turns it off but keeps
    the depth, and `DD n d` slides the depth.
  - `B8`/`B9`/`BA`/`DE` do the same for tremolo.
  - `BC rate wave` and `BD d` are the pan LFO, and `BE` (`0x80032A98`) turns it off.
  - A channel with flag `+0x54` set (by `func_8002A094`, never by songs) reads `B4`/`B8`'s first byte as the depth
    instead of the delay.
- **Timing:** each note restarts the LFOs.
  - **The commands act on the sounding note at once.** `B4`/`B8`/`BC` (`0x80032614`, `0x80032804`, `0x8003298C`)
    restart their LFO on the spot: delay, a step countdown of 1, the waveform's first step, and for vibrato the
    amount from the note's pitch (`+0x30`). `B5` (`0x80032718`) recomputes the vibrato amount at the new depth and
    `B9`/`BD` store theirs, all keeping the LFO's place. `B6`/`BA`/`BE` clear the LFO's flag and zero its offset
    (`+0xD6`/`+0xD8`/`+0xDA`).
  - **Before:** the player only started LFOs with a note, so an `on` under a tie did nothing. Effect 302 switches
    vibrato on during a held note and swaps its depth every 24 ticks. The export marks each `on` with a
    `vibrato_on`/`tremolo_on`/`pan_lfo_on` field, since only `on` restarts.
  - **Reach:** 85 songs and 61 effects have a vibrato `on` in a cell without a note, 6 songs and 48 effects a
    tremolo one, and 4 songs a pan LFO one. Only those under a held note sound different.
  - The delay counts sequence ticks (`seq_channel_slides`).
  - After the delay, the LFO reads its next waveform step every `rate` **timer interrupts** (≈ 243.86 Hz, whatever
    the tempo; 0 = 256). `seq_tick` runs the voice update (`seq_update_voices` → `seq_channel_lfo`) on every interrupt,
    before the tempo accumulator.
- **Waveforms (`lfo_waveforms`):** 16 pointers to s16 tables that loop back on the marker `0, 0, −n`.
  - 0/1 square ±; 2/3 square 0/+; 4/5 square 0/−.
  - 6/7 sine, 39 steps per cycle (a 40-step sine missing its 4th step).
  - 8/9 triangle, 16 steps; 10/11 sine, 20 steps; 12 noise; 13–15 reuse 9/8/9.
  - **12, noise:** 256 steps that repeat from the start (end marker `0, 0, −256`). It isn't a shape, so the
    exporter copies it from the user's executable into the songs that use it (`lfo_noise`). Song 76 runs it as
    vibrato on channels 1–4 (`B4 00 01 0C`, depth 60): the pitch jumps around −24…+45 cents every interrupt.
    Song 9 selects it at depth 0, so it's silent there. The XM export still leaves it out.
  - Even numbers (and 14) fade in over four cycles at ¼, ½ and ¾ scale, then repeat the full-scale one.
- **Vibrato depth:**
  - `+0x7C` = (d & 0x7F) × pitch × (1 if d's top bit is set, else 15/256) ÷ 128.
  - The pitch offset `+0xD6` = `+0x7C` × step ÷ 65,536, doubled when positive, added to the voice pitch.
  - **Usual setting:** `B4 60 01 06` + `B5 0A` (1,412 of the 1,556 `B5`s). After a half note, a 6.25 Hz sine fades
    in to about +8/−4 cents.
  - **Largest:** `B5 9F` in song 66, +3.6/−2.2 semitones at 0.25 Hz.
- **Tremolo:** volume offset `+0xD8` = voice volume × d/128 × step/32,768 (not doubled).
  - **Deeper than `0x80`** it takes the volume below zero, and the driver writes it unclamped. The SPU's volume is
    15-bit signed, so the wave plays inverted, not silent. Effect 643 slides its tremolo depth to `0xFF` on a square
    wave. We used to clamp at zero, which gated the voice on and off at about 120 Hz.
  - **Most common:** `B8 00 03 05` + `B9 78` (song 71) cuts the volume by 94% every other 3 interrupts, a 40 Hz
    buzz.
- **Pan LFO:** offset `+0xDA` = (d ÷ 2) × step/32,768 pan units.
- **Usage in the 98 songs:** vibrato in 93 (1,561 `B4`), tremolo in 11 (26 `B8`), pan LFO in 4, depth slides only
  in song 71.

### Tracker export (XM)

`python tools/xm.py <file|dir> <SOUND dir> <out>` writes FastTracker 2 modules that play the game's own samples, so
instruments sound like the game rather than General MIDI.
- **Grid:** one XM tick = 2 AKAO ticks, so the XM BPM equals the song's BPM (56–191, inside XM's 32–255).
  Patterns hold whole bars of the song's `FD` time signatures, as many as fit in two bars of 4/4, and one starts
  at the loop. Positions round to the grid (±1 AKAO tick), never lengths.
  - **Loops start on a bar line:** once every channel is looping, the song repeats from any later point, so the
    loop moves to the next bar line. That moved 26 songs, and all 26 were checked to be seamless (the moved stretch
    repeats exactly one period later).
  - **Songs without bends (83):** speed 1, with rows of 2 AKAO ticks.
  - **Songs with bends (15):** speed 2, with rows of 4 AKAO ticks. Portamento only acts on a row's later XM ticks,
    and notes on the second XM tick get note delay `ED1`.
- **Collisions:** a second note in a cell moves to the next row, and later volume or pan values in a row replace
  earlier ones. A pan change that clashes with another effect moves one row earlier.
  - **Still dropped across all 98 songs:** 14 notes (fast runs in songs that need speed 2), 43 detune steps on
    delayed notes, 13 detune steps blocked by pan, and the noise-waveform vibrato on 146 notes (song 76).
- **Instruments:** one sample per AKAO instrument, 16-bit with its loop.
  - **Tuning:** relative note + finetune = 12·log2(44,100 · pitch[C] / 0x1000 / 8,363) − 12·(6 − base − 4), where
    `base` shifts songs whose octaves exceed XM's 8.
  - **Why one value per sample is enough:** the pitch tables are equal-tempered to within 0.86 cents.
  - **Bank choice:** bank 1, except song 82, which gets bank 2 in slots 53 and up (see the sound files table).
  - **Key-offs:** placed after the channel's other events, 2 ticks before the next note as the driver does. Where a
    note already starts in that row, the key-off is skipped, since the note retriggers anyway. Where the row's
    effect column is taken, it moves to the row's start, so bends keep their cells.
  - **Not carried:** slurs (every note retriggers), the channel envelope overrides (an XM instrument has one fixed
    envelope), `F8` overlap and `F4` layers (an XM channel plays one note at a time), the pan LFO, LFO depth slides,
    and FM.
- **Events:** each channel starts from the driver's defaults (instrument 5, octave 2, volume 100, `A3` 120).
  - Notes carry the voice volume: 64 × `A8` × `A3` ÷ 127². Global master volume and the pan law are left out.
  - Slides become per-tick volume/pan values.
  - Drum-kit notes carry their entry's instrument, key, volume and pan. Every one of the 152,284 notes has a sample.
  - Transpose is added to the key.
  - Detune: whole semitones go into the note, and the rest becomes fine portamento `E1x`/`E2x` on the note's row
    (1/16 semitone steps).
  - Bends: portamento `1xx`/`2xx` (16 units per semitone) on each row's second XM tick, stepping to where the
    driver's frequency-linear slide is by the end of that tick. A bend stops at the next note.
  - The MIDI export doesn't carry bends, detune or LFOs yet.
  - **Vibrato and tremolo are baked per row.** Each row gets the average of the driver's LFO over the timer
    interrupts it covers, so the 40 Hz tremolo comes out as its mean level instead of an aliased wobble.
    - **Vibrato:** extra-fine portamento `X1x`/`X2x` (1/64 semitone), or `E1x`/`E2x` for steps over 15. It only
      uses free effect cells; a step whose cell is taken is made up on the next free row.
    - **Tremolo:** absolute values in the volume column.
    - **Why not `4xy`/`7xy`:** FT2's smallest vibrato depth is ±1/8 semitone, twice the usual setting. Its speed
      can't reach 6.25 Hz at most song tempos, and neither effect runs at speed 1.
    - **Left out:** the pan LFO, LFO depth slides (song 71), and LFO changes in the middle of a note.
- **Listening check (2026-10-28):** instruments, volume balance, drum kits, the song loop (song 7), bends
  and detune all sound right. Reverb isn't exported. Vibrato/tremolo were added after this check.
  - `AA` becomes `8xx` panning.
  - A note that doesn't run into the next one gets a key-off.
  - Tempo changes go on an extra control channel.
  - The endless loop is the XM restart position, on a pattern boundary.
- **Envelopes:** each instrument's ADSR is simulated with psx-spx's step rules at 44.1 kHz and sampled into an XM
  volume envelope, timed at the song's first BPM.
  - **Steady instruments:** attack/decay, a sustain point, then the release curve.
  - **Fading instruments:** the sustain fade sits before the sustain point, and the release becomes the fadeout
    (clamped to FT2's 0xFFF).
  - **Ignored:** phases that move less than one XM volume step in 10 seconds count as flat (sustain `0x7F` steps
    once every 23.8 s).
- **Check:** `python tools/test_xm.py assets/xm/*.xm` parses each file back and requires every byte to be
  accounted for.

Sound files (checked against our disc):

| File | Size | Layout |
|---|---|---|
| `SOUND/INSTR.ALL` | 483,232 | 16-byte header: `u32 spu_addr` (`0x1010`), `u32 length` (`0x75F90` = file size − 16), 8 zero bytes; then raw SPU ADPCM sample data uploaded to SPU RAM at `spu_addr` |
| `SOUND/INSTR.DAT` | 8,192 | 128 × 64-byte instruments, copied whole into `instrument_table` by `load_instrument_bank` (`0x800297A4`). Entry: `+0x00` `u32` SPU sample address, `+0x04` `u32` SPU loop address, `+0x08` 8 bytes of ADSR settings, `+0x10` 12 × `u32` SPU pitch for each semitone of octave 6 (other octaves shift it). 93 slots used |
| `SOUND/INSTR2.ALL` | 251,120 | Same header (`spu_addr 0x38560`, length `0x3D4E0`), second bank uploaded over the top part of bank 1's sample memory |
| `SOUND/INSTR2.DAT` | 8,192 | `load_instrument_bank2` (`0x80029818`) copies only the first 75 entries (4,800 bytes) to slot **53** onward (`instrument_table + 0xD40`). 6 used, becoming slots 53–58. `INSTR2.ALL` uploads at SPU `0x38560`, exactly where bank 1's slot 53 starts. **When:** only main-loop mode 15 loads it (`snd_load_bank2`, `0x80011274`: sectors `0x1E4` and `0x25F`), and bank 1 is loaded once at boot. From then on slots 53–127 hold only bank 2's six instruments. Song 82 (`FIELD/LASTMAP.DAT`) uses exactly those six plus low slots, while every other song that uses slots 53+ needs bank 1's instruments there. So song 82 is the bank-2 song (`BANK2_SONGS` in `tools/instruments.py`) |
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
- `0x08` `u16` reverb mode (libspu's type; see "Reverb").
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

Pipeline: `cpp` → `cc1 -O2 -G0` → `maspsx --aspsx-version=2.79` (2.34 for the sound driver) → `as -march=r3000 -G0 -O1` → `ld` at the exact address →
`objcopy -O binary`. `tools/match.sh <file.c> <addr> [gcc]` runs it and either prints `MATCH` or diffs the disassembly
against your exe.

**Proven:** `src/main/bitops.c` (3 functions, `80014A00`–`80014A84`, 132 bytes) is **byte-identical** to the game
with GCC 2.7.2. GCC 2.6.3 matches only one of the three, so that part of the exe was built with 2.7.2.

**Full rebuild:** splat 0.50.1 splits `SCUS_941.63` per `config/main.yaml`, and `make` relinks it. The result is
SHA1-identical to retail, with `bitops.c` and `sound.c` compiled from C (the latter mostly `INCLUDE_ASM` so far)
and the rest still assembly.

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
- **Preprocessor:** the period `cpp` is a 32-bit binary and fails with "Value too large for defined data type" on any
  `#include` from a Windows drive under WSL (it can't `stat` the large inode numbers). The build preprocesses with the
  host `cpp -P -undef -nostdinc` instead, which doesn't change the generated code.

**The sound driver as C** (`src/main/sound.c`, `0x800293D0`–`0x80033A90`):
- **Split:** its own `c` subsegment in `config/main.yaml`, plus a `.rodata` piece for its one jump table
  (`0x80010394`, `seq_look_ahead`'s `switch`). splat found 229 functions; unrecovered ones are `INCLUDE_ASM`.
- **Toolchain: GCC 2.6.3 `-O2 -G0`, ASPSX 2.21 (or older), divides expanded.** The `Makefile` sets all three for
  `sound.o`; for one file, `ASPSX=2.21 MASPSXFLAGS=--expand-div tools/match.sh <file.c> <addr> 2.6.3`.
  - **Divides:** every `/` carries ASPSX's divide-by-zero and overflow checks (`break 7`, `break 6`), which maspsx
    only adds with `--expand-div`.
  - **GCC 2.6.3:** `seq_cmd_loop_point` reloads the loop level after storing it, and `seq_cmd_jump` adds into a
    different register. 2.7.2 gets both wrong, and every function matched so far also matches with 2.6.3.
  - **ASPSX 2.21:** `seq_cmd_slur_on` loads 1 with `ori` (2.34 and older), and the envelope setters' indexed stores
    into `seq_channels` expand through `lui`/`addiu $at`/`addu` (before 2.30). My first guess, 2.34, came from
    the first functions alone, which had no such store.
- **`match.sh`** links against `config/symbols.main*.txt`, splat's `build/undefined_*_auto.txt` and every symbol in
  the last full build, so a function that calls another or reads a global can be checked on its own.
- **Structs** (`include/sound.h`), named from what the matched code does with them:
  - `SeqChannel`, `0x108` bytes: read pointer, loop stack (`+0x04` starts, `+0xB8` level, `+0xBA` passes), drum map
    (`+0x14`), layer voice (`+0x24`), volume, pan, octave, length fields, glide, bend, transpose and detune, LFO
    depths and offsets, the noise and FM countdowns, the envelope (`+0xEC`–`+0x102`), and the flags at `+0x38`.
  - `VoiceSettings` (`+0xDC`, `0x2C` bytes): what `snd_update_voice(voice, settings)` hands to libspu. Its `update`
    (`+0xE0` in the channel) is libspu's voice attribute mask: `0x3` the two volumes, `0x10` pitch, `0x100`–`0x400`
    the envelope modes, `0x800`–`0x8000` the rates and the sustain level. `snd_direct_voice` (`0x8007EBE4`) is a
    standalone one for voices written without a channel. The driver reaches its fields as plain variables (a
    register holds the struct's address while the stores use their own), so each has an alias.
  - **Every sequence command handler takes (channel, song, the channel's SPU voice bit).** `B0` and `CB` pass all
    three on to other handlers, and the reverb, noise and FM commands set or clear that bit in a voice mask.
  - `SeqSong`, the handlers' second argument: tempo (`+0x18`), voice masks for layers, overlaps, noise, reverb and
    FM (`+0x24`–`+0x34`), flags (`+0x38`: `0x10` noise changed, `0x80` reverb depth changed), reverb depth
    (`+0x40`), their slide counters, the noise clock (`+0x52`), and the meter and bar number (`+0x56`–`+0x5E`).
  - `seq_song_flags` (`0x8009A13C`) is `seq_song`'s `+0x38`, but the noise commands reach it as a plain variable:
    as a struct member GCC computes the address once, and the game doesn't.
  - Effect channels (`+0x54` set) use their own copies of the masks and the noise clock: `effect_noise_voices`,
    `effect_reverb_voices`, `effect_fm_voices` (`0x80099FEC`–`F4`) and `effect_noise_clock` (`0x80099FFA`).
  - `Instrument`: the 64-byte `INSTR.DAT` entry in `instrument_table`. `F9` restores the release rate from its
    `adsr[4]`, using the channel's instrument number (`+0x58`).
  - `seq_channels` (`0x80096608`): every channel, music and effects (`A0` showed it: an ending effect sets the full
    update mask on the entry its `+0x28` names, handing the voice back to that voice's music channel). A layer's
    spare voice keeps its envelope in its voice's entry, so the envelope commands write both.
  - `load_channel_instrument` copies the instrument's sample and loop addresses and all eight envelope bytes, and
    sets update bits `0x1FF80` (sample `0x80`, envelope `0xFF00`, loop `0x10000`). `A1` and `F2` rescale a sounding
    note's pitch by the new instrument's C pitch over the old one's, unless an effect holds the voice
    (`effect_voices`, `0x80099FCC`), and with `F8` on they copy everything but the release (update `0x1BB80`).
    `F2` sets the sample to `0x76FE0`, the silent blocks.
- **Recovered: 208 of the file's 232 functions are C** (205 written here; `seq_cmd_slur_off`,
  `seq_cmd_full_length_off` and the sound-command no-op `func_8002CF98` are empty in the game). The sequence
  command handlers below, plus the sound commands `A0`–`B7`. (Earlier counts in this file and in commit messages
  missed functions with upper-case hex in their names; count with a parser, not by eye.)
  Sequence handlers: octave, slur and full length, volume, pan, transpose, detune, lengths (`A2`, `DC`), glide and
  bend, loops, repeats and jumps (`C8`, `C9`, `CA`, `EE`, `F0`, `F1`, `EF`), drum kit, tempo and its slide, reverb
  depth and its slide, time signature and bar number, the tremolo and pan-LFO starts, the LFO depths, depth slides
  and off commands, volume, pan and balance slides, noise (on, off, clock, timed), FM and reverb on/off, pitch and
  volume follow, layer balance and layer off, overlap off, the envelope setters, `B0` and `B3`, `CB` (all off), and
  `F3` (`func_800335CC`, which sets the song's `+0x54`), the instrument changes `A1` and `F2`, and
  `load_channel_instrument`.
  - **Handlers still asm:** `B4` (vibrato start), `F4` and `F8` (they take a spare voice), and `A0`.
  - **Parked, close:** `B4` differs only in where one store is scheduled. `F4`/`F8` scan the voice masks for a free
    voice; every loop form tried either gets rotated or has its exit threaded straight to the return, where the
    game re-tests the bit. `A0` copies the voice bit into another register before its first branch, where the game
    keeps it in `a2`; declaration order and `register` don't change that.
  - **Statement order matters at the ends of functions:** in `A1` and `F2` the voice-update `|=` has to come after the
    last field copy for the stores to schedule as in the game.
  - **`F1`** is `F0` that also pops the loop level; **`EF`** jumps when the song's `+0x4E` (set by the game) is
    non-zero and at least n, and records n at `+0x4C`.
  - `vibrato_amount` (`+0x7C`) is a signed `short`: only then does GCC pick the game's arithmetic shift.
  - `seq_cmd_octave_down` adds an `unsigned short` −1 (the game adds `0xFFFF`).
  - `seq_cmd_glide` (`DA`) also turns slur mode on, and `DB` only clears the glide ticks.
  - `CE`/`CF` are the noise twins of `D2`/`D3`: noise on now (`CE` only) and flipped after n + 1 ticks.
  - `DF` is the pan-LFO depth slide, the twin of `DD` and `DE`.
  - Every unused command byte calls `seq_cmd_end`.
- **Sound commands `A0`–`B7`** (what the game sends a playing effect): balance, balance slide, pan, pan slide,
  pitch shift and pitch-shift slide, four commands each, one per effect pair: `seq_channels` slots 52, 50, 48 and 54
  (the pairs at `0x30`–`0x36` above, so the channel array runs to at least 56). Each handler passes its command and
  its pair to a shared worker (`snd_effect_balance` …), which sets both channels.
  - `SoundCommand` (`include/sound.h`): a 16-bit id, then 32-bit arguments from `+0x04` (`0x24` bytes, as queued).
  - `pitch_shift` (`+0x3C`, step `+0x40`, ticks `+0x5A`) is a signed byte << 8 the game sets on a playing effect.
  - A slide's length 0 means 1 tick. `snd_effect_pitch_shift_slide` is parked: its tick count is copied to another
    register before the final stores, and no form tried reproduces that.
- **Other sound commands in C:** `B8`–`BD` apply those six controls to all four effect pairs (from slot 54 back,
  two channels a step). `D0`/`D1` and `D4`/`D5` set or slide a value at `0x80062FE8` / `snd_pitch_scale` (a signed byte
  << 16; meaning not known yet), `C8` sets `0x80062FD4` and calls `func_8002E428`, `80`/`81`/`82` store 1/2/4 in
  `seq_song`'s first word and call `func_8002A748` and `func_8002A798`, `90` stores its argument and sets every
  music channel's volume-update bits, `92` sets the song's branch level (what `EF` tests), `E0` sets `0x80062F70`
  and the reverb-changed flag, `F2`/`F3` clear one word each, `F0`/`F1`/`FA` just call a function, and `F8`/`F9`
  take the stream's voices away from the effects with their reverb off or on.
  - Names follow the command byte (`snd_cmd_D0`) until what they control is known.
  - `F8`/`F9` reach `effect_voices` through a pointer: only then does GCC keep its address in a register as the
    game does.
  - `C9`/`CA` slide `C8`'s value (to, or from–to), `C0`/`C1`/`C2` set or slide `snd_master_volume`, `D2`/`D6` are `D0`'s and
    `D4`'s from–to slides, and `E4` sets a byte and flag `0x10` at `0x80062FF8` with it.
- **Playing effects (sound commands `20`–`34`):** look the id up with `func_8002A6C4` (its two streams), then start it
  with `func_8002A094(pan, pair, first, second)` after `snd_stop_effect_pairs` on the pair. `20`/`28` play on pair `0x34`,
  `29` on `0x32`, `2A` on `0x30`, `2B` on `0x36`; `21`, `22` and `23` play two, three or four effects together from
  `0x30` (or `0x32`) up; `30` hands its effect to `func_8002A28C`; `34` plays two streams the caller gives, centred.
- **Songs (sound commands `10`, `14`, `15`, `18`, `19`):** `seq_song`'s `+0x4A` is the playing song's id (0 = none).
  - **`10` a b id** loads with `func_80029B78`, then resumes a saved song with that id (either slot,
    `snd_resume_song`) or starts it (`snd_start_song`); song 14 is saved first when it's playing.
  - **`14` a b id** saves whatever is playing (`snd_save_song`) before starting the new one. The game computes
    `&seq_song` once before the branch, so the C takes it into a local there.
  - **`15` a b id** is `10` that swaps: the playing song goes through the second block into a save slot while
    the saved one resumes (or, with no saved match, it's saved and the new one starts).
  - **`18`/`19`** are `10`/`14` that cross-fade: while a song plays, `snd_fade_out_song` first moves it to the
    second block and fades it out. Arg 3 (0 = 16) is `snd_fade_ticks` (`0x80062FC8`).
    - **The move:** channels 0–23 go to 24–47 and `seq_song` to `seq_song2`. The song data goes 12 KB up, with
      every read, loop and drum-map pointer moved by `0x3000` and the layer voices renumbered from 24.
    - **The fade:** `snd_channel_fades[24]` (`0x8009C5A0`, 12 bytes each) takes channel n from `0x7F8000` to 0
      over (24 − n) × `snd_fade_ticks` ticks.
  - **Two save slots:** `saved_song_channels[2][24]` (`0x8007EC10`) and `saved_songs[2]` (`0x80083334`); slot 1
    holds song 14 and slot 0 any other. `F2`/`F3` empty them. Their stride confirms `SeqSong` is `0x60` bytes.
- **CD audio volume:** sound commands `C8`/`C9`/`CA` set, slide and slide from–to `snd_cd_volume` (`0x80062FD4`,
  << 16). `snd_apply_cd_volume` writes its top half to both sides of a libspu `SpuCommonAttr` (mask `0x1C0`: CD
  volume left/right and CD reverb, here 0) and hands it to `0x80039034` *(probably `SpuSetCommonAttr`)*.
- **Effect voices:** `snd_refresh_effect_volumes` walks `effect_voices` from bit `0x10000` against `seq_channels`
  slot 48, so **effect slots 48–55 are SPU voices 16–23**. Its twin walks the song's `active_voices` from slot 0.
- **Helpers in C:** `snd_load_song` (copies the song into `song_data`, `0x80083580`), `snd_save_song` (copies 24
  channels and the `SeqSong` into a save slot), `snd_queue_next` (the next free `SoundCommand` in `snd_queue`).
- **More helpers in C:**
  - `snd_effect_streams`: an effect's two streams, `effect_data` + the 16-bit offsets 2·id and 2·id + 1 in
    `effect_offsets` (`0xFFFF` = none).
  - `snd_set_reverb_mode`: on a change it stores the mode in `seq_song`'s `+0x3C`, turns reverb off, sets the
    mode with its work area cleared (`0x100`) through libspu's `SpuReverbAttr`, and turns it back on.
  - `channel_reset`: the channel defaults (instrument 5, octave 2, volume 100, multiplier 120) and some twenty
    fields zeroed.
  - `snd_flush_queue`: runs each queued command through `snd_command_handlers`, unless `0x80062F8C` holds it.
  - `snd_stop_song`: every voice the song holds (layers and overlaps too) gets a 2-tick key-off and steps into an
    end stub at `0x80049C40`; the song's voice masks are cleared.
  - **The sample stream** (`snd_stream_*`): the game streams sample data from main RAM through a double buffer at
    SPU `0x77000`–`0x78FFF`, on voice 16 (mono) or voices 16/17 (stereo, 2 KB each per half).
    - **Setup:** `snd_stream_voice_mono` / `snd_stream_voices_stereo` set the voices up through `snd_direct_voice`.
      `snd_stream_play_mono` / `_stereo` arm the first IRQ and key the voices on.
    - **Refill:** each half's IRQ (`snd_stream_fill_*_a` / `_b`) copies the next 4 KB into the other half with
      `SpuWrite` and re-arms the IRQ.
    - **At the end:** it starts again from `snd_stream_loop` (`snd_stream_loop_size` bytes), or `snd_stream_stop`
      (also `FA`) keys the voices off.
  - `seq_song`'s voice masks (`+0x04`, `+0x24`, `+0x28`) are also reached as plain globals here, like its flags.
  - **Parked:** `func_80031A70` (skips commands to a length-0 one, returns `CA` or `A0`): the game loads `0xCA`
    into a register before the loop.
- **Pause and resume (sound commands `9A`–`9D`):**
  - **`9B` pauses the music:** its voices (not the effects' or the stream's) get volume 0 and a held sustain
    through `snd_direct_voice`. Its `active_voices` move to `paused_voices` (`+0x14`) and `0x80062FF8` bit 0 is set.
  - **`9A` resumes it:** the voices rewrite volume and sustain, the masks move back, and the noise, reverb and FM
    voices go to the SPU again.
  - **`9D`/`9C`** do the same for the effects (bit 1), but a kind 2 effect on channels 54/55 (`+0x54` = 2) keeps
    playing.
  - **The effects' block:** `effect_voices` (`0x80099FCC`) and the words after it follow `SeqSong`'s first fields:
    `effect_key_on_voices`, `effect_update_voices`, `effect_key_off_voices`, `effect_paused_voices`.
- **`F1`, `F4`, `F5`:**
  - **`F1`** (`snd_stop_effects`) ends every effect but a kind 2 one.
  - **`F4`** sets the effects aside: their 8 channels into `seq_channels[24..31]` and 12 words of their block after
    them. Then it stops them and runs `B8` (0x7F) and `BC` (0) on every effect pair.
  - **`F5`** copies them back and keys off the voices they no longer hold.
- **Voice masks to the SPU:**
  - `snd_apply_noise_voices`, `snd_apply_reverb_voices` and `snd_apply_fm_voices` each build one mask from both
    song blocks and the effects' copy, and set it on and its complement off.
  - Before that, `snd_add_spare_voices` adds each channel's spare voice (sequence commands `F4` layer, `F8`
    overlap). `E4`'s flag puts every voice in
    reverb.
  - `snd_key_off_voices` does the same with the `key_off_voices` masks (`+0x10`).
  - **A second song block** (`seq_song2`, `0x8009A164`, right after `seq_song`) owns channels 24–47, its voices
    limited by `0x80062F68`: the song fading out under `18`/`19`, and a waystation for `15` and `F4`.
  - The masks are written as one expression with the "free voices" mask repeated as the last argument: computed
    first into a local, it schedules differently.
- **More recovered this round:**
  - **Stopping:** `snd_stop_songs` (`F0`) runs `snd_stop_song`'s work on both song blocks. `snd_stop_effect_pairs`
    stops one to four effect pairs.
  - **Keeping what still sounds:** `snd_keep_sounding_voices` runs before a song is saved and keeps the voices
    still keyed on, or whose `F8` overlap voice is. `snd_keep_looping_effects` runs before `F4`: an effect whose
    stream ends in `CA` stays.
  - **The clock:** `snd_timer_tick` runs the sequencer ticks due since the last call (scanlines ÷ 66, 1–8, doubled
    by `0x80062FF8` bit 2). It keeps how long the last four calls took in `snd_tick_time[4]` and their sum in
    `snd_tick_time_total`.
  - **Parked:** `func_8002A094` (effect start) differs only in which saved registers two values get.
    `func_8002A28C` (the kind 2 effect's start) shares field addresses differently whichever way it's written.
- **Voice output, song start and resume, the stream start (this round):**
  - **`snd_update_voice(voice, settings)`** hands each group whose update bits are set to libspu: pitch, volume,
    start, loop, then the envelope pairs (sustain, attack, release, decay with sustain level). It clears each group's
    bits and returns once none are left. `VoiceSettings`' first word is the voice number.
  - **`seq_layer_voice_update`** splits a layered channel's volume by its balance (main voice (127 − n) / 256,
    layer n / 256), copies the pitch to the layer, and writes both voices.
  - **`snd_start_song`** reads `song_data`: a word with the channel mask, then a 16-bit offset per channel (from
    after the offset). Every channel starts on instrument 20 at volume `0x3FFF0000`, centred, and the song state
    starts over (tempo `0xFFFF`, `tempo_phase` 1).
  - **`snd_resume_song`** copies a save slot back field by field (the beat's tick count 2 back), then the channels.
    Channels the song doesn't hold step into the end stub, and its voices rewrite everything.
    - **Fade-in:** unless a slide or the cross-fade is running, `snd_master_volume` (set by `C0`–`C2`) restarts at 10 and
      slides back over 60 ticks.
    - **Matching:** the copies must target `seq_song`'s fields, not the plain aliases. GCC 2.x treats a struct
      field reached through an index as unable to alias a plain scalar, so with the aliases it reorders the copies.
  - **`snd_stream_start`** (called by `F8`/`F9`) reads the stream header: total size, flags (bit 0 stereo, bit 1
    loops; the top half is `snd_stream_pitch`), loop offset and a spare word. It primes 8 KB and stops the `0x30`
    effect pair.
- **Per-tick channel work (this round):**
  - **`seq_channel_slides` / `seq_effect_slides`** step a music / effect channel's slides once a tick: volume,
    balance and pan (music only), the vibrato and tremolo delays (music only), the timed noise and FM flips (on the
    song's masks or the effects' own), the three LFO depth slides with their output recomputed, and a slide on
    `+0x34`.
    - **`+0x34`** (step `+0x4C`, ticks `+0x64`) is a pitch offset: its top half adds to the pitch register.
  - **`seq_channel_lfo`** is the voice update.
    - **LFOs:** each one that's on and past its delay steps through its waveform.
    - **Volume:** A8 × A3 (or the previous channel's pitch, `D6`), plus tremolo, × `snd_master_volume` (`C0`), × the
      cross-fade, split by pan (plus pan LFO) through `snd_pan_left` / `snd_pan_right`.
    - **Pitch:** the note's (or the previous channel's, `D4` mode) plus vibrato and `+0x34`.
    - **`D4`–`D6`'s value is a global pitch scale (`snd_pitch_scale`):** its top byte, read unsigned, makes
      pitch × (128 + n) / 128 below 128 and pitch × n / 256 from 128 up, so negative values lower the pitch.
  - **Matching:**
    - The noise flip in `seq_channel_slides` goes through a pointer. GCC 2.x assumes a struct field reached through
      a pointer can't alias a plain scalar, so without the pointer it moves the flags load above the store.
    - `seq_channel_lfo` reads the master volume's top half through its own `unsigned short` symbol.
- **The output pass and the effects' voice update (this round):**
  - **`seq_effect_lfo`** is `seq_channel_lfo` for effect channels.
    - **Left out:** the LFO delays, the master volume and the cross-fade.
    - **Added:** the effect's balance (`A0` commands) scales its volume and pan places it. Its pitch shift (byte
      `+0x3D`) scales its pitch the way `snd_pitch_scale` does for music.
    - **The kind 2 effect** skips both and plays centred.
  - **`seq_update_voices`** is the per-tick output pass.
    - **Global settings:** a changed reverb depth goes to the SPU, scaled by `E4`'s `snd_reverb_scale` and split
      left/right by `E0`'s `snd_reverb_balance`. A changed noise clock goes too.
    - **Voices:** for both song blocks and the effects, each voice due an update runs its LFO update and writes
      what changed.
      - **Layer channels** write through `seq_layer_voice_update`.
      - **Overlap channels** write to whichever voice their turn is on: flag `0x400` flips on each key-on, and in
        the music block a busy overlap voice falls back to the channel's own.
    - **Key-on:** the voices due a key-on are keyed on together.
  - **Names this pinned down:**
    - `SeqSong`'s `+0x08` is the key-on mask (`key_on_voices`; `seq_song_key_on_voices`, `effect_key_on_voices`).
    - Sound command `90`'s value is a mute mask (`snd_muted_voices`): those voices get volume 0.
    - The meter counters were swapped. `+0x58` is the bar's current beat (`bar_beat`) and `+0x5C` counts ticks into
      the beat (`beat_tick`).
- **Parked, the tick (this round):**
  - **`seq_tick`:**
    - **What it does:** it adds each block's tempo, scaled by `D0`–`D2`'s value (a global tempo scale, byte
      `0x80062FEA`), to the accumulator. On a carry, every channel's gate counts down: the key-off at the high
      byte, the next event (`seq_step_channel`) at the low one.
    - **Effects:** they run only while not paused, except kind 2, and each counts its ticks in `+0x50`.
    - **Afterwards:** a pause queued by `F3` (`+0x54`) runs as `9B`, then the queue flushes, the global slides run
      and voices key off.
    - **Why it's parked:** the C is written, but the game reaches most of `seq_song`'s tempo, reverb and meter
      fields as plain variables, so it needs more aliases.
  - **`func_80030380`:**
    - **What it does:** every 4th tick it runs the global slides: CD volume, the master volume (a fade-out to 0
      stops the songs), the tempo and pitch scales (a pitch-scale step rewrites every music pitch), and each effect's
      balance, pan and pitch-shift slides. A balance fade to 0 ends the effect.
    - **The cross-fade:** it steps the per-channel fades. When channel n's fade ends, the old song's channel ends,
      the new song's channel rewrites everything, and the fade restarts as a fade-in.
    - **Why it's parked:** everything matches but the load order in the pan and pitch-shift slides.
  - **The two song blocks were one array in the original.** `func_80030380` reaches `seq_song`'s fields as offsets
    from `seq_song2`'s address, which GCC only does within one symbol. A `SeqSong songs[2]` would model that.
- **A second file at the start of the range:** `0x800293D0`–`0x80029A4C` (11 functions, including `snd_init` and
  the instrument-bank loaders) reach small globals through `$gp`, so that part was compiled with `-G8`; from
  `0x80029A50` on, small globals like `0x80062F00` are reached with `lui`, which is `-G0`.
  - **Why they stay asm for now:** maspsx only emits `$gp` accesses for small variables the same C file defines,
    and defining them would add storage before the data section is split per file. They need that split first.

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
