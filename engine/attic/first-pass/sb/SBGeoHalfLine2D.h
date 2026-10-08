// SBGeoHalfLine2D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
// A half line from the first point through the second one.
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoLine2D.h"

class SBGeoHalfLine2D : public SBGeoLine2D {
public:
    SBGeoHalfLine2D(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2);  // 0815d870
    SBGeoHalfLine2D(SBGeoLine2D const& p1);  // 0815d880
    SBGeoHalfLine2D(SBGeoHalfLine2D const& p1);  // 0815d8b0
    SBGeoHalfLine2D(SBGeoSegment2D const& p1);  // 0815d8e0
    SBGeoHalfLine2D& operator=(SBGeoHalfLine2D const& p1);  // 0815d910
    bool operator==(SBGeoHalfLine2D const& p1) const;  // 0815d930
    bool IsIntersecting(SBGeoLine2D const& p1) const;  // 0815da10
    bool IsIntersecting(SBGeoHalfLine2D const& p1) const;  // 0815da30
    bool IsIntersecting(SBGeoSegment2D const& p1) const;  // 0815da40
    bool IsIntersecting(SBGeoPolyLine2DArray const& p1) const;  // 0815dd00
    bool IsIntersecting(SBGeoPolyLine2DList const& p1) const;  // 0815dd90
    bool IsIntersecting(SBGeoPolygon2DList const& p1) const;  // 0815de20
    bool IsIntersecting(SBGeoPolygon2DArray const& p1) const;  // 0815deb0
    SBGeoIntersection2D operator^(SBGeoLine2D const& p1) const;  // 0815df40
    SBGeoIntersection2D operator^(SBGeoHalfLine2D const& p1) const;  // 0815df70
    SBGeoIntersection2D operator^(SBGeoSegment2D const& p1) const;  // 0815e900
    SBList<SBGeoIntersection2D> operator^(SBGeoPolyLine2DArray const& p1) const;  // 0815f3f0
    SBList<SBGeoIntersection2D> operator^(SBGeoPolyLine2DList const& p1) const;  // 0815f700
    SBList<SBGeoIntersection2D> operator^(SBGeoPolygon2DList const& p1) const;  // 0815fa10
    SBList<SBGeoIntersection2D> operator^(SBGeoPolygon2DArray const& p1) const;  // 0815fa90
    SBGeoHalfLine2D operator-();  // 0815fb10

    // original size 0x10 (the SBGeoLine2D base: f_0 origin, f_8 second point)
};
