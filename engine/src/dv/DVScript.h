// DVScript (Game/DVScript.cpp, VMCoreCustom): the mission's scripts. Data\Levels\level_XX.scb holds
// the compiled classes: StartUp (the mission), one for each scripted element, waypoint and zone. At
// the start every scripted element gets its instance and its Initialize, then StartUp's
// Initialize and Briefing (DVEngine::LoadStateFromFile). While the mission runs the engine calls
// StartUp's HourGlass every second and CheckVictoryCondition every three (DVEngine::
// PerformHourglass), the NPCs' FilterEvent sees every stimulus of their brains, the zones get
// EnterZone / ExitZone, the waypoints ReachPoint.
//
// The 200 natives (VMCoreCustom::I...) act on the engine. The actors are handed to the scripts as
// handles (their index in the level's element list + 1, the original's DVScript::marrayElement;
// 0 = none); the locations (the SCRP hunk's points, lines and zones, DVFastFindGrid +0x21c, and the
// points made by IGetActorLocation) as kLocation + their index.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "dv/VMCore.h"
#include "sb/SBGeo.h"

class DVElement;
class DVElementActor;
class DVEngine;
class DVSector;
class DVSequence;
struct DVWaypoint;
class SBFile;

// a location of the SCRP hunk (DVObjectScript: DVPointScript, DVLineScript, DVSectorScript)
struct DVObjectScript {
    int type = 0;                  // +4 0 point, 1 line, 2 zone (sector)
    SBGeoPoint2D p;                // point: +4 / line: its first end
    SBGeoPoint2D p2;               // line: its other end
    uint16_t layer = 0;            // +0x20 / zone +0xe0
    DVSector* area = nullptr;      // point: +0x24 the walkable area it is on
    uint16_t areaId = 0;           // zone: +0xe2
    uint8_t u14 = 0;
    std::unique_ptr<DVSector> zone;    // zone: the polygon
    bool hasScript = false;        // zone: +0x50
    std::string scriptName;
    std::unique_ptr<VMInstance> inst;  // zone: +0x5c IZoneScript
    std::vector<DVElementActor*> occupants;  // zone: +0xe4
};

class DVScript {
public:
    static DVScript* mpInstance;
    static constexpr int32_t kLocation = 0x40000000;
    DVScript();
    ~DVScript();

    // the SCRP hunk (DVFastFindGrid::LoadScriptObjectsFromFile, 0x082c2f90)
    int LoadScriptObjectsFromFile(SBFile& f);
    // the level's scripts: loads them, gives the elements, waypoints and zones their instance and
    // runs the Initialize functions, then StartUp's Initialize and Briefing
    bool Initialize(DVEngine* engine, const std::vector<DVElement*>& elements, const std::string& levelName);

    // ---- what the engine calls ------------------------------------------------------------------
    void HourGlass(uint32_t seconds);              // IEngineScript::HourGlass
    int CheckVictoryCondition(uint32_t seconds);   // 1 = won, 2 = lost
    // IActorNPCScript::FilterEvent: the NPC's script sees an event; false = it drops it
    bool FilterEvent(DVElementActor* npc, DVElement* source, int event);
    // IWaypointScript::ReachPoint
    void ReachPoint(DVWaypoint* w, DVElementActor* npc);
    // the zones: who is in which (DVElementActor::CheckForLineCrossing, DVSectorScript::Enter /
    // Leave, InitializeScriptSectorOccupants)
    void CheckZones(DVElementActor* a, bool silent);
    void ForgetZones(DVElementActor* a);
    // IElementTargetScript::Shooted / Hit / Clicked ... for an object
    void TargetEvent(DVElement* e, const char* function, DVElement* by);

    static int32_t Handle(const DVElement* e);
    static DVElement* Element(int32_t h);
    DVObjectScript* Location(int32_t h) const;
    int32_t AddPoint(const SBGeoPoint2D& p, uint16_t layer, DVSector* area);
    static DVElement* scriptThis;  // DVScript::pScriptThis (IThis)

    VMCore vm;
    VMCodeManager code;
    std::vector<DVElement*> marrayElement;
    std::vector<std::unique_ptr<DVObjectScript>> locations;
    std::unique_ptr<VMInstance> startUp;
    std::map<const DVElement*, std::unique_ptr<VMInstance>> instances;
    std::map<std::string, std::unique_ptr<VMInstance>> waypointInstances;
    bool loaded = false;
    // while the elements' Initialize run they are still being loaded: the scripts see as many
    // elements as were loaded before this one (IGetActor(IGetNumberOfActorsInEngine()) = itself)
    int32_t loadingCount = -1;

    // ---- the sequence being recorded (IStart ... IThanx) -------------------------------------------
    DVSequence* mpSequence = nullptr;    // 0x085dd7e8
    uint16_t muwSequenceLevel = 0;       // 0x085dd7f0
    bool StartSequenceRecording();       // 0x0834a170
    bool EndSequenceRecording();         // 0x0834a260

private:
    VMInstance* InstanceOf(const DVElement* e) const;
    int32_t CallFor(VMInstance* inst, DVElement* self, const char* fn, const std::vector<int32_t>& params);
};
