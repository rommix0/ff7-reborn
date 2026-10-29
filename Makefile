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
	$(PYTHON) toolchain/maspsx/maspsx.py --aspsx-version=$(ASPSX) < build/src/$*.s > build/src/$*.ms
	$(CROSS)as $(ASFLAGS) build/src/$*.ms -o $@

# The sound driver was assembled by an older ASPSX: `li 1` becomes `ori` (2.34 and older), and `$$at` expansions
# have no leading nop (2.30 and newer).
build/src/main/sound.o: ASPSX = 2.34

clean:
	rm -rf build asm

.PHONY: check split clean
