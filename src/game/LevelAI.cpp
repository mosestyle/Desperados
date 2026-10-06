// Enemy behaviour: patrols, field of view, suspicion, alarm, chase, shooting, searching.
// Distances are in "ground" pixels: the isometric view squashes the ground 2:1 vertically.
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <string>

#include "Level.h"

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr int kAnimIdle = 0, kAnimWalk = 3, kAnimRun = 5, kAnimImpact = 25, kAnimImpactDown = 26, kAnimDraw = 27,
              kAnimShoot = 28, kAnimDie = 33, kAnimAlert = 104;
constexpr float kFovHalf = 45.0f * kPi / 180.0f;
constexpr float kNearFraction = 0.45f;     // inner part of the cone also sees lying heroes
constexpr float kShootRange = 230.0f;
constexpr float kShoutRadius = 320.0f;     // an alarmed enemy alerts the others around him
constexpr float kHeightStanding = 45.0f, kHeightProne = 12.0f;

float groundDist(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = 2.0f * (by - ay);
    return std::sqrt(dx * dx + dy * dy);
}
// facing direction (0-15, 0 = north, clockwise) as a ground-space unit vector
void dirVector(int dir, float& gx, float& gy) {
    float a = dir * (2 * kPi / 16);
    gx = std::sin(a);
    gy = -std::cos(a);
}
int dirTowards(float dx, float dy) {
    float a = std::atan2(dx, -2.0f * dy);
    int d = (int)std::lround(a / (kPi / 8.0f));
    return (d % 16 + 16) % 16;
}
float frand() { return (float)std::rand() / (float)RAND_MAX; }
}  // namespace

void Level::initAI() {
    for (auto& in : instances_) {
        if (in.el.kind != LevelElement::Actor) continue;
        if (in.el.faction == Faction::Hero) {
            in.ai = Instance::AI::Calm;  // heroes only use health / death
            in.health = in.maxHealth = 100;
            continue;
        }
        if (in.el.faction != Faction::Enemy && in.el.faction != Faction::Civilian) continue;
        // actors of the briefing / intro cutscenes are driven by scripts we don't run yet: keep them passive
        std::string scriptLower = in.el.script;
        for (auto& ch : scriptLower) ch = (char)std::tolower((unsigned char)ch);
        if (scriptLower.find("brief") != std::string::npos || scriptLower.find("intro") != std::string::npos) continue;
        in.ai = Instance::AI::Calm;
        if (in.el.faction == Faction::Enemy) {
            in.profile = &profiles_.get(in.el.profile);
            in.health = in.maxHealth = std::max(10, in.profile->health);
        }
        if (in.el.path >= 0 && in.el.path < (int)paths_.size() && !paths_[in.el.path].empty()) {
            in.pathId = in.el.path;
            in.wpIdx = 0;
            in.atWaypoint = false;
            // start at the waypoint closest to where the NPC stands
            float best = 1e30f;
            const auto& p = paths_[in.pathId];
            for (size_t i = 0; i < p.size(); ++i) {
                float d = groundDist(in.x, in.y, (float)p[i].x, (float)p[i].y);
                if (d < best) { best = d; in.wpIdx = (int)i; }
            }
            if (best < 12) beginWaypoint(in);
        }
        in.thinkT = frand() * 0.2f;
    }
}

float Level::viewRange(const Instance& e) const {
    int attention = e.profile ? e.profile->attention : 50;
    return 170.0f + 1.6f * std::clamp(attention, 0, 100);
}

bool Level::lineOfSight(float ax, float ay, float bx, float by, float targetHeight) const {
    for (const auto& o : sight_)
        if (o.height > targetHeight && o.crosses(ax, ay, bx, by)) return false;
    return true;
}

bool Level::canSee(const Instance& e, const Instance& h, bool& nearZone, float& dist) const {
    if (h.ai == Instance::AI::Dead || h.floor != e.floor || h.hidden || e.hidden) return false;
    // Until the mission scripts run, a hero still at its scripted start position is ignored
    // (several missions start next to enemies in a cutscene).
    if (h.untouched) return false;
    dist = groundDist(e.x, e.y, h.x, h.y);
    const float range = viewRange(e);
    if (dist > range || dist < 1) return dist < 1;
    float fx, fy;
    dirVector(e.dir, fx, fy);
    float gx = (h.x - e.x) / dist, gy = 2.0f * (h.y - e.y) / dist;
    float cosA = fx * gx + fy * gy;
    nearZone = dist < range * kNearFraction;
    if (cosA < std::cos(kFovHalf)) {
        return dist < 28;  // right behind his back still gets noticed
    }
    if (h.prone && !nearZone) return false;  // lying heroes are only seen up close
    return lineOfSight(e.x, e.y, h.x, h.y, h.prone ? kHeightProne : kHeightStanding);
}

void Level::walkTo(Instance& e, float x, float y, bool run, bool direct) {
    e.running = run;
    e.path.clear();
    e.route.clear();
    e.pathIdx = 0;
    if (direct || e.floor >= (int)nav_.size()) {
        e.path.push_back({x, y});
        return;
    }
    if (!nav_[e.floor].findPath({e.x, e.y}, {x, y}, e.path)) e.path.push_back({x, y});
}

// Arrived at a waypoint: pick one of its order lists (by probability) and start executing it.
void Level::beginWaypoint(Instance& e) {
    e.atWaypoint = true;
    e.cmds.clear();
    e.cmdIdx = 0;
    e.waitT = 0;
    const Waypoint& wp = paths_[e.pathId][e.wpIdx];
    if (!wp.script.empty()) {  // scripted waypoint: the mission script decides what happens here
        auto it = waypointScript_.find({e.pathId, e.wpIdx});
        if (it != waypointScript_.end()) callScript(it->second, "ReachPoint", {actorHandle(e)});
        return;
    }
    if (!wp.options.empty()) {
        int total = 0;
        for (const auto& o : wp.options) total += std::max(0, o.probability);
        int roll = total > 0 ? std::rand() % total : 0;
        for (const auto& o : wp.options) {
            roll -= std::max(0, o.probability);
            if (roll < 0) { e.cmds = o.commands; break; }
        }
    }
}

void Level::updatePatrol(Instance& e, float dt) {
    if (e.pathId < 0) {
        if (!e.hasPost || !e.path.empty()) return;
        if (groundDist(e.x, e.y, e.postX, e.postY) > 6) { walkTo(e, e.postX, e.postY, false, false); return; }
        if (e.postDir >= 0 && e.dir != e.postDir) setAnim(e, kAnimIdle, e.postDir);
        return;
    }
    const PatrolPath& p = paths_[e.pathId];
    if (!e.atWaypoint) {
        if (e.path.empty()) {
            const Waypoint& wp = p[e.wpIdx];
            if (groundDist(e.x, e.y, (float)wp.x, (float)wp.y) < 4) beginWaypoint(e);
            else walkTo(e, (float)wp.x, (float)wp.y, false, true);  // routes are drawn as straight lines
        }
        return;
    }
    // executing the waypoint's orders
    if (e.waitT > 0) { e.waitT -= dt; return; }
    while (e.cmdIdx < e.cmds.size()) {
        const WayCommand& c = e.cmds[e.cmdIdx++];
        if (c.op == WayCommand::Wait) { e.waitT = c.a / 25.0f; return; }
        if (c.op == WayCommand::Face) setAnim(e, kAnimIdle, c.a & 15);
        if (c.op == WayCommand::LookAt && (c.a != (int)e.x || c.b != (int)e.y))
            setAnim(e, kAnimIdle, dirTowards(c.a - e.x, c.b - e.y));
    }
    if (p.size() > 1) {  // on to the next waypoint (routes loop)
        e.wpIdx = (e.wpIdx + 1) % (int)p.size();
        e.atWaypoint = false;
    } else {
        beginWaypoint(e);  // a single waypoint: a guard post, repeat the orders (looking around)
        if (e.cmds.empty()) e.waitT = 2.0f;
    }
}

// Puts an enemy on alert towards a position (heard a shot, was shot at, was shouted at).
void Level::alertTo(Instance& e, int heroIdx, float x, float y) {
    if (e.ai == Instance::AI::Dead || e.ai == Instance::AI::KO || e.el.faction != Faction::Enemy) return;
    if (e.ai == Instance::AI::Alert) { e.seenX = x; e.seenY = y; e.lostT = 0; return; }
    e.ai = Instance::AI::Alert;
    e.meter = 1.0f;
    e.target = heroIdx;
    e.seenX = x;
    e.seenY = y;
    e.lostT = 0;
    e.markT = 2.5f;
    e.path.clear();
    e.drawn = false;
    e.waitT = 0.3f + frand() * 0.6f;  // short reaction time
}

void Level::noise(float x, float y, float radius, int heroIdx) {
    for (auto& o : instances_)
        if (o.el.faction == Faction::Enemy && groundDist(x, y, o.x, o.y) < radius) alertTo(o, heroIdx, x, y);
}

void Level::raiseAlarm(Instance& e, int heroIdx, float x, float y) {
    const bool wasAlert = e.ai == Instance::AI::Alert;
    e.ai = Instance::AI::Alert;
    e.meter = 1.0f;
    e.target = heroIdx;
    e.seenX = x;
    e.seenY = y;
    e.lostT = 0;
    e.markT = 2.5f;
    if (wasAlert) return;
    e.path.clear();
    e.drawn = false;
    setAnim(e, kAnimIdle, dirTowards(x - e.x, y - e.y));
    playOnce(e, kAnimAlert);
    // shout: alert the enemies around
    for (auto& o : instances_) {
        if (&o == &e || o.ai == Instance::AI::Alert) continue;
        if (groundDist(e.x, e.y, o.x, o.y) < kShoutRadius) alertTo(o, heroIdx, x, y);
    }
}

void Level::hurtHero(Instance& h, int damage) {
    if (h.ai == Instance::AI::Dead) return;
    h.health -= damage;
    if (h.health <= 0) {
        h.path.clear();
        h.health = 0;
        h.ai = Instance::AI::Dead;
        h.prone = false;
        playOnce(h, kAnimDie, true);
        if (selected_ >= 0 && &instances_[selected_] == &h) selected_ = firstHero();
        return;
    }
    if (h.transition < 0) playOnce(h, h.prone ? kAnimImpactDown : kAnimImpact);
}

void Level::shoot(Instance& e, Instance& h) {
    playOnce(e, kAnimShoot);
    float dist = groundDist(e.x, e.y, h.x, h.y);
    float skill = (e.profile ? e.profile->marksmanship : 50) / 100.0f;
    float chance = std::clamp(skill * (1.15f - dist / (kShootRange * 1.4f)), 0.08f, 0.9f);
    if (h.prone) chance *= 0.6f;
    if (frand() < chance) hurtHero(h, 20 + std::rand() % 16);
}

void Level::updateEnemy(Instance& e, int idx, float dt) {
    (void)idx;
    e.markT = std::max(0.0f, e.markT - dt);
    if (e.ai == Instance::AI::KO) {  // out cold; wakes up confused and starts searching
        e.koT -= dt;
        if (e.koT <= 0 && e.transition < 0) {
            e.holdLast = false;
            e.ai = Instance::AI::Searching;
            e.searchTurns = 5;
            e.meter = 0.6f;
            e.waitT = 0;
            playOnce(e, 34);  // "Se relever" (get up)
        }
        return;
    }
    // --- look for heroes (a few times per second) ---
    e.thinkT -= dt;
    int seen = -1;
    bool seenNear = false;
    if (e.thinkT <= 0) {
        e.thinkT = 0.12f;
        float best = 1e30f;
        for (size_t i = 0; i < instances_.size(); ++i) {
            const Instance& h = instances_[i];
            if (h.el.faction != Faction::Hero || h.ai == Instance::AI::Dead) continue;
            bool nearZone = false;
            float d = 0;
            if (canSee(e, h, nearZone, d) && d < best) { best = d; seen = (int)i; seenNear = nearZone; }
        }
        if (seen >= 0) {
            const Instance& h = instances_[seen];
            float intel = std::clamp((e.profile ? e.profile->intelligence : 50) / 50.0f, 0.6f, 1.6f);
            float rate = (seenNear ? 2.6f : 0.9f) * intel;
            if (!h.path.empty()) rate *= h.running ? 2.0f : 1.4f;  // movement catches the eye
            if (h.prone) rate *= 0.6f;
            e.meter = std::min(1.0f, e.meter + rate * 0.12f);
            e.target = seen;
            e.seenX = h.x;
            e.seenY = h.y;
            e.lostT = 0;
        } else if (e.ai != Instance::AI::Alert) {
            e.meter = std::max(0.0f, e.meter - 0.18f * 0.12f);
        }
        // a dead or unconscious comrade in view: go and look, weapon ready
        if (e.ai == Instance::AI::Calm || e.ai == Instance::AI::Suspicious) {
            float fx, fy;
            dirVector(e.dir, fx, fy);
            const float range = viewRange(e);
            for (auto& b : instances_) {
                if (&b == &e || b.discovered || b.el.faction != Faction::Enemy) continue;
                if (b.ai != Instance::AI::Dead && b.ai != Instance::AI::KO) continue;
                float d = groundDist(e.x, e.y, b.x, b.y);
                if (d > range || d < 1) continue;
                float gx = (b.x - e.x) / d, gy = 2.0f * (b.y - e.y) / d;
                if (fx * gx + fy * gy < std::cos(kFovHalf)) continue;
                if (!lineOfSight(e.x, e.y, b.x, b.y, kHeightProne)) continue;
                b.discovered = true;
                e.ai = Instance::AI::Searching;
                e.searchTurns = 8;
                e.meter = 0.7f;
                e.markT = 2.0f;
                e.waitT = 0;
                walkTo(e, b.x + 10, b.y + 6, true, false);
                break;
            }
        }
    }

    switch (e.ai) {
    case Instance::AI::Calm:
        if (e.meter > 0.25f) {  // something there... stop and look
            if (std::getenv("DESP_AIDEBUG"))
                SDL_Log("AI: %s at %.0f,%.0f floor %d dir %d suspicious of %s at %.0f,%.0f floor %d (dist %.0f)",
                        e.el.set.c_str(), e.x, e.y, e.floor, e.dir, e.target >= 0 ? instances_[e.target].el.set.c_str() : "?",
                        e.seenX, e.seenY, e.target >= 0 ? instances_[e.target].floor : -1,
                        groundDist(e.x, e.y, e.seenX, e.seenY));
            e.ai = Instance::AI::Suspicious;
            e.path.clear();
            e.markT = 1.5f;
            setAnim(e, kAnimIdle, dirTowards(e.seenX - e.x, e.seenY - e.y));
            break;
        }
        if (e.transition < 0) updatePatrol(e, dt);
        break;
    case Instance::AI::Suspicious:
        if (seen >= 0) setAnim(e, kAnimIdle, dirTowards(e.seenX - e.x, e.seenY - e.y));
        if (e.meter >= 1.0f) { raiseAlarm(e, e.target, e.seenX, e.seenY); break; }
        if (e.meter <= 0.02f) {  // must have been nothing
            e.ai = Instance::AI::Calm;
            e.atWaypoint = false;
        }
        break;
    case Instance::AI::Alert: {
        if (e.transition >= 0) break;  // finishing "alert" / shooting animations
        if (e.waitT > 0) { e.waitT -= dt; break; }
        if (e.target < 0 || instances_[e.target].ai == Instance::AI::Dead) {
            e.target = -1;
            for (size_t i = 0; i < instances_.size(); ++i)  // any other hero left?
                if (instances_[i].el.faction == Faction::Hero && instances_[i].ai != Instance::AI::Dead) { e.target = (int)i; break; }
            if (e.target < 0) { e.ai = Instance::AI::Searching; e.searchTurns = 6; e.path.clear(); break; }
        }
        Instance& h = instances_[e.target];
        bool nearZone = false;
        float d = 0;
        const bool visible = canSee(e, h, nearZone, d) ||
                             (groundDist(e.x, e.y, h.x, h.y) < viewRange(e) &&
                              lineOfSight(e.x, e.y, h.x, h.y, h.prone ? kHeightProne : kHeightStanding));
        if (visible) { e.seenX = h.x; e.seenY = h.y; e.lostT = 0; }
        else e.lostT += dt;
        const float dist = groundDist(e.x, e.y, e.seenX, e.seenY);
        if (visible && dist < kShootRange) {
            e.path.clear();
            setAnim(e, kAnimIdle, dirTowards(h.x - e.x, h.y - e.y));
            if (!e.drawn) { e.drawn = true; playOnce(e, kAnimDraw); e.shootT = 0.4f + frand() * 0.5f; break; }
            e.shootT -= dt;
            if (e.shootT <= 0) {
                shoot(e, h);
                e.shootT = 1.3f + frand() * 1.0f;
            }
            break;
        }
        if (e.lostT > 7.0f) {  // lost him: go and look around where he was last seen
            e.ai = Instance::AI::Searching;
            e.searchTurns = 6;
            e.waitT = 0;
            walkTo(e, e.seenX, e.seenY, false, false);
            break;
        }
        e.repathT -= dt;
        if (e.path.empty() || e.repathT <= 0) {
            e.repathT = 0.6f;
            if (dist > 6) walkTo(e, e.seenX, e.seenY, true, false);
        }
        break;
    }
    case Instance::AI::Searching:
        if (e.meter >= 1.0f && seen >= 0) { raiseAlarm(e, seen, e.seenX, e.seenY); break; }
        if (!e.path.empty() || e.transition >= 0) break;
        if (e.waitT > 0) { e.waitT -= dt; break; }
        if (e.searchTurns-- > 0) {  // turn around, looking
            setAnim(e, kAnimIdle, (e.dir + 4 + (std::rand() % 9)) & 15);
            e.waitT = 0.8f + frand() * 0.8f;
            break;
        }
        e.ai = Instance::AI::Calm;  // give up, back to the patrol
        e.meter = 0.2f;
        e.drawn = false;
        e.atWaypoint = false;
        break;
    default:
        break;
    }
}

void Level::updateAI(float dt) {
    for (size_t i = 0; i < instances_.size(); ++i) {
        Instance& in = instances_[i];
        if (in.ai == Instance::AI::None || in.ai == Instance::AI::Dead || in.hidden) continue;
        // the mission script is driving this actor (cutscene, locked AI)
        if (in.el.faction != Faction::Hero && (in.aiLocked || in.seqBusy > 0)) continue;
        if (in.el.faction == Faction::Hero && in.seqBusy > 0) continue;
        if (in.el.faction == Faction::Hero) { updateHeroOrder(in, (int)i, dt); continue; }
        if (in.el.faction == Faction::Enemy) updateEnemy(in, (int)i, dt);
        else if (in.el.faction == Faction::Civilian && in.transition < 0) updatePatrol(in, dt);
    }
}

// ---------------------------------------------------------------------------------------------

int Level::pickEnemy(float wx, float wy, float slack) const {
    int best = -1;
    float bestD = 1e30f;
    for (size_t i = 0; i < instances_.size(); ++i) {
        const auto& in = instances_[i];
        if (in.el.faction != Faction::Enemy || in.ai == Instance::AI::Dead || in.hidden) continue;
        const SDL_Rect& w = in.world;
        if (wx < w.x - slack || wx > w.x + w.w + slack || wy < w.y - slack || wy > w.y + w.h + slack) continue;
        float d = (wx - in.x) * (wx - in.x) + (wy - (in.y - w.h * 0.4f)) * (wy - (in.y - w.h * 0.4f));
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    return best;
}

void Level::toggleCone(int idx) {
    if (idx >= 0 && idx < (int)instances_.size()) instances_[idx].showCone = !instances_[idx].showCone;
}

void Level::debugPlaceSelected(float x, float y) {
    if (selected_ < 0) return;
    Instance& h = instances_[selected_];
    h.untouched = false;
    h.x = x;
    h.y = y;
    h.path.clear();
    updateFrameRect(h);
}

void Level::showAllCones() {
    for (auto& in : instances_) in.showCone = in.el.faction == Faction::Enemy;
}

void Level::hideAllCones() {
    for (auto& in : instances_) in.showCone = false;
}

bool Level::allHeroesDead() const {
    bool anyHero = false;
    for (const auto& in : instances_) {
        if (in.el.faction != Faction::Hero || in.hidden) continue;
        anyHero = true;
        if (in.ai != Instance::AI::Dead) return false;
    }
    return anyHero;
}

int Level::alertedCount() const {
    int n = 0;
    for (const auto& in : instances_) n += in.ai == Instance::AI::Alert;
    return n;
}

void Level::drawCone(SDL_Renderer* r, const Camera& cam, const Instance& e) {
    constexpr int N = 28;
    const float range = viewRange(e), nearR = range * kNearFraction;
    float fx, fy;
    dirVector(e.dir, fx, fy);
    const float base = std::atan2(fx, -fy);
    SDL_Color inner, outer;
    if (e.ai == Instance::AI::Alert) { inner = {230, 40, 30, 95}; outer = {230, 40, 30, 50}; }
    else if (e.ai == Instance::AI::Suspicious || e.ai == Instance::AI::Searching) { inner = {240, 200, 40, 95}; outer = {240, 200, 40, 50}; }
    else { inner = {40, 210, 70, 85}; outer = {40, 210, 70, 42}; }

    // cast rays (in ground space) and stop them at sight obstacles taller than a standing man
    SDL_FPoint nearPts[N + 1], farPts[N + 1];
    for (int i = 0; i <= N; ++i) {
        float a = base - kFovHalf + 2 * kFovHalf * i / N;
        float gx = std::sin(a), gy = -std::cos(a);
        float ex = e.x + gx * range, ey = e.y + gy * range * 0.5f;  // back to screen-space world coords
        float t = 1.0f;
        for (const auto& o : sight_)
            if (o.height > kHeightStanding) t = std::min(t, o.raycast(e.x, e.y, ex, ey));
        float tn = std::min(t, kNearFraction);
        nearPts[i] = {cam.toScreenX(e.x + (ex - e.x) * tn), cam.toScreenY(e.y + (ey - e.y) * tn)};
        farPts[i] = {cam.toScreenX(e.x + (ex - e.x) * t), cam.toScreenY(e.y + (ey - e.y) * t)};
    }
    (void)nearR;
    const SDL_FPoint o{cam.toScreenX(e.x), cam.toScreenY(e.y)};
    std::vector<SDL_Vertex> v;
    v.reserve(N * 9);
    auto tri = [&](SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col) {
        v.push_back({a, col, {0, 0}});
        v.push_back({b, col, {0, 0}});
        v.push_back({c, col, {0, 0}});
    };
    for (int i = 0; i < N; ++i) {
        tri(o, nearPts[i], nearPts[i + 1], inner);
        tri(nearPts[i], farPts[i], farPts[i + 1], outer);
        tri(nearPts[i], farPts[i + 1], nearPts[i + 1], outer);
    }
    SDL_RenderGeometry(r, nullptr, v.data(), (int)v.size(), nullptr, 0);
}

void Level::drawMarker(SDL_Renderer* r, const Camera& cam, const Instance& e) {
    // "!" (alarm, red) or "?" (suspicious, yellow) above the head
    const bool alarm = e.ai == Instance::AI::Alert;
    const float px = std::max(2.0f, 2.2f * cam.zoom);
    const float cx = cam.toScreenX(e.x), top = cam.toScreenY((float)e.world.y) - px * 9;
    SDL_SetRenderDrawColor(r, 20, 10, 5, 220);
    SDL_FRect bg{cx - px * 2.5f, top - px, px * 5, px * 9};
    SDL_RenderFillRectF(r, &bg);
    if (alarm) SDL_SetRenderDrawColor(r, 255, 60, 40, 255);
    else SDL_SetRenderDrawColor(r, 255, 215, 60, 255);
    if (alarm) {
        SDL_FRect bar{cx - px * 0.5f, top, px, px * 4.5f}, dot{cx - px * 0.5f, top + px * 5.8f, px, px};
        SDL_RenderFillRectF(r, &bar);
        SDL_RenderFillRectF(r, &dot);
    } else {
        SDL_FRect parts[5] = {{cx - px * 1.5f, top, px * 3, px},       {cx + px * 0.5f, top + px, px, px * 1.5f},
                              {cx - px * 0.5f, top + px * 2.5f, px * 1.5f, px}, {cx - px * 0.5f, top + px * 3.5f, px, px},
                              {cx - px * 0.5f, top + px * 5.8f, px, px}};
        SDL_RenderFillRectsF(r, parts, 5);
    }
}

void Level::renderAI(SDL_Renderer* r, const Camera& cam, int sw, int sh) {
    (void)sw;
    (void)sh;
    for (const auto& in : instances_) {
        if (in.hidden) continue;
        if (in.el.faction == Faction::Enemy && in.ai != Instance::AI::Dead && in.ai != Instance::AI::KO &&
            (in.ai == Instance::AI::Suspicious || (in.ai == Instance::AI::Alert && in.markT > 0) ||
             in.ai == Instance::AI::Searching))
            drawMarker(r, cam, in);
        if (in.el.faction == Faction::Hero && in.ai != Instance::AI::Dead && in.health < in.maxHealth) {
            // health bar above the hero
            const float w = 26 * cam.zoom, h = std::max(3.0f, 3 * cam.zoom);
            const float x = cam.toScreenX(in.x) - w / 2, y = cam.toScreenY((float)in.world.y) - h * 2.5f;
            SDL_FRect back{x - 1, y - 1, w + 2, h + 2}, fill{x, y, w * in.health / (float)in.maxHealth, h};
            SDL_SetRenderDrawColor(r, 20, 10, 5, 220);
            SDL_RenderFillRectF(r, &back);
            SDL_SetRenderDrawColor(r, in.health > 40 ? 70 : 230, in.health > 40 ? 220 : 60, 60, 255);
            SDL_RenderFillRectF(r, &fill);
        }
    }
}
