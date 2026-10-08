// SBGeoSegment3D (see SBGeoSegment3D.h)
#include "sb/SBGeoSegment3D.h"

// 083b06a0
void SBGeoSegment3D::Set(SBGeoPoint3D p1, SBGeoPoint3D p2) {
    f_0 = p1;
    f_c = p2;
}

// 083b06d0  equal in either direction
bool SBGeoSegment3D::operator==(SBGeoSegment3D& p1) {
    if (f_0.f_0 == p1.f_0.f_0 && f_0.f_4 == p1.f_0.f_4 && f_0.f_8 == p1.f_0.f_8 && f_c.f_0 == p1.f_c.f_0 &&
        f_c.f_4 == p1.f_c.f_4 && f_c.f_8 == p1.f_c.f_8)
        return true;
    return f_c.f_0 == p1.f_0.f_0 && f_c.f_4 == p1.f_0.f_4 && f_c.f_8 == p1.f_0.f_8 && f_0.f_0 == p1.f_c.f_0 &&
           f_0.f_4 == p1.f_c.f_4 && f_0.f_8 == p1.f_c.f_8;
}

// 083b0790
bool SBGeoSegment3D::operator!=(SBGeoSegment3D& p1) {
    if (f_0.f_0 == p1.f_0.f_0 && f_0.f_4 == p1.f_0.f_4 && f_0.f_8 == p1.f_0.f_8 && f_c.f_0 == p1.f_c.f_0 &&
        f_c.f_4 == p1.f_c.f_4 && f_c.f_8 == p1.f_c.f_8)
        return false;
    if (f_c.f_0 == p1.f_0.f_0 && f_c.f_4 == p1.f_0.f_4 && f_c.f_8 == p1.f_0.f_8 && f_0.f_0 == p1.f_c.f_0 &&
        f_0.f_4 == p1.f_c.f_4)
        return f_0.f_8 != p1.f_c.f_8;
    return true;
}
