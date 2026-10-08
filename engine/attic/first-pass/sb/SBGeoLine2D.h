// SBGeoLine2D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
// An infinite line through two points. SBGeoHalfLine2D (from the first point through the
// second) and SBGeoSegment2D derive from it without adding fields. No virtual methods.
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoPoint2D.h"
#include "sb/SBGeoIntersection2D.h"

class SBGeoHalfLine2D;
class SBGeoPolyLine2DArray;
class SBGeoPolyLine2DList;
class SBGeoPolygon2DArray;
class SBGeoPolygon2DList;
class SBGeoSegment2D;
class SBGeoVector2D;

class SBGeoLine2D {
public:
    void Copy(SBGeoLine2D const& p1);  // 08160610
    void Destroy();  // 08160650
    SBGeoLine2D(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2);  // 08160660
    SBGeoLine2D(SBGeoPoint2D const& p1, SBGeoVector2D const& p2);  // 081606b0
    SBGeoLine2D(SBGeoLine2D const& p1);  // 08160740
    SBGeoLine2D(SBGeoHalfLine2D const& p1);  // 081607b0
    SBGeoLine2D(SBGeoSegment2D const& p1);  // 08160820
    SBGeoLine2D& operator=(SBGeoLine2D const& p1);  // 08160890
    void SetVector(SBGeoVector2D const& p1);  // 081608d0
    bool IsIntersecting(SBGeoLine2D const& p1) const;  // 08160930
    bool IsIntersecting(SBGeoHalfLine2D const& p1) const;  // 08160a90
    bool IsIntersecting(SBGeoSegment2D const& p1) const;  // 08160c30
    bool IsIntersecting(SBGeoPolyLine2DArray const& p1) const;  // 08160ea0
    ~SBGeoLine2D();  // 08160f50
    bool IsIntersecting(SBGeoPolyLine2DList const& p1) const;  // 08160f70
    bool IsIntersecting(SBGeoPolygon2DList const& p1) const;  // 08161020
    bool IsIntersecting(SBGeoPolygon2DArray const& p1) const;  // 081610d0
    SBGeoIntersection2D operator^(SBGeoLine2D const& p1) const;  // 08161180
    SBGeoIntersection2D operator^(SBGeoHalfLine2D const& p1) const;  // 08161380
    SBGeoIntersection2D operator^(SBGeoSegment2D const& p1) const;  // 08161700
    SBList<SBGeoIntersection2D> operator^(SBGeoPolyLine2DArray const& p1) const;  // 08161a60
    SBList<SBGeoIntersection2D> operator^(SBGeoPolyLine2DList const& p1) const;  // 08161da0
    SBList<SBGeoIntersection2D> operator^(SBGeoPolygon2DList const& p1) const;  // 081620e0
    SBList<SBGeoIntersection2D> operator^(SBGeoPolygon2DArray const& p1) const;  // 08162450
    float EvalX(float p1) const;  // 081627c0
    float EvalY(float p1) const;  // 08162820
    bool operator==(SBGeoLine2D const& p1) const;  // 08162880
    bool operator!=(SBGeoLine2D const& p1) const;  // 08162a00

    // original size 0x10
    SBGeoPoint2D f_0;  // +0x0  first point
    SBGeoPoint2D f_8;  // +0x8  second point
};
