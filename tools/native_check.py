"""Play songs on the native sound driver (make native) and check each channel's key-ons against tools/akao.py.

usage: python tools/native_check.py <disc folder> <field file or folder> [ticks]

The disc folder gives SCUS_941.63 and SOUND/INSTR.*; songs come from the AKAO blocks in the field files.
"""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from akao import channel_starts, named_songs, play_channel

NATIVE = Path(__file__).resolve().parent.parent / "build/native/snd_native"
# snd_start_song sets each channel's gate to 0x103: its first notes play on the third sequencer step.
FIRST_STEP = 2


def key_ons(events) -> list[int]:
    """Ticks a channel keys its voice on: every note but one a slur carries on from the last."""
    ticks, slur = [], False
    for e in events:
        if e[1] == "slur":
            slur = True
        elif e[1] == "note":
            if not slur:
                ticks.append(e[0])
            slur = False
    return ticks


def check_song(disc: Path, block: bytes, ticks: int) -> list[str]:
    with tempfile.NamedTemporaryFile(suffix=".akao", delete=False) as f:
        f.write(block)
    try:
        out = subprocess.run([str(NATIVE), str(disc / "SCUS_941.63"), str(disc / "SOUND/INSTR.ALL"),
                              str(disc / "SOUND/INSTR.DAT"), f.name, str(ticks + FIRST_STEP)],
                             capture_output=True, text=True, check=True).stdout
    finally:
        Path(f.name).unlink()
    native = [(int(t) - FIRST_STEP, int(m, 16)) for t, m in (line.split() for line in out.splitlines())]
    mask = struct.unpack_from("<I", block, 0x10)[0] & 0xFFFFFF
    voices = [1 << v for v in range(24) if mask >> v & 1]
    problems = []
    for k, (start, voice) in enumerate(zip(channel_starts(block), voices)):
        want = [t for t in key_ons(play_channel(block, start, until=ticks)[0]) if t < ticks]
        got = [t for t, m in native if m & voice and 0 <= t < ticks]
        if want != got:
            at = next((i for i, (a, b) in enumerate(zip(want, got)) if a != b), min(len(want), len(got)))
            problems.append(f"channel {k}: {len(want)} vs {len(got)} key-ons, first difference at key-on {at}: "
                            f"akao.py {want[at:at + 3]} native {got[at:at + 3]}")
    return problems


def main():
    disc, source = Path(sys.argv[1]), Path(sys.argv[2])
    ticks = int(sys.argv[3]) if len(sys.argv) > 3 else 4000
    bad = 0
    songs = named_songs(source)
    for name, origin, block in songs:
        problems = check_song(disc, block, ticks)
        bad += bool(problems)
        for p in problems:
            print(f"{name} ({origin}): {p}")
    print(f"{len(songs) - bad} of {len(songs)} songs match over {ticks} ticks")


if __name__ == "__main__":
    main()
