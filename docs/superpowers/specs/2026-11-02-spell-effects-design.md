# Spell effects in the Battles tab

## What

Playing an action in the Battles tab shows its effect as the game draws it: the spell, summon, limit or item effect,
with the moves and camera the tab already plays.

## Why it isn't like the rest

The moves and cameras were data the battle interprets (action scripts, `CAMDAT`), so `tools/battle.py` could play
them. Effects are code: each is a `MAGIC/*.BIN` overlay (289 of them, 0x300-0xEB00 bytes of MIPS each, 1.8 MB in all)
loaded at `0x801B0000`, entered from `BATTLE.X`'s table `D_800EFAF0` (`func_800D0C80`), that builds its own sprites,
models and GPU packets each frame. A Python player would mean rewriting 289 programs.

## Approach: the lab plays it

The port already runs every effect: all 289 are recompiled into `PC_MODULES`. So the game's own code plays the effect
and the editor shows the result.

1. **`--battle N`** in the lab: from the field, ask for battle N the way an encounter does (request 2 with its
   number, `0x8009ABF5`/`0x8009ABF6`), as `--field` asks for a warp.
2. **`--command actor,command,attack,targets`**: once the battle's menu waits on that member, put the command in as
   the menu does (`func_800A4350`): the next entry of the 16-entry ring at `D_800F3958`, the member off the menu's
   list, its bit moved from waiting to chosen. The battle then plays everything: script, camera, effect, damage and
   sound. An item isn't used up.
3. **`--shots K N`**: N pictures of the screen, K frames apart, from when the command goes in; then the lab exits.
4. **Editor:** "play in the lab" beside "play action". It runs the lab headless (new game, or Continue on a memory
   card's file 01) with the battle, the command and a frame count, then plays the pictures in the tab at 15 a second.
5. **How it works, written down:** how an effect plugs into the battle (the entry table, what it's handed, the tasks
   it starts, how it draws and when it ends), with Ice (`BRIZAD.BIN`) as the worked example in
   `docs/knowledge_base.md`.

## Non-goals

- Rewriting effects in Python. One or two could be done later as showcases.
- Editing effects; XM.
- Enemy attacks through the lab (their turns go through the same ring as command `FF`, their AI).

## Costs and limits

- Needs the lab built (`make pc`). An action takes about 10 seconds from boot.
- The party, stats and equipment are the save's; the command skips the menu, so any spell can be cast whatever
  materia is equipped. Damage, misses and the enemies' turns are whatever the game rolls.
- A save must be in a field: a battle is asked for from one.

## Decided

1. The lab plays effects (the user went ahead with it).
2. A new game by default (Cloud alone, any battle); a card from `saved_games` for other parties (outside the repo).
3. The lab's pictures alone, not beside the tab's own render.
4. The Ice walkthrough in the same change.
