// Desktop check: loads a level with the engine and writes what it draws to a picture.
//   dvrender <game folder> <level_01> <out.ppm> [width height [camX camY [ticks]]]
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVFastFindGrid.h"
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

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "dvrender <game> <level> <out.ppm> [w h [camX camY [ticks]]]\n");
        return 1;
    }
    int W = argc > 5 ? atoi(argv[4]) : 1024, H = argc > 5 ? atoi(argv[5]) : 768;
    if (!initGL()) {
        fprintf(stderr, "no GL\n");
        return 1;
    }
    SBFile::SetGameRoot(argv[1]);
    SBDrawManager draw;
    draw.OpenScreen((uint16_t)W, (uint16_t)H);
    DVEngine engine(&draw);
    engine.screen = SBGeoVector2D((float)W, (float)H);
    if (!engine.LoadStateFromFile(argv[2])) {
        fprintf(stderr, "level not loaded\n");
        return 1;
    }
    if (argc > 7) {
        engine.camera = SBGeoPoint2D((float)atof(argv[6]), (float)atof(argv[7]));
        engine.ClampCamera();
    }
    int ticks = argc > 8 ? atoi(argv[8]) : 0;
    for (int i = 0; i < ticks; ++i) engine.PerformHourglass();
    int counts[8] = {0};
    for (DVElement* e : engine.Elements()) {
        if (e->kind <= 7) counts[0]++;
        else if (e->kind == KIND_VILLAIN) counts[1]++;
        else if (e->kind == KIND_CIVILIAN) counts[2]++;
        else if (e->kind == KIND_FX) counts[3]++;
        else counts[4]++;
    }
    fprintf(stderr, "map %gx%g, elements %zu (heroes %d, villains %d, civilians %d, scenery %d, other %d), sectors %zu\n",
            engine.mapW, engine.mapH, engine.Elements().size(), counts[0], counts[1], counts[2], counts[3], counts[4],
            engine.Grid()->SectorCount());
    engine.Draw();
    SBDrawViewport vp;
    draw.GetSurfaceViewport(1, vp, VIEWPORT_READ);
    FILE* f = fopen(argv[3], "wb");
    fprintf(f, "P6\n%d %d\n255\n", vp.width, vp.height);
    for (int y = 0; y < vp.height; ++y)
        for (int x = 0; x < vp.width; ++x) {
            uint16_t c = vp.data[(size_t)y * vp.width + x];
            unsigned char rgb[3] = {(unsigned char)((c >> 11) * 255 / 31), (unsigned char)((c >> 5 & 63) * 255 / 63),
                                    (unsigned char)((c & 31) * 255 / 31)};
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
    fprintf(stderr, "camera %g,%g -> %s\n", engine.camera.x, engine.camera.y, argv[3]);
    return 0;
}
