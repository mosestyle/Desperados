#include "dv/DVScript.h"

#include <map>

#include "dv/DVArtificialIntelligence.h"
#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVSprite.h"
#include "sb/SBFile.h"

DVScript* DVScript::mpInstance = nullptr;

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

static DVElementActor* Actor(int32_t h) { return dynamic_cast<DVElementActor*>(DVScript::Element(h)); }

static DVElement* HeroOfKind(int kind) {
    if (!DVScript::mpInstance) return nullptr;
    for (DVElement* e : DVScript::mpInstance->marrayElement)
        if (e && e->kind == kind) return e;
    return nullptr;
}

// ---- the natives (VMCoreCustom::I..., DVScript::...) -------------------------------------------

namespace {
std::map<int32_t, int32_t> gGlobals;  // DVScript::InitGlobal / SetGlobal / GetGlobal

int32_t IIsActorEqual(VMCore&, const int32_t* p) { return p[0] == p[1]; }
int32_t IGetNumberOfActorsInEngine(VMCore&, const int32_t*) {
    return DVScript::mpInstance ? (int32_t)DVScript::mpInstance->marrayElement.size() : 0;
}
// DVScript::GetActor (0x0834ab60): the level's elements in their loading order
int32_t IGetActor(VMCore&, const int32_t* p) {
    if (p[0] == -1) return 0;
    if (!DVScript::Element(p[0] + 1)) {
        SBError(false, "DVScript.cpp", 0x52c, "Script Error: Trying to acces an invalid actor ID.");
        return 0;
    }
    return p[0] + 1;
}
int32_t IIsNull(VMCore&, const int32_t* p) { return p[0] == 0; }
int32_t IIsActorObject(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 3) == 1;
}
int32_t IIsActorCharacter(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && (e->classFlags & 3) == 3;
}
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
int32_t IGetActorPosture(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    return e && e->sprite ? (int32_t)e->sprite->pos.posture : 0;
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
// DVScript::Deactivate (0x0834fe50): a character is no more playable (+0xb5), anything else is
// taken off the map (+0x10)
int32_t IDeactivate(VMCore&, const int32_t* p) {
    DVElement* e = DVScript::Element(p[0]);
    if (!e) {
        SBError(false, "DVScript.cpp", 0x10c0, "Script Error: You tried to deactivate an illegal actor");
        return 0;
    }
    if ((e->classFlags & 3) == 3) {
        if (auto* a = dynamic_cast<DVElementActor*>(e)) {
            a->playable = false;
            if (DVEngine::mpEngine && DVEngine::mpEngine->Selected() == a) DVEngine::mpEngine->SelectNextPlayable();
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
        if (auto* a = dynamic_cast<DVElementActor*>(e)) a->playable = true;
    } else {
        e->active = true;
    }
    return 1;
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
int32_t ISetViewCone(VMCore&, const int32_t* p) {
    DVElementActor* a = Actor(p[0]);
    if (a && a->ai) a->SetViewCone((uint32_t)p[1]);
    return 0;
}
int32_t IThis(VMCore& vm, const int32_t*) {
    return vm.current ? DVScript::Handle(static_cast<DVElement*>(vm.current->owner)) : 0;
}
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
int32_t INothing(VMCore&, const int32_t*) { return 0; }

void Install() {
    VMNative* t = VMCore::mparrayNativeFunctions;
    t[0x08] = IIsActorEqual;
    t[0x0f] = IGetNumberOfActorsInEngine;
    t[0x10] = IGetActor;
    t[0x13] = IIsActorObject;
    t[0x14] = IIsActorCharacter;
    t[0x15] = IIsActorPC;
    t[0x16] = IIsActorNPC;
    t[0x17] = IIsActorEnemy;
    t[0x18] = IIsActorCivilian;
    t[0x19] = IIsActorAnimal;
    t[0x1a] = IIsNull;
    t[0x1b] = IGetActorPosture;
    t[0x1d] = IGetCooper;
    t[0x1e] = IGetSam;
    t[0x1f] = IGetDoc;
    t[0x20] = IGetKate;
    t[0x21] = IGetSanchez;
    t[0x22] = IGetMia;
    t[0x2a] = IDeactivate;
    t[0x2b] = IActivate;
    t[0x37] = ISetViewCone;
    t[0x3c] = ILockAI;
    t[0x3d] = IUnlockAI;
    t[0x6b] = IInitGlobal;
    t[0x6c] = ISetGlobal;
    t[0x6d] = IGetGlobal;
    t[0x7b] = IThis;
    t[0xad] = INothing;   // ISetSkipData: what a skipped intro jumps to (the intros come later)
}
}  // namespace

DVScript::DVScript() {
    mpInstance = this;
    Install();
    gGlobals.clear();
}

DVScript::~DVScript() {
    if (mpInstance == this) mpInstance = nullptr;
}

bool DVScript::Initialize(DVEngine* engine, const std::vector<DVElement*>& elements, const std::string& levelName) {
    (void)engine;
    marrayElement = elements;
    std::string path = "Data\\Levels\\" + levelName + ".scb";
    if (!code.Load(path)) {
        SBLog("no scripts for %s", levelName.c_str());
        return false;
    }
    SBLog("%s: %zu script classes", path.c_str(), code.Classes().size());
    // the elements' instances (their script's class is their name in the level)
    for (DVElement* e : marrayElement) {
        if (!e || e->scriptName.empty()) continue;
        const VMClass* c = code.SearchClass(e->scriptName);
        if (!c) continue;
        auto inst = std::make_unique<VMInstance>();
        inst->cls = c;
        inst->owner = e;
        instances.push_back(std::move(inst));
    }
    if (const VMClass* s = code.SearchClass("StartUp")) {
        startUp = std::make_unique<VMInstance>();
        startUp->cls = s;
        vm.Call(*startUp, "Initialize", {0, 0});
    }
    for (auto& i : instances) vm.Call(*i, "Initialize", {0, 0});
    return true;
}
