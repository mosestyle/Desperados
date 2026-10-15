#include "dv/DVScript.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <map>

#include "dv/DVArtificialIntelligence.h"
#include "dv/DVDialogue.h"
#include "dv/DVDoor.h"
#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVHikingGuide.h"
#include "dv/DVLine.h"
#include "dv/DVSector.h"
#include "dv/DVSequence.h"
#include "dv/DVSprite.h"
#include "sb/SBFile.h"

extern int desprand();

DVScript* DVScript::mpInstance = nullptr;
DVElement* DVScript::scriptThis = nullptr;

int32_t DVScript::Handle(const DVElement* e) {
    if (!e || !mpInstance) return 0;
    const auto& a = mpInstance->marrayElement;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] == e) return (int32_t)i + 1;
    return 0;
}

DVElement* DVScript::Element(int32_t h) {
    if (!mpInstance || h <= 0 || (size_t)h > mpInstance->marrayElement.size()) return nullptr;
    return mpInstance->marrayElement[(size_t)h - 1];
}

DVObjectScript* DVScript::Location(int32_t h) const {
    if (h < kLocation) return nullptr;
    size_t i = (size_t)(h - kLocation);
    return i < locations.size() ? locations[i].get() : nullptr;
}

int32_t DVScript::AddPoint(const SBGeoPoint2D& p, uint16_t layer, DVSector* area) {
    auto o = std::make_unique<DVObjectScript>();
    o->type = 0;
    o->p = p;
    o->layer = layer;
    o->area = area;
    locations.push_back(std::move(o));
    return kLocation + (int32_t)locations.size() - 1;
}

namespace {

DVEngine* Eng() { return DVEngine::mpEngine; }
DVScript* S() { return DVScript::mpInstance; }
DVElementActor* Actor(int32_t h) { return dynamic_cast<DVElementActor*>(DVScript::Element(h)); }
DVObjectScript* Loc(int32_t h) { return S() ? S()->Location(h) : nullptr; }
DVObjectScript* Point(int32_t h) {
    DVObjectScript* o = Loc(h);
    return o && o->type == 0 ? o : nullptr;
}
float F(int32_t v) {
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
bool IsChar(DVElement* e) { return e && (e->classFlags & 3) == 3; }

DVElement* HeroOfKind(int kind) {
    if (!S()) return nullptr;
    for (DVElement* e : S()->marrayElement)
        if (e && e->kind == kind) return e;
    return nullptr;
}

std::map<int32_t, int32_t> gGlobals;           // DVScript::InitGlobal / SetGlobal / GetGlobal
std::map<int64_t, int32_t> gPersistent;        // the campaign's persistent properties

// ---- the sequence being recorded ---------------------------------------------------------------

bool Recording(const char* what) {
    if (S() && S()->muwSequenceLevel && S()->mpSequence) return true;
    SBError(false, "DVScript.cpp", 0xbed, "Script Error: to declare a %s sequence you must initialize the sequence at first",
            what);
    return false;
}
DVSequenceElement* Rec() {
    auto* e = new DVSequenceElement();
    e->level = S()->muwSequenceLevel;
    S()->mpSequence->Append(e);
    return e;
}
DVSequenceElement* RecQuick(uint32_t cmd, DVElement* who, int32_t param, bool f10 = false, bool f11 = false) {
    DVSequenceElement* e = Rec();
    e->AsQuickSequence(S()->muwSequenceLevel, cmd, who, param, f10, 0xb9, f11);
    return e;
}

// ---- the natives (VMCoreCustom::I..., DVScript::...) -------------------------------------------

int32_t IStartDialog(VMCore&, const int32_t* p) {
    if (Eng() && Eng()->dialogue) Eng()->dialogue->PreparePlayDialogue((uint32_t)p[0]);
    return 1;
}
int32_t ISetCameraScrollTo(VMCore&, const int32_t* p) {
    if (DVObjectScript* o = Point(p[0])) Eng()->CameraScrollTo(o->p, 2.0f, 0);
    return 1;
}
int32_t ISetCameraScrollSlowlyTo(VMCore&, const int32_t* p) {
    if (DVObjectScript* o = Point(p[0])) Eng()->CameraScrollTo(o->p, F(p[1]), 0);
    return 1;
}
int32_t ISetCameraJumpTo(VMCore&, const int32_t* p) {
    if (DVObjectScript* o = Point(p[0])) Eng()->CameraJumpTo(o->p);
    return 1;
}
int32_t ISetZoomLevel(VMCore&, const int32_t* p) {
    float z = F(p[0]);
    if (z != 2.0f && z != 0.5f && z != 1.0f) {
        SBError(false, "DVScript.cpp", 0x41b, "Wanted zoom level is incorrect");
        return 0;
    }
    Eng()->zoomWanted = z;
    return 1;
}
int32_t IDisplayMap(VMCore&, const int32_t* p) {
    Eng()->mapShown = p[0] != 0;
    return 1;
}
int32_t IPrintConsole(VMCore&, const int32_t* p) {
    SBLog("script console: %d", p[0]);
    return 1;
}
int32_t IIsActorEqual(VMCore&, const int32_t* p) { return p[0] == p[1]; }
int32_t IIsAnyDead(uint32_t flags) {
    for (DVElementActor* n : Eng()->NPCs())
        if ((n->classFlags & 0x1f) == flags && n->sprite->pos.posture == 0xe) return 1;
    return 0;
}
int32_t IIsAnyCivilianDead(VMCore&, const int32_t*) { return IIsAnyDead(7); }
int32_t IIsAnyEnemyDead(VMCore&, const int32_t*) { return IIsAnyDead(0x17); }
// DVScript::GetOverallEnemyAlert (0x08352540): the highest of the living ones (0x08565360)
int32_t IGetOverallAlert(uint32_t flags) {
    static const int32_t k[6] = {0, 1, 2, 1, 1, 1};
    int32_t best = 0;
    for (DVElementActor* n : Eng()->NPCs())
        if ((n->classFlags & 0x1f) == flags && n->sprite->pos.posture != 0xe && n->ai && n->ai->alertStatus < 6)
            best = std::max(best, k[n->ai->alertStatus]);
    return best;
}
int32_t IGetOverallEnemyAlert(VMCore&, const int32_t*) { return IGetOverallAlert(0x17); }
int32_t IGetOverallCivilianAlert(VMCore&, const int32_t*) { return IGetOverallAlert(7); }
int32_t IFreezeAll(VMCore&, const int32_t* p) {
    DVArtificialIntelligence::mbFreeze = p[0] != 0;
    return 1;
}
// DVScript::InflictPain (0x08353cf0): a hit (0x2d / 0x2f) on him
int32_t IInflictPain(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || !IsChar(a)) {
        SBError(false, "DVScript.cpp", 0x1a64, "Script Error: You can only inflict pain to actor");
        return 0;
    }
    a->GotBullet((uint16_t)p[1], nullptr);
    return 1;
}
int32_t IGetNumberOfActorsInEngine(VMCore&, const int32_t*) {
    if (!S()) return 0;
    if (S()->loadingCount >= 0) return S()->loadingCount;
    return (int32_t)S()->marrayElement.size();
}
// DVScript::GetActor (0x0834ab60): the level's elements in their loading order
int32_t IGetActor(VMCore&, const int32_t* p) {
    if (p[0] == -1) return 0;
    if (!DVScript::Element(p[0] + 1)) {
        SBError(false, "DVScript.cpp", 0x52c, "Script Error: Trying to acces an invalid actor ID (%d of %zu).", p[0], S() ? S()->marrayElement.size() : (size_t)0);
        return 0;
    }
    return p[0] + 1;
}
// DVScript::GetLocation (0x0834a8e0): the SCRP hunk's objects
int32_t IGetLocation(VMCore&, const int32_t* p) {
    if (p[0] == -1 || !S()) return 0;
    if ((size_t)(uint16_t)p[0] >= S()->locations.size() || !S()->locations[(uint16_t)p[0]]) {
        SBError(false, "DVScript.cpp", 0x4b3, "Script Error: Invalid location %d", p[0]);
        return 0;
    }
    return DVScript::kLocation + (uint16_t)p[0];
}
int32_t IIsActorAnimation(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 3) == 0;
}
int32_t IIsActorObject(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 3) == 1;
}
int32_t IIsActorCart(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 3) == 2;
}
int32_t IIsActorCharacter(VMCore&, const int32_t* p) { return IsChar(DVScript::Element(p[0])); }
int32_t IIsActorPC(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 0xf) == 0xf;
}
int32_t IIsActorNPC(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 0xf) == 7;
}
int32_t IIsActorEnemy(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 0x1f) == 0x17;
}
int32_t IIsActorCivilian(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 0x1f) == 7;
}
int32_t IIsActorAnimal(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 7) == 3;
}
int32_t IIsNull(VMCore&, const int32_t* p) { return p[0] == 0; }
// DVScript::GetActorPosture (0x08351b00): the posture as the scripts know it
int32_t IGetActorPosture(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || (a->classFlags & 7) != 7) {
        SBError(false, "DVScript.cpp", 0x1522, "Script Error: You can only get the posture of a human.");
        return -1;
    }
    uint32_t q = a->sprite->pos.posture;
    switch (q) {
    case 0: case 1: case 3: case 4: case 5: case 8: case 9: case 10: case 0xb:
        return (int32_t)q;
    case 2:
        if (a->actionState == 6) return 7;
        return a->knockedOut ? 0x11 : 2;
    case 6:
        if (a->life < 1) return 0xf;
        return a->actionState == 6 ? 7 : 6;
    case 7: return 0x10;
    case 0xc: return 0xd;
    case 0xd: return 0xe;
    case 0xe: return 0xf;
    default: return 0x29a;
    }
}
// DVScript::SetActorPosture (0x083521b0), the postures the scripts use
int32_t ISetActorPosture(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || (a->classFlags & 7) != 7) {
        SBError(false, "DVScript.cpp", 0x162c, "Script Error: You can only set the posture of a human.");
        return 0;
    }
    switch (p[1]) {
    case 0:
        a->SetStates(POSTURE_STANDING, 0);
        a->defaultCommand = a->IsNPC() ? CMD_WAIT : CMD_NONE;
        break;
    case 2:
        a->SetStates(POSTURE_LYING, 0);
        a->defaultCommand = CMD_LYING;
        break;
    case 5:  // asleep
        a->SetStates(POSTURE_LYING, 0);
        a->defaultCommand = 0x54;
        break;
    case 7:  // knocked out
        a->SetStates(POSTURE_LYING, 6);
        a->defaultCommand = 0x8b;
        break;
    case 10:
        a->SetStates(POSTURE_GETTING_UP, 0);
        a->SetDefaultWaitAction();
        break;
    case 0x11:  // a body
        a->Stop();
        a->SetStates(POSTURE_LYING, 0);
        a->defaultCommand = CMD_DEAD_KNOCKED;
        break;
    default:
        SBLog("script: posture %d not done yet", p[1]);
        break;
    }
    return 1;
}
int32_t IGetHero(int kind) {
    DVElement* e = HeroOfKind(kind);
    if (!e) SBError(false, "DVScript.cpp", 0x55f, "Script Error: hero %d is not present in this level, you cannot get him", kind);
    return DVScript::Handle(e);
}
int32_t IGetCooper(VMCore&, const int32_t*) { return IGetHero(KIND_COOPER); }
int32_t IGetSam(VMCore&, const int32_t*) { return IGetHero(KIND_SAM); }
int32_t IGetDoc(VMCore&, const int32_t*) { return IGetHero(KIND_DOC); }
int32_t IGetKate(VMCore&, const int32_t*) { return IGetHero(KIND_KATE); }
int32_t IGetSanchez(VMCore&, const int32_t*) { return IGetHero(KIND_SANCHEZ); }
int32_t IGetMia(VMCore&, const int32_t*) { return IGetHero(KIND_MIA); }
// DVScript::GetActorLocation (0x0834ad70): a new point where he stands
int32_t IGetActorLocation(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    if (!e || !e->sprite || (e->classFlags & 1) == 0) {
        SBError(false, "DVScript.cpp", 0x5d3, "Script Error: Trying to get the location of an invalid actor.");
        return 0;
    }
    const DVPositionInterface& pos = e->sprite->pos;
    return S()->AddPoint(pos.posMap, pos.layer, pos.sector);
}
// DVScript::SetActorLocation (0x0834afd0): somewhere else at once; no place = off the map
int32_t ISetActorLocation(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    if (!e) {
        SBError(false, "DVScript.cpp", 0x627, "Script Error: Trying to set the location of an invalid actor.");
        return 0;
    }
    if (p[1] == 0) {
        e->active = false;
        if (auto* a = dynamic_cast<DVElementActor*>(e)) {
            if (a->scriptElement) a->EndScriptElement(false);
            if (S()) S()->ForgetZones(a);
        }
        return 1;
    }
    DVObjectScript* o = Point(p[1]);
    if (!o) {
        SBError(false, "DVScript.cpp", 0x657, "Script Error: Trying to set the actor position using a wrong location type.");
        return 0;
    }
    e->active = true;
    if (auto* a = dynamic_cast<DVElementActor*>(e)) {
        a->Teleport(o->p, o->layer, o->area);
        if (a->InBuilding()) {
            a->active = false;  // in a house he is off the map
            if (S()) S()->ForgetZones(a);
        } else if (S()) {
            S()->CheckZones(a, false);
        }
    } else if (e->sprite) {
        e->sprite->pos.SetPositionMap(o->p);
        e->sprite->pos.layer = o->layer;
        if (o->area) e->sprite->pos.sector = o->area;
        e->sprite->pos.ComputePositionAll();
    }
    return 1;
}
// DVScript::IsInside (0x0834b810) / DVSectorScript::IsReallyInside (0x0835e570)
int32_t IIsInside(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    DVObjectScript* z = Loc(p[1]);
    if (!z) return 0;
    if (z->type != 2) {
        SBError(false, "DVScript.cpp", 0x742, "Script Error: trying to check if a character is inside a location which is not a sector.");
        return 0;
    }
    if (!e || !e->sprite || !e->active) return 0;
    const DVPositionInterface& pos = e->sprite->pos;
    return pos.layer == z->layer && z->zone->polygon.IsInside_p(pos.posMap);
}
int32_t IGetAnyActorInside(VMCore&, const int32_t* p) {
    DVObjectScript* z = Loc(p[0]);
    if (!z || z->type != 2 || z->occupants.empty()) return 0;
    return DVScript::Handle(z->occupants.front());
}
int32_t IDeactivate(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    if (!e) {
        SBError(false, "DVScript.cpp", 0x10c0, "Script Error: You tried to deactivate an illegal actor");
        return 0;
    }
    if ((e->classFlags & 3) == 3) {
        if (auto* a = dynamic_cast<DVElementActor*>(e)) {
            a->playable = false;
            if (Eng() && Eng()->Selected() == a) Eng()->SelectNextPlayable();
        }
    } else {
        e->active = false;
    }
    return 1;
}
int32_t IActivate(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    if (!e) return 0;
    if ((e->classFlags & 3) == 3) {
        if (auto* a = dynamic_cast<DVElementActor*>(e)) {
            a->playable = true;
            if (Eng() && !Eng()->Selected()) Eng()->Select(a);
        }
    } else {
        e->active = true;
    }
    return 1;
}
// the hero's actions the interface offers (+0x1a3)
int32_t ISetActionAvailable(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || (a->classFlags & 0xf) != 0xf || (uint32_t)p[1] >= 6) return 0;
    a->actionAvailable[p[1]] = p[2] != 0;
    return 1;
}
int32_t IIsActionAvailable(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || (a->classFlags & 0xf) != 0xf || (uint32_t)p[1] >= 6) return 0;
    return a->actionAvailable[p[1]];
}
int32_t ISetPersistentProperty(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    gPersistent[((int64_t)(e ? e->kind : 0) << 32) | (uint32_t)p[1]] = p[2];
    return 1;
}
int32_t IGetPersistentProperty(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    auto it = gPersistent.find(((int64_t)(e ? e->kind : 0) << 32) | (uint32_t)p[1]);
    return it == gPersistent.end() ? 0 : it->second;
}
// DVScript::SetAIAlertStatus (0x08350460)
int32_t ISetAIAlertStatus(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || !a->ai) return 0;
    static const uint32_t k[5] = {0, 1, 2, 3, 5};
    if ((uint32_t)p[1] < 5) a->ai->SetAlertStatus(k[p[1]], false);
    else if (p[1] == 100) {
        a->ai->SetAlertStatus(0, false);
        a->ai->suspect = 0;
    } else if (p[1] == 0x65) {
        a->ai->SetAlertStatus(1, false);
        a->ai->yellow = 0;
    }
    return 1;
}
int32_t IGetAIAlertStatus(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || !a->ai) return 0;
    static const int32_t k[6] = {0, 1, 2, 3, 3, 4};
    return a->ai->alertStatus < 6 ? k[a->ai->alertStatus] : 0;
}
// DVScript::SetAIState (0x08350810)
int32_t ISetAIState(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || !a->ai) {
        SBError(false, "DVScript.cpp", 0x122b, "Script Error: Trying to set AI state of a non-NPC.");
        return 0;
    }
    switch (p[1]) {
    case 0: case 1: case 2: case 4: case 8:
        a->ai->SetAIState((uint32_t)p[1], false);
        return 1;
    case 3: case 6: case 0xb:
        if ((a->classFlags & 0x1f) != 0x17) return 0;
        a->ai->SetAIState((uint32_t)p[1], false);
        return 1;
    case 0xc:
        a->ai->SetState(2, 0x95);
        return 1;
    default:
        SBError(false, "DVScript.cpp", 0x127e, "Script Error: Illegal AI substate %d", p[1]);
        return 0;
    }
}
int32_t IGetAIState(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || !a->ai) return 0;
    return a->ai->state > 0xb ? 0 : (int32_t)a->ai->state;
}
int32_t ISetAIAttitude(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || !a->ai || (uint32_t)p[1] > 4) return 0;
    a->ai->attitude = (uint32_t)p[1];
    return 1;
}
int32_t IGetAIAttitude(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    return a && a->ai ? (int32_t)a->ai->attitude : 0;
}
int32_t ISetAILevel(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || !a->ai) return 0;
    if (p[1] == 6)
        for (uint8_t& d : a->ai->detections) d = 0;
    return 1;
}
int32_t ISetViewCone(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (a && a->ai) a->SetViewCone((uint32_t)p[1]);
    return 0;
}
int32_t IStareActor(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    DVElement* t = DVScript::Element(p[1]);
    if (!a || !a->ai || !t) return 0;
    a->SetViewTarget(t, p[2] != 0);
    return 1;
}
int32_t IStareLocation(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    DVObjectScript* o = Point(p[1]);
    if (!a || !a->ai || !o) return 0;
    a->SetViewTarget(o->p, p[2] != 0);
    return 1;
}
int32_t IAssignPath(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (a && a->ai) a->ai->AssignNewPatrolPath((uint16_t)p[1]);
    return 0;
}
int32_t IAssignPost(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    DVObjectScript* o = Point(p[1]);
    if (!a || !a->ai || !o) return 0;
    DVposition pos;
    pos.p = o->p;
    pos.layer = (int16_t)o->layer;
    pos.sector = o->area;
    a->ai->AssignNewPost(pos, (uint8_t)p[2]);
    return 0;
}
// DVScript::LockAI (0x08352a90): the NPC's brain waits for the story (the stimuli are dropped,
// or kept for later when asked)
int32_t ILockAI(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a) {
        SBError(false, "DVScript.cpp", 0x1796, "Script Error: You tried to lock the AI of an illegal actor");
        return 0;
    }
    if (a->ai) a->ai->ScriptLockAI(p[1] != 0);
    else if ((a->classFlags & 7) == 3 && !a->IsHero()) a->animalLocked = true;
    else SBError(false, "DVScript.cpp", 0x17a4, "Script Error: You tried to lock the AI of a PC.");
    return 0;
}
int32_t IUnlockAI(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a) return 0;
    if (a->ai) a->ai->ScriptUnlockAI();
    else a->animalLocked = false;
    return 0;
}
int32_t IForceBattleDecision(VMCore&, const int32_t*) { return 0; }
// DVScript::MakeNoise (0x0834a470): a noise at a place (0x08565300)
int32_t IMakeNoise(VMCore&, const int32_t* p) {
    static const uint32_t k[15] = {0, 1, 2, 3, 4, 5, 8, 9, 10, 13, 14, 15, 16, 17, 18};
    DVObjectScript* o = Point(p[0]);
    if (!o || (uint32_t)p[1] >= 15) return 0;
    Eng()->Noise(k[p[1]], SBGeoPoint3D(o->p.x, o->p.y, 0.0f), nullptr);
    return 0;
}
int32_t IIsAnimationActive(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 3) == 0 && e->active;
}
// DVScript::SetAnimationState (0x08349d40): an animated scenery element on or off
int32_t ISetAnimationState(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    if (!e || (e->classFlags & 3) != 0) {
        SBError(false, "DVScript.cpp", 0x2b2, "Script Error: You can't do a SetAnimationState on a non FX element.");
        return 0;
    }
    e->active = p[1] != 0;
    return 1;
}
int32_t IResetAnim(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    if (!e || !e->sprite) return 0;
    if ((e->classFlags & 3) == 0) e->sprite->ResetSpriteFrame(false);
    return 1;
}
int32_t IIsPatchApplied(VMCore&, const int32_t* p) { return Eng()->patches.count(p[0]) != 0; }
int32_t IApplyPatch(VMCore&, const int32_t* p) {
    Eng()->patches.insert(p[0]);
    return 1;
}
int32_t IResetPatch(VMCore&, const int32_t* p) {
    Eng()->patches.erase(p[0]);
    return 1;
}
int32_t IStartSequenceRecording(VMCore&, const int32_t*) { return S()->StartSequenceRecording(); }
int32_t IEndSequenceRecording(VMCore&, const int32_t*) { return S()->EndSequenceRecording(); }
int32_t IThen(VMCore&, const int32_t*) {
    if (!S()->muwSequenceLevel) {
        SBError(false, "DVScript.cpp", 0x371, "Trying to increment the sequence level without starting a sequence");
        return 0;
    }
    return ++S()->muwSequenceLevel;
}

// ---- the recorded elements ----------------------------------------------------------------------

int32_t IRecordCameraScrollTo(VMCore&, const int32_t* p) {
    if (!Recording("camera")) return 0;
    DVObjectScript* o = Point(p[0]);
    if (!o) return 0;
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 6, nullptr, o->p, nullptr, 0);
    return 1;
}
int32_t IRecordCameraJumpTo(VMCore&, const int32_t* p) {
    if (!Recording("camera")) return 0;
    DVObjectScript* o = Point(p[0]);
    if (!o) return 0;
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 7, nullptr, o->p, nullptr, 0);
    return 1;
}
int32_t IRecordCameraMoveTo(VMCore&, const int32_t* p) {
    if (!Recording("camera")) return 0;
    DVObjectScript* o = Point(p[0]);
    if (!o) return 0;
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 6, nullptr, o->p, nullptr, (uint16_t)p[1]);
    return 1;
}
int32_t IRecordSetZoom(VMCore&, const int32_t* p) {
    if (!Recording("zoom")) return 0;
    DVSequenceElement* e = Rec();
    e->command = 8;
    e->value = F(p[0]);
    return 1;
}
int32_t IRecordDisplayMap(VMCore&, const int32_t* p) {
    if (!Recording("map")) return 0;
    RecQuick(9, nullptr, 0, p[0] != 0);
    return 1;
}
int32_t IRecordStartBriefing(VMCore&, const int32_t*) {
    if (!Recording("briefing")) return 0;
    RecQuick(0xd, nullptr, 0);
    return 1;
}
int32_t IRecordEndBriefing(VMCore&, const int32_t*) {
    if (!Recording("briefing")) return 0;
    RecQuick(0xe, nullptr, 0);
    return 1;
}
int32_t IRecordActionAvailable(VMCore&, const int32_t* p) {
    if (!Recording("action")) return 0;
    DVSequenceElement* e = RecQuick(0x56, DVScript::Element(p[0]), 0, false, p[2] != 0);
    e->param16 = (uint16_t)p[1];
    return 1;
}
int32_t IRecordCharacterAvailable(VMCore&, const int32_t* p) {
    if (!Recording("character")) return 0;
    RecQuick(0x57, DVScript::Element(p[0]), 0, false, p[1] != 0);
    return 1;
}
int32_t IRecordLockCameraOn(VMCore&, const int32_t* p) {
    if (!Recording("camera")) return 0;
    DVSequenceElement* e = Rec();
    e->AsInteraction(e->level, 0xf, nullptr, DVScript::Element(p[0]), 0);
    return 1;
}
int32_t IRecordClearCameraLock(VMCore&, const int32_t*) {
    if (!Recording("camera")) return 0;
    RecQuick(0x10, nullptr, 0);
    return 1;
}
int32_t IRecordPlayDialog(VMCore&, const int32_t* p) {
    if (!Recording("dialog")) return 0;
    RecQuick(0xc, nullptr, p[0]);
    return 1;
}
// DVScript::RecordMove (0x0834d2e0): the style is the motion
int32_t IRecordMove(VMCore&, const int32_t* p) {
    if (!Recording("move")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    DVObjectScript* o = Point(p[1]);
    if (!o || !IsChar(a)) {
        SBError(false, "DVScript.cpp", 0xbf9, "Script Error: You tried to create a move sequence with an illegal location or actor");
        return 0;
    }
    static const uint32_t k[6] = {3, 5, 3, 0xf0, 0xf1, 0x85};
    if ((uint32_t)p[2] >= 6) return 0;
    if (DVSequenceManager::trace && S()->vm.current && S()->vm.current->cls)
        SBLog("  move of #%d recorded by %s", p[0], S()->vm.current->cls->name.c_str());
    // DVSequence::AppendMoveToSequence: a move (0x17) with the motion's animation
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 0x17, a, o->p, nullptr, 0);
    e->anim = k[p[2]];
    e->layer = o->layer;
    e->goalArea = o->area;
    e->goalLayer = o->layer;
    return 1;
}
int32_t IRecordLeaveGame(VMCore&, const int32_t* p) {
    if (!Recording("leave")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    DVObjectScript* o = Point(p[1]);
    if (!o || !IsChar(a)) return 0;
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 0x1a, a, o->p, nullptr, 0);
    e->anim = p[2] == 1 ? 5 : p[2] == 2 ? 9 : 3;
    e->goalArea = o->area;
    e->goalLayer = o->layer;
    return 1;
}
// DVScript::RecordLeaveGameDirectionnal (0x08355550): out of the map that way
int32_t IRecordLeaveGameDirectionnal(VMCore&, const int32_t* p) {
    if (!Recording("leave")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    if (!IsChar(a) || (uint32_t)p[2] >= 2) return 0;
    SBGeoPoint2D at = a->sprite->pos.posMap + SBSetSector0to15((unsigned)p[1] & 0xf, 1.0f) * 1000.0f;
    at.x = std::max(1.0f, std::min(at.x, Eng()->mapW - 1));
    at.y = std::max(1.0f, std::min(at.y, Eng()->mapH - 1));
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(S()->muwSequenceLevel++, 0x1a, a, at, nullptr, 0);
    e->anim = p[2] ? 5 : 3;
    return 1;
}
// DVScript::RecordEnterGame (0x08353e80): comes in from a place (off the screen) and walks to another
int32_t IRecordEnterGame(VMCore&, const int32_t* p) {
    if (!Recording("enter")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    DVObjectScript* from = Point(p[1]);
    DVObjectScript* to = Point(p[2]);
    if (!IsChar(a) || !from || !to || (uint32_t)p[3] >= 2) return 0;
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 0xe9, a, to->p, nullptr, 0);
    e->goalArea = to->area;
    e->goalLayer = to->layer;
    e->point2 = from->p;
    e->layer = from->layer;
    e->area2 = from->area;
    e->anim = p[3] ? 5 : 3;
    return 1;
}
// DVScript::SwitchProfile (0x0834a050): the actor's other costume
int32_t ISwitchProfile(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    if (!e || !e->sprite || (e->classFlags & 3) != 3) {
        SBError(false, "DVScript.cpp", 0x31c, "Script Error: Trying to switch a profile for a non actor.");
        return 0;
    }
    if (e->sprite->rowsAlt.empty()) return 1;  // (no other costume)
    e->sprite->SwitchAlternateProfile();
    e->sprite->ResetSpriteFrame(false);
    return 1;
}
// the door nearest to a point location, within 300 (DVScript::LockNearestDoorForPCs etc.)
DVDoor* DoorNear(int32_t loc, const char* what) {
    DVObjectScript* o = Point(loc);
    if (!o) {
        SBError(false, "DVScript.cpp", 0x1923, "Script Error: Trying to %s using a wrong location type.", what);
        return nullptr;
    }
    DVDoor* d = Eng()->Grid()->NearestDoor(o->p, 300.0f);
    if (!d)
        SBError(false, "DVScript.cpp", 0x193d, "Script Error: Trying to %s, but no door found in a radius of 300 points around (%u, %u)",
                what, (unsigned)o->p.x, (unsigned)o->p.y);
    return d;
}
// DVScript::RecordMoveIntoBuilding (0x0834d9d0): a move to the inside of the nearest door's house
int32_t IRecordMoveIntoBuilding(VMCore&, const int32_t* p) {
    if (!Recording("building")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    DVObjectScript* o = Point(p[1]);
    if (!o || !IsChar(a)) {
        SBError(false, "DVScript.cpp", 0xc8a, "Script Error: Trying to walk into a building using a wrong location type.");
        return 0;
    }
    DVDoor* d = Eng()->Grid()->NearestDoor(o->p, 300.0f);
    if (!d) {
        SBError(false, "DVScript.cpp", 0xca4,
                "Script Error: Trying to walk into a building, but no door found in a radius of 300 points around (%u, %u)",
                (unsigned)o->p.x, (unsigned)o->p.y);
        return 0;
    }
    static const uint32_t k[6] = {3, 5, 3, 0xf0, 0xf1, 0x85};
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 0x17, a, d->pB, nullptr, 0);
    e->anim = (uint32_t)p[2] < 6 ? k[p[2]] : 3;
    e->layer = d->layerB;
    e->goalArea = d->b;
    e->goalLayer = d->layerB;
    return 1;
}
// DVScript::IsInsideBuilding (0x08355780): in that house (-1: in any)
int32_t IIsInsideBuilding(VMCore&, const int32_t* p) {
    auto* a = dynamic_cast<DVElementActor*>(DVScript::Element(p[0]));
    if (!a) return 0;
    DVSectorBuilding* b = a->Building();
    if (p[1] == -1) return b != nullptr;
    const auto& all = Eng()->Grid()->Buildings();
    return (size_t)p[1] < all.size() && b == all[(size_t)p[1]];
}
int32_t ICleanFromHisBuildingBeforeTeleport(VMCore&, const int32_t* p) {
    auto* a = dynamic_cast<DVElementActor*>(DVScript::Element(p[0]));
    if (!a) {
        SBError(false, "DVScript.cpp", 0x186f, "Script Error: You tried to clean an illegal actor from a building");
        return 0;
    }
    DVSectorBuilding* b = a->Building();
    if (!b) {
        SBError(false, "DVScript.cpp", 0x1877, "Script Error: You tried to clean an actor from a building who was on open air");
        return 0;
    }
    b->Leave(a);
    return 1;
}
int32_t IAddToHisNewBuildingAfterTeleport(VMCore&, const int32_t* p) {
    auto* a = dynamic_cast<DVElementActor*>(DVScript::Element(p[0]));
    if (!a) return 0;
    DVSectorBuilding* b = a->Building();
    if (!b) {
        SBError(false, "DVScript.cpp", 0x18cc, "Script Error: You tried to add an actor to a building who was on open air");
        return 0;
    }
    b->Enter(a);
    a->active = false;
    return 1;
}
int32_t ILockNearestDoorForPCs(VMCore&, const int32_t* p) {
    if (DVDoor* d = DoorNear(p[0], "open a door")) {
        d->pcLocked = p[1] != 0;
        if (!p[1]) d->enabled = true;
    }
    return 0;
}
int32_t ILockNearestDoorForVillains(VMCore&, const int32_t* p) {
    if (DVDoor* d = DoorNear(p[0], "open a door")) {
        d->villainLocked = p[1] != 0;
        if (!p[1]) d->enabled = true;
    }
    return 0;
}
int32_t ILockNearestDoorForCivilians(VMCore&, const int32_t* p) {
    if (DVDoor* d = DoorNear(p[0], "open a door")) {
        d->civilianLocked = p[1] != 0;
        if (!p[1]) d->enabled = true;
    }
    return 0;
}
int32_t ISetNearestDoorDocLockPickable(VMCore&, const int32_t* p) {
    if (DVDoor* d = DoorNear(p[0], "make a door doclockpickable")) d->docPickable = p[1] != 0;
    return 0;
}
// DVScript::GetDoorStateForPC (0x08353b20): 0 open, 1 locked (Doc can pick it), 2 locked
int32_t IGetDoorStateForPC(VMCore&, const int32_t* p) {
    DVDoor* d = DoorNear(p[0], "ask door state for PC");
    if (!d || !d->pcLocked) return 0;
    return d->docPickable ? 1 : 2;
}
// DVScript::SetBuildingActive (0x08357270): the house and its doors on / off
int32_t ISetBuildingActive(VMCore&, const int32_t* p) {
    const auto& all = Eng()->Grid()->Buildings();
    if ((size_t)p[0] >= all.size()) {
        SBError(false, "DVScript.cpp", 0x1fbb, "Building %d does not exist !", p[0]);
        return 0;
    }
    DVSectorBuilding* b = all[(size_t)p[0]];
    b->flag30 = p[1] != 0;
    for (DVDoor* d : b->doors) d->enabled = p[1] != 0;
    return 0;
}
// DVScript::ActivateDoorMouseSector (0x08357c00): the door's picture can (not) be clicked on
int32_t IActivateDoorMouseSector(VMCore&, const int32_t* p) {
    DVObjectScript* o = Point(p[1]);
    if (!o) return 0;
    for (const auto& d : Eng()->Grid()->Doors()) {
        SBGeoVector2D v = d->mid - o->p;
        if (std::max(std::fabs(v.x), std::fabs(v.y)) < 20.0f && d->plane) {
            d->plane->flag30 = p[0] != 0;
            return 0;
        }
    }
    SBError(false, "DVScript.cpp", 0x2160, "Door for activation not found !");
    return 0;
}
int32_t ISetCorpseExistsInBuilding(VMCore&, const int32_t* p) {
    if (auto* a = dynamic_cast<DVElementActor*>(DVScript::Element(p[0])))
        if (DVSectorBuilding* b = a->Building()) b->corpse = true;
    return 0;
}
int32_t IRecordTurnTo(VMCore&, const int32_t* p) {
    if (!Recording("turn")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    DVObjectScript* o = Point(p[1]);
    if (!o || !IsChar(a)) return 0;
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 0x1d, a, o->p, nullptr, 0);
    return 1;
}
int32_t IRecordFireAt(VMCore&, const int32_t* p) {
    if (!Recording("fire")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    DVElement* t = DVScript::Element(p[1]);
    if (!IsChar(a) || !t) return 0;
    DVSequenceElement* e = Rec();
    e->AsInteraction(e->level, 0x32, a, t, 0);
    return 1;
}
int32_t IRecordFireLocation(VMCore&, const int32_t* p) {
    if (!Recording("fire")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    DVObjectScript* o = Point(p[1]);
    if (!IsChar(a) || !o) return 0;
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 0x32, a, o->p, nullptr, 0);
    return 1;
}
int32_t RecordAnim(uint32_t cmd, const int32_t* p) {
    if (!Recording("play anim")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    if (!a || (!IsChar(a) && (a->classFlags & 0xf) != 8)) {
        SBError(false, "DVScript.cpp", 0xa66, "Script Error: trying to do a play anim on a non actor thing.");
        return 0;
    }
    RecQuick(cmd, a, p[1]);
    return 1;
}
int32_t IRecordPlayAnim(VMCore&, const int32_t* p) { return RecordAnim(0x20, p); }
int32_t IRecordPlayAnimLoop(VMCore&, const int32_t* p) { return RecordAnim(0x21, p); }
int32_t IRecordPlayAnimFreeze(VMCore&, const int32_t* p) { return RecordAnim(0x22, p); }
int32_t IRecordLockAI(VMCore&, const int32_t* p) {
    if (!Recording("lock AI")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    if (!a || (a->classFlags & 0xf) != 7) return 0;
    RecQuick(0xa6, a, 0);
    return 1;
}
int32_t IRecordUnlockAI(VMCore&, const int32_t* p) {
    if (!Recording("unlock AI")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    if (!a || (a->classFlags & 0xf) != 7) return 0;
    RecQuick(0xa7, a, 0);
    return 1;
}
int32_t IRecordLockUser(VMCore&, const int32_t*) {
    if (!Recording("lock user")) return 0;
    RecQuick(4, nullptr, 0);
    return 1;
}
int32_t IRecordUnLockUser(VMCore&, const int32_t*) {
    if (!Recording("unlock user")) return 0;
    RecQuick(5, nullptr, 0);
    return 1;
}
int32_t IRecordTimer(VMCore&, const int32_t* p) {
    if (!Recording("timer")) return 0;
    RecQuick(0xb, nullptr, p[0]);
    return 1;
}
// DVScript::RecordSeekActor (0x0834d0b0): goes to someone (where he is now)
int32_t IRecordSeekActor(VMCore&, const int32_t* p) {
    if (!Recording("seek")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    DVElement* t = DVScript::Element(p[1]);
    if (!IsChar(a) || !t || !t->sprite) return 0;
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 0x17, a, t->sprite->pos.posMap, t, 0);
    e->anim = p[2] ? 5 : 3;
    e->seek = true;
    return 1;
}
int32_t IRecordSeekActorCustomEvent(VMCore&, const int32_t* p) {
    if (!Recording("seek")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    DVElement* t = DVScript::Element(p[1]);
    if (!IsChar(a) || !t || !t->sprite) return 0;
    DVSequenceElement* e = Rec();
    e->AsPhysicsOperation(e->level, 0x17, a, t->sprite->pos.posMap, t, 0);
    e->anim = p[2] ? 5 : 3;
    e->seek = true;
    e->eventTo = DVScript::Element(p[4]);
    e->event = p[5];
    return 1;
}
int32_t IRecordSendCustomEvent(VMCore&, const int32_t* p) {
    if (!Recording("event")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    if (!IsChar(a)) return 0;
    if (p[1] < 1000) {
        SBError(false, "DVScript.cpp", 0xfd4, "Script Error: ID for custom event is %u, must be >= 1000.", p[1]);
        return 0;
    }
    RecQuick(0xa8, a, p[1]);
    return 1;
}
// DVScript::RecordAction (0x0834cbb0): the action's command
int32_t IRecordAction(VMCore&, const int32_t* p) {
    if (!Recording("action")) return 0;
    static const uint32_t k[17] = {0x44, 0x9c, 0x98, 0x9b, 0x1d, 0xa2, 0x43, 0x2d, 0xa9,
                                   0x83, 0x2b, 0x80, 0x3a, 0x65, 0x95, 0x38, 0x96};
    DVElement* a = DVScript::Element(p[0]);
    if (!IsChar(a) || (uint32_t)p[1] >= 17) {
        SBError(false, "DVScript.cpp", 0xb1f, "Script Error: illegal ID for RecordAction");
        return 0;
    }
    DVSequenceElement* e = RecQuick(k[p[1]], a, p[2]);
    e->action = true;
    return 1;
}
int32_t IRecordSpeak(VMCore&, const int32_t* p) {
    if (!Recording("speak")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    if (!a || (a->classFlags & 7) != 7) return 0;
    RecQuick(0xab, a, p[1], true, true);
    return 1;
}
int32_t IRecordSpeakPC(VMCore&, const int32_t* p) {
    if (!Recording("speak")) return 0;
    DVElement* a = DVScript::Element(p[0]);
    if (!a || (a->classFlags & 0xf) != 0xf) return 0;
    DVSequenceElement* e = RecQuick(0xab, a, p[1], true, true);
    e->param16 = (uint16_t)p[2];
    return 1;
}
// the recorded things this engine doesn't do yet (horses, corpses, barrels, mobile elements):
// nothing is recorded, the sequence goes on
int32_t IRecordNothing(VMCore&, const int32_t*) {
    static bool told = false;
    if (!told) {
        told = true;
        SBLog("script: a recorded action this engine doesn't do yet (horse / corpse / barrel / cart) was skipped");
    }
    return 1;
}

// ---- globals, This, the rest ----------------------------------------------------------------------

int32_t IInitGlobal(VMCore&, const int32_t* p) {
    gGlobals[p[0]] = p[1];
    return 0;
}
int32_t ISetGlobal(VMCore&, const int32_t* p) {
    gGlobals[p[0]] = p[1];
    return 0;
}
int32_t IGetGlobal(VMCore&, const int32_t* p) {
    auto it = gGlobals.find(p[0]);
    return it == gGlobals.end() ? 0 : it->second;
}
int32_t IThis(VMCore&, const int32_t*) { return DVScript::Handle(DVScript::scriptThis); }
int32_t IGetActorDirection(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && e->sprite ? (int32_t)e->sprite->pos.direction : 0;
}
int32_t ISetActorDirection(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    if (!e || !e->sprite) return 0;
    e->sprite->pos.direction = (uint16_t)(p[1] & 0xf);
    e->sprite->pos.directionWanted = (uint16_t)(p[1] & 0xf);
    return 1;
}
int32_t IStopActor(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a) return 0;
    a->Stop();
    return 1;
}
int32_t IGetDistance(VMCore&, const int32_t* p) {
    DVObjectScript* a = Point(p[0]);
    DVObjectScript* b = Point(p[1]);
    if (!a || !b) return 0;
    SBGeoVector2D d = b->p - a->p;
    return (int32_t)std::sqrt(d.x * d.x + d.y * d.y);
}
int32_t IGetCurrentAction(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    return a ? (int32_t)a->CurrentCommand() : 0;
}
int32_t ISetPathWalkingStyle(VMCore&, const int32_t* p) {
    static const uint16_t k[4] = {0, 1, 2, 0x2000};
    DVElementActor* a = Actor(p[0]);
    if (a && a->ai && (uint32_t)p[1] < 4) a->ai->routeFlags = k[p[1]];
    return 0;
}
// DVScript::Sees (0x08355e50): the NPC sees him
int32_t ISees(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    DVElementActor* b = Actor(p[1]);
    if (!a || !b || !a->ai || !b->sprite || !b->active) return 0;
    if (b->IsHero()) {
        uint16_t speed = 0;
        return a->DetectHero(b, speed) != 0;
    }
    SBGeoPoint3D x, y, z;
    b->DetectionPoints(x, y, z);
    return a->SeesPoint(y, true);
}
int32_t IGetActorIndex(VMCore&, const int32_t* p) { return p[0] - 1; }
int32_t IDisplayHint(VMCore&, const int32_t* p) {
    Eng()->SetHint((uint32_t)p[0] + 1);
    return 0;
}
int32_t IHideHint(VMCore&, const int32_t*) {
    Eng()->SetHint(0);
    return 0;
}
int32_t IPutActorInBarrel(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    if (e) e->active = false;
    return 1;
}
int32_t ISetSkipData(VMCore&, const int32_t* p) {
    Eng()->skipData = p[0];
    return 0;
}
int32_t IRand(VMCore&, const int32_t* p) { return (int32_t)(desprand() & (uint32_t)p[0]); }
int32_t IPrototypeFilterEvent(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (!a || !S()) return 0;
    return S()->FilterEvent(a, DVScript::Element(p[1]), p[2]);
}
int32_t IAddSentence(VMCore&, const int32_t* p) {
    Eng()->AddSentence((uint32_t)p[0]);
    return 0;
}
int32_t IDoneSentence(VMCore&, const int32_t* p) {
    Eng()->DoneSentence((uint32_t)p[0]);
    return 0;
}
int32_t IChooseVictoryDefeatText(VMCore&, const int32_t* p) {
    Eng()->victoryText = p[0];
    return 0;
}
int32_t IEnableViewCone(VMCore&, const int32_t* p) {
    Eng()->watched = Actor(p[0]);
    return 0;
}
int32_t ISelectPC(VMCore&, const int32_t* p) {
    Eng()->StateSelectActor((uint16_t)p[0]);
    return 1;
}
int32_t IGetOutlineDisplay(VMCore&, const int32_t*) { return Eng()->silhouettes; }
int32_t ISetOutlineDisplay(VMCore&, const int32_t* p) {
    Eng()->silhouettes = p[0] != 0;
    return 0;
}
int32_t IForceCheckVictory(VMCore&, const int32_t*) {
    Eng()->forceVictoryCheck = true;
    return 0;
}
int32_t INothing(VMCore&, const int32_t*) { return 0; }
int32_t IOne(VMCore&, const int32_t*) { return 1; }

void Install() {
    VMNative* t = VMCore::mparrayNativeFunctions;
    for (int i = 0; i < 200; ++i) t[i] = nullptr;
    t[0x00] = IStartDialog;
    t[0x01] = ISetCameraScrollTo;
    t[0x02] = ISetCameraScrollSlowlyTo;
    t[0x03] = ISetCameraJumpTo;
    t[0x04] = ISetZoomLevel;
    t[0x05] = IDisplayMap;
    t[0x06] = IPrintConsole;
    t[0x07] = INothing;      // IDisplayConsole
    t[0x08] = IIsActorEqual;
    t[0x09] = IIsAnyCivilianDead;
    t[0x0a] = IIsAnyEnemyDead;
    t[0x0b] = IGetOverallEnemyAlert;
    t[0x0c] = IGetOverallCivilianAlert;
    t[0x0d] = IFreezeAll;
    t[0x0e] = IInflictPain;
    t[0x0f] = IGetNumberOfActorsInEngine;
    t[0x10] = IGetActor;
    t[0x11] = IGetLocation;
    t[0x12] = IIsActorAnimation;
    t[0x13] = IIsActorObject;
    t[0x14] = IIsActorCharacter;
    t[0x15] = IIsActorPC;
    t[0x16] = IIsActorNPC;
    t[0x17] = IIsActorEnemy;
    t[0x18] = IIsActorCivilian;
    t[0x19] = IIsActorAnimal;
    t[0x1a] = IIsNull;
    t[0x1b] = IGetActorPosture;
    t[0x1c] = ISetActorPosture;
    t[0x1d] = IGetCooper;
    t[0x1e] = IGetSam;
    t[0x1f] = IGetDoc;
    t[0x20] = IGetKate;
    t[0x21] = IGetSanchez;
    t[0x22] = IGetMia;
    t[0x23] = IGetActorLocation;
    t[0x24] = ISetActorLocation;
    t[0x25] = IIsInside;
    t[0x26] = IIsInsideBuilding;
    t[0x27] = IGetAnyActorInside;
    t[0x28] = ISwitchProfile;
    t[0x29] = INothing;      // IGetMovementStyle
    t[0x2a] = IDeactivate;
    t[0x2b] = IActivate;
    t[0x2c] = ISetActionAvailable;
    t[0x2d] = IIsActionAvailable;
    t[0x2e] = ISetPersistentProperty;
    t[0x2f] = IGetPersistentProperty;
    t[0x30] = ISetAIAlertStatus;
    t[0x31] = IGetAIAlertStatus;
    t[0x32] = ISetAIState;
    t[0x33] = IGetAIState;
    t[0x34] = ISetAIAttitude;
    t[0x35] = IGetAIAttitude;
    t[0x36] = ISetAILevel;
    t[0x37] = ISetViewCone;
    t[0x38] = IStareActor;
    t[0x39] = IStareLocation;
    t[0x3a] = IAssignPath;
    t[0x3b] = IAssignPost;
    t[0x3c] = ILockAI;
    t[0x3d] = IUnlockAI;
    t[0x3e] = IForceBattleDecision;
    t[0x3f] = IMakeNoise;
    t[0x40] = INothing;      // IFreeze
    t[0x41] = INothing;      // IGetTheRider
    t[0x42] = INothing;      // IGetTheHorse
    t[0x43] = IIsAnimationActive;
    t[0x44] = ISetAnimationState;
    t[0x45] = IIsPatchApplied;
    t[0x46] = IApplyPatch;
    t[0x47] = INothing;      // IIsHorseUsed
    t[0x48] = IStartSequenceRecording;
    t[0x49] = IEndSequenceRecording;
    t[0x4a] = IThen;         // IIncrementSequenceLevel
    t[0x4b] = IStartSequenceRecording;  // IStart
    t[0x4c] = IEndSequenceRecording;    // IThanx
    t[0x4d] = IThen;
    t[0x4e] = IRecordCameraScrollTo;
    t[0x4f] = IRecordCameraJumpTo;
    t[0x50] = IRecordSetZoom;
    t[0x51] = IRecordDisplayMap;
    t[0x52] = IRecordStartBriefing;
    t[0x53] = IRecordEndBriefing;
    t[0x54] = IRecordActionAvailable;
    t[0x55] = IRecordCharacterAvailable;
    t[0x56] = IRecordLockCameraOn;
    t[0x57] = IRecordClearCameraLock;
    t[0x58] = IRecordPlayDialog;
    t[0x59] = IRecordMove;
    t[0x5a] = IRecordLeaveGame;
    t[0x5b] = IRecordTurnTo;
    t[0x5c] = IRecordNothing;   // IRecordMountHorse
    t[0x5d] = IRecordNothing;   // IRecordDismountHorse
    t[0x5e] = IRecordFireAt;
    t[0x5f] = IRecordPlayAnim;
    t[0x60] = IRecordPlayAnimLoop;
    t[0x61] = IRecordPlayAnimFreeze;
    t[0x62] = IRecordLockAI;
    t[0x63] = IRecordUnlockAI;
    t[0x64] = IRecordLockUser;
    t[0x65] = IRecordUnLockUser;
    t[0x66] = IRecordTimer;
    t[0x67] = IRecordSeekActor;
    t[0x68] = IOne;             // IRecordStopSeek
    t[0x69] = IRecordSendCustomEvent;
    t[0x6a] = IRecordAction;
    t[0x6b] = IInitGlobal;
    t[0x6c] = ISetGlobal;
    t[0x6d] = IGetGlobal;
    for (int i = 0x6e; i <= 0x73; ++i) t[i] = INothing;   // sound sources
    t[0x74] = ICleanFromHisBuildingBeforeTeleport;
    t[0x75] = IAddToHisNewBuildingAfterTeleport;
    t[0x76] = INothing;   // ICleanFromScriptZoneBeforeTeleport (the zones follow the actors)
    t[0x77] = INothing;   // IAddToScriptZoneAfterTeleport
    t[0x78] = ILockNearestDoorForPCs;
    t[0x79] = ILockNearestDoorForVillains;
    t[0x7a] = ILockNearestDoorForCivilians;
    t[0x7b] = IThis;
    t[0x7c] = IGetActorDirection;
    t[0x7d] = ISetActorDirection;
    t[0x7e] = IStopActor;
    t[0x7f] = IGetDistance;
    t[0x80] = IGetCurrentAction;
    t[0x81] = IIsActorCart;
    t[0x82] = ISetPathWalkingStyle;
    t[0x83] = IRecordNothing;   // IRecordReplaceAnim
    t[0x84] = IRecordNothing;   // IRecordRestoreAnim
    t[0x85] = ISees;
    t[0x86] = IGetActorIndex;
    for (int i = 0x87; i <= 0x96; ++i) t[i] = INothing;   // armies, snake, watch
    t[0x97] = IRecordNothing;   // IRecordTakeCorpse
    t[0x98] = IRecordMoveIntoBuilding;
    t[0x99] = IRecordCameraMoveTo;
    t[0x9a] = IRecordNothing;   // IRecordEnterBarrel
    t[0x9b] = IRecordNothing;   // IRecordLeaveBarrel
    t[0x9c] = IRecordEnterGame;
    t[0x9d] = IRecordNothing;   // IRecordLeaveCorpse
    t[0x9e] = INothing;
    t[0x9f] = INothing;         // ICallKnife
    t[0xa0] = IResetAnim;
    t[0xa1] = IRecordNothing;   // IRecordJumpOnHorse
    t[0xa2] = INothing;
    t[0xa3] = INothing;
    t[0xa4] = IRecordNothing;   // IRecordTieCorpse
    t[0xa5] = INothing;
    t[0xa6] = ISetBuildingActive;
    t[0xa7] = IRecordNothing;   // IRecordStartMobileElement
    t[0xa8] = IRecordNothing;   // IRecordStopMobileElement
    t[0xa9] = ISetNearestDoorDocLockPickable;
    t[0xaa] = IDisplayHint;
    t[0xab] = IHideHint;
    t[0xac] = IPutActorInBarrel;
    t[0xad] = ISetSkipData;
    t[0xae] = INothing;
    t[0xaf] = IRecordSpeak;
    t[0xb0] = IRecordFireLocation;
    t[0xb1] = IRand;
    t[0xb2] = IResetPatch;
    t[0xb3] = IPrototypeFilterEvent;
    t[0xb4] = IAddSentence;
    t[0xb5] = IDoneSentence;
    t[0xb6] = IChooseVictoryDefeatText;
    t[0xb7] = IEnableViewCone;
    t[0xb8] = INothing;
    t[0xb9] = INothing;
    t[0xba] = IRecordSpeakPC;
    t[0xbb] = ISelectPC;
    t[0xbc] = INothing;
    t[0xbd] = IRecordSeekActorCustomEvent;
    t[0xbe] = IActivateDoorMouseSector;
    t[0xbf] = IGetOutlineDisplay;
    t[0xc0] = ISetOutlineDisplay;
    t[0xc1] = INothing;
    t[0xc2] = IGetDoorStateForPC;
    t[0xc3] = IRecordLeaveGameDirectionnal;
    t[0xc4] = ISetCorpseExistsInBuilding;
    t[0xc5] = IForceCheckVictory;
    t[0xc6] = INothing;
    t[0xc7] = INothing;
}
}  // namespace

// ---- DVScript --------------------------------------------------------------------------------------

DVScript::DVScript() {
    mpInstance = this;
    Install();
    gGlobals.clear();
}

DVScript::~DVScript() {
    if (mpSequence) delete mpSequence;
    if (mpInstance == this) mpInstance = nullptr;
    scriptThis = nullptr;
}

// DVScript::StartSequenceRecording (0x0834a170)
bool DVScript::StartSequenceRecording() {
    if (mpSequence) delete mpSequence;  // a sequence never ended
    mpSequence = new DVSequence();
    muwSequenceLevel = 1;
    return true;
}

// DVScript::EndSequenceRecording (0x0834a260): the sequence is launched
bool DVScript::EndSequenceRecording() {
    if (!muwSequenceLevel) {
        SBError(false, "DVScript.cpp", 0x349, "Trying to end a sequence that was never started");
        return false;
    }
    DVSequence* s = mpSequence;
    mpSequence = nullptr;
    muwSequenceLevel = 0;
    if (s->elements.empty()) {
        SBError(false, "DVScript.cpp", 0x359, "Ending a sequence with no element");
        delete s;
        return true;
    }
    if (DVSequenceManager::mpInstance) DVSequenceManager::mpInstance->LaunchSequence(s);
    else delete s;
    return true;
}

// DVFastFindGrid::LoadScriptObjectsFromFile (0x082c2f90), DVPointScript::LoadFromFile
// (0x082f4d50), DVLineScript::LoadFromFile (0x082f0d30), DVSectorScript::LoadFromFile (0x0835dc10)
int DVScript::LoadScriptObjectsFromFile(SBFile& f) {
    f.version = f.U32();
    if (f.version != 1)
        SBError(true, "DVFastFindGrid.cpp", 0x21a0, "Version Check Failed on %s (file version %u, expected version %u).",
                "Hunk Script", f.version, 1);
    uint16_t n = f.U16();
    int size = 6;
    DVFastFindGrid* grid = DVEngine::mpEngine ? DVEngine::mpEngine->Grid() : nullptr;
    locations.clear();
    for (uint16_t i = 0; i < n; ++i) {
        auto o = std::make_unique<DVObjectScript>();
        uint16_t type = f.U16();
        size += 2;
        if (type == 1) {
            o->type = 0;
            int16_t x = f.S16(), y = f.S16();
            o->p = SBGeoPoint2D((float)x, (float)y);
            o->layer = f.U16();
            uint16_t area = f.U16();
            o->area = grid ? grid->Sector(area) : nullptr;
            o->u14 = f.U8();
            size += 9;
        } else if (type == 2) {
            o->type = 1;
            int16_t x0 = f.S16(), y0 = f.S16(), x1 = f.S16(), y1 = f.S16();
            o->p = SBGeoPoint2D((float)x0, (float)y0);
            o->p2 = SBGeoPoint2D((float)x1, (float)y1);
            o->layer = f.U16();
            f.U16();
            o->u14 = f.U8();
            size += 13;
        } else {
            f.Skip(-2, 1);
            size -= 2;
            o->type = 2;
            o->zone.reset(new DVSector(0x80));
            size += o->zone->LoadFromFile(f);
            o->layer = f.U16();
            o->areaId = f.U16();
            o->hasScript = f.U8() != 0;
            size += 5;
            if (o->hasScript) {
                uint16_t len = f.U16();
                std::string name(len, '\0');
                if (len) f.Serialize(&name[0], len);
                o->scriptName = name.c_str();
                size += 2 + len;
            }
        }
        locations.push_back(std::move(o));
    }
    return size;
}

VMInstance* DVScript::InstanceOf(const DVElement* e) const {
    auto it = instances.find(e);
    return it == instances.end() ? nullptr : it->second.get();
}

int32_t DVScript::CallFor(VMInstance* inst, DVElement* self, const char* fn, const std::vector<int32_t>& params) {
    if (!inst || !loaded) return 0;
    DVElement* saved = scriptThis;
    scriptThis = self;
    int32_t r = vm.Call(*inst, fn, params);
    scriptThis = saved;
    return r;
}

bool DVScript::Initialize(DVEngine* engine, const std::vector<DVElement*>& elements, const std::string& levelName) {
    (void)engine;
    marrayElement = elements;
    std::string path = "Data\\Levels\\" + levelName + ".scb";
    if (!code.Load(path)) {
        SBLog("no scripts for %s", levelName.c_str());
        return false;
    }
    loaded = true;
    SBLog("%s: %zu script classes, %zu locations", path.c_str(), code.Classes().size(), locations.size());
    // the elements' instances (their script's class is their name in the level), in loading order
    // (IActorNPCScript::Initialize as each is bound)
    for (DVElement* e : marrayElement) {
        if (!e || e->scriptName.empty()) continue;
        const VMClass* c = code.SearchClass(e->scriptName);
        if (!c) continue;
        auto inst = std::make_unique<VMInstance>();
        inst->cls = c;
        inst->owner = e;
        VMInstance* raw = inst.get();
        instances[e] = std::move(inst);
        if (auto* a = dynamic_cast<DVElementActor*>(e))
            if (a->ai) a->ai->scripted = true;
        loadingCount = Handle(e) - 1;
        CallFor(raw, e, "Initialize", {0, 0});
        loadingCount = -1;
    }
    // the zones (IZoneScript::Initialize, while the SCRP hunk loads)
    for (auto& o : locations) {
        if (!o || o->type != 2 || !o->hasScript) continue;
        const VMClass* c = code.SearchClass(o->scriptName);
        if (!c) {
            SBError(false, "DVSectorScript.cpp", 0xb10, "Structural error in DVD, a Sector has got a script reference that does not exist !");
            o->hasScript = false;
            continue;
        }
        o->inst = std::make_unique<VMInstance>();
        o->inst->cls = c;
        CallFor(o->inst.get(), nullptr, "Initialize", {0, 0});
    }
    // the mission (IEngineScript::Initialize, IEngineScript::Briefing)
    if (const VMClass* s = code.SearchClass("StartUp")) {
        startUp = std::make_unique<VMInstance>();
        startUp->cls = s;
        CallFor(startUp.get(), nullptr, "Initialize", {0, 0});
        CallFor(startUp.get(), nullptr, "Briefing", {});
    }
    // DVFastFindGrid::InitializeScriptSectorOccupants: who is where, without the events
    if (DVEngine::mpEngine)
        for (DVElementActor* a : DVEngine::mpEngine->Characters()) CheckZones(a, true);
    return true;
}

void DVScript::HourGlass(uint32_t seconds) {
    if (startUp) CallFor(startUp.get(), nullptr, "HourGlass", {(int32_t)seconds});
}

int DVScript::CheckVictoryCondition(uint32_t seconds) {
    if (!startUp) return 0;
    return CallFor(startUp.get(), nullptr, "CheckVictoryCondition", {(int32_t)seconds});
}

bool DVScript::FilterEvent(DVElementActor* npc, DVElement* source, int event) {
    VMInstance* inst = InstanceOf(npc);
    if (!inst || !inst->cls->Function("FilterEvent")) return true;
    return CallFor(inst, npc, "FilterEvent", {Handle(source), event}) != 0;
}

void DVScript::ReachPoint(DVWaypoint* w, DVElementActor* npc) {
    if (!w || w->script.empty() || !loaded) return;
    std::string name = w->script.c_str();
    auto it = waypointInstances.find(name);
    if (it == waypointInstances.end()) {
        const VMClass* c = code.SearchClass(name);
        if (!c) {
            SBLog("script: no class %s for a waypoint", name.c_str());
            waypointInstances[name] = nullptr;
            return;
        }
        auto inst = std::make_unique<VMInstance>();
        inst->cls = c;
        CallFor(inst.get(), npc, "Initialize", {0, 0});
        it = waypointInstances.emplace(name, std::move(inst)).first;
    }
    if (it->second) CallFor(it->second.get(), npc, "ReachPoint", {Handle(npc)});
}

// DVElementActor::CheckForLineCrossing (0x0820b700) with DVSectorScript::Enter / Leave: the zones
// he came into or left since the last step
void DVScript::CheckZones(DVElementActor* a, bool silent) {
    if (!a || !a->sprite) return;
    const DVPositionInterface& pos = a->sprite->pos;
    for (auto& o : locations) {
        if (!o || o->type != 2) continue;
        bool in = a->active && pos.layer == o->layer && o->zone->box.IsInside_p(pos.posMap) &&
                  o->zone->polygon.IsInside_p(pos.posMap);
        auto it = std::find(o->occupants.begin(), o->occupants.end(), a);
        bool was = it != o->occupants.end();
        if (in == was) continue;
        if (in) o->occupants.push_back(a);
        else o->occupants.erase(it);
        if (silent || !o->hasScript || !o->inst) continue;
        CallFor(o->inst.get(), a, in ? "EnterZone" : "ExitZone", {Handle(a)});
    }
}

void DVScript::ForgetZones(DVElementActor* a) {
    for (auto& o : locations) {
        if (!o || o->type != 2) continue;
        auto it = std::find(o->occupants.begin(), o->occupants.end(), a);
        if (it == o->occupants.end()) continue;
        o->occupants.erase(it);
        if (o->hasScript && o->inst) CallFor(o->inst.get(), a, "ExitZone", {Handle(a)});
    }
}

void DVScript::TargetEvent(DVElement* e, const char* function, DVElement* by) {
    VMInstance* inst = InstanceOf(e);
    if (inst) CallFor(inst, by, function, {Handle(by)});
}
