// SBGeoPolygon2DArray (original class; translated from the decompiled game, see engine/TRANSLATING.md)
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoPolyLine2DArray.h"

class SBGeoBoundingBox2D;
class SBGeoHalfLine2D;
class SBGeoLine2D;
class SBGeoPoint2D;
class SBGeoPolyLine2DList;
class SBGeoPolygon2DList;
class SBGeoSegment2D;
class SBGeoVector2D;

class SBGeoPolygon2DArray : public SBGeoPolyLine2DArray {
public:
    int Size() const;  // 08163110
    void Get(unsigned int p1) const;  // 08163120
    uint32_t GetSegment(unsigned int p1) const;  // 08166710
    bool IsOnALine();  // 0816b940
    bool IsConvex();  // 0816b980
    void ClipAttached(SBGeoPolygon2DList const& p1, SBGeoPolygon2DList& p2);  // 0816b9d0
    void Copy(SBGeoPolygon2DList const& p1);  // 0816be00
    void Delete();  // 0816be80
    void AddLast(SBGeoPoint2D const& p1);  // 0816be90
    void Copy(SBGeoPolygon2DArray const& p1);  // 0816bea0
    void Destroy();  // 0816bf20
    SBGeoPolygon2DArray();  // 0816bf30
    SBGeoPolygon2DArray(SBGeoPolygon2DArray const& p1);  // 0816bf60
    SBGeoPolygon2DArray(SBGeoPolyLine2DList const& p1);  // 0816c060
    SBGeoPolygon2DArray(SBGeoPolyLine2DArray const& p1);  // 0816c090
    SBGeoPolygon2DArray(SBGeoBoundingBox2D const& p1);  // 0816c0c0
    int32_t GetType() const;  // 0816c2d0
    uint8_t IsValid();  // 0816c2e0
    int32_t IsReallyValid();  // 0816c310
    bool IsReallyConvex();  // 0816c850
    bool IsConcave();  // 0816c870
    int UpdateSpeciesOfPolygon();  // 0816c8c0
    bool IsReallyConcave();  // 0816cbb0
    int GetSpeciesOfPolygon() const;  // 0816cbd0
    void SetSpeciesOfPolygon(short p1);  // 0816cbe0
    bool IsClockwise();  // 0816cc10
    bool IsReallyClockwise();  // 0816cc50
    bool IsCounterClockwise();  // 0816cc70
    bool IsReallyCounterClockwise();  // 0816ccb0
    bool IsReallyOnALine();  // 0816ccd0
    bool IsClockwiseForConcavePolygons();  // 0816ccf0
    int UpdateOrientationOfPolygon();  // 0816cf70
    int GetOrientationOfPolygon() const;  // 0816d210
    void SetOrientationOfPolygon(short p1);  // 0816d220
    uint8_t IsIntersecting(SBGeoLine2D const& p1) const;  // 0816d240
    uint8_t IsIntersecting(SBGeoHalfLine2D const& p1) const;  // 0816d260
    uint8_t IsIntersecting(SBGeoSegment2D const& p1) const;  // 0816d280
    int32_t IsIntersecting(SBGeoPolyLine2DList const& p1) const;  // 0816d2a0
    bool IsInside_p(SBGeoPoint2D const& p1) const;  // 0816d6c0
    int32_t IsIntersecting(SBGeoPolyLine2DArray const& p1) const;  // 0816db80
    uint8_t IsIntersecting(SBGeoPolygon2DList const& p1) const;  // 0816dfa0
    uint8_t IsIntersecting(SBGeoPolygon2DArray const& p1) const;  // 0816e040
    uint8_t IsIntersecting(SBGeoBoundingBox2D const& p1) const;  // 0816e0e0
    uint8_t IsInside_s(SBGeoSegment2D const& p1) const;  // 0816e100
    int32_t IsInside(SBGeoPolyLine2DList const& p1);  // 0816edf0
    uint8_t IsInside(SBGeoBoundingBox2D const& p1);  // 0816ef90
    int32_t IsInside(SBGeoPolyLine2DArray const& p1);  // 0816f0a0
    int32_t IsInside(SBGeoPolygon2DList const& p1);  // 0816f240
    uint32_t IsInside(SBGeoPolygon2DArray const& p1);  // 0816f440
    SBGeoPolygon2DArray * operator=(SBGeoPolygon2DList const& p1);  // 0816f5d0
    SBGeoPolygon2DArray * operator=(SBGeoPolygon2DArray const& p1);  // 0816f650
    SBGeoPolygon2DArray * operator^(SBGeoLine2D const& p1) const;  // 0816f6e0
    void * ConvertToIntersectionList(SBList<SBGeoSegment2D> p1) const;  // 0816f740
    SBGeoLine2D * Clip(SBGeoLine2D const& p1) const;  // 0816fa00
    SBGeoPolygon2DArray * operator^(SBGeoHalfLine2D const& p1) const;  // 0816fae0
    SBGeoHalfLine2D * Clip(SBGeoHalfLine2D const& p1) const;  // 0816fb40
    SBGeoPolygon2DArray * operator^(SBGeoSegment2D const& p1) const;  // 0816fc20
    SBGeoSegment2D * Clip(SBGeoSegment2D const& p1) const;  // 0816fc80
    SBGeoPolygon2DArray * operator^(SBGeoPolyLine2DList const& p1) const;  // 0816fd60
    void * ConvertToIntersectionList(SBListAutoDelete<SBGeoPolyLine2DList*> p1, SBList<unsigned int>& p2) const;  // 0816fef0
    SBGeoPolyLine2DList * MyClip(SBGeoPolyLine2DList const& p1, SBList<unsigned int>& p2) const;  // 08170330
    SBGeoPolygon2DArray * operator^(SBGeoPolyLine2DArray const& p1) const;  // 08170a90
    int32_t GetApproximativeDistanceToBoundary(SBGeoPoint2D& p1, float p2);  // 08170c60
    void Add(SBGeoPoint2D const& p1, unsigned int p2);  // 08171140
    void AddFirst(SBGeoPoint2D const& p1);  // 08171150
    void DeleteFirst();  // 08171160
    void DeleteLast();  // 08171170
    void Delete(unsigned int p1);  // 08171180
    void Delete(unsigned int p1, unsigned int p2);  // 08171190
    void Set(SBGeoPoint2D const& p1, unsigned int p2);  // 081711a0
    SBGeoLine2D * ClipSegmentOrWhatever(SBGeoLine2D const& p1, bool p2, bool p3) const;  // 081711b0
    SBGeoPolygon2DArray * Clip(SBGeoPolyLine2DList const& p1);  // 08173810
    SBGeoPolyLine2DArray * Clip(SBGeoPolyLine2DArray const& p1);  // 081738d0
    void ClipPolygonAgainstHalfplane(SBGeoPolygon2DList& p1, SBGeoVector2D& p2, SBGeoPoint2D& p3, SBGeoPolygon2DList& p4) const;  // 081739b0
    void ClipAttached(SBGeoPolygon2DArray const& p1, SBGeoPolygon2DList& p2);  // 08173e30
    void ClipAttached(SBGeoBoundingBox2D const& p1, SBGeoPolygon2DList& p2);  // 081742b0

    // original size 0x26
    int16_t f_20;  // +0x20  short x25, uint x2, ushort x2, undefined2 x6, undefined4 x3
    int16_t f_22;  // +0x22  short x16, undefined2 x9
    uint16_t f_24;  // +0x24  undefined2 x28, byte x9
    uint8_t f_25;  // +0x25  byte x10
};
