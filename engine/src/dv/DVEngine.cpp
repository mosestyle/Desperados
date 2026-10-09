#include "dv/DVEngine.h"

#include "dv/DVArtificialIntelligence.h"
#include "dv/DVHikingGuide.h"

#include <algorithm>
#include <cmath>

#include "dv/DVElement.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVFrameHolder.h"
#include "dv/DVPathFinder.h"
#include "dv/DVSector.h"
#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"
#include "sb/SBPicture.h"

DVEngine* DVEngine::mpEngine = nullptr;

static const char* kDirLevels = "Data\\Levels";  // 0x085dd99c

DVEngine::DVEngine(SBDrawManager* d) : draw(d) {
    mpEngine = this;
    frames.reset(new DVFrameHolder());
    grid.reset(new DVFastFindGrid());
    pathFinder.reset(new DVPathFinder());
    pathFinder->SetObstacles(grid.get());
    hiking.reset(new DVHikingGuide());
    DVSector::ResetCounters();
}

DVEngine::~DVEngine() {
    ownElements.clear();
    elements.clear();
    if (draw) {
        if (background) draw->DeleteSurface(background);
        if (minimap) draw->DeleteSurface(minimap);
    }
    if (mpEngine == this) mpEngine = nullptr;
}

// DVEngine::StateGet (0x082a4540), the states used so far
uint32_t DVEngine::StateGet(int s) const {
    switch (s) {
    case 0xe: return nightPercent;
    case 0xf: return shadowKey;
    }
    return 0;
}

static uint32_t tag4(const uint8_t* t) { return (uint32_t)t[0] << 24 | t[1] << 16 | t[2] << 8 | t[3]; }

// DVEngine::LoadStateFromFile (0x08269a30)
bool DVEngine::LoadStateFromFile(const std::string& name) {
    levelName = name;
    SBFile f;
    std::string path = std::string(kDirLevels) + "\\" + name + ".dvd";
    if (f.Open(path, 1) != 0) {
        SBError(true, "DVEngine.cpp", 799, "Unable to locate state file %s", path.c_str());
        return false;
    }
    while (f.Tell() + 8 <= f.Size()) {
        uint8_t t[4];
        f.Serialize(t, 4);
        uint32_t len = f.U32();
        int start = f.Tell();
        uint32_t tag = tag4(t);
        int done = -1;
        switch (tag) {
        case 0x4d495343: done = LoadMiscFromFile(f); break;                 // MISC
        case 0x42474e44: done = LoadBackgroundFromFile(f); break;           // BGND
        case 0x4d4f5645:  // MOVE: the walkable areas, then the path finder's graph
            done = grid->LoadMotionObstaclesFromFile(f);
            done += pathFinder->LoadGraphFromFile(f);
            break;
        case 0x53474854: done = grid->LoadSightObstaclesFromFile(f); break;   // SGHT
        case 0x4d41534b: done = grid->LoadMaskFromFile(f); break;            // MASK
        case 0x454c454d: done = LoadElemFromFile(f); break;                 // ELEM
        case 0x57415953: done = hiking->LoadAllPathesFromFile(f); break;     // WAYS
        default: break;  // the others come with the next steps
        }
        if (done >= 0 && (uint32_t)done != len)
            SBError(true, "DVEngine.cpp", 0x3f2, "Processed and expected length are not matching in hunk %.4s.(%u != %d)",
                    (const char*)t, len, done);
        f.Skip(start + (int)len, 0);
    }
    // DVSprite::InitializeDisplayOrder etc. for every element, then sorted for drawing
    for (DVElement* e : elements) {
        e->sprite->pos.ComputePositionAll();
        e->sprite->ComputeDisplayOrder(nullptr, true);
    }
    SortForEngine();
    // the heroes, in the level's order; the first one is selected (and the camera on him, as
    // StateCenterOn does)
    heroes.clear();
    for (auto& e : ownElements)
        if (e->kind >= KIND_COOPER && e->kind <= KIND_MIA && e->active) heroes.push_back(static_cast<DVElementActor*>(e.get()));
    std::sort(heroes.begin(), heroes.end(), [](DVElementActor* a, DVElementActor* b) { return a->kind < b->kind; });
    if (!heroes.empty()) {
        Select(heroes[0]);
        camera = heroes[0]->sprite->pos.posMap - screen * 0.5f;
    }
    ClampCamera();
    // the NPCs (DVEngine +0xd88) and their brains (DVArtificialIntelligence::InitAI, called by
    // DVGame::GameLoop when the mission starts)
    npcs.clear();
    for (auto& e : ownElements)
        if ((e->kind == KIND_VILLAIN || e->kind == KIND_CIVILIAN) && e->active) {
            auto* a = static_cast<DVElementActor*>(e.get());
            a->frameOffset = (uint8_t)npcs.size();
            npcs.push_back(a);
        }
    for (DVElementActor* a : npcs)
        if (a->ai) a->ai->InitOneAI();
    return background != 0;
}

// DVEngine::LoadMiscFromFile (0x0826c920)
int DVEngine::LoadMiscFromFile(SBFile& f) {
    int start = f.Tell();
    f.version = f.U32();
    if (f.version != 6)
        SBError(true, "DVEngine.cpp", 0x64e, "Version Check Failed on %s (file version %u, expected version %u).", "Hunk Misc",
                f.version, 6);
    f.U8();             // +0x1d4
    f.S16();            // +0xb98 / 10
    f.S16();            // +0xb9c / 10
    // the view cones' colours (DVElementActorNPC::InitColors): calm, suspicious, alarmed
    coneColors[0] = f.U32();
    coneColors[1] = f.U32();
    coneColors[2] = f.U32();
    f.U8();
    DVArtificialIntelligence::muwStandardViewPolygonRadius = f.U16();
    hearingFactor = f.F32();
    if (hearingFactor == 0.0f) {
        SBError(false, "DVEngine.cpp", 0x673, "Hearing coeff imported as 0, forced to 0.75");
        hearingFactor = 0.75f;
    }
    night = f.U8() != 0;
    nightPercent = f.U8();
    uint32_t c = f.U32();
    shadowKey = (uint16_t)((c >> 0x13 & 0x1f) | (c >> 5 & 0x7e0) | (c << 8 & 0xf800));
    weather = f.U8();
    if (weather) {
        f.U16();
        f.U16();
    }
    return f.Tell() - start;
}

// DVEngine::LoadBackgroundFromFile (0x0826afd0)
int DVEngine::LoadBackgroundFromFile(SBFile& f) {
    f.version = f.U32();
    if (f.version != 4)
        SBError(true, "DVEngine.cpp", 0x73e, "Version Check Failed on %s (file version %u, expected version %u).", "Hunk Bgnd",
                f.version, 4);
    uint16_t len = f.U16();
    std::string name(len, '\0');
    if (len) f.Serialize(&name[0], len);
    name = name.c_str();
    backgroundName = name;
    SBPictureSixteen pic;
    if (!pic.LoadFromFile(std::string(kDirLevels) + "\\" + name + ".dvm"))
        SBError(true, "DVEngine.cpp", 0x750, "Unable to load the background %s", name.c_str());
    mapW = (float)pic.GetWidth();
    mapH = (float)pic.GetHeight();
    background = draw->CreateSurface(pic.GetWidth(), pic.GetHeight());
    grid->SizeMap((uint16_t)(int)(mapW * 0.015625f), (uint16_t)(int)(0.015625f * mapH));
    draw->UploadPicture(background, pic.Pixels().data(), pic.GetWidth(), pic.GetHeight());
    int a = f.Tell();
    SBPictureSixteen mini;
    mini.LoadFromStream(f);
    minimap = draw->CreateSurface(mini.GetWidth(), mini.GetHeight());
    draw->UploadPicture(minimap, mini.Pixels().data(), mini.GetWidth(), mini.GetHeight());
    int b = f.Tell();
    return (len + 6) - a + b + 4 - 4;
}

// DVEngine::LoadElemFromFile (0x0826b930)
int DVEngine::LoadElemFromFile(SBFile& f) {
    f.version = f.U32();
    if (f.version != 0x1c)
        SBError(true, "DVEngine.cpp", 0x476, "Version Check Failed on %s (file version %u, expected version %u).", "Hunk Elem",
                f.version, 0x1c);
    uint16_t count = f.U16();
    int n = 6;
    for (uint16_t i = 0; i < count; ++i) {
        uint16_t kind = f.U16();
        DVElement* e = nullptr;
        if (kind >= KIND_COOPER && kind <= KIND_MRLEONE) e = new DVElementActor();
        else if (kind == KIND_VILLAIN || kind == KIND_CIVILIAN || kind == KIND_HORSE) e = new DVElementActor();
        else if (kind >= 0x210 && kind <= 0x215) e = new DVElementActor();
        else if (kind == KIND_OBJECT) e = new DVElementObject();
        else if (kind == KIND_TARGET) e = new DVElementTarget();
        else if (kind == KIND_FX) e = new DVElementFX();
        else if (kind >= 0x1101 && kind <= 0x1117 && kind != 0x110f) {
            static const uint8_t types[] = {6, 3, 1, 7, 2, 4, 8, 5, 9, 0xd, 0xb, 0xc, 0xf, 0xe, 0, 0x11, 0x10, 0x13, 0x14, 0x15, 0x16, 0x17, 0x12};
            auto* o = new DVElementObject();
            o->objectType = types[kind - 0x1101];
            e = o;
        } else {
            SBError(true, "DVEngine.cpp", 0x562, "Unknow object in hunk elem index=%d", i);
            return -1;
        }
        e->kind = kind;
        if (kind == KIND_VILLAIN) e->classFlags = 0x17;
        else if (kind == KIND_CIVILIAN) e->classFlags = 7;
        else if (kind >= KIND_COOPER && kind <= KIND_MIA) e->classFlags = kind == KIND_COOPER ? 0x10f : 0xf;
        else if (kind == KIND_HORSE) e->classFlags = 0x203;
        else if (kind == KIND_MRLEONE || (kind >= 0x210 && kind <= 0x215)) e->classFlags = 3;
        // outline colours of the constructors
        auto rgb = [](uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)((b >> 3) | ((r & 0xf8u) * 0x100 + (g & 0xfcu) * 8)); };
        if (kind == KIND_VILLAIN) e->colorOutline = e->colorSelect = e->colorFocus = rgb(0xff, 0, 0);
        else if (kind == KIND_CIVILIAN) e->colorOutline = e->colorSelect = e->colorFocus = rgb(0, 0xbe, 0xff);
        else if (kind >= KIND_COOPER && kind <= KIND_MIA) e->colorOutline = e->colorSelect = e->colorFocus = rgb(0xa5, 0xff, 'R');
        else if (kind == KIND_MRLEONE || kind == KIND_HORSE || (kind >= 0x210 && kind <= 0x215)) e->colorOutline = rgb(0x8c, 0x8c, 0x8c);
        ownElements.emplace_back(e);
        int k = e->LoadStateFromFile(f);
        // the heroes' own weapons go to their second list (+0xa0), everything else on the map
        bool onMap = true;
        if (kind == KIND_MRLEONE) e->active = false;
        if (kind == 0x1101 || kind == 0x1103 || kind == 0x1106 || kind == 0x1109 || kind == 0x110a || kind == 0x110e)
            onMap = false;
        if (kind == 0x110d && !e->active) {
            for (DVElement* o : elements)
                if (o->kind == KIND_SANCHEZ) onMap = false;
        }
        if (onMap) elements.push_back(e);
        n += 2 + k;
    }
    return n;
}

// PredEngine (0x082a3ba0) and DVEngine::SortForEngine (0x08272070)
void DVEngine::SortForEngine() {
    std::sort(elements.begin(), elements.end(), [](DVElement* a, DVElement* b) {
        int ba = a->sprite->displayBias, bb = b->sprite->displayBias;
        if (ba != bb) return ba < bb;
        float fa = a->sprite->displayOrder, fb = b->sprite->displayOrder;
        if (fa != fb) return fa < fb;
        return a->creation < b->creation;
    });
}

void DVEngine::ClampCamera() {
    float w = screen.x / zoom, h = screen.y / zoom;
    float maxX = mapW - w, maxY = mapH - h;
    if (camera.x > maxX) camera.x = maxX;
    if (camera.y > maxY) camera.y = maxY;
    if (camera.x < 0) camera.x = 0;
    if (camera.y < 0) camera.y = 0;
}

// DVEngine::PerformHourglass (0x08272300), the part these steps have: every element's 25 Hz
// step (heroes' orders, scenery animations), then the display order again
void DVArtificialIntelligenceTick();
void DVEngine::PerformHourglass() {
    ++ticks;
    DVArtificialIntelligenceTick();
    for (DVElement* e : elements) e->Hourglass();
    SortForEngine();
}

void DVEngine::Select(DVElementActor* h) {
    for (DVElementActor* o : heroes) o->selected = false;
    selected = h;
    if (h) {
        h->selected = true;
        h->selectionPulseLen = 20;
        h->selectionPulse = 20;
    }
}

DVElementActor* DVEngine::HeroAt(const SBGeoPoint2D& p, float slack) const {
    DVElementActor* best = nullptr;
    for (DVElementActor* h : heroes) {
        if (!h->active) continue;
        DVSprite* s = h->sprite.get();
        SBGeoBoundingBox2D b = s->boxOnMap;
        if (!b.valid) continue;
        b.p0 = b.p0 - SBGeoVector2D(slack, slack);
        b.p1 = b.p1 + SBGeoVector2D(slack, slack);
        if (!b.IsInside_p(p)) continue;
        if (!best || best->sprite->displayOrder < s->displayOrder) best = h;
    }
    return best;
}

bool DVEngine::OrderMove(const SBGeoPoint2D& p, bool run) {
    if (!selected) return false;
    return selected->MoveTo(p, run);
}

void DVEngine::OrderMakeRunning() {
    if (selected) selected->MakeRunning();
}

void DVEngine::OrderCrouch() {
    if (!selected) return;
    if (selected->Posture() == POSTURE_LYING) selected->MakeWalking();
    else selected->MakeCrawling();
}

void DVEngine::OrderStop() {
    if (selected) selected->Stop();
}

// DVEngine::Draw (0x08276f10) mode 2 / 5 and DrawBackground (0x082797f0)
void DVEngine::Draw() {
    SBGeoPoint2D cam(std::floor(camera.x), std::floor(camera.y));
    SBGeoVector2D view = screen / zoom;
    SBGeoBoundingBox2D src(cam, cam + view);
    SBGeoBoundingBox2D dst(SBGeoPoint2D(0, 0), SBGeoPoint2D(screen.x, screen.y));
    draw->Blit(background, &src, 1, &dst, 0);
    PerformRefreshAllElements(1);
}

// DVEngine::PerformRefreshAllElements (0x08279ad0): every element, in display order
void DVEngine::PerformRefreshAllElements(uint32_t surface) {
    SBGeoPoint2D cam(std::floor(camera.x), std::floor(camera.y));
    SBGeoBoundingBox2D viewBox(cam, cam + screen / zoom);
    for (DVElement* e : elements) e->Refresh(surface, zoom, viewBox, silhouettes);
}
