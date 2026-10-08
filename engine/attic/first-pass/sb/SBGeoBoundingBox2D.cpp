// SBGeoBoundingBox2D (see SBGeoBoundingBox2D.h)
#include "sb/SBGeoBoundingBox2D.h"
#include "sb/SBFile.h"
#include "sb/SBGeoHalfLine2D.h"
#include "sb/SBGeoLine2D.h"
#include "sb/SBGeoPolyLine2DArray.h"
#include "sb/SBGeoPolyLine2DList.h"
#include "sb/SBGeoPolygon2DArray.h"
#include "sb/SBGeoPolygon2DList.h"
#include "sb/SBGeoSegment2D.h"
#include "sb/SBGeoVector2D.h"
#include <cfloat>
#include <cstdio>

#ifdef __clang__
#pragma clang fp contract(off)
#endif

static const char THIS_FILE[] = "../code/Game/SBLibNG/SBGeoBoundingBox2D.cpp";

// the point test inlined everywhere in the original (same as IsInside_p)
static inline bool PointIn(SBGeoBoundingBox2D const& b, SBGeoPoint2D const& p) {
    if (b.f_10 == 0) return false;
    if (b.f_0.f_0 <= p.f_0) {
        if (p.f_0 <= b.f_8.f_0) {
            if (b.f_0.f_4 <= p.f_4) return p.f_4 <= b.f_8.f_4;
        }
    }
    return false;
}

// 081576e0
void SBGeoBoundingBox2D::Copy(SBGeoBoundingBox2D const& p1) {
    f_10 = p1.f_10;
    if (f_10 != 0) {
        f_0 = p1.f_0;
        f_8 = p1.f_8;
    }
}

// 08157720
void SBGeoBoundingBox2D::Destroy() {}

// 08157730
SBGeoBoundingBox2D::SBGeoBoundingBox2D() {
    f_10 = 0;
    f_0 = SBGeoPoint2D(0.0f, 0.0f);
    f_8 = SBGeoPoint2D(0.0f, 0.0f);
}

// 081577f0  the box between p1 and p1 + (p2, p3). (sic) The original does not set f_10 (it is left
// uninitialized); we clear it. Its only caller (SBFontRenderer::Render) only reads the corners.
SBGeoBoundingBox2D::SBGeoBoundingBox2D(SBGeoPoint2D const& p1, float p2, float p3) {
    f_10 = 0;
    float x = p1.f_0;
    float y = p1.f_4;
    float x2 = p2 + x;
    float y2 = p3 + y;
    f_0 = SBGeoPoint2D(x < x2 ? x : x2, y < y2 ? y : y2);
    f_8 = SBGeoPoint2D(x > x2 ? x : x2, y > y2 ? y : y2);
}

// 08157910  a box reduced to one point
SBGeoBoundingBox2D::SBGeoBoundingBox2D(float p1, float p2) : f_0(p1, p2), f_8(p1, p2) {
    f_10 = 1;
}

// 08157980
SBGeoBoundingBox2D::SBGeoBoundingBox2D(SBGeoBoundingBox2D const& p1) {
    f_10 = p1.f_10;
    if (f_10 != 0) {
        f_0 = p1.f_0;
        f_8 = p1.f_8;
    }
}

// 081579f0
SBGeoBoundingBox2D::SBGeoBoundingBox2D(SBGeoPoint2D const& p1) {
    f_0 = p1;
    f_8 = p1;
    f_10 = 1;
}

// 08157a60
SBGeoBoundingBox2D::SBGeoBoundingBox2D(SBGeoSegment2D const& p1) {
    float x = p1.f_0.f_0;
    float y = p1.f_0.f_4;
    f_0.f_0 = x;
    f_8.f_0 = x;
    f_0.f_4 = y;
    f_8.f_4 = y;
    f_10 = 1;
    float x2 = p1.f_8.f_0;
    float y2 = p1.f_8.f_4;
    if (x2 < x) f_0.f_0 = x2;
    if (x < x2) f_8.f_0 = x2;
    if (y2 < y) f_0.f_4 = y2;
    if (y < y2) f_8.f_4 = y2;
    f_10 = 1;
}

// 08157af0
void SBGeoBoundingBox2D::Expand(SBGeoSegment2D const& p1) {
    Expand(p1.f_0);
    Expand(p1.f_8);
}

// 08157ba0
SBGeoBoundingBox2D::SBGeoBoundingBox2D(SBGeoPolyLine2DArray const& p1) {
    f_10 = 0;
    uint32_t n = p1.Size();
    if (n != 0) {
        uint32_t i = 0;
        do {
            Expand(p1.Get(i));
            i++;
        } while (i < n);
    }
}

// 08157c80
void SBGeoBoundingBox2D::Expand(SBGeoPoint2D const& p1) {
    float x = p1.f_0;
    float y = p1.f_4;
    if (f_10 == 0) {
        f_0.f_0 = x;
        f_8.f_0 = x;
        f_0.f_4 = y;
        f_8.f_4 = y;
    } else {
        if (x < f_0.f_0) f_0.f_0 = x;
        if (x > f_8.f_0) f_8.f_0 = x;
        if (y < f_0.f_4) f_0.f_4 = y;
        if (y > f_8.f_4) f_8.f_4 = y;
    }
    f_10 = 1;
}

// 08157ce0
SBGeoBoundingBox2D::SBGeoBoundingBox2D(SBGeoPolyLine2DList const& p1) {
    f_10 = 0;
    uint32_t n = p1.Size();
    if (n != 0) {
        uint32_t i = 0;
        do {
            Expand(p1.Get(i));
            i++;
        } while (i < n);
    }
}

// 08157dc0  (the corners are taken as given)
SBGeoBoundingBox2D::SBGeoBoundingBox2D(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2) {
    f_0 = p1;
    f_8 = p2;
    f_10 = 1;
}

// 08157e30  (left, top, right, bottom)
SBGeoBoundingBox2D::SBGeoBoundingBox2D(float p1, float p2, float p3, float p4) : f_0(p1, p2), f_8(p3, p4) {
    f_10 = 1;
}

// 08157ea0  the corners only (not the flag)
int32_t SBGeoBoundingBox2D::Serialize(SBFile& p1) {
    int line;
    if (p1.Serialize(&f_0.f_0, 4) < 0) {
        line = 0x115;
    } else if (p1.Serialize(&f_0.f_4, 4) < 0) {
        line = 0x116;
    } else if (p1.Serialize(&f_8.f_0, 4) < 0) {
        line = 0x117;
    } else {
        if (p1.Serialize(&f_8.f_4, 4) >= 0) return 1;
        line = 0x118;
    }
    char msg[512];
    snprintf(msg, sizeof(msg), "Serialize failed reason is %s", p1.TranslateLastError().c_str());
    SBError(false, THIS_FILE, line, msg);
    return 0;
}

// 08157fd0
void SBGeoBoundingBox2D::Reset() {
    f_10 = 0;
}

// 08157fe0  (the corners of an empty source are not copied)
SBGeoBoundingBox2D& SBGeoBoundingBox2D::operator=(SBGeoBoundingBox2D const& p1) {
    if (this != &p1) {
        f_10 = p1.f_10;
        if (f_10 != 0) {
            f_0 = p1.f_0;
            f_8 = p1.f_8;
        }
    }
    return *this;
}

// 08158030
void SBGeoBoundingBox2D::Expand(SBGeoPolyLine2DArray const& p1) {
    uint32_t n = p1.Size();
    if (n != 0) {
        uint32_t i = 0;
        do {
            Expand(p1.Get(i));
            i++;
        } while (n != i);
    }
}

// 081580d0
void SBGeoBoundingBox2D::Expand(SBGeoPolyLine2DList const& p1) {
    uint32_t n = p1.Size();
    if (n != 0) {
        uint32_t i = 0;
        do {
            Expand(p1.Get(i));
            i++;
        } while (n != i);
    }
}

// 08158170  (the flag of p1 is not checked)
void SBGeoBoundingBox2D::Expand(SBGeoBoundingBox2D const& p1) {
    Expand(SBGeoPoint2D(p1.f_0.f_0, p1.f_0.f_4));
    Expand(SBGeoPoint2D(p1.f_8.f_0, p1.f_8.f_4));
}

// 08158290  true when the segment is surely outside (both ends beyond the same side)
bool SBGeoBoundingBox2D::IsTriviallyRejected(SBGeoSegment2D const& p1) const {
    SBGeoPoint2D a;
    SBGeoPoint2D b;
    bool r = true;
    if (f_10 != 0) {
        a = p1.f_0;
        b = p1.f_8;
        if ((f_0.f_0 <= a.f_0 || f_0.f_0 <= b.f_0) && (a.f_0 <= f_8.f_0 || b.f_0 <= f_8.f_0)) {
            if ((f_0.f_4 <= a.f_4 || f_0.f_4 <= b.f_4) && (a.f_4 <= f_8.f_4 || b.f_4 <= f_8.f_4)) r = false;
        }
    }
    return r;
}

// 08158380
bool SBGeoBoundingBox2D::IsTriviallyRejected(SBGeoHalfLine2D const& p1) const {
    SBGeoPoint2D a;
    SBGeoPoint2D b;
    bool r = true;
    if (f_10 != 0) {
        a = p1.f_0;
        b = p1.f_8;
        if ((f_0.f_0 <= a.f_0 || a.f_0 < b.f_0) && (a.f_0 <= f_8.f_0 || b.f_0 < a.f_0) &&
            (f_0.f_4 <= a.f_4 || a.f_4 < b.f_4) && (a.f_4 <= f_8.f_4 || b.f_4 < a.f_4))
            r = false;
    }
    return r;
}

// 08158460  only a vertical/horizontal line can be rejected
bool SBGeoBoundingBox2D::IsTriviallyRejected(SBGeoLine2D const& p1) const {
    SBGeoPoint2D a;
    SBGeoPoint2D b;
    bool r = true;
    if (f_10 != 0) {
        a = p1.f_0;
        b = p1.f_8;
        if (a.f_0 != b.f_0 || (f_0.f_0 <= a.f_0 && a.f_0 <= f_8.f_0)) {
            if (a.f_4 != b.f_4 || (f_0.f_4 <= a.f_4 && a.f_4 <= f_8.f_4)) r = false;
        }
    }
    return r;
}

// 08158530
bool SBGeoBoundingBox2D::IsTriviallyRejected(SBGeoPolyLine2DList const& p1) const {
    bool r = true;
    if (f_10 != 0 && p1.Size() != 0) {
        if (p1.Size() == 1) {
            SBGeoPoint2D const& p = p1.Get(0);
            if (f_10 != 0 && f_0.f_0 <= p.f_0 && p.f_0 <= f_8.f_0 && f_0.f_4 <= p.f_4) r = f_8.f_4 < p.f_4;
        } else {
            int n = p1.Size();
            if (n != 1) {
                uint32_t i = 0;
                do {
                    SBGeoSegment2D s = p1.GetSegment(i);
                    if (!IsTriviallyRejected(s)) return false;
                    i++;
                } while (i < (uint32_t)(n - 1));
                r = true;
            }
        }
    }
    return r;
}

// 08158630  (borders included)
bool SBGeoBoundingBox2D::IsInside_p(SBGeoPoint2D const& p1) const {
    if (f_10 == 0) return false;
    if (p1.f_0 < f_0.f_0) return false;
    if (f_8.f_0 < p1.f_0) return false;
    if (p1.f_4 < f_0.f_4) return false;
    return p1.f_4 <= f_8.f_4;
}

// 08158690
bool SBGeoBoundingBox2D::IsTriviallyRejected(SBGeoPolyLine2DArray const& p1) const {
    bool r = true;
    if (f_10 != 0 && p1.Size() != 0) {
        if (p1.Size() == 1) {
            SBGeoPoint2D const& p = p1.Get(0);
            if (f_10 != 0 && f_0.f_0 <= p.f_0 && p.f_0 <= f_8.f_0 && f_0.f_4 <= p.f_4) r = f_8.f_4 < p.f_4;
        } else {
            int n = p1.Size();
            if (n != 1) {
                uint32_t i = 0;
                do {
                    SBGeoSegment2D s = p1.GetSegment(i);
                    if (!IsTriviallyRejected(s)) return false;
                    i++;
                } while (i < (uint32_t)(n - 1));
                r = true;
            }
        }
    }
    return r;
}

// 08158790
bool SBGeoBoundingBox2D::IsTriviallyRejected(SBGeoPolygon2DList const& p1) const {
    bool r = true;
    if (f_10 != 0 && p1.Size() != 0) {
        if (p1.Size() == 1) {
            SBGeoPoint2D const& p = p1.Get(0);
            if (f_10 != 0 && f_0.f_0 <= p.f_0 && p.f_0 <= f_8.f_0 && f_0.f_4 <= p.f_4) r = f_8.f_4 < p.f_4;
        } else {
            bool rej;
            {
                SBGeoPolyLine2DList open(p1);
                rej = IsTriviallyRejected(open);
            }
            if (!rej) {
                r = false;
            } else {
                SBGeoSegment2D closing(p1.Get(0), p1.Get(p1.Size() - 1));
                r = IsTriviallyRejected(closing);
            }
        }
    }
    return r;
}

// 081588d0
bool SBGeoBoundingBox2D::IsTriviallyRejected(SBGeoPolygon2DArray const& p1) const {
    bool r = true;
    if (f_10 != 0 && p1.Size() != 0) {
        if (p1.Size() == 1) {
            SBGeoPoint2D const& p = p1.Get(0);
            if (f_10 != 0 && f_0.f_0 <= p.f_0 && p.f_0 <= f_8.f_0 && f_0.f_4 <= p.f_4) r = f_8.f_4 < p.f_4;
        } else {
            bool rej;
            {
                SBGeoPolyLine2DList open(p1);
                rej = IsTriviallyRejected(open);
            }
            if (!rej) {
                r = false;
            } else {
                SBGeoSegment2D closing(p1.Get(0), p1.Get(p1.Size() - 1));
                r = IsTriviallyRejected(closing);
            }
        }
    }
    return r;
}

// 08158a10  (the diagonal of p1 is tested as a segment)
bool SBGeoBoundingBox2D::IsTriviallyRejected(SBGeoBoundingBox2D const& p1) const {
    SBGeoSegment2D diag(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    bool r = true;
    if (f_10 != 0 && p1.f_10 != 0) {
        diag = SBGeoSegment2D(p1.f_0, p1.f_8);
        r = IsTriviallyRejected(diag);
    }
    return r;
}

// 08158b20  (right and bottom borders excluded)
bool SBGeoBoundingBox2D::IsBoxed(SBGeoPoint2D const& p1) const {
    if (f_10 == 0) return false;
    if (p1.f_0 < f_0.f_0) return false;
    if (f_8.f_0 <= p1.f_0) return false;
    if (p1.f_4 < f_0.f_4) return false;
    return p1.f_4 < f_8.f_4;
}

// 08158b80
bool SBGeoBoundingBox2D::IsInside_s(SBGeoSegment2D const& p1) const {
    if (f_10 == 0) return false;
    if (p1.f_0.f_0 < f_0.f_0) return false;
    if (f_8.f_0 < p1.f_0.f_0) return false;
    if (p1.f_0.f_4 < f_0.f_4) return false;
    if (f_8.f_4 < p1.f_0.f_4) return false;
    bool r = false;
    if (f_0.f_0 <= p1.f_8.f_0 && p1.f_8.f_0 <= f_8.f_0) {
        if (p1.f_8.f_4 < f_0.f_4) return false;
        r = p1.f_8.f_4 <= f_8.f_4;
    }
    return r;
}

// 08158c10  all the points inside (true for an empty polyline)
bool SBGeoBoundingBox2D::IsInside(SBGeoPolyLine2DList const& p1) const {
    bool r = true;
    if (p1.Size() != 0) {
        uint32_t i = 0;
        r = false;
        if (f_10 != 0) {
            int n = p1.Size();
            while (true) {
                SBGeoPoint2D const& p = p1.Get(i);
                if (f_10 == 0) return false;
                if (p.f_0 < f_0.f_0) break;
                if (f_8.f_0 < p.f_0) return false;
                if (p.f_4 < f_0.f_4) return false;
                i++;
                r = p.f_4 <= f_8.f_4;
                if ((uint32_t)(n - 1) < i) return r;
                if (!r) return r;
            }
            r = false;
        }
    }
    return r;
}

// 08158cc0
bool SBGeoBoundingBox2D::IsInside(SBGeoPolyLine2DArray const& p1) const {
    bool r = true;
    if (p1.Size() != 0) {
        uint32_t i = 0;
        r = false;
        if (f_10 != 0) {
            int n = p1.Size();
            while (true) {
                SBGeoPoint2D const& p = p1.Get(i);
                if (f_10 == 0) return false;
                if (p.f_0 < f_0.f_0) break;
                if (f_8.f_0 < p.f_0) return false;
                if (p.f_4 < f_0.f_4) return false;
                i++;
                r = p.f_4 <= f_8.f_4;
                if ((uint32_t)(n - 1) < i) return r;
                if (!r) return r;
            }
            r = false;
        }
    }
    return r;
}

// 08158d70
bool SBGeoBoundingBox2D::IsInside_b(SBGeoBoundingBox2D const& p1) const {
    SBGeoSegment2D diag(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    if (p1.f_10 == 0) return false;
    diag = SBGeoSegment2D(p1.f_0, p1.f_8);
    if (f_10 == 0) return false;
    if (!(f_0.f_0 <= diag.f_0.f_0)) return false;
    if (!(diag.f_0.f_0 <= f_8.f_0)) return false;
    if (!(f_0.f_4 <= diag.f_0.f_4)) return false;
    if (!(diag.f_0.f_4 <= f_8.f_4)) return false;
    if (f_0.f_0 <= diag.f_8.f_0 && diag.f_8.f_0 <= f_8.f_0) {
        if (f_0.f_4 <= diag.f_8.f_4) return diag.f_8.f_4 <= f_8.f_4;
        return false;
    }
    return false;
}

// 08158ee0  on one of the four sides (the flag is not checked)
bool SBGeoBoundingBox2D::IsOnBoundary(SBGeoPoint2D const& p1) const {
    float x = p1.f_0;
    float y = p1.f_4;
    bool le;
    bool lt;
    if (x != f_0.f_0 && x != f_8.f_0) {
        if (y != f_0.f_4) {
            if (y != f_8.f_4) return false;
        }
        le = x <= f_8.f_0;
        lt = x < f_0.f_0;
    } else {
        le = y <= f_8.f_4;
        lt = y < f_0.f_4;
    }
    return !lt && le;
}

// 08158f50
bool SBGeoBoundingBox2D::IsIntersecting(SBGeoPoint2D const& p1) const {
    if (f_10 == 0) return false;
    if (p1.f_0 < f_0.f_0) return false;
    if (f_8.f_0 < p1.f_0) return false;
    if (p1.f_4 < f_0.f_4) return false;
    return p1.f_4 <= f_8.f_4;
}

// 08158fb0
bool SBGeoBoundingBox2D::IsIntersecting(SBGeoSegment2D const& p1) const {
    if (f_10 == 0) return false;
    if (IsTriviallyRejected(p1)) return false;
    if (f_10 != 0) {
        if (f_0.f_0 <= p1.f_0.f_0 && p1.f_0.f_0 <= f_8.f_0 && f_0.f_4 <= p1.f_0.f_4 && p1.f_0.f_4 <= f_8.f_4)
            return true;
        if (f_0.f_0 <= p1.f_8.f_0 && p1.f_8.f_0 <= f_8.f_0 && f_0.f_4 <= p1.f_8.f_4 && p1.f_8.f_4 <= f_8.f_4)
            return true;
    }
    SBGeoLine2D l(p1);
    return IsIntersecting(l);
}

// 08159090  the line crosses the box when the corners are not all on the same side
bool SBGeoBoundingBox2D::IsIntersecting(SBGeoLine2D const& p1) const {
    SBGeoPoint2D c1;  // (right, top)
    SBGeoPoint2D c3;  // (left, bottom)
    SBGeoPoint2D a;
    SBGeoPoint2D b;
    SBGeoVector2D v;
    if (f_10 == 0) return false;
    if (IsTriviallyRejected(p1)) return false;
    a = p1.f_0;
    b = p1.f_8;
    v = b - a;
    c1 = SBGeoPoint2D(f_8.f_0, f_0.f_4);
    c3 = SBGeoPoint2D(f_0.f_0, f_8.f_4);
    float d = v.Det(f_0 - a);
    if (d == 0.0f) return true;
    if (d > 0.0f) {
        if (0.0f >= v.Det(c1 - a)) return true;
        if (0.0f >= v.Det(f_8 - a)) return true;
        return 0.0f >= v.Det(c3 - a);
    }
    if (v.Det(c1 - a) >= 0.0f) return true;
    if (v.Det(f_8 - a) >= 0.0f) return true;
    return v.Det(c3 - a) >= 0.0f;
}

// 08159550
bool SBGeoBoundingBox2D::IsIntersecting(SBGeoHalfLine2D const& p1) const {
    if (f_10 == 0) return false;
    if (IsTriviallyRejected(p1)) return false;
    if (f_10 != 0 && !(p1.f_0.f_0 < f_0.f_0) && !(f_8.f_0 < p1.f_0.f_0) && !(p1.f_0.f_4 < f_0.f_4) &&
        !(f_8.f_4 < p1.f_0.f_4))
        return true;
    SBGeoLine2D l(p1);
    return IsIntersecting(l);
}

// 081595f0  a point inside, or a segment crossing
bool SBGeoBoundingBox2D::IsIntersecting(SBGeoPolyLine2DList const& p1) const {
    if (p1.Size() == 0) return false;
    if (p1.Size() == 1) return PointIn(*this, p1.Get(0));
    int n = p1.Size();
    uint32_t i = 0;
    bool in;
    do {
        in = PointIn(*this, p1.Get(i));
        i++;
    } while (i <= (uint32_t)(n - 1) && !in);
    if (in) return true;
    i = 0;
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

// 08159770
bool SBGeoBoundingBox2D::IsIntersecting(SBGeoPolyLine2DArray const& p1) const {
    if (p1.Size() == 0) return false;
    if (p1.Size() == 1) return PointIn(*this, p1.Get(0));
    int n = p1.Size();
    uint32_t i = 0;
    bool in;
    do {
        in = PointIn(*this, p1.Get(i));
        i++;
    } while (i <= (uint32_t)(n - 1) && !in);
    if (in) return true;
    i = 0;
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

// 081598f0  a point inside, an edge crossing, or the box inside the polygon (tested with its
// top left corner)
bool SBGeoBoundingBox2D::IsIntersecting(SBGeoPolygon2DList const& p1) const {
    if (p1.Size() == 0) return false;
    if (p1.Size() == 1) return PointIn(*this, p1.Get(0));
    bool c;
    {
        SBGeoPolyLine2DList open(p1);
        c = IsIntersecting(open);
    }
    if (c) return true;
    SBGeoSegment2D closing(p1.Get(0), p1.Get(p1.Size() - 1));
    if (IsIntersecting(closing)) return true;
    return p1.IsInside_p(f_0);
}

// 08159a50
bool SBGeoBoundingBox2D::IsIntersecting(SBGeoPolygon2DArray const& p1) const {
    if (p1.Size() == 0) return false;
    if (p1.Size() == 1) return PointIn(*this, p1.Get(0));
    bool c;
    {
        SBGeoPolyLine2DArray open(p1);
        c = IsIntersecting(open);
    }
    if (c) return true;
    SBGeoSegment2D closing(p1.Get(0), p1.Get(p1.Size() - 1));
    if (IsIntersecting(closing)) return true;
    return p1.IsInside_p(f_0);
}

// 08159c60
bool SBGeoBoundingBox2D::IsIntersecting(SBGeoBoundingBox2D const& p1) const {
    return !IsTriviallyRejected(p1);
}

// 08159c80
SBList<unsigned int> SBGeoBoundingBox2D::IntersectionIndexList(SBGeoPolyLine2DList const& p1) const {
    return MyIntersectionIndexList(p1);
}

// 08159cb0  indices of the segments crossing the box
SBList<unsigned int> SBGeoBoundingBox2D::MyIntersectionIndexList(SBGeoPolyLine2DList const& p1) const {
    SBList<unsigned int> res;
    if (1 < p1.Size()) {
        int n = p1.Size();
        if (n != 1) {
            uint32_t i = 0;
            do {
                bool c;
                {
                    SBGeoSegment2D s = p1.GetSegment(i);
                    c = IsIntersecting(s);
                }
                if (c) res.AddLast(i);
                i++;
            } while (i < (uint32_t)(n - 1));
        }
    }
    return res;
}

// 08159dd0
SBList<unsigned int> SBGeoBoundingBox2D::IntersectionIndexList(SBGeoPolyLine2DArray const& p1) const {
    return MyIntersectionIndexList(p1);
}

// 08159e00
SBList<unsigned int> SBGeoBoundingBox2D::MyIntersectionIndexList(SBGeoPolyLine2DArray const& p1) const {
    SBList<unsigned int> res;
    if (1 < p1.Size()) {
        int n = p1.Size();
        if (n != 1) {
            uint32_t i = 0;
            do {
                bool c;
                {
                    SBGeoSegment2D s = p1.GetSegment(i);
                    c = IsIntersecting(s);
                }
                if (c) res.AddLast(i);
                i++;
            } while (i < (uint32_t)(n - 1));
        }
    }
    return res;
}

// 08159f20  the open segments, then the closing one (index n - 1)
SBList<unsigned int> SBGeoBoundingBox2D::IntersectionIndexList(SBGeoPolygon2DList const& p1) const {
    SBList<unsigned int> res;
    uint32_t n = p1.Size();
    if (1 < n) {
        res = MyIntersectionIndexList((SBGeoPolyLine2DList const&)p1);
        bool c;
        {
            SBGeoSegment2D s = p1.GetSegment(n - 1);
            c = IsIntersecting(s);
        }
        if (c) res.AddLast(n - 1);
    }
    return res;
}

// 0815a0e0
SBList<unsigned int> SBGeoBoundingBox2D::IntersectionIndexList(SBGeoPolygon2DArray const& p1) const {
    SBList<unsigned int> res;
    uint32_t n = p1.Size();
    if (1 < n) {
        res = MyIntersectionIndexList((SBGeoPolyLine2DArray const&)p1);
        bool c;
        {
            SBGeoSegment2D s = p1.GetSegment(n - 1);
            c = IsIntersecting(s);
        }
        if (c) res.AddLast(n - 1);
    }
    return res;
}

// 0815a2a0
bool SBGeoBoundingBox2D::IsSomewhere() const {
    return f_10 != 0;
}

// 0815a2b0
SBGeoPoint2D SBGeoBoundingBox2D::GetCenter() const {
    SBGeoVector2D c;
    SBGeoVector2D a(f_0);
    SBGeoVector2D b(f_8);
    c = SBGeoVector2D((SBGeoPoint2D const&)a + b);
    c /= 2.0f;
    return SBGeoPoint2D(c);
}

// 0815a3e0  sets the corners; tells whether they are in order
bool SBGeoBoundingBox2D::Set(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2) {
    f_0 = p1;
    f_8 = p2;
    f_10 = 1;
    if (f_0.f_0 <= f_8.f_0) return f_0.f_4 <= f_8.f_4;
    return false;
}

// 0815a440
bool SBGeoBoundingBox2D::IsOK() const {
    bool r = true;
    if (f_10 != 0) {
        if (f_8.f_0 < f_0.f_0) return false;
        r = f_0.f_4 <= f_8.f_4;
    }
    return r;
}

// 0815a470
SBGeoBoundingBox2D SBGeoBoundingBox2D::operator+(SBGeoVector2D const& p1) const {
    SBGeoPoint2D a = f_0 + p1;
    SBGeoPoint2D b = f_8 + p1;
    return SBGeoBoundingBox2D(a, b);
}

// 0815a500
SBGeoBoundingBox2D& SBGeoBoundingBox2D::operator+=(SBGeoVector2D const& p1) {
    f_0 += p1;
    f_8 += p1;
    return *this;
}

// 0815a530
void SBGeoBoundingBox2D::operator*=(float p1) {
    f_0.f_0 = p1 * f_0.f_0;
    f_0.f_4 = p1 * f_0.f_4;
    f_8.f_0 = p1 * f_8.f_0;
    f_8.f_4 = p1 * f_8.f_4;
}

// 0815a550
SBGeoBoundingBox2D SBGeoBoundingBox2D::operator-(SBGeoVector2D const& p1) const {
    SBGeoPoint2D a = f_0 - p1;
    SBGeoPoint2D b = f_8 - p1;
    return SBGeoBoundingBox2D(a, b);
}

// 0815a5e0
SBGeoBoundingBox2D& SBGeoBoundingBox2D::operator-=(SBGeoVector2D const& p1) {
    f_0 -= p1;
    f_8 -= p1;
    return *this;
}

// 0815a610  intersection of the two boxes (empty when they do not meet)
SBGeoBoundingBox2D SBGeoBoundingBox2D::Clip(SBGeoBoundingBox2D const& p1) const {
    SBGeoBoundingBox2D r;
    r = p1;
    if (f_10 != 0 && !IsTriviallyRejected(r)) {
        if (f_0.f_0 > r.f_0.f_0) r.f_0.f_0 = f_0.f_0;
        if (f_8.f_0 < r.f_8.f_0) r.f_8.f_0 = f_8.f_0;
        if (f_0.f_4 > r.f_0.f_4) r.f_0.f_4 = f_0.f_4;
        if (!(r.f_8.f_4 <= f_8.f_4)) r.f_8.f_4 = f_8.f_4;
        return r;
    }
    r.f_10 = 0;
    return r;
}

// 0815a6e0
SBGeoIntersection2D SBGeoBoundingBox2D::Clip(SBGeoLine2D const& p1) const {
    if (IsTriviallyRejected(p1)) return SBGeoIntersection2D();
    return MyClip(p1, -(double)FLT_MAX, (double)FLT_MAX);
}

// 0815a750  Liang-Barsky clipping of the part of the line with parameter in [p2, p3]
// (0 = first point, 1 = second point); the result is a segment, or nothing
SBGeoIntersection2D SBGeoBoundingBox2D::MyClip(SBGeoLine2D const& p1, double p2, double p3) const {
    SBGeoPoint2D a;
    SBGeoPoint2D b;
    SBGeoPoint2D ra;
    SBGeoPoint2D rb;
    float xmin = f_0.f_0;
    float ymin = f_0.f_4;
    float xmax = f_8.f_0;
    float ymax = f_8.f_4;
    a = p1.f_0;
    b = p1.f_8;
    double lo = p2;
    double hi = p3;
    float dx = b.f_0 - a.f_0;
    if (dx == 0.0f) {
        // vertical: only y is clipped (x was checked by IsTriviallyRejected)
        ra = a;
        rb = b;
        if ((0.0 > lo && b.f_4 > a.f_4) || ymin > a.f_4)
            ra.f_4 = ymin;
        else if (a.f_4 > ymax || (0.0 > lo && a.f_4 > b.f_4))
            ra.f_4 = ymax;
        if ((hi > 1.0 && a.f_4 > b.f_4) || ymin > b.f_4)
            rb.f_4 = ymin;
        else if (b.f_4 > ymax || (hi > 1.0 && b.f_4 > a.f_4))
            rb.f_4 = ymax;
        return SBGeoIntersection2D(SBGeoSegment2D(ra, rb));
    }
    float dy = b.f_4 - a.f_4;
    if (dy == 0.0f) {
        // horizontal: only x is clipped
        ra = a;
        rb = b;
        if ((0.0 > lo && b.f_0 > a.f_0) || xmin > a.f_0)
            ra.f_0 = xmin;
        else if (a.f_0 > xmax || (0.0 > lo && a.f_0 > b.f_0))
            ra.f_0 = xmax;
        if ((hi > 1.0 && a.f_0 > b.f_0) || xmin > b.f_0)
            rb.f_0 = xmin;
        else if (b.f_0 > xmax || (hi > 1.0 && b.f_0 > a.f_0))
            rb.f_0 = xmax;
        return SBGeoIntersection2D(SBGeoSegment2D(ra, rb));
    }
    double t0 = lo;
    double t1;
    if (dx > 0.0f) {
        if (0.0 > lo || xmin > a.f_0) {
            double t = (double)(xmin - a.f_0) / (double)dx;
            if (t > lo) t0 = t;
        }
        if (t0 > hi) {
            t1 = hi;
        } else if (hi > 1.0 || b.f_0 > xmax) {
            double t = (double)(xmax - a.f_0) / (double)dx;
            t1 = hi > t ? t : hi;
        } else {
            t1 = hi;
        }
    } else {
        if (0.0 > lo || a.f_0 > xmax) {
            double t = (double)(xmax - a.f_0) / (double)dx;
            if (t > lo) t0 = t;
        }
        if (t0 > hi) {
            t1 = hi;
        } else if (hi > 1.0 || xmin > b.f_0) {
            double t = (double)(xmin - a.f_0) / (double)dx;
            t1 = hi > t ? t : hi;
        } else {
            t1 = hi;
        }
    }
    double t2;
    if (dy > 0.0f) {
        if (!(t0 > t1) && (0.0 > lo || ymin > a.f_4)) {
            double t = (double)(ymin - a.f_4) / (double)dy;
            if (t > t0) t0 = t;
        }
        if (t0 > t1) {
            t2 = t1;
        } else if (hi > 1.0 || b.f_4 > ymax) {
            double t = (double)(ymax - a.f_4) / (double)dy;
            t2 = t1 > t ? t : t1;
        } else {
            t2 = t1;
        }
    } else {
        if (!(t0 > t1) && (0.0 > lo || a.f_4 > ymax)) {
            double t = (double)(ymax - a.f_4) / (double)dy;
            if (t > t0) t0 = t;
        }
        if (t0 > t1) {
            t2 = t1;
        } else if (hi > 1.0 || ymin > b.f_4) {
            double t = (double)(ymin - a.f_4) / (double)dy;
            t2 = t1 > t ? t : t1;
        } else {
            t2 = t1;
        }
    }
    if (t0 > t2) return SBGeoIntersection2D();
    ra.f_0 = (float)(t0 * (double)dx) + a.f_0;
    ra.f_4 = (float)(t0 * (double)dy) + a.f_4;
    rb.f_0 = (float)(t2 * (double)dx) + a.f_0;
    rb.f_4 = (float)(t2 * (double)dy) + a.f_4;
    return SBGeoIntersection2D(SBGeoSegment2D(ra, rb));
}

// 0815ad90
SBGeoIntersection2D SBGeoBoundingBox2D::Clip(SBGeoHalfLine2D const& p1) const {
    if (IsTriviallyRejected(p1)) return SBGeoIntersection2D();
    return MyClip(p1, 0.0, (double)FLT_MAX);
}

// 0815ae00
SBGeoIntersection2D SBGeoBoundingBox2D::Clip(SBGeoSegment2D const& p1) const {
    if (IsTriviallyRejected(p1)) return SBGeoIntersection2D();
    return MyClip(p1, 0.0, 1.0);
}

// the pieces of a polyline inside the box (shared by the two polyline kinds)
template <class P>
static SBListAutoDelete<SBGeoPolyLine2DList*>* ClipPolyLine(SBGeoBoundingBox2D const& box, P const& p1) {
    SBGeoPoint2D prev;
    SBGeoPoint2D cur;
    SBGeoIntersection2D inter;
    SBGeoSegment2D piece(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoPoint2D pa;
    SBGeoPoint2D pb;
    SBListAutoDelete<SBGeoPolyLine2DList*>* res = new SBListAutoDelete<SBGeoPolyLine2DList*>;
    if (p1.Size() != 0) {
        SBGeoPolyLine2DList* part = nullptr;
        prev = p1.Get(0);
        bool wasIn = PointIn(box, prev);
        if (wasIn) {
            part = new SBGeoPolyLine2DList;
            part->AddLast(prev);
        }
        bool in;
        int n = p1.Size();
        if (n == 1) {
            in = wasIn;
        } else {
            uint32_t i = 1;
            do {
                cur = p1.Get(i);
                {
                    SBGeoSegment2D s(prev, cur);
                    if (!box.IsTriviallyRejected(s))
                        inter = box.MyClip(s, 0.0, 1.0);
                    else
                        inter = SBGeoIntersection2D();
                }
                in = PointIn(box, cur);
                if (inter.GetType() == SBGeoIntersection2D::SEGMENT) {
                    piece = *inter.GetSegment();
                    pa = piece.f_0;
                    pb = piece.f_8;
                    if (wasIn != true) {
                        part = new SBGeoPolyLine2DList;
                        part->AddLast(pa);
                    }
                    if (pb != pa) part->AddLast(pb);
                    if (!in) res->AddLast(part);
                }
                prev = cur;
                i++;
                wasIn = in;
            } while (i <= (uint32_t)(n - 1));
        }
        if (in && part->Size() != 0) res->AddLast(part);
    }
    return res;
}

// 0815ae70
SBListAutoDelete<SBGeoPolyLine2DList*>* SBGeoBoundingBox2D::Clip(SBGeoPolyLine2DList const& p1) const {
    return ClipPolyLine(*this, p1);
}

// 0815b440
SBListAutoDelete<SBGeoPolyLine2DList*>* SBGeoBoundingBox2D::Clip(SBGeoPolyLine2DArray const& p1) const {
    return ClipPolyLine(*this, p1);
}

// Liang-Barsky polygon clipping against the box into p2, then removal of the degenerate box
// corners (shared by the two polygon kinds; strictCorner selects the corner test of the list
// version, which differs from the array version in the original)
template <class P>
static void ClipPolygon(SBGeoBoundingBox2D const& box, P const& p1, SBGeoPolygon2DList& p2, bool strictCorner) {
    SBGeoPoint2D prev;
    SBGeoPoint2D cur;
    SBList<uint8_t> codes;
    float xmin = box.f_0.f_0;
    float ymin = box.f_0.f_4;
    float xmax = box.f_8.f_0;
    float ymax = box.f_8.f_4;
    int n0 = p1.Size();
    p2.Delete();
    if (p1.Size() == 0) return;
    prev = p1.Get(n0 - 1);
    uint32_t i = 0;
    do {
        cur = p1.Get(i);
        float x0 = prev.f_0;
        float y0 = prev.f_4;
        float dx = cur.f_0 - x0;
        float dy = cur.f_4 - y0;
        float xIn, xOut, yIn, yOut;
        if (dx > 0.0f || (dx == 0.0f && x0 > xmax)) {
            xIn = xmin;
            xOut = xmax;
        } else {
            xIn = xmax;
            xOut = xmin;
        }
        if (dy > 0.0f || (dy == 0.0f && y0 > ymax)) {
            yIn = ymin;
            yOut = ymax;
        } else {
            yIn = ymax;
            yOut = ymin;
        }
        double tOutX;
        if (dx != 0.0f) {
            tOutX = (double)((xOut - x0) / dx);  // (sic) single precision division here only
        } else if (x0 < xmin) {
            tOutX = -(double)FLT_MAX;
        } else {
            tOutX = xmax < x0 ? -(double)FLT_MAX : (double)FLT_MAX;
        }
        double tOutY;
        if (dy != 0.0f) {
            tOutY = (double)(yOut - y0) / (double)dy;
        } else if (y0 < ymin) {
            tOutY = -(double)FLT_MAX;
        } else {
            tOutY = ymax < y0 ? -(double)FLT_MAX : (double)FLT_MAX;
        }
        double tOut2 = tOutY > tOutX ? tOutY : tOutX;
        if (tOut2 > 0.0) {
            double tInX = -(double)FLT_MAX;
            if (dx != 0.0f) tInX = (double)(xIn - x0) / (double)dx;
            double tOut1 = tOutX < tOutY ? tOutX : tOutY;
            double tInY = -(double)FLT_MAX;
            if (dy != 0.0f) tInY = (double)(yIn - y0) / (double)dy;
            double tIn2 = tInY > tInX ? tInY : tInX;
            if (tIn2 > tOut1) {
                // no visible part: turning vertex
                if (tOut1 > 0.0 && 1.0 >= tOut1) {
                    if (tInY > tInX)
                        p2.AddLast(SBGeoPoint2D(xOut, yIn));
                    else
                        p2.AddLast(SBGeoPoint2D(xIn, yOut));
                }
            } else if (tOut1 > 0.0 && 1.0 >= tIn2) {
                if (tIn2 > 0.0) {
                    if (tInX > tInY)
                        p2.AddLast(SBGeoPoint2D(xIn, (float)((double)dy * tInX) + y0));
                    else
                        p2.AddLast(SBGeoPoint2D((float)((double)dx * tInY) + x0, yIn));
                }
                if (1.0 > tOut1) {
                    if (tOutY > tOutX)
                        p2.AddLast(SBGeoPoint2D(xOut, (float)((double)dy * tOutX) + y0));
                    else
                        p2.AddLast(SBGeoPoint2D((float)((double)dx * tOutY) + x0, yOut));
                } else {
                    p2.AddLast(cur);
                }
            }
            if (1.0 >= tOut2) p2.AddLast(SBGeoPoint2D(xOut, yOut));
        }
        prev = cur;
        i++;
    } while (i <= (uint32_t)(n0 - 1));

    // where each vertex is: 1 top, 2 right, 4 bottom, 8 left, 0x10 corner
    uint32_t n = p2.Size();
    if (n < 3) {
        p2.Delete();
        return;
    }
    uint32_t last = n - 1;
    for (uint32_t j = 0; j <= last; j++) {
        SBGeoPoint2D const& p = p2.Get(j);
        uint8_t code;
        if (p.f_0 != xmin) {
            if (p.f_0 != xmax) {
                if (p.f_4 != ymin) code = p.f_4 != ymax ? 0x00 : 0x04;
                else code = 0x01;
            } else if (p.f_4 != ymin) {
                code = p.f_4 != ymax ? 0x02 : 0x16;
            } else {
                code = 0x13;
            }
        } else if (p.f_4 != ymin) {
            code = p.f_4 != ymax ? 0x08 : 0x1c;
        } else {
            code = 0x19;
        }
        codes.AddLast(code);
    }
    // remove the corners added for nothing (between two vertices on the same sides)
    uint32_t k = 1;
    i = 0;
    do {
        uint8_t a = codes[i];
        uint8_t b = codes[k];
        bool remove;
        if (strictCorner) {
            bool all = false;
            if ((codes[last] & a & 0xf) != 0 && (a & 0xf & b) != 0) all = (codes[last] & b & 0xf) != 0;
            remove = (a & 0x10) != 0 && (a == b || all);
        } else {
            remove = (a & 0x10) != 0 && (a == b || (b & codes[last] & 0xf) != 0);
        }
        if (!remove) {
            last = i;
            uint32_t wrap = (k + 1 < n) ? 0 : n;
            i = last + 1;
            if (n <= i) return;
            k = (k + 1) - wrap;
        } else {
            codes.RemoveAt(i);
            p2.Delete(i);
            uint32_t n1 = n - 1;
            k = 1;
            i = 0;
            if (n1 < 3) {
                n = n1;
                last = 0;
                uint32_t wrap = (k + 1 < n) ? 0 : n;
                i = 1;
                if (n <= i) return;
                k = (k + 1) - wrap;
            } else {
                last = n - 2;
                n = n1;
            }
        }
    } while (2 < n);
    p2.Delete();
}

// 0815ba10
void SBGeoBoundingBox2D::ClipAttached(SBGeoPolygon2DList const& p1, SBGeoPolygon2DList& p2) const {
    ClipPolygon(*this, p1, p2, true);
}

// 0815c840
void SBGeoBoundingBox2D::ClipAttached(SBGeoPolygon2DArray const& p1, SBGeoPolygon2DList& p2) const {
    ClipPolygon(*this, p1, p2, false);
}

// 08190b00
SBGeoBoundingBox2D::~SBGeoBoundingBox2D() {}
