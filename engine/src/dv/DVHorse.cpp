// Horses (Game/DVElementActorHorse.cpp and the riding parts of DVElementActorHuman): a rider
// sits on his horse (posture 1, rider +0x134 / horse +0x270). The horse is the one that walks
// (its walk 3 and gallop 5); the rider follows it and plays his riding animation in step with
// it (0x10 waiting, 0x12 walking, 0x13 galloping), the horse's saddle (+0x274) too.
#include <cmath>

#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVScript.h"
#include "dv/DVSector.h"
#include "dv/DVSequence.h"

DVElementActor* DVElementActor::Horse() const {
    auto* h = dynamic_cast<DVElementActor*>(horse);
    return h && h->rider == this ? h : nullptr;
}

// DVElement::CopyPositionMapEtc: the same place, layer, area, obstacle and direction
void DVElementActor::CopyPositionFrom(const DVElement* e) {
    if (!e || !e->sprite) return;
    DVPositionInterface& pos = sprite->pos;
    const DVPositionInterface& src = e->sprite->pos;
    pos.SetPositionMap(src.posMap);
    pos.layer = src.layer;
    pos.sector = src.sector;
    pos.SetObstacle(src.obstacle);
    pos.direction = src.direction;
    pos.directionWanted = src.directionWanted;
    pos.ComputePositionAll();
}

void DVElementActor::SitOnHorse(DVElementActor* h) {
    if (!h) return;
    horse = h;
    h->rider = this;
    h->horseUsed = true;
    defaultCommand = CMD_RIDE_WAIT;
    SetStates(POSTURE_RIDING, 0);
    CopyPositionFrom(h);
    sprite->displayOrder = h->sprite->displayOrder + 1.0f;
}

// a walk to the horse's side (DVScript::RecordMountHorse: AppendMoveToSequence to the horse),
// then the mount
bool DVElementActor::BuildMount(DVElementActor* h, std::deque<DVOrder>& out) const {
    if (!h || !h->sprite || !canRide || (h->rider && h->rider != this)) return false;
    const DVPositionInterface& hp = h->sprite->pos;
    // beside it, on its left
    SBGeoVector2D side = SBSetSector0to15((unsigned)(hp.direction + 12) & 0xf, 1.0f);
    SBGeoPoint2D at = hp.posMap + SBGeoVector2D(side.x * 28.0f, side.y * 16.0f);
    DVFastFindGrid* grid = sprite->grid;
    if (grid) {
        SBGeoBoundingBox2D box = sprite->pos.GetMoveBox(POSTURE_STANDING) + at;
        if (grid->FindAutorizedPosition(box, at, hp.layer)) at = SBCenter(box);
    }
    SBGeoVector2D d = at - sprite->pos.posMap;
    if (std::fabs(d.x) > 3.0f || std::fabs(d.y) > 3.0f) {
        auto* area = dynamic_cast<DVSectorMotionArea*>(hp.sector);
        if (!BuildMove(at, CMD_WALK, POSTURE_STANDING, out, area)) return false;
    }
    DVOrder o;
    o.id = DVOrder::NewID();
    o.command = CMD_MOUNT;
    o.target = h;
    out.push_back(o);
    return true;
}

void DVElementActor::OrderDismount() {
    if (!Horse()) return;
    if (scriptElement) EndScriptElement(false);
    orders.clear();
    DVOrder o;
    o.id = DVOrder::NewID();
    o.command = CMD_DISMOUNT;
    orders.push_back(o);
    newOrder = true;
}

static uint32_t RideAnimation(const DVElementActor* h) {
    if (h->orders.empty()) return CMD_RIDE_WAIT;
    const DVOrder& o = h->orders.front();
    if (o.command == CMD_RUN || (o.command == CMD_MOVE_ANIM && o.anim == CMD_RUN)) return CMD_RIDE_GALLOP;
    if (o.command == CMD_WALK || o.command == CMD_CRAWL || o.command == CMD_TURN || o.command == CMD_MOVE_ANIM)
        return CMD_RIDE_WALK;
    return CMD_RIDE_WAIT;
}

// plays `anim` on the sprite at the frame matching the leader's progress in his row
static void SyncFrame(DVSprite* s, uint32_t anim, const DVSprite* leader) {
    if (!s->HasAnimation(anim)) return;
    s->row = (uint16_t)(s->animRows[anim] + s->pos.direction);
    const DVspriteScript* r = s->CurrentRow();
    const DVspriteScript* lr = leader->CurrentRow();
    unsigned n = r ? (unsigned)r->frames.size() : 0;
    unsigned ln = lr ? (unsigned)lr->frames.size() : 0;
    if (n == 0) return;
    unsigned e = ln ? (unsigned)leader->entry * n / ln : 0;
    s->entry = (uint16_t)(e < n ? e : n - 1);
    s->animation = anim;
}

void DVElementActor::RideSync() {
    DVElementActor* h = Horse();
    if (!h) return;
    CopyPositionFrom(h);
    sprite->displayOrder = h->sprite->displayOrder + 1.0f;
    uint32_t a = RideAnimation(h);
    if (!sprite->HasAnimation(a)) a = CMD_RIDE_WAIT;
    SyncFrame(sprite.get(), a, h->sprite.get());
    sprite->orderId = 0xffffffff;  // (a later action starts afresh)
}

// DVElementActorHorse::Hourglass / Execute: the horse does its orders (or waits); the saddle
// follows it: 0 standing, 0x12 walking, 0x13 galloping
void DVElementActor::HorseHourglass() {
    StepOrders();
    if (saddle && saddle->sprite) {
        DVSprite* ss = saddle->sprite.get();
        const DVPositionInterface& hp = sprite->pos;
        ss->pos.SetPositionMap(hp.posMap);
        ss->pos.layer = hp.layer;
        ss->pos.sector = hp.sector;
        ss->pos.SetObstacle(hp.obstacle);
        ss->pos.direction = hp.direction;
        ss->pos.ComputePositionAll();
        uint32_t c = CurrentCommand();
        bool moving = sprite->pos.incMap.x != 0.0f || sprite->pos.incMap.y != 0.0f;
        uint32_t a = c == CMD_RUN && moving ? 0x13u : (c == CMD_WALK || c == CMD_TURN) && moving ? 0x12u : 0u;
        if (!ss->HasAnimation(a)) a = 0;
        SyncFrame(ss, a, sprite.get());
        if (auto* o = dynamic_cast<DVElementObject*>(saddle)) o->animation = a;
        ss->displayOrder = sprite->displayOrder + 0.5f;
        saddle->active = active;
    }
    if (rider && rider->Horse() == this && rider->orders.empty()) rider->RideSync();
}

// ---- carrying bodies (DVCooper::Translate / Execute 0x29, 0x2a, 0x2b) --------------------------

bool DVElementActor::IsCarriable(bool scripted) const {
    if ((classFlags & 3) != 3 || carrier || carried || Horse() || InBuilding()) return false;
    if (scripted) return true;
    if (!active) return false;
    if (!dead && !knockedOut) return false;
    return sprite->pos.posture == POSTURE_LYING || sprite->pos.posture == 0xe || dead;
}

bool DVElementActor::BuildTakeBody(DVElementActor* body, std::deque<DVOrder>& out, bool scripted) const {
    if (!body || body == this || !CanCarry() || carried || Horse() || !body->IsCarriable(scripted)) return false;
    SBGeoPoint2D at = body->sprite->pos.posMap;
    SBGeoVector2D d = at - sprite->pos.posMap;
    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len > 20.0f) {
        // DVScript::RecordTakeCorpse: up to the body, the pick-up's reach (about 14) short of it
        SBGeoPoint2D near = at - d * (14.0f / len);
        DVFastFindGrid* grid = sprite->grid;
        if (grid) {
            SBGeoBoundingBox2D box = sprite->pos.GetMoveBox(POSTURE_STANDING) + near;
            if (grid->FindAutorizedPosition(box, near, body->sprite->pos.layer)) near = SBCenter(box);
        }
        if (!BuildMove(near, CMD_WALK, POSTURE_STANDING, out, dynamic_cast<DVSectorMotionArea*>(body->sprite->pos.sector)))
            return false;
    }
    DVOrder o;
    o.id = DVOrder::NewID();
    o.command = CMD_TAKE_BODY;
    o.target = body;
    o.goal = at;
    o.anim = scripted ? 1 : 0;
    out.push_back(o);
    return true;
}

void DVElementActor::OrderDropBody() {
    if (!carried) return;
    if (scriptElement) EndScriptElement(false);
    orders.clear();
    DVOrder o;
    o.id = DVOrder::NewID();
    o.command = CMD_DROP_BODY;
    orders.push_back(o);
    newOrder = true;
}

// ---- Mia's barrel ------------------------------------------------------------------------------

// DVKung::SetInBarrel: in it (posture 3: no one sees her), where it stands
void DVElementActor::SetInBarrel(DVElement* b) {
    if (!b || !b->sprite) return;
    barrel = b;
    SetStates(3, 0);
    DVPositionInterface& pos = sprite->pos;
    pos.SetPositionMap(b->sprite->pos.posMap);
    pos.ComputePositionAll();
    sprite->ComputeDisplayOrder(b, false);
    defaultCommand = 0x63;
    if (sprite->HasAnimation(0x63)) sprite->row = (uint16_t)sprite->animRows[0x63];
    sprite->entry = 0;
    sprite->animation = 0x63;
}
