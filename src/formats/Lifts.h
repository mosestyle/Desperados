// LIFT chunk (DVFastFindGrid "lifts"): stairs, ladders and climbable walls that connect two
// floor layers of the motion areas.
//
//   u32 version (2), u16 count, per lift:
//     u16 sector (the lift's own area, numbered over all MOVE polygons, layer after layer)
//     u8 type: 0 slope, 1 stairs, 2 ladder, 3 wall (only Cooper can climb)
//     u16 sectorA, u16 layerA, u16 sectorB, u16 layerB      the floors at both ends
//     u16 n, n * (s16 x, s16 y)                             outline
//     6 * (s16 x, s16 y): A outer point, A middle, A start of the climb,
//                         B outer point, B middle, B start of the climb
//     u16 direction (0-15) the character faces on the way from A to B
#pragma once
#include <SDL.h>

#include <cstdint>
#include <vector>

struct Lift {
    enum Type { Slope = 0, Stairs = 1, Ladder = 2, Wall = 3 };
    int sector = 0, type = 0;
    int sectorA = 0, layerA = 0, sectorB = 0, layerB = 0;
    std::vector<SDL_Point> outline;
    SDL_FPoint a[3], b[3];  // [0] outer, [1] middle, [2] where the climb starts
    int dir = 0;
};

bool parseLifts(const uint8_t* data, size_t size, std::vector<Lift>& out);
