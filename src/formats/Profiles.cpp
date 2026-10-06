#include "Profiles.h"

#include <vector>

#include "../core/FileSystem.h"
#include "ByteReader.h"

bool ProfileTable::load() {
    std::vector<uint8_t> b;
    if (!fs_::readData("data/configuration/characters.dat", b)) return false;
    ByteReader r(b.data(), b.size());
    r.u16();
    int n = r.u16();
    for (int i = 0; i < n && r.remaining() >= 41; ++i) {
        size_t start = r.pos;
        Profile p;
        p.id = r.s32();
        p.name = r.str(16);
        p.health = r.u8();
        p.courage = r.u8();
        p.intelligence = r.u8();
        p.attention = r.u8();
        r.u8();
        r.u8();
        p.marksmanship = r.u8();
        byId_[p.id] = p;
        r.pos = start + 41;
    }
    if (byId_.count(0)) fallback_ = byId_[0];  // "Default Villain"
    return !byId_.empty();
}

const Profile& ProfileTable::get(int id) const {
    auto it = byId_.find(id);
    return it != byId_.end() ? it->second : fallback_;
}
