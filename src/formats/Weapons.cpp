#include "Weapons.h"

#include <vector>

#include "../core/FileSystem.h"
#include "ByteReader.h"

bool WeaponTable::load() {
    std::vector<uint8_t> b;
    if (!fs_::readData("data/configuration/weapons.dat", b)) return false;
    ByteReader r(b.data(), b.size());
    r.u16();
    int n = r.u16();
    for (int i = 0; i < n && r.remaining() >= 51; ++i) {
        size_t start = r.pos;
        r.s32();
        Weapon w;
        w.name = r.str(16);
        w.range = r.u16();
        r.u16();
        w.ammo = r.u16();
        byName_[w.name] = w;
        r.pos = start + 51;
    }
    return !byName_.empty();
}

const Weapon* WeaponTable::find(const std::string& name) const {
    auto it = byName_.find(name);
    return it == byName_.end() ? nullptr : &it->second;
}
