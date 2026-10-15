// DVDoor (Game/DVDoor.cpp): a passage between two areas. A house's doors lead from the street
// into the house (DVSectorBuilding), a lift's two doors from the floor below and the floor above
// onto the stairs / the ladder / the wall (the lift is an area of the last layer), and the other
// doors (gates) join two walkable areas. Passing one: walk to its middle (+0x24) with its first
// animation, change area (DVElementActor::PassDoor), walk on to its other end with the second.
#pragma once
#include <cstdint>

#include "sb/SBGeo.h"

class SBFile;
class DVSector;
class DVSectorMotionArea;
class DVElementActor;
class DVFastFindGrid;

enum : uint32_t {
    DOOR_DEFAULT = 0,
    DOOR_BUILDING = 1,       // a house's door
    DOOR_BUILDING_VILLAINS = 2,
    DOOR_GATE = 3,
    DOOR_LIFT_TOP = 4,       // a lift's upper door (DVSectorLift::LoadFromFile)
    DOOR_LIFT_BOTTOM = 5,
};

class DVDoor {
public:
    // DVDoor::LoadFromFile (0x0835ac50); `building` = the house it belongs to (nullptr: a gate)
    int LoadFromFile(SBFile& f, DVFastFindGrid& grid, DVSectorMotionArea* building);
    // DVDoor::IsActorAutorized (0x0835a310): may he pass it that way (enter = from the first side)
    bool IsActorAutorized(bool enter, const DVElementActor* a) const;
    // DVDoor::GetAction1 / GetAction2 (0x0835a730 / 0x0835a7b0): the animations of the two halves
    // of the passage for a move with `anim` (3 walk, 5 run, 9 crawl)
    uint32_t Action1(bool enter, uint32_t anim) const;
    uint32_t Action2(bool enter, uint32_t anim) const;
    SBGeoPoint2D Entry(bool enter) const { return enter ? pA : pB; }
    SBGeoPoint2D Exit(bool enter) const { return enter ? pB : pA; }
    DVSectorMotionArea* To(bool enter) const { return enter ? b : a; }
    uint16_t ToLayer(bool enter) const { return enter ? layerB : layerA; }

    uint32_t type = DOOR_DEFAULT;   // +0
    bool enabled = true;            // +4
    bool pcLocked = false;          // +5
    bool villainLocked = false;     // +6
    bool civilianLocked = false;    // +7
    bool docPickable = false;       // +8 Doc can pick its lock
    DVSectorMotionArea* a = nullptr;  // +0xc the first side (the street; a lift's floor)
    DVSectorMotionArea* b = nullptr;  // +0x10 the second side (the house; the lift)
    SBGeoPoint2D pA;                // +0x14 where one stands on the first side
    SBGeoPoint2D pB;                // +0x1c ... on the second side
    SBGeoPoint2D mid;               // +0x24 the door itself
    uint16_t layerA = 0;            // +0x2c
    uint16_t layerB = 0;            // +0x2e
    uint32_t act[4] = {3, 3, 3, 3}; // +0x78 .. +0x84: in (first, second half), out (first, second)
    DVSector* plane = nullptr;      // +0x88 the picture one clicks on
    int32_t patch = -1;             // +0x8c the patch shown when it opens
};

// the animation of a move inside a lift (up or down the stairs, the ladder, the wall)
uint32_t DVLiftAnimation(const DVSectorMotionArea* lift, const SBGeoPoint2D& from, const SBGeoPoint2D& to, uint32_t anim);
