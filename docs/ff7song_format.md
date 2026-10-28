# FF7 song and instrument bank formats

Our own tracker format for the PS1 game's music, written by `tools/ff7song.py` and played and edited by `tools/ff7player.py`.
It keeps what XM couldn't: bars of any time signature, events at exact AKAO ticks, and the driver's own effect
parameters (the player emulates the driver rather than translating effects). Songs hold no samples; they name
instrument slots in one shared bank file.

`python tools/ff7song.py <file|assets dir> <SOUND dir> <out dir>` writes `instruments.ff7bank` and one
`song_NNN.ff7song` per song.

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
- **Cells** are `[tick within the bar, channel, fields]`. Each channel is one voice. All fields are optional:

| Field | Meaning |
|---|---|
| `note`, `len` | Key on: key 0–127 (60 = C of octave 4, the driver's octave + 1 × 12 + semitone, transpose included), held `len` ticks until key-off (ties included). The driver keys a note off 2 ticks before the next event unless a tie follows or slur/full-length mode is on, and `len` already counts that. A new note on the channel cuts the old one. `len` may run past the bar and past `loop.end` (a loop that opens with a tie holds the note from the end of the previous pass); `null` holds it until the channel's next note (a drone the loop ties on forever) |
| `slur` | `true`: this note takes over the sounding voice (new pitch and length) instead of restarting the sample and envelope (`CC` slur mode) |
| `inst` | Bank slot for the following notes; also resets the channel's envelope to the instrument's |
| `overlap` | `F8`: a number turns on two voices taking turns, so each note rings on in its release while the next starts, with that release rate, kept across `inst` changes. `null` (`F9`) turns it off and the release rate is the instrument's again |
| `ar`, `dr`, `sl`, `sr`, `rr` | The channel's own attack rate, decay rate, sustain level, sustain rate, release rate (`AD`, `AE`, `AF`, `B1`, `B2`; the same values as the bank's ADSR bytes), until the next `inst`. They reach a voice that is already sounding |
| `volume`, `expression` | `A8` volume and `A3` multiplier, 0–127; the voice plays at volume × expression ÷ 127² of full scale. Slides arrive as one value per tick |
| `pan` | 0–127, 64 = centre |
| `tempo` | New tempo for the whole song |
| `detune` | Scales the pitch by 1 + d/128 (d > 0) or 1 + d/256 (d < 0) |
| `bend` | `[ticks, semitones]`: from this tick, slide linearly in frequency to `semitones` above the held note |
| `vibrato`, `tremolo` | `[delay, rate, waveform, depth]` from now on; depth 0 = off. Each note restarts them: after `delay` ticks the LFO takes its next waveform step every `rate` timer interrupts |

LFO details (waveform shapes, depth scaling) are in `knowledge_base.md`, "LFOs: vibrato, tremolo, pan".

## Not carried yet

Reverb, the pan LFO, LFO depth slides, and the SPU's own interpolation and pan law (the player uses linear
interpolation and equal-power panning).
