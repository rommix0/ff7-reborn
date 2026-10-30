"""Rewrap FF7's movies for the PC port: MJPEG and PCM in an AVI, with each frame's camera data beside it.

usage: python tools/movie.py <assets> [-o assets/movies]

Converts every STR/MOV movie under <assets>/*/MOVIE (tools/extract_disc.py output). A movie on several discs is the
same file on each, so it's converted once. For each movie:
  <name>.avi  MJPEG video at 15 fps, plus the XA audio as 16-bit PCM when the movie has sound
  <name>.cam  the 40 bytes of camera data that open each frame, in frame order; frame N's camera goes with the
              AVI's Nth non-empty video chunk

The video isn't re-encoded. Each frame's coefficients are dequantized the way the MDEC does it, with the tables read
from SCUS_941.63, and stored in a baseline JPEG whose quantization table divides them exactly, so a JPEG decoder
starts from the same coefficients the MDEC did.

Frames advance at 15 fps but never before their data arrives (the CD streams 150 sectors a second). A frame that
arrives late repeats the previous one in the AVI (an empty chunk), which keeps the audio in sync.
"""
import argparse
import struct
from array import array
from collections import Counter
from functools import cache
from heapq import heappop, heappush
from itertools import count
from math import gcd
from multiprocessing import Pool
from pathlib import Path

EXE_BASE = 0x80010000 - 0x800  # load address minus the PS-X EXE header
MDEC_TABLES = 0x80051A50  # libpress's luma then chroma quantization tables, 64 bytes each in zigzag order
SYNC = b"\0" + b"\xff" * 10 + b"\0"
STR_MAGIC = b"\x60\x01\x01\x80"
CAMERA_BYTES = 40
FPS = 15
SECTORS_PER_FRAME = 150 // FPS
XA_HZ = 37800
XA_FILTERS = ((0, 0), (60, 0), (115, -52), (98, -55))

# MPEG-1 DCT coefficient codes (ISO 11172-2 table B.14) as code/run/level; a sign bit follows each code.
# The PS1 bitstream ends a block with 10 and escapes with 000001, a 6-bit run and a 10-bit signed level.
AC_CODES = """
    11/0/1 011/1/1 0100/0/2 0101/2/1 00101/0/3 00111/3/1 00110/4/1 000110/1/2 000111/5/1 000101/6/1 000100/7/1
    0000110/0/4 0000100/2/2 0000111/8/1 0000101/9/1 00100110/0/5 00100001/0/6 00100101/1/3 00100100/3/2
    00100111/10/1 00100011/11/1 00100010/12/1 00100000/13/1 0000001010/0/7 0000001100/1/4 0000001011/2/3
    0000001111/4/2 0000001001/5/2 0000001110/14/1 0000001101/15/1 0000001000/16/1
    000000011101/0/8 000000011000/0/9 000000010011/0/10 000000010000/0/11 000000011011/1/5 000000010100/2/4
    000000011100/3/3 000000010010/4/3 000000011110/6/2 000000010101/7/2 000000010001/8/2 000000011111/17/1
    000000011010/18/1 000000011001/19/1 000000010111/20/1 000000010110/21/1
    0000000011010/0/12 0000000011001/0/13 0000000011000/0/14 0000000010111/0/15 0000000010110/1/6
    0000000010101/1/7 0000000010100/2/5 0000000010011/3/4 0000000010010/5/3 0000000010001/9/2 0000000010000/10/2
    0000000011111/22/1 0000000011110/23/1 0000000011101/24/1 0000000011100/25/1 0000000011011/26/1
    00000000011111/0/16 00000000011110/0/17 00000000011101/0/18 00000000011100/0/19 00000000011011/0/20
    00000000011010/0/21 00000000011001/0/22 00000000011000/0/23 00000000010111/0/24 00000000010110/0/25
    00000000010101/0/26 00000000010100/0/27 00000000010011/0/28 00000000010010/0/29 00000000010001/0/30
    00000000010000/0/31 000000000011000/0/32 000000000010111/0/33 000000000010110/0/34 000000000010101/0/35
    000000000010100/0/36 000000000010011/0/37 000000000010010/0/38 000000000010001/0/39 000000000010000/0/40
    000000000011111/1/8 000000000011110/1/9 000000000011101/1/10 000000000011100/1/11 000000000011011/1/12
    000000000011010/1/13 000000000011001/1/14 0000000000010011/1/15 0000000000010010/1/16 0000000000010001/1/17
    0000000000010000/1/18 0000000000010100/6/3 0000000000011010/11/2 0000000000011001/12/2 0000000000011000/13/2
    0000000000010111/14/2 0000000000010110/15/2 0000000000010101/16/2 0000000000011111/27/1 0000000000011110/28/1
    0000000000011101/29/1 0000000000011100/30/1 0000000000011011/31/1
"""


@cache
def ac_lookup() -> list:
    """Maps the next 17 bits of a block to (bits used, run, level); run None ends the block, run -1 escapes."""
    table = [None] * (1 << 17)

    def put(code, entry):
        span = 1 << 17 - len(code)
        table[int(code, 2) * span:(int(code, 2) + 1) * span] = [entry] * span

    put("10", (2, None, 0))
    put("000001", (6, -1, 0))
    for entry in AC_CODES.split():
        code, run, level = entry.split("/")
        put(code + "0", (len(code) + 1, int(run), int(level)))
        put(code + "1", (len(code) + 1, int(run), -int(level)))
    return table


def mdec_blocks(mdec: bytes, blocks: int, tables: tuple[bytes, bytes]) -> list[list[tuple[int, int]]]:
    """Each block's coefficients as (zigzag index, value), dequantized and saturated the way the MDEC does it."""
    _, magic, qscale, version = struct.unpack_from("<4H", mdec)
    if magic != 0x3800 or version not in (1, 2):
        raise ValueError(f"not an MDEC v1/v2 frame (magic {magic:#x}, version {version})")
    body = bytearray(mdec[8:len(mdec) & ~1])
    body[0::2], body[1::2] = body[1::2], body[0::2]  # 16-bit little-endian words, read from the top bit down
    bits = format(int.from_bytes(body, "big"), f"0{len(body) * 8}b") + "0" * 17
    ac = ac_lookup()
    out, pos = [], 0
    for i in range(blocks):
        qt = tables[0] if i % 6 >= 2 else tables[1]  # each macroblock is Cr, Cb, then four Y
        dc = int(bits[pos:pos + 10], 2)
        pos += 10
        coefs = [(0, max(-1024, min(1023, (dc - (dc >> 9 << 10)) * qt[0])))]
        k = 0
        while True:
            used, run, level = ac[int(bits[pos:pos + 17], 2)]
            pos += used
            if run is None:
                break
            if run < 0:
                run, level = int(bits[pos:pos + 6], 2), int(bits[pos + 6:pos + 16], 2)
                level -= level >> 9 << 10
                pos += 16
            k += run + 1
            coefs.append((k, max(-1024, min(1023, level * qt[k] * qscale + 4 >> 3))))
        out.append(coefs)
    if pos > len(bits) - 17:
        raise ValueError("MDEC bitstream ran out before the last block")
    return out


def huffman(freq: Counter) -> tuple[bytes, dict]:
    """A JPEG Huffman table for these symbol counts: the DHT counts and symbols, and each symbol's code as bits."""
    ids = count()
    heap = [(n, next(ids), [s]) for s, n in freq.items()] + [(0, next(ids), [None])]  # None: a spare, so no code is all ones
    depth = Counter()
    while len(heap) > 1:
        n1, _, a = heappop(heap)
        n2, _, b = heappop(heap)
        for s in a + b:
            depth[s] += 1
        heappush(heap, (n1 + n2, next(ids), a + b))
    per_length = Counter(depth.values())
    # JPEG caps codes at 16 bits: lift pairs of over-long codes, splitting a shorter code to make room
    for n in range(max(per_length), 16, -1):
        while per_length[n]:
            j = n - 2
            while not per_length[j]:
                j -= 1
            per_length[n] -= 2
            per_length[n - 1] += 1
            per_length[j + 1] += 2
            per_length[j] -= 1
    per_length[max(n for n in per_length if per_length[n])] -= 1  # the spare had the longest code
    order = sorted(freq, key=lambda s: (depth[s], -freq[s]))
    lengths = [n for n in range(1, 17) for _ in range(per_length[n])]
    codes, code, previous = {}, 0, 1
    for s, n in zip(order, lengths):
        code <<= n - previous
        previous = n
        codes[s] = format(code, f"0{n}b")
        code += 1
    return bytes(per_length[n] for n in range(1, 17)) + bytes(order), codes


def jpeg_frame(blocks: list[list[tuple[int, int]]], width: int, height: int) -> bytes:
    """A baseline 4:2:0 JPEG holding exactly these MDEC coefficients (macroblocks column by column, Cr Cb Y Y Y Y)."""
    # Quantize each position by the largest step that divides all its values, so nothing is rounded
    quant = []
    for luma in (True, False):
        steps = [0] * 64
        for i, block in enumerate(blocks):
            if (i % 6 >= 2) == luma:
                for k, v in block:
                    steps[k] = gcd(steps[k], v)
        quant.append([next(d for d in range(min(n, 255), 0, -1) if n % d == 0) if n else 1 for n in steps])

    # Symbols per table: DC luma, AC luma, DC chroma, AC chroma
    symbols, predictors = [], [0, 0, 0]
    rows = height // 16
    for y in range(rows):
        for x in range(width // 16):
            mb = blocks[(x * rows + y) * 6:(x * rows + y + 1) * 6]
            for component, block in ((0, mb[2]), (0, mb[3]), (0, mb[4]), (0, mb[5]), (1, mb[1]), (2, mb[0])):
                q, table = quant[component > 0], 2 * (component > 0)
                dc = block[0][1] // q[0]
                diff = dc - predictors[component]
                predictors[component] = dc
                symbols.append((table, abs(diff).bit_length(), diff))
                last = 0
                for k, v in block[1:]:
                    if not v:
                        continue
                    v = max(v // q[k], -1023)  # -1024 needs an 11-bit AC category baseline JPEG lacks; it's one step off
                    run = k - last - 1
                    while run > 15:
                        symbols.append((table + 1, 0xF0, 0))
                        run -= 16
                    size = abs(v).bit_length()
                    symbols.append((table + 1, run << 4 | size, v))
                    last = k
                if last < 63:
                    symbols.append((table + 1, 0x00, 0))

    tables = [huffman(Counter(s for t, s, _ in symbols if t == n)) for n in range(4)]
    parts = []
    for table, symbol, v in symbols:
        parts.append(tables[table][1][symbol])
        size = symbol & 15 if table & 1 else symbol
        if size:
            parts.append(format(v if v >= 0 else v + (1 << size) - 1, f"0{size}b"))
    bits = "".join(parts)
    bits += "1" * (-len(bits) % 8)
    scan = int(bits, 2).to_bytes(len(bits) // 8, "big").replace(b"\xff", b"\xff\x00")

    def segment(marker, payload):
        return struct.pack(">HH", marker, len(payload) + 2) + payload

    return b"".join([
        b"\xff\xd8",
        segment(0xFFDB, b"\0" + bytes(quant[0]) + b"\1" + bytes(quant[1])),
        segment(0xFFC0, struct.pack(">BHHB", 8, height, width, 3) + bytes([1, 0x22, 0, 2, 0x11, 1, 3, 0x11, 1])),
        segment(0xFFC4, b"".join(bytes([(n & 1) << 4 | n >> 1]) + tables[n][0] for n in range(4))),
        segment(0xFFDA, bytes([3, 1, 0x00, 2, 0x11, 3, 0x11, 0, 63, 0])),
        scan,
        b"\xff\xd9",
    ])


def xa_pcm(sectors: list[bytes]) -> bytes:
    """Decode stereo 37.8 kHz 4-bit XA ADPCM sectors to interleaved 16-bit PCM, de-emphasizing flagged sectors."""
    # Emphasis is the CD's 50/15 us first-order shelf (IEC 60908); undo it with the bilinear transform of its inverse,
    # within 1 dB of the analog curve up to 10 kHz. The PS1's exact filter is undocumented.
    t50, t15 = 2 * XA_HZ * 50e-6, 2 * XA_HZ * 15e-6
    b0, b1, a1 = (1 + t15) / (1 + t50), (1 - t15) / (1 + t50), (1 - t50) / (1 + t50)
    emphasis = [[0.0, 0.0], [0.0, 0.0]]  # per channel: last input, last output
    history = [[0, 0], [0, 0]]
    out = array("h")
    for sector in sectors:
        pcm = []
        for group in range(24, 24 + 18 * 128, 128):
            for pair in range(0, 8, 2):
                channels = []
                for unit in (pair, pair + 1):
                    param = sector[group + 4 + unit]
                    shift, (k0, k1) = param & 15, XA_FILTERS[param >> 4 & 3]
                    h = history[unit & 1]
                    samples = []
                    for j in range(28):
                        nibble = sector[group + 16 + j * 4 + unit // 2] >> (unit & 1) * 4 & 15
                        s = ((nibble << 12 ^ 0x8000) - 0x8000 >> shift) + (h[0] * k0 + h[1] * k1 + 32 >> 6)
                        h[1], h[0] = h[0], max(-0x8000, min(0x7FFF, s))
                        samples.append(h[0])
                    channels.append(samples)
                for left, right in zip(*channels):
                    pcm.extend((left, right))
        if sector[19] & 0x40:
            for i, x in enumerate(pcm):
                state = emphasis[i & 1]
                y = b0 * x + b1 * state[0] - a1 * state[1]
                state[0], state[1] = x, y
                pcm[i] = max(-0x8000, min(0x7FFF, round(y)))
        out.extend(pcm)
    return out.tobytes()


def write_avi(path: Path, width: int, height: int, slots: int, video, pcm: bytes):
    """MJPEG at FPS, plus 16-bit stereo PCM at XA_HZ when pcm isn't empty. video yields a JPEG per slot, b"" to repeat."""
    def chunk(tag, data):
        return tag + struct.pack("<I", len(data)) + data + b"\0" * (len(data) & 1)

    def strl(strh, strf):
        return chunk(b"LIST", b"strl" + chunk(b"strh", strh) + chunk(b"strf", strf))

    streams = strl(struct.pack("<4s4sIHHIIIIIIIIhhhh", b"vids", b"MJPG", 0, 0, 0, 0, 1, FPS, 0, slots, 0,
                               0xFFFFFFFF, 0, 0, 0, width, height),
                   struct.pack("<IiiHH4sIiiII", 40, width, height, 1, 24, b"MJPG", width * height * 3, 0, 0, 0, 0))
    if pcm:
        streams += strl(struct.pack("<4s4sIHHIIIIIIIIhhhh", b"auds", b"\0" * 4, 0, 0, 0, 0, 4, XA_HZ * 4, 0,
                                    len(pcm) // 4, 0, 0xFFFFFFFF, 4, 0, 0, 0, 0),
                        struct.pack("<HHIIHHH", 1, 2, XA_HZ, XA_HZ * 4, 4, 16, 0))
    avih = struct.pack("<14I", round(1e6 / FPS), 0, 0, 0x10, slots, 0, 2 if pcm else 1, 0, width, height, 0, 0, 0, 0)
    audio_per_slot = XA_HZ // FPS * 4
    with path.open("wb") as f:
        f.write(b"RIFF\0\0\0\0AVI " + chunk(b"LIST", b"hdrl" + chunk(b"avih", avih) + streams) + b"LIST\0\0\0\0movi")
        movi = f.tell() - 4
        index = []
        video = iter(video)
        for slot in range(max(slots, -(-len(pcm) // audio_per_slot))):
            pieces = []
            if slot * audio_per_slot < len(pcm):
                pieces.append((b"01wb", pcm[slot * audio_per_slot:(slot + 1) * audio_per_slot]))
            if slot < slots:
                pieces.append((b"00dc", next(video)))
            for tag, data in pieces:
                index.append(struct.pack("<4sIII", tag, 0x10 if data else 0, f.tell() - movi, len(data)))
                f.write(chunk(tag, data))
        end = f.tell()
        f.write(chunk(b"idx1", b"".join(index)))
        size = f.tell()
        f.seek(4)
        f.write(struct.pack("<I", size - 8))
        f.seek(movi - 4)
        f.write(struct.pack("<I", end - movi))


def convert(job: tuple[Path, Path, tuple[bytes, bytes]]) -> str | None:
    """Write <out>/<name>.avi and .cam for one movie file; None if it isn't a movie."""
    path, out, tables = job
    data = path.read_bytes()
    size = 2352 if data.startswith(SYNC) else 2048  # video-only movies extract as plain 2048-byte sectors
    first, parts, audio = {}, {}, []
    for n in range(len(data) // size):
        sector = data[n * size:(n + 1) * size]
        if size == 2352 and sector[18] & 4:
            if sector[19] & ~0x40 != 1:  # bit 6 is emphasis, which xa_pcm undoes
                raise ValueError(f"{path.name}: XA coding {sector[19]:#x} isn't stereo 37.8 kHz 4-bit")
            audio.append(sector)
            continue
        user = sector[24:] if size == 2352 else sector
        if not user.startswith(STR_MAGIC):
            continue
        part, _, number, _, width, height = struct.unpack_from("<HHIIHH", user, 4)
        first.setdefault(number, n)
        parts.setdefault(number, {})[part] = user[32:2048]
    if not first:
        return None

    numbers = sorted(first)
    payloads = [b"".join(p for _, p in sorted(parts[n].items())) for n in numbers]
    (out / f"{path.stem}.cam").write_bytes(b"".join(p[:CAMERA_BYTES] for p in payloads))
    frame_at, slot = {}, -1
    for i, n in enumerate(numbers):
        slot = max(slot + 1, round((first[n] - first[numbers[0]]) / SECTORS_PER_FRAME))
        frame_at[slot] = i
    blocks = width // 16 * (height // 16) * 6
    video = (jpeg_frame(mdec_blocks(payloads[frame_at[s]][CAMERA_BYTES:], blocks, tables), width, height)
             if s in frame_at else b"" for s in range(slot + 1))
    pcm = xa_pcm(audio)
    write_avi(out / f"{path.stem}.avi", width, height, slot + 1, video, pcm)
    return (f"{path.stem}: {len(numbers)} frames over {(slot + 1) / FPS:.1f} s"
            + (f", audio {len(pcm) / 4 / XA_HZ:.1f} s" if pcm else ""))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("assets", type=Path)
    ap.add_argument("-o", "--out", type=Path, default=Path("assets/movies"))
    args = ap.parse_args()
    exe = next(args.assets.glob("*/SCUS_941.63")).read_bytes()
    at = MDEC_TABLES - EXE_BASE
    tables = (exe[at:at + 64], exe[at + 64:at + 128])
    movies = {}
    for path in sorted(args.assets.glob("*/MOVIE/*")):
        movies.setdefault(path.name, path)
    args.out.mkdir(parents=True, exist_ok=True)
    jobs = [(p, args.out, tables) for p in sorted(movies.values(), key=lambda p: -p.stat().st_size)]  # longest first
    with Pool() as pool:
        for line in pool.imap_unordered(convert, jobs):
            if line:
                print(line)
    print(f"-> {args.out}")


if __name__ == "__main__":
    main()
