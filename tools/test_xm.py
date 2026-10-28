import struct
import sys
from pathlib import Path

from xm import KEY_OFF, adsr_envelope, song_to_xm


def walk(xm: bytes) -> dict:
    """Parse an XM back into its parts and require that they account for every byte of the file."""
    assert xm[:17] == b"Extended Module: " and xm[37] == 0x1A
    header_size = struct.unpack_from("<I", xm, 60)[0]
    song_len, restart, channels, n_pat, n_inst, flags, speed, bpm = struct.unpack_from("<8H", xm, 64)
    pos, rows, notes = 60 + header_size, 0, []
    for _ in range(n_pat):
        plen, _, n_rows, size = struct.unpack_from("<IBHH", xm, pos)
        data, pos, i = xm[pos + plen:pos + plen + size], pos + plen + size, 0
        for _ in range(n_rows * channels):
            flags_byte = data[i]
            i += 1
            assert flags_byte & 0x80, "writer always packs"
            if flags_byte & 1:
                notes.append(data[i])
            i += bin(flags_byte & 0x1F).count("1")
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
            "speed": speed, "bpm": bpm, "notes": notes}


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
    assert dropped == 0
    assert (info["channels"], info["instruments"], info["speed"]) == (2, 1, 1)
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
    print("ok")
