#include "Buildings.h"

#include "ByteReader.h"

namespace {
bool readDoor(ByteReader& r, Door& d) {
    d.type = r.u8();
    d.known = r.u8() != 0;
    d.lockPC = r.u8() != 0;
    d.pickable = r.u8() != 0;
    d.lockVillain = r.u8() != 0;
    d.lockCivilian = r.u8() != 0;
    r.u32();  // colour
    int n = r.u16();
    if (n > 256) return false;
    d.outline.resize(n);
    for (auto& p : d.outline) { p.x = r.s16(); p.y = r.s16(); }
    int k = r.u16();
    if (k > 16) return false;
    for (int i = 0; i < k; ++i) {
        float x = r.s16(), y = r.s16();
        int s = r.u16(), l = r.u16();
        if (i == 0) { d.out = {x, y}; d.outSector = s; d.outLayer = l; }
        else if (i == 1) { d.mid = {x, y}; d.midSector = s; d.midLayer = l; }
        else if (i == 2) { d.in = {x, y}; d.inSector = s; d.inLayer = l; }
    }
    d.elem = r.s16();
    if (d.elem != -1) { r.u8(); r.u8(); }
    return r.ok;
}
}  // namespace

bool parseBuildings(const uint8_t* data, size_t size, std::vector<Building>& buildings, std::vector<Door>& doors) {
    ByteReader r(data, size);
    if (r.u32() != 4) return false;
    int nb = r.u16();
    for (int b = 0; b < nb && r.ok; ++b) {
        Building B;
        r.u16();
        int ns = r.u16();
        for (int i = 0; i < ns; ++i) B.sectors.push_back(r.u16());
        int nd = r.u16();
        for (int i = 0; i < nd && r.ok; ++i) {
            Door d;
            if (!readDoor(r, d)) return false;
            d.building = b;
            B.doors.push_back((int)doors.size());
            doors.push_back(std::move(d));
        }
        buildings.push_back(std::move(B));
    }
    int nd = r.u16();
    for (int i = 0; i < nd && r.ok; ++i) {
        Door d;
        if (!readDoor(r, d)) return false;
        doors.push_back(std::move(d));
    }
    return r.ok;
}
