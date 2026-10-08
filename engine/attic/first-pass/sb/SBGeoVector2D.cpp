// SBGeoVector2D (see SBGeoVector2D.h)
#include "sb/SBGeoVector2D.h"

#ifdef __clang__
#pragma clang fp contract(off)
#endif

// 0855753c: sin(i * 22.5 degrees)
const float SBGeoVector2D::marraySectorX[16] = {
    0.0f, 0.38268343f, 0.70710677f, 0.92387950f, 1.0f, 0.92387950f, 0.70710677f, 0.38268343f,
    0.0f, -0.38268343f, -0.70710677f, -0.92387950f, -1.0f, -0.92387950f, -0.70710677f, -0.38268343f};
// 0855757c: -cos(i * 22.5 degrees)
const float SBGeoVector2D::marraySectorY[16] = {
    -1.0f, -0.92387950f, -0.70710677f, -0.38268343f, 0.0f, 0.38268343f, 0.70710677f, 0.92387950f,
    1.0f, 0.92387950f, 0.70710677f, 0.38268343f, 0.0f, -0.38268343f, -0.70710677f, -0.92387950f};

// 0817c390
void SBGeoVector2D::Copy(SBGeoPoint2D const& p1) {
    f_0 = p1.f_0;
    f_4 = p1.f_4;
}

// 0817c3b0
void SBGeoVector2D::Destroy() {}

// 0817c3c0
SBGeoVector2D::SBGeoVector2D() {
    f_0 = 0.0f;
    f_4 = 0.0f;
}

// 0817c3d0
SBGeoVector2D::SBGeoVector2D(SBGeoVector2D const& p1) : SBGeoPoint2D() {
    f_0 = p1.f_0;
    f_4 = p1.f_4;
}

// 0817c400
SBGeoVector2D::SBGeoVector2D(float p1, float p2) {
    f_0 = p1;
    f_4 = p2;
}

// 0817c410
SBGeoVector2D::SBGeoVector2D(SBGeoPoint2D const& p1) : SBGeoPoint2D() {
    f_0 = p1.f_0;
    f_4 = p1.f_4;
}

// 0817c440  the vector from p1 to p2
SBGeoVector2D::SBGeoVector2D(SBGeoPoint2D const& p1, SBGeoPoint2D const& p2)
    : SBGeoPoint2D(p2.f_0 - p1.f_0, p2.f_4 - p1.f_4) {}

// 0817c480
void SBGeoVector2D::operator=(SBGeoVector2D const& p1) {
    if (this != &p1) {
        f_0 = p1.f_0;
        f_4 = p1.f_4;
    }
}

// 0817c4a0
SBGeoVector2D SBGeoVector2D::operator-() const {
    SBGeoVector2D r;
    r.f_0 = -f_0;
    r.f_4 = -f_4;
    return r;
}

// 0817c4e0
void SBGeoVector2D::operator*=(float p1) {
    f_0 = f_0 * p1;
    f_4 = p1 * f_4;
}

// 0817c590
void SBGeoVector2D::operator/=(float p1) {
    f_0 = f_0 / p1;
    f_4 = f_4 / p1;
}

// 0817c5c0
SBGeoVector2D SBGeoVector2D::operator/(float p1) const {
    SBGeoVector2D r;
    r.f_0 = f_0 / p1;
    r.f_4 = f_4 / p1;
    return r;
}

// 0817c600  dot product
float SBGeoVector2D::operator*(SBGeoVector2D const& p1) const {
    return p1.f_4 * f_4 + p1.f_0 * f_0;
}

// 0817c630  cross product (z of this x p1)
float SBGeoVector2D::Det(SBGeoVector2D const& p1) const {
    return p1.f_4 * f_0 - p1.f_0 * f_4;
}

// 0817c670
float SBGeoVector2D::Norm() const {
    return sqrtf(f_4 * f_4 + f_0 * f_0);
}

// 0817c6c0
float SBGeoVector2D::SquareNorm() const {
    return f_4 * f_4 + f_0 * f_0;
}

// 0817c6f0
float SBGeoVector2D::MaxNorm() const {
    float x = f_0;
    float y = f_4;
    float r;
    if (x <= 0.0f) {
        r = -x;
        if (y <= 0.0f) {
            if (y <= x) r = -y;
        } else if (r <= y) {
            r = y;
        }
    } else {
        if (y <= 0.0f) y = -y;
        r = x;
        if (x <= y) r = y;
    }
    return r;
}

// 0817c750
float SBGeoVector2D::OneNorm() const {
    float x = f_0;
    float y = f_4;
    if (x <= 0.0f) {
        if (0.0f < y) return y - x;
        x = -x;
    } else if (0.0f < y) {
        return x + y;
    }
    return x - y;
}

// 0817c7a0
void SBGeoVector2D::Normalize() {
    float n = sqrtf(f_4 * f_4 + f_0 * f_0);
    f_0 = f_0 / n;
    f_4 = f_4 / n;
}

// 0817c800
SBGeoVector2D SBGeoVector2D::GetNormalized() const {
    SBGeoVector2D r;
    float n = sqrtf(f_4 * f_4 + f_0 * f_0);
    r.f_0 = f_0 / n;
    r.f_4 = f_4 / n;
    return r;
}

// 0817c870  rotation by +90 degrees when p1, -90 otherwise
void SBGeoVector2D::Normal(bool p1) {
    float x = f_0;
    if (p1) {
        f_0 = -f_4;
        f_4 = x;
        return;
    }
    f_0 = f_4;
    f_4 = -x;
}

// 0817c8b0
SBGeoVector2D SBGeoVector2D::GetNormal(bool p1) const {
    SBGeoVector2D r;
    float x = f_4;
    if (!p1) {
        r.f_4 = -f_0;
    } else {
        x = -x;
        r.f_4 = f_0;
    }
    r.f_0 = x;
    return r;
}

// 0817c900  (double precision trigonometry, as the original)
void SBGeoVector2D::Rotate(float p1) {
    float x = f_0;
    double s = sin((double)p1);
    double c = cos((double)p1);
    f_0 = (float)((double)f_0 * c - s * (double)f_4);
    f_4 = (float)(c * (double)f_4 + (double)x * s);
}

// 0817c9b0
SBGeoVector2D SBGeoVector2D::GetRotated(float p1) const {
    SBGeoVector2D r;
    double a = (double)p1;
    double xc = (double)f_0 * cos(a);
    r.f_0 = (float)(xc - (double)f_4 * sin(a));
    double xs = (double)f_0 * sin(a);
    r.f_4 = (float)((double)f_4 * cos(a) + xs);
    return r;
}

// 0817cac0  angle from this to p1 (radians)
float SBGeoVector2D::Angle(SBGeoVector2D const& p1) const {
    SBGeoPoint2D normal;
    {
        SBGeoPoint2D tmp;
        tmp.f_0 = -f_4;
        tmp.f_4 = f_0;
        normal.f_0 = tmp.f_0;
        normal.f_4 = tmp.f_4;
    }
    float dot = f_4 * p1.f_4 + f_0 * p1.f_0;
    float cross = normal.f_4 * p1.f_4 + normal.f_0 * p1.f_0;
    float r = 0.0f;
    if (cross != 0.0f) {
        static const float table[2] = {1.5707964f, -1.5707964f};  // 08557524
        float a = (float)atan((double)(dot / cross));
        r = table[0.0f < cross ? 1 : 0] + a;
    } else if (dot <= 0.0f) {
        r = dot < 0.0f ? 1.5707964f : 0.0f;  // (sic) pi/2 for opposite vectors
    }
    return r;
}

// 0817cbd0  which of the 16 sectors (22.5 degrees each) the vector points to; p1 scales x
uint8_t SBGeoVector2D::GetSector0to15(float p1) const {
    // rotated by 11.25 degrees so that the sector boundaries fall on the axes
    float a = f_0 * 0.98078525f * p1 - f_4 * 0.19509032f;
    float b = f_4 * 0.98078525f + f_0 * 0.19509032f * p1;
    bool neg = a < 0.0f;
    if (a < 0.0f) a = -a;
    bool pos = 0.0f < b;
    if (!(0.0f < b)) b = -b;
    bool half;
    bool aBig = b < a;
    if (aBig)
        half = a * 0.41421357f < b;
    else
        half = b * 0.41421357f < a;
    uint8_t r = (uint8_t)(neg << 3);
    bool bit = pos != neg;
    if (bit) r |= 4;
    bit = bit != aBig;
    if (bit) r |= 2;
    return (uint8_t)((bit != half) | r);
}

// 0817cc80  (sic) only x is scaled by p2
void SBGeoVector2D::SetSector0to15(unsigned char p1, float p2) {
    f_0 = marraySectorX[p1 & 0xf] * p2;
    f_4 = marraySectorY[p1 & 0xf];
}
