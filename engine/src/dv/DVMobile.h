// Mobile elements (Game/DVElementMobile.cpp): the carts, coaches, the big gatling and the train of
// the CART hunk. A mobile element has no picture of its own: it is a point that goes along a
// route of the WAYS hunk (its waypoints carry macro commands: speed, braking, waits, animation,
// direction), and takes its parts (DVElementFXMasked sprites), its sight obstacles and its shape
// with it.
#pragma once
#include <memory>
#include <vector>

#include "dv/DVElement.h"
#include "dv/DVHikingGuide.h"
#include "dv/DVSector.h"

class DVElementMobile;

// DVElementFXMasked: a part of a mobile element; its animation goes with the element's speed
class DVElementFXMasked : public DVElementFX {
public:
    DVElementFXMasked();
    void Refresh(uint32_t surface, float zoom, const SBGeoBoundingBox2D& view, bool silhouettes) override;
    void Hourglass() override;                // 0x0824b830
    float frameScale = 1.0f;                  // +0xb8 (1 / the element's speed)
    DVElementMobile* mobile = nullptr;        // +0xbc
};

class DVElementMobile : public DVElement {
public:
    DVElementMobile();
    ~DVElementMobile() override;
    int LoadStateFromFile(SBFile& f) override;    // 0x0824fc50
    void Hourglass() override;                    // 0x0824c0c0
    // the scripts' Start / StopMobileElement (0xad / 0xae)
    void ExecuteImmediately(class DVSequenceElement* e);  // 0x082537e0
    void Move(const SBGeoVector2D& v);            // 0x0824c870
    DVElementActor* hooked = nullptr;             // +0x1c4: Sanchez pushing it
    SBGeoVector2D RouteDirection() const;         // its route's first to last waypoint
    void SetAnimationAllSprites(uint32_t anim);   // 0x0824d7f0

    std::vector<DVElementFXMasked*> parts;        // +0x94 (the engine owns them)
    std::vector<std::unique_ptr<DVSightObstacle>> obstacles;  // +0xb0
    DVSector shape;                               // +0xcc
    SBGeoBoundingBox2D box;                       // +0x178: around its obstacles
    bool shaky = false;                           // +0x18c: the parts jolt (a coach on a track)
    std::vector<SBGeoVector2D> jolts;             // +0x194
    uint32_t anim = 0;                            // +0x1b4
    bool pushable = false;                        // +0x1b8: Sanchez pushes it from pushPoint
    SBGeoPoint2D pushPoint;                       // +0x1bc
    bool dust = false;                            // +0x1c8
    SBGeoVector3D dustPoint;                      // +0x1cc
    bool railroad = false;                        // +0x1e0: the train
    uint16_t u1e8 = 0, u1ea = 0;                  // +0x1e8, +0x1ea
    float f1ec = 0;                               // +0x1ec
    bool stopped = false;                         // +0x90
    DVPath path;                                  // +0x158
    DVWaypoint* waypoint = nullptr;               // +0x15c: the waypoint whose commands are run
    const uint8_t* macro = nullptr;               // +0x160 the next command
    const uint8_t* macroBase = nullptr;           //       (the waypoint's data)
    int16_t remaining = 0;                        // +0x164 bytes of commands left
    int wait = 0;                                 // +0x168 ticks
    float speed = 10.0f;                          // +0x16c pixels per tick
    float targetSpeed = 0;                        // +0x170
    float accel = 0;                              // +0x174
    SBGeoVector2D dir;                            // the sprite's +0x6c
    bool bigGatling = false;                      // "Level10_Big Gattling" (DVEngine +0xdf8)

private:
    void Update();                                // 0x0824c400
    void ExecuteWayPoint();                       // 0x0824c510
    bool ExecuteMacroCommand();                   // 0x0824d240
    bool GoalReached() const;
    SBGeoPoint2D goal;
};

// DVSightObstacle::Translate (0x08389a90)
void TranslateObstacle(DVSightObstacle& o, const SBGeoVector2D& v);
