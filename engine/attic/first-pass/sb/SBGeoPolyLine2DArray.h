// SBGeoPolyLine2DArray (original class; translated from the decompiled game, see engine/TRANSLATING.md)
// An open polyline stored in an array of points. In the original it is an SBArray<SBGeoPoint2D>
// (32 bytes: the array header plus the spare element at +0x18); SBGeoPolygon2DArray derives
// from it (the original has RTTI for both but no virtual methods).
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoPoint2D.h"
#include "sb/SBGeoIntersection2D.h"

class SBGeoBoundingBox2D;
class SBGeoHalfLine2D;
class SBGeoLine2D;
class SBGeoPolyLine2DList;
class SBGeoPolygon2DArray;
class SBGeoPolygon2DList;
class SBGeoSegment2D;
class SBGeoVector2D;

class SBGeoPolyLine2DArray : public SBArray<SBGeoPoint2D> {
public:
    int32_t Size() const;  // 08175400
    SBGeoPoint2D const& Get(unsigned int p1) const;  // 08175410
    void Copy(SBGeoPolyLine2DList const& p1);  // 08177520
    void AddLast(SBGeoPoint2D const& p1);  // 08177610
    void Copy(SBGeoPolyLine2DArray const& p1);  // 08177650
    void Destroy();  // 081776c0
    SBGeoPolyLine2DArray();  // 081776d0
    SBGeoPolyLine2DArray(SBGeoPolyLine2DArray const& p1);  // 08177750
    SBGeoPolyLine2DArray(SBGeoPolyLine2DList const& p1);  // 08177880
    SBGeoPolyLine2DArray(SBGeoPoint2D const& p1);  // 08177960
    SBGeoPolyLine2DArray& operator=(SBGeoPolyLine2DArray const& p1);  // 08177a60
    int32_t GetType() const;  // 08177ae0
    bool IsIntersecting(SBGeoLine2D const& p1) const;  // 08177af0
    bool IsIntersecting(SBGeoHalfLine2D const& p1) const;  // 08177b10
    bool IsIntersecting(SBGeoSegment2D const& p1) const;  // 08177b30
    bool IsIntersecting(SBGeoPolyLine2DArray const& p1) const;  // 08177b50
    SBGeoSegment2D GetSegment(unsigned int p1) const;  // 08177c00
    bool IsIntersecting(SBGeoPolygon2DList const& p1) const;  // 08177c40
    bool IsIntersecting(SBGeoPolygon2DArray const& p1) const;  // 08177cf0
    bool IsIntersecting(SBGeoBoundingBox2D const& p1) const;  // 08177da0
    SBList<SBGeoIntersection2D> operator^(SBGeoLine2D const& p1) const;  // 08177dc0
    SBList<SBGeoIntersection2D> operator^(SBGeoHalfLine2D const& p1) const;  // 08177fb0
    SBList<SBGeoIntersection2D> operator^(SBGeoSegment2D const& p1) const;  // 081781a0
    void Add(SBGeoPoint2D const& p1, unsigned int p2);  // 08178390
    void AddFirst(SBGeoPoint2D const& p1);  // 08178410
    void Add(SBList<SBGeoPoint2D> const& p1, unsigned int p2);  // 08178450
    void Add(SBArray<SBGeoPoint2D> const& p1, unsigned int p2);  // 08178590
    void AddFirst(SBList<SBGeoPoint2D> const& p1);  // 08178670
    void AddFirst(SBArray<SBGeoPoint2D> const& p1);  // 08178750
    void AddLast(SBList<SBGeoPoint2D> const& p1);  // 081787e0
    void AddLast(SBArray<SBGeoPoint2D> const& p1);  // 081788d0
    void Delete();  // 08178940
    void DeleteFirst();  // 08178980
    void DeleteLast();  // 081789f0
    void Delete(unsigned int p1);  // 08178a20
    void Delete(unsigned int p1, unsigned int p2);  // 08178a90
    void Set(SBGeoPoint2D const& p1, unsigned int p2);  // 08178b30
    SBList<SBGeoIntersection2D> operator^(SBGeoPolyLine2DList const& p1) const;  // 08178b60
    SBList<SBGeoIntersection2D> operator^(SBGeoPolyLine2DArray const& p1) const;  // 08179240
    void operator+=(SBGeoVector2D const& p1);  // 081798e0

    // original size 0x20 (all of it is the SBArray base)
};
