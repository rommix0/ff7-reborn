"""Record the SPU reverb presets in FF7's executable as impulse-response WAVs, for convolution reverbs.

usage: python tools/reverb_ir.py <SCUS_941.63> [-o assets/reverb]

Plays a click through the player's reverb (tools/akao_synth.py) for each of libspu's nine reverb types and writes
24-bit stereo WAVs at 44100 Hz:
  <n>_<type>.wav           the click on both inputs (a centred send), the one most plugins want
  <n>_<type>_left_in.wav   the click on the left input only; with _right_in, a "true stereo" pair
  <n>_<type>_right_in.wav
Levels are at full reverb depth; in the game the song's depth scales them (0x2800 = 0.31 in most songs).
"""
import argparse
import wave
from pathlib import Path

import numpy as np

from akao_synth import SPU_HZ, Reverb
from ff7song import reverb_presets

TYPES = ["off", "room", "studio_a", "studio_b", "studio_c", "hall", "space_echo", "echo", "delay", "pipe"]
LONGEST = 12  # seconds; Echo and Delay ring longest
FLOOR = 0.5 / 0x7FFFFF  # below half a 24-bit step the sample is silence


def impulse_response(preset: dict, left: float, right: float) -> np.ndarray:
    """(frames, 2): the reverb's output for one click of the given size on each input, trimmed to its tail."""
    reverb, block = Reverb(preset), 1024
    click = np.zeros((block, 2))
    click[0] = left, right
    out = [reverb.process(click, SPU_HZ)]
    silence = np.zeros((block, 2))
    out += [reverb.process(silence, SPU_HZ) for _ in range(LONGEST * SPU_HZ // block)]
    out = np.vstack(out)
    return out[:np.flatnonzero(np.abs(out).max(axis=1) >= FLOOR)[-1] + 1]


def write_wav_24(path: Path, frames: np.ndarray):
    pcm = np.round(np.clip(frames, -1, 1) * 0x7FFFFF).astype("<i4")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(3)
        w.setframerate(SPU_HZ)
        w.writeframes(pcm.view(np.uint8).reshape(-1, 4)[:, :3].tobytes())


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe", type=Path)
    ap.add_argument("-o", "--out", type=Path, default=Path("assets/reverb"))
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    for preset in reverb_presets(args.exe.read_bytes())[1:]:
        stem = f"{preset['mode']}_{TYPES[preset['mode']]}"
        for suffix, left, right in (("", 1, 1), ("_left_in", 1, 0), ("_right_in", 0, 1)):
            ir = impulse_response(preset, left, right)
            write_wav_24(args.out / f"{stem}{suffix}.wav", ir)
            if not suffix:
                print(f"{stem}: {len(ir) / SPU_HZ:.2f} s, peak {np.abs(ir).max():.3f}")
    print(f"-> {args.out}")


if __name__ == "__main__":
    main()
