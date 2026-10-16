#include "dv/DVElement.h"
#include "dv/DVPatch.h"
#include "dv/DVDoor.h"
#include "dv/DVDoor.h"
#include "dv/DVSector.h"

#include "dv/DVArmsTrader.h"

#include <cmath>
#include <cstdlib>

#include "dv/DVArtificialIntelligence.h"

#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVFrameHolder.h"
#include "dv/DVPathFinder.h"
#include "dv/DVSector.h"
#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"

static uint32_t gCreationCounter = 0;  // gulCreationCounter

// SBDrawManager::CreateColor (0x0814ed60)
static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((b >> 3) | ((r & 0xf8u) * 0x100 + (g & 0xfcu) * 8));
}

// DVElement::DVElement (0x08249160)
DVElement::DVElement() {
    creation = gCreationCounter++;
    active = true;
    shadows = true;
    sprite.reset(new DVSprite());
    shadowKey = DVEngine::mpEngine ? DVEngine::mpEngine->StateGet(0xf) : 0x1f;
}

DVElement::~DVElement() {
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    if (d) {
        if (surface != 0xffffffff) d->DeleteSurface(surface);
        if (surface2 != 0xffffffff) d->DeleteSurface(surface2);
    }
}

void DVElement::MakeSurfaces(int extraW2, int extraH2, bool second) {
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    surface = d->CreateSurface(sprite->width, sprite->height);
    if (second) surface2 = d->CreateSurface((uint16_t)(sprite->width + extraW2), (uint16_t)(sprite->height + extraH2));
}

const char* DVElement::KindName() const {
    switch (kind) {
    case KIND_COOPER: return "Cooper";
    case KIND_DOC: return "Doc";
    case KIND_SAM: return "Sam";
    case KIND_KATE: return "Kate";
    case KIND_SANCHEZ: return "Sanchez";
    case KIND_MIA: return "Mia";
    case KIND_MRLEONE: return "Mr Leone";
    case KIND_VILLAIN: return "villain";
    case KIND_CIVILIAN: return "civilian";
    case KIND_HORSE: return "horse";
    case KIND_OBJECT: return "object";
    case KIND_TARGET: return "target";
    case KIND_FX: return "scenery";
    case KIND_MOBILE: return "cart";
    }
    if (kind >= 0x210 && kind <= 0x215) return "animal";
    if (kind >= 0x1101 && kind <= 0x1117) return "item";
    return "?";
}

void DVElement::Refresh(uint32_t, float, const SBGeoBoundingBox2D&, bool) {}

// what DVElementObject / DVElementActorNPC::Refresh do to draw: the frame into the element's
// surface (masked), then onto the screen with the shadow colour at the frame holder's percentage
void DVElement::DrawSprite(uint32_t screen, float zoom, const SBGeoBoundingBox2D& view, bool masks, bool hiddenMasks,
                           bool shadowBlit) {
    DVSprite* s = sprite.get();
    if (!s->IsOnScreen(view, zoom)) return;
    DVspriteRenderingInfo info;
    info.surface = surface;
    info.hidden = hiddenMasks;
    info.outlineColor = colorOutline;
    info.applyMasks = masks;
    s->CreateTargetSprite(info);
    SBGeoBoundingBox2D src, dst;
    s->GenerateBlitBox(view, zoom, src, dst);
    if (!src.IsOK() || !dst.IsOK()) return;
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    if (shadowBlit && shadows) {
        uint16_t pct = DVFrameHolder::mpFrameHolder ? DVFrameHolder::mpFrameHolder->shadowPercent : 40;
        d->BlitAlphaKeying(surface, &src, screen, &dst, shadowKey, pct, BLIT_COLORKEY);
    } else {
        d->Blit(surface, &src, screen, &dst, BLIT_COLORKEY);
    }
}

// ---- actors -------------------------------------------------------------------------------

static bool s_HasAnim(DVSprite* s, uint32_t anim) { return s->HasAnimation(anim); }

DVElementActor::DVElementActor() = default;
DVElementActor::~DVElementActor() = default;

// DVElementActorPC / NPC / Animal / Horse ::LoadStateFromFile (0x08241a70, 0x08234f20,
// 0x082045e0, 0x082167f0)
int DVElementActor::LoadStateFromFile(SBFile& f) {
    int n = sprite->LoadSpriteFromFile(f, FRAME_ACTOR);
    if (kind >= KIND_COOPER && kind <= KIND_MIA) {
        uint16_t len = f.U16();
        std::string name(len, '\0');
        if (len) f.Serialize(&name[0], len);
        scriptName = name.substr(0, name.find('\0'));
        profile = f.U16();
        MakeSurfaces(4, 0, true);
        return n + 4 + len;
    }
    if (kind == KIND_VILLAIN || kind == KIND_CIVILIAN) {
        // DVElementActorNPC::LoadStateFromFile: the script's name, the brain's attributes
        // (DVArtificialMalignity / Bonhomie::LoadAttributesFromFile), the route (DVPath) and the
        // starting state
        scriptName = f.String16();
        int len = (int)scriptName.size();
        if (kind == KIND_VILLAIN)
            ai = std::make_unique<DVArtificialMalignity>(this);
        else
            ai = std::make_unique<DVArtificialBonhomie>(this);
        int a = ai->LoadAttributesFromFile(f);
        int p = ai->path.LoadFromFile(f);
        ai->role = f.U16();
        MakeSurfaces(2, 2, true);
        // can walk and run (animations 3 and 5)
        ai->canMove = s_HasAnim(sprite.get(), CMD_WALK) && s_HasAnim(sprite.get(), CMD_RUN);
        return a + p + 4 + n + len;
    }
    if (kind == KIND_HORSE) {
        uint16_t len = f.U16();
        f.Skip(len, 1);
        f.U16();
        MakeSurfaces(0, 0, true);
        return n + 4 + len;
    }
    // animals (and Mr Leone's dog, kind 7)
    uint16_t len = f.U16();
    f.Skip(len, 1);
    MakeSurfaces(0, 0, true);
    f.Skip(2, 1);
    return n + 4 + len;
}

// DVElementActorNPC::Refresh (0x0822cae0), the drawing part; for the heroes, the selection
// outline of DVElementActorPC::ShowSelection (0x0823fc50) over it
void DVElementActor::Refresh(uint32_t screen, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) {
    if (!active || barrel) return;
    DrawSprite(screen, zoom, view, true, silhouettes, true);
    if (!IsHero() || !selected || surface2 == 0xffffffff) return;
    DVSprite* s = sprite.get();
    if (!s->IsOnScreen(view, zoom)) return;
    // the outline: the free pixels next to the frame's (DVSprite::ApplyEdgeMapToCreateOutline)
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    uint32_t id = s->CurrentFrameId();
    if (id == 0xffffffff || !s->frameHolder) return;
    SBDrawViewport vp;
    if (!d->GetSurfaceViewport(surface2, vp, VIEWPORT_WRITE)) return;
    uint16_t key = vp.colorKey;
    int w = s->frameHolder->GetSpriteWidth(id), h = s->frameHolder->GetSpriteHeight(id);
    if (w > vp.width) w = vp.width;
    if (h > vp.height) h = vp.height;
    s->frameHolder->UnCompressFrame(vp, id);
    std::vector<uint8_t> solid((size_t)w * h);
    uint16_t shadow = shadowKey;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint16_t c = vp.data[(size_t)y * vp.width + x];
            solid[(size_t)y * w + x] = c != key && c != shadow;
        }
    for (int y = 0; y < vp.height; ++y)
        for (int x = 0; x < vp.width; ++x) {
            bool edge = false;
            if (x < w && y < h && !solid[(size_t)y * w + x]) {
                edge = (x > 0 && solid[(size_t)y * w + x - 1]) || (x + 1 < w && solid[(size_t)y * w + x + 1]) ||
                       (y > 0 && solid[(size_t)(y - 1) * w + x]) || (y + 1 < h && solid[(size_t)(y + 1) * w + x]);
            }
            vp.data[(size_t)y * vp.width + x] = edge ? colorSelect : key;
        }
    d->ReleaseSurfaceViewport(vp);
    SBGeoBoundingBox2D src, dst;
    s->GenerateBlitBox(view, zoom, src, dst);
    if (!src.IsOK() || !dst.IsOK()) return;
    uint16_t pct = 50;
    if (selectionPulse > 0 && selectionPulseLen > 0)
        pct = (uint16_t)((int)((float)(selectionPulseLen - selectionPulse) * 60.0f / (float)selectionPulseLen) + 0x28);
    d->BlitAlphaConstant(surface2, &src, screen, &dst, pct, BLIT_COLORKEY);
}

// ---- the heroes' motion ------------------------------------------------------------------------

static uint32_t gNextOrderID = 1;  // DVOrder::mulNextID (0 = no order)
uint32_t DVOrder::NewID() { return gNextOrderID++; }

// DVElementActor::SetStates (0x0820f7b0) with DVElement::SetPosture (0x08249970)
void DVElementActor::SetStates(uint32_t posture, uint32_t state) {
    if (sprite->pos.posture != 0xe) sprite->pos.SetPosture(posture);
    actionState = state;
}

// DVElementActor::SetDefaultWaitAction (0x0820cbe0), the heroes' cases used so far
void DVElementActor::SetDefaultWaitAction() {
    switch (sprite->pos.posture) {
    case POSTURE_LYING: defaultCommand = CMD_LYING; break;
    default: defaultCommand = CMD_WAIT; break;
    }
}

std::vector<SBGeoPoint2D> DVElementActor::PlannedPath() const {
    std::vector<SBGeoPoint2D> r;
    if (orders.empty()) return r;
    r.push_back(sprite->pos.posMap);
    for (const DVOrder& o : orders)
        if (o.command == CMD_WALK || o.command == CMD_RUN || o.command == CMD_CRAWL) r.push_back(o.goal);
    return r;
}

// DVElementActor::Translate (0x08211170), command 0x17 "go there": straight there when the way is
// free (DVFastFindGrid::IsReachableThick), else along the path finder's way (AddPathRequest,
// DVPathFinder::FindPath, then DVEngine::ProcessPathRequests turns the points into orders)
bool DVElementActor::BuildMoveInArea(const SBGeoPoint2D& start0, DVSectorMotionArea* area, uint16_t layer,
                                     const SBGeoPoint2D& goal, uint32_t command, uint32_t movePosture, bool fromHere,
                                     std::deque<DVOrder>& out) const {
    DVSprite* s = sprite.get();
    DVPositionInterface& pos = s->pos;
    DVFastFindGrid* grid = s->grid;
    if (!grid) return false;
    auto add = [&](const SBGeoPoint2D& p) {
        DVOrder o;
        o.id = DVOrder::NewID();
        o.command = command;
        o.goal = p;
        out.push_back(o);
    };
    SBGeoVector2D d = goal - start0;
    if (std::fabs(d.x) < 0.5f && std::fabs(d.y) < 0.5f) return true;
    SBGeoVector2D half = pos.GetMoveBox(movePosture).p1;
    if (grid->IsReachableThick(start0, goal, layer, half)) {
        add(goal);
        return true;
    }
    // DVPathFinder::AddPathRequest (0x0832aa40): a start inside a wall is moved out first
    SBGeoPoint2D start = start0;
    bool adjusted = false;
    {
        SBGeoBoundingBox2D mb = fromHere ? pos.GetMoveBoxMap() : pos.GetMoveBox(movePosture) + start0;
        if (!grid->IsPositionAutorized(mb, layer)) {
            adjusted = true;
            if (!grid->FindAutorizedPosition(mb, layer)) {
                SBError(false, "DVPathFinder.cpp", 0x19c, "Actor in a wall at (%f,%f)", start0.x, start0.y);
                return false;
            }
            start = SBCenter(mb);
        }
    }
    if (!area) {
        SBLog("%s is on no walkable area", KindName());
        return false;
    }
    DVPathFinder* pf = DVEngine::mpEngine ? DVEngine::mpEngine->PathFinder() : nullptr;
    if (!pf) return false;
    std::vector<SBGeoPoint2D> path;
    if (!pf->FindPath(layer, area->areaIndex, pos.GetPathFinderIndex(movePosture), start, goal, adjusted, path)) {
        SBLog("no way from %.0f,%.0f to %.0f,%.0f (layer %u area %u size %u)", start.x, start.y, goal.x, goal.y, layer,
              area->areaIndex, pos.GetPathFinderIndex(movePosture));
        return false;
    }
    for (size_t i = adjusted ? 0 : 1; i < path.size(); ++i) add(path[i]);
    return true;
}

bool DVElementActor::BuildMove(const SBGeoPoint2D& goal, uint32_t command, uint32_t movePosture,
                               std::deque<DVOrder>& out, DVSectorMotionArea* goalArea) const {
    DVSprite* s = sprite.get();
    DVPositionInterface& pos = s->pos;
    DVFastFindGrid* grid = s->grid;
    if (!grid) return false;
    if (goal.x == 0.0f && goal.y == 0.0f) {
        SBError(false, "DVElementActor.cpp", 0x9ad, "VERBOTEN: Actor trying to go to (0,0)!");
        return false;
    }
    DVSectorMotionArea* cur = Area();
    if (!goalArea) {
        // the area under the goal: his own if it is there, else one of his layer, else any
        if (cur && !cur->IsBuilding() && cur->polygon.IsInside_p(goal)) goalArea = cur;
        if (!goalArea && !cur) goalArea = grid->FindMotionArea(pos.layer, goal);
        if (!goalArea && cur && !cur->IsBuilding()) goalArea = grid->FindMotionArea(pos.layer, goal);
        if (!goalArea)
            for (int l = (int)grid->layerCount - 2; l >= 0 && !goalArea; --l) goalArea = grid->FindMotionArea((uint16_t)l, goal);
        if (!goalArea) goalArea = cur;
    }
    auto order = [&](uint32_t cmd, uint32_t anim, const SBGeoPoint2D& p, DVDoor* door, bool enter) {
        DVOrder o;
        o.id = DVOrder::NewID();
        o.command = cmd;
        o.anim = anim;
        o.goal = p;
        o.door = door;
        o.enter = enter;
        out.push_back(o);
    };
    // a walk inside one area, a lift or a house
    auto inside = [&](DVSectorMotionArea* area, uint16_t layer, const SBGeoPoint2D& from, const SBGeoPoint2D& to,
                      bool fromHere) {
        if (area && area->IsBuilding()) {
            order(CMD_TELEPORT, 0, to, nullptr, false);
            return true;
        }
        if (area && area->IsLift()) {
            SBGeoVector2D d = to - from;
            if (std::fabs(d.x) > 0.5f || std::fabs(d.y) > 0.5f)
                order(CMD_MOVE_ANIM, DVLiftAnimation(area, from, to, command), to, nullptr, false);
            return true;
        }
        return BuildMoveInArea(from, area, layer, to, command, movePosture, fromHere, out);
    };
    if (!cur || !goalArea || cur == goalArea) return inside(cur, pos.layer, pos.posMap, goal, true);
    // DVSequence::AppendMoveToSequence: the doors on the way (FindPathDoors), each passed in two
    // halves around its middle
    std::vector<std::pair<DVDoor*, bool>> doors;
    if (!grid->FindPathDoors(this, pos.posMap, cur, goal, goalArea, doors)) {
        SBLog("%s: no door leads from area %d to area %d", KindName(), cur->id, goalArea->id);
        // the old way: a walk on his own area
        return inside(cur, pos.layer, pos.posMap, goal, true);
    }
    SBGeoPoint2D at = pos.posMap;
    DVSectorMotionArea* area = cur;
    uint16_t layer = pos.layer;
    bool fromHere = true;
    for (auto& [d, enter] : doors) {
        if (!inside(area, layer, at, d->Entry(enter), fromHere)) return false;
        order(CMD_MOVE_ANIM, d->Action1(enter, command), d->mid, d, enter);
        order(CMD_MOVE_ANIM, d->Action2(enter, command), d->Exit(enter), nullptr, false);
        at = d->Exit(enter);
        area = d->To(enter);
        layer = d->ToLayer(enter);
        fromHere = false;
    }
    if (area && area->IsBuilding()) return true;  // he stays inside
    return inside(area, layer, at, goal, false);
}

// a move ordered by the player (DVEngine::PerformGroupMove for one hero, 0x08294710): the place
// is moved out of the walls (FindAutorizedPosition), lying heroes crawl, a run first stands up
bool DVElementActor::MoveTo(const SBGeoPoint2D& dest, bool run) {
    DVSprite* s = sprite.get();
    DVPositionInterface& pos = s->pos;
    DVFastFindGrid* grid = s->grid;
    if (!grid || (!active && !InBuilding())) return false;
    // what is under the tap (DVEngine::ManageInputMouseFocus: GetSectorScreen): a walkable area
    // of some layer, a lift, a house's door
    DVSectorMotionArea* area = nullptr;
    uint16_t layer = pos.layer;
    DVDoor* door = nullptr;
    if (!grid->GetSectorScreen(dest, area, layer, door)) {
        area = nullptr;
        layer = InBuilding() ? 0 : pos.layer;
    }
    SBGeoPoint2D goal = dest;
    if (door) {
        // into the house (or through the gate)
        bool enter = Area() != door->b;
        goal = door->Exit(enter);
        area = door->To(enter);
    } else if (!area || !area->IsLift()) {
        SBGeoBoundingBox2D box = pos.GetMoveBoxMap() - pos.posMap + dest;
        if (!grid->FindAutorizedPosition(box, dest, layer)) {
            SBLog("no place near %.0f,%.0f", dest.x, dest.y);
            return false;
        }
        goal = SBCenter(box);
    }
    uint32_t posture = pos.posture;
    std::deque<DVOrder> seq;
    uint32_t movePosture = posture == POSTURE_LYING ? POSTURE_LYING : POSTURE_STANDING;
    if (posture == POSTURE_LYING && run) {
        DVOrder o;
        o.id = DVOrder::NewID();
        o.command = CMD_STAND_UP;
        seq.push_back(o);
        movePosture = POSTURE_STANDING;
    }
    uint32_t cmd = run ? CMD_RUN : (movePosture == POSTURE_LYING ? CMD_CRAWL : CMD_WALK);
    if (!BuildMove(goal, cmd, movePosture, seq, area)) return false;
    if (scriptElement) EndScriptElement(false);
    orders = seq;
    newOrder = true;
    return true;
}

void DVElementActor::Stop() {
    if (scriptElement) EndScriptElement(false);
    orders.clear();
    newOrder = true;
    defaultCommand = Mounted() ? (uint32_t)CMD_RIDE_WAIT : (uint32_t)CMD_NONE;
    if (DVElementActor* h = Horse()) h->Stop();
}

// DVElementActor::MakeRunning (0x08214dd0): the move goes on running (standing up first)
void DVElementActor::MakeRunning() {
    if (DVElementActor* h = Horse()) {
        h->MakeRunning();
        return;
    }
    if (orders.empty()) return;
    bool lying = sprite->pos.posture == POSTURE_LYING;
    for (DVOrder& o : orders) {
        if (o.command == CMD_WALK || o.command == CMD_CRAWL) o.command = CMD_RUN;
        if (o.command == CMD_MOVE_ANIM && (o.anim == CMD_WALK || o.anim == CMD_CRAWL)) o.anim = CMD_RUN;
    }
    if (lying && orders.front().command != CMD_STAND_UP) {
        DVOrder o;
        o.id = DVOrder::NewID();
        o.command = CMD_STAND_UP;
        orders.push_front(o);
        newOrder = true;
    }
}

// DVElementActor::MakeCrawling (0x08214a30) with the hero's 0x47 (DVElementActorPC::Translate):
// lie down a little ahead (as far as the animation goes), then crawl on
void DVElementActor::MakeCrawling() {
    DVSprite* s = sprite.get();
    DVPositionInterface& pos = s->pos;
    if (pos.posture == POSTURE_LYING) return;
    for (DVOrder& o : orders)
        if (o.command == CMD_WALK || o.command == CMD_RUN) o.command = CMD_CRAWL;
    DVFastFindGrid* grid = s->grid;
    if (!grid) return;
    SBGeoBoundingBox2D box = pos.GetMoveBox(POSTURE_LYING) + pos.posMap;
    box = box + SBSetSector0to15(pos.direction, 1.0f) * s->GetDistanceForAnimation(CMD_LIE_DOWN);
    if (!grid->FindAutorizedPosition(box, pos.posMap, pos.layer)) return;
    DVOrder o;
    o.id = DVOrder::NewID();
    o.command = CMD_LIE_DOWN;
    o.goal = SBCenter(box);
    orders.push_front(o);
    newOrder = true;
}

// DVElementActor::MakeWalking (0x08214bb0): a lying hero stands up (0x46 -> order 10) and the move
// goes on walking
void DVElementActor::MakeWalking() {
    for (DVOrder& o : orders)
        if (o.command == CMD_RUN || o.command == CMD_CRAWL) o.command = CMD_WALK;
    if (sprite->pos.posture != POSTURE_LYING) return;
    DVOrder o;
    o.id = DVOrder::NewID();
    o.command = CMD_STAND_UP;
    orders.push_front(o);
    newOrder = true;
}

// DVElementActor::Hourglass (0x0820b2e0) with the Execute functions (DVElementActorPC 0x08244e80,
// DVElementActorHuman 0x0821e420, DVElementActor 0x0820da70) for the commands of this step
void DVElementActor::Hourglass() {
    if (selectionPulse > 0) --selectionPulse;
    // on someone's shoulder: he moves it (StepOrders of the carrier)
    if (carrier) return;
    if (!active) {
        // in a house he is off the map, but goes on with his orders (out through a door)
        // (and one taken off the map still finishes what a script told him)
        if ((InBuilding() || scriptElement) && !orders.empty()) StepOrders();
        return;
    }
    // DVElementActorHuman::Hourglass: out cold he lies; the concussion wears off
    if (knockedOut && orders.empty() && !dead) defaultCommand = CMD_DEAD_KNOCKED;
    if (concussion && DVEngine::mpEngine && ((DVEngine::mpEngine->frameCounter & 0x3f) == (creation & 0x1f)))
        SetConcussion(concussion - 1);
    if (IsNPC()) {
        NPCHourglass();
        return;
    }
    if (IsHorse()) {
        HorseHourglass();
        return;
    }
    // the animals only move for the scripts here
    if (!IsHero() && orders.empty() && !scriptElement) return;
    StepOrders();
}

// one step of the current orders; true when the last one just ended
bool DVElementActor::StepOrders() {
    DVSprite* s = sprite.get();
    DVPositionInterface& pos = s->pos;
    if (carried && carried->carrier == this) {
        // the body follows him (DVElement::CopyPositionMapEtc), drawn on his shoulder
        carried->CopyPositionFrom(this);
        if (carried->sprite->HasAnimation(CMD_CARRIED)) {
            carried->sprite->row = (uint16_t)(carried->sprite->animRows[CMD_CARRIED] + carried->sprite->pos.direction);
            carried->sprite->entry = 0;
        }
        carried->sprite->displayOrder = s->displayOrder + 0.5f;
        if (orders.empty()) {
            // standing still with it: the carrying walk's first frame
            if (s->HasAnimation(CMD_CARRY)) {
                s->row = (uint16_t)(s->animRows[CMD_CARRY] + pos.direction);
                s->entry = 0;
                s->orderId = 0xffffffff;
            }
            s->displayOrder = pos.pos3D.y;
            carried->sprite->displayOrder = s->displayOrder + 0.5f;
            return false;
        }
    }
    // on a horse: the walks are the horse's (its own orders); he follows it
    if (DVElementActor* h = Horse(); h && pos.posture == POSTURE_RIDING &&
                                     (orders.empty() || orders.front().command != CMD_MOUNT)) {
        auto isMove = [](uint32_t c) {
            return c == CMD_WALK || c == CMD_RUN || c == CMD_CRAWL || c == CMD_MOVE_ANIM || c == CMD_TURN || c == CMD_TELEPORT;
        };
        if (!orders.empty() && isMove(orders.front().command)) {
            std::deque<DVOrder> hs;
            while (!orders.empty() && isMove(orders.front().command)) {
                DVOrder o = orders.front();
                orders.pop_front();
                if (o.command == CMD_CRAWL) o.command = CMD_WALK;
                hs.push_back(o);
            }
            if (h->scriptElement) h->EndScriptElement(false);
            h->orders = hs;
            h->newOrder = true;
            h->isMove = true;
            if (orders.empty() && scriptElement) {
                // the script waits for the horse
                h->scriptElement = scriptElement;
                scriptElement = nullptr;
            }
            newOrder = true;
            rideWalking = true;
        }
        if (!h->orders.empty()) {
            // he waits for the horse to get there
            RideSync();
            return false;
        }
        if (orders.empty()) {
            RideSync();
            // the horse is there: his walk is over (for his brain)
            bool ended = rideWalking;
            rideWalking = false;
            return ended;
        }
        CopyPositionFrom(h);
    }
    if (orders.empty()) {
        if (defaultCommand == CMD_NONE) SetDefaultWaitAction();
        if (defaultCommand == CMD_LYING) {
            s->PerformAction(0, CMD_LYING, 0xb, false);
        } else if (defaultCommand == CMD_DEAD) {
            // DVElementActorHuman::Execute 0xd2: the death's last frame
            s->PerformAction(0, CMD_DIE, 0xc, false);
        } else if (defaultCommand == CMD_DEAD_KNOCKED) {
            pos.direction ^= 8;
            s->PerformAction(0, CMD_KNOCKED, 0xc, false);
            pos.direction ^= 8;
        } else if ((defaultCommand == CMD_SCRIPT_FREEZE || defaultCommand == CMD_SCRIPT_LOOP) &&
                   s->HasAnimation(defaultAnim)) {
            // DVElementActor::Execute 0xec without an order: the script's animation stays on its
            // last frame (or goes on looping)
            s->PerformAction(0, defaultAnim, defaultCommand == CMD_SCRIPT_LOOP ? 0xb : 0xc, false);
        } else if (defaultCommand == CMD_SHOOT || defaultCommand == CMD_SHOOT_LYING) {
            // after a shot the gun stays up (Execute 0x1c without an order)
            s->PerformAction(0, defaultCommand, 0xb, false);
        } else if (defaultCommand != CMD_WAIT && s->HasAnimation(defaultCommand)) {
            // DVElementActor::SetInitialAnimation's idle animations (sitting, sleeping, ...)
            s->PerformAction(0, defaultCommand, 0xc, false);
        } else {
            int r = s->PerformAction(0, CMD_WAIT, 4, false);
            if (r == 1) SetStates(POSTURE_STANDING, 0);
        }
        s->displayOrder = pos.pos3D.y;
        return false;
    }
    DVOrder& o = orders.front();
    bool first = newOrder;
    newOrder = false;
    int r = 4;
    switch (o.command) {
    case CMD_WALK:
        pos.Turn();
        if (carried && s->HasAnimation(CMD_CARRY)) {
            // DVCooper::Execute 0x2a: the walk with the body on his shoulder
            r = s->PerformMotion(o.id, o.goal, o.tolerance, CMD_CARRY, 1, 0, false);
            SetStates(POSTURE_CARRYING, r == 3 ? 0 : 1);
            break;
        }
        r = s->PerformMotion(o.id, o.goal, o.tolerance, CMD_WALK, 1, 0, false);
        if (r == 1) SetStates(POSTURE_STANDING, 1);
        else if (r == 3) SetStates(POSTURE_STANDING, 0);
        break;
    case CMD_RUN:
        if (carried) {
            // no running with a body
            o.command = CMD_WALK;
            newOrder = first;
            r = 2;
            break;
        }
        pos.Turn();
        r = s->PerformMotion(o.id, o.goal, o.tolerance, CMD_RUN, 2, 0, false);
        if (r == 1) SetStates(POSTURE_STANDING, 1);
        else if (r == 3) SetStates(POSTURE_STANDING, 0);
        break;
    case CMD_CRAWL:
        pos.Turn();
        r = s->PerformMotion(o.id, o.goal, o.tolerance, CMD_CRAWL, 1, 0, false);
        if (r == 3) SetStates(POSTURE_LYING, 0);
        break;
    case CMD_LIE_DOWN:
        if (!first && pos.incMap.x == 0.0f && pos.incMap.y == 0.0f && s->orderId == o.id)
            r = s->PerformAction(o.id, CMD_LIE_DOWN, 0, false);
        else
            r = s->PerformMotion(o.id, o.goal, 0.0f, CMD_LIE_DOWN, 5, 0, false);
        if (r == 1) {
            SetStates(POSTURE_LYING, 0);
            defaultCommand = CMD_LYING;
        }
        break;
    case CMD_STAND_UP:
        if (first && pos.posture != POSTURE_LYING) {
            SetStates(POSTURE_STANDING, 0);
            r = 3;
            break;
        }
        r = s->PerformAction(o.id, CMD_STAND_UP, 0, false);
        if (r == 1) SetStates(POSTURE_GETTING_UP, 0);
        else if (r == 3) SetStates(POSTURE_STANDING, 0);
        break;
    case CMD_SHOOT:
    case CMD_SHOOT_LYING: {
        // DVElementActorHuman::Execute 0x1c / 8: turn to the target, the shot at the animation's
        // "action" frame
        if (first) {
            if (IsHero() && ammo < 1) {
                SBLog("%s: no bullets left (double-tap the gun to reload)", KindName());
                r = 3;
                break;
            }
            if (pos.posture != POSTURE_RIDING) SetStates(pos.posture == POSTURE_LYING ? POSTURE_LYING : POSTURE_STANDING, 0);
        }
        if (o.target && o.target->sprite) o.aim = o.target->sprite->pos.pos3D;
        SBGeoVector2D d(o.aim.x - pos.pos3D.x, o.aim.y - pos.pos3D.y);
        if (d.x != 0.0f || d.y != 0.0f) pos.directionWanted = (uint16_t)(SBGetSector0to15(d, 1.0f) & 0xf);
        pos.TurnFast();
        r = s->PerformAction(o.id, pos.posture == POSTURE_RIDING && s->HasAnimation(CMD_RIDE_SHOOT) ? (uint32_t)CMD_RIDE_SHOOT : o.command,
                             0, false);
        if (r == 3 && pos.posture == POSTURE_RIDING) {
            actionState = 0;
        } else if (r == 3) {
            SetStates(pos.posture == POSTURE_LYING ? POSTURE_LYING : POSTURE_STANDING, 4);
        } else if (r == 0 && firearm) {
            if (o.target) Shoot(o.target, firearm, false);
            else
                for (uint16_t i = 0; i < firearm->bullets; ++i) ShootSingleBullet(o.aim, firearm, false, true);
            ammo = (int16_t)(ammo - 1);
            // the shot's noise: a villain's 2, a hero's 0
            if (DVEngine* e = DVEngine::mpEngine) e->Noise(IsHero() ? 0 : 2, pos.pos3D, this);
        }
        break;
    }
    case CMD_HIT:
    case CMD_HIT_LYING:
        r = s->PerformAction(o.id, o.command, 0, false);
        if (r == 1) SetStates(o.command == CMD_HIT ? POSTURE_STANDING : POSTURE_LYING, 0);
        break;
    case CMD_DIE:
        r = s->PerformAction(o.id, CMD_DIE, 0, false);
        if (r == 3) {
            pos.SetPosture(0xe);
            actionState = 0;
        }
        break;
    case CMD_RELOAD:
        if (first) o.tolerance = 30.0f;
        s->PerformAction(o.id, pos.posture == POSTURE_LYING ? CMD_LYING : CMD_WAIT, 0xb, false);
        o.tolerance -= 1.0f;
        r = o.tolerance <= 0.0f ? 3 : 2;
        if (r == 3 && firearm) ammo = (int16_t)firearm->magazine;
        break;
    case CMD_PUNCH: {
        // DVElementActorHuman::Execute 0xe9: the punch; at the blow's frame whoever is within 40
        // in front of him is hit (0x2d)
        if (first) pos.direction = pos.directionWanted;
        r = s->PerformAction(o.id, CMD_PUNCH_ANIM, 0, false);
        if (r == 0 && DVEngine::mpEngine) {
            for (DVElementActor* v : DVEngine::mpEngine->Characters()) {
                if (v == this || !v->active || (v->classFlags & 7) != 7 || v->dead) continue;
                SBGeoVector2D d = v->sprite->pos.posMap - pos.posMap;
                if (std::max(std::fabs(d.x), std::fabs(d.y)) >= 40.0f) continue;
                if ((SBGetSector0to15(d, 0.57357645f) & 0xf) != pos.direction) continue;
                v->KnockHit(o.damage, this);
                break;
            }
        }
        break;
    }
    case CMD_KNOCKED:
        // DVElementActorHuman::Translate 0x2d: he falls (the animation is drawn the other way
        // round, as the knocked out default shows it)
        if (first) pos.direction = pos.directionWanted;
        pos.direction ^= 8;
        r = s->PerformAction(o.id, CMD_KNOCKED, 0, false);
        pos.direction ^= 8;
        if (r == 3) {
            SetStates(POSTURE_LYING, 0);
            if (!knockedOut) {
                DVOrder up;
                up.id = DVOrder::NewID();
                up.command = CMD_STAND_UP;
                orders.push_back(up);
            }
        }
        break;
    case CMD_SCRIPT_ANIM:
    case CMD_SCRIPT_LOOP:
    case CMD_SCRIPT_FREEZE:
        // DVElementActor::Execute 0xea / 0xeb / 0xec: the script's animation
        if (!s->HasAnimation(o.anim)) {
            r = 4;
            break;
        }
        r = s->PerformAction(o.id, o.anim, 0, false);
        if (r == 3 && o.command == CMD_SCRIPT_FREEZE) {
            s->ResetSpriteFrame(true);
        }
        break;
    case CMD_MOVE_ANIM: {
        // a door's half or a lift: the move with its animation; an animation that doesn't move
        // (getting on a ladder) plays to its end and puts him there. At the door's middle he
        // passes it (DVElementActor::Execute: PassDoor)
        uint32_t a = o.anim;
        // DVElementActor::Translate 0x16: the door opens (its patch)
        if (first && o.door && o.door->patch) o.door->patch->Apply();
        if (!s->HasAnimation(a)) a = s->HasAnimation(CMD_WALK) ? (uint32_t)CMD_WALK : a;
        if (!s->HasAnimation(a) || InBuilding()) {
            pos.SetPositionMap(o.goal);
            pos.ComputePositionAll();
            r = 3;
        } else if (s->GetDistanceForAnimation(a) <= 0.0f) {
            if (first) {
                SBGeoVector2D d = o.goal - pos.posMap;
                if (d.x != 0.0f || d.y != 0.0f) pos.direction = pos.directionWanted = (uint16_t)(SBGetSector0to15(d, 0.57357645f) & 0xf);
            }
            r = s->PerformAction(o.id, a, 0, false);
            if (r == 3) {
                pos.SetPositionMap(o.goal);
                pos.ComputePositionAll();
            }
        } else {
            pos.Turn();
            r = s->PerformMotion(o.id, o.goal, 0.0f, a, 1, 0, false);
            if (r == 1 && pos.posture == POSTURE_LYING && a != CMD_CRAWL) SetStates(POSTURE_STANDING, 1);
        }
        if (r == 3 && o.door) PassDoor(o.door, o.enter);
        break;
    }
    case CMD_MOUNT: {
        // DVElementActorHuman::Execute 0xf: on the horse at once, then the animation
        auto* h = dynamic_cast<DVElementActor*>(o.target);
        if (first) {
            if (!h || !canRide || (h->rider && h->rider != this) || h->dead) {
                r = 4;
                break;
            }
            h->Stop();
            SitOnHorse(h);
        }
        r = s->HasAnimation(CMD_MOUNT) ? s->PerformAction(o.id, CMD_MOUNT, 0, false) : 3;
        if (h) CopyPositionFrom(h);
        break;
    }
    case CMD_JUMP_HORSE: {
        // DVElementActorPC::Execute 0x77 / PerformHorseJump: the leap from where he is down into
        // the saddle, along the jump's animation; then he sits on the horse (posture 1)
        auto* h = dynamic_cast<DVElementActor*>(o.target);
        if (first) {
            if (!h || !canRide || (h->rider && h->rider != this) || h->dead) {
                r = 4;
                break;
            }
            h->Stop();
            o.goal = pos.posMap;  // (where he leaps from)
            o.tolerance = 0;
            SBGeoVector2D d = h->sprite->pos.posMap - pos.posMap;
            if (d.x != 0.0f || d.y != 0.0f) pos.direction = pos.directionWanted = (uint16_t)(SBGetSector0to15(d, 1.0f) & 0xf);
        }
        if (!h) {
            r = 4;
            break;
        }
        r = s->HasAnimation(CMD_JUMP_HORSE) ? s->PerformAction(o.id, CMD_JUMP_HORSE, 0, false) : 3;
        o.tolerance += 1.0f;
        float t = std::min(1.0f, o.tolerance / 18.0f);
        SBGeoPoint2D to = h->sprite->pos.posMap;
        pos.SetPositionMap(o.goal + (to - o.goal) * t);
        pos.ComputePositionAll();
        if (r == 3 || r == 4) {
            SitOnHorse(h);
            r = 3;
        }
        break;
    }
    case CMD_ENTER_BARREL: {
        // DVKung::Execute 0xee: the hop into the barrel (animation 0x63) onto its place
        DVElement* b = o.target;
        if (!b || !b->sprite) {
            r = 4;
            break;
        }
        if (first) {
            SBGeoVector2D d = b->sprite->pos.posMap - pos.posMap;
            if (d.x != 0.0f || d.y != 0.0f) pos.direction = pos.directionWanted = (uint16_t)(SBGetSector0to15(d, 1.0f) & 0xf);
        }
        if (s->HasAnimation(0x63) && s->GetDistanceForAnimation(0x63) > 0.0f)
            r = s->PerformMotion(o.id, b->sprite->pos.posMap, 0.0f, 0x63, 5, 0, false);
        else
            r = 3;
        if (r == 3 || r == 4) {
            SetInBarrel(b);
            r = 3;
        }
        break;
    }
    case CMD_LEAVE_BARREL: {
        // DVKung::Execute 100: out of the barrel, a step in front of it (animation 0x64)
        if (first) {
            DVElement* b = barrel;
            barrel = nullptr;
            SetStates(POSTURE_STANDING, 0);
            float dist = s->HasAnimation(0x64) ? s->GetDistanceForAnimation(0x64) : 20.0f;
            if (dist <= 0.0f) dist = 20.0f;
            SBGeoVector2D f = SBSetSector0to15(pos.direction & 0xf, 1.0f);
            SBGeoPoint2D at = (b ? b->sprite->pos.posMap : pos.posMap) + SBGeoVector2D(f.x * dist, f.y * dist * 0.57357645f);
            SBGeoBoundingBox2D box = pos.GetMoveBox(POSTURE_STANDING) + at;
            if (s->grid && s->grid->FindAutorizedPosition(box, at, pos.layer)) at = SBCenter(box);
            o.goal = at;
            defaultCommand = CMD_WAIT;
        }
        if (s->HasAnimation(0x64) && s->GetDistanceForAnimation(0x64) > 0.0f)
            r = s->PerformMotion(o.id, o.goal, 0.0f, 0x64, 5, 0, false);
        else {
            pos.SetPositionMap(o.goal);
            pos.ComputePositionAll();
            r = 3;
        }
        if (r == 3 || r == 4) {
            SetStates(POSTURE_STANDING, 0);
            r = 3;
        }
        break;
    }
    case CMD_DISMOUNT: {
        // DVElementActorHuman::Execute 0x14: down, beside the horse
        DVElementActor* h = Horse();
        if (!h) {
            r = 3;
            break;
        }
        if (first) h->Stop();
        CopyPositionFrom(h);
        r = s->HasAnimation(CMD_DISMOUNT) ? s->PerformAction(o.id, CMD_DISMOUNT, 0, false) : 3;
        if (r == 3 || r == 4) {
            SBGeoVector2D side = SBSetSector0to15((unsigned)(pos.direction + 12) & 0xf, 1.0f);
            SBGeoPoint2D at = pos.posMap + SBGeoVector2D(side.x * 28.0f, side.y * 16.0f);
            SBGeoBoundingBox2D box = pos.GetMoveBox(POSTURE_STANDING) + at;
            if (s->grid && s->grid->FindAutorizedPosition(box, at, pos.layer)) at = SBCenter(box);
            h->rider = nullptr;
            h->horseUsed = true;
            horse = nullptr;
            SetStates(POSTURE_STANDING, 0);
            Teleport(at, pos.layer, pos.sector);
            r = 3;
        }
        break;
    }
    case CMD_TAKE_BODY: {
        // DVCooper::Execute 0x29: the pick-up; at its end the body is on his shoulder (it
        // follows him, 0x6c, posture 6)
        auto* b = dynamic_cast<DVElementActor*>(o.target);
        if (first) {
            if (!b || !b->IsCarriable(o.anim == 1) || carried || !CanCarry()) {
                r = 4;
                break;
            }
            SBGeoVector2D d = b->sprite->pos.posMap - pos.posMap;
            if (d.x != 0.0f || d.y != 0.0f) pos.direction = pos.directionWanted = (uint16_t)(SBGetSector0to15(d, 0.57357645f) & 0xf);
        }
        r = s->HasAnimation(CMD_TAKE_BODY) ? s->PerformAction(o.id, CMD_TAKE_BODY, 0, false) : 3;
        if (r == 3 && b && b->IsCarriable(o.anim == 1)) {
            b->active = true;
            carried = b;
            b->carrier = this;
            if (b->scriptElement) b->EndScriptElement(false);
            b->orders.clear();
            b->SetStates(POSTURE_CARRIED, 0);
            b->defaultCommand = CMD_CARRIED;
            b->CopyPositionFrom(this);
            SetStates(POSTURE_CARRYING, 0);
        }
        break;
    }
    case CMD_DROP_BODY: {
        // DVCooper::Execute 0x2b: down in front of him, lying as he was (out cold or dead)
        DVElementActor* b = carried;
        r = s->HasAnimation(CMD_DROP_BODY) ? s->PerformAction(o.id, CMD_DROP_BODY, 0, false) : 3;
        if ((r == 3 || r == 4) && b) {
            SBGeoVector2D f = SBSetSector0to15(pos.direction & 0xf, 1.0f);
            SBGeoPoint2D at = pos.posMap + SBGeoVector2D(f.x * 22.0f, f.y * 13.0f);
            carried = nullptr;
            b->carrier = nullptr;
            b->Teleport(at, pos.layer, pos.sector);
            b->sprite->pos.direction = pos.direction;
            b->SetStates(b->dead ? 0xeu : (uint32_t)POSTURE_LYING, 0);
            b->defaultCommand = b->dead ? CMD_DEAD : CMD_DEAD_KNOCKED;
            b->sprite->displayOrder = b->sprite->pos.pos3D.y;
            SetStates(POSTURE_STANDING, 0);
            r = 3;
        }
        break;
    }
    case CMD_TELEPORT:
        // inside a house: to the door he goes out by
        pos.SetPositionMap(o.goal);
        pos.ComputePositionAll();
        r = 3;
        break;
    case CMD_LEAVE:
        // the end of DVScript::RecordLeaveGame's walk: off the map
        active = false;
        if (DVElementActor* h = Horse()) {
            h->active = false;
            if (h->saddle) h->saddle->active = false;
        }
        r = 3;
        break;
    case CMD_TURN:
        // DVElementActor::Translate 0x1d sets the wanted direction; Execute 0xe0 turns two
        // sixteenths a tick, stepping on the spot (the walk animation)
        if (first) {
            pos.directionWanted = o.direction & 0xf;
            if (pos.direction == pos.directionWanted) {
                r = 3;
                break;
            }
        }
        if (pos.posture != POSTURE_STANDING) {
            pos.direction = pos.directionWanted;
            r = 3;
            break;
        }
        {
            bool turning = pos.TurnFast();
            if (first) {
                s->PerformAction(o.id, CMD_WAIT, 0xb, false);
                r = 1;
            } else {
                s->PerformAction(o.id, CMD_WALK, 0, false);
                r = turning ? 2 : 3;
            }
        }
        break;
    }
    bool ended = false;
    if (r == 3 || r == 4) {
        uint32_t done = o.command;
        uint32_t doneAnim = o.anim;
        orders.pop_front();
        newOrder = true;
        if (orders.empty()) {
            defaultCommand = IsNPC() ? CMD_WAIT : CMD_NONE;
            if (done == CMD_DIE) defaultCommand = CMD_DEAD;
            else if (done == CMD_KNOCKED && knockedOut) defaultCommand = CMD_DEAD_KNOCKED;
            else if (done == CMD_SHOOT || done == CMD_SHOOT_LYING) defaultCommand = done;
            else if (done == CMD_MOUNT || Mounted()) defaultCommand = CMD_RIDE_WAIT;
            else if (done == CMD_SCRIPT_FREEZE || done == CMD_SCRIPT_LOOP) {
                defaultCommand = done;
                defaultAnim = doneAnim;
            }
            ended = true;
        }
    }
    if (ended && scriptElement) EndScriptElement(true);
    s->displayOrder = pos.pos3D.y;
    return ended;
}

// ---- the NPCs --------------------------------------------------------------------------------

uint32_t DVElementActor::CurrentCommand() const {
    if (!orders.empty()) return orders.front().command;
    return defaultCommand == CMD_NONE ? CMD_WAIT : defaultCommand;
}

// DVSequenceManager::LaunchSequence for one actor: the orders replace the current ones
void DVElementActor::Launch(const std::deque<DVOrder>& seq, bool move) {
    if (scriptElement) EndScriptElement(false);
    orders = seq;
    newOrder = true;
    isMove = move;
}

// DVElementActorNPC::Halt
void DVElementActor::Halt() {
    if (ai) ai->halting = true;
    uint32_t c = CurrentCommand();
    if (c - 0x43u > 1) {
        orders.clear();
        newOrder = true;
        if (ai) ai->halted = false;
    }
    if (ai) ai->halting = false;
}

// DVElementActorNPC::StoreInitialPositionParameters: its post (place and direction)
void DVElementActor::StoreInitialPositionParameters() {
    DVPositionInterface& pos = sprite->pos;
    homeDir = SBSetSector0to15(pos.direction, 1.0f);
    home.p = pos.posMap;
    home.layer = (int16_t)pos.layer;
    home.sector = pos.sector;
}

void DVElementActor::LaunchTimer(uint32_t frames, bool macro) {
    if (!ai) return;
    uint32_t now = *DVArtificialIntelligence::mpUniversalFrameCounter;
    if (macro) {
        ai->macroTimerAt = frames + now;
        ai->macroTimerOn = true;
        return;
    }
    ai->timerSubstate = ai->substate;
    ai->timerAt = frames + now;
    ai->timerOn = true;
}

void DVElementActor::KillTimer(bool macro) {
    if (!ai) return;
    if (macro)
        ai->macroTimerOn = false;
    else
        ai->timerOn = false;
}

// DVElementActorNPC::SetViewStatus: while glancing, the status for afterwards
void DVElementActor::SetViewStatus(uint8_t status, bool force) {
    (void)force;
    if (ai && ai->glanceOn) {
        savedView.status = status;
        return;
    }
    view.changed = view.status != status;
    view.status = status;
}

// DVElementActorNPC::SetViewCone: the cone's shape and motion for each kind of watching
void DVElementActor::SetViewCone(uint32_t cone) {
    if (knockedOut || !ai) return;
    ai->viewCone = cone;
    DVviewParameters& v = view;
    uint16_t R = DVArtificialIntelligence::muwStandardViewPolygonRadius;
    int status = 2;  // the usual: sweeping
    switch (cone) {
    case 0:
    case 1:
        v.widthTarget = 0.5f; v.widthSpeed = 0.1f; v.radius = R; v.sweepAmplitude = 0.8f;
        v.sweepSpeed = 0.0392f; v.u2ae = 0x9a; v.turnStep = 0.19635f;
        break;
    case 2:
        v.widthTarget = 0.5f; v.widthSpeed = 0.1f; v.radius = R; v.sweepAmplitude = 0.8f;
        v.sweepSpeed = 0.19635f; v.u2ae = 0x9a; v.turnStep = 0.19635f;
        break;
    case 3:
        v.widthTarget = 0.8f; v.widthSpeed = 0.3f; v.radius = R; v.sweepAmplitude = 1.5f;
        v.sweepSpeed = 0.3927f; v.angle = 0; v.u2ae = 0x9a; v.turnStep = 0.19635f;
        break;
    case 4:
        v.widthTarget = 0.8f; v.widthSpeed = 0.3f; v.radius = R; v.angle = 0; v.u2ae = 0x9a; v.turnStep = 0.19635f;
        status = -1;
        break;
    case 5:
        v.widthTarget = 0.8f; v.widthSpeed = 0.1f; v.radius = R; v.sweepAmplitude = 1.5f;
        v.sweepSpeed = 0.15708f; v.angle = 0; v.u2ae = 0x9a; v.turnStep = 0.19635f;
        break;
    case 6:
        v.widthTarget = 0.5f; v.widthSpeed = 0.3f; v.radius = R; v.sweepAmplitude = 1.5f; v.u2ae = 0x9a;
        v.turnStep = 0.19635f;
        status = 1;
        break;
    case 7:
        v.widthTarget = 0.5f; v.radius = 0x78; v.sweepAmplitude = 0.8f; v.sweepSpeed = 0.0392f; v.u2ae = 0x9a;
        v.turnStep = 0.19635f;
        status = -1;
        break;
    case 8:
        v.widthTarget = 0.3f; v.widthSpeed = 0.1f; v.radius = R; v.sweepAmplitude = 1.5f; v.sweepSpeed = 0.0392f;
        v.u2ae = 0x9a; v.turnStep = 0.7854f;
        status = -1;
        break;
    case 10:
        v.widthTarget = 0.3f; v.widthSpeed = 0.1f; v.radius = R; v.sweepAmplitude = 0.1f; v.sweepSpeed = 0.0157f;
        v.u2ae = 0x9a; v.turnStep = 0.19635f;
        break;
    case 0xb:
        v.widthTarget = 0.15f; v.widthSpeed = 0.1f;
        if (v.radius < R) v.radius = R;
        v.sweepAmplitude = 0.8f; v.sweepSpeed = 0.0392f; v.u2ae = 0x9a; v.turnStep = 0.7854f;
        status = -1;
        break;
    case 0xc:
        v.widthTarget = 0.22f; v.widthSpeed = 0.1f; v.radius = R; v.sweepAmplitude = 0.8f; v.sweepSpeed = 0.0392f;
        v.u2ae = 0x9a; v.turnStep = 0.7854f;
        status = -1;
        break;
    case 0xd:
        v.widthTarget = 0.5f; v.widthSpeed = 0.1f; v.radius = (uint16_t)(int)((float)R * 0.6f); v.sweepAmplitude = 0.5f;
        v.sweepSpeed = 0.0157f; v.u2ae = 0x9a; v.turnStep = 0.19635f;
        break;
    case 0xe:
        v.widthTarget = 0.5f; v.widthSpeed = 0.1f; v.radius = R; v.sweepAmplitude = 0.8f; v.sweepSpeed = 0.00628f;
        v.u2ae = 0x9a; v.turnStep = 0.19635f;
        break;
    case 0xf:
        v.widthTarget = 0.15f; v.widthSpeed = 0.1f; v.radius = R; v.sweepAmplitude = 1.5f; v.f310 = 2.0f;
        v.sweepSpeed = 0.00628f; v.u2ae = 0x9a; v.turnStep = 0.19635f;
        break;
    case 0x10:
        v.widthTarget = 0.5f; v.widthSpeed = 0.1f; v.radius = R; v.sweepAmplitude = 0.5f; v.sweepSpeed = 0.00628f;
        v.u2ae = 0x9a; v.turnStep = 0.19635f; v.f31c = 0.1f; v.f310 = 2.0f; v.b330 = true;
        break;
    case 0x11:
        v.widthTarget = 0.5f; v.widthSpeed = 0.1f; v.radius = R; v.sweepAmplitude = 1.0f; v.sweepSpeed = 0.1047f;
        v.u2ae = 0x9a; v.turnStep = 0.19635f;
        break;
    default:
        status = -1;
        break;
    }
    if (status >= 0) {
        if (ai->glanceOn)
            savedView.status = (uint8_t)status;
        else {
            v.changed = v.status != status;
            v.status = (uint8_t)status;
        }
    }
    v.widthMoving = true;
    ai->viewCone = cone;
}

// DVElementActorNPC::GlanceAt: looks at a place for a while, then back to the usual view
void DVElementActor::GlanceAt(const DVposition& p, uint32_t frames) {
    if (!ai) return;
    ai->glanceUntil = *DVArtificialIntelligence::mpUniversalFrameCounter + frames;
    if (!ai->glanceOn) savedView = view;
    view.target = p.p;
    view.targetElement = nullptr;
    view.changed = view.status != 9;
    view.status = 9;
    ai->glanceOn = true;
}

void DVElementActor::RestoreView() { view = savedView; }

void DVElementActor::SetViewTarget(const SBGeoPoint2D& p, bool turn) {
    if (ai) ai->glanceOn = false;
    view.target = p;
    view.targetElement = nullptr;
    DVPositionInterface& pos = sprite->pos;
    if (pos.posture == 1 || !turn) {
        view.status = 4;
        return;
    }
    Halt();
    pos.directionWanted = SBGetSector0to15(p - pos.posMap, 1.0f) & 0xf;
    view.status = 5;
}

void DVElementActor::SetViewTarget(DVElement* e, bool turn) {
    if (!e || !e->sprite) return;
    SetViewTarget(e->sprite->pos.posMap, turn);
    view.targetElement = e;
    view.status = view.status == 5 ? 7 : 6;
}

void DVElementActor::DontFixDirectionOnViewTarget() {
    if (view.status == 5)
        view.status = 4;
    else if (view.status == 7)
        view.status = 6;
}

// DVElementActorNPC::RefreshView: where the cone points this frame. The body's direction gives
// the base; status 1 looks straight ahead, 2 sweeps from side to side (sine of a phase), 3 turns
// round, 4-7 and 9 look at a target point
void DVElementActor::RefreshView() {
    DVviewParameters& v = view;
    DVPositionInterface& pos = sprite->pos;
    SBGeoVector2D body = SBSetSector0to15(pos.direction, 1.0f);
    if (v.status != 8 && (knockedOut || actionState == 6 || pos.posture == 0xe)) v.status = 0;
    uint16_t d = pos.direction;
    if (d != lastDirection) {
        // the body turned: the cone keeps looking where it was, up to a quarter turn
        int steps = (int)(int8_t)((uint8_t)((d - lastDirection) * 1 + 7) & 0xf) - 7;
        float a = (float)steps * -0.3926991f + v.angle;
        if (a < -1.5707964f) a = -1.5707964f;
        if (a > 1.5707964f) a = 1.5707964f;
        v.angle = a;
        v.changed = true;
        lastDirection = d;
    }
    if (v.widthMoving) {
        if (v.widthTarget <= v.width) {
            v.width -= v.widthSpeed;
            if (v.width < v.widthTarget) {
                v.width = v.widthTarget;
                v.widthMoving = false;
            }
        } else {
            v.width += v.widthSpeed;
            if (v.widthTarget < v.width) {
                v.width = v.widthTarget;
                v.widthMoving = false;
            }
        }
    }
    if (v.status == 0) return;
    auto rotated = [](const SBGeoVector2D& b, float ang) {
        float c = std::cos(ang), s = std::sin(ang);
        return SBGeoVector2D(b.x * c - b.y * s, b.x * s + b.y * c);
    };
    switch (v.status) {
    case 1:
        // back to straight ahead, a step at a time
        if (v.angle > 0.0f) {
            if (v.turnStep < v.angle) {
                v.angle -= v.turnStep;
                v.dir = rotated(body, v.angle);
                break;
            }
        } else if (v.angle < -v.turnStep) {
            v.angle += v.turnStep;
            v.dir = rotated(body, v.angle);
            break;
        }
        v.angle = 0;
        v.sweepPhase = 0;
        v.dir = body;
        v.changed = false;
        break;
    case 2: {
        float sp = v.sweepSpeed * 1.0f;
        float ph = v.sweepPhase + sp;
        if (ph > 6.2831855f) ph -= 6.2831855f;
        v.sweepPhase = ph;
        v.angle = v.sweepAmplitude * std::sin(ph);
        v.dir = rotated(body, v.angle);
        break;
    }
    case 3: {
        float a = v.turnStep + v.angle;
        if (a > 6.2831855f) a -= 6.2831855f;
        v.angle = a;
        v.dir = rotated(body, v.angle);
        break;
    }
    case 4:
    case 5:
    case 6:
    case 7:
    case 9: {
        if ((v.status == 6 || v.status == 7) && v.targetElement && v.targetElement->sprite)
            v.target = v.targetElement->sprite->pos.posMap;
        if (v.status == 7 || v.status == 5) {
            // the body follows the target
            pos.directionWanted = SBGetSector0to15(v.target - pos.posMap, 1.0f) & 0xf;
            if (orders.empty()) pos.Turn();
        }
        SBGeoVector2D to = v.target - pos.posMap;
        if (SBMaxNorm(to) == 0.0f) to = body;
        to.y *= 0.57357645f;
        // turns the cone towards the target, at most a quarter turn from the body
        float want = std::atan2(to.y, to.x) - std::atan2(body.y * 0.57357645f, body.x);
        while (want > 3.14159265f) want -= 6.2831855f;
        while (want < -3.14159265f) want += 6.2831855f;
        float lim = v.status == 9 ? 1.5707964f : 3.14159265f;
        if (want > lim) want = lim;
        if (want < -lim) want = -lim;
        if (want > v.angle + v.turnStep)
            v.angle += v.turnStep;
        else if (want < v.angle - v.turnStep)
            v.angle -= v.turnStep;
        else
            v.angle = want;
        v.dir = rotated(body, v.angle);
        break;
    }
    default:
        break;
    }
    // the eyes open: the cone grows to its length
    if (v.opening < 1000) {
        v.radiusNow = (uint16_t)((float)v.opening * 0.001f * (float)v.radius);
        v.opening = (uint16_t)(v.opening + (v.opening < 0x96 ? 5 : 2));
    } else {
        v.radiusNow = v.radius;
    }
    v.halfWidth = v.width * v.f31c;
    v.length = (uint16_t)((float)v.radiusNow * v.f310);
    v.edgeL = rotated(v.dir, -v.halfWidth);
    v.edgeL.y *= 0.57357645f;
    v.edgeR = rotated(v.dir, v.halfWidth);
    v.edgeR.y *= 0.57357645f;
}

// ---- sight ------------------------------------------------------------------------------------

// gpCrawlingOffsetsX / Y (0x0855d720 / 0x0855d760): from the middle of a lying body to its head,
// by direction
static const float kCrawlX[16] = {0.0f, 6.1f, 11.3f, 14.8f, 16.0f, 14.8f, 11.3f, 6.1f,
                                  0.0f, -6.1f, -11.3f, -14.8f, -16.0f, -14.8f, -11.3f, -6.1f};
static const float kCrawlY[16] = {-9.18f, -8.49f, -6.48f, -3.5f, 0.0f, 3.5f, 6.48f, 8.49f,
                                  9.18f, 8.49f, 6.48f, 3.5f, 0.0f, -3.5f, -6.48f, -8.49f};

SBGeoPoint3D DVElementActor::EyesPoint() const {
    const DVPositionInterface& pos = sprite->pos;
    SBGeoPoint3D e = pos.pos3D;
    switch (pos.posture) {
    case 1:
        e.z += 46.0f;
        break;
    case 2:
    case 0xe: {
        int d = pos.direction & 0xf;
        bool half = pos.u_b4 || (pos.sector && pos.sector->type & 3 && static_cast<DVSectorMotionArea*>(pos.sector)->flagC0);
        float k = half ? 0.5f : 1.0f;
        e.x += kCrawlX[d] * k;
        e.y += kCrawlY[d] * k;
        e.z += 2.0f;
        break;
    }
    default:
        if (pos.posture <= 0xd) e.z += 32.0f;
        break;
    }
    return e;
}

void DVElementActor::DetectionPoints(SBGeoPoint3D& a, SBGeoPoint3D& b, SBGeoPoint3D& c) const {
    const DVPositionInterface& pos = sprite->pos;
    a = b = c = pos.pos3D;
    switch (pos.posture) {
    case 0: case 4: case 8: case 9: case 0xb: case 0xc: case 0xd:
        a.z += 16.0f;
        b.z += 32.0f;
        c.z += 2.0f;
        break;
    case 1:
        a.z += 30.0f;
        b.z += 46.0f;
        c.z += 16.0f;
        break;
    case 2:
    case 0xe: {
        int d = pos.direction & 0xf;
        bool half = pos.u_b4 || (pos.sector && pos.sector->type & 3 && static_cast<DVSectorMotionArea*>(pos.sector)->flagC0);
        float k = half ? 0.5f : 1.0f;
        float dx = kCrawlX[d] * k, dy = kCrawlY[d] * k;
        a.z += 2.0f;
        b.x += dx; b.y += dy; b.z += 2.0f;
        c.x -= dx; c.y -= dy; c.z += 2.0f;
        break;
    }
    case 5: case 7: case 10:
        a.z += 8.0f;
        b.z += 16.0f;
        c.z += 2.0f;
        break;
    case 6:
        a.z += 30.0f;
        b.z += 25.0f;
        c.z += 20.0f;
        break;
    default:
        break;
    }
}

bool DVElementActor::SeesPoint(const SBGeoPoint3D& p, bool range) const {
    const DVviewParameters& v = view;
    if ((v.status | 8) == 8) return false;
    SBGeoPoint3D e = EyesPoint();
    SBGeoVector2D d(p.x - e.x, p.y - e.y);
    if (range) {
        float gy = d.y * 1.7434468f;
        float dz = p.z - e.z;
        if ((float)((int)v.length * (int)v.length) < dz * dz + gy * gy + d.x * d.x) return false;
        if (SBDot(SBGeoVector2D(d.x, gy), v.dir) < 0.0f) return false;
    }
    // between the cone's two sides
    if (SBDet(v.edgeL, d) < 0.0f) return false;
    if (SBDet(v.edgeR, d) > 0.0f) return false;
    DVFastFindGrid* grid = DVFastFindGrid::mpFastFindGrid;
    return grid ? grid->IsReachable(e, p, true) : true;
}

// the cone's reach on the ground: the view is a sphere of the cone's length around the eyes (the
// night's lights come with the night missions)
float DVElementActor::ViewRadius(const SBGeoPoint3D& eyes) const {
    float r2 = (float)((int)view.length * (int)view.length) - eyes.z * eyes.z;
    float r = r2 > 0.0f ? std::sqrt(r2) : 0.0f;
    // DVElementActorNPC::ComputeViewRadius at night: half as far, unless a lit zone he can see
    // is near the middle or the sides of his view (up to the whole length within 10 pixels)
    DVEngine* e = DVEngine::mpEngine;
    DVFastFindGrid* grid = sprite ? sprite->grid : nullptr;
    if (!e || !e->night || !grid || r <= 0.0f) return r;
    float h = r * 0.5f;
    SBGeoPoint2D base(eyes.x, eyes.y);
    SBGeoPoint2D mid = base + SBGeoVector2D(view.dir.x, view.dir.y * 0.57357645f) * h;
    SBGeoPoint2D left = base + view.edgeL * h, right = base + view.edgeR * h;
    uint16_t layer = sprite->pos.layer;
    float best = 0.5f;
    auto factor = [](const SBGeoPoint2D& a, const SBGeoPoint2D& b) {
        float d = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
        return d < 10.0f ? 1.0f : d > 110.0f ? 0.5f : 1.05f - d * 0.005f;
    };
    for (const auto& l : grid->Lights()) {
        if (l.layer != layer || std::fabs(l.center.x - mid.x) > 400.0f || std::fabs(l.center.y - mid.y) > 400.0f) continue;
        float f = std::max(factor(l.center, mid), std::max(factor(l.center, left), factor(l.center, right)));
        if (f <= best) continue;
        if (!grid->IsReachable(eyes, l.point, true)) continue;
        best = f;
    }
    return r * best;
}

// the hero's current command scales how fast he is noticed (running x4, walking x2, crawling
// x0.4, ...)
static float CommandFactor(uint32_t c) {
    switch (c) {
    case 3: case 4: case 8: case 10: case 0xb: case 0xe: case 0x19: case 0x1a: case 0x1c: case 0x1d:
    case 0x1f: case 0x20: case 0x23: case 0x24: case 0x2c: case 0x2d:
        return 2.0f;
    case 5: case 0xc: case 0x15: case 0x2e: case 0x2f: case 0xc2:
        return 4.0f;
    case 9:
        return 0.4f;
    case 0xf: case 0x12: case 0x14: case 0x29: case 0x2a: case 0x2b: case 0x40: case 0x58: case 0x59: case 0x5a:
        return 3.5f;
    case 0x10: case 0x11:
        return 2.5f;
    case 0x13: case 0x77:
        return 5.0f;
    case 0x44: case 0x45:
        return 3.0f;
    case 0x54:
        return 0.6f;
    default:
        if (c >= 0x30 && c <= 0x66 && c != 0x42 && c != 0x43 && c != 0x49 && c != 0x4a && c != 0x4b && c != 0x4c &&
            c != 0x4d && c != 0x4e && c != 0x4f && c != 0x50 && c != 0x51 && c != 0x5d && c != 0x5e && c != 0x63)
            return 2.0f;
        return 1.0f;
    }
}

// DVElementActorHuman::GetShadowZone (0x08229280): at night, the shadow (a sight obstacle that
// doesn't stop the eyes, flag 1 off) whose volume holds him; -1 if none. An NPC sees someone in a
// shadow only from inside the same shadow (DVElementActorNPC::IsDetecting).
int32_t DVElementActor::ShadowZone() const {
    DVEngine* e = DVEngine::mpEngine;
    if (!e || !e->night || !sprite || !sprite->grid) return -1;
    SBGeoPoint3D mid, head, feet;
    DetectionPoints(mid, head, feet);
    auto top = [](const DVSightObstacle* o, float x, float y) { return o->plane.zx * x + o->plane.zy * y + o->plane.zc; };
    auto ground = [](const DVSightObstacle* o, float x, float y) {
        return o->planeGround.zx * x + o->planeGround.zy * y + o->planeGround.zc;
    };
    SBGeoBoundingBox2D box;
    box.Expand(SBGeoPoint2D(mid.x, mid.y));
    box.Expand(SBGeoPoint2D(head.x, head.y));
    box.Expand(SBGeoPoint2D(feet.x, feet.y));
    std::vector<DVSightObstacle*> obs;
    sprite->grid->GetObstacles(obs, 0, box, 2);
    SBGeoPoint2D p(mid.x, mid.y);
    bool lying = sprite->pos.posture == POSTURE_LYING;
    for (const DVSightObstacle* o : obs) {
        if (!o || (o->flags & 1) || !o->box.IsInside_p(p) || !o->polygon.IsInside_p(p)) continue;
        if (lying) {
            if (head.z <= top(o, head.x, head.y) && (o->flat || ground(o, head.x, head.y) <= head.z) &&
                feet.z <= top(o, feet.x, feet.y) && (o->flat || ground(o, feet.x, feet.y) <= feet.z))
                return (int32_t)o->serial;
        } else {
            float t = top(o, mid.x, mid.y);
            if (o->flat) {
                if (head.z <= t && feet.z <= t) return (int32_t)o->serial;
            } else {
                float g = ground(o, mid.x, mid.y);
                if (head.z <= t && g <= head.z && feet.z <= t && g <= feet.z) return (int32_t)o->serial;
            }
        }
    }
    return -1;
}

// DVElementActorPC::IsObviouslyHostile (0x08247ea0): what he is doing is plainly an attack (a
// shot, a blow, a knife, a trap...)
bool DVElementActor::IsObviouslyHostile() const {
    uint32_t c = CurrentCommand();
    if (c == CMD_PUNCH) return true;
    if (c >= 0x76) return c - 0x76u < 0x1e && ((0x3f800001u >> (c - 0x76u)) & 1);
    switch (c) {
    case 8: case 0x11: case 0x1b: case 0x1c: case 0x1d: case 0x23: case 0x24: case 0x29: case 0x2a:
    case 0x2b: case 0x31: case 0x36: case 0x37: case 0x38: case 0x39: case 0x3c: case 0x3e: case 0x3f:
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x52:
    case 0x59: case 0x5a: case 0x5f:
        return true;
    default:
        return false;
    }
}

uint8_t DVElementActor::DetectHero(const DVElementActor* hero, uint16_t& speed) const {
    speed = 0;
    if ((view.status | 8) == 8) return 0;
    if (!hero || !hero->active || hero->dead) return 0;
    const DVPositionInterface& hp = hero->sprite->pos;
    if (hp.layer != sprite->pos.layer) return 0;
    if (hp.posture == 3 || hp.posture == 0xe) return 0;
    // at night one in a shadow is only seen from inside it
    int32_t zone = hero->ShadowZone();
    if (zone != -1 && zone != ShadowZone()) return 0;
    // Kate in her dress (the other profile) is a lady: no one minds her unless she does
    // something hostile or has already scared him (IsDetecting: sprite +0x1fe)
    if (hero->kind == KIND_KATE && hero->sprite->useAlt && !hero->IsObviouslyHostile() && hero->CurrentCommand() != 0x4a &&
        !(ai && ai->yellowBack))
        return 0;
    SBGeoPoint3D e = EyesPoint();
    SBGeoPoint3D P = hp.pos3D;
    SBGeoVector2D d(P.x - e.x, (P.y - e.y) * 1.7434468f);
    float d2 = d.x * d.x + d.y * d.y;
    if ((float)((int)view.length * (int)view.length) < d2 || SBDot(d, view.dir) < 0.0f) return 0;
    if (d2 <= 225.0f) {
        speed = 2000;
        return 2;
    }
    float r = ViewRadius(e);
    float r2 = r * r;
    float identify2 = hero->CurrentCommand() == 0x54 ? 2500.0f : r2 * 0.25f;
    if (r2 < d2) return 0;
    SBGeoPoint3D a, b, c;
    hero->DetectionPoints(a, b, c);
    if (!SeesPoint(a, false) && !SeesPoint(b, false) && !SeesPoint(c, false)) return 0;
    SBGeoVector3D t(P.x - e.x, P.y - e.y, P.z - e.z);
    float f = (t.x * t.x + t.y * t.y + t.z * t.z) / (float)((int)view.length * (int)view.length);
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    float k = (1.0f - f) * (1.0f - f);
    if (f < 0.25f) k *= (0.25f - f) * 8.0f + 1.0f;
    uint8_t iq = 50;  // GetIQ: the character profile comes with the fights
    float sp = k * (float)(iq * 3 + 400);
    sp *= CommandFactor(hero->CurrentCommand());
    speed = (uint16_t)(sp > 65535.0f ? 65535.0f : sp);
    if (speed == 0) return 0;
    return d2 < identify2 ? 2 : 1;
}

// ---- the view cone's colours (DVElementActorNPC::InitColors / InitCivilianColors) -----------
// colours are 0xBBGGRR. Calm: green going yellow as the suspicion grows; suspicious: yellow going
// red (and back); alarmed: red; pink: calming down after a fight.
namespace {
struct RGB8 {
    uint8_t r = 0, g = 0, b = 0;
};
RGB8 gGreen[32], gYellow[32 * 32], gPink[32], gRed;
RGB8 gCivGreen[32], gCivYellow[32], gCivRed;
RGB8 Of(uint32_t c) { return {(uint8_t)(c & 0xff), (uint8_t)(c >> 8 & 0xff), (uint8_t)(c >> 16 & 0xff)}; }
}  // namespace

void DVElementActor::InitColors(uint32_t green, uint32_t yellow, uint32_t red, uint32_t pink) {
    RGB8 G = Of(green), Y = Of(yellow), R = Of(red), P = Of(pink);
    for (unsigned i = 0; i < 32; ++i) {
        float t = (float)i / 36.0f, u = 1.0f - t;
        gGreen[i] = {(uint8_t)(int)(Y.r * t + G.r * u), (uint8_t)(int)(Y.g * t + G.g * u), (uint8_t)(int)(Y.b * t + G.b * u)};
        for (unsigned j = 0; j < 32 - i; ++j) {
            float s = (float)j / 36.0f, v = u - s;
            gYellow[i * 32 + j] = {(uint8_t)(int)(P.r * s + Y.r * v + R.r * t), (uint8_t)(int)(P.g * s + Y.g * v + R.g * t),
                                   (uint8_t)(int)(P.b * s + Y.b * v + R.b * t)};
        }
        gPink[i] = {(uint8_t)(int)(R.r * t + P.r * u), (uint8_t)(int)(R.g * t + P.g * u), (uint8_t)(int)(R.b * t + P.b * u)};
    }
    gRed = R;
    // the civilians' (fixed colours, InitCivilianColors(0xafffaf, 0xa0f8ff, 0x618eff))
    RGB8 cg = Of(0xafffaf), cy = Of(0xa0f8ff), cr = Of(0x618eff);
    for (unsigned i = 0; i < 32; ++i) {
        float t = (float)i / 36.0f, u = 1.0f - t;
        gCivGreen[i] = {(uint8_t)(int)(cy.r * t + cg.r * u), (uint8_t)(int)(cy.g * t + cg.g * u), (uint8_t)(int)(cy.b * t + cg.b * u)};
        gCivYellow[i] = {(uint8_t)(int)(cr.r * t + cy.r * u), (uint8_t)(int)(cr.g * t + cy.g * u), (uint8_t)(int)(cr.b * t + cy.b * u)};
    }
    gCivRed = cr;
}

void DVElementActor::GetAlertColor(uint8_t& r, uint8_t& g, uint8_t& b) const {
    RGB8 c{0xff, 0xff, 0xff};
    const DVArtificialIntelligence* a = ai.get();
    if (!detecting && a) {
        if (view.status == 8) {
            // the eyes closing: red, darker as the cone shrinks
            float f = (float)view.radiusNow / (float)DVArtificialIntelligence::muwStandardViewPolygonRadius;
            f *= f;
            c = {(uint8_t)(int)(gRed.r * f), (uint8_t)(int)(gRed.g * f), (uint8_t)(int)(gRed.b * f)};
        } else if (!a->IsVillain()) {
            switch (view.alert) {
            case 0: c = gCivGreen[std::min(31, a->suspect >> 8)]; break;
            case 1: c = gCivYellow[std::min(31, a->yellow >> 8)]; break;
            case 2: c = gCivRed; break;
            case 3: case 4: c = gCivYellow[20]; break;
            default: break;
            }
        } else {
            switch (view.alert) {
            case 0: c = gGreen[std::min(31, std::max(a->u24e >> 9, a->suspect >> 8))]; break;
            case 1: c = gYellow[std::min(31, a->yellow >> 8) * 32 + std::min(31, a->yellowBack >> 8)]; break;
            case 2: c = gRed; break;
            case 3: case 4: c = gYellow[0]; break;
            case 5: {
                int k = (a->yellowBack >> 8) == 0x20 ? 0 : 0x1f - (a->yellowBack >> 8);
                c = gPink[std::max(0, std::min(31, k))];
                break;
            }
            default: break;
            }
        }
    }
    r = c.r;
    g = c.g;
    b = c.b;
}

// DVElementActorNPC::RefreshDetection, the heroes: each one seen grows the suspicion (+0x246);
// when it is full, the brain gets "I see him" (and "I know who it is" when he is near enough)
void DVElementActor::RefreshDetection() {
    DVArtificialIntelligence* b = ai.get();
    if (!b || sprite->pos.posture == 0xe || knockedOut) return;
    detecting = false;
    DVEngine* e = DVEngine::mpEngine;
    if (!e) return;
    const std::vector<DVElementActor*>& heroes = e->Heroes();
    uint16_t best = 0;
    int bestHero = -1;
    uint8_t bestLevel = 0;
    for (size_t i = 0; i < heroes.size(); ++i) {
        uint16_t sp = 0;
        // who he looks out for: a villain off duty (attitude 1) only for a hero who does something
        // hostile (DVElementActorPC::IsObviouslyHostile), a civilian off duty for no one; Kate
        // is seen anyway when she shows her leg (action state 7) or after a scare (+0x24c)
        const DVElementActor* h = heroes[i];
        bool look = true;
        if (b->attitude == 1) look = kind == KIND_VILLAIN && h->IsObviouslyHostile();
        if (h->kind == KIND_KATE) look = h->actionState == 7 || look || b->yellowBack != 0;
        // DVArtificialMalignity::EventViewStandardProcedure: a prisoner held at gunpoint is no
        // business of theirs (IsPrisoner), as long as he does nothing hostile
        if (h->IsHeldPrisoner() && !h->IsObviouslyHostile()) look = false;
        uint8_t lvl = look ? DetectHero(h, sp) : 0;
        if (lvl) {
            if (sp > best) {
                best = sp;
                bestHero = (int)i;
                bestLevel = lvl;
            }
            if (b->alertStatus == 0) {
                if (b->suspect > 0x1a00) sp = (uint16_t)(sp * 3);
                uint32_t s2 = (uint32_t)b->suspect + sp;
                b->suspect = (uint16_t)(s2 > 0xffff ? 0xffff : s2);
            } else {
                b->UpdateDetection((uint8_t)i, lvl, heroes[i]);
            }
        } else {
            b->UpdateDetection((uint8_t)i, 0, heroes[i]);
        }
    }
    if (b->alertStatus == 0) {
        if (b->suspect >= 0x2000) {
            b->suspect = 0x1fff;
            if (bestHero >= 0) b->UpdateDetection((uint8_t)bestHero, bestLevel, heroes[bestHero]);
        } else if (best == 0) {
            uint16_t s = b->suspect;
            b->suspect = s < 9 ? 0 : (uint16_t)(s - (s < 0x1001 ? 8 : 3));
        }
    }
}

// DVArtificialIntelligence::InitState: the starting state given by the level
bool DVElementActor::InitState(DVArtificialIntelligence& a) {
    a.riding = false;
    auto timer = [&]() {
        int r = (std::rand() & 0x7fff) % 0x46 + 0x1e;
        LaunchTimer(r ? (uint32_t)r : 1u, false);
    };
    if (a.role == 0x10) {
        // DVArtificialIntelligence::InitState: on the nearest free horse
        DVElementActor* best = nullptr;
        float bd = 0;
        if (DVEngine::mpEngine)
            for (DVElementActor* c : DVEngine::mpEngine->Characters())
                if (c->IsHorse() && !c->rider) {
                    SBGeoVector2D d = c->sprite->pos.posMap - sprite->pos.posMap;
                    float q = d.x * d.x + d.y * d.y;
                    if (!best || q < bd) best = c, bd = q;
                }
        if (best) SitOnHorse(best);
        else SBError(false, "DVArtificialIntelligence.cpp", 0x12f6, "Villain on (%u, %u) didn't find a horse !",
                     (unsigned)sprite->pos.posMap.x, (unsigned)sprite->pos.posMap.y);
        a.SetState(AI_NORMAL, SUB_AT_POST);
        SetViewCone(0);
        return true;
    }
    switch (a.role) {
    case 0:
        a.SetState(AI_NORMAL, SUB_AT_POST);
        timer();
        SetViewCone(0);
        return true;
    case 1:
        a.SetState(AI_NORMAL, SUB_AT_POST);
        timer();
        defaultCommand = 1;   // SetInitialAnimation(1)
        a.atPost = true;
        return false;
    case 2:
        a.SetState(AI_NORMAL, SUB_AT_POST);
        SetViewCone(1);
        timer();
        return true;
    case 7:
    case 0x75:
        // a combat point or a horse to start on: they come with the fights
        a.SetState(AI_NORMAL, SUB_AT_POST);
        SetViewCone(0);
        timer();
        return true;
    case 0x16:  // a prisoner
        SetViewCone(0xa);
        a.SetState(9, 0xd8);
        return false;
    case 0x1b:  // threatening someone
        SetViewCone(0xa);
        a.SetState(5, 0xc2);
        return false;
    case 0x1e:
    case 0x1f:
    case 0x20:
        // knocked out at the start (DVElementActorNPC::SetConcussionOfTheBrain)
        a.SetState(0, 2);
        SetViewStatus(8, false);
        CloseEyes();
        defaultCommand = a.role == 0x20 ? 0x20 : 0xd3;
        return false;
    case 0x21:  // a body
        a.SetState(0, 2);
        CloseEyes();
        defaultCommand = 0xd2;
        SetStates(0xe, 0);
        return false;
    case 0x54:  // asleep
        a.SetState(0, 1);
        SetViewStatus(8, false);
        CloseEyes();
        defaultCommand = 0x54;
        return false;
    default:
        SBError(false, "DVArtificialIntelligence.cpp", 0x136d, "Actor at (%f, %f): Initial action %u not supported by AI.",
                sprite->pos.posMap.x, sprite->pos.posMap.y, a.role);
        return false;
    }
}

// DVElementActorNPC::Hourglass: the brain's timers and stimuli, the view, the orders; every 16
// frames DVArtificialIntelligence::The16thFrame
void DVElementActor::NPCHourglass() {
    DVArtificialIntelligence* b = ai.get();
    bool ended = StepOrders();
    if (!b || dead) return;
    if (ended && isMove) {
        isMove = false;
        b->SequenceDone(true, false);
    }
    RefreshView();
    RefreshDetection();
    uint32_t now = *DVArtificialIntelligence::mpUniversalFrameCounter;
    if (sprite->pos.posture != 0xe && ((now - frameOffset) & 0xf) == 0) b->The16thFrame((uint8_t)(now - frameOffset));
    b->Hourglass();
}

// ---- objects ------------------------------------------------------------------------------

DVElementObject::DVElementObject() {
    classFlags = 1;
    colorOutline = color565(0xff, 0xf7, 'Z');
    colorFocus = colorOutline;
}

// DVElementObject::LoadStateFromFile (0x0825b8f0)
int DVElementObject::LoadStateFromFile(SBFile& f) {
    int n = sprite->LoadSpriteFromFile(f, FRAME_OBJECT);
    active = sprite->visible;
    value9a = sprite->objectValue;
    animation = sprite->animation;
    u9c = f.U16();
    if (objectType == 0xc) {
        u94 = f.U16();
        u96 = f.U16();
        if (u96 < u94) u96 = (uint16_t)(u96 + 0x10);
        u98 = sprite->pos.direction;
        n += 6;
    } else {
        n += 2;
    }
    MakeSurfaces(2, 0, objectType == 9);
    return n;
}

// DVElementObject::Refresh (0x0825bfc0)
void DVElementObject::Refresh(uint32_t screen, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) {
    if (!active) return;
    bool hidden = (objectType == 0xb && animation <= 0x13 && ((0xc0001u >> (animation & 0x1f)) & 1)) ? true : silhouettes;
    DrawSprite(screen, zoom, view, true, hidden, shadows);
}

// ---- animated scenery ---------------------------------------------------------------------

DVElementFX::DVElementFX() { classFlags = 0; }

// DVElementFX::LoadStateFromFile (0x0824afb0)
int DVElementFX::LoadStateFromFile(SBFile& f) {
    int n = sprite->LoadSpriteFromFile(f, FRAME_FX);
    shadowKind = f.U8();
    active = f.U8() != 0;
    restore = f.U8() != 0;
    MakeSurfaces(0, 0, false);
    return n + 3;
}

// DVElementFX::Refresh (0x08249ea0): no masks, a shadow if the scenery has one
void DVElementFX::Refresh(uint32_t screen, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) {
    if (!active) return;
    DVSprite* s = sprite.get();
    if (!s->IsOnScreen(view, zoom)) return;
    DVspriteRenderingInfo info;
    info.surface = surface;
    info.hidden = true;
    s->CreateTargetSprite(info);
    SBGeoBoundingBox2D src, dst;
    s->GenerateBlitBox(view, zoom, src, dst);
    if (!src.IsOK() || !dst.IsOK()) return;
    SBDrawManager* d = SBDrawManager::mpDrawManager;
    if (shadowKind == 1 && shadows) {
        uint16_t pct = DVFrameHolder::mpFrameHolder ? DVFrameHolder::mpFrameHolder->shadowPercent : 40;
        d->BlitAlphaKeying(surface, &src, screen, &dst, shadowKey, pct, BLIT_COLORKEY);
    } else {
        d->Blit(surface, &src, screen, &dst, BLIT_COLORKEY);
    }
}

// DVElementFX::Hourglass (0x0824b330)
void DVElementFX::Hourglass() {
    if (!active) return;
    if (!patch) {
        sprite->PerformVirginIncrement(0);
        return;
    }
    // a patch's animation: the change plays forwards to apply it, backwards to take it off
    int r = sprite->PerformVirginIncrement(patch->applied && patch->animating ? 0xd : 0);
    if (r == 3 && patch->animating) {
        patch->animating = false;
        patch->ApplyFinal();
    }
}

// DVElementTarget::LoadStateFromFile (0x08265870)
int DVElementTarget::LoadStateFromFile(SBFile& f) {
    int n = DVElementFX::LoadStateFromFile(f);
    sprite->pos.ComputePositionSprite();
    int16_t x = f.S16(), y = f.S16();
    sprite->pos.SetPositionMap(SBGeoPoint2D((float)x, (float)y));
    sprite->pos.layer = f.U16();
    uint16_t sec = f.U16();
    sprite->pos.sector = sprite->grid && sec != 0xffff ? sprite->grid->Sector(sec) : nullptr;
    sprite->pos.valid |= 6;
    uint16_t ob = f.U16();
    if (ob == 0) sprite->pos.SetObstacle(nullptr);
    else sprite->pos.SetObstacle(sprite->grid ? sprite->grid->ViewArea(ob) : nullptr);
    sprite->pos.valid |= 1;
    value144 = f.U32();
    scriptName = f.String16();
    return n + 0x10 + (int)scriptName.size();
}

// DVElementTarget::Hourglass (0x08265c90)
void DVElementTarget::Hourglass() {
    if (active) sprite->PerformVirginIncrement(progression);
}
