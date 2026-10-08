// SBDisVector2D (see SBDisVector2D.h)
#include "sb/SBDisVector2D.h"
#include "sb/SBGeoVector2D.h"

// 081476b0  (leaves the coordinates uninitialized)
SBDisVector2D::SBDisVector2D() {}

// 081476c0
SBDisVector2D::SBDisVector2D(SBDisVector2D const& p1) {
    f_0 = p1.f_0;
    f_4 = p1.f_4;
}

// 081476e0
SBDisVector2D::SBDisVector2D(int p1, int p2) {
    f_0 = p1;
    f_4 = p2;
}

// 08147700  truncation toward zero
SBDisVector2D::SBDisVector2D(SBGeoVector2D const& p1) {
    f_0 = (int32_t)p1.f_0;
    f_4 = (int32_t)p1.f_4;
}
