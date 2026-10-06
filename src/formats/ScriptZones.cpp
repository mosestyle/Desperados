#include "ScriptZones.h"

#include "ByteReader.h"
#include "MotionAreas.h"

bool ScriptZone::contains(float x, float y) const {
    return pts.size() >= 3 && pointInPolygon(x, y, pts);
}

SDL_FPoint ScriptZone::centre() const {
    if (pts.empty()) return {0, 0};
    float x = 0, y = 0;
    for (const auto& p : pts) { x += p.x; y += p.y; }
    return {x / pts.size(), y / pts.size()};
}

bool parseScriptZones(const uint8_t* data, size_t size, std::vector<ScriptZone>& out) {
    ByteReader r(data, size);
    if (r.u32() != 1) return false;
    int n = r.u16();
    out.resize(n);
    for (auto& z : out) {
        int k = r.u16();
        if (!r.ok || k > 4096) return false;
        z.pts.resize(k);
        for (auto& p : z.pts) { p.x = r.u16(); p.y = r.u16(); }
        z.flag = r.u16();
        z.layer = r.u16();
        if (r.u8()) z.script = r.pstr16();
    }
    return r.ok;
}
