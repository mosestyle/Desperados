#include "NavGrid.h"

#include <algorithm>
#include <cmath>
#include <queue>

// Fills (value) the cells whose centres lie inside the polygon, using scanlines.
static void fillPolygon(std::vector<uint8_t>& cells, int w, int h, const std::vector<SDL_Point>& poly, uint8_t value) {
    if (poly.size() < 3) return;
    int minY = poly[0].y, maxY = poly[0].y;
    for (const auto& p : poly) { minY = std::min(minY, p.y); maxY = std::max(maxY, p.y); }
    int cy0 = std::max(0, minY / NavGrid::kCell), cy1 = std::min(h - 1, maxY / NavGrid::kCell + 1);
    std::vector<float> xs;
    for (int cy = cy0; cy <= cy1; ++cy) {
        float y = (cy + 0.5f) * NavGrid::kCell;
        xs.clear();
        for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
            const SDL_Point& a = poly[i];
            const SDL_Point& b = poly[j];
            if ((a.y > y) != (b.y > y)) xs.push_back(a.x + (y - a.y) * (float)(b.x - a.x) / (float)(b.y - a.y));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2) {
            int cx0 = std::max(0, (int)std::ceil(xs[k] / NavGrid::kCell - 0.5f));
            int cx1 = std::min(w - 1, (int)std::floor(xs[k + 1] / NavGrid::kCell - 0.5f));
            for (int cx = cx0; cx <= cx1; ++cx) cells[(size_t)cy * w + cx] = value;
        }
    }
}

// Blocks every cell the segment passes through (supercover traversal), so even
// obstacles thinner than a cell - fences are often 1-3 px wide - can't be crossed.
void NavGrid::blockSegment(float x0, float y0, float x1, float y1) {
    float fx0 = x0 / kCell, fy0 = y0 / kCell, fx1 = x1 / kCell, fy1 = y1 / kCell;
    int cx = (int)std::floor(fx0), cy = (int)std::floor(fy0);
    const int ex = (int)std::floor(fx1), ey = (int)std::floor(fy1);
    const float dx = fx1 - fx0, dy = fy1 - fy0;
    const int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
    const float tdx = dx != 0 ? std::fabs(1.0f / dx) : 1e30f, tdy = dy != 0 ? std::fabs(1.0f / dy) : 1e30f;
    float tx = dx != 0 ? ((sx > 0 ? (cx + 1 - fx0) : (fx0 - cx)) * tdx) : 1e30f;
    float ty = dy != 0 ? ((sy > 0 ? (cy + 1 - fy0) : (fy0 - cy)) * tdy) : 1e30f;
    for (int guard = 0; guard < 100000; ++guard) {
        if (cx >= 0 && cy >= 0 && cx < w_ && cy < h_) cells_[(size_t)cy * w_ + cx] = 0;
        if (cx == ex && cy == ey) break;
        if (std::fabs(tx - ty) < 1e-6f) {  // passing exactly through a corner: block both neighbours
            if (cx + sx >= 0 && cx + sx < w_ && cy >= 0 && cy < h_) cells_[(size_t)cy * w_ + cx + sx] = 0;
            if (cy + sy >= 0 && cy + sy < h_ && cx >= 0 && cx < w_) cells_[(size_t)(cy + sy) * w_ + cx] = 0;
            tx += tdx; ty += tdy; cx += sx; cy += sy;
        } else if (tx < ty) { tx += tdx; cx += sx; }
        else { ty += tdy; cy += sy; }
        if (tx > 1.0f + tdx && ty > 1.0f + tdy) break;
    }
}

void NavGrid::build(const MotionLayer& layer, int worldW, int worldH) {
    w_ = (worldW + kCell - 1) / kCell;
    h_ = (worldH + kCell - 1) / kCell;
    cells_.assign((size_t)w_ * h_, 0);
    for (const auto& area : layer.areas) fillPolygon(cells_, w_, h_, area.outline, 1);
    for (const auto& area : layer.areas) {
        for (const auto& hole : area.holes) {
            fillPolygon(cells_, w_, h_, hole, 0);
            for (size_t i = 0, j = hole.size() - 1; i < hole.size(); j = i++)
                blockSegment((float)hole[j].x, (float)hole[j].y, (float)hole[i].x, (float)hole[i].y);
        }
        for (const auto& ln : area.lines) blockSegment((float)ln.x0, (float)ln.y0, (float)ln.x1, (float)ln.y1);
    }
}

bool NavGrid::walkableAt(float x, float y) const { return cell((int)(x / kCell), (int)(y / kCell)); }

bool NavGrid::nearestWalkable(SDL_FPoint p, SDL_FPoint& out, int maxR) const {
    int cx = (int)std::floor(p.x / kCell), cy = (int)std::floor(p.y / kCell);
    if (cell(cx, cy)) { out = p; return true; }
    float best = 1e30f;
    bool found = false;
    for (int r = 1; r <= maxR && !found; ++r) {
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                if (!cell(cx + dx, cy + dy)) continue;
                float d = (float)(dx * dx + 4 * dy * dy);
                if (d < best) { best = d; out = {(cx + dx + 0.5f) * kCell, (cy + dy + 0.5f) * kCell}; found = true; }
            }
    }
    return found;
}

bool NavGrid::lineOfSight(int ax, int ay, int bx, int by) const {
    int dx = std::abs(bx - ax), dy = std::abs(by - ay), sx = ax < bx ? 1 : -1, sy = ay < by ? 1 : -1;
    int err = dx - dy, x = ax, y = ay;
    for (;;) {
        if (!cell(x, y)) return false;
        if (x == bx && y == by) return true;
        int e2 = 2 * err;
        // supercover: don't slip diagonally between two blocked cells
        if (e2 > -dy && e2 < dx && (!cell(x + sx, y) || !cell(x, y + sy))) return false;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

bool NavGrid::findPath(SDL_FPoint from, SDL_FPoint to, std::vector<SDL_FPoint>& out) const {
    out.clear();
    if (cells_.empty()) return false;
    SDL_FPoint start, goal;
    if (!nearestWalkable(from, start, 30) || !nearestWalkable(to, goal, 90)) return false;
    const int sx = (int)(start.x / kCell), sy = (int)(start.y / kCell);
    const int gx = (int)(goal.x / kCell), gy = (int)(goal.y / kCell);
    if (sx == gx && sy == gy) { out.push_back(goal); return true; }

    const size_t n = (size_t)w_ * h_;
    std::vector<float> g(n, 1e30f);
    std::vector<int32_t> parent(n, -1);
    std::vector<uint8_t> closed(n, 0);
    struct Node { float f; int32_t idx; bool operator<(const Node& o) const { return f > o.f; } };
    std::priority_queue<Node> open;
    auto H = [&](int x, int y) { float dx = (float)(x - gx), dy = 2.0f * (y - gy); return std::sqrt(dx * dx + dy * dy); };
    const int32_t s = sy * w_ + sx, goalIdx = gy * w_ + gx;
    g[s] = 0;
    open.push({H(sx, sy), s});
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static const float COST[8] = {1, 1, 2, 2, 2.2360680f, 2.2360680f, 2.2360680f, 2.2360680f};
    bool reached = false;
    int expanded = 0;
    while (!open.empty()) {
        Node cur = open.top();
        open.pop();
        if (closed[cur.idx]) continue;
        closed[cur.idx] = 1;
        if (cur.idx == goalIdx) { reached = true; break; }
        if (++expanded > 600000) break;
        int x = cur.idx % w_, y = cur.idx / w_;
        for (int k = 0; k < 8; ++k) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!cell(nx, ny)) continue;
            if (k >= 4 && (!cell(x + DX[k], y) || !cell(x, y + DY[k]))) continue;  // no corner cutting
            int32_t ni = ny * w_ + nx;
            float ng = g[cur.idx] + COST[k];
            if (ng < g[ni]) { g[ni] = ng; parent[ni] = cur.idx; open.push({ng + H(nx, ny), ni}); }
        }
    }
    if (!reached) return false;

    std::vector<int32_t> cellsPath;
    for (int32_t i = goalIdx; i != -1; i = parent[i]) cellsPath.push_back(i);
    std::reverse(cellsPath.begin(), cellsPath.end());
    // string pulling: keep only the corners needed to stay on walkable ground
    size_t anchor = 0;
    for (size_t i = 2; i < cellsPath.size(); ++i) {
        int ax = cellsPath[anchor] % w_, ay = cellsPath[anchor] / w_;
        int bx = cellsPath[i] % w_, by = cellsPath[i] / w_;
        if (!lineOfSight(ax, ay, bx, by)) {
            anchor = i - 1;
            int cx = cellsPath[anchor] % w_, cy = cellsPath[anchor] / w_;
            out.push_back({(cx + 0.5f) * kCell, (cy + 0.5f) * kCell});
        }
    }
    out.push_back(goal);
    return true;
}
