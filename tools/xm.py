"""Export AKAO songs as FastTracker 2 modules (.xm) that use the game's own instrument samples.

usage: python tools/xm.py <file|assets dir> <SOUND dir> <out dir>

Grid: one XM tick = 2 AKAO ticks, so the XM BPM equals the song's BPM, and speed 1 makes each row one XM tick:
24 rows per quarter note, fine enough to keep grace notes and fast runs apart.
Event positions are rounded to the grid, never lengths, so nothing drifts. The last channel carries tempo changes.
The song's endless loop becomes the XM restart point.
"""
import math
import struct
import sys
from pathlib import Path

import akao
from instruments import BANKS, decode_adpcm
from unpack import lzs

SPEED = 1          # XM ticks per row
AKAO_PER_XM = 2    # AKAO ticks per XM tick
ROWS = 256         # rows per pattern (the XM maximum; keeps long songs under 256 patterns)
KEY_OFF = 97
SPU_HZ = 44100
ENVELOPE_SECONDS = 10  # longest stretch of a phase we model; a phase that barely moves in that time is flat


def bpm(tempo: int) -> int:
    return max(32, min(255, round(akao.DRIVER_HZ * tempo / 65536 * 60 / akao.TICKS_PER_QUARTER)))


def load_bank_instruments(sound_dir: Path) -> dict:
    """slot -> (samples, loop_start or None, pitch for C at octave 6, ADSR bytes +0x08..+0x0F).

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
                pitch = struct.unpack_from("<I", table, i * 64 + 16)[0]
                slots[first_slot + i] = (pcm, loop, pitch, table[i * 64 + 8:i * 64 + 16])
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


def xm_instrument(name: str, pcm: list[int], loop: int | None, pitch_c6: int, adsr: bytes, octave_base: int,
                  bpm: int) -> bytes:
    # XM plays C-4 (note 49) at 8363 Hz. We write AKAO octave o as XM octave o - octave_base, and the game plays
    # C of octave 6 at 44100 * pitch / 0x1000 Hz, so shift the sample by the difference in semitones.
    semis = 12 * math.log2(44100 * pitch_c6 / 0x1000 / 8363) - 12 * (6 - octave_base - 4)
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


def level(volume: int, expression: int) -> int:
    """XM volume 0-64 from the driver's voice volume, A8 volume * A3 multiplier (both 0-127)."""
    return max(0, min(64, round(64 * volume * expression / (127 * 127))))


def song_to_xm(block: bytes, bank: dict, title: str) -> tuple[bytes, int]:
    """Return (XM file bytes, events dropped because two landed in one cell)."""
    channels = [akao.play_channel(block, start) for start in akao.channel_starts(block)]
    notes = [e[2] for events, _, _ in channels for e in events if e[1] == "note"]
    octaves = [k // 12 - 1 for k in notes] or [4]
    octave_base = max(0, max(octaves) - 7)  # XM has 8 octaves; shift the whole song down if it goes higher
    programs = sorted({e[2] for events, _, _ in channels for e in events if e[1] == "program" and e[2] in bank})
    inst_no = {slot: i + 1 for i, slot in enumerate(programs)}

    end_tick = max(end for *_, end in channels) + 1
    rows_total = -(-round(end_tick / AKAO_PER_XM) // SPEED)
    n_chan = len(channels) + 1
    cells = [[None] * n_chan for _ in range(rows_total)]  # [note, instrument, volume, effect, param]
    dropped = 0

    def put(tick: int, ch: int, note=0, inst=0, vol=0, fx=0, param=0):
        nonlocal dropped
        xt = round(tick / AKAO_PER_XM)
        row, delay = divmod(xt, SPEED)
        if row >= rows_total:
            return
        cell = cells[row][ch] or [0, 0, 0, 0, 0]
        if note and cell[0] and row + 1 < rows_total and not (cells[row + 1][ch] or [0])[0]:
            row, delay, cell = row + 1, 0, cells[row + 1][ch] or [0, 0, 0, 0, 0]  # 1-tick notes: nudge one row
        if note and delay and not fx:
            fx, param = 0x0E, 0xD0 | delay
        # Volume and pan are continuous controls: a later value in the same row replaces the earlier one.
        # Only a second note, or a different effect, has nowhere to go.
        if (note and cell[0]) or (fx and cell[3] and cell[3] != fx):
            dropped += 1
            return
        cells[row][ch] = [note or cell[0], inst or cell[1], vol or cell[2], fx or cell[3], param if fx else cell[4]]

    first_tempo = None
    for ch, (events, _, _) in enumerate(channels):
        inst, volume, expression = 0, 100, 120
        timeline = sorted(events, key=lambda e: e[0])
        for i, (tick, kind, *rest) in enumerate(timeline):
            if kind == "note":
                key, length = rest
                n = min(96, max(1, 12 * (key // 12 - 1 - octave_base) + key % 12 + 1))
                put(tick, ch, note=n, inst=inst, vol=0x10 + level(volume, expression))
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
            elif kind == "tempo" and rest[0]:
                first_tempo = first_tempo or bpm(rest[0])
                put(tick, n_chan - 1, fx=0x0F, param=bpm(rest[0]))

    # Patterns break at the loop start so the restart position can point at it.
    loop_ticks = [loop for _, loop, _ in channels if loop is not None]
    loop_row = round(min(loop_ticks) / AKAO_PER_XM) // SPEED if loop_ticks else 0
    bounds = list(range(0, loop_row, ROWS)) + list(range(loop_row, rows_total, ROWS))
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
    restart = bounds.index(loop_row) if loop_ticks else 0
    if len(patterns) > 256:
        raise ValueError(f"{title}: {len(patterns)} patterns, XM allows 256")

    header = (b"Extended Module: " + title.encode()[:20].ljust(20) + b"\x1a" + b"ff7_psx_recovery".ljust(20)
              + struct.pack("<HI", 0x0104, 276)
              + struct.pack("<8H", len(patterns), restart, n_chan, len(patterns), len(programs), 1, SPEED,
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
    if source.is_dir():
        found = list(akao.sequences(source))
    else:
        data = source.read_bytes()
        if len(data) > 4 and int.from_bytes(data[:4], "little") == len(data) - 4:
            data = lzs(data)
        found = [(source.name, block) for _, block in akao.blocks(data)]
    names, total_dropped = set(), 0
    for origin, block in found:
        name = f"song_{struct.unpack_from('<H', block, 4)[0]:03d}"
        while name in names:
            name += "b"
        names.add(name)
        xm, dropped = song_to_xm(block, bank, name)
        total_dropped += dropped
        (out / f"{name}.xm").write_bytes(xm)
    print(f"{len(names)} songs -> {out}; {total_dropped} notes or effects dropped where two landed in one cell")


if __name__ == "__main__":
    main()
