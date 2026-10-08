// SBGeoPolyLine2DArray (see SBGeoPolyLine2DArray.h)
#include "sb/SBGeoPolyLine2DArray.h"
#include "sb/SBGeoBoundingBox2D.h"
#include "sb/SBGeoHalfLine2D.h"
#include "sb/SBGeoLine2D.h"
#include "sb/SBGeoPolyLine2DList.h"
#include "sb/SBGeoPolygon2DArray.h"
#include "sb/SBGeoPolygon2DList.h"
#include "sb/SBGeoSegment2D.h"
#include "sb/SBGeoVector2D.h"

// 08175400
int32_t SBGeoPolyLine2DArray::Size() const {
    return (int32_t)GetCount();
}

// 08175410  (no bounds check)
SBGeoPoint2D const& SBGeoPolyLine2DArray::Get(unsigned int p1) const {
    return items.data()[p1];
}

// 08177520  appends the points of p1
void SBGeoPolyLine2DArray::Copy(SBGeoPolyLine2DList const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) items.push_back(p1[i]);
}

// 08177610
void SBGeoPolyLine2DArray::AddLast(SBGeoPoint2D const& p1) {
    items.push_back(p1);
}

// 08177650  appends
void SBGeoPolyLine2DArray::Copy(SBGeoPolyLine2DArray const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) items.push_back(p1.items[i]);
}

// 081776c0
void SBGeoPolyLine2DArray::Destroy() {}

// 081776d0
SBGeoPolyLine2DArray::SBGeoPolyLine2DArray() {}

// 08177750
SBGeoPolyLine2DArray::SBGeoPolyLine2DArray(SBGeoPolyLine2DArray const& p1) : SBArray<SBGeoPoint2D>() {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) items.push_back(p1.items[i]);
}

// 08177880
SBGeoPolyLine2DArray::SBGeoPolyLine2DArray(SBGeoPolyLine2DList const& p1) {
    Copy(p1);
}

// 08177960
SBGeoPolyLine2DArray::SBGeoPolyLine2DArray(SBGeoPoint2D const& p1) {
    items.insert(items.begin(), p1);
}

// 08177a60  (sic) appends p1 to this polyline instead of replacing it
SBGeoPolyLine2DArray& SBGeoPolyLine2DArray::operator=(SBGeoPolyLine2DArray const& p1) {
    if (this != &p1) {
        uint32_t n = p1.GetCount();
        for (uint32_t i = 0; i != n; i++) items.push_back(p1.items[i]);
    }
    return *this;
}

// 08177ae0
int32_t SBGeoPolyLine2DArray::GetType() const {
    return 2;
}

// 08177af0
bool SBGeoPolyLine2DArray::IsIntersecting(SBGeoLine2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// 08177b10
bool SBGeoPolyLine2DArray::IsIntersecting(SBGeoHalfLine2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// 08177b30
bool SBGeoPolyLine2DArray::IsIntersecting(SBGeoSegment2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// 08177b50
bool SBGeoPolyLine2DArray::IsIntersecting(SBGeoPolyLine2DArray const& p1) const {
    int n = (int)GetCount();
    if (n == 0) return false;
    bool r = false;
    if (n != 1) {
        uint32_t k = 1;
        uint32_t i = 0;
        while (true) {
            SBGeoSegment2D s(items[i], items[i + 1]);
            r = s.IsIntersecting(p1);
            if ((uint32_t)(n - 1) <= k || r == true) break;
            i++;
            k++;
        }
    }
    return r;
}

// 08177c00  (no bounds check)
SBGeoSegment2D SBGeoPolyLine2DArray::GetSegment(unsigned int p1) const {
    return SBGeoSegment2D(items.data()[p1], items.data()[p1 + 1]);
}

// 08177c40
bool SBGeoPolyLine2DArray::IsIntersecting(SBGeoPolygon2DList const& p1) const {
    int n = (int)GetCount();
    if (n == 0) return false;
    bool r = false;
    if (n != 1) {
        uint32_t k = 1;
        uint32_t i = 0;
        while (true) {
            SBGeoSegment2D s(items[i], items[i + 1]);
            r = p1.IsIntersecting(s);
            if ((uint32_t)(n - 1) <= k || r == true) break;
            i++;
            k++;
        }
    }
    return r;
}

// 08177cf0
bool SBGeoPolyLine2DArray::IsIntersecting(SBGeoPolygon2DArray const& p1) const {
    int n = (int)GetCount();
    if (n == 0) return false;
    bool r = false;
    if (n != 1) {
        uint32_t k = 1;
        uint32_t i = 0;
        while (true) {
            SBGeoSegment2D s(items[i], items[i + 1]);
            r = p1.IsIntersecting(s);
            if ((uint32_t)(n - 1) <= k || r == true) break;
            i++;
            k++;
        }
    }
    return r;
}

// 08177da0
bool SBGeoPolyLine2DArray::IsIntersecting(SBGeoBoundingBox2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// (sic) the original reverses the indices of a copy of each result only
static void ReverseCopies(SBList<SBGeoIntersection2D>& res) {
    SBGeoIntersection2D tmp;
    uint32_t n = res.GetCount() & 0xffff;
    for (uint32_t i = 0; i < n; i = (i + 1) & 0xffff) {
        tmp = res[i];
        tmp.ReverseIndexes();
    }
}

// 08177dc0
SBList<SBGeoIntersection2D> SBGeoPolyLine2DArray::operator^(SBGeoLine2D const& p1) const {
    SBList<SBGeoIntersection2D> res;
    res = p1 ^ *this;
    ReverseCopies(res);
    return res;
}

// 08177fb0
SBList<SBGeoIntersection2D> SBGeoPolyLine2DArray::operator^(SBGeoHalfLine2D const& p1) const {
    SBList<SBGeoIntersection2D> res;
    res = p1 ^ *this;
    ReverseCopies(res);
    return res;
}

// 081781a0
SBList<SBGeoIntersection2D> SBGeoPolyLine2DArray::operator^(SBGeoSegment2D const& p1) const {
    SBList<SBGeoIntersection2D> res;
    res = p1 ^ *this;
    ReverseCopies(res);
    return res;
}

// 08178390  inserts before point p2
void SBGeoPolyLine2DArray::Add(SBGeoPoint2D const& p1, unsigned int p2) {
    if (GetCount() == p2)
        items.push_back(p1);
    else
        items.insert(items.begin() + p2, p1);
}

// 08178410
void SBGeoPolyLine2DArray::AddFirst(SBGeoPoint2D const& p1) {
    items.insert(items.begin(), p1);
}

// 08178450
void SBGeoPolyLine2DArray::Add(SBList<SBGeoPoint2D> const& p1, unsigned int p2) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) {
        SBGeoPoint2D const& p = p1[i];
        if (GetCount() == i + p2)
            items.push_back(p);
        else
            items.insert(items.begin() + (i + p2), p);
    }
}

// 08178590
void SBGeoPolyLine2DArray::Add(SBArray<SBGeoPoint2D> const& p1, unsigned int p2) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) {
        SBGeoPoint2D p = p1.items[i];
        if (p2 == GetCount())
            items.push_back(p);
        else
            items.insert(items.begin() + p2, p);
        p2++;
    }
}

// 08178670  (sic) each point is put first in turn, so p1 ends up reversed
void SBGeoPolyLine2DArray::AddFirst(SBList<SBGeoPoint2D> const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) items.insert(items.begin(), p1[i]);
}

// 08178750  (sic) reversed as well
void SBGeoPolyLine2DArray::AddFirst(SBArray<SBGeoPoint2D> const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) {
        SBGeoPoint2D p = p1.items[i];
        items.insert(items.begin(), p);
    }
}

// 081787e0
void SBGeoPolyLine2DArray::AddLast(SBList<SBGeoPoint2D> const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) items.push_back(p1[i]);
}

// 081788d0
void SBGeoPolyLine2DArray::AddLast(SBArray<SBGeoPoint2D> const& p1) {
    uint32_t n = p1.GetCount();
    for (uint32_t i = 0; i != n; i++) items.push_back(p1.items[i]);
}

// 08178940
void SBGeoPolyLine2DArray::Delete() {
    items.clear();
}

// 08178980
void SBGeoPolyLine2DArray::DeleteFirst() {
    items.erase(items.begin());
}

// 081789f0
void SBGeoPolyLine2DArray::DeleteLast() {
    items.pop_back();
}

// 08178a20
void SBGeoPolyLine2DArray::Delete(unsigned int p1) {
    items.erase(items.begin() + p1);
}

// 08178a90  deletes the points p1 to p2 (both included)
void SBGeoPolyLine2DArray::Delete(unsigned int p1, unsigned int p2) {
    if (p1 != p2 + 1) items.erase(items.begin() + p1, items.begin() + (p2 + 1));
}

// 08178b30
void SBGeoPolyLine2DArray::Set(SBGeoPoint2D const& p1, unsigned int p2) {
    items.data()[p2] = p1;
}

// every segment of a against every segment of b; see SBGeoPolyLine2DList::operator^
template <class B>
static SBList<SBGeoIntersection2D> PolyLineXPolyLine(SBGeoPolyLine2DArray const& a, B const& b) {
    SBList<SBGeoIntersection2D> res;
    SBGeoPoint2D a0, a1, b0, b1;
    SBGeoSegment2D sa(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoSegment2D sb(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoIntersection2D cur;
    SBGeoIntersection2D prev;
    if (a.GetCount() != 0) a0 = a.items[0];  // (the original reads point 0 even of an empty polyline)
    uint32_t n = a.GetCount() & 0xffff;
    if (n < 2) return res;
    uint32_t m = b.GetCount() & 0xffff;
    uint32_t i = 1;
    do {
        a1 = a.items[i];
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

// 08178b60
SBList<SBGeoIntersection2D> SBGeoPolyLine2DArray::operator^(SBGeoPolyLine2DList const& p1) const {
    return PolyLineXPolyLine(*this, p1);
}

// 08179240
SBList<SBGeoIntersection2D> SBGeoPolyLine2DArray::operator^(SBGeoPolyLine2DArray const& p1) const {
    return PolyLineXPolyLine(*this, p1);
}

// 081798e0  (the count is taken modulo 0x10000)
void SBGeoPolyLine2DArray::operator+=(SBGeoVector2D const& p1) {
    uint32_t n = GetCount() & 0xffff;
    for (uint32_t i = 0; i < n; i = (i + 1) & 0xffff) items[i] += p1;
}
