#include "dv/DVEngine.h"

#include "dv/DVArtificialIntelligence.h"
#include "dv/DVHikingGuide.h"
#include "dv/DVArmsTrader.h"
#include "dv/DVDialogue.h"
#include "dv/DVSequence.h"
#include "dv/DVScript.h"
#include "dv/DVShadowPolygon.h"

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

std::unique_ptr<DVArmsTrader> DVEngine::sArms;
std::unique_ptr<DVPsychoanalyst> DVEngine::sPsy;

DVEngine::DVEngine(SBDrawManager* d) : draw(d) {
    mpEngine = this;
    // the weapons' and the characters' catalogues (DVGame: once, at the start)
    if (!sArms) {
        sArms.reset(new DVArmsTrader());
        sArms->LoadWeaponCatalogFromFile();
    }
    if (!sPsy) {
        sPsy.reset(new DVPsychoanalyst());
        sPsy->LoadCompendiumOfPsychologyFromFile();
    }
    frames.reset(new DVFrameHolder());
    grid.reset(new DVFastFindGrid());
    pathFinder.reset(new DVPathFinder());
    pathFinder->SetObstacles(grid.get());
    hiking.reset(new DVHikingGuide());
    shadow.reset(new DVShadowPolygon());
    sequences.reset(new DVSequenceManager());
    dialogue.reset(new DVDialogue());
    script.reset(new DVScript());
    DVSector::ResetCounters();
    DVArtificialIntelligence::mCurrentOverallAlertStatus = 0;
    DVArtificialIntelligence::mbFreeze = false;
    DVArtificialIntelligence::mCurrentOverallVillainAlertStatus = 0;
}

DVEngine::~DVEngine() {
    script.reset();
    sequences.reset();
    dialogue.reset();
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
        case 0x53435250: done = script->LoadScriptObjectsFromFile(f); break;  // SCRP
        case 0x444c4753: done = dialogue->LoadDialogue(f); break;            // DLGS
        case 0x4c494654: done = grid->LoadLiftsFromFile(f); break;            // LIFT
        case 0x4255494c: {                                                   // BUIL
            std::vector<DVElementActor*> actors;  // the characters (+0xda4) in their loading order
            for (auto& e : ownElements)
                if (auto* a = dynamic_cast<DVElementActor*>(e.get())) actors.push_back(a);
            done = grid->LoadBuildingsFromFile(f, actors);
            break;
        }
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
        if (e->kind >= KIND_COOPER && e->kind <= KIND_MIA) heroes.push_back(static_cast<DVElementActor*>(e.get()));
    std::sort(heroes.begin(), heroes.end(), [](DVElementActor* a, DVElementActor* b) { return a->kind < b->kind; });
    for (DVElementActor* h : heroes)
        if (h->active) {
            Select(h);
            camera = h->sprite->pos.posMap - screen * 0.5f;
            break;
        }
    ClampCamera();
    // the NPCs (DVEngine +0xd88) and their brains (DVArtificialIntelligence::InitAI, called by
    // DVGame::GameLoop when the mission starts)
    npcs.clear();
    characters.clear();
    for (auto& e : ownElements)
        if (auto* a = dynamic_cast<DVElementActor*>(e.get())) characters.push_back(a);
    for (auto& e : ownElements)
        if (e->kind == KIND_VILLAIN || e->kind == KIND_CIVILIAN) {
            auto* a = static_cast<DVElementActor*>(e.get());
            a->frameOffset = (uint8_t)npcs.size();
            npcs.push_back(a);
        }
    for (DVElementActor* a : npcs)
        if (a->ai) a->ai->InitOneAI();
    // DVArtificialIntelligence::InitAI: the prisoners (the NPCs of role 0x16 and the heroes a
    // menacer sees around him) each held by the nearest menacer (role 0x1b), one each
    {
        std::vector<DVElementActor*> menacers, prisoners;
        for (DVElementActor* a : npcs)
            if (a->ai && a->active) {
                if (a->ai->role == 0x1b) menacers.push_back(a);
                if (a->ai->role == 0x16) prisoners.push_back(a);
            }
        for (DVElementActor* h : heroes) {
            if (!h->active) continue;
            for (DVElementActor* m : menacers) {
                if (m->sprite->pos.layer != h->sprite->pos.layer) continue;
                SBGeoVector2D d = h->sprite->pos.posMap - m->sprite->pos.posMap;
                if (d.x * d.x + d.y * d.y > (float)m->view.radius * (float)m->view.radius) continue;
                prisoners.push_back(h);
                break;
            }
        }
        if (!prisoners.empty() && menacers.empty())
            SBError(false, "DVArtificialIntelligence.cpp", 0x110b, "There are %zu prisoners, but no menacers", prisoners.size());
        for (DVElementActor* p : prisoners) {
            DVElementActor* best = nullptr;
            float bd = 0;
            for (DVElementActor* m : menacers) {
                SBGeoVector2D d = p->sprite->pos.posMap - m->sprite->pos.posMap;
                float q = d.x * d.x + d.y * d.y;
                if (!best || q < bd) best = m, bd = q;
            }
            if (!best) {
                SBError(false, "DVArtificialIntelligence.cpp", 0x1170, "Prisoner at point (%u, %u) couldn't find a menacer.",
                        (unsigned)p->sprite->pos.posMap.x, (unsigned)p->sprite->pos.posMap.y);
                continue;
            }
            menacers.erase(std::find(menacers.begin(), menacers.end(), best));
            SBLog("%s is held at gunpoint by %s at %.0f,%.0f", p->KindName(), best->KindName(), best->sprite->pos.posMap.x,
                  best->sprite->pos.posMap.y);
            p->menacer = best;
            best->SetViewTarget(p, true);
            best->SetViewCone(8);
            if (best->ai) best->ai->target = p;
        }
    }
    for (DVElementActor* h : heroes) h->InitFirearm();
    // the mission's scripts (DVScript::Initialize): StartUp and the scripted elements
    {
        std::vector<DVElement*> all;
        for (auto& e : ownElements) all.push_back(e.get());
        script->Initialize(this, all, levelName);
    }
    if (!selected || !selected->playable || (!selected->active && !selected->InBuilding())) SelectNextPlayable();
    if (selected && !cameraMoved) camera = selected->sprite->pos.posMap - screen * 0.5f;
    ClampCamera();
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
    DVElementActor::InitColors(coneColors[0], coneColors[1], coneColors[2], 0xffa0ff);
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
    if (dialogue) dialogue->Refresh();
    // the scripts: StartUp's HourGlass each second, CheckVictoryCondition every third
    if (script && script->loaded && missionResult == 0 && !missionFailed && frameCounter % 25 == 0) {
        uint32_t sec = frameCounter / 25;
        script->HourGlass(sec);
        if (sec % 3 == 0 || forceVictoryCheck) {
            forceVictoryCheck = false;
            int r = script->CheckVictoryCondition(sec);
            if (r == 1) {
                missionResult = 1;
                SBLog("mission accomplished");
            } else if (r == 2) {
                missionResult = 2;
                missionFailed = true;
                SBLog("mission failed (the script)");
            }
        }
    }
    ++frameCounter;
    DVArtificialIntelligenceTick();
    if (sequences) sequences->Hourglass();
    // the camera the scripts move (DVEngine::PerformManageKeyScrolling)
    if (cameraScrolling) {
        SBGeoVector2D d = cameraGoal - camera;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        float speed = cameraFixedSpeed ? (float)cameraFixedSpeed : cameraRamp;
        cameraRamp = std::min(cameraRamp * 1.05f, 31.1f);
        if (len <= speed + 0.5f) {
            camera = cameraGoal;
            cameraScrolling = false;
            if (DVSequenceElement* e = cameraElement) {
                cameraElement = nullptr;
                e->SetState(SEQ_DONE);
            }
        } else {
            camera = camera + d * (speed / len);
        }
        cameraMoved = true;
    }
    if (follow && follow->sprite && !cameraScrolling) {
        camera = CheckLocationIsValidForCamera(follow->sprite->pos.posMap);
        cameraMoved = true;
    }
    for (DVElement* e : elements) e->Hourglass();
    // the zones the characters came into or left
    if (script && script->loaded)
        for (DVElementActor* a : characters)
            if (a->active) script->CheckZones(a, false);
    // the scripts' timers (in ticks)
    for (size_t i = 0; i < timers.size();) {
        DVSequenceElement* e = timers[i];
        if (--e->param < 0) {
            timers.erase(timers.begin() + i);
            e->SetState(SEQ_DONE);
        } else {
            ++i;
        }
    }
    SortForEngine();
}

void DVEngine::PerformExecuteCommand(DVSequenceElement* e) {
    switch (e->command) {
    case 4:
        LockUser(true);
        break;
    case 5:
        LockUser(false);
        break;
    case 6:  // the camera scrolls there; the element waits for it
        if (DVSequenceElement* old = cameraElement) {
            cameraElement = nullptr;
            old->SetState(SEQ_DONE);
        }
        cameraElement = e;
        e->state = SEQ_RUNNING;
        CameraScrollTo(e->point, 2.0f, e->param16);
        return;
    case 7:
        if (DVSequenceElement* old = cameraElement) {
            cameraElement = nullptr;
            old->SetState(SEQ_DONE);
        }
        CameraJumpTo(e->point);
        break;
    case 8:
        zoomWanted = e->value;
        break;
    case 9:
        mapShown = e->flag10;
        break;
    case 0xb:  // a timer
        e->state = SEQ_RUNNING;
        timers.push_back(e);
        return;
    case 0xc:  // a dialogue; the element waits for its end
        if (dialogue && dialogue->PreparePlayDialogue((uint32_t)e->param)) {
            if (dialogue->element && dialogue->element != e) dialogue->element->SetState(SEQ_DONE);
            dialogue->element = e;
            e->state = SEQ_RUNNING;
            return;
        }
        break;
    case 0xd:  // the briefing starts (PerformSnapshot / SaveStateBeforeBriefing are for the save)
        briefing = true;
        LockUser(true);
        break;
    case 0xe:
        briefing = false;
        LockUser(false);
        if (fast) {
            fast = false;
            if (dialogue) dialogue->fast = false;
        }
        break;
    case 0xf:  // SelectFollowElement
        if (DVSequenceElement* old = cameraElement) {
            cameraElement = nullptr;
            old->SetState(SEQ_DONE);
        }
        follow = e->target;
        break;
    case 0x10:
        follow = nullptr;
        break;
    case 0x56:
        if (auto* a = dynamic_cast<DVElementActor*>(e->actor))
            if (e->param16 < 6) a->actionAvailable[e->param16] = e->flag11;
        break;
    case 0x57:
        if (auto* a = dynamic_cast<DVElementActor*>(e->actor)) {
            a->playable = e->flag11;
            if (!a->playable && selected == a) SelectNextPlayable();
            if (a->playable && !selected) Select(a);
        }
        break;
    default:
        break;
    }
    e->SetState(SEQ_DONE);
}

void DVEngine::LockUser(bool on) {
    if (on == userLocked) return;
    userLocked = on;
}

SBGeoPoint2D DVEngine::CheckLocationIsValidForCamera(const SBGeoPoint2D& p) const {
    SBGeoPoint2D c(std::floor(p.x - screen.x / (zoom * 2.0f)), std::floor(p.y - screen.y / (zoom * 2.0f)));
    float w = screen.x / zoom, h = screen.y / zoom;
    if (c.x + w > mapW) c.x = mapW - w;
    if (c.y + h > mapH) c.y = mapH - h;
    if (c.x < 0) c.x = 0;
    if (c.y < 0) c.y = 0;
    return c;
}

void DVEngine::CameraScrollTo(const SBGeoPoint2D& p, float speed, uint16_t fixedSpeed) {
    (void)speed;
    cameraGoal = CheckLocationIsValidForCamera(p);
    cameraScrolling = true;
    cameraFixedSpeed = fixedSpeed;
    cameraRamp = 6.0f;
    follow = nullptr;
}

void DVEngine::CameraJumpTo(const SBGeoPoint2D& p) {
    camera = CheckLocationIsValidForCamera(p);
    cameraScrolling = false;
    cameraMoved = true;
    follow = nullptr;
}

void DVEngine::AddSentence(uint32_t id) {
    for (const Sentence& s : sentences)
        if (s.id == id) return;
    sentences.push_back({id, false});
}

void DVEngine::DoneSentence(uint32_t id) {
    for (Sentence& s : sentences)
        if (s.id == id) s.done = true;
}

void DVEngine::StateSelectActor(uint16_t kind) {
    for (DVElementActor* h : heroes)
        if (h->kind == kind && h->playable && (h->active || h->InBuilding())) Select(h);
}

// the intro skipped: the game runs at once to its end (the original loads the state saved after
// it, Data\Levels\Briefing\..., ISetSkipData)
void DVEngine::SkipBriefing() {
    if (!briefing) return;
    fast = true;
    if (dialogue) {
        dialogue->fast = true;
        dialogue->Mute();
    }
    for (int i = 0; i < 25 * 60 * 10 && briefing && !missionFailed; ++i) PerformHourglass();
    fast = false;
    if (dialogue) dialogue->fast = false;
    cameraMoved = true;
}

void DVEngine::SelectNextPlayable() {
    for (DVElementActor* h : heroes)
        if (h->playable && (h->active || h->InBuilding()) && !h->dead) {
            Select(h);
            return;
        }
    Select(nullptr);
}

void DVEngine::Select(DVElementActor* h) {
    if (h && (!h->playable || (!h->active && !h->InBuilding()))) return;
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
        if (!h->active || !h->playable) continue;
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

DVElementActor* DVEngine::NPCAt(const SBGeoPoint2D& p, float slack) const {
    DVElementActor* best = nullptr;
    for (DVElementActor* n : npcs) {
        if (!n->active || !n->sprite) continue;
        DVSprite* s = n->sprite.get();
        SBGeoBoundingBox2D b = s->boxOnMap;
        if (!b.valid) continue;
        b.p0 = b.p0 - SBGeoVector2D(slack, slack);
        b.p1 = b.p1 + SBGeoVector2D(slack, slack);
        if (!b.IsInside_p(p)) continue;
        if (!best || best->sprite->displayOrder < s->displayOrder) best = n;
    }
    return best;
}

void DVEngine::ToggleWatched(DVElementActor* n) { watched = watched == n ? nullptr : n; }

bool DVEngine::OrderShoot(DVElement* target) {
    if (!selected || missionFailed) return false;
    if (userLocked || missionResult) return false;
    return selected->OrderShoot(target);
}

void DVEngine::OrderReload() {
    if (selected && !missionFailed) selected->Reload();
}

void DVEngine::Noise(uint32_t type, const SBGeoPoint3D& at, DVElementActor* who) {
    uint16_t radius = 0;
    switch (type) {
    case 0: radius = who && who->kind == KIND_KATE ? 400 : 0x28a; break;
    case 1: case 0xb: case 0xe: case 0x10: radius = 500; break;
    case 2: case 3: case 0xc: radius = 0x226; break;
    case 4: case 5: radius = 0x4b0; break;
    case 6: case 0x14: radius = 100; break;
    case 7: radius = 800; break;
    case 8: radius = 300; break;
    case 9: case 0xf: case 0x11: case 0x12: radius = 200; break;
    case 0xd: radius = 0xfa; break;
    case 0x13: radius = 1000; break;
    default: break;
    }
    if (!radius) return;
    // DVElementActorNPC::GetHearVolume: the noise reaches as far as its radius times the
    // level's hearing factor (the walls between and the deafness come later)
    float reach = (float)radius * hearingFactor;
    for (DVElementActor* n : npcs) {
        if (!n->ai || n == who || n->dead || !n->active) continue;
        const SBGeoPoint3D& p = n->sprite->pos.pos3D;
        float dx = p.x - at.x, dy = (p.y - at.y) * 1.7434468f;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d >= reach) continue;
        DVStimulus st = DVStimulus::Of(STIM_HEAR);
        st.point = SBGeoPoint2D(at.x, at.y - at.z);
        st.u10 = type;
        st.u14 = (uint16_t)(reach - d);
        st.element = who;
        st.source = 3;
        n->ai->Think(st);
    }
}

void DVEngine::HeroDied(DVElementActor* hero) {
    if (missionFailed) return;
    missionFailed = true;
    deadHero = hero;
    SBLog("%s is dead: mission failed", hero ? hero->KindName() : "a hero");
}

bool DVEngine::OrderMove(const SBGeoPoint2D& p, bool run) {
    if (!selected || selected->dead || missionFailed) return false;
    if (userLocked || missionResult) return false;
    return selected->MoveTo(p, run);
}

void DVEngine::OrderMakeRunning() {
    if (selected) selected->MakeRunning();
}

void DVEngine::OrderCrouch() {
    if (!selected || selected->dead || missionFailed || userLocked) return;
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
    // the watched NPC's view cone, over the ground and under the characters
    // (PerformRefreshAllElements: after the flat items, ShowDetectionPolygon)
    if (watched && shadow) shadow->Display(1, cam, zoom, watched, night);
    PerformRefreshAllElements(1);
}

// DVEngine::PerformRefreshAllElements (0x08279ad0): every element, in display order
void DVEngine::PerformRefreshAllElements(uint32_t surface) {
    SBGeoPoint2D cam(std::floor(camera.x), std::floor(camera.y));
    SBGeoBoundingBox2D viewBox(cam, cam + screen / zoom);
    for (DVElement* e : elements) e->Refresh(surface, zoom, viewBox, silhouettes);
}
