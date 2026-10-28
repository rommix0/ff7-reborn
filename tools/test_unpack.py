from unpack import lzs

# Three literals, then a reference back to them (window start 0xFEE) that overlaps its own output,
# then a reference into the zero-primed part of the window.
body = bytes([0b00000111]) + b"abc" + bytes([0xEE, 0xF3, 0x00, 0x00])
assert lzs(len(body).to_bytes(4, "little") + body) == b"abcabcabc" + b"\0\0\0"
print("ok")
