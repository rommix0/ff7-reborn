import struct

from psyq_match import find, masked, parse_obj

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
print("ok")
