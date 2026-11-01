import random

from kernel import encode, scene_table
from scene import SCENE_SIZE, pack_scenes, read_scenes, scene_json, scene_raw

# Scenes go into 8 KB blocks, at most 16 to a block: a u32 offset in words per scene (FF for none), then their gzip
# streams on 4-byte boundaries, FF to the end. One that won't fit starts the next block.
scenes = [bytes([n]) * SCENE_SIZE for n in range(20)]
packed = pack_scenes(scenes)
assert read_scenes(packed) == scenes and len(packed) == 2 * 8192
assert scene_table(packed)[:3] == bytes([0, 16, 0xFF])
noise = random.Random(7)
loud = [bytes(noise.randrange(256) for _ in range(SCENE_SIZE)) for _ in range(2)]
assert scene_table(pack_scenes(loud))[:3] == bytes([0, 1, 0xFF]) and read_scenes(pack_scenes(loud)) == loud

# A scene's names are 32 bytes, the text then FF to the end; the parts not worked out yet come back as they were.
scene = bytearray(noise.randrange(256) for _ in range(SCENE_SIZE))
scene[6:8] = b"\xff\xff"  # after the three enemy IDs
for name in [0x298 + 184 * k for k in range(3)] + [0x880 + 32 * k for k in range(32)]:
    scene[name:name + 32] = encode("-").ljust(32, b"\xff")
scene[0x298:0x298 + 32] = encode("Guard Hound").ljust(32, b"\xff")
scene[0x880:0x880 + 32] = encode("Bite").ljust(32, b"\xff")
table = scene_json(bytes(scene))
assert table["enemies"][0]["name"] == "Guard Hound" and table["attacks"][0]["name"] == "Bite"
assert scene_raw(table) == scene
table["enemies"][0]["name"] = "Hound"
assert scene_raw(table)[0x298:0x298 + 32] == encode("Hound").ljust(32, b"\xff")

# The enemy record's fields the battle code reads (docs/knowledge_base.md, SCENE.BIN); what's left stays unknown_XX.
hound = table["enemies"][0]
assert hound["level"] == scene[0x298 + 0x20] and hound["magic_defense"] == scene[0x298 + 0x27]
assert hound["item_2"] == int.from_bytes(scene[0x298 + 0x8E:0x298 + 0x90], "little")
assert hound["camera_16"] == int.from_bytes(scene[0x298 + 0x86:0x298 + 0x88], "little")
assert hound["statuses"] == int.from_bytes(scene[0x298 + 0xB0:0x298 + 0xB4], "little")
assert sorted(k for k in hound if k.startswith("unknown_")) == ["unknown_9A", "unknown_9B", "unknown_A3"] + \
    [f"unknown_{i:02X}" for i in range(0xB4, 0xB8)]

# Each setup carries its own four cameras and six formation entries; positions are signed.
setup = table["setups"][1]
assert setup["escape"] == int.from_bytes(scene[0x1C + 4:0x1C + 6], "little") and setup["layout"] == scene[0x1C + 0x12]
assert [k for k in setup if k.startswith("unknown_")] == ["unknown_06", "unknown_07"]
assert setup["cameras"][3]["target_z"] == int.from_bytes(scene[0x88 + 46:0x88 + 48], "little", signed=True)
assert setup["formation"][5]["x"] == int.from_bytes(scene[0x178 + 0x52:0x178 + 0x54], "little", signed=True)
assert setup["formation"][5]["flags"] == int.from_bytes(scene[0x178 + 0x5C:0x178 + 0x60], "little")
print("ok")
