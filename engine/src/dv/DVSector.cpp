#include "dv/DVSector.h"

#include "sb/SBFile.h"

static uint32_t gTotalSightObstacles = 0;
static int16_t gTotalSectors = 0;  // gswTotalNumberOfSectors

DVSector::DVSector(uint32_t t) : type(t) { id = gTotalSectors++; }

void DVSector::ResetCounters() {
    gTotalSectors = 0;
    gTotalSightObstacles = 0;
}

// DVSector::LoadFromFile (0x08358680)
int DVSector::LoadFromFile(SBFile& f) {
    uint16_t n = f.U16();
    for (uint16_t i = 0; i < n; ++i) {
        int16_t x = f.S16(), y = f.S16();
        SBGeoPoint2D p((float)x, (float)y);
        polygon.pts.push_back(p);
        box.Expand(p);
    }
    return 2 + 4 * n;
}

DVSightObstacle::DVSightObstacle(uint32_t f) : flags(f) { serial = gTotalSightObstacles++; }

// DVSightObstacle::LoadFromFile (0x08388700)
int DVSightObstacle::LoadFromFile(SBFile& f) {
    SBGeoVector3D a, b, c;        // top plane points (x, y - h, h)
    SBGeoVector3D ga, gb, gc;     // ground plane points (x, y, z)
    uint16_t n = f.U16();
    int size = 2;
    float minY = 3.4028235e38f, maxY = -3.4028235e38f;  // DAT_08569980 / DAT_08569984
    for (uint16_t i = 0; i < n; ++i) {
        float x = f.F32();
        float y = f.F32();
        float z = f.F32();  // (stored before h)
        float h = f.F32();
        float yy = y - h;
        points.push_back(SBGeoVector3D(x, y, h));
        polygon.pts.push_back(SBGeoPoint2D(x, y));
        if (z != 0.0f) flat = false;
        if (i == 2) {
            b = {x, y, h};
            gb = {x, y, z};
        } else if (i == 1) {
            a = {x, y, h};
            ga = {x, y, z};
        } else if (i == 0) {
            c = {x, y, h};
            gc = {x, y, z};
        }
        size += 16;
        if (yy <= minY) minY = yy;
        if (maxY <= yy) maxY = yy;
    }
    plane.SetPoints(a, b, c);
    plane.InitializeAll();
    planeGround.SetPoints(ga, gb, gc);
    planeGround.InitializeAll();
    if (plane.normal.z < 0.0f) {
        plane.SetPoints(a, c, b);
        plane.InitializeAll();
        planeGround.SetPoints(ga, gc, gb);
        planeGround.InitializeAll();
    }
    float x0 = f.F32(), z0 = f.F32(), y0 = f.F32();
    float x1 = f.F32(), z1 = f.F32(), y1 = f.F32();
    box3dMin = {x0, y0, z0};
    box3dMax = {x1, y1, z1};
    box.Expand(SBGeoPoint2D(x0, y0));
    box.Expand(SBGeoPoint2D(x1, y1));
    boxScreen.Set(SBGeoPoint2D(x0, minY), SBGeoPoint2D(x1, maxY));
    uint8_t onArea = f.U8();
    if (!onArea) {
        size += 0x19;
        motionLayer = -1;
    } else {
        flags |= 4;
        motionLayer = f.U16();
        motionArea = f.U16();
        size += 0x1d;
    }
    uint8_t b1 = f.U8();
    flags = b1 ? (flags | 2) : (flags & ~2u);
    uint8_t b2 = f.U8();
    flags = b2 ? (flags | 1) : (flags & ~1u);
    uint8_t b3 = f.U8();
    if (b2) flags = b3 ? (flags | 8) : (flags & ~8u);
    material = f.U8();
    u15c = f.U32();
    u160 = f.U32();
    u168 = f.U8();
    f.U32();
    return size + 0x11;
}
