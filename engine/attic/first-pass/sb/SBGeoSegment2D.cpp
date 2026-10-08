// SBGeoSegment2D (see SBGeoSegment2D.h)
#include "sb/SBGeoSegment2D.h"
#include "sb/SBGeoBoundingBox2D.h"
#include "sb/SBGeoPolyLine2DArray.h"
#include "sb/SBGeoPolyLine2DList.h"
#include "sb/SBGeoPolygon2DArray.h"
#include "sb/SBGeoPolygon2DList.h"
#include "sb/SBGeoVector2D.h"

#ifdef __clang__
#pragma clang fp contract(off)
#endif

static const double kEps = (double)1e-09f;

// 0817a220
SBGeoSegment2D::SBGeoSegment2D(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2) : SBGeoHalfLine2D(p1, p2) {}

// 0817a230
SBGeoSegment2D::SBGeoSegment2D(SBGeoLine2D const& p1) : SBGeoHalfLine2D(p1.f_0, p1.f_8) {}

// 0817a260
SBGeoSegment2D::SBGeoSegment2D(SBGeoHalfLine2D const& p1) : SBGeoHalfLine2D(p1.f_0, p1.f_8) {}

// 0817a290
SBGeoSegment2D::SBGeoSegment2D(SBGeoSegment2D const& p1) : SBGeoHalfLine2D(p1.f_0, p1.f_8) {}

// 0817a2c0
SBGeoSegment2D& SBGeoSegment2D::operator=(SBGeoSegment2D const& p1) {
    SBGeoHalfLine2D::operator=(p1);
    return *this;
}

// 0817a2e0  same ends in either order
bool SBGeoSegment2D::operator==(SBGeoSegment2D const& p1) const {
    if (f_0 == p1.f_0 && f_8 == p1.f_8) return true;
    if (!(f_0 == p1.f_8)) return false;
    return f_8 == p1.f_0;
}

// 0817a350
bool SBGeoSegment2D::IsIntersecting(SBGeoPoint2D const& p1) const {
    bool inside;
    {
        SBGeoBoundingBox2D box(*this);
        inside = box.IsInside_p(p1);
    }
    if (!inside) return false;
    return (f_8 - f_0).Det(p1 - f_0) == 0.0f;
}

// 0817a450
bool SBGeoSegment2D::IsIntersecting(SBGeoLine2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// 0817a470
bool SBGeoSegment2D::IsIntersecting(SBGeoHalfLine2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// 0817a490
bool SBGeoSegment2D::IsIntersecting(SBGeoSegment2D const& p1) const {
    SBGeoBoundingBox2D box1(*this);
    SBGeoBoundingBox2D box2(p1);
    SBGeoVector2D u;
    SBGeoVector2D w;
    SBGeoVector2D v;
    if (!box1.IsIntersecting(box2)) return false;
    u = f_0 - f_8;
    w = p1.f_8 - p1.f_0;
    v = p1.f_0 - f_0;
    float d1 = u.Det(v);
    float d2 = u.Det(p1.f_8 - f_0);
    if (!(d1 >= 0.0f && 0.0f >= d2)) {
        if (!(0.0f >= d1)) return false;
        if (!(d2 >= 0.0f)) return false;
    }
    float e1 = w.Det(v);
    float e2 = w.Det(f_8 - p1.f_0);
    if (0.0f >= e1 && 0.0f >= e2) return true;
    return e1 >= 0.0f && e2 >= 0.0f;
}

// 0817a7e0
bool SBGeoSegment2D::IsIntersecting(SBGeoPolyLine2DList const& p1) const {
    int n = p1.Size();
    if (n == 0) return false;
    uint32_t i = 0;
    bool r = false;
    if (n != 1) {
        do {
            SBGeoSegment2D s = p1.GetSegment(i);
            r = IsIntersecting(s);
            i++;
            if ((uint32_t)(n - 1) <= i) return r;
        } while (r != true);
    }
    return r;
}

// 0817a870
bool SBGeoSegment2D::IsIntersecting(SBGeoPolyLine2DArray const& p1) const {
    int n = p1.Size();
    if (n == 0) return false;
    uint32_t i = 0;
    bool r = false;
    if (n != 1) {
        do {
            SBGeoSegment2D s = p1.GetSegment(i);
            r = IsIntersecting(s);
            i++;
            if ((uint32_t)(n - 1) <= i) return r;
        } while (r != true);
    }
    return r;
}

// 0817a900  all the n segments, the closing one included
bool SBGeoSegment2D::IsIntersecting(SBGeoPolygon2DList const& p1) const {
    uint32_t n = p1.Size();
    uint32_t i = 0;
    bool r = false;
    if (1 < n) {
        do {
            SBGeoSegment2D s = p1.GetSegment(i);
            r = IsIntersecting(s);
            i++;
        } while (i <= n - 1 && !r);
    }
    return r;
}

// 0817a990
bool SBGeoSegment2D::IsIntersecting(SBGeoPolygon2DArray const& p1) const {
    uint32_t n = p1.Size();
    uint32_t i = 0;
    bool r = false;
    if (1 < n) {
        do {
            SBGeoSegment2D s = p1.GetSegment(i);
            r = IsIntersecting(s);
            i++;
        } while (i <= n - 1 && !r);
    }
    return r;
}

// 0817aa20
bool SBGeoSegment2D::IsIntersecting(SBGeoBoundingBox2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// 0817aa40
SBGeoIntersection2D SBGeoSegment2D::operator^(SBGeoLine2D const& p1) const {
    return p1 ^ *this;
}

// 0817aa70
SBGeoIntersection2D SBGeoSegment2D::operator^(SBGeoHalfLine2D const& p1) const {
    return p1 ^ *this;
}

// between a and b (in either order), with tolerance
static inline bool InRange(double v, float a, float b) {
    return (v <= (double)a + kEps && (double)b - kEps <= v) || ((double)a - kEps <= v && v <= (double)b + kEps);
}

// overlap of two collinear segments along one coordinate (c selects x or y)
static void Overlap(SBGeoSegment2D const& s1, SBGeoSegment2D const& s2, int c, SBGeoIntersection2D& r) {
    SBGeoPoint2D lo1, hi1, lo2, hi2;
    SBGeoSegment2D seg(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
#define CO(p) (c == 0 ? (p).f_0 : (p).f_4)
    // lo/hi along the coordinate
    if (CO(s1.f_8) <= CO(s1.f_0)) {
        hi1 = s1.f_0;
        lo1 = s1.f_8;
    } else {
        lo1 = s1.f_0;
        hi1 = s1.f_8;
    }
    if (CO(s2.f_8) <= CO(s2.f_0)) {
        hi2 = s2.f_0;
        lo2 = s2.f_8;
    } else {
        lo2 = s2.f_0;
        hi2 = s2.f_8;
    }
    float l1 = CO(lo1), h1 = CO(hi1), l2 = CO(lo2), h2 = CO(hi2);
#undef CO
    if (h2 < l1 || h1 < l2) {
        r.SetNone();
    } else if (h2 < l1 || l1 <= l2) {
        if (l2 <= l1 || h1 < l2) {
            if (l1 != h2 || l1 <= l2) {
                if (l1 < l2) {
                    if (h1 == l2) r.SetPoint(lo2);
                }
            } else {
                r.SetPoint(lo1);
            }
        } else if (h2 <= h1) {
            r.SetSegment(s2);
        } else {
            seg.f_0 = lo2;
            seg.f_8 = hi1;
            r.SetSegment(seg);
        }
    } else if (h1 <= h2) {
        r.SetSegment(s1);
    } else {
        seg.f_0 = lo1;
        seg.f_8 = hi2;
        r.SetSegment(seg);
    }
}

// 0817aaa0
SBGeoIntersection2D SBGeoSegment2D::operator^(SBGeoSegment2D const& p1) const {
    SBGeoPoint2D pt;
    SBGeoIntersection2D r;
    float x1 = f_0.f_0;
    float x2 = f_8.f_0;
    float dx1 = x2 - x1;
    float x3 = p1.f_0.f_0;
    float x4 = p1.f_8.f_0;
    float dx2 = x4 - x3;
    if (dx1 != 0.0f) {
        float y1 = f_0.f_4;
        float y2 = f_8.f_4;
        double a1 = (double)(y2 - y1) / (double)dx1;
        double b1 = (double)y1 - (double)x1 * a1;
        if (dx2 != 0.0f) {
            double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
            double b2 = (double)p1.f_0.f_4 - (double)x3 * a2;
            if (a1 != a2) {
                double x = (b2 - b1) / (a1 - a2);
                if (InRange(x, x1, x2)) {
                    if (InRange(x, x3, x4)) {
                        pt.f_0 = (float)x;
                        pt.f_4 = (float)(b1 + a1 * x);
                        r.SetPoint(pt);
                    } else {
                        r.SetNone();
                    }
                } else {
                    r.SetNone();
                }
            } else if (b1 != b2) {
                r.SetNone();
            } else {
                Overlap(*this, p1, 0, r);
            }
        } else {
            // p1 vertical
            double y = (double)x3 * a1 + b1;
            if (InRange(y, y1, y2)) {
                if (InRange(y, p1.f_0.f_4, p1.f_8.f_4)) {
                    pt.f_0 = x3;
                    pt.f_4 = (float)y;
                    r.SetPoint(pt);
                } else {
                    r.SetNone();
                }
            } else {
                r.SetNone();
            }
        }
    } else if (dx2 != 0.0f) {
        // this one vertical
        double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
        double y = a2 * (double)x1 + ((double)p1.f_0.f_4 - (double)x3 * a2);
        if (InRange(y, f_0.f_4, f_8.f_4)) {
            if (InRange((double)x1, x3, x4)) {
                pt.f_0 = x1;
                pt.f_4 = (float)y;
                r.SetPoint(pt);
            } else {
                r.SetNone();
            }
        } else {
            r.SetNone();
        }
    } else if (x1 == x3) {
        Overlap(*this, p1, 1, r);
    }
    return r;
}

// intersections with the segments of a polyline; a result equal to the previous one is skipped
template <class P>
static SBList<SBGeoIntersection2D> SegmentXPolyLine(SBGeoSegment2D const& s, P const& p1) {
    SBList<SBGeoIntersection2D> res;
    SBGeoPoint2D cur;
    SBGeoPoint2D prev;
    SBGeoSegment2D seg(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoIntersection2D inter;
    uint32_t n = p1.Size();
    if (n != 0) prev = p1.Get(0);  // (the original reads element 0 even of an empty polyline)
    if (1 < (n & 0xffff)) {
        uint32_t i = 1;
        uint32_t k = 2;
        do {
            cur = p1.Get(i);
            seg.f_0 = prev;
            seg.f_8 = cur;
            if (s.IsIntersecting(seg)) {
                inter = s ^ seg;
                if (res.GetCount() == 0 || inter != SBGeoIntersection2D(res.Last())) res.AddLast(inter);
            }
            prev = cur;
            i = k & 0xffff;
            k++;
        } while (i < (n & 0xffff));
    }
    return res;
}

// 0817b540
SBList<SBGeoIntersection2D> SBGeoSegment2D::operator^(SBGeoPolyLine2DArray const& p1) const {
    return SegmentXPolyLine(*this, p1);
}

// 0817b880
SBList<SBGeoIntersection2D> SBGeoSegment2D::operator^(SBGeoPolyLine2DList const& p1) const {
    return SegmentXPolyLine(*this, p1);
}

// intersections with the segments of a closed polygon: (n-1, 0), (0, 1)...
template <class P>
static SBList<SBGeoIntersection2D> SegmentXPolygon(SBGeoSegment2D const& s, P const& p1) {
    SBList<SBGeoIntersection2D> res;
    SBGeoPoint2D cur;
    SBGeoPoint2D prev;
    SBGeoSegment2D seg(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoIntersection2D inter;
    uint32_t n = p1.Size() & 0xffff;
    if (n != 0) prev = p1.Get(n - 1);
    if (n != 0) {
        uint32_t i = 0;
        uint32_t k = 1;
        do {
            cur = p1.Get(i);
            seg.f_0 = prev;
            seg.f_8 = cur;
            if (s.IsIntersecting(seg)) {
                inter = s ^ seg;
                if (res.GetCount() == 0 || inter != SBGeoIntersection2D(res.Last())) res.AddLast(inter);
            }
            prev = cur;
            i = k & 0xffff;
            k++;
        } while (i < n);
    }
    return res;
}

// 0817bbc0
SBList<SBGeoIntersection2D> SBGeoSegment2D::operator^(SBGeoPolygon2DList const& p1) const {
    return SegmentXPolygon(*this, p1);
}

// 0817bf00
SBList<SBGeoIntersection2D> SBGeoSegment2D::operator^(SBGeoPolygon2DArray const& p1) const {
    return SegmentXPolygon(*this, p1);
}

// 0817c240  is the projection of p1 on the supporting line between the ends?
bool SBGeoSegment2D::IsBetween(SBGeoPoint2D const& p1) const {
    SBGeoVector2D v;
    SBGeoVector2D d;
    d = f_8 - f_0;
    v = p1 - f_0;
    if (v * d < 0.0f) return false;
    v = p1 - f_8;
    return v * d <= 0.0f;
}
