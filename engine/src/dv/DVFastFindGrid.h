// DVFastFindGrid (Game/DVFastFindGrid.cpp): the level's spatial index. The map is cut in 64x64
// cells per floor layer; each cell lists the masks (and later lines, sectors, obstacles...) over
// it. It also owns the level's sectors (motion areas) and sight obstacles.
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "sb/SBGeo.h"

class SBFile;
class DVMask;
class DVSector;
class DVSightObstacle;

class DVFastFindGrid {
public:
    static DVFastFindGrid* mpFastFindGrid;
    DVFastFindGrid();
    ~DVFastFindGrid();

    bool SizeMap(uint16_t cellsW, uint16_t cellsH);
    int LoadMotionObstaclesFromFile(SBFile& f);
    int LoadSightObstaclesFromFile(SBFile& f);
    int LoadMaskFromFile(SBFile& f);

    void AddMask(DVMask* m, uint16_t layer, bool keep);
    void AddSector(DVSector* s);
    // masks over the box that hide a character standing at `feet` on that layer
    void GetMasksAppliedToCharacter(std::vector<DVMask*>& out, uint16_t layer, const SBGeoBoundingBox2D& box,
                                    const SBGeoPoint2D& feet) const;

    DVSector* Sector(unsigned i) const { return i < sectors.size() ? sectors[i] : nullptr; }
    size_t SectorCount() const { return sectors.size(); }
    // the obstacle list of layer 0 (all sight obstacles, as loaded): masks and patches refer to it
    DVSightObstacle* SightObstacle(unsigned i) const;
    // the view areas (+0x190): index 0 = the default ground, then obstacles one can stand on
    DVSightObstacle* ViewArea(unsigned i) const { return i < viewAreas.size() ? viewAreas[i] : nullptr; }
    DVMask* LayerMask(unsigned layer, unsigned i) const;

    uint16_t gridW = 0, gridH = 0;    // +0x18, +0x1a
    SBGeoBoundingBox2D box;           // +4
    uint16_t layerCount = 0;          // +0x1c

private:
    struct Layer {
        std::vector<DVSightObstacle*> obstacles;  // +0x54
        std::vector<DVMask*> masks;               // +0x70
        std::vector<DVSector*> areas;
    };
    std::vector<Layer> layers;                     // +0x20
    std::vector<std::vector<DVMask*>> maskCells;   // per cell (layer, y, x)
    std::vector<DVSightObstacle*> viewAreas;       // +0x190
    std::vector<DVSector*> sectors;                // +0x1ac
    // ownership
    std::vector<std::unique_ptr<DVSector>> ownSectors;
    std::vector<std::unique_ptr<DVSightObstacle>> ownObstacles;
    std::vector<std::unique_ptr<DVMask>> ownMasks;
    void EnsureCells();
};
