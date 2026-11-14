# PC buttons: mapping and glyphs (draft)

## Intent

The PC port should feel like a PC game: a keyboard or an Xbox-style pad plays it without thinking about a PS1 pad,
and when the game says "press ○" it shows the button the player actually presses. The game's own Config screen keeps
working as it is.

## What the game does (found in the code)

- **The pad word.** The exe reads one 16-bit word per pad, Psy-Q's order (knowledge base, "Pad input"). The port
  fills it in `pc/sdl.c` from the keyboard and the first controller, by position: Xbox A is ✕, B is ○, X is □, Y is
  △; keys Z ✕, X ○, A □, S △, Q/W L1/R1, 1/2 L2/R2, Enter Start, Backspace Select. Escape quits.
- **The game's own remap.** Config's Customize writes a table (`D_8009D7C0`) that the pad reader applies to port 1
  (bit n becomes bit `table[n]`). It's checked in the port: swapping ○ and ✕ there works.
- **Button glyphs in text.** Dialogue bytes `F6`-`F9` are drawn by `func_8001D6A8` (exe, still assembly) as 16×16
  sprites from `WINDOW.BIN`'s first texture, resident at VRAM (960, 256): `F6` ○ (u `0x60`), `F7` △ (`0x90`), `F8` □
  (`0x80`), `F9` ✕ (`0x70`), all at v `0x40`, palette row (256, 490). The field and the world map both reach it
  through `func_8001F1BC`. The strip below (v `0x50`) holds R1, R2, L1, L2.
- **The glyph doesn't follow the remap.** `func_8001D6A8` never reads the Config table: after swapping ○ and ✕ in
  Config, the tutorials still show the original buttons. That's the original game's behaviour.
- **Much of the text names buttons in words:** "[SELECT] button", "Start button", "L1 and R1", "directional button".
  212 of disc 1's 703 maps mention a button one way or the other.

## The proposal

1. **Bindings by function, in a file.** `controls.txt` in the working directory (next to `memcard1.mcd`): each line a
   PS1 button, `=`, then the keys (SDL's names) and pad buttons (`pad` and SDL's name, `pad lefttrigger`) that press
   it. That button loses its defaults, a control named there leaves the button it had, the rest keep theirs. No file:
   the defaults (Decided, below). The game's Config remap still works on top: it remaps functions, this file devices.
2. **Glyphs for the device in use.** The port draws its glyphs over the game's in the window texture for the device the
   player last touched: a PlayStation pad (SDL reports its type) keeps the original; any other pad gets A/B/X/Y,
   LB/RB/LT/RT, View and Menu; the keyboard gets key caps with the bound keys. The glyph for a PS1 button is the
   control bound to it, so Config's Customize screen and the tutorials agree with what the player holds.
   - Drawn by code in the port, in the texture's own 16 colours: no image files, nothing on the disc changed.
   - In the game's own style, as its ○ is drawn: a dark disc (colour 15) with its three-texel glint at the top left
     and two-texel strokes. Xbox face buttons carry their letter in the pad's colours (A green, B red, X blue, Y
     yellow); LB/RB/LT/RT copy the game's R1; keys are caps with a gap round them and a deeper bottom edge. Mockup:
     `button_mock2.png` (the game's, Xbox, keyboard).
   - Drawn when the texture is uploaded (once, whole) and when the device or the bindings change.
3. **Words stay words.** "[SELECT] button" and the like are left as the game wrote them.

## Steps

1. **Lab check:** which screens draw from the strip (field and world dialogue, the Config screen, battle help, the
   save menu): paint the strip a solid colour in a VRAM edit and take pictures of each.
2. **Bindings:** `controls.txt`, the new defaults, Escape. Test: the file's parsing, and a pad word from a set of held
   controls.
3. **Glyphs:** the strips for Xbox and keyboard, switching on the last device used. Lab check: a tutorial line and
   the Config screen with each.

## Done

- **2026-11-09, glyphs and the pad:** `pc/buttons.c` draws the Xbox set (and View and Menu over Select and Start,
  which Customize shows); the shoulders copy the game's R1 exactly, letters in colour 1, since the menu draws them with
  another palette. The window switches glyphs on the last device used, a PlayStation pad keeps the game's; A is OK, B
  cancel; Escape no longer quits. `--buttons xbox` shows them in the lab. Checked on Config's Customize screen; not yet
  on a dialogue line with a glyph (same strip and palette as Customize's faces).
- **2026-11-09, the keyboard:** the JKLI layout bound in `pc/sdl.c`, and its key caps (`--buttons keys`), checked on
  Customize.
- **2026-11-13, `controls.txt`:** `pc/controls.c` holds the bindings (the defaults, then the file's lines; tested
  without a window), and the key caps show the bound keys (A-Z, 0-9, Enter, Tab; a blank cap for others). Not yet
  tried by hand in the window.

## Non-goals

- The mini-games' own instruction graphics, the words in the text, the manual.
- An in-game rebinding screen: `controls.txt` and the game's Config cover it.
- Analogue control beyond the stick as a d-pad.

## Decided (2026-11-09)

1. **A is OK (○)**, B cancel (✕).
2. **Escape does nothing;** closing the window quits.
3. **Keyboard:** WASD (or the arrows) moves; the right hand's J K L I are the face buttons as a pad's diamond (J OK,
   K cancel, I menu, L EXT); Q/E L1/R1, 1/3 L2/R2, Enter Start, Tab Select.
4. **Glyphs follow the last device used.**
5. **Nintendo pads** follow the label (A is OK), as SDL reports them: my call, not asked.
6. **The look** (`button_mock2.png`): approved.
