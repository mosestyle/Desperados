#include "SpriteFile.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>

#include "../core/FileSystem.h"
#include "ByteReader.h"

static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

static std::string trim(std::string s) {
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

const SpriteRecord* SpriteSet::find(int anim, int dir) const {
    const SpriteRecord* anyDir = nullptr;
    for (const auto& r : records) {
        if (r.anim != anim) continue;
        if (r.dir == dir) return &r;
        if (!anyDir || r.dir == 0) anyDir = &r;
    }
    if (anyDir) return anyDir;
    return records.empty() ? nullptr : &records.front();
}

bool SpriteFile::load(const std::string& path) {
    std::vector<uint8_t> bytes;
    if (!fs_::readFile(path, bytes)) return false;
    if (!parse(std::move(bytes))) {
        SDL_Log("Bad sprite file: %s", path.c_str());
        return false;
    }
    return true;
}

bool SpriteFile::parse(std::vector<uint8_t> bytes) {
    data_ = std::move(bytes);
    frames_.clear();
    sets_.clear();
    ByteReader r(data_.data(), data_.size());
    r.u16();  // version
    const uint16_t frameCount = r.u16();
    r.pos = 0x1E;
    frames_.reserve(frameCount);
    for (int i = 0; i < frameCount && r.ok; ++i) {
        uint32_t size = r.u32();
        Frame f;
        f.w = r.u16();
        f.h = r.u16();
        r.u16();
        f.offset = (uint32_t)r.pos;
        frames_.push_back(f);
        r.skip(size);
    }
    const uint16_t setCount = r.u16();
    for (int s = 0; s < setCount && r.ok; ++s) {
        SpriteSet set;
        set.name = trim(r.str(32));
        set.dirCount = std::max<int>(1, r.u16());
        r.skip(32);
        set.animCount = (int)r.u32();
        r.skip(14);
        r.u16();  // maxW
        r.u16();  // maxH
        set.anchorX = (int)r.u32();
        set.anchorY = (int)r.u32();
        r.skip(20);
        const int recordCount = set.animCount * set.dirCount;
        if (recordCount < 0 || recordCount > 100000) return false;
        set.records.reserve(recordCount);
        for (int i = 0; i < recordCount && r.ok; ++i) {
            SpriteRecord rec;
            r.u32();
            uint16_t n = r.u16();
            rec.keyEntry = r.u16();
            rec.flags = r.u16();
            rec.anchorX = (int32_t)r.u32();
            rec.anchorY = (int32_t)r.u32();
            rec.dir = r.u16();
            rec.anim = r.u16();
            rec.name = trim(r.str(32));
            rec.entries.resize(n);
            for (auto& e : rec.entries) {
                e.frame = r.u16();
                e.duration = r.u16();
                e.step = r.s16();
                e.x = r.s16();
                e.y = r.s16();
                e.event = r.u32();
            }
            set.records.push_back(std::move(rec));
        }
        sets_.push_back(std::move(set));
    }
    return r.ok && !frames_.empty();
}

const SpriteSet* SpriteFile::set(const std::string& name) const {
    for (const auto& s : sets_)
        if (iequals(s.name, name)) return &s;
    return nullptr;
}

bool SpriteFile::decode(int index, std::vector<uint32_t>& argb) const {
    if (index < 0 || index >= (int)frames_.size()) return false;
    const Frame& f = frames_[index];
    argb.assign((size_t)f.w * f.h, 0);
    ByteReader r(data_.data(), data_.size());
    r.pos = f.offset;
    for (int y = 0; y < f.h; ++y) {
        int x0 = r.s16(), x1 = r.s16();
        int n = x1 - x0 + 1;
        if (!r.ok) return false;
        if (n <= 0) continue;
        if (x0 < 0 || x1 >= f.w || !r.need((size_t)n * 2)) return false;
        uint32_t* out = argb.data() + (size_t)y * f.w + x0;
        const uint8_t* p = r.here();
        for (int i = 0; i < n; ++i) {
            uint16_t px = (uint16_t)(p[2 * i] | (p[2 * i + 1] << 8));
            if (px == 0x001F) { out[i] = 0; continue; }                 // transparent
            if (px == 0x07C0) { out[i] = 0x6E000000u; continue; }       // shadow: translucent black
            uint32_t R = ((px >> 11) & 31) * 255 / 31, G = ((px >> 5) & 63) * 255 / 63, B = (px & 31) * 255 / 31;
            out[i] = 0xFF000000u | (R << 16) | (G << 8) | B;
        }
        r.skip((size_t)n * 2);
    }
    return true;
}
