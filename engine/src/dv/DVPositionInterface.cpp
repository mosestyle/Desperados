#include "dv/DVPositionInterface.h"

#include <cmath>

#include "dv/DVSector.h"

DVPositionInterface::DVPositionInterface() {}

// DVPositionInterface::GetMoveBox (0x08339a00): the standing box (+0x8c) for most postures, the
// lying one (+0xa0) when lying (2, 14) on an ordinary area
const SBGeoBoundingBox2D& DVPositionInterface::GetMoveBox(uint32_t p) const {
    static const SBGeoBoundingBox2D emergency(0, 0, 0, 0);
    if (p >= 0xf) return emergency;
    bool flagC0 = sector && (sector->type & 3) == 3 && static_cast<const DVSectorMotionArea*>(sector)->flagC0;
    if ((0x3efbu >> p & 1) == 0 && ((0x4004u >> p & 1) == 0 || (!u_b4 && !flagC0))) return moveBox;
    return moveBoxUp;
}

// DVPositionInterface::GetPathFinderIndex (0x0833a750): which character size the path finder uses
uint16_t DVPositionInterface::GetPathFinderIndex(uint32_t p) const {
    if (p >= 0xf) return 0;
    if ((0x3ebbu >> p & 1) != 0) return pathIndex;
    if ((0x4004u >> p & 1) == 0) return 0;
    bool flagC0 = sector && (sector->type & 3) == 3 && static_cast<const DVSectorMotionArea*>(sector)->flagC0;
    if (!u_b4 && !flagC0) return pathIndexAlt;
    return pathIndex;
}

// DVPositionInterface::SetPosture (0x08249c40)
void DVPositionInterface::SetPosture(uint32_t p) {
    u84 = posture;
    posture = p;
    u_b4 = false;
    moveBoxMap = GetMoveBox(p) + posMap;
}

// DVPositionInterface::ComputeIncrementAll (0x08339de0)
void DVPositionInterface::ComputeIncrementAll(bool direction) {
    if (u4 == 7) return;
    bool skip = false;
    if ((u4 & 2) == 0) {
        if ((u4 & 1) == 0) {
            incMap = goal - posMap;
            if (incMap.x == 0.0f && incMap.y == 0.0f) skip = true;
            else SBNormalize(incMap);
        }
        if (!skip) {
            inc3D.x = incMap.x;
            if (!plane) {
                inc3D.y = incMap.y;
                inc3D.z = 0;
            } else {
                float dz = (incMap.x * plane->zx + plane->zy * incMap.y) / (1.0f - plane->zy);
                inc3D.z = dz;
                inc3D.y = dz + incMap.y;
            }
        }
    } else {
        incMap.x = inc3D.x;
        incMap.y = inc3D.y - inc3D.z;
    }
    if (direction) {
        unsigned d = SBGetSector0to15(SBGeoVector2D(inc3D.x, inc3D.y)) & 0xf;
        if (reverse) d ^= 8;
        directionWanted = (uint16_t)d;
    }
    u4 = 7;
}

// DVPositionInterface::Turn (0x08338f40): one sixteenth of a turn toward the wanted direction
bool DVPositionInterface::Turn() {
    uint16_t diff = (uint16_t)((directionWanted - direction) & 0xf);
    if (diff == 0) return false;
    if (!uf6) {
        direction = (uint16_t)((direction + (diff < 8 ? 1 : 0xf)) & 0xf);
        return true;
    }
    // (the anti-vibration turn of the anti-collision mode)
    if (diff < 8) {
        if ((int8_t)antiVib < 2) {
            if ((int8_t)antiVib >= 0) antiVib = (uint8_t)(antiVib + 1);
            else antiVib = 0;
            return true;
        }
        direction = (uint16_t)((direction + 1) & 0xf);
    } else {
        if ((int8_t)antiVib > -2) {
            if ((int8_t)antiVib < 1) antiVib = (uint8_t)(antiVib - 1);
            else antiVib = 0;
            return true;
        }
        direction = (uint16_t)((direction + 0xf) & 0xf);
    }
    return true;
}

// DVPositionInterface::TurnFast (0x08339070): two sixteenths at a time
bool DVPositionInterface::TurnFast() {
    uint16_t diff = (uint16_t)((directionWanted - direction) & 0xf);
    if (diff == 0) return false;
    if (diff < 8) {
        if (1 < diff) {
            direction = (uint16_t)((direction + 2) & 0xf);
            return true;
        }
    } else if (diff != 0xf) {
        direction = (uint16_t)((direction + 0xe) & 0xf);
        return true;
    }
    direction = directionWanted & 0xf;
    return true;
}

// DVPositionInterface::IsGoalReached (0x0833a820), the ordinary case: the goal is behind us
bool DVPositionInterface::IsGoalReached() const {
    if (!uf6) return SBDot(incMap, goal - posMap) <= tolerance;
    return SBMaxNorm(goal - posMap) < 10.0f;
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
