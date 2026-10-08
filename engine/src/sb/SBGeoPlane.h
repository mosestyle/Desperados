// SBGeoPlane3D and the polygon / polyline arrays of the original (SBLibNG/SBGeo*.cpp).
#pragma once
#include <cmath>
#include <vector>

#include "sb/SBGeo.h"

struct SBGeoPlane3D {
    SBGeoVector3D p[3];          // +0, +0xc, +0x18
    SBGeoVector3D v1, v2;        // +0x24, +0x30
    SBGeoVector3D normal;        // +0x3c
    float d = 0;                 // +0x48
    float zx = 0, zy = 0, zc = 0;  // +0x4c..+0x54: z = zx * x + zy * y + zc

    void SetPoints(const SBGeoVector3D& a, const SBGeoVector3D& b, const SBGeoVector3D& c) {
        p[0] = a;
        p[1] = b;
        p[2] = c;
    }
    // SBGeoPlane3D::InitializeAll (0x083b0b40)
    void InitializeAll() {
        float x0 = p[0].x, y0 = p[0].y, z0 = p[0].z;
        float ax = p[1].x - x0, ay = p[1].y - y0, az = p[1].z - z0;
        float bx = p[2].x - x0, by = p[2].y - y0, bz = p[2].z - z0;
        v1 = {ax, ay, az};
        v2 = {bx, by, bz};
        float nx = bz * ay - by * az;
        float ny = az * bx - bz * ax;
        float nz = by * ax - bx * ay;
        float len = std::sqrt(nz * nz + ny * ny + nx * nx);
        nx /= len;
        ny /= len;
        nz /= len;
        normal = {nx, ny, nz};
        d = (-(x0 * nx) - y0 * ny) - z0 * nz;
        float k = -1.0f / nz;
        zx = nx * k;
        zy = ny * k;
        zc = k * d;
    }
    float ComputeZ(float x, float y) const { return zx * x + zy * y + zc; }
};

// SBGeoPolygon2DArray: a closed outline
struct SBGeoPolygon2DArray {
    std::vector<SBGeoPoint2D> pts;
    size_t Size() const { return pts.size(); }
    // even-odd test, as the original's IsInside_p
    bool IsInside_p(const SBGeoPoint2D& q) const {
        bool in = false;
        size_t n = pts.size();
        for (size_t i = 0, j = n ? n - 1 : 0; i < n; j = i++) {
            const SBGeoPoint2D& a = pts[i];
            const SBGeoPoint2D& b = pts[j];
            if ((a.y > q.y) != (b.y > q.y) && q.x < (b.x - a.x) * (q.y - a.y) / (b.y - a.y) + a.x) in = !in;
        }
        return in;
    }
};

// SBGeoPolyLine2DArray: an open line (masks use them sorted by x)
struct SBGeoPolyLine2DArray {
    std::vector<SBGeoPoint2D> pts;
    size_t Size() const { return pts.size(); }
    const SBGeoPoint2D& Get(size_t i) const { return pts[i]; }
};
