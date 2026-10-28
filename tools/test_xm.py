import struct
import sys
from pathlib import Path

from xm import KEY_OFF, song_to_xm


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
    bank = {0: ([0, 1000, -1000, 0] * 10, 8, 0x1000)}
    xm, dropped = song_to_xm(song, bank, "test")
    info = walk(xm)
    assert dropped == 0
    assert (info["channels"], info["instruments"], info["speed"]) == (2, 1, 1)
    # 0x6000 tempo = 114 BPM; C of AKAO octave 4 is XM note 49 (C-4); the rest gets a key-off.
    assert info["bpm"] == 114 and info["notes"] == [49, KEY_OFF]
    assert info["rows"] == 48 and info["restart"] == 0  # 96 AKAO ticks = 48 rows of 2; loop back to the start
    print("ok")
