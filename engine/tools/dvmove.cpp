// Desktop check of the heroes' motion: orders a hero to a point and writes frames of the walk.
//   dvmove <game> <level> <outprefix> <w> <h> <hero> <x> <y> <run 0/1> <ticks> <every> [crouchAt]
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
#include "dv/DVPathFinder.h"
#include "dv/DVSprite.h"
#include "dv/DVSector.h"
#include "dv/DVLine.h"
#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"

static bool initGL() {
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, nullptr, nullptr)) return false;
    const EGLint cfgAttr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig cfg;
    EGLint n = 0;
    if (!eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n) || n < 1) return false;
    const EGLint pb[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (!ctx) return false;
    return eglMakeCurrent(dpy, surf, surf, ctx);
}


static void plot(SBDrawViewport& vp, int x, int y, uint16_t c) {
    if (x < 0 || y < 0 || x >= vp.width || y >= vp.height) return;
    vp.data[(size_t)y * vp.width + x] = c;
}
static void line(SBDrawViewport& vp, SBGeoPoint2D a, SBGeoPoint2D b, uint16_t c) {
    float dx = b.x - a.x, dy = b.y - a.y;
    int n = (int)(std::fabs(dx) > std::fabs(dy) ? std::fabs(dx) : std::fabs(dy)) + 1;
    for (int i = 0; i <= n; ++i) plot(vp, (int)(a.x + dx * i / n), (int)(a.y + dy * i / n), c);
}

static SBGeoPoint2D gDest(-1, -1);
static void save(SBDrawManager& draw, DVEngine& engine, const char* name, DVElementActor* h, bool walls) {
    engine.Draw();
    SBDrawViewport vp;
    draw.GetSurfaceViewport(1, vp, VIEWPORT_READ);
    SBGeoPoint2D cam(std::floor(engine.camera.x), std::floor(engine.camera.y));
    if (walls) {
        std::vector<DVLine*> lines;
        SBGeoBoundingBox2D view(cam, cam + engine.screen);
        engine.Grid()->GetLines(lines, h->sprite->pos.layer, view, LINE_MOTION);
        for (DVLine* l : lines) line(vp, l->seg.a - cam, l->seg.b - cam, 0xf800);
    }
    if (walls) {
        DVSector* sec = h->sprite->pos.sector;
        if (sec && (sec->type & 3) == 3)
            for (const DVpathGraphNode* n : engine.PathFinder()->AreaNodes(h->sprite->pos.layer,
                                                                            static_cast<DVSectorMotionArea*>(sec)->areaIndex)) {
                SBGeoPoint2D q = n->p - cam;
                uint8_t dk = n->docks.empty() ? 0 : n->docks[0];
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) plot(vp, (int)q.x + dx, (int)q.y + dy, 0x001f);
                SBGeoVector2D hd = engine.PathFinder()->HalfDiagonals()[0];
                if (dk & 1) plot(vp, (int)(q.x - hd.x), (int)(q.y - hd.y), 0x07ff);
                if (dk & 2) plot(vp, (int)(q.x + hd.x), (int)(q.y - hd.y), 0x07ff);
                if (dk & 4) plot(vp, (int)(q.x + hd.x), (int)(q.y + hd.y), 0x07ff);
                if (dk & 8) plot(vp, (int)(q.x - hd.x), (int)(q.y + hd.y), 0x07ff);
            }
        if (gDest.x > 0) {
            SBGeoPoint2D q = gDest - cam;
            for (int d = -4; d <= 4; ++d) {
                plot(vp, (int)q.x + d, (int)q.y, 0xffff);
                plot(vp, (int)q.x, (int)q.y + d, 0xffff);
            }
        }
    }
    std::vector<SBGeoPoint2D> p = h->PlannedPath();
    for (size_t i = 1; i < p.size(); ++i) line(vp, p[i - 1] - cam, p[i] - cam, 0xffe0);
    FILE* f = fopen(name, "wb");
    fprintf(f, "P6\n%d %d\n255\n", vp.width, vp.height);
    for (int y = 0; y < vp.height; ++y)
        for (int x = 0; x < vp.width; ++x) {
            uint16_t c = vp.data[(size_t)y * vp.width + x];
            unsigned char rgb[3] = {(unsigned char)((c >> 11) * 255 / 31), (unsigned char)((c >> 5 & 63) * 255 / 63),
                                    (unsigned char)((c & 31) * 255 / 31)};
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}

int main(int argc, char** argv) {
    if (argc < 12) {
        fprintf(stderr, "dvmove <game> <level> <outprefix> <w> <h> <hero> <x> <y> <run> <ticks> <every> [crouchAt]\n");
        return 1;
    }
    int W = atoi(argv[4]), H = atoi(argv[5]);
    if (!initGL()) return 1;
    SBFile::SetGameRoot(argv[1]);
    SBDrawManager draw;
    draw.OpenScreen((uint16_t)W, (uint16_t)H);
    DVEngine engine(&draw);
    engine.screen = SBGeoVector2D((float)W, (float)H);
    if (!engine.LoadStateFromFile(argv[2])) return 1;
    fprintf(stderr, "graph nodes %zu, half diagonals %zu\n", engine.PathFinder()->NodeCount(),
            engine.PathFinder()->HalfDiagonals().size());
    int hi = atoi(argv[6]);
    if (hi >= (int)engine.Heroes().size()) return 1;
    DVElementActor* h = engine.Heroes()[hi];
    engine.Select(h);
    SBGeoPoint2D dest((float)atof(argv[7]), (float)atof(argv[8]));
    if (getenv("DVREL")) dest = h->sprite->pos.posMap + dest;
    bool run = atoi(argv[9]) != 0;
    gDest = dest;
    int ticks = atoi(argv[10]), every = atoi(argv[11]);
    int crouchAt = argc > 12 ? atoi(argv[12]) : -1;
    DVPositionInterface& pos = h->sprite->pos;
    fprintf(stderr, "%s at %.1f,%.1f layer %u area %d posture %u boxes %g,%g %g,%g paths %u/%u\n", h->KindName(), pos.posMap.x,
            pos.posMap.y, pos.layer, pos.sector ? pos.sector->id : -1, pos.posture, pos.moveBoxUp.p1.x, pos.moveBoxUp.p1.y,
            pos.moveBox.p1.x, pos.moveBox.p1.y, pos.pathIndex, pos.pathIndexAlt);
    if (getenv("DVDEBUG")) {
        std::vector<SBGeoPoint2D> pts = {pos.posMap};
        float a = (float)atof(getenv("DVDEBUG")), b = (float)atof(strchr(getenv("DVDEBUG"), ',') + 1);
        SBGeoSegment2D sg(pos.posMap, SBGeoPoint2D(a, b));
        std::vector<DVLine*> ls;
        engine.Grid()->GetLines(ls, pos.layer, sg, LINE_MOTION);
        fprintf(stderr, "lines crossing: %zu\n", ls.size());
        for (DVLine* l : ls) fprintf(stderr, "  %g,%g - %g,%g flags %x sector %d\n", l->seg.a.x, l->seg.a.y, l->seg.b.x, l->seg.b.y, l->flags, l->sectorId);
        fprintf(stderr, "thick %d\n", engine.Grid()->IsReachableThick(pos.posMap, SBGeoPoint2D(a, b), pos.layer, SBGeoVector2D(6, 3)));
    }
    bool ok = dest.x < 0 ? true : engine.OrderMove(dest, run);
    fprintf(stderr, "order %s, %zu orders\n", ok ? "ok" : "REFUSED", h->orders.size());
    for (const DVOrder& o : h->orders) fprintf(stderr, "  cmd %u -> %.1f,%.1f\n", o.command, o.goal.x, o.goal.y);
    char name[512];
    int frame = 0;
    for (int t = 0; t <= ticks; ++t) {
        if (t == crouchAt) engine.OrderCrouch();
        if (t % every == 0 && (every < 1000 || t == 0)) {
            engine.camera = (t == 0 && dest.x > 0 ? (pos.posMap + dest) * 0.5f : pos.posMap) - engine.screen * 0.5f;
            engine.ClampCamera();
            snprintf(name, sizeof name, "%s%03d.ppm", argv[3], frame++);
            save(draw, engine, name, h, t == 0);
            fprintf(stderr, "t %d pos %.1f,%.1f dir %u/%u anim %u entry %u posture %u orders %zu\n", t, pos.posMap.x,
                    pos.posMap.y, pos.direction, pos.directionWanted, h->sprite->animation, h->sprite->entry, pos.posture,
                    h->orders.size());
        }
        engine.PerformHourglass();
    }
    fprintf(stderr, "END %s %.1f,%.1f orders %zu goal %.1f,%.1f\n", h->KindName(), pos.posMap.x, pos.posMap.y, h->orders.size(),
            dest.x, dest.y);
    return 0;
}
