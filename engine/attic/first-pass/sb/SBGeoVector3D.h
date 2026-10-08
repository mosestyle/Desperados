// SBGeoVector3D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoPoint3D.h"

class SBFile;
class SBGeoPoint2D;

class SBGeoVector3D : public SBGeoPoint3D {
public:
    SBGeoVector3D();  // 083b0380
    SBGeoVector3D(SBGeoPoint2D const& p1, float p2);  // 083b0390
    SBGeoVector3D(float p1, float p2, float p3);  // 083b03b0
    int32_t Serialize(SBFile& p1);  // 083b03e0
    SBGeoVector3D operator-(SBGeoVector3D const& p1) const;  // 083b0440
    SBGeoVector3D operator+(SBGeoVector3D const& p1) const;  // 083b0480
    float operator*(SBGeoVector3D const& p1) const;  // 083b04c0
    SBGeoVector3D operator^(SBGeoVector3D const& p1) const;  // 083b0500
    float MaxNorm() const;  // 083b0570
    SBGeoVector3D DistanceVectorToLine(SBGeoVector3D const& p1, SBGeoVector3D const& p2);  // 083b05c0

    // original size 0xc (the SBGeoPoint3D base: f_0, f_4, f_8)
};
