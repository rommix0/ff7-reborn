from ff7player import (cell_text, delete_bar, insert_bar, make_unique, note_name, parse_note, prune, set_cell,
                       set_loop, set_meter)


def song():
    return {"channels": 2, "ticks_per_quarter": 48, "timer_hz": 243.857, "loop": {"start": 192, "end": 576},
            "restart": 1, "order": [0, 1, 1],
            "patterns": [{"ticks": 192, "beat": 48, "beats": 4, "cells": [[0, 0, {"note": 60, "len": 48}]]},
                         {"ticks": 192, "beat": 48, "beats": 4, "cells": [[96, 1, {"volume": 80}]]}]}


assert note_name(60) == "C-4" and note_name(61) == "C#4" and parse_note("C-4") == 60 and parse_note("c#4") == 61
assert parse_note("B-0") == 23 and parse_note("72") == 72

# Cells: set replaces the cell on that tick and channel, keeps time order, and no fields removes it.
p = song()["patterns"][0]
set_cell(p, 24, 1, {"inst": 3})
set_cell(p, 0, 0, {"note": 62, "len": 12})
assert p["cells"] == [[0, 0, {"note": 62, "len": 12}], [24, 1, {"inst": 3}]]
set_cell(p, 24, 1, {})
assert p["cells"] == [[0, 0, {"note": 62, "len": 12}]]
assert cell_text({"note": 62, "inst": 3, "volume": 90, "tempo": 0x6000}, 243.857, 48) == "D-4 i3 v90 T114"

# Bars: a new bar goes after the chosen one (empty in its time signature, or a copy). Inserting before the loop
# shifts it, and the loop always runs to the end of the song.
s = song()
insert_bar(s, 0)
assert s["order"] == [0, 2, 1, 1] and s["patterns"][2]["cells"] == [] and s["restart"] == 2
assert s["loop"] == {"start": 384, "end": 768}
insert_bar(s, 3, copy=True)
assert s["order"] == [0, 2, 1, 1, 1] and s["restart"] == 2 and s["loop"]["end"] == 960
delete_bar(s, 1)
assert s["order"] == [0, 1, 1, 1] and s["restart"] == 1 and s["loop"] == {"start": 192, "end": 768}

# Bars that share a pattern change together; make_unique gives one bar its own copy first.
make_unique(s, 2)
assert s["order"] == [0, 1, 3, 1] and s["patterns"][3] == s["patterns"][1] and s["patterns"][3] is not s["patterns"][1]
# A new time signature resizes the bar and drops the cells that no longer fit.
assert set_meter(s, 2, 24, 3) == 1  # 3/8: 72 ticks; the cell at 96 is gone
assert s["patterns"][3] == {"ticks": 72, "beat": 24, "beats": 3, "cells": []} and s["loop"]["end"] == 648
set_loop(s, 3)
assert s["restart"] == 3 and s["loop"] == {"start": 456, "end": 648}

# Saving drops patterns no bar uses and renumbers the rest.
s["order"] = [0, 3]
prune(s)
assert s["order"] == [0, 1] and len(s["patterns"]) == 2 and s["patterns"][1]["ticks"] == 72
print("ok")
