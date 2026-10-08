// DVSector (Game/DVSector.cpp) and DVSightObstacle (Game/DVSightObstacle.cpp): areas of the
// level. Motion areas (where characters can walk, one list per floor layer), sight obstacles
// (3D volumes: walls, roofs, the planes characters stand on), patch / script sectors...
#pragma once
#include <cstdint>
#include <vector>

#include "sb/SBGeo.h"
#include "sb/SBGeoPlane.h"

class SBFile;

// sector types (+0x2c)
enum : uint32_t {
    SECTOR_MOTION = 3,       // a motion area (bits 1|2)
    SECTOR_PLANE = 0x42,
    SECTOR_PATCH = 0x44,
};

class DVSector {
public:
    explicit DVSector(uint32_t type = 0);
    virtual ~DVSector() = default;
    int LoadFromFile(SBFile& f);
    // the engine resets the sector and sight obstacle numbering for each level
    static void ResetCounters();

    SBGeoPolygon2DArray polygon;  // +4
    uint32_t type = 0;            // +0x2c
    bool flag30 = true;           // +0x30
    int16_t id = 0;               // +0x32: creation number (= index in the grid's list)
    SBGeoBoundingBox2D box;       // +0x34
    int layer = 0;                // the motion layer it was loaded in
    std::vector<class DVSightObstacle*> obstacles;  // +0x9c list (motion areas)
};

class DVSectorMotionArea : public DVSector {
public:
    DVSectorMotionArea() : DVSector(SECTOR_MOTION) {}
    bool isLift = false;  // the last layer's areas are DVSectorLift
    uint16_t areaIndex = 0;  // +0x50: its index in its layer (DVPathFinder::ConvertSector)
    bool flagC0 = false;     // +0xc0
};

class DVSightObstacle {
public:
    explicit DVSightObstacle(uint32_t flags = 3);
    int LoadFromFile(SBFile& f);

    uint32_t flags = 3;            // +0: 1 = blocks sight, 2 = ?, 4 = can be stood on (view area), 8
    uint32_t serial = 0;           // +4
    SBGeoVector3D box3dMin, box3dMax;  // +0xc
    SBGeoBoundingBox2D box;        // +0x24: ground box
    SBGeoBoundingBox2D boxScreen;  // +0x38: box on the picture (with heights)
    std::vector<SBGeoVector3D> points;  // +0x4c list (x, y, height h)
    SBGeoPolygon2DArray polygon;   // +0x7c
    SBGeoPlane3D plane;            // +0xa4: the top plane (z of a point)
    SBGeoPlane3D planeGround;      // +0xfc
    bool flat = true;              // +0x154
    int32_t motionLayer = -1;      // +0x156 (u16) and
    uint16_t motionArea = 0;       // +0x158
    uint32_t u15c = 0, u160 = 0;   // +0x15c, +0x160
    uint32_t material = 0;         // +0x164
    uint8_t u168 = 0;              // +0x168
    class DVSector* sectorPlane = nullptr;  // +0x190
    bool u1a2 = false;             // +0x1a2
};
