// SBGeoPlane3D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
// A plane through three points, with its frame vectors, unit normal, homogeneous equation
// (n.p + d = 0) and the z = a*x + b*y + c form.
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoPoint3D.h"
#include "sb/SBGeoVector3D.h"

class SBGeoVector2D;

class SBGeoPlane3D {
public:
    SBGeoPlane3D();  // 083b0850
    SBGeoPlane3D(SBGeoPoint3D const& p1, SBGeoPoint3D const& p2, SBGeoPoint3D const& p3);  // 083b0860
    void SetPoints(SBGeoPoint3D const& p1, SBGeoPoint3D const& p2, SBGeoPoint3D const& p3);  // 083b08b0
    void SetFrame(SBGeoPoint3D const& p1, SBGeoVector3D const& p2, SBGeoVector3D const& p3);  // 083b0900
    void ComputeVectors();  // 083b0950
    void ComputeNormal();  // 083b09a0
    SBGeoVector3D GetNormal();  // 083b0a30
    void ComputeHomogenEquation();  // 083b0a50
    void ComputeZEquation();  // 083b0a90
    float ComputeZ(float p1, float p2);  // 083b0ad0
    float ComputeZIncrement(float p1, float p2);  // 083b0b00
    void InitializeAll();  // 083b0b40
    void operator+=(SBGeoVector3D const& p1);  // 083b0c70
    void operator+=(SBGeoVector2D const& p1);  // 083b0d40

    // original size 0x58
    SBGeoPoint3D f_0;    // +0x0   first point
    SBGeoPoint3D f_c;    // +0xc   second point
    SBGeoPoint3D f_18;   // +0x18  third point
    SBGeoVector3D f_24;  // +0x24  second - first
    SBGeoVector3D f_30;  // +0x30  third - first
    SBGeoVector3D f_3c;  // +0x3c  unit normal
    float f_48;          // +0x48  d of n.p + d = 0
    float f_4c;          // +0x4c  z = f_4c * x + f_50 * y + f_54
    float f_50;          // +0x50
    float f_54;          // +0x54
};
