// DVPatch (Game/DVPatch.cpp): a change of the scenery, from the PAT hunk. A patch is a piece of
// the background (a JPEG picture) put over the map when the patch is applied, and with it masks,
// sight obstacles and click areas that come and go (a gate that opens, a roof that disappears
// while a hero is in the house, a bridge blown up). It is applied by the scripts (ApplyPatch),
// when someone goes through its door, or when a hero walks into its sector; an animation of the
// scenery (a DVElementFX) can play the change.
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "sb/SBGeo.h"

class SBFile;
class DVFastFindGrid;
class DVMask;
class DVSector;
class DVSightObstacle;
class DVElementFX;
class DVElementActor;
class DVDoor;

class DVPatch {
public:
    DVPatch();
    ~DVPatch();
    int LoadFromFile(SBFile& f, DVFastFindGrid& grid);  // 0x08326eb0

    void Apply();               // 0x08328120: on, or back off
    void ApplyFinal();          // 0x08328020: the end of the change (its animation done)
    void ForceReset();          // 0x08328fc0: off now
    void Enter(DVElementActor* a);  // 0x083290d0: a hero in its sector
    void Leave(DVElementActor* a);  // 0x083291d0

    bool applied = false;       // +0
    bool permanent = false;     // +1: once on, it stays
    bool trigger = false;       // +2: the hero's sector applies it
    bool triggerInit = false;   // +3
    bool u4 = false;            // +4
    bool hasFx = false;         // +7
    bool fxOff = false;         // +8: the element shown while off (animation 0x94)
    bool fxChange = false;      // +9: ... the change (0x95)
    bool fxOn = false;          // +0xa: ... while on (0x96)
    bool animating = false;     // +0xb
    DVElementFX* fx = nullptr;  // +0x18
    SBGeoPoint2D at;            // +0xc: the picture's corner on the map
    uint32_t surface = 0xffffffff;  // +0x14: the picture (swapped with the background's)
    std::vector<DVMask*> masksOff, masksOn;            // +0x1c / +0x40: active while off / on
    std::vector<DVSightObstacle*> obstaclesOff, obstaclesOn;  // +0x64 / +0x88
    std::vector<DVSector*> sectorsOff, sectorsOn;      // +0xac / +0xd0
    uint16_t layer = 0;         // +0x13c
    uint16_t u13e = 0;          // +0x13e
    SBGeoPoint2D point;         // +0x140
    DVSector* zone = nullptr;   // +0x148: the sector a hero walks into
    DVSector* zone2 = nullptr;  // +0x14c
    DVDoor* door = nullptr;     // +0x158: its lock flags change with the patch
    uint8_t doorFlags[4] = {};  // +0x15c (pc, doc, villain, civilian locked)
    std::vector<DVElementActor*> inside;  // +0x160

private:
    void SwapBackground();      // 0x083282d0
    void SwapObjects(bool on);  // 0x08328950
    void SwapDoors();           // 0x08327fb0
    void ShowFx(uint32_t anim, bool fromEnd);
    std::vector<std::unique_ptr<DVSector>> own;
};
