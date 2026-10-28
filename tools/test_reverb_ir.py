import tempfile
import wave
from pathlib import Path

import numpy as np

from reverb_ir import impulse_response, write_wav_24

# A made-up preset (not one of libspu's), laid out like the real ones: each filter's output is read by the combs
# before the next filter down overwrites it, and the reflections read just above the other filters' writes.
preset = {"mode": 3, "start": 0x10000 - 0x200, "registers": [
    0x10, 0x08, 0x6000, 0x4000, 0xC000, 0x3000, 0xD000, 0xB000, 0x5000, 0x4800,
    0x1F0, 0x190, 0x1C0, 0x170, 0x1A8, 0x158, 0x191, 0x101, 0x100, 0x0C0, 0x0F0, 0x0B0, 0x0E0, 0x0A0,
    0x0C1, 0x081, 0x080, 0x060, 0x040, 0x020, 0x8000, 0x8000]}

# The reverb is linear: the centred response is the sum of the two one-sided ones, and it dies out.
both, left, right = (impulse_response(preset, *inputs) for inputs in ((1, 1), (1, 0), (0, 1)))
n = min(len(both), len(left), len(right))
assert np.abs(both[:n] - left[:n] - right[:n]).max() < 1e-9
assert 0.05 < len(both) / 44100 < 12 and np.abs(both).max() > 0.01

# 24-bit stereo WAV, read back to the same values within a 24-bit step.
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / "ir.wav"
    write_wav_24(path, both)
    with wave.open(str(path)) as w:
        assert (w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()) == (2, 3, 44100, len(both))
        raw = np.frombuffer(w.readframes(len(both)), np.uint8).reshape(-1, 3)
back = (np.pad(raw, ((0, 0), (1, 0))).view("<i4").ravel() >> 8) / 0x7FFFFF
assert np.abs(back.reshape(-1, 2) - both).max() <= 1 / 0x7FFFFF
print("ok")
