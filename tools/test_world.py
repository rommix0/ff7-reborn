from field import lzs_pack, repack
from unpack import lzs
from world import CODE, SIZE, assemble, disassemble, scripts_json, scripts_raw


def words(*ws: int) -> bytes:
    return b"".join(w.to_bytes(2, "little") for w in ws)


# The code is u16 words: operators (00-FF) and commands (300-) take nothing inline, a push (101-1FF) its value,
# goto/ifnot (200/201) a word number from the code's start. A push's type says what the value is: 110 itself, 114
# a bit, 118 a byte, 11C a u16 at that address; the low two bits pick the save data (0), a scratch area (1, 2) or
# a special value (3). Saved bytes and u16s take the field's names.
code = words(0x203, 0x100, 0x110, 300, 0x11C, 0, 0x70, 0x201, 11, 0x119, 0x0A, 0x317, 0x200, 1, 0x203, 0x20D, 0x331)
lines = ["L0000:", "ret", "L0001:", "reset", "push #300", "push story", "eq", "ifnot L000B", "push byte.1[A]",
         "L000B:", "battle", "goto L0001", "ret", "run 9", "op_331"]
assert disassemble(code, {0}) == lines and assemble(lines) == code
assert disassemble(words(0x11B, 0x9, 0x118, 0x10, 0x112, 7, 0x16)) == ["push byte.3[9]", "push play_hours",
                                                                         "push_112 0007", "op_016"]
assert assemble(["push word[10]", "push bit[523]", "push_112 7"]) == words(0x11C, 0x10, 0x114, 0x523, 0x112, 7)
# Commands, the measuring operators and the special values the code shows the meaning of go by name.
named = words(0x11B, 8, 0x330, 0x117, 0x15, 0x110, 50, 0x110, 0, 0x318, 0x19)
assert disassemble(named) == ["push byte.player_id", "select_entity", "push bit.3[15]", "push #50", "push #0",
                              "enter_field", "entity_distance"]
assert assemble(disassemble(named)) == named
for wrong in (["goto L0005"], ["push"], ["push word.4[1]"], ["fly"]):
    try:
        assemble(wrong)
        raise AssertionError(wrong)
    except ValueError:
        pass

# A section: 256 table entries of a u16 key and a word number (entry 0 and the unused FFFF 0000 read nothing), the
# code from 0x400, zeros to 0x7000. Keys: below 4000 a system script, 4000 an entity's function, 8000 a map block's.
section = bytearray(SIZE)
section[4:16] = words(0x0001, 1, 0x4E03, 1, 0x8000 | (5 + 36 * 2) << 4 | 1, 0)
section[16:0x400] = words(0xFFFF, 0) * 252
section[CODE:CODE + len(code)] = code
table = scripts_json(bytes(section))
assert table["scripts"] == [{"key": "0001", "_what": "system 1", "at": "L0001"},
                            {"key": "4E03", "_what": "entity 0E function 3", "at": "L0001"},
                            {"key": "84D1", "_what": "block 5,2 type 4", "at": "L0000"}]
assert table["code"] == lines and scripts_raw(table) == section
table["code"][4] = "push #1"
assert scripts_raw(table)[CODE + 6:CODE + 8] == words(1)

# A container keeps its packed bytes up to the flag byte before what changed; the rest is packed again.
data = bytes(range(256)) * 40
new = data[:6000] + b"changed" + data[6007:]
packed = repack(lzs_pack(data), new, 6000)
assert lzs(packed) == new and packed[4:900] == lzs_pack(data)[4:900]
print("ok")
