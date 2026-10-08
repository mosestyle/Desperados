// SBGeoPoint3D (original type; translated from the decompiled game, see engine/TRANSLATING.md)
// A plain 3D point (x, y, z) used by value; it has no methods of its own in the original
// (12 bytes: SBGeoSegment3D::Set and SBGeoBoundingBox3D::Expand take it as 3 floats on the stack).
// SBGeoVector3D derives from it, so a vector can be passed where a point is expected (the game
// does this, e.g. DVFastFindGrid::Convert2DTo3D).
#pragma once
#include "sb/SBCommon.h"

struct SBGeoPoint3D {
    // original size 0xc
    float f_0;  // +0x0  x
    float f_4;  // +0x4  y
    float f_8;  // +0x8  z
};
