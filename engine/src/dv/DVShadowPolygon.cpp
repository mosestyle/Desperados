#include "dv/DVShadowPolygon.h"

#include <algorithm>
#include <cmath>

#include "dv/DVArtificialIntelligence.h"
#include "dv/DVElement.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVMask.h"
#include "dv/DVSprite.h"
#include "sb/SBDrawManager.h"

namespace {
const float kGround = 0.57357645f;  // ground y -> map y (cos 55 degrees)
const float kRayStep = 0.03f;       // radians between two rays
const float kStep = 6.0f;           // ground pixels between two samples of a ray
}  // namespace

void DVShadowPolygon::Build(DVElementActor* n) {
    runs.clear();
    rays = steps = 0;
    const DVviewParameters& v = n->view;
    SBGeoPoint3D e = n->EyesPoint();
    floorZ = n->sprite->pos.pos3D.z;
    float dz = e.z - floorZ;
    float r2 = (float)v.length * (float)v.length - dz * dz;
    if (r2 <= 1.0f) return;
    radius = std::sqrt(r2);
    SBGeoVector2D d = v.dir;
    if (d.x == 0.0f && d.y == 0.0f) d = SBGeoVector2D(1, 0);
    a0 = std::atan2(d.y, d.x);
    float hw = v.halfWidth;
    rays = (uint16_t)std::max(2, (int)std::ceil(2.0f * hw / kRayStep));
    steps = (uint16_t)std::max(1, (int)std::ceil(radius / kStep));
    DVFastFindGrid* grid = DVFastFindGrid::mpFastFindGrid;
    uint16_t layer = n->sprite->pos.layer;
    std::vector<DVSightObstacle*> obs;
    std::vector<DVMask*> masks;
    for (uint16_t i = 0; i < rays; ++i) {
        float ang = a0 - hw + ((float)i + 0.5f) * (2.0f * hw / rays);
        float c = std::cos(ang), s = std::sin(ang);
        // what can be in the way along this ray, picked once
        SBGeoPoint2D far(e.x + radius * c, e.y + radius * s * kGround);
        SBGeoBoundingBox2D rb;
        rb.Expand(SBGeoPoint2D(e.x, e.y));
        rb.Expand(far);
        obs.clear();
        masks.clear();
        if (grid) {
            grid->GetObstacles(obs, 0, rb, 2);
            SBGeoBoundingBox2D mb;
            mb.Expand(SBGeoPoint2D(e.x, e.y - floorZ));
            mb.Expand(SBGeoPoint2D(far.x, far.y - floorZ));
            grid->GetMasks(masks, layer, mb);
        }
        auto sees = [&](const SBGeoPoint3D& p) { return !grid || grid->IsReachable(e, p, obs); };
        uint16_t start = 0;
        uint8_t cur = 0xff;
        for (uint16_t j = 0; j < steps; ++j) {
            float rho = ((float)j + 0.5f) * radius / steps;
            SBGeoPoint3D p(e.x + rho * c, e.y + rho * s * kGround, floorZ);
            uint8_t st = 0;
            SBGeoPoint3D q = p;
            q.z = floorZ + 2.0f;   // a lying hero (ComputeDetectionPoints, posture 2)
            if (sees(q)) {
                st = 2;
            } else {
                q.z = floorZ + 46.0f;  // standing: the head, the feet
                if (sees(q)) st = 1;
                else {
                    q.z = floorZ + 16.0f;
                    if (sees(q)) st = 1;
                }
            }
            if (st) {
                // the background in front hides that bit of ground
                SBGeoPoint2D m(p.x, p.y - floorZ);
                for (DVMask* mk : masks)
                    if (mk->box.IsInside_p(m) && mk->IsAppliedToPointCharacter(m) && mk->Covers((int)m.x, (int)m.y)) {
                        st = 0;
                        break;
                    }
            }
            if (st != cur) {
                if (cur != 0xff && cur != 0) runs.push_back({i, start, j, cur});
                start = j;
                cur = st;
            }
        }
        if (cur != 0xff && cur != 0) runs.push_back({i, start, steps, cur});
    }
}

void DVShadowPolygon::Display(uint32_t surface, const SBGeoPoint2D& cam, float zoom, DVElementActor* n, bool night) {
    if (!n || !n->ai || !n->sprite) return;
    const DVviewParameters& v = n->view;
    // DVElementActorNPC::ShowDetectionPolygon: not while lying (dead / knocked out), nor with the
    // eyes shut
    if (n->sprite->pos.posture == 0xe || v.status == 0 || n->knockedOut) return;
    SBGeoPoint3D e = n->EyesPoint();
    uint32_t now = *DVArtificialIntelligence::mpUniversalFrameCounter;
    if (n != npc || e.x != eyes.x || e.y != eyes.y || e.z != eyes.z || v.dir.x != dir.x || v.dir.y != dir.y ||
        v.halfWidth != halfWidth || (float)v.length != length || now - frame >= 25) {
        npc = n;
        eyes = e;
        dir = v.dir;
        halfWidth = v.halfWidth;
        length = (float)v.length;
        frame = now;
        Build(n);
    }
    if (runs.empty()) return;
    uint8_t cr, cg, cb;
    n->GetAlertColor(cr, cg, cb);
    float R = cr / 255.0f, G = cg / 255.0f, B = cb / 255.0f;
    // the alpha at the eyes (shorter on the ground when the eyes are high) and at the end
    float top = night ? 120.0f : 192.0f;
    float a0v = std::max(4.0f, top * std::min(1.0f, radius / std::max(1.0f, length)));
    float hw = halfWidth;
    auto vertex = [&](std::vector<float>& out, uint16_t ray, uint16_t step) {
        float ang = a0 - hw + (float)ray * (2.0f * hw / rays);
        float rho = (float)step * radius / steps;
        float mx = eyes.x + rho * std::cos(ang);
        float my = eyes.y + rho * std::sin(ang) * kGround - floorZ;
        float a = (a0v + (4.0f - a0v) * (float)step / steps) / 256.0f;
        out.push_back((mx - cam.x) * zoom);
        out.push_back((my - cam.y) * zoom);
        out.push_back(R);
        out.push_back(G);
        out.push_back(B);
        out.push_back(a);
    };
    solid.clear();
    pattern.clear();
    for (const Run& r : runs) {
        std::vector<float>& out = r.state == 2 ? solid : pattern;
        vertex(out, r.ray, r.from);
        vertex(out, (uint16_t)(r.ray + 1), r.from);
        vertex(out, (uint16_t)(r.ray + 1), r.to);
        vertex(out, r.ray, r.from);
        vertex(out, (uint16_t)(r.ray + 1), r.to);
        vertex(out, r.ray, r.to);
    }
    SBDrawManager* dm = SBDrawManager::mpDrawManager;
    if (!dm) return;
    if (!solid.empty()) dm->DrawTriangles(surface, solid, false);
    if (!pattern.empty()) dm->DrawTriangles(surface, pattern, true);
}
