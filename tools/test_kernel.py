from kernel import decode, encode, pack_kernel, pack_text, read_text
from unpack import bingz

# Characters are Mac OS Roman less 0x20 (the order of WINDOW.BIN's font): every byte below 0xE0 survives a round trip,
# and the ones that would read ambiguously in a text file ({, DEL, the no-break space, the Apple logo) come out as hex.
assert encode("Ä…“·") == bytes([0x60, 0xA9, 0xB2, 0xC1])
assert decode(bytes(range(0xE0))).count("{") == 4 and encode(decode(bytes(range(0xE0)))) == bytes(range(0xE0))
assert decode(b"\x5b") == "{5B}"

# Codes take their arguments: EA-F1 two (the battle text's inserts), F8 and FA-FE one (colour, glyph pages).
assert decode(bytes([0x33, 0x54, 0x4F, 0x4C, 0x45, 0x00, 0xEB, 0x00, 0x05, 0x01])) == "Stole {EB 00 05}!"
assert decode(bytes([0xF8, 0x02, 0x21, 0xE7])) == "{F8 02}A{E7}"
assert encode("{F8 02}A{E7}") == bytes([0xF8, 0x02, 0x21, 0xE7])

# A section is a u16 offset per string, then the strings, each ending in FF. Repeats come back as F9 references
# (length 4-10, from 1-64 bytes back), which must never start inside a code's arguments or copy an F9 of their own.
strings = [encode(s) for s in ["Restores HP", "Restores HP", "", "Restores MP", "", "{EB 00 05}{EB 00 05}{EB 00 05}",
                               "Cures Poison Cures Poison", "{F8 02}Restores HP Restores"]]
section = pack_text(strings)
assert read_text(section) == strings
assert len(section) < 2 * len(strings) + sum(len(s) + 1 for s in strings)
assert section[:2] == section[2:4] and section[4:6] == section[8:10]  # identical strings share one copy

# The battle text has its inserts filled in (func_80014E74) before the references are unpacked, with positions counted
# in the packed string, so nothing after an insert may be a reference.
inserted = pack_text([encode("Restores HP"), encode("{EB 00 05}Restores HP")])
assert 0xF9 not in inserted[inserted.index(0xEB):]

# Names are copied without unpacking (func_80014D58), so their sections are packed without references.
plain = pack_text(strings, compress=False)
assert read_text(plain) == strings and 0xF9 not in plain

# A copy that brings an earlier string's FF ends the text there (the battle text has them), but an argument of FF
# doesn't: {EA FF FF} is the battle text's "LIMIT BREAK" name.
assert read_text(bytes([0x04, 0x00, 0x08, 0x00, 0x21, 0x22, 0x23, 0xFF, 0x24, 0xF9, 0x03, 0xFF])) == [b"\x21\x22\x23",
                                                                                                  b"\x24\x22\x23"]
assert read_text(bytes([0x02, 0x00, 0xEA, 0xFF, 0xFF, 0x21, 0xFF])) == [b"\xEA\xFF\xFF\x21"]

# An offset table that starts on an odd byte (section 9's first entry points into the table) still counts its strings.
assert len(read_text(bytes([0x03, 0x00, 0x05, 0x00, 0x21, 0xFF]))) == 2

# A BIN-GZ archive packs back to the same sections, ending in a zero length.
sections = [(0, bytes(range(256))), (9, section)]
packed = pack_kernel(sections)
assert bingz(packed) == sections and packed.endswith(b"\0\0")
print("ok")
