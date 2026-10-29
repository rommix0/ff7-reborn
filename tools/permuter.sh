#!/bin/bash
# Set up a decomp-permuter directory for a sound.c function: tools/permuter.sh NAME SOURCE.c
# SOURCE.c holds the function's C (plus any declarations it needs); it is compiled against sound.h with sound.c's flags.
# Then run: PATH=$PWD/toolchain/permuter-venv/bin:$PATH toolchain/permuter-venv/bin/python toolchain/decomp-permuter/permuter.py build/perm/NAME -j8
# (the permuter looks for mips-linux-gnu-objdump; the venv links it to the mipsel one)
set -eu
cd "$(dirname "$0")/.."
name=$1 src=$2 d=build/perm/$1
mkdir -p $d
ln -sf "$(command -v mipsel-linux-gnu-objdump)" toolchain/permuter-venv/bin/mips-linux-gnu-objdump

{ echo '#include "common.h"'; echo '#include "sound.h"'; grep -v '^/\*prelude\*/$\|^/\*end\*/$' "$src"; } > $d/full.c
cpp -P -undef -nostdinc -D__GNUC__=2 -DPERMUTER -Iinclude $d/full.c > $d/base.c
toolchain/permuter-venv/bin/python toolchain/decomp-permuter/strip_other_fns.py $d/base.c $name

{ printf '.include "include/macro.inc"\n.set noat\n.set noreorder\n'; cat asm/nonmatchings/main/sound/$name.s; } > $d/target.s
mipsel-linux-gnu-as -EL -march=r3000 -mtune=r3000 -no-pad-sections -O1 -G0 -Iinclude $d/target.s -o $d/target.o

# Same pipeline as tools/match.sh, on the permuter's already-preprocessed candidate.
cat > $d/compile.sh <<EOF
#!/bin/bash
# compile.sh input.c -o output.o
set -e
t=\$(mktemp -d)
$PWD/toolchain/gcc-2.6.3/cc1 -quiet -O2 -G0 "\$1" -o \$t/f.s
python3 $PWD/toolchain/maspsx/maspsx.py --aspsx-version=2.21 --expand-div < \$t/f.s > \$t/f.ms
mipsel-linux-gnu-as -EL -march=r3000 -mtune=r3000 -no-pad-sections -O1 -G0 \$t/f.ms -o "\$3"
rm -rf \$t
EOF
chmod +x $d/compile.sh
printf 'func_name = "%s"\ncompiler_type = "gcc"\n' $name > $d/settings.toml
