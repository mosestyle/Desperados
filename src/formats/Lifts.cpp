#include "Lifts.h"

#include "ByteReader.h"

bool parseLifts(const uint8_t* data, size_t size, std::vector<Lift>& out) {
    ByteReader r(data, size);
    if (r.u32() != 2) return false;
    int n = r.u16();
    for (int i = 0; i < n && r.ok; ++i) {
        Lift l;
        l.sector = r.u16();
        l.type = r.u8();
        l.sectorA = r.u16();
        l.layerA = r.u16();
        l.sectorB = r.u16();
        l.layerB = r.u16();
        int k = r.u16();
        if (k > 1000) return false;
        l.outline.resize(k);
        for (auto& p : l.outline) { p.x = r.s16(); p.y = r.s16(); }
        for (int j = 0; j < 3; ++j) { l.a[j].x = r.s16(); l.a[j].y = r.s16(); }
        for (int j = 0; j < 3; ++j) { l.b[j].x = r.s16(); l.b[j].y = r.s16(); }
        l.dir = r.u16() & 15;
        if (r.ok) out.push_back(std::move(l));
    }
    return r.ok;
}
