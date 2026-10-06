// Hero actions: shooting (each hero's own gun from weapons.dat), knife / punch / kick.
// The hero walks until the target is in range (and in sight for guns), then plays the
// original animations; the effect is applied when the animation ends.
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "Level.h"

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr int kAnimIdle = 0, kAnimProneShoot = 8, kAnimImpact = 25, kAnimKO = 30, kAnimDie = 33, kAnimDraw = 27,
              kAnimShoot = 28, kAnimKnife = 35, kAnimPunch = 36, kAnimKick = 70, kAnimReload = 136,
              kAnimReloadProne = 137;
constexpr float kMeleeRange = 22.0f;
constexpr float kGunNoise = 420.0f;      // ground px: enemies in this radius hear a shot
constexpr float kRangeScale = 0.6f;      // weapons.dat ranges -> ground pixels
constexpr float kHeightStanding = 45.0f;
constexpr int kEffectShot = 1, kEffectKill = 2, kEffectKO = 3, kEffectReload = 4;

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
        // melee: Cooper's knife kills silently, Kate's kick and punches knock out
        if (id == 1 && hasAnim(h.set, kAnimKnife)) { h.meleeAnim = kAnimKnife; h.meleeKills = true; }
        else if (hasAnim(h.set, kAnimKick)) h.meleeAnim = kAnimKick;
        else if (hasAnim(h.set, kAnimPunch)) h.meleeAnim = kAnimPunch;
    }
}

bool Level::selectedCan(Action a) const {
    if (selected_ < 0 || selected_ >= (int)instances_.size()) return false;
    const Instance& h = instances_[selected_];
    if (h.ai == Instance::AI::Dead) return false;
    if (a == Action::Gun) return h.gunRange > 0;
    if (a == Action::Melee) return h.meleeAnim >= 0;
    return false;
}

int Level::selectedActionAnim(Action a) const {
    if (selected_ < 0) return -1;
    const Instance& h = instances_[selected_];
    return a == Action::Gun ? kAnimShoot : (a == Action::Melee ? h.meleeAnim : -1);
}

float Level::selectedGunRange() const {
    return selected_ >= 0 ? instances_[selected_].gunRange : 0;
}

int Level::selectedAmmo(int* maxAmmo) const {
    if (selected_ < 0) return 0;
    if (maxAmmo) *maxAmmo = instances_[selected_].maxAmmo;
    return instances_[selected_].ammo;
}

bool Level::orderAttack(Action a, int enemyIdx) {
    if (!selectedCan(a) || enemyIdx < 0 || enemyIdx >= (int)instances_.size()) return false;
    Instance& h = instances_[selected_];
    const Instance& e = instances_[enemyIdx];
    if (e.ai == Instance::AI::Dead) return false;
    if (a == Action::Gun && e.ai == Instance::AI::KO) return false;  // no point shooting someone out cold
    h.untouched = false;
    h.order = a;
    h.orderTarget = enemyIdx;
    h.orderRepath = 0;
    h.path.clear();
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
    if (e.ai == Instance::AI::Dead || (h.order == Action::Gun && e.ai == Instance::AI::KO)) {
        h.order = Action::None;
        h.path.clear();
        return;
    }
    const float d = groundDist(h.x, h.y, e.x, e.y);
    bool ready;
    if (h.order == Action::Gun)
        ready = d <= h.gunRange && lineOfSight(h.x, h.y, e.x, e.y, kHeightStanding);
    else
        ready = d <= kMeleeRange;

    if (!ready) {  // walk (or crawl) closer, re-planning while the target moves
        h.orderRepath -= dt;
        if (h.path.empty() || h.orderRepath <= 0) {
            h.orderRepath = 0.5f;
            if (h.floor < (int)nav_.size()) {
                std::vector<SDL_FPoint> p;
                if (nav_[h.floor].findPath({h.x, h.y}, {e.x, e.y}, p) && !p.empty()) {
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
    setAnim(h, h.prone ? 7 : kAnimIdle, dirTowards(e.x - h.x, e.y - h.y));

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
    // melee
    playOnce(h, h.meleeAnim);
    h.pending = h.meleeKills ? kEffectKill : kEffectKO;
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
    if (effect == kEffectKill) { killEnemy(e); return; }
    if (effect == kEffectKO) { knockOut(e); return; }
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
    playOnce(e, kAnimDie, true);
}

void Level::knockOut(Instance& e) {
    if (e.ai == Instance::AI::Dead) return;
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
        if (e.el.faction != Faction::Enemy || e.ai == Instance::AI::Dead || e.ai == Instance::AI::None) continue;
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
