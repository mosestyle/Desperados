// SBDisBox2D (see SBDisBox2D.h)
#include "sb/SBDisBox2D.h"
#include "sb/SBGeoBoundingBox2D.h"

// 081474b0  empty box
SBDisBox2D::SBDisBox2D() {
    f_0 = 0x7fff;
    f_4 = -0x7fff;
    f_8 = 0x7fff;
    f_c = -0x7fff;
}

// 081474c0  (float to int truncation)
SBDisBox2D::SBDisBox2D(SBGeoBoundingBox2D const& p1) {
    f_0 = (int32_t)p1.f_0.f_0;
    f_8 = (int32_t)p1.f_0.f_4;
    f_4 = (int32_t)p1.f_8.f_0 + 1;
    f_c = (int32_t)p1.f_8.f_4 + 1;
}

// 081474f0
SBDisBox2D::SBDisBox2D(SBDisBox2D const& p1) {
    f_0 = p1.f_0;
    f_4 = p1.f_4;
    f_8 = p1.f_8;
    f_c = p1.f_c;
}

// 08147500  (min x, min y, max x, max y)
SBDisBox2D::SBDisBox2D(int p1, int p2, int p3, int p4) {
    f_0 = p1;
    f_8 = p2;
    f_4 = p3;
    f_c = p4;
}

// 08147530
void SBDisBox2D::Expand(int p1, int p2) {
    if (p1 < f_0) f_0 = p1;
    if (p2 < f_8) f_8 = p2;
    if (f_4 <= p1) f_4 = p1 + 1;
    if (f_c <= p2) f_c = p2 + 1;
}

// 08147560
void SBDisBox2D::Expand(SBDisPoint2D const& p1) {
    int x = p1.f_0;
    if (x < f_0) f_0 = x;
    int y = p1.f_4;
    if (y < f_8) f_8 = y;
    if (f_4 <= x) f_4 = x + 1;
    if (f_c <= y) f_c = y + 1;
}

// 08147590
void SBDisBox2D::Expand(SBDisBox2D const& p1) {
    if (p1.f_0 < f_0) f_0 = p1.f_0;
    if (p1.f_8 < f_8) f_8 = p1.f_8;
    if (f_4 < p1.f_4) f_4 = p1.f_4;
    if (f_c < p1.f_c) f_c = p1.f_c;
}

// 081475d0  p1 = union of this and p2
void SBDisBox2D::Expand(SBDisBox2D& p1, SBDisBox2D const& p2) const {
    int v = f_0;
    if (p2.f_0 <= f_0) v = p2.f_0;
    p1.f_0 = v;
    v = f_8;
    if (p2.f_8 <= f_8) v = p2.f_8;
    p1.f_8 = v;
    v = f_4;
    if (f_4 <= p2.f_4) v = p2.f_4;
    p1.f_4 = v;
    v = f_c;
    if (f_c <= p2.f_c) v = p2.f_c;
    p1.f_c = v;
}

// 08147620
void SBDisBox2D::Clip(SBDisBox2D const& p1) {
    if (f_0 < p1.f_0) f_0 = p1.f_0;
    if (f_8 < p1.f_8) f_8 = p1.f_8;
    if (p1.f_4 < f_4) f_4 = p1.f_4;
    if (p1.f_c < f_c) f_c = p1.f_c;
}

// 08147660  p1 = intersection of this and p2
void SBDisBox2D::Clip(SBDisBox2D& p1, SBDisBox2D const& p2) const {
    int v = f_0;
    if (f_0 <= p2.f_0) v = p2.f_0;
    p1.f_0 = v;
    v = f_8;
    if (f_8 <= p2.f_8) v = p2.f_8;
    p1.f_8 = v;
    v = f_4;
    if (p2.f_4 <= f_4) v = p2.f_4;
    p1.f_4 = v;
    v = f_c;
    if (p2.f_c <= f_c) v = p2.f_c;
    p1.f_c = v;
}
