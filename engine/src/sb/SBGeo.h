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
