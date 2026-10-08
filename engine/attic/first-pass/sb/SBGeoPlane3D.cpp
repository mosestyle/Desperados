// SBGeoPlane3D (see SBGeoPlane3D.h)
#include "sb/SBGeoPlane3D.h"
#include "sb/SBGeoVector2D.h"

#ifdef __clang__
#pragma clang fp contract(off)
#endif

// 083b0850
SBGeoPlane3D::SBGeoPlane3D() {}

// 083b0860
SBGeoPlane3D::SBGeoPlane3D(SBGeoPoint3D const& p1, SBGeoPoint3D const& p2, SBGeoPoint3D const& p3) {
    f_0 = p1;
    f_c = p2;
    f_18 = p3;
}

// 083b08b0
void SBGeoPlane3D::SetPoints(SBGeoPoint3D const& p1, SBGeoPoint3D const& p2, SBGeoPoint3D const& p3) {
    f_0 = p1;
    f_c = p2;
    f_18 = p3;
}

// 083b0900
void SBGeoPlane3D::SetFrame(SBGeoPoint3D const& p1, SBGeoVector3D const& p2, SBGeoVector3D const& p3) {
    f_0 = p1;
    f_24 = p2;
    f_30 = p3;
}

// 083b0950
void SBGeoPlane3D::ComputeVectors() {
    f_24.f_0 = f_c.f_0 - f_0.f_0;
    f_24.f_4 = f_c.f_4 - f_0.f_4;
    f_24.f_8 = f_c.f_8 - f_0.f_8;
    f_30.f_0 = f_18.f_0 - f_0.f_0;
    f_30.f_4 = f_18.f_4 - f_0.f_4;
    f_30.f_8 = f_18.f_8 - f_0.f_8;
}

// 083b09a0
void SBGeoPlane3D::ComputeNormal() {
    float ux = f_24.f_0, uy = f_24.f_4, uz = f_24.f_8;
    float vx = f_30.f_0, vy = f_30.f_4, vz = f_30.f_8;
    float nx = uy * vz - vy * uz;
    float ny = uz * vx - vz * ux;
    float nz = ux * vy - vx * uy;
    float n = sqrtf(nz * nz + (ny * ny + nx * nx));
    f_3c.f_0 = nx / n;
    f_3c.f_4 = ny / n;
    f_3c.f_8 = nz / n;
}

// 083b0a30
SBGeoVector3D SBGeoPlane3D::GetNormal() {
    return f_3c;
}

// 083b0a50
void SBGeoPlane3D::ComputeHomogenEquation() {
    f_48 = (-(f_0.f_0 * f_3c.f_0) - f_0.f_4 * f_3c.f_4) - f_0.f_8 * f_3c.f_8;
}

// 083b0a90
void SBGeoPlane3D::ComputeZEquation() {
    float inv = -1.0f / f_3c.f_8;
    f_4c = f_3c.f_0 * inv;
    f_50 = f_3c.f_4 * inv;
    f_54 = inv * f_48;
}

// 083b0ad0
float SBGeoPlane3D::ComputeZ(float p1, float p2) {
    return (f_50 * p2 + f_4c * p1) + f_54;
}

// 083b0b00  z of the plane at (first point + (p1, p2)) minus the z of the first point
float SBGeoPlane3D::ComputeZIncrement(float p1, float p2) {
    return (((f_0.f_4 + p2) * f_50 + (f_0.f_0 + p1) * f_4c) + f_54) - f_0.f_8;
}

// 083b0b40  ComputeVectors, ComputeNormal, ComputeHomogenEquation and ComputeZEquation in one
void SBGeoPlane3D::InitializeAll() {
    float x0 = f_0.f_0, y0 = f_0.f_4, z0 = f_0.f_8;
    float ux = f_c.f_0 - x0, uy = f_c.f_4 - y0, uz = f_c.f_8 - z0;
    float vx = f_18.f_0 - x0, vy = f_18.f_4 - y0, vz = f_18.f_8 - z0;
    float nx = vz * uy - vy * uz;
    f_24.f_0 = ux;
    f_24.f_4 = uy;
    f_24.f_8 = uz;
    f_30.f_0 = vx;
    f_30.f_4 = vy;
    f_30.f_8 = vz;
    float ny = uz * vx - vz * ux;
    float nz = vy * ux - vx * uy;
    float n = sqrtf(nz * nz + (ny * ny + nx * nx));
    nx = nx / n;
    ny = ny / n;
    nz = nz / n;
    f_3c.f_0 = nx;
    f_3c.f_4 = ny;
    f_3c.f_8 = nz;
    float d = (-(x0 * nx) - y0 * ny) - z0 * nz;
    float inv = -1.0f / nz;
    f_48 = d;
    f_4c = nx * inv;
    f_50 = ny * inv;
    f_54 = inv * d;
}

// 083b0c70  translation (only d and the z constant are updated)
void SBGeoPlane3D::operator+=(SBGeoVector3D const& p1) {
    float x = p1.f_0 + f_0.f_0;
    f_0.f_0 = x;
    float y = p1.f_4 + f_0.f_4;
    f_0.f_4 = y;
    float z = p1.f_8 + f_0.f_8;
    f_0.f_8 = z;
    f_c.f_0 = p1.f_0 + f_c.f_0;
    f_c.f_4 = p1.f_4 + f_c.f_4;
    f_c.f_8 = p1.f_8 + f_c.f_8;
    f_18.f_0 = p1.f_0 + f_18.f_0;
    f_18.f_4 = p1.f_4 + f_18.f_4;
    f_18.f_8 = p1.f_8 + f_18.f_8;
    float d = (-(x * f_3c.f_0) - y * f_3c.f_4) - z * f_3c.f_8;
    f_48 = d;
    f_54 = -d / f_3c.f_8;
}

// 083b0d40
void SBGeoPlane3D::operator+=(SBGeoVector2D const& p1) {
    float dx = p1.f_0;
    float x = f_0.f_0 + dx;
    f_0.f_0 = x;
    float dy = p1.f_4;
    float y = f_0.f_4 + dy;
    f_0.f_4 = y;
    f_c.f_0 = f_c.f_0 + dx;
    f_c.f_4 = f_c.f_4 + dy;
    f_18.f_0 = dx + f_18.f_0;
    f_18.f_4 = dy + f_18.f_4;
    float d = (-(x * f_3c.f_0) - y * f_3c.f_4) - f_0.f_8 * f_3c.f_8;
    f_48 = d;
    f_54 = -d / f_3c.f_8;
}
