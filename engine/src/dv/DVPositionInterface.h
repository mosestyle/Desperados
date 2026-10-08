// DVPositionInterface (Game/DVPositionInterface.cpp): where something is, in three forms kept in
// step: the map point (its feet on the ground picture), the sprite's top-left corner, and the 3D
// point (x, y + height, height) given by the plane of the sight obstacle it stands on.
#pragma once
#include <cstdint>

#include "sb/SBGeo.h"

class DVSightObstacle;
class DVSector;

class DVPositionInterface {
public:
    DVPositionInterface();

    void ComputePositionAll();
    void ComputePositionMap();
    void ComputePositionSprite();
    void ComputePosition3D();
    const SBGeoBoundingBox2D& GetMoveBox(uint32_t posture) const;
    void SetObstacle(DVSightObstacle* o);
    void SetPositionMap(const SBGeoPoint2D& p);

    uint32_t valid = 0;          // +0: 1 = 3D, 2 = map, 4 = sprite position up to date
    uint32_t u4 = 0;             // +4
    SBGeoVector3D pos3D;         // +8
    SBGeoPoint2D posSprite;      // +0x14
    SBGeoPoint2D posMap;         // +0x1c
    SBGeoVector2D anchor;        // +0x78 sprite corner -> feet
    uint32_t posture = 0;        // +0x80
    uint32_t u84 = 0;            // +0x84
    uint16_t pathIndex = 0xffff;     // +0x88
    uint16_t pathIndexAlt = 0xffff;  // +0x8a
    SBGeoBoundingBox2D moveBoxUp;    // +0x8c
    SBGeoBoundingBox2D moveBox;      // +0xa0
    bool u_b4 = false;               // +0xb4
    SBGeoBoundingBox2D moveBoxMap;   // +0xb8
    uint16_t direction = 0;          // +0xcc
    uint16_t directionWanted = 0;    // +0xce
    uint8_t ud0 = 2;                 // +0xd0
    uint16_t layer = 0;              // +0xd2 floor layer
    DVSector* sector = nullptr;      // +0xd4 the motion area
    DVSightObstacle* obstacle = nullptr;  // +0xe0
    const struct SBGeoPlane3D* plane = nullptr;  // +0xe4
    uint32_t uf0 = 0;                // +0xf0
};
