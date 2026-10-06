// MASK chunk: pieces of the background that must be drawn again in front of
// characters standing behind them (walls, roofs, trees, fences...).
//
//   u32 version, u16 groupCount, per group: u16 count, per mask:
//     u8 flags; [bit0] u16 n + n*(s16 x, s16 y)  sort line (along the object's base)
//               [bit1] u16 n + n points          second line
//               [bit4] s16 height
//     s16 x, s16 y, s16 w, s16 h, u16 dataSize, data:
//       per row: u8 byteCount, then runs: c>=0x80 -> repeat next byte (c-0x80) times,
//                                         c<0x80  -> copy c bytes. 1 bit per pixel, MSB first.
// Group 0 is the ground floor; higher groups seem to belong to upper floors.
#pragma once
#include <SDL.h>

#include <cstdint>
#include <vector>

struct Mask {
    int group = 0;
    uint8_t flags = 0;
    int height = 0;
    std::vector<SDL_Point> line, line2;
    SDL_Rect rect{0, 0, 0, 0};
    std::vector<uint8_t> bits;  // w*h, 1 = foreground pixel
    // y of the sort line at x (ends extended horizontally)
    float lineY(float x) const;
};

bool parseMasks(const uint8_t* data, size_t size, std::vector<Mask>& out);
