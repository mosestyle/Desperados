#include "sb/SBPicture.h"

#include "sb/SBFile.h"

#include <bzlib.h>
#include <cstring>
#include <zlib.h>

bool SBUncompressZlib(const uint8_t* src, size_t n, uint8_t* dst, size_t want) {
    uLongf got = (uLongf)want;
    return uncompress(dst, &got, src, (uLong)n) == Z_OK;
}

bool SBUncompressBzip2(const uint8_t* src, size_t n, uint8_t* dst, size_t want) {
    unsigned int got = (unsigned int)want;
    return BZ2_bzBuffToBuffDecompress((char*)dst, &got, (char*)src, (unsigned int)n, 0, 0) == BZ_OK;
}

// SBPictureSixteen::LoadFromStream (0x08181ec0)
bool SBPictureSixteen::LoadFromStream(SBFile& f) {
    width = f.U16();
    height = f.U16();
    uint32_t codec = f.U32();
    uint32_t size = f.U32();
    if (codec > 2) return false;
    size_t bytes = (size_t)width * height * 2;
    pixels.assign((size_t)width * height, 0);
    if (codec == 0) {
        std::vector<uint8_t> raw(size);
        f.Serialize(raw.data(), (int)size);
        memcpy(pixels.data(), raw.data(), size < bytes ? size : bytes);
        return true;
    }
    std::vector<uint8_t> packed(size);
    f.Serialize(packed.data(), (int)size);
    if (codec == 2) SBUncompressBzip2(packed.data(), size, (uint8_t*)pixels.data(), bytes);
    else SBUncompressZlib(packed.data(), size, (uint8_t*)pixels.data(), bytes);
    return true;
}

// SBPictureSixteen::LoadFromFile (0x08181e40)
bool SBPictureSixteen::LoadFromFile(const std::string& path) {
    SBFile f;
    if (f.Open(path, 1) != 0) return false;
    bool ok = LoadFromStream(f);
    f.Close();
    return ok;
}

// bzip2 built without stdio wants this
extern "C" void bz_internal_error(int code) { SBError(false, "bzlib", 0, "bzip2 internal error %d", code); }
