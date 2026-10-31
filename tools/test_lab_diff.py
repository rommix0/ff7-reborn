from lab_diff import diff_runs

base = bytes(16)
other = bytearray(base)
other[1] = other[3] = 1  # two bytes under --gap apart: one run
other[10] = 2
other[14] = 3  # also differs between two runs with the same input: noise
noise = bytearray(base)
noise[14] = 9
assert diff_runs(base, bytes(other), bytes(noise)) == [(1, 3), (10, 1)]
assert diff_runs(base, bytes(other), gap=1) == [(1, 1), (3, 1), (10, 1), (14, 1)]
print("ok")
