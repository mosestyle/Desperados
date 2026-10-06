// SGHT chunk ("sight obstacles" in DVFastFindGrid): 3D blockers for enemy vision.
//
//   u32 version (6), u16 count, per obstacle:
//     u16 n, n * (f32 x, f32 y, f32 zBottom, f32 zTop)      ground outline + height
//     6 * f32 bounding box (minX, minZ, minY, maxX, maxZ, maxY)
//     u8 hasLink [, u16, u16], u8, u8, u8, u8, f32, f32, u8 opacity?, f32
#pragma once
#include <cstdint>
#include <vector>

struct SightObstacle {
    std::vector<float> xs, ys;  // ground outline
    float height = 0;
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    // does the ground segment (ax,ay)-(bx,by) cross this obstacle's outline?
    bool crosses(float ax, float ay, float bx, float by) const;
    // first hit along (ax,ay)->(bx,by) as a fraction 0..1, or >1 if none
    float raycast(float ax, float ay, float bx, float by) const;
};

bool parseSightObstacles(const uint8_t* data, size_t size, std::vector<SightObstacle>& out);
