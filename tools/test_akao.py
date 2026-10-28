import struct

from akao import blocks, channel_starts, disassemble, play_channel, play_song, song_to_midi

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
assert notes[:3] == [(0, 36, 48), (96, 36, 48), (192, 36, 48)]  # no A5: driver default octave 2
assert notes[3] == (240, 37, 96 + 48)  # C# half note tied to a quarter
assert (loop_tick, end) == (240, 384)
assert [e for e in events if e[1] == "tempo"] == [[0, "tempo", 0x4000]]
midi, loop_lengths, runaway = song_to_midi(song)
assert midi[:4] == b"MThd" and struct.unpack(">HHH", midi[8:14]) == (1, 2, 48) and not runaway
assert loop_lengths == [144]


def one_channel(seq: bytes) -> bytes:
    return b"AKAO" + struct.pack("<HHH", 9, 6 + len(seq), 0) + bytes(6) + struct.pack("<IH", 1, 0) + seq


def at(events, kind):
    return [(e[0], *e[2:]) for e in events if e[1] == kind]


# Channel defaults from the driver's channel reset: instrument 5, octave 2, volume 100, A3 multiplier 120.
events, *_ = play_channel(one_channel(bytes([0x02, 0xA0])), 0x16)
assert at(events, "program") == [(0, 5)] and at(events, "volume") == [(0, 100)]
assert at(events, "expression") == [(0, 120)] and at(events, "note") == [(0, 36, 48)]

# Drum-kit mode: EC points at 5-byte entries per semitone (instrument, key, u16 volume, pan); ED switches it off.
#   0 EC +4 -> map at 7 | 3 note semitone 1 (0x0B), quarter: 0x0B + 2 = 0x0D | 4 ED | 5 note C | 6 A0 | 7 map
drum_map = bytes([0, 0, 0, 0, 0]) + bytes([9, 60, 0x80, 0x3F, 32])  # semitone 1: instr 9, key 60, vol 0x3F80, pan 32
events, *_ = play_channel(one_channel(bytes([0xEC, 0x04, 0x00, 0x0D, 0xED, 0x02, 0xA0]) + drum_map), 0x16)
assert at(events, "program") == [(0, 5), (0, 9)]
assert at(events, "volume")[-1] == (0, 127) and at(events, "pan") == [(0, 32)]
assert at(events, "note")[0] == (0, 72, 48)  # drum key 60 is octave 5 C: one octave up like A5's numbering

# A9 n v slides the volume to v over n ticks; A3 n sets the multiplier.
events, *_ = play_channel(one_channel(bytes([0xA3, 0x40, 0xA8, 0x00, 0xA9, 0x04, 0x40, 0x02, 0xA0])), 0x16)
assert at(events, "expression")[-1] == (0, 64)
assert at(events, "volume")[2:] == [(1, 16), (2, 32), (3, 48), (4, 64)]

# Channels loop independently. Channel 0 loops a half note from tick 0; channel 1 plays a quarter-note intro and
# then loops a half note. The song repeats from tick 48 (the later loop start) every 96 ticks, so channel 0 must
# keep looping until 48 + 96 = 144 and everything stops there.
ch0 = bytes([0x01, 0xEE, 0xFC, 0xFF])        # note C half, EE -4: back to the note
ch1 = bytes([0x02, 0x0C, 0xEE, 0xFC, 0xFF])  # note C quarter, note C# half, EE -4: back to the C#
table = struct.pack("<IHH", 0b11, 2, len(ch0))
two = b"AKAO" + struct.pack("<HHH", 9, 8 + len(ch0) + len(ch1), 0) + bytes(6) + table + ch0 + ch1
channels, loop_start, loop_end, loop_lengths, runaway = play_song(two)
assert (loop_start, loop_end, loop_lengths, runaway) == (48, 144, [96, 96], False)
assert [e[0] for e in channels[0] if e[1] == "note"] == [0, 96]  # second pass of channel 0's loop
assert [(e[0], e[3]) for e in channels[1] if e[1] == "note"] == [(0, 48), (48, 96)]
print("ok")
