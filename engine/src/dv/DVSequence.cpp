#include "dv/DVSequence.h"
#include "dv/DVMobile.h"

#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVScript.h"
#include "sb/SBFile.h"

DVSequenceManager* DVSequenceManager::mpInstance = nullptr;
bool DVSequenceManager::trace = false;

void DVSequenceElement::AsQuickSequence(uint16_t lvl, uint32_t cmd, DVElement* who, int32_t p, bool f10, uint32_t a,
                                        bool f11) {
    level = lvl;
    command = cmd;
    actor = who;
    param = p;
    flag10 = f10;
    anim = a;
    flag11 = f11;
}

void DVSequenceElement::AsPhysicsOperation(uint16_t lvl, uint32_t cmd, DVElement* who, const SBGeoPoint2D& p,
                                           DVElement* t, uint16_t p16) {
    level = lvl;
    command = cmd;
    actor = who;
    point = p;
    target = t;
    param16 = p16;
}

void DVSequenceElement::AsInteraction(uint16_t lvl, uint32_t cmd, DVElement* who, DVElement* t, uint16_t p16) {
    level = lvl;
    command = cmd;
    actor = who;
    target = t;
    param16 = p16;
}

void DVSequenceElement::SetState(int s) {
    if (state == s) return;
    int was = state;
    state = s;
    if ((s == SEQ_DONE || s == SEQ_CANCELLED || s == SEQ_STOPPED) && was != SEQ_DONE && was != SEQ_CANCELLED &&
        was != SEQ_STOPPED) {
        if (DVSequenceManager::trace)
            SBLog("  seq %p elem %p lvl %u cmd 0x%x -> %d (pending %d)", (void*)sequence, (void*)this, level, command, s,
                  sequence ? sequence->pending : -1);
        if (auto* a = dynamic_cast<DVElementActor*>(actor))
            if (a->scriptElement == this) a->scriptElement = nullptr;
        if (s == SEQ_DONE && eventTo && DVScript::mpInstance)
            if (auto* t = dynamic_cast<DVElementActor*>(eventTo)) DVScript::mpInstance->FilterEvent(t, nullptr, event);
        if (sequence) sequence->Ready();
    }
}

bool DVSequenceElement::ExecutedImmediately() {
    uint32_t c = command;
    bool actorSide = false, engineSide = false;
    if (c > 0x55) {
        uint32_t k = c - 0xa6;
        if (k < 0xb && ((0x1a7u >> k) & 1)) actorSide = true;
        else if (k == 10 || c - 0x56 <= 1) engineSide = true;
    } else if (c < 0xc && ((0x8b0u >> c) & 1)) {
        engineSide = true;
    } else if (c == 2 || c == 0x25 || c == 0x26 || c == 0x4a) {
        actorSide = true;
    }
    if (engineSide) {
        if (DVEngine::mpEngine) DVEngine::mpEngine->PerformExecuteCommand(this);
        else SetState(SEQ_DONE);
        return true;
    }
    if (actorSide) {
        if (auto* a = dynamic_cast<DVElementActor*>(actor)) a->ExecuteImmediately(this);
        else if (auto* m = dynamic_cast<DVElementMobile*>(actor)) m->ExecuteImmediately(this);
        else SetState(SEQ_DONE);
        return true;
    }
    return false;
}

void DVSequenceElement::Go() {
    if (state != SEQ_WAITING) return;
    if (!actor) {
        if (DVEngine::mpEngine) DVEngine::mpEngine->PerformExecuteCommand(this);
        else SetState(SEQ_DONE);
        return;
    }
    if (auto* a = dynamic_cast<DVElementActor*>(actor)) a->Instruct(this);
    else if (auto* t = dynamic_cast<DVElementTarget*>(actor)) t->Instruct(this);
    else SetState(SEQ_DONE);
}

void DVSequence::Append(DVSequenceElement* e) {
    e->sequence = this;
    elements.emplace_back(e);
}

bool DVSequence::Launch() {
    launched = true;
    if (next != elements.size()) {
        NextSequenceElementsGo();
        return true;
    }
    return false;
}

// the elements of the next level all go; the sequence waits for each of them (Ready)
void DVSequence::NextSequenceElementsGo() {
    if (next >= elements.size()) return;
    size_t first = next;
    uint16_t lvl = elements[first]->level;
    while (next < elements.size() && elements[next]->level == lvl) {
        ++next;
        ++pending;
    }
    // (an element done at once may start the next level from inside this loop)
    size_t end = next;
    for (size_t i = first; i < end && i < elements.size(); ++i)
        if (DVSequenceManager::mpInstance) DVSequenceManager::mpInstance->RegisterSequenceElementToGo(elements[i].get());
}

void DVSequence::Ready() {
    if (--pending != 0) return;
    NextSequenceElementsGo();
}

DVSequenceManager::DVSequenceManager() { mpInstance = this; }

DVSequenceManager::~DVSequenceManager() {
    toGo.clear();
    sequences.clear();
    if (mpInstance == this) mpInstance = nullptr;
}

DVSequence* DVSequenceManager::LaunchSequence(DVSequence* s) {
    sequences.emplace_back(s);
    s->Launch();
    return s;
}

void DVSequenceManager::RegisterSequenceElementToGo(DVSequenceElement* e) {
    if (trace)
        SBLog("seq %p elem %p lvl %u cmd 0x%x actor %s param %d at %.0f,%.0f", (void*)e->sequence, (void*)e, e->level, e->command,
              e->actor ? e->actor->KindName() : "-", e->param, e->point.x, e->point.y);
    if (!e->ExecutedImmediately()) toGo.push_back(e);
}

void DVSequenceManager::Hourglass() {
    size_t n = toGo.size();
    for (size_t i = 0; i < n && !toGo.empty(); ++i) {
        DVSequenceElement* e = toGo.front();
        toGo.pop_front();
        e->Go();
    }
    // FridayEveningCleanUp: the sequences that are over
    for (auto it = sequences.begin(); it != sequences.end();) {
        if ((*it)->Finished()) {
            it = sequences.erase(it);
        } else {
            ++it;
        }
    }
}

void DVSequenceManager::Forget(DVElement* who) {
    for (auto& s : sequences)
        for (auto& e : s->elements)
            if (e->actor == who && (e->state == SEQ_RUNNING || e->state == SEQ_WAITING)) {
                e->actor = nullptr;
                if (e->state == SEQ_RUNNING) e->SetState(SEQ_STOPPED);
            }
}
