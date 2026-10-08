// SBGeoVector2D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
// A 2D vector; in the original it is an SBGeoPoint2D (its constructors build the point base).
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoPoint2D.h"

class SBGeoVector2D : public SBGeoPoint2D {
public:
    void Copy(SBGeoPoint2D const& p1);  // 0817c390
    void Destroy();  // 0817c3b0
    SBGeoVector2D();  // 0817c3c0
    SBGeoVector2D(SBGeoVector2D const& p1);  // 0817c3d0
    SBGeoVector2D(float p1, float p2);  // 0817c400
    SBGeoVector2D(SBGeoPoint2D const& p1);  // 0817c410
    SBGeoVector2D(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2);  // 0817c440
    void operator=(SBGeoVector2D const& p1);  // 0817c480
    SBGeoVector2D operator-() const;  // 0817c4a0
    void operator*=(float p1);  // 0817c4e0
    void operator/=(float p1);  // 0817c590
    SBGeoVector2D operator/(float p1) const;  // 0817c5c0
    float operator*(SBGeoVector2D const& p1) const;  // 0817c600
    float Det(SBGeoVector2D const& p1) const;  // 0817c630
    float Norm() const;  // 0817c670
    float SquareNorm() const;  // 0817c6c0
    float MaxNorm() const;  // 0817c6f0
    float OneNorm() const;  // 0817c750
    void Normalize();  // 0817c7a0
    SBGeoVector2D GetNormalized() const;  // 0817c800
    void Normal(bool p1);  // 0817c870
    SBGeoVector2D GetNormal(bool p1) const;  // 0817c8b0
    void Rotate(float p1);  // 0817c900
    SBGeoVector2D GetRotated(float p1) const;  // 0817c9b0
    float Angle(SBGeoVector2D const& p1) const;  // 0817cac0
    uint8_t GetSector0to15(float p1) const;  // 0817cbd0
    void SetSector0to15(unsigned char p1, float p2);  // 0817cc80

    // the 16 directions of the sector functions (x and y tables)
    static const float marraySectorX[16];  // 0855753c
    static const float marraySectorY[16];  // 0855757c

    // original size 0x8 (the SBGeoPoint2D base: f_0, f_4)
};
