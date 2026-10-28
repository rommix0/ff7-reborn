import math

import numpy as np

from akao_synth import Instrument, Synth, timeline

RATE = 44100
TIMER_HZ = 33_868_800 / 8 / 0x43D1
STEADY = bytes([0x00, 0x0F, 0x0F, 0x7F, 0x06, 1, 3, 3])  # instant attack, sustain held, quick release
TONE = np.round(np.sin(2 * np.pi * np.arange(64) / 64) * 20000).astype(np.float32) / 32768
BANK = {0: Instrument(TONE, True, 0, (0x1000,) * 12, STEADY)}


def song(cells, ticks=192, loop=None):
    return {"format": "ff7song", "version": 1, "song": 1, "channels": 1, "ticks_per_quarter": 48,
            "timer_hz": TIMER_HZ, "loop": loop, "patterns": [{"ticks": ticks, "beat": 48, "beats": 4, "cells": cells}],
            "order": [0], "restart": 0}


def synth(s):
    out = Synth(BANK, RATE)
    out.load(s)
    out.playing = True
    return out


def frames_for(ticks, tempo=0x6000):
    return math.ceil(ticks * 65536 / tempo * RATE / TIMER_HZ)


# Timing: tempo 0x6000 adds 0x6000 to a 16-bit accumulator per timer interrupt; tick 96 comes 256 interrupts in.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0}], [96, 0, {"note": 60, "len": 48}]]))
s.render(math.floor(255 * RATE / TIMER_HZ))
assert s.tick == 95 and s.channels[0].voice is None
s.render(math.ceil(2 * RATE / TIMER_HZ))
assert s.tick == 96 and s.channels[0].voice.key == 60
# The note sounds, then releases to silence after its 48 ticks.
assert np.abs(s.render(4000)).max() > 0.05
s.render(frames_for(48) + 2000)
assert s.channels[0].voice.phase == 4 and not np.abs(s.render(1000)).any()

# Loop: at loop.end the song jumps back to loop.start.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 60, "len": 48}]], ticks=96, loop={"start": 48, "end": 96}))
s.render(frames_for(100))
assert 48 <= s.tick < 60, s.tick

# Vibrato 7 (full-scale sine), depth 0x7F: the pitch swings +0.98 / -0.51 semitone around 0x1000 (C-6).
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "vibrato": [0, 1, 7, 0x7F], "note": 84, "len": 192}]]))
offsets = set()
for _ in range(200):
    s.render(128)
    offsets.add(s.vibrato_offset(s.channels[0].voice))
semis = [12 * math.log2((0x1000 + o) / 0x1000) for o in (max(offsets), min(offsets))]
assert abs(semis[0] - 0.98) < 0.05 and abs(semis[1] + 0.51) < 0.05, semis

# Seeking keeps the state the sequence set before the target, and a note right on the target still plays.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "volume": 50}], [96, 0, {"note": 60, "len": 48}]]))
s.seek(96)
assert s.playing and s.tick == 95 and s.channels[0].volume == 50 and s.tempo == 0x6000
s.render(frames_for(2))
assert s.channels[0].voice.key == 60

# A note with no length (a drone the loop ties on forever) never releases, across loop after loop.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 60, "len": None}]], ticks=96, loop={"start": 48, "end": 96}))
s.render(frames_for(500))
assert s.channels[0].voice.phase < 3 and np.abs(s.render(1000)).max() > 0.05

# Muted channels are silent; the stereo output is (frames, 2).
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 60, "len": 192}]]))
s.muted = 1
out = s.render(8000)
assert out.shape == (8000, 2) and not np.abs(out).any()

# The timeline flattens bars in order: bar starts, total length, events at absolute ticks.
two = song([[0, 0, {"inst": 0}], [10, 0, {"note": 60, "len": 5}]], ticks=96)
two["order"] = [0, 0]
starts, end, events = timeline(two)
assert starts == [0, 96] and end == 192 and [e[0] for e in events] == [0, 10, 96, 106]
print("ok")
