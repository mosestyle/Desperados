// BUIL chunk (DVFastFindGrid "buildings"): houses the characters can enter and leave through
// doors, and stand-alone doors (gates, passages) between two motion areas.
//
// A building has no visible interior: whoever goes in disappears until leaving through one of
// its doors (possibly another one, e.g. the street door after the roof hatch).
//
//   u32 version (4), u16 buildings, per building:
//     u16 (ignored by the game), u16 n + n * u16 sector ids (areas counted as inside), u16 doors + doors
//   u16 stand-alone doors + doors
//   door: u8 type (0 default, 1 gate, 2 hatch / trapdoor, 3 element-driven gate),
//         u8 known, u8 lockedForPCs, u8 lock pickable by Doc, u8 lockedForVillains, u8 lockedForCivilians, u8[4] colour,
//         u16 n + n * (s16 x, s16 y) clickable outline,
//         u16 3, 3 * (s16 x, s16 y, u16 sector, u16 layer): outside, threshold, inside
//           (for a building door the inside point's sector / layer are unused),
//         s16 element (-1 none) [+ u8, u8] (animated door picture)
#pragma once
#include <SDL.h>

#include <cstdint>
#include <vector>

struct Door {
    int type = 0, building = -1;
    bool known = true, lockPC = false, pickable = false, clickable = true, lockVillain = false, lockCivilian = false;
    std::vector<SDL_Point> outline;
    SDL_FPoint out{0, 0}, mid{0, 0}, in{0, 0};
    int outSector = 0, outLayer = 0, midSector = 0, midLayer = 0, inSector = 0, inLayer = 0;
    int elem = -1;
};

struct Building {
    std::vector<int> sectors;
    std::vector<int> doors;  // indices into the door list
    bool active = true;
};

bool parseBuildings(const uint8_t* data, size_t size, std::vector<Building>& buildings, std::vector<Door>& doors);
