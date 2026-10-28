import struct
import sys
from pathlib import Path

from xm import KEY_OFF, adsr_envelope, song_to_xm


def walk(xm: bytes) -> dict:
    """Parse an XM back into its parts and require that they account for every byte of the file."""
    assert xm[:17] == b"Extended Module: " and xm[37] == 0x1A
    header_size = struct.unpack_from("<I", xm, 60)[0]
    song_len, restart, channels, n_pat, n_inst, flags, speed, bpm = struct.unpack_from("<8H", xm, 64)
    pos, rows, notes, effects, volumes, pattern_rows = 60 + header_size, 0, [], [], [], []
    for _ in range(n_pat):
        plen, _, n_rows, size = struct.unpack_from("<IBHH", xm, pos)
        data, pos, i = xm[pos + plen:pos + plen + size], pos + plen + size, 0
        for cell_no in range(n_rows * channels):
            flags_byte = data[i]
            i += 1
            assert flags_byte & 0x80, "writer always packs"
            fields = {}
            for bit, name in enumerate(("note", "inst", "vol", "fx", "param")):
                if flags_byte >> bit & 1:
                    fields[name] = data[i]
                    i += 1
            if "note" in fields:
                notes.append(fields["note"])
            if "vol" in fields:
                volumes.append((rows + cell_no // channels, cell_no % channels, fields["vol"]))
            if "fx" in fields:
                effects.append((rows + cell_no // channels, cell_no % channels, fields["fx"], fields.get("param", 0)))
        assert i == size, "pattern data must hold exactly rows x channels cells"
        rows += n_rows
        pattern_rows.append(n_rows)
    for _ in range(n_inst):
        isize, n_samples = struct.unpack_from("<I", xm, pos)[0], struct.unpack_from("<H", xm, pos + 27)[0]
        shdr = struct.unpack_from("<I", xm, pos + 29)[0]
        pos += isize
        lengths = []
        for _ in range(n_samples):
            length, loop_start, loop_len = struct.unpack_from("<III", xm, pos)
            assert loop_start + loop_len <= length
            lengths.append(length)
            pos += shdr
        pos += sum(lengths)
    assert pos == len(xm), f"{len(xm) - pos} bytes unaccounted for"
    return {"channels": channels, "patterns": n_pat, "restart": restart, "instruments": n_inst, "rows": rows,
            "speed": speed, "bpm": bpm, "notes": notes, "effects": effects,
            "volumes": volumes, "pattern_rows": pattern_rows}


if __name__ == "__main__":
    if len(sys.argv) > 1:  # walk exported files: python tools/test_xm.py assets/xm/*.xm
        for path in sys.argv[1:]:
            walk(Path(path).read_bytes())
        print(f"{len(sys.argv) - 1} files parse cleanly")
        sys.exit()
    # One channel: tempo, instrument 0, C quarter, rest quarter, then an endless jump back to the start.
    seq = bytes([0xE8, 0x00, 0x60, 0xA1, 0x00, 0xA5, 0x04, 0x02, 0x91, 0xEE, 0xF7, 0xFF])  # EE -9: back to A1
    song = b"AKAO" + struct.pack("<HHH", 1, 6 + len(seq), 0) + bytes(6) + struct.pack("<IH", 1, 0) + seq
    bank = {0: ([0, 1000, -1000, 0] * 10, 8, (0x1000,) * 12, bytes([0, 0x0F, 0x0F, 0x7F, 0x06, 1, 3, 3]))}
    xm, dropped = song_to_xm(song, bank, "test")
    info = walk(xm)
    assert not dropped
    assert (info["channels"], info["instruments"], info["speed"]) == (2, 1, 1)  # no bends: 2-tick rows
    # 0x6000 tempo = 114 BPM; C of AKAO octave 4 is XM note 49 (C-4); the rest gets a key-off.
    assert info["bpm"] == 114 and info["notes"] == [49, KEY_OFF]
    assert info["rows"] == 48 and info["restart"] == 0  # 96 AKAO ticks = 48 rows of 2; loop back to the start

    # Envelopes from the instrument's SPU ADSR bytes (+0x08: AR DR SL SR RR, +0x0D modes A S R).
    steady = bytes([0x00, 0x0F, 0x0F, 0x7F, 0x06, 0x01, 0x03, 0x03])  # instant attack, sustain held, quick release
    points, sustain, fadeout = adsr_envelope(steady, bpm=120)
    assert points[sustain][1] == 64 and points[-1][1] == 0 and sustain < len(points) - 1
    assert points[-1][0] - points[sustain][0] < 30  # release rate 6 is well under a second
    fading = bytes([0x00, 0x0F, 0x0F, 0x40, 0x05, 0x01, 0x03, 0x03])  # sustain rate 0x40: fades while held
    points, sustain, fadeout = adsr_envelope(fading, bpm=120)
    assert sustain == len(points) - 1 and points[-1][1] < points[0][1] and fadeout > 0
    assert all(a[1] >= b[1] for a, b in zip(points, points[1:])) and len(points) <= 12

    # A3 scales the channel volume: A8 127 with A3 64 plays at half the volume of A3 127.
    def note_volume(a3):
        seq = bytes([0xA1, 0x00, 0xA8, 0x7F, 0xA3, a3, 0x02, 0xA0])
        song = b"AKAO" + struct.pack("<HHH", 1, 6 + len(seq), 0) + bytes(6) + struct.pack("<IH", 1, 0) + seq
        xm, _ = song_to_xm(song, {0: ([0, 1000, -1000, 0] * 10, 8, (0x1000,) * 12, steady)}, "test")
        cell = xm.index(bytes([0x80 | 0b111, 25]))  # first note cell: C-2 (driver default octave 2), instr, volume
        return xm[cell + 3] - 0x10
    assert note_volume(127) == 64 and note_volume(64) == 32

    # Channels that start looping at different ticks: the XM repeats from the later loop start (tick 48, row 24)
    # for one common loop length (96 ticks), not from the earliest start to the latest end.
    ch0 = bytes([0x01, 0xEE, 0xFC, 0xFF])
    ch1 = bytes([0x02, 0x0C, 0xEE, 0xFC, 0xFF])
    table = struct.pack("<IHH", 0b11, 2, len(ch0))
    two = b"AKAO" + struct.pack("<HHH", 9, 8 + len(ch0) + len(ch1), 0) + bytes(6) + table + ch0 + ch1
    xm, _ = song_to_xm(two, bank, "test")
    info = walk(xm)
    assert info["patterns"] == 2 and info["restart"] == 1 and info["rows"] == 72

    # Bend: 2-tick note, A4 04 01, tie. Portamento up runs on each row's second XM tick (2 AKAO ticks in), following
    # the driver's slide (linear in frequency): half way after 2 ticks (~8/16 semitone), all of it after 4.
    def effects_of(seq, rows=False):
        song = b"AKAO" + struct.pack("<HHH", 1, 6 + len(seq), 0) + bytes(6) + struct.pack("<IH", 1, 0) + seq
        xm, _ = song_to_xm(song, bank, "test")
        return [(e[0], *e[2:]) if rows else e[2:] for e in walk(xm)["effects"] if e[1] == 0]
    bend = bytes([0xA1, 0x00, 0xA5, 0x04, 0xA2, 0x02, 0x00, 0xA4, 0x04, 0x01, 0xA2, 0x0A, 0x86, 0xA0])
    # The tie keys off 2 ticks before the end (tick 10): row 2, delayed one XM tick.
    assert effects_of(bend) == [(0x01, 8), (0x01, 8), (0x0E, 0xD1)]
    # Detune D8 -2 is 1 - 2/256 of the pitch, about -13.6 cents: fine portamento down 2/16 on the note's row.
    assert effects_of(bytes([0xA1, 0x00, 0xD8, 0xFE, 0x02, 0xA0])) == [(0x0E, 0x22)]
    # Song 7 channel 16 sets pan right before each bending note: the pan moves a row earlier so the bend's first
    # step keeps its cell. 4-tick rest (row 0), pan 0x30, note on row 1, bend steps on rows 1 and 2.
    pan_then_bend = bytes([0xA1, 0x00, 0xA5, 0x04, 0xA2, 0x04, 0x8F, 0xAA, 0x30,
                           0xA2, 0x02, 0x00, 0xA4, 0x04, 0x01, 0xA2, 0x0A, 0x86, 0xA0])
    assert effects_of(pan_then_bend, rows=True) == [(0, 0x08, 0x60), (1, 0x01, 8), (2, 0x01, 8), (3, 0x0E, 0xD1)]

    # LFOs are baked per row: FT2's vibrato can't go as shallow as the game's (its least depth is 1/8 semitone)
    # and neither vibrato nor tremolo runs at speed 1. Tempo 0x6000: 91.4 ticks/s, 45.7 rows/s.
    def lfo_song(lfo):  # whole note C at full volume
        return bytes([0xE8, 0x00, 0x60, 0xA1, 0x00, 0xA8, 0x7F, 0xA3, 0x7F]) + lfo + bytes([0xA5, 0x04, 0x00, 0xA0])

    def pitch_curve(seq):  # row -> pitch offset in semitones from X1x/X2x (1/64) and E1x/E2x (1/16) steps
        offset, curve = 0, {}
        for row, fx, param in effects_of(seq, rows=True):
            if fx in (0x21, 0x0E) and param >> 4 in (1, 2):
                offset += (param & 15) * (4 if fx == 0x0E else 1) * (1 if param >> 4 == 1 else -1)
                curve[row] = offset / 64
        return curve

    # Full-scale sine (waveform 7), rate 1, depth 7F: the driver adds up to 15/256 * 127/128 * 1/2 of the pitch,
    # doubled going up: +0.98 / -0.51 semitone, one cycle per 39 timer steps (6.25 Hz), 13 cycles in a whole note.
    curve = pitch_curve(lfo_song(bytes([0xB4, 0x00, 0x01, 0x07, 0xB5, 0x7F])))
    assert 0.85 < max(curve.values()) < 1.0 and -0.55 < min(curve.values()) < -0.4
    values = [curve[row] for row in sorted(curve)]
    assert 12 <= sum(a <= 0 < b for a, b in zip(values, values[1:])) <= 14
    # The game's usual vibrato, B4 60 01 06 + B5 0A: starts a half note (48 rows) in, fades in, about 8 cents up.
    curve = pitch_curve(lfo_song(bytes([0xB4, 0x60, 0x01, 0x06, 0xB5, 0x0A])))
    assert curve and min(curve) >= 48 and max(abs(v) for v in curve.values()) <= 0.14

    def volume_curve(seq):
        song = b"AKAO" + struct.pack("<HHH", 1, 6 + len(seq), 0) + bytes(6) + struct.pack("<IH", 1, 0) + seq
        return [v - 0x10 for row, ch, v in walk(song_to_xm(song, bank, "test")[0])["volumes"] if ch == 0]
    # Tremolo B9 7F on a full-scale sine swings the volume between about zero and full.
    levels = volume_curve(lfo_song(bytes([0xB8, 0x00, 0x01, 0x07, 0xB9, 0x7F])))
    assert min(levels) < 10 and max(levels) == 64
    # B8 00 03 05 + B9 78 cuts the volume by 94% every other 3 timer steps (40 Hz), faster than the rows: each row
    # gets the average, about half volume, rather than an aliased wobble.
    levels = volume_curve(lfo_song(bytes([0xB8, 0x00, 0x03, 0x05, 0xB9, 0x78])))
    assert all(25 <= v <= 42 for v in levels[1:]), levels

    # Patterns follow the bars that FD time signatures set, like song 76's alternating 6/8 and 5/8: a 1/4 pickup
    # bar, then a loop of two 6/8 (144 ticks) + 5/8 (120 ticks) pairs. The pickup is cut off as its own pattern
    # because the loop starts after it; then each pattern holds a pair, which fits in two bars of 4/4 (384 ticks).
    pair = bytes([0xFD, 0x18, 0x06, 0xA2, 0x90, 0x00,             # 6/8, note C for 144 ticks
                  0xFD, 0x18, 0x05, 0xA2, 0x78, 0x00])            # 5/8, note C for 120 ticks
    seq = bytes([0xFD, 0x30, 0x01, 0x91]) + pair + pair + bytes([0xEE, 0xE5, 0xFF])  # 1/4 rest; EE -27: to pair 1
    song = b"AKAO" + struct.pack("<HHH", 1, 6 + len(seq), 0) + bytes(6) + struct.pack("<IH", 1, 0) + seq
    info = walk(song_to_xm(song, bank, "test")[0])
    assert info["pattern_rows"] == [24, 132, 132] and info["restart"] == 1, info["pattern_rows"]
    print("ok")
