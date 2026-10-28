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

# Before its first tempo command a song runs at the driver's start tempo 0xFFFF, about a tick per interrupt (songs 50
# and 85 set theirs at ticks 4 and 72). The accumulator starts at 1: tick 0 comes on the first interrupt, and since
# 0xFFFF falls just short of a carry, tick 4 on the sixth.
s = synth(song([[4, 0, {"tempo": 0x6000, "inst": 0}], [8, 0, {"note": 60, "len": 48}]]))
s.render(math.ceil(6 * RATE / TIMER_HZ))
assert s.tick == 4 and s.tempo == 0x6000
s.render(frames_for(5))
assert s.channels[0].voice and s.channels[0].voice.key == 60

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

# Master tempo scales what each interrupt adds to the tick accumulator; transpose scales every voice's pitch.
normal, fast = synth(song([[0, 0, {"tempo": 0x6000}]])), synth(song([[0, 0, {"tempo": 0x6000}]]))
fast.tempo_scale = 2.0
normal.render(frames_for(40))
fast.render(frames_for(40))
assert abs(fast.tick - 2 * normal.tick) <= 2, (normal.tick, fast.tick)
fast.tempo_scale = 4.0  # 0xFFFF x 4 per interrupt: several ticks at once
fast.tempo = 0xFFFF
before = fast.tick
fast.render(math.ceil(10 * RATE / TIMER_HZ))
assert fast.tick - before >= 38
up = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 72, "len": 192}]]))
up.transpose = 12
plain = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 72, "len": 192}]]))
for s_ in (up, plain):
    s_.render(1000)
    s_.channels[0].voice.pos = 0.0
    s_.render(60)  # under one 64-sample loop of the test tone, even at double speed
assert abs(up.channels[0].voice.pos - 2 * plain.channels[0].voice.pos) < 1e-6

# Layer (F4): each note also sounds the layer instrument on a second voice; the balance sets the split, and a
# balance slide moves it a step per tick.
two = {**BANK, 1: Instrument(TONE, True, 0, (0x2000,) * 12, STEADY)}
s = Synth(two, None, RATE)
s.load(song([[0, 0, {"tempo": 0x6000, "inst": 0, "layer": [0, 1], "balance": 32, "note": 60, "len": 96}],
             [4, 0, {"balance_slide": [4, 96]}]]))
s.playing = True
s.render(frames_for(2))
ch = s.channels[0]
# The layer voice plays the main voice's pitch register (the driver copies +0xF8), not its own instrument's table.
assert ch.layer_voice and ch.layer_voice.inst is two[1] and ch.layer_voice.follow is ch.voice
assert ch.balance == 32
s.render(frames_for(8))
assert abs(ch.balance - 96) < 1e-9

# Pan LFO: the pan swings around the channel's pan by (depth >> 1) * step >> 15, the driver's integer maths.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "pan": 64, "pan_lfo": [0, 1, 7, 40], "note": 60, "len": 192}]]))
offsets = set()
for _ in range(100):
    s.render(128)
    offsets.add(round(s.pan_offset(s.channels[0].voice)))
assert max(offsets) == 19 and min(offsets) == -20

# Depth slides (DD): the vibrato depth moves to the target over the given ticks, on the sounding note too.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "vibrato": [0, 1, 7, 0x40], "note": 60, "len": 192}],
                [8, 0, {"vibrato_slide": [8, 0]}]]))
s.render(frames_for(10))
assert 0 < s.channels[0].vibrato[3] < 0x40
s.render(frames_for(10))
assert s.channels[0].vibrato[3] == 0 and s.channels[0].voice.vibrato.amount == 0

# FM (C6): the voice's sample steps follow the previous channel's output, so it runs at a different rate.
fm = song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 60, "len": 192}],
           [0, 1, {"inst": 0, "fm": True, "note": 60, "len": 192}]])
fm["channels"] = 2
s = synth(fm)
s.render(frames_for(4))
plain, modulated = s.channels[0].voice, s.channels[1].voice
plain.pos = modulated.pos = 0.0
s.render(37)
assert abs(modulated.pos - plain.pos) > 0.5

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
