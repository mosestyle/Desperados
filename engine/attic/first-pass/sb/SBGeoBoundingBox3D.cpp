// SBGeoBoundingBox3D (see SBGeoBoundingBox3D.h)
#include "sb/SBGeoBoundingBox3D.h"

// 083b0de0  empty box: min = 1e30, max = -1e30 (0x7149f2ca / 0xf149f2ca)
SBGeoBoundingBox3D::SBGeoBoundingBox3D() {
    f_0 = 1e30f;
    f_4 = -1e30f;
    f_8 = 1e30f;
    f_c = -1e30f;
    f_10 = 1e30f;
    f_14 = -1e30f;
}

// 083b0e00
void SBGeoBoundingBox3D::SetTopLeftFront(SBGeoVector3D p1) {
    f_0 = p1.f_0;
    f_8 = p1.f_4;
    f_10 = p1.f_8;
}

// 083b0e20
void SBGeoBoundingBox3D::SetBottomRightBack(SBGeoVector3D p1) {
    f_4 = p1.f_0;
    f_c = p1.f_4;
    f_14 = p1.f_8;
}

// 083b0e40
void SBGeoBoundingBox3D::Expand(SBGeoPoint3D p1) {
    if (p1.f_0 < f_0) f_0 = p1.f_0;
    if (p1.f_0 > f_4) f_4 = p1.f_0;
    if (p1.f_4 < f_8) f_8 = p1.f_4;
    if (p1.f_4 > f_c) f_c = p1.f_4;
    if (p1.f_8 < f_10) f_10 = p1.f_8;
    if (p1.f_8 > f_14) f_14 = p1.f_8;
}

// 083b0eb0
bool SBGeoBoundingBox3D::IsInside(SBGeoVector3D const& p1) {
    float v = p1.f_0;
    if (v < f_0) return false;
    if (v > f_4) return false;
    v = p1.f_4;
    if (v < f_8) return false;
    if (v > f_c) return false;
    v = p1.f_8;
    if (v < f_10) return false;
    if (v > f_14) return false;
    return true;
}
