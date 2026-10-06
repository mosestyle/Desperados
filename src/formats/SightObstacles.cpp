#include "SightObstacles.h"

#include <algorithm>
#include <cmath>

#include "ByteReader.h"

static bool segmentsCross(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
    auto orient = [](float px, float py, float qx, float qy, float rx, float ry) {
        return (qx - px) * (ry - py) - (qy - py) * (rx - px);
    };
    float o1 = orient(ax, ay, bx, by, cx, cy), o2 = orient(ax, ay, bx, by, dx, dy);
    float o3 = orient(cx, cy, dx, dy, ax, ay), o4 = orient(cx, cy, dx, dy, bx, by);
    return ((o1 > 0) != (o2 > 0)) && ((o3 > 0) != (o4 > 0));
}

bool SightObstacle::crosses(float ax, float ay, float bx, float by) const {
    if (std::max(ax, bx) < minX || std::min(ax, bx) > maxX || std::max(ay, by) < minY || std::min(ay, by) > maxY)
        return false;
    const size_t n = xs.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++)
        if (segmentsCross(ax, ay, bx, by, xs[j], ys[j], xs[i], ys[i])) return true;
    return false;
}

float SightObstacle::raycast(float ax, float ay, float bx, float by) const {
    if (std::max(ax, bx) < minX || std::min(ax, bx) > maxX || std::max(ay, by) < minY || std::min(ay, by) > maxY)
        return 2.0f;
    float best = 2.0f;
    const float rx = bx - ax, ry = by - ay;
    const size_t n = xs.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float sx = xs[i] - xs[j], sy = ys[i] - ys[j];
        float den = rx * sy - ry * sx;
        if (std::fabs(den) < 1e-6f) continue;
        float qx = xs[j] - ax, qy = ys[j] - ay;
        float t = (qx * sy - qy * sx) / den, u = (qx * ry - qy * rx) / den;
        if (t >= 0 && t <= 1 && u >= 0 && u <= 1 && t < best) best = t;
    }
    return best;
}

bool parseSightObstacles(const uint8_t* data, size_t size, std::vector<SightObstacle>& out) {
    ByteReader r(data, size);
    r.u32();
    int count = r.u16();
    for (int i = 0; i < count && r.ok; ++i) {
        SightObstacle o;
        int n = r.u16();
        if (!r.need((size_t)n * 16)) return false;
        for (int k = 0; k < n; ++k) {
            float x = r.f32(), y = r.f32();
            r.f32();
            float top = r.f32();
            o.xs.push_back(x);
            o.ys.push_back(y);
            o.height = std::max(o.height, top);
        }
        o.minX = r.f32(); r.f32(); o.minY = r.f32();
        o.maxX = r.f32(); r.f32(); o.maxY = r.f32();
        if (r.u8()) r.skip(4);
        r.skip(4 + 8 + 1 + 4);
        if (n >= 2) out.push_back(std::move(o));
    }
    return r.ok;
}
