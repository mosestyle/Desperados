// SBFont (SBLibNG/SBFont.cpp): the game's bitmap fonts, data/interface/fonts/*.fnt:
// "SBFONT", u32 version, name[36], u32 type, u32 height, u32 baseline, u32, u32 glyph count,
// [u32 if version >= 0x200], per glyph (u16 character, u32 x in the picture, u32 width,
// s32 space before, s32 space after), then one 16-bit picture holding every glyph side by side
// (colour key 0). Characters are UTF-16.
//
// SBTextResources: localisation/<language>/data/interface/texts.res ("SRES", u32 version,
// u32 count, per resource: tag[4], u32 id, u32, u16 n, n entries; TEXT entries are u16 length +
// UTF-16, WAVE entries u16 length + the voice file's path).
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class SBFont {
public:
    bool LoadFromFile(const std::string& gamePath);
    // width in pixels of a text
    int Width(const std::u16string& s) const;
    int Height() const { return height; }
    // draws the text on a surface, top-left at x, y. `shade`: a dark copy one pixel down-right
    // first (subtitles over the map)
    void Draw(uint32_t surface, int x, int y, const std::u16string& s, bool shade = false);
    // breaks a text into lines no wider than maxWidth
    std::vector<std::u16string> Wrap(const std::u16string& s, int maxWidth) const;

    struct Glyph {
        int x = 0, w = 0, before = 0, after = 0;
    };
    const Glyph* Find(char16_t c) const;
    uint16_t PictureWidth() const { return pictureW; }
    uint16_t PictureHeight() const { return pictureH; }
    const std::vector<uint16_t>& Pixels() const { return pixels; }

private:
    std::map<char16_t, Glyph> glyphs;
    int height = 0, baseline = 0;
    uint16_t pictureW = 0, pictureH = 0;
    std::vector<uint16_t> pixels;
    uint32_t tex = 0;        // the glyphs' surface (made on first use)
    uint32_t shadeTex = 0;   // the same in black
};

// SBResourceManager's pictures (localisation/<language>/data/interface/default.res, "SRES": the
// interface's pictures, the dialogue portraits...): PICC id -> its pictures
class SBPictureResources {
public:
    bool Load(const std::string& language);
    bool Get(uint32_t id, uint32_t entry, class SBPictureSixteen& out) const;
    size_t Count(uint32_t id) const;

private:
    std::vector<uint8_t> data;
    std::map<uint32_t, std::vector<size_t>> pictures;  // id -> the offsets of its pictures
};

class SBTextResources {
public:
    // the first language found under localisation/ (english first)
    bool Load();
    const std::u16string& Text(uint32_t id, uint32_t index) const;
    const std::string& Wave(uint32_t id, uint32_t index) const;
    std::string language;

private:
    std::map<uint32_t, std::vector<std::u16string>> texts;
    std::map<uint32_t, std::vector<std::string>> waves;
};

std::string SBToUtf8(const std::u16string& s);
