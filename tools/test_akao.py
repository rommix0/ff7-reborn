import struct

from akao import blocks, channel_starts, disassemble

# Two channels: ch0 = octave, note C/len0, tie, rest, loop-forever; ch1 = volume, end.
ch0 = bytes([0xA5, 0x04, 0x00, 0x84, 0x8F, 0xCA])
ch1 = bytes([0xA8, 0x7F, 0xA0])
table = struct.pack("<I", 0b11) + struct.pack("<HH", 2, len(ch0))  # each entry relative to the byte after itself
body = table + ch0 + ch1
block = b"AKAO" + struct.pack("<HHH", 1, len(body), 0) + bytes(6) + body

[(offset, found)] = list(blocks(b"junk" + block))
assert offset == 4 and found == block
assert channel_starts(found) == [0x18, 0x18 + len(ch0)]
events, problem = disassemble(found, 0x18, len(found))
assert problem is None
assert [k for _, k, _ in events] == ["cmd", "note", "tie", "rest", "cmd"]
assert events[0][2] == bytes([0xA5, 0x04]) and events[-1][2] == bytes([0xCA])
events, problem = disassemble(found, 0x18 + len(ch0), len(found))
assert problem is None and events[-1][2] == bytes([0xA0])
# A stray "AKAO" with a nonsense header is not a block.
assert list(blocks(b"AKAO" + bytes(4))) == []
print("ok")
