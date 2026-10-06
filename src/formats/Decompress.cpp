#include "Decompress.h"

#include <SDL.h>
#include <bzlib.h>
#include <zlib.h>

// bzip2 is built with BZ_NO_STDIO, which requires the host to provide this.
extern "C" void bz_internal_error(int errcode) { SDL_Log("bzip2 internal error %d", errcode); }

static bool bzip2(const uint8_t* in, size_t inSize, size_t expected, std::vector<uint8_t>& out, size_t* consumed) {
    bz_stream s{};
    if (BZ2_bzDecompressInit(&s, 0, 0) != BZ_OK) return false;
    out.resize(expected ? expected : inSize * 4 + 1024);
    s.next_in = (char*)in;
    s.avail_in = (unsigned)inSize;
    size_t produced = 0;
    int rc;
    for (;;) {
        s.next_out = (char*)out.data() + produced;
        s.avail_out = (unsigned)(out.size() - produced);
        rc = BZ2_bzDecompress(&s);
        produced = out.size() - s.avail_out;
        if (rc == BZ_STREAM_END) break;
        if (rc != BZ_OK) { BZ2_bzDecompressEnd(&s); return false; }
        if (s.avail_out == 0) out.resize(out.size() * 2);
        else if (s.avail_in == 0) { BZ2_bzDecompressEnd(&s); return false; }
    }
    if (consumed) *consumed = inSize - s.avail_in;
    BZ2_bzDecompressEnd(&s);
    out.resize(produced);
    return true;
}

static bool zlibInflate(const uint8_t* in, size_t inSize, size_t expected, std::vector<uint8_t>& out, size_t* consumed) {
    z_stream s{};
    if (inflateInit(&s) != Z_OK) return false;
    out.resize(expected ? expected : inSize * 4 + 1024);
    s.next_in = (Bytef*)in;
    s.avail_in = (uInt)inSize;
    size_t produced = 0;
    int rc;
    for (;;) {
        s.next_out = out.data() + produced;
        s.avail_out = (uInt)(out.size() - produced);
        rc = inflate(&s, Z_NO_FLUSH);
        produced = out.size() - s.avail_out;
        if (rc == Z_STREAM_END) break;
        if (rc != Z_OK && rc != Z_BUF_ERROR) { inflateEnd(&s); return false; }
        if (s.avail_out == 0) out.resize(out.size() * 2);
        else if (s.avail_in == 0) { inflateEnd(&s); return false; }
    }
    if (consumed) *consumed = inSize - s.avail_in;
    inflateEnd(&s);
    out.resize(produced);
    return true;
}

bool decompress(Codec codec, const uint8_t* in, size_t inSize, size_t expected, std::vector<uint8_t>& out, size_t* consumed) {
    // Trust the stream signature over the declared codec.
    if (inSize >= 3 && in[0] == 'B' && in[1] == 'Z' && in[2] == 'h') return bzip2(in, inSize, expected, out, consumed);
    if (inSize >= 2 && in[0] == 0x78) return zlibInflate(in, inSize, expected, out, consumed);
    if (codec == Codec::Bzip2) return bzip2(in, inSize, expected, out, consumed);
    return zlibInflate(in, inSize, expected, out, consumed);
}
