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
| `MASK` | occlusion polygons: what is drawn in front of characters | todo |
| `WAYS` | waypoints / patrol routes (`DVWaypoint`) | todo |
| `ELEM` | placed elements: characters, objects, animations (`DVElement*`) | todo |
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

## Sprites (.dvf) - frames decoded

Header (to be decoded), then frames:

```
u32 size
u16 width, u16 height
u16 unknown (1)
height x row:  u16 x0, u16 x1 (inclusive), (x1-x0+1) RGB565 pixels
```

`0x001F` (pure blue) inside sprites appears to mark shadow pixels. Animation tables follow
the frame block (not decoded yet).

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
