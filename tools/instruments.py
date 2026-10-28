"""Convert FF7's SPU instrument banks (SOUND/INSTR*.ALL samples + INSTR*.DAT tables) into WAV files.

usage: python tools/instruments.py <SOUND dir> [-o assets/instruments]

Writes one 16-bit mono WAV per instrument (looping ones carry a `smpl` loop chunk) plus instruments.csv with each
instrument's addresses, ADSR bytes and pitch. WAVs play at 44100 Hz, the SPU rate for pitch 0x1000; the game plays
semitone s of octave 6 at 44100 * pitch[s] / 0x1000 Hz.
"""
import argparse
import csv
import struct
from pathlib import Path

SPU_RATE = 44100
# (sample file, table file, first instrument slot the table fills, entries copied) as the driver's loaders do it.
BANKS = (("INSTR.ALL", "INSTR.DAT", 0, 128), ("INSTR2.ALL", "INSTR2.DAT", 53, 75))
# SPU-ADPCM prediction filters (psx-spx, "SPU ADPCM Samples").
FILTERS = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))


def decode_adpcm(data: bytes, pos: int) -> tuple[list[int], bool]:
    """Decode 16-byte SPU-ADPCM blocks from `pos` up to the block flagged 'end'. Returns (samples, repeats)."""
    out, old, older = [], 0, 0
    while pos + 16 <= len(data):
        header, flags = data[pos], data[pos + 1]
        f0, f1 = FILTERS[min(header >> 4 & 7, 4)]
        for byte in data[pos + 2:pos + 16]:
            for nibble in (byte & 0xF, byte >> 4):
                # (t << 12) >> range is the same as t << (12 - range), and stays defined for range 13-15.
                s = ((nibble - 16 if nibble & 8 else nibble) << 12 >> (header & 0xF)) + ((old * f0 + older * f1 + 32) >> 6)
                s = max(-0x8000, min(0x7FFF, s))
                out.append(s)
                older, old = old, s
        pos += 16
        if flags & 1:
            return out, bool(flags & 2)
    return out, False


def write_wav(path: Path, samples: list[int], loop_start: int | None):
    pcm = struct.pack(f"<{len(samples)}h", *samples)
    fmt = struct.pack("<HHIIHH", 1, 1, SPU_RATE, SPU_RATE * 2, 2, 16)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    if loop_start is not None:
        # smpl: unity note 60, one forward loop from loop_start to the last sample.
        smpl = struct.pack("<9I", 0, 0, 1_000_000_000 // SPU_RATE, 60, 0, 0, 0, 1, 0)
        smpl += struct.pack("<6I", 0, 0, loop_start, len(samples) - 1, 0, 0)
        chunks += b"smpl" + struct.pack("<I", len(smpl)) + smpl
    chunks += b"data" + struct.pack("<I", len(pcm)) + pcm
    path.write_bytes(b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sound_dir", type=Path)
    ap.add_argument("-o", "--out", type=Path, default=Path("assets/instruments"))
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    rows = []
    for bank, (all_name, dat_name, first_slot, count) in enumerate(BANKS, 1):
        samples_file = (args.sound_dir / all_name).read_bytes()
        table = (args.sound_dir / dat_name).read_bytes()
        spu_base, length = struct.unpack_from("<II", samples_file)
        adpcm = samples_file[16:16 + length]
        for i in range(count):
            entry = table[i * 64:(i + 1) * 64]
            sample_addr, loop_addr = struct.unpack_from("<II", entry)
            # Unused slots are zero, or point outside this bank's SPU range.
            if not spu_base <= sample_addr < spu_base + length:
                continue
            samples, repeats = decode_adpcm(adpcm, sample_addr - spu_base)
            loop_start = (loop_addr - sample_addr) // 16 * 28 if repeats and loop_addr >= sample_addr else None
            slot = first_slot + i
            name = f"bank{bank}_{slot:03d}.wav"
            write_wav(args.out / name, samples, loop_start)
            pitch = struct.unpack_from("<12I", entry, 16)
            rows.append({
                "file": name, "bank": bank, "slot": slot,
                "spu_sample": f"{sample_addr:#07x}", "spu_loop": f"{loop_addr:#07x}",
                "samples": len(samples), "loop_start": "" if loop_start is None else loop_start,
                "adsr_bytes": entry[8:16].hex(" "),
                "pitch_c": f"{pitch[0]:#06x}", "hz_for_c_octave6": round(SPU_RATE * pitch[0] / 0x1000),
            })
    with (args.out / "instruments.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(f"{len(rows)} instruments -> {args.out}")


if __name__ == "__main__":
    main()
