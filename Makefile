# Rebuild SCUS_941.63 from split assembly + recovered C, then check it against the retail hash.
# First time: tools/setup_toolchain.sh, then `make split`, then `make`.

ORIG_EXE ?= assets/Final Fantasy VII (USA) (Disc 1)/SCUS_941.63
DISC1 ?= assets/Final Fantasy VII (USA) (Disc 1)
PYTHON   := toolchain/venv/bin/python
GCC      ?= 2.7.2
CFLAGS   ?= -O2 -G0
ASPSX    ?= 2.79
ASFLAGS  := -EL -march=r3000 -mtune=r3000 -no-pad-sections -G0 -Iinclude
CROSS    := mipsel-linux-gnu-

# The object list comes from splat's linker script, so it only exists after `make split`.
OBJS := $(shell grep -o 'build/[^ ()]*\.o' build/main.ld 2>/dev/null | sort -u)
# The menu modules being decompiled (docs/superpowers/specs/2026-11-15-menu-decompile-design.md), each with its
# splat config in config/menu and checked against the disc's file. Each links at 0x801D0000 against the exe.
MENUS := chngmenu patymenu bginmenu limtmenu statmenu cnfgmenu formmenu namemenu mgicmenu itemmenu shopmenu savemenu eqipmenu
# The other overlays being decompiled, as name:file, each with its splat config in config/overlay.
OVERLAYS := dschange:FIELD/DSCHANGE.X ending:FIELD/ENDING.X batini:BATTLE/BATINI.X.dec batres:BATTLE/BATRES.X.dec \
	jet:MINI/JET.BIN.dec submar:MINI/SUBMAR.BIN.dec \
	highway:MINI/HIGHWAY.BIN.dec chocobo:MINI/CHOCOBO.BIN.dec condor:MINI/CONDOR.BIN.dec snobo:MINI/SNOBO.BIN.dec
# Every module the build relinks, as its config's path:the file it rebuilds.
MODULES := $(foreach m,$(MENUS),menu/$(m):MENU/$(shell echo $(m) | tr a-z A-Z).MNU) $(addprefix overlay/,$(OVERLAYS))
MODULE_FILES := $(foreach m,$(MODULES),build/$(word 2,$(subst :, ,$(m))))

check: build/SCUS_941.63 $(MODULE_FILES)
	@echo "a95e8b16b97071203b953bb81a33980509262f30  build/SCUS_941.63" | sha1sum -c
	@for m in $(MODULES); do set -- $$(echo $$m | tr : ' '); echo "$$(sed -n 's/^sha1: //p' config/$$1.yaml)  build/$$2"; \
		done | sha1sum -c

disks/SCUS_941.63:
	mkdir -p disks
	cp "$(ORIG_EXE)" $@

# The modules the PC port recompiles, as name:file on disc 1:load address (each the same on all three discs; a .dec
# is a packed one as the game unpacks it). `make split` splits each into asm/<name> with the splat config
# tools/module_yaml.py writes for it. The battle's effect files (MAGIC/*.BIN) all load at 0x801B0000.
PC_MODULES := ending:FIELD/ENDING.X:800A0000 dschange:FIELD/DSCHANGE.X:800A0000 field:FIELD/FIELD.BIN.dec:800A0000 \
	battle:BATTLE/BATTLE.X.dec:800A0000 batini:BATTLE/BATINI.X.dec:801B0000 batres:BATTLE/BATRES.X.dec:801B0000 \
	world:WORLD/WORLD.BIN.dec:800A0000 \
	$(foreach m,HIGHWAY CHOCOBO SNOBO CONDOR SUBMAR JET SNOBO2,\
		$(shell echo $(m) | tr A-Z a-z):MINI/$(m).BIN.dec:800A0000) \
	$(foreach m,BGIN CHNG CNFG EQIP FORM ITEM LIMT MGIC NAME PATY SAVE SHOP STAT,\
		$(shell echo $(m) | tr A-Z a-z)menu:MENU/$(m)MENU.MNU:801D0000) \
	$(shell ls "$(DISC1)/MAGIC" 2>/dev/null | awk '/\.BIN$$/ { sub(/\.BIN$$/, ""); \
		print "magic_" tolower($$0) ":MAGIC/" $$0 ".BIN:801B0000" }')
module = $(word $(2),$(subst :, ,$(filter $(1):%,$(PC_MODULES))))
PC_MODULE_NAMES := $(foreach m,$(PC_MODULES),$(firstword $(subst :, ,$(m))))

disks/%:
	mkdir -p $(dir $@)
	cp "$(DISC1)/$*" $@
disks/%.dec:
	mkdir -p $(dir $@)
	cp "$(DISC1)/$*" disks/$*
	$(PYTHON) tools/unpack.py disks/$*

split: disks/SCUS_941.63 $(foreach m,$(PC_MODULE_NAMES),disks/$(call module,$(m),2))
	$(PYTHON) -m splat split config/main.yaml
	mkdir -p build/modules
	for m in $(PC_MODULES); do set -- $$(echo $$m | tr : ' '); rm -rf asm/$$1; \
		$(PYTHON) tools/module_yaml.py disks/$$2 $$1 $$3 > build/modules/$$1.yaml && \
		$(PYTHON) -m splat split build/modules/$$1.yaml || exit 1; done
	mkdir -p build/menu build/overlay
	for m in $(MODULES); do set -- $$(echo $$m | tr : ' '); rm -rf asm/$$1; \
		$(PYTHON) -m splat split config/$$1.yaml || exit 1; done

build/SCUS_941.63: build/main.elf
	$(CROSS)objcopy -O binary $< $@

build/main.elf: $(OBJS) build/main.ld build/absolute_syms.ld config/module_entries.ld
	$(CROSS)ld -EL -T build/main.ld -T build/undefined_syms_auto.txt -T build/undefined_funcs_auto.txt \
		-T build/absolute_syms.ld -T config/module_entries.ld --no-check-sections -Map build/main.map -o $@

# A module calls the exe by its addresses: every name in the exe's ELF, splat's D_ and func_ ones included.
build/exe_syms.ld: build/main.elf
	$(CROSS)nm $< | awk '$$3 !~ /^\./ && $$2 != "a" {print $$3 " = 0x" $$1 ";"}' > $@
# $(1) is the module's config path (menu/chngmenu), $(2) the file it rebuilds (MENU/CHNGMENU.MNU).
define module_rules
build/$(1).elf: $$(shell grep -o 'build/[^ ()]*\.o' build/$(1).ld 2>/dev/null | sort -u) build/$(1).ld build/exe_syms.ld
	$$(CROSS)ld -EL -T build/$(1).ld -T build/exe_syms.ld -T build/$(1)_undefined_syms_auto.txt \
		-T build/$(1)_undefined_funcs_auto.txt --no-check-sections -Map build/$(1).map -o $$@
build/$(2): build/$(1).elf
	@mkdir -p $$(dir $$@)
	$$(CROSS)objcopy -O binary $$< $$@
endef
$(foreach m,$(MODULES),$(eval $(call module_rules,$(word 1,$(subst :, ,$(m))),$(word 2,$(subst :, ,$(m))))))

# splat leaves `absolute:True` symbols (BSS past the end of the file) for us to define.
build/absolute_syms.ld: config/symbols.main.txt
	grep 'absolute:True' $< | cut -d';' -f1 | sed 's/$$/;/' > $@

build/asm/%.o: asm/%.s
	@mkdir -p $(dir $@)
	$(CROSS)as $(ASFLAGS) $< -o $@

# The period cpp is 32-bit and cannot stat files on a Windows drive under WSL; preprocessing does not affect the
# generated code, so the host cpp does it.
build/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	cpp -P -undef -nostdinc -D__GNUC__=2 -Iinclude $< > build/src/$*.i
	toolchain/gcc-$(GCC)/cc1 -quiet $(CFLAGS) build/src/$*.i -o build/src/$*.s
	$(if $(JTBL_ALIGN),$(PYTHON) tools/jtbl_align.py build/src/$*.s)
	$(PYTHON) toolchain/maspsx/maspsx.py --aspsx-version=$(ASPSX) $(MASPSXFLAGS) < build/src/$*.s > build/src/$*.ms
	$(CROSS)as $(ASFLAGS) build/src/$*.ms -o $@

# The sound driver was assembled by an older ASPSX: `li 1` becomes `ori` (2.34 and older), and indexed stores
# expand through `addiu $$at` (2.21 and older).
build/src/main/sound.o: ASPSX = 2.21
# ...and compiled by GCC 2.6.3: 2.7.2 keeps a stored value in its register where the game reloads it (seq_cmd_loop_point).
build/src/main/sound.o: GCC = 2.6.3
# Its divides carry ASPSX's divide-by-zero and overflow checks.
build/src/main/sound.o: MASPSXFLAGS = --expand-div
# The boot file was assembled by ASPSX 2.21 (`li` as `ori`, indexed loads through `addiu $at`), the loader by 2.34
# (indexed loads fold into `%lo`). The boot file's divides carry ASPSX's checks.
build/src/main/main.o: ASPSX = 2.21
build/src/main/main.o: MASPSXFLAGS = --expand-div
# ...and compiled by GCC 2.6.3: 2.7.2 picks another register for func_800135C0's quotient.
build/src/main/main.o: GCC = 2.6.3
# The CD layer: GCC 2.6.3 and ASPSX 2.21, like the boot file.
build/src/main/cd.o: ASPSX = 2.21
build/src/main/cd.o: GCC = 2.6.3
build/src/main/cd.o: MASPSXFLAGS = --expand-div
build/src/main/movie.o: ASPSX = 2.21
build/src/main/movie.o: GCC = 2.6.3
build/src/main/movie.o: MASPSXFLAGS = --expand-div
build/src/main/transition.o: ASPSX = 2.34
build/src/main/loader.o: ASPSX = 2.34
build/src/main/loader.o: CFLAGS = -O2 -G8
build/src/main/loader.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/random.o: ASPSX = 2.34
build/src/main/random.o: CFLAGS = -O2 -G8
build/src/main/random.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/text.o: ASPSX = 2.34
build/src/main/text.o: CFLAGS = -O2 -G8
build/src/main/text.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/battle.o: ASPSX = 2.34
build/src/main/battle.o: CFLAGS = -O2 -G8
build/src/main/battle.o: MASPSXFLAGS = --use-comm-section -G8 --expand-div
build/src/main/inflate.o: ASPSX = 2.34
build/src/main/inflate.o: CFLAGS = -O2 -G8
build/src/main/inflate.o: MASPSXFLAGS = --use-comm-section -G8
# ...and the materia code by GCC 2.6.3 and ASPSX 2.21, like the sound driver.
build/src/main/materia_ap.o: GCC = 2.6.3
build/src/main/materia_ap.o: ASPSX = 2.21
build/src/main/materia_ap.o: CFLAGS = -O2 -G8
build/src/main/materia_ap.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/materia.o: GCC = 2.6.3
build/src/main/materia.o: ASPSX = 2.21
build/src/main/materia.o: CFLAGS = -O2 -G8
build/src/main/materia.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/materia_own.o: GCC = 2.6.3
build/src/main/materia_own.o: ASPSX = 2.21
build/src/main/materia_own.o: CFLAGS = -O2 -G8
build/src/main/materia_own.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/member_build.o: GCC = 2.6.3
build/src/main/member_build.o: ASPSX = 2.21
build/src/main/member_build.o: CFLAGS = -O2 -G8
build/src/main/member_build.o: MASPSXFLAGS = --use-comm-section -G8
# The Magic order sort by GCC 2.7.2 and ASPSX 2.34, the controller code by GCC 2.7.2 and ASPSX 2.21.
build/src/main/magic_order.o: ASPSX = 2.34
build/src/main/magic_order.o: CFLAGS = -O2 -G8
build/src/main/magic_order.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/pad.o: ASPSX = 2.21
build/src/main/pad.o: CFLAGS = -O2 -G8
build/src/main/pad.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/menu_prims.o: ASPSX = 2.21
build/src/main/menu_prims.o: CFLAGS = -O2 -G8
build/src/main/menu_prims.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/text_draw.o: ASPSX = 2.21
build/src/main/text_draw.o: CFLAGS = -O2 -G8
build/src/main/text_draw.o: MASPSXFLAGS = --use-comm-section -G8 --expand-div
build/src/main/party.o: ASPSX = 2.21
build/src/main/party.o: CFLAGS = -O2 -G8
build/src/main/party.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/results.o: ASPSX = 2.21
build/src/main/results.o: CFLAGS = -O2 -G8
build/src/main/results.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/menu.o: ASPSX = 2.21
build/src/main/menu.o: CFLAGS = -O2 -G8
build/src/main/menu.o: MASPSXFLAGS = --use-comm-section -G8 --expand-div
build/src/main/menu_util.o: ASPSX = 2.21
build/src/main/menu_util.o: CFLAGS = -O2 -G8
build/src/main/menu_util.o: MASPSXFLAGS = --use-comm-section -G8
build/src/main/menu_text.o: ASPSX = 2.21
build/src/main/menu_text.o: CFLAGS = -O2 -G8
build/src/main/menu_text.o: MASPSXFLAGS = --use-comm-section -G8 --expand-div
# The menu modules: GCC 2.7.2, -G0 (the exe's small globals through `lui`, not $gp). CHNGMENU's code is the same
# under ASPSX 2.21 and 2.34; 2.21 as for the exe's menu files.
build/src/menu/chngmenu.o build/src/menu/patymenu.o build/src/menu/bginmenu.o \
		build/src/menu/limtmenu.o build/src/menu/statmenu.o build/src/menu/cnfgmenu.o build/src/menu/formmenu.o build/src/menu/namemenu.o \
		build/src/menu/mgicmenu.o build/src/menu/itemmenu.o build/src/menu/shopmenu.o build/src/menu/savemenu.o \
		build/src/menu/savecard.o build/src/menu/saveload.o build/src/menu/eqipmenu.o build/src/menu/eqipmateria.o: ASPSX = 2.21
# DSCHANGE.X: GCC 2.7.2 and ASPSX 2.21 (`li` as `ori`), -G0, like the menus.
build/src/overlay/dschange.o: ASPSX = 2.21
# ENDING.X: GCC 2.6.3 and ASPSX 2.21 with its divide checks, like the CD layer and the movie player (2.7.2 fills the
# epilogue's delay slot).
build/src/overlay/ending.o: ASPSX = 2.21
build/src/overlay/ending.o: GCC = 2.6.3
build/src/overlay/ending.o: MASPSXFLAGS = --expand-div
# JET.BIN: the same, and -fforce-addr: a global used twice is reached through its address in a register.
build/src/overlay/jet.o: ASPSX = 2.21
build/src/overlay/jet.o: GCC = 2.6.3
build/src/overlay/jet.o: MASPSXFLAGS = --expand-div
build/src/overlay/jet.o: CFLAGS = -O2 -G0 -fforce-addr
# SUBMAR.BIN: GCC 2.6.3 and ASPSX 2.21 with its divide checks, at -G0.
build/src/overlay/submar.o: ASPSX = 2.21
build/src/overlay/submar.o: GCC = 2.6.3
build/src/overlay/submar.o: MASPSXFLAGS = --expand-div
build/src/overlay/submar.o: CFLAGS = -O2 -G0 -fforce-addr
# HIGHWAY.BIN: the same, but without -fforce-addr (it reloads globals by %hi/%lo every time).
build/src/overlay/highway.o: ASPSX = 2.21
build/src/overlay/highway.o: GCC = 2.6.3
build/src/overlay/highway.o: MASPSXFLAGS = --expand-div
build/src/overlay/highway.o: CFLAGS = -O2 -G0
# CHOCOBO.BIN is four objects. The main loop and the race: GCC 2.7.2 at -G0 with -fforce-addr and loops unrolled,
# ASPSX 2.56 (`li` as `addiu`, indexed loads folded into `%lo`) with its divide checks, and the jump tables aligned as
# its assembler did. The setup between them: the same at plain -O2 -G0. The figure code: HIGHWAY.BIN's toolchain.
build/src/overlay/chocobo.o build/src/overlay/chocobo_race.o: ASPSX = 2.56
build/src/overlay/chocobo.o build/src/overlay/chocobo_race.o: MASPSXFLAGS = --expand-div
build/src/overlay/chocobo.o build/src/overlay/chocobo_race.o: CFLAGS = -O2 -G0 -fforce-addr -funroll-loops
build/src/overlay/chocobo.o build/src/overlay/chocobo_race.o: JTBL_ALIGN = 1
build/src/overlay/chocobo_init.o: ASPSX = 2.56
build/src/overlay/chocobo_init.o: MASPSXFLAGS = --expand-div
build/src/overlay/chocobo_figure.o: ASPSX = 2.21
build/src/overlay/chocobo_figure.o: GCC = 2.6.3
build/src/overlay/chocobo_figure.o: MASPSXFLAGS = --expand-div
build/src/overlay/chocobo_figure.o: CFLAGS = -O2 -G0
# CONDOR.BIN: GCC 2.6.3 and ASPSX 2.21 with its divide checks, at -G0.
build/src/overlay/condor.o: ASPSX = 2.21
build/src/overlay/condor.o: GCC = 2.6.3
build/src/overlay/condor.o: MASPSXFLAGS = --expand-div
build/src/overlay/condor.o: CFLAGS = -O2 -G0
# SNOBO.BIN: GCC 2.6.3 and ASPSX 2.34 (an indexed global load keeps `%lo` in the load) with its divide checks.
build/src/overlay/snobo.o: ASPSX = 2.34
build/src/overlay/snobo.o: GCC = 2.6.3
build/src/overlay/snobo.o: MASPSXFLAGS = --expand-div
build/src/overlay/snobo.o: CFLAGS = -O2 -G0
# BATRES.X: ASPSX 2.34 (indexed loads fold into `%lo`) with its divide checks, like NAMEMENU's level-up code.
build/src/overlay/batres.o: ASPSX = 2.34
build/src/overlay/batres.o: MASPSXFLAGS = --expand-div
# BATINI.X: the same.
build/src/overlay/batini.o: ASPSX = 2.34
build/src/overlay/batini.o: MASPSXFLAGS = --expand-div
# NAMEMENU's level-up code is an object of its own: ASPSX 2.34 (indexed loads fold into `%lo`), with divide checks.
build/src/menu/namelevel.o: ASPSX = 2.34
build/src/menu/namelevel.o: MASPSXFLAGS = --expand-div
# The driver's first file (0x800293D0-0x80029A4C) reaches small globals through $gp: -G8. Its tentative definitions
# of them stay common, so the linker resolves them to the data section's labels instead of adding storage.
build/src/main/sound_init.o: ASPSX = 2.21
build/src/main/sound_init.o: GCC = 2.6.3
build/src/main/sound_init.o: CFLAGS = -O2 -G8
build/src/main/sound_init.o: MASPSXFLAGS = --expand-div --use-comm-section -G8

# The sound driver built for the PC (native/snd_native.c): `make native`, then tools/native_check.py.
# The driver copies seq_song into seq_song2, and the effect state from effect_voices on, as one block each; in snd_ram
# they stay adjacent as in the game, so GCC's out-of-bounds warnings about it are expected.
NATIVE_CFLAGS := -std=gnu99 -O2 -g -Wall -Wno-array-bounds -Wno-stringop-overflow -Wno-stringop-overread \
	-DNATIVE -DNON_MATCHING -Iinclude
NATIVE_DRIVER := build/native/sound.o build/native/sound_init.o

native: build/native/snd_native

build/native/%.o: src/main/%.c include/sound.h
	@mkdir -p $(dir $@)
	gcc $(NATIVE_CFLAGS) -c $< -o $@
# Its -G8 globals are tentative definitions; as common symbols they resolve to their place in snd_ram, as on the PS1.
build/native/sound_init.o: NATIVE_CFLAGS += -fcommon

build/native/snd_native.o: native/snd_native.c include/sound.h
	@mkdir -p $(dir $@)
	gcc $(NATIVE_CFLAGS) -c $< -o $@

build/native/ram.ld: $(NATIVE_DRIVER) build/native/snd_native.o config/symbols.main.txt tools/native_syms.py
	$(PYTHON) tools/native_syms.py build/native $(NATIVE_DRIVER) -- build/native/snd_native.o

# ram.ld's addresses are absolute (snd_ram plus an offset), so the program can't be position-independent.
build/native/snd_native: $(NATIVE_DRIVER) build/native/snd_native.o build/native/ram.ld
	gcc -no-pie $(NATIVE_DRIVER) build/native/snd_native.o build/native/funcs.c build/native/ram.ld -o $@

# The PC port (docs/pc_port.md): a 32-bit program with the PS1's RAM at its own address. From WSL the default compiler
# is the Windows MinGW one; on Linux: make pc-test PC_CC='gcc -m32' PC_LDFLAGS=-no-pie PC_EXE=
PC_CC ?= /mnt/c/winlibs/mingw32/bin/gcc.exe
PC_NM ?= /mnt/c/winlibs/mingw32/bin/nm.exe
# NATIVE turns INCLUDE_ASM off: the PC build recompiles whatever is still assembly.
PC_CFLAGS := -std=gnu99 -O2 -g -Wall -Iinclude -Ipc -DNATIVE
# Game addresses are absolute, so the program must load where it was linked: no ASLR.
PC_LDFLAGS ?= -Wl,--large-address-aware -Wl,--disable-dynamicbase
# SDL2 from tools/setup_sdl2.sh; a Linux build passes its own (sdl2-config --cflags / --libs).
SDL2_DIR ?= toolchain/SDL2
PC_SDL_CFLAGS ?= -I$(SDL2_DIR)/include/SDL2
PC_SDL_LIBS ?= -L$(SDL2_DIR)/lib -lSDL2
PC_EXE ?= .exe
PC_EXE_FILE := assets/Final Fantasy VII (USA) (Disc 1)/SCUS_941.63

build/pc/test_memory$(PC_EXE): pc/test_memory.c pc/memory.c pc/exe.c pc/memory.h
	@mkdir -p $(dir $@)
	$(PC_CC) $(PC_CFLAGS) pc/test_memory.c pc/memory.c pc/exe.c $(PC_LDFLAGS) -o $@

# The game's C keeps the dialect GCC 2.x compiled: undeclared functions are called as int f().
PC_GAME_CFLAGS := -std=gnu89 -Wno-implicit-function-declaration -Wno-implicit-int
# The game's C, built for the PC. main.c's main becomes game_main (the PC build has its own), and the -G8 files'
# tentative definitions stay common, so tools/pc_syms.py can place them at their game addresses.
build/pc/game/%.o: src/main/%.c
	@mkdir -p $(dir $@)
	$(PC_CC) $(PC_CFLAGS) $(PC_GAME_CFLAGS) -c $< -o $@
build/pc/game/main.o: PC_CFLAGS += -Dmain=game_main
build/pc/game/menu/%.o: src/menu/%.c
	@mkdir -p $(dir $@)
	$(PC_CC) $(PC_CFLAGS) $(PC_GAME_CFLAGS) -c $< -o $@
build/pc/game/overlay/%.o: src/overlay/%.c
	@mkdir -p $(dir $@)
	$(PC_CC) $(PC_CFLAGS) $(PC_GAME_CFLAGS) -c $< -o $@
build/pc/game/sound_init.o build/pc/game/loader.o build/pc/game/random.o build/pc/game/text.o build/pc/game/battle.o build/pc/game/inflate.o build/pc/game/materia_ap.o \
	build/pc/game/materia.o build/pc/game/materia_own.o \
	build/pc/game/member_build.o build/pc/game/magic_order.o build/pc/game/pad.o build/pc/game/text_draw.o build/pc/game/results.o \
	build/pc/game/menu.o build/pc/game/menu_util.o build/pc/game/menu_text.o: PC_CFLAGS += -fcommon
# The game reads a materia's bonus unset when no level has one, and always on Enemy Away's detail page.
build/pc/game/materia_own.o: PC_CFLAGS += -Wno-maybe-uninitialized
# HIGHWAY.BIN calls the exe's csqrt (an integer square root), not the host's complex one.
build/pc/game/overlay/highway.o: PC_CFLAGS += -fno-builtin-csqrt
build/pc/obj/%.o: pc/%.c pc/memory.h pc/recomp.h pc/gpu.h pc/cd_native.h pc/controls.h
	@mkdir -p $(dir $@)
	$(PC_CC) $(PC_CFLAGS) -c $< -o $@
# Generated from the user's exe: recompiled code compiles without the warnings hand-written C gets.
build/pc/obj/gen/%.o: build/pc/gen/%.c pc/memory.h pc/recomp.h
	@mkdir -p $(dir $@)
	$(PC_CC) $(PC_CFLAGS) -w -c $< -o $@

build/pc/main.nm: build/main.elf
	@mkdir -p $(dir $@)
	mipsel-linux-gnu-nm -n $< > $@

build/pc/gen/test_recomp.c: tools/recomp.py build/pc/main.nm
	$(PYTHON) tools/recomp.py --image "$(PC_EXE_FILE)" --symbols build/pc/main.nm 		--only copy_words,highest_bit,count_bits,lzs_decode --thunk-prefix rc_ -o $@

PC_TEST_RECOMP := build/pc/obj/test_recomp.o build/pc/obj/memory.o build/pc/game/bitops.o build/pc/game/cd.o 	build/pc/obj/gen/test_recomp.o
build/pc/test_recomp.ld: $(PC_TEST_RECOMP) tools/pc_syms.py
	$(PYTHON) tools/pc_syms.py $@ "$(PC_CC) $(PC_LDFLAGS)" $(PC_TEST_RECOMP)
build/pc/test_recomp$(PC_EXE): $(PC_TEST_RECOMP) build/pc/test_recomp.ld
	$(PC_CC) $(PC_TEST_RECOMP) build/pc/test_recomp.ld $(PC_LDFLAGS) -o $@

build/pc/test_bridge.native: build/pc/obj/test_bridge.o
	$(PC_NM) $< > $@
build/pc/gen/test_bridge.c: tools/recomp.py build/pc/main.nm build/pc/test_bridge.native
	$(PYTHON) tools/recomp.py --image "$(PC_EXE_FILE)" --symbols build/pc/main.nm --native build/pc/test_bridge.native 		--asm asm --only cd_read,cd_state_done --tag bridge -o $@

PC_TEST_BRIDGE := build/pc/obj/test_bridge.o build/pc/obj/memory.o build/pc/obj/exe.o build/pc/obj/dispatch.o 	build/pc/obj/gen/test_bridge.o
build/pc/test_bridge.ld: $(PC_TEST_BRIDGE) tools/pc_syms.py
	$(PYTHON) tools/pc_syms.py $@ "$(PC_CC) $(PC_LDFLAGS)" $(PC_TEST_BRIDGE)
build/pc/test_bridge$(PC_EXE): $(PC_TEST_BRIDGE) build/pc/test_bridge.ld
	$(PC_CC) $(PC_TEST_BRIDGE) build/pc/test_bridge.ld $(PC_LDFLAGS) -o $@

PC_TEST_BIOS := build/pc/obj/test_bios.o build/pc/obj/bios.o build/pc/obj/card.o build/pc/obj/memory.o build/pc/obj/dispatch.o
build/pc/test_bios$(PC_EXE): $(PC_TEST_BIOS)
	$(PC_CC) $(PC_TEST_BIOS) $(PC_LDFLAGS) -o $@

PC_TEST_MODULES := build/pc/obj/test_modules.o build/pc/obj/memory.o build/pc/obj/dispatch.o
build/pc/test_modules$(PC_EXE): $(PC_TEST_MODULES)
	$(PC_CC) $(PC_TEST_MODULES) $(PC_LDFLAGS) -o $@

PC_TEST_HW :=build/pc/obj/test_hw.o build/pc/obj/hw.o build/pc/obj/mdec.o build/pc/obj/spu.o build/pc/obj/memory.o
build/pc/test_hw$(PC_EXE): $(PC_TEST_HW)
	$(PC_CC) $(PC_TEST_HW) $(PC_LDFLAGS) -o $@

PC_TEST_GPU := build/pc/obj/test_gpu.o build/pc/obj/gpu.o
build/pc/test_gpu$(PC_EXE): $(PC_TEST_GPU)
	$(PC_CC) $(PC_TEST_GPU) $(PC_LDFLAGS) -o $@

PC_TEST_BUTTONS := build/pc/obj/test_buttons.o build/pc/obj/buttons.o build/pc/obj/gpu.o
build/pc/test_buttons$(PC_EXE): $(PC_TEST_BUTTONS)
	$(PC_CC) $(PC_TEST_BUTTONS) $(PC_LDFLAGS) -o $@

# The controls read SDL's key and button names, so their test links SDL and runs beside SDL2.dll.
PC_TEST_CONTROLS := build/pc/obj/test_controls.o build/pc/obj/controls.o
build/pc/obj/test_controls.o build/pc/obj/controls.o: PC_CFLAGS += $(PC_SDL_CFLAGS)
build/pc/test_controls$(PC_EXE): $(PC_TEST_CONTROLS) | build/pc/SDL2.dll
	$(PC_CC) $(PC_TEST_CONTROLS) $(PC_LDFLAGS) $(PC_SDL_LIBS) -o $@

PC_TEST_SPU := build/pc/obj/test_spu.o build/pc/obj/spu.o
build/pc/test_spu$(PC_EXE): $(PC_TEST_SPU)
	$(PC_CC) $(PC_TEST_SPU) $(PC_LDFLAGS) -o $@

PC_TEST_MDEC := build/pc/obj/test_mdec.o build/pc/obj/mdec.o
build/pc/test_mdec$(PC_EXE): $(PC_TEST_MDEC)
	$(PC_CC) $(PC_TEST_MDEC) $(PC_LDFLAGS) -o $@

PC_TEST_CARD := build/pc/obj/test_card.o build/pc/obj/card.o
build/pc/test_card$(PC_EXE): $(PC_TEST_CARD)
	$(PC_CC) $(PC_TEST_CARD) $(PC_LDFLAGS) -o $@

PC_TEST_LAB := build/pc/obj/test_lab.o build/pc/obj/lab.o build/pc/obj/memory.o build/pc/obj/display.o
build/pc/test_lab$(PC_EXE): $(PC_TEST_LAB)
	$(PC_CC) $(PC_TEST_LAB) $(PC_LDFLAGS) -o $@

PC_TEST_GTE := build/pc/obj/test_gte.o build/pc/obj/gte.o
build/pc/test_gte$(PC_EXE): $(PC_TEST_GTE)
	$(PC_CC) $(PC_TEST_GTE) $(PC_LDFLAGS) -o $@

PC_TEST_DISPLAY := build/pc/obj/test_display.o build/pc/obj/display.o build/pc/obj/gpu.o
build/pc/test_display$(PC_EXE): $(PC_TEST_DISPLAY)
	$(PC_CC) $(PC_TEST_DISPLAY) $(PC_LDFLAGS) -o $@

build/pc/test_cd.native: build/pc/obj/test_cd.o build/pc/game/cd.o build/pc/obj/cd_native.o
	$(PC_NM) $^ > $@
# No functions: just the table of code pointers in the exe's data, for the C this test links.
build/pc/gen/test_cd.c: tools/recomp.py build/pc/main.nm build/pc/test_cd.native
	$(PYTHON) tools/recomp.py --image "$(PC_EXE_FILE)" --symbols build/pc/main.nm --native build/pc/test_cd.native 		--asm asm --only none --tag cd -o $@

PC_TEST_CD := build/pc/obj/test_cd.o build/pc/game/cd.o build/pc/obj/cd_native.o build/pc/obj/memory.o 	build/pc/obj/exe.o build/pc/obj/dispatch.o build/pc/obj/gen/test_cd.o
build/pc/test_cd.ld: $(PC_TEST_CD) tools/pc_syms.py
	$(PYTHON) tools/pc_syms.py $@ "$(PC_CC) $(PC_LDFLAGS)" $(PC_TEST_CD)
build/pc/test_cd$(PC_EXE): $(PC_TEST_CD) build/pc/test_cd.ld
	$(PC_CC) $(PC_TEST_CD) build/pc/test_cd.ld $(PC_LDFLAGS) -o $@

# The game: its C, the PC runtime, and the rest of the exe recompiled from the user's copy.
PC_GAME := $(addprefix build/pc/game/,main.o transition.o loader.o bitops.o random.o text.o battle.o inflate.o materia_ap.o materia.o materia_own.o member_build.o magic_order.o pad.o menu_prims.o text_draw.o party.o results.o menu.o menu_util.o menu_text.o sound_init.o sound.o cd.o movie.o)
PC_GAME += $(foreach m,$(MENUS),build/pc/game/menu/$(m).o) $(if $(filter namemenu,$(MENUS)),build/pc/game/menu/namelevel.o) \
	$(if $(filter savemenu,$(MENUS)),build/pc/game/menu/savecard.o build/pc/game/menu/saveload.o) \
	$(if $(filter eqipmenu,$(MENUS)),build/pc/game/menu/eqipmateria.o)
PC_GAME += $(foreach o,$(OVERLAYS),build/pc/game/overlay/$(firstword $(subst :, ,$(o))).o)
PC_GAME += $(addprefix build/pc/game/overlay/,chocobo_init.o chocobo_race.o chocobo_figure.o)
PC_RUNTIME := $(addprefix build/pc/obj/,main.o memory.o exe.o dispatch.o bios.o hw.o mdec.o spu.o gpu.o buttons.o gte.o display.o cd_native.o sdl.o controls.o lab.o card.o)
build/pc/obj/sdl.o build/pc/obj/controls.o: PC_CFLAGS += $(PC_SDL_CFLAGS)
build/pc/ff7.native: $(PC_GAME) $(PC_RUNTIME)
	$(PC_NM) $^ > $@
build/pc/gen/exe.c: tools/recomp.py build/pc/main.nm build/pc/ff7.native
	$(PYTHON) tools/recomp.py --image "$(PC_EXE_FILE)" --symbols build/pc/main.nm --native build/pc/ff7.native 		--asm asm --rename main=game_main --tag exe -o $@
build/pc/gen/mod/%.c: tools/recomp.py build/pc/main.nm build/pc/ff7.native
	@mkdir -p $(dir $@)
	$(PYTHON) tools/recomp.py --image disks/$(call module,$*,2) --base $(call module,$*,3) --symbols build/pc/main.nm \
		--native build/pc/ff7.native --asm asm/$* --tag $* -o $@
.PRECIOUS: build/pc/gen/mod/%.c
build/pc/gen/module_list.c: Makefile
	@mkdir -p $(dir $@)
	echo '#include "recomp.h"' > $@
	for m in $(PC_MODULE_NAMES); do echo "extern const Module $${m}_module;"; done >> $@
	echo 'const Module *const pc_modules[] = {$(foreach m,$(PC_MODULE_NAMES),&$(m)_module,)};' >> $@
	echo 'const int pc_module_count = $(words $(PC_MODULE_NAMES));' >> $@
PC_GEN := build/pc/obj/gen/exe.o build/pc/obj/gen/module_list.o \
	$(foreach m,$(PC_MODULE_NAMES),build/pc/obj/gen/mod/$(m).o)
build/pc/ff7.ld: $(PC_GAME) $(PC_RUNTIME) $(PC_GEN) tools/pc_syms.py
	$(PYTHON) tools/pc_syms.py $@ "$(PC_CC) $(PC_LDFLAGS)" $(PC_GAME) $(PC_RUNTIME) $(PC_GEN) 		--nm $(PC_NM) --stubs build/pc/gen/modules.c
build/pc/gen/modules.c: build/pc/ff7.ld ; @: # an empty recipe: written with ff7.ld, so modules.o follows it
build/pc/ff7$(PC_EXE): $(PC_GAME) $(PC_RUNTIME) $(PC_GEN) build/pc/obj/gen/modules.o build/pc/ff7.ld | build/pc/SDL2.dll
	$(PC_CC) $^ $(PC_LDFLAGS) $(PC_SDL_LIBS) -o $@
build/pc/SDL2.dll: $(SDL2_DIR)/bin/SDL2.dll
	cp $< $@
pc: build/pc/ff7$(PC_EXE)

PC_TESTS := memory recomp bridge bios modules hw mdec spu gpu buttons controls gte display cd lab card
pc-test: $(foreach t,$(PC_TESTS),build/pc/test_$(t)$(PC_EXE)) build/pc/ff7$(PC_EXE)
	$(PYTHON) tools/test_recomp.py
	$(PYTHON) tools/test_module_yaml.py
	$(PYTHON) tools/test_lab_diff.py
	$(PYTHON) tools/test_bios_coverage.py "$(PC_EXE_FILE)"
	build/pc/test_memory$(PC_EXE) "$(PC_EXE_FILE)"
	build/pc/test_recomp$(PC_EXE) assets
	build/pc/test_bridge$(PC_EXE) "$(PC_EXE_FILE)"
	build/pc/test_bios$(PC_EXE)
	build/pc/test_modules$(PC_EXE)
	build/pc/test_hw$(PC_EXE)
	build/pc/test_mdec$(PC_EXE)
	build/pc/test_spu$(PC_EXE)
	build/pc/test_gpu$(PC_EXE)
	build/pc/test_buttons$(PC_EXE)
	cd build/pc && ./test_controls$(PC_EXE)
	build/pc/test_gte$(PC_EXE)
	build/pc/test_display$(PC_EXE)
	build/pc/test_cd$(PC_EXE) "$(PC_EXE_FILE)" assets
	cd build/pc && ./test_lab$(PC_EXE)
	cd build/pc && ./test_card$(PC_EXE)
	@# boots past the second sound load (frame ~680), which waits on an SPU transfer in C; 900 frames take 15 s
	timeout 60 build/pc/ff7$(PC_EXE) assets --frames 900 --shot build/pc/boot.bmp > /dev/null 2>&1

clean:
	rm -rf build asm

# The model editor's PS1 view (tools/model_editor.py): the software GPU as a library for the editor's Python, so built
# for the host as it is (64-bit). From WSL, a Windows gcc finds its own files by searching the Windows PATH, so the
# 64-bit MinGW goes first there. On Linux: make editor EDITOR_CC=gcc EDITOR_LIB=libps1gpu.so
EDITOR_CC ?= gcc.exe
EDITOR_BIN ?= /mnt/c/winlibs/mingw64/bin
EDITOR_LIB ?= ps1gpu.dll
editor: build/editor/$(EDITOR_LIB)
build/editor/$(EDITOR_LIB): pc/gpu.c pc/gpu.h pc/gte.c tools/ps1gpu.c
	mkdir -p build/editor
	WSLENV=PATH/l PATH="$(EDITOR_BIN):$$PATH" $(EDITOR_CC) -std=gnu99 -O2 -Wall -Ipc -shared -fPIC pc/gpu.c pc/gte.c \
		tools/ps1gpu.c -o $@

.PHONY: check split clean native pc pc-test editor
