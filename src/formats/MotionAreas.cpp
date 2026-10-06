#include "MotionAreas.h"

#include "ByteReader.h"

bool pointInPolygon(float x, float y, const std::vector<SDL_Point>& poly) {
    bool in = false;
    const size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const SDL_Point& a = poly[i];
        const SDL_Point& b = poly[j];
        if ((a.y > y) != (b.y > y) && x < a.x + (y - a.y) * (float)(b.x - a.x) / (float)(b.y - a.y)) in = !in;
    }
    return in;
}

bool MotionLayer::walkable(float x, float y) const {
    for (const auto& a : areas) {
        if (!pointInPolygon(x, y, a.outline)) continue;
        bool inHole = false;
        for (const auto& h : a.holes)
            if (pointInPolygon(x, y, h)) { inHole = true; break; }
        if (!inHole) return true;
    }
    return false;
}

static void readPolygon(ByteReader& r, std::vector<SDL_Point>& pts) {
    int n = r.u16();
    if (!r.need((size_t)n * 4)) return;
    pts.resize(n);
    for (auto& p : pts) { p.x = r.s16(); p.y = r.s16(); }
}

bool parseMotionAreas(const uint8_t* data, size_t size, std::vector<MotionLayer>& layers) {
    ByteReader r(data, size);
    if (r.u32() != 1) return false;
    int layerCount = r.u16();
    for (int l = 0; l < layerCount && r.ok; ++l) {
        MotionLayer layer;
        r.u16();  // total polygon count
        int areaCount = r.u16();
        for (int a = 0; a < areaCount && r.ok; ++a) {
            MotionArea area;
            readPolygon(r, area.outline);
            int lines = r.u16();
            for (int i = 0; i < lines && r.ok; ++i) {
                MotionArea::Line ln;
                ln.x0 = r.s16(); ln.y0 = r.s16(); ln.x1 = r.s16(); ln.y1 = r.s16();
                area.lines.push_back(ln);
            }
            int holes = r.u16();
            area.holes.resize(holes);
            for (auto& h : area.holes) readPolygon(r, h);
            layer.areas.push_back(std::move(area));
        }
        layers.push_back(std::move(layer));
    }
    return r.ok;
}
