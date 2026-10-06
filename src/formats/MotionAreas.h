// MOVE chunk of a level file (read by DVFastFindGrid in the original): where characters can walk.
//
//   u32 version (1), u16 layerCount, per layer (ground, roofs/upper floors...):
//     u16 polygonCount (outlines + holes), u16 areaCount, per area:
//       polygon outline           (u16 n, n * (s16 x, s16 y))
//       u16 lineCount, lineCount * (s16 x0, s16 y0, s16 x1, s16 y1)   thin barriers
//       u16 holeCount, holeCount * polygon                            obstacles (buildings...)
//   followed by the original's precomputed search grid, which we don't need.
#pragma once
#include <SDL.h>

#include <cstdint>
#include <vector>

struct MotionArea {
    std::vector<SDL_Point> outline;
    std::vector<std::vector<SDL_Point>> holes;
    struct Line { int x0, y0, x1, y1; };
    std::vector<Line> lines;
};

struct MotionLayer {
    std::vector<MotionArea> areas;
    int polygonCount = 0;  // outlines + holes; areas are numbered ("sectors") over all layers
    int firstSector = 0;
    bool liftLayer = false;  // holds only the outlines of stairs / ladders (see Lifts.h)
    bool walkable(float x, float y) const;
};

bool parseMotionAreas(const uint8_t* data, size_t size, std::vector<MotionLayer>& layers);
bool pointInPolygon(float x, float y, const std::vector<SDL_Point>& poly);
