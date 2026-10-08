// DVPathFinder (Game/DVPathFinder.cpp): the original's path finder. The level file holds a graph
// per walkable area (in the MOVE hunk, after the areas): its nodes are the corners of the
// obstacles, each with up to four "docking places" (the corners of a character's box touching the
// obstacle corner: 1 top-left, 2 top-right, 4 bottom-right, 8 bottom-left) usable by each
// character size, and its links say from which docking place to which a character can go from one
// corner to the next. A search links the start to the nearby corners it can see, runs A* over the
// corners until one sees the goal, builds the polyline by going round each corner through its
// docking places, then drops the points the character can skip. The original runs it in a thread;
// here it runs when asked (the result is the same, a little sooner).
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "sb/SBGeo.h"

class SBFile;
class DVFastFindGrid;
struct DVmotionArea;

struct DVpathGraphLink;

// one link configuration (0x4c bytes): docking places at the far node (x) and this node (y),
// and the pairs (list2 at this node -> list1 at the next one)
struct DVpathLinkConfig {
    uint8_t x = 0, y = 0;            // +0, +1
    std::vector<uint8_t> list1;      // +4
    std::vector<uint8_t> list2;      // +0x28
};

struct DVpathGraphNode {             // 0x98 bytes
    SBGeoPoint2D p;                  // +0
    bool visited = false;            // +8
    float g = 1e10f, h = 1e10f, f = 2e10f;  // +0xc, +0x10, +0x14
    DVpathGraphLink* parent = nullptr;     // +0x18
    uint8_t endDocks = 0;            // +0x1c: docking places that see the goal
    uint8_t startDocks = 0;          // +0x1d: docking places the start sees
    std::vector<DVpathGraphLink*> links;   // +0x20 (links whose node b is this one)
    SBGeoVector2D v6c, v74;          // +0x6c, +0x74: the obstacle's two edges at this corner
    std::vector<uint8_t> docks;      // +0x7c: docking places per character size
};

struct DVpathGraphLink {             // 0x34 bytes
    DVpathGraphNode* a = nullptr;    // +0
    DVpathGraphNode* b = nullptr;    // +4
    float cost = 0;                  // +8
    std::vector<DVpathLinkConfig*> confs;  // +0x10, per character size (nullptr = can't)
};

class DVPathFinder {
public:
    DVPathFinder();
    ~DVPathFinder();

    // reads the graph that follows the walkable areas in the MOVE hunk (0x08332290)
    int LoadGraphFromFile(SBFile& f);
    void SetObstacles(DVFastFindGrid* grid) { this->grid = grid; }

    // DVPathFinder::FindPath (0x0832f320). `startAdjusted` is the request's +0x34 flag (the start
    // had to be moved out of a wall): then a straight line is tried first. On success the path
    // runs from start to end (or is just {end}).
    bool FindPath(uint16_t layer, uint16_t areaIndex, uint16_t sizeIndex, const SBGeoPoint2D& start,
                  const SBGeoPoint2D& end, bool startAdjusted, std::vector<SBGeoPoint2D>& path);

    const std::vector<SBGeoVector2D>& HalfDiagonals() const { return halfDiagonals; }
    size_t NodeCount() const;
    // the corners of an area (debug view)
    std::vector<const DVpathGraphNode*> AreaNodes(uint16_t layer, uint16_t area) const {
        std::vector<const DVpathGraphNode*> r;
        if (layer < layers.size() && area < layers[layer].size())
            for (auto& li : layers[layer][area])
                for (auto* n : li) r.push_back(n);
        return r;
    }

private:
    DVFastFindGrid* grid = nullptr;
    std::vector<SBGeoVector2D> halfDiagonals;  // mListHalfDiagonals
    // mListPathGraphLayers: layer -> area -> lists of nodes (the outline's, then each hole's)
    std::vector<std::vector<std::vector<std::vector<DVpathGraphNode*>>>> layers;
    std::vector<std::unique_ptr<DVpathGraphNode[]>> ownNodes;
    std::vector<DVpathGraphLink> links;        // pAllLinks
    std::vector<DVpathLinkConfig> configs;     // pAllLinkConfigurations

    // the current search (the original's statics)
    uint16_t curLayer = 0, curSize = 0;
    const std::vector<std::vector<DVpathGraphNode*>>* curArea = nullptr;
    const DVmotionArea* curMotionArea = nullptr;
    SBGeoVector2D hd;                          // mCurrentHalfDiagonal
    std::vector<DVpathGraphNode*> open;        // mListOpenNodes, sorted by f
    float shortest = 2e10f;                    // mfShortestDistanceFound

    void ResetGraph();
    bool ObjectPositionAutorized(const SBGeoPoint2D& p) const;
    bool IsReachableFast(const SBGeoPoint2D& a, const SBGeoPoint2D& b) const;
    bool IsReachableGrid(SBGeoPoint2D a, SBGeoPoint2D b) const;
    bool IsUsefulLink(const SBGeoPoint2D& src, const DVpathGraphNode* n) const;
    bool IsGoodDockingPlace(const SBGeoPoint2D& p, const DVpathGraphNode* n, uint8_t place, bool side) const;
    SBGeoPoint2D DockingPoint(const DVpathGraphNode* n, uint8_t place) const;
    void LinkSource(const SBGeoPoint2D& start, const SBGeoPoint2D& end);
    DVpathGraphNode* FindPathNodes(const SBGeoPoint2D& end);
    void AddToListOpenNodes(DVpathGraphNode* n);
    uint8_t PassAround(const DVpathGraphNode* n, uint8_t flags, uint8_t wanted, std::vector<SBGeoPoint2D>& path) const;
    uint8_t PassAroundNode(const DVpathGraphLink* l, uint8_t flags, std::vector<SBGeoPoint2D>& path) const;
};
