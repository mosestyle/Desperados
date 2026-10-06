#include "Masks.h"

#include <algorithm>

#include "ByteReader.h"

float Mask::lineY(float x) const {
    if (line.empty()) return 1e9f;
    if (x <= line.front().x) return (float)line.front().y;
    for (size_t i = 1; i < line.size(); ++i) {
        const SDL_Point a = line[i - 1], b = line[i];
        if (x <= b.x) {
            float t = b.x == a.x ? 0.0f : (x - a.x) / (float)(b.x - a.x);
            return a.y + (b.y - a.y) * t;
        }
    }
    return (float)line.back().y;
}

static void readLine(ByteReader& r, std::vector<SDL_Point>& pts) {
    int n = r.u16();
    pts.resize(n);
    for (auto& p : pts) { p.x = r.s16(); p.y = r.s16(); }
}

bool parseMasks(const uint8_t* data, size_t size, std::vector<Mask>& out) {
    ByteReader r(data, size);
    r.u32();  // version
    int groups = r.u16();
    for (int g = 0; g < groups && r.ok; ++g) {
        int count = r.u16();
        for (int i = 0; i < count && r.ok; ++i) {
            Mask m;
            m.group = g;
            m.flags = r.u8();
            if (m.flags & 1) readLine(r, m.line);
            if (m.flags & 2) readLine(r, m.line2);
            if (m.flags & 0x10) m.height = r.s16();
            m.rect.x = r.s16();
            m.rect.y = r.s16();
            m.rect.w = r.s16();
            m.rect.h = r.s16();
            size_t dataSize = r.u16();
            if (!r.need(dataSize) || m.rect.w <= 0 || m.rect.h <= 0) return false;
            const uint8_t* d = r.here();
            const size_t end = r.pos + dataSize;
            m.bits.assign((size_t)m.rect.w * m.rect.h, 0);
            size_t p = 0;
            const int stride = (m.rect.w + 7) / 8;
            std::vector<uint8_t> row;
            for (int y = 0; y < m.rect.h && p < dataSize; ++y) {
                size_t n = d[p++];
                size_t rowEnd = p + n;
                row.clear();
                while (p < rowEnd && p < dataSize) {
                    uint8_t c = d[p++];
                    if (c >= 0x80) { if (p < dataSize) row.insert(row.end(), c - 0x80, d[p++]); }
                    else { size_t k = std::min<size_t>(c, dataSize - p); row.insert(row.end(), d + p, d + p + k); p += k; }
                }
                for (int x = 0; x < m.rect.w && x / 8 < (int)row.size() && x / 8 < stride; ++x)
                    m.bits[(size_t)y * m.rect.w + x] = (row[x / 8] >> (7 - (x & 7))) & 1;
            }
            r.pos = end;
            out.push_back(std::move(m));
        }
    }
    return r.ok;
}
