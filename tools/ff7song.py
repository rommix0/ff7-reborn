"""Export AKAO songs in our own tracker format (docs/ff7song_format.md): one instrument bank, sample-free songs.

usage: python tools/ff7song.py <file|assets dir> <SOUND dir> <out dir>
"""
import bisect
import json
import struct
import sys
from pathlib import Path

import akao
from instruments import BANK2_SONGS
from xm import load_bank_instruments

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


def song_to_ff7song(block: bytes) -> dict:
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
    song_id = struct.unpack_from("<H", block, 4)[0]
    return {"format": "ff7song", "version": 1, "song": song_id, "bank": 2 if song_id in BANK2_SONGS else 1,
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
    songs = akao.named_songs(source)
    for name, _, block in songs:
        (out / f"{name}.ff7song").write_text(json.dumps(song_to_ff7song(block), separators=(",", ":")),
                                             encoding="utf-8")
    print(f"{len(songs)} songs and instruments.ff7bank -> {out}")


if __name__ == "__main__":
    main()
