"""Dump BATTLE/SCENE.BIN's 256 battle scenes to files people can edit, and build it back.

  python tools/scene.py dump "assets/Final Fantasy VII (USA) (Disc 1)" scene
  python tools/scene.py build scene assets

A scene is up to three enemies (their IDs, records and 32 attacks with names) and four battle setups, each with its
cameras and the six places enemies stand (by enemy ID); battle number N is scene N // 4, setup N % 4. The AI scripts
are dumped as hex until they're worked out, and bytes not named yet as unknown_XX. build writes SCENE.BIN into every
disc folder under assets, keeping the first original as .orig, and rebuilds KERNEL.BIN there with its table of the
blocks each scene sits in.
Names are up to 31 characters, in the text format tools/kernel.py describes.
"""
import argparse
import gzip
import json
import zlib
from pathlib import Path

import kernel
from unpack import bingz

SCENE_SIZE = 7808
BLOCK = 8192
ENEMIES, ENEMY_SIZE = 0x298, 184
ATTACKS, ATTACK_SIZE, ATTACK_IDS, ATTACK_NAMES = 0x4C0, 28, 0x840, 0x880
SETUPS, CAMERAS, FORMATIONS, AI = 0x008, 0x058, 0x118, 0xC80
SETUP_FIELDS = [(0x00, 2, "location"), (0x02, 2, "next_battle"), (0x04, 2, "escape"),
                *[(0x08 + 2 * i, 2, f"arena_{i + 1}") for i in range(4)], (0x10, 2, "flags"), (0x12, 1, "layout"),
                (0x13, 1, "camera_script")]
CAMERA_FIELDS = [(2 * i, 2, f, True) for i, f in enumerate(["x", "y", "z", "target_x", "target_y", "target_z"])]
FORMATION_FIELDS = [(0x0, 2, "enemy_id"), (0x2, 2, "x", True), (0x4, 2, "y", True), (0x6, 2, "z", True),
                    (0x8, 2, "row"), (0xA, 2, "cover"), (0xC, 4, "flags")]
# A record's fields past its name, as the battle code reads them (docs/knowledge_base.md, SCENE.BIN).
ENEMY_FIELDS = [
    *[(0x20 + i, 1, f) for i, f in enumerate(["level", "dexterity", "luck", "defense_pct", "strength", "defense",
                                              "magic", "magic_defense"])],
    *[(0x28 + i, 1, f"element_{i + 1}") for i in range(8)],
    *[(0x30 + i, 1, f"element_kind_{i + 1}") for i in range(8)],
    *[(0x38 + i, 1, f"animation_{i + 1}") for i in range(16)],
    *[(0x48 + 2 * i, 2, f"attack_{i + 1}") for i in range(16)],
    *[(0x68 + 2 * i, 2, f"camera_{i + 1}") for i in range(16)],
    *[(0x88 + i, 1, f"item_rate_{i + 1}") for i in range(4)], *[(0x8C + 2 * i, 2, f"item_{i + 1}") for i in range(4)],
    *[(0x94 + 2 * i, 2, f"manipulate_{i + 1}") for i in range(3)],
    (0x9C, 2, "mp"), (0x9E, 2, "ap"), (0xA0, 2, "morph"), (0xA2, 1, "back_damage"), (0xA4, 4, "hp"), (0xA8, 4, "exp"),
    (0xAC, 4, "gil"), (0xB0, 4, "statuses")]
ATTACK_FIELDS = kernel.LAYOUTS[1][2]  # a scene's attacks are the kernel's spell records


def read_scenes(data: bytes) -> list[bytes]:
    scenes = []
    for block in range(0, len(data), BLOCK):
        for i in range(16):
            offset = int.from_bytes(data[block + 4 * i:block + 4 * i + 4], "little")
            if offset != 0xFFFFFFFF:
                scenes.append(zlib.decompressobj(31).decompress(data[block + 4 * offset:block + BLOCK]))
    return scenes


def pack_scenes(scenes: list[bytes]) -> bytes:
    """8 KB blocks of up to 16 scenes: a u32 offset in words per scene (FF for none), then their gzip streams on
    4-byte boundaries, FF to the end. The game reads a scene's block alone, so none spans two."""
    blocks = [[]]
    for raw in scenes:
        gz = gzip.compress(raw, 9, mtime=0)
        gz += b"\xff" * (-len(gz) % 4)
        if len(blocks[-1]) == 16 or 64 + sum(map(len, blocks[-1])) + len(gz) > BLOCK:
            blocks.append([])
        blocks[-1].append(gz)
    out = bytearray()
    for streams in blocks:
        header, at = bytearray(), 64
        for gz in streams:
            header += (at // 4).to_bytes(4, "little")
            at += len(gz)
        if at > BLOCK:
            raise SystemExit(f"a scene packs to {len(streams[0])} bytes; a block has room for {BLOCK - 64}")
        out += (header.ljust(64, b"\xff") + b"".join(streams)).ljust(BLOCK, b"\xff")
    return bytes(out)


def name_text(field: bytes) -> str:
    return kernel.decode(field[:field.index(0xFF)])


def name_bytes(text: str) -> bytes:
    data = kernel.encode(text)
    if len(data) > 31:
        raise ValueError(f"{text!r} is {len(data)} bytes; a name has room for 31")
    return data.ljust(32, b"\xff")


def scene_json(raw: bytes) -> dict:
    table = {"enemy_ids": [int.from_bytes(raw[2 * i:2 * i + 2], "little") for i in range(3)], "setups": []}
    for k in range(4):
        setup = kernel.fields_json(raw[SETUPS + 20 * k:SETUPS + 20 * (k + 1)], SETUP_FIELDS)
        setup["cameras"] = [kernel.fields_json(raw[at:at + 12], CAMERA_FIELDS)
                            for at in range(CAMERAS + 48 * k, CAMERAS + 48 * (k + 1), 12)]
        setup["formation"] = [kernel.fields_json(raw[at:at + 16], FORMATION_FIELDS)
                              for at in range(FORMATIONS + 96 * k, FORMATIONS + 96 * (k + 1), 16)]
        table["setups"].append(setup)
    table["enemies"] = []
    for k in range(3):
        record = raw[ENEMIES + k * ENEMY_SIZE:ENEMIES + (k + 1) * ENEMY_SIZE]
        table["enemies"].append({"name": name_text(record[:32]), **kernel.fields_json(record, ENEMY_FIELDS, range(32))})
    table["attacks"] = [{"id": int.from_bytes(raw[ATTACK_IDS + 2 * k:ATTACK_IDS + 2 * k + 2], "little"),
                         "name": name_text(raw[ATTACK_NAMES + 32 * k:ATTACK_NAMES + 32 * (k + 1)]),
                         **kernel.fields_json(raw[ATTACKS + ATTACK_SIZE * k:ATTACKS + ATTACK_SIZE * (k + 1)],
                                              ATTACK_FIELDS)} for k in range(32)]
    table["ai"] = [raw[i:min(i + 16, SCENE_SIZE)].hex(" ") for i in range(AI, SCENE_SIZE, 16)]
    return table


def scene_raw(table: dict) -> bytes:
    raw = bytearray(SCENE_SIZE)
    raw[0:8] = b"".join(n.to_bytes(2, "little") for n in table["enemy_ids"]) + b"\xff\xff"
    for k, setup in enumerate(table["setups"]):
        raw[SETUPS + 20 * k:SETUPS + 20 * (k + 1)] = kernel.fields_raw(setup, 20, SETUP_FIELDS)
        raw[CAMERAS + 48 * k:CAMERAS + 48 * (k + 1)] = b"".join(kernel.fields_raw(c, 12, CAMERA_FIELDS)
                                                               for c in setup["cameras"])
        raw[FORMATIONS + 96 * k:FORMATIONS + 96 * (k + 1)] = b"".join(kernel.fields_raw(f, 16, FORMATION_FIELDS)
                                                                     for f in setup["formation"])
    raw[AI:] = bytes.fromhex("".join(table["ai"]))
    for k, enemy in enumerate(table["enemies"]):
        record = kernel.fields_raw(enemy, ENEMY_SIZE, ENEMY_FIELDS)
        record[:32] = name_bytes(enemy["name"])
        raw[ENEMIES + k * ENEMY_SIZE:ENEMIES + (k + 1) * ENEMY_SIZE] = record
    for k, attack in enumerate(table["attacks"]):
        raw[ATTACKS + ATTACK_SIZE * k:ATTACKS + ATTACK_SIZE * (k + 1)] = kernel.fields_raw(attack, ATTACK_SIZE,
                                                                                         ATTACK_FIELDS)
        raw[ATTACK_IDS + 2 * k:ATTACK_IDS + 2 * k + 2] = attack["id"].to_bytes(2, "little")
        raw[ATTACK_NAMES + 32 * k:ATTACK_NAMES + 32 * (k + 1)] = name_bytes(attack["name"])
    return bytes(raw)


def dump(disc: Path, out: Path):
    out.mkdir(parents=True, exist_ok=True)
    for n, raw in enumerate(read_scenes((disc / "BATTLE/SCENE.BIN").read_bytes())):
        (out / f"{n:03d}.json").write_text(json.dumps(scene_json(raw), indent=1, ensure_ascii=False) + "\n", "utf-8")


def build(folder: Path, assets: Path):
    scenes = []
    for path in sorted(folder.glob("[0-9][0-9][0-9].json")):
        try:
            scenes.append(scene_raw(json.loads(path.read_text("utf-8"))))
        except ValueError as e:
            raise SystemExit(f"{path}: {e}")
    if len(scenes) != 256:
        raise SystemExit(f"{folder}: {len(scenes)} scenes; the game has 256")
    data = pack_scenes(scenes)
    for disc in kernel.discs(assets):
        kernel.replace(disc, "BATTLE/SCENE.BIN", data)
        kernel.install(disc, bingz((disc / "INIT/KERNEL.BIN").read_bytes()))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("dump", help="SCENE.BIN from a disc folder into a folder of files")
    d.add_argument("disc", type=Path)
    d.add_argument("out", type=Path)
    b = sub.add_parser("build", help="a dump folder into every disc folder's SCENE.BIN (and KERNEL.BIN's table)")
    b.add_argument("dump", type=Path)
    b.add_argument("assets", type=Path)
    args = ap.parse_args()
    if args.command == "dump":
        dump(args.disc, args.out)
    else:
        build(args.dump, args.assets)


if __name__ == "__main__":
    main()
