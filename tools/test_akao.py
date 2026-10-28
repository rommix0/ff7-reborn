import struct

from akao import bar_lines, blocks, channel_starts, disassemble, play_channel, play_song, song_to_midi

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
# Lengths count to key-off, 2 ticks before the next event (see "Articulation" below).
assert notes[:3] == [(0, 36, 46), (96, 36, 46), (192, 36, 46)]  # no A5: driver default octave 2
assert notes[3] == (240, 37, 96 + 48 - 2)  # C# half note tied to a quarter
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
assert at(events, "expression") == [(0, 120)] and at(events, "note") == [(0, 36, 46)]

# Drum-kit mode: EC points at 5-byte entries per semitone (instrument, key, u16 volume, pan); ED switches it off.
#   0 EC +4 -> map at 7 | 3 note semitone 1 (0x0B), quarter: 0x0B + 2 = 0x0D | 4 ED | 5 note C | 6 A0 | 7 map
drum_map = bytes([0, 0, 0, 0, 0]) + bytes([9, 60, 0x80, 0x3F, 32])  # semitone 1: instr 9, key 60, vol 0x3F80, pan 32
events, *_ = play_channel(one_channel(bytes([0xEC, 0x04, 0x00, 0x0D, 0xED, 0x02, 0xA0]) + drum_map), 0x16)
assert at(events, "program") == [(0, 5), (0, 9)]
assert at(events, "volume")[-1] == (0, 127) and at(events, "pan") == [(0, 32)]
assert at(events, "note")[0] == (0, 72, 46)  # drum key 60 is octave 5 C: one octave up like A5's numbering

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
assert [(e[0], e[3]) for e in channels[1] if e[1] == "note"] == [(0, 46), (48, 94)]
# With a time signature the loop moves to the next bar line: channel 0 sets 2/4 (96-tick bars), so the loop that
# could start at tick 48 starts at 96 instead, and runs one period (96 ticks) from there.
ch0 = bytes([0xFD, 0x30, 0x02, 0x01, 0xEE, 0xFC, 0xFF])  # 2/4, note C half, EE -4: back to the note
table = struct.pack("<IHH", 0b11, 2, len(ch0))
two = b"AKAO" + struct.pack("<HHH", 9, 8 + len(ch0) + len(ch1), 0) + bytes(6) + table + ch0 + ch1
channels, loop_start, loop_end, loop_lengths, runaway = play_song(two)
assert (loop_start, loop_end) == (96, 192)
assert [(e[0], e[3]) for e in channels[1] if e[1] == "note"] == [(0, 46), (48, 94), (144, 48)]  # cut at 192
assert bar_lines([(0, 192), (2, 144), (146, 120)], 400) == [0, 2, 146, 266, 386]  # FD restarts the bar count

# Pitch: C0 transposes keys, D8 sets detune, and A4 n s bends by s semitones over n ticks, starting at the next
# tie (one-shot). Song 7 channel 16 does exactly this: 2-tick note, A4 04 01, 10-tick tie.
events, *_ = play_channel(one_channel(bytes([
    0xC0, 0x02, 0xD8, 0xFE, 0xA2, 0x02, 0x02,  # transpose +2, detune -2, note C with length 2
    0xA4, 0x04, 0x01, 0xA2, 0x0A, 0x86,        # bend +1 semitone over 4 ticks, tie for 10 ticks
    0xA2, 0x0A, 0x86, 0xA0])), 0x16)           # a second tie: the bend was one-shot
assert at(events, "note") == [(0, 38, 20)]  # C of octave 2 (36) + 2; 2 + 10 + 10 ticks, last key-off 2 early
assert at(events, "detune") == [(0, -2)] and at(events, "bend") == [(2, 4, 1)]

# LFOs: B4 delay rate waveform turns vibrato on, B5 sets its depth, B6 turns it off but keeps the depth; B8/B9/BA
# do the same for tremolo. Events carry (delay, rate, waveform, depth), depth 0 while off; rate 0 means 256.
events, *_ = play_channel(one_channel(bytes([0xB4, 0x60, 0x01, 0x06, 0xB5, 0x0A, 0x02, 0xB6, 0xB4, 0x00, 0x00, 0x07,
                                             0xB8, 0x00, 0x03, 0x05, 0xB9, 0x78, 0x02, 0xA0])), 0x16)
assert at(events, "vibrato") == [(0, 0x60, 1, 6, 0), (0, 0x60, 1, 6, 10), (48, 0x60, 1, 6, 0), (48, 0, 256, 7, 10)]
assert at(events, "tremolo") == [(48, 0, 3, 5, 0), (48, 0, 3, 5, 0x78)]

# FD t n: time signature, n beats of t ticks; the driver restarts its beat count there, so it marks a bar line.
events, *_ = play_channel(one_channel(bytes([0xFD, 0x18, 0x06, 0x01, 0xFD, 0x18, 0x05, 0xA0])), 0x16)
assert at(events, "meter") == [(0, 24, 6), (96, 24, 5)]
# Articulation, as seq_step_channel sets each note's key-off countdown. The length table holds (key-off << 8 |
# length); a normal note keys off 2 ticks early. Notes before a tie, in CC slur mode or in D0 mode are held to the end.
events, *_ = play_channel(one_channel(bytes([0x02, 0x02, 0xA0])), 0x16)
assert at(events, "note") == [(0, 36, 46), (48, 36, 46)]
events, *_ = play_channel(one_channel(bytes([0x02, 0x86, 0xA0])), 0x16)
assert at(events, "note") == [(0, 36, 94)]  # the tie segment keys off early, the first one doesn't
# CC slurs: n1 is held into n2, which takes over the sounding voice without restarting it. The driver ends the slur
# by looking ahead from each note: a CD (or a rest) before the next note clears it, so n2 keys off early and n3
# starts fresh. D0 holds notes to full length the same way, without slurring; D1 ends it.
events, *_ = play_channel(one_channel(bytes([0xCC, 0x02, 0x02, 0xCD, 0x02, 0xA0])), 0x16)
assert at(events, "note") == [(0, 36, 48), (48, 36, 46), (96, 36, 46)] and at(events, "slur") == [(48,)]
events, *_ = play_channel(one_channel(bytes([0xD0, 0x02, 0x02, 0xD1, 0x02, 0xA0])), 0x16)
assert at(events, "note") == [(0, 36, 48), (48, 36, 46), (96, 36, 46)] and at(events, "slur") == []
# A rest ends a slur too, and the look-ahead follows loops: here the EE jump lands on a note, so n1 is held.
events, *_ = play_channel(one_channel(bytes([0xCC, 0x02, 0x8F, 0x02, 0xA0])), 0x16)
assert at(events, "note")[0] == (0, 36, 46) and at(events, "slur") == []
events, *_ = play_channel(one_channel(bytes([0xD0, 0x02, 0xEE, 0xFC, 0xFF])), 0x16)
assert at(events, "note")[0] == (0, 36, 48)

# AD AE AF B1 B2 override the channel's attack rate, decay rate, sustain level, sustain rate and release rate.
events, *_ = play_channel(one_channel(bytes([0xA1, 0x03, 0xAD, 0x28, 0xAE, 0x0E, 0xAF, 0x05, 0xB1, 0x40, 0xB2, 0x12,
                                             0x02, 0xA0])), 0x16)
assert [(e[1], e[2]) for e in events if e[1] in ("ar", "dr", "sl", "sr", "rr")] == [
    ("ar", 0x28), ("dr", 0x0E), ("sl", 0x05), ("sr", 0x40), ("rr", 0x12)]
print("ok")
