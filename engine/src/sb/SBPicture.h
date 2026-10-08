// SBPictureSixteen: the original's 16-bit picture (RGB565) as stored in .dvm, .map, .pak files
// and inside level hunks: u16 width, u16 height, u32 codec (0 raw, 1 zlib, 2 bzip2), u32 size,
// data. (SBLibNG/SBPictureSixteen.cpp: LoadFromStream / LoadFromFile.)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

class SBFile;

class SBPictureSixteen {
public:
    bool LoadFromStream(SBFile& file);
    bool LoadFromFile(const std::string& gamePath);
    uint16_t GetWidth() const { return width; }
    uint16_t GetHeight() const { return height; }
    const std::vector<uint16_t>& Pixels() const { return pixels; }
    std::vector<uint16_t>& Pixels() { return pixels; }

private:
    uint16_t width = 0, height = 0;
    std::vector<uint16_t> pixels;
};

// helpers shared by the loaders
bool SBUncompressZlib(const uint8_t* src, size_t n, uint8_t* dst, size_t want);
bool SBUncompressBzip2(const uint8_t* src, size_t n, uint8_t* dst, size_t want);
