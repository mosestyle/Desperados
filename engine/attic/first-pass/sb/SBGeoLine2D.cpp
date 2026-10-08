// SBGeoLine2D (see SBGeoLine2D.h)
// The intersections use the slope/intercept form y = a*x + b computed in double precision from
// single precision coordinate differences, exactly as the original.
#include "sb/SBGeoLine2D.h"
#include "sb/SBGeoBoundingBox2D.h"
#include "sb/SBGeoHalfLine2D.h"
#include "sb/SBGeoPolyLine2DArray.h"
#include "sb/SBGeoPolyLine2DList.h"
#include "sb/SBGeoPolygon2DArray.h"
#include "sb/SBGeoPolygon2DList.h"
#include "sb/SBGeoSegment2D.h"
#include "sb/SBGeoVector2D.h"

#ifdef __clang__
#pragma clang fp contract(off)
#endif

// the tolerance of the original: the float 1e-9 used in double comparisons
static const double kEps = (double)1e-09f;

// 08160610
void SBGeoLine2D::Copy(SBGeoLine2D const& p1) {
    f_0 = p1.f_0;
    f_8 = p1.f_8;
}

// 08160650
void SBGeoLine2D::Destroy() {}

// 08160660
SBGeoLine2D::SBGeoLine2D(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2) : f_0(p1), f_8(p2) {}

// 081606b0  the line through p1 with direction p2
SBGeoLine2D::SBGeoLine2D(SBGeoPoint2D const& p1, SBGeoVector2D const& p2) {
    f_0 = p1;
    SBGeoPoint2D b = p1 + p2;
    f_8 = b;
}

// 08160740
SBGeoLine2D::SBGeoLine2D(SBGeoLine2D const& p1) {
    f_0 = p1.f_0;
    f_8 = p1.f_8;
}

// 081607b0
SBGeoLine2D::SBGeoLine2D(SBGeoHalfLine2D const& p1) {
    f_0 = p1.f_0;
    f_8 = p1.f_8;
}

// 08160820
SBGeoLine2D::SBGeoLine2D(SBGeoSegment2D const& p1) {
    f_0 = p1.f_0;
    f_8 = p1.f_8;
}

// 08160890
SBGeoLine2D& SBGeoLine2D::operator=(SBGeoLine2D const& p1) {
    if (&p1 != this) {
        f_0 = p1.f_0;
        f_8 = p1.f_8;
    }
    return *this;
}

// 081608d0
void SBGeoLine2D::SetVector(SBGeoVector2D const& p1) {
    SBGeoPoint2D b = f_0 + p1;
    f_8 = b;
}

// 08160930  not parallel, or the same line
bool SBGeoLine2D::IsIntersecting(SBGeoLine2D const& p1) const {
    SBGeoVector2D v1;
    SBGeoVector2D v2;
    v1 = f_8 - f_0;
    v2 = p1.f_8 - p1.f_0;
    bool r = true;
    if (v1.Det(v2) == 0.0f) {
        v2 = p1.f_0 - f_0;
        r = v1.Det(v2) == 0.0f;
    }
    return r;
}

// 08160a90
bool SBGeoLine2D::IsIntersecting(SBGeoHalfLine2D const& p1) const {
    SBGeoVector2D v1;
    SBGeoVector2D v2;
    SBGeoVector2D v3;
    v1 = f_8 - f_0;
    v2 = p1.f_8 - p1.f_0;
    v3 = p1.f_0 - f_0;
    float d = v1.Det(v3);
    bool r = true;
    if (d != 0.0f) {
        if (d > 0.0f)
            r = 0.0f > v1.Det(v2);
        else
            r = v1.Det(v2) > 0.0f;
    }
    return r;
}

// 08160c30
bool SBGeoLine2D::IsIntersecting(SBGeoSegment2D const& p1) const {
    SBGeoVector2D v1;
    SBGeoVector2D v0;  // unused
    SBGeoVector2D v2;
    SBGeoVector2D v3;
    SBGeoBoundingBox2D box;
    box.Expand(p1);
    if (box.IsIntersecting(*this)) {
        v1 = f_8 - f_0;
        v2 = p1.f_0 - f_0;
        v3 = p1.f_8 - f_0;
        if (v1.Det(v2) >= 0.0f) {
            if (0.0f >= v1.Det(v3)) return true;
        }
        if (!(0.0f < v1.Det(v2))) {
            if (v1.Det(v3) >= 0.0f) return true;
        }
    }
    return false;
}

// 08160ea0  (a polyline of one point gives false; an empty one walks 0xffffffff segments, sic)
bool SBGeoLine2D::IsIntersecting(SBGeoPolyLine2DArray const& p1) const {
    int n = p1.Size();
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

// 08160f50
SBGeoLine2D::~SBGeoLine2D() {}

// 08160f70
bool SBGeoLine2D::IsIntersecting(SBGeoPolyLine2DList const& p1) const {
    int n = p1.Size();
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

// 08161020  (only the n-1 open segments are tested, the closing one is not)
bool SBGeoLine2D::IsIntersecting(SBGeoPolygon2DList const& p1) const {
    int n = p1.Size();
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

// 081610d0
bool SBGeoLine2D::IsIntersecting(SBGeoPolygon2DArray const& p1) const {
    int n = p1.Size();
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

// 08161180
SBGeoIntersection2D SBGeoLine2D::operator^(SBGeoLine2D const& p1) const {
    SBGeoPoint2D pt;
    SBGeoIntersection2D r;
    float x1 = f_0.f_0;
    float dx1 = f_8.f_0 - x1;
    float x2 = p1.f_0.f_0;
    float dx2 = p1.f_8.f_0 - x2;
    if (dx1 != 0.0f) {
        double a1 = (double)(f_8.f_4 - f_0.f_4) / (double)dx1;
        double b1 = (double)f_0.f_4 - (double)x1 * a1;
        if (dx2 != 0.0f) {
            double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
            double b2 = (double)p1.f_0.f_4 - (double)x2 * a2;
            if (a1 != a2) {
                double x = (b2 - b1) / (a1 - a2);
                pt.f_0 = (float)x;
                pt.f_4 = (float)(x * a1 + b1);
                r.SetPoint(pt);
            } else if (b1 != b2) {
                r.SetNone();
            } else {
                r.SetLine(*this);
            }
        } else {
            pt.f_0 = x2;
            pt.f_4 = (float)((double)x2 * a1 + b1);
            r.SetPoint(pt);
        }
    } else if (dx2 != 0.0f) {
        double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
        double b2 = (double)p1.f_0.f_4 - (double)x2 * a2;
        pt.f_0 = x1;
        pt.f_4 = (float)((double)x1 * a2 + b2);
        r.SetPoint(pt);
    } else if (x1 != x2) {
        r.SetNone();
    } else {
        r.SetLine(*this);
    }
    return r;
}

// 08161380
SBGeoIntersection2D SBGeoLine2D::operator^(SBGeoHalfLine2D const& p1) const {
    SBGeoPoint2D pt;
    SBGeoIntersection2D r;
    float x1 = f_0.f_0;
    float dx1 = f_8.f_0 - x1;
    float hx = p1.f_0.f_0;
    float hx2 = p1.f_8.f_0;
    float dx2 = hx2 - hx;
    if (dx1 != 0.0f) {
        double a1 = (double)(f_8.f_4 - f_0.f_4) / (double)dx1;
        double b1 = (double)f_0.f_4 - (double)x1 * a1;
        if (dx2 != 0.0f) {
            double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
            double b2 = (double)p1.f_0.f_4 - (double)hx * a2;
            if (a1 != a2) {
                double x = (b2 - b1) / (a1 - a2);
                bool in;
                if (hx2 <= hx)
                    in = x <= (double)hx + kEps;
                else
                    in = (double)hx - kEps <= x;
                if (in) {
                    pt.f_0 = (float)x;
                    pt.f_4 = (float)(b1 + a1 * x);
                    r.SetPoint(pt);
                } else {
                    r.SetNone();
                }
            } else if (b1 != b2) {
                r.SetNone();
            } else {
                r.SetLine(*this);
            }
        } else {
            // vertical half line
            double y = (double)hx * a1 + b1;
            float hy = p1.f_0.f_4;
            bool in;
            if (p1.f_8.f_4 <= hy)
                in = y <= (double)hy + kEps;
            else
                in = (double)hy - kEps <= y;
            if (in) {
                pt.f_0 = hx;
                pt.f_4 = (float)y;
                r.SetPoint(pt);
            } else {
                r.SetNone();
            }
        }
    } else if (dx2 != 0.0f) {
        // vertical line
        double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
        double b2 = (double)p1.f_0.f_4 - (double)hx * a2;
        double x = (double)x1;
        bool in;
        if (hx2 <= hx)
            in = x <= (double)hx + kEps;
        else
            in = (double)hx - kEps <= x;
        if (in) {
            pt.f_0 = x1;
            pt.f_4 = (float)(x * a2 + b2);
            r.SetPoint(pt);
        } else {
            r.SetNone();
        }
    } else if (x1 != hx) {
        r.SetNone();
    } else {
        r.SetHalfLine(p1);
    }
    return r;
}

// 08161700
SBGeoIntersection2D SBGeoLine2D::operator^(SBGeoSegment2D const& p1) const {
    SBGeoPoint2D pt;
    SBGeoIntersection2D r;
    float x1 = f_0.f_0;
    float dx1 = f_8.f_0 - x1;
    float sx1 = p1.f_0.f_0;
    float sx2 = p1.f_8.f_0;
    float dx2 = sx2 - sx1;
    if (dx1 != 0.0f) {
        double a1 = (double)(f_8.f_4 - f_0.f_4) / (double)dx1;
        double b1 = (double)f_0.f_4 - (double)x1 * a1;
        if (dx2 != 0.0f) {
            double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
            double b2 = (double)p1.f_0.f_4 - (double)sx1 * a2;
            if (a1 != a2) {
                double x = (b2 - b1) / (a1 - a2);
                if ((x <= (double)sx1 + kEps && (double)sx2 - kEps <= x) ||
                    ((double)sx1 - kEps <= x && x <= (double)sx2 + kEps)) {
                    pt.f_0 = (float)x;
                    pt.f_4 = (float)(b1 + a1 * x);
                    r.SetPoint(pt);
                } else {
                    r.SetNone();
                }
            } else if (b1 != b2) {
                r.SetNone();
            } else {
                r.SetSegment(p1);
            }
        } else {
            // vertical segment
            double y = (double)sx1 * a1 + b1;
            double sy1 = (double)p1.f_0.f_4;
            if ((y <= sy1 + kEps && (double)p1.f_8.f_4 - kEps <= y) ||
                (sy1 - kEps <= y && y <= (double)p1.f_8.f_4 + kEps)) {
                pt.f_0 = sx1;
                pt.f_4 = (float)y;
                r.SetPoint(pt);
            } else {
                r.SetNone();
            }
        }
    } else if (dx2 != 0.0f) {
        // vertical line
        double x = (double)x1;
        double s1 = (double)sx1;
        if ((x <= s1 + kEps && (double)sx2 - kEps <= x) || (s1 - kEps <= x && x <= (double)sx2 + kEps)) {
            double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
            double b2 = (double)p1.f_0.f_4 - s1 * a2;
            pt.f_0 = x1;
            pt.f_4 = (float)(a2 * x + b2);
            r.SetPoint(pt);
        } else {
            r.SetNone();
        }
    } else if (x1 != sx1) {
        r.SetNone();
    } else {
        r.SetSegment(p1);
    }
    return r;
}

// 08161a60  intersections with each segment; a point equal to (an end of) the previous result is
// skipped. The second index is the segment index.
SBList<SBGeoIntersection2D> SBGeoLine2D::operator^(SBGeoPolyLine2DArray const& p1) const {
    SBList<SBGeoIntersection2D> res;
    SBGeoPoint2D cur;
    SBGeoPoint2D prev;
    SBGeoSegment2D seg(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoIntersection2D inter;
    uint32_t n = p1.Size();
    if (n != 0) prev = p1.Get(0);  // (the original reads element 0 even of an empty polyline)
    if (1 < (n & 0xffff)) {
        uint32_t i = 1;
        uint16_t k = 2;
        do {
            cur = p1.Get(i);
            seg.f_0 = prev;
            seg.f_8 = cur;
            if (IsIntersecting(seg)) {
                inter = *this ^ seg;
                inter.SetSecondIndex((uint16_t)(k - 2));
                if (res.GetCount() == 0 || !inter.BelongsTo(res.Last())) res.AddLast(inter);
            }
            prev = cur;
            i = k;
            k++;
        } while (i < (n & 0xffff));
    }
    return res;
}

// 08161da0
SBList<SBGeoIntersection2D> SBGeoLine2D::operator^(SBGeoPolyLine2DList const& p1) const {
    SBList<SBGeoIntersection2D> res;
    SBGeoPoint2D cur;
    SBGeoPoint2D prev;
    SBGeoSegment2D seg(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoIntersection2D inter;
    uint32_t n = p1.Size();
    if (n != 0) prev = p1.Get(0);
    if (1 < (n & 0xffff)) {
        uint32_t i = 1;
        uint16_t k = 2;
        do {
            cur = p1.Get(i);
            seg.f_0 = prev;
            seg.f_8 = cur;
            if (IsIntersecting(seg)) {
                inter = *this ^ seg;
                inter.SetSecondIndex((uint16_t)(k - 2));
                if (res.GetCount() == 0 || !inter.BelongsTo(res.Last())) res.AddLast(inter);
            }
            prev = cur;
            i = k;
            k++;
        } while (i < (n & 0xffff));
    }
    return res;
}

// 081620e0  closed polygon: the segments are (n-1, 0), (0, 1)...; a result equal to the previous one
// is skipped; no indices are set
SBList<SBGeoIntersection2D> SBGeoLine2D::operator^(SBGeoPolygon2DList const& p1) const {
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
            if (IsIntersecting(seg)) {
                inter = *this ^ seg;
                if (res.GetCount() == 0 || inter != SBGeoIntersection2D(res.Last())) res.AddLast(inter);
            }
            prev = cur;
            i = k & 0xffff;
            k++;
        } while (i < n);
    }
    return res;
}

// 08162450
SBList<SBGeoIntersection2D> SBGeoLine2D::operator^(SBGeoPolygon2DArray const& p1) const {
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
            if (IsIntersecting(seg)) {
                inter = *this ^ seg;
                if (res.GetCount() == 0 || inter != SBGeoIntersection2D(res.Last())) res.AddLast(inter);
            }
            prev = cur;
            i = k & 0xffff;
            k++;
        } while (i < n);
    }
    return res;
}

// 081627c0  x of the point of the line at height p1 (the first x when the line is vertical)
float SBGeoLine2D::EvalX(float p1) const {
    float x = f_0.f_0;
    float dx = f_8.f_0 - x;
    if (dx != 0.0f) {
        float y1 = f_0.f_4;
        x = (float)((double)x + ((double)(p1 - y1) * (double)dx) / (double)(f_8.f_4 - y1));
    }
    return x;
}

// 08162820
float SBGeoLine2D::EvalY(float p1) const {
    float y = f_0.f_4;
    float dy = f_8.f_4 - y;
    if (dy != 0.0f) {
        float x1 = f_0.f_0;
        y = (float)((double)y + ((double)(p1 - x1) * (double)dy) / (double)(f_8.f_0 - x1));
    }
    return y;
}

// 08162880  parallel and the same y at x = 0
bool SBGeoLine2D::operator==(SBGeoLine2D const& p1) const {
    SBGeoVector2D v;
    v = f_8 - f_0;
    bool r = false;
    if (v.Det(p1.f_8 - p1.f_0) == 0.0f) r = EvalY(0.0f) == p1.EvalY(0.0f);
    return r;
}

// 08162a00
bool SBGeoLine2D::operator!=(SBGeoLine2D const& p1) const {
    return !(*this == p1);
}
