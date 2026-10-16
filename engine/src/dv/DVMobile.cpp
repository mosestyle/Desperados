// Mobile elements: see DVMobile.h
#include "dv/DVMobile.h"

#include <cmath>
#include <cstring>

#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVScript.h"
#include "dv/DVSequence.h"
#include "sb/SBFile.h"
#include "sb/SBFile.h"

extern int desprand();

static void MoveMap(DVPositionInterface& p, const SBGeoVector2D& v) {
    p.SetPositionMap(p.posMap + v);
    p.ComputePositionAll();
}

void TranslateObstacle(DVSightObstacle& o, const SBGeoVector2D& v) {
    for (auto& p : o.points) p.x += v.x, p.y += v.y;
    for (auto& p : o.polygon.pts) p = p + v;
    for (SBGeoPlane3D* pl : {&o.plane, &o.planeGround}) {
        for (auto& q : pl->p) q.x += v.x, q.y += v.y;
        pl->zc -= pl->zx * v.x + pl->zy * v.y;
        pl->d -= pl->normal.x * v.x + pl->normal.y * v.y;
    }
    o.box3dMin.x += v.x, o.box3dMin.y += v.y;
    o.box3dMax.x += v.x, o.box3dMax.y += v.y;
    o.box = o.box + v;
    o.boxScreen = o.boxScreen + v;
}

// ---- the parts ---------------------------------------------------------------------------------

DVElementFXMasked::DVElementFXMasked() {
    classFlags = 4;
    shadowKind = 1;
    active = true;
}

// DVElementFXMasked::Refresh (0x0824b5b0): drawn while it and its element are on
void DVElementFXMasked::Refresh(uint32_t screen, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) {
    if (mobile && !mobile->active) return;
    DVElementFX::Refresh(screen, zoom, view, silhouettes);
}

void DVElementFXMasked::Hourglass() {
    if (mobile && mobile->stopped) return;
    if (frameScale != 1.0f) sprite->PerformIncrementModulated(frameScale, 0);
    else sprite->PerformVirginIncrement(0);
}

// ---- the element -------------------------------------------------------------------------------

DVElementMobile::DVElementMobile() {
    classFlags = 2;
    kind = KIND_MOBILE;
}

DVElementMobile::~DVElementMobile() = default;

int DVElementMobile::LoadStateFromFile(SBFile& f) {
    DVFastFindGrid* grid = sprite->grid ? sprite->grid : DVFastFindGrid::mpFastFindGrid;
    int n = 2;
    uint16_t count = f.U16();
    for (uint16_t i = 0; i < count; ++i) {
        auto* p = new DVElementFXMasked();
        f.Skip(2, 1);
        p->kind = KIND_FX;
        int k = p->LoadStateFromFile(f);
        p->mobile = this;
        parts.push_back(p);
        n += 2 + k;
    }
    count = f.U16();
    n += 2;
    for (uint16_t i = 0; i < count; ++i) {
        auto o = std::make_unique<DVSightObstacle>();
        n += o->LoadFromFile(f);
        box.Expand(o->box);
        obstacles.push_back(std::move(o));
    }
    shape.type = 0x42;
    n += shape.LoadFromFile(f);
    // (the shape's repulsive lines and points come with the characters' collisions)
    dust = f.U8() != 0;
    n += 1;
    if (dust) {
        int16_t x = f.S16(), y = f.S16(), z = f.S16();
        dustPoint = SBGeoVector3D((float)x, (float)(y + z), (float)z);
        n += 6;
    }
    shaky = f.U8() != 0;
    pushable = f.U8() != 0;
    n += 2;
    if (pushable) {
        int16_t x = f.S16(), y = f.S16();
        pushPoint = SBGeoPoint2D((float)x, (float)y);
        n += 4;
    }
    uint16_t route = f.U16();
    path.index = route;
    if (route != 0xffff && DVHikingGuide::mpInstance) {
        path.current = 0;
        path.forward = true;
        path.waypoints = DVHikingGuide::mpInstance->Path(route);
    }
    uint16_t at = f.U16();
    u1e8 = f.U16();
    railroad = f.U8() != 0;
    n += 7;
    if (railroad) {
        f1ec = (float)f.U32();
        u1ea = (uint16_t)(f.U16() + 1);
        n += 6;
    }
    path.SetCurrentWaypointIndex(at);
    waypoint = path.Current();
    DVPositionInterface& pos = sprite->pos;
    if (waypoint) {
        pos.layer = waypoint->layer;
        pos.sector = waypoint->sector;
    }
    uint16_t ob = f.U16();
    pos.SetObstacle(ob && grid ? grid->ViewArea(ob) : nullptr);
    SBGeoPoint2D start = waypoint ? SBGeoPoint2D((float)waypoint->x, (float)waypoint->y) : SBGeoPoint2D();
    pos.SetPositionMap(start);
    pos.ComputePositionAll();
    SBGeoVector2D v(start.x, start.y);
    box = box + v;
    for (auto& q : shape.polygon.pts) q = q + v;
    shape.box = shape.box + v;
    pushPoint = pushPoint + v;
    dustPoint.x += start.x;
    dustPoint.y += start.y;
    for (DVElementFXMasked* p : parts) {
        DVPositionInterface& pp = p->sprite->pos;
        pp.SetPosture(0);
        SBGeoPoint2D rel = pp.posMap;
        pp.SetPositionMap(rel + v);
        pp.SetObstacle(pos.obstacle);
        pp.layer = pos.layer;
        pp.sector = pos.sector;
        pp.ComputePositionAll();
        p->sprite->ComputeDisplayOrder(nullptr, true);
        jolts.push_back(SBGeoVector2D(0, 0));
        if (!std::strcmp(p->sprite->fileName.c_str(), "Level10_Big Gattling")) bigGatling = true;
    }
    for (auto& o : obstacles) TranslateObstacle(*o, v);
    ExecuteWayPoint();
    if (pushable) stopped = true;
    float fs = speed != 0.0f ? 1.0f / speed : 1e6f;
    for (DVElementFXMasked* p : parts) p->frameScale = fs;
    SBLog("mobile element: %zu parts, %zu obstacles, route %u at waypoint %u of %zu, %s%s%s", parts.size(), obstacles.size(),
          route, at, path.Count(), pushable ? "pushed by Sanchez, " : "", railroad ? "a train, " : "",
          parts.empty() ? "" : parts[0]->sprite->fileName.c_str());
    return n + 2;
}

bool DVElementMobile::GoalReached() const { return sprite->pos.IsGoalReached(); }

void DVElementMobile::Hourglass() {
    if (!active || stopped) return;
    // (the dust and the rolling sounds, +0x1c8 / +0x234, come with the sound effects)
    if (wait == 0) {
        if (!waypoint) {
            Update();
            if (!GoalReached()) return;
            waypoint = path.Current();
        }
        ExecuteWayPoint();
    } else {
        --wait;
    }
}

void DVElementMobile::Update() {
    if (!active) return;
    if (accel != 0.0f) {
        speed += accel;
        if ((accel > 0.0f && targetSpeed <= speed) || (accel < 0.0f && speed <= targetSpeed)) {
            speed = targetSpeed;
            accel = 0;
        }
    }
    if (speed != 0.0f) Move(dir * speed);
}

SBGeoVector2D DVElementMobile::RouteDirection() const {
    size_t n = path.Count();
    if (n < 2) return SBGeoVector2D(0, 0);
    const DVWaypoint* a = (*path.waypoints)[0].get();
    const DVWaypoint* b = (*path.waypoints)[n - 1].get();
    if (!a || !b) return SBGeoVector2D(0, 0);
    return SBGeoVector2D((float)b->x - (float)a->x, (float)b->y - (float)a->y);
}

void DVElementMobile::Move(const SBGeoVector2D& v) {
    MoveMap(sprite->pos, v);
    // Sanchez goes with it
    if (hooked && hooked->sprite) {
        MoveMap(hooked->sprite->pos, v);
        hooked->sprite->pos.ComputePositionAll();
    }
    box = box + v;
    pushPoint = pushPoint + v;
    for (auto& q : shape.polygon.pts) q = q + v;
    shape.box = shape.box + v;
    for (auto& o : obstacles) TranslateObstacle(*o, v);
    float fs = speed != 0.0f ? 1.0f / speed : 1e6f;
    for (size_t i = 0; i < parts.size(); ++i) {
        DVElementFXMasked* p = parts[i];
        p->frameScale = fs;
        SBGeoVector2D m = v;
        if (shaky) {
            // a small random jolt, at most a pixel each way, with the speed
            float a = speed * 0.1f;
            SBGeoVector2D j = jolts[i];
            m = m - j;
            j.x += ((float)desprand() / 32767.0f * 2.0f) * a - a;
            j.y += ((float)desprand() / 32767.0f * 2.0f) * a - a;
            j.x = std::fmax(-1.0f, std::fmin(1.0f, j.x));
            j.y = std::fmax(-1.0f, std::fmin(1.0f, j.y));
            jolts[i] = j;
            m = m + j;
        }
        MoveMap(p->sprite->pos, m);
        p->sprite->ComputeDisplayOrder(nullptr, true);
    }
    dustPoint.x += v.x;
    dustPoint.y += v.y;
}

void DVElementMobile::SetAnimationAllSprites(uint32_t a) {
    anim = a;
    for (DVElementFXMasked* p : parts) {
        DVSprite* s = p->sprite.get();
        if (!s->HasAnimation(a)) continue;
        s->row = (uint16_t)s->animRows[a];
        s->animation = a;
        s->ResetSpriteFrame(false);
    }
}

void DVElementMobile::ExecuteWayPoint() {
    if (macro) {
        while (remaining != 0)
            if (!ExecuteMacroCommand()) break;
    } else if (waypoint && waypoint->dataLen != 0) {
        if (waypoint->isScript) {
            if (DVScript::mpInstance) DVScript::mpInstance->ReachPoint(waypoint, this);
        } else if (waypoint->data.size() >= 5) {
            // the waypoint's data: [blocks s16][direction u8][offset u16]([direction u8][offset u16]);
            // each block: [choices s16] then [chance u8][offset u16] each, one picked at random; the
            // commands: [size s16] then the commands
            const uint8_t* d = waypoint->data.data();
            size_t len = waypoint->data.size();
            auto u16 = [&](size_t o) -> uint16_t { return o + 1 < len ? (uint16_t)(d[o] | d[o + 1] << 8) : 0; };
            int16_t blocks = (int16_t)u16(0);
            size_t at = 3;
            bool right = path.HasRightDirection((int8_t)d[2]);
            if (!right && blocks == 2 && len > 5) {
                at = 6;
                right = path.HasRightDirection((int8_t)d[5]);
            }
            if (right) {
                uint16_t off = u16(at);
                int16_t choices = (int16_t)u16(off);
                size_t c = (size_t)off + 2;
                int r = desprand() % 100 + 1;
                int left = choices;
                while (left > 1 && c < len && d[c] < r) {
                    r -= d[c];
                    c += 3;
                    --left;
                }
                uint16_t cmds = u16(c + 1);
                remaining = (int16_t)u16(cmds);
                macroBase = d;
                macro = d + cmds + 2;
                bool go = true;
                while (go && remaining != 0) go = ExecuteMacroCommand();
                if (remaining != 0) return;  // a wait: the rest after it
            }
        }
    }
    // to the next waypoint
    path.Next();
    DVWaypoint* w = path.Current();
    DVPositionInterface& pos = sprite->pos;
    if (w) {
        goal = SBGeoPoint2D((float)w->x, (float)w->y);
        pos.goal = goal;
        pos.tolerance = 0;
        SBGeoVector2D d = goal - pos.posMap;
        float l = std::sqrt(d.x * d.x + d.y * d.y);
        dir = l > 0.0f ? d * (1.0f / l) : SBGeoVector2D(0, 0);
        pos.incMap = dir;
        for (DVElementFXMasked* p : parts) p->sprite->pos.incMap = dir;
    }
    waypoint = nullptr;
    macro = nullptr;
}

// DVElementMobile::ExecuteMacroCommand: false when the commands stop here (a wait)
bool DVElementMobile::ExecuteMacroCommand() {
    if (!macro) return false;
    auto u16 = [](const uint8_t* q) { return (uint16_t)(q[0] | q[1] << 8); };
    auto f32 = [](const uint8_t* q) {
        float v;
        std::memcpy(&v, q, 4);
        return v;
    };
    uint8_t c = *macro++;
    switch (c) {
    case 0x80:  // the parts' animation
    case 0x85:
        SetAnimationAllSprites(u16(macro));
        macro += 2;
        remaining -= 3;
        return true;
    case 0x81:  // the speed
        speed = f32(macro);
        macro += 4;
        remaining -= 5;
        return true;
    case 0x82: {  // to this speed by that waypoint
        float t = f32(macro);
        targetSpeed = t == 0.0f ? 0.1f : t;
        uint16_t wi = u16(macro + 4);
        macro += 6;
        remaining -= 7;
        if (!path.waypoints || wi >= path.waypoints->size()) {
            SBError(true, "DVElementMobile.cpp", 0x2be, "VERBOTEN : Mobile element waypoint command referencing non-existing waypoint (%d) !", wi);
            return true;
        }
        const DVWaypoint* w = (*path.waypoints)[wi].get();
        SBGeoVector2D d = sprite->pos.posMap - SBGeoPoint2D((float)w->x, (float)w->y);
        float dist = std::sqrt(d.x * d.x + d.y * d.y);
        accel = dist > 0.0f ? (targetSpeed + speed) * (targetSpeed - speed) * 0.5f / dist : 0.0f;
        return true;
    }
    case 0x83:  // wait
        wait = u16(macro);
        macro += 2;
        for (DVElementFXMasked* p : parts) p->frameScale = 1e6f;
        remaining -= 3;
        return false;
    case 0x84: {  // back to the route's start (or its end, going backwards)
        if (!path.waypoints || path.waypoints->empty()) {
            remaining -= 1;
            return true;
        }
        uint16_t i = path.forward ? 0 : (uint16_t)(path.waypoints->size() - 1);
        const DVWaypoint* w = (*path.waypoints)[i].get();
        path.SetCurrentWaypointIndex(i);
        Move(SBGeoPoint2D((float)w->x, (float)w->y) - sprite->pos.posMap);
        remaining -= 1;
        return true;
    }
    case 0x86: {  // the parts' direction
        uint16_t v = u16(macro);
        macro += 2;
        for (DVElementFXMasked* p : parts) {
            DVSprite* s = p->sprite.get();
            s->pos.direction = s->pos.directionWanted = (uint16_t)(v & 0xf);
            if (s->HasAnimation(anim)) s->row = (uint16_t)(s->animRows[anim] + v);
            s->animation = anim;
            s->ResetSpriteFrame(false);
        }
        remaining -= 3;
        return true;
    }
    default:
        SBError(false, "DVElementMobile.cpp", 0x319, "VERBOTEN : Unknown mobile element waypoint command !");
        remaining = 0;
        return false;
    }
}

void DVElementMobile::ExecuteImmediately(DVSequenceElement* e) {
    if (e->command == 0xae) stopped = true;
    else if (e->command == 0xad) stopped = false;
    else return;
    e->SetState(SEQ_DONE);
}
