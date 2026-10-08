// DVEngine (Game/DVEngine.cpp): one mission. Loads the level file (.dvd: hunks MISC, BGND, MOVE,
// SGHT, MASK, ELEM, ...), keeps the elements sorted by display order, steps them at 25 Hz
// (PerformHourglass) and draws the visible part of the map (Draw).
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "sb/SBGeo.h"

class SBFile;
class SBDrawManager;
class DVElement;
class DVFastFindGrid;
class DVFrameHolder;

class DVEngine {
public:
    static DVEngine* mpEngine;
    explicit DVEngine(SBDrawManager* draw);
    ~DVEngine();

    // levelName = "level_01": reads Data\Levels\level_01.dvd. Returns false if it can't.
    bool LoadStateFromFile(const std::string& levelName);
    uint32_t StateGet(int state) const;

    // one 25 Hz step of the mission
    void PerformHourglass();
    // draws the map at the camera onto the screen surface
    void Draw();

    // the camera: top-left of the view on the map, in map pixels (DVEngine +0xb4c)
    SBGeoPoint2D camera;
    SBGeoVector2D screen;      // +0xbb0 size of the screen surface
    float mapW = 0, mapH = 0;  // +0xbb8, +0xbbc
    float zoom = 1.0f;         // +0xbc0
    void ClampCamera();
    bool silhouettes = false;  // the "show silhouettes" key (DVrawInput +0x15)

    // MISC
    bool night = false;            // +0x1000
    uint8_t nightPercent = 100;    // +0x1001
    uint16_t shadowKey = 0x1f;     // +0x1002 (the level's shadow colour)
    uint8_t weather = 0;           // +0x1004

    std::string levelName;
    std::string backgroundName;
    const std::vector<DVElement*>& Elements() const { return elements; }
    DVFastFindGrid* Grid() const { return grid.get(); }
    uint32_t BackgroundSurface() const { return background; }
    uint32_t MinimapSurface() const { return minimap; }
    int ticks = 0;

private:
    SBDrawManager* draw;
    std::unique_ptr<DVFrameHolder> frames;  // +0x168
    std::unique_ptr<DVFastFindGrid> grid;   // +0x44c
    std::vector<std::unique_ptr<DVElement>> ownElements;
    std::vector<DVElement*> elements;       // +0x84
    uint32_t background = 0;   // +0xba4
    uint32_t minimap = 0;      // +0xba8

    int LoadMiscFromFile(SBFile& f);
    int LoadBackgroundFromFile(SBFile& f);
    int LoadElemFromFile(SBFile& f);
    void SortForEngine();
    void PerformRefreshAllElements(uint32_t surface);
};
