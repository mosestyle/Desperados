// The level's elements (Game/DVElement*.cpp): heroes, enemies, civilians, animals, objects,
// animated scenery (DVElementFX) and script targets. Each is drawn from its DVSprite into its own
// surface (with the masks in front of it), then onto the screen (Refresh); Hourglass is its 25 Hz
// update. This step has their loading and drawing; their behaviour comes with the next steps.
#pragma once
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "dv/DVSprite.h"
#include "sb/SBGeo.h"

class SBFile;
class DVPatch;

// the element kinds in the ELEM hunk (DVEngine::LoadElemFromFile)
enum : uint16_t {
    KIND_COOPER = 1, KIND_DOC = 2, KIND_SAM = 3, KIND_KATE = 4, KIND_SANCHEZ = 5, KIND_MIA = 6, KIND_MRLEONE = 7,
    KIND_VILLAIN = 0x101, KIND_CIVILIAN = 0x102, KIND_HORSE = 0x201,
    KIND_ANIMAL1 = 0x210,  // .. 0x215
    KIND_OBJECT = 0x301, KIND_TARGET = 0x800, KIND_FX = 0x1001,
};

class DVElement {
public:
    DVElement();
    virtual ~DVElement();
    virtual int LoadStateFromFile(SBFile& f) = 0;
    // draws the element on `surface` (the screen); view = the visible map rectangle
    virtual void Refresh(uint32_t surface, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes);
    virtual void Hourglass() {}
    const char* KindName() const;

    uint32_t creation = 0;          // +4
    uint16_t kind = 0;              // +8
    uint32_t classFlags = 0;        // +0xc
    bool active = true;             // +0x10
    std::unique_ptr<DVSprite> sprite;  // +0x50
    uint32_t surface = 0xffffffff;  // +0x54
    uint32_t surface2 = 0xffffffff; // +0x58
    bool shadows = true;            // +0x86
    uint16_t shadowKey = 0;         // +0x88
    uint16_t colorSelect = 0;       // +0x8a
    uint16_t colorFocus = 0;        // +0x8c
    uint16_t colorOutline = 0;      // +0x8e
    std::string scriptName;

protected:
    void MakeSurfaces(int extraW2, int extraH2, bool second);
    // the common drawing of a character / object: frame + masks into the surface, then onto the screen
    void DrawSprite(uint32_t screen, float zoom, const SBGeoBoundingBox2D& view, bool masks, bool hiddenMasks,
                    bool shadowBlit);
};

// one step of what a character does (DVOrder): a command (most are the animation they play) and
// for motions the point to reach
struct DVOrder {
    uint32_t id = 0;            // +0x34 (DVOrder::mulNextID)
    uint32_t command = 0xba;    // +0x24
    SBGeoPoint2D goal;          // +0x14
    float tolerance = 0;        // +4
    static uint32_t NewID();
};

// the commands used so far (DVcommand / DVanimation numbers of the original)
enum : uint32_t {
    CMD_WAIT = 0,        // stand ("Attendre")
    CMD_WALK = 3,        // "Marcher"
    CMD_RUN = 5,         // "Courir"
    CMD_LIE_DOWN = 6,    // "Se baisser"
    CMD_LYING = 7,       // "Couché"
    CMD_CRAWL = 9,       // "Ramper"
    CMD_STAND_UP = 10,   // "Se lever"
    CMD_NONE = 0xbb,
};

// postures (DVposture)
enum : uint32_t { POSTURE_STANDING = 0, POSTURE_LYING = 2, POSTURE_GETTING_UP = 10 };

class DVElementActor : public DVElement {
public:
    int LoadStateFromFile(SBFile& f) override;
    void Refresh(uint32_t surface, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) override;
    void Hourglass() override;

    bool IsHero() const { return kind >= KIND_COOPER && kind <= KIND_MIA; }
    // the player's orders (DVEngine::PerformGroupMove + PerformMove + DVElementActor::Translate)
    bool MoveTo(const SBGeoPoint2D& dest, bool run);
    void MakeRunning();
    void MakeCrawling();   // lie down
    void MakeWalking();    // stand up
    void Stop();
    bool IsInMotion() const { return !orders.empty(); }
    uint32_t Posture() const { return sprite->pos.posture; }
    // the path of the current move (for the debug view)
    std::vector<SBGeoPoint2D> PlannedPath() const;

    bool selected = false;
    int selectionPulse = 0;     // +0x188 ticks left of the selection flash
    int selectionPulseLen = 0;  // +0x18c

    std::deque<DVOrder> orders;     // the current sequence element's orders
    uint32_t defaultCommand = CMD_NONE;  // +0xcc
    uint32_t actionState = 0;            // +0xc4
    bool newOrder = false;               // +200
    void SetStates(uint32_t posture, uint32_t state);
    void SetDefaultWaitAction();
    bool BuildMove(const SBGeoPoint2D& goal, uint32_t command, uint32_t movePosture, std::deque<DVOrder>& out) const;
    uint32_t profile = 0;       // PC: +0x2b8, NPC: AI profile
    uint16_t pathIndex = 0xffff;
    uint32_t aiValue = 0;
    uint8_t aiAttitude = 0, aiByte = 0;
    uint16_t aiShort = 0;
};

class DVElementObject : public DVElement {
public:
    DVElementObject();
    int LoadStateFromFile(SBFile& f) override;
    void Refresh(uint32_t surface, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) override;
    uint32_t objectType = 0;   // +0xa0
    uint16_t u94 = 0, u96 = 0, u98 = 0, value9a = 0, u9c = 0;
    uint32_t animation = 0;    // +0xa8
};

class DVElementFX : public DVElement {
public:
    DVElementFX();
    int LoadStateFromFile(SBFile& f) override;
    void Refresh(uint32_t surface, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) override;
    void Hourglass() override;
    uint32_t shadowKind = 0;   // +0x90 (1 = has a shadow)
    DVPatch* patch = nullptr;  // +0x94
    bool restore = false;      // +0xb4
};

class DVElementTarget : public DVElementFX {
public:
    int LoadStateFromFile(SBFile& f) override;
    void Hourglass() override;
    uint32_t value144 = 0;
};
