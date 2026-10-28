import struct

from akao import blocks, channel_starts, disassemble, play_channel, song_to_midi

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

# Playback: tempo, a 3-pass repeat whose 3rd pass jumps out early (F0), a tied note, then an endless jump back.
#   0 E8 lo hi | 3 C8 | 4 note C quarter (0x02) | 5 F0 03 +1: on pass 3 skip the rest at 9 | 9 rest quarter (0x91)
#   10 C9 03 | 12 note C# half (0x0C: 12 // 11 = semitone 1) + tie quarter (0x86) | 14 EE -5: back to offset 12
seq = bytes([0xE8, 0x00, 0x40, 0xC8, 0x02, 0xF0, 0x03, 0x01, 0x00, 0x91, 0xC9, 0x03, 0x0C, 0x86, 0xEE, 0xFB, 0xFF])
song = b"AKAO" + struct.pack("<HHH", 7, 6 + len(seq), 0) + bytes(6) + struct.pack("<IH", 1, 0) + seq
events, loop_tick, end = play_channel(song, 0x16)
notes = [(t, key, length) for t, kind, *rest in events if kind == "note" for key, length in [rest]]
# Passes 1-2: note + rest (96 ticks each); pass 3: note then F0 jumps over the rest to C9, which ends the repeat.
assert notes[:3] == [(0, 60, 48), (96, 60, 48), (192, 60, 48)]
assert notes[3] == (240, 61, 96 + 48)  # C# half note tied to a quarter
assert (loop_tick, end) == (240, 384)
assert [e for e in events if e[1] == "tempo"] == [[0, "tempo", 0x4000]]
midi, loop_lengths, runaway = song_to_midi(song)
assert midi[:4] == b"MThd" and struct.unpack(">HHH", midi[8:14]) == (1, 2, 48) and not runaway
assert loop_lengths == [144]
print("ok")
