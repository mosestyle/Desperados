// SBGeoHalfLine2D (see SBGeoHalfLine2D.h)
#include "sb/SBGeoHalfLine2D.h"
#include "sb/SBGeoBoundingBox2D.h"
#include "sb/SBGeoPolyLine2DArray.h"
#include "sb/SBGeoPolyLine2DList.h"
#include "sb/SBGeoPolygon2DArray.h"
#include "sb/SBGeoPolygon2DList.h"
#include "sb/SBGeoSegment2D.h"
#include "sb/SBGeoVector2D.h"

#ifdef __clang__
#pragma clang fp contract(off)
#endif

static const double kEps = (double)1e-09f;

// 0815d870
SBGeoHalfLine2D::SBGeoHalfLine2D(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2) : SBGeoLine2D(p1, p2) {}

// 0815d880
SBGeoHalfLine2D::SBGeoHalfLine2D(SBGeoLine2D const& p1) : SBGeoLine2D(p1.f_0, p1.f_8) {}

// 0815d8b0
SBGeoHalfLine2D::SBGeoHalfLine2D(SBGeoHalfLine2D const& p1) : SBGeoLine2D(p1.f_0, p1.f_8) {}

// 0815d8e0
SBGeoHalfLine2D::SBGeoHalfLine2D(SBGeoSegment2D const& p1) : SBGeoLine2D(p1.f_0, p1.f_8) {}

// 0815d910
SBGeoHalfLine2D& SBGeoHalfLine2D::operator=(SBGeoHalfLine2D const& p1) {
    SBGeoLine2D::operator=(p1);
    return *this;
}

// 0815d930  same origin and parallel directions (sic: also true for opposite directions)
bool SBGeoHalfLine2D::operator==(SBGeoHalfLine2D const& p1) const {
    SBGeoVector2D v;
    v = f_8 - f_0;
    if (!(f_0 == p1.f_0)) return false;
    return v.Det(p1.f_8 - p1.f_0) == 0.0f;
}

// 0815da10
bool SBGeoHalfLine2D::IsIntersecting(SBGeoLine2D const& p1) const {
    return p1.IsIntersecting(*this);
}

// 0815da30  (sic) always true
bool SBGeoHalfLine2D::IsIntersecting(SBGeoHalfLine2D const& p1) const {
    return true;
}

// 0815da40
bool SBGeoHalfLine2D::IsIntersecting(SBGeoSegment2D const& p1) const {
    SBGeoBoundingBox2D box;
    SBGeoVector2D v1;
    SBGeoVector2D v2;
    SBGeoVector2D v3;
    box.Expand(p1);
    if (!box.IsIntersecting(*this)) return false;
    v1 = f_8 - f_0;
    v2 = p1.f_0 - f_0;
    v3 = p1.f_8 - f_0;
    // the segment ends must be on both sides of (or on) the supporting line
    bool straddle = false;
    if (v1.Det(v2) >= 0.0f && 0.0f >= v1.Det(v3)) straddle = true;
    if (!straddle) {
        if (!(0.0f >= v1.Det(v2))) return false;
        if (!(v1.Det(v3) >= 0.0f)) return false;
    }
    // and the crossing must be on the right side of the origin
    if (v2.Det(v1) > 0.0f) return v2.Det(v3) >= 0.0f;
    return 0.0f >= v2.Det(v3);
}

// 0815dd00
bool SBGeoHalfLine2D::IsIntersecting(SBGeoPolyLine2DArray const& p1) const {
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

// 0815dd90
bool SBGeoHalfLine2D::IsIntersecting(SBGeoPolyLine2DList const& p1) const {
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

// 0815de20  (the closing segment is not tested)
bool SBGeoHalfLine2D::IsIntersecting(SBGeoPolygon2DList const& p1) const {
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

// 0815deb0
bool SBGeoHalfLine2D::IsIntersecting(SBGeoPolygon2DArray const& p1) const {
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

// 0815df40
SBGeoIntersection2D SBGeoHalfLine2D::operator^(SBGeoLine2D const& p1) const {
    return p1 ^ *this;
}

// on the half line starting at s going toward e (in one coordinate), with tolerance
static inline bool OnHalf(float s, float e, double v) {
    if (e <= s) return v <= (double)s + kEps;
    return (double)s - kEps <= v;
}

// 0815df70
SBGeoIntersection2D SBGeoHalfLine2D::operator^(SBGeoHalfLine2D const& p1) const {
    SBGeoPoint2D pt;
    SBGeoSegment2D seg(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoIntersection2D r;
    float x1 = f_0.f_0;
    float x2 = f_8.f_0;
    float dx1 = x2 - x1;
    float x3 = p1.f_0.f_0;
    float x4 = p1.f_8.f_0;
    float dx2 = x4 - x3;
    if (dx1 != 0.0f) {
        double a1 = (double)(f_8.f_4 - f_0.f_4) / (double)dx1;
        double b1 = (double)f_0.f_4 - (double)x1 * a1;
        if (dx2 != 0.0f) {
            double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
            double b2 = (double)p1.f_0.f_4 - (double)x3 * a2;
            if (a1 != a2) {
                double x = (b2 - b1) / (a1 - a2);
                if (OnHalf(x1, x2, x) && OnHalf(x3, x4, x)) {
                    pt.f_0 = (float)x;
                    pt.f_4 = (float)(b1 + a1 * x);
                    r.SetPoint(pt);
                } else {
                    r.SetNone();
                }
            } else if (b1 != b2) {
                r.SetNone();
            } else if (x2 <= x1) {
                // same line, this one going toward -x
                if (x3 <= x4) {
                    if (x1 <= x3) {
                        r.SetNone();
                    } else {
                        seg.f_0 = f_0;
                        seg.f_8 = p1.f_0;
                        r.SetSegment(seg);
                    }
                } else if (x1 <= x3) {
                    r.SetHalfLine(*this);
                } else {
                    r.SetHalfLine(p1);
                }
            } else if (x4 <= x3) {
                if (x3 <= x1) {
                    r.SetNone();
                } else {
                    seg.f_0 = f_0;
                    seg.f_8 = p1.f_0;
                    r.SetSegment(seg);
                }
            } else if (x3 <= x1) {
                r.SetHalfLine(*this);
            } else {
                r.SetHalfLine(p1);
            }
        } else {
            // p1 vertical
            double y = a1 * (double)x3 + b1;
            if (OnHalf(x1, x2, (double)x3) && OnHalf(p1.f_0.f_4, p1.f_8.f_4, y)) {
                pt.f_0 = x3;
                pt.f_4 = (float)y;
                r.SetPoint(pt);
            } else {
                r.SetNone();
            }
        }
    } else if (dx2 != 0.0f) {
        // this one vertical
        double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
        double y = a2 * (double)x1 + ((double)p1.f_0.f_4 - (double)x3 * a2);
        if (OnHalf(f_0.f_4, f_8.f_4, y) && OnHalf(x3, x4, (double)x1)) {
            pt.f_0 = x1;
            pt.f_4 = (float)y;
            r.SetPoint(pt);
        } else {
            r.SetNone();
        }
    } else if (x1 != x3) {
        r.SetNone();
    } else {
        // both vertical on the same x
        float y1 = f_0.f_4;
        float y3 = p1.f_0.f_4;
        if (f_8.f_4 <= y1) {
            if (y3 <= p1.f_8.f_4) {
                if (y1 <= y3) {
                    r.SetNone();
                } else {
                    seg.f_0 = f_0;
                    seg.f_8 = p1.f_0;
                    r.SetSegment(seg);
                }
            } else if (y1 <= y3) {
                r.SetHalfLine(*this);
            } else {
                r.SetHalfLine(p1);
            }
        } else if (p1.f_8.f_4 <= y3) {
            if (y3 <= y1) {
                r.SetNone();
            } else {
                seg.f_0 = f_0;
                seg.f_8 = p1.f_0;
                r.SetSegment(seg);
            }
        } else if (y3 <= y1) {
            r.SetHalfLine(*this);
        } else {
            r.SetHalfLine(p1);
        }
    }
    return r;
}

// between a and b (in either order), with tolerance
static inline bool InRange(double v, float a, float b) {
    return (v <= (double)a + kEps && (double)b - kEps <= v) || ((double)a - kEps <= v && v <= (double)b + kEps);
}

// 0815e900
SBGeoIntersection2D SBGeoHalfLine2D::operator^(SBGeoSegment2D const& p1) const {
    SBGeoPoint2D pt;
    SBGeoSegment2D seg(SBGeoPoint2D(0.0f, 0.0f), SBGeoPoint2D(0.0f, 0.0f));
    SBGeoIntersection2D r;
    float x1 = f_0.f_0;
    float x2 = f_8.f_0;
    float dx1 = x2 - x1;
    float x3 = p1.f_0.f_0;
    float x4 = p1.f_8.f_0;
    float dx2 = x4 - x3;
    if (dx1 != 0.0f) {
        double a1 = (double)(f_8.f_4 - f_0.f_4) / (double)dx1;
        double b1 = (double)f_0.f_4 - (double)x1 * a1;
        if (dx2 != 0.0f) {
            double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
            double b2 = (double)p1.f_0.f_4 - (double)x3 * a2;
            if (a1 != a2) {
                double x = (b2 - b1) / (a1 - a2);
                if (OnHalf(x1, x2, x) && InRange(x, x3, x4)) {
                    pt.f_0 = (float)x;
                    pt.f_4 = (float)(b1 + a1 * x);
                    r.SetPoint(pt);
                } else {
                    r.SetNone();
                }
            } else if (b1 != b2) {
                r.SetNone();
            } else if (x2 <= x1) {
                // same line, the half line going toward -x
                if (x4 <= x1 || x3 <= x1) {
                    if (x1 < x3 || x1 < x4) {
                        if (x3 < x1 || x1 <= x4) {
                            if (x1 <= x3 || x4 < x1) {
                                if ((x3 == x1 && x1 < x4) || (x4 == x1 && x1 < x3)) r.SetPoint(f_0);
                            } else {
                                seg.f_0 = f_0;
                                seg.f_8 = p1.f_0;
                                r.SetSegment(seg);
                            }
                        } else {
                            seg.f_0 = f_0;
                            seg.f_8 = p1.f_8;
                            r.SetSegment(seg);
                        }
                    } else {
                        r.SetSegment(p1);
                    }
                } else {
                    r.SetNone();
                }
            } else if (x1 <= x4 || x1 <= x3) {
                if (x3 < x1 || x4 < x1) {
                    if (x1 < x3 || x4 <= x1) {
                        if (x3 <= x1 || x1 < x4) {
                            if ((x3 == x1 && x4 < x1) || (x4 == x1 && x3 < x1)) r.SetPoint(f_0);
                        } else {
                            seg.f_0 = f_0;
                            seg.f_8 = p1.f_0;
                            r.SetSegment(seg);
                        }
                    } else {
                        seg.f_0 = f_0;
                        seg.f_8 = p1.f_8;
                        r.SetSegment(seg);
                    }
                } else {
                    r.SetSegment(p1);
                }
            } else {
                r.SetNone();
            }
        } else {
            // vertical segment
            double y = a1 * (double)x3 + b1;
            if (OnHalf(x1, x2, (double)x3) && InRange(y, p1.f_0.f_4, p1.f_8.f_4)) {
                pt.f_0 = x3;
                pt.f_4 = (float)y;
                r.SetPoint(pt);
            } else {
                r.SetNone();
            }
        }
    } else if (dx2 != 0.0f) {
        // vertical half line
        double a2 = (double)(p1.f_8.f_4 - p1.f_0.f_4) / (double)dx2;
        double y = a2 * (double)x1 + ((double)p1.f_0.f_4 - (double)x3 * a2);
        if (OnHalf(f_0.f_4, f_8.f_4, y) && InRange((double)x1, x3, x4)) {
            pt.f_0 = x1;
            pt.f_4 = (float)y;
            r.SetPoint(pt);
        } else {
            r.SetNone();
        }
    } else if (x1 != x3) {
        r.SetNone();
    } else {
        // both vertical on the same x
        float y1 = f_0.f_4;
        float y3 = p1.f_0.f_4;
        float y4 = p1.f_8.f_4;
        if (f_8.f_4 <= y1) {
            if (y3 <= y1 || y4 <= y1) {
                if (y1 < y3 || y1 < y4) {
                    if (y3 < y1 || y1 <= y4) {
                        if (y1 <= y3 || y4 < y1) {
                            if (y3 != y1 || y4 <= y1) {
                                if (y3 <= y1) return r;
                                if (y4 != y1) return r;
                            }
                            r.SetPoint(f_0);
                        } else {
                            seg.f_0 = f_0;
                            seg.f_8 = p1.f_0;
                            r.SetSegment(seg);
                        }
                    } else {
                        seg.f_0 = f_0;
                        seg.f_8 = p1.f_8;
                        r.SetSegment(seg);
                    }
                } else {
                    r.SetSegment(p1);
                }
            } else {
                r.SetNone();
            }
        } else if (y1 <= y3 || y1 <= y4) {
            if (y3 < y1 || y4 < y1) {
                if (y1 < y3 || y4 <= y1) {
                    if (y3 <= y1 || y1 < y4) {
                        if (y3 != y1 || y1 <= y4) {
                            if (y1 <= y3) return r;
                            if (y4 != y1) return r;
                        }
                        r.SetPoint(f_0);
                    } else {
                        seg.f_0 = f_0;
                        seg.f_8 = p1.f_0;
                        r.SetSegment(seg);
                    }
                } else {
                    seg.f_0 = f_0;
                    seg.f_8 = p1.f_8;
                    r.SetSegment(seg);
                }
            } else {
                r.SetSegment(p1);
            }
        } else {
            r.SetNone();
        }
    }
    return r;
}

// intersections with the segments of a polyline (shared by the two polyline kinds)
template <class P>
static SBList<SBGeoIntersection2D> HalfLineXPolyLine(SBGeoHalfLine2D const& h, P const& p1) {
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
            if (h.IsIntersecting(seg)) {
                inter = h ^ seg;
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

// 0815f3f0
SBList<SBGeoIntersection2D> SBGeoHalfLine2D::operator^(SBGeoPolyLine2DArray const& p1) const {
    return HalfLineXPolyLine(*this, p1);
}

// 0815f700
SBList<SBGeoIntersection2D> SBGeoHalfLine2D::operator^(SBGeoPolyLine2DList const& p1) const {
    return HalfLineXPolyLine(*this, p1);
}

// 0815fa10  (sic) not implemented in the original: always empty
SBList<SBGeoIntersection2D> SBGeoHalfLine2D::operator^(SBGeoPolygon2DList const& p1) const {
    return SBList<SBGeoIntersection2D>();
}

// 0815fa90  (sic) always empty
SBList<SBGeoIntersection2D> SBGeoHalfLine2D::operator^(SBGeoPolygon2DArray const& p1) const {
    return SBList<SBGeoIntersection2D>();
}

// 0815fb10  (sic) the "opposite" half line passes the difference vector (first - second) as its
// second point instead of first + (first - second)
SBGeoHalfLine2D SBGeoHalfLine2D::operator-() {
    SBGeoVector2D v = f_0 - f_8;
    return SBGeoHalfLine2D(f_0, static_cast<SBGeoPoint2D const&>(v));
}
