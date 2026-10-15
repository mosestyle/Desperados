#include "dv/DVArtificialIntelligence.h"

#include <cmath>
#include <cstdlib>

#include "dv/DVArmsTrader.h"
#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVSector.h"
#include "dv/DVSprite.h"
#include "sb/SBFile.h"

static uint32_t gFrameCounter = 0;
uint32_t* DVArtificialIntelligence::mpUniversalFrameCounter = &gFrameCounter;
uint16_t DVArtificialIntelligence::muwStandardViewPolygonRadius = 300;
static uint8_t msbThinkMethodRecursionDepth = 0;

static uint32_t Now() { return *DVArtificialIntelligence::mpUniversalFrameCounter; }
// desprand (0x083b5610): the game's random numbers
static int desprand() { return std::rand() & 0x7fff; }

DVArtificialIntelligence::DVArtificialIntelligence(DVElementActor* owner) : npc(owner) {}

void DVArtificialIntelligence::ScriptLockAI(bool keep) {
    locked = true;
    lockKeep = keep;
    if (npc->CurrentCommand() != 0xa6) npc->Stop();
    inMacro = false;
    npc->KillTimer(true);
}

void DVArtificialIntelligence::ScriptUnlockAI() {
    for (uint8_t& d : detections) d = 0;
    locked = false;
    // the brain starts again: "back to your duty" (stimulus 0x22) unless one is waiting
    bool waiting = false;
    for (const DVStimulus& q : queue) waiting |= q.type == STIM_WAYPOINT_DONE;
    if (!waiting && state != 0 && !npc->knockedOut) {
        DVStimulus s = DVStimulus::Of(STIM_RETURN_TO_DUTY);
        Think(s);
    }
}

// the base SetState: the state and substate (each brain adds its own checks)
void DVArtificialIntelligence::SetState(uint32_t s, uint32_t sub) {
    state = s;
    substate = sub;
}

DVposition DVArtificialIntelligence::Position() const {
    DVposition p;
    DVPositionInterface& pos = npc->sprite->pos;
    p.p = pos.posMap;
    p.sector = pos.sector;
    p.layer = (int16_t)pos.layer;
    return p;
}

// DVArtificialIntelligence::StartThink: whether the brain thinks about this stimulus now. Busy
// brains queue it; some stimuli are handled right here.
bool DVArtificialIntelligence::StartThink(DVStimulus& s) {
    uint32_t type = s.type;
    ++msbThinkMethodRecursionDepth;
    // a brain the scripts locked: the stimuli wait (or are dropped) until it is unlocked
    if (locked) {
        if (lockKeep && type != STIM_REACHED && type != STIM_ACTION_DONE) queue.push_back(s);
        return false;
    }
    gotoFailed = gotoDone = false;
    previousState = state;
    uint32_t cmd = npc->CurrentCommand();
    if (cmd - 0xcfu > 1 && cmd != 0x69) busy = false;
    if (type == 0x11) SetAlertStatus(0, true);
    gotoFailed = gotoDone = false;
    if (!busy) {
        if ((npc->actionState == 6 || npc->knockedOut) && type != 0x11) {
            if (type != 0xf) return false;
            if (npc->sprite->pos.posture == 6) return false;
        }
        boredTicks = 0;
        if (!timerOn) {
            if (type == STIM_TIMER && substate != timerSubstate) return false;
        } else if (substate != timerSubstate) {
            npc->KillTimer(false);
        }
        if (npc->sprite->pos.posture == 0xe) return false;
        if (type != 0xf && substate == 2) return false;
        switch (type) {
        case STIM_REACHED:
            gotoActive = false;
            halted = false;
            if (inMacro) return false;
            break;
        case STIM_FAILED:
            if (halting) return false;
            gotoActive = false;
            halted = false;
            break;
        case 0xf:
            if ((substate > 6 || ((0x45u >> substate) & 1) == 0) && state != 0xb) return false;
            break;
        case 0x10:
            inMacro = false;
            npc->KillTimer(true);
            if (IsVillain())
                SetState(10, 0xaf);
            else
                SetState(8, 0xc5);
            npc->SetViewStatus(8, false);
            return false;
        case 0x11:
            inMacro = false;
            npc->KillTimer(true);
            SetState(0, 2);
            npc->SetViewStatus(8, false);
            SetAlertStatus(0, true);
            return false;
        case 0x17:
            if (!IsVillain()) return false;
            break;
        case 0x26:
            if (state != 5) return false;
            break;
        case STIM_RETURN_TO_DUTY:
            ReturnToDutyStandardProcedure();
            return false;
        case STIM_WAYPOINT_DONE: {
            // the stimuli kept while the waypoint's script ran are thought about now
            while (!queue.empty()) {
                if (busy) return false;
                DVStimulus q = queue.front();
                queue.pop_front();
                if (q.type != STIM_WAYPOINT_DONE) Think(q);
            }
            if (state == AI_NORMAL) {
                if (path.waypoints) {
                    path.Next();
                    SetState(AI_NORMAL, SUB_ON_ROUTE);
                    if (DVWaypoint* w = path.Current()) GoTo(w->Position(), routeFlags | 0x40);
                    return false;
                }
                ReturnToDutyStandardProcedure();
                return false;
            }
            break;
        }
        default:
            break;
        }
        return true;
    }
    queue.push_back(s);
    return false;
}

// DVArtificialIntelligence::EndThink: a walk that failed or ended during the thought is thought
// about at once
void DVArtificialIntelligence::EndThink() {
    // too deep (a route whose walks all fail or all end at once): back to duty, once (the
    // original's recursion can branch twice at each level and never end)
    static bool recovering = false;
    auto recover = [&]() {
        if (recovering || msbThinkMethodRecursionDepth >= 0x6f) return;
        recovering = true;
        ReturnToDutyStandardProcedure();
        gotoFailed = gotoDone = false;
        recovering = false;
    };
    if (gotoFailed) {
        gotoFailed = false;
        if (msbThinkMethodRecursionDepth < 100 && !recovering) {
            DVStimulus s = DVStimulus::Of(STIM_FAILED);
            Think(s);
        } else {
            recover();
        }
    }
    if (gotoDone) {
        gotoDone = false;
        if (msbThinkMethodRecursionDepth < 100 && !recovering) {
            DVStimulus s = DVStimulus::Of(STIM_REACHED);
            Think(s);
        } else {
            recover();
        }
    }
    if (msbThinkMethodRecursionDepth) --msbThinkMethodRecursionDepth;
}

// DVArtificialIntelligence::ExecuteNextMacroCommand: the next command of the waypoint's macro;
// at its end, on along the route (or the post's macro again after a while)
void DVArtificialIntelligence::ExecuteNextMacroCommand(bool keepWaypoint) {
    ++msbThinkMethodRecursionDepth;
    if (state == AI_NORMAL) SetState(AI_NORMAL, SUB_MACRO);
    boredTicks = 0;
    if (npc->sprite->pos.posture == 0xe) {
        --msbThinkMethodRecursionDepth;
        return;
    }
    int16_t len = macroLen;
    if (len < 1) {
        if (!DefaultBoredStandardProcedure()) {
            if (path.Count() != 1) {
                if (!keepWaypoint) path.Next();
                SetState(AI_NORMAL, SUB_ON_ROUTE);
                if (!keepViewCone) npc->SetViewCone(1);
                if (DVWaypoint* w = path.Current()) GoTo(w->Position(), routeFlags | 0x40);
                inMacro = false;
                npc->KillTimer(true);
            } else if (!macroFirst) {
                npc->SetViewCone(1);
                inMacro = false;
                npc->KillTimer(true);
                SetState(AI_NORMAL, SUB_ON_ROUTE);
                DVStimulus s = DVStimulus::Of(STIM_REACHED);
                Think(s);
            } else {
                SetState(AI_NORMAL, SUB_MACRO);
                macroFirst = false;
                npc->LaunchTimer(100, true);
            }
        } else {
            inMacro = false;
            npc->KillTimer(true);
        }
        --msbThinkMethodRecursionDepth;
        return;
    }
    macroLen = len - 1;
    const uint8_t* p = macro;
    macro = p + 1;
    uint8_t op = *p;
    inMacro = true;
    auto u16at = [](const uint8_t* q) { return (uint16_t)(q[0] | q[1] << 8); };
    bool next = true;
    switch (op) {
    case 0:  // turn back on the route
        path.forward = !path.forward;
        break;
    case 1:  // skip the next waypoint
        path.Next();
        macroLen = 0;
        break;
    case 2:  // go on at another waypoint
        path.SetCurrentWaypointIndex(u16at(p + 1));
        macroLen = 0;
        ExecuteNextMacroCommand(true);
        next = false;
        break;
    case 3:  // stop here
        next = false;
        break;
    case 4:  // change state
        macro = p + 2;
        SetAIState(p[1], false);
        macroLen -= 1;
        break;
    case 5:  // face a point
        Face(SBGeoPoint2D((float)u16at(p + 1), (float)u16at(p + 3)), false);
        macro += 4;
        macroLen -= 4;
        npc->SetViewStatus(1, false);
        break;
    case 6: {  // glance at a point
        SBGeoPoint2D at((float)u16at(macro), (float)u16at(macro + 2));
        DVposition g;
        g.p = at;
        DVFastFindGrid* grid = DVFastFindGrid::mpFastFindGrid;
        if (grid) {
            g.sector = grid->FindMotionArea(npc->sprite->pos.layer, at);
            g.layer = (int16_t)npc->sprite->pos.layer;
        }
        npc->GlanceAt(g, 0x1e);
        macro += 4;
        macroLen -= 4;
        ExecuteNextMacroCommand(false);
        next = false;
        break;
    }
    case 7:  // wait
        npc->LaunchTimer(u16at(p + 1), true);
        macro += 2;
        macroLen -= 2;
        macroFirst = false;
        next = false;
        break;
    case 8:   // check for a friend (InitializeFriendCheck): comes with the alarms
        if (!IsVillain()) SBError(true, "DVArtificialIntelligence.cpp", 0x242, "Waypoint command 'CheckFor' is illegal for civilists");
        macro = p + 5;
        macroLen = len - 5;
        macroFirst = false;
        next = false;
        break;
    case 9:
        if (!IsVillain()) SBError(true, "DVArtificialIntelligence.cpp", 0x253, "Waypoint command 'CheckFor' is illegal for civilists");
        macro = p + 7;
        macroLen = len - 7;
        macroFirst = false;
        next = false;
        break;
    case 10:  // face a direction
        FaceTo((uint8_t)u16at(p + 1), false);
        macro += 2;
        macroLen -= 2;
        break;
    default:
        macroLen = 0;
        break;
    }
    if (next) ExecuteNextMacroCommand(false);
    --msbThinkMethodRecursionDepth;
}

// DVArtificialIntelligence::ProceedOnPath: on to the next waypoint
void DVArtificialIntelligence::ProceedOnPath() {
    if (path.Count() == 1) {
        gotoDone = true;
        return;
    }
    path.Next();
    DVWaypoint* w = path.Current();
    if (!w) return;
    if (!riding || npc->horse || substate != SUB_ON_ROUTE_RIDING)
        GoTo(w->Position(), routeFlags | 0x44);
    else
        GoTo(w->Position(), routeFlags | 0x204);
}

// DVArtificialIntelligence::GoTo: walks (flag 1: runs) to the place; 0x20: then faces the
// direction of its post; 0x100: only turns towards it; 0x10: only a straight walk
bool DVArtificialIntelligence::GoTo(const DVposition& p, uint16_t flags) {
    uint32_t f = flags;
    if (drunk) f = 0;
    gotoActive = true;
    DVPositionInterface& pos = npc->sprite->pos;
    float tolerance = 0.0f;
    if ((f & 0x100) == 0) {
        float d = SBMaxNorm(pos.posMap - p.p);
        if (d < 5.0f && !atPost) {
            uint32_t c = npc->CurrentCommand();
            if (c == 0 || c == 0x75) {
                if (msbThinkMethodRecursionDepth) {
                    gotoDone = true;
                    return true;
                }
                DVStimulus s = DVStimulus::Of(STIM_REACHED);
                Think(s);
                return true;
            }
        }
    } else {
        uint8_t dir = SBGetSector0to15(p.p - pos.posMap, 1.0f);
        if (pos.direction == dir) {
            gotoDone = true;
            return true;
        }
        tolerance = 2.0f;
    }
    gotoTarget = p;
    gotoFlags = (uint16_t)f;
    DVEngine* e = DVEngine::mpEngine;
    if (p.p.x <= 0.0f || (e && p.p.x >= e->MapSize().x) || p.p.y <= 0.0f || (e && p.p.y >= e->MapSize().y) ||
        !p.sector || p.layer < 0) {
        gotoFailed = true;
        return false;
    }
    DVFastFindGrid* grid = DVFastFindGrid::mpFastFindGrid;
    if ((f & 0x10) == 0) {
        if ((f & 0x14) == 4 && (pos.sector != p.sector || pos.layer != (uint16_t)p.layer)) f ^= 4;
    } else if (grid && !grid->IsReachableThick(pos.posMap, p.p, pos.layer, pos.GetMoveBox(pos.posture).p1)) {
        gotoFailed = true;
        return false;
    }
    npc->Halt();
    uint32_t cmd;
    if (f & 8) {
        cmd = 0x85;
    } else {
        if ((f & 0x80) == 0) npc->DontFixDirectionOnViewTarget();
        cmd = (f & 1) ? CMD_RUN : CMD_WALK;
    }
    if ((f & 4) == 0 || pos.layer != (uint16_t)p.layer) halted = true;
    std::deque<DVOrder> seq;
    if (pos.layer != (uint16_t)p.layer) {
        // DVSequence::AppendMoveToSequence: floor changes come with the lifts
        gotoFailed = true;
        return false;
    }
    uint32_t moveCmd = cmd == CMD_RUN ? CMD_RUN : CMD_WALK;
    if (!npc->BuildMove(p.p, moveCmd, POSTURE_STANDING, seq)) {
        gotoFailed = true;
        return false;
    }
    for (DVOrder& o : seq) o.tolerance = tolerance;
    if (f & 0x20) {
        DVOrder t;
        t.id = DVOrder::NewID();
        t.command = CMD_TURN;
        t.direction = SBGetSector0to15(npc->homeDir, 0.57357645f);
        seq.push_back(t);
    }
    npc->Launch(seq, true);
    return true;
}

// DVArtificialIntelligence::GoNear: to within `dist` of the place (the place itself here)
bool DVArtificialIntelligence::GoNear(const DVposition& p, uint16_t dist, uint16_t flags) {
    (void)dist;
    return GoTo(p, flags & ~0x1000);
}

// DVArtificialIntelligence::ExecuteWaypointScript: the waypoint's script (IWaypointScript::
// ReachPoint) comes with the script engine; then "done" so that the walk goes on
void DVArtificialIntelligence::ExecuteWaypointScript(DVWaypoint* w) {
    (void)w;
    if (substate != 0x95) {
        DVStimulus s = DVStimulus::Of(STIM_WAYPOINT_DONE);
        Think(s);
    }
}

void DVArtificialIntelligence::Face(const SBGeoPoint2D& p, bool check) {
    FaceTo(SBGetSector0to15(p - npc->sprite->pos.posMap, 1.0f), check);
}

bool DVArtificialIntelligence::FaceTo(const SBGeoVector2D& v, bool check) {
    return FaceTo(SBGetSector0to15(v, 1.0f), check);
}

// DVArtificialIntelligence::FaceTo: turns on the spot (if `check`, only when there is room in
// front)
bool DVArtificialIntelligence::FaceTo(uint8_t dir, bool check) {
    DVPositionInterface& pos = npc->sprite->pos;
    if (check) {
        DVFastFindGrid* grid = DVFastFindGrid::mpFastFindGrid;
        SBGeoPoint2D ahead = pos.posMap + SBSetSector0to15(dir, 0.57357645f) * 30.0f;
        if (grid && !grid->IsReachableThick(pos.posMap, ahead, pos.layer, SBGeoVector2D(0.5f, 0.5f))) return false;
    }
    npc->Halt();
    std::deque<DVOrder> seq;
    if (pos.posture == POSTURE_GETTING_UP) {
        DVOrder u;
        u.id = DVOrder::NewID();
        u.command = CMD_STAND_UP;
        seq.push_back(u);
    }
    DVOrder t;
    t.id = DVOrder::NewID();
    t.command = CMD_TURN;
    t.direction = dir & 0xf;
    seq.push_back(t);
    npc->Launch(seq, false);
    return true;
}

// DVArtificialIntelligence::SetAIState: a state change ordered by a macro or a script
void DVArtificialIntelligence::SetAIState(uint32_t s, bool direct) {
    switch (s) {
    case 0:
        SetState(0, 0);
        npc->CloseEyes();
        break;
    case 1:
        SetState(1, 6);
        npc->SetViewCone(0xd);
        break;
    case 2:
        if (!direct) {
            DVStimulus st = DVStimulus::Of(STIM_RETURN_TO_DUTY);
            Think(st);
        } else {
            SetState(AI_NORMAL, SUB_MACRO);
        }
        break;
    case 4:
        SetState(4, 0x4f);
        npc->SetViewCone(2);
        npc->LaunchTimer(0x3c, false);
        break;
    default:
        break;
    }
}

void DVArtificialIntelligence::LaunchTimer(uint32_t frames) { npc->LaunchTimer(frames ? frames : 1, false); }

void DVArtificialIntelligence::BreakMacro() {
    inMacro = false;
    npc->KillTimer(true);
}

// DVArtificialIntelligence::SetAlertStatus, the brain's part (the overall alert and the music
// come with the alarms)
// the alert statuses' levels (DAT_08558d40): calm 0, suspicious 1, alarmed 2
static uint32_t AlertLevel(uint32_t a) {
    static const uint32_t k[6] = {0, 1, 2, 1, 0, 1};
    return a < 6 ? k[a] : 2;
}
uint32_t DVArtificialIntelligence::mCurrentOverallAlertStatus = 0;
uint32_t DVArtificialIntelligence::mCurrentOverallVillainAlertStatus = 0;

void DVArtificialIntelligence::SetAlertStatus(uint32_t alert, bool force) {
    (void)force;  // forces the calm music (the sound comes with step 5)
    uint32_t old = alertStatus;
    uint32_t oldLevel = AlertLevel(old);
    if (old != alert) {
        bool common = true;
        switch (alert) {
        case 0:
            suspect = 0x1fff;   // calm again, but still wary
            yellow = 0;
            yellowBack = 0;
            npc->ResetDazzlingImmunity();
            common = false;
            break;
        case 1:
            suspect = 0x2000;
            if (old == 5) {
                yellow = (uint16_t)(0x1fff - yellowBack);
                u24e = 0;
                yellowBack--;
                common = false;
            } else if (old == 2) {
                yellowBack = 0;
                yellow = 0x1fff;
            } else if (old == 0) {
                yellowBack = 0;
                yellow = 0;
            } else {
                common = false;
            }
            break;
        case 2:
            suspect = 0x2000;
            yellow = 0x2000;
            yellowBack = 0;
            u24e = 0;
            common = false;
            break;
        case 3:
            suspect = 0x2000;
            yellow = old == 0 ? 0 : 0x1fff;
            yellowBack = 0;
            break;
        case 5:
            suspect = 0x2000;
            yellowBack = (uint16_t)(0x1fff - yellow);
            break;
        default:
            common = false;
            break;
        }
        if (common) u24e = 0;
    }
    alertStatus = alert;
    npc->view.alert = alert;
    uint32_t level = AlertLevel(alert);
    DVEngine* e = DVEngine::mpEngine;
    // the villains' overall status: the highest of them (for the music)
    uint32_t v = level;
    if (level <= mCurrentOverallVillainAlertStatus) {
        v = mCurrentOverallVillainAlertStatus;
        if (level < oldLevel && oldLevel == mCurrentOverallVillainAlertStatus && e) {
            v = level;
            for (DVElementActor* n : e->NPCs()) {
                if (!n->ai || !n->ai->IsVillain()) continue;
                uint32_t l = AlertLevel(n->ai->alertStatus);
                if (l > v) v = l;
                if (v == oldLevel) break;
            }
        }
    }
    mCurrentOverallVillainAlertStatus = v;
    // everyone's: when it rises, the view cone of the one who raised it is shown
    if (mCurrentOverallAlertStatus < level) {
        mCurrentOverallAlertStatus = level;
        if (attitude > 1 && e) e->watched = npc;
    } else if (level < oldLevel && oldLevel == mCurrentOverallAlertStatus && e) {
        uint32_t o = level;
        for (DVElementActor* n : e->NPCs()) {
            if (!n->ai) continue;
            uint32_t l = AlertLevel(n->ai->alertStatus);
            if (l > o) o = l;
            if (o == oldLevel) break;
        }
        mCurrentOverallAlertStatus = o;
    }
}

// the brain's part of DVElementActorNPC::Hourglass: the timers and the stimuli kept for later
void DVArtificialIntelligence::Hourglass() {
    if (busy || locked) return;
    uint32_t now = Now();
    if (timerOn && (timerAt <= now || now + 1000000 < timerAt)) {
        timerOn = false;
        DVStimulus s = DVStimulus::Of(STIM_TIMER);
        Think(s);
    }
    if (macroTimerOn && macroTimerAt <= now) {
        macroTimerOn = false;
        if (state == 1 || substate == SUB_MACRO) ExecuteNextMacroCommand(false);
    }
    if (glanceOn && glanceUntil <= now) {
        glanceOn = false;
        npc->RestoreView();
    }
    while (!queue.empty()) {
        if (busy) return;
        DVStimulus s = queue.front();
        queue.pop_front();
        Think(s);
    }
}

// DVElementActorNPC::SendCondolationCard: a walk ends with "reached" (or "failed")
void DVArtificialIntelligence::SequenceDone(bool move, bool failed) {
    if (!move) return;
    DVStimulus s = DVStimulus::Of(failed ? STIM_FAILED : STIM_REACHED);
    Think(s);
}

// DVArtificialIntelligence::UpdateDetection: hero i is now seen (1), identified (2) or not seen
// (0); the brain hears of each change: "I see someone" (0), "I know him" (1), "he's gone" (2)
void DVArtificialIntelligence::UpdateDetection(uint8_t i, uint8_t level, DVElement* hero) {
    if (detections.size() <= i) detections.resize((size_t)i + 1, 0);
    uint8_t old = detections[i];
    if (old == 3) return;
    if (level != 1 || old != 2) detections[i] = level;
    if (level != 0) lastSeen = Now();
    if (old == level) return;
    npc->detecting = true;   // the cone flashes white
    if (old == 0) {
        DVStimulus s = DVStimulus::Of(STIM_VIEW);
        s.element = hero;
        s.source = 3;
        Think(s);
    }
    if (level == 2) {
        DVStimulus s = DVStimulus::Of(STIM_IDENTIFY);
        s.element = hero;
        s.source = 3;
        Think(s);
    } else if (level == 0) {
        if (hero && hero->sprite) {
            lastKnown.p = hero->sprite->pos.posMap;
            lastKnown.sector = hero->sprite->pos.sector;
            lastKnown.layer = (int16_t)hero->sprite->pos.layer;
        }
        DVStimulus s = DVStimulus::Of(STIM_LOST);
        s.element = hero;
        s.source = 3;
        Think(s);
    }
}

// DVArtificialMalignity::React, the waiting part: stops, and thinks again after a moment (the
// character profile's quickness comes with the fights)
void DVArtificialIntelligence::React(uint16_t frames, bool b) {
    int r = frames ? (desprand() % frames) + frames / 2 : 1;
    (void)b;
    LaunchTimer((uint32_t)r);
    npc->Halt();
}

// the arrival on a route's waypoint (state 2, substates 0x7a / 0x7f), shared by both brains:
// the waypoint's macro (one option drawn by its probability), its script, or on to the next one
void DVArtificialIntelligence::ArriveOnRoute(bool villain) {
    if (!path.waypoints || path.Count() == 0) {
        ReturnToDutyStandardProcedure();
        return;
    }
    DVWaypoint* w = path.Current();
    if (!w) return;
    if (w->dataLen == 0) {
        path.Next();
        if (villain && DefaultBoredStandardProcedure()) return;
        if (path.Count() == 1) {
            onRoute = false;
            npc->StoreInitialPositionParameters();
            ReturnToDutyStandardProcedure();
            return;
        }
        if (DVWaypoint* n = path.Current()) GoTo(n->Position(), routeFlags | (substate == SUB_ON_ROUTE ? 0x44 : 0x40));
        return;
    }
    if (w->isScript) {
        ExecuteWaypointScript(w);
        return;
    }
    const uint8_t* d = w->data.data();
    auto s16at = [](const uint8_t* q) { return (int16_t)(q[0] | q[1] << 8); };
    auto u16at = [](const uint8_t* q) { return (uint16_t)(q[0] | q[1] << 8); };
    int16_t lists = s16at(d);
    const uint8_t* list = nullptr;
    if (path.HasRightDirection(d[2]))
        list = d + 3;
    else if (lists == 2 && path.HasRightDirection(d[5]))
        list = d + 6;
    if (!list) {
        ProceedOnPath();
        return;
    }
    uint16_t off = u16at(list);
    int16_t options = s16at(d + off);
    const uint8_t* opt = d + off + 2;
    uint32_t r = (uint32_t)(desprand() % 100 + 1) & 0xff;
    while (true) {
        if (r <= opt[0]) {
            uint16_t m = u16at(opt + 1);
            macroLen = s16at(d + m);
            macro = d + m + 2;
            SetState(AI_NORMAL, SUB_MACRO);
            keepViewCone = false;
            macroFirst = true;
            ExecuteNextMacroCommand(false);
            return;
        }
        r = (r - opt[0]) & 0xff;
        opt += 3;
        if (--options == 0) break;
    }
    ProceedOnPath();
}

// ---- the villains (DVArtificialMalignity) ----------------------------------------------------------

// DVArtificialMalignity::LoadAttributesFromFile: the character profile (DVPsychoanalyst's
// compendium) and the weapon come with the fights
int DVArtificialMalignity::LoadAttributesFromFile(SBFile& f) {
    profile = f.U32();
    aiShort = f.U16();
    // the character profile: his properties (+0x104..), his firearm and his life (the energy)
    const DVCharacterProfile* cp =
        DVPsychoanalyst::mpTheOnlyPsychoanalistInTown ? DVPsychoanalyst::mpTheOnlyPsychoanalistInTown->GetCharacterFromCompendium(profile)
                                                      : nullptr;
    if (cp) {
        for (int i = 0; i < 7; ++i) properties[i] = cp->properties[i];
        properties[7] = cp->hobbies[0];
        npc->firearm = DVArmsTrader::mpTheOnlyArmstraderInTown ? DVArmsTrader::mpTheOnlyArmstraderInTown->GetFirearmFromCatalog(cp->firearm)
                                                               : nullptr;
    }
    npc->life = properties[0];
    uint8_t a = f.U8() & 0x7f;
    if (a == 3)
        attitude = 4;
    else if (a == 2)
        attitude = 2;
    else if (a == 1)
        attitude = 1;
    drunk = f.U8();
    // no property may be 0 (the courage, the IQ, the shooting skill, ...)
    for (int i : {0, 1, 2, 3, 4, 5, 6})
        if (properties[i] == 0) properties[i] = 1;
    npc->villainSkill = properties[6];
    if (npc->firearm) npc->ammo = (int16_t)npc->firearm->magazine;
    return 8;
}

void DVArtificialMalignity::SetState(uint32_t s, uint32_t sub) {
    state = s;
    substate = sub;
}

// DVArtificialMalignity::InitOneAI (the parts without horses, gattlings and ambushes)
void DVArtificialMalignity::InitOneAI() {
    npc->lastDirection = npc->sprite->pos.direction;
    npc->view.radius = npc->view.radiusNow = muwStandardViewPolygonRadius;
    bool init = npc->InitState(*this);
    DVPositionInterface& pos = npc->sprite->pos;
    DVFastFindGrid* grid = DVFastFindGrid::mpFastFindGrid;
    if (grid) {
        SBGeoBoundingBox2D box = pos.GetMoveBox(pos.posture) + pos.posMap;
        if (!grid->FindAutorizedPosition(box, pos.posMap, pos.layer)) {
            SBError(false, "DVArtificialMalignity.cpp", 0x38fc,
                    "NPC at position (%u, %u) stuck in obstacle, unable to find a good position for him.",
                    (unsigned)pos.posMap.x, (unsigned)pos.posMap.y);
        } else {
            pos.SetPositionMap(SBCenter(box));
        }
    }
    npc->StoreInitialPositionParameters();
    if (path.Count() == 0) {
        onRoute = false;
        if (init && role == 0) npc->SetViewCone(0);
    } else {
        onRoute = true;
        timerSubstate = substate;
        if (init) {
            SetState(AI_NORMAL, SUB_ON_ROUTE);
            ReturnToDutyStandardProcedure();
        }
    }
    savedViewCone = viewCone;
}

// the substates in which an idle NPC is waiting on purpose (The16thFrame does not send it back
// to its duty)
static bool WaitingSubstate(uint32_t s) {
    if (s < 0x80) return (s >= 0x25 && s <= 0x27) || s == 6 || s == 0x74;
    if (s - 0x80 < 0x16 && ((0x200805u >> (s - 0x80)) & 1)) return true;
    if (s - 0xc2 < 0x16 && ((0x201001u >> (s - 0xc2)) & 1)) return true;
    return false;
}

// DVArtificialMalignity::The16thFrame: every 16 frames (the moods, the drunk ones' songs, the
// alarms' decay come with the next steps); every 256 frames, a villain idle for too long goes
// back to his duty
void DVArtificialMalignity::The16thFrame(uint8_t frame) {
    if (npc->sprite->pos.posture == 0xe) return;
    if (npc->knockedOut) SetState(0, 2);
    if ((frame & 0x3f) != 0 || frame != 0) return;
    if (state == AI_NORMAL) SetAlertStatus(0, false);
    if (drunk) --drunk;
    if (state == 0) return;
    if (npc->CurrentCommand() != 0 || timerOn || macroTimerOn) return;
    if (++boredTicks > 10 && !WaitingSubstate(substate)) {
        boredTicks = 0;
        ReturnToDutyStandardProcedure();
    }
}

// DVArtificialMalignity::ReturnToDutyStandardProcedure: back to the route (its nearest
// waypoint) or to the post
void DVArtificialMalignity::ReturnToDutyStandardProcedure() {
    if (npc->knockedOut) return;
    SetAlertStatus(0, false);
    if (state != AI_NORMAL) npc->SetViewCone(1);
    if (!onRoute) {
        if (!atPost) {
            if (!riding) {
                SetState(AI_NORMAL, SUB_TO_POST);
                GoTo(npc->home, 0x40);
            } else {
                SetState(AI_NORMAL, SUB_TO_POST_NEAR);
                GoNear(npc->home, 10, 0x1200);
            }
        } else if (npc->CurrentCommand() != 1) {
            SetState(AI_NORMAL, SUB_TO_POST);
            GoTo(npc->home, 0x20);
        }
        return;
    }
    // the nearest waypoint; the next one if it is behind
    DVposition me = Position();
    size_t n = path.Count();
    float best = 54321.0f;
    uint16_t bi = 0;
    for (size_t i = 0; i < n; ++i) {
        const DVWaypoint* w = (*path.waypoints)[i].get();
        float d = SBMaxNorm(SBGeoPoint2D((float)w->x, (float)w->y) - me.p);
        if (d < best) {
            bi = (uint16_t)i;
            best = d;
        }
    }
    path.SetCurrentWaypointIndex(bi);
    DVWaypoint* w = path.Current();
    SBGeoVector2D to = w->Position().p - me.p;
    if (SBMaxNorm(to) < 10.0f) {
        path.Next();
    } else {
        // DVPath::LookupNextWaypoint
        const DVWaypoint* nx = nullptr;
        if (path.Count() == 1) {
            nx = (*path.waypoints)[0].get();
        } else if (path.current != 0 && (path.current == path.Count() - 1 || !path.forward)) {
            nx = (*path.waypoints)[path.current - 1].get();
        } else {
            nx = (*path.waypoints)[path.current + 1].get();
        }
        if (SBDot(to, SBGeoPoint2D((float)nx->x, (float)nx->y) - w->Position().p) < 0.0f) path.Next();
    }
    if (!riding || npc->horse) {
        SetState(AI_NORMAL, SUB_BACK_ON_ROUTE);
        if (DVWaypoint* t = path.Current()) GoTo(t->Position(), routeFlags | 0x40);
    } else {
        SetState(AI_NORMAL, SUB_BACK_ON_ROUTE_NEAR);
        if (DVWaypoint* t = path.Current()) GoNear(t->Position(), 10, 0x1200);
    }
}

// DVArtificialMalignity::DefaultBoredStandardProcedure: the drunk and bored ones' moods come
// with the next steps
bool DVArtificialMalignity::DefaultBoredStandardProcedure() {
    uint32_t s = substate - 0x7b;
    if (s < 5 && ((0x15u >> s) & 1)) return false;
    return false;
}

// DVArtificialMalignity::EventViewStandardProcedure: a villain sees someone. A hero he does not
// know yet makes him suspicious (state 3, the yellow cone): he stops, faces him and watches; the
// hostile ones (attitude 4) know at once
void DVArtificialMalignity::EventViewStandardProcedure(DVElement* e) {
    BreakMacro();
    if (attitude - 1u < 2) {
        if (drunk > 0x32) {
            npc->GlanceAt(Position(), 0x1e);
            return;
        }
        SetState(3, 0xf);
        target = e;
        SetAlertStatus(1, false);
        if (e && e->sprite) Face(e->sprite->pos.posMap, false);
        npc->SetViewTarget(e, true);
        npc->SetViewCone(8);
        React(10, true);
        return;
    }
    if (attitude != 4) return;
    SetAlertStatus(1, false);
    DVEngine* eng = DVEngine::mpEngine;
    if (eng)
        for (size_t i = 0; i < eng->Heroes().size(); ++i)
            if (eng->Heroes()[i] == e) UpdateDetection((uint8_t)i, 2, e);
}

// DVArtificialMalignity::EventIdentifyStandardProcedure: he knows who it is: the alarm (the red
// cone). He calls his friends around (AlertFriends 0x226) and fights (Fight)
void DVArtificialMalignity::EventIdentifyStandardProcedure(DVElement* e) {
    BreakMacro();
    if (attitude != 4 && attitude != 2) return;
    SetAlertStatus(2, false);
    attitude = 4;
    target = e;
    if (e && e->sprite) {
        lastKnown.p = e->sprite->pos.posMap;
        lastKnown.sector = e->sprite->pos.sector;
        lastKnown.layer = (int16_t)e->sprite->pos.layer;
    }
    searched = false;
    searchRounds = 0;
    AlertFriends(0x226);
    Fight(true, true);
}

bool DVArtificialMalignity::MyIsDetecting(const DVElement* e) const {
    DVEngine* eng = DVEngine::mpEngine;
    if (!eng || !e) return false;
    const auto& h = eng->Heroes();
    for (size_t i = 0; i < h.size() && i < detections.size(); ++i)
        if (h[i] == e) return detections[i] != 0;
    return false;
}

static DVElementActor* AsActor(DVElement* e) { return dynamic_cast<DVElementActor*>(e); }

// DVArtificialMalignity::Fight: in sight, he stops, aims and shoots (ShootAt), then thinks again
// 20 frames later; else he looks for his enemies (GetBattleOverview)
void DVArtificialMalignity::Fight(bool first, bool b) {
    npc->KillTimer(false);
    DVElementActor* t = AsActor(target);
    if (!t || t->dead || t->sprite->pos.posture == 0xe || !MyIsDetecting(t)) {
        GetBattleOverview(b);
        return;
    }
    if (first) {
        npc->Halt();
        SetState(10, SUB_FIGHT_SHOT);
    } else {
        SetState(10, SUB_FIGHT);
    }
    npc->SetViewTarget(t, true);
    npc->SetViewCone(0xb);
    ShootAt(t);
    LaunchTimer(0x14);
}

// DVArtificialMalignity::ShootAt: the "shoot at" interaction (0x32) with his firearm
void DVArtificialMalignity::ShootAt(DVElement* t) {
    if (!npc->firearm || !npc->OrderShoot(t)) return;
}

// DVArtificialMalignity::GetBattleOverview: stops and looks around for the enemies he knows
// (state 10, 0xa7), then decides (the timer: 6 frames, 18 when looking around slowly)
void DVArtificialMalignity::GetBattleOverview(bool b) {
    npc->Halt();
    SetState(10, SUB_OVERVIEW);
    npc->SetViewCone(b ? 4 : 3);
    LaunchTimer(b ? 6 : 0x12);
}

void DVArtificialMalignity::BattleDecisions() {
    DVEngine* eng = DVEngine::mpEngine;
    if (!eng) return;
    DVPositionInterface& pos = npc->sprite->pos;
    // the nearest hero he sees (and who is alive)
    DVElementActor* best = nullptr;
    float bestD = 1e30f;
    const auto& heroes = eng->Heroes();
    for (size_t i = 0; i < heroes.size() && i < detections.size(); ++i) {
        DVElementActor* h = heroes[i];
        if (!detections[i] || h->dead) continue;
        SBGeoVector2D d = h->sprite->pos.posMap - pos.posMap;
        float dd = d.x * d.x + d.y * d.y * 3.04f;
        if (dd < bestD) {
            bestD = dd;
            best = h;
        }
    }
    if (best) {
        target = best;
        lastKnown.p = best->sprite->pos.posMap;
        lastKnown.sector = best->sprite->pos.sector;
        lastKnown.layer = (int16_t)best->sprite->pos.layer;
        searched = false;
        searchRounds = 0;
        float range = npc->firearm ? (float)npc->firearm->range : 0.0f;
        float dist = std::sqrt(bestD);
        if (npc->firearm && dist < range * 0.85f && dist <= (float)npc->view.length) {
            Fight(false, false);
            return;
        }
        // too far for his gun: he runs nearer
        SetState(10, SUB_CHASE);
        npc->SetViewTarget(best, false);
        if (!GoTo(lastKnown, 1)) GetBattleOverview(false);
        else LaunchTimer(25);
        return;
    }
    // nobody in sight: where was he last seen?
    if (!searched && lastKnown.sector) {
        searched = true;
        SetState(10, SUB_CHASE);
        npc->SetViewCone(4);
        if (GoTo(lastKnown, 1)) {
            LaunchTimer(250);
            return;
        }
    }
    if (searchRounds < 3) {
        // DVArtificialMalignity::SeekArea: looks around
        ++searchRounds;
        SetState(10, SUB_SEARCH);
        npc->Halt();
        npc->SetViewCone(3);
        if (searchRounds > 1) FaceTo((uint8_t)((pos.direction + 5 + desprand() % 7) & 0xf), false);
        LaunchTimer(50 + desprand() % 25);
        return;
    }
    // he gives up: suspicious for a while, then back to his duty
    SetAlertStatus(1, false);
    SetState(3, SUB_WATCH);
    npc->SetViewCone(8);
    lastSeen = Now();
    LaunchTimer(50);
}

// DVArtificialMalignity::EventGetBulletStandardProcedure: shot at, he knows who his enemy is
void DVArtificialMalignity::EventGetBulletStandardProcedure(DVElement* shooter) {
    BreakMacro();
    attitude = 4;
    target = shooter;
    if (shooter && shooter->sprite) {
        lastKnown.p = shooter->sprite->pos.posMap;
        lastKnown.sector = shooter->sprite->pos.sector;
        lastKnown.layer = (int16_t)shooter->sprite->pos.layer;
        npc->SetViewTarget(shooter, true);
    }
    SetAlertStatus(2, false);
    searched = false;
    searchRounds = 0;
    npc->SetViewCone(6);
    if (state != 10) AlertFriends(0x226);
    if (MyIsDetecting(shooter)) Fight(true, true);
    else GetBattleOverview(true);
}

// DVArtificialMalignity::EventHearStandardProcedure: a shot (or a body falling): he becomes
// hostile, turns to it and goes to see
void DVArtificialMalignity::EventHearStandardProcedure(const DVStimulus& s) {
    uint32_t kind = s.u10;
    if (kind == 0 || kind == 1 || kind == 4 || kind == 5) attitude = 4;
    if (state == 10) {
        if (!target || !MyIsDetecting(target)) {
            lastKnown.p = s.point;
            if (DVFastFindGrid* g = DVFastFindGrid::mpFastFindGrid) {
                DVSectorMotionArea* a = g->FindMotionArea(npc->sprite->pos.layer, s.point);
                if (a) lastKnown.sector = a;
            }
            searched = false;
        }
        return;
    }
    BreakMacro();
    lastKnown.p = s.point;
    lastKnown.layer = (int16_t)npc->sprite->pos.layer;
    lastKnown.sector = npc->sprite->pos.sector;
    if (DVFastFindGrid* g = DVFastFindGrid::mpFastFindGrid)
        if (DVSectorMotionArea* a = g->FindMotionArea(npc->sprite->pos.layer, s.point)) lastKnown.sector = a;
    SetAlertStatus(1, false);
    SetState(3, SUB_NOISE);
    npc->SetViewTarget(s.point, false);
    npc->SetViewCone(2);
    Face(s.point, false);
    lastSeen = Now();
    // after a moment he goes there
    LaunchTimer(15 + desprand() % 20);
}

// DVArtificialMalignity::CallAlertStandardProcedure: a friend calls: he turns to him, alerted
void DVArtificialMalignity::CallAlertStandardProcedure(DVElement* f) {
    BreakMacro();
    alerter = f;
    SetState(3, SUB_HELP);
    if (f && f->sprite) Face(f->sprite->pos.posMap, false);
    SetAlertStatus(1, false);
    npc->SetViewTarget(f, true);
    npc->SetViewCone(8);
    lastSeen = Now();
    React(0x1e, false);
}

// DVArtificialMalignity::AlertFriends: the villains around who can see him (or are near
// enough) are called for help (stimulus 0x27)
void DVArtificialMalignity::AlertFriends(uint16_t radius) {
    DVEngine* eng = DVEngine::mpEngine;
    if (!eng) return;
    DVPositionInterface& pos = npc->sprite->pos;
    DVFastFindGrid* grid = DVFastFindGrid::mpFastFindGrid;
    for (DVElementActor* n : eng->NPCs()) {
        if (n == npc || !n->ai || !n->ai->IsVillain() || n->dead || n->ai->state == 0 || n->ai->state == 10) continue;
        SBGeoVector2D d = n->sprite->pos.posMap - pos.posMap;
        if (SBMaxNorm(d) >= (float)radius || d.x * d.x + d.y * d.y >= (float)radius * radius) continue;
        // IsDetecting360Degrees: he must see the one who calls
        bool sees = d.x * d.x + d.y * d.y < 150.0f * 150.0f ||
                    !grid || grid->IsReachable(n->EyesPoint(), npc->EyesPoint(), true);
        if (!sees) continue;
        DVStimulus st = DVStimulus::Of(STIM_HELP);
        st.element = npc;
        st.source = 3;
        n->ai->Think(st);
    }
}

// DVArtificialMalignity::Think, the patrol part of state 2
bool DVArtificialMalignity::Think(DVStimulus& s) {
    if (!StartThink(s)) {
        EndThink();
        return true;
    }
    uint32_t type = s.type;
    if (type == STIM_VIEW && state <= 4 && state != 0) {
        EventViewStandardProcedure(s.element);
        EndThink();
        return true;
    }
    if (type == STIM_IDENTIFY && state != 0) {
        EventIdentifyStandardProcedure(s.element);
        EndThink();
        return true;
    }
    if (state == 0 || npc->dead) {
        EndThink();
        return true;
    }
    if (type == STIM_BULLET) {
        EventGetBulletStandardProcedure(s.element);
        EndThink();
        return true;
    }
    if (type == STIM_HEAR) {
        EventHearStandardProcedure(s);
        EndThink();
        return true;
    }
    if (type == STIM_HELP && state != 10) {
        CallAlertStandardProcedure(s.element);
        EndThink();
        return true;
    }
    if (state == 10) {
        // the fight
        switch (type) {
        case STIM_TIMER:
            BattleDecisions();
            break;
        case STIM_REACHED:
        case STIM_FAILED:
            if (substate == SUB_CHASE) BattleDecisions();
            break;
        case STIM_VIEW:
            // someone in sight again while he searches: at him
            if (substate == SUB_CHASE || substate == SUB_SEARCH || substate == SUB_OVERVIEW) {
                target = s.element;
                Fight(true, true);
            }
            break;
        default:
            break;
        }
        EndThink();
        return true;
    }
    if (state == 3 && (substate == SUB_HELP || substate == SUB_NOISE) && type == STIM_TIMER) {
        if (substate == SUB_HELP) {
            // his friend's enemy is now his: the alarm, then the fight
            DVArtificialMalignity* f = alerter ? dynamic_cast<DVArtificialMalignity*>(AsActor(alerter)->ai.get()) : nullptr;
            if (f) {
                target = f->target;
                lastKnown = f->lastKnown;
            }
            attitude = 4;
            SetAlertStatus(2, false);
            searched = false;
            searchRounds = 0;
            GetBattleOverview(true);
        } else {
            // the noise: he goes to see (then looks around, then back to duty)
            SetState(3, SUB_WATCH);
            if (lastKnown.sector) GoTo(lastKnown, 0);
            lastSeen = Now();
            LaunchTimer(150);
        }
        EndThink();
        return true;
    }
    if (state == 3 || state == 10) {
        // watching / alarmed: while he still sees the hero he keeps watching; once he has lost
        // him for a while he goes back to his duty
        if (type == STIM_TIMER) {
            bool sees = false;
            for (uint8_t d : detections) sees |= d != 0;
            if (sees) {
                React(25, false);
            } else if (Now() - lastSeen > (state == 10 ? 250u : 100u)) {
                SetAlertStatus(0, false);
                suspect = 0;
                npc->SetViewCone(1);
                ReturnToDutyStandardProcedure();
            } else {
                if (state == 3) npc->GlanceAt(lastKnown, 0x1e);
                React(25, false);
            }
        }
        EndThink();
        return true;
    }
    if (state == AI_NORMAL) {
        switch (type) {
        case STIM_VIEW:
        case STIM_IDENTIFY:
        case STIM_HEAR:
        case 2:
        case 5:
        case 0x16:
        case 0x17:
        case 0x19:
            if (type != 5) break;
            [[fallthrough]];
        default:
            switch (substate) {
            case SUB_ON_ROUTE:
            case SUB_ON_ROUTE_RIDING:
                ArriveOnRoute(true);
                break;
            case SUB_TO_POST:
                if (type < 9 && ((0x170u >> type) & 1)) {
                    if (!atPost) FaceTo(npc->homeDir, false);
                    npc->SetViewCone(savedViewCone);
                    SetState(AI_NORMAL, SUB_AT_POST);
                    LaunchTimer((uint32_t)(desprand() % 0x46 + 0x1e));
                }
                break;
            case SUB_TO_POST_NEAR:
                SetState(AI_NORMAL, SUB_TO_POST);
                GoTo(npc->home, npc->horse ? 0x40 : 0x1200);
                break;
            case SUB_BACK_ON_ROUTE:
                SetState(AI_NORMAL, SUB_ON_ROUTE);
                if (path.Count() < 2 || !path.Current() || path.Current()->dataLen == 0) {
                    DVStimulus r = DVStimulus::Of(STIM_REACHED);
                    Think(r);
                } else {
                    // face along the route (from the previous waypoint), then go on
                    path.Back();
                    DVWaypoint* prev = path.Current();
                    path.Next();
                    if (prev) {
                        uint8_t dir = SBGetSector0to15(Position().p - prev->Position().p, 1.0f);
                        FaceTo(dir, false);
                    }
                    DVStimulus r = DVStimulus::Of(STIM_REACHED);
                    Think(r);
                }
                break;
            case SUB_BACK_ON_ROUTE_NEAR:
                SetState(AI_NORMAL, SUB_BACK_ON_ROUTE);
                if (DVWaypoint* w = path.Current()) GoTo(w->Position(), npc->horse ? 0x40 : 0x1200);
                break;
            case SUB_AT_POST:
            case SUB_AT_POST_2:
                if (type == STIM_TIMER || type == STIM_FAILED) {
                    // looks around now and then at its post
                    npc->SetViewCone(savedViewCone);
                    LaunchTimer((uint32_t)(desprand() % 0x46 + 0x1e));
                }
                break;
            case SUB_MACRO:
                if (type == STIM_TIMER) npc->SetViewCone(1);
                break;
            default:
                break;
            }
            break;
        }
    }
    EndThink();
    return true;
}

// ---- the civilians (DVArtificialBonhomie) ----------------------------------------------------------

int DVArtificialBonhomie::LoadAttributesFromFile(SBFile& f) {
    speech = f.U32();
    uint8_t a = f.U8() & 0x7f;
    switch (a) {
    case 0: attitude = 0; break;
    case 1: attitude = 1; break;
    case 2: attitude = 3; break;
    case 3: attitude = 4; break;
    }
    drunk = f.U8();
    return 6;
}

void DVArtificialBonhomie::SetState(uint32_t s, uint32_t sub) {
    state = s;
    substate = sub;
}

// DVArtificialBonhomie::InitOneAI
void DVArtificialBonhomie::InitOneAI() {
    npc->lastDirection = npc->sprite->pos.direction;
    npc->view.radius = npc->view.radiusNow = muwStandardViewPolygonRadius;
    npc->life = 100;
    bool init = npc->InitState(*this);
    stayOnPost = role == 2;
    npc->StoreInitialPositionParameters();
    if (path.Count() == 0) {
        onRoute = false;
        if (init) {
            LaunchTimer((uint32_t)(desprand() % 0x28 + 0x14));
            SetState(AI_NORMAL, SUB_AT_POST);
            timerSubstate = substate;
        }
    } else {
        onRoute = true;
        timerSubstate = substate;
        if (init) ReturnToDutyStandardProcedure();
    }
    savedViewCone = viewCone;
}

// DVArtificialBonhomie::The16thFrame
void DVArtificialBonhomie::The16thFrame(uint8_t frame) {
    if (npc->sprite->pos.posture == 0xe) return;
    if (npc->knockedOut) SetState(0, 2);
    if ((frame & 0x3f) != 0 || frame != 0) return;
    if (state == 0) return;
    if (npc->CurrentCommand() != 0 || timerOn || macroTimerOn) return;
    if (++boredTicks > 10 && !WaitingSubstate(substate)) {
        boredTicks = 0;
        ReturnToDutyStandardProcedure();
    }
}

// DVArtificialBonhomie::ReturnToDutyStandardProcedure
void DVArtificialBonhomie::ReturnToDutyStandardProcedure() {
    SetAlertStatus(0, false);
    npc->SetViewCone(1);
    if (!onRoute) {
        GoTo(npc->home, 0);
        SetState(AI_NORMAL, SUB_TO_POST);
        goesAway = true;
        return;
    }
    DVposition me = Position();
    size_t n = path.Count();
    float best = SBMaxNorm(path.waypoints && n ? (*path.waypoints)[0]->Position().p - me.p : SBGeoVector2D());
    uint16_t bi = 0;
    for (size_t i = 0; i < n; ++i) {
        const DVWaypoint* w = (*path.waypoints)[i].get();
        float d = SBMaxNorm(SBGeoPoint2D((float)w->x, (float)w->y) - me.p);
        if (d < best) {
            bi = (uint16_t)i;
            best = d;
        }
    }
    path.SetCurrentWaypointIndex(bi);
    DVWaypoint* w = path.Current();
    if (!w) return;
    SBGeoVector2D to = w->Position().p - me.p;
    if (SBMaxNorm(to) < 10.0f) {
        path.Next();
    } else {
        const DVWaypoint* nx = nullptr;
        if (path.Count() == 1) {
            nx = (*path.waypoints)[0].get();
        } else if (path.current != 0 && (path.current == path.Count() - 1 || !path.forward)) {
            nx = (*path.waypoints)[path.current - 1].get();
        } else {
            nx = (*path.waypoints)[path.current + 1].get();
        }
        if (SBDot(to, SBGeoPoint2D((float)nx->x, (float)nx->y) - w->Position().p) < 0.0f) path.Next();
    }
    if (DVWaypoint* t = path.Current()) GoTo(t->Position(), routeFlags | 0x40);
    SetState(AI_NORMAL, SUB_ON_ROUTE);
}

// DVArtificialBonhomie::EventViewStandardProcedure: a civilian sees a hero: most only glance at
// him; the suspicious ones (attitude 1) become wary (4), the wary ones tell (identify)
void DVArtificialBonhomie::EventViewStandardProcedure(DVElement* e) {
    if (!e || !e->sprite) return;
    switch (attitude) {
    case 0:
        npc->GlanceAt(Position(), 0x14);
        npc->view.target = e->sprite->pos.posMap;
        break;
    case 1:
        attitude = 4;
        break;
    case 3:
        npc->GlanceAt(Position(), 0x14);
        npc->view.target = e->sprite->pos.posMap;
        break;
    case 4: {
        SetAlertStatus(1, false);
        BreakMacro();
        DVEngine* eng = DVEngine::mpEngine;
        if (eng)
            for (size_t i = 0; i < eng->Heroes().size(); ++i)
                if (eng->Heroes()[i] == e) UpdateDetection((uint8_t)i, 2, e);
        break;
    }
    default:
        break;
    }
}

// DVArtificialBonhomie::EventIdentifyStandardProcedure: a wary civilian runs to tell a villain
// (RunToYourBigBrother comes with the alarms; for now he stays alarmed and watches)
void DVArtificialBonhomie::EventIdentifyStandardProcedure(DVElement* e) {
    if (attitude != 4) return;
    SetAlertStatus(1, false);
    BreakMacro();
    lastKnown = Position();
    npc->SetViewTarget(e, true);
}

// DVArtificialBonhomie::Think, the patrol part of state 2
bool DVArtificialBonhomie::Think(DVStimulus& s) {
    if (!StartThink(s)) {
        EndThink();
        return true;
    }
    if (!canMove) {
        EndThink();
        return true;
    }
    uint32_t type = s.type;
    if (npc->dead || state == 0) {
        EndThink();
        return true;
    }
    if (type == STIM_HEAR || type == STIM_BULLET) {
        // a shot: the townsfolk look at it (their panic and flight come later)
        DVposition p = Position();
        p.p = type == STIM_HEAR ? s.point : (s.element && s.element->sprite ? s.element->sprite->pos.posMap : p.p);
        npc->GlanceAt(p, 0x32);
        EndThink();
        return true;
    }
    if (state == AI_NORMAL && type == STIM_VIEW) {
        EventViewStandardProcedure(s.element);
        EndThink();
        return true;
    }
    if (state == AI_NORMAL && type == STIM_IDENTIFY) {
        EventIdentifyStandardProcedure(s.element);
        EndThink();
        return true;
    }
    if (state == AI_NORMAL) {
        switch (type) {
        case STIM_REACHED:
        case STIM_FAILED:
        case 7:
        case STIM_TIMER:
        case 9:
        case 0xd:
            switch (substate) {
            case SUB_ON_ROUTE:
                ArriveOnRoute(false);
                break;
            case SUB_TO_POST:
                FaceTo(npc->homeDir, false);
                npc->SetViewCone(savedViewCone);
                SetState(AI_NORMAL, SUB_AT_POST);
                LaunchTimer((uint32_t)(desprand() % 0x28 + 0x14));
                break;
            case SUB_AT_POST:
            case SUB_AT_POST_2:
                if (type == STIM_TIMER || type == STIM_FAILED) LaunchTimer((uint32_t)(desprand() % 0x28 + 0x14));
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
    EndThink();
    return true;
}

void DVArtificialIntelligenceTick() { ++gFrameCounter; }
