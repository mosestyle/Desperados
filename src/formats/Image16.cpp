#include "Image16.h"

#include <SDL.h>

#include <cstring>

#include "../core/FileSystem.h"
#include "ByteReader.h"
#include "Decompress.h"

bool parseImageContainer(const uint8_t* data, size_t size, std::vector<Image16>& out) {
    ByteReader r(data, size);
    while (r.remaining() >= 12) {
        Image16 img;
        img.w = r.u16();
        img.h = r.u16();
        Codec codec = (Codec)r.u32();
        uint32_t csize = r.u32();
        if (img.w == 0 || img.h == 0 || csize > r.remaining()) {
            SDL_Log("Image container: bad header (%dx%d, %u bytes)", img.w, img.h, csize);
            return !out.empty();
        }
        std::vector<uint8_t> raw;
        size_t used = 0;
        const size_t expected = (size_t)img.w * img.h * 2;
        if (!decompress(codec, r.here(), r.remaining(), expected, raw, &used) || raw.size() < expected) {
            SDL_Log("Image container: decompression failed");
            return !out.empty();
        }
        img.px.resize((size_t)img.w * img.h);
        std::memcpy(img.px.data(), raw.data(), expected);  // file data is little endian, as are all targets
        out.push_back(std::move(img));
        r.skip(used);
    }
    return !out.empty();
}

bool loadImageContainer(const std::string& relativePath, std::vector<Image16>& out) {
    std::vector<uint8_t> bytes;
    if (!fs_::readData(relativePath, bytes)) return false;
    return parseImageContainer(bytes.data(), bytes.size(), out);
}
