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
class DVSequenceElement;

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
    class DVElement* target = nullptr;  // the sequence element's +0x30 (who to shoot at)
    SBGeoPoint3D aim;                   // +8 (where to shoot)
    uint16_t damage = 0;                // a bullet's hit (the sequence element's +0x16)
    class DVElement* from = nullptr;    // ... and who shot it (+0x2c)
    uint32_t anim = 0;                  // a script's animation (the sequence element's +0x20)
    class DVDoor* door = nullptr;       // the door passed at the end of this order (position +0xec)
    bool enter = false;                 // ... from its first side (+0xf0)
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
    CMD_SHOOT_LYING = 8, // "Tirer couché"
    CMD_HIT = 0x19,      // hit by a bullet, standing
    CMD_HIT_LYING = 0x1a,
    CMD_SHOOT = 0x1c,    // "Tirer"
    CMD_KNOCKED = 0x20,
    CMD_DIE = 0x21,      // "Mourir"
    CMD_DEAD = 0xd2,     // lying dead (the death's last frame)
    CMD_DEAD_KNOCKED = 0xd3,
    CMD_RELOAD = 0xe1,   // the engine's own: wait while reloading
    CMD_TURN = 0xe0,     // turn on the spot (the sequence's 0x1d)
    CMD_LEAVE = 0xe8,    // the engine's own: leave the map at the end of a script's walk (0x1a)
    CMD_PUNCH = 0xe9,    // a punch (DVElementActorHuman::Execute 0xe9: animation 0x24)
    CMD_PUNCH_ANIM = 0x24,
    CMD_SCRIPT_ANIM = 0xea,    // a script's animation, once (the sequence's 0x20)
    CMD_SCRIPT_LOOP = 0xeb,    // ... looped (0x21)
    CMD_SCRIPT_FREEZE = 0xec,  // ... stopping on its last frame (0x22)
    CMD_MOVE_ANIM = 0xee,      // the engine's own: a move with o.anim (doors, lifts)
    CMD_TELEPORT = 0xef,       // the engine's own: put there (inside a house: to the next door)
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
    bool playable = true;       // +0xb5 (DVScript::Deactivate: a hero the player can't use yet)
    bool animalLocked = false;  // +0x139 (DVScript::LockAI for the animals)
    bool actionAvailable[6] = {true, true, true, true, true, true};  // PC +0x1a3 (the scripts' ISetActionAvailable)
    int selectionPulse = 0;     // +0x188 ticks left of the selection flash
    int selectionPulseLen = 0;  // +0x18c

    std::deque<DVOrder> orders;     // the current sequence element's orders
    // ---- the scripts' sequences (DVElementActor::Instruct) -------------------------------------
    DVSequenceElement* scriptElement = nullptr;  // +0xb8 the script's element being done
    uint32_t defaultAnim = 0;       // +0xd0 the animation of a frozen / looped default (0xec)
    bool defaultLoop = false;
    // DVElementActor::Instruct (0x0820d540) and Translate: the element becomes orders
    void Instruct(DVSequenceElement* e);
    // DVElementActor[Human/NPC/PC]::ExecuteImmediately: the elements done at once
    void ExecuteImmediately(DVSequenceElement* e);
    // the script's element is over (done, or replaced by other orders)
    void EndScriptElement(bool done);
    // DVScript::SetActorLocation: put him there at once
    void Teleport(const SBGeoPoint2D& p, uint16_t layer, class DVSector* area);
    uint32_t defaultCommand = CMD_NONE;  // +0xcc
    uint32_t actionState = 0;            // +0xc4
    bool newOrder = false;               // +200
    void SetStates(uint32_t posture, uint32_t state);
    void SetDefaultWaitAction();
    // the orders of a move to `goal` on `goalArea` (nullptr: the area under the goal, his own if
    // none): the walks, and the doors and lifts on the way (DVSequence::AppendMoveToSequence)
    bool BuildMove(const SBGeoPoint2D& goal, uint32_t command, uint32_t movePosture, std::deque<DVOrder>& out,
                   class DVSectorMotionArea* goalArea = nullptr) const;
    // a walk inside one area (DVPathFinder::FindPath)
    bool BuildMoveInArea(const SBGeoPoint2D& start, class DVSectorMotionArea* area, uint16_t layer,
                         const SBGeoPoint2D& goal, uint32_t command, uint32_t movePosture, bool fromHere,
                         std::deque<DVOrder>& out) const;
    // the area he stands on (a walkable area, a lift or a house), his house if he is in one
    class DVSectorMotionArea* Area() const;
    class DVSectorBuilding* Building() const;
    bool InBuilding() const { return Building() != nullptr; }
    void PassDoor(class DVDoor* d, bool enter);   // 0x0820f7e0
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
    int16_t life = 0x29a;          // +0x130 (DVElementActorHuman: 666; heroes 100; villains their energy)
    DVElement* horse = nullptr;    // +0x134
    // a prisoner (+0x9d) and the villain who holds him at gunpoint (+0xa0)
    DVElementActor* menacer = nullptr;
    bool IsHeldPrisoner() const {
        return menacer && !menacer->dead && !menacer->knockedOut && menacer->active;
    }
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
    // DVElementActorNPC::SetViewTarget (0x0822d590 / 0x0822d140): the cone follows an element
    // (status 6 / 7) or looks at a place (4 / 5); `turn`: the body turns to it too
    void SetViewTarget(DVElement* e, bool turn);
    void SetViewTarget(const SBGeoPoint2D& p, bool turn);
    void RefreshView();                // 0x0822d860
    void NPCHourglass();               // 0x082351e0
    bool StepOrders();
    // ---- sight (DVElementActorHuman / DVElementActorNPC) --------------------------------------
    SBGeoPoint3D EyesPoint() const;    // ComputeEyesPoint (0x08223440), the current posture
    // ComputeDetectionPoints (0x082236b0): feet / body / head (lying: along the body)
    void DetectionPoints(SBGeoPoint3D& a, SBGeoPoint3D& b, SBGeoPoint3D& c) const;
    // IsDetecting(SBGeoPoint3D&, bool) (0x08233fd0): inside the cone and nothing in between
    bool SeesPoint(const SBGeoPoint3D& p, bool range) const;
    float ViewRadius(const SBGeoPoint3D& eyes) const;  // ComputeViewRadius (0x08233490), the ground
    // IsDetecting(DVElementActorHuman*, ...) (0x082323e0): 0 not seen, 1 seen, 2 seen near enough
    // to be identified; `speed` how fast the suspicion grows
    uint8_t DetectHero(const DVElementActor* hero, uint16_t& speed) const;
    bool IsObviouslyHostile() const;   // DVElementActorPC (0x08247ea0)
    int32_t ShadowZone() const;        // DVElementActorHuman::GetShadowZone (0x08229280)
    void RefreshDetection();           // 0x0822eae0 (the heroes' part)
    bool detecting = false;            // +0x23f a stimulus was just sent: the cone flashes white
    void ResetDazzlingImmunity() {}    // 0x0823acf0 (the mirror's blinding comes with the fights)
    // DVElementActorNPC::GetAlertColor (0x08234b40): the view cone's colour
    void GetAlertColor(uint8_t& r, uint8_t& g, uint8_t& b) const;

    // ---- firearms (DVElementActorHuman, Game/DVElementActorHuman.cpp) ----------------------------
    const struct DVFirearm* firearm = nullptr;          // +0x14c
    int16_t ammo = 0;                                   // +0x14a bullets in the gun
    const struct DVCharacterProfile* heroProfile = nullptr;  // PC +0x2b0
    uint8_t villainSkill = 50;                          // villain +0x4de (profile's shooting skill)
    bool dead = false;
    bool lastShotHit = false;                           // +0x3fa
    void InitFirearm();                                 // the profile's weapon and life
    uint8_t GetShootingSkill() const;                   // 0x081fc120 / 0x0823e210
    bool IsSnipering(bool) const { return false; }      // 0x081fc190
    SBGeoPoint3D FirearmPoint() const;                  // ComputeFirearmPoint (the shot's start)
    // GetHitProbability (0x082260d0): % of chance to hit at `ratio` = distance / range
    uint8_t GetHitProbability(float ratio, bool hard, bool special) const;
    // Shoot(DVElement*, DVFirearm const*, bool) (0x082248a0): the bullets at someone
    bool Shoot(DVElement* target, const struct DVFirearm* w, bool special);
    // ShootSingleBullet (0x082257f0): a bullet that misses (or a shot at a place)
    bool ShootSingleBullet(const SBGeoPoint3D& at, const struct DVFirearm* w, bool special, bool place);
    // DVElementActorHuman::Translate 0x2f: a bullet hits; the hit or the death is played
    void GotBullet(uint16_t damage, DVElement* shooter);
    // DVElementActorHuman::Translate 0x2d / 0x30: a blow (a punch): the life it takes, then he
    // falls (0x20) or dies (0x21); GotHit (0x08238210 / 0x08247420) adds to the concussion
    void KnockHit(uint16_t damage, DVElement* from);
    void GotHit(uint16_t damage, DVElement* from);
    // DVElementActorHuman::SetConcussionOfTheBrain (0x0821bf00): over 0x45 he is out cold, under
    // 0x1e he comes to; it goes down with the time (DVElementActorHuman::Hourglass)
    void SetConcussion(int v);
    uint16_t concussion = 0;          // +300
    void Kill();                                        // DVElementActorPC / NPC / NPCVillain::Kill
    // the orders of a shot at someone (DVElementActorHuman::Translate 0x32)
    bool OrderShoot(DVElement* target);
    void Reload();
    static void InitColors(uint32_t green, uint32_t yellow, uint32_t red, uint32_t pink);  // 0x0822b6a0
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
    DVElementTarget() { classFlags = 8; }
    int LoadStateFromFile(SBFile& f) override;
    void Hourglass() override;
    // DVElementTarget::Instruct (0x08265de0): the scripts' animations (0x20 once, 0x22 frozen)
    void Instruct(class DVSequenceElement* e);
    uint32_t value144 = 0;     // +0x144 what the script hears of (1 stabbed, 2 dagger, 4 shot)
    uint32_t animation = 0;    // +0x13c
    int progression = 0;       // +0x140
};
