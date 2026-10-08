// SBGeoPoint2D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
// A 2D point (x, y) in single precision. SBGeoVector2D derives from it.
#pragma once
#include "sb/SBCommon.h"

class SBFile;
class SBGeoVector2D;

class SBGeoPoint2D {
public:
    void Copy(SBGeoPoint2D const& p1);  // 08162a20
    void Destroy();  // 08162a40
    SBGeoPoint2D();  // 08162a50
    SBGeoPoint2D(SBGeoPoint2D const& p1);  // 08162a70
    SBGeoPoint2D(float p1, float p2);  // 08162a90
    void operator=(SBGeoPoint2D const& p1);  // 08162ab0
    bool operator==(SBGeoPoint2D const& p1) const;  // 08162ad0
    bool operator!=(SBGeoPoint2D const& p1) const;  // 08162b00
    bool IsNear(SBGeoPoint2D const& p1, float p2) const;  // 08162b30
    SBGeoVector2D operator-(SBGeoPoint2D const& p1) const;  // 08162b90
    ~SBGeoPoint2D();  // 08162bd0
    SBGeoPoint2D operator+(SBGeoVector2D const& p1) const;  // 08162be0
    void operator+=(SBGeoVector2D const& p1);  // 08162c10
    SBGeoPoint2D operator-(SBGeoVector2D const& p1) const;  // 08162c40
    void operator-=(SBGeoVector2D const& p1);  // 08162c70
    void Rotate(SBGeoPoint2D const& p1, float p2, bool p3);  // 08162ca0
    SBGeoPoint2D GetRotated(SBGeoPoint2D const& p1, float p2, bool p3) const;  // 08162d00
    void Floor();  // 08162d70
    SBGeoPoint2D GetFloored();  // 08162da0
    int32_t Serialize(SBFile& p1);  // 08162df0
    void Discretize();  // 08162e90
    SBGeoPoint2D GetDiscretized();  // 08162ec0
    SBGeoPoint2D operator/(float p1) const;  // 08162f60
    SBGeoPoint2D operator-() const;  // 08162f90

    // original size 0x8
    float f_0;  // +0x0  x
    float f_4;  // +0x4  y
};

// The vector type is needed by value for the operators above.
#include "sb/SBGeoVector2D.h"
