// DVSequence, DVSequenceElement, DVSequenceManager (Game/DVSequence*.cpp): what the scripts' cut
// scenes are made of. A script records a sequence (IStart, IRecordMove, IRecordPlayDialog, ...,
// IThen, ..., IThanx); every recorded element has a level: the elements of one level run together,
// and when all of them are done (DVSequence::Ready) the next level starts
// (NextSequenceElementsGo). An element with an actor is handed to him (DVElementActor::Instruct:
// he turns it into his orders); the others are the engine's (DVEngine::PerformExecuteCommand:
// camera, dialogue, timers, the player's hands locked, the briefing).
//
// The original's elements also carry the actors' own orders (the player's walks and actions are
// sequences too); here the actors keep their own order lists and an element only waits for the
// orders it gave to be done.
#pragma once
#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <vector>

#include "sb/SBGeo.h"

class DVElement;
class DVElementActor;
class DVSequence;

// element states (DVsequenceState, +0x70)
enum : int { SEQ_DONE = 0, SEQ_RUNNING = 2, SEQ_WAITING = 3, SEQ_CANCELLED = 5, SEQ_STOPPED = 6 };

class DVSequenceElement {
public:
    uint16_t level = 0;            // +8
    uint32_t command = 0;          // +0xc DVcommand
    bool flag10 = false;           // +0x10
    bool flag11 = false;           // +0x11
    uint16_t param16 = 0;          // +0x16
    int32_t param = 0;             // +0x20
    float value = 0;               // +0x24
    DVElement* actor = nullptr;    // +0x28
    DVElement* target = nullptr;   // +0x2c
    SBGeoPoint2D point;            // +0x30
    uint16_t layer = 0;            // the point's layer
    SBGeoPoint2D point2;           // RecordEnterGame: where he comes from
    class DVSector* area2 = nullptr;
    class DVSector* goalArea = nullptr;  // a move's goal area (the location's +0x24)
    uint16_t goalLayer = 0;
    bool seek = false;             // RecordSeekActor: goes where the target is
    bool action = false;           // RecordAction: one of the actors' actions
    DVElement* eventTo = nullptr;  // RecordSeekActorCustomEvent: the event at the end
    int32_t event = 0;
    uint32_t anim = 0xb9;          // +0x50
    uint32_t anim2 = 0;            // +0x54
    int state = SEQ_WAITING;       // +0x70
    DVSequence* sequence = nullptr;  // +0x6c

    // DVSequenceElement::AsQuickSequence (0x08364700)
    void AsQuickSequence(uint16_t lvl, uint32_t cmd, DVElement* who, int32_t p, bool f10, uint32_t a, bool f11);
    // DVSequenceElement::AsPhysicsOperation (0x08364510): a place to go / to look at
    void AsPhysicsOperation(uint16_t lvl, uint32_t cmd, DVElement* who, const SBGeoPoint2D& p, DVElement* t,
                            uint16_t p16);
    // DVSequenceElement::AsInteraction (0x08364580): something done with someone / something
    void AsInteraction(uint16_t lvl, uint32_t cmd, DVElement* who, DVElement* t, uint16_t p16);
    // DVSequenceElement::SetState (0x08364780), the part used: done (0) tells the sequence
    void SetState(int s);
    // DVSequenceElement::ExecutedImmediately (0x08366360): the commands done at once
    bool ExecutedImmediately();
    // DVSequenceElement::Go (0x08364b90): hands the element to its actor or to the engine
    void Go();
};

class DVSequence {
public:
    std::vector<std::unique_ptr<DVSequenceElement>> elements;  // +8 list, +0x10 count
    size_t next = 0;               // +0x2c the first element not launched yet
    int pending = 0;               // +0x30 elements of the current level still running
    bool launched = false;         // +0x34
    void Append(DVSequenceElement* e);   // DVSequenceElement::AppendToSequence (0x08364b30)
    bool Launch();                       // DVSequence::Launch (0x08360300)
    void NextSequenceElementsGo();       // 0x08360330
    void Ready();                        // DVSequence::Ready (0x08360540)
    bool Finished() const { return next >= elements.size() && pending <= 0; }
};

class DVSequenceManager {
public:
    static DVSequenceManager* mpInstance;
    static bool trace;   // logs every element that goes
    DVSequenceManager();
    ~DVSequenceManager();
    DVSequence* LaunchSequence(DVSequence* s);   // 0x08366d90 (takes it)
    void RegisterSequenceElementToGo(DVSequenceElement* e);  // 0x08369110
    void Hourglass();                            // 0x08369050
    // the elements whose actor is gone (DVSequenceManager::FridayEveningCleanUp)
    void Forget(DVElement* e);
    bool Busy() const { return !sequences.empty(); }
    const std::list<std::unique_ptr<DVSequence>>& Sequences() const { return sequences; }

private:
    std::list<std::unique_ptr<DVSequence>> sequences;   // +4
    std::deque<DVSequenceElement*> toGo;                // +0x28
};
