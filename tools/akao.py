"""Parse AKAO music sequences (PS1 Final Fantasy VII) and check them against the command lengths we derived.

usage:
  python tools/akao.py check <assets dir>     # parse every distinct sequence under the folder, report problems
  python tools/akao.py dump <file> [index]    # disassemble one .SND file or the AKAO blocks in a field .DAT
  python tools/akao.py midi <file|dir> <out>  # export songs as .mid (a folder exports every distinct song)
"""
import math
import struct
import sys
from pathlib import Path

from unpack import lzs

MAGIC = b"AKAO"
HEADER = 0x10
EFFECT_BASE = 0x1000
# Argument bytes per sequence command (0xA0-0xFF), from how far each driver handler moves the read pointer.
# Every song and sound effect on the three discs parses with these. B7 BE BF CE CF DF E9 EB EF F1 F3 never occur,
# so their lengths rest on the handler code alone. E0-E7, FA-FC and FF are unused (the driver treats them as A0).
ARGS = {
    **dict.fromkeys([0xA0, 0xA6, 0xA7, 0xB3, 0xB6, 0xBA, 0xBE, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xCA,
                     0xCB, 0xCC, 0xCD, 0xD0, 0xD1, 0xD4, 0xD5, 0xD6, 0xD7, 0xDB, 0xED, 0xF3, 0xF5, 0xF9], 0),
    **dict.fromkeys([0xA1, 0xA2, 0xA3, 0xA5, 0xA8, 0xAA, 0xAC, 0xAD, 0xAE, 0xAF, 0xB1, 0xB2, 0xB5, 0xB7, 0xB9,
                     0xBB, 0xBD, 0xBF, 0xC0, 0xC1, 0xC9, 0xCE, 0xCF, 0xD2, 0xD3, 0xD8, 0xD9, 0xDA, 0xDC, 0xF2,
                     0xF6, 0xF8], 1),
    **dict.fromkeys([0xA4, 0xA9, 0xAB, 0xB0, 0xBC, 0xDD, 0xDE, 0xDF, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xEE, 0xF4,
                     0xF7, 0xFD, 0xFE], 2),
    **dict.fromkeys([0xB4, 0xB8, 0xEF, 0xF0, 0xF1], 3),
}
# Commands after which the stream never continues linearly: end of channel, loop back forever, jump.
STOP = {0xA0, 0xCA, 0xEE}
# Note length table in the main exe (seq_note_lengths); a byte's remainder mod 11 picks one. 48 ticks = quarter note.
NOTE_TICKS = (192, 96, 48, 24, 12, 6, 3, 32, 16, 8, 4)
TICKS_PER_QUARTER = 48
# The driver steps on root counter 2 (system clock / 8) with target 0x43D1, and adds the tempo to a 16-bit
# accumulator each step; every carry is one tick.
DRIVER_HZ = 33_868_800 / 8 / 0x43D1


def blocks(data: bytes):
    """Yield (offset, block bytes) for every AKAO block in a buffer (header length field gives the size).

    "AKAO" also turns up by chance inside compressed data, so a hit must have a sane header and channel table.
    """
    pos = data.find(MAGIC)
    while pos >= 0:
        size = HEADER + struct.unpack_from("<H", data, pos + 6)[0] if pos + 0x18 <= len(data) else 0
        block = data[pos:pos + size]
        if HEADER + 4 < size <= len(data) - pos and struct.unpack_from("<I", block, HEADER)[0] \
                and all(s < size for s in channel_starts(block)):
            yield pos, block
            pos = data.find(MAGIC, pos + size)
        else:
            pos = data.find(MAGIC, pos + 1)


def channel_starts(block: bytes) -> list[int]:
    """Offsets of each channel's data. Each u16 table entry is relative to the byte after itself."""
    mask = struct.unpack_from("<I", block, HEADER)[0]
    starts = []
    for i in range(bin(mask).count("1")):
        field = HEADER + 4 + 2 * i
        if field + 2 > len(block):
            return starts + [len(block)]  # table runs off the block: not a real header
        starts.append(field + 2 + struct.unpack_from("<H", block, field)[0])
    return starts


def disassemble(block: bytes, start: int, end: int):
    """Return (events, problem). Events are (offset, kind, value) with kind 'note', 'tie', 'rest' or 'cmd'."""
    events, pos = [], start
    while pos < end:
        b = block[pos]
        if b < 0xA0:
            kind = "note" if b < 0x84 else "tie" if b < 0x8F else "rest"
            events.append((pos, kind, b))
            pos += 1
            continue
        if b not in ARGS:
            return events, f"unused command {b:02X} at {pos:#x}"
        events.append((pos, "cmd", block[pos:pos + 1 + ARGS[b]]))
        pos += 1 + ARGS[b]
        if b in STOP:
            return events, None if pos <= end else f"{b:02X} args run past {end:#x}"
    return events, f"no end command before {end:#x}"


def effect_starts(data: bytes) -> list[int]:
    """Stream offsets in SOUND/EFFECT.ALL: a 0x1000-byte table of u16 offsets (0xFFFF = empty) relative to 0x1000."""
    offsets = struct.unpack_from("<2048H", data)
    return sorted({EFFECT_BASE + o for o in offsets if o != 0xFFFF})


def effects(data: bytes) -> list[tuple[int, list[int]]]:
    """(effect id, its stream offsets) for every effect in SOUND/EFFECT.ALL. Table entries 2·id and 2·id + 1 are
    the effect's two channels (func_8002A6C4), and an empty entry leaves that channel out."""
    offsets = struct.unpack_from("<2048H", data)
    pairs = [(i, [EFFECT_BASE + o for o in offsets[2 * i:2 * i + 2] if o != 0xFFFF]) for i in range(1024)]
    return [(i, streams) for i, streams in pairs if streams]


def sequences(root: Path):
    """Yield (name, block) for every distinct AKAO block in any extracted file, LZS files decompressed first."""
    seen = set()
    # ponytail: pure-Python LZS on every compressed file, ~3 min for three discs; cache decompressed maps if it matters.
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        data = path.read_bytes()
        if len(data) > 4 and int.from_bytes(data[:4], "little") == len(data) - 4:
            data = lzs(data)
        for off, block in blocks(data):
            if block not in seen:
                seen.add(block)
                yield f"{path.relative_to(root)}@{off:#x}", block


def named_songs(source: Path) -> list[tuple[str, str, bytes]]:
    """(song_NNN name, where it was found, block) for every distinct song in a folder, or in one file."""
    if source.is_dir():
        found = list(sequences(source))
    else:
        data = source.read_bytes()
        if len(data) > 4 and int.from_bytes(data[:4], "little") == len(data) - 4:
            data = lzs(data)
        found = [(f"{source.name}@{off:#x}", block) for off, block in blocks(data)]
    names, songs = set(), []
    for origin, block in found:
        name = f"song_{struct.unpack_from('<H', block, 4)[0]:03d}"
        while name in names:  # same id, different data
            name += "b"
        names.add(name)
        songs.append((name, origin, block))
    return songs


def check_streams(name: str, data: bytes, starts: list[int], problems: dict) -> tuple[int, int]:
    """Parse each stream to the end of `data`; return (clean, ending at the next stream's start)."""
    clean = exact = 0
    # A stream may fall through into the next one's bytes (shared tails), so parse to the end of the buffer.
    for start, next_start in zip(starts, starts[1:] + [len(data)]):
        events, problem = disassemble(data, start, len(data))
        if problem:
            problems.setdefault(problem.split(" at ")[0].split(" before")[0], []).append(f"{name}+{start:#x}")
            continue
        clean += 1
        last_pos, _, last = events[-1]
        # Strict check on argument lengths: most streams end exactly where the next one begins.
        exact += last_pos + len(last) in range(next_start - 3, next_start + 1)
    return clean, exact


def check(root: Path):
    problems = {}
    songs = channels = clean = exact = 0
    for name, block in sequences(root):
        starts = channel_starts(block)
        songs += 1
        channels += len(starts)
        c, e = check_streams(name, block, starts, problems)
        clean, exact = clean + c, exact + e
    print(f"music: {songs} distinct sequences, {channels} channels, {clean} parse cleanly, "
          f"{exact} end within 3 bytes of the next channel")
    for path in sorted(root.rglob("EFFECT.ALL")):
        data = path.read_bytes()
        starts = effect_starts(data)
        c, e = check_streams(str(path.relative_to(root)), data, starts, problems)
        print(f"effects {path.relative_to(root)}: {len(starts)} streams, {c} parse cleanly, {e} end at the next one")
    for problem, names in sorted(problems.items(), key=lambda kv: -len(kv[1])):
        print(f"  {len(names):4d}x {problem}  e.g. {names[0]}")


def show(events) -> str:
    return " ".join(v.hex(" ").upper() if k == "cmd" else f"{k[0]}{v // 11 if k == 'note' else ''}/{v % 11}"
                    for _, k, v in events)


def dump(path: Path, index: int):
    data = path.read_bytes()
    if path.name.upper() == "EFFECT.ALL":
        start = effect_starts(data)[index]
        events, problem = disassemble(data, start, len(data))
        print(f"effect stream {index} @{start:#x}" + (f"  !! {problem}" if problem else ""))
        print("   " + show(events))
        return
    if len(data) > 4 and int.from_bytes(data[:4], "little") == len(data) - 4:
        data = lzs(data)
    off, block = list(blocks(data))[index]
    song_id, length, _ = struct.unpack_from("<HHH", block, 4)
    print(f"AKAO block @{off:#x}: id {song_id:#x}, {length} data bytes, stamp 19{block[10:16].hex()}")
    starts = channel_starts(block)
    for ch, start in enumerate(starts):
        events, problem = disassemble(block, start, len(block))
        print(f"-- channel {ch} @{start:#x}" + (f"  !! {problem}" if problem else ""))
        print("   " + show(events))


MAX_TICKS = 192 * 4000  # 4000 bars: far past any song; reaching it means a loop was misread


def look_ahead(block: bytes, pos: int, loops: list) -> tuple[int, bool]:
    """The driver's scan from just after a note (func_800318BC): the next note, tie or rest byte (0xA0 if the
    channel stops first), and whether a slur or full-length mode ends on the way. Loops and jumps are followed; a
    rest, CB CD D1 DB, or anything that stops the channel ends the mode."""
    ends, level = False, len(loops) - 1
    for _ in range(1000):  # a loop with no note in it would spin forever, in the driver too
        b = block[pos]
        if b < 0xA0:
            return b, ends or 0x8F <= b < 0x9A
        if b in (0xCB, 0xCD, 0xD1, 0xDB):
            ends = True
        elif b == 0xC9 and level >= 0 and loops[level][1] + 1 != block[pos + 1]:
            pos = loops[level][0]  # the repeat goes round again
            continue
        elif b == 0xC9:
            level -= 1
        elif b == 0xCA and level >= 0:
            pos = loops[level][0]
            continue
        elif b == 0xF0 and level >= 0 and loops[level][1] + 1 == (block[pos + 1] or 256):
            pos += 4 + struct.unpack_from("<h", block, pos + 2)[0]
            level -= 1
            continue
        elif b == 0xEE:
            pos += 3 + struct.unpack_from("<h", block, pos + 1)[0]
            continue
        elif b == 0xA0 or b not in ARGS or b in (0xEF, 0xF1):
            return 0xA0, True
        pos += 1 + ARGS[b]
    return 0xA0, True


def play_channel(block: bytes, start: int, max_ticks: int = MAX_TICKS, until: int | None = None, effect: bool = False):
    """Follow one channel like the driver does, once through. Returns (events, loop_start_tick, end_tick).

    Events are [tick, kind, ...]: ("note", key, ticks until key-off), ("slur",) just before a note that takes over
    the sounding voice instead of restarting it, ("program", n), ("volume", n), ("expression", n),
    ("pan", n), ("tempo", t), ("detune", d), ("bend", ticks, semitones), ("vibrato" / "tremolo", delay, rate,
    waveform, depth), ("meter", ticks per beat, beats per bar), and the channel's own envelope settings ("ar", "dr",
    "sl", "sr", "rr", n) until the next program change, ("overlap", n) for F8 (two voices taking turns so each
    note rings on in its release, release rate n kept across program changes) and ("overlap", None) for F9;
    ("pan_lfo", ...) like the other LFOs (BC/BD), ("vibrato_slide" / "tremolo_slide", ticks, depth) (DD/DE),
    ("fm", on) (C6/C7), ("layer", [a, b]) / ("layer", None) (F4/F5: instrument b on a second voice under a),
    ("balance", n) and ("balance_slide", ticks, n) (F6/F7: main voice at (127 - n)/256, layer at n/256). Volume and pan slides become one event per tick that changes the value. Detune d scales the
    pitch by 1 + d/128 (d > 0) or 1 + d/256 (d < 0); a bend slides the pitch linearly in frequency from the held
    note to `semitones` above it. An LFO event gives the settings from then on, depth 0 while it is off.
    Counted repeats (C8/C9) are unrolled; an endless loop (CA, C9 0, EE backwards) ends the pass and reports
    the tick its target was first reached at. With `until`, the endless loop is followed instead, up to that tick.
    `effect` plays a sound-effect channel, whose flag +0x54 makes B4/B8 take no delay and a non-zero first byte
    as the depth.
    """
    pos, tick = start, 0
    # The driver's channel reset: instrument 5, octave 2, volume 100, A3 multiplier 120.
    octave, fixed_len, next_len, last_len = 2, 0, 0, 0
    levels = {"volume": 100.0, "pan": 64.0}
    slides = {}  # kind -> [step per tick, ticks left]
    drum_map = None
    transpose, detune, bend = 0, 0, None
    held_transpose = 0  # the transpose in the sounding pitch; a bend aims at its key + s + the transpose now
    lfos = {kind: [0, 1, 0, False, 0] for kind in ("vibrato", "tremolo", "pan_lfo")}  # delay rate waveform on depth
    # The driver's note mode (+0x6E): bit 0 slur (CC), bit 1 "this note slurs from the last", bit 2 full length (D0).
    mode, cut = 0, 0
    glide, last_key = 0, 0  # DA's glide ticks (+0x6C) and the last note's key (+0x6A), 0 = none
    fm, fm_due = False, None  # FM on, and the tick D2/D3's countdown (+0xA6) flips it
    loops = []  # [loop start pos, pass count]; the driver keeps 4 levels
    first_seen, last_note = {}, None
    events = [[0, "program", 5], [0, "volume", 100], [0, "expression", 120]]

    def set_level(kind, value):
        levels[kind] = value
        slides.pop(kind, None)  # A8/AA (and drum entries) cancel a running slide
        events.append([tick, kind, round(value)])

    def advance(ticks):
        nonlocal tick, fm, fm_due
        for _ in range(ticks):
            tick += 1
            if tick == fm_due:
                fm, fm_due = not fm, None
                events.append([tick, "fm", fm])
            for kind, slide in list(slides.items()):
                before = round(levels[kind])
                levels[kind] += slide[0]
                slide[1] -= 1
                if not slide[1]:
                    del slides[kind]
                if round(levels[kind]) != before:
                    events.append([tick, kind, round(levels[kind])])

    loop_tick = None
    while tick < max_ticks:
        if until is not None and tick >= until:
            return events, loop_tick, tick
        first_seen.setdefault(pos, tick)
        b = block[pos]
        if b < 0xA0:
            # Length precedence as in seq_step_channel: DC fixed length, else A2 one-shot length, else the table.
            last_len = fixed_len or next_len or NOTE_TICKS[b % 11]
            next_len = 0
            semitone = b // 11
            following, ends = look_ahead(block, pos + 1, loops)
            if ends:
                mode &= ~5
                glide = 0
            # Key-off comes 2 ticks before the next event, unless a tie follows or slur/full-length mode is on.
            early = 2 if last_len > 2 and not 0x84 <= following < 0x8F and not mode & 5 else 0
            if semitone < 12 and mode & 2:
                events.append([tick, "slur"])
            if semitone < 12 and drum_map is not None:
                # Drum kit: the semitone picks a 5-byte entry (instrument, key, u16 volume, pan) that also sets
                # the channel's volume and pan, as the driver writes them into the channel.
                inst, key, vol_lo, vol_hi, pan = block[drum_map + semitone * 5:drum_map + semitone * 5 + 5]
                events.append([tick, "program", inst & 0x7F])
                set_level("volume", min(127, (vol_lo | vol_hi << 8) >> 7))
                set_level("pan", pan & 0x7F)
                last_note = [tick, "note", min(127, key + 12), last_len - early]
                events.append(last_note)
                held_transpose = transpose
            elif semitone < 12:
                key = max(0, min(127, 12 * (octave + 1) + semitone + transpose))
                # A glide starts the note on the last one's key and bends to its own over the glide time.
                last_note = [tick, "note", last_key if glide and last_key else key, last_len - early]
                events.append(last_note)
                if glide and last_key:  # it takes over the bend fields, so a pending A4 bend is lost
                    bend = (glide, key - last_key) if key != last_key else None
                last_key, held_transpose = key, transpose
            elif semitone == 12 and last_note:
                last_note[3] += cut + last_len - early  # tie: the last segment's early key-off didn't happen
            if semitone <= 12 and last_note and bend:  # a pending bend starts on the next note or tie, then is spent
                events.append([tick, "bend", bend[0], bend[1] + transpose - held_transpose])
                bend, held_transpose = None, transpose
            cut = early
            mode = mode & ~2 | (mode & 1) << 1 if semitone <= 12 else mode & ~2
            advance(last_len)
            pos += 1
            continue
        if b not in ARGS or b == 0xA0:
            return events, None, tick
        args = block[pos + 1:pos + 1 + ARGS[b]]
        nxt = pos + 1 + len(args)
        match b:
            case 0xA1 | 0xF2:  # instrument (F2 also rescales pitch from the old instrument to the new one)
                events.append([tick, "program", args[0] & 0x7F])
            case 0xA2:
                next_len, fixed_len = args[0], 0
            case 0xA4:  # bend by s semitones over n ticks (0 = 256), starting at the next note or tie
                semitones = args[1] - 256 if args[1] & 0x80 else args[1]
                bend = (args[0] or 256, semitones) if semitones else None
            case 0xC0 | 0xC1:  # transpose: set / add signed semitones
                transpose = (0 if b == 0xC0 else transpose) + (args[0] - 256 if args[0] & 0x80 else args[0])
            case 0xD8 | 0xD9:  # detune: set / add
                detune = (0 if b == 0xD8 else detune) + (args[0] - 256 if args[0] & 0x80 else args[0])
                events.append([tick, "detune", detune])
            case 0xB4 | 0xB5 | 0xB6 | 0xB8 | 0xB9 | 0xBA:
                # Vibrato (B4-B6) and tremolo (B8-BA): on with a delay after each note, a rate (0 = 256) and a
                # waveform; depth; off, which keeps the depth for the next on.
                kind = "vibrato" if b < 0xB8 else "tremolo"
                lfo = lfos[kind]
                match b - (0xB4 if kind == "vibrato" else 0xB8):
                    case 0 if effect:
                        lfo[:4] = [0, args[1] or 256, args[2], True]
                        lfo[4] = args[0] or lfo[4]
                    case 0:
                        lfo[:4] = [args[0], args[1] or 256, args[2], True]
                    case 1:
                        lfo[4] = args[0]
                    case 2:
                        lfo[3] = False
                delay, rate, wave, on, depth = lfo
                events.append([tick, kind, delay, rate, wave, depth if on else 0])
            case 0xFD:  # time signature; the driver restarts its beat and bar counting here
                events.append([tick, "meter", args[0], args[1]])
            case 0xAD | 0xAE | 0xAF | 0xB1 | 0xB2:  # the channel's own envelope, until the next program change
                events.append([tick, {0xAD: "ar", 0xAE: "dr", 0xAF: "sl", 0xB1: "sr", 0xB2: "rr"}[b], args[0]])
            case 0xBC | 0xBD | 0xBE:  # pan LFO: on (rate, waveform; no delay) / depth / off
                lfo = lfos["pan_lfo"]
                if b == 0xBC:
                    lfo[:4] = [0, args[0] or 256, args[1], True]
                elif b == 0xBD:
                    lfo[4] = args[0]
                else:
                    lfo[3] = False
                delay, rate, wave, on, depth = lfo
                events.append([tick, "pan_lfo", delay, rate, wave, depth if on else 0])
            case 0xDD | 0xDE:  # slide the vibrato / tremolo depth to d over t ticks (0 = 256)
                events.append([tick, "vibrato_slide" if b == 0xDD else "tremolo_slide", args[0] or 256, args[1]])
            case 0xC6 | 0xC7:  # FM: pitch follows the previous voice's output; C7 also stops a D2/D3 countdown
                fm = b == 0xC6
                fm_due = fm_due if fm else None
                events.append([tick, "fm", fm])
            case 0xD2 | 0xD3:  # FM on now (D2 only), and flipped after n + 1 ticks (0 = 257)
                if b == 0xD2:
                    fm = True
                    events.append([tick, "fm", True])
                fm_due = tick + (args[0] + 1 if args[0] else 0x101)
            case 0xC4 | 0xC5:  # the SPU noise generator instead of the sample
                events.append([tick, "noise", b == 0xC4])
            case 0xAC:  # noise clock 0-63; with either top bit set, add the low 6 bits instead (wrapping)
                events.append([tick, "noise_clock_add" if args[0] & 0xC0 else "noise_clock", args[0] & 0x3F])
            case 0xB0:  # decay rate and sustain level in one (the AE and AF handlers in turn)
                events += [[tick, "dr", args[0]], [tick, "sl", args[1]]]
            case 0xB3:  # the instrument's own envelope back
                events.append([tick, "adsr_reset", True])
            case 0xBB:  # sustain mode (1 linear up, 3 linear down, 5 exponential up, 7 exponential down)
                events.append([tick, "sr_mode", args[0]])
            case 0xD4 | 0xD5:  # pitch from the previous channel's voice, plus this channel's own vibrato
                events.append([tick, "pitch_follow", b == 0xD4])
            case 0xCB:  # all off: noise, FM (and its countdown), reverb, D4/D6, slur and full length
                fm, fm_due, mode = False, None, mode & ~5
                events += [[tick, "noise", False], [tick, "fm", False], [tick, "reverb", False],
                           [tick, "pitch_follow", False]]
            case 0xDB:  # glide off
                glide = 0
            case 0xF4:  # layer: instrument a as usual, instrument b on a second voice playing the same notes
                events.append([tick, "program", args[0] & 0x7F])
                events.append([tick, "layer", [args[0] & 0x7F, args[1] & 0x7F]])
            case 0xF5:
                events.append([tick, "layer", None])
            case 0xF6:  # balance between the main and the layer voice
                events.append([tick, "balance", args[0]])
            case 0xF7:  # slide the balance to n over t ticks (0 = 256)
                events.append([tick, "balance_slide", args[0] or 256, args[1]])
            case 0xF8 | 0xF9:  # overlap on (release rate) / off
                events.append([tick, "overlap", args[0] if b == 0xF8 else None])
            case 0xC2 | 0xC3:  # the channel's reverb send on / off
                events.append([tick, "reverb", b == 0xC2])
            case 0xEA:  # the song's reverb depth (output volume, 0x8000 = 1)
                events.append([tick, "reverb_depth", args[0] | args[1] << 8])
            case 0xCC:  # slur on; the look-ahead turns it off at a CD
                mode = 1
            case 0xDA:  # glide over n ticks (0 = 256) from each note to the next; slur mode, ended the same way
                glide, mode = args[0] or 256, 1
            case 0xD0:  # full-length notes; the look-ahead turns it off at a D1
                mode = 4
            case 0xA3:  # multiplier on the channel volume: voice volume = A8 volume * A3 / 128
                events.append([tick, "expression", args[0]])
            case 0xDC:  # fixed length = last length + signed delta, 0 turns it off
                delta = args[0] - 256 if args[0] & 0x80 else args[0]
                fixed_len = max(1, min(255, last_len + delta)) if delta else 0
            case 0xA5:
                octave = args[0]
            case 0xA6:
                octave = (octave + 1) & 15
            case 0xA7:
                octave = (octave - 1) & 15
            case 0xA8:
                set_level("volume", args[0] & 0x7F)
            case 0xAA:
                set_level("pan", args[0] & 0x7F)
            case 0xA9 | 0xAB:  # slide volume / pan to a target over n ticks (0 = 256)
                kind = "volume" if b == 0xA9 else "pan"
                target = (args[1] - 256 if args[1] & 0x80 else args[1]) if b == 0xA9 else args[1]
                length = args[0] or 256
                slides[kind] = [(target - levels[kind]) / length, length]
            case 0xEC:  # drum-kit mode on, map at a signed offset from after the arguments
                drum_map = nxt + struct.unpack("<h", args)[0]
            case 0xED:
                drum_map = None
            case 0xE8:
                events.append([tick, "tempo", args[0] | args[1] << 8])
            case 0xC8:
                loops = (loops + [[nxt, 0]])[-4:]
            case 0xC9 if loops and args[0]:
                loops[-1][1] += 1
                if loops[-1][1] == args[0]:
                    loops.pop()
                else:
                    nxt = loops[-1][0]
            case 0xCA | 0xC9 if loops:
                loop_tick = first_seen.get(loops[-1][0]) if loop_tick is None else loop_tick
                if until is None:
                    return events, loop_tick, tick
                nxt = loops[-1][0]
            case 0xF0 if loops and loops[-1][1] + 1 == (args[0] or 256):  # on pass n of the repeat, jump
                nxt += struct.unpack("<h", args[1:])[0]
            case 0xEE:
                target = nxt + struct.unpack("<h", args)[0]
                if target <= pos:
                    loop_tick = first_seen.get(target) if loop_tick is None else loop_tick
                    if until is None:
                        return events, loop_tick, tick
                nxt = target
        pos = nxt
    return events, loop_tick, tick


def vlq(n: int) -> bytes:
    out = [n & 0x7F]
    while n > 0x7F:
        n >>= 7
        out.insert(0, n & 0x7F | 0x80)
    return bytes(out)


def midi_track(timed: list[tuple[int, int, bytes]]) -> bytes:
    """(tick, order, message) list -> MTrk chunk. `order` puts note-offs before note-ons on the same tick."""
    data, now = b"", 0
    for tick, _, msg in sorted(timed, key=lambda e: e[:2]):
        data += vlq(tick - now) + msg
        now = tick
    data += b"\x00\xFF\x2F\x00"
    return b"MTrk" + struct.pack(">I", len(data)) + data


def bar_lines(meters: list[tuple[int, int]], end: int) -> list[int]:
    """Ticks where bars start before `end`, from (tick, ticks per bar) time signatures. Bars are 4/4 until the first
    one; each restarts the count where it stands, cutting the bar before it short."""
    bars, length, pos = [], 192, 0
    for tick, new in sorted(m for m in meters if m[0] < end) + [(end, length)]:
        while pos < tick:
            bars.append(pos)
            pos += length
        pos, length = tick, new
    return bars


def play_song(block: bytes, trim_notes: bool = True, starts: list[int] | None = None, effect: bool = False):
    """Play every channel through the song's common loop.

    Returns (events per channel, loop start, loop end, each looping channel's loop length, any channel ran away).
    Channels loop on their own in the driver, and some start their loop later than others (song 7: three channels
    two bars later). The song as a whole repeats from the latest channel loop start, every lcm(loop lengths)
    ticks, so channels that loop earlier are played on until that point and everything is cut there. Once every
    channel loops, any later point repeats too: with time signatures (FD), the loop starts on the next bar line.
    `trim_notes` cuts notes at the loop end; without it they keep their full length, as the driver holds them on,
    including ties that come from the start of the next pass; a note still held after that whole extra pass is
    tied on forever (a drone) and gets length None. `starts` and `effect` play a sound effect's channels instead.
    """
    starts = starts or channel_starts(block)
    first = [play_channel(block, start, effect=effect) for start in starts]
    runaway = any(end >= MAX_TICKS for *_, end in first)
    loops = [(loop, end - loop) for _, loop, end in first if loop is not None]
    if not loops or not all(length for _, length in loops):
        return [events for events, _, _ in first], None, max(end for *_, end in first), [], runaway
    loop_start = max(loop for loop, _ in loops)
    meters = [(e[0], e[2] * e[3]) for events, _, _ in first for e in events if e[1] == "meter"]
    if meters:
        loop_start = next(bar for bar in bar_lines(meters, MAX_TICKS) if bar >= loop_start)
    lengths = [length for _, length in loops]
    period = math.lcm(*lengths)
    # ponytail: a song whose loops don't share a short common period would get a huge file; none on disc does.
    if period > 8 * max(lengths):
        period = max(lengths)
    loop_end = loop_start + period
    channels = []
    # Untrimmed notes need one more pass: a loop that opens with a tie lengthens the note held at its end.
    play_to = loop_end if trim_notes else loop_end + period
    for start, (events, loop, end) in zip(starts, first):
        if loop is not None and end < play_to:
            events, _, _ = play_channel(block, start, until=play_to, effect=effect)
        kept = [e for e in events if e[0] < loop_end]
        for e in kept:
            if e[1] == "note" and trim_notes:
                e[3] = min(e[3], loop_end - e[0])
            elif e[1] == "note" and e[0] + e[3] >= play_to:
                e[3] = None
        channels.append(kept)
    return channels, loop_start, loop_end, lengths, runaway


def song_to_midi(block: bytes) -> tuple[bytes, list[int], bool]:
    """Return (Standard MIDI File bytes, loop length in ticks of each looping channel, any channel ran away)."""
    channels, loop_start, loop_end, loop_lengths, runaway = play_song(block)
    conductor = []
    tracks = []
    for i, events in enumerate(channels):
        ch = i % 16
        timed = []
        for tick, kind, *rest in events:
            if kind == "note":
                key, length = rest
                timed += [(tick, 1, bytes([0x90 | ch, key, 100])), (tick + length, 0, bytes([0x80 | ch, key, 0]))]
            elif kind == "program":
                timed.append((tick, 1, bytes([0xC0 | ch, rest[0]])))
            elif kind in ("volume", "pan", "expression"):
                cc = {"volume": 7, "pan": 10, "expression": 11}[kind]
                timed.append((tick, 1, bytes([0xB0 | ch, cc, max(0, min(127, rest[0]))])))
            elif kind == "tempo" and rest[0]:
                usec = round(TICKS_PER_QUARTER * 65536 * 1_000_000 / (DRIVER_HZ * rest[0]))
                conductor.append((tick, 1, b"\xFF\x51\x03" + usec.to_bytes(3, "big")))
        tracks.append(midi_track(timed))
    # Loop markers (the convention most game-music players understand) where the song's loop starts and ends.
    if loop_start is not None:
        conductor.append((loop_start, 0, b"\xFF\x06\x09loopStart"))
        conductor.append((loop_end, 0, b"\xFF\x06\x07loopEnd"))
    header = b"MThd" + struct.pack(">IHHH", 6, 1, len(tracks) + 1, TICKS_PER_QUARTER)
    return header + midi_track(conductor) + b"".join(tracks), loop_lengths, runaway


def export_midi(source: Path, out: Path):
    out.mkdir(parents=True, exist_ok=True)
    songs, uneven = named_songs(source), []
    for name, origin, block in songs:
        midi, loop_lengths, runaway = song_to_midi(block)
        (out / f"{name}.mid").write_bytes(midi)
        # Looping channels of a song repeat the same length (echo channels may start a little late, and a short
        # pattern like a one-bar drum loop divides it); anything else points at a misread length or loop command.
        if runaway or loop_lengths and any(not n or max(loop_lengths) % n for n in loop_lengths):
            uneven.append(f"{name} ({origin}): loop lengths {sorted(set(loop_lengths))}" + (" + runaway" if runaway else ""))
    print(f"{len(songs)} songs -> {out}; {len(songs) - len(uneven)} with every looping channel the same length")
    for line in uneven:
        print("  " + line)


def main():
    match sys.argv[1:]:
        case ["midi", source, out]:
            export_midi(Path(source), Path(out))
        case ["check", root]:
            check(Path(root))
        case ["dump", path]:
            dump(Path(path), 0)
        case ["dump", path, index]:
            dump(Path(path), int(index))
        case _:
            print(__doc__)


if __name__ == "__main__":
    main()
