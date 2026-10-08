#include "dv/DVMask.h"

#include "dv/DVFastFindGrid.h"
#include "sb/SBFile.h"

// DVMask::LoadFromFile (0x082f6d90)
int DVMask::LoadFromFile(SBFile& f, DVFastFindGrid* grid) {
    type = f.U8();
    int n = 1;
    if (type & 1) {
        uint16_t count = f.U16();
        n = 3;
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t x = f.U16(), y = f.U16();
            lineCharacter.pts.push_back(SBGeoPoint2D((float)x, (float)y));
            float fy = (float)y;
            if (i == 0 || maxY < fy) maxY = fy;
            n += 4;
        }
    }
    if (type & 2) {
        uint16_t count = f.U16();
        n += 2;
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t x = f.U16(), y = f.U16();
            lineProjectile.pts.push_back(SBGeoPoint2D((float)x, (float)y));
            float fy = (float)y;
            if (i == 0 || maxY < fy) maxY = fy;
            n += 4;
        }
        if (type & 0x10) {
            uint16_t idx = f.U16();
            n += 2;
            obstacle = grid ? grid->SightObstacle(idx) : nullptr;
        }
    }
    uint16_t x = f.U16(), y = f.U16();
    box.p0 = SBGeoPoint2D((float)x, (float)y);
    box.valid = true;
    uint16_t w = f.U16(), h = f.U16();
    box.p1 = box.p0 + SBGeoVector2D((float)w, (float)h);
    dataSize = f.U16();
    data.resize(dataSize);
    if (dataSize) f.Serialize(data.data(), dataSize);
    return n + 10 + dataSize;
}

static float evalY(const SBGeoPoint2D& a, const SBGeoPoint2D& b, float x) {
    float dy = b.y - a.y;
    if (dy != 0.0f) return a.y + ((x - a.x) * dy) / (b.x - a.x);
    return a.y;
}

// DVMask::IsAppliedToPointCharacter (0x082f7460): the point is above the base line
bool DVMask::IsAppliedToPointCharacter(const SBGeoPoint2D& p) const {
    const auto& L = lineCharacter.pts;
    if (L.empty()) return false;
    if (!(L[0].x <= p.x)) return false;
    if (!(p.x <= L.back().x)) return false;
    size_t i = 1;
    while (i < L.size() && !(p.x <= L[i].x)) ++i;
    if (i >= L.size()) i = L.size() - 1;
    float y = evalY(L[i - 1], L[i], p.x);
    return p.y < y;
}

// DVMask::IsAppliedToBox (0x082f71b0)
bool DVMask::IsAppliedToBox(const SBGeoBoundingBox2D& b) const {
    const auto& L = lineCharacter.pts;
    if (L.empty()) return false;
    SBGeoPoint2D first = L[0], last = L.back();
    if (!(first.x <= b.p0.x) && !(b.p1.x <= last.x)) return b.p0.y < maxY;
    bool startIn = first.x <= b.p0.x && b.p0.x <= last.x;
    if (first.x <= b.p1.x) {
        if (!startIn && last.x < b.p1.x) return false;
    } else if (!startIn) {
        return false;
    }
    bool below = false, inside = false;
    for (const SBGeoPoint2D& q : L) {
        if (q.x < b.p0.x || b.p1.x < q.x) {
            if (b.p0.y < q.y) below = true;
        } else {
            inside = true;
            if (b.p0.y < q.y) return true;
        }
    }
    if (!inside && below) return true;
    if (IsAppliedToPointCharacter(b.p0)) return true;
    return IsAppliedToPointCharacter(b.p0 + SBGeoVector2D(b.p1.x - b.p0.x, 0));
}
