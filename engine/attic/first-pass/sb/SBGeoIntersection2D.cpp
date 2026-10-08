// SBGeoIntersection2D (see SBGeoIntersection2D.h)
#include "sb/SBGeoIntersection2D.h"
#include "sb/SBGeoHalfLine2D.h"
#include "sb/SBGeoLine2D.h"
#include "sb/SBGeoPoint2D.h"
#include "sb/SBGeoPolyLine2DArray.h"
#include "sb/SBGeoPolyLine2DList.h"
#include "sb/SBGeoPolygon2DArray.h"
#include "sb/SBGeoPolygon2DList.h"
#include "sb/SBGeoSegment2D.h"

// 0815fc60  (same code as DeleteIntersection)
void SBGeoIntersection2D::Destroy() {
    DeleteIntersection();
}

// 0815fc70  frees the object; the type is left unchanged
void SBGeoIntersection2D::DeleteIntersection() {
    switch (f_0) {
    case POINT:
        if (f_8 == nullptr) return;
        delete (SBGeoPoint2D*)f_8;
        break;
    case LINE:
        if (f_8 == nullptr) return;
        delete (SBGeoLine2D*)f_8;
        break;
    case HALFLINE:
        if (f_8 == nullptr) return;
        delete (SBGeoHalfLine2D*)f_8;
        break;
    case SEGMENT:
        if (f_8 == nullptr) return;
        delete (SBGeoSegment2D*)f_8;
        break;
    case POLYLINE:
        if (f_8 == nullptr) return;
        delete (SBGeoPolyLine2DList*)f_8;
        break;
    case POLYGON:
        if (f_8 == nullptr) return;
        delete (SBGeoPolygon2DList*)f_8;
        break;
    default:
        break;
    }
}

// 0815fce0  (does not free a previous object)
void SBGeoIntersection2D::Copy(SBGeoIntersection2D const& p1) {
    switch (p1.f_0) {
    case POINT: f_8 = new SBGeoPoint2D(*(SBGeoPoint2D*)p1.f_8); break;
    case LINE: f_8 = new SBGeoLine2D(*(SBGeoLine2D*)p1.f_8); break;
    case HALFLINE: f_8 = new SBGeoHalfLine2D(*(SBGeoHalfLine2D*)p1.f_8); break;
    case SEGMENT: f_8 = new SBGeoSegment2D(*(SBGeoSegment2D*)p1.f_8); break;
    case POLYLINE: f_8 = new SBGeoPolyLine2DList(*(SBGeoPolyLine2DList*)p1.f_8); break;
    case POLYGON: f_8 = new SBGeoPolygon2DList(*(SBGeoPolygon2DList*)p1.f_8); break;
    default: break;
    }
    f_0 = p1.f_0;
    f_2 = p1.f_2;
    f_4 = p1.f_4;
}

// 0815fe00
uint8_t SBGeoIntersection2D::GetType() const { return f_0; }

// 0815fe10
SBGeoPoint2D* SBGeoIntersection2D::GetPoint() const { return (SBGeoPoint2D*)f_8; }

// 0815fe20
SBGeoSegment2D* SBGeoIntersection2D::GetSegment() const { return (SBGeoSegment2D*)f_8; }

// 0815fe30
SBGeoHalfLine2D* SBGeoIntersection2D::GetHalfLine() const { return (SBGeoHalfLine2D*)f_8; }

// 0815fe40
SBGeoLine2D* SBGeoIntersection2D::GetLine() const { return (SBGeoLine2D*)f_8; }

// 0815fe50
SBGeoPolyLine2DList* SBGeoIntersection2D::GetPolyLine() const { return (SBGeoPolyLine2DList*)f_8; }

// 0815fe60
SBGeoPolygon2DList* SBGeoIntersection2D::GetPolygon() const { return (SBGeoPolygon2DList*)f_8; }

// 0815fe70
uint16_t SBGeoIntersection2D::GetFirstIndex() const { return f_2; }

// 0815fe80
uint16_t SBGeoIntersection2D::GetSecondIndex() const { return f_4; }

// 0815fe90  (the object pointer is left uninitialized in the original)
SBGeoIntersection2D::SBGeoIntersection2D() {
    f_0 = NONE;
    f_2 = 0;
    f_4 = 0;
    f_8 = nullptr;
}

// 0815fea0
SBGeoIntersection2D::SBGeoIntersection2D(SBGeoIntersection2D const& p1) {
    f_8 = nullptr;
    Copy(p1);
}

// 0815feb0
SBGeoIntersection2D::SBGeoIntersection2D(SBGeoPoint2D const& p1) {
    f_0 = POINT;
    f_2 = 0;
    f_4 = 0;
    f_8 = new SBGeoPoint2D(p1);
}

// 0815ff00
SBGeoIntersection2D::SBGeoIntersection2D(SBGeoLine2D const& p1) {
    f_0 = LINE;
    f_2 = 0;
    f_4 = 0;
    f_8 = new SBGeoLine2D(p1);
}

// 0815ff50
SBGeoIntersection2D::SBGeoIntersection2D(SBGeoHalfLine2D const& p1) {
    f_0 = HALFLINE;
    f_2 = 0;
    f_4 = 0;
    f_8 = new SBGeoHalfLine2D(p1);
}

// 0815ffa0
SBGeoIntersection2D::SBGeoIntersection2D(SBGeoSegment2D const& p1) {
    f_0 = SEGMENT;
    f_2 = 0;
    f_4 = 0;
    f_8 = new SBGeoSegment2D(p1);
}

// 0815fff0
SBGeoIntersection2D::SBGeoIntersection2D(SBGeoPolyLine2DList const& p1) {
    f_0 = POLYLINE;
    f_2 = 0;
    f_4 = 0;
    f_8 = new SBGeoPolyLine2DList(p1);
}

// 08160040
SBGeoIntersection2D::SBGeoIntersection2D(SBGeoPolyLine2DArray const& p1) {
    f_0 = POLYLINE;
    f_2 = 0;
    f_4 = 0;
    f_8 = new SBGeoPolyLine2DList(p1);
}

// 08160090
SBGeoIntersection2D::SBGeoIntersection2D(SBGeoPolygon2DList const& p1) {
    f_0 = POLYGON;
    f_2 = 0;
    f_4 = 0;
    f_8 = new SBGeoPolygon2DList(p1);
}

// 081600e0
SBGeoIntersection2D::SBGeoIntersection2D(SBGeoPolygon2DArray const& p1) {
    f_0 = POLYGON;
    f_2 = 0;
    f_4 = 0;
    f_8 = new SBGeoPolygon2DList(p1);
}

// 08160130
SBGeoIntersection2D::~SBGeoIntersection2D() {
    DeleteIntersection();
}

// 08160140
SBGeoIntersection2D& SBGeoIntersection2D::operator=(SBGeoIntersection2D const& p1) {
    if (this != &p1) {
        DeleteIntersection();
        Copy(p1);
    }
    return *this;
}

// 08160170  polylines and polygons never compare equal
bool SBGeoIntersection2D::operator==(SBGeoIntersection2D p1) const {
    if (f_0 != p1.f_0) return false;
    switch (f_0) {
    case NONE: return true;
    case POINT: return *(SBGeoPoint2D*)f_8 == *(SBGeoPoint2D*)p1.f_8;
    case LINE: return *(SBGeoLine2D*)f_8 == *(SBGeoLine2D*)p1.f_8;
    case HALFLINE: return *(SBGeoHalfLine2D*)f_8 == *(SBGeoHalfLine2D*)p1.f_8;
    case SEGMENT: return *(SBGeoSegment2D*)f_8 == *(SBGeoSegment2D*)p1.f_8;
    default: return false;
    }
}

// 081601f0
bool SBGeoIntersection2D::operator!=(SBGeoIntersection2D p1) const {
    return !(*this == p1);
}

// 081602b0
void SBGeoIntersection2D::SetFirstIndex(unsigned short p1) { f_2 = p1; }

// 081602c0
void SBGeoIntersection2D::SetSecondIndex(unsigned short p1) { f_4 = p1; }

// 081602d0
void SBGeoIntersection2D::ReverseIndexes() {
    uint16_t t = f_4;
    f_4 = f_2;
    f_2 = t;
}

// 081602f0
void SBGeoIntersection2D::SetNone() {
    DeleteIntersection();
    f_0 = NONE;
}

// 08160310
void SBGeoIntersection2D::SetPoint(SBGeoPoint2D const& p1) {
    DeleteIntersection();
    f_0 = POINT;
    f_8 = new SBGeoPoint2D(p1);
}

// 08160360
void SBGeoIntersection2D::SetLine(SBGeoLine2D const& p1) {
    DeleteIntersection();
    f_0 = LINE;
    f_8 = new SBGeoLine2D(p1);
}

// 081603b0
void SBGeoIntersection2D::SetHalfLine(SBGeoHalfLine2D const& p1) {
    DeleteIntersection();
    f_0 = HALFLINE;
    f_8 = new SBGeoHalfLine2D(p1);
}

// 08160400
void SBGeoIntersection2D::SetSegment(SBGeoSegment2D const& p1) {
    DeleteIntersection();
    f_0 = SEGMENT;
    f_8 = new SBGeoSegment2D(p1);
}

// 08160450
void SBGeoIntersection2D::SetPolyLine(SBGeoPolyLine2DList const& p1) {
    DeleteIntersection();
    f_0 = POLYLINE;
    f_8 = new SBGeoPolyLine2DList(p1);
}

// 081604a0
void SBGeoIntersection2D::SetPolyLine(SBGeoPolyLine2DArray const& p1) {
    DeleteIntersection();
    f_0 = POLYLINE;
    f_8 = new SBGeoPolyLine2DList(p1);
}

// 081604f0
void SBGeoIntersection2D::SetPolygon(SBGeoPolygon2DList const& p1) {
    DeleteIntersection();
    f_0 = POLYGON;
    f_8 = new SBGeoPolygon2DList(p1);
}

// 08160540
void SBGeoIntersection2D::SetPolygon(SBGeoPolygon2DArray const& p1) {
    DeleteIntersection();
    f_0 = POLYGON;
    f_8 = new SBGeoPolygon2DList(p1);
}

// 08160590  is this point one of the ends of the segment p1 (or the point p1)?
bool SBGeoIntersection2D::BelongsTo(SBGeoIntersection2D const& p1) {
    if (f_0 != POINT) return false;
    if (p1.f_0 == SEGMENT) {
        if (*(SBGeoPoint2D*)f_8 == *(SBGeoPoint2D*)p1.f_8) return true;
        return *(SBGeoPoint2D*)f_8 == ((SBGeoSegment2D*)p1.f_8)->f_8;
    }
    if (p1.f_0 == POINT) return *(SBGeoPoint2D*)f_8 == *(SBGeoPoint2D*)p1.f_8;
    return false;
}
