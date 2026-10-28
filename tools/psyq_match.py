"""Find PsyQ library functions in a PS-X EXE by matching the code in SN .LIB archives (relocated bits masked).

Prints splat symbol lines (`name = 0xADDR; // type:func`) grouped under `// LIBNAME.MODULE` comments. Only names
and addresses come out; no SDK code is copied.
"""
import argparse
import struct
from pathlib import Path

# Patch types seen in the libs and which bits of the target word they rewrite.
PATCH_MASKS = {0x10: 0xFFFFFFFF, 0x4A: 0x03FFFFFF, 0x52: 0xFFFF, 0x54: 0xFFFF, 0x64: 0xFFFF}
MIN_SOLO_WORDS = 10


def pstring(d: bytes, p: int) -> tuple[str, int]:
    return d[p + 1:p + 1 + d[p]].decode("latin-1"), p + 1 + d[p]


def skip_expr(d: bytes, p: int) -> int:
    op = d[p]
    p += 1
    if op == 0x00:
        return p + 4
    if op in (0x02, 0x04, 0x06, 0x0C, 0x16):
        return p + 2
    # Everything from 0x20 up is a binary operator over two sub-expressions.
    if op >= 0x20:
        return skip_expr(d, skip_expr(d, p))
    raise ValueError(f"unknown expression op {op:#x} at {p - 1:#x}")


def parse_obj(d: bytes):
    """Return (sections, symbols): sections {id: (name, bytearray code, set of masked (offset, mask))}."""
    assert d[:4] == b"LNK\x02"
    p = 4
    sections, symbols, current = {}, [], None
    while True:
        op = d[p]
        p += 1
        if op == 0x00:
            return sections, symbols
        if op == 0x02:  # code bytes for the current section
            n = struct.unpack_from("<H", d, p)[0]
            sections[current][1].extend(d[p + 2:p + 2 + n])
            p += 2 + n
        elif op == 0x06:  # switch section
            current = struct.unpack_from("<H", d, p)[0]
            p += 2
        elif op == 0x08:  # uninitialised space
            sections[current][1].extend(bytes(struct.unpack_from("<I", d, p)[0]))
            p += 4
        elif op == 0x0A:  # patch: type, offset in current section, expression
            kind, off = d[p], struct.unpack_from("<H", d, p + 1)[0]
            sections[current][2].append((off, kind))
            p = skip_expr(d, p + 3)
        elif op == 0x0C:  # exported symbol
            _, sec, off = struct.unpack_from("<HHI", d, p)
            name, p = pstring(d, p + 8)
            symbols.append((name, sec, off))
        elif op == 0x0E:  # imported symbol
            _, p = pstring(d, p + 2)
        elif op == 0x10:  # section definition
            sec = struct.unpack_from("<H", d, p)[0]
            name, p = pstring(d, p + 5)
            sections[sec] = (name, bytearray(), [])
        elif op == 0x12:  # local symbol
            _, p = pstring(d, p + 6)
        elif op == 0x1C:  # source file name
            _, p = pstring(d, p + 2)
        elif op == 0x2E:  # cpu type
            p += 1
        elif op == 0x30:  # common (bss) symbol
            _, p = pstring(d, p + 8)
        else:
            raise ValueError(f"unknown record {op:#x} at {p - 1:#x}")


def lib_modules(path: Path):
    d = path.read_bytes()
    assert d[:4] == b"LIB\x01", path
    p = 4
    while p < len(d):
        name = d[p:p + 8].decode("latin-1").strip()
        hdr, size = struct.unpack_from("<II", d, p + 12)
        yield name, d[p + hdr:p + size]
        p += size


def masked(code: bytes, patches) -> tuple[bytes, bytes]:
    """Return (value, mask) byte strings with relocated bits zeroed in both."""
    mask = bytearray(b"\xff" * len(code))
    for off, kind in patches:
        m = PATCH_MASKS.get(kind, 0xFFFFFFFF)
        word_off = off & ~3
        old = struct.unpack_from("<I", mask, word_off)[0]
        struct.pack_into("<I", mask, word_off, old & ~m)
    value = bytes(c & m for c, m in zip(code, mask))
    return value, bytes(mask)


def distinctive(mask: bytes) -> bool:
    """Enough fixed code to trust a unique hit. Tiny wrappers like atof() -> strtod(s, 0) match any game function
    of the same shape once the call target is masked; relocation-free stubs (BIOS calls) are fine as they are."""
    words = len(mask) // 4
    fixed = mask.count(b"\xff\xff\xff\xff")
    return fixed >= MIN_SOLO_WORDS or (fixed == words and words >= 4)


def find(text: bytes, value: bytes, mask: bytes) -> list[int]:
    # Anchor on the longest run of fully unmasked bytes, then verify the rest.
    best, run_start, best_start = 0, 0, 0
    for i in range(len(mask) + 1):
        if i == len(mask) or mask[i] != 0xFF:
            if i - run_start > best:
                best, best_start = i - run_start, run_start
            run_start = i + 1
    if best < 8:
        return []
    anchor = value[best_start:best_start + best]
    hits, pos = [], text.find(anchor)
    while pos >= 0:
        start = pos - best_start
        if start >= 0 and start % 4 == 0 and all(
            (text[start + i] & mask[i]) == value[i] for i in range(len(value))
        ):
            hits.append(start)
        pos = text.find(anchor, pos + 1)
    return hits


def fuzzy_candidates(text: bytes, t_addr: int, named: set[int], sdk_funcs, threshold: float):
    """Best SDK look-alike for each unnamed call target between the first and last named library function."""
    targets = set()
    for i in range(0, len(text) - 3, 4):
        word = struct.unpack_from("<I", text, i)[0]
        if word >> 26 == 3:  # jal
            targets.add((t_addr & 0xF0000000) | (word & 0x03FFFFFF) << 2)
    starts = sorted(targets | named)
    lo, hi = min(named), max(named)
    for addr, end in zip(starts, starts[1:]):
        if addr in named or not lo <= addr <= hi:
            continue
        ours = text[addr - t_addr:end - t_addr]
        best = (0.0, None)
        for name, origin, value, mask in sdk_funcs:
            # Library functions grow or shrink a little between SDK versions; compare only similar sizes.
            if not 0.8 <= len(value) / len(ours) <= 1.25:
                continue
            n = min(len(ours), len(value)) // 4
            same = sum(
                struct.unpack_from("<I", ours, i * 4)[0] & struct.unpack_from("<I", mask, i * 4)[0]
                == struct.unpack_from("<I", value, i * 4)[0]
                for i in range(n)
            )
            score = same / (max(len(ours), len(value)) // 4)
            if score > best[0]:
                best = (score, (name, origin))
        if best[0] >= threshold:
            yield addr, best[0], *best[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("exe", type=Path)
    ap.add_argument("libs", type=Path, nargs="+")
    ap.add_argument("--fuzzy", type=float, metavar="RATIO",
                    help="also list unnamed library-area functions whose code is at least RATIO similar (for review)")
    args = ap.parse_args()

    exe = args.exe.read_bytes()
    t_addr = struct.unpack_from("<I", exe, 0x18)[0]
    text = exe[0x800:]
    found = {}  # addr -> set of (name, origin)
    sdk_funcs = []  # (name, origin, value, mask) per function, for --fuzzy
    for lib in args.libs:
        for module, obj in lib_modules(lib):
            sections, symbols = parse_obj(obj)
            for sec_id, (sec_name, code, patches) in sections.items():
                if sec_name != ".text" or len(code) < 8:
                    continue
                value, mask = masked(bytes(code), patches)
                funcs = sorted((off, name) for name, sec, off in symbols if sec == sec_id)
                origin = f"{lib.stem}.{module}"
                for (off, name), end in zip(funcs, [o for o, _ in funcs[1:]] + [len(code)]):
                    sdk_funcs.append((name, origin, value[off:end], mask[off:end]))
                hits = find(text, value, mask)
                if len(hits) == 1 and distinctive(mask):
                    for off, name in funcs:
                        found.setdefault(t_addr + hits[0] + off, set()).add((name, origin))
                    continue
                # Older libs often differ in one function of a module, so fall back to matching each one alone.
                strong, weak = [], []
                for (off, name), end in zip(funcs, [o for o, _ in funcs[1:]] + [len(code)]):
                    hits = find(text, value[off:end], mask[off:end])
                    # A function that occurs more than once is too generic to name safely.
                    if len(hits) != 1:
                        continue
                    # Weak hits only count when a distinctive sibling agrees on where the module sits.
                    (strong if distinctive(mask[off:end]) else weak).append((hits[0] - off, off, name))
                bases = {base for base, _, _ in strong}
                for base, off, name in strong + [w for w in weak if w[0] in bases]:
                    found.setdefault(t_addr + base + off, set()).add((name, origin))
    # Two different names for one address means the code is too generic to tell them apart.
    found = {addr: hits for addr, hits in found.items() if len({name for name, _ in hits}) == 1}

    if args.fuzzy:
        candidates = list(fuzzy_candidates(text, t_addr, set(found), sdk_funcs, args.fuzzy))
        # A name that lands on several addresses is matching a tiny shape (getter/stub), not the real function.
        names = [name for _, _, name, _ in candidates]
        for addr, score, name, origin in candidates:
            if names.count(name) == 1:
                print(f"// {score:.2f} like {origin}: {name} = 0x{addr:08X}; // type:func")
        return

    last_origin = None
    for addr in sorted(found):
        name, origin = min(found[addr])
        # Origin goes on its own comment line: splat reserves `lib:` as an attribute.
        if origin != last_origin:
            print(f"// {origin}")
            last_origin = origin
        print(f"{name} = 0x{addr:08X}; // type:func")


if __name__ == "__main__":
    main()
