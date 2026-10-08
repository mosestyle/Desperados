// SBGeoPolygon2DList (original class; translated from the decompiled game, see engine/TRANSLATING.md)
#pragma once
#include "sb/SBCommon.h"

class SBGeoBoundingBox2D;
class SBGeoHalfLine2D;
class SBGeoLine2D;
class SBGeoPoint2D;
class SBGeoPolyLine2DArray;
class SBGeoPolyLine2DList;
class SBGeoPolygon2DArray;
class SBGeoSegment2D;
class SBGeoVector2D;

class SBGeoPolygon2DList {
public:
    void Copy(SBGeoPolygon2DList const& p1);  // 08162fc0
    int32_t Size() const;  // 08163040
    void Delete();  // 08163050
    void AddLast(SBGeoPoint2D const& p1);  // 08163060
    void Get(unsigned int p1) const;  // 08163070
    void Copy(SBGeoPolygon2DArray const& p1);  // 08163090
    void Destroy();  // 08163140
    SBGeoPolygon2DList();  // 08163150
    SBGeoPolygon2DList(SBGeoPolygon2DList const& p1);  // 08163180
    SBGeoPolygon2DList(SBGeoPolygon2DArray const& p1);  // 08163220
    SBGeoPolygon2DList(SBGeoPolyLine2DList const& p1);  // 081632c0
    SBGeoPolygon2DList(SBGeoBoundingBox2D const& p1);  // 081632f0
    int32_t GetType() const;  // 081634b0
    uint8_t IsValid();  // 081634c0
    int32_t IsReallyValid();  // 081634f0
    bool IsConvex();  // 08163a30
    bool IsReallyConvex();  // 08163a80
    bool IsConcave();  // 08163aa0
    uint32_t GetSegment(unsigned int p1) const;  // 08163af0
    int UpdateSpeciesOfPolygon();  // 08163c40
    bool IsReallyConcave();  // 08163f30
    int GetSpeciesOfPolygon() const;  // 08163f50
    void SetSpeciesOfPolygon(short p1);  // 08163f60
    bool IsClockwise();  // 08163f90
    bool IsReallyClockwise();  // 08163fd0
    bool IsCounterClockwise();  // 08163ff0
    bool IsReallyCounterClockwise();  // 08164030
    bool IsOnALine();  // 08164050
    bool IsReallyOnALine();  // 08164090
    bool IsClockwiseForConcavePolygons();  // 081640b0
    int UpdateOrientationOfPolygon();  // 08164330
    int GetOrientationOfPolygon() const;  // 081645d0
    void SetOrientationOfPolygon(short p1);  // 081645e0
    uint8_t IsIntersecting(SBGeoLine2D const& p1) const;  // 08164600
    uint8_t IsIntersecting(SBGeoHalfLine2D const& p1) const;  // 08164620
    uint8_t IsIntersecting(SBGeoSegment2D const& p1) const;  // 08164640
    int32_t IsIntersecting(SBGeoPolyLine2DList const& p1) const;  // 08164660
    bool IsInside_p(SBGeoPoint2D const& p1) const;  // 081649a0
    int32_t IsIntersecting(SBGeoPolyLine2DArray const& p1) const;  // 08164e60
    uint8_t IsIntersecting(SBGeoPolygon2DList const& p1) const;  // 081651a0
    uint8_t IsIntersecting(SBGeoPolygon2DArray const& p1) const;  // 08165240
    uint8_t IsIntersecting(SBGeoBoundingBox2D const& p1) const;  // 081652e0
    uint8_t IsInside_s(SBGeoSegment2D const& p1) const;  // 08165300
    int32_t IsInside(SBGeoPolyLine2DList const& p1);  // 08165ff0
    int32_t IsInside(SBGeoPolyLine2DArray const& p1);  // 08166140
    uint8_t IsInside(SBGeoBoundingBox2D const& p1);  // 08166320
    uint32_t IsInside(SBGeoPolygon2DList const& p1);  // 08166370
    uint32_t IsInside(SBGeoPolygon2DArray const& p1);  // 08166540
    SBGeoPolygon2DList * operator=(SBGeoPolygon2DList const& p1);  // 08166860
    SBGeoPolygon2DList * operator=(SBGeoPolygon2DArray const& p1);  // 081668f0
    SBGeoPolygon2DList * operator^(SBGeoLine2D const& p1) const;  // 08166970
    void * ConvertToIntersectionList(SBList<SBGeoSegment2D> p1) const;  // 081669d0
    SBGeoLine2D * Clip(SBGeoLine2D const& p1) const;  // 08166c90
    SBGeoPolygon2DList * operator^(SBGeoHalfLine2D const& p1) const;  // 08166d70
    SBGeoHalfLine2D * Clip(SBGeoHalfLine2D const& p1) const;  // 08166dd0
    SBGeoPolygon2DList * operator^(SBGeoSegment2D const& p1) const;  // 08166eb0
    SBGeoSegment2D * Clip(SBGeoSegment2D const& p1) const;  // 08166f10
    SBGeoPolygon2DList * operator^(SBGeoPolyLine2DList const& p1) const;  // 08166ff0
    void * ConvertToIntersectionList(SBListAutoDelete<SBGeoPolyLine2DList*> p1, SBList<unsigned int>& p2) const;  // 08167180
    SBGeoPolyLine2DList * MyClip(SBGeoPolyLine2DList const& p1, SBList<unsigned int>& p2) const;  // 081675c0
    SBGeoPolygon2DList * operator^(SBGeoPolyLine2DArray const& p1) const;  // 08167d20
    int32_t GetApproximativeDistanceToBoundary(SBGeoPoint2D& p1, float p2);  // 08167ef0
    void Add(SBGeoPoint2D const& p1, unsigned int p2);  // 081683d0
    void AddFirst(SBGeoPoint2D const& p1);  // 081683e0
    void DeleteFirst();  // 081683f0
    void DeleteLast();  // 08168400
    void Delete(unsigned int p1);  // 08168410
    void Delete(unsigned int p1, unsigned int p2);  // 08168420
    void Set(SBGeoPoint2D const& p1, unsigned int p2);  // 08168430
    SBGeoLine2D * ClipSegmentOrWhatever(SBGeoLine2D const& p1, bool p2, bool p3) const;  // 08168440
    SBGeoPolygon2DList * Clip(SBGeoPolyLine2DList const& p1);  // 0816aaa0
    SBGeoPolyLine2DArray * Clip(SBGeoPolyLine2DArray const& p1);  // 0816ab60
    void ClipAttached(SBGeoPolygon2DList const& p1, SBGeoPolygon2DList& p2);  // 0816ac40
    void ClipPolygonAgainstHalfplane(SBGeoPolygon2DList& p1, SBGeoVector2D& p2, SBGeoPoint2D& p3, SBGeoPolygon2DList& p4) const;  // 0816b040
    void ClipAttached(SBGeoPolygon2DArray const& p1, SBGeoPolygon2DList& p2);  // 0816b4c0
    void ClipAttached(SBGeoBoundingBox2D const& p1, SBGeoPolygon2DList& p2);  // 0816bdd0

    // original size 0x30
    int32_t f_8;  // +0x8  undefined4 x1
    int16_t f_28;  // +0x28  short x26, undefined2 x6, undefined4 x2, uint x2, ushort x2
    int16_t f_2a;  // +0x2a  short x17, undefined2 x9
    uint16_t f_2c;  // +0x2c  undefined2 x29, byte x10
    uint8_t f_2d;  // +0x2d  byte x11
};
