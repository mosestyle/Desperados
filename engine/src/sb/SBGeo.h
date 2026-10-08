// The original's 2D/3D geometry types (SBLibNG/SBGeo*.cpp), with the same rules: a bounding box
// has a "valid" flag, Set() doesn't reorder its corners, Clip() clamps a copy of the argument to
// this box, and the "trivially rejected" test works on the box's diagonal.
#pragma once
#include <cmath>

struct SBGeoPoint2D {
    float x = 0, y = 0;
    SBGeoPoint2D() = default;
    SBGeoPoint2D(float x_, float y_) : x(x_), y(y_) {}
    SBGeoPoint2D operator+(const SBGeoPoint2D& v) const { return {x + v.x, y + v.y}; }
    SBGeoPoint2D operator-(const SBGeoPoint2D& v) const { return {x - v.x, y - v.y}; }
    SBGeoPoint2D operator*(float k) const { return {x * k, y * k}; }
    SBGeoPoint2D operator/(float k) const { return {x / k, y / k}; }
    SBGeoPoint2D& operator+=(const SBGeoPoint2D& v) { x += v.x; y += v.y; return *this; }
    SBGeoPoint2D& operator-=(const SBGeoPoint2D& v) { x -= v.x; y -= v.y; return *this; }
    bool operator==(const SBGeoPoint2D& o) const { return x == o.x && y == o.y; }
    bool operator!=(const SBGeoPoint2D& o) const { return !(*this == o); }
};
// the original has separate point and vector classes with the same layout
using SBGeoVector2D = SBGeoPoint2D;

struct SBGeoVector3D {
    float x = 0, y = 0, z = 0;
    SBGeoVector3D() = default;
    SBGeoVector3D(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};
using SBGeoPoint3D = SBGeoVector3D;

struct SBGeoSegment2D {
    SBGeoPoint2D a, b;
    SBGeoSegment2D() = default;
    SBGeoSegment2D(const SBGeoPoint2D& a_, const SBGeoPoint2D& b_) : a(a_), b(b_) {}
};

struct SBGeoBoundingBox2D {
    SBGeoPoint2D p0, p1;  // +0 min, +8 max
    bool valid = false;   // +0x10

    SBGeoBoundingBox2D() = default;
    SBGeoBoundingBox2D(float x0, float y0, float x1, float y1) : p0(x0, y0), p1(x1, y1), valid(true) {}
    SBGeoBoundingBox2D(const SBGeoPoint2D& a, const SBGeoPoint2D& b) : p0(a), p1(b), valid(true) {}
    // SBGeoBoundingBox2D(SBGeoPoint2D const&, float w, float h)
    SBGeoBoundingBox2D(const SBGeoPoint2D& p, float w, float h) {
        float x1 = p.x + w, y1 = p.y + h;
        p0 = {x1 <= p.x ? x1 : p.x, y1 <= p.y ? y1 : p.y};
        p1 = {p.x <= x1 ? x1 : p.x, p.y <= y1 ? y1 : p.y};
        valid = false;  // (the original leaves the flag as constructed: unset)
    }

    bool Set(const SBGeoPoint2D& a, const SBGeoPoint2D& b) {
        p0 = a;
        p1 = b;
        valid = true;
        return p0.x <= p1.x && p0.y <= p1.y;
    }
    void Reset() { valid = false; }
    bool IsSomewhere() const { return valid; }
    bool IsOK() const { return !valid || (p0.x <= p1.x && p0.y <= p1.y); }
    float Width() const { return p1.x - p0.x; }
    float Height() const { return p1.y - p0.y; }

    void Expand(const SBGeoPoint2D& p) {
        if (!valid) {
            p0 = p1 = p;
        } else {
            if (p.x < p0.x) p0.x = p.x;
            if (p1.x < p.x) p1.x = p.x;
            if (p.y < p0.y) p0.y = p.y;
            if (p1.y < p.y) p1.y = p.y;
        }
        valid = true;
    }
    void Expand(const SBGeoBoundingBox2D& b) {
        Expand(b.p0);
        Expand(b.p1);
    }

    bool IsTriviallyRejected(const SBGeoSegment2D& s) const {
        if (!valid) return true;
        if ((p0.x <= s.a.x || p0.x <= s.b.x) && (s.a.x <= p1.x || s.b.x <= p1.x) &&
            (p0.y <= s.a.y || p0.y <= s.b.y) && (s.a.y <= p1.y || s.b.y <= p1.y))
            return false;
        return true;
    }
    bool IsTriviallyRejected(const SBGeoBoundingBox2D& b) const {
        if (!valid || !b.valid) return true;
        return IsTriviallyRejected(SBGeoSegment2D(b.p0, b.p1));
    }
    bool IsIntersecting(const SBGeoBoundingBox2D& b) const { return !IsTriviallyRejected(b); }
    bool IsInside_p(const SBGeoPoint2D& p) const {
        return valid && p0.x <= p.x && p.x <= p1.x && p0.y <= p.y && p.y <= p1.y;
    }

    // SBGeoBoundingBox2D::Clip(SBGeoBoundingBox2D const&) const: `b` clamped to this box
    SBGeoBoundingBox2D Clip(const SBGeoBoundingBox2D& b) const {
        SBGeoBoundingBox2D r;
        r.valid = b.valid;
        if (b.valid) {
            r.p0 = b.p0;
            r.p1 = b.p1;
        }
        if (valid && !IsTriviallyRejected(r)) {
            if (r.p0.x < p0.x) r.p0.x = p0.x;
            if (p1.x < r.p1.x) r.p1.x = p1.x;
            if (r.p0.y < p0.y) r.p0.y = p0.y;
            if (p1.y < r.p1.y) r.p1.y = p1.y;
            return r;
        }
        r.valid = false;
        return r;
    }

    SBGeoBoundingBox2D operator+(const SBGeoVector2D& v) const { return {p0 + v, p1 + v}; }
    SBGeoBoundingBox2D operator-(const SBGeoVector2D& v) const { return {p0 - v, p1 - v}; }
};

// ---- the vector and intersection helpers of the original (SBGeoVector2D.cpp, SBGeoSegment2D.cpp,
// SBGeoBoundingBox2D.cpp), with their exact comparisons -----------------------------------------

// SBGeoVector2D::Det (0x0817c630): a.x * b.y - a.y * b.x
inline float SBDet(const SBGeoVector2D& a, const SBGeoVector2D& b) { return b.y * a.x - b.x * a.y; }
// SBGeoVector2D::operator* (dot product)
inline float SBDot(const SBGeoVector2D& a, const SBGeoVector2D& b) { return a.x * b.x + a.y * b.y; }
inline float SBNorm(const SBGeoVector2D& v) { return std::sqrt(v.y * v.y + v.x * v.x); }
// SBGeoVector2D::Normalize (0x0817c7a0)
inline void SBNormalize(SBGeoVector2D& v) {
    float n = std::sqrt(v.y * v.y + v.x * v.x);
    v.x = v.x / n;
    v.y = v.y / n;
}
// SBGeoVector2D::MaxNorm (0x0817c6f0)
inline float SBMaxNorm(const SBGeoVector2D& v) {
    float ax = std::fabs(v.x), ay = std::fabs(v.y);
    return ax < ay ? ay : ax;
}
// SBGeoVector2D::GetNormal (0x0817c8b0): false = (y, -x), true = (-y, x)
inline SBGeoVector2D SBGetNormal(const SBGeoVector2D& v, bool left) {
    return left ? SBGeoVector2D(-v.y, v.x) : SBGeoVector2D(v.y, -v.x);
}
// SBGeoVector2D::GetSector0to15 (0x0817cbd0): 0 = up, 4 = right, 8 = down, 12 = left
inline unsigned SBGetSector0to15(const SBGeoVector2D& v, float k = 1.0f) {
    float fx = v.x * 0.98078525f * k - v.y * 0.19509032f;
    float fy = v.y * 0.98078525f + v.x * 0.19509032f * k;
    bool neg = fx < 0.0f;
    if (fx < 0.0f) fx = -fx;
    bool pos = 0.0f < fy;
    if (fy <= 0.0f) fy = -fy;
    bool b4 = fx * 0.41421357f < fy;
    if (fx <= fy) b4 = fy * 0.41421357f < fx;
    unsigned r = (unsigned)neg << 3;
    bool t = pos != neg;
    if (t) r |= 4;
    t = t != (fy < fx);
    if (t) r |= 2;
    return (unsigned)(t ^ b4) | r;
}
// SBGeoVector2D::SetSector0to15 (0x0817cc80), with the tables marraySectorX / Y (0x0855753c)
inline SBGeoVector2D SBSetSector0to15(unsigned d, float k = 1.0f) {
    static const float X[16] = {0.0f, 0.38268343f, 0.70710677f, 0.9238795f, 1.0f, 0.9238795f, 0.70710677f, 0.38268343f,
                                0.0f, -0.38268343f, -0.70710677f, -0.9238795f, -1.0f, -0.9238795f, -0.70710677f, -0.38268343f};
    static const float Y[16] = {-1.0f, -0.9238795f, -0.70710677f, -0.38268343f, 0.0f, 0.38268343f, 0.70710677f, 0.9238795f,
                                1.0f, 0.9238795f, 0.70710677f, 0.38268343f, 0.0f, -0.38268343f, -0.70710677f, -0.9238795f};
    return SBGeoVector2D(X[d & 0xf] * k, Y[d & 0xf]);
}
// SBGeoBoundingBox2D::GetCenter (0x0815a2b0)
inline SBGeoPoint2D SBCenter(const SBGeoBoundingBox2D& b) { return SBGeoPoint2D((b.p0.x + b.p1.x) / 2.0f, (b.p0.y + b.p1.y) / 2.0f); }

// SBGeoSegment2D::IsIntersecting(SBGeoSegment2D const&) (0x0817a490)
inline bool SBIntersects(const SBGeoSegment2D& s, const SBGeoSegment2D& t) {
    SBGeoBoundingBox2D bs, bt;
    bs.Expand(s.a);
    bs.Expand(s.b);
    bt.Expand(t.a);
    bt.Expand(t.b);
    if (!bs.IsIntersecting(bt)) return false;
    SBGeoVector2D d1 = s.a - s.b;
    SBGeoVector2D d2 = t.b - t.a;
    SBGeoVector2D w = t.a - s.a;
    float a = SBDet(d1, w);
    float b = SBDet(d1, t.b - s.a);
    if (!((0.0f <= a && b <= 0.0f) || (a <= 0.0f && 0.0f <= b))) return false;
    float c = SBDet(d2, w);
    float e = SBDet(d2, s.b - t.a);
    if (0.0f < c || 0.0f < e) return 0.0f <= e && 0.0f <= c;
    return true;
}
// SBGeoBoundingBox2D::IsIntersecting(SBGeoLine2D const&) (0x08159090): the box's corners are
// not all strictly on one side of the line through the segment
inline bool SBBoxIntersectsLine(const SBGeoBoundingBox2D& box, const SBGeoSegment2D& l) {
    if (!box.valid) return false;
    if (box.IsTriviallyRejected(l)) return false;
    SBGeoVector2D d = l.b - l.a;
    SBGeoPoint2D c1(box.p1.x, box.p0.y), c3(box.p0.x, box.p1.y);
    float f = SBDet(d, box.p0 - l.a);
    if (f == 0.0f) return true;
    if (f <= 0.0f) {
        if (!(SBDet(d, c1 - l.a) < 0.0f)) return true;
        if (!(SBDet(d, box.p1 - l.a) < 0.0f)) return true;
        return 0.0f <= SBDet(d, c3 - l.a);
    }
    if (!(0.0f < SBDet(d, c1 - l.a))) return true;
    if (!(0.0f < SBDet(d, box.p1 - l.a))) return true;
    return SBDet(d, c3 - l.a) <= 0.0f;
}
// SBGeoBoundingBox2D::IsIntersecting(SBGeoSegment2D const&) (0x08158fb0)
inline bool SBIntersects(const SBGeoBoundingBox2D& box, const SBGeoSegment2D& s) {
    if (!box.valid) return false;
    if (box.IsTriviallyRejected(s)) return false;
    if (box.p0.x <= s.a.x && s.a.x <= box.p1.x && box.p0.y <= s.a.y && s.a.y <= box.p1.y) return true;
    if (box.p0.x <= s.b.x && s.b.x <= box.p1.x && box.p0.y <= s.b.y && s.b.y <= box.p1.y) return true;
    return SBBoxIntersectsLine(box, s);
}
