// DVMask (Game/DVMask.cpp): a piece of the background that is drawn in front of whatever stands
// behind it. Stored as a 1-bit run-length bitmap over its box; characters / projectiles behind
// the mask's base line get the masked pixels removed from their picture (DVSprite::
// DrawMaskOverSprite), so the background shows through.
#pragma once
#include <cstdint>
#include <vector>

#include "sb/SBGeo.h"
#include "sb/SBGeoPlane.h"

class SBFile;
class DVFastFindGrid;
class DVSightObstacle;

class DVMask {
public:
    bool active = true;               // +0
    uint32_t type = 0;                // +4: 1 = character base line, 2 = projectile line, 0x10 = has sight obstacle
    SBGeoBoundingBox2D box;           // +8
    float maxY = 0;                   // +0x1c: the lowest point of the base lines
    uint16_t dataSize = 0;            // +0x20
    std::vector<uint8_t> data;        // +0x24: the bitmap
    SBGeoPolyLine2DArray lineCharacter;   // +0x28
    SBGeoPolyLine2DArray lineProjectile;  // +0x48
    DVSightObstacle* obstacle = nullptr;  // +0x68

    int LoadFromFile(SBFile& f, DVFastFindGrid* grid);
    uint8_t operator[](unsigned i) const { return i < data.size() ? data[i] : 0; }
    bool IsAppliedToPointCharacter(const SBGeoPoint2D& p) const;
    bool IsAppliedToBox(const SBGeoBoundingBox2D& b) const;
};
