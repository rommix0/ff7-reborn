"""AKAO driver emulation for .ff7song files (docs/ff7song_format.md), mixed with numpy.

The sequencer runs like the driver: a timer interrupt at DRIVER_HZ adds the tempo to a 16-bit accumulator, every
carry is a tick, LFOs step on every interrupt. Voices are mixed in numpy between interrupts, with the SPU ADSR
envelope worked out per stretch rather than per sample.
"""
import collections
import math
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import NamedTuple

import numpy as np

from xm import lfo_wave

SPU_HZ = 44100
ATTACK, DECAY, SUSTAIN, RELEASE, OFF = range(5)


class Instrument(NamedTuple):
    pcm: np.ndarray  # float32, -1..1
    loops: bool
    loop_start: int
    pitches: tuple  # SPU pitch of C..B at octave 6, 0x1000 = 44.1 kHz
    adsr: bytes


@dataclass
class Lfo:
    delay: int
    rate: int
    depth: int
    steps: list
    loop: int
    countdown: int = 1
    index: int = 0
    value: int = 0
    amount: float = 0.0  # vibrato: the depth as SPU pitch units at the note's pitch


@dataclass
class Voice:
    inst: Instrument
    key: int
    pitch: float
    left: int  # ticks until key-off; -1 holds it until the channel's next note
    vibrato: Lfo | None
    tremolo: Lfo | None
    pos: float = 0.0
    bend: list | None = None  # [from, to, ticks, done]
    phase: int = ATTACK
    level: float = 0.0
    env_count: float = 0.0


@dataclass
class Channel:
    inst: int = 5
    volume: int = 100
    expression: int = 120
    pan: int = 64
    detune: int = 0
    vibrato: list | None = None
    tremolo: list | None = None
    voice: Voice | None = None


def read_bank(path: Path) -> dict:
    """slot -> Instrument from an .ff7bank file."""
    data = path.read_bytes()
    if data[:8] != b"FF7BANK\0":
        raise ValueError(f"{path} is not an .ff7bank file")
    count = struct.unpack_from("<H", data, 10)[0]
    bank, at = {}, 12 + count * 68
    for i in range(count):
        slot, flags, length, loop, *rest = struct.unpack_from("<HHII12I8s", data, 12 + i * 68)
        pcm = np.frombuffer(data, "<i2", length, at).astype(np.float32) / 32768
        bank[slot] = Instrument(pcm, bool(flags & 1), loop, tuple(rest[:12]), rest[12])
        at += 2 * length
    return bank


def timeline(song: dict) -> tuple[list[int], int, list]:
    """(bar start ticks, song length, [tick, channel, fields] in time order) for a song's order list."""
    starts, t = [], 0
    for p in song["order"]:
        starts.append(t)
        t += song["patterns"][p]["ticks"]
    events = [[start + tick, ch, fields] for start, p in zip(starts, song["order"])
              for tick, ch, fields in song["patterns"][p]["cells"]]
    events.sort(key=lambda e: e[0])
    return starts, t, events


class Synth:
    def __init__(self, bank: dict, rate: int = SPU_HZ):
        self.bank, self.rate = bank, rate
        self.song = None
        self.tick, self.tempo = -1, 0
        self.muted = 0
        self.playing = False
        # Commands from the UI thread, run at the start of the next audio block so state only changes there.
        self.commands = collections.deque()

    def post(self, command):
        self.commands.append(command)

    def load(self, song: dict):
        self.song = song
        self.starts, self.end, self.events = timeline(song)
        self.reset()

    def update(self, song: dict):
        """Take an edited song without losing the play position."""
        self.song = song
        self.starts, self.end, self.events = timeline(song)
        self.next = next((i for i, e in enumerate(self.events) if e[0] > self.tick), len(self.events))

    def reset(self):
        self.tick, self.next, self.tempo, self.accumulator = -1, 0, 0, 0
        self.until_interrupt = 0.0
        self.ended = False
        self.channels = [Channel() for _ in range(self.song["channels"])]

    def seek(self, target: int):
        """Run the sequence silently up to the tick before `target`, so its state is right and notes on it play."""
        playing = self.playing
        self.reset()
        while self.tick < target - 1 and not self.ended:
            self.step_tick()
        for ch in self.channels:
            ch.voice = None
        self.playing = playing

    def pitch_of(self, ch: Channel, key: int) -> float:
        inst = self.bank.get(ch.inst)
        if not inst:
            return 0.0
        pitch = inst.pitches[key % 12] * 2.0 ** (key // 12 - 1 - 6)
        return pitch * (1 + ch.detune / (128 if ch.detune > 0 else 256))

    @staticmethod
    def start_lfo(settings) -> Lfo | None:
        """Each note restarts the channel's LFOs: `delay` ticks, then a waveform step every `rate` interrupts."""
        if not settings or not settings[3]:
            return None
        delay, rate, waveform, depth = settings
        wave = lfo_wave(waveform)
        return Lfo(delay, rate, depth, *wave) if wave else None

    def key_on(self, ch: Channel, key: int, length: int):
        inst = self.bank.get(ch.inst)
        if not inst:
            ch.voice = None
            return
        pitch = self.pitch_of(ch, key)
        vibrato = self.start_lfo(ch.vibrato)
        if vibrato:
            # (d & 0x7F)/128 of the pitch with d's top bit set, else 15/256 of that.
            d = vibrato.depth
            vibrato.amount = (d & 0x7F) * (pitch if d & 0x80 else pitch * 15 / 256) / 128
        ch.voice = Voice(inst, key, pitch, -1 if length is None else length, vibrato, self.start_lfo(ch.tremolo))

    def step_tick(self):
        self.tick += 1
        loop = self.song["loop"]
        if loop and self.tick >= loop["end"]:
            self.tick = loop["start"]
            self.next = next((i for i, e in enumerate(self.events) if e[0] >= self.tick), len(self.events))
        elif self.tick >= self.end:
            self.ended = True
            return
        for ch in self.channels:
            v = ch.voice
            if not v:
                continue
            if v.left > 0:
                v.left -= 1
                if not v.left and v.phase < RELEASE:
                    v.phase = RELEASE
            for lfo in (v.vibrato, v.tremolo):
                if lfo and lfo.delay > 0:
                    lfo.delay -= 1
            if v.bend and v.bend[3] < v.bend[2]:
                v.bend[3] += 1
                start, target, ticks, done = v.bend
                v.pitch = start + (target - start) * done / ticks  # linear in frequency
        while self.next < len(self.events) and self.events[self.next][0] == self.tick:
            _, c, f = self.events[self.next]
            self.next += 1
            ch = self.channels[c]
            for name in ("inst", "volume", "expression", "pan", "detune", "vibrato", "tremolo"):
                if name in f:
                    setattr(ch, name, f[name])
            self.tempo = f.get("tempo", self.tempo)
            if "bend" in f and ch.voice:
                ticks, semitones = f["bend"]
                ch.voice.bend = [ch.voice.pitch, self.pitch_of(ch, ch.voice.key + semitones), ticks, 0]
                ch.voice.key += semitones
            if "note" in f:
                self.key_on(ch, f["note"], f["len"])

    def interrupt(self):
        for ch in self.channels:
            if ch.voice:
                for lfo in (ch.voice.vibrato, ch.voice.tremolo):
                    if lfo and lfo.delay <= 0:
                        lfo.countdown -= 1
                        if lfo.countdown <= 0:
                            lfo.countdown = lfo.rate
                            lfo.value = lfo.steps[lfo.index]
                            lfo.index = lfo.index + 1 if lfo.index + 1 < len(lfo.steps) else lfo.loop
        if self.tick < 0:  # the song's first tick sets its tempo
            self.step_tick()
            return
        self.accumulator += self.tempo
        if self.accumulator >= 0x10000:
            self.accumulator -= 0x10000
            self.step_tick()

    @staticmethod
    def vibrato_offset(v: Voice) -> int:
        if not v.vibrato or not v.vibrato.amount:
            return 0
        offset = math.floor(v.vibrato.amount * v.vibrato.value / 65536)
        return 2 * offset if offset > 0 else offset  # the driver doubles the upward half

    def envelope(self, v: Voice, n: int) -> np.ndarray:
        """The SPU envelope (psx-spx ADSR rules, 44.1 kHz) over the next n output samples, 0..1."""
        ar, dr, sl, sr, rr, a_mode, s_mode, r_mode = v.inst.adsr
        match v.phase:
            case 0:
                shift, step, exponential, rising, stop = ar >> 2, 7 - (ar & 3), a_mode == 5, True, 0x7FFF
            case 1:
                shift, step, exponential, rising, stop = dr, -8, True, False, min(0x7FFF, (sl + 1) * 0x800)
            case 2:
                rising = s_mode in (1, 5)
                shift, exponential = sr >> 2, s_mode in (5, 7)
                step, stop = (7 - (sr & 3), 0x7FFF) if rising else (-8 + (sr & 3), 0)
            case 3:
                shift, step, exponential, rising, stop = rr, -8, r_mode == 7, False, 0
            case _:
                return np.zeros(n)
        cycles = 1 << max(0, shift - 11)
        if exponential and rising and v.level > 0x6000:
            cycles *= 4
        delta = step << max(0, 11 - shift)
        # Envelope steps passed by each output sample, counting the part-step carried over from last time.
        steps = np.floor(v.env_count + np.arange(1, n + 1) * (SPU_HZ / self.rate)) // cycles
        v.env_count = (v.env_count + n * SPU_HZ / self.rate) % cycles
        if exponential and not rising:
            levels = v.level * (1 + delta / 32768) ** steps
        else:
            levels = v.level + delta * steps
        levels = np.minimum(levels, stop) if rising else np.maximum(levels, stop)
        v.level = float(levels[-1])
        if v.level == stop or (not rising and v.level < 1):
            v.level = 0.0 if v.phase == RELEASE and v.level < 1 else v.level
            v.phase = {ATTACK: DECAY, DECAY: SUSTAIN, SUSTAIN: SUSTAIN, RELEASE: OFF}[v.phase]
        return np.clip(levels, 0, 0x7FFF) / 0x7FFF

    def mix(self, out: np.ndarray):
        n = len(out)
        for c, ch in enumerate(self.channels):
            v = ch.voice
            if not v or v.phase == OFF:
                continue
            env = self.envelope(v, n)
            pitch = min(0x3FFF, max(0.0, v.pitch + self.vibrato_offset(v)))
            step = pitch / 4096 * SPU_HZ / self.rate
            pcm, length = v.inst.pcm, len(v.inst.pcm)
            pos = v.pos + step * np.arange(n)
            index = pos.astype(np.int64)
            frac = (pos - index).astype(np.float32)
            nxt = index + 1
            if v.inst.loops:
                span = length - v.inst.loop_start
                index = np.where(index >= length, v.inst.loop_start + (index - v.inst.loop_start) % span, index)
                nxt = np.where(nxt >= length, v.inst.loop_start + (nxt - v.inst.loop_start) % span, nxt)
                v.pos += step * n
                if v.pos >= length:
                    v.pos = v.inst.loop_start + (v.pos - v.inst.loop_start) % span
                ended = None
            else:
                ended = index >= length
                index, nxt = np.minimum(index, length - 1), np.minimum(nxt, length - 1)
                v.pos += step * n
                if v.pos >= length:
                    v.phase = OFF
            sample = pcm[index] + (pcm[nxt] - pcm[index]) * frac
            if ended is not None:
                sample[ended] = 0
            if self.muted >> c & 1:
                continue
            gain = ch.volume * ch.expression / (127 * 127) * 0.5
            if v.tremolo:
                gain *= max(0.0, 1 + v.tremolo.depth / 128 * v.tremolo.value / 32768)
            angle = ch.pan / 127 * math.pi / 2
            voiced = sample * env * gain
            out[:, 0] += voiced * math.cos(angle)
            out[:, 1] += voiced * math.sin(angle)

    def render(self, frames: int) -> np.ndarray:
        """The next `frames` stereo samples, (frames, 2) float32."""
        while self.commands:
            self.commands.popleft()(self)
        out = np.zeros((frames, 2), np.float32)
        if not self.song or not self.playing:
            return out
        per_interrupt = self.rate / self.song["timer_hz"]
        done = 0
        while done < frames:
            if self.until_interrupt <= 0:
                self.until_interrupt += per_interrupt
                self.interrupt()
            n = min(frames - done, max(1, math.ceil(self.until_interrupt)))
            self.until_interrupt -= n
            self.mix(out[done:done + n])
            done += n
        if self.ended and all(not ch.voice or ch.voice.phase == OFF for ch in self.channels):
            self.playing = False
        return np.clip(out, -1, 1, out=out)
