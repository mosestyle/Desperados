#include "dv/DVElement.h"

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
        f.Skip(len, 1);
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
    if (!active) return;
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
bool DVElementActor::BuildMove(const SBGeoPoint2D& goal, uint32_t command, uint32_t movePosture,
                               std::deque<DVOrder>& out) const {
    DVSprite* s = sprite.get();
    DVPositionInterface& pos = s->pos;
    DVFastFindGrid* grid = s->grid;
    if (!grid) return false;
    if (goal.x == 0.0f && goal.y == 0.0f) {
        SBError(false, "DVElementActor.cpp", 0x9ad, "VERBOTEN: Actor trying to go to (0,0)!");
        return false;
    }
    SBGeoVector2D half = pos.GetMoveBox(movePosture).p1;
    if (grid->IsReachableThick(pos.posMap, goal, pos.layer, half)) {
        DVOrder o;
        o.id = DVOrder::NewID();
        o.command = command;
        o.goal = goal;
        out.push_back(o);
        return true;
    }
    // DVPathFinder::AddPathRequest (0x0832aa40): a start inside a wall is moved out first
    SBGeoPoint2D start = pos.posMap;
    SBGeoBoundingBox2D mb = pos.GetMoveBoxMap();
    bool adjusted = false;
    if (!grid->IsPositionAutorized(mb, pos.layer)) {
        adjusted = true;
        if (!grid->FindAutorizedPosition(mb, pos.layer)) {
            SBError(false, "DVPathFinder.cpp", 0x19c, "Actor in a wall at (%f,%f)", pos.posMap.x, pos.posMap.y);
            return false;
        }
        start = SBCenter(mb);
    }
    if (!pos.sector || (pos.sector->type & 3) != 3) {
        SBLog("%s is on no walkable area", KindName());
        return false;
    }
    uint16_t area = static_cast<DVSectorMotionArea*>(pos.sector)->areaIndex;
    DVPathFinder* pf = DVEngine::mpEngine ? DVEngine::mpEngine->PathFinder() : nullptr;
    if (!pf) return false;
    std::vector<SBGeoPoint2D> path;
    if (!pf->FindPath(pos.layer, area, pos.GetPathFinderIndex(movePosture), start, goal, adjusted, path)) {
        SBLog("no way from %.0f,%.0f to %.0f,%.0f (layer %u area %u size %u)", start.x, start.y, goal.x, goal.y, pos.layer,
              area, pos.GetPathFinderIndex(movePosture));
        return false;
    }
    for (size_t i = adjusted ? 0 : 1; i < path.size(); ++i) {
        DVOrder o;
        o.id = DVOrder::NewID();
        o.command = command;
        o.goal = path[i];
        out.push_back(o);
    }
    return !out.empty();
}

// a move ordered by the player (DVEngine::PerformGroupMove for one hero, 0x08294710): the place
// is moved out of the walls (FindAutorizedPosition), lying heroes crawl, a run first stands up
bool DVElementActor::MoveTo(const SBGeoPoint2D& dest, bool run) {
    DVSprite* s = sprite.get();
    DVPositionInterface& pos = s->pos;
    DVFastFindGrid* grid = s->grid;
    if (!grid || !active) return false;
    SBGeoBoundingBox2D box = pos.GetMoveBoxMap() - pos.posMap + dest;
    if (!grid->FindAutorizedPosition(box, dest, pos.layer)) {
        SBLog("no place near %.0f,%.0f", dest.x, dest.y);
        return false;
    }
    SBGeoPoint2D goal = SBCenter(box);
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
    if (!BuildMove(goal, cmd, movePosture, seq)) return false;
    orders = seq;
    newOrder = true;
    return true;
}

void DVElementActor::Stop() {
    orders.clear();
    newOrder = true;
    defaultCommand = CMD_NONE;
}

// DVElementActor::MakeRunning (0x08214dd0): the move goes on running (standing up first)
void DVElementActor::MakeRunning() {
    if (orders.empty()) return;
    bool lying = sprite->pos.posture == POSTURE_LYING;
    for (DVOrder& o : orders)
        if (o.command == CMD_WALK || o.command == CMD_CRAWL) o.command = CMD_RUN;
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
    if (!active) return;
    if (IsNPC()) {
        NPCHourglass();
        return;
    }
    if (!IsHero()) return;
    StepOrders();
}

// one step of the current orders; true when the last one just ended
bool DVElementActor::StepOrders() {
    DVSprite* s = sprite.get();
    DVPositionInterface& pos = s->pos;
    if (orders.empty()) {
        if (defaultCommand == CMD_NONE) SetDefaultWaitAction();
        if (defaultCommand == CMD_LYING) {
            s->PerformAction(0, CMD_LYING, 0xb, false);
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
        r = s->PerformMotion(o.id, o.goal, o.tolerance, CMD_WALK, 1, 0, false);
        if (r == 1) SetStates(POSTURE_STANDING, 1);
        else if (r == 3) SetStates(POSTURE_STANDING, 0);
        break;
    case CMD_RUN:
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
        orders.pop_front();
        newOrder = true;
        if (orders.empty()) {
            defaultCommand = IsNPC() ? CMD_WAIT : CMD_NONE;
            ended = true;
        }
    }
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

// DVArtificialIntelligence::InitState: the starting state given by the level
bool DVElementActor::InitState(DVArtificialIntelligence& a) {
    a.riding = false;
    auto timer = [&]() {
        int r = (std::rand() & 0x7fff) % 0x46 + 0x1e;
        LaunchTimer(r ? (uint32_t)r : 1u, false);
    };
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
    if (!b) return;
    if (ended && isMove) {
        isMove = false;
        b->SequenceDone(true, false);
    }
    RefreshView();
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
    sprite->PerformVirginIncrement(0);
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

void DVElementTarget::Hourglass() {}
