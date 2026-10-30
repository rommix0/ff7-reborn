from collections import Counter

from movie import AC_CODES, huffman

# The coefficient codes (with their sign bit), end of block and escape must form a prefix code.
codes = ["10", "000001"] + [entry.split("/")[0] + sign for entry in AC_CODES.split() for sign in "01"]
assert not [(a, b) for a in codes for b in codes if a != b and b.startswith(a)]

# Fibonacci counts make plain Huffman codes far longer than JPEG's 16-bit limit.
fibonacci = [1, 1]
while len(fibonacci) < 30:
    fibonacci.append(fibonacci[-1] + fibonacci[-2])
dht, table = huffman(Counter(dict(enumerate(fibonacci))))
lengths = [len(code) for code in table.values()]
assert len(table) == 30 and max(lengths) <= 16 and list(dht[:16]) == [lengths.count(n) for n in range(1, 17)]
assert all("0" in code for code in table.values())  # JPEG forbids an all-ones code
assert not [(a, b) for a in table.values() for b in table.values() if a != b and b.startswith(a)]
print("ok")
