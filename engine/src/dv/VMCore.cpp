#include "dv/VMCore.h"

#include <cstdio>
#include <cstring>

#include "sb/SBFile.h"

std::vector<uint8_t> VMCore::globals;

const char* const VMCore::kNativeNames[200] = {
    "IStartDialog",
    "ISetCameraScrollTo",
    "ISetCameraScrollSlowlyTo",
    "ISetCameraJumpTo",
    "ISetZoomLevel",
    "IDisplayMap",
    "IPrintConsole",
    "IDisplayConsole",
    "IIsActorEqual",
    "IIsAnyCivilianDead",
    "IIsAnyEnemyDead",
    "IGetOverallEnemyAlert",
    "IGetOverallCivilianAlert",
    "IFreezeAll",
    "IInflictPain",
    "IGetNumberOfActorsInEngine",
    "IGetActor",
    "IGetLocation",
    "IIsActorAnimation",
    "IIsActorObject",
    "IIsActorCharacter",
    "IIsActorPC",
    "IIsActorNPC",
    "IIsActorEnemy",
    "IIsActorCivilian",
    "IIsActorAnimal",
    "IIsNull",
    "IGetActorPosture",
    "ISetActorPosture",
    "IGetCooper",
    "IGetSam",
    "IGetDoc",
    "IGetKate",
    "IGetSanchez",
    "IGetMia",
    "IGetActorLocation",
    "ISetActorLocation",
    "IIsInside",
    "IIsInsideBuilding",
    "IGetAnyActorInside",
    "ISwitchProfile",
    "IGetMovementStyle",
    "IDeactivate",
    "IActivate",
    "ISetActionAvailable",
    "IIsActionAvailable",
    "ISetPersistentProperty",
    "IGetPersistentProperty",
    "ISetAIAlertStatus",
    "IGetAIAlertStatus",
    "ISetAIState",
    "IGetAIState",
    "ISetAIAttitude",
    "IGetAIAttitude",
    "ISetAILevel",
    "ISetViewCone",
    "IStareActor",
    "IStareLocation",
    "IAssignPath",
    "IAssignPost",
    "ILockAI",
    "IUnlockAI",
    "IForceBattleDecision",
    "IMakeNoise",
    "IFreeze",
    "IGetTheRider",
    "IGetTheHorse",
    "IIsAnimationActive",
    "ISetAnimationState",
    "IIsPatchApplied",
    "IApplyPatch",
    "IIsHorseUsed",
    "IStartSequenceRecording",
    "IEndSequenceRecording",
    "IIncrementSequenceLevel",
    "IStart",
    "IThanx",
    "IThen",
    "IRecordCameraScrollTo",
    "IRecordCameraJumpTo",
    "IRecordSetZoom",
    "IRecordDisplayMap",
    "IRecordStartBriefing",
    "IRecordEndBriefing",
    "IRecordActionAvailable",
    "IRecordCharacterAvailable",
    "IRecordLockCameraOn",
    "IRecordClearCameraLock",
    "IRecordPlayDialog",
    "IRecordMove",
    "IRecordLeaveGame",
    "IRecordTurnTo",
    "IRecordMountHorse",
    "IRecordDismountHorse",
    "IRecordFireAt",
    "IRecordPlayAnim",
    "IRecordPlayAnimLoop",
    "IRecordPlayAnimFreeze",
    "IRecordLockAI",
    "IRecordUnlockAI",
    "IRecordLockUser",
    "IRecordUnLockUser",
    "IRecordTimer",
    "IRecordSeekActor",
    "IRecordStopSeek",
    "IRecordSendCustomEvent",
    "IRecordAction",
    "IInitGlobal",
    "ISetGlobal",
    "IGetGlobal",
    "ISuspendAllSoundSources",
    "IResumeAllSoundSources",
    "IGetSoundSource",
    "IActivateSoundSource",
    "IDeactivateSoundSource",
    "IDestroySoundSource",
    "ICleanFromHisBuildingBeforeTeleport",
    "IAddToHisNewBuildingAfterTeleport",
    "ICleanFromScriptZoneBeforeTeleport",
    "IAddToScriptZoneAfterTeleport",
    "ILockNearestDoorForPCs",
    "ILockNearestDoorForVillains",
    "ILockNearestDoorForCivilians",
    "IThis",
    "IGetActorDirection",
    "ISetActorDirection",
    "IStopActor",
    "IGetDistance",
    "IGetCurrentAction",
    "IIsActorCart",
    "ISetPathWalkingStyle",
    "IRecordReplaceAnim",
    "IRecordRestoreAnim",
    "ISees",
    "IGetActorIndex",
    "IJoinArmy",
    "IDeclareWar",
    "IGetArmy",
    "ISetSoldierSeesSoldier",
    "ISetSoldierSeesArmy",
    "ISetArmySeesSoldier",
    "ISetArmySeesArmy",
    "IGetNumberOfVisibleSoldiers",
    "IGetVisibleSoldier",
    "IGetNearestVisibleSoldier",
    "ICreateListOfRecentlySeenSoldiers",
    "IGetNumberOfRecentlySeenSoldiers",
    "IGetRecentlySeenSoldier",
    "IGetNearestRecentlySeenSoldier",
    "IPutSnakeOnMap",
    "IPutWatchOnMap",
    "IRecordTakeCorpse",
    "IRecordMoveIntoBuilding",
    "IRecordCameraMoveTo",
    "IRecordEnterBarrel",
    "IRecordLeaveBarrel",
    "IRecordEnterGame",
    "IRecordLeaveCorpse",
    "ISetUltimateWillOnHorse",
    "ICallKnife",
    "IResetAnim",
    "IRecordJumpOnHorse",
    "IEnableHorseUseFor",
    "IDefineAnimalAlert",
    "IRecordTieCorpse",
    "IPutActorInBulding",
    "ISetBuildingActive",
    "IRecordStartMobileElement",
    "IRecordStopMobileElement",
    "ISetNearestDoorDocLockPickable",
    "IDisplayHint",
    "IHideHint",
    "IPutActorInBarrel",
    "ISetSkipData",
    "IForbidHorseForAI",
    "IRecordSpeak",
    "IRecordFireLocation",
    "IRand",
    "IResetPatch",
    "IPrototypeFilterEvent",
    "IAddSentence",
    "IDoneSentence",
    "IChooseVictoryDefeatText",
    "IEnableViewCone",
    "ISetPCAsUnwantedFor",
    "ISpecialAutorisation",
    "IRecordSpeakPC",
    "ISelectPC",
    "ISetBuildingAsTrainWaggon",
    "IRecordSeekActorCustomEvent",
    "IActivateDoorMouseSector",
    "IGetOutlineDisplay",
    "ISetOutlineDisplay",
    "ICustomizeMinimapDisplay",
    "IGetDoorStateForPC",
    "IRecordLeaveGameDirectionnal",
    "ISetCorpseExistsInBuilding",
    "IForceCheckVictory",
    "IForceEmergencyBoxForMotionArea",
    "ISetUserInputPossibleForHorse"
};

// a native without its translation yet: logged once, gives 0
static int32_t NotYet(VMCore& vm, const int32_t*);
VMNative VMCore::mparrayNativeFunctions[200];

static bool sNamesInit = [] {
    for (auto& f : VMCore::mparrayNativeFunctions) f = nullptr;
    return true;
}();

const VMFunction* VMClass::Function(const std::string& n) const {
    for (const VMFunction& f : functions)
        if (f.name == n) return &f;
    return nullptr;
}

const VMClass* VMCodeManager::SearchClass(const std::string& name) const {
    for (const auto& c : classes)
        if (c->name == name) return c.get();
    return nullptr;
}

// the .scb file: text lines, each class's quads (10 bytes each) right after "nbOfQuads N"
bool VMCodeManager::Load(const std::string& path) {
    classes.clear();
    std::vector<uint8_t> d;
    if (!SBFile::ReadAll(path, d)) return false;
    size_t p = 0;
    auto line = [&](std::string& out) -> bool {
        if (p >= d.size()) return false;
        size_t e = p;
        while (e < d.size() && d[e] != '\n') ++e;
        out.assign((const char*)&d[p], e - p);
        if (!out.empty() && out.back() == '\r') out.pop_back();
        p = e < d.size() ? e + 1 : e;
        return true;
    };
    std::string l;
    float version = 0;
    int debug = 0, nClasses = 0;
    if (!line(l) || sscanf(l.c_str(), "version %f, debug %d", &version, &debug) != 2) return false;
    if (!line(l) || sscanf(l.c_str(), "nbOfClasses %d", &nClasses) != 1) return false;
    for (int c = 0; c < nClasses; ++c) {
        auto k = std::make_unique<VMClass>();
        char a[1024] = {}, b[1024] = {};
        if (!line(l) || sscanf(l.c_str(), "fileName %1023s , className %1023s", a, b) != 2) return false;
        k->fileName = a;
        k->name = b;
        if (!line(l) || sscanf(l.c_str(), "nbOfVariables %d, sizeOfVariables %d", &k->nbOfVariables, &k->sizeOfVariables) != 2)
            return false;
        int nf = 0;
        if (!line(l) || sscanf(l.c_str(), "nbOfFunctions %d", &nf) != 1) return false;
        for (int i = 0; i < nf; ++i) {
            VMFunction f;
            char fn[1024] = {};
            if (!line(l) || sscanf(l.c_str(), "functionName %1023s , address %u, nbOfParams %d, sizeOfRetVal %d, sizeOfParams %d", fn,
                                   &f.address, &f.params, &f.sizeOfRetVal, &f.sizeOfParams) != 5)
                return false;
            f.name = fn;
            // "functionParameters", its parameters, an empty line, then the sizes
            do {
                if (!line(l)) return false;
            } while (sscanf(l.c_str(), " sizeOfVolatile %d, sizeOfTempor %d", &f.sizeOfVolatile, &f.sizeOfTempor) != 2);
            k->functions.push_back(f);
        }
        int nq = 0;
        if (!line(l) || sscanf(l.c_str(), "nbOfQuads %d", &nq) != 1) return false;
        if (p + (size_t)nq * 10 > d.size()) return false;
        k->quads.resize(nq);
        for (int i = 0; i < nq; ++i) {
            const uint8_t* q = &d[p + (size_t)i * 10];
            VMQuad& o = k->quads[i];
            o.op = q[0];
            o.a = (uint16_t)(q[1] | q[2] << 8);
            o.b = (uint16_t)(q[3] | q[4] << 8);
            o.c = (int32_t)((uint32_t)q[5] | (uint32_t)q[6] << 8 | (uint32_t)q[7] << 16 | (uint32_t)q[8] << 24);
        }
        p += (size_t)nq * 10;
        classes.push_back(std::move(k));
    }
    return true;
}

uint8_t* VMCore::Symbol(uint16_t s) {
    static uint8_t junk[8];
    uint32_t off = s & 0x3fff;
    std::vector<uint8_t>* a = nullptr;
    switch (s & 0xc000) {
    case 0x0000: a = &globals; break;
    case 0x4000: a = current ? &current->vars : nullptr; break;
    case 0x8000: a = stack.empty() ? nullptr : &stack.back().vol; break;
    default: a = stack.empty() ? nullptr : &stack.back().tmp; break;
    }
    if (!a) {
        std::memset(junk, 0, sizeof junk);
        return junk;
    }
    if (a->size() < off + 4) a->resize(off + 4, 0);
    return a->data() + off;
}

int32_t VMCore::Call(VMInstance& inst, const std::string& function, const std::vector<int32_t>& params) {
    const VMFunction* f = inst.cls ? inst.cls->Function(function) : nullptr;
    if (!f) return 0;
    // a native of a running script may call another script (an event): the running one's state
    // is kept
    VMInstance* saved = current;
    int32_t savedPc = pc, savedRet = retValue, savedNativeRet = nativeRet;
    int savedCount = nativeCount;
    int32_t savedParams[16];
    std::memcpy(savedParams, nativeParams, sizeof savedParams);
    std::vector<uint8_t> savedPending = pending;
    pending.clear();
    nativeCount = 0;
    current = &inst;
    if (inst.vars.size() < (size_t)inst.cls->sizeOfVariables) inst.vars.resize(inst.cls->sizeOfVariables, 0);
    Record r;
    r.params.resize(params.size() * 4);
    if (!params.empty()) std::memcpy(r.params.data(), params.data(), params.size() * 4);
    r.retAddress = -1;
    r.vol.resize(f->sizeOfVolatile, 0);
    size_t depth = stack.size();
    stack.push_back(std::move(r));
    pc = (int32_t)f->address;
    retValue = 0;
    Run();
    stack.resize(depth);
    current = saved;
    int32_t result = retValue;
    pc = savedPc;
    retValue = savedRet;
    nativeRet = savedNativeRet;
    nativeCount = savedCount;
    std::memcpy(nativeParams, savedParams, sizeof savedParams);
    pending = std::move(savedPending);
    return result;
}

static int32_t NotYet(VMCore&, const int32_t*) { return 0; }

void VMCore::Run() {
    const std::vector<VMQuad>& Q = current->cls->quads;
    int budget = 200000;   // a script that never ends is stopped
    size_t base = stack.size();
    while (budget-- > 0) {
        if (pc < 0 || (size_t)pc >= Q.size()) {
            SBError(false, "VMCore.cpp", 0x3ad, "SCRIPT %s: out of the code (%d)", current->cls->name.c_str(), pc);
            return;
        }
        const VMQuad& q = Q[pc];
        uint16_t c16 = (uint16_t)(q.c & 0xffff);
        switch (q.op) {
        case 0:   // Empty
            SBError(false, "VMCore.cpp", 0x21, "VMCore::Empty: Operation not allowed.");
            return;
        case 1:   // Nop
            ++pc;
            break;
        case 2: {  // Param
            uint8_t* v = Symbol(q.a);
            pending.insert(pending.end(), v, v + 4);
            ++pc;
            break;
        }
        case 3:   // BeginFunction: the sizes of the volatile variables and the temporaries
            stack.back().vol.assign(q.a, 0);
            stack.back().tmp.assign(q.b, 0);
            ++pc;
            break;
        case 4:   // EndFunction
            SBError(false, "VMCore.cpp", 0x49, "VMCore::EndFunction: Operation not allowed (add a Return !!!).");
            ++pc;
            break;
        case 5: {  // Call
            Record r;
            r.params.swap(pending);
            r.retAddress = pc + 1;
            stack.push_back(std::move(r));
            pc = q.imm();
            break;
        }
        case 0xe:  // Goto
            pc = q.imm();
            break;
        case 6:    // Return
        case 7: {  // ReturnVal
            if (q.op == 7) {
                int32_t v = I(q.a);
                if (stack.size() - base + 1 <= 1 || stack.size() < 2) retValue = v;
                else stack[stack.size() - 2].ret = v;
            }
            int32_t to = stack.back().retAddress;
            if (stack.size() <= base) {
                return;
            }
            if (to == -1) return;
            stack.pop_back();
            pc = to;
            break;
        }
        case 8: {  // GetParam
            Record& r = stack.back();
            uint32_t off = (uint32_t)q.c;
            int32_t v = 0;
            if (off + 4 <= r.params.size()) std::memcpy(&v, &r.params[off], 4);
            I(q.a) = v;
            ++pc;
            break;
        }
        case 9: {  // SetParam
            Record& r = stack.back();
            uint32_t off = (uint32_t)q.c;
            if (r.params.size() < off + 4) r.params.resize(off + 4, 0);
            int32_t v = I(q.a);
            std::memcpy(&r.params[off], &v, 4);
            ++pc;
            break;
        }
        case 0xa:  // GetReturn
            I(q.a) = stack.back().ret;
            ++pc;
            break;
        case 0xb:  // NativeParam
            if (nativeCount < 16) nativeParams[nativeCount++] = I(q.a);
            ++pc;
            break;
        case 0xc: {  // NativeCall
            int32_t n = q.imm();
            VMNative fn = (n >= 0 && n < 200) ? mparrayNativeFunctions[n] : nullptr;
            if (trace) SBLog("SCRIPT %s: %s", current->cls->name.c_str(), n >= 0 && n < 200 ? kNativeNames[n] : "?");
            if (!fn) {
                static bool told[200];
                if (n >= 0 && n < 200 && !told[n]) {
                    told[n] = true;
                    SBLog("script native %s not translated yet", kNativeNames[n]);
                }
                fn = NotYet;
            }
            nativeRet = fn(*this, nativeParams);
            nativeCount = 0;
            ++pc;
            break;
        }
        case 0xd:  // NativeGetReturn
            I(q.a) = nativeRet;
            ++pc;
            break;
        case 0xf:  // IfNotZeroGoto
            pc = I(q.a) != 0 ? q.c : pc + 1;
            break;
        case 0x10:  // IfZeroGoto
            pc = I(q.a) == 0 ? q.c : pc + 1;
            break;
        case 0x11: I(q.a) = I(q.b); ++pc; break;                      // Affect0Integer
        case 0x12: F(q.a) = F(q.b); ++pc; break;                      // Affect0Float
        case 0x13: I(q.a) = q.c; ++pc; break;                         // Affect0ConstantInteger
        case 0x14: {                                                  // Affect0ConstantFloat
            float f;
            std::memcpy(&f, &q.c, 4);
            F(q.a) = f;
            ++pc;
            break;
        }
        case 0x15: I(q.a) = -I(q.b); ++pc; break;                     // Affect1MinusInteger
        case 0x16: F(q.a) = -F(q.b); ++pc; break;                     // Affect1MinusFloat
        case 0x17: I(q.a) = (int32_t)F(q.b); ++pc; break;             // Affect1CastToInteger
        case 0x18: F(q.a) = (float)I(q.b); ++pc; break;               // Affect1CastToFloat
        case 0x19: I(q.a) = I(q.b) + I(c16); ++pc; break;
        case 0x1a: I(q.a) = I(q.b) - I(c16); ++pc; break;
        case 0x1b: I(q.a) = I(q.b) * I(c16); ++pc; break;
        case 0x1c: {
            int32_t d = I(c16);
            I(q.a) = d ? I(q.b) / d : 0;
            ++pc;
            break;
        }
        case 0x1d: F(q.a) = F(q.b) + F(c16); ++pc; break;
        case 0x1e: F(q.a) = F(q.b) - F(c16); ++pc; break;
        case 0x1f: F(q.a) = F(q.b) * F(c16); ++pc; break;
        case 0x20: F(q.a) = F(q.b) / F(c16); ++pc; break;
        case 0x21: I(q.a) = I(q.b) <= I(c16); ++pc; break;
        case 0x22: I(q.a) = I(q.b) < I(c16); ++pc; break;
        case 0x23: I(q.a) = I(q.b) >= I(c16); ++pc; break;
        case 0x24: I(q.a) = I(q.b) > I(c16); ++pc; break;
        case 0x25: I(q.a) = I(q.b) != I(c16); ++pc; break;
        case 0x26: I(q.a) = I(q.b) == I(c16); ++pc; break;
        case 0x27: I(q.a) = F(q.b) <= F(c16); ++pc; break;
        case 0x28: I(q.a) = F(q.b) < F(c16); ++pc; break;
        case 0x29: I(q.a) = F(q.b) >= F(c16); ++pc; break;
        case 0x2a: I(q.a) = F(q.b) > F(c16); ++pc; break;
        case 0x2b: I(q.a) = F(q.b) != F(c16); ++pc; break;
        case 0x2c: I(q.a) = F(q.b) == F(c16); ++pc; break;
        default:
            SBError(false, "VMCore.cpp", 0, "SCRIPT %s: unknown quad %u at %d", current->cls->name.c_str(), q.op, pc);
            return;
        }
    }
    SBError(false, "VMCore.cpp", 0, "SCRIPT %s: too long, stopped", current->cls->name.c_str());
}
