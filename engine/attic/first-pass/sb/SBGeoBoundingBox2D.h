// SBGeoBoundingBox2D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
// An axis-aligned box: f_0 = min corner, f_8 = max corner, f_10 = "is somewhere" (set once the
// box contains something). Most tests treat an empty box as containing nothing.
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoPoint2D.h"
#include "sb/SBGeoIntersection2D.h"

class SBFile;
class SBGeoHalfLine2D;
class SBGeoLine2D;
class SBGeoPolyLine2DArray;
class SBGeoPolyLine2DList;
class SBGeoPolygon2DArray;
class SBGeoPolygon2DList;
class SBGeoSegment2D;
class SBGeoVector2D;

class SBGeoBoundingBox2D {
public:
    void Copy(SBGeoBoundingBox2D const& p1);  // 081576e0
    void Destroy();  // 08157720
    SBGeoBoundingBox2D();  // 08157730
    SBGeoBoundingBox2D(SBGeoPoint2D const& p1, float p2, float p3);  // 081577f0
    SBGeoBoundingBox2D(float p1, float p2);  // 08157910
    SBGeoBoundingBox2D(SBGeoBoundingBox2D const& p1);  // 08157980
    SBGeoBoundingBox2D(SBGeoPoint2D const& p1);  // 081579f0
    SBGeoBoundingBox2D(SBGeoSegment2D const& p1);  // 08157a60
    void Expand(SBGeoSegment2D const& p1);  // 08157af0
    SBGeoBoundingBox2D(SBGeoPolyLine2DArray const& p1);  // 08157ba0
    void Expand(SBGeoPoint2D const& p1);  // 08157c80
    SBGeoBoundingBox2D(SBGeoPolyLine2DList const& p1);  // 08157ce0
    SBGeoBoundingBox2D(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2);  // 08157dc0
    SBGeoBoundingBox2D(float p1, float p2, float p3, float p4);  // 08157e30
    int32_t Serialize(SBFile& p1);  // 08157ea0
    void Reset();  // 08157fd0
    SBGeoBoundingBox2D& operator=(SBGeoBoundingBox2D const& p1);  // 08157fe0
    void Expand(SBGeoPolyLine2DArray const& p1);  // 08158030
    void Expand(SBGeoPolyLine2DList const& p1);  // 081580d0
    void Expand(SBGeoBoundingBox2D const& p1);  // 08158170
    bool IsTriviallyRejected(SBGeoSegment2D const& p1) const;  // 08158290
    bool IsTriviallyRejected(SBGeoHalfLine2D const& p1) const;  // 08158380
    bool IsTriviallyRejected(SBGeoLine2D const& p1) const;  // 08158460
    bool IsTriviallyRejected(SBGeoPolyLine2DList const& p1) const;  // 08158530
    bool IsInside_p(SBGeoPoint2D const& p1) const;  // 08158630
    bool IsTriviallyRejected(SBGeoPolyLine2DArray const& p1) const;  // 08158690
    bool IsTriviallyRejected(SBGeoPolygon2DList const& p1) const;  // 08158790
    bool IsTriviallyRejected(SBGeoPolygon2DArray const& p1) const;  // 081588d0
    bool IsTriviallyRejected(SBGeoBoundingBox2D const& p1) const;  // 08158a10
    bool IsBoxed(SBGeoPoint2D const& p1) const;  // 08158b20
    bool IsInside_s(SBGeoSegment2D const& p1) const;  // 08158b80
    bool IsInside(SBGeoPolyLine2DList const& p1) const;  // 08158c10
    bool IsInside(SBGeoPolyLine2DArray const& p1) const;  // 08158cc0
    bool IsInside_b(SBGeoBoundingBox2D const& p1) const;  // 08158d70
    bool IsOnBoundary(SBGeoPoint2D const& p1) const;  // 08158ee0
    bool IsIntersecting(SBGeoPoint2D const& p1) const;  // 08158f50
    bool IsIntersecting(SBGeoSegment2D const& p1) const;  // 08158fb0
    bool IsIntersecting(SBGeoLine2D const& p1) const;  // 08159090
    bool IsIntersecting(SBGeoHalfLine2D const& p1) const;  // 08159550
    bool IsIntersecting(SBGeoPolyLine2DList const& p1) const;  // 081595f0
    bool IsIntersecting(SBGeoPolyLine2DArray const& p1) const;  // 08159770
    bool IsIntersecting(SBGeoPolygon2DList const& p1) const;  // 081598f0
    bool IsIntersecting(SBGeoPolygon2DArray const& p1) const;  // 08159a50
    bool IsIntersecting(SBGeoBoundingBox2D const& p1) const;  // 08159c60
    SBList<unsigned int> IntersectionIndexList(SBGeoPolyLine2DList const& p1) const;  // 08159c80
    SBList<unsigned int> MyIntersectionIndexList(SBGeoPolyLine2DList const& p1) const;  // 08159cb0
    SBList<unsigned int> IntersectionIndexList(SBGeoPolyLine2DArray const& p1) const;  // 08159dd0
    SBList<unsigned int> MyIntersectionIndexList(SBGeoPolyLine2DArray const& p1) const;  // 08159e00
    SBList<unsigned int> IntersectionIndexList(SBGeoPolygon2DList const& p1) const;  // 08159f20
    SBList<unsigned int> IntersectionIndexList(SBGeoPolygon2DArray const& p1) const;  // 0815a0e0
    bool IsSomewhere() const;  // 0815a2a0
    SBGeoPoint2D GetCenter() const;  // 0815a2b0
    bool Set(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2);  // 0815a3e0
    bool IsOK() const;  // 0815a440
    SBGeoBoundingBox2D operator+(SBGeoVector2D const& p1) const;  // 0815a470
    SBGeoBoundingBox2D& operator+=(SBGeoVector2D const& p1);  // 0815a500
    void operator*=(float p1);  // 0815a530
    SBGeoBoundingBox2D operator-(SBGeoVector2D const& p1) const;  // 0815a550
    SBGeoBoundingBox2D& operator-=(SBGeoVector2D const& p1);  // 0815a5e0
    SBGeoBoundingBox2D Clip(SBGeoBoundingBox2D const& p1) const;  // 0815a610
    SBGeoIntersection2D Clip(SBGeoLine2D const& p1) const;  // 0815a6e0
    SBGeoIntersection2D MyClip(SBGeoLine2D const& p1, double p2, double p3) const;  // 0815a750
    SBGeoIntersection2D Clip(SBGeoHalfLine2D const& p1) const;  // 0815ad90
    SBGeoIntersection2D Clip(SBGeoSegment2D const& p1) const;  // 0815ae00
    // the parts of the polyline inside the box, as new polylines in a new list (owned by the caller)
    SBListAutoDelete<SBGeoPolyLine2DList*>* Clip(SBGeoPolyLine2DList const& p1) const;  // 0815ae70
    SBListAutoDelete<SBGeoPolyLine2DList*>* Clip(SBGeoPolyLine2DArray const& p1) const;  // 0815b440
    void ClipAttached(SBGeoPolygon2DList const& p1, SBGeoPolygon2DList& p2) const;  // 0815ba10
    void ClipAttached(SBGeoPolygon2DArray const& p1, SBGeoPolygon2DList& p2) const;  // 0815c840
    ~SBGeoBoundingBox2D();  // 08190b00

    // original size 0x14
    SBGeoPoint2D f_0;  // +0x0   min corner (left, top)
    SBGeoPoint2D f_8;  // +0x8   max corner (right, bottom)
    uint8_t f_10;      // +0x10  the box contains something
};
