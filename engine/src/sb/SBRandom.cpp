// desprand (0x083b5610): rand() & 0x7fff. rand() is the C library's of the original (glibc's
// additive feedback generator), so the same seed gives the Linux build's sequence.
#include <cstdint>

namespace {
struct GlibcRand {
    int32_t ring[34];  // the last 34 values r[i-34 .. i-1]
    int pos = 0;       // where r[i-34] is
    bool seeded = false;

    void seed(unsigned s) {
        int32_t r[344];
        r[0] = (int32_t)s;
        for (int i = 1; i < 31; ++i) {
            int64_t hi = r[i - 1] / 127773, lo = r[i - 1] % 127773;
            int64_t w = 16807 * lo - 2836 * hi;
            if (w < 0) w += 2147483647;
            r[i] = (int32_t)w;
        }
        for (int i = 31; i < 34; ++i) r[i] = r[i - 31];
        for (int i = 34; i < 344; ++i) r[i] = (int32_t)((uint32_t)r[i - 31] + (uint32_t)r[i - 3]);
        for (int i = 0; i < 34; ++i) ring[i] = r[310 + i];
        pos = 0;
        seeded = true;
    }
    int32_t next() {
        if (!seeded) seed(1);
        // r[i] = r[i-31] + r[i-3]; ring[pos] holds r[i-34]
        int32_t v = (int32_t)((uint32_t)ring[(pos + 3) % 34] + (uint32_t)ring[(pos + 31) % 34]);
        ring[pos] = v;
        pos = (pos + 1) % 34;
        return (int32_t)((uint32_t)v >> 1);
    }
} gRand;
}  // namespace

void despsrand(unsigned seed) { gRand.seed(seed); }
int desprand() { return gRand.next() & 0x7fff; }
