#include "dv/DVFastFindGrid.h"

#include "dv/DVMask.h"
#include "dv/DVSector.h"

#include <algorithm>
#include <utility>
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
    if (lineCells.size() < want) lineCells.resize(want);
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
    moveLayers.assign(count, {});
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
            area->areaIndex = a;
            moveLayers[layer].push_back(DVmotionArea());
            DVmotionArea& ma = moveLayers[layer].back();
            ma.sector = area;
            int k = area->LoadFromFile(f);
            AddSector(area);
            AddSectorLines(area, layer, true);
            layers[layer].areas.push_back(area);
            uint16_t segs = f.U16();
            k = n + 2 + k;
            for (uint16_t s = 0; s < segs; ++s) {
                float x0 = f.S16(), y0 = f.S16(), x1 = f.S16(), y1 = f.S16();
                ma.segments.push_back(SBGeoSegment2D(SBGeoPoint2D(x0, y0), SBGeoPoint2D(x1, y1)));
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
                AddSectorLines(sec, layer, true);
            }
        }
    }
    return n;
}

// ---- lines ------------------------------------------------------------------------------------

static int CellClamp(float v, int n) {
    int16_t c = (int16_t)((int16_t)(int)v >> 6);
    if (c < 0) return 0;
    return c < n ? c : n - 1;
}

// DVFastFindGrid::AddLine (0x082ad4f0): the line goes in every cell its segment crosses
void DVFastFindGrid::AddLine(DVLine* l, uint16_t layer, bool keep) {
    EnsureCells();
    if (layer >= layers.size()) layers.resize(layer + 1);
    if (keep) layers[layer].lines.push_back(l);
    float xmin = l->seg.a.x < l->seg.b.x ? l->seg.a.x : l->seg.b.x;
    float xmax = l->seg.a.x < l->seg.b.x ? l->seg.b.x : l->seg.a.x;
    float ymin = l->seg.a.y < l->seg.b.y ? l->seg.a.y : l->seg.b.y;
    float ymax = l->seg.a.y < l->seg.b.y ? l->seg.b.y : l->seg.a.y;
    int x0 = CellClamp(xmin, gridW), y0 = CellClamp(ymin, gridH);
    int x1 = CellClamp(xmax, gridW), y1 = CellClamp(ymax, gridH);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            SBGeoPoint2D c((float)(x << 6), (float)(y << 6));
            SBGeoBoundingBox2D cell(c, c + SBGeoVector2D(64.0f, 64.0f));
            if (!SBIntersects(cell, l->seg)) continue;
            size_t idx = ((size_t)y + (size_t)layer * gridH) * gridW + x;
            if (idx >= lineCells.size()) lineCells.resize(idx + 1);
            lineCells[idx].push_back(l);
        }
}

// DVFastFindGrid::AddSectorLines (0x082ad1d0), the walkable areas' part: every edge of the
// sector's outline becomes a DVRepulsiveLine (0, 5) that blocks movement
void DVFastFindGrid::AddSectorLines(DVSector* s, uint16_t layer, bool keep) {
    size_t n = s->polygon.pts.size();
    if (n == 0) return;
    SBGeoPoint2D prev = s->polygon.pts[n - 1];
    for (size_t i = 0; i < n; ++i) {
        SBGeoPoint2D cur = s->polygon.pts[i];
        if (s->type & 2) {
            auto* l = new DVLine();
            ownLines.emplace_back(l);
            l->flags = LINE_MOTION | LINE_REPULSIVE | (s->type & 1);
            l->enabled = s->flag30;
            l->seg = SBGeoSegment2D(prev, cur);
            l->sectorId = s->id;
            l->sector = s;
            l->InitializeNormal();
            AddLine(l, layer, keep);
        }
        prev = cur;
    }
}

static void AddUnique(std::vector<DVLine*>& out, DVLine* l) {
    for (DVLine* o : out)
        if (o == l) return;
    out.push_back(l);
}

// DVFastFindGrid::GetLines (0x082b0d20 box, 0x082b1090 segment, with 0x082b0c30 per cell)
void DVFastFindGrid::GetLines(std::vector<DVLine*>& out, uint16_t layer, const SBGeoBoundingBox2D& b,
                              uint32_t type) const {
    size_t before = out.size();
    int x0 = CellClamp(b.p0.x, gridW), y0 = CellClamp(b.p0.y, gridH);
    int x1 = CellClamp(b.p1.x, gridW), y1 = CellClamp(b.p1.y, gridH);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            size_t idx = ((size_t)y + (size_t)layer * gridH) * gridW + x;
            if (idx >= lineCells.size()) continue;
            for (DVLine* l : lineCells[idx])
                if (l->enabled && (l->flags & type) == type) AddUnique(out, l);
        }
    size_t w = before;
    for (size_t i = before; i < out.size(); ++i)
        if (SBIntersects(b, out[i]->seg)) out[w++] = out[i];
    out.resize(w);
}

void DVFastFindGrid::GetLines(std::vector<DVLine*>& out, uint16_t layer, const SBGeoSegment2D& s,
                              uint32_t type) const {
    size_t before = out.size();
    SBGeoBoundingBox2D b;
    b.Expand(s.a);
    b.Expand(s.b);
    int x0 = CellClamp(b.p0.x, gridW), y0 = CellClamp(b.p0.y, gridH);
    int x1 = CellClamp(b.p1.x, gridW), y1 = CellClamp(b.p1.y, gridH);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            SBGeoPoint2D c((float)(x << 6), (float)(y << 6));
            SBGeoBoundingBox2D cell(c, c + SBGeoVector2D(64.0f, 64.0f));
            if (!SBIntersects(cell, s)) continue;
            size_t idx = ((size_t)y + (size_t)layer * gridH) * gridW + x;
            if (idx >= lineCells.size()) continue;
            for (DVLine* l : lineCells[idx])
                if (l->enabled && (l->flags & type) == type) AddUnique(out, l);
        }
    size_t w = before;
    for (size_t i = before; i < out.size(); ++i)
        if (SBIntersects(out[i]->seg, s)) out[w++] = out[i];
    out.resize(w);
}

// DVFastFindGrid::IsPositionAutorized (0x082ab440): no wall crosses the box
bool DVFastFindGrid::IsPositionAutorized(const SBGeoBoundingBox2D& b, uint16_t layer) const {
    if (!box.IsIntersecting(b)) return false;
    std::vector<DVLine*> lines;
    GetLines(lines, layer, b, LINE_MOTION);
    return lines.empty();
}

// pushes the box out of the walls it crosses, along their normals (the loop body shared by the
// two FindAutorizedPosition)
static void PushOut(SBGeoBoundingBox2D& b, const DVLine* l) {
    const SBGeoVector2D& n = l->normal;
    const SBGeoPoint2D& a = l->seg.a;
    float d = SBDot(n, a - b.p0);
    if (-0.1f < d) b = b + n * (d + 1.0f);
    d = SBDot(n, a - b.p1);
    if (-0.1f < d) b = b + n * (d + 1.0f);
    d = SBDot(n, a - SBGeoPoint2D(b.p1.x, b.p0.y));
    if (-0.1f < d) b = b + n * (d + 1.0f);
    d = SBDot(n, a - SBGeoPoint2D(b.p0.x, b.p1.y));
    if (-0.1f < d) b = b + n * (d + 1.0f);
}

// DVFastFindGrid::FindAutorizedPosition (0x082ab520): moves the box out of the walls (50 tries)
bool DVFastFindGrid::FindAutorizedPosition(SBGeoBoundingBox2D& b, uint16_t layer) const {
    if (!box.IsIntersecting(b)) {
        float dx = 0.0f;
        if (0.0f <= b.p1.x) {
            if (box.p1.x < b.p0.x) dx = -(b.p1.x - box.p1.x);
        } else {
            dx = -b.p0.x;
        }
        float dy = 0.0f;
        if (0.0f <= b.p1.y) {
            if (box.p1.y < b.p0.y) dy = -(b.p1.y - box.p1.y);
        } else {
            dy = -b.p0.y;
        }
        b = b + SBGeoVector2D(dx, dy);
    }
    for (int tries = 0; tries < 0x32; ++tries) {
        std::vector<DVLine*> lines;
        GetLines(lines, layer, b, LINE_MOTION);
        if (lines.empty()) return true;
        for (DVLine* l : lines)
            if (0.0f < SBDot(l->normal, SBCenter(b) - l->seg.a)) PushOut(b, l);
    }
    return false;
}

// DVFastFindGrid::FindAutorizedPosition (0x082bbc10): the same, also keeping the way from the
// box to `toward` free, and pushing only from the walls `toward` is in front of
bool DVFastFindGrid::FindAutorizedPosition(SBGeoBoundingBox2D& b, const SBGeoPoint2D& toward, uint16_t layer) const {
    for (int tries = 0; tries < 0x32; ++tries) {
        std::vector<DVLine*> lines;
        GetLines(lines, layer, SBGeoSegment2D(SBCenter(b), toward), LINE_MOTION);
        GetLines(lines, layer, b, LINE_MOTION);
        if (lines.empty()) return true;
        for (DVLine* l : lines)
            if (0.0f < SBDot(l->normal, toward - l->seg.a)) PushOut(b, l);
    }
    return false;
}

// the swept box of IsReachableThick / DVPathFinder::IsReachableGrid: the two long edges (A-B and
// C-D) of the shape a box of half size h covers moving from p to q, and its bounding box.
// Returns false when p == q.
bool DVSweptBox(SBGeoPoint2D p, SBGeoPoint2D q, float hx, float hy, SBGeoPoint2D& A, SBGeoPoint2D& B,
                SBGeoPoint2D& C, SBGeoPoint2D& D, SBGeoBoundingBox2D& bb) {
    SBGeoVector2D d = q - p;
    bb = SBGeoBoundingBox2D();
    if (d.x != 0.0f) {
        if (d.x < 0.0f) {
            std::swap(p, q);
            d = SBGeoVector2D(-d.x, -d.y);
        }
        if (0.0f < d.y) {
            D = SBGeoPoint2D(p.x - hx, p.y + hy);
            bb.Expand(SBGeoPoint2D(p.x - hx, p.y - hy));
            A = SBGeoPoint2D(p.x + hx, p.y - hy);
            B = SBGeoPoint2D(q.x + hx, q.y - hy);
            bb.Expand(SBGeoPoint2D(q.x + hx, q.y + hy));
            C = SBGeoPoint2D(q.x - hx, q.y + hy);
        }
        if (d.y < 0.0f) {
            D = SBGeoPoint2D(p.x + hx, p.y + hy);
            bb.Expand(SBGeoPoint2D(p.x - hx, p.y + hy));
            A = SBGeoPoint2D(p.x - hx, p.y - hy);
            B = SBGeoPoint2D(q.x - hx, q.y - hy);
            bb.Expand(SBGeoPoint2D(q.x + hx, q.y - hy));
            C = SBGeoPoint2D(q.x + hx, q.y + hy);
        }
        if (d.y == 0.0f) {
            D = SBGeoPoint2D(p.x - (hx + 1.0f), p.y + hy);
            A = SBGeoPoint2D(p.x - (hx + 1.0f), p.y - hy);
            bb.Expand(A);
            B = SBGeoPoint2D(q.x + hx - 1.0f, q.y - hy);
            C = SBGeoPoint2D(q.x + hx - 1.0f, q.y + hy);
            bb.Expand(C);
        }
        return true;
    }
    if (d.y <= 0.0f) {
        if (d.y == 0.0f) return false;
        A = SBGeoPoint2D(p.x - hx, p.y + hy);
        D = SBGeoPoint2D(p.x + hx, p.y + hy);
        bb.Expand(D);
        C = SBGeoPoint2D(q.x + hx, q.y - hy);
        B = SBGeoPoint2D(q.x - hx, q.y - hy);
        bb.Expand(B);
        return true;
    }
    D = SBGeoPoint2D(p.x - hx, p.y - hy);
    bb.Expand(D);
    A = SBGeoPoint2D(p.x + hx, p.y - hy);
    B = SBGeoPoint2D(q.x + hx, q.y + hy);
    bb.Expand(B);
    C = SBGeoPoint2D(q.x - hx, q.y + hy);
    return true;
}

// the common end of the two sweep tests: a line blocks if it crosses one of the long edges or
// starts inside the swept shape
bool DVSweptBoxBlocked(const std::vector<DVLine*>& lines, const SBGeoPoint2D& A, const SBGeoPoint2D& B,
                       const SBGeoPoint2D& C, const SBGeoPoint2D& D) {
    SBGeoSegment2D s1(A, B), s2(C, D);
    for (DVLine* l : lines)
        if (SBIntersects(l->seg, s1) || SBIntersects(l->seg, s2)) return true;
    SBGeoVector2D v1 = B - A, v2 = D - C, v3 = C - B, v4 = A - D;
    for (DVLine* l : lines) {
        const SBGeoPoint2D& p = l->seg.a;
        if (SBDet(v1, p - A) <= 0.0f) continue;
        if (SBDet(v2, p - C) <= 0.0f) continue;
        if (SBDet(v3, p - B) <= 0.0f) continue;
        if (0.0f < SBDet(v4, p - D)) return true;
    }
    return false;
}

// DVFastFindGrid::IsReachableThick (0x082bc2c0)
bool DVFastFindGrid::IsReachableThick(const SBGeoPoint2D& a, const SBGeoPoint2D& b, uint16_t layer,
                                      const SBGeoVector2D& half) const {
    if (a == b) return true;
    SBGeoPoint2D A, B, C, D;
    SBGeoBoundingBox2D bb;
    if (!DVSweptBox(a, b, half.x - 1.0f, half.y - 1.0f, A, B, C, D, bb)) return true;
    std::vector<DVLine*> lines;
    GetLines(lines, layer, bb, LINE_MOTION);
    if (lines.empty()) return true;
    return !DVSweptBoxBlocked(lines, A, B, C, D);
}

// the walkable area of the layer containing the point
DVSectorMotionArea* DVFastFindGrid::FindMotionArea(uint16_t layer, const SBGeoPoint2D& p) const {
    if (layer >= moveLayers.size()) return nullptr;
    for (const DVmotionArea& a : moveLayers[layer]) {
        if (!a.sector || !a.sector->box.IsInside_p(p) || !a.sector->polygon.IsInside_p(p)) continue;
        return a.sector;
    }
    return nullptr;
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
