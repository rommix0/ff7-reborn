# Scene scripts: the models acting out a script, recorded (draft)

## Intent

For the explainer video: write down what the characters do, shot by shot, and get footage of it without playing it
by hand. The script should be text, live in the repo next to the video's plan, and replay the same way every run.

## What's there already

- **The game's own scripts.** Every map's characters already follow scripts: `tools/field.py dump` writes each map
  as JSON, a script a line per operation with the field module's own names (`move`, `dir`, `turnr`, `anime`,
  `xyzi`, `visi`, `mes`, `wait`, `scr2d`, `fade`, `music`, `se` ...), and `field.py build` writes edited maps back
  into `assets/`, keeping the originals as `.DAT.orig`. 222 operations are in use across the maps.
- **The lab** warps to any map (`--field N`), plays a pad script (`--input`), and records headless: `--shots 1 N`
  for every frame, `--wav` for the sound, ffmpeg for the MP4.
- **The Battles tab** already plays a battle's action scripts and attack cameras in the editor, and "play in the
  lab" has the game itself play a command, effect and all.

## The approach: scenes as field scripts, played by the game

A scene is one map's JSON with a script added: an entity whose start-up script places the actors (`xyzi`, `dir`,
`visi`), then runs the shot list (walk with `move`, turn, play an animation, a line of dialogue with `mes`, waits,
camera scrolls, fades, music and sound). `field.py build` puts it in, the lab warps there and records it, ffmpeg makes
the clip. What comes out is exactly what the game draws: its backgrounds, lighting, text boxes and sound.

A small tool ties it together: `tools/scene.py SCENE.json` (the map, its added script, the save or new game to start
from, how long to record) builds the map, runs the lab, makes the MP4, and puts the originals back.

### Steps

1. **One shot by hand:** in `MD1STIN`, Cloud walks to a point, turns, plays an animation and says a line; recorded
   to an MP4. This finds which operations' operands still need working out (many are raw hex today).
   **Done (2026-11-04):** Cloud's new script (`msped`, `move`, `turnr`, `wait`, `anime`, `animw`, `wsize`, `mes`)
   asked for by the director with `reqew 01 45` after Barret's line; recorded with `--from 10720 --shots 1 720
   --wav` and ○ at frame 10,800, 12 s. What it showed: the camera stays put, so walking away from it shrinks the
   actor fast; `turnr`'s and `anime`'s other operands were copied from other maps, not yet read from their
   handlers; animation numbers are per model (Cloud's 3 is his jump off the train); `{EA}` before the naming prints
   "Ex-SOLDIE"; a `mes` waits for ○, so a scene's last line holds until the recording stops.
2. **Operands named** for the ones scenes use (`move`, `dir`, `turnr`, `anime`/`animw`, `xyzi`, `mes`, `scr2d`,
   `fade`): read from their handlers in the field module, written into `field.py`'s text so a scene reads as
   words and numbers, not hex.
3. **`tools/scene.py`** as above, with a test that a scene builds and the map comes back byte for byte afterwards.

## Non-goals

- A new renderer or engine: the game plays the scenes.
- Changing the game for players: scene maps are built only for recording and put back after.

## Limits that come with it

- A map's camera is its pre-rendered background: it can scroll, not orbit. Close-ups, turntables and "inside the
  model" shots (bones, polygons, wireframe) need the editor instead (open point 2).
- Actors stay on the walkmesh, and a map has room for 77,852 bytes unpacked.
- Battles keep their own scripts: a battle shot is still a command played in the lab, as now.

## Open

1. Is this the "script the models follow" you meant, or did you mean the model editor posing and animating models on
   a timeline of its own?
2. Do you want the shots the game can't take (free camera, close-ups, exploded models)? That's a timeline in the
   editor drawing through `pc/gpu.c`: a second, bigger piece, after this one.
3. Which first scene would you like? It sets which operations get named first.
