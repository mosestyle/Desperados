#include "dv/DVPathFinder.h"

#include "dv/DVFastFindGrid.h"
#include "dv/DVLine.h"
#include "dv/DVSector.h"
#include "sb/SBFile.h"

#include <cstdlib>

DVPathFinder::DVPathFinder() {}
DVPathFinder::~DVPathFinder() {}

size_t DVPathFinder::NodeCount() const {
    size_t n = 0;
    for (auto& l : layers)
        for (auto& a : l)
            for (auto& li : a) n += li.size();
    return n;
}

// DVPathFinder::LoadGraphFromFile (0x08332290)
int DVPathFinder::LoadGraphFromFile(SBFile& f) {
    int n = 0;
    uint16_t nd = f.U16();
    n += 2;
    for (uint16_t i = 0; i < nd; ++i) {
        float x = f.F32(), y = f.F32();
        halfDiagonals.push_back(SBGeoVector2D(x, y));
        n += 8;
    }
    // the nodes, by layer / area / list; each keeps the indices of its links until they're read
    std::vector<std::vector<uint16_t>> nodeLinks;
    std::vector<DVpathGraphNode*> allNodes;
    uint16_t nl = f.U16();
    n += 2;
    layers.assign(nl, {});
    for (uint16_t l = 0; l < nl; ++l) {
        uint16_t na = f.U16();
        n += 2;
        layers[l].assign(na, {});
        for (uint16_t a = 0; a < na; ++a) {
            uint16_t nlists = f.U16();
            n += 2;
            layers[l][a].assign(nlists, {});
            for (uint16_t li = 0; li < nlists; ++li) {
                uint16_t nn = f.U16();
                n += 2;
                ownNodes.emplace_back(new DVpathGraphNode[nn ? nn : 1]);
                DVpathGraphNode* arr = ownNodes.back().get();
                for (uint16_t k = 0; k < nn; ++k) {
                    DVpathGraphNode& node = arr[k];
                    uint16_t nd2 = f.U16();
                    for (uint16_t d = 0; d < nd2; ++d) node.docks.push_back(f.U8());
                    n += 2 + nd2;
                    float px = f.S16(), py = f.S16();
                    node.p = SBGeoPoint2D(px, py);
                    float ax = f.S16(), ay = f.S16();
                    node.v74 = SBGeoVector2D(ax, ay);
                    float bx = f.S16(), by = f.S16();
                    node.v6c = SBGeoVector2D(bx, by);
                    uint16_t nlk = f.U16();
                    n += 14;
                    std::vector<uint16_t> idx;
                    for (uint16_t j = 0; j < nlk; ++j) idx.push_back(f.U16());
                    n += 2 * nlk;
                    nodeLinks.push_back(idx);
                    allNodes.push_back(&node);
                    layers[l][a][li].push_back(&node);
                }
            }
        }
    }
    auto nodeAt = [&](uint16_t l, uint16_t a, uint16_t li, uint16_t k) -> DVpathGraphNode* {
        if (l >= layers.size() || a >= layers[l].size() || li >= layers[l][a].size() || k >= layers[l][a][li].size())
            return nullptr;
        return layers[l][a][li][k];
    };
    // the links
    uint16_t nlinks = f.U16();
    n += 2;
    links.assign(nlinks, {});
    std::vector<std::vector<uint16_t>> linkConfs(nlinks);
    for (uint16_t i = 0; i < nlinks; ++i) {
        uint16_t l = f.U16(), a = f.U16(), li = f.U16(), k = f.U16();
        links[i].a = nodeAt(l, a, li, k);
        l = f.U16(), a = f.U16(), li = f.U16(), k = f.U16();
        links[i].b = nodeAt(l, a, li, k);
        links[i].cost = f.F32();
        uint16_t nc = f.U16();
        n += 0x16;
        for (uint16_t j = 0; j < nc; ++j) linkConfs[i].push_back(f.U16());
        n += 2 * nc;
    }
    // the link configurations (0xff = none)
    uint16_t nconf = f.U16();
    n += 2;
    configs.assign(nconf, {});
    std::vector<bool> none(nconf, false);
    for (uint16_t i = 0; i < nconf; ++i) {
        uint8_t x = f.U8();
        configs[i].x = x;
        if (x == 0xff) {
            none[i] = true;
            n += 1;
            continue;
        }
        configs[i].y = f.U8();
        uint16_t c1 = f.U16();
        for (uint16_t j = 0; j < c1; ++j) configs[i].list1.push_back(f.U8());
        uint16_t c2 = f.U16();
        for (uint16_t j = 0; j < c2; ++j) configs[i].list2.push_back(f.U8());
        n += 6 + c1 + c2;
    }
    for (uint16_t i = 0; i < nlinks; ++i)
        for (uint16_t c : linkConfs[i]) links[i].confs.push_back(c < nconf && !none[c] ? &configs[c] : nullptr);
    for (size_t i = 0; i < allNodes.size(); ++i)
        for (uint16_t k : nodeLinks[i])
            if (k < links.size()) allNodes[i]->links.push_back(&links[k]);
    return n;
}

// DVPathFinder::ResetGraph (0x0832fae0)
void DVPathFinder::ResetGraph() {
    open.clear();
    shortest = 2e10f;
    if (!curArea) return;
    for (auto& list : *curArea)
        for (DVpathGraphNode* n : list) {
            n->visited = false;
            n->g = 1e10f;
            n->h = 1e10f;
            n->f = 2e10f;
            n->parent = nullptr;
        }
}

// DVPathFinder::DockingPoint (0x0832c910)
SBGeoPoint2D DVPathFinder::DockingPoint(const DVpathGraphNode* n, uint8_t place) const {
    switch (place) {
    case 1: return n->p - hd;
    case 2: return SBGeoPoint2D(n->p.x + hd.x, n->p.y - hd.y);
    case 4: return n->p + hd;
    case 8: return SBGeoPoint2D(n->p.x - hd.x, n->p.y + hd.y);
    }
    return SBGeoPoint2D();
}

// DVPathFinder::IsGoodDockingPlace (0x0832c620): the point lies in the quarter the docking place
// faces, on one side or the other
bool DVPathFinder::IsGoodDockingPlace(const SBGeoPoint2D& p, const DVpathGraphNode* n, uint8_t place, bool side) const {
    SBGeoVector2D v = p - DockingPoint(n, place);
    SBGeoVector2D a, b;
    if (side) {
        switch (place) {
        case 1: a = {-1, 0}; b = {0, 1}; break;
        case 2: a = {0, -1}; b = {-1, 0}; break;
        case 4: a = {1, 0}; b = {0, -1}; break;
        case 8: a = {0, 1}; b = {1, 0}; break;
        }
        return SBDet(a, v) < 0.0f && 0.0f <= SBDet(b, v);
    }
    switch (place) {
    case 1: a = {1, 0}; b = {0, -1}; break;
    case 2: a = {0, 1}; b = {1, 0}; break;
    case 4: a = {-1, 0}; b = {0, 1}; break;
    case 8: a = {0, -1}; b = {-1, 0}; break;
    }
    return SBDet(a, v) <= 0.0f && 0.0f < SBDet(b, v);
}

// DVPathFinder::IsUsefulLink (0x0832d430): one corner of the box at `src` is outside the corner's
// two edges
bool DVPathFinder::IsUsefulLink(const SBGeoPoint2D& src, const DVpathGraphNode* n) const {
    const SBGeoVector2D corners[4] = {SBGeoVector2D(-hd.x, -hd.y), SBGeoVector2D(hd.x, -hd.y), SBGeoVector2D(hd.x, hd.y),
                                      SBGeoVector2D(-hd.x, hd.y)};
    for (const SBGeoVector2D& c : corners) {
        SBGeoVector2D v = (src + c) - n->p;
        if (0.0f < SBDet(n->v6c, v)) return true;
        if (0.0f < SBDet(n->v74, v)) return true;
    }
    return false;
}

// DVPathFinder::IsReachableFast (0x0832d670): the segment crosses none of the area's thin barriers
bool DVPathFinder::IsReachableFast(const SBGeoPoint2D& a, const SBGeoPoint2D& b) const {
    if (!curMotionArea) return true;
    SBGeoSegment2D s(a, b);
    for (const SBGeoSegment2D& t : curMotionArea->segments)
        if (SBIntersects(s, t)) return false;
    return true;
}

// DVPathFinder::IsReachableGrid (0x0832d790): the character's box can slide from a to b. The walls
// are taken from the grid cells the two long edges of the swept box cross.
bool DVPathFinder::IsReachableGrid(SBGeoPoint2D a, SBGeoPoint2D b) const {
    SBGeoPoint2D A, B, C, D;
    SBGeoBoundingBox2D bb;
    if (!DVSweptBox(a, b, hd.x - 1.0f, hd.y - 1.0f, A, B, C, D, bb)) return true;
    std::vector<DVLine*> lines;
    SBGeoSegment2D s1(A, B), s2(C, D);
    // (the original converts with a 16-bit wrap: a negative start skips the scan)
    auto cellOf = [](float v) -> unsigned { return (unsigned)(int)(v * 0.015625f) & 0xffff; };
    unsigned y0 = cellOf(bb.p0.y), y1 = cellOf(bb.p1.y), x0 = cellOf(bb.p0.x), x1 = cellOf(bb.p1.x);
    for (unsigned y = y0; y <= y1; ++y)
        for (unsigned x = x0; x <= x1; ++x) {
            if (x >= grid->gridW || y >= grid->gridH) continue;
            SBGeoPoint2D c((float)(x << 6), (float)(y << 6));
            SBGeoBoundingBox2D cell(c, c + SBGeoVector2D(64.0f, 64.0f));
            if (!SBIntersects(cell, s1) && !SBIntersects(cell, s2)) continue;
            size_t idx = ((size_t)y + (size_t)curLayer * grid->gridH) * grid->gridW + x;
            const std::vector<DVLine*>* cl = grid->CellLines(idx);
            if (!cl) continue;
            for (DVLine* l : *cl) {
                if (!(l->flags & LINE_MOTION) || !l->enabled) continue;
                bool have = false;
                for (DVLine* o : lines)
                    if (o == l) {
                        have = true;
                        break;
                    }
                if (!have) lines.push_back(l);
            }
        }
    if (lines.empty()) return true;
    return !DVSweptBoxBlocked(lines, A, B, C, D);
}

// DVPathFinder::ObjectPositionAutorized (0x0832fc60): the box at p is inside the grid and touches
// no wall
bool DVPathFinder::ObjectPositionAutorized(const SBGeoPoint2D& p) const {
    SBGeoVector2D h(hd.x - 1.0f, hd.y - 1.0f);
    SBGeoBoundingBox2D b;
    b.Expand(p + h);
    b.Expand(p - h);
    int x0 = (int16_t)(int)b.p0.x >> 6, y0 = (int16_t)(int)b.p0.y >> 6;
    int x1 = (int16_t)(int)b.p1.x >> 6, y1 = (int16_t)(int)b.p1.y >> 6;
    if (x0 < 0 || y0 < 0 || x1 >= grid->gridW || y1 >= grid->gridH) return false;
    std::vector<DVLine*> lines;
    grid->GetLines(lines, curLayer, b, LINE_MOTION);
    return lines.empty();
}

// DVPathFinder::AddToListOpenNodes (0x0832ef80): sorted by f; a node met again before its place
// isn't added twice
void DVPathFinder::AddToListOpenNodes(DVpathGraphNode* n) {
    if (open.empty()) {
        open.push_back(n);
        return;
    }
    size_t i = 0;
    DVpathGraphNode* cur = open[0];
    do {
        if (n->f <= cur->f) {
            open.insert(open.begin() + (long)i, n);
            return;
        }
        ++i;
        if (i >= open.size()) {
            open.push_back(n);
            return;
        }
        cur = open[i];
    } while (cur != n);
}

// DVPathFinder::LinkSource (0x0832c9f0): the corners near the way that the start sees become the
// first open nodes
void DVPathFinder::LinkSource(const SBGeoPoint2D& start, const SBGeoPoint2D& end) {
    SBGeoVector2D d = end - start;
    SBGeoVector2D m(200.0f, 200.0f);
    SBGeoBoundingBox2D box;
    if (d.x <= 0.0f) {
        if (d.y <= 0.0f) box.Set(end - m, start + m);
        else box.Set(SBGeoPoint2D(end.x, start.y) - m, SBGeoPoint2D(start.x, end.y) + m);
    } else {
        if (0.0f < d.y) box.Set(start - m, end + m);
        else box.Set(SBGeoPoint2D(start.x, end.y) - m, SBGeoPoint2D(end.x, start.y) + m);
    }
    for (auto& list : *curArea)
        for (DVpathGraphNode* n : list) {
            uint8_t docks = curSize < n->docks.size() ? n->docks[curSize] : 0;
            if (!docks || !box.IsInside_p(n->p) || !IsUsefulLink(start, n) || !IsReachableFast(start, n->p)) continue;
            uint8_t found = 0;
            for (uint8_t bit = 1; bit < 0x10; bit = (uint8_t)(bit << 1)) {
                if (!(docks & bit)) continue;
                if (!IsGoodDockingPlace(start, n, bit, true) && !IsGoodDockingPlace(start, n, bit, false)) continue;
                if (IsReachableGrid(start, DockingPoint(n, bit))) found |= bit;
            }
            if (!found) continue;
            n->startDocks = found;
            n->g = SBNorm(n->p - start);
            n->h = SBNorm(end - n->p);
            n->f = n->h + n->g;
            n->parent = nullptr;
            n->visited = true;
            AddToListOpenNodes(n);
        }
}

// DVPathFinder::FindPathNodes (0x083301b0): A* over the corners until one sees the goal
DVpathGraphNode* DVPathFinder::FindPathNodes(const SBGeoPoint2D& end) {
    DVpathGraphNode* best = nullptr;
    int attempts = 1;  // muwNumberOfAttempts
    while (!open.empty()) {
        DVpathGraphNode* n = open.front();
        open.erase(open.begin());
        uint8_t docks = curSize < n->docks.size() ? n->docks[curSize] : 0;
        if (IsReachableFast(end, n->p)) {
            uint8_t found = 0;
            for (uint8_t bit = 1; bit < 0x10; bit = (uint8_t)(bit << 1)) {
                if (!(docks & bit)) continue;
                bool good = IsGoodDockingPlace(end, n, bit, true) || IsGoodDockingPlace(end, n, bit, false);
                if (!good || docks == 5 || docks == 10) continue;
                if (IsReachableGrid(end, DockingPoint(n, bit))) found |= bit;
            }
            if (found) {
                if (n->f < shortest) {
                    shortest = n->f;
                    n->endDocks = found;
                    --attempts;
                    best = n;
                }
                if (attempts == 0) return best;
            }
        }
        for (DVpathGraphLink* l : n->links) {
            DVpathGraphNode* m = l->a;
            const DVpathLinkConfig* c = curSize < l->confs.size() ? l->confs[curSize] : nullptr;
            if (!m || !c || docks == 5 || docks == 10) continue;
            float g = n->g + l->cost;
            if (!(g < m->g) || !(g < shortest)) continue;
            m->parent = l;
            m->g = g;
            if (!m->visited) m->h = SBNorm(end - m->p);
            m->f = g + m->h;
            m->visited = true;
            AddToListOpenNodes(m);
        }
    }
    return best;
}

static uint8_t NextCW(uint8_t d) { return d == 8 ? 1 : (uint8_t)(d * 2); }
static uint8_t NextCCW(uint8_t d) { return d == 1 ? 8 : (uint8_t)(d >> 1); }
static bool SinglePlace(uint8_t v) { return v != 0 && v < 0x10 && ((0x116u >> v) & 1); }

// the walk round a corner shared by PassAroundNode (0x083306f0) and PassAroundLastNode
// (0x08331160): from the docking places we arrive at (`flags`) to one of the `wanted` ones, going
// round the corner through its free docking places, the shortest way. Adds the points (in front
// of the path) and returns the docking place reached.
uint8_t DVPathFinder::PassAround(const DVpathGraphNode* n, uint8_t flags, uint8_t wanted, std::vector<SBGeoPoint2D>& path) const {
    if (flags == 0) return 0;
    uint8_t docks = curSize < n->docks.size() ? n->docks[curSize] : 0;
    auto add = [&](uint8_t place) { path.insert(path.begin(), DockingPoint(n, place)); };
    uint8_t common = flags & wanted;
    if (SinglePlace(common)) {
        add(common);
        return common;
    }
    uint8_t start = 0, target = 0;
    bool cwDir = false, found = false;
    if (SinglePlace(flags)) {
        bool cwOn = true, ccwOn = true;
        uint8_t cw = flags, ccw = flags;
        for (int guard = 0; guard < 16 && !found && (cwOn || ccwOn); ++guard) {
            if (cwOn) {
                cw = NextCW(cw);
                if (!(docks & cw)) cwOn = false;
                else if (wanted & cw) {
                    found = true;
                    cwDir = true;
                    target = cw;
                }
            }
            if (ccwOn) {
                ccw = NextCCW(ccw);
                if (docks & ccw) {
                    if (wanted & ccw) {
                        found = true;
                        cwDir = false;
                        target = ccw;
                    }
                } else {
                    ccwOn = false;
                }
            }
        }
        start = flags;
    } else {
        int best = 5;
        uint8_t bit = 1;
        int count = 0;
        for (uint8_t v = flags; v; v &= (uint8_t)(v - 1)) ++count;
        for (int k = 0; k < count; ++k) {
            uint8_t s;
            do {
                s = bit;
                bit = (uint8_t)(bit * 2);
            } while (!(s & flags));
            bool cwOn = true, ccwOn = true, done = false;
            uint8_t cw = s, ccw = s;
            int ncw = 0, nccw = 0;
            for (int guard = 0; guard < 16 && !done && (cwOn || ccwOn); ++guard) {
                if (cwOn) {
                    cw = NextCW(cw);
                    if (!(docks & cw)) cwOn = false;
                    else if (!(wanted & cw)) ++ncw;
                    else {
                        done = true;
                        if (ncw < best) {
                            best = ncw;
                            start = s;
                            target = cw;
                            cwDir = true;
                            found = true;
                        }
                    }
                }
                if (ccwOn) {
                    ccw = NextCCW(ccw);
                    if (docks & ccw) {
                        if (!(wanted & ccw)) ++nccw;
                        else {
                            done = true;
                            if (nccw < best) {
                                best = nccw;
                                start = s;
                                target = ccw;
                                cwDir = false;
                                found = true;
                            }
                        }
                    } else {
                        ccwOn = false;
                    }
                }
            }
        }
    }
    if (!found) {
        // (the original would loop forever here; never seen in the game's data)
        uint8_t first = flags & (uint8_t)(-flags);
        add(first);
        return first;
    }
    for (uint8_t d = start; d != target; d = cwDir ? NextCW(d) : NextCCW(d)) add(d);
    add(target);
    return target;
}

// DVPathFinder::PassAroundNode (0x083306f0): round the link's node a, then the docking places
// this leads to at node b
uint8_t DVPathFinder::PassAroundNode(const DVpathGraphLink* l, uint8_t flags, std::vector<SBGeoPoint2D>& path) const {
    if (flags == 0) return 0;
    const DVpathLinkConfig* c = curSize < l->confs.size() ? l->confs[curSize] : nullptr;
    if (!c) return 0;
    uint8_t target = PassAround(l->a, flags, c->y, path);
    uint8_t out = 0;
    for (size_t i = 0; i < c->list2.size() && i < c->list1.size(); ++i)
        if (c->list2[i] == target) out |= c->list1[i];
    return out;
}

// DVPathFinder::FindPath (0x0832f320)
bool DVPathFinder::FindPath(uint16_t layer, uint16_t areaIndex, uint16_t sizeIndex, const SBGeoPoint2D& start,
                            const SBGeoPoint2D& end, bool startAdjusted, std::vector<SBGeoPoint2D>& path) {
    path.clear();
    if (!grid || layer >= layers.size() || areaIndex >= layers[layer].size()) return false;
    const auto& ml = grid->MoveLayers();
    if (layer >= ml.size() || areaIndex >= ml[layer].size()) return false;
    curLayer = layer;
    curArea = &layers[layer][areaIndex];
    curMotionArea = &ml[layer][areaIndex];
    curSize = sizeIndex;
    hd = sizeIndex < halfDiagonals.size() ? halfDiagonals[sizeIndex] : SBGeoVector2D(11, 6);
    ResetGraph();
    static bool dbg = getenv("DVPFDEBUG") != nullptr;
    if (!ObjectPositionAutorized(end)) {
        if (dbg) SBLog("pf: end %g,%g not authorized", end.x, end.y);
        return false;
    }
    if (startAdjusted && IsReachableFast(start, end) && IsReachableGrid(start, end)) {
        path.push_back(end);
        return true;
    }
    LinkSource(start, end);
    if (dbg) {
        size_t nn = 0;
        for (auto& li : *curArea) nn += li.size();
        SBLog("pf: area has %zu nodes in %zu lists, %zu linked from the start, %zu barriers", nn, curArea->size(), open.size(),
              curMotionArea->segments.size());
    }
    DVpathGraphNode* n = FindPathNodes(end);
    if (!n) {
        if (dbg) SBLog("pf: no corner sees the end");
        return false;
    }
    path.insert(path.begin(), end);
    uint8_t flags = n->endDocks;
    while (DVpathGraphLink* l = n->parent) {
        flags = PassAroundNode(l, flags, path);
        n = l->b;
        if (!n) break;
    }
    if (n) PassAround(n, flags, n->startDocks, path);
    path.insert(path.begin(), start);
    // drop the points the character can skip
    size_t size = path.size();
    if ((size & 0xfffc) > 3) {
        SBGeoPoint2D p0 = path[0], p1 = path[1];
        for (size_t i = 2; i < size; ++i) {
            SBGeoPoint2D p2 = path[i];
            SBGeoVector2D v = (p2 - p0) * 5e-05f;
            if (IsReachableGrid(p0 + v, p2 - v)) {
                path.erase(path.begin() + (long)(i - 1));
                --size;
                --i;
            } else {
                p0 = p1;
            }
            p1 = p2;
        }
    }
    return true;
}
