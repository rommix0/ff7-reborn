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
REVERB_HZ = SPU_HZ / 2  # the SPU runs its reverb on every other sample
MASTER_VOLUME = 127 / 128  # the driver's master volume at boot (0x80062F5E = 0x7F)
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
    adsr: list  # the channel's envelope settings, shared so AD/B1/... reach a sounding voice as in the driver
    key: int
    pitch: float
    left: int  # ticks until key-off; -1 holds it until the channel's next note
    vibrato: Lfo | None
    tremolo: Lfo | None
    pan_lfo: Lfo | None = None
    follow: "Voice | None" = None  # a layer voice (F4) plays the main voice's pitch
    pos: float = 0.0
    bend: list | None = None  # [from, to, ticks, done]
    phase: int = ATTACK
    released_from: int = ATTACK  # the phase a key-off interrupted, for a slur that lands on the same tick
    held_pitch: int | None = None  # the last pitch register written, which the SPU keeps through the release
    held_mix: tuple | None = None  # likewise (gain, left, right)
    level: float = 0.0
    env_count: float = 0.0  # the part of the current envelope step already waited, 0..1


@dataclass
class Channel:
    inst: int = 5
    volume: int = 100
    expression: int = 120
    pan: int = 64
    detune: int = 0
    vibrato: list | None = None
    tremolo: list | None = None
    adsr: list = field(default_factory=lambda: [0, 0x0F, 0x0F, 0x7F, 0x06, 1, 3, 3])  # the instrument's, or AD/B1...
    pan_lfo: list | None = None
    vibrato_slide: list | None = None  # [depth now, step per tick, ticks left]
    tremolo_slide: list | None = None
    fm: bool = False  # pitch follows the previous channel's output (C6)
    overlap: int | None = None  # F8's release rate while two voices take turns, None when off
    layer_inst: int | None = None  # F4's second instrument, sounding every note on a second voice
    layer_adsr: list = field(default_factory=list)
    balance: float = 0.0  # F6: main voice at (127 - balance)/256, layer at balance/256
    balance_slide: list | None = None  # [step per tick, ticks left]
    voice: Voice | None = None
    tail: Voice | None = None  # the previous note, ringing on in its release while overlap is on
    layer_voice: Voice | None = None
    reverb: bool = False  # C2/C3: the channel's voices feed the reverb
    noise: bool = False  # C4/C5: the voices play the SPU noise generator instead of their sample
    pitch_follow: bool = False  # D4/D5: the previous channel's pitch, plus this channel's own vibrato
    volume_follow: bool = False  # D6/D7: the volume from the previous channel's pitch register


ENVELOPE_FIELDS = {"ar": 0, "dr": 1, "sl": 2, "sr": 3, "rr": 4, "sr_mode": 6}  # cell field -> ADSR byte


def read_bank(path: Path) -> tuple[dict, dict]:
    """(bank 1, bank 2): slot -> Instrument from an .ff7bank file."""
    data = path.read_bytes()
    if data[:8] != b"FF7BANK\0":
        raise ValueError(f"{path} is not an .ff7bank file")
    count = struct.unpack_from("<H", data, 10)[0]
    banks, at = ({}, {}), 12 + count * 68
    for i in range(count):
        slot, flags, length, loop, *rest = struct.unpack_from("<HHII12I8s", data, 12 + i * 68)
        pcm = np.frombuffer(data, "<i2", length, at).astype(np.float32) / 32768
        banks[flags >> 1 & 1][slot] = Instrument(pcm, bool(flags & 1), loop, tuple(rest[:12]), rest[12])
        at += 2 * length
    return banks


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


def gaussian_weights() -> np.ndarray:
    """(256, 4): the SPU's 4-point interpolation weights for each step between two samples, applied to the sample
    before the position, at it, and the two after.

    psx-spx documents the hardware's 512-entry table but no formula. A Gaussian (sigma 0.85 samples) under a
    (1 - (d/2)^2)^3 window, scaled so each step's weights add up to 0x7F80 as the hardware's do, comes within 17 of
    its 32,768ths, which differs from it by about -63 dB on white noise.
    """
    f = (np.arange(256) + 0.5) / 256  # the middle of each step

    def kernel(d):
        return np.exp(-d ** 2 / (2 * 0.85 ** 2)) * (1 - (d / 2) ** 2) ** 3
    weights = np.column_stack([kernel(1 + f), kernel(f), kernel(1 - f), kernel(2 - f)])
    return weights * (0x7F80 / 0x8000 / weights.sum(axis=1, keepdims=True))


def noise_levels() -> np.ndarray:
    """The SPU noise generator's output after each shift, from power-on (psx-spx): a 16-bit register that shifts in
    the inverted parity of bits 15, 12, 11 and 10. One cycle of it, as -1..1."""
    level, seen, levels = 0, {}, []
    while level not in seen:
        seen[level] = len(levels)
        parity = (level >> 15 ^ level >> 12 ^ level >> 11 ^ level >> 10 ^ 1) & 1
        level = (level << 1 | parity) & 0xFFFF
        levels.append(level - 0x10000 if level & 0x8000 else level)
    return np.array(levels[seen[level]:], np.float64) / 32768


def reverb_filter(rate: float) -> np.ndarray:
    """The low-pass the SPU puts around its 22.05 kHz reverb, for output at `rate`: our own windowed sinc (Kaiser,
    beta 13) cut off at 11.025 kHz, 39 taps at 44.1 kHz. That is the hardware's half-band in shape (within 0.1 dB to
    12 kHz), where a plain average or straight-line interpolation lets aliases through."""
    half = round(19 * rate / SPU_HZ)
    cutoff = REVERB_HZ / 2 / rate
    taps = 2 * cutoff * np.sinc(2 * cutoff * np.arange(-half, half + 1)) * np.kaiser(2 * half + 1, 13)
    return taps / taps.sum()


def signed(register: int) -> float:
    """A reverb volume register as a fraction, 0x8000 = -1."""
    return (register - 0x10000 if register & 0x8000 else register) / 0x8000


class Reverb:
    """The SPU reverb (psx-spx, "SPU Reverb Formula") on its own work area, running at 22.05 kHz.

    The SPU works sample by sample. Here a stretch of `block` samples goes at once in numpy: every read comes from
    the work area as it was before the stretch, then the writes follow. That matches the SPU while the stretch is no
    longer than the shortest delay from a write to a read of the same place. The one shorter loop, each filter
    reading its own last output, is worked out as a filter instead.
    """

    def __init__(self, preset: dict, rate: float = SPU_HZ):
        r = preset["registers"]
        self.apf_delays = (r[0] * 4, r[1] * 4)  # addresses count 8 bytes, the work area holds 16-bit samples
        self.iir, *self.combs, self.wall, apf1, apf2 = map(signed, r[2:10])
        self.apf = (apf1, apf2)
        m = {name: v * 4 for name, v in zip(
            "lsame rsame lcomb1 rcomb1 lcomb2 rcomb2 dlsame drsame ldiff rdiff lcomb3 rcomb3 lcomb4 rcomb4 dldiff "
            "drdiff lapf1 rapf1 lapf2 rapf2".split(), r[10:30])}
        self.vlin, self.vrin = signed(r[30]), signed(r[31])
        # (write, read, input side) for the four filters: same-side and cross-side reflections.
        self.filters = ((m["lsame"], m["dlsame"], 0), (m["rsame"], m["drsame"], 1),
                        (m["ldiff"], m["drdiff"], 0), (m["rdiff"], m["dldiff"], 1))
        self.comb_taps = ([m[f"lcomb{i}"] for i in range(1, 5)], [m[f"rcomb{i}"] for i in range(1, 5)])
        self.apf_taps = ((m["lapf1"], m["rapf1"]), (m["lapf2"], m["rapf2"]))
        self.size = (0x10000 - preset["start"]) * 4  # from the work area's start to the end of SPU RAM
        writes = [f[0] for f in self.filters] + [tap for taps in self.apf_taps for tap in taps]  # in formula order
        reads = [f[1] for f in self.filters] + self.comb_taps[0] + self.comb_taps[1] + \
            [tap - delay for taps, delay in zip(self.apf_taps, self.apf_delays) for tap in taps]
        # Two writes to one place within a stretch must land in time order: the smaller address writes later. Writes
        # go in address order, counted from just after the widest gap between them, so only a pair across that gap
        # could swap.
        offsets = sorted(set(writes))
        gap, self.cut = max(((b - a) % self.size or self.size, b) for a, b in zip(offsets, offsets[1:] + offsets[:1]))
        delays = [(w - r) % self.size for w in writes for r in reads]  # from a write to a read of the same place
        self.block = min([d for d in delays if d] + [gap, 512])
        c = 1 - self.iir
        powers = c ** np.arange(self.block)
        self.kernel = self.iir * powers[np.abs(powers) > 1e-10]  # the filter's response; older input has died out
        self.decay = c ** np.arange(1, self.block + 1)
        self.ram = np.zeros(self.size)
        self.pos = 0
        self.last = np.zeros(4)  # each filter's last output
        # Resampling to and from 22.05 kHz through the low-pass: the next reverb sample's time in output frames, the
        # input frames the filter still needs, and the filtered output that runs into the next block.
        self.rate, self.lowpass = rate, reverb_filter(rate)
        self.phase = 0.0
        self.history = np.zeros((len(self.lowpass) - 1, 2))
        self.carry = np.zeros((len(self.lowpass), 2))
        self.peak = 0.0

    def run(self, left: np.ndarray, right: np.ndarray) -> np.ndarray:
        ram, size = self.ram, self.size
        out = np.empty((len(left), 2))
        for start in range(0, len(left), self.block):
            b = min(self.block, len(left) - start)
            at = np.arange(self.pos, self.pos + b)
            inputs = (self.vlin * left[start:start + b], self.vrin * right[start:start + b])
            reflections = [ram[(at + read) % size] for _, read, _ in self.filters]
            sides = [sum(v * ram[(at + tap) % size] for v, tap in zip(self.combs, taps)) for taps in self.comb_taps]
            backs = [[ram[(at + tap - delay) % size] for tap in taps]
                     for taps, delay in zip(self.apf_taps, self.apf_delays)]
            written = []  # (address, samples) in formula order
            for k, ((write, _, side), reflection) in enumerate(zip(self.filters, reflections)):
                x = inputs[side] + reflection * self.wall
                y = np.clip(np.convolve(x, self.kernel)[:b] + self.decay[:b] * self.last[k], -1, 1)
                written.append((write, y))
                self.last[k] = y[-1]
            for taps, back, v in zip(self.apf_taps, backs, self.apf):
                for side, tap in enumerate(taps):
                    sides[side] = sides[side] - v * back[side]
                    written.append((tap, np.clip(sides[side], -1, 1)))
                    sides[side] = sides[side] * v + back[side]
            for write, samples in sorted(written, key=lambda w: -((w[0] - self.cut) % size)):  # stable: ties in order
                ram[(at + write) % size] = samples
            out[start:start + b] = np.column_stack(sides)
            self.pos = (self.pos + b) % size
        return out

    def process(self, send: np.ndarray) -> np.ndarray:
        """The reverb's output for `send` (frames, 2) at the output rate, before the depth is applied."""
        n, period = len(send), self.rate / REVERB_HZ
        times = np.arange(self.phase, n, period)
        # Down: low-pass, then take the filtered signal at the reverb's sample times (every other frame at 44.1 kHz).
        padded = np.vstack([self.history, send])
        filtered = np.column_stack([np.convolve(padded[:, side], self.lowpass, "valid") for side in (0, 1)])
        self.history = padded[len(padded) - len(self.history):]
        frames = np.arange(n)
        wet = self.run(np.interp(times, frames, filtered[:, 0]), np.interp(times, frames, filtered[:, 1]))
        # Up: each reverb sample as an impulse worth `period` frames (zero-stuffing at 44.1 kHz), then the low-pass.
        at = times.astype(np.int64)
        share = (times - at)[:, None]
        impulses = np.zeros((n + 1, 2))
        np.add.at(impulses, at, wet * (1 - share) * period)
        np.add.at(impulses, at + 1, wet * share * period)
        out = np.column_stack([np.convolve(impulses[:, side], self.lowpass) for side in (0, 1)])
        out[:len(self.carry)] += self.carry
        out, self.carry = out[:n], out[n:]
        self.phase = (times[-1] + period if len(times) else self.phase) - n
        self.peak = float(np.abs(out).max(initial=0))
        return out


class Synth:
    def __init__(self, bank: dict, bank2: dict | None = None, rate: int = SPU_HZ):
        self.bank, self.bank2, self.rate = bank, bank2 or {}, rate
        self.gauss = gaussian_weights()
        self.noise = noise_levels()
        self.instruments = bank
        self.song = None
        self.tick, self.tempo = -1, 0
        self.muted = 0
        self.tempo_scale = 1.0  # master tempo, on top of the song's own
        self.transpose = 0  # master transpose in semitones
        self.reverb_on = True  # the player's switch; the song still sets mode, depth and sends
        self.reverb = None
        self.playing = False
        # Commands from the UI thread, run at the start of the next audio block so state only changes there.
        self.commands = collections.deque()

    def post(self, command):
        self.commands.append(command)

    def load(self, song: dict):
        self.song = song
        # Bank 2 replaces slots 53 and up, leaving only its own instruments there (load_instrument_bank2).
        self.instruments = self.bank
        if song.get("bank", 1) == 2:
            self.instruments = {slot: i for slot, i in self.bank.items() if slot < 53} | self.bank2
        self.starts, self.end, self.events = timeline(song)
        self.reset()

    def update(self, song: dict):
        """Take an edited song without losing the play position."""
        self.song = song
        self.starts, self.end, self.events = timeline(song)
        self.next = next((i for i, e in enumerate(self.events) if e[0] > self.tick), len(self.events))

    def reset(self):
        # The driver's song start (func_80029C48): tempo 0xFFFF and accumulator 1, so tick 0 comes on the first
        # interrupt and the song runs about a tick per interrupt until its first tempo command.
        self.tick, self.next, self.tempo, self.accumulator = -1, 0, 0xFFFF, 1
        self.until_interrupt = 0.0
        self.ended = False
        self.channels = [Channel() for _ in range(self.song["channels"])]
        # The driver sets the song's reverb mode, clearing the work area, and zeroes the depth (EA sets it).
        self.reverb = Reverb(self.song["reverb"], self.rate) if self.song.get("reverb") else None
        self.reverb_depth = 0.0
        self.noise_clock, self.noise_pos = 0, 0.0  # AC's clock (one for all voices) and shifts so far
        self.pitch_registers = {}  # channel -> its main voice's last pitch, which D6 reads even after the note
        for ch in self.channels:
            self.set_instrument(ch, ch.inst)

    def set_instrument(self, ch: Channel, slot: int):
        """The driver's load_channel_instrument: a program change also resets the channel's envelope."""
        ch.inst = slot
        if slot in self.instruments:
            ch.adsr = list(self.instruments[slot].adsr)
        if ch.overlap is not None:  # F8's release rate survives program changes
            ch.adsr[4] = ch.overlap

    def seek(self, target: int):
        """Run the sequence silently up to the tick before `target`, so its state is right and notes on it play."""
        playing = self.playing
        self.reset()
        while self.tick < target - 1 and not self.ended:
            self.step_tick()
        for ch in self.channels:
            ch.voice = ch.tail = ch.layer_voice = None
        self.playing = playing

    def pitch_of(self, ch: Channel, key: int, detune: bool = True) -> float:
        inst = self.instruments.get(ch.inst)
        if not inst:
            return 0.0
        pitch = inst.pitches[key % 12] * 2.0 ** (key // 12 - 1 - 6)
        return pitch * (1 + ch.detune / (128 if ch.detune > 0 else 256)) if detune else pitch

    def start_lfo(self, settings) -> Lfo | None:
        """Each note restarts the channel's LFOs: `delay` ticks, then a waveform step every `rate` interrupts."""
        if not settings or not settings[3]:
            return None
        delay, rate, waveform, depth = settings
        wave = (self.song["lfo_noise"], 0) if waveform == 12 and "lfo_noise" in self.song else lfo_wave(waveform)
        return Lfo(delay, rate, depth, *wave) if wave else None

    def key_on(self, ch: Channel, key: int, length: int):
        inst = self.instruments.get(ch.inst)
        if not inst:
            ch.voice = None
            return
        if ch.overlap is not None and ch.voice:
            # The new note takes the channel's other voice; this one goes on in its release.
            if ch.voice.phase < RELEASE:
                ch.voice.released_from, ch.voice.phase = ch.voice.phase, RELEASE
            ch.tail = ch.voice
        pitch = self.pitch_of(ch, key)
        vibrato = self.start_lfo(ch.vibrato)
        if vibrato:
            self.set_vibrato_amount(vibrato, pitch)
        left = -1 if length is None else length
        ch.voice = Voice(inst, ch.adsr, key, pitch, left, vibrato, self.start_lfo(ch.tremolo),
                         self.start_lfo(ch.pan_lfo))
        layer = self.instruments.get(ch.layer_inst) if ch.layer_inst is not None else None
        ch.layer_voice = Voice(layer, ch.layer_adsr, key, pitch, left, None, None, follow=ch.voice) if layer else None

    def step_tick(self):
        self.tick += 1
        loop = self.song["loop"]
        if loop and self.tick >= loop["end"]:
            self.tick = loop["start"]
            self.next = next((i for i, e in enumerate(self.events) if e[0] >= self.tick), len(self.events))
        elif self.tick >= self.end:
            if not self.ended:
                # The driver's A0 keys nothing off: a note held to the end sounds on at its last volume until the
                # music or the next effect takes the voice (effect 643). The player has neither, so it keys off.
                for ch in self.channels:
                    for v in (ch.voice, ch.tail, ch.layer_voice):
                        if v and v.phase < RELEASE:
                            v.released_from, v.phase = v.phase, RELEASE
            self.ended = True
            return
        for ch in self.channels:
            for kind in ("vibrato", "tremolo"):
                slide, settings = getattr(ch, f"{kind}_slide"), getattr(ch, kind)
                if slide and settings and settings[3]:
                    slide[0] += slide[1]
                    slide[2] -= 1
                    settings[3] = round(slide[0])
                    lfo = getattr(ch.voice, kind) if ch.voice else None
                    if lfo:
                        lfo.depth = settings[3]
                        if kind == "vibrato":
                            self.set_vibrato_amount(lfo, ch.voice.pitch)
                    if not slide[2]:
                        setattr(ch, f"{kind}_slide", None)
            if ch.balance_slide:
                ch.balance += ch.balance_slide[0]
                ch.balance_slide[1] -= 1
                if not ch.balance_slide[1]:
                    ch.balance_slide = None
            for v in (ch.layer_voice, ch.voice):
                if v and v.left > 0:
                    v.left -= 1
                    if not v.left and v.phase < RELEASE:
                        v.released_from, v.phase = v.phase, RELEASE
            v = ch.voice
            if not v:
                continue
            for lfo in (v.vibrato, v.tremolo, v.pan_lfo):
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
            if "inst" in f:
                old, new = self.instruments.get(ch.inst), self.instruments.get(f["inst"])
                v = ch.voice
                if v and v.phase != OFF and old and new:
                    # A1/F2 rescale a sounding voice's pitch by the instruments' C pitches; it keeps its sample.
                    ratio = new.pitches[0] / old.pitches[0]
                    v.pitch *= ratio
                    if v.bend:
                        v.bend[0] *= ratio
                        v.bend[1] *= ratio
                self.set_instrument(ch, f["inst"])
            for name in ("volume", "expression", "pan", "detune", "vibrato", "tremolo", "pan_lfo", "fm", "balance"):
                if name in f:
                    setattr(ch, name, list(f[name]) if isinstance(f[name], list) else f[name])
            for kind in ("vibrato", "tremolo", "pan_lfo"):
                # The LFO commands reach a sounding voice at once: on restarts it, off stops it, depth applies now.
                if kind not in f or not ch.voice:
                    continue
                v, settings = ch.voice, getattr(ch, kind)
                if f.get(f"{kind}_on") or not settings[3]:
                    setattr(v, kind, self.start_lfo(settings))
                elif getattr(v, kind):
                    getattr(v, kind).depth = settings[3]
                if kind == "vibrato" and v.vibrato:
                    self.set_vibrato_amount(v.vibrato, v.pitch)
            for kind in ("vibrato", "tremolo"):
                if f"{kind}_slide" in f:
                    ticks, target = f[f"{kind}_slide"]
                    now = getattr(ch, kind)[3] if getattr(ch, kind) else 0
                    setattr(ch, f"{kind}_slide", [float(now), (target - now) / ticks, ticks])
            if "balance_slide" in f:
                ticks, target = f["balance_slide"]
                ch.balance_slide = [(target - ch.balance) / ticks, ticks]
            if "layer" in f:
                ch.layer_inst = f["layer"][1] if f["layer"] else None
                if ch.layer_inst is None:
                    ch.layer_voice = None  # F5 frees the second voice
                elif ch.layer_inst in self.instruments:
                    ch.layer_adsr = list(self.instruments[ch.layer_inst].adsr)
            ch.noise = f.get("noise", ch.noise)
            ch.pitch_follow = f.get("pitch_follow", ch.pitch_follow)
            ch.volume_follow = f.get("volume_follow", ch.volume_follow)
            if "noise_clock" in f:
                self.noise_clock = f["noise_clock"]
            if "noise_clock_add" in f:
                self.noise_clock = (self.noise_clock + f["noise_clock_add"]) & 0x3F
            if f.get("adsr_reset") and ch.inst in self.instruments:
                ch.adsr = list(self.instruments[ch.inst].adsr)
            if "overlap" in f:
                ch.overlap = f["overlap"]
                inst = self.instruments.get(ch.inst)
                ch.adsr[4] = ch.overlap if ch.overlap is not None else inst.adsr[4] if inst else ch.adsr[4]
            for name, index in ENVELOPE_FIELDS.items():
                if name in f:
                    ch.adsr[index] = f[name]
                    if ch.layer_adsr:  # the driver writes them to the layer voice's channel too
                        ch.layer_adsr[index] = f[name]
            self.tempo = f.get("tempo", self.tempo)
            ch.reverb = f.get("reverb", ch.reverb)
            if "reverb_depth" in f:
                self.reverb_depth = signed(f["reverb_depth"])
            if "note" in f and f.get("slur") and ch.voice:
                # A slurred note takes over the sounding voices (and a layer's): new key and length, no key-on.
                for v in (ch.voice, ch.layer_voice):
                    if not v:
                        continue
                    if v.phase == RELEASE and not v.left:  # its key-off came on this very tick: the driver has none
                        v.phase = v.released_from
                    v.key, v.bend = f["note"], None
                    v.left = -1 if f["len"] is None else f["len"]
                v = ch.voice
                v.pitch = self.pitch_of(ch, f["note"])  # a layer voice follows the main voice's pitch
                # The driver skips only the key-on: the LFOs restart with the channel's current settings.
                v.vibrato, v.tremolo, v.pan_lfo = (self.start_lfo(s) for s in (ch.vibrato, ch.tremolo, ch.pan_lfo))
                if v.vibrato:
                    self.set_vibrato_amount(v.vibrato, v.pitch)
            elif "note" in f:
                self.key_on(ch, f["note"], f["len"])
            if "bend" in f and ch.voice:  # after the note: a glide (DA) bends the note it starts
                ticks, semitones = f["bend"]
                # Effect channels aim bends at the plain key's pitch: their path skips the detune.
                target = self.pitch_of(ch, ch.voice.key + semitones, detune="effect" not in self.song)
                ch.voice.bend = [ch.voice.pitch, target, ticks, 0]
                ch.voice.key += semitones

    def interrupt(self):
        for ch in self.channels:
            if ch.voice:
                for lfo in (ch.voice.vibrato, ch.voice.tremolo, ch.voice.pan_lfo):
                    if lfo and lfo.delay <= 0:
                        lfo.countdown -= 1
                        if lfo.countdown <= 0:
                            lfo.countdown = lfo.rate
                            lfo.value = lfo.steps[lfo.index]
                            lfo.index = lfo.index + 1 if lfo.index + 1 < len(lfo.steps) else lfo.loop
        self.accumulator += round(self.tempo * self.tempo_scale)
        while self.accumulator >= 0x10000:  # more than one tick per interrupt when the master tempo is fast
            self.accumulator -= 0x10000
            self.step_tick()

    @staticmethod
    def set_vibrato_amount(lfo: Lfo, pitch: float):
        """(d & 0x7F)/128 of the pitch with d's top bit set, else 15/256 of that."""
        d = lfo.depth
        lfo.amount = (d & 0x7F) * (pitch if d & 0x80 else pitch * 15 / 256) / 128

    @staticmethod
    def pan_offset(v: Voice) -> int:
        """The pan LFO's offset in pan units, as the driver computes it."""
        return (v.pan_lfo.depth >> 1) * v.pan_lfo.value >> 15 if v and v.pan_lfo else 0

    @staticmethod
    def vibrato_offset(v: Voice) -> int:
        if not v.vibrato or not v.vibrato.amount:
            return 0
        offset = math.floor(v.vibrato.amount * v.vibrato.value / 65536)
        return 2 * offset if offset > 0 else offset  # the driver doubles the upward half

    def envelope(self, v: Voice, n: int) -> np.ndarray:
        """The SPU envelope (psx-spx ADSR rules, 44.1 kHz) over the next n output samples, 0..1."""
        ar, dr, sl, sr, rr, a_mode, s_mode, r_mode = v.adsr
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
        # Envelope steps passed by each output sample, counting the part of a step carried over from last time. The
        # carry is a fraction of a step, so a new rate (B1 mid-note, or a key-off after a 2^20-sample sustain step)
        # doesn't cash in a long wait at the new rate's speed.
        per_sample = SPU_HZ / self.rate / cycles
        steps = np.floor(v.env_count + np.arange(1, n + 1) * per_sample)
        v.env_count = (v.env_count + n * per_sample) % 1
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

    def mix(self, out: np.ndarray, send: np.ndarray | None):
        """Add the voices to `out`, and those of channels with reverb on to `send` as well."""
        n = len(out)
        outputs = {}  # channel -> its main voice's output this stretch, for FM on the next channel
        pitches = {}  # channel -> its main voice's pitch, for D4 on the next channel
        noise = None
        if any(ch.noise for ch in self.channels):
            # The clock's low 2 bits make a step of 4..7 that counts down a timer of 0x20000 >> (high 4 bits) at
            # 44.1 kHz; each time it runs out the register shifts, at most once per sample.
            shift, step = self.noise_clock >> 2, 4 + (self.noise_clock & 3)
            per_frame = min(1.0, step / (0x20000 >> shift)) * SPU_HZ / self.rate
            at = self.noise_pos + np.arange(n) * per_frame
            noise = self.noise[at.astype(np.int64) % len(self.noise)]
            self.noise_pos = (self.noise_pos + n * per_frame) % len(self.noise)
        for c, ch in enumerate(self.channels):
            for v in (ch.voice, ch.tail, ch.layer_voice):
                if not v or v.phase == OFF:
                    continue
                env = self.envelope(v, n)
                source = v.follow or v
                # Key-off also takes the voice out of the driver's update mask, so through the release the SPU
                # keeps the pitch, volume and pan written last: bends, slides and LFOs stop there.
                released = v.phase == RELEASE
                if released and v.held_pitch is not None:
                    pitch = v.held_pitch
                else:
                    pitch = source.pitch + self.vibrato_offset(source)
                    if ch.pitch_follow and c - 1 in pitches:
                        pitch = pitches[c - 1] + self.vibrato_offset(source)
                    # The driver writes the pitch register masked to 14 bits, so a pitch past 0x3FFF wraps round.
                    pitch = v.held_pitch = int(max(0.0, pitch)) & 0x3FFF
                if v is ch.voice:
                    pitches[c] = self.pitch_registers[c] = pitch
                steps = np.full(n, pitch * 2 ** (self.transpose / 12))
                if ch.fm and c - 1 in outputs:  # SPU pitch modulation: step * (1 + previous voice's output)
                    steps *= np.maximum(0.0, 1 + outputs[c - 1])
                steps = np.minimum(steps, 0x4000) / 4096 * SPU_HZ / self.rate  # the SPU's step limit, after FM
                pcm, length = v.inst.pcm, len(v.inst.pcm)
                pos = v.pos + np.cumsum(steps) - steps
                index = pos.astype(np.int64)
                step = ((pos - index) * 256).astype(np.int64)  # the SPU interpolates in 256 steps per sample
                taps = index[:, None] + np.arange(-1, 3)  # the samples before, at, and the two after the position
                v.pos += float(steps.sum())
                if v.inst.loops:
                    span = length - v.inst.loop_start
                    taps = np.where(taps >= length, v.inst.loop_start + (taps - v.inst.loop_start) % span, taps)
                    if v.pos >= length:
                        v.pos = v.inst.loop_start + (v.pos - v.inst.loop_start) % span
                else:
                    if v.pos >= length:
                        v.phase = OFF
                # Before the start (the decoder's history begins at zero) and past a one-shot sample's end: silence.
                values = np.where((taps < 0) | (taps >= length), 0, pcm[np.clip(taps, 0, length - 1)])
                sample = (values * self.gauss[step]).sum(axis=1) if noise is None or not ch.noise else noise
                if v is ch.voice:
                    outputs[c] = sample * env
                if self.muted >> c & 1:
                    continue
                if released and v.held_mix:
                    gain, left, right = v.held_mix
                else:
                    # The driver's voice volume, A8 x A3 x master 127/128, as SPU volume (0x4000 = full scale).
                    volume = ch.volume
                    if ch.volume_follow and c - 1 in self.pitch_registers:  # D6: 2 x register x A3 / 128
                        volume = min(0x3FFF, self.pitch_registers[c - 1]) / 64
                    gain = volume * ch.expression * MASTER_VOLUME / 0x4000
                    if ch.layer_inst is not None:  # F4: the balance splits the note between main and layer voice
                        gain *= (ch.balance if v is ch.layer_voice else 127 - ch.balance) / 256
                    if v.tremolo:  # past depth 0x80 it swings the volume below zero (see the wrap below)
                        gain *= 1 + v.tremolo.depth / 128 * v.tremolo.value / 32768
                    # The driver's pan tables (stereo mode): linear, so each side is -6 dB at the centre. It indexes
                    # them with (pan + pan LFO) & 0xFF and other data follows entry 127, but no song gets past it.
                    pan = min(127, max(0, ch.pan + self.pan_offset(source)))
                    left, right = (32640 - 256 * pan) / 32768, (128 + 256 * pan) / 32768
                    # The SPU volume is 15-bit signed (0x4000 = full) and the driver writes it unclamped: past full
                    # scale (D6) it wraps round to a small negative volume, and below zero (a deep tremolo) the
                    # wave plays inverted rather than going silent.
                    left, right = [((gain * side * 0x4000 + 0x4000) % 0x8000 - 0x4000) / 0x4000
                                   for side in (left, right)]
                    gain = 1.0
                    v.held_mix = gain, left, right
                voiced = sample * env * gain
                out[:, 0] += voiced * left
                out[:, 1] += voiced * right
                if send is not None and ch.reverb:
                    send[:, 0] += voiced * left
                    send[:, 1] += voiced * right

    def render(self, frames: int) -> np.ndarray:
        """The next `frames` stereo samples, (frames, 2) float32."""
        while self.commands:
            self.commands.popleft()(self)
        out = np.zeros((frames, 2), np.float32)
        if not self.song or not self.playing:
            return out
        per_interrupt = self.rate / self.song["timer_hz"]
        reverb = self.reverb if self.reverb_on else None
        send = np.zeros((frames, 2)) if reverb else None
        done = 0
        while done < frames:
            if self.until_interrupt <= 0:
                self.until_interrupt += per_interrupt
                self.interrupt()
            n = min(frames - done, max(1, math.ceil(self.until_interrupt)))
            self.until_interrupt -= n
            self.mix(out[done:done + n], send[done:done + n] if reverb else None)
            done += n
        if reverb:
            out += reverb.process(send) * self.reverb_depth
        if self.ended and all(not ch.voice or ch.voice.phase == OFF for ch in self.channels) and \
                (not reverb or reverb.peak * abs(self.reverb_depth) < 1e-4):  # let the reverb ring out
            self.playing = False
        return np.clip(out, -1, 1, out=out)
