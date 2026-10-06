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

bool Level::doorOpenFor(const Door& d, const Instance& in) const {
    if (d.building >= 0 && !buildings_[d.building].active) return false;
    switch (in.el.faction) {
    case Faction::Hero: return !d.lockPC;
    case Faction::Enemy: return !d.lockVillain;
    default: return !d.lockCivilian;
    }
}

int Level::nearestDoor(float x, float y, float radius) const {
    int best = -1;
    float bd = radius * radius;
    for (int i = 0; i < (int)doors_.size(); ++i) {
        const float dx = doors_[i].mid.x - x, dy = doors_[i].mid.y - y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

int Level::doorAt(float x, float y) const {
    for (int i = 0; i < (int)doors_.size(); ++i)
        if (doors_[i].building >= 0 && doors_[i].outline.size() >= 3 && pointInPolygon(x, y, doors_[i].outline)) return i;
    return -1;
}

// Route over floors and through buildings. Lifts connect motion areas ("sectors"), possibly on
// the same layer (a wall between two roofs); a building connects the outsides of its doors;
// a stand-alone door connects its two sides. Breadth-first search from the actor's area (or
// building) to the target's.
bool Level::planRoute(Instance& in, float x, float y, int floor, std::vector<Instance::Leg>& legs, int goalBuilding) const {
    using Leg = Instance::Leg;
    legs.clear();
    const int from = in.floor;
    const bool inside = in.building >= 0;
    int startSec = inside ? -2 : sectorAt(from, in.x, in.y), goalSec = goalBuilding >= 0 ? -2 : sectorAt(floor, x, y);
    if (!inside && goalBuilding < 0 && ((lifts_.empty() && doors_.empty()) ||
                                        (floor == from && (startSec == goalSec || startSec < 0 || goalSec < 0)))) {
        legs.push_back({Leg::Walk, -1, true, x, y, floor == from ? from : floor});
        return floor == from;
    }
    const bool climber = in.el.faction == Faction::Hero && in.el.sub == kSubCooper;
    // node: an area of a floor (sector -1 = anywhere on that floor) or the inside of a building
    enum Via { Start, ByLift, ByFreeDoor, Into, OutOf };
    struct Node { int sector, floor, building; SDL_FPoint pos; Via via; int idx; bool fwd; int prev; };
    std::vector<Node> nodes{{startSec, from, in.building, {in.x, in.y}, Start, -1, true, -1}};
    std::vector<bool> usedLift(lifts_.size(), false), usedDoor(doors_.size(), false), usedBuilding(buildings_.size(), false);
    if (inside) usedBuilding[in.building] = true;
    auto atSide = [&](const Node& n, int layer, int sec) { return layer == n.floor && (n.sector < 0 || sec < 0 || sec == n.sector); };
    int found = -1;
    for (size_t head = 0; head < nodes.size() && found < 0; ++head) {
        const Node cur = nodes[head];
        if (goalBuilding >= 0 ? cur.building == goalBuilding
                              : cur.building < 0 && cur.floor == floor && (goalSec < 0 || cur.sector < 0 || cur.sector == goalSec)) {
            found = (int)head;
            break;
        }
        struct Cand { float d; Node n; };
        std::vector<Cand> cand;
        if (cur.building >= 0) {  // leave through any open door
            for (int e : buildings_[cur.building].doors) {
                const Door& d = doors_[e];
                if (usedDoor[e] || !doorOpenFor(d, in) || d.outLayer >= (int)layers_.size()) continue;
                cand.push_back({0, {doorOutSec_[e], d.outLayer, -1, d.out, OutOf, e, true, (int)head}});
            }
        } else {
            for (int i = 0; i < (int)lifts_.size(); ++i) {
                const Lift& l = lifts_[i];
                if (usedLift[i] || (l.type == Lift::Wall && !climber)) continue;
                const bool atA = l.layerA == cur.floor && (cur.sector < 0 || l.sectorA == cur.sector);
                const bool atB = l.layerB == cur.floor && (cur.sector < 0 || l.sectorB == cur.sector);
                if (!atA && !atB) continue;
                const int toFloor = atA ? l.layerB : l.layerA;
                if (toFloor >= (int)layers_.size()) continue;
                const SDL_FPoint entry = atA ? l.a[0] : l.b[0], exit = atA ? l.b[0] : l.a[0];
                int toSec = sectorAt(toFloor, exit.x, exit.y);
                if (toSec < 0) toSec = atA ? l.sectorB : l.sectorA;
                cand.push_back({groundDist(cur.pos.x, cur.pos.y, entry.x, entry.y), {toSec, toFloor, -1, exit, ByLift, i, atA, (int)head}});
            }
            for (int i = 0; i < (int)doors_.size(); ++i) {
                const Door& d = doors_[i];
                if (usedDoor[i] || !doorOpenFor(d, in)) continue;
                if (d.building >= 0) {
                    if (usedBuilding[d.building] || !atSide(cur, d.outLayer, doorOutSec_[i])) continue;
                    cand.push_back({groundDist(cur.pos.x, cur.pos.y, d.out.x, d.out.y), {-2, cur.floor, d.building, d.in, Into, i, true, (int)head}});
                } else {
                    const bool atOut = atSide(cur, d.outLayer, doorOutSec_[i]), atIn = atSide(cur, d.inLayer, doorInSec_[i]);
                    if (!atOut && !atIn) continue;
                    const int toFloor = atOut ? d.inLayer : d.outLayer;
                    if (toFloor >= (int)layers_.size()) continue;
                    const SDL_FPoint entry = atOut ? d.out : d.in, exit = atOut ? d.in : d.out;
                    cand.push_back({groundDist(cur.pos.x, cur.pos.y, entry.x, entry.y),
                                    {atOut ? doorInSec_[i] : doorOutSec_[i], toFloor, -1, exit, ByFreeDoor, i, atOut, (int)head}});
                }
            }
        }
        std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.d < b.d; });
        for (const auto& c : cand) {
            if (c.n.via == ByLift) usedLift[c.n.idx] = true;
            else usedDoor[c.n.idx] = true;
            if (c.n.via == Into) usedBuilding[c.n.building] = true;
            nodes.push_back(c.n);
        }
    }
    if (found < 0) return false;
    std::vector<int> chain;
    for (int k = found; nodes[k].prev >= 0; k = nodes[k].prev) chain.push_back(k);
    std::reverse(chain.begin(), chain.end());
    int f = from;
    for (int k : chain) {
        const Node& n = nodes[k];
        switch (n.via) {
        case ByLift: {
            const Lift& l = lifts_[n.idx];
            const SDL_FPoint entry = n.fwd ? l.a[0] : l.b[0];
            legs.push_back({Leg::Walk, -1, true, entry.x, entry.y, f});
            legs.push_back({Leg::Lift, n.idx, n.fwd, 0, 0, n.floor});
            break;
        }
        case ByFreeDoor: {
            const Door& d = doors_[n.idx];
            const SDL_FPoint entry = n.fwd ? d.out : d.in;
            legs.push_back({Leg::Walk, -1, true, entry.x, entry.y, f});
            legs.push_back({Leg::Direct, -1, true, d.mid.x, d.mid.y, f});
            legs.push_back({Leg::Direct, -1, true, n.pos.x, n.pos.y, n.floor});
            break;
        }
        case Into: {
            const Door& d = doors_[n.idx];
            legs.push_back({Leg::Walk, -1, true, d.out.x, d.out.y, f});
            legs.push_back({Leg::Direct, -1, true, d.mid.x, d.mid.y, f});
            legs.push_back({Leg::Direct, -1, true, d.in.x, d.in.y, f});
            legs.push_back({Leg::Enter, -1, true, d.in.x, d.in.y, f, n.idx});
            break;
        }
        case OutOf: {
            const Door& d = doors_[n.idx];
            legs.push_back({Leg::Exit, -1, true, d.in.x, d.in.y, n.floor, n.idx});
            legs.push_back({Leg::Direct, -1, true, d.mid.x, d.mid.y, n.floor});
            legs.push_back({Leg::Direct, -1, true, d.out.x, d.out.y, n.floor});
            break;
        }
        default: break;
        }
        f = n.floor;
    }
    if (goalBuilding < 0) legs.push_back({Leg::Walk, -1, true, x, y, floor});
    return true;
}

// Starts the leg at in.routeIdx. Returns false when the route is over.
bool Level::startLeg(Instance& in) {
    using Leg = Instance::Leg;
    while (in.routeIdx < in.route.size()) {
        const Leg leg = in.route[in.routeIdx++];
        in.path.clear();
        in.pathIdx = 0;
        switch (leg.kind) {
        case Leg::Lift: {
            const Lift& l = lifts_[leg.lift];
            in.lift = leg.lift;
            in.liftFwd = leg.forward;
            in.liftPos = 0;
            const SDL_FPoint* s = leg.forward ? l.a : l.b;
            const SDL_FPoint* e = leg.forward ? l.b : l.a;
            in.liftPts = {{in.x, in.y}, s[1], s[2], e[2], e[1], e[0]};
            if (in.prone) startTransition(in, false);  // nobody crawls up a ladder
            return true;
        }
        case Leg::Enter: {  // gone inside: out of sight while crossing the building
            const Door& d = doors_[leg.door];
            in.building = d.building;
            in.doorWait = 0;
            // the time to cross the house: until the next exit, if any
            if (in.routeIdx < in.route.size() && in.route[in.routeIdx].kind == Leg::Exit) {
                const Door& e = doors_[in.route[in.routeIdx].door];
                in.doorWait = std::clamp(groundDist(d.in.x, d.in.y, e.in.x, e.in.y) / 60.0f, 0.6f, 4.0f);
                return true;
            }
            if (selected_ >= 0 && &instances_[selected_] == &in) target_ = d.mid;
            continue;
        }
        case Leg::Exit: {
            const Door& d = doors_[leg.door];
            in.building = -1;
            in.x = leg.x;
            in.y = leg.y;
            in.floor = std::clamp(leg.floor, 0, std::max(0, (int)nav_.size() - 1));
            in.dir = dirTowards(d.out.x - d.in.x, d.out.y - d.in.y);
            continue;
        }
        case Leg::Direct:
            in.floor = std::clamp(leg.floor, 0, std::max(0, (int)nav_.size() - 1));
            if (groundDist(in.x, in.y, leg.x, leg.y) > 1) { in.path.push_back({leg.x, leg.y}); return true; }
            continue;
        case Leg::Walk:
            in.floor = std::clamp(leg.floor, 0, std::max(0, (int)nav_.size() - 1));
            if (in.floor < (int)nav_.size() && nav_[in.floor].findPath({in.x, in.y}, {leg.x, leg.y}, in.path) && !in.path.empty())
                return true;
            in.path.clear();
            if (groundDist(in.x, in.y, leg.x, leg.y) > 2) { in.path.push_back({leg.x, leg.y}); return true; }
            continue;
        }
    }
    in.route.clear();
    in.routeIdx = 0;
    return false;
}

bool Level::routeTo(Instance& in, float x, float y, int floor, bool run, int goalBuilding) {
    std::vector<Instance::Leg> legs;
    const bool ok = planRoute(in, x, y, floor, legs, goalBuilding);
    if (SDL_getenv("DESP_SCRIPTLOG")) {
        std::string d;
        for (const auto& l : legs)
            switch (l.kind) {
            case Instance::Leg::Lift: d += " lift" + std::to_string(l.lift); break;
            case Instance::Leg::Enter: d += " enter(door" + std::to_string(l.door) + ")"; break;
            case Instance::Leg::Exit: d += " exit(door" + std::to_string(l.door) + ")"; break;
            case Instance::Leg::Direct: break;
            default: d += " walk(" + std::to_string((int)l.x) + "," + std::to_string((int)l.y) + " f" + std::to_string(l.floor) + ")";
            }
        SDL_Log("route %s from %.0f,%.0f f%d s%d%s to %.0f,%.0f f%d s%d%s:%s", ok ? "ok" : "none", in.x, in.y, in.floor,
                sectorAt(in.floor, in.x, in.y), in.building >= 0 ? " (inside)" : "", x, y, floor, sectorAt(floor, x, y),
                goalBuilding >= 0 ? " (building)" : "", d.c_str());
    }
    if (!ok) return false;
    in.running = run;
    in.route = std::move(legs);
    in.routeIdx = 0;
    if (in.lift >= 0 || in.doorWait > 0) return true;  // finish the climb / the crossing first
    return startLeg(in) || true;
}

bool Level::moveIntoBuilding(Instance& in, int building, bool run) {
    if (building < 0 || building >= (int)buildings_.size()) return false;
    if (in.building == building) return true;
    return routeTo(in, in.x, in.y, in.floor, run, building);
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
