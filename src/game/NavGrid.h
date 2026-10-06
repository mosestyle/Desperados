// Walkability grid built from the level's motion areas, with A* path finding.
// Distances use "ground" metric: the isometric view squashes the ground 2:1,
// so one screen pixel up/down is two pixels of real distance.
#pragma once
#include <SDL.h>

#include <cstdint>
#include <vector>

#include "../formats/MotionAreas.h"

class NavGrid {
public:
    static constexpr int kCell = 4;  // world pixels per cell

    void build(const MotionLayer& layer, int worldW, int worldH);
    bool walkableAt(float x, float y) const;
    // Path from `from` to `to` (world pixels). The goal snaps to the nearest walkable spot.
    // Returns false if no path exists. `out` holds the waypoints after `from`.
    bool findPath(SDL_FPoint from, SDL_FPoint to, std::vector<SDL_FPoint>& out) const;
    bool nearestWalkable(SDL_FPoint p, SDL_FPoint& out, int maxRadiusCells = 40) const;
    bool empty() const { return cells_.empty(); }
    int width() const { return w_; }
    int height() const { return h_; }
    bool blockedCell(int cx, int cy) const { return !cell(cx, cy); }

private:
    bool cell(int cx, int cy) const { return cx >= 0 && cy >= 0 && cx < w_ && cy < h_ && cells_[(size_t)cy * w_ + cx]; }
    bool lineOfSight(int ax, int ay, int bx, int by) const;
    void blockSegment(float x0, float y0, float x1, float y1);
    int w_ = 0, h_ = 0;
    std::vector<uint8_t> cells_;
};
