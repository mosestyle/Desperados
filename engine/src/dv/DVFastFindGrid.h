// DVFastFindGrid (Game/DVFastFindGrid.cpp): the level's spatial index. The map is cut in 64x64
// cells per floor layer; each cell lists the masks (and later lines, sectors, obstacles...) over
// it. It also owns the level's sectors (motion areas) and sight obstacles.
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "dv/DVLine.h"

#include "sb/SBGeo.h"

class SBFile;
class DVMask;
class DVSector;
class DVSightObstacle;
class DVSectorMotionArea;
struct DVLine;

// a walkable area as the path finder sees it (DVmotionArea): its sector and its thin barriers
struct DVmotionArea {
    DVSectorMotionArea* sector = nullptr;
    std::vector<SBGeoSegment2D> segments;
};

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

    // ---- the lines (walls of the walkable areas) ---------------------------------------------
    void AddLine(DVLine* l, uint16_t layer, bool keep);
    void AddSectorLines(DVSector* s, uint16_t layer, bool keep);
    // lines of `layer` with all the bits of `type` that cross the box / the segment (unique)
    void GetLines(std::vector<DVLine*>& out, uint16_t layer, const SBGeoBoundingBox2D& box, uint32_t type) const;
    void GetLines(std::vector<DVLine*>& out, uint16_t layer, const SBGeoSegment2D& seg, uint32_t type) const;
    // the lines listed in one cell (index = (layer * gridH + y) * gridW + x)
    const std::vector<DVLine*>* CellLines(size_t idx) const { return idx < lineCells.size() ? &lineCells[idx] : nullptr; }
    bool IsPositionAutorized(const SBGeoBoundingBox2D& box, uint16_t layer) const;
    bool FindAutorizedPosition(SBGeoBoundingBox2D& box, uint16_t layer) const;
    bool FindAutorizedPosition(SBGeoBoundingBox2D& box, const SBGeoPoint2D& toward, uint16_t layer) const;
    // can a box of half size `half` slide from a to b without touching a wall?
    bool IsReachableThick(const SBGeoPoint2D& a, const SBGeoPoint2D& b, uint16_t layer, const SBGeoVector2D& half) const;

    // the walkable areas of each layer, for the path finder (DVPathFinder::mlpListMoveLayers)
    const std::vector<std::vector<DVmotionArea>>& MoveLayers() const { return moveLayers; }
    // the walkable area of `layer` under the point (nullptr if none)
    DVSectorMotionArea* FindMotionArea(uint16_t layer, const SBGeoPoint2D& p) const;

    uint16_t gridW = 0, gridH = 0;    // +0x18, +0x1a
    SBGeoBoundingBox2D box;           // +4
    uint16_t layerCount = 0;          // +0x1c

private:
    struct Layer {
        std::vector<DVSightObstacle*> obstacles;  // +0x54
        std::vector<DVMask*> masks;               // +0x70
        std::vector<DVSector*> areas;
        std::vector<DVLine*> lines;               // +0x1c
    };
    std::vector<Layer> layers;                     // +0x20
    std::vector<std::vector<DVMask*>> maskCells;   // per cell (layer, y, x)
    std::vector<std::vector<DVLine*>> lineCells;   // +0x188 per cell (layer, y, x)
    std::vector<std::vector<DVmotionArea>> moveLayers;
    std::vector<std::unique_ptr<DVLine>> ownLines;
    std::vector<DVSightObstacle*> viewAreas;       // +0x190
    std::vector<DVSector*> sectors;                // +0x1ac
    // ownership
    std::vector<std::unique_ptr<DVSector>> ownSectors;
    std::vector<std::unique_ptr<DVSightObstacle>> ownObstacles;
    std::vector<std::unique_ptr<DVMask>> ownMasks;
    void EnsureCells();
};

// the swept box shared by DVFastFindGrid::IsReachableThick and DVPathFinder::IsReachableGrid
bool DVSweptBox(SBGeoPoint2D p, SBGeoPoint2D q, float hx, float hy, SBGeoPoint2D& A, SBGeoPoint2D& B,
                SBGeoPoint2D& C, SBGeoPoint2D& D, SBGeoBoundingBox2D& bb);
bool DVSweptBoxBlocked(const std::vector<DVLine*>& lines, const SBGeoPoint2D& A, const SBGeoPoint2D& B,
                       const SBGeoPoint2D& C, const SBGeoPoint2D& D);
