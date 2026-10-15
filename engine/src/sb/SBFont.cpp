#include "sb/SBFont.h"

#include <cstring>

#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"
#include "sb/SBPicture.h"

bool SBFont::LoadFromFile(const std::string& gamePath) {
    SBFile f;
    if (f.Open(gamePath) != 0) return false;
    char magic[6];
    f.Serialize(magic, 6);
    if (memcmp(magic, "SBFONT", 6) != 0) return false;
    uint32_t version = f.U32();
    char name[36];
    f.Serialize(name, 36);
    f.U32();  // type
    height = (int)f.U32();
    baseline = (int)f.U32();
    f.U32();
    uint32_t n = f.U32();
    if (version >= 0x200) f.U32();
    for (uint32_t i = 0; i < n; ++i) {
        char16_t c = (char16_t)f.U16();
        Glyph g;
        g.x = (int)f.U32();
        g.w = (int)f.U32();
        g.before = f.S32();
        g.after = f.S32();
        glyphs[c] = g;
    }
    SBPictureSixteen pic;
    if (!pic.LoadFromStream(f)) return false;
    pictureW = pic.GetWidth();
    pictureH = pic.GetHeight();
    pixels = pic.Pixels();
    if (height <= 0 || height > pictureH) height = pictureH;
    return f.lastError == 0;
}

const SBFont::Glyph* SBFont::Find(char16_t c) const {
    auto it = glyphs.find(c);
    if (it != glyphs.end()) return &it->second;
    // typographic quotes and dashes the font may lack
    char16_t alt = 0;
    switch (c) {
        case 0x2018: case 0x2019: alt = '\''; break;
        case 0x201c: case 0x201d: alt = '"'; break;
        case 0x2013: case 0x2014: alt = '-'; break;
        case 0x2026: alt = '.'; break;
        case 0xa0: alt = ' '; break;
    }
    if (alt) {
        it = glyphs.find(alt);
        if (it != glyphs.end()) return &it->second;
    }
    return nullptr;
}

int SBFont::Width(const std::u16string& s) const {
    int w = 0;
    for (char16_t c : s) {
        int k = c == 0x2026 ? 3 : 1;
        if (const Glyph* g = Find(c)) w += k * (g->before + g->w + g->after);
    }
    return w;
}

void SBFont::Draw(uint32_t surface, int x, int y, const std::u16string& s, bool shade) {
    SBDrawManager* dm = SBDrawManager::mpDrawManager;
    if (!dm || pixels.empty()) return;
    if (!tex) {
        tex = dm->CreateSurface(pictureW, pictureH);
        dm->UploadPicture(tex, pixels.data(), pictureW, pictureH);
        dm->SetColorKeyingForSurface(tex, 0);
        std::vector<uint16_t> dark(pixels.size());
        for (size_t i = 0; i < pixels.size(); ++i) dark[i] = pixels[i] ? 0x0821 : 0;
        shadeTex = dm->CreateSurface(pictureW, pictureH);
        dm->UploadPicture(shadeTex, dark.data(), pictureW, pictureH);
        dm->SetColorKeyingForSurface(shadeTex, 0);
    }
    for (int pass = shade ? 0 : 1; pass < 2; ++pass) {
        int px = x + (pass == 0 ? 2 : 0), py = y + (pass == 0 ? 2 : 0);
        for (char16_t c : s) {
            int k = c == 0x2026 ? 3 : 1;
            const Glyph* g = Find(c);
            if (!g) continue;
            for (int r = 0; r < k; ++r) {
                px += g->before;
                if (g->w > 0) {
                    SBGeoBoundingBox2D src(SBGeoPoint2D((float)g->x, 0), SBGeoPoint2D((float)(g->x + g->w), (float)pictureH));
                    SBGeoBoundingBox2D dst(SBGeoPoint2D((float)px, (float)py),
                                           SBGeoPoint2D((float)(px + g->w), (float)(py + pictureH)));
                    dm->Blit(pass == 0 ? shadeTex : tex, &src, surface, &dst, BLIT_COLORKEY);
                }
                px += g->w + g->after;
            }
        }
    }
}

std::vector<std::u16string> SBFont::Wrap(const std::u16string& s, int maxWidth) const {
    std::vector<std::u16string> lines;
    std::u16string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::u16string tryLine = line.empty() ? word : line + u" " + word;
        if (!line.empty() && Width(tryLine) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = tryLine;
        }
        word.clear();
    };
    for (char16_t c : s) {
        if (c == ' ' || c == '\n') {
            flushWord();
            if (c == '\n') {
                lines.push_back(line);
                line.clear();
            }
        } else {
            word += c;
        }
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// ---- texts.res ---------------------------------------------------------------------------------

bool SBTextResources::Load() {
    static const char* langs[] = {"english", "french", "german", "spanish", "italian", "polish", "russian", "czech"};
    std::vector<std::string> tries;
    for (const char* l : langs) tries.push_back(l);
    std::vector<uint8_t> d;
    for (const std::string& l : tries) {
        if (SBFile::ReadAll("localisation\\" + l + "\\data\\interface\\texts.res", d)) {
            language = l;
            break;
        }
    }
    if (d.empty() && SBFile::ReadAll("data\\interface\\texts.res", d)) language = "";
    if (d.size() < 12 || memcmp(d.data(), "SRES", 4) != 0) return false;
    size_t o = 4;
    auto u16 = [&](size_t& p) {
        uint16_t v = 0;
        if (p + 2 <= d.size()) memcpy(&v, &d[p], 2);
        p += 2;
        return v;
    };
    auto u32 = [&](size_t& p) {
        uint32_t v = 0;
        if (p + 4 <= d.size()) memcpy(&v, &d[p], 4);
        p += 4;
        return v;
    };
    u32(o);
    uint32_t count = u32(o);
    for (uint32_t i = 0; i < count && o + 14 <= d.size(); ++i) {
        char tag[4];
        memcpy(tag, &d[o], 4);
        o += 4;
        uint32_t id = u32(o);
        u32(o);
        uint16_t n = u16(o);
        bool text = memcmp(tag, "TEXT", 4) == 0;
        for (uint16_t k = 0; k < n && o + 2 <= d.size(); ++k) {
            uint16_t len = u16(o);
            if (text) {
                std::u16string s;
                for (uint16_t c = 0; c < len && o + 2 <= d.size(); ++c) s += (char16_t)u16(o);
                texts[id].push_back(s);
            } else {
                std::string s(reinterpret_cast<const char*>(&d[o]), std::min<size_t>(len, d.size() - o));
                o += len;
                waves[id].push_back(s);
            }
        }
    }
    return true;
}

const std::u16string& SBTextResources::Text(uint32_t id, uint32_t index) const {
    static const std::u16string none;
    auto it = texts.find(id);
    if (it == texts.end() || index >= it->second.size()) return none;
    return it->second[index];
}

const std::string& SBTextResources::Wave(uint32_t id, uint32_t index) const {
    static const std::string none;
    auto it = waves.find(id);
    if (it == waves.end() || index >= it->second.size()) return none;
    return it->second[index];
}

std::string SBToUtf8(const std::u16string& s) {
    std::string o;
    for (char16_t c : s) {
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) {
            o += (char)(0xc0 | (c >> 6));
            o += (char)(0x80 | (c & 0x3f));
        } else {
            o += (char)(0xe0 | (c >> 12));
            o += (char)(0x80 | ((c >> 6) & 0x3f));
            o += (char)(0x80 | (c & 0x3f));
        }
    }
    return o;
}

// ---- default.res -----------------------------------------------------------------------------------

bool SBPictureResources::Load(const std::string& language) {
    std::vector<std::string> tries;
    if (!language.empty()) tries.push_back("localisation\\" + language + "\\data\\interface\\default.res");
    tries.push_back("localisation\\english\\data\\interface\\default.res");
    tries.push_back("data\\interface\\default.res");
    for (const std::string& t : tries)
        if (SBFile::ReadAll(t, data)) break;
    if (data.size() < 12 || memcmp(data.data(), "SRES", 4) != 0) return false;
    auto u16 = [&](size_t p) {
        uint16_t v = 0;
        if (p + 2 <= data.size()) memcpy(&v, &data[p], 2);
        return v;
    };
    auto u32 = [&](size_t p) {
        uint32_t v = 0;
        if (p + 4 <= data.size()) memcpy(&v, &data[p], 4);
        return v;
    };
    uint32_t count = u32(8);
    size_t o = 12;
    for (uint32_t i = 0; i < count && o + 12 <= data.size(); ++i) {
        char tag[4];
        memcpy(tag, &data[o], 4);
        uint32_t id = u32(o + 4);
        o += 12;  // tag, id, a u32
        int images = -1;
        if (!memcmp(tag, "TEXT", 4)) {
            uint16_t n = u16(o);
            o += 2;
            for (uint16_t k = 0; k < n; ++k) o += 2 + 2 * (size_t)u16(o);
            continue;
        }
        if (!memcmp(tag, "WAVE", 4)) {
            uint16_t n = u16(o);
            o += 2;
            for (uint16_t k = 0; k < n; ++k) o += 2 + (size_t)u16(o);
            continue;
        }
        auto bits = [&](int k) {
            uint32_t m = u32(o);
            o += 4;
            int c = 0;
            for (int b = 0; b < k; ++b) c += (m >> b) & 1;
            return c;
        };
        if (!memcmp(tag, "PICC", 4)) {
            images = (int)u32(o);
            o += 4;
        } else if (!memcmp(tag, "PIC ", 4)) {
            images = 1;
        } else if (!memcmp(tag, "TOGL", 4)) {
            images = bits(5);
        } else if (!memcmp(tag, "BTTN", 4)) {
            images = bits(4);
        } else if (!memcmp(tag, "NPTF", 4) || !memcmp(tag, "SLID", 4)) {
            images = bits(6);
        } else if (!memcmp(tag, "RDO ", 4)) {
            images = bits(7);
        } else if (!memcmp(tag, "CUR ", 4)) {
            o += 8;
            images = (int)u32(o);
            o += 4;
        } else {
            SBLog("default.res: unknown resource %.4s, %zu pictures read", tag, pictures.size());
            break;
        }
        std::vector<size_t>& list = pictures[id];
        for (int k = 0; k < images && o + 12 <= data.size(); ++k) {
            list.push_back(o);
            o += 12 + (size_t)u32(o + 8);
        }
    }
    return !pictures.empty();
}

size_t SBPictureResources::Count(uint32_t id) const {
    auto it = pictures.find(id);
    return it == pictures.end() ? 0 : it->second.size();
}

bool SBPictureResources::Get(uint32_t id, uint32_t entry, SBPictureSixteen& out) const {
    auto it = pictures.find(id);
    if (it == pictures.end() || entry >= it->second.size()) return false;
    size_t o = it->second[entry], used = 0;
    return out.LoadFromMemory(&data[o], data.size() - o, used);
}
