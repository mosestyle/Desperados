// SBGeoSegment3D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoPoint3D.h"

class SBGeoSegment3D {
public:
    void Set(SBGeoPoint3D p1, SBGeoPoint3D p2);  // 083b06a0
    bool operator==(SBGeoSegment3D& p1);  // 083b06d0
    bool operator!=(SBGeoSegment3D& p1);  // 083b0790

    // original size 0x18
    SBGeoPoint3D f_0;  // +0x0  first end
    SBGeoPoint3D f_c;  // +0xc  second end
};
