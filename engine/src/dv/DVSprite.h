// DVSprite (Game/DVSprite.cpp): something drawn from a .dvf sprite file: its animations ("rows":
// one per animation and direction), the current row and frame, where it is (DVPositionInterface),
// and the drawing of its current frame into its element's surface, with the masks of the
// background that stand in front of it (CreateTargetSprite / DrawMaskOverSprite).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "dv/DVPositionInterface.h"
#include "sb/SBGeo.h"

class SBFile;
class DVFrameHolder;
class DVFastFindGrid;
class DVMask;
class SBDrawManager;
struct SBDrawViewport;

// one animation row of a sprite (DVspriteScript, 0xa0 bytes)
struct DVspriteScript {
    uint16_t keyEntry = 0;              // +0
    float averageStep = 0;              // +4
    SBGeoPoint2D anchor;                // +8
    std::vector<uint32_t> frames;       // +0x10 bank indices
    std::vector<int16_t> durations;     // +0x2c in 25 Hz ticks (0 = 1)
    std::vector<uint16_t> steps;        // +0x48 pixels moved
    std::vector<SBGeoVector2D> offsets; // +0x64 frame corner in the anchor box
    std::vector<uint16_t> sounds;       // +0x84
    uint16_t animId = 0;
    uint16_t dirIndex = 0;
};

// how to draw (DVspriteRenderingInfo)
struct DVspriteRenderingInfo {
    uint32_t surface = 0;      // +0
    uint32_t flags = 0;        // +8: 1 = next frame, 2 = wipe the shadow, 4 = outline, 8 = all in one colour
    uint32_t maskKind = 0;     // +0xc: 0 = character, 1 = projectile
    bool applyMasks = false;   // +0x10
    bool hidden = false;       // +5: masks as "hidden" (silhouette)
    uint16_t outlineColor = 0; // +0x12
    uint16_t shadowColor = 0;  // +0x14
};

enum DVframeKind { FRAME_ACTOR = 0, FRAME_FX = 1, FRAME_OBJECT = 2 };

class DVSprite {
public:
    DVSprite();

    int LoadSpriteFromFile(SBFile& f, DVframeKind kind);
    int LoadFrameInfoFromFile(SBFile& f, DVframeKind kind);
    int LoadPositionInfoFromFile(SBFile& f, DVframeKind kind);

    void ComputeDisplayOrder(class DVElement* other, bool before);
    bool IsOnScreen(const SBGeoBoundingBox2D& view, float zoom);
    bool GenerateBlitBox(const SBGeoBoundingBox2D& view, float zoom, SBGeoBoundingBox2D& src, SBGeoBoundingBox2D& dst);
    bool CreateTargetSprite(const DVspriteRenderingInfo& info);
    void DrawMaskOverSprite(const DVMask& m, SBDrawViewport& vp, const SBGeoPoint2D& corner, uint16_t outline);
    void DrawMaskOverSpriteHidden(const DVMask& m, SBDrawViewport& vp, const SBGeoPoint2D& corner);

    // the current row / frame
    const std::vector<DVspriteScript>& Rows() const { return useAlt ? rowsAlt : rows; }
    const DVspriteScript* CurrentRow() const;
    uint32_t CurrentFrameId(unsigned next = 0) const;
    // animation stepping (one call per 25 Hz tick); returns true at the end of the row
    bool IncrementFrame(int progression);
    int PerformVirginIncrement(int progression);
    void ResetSpriteFrame(bool toEnd);
    bool IsEmptyFrame() const;
    uint16_t GetCurrentSoundID() const;

    // ---- actions and motions (one call per 25 Hz tick). Results: 1 = started, 2 = going on,
    // 0 = the row's action frame reached, 3 = finished, 4 = error.
    bool HasAnimation(uint32_t anim) const { return anim < animRows.size() && animRows[anim] != -1; }
    bool MaybeInitializeFrame(uint32_t anim, int progression);
    void InitializeActionDone(uint32_t anim);
    // orderId 0 = no order
    int PerformAction(uint32_t orderId, uint32_t anim, int progression, bool restart);
    // walks toward `goal` (a new orderId starts a new leg)
    int PerformMotion(uint32_t orderId, const SBGeoPoint2D& goal, float tolerance, uint32_t anim, int method,
                      int progression, bool restart);
    float GetDistanceForAnimation(uint32_t anim) const;

    DVPositionInterface pos;          // +4
    uint16_t row = 0;                 // +0x14c (low half of the u32)
    uint16_t entry = 0;               // +0x14e
    uint16_t tick = 0xffff;           // +0x150 ticks spent on the current frame
    uint32_t orderId = 0xffffffff;    // +0x148 the order being played
    uint16_t doneEntry = 0, doneTick = 0;  // +0x154, +0x156 the row's action frame
    std::vector<int16_t> animRows;    // +0x15c anim id -> first row (185 entries)
    std::vector<int16_t> animRowsAlt; // +0x178
    std::vector<DVspriteScript> rows;     // +0x194
    std::vector<DVspriteScript> rowsAlt;  // +0x1b0
    std::string fileName;             // +0x1cc
    std::string fileNameAlt;          // +0x1e4
    bool drawn = false;               // +0x1fc
    bool masked = false;              // +0x1fd
    bool useAlt = false;              // +0x1fe
    bool visible = true;              // +0x1ff (objects)
    uint16_t objectValue = 0;         // +0x200
    uint16_t width = 0, height = 0;   // +0x202, +0x204 surface / frame size
    SBGeoBoundingBox2D boxOnMap;      // +0x208
    uint32_t lastEdgeFrame = 0xffffffff;  // +0x220
    float displayOrder = 0;           // +0x22c
    int32_t displayBias = 0;          // +0x230
    uint32_t animation = 0xb9;        // +0x240
    uint16_t profileWidth = 0, profileHeight = 0;

    DVFrameHolder* frameHolder = nullptr;  // +0x234
    DVFastFindGrid* grid = nullptr;        // +0x238
    SBDrawManager* draw = nullptr;         // +0x23c

    // keeps the last drawn picture: drawing again is skipped when nothing changed
    uint64_t lastTargetKey = ~0ull;
};
