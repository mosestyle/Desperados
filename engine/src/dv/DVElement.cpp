#include "dv/DVElement.h"

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
        scriptName = f.String16();
        int len = (int)scriptName.size();
        // the AI attributes (DVArtificialMalignity / Bonhomie::LoadAttributesFromFile)
        int ai;
        if (kind == KIND_VILLAIN) {
            profile = f.U32();
            aiShort = f.U16();
            aiAttitude = f.U8() & 0x7f;
            aiByte = f.U8();
            ai = 8;
        } else {
            aiValue = f.U32();
            aiAttitude = f.U8() & 0x7f;
            aiByte = f.U8();
            ai = 6;
        }
        // DVPath::LoadFromFile: the patrol route
        pathIndex = f.U16();
        f.U16();  // (stored in the AI: +0x24)
        MakeSurfaces(2, 2, true);
        return ai + 2 + 4 + n + len;
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

// DVElementActor::Hourglass (0x0820b2e0) with the heroes' Execute (DVElementActorPC 0x08244e80,
// DVElementActorHuman 0x0821e420, DVElementActor 0x0820da70) for the commands of this step
void DVElementActor::Hourglass() {
    if (selectionPulse > 0) --selectionPulse;
    if (!IsHero() || !active) return;
    DVSprite* s = sprite.get();
    DVPositionInterface& pos = s->pos;
    if (orders.empty()) {
        if (defaultCommand == CMD_NONE) SetDefaultWaitAction();
        if (defaultCommand == CMD_LYING) {
            s->PerformAction(0, CMD_LYING, 0xb, false);
        } else {
            int r = s->PerformAction(0, CMD_WAIT, 4, false);
            if (r == 1) SetStates(POSTURE_STANDING, 0);
        }
        return;
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
    }
    if (r == 3 || r == 4) {
        orders.pop_front();
        newOrder = true;
        if (orders.empty()) defaultCommand = CMD_NONE;
    }
    s->displayOrder = pos.pos3D.y;
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
