# Final Fantasy VII (PS1, NTSC-U) knowledge base

Facts about the game's discs, code and data formats. Claims were checked against our own disc images; guesses say so,
and outside sources are named where a fact comes from them.

## 1. Target discs

| Disc | Image | Boot exe | Exe SHA1 |
|---|---|---|---|
| 1 | `Final Fantasy VII (USA) (Disc 1).bin` (MODE2/2352, 1 track) | `SCUS_941.63` | `a95e8b16b97071203b953bb81a33980509262f30` |
| 2 | `... (Disc 2).bin` | `SCUS_941.64` | same as disc 1 |
| 3 | `... (Disc 3).bin` | `SCUS_941.65` | same as disc 1 |

- The three boot executables are **byte-identical** (397,312 bytes); only the file name and `SYSTEM.CNF` differ.
  `INIT/KERNEL.BIN` and `FIELD/FIELD.BIN` are identical across discs too, so code recovery needs disc 1 plus any
  overlay that exists on a single disc.
- Main exe: loads at `0x80010000`, `$gp = 0x80062D44`.

## 2. Disc layout (disc 1, 3,377 files)

| Dir | Files | What it is |
|---|---|---|
| `INIT/` | 3 | `KERNEL.BIN` (game tables + text, gzip-chunked), `WINDOW.BIN` (font/UI), `YAMADA.BIN` (80 bytes: LBA/size table of boot-time files) |
| `FIELD/` | 2374 | `FIELD.BIN` (field engine overlay, compressed), `*.DAT`/`*.MIM`/`*.BSX` per map, `DSCHANGE.X`, `ENDING.X` |
| `BATTLE/` | 7 | `BATTLE.X` (battle engine), `BATINI.X`, `BATRES.X`, `BROM.X`, `SCENE.BIN` (encounters), `TITLE.BIN`, `CO.BIN` |
| `ENEMY1..6/`, `STAGE1..2/` | ~430 | Battle models and backgrounds |
| `MAGIC/` | 318 (289 `.BIN`) | One code+data overlay per spell / summon / limit effect |
| `MENU/` | 26 | One overlay per menu screen (`*.MNU`) |
| `MINI/` | 38 | Mini-games: `CHOCOBO`, `JET` (coaster), `CONDOR`, `HIGHWAY` (bike), `SNOBO`/`SNOBO2` (snowboard), `SUBMAR` (submarine) |
| `WORLD/` | 43 | `WORLD.BIN` (world map overlay), `WM*.MAP/BOT/TXZ/EV` world data |
| `SOUND/` | 5 | AKAO sound banks, see §4 |
| `MOVIE/` | 72 | 51 MDEC movies: 28 `.MOV` with interleaved XA audio (Form 2 sectors), 23 silent `.STR`; the rest are camera/sound data |
| `MINT/` | 2 | `DISKINFO.CNF`, `MOVIE_ID.BIN` |

Compression (all checked on disc 1 with `tools/unpack.py`):

- **Gzip overlays** (`BATTLE/*.X`, `FIELD/FIELD.BIN`, `WORLD/WORLD.BIN`, the mini-game `.BIN`s): `u32` decompressed
  size, a second `u32`, then a gzip stream. The size matches in every case.
- **Plain gzip:** `BATTLE/TITLE.BIN`, no header.
- **BIN-GZ archive** (`INIT/KERNEL.BIN`, `INIT/WINDOW.BIN`): each section is a 6-byte header (`u16` gzip length,
  `u16` decompressed length, `u16` section type; Gears calls the last two "unknown" and "file number") and its gzip
  data. A zero `u16` ends the archive.
  - `KERNEL.BIN`: 27 sections. Types 0–8 are the data tables; all 18 text sections are type 9.
  - `WINDOW.BIN`: 3 sections (33,312 + 32,800 + 1,302 bytes). Gears' offset table for it doesn't match the disc.
- **LZS** (`FIELD/*.DAT`, 703 of 787 files): `u32` **compressed** length (file size − 4; Gears says decompressed),
  then LZSS data with a 4 KiB window. All 703 decompress to a sane 7-pointer header. The other 84 `.DAT` files are
  2,048-byte uncompressed stubs, likely placeholder or cut maps.

**The three discs share one layout.** Of the 3,337 files on all three, 3,325 have the same sector, size and contents
on each. The rest:
- `MOVIE/`: each disc has its own set from sector 128,000 on (72, 79 and 48 files; 33 names on all three, 23 of them at
  the same sector).
- `SNOVA/` (17 files, sectors 127,101–127,660): disc 3 only; discs 1 and 2 have nothing there.
- `FIELD/TUNNEL_6.MIM`: same place and size, but disc 1's contents differ.
- `MINT/DISKINFO.CNF` starts `DISK0001`/`2`/`3` (the game's disc check, §7); `MINT/MOVIE_ID.BIN` differs too.

A movie name found on several discs has the same contents on each, so the PC port needs one copy of each movie.

**Movies** (`tools/movie.py` rewraps them for the PC port):
- Each video sector starts with the standard 32-byte STR header (`u16` `0x0160`, `u16` `0x8001`, sector index and
  count within the frame, `u32` frame number from 1, `u32` frame size, `u16` width and height); 2,016 bytes of frame
  data follow. A frame's data opens with 40 bytes of camera data, then the MDEC frame: `u16` code count, `0x3800`,
  `u16` qscale, version 1, then the libpress bitstream. All are 320×224 except `ENDING2E`/`ENDING3E` (640×192).
- `.MOV` files put an XA audio sector (stereo, 37.8 kHz, 4-bit) every 8th sector. `.STR` files are video only, all
  Form 1, so `tools/extract_disc.py` writes them as 2,048-byte sectors.
- 15 `.MOV`s set the emphasis bit in every audio sector (`CANON`, `FALLPL`, `GELNICA`, `HWINDJET`, `JAIROFAL`,
  `MAINPLR`, `NORTHMK`, `NRCRL`, `ONTRAIN`, `PARASHOT`, `PLREXP`, `RCKTFAIL`, `SOUTHMK`, `U_ROPEGO`, `U_ROPEIN`). Their
  audio was mastered with the CD's 50/15 µs pre-emphasis (IEC 60908): against the same movies on the PC release,
  their spectrum rises by that curve to within 1 dB, while unflagged movies match flat. The PS1 de-emphasizes them on
  playback (psx-spx; its exact filter is undocumented).
- The movie player (§7) calls libpress (`DecDCTvlc`, `DecDCTin`) and its reset loads the default quantization tables
  at `0x80051A50` (luma) and `0x80051A90` (chroma). Both tables are the same.
- Of the 40 bytes before each MDEC frame, the player copies all to `movie_camera` for the field; the last `u16` is a
  colour flag: 1 decodes in 24-bit colour from the next frame on, 0 in 16-bit. A movie without camera data has
  zeros and that 1.
- `MINT/MOVIE_ID.BIN` is the movie table, by number (54 entries on disc 1): 20 bytes each, `u32` sector, `u32` size,
  `u16` x, y (where the frame goes), `u16` w, h, `u16` last frame, `u16` type. Type 0 streams an STR or MOV, and w, h
  are its XA volume, left and right (255 for most, down to 152). Types 2 (looping) and 3 (once) are read into memory
  whole: a table of `u32` frame offsets, then a bare MDEC frame at each, no STR headers, no camera data. There are
  seven: `FSHIP2.BIN` and `FSHIP2N.BIN` (320×128, 45 frames, type 2), `OPENMV.BIN` (320×224 at y 8, 30 frames, type
  3), and 320×224 type 2 loops `WHITE2.BIN` (discs 2–3), `LAST4_2.BIN`, `LAST4_3.BIN` and `LASTMAP.BIN` (disc 3).
- Only the field opens movies. Its main loop waits `VSync(2)` (30 fps), or `VSync(3)` while a streamed movie plays;
  an in-memory movie (`D_800965E4` = 1) keeps 30 fps, and advances one frame per pass, since the field calls
  `cd_step` once per pass.
- Each movie fades the XA audio up (sound command `C9`, 30 ticks) when it opens and down 15 frames before its end. On
  disc 1, movie `0x26` (`HIWIND0`) slides to 0 and `0x31` (`GOLD7_2`) to `0x1800` instead of `0x7FFF`.
- The movies are 15 fps, and the CD streams 150 sectors a second, about 10 per frame. In most `.MOV`s the frames
  arrive a little late (0.6 s by the end of `ENDING2E`); in `SMK.STR` and `LSLMV.STR` they arrive early. The
  converter shows each frame at the next 15 fps tick or when its data has arrived, whichever is later.
- Audio usually runs past the last frame (median 3.2 s).

**Text and `KERNEL.BIN`** (`tools/kernel.py` dumps it to JSON and text files and builds it back):
- **Characters:** a game byte plus 0x20 is Mac OS Roman. `WINDOW.BIN`'s second TIM is the font (4-bit, 12-pixel
  cells, 21 a row) in that order; a few Mac Roman symbols (†, §, ©, ®, ™) are blank in it. From 0xE0 they're codes:
  `EA`–`F1` take a big-endian `u16` that `func_80014E74` fills in (`EA` a character's name, `EB` an item, `EC` a
  number, `ED` a party member or enemy, `EE` and `F0` other name lists, `EF` one character, `F1` any list by list and
  index); `F8` (colour) and `FA`–`FE` (more glyph pages) take a byte (the drawing at `0x80026F7C`); `FF` ends a string.
- **Sections:** 0–8 are tables, 9–26 text. A text section is a `u16` offset per string, then the strings. The help
  texts (9–16) and the battle text (25) are packed: `F9 b` copies `(b >> 6) * 2 + 4` bytes of the packed section from
  `(b & 0x3F) + 1` bytes before the `F9` (`func_800150E4`). The names (17–24, 26) aren't (`func_80014D58` copies them
  as they are; packed, "Hi-Potion" came out "Hi-i"). A copy can bring an earlier string's `FF`, which ends the text.
  The battle text's inserts are filled in before its copies are unpacked, at positions counted in the packed string,
  so the original never puts a copy after an insert (0 of 33) and neither may a rebuild. Section 9's first offset is
  `0x3F`, a byte into its own table.
- **Loading:** `load_boot_files` reads the file to `0x801B0000` by `YAMADA.BIN`'s entry (sector 622, 22,376 bytes).
  `func_80014C80` gives each text section the next part of `0x80063690`–`0x8006947C` (24,044 bytes; the original
  uses 23,030) without a check and keeps its offset in the `u16` table at `0x80069490`. The tables load whole:
  commands `0x800707C4` (32 × 8 bytes), spells `0x800708C4` (128 × 28), section 2 `0x80082268`, items `0x800722CC`
  (128 × 28), weapons `0x800738A0` (128 × 44), armor `0x80071E44` (32 × 36), accessories `0x80071C24` (32 × 16),
  materia `0x800730D0` (96 × 20). Section 3 is a new game's save data: `func_80014934` reads the file again and copies
  it to `0x8009C738`. `func_8001FAF8` numbers the inventory: items 0–127, weapons 128–255, armor 256–287, accessories
  288–319.
- **Fields shown by the game:** with each candidate bumped by a different amount, Cloud's Status and Equip screens
  moved by exactly that much for weapon attack (`+0x04`), hit % (`+0x08`) and stat bonus (stat at `+0x14`, amount at
  `+0x18`; 0 Strength, 1 Vitality, 2 Magic, 3 Spirit, 4 Dexterity, 5 Luck), and armor defense (`+0x02`), magic
  defense (`+0x03`), defense % (`+0x04`) and magic defense % (`+0x05`). The Status screen's Magic def leaves the
  armor's magic defense out (it showed Spirit alone, before and after); the Equip screen shows it. Growth 1 shows as
  Normal; a slot byte is 5 for a single slot, 6 and 7 for a linked pair, 0 for none. The `u16` that
  `func_8001FAF8` returns for every kind is clear where allowed: 1 sell (the shop), 2 battle, 4 menu, 8 throw (the
  item menu's tests; an Antidote can't be used from the menu, a Hardedge can be thrown, the Buster Sword can't be
  sold). An item's power times 20 is what Potion and Hi-Potion say they restore; a spell's MP cost is the `u16` at
  `+0x04`; materia lists the AP for each level in hundreds (Fire: 2,000, 18,000, 35,000). The rest, sections 2 and
  3 among them, are dumped as bytes until something shows what they are.
- **Rebuilding:** the dump of an unchanged file builds back to the same tables and the same text, string for string;
  the packing differs (the original keeps more repeats as they are), so it comes out smaller: 21,832 bytes, 21,032 of
  text. A bigger file needs `YAMADA.BIN`'s size (the game reads that much) and `files.txt`'s (the PC port maps sectors
  to files by it); on the disc there are 17 sectors before `MINI/CHOCOBO.BIN`. Checked in the lab: a renamed Potion
  and its new help text show in the item menu, and a battle runs to the same frame, pixel for pixel.

**`BATTLE/SCENE.BIN`** (`tools/scene.py` dumps it to a JSON file per scene and builds it back):
- **Blocks:** 33 blocks of 8 KB, the same on all three discs. A block starts with 16 `u32` offsets in 4-byte words
  (`FF` for none) and holds up to 16 gzip streams, one per scene, `FF` after them; the battle reads a block alone
  (the trace's `SCENE.BIN +N` is 4 sectors a block). 256 scenes, 7,808 bytes each unpacked. Which block a scene is in
  comes from `KERNEL.BIN` section 2 at `0xF1C`: the first scene of each block, `FF` after the last (`BATINI.X` reads
  it), so the two files have to agree. Battle number N is scene N / 4, setup N % 4: the opening battle, `0x12C`, is
  scene 75 (MP and Guard Hound) setup 0.
- **A scene:** three `u16` enemy IDs (`FFFF` for none, then `FFFF`), four 20-byte setups (`0x008`), four 48-byte
  cameras (`0x058`), 4 × 6 16-byte formation entries (`0x118`), three 184-byte enemy records (`0x298`), 32 attacks in
  the kernel's 28-byte spell record (`0x4C0`), their `u16` IDs (`0x840`) and 32-byte names (`0x880`), then the
  setups' AI scripts (`0xC80`, 512 bytes) and the enemies' (`0xE80` to the end, 4 KB). Names are 32 bytes, the text
  then `FF` to the end, in every scene. Scenes 0–2 number their enemies (`…0(…)` to `…6(…)`) in bytes this font shows
  as accented letters. An enemy record starts with its name; the battle copies them to `0x800F5F44`, where the battle
  text's `ED` insert takes names from.
- **Setups, cameras and formations:** `BATINI.X` copies the scene's IDs and the battle's own setup, cameras and
  formation to `0x8016360C` (setup `0x80163614`, cameras `0x80163628`, formation `0x80163658`). A setup's `u16`
  `+0x00` is the battle area (an 8-byte entry of the file table at `0x800E7D98` that the battle loads), `+0x02` the
  battle that follows when this one ends (`FFFF` none), `+0x04` the escape counter (it counts down while the party
  runs; held L1+R1 in the opening battle got away at frame 18400 with 9, 15620 with 1), `+0x08` four battle IDs
  `func_800A35F8` picks from when flag `0x40` of `0x8016376A` is set (999 or the battle itself in most), `+0x10`
  flags (bit 2 set allows escape: the opening battle's `FFF9` says "Can't escape!", `FFFD` lets the party run),
  `+0x12` the layout (`0x801B0044` turns it into the battle type; 2 put the party on the enemies' far side) and
  `+0x13` the camera script the battle starts (`0x80163F30`). `+0x06` is `FFFF` everywhere. A camera is 12 bytes,
  four per setup: a position and the point it looks at, three `i16` each (`0x80158D00`, `0x801031E8`). A formation
  is six 16-byte entries: the enemy ID (`FFFF` none; `BATINI.X` turns it into the record index), `i16` x, y, z (MP's
  x from -500 to 1,500 moved it to the screen's edge), `u16` row (into the actor's `0x4E`), a `u16` cover mask (an
  enemy is covered, actor flag `0x800`, when one in a lower row shares a bit) and `u32` flags whose low five bits
  go to the actor. Changing the first entry's ID from MP's to Guard Hound's put a Guard Hound in its place.
- **Enemy fields shown by the game** (the opening battle, two MPs, from a new game): with MP's `+0xA8` (`u32`) at
  1,000, Cloud's EXP (`0x8009C774`) rose 2,000 instead of 32; `+0xAC` (`u32`) at 300 put 600 gil at `0x8009D7E0`
  instead of 20; `+0x9E` (`u16`) at 7 gave the materia 14 AP instead of 4; `+0xA4` (`u32`) at 1 turned up as the
  enemy's HP in the battle's copy (`0x800F5FE8`) and its actor (`0x800F85AC`).
- **The rest of the record, from the code that reads it:** `BATINI.X` builds each enemy's actor (`0x800F8580`, `0x68`
  bytes) from it, into the fields it fills for the party from the 64-byte block at `0x8009D84C` that the Status
  screen draws (bytes `+2`–`+7` Strength, Vitality, Magic, Spirit, Dexterity, Luck, as `func_8001FCDC` numbers them;
  `u16` Attack `+8`, Defense `+0xA`, Magic atk `+0xC`, Magic def `+0xE`). So `+0x20` level (the party's comes from
  the save's level byte), `+0x21` dexterity, `+0x22` luck, `+0x23` defense % (the party's is the armor's), `+0x24`
  strength, `+0x25` defense and `+0x27` magic defense (both doubled into the actor), `+0x26` magic, `+0x9C` MP. In
  the lab, MP's strength at 60 instead of 6 took Cloud to 87 of 302 HP before the battle ended (284 left without).
  - Items: four rate bytes at `+0x88` and four `u16` inventory numbers at `+0x8C` (`FF`/`FFFF` for none).
    `func_800B0F04` takes the first slot whose top two bits match (`0x80` steal, `0` drop) and whose low six bits,
    as they are for a drop and times a factor `func_800A2974` works out for a steal, are at least a 0–63 roll. MP's
    first slot at `0x3F` with item 5 had both MPs drop an Elixir in the lab (the spoils list at `0x8009D808`).
    `+0xA0` is the item Morph turns it into (`FFFF` for none).
  - Attacks: 16 `u16` scene attack IDs at `+0x48`; for the one being used, the byte at the same index from `+0x38`
    is its animation and the `u16` from `+0x68` its camera (`FFFF`: the attack's own). `+0x94` is three `u16`
    attack IDs, the Manipulate menu's (`BATINI.X` looks them up in the scene's list); the first is what it does
    when Berserk, and under one more status unless bit 0 of `+0x9A` is clear.
  - Elements: eight ids at `+0x28` (`FF` none; elements below 32, statuses from 32) and at `+0x30` the kind each
    is (the battle keeps a bit mask per kind; 0, 1, 2, 4, 5 and 6 are used, not named yet). `+0xB0` is the
    statuses it can be given: `FFFFFFFF` for most enemies, bits cleared for bosses (the actor keeps the inverse).
  - `+0xA2`: damage from behind is multiplied by it over 8 (16, double, for nearly all; the party's is 16 too).
    `+0x9A` is `FFFF` or `FFFE`, `+0xA3` and `+0xB4`–`+0xB7` are `FF` in every record.
- **AI scripts:** `BATINI.X` copies both parts to `0x800F6BA4` and `0x800F6DA4`. Each starts with a `u16` offset per
  setup (4) or enemy record (3) to a block (`FFFF` none); a block is 16 `u16` slot offsets from its start (`FFFF`
  none), then the scripts, in slot order, one after another. `func_800A5FB0` finds one (it clears bit 0 of the
  block's offset, so blocks start even: 174 scripts end in an `FF` that pads them). `func_800A6000` runs an enemy's
  (slot 0 from `BATINI.X`; 1 on its turn: a script that showed text and used one attack instead of MP's did both;
  3, 4 and 7 from elsewhere in `BATTLE.X`), `func_800A61D4` the setup's slots 0–7; the party's come from
  `KERNEL.BIN` section 2 (`0x80082884`), in the same block format.
  - `func_800B1D48` runs them, a byte stack machine (stack at `+0x7C` of its state, `0x800F4AC4`): an operation's
    high nibble picks its group. `0X` push a variable and `1X` its address (a `u16` operand; the low nibble is the
    size, 0 bit to 3 word; `func_800B13B0` maps an address below `0x2000` to the actor's own 128 bytes at
    `0x800F87F0`, from `0x2000` to the battle's at `0x800F83A4`, from `0x4000` to the actors' at `0x800F83E0`), `3X`
    add, sub, mul, div, mod, and, or, not, `4X` eq, ne, ge, le, gt, lt (the left operand deeper in the stack),
    `50`–`52` logical and, or, not, `60`–`62` push a 1-, 2- or 3-byte constant, `70` jump if false, `71` jump if not
    equal, `72` jump (a `u16` from the script's start), `73` end, `74` pop, `90` store, `92` attack
    (pops a command and an attack ID), `93` text (to `FF`, with `EA`–`F1`'s two bytes skipped), `A0` debug (a count,
    then a NUL-ended string). Every other byte is a 1-byte operation; `80`–`87`, `91`, `94`–`96` and `A1` do things
    not named yet.
  - Every script on all three discs disassembles to the end of its bytes and its jumps land on operations;
    `tools/scene.py` dumps them a line per operation with labels and assembles them back byte for byte.
- **Rebuilding:** every scene dumps and builds back byte for byte, and packed in order they land in the original
  blocks. `SCENE.BIN` can't grow: `BATRES.X` follows it on the disc. `build` remakes `KERNEL.BIN` in each disc folder
  with the table for the new blocks, and the kernel tool takes the table from the folder's `SCENE.BIN` too, so
  rebuilding either keeps them in step.

**`FIELD/*.DAT`, the maps** (`tools/field.py` dumps each map to a JSON file and builds it back):
- **Loading:** `FIELD.BIN` keeps a 24-byte entry per map at `0x800DA5B8` (file offset `0x3A5B8`): sector and size of
  its `.DAT`, `.MIM` and `.BSX`, by map number. `func_800A1368` unpacks the `.MIM` (LZS) to `0x80128000` and the
  `.DAT` to `0x80114FE4`, so a map unpacks to at most 77,852 bytes (`FRCYO`, the biggest, is 77,776). The `.DAT`
  starts with seven pointers into that copy, one per section; they're 4-aligned. The exe reads `FIELD.BIN` itself
  by a sector and size of its own (`D_80048D2C`), but gzip stops at the end of its stream, so a rebuilt one padded
  to the old size reads the same (level 9 packs the image 82 bytes smaller than the original).
- **Section 1, the scripts** (`func_800BA534` checks it, `func_800BA7C4` and `func_800BAF54` set the entities up):
  `+0` version 2 and 5, `+2` entity count, `+3` model count, `+4` the dialogue's offset (0 for none), `+6` the count of
  music blocks, `+8` a `u16` the field keeps; entity names (8 bytes each) from `+0x20`, then a `u32` offset per music
  block, then 32 `u16` script offsets per entity, then the scripts. Each entity runs from its own position
  (`D_800831FC`, an offset from the section, `D_8009C6DC`); several slots point at one script, and in `JUNIN2` two
  entities share one.
- **The interpreter:** `D_800E0228` holds a handler per operation. Each handler shows its name through the debug
  printer `func_800BEAD4(name, bytes)` (the names in `FIELD.BIN`'s rodata; `tools/field.py` reads them from there)
  and moves the entity's position on itself, by the operation's length. Lengths come from those moves:
  `spcal` (`0F`) by its sub-code (`F5`–`FF`), `kawai` (`28`) by its own length byte; 13 codes have no handler of their
  own ("Bad Event code!"). Jumps: `skip`/`lskip` (`10`/`11`), the `if` family (`14`–`19`), `key!`/`keyon`/`keyof`
  (`30`–`32`) and `prtyq`/`membq` (`CB`/`CC`) count forward from their operand, `back`/`lback` (`12`/`13`) back from
  the operation. Every script on all three discs (2,109 maps, 144,180 scripts) decodes exactly to the next one's
  start, all 122,658 jumps land on an operation, and 1,854 of them on the end of their own script.
- **Values and variables:** handlers read operands through `func_800BEE10` (a byte) and `func_800BF908` (a `u16`) and
  write them through `func_800BF3AC` and `func_800C0248`, each given a bank number 1–6 (the high or low nibble of
  operand byte 1, 2 or 3) and the operand's offset. With the nibble 0 the operand is the value; otherwise it's an
  address in that bank. Banks 1/2 are `0x8009D288`, 3/4 `+0x100`, 11/12 `+0x200`, 13/14 `+0x300`, 15/7 `+0x400`
  (1,280 bytes kept in the save; the first of each pair reads a byte, the second a `u16` at the same address), 5/6
  `0x80075E24`, each map's scratch space (8–10 aren't banks). Harvesting each handler's constant arguments gives 130
  operations' value operands (`tools/field.py`'s `SPEC`; the if family's come from its compare helpers
  `func_800C2000`, `func_800C24A8` and `func_800C2970`, which switch on an operator byte: `==`, `!=`, `>`, `<`, `>=`,
  `<=`, `&`, `^`, `|`, bit set, bit clear). The dump shows them as `#value` or `var[bank][address]`, the if family as
  `if var[1][2B] == #5 L0034`; an operation that can't be said that way exactly (177 `mpara`/`mpra2` use a nibble
  the table doesn't name) stays in hex. Checked in the lab: `batle #300` in `MD1STIN` made `#324` opened the opening
  fight as battle `0x144`, Guard Scorpion's.
- **Saved variables with a known meaning** go by name in the dump (`tools/field.py`'s `VARS`), each from the code
  that uses it outside the scripts: `story` (bank 2 `00`; set only to constants, 0 to 1999, and the world map picks
  its version by 1000, 1580 and 1620), the play time and countdown (bank 1 `10`–`12` and `14`–`16`, from the
  field's second counters `D_8009D264`/`D_8009D268`), `battles` (`main` adds 1 per battle), `menu_shown` and
  `menu_locked` (a bit per main menu entry, Item to Save; a field load locks PHS and Save), `battle_points` (the
  arena's `u32`, drawn under "Current Battle Points"), the party (bank 3 `09`–`0B`), `disc` (`dskcg`; `main` swaps
  discs when it differs), `materia_full` (`smtra` when all 200 slots are taken) and the mini-games' scores, courses
  and chocobo settings. Checked in the lab on a new game: `menu_shown` `0x2FB` (`MD1STIN`'s `#763`), `menu_locked`
  `0x300`, `disc` 1, the party `00 FF FF`, play time 2:30 at frame 9,000.
- **Dialogue:** a `u16` count, an offset per string from the table, the strings ending `FF`. The field's text parser
  (`func_800D5C9C`) switches on `E0`–`FF`: `EA`–`F2` are names and take nothing, `FA`–`FD` take one byte, `FE` a
  sub-code (`D2`–`E9`; `DD` waits on two more bytes, `DE`, `DF`, `E1` and `E2` insert numbers). An argument can be
  `FF`, so a string runs to the next one's offset. Some maps have bytes nothing points at after the strings (12 KB in
  the `BLIN66` maps, an old table), strings that share an offset, separate empty ones, or a pointer with no room for
  a count behind it; a rebuild keeps all of it, and every map dumps and builds back byte for byte.
- **Checked in the lab:** Barret's "C'mon newcomer. Follow me." in `MD1STIN`, made longer, showed in the opening
  (the map 28 bytes bigger, `FIELD.BIN`'s entry moved with it), cut off by its window until the `wsize` before the
  `mes` was made wider in the script.
- **The other sections** (`tools/field.py` dumps and builds all of them; every map on all three discs, 2,109,
  byte for byte). At `0x800A1BB0` the field keeps where each header word is: section 2 `D_8009A044`, 3 `D_8009D848`,
  4 `D_80083578`, 5 `D_8009AD28`, 6 `D_8009C55C`, 7 `D_80070784`. `func_800A1368` reads 5, 6 and 7 into
  `D_800716C4`, `D_80071A54` and `D_8007E770`. The sections lie back to back and none holds its own length except 7.
- **Section 2, the walkmesh:** a `u16` count n (at most 496 on the discs), a `u16` nothing reads, n triangles of three
  vertices (`s16` x, y, z, w), then each triangle's three neighbours (`s16`), padded to a word with whatever was
  there.
  - `func_800A8968` finds the triangle a point is on: for each edge (v0→v1, v1→v2, v2→v0) a 2D side test on x and
    y; on the wrong side it steps to that edge's neighbour. Inside, `func_800A8E34` gets the height z from the
    triangle's plane. A negative neighbour is a wall (all are −1).
  - Nothing reads w. It's z in most vertices and other values in about a fifth.
  - `idlck` (`6D`) sets or clears a triangle's bit in `D_8009ACA6` (512 bits). A locked triangle is a wall too.
- **Section 3, the background tiles** (`func_800A3020` builds the sprites, twice for the two buffers;
  `func_800A364C` links them each frame):
  - Four `u32` offsets: layer 1's tiles, the texture pages, layer 2's tiles, layers 3 and 4's tiles.
  - At `+0x10`, a run list per layer: `s16` y, the first tile's index, a count; `7FFE` with an index picks the next
    texture page (layers 1, 3 and 4); `7FFF` ends the list. A run's tiles share its y. The index counts across all
    four layers, the tiles' place in the field's state bytes; the build works it out.
  - Layer 1: 16×16 tiles, 8 bytes (`s16` x, y, `u8` u, v, `u16` palette), opaque, at the back of the ordering
    table, culled by row and column against the camera (`D_80071A48`, `D_80071A4A`).
  - Layer 2: 16×16 tiles, 14 bytes: also a texture page each, a depth (its slot in the ordering table, 0–4095), a
    flags byte (bit 7 blended, bits 0–5 a group) and the state bits it shows in.
  - Layers 3 and 4: 32×32 tiles, 10 bytes: the flags byte and state bits. Each layer is linked at one slot
    (`D_8009ACA4`, `0xFFF` at load; `D_8009ACA2`, 1), and scrolls by section 5's values.
  - A tile in group g shows when g is 0 or when `D_8009ACE6[g]` and its state bits share a bit. `bgon`, `bgoff`
    and `bgclr` (`E0`, `E1`, `E4`) set and clear those bits.
  - The padding after the last tile isn't always zero; the build keeps it.
  - **Draw order:** each slot of the ordering table draws the packet linked last first, and a layer's page switches
    are linked in its list among its tiles. So a switch is the page of the tiles before it in the file: lists end
    with one (`MD1STIN`'s layer 1, nine). Layer 3 is linked after layer 1 at the same slot, so it's drawn behind it.
  - **The `.MIM`** (LZS, unpacked to `0x80128000`; `func_800A2D5C`): blocks of a `u32` length (with this 12-byte
    head), x, y, w, h and the VRAM words, a 0 length after the last. The first is the palettes, `LoadImage`d at
    (0, 480) 256×16 whatever its head says; then one or two 8-bit pages (`LoadTPage`), all at x 384 or more.
    `TUNNEL_6`'s second block runs 18 KB past the file's end, so the game loads what lies after it in memory.
  - **Checked:** `tools/model_editor.py` draws a map's background with the port's GPU this way. `MD1STIN`'s, cut
    at the camera's place in a lab frame, matches the frame in 98.4% of its pixels, exactly; the rest are the
    characters (the frame widens each 5-bit colour as `c << 3 | c >> 2`).
- **Projection** (`func_800A41CC`): a field position (x, y, z; z up) goes through `RotTransPers` with section 4's
  rotation and translation and a geometry offset of 0, so a 3D point lands at background pixel (x, y) × H / z. The
  camera centres on the player's point so found, kept in section 5's scroll box. Walkmeshes drawn so lie on their
  floors (`MD1STIN`, `FSHIP_2`, `GAMES`).
- **Section 4, the camera:** `func_800AB2B4` points `D_80071E40` at it (or at a movie's camera).
  - A rotation (`s16` 3×3, 4.12) for `SetRotMatrix`; a word nothing reads (m[2][2] again in every map); a
    translation (`s32` ×3) for `SetTransMatrix`; an `s16` screen offset x and y (0 everywhere); the projection
    distance H (`SetGeomScreen`, `func_800A4BEC`).
  - The rest (2 bytes, or 18 in 216 maps) is never read.
- **Section 5, the map** (740 bytes everywhere):
  - **Header:**
    - The name (lower case) and a byte nothing reads.
    - The pad's turn, a byte added to the direction pressed (`func_800A65A4`).
    - The camera's focus height, added to the player's z (`D_8009AC0A`).
    - The scroll box (`s16` min x, min y, max x, max y): `func_800A47F8` keeps the camera within it, less half the
      screen.
    - A mode byte: 1 or 2 slides the camera along one of its diagonals (`func_800A45D4`).
    - Layers 3 and 4's wrap width and height, start scroll (1/16 pixel) and parallax (8.8, by the camera's
      position; `func_800A4BEC`).
  - **12 exits** at `0x38`, 24 bytes each: a line (two `s16` x, y, z), then the arrival x, y, triangle, map and
    facing (copied to `D_8009ABF8`, `ABFA`, `AC16`, `ABF6`, `AC18`). Map `7FFF` is an unused slot. Three bytes after
    the facing are never read (the facing again in every used exit).
    - `func_800AA348` fires an exit when the player, within its collision radius of the line, crosses from one side
      to the other. `D_8009ABF5` = 1 then asks for the map.
    - `func_800A14D8` starts reading the nearest exit's background early.
  - **12 triggers** at `0x158`, 16 bytes each: a line, a background group (`FF` unused), a bit, a kind and a sound
    (one of 4).
    - Even kinds set the bit while the player touches the line (within the collision radius, and facing within
      `0x40` of the nearest point or standing on it); odd kinds clear it. Kinds 2–5 do the opposite every frame the
      player doesn't (4 and 5 only on one side of the line); 0 and 1 never undo. The sound plays only when the
      bit changes.
    - At load, even kinds start with the bit clear and odd kinds with it set.
  - **A flag per exit** at `0x218` draws an arrow at the line's middle (`func_800ABFE8`: the middle in 3D,
    halved toward 0, projected; a 16×16 sprite at −7, −8 from it; skipped when its x and y are 0).
  - **12 markers** at `0x224` (x, y, z as `u32`s read as `s16`, and a kind: 0 off, 2 drawn with the palette at (256, 488), others with
    the default one).
- **Section 6, random encounters:** two tables of 24 bytes; `btltb` (`4B`) picks one (`D_8009AC30`), and
  `btlon` (`71`) with a value other than 0 stops random battles (`D_8009AC2F`; `MD1STIN` runs `btlon 01`).
  - `func_800ABA70`, each step: the danger (`D_8007173C`) goes up by the player's actor `+0x70` over the rate (the
    table's `+0` high byte; bit 0 is the table's on switch), and a battle comes when a random byte falls under the
    danger times `D_80062F19`, over 4096. The main exe sets `D_80062F18`–`1B` from the party (`0x800176B0`).
  - **The special battles** (`+0xE`, `+0x10`, `+0x12`, `+0x14`) are tried first, against a roll out of 64.
    `D_80062F1B`'s bit 7 halves their chances.
  - **Otherwise one of the six battles** (`+2` to `+0xC`), each a `u16` of the chance (6 bits) and the battle
    (10 bits). The sixth takes whatever the others leave. A battle the same as the last is rolled once more.
  - Another roll under `D_80062F1B`'s low 7 bits sets `D_800716D0` to 4 for the battle (the first two special
    battles clear it again).
  - The chances of the six add up to 64 in every table that's on.
- **Section 7, the models:** a `u16` size, a `u16` count, then 8 bytes each: face id, bones, parts, animations, a
  slot the loader fills in (0 in the file), whether to load it, a 0, the `.BCX` number (0: in the `.BSX`).
  - `func_800BB1B4` sets "load" for the party's models before `func_800ADAA4` hands out slots.
  - Entities whose model isn't loaded are hidden.
- **Checked in the lab:**
  - A second exit across Cloud's walk north in `MD1STIN`, after the opening fight, took him to the debug room
    (`0x41`).
  - `MD1_1`'s table, given only battle `0x144` and rate 1, started that battle within a few steps (the warp given
    the arrival `MD1STIN`'s own exit uses).

**`FIELD/*.BCX` and `.BSX`, the field's models** (`tools/model.py` dumps them to JSON files and builds them back;
decoded from the loader and the drawing code, every `.BCX` and real `.BSX`, 721 files the same on all three discs,
dumps and builds back byte for byte):
- **Loading** (`func_800AA930`): all LZS.
  1. The map's `.DAT` section 7 lists its models (see the maps above).
  2. The `.BSX` (the third pair in the map's `FIELD.BIN` entry) unpacks to `0x801B0000`.
  3. `func_800ADC90` loads each `.BCX` once (table `D_800DF08C`: 1 `CLOUD`, 2 `EARITH`, 3 `BALLET`, 4 `TIFA`, 5 `RED`,
     6 `CID`, 7 `YUFI`, 8 `KETCY`, 9 `VINCENT`) and `FIELD.TDB` to `0x80128000`.
  4. `func_800AC35C` copies the `.BSX` to the free pointer (`D_800E0204`) and builds the GPU packets.
- **`.BCX`:** `u32` size, `u32` header offset H; the data from 8; at H a `0x24`-byte model header (bones, parts,
  animations, a scale, 512 in most), then the bones (4 bytes), parts (32) and animations (16), to the end.
- **`.BSX`:** `u32` size, `u32` H; at H the model count and the texture block's offset, then a `0x30`-byte entry per
  model: face id, `.BCX` number, scale, its block's offset, an ambient colour, and three lights (`func_800B1E40`: a
  colour each, the columns of the GTE's light colour matrix, and an `s16` x, y, z each, turned by the bone into the
  light matrix). For a `.BCX` model the entry only adds animations (`MD1STIN`: Cloud's 3).
- **Bone:** `s16` length, `s8` parent (-1 for the root, else lower than its own index), a byte set when a part hangs on
  it. A bone sits at its parent's matrix times (tx, ty, length + tz).
- **Part** (32 bytes): its bone, vertex and UV counts, the count of each of the eight polygon kinds (GT4, GT3, FT4,
  FT3, F3, F4, G3, G4), the texture words, offsets to its tables, and the packet size. Its data: a zero word, 8-byte
  vertices (`s16` x, y, z, 0, in the bone's space), the polygons (4 vertex indices, then 4 or 3 or 1 colours, then the
  UV indices for the textured ones), `u32` texture words, `u16` UVs, a flag byte per polygon (low nibble the texture
  word, bit 4 semi-transparent, bits 5–6 the blend mode).
- **Colours and shading:** a colour is RGB plus the index of a normal in `D_800DF520` (240 `s16` directions in
  `FIELD.BIN`). As the map loads, `func_800B0A48` lights each one with `nccs` from that normal and the model's lights,
  so the Gouraud colours drawn are the stored ones lit once, not each frame.
- **Drawing** (`func_800AE4DC`): `rtpt`, back faces dropped by `nclip`, the depth for the ordering table from the
  vertices' average (`avsz3` for triangles). The packets are the GPU's own: GT4 `3C`, GT3 `34`, FT4 `2C`, FT3 `24`,
  F3 `20`, F4 `28`, G3 `30`, G4 `38`, `+2` when semi-transparent. On disc 1: 307,208 G4, 288,268 G3, 9,121 GT4, 4,448
  GT3, 4,109 FT3, 779 FT4, 762 F3, 112 F4.
- **Textures:**
  - **Texture word:** its kind places the texture in a slot of VRAM per model, or keeps it where it is. It also holds
    the colour depth, the texture page, and the palette's x/16 and y.
  - **The `.BSX` texture block:** raw VRAM rectangles for `LoadImage`, palettes among them. Its patches can also
    replace a `FIELD.TDB` image or palette, or copy bytes into RAM.
  - **`FIELD.TDB`:** 127 images of 32×32 at 4 bits and 34 palettes. For a face id below 33 the model gets its palette
    and three images in its slot (`func_800B1C7C`, called again at run time): textures it swaps, two side by side and
    one apart.
- **Animation** (16 bytes; `func_800AEE24`): frames, bones, and per bone three rotation angles (a byte, 256 to a turn;
  applied Ry·Rx·Rz, the sine table `D_800DF120`) and three offsets, each either fixed or a channel with a value per
  frame. Scripts pick it by number (`anime`, `canim`, `dfanm`, `animw` ...; `func_800C5B38`). 36 animations have a
  bone count of their own; the engine loops over the model's.
- **Rebuilding byte for byte:**
  - blobs shared by several parts or models (`FARM`, `FRCYO`)
  - the 12 bytes left in a texture block with no images
  - 2 bytes after an odd-sized image in `GHOTIN_2`
  - a junk fourth UV index on 3,172 GT3s

  - the 4 zero bytes ending a `.BCX` header
  - the bytes that 4-align a part's or animation's data: in 538 files some hold leftovers, not zeros

  The 75 2,048-byte `.BSX` stubs aren't models (their LZS length doesn't match the file).
- **Building:** a file is packed again only from its first changed byte (`field.repack`); its size goes into
  `FIELD.BIN` (the map's entry for a `.BSX`, `0x800DF08C` for a `.BCX`). `field.py` and `model.py` each restore only
  their own files and patch only their own entries, and `FIELD.BIN` goes back to its original bytes when nothing in it
  differs.
- **Checked in the lab:** Cloud's parts 0 and 1 (his shirt) given `FF0000` with their normals kept showed bright red
  in `MD1STIN` ("C'mon newcomer"), lit and shaded as before.
- **What drawing them showed** (`tools/model_editor.py`):
  - **Posing:** a bone turns by its parent's turn times its own (Ry·Rx·Rz) and starts at its parent's start plus
    the parent's turn of (x, y, its length + z), x, y, z the animation's offsets for the bone (a moving channel by
    flag bits 4–6, a fixed value, or none at `FF`; `func_800AEE24` always applies them in the field). Every model
    drawn that way stands as in the game.
  - **Lights:** `func_800B1E40` takes them from the `.BSX` entry in order, the colours at `+14`, `+20` and `+2C` and
    the vectors at `+C`, `+18` and `+24`, so the n-th colour goes with the n-th vector. The vectors and the normals
    are unit vectors at 4096.
  - **`FIELD.TDB`:** LZS-packed on the disc like the rest. Every face's lists start with image `0x7E`, which is
    blank, and that's the one the field loads (`a = b = c = 0`). Scripts swap in the eyes and mouths after; drawn
    with each list's second entry, Cloud, Barret and the rest show their faces.
  - **Draw order:** within a depth bucket of the ordering table, the packet added last is drawn first, so a part's
    Gouraud polygons go under its textured ones (an eye on the face).
- **A model on its map** (`func_800AAB24`, `func_800AEE24`, `func_800AE4DC`):
  - **At load:** every bone length and vertex becomes `(v × scale) >> 12`, the scale from the `.BSX` entry (512 in
    most, so ÷8).
  - **Placing:** the model turns by `Rz(facing)·Rx(−64)`, so its −y is up and its −z ahead; facing 0 looks along
    −y and 64 along +x. It stands at the entity's (x, y, z − 10), plus offsets scripts set.
  - **Projecting:** then the map's camera, and `RotTransPers` with offset 0, so it lands in background pixels.
    A model whose own point is deeper than `0xF00 << 2` isn't drawn. The facing drawn is entity `+0x38`; `+0x36`
    is where it's moving, copied over when `+0x37` is 0.
  - **Draw order:** a quad goes at slot `(SZ0 + SZ1 + SZ2 + SZ3) >> 4`, a triangle at avsz3's
    `(0x155 · (SZ0 + SZ1 + SZ2)) >> 12`, back faces dropped (NCLIP). Models share the background's 4096-slot
    ordering table, and are linked after it, so a tile at the same slot covers them.
  - **Draw mode:** the tiles' `SetDrawMode` (`func_80044A68`) has dithering on, and the models drawn after it are
    dithered too.
  - **Before any script moves it:** an entity holds animation 0, frame 0 (`func_800BA7C4` clears them;
    `func_800A6418` advances only to an end frame of 0). The player loops its animation.
- **How a map starts:** at load each entity runs its first script up to its first `ret`, in entity order.
  - `char` gives it the next model in section 7's list (a counter, `D_8009C6C4`; its operand goes to the entity's
    `+0x66`). In 702 of the 703 maps the `char`s there equal the models.
  - `pc` makes it a party member (`D_8009AD30[character]` = the entity); the player has no position of its own
    until a gateway or a script gives one.
  - `xyzi`, `xyi` and `xyz` place it, `dir` turns it, `visi 00` hides it and `dfanm` sets its animation.
  - `bgon`, `bgoff` and `bgclr` set the background groups' bits (`D_8009ACE6[g]`), over the triggers' load state.
  - Section 7's entry n is the `.BSX`'s model n in every map.
- **Checked:** `tools/model_editor.py` draws `MD1STIN` as it starts at story 0, through the port's GPU. Against a lab
  frame just after the opening movie, both guards stand in the same places and poses. Their pixels are a pixel
  off at most and a few levels apart in colour, because the viewer poses and lights in floating point, not the GTE's
  fixed point.

**`ENEMY1..6/*.LZS`, the battle's models** (`tools/battle.py` reads them for the viewer; decoded from `BATTLE.X`'s
loader, renderer and animation code, all 1,170 files on the three discs read; `ENEMY6/SEFFECT.LZS` is the effects'
own format, not a model):
- **Which file:** an enemy's ID in `SCENE.BIN` is its file number (battle `0x12C`, the MP, ID 16, reads
  `ENEMY1/ENEMY016.LZS`). The party's are the named files in `ENEMY6/` (`CLOUD`, `HICLOUD`, `SEFIROS`, ...).
- **Sections** (LZS; a `u32` count and that many offsets): 0 the skeleton and its parts, 1 the settings and action
  scripts (a table at `+0x68`; byte `+2` bit 7: it holds a weapon), then animations; a 4-byte section is an empty
  slot.
  - **Party** (`func_800B5C1C`, `func_800CD400`): body animations from 2 (52 slots), each one's weapon animation 52
    on; the TIM at n - 17; the 16 weapons after it, the one used by the character record's byte `+0x411 & 0xF`.
    Sections up to the TIM are copied to `0x80103200 +` slot `x 0xF000`, the weapon to `0x80163F34`.
  - **Enemy** (`func_800B54B8`, `func_800B5D38`): the TIM last. One with a weapon has 16 body and 16 weapon
    animations and the weapon just before the TIM; the rest have animations up to the TIM.
- **Skeleton** (`func_800C76C8`): `u16` count, a `u16`, then count + 1 bones of `s16` parent, `s16` length, `u32`
  the offset of its part in the section (0: none). Bone 0 is the root (all zeros) and hangs on the actor's own
  node. A bone starts at its parent's start plus the parent's turn of (0, 0, **the parent's** length), and turns
  by the parent's turn times `RotMatrixYXZ` of its angles (`func_800BACEC`). No scale at load; -y is up.
- **Part** (`func_800D29D4`, the hand-written renderer): a `u32` size and the vertices (`s16` x, y, z, pad), then
  four lists back to back. Corners are byte offsets into the vertices.
  - textured triangles: `s16` count, `u16` the part's texture page; 16 bytes each (3 corners, a grey, a code byte
    whose bit 1 is semi-transparency, then UV, the palette, UV, UV)
  - textured quads: `u32` count; 20 bytes each (4 corners, UV, the palette, 3 UVs, grey, code byte)
  - Gouraud triangles: `s16` count, an `s16`; 20 bytes each (3 corners, an `s16` the renderer never reads, 3 GPU
    colour words, code byte included)
  - Gouraud quads: `u32` count; 24 bytes each (4 corners, 4 colour words)

  Unlike the field's parts, the colours come lit: nothing is lit at load or per frame. The renderer forces blend
  mode 1 on the page, adds the party slot's or enemy's page offset at load and 3 palette rows per actor at draw.
- **Animation** (`func_800D376C`, one frame per call of `func_800CD400`): `u16` frames, `u16` the bitstream's
  length in bytes, `u8` shift, then the bitstream, read most significant bit first, values sign-extended.
  - Frame 0: the root's x, y, z in 16 bits each, then every bone's rotation x, y, z in 12 - shift bits, shifted
    left by shift (4096 a turn).
  - Each later frame adds to the last: the root's three with a flag bit (1: 16 bits, 0: 7); each rotation a flag bit
    (0: no change), then 3 bits k: 0 is -(1 << shift), 7 is 12 - shift bits shifted, otherwise k bits v moved away
    from 0 by 1 << (k - 1), shifted.
  - Action scripts pick them: a script byte below `0x8E` plays that animation (`func_800C7C4C`); script 0 loops
    animation 0, the battle stance. See "the action scripts" under "a battle as `BATTLE.X` sets it up".
- **Weapons** (`func_800C7924`): a skeleton like the body's, its root hung on the actor's node, not a hand. Its own
  animation, body animation + 52 (+ 16 for an enemy), puts it in the hand.
- **Checked against a lab dump** of the first battle (Cloud casting Ice, the MP waiting): Cloud's animation 12
  decoded to frame 11 and the MP's animation 0 at frame 0 give every bone's angles and the root exactly as in RAM
  (`D_80151A58 +` actor `x 0xB9C`, `0x34` bytes a bone). Posed, every bone is within 2.5 units of the game's own
  matrices relative to the root, and the angles within the GTE's fixed point.

**A battle as `BATTLE.X` sets it up** (`tools/model_editor.py` draws one; battle N is `SCENE.BIN` scene N // 4, setup
N % 4):
- **The stage** (`STAGE1/STAGE00..49.LZS`, `STAGE2/STAGE50..89.LZS`; `func_800B383C` reads the setup's location
  through the table `D_800E7D98`, a sector and a size per location, which says the same): sections of a `u32` count
  and offsets (bit 31 of one, never set, would draw that model semi-transparent).
  - Section 0 is 8 bytes: a type and a first section. The last is the TIM: 8-bit, at (384, 0) and palettes at
    (320, 504-511), loaded where it says. The rest are models, battle parts (as above) in world coordinates.
  - `func_800B9568` draws each into one of three lists, drawn in this order: the far layer (16 coarse depths), the
    backdrop (one list, so the last linked is drawn first), then the main ordering table, which the actors share.
  - Type 1 (83 of the 90 stages on disc 1): section 1 and 5 up to first in the backdrop, 2-4 the far layer, first
    on the main table. Type 5 is type 1 with the backdrop's sections on the main table.
  - The other types move models each battle frame, through transform entries (`D_800F8158`, 10 of them, each a
    rotation and a position under the camera). A change made after drawing shows the next frame.
    - Type 0, a train: the carriage jolts 40 up every 4 frames and 120 every 32 (`func_800BA24C`). Sections 8-10
      are drawn 8 times, on entries 2-9, each moving 1000 a frame in z and wrapping as a `s16` from its start
      back to it at +10000 (`func_800BA11C`; entries 8 and 9 start off that pattern, so 9 sweeps the whole range).
    - Type 2, an elevator shaft: its walls drop 100 a frame over 125 frames, drawn twice, 12500 apart; its two
      cars bob 25 a frame, 16 frames each way, in opposite phase (`func_800BA2BC`).
    - Type 3: three parts turn about y by 1, 2 and 4 a frame. Type 4: three rings, 2500 up in the backdrop, turn
      about z by 8, 6 and 4; its far layer is coarser (`>> 11`) for two sections.
  - A stage is unlit and nothing in one on disc 1 is semi-transparent.
- **Timing:** a battle frame is 4 video frames (`VSync(4)` in `func_800D8A88`, from `func_800B37EC`), 15 a
  second. Each actor steps its animation one frame per battle frame, looping cleanly.
- **The camera scripts** (`func_800BC81C`, each battle frame): three channels, each a script for the target
  (`func_800BEA38`), then the eye (`func_800BCB1C`), then up to 16 tasks (the eases they start), then the camera
  takes a channel's eye and target (`func_800BC8B0`).
  - At the start a setup's `camera_script` picks `BATTLE.X`'s own pair of scripts (`D_800E9E5C`, `D_800E9FF8`,
    103 of each). They set a point (`F9`, `FA`), choose a setup camera (`DD`), take the current camera (`D9`), ease
    to the setup camera (`E2` n) or to a point (`E6` n), wait (`F5` n, then `F4`), and end (`FF`, held). Most ease
    from far off to camera 0 over 60 frames.
  - An ease (`func_800C0630`) runs first in the frame that starts it: f from 0 to n - 2, a + ((rcos(f·2048 / n +
    2048) + 4096)(b - a) / 8192) (C division), then the exact point. libgte's `rcos` is a quarter-wave table,
    round(4096 sin(i π / 2048)) for i 0-1024.
  - After that the camera holds; an attack starts one of the `CAMDAT*.LZS` scripts (below, the attack cameras).
- **The camera matrix** (`func_800D85B0`, each battle frame), from the eye and target:
  - z = `VectorNormal(target - eye)` (its z nudged off 0), x = `VectorNormal(z x (0, -4096, 0))`, y =
    `VectorNormal(z x x)`, the products by the GTE's `op` (>> 12); the rows x, y, z; the translation
    -(R eye >> 12). View space: x right, y down, z ahead.
  - `VectorNormal` (libgte, `0x8003A0E8`): the squares' sum, its leading zeros rounded down to even, a 1/root
    from a 192-entry table that is floor(32768 / sqrt(64 + i)), then `gpf` and a shift. Its vectors come out a
    little long.
  - H is 512 and the centre (160, 120) (`func_800B30E4`, `func_800D91DC`). The draw area is rows 8-165, so the
    menu, drawn over the rest, isn't on top of anything 3D.
  - The stage and actors are drawn with the previous frame's camera (`func_800B9568` runs before the camera
    update).
- **The action scripts** (`func_800C7C4C`, once a battle frame for each actor; `tools/battle.py` plays them):
  - Each model's section 1 holds them: pointers from `+0x68` up to the first script (74 for the party, 32 for an
    enemy). Numbers `0x2E`-`0x3B` (but `0x33`) run `BATTLE.X`'s own instead (`D_800EEAA0`-`D_800EEB1C`, by the jump
    table `jtbl_800A0888`). An enemy's attack n is its record's `animation_n` script with camera `camera_n`
    (0xFFFF: the attack record's own, `+8`).
  - The actor record (`D_801518E4`, `0xB9C` apiece): script number `+0x02`, animation `+0x0E`, size `+0x12`
    (section 1 `+4`), its battle facing `+0x18`, "animation ended" `+0x3B`, pc `+0x3C`, a wait `+0x3D`, facing
    `+0x162`, place `+0x168`. Its `0x74`-byte record at `D_80151200`: frames since the animation began `+0x36`, the
    angle FC turned its attacker by `+0x2C`. Home places `D_80163C80`; the idle script `D_80163784`.
  - The script runs only once its animation has ended (`+0x3B`, which the step writes when the bitstream wraps):
    then opcodes run until one plays an animation (a byte below `0x8E`, or `BB C0 CD D2 D3 D9 EF`, which reach the
    default) or waits; then the animation steps a frame (`func_800CD400`). So an N-frame animation holds the script
    N frames and animations play back to back.
  - Movement: `FC` turns to the target (`ratan2` + `0x800`) and stores the angle on the target; `E5`, `F9`, `B4`,
    `95` turn back to the battle facing (by layout); `FA` goes home; `FD` x y z sets the place; `ED`/`E4` step 516
    along/against the facing, `B1`/`B0` 258; `9A`/`FB` d y place it d plus the target's size short of the target;
    `D1` d h n (also `99`, `E9`, `CC`, `D0`, `D5`, `CF`, with other curves) runs there over n frames, a task (10
    slots at `D_801620AC`) adding the distance over n (C division) each frame; `C8` dx dz n slides.
  - Flow: `F4` n `F3` waits n frames; `EE`/`FF` end (back to the idle script, the same frame); `FE C0` loops;
    `F1` parks; `CE`...`CD` runs only for the party, `B3`...`B2` unless a flag; `C1` jumps past the first `C9`; `9E`
    waits for the effect tasks; `A9` x n starts n 0-15 frames in, by a counter (`D_800FAFD8`, +3 a use); `A7`/`B6`
    switch animation as far in as the last had gone.
  - Effects and sounds (passed over by `tools/battle.py`): `F7`/`BE` n queue a hit on each target after n frames;
    `D8` delay id plays a sound; `96`, `DB`, `DD`, `DE`, `AD`, `F0`, `B5` particle emitters on bones; `AC` n loads
    an effect module to `0x801B0000` and `EC`/`EB` call into it; `A2` swaps the model; `A0`/`AF`/`AE` hang the
    target on a bone and back.
  - A hit: n + 1 frames after `F7 n`, the battle (outside the scripts) turns the target to the attacker's facing
    - `0x800` and restarts it on its hurt script (1 for the MP, 5 for Cloud in the first battle).
- **The attack cameras** (`STAGE1/CAMDAT0..2.LZS` by the setup's layout: 0, 1 and 8 CAMDAT0, 2 CAMDAT1, 3-7 CAMDAT2;
  read to `0x801A0000`, its pointers already absolute):
  - The file: 4 pointers (the eye table, the target table, and camera -3's), then the scripts. Camera n's
    variant c (0-2) is entry `n * 3 + c` of each table (394, 375, 374 cameras). An action's camera number comes
    from its attack (`D_801637A0`); -1 holds the camera, -2 is the opening.
  - `func_800BB684` starts it on every channel and shows variant `D_801516F4 & 3` (a frame counter; 3 keeps the
    last).
  - Besides the opening's opcodes, both scripts take an actor's camera bone (16 per model, section 1 `+0x12`) plus
    an offset: ease to it (eye `D8 E3 E4 E5`, target the same), follow it n frames (eye `E7 E9 EB`, target `E8 EA
    EC`), set it now and wait a frame (eye `EF F0 F7`, target `F0 F8 F9`); of the attacker (`D_801590CC`), the
    target (`D_80161EEC`; 0xF the middle of all hit), a hit actor by number or any actor. `E0 p n` eases to the
    target's camera point p (section 1 `+0x4C`), eye `F8` circles the target, `F2`/`F3` stand round an actor.
  - An eye's offset is scaled by the model's offset scales (section 1 `+0x44`, 256 being 1) and, after `DC`, turned
    with the actor (`func_800C0DD8`); a target's y is raised by the model's adjust (`+0x64`), but not a following
    one's. Eye `DA`/`DB` and target `DB`/`DC` switch eases between rcos's curve and a straight line, whose step is
    the distance by libgte's `SquareRoot0` (a table of floor(512 sqrt(64 + i))), plus 1, in 16ths, over n.
  - A bone's place is its matrix's translation (`+0x188 + bone x 0x34`) turned back by the camera it was posed
    under (`func_800D3994`): the actors are posed after the frame's moves, and the camera follows those poses.
  - When the action is over the camera halves its way back to the setup's camera each frame (`func_800C1104`, C
    division) and stops once every half is 0.
- **Places** (`func_800B4794`, `func_800B5138`):
  - Enemies stand at their formation places. One on the far side (z < 0, or z >= 0 in layouts 3, 5, 6 and 7) is
    turned half round, except in layout 1.
  - The party's places come from `BATTLE.X`'s tables by layout: 3 members `D_800E8E84`, 1 the same with x 0, 2
    `D_800E8F28`. The back row moves 0x204 back. Their facings come from `D_800E8F94` (0 in layout 2).
  - An actor's node is `RotMatrixYXZ` of its turn under the camera; its bones hang on it.
- **Shadows** (`func_800BA598`, after each actor): `BATTLE.X`'s own model at `D_800EA294`, a disc of radius 900
  at y 0 in 16 Gouraud triangles, grey 200 in the middle fading to 6 at the rim. It's scaled by the model's size
  across and along (its section 1 `+6`, `+8`) and put at the root's x and z plus an offset (`+0xA`; -1: at the
  actor's place), on the floor. It goes in a list of its own, drawn after the backdrop and before the main table,
  between two draw modes: blend 2 (subtract) before, 1 after (`func_800B8360`). So the floor's main sections and
  the actors draw over it.
- **Textures** (`func_800D2980`): a party member's TIM goes a texture page word of 0, 0x10 or 0x11 on by slot, an
  enemy model's of 0x12 + its place in the scene (0x13 + at location 0x4E). The polygons' page words get the same
  addition, and each actor's palettes sit 3 rows apiece from row 480 (`func_800C62F4`).
- **Draw order:** `rtpt` (dropped nearer than H/2), `nclip`, then slot `avsz >> 2` in a 4096-entry table (ZSF3
  0x155, ZSF4 0x100), the far layer `>> 10`.
- **Checked against the lab** (battle `0x12C`, frame 13770, the opening camera settled):
  - The camera rebuilt from the setup's camera 0 matches the game's matrix and translation exactly, as it does
    in mid fly-in and in Cloud's spell.
  - The stage's packets match the game's in position and colour.
  - Drawn by the port's GPU, 99.1% of the 3D area outside the actors is the same pixel for pixel. With each
    actor at its frame in RAM and the shadows, the whole area is 93.7%; the rest is a pixel or a level or two of
    colour from posing in floating point (most of it along Cloud's sword).
  - The opening camera played from the setup's script gives the game's eye and target exactly at every dumped
    frame (battle frames 5, 15, 50 and 68). Drawn at battle frames 38 and 48 it matches the lab frames 13660 and
    13700, mid fly-in, for 99.4% and 98.3% of the stage's pixels: lab frame 13508 + 4k shows battle frame k.
  - Not checked in the lab: the moving stages (types 0, 2, 3, 4).
- **Actions checked against the lab** (the first battle, dumped every battle frame, `--dump-every 4`):
  - Two of Cloud's Attacks, two of the MP's Machine Guns and Cloud's Ice, 360 battle frames: stepped one frame from
    each dump's state, `tools/battle.py` gives every actor's script, pc, animation and frame, wait, place and
    facing as the next dump, but where the battle itself starts a script (a command, a hit, the victory).
  - Played from an idle frame as the viewer plays them (the attacker's script started, the hit as above), Cloud's
    Attacks and the MP's attacks match every actor in every frame until the next command.
  - Ice's camera (camera 7, variant 1), played from its start with the game's own bones, gives the game's eye and
    target exactly in all 80 frames; with the viewer's floating-point poses it's within 23 units. The physical
    attacks here hold the camera (-1).
  - Drawn by the port's GPU, Cloud's Attack matches the lab's frames on 89-96% of the 3D area; the rest is the
    sword trails and dust (effects aren't played) and posing in floating point.

**`MENU/SHOPMENU.MNU`, the shops** (`tools/shop.py` dumps them to a JSON file and builds them back): the module runs
from `0x801D0000` as the file is, the same on all three discs. A field script opens it with `menu 08 #N`, shop N.
- `0x42C8`: 9 shop titles, 20 bytes each ("Item Shop", "Weapon Shop", ...); `0x437C`: two sets of 10 clerk lines, 46
  bytes each (the second in dialect; no shop uses it). Text ends `FF` unless it fills its slot, then zeros.
- `0x4714`: 80 shops of 84 bytes (`func_801D0FE0` picks one): a title, a set of lines, a `u16` count, then 10
  entries of a `u16` kind (0 an inventory number, 1 a materia; `func_801D11A4`), 2 zero bytes and a `u32` id.
- `0x6854`: a `u32` price per inventory number (0–319), and from `+0x600` per materia (`func_801D10B0`); the rest is
  `FF`. Of the 165 inventory numbers no shop sells, 89 cost 2 and 23 cost 0.
- Checked in the lab: shop 1 retitled "Lab Shop", stocked with a Potion, an Elixir and materia 49, the Potion priced
  7 and the Elixir 1234, opened from `MD1STIN` after the opening movie; its buy list showed all of it, Fire at its
  own 600.

**The world map's event scripts** (`tools/world.py` dumps them to JSON files and builds them back):
- **Where:** section 4 of each `WORLD/WM*.TXZ` (LZS; a `u32` count of 12, then 12 offsets counted from byte 4).
  `func_800A0D2C` copies its `0x7000` bytes to `D_800D05EC`. `WM0`–`WMA` and their `S` versions carry one set, `WMB`
  and `WMC` one each, the same on all three discs. The loose `WM0`–`WM3.EV` match none of them (older copies; nothing
  reads them).
- **Table:** 256 entries of a `u16` key and a word number from the code at `+0x400`. `func_800AB6E4` searches
  entries 1–255 by key (sorted; unused `FFFF 0000`; entry 0 `0 0`), and several keys can share a script. A key below
  `0x4000` is system script N (0 at load; 2 whenever the system thread's script ends, so it runs over and over);
  `0x4000 | entity << 8 | fn` an entity's function; `0x8000 | (x + 36z) << 4 | (t - 3)` a map block's (`func_800ABA78`).
- **The machine** (`func_800AD63C`): a stack of 4 values, `u16` words.
  - Below `0x100` an operator (`func_800ABFC0`): `neg`, `not`, `mul`, `add`, `sub`, `shl`, `shr`, the signed
    comparisons, `eq`, `ne`, `and`, `or`, logical `land`/`lor`, `set` (assign); 18–1B take an entity and give
    something measured to it.
  - `0x100` empties the stack; `0x101`–`0x1FF` push the next word, the opcode its type (`func_800ABB24`): kind `110`
    the value itself, `114` a bit, `118` a byte, `11C` an `s16` at that address; the low two bits the area: 0 the save
    data at `0x8009D288` (all five banks, so the field's variable names apply), 1–2 a 64-byte scratch area, 3 one of 21
    special values (the entity's block and position, direction, its id, ...).
  - `0x200` goto, `0x201` jump if zero (word numbers from the code's start), `0x203` return (to a caller, or the end),
    `0x204 + N` run function N of the entity on the stack.
  - `0x300`–`0x355` commands, their arguments popped (none inline), most on the current entity (`D_8010AD3C`); a
    "wait" one takes its own word back off the script position and yields, so it runs again next frame. Named by
    what their handlers do (`tools/world.py`'s `COMMANDS`): `spawn_entity` N (the entity with model N, made if there
    isn't one, its function 0 started), `select_entity`, `set_player`; speed, direction, `set_block` /
    `set_block_pos` (the high and low 13 bits of x and z), `set_wait` N then `wait_move` (moves N frames);
    `play_animation`; `battle` N (`func_800A3F4C`); `enter_field` (a, b): entry `(a - 1) * 2 + (b & 1)` of the 12-byte
    table at `0x800BF5F0` (`+6` the field ID, `+0`/`+2`/`+4` where to stand); `set_window` (x, y, w, h of window 0),
    `message` N, `ask` (message, first and last choice line), `close_window`, `wait_window`, `wait_answer`;
    `play_sound` (sound command `0x30`), `play_music`, `music_volume` (`C0`), `screen_shake`, `fade_out` /
    `fade_in`; `set_encounters` (random battles roll only while it's set); the camera and view; 16 light points
    (`point_*`); effects. The 68 uses of the rest stay `op_3XX`.
  - Special values by name: the current entity's `block_x`/`block_z`, `x`/`z` in the block, `direction`,
    `entity_id`; `player_id`, `view_mode` (as `set_view` takes it), `exit_kind`/`exit_key` (the last field exit),
    `answer` (-1 while the window is open), `random` (the next byte of a table). Operators 18–1B measure from the
    current entity: `point_distance`, `entity_distance`, `saved_pos_distance`, `point_angle`.
  - Only pushes and jumps take a second word, so every script disassembles linearly; all three sets assemble back
    byte for byte.
- **Building:** a container is packed again only from the flag byte before its scripts (LZS only looks back), within
  3 bytes of the original's size; the containers sit back to back, with 47 to 1,967 bytes left in their last sectors.
  `WORLD.BIN` keeps a sector and size per container (`0x800C73E4`, the `S` ones `0x800C744C`), but the CD layer only
  takes the sector count from the size (`cd.c`), and `WORLD.BIN` gzips back 41 bytes bigger than it was, so it changes
  only if a count does.
- **Checked in the lab:** `push #324`, `battle` at the start of system script 2 opened battle `0x144` (Guard Scorpion)
  ten frames after the world map loaded; at the start of script 0 (still loading) the world map stopped on its whole
  map instead. A word added to script 0 moved every label after it and the world map ran as before. In script 2,
  `enter_field` (50, 0) left for field `0x46` (`FSHIP_23`), entry 98's ID; `set_window` (30, 160, 260, 41) then
  `message` 49 showed Cloud's "...First, I gotta do something about her." in a box there.

**`WORLD/WM*.MAP`, the world map's terrain** (`tools/world.py` reads it; the viewer's World tab draws it):
- **Which:** map kind k reads `WM{k}.MAP` (`WORLD.BIN`'s sector and size per kind at `D_800C74E4`): 0 the world, 2
  and 3 two maps that tile parts of it (`WM1.MAP` is 2 bytes). Kind 0 takes its textures from `WM0`–`WMA.TXZ`
  (`func_800B717C` picks one), kind 2 from `WMB.TXZ` and kind 3 from `WMC.TXZ` (`func_800A460C`: kind + 9); map 2
  uses exactly `WMB`'s 8 textures and map 3 `WMC`'s 4.
- **Blocks:** `0xB800` bytes (23 sectors), read one at a time as the party moves (`func_800A54F0`, `cd_read` into one
  of the slots at `D_800E5714`). The world is 9 by 7 blocks, row by row; a block is 4 by 4 meshes and a mesh 8192
  units square, so x runs to `0x48000` and z to `0x38000`, and both wrap.
  - World block b is file block b for map 0, except that `map_stage` (script command `0x349`, `D_800E5824`) swaps six
    for the six after the 63: `0x32` from stage 1, `0x29` and `0x2A` from 2, `0x3C` from 3, `0x2F` and `0x30` from 4
    (`0x3F`–`0x44`). Map 2 is 3 by 4 blocks repeated over the world; map 3 is 2 by 2, and only some world blocks
    reach it (the rest work out to numbers outside the file).
- **A block:** 16 `u32` offsets (its meshes row by row), each to a `u32` length and the mesh, LZS. `func_800A5D00`
  unpacks one into its own `0x1200`-byte buffer at `D_800E5A38` and keeps its triangles, vertices and normals.
- **A mesh:** `u16` triangle and vertex counts, then
  - per triangle 12 bytes: 3 corners (vertex numbers), a byte the draw doesn't read, 3 UVs (u, v), and a `u16` whose
    low 9 bits pick the texture (the high 7 bits the draw doesn't read either);
  - the vertices (`s16` x, y, z, pad; x and z 0-8192 across the mesh), then as many normals (the same layout).
  - A world mesh is typically 9 by 9 vertices and 128 triangles; map 2's open sea floor is often a single quad.
- **Textures:** section 2 of the container (`func_800A0D2C`): 512 words for `D_800BD148`, a texture each (a palette in
  the low half, a page in the high: the triangle's UVs are already in that page), then images to `LoadImage` (`u32`
  length, x, y, w, h, the pixels; a 0 length ends them). `WM0.TXZ`: palettes at (0, 480) 256×22, pages at (384, 0)
  640×256 and (384, 256) 384×256.
- **Drawing** (`func_800A71E8`, each frame): the 5 by 5 meshes round the camera's focus (`D_80109D44`; the window by
  `func_800A63FC`, culled by `func_800A6168`), each in three hand-written GTE loops:
  - The translation (`func_800A1FAC`): the camera's rotation `D_800E5698` and, through `RotTrans`, the mesh's corner
    from the focus (x and z, `y` = -the focus's height `D_80116508`) turned and added to `D_800E56B8`'s translation.
  - The curve (`func_800BCA78`): each vertex is copied to the scratchpad and projected (`rtps`); one past depth
    4·`D_800E5678` (SZ >> 2 over it; 5000 in the lab) drops by ((that >> 6) + (|SX - 160| >> 5))², so the far world
    bends away, more to the sides.
  - Projection (`func_800BCB2C`): `rtpt` over the scratchpad, each vertex's SXY and SZ, with flags for x < 320 and
    y < 240 (unsigned).
  - The triangles (`func_800BCBE8`): one goes when some corner is on screen across and some down and it faces the
    camera (`nclip` ≥ 0). Its colours are `nct` of its corners' normals (light matrix `D_800C6808`, map 2
    `D_800C6828`; back colour 64, map 2 (32, 32, 48)); a `POLY_GT3` with the texture word's palette and page, linked
    at its farthest corner's SZ >> 4. Map 2 has fog: past SZ `0x3E80` the palette moves down a row per 8 of depth
    (`D_800BD138`; at most 15).
  - The frame is linked into a reverse ordering table and `DrawOTag` starts at entry 2500 (`func_800A141C`), so
    anything linked deeper is never drawn: the world's far limit.
  - The world draws a 320×224 picture at row 8 of VRAM (draw offset (0, 8)), dithered, on black.
- **The camera** (`func_800A1DF0`, each frame, given the yaw):
  - A view mode (L2/R2, `D_800E5648`) sets a tilt and a distance (`D_800C6628`, `D_800C6638`: 1500 and 10000 in view
    0, 1800 and 7000, 7000, 4000 in views 1-3) that `func_800A3304` eases toward (`D_800E5610`, `D_800E5614`, 7/8
    or 3/4 of the old each frame), and a screen centre row (`func_800A2088`: 120 in view 0, else 160) eased into
    `D_800E55F0`. The loader starts at 1500, 10000 and 160 (`func_800A0D2C`).
  - The eye: `RotMatrix(tilt, 0, 0)` turns (0, 0, distance); its y · 16 less the focus's height, · 256, over its
    z · 16 (C division, held to 0-511) indexes the arctangent table `D_800BE1E8`; plus `0x800` that's the pitch
    (`D_800E56D8`).
  - The rotation `D_800E5698` = `RotMatrix(0, 0, roll)` · `RotMatrix(pitch, yaw, 0)` (`MulMatrix0`; roll
    `D_800E5654`), the translation `D_800E56B8` (0, 0, distance).
  - libgte's `RotMatrix` reads its own sine table (`D_8004BC98` in the exe, 4096 sine and cosine pairs; not
    round(4096 sin) in 1944 entries) and multiplies in fixed steps; `tools/world.py` (`rot_matrix`, `camera`) does
    the same and gets the dump's matrix exactly.
- **Light zones** (`func_800AF3A4`, before the draw): 16 at `D_8010B178`, `0x24` bytes: x, y, z; the light's red,
  green, blue at `+0x10` (`point_rgb1`), on at `+0x13`; the sky's top colour at `+0x14` (`point_rgb2`) and bottom
  at `+0x18` (`point_rgb3`); outer and inner reach in 1024s at `+0x1C`, `+0x20` (`point_range` outer, inner).
  - The script commands pop their arguments in push order: `point_select` (zone, on; -1 keeps on), `point_block` and
    `point_pos` (x and z, their high and low 13 bits), the colours (red, green, blue), `point_range`.
  - System script 0 sets them with constants: `WM0`'s turns on zones 0-2 (Midgar's at (185605, 120033): light (60,
    90, 100), sky (0, 0, 65) over (145, 255, 190)), `WMB`'s one white zone; the others stay off, points the scripts
    measure distances to (`point_distance`).
  - Within a zone's inner reach (x + y + z distances from the player, round the world) light 0's colour (the colour
    matrix's first column) is 7× the zone's and the sky the zone's colours; they fade back to the base ones
    (`D_800C6848`'s 1774, `D_800C6768` blue (0, 0, 255) over `D_800C676C` (250, 255, 255)) at the outer reach past
    that. The last zone in reach wins.
- **The sky** (not on map 2; `D_800E55F4`), linked at entry 2500 so it's drawn first:
  - The horizon (`func_800AE638`): a point 0x4000 ahead of the focus at the camera's height, turned by -yaw, lands
    on a screen row; that + 26 is `D_8010B080`.
  - The gradient: a `POLY_G4` (`D_800C6770`, one per buffer) from 24 rows above the screen to the horizon, top
    colour over bottom, turned by the roll about the screen's centre column; drawn without dither (a draw mode packet
    before it, dither back on after).
  - The clouds (`func_800AEA48`): grey `0x40` semi-transparent quads of a texture strip (palette `0x7B0F`, page
    `0x39`; `0xA9` on GPU types 1 and 2), 64 rows high, (curve >> 8) + 44 rows above the horizon, the whole screen
    wide: two quads, or three once the scroll `D_8010AE44` reaches `0xC0`. Turning the camera scrolls them: the yaw
    turned since `D_8010AE48`, times the curve over 10000, off the scroll (wrapping at 256).
  - A fourth quad (`D_800C67B8`, the sun or moon?) is drawn when `D_8010B174` is set; it wasn't in either lab frame.
- **The party leader** (model ids 0-2: `WM0`, `WM1`, `WM2.BSZ`, Cloud, Tifa and Cid; `func_800B69A4` reads one to
  `0x8014A600`, LZS):
  - The file: 2 sections; section 0 the models (a count, a pointer to the first header, a size; every pointer already
    a RAM address for that load address), section 1 the images (a count at 4, then w, h, x, y and an offset per
    image; `func_800C0808` loads them). The other models (ids 3 up) are the container's section 0, at `0x8013A800`
    (`func_800B6EFC`; not drawn in the lab frames, not checked).
  - A model's 0x24-byte header is its instance: on, mode, bone, part and animation counts, root angles, a scale at
    `0x16` (`0x200`), the parts' and animations' offsets from the base pointer at `0x1C`, the bone matrices at `0x20`.
    At the base, 4 bytes a bone (s16 length, s8 parent, a byte: copy its matrix out).
  - An animation, 16 bytes: frames, counts of s16 tracks, s16 statics and byte tracks, their offsets, and a pointer
    to its data: a word, then 8 bytes a bone (flags: bits 0-2 an angle from a track, 4-6 a translation; three angles
    in 256ths or track numbers; three translation numbers, `0xFF` none). Cloud has 22 bones, animation 0 (idle, 1
    frame) and 1 (walking, 15).
  - A part, 0x20 bytes: on, bone, vertex count; counts of `GT4`, `GT3`, `FT4`, `FT3`, `F3`, `F4`, `G3`, `G4`; the
    offsets of its polygons, UVs, texture words and texture bytes; the bytes of packets per buffer; its data: a word,
    the vertices, the polygons (corner numbers, colour words: RGB and a normal number on top, a UV numbers word).
  - At load (`func_800BFBF0`, `func_800C1D58`): lengths, translations and vertices · scale >> 12; each part's packets
    built twice (double-buffered, per model, not in the frame's packet buffers); then lit once (`func_800B6724`):
    posed at frame 0 of animation 0 under no camera, each colour through `NCCS` with its normal (`D_800C7938`) and
    three grey lights (`0x6E`, `0xAA`, `0x3C`) turned with the bone, back colour `0x40`; the normal number stays.
  - Posing (`func_800C1490`): angles from a 256-entry table (`D_800C7538`, truncated 4096 sine and cosine); each bone
    is its parent's matrix times Ry · Rx · Rz of its angles, moved along its z by its length plus its translations.
    The root (the player's: x `0xC0`, y its facing >> 4) is turned a quarter about x under the camera, at its place
    from the focus / 4 plus the camera's translation / 4 (`func_800A36AC`, `func_800AAB18`). The frame steps once a
    tick while walking (`+0x53`).
  - Drawing (`func_800C0B48`): per part under its bone's matrix, the vertices projected, a polygon facing the camera
    (`nclip` > 0) linked at its average depth (`avsz3`; a quad's four over 16), no clipping. Textured ones take a
    texture byte (word 0-15, `0x10` semi-transparent, `0x60` blend): the word's palette and page.
- **Shadows** (`func_800B5C7C`, `func_800B59F4`): a subtracting textured quad (palette `0x7CC4`, page `0x59`) in the
  entity, grey `0x20` and 100 by 100 (half) while it stands on the ground, smaller and paler as it rises, scaled per
  model (a table); at the ground under it, turned by its facing + `0x800` under the camera; linked at its nearest
  corner's depth >> 4. Cloud's texture rectangle is (24, 56) 15 by 15.
- **Checked against the lab:**
  - Near Midgar, frame 3300 of a new game warped to the world map, and the same at frame 3480 after R2 (view 1,
    mid-turn), with the sky in sight: the 16 meshes in RAM are the decoded ones byte for byte, the texture table is
    `D_800BD148`'s and the images (the container's and `WM0.BSZ`'s) are in VRAM.
  - From each dump's camera state, `tools/world.py`'s sky, terrain (213 and 471 triangles), Cloud (built from
    `WM0.BSZ`: scaled, lit, posed) and his shadow are the game's packets word for word (but for bits the GPU doesn't
    read), and drawn by the port's GPU every pixel of both frames is the game's but the minimap (72 by 56 at
    (228, 156)). The camera and zones rebuilt from their tables give the dumps' matrices and colours exactly, and the
    viewer's frame for that spot, from the disc alone, is the lab's pixel for pixel but the minimap.
  - The game's packet buffers hold 5 terrain triangles more than it links: they're never drawn.
  - Not checked: maps 2 and 3, models other than the party leader, the minimap, the sun.

## 3. Code: what is recovered and what isn't

The main exe `SCUS_941.63` is split with splat and `make` rebuilds it byte-identical (SHA1-checked) from asm plus the
recovered C. Details in §7.

| Part | Range | Status |
|---|---|---|
| Boot code (`main.c`, `transition.c`, `loader.c`) | `0x8001117C`–`0x80014A00` | all 47 functions in C |
| `bitops.c` | `0x80014A00`–`0x80014A84` | 3 functions in C |
| AKAO sound driver (`sound_init.c`, `sound.c`) | `0x800293D0`–`0x80033A90` | 231 of 232 functions in C |
| CD layer (`cd.c`) | `0x80033A90`–`0x80034CF0` | all 38 functions in C |
| Movie player (`movie.c`) | `0x80034F3C`–`0x8003623C` | all 14 functions in C |
| PsyQ library block | `0x80036E84`–`0x80048D18` | 267 of 454 functions named (§8) |

Not recovered yet: the rest of the exe's game code (still asm) and every overlay: battle, field, world, menus,
mini-games, the 289 `MAGIC/*.BIN` effects, `DSCHANGE.X`, `ENDING.X`, `BATTLE/TITLE.BIN` (probably the
title screen).

### PC vs PS1

- The PC port (Eidos 1998, `FF7.EXE` 5.8 MB, x86) is a separate build from an earlier beta, so expect differences.
  Its code is the base for FFNx and ergonomy_joe's PC decomps (world map, coaster, chocobo race).
- **PC compiles all 13 menu modules into `FF7.EXE`**; PS1 loads each from a `.MNU` overlay (per Gears). The PC code
  is a logic reference for the PS1 menu overlays.
- PC music is MIDI (`DATA/MIDI/MIDI.LGP`, plus `AWE`, `XG`, `YGM` variants) through a separate sound library
  (ergonomy_joe ships it only as the binary `FF7SND.lib`). PC never had the AKAO sequencer, so PC-based reversing
  never covered the PS1 music/SFX engine.
- PC data lives in `.LGP` archives, PS1 data in loose files. Many formats inside are shared (field scripts,
  `SCENE.BIN`, `KERNEL.BIN` sections), so PC tools (Makou Reactor, ff7tk) document formats that also apply here.

## 4. Sound: the AKAO engine

The PS1 sound driver is Square's **AKAO** sequencer, linked into the main exe. Sequences are byte code that picks
instruments, sets pitch, volume, pan, ADSR and reverb, and plays notes on the 24 SPU voices. It reaches the SPU
**only through libspu**: no game code writes the SPU registers (`0x1F801C00`–`0x1F801DFF`) itself. Everything here
comes from our own disassembly of `SCUS_941.63`.

### Driver map

**Range: `0x800293D0`–`0x80033A90` (~42 KB).** Every game function that calls a named libspu function (§8) sits in
`0x8002988C`–`0x8002F848`. The call graph bounds the rest: graphics code (`AddPrim`, `GetClut`) ends at `0x800293D0`
and CD code (`CdInit`, `CdRead`, `CdReadSync`) starts at `0x80033A90`. Most of the driver's ~330 functions have no
direct caller; they're reached through pointer tables.

| Address | Our name | What it is (read from the code) |
|---|---|---|
| `0x8002DA7C` | (unnamed) | The only driver function called from all over the game (10 outside callers); a public entry point |
| `0x8002DF88` | `snd_run_command` | Runs one sound command immediately. Shorthands: `24`–`27` play 1–4 effects with consecutive ids (as `20`–`23`); `98`/`99` run `9A`+`9C` / `9B`+`9D`; `D8`–`DA` run `D0`–`D2` with `D4`–`D6`. The song commands do nothing here. Then `snd_command_handlers[cmd](entry)` |
| `0x8002E1A8` | `snd_flush_queue` | Runs every queued command: entries of 0x24 bytes at `snd_queue` (`0x80081DC8`), count at `snd_queue_count` (`0x80063010`) |
| `0x8002E23C` | (unnamed) | Calls a run of 8 libspu setters; probably the per-voice SPU update |
| `0x80030E7C` | `seq_step_channel` | Sequence interpreter for one channel (below) |
| `0x80049548` | `snd_command_handlers` | 256 handlers indexed by command byte. Live: `10`, `14`, `15`, `18`, `19`, `20`–`23`, `28`–`2B`, `30`, `34`, `80`–`82`, `90`, `92`, `9A`–`9D`, `A0`–`BD`, `C0`–`C2`, `C8`–`CA`, `D0`–`D2`, `D4`–`D6`, `E0`, `E4`, `F0`–`F5`, `F8`–`FA`. Everything else is the no-op `0x8002CF98` |
| `0x80049AA8` | `seq_cmd_handlers` | 96 handlers for sequence commands `0xA0`–`0xFF`. The code indexes it with the raw byte from base `0x80049828` |
| `0x80049C28` | `seq_note_lengths` | 11 `u16`: `0xC0C0, 0x6060, 0x3030, 0x1818, 0x0C0C, 0x0606, 0x0303, 0x2020, 0x1010, 0x0808, 0x0404` |
| `0x80075F28` | `instrument_table` | 128 × 64-byte instruments (= `INSTR.DAT` 8,192 bytes). Field `+0` is the SPU sample address (`0x1010` for entry 0, matching `INSTR.ALL`'s header) |

The command table is presumably what the field opcodes `AKAO`/`AKAO2` feed; command `0x10` is the first live
handler.

### Sequence format

Each channel has a state struct whose `+0x00` is the read pointer. Each step of `seq_step_channel`:
1. Read byte `b` and advance the pointer.
2. If `b >= 0xA0`, call `seq_cmd_handlers[b - 0xA0](channel, …)`. Repeat until a note, or until `b == 0xA0`, which
   ends the channel.
3. Otherwise `b` is a **note**: `pitch = b / 11`, `length = seq_note_lengths[b % 11]` (the low byte is the tick count:
   192 = whole, 96, 48, 24, 12, 6, 3; triplets 32, 16, 8, 4). Pitch 0–11 is a semitone in the current octave;
   `0x84`–`0x8E` (pitch 12) is a **tie**; `0x8F`–`0x9F` (pitch 13+) is a **rest**.
4. With channel flag `+0x38` bit 3 set, the semitone instead indexes a 5-byte-per-key map at `channel+0x14` and picks
   a per-key instrument from `instrument_table`: drum-kit mode *(name is our guess)*.

Argument lengths come from how far each handler moves the read pointer, confirmed by `tools/akao.py check`. **All 98
distinct songs (1,340 channels) and all 1,423 sound-effect streams on the three discs parse cleanly** with this
table, no unused command is hit, and about 95% of streams end exactly where the next one starts. Channels may fall
through into the next channel's bytes (shared tails), and some songs leave trailing bytes after the last channel.

| Args | Commands |
|---|---|
| 0 | `A0` end · `A6` `A7` octave ± · `B3` `B6` `BA` `BE` `C2`–`C7` · `C8` loop point · `CA` loop back (stops linear parsing) · `CB`–`CD` `D0` `D1` `D4`–`D7` `DB` `ED` `F3` `F5` `F9` |
| 1 | `A1`–`A3` `A5` octave · `A8` volume · `AA` `AC`–`AF` `B1` `B2` `B5` `B7` `B9` `BB` `BD` `BF` `C0` `C1` · `C9` repeat count · `CE` `CF` `D2` `D3` `D8`–`DA` `DC` `F2` `F6` `F8` |
| 2 | `A4` `A9` `AB` `B0` `BC` `DD`–`DF` `E8` `EA` `EC` · `EE` jump (stops linear parsing) · `F4` `F7` `FD` `FE` |
| 3 | `B4` `B8` `E9` `EB` `EF` `F0` `F1` |
| — | `E0`–`E7` `FA`–`FC` `FF`: unused; they go to `seq_cmd_unknown`, which just calls the `A0` handler |

- **Lengths rest on handler code alone** for `B7`, `BE`, `BF`, `CE`, `CF`, `DF`, `E9`, `EB`, `EF`, `F1` and `F3`,
  which never occur on the discs. Every other length is confirmed by real data.
- **Never used in songs:** `AC B0 B3 B7 BB BE BF C4 C5 CB CE CF D2–D7 D9 DA DB DF E9 EB EF F1 F3` and the unused
  `E0–E7 FA–FC FF`. `DA` (glide) appears only in sound effects. The player handles every command a song uses; `FE`
  has no audio effect.
- **Read in detail:** `A0`, `A1`, `A2`, `A5`–`A7`, `C8`–`CA`, `DC`, `E8`, `EE`, `F0` (also confirmed by the MIDI loop
  check); `A3`, `A4`, `A8`, `A9`, `AA`, `AB`, `C0`, `C1`, `D8`, `D9`, `DA`, `EC`, `ED` (volume, drums, pitch);
  `B4`–`B6`, `B8`–`BA`, `BC`, `BD`, `DD`, `DE`, `FD`, `FE` (LFOs and bars); `AD`–`AF`, `B1`, `B2`, `CC`, `CD`, `D0`,
  `D1` (articulation and envelopes). The rest are not studied yet.

**Basic commands**
- **`A5 n` / `A6` / `A7`:** set the octave (`+0x66`) / raise it by one / lower it by one, masked to 0–15.
- **`A3 n`, `A8 n`:** store a byte into a channel field and flag it for an SPU update; `A8` stores it sign-extended
  `<< 23` into `+0x44` (volume; see below).
- **`FD t n`: time signature** (`0x80033548`). Writes ticks per beat (`+0x5A`) and beats per bar (`+0x56`) into the
  song state and resets the beat and tick counters, so the bar starts where it stands. `30 04` (4/4) occurs in 132
  places; also 3/4, 2/4, 5/4–8/4 and 3/8–10/8 (`18 n`). Song 76 alternates three bars of 6/8 with one of 5/8; song
  71 runs through 6/4, 7/4 and 8/4; songs 1, 66 and 89 have none.
  - Most songs open with a 1–4 tick rest, then `FD` + `FE 01 00`, so their bar lines sit a few ticks after tick 0.
    Some open with a pickup bar (song 47: `FD 30 01`).
- **`FE lo hi`: bar number** (`0x80033588`), a `u16` at `+0x5E`; a counter for the game, usually `01 00` at bar 1.

**Articulation**
- **A tie after a rest** keys nothing on: neither the tie path nor the rest path sets the key-on mask, so `n r t` is
  a note and a longer rest (song 12's organ chords, channels 0–2, `n0/8 r/7 t/7`, stop at the rest). A pending `A4`
  bend is still spent on that tie, on the silent voice.
- **`CC` slur / `D0` full length** (`0x80032D44` / `0x80032D58`) set the note mode `+0x6E` to 1 / 4. At each note or
  tie, bit 1 takes bit 0, and a note with bit 1 set skips key-on: it takes over the sounding voice with a new pitch
  and no new attack, so `CC n1 n2` slurs n1 into n2.
  - Only the key-on mask is skipped (the slur branch `bnez .L80031454`). The new pitch, the restart of vibrato,
    tremolo and pan LFO (current settings, delay included) and the zeroed vibrato and bend offsets all still happen.
    So a long slurred line under a delayed vibrato restarts the delay on every note, and a depth change right before
    a slur takes effect (effect 274 raises its vibrato from `0x83` to `0xA3` that way). 53 songs and 176 effects
    slur with an LFO on; song 97 does it most (2,887 notes). The player does the same.
  - `CD` and `D1` (`0x80032D50`, `0x80032D64`) are empty. The mode ends in `seq_look_ahead` (`0x800318BC`), which runs
    before each note and scans to the next note, following `C9`/`CA`/`EE`/`F0` like the sequencer: `CB`, `CD`, `D1`
    or `DB` clear bits 0 and 2 and it keeps scanning; a rest, `A0` or an unused command clears them and stops. So
    `CC n1 n2 CD n3` holds n1 into n2, keys n2 off early, and starts n3 fresh.
  - `CC`/`CD` appear 1,601 times in 56 songs, and 12,039 of 156,161 notes are slurred. `D0`/`D1` appear 97 times in
    14 songs.
- **`AD`, `AE`, `AF`, `B1`, `B2`: the channel's envelope** (`0x80032E08`…`0x80032F98`): attack rate, decay rate,
  sustain level, sustain rate and release rate (`+0xFA`…`+0x102`), with a voice-update flag so they reach a sounding
  note. `load_channel_instrument` resets all five, and the three modes, from the instrument on every program change
  (the drum-kit path per note). `AD` is in 47 songs, `B1` in 35, `AF` in 7, `AE` in 5 and `B2` in 1: 476 overrides.
  The mode commands `B7`, `BB` and `BF` never occur.
  - The SPU waits a number of samples per envelope step that grows with the rate, up to 2^20 samples (about 24 s) at
    sustain rate `0x7F`, the "hold" most instruments use. The player keeps the wait already spent as a fraction of a
    step, which a new rate continues, so a mid-note change (effect 448's `B1 3E`) or a slow release after a held
    sustain plays out in full (release rate 12: about 180 ms). 50 songs and 193 effects change an envelope rate on a
    cell without a note.

**Extra voices and FM**
- **`F4 a b` / `F5`: layer** (`0x80031D6C` / `0x80031E98`). `F4` reserves a free SPU voice for the channel (`+0x24`,
  flag `0x100`), loads instrument `a` for the channel and `b` into that voice's channel slot, and every key-on keys on
  both. The envelope commands write both, and `seq_layer_voice_update` (`0x8002F738`) copies the main voice's pitch
  register (`+0xF8`) to the layer, vibrato included. `F5` frees the voice.
- **`F6 n` / `F7 t n`: layer balance** (`0x80031EEC` / `0x80031F30`): `+0xC6 = n << 8`. The main voice plays at
  `(127 − n) / 256` of its volume and the layer at `n / 256`; `F7` slides the balance to `n` over `t` ticks (0 = 256).
  Only song 10 uses it, channels 1–5: instruments 84 and 83, crossfaded.
- **`F8 n`: overlap** (`0x80033128`). Sets the release rate to `n` and, on first use, reserves a second voice for the
  channel (`+0x28`, flag `0x200`). Every key-on then toggles flag `0x400` in the voice update (`0x8002FA74`), so notes
  take turns between the two voices and each rings on in its release while the next starts, like a plucked string.
  With `0x200` set, a program change keeps the release rate (the update mask `0x1BB80` leaves it out), and so does a
  drum-kit note.
  - `F9` (`0x800331CC`) frees the second voice and takes the release rate from the instrument again.
  - 48 uses in 18 songs, almost all `F8 0C` (about a 2-second ring), e.g. song 96's plucked part (channels 11–13,
    instrument 21 with `AD 20` and `B1 33`). `F9` is used once, in song 2.
- **`C6` / `C7`: FM on / off** (`0x80032BB4` / `0x80032C20`) set and clear the channel's bit in the song's
  pitch-modulation mask (`+0x34`), which `snd_apply_fm_voices` hands to the SPU. Used in songs 53, 66 and 89. The
  SPU's rule (psx-spx): step = pitch register × (previous voice's output after its envelope, + 0x8000) >> 15, then
  anything over `0x3FFF` becomes `0x4000`. The cap comes after the modulation, so effect 270's FM voice (register
  `0x2B2E`) never steps past `0x4000`.

**Voice updates**
- **Pitch register: masked, not clamped.** Every voice update writes the pitch as `& 0x3FFF` (`0x8002F1D0`,
  `0x8002F23C` for songs; `0x8002F6AC`, `0x8002F728` for effects), and the setter at `0x800395C8` copies that straight
  into the voice, so a pitch past `0x3FFF` wraps round to a low one. The player does the same; the XM export keeps the
  unwrapped pitch.
  - Songs 2, 9, 53 and 72 and 87 effects have such notes. Song 2's channel 10 plays instrument 30, whose table starts
    at `0x6590` for octave 6, so its key-85 notes (`0x6BA0`) play at `0x2BA0`.
- **Key-off freezes the voice.** A key-off clears the voice's bit in the update mask (songs: `seq_song + 0xC`, set at
  key-on in `seq_step_channel`, cleared in `seq_tick`; effects: `0x80099FD4`), and `seq_update_voices` runs the voice
  update (`seq_channel_lfo` / `seq_effect_lfo`) and `snd_update_voice` only for voices in that mask. Through the
  release the SPU keeps the pitch, volume and pan it was last given: bends, volume slides and LFOs stop where the
  key-off found them. FM still applies, since the chip does that itself. The player does the same.
- **Song start** (`snd_start_song`): tempo `0xFFFF` (`D_8009A11E`) and accumulator 1 (`D_8009A124`), so before the
  first `E8` a song runs at about a tick per timer interrupt. Song 50 sets its tempo at tick 4, song 85 at tick 72.

**Reverb.** Most instruments have release rate 5, linear, about 1.5 ms, so notes mostly die at key-off; the tail you
hear in the game is SPU reverb.
- **Mode:** the song header's `u16` at `+8`. The song-start code (`snd_submit_request`) passes it to
  `snd_set_reverb_mode` (`0x80029AF0`), which acts only when it changes: it turns reverb off, calls
  `SpuSetReverbModeParam` with mask 1 and mode `| 0x100` (clear the work area), and turns it back on. 45 songs use 1
  (Room), 40 use 3 (Studio B, medium), and 13 use 4 (Studio C, large).
- **Presets** (libspu's tables in the exe): `spu_reverb_work_starts` (`0x8004AB5C`) is ten `u32`s, the work area
  start per mode in 8-byte units (Room `0xFB28`: 4,960 samples; Studio B `0xF6F8`: 9,248; Studio C `0xF204`: 14,320).
  `spu_reverb_presets` (`0x8004ABAC`) is ten 0x44-byte entries: a `u32` mask, then the 32 registers
  `0x1F801DC0`–`0x1F801DFE`. All three presets FF7 uses take `vLIN`/`vRIN` = `0x8000` (−1).
- **Depth:** `EA lo hi` (`seq_cmd_reverb_depth`, `0x80031BA0`) sets it to `(hi << 8 | lo) << 16`. It's zeroed at song
  start and used in 93 songs, mostly `EA 00 28` (`0x2800`). `seq_update_voices` takes the top half (`0x8009A146`),
  scales it by the game's sound setting `0x80062FB8` (0 at boot: unchanged) and pans it by `0x80062F70` (`0x40` at
  boot: both sides equal). `SpuSetReverbDepth` writes it as the reverb output volume (`vLOUT`/`vROUT`), so `0x2800`
  = 0.31.
- **Per channel:** `C2` turns a channel's reverb send on and `C3` off (1,876 and 59 uses). Song 42 turns it on for
  all 16 channels.
- **In the player** (`Reverb` in `tools/akao_synth.py`): psx-spx's formula on a copy of the work area at 22.05 kHz,
  resampled to and from the output rate; the sends are the voices' panned outputs, as on the SPU.
  - The SPU halves the rate on the way in and doubles it on the way out through a 39-tap half-band low-pass (checked
    against Mednafen). Ours is a windowed sinc (Kaiser, β 13, cut off at 11.025 kHz), within 0.1 dB of the hardware
    up to 12 kHz, every coefficient within 13/32768. Bright, aliasing voices such as effect 270's FM voice come out
    clean through the reverb, as in the game.
  - It works a stretch of up to 512 samples at a time with all reads before the writes, which matches the
    sample-by-sample formula while the stretch is shorter than the shortest write-to-read delay. Each filter's
    feedback from its own last output is computed as a filter. A test checks it against a plain per-sample version
    to within 1e-9. Cost: about 2% CPU for FF7's presets; Echo and Delay (4-sample delays) run slowly, but no song
    uses them.
  - Over 20 s, reverb adds 4–17% RMS: song 42 (Room) 0.086 → 0.089, song 96 (Studio B) 0.047 → 0.051, song 82
    (Studio C) 0.067 → 0.078.
  - Known gap: Room's two "diff" filters both write offset 0, so the SPU's left filter reads the right one's last
    output; ours uses its own. Nothing reaches the output from there (`vCOMB3`/`vCOMB4` are 0).
- **Aliasing in the voices themselves is real.** Above 44.1 kHz the SPU just skips samples, and the 4-point Gaussian
  is the only filtering (psx-spx); neither the driver nor Mednafen adds any. FM voices that swing up to the `0x4000`
  step limit alias on the console too, and their reverb send softens that.
- **Impulse responses** (`tools/reverb_ir.py`): the reverb is linear, so a click through it records each type for
  convolution reverbs. Levels are at full depth. Tails, trimmed where they fall below 24-bit: Room 1.65 s, Studio A
  2.00 s, Studio B 2.07 s, Studio C 4.76 s, Hall 4.02 s, Space Echo 8.15 s, Echo 11.87 s, Delay 0.74 s (a single
  repeat) and Pipe 3.94 s.

### Timing, loops and MIDI export

- **Clock:** `snd_init` (`0x8002988C`) sets up root counter 2 with target `0x43D1` and `snd_timer_tick`
  (`0x80030234`) as its interrupt. Counter 2 runs at system clock ÷ 8 (the driver's own catch-up code counts elapsed
  scanlines in units of 66), so the driver steps about **243.86 times a second**.
- **Tempo:** each step, `seq_tick` (`0x800308D4`) adds the song tempo (the `u16` from `E8 lo hi`, kept at
  `seq_song + 0x18`) to a 16-bit accumulator, and every carry is one sequence tick. So
  ticks/s = 243.86 × tempo / 65,536, and with 48 ticks per quarter note, **BPM = tempo × 0.004651**. The 96 songs that
  set a tempo span 56–191 BPM (median 106).
- **Per-channel counter:** `+0x56` holds `gate << 8 | length`, loaded from `seq_note_lengths` (`0x3030` = 48, 48), and
  `seq_tick` takes `0x101` off it each tick. Length 0 steps the channel to its next event; gate 0 keys the voice off.
  - For an ordinary note `seq_step_channel` subtracts `0x200`, so the voice keys off 2 ticks before the next event.
    Notes of 2 ticks or less never key off early, and a note keeps its full length when the next byte is a tie, or
    in slur or full-length mode.
- **Length commands:** `A2 n` makes the next note last `n` ticks and clears the fixed length. `DC d` sets a fixed
  length for every following note, equal to the last length + signed `d`, clamped to 1–255; `d` = 0 turns it off.
  Precedence: fixed length, then `A2`, then the length table.
- **Loop commands:**
  - **`C8`:** pushes a loop point (4 levels).
  - **`C9 n`:** jumps back until pass `n` (0 = 256), then pops. So effects 294 and 369 end after 256 passes, and
    effect 396 goes on to the notes after its repeat.
  - **`CA`:** jumps back forever.
  - **`F0 n lo hi`:** jumps by a signed 16-bit offset on pass `n` of the current repeat, for a different ending on the
    last pass.
  - **`EE lo hi`:** jumps by a signed 16-bit offset, counted from after its arguments; a backward jump is the song's
    endless loop.
  - **Look-ahead counts:** `seq_look_ahead` compares the raw `C9` count, so at a `C9 00` it always assumes another
    pass; our `look_ahead` does the same on purpose. The driver also compares `F0`/`F1` counts raw and follows `EF`
    and `F1`, where ours reads `F0 00` as 256 and stops at `EF`/`F1`. No song or effect uses `F0 00`, `EF` or `F1`.
- **Instrument commands:** `A1 n` selects instrument `n` and frees an `F4` layer's second voice. `F2 n` also changes
  instrument; both rescale a sounding note's pitch from the old instrument to the new one.
  - **`F2` plays from the loop point** (`0x80032274`): it sets the voice's sample start to SPU `0x76FE0` instead of the
    instrument's and keeps the instrument's loop address. `snd_init` uploads 32 bytes there from `0x8004A60C`: two
    silent ADPCM blocks, the second flagged end + repeat. So every note after `F2`, until the next `A1`, plays 56
    silent samples and jumps to the loop, skipping the sample's attack.
  - 21 songs use `F2` (42, 47, 48, 49, 71, 78, 80–82, 84, 85 and others), all on notes; no effect does. The export
    marks it `from_loop`; the player skips the 56 silent samples.

`python tools/akao.py midi <file|dir> <out>` writes one Standard MIDI File per song:
- **Layout:** format 1 at 48 PPQ, a conductor track with tempos plus `loopStart`/`loopEnd` markers, and one track per
  channel (MIDI channel = index mod 16).
- **Notes:** key = 12 × (octave + 1) + semitone, so octave 4 = middle C.
- **Controls:** program = AKAO instrument; CC7 = `A8`; CC10 = `AA`; velocity is fixed at 100.
- **Loops:** counted repeats are unrolled, and endless loops play once.
- **Left out:** vibrato/tremolo/pan LFOs, slides, ADSR, reverb, drum-kit mode and the other effect commands.

**Song loop.** Each channel loops on its own (`EE`/`CA` back to its own target), and in 38 songs the channels don't
all start looping at the same tick (song 7: 13 channels loop from tick 768, three from tick 1152). No command ties
them together; the song as a whole repeats from the **latest** channel loop start, every lcm(loop lengths) ticks (in
practice the longest loop): 5,376 ticks for song 7, 4,608 for song 74. `play_song` keeps the earlier channels looping
until that point and cuts everything there, and both exports use it.

**A loop's first pass can play differently.** After `CC` (slur) before a loop, every pass but the first slurs into
its note with no key-on, so the sample keeps running instead of restarting each time (effect 493, a drone). A bend
(`A4`) set before a loop fires on the first pass only (effect 244). The export compares each looping channel's
notes, slurs and bends over three passes; when the first differs and the next two match, the loop starts a pass
later. That moves 14 effects' loops and no song's.

**Check:** in all **98 songs, every looping channel repeats the same length**, or a length that divides it, such as a
one-bar drum loop; echo channels may start a few ticks late. This check is what confirmed `F0`'s meaning. No channel
runs past 4,000 bars.

### Channel defaults, volume and drum kits

- **Channel reset:** `channel_reset` (`0x80029BAC`) starts every channel on instrument 5 (via
  `load_channel_instrument`), octave 2, volume 100 (`+0x44` = `0x32000000`) and an `A3` multiplier of 120
  (`+0x2C` = `0x78`).
- **Voice volume** (`seq_channel_lfo`, `0x8002ED34`): (`A8` volume `<< 7`) × `A3` ÷ 128, plus a tremolo offset
  (`+0xD8`), scaled by the global master volume `0x80062F5E` ÷ 128. That's `0x7F` at boot (`0x800294A4` stores
  `0x7F0000` to `snd_master_volume`).
- **Output mode:** the word at `0x8009A104` (named `seq_song` in our symbols) picks how the volume is split. The
  sound-setting functions at `0x8002C7E8`, `0x8002C81C` and `0x8002C850` set it to 1, 4 and 2; boot sets 1.
  - **1, stereo:** left = volume × `snd_pan_left`[i] >> 15, right = volume × `snd_pan_right`[i] >> 15.
  - **4:** the same, but odd-numbered voices get their right side inverted (`~right`).
  - **Anything else (mono):** both sides = volume × `0x80049CC4` (entry 64 of the left table, 16,256) >> 15.
- **Pan tables:** i = (`AA` pan + pan-LFO offset `+0xDA`) & 0xFF. Entries 0–127 are linear: left 32,640 − 256·i, right
  128 + 256·i. The two always add up to 32,768, so the centre is −6 dB per side, not the −3 dB of equal-power panning.
  Entries 128–255 hold unrelated data (signed squares); only a pan LFO could reach them, and simulating all four
  pan-LFO songs keeps i within 5–114.
- **On the SPU:** a volume register r plays at r ÷ `0x4000` of full scale (psx-spx). So a voice plays at
  `A8` × `A3` × 127/128 ÷ 16,384 × the pan table entry ÷ 32,768.
- **`A3 n`:** the multiplier above. In songs, 1,317 of 2,108 uses are 127, and the rest go as low as 10.
- **`A9 n v` / `AB n p`:** slide volume or pan to `v`/`p` over `n` ticks (0 = 256). The per-tick step sits at
  `+0x48`/`+0xCA` and the counter at `+0x5C`/`+0x62`. `A8`/`AA` cancel a running slide. 266 of the slides fade in
  from volume 0.
- **`EC lo hi` / `ED`:** drum-kit mode on/off (channel flag bit 3). The map pointer (`+0x14`) is a signed offset from
  after the arguments. Each note's semitone picks a 5-byte entry: instrument, key (octave = key ÷ 12, same numbering
  as `A5`), `u16` volume (stored `<< 16` into `+0x44`, so the `A8` equivalent is the value `>> 7`), and pan (`<< 8`
  into `+0x60`). 4,585 notes as written (11,645 once loops are played out) are drum notes.
- **Envelope (ADSR):** when a note starts, the driver copies instrument bytes `+0x08`–`+0x0F` into the voice, and
  `snd_update_voice` hands them to libspu. The packing, checked against the register writes and psx-spx:

| Bytes | Setter | Register bits |
|---|---|---|
| `+0x08` attack rate, `+0x0D` attack mode | `SpuSetVoiceARAttr` (`0x80039850`) | ADSR bits 8–15 (rate = shift `<< 2` \| step) |
| `+0x09` decay shift | `SpuSetVoiceDR` (`0x8003973C`) | bits 4–7 |
| `+0x0A` sustain level | `SpuSetVoiceSL` (`0x800397C8`) | bits 0–3 |
| `+0x0B` sustain rate, `+0x0E` sustain mode | `SpuSetVoiceSRAttr` (`0x800398EC`) | bits 22–31 |
| `+0x0C` release shift, `+0x0F` release mode | `SpuSetVoiceRRAttr` (`0x800399D0`) | bits 16–21 |

- **Mode values:** 1 linear up, 3 linear down, 5 exponential up, 7 exponential down.
- **What the instruments use:** all 93 have sustain level 15. 75 hold at full level (sustain rate `0x7F`), and 18
  fade while held (sustain rate 52–65).

### Pitch: bends, transpose, detune

From `seq_step_channel`'s note and tie paths:
- **Key:** octave × 12 + semitone + transpose. The pitch comes from the instrument's table for that key, then the
  detune is applied.
- **`C0 s` / `C1 s` (`seq_cmd_transpose`, `_add`):** set or add a signed semitone transpose (`+0xCC`). Drum-kit notes
  ignore it.
- **`D8 d` / `D9 d` (`seq_cmd_detune`, `_add`):** set or add a signed detune (`+0xCE`). It scales the pitch by
  1 + d/128 when positive and 1 + d/256 when negative. Song values are mostly −2…+1 (≈ ±14 cents, chorus-style), plus
  −128 (exactly one octave down) and ±60.
- **`A4 n s` (`seq_cmd_bend`):** stores a slide length `n` (0 = 256, at `+0x68`) and a signed semitone amount `s`
  (`+0xD2`). 321 bends in the songs, from ±1 semitone over 4 ticks up to +65 over 8.
  - **Start:** the next **note or tie** starts it. Both reach the same check in `seq_step_channel` (`.L8003161C`,
    after a note's key-on): a non-zero `+0xD2` makes the target key the current key + `s`, and the pitch slides
    linearly *in frequency* to the target's pitch (transpose and detune included) over `n` ticks. The amount is then
    cleared, so later ties hold the new pitch.
  - **Examples:** most songs put `A4` before a tie (song 7 channel 16: a 2-tick note, `A4 04 01`, a 10-tick tie,
    repeated). Effects often put it before a note (effect 1: +15 and +12; effect 36: +32), and so do songs 9 and 66.
  - **Target key:** `+0xD0` (the note's key before transpose) += `s`, plus the transpose `+0xCC` *now*, so a `C1`
    between the note and the bend moves the target. Effect 36's last bend comes after a loop of `C1 02`, so it lands
    2 semitones higher than `s` above the held note (key 90). The export folds this into the bend's semitones.
  - **Effects skip the detune:** with flag `+0x54` set, the target pitch comes straight from the instrument's table,
    without the `+0xCE` detune scaling that songs apply.
  - **Glide:** a `DA` glide writes the same fields on a note, so a pending `A4` bend is lost.
- **`DA n` (`seq_cmd_glide`, `0x8003257C`):** a glide (portamento). No song uses it, only sound effects (43 uses).
  - **Setting it:** glide time `+0x6C` = n (0 = 256) and slur mode on (`+0x6E` = 1, as `CC`). It also clears the last
    key (`+0x6A`) and its transpose (`+0xD4`), so the first note after it doesn't glide from a note played before
    `DA` (14 effects have one).
  - **On each note** (`seq_step_channel`, `0x8003131C`): with a glide time and a last key (`+0x6A`, 0 = none), the
    note starts on the last key and bends to its own over the glide time, through the bend fields `+0x68`/`+0xD2`.
    The last key is then this note's.
  - **Ending:** the look-ahead clears the glide time wherever it ends slur mode. It runs before the glide check, so
    the note just before a rest doesn't glide.

### Sound effects

- **The data:** `SOUND/EFFECT.ALL`. Its table's entries 2·id and 2·id + 1 (`func_8002A6C4`) are an effect's two
  channels, as offsets from `0x1000` (`0xFFFF` = none): 721 effects, 702 with two channels and 19 with one, 1,423
  streams in all. 90 loop forever (ambient sounds).
- **Playing one:** the sound commands around `0x20`–`0x34` (handlers `0x8002B6AC` and `0x8002B8B4`) look up the id
  and hand both streams to `snd_start_effect`. It sets up a pair of effect channels (`0x80096608` + slot × `0x108`)
  with the command's pan and flag `+0x54` = 1.
- **Tempo:** effect channels tick on their own accumulator (`0x80099FE8`) at tempo `0x66A8` (`0x80099FE0`, set at
  boot and never changed): about 97.8 ticks a second, 122 BPM.
- **Commands:** none use `E8` (tempo). They use plenty of slurs, detune, bends, LFOs, envelope overrides, FM (755)
  and reverb sends (1,331 `C2`). There are 29 instruments, all in the boot bank.
- **Our export:** `effects/effect_NNNN.ff7song`, which the player opens like songs
  (`python tools/ff7player.py assets/ff7song/effects`). All 721 render with sound. Half end within 3 s; the longest
  one-shots (115–117) run 7,080 ticks (72 s).
- **Noise (`C4`/`C5`, `0x80032ABC`/`0x80032B30`):** set and clear the channel's bit in the noise mask (effects:
  `0x80099FEC`; songs: `+0x2C`), so the SPU plays its noise generator on that voice instead of the sample. 305 of
  the 721 effects use it (effect 36's first channel hisses, rising).
  - **Clock (`AC n`, `0x80032D6C`):** n sets it (0–63). With either top bit set, it adds n & 0x3F instead, wrapping
    at 64. Effects keep it at `0x80099FFA`, songs at `+0x52`; it starts at 0. Several `AC`s on one tick (effect 36
    adds 1, then sets 46) fold into one cell field in the driver's order: a set drops the earlier adds, and later
    adds go onto it.
  - **The generator** (psx-spx): a 16-bit register that shifts in the inverted parity of bits 15, 12, 11 and 10, at
    most once per 44.1 kHz sample. A timer of 0x20000 >> (clock >> 2) counts down by 4 + (clock & 3) each sample.
    From power-on it repeats every 65,535 shifts. One generator and one clock serve every voice. Sony's docs give
    only the 0–63 range; Mednafen times it differently but gives the same rate.
  - **Rates** (measured in the player, all matching the formula): clock 0 gives 1 shift a second, 32 gives 345, 46
    gives 4,134, 50 gives 8,269, and 56 and up give 44,100. Effect 36's noise runs at 50–62, then settles at 46
    (about 4 kHz of shifts, a dark hiss).
- **Effect-only commands:**

| Command | Action |
|---|---|
| `B0 a b` | Decay rate a, then sustain level b (calls the `AE` and `AF` handlers) |
| `B3` | The instrument's envelope back (all five rates and the three modes) |
| `BB n` | Sustain mode |
| `CB` | All off: vibrato, tremolo and pan LFO (flags `0x01`/`0x02`/`0x04`, offsets left as they are), noise, FM and its countdown, reverb, flags `0x10`/`0x20`, slur and full length |
| `D2 n` / `D3 n` | FM on now (`D2` only), then flipped after n + 1 ticks (0 = 257) by the countdown `+0xA6` in `seq_channel_slides`. `C7` stops the countdown |
| `D4` / `D5` | Flag `0x10`: the voice's pitch register becomes the previous channel's (`−0x108 + 0xF8`) plus its own vibrato and `+0x36`, instead of its own pitch |
| `D6` / `D7` | Flag `0x20`: the voice volume becomes the previous channel's pitch register × 2 × `A3` ÷ 128 instead of `A8` × `A3`, so it follows the other voice's pitch |
| `DB` | Glide off (`+0x6C` = 0). The look-ahead ends a glide there too |

- **`D6`/`D7` in use:** only effects 35 and 65, both with FM. The volume can pass the SPU's 15 bits: the setter at
  `0x800393C8` masks with `0x7FFF` without clamping, so it wraps round to a small negative volume. The player does
  the same.

### How the driver treats effects

Every command in `EFFECT.ALL` is handled. The three without a case need none: `A0` ends a stream, and `CD`/`D1` have
empty handlers because the look-ahead does their work.
- **Flag `+0x54`:** `A0`, `A1`, `AC`, `B4`, `B8`, `C2`–`C7`, `F2` and `F3` branch on it.
  - Most only pick the effect copy of a mask or setting: noise (`0x80099FEC`), reverb (`0x80099FF0`), FM
    (`0x80099FF4`) and the noise clock.
  - `B4`/`B8` take no delay: a non-zero first byte is the depth instead.
  - `A0` hands the voice back to the music without a key-off: it clears the channel from the effect masks
    (`0x80099FCC`, noise, reverb, FM) and sets the music channel's update flags. A note held to the end sounds on at
    its last volume until the music or the next effect takes the voice. Effect 643's last note fades only to volume
    1: its slide's final step to 0 comes on the `A0` tick, after the voice has left the update mask. The player
    keys everything off when a piece ends without a loop, since it has no music to take the voice back.
  - `C6` (`0x80032BB4`, also behind `D2`) sets an effect voice's FM bit only outside the mask `0x555555`, and an
    effect's first channel is always on an even voice, so FM on a first channel does nothing. 25 effects ask for it
    (270, 297, 302 and 447 among them); the player drops those `C6`s too.
  - `F3` sets the flag on a song and is never used.
- **`A1`/`F2` on a sounding note** (songs too): the voice's pitch (`+0x30`) is rescaled by the new instrument's C
  pitch over the old one's (`instrument_table` + 0x10), and the voice keeps its sample until the next key-on.
  Effects 86, 309, 412, 435 and 724 and songs 38 and 72 change instrument between a note and its tie. The player
  rescales the same way.
- **Voice update** (`seq_effect_lfo`, run on effect channels by `seq_update_voices`): vibrato, tremolo and pan LFO
  as for songs, but volume skips the master volume and uses the channel's own scale (`+0xC6` high byte ÷ 128), and
  pitch gets the channel's own scale (`+0x3D`). It steps the effect LFOs itself (songs do it in `seq_channel_lfo`),
  on every interrupt while the voice is keyed on.
- **Per-tick update** (`seq_effect_slides`, where songs run `seq_channel_slides`): the volume slide, the noise and
  FM countdowns (`+0xA4`/`+0xA6`), bends and LFO depth slides. It has no pan slide (`+0x60`/`+0x62`), so `AB` does
  nothing on an effect (effects 112, 113, 117; the export leaves their pan alone). It has no LFO delay countdown
  (`B4`/`B8` start effect LFOs with none) and no layer balance slide.
- **Set by the game at play time, not by the effect data:**
  - the pan: the play command's argument (the player uses the centre);
  - the volume and pitch scales: `func_8002BCCC` sets a pair's volume and `func_8002C2CC` the pitch scale. Boot sets
    them to `0x7F00` and 0 (`0x800294A4`'s loop over the effect channels), which the player plays: 127/128 and no
    change;
  - the reverb: the current song's (the player plays effects dry);
  - stopping the 90 that loop.
- **Left to context:** effect 419 plays noise before setting the clock, so in the game it uses whatever clock the
  last effect left. On its own it starts at 0.
- **Settled by ear:** effects 270, 297, 302 and 643 have no known driver-side cause for sounding odd, and only a
  recording from the game could reopen them.
  - Effect 302's tremolo (`B8 1F 02 00` on the cymbal, ±24% at 61 Hz) sounds wrong, but the driver's math and
    timing for it match ours as far as read, so it stays.
  - Effect 270 was checked step by step, its FM against psx-spx and Mednafen. Its commands come out as the driver
    plays them: a 58-semitone swoop and a +16 bend on the modulator, a 1-tick blip then a long note with a square
    vibrato on the FM voice, and their envelopes.

### Driver cross-check

Every driver function the player depends on is decompiled with Ghidra from `build/main.elf` (our matching build,
with our names) and compared with `tools/akao.py` and `tools/akao_synth.py`: `seq_tick`, `seq_step_channel`, the
look-ahead `func_800318BC`, the per-tick updates (`func_8002E478` songs, `seq_effect_slides` effects), the voice
updates (`func_8002ED34`, `seq_effect_lfo`), `seq_update_voices` and all 85 sequence command handlers.
- **Notes:** the note, tie and rest paths, the key-off gate (table lengths, `A2`/`DC`, slur and full length), the
  look-ahead, glides and drum kits all match: no note goes missing.
- **Argument lengths** match the driver's skip table (`0x800498A8`): `E9` and `EB` take 3 bytes (neither occurs on
  the discs).
- **`A1`** frees an `F4` layer, and **`A2`** sets the length `DC` counts from (no data hits either).
- **Slurred notes and bends:** a slurred note zeroes the bend offset (`+0x34`) but not its countdown (`+0x64`), so a
  running bend carries on from the new note at the same step. The player does the same.
- **Not modelled by the player:** `A8`/`AA` read signed, unmasked bytes (no data goes past `0x7F`); volume and pan
  slides keep 1/128 and 1/256 of a step where the export rounds per tick; `F4`'s voice allocation can fail when all
  24 voices are taken.

### Our song format and player

`tools/ff7song.py` and the player/editor `tools/ff7player.py` (spec: [`ff7song_format.md`](ff7song_format.md)) play
the driver's own behaviour.
- **Patterns are bars**, so any `FD` time signature works. Identical bars share a pattern: 81 of 98 songs reuse some.
  The 98 songs come to 9.8 MB of JSON plus a 1.7 MB bank (93 instruments).
- **Events sit at exact ticks**, and effects keep the driver's parameters.
- **Notes keep their full length across the loop end**, as the driver holds them. A channel whose loop opens with a
  tie keeps the previous pass's last note sounding; the exporter plays one extra pass to count those ties (87 notes
  in 21 songs). A note still held after that pass is a drone, tied on forever with `"len": null`: songs 66
  (channels 1–2) and 75 (channels 8 and 14).
- **Timing** (`tools/akao_synth.py`): 243.86 Hz interrupts and the tempo accumulator; note lengths, bends and LFO
  delays per tick; LFO steps per interrupt; the SPU ADSR at 44.1 kHz, worked out per stretch between interrupts.
  Songs start at the driver's tempo `0xFFFF` until their first `E8`.
- **Articulation:** early key-off, slurs, the channel's own envelope settings, and `F8` overlap (the previous note
  rings on in its release on a second voice).
- **Other driver features:** the layered second instrument with its balance (`F4`–`F7`), the pan LFO (`BC`/`BD`),
  LFO depth slides (`DD`/`DE`), FM (`C6`/`C7`), and SPU reverb with the song's preset, depth and sends (see
  "Reverb"). Song 82 gets instrument bank 2 (`"bank": 2`; see the sound files table).
- **Volume and pan:** the driver's own (see "Voice volume"): stereo mode, the master volume at boot, linear pan
  tables. Boot sets the SPU main volume to `0x3FFF` (full scale, in the common attributes at `0x8009C578`), so the
  console clips a loud mix just as the player does: 10 of 98 songs touch full scale in their first 15 s, at most
  0.031% of samples (song 92).
- **Interpolation:** the SPU's 4-point Gaussian (psx-spx). Each output sample weighs the sample before the position,
  the one at it and the two after, by one of 256 steps between samples.
  - **Weights:** psx-spx lists the hardware's 512-entry table but no formula, so we build our own curve rather than
    copy it: a Gaussian (σ 0.85 samples) under a (1 − (d/2)²)³ window, scaled so each step's four weights add up to
    `0x7F80` as the hardware's do. It comes within 17 of the table's 32,768ths, about −63 dB on white noise. (A
    degree-14 polynomial would match to ±1, but its coefficients would just be the table re-encoded.)
  - **Sound:** it smooths. A signal flipping every sample comes out at 0.4, where linear interpolation passes it
    whole, so high notes and samples pitched far up sound duller than with linear.
  - **Cost:** rendering takes 22–30% of real time on our machine, with no dropouts.

### LFOs: vibrato, tremolo, pan

Read from the command handlers, the note path at `0x80031518` and `seq_channel_lfo` (`0x8002ED34`).

| | Vibrato | Tremolo | Pan LFO |
|---|---|---|---|
| On | `B4 delay rate wave` (`seq_cmd_vibrato`, `0x80032614`) | `B8` (`0x80032804`) | `BC rate wave` (`0x8003298C`) |
| Depth | `B5 d` (`0x80032718`) | `B9` | `BD d` |
| Off | `B6` | `BA` | `BE` (`0x80032A98`) |
| Depth slide | `DD n d` | `DE` | none |
| Offset | `+0xD6` | `+0xD8` | `+0xDA` |

- **Effects:** a channel with flag `+0x54` set (by `snd_start_effect`, never by songs) reads `B4`/`B8`'s first byte
  as the depth instead of the delay.
- **Timing:**
  - Each note restarts the LFOs, and the commands act on the sounding note at once. An `on` restarts its LFO on the
    spot: delay, a step countdown of 1, the waveform's first step, and for vibrato the amount from the note's pitch
    (`+0x30`). `B5` recomputes the vibrato amount at the new depth and `B9`/`BD` store theirs, all keeping the LFO's
    place. An `off` clears the LFO's flag and zeroes its offset; `B6`/`BA` keep the depth.
  - Effect 302 switches vibrato on during a held note and swaps its depth every 24 ticks. Since only `on` restarts,
    the export marks each one with a `vibrato_on`/`tremolo_on`/`pan_lfo_on` field. 85 songs and 61 effects have a
    vibrato `on` in a cell without a note, 6 songs and 48 effects a tremolo one, and 4 songs a pan LFO one; only
    those under a held note change the sound.
  - The delay counts sequence ticks (`seq_channel_slides`). After it, the LFO reads its next waveform step every
    `rate` **timer interrupts** (≈ 243.86 Hz, whatever the tempo; 0 = 256). `seq_tick` runs the voice update
    (`seq_update_voices` → `seq_channel_lfo`) on every interrupt, before the tempo accumulator.
- **Waveforms (`lfo_waveforms`):** 16 pointers to s16 tables that loop back on the marker `0, 0, −n`.
  - 0/1 square ±; 2/3 square 0/+; 4/5 square 0/−; 6/7 sine, 39 steps per cycle (a 40-step sine missing its 4th
    step); 8/9 triangle, 16 steps; 10/11 sine, 20 steps; 12 noise; 13–15 reuse 9/8/9.
  - Even numbers (and 14) fade in over four cycles at ¼, ½ and ¾ scale, then repeat the full-scale one.
  - **12, noise:** 256 steps that repeat from the start (end marker `0, 0, −256`). It isn't a shape, so the exporter
    copies it from the user's executable into the songs that use it (`lfo_noise`). Song 76 runs it as vibrato on
    channels 1–4 (`B4 00 01 0C`, depth 60): the pitch jumps around −24…+45 cents every interrupt. Song 9 selects it
    at depth 0, so it's silent there. The XM export leaves it out.
- **Vibrato:** `+0x7C` = (d & 0x7F) × pitch × (1 if d's top bit is set, else 15/256) ÷ 128. The pitch offset
  `+0xD6` = `+0x7C` × step ÷ 65,536, doubled when positive, is added to the voice pitch.
  - **Usual setting:** `B4 60 01 06` + `B5 0A` (1,412 of the 1,556 `B5`s): after a half note, a 6.25 Hz sine fades
    in to about +8/−4 cents.
  - **Largest:** `B5 9F` in song 66, +3.6/−2.2 semitones at 0.25 Hz.
- **Tremolo:** volume offset `+0xD8` = voice volume × d/128 × step/32,768 (not doubled).
  - **Deeper than `0x80`** it takes the volume below zero, and the driver writes it unclamped. The SPU's volume is
    15-bit signed, so the wave plays inverted, not silent; the player does the same. Effect 643 slides its tremolo
    depth to `0xFF` on a square wave.
  - **Most common:** `B8 00 03 05` + `B9 78` (song 71) cuts the volume by 94% every other 3 interrupts, a 40 Hz buzz.
- **Pan LFO:** offset `+0xDA` = (d ÷ 2) × step/32,768 pan units.
- **Usage in the 98 songs:** vibrato in 93 (1,561 `B4`), tremolo in 11 (26 `B8`), pan LFO in 4, depth slides only in
  song 71.

### Tracker export (XM)

`python tools/xm.py <file|dir> <SOUND dir> <out>` writes FastTracker 2 modules that play the game's own samples. The
exporter is frozen.
- **Grid:** one XM tick = 2 AKAO ticks, so the XM BPM equals the song's BPM (56–191, inside XM's 32–255). Songs
  without bends (83) run at speed 1 (rows of 2 AKAO ticks), songs with bends (15) at speed 2 (rows of 4), since
  portamento only acts on a row's later XM ticks; their notes on the second XM tick get note delay `ED1`. Positions
  round to the grid (±1 AKAO tick), never lengths.
- **Patterns** hold whole bars of the song's `FD` time signatures, as many as fit in two bars of 4/4, and one starts
  at the loop. Once every channel is looping, the song repeats from any later point, so the loop moves to the next
  bar line (26 songs, each checked seamless: the moved stretch repeats exactly one period later). The endless loop
  is the XM restart position.
- **Instruments:** one 16-bit sample per AKAO instrument, with its loop. Bank 1, except song 82, which gets bank 2
  in slots 53 and up.
  - **Tuning:** relative note + finetune = 12·log2(44,100 · pitch[C] / 0x1000 / 8,363) − 12·(6 − base − 4), where
    `base` shifts songs whose octaves exceed XM's 8. One value per sample is enough: the pitch tables are
    equal-tempered to within 0.86 cents.
  - **Envelopes:** each instrument's ADSR is simulated with psx-spx's step rules at 44.1 kHz and sampled into an XM
    volume envelope, timed at the song's first BPM. Steady instruments get attack/decay, a sustain point, then the
    release curve. Fading ones put the sustain fade before the sustain point and turn the release into the fadeout
    (clamped to FT2's 0xFFF). Phases that move less than one XM volume step in 10 s count as flat (sustain `0x7F`
    steps once every 23.8 s).
- **Events:** each channel starts from the driver's defaults (instrument 5, octave 2, volume 100, `A3` 120).
  - Notes carry the voice volume 64 × `A8` × `A3` ÷ 127²; the global master volume and the pan law are left out.
    `AA` becomes `8xx` panning, slides become per-tick volume/pan values, and transpose is added to the key.
  - Drum-kit notes carry their entry's instrument, key, volume and pan. Every one of the 152,284 notes has a sample.
  - Detune: whole semitones go into the note, the rest into fine portamento `E1x`/`E2x` on the note's row (1/16
    semitone steps).
  - Bends: portamento `1xx`/`2xx` (16 units per semitone) on each row's second XM tick, stepping to where the
    driver's frequency-linear slide is by the end of that tick. A bend stops at the next note.
  - Tempo changes go on an extra control channel.
  - **Key-offs:** a note that doesn't run into the next one gets a key-off after the channel's other events, 2 ticks
    before the next note as the driver does. It is skipped where a note already starts in that row, and moves to the
    row's start where the effect column is taken, so bends keep their cells.
- **Vibrato and tremolo are baked per row:** each row gets the average of the driver's LFO over the timer
  interrupts it covers, so the 40 Hz tremolo comes out as its mean level instead of an aliased wobble. Vibrato
  becomes extra-fine portamento `X1x`/`X2x` (1/64 semitone), or `E1x`/`E2x` for steps over 15, in free effect cells
  only (a step whose cell is taken is made up on the next free row). Tremolo becomes absolute values in the volume
  column. `4xy`/`7xy` don't fit: FT2's smallest vibrato depth is ±1/8 semitone, twice the usual setting, its speed
  can't reach 6.25 Hz at most song tempos, and neither effect runs at speed 1.
- **Collisions:** a second note in a cell moves to the next row, and later volume or pan values in a row replace
  earlier ones. A pan change that clashes with another effect moves one row earlier. Still dropped across all 98
  songs: 14 notes (fast runs in songs that need speed 2), 43 detune steps on delayed notes, 13 detune steps blocked
  by pan, and the noise-waveform vibrato on 146 notes (song 76).
- **Not carried:** slurs (every note retriggers), the channel envelope overrides (an XM instrument has one fixed
  envelope), `F8` overlap and `F4` layers (an XM channel plays one note at a time), the pan LFO, LFO depth slides,
  LFO changes in the middle of a note, FM and reverb. The MIDI export doesn't carry bends, detune or LFOs.
- **Checks:** `python tools/test_xm.py assets/xm/*.xm` parses each file back and requires every byte to be accounted
  for. A listening check (2026-10-28, before vibrato/tremolo were added) found instruments, volume balance, drum
  kits, the song loop (song 7), bends and detune right.

### Sound files

Checked against our disc:

| File | Size | Layout |
|---|---|---|
| `SOUND/INSTR.ALL` | 483,232 | 16-byte header: `u32 spu_addr` (`0x1010`), `u32 length` (`0x75F90` = file size − 16), 8 zero bytes; then raw SPU ADPCM sample data uploaded to SPU RAM at `spu_addr` |
| `SOUND/INSTR.DAT` | 8,192 | 128 × 64-byte instruments, copied whole into `instrument_table` by `load_instrument_bank` (`0x800297A4`). Entry: `+0x00` `u32` SPU sample address, `+0x04` `u32` SPU loop address, `+0x08` 8 bytes of ADSR settings, `+0x10` 12 × `u32` SPU pitch for each semitone of octave 6 (other octaves shift it). 93 slots used |
| `SOUND/INSTR2.ALL` | 251,120 | Same header (`spu_addr 0x38560`, length `0x3D4E0`), second bank uploaded over the top part of bank 1's sample memory |
| `SOUND/INSTR2.DAT` | 8,192 | `load_instrument_bank2` (`0x80029818`) copies only the first 75 entries (4,800 bytes) to slot **53** onward (`instrument_table + 0xD40`). 6 used, becoming slots 53–58 |
| `SOUND/EFFECT.ALL` | 51,200 | Sound effects: a `0x1000`-byte table of 2,048 `u16` offsets (`0xFFFF` = empty) relative to `0x1000`, then 1,423 header-less command streams (same command set as music). Identical on all discs |
| `*.SND` (e.g. `MINI/ASERI2.SND`, `ENEMY6/FAN2.SND`) | small | One AKAO block (layout below) |

- **Bank 1** (`INSTR.DAT`, `INSTR.ALL`) is read by LBA from the exe's file table (`instr_dat_file`, `instr_all_file`
  in `src/main/main.c`) and loaded once at boot.
- **Bank 2:** only main-loop mode 15 loads it (`snd_load_bank2`, `0x80011274`: sectors `0x1E4` and `0x25F`).
  `INSTR2.ALL` uploads at SPU `0x38560`, exactly where bank 1's slot 53 starts, so from then on slots 53–127 hold
  only bank 2's six instruments. Song 82 (`FIELD/LASTMAP.DAT`) uses exactly those six plus low slots, while every
  other song that uses slots 53+ needs bank 1's instruments there. So song 82 is the bank-2 song (`BANK2_SONGS` in
  `tools/instruments.py`).

**Samples** are SPU-ADPCM (psx-spx: 16-byte blocks, 28 four-bit samples, 5 prediction filters):
- **Loops in bank 1:** bank 1 samples set no loop-start flags. Their blocks carry flag `0x02` and the last one `0x03`
  ("end, then repeat"), so the loop point comes only from `INSTR.DAT`'s loop address.
- **Converter:** `tools/instruments.py` turns both banks into 99 WAVs (64 looping, with `smpl` loop chunks) plus
  `instruments.csv`. Checks: no sample runs into the next one, no output clips, and a listening check (2026-10-28)
  found the WAVs right.
- **Playback rate:** WAVs play at 44,100 Hz, which is SPU pitch `0x1000`. The game plays semitone *s* of octave 6 at
  44,100 × `pitch[s]` / 0x1000 Hz; the CSV lists the rate for C.

**AKAO block** (in `.SND` files, field maps, `MINI/BGM*.LZS`, `MINI/CHOCOBO.DAT`, `MOVIE/STAFF*.BIN`):
- `0x00` `"AKAO"`.
- `0x04` `u16` song id.
- `0x06` `u16` data length (block = 16 + length).
- `0x08` `u16` reverb mode (libspu's type; see "Reverb").
- `0x0A` 6-byte BCD timestamp, e.g. `96 12 18 22 46 28` = 1996-12-18 22:46:28.
- `0x10` `u32` channel mask.
- `0x14` one `u16` per set bit. Each is relative to the byte after itself, so channel *i* starts at
  `0x14 + 2i + 2 + offset`.

Field maps embed their music as AKAO blocks: 613 of the 703 LZS field `.DAT`s contain the `"AKAO"` magic after
decompression, roughly 600 real sequences to test a sequence parser against.

Field script opcodes that drive sound, as named in Gears' opcode table (the bytes are facts; behaviour is still to be
confirmed in code): `0xDA AKAO2`, `0xF0 MUSIC`, `0xF1 SOUND`, `0xF2 AKAO`, `0xF3 MUSVT`, `0xF4 MUSVM`, `0xF5 MULCK`,
`0xF6 BMUSC`, `0xFC FMUSC`, `0xFD CMUSC`.

On PC, Gears says the PsyQ SEQ player was swapped for a MIDI player. Our `FF7.EXE` has the source paths
`lib\src\sound\midi1.cpp`, `sound.cpp`, `dx_snd.cpp`, `acm.cpp` and `creative\sfutils.cpp`. Nothing in the PC sound
path descends from AKAO, so the PC build is no reference for PS1 sound.

## 5. Engine architecture (Gears, PC-era research)

Gears is the Qhimm team's 2000s write-up of the engine, edited by Joshua "Halkun" Walker. It's an unfinished draft:
several chapters (most mini-games, the platform bug lists) are lorem-ipsum placeholders. What matters for PS1 code:

- **Kernel and modules.** The main exe is a resident kernel: threaded main loop, memory manager, CD loader and
  statically linked PsyQ libs. Field, battle, world, menu and mini-games swap in as overlays. The field module drives
  everything and its script can call any other module; battle can't open the menu.
- **Save map.** All persistent state is one 0x10F4-byte (4,340) block, copied wholesale to the memory card. Field
  scripts reach 5 banks in it, 8- or 16-bit wide, plus 256 bytes of non-saved temporaries. `SAVEMENU` previews only
  the first 80 bytes of each slot.
- **CD loading.** No BIOS file API: reads go by **sector number** through low-level calls, in chunks of up to 8 KiB,
  so the next module can preload while the current one runs. Fits `YAMADA.BIN` being an LBA/size table.
- **VRAM.** Double-buffered framebuffers on the left, CLUTs below them, then field textures and texture caches. Menu
  textures and the font stay resident on the right. 24-bit movies widen the display over the first two texture caches.
- **Menu module IDs** for the field `MENU` opcode (first argument always `0`): Party `0x09`; Form `0x07` (argument
  0 = make a party of 3, 1 = split into 3 groups, 2 = split into 2); Save `0x0E`; Name `0x06` (argument
  0–9 = Cloud … Chocobo); Shop `0x08` (argument = shop number). Other menus are reached only internally.
- **Menu assets.** Some `.MNU` overlays have TIM images appended; `PATYMENU` and `FORMMENU` share external TIMs on
  disc. `WINDOW.BIN` holds the resident window textures and font.
- **Debug rooms.** Field maps named after staff (Kitase, Nojima, Kyounen, …) jump to other scenes; useful entry
  points when tracing field code.
  - The lobby is field `0x41`. The well-known US GameShark code `8009ABF6 0041` holds the field module's request
    number at it. `D_8009ABF5` says what the request is: 1 a field change (the gateway code writes it with the
    target), 2 a battle (`D_8009ABF6` is then the battle's number, which `func_800111E4` hands on), 3 a movie (its
    number in `MOVIE_ID.BIN`). So the code turns every request into number `0x41`, and is only good for steering a
    field change.
  - Held on through NEW GAME, it turns the opening movie (`0x35`) into movie `0x41`, which disc 1's table leaves
    empty (LBA 0). The field script asks for the movie and `VSync`s before the field module reads the number, so a
    code applied at every vblank always gets in between. libcd's `StCdInterrupt` keeps only sectors with the `0x160`
    id and the stream's channel, so the ring never fills: `movie_poll_start` never sees its 8 sectors, and the field
    waits on the movie with the game still running.
  - Checked on the PC build: with the code left off while `D_8009ABF5` is 3, the opening movie plays, and a field
    change from the first field (`0x74`) lands in the lobby, which draws. Walking there instead starts the guards'
    battle, which the PC build can't run yet. The lobby showed white bars above and below its picture; not yet
    checked against the PS1.

### Original PC source tree

The PC `FF7.EXE` still holds 100 source paths, left over from disabled memory-allocation tracing (Gears lists the
same set). They give the original module and file split, and the staff folders behind PS1 overlay names (e.g.
`battle\yama\` ↔ `INIT/YAMADA.BIN`), useful when naming recovered PS1 files:

| Folder | Files (examples) |
|---|---|
| `c:\ff7\field\src\` | `ad_app`, `ad_bk`, `ad_cdr`, `ad_data`, `ad_human`, `ad_obj`, `ad_tile`, `tutaddr` |
| `c:\ff7\src\battle\` | `battle`, `b3ddata`; `battle3d\` (`char`, `enemy`, `limitbrk`, `stage`, `mdl`, `lmd`); staff dirs `yama\`, `yasui\`, `myoshiok\` |
| `c:\ff7\src\menu\` | `english\loadmenu`, `btlmenu\english\callback` |
| `c:\ff7\src\wm\` | `wmdefine`, `wmfile` |
| `c:\ff7\src\main\` | `main`, `initpath` |
| `c:\ff7\{chocobo,condor,coaster,highway,snobo}\` | per mini-game: `ch_*`, `cd_*`, `psxdata*`, `memory`, `tmd` |
| `c:\lib\src\` | Eidos/Square port library: `graphics\` (DirectX + software renderer, `psx.cpp`, `psxgraph.cpp`), `sound\`, `file\`, `mem\`, `thread\`, `movie\`, `polygon\` |

- `snobo\tmd.cpp` and `highway\psxdata.cpp` suggest the PC mini-games wrap PS1-format data almost directly.
- To regenerate the list, extract `FF7.EXE` from the PC install disc and search it for strings starting with `C:\`.
  Error strings such as `ACCESS.CPP::SOUND_SIDE_SWITCH` survive too.

## 6. Tooling notes

- `tools/extract_disc.py`: stdlib ISO9660 reader for raw 2352-byte images, MODE2 (PS1) and MODE1 (PC). Files with
  any Form 2 sector (movies) are written as raw 2352-byte sectors so video players / jPSXdec keep the interleaved XA
  audio. `-l` lists LBA, size and SHA1 without writing anything. Extracting also writes `files.txt` (first sector,
  size, raw or not, path), the sector index the PC build reads through.
- `tools/unpack.py`: detects and decompresses gzip overlays, plain gzip, BIN-GZ archives and LZS.
  `tools/test_unpack.py` is the LZS self-check.
- `tools/kernel.py`: `KERNEL.BIN` to editable files and back (§2, Text and `KERNEL.BIN`); `tools/test_kernel.py`
  checks the character set, the codes, the `F9` packing and its limits, and the archive.
- `tools/scene.py`: `SCENE.BIN` to a JSON file per scene and back (§2, `BATTLE/SCENE.BIN`); `tools/test_scene.py`
  checks the block packing, the scene table and the names.
- `tools/field.py`: each map to a JSON file and back (§2, `FIELD/*.DAT`): scripts, dialogue, walkmesh, background
  tiles, camera, exits and triggers, encounters, models; with an LZS packer. `tools/test_field.py` checks the
  disassembler, the names, each section's layout and the packer.
- `tools/shop.py`: `SHOPMENU.MNU`'s shops, titles, clerk lines and prices to a JSON file and back (§2,
  `MENU/SHOPMENU.MNU`); `tools/test_shop.py` checks the layout and its limits.
- `tools/model.py`: the field's models (`.BCX`, `.BSX`) to JSON files and back (§2, `FIELD/*.BCX` and `.BSX`), with
  posing and the GTE's lighting; `tools/test_model.py` checks the layout, shared data, its limits, posing and light.
- `tools/model_editor.py`: the field viewer, models and maps (`docs/model_editor.md`); its PS1 view and the maps'
  backgrounds are `pc/gpu.c` as a library (`make editor`, with `tools/ps1gpu.c`); `tools/test_model_editor.py`
  checks the texture words, packets, the background's draw order, the projection and how a map starts.
- `tools/battle.py`: the battle's models (§2, `ENEMY1..6/*.LZS`) as the field's shapes, its stages, places and
  camera, shadows, the action scripts and the camera scripts (the opening's and the attacks' `CAMDAT`), for the
  viewer; `tools/test_battle.py` checks the animation bitstream, the layouts, posing, loading places, the stage's
  lists and motion, shadows, the camera's integers, the opening's eases, an attack's script and an attack camera.
- `tools/world.py`: the world map's event scripts to JSON files and back (§2, the world map's event scripts), with
  a repacker that keeps an LZS stream up to a change; the world map's frame (§2, `WORLD/WM*.MAP`): meshes, block
  swaps, textures, light zones, the camera, the sky, the party leader's model (posed and lit) and shadows, drawn
  through the port's GTE; `tools/test_world.py` checks the encoding, the table, the repacker, the map's layout, the
  zones, the camera, the models and the draws with a stand-in GTE.
- `tools/movie.py`: each streamed movie becomes an AVI (MJPEG plus 16-bit PCM) and a `.cam` file (40 bytes per
  frame); each in-memory one a silent 30 fps AVI, found by matching file sizes against `MOVIE_ID.BIN`. The JPEGs hold the MDEC's own dequantized coefficients, with a quantization step per position that divides them
  exactly, so nothing is re-encoded. Checked on `MK8.STR` against ffmpeg's MDEC decoder: the planes agree to ±1
  (decoder rounding), and on `FSHIP2.BIN` and `OPENMV.BIN` too. Audio on `BRGNVL.MOV` is bit-identical to ffmpeg's XA decoder. Emphasized sectors go through a
  50/15 µs de-emphasis (bilinear, within 1 dB of the analog curve to 10 kHz), which brings those movies within about
  1 dB of their PC versions (up to 8 dB brighter before). `tools/test_movie.py` checks the coefficient code table
  and the 16-bit Huffman limit.
- Matching needs the original compiler era: PsyQ's gcc 2.x for mipsel. The open-source matching tools (splat,
  maspsx, m2c, asm-differ) are separate projects; check each license before vendoring. rabbitizer (MIPS/GTE
  disassembler) is MIT.
- References: psx-spx for the hardware (SPU, GPU, GTE, CD-ROM, MDEC; SPU ADPCM and register layouts); the Sony PsyQ
  SDK docs (`Libref.pdf`, `Filefrmt.pdf` for TIM/TMD/VAG/VAB). Which libraries the game links: §8.

## 7. Toolchain: what built the game, and what our SDK has

### What built the game (checked on `SCUS_941.63`)

- Linked PsyQ libraries carry CVS stamps from 1995–96: `libetc intr.c 1.73` (1995-11-10), `libcd bios.c 1.76`
  (1996-04-03), `libgpu sys.c 1.115` (1995-11-29), i.e. roughly PsyQ library 3.3–3.5.
- Game code shows the SN assembler's signature: indexed global access expands through `$at`
  (`lui $at,hi; addu $at,$at,$a0; lw $v0,lo($at)`), load-delay `nop`s are inserted, and branch delay slots are
  filled only from instructions *before* the branch, never copied from the branch target.
- Every file builds with `-O2`. Per file, as matched byte for byte (the `Makefile` sets these):

| File | Range | GCC | ASPSX | `-G` | maspsx extras |
|---|---|---|---|---|---|
| `main.c` | `0x8001117C`–`0x80013C9C` | 2.6.3 | 2.21 | 0 | `--expand-div` |
| `transition.c` | `0x80013C9C`–`0x800144D8` | 2.7.2 | 2.34 | 0 | |
| `loader.c` | `0x800144D8`–`0x80014A00` | 2.7.2 (2.6.3 also matches) | 2.34 | 8 | `--use-comm-section -G8` |
| `bitops.c` | `0x80014A00`–`0x80014A84` | 2.7.2 | any 2.21–2.79 (2.79 set) | 0 | |
| `sound_init.c` | `0x800293D0`–`0x80029A4C` | 2.6.3 | 2.21 | 8 | `--expand-div --use-comm-section -G8` |
| `sound.c` | `0x80029A50`–`0x80033A90` | 2.6.3 | 2.21 (or older) | 0 | `--expand-div` |
| `cd.c` | `0x80033A90`–`0x80034CF0` | 2.6.3 | 2.21 | 0 | `--expand-div` |
| `movie.c` | `0x80034F3C`–`0x8003623C` | 2.6.3 | 2.21 | 0 | `--expand-div` |

- **How the versions show:**
  - GCC 2.6.3 over 2.7.2: `func_800135C0` (2.7.2 keeps the quotient in another register); `seq_cmd_loop_point`
    reloads the loop level after storing it and `seq_cmd_jump` adds into a different register (2.7.2 gets both
    wrong). Every sound function matched also matches with 2.6.3.
  - GCC 2.7.2 over 2.6.3: `bitops.c` (2.6.3 matches one of its three functions); `transition.c` (under `main.c`'s
    toolchain a loop counter reset isn't duplicated as the game has it).
  - ASPSX 2.21: `li 1` is `ori` (2.34 and older), and an indexed access goes through `addiu $at` (before 2.30):
    `lui at; addiu at, at, %lo(sym); addu at, at, v1; lw v0, 0(at)`.
  - ASPSX 2.34: `li` is still `ori`, but an indexed load folds into `%lo`: `lui at; addu at, at, a0; lw v0,
    %lo(sym)(at)`.
  - `--expand-div`: every `/` carries ASPSX's divide-by-zero and overflow checks (`break 7`, `break 6`), which maspsx
    only adds with this flag.
  - `-G8`: small globals reached through `$gp`. In the driver, `0x800293D0`–`0x80029A4C` use `$gp`; from `0x80029A50`
    on, small globals like `0x80062F00` are reached with `lui`.
  - The ASPSX version makes no difference for simple leaf functions (`bitops.c`); it shows with `div`, `li`,
    indexed access or `$gp`.

### The PsyQ 4.4 SDK (runtime library 4.4, "DTL-S2350")

| Tool | Version | Runs on Win10 x64? |
|---|---|---|
| `pssn/bin/CC1PSX.EXE` | GCC **2.8.1** SN32 build 4.0.0010 | Yes (Win32) |
| `pssn/bin/ASPSX.EXE` | ASPSX 2.79 | Yes (needs absolute paths) |
| `pssn/bin/PSYLINK.EXE` | Psylink 2.73 (`/p` = raw binary at `/o` org) | Yes |
| `pssn/bin/DOS/CC1PSX.EXE` | GCC **2.6.3** (go32 DOS) | No, needs DOSBox |
| `pssn/bin/DOS/ASPSXD.EXE` | ASPSX 2.79D (DOS) | No, needs DOSBox |
| `psx/lib`, `zips/Lib44`, `zips/Lib45` | libs 4.4/4.5, stamps from 1997–98 | n/a |

- **Win32 `CC1PSX` → `ASPSX` → `PSYLINK /p`** on `80014A00`–`80014A80` gives the game's instruction selection and
  register allocation, but not its bytes: GCC 2.8.1 fills a loop branch's delay slot with a copy from the branch
  target where the game has a `nop`, and `-fno-delayed-branch` leaves every slot empty (ASPSX 2.79 doesn't fill
  them). It also omits an empty 8-byte stack frame the game has *(probably the compiler version)*. Useful for quick
  "same instructions?" checks only.
- **The DOS GCC 2.6.3** is the right version family but needs DOSBox; untested.
- **The libraries can't be used** for matching: they're two years newer than what's linked. Library code in the exe
  stays as bytes from the user's own disc, which also keeps Sony code out of the repo.
- **Headers** (`psx/include`, `LIBSPU.H`, …) and `Devrefs/Libref.pdf` give the library signatures and structs, for
  naming the library functions in the exe.

### Our build toolchain (open, runs in WSL/Linux)

`tools/setup_toolchain.sh` fetches everything into the git-ignored `toolchain/` folder; nothing is vendored.

| Piece | Source | License | Role |
|---|---|---|---|
| GCC 2.6.3 / 2.7.2 "Sony Playstation" `cc1`, `cpp` | decompals/old-gcc release `0.17`, SHA256-pinned in `tools/toolchain.sha256` | GPL (GCC) | Compiler |
| maspsx | mkst/maspsx at commit `4ffa8b51` | MIT | Rewrites GCC output the way SN's ASPSX would: delay slots, `$at` expansions, `nop`s |
| GNU binutils (`mipsel-linux-gnu-*`) | Ubuntu package `binutils-mipsel-linux-gnu` | GPL | Assemble, link, objdump |
| decomp-permuter | simonlindholm/decomp-permuter, cloned into `toolchain/` (ignored) with its own WSL venv | MIT | Tries random rewrites of one function and keeps those that compile closer to the game |

- **Pipeline:** `cpp` → `cc1 -O2 -G0` → `maspsx --aspsx-version=<per file>` → `as -march=r3000 -G0 -O1` → `ld` at
  the exact address → `objcopy -O binary`.
- **`tools/match.sh <file.c> <addr> [gcc]`** runs it and prints `MATCH` or diffs the disassembly against your exe.
  Per-file settings go in the environment: `ASPSX=2.21 MASPSXFLAGS=--expand-div tools/match.sh <file.c> <addr> 2.6.3`
  for a `sound.c` function. It links against `config/symbols.main*.txt`, splat's `build/undefined_*_auto.txt` and
  every symbol in the last full build, so a function that calls another or reads a global can be checked alone.
- **Link address:** `ld` aligns `.text` to 16 bytes, so link with an explicit section address and `SUBALIGN(4)`
  (`match.sh` does).
- **Preprocessor:** the period `cpp` is 32-bit and fails with "Value too large for defined data type" on any
  `#include` from a Windows drive under WSL (it can't `stat` the large inode numbers). The build uses the host
  `cpp -P -undef -nostdinc` instead, which doesn't change the generated code.
- **Permuter:** `tools/permuter.sh NAME SOURCE.c` sets up `build/perm/NAME`: the preprocessed C, the game's asm
  assembled as `target.o`, and a `compile.sh` with the file's flags (`DIR`, `GCC`, `ASPSX` and `MASPSXFLAGS` select
  them; the default is `sound.c`'s). Run it with the venv's `bin` on `PATH` (command in the script's header). Treat a
  find as a hint and rewrite it by hand in the file's style.
- **Full rebuild:** splat 0.50.1 splits `SCUS_941.63` per `config/main.yaml` and `make` relinks it, SHA1-identical to
  retail. All 47 boot functions, `bitops.c` and 231 of the sound driver's 232 functions are C; the rest is assembly.

**Main exe layout** (splat's guess, corrected by hand):

| File offset | VRAM | Contents |
|---|---|---|
| `0x0000` | n/a | PS-X EXE header (t_addr `0x80010000`, t_size `0x60800`, pc0 `0x800110C0`) |
| `0x0800`–`0x18B8` | `0x80010000` | read-only data (jump tables, strings such as `"battle.x"`) |
| `0x18B8`–`0x394FC` | `0x800110B8` | code. splat guessed `0x18AC`, but the 12 bytes there are the string `" (%08x)\n"` |
| `0x394FC`–`0x61000` | `0x80048CFC` | data. splat guessed `0x80048D18`, inside the sound files' disc table that opens it |

- **No padding between sections:** the retail link packs them back to back, so splat needs
  `ld_align_section_vram_end: False` and `ld_align_segment_vram_end: False`, or everything after the read-only data
  shifts by 8 bytes.
- **BSS size isn't set yet.** BSS isn't in the file, so it doesn't affect the hash.
- **A function's jump tables must sit in its `.rodata` subsegment** (`.rodata main/main` for `0x80010000`–`0x80010100`,
  a `.rodata` piece at `0x80010394` for `seq_look_ahead`'s `switch`), so splat moves each table into its function's
  `.s` file. Left in the plain `rodata` piece, the tables point at labels local to the C file and the link fails.

### Native build (the sound driver on the PC)

`make native` compiles `sound.c` and `sound_init.c` with the host GCC (64-bit, `-DNATIVE -DNON_MATCHING`) and links
them with `native/snd_native.c`: libspu and kernel stubs, a loader, and a song runner that prints each sequencer
tick's key-ons. `tools/native_check.py` plays songs on it and compares every channel's key-on ticks with
`tools/akao.py` (an F4 layer or F8 overlap voice counts as its channel's, since `akao.py` reports per channel). Songs 2
and 3 (`FIELD/MD1STIN.DAT`) match on every channel over 4000 ticks; a whole-disc run (every field file decompressed in
Python first) is slow and unfinished.

Rules for C that builds both ways and still matches:
- **A value that can hold an address is `long`** (`SoundCommand.arg`): 32 bits on the PS1, pointer-sized on the PC.
  `int` truncates the song and effect pointers.
- **The data stays in the user's exe.** The runner loads `SCUS_941.63` into `snd_ram` (2 MB standing for
  `0x80000000`), and `tools/native_syms.py` writes a linker script placing every data symbol the driver uses at its
  game address there. Code that treats neighbouring globals as one block (`seq_song` copied into `seq_song2`, the
  twelve words of effect state from `effect_voices`) works unchanged, and no game table enters the repo.
- **Objects that hold pointers are real C definitions** in `snd_native.c` (the channels, the command queue and
  request, the two handler tables, `lfo_waveforms`, the stream and effect-file pointers): a PC pointer doesn't fit in
  4 bytes of `snd_ram`. The game's address tables are translated at start-up via `funcs.c` (also from
  `native_syms.py`), which gives each driver function's game address.
- **Structs with pointers grow on the PC** (`SeqChannel` isn't `0x108` bytes there). The driver copies them as
  `sizeof(...) / 4` words, which still works.
- **A function still in assembly needs its C under `#ifdef NON_MATCHING`** (`snd_start_lasting_effect`); `NATIVE`
  turns `INCLUDE_ASM` off.
- **`-G8` tentative definitions stay common** (`-fcommon`), so they resolve into `snd_ram` as they resolve to the data
  section's labels on the PS1. The link is `-no-pie`, since those addresses are absolute.

### Native build: the CD layer

The PC build (`make pc`, below) compiles `cd.c` over `pc/cd_native.c`, a libcd that reads the extracted discs:
- **By sector, as the game does:** the exe's file tables give sectors, and `load_disc_table` reads sector `0x266`
  (`INIT/YAMADA.BIN`, 80 bytes, read as a whole sector). `files.txt` maps each sector to a file; past a file's end and
  outside every file a sector reads as zeros. Raw movie files give each sector's 2048 data bytes.
- **No disc swapping:** the drive always holds the disc the save is on (`0x8009D588`, disc 1 before there's a save),
  so `\MINT\DISKINFO.CNF` always matches it and `main` never loads `FIELD/DSCHANGE.X` for a disc change (it still
  does for the game over screen, which lives there too). `cd.c` and `main.c` are
  unchanged; the PS1 build keeps its disc check.
- Reads finish at once and the drive is always ready, so `cd.c`'s retry paths don't run. The hand-written streaming
  decoder (`lzs_stream_start`/`lzs_stream_resume`) has a C version there: 9 sectors of `lzs_stream_src` per call,
  leaving `lzs_stream_src` where it stopped. `cd.c` points it at each new read; `ENDING.X` unpacks `MOVIE/OPENING.BIN`'s
  pictures from memory a frame at a time and relies on the saved position (without it the opening's background broke
  up after its first 9 sectors).
- The movie player's hooks do nothing on the PC yet: playing a movie also needs libpress and a renderer.
- `build/pc/test_cd` (`make pc-test`) checks it: on each disc as the save's, the disc check passes and reports that disc; all
  10,052 files read back byte for byte through `cd_read`/`cd_step`; and the 8,143 files that start with their packed
  length unpack the same streamed (`cd_read_lzs`), streamed from memory, and in memory (`lzs_decode`).
- Two header fixes it needed don't change the PS1 code: `snd_submit_request` has a prototype in `sound.h`, and
  `CdSearchFile` takes a `const char *`. `cd_read_disc_number`'s switch casts the found file to `long`, not `int`.

### PC build: the exe runs (milestone 1)

`make pc` builds `build/pc/ff7.exe`, a 32-bit Windows program (MinGW i686, called from WSL) that runs the whole boot
exe: the C from `src/main`, and every function still in assembly recompiled to C from the user's own `SCUS_941.63` by
`tools/recomp.py`. It boots through `ENDING.X`'s opening credits and the logo's movie (`OPENMV.BIN`, around frame
9000) to the title logo, on Start to the title menu (NEW GAME / Continue?, the menu modules at `0x801D0000`), and on
NEW GAME into the field module, which loads `MD1STIN` and plays the opening movie (`OPENINGE.MOV`, streamed).
`pc/sdl.c` shows it in an SDL2 window (640×480 at 4:3; the keyboard is pad 1; F12 saves `screenshot.bmp`), or
`ff7 [ASSETS] --frames N [--shot FILE.bmp]` runs without one and saves frame N.
- **Memory:** the PS1's RAM sits at its own address, `0x80000000` (with the 64 KB after it, for the stack words a
  call from the top of the stack reads), and the scratchpad at `0x1F800000`; the I/O window `0x1F801000`–`0x1F802FFF`
  goes to `pc/hw.c`. Of RAM's mirrors, `0xA0000000` folds back, and so does KUSEG's 2 MB from 0 (the field reads
  through a null pointer at the start of `MD1STIN`; the bike and the Speed Square store through 24-bit addresses):
  the host's main stack and image sit above it, and `mem_map` keeps Windows from handing out what's free below 2 MB.
  The C runtime's heap is made before `mem_map` and can sit there (under `make pc-test` a `malloc` came back at
  `0x001F4FC8`, which recompiled code then read as `0x801F4FC8`), so a buffer handed to game code is static.
  Game pointers need no translation, and C and recompiled code share the game's data. The link is
  `--large-address-aware --disable-dynamicbase`, and `tools/pc_syms.py` places each game symbol at its address.
- **Recompiler rules:** one C function per game function over a register context (`pc/recomp.h`); delay slots
  folded; a backward branch polls the hardware. Each function has a C-callable thunk taking eight ints (the last four
  are the PS1's stack words). Code pointers are native: a `lui`/`addiu` pair that builds a function's address gives
  the native function only when the address escapes as a code pointer (stored, passed, returned or called; libgte's
  `_patch_gte` uses one as a loop's end), and the code addresses in the game's data are patched at start: every
  aligned word in a data file's range that holds a function's address, whatever type splat gave it. (A `.word func`
  parse missed most of `snd_command_handlers`: `seq_cmd_scan_sizes` and `seq_cmd_sizes` are named at their C base,
  0xA0 entries before their tables, which puts a label inside the handler table, and splat emits the rest of it as
  bytes. Sound command 0xF0 then called a game address natively.) A jump table splat ends the same way is read on
  from the image while its words land in the switch's function: `BATTLE.X`'s at `0x800A0220` has 22 entries, but
  `D_800A0240` cut it at 8, so entry 11 (`0x800A5454`, an actor script byte of 13) left `func_800A5250` without its
  epilogue, and the caller's saved registers came back wrong: a world battle with Vincent jumped to 0. A game
  address that still turns up is looked up in the code tables. The point after a `jal setjmp` is an entry of its own:
  libetc's interrupt entry resumes there, through the BIOS's custom exit.
- **Modules:** the Makefile's `PC_MODULES` lists each with its file and load address (`ENDING.X` and the field's
  `FIELD.BIN` at `0x800A0000`, the 13 `MENU/*.MNU` at `0x801D0000`, all the same on every disc; `FIELD.BIN` is
  gzipped behind an 8-byte header, so the list names `FIELD.BIN.dec`, which `tools/unpack.py` makes). `make split`
  clears each `asm/<name>` (a stale file from an earlier split gets read as more functions) and splits it with a
  splat config from `tools/module_yaml.py`: a module is read-only data, then code from its first function to the last
  `jr $ra`, then data. The first function starts at its stack-frame setup or at the instructions GCC scheduled before
  it (two loads, in the field), which gives `ENDING.X`'s hand-found bounds exactly. `recomp.py` takes a module's functions
  from splat's `glabel`s, and its calls into the exe by the exe's symbols. Modules share load addresses, so a
  module's functions stay private to its file. `module_function` (`pc/dispatch.c`) picks the module whose first
  function's opening 64 bytes are in RAM and whose code has the address (a module at another address can still be
  in RAM), and patches its data's code pointers that still hold game addresses.
- **Code behind a BIOS stub:** splat runs a three-instruction stub (`jr $t2`) on into whatever follows when that has
  no symbol, so the function after it is never recompiled ("reached … which wasn't recompiled"). `format`,
  `_new_card` and `StopCARD` hid one each; they now have symbols.
- **What's emulated:** the BIOS calls the exe makes (56, checked by `tools/test_bios_coverage.py`) and the interrupt
  path (events, root counters, the custom exit; an event opened `EvMdINTR`, 0x1000, calls its function, and one
  opened `EvMdNOINTR`, 0x2000, is marked ready for `TestEvent`: the title menu waits that way on the memory cards,
  which answer "no card"); DMA, timers and vblank; a software GPU (VRAM, every primitive,
  textures, semi-transparency, masks; in 480i the field on screen changes each vblank, and `ENDING.X` uploads the other
  one line by line); the GTE (a command reads all its inputs before it writes any: OP and MVMVA on IR overwrite the
  IR they read, and a zero walkmesh normal from OP hit GCC's divide-by-zero `break` when the opening movie ends); the
  MDEC (`pc/mdec.c`, dequantizing as `tools/movie.py` does, so the in-memory movies
  run through libpress and `movie.c` as they are). Time is counted in polls of 32 R3000 cycles, about a short loop's pass, because the
  libraries' spin timeouts count passes (libetc's `VSync` gives up after 32768, ~1.7 frames) and recompiled code runs
  them far faster; after each vblank the host clock paces it to 59.94 Hz.
- **Waits in C:** the hardware moves only when something polls it. Recompiled loops do at every backward branch;
  decompiled C doesn't, so a C loop that waits on a flag only an interrupt changes has `HW_WAIT()` (`common.h`) as
  its body, which is `hw_poll()` under `NATIVE` and nothing on the PS1 (`snd_transfer_wait` hung the boot at frame
  ~680 without it; `movie_wait_frame` waits on the MDEC's last column the same way). `make pc-test` boots 900 frames
  to catch the next one.
- **Ports:** a register that's a port (MDEC0/1, GP0/1) takes a write as it is: merging a partial write with a read
  of it would take its data.
- **Streamed movies:** libcd's streaming is C in `pc/cd_native.c`. `CdRead2` starts reading the STR sectors from the
  drive's position at 150 a second of PS1 time; `StGetNext` waits for a frame's last sector and hands out its video
  sectors' data joined after the first one's header, in the ring `movie.c` gave `StSetRing`. libpress, the MDEC and
  `movie.c` run as they are, camera data included. The XA audio sectors among them are decoded there (FF7's are stereo
  37.8 kHz 4-bit; mono, 18.9 kHz and 8-bit decode too), through `CdMix`'s matrix, into the SPU's CD input.
  As libcd's ring does, it counts only video sectors: whatever else comes in is passed over as it arrives, and a
  `StGetNext` with no frame started lets a poll's time pass, since `movie.c` retries it up to 0x800000 times. A
  stream with no movie in it then waits, as on the PS1, instead of reading ahead through the disc (see "Debug
  rooms").
- **Sound:** the SPU is `pc/spu.c`: 24 voices of ADPCM from its RAM with ADSR, volume sweeps, pitch modulation and
  noise, the 4-point interpolation, the interrupt at an address, the reverb on its work area, and the CD input, mixed
  at 44.1 kHz of PS1 time as `hw_poll` advances it, then a ring that SDL's audio thread reads. The opening movie's
  sound comes out as `tools/movie.py` decodes it, with no drift; from about 50 s in, the field's own sound plays
  along with it.
  - The interpolation's weights and the low-pass around the reverb are the player's curves ("Interpolation", and
    "In the player" under "Reverb"), not the hardware's tables. The reverb runs psx-spx's formula a sample at a time
    at 22.05 kHz, writing its work area only while SPUCNT's bit 7 is on. A click through each of the game's nine
    presets matches the player's reverb (correlation 0.9997 or better, peaks within 0.2%). The credits play with
    Studio B at output volume `0x2800`, the first field at `0x3200`.
  - The interrupt goes off when a voice decodes, or a transfer writes, the block holding the IRQ address, or the
    reverb reads or writes it (psx-spx doesn't know which of its accesses count, so all do); and again only after
    SPUCNT's bit 6 goes off and on. The driver streams samples through it (`snd_stream_start`): each half of the
    buffer at `0x77000` is refilled when the voice reaches the other.
  - A sweep's negative phase heads for `-0x7FFF`, except in an exponential decrease, which it leaves alone. psx-spx
    marks both as untested guesses; the game writes no sweeps up to the first field.
  - Left out: the capture buffers (the SPU writing the CD input and voices 1 and 3 into RAM `0x000`–`0xFFF`, with
    their interrupt). The exe has no libspu reader for them. If streamed sound (XA, or the sample streams) ever
    misbehaves, check first whether a module reads that RAM or sets the IRQ address below `0x1000`.
- **Reversing lab** (`pc/lab.c`; the options are in the README): a trace of the game frame by frame, the pad from a
  script, a warp to a field, and dumps of RAM, VRAM and SPU RAM.
  - The warp asks as a gateway does (`D_8009ABF5` = 1, `D_8009ABF6` = the field) whenever `game_mode` is 1 (in a
    field) and nothing is asked for, until the field is the one wanted: a field setting up clears what's asked.
    `--field 0x41` from NEW GAME lands in the debug room 21 frames after the first field loads, before the
    opening movie is asked for.
  - What the traces showed: a field loads its `.MIM` (background), `.DAT` and `.BSX`, then each character's `.BCX`
    and `FIELD.TDB`. The opening field `MD1STIN` loads `CLOUD.BCX` and `BALLET.BCX`; the debug room (`STARTMAP`)
    `CLOUD.BCX`, `TIFA.BCX` and `CID.BCX`, then `BLACKBG3`, `5` and `1.MIM` as it runs. The opening movie is
    request 3 while asked for and 4 while it plays.
  - The PC build is deterministic: two runs with one script, dumped at one frame, differ only in two words
    (`0x8005141C`, `0x8005143C`, host addresses). So runs that differ by one button press, diffed with
    `tools/lab_diff.py` (`--noise` takes a second dump of the same script), show what that press did.
- **Pad input** (exe):
  - `func_8001C434` gives the BIOS the pad buffers `D_800696AC` (port 1) and `D_800696CE` (port 2).
    `func_8001C808` reads both as one word, active high, in Psy-Q's order: L2 `0x1`, R2 `0x2`, L1 `0x4`, R1 `0x8`,
    △ `0x10`, ○ `0x20`, ✕ `0x40`, □ `0x80`, Select `0x100`, Start `0x800`, Up `0x1000`, Right `0x2000`,
    Down `0x4000`, Left `0x8000`; port 2 in the high half. A pad that isn't a digital pad (`0x41`) reads as nothing.
  - `func_8001CB48`, once a frame, keeps port 1 as held `D_80062D78`, last frame's `D_80062D7A`, pressed
    `D_80062D7C`, and pressed-or-repeating `D_80062D7E` (repeats after 16 frames held, every 4); port 2 in
    `D_80062D80`–`D_80062D86`. With the Config menu's custom layout on (`(D_8009D7BE >> 2) & 3`), port 1's bits
    go through the table `D_8009D7C0` first (bit n becomes bit `table[n]`).
  - `func_8001C980`, which the battle calls, keeps a second set from those: `D_80062D88` held, `D_80062D8C`
    pressed, `D_80062D8E` pressed-or-repeating (after 8 frames, every 2); port 2 in `D_80062D90`–`D_80062D96`.
- **Battle controls**, each one press (or hold) on Cloud's command menu in the first battle, diffed in the lab:
  - Start (`D_80062D7C & 0x800`, battle state `D_80163C7C` = 4): toggles the pause flag `D_80062D99` and plays
    sound 0x99 (paused) or 0x98.
  - Select: toggles the help window, which is a setting: bit 14 of the config word `D_8009D7BE`. The help line goes
    to `0x80063560` in the game's text code (ASCII − 0x20), "Attack…" for the Attack command.
  - R2 (`func_800DE618`): toggles bit `0x40` of `0x8009D302`, the target cursor.
  - □ held (`D_80062D78 & 0x80`, `func_800E0E34`, `func_800E2098`): `0x800F99E4` goes to 1 and the command window
    gives way to the Barrier gauges.
  - L1+R1 held (`D_80062D7E & 0xC`, `func_800E7170`): tries to run; the first battle can't be fled, and the
    message buffer gets "Can't escape!".
  - △ (`func_800E0E34`, newly pressed `0x10`) and R1 changed nothing here: going by the US manual, △ passes the
    turn to the next ready member and R1 switches single and all targets while one is chosen, and only Cloud is on
    the menu. R2's flag is the manual's target cursor by the same reading; neither is checked further yet.
- **Battle** (`BATTLE.X`, `BATINI.X`, `BATRES.X` in `PC_MODULES`):
  - A module file's 8-byte header is its unpacked size and a BSS size, which `func_80015CA0` zeroes after the image.
    `BATRES.X` ends on a `jr $ra` whose delay slot is that BSS.
  - `BATTLE.X` runs at `0x800A0000` from `func_800A1158` (`run_overlay(5, …)`); `loader.c` called it and the file-3
    entry by `D_` names, which the PC link took as plain addresses, so they're `func_` names now, given to the PS1 link
    by `config/module_entries.ld` (the module splits read `symbols.main.txt`, and would cut their code there).
  - `BATINI.X` runs at `0x801B0000`, read straight after `BATTLE.X`: a table of pointers, then code from `0x50`.
  - `BATRES.X` (the results screen) runs at `0x801B0000` too: when the battle ends `BATTLE.X` reads it to
    `0x801C0000`, unpacks it to `0x801B0000` and calls `func_801B0000` (at `0x800A1700`).
  - The first battle (`0x12C`, the guards on the opening platform) reads `BATTLE.X`, `BATINI.X`, `SCENE.BIN` from
    sector 36, `STAGE1/CAMDAT0.LZS` (the battle cameras) and `STAGE1/STAGE09.LZS` (the stage).
  - `BATTLE.X`'s hand-written renderer (`func_800D29D4`) keeps a resume address in `$at` (`lui`/`ori` of its own
    label) and calls helpers (`func_800D32B4`, `func_800D3354`) that leave by `jr $at` into it. `recomp.py` makes
    such addresses labels; a `jr` to no function leaves it in `ctx.pending`, which the caller takes after the call.
  - That renderer saves its registers below `$sp` without moving it. The BIOS runs root-counter callbacks on its own
    stack, so `irq_check` does too (`KERNEL_STACK`, in the kernel's untouched 64 KB): run on the game's stack, one
    clobbered the stage switch's `$s1` and the next model drawn was the stage's texture.
  - A stage file (`STAGE09.LZS`, at `0x801590E4`): see §2, a battle as `BATTLE.X` sets it up.
  - Spells and attacks are effect files: `MAGIC/*.BIN` (289, the same on all three discs; the 29 `.LZS` beside them
    are summon data), each loaded at `0x801B0000` when used. A header of 0 to 0x8C bytes (pointers into the file,
    sizes, colours) comes first, then the code (0x300 to 0xEB00 bytes, 1.8 MB in all), then models and textures.
    `BATTLE.X` enters one through its table `D_800EFAF0` (54 entries: `0x801B0054`, `0x801B037C` for Ice, ...), and
    an entry can be a leaf with no stack frame (`AQUA.BIN`'s at 0), so `module_yaml.py` walks back from the first
    stack-frame setup over anything GCC could have emitted.
  - All of them are in `PC_MODULES` as `magic_<name>`. Eight groups open with the same 64 bytes (a call to a shared
    `BATTLE.X` helper) and differ later, so modules at one address with one signature are told apart by an FNV-1a
    hash of their code (`code_hash`).
  - With those, the first battle plays through: Cloud's Ice twice (`BRIZAD.BIN`), the results (`BATRES.X`), then
    back to `MD1STIN` in the field, driven by a lab `--input` script that walks north tapping circle.
- **Field menu** (exe, `func_80023xxx`): `0x8009D2A4` is which of its ten entries show (bit n for Item, Magic,
  Materia, Equip, Status, Order, Limit, Config, PHS, Save) and `0x8009D2A6` which are **locked** (a set bit greys the
  entry and ○ skips it); PHS is bit 8 and Save bit 9, which the world map clears as it starts (under a condition not
  traced yet) and sets again as it leaves (`func_800B7620`); save points are not traced yet. The labels are at
  `0x80049248`, 12 bytes each.
  - Opening it ran the main loop's `while (D_80095DD4) ;` (mode 5), a wait for the VSync callback: in C on the PC
    nothing let the interrupt in, so it hung. Both such waits in `main.c` now poll (`HW_WAIT()`, empty on the PS1).
  - Opening it reads `LIMTMENU` to `0x801D0000`; each entry then reads its own module there (`ITEMMENU`,
    `MGICMENU`, `EQIPMENU` for Materia and Equip, `STATMENU`, `LIMTMENU` again for Limit, `CNFGMENU`, `FORMMENU`
    and every portrait for PHS, `SAVEMENU`); Order needs none. Checked in the lab, from
    a new game, after the guard's first line (menu allowed from frame ~11200): all ten
    open and draw; Ether on Cloud fills his MP and leaves the list; taking Lightning off undoes its MaxHP -2% and
    MaxMP +2%; Item's Arrange list, Materia's Check, Limit's Set and the field after closing the menu all work.
  - Config's controller Customize writes the remap table `D_8009D7C0` (16 bytes, one per pad bit, each the bit it
    stands for; ○ and ✕ swapped is `05`↔`06` at entries 5 and 6), and the pad reader applies it at once: after the
    swap, ○ backs out of Config and ✕ opens it. The game won't leave Customize while a function has no button.
- **World map** (`WORLD/WORLD.BIN`, packed; runs at `0x800A0000`, entered at `func_800A460C`): a field ID below
  `STARTMAP` (`--field 1`) asks for it. It reads `WM0.TXZ`, `WM0.BOT` and `WM0.BSZ`, then streams `WM0.MAP` in blocks
  as the party walks (§2, `WORLD/WM*.MAP`).
  - It was linked from two or more objects: data sits between their code (`0x800BD130`-`0x800BFBF0`), so
    `module_yaml.py` ends a run of code at a word no R3000 runs (from the last return before it), and `recomp.py` ends
    a function where its asm file's code ends.
  - Its loop runs while `D_800E566C` is below 6; `func_800B7620` then hands the main loop the exit (`D_8011626C`: 1 is
    a battle, read by `func_800112E8`), the battle and the field asked for.
  - Random battles (`func_800B832C`, each frame): none while `D_8009AC2F` is set (the field reads it the same way; a
    field script sets it, and the opening field leaves it set, so the lab holds `--code "3009AC2F 0000"` after a warp
    from there). Otherwise every 16 frames of walking `func_800B7C7C` picks a formation or -1, and `func_800A3F4C`
    starts it. Near Midgar it's two wolves on stage 02 (`ENEMY074`/`075`).
  - Its menu (`func_80024A3C`, called by the world itself) reloads the portraits only when `D_80071E34` is set, as the
    main loop does; after a warp the slot still holds the new game's last preload (Barret's face over Cloud's stats).
  - After a lost battle, `DSCHANGE.X` shows the game over screen (`MOVIE/OVER.LZS`, 640x480, `MOVIE/OVER2.SND`), then
    the title.
  - Checked in the lab against the manual: START cycles the map (small, large, none), L1/R1 turn, L2/R2 change the
    view, △ opens the menu ("Midgar Area"); escaping (L1+R1) returns to the same spot; walking north at Midgar enters
    field 0xAA (`MDS5_5`). Swinging to the low view, a dark polygon crosses the screen: the game's own terrain, a
    big triangle near the camera (it's in the game's ordering table, and `tools/world.py` draws it the same).
- **Mini-games** (`MINI/*.BIN`, packed, each run at `0x800A0000`): a field asks for one with request `0xC`, the field
  to come back to as the request's ID, and `0x8009ACE6` saying which (0-6). The field module turns that into a main
  loop mode, and the main loop reads the file through its table at `0x80048D34` and calls the entry:

  | # | Mode | File | Entry | Game |
  |---|---|---|---|---|
  | 0 | 6 | `HIGHWAY.BIN` | `func_800A00D0` | the motorcycle chase |
  | 1 | 7 | `CHOCOBO.BIN` | `func_800A02D0` | the chocobo races |
  | 2 | 8 | `SNOBO.BIN` | `func_800A0390` | the snowboard down to the Great Glacier |
  | 3 | 9 | `CONDOR.BIN` | `func_800B6B58` | Fort Condor |
  | 4 | 10 | `SUBMAR.BIN` | `func_800A00BC` | the submarine (its result to `0x8009D5E6`) |
  | 5 | 11 | `JET.BIN` | `func_800A0450` | the Speed Square (its score to `0x8009D3EA`) |
  | 6 | 14 | `SNOBO2.BIN` | `func_800A0448` | the Gold Saucer snowboard |

  - In the lab, `--minigame N` asks for one from the field that's up. All seven start from the opening field and draw;
    the chocobo bet (○, 100 gil), Fort Condor's unit placing, the submarine (dive, speed, the 10-minute timer), the
    Speed Square (○ fires; a ride scores and hands back to the field) and the bike's swings answer the pad. The
    snowboards wait at the top until Up.
  - `JET.BIN` ends with data and then hand-written GTE routines with no stack frame, so after data with no stack-frame
    setup behind it `module_yaml.py` resumes the code at the first word that can be code.
  - `HIGHWAY.BIN` and `JET.BIN` build GPU lists through 24-bit addresses (`0x000C9D18`): KUSEG, which the PS1 maps onto
    RAM over all 2 MB. `mem_ptr` folds all of it now, not just the first 64 KB, and `mem_map` reserves the free blocks
    there on Windows (which hands out the lowest free addresses first) so no host pointer can ever be one.
- **Field numbers:** the `.DAT` files of `FIELD/` lie on the disc in field order: by sector, `STARTMAP` (the debug
  room) is 0x41 and the rest follow (`MD1STIN` 0x74, `MD1_1` 0x75, ...; `WM*` before it are the world map's). The
  debug room's side rooms are `BLACKBG1`-`BLACKBGK` (0x5D-0x70); Takashi Tokita's, with the save point, is
  `BLACKBGI` (0x6E). Warped into directly, the side rooms leave Cloud unable to move, and the woman by the door
  reopens her warp list while he touches her.
- **Memory cards** (`pc/card.c`): `memcard1.mcd` and `memcard2.mcd` in the working directory, raw 128 KB images as
  emulators keep them; a missing one is a formatted empty card. The BIOS's file functions on `bu00:`/`bu10:` work
  on them in whole sectors, `_card_info` and `_card_load` answer at once (`F4000001h`: 4 done, 2000h unformatted),
  and every change goes to a temporary file that then replaces the image.
  - The save menu (`SAVEMENU.MNU`) keeps all card access: `func_801D19C4` opens four software and four hardware card
    events and calls `_bu_init` and `_card_auto`; a check is `_card_info` then a wait on the four (a new card gets
    `_card_clear` and `_card_load`); `func_801D1C2C` lists `bu00:*` for `BASCUS-94163FF7-S01`-`S15`; `func_801D1D40` opens one with
    mode 1 and reads 0x280 bytes, `func_801D1F40` the same way, each retrying a short read like the write below.
  - Saving (`func_801D2408`): if the file opens it's deleted; then created (mode `0x200` with the block count in
    bits 16-31), closed, reopened for writing (mode 2) and written at once, 0x2000 bytes. It retries each open up to
    30 times, and a short write by writing the remaining length again **from the start of its buffer**, not from
    where the write stopped. A card that took a partial write would get the save's opening bytes where its end
    belongs, and since the old file is deleted first, a failed save loses the old one too: candidates for the
    corrupted saves reported on real cards. Here writes never come up short.
  - Checked in the lab: Save from the opening field (`--code "8009D2A4 03FF" --code "8009D2A6 0000"`), slot 1, file
    01 makes a one-block `BASCUS-94163FF7-S01` (title frame `SC`, icon flag 11h, "ＦＦ７／ＳＡＶＥ０１"); Continue
    on the title loads it back into `MD1STIN`.
  - A save's block is the 0x200-byte title frame and icons, then the game's data, which is RAM from `0x8009C6E4`
    (the menu masks at `+0xBC0`, the field at `+0xB96` = `0x8009D27A`, the disc at `+0xEA4` = `0x8009D588`). Its
    opening bytes are what the file list shows: the leader's level at `+4`, the party's portraits at `+5`, the
    leader's name at `+8`, HP and MP at `+0x18`, gil at `+0x20`, seconds played at `+0x24`, the place at `+0x28`.
  - Cards from emulators (ePSXe's `.mcr`) are the same raw images and load as they are, renamed `memcard1.mcd`. In
    the lab, Continue is: the credits skipped (up 2040, ○ 2100, START 2160), then ○ at 2390 (the title's cursor
    starts on Continue when the card has a save), ○ at 2520 (slot 1), ○ at 2660 (file 01); the game is up by 2692.
  - Swept with a playthrough of 57 emulator saves, one per story stop on all three discs: each loads into its field or
    onto the world map (all three world maps, the Highwind and a chocobo among them) and draws. Then 9200 frames of
    turning and ○ every half second walks 18 world saves into fields and opens dialogue and the snowboard's
    instructions, and 600-frame straight walks bring on 28 battles (fields and world map, spells and summons cast, six
    of them over, one a loss through the game over screen) with no crash once the jump table above is read whole. A
    random walk on the world map meets few battles: it turns before it covers ground.
- **Not yet:** waiting for the host's clock spins a core.
- `make pc-test` runs the recompiler on hand-assembled fixtures and C tests of the memory map, the C/recompiled
  bridge, the BIOS, modules, the hardware registers, MDEC, SPU, GPU, GTE, display, the CD layer and the memory cards.

### Matching rules for GCC 2.6.3/2.7.2 + maspsx

**Loop invariants (GCC 2.6.3's loop optimizer).** It moves a loop-invariant register out of a loop when
`threshold × savings × lifetime ≥ insn_count`:
- `threshold` is 26 here (measured) and drops by 3 for every register already moved; `insn_count` doubles for each
  register already moved out of an inner loop.
- `savings` counts the temporaries holding the same value; `lifetime` is in luids, and **every note that isn't a line
  number takes a luid** (deleted insns, loop and block notes).
- `-dL` prints each decision: compile the C and read the dump to see why an address was or wasn't hoisted.
- So a match can hinge on shape, not statements (see `main` below): a user variable deriving from an address
  lengthens its life and gets it hoisted; **a call without a prototype** makes GCC 2.6.3 put an implicit declaration
  in that block, which keeps the block's notes and lengthens lifetimes.

**Loop rotation.** GCC rotates a loop that starts with its exit test. To keep the game's unrotated form:
- make the test the `for` condition (`F8`), put it in its own block with a declaration (`F4`), use `goto`
  (`highest_bit`), or put the test inside a `for (;;)` with the `return` there (`seq_stream_end`);
- `seq_step_channel`'s command loop is a plain `do`/`while`.

**Register-held addresses.** GCC 2.6.3 reaches a plain scalar global with `lui`/`%lo`, but loads the address of a
struct field or a pointer expression (`*(T *)&x`) into a register, and folds later accesses to the same object back
to `lui`/`%lo`: within a function, the first one touched is the one in a register. In a loop those registers can be
hoisted. To steer it:
- Want the address in a register? Reach it through a pointer (`F8`/`F9`'s `effect_voices`) or a struct, ordered so
  the right field comes first (the save-data reset writes the disc number first).
- Don't want it? Use a plain global alias: `seq_song_flags` (`0x8009A13C`, `seq_song`'s `+0x38`) and the voice masks
  (`+0x04`, `+0x24`, `+0x28`) are reached as plain variables, since as struct members GCC computes the address once
  and the game doesn't. `VoiceSettings`' fields likewise each have an alias (a register holds the struct's address
  while the stores use their own).
- One symbol or two: GCC reaches one object's fields as offsets from another field's address only within one symbol.
  `snd_slow_slides` does that across the two song blocks, so `seq_songs[2]` (the same storage as `seq_song` and
  `seq_song2`) models them; the rest of `sound.c` uses the two names.
- An address the game computes once before a branch goes in a local there (`&seq_song` in sound command `14`).
- A plain global whose address the game holds for a load and a later store is reached through a pointer local: the
  CD layer's frame and retry counters (`int *last = &cd_last_vsync`), `&cd_request` in `cd_step`.
- Many of `main.c`'s older `*(volatile T *)&x` casts are probably this rule in disguise.

**Address forms of a big constant offset** (`transition.c`'s `ShardState` at `0x8019C000`):
- as a struct field (`shard_state->screen[i][j].vx`) GCC folds the whole address into one constant
  (`lhu v0, -2145789780(v1)`), which maspsx expands register first (`lui at; addu at, v1, at`), as in the game;
- through an extern symbol it expands as `addu at, at, v1` (8 words off);
- through a bare constant pointer (`((DVECTOR (*)[8])0x8019D8AC)[i][j]`) GCC keeps the base in a register for the
  offset-0 field and strength-reduces the loop. Where the game does show that register (`a3` for `.vx` in the setup's
  vertex grid), decay the field to a pointer first: `((SVECTOR (*)[8])shard_state->verts)[i][j]`.

**Aliasing.** GCC 2.x assumes a struct field (reached through an index or a pointer) can't alias a fixed scalar
global, so it moves accesses past each other:
- Where the game keeps the order, go through a plain pointer: `A0` reads the overlap voice and clears the channel flags
  through the same `unsigned int *` it used for `effect_voices` (through `ch->` the update mask loads early and the
  voice bit leaves `a2`); `seq_channel_slides`' noise flip (else the flags load rises above the store); `seq_tick`'s
  gate through an `unsigned short *` (else the mask loads rise above the gate store).
- Where the game reorders, use the field: `snd_resume_song`'s copies target `seq_song`'s fields, not the aliases.
- The opposite case: as separate globals, the save function lets GCC move a table load above the stores, which it
  doesn't do past a struct field, so the save-data block is one struct.
- `snd_submit_request` reads `snd_request.arg[n]`: the game reloads the fields after each queue store.

**`volatile`.** A `short` that the game loads with `lhu` and sign-extends with shifts, reloads after storing, and
reads in order (even twice) is `volatile` (probably shared with the VSync callback). `x++` on a volatile reloads `x`
after the store and discards it, as the game does. Applies to `game_mode`, `frame`, the fade level and step
(`0x8009AC42`/`0x8009AC44`), the save-data block, and `snd_transfer_busy` (the wait loop reloads it).

**Types and variable sharing** decide registers and shifts:
- One variable for two values: `snd_effect_pitch_shift_slide` (the length argument, then each channel's distance);
  `seq_tick` (the tempo scale and the phase); the bank loaders (the copy counter is the variable that held the transfer
  size, and the header pointer is stepped to the sample data before the call; a fresh counter or `header + 3` in the
  call gives `a0`/`v1` instead of `v1`/`a1`); `snd_start_effect` (the shifted pan goes back into the parameter; a
  separate local swaps the saved registers of the pan and the second stream).
- Exact types: `snd_start_effect`'s pan parameter is `unsigned short` (every caller casts); `vibrato_amount` (`+0x7C`)
  is a signed `short`, for the arithmetic shift; `B4`'s `amount` is `unsigned short`; `seq_cmd_octave_down` adds an
  `unsigned short` −1 (`0xFFFF`); `seq_step_channel`'s octave is an `unsigned short` filled from a byte, and it reads
  the look-ahead's result into the pitch variable; in `func_80014750` the entry size is an `int` taking an
  `unsigned short` result; `seq_channel_lfo` reads the master volume's top half through its own `unsigned short`
  symbol.

**Statement order** at function ends and in expressions:
- In `A1` and `F2` the voice-update `|=` comes after the last field copy.
- The SPU voice masks are one expression with the "free voices" mask repeated as the last argument; a local computed
  first schedules differently.
- `seq_tick`'s song 2 loop setup follows the game's order, or a register or the bit's constant moves.
- `snd_slow_slides`: slides decrement with `--`, the fade's step divides by `snd_fade_ticks` itself, and the pointer
  to song 2's masks is set at the top of each loop pass.

**`-G8` files** (`sound_init.c`, `loader.c`):
- maspsx emits `$gp` accesses only for small variables the same file defines, so the file gives them tentative
  definitions and `--use-comm-section` keeps them common: the linker resolves them to the data section's labels and
  adds no storage.
- GCC writes every function after the file's top-level `asm`, so a plain `INCLUDE_ASM` lands first. maspsx's
  workaround is each include inside a `__maspsx_include_asm_hack_` function (neither file needs it now).
  `-mno-gpopt` doesn't help: GCC forces it on when optimizing with `-G`.

**Switches.**
- A jump table runs to the highest listed case: `cd_cancel` lists states that do what `default` does, because the
  game's table runs to 18.
- `case -1: case 0:` compiles to a range check (`bgtz`, then `slti -1`): `cd_read_disc_number`'s `CdSearchFile` test.
- Code that falls into a function's final `return 0` must not end in its own return: `cd_check_disc` uses
  `break`-style switches and lets the success path fall out of the `if`. Likewise `movie_open`'s in-memory case ends
  in `break` and the function's `return buf`.
- Three cases in a row with no jump table compile to two `sltiu`s, upper bound first: `movie_stop`'s states 8–10.
  `x >= 8 && x < 11` instead folds to one range check.
- Identical stores ending both arms of an `if` are merged after them: `movie_next_frame` sets the frame's bottom edge
  in each arm, so the height loads twice.

**Inlining.** A non-static `inline` function is emitted and also expanded into later callers in the file, each copy
with its own locals: `movie_stop`, twice in `movie_play_frame` (its `RECT` sits above the caller's `CdlLOC`). To test
a caller alone, give the callee as `extern inline` in the prelude.

**Rounding up.** `(n + 3) / 4 * 4` on an unsigned gives `srl`/`sll`; `(n + 3) & ~3` gives a `li -4` and `and`.

**`.rodata` packing.** The link packs each file's `.rodata` back to back (`SUBALIGN(2)`), so a file whose last string
ends off a word boundary shifts everything after it. Size the string to the game's padded length:
`diskinfo_path[24]` in `cd.c`.

**Counting:** count recovered functions with a parser, not by eye; names with upper-case hex are easy to miss.

### Boot code (`0x8001117C`–`0x80014A00`, 47 functions, all C)

Before it sit the hand-written entry at `0x800110C0` (clears BSS, sets `$sp`/`$gp`, calls the main loop; asm segment
`18B8`) and `__main` (empty: GCC calls it first thing in `main`). `include/main.h` holds what the files share.

- **`main`** (`0x80011C1C`, 3 KB): sets up memory, the display and the boot files, then loops forever: load the sound
  files and the first module, then run modules by `game_mode` (a 16-case switch; most cases read a file to
  `0x80180000`, unpack it to `0x800A0000` and call it) until `D_8009ABF5` asks for 5, `0x0A` or `0x1A`. It has an
  unused local `"battle.x"`. The match hinges on the loop optimizer: the game rebuilds `&D_8009AC32` at the top of
  each pass and keeps `&D_8009ABF5` and `&D_8009ABF6` in registers set before the middle loop. That needs:
  - no `mode` pointer (a user variable deriving from `&D_8009AC32` gets it hoisted);
  - the middle loop as a `while (1)` whose four exits (disc refused, ending, game over, title) each set the state,
    shut the drivers down and `break` (GCC cross-jumps the copies and parks the blocks after the loop);
  - the sound requests through a block-scoped `SoundCommand *`;
  - most callees without a prototype. With prototypes the code is 5 luids short and the `&D_8009ABF5` temporary
    isn't moved.
- **Disc files:** `{lba, size}` pairs. The data section opens with the sound files (`instr_all_file`,
  `effect_all_file`, `instr_dat_file`, `instr2_all_file`, `instr2_dat_file`) and more; `disc_files` (`0x80048D84`) is
  a table by number, its first 10 entries reloaded from sector `0x266` by `load_disc_table`.
  - `read_disc_file(i, dest, callback)` starts a read through `cd_read(lba, size, dest, callback)`;
    `wait_disc_read(idle)` polls `cd_step`, calling `idle` while it waits.
  - `load_sound_files` puts INSTR.ALL at `0x800F0000`, EFFECT.ALL at `0x801B0000` and INSTR.DAT at `0x801BC800`, then
    calls `snd_init` and `snd_load_effects`; `snd_load_bank2` loads the second bank into `0x800A0000`/`0x800E0000`.
  - `load_boot_files` reads files 1 and 2 with read callbacks that walk their numbered entries
    (`func_80015B44`/`B50`/`B88`/`BC0`): file 1's entry 0 is an image for VRAM, file 2's entries go to the addresses
    in `D_80048DD4`. It and `func_800148A0` reach `D_80062F88` and `D_80063014` (the scratchpad pointer) through
    `$gp`.
- **Modules:** `run_overlay(file, entry)` reads the file to `0x801B0000`, unpacks it to `0x800A0000`
  (`func_80015CA0`) and calls `entry`. `func_800146A4` loops on `game_mode`: 4 starts file 3, 2 file 5.
- **Display:** two `DISPENV`s (`disp_env`, y 232 and 0) and two `DRAWENV`s (`draw_env`, y 8 and 240, `dtd` on).
  `init_display` sets them up, `flip_frame` toggles `frame` and puts the next pair and its ordering table,
  `copy_frame` copies the shown buffer to the other. `vsync_callback` runs what `D_80095DD4` picks (1 `flip_frame`),
  then advances the play-time clock: `D_8009D26C` gains `0x444` a frame, so its top half is one second at 60 Hz,
  counted in `D_8009D264`; `D_8009D270` drives the countdown `D_8009D268` the same way.
- **Screen fade:** `func_800138EC` runs it each frame, a switch on mode `D_8009AC40` (14 cases, written in the order
  0, 1, 2, 7, 8, 3, 13, 4, 5, 6, 9, 10, 11, 12, as the game lays them out). The modes pick a blend (`func_80012840`
  sets both draw modes' tpage), step the level `D_8009AC42` down or up (`func_800134F4`, `func_80013564`) or count it
  to an end, and draw.
  - `func_80013624` moves a colour from `D_8009AC4C`–`50` to `D_8009AC52`–`56` over `D_8009AC44` steps through
    `func_800135C0` (`from + (to - from) * t / n`).
  - The draw routines clear the frame's ordering table, colour both tiles of `D_8009A068[frame]` and link the tiles
    and the draw mode: `func_800131B8` in a colour, `func_80012DB0` in a colour scaled by the level
    (`level * c / 256`), `func_80012A8C` in grey. `func_800128B8` sets the tiles up (160x224 each, side by side).
- **libgpu macros:** colour, size and position stores only match written as libgpu's `setRGB0`/`setWH`/`setXY0`
  expand, through `&array[frame].tile[j]`: GCC then holds the tile's address (symbol plus offset) in a register and
  adds the field offset, where `array[frame].tile[j].r0` folds it into the `$at` form. `main.c` has them as
  `set_rgb`, `set_wh`, `set_xy`; linking is libgpu's 24-bit `next` field (the `0xFF000000`/`0xFFFFFF` masks), a
  bitfield in `add_prim`. `transition.c` uses the same macros for its quads' colour, texture coordinates and corners.
- **Save data:** one struct from `0x8009D274` to at least `0x8009D588` (`D_8009D274.disc` the disc number; two
  byte-pair counters at `0x8009D2A0`). `func_800119E4` saves its state, `func_80011AEC` restores it, `func_80011BB4`
  resets it. As a struct starting at the state, the first field (in a register) would be the wrong one.
- **Screen transition** (`transition.c`): `func_800140F4` gives it its own display, copies the screen in four strips
  with every pixel's semi-transparency bit set, and sets up a 10x7 grid of textured quads over an 11x8 vertex grid.
  `func_80013C9C` draws a frame: it alternates two quad grids, fades or re-pages them, rotates and scales the vertex
  grid with the GTE and links the projected quads. Its buffers sit back to back from `0x8019C000` (two quad grids, the
  current one, the vertex grid, the projected grid) as the struct `ShardState`; scalars past the grids are reached by
  symbol.

### CD layer (`0x80033A90`–`0x80034CF0`, 38 functions, all C)

`src/main/cd.c`. One request at a time, by sector: `cd_start_request(state, lba, size, dest, callback)` waits for
the current one, then queues the new one in `cd_request` (`0x80071A60`); `cd_step` runs that state's handler from
`cd_state_handlers` (`0x8004A634`, 21 entries) and returns the state, `CD_IDLE` (0) when done. Callers poll it.
- **Entry points:** `cd_read` (state 3), `cd_seek` (1), `cd_read_lzs` (11: reads 9 sectors at a time into
  `cd_sector_buf` and unpacks them as they arrive), and `_wait` versions that poll with `VSync(0)`. `cd_init` runs at
  boot, `cd_shutdown` on every way out of `main`'s loop.
- **States:** each request goes seek (`CdControlF(CdlSetloc)`), wait for `CdSync`, read, wait for `CdReadSync`, then
  17 (done) and 16 (go idle and call the callback). 19–20 pause the drive. 8–10 belong to the movie player (below);
  18 idles, but the next request pauses the drive first.
- **Errors:** a `CdlDiskError` retries the step; 16 in a row stop the drive (`cd_stop`, state 7) and loop
  `cd_error_sound(3)` (sound command 30, then a second's wait) until `cd_check_disc` returns 0. A step that waits
  3600 frames retries with the same sound.
- **Disc check:** `cd_check_disc` spins the disc up, checks `CdDiskReady` and the disc type, seeks sector 23
  (`SYSTEM.CNF`), then reads the disc number; `cd_read_disc_number` finds `\MINT\DISKINFO.CNF` with `CdSearchFile`
  and returns its eighth character minus `'0'`. `get_disc_number` wraps it; `main` compares it with the save's disc
  (`0x8009D588`) and runs `FIELD/DSCHANGE.X` when they differ.
- **`lzs_decode(src, dst)`:** the in-memory LZS decoder (`u32` packed size, flag bytes, 12-bit offsets into a 4 KiB
  window, lengths 3–18; window positions before the output read as zeros).
- **After it:** `0x80034CF0`–`0x80034F3C` is hand-written assembly (data words in `.text`, registers kept across
  calls): `lzs_stream_start`/`lzs_stream_resume`, the decoder `cd_read_lzs` feeds, and `offset_table_entry`
  (`table + table[i]`), which the movie player uses.
- `func_80033A90` sits here but isn't CD code: it clears the 0x500 bytes up to `0x8009D787` and resets play time.

### Movie player (`0x80034F3C`–`0x8003623C`, 14 functions, all C)

`src/main/movie.c`, built on libpress and libcd's streaming (`St*`). Its state is `movie` (`0x80095D88`).
- **Opening:** `movie_open(buf, id)` (called from outside the exe) refuses while the CD layer is busy, fades the XA
  audio up, and looks `id` up in `movie_table` (`MOVIE_ID.BIN`, loaded at boot by `movie_load_table`). It carves two
  run-level buffers, a column buffer and (type 0) a 36-sector ring out of `buf` and returns the end of them. Type 0
  seeks to the movie, types 2 and 3 read it whole; `movie_on_ready` then sets CD state 9.
- **Starting:** in state 9 `movie_poll_start` waits for 8 buffered sectors, then sets 10. The caller then runs
  `movie_start`, which decodes the first frame and sets 8.
- **Playing:** in state 8 every `cd_step` runs `movie_play_frame`. It sends the decoded frame to the MDEC into the
  buffer being drawn (`movie_target`), one 16-pixel column at a time (`movie_slice_done` copies each to VRAM and asks
  for the next). Meanwhile `movie_decode_next` takes the next frame, copies its camera block and colour flag, and
  VLC-decodes it into the other buffer. Then it waits for the last column. A stream that stalls is sought back and
  restarted (`movie_reread`).
- **Ending:** a frame number at or past the table's last frame, or one that doesn't advance, ends a streamed movie;
  type 3 ends after its last frame and type 2 loops. `movie_stop` pauses the drive, drops the callbacks and returns the
  CD layer to idle. If the screen was in 24-bit colour, it clears the 480 lines it used.
- **Colour depth:** a change in the frame's flag is held for one frame (`movie_depth_pending`), then applies to both
  the MDEC and the display (`isrgb24`); a 24-bit frame's columns are 24 halfwords wide.
- **Timeouts:** the waits count down from `0x800000` (splat shows it as `D_7FFFFF`, from `lui 0x80`; `addiu -1`).
- From `0x8003623C`: two data words and an unreferenced hand-written helper, then libspu (its `SPU:T/O` strings).

### Sound driver (`sound_init.c` `0x800293D0`–`0x80029A4C`, `sound.c` `0x80029A50`–`0x80033A90`)

Each file is its own splat segment. 231 of its 232 functions are C (`seq_cmd_slur_off`, `seq_cmd_full_length_off` and
the sound-command no-op `func_8002CF98` are empty in the game). Unrecovered ones are `INCLUDE_ASM`. **Parked:**
`snd_start_lasting_effect` (the kind 2 effect's start): the game keeps `&seq_channels[54]` in `s1` across the first
`channel_reset`; GCC treats it as a constant, rebuilds it for each use and addresses channel 55's fields from it. About
190,000 permuter tries didn't find the form.

**Structs** (`include/sound.h`, named from what the code does with them):
- `SeqChannel`, `0x108` bytes: read pointer, loop stack (`+0x04` starts, `+0xB8` level, `+0xBA` passes), drum map
  (`+0x14`), layer voice (`+0x24`), volume, pan, octave, length fields, glide, bend, transpose and detune, LFO depths
  and offsets, the noise and FM countdowns, the envelope (`+0xEC`–`+0x102`), flags (`+0x38`), `+0x54` set on effect
  channels (2 = kind 2), instrument number (`+0x58`), `vibrato_amount` (`+0x7C`), `key` (`+0xD0`). Pitch offset
  `bend_offset` (`+0x34`, its top half adds to the pitch register; step `bend_step` `+0x4C`, ticks `bend_count`
  `+0x64`). Effect `pitch_shift` `+0x3C` (a signed byte << 8, read as byte `+0x3D`; step `+0x40`, ticks `+0x5A`);
  `effect_ticks` (`+0x50`).
- `VoiceSettings` (`+0xDC`, `0x2C` bytes): what `snd_update_voice(voice, settings)` hands to libspu; first word the
  voice number. Its `update` (`+0xE0` in the channel) is libspu's voice attribute mask: `0x3` the two volumes, `0x10`
  pitch, `0x80` sample, `0x100`–`0x400` the envelope modes, `0x800`–`0x8000` the rates and sustain level, `0x10000`
  loop. `snd_direct_voice` (`0x8007EBE4`) is a standalone one for voices written without a channel.
- `SeqSong`, `0x60` bytes (the save slots' stride confirms it): voice masks `active_voices` (`+0x04`), `key_on_voices`
  (`+0x08`; alias `seq_song_key_on_voices`, effects' copy `effect_key_on_voices`), `key_off_voices` (`+0x10`),
  `paused_voices` (`+0x14`), tempo (`+0x18`), masks for layers, overlaps, noise, reverb and FM (`+0x24`–`+0x34`), flags
  (`+0x38`: `0x10` noise changed, `0x80` reverb depth changed), reverb mode (`+0x3C`), reverb depth (`+0x40`), their
  slide counters, `EF`'s recorded n (`+0x4C`), branch level (`+0x4E`, set by `92`), song id (`+0x4A`, 0 = none), the
  noise clock (`+0x52`), `pause_pending` (`+0x54`, set by `F3`), and the meter (`+0x56`–`+0x5E`: `bar_beat` `+0x58`,
  `beat_tick` `+0x5C`).
- `Instrument`: the 64-byte `INSTR.DAT` entry in `instrument_table`.
- `SoundCommand`: a 16-bit id, then 32-bit arguments from `+0x04` (`0x24` bytes, as queued).

**Globals:**
- `seq_channels` (`0x80096608`): every channel. 0–23 the song, 24–47 the second song block, slots 48–55 effects
  (four pairs at `0x30`, `0x32`, `0x34`, `0x36`), and **effect slots 48–55 are SPU voices 16–23**. A layer's spare voice
  keeps its envelope in its voice's entry, so the envelope commands write both.
- `seq_song`, and `seq_song2` (`0x8009A164`, right after it) owning channels 24–47 with its voices limited by
  `seq_song2_voices` (`0x80062F68`): the song fading out under `18`/`19`, and a waystation for `15` and `F4`.
- The effects' block: `effect_voices` (`0x80099FCC`) and the words after it follow `SeqSong`'s first fields
  (`effect_key_on_voices`, `effect_update_voices`, `effect_key_off_voices`, `effect_paused_voices`). Effect channels
  use their own masks and noise clock: `effect_noise_voices`, `effect_reverb_voices`, `effect_fm_voices`
  (`0x80099FEC`–`F4`), `effect_noise_clock` (`0x80099FFA`), and their own `effect_tempo`, `effect_tempo_phase`.
- `snd_flags` bits: 0 `SND_MUSIC_PAUSED`, 1 effects paused, 2 `SND_DOUBLE_SPEED`, and `SND_REVERB_SCALED`.
- Save slots `saved_song_channels[2][24]` (`0x8007EC10`), `saved_songs[2]` (`0x80083334`): slot 1 holds song 14,
  slot 0 any other. `song_data` (`0x80083580`), `snd_channel_fades[24]` (`0x8009C5A0`, 12 bytes each),
  `snd_fade_ticks` (`0x80062FC8`), `snd_request` (`0x8009A000`), `snd_queue_locked` (`0x80062F8C`),
  `seq_cmd_sizes` (`0x800499A8`), `seq_cmd_scan_sizes` (`0x800498A8`), the end stub `seq_end_stub` (`0x80049C40`).

**Sequence command handlers** (`seq_cmd_handlers[op - 0xA0]`) all take (channel, song, the channel's SPU voice bit).
`B0` and `CB` pass all three on; the reverb, noise and FM commands set or clear that bit in a voice mask. Every unused
command byte calls `seq_cmd_end`. Covered: octave, slur and full length, volume, pan, transpose, detune, lengths (`A2`,
`DC`), glide and bend, loops, repeats and jumps (`C8`, `C9`, `CA`, `EE`, `F0`, `F1`, `EF`), drum kit, tempo and its
slide, reverb depth and its slide, time signature and bar number, the tremolo and pan-LFO starts, the LFO depths, depth
slides and off commands, volume, pan and balance slides, noise (on, off, clock, timed), FM and reverb on/off, pitch and
volume follow, layer balance and layer off, overlap off, the envelope setters, `B0`, `B3`, `B4` (vibrato start), `CB`
(all off), `F3` (`func_800335CC`, sets the song's `+0x54`), `F4`/`F8` (the layer and overlap voice searches).
- `A0`: an ending effect sets the full update mask on the `seq_channels` entry its `+0x28` names, handing the voice
  back to that voice's music channel.
- `A1`, `F2` (instrument changes): rescale a sounding note's pitch by the new instrument's C pitch over the old one's,
  unless an effect holds the voice (`effect_voices`); with `F8` on they copy everything but the release (update
  `0x1BB80`). `F2` sets the sample to `0x76FE0`, the silent blocks. `load_channel_instrument` copies the sample and
  loop addresses and all eight envelope bytes and sets `0x1FF80`.
- `F9` restores the release rate from the instrument's `adsr[4]`.
- `F1` is `F0` that also pops the loop level; `EF` jumps when the song's `+0x4E` is non-zero and at least n, and
  records n at `+0x4C`.
- `DA` (glide) also turns slur mode on; `DB` only clears the glide ticks.
- `CE`/`CF` are the noise twins of `D2`/`D3`: noise on now (`CE` only) and flipped after n + 1 ticks. `DF` is the
  pan-LFO depth slide, twin of `DD` and `DE`.

**`seq_step_channel`** (`0x80030E7C`) runs a channel's next event:
- Calls handlers until a note, tie or rest (or `A0`, which stops it).
- A note byte is note × 11 + length (`seq_note_lengths`, a gate of key-off << 8 | ticks), then ties `0x84`–`0x8E` and
  rests `0x8F`–`0x99`. With `DC` the gate is length × `0x101`. The key-off comes 2 ticks early before a note or rest,
  unless the next note is a tie or the channel slurs (`CC`) or plays full length (`D0`); before a rest it always does
  when `DC` is on.
- Drums (`EC`): the note picks 5 bytes from the drum map: instrument (loaded only when it changes, release kept under
  `F8`), key, volume (2 bytes, << 16) and pan.
- Otherwise key = octave × 12 + note (kept in `key`) plus transpose; under `DA` the note starts from the last key and
  bends to its own over the glide's ticks.
- Key-on unless the note slurs; a layered channel (`F4`) keys its layer voice too.
- Pitch: the instrument's octave-6 pitch for the semitone, shifted by octave − 6, then detune (n/128 up, n/256 down).
  Vibrato's amount comes from the depth and this pitch; the three LFOs restart. `A4` (or a glide) sets `bend_step` so
  `bend_offset` reaches the new key's pitch in `bend_count` ticks.
- `seq_look_ahead` follows `C9`/`CA`/`EE`/`EF`/`F0`/`F1` on a copy of the loop level, by `seq_cmd_scan_sizes`.
- `seq_stream_end` (was `func_80031A70`) walks a stream by `seq_cmd_sizes` (0 for the commands that end one) and
  returns `CA` or `A0`.

**Sound commands** (`snd_command_handlers`; names follow the byte, `snd_cmd_D0`, until the meaning is known):
- **`snd_submit_request`** (was `func_8002DA7C`) is the game's way in: it reads `snd_request` and queues commands with
  the queue locked. Song starts (`10`, `14`, `15`, `18`, `19`) need data starting `AKAO`, then id, size and reverb
  mode; it returns 1 when that song already plays and -1 without the header. `24`–`27` queue `20`–`23` (effects with
  consecutive ids), `D8`–`DA` queue the tempo and pitch scale commands together, `98`/`99` resume or pause music and
  effects; anything else passes through. `snd_flush_queue` runs each queued command unless `snd_queue_locked` holds
  it; `snd_queue_next` gives the next free `SoundCommand` in `snd_queue`.
- **Songs** (`10`, `14`, `15`, `18`, `19`):
  - `10` a b id loads with `func_80029B78`, then resumes a saved song with that id (either slot, `snd_resume_song`)
    or starts it (`snd_start_song`); song 14 is saved first when it's playing.
  - `14` saves whatever plays (`snd_save_song`) before starting the new one.
  - `15` is `10` that swaps: the playing song goes through the second block into a save slot while the saved one
    resumes (or, with no saved match, it's saved and the new one starts).
  - `18`/`19` are `10`/`14` that cross-fade: `snd_fade_out_song` moves a playing song to the second block (channels
    0–23 to 24–47, `seq_song` to `seq_song2`, song data 12 KB up with every read, loop and drum-map pointer moved by
    `0x3000` and layer voices renumbered from 24) and fades it: `snd_channel_fades` takes channel n from `0x7F8000`
    to 0 over (24 − n) × `snd_fade_ticks` ticks (arg 3, 0 = 16).
  - `snd_start_song` reads `song_data`: a channel-mask word, then a 16-bit offset per channel (from after the
    offset). Every channel starts on instrument 20 at volume `0x3FFF0000`, centred; the song state starts over
    (tempo `0xFFFF`, `tempo_phase` 1).
  - `snd_resume_song` copies a save slot back field by field (the beat's tick count 2 back), then the channels.
    Channels the song doesn't hold step into the end stub; its voices rewrite everything. Unless a slide or the
    cross-fade runs, `snd_master_volume` restarts at 10 and slides back over 60 ticks.
  - `snd_load_song` copies the song into `song_data`; `snd_save_song` copies 24 channels and the `SeqSong` into a
    slot; `snd_keep_sounding_voices` runs first and keeps the voices still keyed on, or whose `F8` overlap voice is.
- **Effects** (`20`–`34`): look the id up with `func_8002A6C4` (its two streams), then `snd_stop_effect_pairs` (one to
  four pairs) and `snd_start_effect(pan, pair, first, second)`. `20`/`28` play on pair `0x34`, `29` on `0x32`, `2A` on
  `0x30`, `2B` on `0x36`; `21`–`23` play two, three or four effects together from `0x30` (or `0x32`) up; `30` hands
  its effect to `snd_start_lasting_effect`; `34` plays two caller-given streams, centred.
- **Effect controls `A0`–`B7`:** balance, balance slide, pan, pan slide, pitch shift, pitch-shift slide, four commands
  each, one per pair (slots 52, 50, 48, 54), via a shared worker (`snd_effect_balance` …) that sets both channels. A
  slide's length 0 means 1 tick. `B8`–`BD` apply the six to all four pairs (from slot 54 back, two channels a step).
- **Scales and volumes:** `D0`/`D1` and `D4`/`D5` set or slide `snd_tempo_scale` / `snd_pitch_scale` (a signed byte
  << 16), `D2`/`D6` are their from–to slides. `C0`/`C1`/`C2` set or slide `snd_master_volume`. `C8`/`C9`/`CA` set,
  slide and slide from–to `snd_cd_volume` (`0x80062FD4`, << 16); `C8` also calls `func_8002E428`.
  `snd_apply_cd_volume` writes its top half to both sides of a libspu `SpuCommonAttr` (mask `0x1C0`: CD volume
  left/right and CD reverb, here 0) and hands it to `0x80039034` *(probably `SpuSetCommonAttr`)*.
- **Others:** `80`/`81`/`82` store 1/2/4 in `seq_song`'s first word and call `func_8002A748` and `func_8002A798`.
  `90` stores a mute mask (`snd_muted_voices`: volume 0) and sets every music channel's volume-update bits. `92` sets
  the branch level. `E0` sets `snd_reverb_balance` (`0x80062F70`) and the reverb-changed flag; `E4` sets
  `snd_reverb_scale` and `SND_REVERB_SCALED` (every voice in reverb). `F0` (`snd_stop_songs`) runs `snd_stop_song` on
  both blocks; `F1` (`snd_stop_effects`) ends every effect but kind 2; `F2`/`F3` clear one word each (emptying the
  save slots); `FA` (`snd_stream_stop`) and `F8`/`F9` (`snd_stream_start`, taking the stream's voices from the
  effects with reverb off or on).
- **Pause:** `9B` gives the music's voices (not the effects' or the stream's) volume 0 and a held sustain through
  `snd_direct_voice`, moves `active_voices` to `paused_voices` and sets `SND_MUSIC_PAUSED`. `9A` rewrites volume and
  sustain, moves the masks back and resends the noise, reverb and FM voices. `9D`/`9C` do the same for the effects
  (bit 1), but a kind 2 effect on channels 54/55 keeps playing.
- **`F4`/`F5`:** `F4` runs `snd_keep_looping_effects` (an effect whose stream ends in `CA` stays), sets the effects
  aside (their 8 channels into `seq_channels[24..31]`, 12 words of their block after them), stops them and runs `B8`
  (`0x7F`) and `BC` (0) on every pair. `F5` copies them back and keys off the voices they no longer hold.

**Per-tick work:**
- **`snd_timer_tick`** runs the ticks due since the last call (scanlines ÷ 66, 1–8, doubled by `SND_DOUBLE_SPEED`),
  keeping the last four calls' times in `snd_tick_time[4]` and their sum in `snd_tick_time_total`.
- **`seq_tick`:** each block adds its tempo, scaled by `snd_tempo_scale` (top byte, read unsigned), to its phase; on a
  carry every channel's gate counts down, the key-off at the high byte and the next event (`seq_step_channel`) at the
  low one. Effects run only while not paused (except kind 2), counting `effect_ticks`. Then the first song's tempo,
  reverb and meter counters advance, a pending `F3` pause runs as `9B`, the queue flushes, the global slides run and
  voices key off. It reaches the first song through plain aliases (tempo, phase, slides, meter, pause flag) and the
  second through `seq_song2`'s fields.
- **`snd_slow_slides`** (every 4th tick, `snd_slow_slide_tick`): CD volume, the master volume (a fade to 0 stops the
  songs), the tempo and pitch scales (a pitch-scale step rewrites every music pitch); each effect's balance, pan and
  pitch-shift slides (a balance fade to 0 ends it); and the cross-fade: when channel n's fade ends, the old song's
  channel ends, the new song's rewrites everything, and the fade restarts as a fade-in.
- **`seq_channel_slides` / `seq_effect_slides`:** volume, balance and pan (music only), the vibrato and tremolo delays
  (music only), the timed noise and FM flips (on the song's masks or the effects'), the three LFO depth slides with
  output recomputed, and the `+0x34` pitch slide.
- **`seq_channel_lfo`** (music voice update): each LFO that's on and past its delay steps through its waveform.
  Volume is A8 × A3 (or the previous channel's pitch, `D6`) plus tremolo, × `snd_master_volume`, × the cross-fade,
  split by pan plus pan LFO through `snd_pan_left` / `snd_pan_right`. Pitch is the note's (or the previous channel's,
  `D4` mode) plus vibrato and `+0x34`, then scaled by `snd_pitch_scale`'s top byte read unsigned: × (128 + n) / 128
  below 128, × n / 256 from 128 up, so negative values lower the pitch.
- **`seq_effect_lfo`:** the same without LFO delays, master volume and cross-fade; the effect's balance scales its
  volume, pan places it, and its pitch shift scales the pitch as `snd_pitch_scale` does. Kind 2 skips both, centred.
- **`seq_update_voices`** (output pass): a changed reverb depth goes to the SPU scaled by `snd_reverb_scale` and split
  by `snd_reverb_balance`; a changed noise clock goes too. For both song blocks and the effects, each voice due an
  update runs its LFO update and writes what changed: layer channels through `seq_layer_voice_update` (main voice volume
  × (127 − n) / 256, layer n / 256, pitch copied), overlap channels to whichever voice their turn is on (flag `0x400`
  flips on each key-on; in the music block a busy overlap voice falls back to the channel's own). Then the due voices
  key on together.
- **`snd_update_voice`** hands each group with update bits set to libspu (pitch, volume, start, loop, then the
  envelope pairs: sustain, attack, release, decay with sustain level), clearing each group's bits until none are left.
- **Voice masks to the SPU:** `snd_apply_noise_voices`, `_reverb_voices` and `_fm_voices` build one mask from both
  song blocks and the effects' copy, after `snd_add_spare_voices` adds each channel's `F4` layer or `F8` overlap
  voice, and set it on and its complement off. `snd_key_off_voices` does the same with `key_off_voices`.
  `snd_refresh_effect_volumes` walks `effect_voices` from bit `0x10000` against slot 48; its twin walks
  `active_voices` from slot 0.
- **`snd_stop_song`:** every voice the song holds (layers and overlaps too) gets a 2-tick key-off and steps into
  `seq_end_stub`; the song's masks are cleared.

**Setup and helpers:**
- `snd_effect_streams`: an effect's two streams, `effect_data` + the 16-bit offsets 2·id and 2·id + 1 in
  `effect_offsets` (`0xFFFF` = none).
- `channel_reset`: instrument 5, octave 2, volume 100, multiplier 120, some twenty fields zeroed.
- `snd_set_reverb_mode`: on a change it stores the mode at `+0x3C`, turns reverb off, sets the mode with its work area
  cleared (`0x100`) through libspu's `SpuReverbAttr`, and turns it back on.
- **The sample stream** (`snd_stream_*`): sample data streams from main RAM through a double buffer at SPU
  `0x77000`–`0x78FFF`, on voice 16 (mono) or 16/17 (stereo, 2 KB each per half). `snd_stream_start` reads the header
  (total size, flags: bit 0 stereo, bit 1 loops, top half `snd_stream_pitch`; loop offset; a spare word), primes
  8 KB and stops the `0x30` pair. `snd_stream_voice_mono` / `snd_stream_voices_stereo` set the voices up through
  `snd_direct_voice`; `snd_stream_play_mono` / `_stereo` arm the first IRQ and key on. Each half's IRQ
  (`snd_stream_fill_*_a` / `_b`) copies the next 4 KB into the other half with `SpuWrite` and re-arms. At the end it
  restarts from `snd_stream_loop` (`snd_stream_loop_size` bytes) or `snd_stream_stop` keys the voices off.
- **`sound_init.c`:** the SPU transfer helpers (`snd_transfer_write`/`read`/`wait`, `snd_transfer_busy`), `snd_reset`
  (the boot state, `0x800294BC`), `snd_init`, `snd_load_effects` (copies EFFECT.ALL, `0xC800` bytes, into its
  buffer), `snd_quit`, and the bank loaders `load_instrument_bank` / `load_instrument_bank2` (all 128 instruments, or
  from 53 on).

## 8. Sony library map (main exe)

`tools/psyq_match.py` parses SN `.LIB` archives (`LIB` with `LNK` objects), masks each relocated field by its patch
type and searches the exe for each library module, falling back to single functions. `--ref-exe EXE NM_LIST` also
matches a linked library EXE named by an `nm` listing. Only names and addresses come out, into
`config/symbols.main.psyq.txt`, which splat reads; the rebuild stays byte-identical.

Sources, in priority order (on a rename the first wins, so the closest version goes first):

| Source | Where | Version stamps (libetc / libcd / libgpu) | Names alone |
|---|---|---|---|
| PsyQ **3.5** libs | `LIB35.ZIP` (1996-05) from a PsyQ SDK archive | 1.73 / 1.77 / 1.120 | 247 |
| PsyQ 4.4/4.5 libs | PsyQ 4.4 SDK (`psx/lib`, patches, `Lib44`, `Lib45`) | 1.75 / 1.86 / 1.140 | 154 |
| Net Yaroze resident library | `MON/LIBPS.EXE` on DTL-S3030/S3035, named by `PSX/LIB/LIBPS.A` on DTL-S3040 (`nm` listing) | 1.73 / 1.76 / 1.120 | 40 |
| **FF7 itself** | `SCUS_941.63` | 1.73 / 1.76 / 1.115 | |

Combined: **267 of 454** functions in the library block `0x80036E84`–`0x80048D18` named (58%): libgte 110, libgpu 40,
libapi 40, libspu 14, libcd 13, libetc 12, libc 14, libcard 10, libpress 5, Net-Yaroze-only 10. The libspu names
(`SpuSetReverb`, `SpuSetIRQCallback`, `SpuGetKeyStatus`, `SpuMallocWithStartAddr`) are anchors for the AKAO driver.

Rules that keep false names out:
- **Unique:** a hit occurs exactly once in the exe.
- **Distinctive:** a hit needs at least 10 words not touched by relocations, or no relocations and at least 4 words
  (BIOS call stubs carry a unique call number). This rejects e.g. `atof` at `0x800144F0` in game code.
- **Siblings:** smaller functions count only when a distinctive sibling from the same module puts the module at the
  same base.
- **Clashes:** two names for one address are dropped, unless both come from the same library: then it's an API renamed
  between versions (`InitPAD` → `InitPAD2`, `delete` → `erase`) and the first-listed source wins. Dropped example:
  `0x80048A7C`, a "call three functions" wrapper that fits both libsnd `SsInitHot` and libmcrd `MemCardInit`.
- **One name per address:** a name found at two addresses stays with the higher-priority source (3.5 `StartCARD` is
  a BIOS stub; 4.4's is a different wrapper).
- **Reference EXEs** have no relocation records, so jump targets, `lui` immediates, and offsets from a `lui`-loaded
  register or `$gp` are masked.

Review candidates (`--fuzzy 0.8`, not applied), with similarity scores. Fuzzy names need checking: of 20 guesses
from the 4.4 libs alone, 8 are confirmed by exact matches and 2 are wrong (`0x80038FEC` is not
`SpuStSetPreparationFinishedCallback`, `0x80043928` is not `ReadInitPadFlag`).
- **libspu** (1.00 but too small to pass alone): `_SpuDataCallback` `0x80036FD8`, `SpuSetNoiseVoice` `0x80037964`,
  `SpuSetReverbVoice` `0x800388C4`, `_SpuCallback` `0x80038C48`, `SpuSetTransferMode` `0x80038FB8`,
  `SpuSetTransferCallback` `0x80038FEC`, `SpuSetPitchLFOVoice` `0x80039010`; `_SpuIsInAllocateArea_` `0x80037D90` 0.92.
- **libcd:** `CdControl` `0x8003DE9C` 0.90, `CdControlF` `0x8003DFD4` 0.84, `CdMix` `0x8003E244` 0.89, `CdGetSector`
  `0x8003E268` 0.89, `CdLastPos` `0x8003DD08` 1.00, `CdPosToInt` `0x8003E3D4` 1.00, `CD_vol` `0x8003F398` 0.91,
  `CD_initvol` `0x8003F500` 0.85, `StSetRing` `0x8003DBA0` 1.00, `StClearRing` `0x80040898` 1.00, `StSetMask`
  `0x80040C88` 1.00, `CdGetToc` `0x80042144` 1.00, `CdGetDiskType` `0x8004076C` 0.81.
- **libgpu:** `ClearOTag` `0x8004418C` 0.85, `DrawOTag` `0x8004433C` 0.90, `SetDrawMode` `0x80044A68` 0.91,
  `SetSemiTrans` `0x80046848` 0.90, `SetShadeTex` `0x80046870` 0.90, `BreakDraw` `0x800484A8` 0.89.
- **libetc:** `GetVideoMode` `0x80043928` 1.00, `startIntrDMA` `0x8003D8F8` 0.81.

The other ~187 unnamed functions are probably from PsyQ 3.3/3.4 builds or are static helpers with no exported name;
the game's libgpu `sys.c 1.115` is older than every source we have.

**Game code ends just before `0x80036E84`.** Open question: are the last unnamed functions before it (`0x80036D98`,
`0x80036DE0`) game code or the start of libspu?
