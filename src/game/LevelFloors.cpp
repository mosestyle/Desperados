// Moving between floors: stairs, ladders and climbable walls (LIFT chunk).
//
// A route is a list of legs: walk on one floor to the outer point of a lift, traverse the lift
// (climbing / stairs animation), walk on the next floor... Floors are the MOVE layers; the
// layer holding the lifts' own outlines is never a destination.
#include <algorithm>
#include <cmath>
#include <queue>
#include <string>

#include "Level.h"

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr int kAnimWalk = 3, kAnimStairs = 4, kAnimLadderUp = 11, kAnimLadderDown = 14, kAnimWallUp = 45,
              kAnimWallDown = 48, kAnimIdle = 0;
constexpr int kSubCooper = 1;

float groundDist(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = 2.0f * (by - ay);
    return std::sqrt(dx * dx + dy * dy);
}
int dirTowards(float dx, float dy) {
    float a = std::atan2(dx, -2.0f * dy);
    int d = (int)std::lround(a / (kPi / 8.0f));
    return (d % 16 + 16) % 16;
}
}  // namespace

// Floor of a tapped / scripted point: the highest floor walkable there (roofs hide the ground
// behind them), the preferred floor when it is one of them.
int Level::floorForPoint(float x, float y, int prefer) const {
    int best = -1;
    for (int l = 0; l < (int)layers_.size(); ++l) {
        if (layers_[l].liftLayer || !layers_[l].walkable(x, y)) continue;
        best = l;
    }
    if (best < 0) return std::clamp(prefer, 0, std::max(0, (int)layers_.size() - 1));
    return best;
}

// Sector (motion area id, numbered over all layers) at a point of a layer, -1 if none.
int Level::sectorAt(int layer, float x, float y) const {
    if (layer < 0 || layer >= (int)layers_.size()) return -1;
    const MotionLayer& L = layers_[layer];
    int id = L.firstSector;
    for (const auto& a : L.areas) {
        if (pointInPolygon(x, y, a.outline)) {
            bool hole = false;
            for (const auto& h : a.holes) hole |= pointInPolygon(x, y, h);
            if (!hole) return id;
        }
        id += 1 + (int)a.holes.size();
    }
    return -1;
}

// Route over floors: lifts connect motion areas ("sectors"), possibly on the same layer
// (a wall between two roofs). Breadth-first search from the actor's area to the target's.
bool Level::planRoute(Instance& in, float x, float y, int floor, std::vector<Instance::Leg>& legs) const {
    legs.clear();
    const int from = in.floor;
    int startSec = sectorAt(from, in.x, in.y), goalSec = sectorAt(floor, x, y);
    if (lifts_.empty() || (floor == from && (startSec == goalSec || startSec < 0 || goalSec < 0))) {
        legs.push_back({-1, true, x, y, floor == from ? from : floor});
        return floor == from;
    }
    const bool climber = in.el.faction == Faction::Hero && in.el.sub == kSubCooper;
    // nodes: sectors; a node unknown (-1) stands for "anywhere on that floor"
    struct Node { int sector, floor; SDL_FPoint pos; int lift; bool fwd; int prev; };
    std::vector<Node> nodes{{startSec, from, {in.x, in.y}, -1, true, -1}};
    std::vector<bool> used(lifts_.size(), false);
    int found = -1;
    for (size_t head = 0; head < nodes.size() && found < 0; ++head) {
        const Node cur = nodes[head];
        if (cur.floor == floor && (goalSec < 0 || cur.sector < 0 || cur.sector == goalSec)) { found = (int)head; break; }
        std::vector<std::pair<float, int>> cand;
        for (int i = 0; i < (int)lifts_.size(); ++i) {
            const Lift& l = lifts_[i];
            if (used[i] || (l.type == Lift::Wall && !climber)) continue;
            const bool atA = l.layerA == cur.floor && (cur.sector < 0 || l.sectorA == cur.sector);
            const bool atB = l.layerB == cur.floor && (cur.sector < 0 || l.sectorB == cur.sector);
            if (atA) cand.push_back({groundDist(cur.pos.x, cur.pos.y, l.a[0].x, l.a[0].y), i});
            else if (atB) cand.push_back({groundDist(cur.pos.x, cur.pos.y, l.b[0].x, l.b[0].y), -1 - i});
        }
        std::sort(cand.begin(), cand.end());
        for (const auto& c : cand) {
            const bool fwd = c.second >= 0;
            const int i = fwd ? c.second : -1 - c.second;
            const Lift& l = lifts_[i];
            used[i] = true;
            const int toFloor = fwd ? l.layerB : l.layerA;
            if (toFloor >= (int)layers_.size()) continue;
            const SDL_FPoint exit = fwd ? l.b[0] : l.a[0];
            int toSec = sectorAt(toFloor, exit.x, exit.y);
            if (toSec < 0) toSec = fwd ? l.sectorB : l.sectorA;
            nodes.push_back({toSec, toFloor, exit, i, fwd, (int)head});
        }
    }
    if (found < 0) return false;
    std::vector<int> chain;
    for (int k = found; nodes[k].prev >= 0; k = nodes[k].prev) chain.push_back(k);
    std::reverse(chain.begin(), chain.end());
    int f = from;
    for (int k : chain) {
        const Lift& l = lifts_[nodes[k].lift];
        const SDL_FPoint entry = nodes[k].fwd ? l.a[0] : l.b[0];
        legs.push_back({-1, true, entry.x, entry.y, f});
        f = nodes[k].floor;
        legs.push_back({nodes[k].lift, nodes[k].fwd, 0, 0, f});
    }
    legs.push_back({-1, true, x, y, floor});
    return true;
}

// Starts the leg at in.routeIdx. Returns false when the route is over.
bool Level::startLeg(Instance& in) {
    while (in.routeIdx < in.route.size()) {
        const Instance::Leg leg = in.route[in.routeIdx++];
        if (leg.lift >= 0) {
            const Lift& l = lifts_[leg.lift];
            in.lift = leg.lift;
            in.liftFwd = leg.forward;
            in.liftPos = 0;
            in.liftPts.clear();
            const SDL_FPoint* s = leg.forward ? l.a : l.b;
            const SDL_FPoint* e = leg.forward ? l.b : l.a;
            in.liftPts = {{in.x, in.y}, s[1], s[2], e[2], e[1], e[0]};
            in.path.clear();
            in.pathIdx = 0;
            if (in.prone) startTransition(in, false);  // nobody crawls up a ladder
            return true;
        }
        in.floor = std::clamp(leg.floor, 0, std::max(0, (int)nav_.size() - 1));
        in.path.clear();
        in.pathIdx = 0;
        if (in.floor < (int)nav_.size() && nav_[in.floor].findPath({in.x, in.y}, {leg.x, leg.y}, in.path) && !in.path.empty())
            return true;
        if (groundDist(in.x, in.y, leg.x, leg.y) > 2) { in.path.push_back({leg.x, leg.y}); return true; }
    }
    in.route.clear();
    in.routeIdx = 0;
    return false;
}

bool Level::routeTo(Instance& in, float x, float y, int floor, bool run) {
    std::vector<Instance::Leg> legs;
    const bool ok = planRoute(in, x, y, floor, legs);
    if (SDL_getenv("DESP_SCRIPTLOG")) {
        std::string d;
        for (const auto& l : legs) d += l.lift >= 0 ? " lift" + std::to_string(l.lift) : " walk(" + std::to_string((int)l.x) + "," + std::to_string((int)l.y) + " f" + std::to_string(l.floor) + ")";
        SDL_Log("route %s from %.0f,%.0f f%d s%d to %.0f,%.0f f%d s%d:%s", ok ? "ok" : "none", in.x, in.y, in.floor,
                sectorAt(in.floor, in.x, in.y), x, y, floor, sectorAt(floor, x, y), d.c_str());
    }
    if (!ok) return false;
    in.running = run;
    if (in.lift >= 0) {  // finish the climb first, then follow the new route
        in.route = std::move(legs);
        in.routeIdx = 0;
        return true;
    }
    in.route = std::move(legs);
    in.routeIdx = 0;
    return startLeg(in) || true;
}

void Level::updateLift(Instance& in, float dt) {
    const Lift& l = lifts_[in.lift];
    const bool up = in.liftFwd ? l.layerB > l.layerA : l.layerA > l.layerB;
    int climbAnim = kAnimWalk;
    float speed = 45.0f;
    int dir = l.dir;
    if (l.type == Lift::Ladder) { climbAnim = up ? kAnimLadderUp : kAnimLadderDown; speed = 32.0f; }
    else if (l.type == Lift::Wall) { climbAnim = up ? kAnimWallUp : kAnimWallDown; speed = 26.0f; }
    else if (l.type == Lift::Stairs) { climbAnim = kAnimStairs; speed = 50.0f; dir = in.liftFwd ? l.dir : (l.dir + 8) & 15; }
    else dir = in.liftFwd ? l.dir : (l.dir + 8) & 15;
    if (!in.set->find(climbAnim, dir)) climbAnim = kAnimWalk;

    float budget = (in.running && l.type <= Lift::Stairs ? 1.6f : 1.0f) * speed * dt;
    while (budget > 0 && in.liftPos + 1 < in.liftPts.size()) {
        const size_t seg = (size_t)in.liftPos;
        const SDL_FPoint a = in.liftPts[seg], b = in.liftPts[seg + 1];
        const float len = std::max(0.01f, std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y)));
        const float frac = in.liftPos - (float)seg;
        const float left = (1 - frac) * len, step = std::min(budget, left);
        const float t = frac + step / len;
        in.x = a.x + (b.x - a.x) * t;
        in.y = a.y + (b.y - a.y) * t;
        budget -= step;
        in.liftPos = t >= 0.999f ? (float)(seg + 1) : (float)seg + t;
        // the outer parts are walked, the middle part is the climb
        const bool climbing = seg >= 1 && seg <= 3;
        if (climbing) setAnim(in, climbAnim, l.type >= Lift::Ladder ? dir : dirTowards(b.x - a.x, b.y - a.y));
        else setAnim(in, kAnimWalk, dirTowards(b.x - a.x, b.y - a.y));
        if (seg >= 3) in.floor = in.liftFwd ? l.layerB : l.layerA;  // near the top / bottom: on the new floor
    }
    // animation: climbing frames advance with time
    const auto& entries = in.rec->entries;
    in.ticks += dt * 25.0f;
    for (int guard = 0; guard < 32; ++guard) {
        float dur = (float)std::max<int>(1, entries[in.entry].duration);
        if (in.ticks < dur) break;
        in.ticks -= dur;
        in.entry = (in.entry + 1) % (int)entries.size();
    }
    if (in.liftPos + 1 >= in.liftPts.size()) {  // arrived
        in.floor = in.liftFwd ? l.layerB : l.layerA;
        in.lift = -1;
        in.liftPts.clear();
        if (!startLeg(in)) setAnim(in, kAnimIdle, in.dir);
    }
}
