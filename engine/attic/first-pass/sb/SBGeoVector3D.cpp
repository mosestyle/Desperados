// SBGeoVector3D (see SBGeoVector3D.h)
#include "sb/SBGeoVector3D.h"
#include "sb/SBFile.h"
#include "sb/SBGeoPoint2D.h"

#ifdef __clang__
#pragma clang fp contract(off)
#endif

// 083b0380  (leaves the coordinates uninitialized)
SBGeoVector3D::SBGeoVector3D() {}

// 083b0390
SBGeoVector3D::SBGeoVector3D(SBGeoPoint2D const& p1, float p2) {
    f_0 = p1.f_0;
    f_4 = p1.f_4;
    f_8 = p2;
}

// 083b03b0
SBGeoVector3D::SBGeoVector3D(float p1, float p2, float p3) {
    f_0 = p1;
    f_4 = p2;
    f_8 = p3;
}

// 083b03e0  (errors are ignored)
int32_t SBGeoVector3D::Serialize(SBFile& p1) {
    p1.Serialize(&f_0, 4);
    p1.Serialize(&f_4, 4);
    p1.Serialize(&f_8, 4);
    return 1;
}

// 083b0440
SBGeoVector3D SBGeoVector3D::operator-(SBGeoVector3D const& p1) const {
    SBGeoVector3D r;
    r.f_0 = f_0 - p1.f_0;
    r.f_4 = f_4 - p1.f_4;
    r.f_8 = f_8 - p1.f_8;
    return r;
}

// 083b0480
SBGeoVector3D SBGeoVector3D::operator+(SBGeoVector3D const& p1) const {
    SBGeoVector3D r;
    r.f_0 = f_0 + p1.f_0;
    r.f_4 = f_4 + p1.f_4;
    r.f_8 = f_8 + p1.f_8;
    return r;
}

// 083b04c0  dot product
float SBGeoVector3D::operator*(SBGeoVector3D const& p1) const {
    return f_8 * p1.f_8 + (f_4 * p1.f_4 + f_0 * p1.f_0);
}

// 083b0500  cross product
SBGeoVector3D SBGeoVector3D::operator^(SBGeoVector3D const& p1) const {
    SBGeoVector3D r;
    r.f_0 = f_4 * p1.f_8 - f_8 * p1.f_4;
    r.f_4 = f_8 * p1.f_0 - p1.f_8 * f_0;
    r.f_8 = p1.f_4 * f_0 - f_4 * p1.f_0;
    return r;
}

// 083b0570
float SBGeoVector3D::MaxNorm() const {
    float ax = fabsf(f_0);
    float ay = fabsf(f_4);
    float az = fabsf(f_8);
    if (ay < ax) return ax > az ? ax : az;
    return ay > az ? ay : az;
}

// 083b05c0  component of (this - p1) orthogonal to the line through p1 and p2
SBGeoVector3D SBGeoVector3D::DistanceVectorToLine(SBGeoVector3D const& p1, SBGeoVector3D const& p2) {
    float dx = p2.f_0 - p1.f_0;
    float dy = p2.f_4 - p1.f_4;
    float dz = p2.f_8 - p1.f_8;
    float inv = 1.0f / sqrtf(dz * dz + (dy * dy + dx * dx));
    float ux = dx * inv;
    float uy = dy * inv;
    float uz = inv * dz;
    float wx = f_0 - p1.f_0;
    float wy = f_4 - p1.f_4;
    float wz = f_8 - p1.f_8;
    float t = uz * wz + (uy * wy + wx * ux);
    SBGeoVector3D r;
    r.f_0 = wx - ux * t;
    r.f_4 = wy - uy * t;
    r.f_8 = wz - t * uz;
    return r;
}
