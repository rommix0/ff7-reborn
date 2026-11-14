# Decompiling the rest of the main exe

## Intent

Most of the main exe's own code is C now: the boot and main loop, the loader, the CD layer, the movie player, the
sound driver. What's left of the game's code is one block in assembly, `0x80014A84`–`0x800293D0` (splat's `5284`):
282 functions, 21,075 instructions, about 84 KB. The PC port runs it recompiled. As C it can be read, named,
changed and built natively like the rest.

## Scope

- **In:** the 282 functions of `asm/5284.s`: random numbers, the pad, text and window drawing, the menus' shared
  helpers, kernel and save data handling, and whatever else turns up there.
- **Out:**
  - the hand-written assembly (`18B8`, the entry code; `254F0`, the streaming LZS decoder; the helper at the start of
    `26A3C`), which has no C form;
  - Sony's libraries (the rest of `26A3C`, 471 functions named by `tools/psyq_match.py`): not the game's code, and
    their names are enough to read the callers;
  - the overlay modules (field, battle, world, menus, mini-games): separate segments, a later project if wanted.

## How

As the sound driver was done (knowledge base, "Toolchain"):

1. **Matching C, one function at a time.** `tools/match.sh` compiles a function and compares it with the exe. The
   build must stay SHA1-identical (`wsl make` ends `build/SCUS_941.63: OK`).
2. **Files by subsystem.** The block is cut into C files where the code's subject changes (random numbers, pad, text,
   windows, ...), each its own splat `c` subsegment in `config/main.yaml`, with `INCLUDE_ASM` for functions not yet
   done. A change of compiler settings (`$gp` use is `-G8`, delay-slot and `nop` patterns are the ASPSX version, a
   register choice can be GCC 2.6.3 against 2.7.2) also marks a new file, since one file has one set of flags.
3. **Names and structs** from what the code does, checked against what the game does (the lab), as before.
4. **Both builds.** The PC build compiles the new C natively instead of recompiling those functions, so each file
   also has to pass `wsl make -j8 pc-test` and a lab run through boot, a field, a battle and a menu. Values that hold
   addresses are `long`, as in `sound.c`.
5. **A function that won't match** after a fair try (permuter included) stays `INCLUDE_ASM`, with its C under
   `#ifdef NON_MATCHING` for the PC build, and is listed as parked.

## Sources

The C is written from the game's assembly (and Ghidra's reading of our own `build/main.elf`). Other projects'
decompilations of this exe are not opened for these functions: matching C written after reading someone else's would
be a disguised copy of it.

## Checks

- `wsl make`: `build/SCUS_941.63: OK` after every file.
- `wsl make -j8 build/pc/ff7.exe` and `pc-test`; a headless lab run: boot, the opening field, a battle, the menu.
- Each file's header comment says its range and its compiler settings, and why they're those.

## Order

Front to back through the block, one file at a time, a commit per file or per batch of functions. Small leaf
functions first within a file, so the larger ones can call named helpers. The size of the job: 169 functions are
under 50 instructions, 61 under 100, 30 under 300, and 22 are larger, the biggest 972 (`func_80023AD4`).

## Decided

1. The main exe is the goal; the overlay modules aren't planned.
2. A commit per finished file.
