// SBGeoPolyLine2DList (original class; translated from the decompiled game, see engine/TRANSLATING.md)
// An open polyline stored in a list of points. In the original it is an SBList<SBGeoPoint2D>
// (40 bytes: the 24-byte list header plus the list's spare element at +0x20); SBGeoPolygon2DList
// derives from it.
#pragma once
#include "sb/SBCommon.h"
#include "sb/SBGeoPoint2D.h"
#include "sb/SBGeoIntersection2D.h"

class SBGeoBoundingBox2D;
class SBGeoHalfLine2D;
class SBGeoLine2D;
class SBGeoPolyLine2DArray;
class SBGeoPolygon2DArray;
class SBGeoPolygon2DList;
class SBGeoSegment2D;
class SBGeoVector2D;

class SBGeoPolyLine2DList : public SBList<SBGeoPoint2D> {
public:
    void Copy(SBGeoPolyLine2DList const& p1);  // 081751d0
    int32_t Size() const;  // 081752a0
    void AddLast(SBGeoPoint2D const& p1);  // 081752b0
    SBGeoPoint2D const& Get(unsigned int p1) const;  // 081752e0
    void Copy(SBGeoPolyLine2DArray const& p1);  // 08175370
    void Destroy();  // 08175420
    SBGeoPolyLine2DList();  // 08175430
    SBGeoPolyLine2DList(SBGeoPolyLine2DList const& p1);  // 081754b0
    SBGeoPolyLine2DList(SBGeoPolyLine2DArray const& p1);  // 08175540
    SBGeoPolyLine2DList(SBGeoPoint2D const& p1);  // 08175600
    SBGeoPolyLine2DList& operator=(SBGeoPolyLine2DList const& p1);  // 081756a0
    int32_t GetType() const;  // 081756d0
    bool IsIntersecting(SBGeoLine2D const& p1) const;  // 081756e0
    bool IsIntersecting(SBGeoHalfLine2D const& p1) const;  // 08175700
    bool IsIntersecting(SBGeoSegment2D const& p1) const;  // 08175720
    bool IsIntersecting(SBGeoPolyLine2DList const& p1) const;  // 08175740
    SBGeoSegment2D GetSegment(unsigned int p1) const;  // 081757c0
    bool IsIntersecting(SBGeoPolygon2DList const& p1) const;  // 081758f0
    bool IsIntersecting(SBGeoPolygon2DArray const& p1) const;  // 08175970
    bool IsIntersecting(SBGeoBoundingBox2D const& p1) const;  // 081759f0
    SBList<SBGeoIntersection2D> operator^(SBGeoLine2D const& p1) const;  // 08175a10
    SBList<SBGeoIntersection2D> operator^(SBGeoHalfLine2D const& p1) const;  // 08175c00
    SBList<SBGeoIntersection2D> operator^(SBGeoSegment2D const& p1) const;  // 08175df0
    void Add(SBGeoPoint2D const& p1, unsigned int p2);  // 08175fe0
    void AddFirst(SBGeoPoint2D const& p1);  // 08175ff0
    void Add(SBList<SBGeoPoint2D> const& p1, unsigned int p2);  // 08176030
    void Add(SBArray<SBGeoPoint2D> const& p1, unsigned int p2);  // 08176100
    void AddFirst(SBList<SBGeoPoint2D> const& p1);  // 08176170
    void AddFirst(SBArray<SBGeoPoint2D> const& p1);  // 08176260
    void AddLast(SBList<SBGeoPoint2D> const& p1);  // 081762f0
    void AddLast(SBArray<SBGeoPoint2D> const& p1);  // 081763c0
    void Delete();  // 08176450
    void DeleteFirst();  // 081764a0
    void DeleteLast();  // 081764e0
    void Delete(unsigned int p1);  // 08176520
    void Delete(unsigned int p1, unsigned int p2);  // 081765d0
    void Set(SBGeoPoint2D const& p1, unsigned int p2);  // 081765e0
    SBList<SBGeoIntersection2D> operator^(SBGeoPolyLine2DList const& p1) const;  // 08176680
    SBList<SBGeoIntersection2D> operator^(SBGeoPolyLine2DArray const& p1) const;  // 08176e60
    void operator+=(SBGeoVector2D const& p1);  // 08179810
    ~SBGeoPolyLine2DList();  // 083362f0

    // original size 0x28 (all of it is the SBList base)
};
