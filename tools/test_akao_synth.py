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


def synth(s, bank2=None):
    out = Synth(BANK, bank2, RATE)
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

# The channel's own envelope: rr 0x1A releases far slower than the instrument's 6, until the next instrument change.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "rr": 0x1A, "note": 60, "len": 4}]]))
s.render(frames_for(4) + 3000)
assert s.channels[0].voice.phase == 3 and s.channels[0].voice.level > 0x4000
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "rr": 0x1A}], [1, 0, {"inst": 0, "note": 60, "len": 4}]]))
s.render(frames_for(5) + 3000)
assert s.channels[0].voice.phase == 4
# A slurred note takes over the sounding voice: same voice, sample position not restarted, new key.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 60, "len": 24}],
                [24, 0, {"slur": True, "note": 62, "len": 24}]]))
s.render(frames_for(20))
first = s.channels[0].voice
s.render(frames_for(6))
assert s.channels[0].voice is first and first.key == 62 and first.phase < 3  # not released between the two

# Overlap (F8): the previous note keeps ringing in its release on the channel's second voice while the next starts,
# with the given release rate, which an instrument change doesn't reset. Without it the new note cuts the old.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "overlap": 0x0C, "note": 60, "len": 4}],
                [8, 0, {"inst": 0, "note": 62, "len": 4}]]))
s.render(frames_for(9))
ch = s.channels[0]
assert ch.adsr[4] == 0x0C and ch.voice.key == 62 and ch.tail.key == 60 and ch.tail.phase == 3
assert ch.tail.level > 0x1000  # release rate 0x0C rings on for seconds
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 60, "len": 4}], [8, 0, {"note": 62, "len": 4}]]))
s.render(frames_for(9))
assert s.channels[0].tail is None

# A song marked "bank": 2 gets the second bank in slots 53 and up, with nothing of bank 1 left there.
s = Synth({**BANK, 53: BANK[0], 60: BANK[0]}, {54: BANK[0]}, RATE)
s.load({**song([]), "bank": 2})
assert set(s.instruments) == {0, 54}
s.load(song([]))
assert set(s.instruments) == {0, 53, 60}

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
