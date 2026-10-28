import struct
import tempfile
import wave
from pathlib import Path

from instruments import decode_adpcm, write_wav

# Block 1: range 12 (sample = nibble), no filter: nibbles 1, 2, -1 (0xF), then zeros. Flags 0 = continue.
# Block 2: filter 1 (old * 60 / 64), nibbles 0: decays the last value. Flags 3 = end + repeat.
block1 = bytes([0x0C, 0x00, 0x21, 0x0F]) + bytes(12)
block2 = bytes([0x1C, 0x03]) + bytes(14)
samples, repeats = decode_adpcm(block1 + block2 + bytes(16), 0)
assert len(samples) == 56 and repeats
assert samples[:4] == [1, 2, -1, 0]
# Block 1 ending on sample 7, then block 2's filter 1 predicts from it: (7 * 60 + 32) >> 6.
seeded, _ = decode_adpcm(bytes([0x0C, 0x00]) + bytes(13) + bytes([0x70]) + block2, 0)
assert seeded[27] == 7 and seeded[28] == (7 * 60 + 32) >> 6

with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp, "t.wav")
    write_wav(path, [0, 100, -100, 0], loop_start=1)
    with wave.open(str(path)) as w:
        assert (w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()) == (1, 2, 44100, 4)
    data = path.read_bytes()
    smpl = data.index(b"smpl")
    assert struct.unpack_from("<2I", data, smpl + 8 + 36 + 8) == (1, 3)  # loop start, loop end
print("ok")
