// Little-endian reader over a byte buffer with bounds checking.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>

struct ByteReader {
    const uint8_t* p = nullptr;
    size_t size = 0;
    size_t pos = 0;
    bool ok = true;

    ByteReader() = default;
    ByteReader(const uint8_t* data, size_t n) : p(data), size(n) {}

    size_t remaining() const { return pos <= size ? size - pos : 0; }
    bool need(size_t n) { if (remaining() < n) { ok = false; return false; } return true; }

    uint8_t u8() { if (!need(1)) return 0; return p[pos++]; }
    uint16_t u16() { if (!need(2)) return 0; uint16_t v = (uint16_t)(p[pos] | (p[pos + 1] << 8)); pos += 2; return v; }
    int16_t s16() { return (int16_t)u16(); }
    uint32_t u32() {
        if (!need(4)) return 0;
        uint32_t v = (uint32_t)p[pos] | ((uint32_t)p[pos + 1] << 8) | ((uint32_t)p[pos + 2] << 16) | ((uint32_t)p[pos + 3] << 24);
        pos += 4;
        return v;
    }
    int32_t s32() { return (int32_t)u32(); }
    float f32() { uint32_t v = u32(); float f; std::memcpy(&f, &v, 4); return f; }
    std::string str(size_t n) {
        if (!need(n)) return {};
        std::string s((const char*)p + pos, n);
        pos += n;
        size_t z = s.find('\0');
        if (z != std::string::npos) s.resize(z);
        return s;
    }
    // u16 length followed by that many characters
    std::string pstr16() { uint16_t n = u16(); return str(n); }
    const uint8_t* here() const { return p + pos; }
    void skip(size_t n) { if (need(n)) pos += n; }
};
