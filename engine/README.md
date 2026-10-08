# The new Desperados engine

A native engine for Desperados: Wanted Dead or Alive, written by translating the original game's
own code (the Linux build `desperados32` still carries the names of all its classes and functions)
into C++ that runs on the phone directly, with no emulator. It reads the player's own game files.

* `src/sb/` the original's base library ("SB"): files, pictures, the drawing layer.
* `src/dv/` the game ("DV"): engine, elements, sprites, masks, sectors...
* `src/app/` the phone side (our own): window, touch, the smooth camera.
* `tools/dvrender.cpp` desktop check: draws a mission to a picture.
* `tools/dvmove.cpp` desktop check: orders a hero somewhere and writes frames of the walk
  (with the walls, the path finder's corners and the planned way drawn on the first one).
* `attic/first-pass/` an earlier, unfinished pass at the geometry classes, kept for reference.

Every translated function carries the original's name and address in a comment
(`// DVSprite::CreateTargetSprite (0x0839fbd0)`); fields note the offset they had in the original
object (`// +0x22c`). The rule is to keep the original's behaviour, including its quirks.

Desktop build: `cmake -S engine -B engine/build-desk && cmake --build engine/build-desk`, then
`xvfb-run engine/build-desk/dvrender <game folder> level_01 out.ppm 1280 720`.

## Steps

1. Missions drawn like the original, smooth camera and touch.
2. The heroes: selecting, walking / running / crawling along the original's path finder
   (`DVPathFinder`: the corner graph stored in the MOVE hunk, A* over the corners, the way round
   each corner through its docking places), lying down and standing up, idle animations.
3. Enemies (to come): patrols, vision cones, alarms.
4. The script engine (to come): intros, cutscenes, dialogue, objectives.
5. The game's interface, menus, saves, sound, the whole campaign (to come).
