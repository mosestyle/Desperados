// The NPCs' walking routes (Game/DVHikingGuide.cpp, DVPath.cpp, DVWaypoint.cpp): the WAYS hunk
// holds the routes, each a list of waypoints. A waypoint is a place (point, sector, layer) and
// what to do there: either the name of a script, or "macro" data: one or two option lists (one per
// walking direction), each option a probability and a string of macro commands (wait, face a
// direction, glance at a point, jump to another waypoint, ...), run by
// DVArtificialIntelligence::ExecuteNextMacroCommand.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "sb/SBGeo.h"

class SBFile;
class DVSector;

// DVposition (16 bytes): a point on a floor
struct DVposition {
    SBGeoPoint2D p;
    DVSector* sector = nullptr;  // +8
    int16_t layer = 0;           // +0xc
};

struct DVWaypoint {
    int16_t x = 0, y = 0;        // +0x84
    DVSector* sector = nullptr;  // +0x88
    uint16_t layer = 0;          // +0x8c
    bool isScript = false;       // +0x8e
    uint16_t dataLen = 0;        // +0x90
    std::vector<uint8_t> data;   // +0x94 (macro data)
    std::string script;          // +0x98
    DVposition Position() const;
    int LoadDataFromFile(SBFile& f);   // 0x08329e10
};

using DVWaypointList = std::vector<std::unique_ptr<DVWaypoint>>;

class DVHikingGuide {
public:
    static DVHikingGuide* mpInstance;
    DVHikingGuide();
    ~DVHikingGuide();
    int LoadAllPathesFromFile(SBFile& f);  // 0x082ea390
    int LoadNewPathFromFile(SBFile& f);    // 0x082ea260
    const DVWaypointList* Path(uint16_t i) const { return i < paths.size() ? paths[i].get() : nullptr; }
    size_t Count() const { return paths.size(); }

private:
    std::vector<std::unique_ptr<DVWaypointList>> paths;  // +4
};

// DVPath (in the NPC, +0xd8): where the NPC is on its route
struct DVPath {
    uint8_t current = 0;     // +0
    uint8_t previous = 0;    // +1
    bool forward = true;     // +2
    const DVWaypointList* waypoints = nullptr;  // +4
    uint16_t index = 0xffff;                    // +8

    int LoadFromFile(SBFile& f);           // 0x083294e0
    void SetCurrentWaypointIndex(uint16_t i) { previous = current; current = (uint8_t)i; }  // 0x083293d0
    void Next();                            // operator++ (0x083293e0)
    void Back();                            // operator-- (0x08329440)
    bool HasRightDirection(int flag) const; // 0x083294b0
    size_t Count() const { return waypoints ? waypoints->size() : 0; }
    DVWaypoint* Current() const {
        return waypoints && current < waypoints->size() ? (*waypoints)[current].get() : nullptr;
    }
};
