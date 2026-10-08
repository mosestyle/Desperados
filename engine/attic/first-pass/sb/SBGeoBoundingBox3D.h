// SBGeoBoundingBox3D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoPoint3D.h"
#include "sb/SBGeoVector3D.h"

class SBGeoBoundingBox3D {
public:
    SBGeoBoundingBox3D();  // 083b0de0
    void SetTopLeftFront(SBGeoVector3D p1);  // 083b0e00
    void SetBottomRightBack(SBGeoVector3D p1);  // 083b0e20
    void Expand(SBGeoPoint3D p1);  // 083b0e40
    bool IsInside(SBGeoVector3D const& p1);  // 083b0eb0

    // original size 0x18
    float f_0;   // +0x0   min x
    float f_4;   // +0x4   max x
    float f_8;   // +0x8   min y
    float f_c;   // +0xc   max y
    float f_10;  // +0x10  min z
    float f_14;  // +0x14  max z
};
