#include "TextResource.h"

#include "../core/FileSystem.h"
#include "ByteReader.h"

static void appendUtf8(std::string& s, uint32_t c) {
    if (c < 0x80) s += (char)c;
    else if (c < 0x800) { s += (char)(0xC0 | (c >> 6)); s += (char)(0x80 | (c & 63)); }
    else { s += (char)(0xE0 | (c >> 12)); s += (char)(0x80 | ((c >> 6) & 63)); s += (char)(0x80 | (c & 63)); }
}

bool TextResource::load(const std::string& relativePath) {
    std::vector<uint8_t> buf;
    if (!fs_::readData(relativePath, buf) || buf.size() < 12 || std::string((const char*)buf.data(), 4) != "SRES")
        return false;
    ByteReader r(buf.data(), buf.size());
    r.skip(8);
    uint32_t count = r.u32();
    for (uint32_t i = 0; i < count && r.ok; ++i) {
        std::string tag = r.str(4);
        uint32_t id = r.u32();
        r.u32();
        int n = r.u16();
        std::vector<std::string> entries;
        for (int k = 0; k < n && r.ok; ++k) {
            int len = r.u16();
            std::string s;
            if (tag == "TEXT") {
                for (int c = 0; c < len && r.ok; ++c) appendUtf8(s, r.u16());
            } else {
                s = r.str(len);
            }
            entries.push_back(std::move(s));
        }
        res_[id] = std::move(entries);
    }
    return !res_.empty();
}

const std::vector<std::string>* TextResource::get(uint32_t id) const {
    auto it = res_.find(id);
    return it == res_.end() ? nullptr : &it->second;
}

std::string TextResource::line(uint32_t id, int index) const {
    const auto* v = get(id);
    if (!v || index < 0 || index >= (int)v->size()) return {};
    return (*v)[index];
}

static std::string g_folder = "localisation/english/data/interface/";

const TextResource& gameTexts() {
    static TextResource texts;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        static const char* langs[] = {"english", "german", "french", "spanish", "italian", "russian", "dutch"};
        for (const char* l : langs) {
            std::string folder = std::string("localisation/") + l + "/data/interface/";
            if (texts.load(folder + "texts.res")) { g_folder = folder; break; }
        }
    }
    return texts;
}

const std::string& gameInterfaceFolder() {
    gameTexts();
    return g_folder;
}
