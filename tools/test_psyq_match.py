import struct
import tempfile
from pathlib import Path

from psyq_match import find, masked, parse_obj, ref_exe_funcs

# Synthetic SN object: one .text section with an exported `caller` whose jal is relocated.
code = struct.pack("<4I", 0x27BDFFE8, 0x0C000000, 0x00000000, 0x03E00008)
obj = (
    b"LNK\x02"
    + b"\x2e\x07"                                            # cpu R3000
    + b"\x10" + struct.pack("<HHB", 0xF002, 0, 8) + b"\x05.text"
    + b"\x0c" + struct.pack("<HHI", 1, 0xF002, 0) + b"\x06caller"
    + b"\x06" + struct.pack("<H", 0xF002)
    + b"\x02" + struct.pack("<H", len(code)) + code
    + b"\x0a\x4a" + struct.pack("<H", 4) + b"\x02" + struct.pack("<H", 2)  # jal -> symbol 2
    + b"\x00"
)
sections, symbols = parse_obj(obj)
assert symbols == [("caller", 0xF002, 0)]
name, text_code, patches = sections[0xF002]
assert name == ".text" and bytes(text_code) == code and patches == [(4, 0x4A)]

value, mask = masked(bytes(text_code), patches)
# The same function in an exe, with a different jal target, found at offset 8 only.
exe = bytes(8) + struct.pack("<4I", 0x27BDFFE8, 0x0C012345, 0, 0x03E00008) + bytes(8)
assert find(exe, value, mask) == [8]

# Reference EXE (no relocations): lui + load through $at, a jal, then return. Linked addresses must be masked.
body = [0x3C018005, 0x8C221234, 0x0C008000, 0x00000000, 0x24420001, 0x24420002, 0x03E00008, 0x00000000]
header = bytearray(0x800)
header[0:8] = b"PS-X EXE"
struct.pack_into("<I", header, 0x18, 0x80010000)
with tempfile.TemporaryDirectory() as tmp:
    ref, nm = Path(tmp, "ref.exe"), Path(tmp, "ref.nm")
    ref.write_bytes(bytes(header) + struct.pack(f"<{len(body)}I", *body))
    nm.write_text("80010000 T getter\n")
    [(name, _, value, mask)] = ref_exe_funcs(ref, nm)
assert name == "getter"
assert struct.unpack("<8I", mask)[:3] == (0xFFFF0000, 0xFFFF0000, 0xFC000000)
other = [0x3C018007, 0x8C225678, 0x0C00ABCD] + body[3:]
assert find(struct.pack("<8I", *other), value, mask) == [0]
print("ok")
