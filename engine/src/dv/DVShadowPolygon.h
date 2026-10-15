// DVShadowPolygon (Game/DVShadowPolygon.cpp): draws the view cone of the NPC the player watches
// (DVEngine +0x120) on the ground, between the background and the characters.
//
// The original's cone (InitializeDisplay 0x0836e040) is a fan from the eyes: its two sides at
// +/- the half width around the view's axis, rays every 90 / length radians in between, the
// length of the view. Over the floors it is lit with the alert colour, 0xc0 / 256 opaque at the
// eyes fading to 4 / 256 at its end (PrepareSlice: CreateAlphaIterator), 0x78 at night. Where an
// obstacle hides a lying hero but not a standing one the cone is drawn every other pixel; where
// nothing can be seen it is not drawn; the background's masks in front cut it.
//
// The original works it out by projecting each obstacle's planes on the floors and drawing the
// slices column by column (GetProjectionAreas, BuildBoundaries, ProjectObstacles, DisplaySlice:
// 18000 decompiled lines). Here the same picture is sampled: each ray is walked in steps and at
// each step DVFastFindGrid::IsReachable (the test the detection uses) says what the NPC sees there,
// then the cells are drawn as triangles with the GPU.
#pragma once
#include <cstdint>
#include <vector>

#include "sb/SBGeo.h"

class DVElementActor;

class DVShadowPolygon {
public:
    // the cone of `npc` onto `surface`; `cam` the map point at the surface's top-left
    void Display(uint32_t surface, const SBGeoPoint2D& cam, float zoom, DVElementActor* npc, bool night);

private:
    // what the NPC sees on the ground: 0 nothing, 1 a standing hero only, 2 even a lying one
    struct Run {
        uint16_t ray;          // between ray `ray` and ray + 1
        uint16_t from, to;     // steps
        uint8_t state;
    };
    std::vector<Run> runs;
    // the inputs the runs were computed for
    const DVElementActor* npc = nullptr;
    SBGeoPoint3D eyes;
    SBGeoVector2D dir;
    float halfWidth = 0, length = 0;
    uint32_t frame = 0xffffffff;
    // the fan
    float a0 = 0, radius = 0, floorZ = 0;
    uint16_t rays = 0, steps = 0;
    std::vector<float> solid, pattern;   // the triangles, 6 floats a vertex

    void Build(DVElementActor* npc);
};
