"""Play and edit .ff7song files (docs/ff7song_format.md) with the game's own instruments.

usage: python tools/ff7player.py [folder]    # holds .ff7song files; default assets/ff7song
       python tools/ff7player.py assets/ff7song/effects    # the sound effects

instruments.ff7bank is found in the folder or the one above it.

Needs numpy and sounddevice (pip install -r tools/requirements-player.txt).
Space plays/pauses, Ctrl+S saves. Tempo % and Transpose change playback only, not the song. Click a cell to select it, double-click to edit, Delete clears it. Double-click a
bar in the order list to jump there; click a channel heading to mute it. Bars that share a pattern change together
until "Own copy" gives one bar its own.
"""
import bisect
import copy
import json
import re
import sys
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, simpledialog, ttk

NOTE_NAMES = ["C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"]
LIST_FIELDS = {"bend": 2, "vibrato": 4, "tremolo": 4, "pan_lfo": 4, "vibrato_slide": 2, "tremolo_slide": 2,
               "layer": 2, "balance_slide": 2}  # fields that hold several numbers
FIELDS = ["note", "len", "slur", "inst", "volume", "expression", "pan", "detune", "tempo", "bend", "vibrato", "tremolo",
          "ar", "dr", "sl", "sr", "rr", "overlap", "pan_lfo", "vibrato_slide", "tremolo_slide", "fm", "layer",
          "balance", "balance_slide", "reverb", "reverb_depth", "noise", "noise_clock", "noise_clock_add", "adsr_reset",
          "sr_mode", "pitch_follow", "volume_follow", "vibrato_on", "tremolo_on", "pan_lfo_on"]
SWITCHES = ("fm", "reverb", "noise", "adsr_reset", "pitch_follow", "volume_follow", "vibrato_on", "tremolo_on",
            "pan_lfo_on")  # fields that are true or false, shown as 1/0


def note_name(key: int) -> str:
    return f"{NOTE_NAMES[key % 12]}{key // 12 - 1}"


def parse_note(text: str) -> int:
    """Key from "C-4", "C#4" or a plain key number."""
    text = text.strip().upper()
    if text.isdigit():
        return int(text)
    match = re.fullmatch(r"([A-G])([-#])(\d)", text)
    if not match:
        raise ValueError(f"note {text!r}: write it like C-4 or C#4")
    return NOTE_NAMES.index(match[1] + match[2]) + 12 * (int(match[3]) + 1)


def bpm(tempo: int, timer_hz: float, ticks_per_quarter: int) -> int:
    return round(timer_hz * tempo / 65536 * 60 / ticks_per_quarter)


def cell_text(f: dict, timer_hz: float, ticks_per_quarter: int) -> str:
    parts = [("~" if f.get("slur") else "") + note_name(f["note"])] if "note" in f else []
    for name, letter in (("inst", "i"), ("volume", "v"), ("expression", "x"), ("pan", "p"), ("detune", "d"),
                         ("ar", "ar"), ("dr", "dr"), ("sl", "sl"), ("sr", "sr"), ("rr", "rr")):
        if name in f:
            parts.append(f"{letter}{f[name]}")
    if "bend" in f:
        parts.append(f"b{f['bend'][1]:+d}/{f['bend'][0]}")
    for name, letter in (("vibrato", "V"), ("tremolo", "R"), ("pan_lfo", "P")):
        if name in f:  # "!": switched on here, restarting on the sounding note
            parts.append(f"{letter}{'!' if f.get(name + '_on') else ''}{f[name][3] or '-'}")
    for name, letter in (("vibrato_slide", "V"), ("tremolo_slide", "R"), ("balance_slide", "bal")):
        if name in f:
            parts.append(f"{letter}>{f[name][1]}/{f[name][0]}")
    if "fm" in f:
        parts.append("FM" if f["fm"] else "FM-")
    if "reverb" in f:
        parts.append("Rv" if f["reverb"] else "Rv-")
    if "noise" in f:
        parts.append("N" if f["noise"] else "N-")
    for name, letter in (("noise_clock", "nc"), ("noise_clock_add", "nc+"), ("sr_mode", "srm")):
        if name in f:
            parts.append(f"{letter}{f[name]}")
    if f.get("adsr_reset"):
        parts.append("env")
    if "pitch_follow" in f:
        parts.append("PF" if f["pitch_follow"] else "PF-")
    if "volume_follow" in f:
        parts.append("VF" if f["volume_follow"] else "VF-")
    if "reverb_depth" in f:
        parts.append(f"Rd{f['reverb_depth']:X}")
    if "layer" in f:
        parts.append(f"L{f['layer'][1]}" if f["layer"] else "L-")
    if "balance" in f:
        parts.append(f"bal{f['balance']}")
    if "overlap" in f:
        parts.append(f"o{'-' if f['overlap'] is None else f['overlap']}")
    if "tempo" in f:
        parts.append(f"T{bpm(f['tempo'], timer_hz, ticks_per_quarter)}")
    return " ".join(parts)


def bar_starts(song: dict) -> list[int]:
    starts, t = [], 0
    for p in song["order"]:
        starts.append(t)
        t += song["patterns"][p]["ticks"]
    return starts + [t]


def fix_loop(song: dict):
    """After bar edits: the loop starts at the restart bar and runs to the end of the song."""
    if song["loop"] is None:
        return
    song["restart"] = min(song["restart"], len(song["order"]) - 1)
    starts = bar_starts(song)
    song["loop"] = {"start": starts[song["restart"]], "end": starts[-1]}


def set_cell(pattern: dict, tick: int, ch: int, fields: dict):
    """Replace the cell on (tick, channel); no fields removes it."""
    cells = [c for c in pattern["cells"] if c[:2] != [tick, ch]]
    if fields:
        cells.append([tick, ch, fields])
    pattern["cells"] = sorted(cells, key=lambda c: c[:2])


def insert_bar(song: dict, at: int, copy: bool = False):
    """A new bar after bar `at`: its copy (same pattern), or empty in its time signature."""
    if copy:
        pattern = song["order"][at]
    else:
        source = song["patterns"][song["order"][at]]
        song["patterns"].append({"ticks": source["beat"] * source["beats"], "beat": source["beat"],
                                 "beats": source["beats"], "cells": []})
        pattern = len(song["patterns"]) - 1
    song["order"].insert(at + 1, pattern)
    if at + 1 <= song["restart"]:
        song["restart"] += 1
    fix_loop(song)


def delete_bar(song: dict, at: int):
    if len(song["order"]) == 1:
        raise ValueError("a song needs at least one bar")
    del song["order"][at]
    if at < song["restart"]:
        song["restart"] -= 1
    fix_loop(song)


def make_unique(song: dict, at: int):
    song["patterns"].append(copy.deepcopy(song["patterns"][song["order"][at]]))
    song["order"][at] = len(song["patterns"]) - 1


def set_meter(song: dict, at: int, beat: int, beats: int) -> int:
    """Give bar `at`'s pattern a new time signature; returns how many cells past its new end were dropped."""
    pattern = song["patterns"][song["order"][at]]
    ticks = beat * beats
    kept = [c for c in pattern["cells"] if c[0] < ticks]
    dropped = len(pattern["cells"]) - len(kept)
    pattern.update(ticks=ticks, beat=beat, beats=beats, cells=kept)
    fix_loop(song)
    return dropped


def set_loop(song: dict, at: int):
    song["loop"], song["restart"] = song["loop"] or {}, at
    fix_loop(song)


def prune(song: dict):
    """Drop patterns no bar uses and renumber the rest."""
    used = sorted(set(song["order"]))
    song["patterns"] = [song["patterns"][p] for p in used]
    song["order"] = [used.index(p) for p in song["order"]]


class PlayerWindow:
    def __init__(self, root: tk.Tk, folder: Path):
        import sounddevice
        from akao_synth import SPU_HZ, Synth, read_bank

        self.root, self.folder = root, folder
        self.synth = Synth(*read_bank(find_bank(folder)))
        self.paths = sorted(folder.glob("*.ff7song"))
        self.song, self.path, self.dirty = None, None, False
        self.shown_bar, self.row_ticks, self.selected = -1, [], None
        self.follow = tk.BooleanVar(value=True)
        self.reverb = tk.BooleanVar(value=True)
        self.row_step = tk.StringVar(value="auto")
        self.tempo_percent = tk.IntVar(value=100)
        self.transpose = tk.IntVar(value=0)
        self.build()
        # ponytail: numpy allocates in the audio callback; a high-latency stream absorbs it. A C mixer if it glitches.
        self.stream = sounddevice.OutputStream(samplerate=SPU_HZ, channels=2, dtype="float32", blocksize=1024,
                                               latency="high", callback=self.audio)
        self.stream.start()
        self.poll()

    def audio(self, outdata, frames, time, status):
        outdata[:] = self.synth.render(frames)

    def build(self):
        root = self.root
        root.title("FF7 Music Player")
        root.geometry(f"{min(1200, root.winfo_screenwidth() - 80)}x{min(760, root.winfo_screenheight() - 120)}")
        bar = ttk.Frame(root, padding=6)
        bar.pack(fill="x")
        self.play_button = ttk.Button(bar, text="Play", command=self.toggle_play)
        self.play_button.pack(side="left")
        ttk.Button(bar, text="Save", command=self.save).pack(side="left", padx=4)
        ttk.Checkbutton(bar, text="Follow playback", variable=self.follow).pack(side="left", padx=8)
        ttk.Checkbutton(bar, text="Reverb", variable=self.reverb,
                        command=lambda: setattr(self.synth, "reverb_on", self.reverb.get())).pack(side="left")
        # Master tempo and transpose: playback only, the song file doesn't change.
        for label, var, low, high, step, unit in (("Tempo", self.tempo_percent, 25, 400, 5, "%"),
                                                 ("Transpose", self.transpose, -24, 24, 1, "st")):
            ttk.Label(bar, text=label).pack(side="left", padx=(8, 2))
            box = ttk.Spinbox(bar, textvariable=var, from_=low, to=high, increment=step, width=4,
                              command=self.apply_master)
            box.pack(side="left")
            box.bind("<Return>", lambda e: self.apply_master())
            box.bind("<FocusOut>", lambda e: self.apply_master())
            ttk.Label(bar, text=unit).pack(side="left")
        ttk.Button(bar, text="Reset", width=6, command=self.reset_master).pack(side="left", padx=4)
        ttk.Label(bar, text="Rows every").pack(side="left")
        step = ttk.Combobox(bar, textvariable=self.row_step, width=5, state="readonly",
                            values=["auto", "1", "2", "3", "4", "6", "8", "12", "24", "48"])
        step.pack(side="left", padx=4)
        step.bind("<<ComboboxSelected>>", lambda e: self.show_bar(self.shown_bar, force=True))
        ttk.Label(bar, text="ticks").pack(side="left")
        self.info = ttk.Label(root, text="Pick a song.", padding=(10, 0, 10, 4))  # its own row: the toolbar is full
        self.info.pack(fill="x")

        panes = ttk.PanedWindow(root, orient="horizontal")
        panes.pack(fill="both", expand=True)
        left = ttk.Frame(panes, padding=(6, 0))
        panes.add(left, weight=0)
        self.songs = tk.Listbox(left, height=10, exportselection=False, font="TkFixedFont")
        for path in self.paths:
            self.songs.insert("end", path.stem)
        self.songs.pack(fill="x")
        self.songs.bind("<<ListboxSelect>>", lambda e: self.open_song(self.songs.curselection()[0]))
        buttons = ttk.Frame(left)
        buttons.pack(side="bottom", fill="x", pady=(0, 6))  # packed before the list so it never gets squeezed out
        for i, (text, command) in enumerate((("Insert bar", self.insert_bar), ("Duplicate", self.duplicate_bar),
                                             ("Delete", self.delete_bar), ("Time sig…", self.change_meter),
                                             ("Own copy", self.own_copy), ("Loop here", self.loop_here))):
            ttk.Button(buttons, text=text, command=command).grid(row=i // 2, column=i % 2, sticky="ew")
        self.order = ttk.Treeview(left, columns=("bar", "pattern", "meter", "ticks"), show="headings", height=8,
                                  selectmode="browse")
        for col, width in (("bar", 50), ("pattern", 70), ("meter", 60), ("ticks", 50)):
            self.order.heading(col, text=col)
            self.order.column(col, width=width, anchor="e")
        self.order.pack(fill="both", expand=True, pady=6)
        self.order.bind("<<TreeviewSelect>>", self.order_selected)
        self.order.bind("<Double-1>", lambda e: self.seek_bar(self.current_order()))

        right = ttk.Frame(panes)
        panes.add(right, weight=1)
        self.grid = ttk.Treeview(right, show="headings", selectmode="none")
        yscroll = ttk.Scrollbar(right, orient="vertical", command=self.grid.yview)
        xscroll = ttk.Scrollbar(right, orient="horizontal", command=self.grid.xview)
        self.grid.configure(yscrollcommand=yscroll.set, xscrollcommand=xscroll.set)
        self.grid.grid(row=0, column=0, sticky="nsew")
        yscroll.grid(row=0, column=1, sticky="ns")
        xscroll.grid(row=1, column=0, sticky="ew")
        right.rowconfigure(0, weight=1)
        right.columnconfigure(0, weight=1)
        self.grid.tag_configure("beat", background="#eeeeea")
        self.grid.tag_configure("now", background="#fff0b8")
        self.grid.bind("<Button-1>", self.grid_click)
        self.grid.bind("<Double-1>", self.grid_edit)
        self.status = ttk.Label(right, text="", padding=4, font="TkFixedFont")
        self.status.grid(row=2, column=0, sticky="ew")

        root.bind("<space>", lambda e: self.toggle_play() if e.widget.winfo_class() != "TEntry" else None)
        root.bind("<Control-s>", lambda e: self.save())
        root.bind("<Delete>", lambda e: self.clear_cell())
        root.protocol("WM_DELETE_WINDOW", self.close)

    # Songs and playback

    def keep_changes(self) -> bool:
        """False if the user cancels; saves first if they want to."""
        if not self.dirty:
            return True
        answer = messagebox.askyesnocancel("Unsaved changes", f"Save changes to {self.path.name}?")
        if answer:
            self.save()
        return answer is not None

    def open_song(self, index: int):
        if self.paths[index] == self.path or not self.keep_changes():
            return
        self.path = self.paths[index]
        self.song = json.loads(self.path.read_text(encoding="utf-8"))
        self.dirty, self.shown_bar, self.selected = False, -1, None
        self.synth.muted = 0
        song = copy.deepcopy(self.song)
        self.synth.post(lambda s: (s.load(song), setattr(s, "playing", True)))
        self.refresh_order()
        self.build_columns()
        self.show_bar(0)

    def build_columns(self):
        channels = [f"c{c}" for c in range(self.song["channels"])]
        self.grid.configure(columns=["beat"] + channels)
        self.grid.heading("beat", text="beat")
        self.grid.column("beat", width=60, anchor="e", stretch=False)
        for c, col in enumerate(channels):
            self.grid.heading(col, text=str(c + 1), command=lambda c=c: self.toggle_mute(c))
            self.grid.column(col, width=130, anchor="w", stretch=False)

    def toggle_mute(self, c: int):
        self.synth.muted ^= 1 << c
        self.grid.heading(f"c{c}", text=f"{c + 1} (muted)" if self.synth.muted >> c & 1 else str(c + 1))

    def toggle_play(self):
        if not self.song:
            return
        if self.synth.playing:
            self.synth.post(lambda s: setattr(s, "playing", False))
        else:
            self.synth.post(lambda s: (s.ended and s.seek(0), setattr(s, "playing", True)))

    def seek_bar(self, i: int):
        if self.song is not None and i is not None:
            tick = bar_starts(self.song)[i]
            self.synth.post(lambda s: s.seek(tick))
            self.show_bar(i)

    def changed(self):
        """After an edit: mark unsaved, hand the synth a copy, redraw."""
        self.dirty = True
        self.root.title(f"FF7 Music Player — {self.path.name} *")
        song = copy.deepcopy(self.song)
        self.synth.post(lambda s: s.update(song))
        self.refresh_order()
        self.show_bar(min(self.shown_bar, len(self.song["order"]) - 1), force=True)

    def save(self):
        if not self.song:
            return
        prune(self.song)
        self.path.write_text(json.dumps(self.song, separators=(",", ":")), encoding="utf-8")
        self.dirty = False
        self.root.title(f"FF7 Music Player — {self.path.name}")
        self.refresh_order()

    def apply_master(self):
        """Hand the tempo and transpose boxes to the synth, clamped to their ranges; bad input goes back to the last
        good value."""
        try:
            percent = min(400, max(25, self.tempo_percent.get()))
            semitones = min(24, max(-24, self.transpose.get()))
        except tk.TclError:
            percent, semitones = round(self.synth.tempo_scale * 100), self.synth.transpose
        self.tempo_percent.set(percent)
        self.transpose.set(semitones)
        self.synth.tempo_scale, self.synth.transpose = percent / 100, semitones

    def reset_master(self):
        self.tempo_percent.set(100)
        self.transpose.set(0)
        self.apply_master()

    def close(self):
        if self.keep_changes():
            self.stream.stop()
            self.root.destroy()

    def poll(self):
        self.root.after(50, self.poll)
        self.play_button.configure(text="Pause" if self.synth.playing else "Play")
        if self.song and self.synth.tick >= 0:
            starts = bar_starts(self.song)
            tick = self.synth.tick
            i = min(bisect.bisect_right(starts, tick) - 1, len(self.song["order"]) - 1)
            p = self.song["patterns"][self.song["order"][i]]
            tempo = self.synth.tempo * self.synth.tempo_scale
            self.info.configure(text=f"{self.path.stem} · bar {i + 1} of {len(self.song['order'])} · "
                                     f"{p['beats']}/{192 // p['beat']} · "
                                     f"{bpm(tempo, self.song['timer_hz'], self.song['ticks_per_quarter']) if tempo else '–'}"
                                     f" BPM · " + (f"loops to bar {self.song['restart'] + 1}" if self.song["loop"]
                                                   else "no loop"))
            if self.follow.get() and self.synth.playing:
                if i != self.shown_bar:
                    self.show_bar(i)
                self.mark_row(tick - starts[i])

    # Order list and bar edits

    def refresh_order(self):
        self.order.delete(*self.order.get_children())
        uses = {p: self.song["order"].count(p) for p in self.song["order"]}
        for i, p in enumerate(self.song["order"]):
            pat = self.song["patterns"][p]
            loop = "↻ " if self.song["loop"] and i == self.song["restart"] else ""
            shared = f" ×{uses[p]}" if uses[p] > 1 else ""
            self.order.insert("", "end", iid=str(i), values=(f"{loop}{i + 1}", f"{p}{shared}",
                                                             f"{pat['beats']}/{192 // pat['beat']}", pat["ticks"]))

    def current_order(self) -> int | None:
        selection = self.order.selection()
        return int(selection[0]) if selection else (self.shown_bar if self.shown_bar >= 0 else None)

    def order_selected(self, event):
        i = self.current_order()
        if i is not None and i != self.shown_bar:
            self.follow.set(False)
            self.show_bar(i)

    def bar_edit(self, edit):
        i = self.current_order()
        if self.song is None or i is None:
            return
        try:
            edit(i)
        except ValueError as error:
            messagebox.showerror("Can't do that", str(error))
            return
        self.changed()

    def insert_bar(self):
        self.bar_edit(lambda i: insert_bar(self.song, i))

    def duplicate_bar(self):
        self.bar_edit(lambda i: insert_bar(self.song, i, copy=True))

    def delete_bar(self):
        self.bar_edit(lambda i: delete_bar(self.song, i))

    def own_copy(self):
        self.bar_edit(lambda i: make_unique(self.song, i))

    def loop_here(self):
        self.bar_edit(lambda i: set_loop(self.song, i))

    def change_meter(self):
        i = self.current_order()
        if self.song is None or i is None:
            return
        p = self.song["patterns"][self.song["order"][i]]
        text = simpledialog.askstring("Time signature", "Beats / note value, like 4/4 or 6/8.\n"
                                      "Every bar using this pattern changes.",
                                      initialvalue=f"{p['beats']}/{192 // p['beat']}", parent=self.root)
        match = re.fullmatch(r"\s*(\d+)\s*/\s*(1|2|4|8|16)\s*", text or "")
        if not match:
            if text:
                messagebox.showerror("Time signature", f"{text!r} isn't like 4/4 or 6/8.")
            return
        beats, beat = int(match[1]), 192 // int(match[2])
        if any(c[0] >= beat * beats for c in p["cells"]) and not messagebox.askokcancel(
                "Time signature", "The bar gets shorter; cells past its new end will be deleted."):
            return
        self.bar_edit(lambda i: set_meter(self.song, i, beat, beats))

    # The pattern grid

    def show_bar(self, i: int, force: bool = False):
        if self.song is None or i < 0 or (i == self.shown_bar and not force):
            return
        self.shown_bar = i
        p = self.song["patterns"][self.song["order"][i]]
        step = p["beat"] // 4 if self.row_step.get() == "auto" else int(self.row_step.get())
        by_tick = {t: {} for t in range(0, p["ticks"], max(1, step))}
        for tick, ch, fields in p["cells"]:
            by_tick.setdefault(tick, {})[ch] = fields
        self.row_ticks = sorted(by_tick)
        self.grid.delete(*self.grid.get_children())
        hz, tpq = self.song["timer_hz"], self.song["ticks_per_quarter"]
        for t in self.row_ticks:
            cells = by_tick[t]
            values = [f"{t // p['beat'] + 1}.{t % p['beat']:02d}"]
            values += [cell_text(cells[c], hz, tpq) if c in cells else "" for c in range(self.song["channels"])]
            self.grid.insert("", "end", iid=str(t), values=values, tags=("beat",) if t % p["beat"] == 0 else ())
        self.order.selection_set(str(i))
        self.order.see(str(i))
        self.show_selected()

    def mark_row(self, tick_in_bar: int):
        now = self.row_ticks[max(0, bisect.bisect_right(self.row_ticks, tick_in_bar) - 1)] if self.row_ticks else None
        beat = self.song["patterns"][self.song["order"][self.shown_bar]]["beat"]
        for t in self.grid.tag_has("now"):
            self.grid.item(t, tags=("beat",) if int(t) % beat == 0 else ())
        if now is not None:
            self.grid.item(str(now), tags=("now",))  # alone: a beat row's shading would hide it
            self.grid.see(str(now))

    def cell_at(self, event) -> tuple[int, int] | None:
        row, col = self.grid.identify_row(event.y), self.grid.identify_column(event.x)
        if not row or not col or col == "#1":
            return None
        return int(row), int(col[1:]) - 2

    def grid_click(self, event):
        if self.grid.identify_region(event.x, event.y) == "cell":
            self.selected = self.cell_at(event)
            self.show_selected()

    def show_selected(self):
        if not self.selected or not self.song:
            self.status.configure(text="")
            return
        tick, ch = self.selected
        p = self.song["patterns"][self.song["order"][self.shown_bar]]
        fields = next((c[2] for c in p["cells"] if c[:2] == [tick, ch]), {})
        self.status.configure(text=f"tick {tick}, channel {ch + 1}: {json.dumps(fields) if fields else '(empty)'}")

    def clear_cell(self):
        if self.selected and self.song:
            set_cell(self.song["patterns"][self.song["order"][self.shown_bar]], *self.selected, {})
            self.changed()

    def grid_edit(self, event):
        cell = self.cell_at(event)
        if cell and self.song:
            self.selected = cell
            CellDialog(self, *cell)


class CellDialog:
    """Edit one cell's fields; blank fields are left out."""

    def __init__(self, window: PlayerWindow, tick: int, ch: int):
        self.window, self.ch = window, ch
        self.pattern = window.song["patterns"][window.song["order"][window.shown_bar]]
        fields = next((c[2] for c in self.pattern["cells"] if c[:2] == [tick, ch]), {})
        self.old_tick = tick
        self.top = top = tk.Toplevel(window.root)
        top.title(f"Bar {window.shown_bar + 1}, channel {ch + 1}")
        top.transient(window.root)
        self.entries = {}
        rows = [("tick", tick)] + [(name, fields.get(name)) for name in FIELDS]
        if "note" in fields and fields.get("len") is None:
            rows[FIELDS.index("len") + 1] = ("len", "hold")  # held until the channel's next note
        for name in ("overlap", "layer"):
            if name in fields and fields[name] is None:
                rows[FIELDS.index(name) + 1] = (name, "off")
        for name in SWITCHES:
            if name in fields:
                rows[FIELDS.index(name) + 1] = (name, int(fields[name]))
        for r, (name, value) in enumerate(rows):
            ttk.Label(top, text=name).grid(row=r, column=0, sticky="e", padx=6, pady=2)
            entry = ttk.Entry(top, width=24)
            if value is not None:
                text = "1" if name == "slur" else note_name(value) if name == "note" else \
                    ", ".join(map(str, value)) if isinstance(value, list) else str(value)
                entry.insert(0, text)
            entry.grid(row=r, column=1, padx=6, pady=2)
            self.entries[name] = entry
        hint = ("note: C-4 / C#4 · len: ticks, or hold · slur: 1 = no restart · ar…rr: envelope\n"
                "overlap: release rate, or off · bend: ticks, semitones · fm: 1/0\n"
                "vibrato/tremolo/pan_lfo: delay, rate, waveform, depth · *_slide: ticks, target\n"
                "layer: instrument, layer instrument, or off · balance: 0–127 (layer share)\n"
                "reverb: 1/0 (the channel's send) · reverb_depth: song-wide, 32768 = full (the songs use 10240)\n"
                "noise: 1/0 · noise_clock: 0–63 (+_add wraps) · adsr_reset: 1 · sr_mode: 1/3/5/7 · pitch/volume_follow: 1/0")
        ttk.Label(top, text=hint, foreground="#666").grid(row=len(rows), column=0, columnspan=2, padx=6)
        buttons = ttk.Frame(top)
        buttons.grid(row=len(rows) + 1, column=0, columnspan=2, pady=6)
        ttk.Button(buttons, text="OK", command=self.ok).pack(side="left", padx=4)
        ttk.Button(buttons, text="Clear cell", command=self.clear).pack(side="left", padx=4)
        ttk.Button(buttons, text="Cancel", command=top.destroy).pack(side="left", padx=4)
        top.bind("<Return>", lambda e: self.ok())
        top.bind("<Escape>", lambda e: top.destroy())
        self.entries["note"].focus_set()

    def ok(self):
        try:
            tick = int(self.entries["tick"].get())
            if not 0 <= tick < self.pattern["ticks"]:
                raise ValueError(f"tick must be 0–{self.pattern['ticks'] - 1} in this bar")
            fields = {}
            for name in FIELDS:
                text = self.entries[name].get().strip()
                if not text:
                    continue
                if name == "note":
                    fields[name] = parse_note(text)
                elif name == "len" and text.lower() == "hold":
                    fields[name] = None
                elif name in ("overlap", "layer") and text.lower() == "off":
                    fields[name] = None
                elif name in SWITCHES:
                    fields[name] = bool(int(text))
                elif name == "slur":
                    if int(text):
                        fields[name] = True
                elif name in LIST_FIELDS:
                    values = [int(v) for v in text.replace(",", " ").split()]
                    if len(values) != LIST_FIELDS[name]:
                        raise ValueError(f"{name} takes {LIST_FIELDS[name]} numbers")
                    fields[name] = values
                else:
                    fields[name] = int(text)
            if ("note" in fields) != ("len" in fields) or "slur" in fields and "note" not in fields:
                raise ValueError("a note needs a len (ticks), and len and slur only go with a note")
        except ValueError as error:
            messagebox.showerror("Cell", str(error), parent=self.top)
            return
        set_cell(self.pattern, self.old_tick, self.ch, {})
        set_cell(self.pattern, tick, self.ch, fields)
        self.finish(tick)

    def clear(self):
        set_cell(self.pattern, self.old_tick, self.ch, {})
        self.finish(self.old_tick)

    def finish(self, tick: int):
        self.top.destroy()
        self.window.selected = (tick, self.ch)
        self.window.changed()


def find_bank(folder: Path) -> Path | None:
    """The instrument bank in `folder`, or in its parent (effects sit in a subfolder of the songs)."""
    return next((p for p in (folder / "instruments.ff7bank", folder.parent / "instruments.ff7bank") if p.exists()), None)


def main():
    folder = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("assets/ff7song")
    if not find_bank(folder):
        print(__doc__)
        print(f"No instruments.ff7bank in {folder} or above it; export with tools/ff7song.py first.")
        return
    root = tk.Tk()
    PlayerWindow(root, folder)
    root.mainloop()


if __name__ == "__main__":
    main()
