# Save tool: reading and changing saves for testing

## Intent

The lab tests from saves: 57 of them, one per story stop. Today a test can only start where a save happens to be.
A tool that reads and changes saves lets a test start anywhere: a given map, party, items, gil, levels or story
point, without playing there first. Saves stay outside the repo (the user's cards), and so do changed ones.

## What we know (knowledge base, "Memory cards")

A card is 128 KB: a directory block, then fifteen 8 KB blocks. An FF7 save is one block, `BASCUS-94163FF7-Snn`: the
0x200-byte title frame and icons, then 0x10F4 bytes that are the game's RAM from `0x8009C6E4`, then zeros. The
checksum (the save menu's CRC-16 over `+4` to the end) is checked on load. The layout of the characters, party,
items, gil, time, module, field, disc and place name is checked against the game; the rest comes from a study map
(LGPL, ideas only), to be checked as the tool needs it.

## The proposal

`tools/save.py`, like `model.py` and `field.py`: plain functions, no classes, a JSON form people can edit.

1. **`save.py dump CARD OUT`:** each FF7 save on the card into `OUT/Snn.json`: the fields we know by name (party,
   characters' levels, HP, MP, EXP and names, items as name or number and count, gil, time, module, field, disc,
   place), and everything else as hex, so a dump builds back byte for byte.
2. **`save.py build FOLDER CARD`:** the JSON files back into the card's blocks, the checksum worked out again, the
   title frame's preview (`+4`) kept in step with the data it shows. A card that doesn't have the save yet gets a
   new file in a free block, its directory frame written as the BIOS writes one.
3. **`save.py set CARD Snn key=value ...`:** a quick change without the JSON, for test scripts: `gil=999999`,
   `field=116`, `party=cloud,tifa,barret`, `level.cloud=50`, `item.potion=99`. Built on the same functions.
4. **Lab use:** a test copies a card, sets what it needs, names it `memcard1.mcd` and runs the Continue route.

## Checks

- Every one of the 57 saves dumps and builds back byte for byte, checksum included.
- A changed save loads in the lab (Continue) and shows the change: the menu's gil and level, the field it starts in.
- The checksum against the game's function, on a hand-made example.

## Done (2026-11-13)

`tools/save.py` (dump, build, set) and `tools/test_save.py`. The open points went to their defaults: as drafted;
the checked fields named; items by the game's names (`#n` for a blank or shared name). Writing a save into a free
block of a card that doesn't have one was left out: every test card has its save already. A changed save loads in
the lab and shows its changes. Setting a level leaves HP and MP as they were.

## Non-goals

- A save editor with a window (Black Chocobo exists).
- PC (Steam) saves, other regions, PSP/PS3 formats.
- Story flags by meaning: they stay hex until a test needs one named, and then it gets checked in the game first.

## Open

1. Is this the manipulation you had in mind, or did you mean something narrower (only a few fields from the lab's
   command line)?
2. Which changes would you reach for first? They set which fields get names (and checks) first.
3. Item and materia names in the JSON: from the game's own text (the kernel's item names, which `kernel.py` already
   dumps), or numbers only?
