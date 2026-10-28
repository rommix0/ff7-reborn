import math

import numpy as np

from akao_synth import Instrument, Reverb, Synth, gaussian_weights, signed, timeline

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

# Waveform 12 is the song's noise table, stepped through in order and repeated from the start; without the table
# (an old export) it stays off.
noise = [0, 30000, -30000, 15000]
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "vibrato": [0, 1, 12, 0x7F], "note": 84, "len": 192}]])
          | {"lfo_noise": noise})
s.interrupt()  # tick 0: the note keys on
lfo, seen = s.channels[0].voice.vibrato, []
for _ in range(9):
    s.interrupt()
    seen.append(lfo.value)
assert seen == noise * 2 + [0]
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "vibrato": [0, 1, 12, 0x7F], "note": 84, "len": 192}]]))
s.render(64)
assert s.channels[0].voice.vibrato is None

# A bend in a note's own cell (a glide, DA) bends that note: the slurred note starts on 60 and reaches 62.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 60, "len": 96}],
                [48, 0, {"slur": True, "note": 60, "len": 48, "bend": [8, 2]}]]))
s.render(frames_for(58))
v = s.channels[0].voice
assert v.key == 62 and abs(v.pitch - s.pitch_of(s.channels[0], 62)) < 1e-6

# Noise (C4): the voice plays the noise generator instead of its sample. At the top clock it changes every
# sample, so neighbouring samples barely correlate (each is the last shifted), where the tone's do; adds wrap at 64.
def lag1(cells):
    s = synth(song(cells))
    out = s.render(frames_for(24))[2000:, 0]
    return np.corrcoef(out[:-1], out[1:])[0, 1], s


tone, _ = lag1([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 72, "len": 96}]])
hiss, s = lag1([[0, 0, {"tempo": 0x6000, "inst": 0, "noise": True, "noise_clock": 60, "noise_clock_add": 3,
                        "note": 72, "len": 96}]])
assert tone > 0.9 and abs(hiss) < 0.5 and s.noise_clock == 63
s = synth(song([[0, 0, {"tempo": 0x6000, "noise_clock": 62, "noise_clock_add": 3}]]))
s.render(64)
assert s.noise_clock == 1

# The noise rate: (4 + clock & 3) / (0x20000 >> (clock >> 2)) shifts per 44.1 kHz sample, at most one. Each shift
# changes the output, so counting changes over a second measures it.
for clock, per_second in ((32, 344.5), (50, 8268.75), (56, 22050), (63, 44100)):
    s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "noise": True, "noise_clock": clock, "note": 72, "len": 190}]]))
    s.render(4410)
    assert abs(np.count_nonzero(np.diff(s.render(RATE)[:, 0])) - per_second) < 2

# D4: the channel plays at the previous channel's pitch.
two = song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 72, "len": 96}],
            [0, 1, {"inst": 0, "pitch_follow": True, "note": 60, "len": 96}]])
two["channels"] = 2
s = synth(two)
s.render(frames_for(24))
assert s.channels[1].voice.pos == s.channels[0].voice.pos

# On an effect channel a bend lands on the key's plain pitch: the driver's effect path skips the detune there.
for effect, want in ((False, 1.5), (True, 1.0)):
    s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "detune": 64, "note": 60, "len": 96, "bend": [1, 0]}]])
              | ({"effect": 1} if effect else {}))
    s.render(frames_for(4))
    assert abs(s.channels[0].voice.pitch / s.pitch_of(s.channels[0], 60, detune=False) - want) < 1e-9

# D6: the volume comes from the previous channel's pitch register (2 x register x A3 / 128): at pitch 0x1000 that
# is the same as A8 64, so the output matches a plain voice at volume 64.
def peak(cells):
    two = song(cells)
    two["channels"] = 2
    s = synth(two)
    s.muted = 1  # hear channel 1 only
    return np.abs(s.render(frames_for(24))[3000:]).max()


lead = [0, 0, {"tempo": 0x6000, "inst": 0, "note": 84, "len": 96}]  # pitch 0x1000
assert abs(peak([lead, [0, 1, {"inst": 0, "volume_follow": True, "volume": 10, "note": 60, "len": 96}]]) /
           peak([lead, [0, 1, {"inst": 0, "volume": 64, "note": 60, "len": 96}]]) - 1) < 1e-3
# Past the 15-bit volume it wraps: register 0x3FC0 (key 96 detuned by 255/128) is volume 255; with A3 120 and the
# master volume, hard left, that makes register 30,243, which plays as 30,243 - 0x8000 (-0.154), not 1.85.
high = [0, 0, {"tempo": 0x6000, "inst": 0, "detune": 127, "note": 96, "len": 96}]
wrapped = peak([high, [0, 1, {"inst": 0, "volume_follow": True, "pan": 0, "note": 60, "len": 96}]])
register = round(0x3FC0 / 64 * 120 * 127 / 128 / 0x4000 * 32640 / 32768 * 0x4000)
assert abs(wrapped / TONE.max() - (0x8000 - register) / 0x4000) < 1e-3

# The pitch register is the pitch masked to 14 bits (the driver's & 0x3FFF): key 108 (0x4000) plays at 0.
s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 108, "len": 96}]]))
s.render(frames_for(4))
assert s.pitch_registers[0] == 0 and s.channels[0].voice.pos == 0
# FM: a steady modulator at 0.9 takes a 0x3000 voice (key 96, detune +64) to 1.9 x 0x3000, which stops at 0x4000:
# exactly 4 samples per output frame.
flat = Instrument(np.full(200_000, 0.9, np.float32), False, 0, (0x1000,) * 12, STEADY)
two = song([[0, 0, {"tempo": 0x6000, "inst": 2, "note": 84, "len": 190}],
            [0, 1, {"inst": 2, "fm": True, "detune": 64, "note": 96, "len": 190}]])
two["channels"] = 2
s = Synth({**BANK, 2: flat}, None, RATE)
s.load(two)
s.playing = True
s.render(2000)
start = s.channels[1].voice.pos
s.render(1000)
assert abs(s.channels[1].voice.pos - start - 4000) < 1e-6

# An instrument change while a note sounds (A1/F2) rescales its pitch by the two instruments' C pitches.
bank2 = {0: BANK[0], 1: Instrument(TONE, True, 0, (0x1800,) * 12, STEADY)}
s = Synth(bank2, None, RATE)
s.load(song([[0, 0, {"tempo": 0x6000, "inst": 0, "note": 84, "len": 96}], [24, 0, {"inst": 1}]]))  # 0x1000
s.playing = True
s.render(frames_for(30))
assert s.channels[0].voice.pitch == 0x1800 and s.channels[0].voice.inst is BANK[0]

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

# A slur on a layered channel takes over both voices (song 10, bar 6): the layer keeps sounding too.
s = Synth(two, None, RATE)
s.load(song([[0, 0, {"tempo": 0x6000, "inst": 0, "layer": [0, 1], "balance": 64, "note": 60, "len": 24}],
             [24, 0, {"slur": True, "note": 62, "len": 24}]]))
s.playing = True
s.render(frames_for(30))
ch = s.channels[0]
assert ch.voice.phase < 3 and ch.layer_voice.phase < 3 and ch.layer_voice.key == 62

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

# Level and pan follow the driver: A8 x A3 x master 127/128 of SPU full scale, split by its linear pan tables.
peaks = {}
for pan in (0, 64, 127):
    s = synth(song([[0, 0, {"tempo": 0x6000, "inst": 0, "volume": 127, "expression": 127, "pan": pan,
                            "note": 72, "len": 192}]]))
    peaks[pan] = np.abs(s.render(8000)[2000:]).max(axis=0)
full = TONE.max() * 127 * 127 * 127 / 128 / 0x4000
assert abs(peaks[0][0] / (full * 32640 / 32768) - 1) < 0.01 and abs(peaks[0][1] / peaks[0][0] - 128 / 32640) < 1e-3
assert abs(peaks[64][0] / (full * 16256 / 32768) - 1) < 0.01 and abs(peaks[64][1] / peaks[64][0] - 16512 / 16256) < 1e-3
assert abs(peaks[127][1] / (full * 32640 / 32768) - 1) < 0.01

# Gaussian interpolation: every step's four weights add up to the hardware's 0x7F80, the curve is symmetric, and
# it smooths: a signal flipping every sample comes out under half as loud, where linear interpolation passes it.
w = gaussian_weights()
assert w.shape == (256, 4) and np.allclose(w.sum(axis=1), 0x7F80 / 0x8000)
assert np.allclose(w[::-1, ::-1], w) and w[0].argmax() == 1 and w[255].argmax() == 2
assert abs(w[0] @ [-1, 1, -1, 1]) < 0.45

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

# Reverb: the stretch-at-a-time unit matches the psx-spx formula worked one sample at a time. A made-up preset with
# a 2048-sample work area whose taps sit close together, cross its end, and have two writes 8 samples apart.
registers = [0x20, 0x18, 0x6000, 0x5000, 0xC000, 0x3000, 0xD000, 0xB000, 0x5800, 0x5000,
             0x1F0, 0x1A0, 0x150, 0x140, 0x120, 0x110, 0x0F0, 0x0C0, 0x1EE, 0x0A0, 0x090, 0x080, 0x070, 0x060,
             0x050, 0x1FF, 0x040, 0x030, 0x020, 0x010, 0x8000, 0x7000]
preset = {"mode": 3, "start": 0x10000 - 0x200, "registers": registers}


def reference(left, right):
    s16 = [signed(v) for v in registers]
    dapf1, dapf2 = registers[0] * 4, registers[1] * 4
    viir, c1, c2, c3, c4, wall, apf1, apf2 = s16[2:10]
    (lsame, rsame, lc1, rc1, lc2, rc2, dlsame, drsame, ldiff, rdiff, lc3, rc3, lc4, rc4, dldiff, drdiff,
     lapf1, rapf1, lapf2, rapf2) = [v * 4 for v in registers[10:30]]
    size, ram, pos, out = 0x200 * 4, np.zeros(0x200 * 4), 0, []
    for lin, rin in zip(left * s16[30], right * s16[31]):
        def at(a):
            return (pos + a) % size
        for m, d, x in ((lsame, dlsame, lin), (rsame, drsame, rin), (ldiff, drdiff, lin), (rdiff, dldiff, rin)):
            ram[at(m)] = (x + ram[at(d)] * wall - ram[at(m - 1)]) * viir + ram[at(m - 1)]
        sides = []
        for combs, (t1, t2) in (((lc1, lc2, lc3, lc4), (lapf1, lapf2)), ((rc1, rc2, rc3, rc4), (rapf1, rapf2))):
            sides.append(sum(v * ram[at(t)] for v, t in zip((c1, c2, c3, c4), combs)))
        for (lt, rt), d, v in (((lapf1, rapf1), dapf1, apf1), ((lapf2, rapf2), dapf2, apf2)):
            for side, t in enumerate((lt, rt)):
                back = ram[at(t - d)]
                sides[side] -= v * back
                ram[at(t)] = sides[side]
                sides[side] = sides[side] * v + back
        out.append(sides)
        pos = (pos + 1) % size
    return np.array(out)


rng = np.random.default_rng(1)
left, right = rng.uniform(-0.1, 0.1, 6000) * (np.arange(6000) < 300), rng.uniform(-0.1, 0.1, 6000) * (np.arange(6000) < 300)
unit = Reverb(preset)
assert 8 <= unit.block < 64
got = np.vstack([unit.run(left[i:i + 700], right[i:i + 700]) for i in range(0, 6000, 700)])
want = reference(left, right)
assert np.abs(want[3000:]).max() > 0.01 and np.abs(got - want).max() < 1e-9

# In the synth: only channels with reverb on feed it, the depth scales what comes back, and the player's switch
# turns it off. The tail rings on after the note.
cells = [[0, 0, {"tempo": 0x6000, "inst": 0, "reverb_depth": 0x4000, "note": 60, "len": 24}]]
dry = synth(song(cells))
dry.song["reverb"] = preset
dry.reset()
assert not np.abs(dry.render(frames_for(48))[frames_for(30):]).any()  # reverb off for the channel: no tail
cells[0][2]["reverb"] = True
wet = synth(song(cells) | {"reverb": preset})
assert np.abs(wet.render(frames_for(48))[frames_for(30):]).max() > 0.001
off = synth(song(cells) | {"reverb": preset})
off.reverb_on = False
assert not np.abs(off.render(frames_for(48))[frames_for(30):]).any()
print("ok")
