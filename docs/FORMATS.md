# Desperados: Wanted Dead or Alive - file formats

Research notes for the 2018 THQ Nordic re-release (SDL2/OpenGL build, `desperados.exe` 32-bit,
timestamp 2018-07-19). All values are little endian. Pixels are RGB565 unless noted.
Pure green `0x07C0` is the transparent colour key in 16-bit images.

## Folder layout

| Path | Content |
|---|---|
| `data/levels/level_XX.dvm` | Level background (one big 16-bit image) |
| `data/levels/level_XX.dvd` | Level description: chunked (see below) |
| `data/levels/level_XX.scb` | Compiled mission script (bytecode) |
| `data/levels/level_XX.stf` | Text, first line = briefing file name (`D00BS05`) |
| `data/levels/briefing/d00bsXX` | Briefing slideshow (`GSVD` header, bzip2 image) |
| `data/characters/*.dvf` | Character sprites + animations |
| `data/animations/*.dvf` | Level-specific animated objects |
| `data/configuration/characters.dat` | Heroes and enemy profiles (stats) |
| `data/configuration/weapons.dat` | Weapon table |
| `data/interface/maps/*.map` | Campaign maps (zlib image) |
| `data/interface/*.pak` | Loading screens (several bzip2 images) |
| `data/interface/fonts/*.fnt` | Bitmap fonts (`SBFONT`) |
| `data/sounds/desperados.sbk` | Sound bank: id -> `SND_xxxx.wav` |
| `localisation/<lang>/data/interface/texts.res` | Strings (`SRES` / `TEXT`, UTF-16) |
| `localisation/<lang>/data/interface/default.res` | UI pictures (`SRES` / `PICC`, zlib) |
| `localisation/<lang>/data/configuration/eldiablo.cpg` | Campaign: INI text (level, cinematic, map position) |
| `data/cinematics/*.ogv` | Videos (Ogg Theora) |

## 16-bit image container (.dvm, .map, .pak) - decoded

Repeated until end of file:

```
u16 width
u16 height
u32 codec          1 = zlib, 2 = bzip2
u32 compressedSize
u8  data[compressedSize]   -> width*height RGB565 pixels
```

Level backgrounds range from 1280x768 (level 5) to 2560x1728 (level 18).

## Level description (.dvd) - container decoded

Sequence of chunks `char tag[4]; u32 length; u8 body[length]`. Order in every level:

| Tag | Probable meaning | State |
|---|---|---|
| `MISC` | general settings | raw |
| `BGND` | minimap: `u32 ver, u16 len, name, u16 w, u16 h, u32 codec, u32 csize, data` | **decoded** |
| `MOVE` | walkable sectors / motion areas (`DVSectorMotionArea`) | todo |
| `SGHT` | sight blockers (floats) | todo |
| `MASK` | depth masks: background pieces drawn in front of characters (see below) | **decoded** |
| `WAYS` | waypoints / patrol routes (`DVWaypoint`) | todo |
| `ELEM` | placed elements: characters, objects, animations (`DVElement*`) | positions decoded |
| `FXBK` | effect bank | todo |
| `MSIC` | music tracks (`green01.wav`, ...) | todo |
| `SND ` | ambient sound sources | todo |
| `PAT ` | patches (contains JPEG data) | todo |
| `BOND` | boundaries | todo |
| `MAT ` | ground materials (footstep sounds) | todo |
| `LIFT` | lifts / ladders | todo |
| `AI  ` | AI points | todo |
| `BUIL` | buildings (enter/exit, roofs) | todo |
| `SCRP` | script zones (`DVSectorScript`) | todo |
| `JUMP` | jump lines | todo |
| `CART` | carts / vehicles, cutscene paths | todo |
| `DLGS` | dialogues | todo |

## Sprites (.dvf) - decoded

```
u16 version (0x200), u16 frameCount, u16 0, u16 maxW, u16 maxH, zero padding up to 0x1E
frameCount x frame:
    u32 size (bytes after the next 6), u16 w, u16 h, u16 1
    h rows: s16 x0, s16 x1 (inclusive; (0,-1) = empty row), (x1-x0+1) RGB565 pixels
u16 setCount, per set:
    char name[32], u16 dirCount (16 or 1), 32 bytes ?, u32 animCount, 14 bytes ?,
    u16 maxW, u16 maxH, u32 anchorX, u32 anchorY, 20 bytes ?
    animCount*dirCount records (anim-major):
        u32 ?, u16 entryCount, u16 keyEntry, u16 flags, u32 anchorX, u32 anchorY,
        u16 dir, u16 animId, char name[32] (French, e.g. "Attendre", "Marcher"),
        entryCount x { u16 frame, u16 duration (ticks, 25 per second), s16 step (pixels moved),
                       s16 x, s16 y (frame top-left in the anchor box), u32 event (sound id) }
```

Pixel `0x001F` (blue) = transparent, `0x07C0` (green) = shadow (drawn as translucent black),
matching the colour-key / alpha-key in the original shader. Characters use an anchor of (70,71) =
the feet. Direction 0 faces up (north), increasing clockwise in 16 steps. Animation ids are global:
0 idle, 3 walk, 5 run, 7 lying, 9 crawl, 11 climb ladder, 33 die, ... Files in `data/animations`
hold several sets (one per animated scenery object of a level). All 154 files / 126,750 frames verified.

## Placed elements (ELEM chunk)

Found by signature (u16-length sprite file name + u16-length set name):

* scenery (`data/animations`): `s16 x, s16 y` (top-left of the anchor box), `s16 z` (height of the
  object's base, used for sorting: base = y+z), 3 flag bytes, optional script link.
* actors (`data/characters`): `u8 hasAlt [, alt file, alt set]`, two collision boxes
  (`u8 type + 4 x u16` each), `s16 x, s16 y` (top-left of the 140x142 anchor box, so the feet are at x+70, y+71), 6 bytes, `u8 floor`, `u8 direction`, then
  type-specific data (script class name, AI profile, ...).
* `Zombie` = invisible script target, `Accessories` = inventory items.

## Depth masks (MASK chunk) - decoded

```
u32 version, u16 groupCount, per group: u16 count, per mask:
    u8 flags
    [flags & 1]    u16 n, n * (s16 x, s16 y)   sort line along the object's base
    [flags & 2]    u16 n, n points              second line
    [flags & 0x10] s16 height
    s16 x, s16 y, s16 w, s16 h, u16 dataSize
    per row: u8 byteCount, runs: c >= 0x80 -> repeat next byte (c-0x80) times, else copy c bytes
             (1 bit per pixel, MSB first)
```

A character whose feet are above (smaller y than) a mask's sort line at its x is behind that
object, so the masked background pixels are drawn again over the character. Group 0 = ground,
higher groups match the actor's floor value. 7,377 masks in 25 levels verified.

## Scripts (.scb)

Text header (`version 1.00`, classes, variables, functions with addresses) followed by
binary "quads" (three-address code). Class names come from the level editor
(`D:\DEATHVALLEY\BOOGIE_EDITEUR\script.scs`). Interpreted in the original by
`SBSimpleVirtualMachine` / `VMCore`. Script interfaces seen in the executable:
`IActorNPCScript`, `IElementTargetScript`, `IEngineScript`, `IWaypointScript`, `IZoneScript`.

## Executable

The engine runs its logic at 25 Hz (see `readme.txt`, NOSMOOTHMOUSE). The binary keeps
159 RTTI class names, which outline the engine:
AI (`DVArtificialIntelligence`, `DVArtificialMalignity`, `DVArtificialBonhomie`, `DVWill`,
`DVPsychoanalyst`), map (`DVSector*`, `DVLine*`, `DVPoint*`, `DVFastFindGrid`),
actors (`DVElementActorPC/NPC/Villain/Civilian/Horse/Animal`, `DVCooper`, `DVDoc`, `DVSanchez`, ...),
orders (`DVOrder`, `DVSequence*`), scripts (`DVScript`, `VMCore`).
