// Hero actions: shooting (each hero's own gun from weapons.dat), melee (knife / punch / kick)
// and Cooper's knife throw. The hero walks until the target is in range (and in sight for
// ranged attacks), then plays the original animation; the effect is applied when it ends.
//
// The mission scripts can switch actions on and off (SetActionAvailable). The numbers are the
// "Action N" of the original animation names: Cooper 1 Colt, 2 fist, 3 knife, 4 knife throw,
// 5 watch; Kate 2 kick; Sanchez 2 punch; everybody's gun is action 1.
// Targets are enemies, civilians and scripted objects (the flower pot of mission 1, targets
// of the knife-throwing stand...): hitting an object runs its script (Shooted, Stabbed, Hit, Dagger).
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "Level.h"

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr int kAnimIdle = 0, kAnimProneShoot = 8, kAnimImpact = 25, kAnimKO = 30, kAnimDie = 33, kAnimDraw = 27,
              kAnimShoot = 28, kAnimKnife = 35, kAnimPunch = 36, kAnimThrow = 44, kAnimKick = 70, kAnimSanchezPunch = 82,
              kAnimReload = 136, kAnimReloadProne = 137;
constexpr float kMeleeRange = 22.0f, kThrowRange = 150.0f;
constexpr float kGunNoise = 420.0f;      // ground px: enemies in this radius hear a shot
constexpr float kRangeScale = 0.6f;      // weapons.dat ranges -> ground pixels
constexpr float kHeightStanding = 45.0f;
constexpr int kEffectShot = 1, kEffectKill = 2, kEffectKO = 3, kEffectReload = 4, kEffectThrow = 5;
constexpr int kSubCooper = 1, kSubKate = 4, kSubSanchez = 5;

float groundDist(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = 2.0f * (by - ay);
    return std::sqrt(dx * dx + dy * dy);
}
int dirTowards(float dx, float dy) {
    float a = std::atan2(dx, -2.0f * dy);
    int d = (int)std::lround(a / (kPi / 8.0f));
    return (d % 16 + 16) % 16;
}
float frand() { return (float)std::rand() / (float)RAND_MAX; }
bool hasAnim(const SpriteSet* set, int anim) {
    for (const auto& r : set->records)
        if (r.anim == anim) return true;
    return false;
}
}  // namespace

void Level::initHeroes() {
    weapons_.load();
    static const char* kWeapon[8] = {"", "Colt Cooper", "Colt Doc", "Winchester Sam", "Pistolet Kate", "Fusil Sanchez",
                                     "Sarbacanne Mia", ""};
    for (auto& h : instances_) {
        if (h.el.faction != Faction::Hero || h.el.kind != LevelElement::Actor) continue;
        const int id = std::clamp(h.el.sub, 0, 7);
        if (const Weapon* w = weapons_.find(kWeapon[id])) {
            h.gunRange = w->range * kRangeScale;
            h.maxAmmo = h.ammo = std::max(1, w->ammo);
        } else if (hasAnim(h.set, kAnimShoot)) {
            h.gunRange = 260;
            h.maxAmmo = h.ammo = 6;
        }
        if (!hasAnim(h.set, kAnimShoot)) h.gunRange = 0;
    }
}

bool Level::actionAvailable(const Instance& h, int action) const {
    auto it = actionAvail_.find({h.elem, action});
    return it == actionAvail_.end() || it->second;
}

// Punch / kick of a hero (knocks out). Cooper's knife is a separate action (Action::Knife).
int Level::meleeFor(const Instance& h, bool& kills) const {
    kills = false;
    if (!actionAvailable(h, 2)) return -1;
    if (h.el.sub == kSubKate && hasAnim(h.set, kAnimKick)) return kAnimKick;
    if (h.el.sub == kSubSanchez && hasAnim(h.set, kAnimSanchezPunch)) return kAnimSanchezPunch;
    if (hasAnim(h.set, kAnimPunch)) return kAnimPunch;
    return -1;
}

bool Level::selectedCan(Action a) const {
    if (selected_ < 0 || selected_ >= (int)instances_.size()) return false;
    const Instance& h = instances_[selected_];
    if (h.ai == Instance::AI::Dead || h.hidden || h.building >= 0) return false;
    bool kills;
    switch (a) {
    case Action::Gun: return h.gunRange > 0 && actionAvailable(h, 1);
    case Action::Melee: return meleeFor(h, kills) >= 0;
    case Action::Throw: return h.el.sub == kSubCooper && hasAnim(h.set, kAnimThrow) && actionAvailable(h, 4);
    case Action::Knife: return h.el.sub == kSubCooper && hasAnim(h.set, kAnimKnife) && actionAvailable(h, 3);
    default: return false;
    }
}

int Level::selectedActionAnim(Action a) const {
    if (selected_ < 0) return -1;
    const Instance& h = instances_[selected_];
    bool kills;
    switch (a) {
    case Action::Gun: return kAnimShoot;
    case Action::Melee: return meleeFor(h, kills);
    case Action::Throw: return kAnimThrow;
    case Action::Knife: return kAnimKnife;
    default: return -1;
    }
}

float Level::selectedGunRange() const {
    if (selected_ < 0) return 0;
    return instances_[selected_].gunRange;
}

bool Level::reloadSelected() {
    if (selected_ < 0 || userLocked()) return false;
    Instance& h = instances_[selected_];
    if (h.ai == Instance::AI::Dead || h.hidden || h.transition >= 0 || h.ammo >= h.maxAmmo || h.gunRange <= 0) return false;
    h.path.clear();
    h.route.clear();
    h.order = Action::None;
    playOnce(h, h.prone ? kAnimReloadProne : kAnimReload);
    h.pending = kEffectReload;
    return true;
}

int Level::selectedAmmo(int* maxAmmo) const {
    if (selected_ < 0) return 0;
    if (maxAmmo) *maxAmmo = instances_[selected_].maxAmmo;
    return instances_[selected_].ammo;
}

// Something the selected hero can attack at a world point: enemies first, then civilians and
// animals, then scripted objects. Returns an instance index or -1.
int Level::pickTarget(float wx, float wy, float slack) const {
    int e = pickEnemy(wx, wy, slack);
    if (e >= 0) return e;
    int best = -1;
    float bestD = 1e30f;
    for (size_t i = 0; i < instances_.size(); ++i) {
        const auto& in = instances_[i];
        if (in.hidden || in.building >= 0 || in.ai == Instance::AI::Dead || in.el.faction == Faction::Hero) continue;
        const bool object = in.script >= 0 && in.el.kind != LevelElement::Actor;
        const bool person = in.el.kind == LevelElement::Actor && (in.el.faction == Faction::Civilian || in.el.faction == Faction::Enemy);
        if (!object && !person) continue;
        const SDL_Rect& w = in.world;
        if (wx < w.x - slack || wx > w.x + w.w + slack || wy < w.y - slack || wy > w.y + w.h + slack) continue;
        float cx = w.x + w.w * 0.5f, cy = w.y + w.h * 0.5f;
        float d = (wx - cx) * (wx - cx) + (wy - cy) * (wy - cy) + (object ? 0 : 400);  // prefer objects under the finger
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    return best;
}

bool Level::orderAttack(Action a, int targetIdx) {
    if (!selectedCan(a) || userLocked() || targetIdx < 0 || targetIdx >= (int)instances_.size()) return false;
    Instance& h = instances_[selected_];
    const Instance& e = instances_[targetIdx];
    if (e.ai == Instance::AI::Dead || e.hidden || &e == &h) return false;
    if (a == Action::Gun && e.ai == Instance::AI::KO) return false;  // no point shooting someone out cold
    h.untouched = false;
    h.order = a;
    h.orderTarget = targetIdx;
    h.orderRepath = 0;
    h.path.clear();
    h.route.clear();
    return true;
}

void Level::drawRange(SDL_Renderer* r, const Camera& cam, float range, SDL_Color c) {
    float x, y;
    if (!selectedPosition(x, y) || range <= 0) return;
    drawEllipse(r, cam, x, y, range, range * 0.5f, c);
}

// Runs every frame for heroes with an attack order.
void Level::updateHeroOrder(Instance& h, int idx, float dt) {
    if (h.order == Action::None || h.transition >= 0) return;
    if (h.orderTarget < 0 || h.orderTarget >= (int)instances_.size()) { h.order = Action::None; return; }
    Instance& e = instances_[h.orderTarget];
    const bool object = e.el.kind != LevelElement::Actor;
    if (e.ai == Instance::AI::Dead || e.hidden || (h.order == Action::Gun && e.ai == Instance::AI::KO)) {
        h.order = Action::None;
        h.path.clear();
        return;
    }
    const float tx = e.x, ty = e.y;
    const float d = groundDist(h.x, h.y, tx, ty);
    bool ready;
    if (h.order == Action::Gun)
        ready = d <= h.gunRange && (object || lineOfSight(h.x, h.y, tx, ty, kHeightStanding));
    else if (h.order == Action::Throw)
        ready = d <= kThrowRange && (object || lineOfSight(h.x, h.y, tx, ty, kHeightStanding));
    else
        ready = d <= kMeleeRange + (object ? 14.0f : 0.0f);

    if (!ready) {  // walk (or crawl) closer, re-planning while the target moves
        h.orderRepath -= dt;
        if (h.path.empty() || h.orderRepath <= 0) {
            h.orderRepath = 0.5f;
            if (h.floor < (int)nav_.size()) {
                std::vector<SDL_FPoint> p;
                if (nav_[h.floor].findPath({h.x, h.y}, {tx, ty}, p) && !p.empty()) {
                    h.path = std::move(p);
                    h.pathIdx = 0;
                } else {
                    h.order = Action::None;  // unreachable
                }
            }
        }
        return;
    }
    h.path.clear();
    h.pathIdx = 0;
    setAnim(h, h.prone ? 7 : kAnimIdle, dirTowards(tx - h.x, ty - h.y));

    if (h.order == Action::Gun) {
        if (h.ammo <= 0) {  // reload first
            playOnce(h, h.prone ? kAnimReloadProne : kAnimReload);
            h.pending = kEffectReload;
            return;
        }
        if (!h.prone && !h.drawn) {
            h.drawn = true;
            playOnce(h, kAnimDraw);
            return;
        }
        playOnce(h, h.prone ? kAnimProneShoot : kAnimShoot);
        h.pending = kEffectShot;
        h.pendingTarget = h.orderTarget;
        h.order = Action::None;  // one shot per command
        --h.ammo;
        noise(h.x, h.y, kGunNoise, idx);
        return;
    }
    if (h.order == Action::Throw) {
        playOnce(h, kAnimThrow);
        h.pending = kEffectThrow;
        h.pendingTarget = h.orderTarget;
        h.order = Action::None;
        return;
    }
    // melee: Cooper's knife kills, punches and kicks knock out
    bool kills = h.order == Action::Knife;
    int anim = kills ? kAnimKnife : meleeFor(h, kills);
    if (anim < 0) { h.order = Action::None; return; }
    playOnce(h, anim);
    h.pending = kills ? kEffectKill : kEffectKO;
    h.pendingTarget = h.orderTarget;
    h.order = Action::None;
}

// Called when a hero's one-shot animation (shot, stab, punch, reload) has finished.
void Level::applyPending(Instance& h) {
    const int effect = h.pending;
    h.pending = 0;
    if (effect == kEffectReload) { h.ammo = h.maxAmmo; return; }
    if (h.pendingTarget < 0 || h.pendingTarget >= (int)instances_.size()) return;
    Instance& e = instances_[h.pendingTarget];
    const int heroIdx = (int)(&h - &instances_[0]);
    if (e.ai == Instance::AI::Dead) return;
    if (e.el.kind != LevelElement::Actor) {  // scripted object: its script decides what happens
        const char* ev = effect == kEffectShot ? "Shooted" : effect == kEffectKill ? "Stabbed" : effect == kEffectThrow ? "Dagger" : "Hit";
        if (e.script >= 0) callScript(e.script, ev, {actorHandle(h)});
        return;
    }
    if (effect == kEffectKill || effect == kEffectThrow) { killEnemy(e); return; }
    if (effect == kEffectKO) { knockOut(e); npcEvent(e, 17, &h); return; }
    if (effect == kEffectShot) {
        const float d = groundDist(h.x, h.y, e.x, e.y);
        float chance = std::clamp(1.05f - 0.6f * d / std::max(1.0f, h.gunRange), 0.2f, 0.95f);
        if (e.prone) chance *= 0.7f;
        if (frand() < chance) hurtEnemy(e, 55 + std::rand() % 45, heroIdx);
        else alertTo(e, heroIdx, h.x, h.y);  // missed: he knows where the shot came from
    }
}

void Level::killEnemy(Instance& e) {
    e.health = 0;
    e.ai = Instance::AI::Dead;
    e.path.clear();
    e.order = Action::None;
    e.pending = 0;
    e.discovered = false;
    e.prone = false;
    playOnce(e, kAnimDie, true);
}

void Level::knockOut(Instance& e) {
    if (e.ai == Instance::AI::Dead) return;
    if (e.ai == Instance::AI::None) e.ai = Instance::AI::Calm;  // civilians get up again too
    e.ai = Instance::AI::KO;
    e.koT = 25.0f + frand() * 10.0f;
    e.path.clear();
    e.discovered = false;
    playOnce(e, kAnimKO, true);
}

void Level::hurtEnemy(Instance& e, int damage, int heroIdx) {
    e.health -= damage;
    if (e.health <= 0) { killEnemy(e); return; }
    playOnce(e, kAnimImpact);
    alertTo(e, heroIdx, instances_[heroIdx].x, instances_[heroIdx].y);
}

bool Level::debugNearestEnemy(float x, float y, float& ex, float& ey, int skip) const {
    std::vector<std::pair<float, int>> list;
    for (size_t i = 0; i < instances_.size(); ++i) {
        const auto& e = instances_[i];
        if (e.el.faction != Faction::Enemy || e.ai == Instance::AI::Dead || e.ai == Instance::AI::None || e.hidden) continue;
        list.push_back({groundDist(x, y, e.x, e.y), (int)i});
    }
    std::sort(list.begin(), list.end());
    if (skip >= (int)list.size()) return false;
    const auto& e = instances_[list[skip].second];
    ex = e.x;
    ey = e.y - e.world.h * 0.4f;
    return true;
}

void Level::debugState() const {
    static const char* names[] = {"None", "Calm", "Suspicious", "Alert", "Searching", "KO", "Dead"};
    for (const auto& e : instances_) {
        if (e.el.faction == Faction::Hero && e.el.kind == LevelElement::Actor)
            SDL_Log("  hero %s at %.0f,%.0f health %d ammo %d", e.el.set.c_str(), e.x, e.y, e.health, e.ammo);
        if (e.el.faction == Faction::Enemy && e.ai != Instance::AI::Calm && e.ai != Instance::AI::None)
            SDL_Log("  enemy %s at %.0f,%.0f %s health %d", e.el.set.c_str(), e.x, e.y, names[(int)e.ai], e.health);
    }
}

bool Level::debugAttackElement(int elem, Action a) {
    if (elem < 0 || elem >= (int)elemInst_.size() || elemInst_[elem] < 0) return false;
    if (selected_ < 0) selected_ = firstHero();
    bool ok = orderAttack(a, elemInst_[elem]);
    SDL_Log("debug: attack element %d with action %d: %s", elem, (int)a, ok ? "ordered" : "refused");
    return ok;
}
