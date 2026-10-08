#include "dv/DVFrameHolder.h"

#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"

DVFrameHolder* DVFrameHolder::mpFrameHolder = nullptr;

DVFrameHolder::DVFrameHolder() { mpFrameHolder = this; }
DVFrameHolder::~DVFrameHolder() {
    if (mpFrameHolder == this) mpFrameHolder = nullptr;
}

// the night darkening of DVFrameHolder::ApplyNightEffect (0x082c8630), on packed data
static void applyNight(std::vector<uint16_t>& d, unsigned percent) {
    size_t n = d.size(), i = 0;
    while (i < n) {
        size_t j = i + 2;
        if (i + 1 >= n) break;
        uint16_t x1 = d[i + 1];
        if (x1 != 0xffff) {
            for (uint16_t x = d[i]; x <= x1 && j < n; ++x, ++j) {
                uint16_t c = d[j];
                if (c != 0x1f && c != 0x7c0) {
                    unsigned b = (c & 0x1f) * percent / 100;
                    unsigned g = (unsigned)((c >> 5 & 0x3e) * percent / 100);
                    unsigned r = (unsigned)((c >> 11) * percent / 100);
                    d[j] = (uint16_t)((b & 0x1f) | ((g << 5) & 0x7c0) | ((r << 11) & 0xf800));
                }
                if (x == 0xffff) break;
            }
        }
        i = j;
    }
}

// DVFrameHolder::ApplyArnoLaw (0x082c8730): the file's shadow colour becomes the level's shadow
// key; pixels that already had the key's value move one step away
static void applyShadowKey(std::vector<uint16_t>& d, uint16_t key) {
    size_t n = d.size(), i = 0;
    while (i < n) {
        size_t j = i + 2;
        if (i + 1 >= n) break;
        uint16_t x1 = d[i + 1];
        if (x1 != 0xffff) {
            for (uint16_t x = d[i]; x <= x1 && j < n; ++x, ++j) {
                uint16_t c = d[j];
                if (c == key) {
                    c = (uint16_t)(key + 1);
                    d[j] = c;
                }
                if (c == 0x1f) d[j] = key;
                if (x == 0xffff) break;
            }
        }
        i = j;
    }
}

// DVFrameHolder::AddRowFromDVFStream (0x082ca3c0)
unsigned DVFrameHolder::AddRowFromDVFStream(SBFile& f, uint16_t count, const std::string& name, uint16_t shadowKey,
                                            bool night, uint16_t nightPercent, bool noNight) {
    auto it = files.find(name);
    bool known = it != files.end();
    unsigned first = known ? it->second : (unsigned)frames.size();
    if (!known) {
        files[name] = (uint16_t)first;
        frames.resize(first + count);
    }
    for (unsigned i = 0; i < count; ++i) {
        uint32_t size = f.U32();
        uint16_t w = f.U16(), h = f.U16();
        f.U16();
        if (known) {
            f.Skip((int)frames[first + i].size, 1);
            continue;
        }
        DVpackedSprite& s = frames[first + i];
        s.width = w;
        s.height = h;
        s.size = size;
        s.data.assign((size & ~1u) / 2, 0);
        f.Serialize(s.data.data(), (int)(s.data.size() * 2));
        if (size & 1) f.Skip(1, 1);
        if (night && !noNight) applyNight(s.data, nightPercent);
        applyShadowKey(s.data, shadowKey);
    }
    return first;
}

// The three unpackers (0x082c8830, 0x082c8970, 0x082c8ae0): rows of "x0, x1, pixels" into the
// viewport, the rest of each row in the transparent colour 0x07C0.
template <typename F>
static void unpack(const DVpackedSprite& s, SBDrawViewport& vp, F pixel) {
    uint16_t* out = vp.data;
    if (!out) return;
    const uint16_t* in = s.data.data();
    const uint16_t* end = in + s.data.size();
    unsigned pitch = vp.pitch >> 1;
    for (unsigned row = 0; row < s.height && in + 2 <= end; ++row) {
        uint16_t* o = out;
        unsigned x0 = in[0];
        uint16_t x1 = in[1];
        in += 2;
        for (unsigned x = 0; x < x0 && x < s.width; ++x) *o++ = 0x7c0;
        unsigned stop = 0;
        if ((uint16_t)(x1 + 1) != 0) {
            stop = (uint16_t)(x1 + 1);
            for (unsigned x = x0; x < stop && in < end; ++x) *o++ = pixel(*in++);
        } else {
            stop = x0 > s.width ? s.width : x0;
        }
        for (unsigned x = stop; x < s.width; ++x) *o++ = 0x7c0;
        out += pitch;
    }
}

void DVFrameHolder::UnCompressFrame(SBDrawViewport& vp, unsigned i) const {
    if (i < frames.size()) unpack(frames[i], vp, [](uint16_t c) { return c; });
}
void DVFrameHolder::UnCompressFrameWipeShadow(SBDrawViewport& vp, unsigned i) const {
    if (i < frames.size()) unpack(frames[i], vp, [](uint16_t c) { return c == 0x1f ? (uint16_t)0 : c; });
}
void DVFrameHolder::UnCompressFrameIntoTheShadow(SBDrawViewport& vp, unsigned i, uint16_t color) const {
    if (i < frames.size())
        unpack(frames[i], vp, [color](uint16_t c) { return (c == 0x7c0 || c == 0x1f) ? c : color; });
}
