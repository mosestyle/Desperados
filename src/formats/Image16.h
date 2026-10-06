// 16-bit (RGB565) images: level backgrounds (.dvm), campaign maps (.map),
// loading screens (.pak) and the minimap stored inside level files.
//
// Container layout, repeated until end of file:
//   u16 width, u16 height, u32 codec (1 = zlib, 2 = bzip2), u32 compressedSize,
//   compressed RGB565 pixels (width*height*2 bytes when unpacked)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Image16 {
    int w = 0, h = 0;
    std::vector<uint16_t> px;  // RGB565, row major
};

// Colour used by the game for "transparent" in 16-bit images (pure green).
constexpr uint16_t kColorKeyGreen = 0x07C0;

bool parseImageContainer(const uint8_t* data, size_t size, std::vector<Image16>& out);
bool loadImageContainer(const std::string& relativePath, std::vector<Image16>& out);
