#include "dv/DVPositionInterface.h"

#include <cmath>

#include "dv/DVSector.h"

DVPositionInterface::DVPositionInterface() {}

// DVPositionInterface::GetMoveBox (0x08339a00)
const SBGeoBoundingBox2D& DVPositionInterface::GetMoveBox(uint32_t p) const {
    static const SBGeoBoundingBox2D emergency(0, 0, 0, 0);
    if (p >= 0xf) return emergency;
    if ((0x3efbu >> p & 1) == 0 && ((0x4004u >> p & 1) == 0 || (!u_b4 && !(sector && false)))) return moveBox;
    return moveBoxUp;
}

static float planeHeight(const SBGeoPlane3D* pl, const SBGeoPoint2D& m) {
    // the height of the plane above the map point, as the original: (a x + b y + c) / (1 - b)
    return (m.x * pl->zx + pl->zy * m.y + pl->zc) / (1.0f - pl->zy);
}

// DVPositionInterface::ComputePositionAll (0x08339140)
void DVPositionInterface::ComputePositionAll() {
    if (valid == 7) return;
    if ((valid & 1) == 0) {
        if ((valid & 2) == 0) {
            if ((valid & 4) == 0) {
                valid = 7;
                return;
            }
            posMap = posSprite + anchor;
            moveBoxMap = GetMoveBox(posture) + posMap;
        } else {
            posSprite = posMap - anchor;
            posSprite.x = std::floor(posSprite.x);
            posSprite.y = std::floor(posSprite.y);
        }
        pos3D.x = posMap.x;
        float h = 0;
        if (!plane) {
            pos3D.y = posMap.y;
        } else {
            h = planeHeight(plane, posMap);
            pos3D.y = posMap.y + h;
        }
        pos3D.z = h;
    } else {
        posMap.x = pos3D.x;
        posMap.y = pos3D.y - pos3D.z;
        moveBoxMap = GetMoveBox(posture) + posMap;
        posSprite = posMap - anchor;
        posSprite.x = std::floor(posSprite.x);
        posSprite.y = std::floor(posSprite.y);
    }
    valid = 7;
}

// DVPositionInterface::ComputePositionMap (0x08339400)
void DVPositionInterface::ComputePositionMap() {
    if (valid & 2) return;
    if ((valid & 4) == 0) {
        posMap.x = pos3D.x;
        posMap.y = pos3D.y - pos3D.z;
    } else {
        posMap = posSprite + anchor;
    }
    moveBoxMap = GetMoveBox(posture) + posMap;
    valid |= 2;
}

// DVPositionInterface::ComputePositionSprite (0x08339580)
void DVPositionInterface::ComputePositionSprite() {
    if (valid & 4) return;
    if ((valid & 2) == 0) {
        posMap.x = pos3D.x;
        posMap.y = pos3D.y - pos3D.z;
        moveBoxMap = GetMoveBox(posture) + posMap;
        posSprite = posMap - anchor;
        posSprite.x = std::floor(posSprite.x);
        posSprite.y = std::floor(posSprite.y);
        valid = 7;
    } else {
        posSprite = posMap - anchor;
        posSprite.x = std::floor(posSprite.x);
        posSprite.y = std::floor(posSprite.y);
        valid |= 4;
    }
}

// DVPositionInterface::ComputePosition3D (0x08339720)
void DVPositionInterface::ComputePosition3D() {
    if (valid & 1) return;
    if ((valid & 2) == 0) {
        posMap = posSprite + anchor;
        moveBoxMap = GetMoveBox(posture) + posMap;
    }
    pos3D.x = posMap.x;
    float h = 0;
    if (!plane) {
        pos3D.y = posMap.y;
    } else {
        h = planeHeight(plane, posMap);
        pos3D.y = posMap.y + h;
    }
    pos3D.z = h;
    valid = (valid & 2) ? (valid | 1) : 7;
}

// DVPositionInterface::SetObstacle (0x08339ae0)
void DVPositionInterface::SetObstacle(DVSightObstacle* o) {
    obstacle = o;
    plane = o ? &o->plane : nullptr;
    valid &= ~1u;
    u4 &= ~2u;
}

// DVPositionInterface::SetPositionMap (0x081b3560)
void DVPositionInterface::SetPositionMap(const SBGeoPoint2D& p) {
    posMap = p;
    moveBoxMap = GetMoveBox(posture) + p;
    valid = 2;
}
