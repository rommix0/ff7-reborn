# FF7 song and instrument bank formats

Our own tracker format for the PS1 game's music, written by `tools/ff7song.py` and played by `player/index.html`.
It keeps what XM couldn't: bars of any time signature, events at exact AKAO ticks, and the driver's own effect
parameters (the player emulates the driver rather than translating effects). Songs hold no samples; they name
instrument slots in one shared bank file.

`python tools/ff7song.py <file|assets dir> <SOUND dir> <out dir>` writes `instruments.ff7bank`, one
`song_NNN.ff7song` per song and an `index.json` listing the songs.

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
| 2 | `u16` | flags: bit 0 = the sample loops |
| 4 | `u32` | length in samples |
| 8 | `u32` | loop start in samples (0 when it doesn't loop) |
| 12 | `u32[12]` | SPU pitch of C … B in octave 6 (`0x1000` = 44,100 Hz); octave `o` is scaled by 2^(o − 6) |
| 60 | `u8[8]` | SPU ADSR: attack rate, decay rate, sustain level, sustain rate, release rate, then the attack, sustain and release modes (1 linear up, 3 linear down, 5 exponential up, 7 exponential down) |

Samples are the instrument's SPU ADPCM decoded to 16-bit PCM, played at the pitch above.

## Song (`song_NNN.ff7song`, JSON)

```json
{
  "format": "ff7song", "version": 1, "song": 76, "channels": 13,
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
- **Cells** are `[tick within the bar, channel, fields]`. Each channel is one voice. All fields are optional:

| Field | Meaning |
|---|---|
| `note`, `len` | Key on: key 0–127 (60 = C of octave 4, the driver's octave + 1 × 12 + semitone, transpose included), held `len` ticks (ties included). A new note on the channel cuts the old one. `len` may run past the bar and past `loop.end` |
| `inst` | Bank slot for the following notes |
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
