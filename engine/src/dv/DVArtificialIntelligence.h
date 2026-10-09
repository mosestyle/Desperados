// The NPCs' brains (Game/DVArtificialIntelligence.cpp, DVArtificialMalignity.cpp for the
// villains, DVArtificialBonhomie.cpp for the civilians). A brain is a state machine: a state
// (DVaIState), a substate (DVsubstate) and the stimuli (DVStimulus) that make it think: "I see
// something", "I hear something", "I reached the place I was walking to", "my timer ran out"...
// Think(stimulus) is the heart of each brain; StartThink / EndThink wrap every call.
//
// This step has the parts used by the patrols: the routes and their macro commands, the posts,
// the walks to the waypoints, the timers, the view cone settings. The other branches of Think
// (seeing, hearing, alarms, fights) come with the next steps; a stimulus they would handle is
// for now ignored.
#pragma once
#include <cstdint>
#include <deque>

#include "dv/DVHikingGuide.h"
#include "sb/SBGeo.h"

class SBFile;
class DVElement;
class DVElementActor;

// a stimulus (0x2c bytes)
struct DVStimulus {
    DVElement* element = nullptr;  // +0 who / what
    SBGeoPoint2D point;            // +4
    uint32_t u0c = 0;              // +0xc
    uint32_t u10 = 0;              // +0x10 (noise: kind)
    uint16_t u14 = 0;              // +0x14 (noise: volume)
    uint16_t u18 = 0xffff;         // +0x18 (index of a hero, a waypoint, ...)
    uint32_t source = 0;           // +0x20 (1 = a hero by index, 3 / 4 = element)
    uint32_t type = 0;             // +0x24
    uint32_t param = 0;            // +0x28
    static DVStimulus Of(uint32_t type) {
        DVStimulus s;
        s.type = type;
        return s;
    }
};

// stimulus types used so far
enum : uint32_t {
    STIM_VIEW = 0,            // sees someone
    STIM_IDENTIFY = 1,
    STIM_HEAR = 3,
    STIM_REACHED = 4,         // the walk (sequence) is finished
    STIM_FAILED = 5,          // the walk could not be done
    STIM_ACTION_DONE = 6,     // an action (animation) is finished
    STIM_TIMER = 8,           // the brain's timer ran out
    STIM_WAYPOINT_DONE = 0x21,
    STIM_RETURN_TO_DUTY = 0x22,
};

// states and substates seen so far (the original only has numbers)
enum : uint32_t {
    AI_SLEEPING = 0,
    AI_BORED = 1,
    AI_NORMAL = 2,    // on duty: patrol / post
    AI_CURIOUS = 4,
};
enum : uint32_t {
    SUB_ON_ROUTE = 0x7a,      // walking to the next waypoint
    SUB_TO_POST = 0x7b,       // going back to its post
    SUB_TO_POST_NEAR = 0x7c,
    SUB_BACK_ON_ROUTE = 0x7d, // walking back to the route
    SUB_BACK_ON_ROUTE_NEAR = 0x7e,
    SUB_ON_ROUTE_RIDING = 0x7f,
    SUB_AT_POST = 0x80,       // standing at its post
    SUB_AT_POST_2 = 0x84,
    SUB_MACRO = 0x85,         // running a waypoint's macro commands
};

class DVArtificialIntelligence {
public:
    explicit DVArtificialIntelligence(DVElementActor* owner);
    virtual ~DVArtificialIntelligence() = default;

    // the virtual base's table (0x0855eb78 / 0x0855d9e4)
    virtual void ReturnToDutyStandardProcedure() {}        // +0
    virtual int LoadAttributesFromFile(SBFile& f) = 0;     // +8
    virtual void SetState(uint32_t state, uint32_t sub);  // +0xc
    virtual void InitOneAI() {}                            // +0x10
    virtual void The16thFrame(uint8_t frame) {}            // +0x14
    virtual bool Think(DVStimulus& s) { return true; }     // +0x34
    virtual bool DefaultBoredStandardProcedure() { return false; }  // +0x38
    virtual bool IsVillain() const { return false; }

    // ---- DVArtificialIntelligence ---------------------------------------------------------------
    bool StartThink(DVStimulus& s);        // 0x081a1040
    void EndThink();                       // 0x081a1c30
    void ExecuteNextMacroCommand(bool keepWaypoint);  // 0x081a0040
    void ProceedOnPath();                  // 0x081a1db0
    bool GoTo(const DVposition& p, uint16_t flags);   // 0x081a1ed0
    bool GoTo(const DVWaypoint* w, uint16_t flags) { return GoTo(w->Position(), flags); }  // 0x081a0f60
    bool GoNear(const DVposition& p, uint16_t dist, uint16_t flags);  // 0x081a36d0
    void ExecuteWaypointScript(DVWaypoint* w);        // 0x081a0fd0
    void Face(const SBGeoPoint2D& p, bool check);     // 0x081a0a80
    bool FaceTo(uint8_t dir, bool check);              // 0x081a0b20
    bool FaceTo(const SBGeoVector2D& v, bool check);   // 0x081a37d0
    void SetAIState(uint32_t state, bool direct);      // 0x081a0660
    void LaunchTimer(uint32_t frames);                 // 0x081a31d0
    void BreakMacro();                                 // 0x081a0f20
    void SetAlertStatus(uint32_t alert, bool force);   // 0x081a18c0 (the brain's part)
    DVposition Position() const;                       // 0x081a3200 (for the owner)
    // the parts of DVElementActorNPC::Hourglass that run the brain: timers, the queue of stimuli
    void Hourglass();
    // a sequence the brain launched is over (DVElementActorNPC::SendCondolationCard)
    void SequenceDone(bool move, bool failed);

    DVElementActor* npc;                 // +4
    uint32_t state = 0;                  // +8
    uint32_t previousState = 0;          // +0xc
    uint32_t substate = 0;               // +0x10
    uint32_t alertStatus = 0;            // +0x14
    uint32_t timerSubstate = 0;          // +0x18 substate when the timer was launched
    uint32_t attitude = 0;               // +0x1c
    uint8_t drunk = 0;                   // +0x20
    uint32_t role = 0;                   // +0x24 the starting state given by the level
    DVPath path;                         // +0xd8 (in the NPC's virtual base)
    bool onRoute = false;                // +0xe4 has a route
    bool keepViewCone = false;           // +0xe5
    bool canMove = true;                 // +0xe6 has walk and run animations
    const uint8_t* macro = nullptr;      // +0xec
    int16_t macroLen = 0;                // +0xf0
    bool inMacro = false;                // +0xf2
    bool timerOn = false;                // +0x224
    uint32_t timerAt = 0;                // +0x228
    bool macroTimerOn = false;           // +0x22c
    uint32_t macroTimerAt = 0;           // +0x230
    bool glanceOn = false;               // +0x234
    uint32_t glanceUntil = 0;            // +0x238
    uint16_t boredTicks = 0;             // +0x244
    bool gotoFailed = false;             // +0x318
    bool gotoDone = false;               // +0x319
    bool riding = false;                 // +800
    bool atPost = false;                 // +0x322 (role 1)
    bool halted = false;                 // +0x325
    bool gotoActive = false;             // +0x35e
    DVposition gotoTarget;               // +0x348
    uint16_t gotoFlags = 0;              // +0x35c
    uint32_t viewCone = 0;               // +0x360
    bool busy = false;                   // +0x367
    std::deque<DVStimulus> queue;        // +0x36c
    bool halting = false;                // +0x450
    bool macroFirst = false;             // +0x451
    uint16_t routeFlags = 0;             // +0x478 added to the walks on the route
    uint32_t savedViewCone = 0;          // +0x47c

    static uint32_t* mpUniversalFrameCounter;
    static uint16_t muwStandardViewPolygonRadius;

protected:
    bool sequenceIsMove = false;
    // the shared part of the routes' arrival (state 2, substates 0x7a / 0x7f) in both brains
    void ArriveOnRoute(bool villain);
};

class DVArtificialMalignity : public DVArtificialIntelligence {
public:
    using DVArtificialIntelligence::DVArtificialIntelligence;
    int LoadAttributesFromFile(SBFile& f) override;  // 0x081e5dc0
    void SetState(uint32_t state, uint32_t sub) override;  // 0x081e4dc0
    void InitOneAI() override;                       // 0x081e50e0
    void The16thFrame(uint8_t frame) override;       // 0x081e4300
    bool Think(DVStimulus& s) override;              // 0x081b49a0
    void ReturnToDutyStandardProcedure() override;   // 0x081e2970
    bool DefaultBoredStandardProcedure() override;   // 0x081e3ad0
    bool IsVillain() const override { return true; }
    uint32_t profile = 0;    // +0x10 of the Malignity part
    uint16_t aiShort = 0;
    uint8_t aiByte = 0;
};

class DVArtificialBonhomie : public DVArtificialIntelligence {
public:
    using DVArtificialIntelligence::DVArtificialIntelligence;
    int LoadAttributesFromFile(SBFile& f) override;  // 0x0819e3a0
    void SetState(uint32_t state, uint32_t sub) override;  // 0x0819ded0
    void InitOneAI() override;                       // 0x0819e000
    void The16thFrame(uint8_t frame) override;       // 0x0819dce0
    bool Think(DVStimulus& s) override;              // 0x08199110
    void ReturnToDutyStandardProcedure() override;   // 0x0819d780
    uint32_t speech = 0;     // +0x10 of the Bonhomie part
    bool stayOnPost = false; // +4 (role 2)
    bool goesAway = false;   // +0xc
};
