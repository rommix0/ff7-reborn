# ff7_psx_recovery

Code recovery for **Final Fantasy VII (PS1, NTSC-U, SCUS-94163/4/5)**. The original source is lost; this repo
rebuilds it from the retail discs, plus scripts so owners of a legit copy can extract the game data themselves.

No game data is in this repo. `assets/` and any disc images are git-ignored. Bring your own discs.

## Extract your discs

Needs Python 3.10+, no other dependencies.

```bash
python tools/extract_disc.py "Final Fantasy VII (USA) (Disc 1).bin"
```

Files land in `assets/<image name>/`. Add `-l` to list files with LBA, size and SHA1 instead. Your disc 1
`SCUS_941.63` should hash to `a95e8b16b97071203b953bb81a33980509262f30`.

Then decompress what you need (overlays, `KERNEL.BIN`, field maps). The format is detected per file:

```bash
python tools/unpack.py "assets/Final Fantasy VII (USA) (Disc 1)/BATTLE/BATTLE.X"
```

## Build toolchain (Linux or WSL)

```bash
sudo apt install binutils-mipsel-linux-gnu python3 curl git
sh tools/setup_toolchain.sh
bash tools/match.sh src/main/bitops.c 80014A00
```

`match.sh` compiles a C file with the period compiler and checks it byte-for-byte against your extracted
`SCUS_941.63`.

## Layout

- `src/`: recovered C source, one folder per executable or overlay
- `tools/`: extractor scripts and the build/match tooling
- `docs/knowledge_base.md`: what we know about the discs, overlays, sound engine and what is left to recover
- `REFERENCES.md`: every source studied, its license, and what we may reuse

## License

MIT (see `LICENSE`) for the code and docs in this repo. Final Fantasy VII and all its data belong to Square Enix
and are not covered. Contributions must be original work: no code copied from copyleft or unlicensed sources
(see `REFERENCES.md`).
