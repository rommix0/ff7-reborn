#!/bin/sh
# Fetch the pinned compilers and assembler shim into toolchain/ (git-ignored). Run on Linux or WSL.
set -eu
cd "$(dirname "$0")/.."

OLD_GCC=https://github.com/decompals/old-gcc/releases/download/0.17
MASPSX_REV=4ffa8b5106f9780d5d4aa7609f184949433ddbbc

for v in 2.6.3 2.7.2; do
    dir=toolchain/gcc-$v
    [ -e "$dir/cc1" ] && continue
    mkdir -p "$dir"
    curl -fsSL "$OLD_GCC/gcc-$v-psx.tar.gz" -o "toolchain/gcc-$v.tar.gz"
    (cd toolchain && sha256sum -c --quiet "../tools/toolchain.sha256" --ignore-missing)
    tar xzf "toolchain/gcc-$v.tar.gz" -C "$dir"
    rm "toolchain/gcc-$v.tar.gz"
done

if [ ! -d toolchain/maspsx ]; then
    git clone -q https://github.com/mkst/maspsx toolchain/maspsx
    git -C toolchain/maspsx checkout -q "$MASPSX_REV"
fi

command -v mipsel-linux-gnu-as >/dev/null || {
    echo "missing GNU binutils for MIPS: sudo apt install binutils-mipsel-linux-gnu" >&2
    exit 1
}
echo "toolchain ready"
