"""Export AKAO songs as FastTracker 2 modules (.xm) that use the game's own instrument samples.

usage: python tools/xm.py <file|assets dir> <SOUND dir> <out dir>

Grid: one XM tick = 2 AKAO ticks, so the XM BPM equals the song's BPM. Songs without pitch bends use speed 1
(rows of 2 AKAO ticks, 24 per quarter note). Portamento only runs on a row's later ticks, so songs with bends use
speed 2 (rows of 4 AKAO ticks; notes on the second XM tick get note delay ED1, so placement stays 2-tick exact).
Patterns hold whole bars of the song's time signatures (FD), as many as fit in two bars of 4/4, and one starts at
the loop. Event positions are rounded to the grid, never lengths, so nothing drifts. The last channel carries
tempo changes. The song's endless loop becomes the XM restart point.

Vibrato and tremolo are baked per row (pitch: extra-fine portamento X1x/X2x, or E1x/E2x for big steps; tremolo:
the volume column), because FT2's own 4xy can't go as shallow as the game's usual vibrato and 4xy/7xy don't run
at speed 1. The pan LFO (BC/BD) and the LFO depth slides (DD/DE) are left out.
"""
import bisect
import collections
import itertools
import math
import struct
import sys
from pathlib import Path

import akao
from instruments import BANKS, decode_adpcm

AKAO_PER_XM = 2    # AKAO ticks per XM tick
BAR_TICKS = 192    # AKAO ticks in a bar of 4/4; patterns hold two
PORTA_UNITS = 16   # XM linear-frequency portamento units per semitone (1xx moves xx units; E1x/E2x move x)
KEY_OFF = 97
SPU_HZ = 44100
ENVELOPE_SECONDS = 10  # longest stretch of a phase we model; a phase that barely moves in that time is flat


def bpm(tempo: int) -> int:
    return max(32, min(255, round(akao.DRIVER_HZ * tempo / 65536 * 60 / akao.TICKS_PER_QUARTER)))


def load_bank_instruments(sound_dir: Path) -> dict:
    """slot -> (samples, loop_start or None, pitches of C..B at octave 6, ADSR bytes +0x08..+0x0F).

    Bank 1 wins where both banks define a slot.
    """
    slots = {}
    for all_name, dat_name, first_slot, count in reversed(BANKS):
        samples_file = (sound_dir / all_name).read_bytes()
        table = (sound_dir / dat_name).read_bytes()
        base, length = struct.unpack_from("<II", samples_file)
        for i in range(count):
            sample_addr, loop_addr = struct.unpack_from("<II", table, i * 64)
            if base <= sample_addr < base + length:
                pcm, repeats = decode_adpcm(samples_file[16:16 + length], sample_addr - base)
                loop = (loop_addr - sample_addr) // 16 * 28 if repeats and loop_addr >= sample_addr else None
                pitches = struct.unpack_from("<12I", table, i * 64 + 16)
                slots[first_slot + i] = (pcm, loop, pitches, table[i * 64 + 8:i * 64 + 16])
    return slots


def spu_phase(level: int, target: int, shift: int, step: int, exponential: bool) -> list[tuple[float, int]]:
    """(seconds, level) after each SPU envelope step until the level passes target (psx-spx, SPU ADSR)."""
    increase, t, out = step > 0, 0.0, []
    while level < target if increase else level > target:
        cycles = 1 << max(0, shift - 11)
        if exponential and increase and level > 0x6000:
            cycles *= 4
        delta = step << max(0, 11 - shift)
        if exponential and not increase:
            delta = delta * level // 0x8000
        # The slowest rates (e.g. sustain 0x7F: one step every 23.8 s) hold the level in practice.
        if not delta or t + cycles / SPU_HZ > ENVELOPE_SECONDS:
            break
        level = max(0, min(0x7FFF, level + delta))
        t += cycles / SPU_HZ
        out.append((t, level))
    return out


def adsr_envelope(adsr: bytes, bpm: int) -> tuple[list[tuple[int, int]], int, int]:
    """XM volume envelope (points, sustain index, fadeout) from an instrument's SPU ADSR bytes.

    Byte order as the driver hands them to libspu: attack rate, decay rate, sustain level, sustain rate, release
    rate, then attack / sustain / release modes (1 linear up, 3 linear down, 5 exponential up, 7 exponential down).
    Attack and sustain rates are shift << 2 | step; decay and release are a shift with step -8.
    """
    ar, dr, sl, sr, rr, a_mode, s_mode, r_mode = adsr
    tick_s = 2.5 / bpm

    def pick(phase, start_t, start_x, count):
        if not phase:
            return []
        chosen = [phase[min(len(phase) - 1, round((i + 1) * len(phase) / count) - 1)] for i in range(count)]
        return [(start_x + round((start_t + t) / tick_s), round(level * 64 / 0x7FFF)) for t, level in chosen]

    attack = spu_phase(0, 0x7FFF, ar >> 2, 7 - (ar & 3), a_mode == 5)
    level, t = (attack[-1][1], attack[-1][0]) if attack else (0x7FFF, 0.0)
    decay = spu_phase(level, min(0x7FFF, (sl + 1) * 0x800), dr, -8, True)
    points = [(0, 0)] + pick(attack, 0, 0, 2) + pick(decay, t, 0, 2)
    t += decay[-1][0] if decay else 0
    level = decay[-1][1] if decay else level
    increase = s_mode in (1, 5)
    sustain = spu_phase(level, 0x7FFF if increase else 0, sr >> 2, 7 - (sr & 3) if increase else -8 + (sr & 3),
                        s_mode in (5, 7))
    # Slow enough to change by less than one XM volume step over ENVELOPE_SECONDS: the held level stays put.
    if sustain and abs(sustain[-1][1] - level) * 64 < 0x7FFF:
        sustain = []
    release = spu_phase(0x7FFF, 0, rr, -8, r_mode == 7)
    release_ticks = max(1.0, (release[-1][0] if release else 0) / tick_s)
    if sustain:
        # Fades (or swells) while held: put that curve before the sustain point; release becomes the fadeout.
        points += pick(sustain, t, 0, 5)
        fadeout = min(0xFFF, math.ceil(65536 / release_ticks))
    else:
        fadeout = 0
    # Keep the last point per tick; XM needs strictly increasing x.
    by_x = dict(points)
    points = sorted(by_x.items())
    sustain_index = len(points) - 1
    if not sustain:
        # Held level stays put; after key-off the envelope runs on into the release curve.
        start_x, start_y = points[-1]
        release_from = spu_phase(start_y * 0x7FFF // 64, 0, rr, -8, r_mode == 7)
        # A release shorter than a tick collapses into one step to 0 on the next tick.
        tail = {max(x, start_x + 1): y for x, y in pick(release_from, 0, start_x, 3)}
        points += sorted(tail.items()) or [(start_x + 1, 0)]
        if points[-1][1]:
            points.append((points[-1][0] + 1, 0))
    return points[:12], min(sustain_index, 11), fadeout


def xm_instrument(name: str, pcm: list[int], loop: int | None, pitches: tuple, adsr: bytes, octave_base: int,
                  bpm: int) -> bytes:
    # XM plays C-4 (note 49) at 8363 Hz. We write AKAO octave o as XM octave o - octave_base, and the game plays
    # C of octave 6 at 44100 * pitch / 0x1000 Hz, so shift the sample by the difference in semitones.
    semis = 12 * math.log2(44100 * pitches[0] / 0x1000 / 8363) - 12 * (6 - octave_base - 4)
    relative = math.floor(semis)
    finetune = min(127, round((semis - relative) * 128))
    deltas, prev = bytearray(), 0
    for s in pcm:
        deltas += struct.pack("<h", (s - prev + 0x8000) % 0x10000 - 0x8000)
        prev = s
    loop_start, loop_len, kind = (loop * 2, (len(pcm) - loop) * 2, 0x11) if loop is not None else (0, 0, 0x10)
    sample_header = struct.pack("<IIIBbBBbB22s", len(pcm) * 2, loop_start, loop_len, 64, finetune, kind, 128,
                                relative, 0, name.encode()[:22])
    points, sustain, fadeout = adsr_envelope(adsr, bpm)
    env = b"".join(struct.pack("<2H", min(x, 0xFFFF), y) for x, y in points).ljust(48, b"\0")
    header = (struct.pack("<I22sBH", 263, name.encode()[:22], 0, 1) + struct.pack("<I", 40) + bytes(96)
              + env + bytes(48) + bytes([len(points), 0, sustain, 0, 0, 0, 0, 0]) + bytes([0b11, 0]) + bytes(4)
              + struct.pack("<H", fadeout) + bytes(22))
    assert len(header) == 263
    return header + sample_header + bytes(deltas)


def detune_semitones(detune: int) -> float:
    """The driver scales a note's pitch by 1 + d/128 for d > 0 and 1 + d/256 for d < 0."""
    return 12 * math.log2(1 + detune / (128 if detune > 0 else 256))


def bend_semitones(semitones: int, elapsed: int, length: int) -> float:
    """Pitch offset `elapsed` ticks into a bend: the driver slides linearly in frequency, not in semitones."""
    return 12 * math.log2(1 + (2 ** (semitones / 12) - 1) * min(elapsed, length) / length)


def lfo_wave(n: int) -> tuple[list[int], int] | None:
    """The driver's LFO waveform n as (steps, index the loop goes back to), rebuilt from its shape; None for 12.

    Even numbers (and 14) fade in over four cycles at 1/4, 2/4, 3/4 and full scale, then repeat the last cycle;
    odd ones start at full scale. 12 is a table of noise, 13-15 reuse the triangles.
    """
    n = {13: 9, 14: 8, 15: 9}.get(n, n)
    if n > 11:
        return None
    cycle = {
        0: [1, -1], 2: [1, 0], 4: [-1, 0],
        6: [math.sin(math.pi * i / 20) for i in range(40) if i != 3],  # the game's table skips this step
        8: [(4 - abs(i - 4)) / 4 if i < 8 else (abs(i - 12) - 4) / 4 for i in range(16)],
        10: [math.sin(math.pi * i / 10) for i in range(20)],
    }[n & ~1]
    scales = (8191, 16383, 24575, 32767) if n % 2 == 0 else (32767,)
    steps = [round(scale * v) for scale in scales for v in cycle]
    return steps, len(steps) - len(cycle)


def lfo_total(prefix: list[float], loop: int, rate: int, timer_steps: int) -> float:
    """An LFO summed over its first `timer_steps` interrupts. It moves to its next step every `rate` interrupts and
    goes back to step `loop` after the last; `prefix` holds the running sums of its steps."""
    q, rem = divmod(timer_steps, rate)
    loops, part = divmod(max(0, q - loop), len(prefix) - 1 - loop)
    held = min(q, loop) + part
    return rate * (prefix[held] + loops * (prefix[-1] - prefix[loop])) + rem * (prefix[held + 1] - prefix[held])


def vibrato_semitones(depth: int, step: int) -> float:
    """Pitch offset at an LFO step: the driver adds pitch * d * step / 2^16, doubled when positive, where d is
    (B5 & 0x7F) / 128, times 15/256 unless B5's top bit is set."""
    ratio = (depth & 0x7F) / 128 * (1 if depth & 0x80 else 15 / 256) * step / 65536
    return 12 * math.log2(1 + (2 * ratio if ratio > 0 else ratio))


def level(volume: int, expression: int) -> int:
    """XM volume 0-64 from the driver's voice volume, A8 volume * A3 multiplier (both 0-127)."""
    return max(0, min(64, round(64 * volume * expression / (127 * 127))))


def song_to_xm(block: bytes, bank: dict, title: str) -> tuple[bytes, collections.Counter]:
    """Return (XM file bytes, what was dropped because two things landed in one cell)."""
    channels, loop_start, loop_end, _, _ = akao.play_song(block)
    notes = [e[2] for events in channels for e in events if e[1] == "note"]
    octaves = [k // 12 - 1 for k in notes] or [4]
    octave_base = max(0, max(octaves) - 7)  # XM has 8 octaves; shift the whole song down if it goes higher
    programs = sorted({e[2] for events in channels for e in events if e[1] == "program" and e[2] in bank})
    inst_no = {slot: i + 1 for i, slot in enumerate(programs)}

    end_tick = loop_end
    # Speed 2 only where portamento is needed: a 4-tick row can't hold two notes 2 ticks apart.
    speed = 2 if any(e[1] == "bend" for events in channels for e in events) else 1
    row_ticks = speed * AKAO_PER_XM
    rows_total = -(-round(end_tick / AKAO_PER_XM) // speed)
    n_chan = len(channels) + 1
    cells = [[None] * n_chan for _ in range(rows_total)]  # [note, instrument, volume, effect, param]
    dropped = collections.Counter()

    def put(tick: int, ch: int, note=0, inst=0, vol=0, fx=0, param=0):
        xt = round(tick / AKAO_PER_XM)
        row, delay = divmod(xt, speed)
        if row >= rows_total:
            return
        cell = cells[row][ch] or [0, 0, 0, 0, 0]
        if note and cell[0] and row + 1 < rows_total and not (cells[row + 1][ch] or [0])[0]:
            row, delay, cell = row + 1, 0, cells[row + 1][ch] or [0, 0, 0, 0, 0]  # 1-tick notes: nudge one row
        if note and delay and not fx:
            fx, param = 0x0E, 0xD0 | delay
        # Volume and pan are continuous controls: a later value in the same row replaces the earlier one.
        # Only a second note, or a different effect, has nowhere to go.
        if note and cell[0]:
            dropped["note"] += 1
            return
        if fx and cell[3] and cell[3] != fx:
            # A pan change can happen a row early (it only affects the previous note's tail); anything else can't.
            if 0x08 in (fx, cell[3]) and row and (cells[row - 1][ch] or [0, 0, 0, 0, 0])[3] in (0, 0x08):
                pan = param if fx == 0x08 else cell[4]
                earlier = cells[row - 1][ch] or [0, 0, 0, 0, 0]
                cells[row - 1][ch] = earlier[:3] + [0x08, pan]
                if fx == 0x08:
                    return
                cell[3] = 0
            else:
                dropped[f"effect {fx:X} (cell has {cell[3]:X})"] += 1
                return
        cells[row][ch] = [note or cell[0], inst or cell[1], vol or cell[2], fx or cell[3], param if fx else cell[4]]

    tempos = sorted((e[0], e[2]) for events in channels for e in events if e[1] == "tempo" and e[2])

    def timer_steps_per_tick(tick: int) -> float:
        tempo = next((t for at, t in reversed(tempos) if at <= tick), tempos[0][1] if tempos else 0)
        return 65536 / tempo if tempo else akao.DRIVER_HZ * 60 / (120 * akao.TICKS_PER_QUARTER)

    first_tempo = None
    for ch, events in enumerate(channels):
        inst, volume, expression, detune = 0, 100, 120, 0
        timeline = sorted(events, key=lambda e: e[0])
        for i, (tick, kind, *rest) in enumerate(timeline):
            if kind == "note":
                key, length = rest
                offset = detune_semitones(detune)
                key += round(offset)
                n = min(96, max(1, 12 * (key // 12 - 1 - octave_base) + key % 12 + 1))
                put(tick, ch, note=n, inst=inst, vol=0x10 + level(volume, expression))
                fine = round((offset - round(offset)) * PORTA_UNITS)
                if fine:
                    # Fine portamento acts on the row's first XM tick, right after the note; a delayed note (ED1)
                    # already uses the effect column, so its detune is left out.
                    row, delay = divmod(round(tick / AKAO_PER_XM), speed)
                    if delay:
                        dropped["detune on a delayed note"] += 1
                    else:
                        put(tick, ch, fx=0x0E, param=(0x10 if fine > 0 else 0x20) | min(15, abs(fine)))
                # Release only if the next note doesn't start right away (a new note retriggers anyway).
                nxt = next((e[0] for e in timeline[i + 1:] if e[1] == "note"), None)
                if nxt is None or nxt > tick + length:
                    put(tick + length, ch, note=KEY_OFF)
            elif kind == "program":
                inst = inst_no.get(rest[0], 0)
            elif kind in ("volume", "expression"):
                volume, expression = (rest[0], expression) if kind == "volume" else (volume, rest[0])
                put(tick, ch, vol=0x10 + level(volume, expression))
            elif kind == "pan":
                put(tick, ch, fx=0x08, param=min(255, rest[0] * 2))
            elif kind == "detune":
                detune = rest[0]
            elif kind == "bend":
                length, semitones = rest
                # Portamento acts on each row's second XM tick (2 AKAO ticks in). Step to where the driver's slide
                # is by the end of that tick, until the bend ends or the next note retriggers the pitch.
                stop = next((e[0] for e in timeline[i + 1:] if e[1] in ("note", "bend")), end_tick)
                total, done = round(semitones * PORTA_UNITS), 0
                row = -(-(tick - AKAO_PER_XM) // row_ticks)
                while done != total and row * row_ticks + AKAO_PER_XM < stop:
                    second_tick = row * row_ticks + AKAO_PER_XM
                    target = round(bend_semitones(semitones, second_tick + AKAO_PER_XM - tick, length) * PORTA_UNITS)
                    step = max(-255, min(255, target - done))
                    if step:
                        put(row * row_ticks, ch, fx=0x01 if step > 0 else 0x02, param=abs(step))
                        done += step
                    row += 1
            elif kind == "tempo" and rest[0]:
                first_tempo = first_tempo or bpm(rest[0])
                put(tick, n_chan - 1, fx=0x0F, param=bpm(rest[0]))

        # LFOs go last, into cells nothing else wanted: a pitch step whose effect column is taken is made up on the
        # next free row. The driver steps its LFOs on every timer interrupt and restarts them with each note; a row
        # gets the average over the interrupts it spans, so an LFO faster than the rows (B8 00 03 05 runs at
        # 40 Hz) comes out as its mean level rather than an aliased wobble.
        control = {k: ([e[0] for e in timeline if e[1] == k], [e[2] for e in timeline if e[1] == k])
                   for k in ("volume", "expression")}
        spans = [(e[0], e[3]) for e in timeline if e[1] == "note"]
        for kind in ("tremolo", "vibrato"):
            states = [e for e in timeline if e[1] == kind]
            for n, (tick, length) in enumerate(spans):
                state = next((s for s in reversed(states) if s[0] <= tick), None)
                if not state or not state[5]:
                    continue
                delay, rate, wave_no, depth = state[2:]
                wave = lfo_wave(wave_no)
                if not wave:
                    dropped[f"{kind} on the noise waveform"] += 1
                    continue
                steps, loop = wave
                if kind == "vibrato":
                    shape = [vibrato_semitones(depth, v) * 64 for v in steps]
                else:
                    shape = [max(0.0, 1 + depth / 128 * v / 32768) for v in steps]
                prefix = list(itertools.accumulate(shape, initial=0.0))
                start = tick + delay
                stop = min(tick + length, spans[n + 1][0] if n + 1 < len(spans) else end_tick)
                per_tick = timer_steps_per_tick(start)
                done, previous = 0, None
                for row in range(start // row_ticks, min(rows_total, -(-stop // row_ticks))):
                    a, b = max(row * row_ticks, start), min((row + 1) * row_ticks, stop)
                    first, last = round((a - start) * per_tick), round((b - start) * per_tick)
                    if last <= first:
                        continue
                    mean = (lfo_total(prefix, loop, rate, last) - lfo_total(prefix, loop, rate, first)) / (last - first)
                    cell = cells[row][ch] or [0, 0, 0, 0, 0]
                    if kind == "tremolo":
                        ticks_v, vols = control["volume"]
                        ticks_e, exprs = control["expression"]
                        at = row * row_ticks
                        base = level(vols[bisect.bisect_right(ticks_v, at) - 1],
                                     exprs[bisect.bisect_right(ticks_e, at) - 1])
                        vol = 0x10 + max(0, min(64, round(base * mean)))
                        # Write on change, and over any volume another event put in this row.
                        if vol != cell[2] and (cell[2] or vol != previous):
                            put(at, ch, vol=vol)
                        previous = vol
                        continue
                    step = round(mean) - done
                    if not step or cell[3]:
                        continue
                    if abs(step) > 15:  # past extra-fine range: fine portamento moves 4 units per step
                        step = max(-15, min(15, round(step / 4))) * 4
                        put(row * row_ticks, ch, fx=0x0E, param=(0x10 if step > 0 else 0x20) | abs(step) // 4)
                    else:
                        put(row * row_ticks, ch, fx=0x21, param=(0x10 if step > 0 else 0x20) | abs(step))
                    done += step

    # Patterns hold whole bars and break at the loop start so the restart position can point at it. Songs without
    # a time signature count 4/4 bars from the loop.
    meters = [(e[0], e[2] * e[3]) for events in channels for e in events if e[1] == "meter"]
    starts = set(akao.bar_lines(meters or [(loop_start or 0, BAR_TICKS)], end_tick)) | {loop_start or 0}
    tick_bounds = [0]
    for bar, nxt in zip(sorted(starts), sorted(starts)[1:] + [end_tick]):
        if bar and (bar == loop_start or nxt - tick_bounds[-1] > 2 * BAR_TICKS):
            tick_bounds.append(bar)
    bounds = sorted({round(t / AKAO_PER_XM) // speed for t in tick_bounds} - {rows_total} | {0})
    loop_row = round(loop_start / AKAO_PER_XM) // speed if loop_start is not None else 0
    patterns = []
    for start, stop in zip(bounds, bounds[1:] + [rows_total]):
        data = bytearray()
        for row in cells[start:stop]:
            for cell in row:
                if not cell:
                    data.append(0x80)
                    continue
                flags = sum(1 << i for i, v in enumerate(cell) if v)
                data.append(0x80 | flags)
                data += bytes(v for v in cell if v)
        patterns.append(struct.pack("<IBHH", 9, 0, stop - start, len(data)) + data)
    restart = bounds.index(loop_row) if loop_start is not None else 0
    if len(patterns) > 256 or max(b - a for a, b in zip(bounds, bounds[1:] + [rows_total])) > 256:
        raise ValueError(f"{title}: XM allows 256 patterns of up to 256 rows")

    header = (b"Extended Module: " + title.encode()[:20].ljust(20) + b"\x1a" + b"ff7_psx_recovery".ljust(20)
              + struct.pack("<HI", 0x0104, 276)
              + struct.pack("<8H", len(patterns), restart, n_chan, len(patterns), len(programs), 1, speed,
                            first_tempo or 120)
              + bytes(range(len(patterns))).ljust(256, b"\0"))
    instruments = b"".join(xm_instrument(f"akao instrument {slot}", *bank[slot], octave_base, first_tempo or 120)
                           for slot in programs)
    return header + b"".join(patterns) + instruments, dropped


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return
    source, sound_dir, out = map(Path, sys.argv[1:])
    out.mkdir(parents=True, exist_ok=True)
    bank = load_bank_instruments(sound_dir)
    songs, total_dropped = akao.named_songs(source), collections.Counter()
    for name, _, block in songs:
        xm, dropped = song_to_xm(block, bank, name)
        total_dropped += dropped
        (out / f"{name}.xm").write_bytes(xm)
    print(f"{len(songs)} songs -> {out}; dropped where two landed in one cell: {dict(total_dropped) or 'nothing'}")


if __name__ == "__main__":
    main()
