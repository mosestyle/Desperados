// SBDisPoint2D (original type; translated from the decompiled game, see engine/TRANSLATING.md)
// A point in integer (display) coordinates. It has no out-of-line methods in the original; its
// layout comes from SBDisBox2D::Expand(SBDisPoint2D const&) (x at +0, y at +4).
#pragma once
#include "sb/SBCommon.h"

class SBDisPoint2D {
public:
    // original size 0x8
    int32_t f_0;  // +0x0  x
    int32_t f_4;  // +0x4  y
};
