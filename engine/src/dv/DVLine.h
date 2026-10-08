// DVLine (Game/DVLine.cpp): a line of the level kept in the fast-find grid. The edges of the
// walkable areas and of the obstacles inside them are DVRepulsiveLine: they block movement
// (flag 2) and push characters back along their normal, which points into the walkable side.
#pragma once
#include <cstdint>

#include "sb/SBGeo.h"

class DVSector;

enum : uint32_t {
    LINE_MOTION = 2,     // blocks movement (DVlineType 2)
    LINE_REPULSIVE = 0x80,
};

struct DVLine {
    SBGeoSegment2D seg;         // +4
    uint32_t flags = 0;         // +0x14
    bool enabled = true;        // +0x18
    DVSector* sector = nullptr; // +0x1c
    int16_t sectorId = -1;      // +0x20 (DVRepulsiveLine)
    SBGeoVector2D normal;       // +0x34 (DVRepulsiveLine)
    SBGeoVector2D dir;          // +0x3c

    // DVRepulsiveLine::InitializeNormal (0x082f10b0)
    void InitializeNormal() {
        dir = seg.b - seg.a;
        SBNormalize(dir);
        normal = SBGetNormal(dir, (flags & 1) != 0);
    }
};
