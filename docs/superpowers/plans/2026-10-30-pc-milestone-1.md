# PC Port Milestone 1: The Exe Boots on the PC — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `build/pc/ff7.exe assets` runs the whole main exe (the decompiled C plus the rest recompiled from the user's
`SCUS_941.63`) and `FIELD/ENDING.X`, and shows the game's first screen in a window.

**Architecture:** A 32-bit program maps the PS1's RAM at `0x80000000`, so game addresses are real pointers and the
matching C compiles unchanged. `tools/recomp.py` turns every exe and overlay function that has no C into C over a
register context. The PS1 hardware the libraries drive (GPU, DMA, interrupts, timers, SPU registers) is emulated, and so is the GTE;
the BIOS and libcd are replaced in C. SDL2 shows the GPU's display area.

**Tech Stack:** C (gnu99) built with i686 MinGW-w64 (`/mnt/c/winlibs/mingw32/bin/gcc.exe` from WSL) or `gcc -m32`
on Linux; Python 3.10+ with rabbitizer 1.16.2 (`toolchain/venv`); splat (already used); SDL2 2.30.x.

**Spec:** `docs/pc_port.md`

## Global Constraints

- `make` still ends with `build/SCUS_941.63: OK` after every task.
- 32-bit only. RAM: 2 MB at `0x80000000`; scratchpad: 4 KB mapped at `0x1F800000` (1 KB used); I/O window:
  `0x1F801000`–`0x1F802FFF`.
- Recompiled code and anything else derived from the game is generated under `build/` from the user's files and never
  committed; nor is game data.
- No emulator code is copied (most are GPL). Hardware behaviour comes from psx-spx; record what each file took from it
  in `docs/REFERENCES.md` (dev-only, git-ignored).
- SDL2 is the only new dependency: fetched by a script at a pinned version and SHA-256 into `toolchain/`. Ask the user
  before running the download.
- R3000 load-delay slots and `add`/`addi` overflow traps are not emulated: GCC's code never depends on them.
- Style per `CLAUDE.md`. Tests follow the repo's pattern: small assert-based programs printing `ok`, run by
  `make pc-test`; Python tests are assert-based scripts like `tools/test_unpack.py`.
- Commit at a task's end only if the user has OK'd commits for this plan; otherwise leave the work uncommitted.

## Review Focus

1. C code passes the address of a native local (`RECT rect` in `main`) to recompiled code: the access must reach that
   native memory, not PS1 RAM. Pinned by Task 1's `native_pointer_passthrough`.
2. A vblank arrives while recompiled code spins in a loop: the handler runs and every register the loop holds is intact
   afterwards. Pinned by Task 4's `interrupt_preserves_context`.
3. A code address formed in recompiled code (`lui`/`addiu`) or read from a data table is later called from C: it must
   land in the right native function. Pinned by Task 3's `code_pointer_roundtrip`.
4. A module at `0x800A0000` is replaced by another while the first one's table is active: the next call must reach the
   new module. Pinned by Task 10's `overlay_switch_by_content`.
5. A polygon partly outside the drawing area, or with an edge longer than 1023 horizontally / 511 vertically: clipped,
   or skipped whole, as psx-spx describes. Pinned by Task 6's `gpu_clip_and_oversize`.

---

### Task 1: The 32-bit runtime base

**Files:**
- Create: `pc/memory.h`, `pc/memory.c`, `pc/exe.c`, `pc/test_memory.c`
- Modify: `Makefile` (targets `pc`, `pc-test`; `PC_CC ?= /mnt/c/winlibs/mingw32/bin/gcc.exe`,
  `PC_CFLAGS := -std=gnu99 -O2 -g -Wall -Iinclude -Ipc`, link flag `-Wl,--large-address-aware` for MinGW)

**Interfaces:**
- Produces: `int mem_map(void)` (0 on success; reserves RAM and scratchpad at their addresses);
  `unsigned int exe_load(const char *path)` (PS-X EXE: copies `size` bytes from offset `0x800` to the header's load
  address at `0x18`, returns `pc0` from `0x10`); in `memory.h`, `static inline` accessors
  `mem_r8/r16/r32(unsigned int addr)` and `mem_w8/w16/w32(unsigned int addr, unsigned int v)`: addresses
  `0xA0000000`–`0xA01FFFFF` alias `0x80000000`–; the I/O window calls `unsigned int io_read(unsigned int addr, int size)`
  / `void io_write(unsigned int addr, unsigned int v, int size)` (defined by Task 5; the test supplies stubs); every
  other address is dereferenced as a native pointer.

- [ ] **Step 1: Write `pc/test_memory.c`** with four checks: `mem_map` succeeds; after
  `exe_load("assets/…/SCUS_941.63")`, `pc0 == 0x800110C0` and the 64 bytes at `0x80010000` equal the file's at `0x800`;
  `mem_w32(0xA0001000, 7)` reads back from `mem_r32(0x80001000)`; `native_pointer_passthrough`: a local `int x` written
  through `mem_w32((unsigned int)&x, 42)` makes `x == 42`, and an I/O access at `0x1F801814` reaches the stub.
- [ ] **Step 2:** `make pc-test` fails (no `memory.c`).
- [ ] **Step 3:** Implement `memory.c` (`VirtualAlloc` at fixed addresses on Windows, `mmap` with
  `MAP_FIXED_NOREPLACE` elsewhere) and `exe.c`.
- [ ] **Step 4:** `make pc-test` prints `ok`; `make` still prints `build/SCUS_941.63: OK`.
- [ ] **Step 5:** Commit (per Global Constraints).

### Task 2: The recompiler core

**Files:**
- Create: `tools/recomp.py`, `tools/test_recomp.py`, `pc/recomp.h`, `pc/test_recomp.c`

**Interfaces:**
- Consumes: Task 1's accessors.
- Produces: `pc/recomp.h`: `typedef struct Ctx { unsigned int r[32], hi, lo; } Ctx; extern Ctx ctx;` and
  `void hw_poll(void);` (called at every backward branch; Task 5 defines it, tests stub it).
  `tools/recomp.py`: `recompile(image: bytes, base: int, funcs: dict[int, str], native: set[str], jtbls: dict[int, list[int]]) -> str`
  returns C with one `void r_<ADDR>(void)` per entry of `funcs` (address → symbol). CLI:
  `python tools/recomp.py --image FILE --base ADDR --symbols NM_LIST --native NM_LIST -o OUT.c`.
- Translation rules the tests pin: a branch's condition is read before its delay slot runs; the delay slot runs on both
  paths; `jr $ra` is `return`; `div`/`divu` by zero give `lo = (n >= 0 ? -1 : 1)` (`divu`: `0xFFFFFFFF`),
  `hi = n`; `0x80000000 / -1` gives `lo = 0x80000000, hi = 0`; `lwl`/`lwr`/`swl`/`swr` per psx-spx; `bltzal`/`bgezal`
  link. Jump tables come from splat's `jtbl_` labels in the `.rodata` asm.

- [ ] **Step 1: Write `tools/test_recomp.py`**: assemble small fixtures as raw words and assert on the generated C:
  `delay_slot_runs_on_both_paths`, `branch_reads_condition_before_slot` (`beq $a0,$zero,L; addiu $a0,$a0,1`),
  `jr_ra_returns`, `div_by_zero_values`, `unaligned_pair_order`.
- [ ] **Step 2: Write `pc/test_recomp.c`**, a differential test: recompile `copy_words`, `highest_bit`, `count_bits`
  (`0x80014A00`–`0x80014A84`) and `lzs_decode` (`0x80034BB0`) from the user's exe, then compare with
  `src/main/bitops.c` and `src/main/cd.c`'s `lzs_decode` compiled for the PC: 10,000 random `unsigned int` inputs for
  the bit functions, and every LZS file in `assets/*/FIELD/*.DAT` for the decoder (outputs equal byte for byte).
- [ ] **Step 3:** Both tests fail.
- [ ] **Step 4:** Implement `recomp.py` on rabbitizer (`InstrId` per instruction; GTE instructions call Task 7's
  `gte_cmd`, `gte_read`, `gte_write`, `gte_read_ctrl`, `gte_write_ctrl`; tests link stubs) and `recomp.h`.
- [ ] **Step 5:** `python tools/test_recomp.py` and `make pc-test` print `ok`.
- [ ] **Step 6:** Commit.

### Task 3: Calls between recompiled code and C; code pointers

**Files:**
- Modify: `tools/recomp.py`
- Create: `pc/dispatch.c`, `pc/test_bridge.c`

**Interfaces:**
- Consumes: Task 2's `recompile`, `Ctx`.
- Produces:
  - Every recompiled function gets an exported thunk `int <symbol>(int a0, …, int a7)`: it moves `$sp` down `0x20`,
    stores `a4`–`a7` at `sp+0x10`, sets `a0`–`a3`, runs `r_<ADDR>`, restores `$sp`, returns `$v0`.
  - A `jal` to a symbol in `native` becomes `ctx.r[2] = sym(a0, a1, a2, a3, mem_r32(sp+0x10), …, mem_r32(sp+0x1C))`.
  - **Code pointers are native addresses.** `jalr`/indirect `jr` call `((int (*)(int, …))reg)` with the same eight
    arguments. A `lui`/`addiu` (or `ori`) pair whose value is a function start in `funcs` becomes that function's thunk
    address. `recomp.py --patches` writes a table `{ram_address, native_function}` for every `.word <function>` in
    splat's data asm; `void patch_code_pointers(const CodePatch *p, int n)` (in `dispatch.c`) applies it after loading.
- [ ] **Step 1: Write `pc/test_bridge.c`**: recompile `cd_read` (`0x80033E34`) against a native `cd_start_request` stub
  and check its five arguments arrive (the fifth through the PS1 stack); call the recompiled `cd_read` thunk from C and
  check `$sp` is unchanged afterwards; `code_pointer_roundtrip`: after `patch_code_pointers`, `cd_state_handlers[17]`
  (`0x8004A678`) holds the native address of `cd_state_done`, and a recompiled `la` of `cd_seek` passed to a C function
  pointer call reaches the native `cd_seek`.
- [ ] **Step 2:** It fails.
- [ ] **Step 3:** Implement the thunks, native calls, pointer substitution and patch table.
- [ ] **Step 4:** `make pc-test` prints `ok`.
- [ ] **Step 5:** Commit.

### Task 4: The BIOS in C

**Files:**
- Create: `pc/bios.c`, `pc/test_bios.c`, `tools/test_bios_coverage.py`

**Interfaces:**
- Consumes: `Ctx`, the thunk convention.
- Produces: the recompiler turns each BIOS stub (`li $t2, 0xA0|0xB0|0xC0; jr $t2; li $t1, N`) into a call of
  `int bios_call(int table, int n)` (arguments in `ctx`); `bios.c` implements the 54 the exe uses (A0: 13 17 18 1B 27 2A
  2B 2E 2F 30 39 3F 44 49 70 72 9F A1 AB AC AD; B0: 07–0D 12–19 20 32 34–36 38 3F 41–43 45 4A–4C 4E 50 5B; C0: 0A) and
  `syscall` 1/2 (Enter/ExitCriticalSection). `void irq_raise(unsigned int bit)` runs the handlers registered with
  `SysEnqIntRP` and the events delivered by `DeliverEvent`, saving the whole `Ctx` before and restoring it after.
- [ ] **Step 1: Write `tools/test_bios_coverage.py`**: scan the exe for BIOS stubs and assert every `(table, n)` has an
  entry in `bios.c`'s table (it fails on a new one). **Write `pc/test_bios.c`**: an event opened and enabled runs its
  handler on `DeliverEvent`; `interrupt_preserves_context`: with all 32 registers set to known values,
  `irq_raise(1)` runs a recompiled handler that changes them, and afterwards every register, `hi` and `lo` are back.
- [ ] **Step 2:** Both fail.
- [ ] **Step 3:** Implement `bios.c`. Behaviour per psx-spx's BIOS function list; `printf`/`puts` go to stderr.
- [ ] **Step 4:** Both print `ok`.
- [ ] **Step 5:** Commit.

### Task 5: The hardware the libraries drive

**Files:**
- Create: `pc/hw.c`, `pc/test_hw.c`

**Interfaces:**
- Consumes: `irq_raise`, Task 1's `io_read`/`io_write` declarations, `gpu_gp0(unsigned int)`/`gpu_gp1(unsigned int)`/
  `unsigned int gpu_read(void)`/`unsigned int gpu_status(void)` from Task 6.
- Produces: `io_read`/`io_write` for interrupt status and mask (`0x1F801070`/`74`), DMA channels 2 (GPU, block and
  linked list), 4 (SPU, accepted at once), 6 (OT clear), DICR; root counters 0–2; SPU registers kept as memory with
  `SPUSTAT` mirroring `SPUCNT`'s low six bits and never busy; GPU ports (`0x1F801810`/`14`). DMA addresses are physical
  (`+ 0x80000000`). `void hw_poll(void)` raises vblank (bit 0) every `1/59.94` s of host time and finished DMA's
  interrupts; `void hw_set_clock(double (*now)(void))` lets tests drive time. Pads: the BIOS pad buffers are filled from
  `unsigned short pad_buttons` (active low, PS1 bit order), which Task 8 sets.
- [ ] **Step 1: Write `pc/test_hw.c`**: OT clear of 4 entries at `0x80100000` gives `0x00FFFFFF` at the first entry and
  each later one pointing to the one before; a linked-list DMA from it delivers two packets' words to `gpu_gp0` (the test
  links a recording one) in order and stops at `0xFFFFFF`; 60 vblanks in one second of test clock (±1).
- [ ] **Step 2:** It fails.
- [ ] **Step 3:** Implement `hw.c` per psx-spx's I/O map.
- [ ] **Step 4:** `make pc-test` prints `ok`.
- [ ] **Step 5:** Commit.

### Task 6: The software GPU

**Files:**
- Create: `pc/gpu.h`, `pc/gpu.c`, `pc/test_gpu.c`

**Interfaces:**
- Produces: `unsigned short vram[512][1024]`; `gpu_gp0`, `gpu_gp1`, `gpu_read`, `gpu_status` (always ready to receive
  commands and DMA); `typedef struct Display { int x, y, w, h, rgb24; } Display; Display gpu_display(void);`.
  Covers every GP0 command psx-spx lists: fill, VRAM upload/download/copy, polygons and lines (flat, Gouraud, textured
  4/8/15-bit with CLUT, raw texture, semi-transparency 0–3, mask set/check, dithering), rectangles and sprites, and the
  draw-mode, texture-window, drawing-area, offset and mask settings; GP1 reset, display enable, display start, ranges and
  mode (including 24-bit).
- [ ] **Step 1: Write `pc/test_gpu.c`**, pixel-exact cases from psx-spx's rules: `fill_aligns_to_16` (fill at x 3 width
  5 covers x 0–15); `upload_then_copy`; `triangle_fill_rule` (a flat triangle (0,0)-(4,0)-(0,4) sets 4, 3, 2 and 1
  pixels in rows 0–3: nothing on its right or bottom edge; if psx-spx's rule says otherwise, the test follows psx-spx); `sprite_4bpp_clut`;
  `semi_transparency_mode0` (`(B + F) / 2` per channel); `mask_check_skips`; `gouraud_dither` (the 4×4 dither offsets on
  a flat-colour Gouraud quad); `gpu_clip_and_oversize` (a triangle half outside the drawing area draws only the inside
  half; one with a 1024-pixel edge draws nothing).
- [ ] **Step 2:** It fails.
- [ ] **Step 3:** Implement `gpu.c`.
- [ ] **Step 4:** `make pc-test` prints `ok`.
- [ ] **Step 5:** Commit.

### Task 7: The GTE

`ENDING.X` alone has 73 GTE instructions and calls `RotTrans`, `RotMatrix` and `VectorNormal`, so the first screen
needs it.

**Files:**
- Create: `pc/gte.c`, `pc/test_gte.c`

**Interfaces:**
- Produces: `void gte_cmd(unsigned int op)` (every COP2 command: RTPS, RTPT, NCLIP, OP, DPCS, INTPL, MVMVA, NCDS,
  CDP, NCDT, NCCS, CC, NCS, NCT, SQR, DCPL, DPCT, AVSZ3, AVSZ4, GPF, GPL, NCCT, with the `sf` and `lm` bits),
  `unsigned int gte_read(int reg)`, `void gte_write(int reg, unsigned int v)`, and the `_ctrl` pair for the 32 control
  registers, including the FLAG bits and the register quirks psx-spx lists (IRGB/ORGB, LZCS/LZCR, the SXY FIFO, the
  16-bit sign extension of some registers).
- [ ] **Step 1: Write `pc/test_gte.c`**: identity rotation, translation (0,0,1000), H 1000 and a vertex (100,50,0)
  project RTPS to SX 100, SY 50; NCLIP of a clockwise and an anticlockwise triangle give opposite signs; MVMVA with
  `sf` set shifts by 12; a projection with SZ3 < H/2 sets the divide-overflow FLAG bit and saturates; AVSZ3 of
  SZ 100, 200, 300 with ZSF3 `0x555` gives OTZ 199; writing LZCS `0x0000FFFF` reads LZCR 16.
- [ ] **Step 2:** It fails.
- [ ] **Step 3:** Implement `gte.c` per psx-spx's GTE pages, including the unsigned Newton–Raphson divide.
- [ ] **Step 4:** `make pc-test` prints `ok`.
- [ ] **Step 5:** Commit.

### Task 8: The window

**Files:**
- Create: `tools/setup_sdl2.sh`, `pc/sdl.c`, `pc/test_display.c`
- Modify: `Makefile` (SDL2 include and link flags from `toolchain/SDL2`), `README.md` (how to build and run the PC
  build)

**Interfaces:**
- Consumes: `gpu_display`, `vram`, `pad_buttons`.
- Produces: `int sdl_open(void)`, `int sdl_frame(void)` (presents, reads input, returns 0 when the user quits);
  `void display_to_rgba(const Display *d, unsigned int *out)` (15-bit and 24-bit modes to `0xAARRGGBB`). Window
  640×480, display area scaled to fill it at 4:3; arrow keys, Z/X/A/S (×, ○, □, △), Enter (Start), Backspace (Select);
  F12 saves `screenshot.bmp`; Esc quits.
- [ ] **Step 1: Write `pc/test_display.c`**: a 15-bit pixel `0x7FFF` becomes `0xFFFFFFFF`, `0x001F` becomes
  `0xFFFF0000`; in 24-bit mode the three bytes at VRAM byte offsets 0–2 become one pixel `0xFF` R G B.
- [ ] **Step 2:** It fails.
- [ ] **Step 3:** Write `setup_sdl2.sh` (SDL2 2.30.x MinGW development release, i686 part, SHA-256 pinned; on Linux it
  prints the `apt` line for `libsdl2-dev:i386`). After the user OKs it, run it. Implement `sdl.c` and
  `display_to_rgba`.
- [ ] **Step 4:** `make pc-test` prints `ok`.
- [ ] **Step 5:** Commit.

### Task 9: The whole exe on the PC

**Files:**
- Create: `pc/main.c`
- Move: `native/cd_native.c`, `native/cd_native.h` → `pc/`; `native/cd_test.c` → `pc/test_cd.c`
- Modify: `Makefile`, `pc/cd_native.c`, `tools/native_syms.py` (only if the sound runner still needs it)

**Interfaces:**
- Consumes: everything above.
- Produces: `build/pc/ff7.exe ASSETS`: maps memory, loads `ASSETS/<disc 1>/SCUS_941.63`, recompiles nothing at run
  time (the build already generated `build/pc/gen/exe.c` from the user's exe), applies the code-pointer patches, opens
  the window, and calls the recompiled entry point `pc0` (`0x800110C0`, splat's `D_800110C0`, added to the function
  list by hand). The C compiled in: `main.c` (with `-Dmain=game_main`), `transition.c`, `loader.c`, `bitops.c`,
  `sound_init.c`, `sound.c`, `cd.c`, `movie.c`, `pc/cd_native.c`. Everything else in the exe is recompiled.
  `cd_native.c` drops `snd_ram` and `game_funcs`: RAM is at its address and the state table is patched; its movie stubs
  move to `test_cd.c`, since `movie.c` is linked now.
- [ ] **Step 1:** Port `test_cd.c` to the 32-bit runtime; `make pc-test` runs it and it fails until `cd_native.c` moves.
- [ ] **Step 2:** Move and adapt `cd_native.c`; `make pc-test` passes the CD test (10,052 files, 8,143 streamed, 0
  failures).
- [ ] **Step 3:** Generate and link `ff7.exe`. The link must have no undefined symbols; an exe call into the overlay
  range (`0x800A0000`–) reaches a placeholder that prints `no module for 800A0030` and exits.
- [ ] **Step 4:** Run `build/pc/ff7.exe assets`: it gets through `SetMem`, `cd_init` and the `ENDING.X` read, and stops
  at that message.
- [ ] **Step 5:** `make` still prints `build/SCUS_941.63: OK`. Commit.

### Task 10: `ENDING.X` and the first screen

**Files:**
- Create: `config/ending.yaml` (splat: `FIELD/ENDING.X`, vram `0x800A0000`, symbols shared with the exe's),
  `pc/modules.c`, `pc/test_modules.c`
- Modify: `Makefile`, `tools/recomp.py`, `docs/knowledge_base.md`, `docs/pc_port.md` (milestone 1 done)

**Interfaces:**
- Produces: per module, a table of its functions and its code-pointer patches; `void *module_function(unsigned int addr)`
  finds the module whose signature (the first 64 bytes of its first function) matches RAM, switches to it (applying its
  patches) and returns the function at `addr`. Every overlay address the exe calls (the `jal` targets at
  `0x800A0000`– in the exe and the C) gets an exported stub that goes through `module_function`.
- [ ] **Step 1: Write `pc/test_modules.c`**: `overlay_switch_by_content`: two fake modules sharing `0x800A0000`; after
  writing module B's bytes over A's, a call through the stub reaches B's function, and B's code pointers are patched.
- [ ] **Step 2:** It fails.
- [ ] **Step 3:** Implement `modules.c`, the overlay generation and the stubs.
- [ ] **Step 4:** `make pc-test` prints `ok`. Run `build/pc/ff7.exe assets`: the window shows `ENDING.X`'s first screen.
  Save it with F12 and give it to the user to compare with their PS1 or emulator.
- [ ] **Step 5:** Document what milestone 1 established (memory model, recompiler rules, what's emulated) in the
  knowledge base; `make` still prints OK. Commit.
