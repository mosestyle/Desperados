// Items and horses.
//
// Items are the ELEM records of accessories.dvf (the element's sub byte is the item type):
// either in a hero's bag with a count (Cooper's knife and watch, Sam's dynamite, Doc's flasks...)
// or lying on the map (the saddle of mission 1, pick-ups, saddles already on horses). Every item
// gets an instance; a bag item's instance stays hidden until it is on the map (a thrown knife,
// the watch put down).
//
// Cooper's watch: put down, it plays its tune for a while; NPCs within earshot walk over and
// listen (their script gets ActionChange(3)), which leaves their back free.
// Horses: a hero carrying a saddle saddles a horse, then mounts it (posture 1, "on horseback");
// the horse and the saddle follow the rider, using the matching trot / gallop animations.
#include <algorithm>
#include <cmath>

#include "Level.h"

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr int kItemDynamite = 1, kItemScarecrow = 2, kItemTequila = 3, kItemSnake = 4, kItemWatch = 5, kItemCard = 6,
              kItemKnife = 7, kItemTnt = 8, kItemFlasks = 9, kItemPeanuts = 10, kItemSaddle = 11, kItemStone = 13,
              kItemFlash = 14, kItemMedikit = 19, kItemSnipes = 21;
constexpr int kSubCooper = 1, kSubDoc = 2, kSubSam = 3, kSubKate = 4, kSubSanchez = 5, kSubMia = 6;
// hero animations
constexpr int kAnimIdle = 0, kAnimWalk = 3, kAnimRun = 5, kAnimProne = 7, kAnimCrawl = 9, kAnimMount = 15,
              kAnimRideIdle = 16, kAnimTrot = 18, kAnimGallop = 19, kAnimDismount = 20, kAnimWatchPut = 38,
              kAnimWatchTake = 39, kAnimCarryWalk = 42, kAnimSaddleUp = 123, kAnimTakeKnife = 162, kAnimTakeObject = 183;
// horse and item animations
constexpr int kHorseIdle = 0, kHorseTrot = 3, kHorseGallop = 5;
constexpr int kItemLying = 123, kKnifeLying = 113, kSaddleOnGround = 7, kSaddleCarried = 108;
constexpr float kReach = 20.0f;
constexpr float kWatchDelay = 2.0f, kWatchRing = 14.0f, kWatchHear = 260.0f;
constexpr int kNpcLookDown = 103;

float groundDist(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = 2.0f * (by - ay);
    return std::sqrt(dx * dx + dy * dy);
}
int dirTowards(float dx, float dy) {
    float a = std::atan2(dx, -2.0f * dy);
    int d = (int)std::lround(a / (kPi / 8.0f));
    return (d % 16 + 16) % 16;
}
// Whose bag an item of the inventory belongs in.
int ownerOf(int type) {
    switch (type) {
    case kItemKnife: case kItemWatch: return kSubCooper;
    case kItemDynamite: case kItemSnake: case kItemTnt: return kSubSam;
    case kItemScarecrow: case kItemFlasks: case kItemMedikit: case kItemSnipes: return kSubDoc;
    case kItemCard: return kSubKate;
    case kItemTequila: case kItemStone: return kSubSanchez;
    case kItemPeanuts: case kItemFlash: return kSubMia;
    default: return -1;
    }
}
// Pick-ups lying around and what they refill.
int bagTypeOf(int type) {
    switch (type) {
    case 16: return kItemDynamite;
    case 17: return kItemFlasks;
    case 18: return kItemFlash;
    case 20: return kItemPeanuts;
    case 22: return kItemTequila;
    default: return type;
    }
}
bool isHorse(const LevelElement& el) { return el.kind == LevelElement::Actor && el.cls == 2 && el.sub == 1; }
}  // namespace

void Level::initItems() {
    for (size_t i = 0; i < instances_.size(); ++i) {
        Instance& it = instances_[i];
        if (it.itemType < 0) continue;
        if (it.el.carried) {  // in a hero's bag
            const int h = heroBySub(ownerOf(it.itemType));
            if (h >= 0) instances_[h].bag[it.itemType] += it.el.count;
            it.hidden = true;
        } else if (it.itemType == kItemSaddle && it.el.link >= 0 && it.el.link < (int)elemInst_.size() && elemInst_[it.el.link] >= 0) {
            Instance& horse = instances_[elemInst_[it.el.link]];  // already on a horse
            horse.saddle = (int)i;
            it.onHorse = elemInst_[it.el.link];
            it.x = horse.x;
            it.y = horse.y;
            it.floor = horse.floor;
        }
        const int anim = it.itemType == kItemSaddle ? (it.onHorse >= 0 ? kHorseIdle : kSaddleOnGround)
                         : it.itemType == kItemKnife ? kKnifeLying : kItemLying;
        it.anim = -1;
        setAnim(it, anim, it.onHorse >= 0 ? instances_[it.onHorse].dir : 0);
        updateFrameRect(it);
    }
}

int Level::bagCount(int heroIdx, int type) const {
    if (heroIdx < 0 || heroIdx >= (int)instances_.size()) return 0;
    auto it = instances_[heroIdx].bag.find(type);
    return it == instances_[heroIdx].bag.end() ? 0 : it->second;
}

int Level::restAnim(const Instance& in) const {
    if (in.horse >= 0) return kAnimRideIdle;
    if (in.prone) return kAnimProne;
    return kAnimIdle;
}

int Level::moveAnim(const Instance& in) const {
    if (in.horse >= 0) return in.running ? kAnimGallop : kAnimTrot;
    if (in.prone) return kAnimCrawl;
    if (in.carrying >= 0 && in.set->find(kAnimCarryWalk, in.dir) && in.set->find(kAnimCarryWalk, in.dir)->anim == kAnimCarryWalk)
        return kAnimCarryWalk;
    return in.running ? kAnimRun : kAnimWalk;
}

int Level::watchRinging(const Instance& w) const {
    return w.itemType == kItemWatch && !w.hidden && w.itemT >= kWatchDelay && w.itemT < kWatchDelay + kWatchRing ? 1 : 0;
}

// An item lying on the map, or a horse, under a world point.
int Level::pickItem(float wx, float wy, float slack) const {
    int best = -1;
    float bestD = 1e30f;
    for (size_t i = 0; i < instances_.size(); ++i) {
        const Instance& in = instances_[i];
        const bool item = in.itemType >= 0 && in.onHorse < 0 && in.carrier < 0;
        const bool horse = isHorse(in.el) && in.rider < 0 && in.ai != Instance::AI::Dead;
        if (in.hidden || (!item && !horse)) continue;
        const float s = item ? slack + 8 : slack;  // items are small
        const SDL_Rect& w = in.world;
        if (wx < w.x - s || wx > w.x + w.w + s || wy < w.y - s || wy > w.y + w.h + s) continue;
        const float cx = w.x + w.w * 0.5f, cy = w.y + w.h * 0.5f;
        const float d = (wx - cx) * (wx - cx) + (wy - cy) * (wy - cy) + (item ? 0 : 900);
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    return best;
}

bool Level::orderUse(int idx) {
    if (selected_ < 0 || userLocked() || idx < 0 || idx >= (int)instances_.size()) return false;
    Instance& h = instances_[selected_];
    const Instance& t = instances_[idx];
    if (h.ai == Instance::AI::Dead || h.building >= 0 || h.horse >= 0 || h.transition >= 0) return false;
    if (isHorse(t.el)) {
        if (t.saddle < 0 && !(h.carrying >= 0 && instances_[h.carrying].itemType == kItemSaddle)) return false;
    } else if (t.itemType == kItemSaddle) {
        if (h.el.sub != kSubCooper || h.carrying >= 0) return false;  // only Cooper carries saddles
    } else if (ownerOf(bagTypeOf(t.itemType)) >= 0 && ownerOf(bagTypeOf(t.itemType)) != h.el.sub) {
        return false;  // somebody else's tool
    }
    h.untouched = false;
    h.order = Action::PickUp;
    h.orderTarget = idx;
    h.orderRepath = 0;
    h.path.clear();
    h.route.clear();
    if (h.prone) startTransition(h, false);
    target_ = {t.x, t.y};
    targetFade_ = 1.0f;
    return true;
}

bool Level::orderWatch(float wx, float wy) {
    if (selected_ < 0 || userLocked() || !selectedCan(Action::Watch)) return false;
    Instance& h = instances_[selected_];
    const int floor = floorForPoint(wx, wy, h.floor);
    if (floor != h.floor || !nav_[h.floor].walkableAt(wx, wy)) return false;
    h.untouched = false;
    h.order = Action::Watch;
    h.orderTarget = -1;
    h.orderX = wx;
    h.orderY = wy;
    h.orderRepath = 0;
    h.path.clear();
    h.route.clear();
    target_ = {wx, wy};
    targetFade_ = 1.0f;
    return true;
}

bool Level::selectedMounted() const {
    return selected_ >= 0 && selected_ < (int)instances_.size() && instances_[selected_].horse >= 0;
}

bool Level::dismountSelected() {
    if (!selectedMounted() || userLocked()) return false;
    Instance& h = instances_[selected_];
    if (h.transition >= 0) return false;
    h.path.clear();
    h.route.clear();
    h.order = Action::None;
    playOnce(h, kAnimDismount);
    h.pending = -1;  // see useTarget(): finishes the dismount
    return true;
}

void Level::dropItem(int item, float x, float y, int floor) {
    if (item < 0 || item >= (int)instances_.size()) return;
    Instance& it = instances_[item];
    it.hidden = false;
    it.carrier = -1;
    it.onHorse = -1;
    it.itemT = 0;
    it.x = x;
    it.y = y;
    it.floor = floor;
    if (floor < (int)nav_.size() && !nav_[floor].walkableAt(x, y)) {
        SDL_FPoint p;
        if (nav_[floor].nearestWalkable({x, y}, p, 6)) { it.x = p.x; it.y = p.y; }
    }
    it.anim = -1;
    setAnim(it, it.itemType == kItemSaddle ? kSaddleOnGround : it.itemType == kItemKnife ? kKnifeLying : kItemLying, 0);
    updateFrameRect(it);
}

// Runs the hero's PickUp / Ride / Watch order (called from updateHeroOrder). Returns true when
// the order is over (done or impossible).
bool Level::useTarget(Instance& h, int heroIdx, float dt) {
    (void)heroIdx;
    float tx, ty;
    if (h.order == Action::Watch) { tx = h.orderX; ty = h.orderY; }
    else {
        if (h.orderTarget < 0 || h.orderTarget >= (int)instances_.size()) return true;
        const Instance& t = instances_[h.orderTarget];
        if (t.hidden || t.carrier >= 0 || (isHorse(t.el) && t.rider >= 0)) return true;
        tx = t.x;
        ty = t.y;
    }
    const float reach = h.order == Action::Watch ? 6.0f : kReach;
    if (groundDist(h.x, h.y, tx, ty) > reach) {
        h.orderRepath -= dt;
        if (h.path.empty() || h.orderRepath <= 0) {
            h.orderRepath = 1.0f;
            std::vector<SDL_FPoint> p;
            if (h.floor >= (int)nav_.size() || !nav_[h.floor].findPath({h.x, h.y}, {tx, ty}, p) || p.empty()) return true;
            h.path = std::move(p);
            h.pathIdx = 0;
        }
        return false;
    }
    h.path.clear();
    h.pathIdx = 0;
    setAnim(h, restAnim(h), dirTowards(tx - h.x, ty - h.y));
    if (h.order == Action::Watch) {
        playOnce(h, kAnimWatchPut);
        h.pending = -2;
        return true;
    }
    const Instance& t = instances_[h.orderTarget];
    int anim = kAnimTakeObject;
    if (isHorse(t.el)) anim = t.saddle < 0 ? kAnimSaddleUp : kAnimMount;
    else if (t.itemType == kItemWatch) anim = kAnimWatchTake;
    else if (t.itemType == kItemKnife) anim = kAnimTakeKnife;
    else if (t.itemType == kItemSaddle) anim = kAnimSaddleUp;
    if (anim == kAnimMount) {  // climb on: stand where the horse stands
        h.x = t.x;
        h.y = t.y;
        h.dir = t.dir;
        h.anim = -1;
    }
    playOnce(h, anim);
    h.pending = -3;
    h.pendingTarget = h.orderTarget;
    return true;
}

// The end of a hero's item / horse animation (pending -1 dismount, -2 watch down, -3 take / ride).
void Level::updateItems(float dt) {
    for (size_t i = 0; i < instances_.size(); ++i) {
        Instance& in = instances_[i];
        if (in.itemType == kItemWatch && !in.hidden) in.itemT += dt;
        if (in.horse >= 0) followRider(in);
        if (in.carrying >= 0) {  // the saddle in Cooper's arms
            Instance& s = instances_[in.carrying];
            s.x = in.x;
            s.y = in.y;
            s.floor = in.floor;
            s.hidden = in.hidden || in.building >= 0;
            setAnim(s, kSaddleCarried, in.dir);
            updateFrameRect(s);
        }
    }
}

void Level::followRider(Instance& r) {
    Instance& horse = instances_[r.horse];
    horse.x = r.x;
    horse.y = r.y;
    horse.floor = r.floor;
    horse.hidden = r.hidden;
    const int ra = r.transition >= 0 ? kAnimRideIdle : r.anim;
    const int ha = ra == kAnimGallop ? kHorseGallop : ra == kAnimTrot ? kHorseTrot : kHorseIdle;
    setAnim(horse, ha, r.dir);
    if (ha != kHorseIdle && !horse.rec->entries.empty()) horse.entry = r.entry % (int)horse.rec->entries.size();
    updateFrameRect(horse);
    if (horse.saddle >= 0) {
        Instance& s = instances_[horse.saddle];
        s.x = r.x;
        s.y = r.y;
        s.floor = r.floor;
        s.hidden = r.hidden;
        setAnim(s, ra == kAnimGallop ? kAnimGallop : ra == kAnimTrot ? kAnimTrot : kHorseIdle, r.dir);
        if (!s.rec->entries.empty()) s.entry = horse.entry % (int)s.rec->entries.size();
        updateFrameRect(s);
    }
}

// NPC behaviour around a ringing watch. Returns true while the NPC is busy with it.
bool Level::updateLure(Instance& e, float dt) {
    if (e.lure < 0) {
        for (size_t i = 0; i < instances_.size(); ++i) {
            const Instance& w = instances_[i];
            if (!watchRinging(w) || w.floor != e.floor || groundDist(e.x, e.y, w.x, w.y) > kWatchHear) continue;
            e.lure = (int)i;
            e.lureT = 0;
            const float a = (float)((size_t)&e % 16) * kPi / 8;  // don't all stand on the same spot
            walkTo(e, w.x + std::cos(a) * 14, w.y + std::sin(a) * 7, false, false);
            if (e.script >= 0) callScript(e.script, "ActionChange", {3});
            return true;
        }
        return false;
    }
    Instance& w = instances_[e.lure];
    if (w.hidden || w.itemType != kItemWatch) { e.lure = -1; e.atWaypoint = false; return false; }
    if (!e.path.empty()) return true;
    setAnim(e, e.set->find(kNpcLookDown, e.dir) && e.set->find(kNpcLookDown, e.dir)->anim == kNpcLookDown ? kNpcLookDown : kAnimIdle,
            dirTowards(w.x - e.x, w.y - e.y));
    if (!watchRinging(w)) {
        e.lureT += dt;
        if (e.lureT > 2.5f) {  // the tune is over: back to work
            e.lure = -1;
            e.atWaypoint = false;
            setAnim(e, kAnimIdle, e.dir);
            if (e.script >= 0) callScript(e.script, "ActionChange", {0});
            return false;
        }
    }
    return true;
}
