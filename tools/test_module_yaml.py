from module_yaml import code_bounds

JR_RA, NOP = 0x03E00008, 0
LEAF = [0x3C028010, 0x8C428368, NOP, 0x14400003, NOP, JR_RA, NOP]  # lui, lw, load delay, bnez, delay slot, return
FRAME = [0x27BDFFE8, 0xAFBF0010, 0x8FBF0010, 0x27BD0018, JR_RA, NOP]


def image(*words):
    return b"".join(w.to_bytes(4, "little") for w in words)


# A leaf with no stack frame opens the module (AQUA.BIN's entry), and the code ends on the last return's delay slot.
assert code_bounds(image(*LEAF, *FRAME)) == (0, 52)
# Header words before the code: a pointer, jr $zero, a zero, and an add into $zero (Q_SHU.BIN).
assert code_bounds(image(0x801BD644, 0x8, NOP, 0x20, *LEAF, *FRAME)) == (16, 68)
# Shifts of $zero, and a zero that isn't in a delay slot (GAS.BIN).
assert code_bounds(image(0x800, 0x1200, 0x800, NOP, *FRAME)) == (16, 40)
# An R4000 opcode rabbitizer still decodes (BATINI.X's daddi), sltiu into $zero (RESIST.BIN), and a lead-in lui.
assert code_bounds(image(0x6000000C, 0x2E808080, 0x3C028010, *FRAME)) == (8, 36)
# A string's tail that reads as a jump out of RAM (FORMMENU.MNU's "m\0!\x08").
assert code_bounds(image(0x0821006D, *FRAME)) == (4, 28)
print("ok")
