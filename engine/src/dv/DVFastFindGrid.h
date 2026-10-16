// DVFastFindGrid (Game/DVFastFindGrid.cpp): the level's spatial index. The map is cut in 64x64
// cells per floor layer; each cell lists the masks (and later lines, sectors, obstacles...) over
// it. It also owns the level's sectors (motion areas) and sight obstacles.
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "dv/DVLine.h"
#include "sb/SBGeoPlane.h"

#include "sb/SBGeo.h"

class SBFile;
class DVMask;
class DVSector;
class DVSightObstacle;
class DVSectorMotionArea;
class DVSectorBuilding;
class DVDoor;
class DVElementActor;
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

    // ---- sight (the obstacles' cells: +0x188, per cell +0x18 count / +0x1c list) -------------------
    // the obstacles of the cells under the box with all the flags of `type` (1 sight, 2 shots)
    void GetObstacles(std::vector<DVSightObstacle*>& out, uint16_t layer, const SBGeoBoundingBox2D& box,
                      uint32_t type) const;
    // DVFastFindGrid::IsReachable(SBGeoPoint3D, SBGeoPoint3D, bool) (0x082b4c40): can one see from
    // a to b (x, y on the map, z the height)? `shot`: for projectiles (obstacles of type 2)
    bool IsReachable(const SBGeoPoint3D& a, const SBGeoPoint3D& b, bool shot) const;
    // the same with the obstacles already picked (GetObstacles over a box holding the segment)
    bool IsReachable(const SBGeoPoint3D& a, const SBGeoPoint3D& b, const std::vector<DVSightObstacle*>& obs) const;
    // the active masks of `layer` over the box that cut characters (GetMasks, type 1)
    void GetMasks(std::vector<DVMask*>& out, uint16_t layer, const SBGeoBoundingBox2D& box) const;

    // the walkable areas of each layer, for the path finder (DVPathFinder::mlpListMoveLayers)
    const std::vector<std::vector<DVmotionArea>>& MoveLayers() const { return moveLayers; }
    // the walkable area of `layer` under the point (nullptr if none)
    DVSectorMotionArea* FindMotionArea(uint16_t layer, const SBGeoPoint2D& p) const;

    // ---- doors, lifts and buildings (Game/DVDoor.cpp) -------------------------------------------
    // LIFT (DVFastFindGrid::LoadLiftsFromFile, 0x082afed0): the last layer's areas are stairs,
    // ladders and walls, each with a door at its bottom and one at its top
    int LoadLiftsFromFile(SBFile& f);
    // BUIL (0x082b0010): the houses (their doors and who is inside) and the other doors (gates)
    int LoadBuildingsFromFile(SBFile& f, const std::vector<DVElementActor*>& actors);
    // GetSectorScreen (0x082acda0): what is under a point of the map, from the top layer down: a
    // walkable area (or a lift), or a door to a house. Returns false for nothing walkable.
    bool GetSectorScreen(const SBGeoPoint2D& p, DVSectorMotionArea*& area, uint16_t& layer, DVDoor*& door) const;
    // FindPathDoors (0x082c0cb0): the doors to go through from one area to another, with the
    // way each is passed (true = from its first side to its second); false if there is no way
    bool FindPathDoors(const DVElementActor* who, const SBGeoPoint2D& from, DVSectorMotionArea* fromArea,
                       const SBGeoPoint2D& to, DVSectorMotionArea* toArea,
                       std::vector<std::pair<DVDoor*, bool>>& out) const;
    const std::vector<std::unique_ptr<DVDoor>>& Doors() const { return doors; }
    const std::vector<DVSectorBuilding*>& Buildings() const { return buildings; }
    DVDoor* NearestDoor(const SBGeoPoint2D& p, float maxDist) const;
    void AddLayerSector(DVSector* s, uint16_t layer);
    void AdoptSector(DVSector* s);  // the grid owns it, and lists it by its number
    // ---- the lit zones of the night missions (MAT hunk, material 8: DVSectorShadow, 0x4000) ------
    struct Light {
        SBGeoPoint2D center;  // +0x58
        SBGeoPoint3D point;   // +0x60: the light, in 3D
        float radius = 0;     // +0x54
        uint16_t layer = 0;   // +0x50
    };
    int LoadMaterialSectorsFromFile(SBFile& f);  // 0x082afb40
    const std::vector<Light>& Lights() const { return lights; }
    // the ground's material under a point (the footsteps' sound): 0 if none
    uint8_t MaterialAt(const SBGeoPoint2D& p, uint16_t layer) const;
    struct Material {
        SBGeoPolygon2DArray polygon;
        SBGeoBoundingBox2D box;
        uint8_t material = 0;
        int32_t layer = 0;
    };
    std::vector<Material> materials;
    // ---- patches (PAT, DVEngine::LoadPatchFromFile 0x0826b810; +0x1e4) -----------------------
    int LoadPatchesFromFile(SBFile& f);
    class DVPatch* Patch(unsigned i) const { return i < patches.size() ? patches[i].get() : nullptr; }
    const std::vector<std::unique_ptr<class DVPatch>>& Patches() const { return patches; }

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
    std::vector<std::vector<DVSightObstacle*>> obstacleCells;  // per cell of layer 0
    std::vector<std::vector<DVmotionArea>> moveLayers;
    std::vector<std::unique_ptr<DVLine>> ownLines;
    std::vector<DVSightObstacle*> viewAreas;       // +0x190
    std::vector<DVSector*> sectors;                // +0x1ac
    std::vector<std::vector<DVSector*>> layerSectors;  // what can be clicked on, per layer
    std::vector<std::unique_ptr<DVDoor>> doors;    // +0x1c8
    std::vector<DVSectorBuilding*> buildings;      // +0x200
    // ownership
    std::vector<std::unique_ptr<DVSector>> ownSectors;
    std::vector<std::unique_ptr<DVSightObstacle>> ownObstacles;
    std::vector<std::unique_ptr<DVMask>> ownMasks;
    std::vector<std::unique_ptr<class DVPatch>> patches;
    std::vector<Light> lights;
    void EnsureCells();
};

// the swept box shared by DVFastFindGrid::IsReachableThick and DVPathFinder::IsReachableGrid
bool DVSweptBox(SBGeoPoint2D p, SBGeoPoint2D q, float hx, float hy, SBGeoPoint2D& A, SBGeoPoint2D& B,
                SBGeoPoint2D& C, SBGeoPoint2D& D, SBGeoBoundingBox2D& bb);
bool DVSweptBoxBlocked(const std::vector<DVLine*>& lines, const SBGeoPoint2D& A, const SBGeoPoint2D& B,
                       const SBGeoPoint2D& C, const SBGeoPoint2D& D);
