#include "LevelFile.h"

#include <SDL.h>

#include "../core/FileSystem.h"
#include "ByteReader.h"
#include "Decompress.h"

static std::string trimTag(const uint8_t* p) {
    std::string t((const char*)p, 4);
    while (!t.empty() && (t.back() == ' ' || t.back() == '\0')) t.pop_back();
    return t;
}

bool LevelFile::load(const std::string& relativePath) {
    fileName = relativePath;
    chunks.clear();
    order.clear();
    if (!fs_::readData(relativePath, bytes)) return false;
    ByteReader r(bytes.data(), bytes.size());
    while (r.remaining() >= 8) {
        std::string tag = trimTag(r.here());
        r.skip(4);
        uint32_t len = r.u32();
        if (len > r.remaining()) {
            SDL_Log("%s: chunk %s overruns file", relativePath.c_str(), tag.c_str());
            return false;
        }
        chunks[tag] = Chunk{r.pos, len};
        order.push_back(tag);
        r.skip(len);
    }
    return !chunks.empty();
}

const uint8_t* LevelFile::chunkData(const std::string& tag, size_t* size) const {
    auto it = chunks.find(tag);
    if (it == chunks.end()) { if (size) *size = 0; return nullptr; }
    if (size) *size = it->second.size;
    return bytes.data() + it->second.offset;
}

bool LevelFile::readMinimap(std::string& levelName, Image16& minimap) const {
    size_t size = 0;
    const uint8_t* d = chunkData("BGND", &size);
    if (!d) return false;
    ByteReader r(d, size);
    r.u32();  // version
    levelName = r.pstr16();
    minimap.w = r.u16();
    minimap.h = r.u16();
    Codec codec = (Codec)r.u32();
    uint32_t csize = r.u32();
    if (!r.ok || csize > r.remaining() || minimap.w <= 0 || minimap.h <= 0) return false;
    std::vector<uint8_t> raw;
    const size_t expected = (size_t)minimap.w * minimap.h * 2;
    if (!decompress(codec, r.here(), csize, expected, raw) || raw.size() < expected) return false;
    minimap.px.assign((const uint16_t*)raw.data(), (const uint16_t*)raw.data() + (size_t)minimap.w * minimap.h);
    return true;
}
