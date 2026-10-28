import struct
import sys
from pathlib import Path

from xm import KEY_OFF, adsr_envelope, song_to_xm


def walk(xm: bytes) -> dict:
    """Parse an XM back into its parts and require that they account for every byte of the file."""
    assert xm[:17] == b"Extended Module: " and xm[37] == 0x1A
    header_size = struct.unpack_from("<I", xm, 60)[0]
    song_len, restart, channels, n_pat, n_inst, flags, speed, bpm = struct.unpack_from("<8H", xm, 64)
    pos, rows, notes, effects = 60 + header_size, 0, [], []
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
            if "fx" in fields:
                effects.append((rows + cell_no // channels, cell_no % channels, fields["fx"], fields.get("param", 0)))
        assert i == size, "pattern data must hold exactly rows x channels cells"
        rows += n_rows
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
            "speed": speed, "bpm": bpm, "notes": notes, "effects": effects}


if __name__ == "__main__":
    if len(sys.argv) > 1:  # walk exported files: python tools/test_xm.py assets/xm/*.xm
        for path in sys.argv[1:]:
            walk(Path(path).read_bytes())
        print(f"{len(sys.argv) - 1} files parse cleanly")
        sys.exit()
    # One channel: tempo, instrument 0, C quarter, rest quarter, then an endless jump back to the start.
    seq = bytes([0xE8, 0x00, 0x60, 0xA1, 0x00, 0xA5, 0x04, 0x02, 0x91, 0xEE, 0xF7, 0xFF])  # EE -9: back to A1
    song = b"AKAO" + struct.pack("<HHH", 1, 6 + len(seq), 0) + bytes(6) + struct.pack("<IH", 1, 0) + seq
    bank = {0: ([0, 1000, -1000, 0] * 10, 8, 0x1000, bytes([0, 0x0F, 0x0F, 0x7F, 0x06, 1, 3, 3]))}
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
        xm, _ = song_to_xm(song, {0: ([0, 1000, -1000, 0] * 10, 8, 0x1000, steady)}, "test")
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
    assert effects_of(bend) == [(0x01, 8), (0x01, 8)]
    # Detune D8 -2 is 1 - 2/256 of the pitch, about -13.6 cents: fine portamento down 2/16 on the note's row.
    assert effects_of(bytes([0xA1, 0x00, 0xD8, 0xFE, 0x02, 0xA0])) == [(0x0E, 0x22)]
    # Song 7 channel 16 sets pan right before each bending note: the pan moves a row earlier so the bend's first
    # step keeps its cell. 4-tick rest (row 0), pan 0x30, note on row 1, bend steps on rows 1 and 2.
    pan_then_bend = bytes([0xA1, 0x00, 0xA5, 0x04, 0xA2, 0x04, 0x8F, 0xAA, 0x30,
                           0xA2, 0x02, 0x00, 0xA4, 0x04, 0x01, 0xA2, 0x0A, 0x86, 0xA0])
    assert effects_of(pan_then_bend, rows=True) == [(0, 0x08, 0x60), (1, 0x01, 8), (2, 0x01, 8)]
    print("ok")
