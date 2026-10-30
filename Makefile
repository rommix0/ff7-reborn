# Rebuild SCUS_941.63 from split assembly + recovered C, then check it against the retail hash.
# First time: tools/setup_toolchain.sh, then `make split`, then `make`.

ORIG_EXE ?= assets/Final Fantasy VII (USA) (Disc 1)/SCUS_941.63
PYTHON   := toolchain/venv/bin/python
GCC      ?= 2.7.2
CFLAGS   ?= -O2 -G0
ASPSX    ?= 2.79
ASFLAGS  := -EL -march=r3000 -mtune=r3000 -no-pad-sections -G0 -Iinclude
CROSS    := mipsel-linux-gnu-

# The object list comes from splat's linker script, so it only exists after `make split`.
OBJS := $(shell grep -o 'build/[^ ()]*\.o' build/main.ld 2>/dev/null | sort -u)

check: build/SCUS_941.63
	@echo "a95e8b16b97071203b953bb81a33980509262f30  build/SCUS_941.63" | sha1sum -c

disks/SCUS_941.63:
	mkdir -p disks
	cp "$(ORIG_EXE)" $@

split: disks/SCUS_941.63
	$(PYTHON) -m splat split config/main.yaml

build/SCUS_941.63: build/main.elf
	$(CROSS)objcopy -O binary $< $@

build/main.elf: $(OBJS) build/main.ld build/absolute_syms.ld
	$(CROSS)ld -EL -T build/main.ld -T build/undefined_syms_auto.txt -T build/undefined_funcs_auto.txt \
		-T build/absolute_syms.ld --no-check-sections -Map build/main.map -o $@

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
build/src/main/transition.o: ASPSX = 2.34
build/src/main/loader.o: ASPSX = 2.34
build/src/main/loader.o: CFLAGS = -O2 -G8
build/src/main/loader.o: MASPSXFLAGS = --use-comm-section -G8
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

clean:
	rm -rf build asm

.PHONY: check split clean native
