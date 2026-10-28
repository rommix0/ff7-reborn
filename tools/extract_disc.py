"""Extract files from a raw 2352-byte-sector .bin image (ISO9660, MODE1 or MODE2) into assets/."""
import argparse
import hashlib
from pathlib import Path

SECTOR = 2352
FORM1_DATA = 2048
SUBMODE_FORM2 = 0x20


def read_sector(img, lba: int) -> bytes:
    img.seek(lba * SECTOR)
    return img.read(SECTOR)


def user_data(sector: bytes) -> bytes:
    # MODE1 (PC discs) has no XA subheader, so data starts 8 bytes earlier than MODE2.
    start = 16 if sector[15] == 1 else 24
    return sector[start:start + FORM1_DATA]


def walk(img, lba: int, size: int, prefix: str = ""):
    """Yield (path, lba, size) for every file under the directory extent."""
    for n in range((size + FORM1_DATA - 1) // FORM1_DATA):
        data = user_data(read_sector(img, lba + n))
        pos = 0
        # Records never straddle sectors; a zero length byte means the rest is padding.
        while pos < FORM1_DATA and data[pos]:
            rec = data[pos:pos + data[pos]]
            pos += rec[0]
            name = rec[33:33 + rec[32]]
            if name in (b"\0", b"\1"):
                continue
            ext_lba = int.from_bytes(rec[2:6], "little")
            ext_size = int.from_bytes(rec[10:14], "little")
            name = name.decode("ascii").split(";")[0]
            if rec[25] & 2:
                yield from walk(img, ext_lba, ext_size, f"{prefix}{name}/")
            else:
                yield f"{prefix}{name}", ext_lba, ext_size


def read_file(img, lba: int, size: int) -> tuple[bytes, bool]:
    """Return file bytes. Files with any Form 2 sector (STR/XA) come back as raw 2352-byte sectors."""
    count = (size + FORM1_DATA - 1) // FORM1_DATA
    sectors = [read_sector(img, lba + n) for n in range(count)]
    # STR movies interleave Form 1 video with Form 2 audio, so the first sector alone can't tell.
    if any(s[15] == 2 and s[18] & SUBMODE_FORM2 for s in sectors):
        return b"".join(sectors), True
    return b"".join(user_data(s) for s in sectors)[:size], False


def files(img):
    root = user_data(read_sector(img, 16))[156:156 + 34]
    return walk(img, int.from_bytes(root[2:6], "little"), int.from_bytes(root[10:14], "little"))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("bin", type=Path)
    ap.add_argument("-o", "--out", type=Path, help="default: assets/<bin name>")
    ap.add_argument("-l", "--list", action="store_true", help="list files with LBA, size, sha1")
    args = ap.parse_args()
    out = args.out or Path("assets") / args.bin.stem

    with args.bin.open("rb") as img:
        for path, lba, size in files(img):
            data, raw = read_file(img, lba, size)
            if args.list:
                print(f"{lba:7d} {size:10d} {'XA ' if raw else '   '}{hashlib.sha1(data).hexdigest()} {path}")
                continue
            dst = out / path
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(data)


if __name__ == "__main__":
    main()
