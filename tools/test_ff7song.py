import struct
import tempfile
from pathlib import Path

from ff7song import (EFFECT_TEMPO, EXE_BASE, LFO_WAVEFORMS, REVERB_PRESETS, REVERB_WORK_STARTS, effect_to_ff7song,
                     lfo_noise, reverb_presets, song_to_ff7song, write_bank)


def song(*channels: bytes) -> bytes:
    table = struct.pack("<I", (1 << len(channels)) - 1)
    rel = 2 * len(channels)
    for ch in channels[:-1]:  # each offset is relative to the byte after itself
        table += struct.pack("<H", rel - 2)
        rel += len(ch) - 2
    table += struct.pack("<H", rel - 2)
    body = table + b"".join(channels)
    return b"AKAO" + struct.pack("<HHH", 76, len(body), 0) + bytes(6) + body


# One bar per pattern, in the song's own time signatures (song 76 alternates 6/8 and 5/8): a 1/4 pickup, then a loop
# of two identical 6/8 + 5/8 pairs. Identical bars share a pattern; the loop restarts at the first 6/8 bar.
pair = bytes([0xFD, 0x18, 0x06, 0xA2, 0x90, 0x00, 0xFD, 0x18, 0x05, 0xA2, 0x78, 0x00])
out = song_to_ff7song(song(bytes([0xFD, 0x30, 0x01, 0x91]) + pair + pair + bytes([0xEE, 0xE5, 0xFF])))
assert out["song"] == 76 and out["channels"] == 1 and out["loop"] == {"start": 48, "end": 576}
assert [(p["ticks"], p["beat"], p["beats"]) for p in out["patterns"]] == [(48, 48, 1), (144, 24, 6), (120, 24, 5)]
assert out["order"] == [0, 1, 2, 1, 2] and out["restart"] == 1
# Events on one tick and channel share a cell; the driver's channel defaults come first.
assert out["patterns"][0]["cells"] == [[0, 0, {"inst": 5, "volume": 100, "expression": 120}]]
assert out["patterns"][1]["cells"] == [[0, 0, {"note": 36, "len": 142}]]  # C, octave 2; keys off 2 ticks early

# Notes keep their full length across the loop end, as the driver holds them into the next pass. Channel 1 loops a
# whole note from tick 0, channel 0 from tick 48, so the song loops 48..240 and channel 1's note at 192 runs on.
ch0 = bytes([0x91, 0x00, 0xEE, 0xFC, 0xFF])  # quarter rest, then whole note C forever
ch1 = bytes([0x00, 0xEE, 0xFC, 0xFF])        # whole note C forever
out = song_to_ff7song(song(ch0, ch1))
assert out["loop"] == {"start": 48, "end": 240}
assert [p["ticks"] for p in out["patterns"]] == [48, 192]  # no time signature: 4/4 bars counted from the loop
assert [144, 1, {"note": 36, "len": 190}] in out["patterns"][1]["cells"]  # tick 192, untrimmed (not 48)

# A note tied across the loop point: the loop body opens with a tie, so on every pass after the first the last note
# of the loop keeps sounding into the next one. Note at 0, then loop 48..144: tie, note at 96, back to the tie.
out = song_to_ff7song(song(bytes([0x02, 0x86, 0x02, 0xEE, 0xFB, 0xFF])))
assert out["loop"] == {"start": 48, "end": 144}
assert [48, 0, {"note": 36, "len": 94}] in out["patterns"][1]["cells"]  # held through the next pass's tie, not 46
# A drone: one note, then a loop of nothing but ties (songs 66 and 75). It never ends, so its length is null.
out = song_to_ff7song(song(bytes([0x00, 0x84, 0xEE, 0xFC, 0xFF])))
assert out["patterns"][0]["cells"][0][2]["len"] is None

# Slurs and the channel's own envelope settings become cell fields; songs say which instrument bank they need.
out = song_to_ff7song(song(bytes([0xAD, 0x28, 0xCC, 0x02, 0x02, 0xCD, 0xA0])))
cells = {tick: fields for tick, _, fields in out["patterns"][0]["cells"]}
assert cells[0]["ar"] == 0x28 and cells[48] == {"slur": True, "note": 36, "len": 46} and out["bank"] == 1

# Reverb: the header's mode picks one of libspu's presets from the executable; C2/C3 and EA become cell fields.
exe = bytearray(REVERB_PRESETS - EXE_BASE + 10 * 0x44)
struct.pack_into("<I", exe, REVERB_WORK_STARTS - EXE_BASE + 3 * 4, 0xF6F8)
struct.pack_into("<32H", exe, REVERB_PRESETS - EXE_BASE + 3 * 0x44 + 4, *range(32))
block = bytearray(song(bytes([0xC2, 0xEA, 0x00, 0x28, 0x02, 0xC3, 0x02, 0xA0])))
block[8] = 3
out = song_to_ff7song(bytes(block), reverb_presets(bytes(exe)))
assert out["reverb"] == {"mode": 3, "start": 0xF6F8, "registers": list(range(32))}
cells = {tick: fields for tick, _, fields in out["patterns"][0]["cells"]}
assert cells[0]["reverb"] is True and cells[0]["reverb_depth"] == 0x2800 and cells[48]["reverb"] is False
block[8] = 0
assert song_to_ff7song(bytes(block), reverb_presets(bytes(exe)))["reverb"] is None

# The noise LFO (waveform 12): read through the executable's waveform pointers, up to its 0, 0, -n end marker.
# Only songs that use it carry it.
exe += bytes(8 * 2)
struct.pack_into("<I", exe, LFO_WAVEFORMS - EXE_BASE + 12 * 4, len(exe) - 16 + EXE_BASE)
struct.pack_into("<8h", exe, len(exe) - 16, 0, 500, -700, 0, 0, 0, 0, -5)
noise = lfo_noise(bytes(exe))
assert noise == [0, 500, -700, 0, 0]
assert song_to_ff7song(song(bytes([0xB4, 0, 1, 12, 0x02, 0xA0])), None, noise)["lfo_noise"] == noise
assert "lfo_noise" not in song_to_ff7song(song(bytes([0xB4, 0, 1, 6, 0x02, 0xA0])), None, noise)

# A sound effect: its channels at the effect tempo, from tick 0; no reverb of its own.
table = [0xFFFF] * 2048
table[10], table[11] = 0, 2
data = struct.pack("<2048H", *table) + bytes([0x02, 0xA0, 0x18, 0xA0])
out = effect_to_ff7song(data, 5, [0x1000, 0x1002])
assert out["effect"] == 5 and "song" not in out and out["channels"] == 2 and out["reverb"] is None
cells = {(tick, ch): fields for tick, ch, fields in out["patterns"][0]["cells"]}
assert cells[0, 0]["tempo"] == EFFECT_TEMPO == 0x66A8 and cells[0, 1]["note"] == 38

# Bank: header, 68-byte entries, then each instrument's samples.
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / "test.ff7bank"
    write_bank({3: ([0, 100, -100], 1, tuple(range(0x1000, 0x100C)), bytes(range(8))),
                9: ([7], None, (0x2000,) * 12, bytes(8))}, path, bank2={53: ([5, 6], None, (0x1000,) * 12, bytes(8))})
    data = path.read_bytes()
assert data[:12] == b"FF7BANK\0" + struct.pack("<HH", 1, 3) and len(data) == 12 + 3 * 68 + 6 * 2
assert struct.unpack_from("<HH", data, 12 + 2 * 68) == (53, 2)  # flag bit 1: the second bank's instrument
slot, flags, length, loop, *rest = struct.unpack_from("<HHII12I8s", data, 12)
assert (slot, flags, length, loop, rest[0], rest[11], rest[12]) == (3, 1, 3, 1, 0x1000, 0x100B, bytes(range(8)))
assert struct.unpack_from("<HHII", data, 12 + 68) == (9, 0, 1, 0)
assert struct.unpack_from("<6h", data, 12 + 3 * 68) == (0, 100, -100, 7, 5, 6)
print("ok")
