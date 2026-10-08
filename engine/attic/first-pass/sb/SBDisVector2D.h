// SBDisVector2D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
#pragma once
#include "sb/SBCommon.h"

class SBGeoVector2D;

class SBDisVector2D {
public:
    SBDisVector2D();  // 081476b0
    SBDisVector2D(SBDisVector2D const& p1);  // 081476c0
    SBDisVector2D(int p1, int p2);  // 081476e0
    SBDisVector2D(SBGeoVector2D const& p1);  // 08147700

    // original size 0x8
    int32_t f_0;  // +0x0  x
    int32_t f_4;  // +0x4  y
};
