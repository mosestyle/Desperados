// The level's elements (Game/DVElement*.cpp): heroes, enemies, civilians, animals, objects,
// animated scenery (DVElementFX) and script targets. Each is drawn from its DVSprite into its own
// surface (with the masks in front of it), then onto the screen (Refresh); Hourglass is its 25 Hz
// update. This step has their loading and drawing; their behaviour comes with the next steps.
#pragma once
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "dv/DVHikingGuide.h"
#include "dv/DVSprite.h"
#include "sb/SBGeo.h"

class SBFile;
class DVPatch;
class DVArtificialIntelligence;

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
    uint8_t direction = 0;      // the turn's direction (the sequence element's +0x20)
    static uint32_t NewID();
};

// an NPC's view (DVviewParameters, NPC +0x2a8): the cone's direction, width and length and how
// it moves (the status: 0 closed, 1 straight ahead, 2 sweeping, 3 turning round, 4-7 on a
// target, 8 fading, 9 glancing)
struct DVviewParameters {
    uint32_t alert = 0;            // +0x2a8
    uint8_t status = 0;            // +0x2ac
    bool changed = false;          // +0x2ad
    uint16_t u2ae = 0x9a;          // +0x2ae
    float sweepAmplitude = 0.8f;   // +0x2b0
    float sweepPhase = 0;          // +0x2b4
    float sweepSpeed = 0.0392f;    // +0x2b8
    float turnStep = 0.19635f;     // +0x2bc
    float angle = 0;               // +0x2c0 angle from the body's direction
    float width = 0.5f;            // +0x2c4
    float widthTarget = 0.5f;      // +0x2c8
    float widthSpeed = 0.1f;       // +0x2cc
    bool widthMoving = false;      // +0x2d0
    SBGeoVector2D dir;             // +0x2e4 the cone's axis (in ground proportions)
    SBGeoVector2D edgeL, edgeR;    // +0x2ec, +0x2f4 its two sides, on the map (y * 0.5736)
    SBGeoPoint2D target;           // +0x2fc
    class DVElement* targetElement = nullptr;  // +0x304
    uint16_t radius = 300;         // +0x308 the cone's length
    uint16_t radiusNow = 300;      // +0x30a its length now (while the eyes open)
    uint16_t opening = 1000;       // +0x30c 0..1000
    uint16_t u30e = 10;            // +0x30e
    float f310 = 1.0f;             // +0x310 length factor
    float halfWidth = 0.5f;        // +0x314 half the opening angle
    uint16_t length = 300;         // +0x318 radiusNow * f310
    float f31c = 1.0f;             // +0x31c width factor
    bool b330 = false;             // +0x330
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
    CMD_TURN = 0xe0,     // turn on the spot (the sequence's 0x1d)
    CMD_NONE = 0xbb,
};

// postures (DVposture)
enum : uint32_t { POSTURE_STANDING = 0, POSTURE_LYING = 2, POSTURE_GETTING_UP = 10 };

class DVElementActor : public DVElement {
public:
    DVElementActor();
    ~DVElementActor() override;
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
    uint32_t profile = 0;       // PC: +0x2b8

    // ---- the NPCs (DVElementActorNPC) ---------------------------------------------------------------
    bool IsNPC() const { return kind == KIND_VILLAIN || kind == KIND_CIVILIAN; }
    std::unique_ptr<DVArtificialIntelligence> ai;   // the virtual base
    DVviewParameters view;         // +0x2a8
    DVviewParameters savedView;    // +0x334 (while glancing)
    SBGeoVector2D homeDir;         // +0x224
    DVposition home;               // +0x22c
    uint16_t lastDirection = 0;    // +0x220
    bool knockedOut = false;       // +0x12e
    uint16_t hearing = 100;        // +0x130
    DVElement* horse = nullptr;    // +0x134
    bool isMove = false;           // the current sequence is a walk (DVSequenceElement 0x17)
    uint8_t frameOffset = 0;       // +0x23c (spreads The16thFrame over the frames)
    uint32_t CurrentCommand() const;   // +0xbc ? order : default command
    void Launch(const std::deque<DVOrder>& seq, bool move);   // DVSequenceManager::LaunchSequence
    bool InitState(DVArtificialIntelligence& a);   // DVArtificialIntelligence::InitState
    void Halt();                       // 0x0822d360
    void StoreInitialPositionParameters();  // 0x08238b10
    void LaunchTimer(uint32_t frames, bool macro);  // 0x08236090
    void KillTimer(bool macro);        // 0x08236100
    void SetViewCone(uint32_t cone);   // 0x082363e0
    void SetViewStatus(uint8_t status, bool force);  // 0x0822d0e0
    void CloseEyes() { view.status = 0; view.changed = false; }  // 0x0822d850
    void GlanceAt(const DVposition& p, uint32_t frames);  // 0x08236130
    void RestoreView();                // the end of a glance (NPC Hourglass, AI +0x234)
    void DontFixDirectionOnViewTarget();  // 0x0822d820
    void RefreshView();                // 0x0822d860
    void NPCHourglass();               // 0x082351e0
    bool StepOrders();
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
