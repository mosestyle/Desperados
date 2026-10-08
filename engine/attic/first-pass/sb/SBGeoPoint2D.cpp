// SBGeoPoint2D (see SBGeoPoint2D.h)
#include "sb/SBGeoPoint2D.h"
#include "sb/SBFile.h"
#include "sb/SBGeoVector2D.h"
#include <cstdio>

#ifdef __clang__
#pragma clang fp contract(off)
#endif

static const char THIS_FILE[] = "../code/Game/SBLibNG/SBGeoPoint2D.cpp";

// 08162a20
void SBGeoPoint2D::Copy(SBGeoPoint2D const& p1) {
    f_0 = p1.f_0;
    f_4 = p1.f_4;
}

// 08162a40
void SBGeoPoint2D::Destroy() {}

// 08162a50
SBGeoPoint2D::SBGeoPoint2D() {
    f_0 = 0.0f;
    f_4 = 0.0f;
}

// 08162a70
SBGeoPoint2D::SBGeoPoint2D(SBGeoPoint2D const& p1) {
    f_0 = p1.f_0;
    f_4 = p1.f_4;
}

// 08162a90
SBGeoPoint2D::SBGeoPoint2D(float p1, float p2) {
    f_0 = p1;
    f_4 = p2;
}

// 08162ab0
void SBGeoPoint2D::operator=(SBGeoPoint2D const& p1) {
    if (&p1 != this) {
        f_0 = p1.f_0;
        f_4 = p1.f_4;
    }
}

// 08162ad0
bool SBGeoPoint2D::operator==(SBGeoPoint2D const& p1) const {
    return f_0 == p1.f_0 && f_4 == p1.f_4;
}

// 08162b00
bool SBGeoPoint2D::operator!=(SBGeoPoint2D const& p1) const {
    if (f_0 == p1.f_0) return f_4 != p1.f_4;
    return true;
}

// 08162b30
bool SBGeoPoint2D::IsNear(SBGeoPoint2D const& p1, float p2) const {
    bool near = false;
    if (f_0 - p1.f_0 < p2 && p1.f_0 - f_0 < p2) {
        if (p2 <= f_4 - p1.f_4) return false;
        near = p1.f_4 - f_4 < p2;
    }
    return near;
}

// 08162b90
SBGeoVector2D SBGeoPoint2D::operator-(SBGeoPoint2D const& p1) const {
    SBGeoVector2D v;
    v.f_0 = f_0 - p1.f_0;
    v.f_4 = f_4 - p1.f_4;
    return v;
}

// 08162bd0
SBGeoPoint2D::~SBGeoPoint2D() {}

// 08162be0
SBGeoPoint2D SBGeoPoint2D::operator+(SBGeoVector2D const& p1) const {
    SBGeoPoint2D r;
    r.f_0 = f_0 + p1.f_0;
    r.f_4 = f_4 + p1.f_4;
    return r;
}

// 08162c10
void SBGeoPoint2D::operator+=(SBGeoVector2D const& p1) {
    f_0 = f_0 + p1.f_0;
    f_4 = f_4 + p1.f_4;
}

// 08162c40
SBGeoPoint2D SBGeoPoint2D::operator-(SBGeoVector2D const& p1) const {
    SBGeoPoint2D r;
    r.f_0 = f_0 - p1.f_0;
    r.f_4 = f_4 - p1.f_4;
    return r;
}

// 08162c70
void SBGeoPoint2D::operator-=(SBGeoVector2D const& p1) {
    f_0 = f_0 - p1.f_0;
    f_4 = f_4 - p1.f_4;
}

// 08162ca0  the bool is not used
void SBGeoPoint2D::Rotate(SBGeoPoint2D const& p1, float p2, bool p3) {
    SBGeoVector2D v(p1, *this);
    v.Rotate(p2);
    f_0 = p1.f_0 + v.f_0;
    f_4 = p1.f_4 + v.f_4;
}

// 08162d00
SBGeoPoint2D SBGeoPoint2D::GetRotated(SBGeoPoint2D const& p1, float p2, bool p3) const {
    SBGeoVector2D v(p1, *this);
    v.Rotate(p2);
    SBGeoPoint2D r;
    r.f_0 = p1.f_0 + v.f_0;
    r.f_4 = p1.f_4 + v.f_4;
    return r;
}

// 08162d70
void SBGeoPoint2D::Floor() {
    f_0 = floorf(f_0);
    f_4 = floorf(f_4);
}

// 08162da0
SBGeoPoint2D SBGeoPoint2D::GetFloored() {
    float y = f_4;
    float fx = floorf(f_0);
    float fy = floorf(y);
    return SBGeoPoint2D(fx, fy);
}

// 08162df0
int32_t SBGeoPoint2D::Serialize(SBFile& p1) {
    int line;
    const char* reason;
    if (p1.Serialize(this, 4) < 0) {
        reason = p1.TranslateLastError().c_str();
        line = 0x109;
    } else {
        if (p1.Serialize(&f_4, 4) >= 0) return 1;
        reason = p1.TranslateLastError().c_str();
        line = 0x10a;
    }
    char msg[512];
    snprintf(msg, sizeof(msg), "Serialize failed reason is %s", reason);
    SBError(false, THIS_FILE, line, msg);
    return 0;
}

// 08162e90
void SBGeoPoint2D::Discretize() {
    f_0 = floorf(f_0);
    f_4 = floorf(f_4);
}

// 08162ec0
SBGeoPoint2D SBGeoPoint2D::GetDiscretized() {
    SBGeoPoint2D r;
    r.f_0 = floorf(f_0);
    r.f_4 = floorf(f_4);
    return r;
}

// 08162f60
SBGeoPoint2D SBGeoPoint2D::operator/(float p1) const {
    SBGeoPoint2D r;
    r.f_0 = f_0 / p1;
    r.f_4 = f_4 / p1;
    return r;
}

// 08162f90  (sign flip by xor, as the original)
SBGeoPoint2D SBGeoPoint2D::operator-() const {
    SBGeoPoint2D r;
    r.f_0 = -f_0;
    r.f_4 = -f_4;
    return r;
}
