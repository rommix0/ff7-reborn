# FF7 song and instrument bank formats

Our own tracker format for the PS1 game's music, written by `tools/ff7song.py` and played and edited by `tools/ff7player.py`.
It keeps what XM couldn't: bars of any time signature, events at exact AKAO ticks, and the driver's own effect
parameters (the player emulates the driver rather than translating effects). Songs hold no samples; they name
instrument slots in one shared bank file.

`python tools/ff7song.py <file|assets dir> <SOUND dir> <out dir>` writes `instruments.ff7bank` and one
`song_NNN.ff7song` per song. The reverb presets are read from `SCUS_941.63` next to the `SOUND` folder.

## Instrument bank (`instruments.ff7bank`, binary, little-endian)

| Offset | Type | Field |
|---|---|---|
| 0 | `char[8]` | magic `FF7BANK\0` |
| 8 | `u16` | version, 1 |
| 10 | `u16` | instrument count `n` |
| 12 | `n` × 68 bytes | entries, below |
| … | `s16[]` | each entry's samples, in entry order, back to back |

Entry (68 bytes):

| Offset | Type | Field |
|---|---|---|
| 0 | `u16` | slot the songs refer to (0–127; bank 2 fills 53–127 where bank 1 doesn't) |
| 2 | `u16` | flags: bit 0 = the sample loops; bit 1 = it belongs to the second bank (`INSTR2`, slots 53–58) |
| 4 | `u32` | length in samples |
| 8 | `u32` | loop start in samples (0 when it doesn't loop) |
| 12 | `u32[12]` | SPU pitch of C … B in octave 6 (`0x1000` = 44,100 Hz); octave `o` is scaled by 2^(o − 6) |
| 60 | `u8[8]` | SPU ADSR: attack rate, decay rate, sustain level, sustain rate, release rate, then the attack, sustain and release modes (1 linear up, 3 linear down, 5 exponential up, 7 exponential down) |

Samples are the instrument's SPU ADPCM decoded to 16-bit PCM, played at the pitch above.

## Song (`song_NNN.ff7song`, JSON)

```json
{
  "format": "ff7song", "version": 1, "song": 76, "bank": 1, "channels": 13,
  "reverb": {"mode": 1, "start": 64296, "registers": ["…32 numbers from the executable…"]},
  "ticks_per_quarter": 48, "timer_hz": 243.856,
  "loop": {"start": 144, "end": 3504},
  "patterns": [{"ticks": 144, "beat": 24, "beats": 6, "cells": [[0, 0, {"note": 62, "len": 72, "inst": 34}]]}],
  "order": [0, 1, 2, 1],
  "restart": 1
}
```

- **Timing:** the driver's timer runs at `timer_hz`. Each interrupt adds the tempo to a 16-bit accumulator, and
  every carry is one tick. BPM = `timer_hz` × tempo ÷ 65,536 × 60 ÷ `ticks_per_quarter`.
- **Patterns are bars.** `beats` beats of `beat` ticks each (the song's `FD` time signature). `ticks` is the bar's
  real length, which is shorter when the next time signature or the end of the song cuts it. Songs without a time
  signature count 4/4 bars from the loop start.
- **`order`** lists the bars in time order; identical bars share a pattern. `restart` is the order index the song
  jumps back to when it reaches `loop.end`. `loop` is `null` for songs that end.
- **`bank`:** 1 for the instruments loaded at boot. With 2 (song 82), the second bank replaces slots 53 and up, and
  only its own instruments are left there, as when the game loads `INSTR2`.
- **`reverb`:** the SPU reverb the song sets up (the header's mode), or `null` for none. `mode` is libspu's
  reverb type (1 Room, 3 Studio B, 4 Studio C in the songs). `start` is the work area's start in SPU RAM in 8-byte
  units; the area runs from there to the end of the 512 KB, and its size sets the longest echo. `registers` are the
  32 reverb registers `0x1F801DC0`–`0x1F801DFE` in order (`dAPF1`, `dAPF2`, `vIIR`, `vCOMB1`–`4`, `vWALL`,
  `vAPF1`, `vAPF2`, `mLSAME`, `mRSAME`, `mLCOMB1`, `mRCOMB1`, `mLCOMB2`, `mRCOMB2`, `dLSAME`, `dRSAME`, `mLDIFF`,
  `mRDIFF`, `mLCOMB3`, `mRCOMB3`, `mLCOMB4`, `mRCOMB4`, `dLDIFF`, `dRDIFF`, `mLAPF1`, `mRAPF1`, `mLAPF2`,
  `mRAPF2`, `vLIN`, `vRIN`), as psx-spx names them. The player runs psx-spx's reverb formula on them at 22.05 kHz.
- **`lfo_noise`:** only in songs whose LFOs use waveform 12. It holds the driver's noise waveform: 256 signed 16-bit
  steps, repeated from the start, read from the executable.
- **Cells** are `[tick within the bar, channel, fields]`. Each channel is one voice. All fields are optional:

| Field | Meaning |
|---|---|
| `note`, `len` | Key on: key 0–127 (60 = C of octave 4, the driver's octave + 1 × 12 + semitone, transpose included), held `len` ticks until key-off (ties included). The driver keys a note off 2 ticks before the next event unless a tie follows or slur/full-length mode is on, and `len` already counts that. A new note on the channel cuts the old one. `len` may run past the bar and past `loop.end` (a loop that opens with a tie holds the note from the end of the previous pass); `null` holds it until the channel's next note (a drone the loop ties on forever) |
| `slur` | `true`: this note takes over the sounding voice (new pitch and length) instead of restarting the sample and envelope (`CC` slur mode) |
| `inst` | Bank slot for the following notes; also resets the channel's envelope to the instrument's |
| `overlap` | `F8`: a number turns on two voices taking turns, so each note rings on in its release while the next starts, with that release rate, kept across `inst` changes. `null` (`F9`) turns it off and the release rate is the instrument's again |
| `layer` | `[a, b]` (`F4`): instrument `a` as usual, and instrument `b` on a second voice playing the same notes at the main voice's pitch. `null` (`F5`) ends it |
| `balance`, `balance_slide` | Layer balance 0–127 (`F6`): main voice at (127 − n)/256, layer at n/256. `[ticks, n]` (`F7`) slides it there |
| `pan_lfo` | `[delay, rate, waveform, depth]` like `vibrato` (`BC`/`BD`): the pan moves by (depth >> 1) × step >> 15 |
| `vibrato_slide`, `tremolo_slide` | `[ticks, depth]` (`DD`/`DE`): slide the LFO depth there, one step per tick |
| `fm` | `true` (`C6`): the voice's pitch follows the previous channel's output (SPU pitch modulation); `false` (`C7`) ends it |
| `reverb` | `true` (`C2`): the channel's voices feed the reverb; `false` (`C3`) stops it. Off at song start |
| `reverb_depth` | `EA`: the reverb's output volume for the whole song, signed 16-bit, `0x8000` = −1 (most songs use `0x2800`). 0 at song start |
| `ar`, `dr`, `sl`, `sr`, `rr` | The channel's own attack rate, decay rate, sustain level, sustain rate, release rate (`AD`, `AE`, `AF`, `B1`, `B2`; the same values as the bank's ADSR bytes), until the next `inst`. They reach a voice that is already sounding |
| `volume`, `expression` | `A8` volume and `A3` multiplier, 0–127; the voice plays at volume × expression ÷ 127² of full scale. Slides arrive as one value per tick |
| `pan` | 0–127, 64 = centre |
| `tempo` | New tempo for the whole song |
| `detune` | Scales the pitch by 1 + d/128 (d > 0) or 1 + d/256 (d < 0) |
| `bend` | `[ticks, semitones]`: from this tick, slide linearly in frequency to `semitones` above the held note |
| `vibrato`, `tremolo` | `[delay, rate, waveform, depth]` from now on; depth 0 = off. Each note restarts them: after `delay` ticks the LFO takes its next waveform step every `rate` timer interrupts |

LFO details (waveform shapes, depth scaling) are in `knowledge_base.md`, "LFOs: vibrato, tremolo, pan".

## Not carried yet

The SPU's own interpolation and pan law (the player uses linear interpolation and equal-power panning).
