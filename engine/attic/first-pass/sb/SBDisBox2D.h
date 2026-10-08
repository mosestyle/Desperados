// SBDisBox2D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
// An integer box; the max coordinates are exclusive (one past the last pixel).
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBDisPoint2D.h"

class SBGeoBoundingBox2D;

class SBDisBox2D {
public:
    SBDisBox2D();  // 081474b0
    SBDisBox2D(SBGeoBoundingBox2D const& p1);  // 081474c0
    SBDisBox2D(SBDisBox2D const& p1);  // 081474f0
    SBDisBox2D(int p1, int p2, int p3, int p4);  // 08147500
    void Expand(int p1, int p2);  // 08147530
    void Expand(SBDisPoint2D const& p1);  // 08147560
    void Expand(SBDisBox2D const& p1);  // 08147590
    void Expand(SBDisBox2D& p1, SBDisBox2D const& p2) const;  // 081475d0
    void Clip(SBDisBox2D const& p1);  // 08147620
    void Clip(SBDisBox2D& p1, SBDisBox2D const& p2) const;  // 08147660

    // original size 0x10
    int32_t f_0;  // +0x0  min x
    int32_t f_4;  // +0x4  max x (exclusive)
    int32_t f_8;  // +0x8  min y
    int32_t f_c;  // +0xc  max y (exclusive)
};
