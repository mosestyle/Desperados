#include "BitmapFont.h"

#include "../core/FileSystem.h"
#include "../formats/ByteReader.h"
#include "../formats/Image16.h"

static uint32_t nextChar(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int extra = c >= 0xE0 ? 2 : 1;
    uint32_t v = c & (extra == 2 ? 0x0F : 0x1F);
    for (int k = 0; k < extra && i < s.size(); ++k) v = (v << 6) | ((unsigned char)s[i++] & 0x3F);
    return v;
}

bool BitmapFont::load(const std::string& name) {
    std::vector<uint8_t> buf;
    if (!fs_::readData("data/interface/fonts/" + name + ".fnt", buf) || buf.size() < 70) return false;
    ByteReader r(buf.data(), buf.size());
    if (r.str(6) != "SBFONT") return false;
    uint32_t version = r.u32();
    r.skip(36);
    uint32_t type = r.u32();
    height_ = (int)r.u32();
    r.u32();
    r.u32();
    uint32_t n = r.u32();
    if (version >= 0x200) r.u32();
    for (uint32_t i = 0; i < n && r.ok; ++i) {
        uint32_t c = r.u16();
        Glyph g;
        g.x = (int)r.u32();
        g.w = (int)r.u32();
        g.before = r.s32();
        g.after = r.s32();
        glyphs_[c] = g;
    }
    std::vector<Image16> imgs;
    if (!r.ok || !parseImageContainer(r.here(), r.remaining(), imgs) || imgs.empty()) return false;
    const Image16& im = imgs[0];
    imgW_ = im.w;
    imgH_ = im.h;
    const uint16_t key = type == 2 ? 0 : im.px[0];
    img_.resize(im.px.size());
    for (size_t i = 0; i < im.px.size(); ++i) {
        uint16_t p = im.px[i];
        if (p == key) { img_[i] = 0; continue; }
        uint32_t R = ((p >> 11) & 31) * 255 / 31, G = ((p >> 5) & 63) * 255 / 63, B = (p & 31) * 255 / 31;
        img_[i] = 0xFF000000u | (R << 16) | (G << 8) | B;
    }
    return true;
}

void BitmapFont::release() {
    if (tex_) SDL_DestroyTexture(tex_);
    tex_ = nullptr;
    owner_ = nullptr;
}

bool BitmapFont::upload(SDL_Renderer* r) {
    if (tex_ && owner_ == r) return true;
    release();
    if (img_.empty()) return false;
    tex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, imgW_, imgH_);
    if (!tex_) return false;
    SDL_UpdateTexture(tex_, nullptr, img_.data(), imgW_ * 4);
    SDL_SetTextureBlendMode(tex_, SDL_BLENDMODE_BLEND);
    owner_ = r;
    return true;
}

const BitmapFont::Glyph* BitmapFont::glyph(uint32_t c) const {
    auto it = glyphs_.find(c);
    if (it != glyphs_.end()) return &it->second;
    // fall back for typographic characters the font lacks
    if (c == 0x2019 || c == 0x2018) c = '\'';
    else if (c == 0x201C || c == 0x201D) c = '"';
    else if (c == 0x2013 || c == 0x2014) c = '-';
    else if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
    else return nullptr;
    it = glyphs_.find(c);
    return it == glyphs_.end() ? nullptr : &it->second;
}

float BitmapFont::width(const std::string& s, float scale) const {
    float w = 0;
    for (size_t i = 0; i < s.size();) {
        const Glyph* g = glyph(nextChar(s, i));
        if (g) w += (g->before + g->w + g->after) * scale;
        else w += height_ * 0.3f * scale;
    }
    return w;
}

float BitmapFont::draw(SDL_Renderer* r, const std::string& s, float x, float y, float scale, SDL_Color tint) {
    if (!upload(r)) return 0;
    SDL_SetTextureColorMod(tex_, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(tex_, tint.a);
    float x0 = x;
    for (size_t i = 0; i < s.size();) {
        const Glyph* g = glyph(nextChar(s, i));
        if (!g) { x += height_ * 0.3f * scale; continue; }
        x += g->before * scale;
        SDL_Rect src{g->x, 0, g->w, imgH_};
        SDL_FRect dst{x, y, g->w * scale, imgH_ * scale};
        SDL_RenderCopyF(r, tex_, &src, &dst);
        x += (g->w + g->after) * scale;
    }
    return x - x0;
}

std::vector<std::string> BitmapFont::wrap(const std::string& s, float maxW, float scale) const {
    std::vector<std::string> lines;
    std::string cur, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string test = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && width(test, scale) > maxW) { lines.push_back(cur); cur = word; }
        else cur = test;
        word.clear();
    };
    for (char c : s) {
        if (c == ' ') flushWord();
        else if (c == '\n') { flushWord(); lines.push_back(cur); cur.clear(); }
        else word += c;
    }
    flushWord();
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

static BitmapFont* g_ui = nullptr;
static BitmapFont* g_title = nullptr;

BitmapFont& uiFont() {
    if (!g_ui) { g_ui = new BitmapFont(); if (!g_ui->load("tooltips")) g_ui->load("tips"); }
    return *g_ui;
}

BitmapFont& titleFont() {
    if (!g_title) { g_title = new BitmapFont(); if (!g_title->load("entries_0")) g_title->load("title"); }
    return *g_title;
}

void releaseFonts() {
    if (g_ui) g_ui->release();
    if (g_title) g_title->release();
}
