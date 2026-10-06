// The game's own bitmap fonts (data/interface/fonts/*.fnt, "SBFONT").
//
//   "SBFONT", u32 version, char name[36], u32 type, u32 height, u32 ?, u32 maxWidth, u32 glyphCount,
//   [version >= 0x200: u32 ?], glyphCount * (u16 char, u32 x, u32 width, s32 before, s32 after),
//   then one 16-bit image (u16 w, u16 h, u32 codec, u32 size, data) holding all glyphs side by side.
//   Type 2 fonts are drawn on black (transparent), type 0 on a key colour (the first pixel).
#pragma once
#include <SDL.h>

#include <map>
#include <string>
#include <vector>

class BitmapFont {
public:
    ~BitmapFont() { release(); }
    bool load(const std::string& name);  // e.g. "tooltips"
    void release();
    bool ready() const { return tex_ != nullptr || !img_.empty(); }
    int height() const { return height_; }
    float width(const std::string& utf8, float scale) const;
    // Draws one line; returns its width.
    float draw(SDL_Renderer* r, const std::string& utf8, float x, float y, float scale, SDL_Color tint = {255, 255, 255, 255});
    // Splits text into lines no wider than maxW.
    std::vector<std::string> wrap(const std::string& utf8, float maxW, float scale) const;

private:
    struct Glyph { int x, w, before, after; };
    const Glyph* glyph(uint32_t c) const;
    bool upload(SDL_Renderer* r);
    std::map<uint32_t, Glyph> glyphs_;
    int height_ = 0, imgW_ = 0, imgH_ = 0;
    std::vector<uint32_t> img_;  // ARGB, kept to re-create the texture
    SDL_Texture* tex_ = nullptr;
    SDL_Renderer* owner_ = nullptr;
};

// Shared fonts, created on first use.
BitmapFont& uiFont();     // subtitles, hints, objectives
BitmapFont& titleFont();  // big titles
void releaseFonts();      // when the GL context is lost
