// SBGeoIntersection2D (original class; translated from the decompiled game, see engine/TRANSLATING.md)
// The result of intersecting two geometric objects: nothing, or a copy (on the heap) of a point,
// line, half line, segment, polyline or polygon, plus the indices of the segments involved.
#pragma once
#include "sb/SBCommon.h"

class SBGeoHalfLine2D;
class SBGeoLine2D;
class SBGeoPoint2D;
class SBGeoPolyLine2DArray;
class SBGeoPolyLine2DList;
class SBGeoPolygon2DArray;
class SBGeoPolygon2DList;
class SBGeoSegment2D;

class SBGeoIntersection2D {
public:
    // values of f_0
    enum { NONE = 0, POINT = 1, LINE = 2, HALFLINE = 3, SEGMENT = 4, POLYLINE = 5, POLYGON = 6 };

    void Destroy();  // 0815fc60
    void DeleteIntersection();  // 0815fc70
    void Copy(SBGeoIntersection2D const& p1);  // 0815fce0
    uint8_t GetType() const;  // 0815fe00
    SBGeoPoint2D* GetPoint() const;  // 0815fe10
    SBGeoSegment2D* GetSegment() const;  // 0815fe20
    SBGeoHalfLine2D* GetHalfLine() const;  // 0815fe30
    SBGeoLine2D* GetLine() const;  // 0815fe40
    SBGeoPolyLine2DList* GetPolyLine() const;  // 0815fe50
    SBGeoPolygon2DList* GetPolygon() const;  // 0815fe60
    uint16_t GetFirstIndex() const;  // 0815fe70
    uint16_t GetSecondIndex() const;  // 0815fe80
    SBGeoIntersection2D();  // 0815fe90
    SBGeoIntersection2D(SBGeoIntersection2D const& p1);  // 0815fea0
    SBGeoIntersection2D(SBGeoPoint2D const& p1);  // 0815feb0
    SBGeoIntersection2D(SBGeoLine2D const& p1);  // 0815ff00
    SBGeoIntersection2D(SBGeoHalfLine2D const& p1);  // 0815ff50
    SBGeoIntersection2D(SBGeoSegment2D const& p1);  // 0815ffa0
    SBGeoIntersection2D(SBGeoPolyLine2DList const& p1);  // 0815fff0
    SBGeoIntersection2D(SBGeoPolyLine2DArray const& p1);  // 08160040
    SBGeoIntersection2D(SBGeoPolygon2DList const& p1);  // 08160090
    SBGeoIntersection2D(SBGeoPolygon2DArray const& p1);  // 081600e0
    ~SBGeoIntersection2D();  // 08160130
    SBGeoIntersection2D& operator=(SBGeoIntersection2D const& p1);  // 08160140
    bool operator==(SBGeoIntersection2D p1) const;  // 08160170
    bool operator!=(SBGeoIntersection2D p1) const;  // 081601f0
    void SetFirstIndex(unsigned short p1);  // 081602b0
    void SetSecondIndex(unsigned short p1);  // 081602c0
    void ReverseIndexes();  // 081602d0
    void SetNone();  // 081602f0
    void SetPoint(SBGeoPoint2D const& p1);  // 08160310
    void SetLine(SBGeoLine2D const& p1);  // 08160360
    void SetHalfLine(SBGeoHalfLine2D const& p1);  // 081603b0
    void SetSegment(SBGeoSegment2D const& p1);  // 08160400
    void SetPolyLine(SBGeoPolyLine2DList const& p1);  // 08160450
    void SetPolyLine(SBGeoPolyLine2DArray const& p1);  // 081604a0
    void SetPolygon(SBGeoPolygon2DList const& p1);  // 081604f0
    void SetPolygon(SBGeoPolygon2DArray const& p1);  // 08160540
    bool BelongsTo(SBGeoIntersection2D const& p1);  // 08160590

    // original size 0xc
    uint8_t f_0;   // +0x0  type (NONE...POLYGON)
    uint16_t f_2;  // +0x2  first index
    uint16_t f_4;  // +0x4  second index
    void* f_8;     // +0x8  the object (owned), its class given by f_0 (polylines and polygons are
                   //       stored as SBGeoPolyLine2DList / SBGeoPolygon2DList)
};
