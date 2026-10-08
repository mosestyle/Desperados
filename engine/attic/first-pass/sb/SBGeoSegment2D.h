// SBGeoSegment2D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoHalfLine2D.h"

class SBGeoBoundingBox2D;

class SBGeoSegment2D : public SBGeoHalfLine2D {
public:
    SBGeoSegment2D(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2);  // 0817a220
    SBGeoSegment2D(SBGeoLine2D const& p1);  // 0817a230
    SBGeoSegment2D(SBGeoHalfLine2D const& p1);  // 0817a260
    SBGeoSegment2D(SBGeoSegment2D const& p1);  // 0817a290
    SBGeoSegment2D& operator=(SBGeoSegment2D const& p1);  // 0817a2c0
    bool operator==(SBGeoSegment2D const& p1) const;  // 0817a2e0
    bool IsIntersecting(SBGeoPoint2D const& p1) const;  // 0817a350
    bool IsIntersecting(SBGeoLine2D const& p1) const;  // 0817a450
    bool IsIntersecting(SBGeoHalfLine2D const& p1) const;  // 0817a470
    bool IsIntersecting(SBGeoSegment2D const& p1) const;  // 0817a490
    bool IsIntersecting(SBGeoPolyLine2DList const& p1) const;  // 0817a7e0
    bool IsIntersecting(SBGeoPolyLine2DArray const& p1) const;  // 0817a870
    bool IsIntersecting(SBGeoPolygon2DList const& p1) const;  // 0817a900
    bool IsIntersecting(SBGeoPolygon2DArray const& p1) const;  // 0817a990
    bool IsIntersecting(SBGeoBoundingBox2D const& p1) const;  // 0817aa20
    SBGeoIntersection2D operator^(SBGeoLine2D const& p1) const;  // 0817aa40
    SBGeoIntersection2D operator^(SBGeoHalfLine2D const& p1) const;  // 0817aa70
    SBGeoIntersection2D operator^(SBGeoSegment2D const& p1) const;  // 0817aaa0
    SBList<SBGeoIntersection2D> operator^(SBGeoPolyLine2DArray const& p1) const;  // 0817b540
    SBList<SBGeoIntersection2D> operator^(SBGeoPolyLine2DList const& p1) const;  // 0817b880
    SBList<SBGeoIntersection2D> operator^(SBGeoPolygon2DList const& p1) const;  // 0817bbc0
    SBList<SBGeoIntersection2D> operator^(SBGeoPolygon2DArray const& p1) const;  // 0817bf00
    bool IsBetween(SBGeoPoint2D const& p1) const;  // 0817c240

    // original size 0x10 (the SBGeoLine2D base: f_0 and f_8 are the ends)
};
