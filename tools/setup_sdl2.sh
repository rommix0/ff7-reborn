#!/bin/sh
# Fetch SDL2's MinGW development release (its i686 part) into toolchain/SDL2 (git-ignored) for the PC build's window,
# sound and pads. Run on Linux or WSL. A native Linux build uses the distribution's 32-bit SDL2 instead.
set -eu
cd "$(dirname "$0")/.."

SDL2=2.30.12
[ -e toolchain/SDL2/lib/libSDL2.dll.a ] && exit 0
mkdir -p toolchain
curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2/SDL2-devel-$SDL2-mingw.tar.gz" \
    -o "toolchain/SDL2-devel-$SDL2-mingw.tar.gz"
(cd toolchain && sha256sum -c --quiet ../tools/toolchain.sha256 --ignore-missing)
tar xzf "toolchain/SDL2-devel-$SDL2-mingw.tar.gz" -C toolchain "SDL2-$SDL2/i686-w64-mingw32"
mv "toolchain/SDL2-$SDL2/i686-w64-mingw32" toolchain/SDL2
rm -r "toolchain/SDL2-$SDL2" "toolchain/SDL2-devel-$SDL2-mingw.tar.gz"
echo "SDL2 $SDL2 is in toolchain/SDL2. For a native Linux build instead: sudo apt install libsdl2-dev:i386"
