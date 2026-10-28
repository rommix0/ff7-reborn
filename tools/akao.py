"""Parse AKAO music sequences (PS1 Final Fantasy VII) and check them against the command lengths we derived.

usage:
  python tools/akao.py check <assets dir>     # parse every distinct sequence under the folder, report problems
  python tools/akao.py dump <file> [index]    # disassemble one .SND file or the AKAO blocks in a field .DAT
"""
import struct
import sys
from pathlib import Path

from unpack import lzs

MAGIC = b"AKAO"
HEADER = 0x10
EFFECT_BASE = 0x1000
# Argument bytes per sequence command (0xA0-0xFF), from how far each driver handler moves the read pointer.
# Every song and sound effect on the three discs parses with these. B7 BE BF CE CF DF E9 EB EF F1 F3 never occur,
# so their lengths rest on the handler code alone. E0-E7, FA-FC and FF are unused (the driver treats them as A0).
ARGS = {
    **dict.fromkeys([0xA0, 0xA6, 0xA7, 0xB3, 0xB6, 0xBA, 0xBE, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xCA,
                     0xCB, 0xCC, 0xCD, 0xD0, 0xD1, 0xD4, 0xD5, 0xD6, 0xD7, 0xDB, 0xED, 0xF3, 0xF5, 0xF9], 0),
    **dict.fromkeys([0xA1, 0xA2, 0xA3, 0xA5, 0xA8, 0xAA, 0xAC, 0xAD, 0xAE, 0xAF, 0xB1, 0xB2, 0xB5, 0xB7, 0xB9,
                     0xBB, 0xBD, 0xBF, 0xC0, 0xC1, 0xC9, 0xCE, 0xCF, 0xD2, 0xD3, 0xD8, 0xD9, 0xDA, 0xDC, 0xF2,
                     0xF6, 0xF8], 1),
    **dict.fromkeys([0xA4, 0xA9, 0xAB, 0xB0, 0xBC, 0xDD, 0xDE, 0xDF, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xEE, 0xF4,
                     0xF7, 0xFD, 0xFE], 2),
    **dict.fromkeys([0xB4, 0xB8, 0xEF, 0xF0, 0xF1], 3),
}
# Commands after which the stream never continues linearly: end of channel, loop back forever, jump.
STOP = {0xA0, 0xCA, 0xEE}


def blocks(data: bytes):
    """Yield (offset, block bytes) for every AKAO block in a buffer (header length field gives the size).

    "AKAO" also turns up by chance inside compressed data, so a hit must have a sane header and channel table.
    """
    pos = data.find(MAGIC)
    while pos >= 0:
        size = HEADER + struct.unpack_from("<H", data, pos + 6)[0] if pos + 0x18 <= len(data) else 0
        block = data[pos:pos + size]
        if HEADER + 4 < size <= len(data) - pos and struct.unpack_from("<I", block, HEADER)[0] \
                and all(s < size for s in channel_starts(block)):
            yield pos, block
            pos = data.find(MAGIC, pos + size)
        else:
            pos = data.find(MAGIC, pos + 1)


def channel_starts(block: bytes) -> list[int]:
    """Offsets of each channel's data. Each u16 table entry is relative to the byte after itself."""
    mask = struct.unpack_from("<I", block, HEADER)[0]
    starts = []
    for i in range(bin(mask).count("1")):
        field = HEADER + 4 + 2 * i
        if field + 2 > len(block):
            return starts + [len(block)]  # table runs off the block: not a real header
        starts.append(field + 2 + struct.unpack_from("<H", block, field)[0])
    return starts


def disassemble(block: bytes, start: int, end: int):
    """Return (events, problem). Events are (offset, kind, value) with kind 'note', 'tie', 'rest' or 'cmd'."""
    events, pos = [], start
    while pos < end:
        b = block[pos]
        if b < 0xA0:
            kind = "note" if b < 0x84 else "tie" if b < 0x8F else "rest"
            events.append((pos, kind, b))
            pos += 1
            continue
        if b not in ARGS:
            return events, f"unused command {b:02X} at {pos:#x}"
        events.append((pos, "cmd", block[pos:pos + 1 + ARGS[b]]))
        pos += 1 + ARGS[b]
        if b in STOP:
            return events, None if pos <= end else f"{b:02X} args run past {end:#x}"
    return events, f"no end command before {end:#x}"


def effect_starts(data: bytes) -> list[int]:
    """Stream offsets in SOUND/EFFECT.ALL: a 0x1000-byte table of u16 offsets (0xFFFF = empty) relative to 0x1000."""
    offsets = struct.unpack_from("<2048H", data)
    return sorted({EFFECT_BASE + o for o in offsets if o != 0xFFFF})


def sequences(root: Path):
    """Yield (name, block) for every distinct AKAO block in any extracted file, LZS files decompressed first."""
    seen = set()
    # ponytail: pure-Python LZS on every compressed file, ~3 min for three discs; cache decompressed maps if it matters.
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        data = path.read_bytes()
        if len(data) > 4 and int.from_bytes(data[:4], "little") == len(data) - 4:
            data = lzs(data)
        for off, block in blocks(data):
            if block not in seen:
                seen.add(block)
                yield f"{path.relative_to(root)}@{off:#x}", block


def check_streams(name: str, data: bytes, starts: list[int], problems: dict) -> tuple[int, int]:
    """Parse each stream to the end of `data`; return (clean, ending at the next stream's start)."""
    clean = exact = 0
    # A stream may fall through into the next one's bytes (shared tails), so parse to the end of the buffer.
    for start, next_start in zip(starts, starts[1:] + [len(data)]):
        events, problem = disassemble(data, start, len(data))
        if problem:
            problems.setdefault(problem.split(" at ")[0].split(" before")[0], []).append(f"{name}+{start:#x}")
            continue
        clean += 1
        last_pos, _, last = events[-1]
        # Strict check on argument lengths: most streams end exactly where the next one begins.
        exact += last_pos + len(last) in range(next_start - 3, next_start + 1)
    return clean, exact


def check(root: Path):
    problems = {}
    songs = channels = clean = exact = 0
    for name, block in sequences(root):
        starts = channel_starts(block)
        songs += 1
        channels += len(starts)
        c, e = check_streams(name, block, starts, problems)
        clean, exact = clean + c, exact + e
    print(f"music: {songs} distinct sequences, {channels} channels, {clean} parse cleanly, "
          f"{exact} end within 3 bytes of the next channel")
    for path in sorted(root.rglob("EFFECT.ALL")):
        data = path.read_bytes()
        starts = effect_starts(data)
        c, e = check_streams(str(path.relative_to(root)), data, starts, problems)
        print(f"effects {path.relative_to(root)}: {len(starts)} streams, {c} parse cleanly, {e} end at the next one")
    for problem, names in sorted(problems.items(), key=lambda kv: -len(kv[1])):
        print(f"  {len(names):4d}x {problem}  e.g. {names[0]}")


def show(events) -> str:
    return " ".join(v.hex(" ").upper() if k == "cmd" else f"{k[0]}{v // 11 if k == 'note' else ''}/{v % 11}"
                    for _, k, v in events)


def dump(path: Path, index: int):
    data = path.read_bytes()
    if path.name.upper() == "EFFECT.ALL":
        start = effect_starts(data)[index]
        events, problem = disassemble(data, start, len(data))
        print(f"effect stream {index} @{start:#x}" + (f"  !! {problem}" if problem else ""))
        print("   " + show(events))
        return
    if len(data) > 4 and int.from_bytes(data[:4], "little") == len(data) - 4:
        data = lzs(data)
    off, block = list(blocks(data))[index]
    song_id, length, _ = struct.unpack_from("<HHH", block, 4)
    print(f"AKAO block @{off:#x}: id {song_id:#x}, {length} data bytes, stamp 19{block[10:16].hex()}")
    starts = channel_starts(block)
    for ch, start in enumerate(starts):
        events, problem = disassemble(block, start, len(block))
        print(f"-- channel {ch} @{start:#x}" + (f"  !! {problem}" if problem else ""))
        print("   " + show(events))


def main():
    match sys.argv[1:]:
        case ["check", root]:
            check(Path(root))
        case ["dump", path]:
            dump(Path(path), 0)
        case ["dump", path, index]:
            dump(Path(path), int(index))
        case _:
            print(__doc__)


if __name__ == "__main__":
    main()
