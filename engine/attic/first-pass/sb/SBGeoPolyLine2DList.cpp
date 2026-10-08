// SBGeoPolyLine2DList (see SBGeoPolyLine2DList.h)
#include "sb/SBGeoPolyLine2DList.h"
#include "sb/SBGeoBoundingBox2D.h"
#include "sb/SBGeoHalfLine2D.h"
#include "sb/SBGeoLine2D.h"
#include "sb/SBGeoPolyLine2DArray.h"
#include "sb/SBGeoPolygon2DArray.h"
#include "sb/SBGeoPolygon2DList.h"
#include "sb/SBGeoSegment2D.h"
#include "sb/SBGeoVector2D.h"

// 081751d0  appends the points of p1 (the list is not emptied first)
void SBGeoPolyLine2DList::Copy(SBGeoPolyLine2DList const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) SBList<SBGeoPoint2D>::AddLast(p1[i]);
}

// 081752a0
int32_t SBGeoPolyLine2DList::Size() const {
    return (int32_t)GetCount();
}

// 081752b0
void SBGeoPolyLine2DList::AddLast(SBGeoPoint2D const& p1) {
    SBList<SBGeoPoint2D>::AddLast(p1);
}

// 081752e0
SBGeoPoint2D const& SBGeoPolyLine2DList::Get(unsigned int p1) const {
    return (*this)[p1];
}

// 08175370  appends
void SBGeoPolyLine2DList::Copy(SBGeoPolyLine2DArray const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) SBList<SBGeoPoint2D>::AddLast(p1.items[i]);
}

// 08175420
void SBGeoPolyLine2DList::Destroy() {}

// 08175430
SBGeoPolyLine2DList::SBGeoPolyLine2DList() {}

// 081754b0
SBGeoPolyLine2DList::SBGeoPolyLine2DList(SBGeoPolyLine2DList const& p1) : SBList<SBGeoPoint2D>() {
    Copy(p1);
}

// 08175540
SBGeoPolyLine2DList::SBGeoPolyLine2DList(SBGeoPolyLine2DArray const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) SBList<SBGeoPoint2D>::AddLast(p1.items[i]);
}

// 08175600
SBGeoPolyLine2DList::SBGeoPolyLine2DList(SBGeoPoint2D const& p1) {
    SBList<SBGeoPoint2D>::AddLast(p1);
}

// 081756a0  (sic) appends p1 to this polyline instead of replacing it
SBGeoPolyLine2DList& SBGeoPolyLine2DList::operator=(SBGeoPolyLine2DList const& p1) {
    if (this != &p1) Copy(p1);
    return *this;
}

// 081756d0
int32_t SBGeoPolyLine2DList::GetType() const {
    return 1;
}

// 081756e0
bool SBGeoPolyLine2DList::IsIntersecting(SBGeoLine2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// 08175700
bool SBGeoPolyLine2DList::IsIntersecting(SBGeoHalfLine2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// 08175720
bool SBGeoPolyLine2DList::IsIntersecting(SBGeoSegment2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// 08175740
bool SBGeoPolyLine2DList::IsIntersecting(SBGeoPolyLine2DList const& p1) const {
    int n = (int)GetCount();
    if (n == 0) return false;
    uint32_t i = 0;
    bool r = false;
    if (n != 1) {
        do {
            SBGeoSegment2D s = GetSegment(i);
            r = s.IsIntersecting(p1);
            i++;
            if ((uint32_t)(n - 1) <= i) return r;
        } while (r != true);
    }
    return r;
}

// 081757c0  the segment from point p1 to point p1 + 1
SBGeoSegment2D SBGeoPolyLine2DList::GetSegment(unsigned int p1) const {
    SBGeoPoint2D const& a = (*this)[p1];
    SBGeoPoint2D const& b = (*this)[p1 + 1];
    return SBGeoSegment2D(a, b);
}

// 081758f0
bool SBGeoPolyLine2DList::IsIntersecting(SBGeoPolygon2DList const& p1) const {
    int n = (int)GetCount();
    if (n == 0) return false;
    uint32_t i = 0;
    bool r = false;
    if (n != 1) {
        do {
            SBGeoSegment2D s = GetSegment(i);
            r = p1.IsIntersecting(s);
            i++;
            if ((uint32_t)(n - 1) <= i) return r;
        } while (r != true);
    }
    return r;
}

// 08175970
bool SBGeoPolyLine2DList::IsIntersecting(SBGeoPolygon2DArray const& p1) const {
    int n = (int)GetCount();
    if (n == 0) return false;
    uint32_t i = 0;
    bool r = false;
    if (n != 1) {
        do {
            SBGeoSegment2D s = GetSegment(i);
            r = p1.IsIntersecting(s);
            i++;
            if ((uint32_t)(n - 1) <= i) return r;
        } while (r != true);
    }
    return r;
}

// 081759f0
bool SBGeoPolyLine2DList::IsIntersecting(SBGeoBoundingBox2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// (sic) the original copies each result and reverses the indices of the copy only, so the
// indices are not reversed in the returned list
static void ReverseCopies(SBList<SBGeoIntersection2D>& res) {
    SBGeoIntersection2D tmp;
    uint32_t n = res.GetCount() & 0xffff;
    for (uint32_t i = 0; i < n; i = (i + 1) & 0xffff) {
        tmp = res[i];
        tmp.ReverseIndexes();
    }
}

// 08175a10
SBList<SBGeoIntersection2D> SBGeoPolyLine2DList::operator^(SBGeoLine2D const& p1) const {
    SBList<SBGeoIntersection2D> res;
    res = p1 ^ *this;
    ReverseCopies(res);
    return res;
}

// 08175c00
SBList<SBGeoIntersection2D> SBGeoPolyLine2DList::operator^(SBGeoHalfLine2D const& p1) const {
    SBList<SBGeoIntersection2D> res;
    res = p1 ^ *this;
    ReverseCopies(res);
    return res;
}

// 08175df0
SBList<SBGeoIntersection2D> SBGeoPolyLine2DList::operator^(SBGeoSegment2D const& p1) const {
    SBList<SBGeoIntersection2D> res;
    res = p1 ^ *this;
    ReverseCopies(res);
    return res;
}

// 08175fe0  inserts before point p2
void SBGeoPolyLine2DList::Add(SBGeoPoint2D const& p1, unsigned int p2) {
    InsertAt(p2, p1);
}

// 08175ff0
void SBGeoPolyLine2DList::AddFirst(SBGeoPoint2D const& p1) {
    SBList<SBGeoPoint2D>::AddFirst(p1);
}

// 08176030
void SBGeoPolyLine2DList::Add(SBList<SBGeoPoint2D> const& p1, unsigned int p2) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) InsertAt(i + p2, p1[i]);
}

// 08176100
void SBGeoPolyLine2DList::Add(SBArray<SBGeoPoint2D> const& p1, unsigned int p2) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) InsertAt(p2 + i, p1[i]);
}

// 08176170  (sic) each point is put first in turn, so p1 ends up reversed
void SBGeoPolyLine2DList::AddFirst(SBList<SBGeoPoint2D> const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) SBList<SBGeoPoint2D>::AddFirst(p1[i]);
}

// 08176260  (sic) reversed as well
void SBGeoPolyLine2DList::AddFirst(SBArray<SBGeoPoint2D> const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) SBList<SBGeoPoint2D>::AddFirst(p1[i]);
}

// 081762f0
void SBGeoPolyLine2DList::AddLast(SBList<SBGeoPoint2D> const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) SBList<SBGeoPoint2D>::AddLast(p1[i]);
}

// 081763c0
void SBGeoPolyLine2DList::AddLast(SBArray<SBGeoPoint2D> const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) SBList<SBGeoPoint2D>::AddLast(p1[i]);
}

// 08176450
void SBGeoPolyLine2DList::Delete() {
    RemoveAll();
}

// 081764a0
void SBGeoPolyLine2DList::DeleteFirst() {
    items.pop_front();
}

// 081764e0
void SBGeoPolyLine2DList::DeleteLast() {
    items.pop_back();
}

// 08176520
void SBGeoPolyLine2DList::Delete(unsigned int p1) {
    RemoveAt(p1);
}

// 081765d0  deletes the points p1 to p2 (both included)
void SBGeoPolyLine2DList::Delete(unsigned int p1, unsigned int p2) {
    iterator first = begin();
    std::advance(first, p1);
    iterator last = begin();
    std::advance(last, p2);
    ++last;
    if (first != last) items.erase(first, last);
}

// 081765e0
void SBGeoPolyLine2DList::Set(SBGeoPoint2D const& p1, unsigned int p2) {
    (*this)[p2] = p1;
}

// every segment of a against every segment of b (shared by the two operators below); the first
// index is the segment of a, the second the segment of b. A point equal to (an end of) the
// previous result found on the next segments is skipped.
template <class B>
static SBList<SBGeoIntersection2D> PolyLineXPolyLine(SBGeoPolyLine2DList const& a, B const& b) {
    SBList<SBGeoIntersection2D> res;
    SBGeoPoint2D a0, a1, b0, b1;
    SBGeoSegment2D sa(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoSegment2D sb(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoIntersection2D cur;
    SBGeoIntersection2D prev;
    if (a.GetCount() != 0) a0 = a[0];  // (the original reads point 0 even of an empty polyline)
    uint32_t n = a.GetCount() & 0xffff;
    if (n < 2) return res;
    uint32_t m = b.GetCount() & 0xffff;
    uint32_t i = 1;
    do {
        a1 = a[i];
        sa.f_0 = a0;
        sa.f_8 = a1;
        if (b.GetCount() != 0) b0 = b[0];
        if (1 < m) {
            uint32_t j = 1;
            do {
                b1 = b[j];
                sb.f_0 = b0;
                sb.f_8 = b1;
                if (sa.IsIntersecting(sb)) {
                    prev = cur;
                    cur = sa ^ sb;
                    if (res.GetCount() == 0 || prev.GetFirstIndex() != i - 1 || prev.GetSecondIndex() != j - 1 ||
                        !cur.BelongsTo(res.Last())) {
                        cur.SetFirstIndex((uint16_t)(i - 1));
                        cur.SetSecondIndex((uint16_t)(j - 1));
                        res.AddLast(cur);
                    }
                }
                b0 = b1;
                j = (j + 1) & 0xffff;
            } while (j < m);
        }
        a0 = a1;
        i = (i + 1) & 0xffff;
    } while (i < n);
    return res;
}

// 08176680
SBList<SBGeoIntersection2D> SBGeoPolyLine2DList::operator^(SBGeoPolyLine2DList const& p1) const {
    return PolyLineXPolyLine(*this, p1);
}

// 08176e60
SBList<SBGeoIntersection2D> SBGeoPolyLine2DList::operator^(SBGeoPolyLine2DArray const& p1) const {
    return PolyLineXPolyLine(*this, p1);
}

// 08179810  (the count is taken modulo 0x10000, as the original)
void SBGeoPolyLine2DList::operator+=(SBGeoVector2D const& p1) {
    uint32_t n = GetCount() & 0xffff;
    for (uint32_t i = 0; i < n; i = (i + 1) & 0xffff) (*this)[i] += p1;
}

// 083362f0
SBGeoPolyLine2DList::~SBGeoPolyLine2DList() {}
