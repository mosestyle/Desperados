#include "dv/DVFastFindGrid.h"

#include "dv/DVMask.h"
#include "dv/DVSector.h"
#include "sb/SBFile.h"

DVFastFindGrid* DVFastFindGrid::mpFastFindGrid = nullptr;

DVFastFindGrid::DVFastFindGrid() { mpFastFindGrid = this; }
DVFastFindGrid::~DVFastFindGrid() {
    if (mpFastFindGrid == this) mpFastFindGrid = nullptr;
}

// DVFastFindGrid::SizeMap (0x082a9da0)
bool DVFastFindGrid::SizeMap(uint16_t w, uint16_t h) {
    gridW = w;
    gridH = h;
    box.Set(SBGeoPoint2D(0, 0), SBGeoPoint2D((float)(int)(w * 0x40 - 1), (float)(int)(h * 0x40 - 1)));
    return true;
}

void DVFastFindGrid::EnsureCells() {
    size_t want = (size_t)gridW * gridH * (layerCount + 1);
    if (maskCells.size() < want) maskCells.resize(want);
    if (layers.size() < (size_t)layerCount + 1) layers.resize(layerCount + 1);
}

// DVFastFindGrid::AddSector (0x082ac1c0): the sector goes to the index of its number
void DVFastFindGrid::AddSector(DVSector* s) {
    if (s->id < 0) return;
    if ((size_t)s->id >= sectors.size()) sectors.resize((size_t)s->id + 1, nullptr);
    sectors[(size_t)s->id] = s;
}

DVSightObstacle* DVFastFindGrid::SightObstacle(unsigned i) const {
    if (layers.empty() || i >= layers[0].obstacles.size()) return nullptr;
    return layers[0].obstacles[i];
}

DVMask* DVFastFindGrid::LayerMask(unsigned layer, unsigned i) const {
    if (layer >= layers.size() || i >= layers[layer].masks.size()) return nullptr;
    return layers[layer].masks[i];
}

// DVFastFindGrid::LoadMotionObstaclesFromFile (0x082af3f0): the walkable areas of every floor
// layer, each with its blocked segments and inner sectors.
int DVFastFindGrid::LoadMotionObstaclesFromFile(SBFile& f) {
    f.version = f.U32();
    if (f.version != 1)
        SBError(true, "DVFastFindGrid.cpp", 0x733, "Version Check Failed on %s (file version %u, expected version %u).",
                "Hunk motion", f.version, 1);
    uint16_t count = f.U16();
    layerCount = count;
    EnsureCells();
    int n = 6;
    for (uint16_t layer = 0; layer < count; ++layer) {
        f.U16();
        uint16_t areas = f.U16();
        n += 4;
        for (uint16_t a = 0; a < areas; ++a) {
            auto* area = new DVSectorMotionArea();
            ownSectors.emplace_back(area);
            area->isLift = layer == (uint16_t)(layerCount - 1);
            area->type |= 0x40;
            area->layer = layer;
            int k = area->LoadFromFile(f);
            AddSector(area);
            layers[layer].areas.push_back(area);
            uint16_t segs = f.U16();
            k = n + 2 + k;
            for (uint16_t s = 0; s < segs; ++s) {
                f.S16();
                f.S16();
                f.S16();
                f.S16();
                k += 8;
            }
            uint16_t inner = f.U16();
            n = k + 2;
            for (uint16_t s = 0; s < inner; ++s) {
                auto* sec = new DVSector(SECTOR_PLANE);
                ownSectors.emplace_back(sec);
                sec->layer = layer;
                n += sec->LoadFromFile(f);
                AddSector(sec);
            }
        }
    }
    return n;
}

// DVFastFindGrid::LoadSightObstaclesFromFile (0x082b3d60)
int DVFastFindGrid::LoadSightObstaclesFromFile(SBFile& f) {
    f.version = f.U32();
    if (f.version != 6)
        SBError(true, "DVFastFindGrid.cpp", 0xf17, "Version Check Failed on %s (file version %u, expected version %u).",
                "Hunk sight obstacles", f.version, 6);
    EnsureCells();
    // view area 0: the ground, a flat plane everywhere
    auto* ground = new DVSightObstacle(4);
    ownObstacles.emplace_back(ground);
    ground->plane.SetPoints(SBGeoVector3D(0, 0, 0), SBGeoVector3D(10, 0, 0), SBGeoVector3D(0, 10, 0));
    ground->plane.InitializeAll();
    ground->planeGround = ground->plane;
    ground->box.Expand(SBGeoPoint2D(0, 0));
    ground->box.Expand(SBGeoPoint2D(2000, 2000));
    ground->boxScreen = ground->box;
    ground->motionLayer = 0;
    ground->motionArea = 0;
    ground->flags |= 4;
    viewAreas.push_back(ground);
    uint16_t count = f.U16();
    int n = 6;
    for (uint16_t i = 0; i < count; ++i) {
        auto* o = new DVSightObstacle();
        ownObstacles.emplace_back(o);
        n += o->LoadFromFile(f);
        if (o->flags & 4) {
            viewAreas.push_back(o);
            // the motion area it belongs to gets it in its list
            DVSector* area = Sector(o->motionArea);
            if (area && (area->type & 3) == 3) area->obstacles.push_back(o);
            else SBError(true, "DVFastFindGrid.cpp", 0xf40, "Illegal sight obstacle/motion area association !");
            // (CreateSectorPlane + AddSector: the obstacle's outline as a sector)
            auto* plane = new DVSector(SECTOR_PLANE);
            ownSectors.emplace_back(plane);
            plane->polygon = o->polygon;
            for (const SBGeoPoint2D& p : o->polygon.pts) plane->box.Expand(p);
            o->sectorPlane = plane;
            AddSector(plane);
        }
        layers[0].obstacles.push_back(o);
    }
    return n;
}

// DVFastFindGrid::LoadMaskFromFile (0x082c3260)
int DVFastFindGrid::LoadMaskFromFile(SBFile& f) {
    f.version = f.U32();
    if (f.version != 4)
        SBError(true, "DVFastFindGrid.cpp", 0x21fd, "Version Check Failed on %s (file version %u, expected version %u).",
                "Hunk Mkpo", f.version, 4);
    EnsureCells();
    uint16_t layerN = f.U16();
    int n = 6;
    for (uint16_t layer = 0; layer < layerN; ++layer) {
        uint16_t count = f.U16();
        n += 2;
        for (uint16_t i = 0; i < count; ++i) {
            auto* m = new DVMask();
            ownMasks.emplace_back(m);
            n += m->LoadFromFile(f, this);
            AddMask(m, layer, true);
            if (m->type & 2) AddMask(m, layerCount, false);
        }
    }
    return n;
}

// DVFastFindGrid::AddMask (0x082b1a30)
void DVFastFindGrid::AddMask(DVMask* m, uint16_t layer, bool keep) {
    EnsureCells();
    if (layer >= layers.size()) layers.resize(layer + 1);
    if (keep) layers[layer].masks.push_back(m);
    auto cell = [](float v, int n) -> int {
        int16_t c = (int16_t)((int16_t)(int)v >> 6);
        if (c < 0) return 0;
        return c < n ? c : n - 1;
    };
    int x0 = cell(m->box.p0.x, gridW), y0 = cell(m->box.p0.y, gridH);
    int x1 = cell(m->box.p1.x, gridW), y1 = cell(m->box.p1.y, gridH);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            size_t idx = ((size_t)y + (size_t)layer * gridH) * gridW + x;
            if (idx >= maskCells.size()) maskCells.resize(idx + 1);
            maskCells[idx].push_back(m);
        }
}

// DVFastFindGrid::GetMasksAppliedToCharacter (0x082b2100) with GetMasks (0x082b1ca0, type 1)
void DVFastFindGrid::GetMasksAppliedToCharacter(std::vector<DVMask*>& out, uint16_t layer,
                                                const SBGeoBoundingBox2D& b, const SBGeoPoint2D& feet) const {
    size_t before = out.size();
    auto cell = [](float v, int n) -> int {
        int16_t c = (int16_t)((int16_t)(int)v >> 6);
        if (c < 0) return 0;
        return c < n ? c : n - 1;
    };
    int x0 = cell(b.p0.x, gridW), y0 = cell(b.p0.y, gridH);
    int x1 = cell(b.p1.x, gridW), y1 = cell(b.p1.y, gridH);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            size_t idx = ((size_t)y + (size_t)layer * gridH) * gridW + x;
            if (idx >= maskCells.size()) continue;
            for (DVMask* m : maskCells[idx]) {
                if (!m->active || (m->type & 1) != 1) continue;
                bool have = false;
                for (DVMask* o : out)
                    if (o == m) {
                        have = true;
                        break;
                    }
                if (!have) out.push_back(m);
            }
        }
    // keep the ones in front of the character
    size_t w = before;
    for (size_t i = before; i < out.size(); ++i) {
        DVMask* m = out[i];
        if (m->box.IsIntersecting(b) && m->IsAppliedToPointCharacter(feet)) out[w++] = m;
    }
    out.resize(w);
}
