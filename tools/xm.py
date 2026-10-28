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
# ponytail: one fixed release for every instrument; decode the AKAO ADSR bytes into envelopes for a closer sound.
FADEOUT = 2048


def bpm(tempo: int) -> int:
    return max(32, min(255, round(akao.DRIVER_HZ * tempo / 65536 * 60 / akao.TICKS_PER_QUARTER)))


def load_bank_instruments(sound_dir: Path) -> dict:
    """slot -> (samples, loop_start or None, pitch for C at octave 6). Bank 1 wins where both banks define a slot."""
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
                slots[first_slot + i] = (pcm, loop, struct.unpack_from("<I", table, i * 64 + 16)[0])
    return slots


def xm_instrument(name: str, pcm: list[int], loop: int | None, pitch_c6: int, octave_base: int) -> bytes:
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
    # Volume envelope: hold full volume while the key is down (sustain point 0), then FADEOUT after key-off.
    env = struct.pack("<4H", 0, 64, 1, 64) + bytes(40)
    header = (struct.pack("<I22sBH", 263, name.encode()[:22], 0, 1) + struct.pack("<I", 40) + bytes(96)
              + env + bytes(48) + bytes([2, 0, 0, 0, 0, 0, 0, 0]) + bytes([0b11, 0]) + bytes(4)
              + struct.pack("<H", FADEOUT) + bytes(22))
    assert len(header) == 263
    return header + sample_header + bytes(deltas)


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
        if (note and cell[0]) or (fx and cell[3] and (cell[3], cell[4]) != (fx, param)):
            dropped += 1
            return
        cells[row][ch] = [note or cell[0], inst or cell[1], vol or cell[2], fx or cell[3], param if fx else cell[4]]

    first_tempo = None
    for ch, (events, _, _) in enumerate(channels):
        inst, volume, playing_until = 0, 64, None
        timeline = sorted(events, key=lambda e: e[0])
        for i, (tick, kind, *rest) in enumerate(timeline):
            if kind == "note":
                key, length = rest
                n = min(96, max(1, 12 * (key // 12 - 1 - octave_base) + key % 12 + 1))
                put(tick, ch, note=n, inst=inst, vol=0x10 + volume)
                # Release only if the next note doesn't start right away (a new note retriggers anyway).
                nxt = next((e[0] for e in timeline[i + 1:] if e[1] == "note"), None)
                if nxt is None or nxt > tick + length:
                    put(tick + length, ch, note=KEY_OFF)
            elif kind == "program":
                inst = inst_no.get(rest[0], 0)
            elif kind == "volume":
                volume = rest[0] * 64 // 127
                put(tick, ch, vol=0x10 + volume)
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
    instruments = b"".join(xm_instrument(f"akao instrument {slot}", *bank[slot], octave_base) for slot in programs)
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
    print(f"{len(names)} songs -> {out}; {total_dropped} events dropped where two landed in one cell")


if __name__ == "__main__":
    main()
