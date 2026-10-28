"""Decompress FF7 PS1 files: gzip overlays (.X, FIELD.BIN, ...), BIN-GZ archives (KERNEL.BIN, WINDOW.BIN), LZS (field .DAT).

Writes <file>.dec next to each input, or <file>.d/<n>.bin per BIN-GZ section.
"""
import argparse
import gzip
from pathlib import Path

GZIP_MAGIC = b"\x1f\x8b\x08"


def lzs(data: bytes) -> bytes:
    """Okumura-style LZSS: 4 KiB window primed with zeros, writes start 18 bytes before its end."""
    window = bytearray(4096)
    head = 4096 - 18
    out = bytearray()
    pos = 4  # u32 compressed length
    while pos < len(data):
        flags = data[pos]
        pos += 1
        for bit in range(8):
            if pos >= len(data):
                break
            if flags >> bit & 1:
                literal, count = data[pos], 1
                pos += 1
            else:
                lo, hi = data[pos], data[pos + 1]
                pos += 2
                literal, offset, count = None, lo | (hi & 0xF0) << 4, (hi & 0x0F) + 3
            # Byte-at-a-time so a reference can overlap the bytes it is producing.
            for i in range(count):
                b = literal if literal is not None else window[(offset + i) & 0xFFF]
                window[head] = b
                head = (head + 1) & 0xFFF
                out.append(b)
    return bytes(out)


def bingz(data: bytes) -> list[tuple[int, bytes]]:
    """Sections of a BIN-GZ archive as (type, bytes). Header per section: u16 gz length, u16 raw length, u16 type."""
    sections = []
    pos = 0
    while pos + 6 <= len(data):
        gz_len = int.from_bytes(data[pos:pos + 2], "little")
        if not gz_len:
            break
        raw = gzip.decompress(data[pos + 6:pos + 6 + gz_len])
        assert len(raw) == int.from_bytes(data[pos + 2:pos + 4], "little")
        sections.append((int.from_bytes(data[pos + 4:pos + 6], "little"), raw))
        pos += 6 + gz_len
    return sections


def unpack(path: Path):
    data = path.read_bytes()
    if data.startswith(GZIP_MAGIC):
        path.with_name(path.name + ".dec").write_bytes(gzip.decompress(data))
    elif data[8:11] == GZIP_MAGIC:
        raw = gzip.decompress(data[8:])
        assert len(raw) == int.from_bytes(data[:4], "little"), f"{path}: size mismatch"
        path.with_name(path.name + ".dec").write_bytes(raw)
    elif data[6:9] == GZIP_MAGIC:
        out = path.with_name(path.name + ".d")
        out.mkdir(exist_ok=True)
        for n, (kind, raw) in enumerate(bingz(data)):
            (out / f"{n:02d}_type{kind}.bin").write_bytes(raw)
    elif int.from_bytes(data[:4], "little") == len(data) - 4:
        path.with_name(path.name + ".dec").write_bytes(lzs(data))
    else:
        print(f"skip {path}: unknown format")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("files", type=Path, nargs="+")
    for path in ap.parse_args().files:
        unpack(path)


if __name__ == "__main__":
    main()
