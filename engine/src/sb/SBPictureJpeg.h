// SBPictureJpeg: a JPEG picture read from a stream, in 16-bit pixels (RGB565)
#pragma once
#include <cstdint>
#include <vector>

class SBFile;

class SBPictureJpeg {
public:
    bool LoadFromStream(SBFile& f);  // 0x08181890
    uint16_t GetWidth() const { return width; }
    uint16_t GetHeight() const { return height; }
    const std::vector<uint16_t>& Pixels() const { return pixels; }

private:
    uint16_t width = 0, height = 0;
    std::vector<uint16_t> pixels;
};
