# Decompiling the menu modules

## Intent

The main exe's game code is C now, with 12 functions parked. The menu screens aren't in the exe: each is an overlay,
`MENU/*.MNU`, loaded at `0x801D0000` when its screen opens. The PC port runs them recompiled. As C they can be read,
named and changed like the exe's menu code, which they call all the time (`menu.c`, `menu_util.c`, `menu_text.c`,
`menu_prims.c`). They are the smallest overlays, so they're also where an overlay's matching build gets set up.

## Scope

- **In:** the 13 menu modules. The figures are splat's (each module's function count is a floor where it didn't cut
  every function apart):

  | Module | Screen | Bytes | Instructions | Functions |
  |---|---|---|---|---|
  | `CHNGMENU` | not known yet | 536 | 113 | 3 |
  | `PATYMENU` | a debug party editor (module 11) | 29,900 | 305 | 3 |
  | `BGINMENU` | Beginner's Hall (module 12, a guess from its command name) | 2,200 | 508 | 12 |
  | `LIMTMENU` | Limit; also read first when the menu opens | 9,236 | 1,213 | 6 |
  | `STATMENU` | Status | 24,096 | 1,626 | 13 |
  | `CNFGMENU` | Config | 9,528 | 1,690 | 8 |
  | `FORMMENU` | PHS, and the party forming of field `MENU` 7 | 60,020 | 1,752 | 13 |
  | `NAMEMENU` | naming | 49,272 | 1,945 | 26 |
  | `MGICMENU` | Magic | 11,008 | 2,563 | 7 |
  | `ITEMMENU` | Item | 16,416 | 3,102 | 36 |
  | `SHOPMENU` | shops | 61,892 | 4,241 | 24 |
  | `SAVEMENU` | Save | 113,732 | 4,783 | 39 |
  | `EQIPMENU` | Equip / Materia | 96,140 | 8,288 | 53 |

  About 32,000 instructions in all, half again the exe's block.
- **Out:** the other overlays (field, battle, world map, mini-games, effects); the data some modules carry after
  their code (images, buffers), which stays data read from the user's disc.

## How

As the exe was done (`2026-11-14-exe-decompile-design.md`), with what an overlay adds:

1. **Each module is a matching build target.** Its splat config, generated today into `build/modules/` for the PC
   port by `tools/module_yaml.py`, moves to `config/menu/<name>.yaml` so it can carry `c` subsegments. Each module
   links at `0x801D0000` against the exe's symbols, and `wsl make` checks it with the exe:
   `build/MENU/ITEMMENU.MNU: OK`. The modules are the same on all three discs, so disc 1's are the reference.
2. **Symbols per module.** The modules share an address range, so each has its own symbol file,
   `config/symbols.<name>.txt`. Their calls into the exe take the exe's names.
3. **Names unique across modules.** The PC build links every module's C into one program, so no two modules may
   have a function or variable of the same name. Splat names them by address (`func_801D01E8` exists in several
   modules), so a function keeps that name only in its own module's asm; once it's in C it gets a real name, or the
   module's prefix while unknown (`item_801D01E8`). File-local data in C is `static`.
4. **Compiler per module,** found as for each exe file: try GCC 2.7.2 and 2.6.3 with ASPSX 2.21 and 2.34 on small
   functions first. A module is one file unless its flags change partway.
5. **Shared headers.** The structs and helpers the exe's menu files already named (`MenuCursor`, the window and text
   drawers, `prim_next`, the member and materia records) go in a header both use rather than being declared again.
6. **The PC build** compiles each module's finished C natively in place of its recompiled functions (`recomp.py
   --native` already takes a module's names), so each module also passes `wsl make -j8 pc-test` and a headless run
   that opens its screen (the menu run in `pt/menu2` covers most; the shop and naming screens, and whatever opens
   `CHNGMENU`, need runs of their own).
7. **Parking** as for the exe: after a fair try, permuter included, a function stays `INCLUDE_ASM` with its C under
   `#ifdef NON_MATCHING`.

## Sources

Written from the game's assembly and Ghidra's reading of our own build. Other projects' decompilations aren't opened
for these functions. Screen names and behaviour can be checked against the manual and the guide, never copied.

## Checks

- `wsl make`: the exe and every finished module `OK`, after every file.
- `wsl make -j8 pc-test`: 19 ok, plus the headless screen run.
- Each file's header says its range and compiler settings, and why.

## Order

Smallest first, so the build target, symbol files and naming are settled on a module of 113 instructions before a
module of 8,000: `CHNGMENU`, `PATYMENU`, `BGINMENU`, then by size up to `EQIPMENU`. A commit per finished module (or
per batch of functions in the large ones).

## Open

1. **The prefix for unnamed functions:** `item_801D01E8` (short) or `itemmenu_801D01E8` (the module's file name).
   Proposed: the short form.
2. **The save menu holds all memory-card access.** Its C is checked through the PC build's card files only, never a
   real card (the save-corruption rule stands).
