# OpenDesperados

An open re-implementation of the engine of **Desperados: Wanted Dead or Alive** (Spellbound, 2001),
built for Android phones with touch controls. Written in C++17 with SDL2.

**No game data is included.** You need your own copy of the game (Steam / GOG); the app reads
the original files from your phone's storage.

## Status

| Milestone | State |
|---|---|
| 1. Asset loaders + touch map viewer (level select, pan, pinch-zoom, minimap) | ✅ done |
| 2. Characters, animals and animated scenery on the map, depth masking | ✅ done |
| 3. Walkable areas, pathfinding, tap to select / walk, double-tap to run, coloured minimap | ✅ done |
| 4. Enemy AI: patrols, vision cones, suspicion, alarm, chase, shooting, searching | ✅ first version |
| 5. Weapons: every hero's own gun (range/ammo from weapons.dat), knife / punch / kick, reload, KO, bodies | ✅ first version |
| 6. Special abilities (dynamite, sniper, mirror, ...), ladders/stairs, mission scripts | ⏳ next |
| 7. Dialogues, sound, HUD, the full campaign | ⏳ |

## Install on your phone

1. Open the [latest build](../../releases/tag/latest) on your phone and install `OpenDesperados.apk`
   (allow "install unknown apps" for your browser when Android asks).
2. Copy your game folder from the PC to the phone's **internal storage** and name it `Desperados`,
   so that you have `Internal storage/Desperados/data/levels/...`.
   The `.ogv` videos (~1 GB) are not used yet and can be left out.
3. Start the app and allow **All files access** when asked (needed to read that folder).

Controls: tap a hero to select, tap the ground to walk, double-tap to run, double-tap a hero to centre
on them. Drag to scroll, pinch to zoom, tap or drag the minimap to jump (tap a green dot to select
that hero). Stance button (bottom right): lie down / stand up; moving while down crawls.
Gun / melee buttons: tap one, then tap an enemy (the hero walks into range first).
Long-press an enemy to show their field of view (long-press the ground to hide). Android back button
returns to the level list.

## Build on a PC

```
cmake -B build -DCMAKE_BUILD_TYPE=Release   # needs SDL2 + zlib development packages
cmake --build build
./build/desperados --data "/path/to/Desperados Wanted Dead or Alive"
```

Options: `--level N` opens a level directly, `--size 1600x720` sets the window size,
`--autotest prefix` runs a scripted touch test and saves screenshots.

The Android APK is built automatically by GitHub Actions on every push to `main`
(`.github/workflows/android.yml`).

## Documentation

* [`docs/FORMATS.md`](docs/FORMATS.md) - what is known about the game's file formats.
* `tools/desp.py` - Python helpers used to research the formats.

Third-party code: [bzip2](third_party/bzip2/LICENSE), SDL2 (zlib license, downloaded at build time).
