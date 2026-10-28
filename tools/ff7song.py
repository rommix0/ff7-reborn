"""Export AKAO songs in our own tracker format (docs/ff7song_format.md): one instrument bank, sample-free songs.

usage: python tools/ff7song.py <file|assets dir> <SOUND dir> <out dir>

The reverb presets and the noise LFO come from the executable next to the SOUND folder (SCUS_941.63).
"""
import bisect
import json
import struct
import sys
from pathlib import Path

import akao
from instruments import BANK2_SONGS
from xm import load_bank_instruments

EXE_BASE = 0x80010000 - 0x800  # load address minus the PS-X EXE header
# libspu's tables in SCUS_941.63, indexed by reverb mode (0 off, 1 Room ... 9 Pipe).
REVERB_WORK_STARTS = 0x8004AB5C  # u32 each: the work area's start in SPU RAM, in 8-byte units
REVERB_PRESETS = 0x8004ABAC  # 0x44 bytes each: u32 mask, then the 32 reverb registers (0x1F801DC0..0x1F801DFE)
LFO_WAVEFORMS = 0x8004A5CC  # 16 pointers to the driver's s16 LFO waveforms, each ending 0, 0, -(steps to repeat)
NOISE_WAVEFORM = 12  # the one waveform that isn't a shape we can rebuild
DEFAULT_METER = (48, 4)  # 4/4 until the first time signature
# Event kind -> cell field; the rest keep their name.
FIELD = {"program": "inst"}


def write_bank(bank: dict, path: Path, bank2: dict | None = None):
    """Bank file: header, one 68-byte entry per instrument, then every instrument's 16-bit samples. Flag bit 0:
    the sample loops; bit 1: the instrument belongs to the second bank (INSTR2)."""
    entries, samples = b"", b""
    sets = [(slot, v, 0) for slot, v in sorted(bank.items())] + [(slot, v, 2) for slot, v in sorted((bank2 or {}).items())]
    for slot, (pcm, loop, pitches, adsr), flag in sets:
        entries += struct.pack("<HHII12I8s", slot, flag | (loop is not None), len(pcm), loop or 0, *pitches, adsr)
        samples += struct.pack(f"<{len(pcm)}h", *pcm)
    path.write_bytes(b"FF7BANK\0" + struct.pack("<HH", 1, len(sets)) + entries + samples)


def reverb_presets(exe: bytes) -> list[dict]:
    """The SPU reverb settings for each libspu reverb mode, read from the game's executable."""
    starts = struct.unpack_from("<10I", exe, REVERB_WORK_STARTS - EXE_BASE)
    return [{"mode": mode, "start": starts[mode],
             "registers": list(struct.unpack_from("<32H", exe, REVERB_PRESETS - EXE_BASE + mode * 0x44 + 4))}
            for mode in range(10)]


def lfo_noise(exe: bytes) -> list[int]:
    """The driver's noise LFO waveform: 256 steps that repeat from the start."""
    at = struct.unpack_from("<I", exe, LFO_WAVEFORMS - EXE_BASE + NOISE_WAVEFORM * 4)[0] - EXE_BASE
    steps = []
    while steps[-2:] != [0, 0] or struct.unpack_from("<h", exe, at)[0] >= 0:
        steps.append(struct.unpack_from("<h", exe, at)[0])
        at += 2
    if -struct.unpack_from("<h", exe, at)[0] != len(steps) - 2:
        raise ValueError("the noise LFO doesn't repeat from its start")
    return steps[:-2]


def song_to_ff7song(block: bytes, presets: list | None = None, noise: list | None = None) -> dict:
    channels, loop_start, end, _, _ = akao.play_song(block, trim_notes=False)
    meters = sorted((e[0], e[2], e[3]) for events in channels for e in events if e[1] == "meter")
    bar_meters = [(tick, beat * beats) for tick, beat, beats in meters]
    bars = akao.bar_lines(bar_meters or [(loop_start or 0, DEFAULT_METER[0] * DEFAULT_METER[1])], end) or [0]
    cells = {bar: [] for bar in bars}
    for ch, events in enumerate(channels):
        merged = {}  # tick -> fields; a later event on the same tick wins, as in the driver
        for tick, kind, *args in events:
            if kind == "meter":
                continue
            value = True if kind == "slur" else args[0] if len(args) == 1 else args
            merged.setdefault(tick, {}).update({"note": args[0], "len": args[1]} if kind == "note"
                                               else {FIELD.get(kind, kind): value})
        for tick, fields in merged.items():
            bar = bars[bisect.bisect_right(bars, tick) - 1]
            cells[bar].append([tick - bar, ch, fields])
    patterns, order, seen = [], [], {}
    for bar, nxt in zip(bars, bars[1:] + [end]):
        beat, beats = next((m[1:] for m in reversed(meters) if m[0] <= bar), DEFAULT_METER)
        pattern = {"ticks": nxt - bar, "beat": beat, "beats": beats, "cells": sorted(cells[bar], key=lambda c: c[:2])}
        key = json.dumps(pattern, sort_keys=True)
        if key not in seen:
            seen[key] = len(patterns)
            patterns.append(pattern)
        order.append(seen[key])
    song_id, reverb_mode = struct.unpack_from("<H", block, 4)[0], struct.unpack_from("<H", block, 8)[0]
    uses_noise = any(e[1] in ("vibrato", "tremolo", "pan_lfo") and e[4] == NOISE_WAVEFORM
                     for events in channels for e in events)
    return {"format": "ff7song", "version": 1, "song": song_id, "bank": 2 if song_id in BANK2_SONGS else 1,
            "reverb": presets[reverb_mode] if presets and reverb_mode else None,
            **({"lfo_noise": noise} if noise and uses_noise else {}),
            "channels": len(channels), "ticks_per_quarter": akao.TICKS_PER_QUARTER, "timer_hz": akao.DRIVER_HZ,
            "loop": {"start": loop_start, "end": end} if loop_start is not None else None,
            "patterns": patterns, "order": order, "restart": bars.index(loop_start) if loop_start is not None else 0}


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return
    source, sound_dir, out = map(Path, sys.argv[1:])
    out.mkdir(parents=True, exist_ok=True)
    bank2 = {slot: v for slot, v in load_bank_instruments(sound_dir, bank2=True).items() if slot >= 53}
    write_bank(load_bank_instruments(sound_dir), out / "instruments.ff7bank", bank2)
    exe = (sound_dir.parent / "SCUS_941.63").read_bytes()
    presets, noise = reverb_presets(exe), lfo_noise(exe)
    songs = akao.named_songs(source)
    for name, _, block in songs:
        song = song_to_ff7song(block, presets, noise)
        (out / f"{name}.ff7song").write_text(json.dumps(song, separators=(",", ":")), encoding="utf-8")
    print(f"{len(songs)} songs and instruments.ff7bank -> {out}")


if __name__ == "__main__":
    main()
