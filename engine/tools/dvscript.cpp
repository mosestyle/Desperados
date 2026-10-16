// Desktop check of the mission scripts: loads a level and lets it run on its own, printing what the
// scripts do (cut scenes, dialogue lines, hints, objectives, the camera, the end).
//   dvscript <game> <level> <seconds> [outprefix every-seconds]
// DVSKIP=1 skips the intro as the player's tap does. DVMOVE=hero,x,y,tick orders a hero somewhere.
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "dv/DVArtificialIntelligence.h"
#include "dv/DVDialogue.h"
#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVScript.h"
#include "dv/DVPatch.h"
#include "dv/DVDoor.h"
#include "dv/DVSector.h"
#include "dv/DVSequence.h"
#include "dv/DVFastFindGrid.h"
#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"
#include "sb/SBFont.h"

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

static void savePPM(SBDrawManager& draw, const char* name) {
    SBDrawViewport vp;
    draw.GetSurfaceViewport(1, vp, VIEWPORT_READ);
    FILE* f = fopen(name, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", vp.width, vp.height);
    for (int y = 0; y < vp.height; ++y)
        for (int x = 0; x < vp.width; ++x) {
            uint16_t c = vp.data[(size_t)y * vp.width + x];
            unsigned char rgb[3] = {(unsigned char)((c >> 11) * 255 / 31), (unsigned char)((c >> 5 & 63) * 255 / 63),
                                    (unsigned char)((c & 31) * 255 / 31)};
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
    draw.ReleaseSurfaceViewport(vp);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "dvscript <game> <level> <seconds> [outprefix every-seconds]\n");
        return 1;
    }
    if (!initGL()) return 1;
    SBFile::SetGameRoot(argv[1]);
    SBDrawManager draw;
    int W = 1024, H = 640;
    draw.OpenScreen((uint16_t)W, (uint16_t)H);
    DVEngine engine(&draw);
    engine.screen = SBGeoVector2D((float)W, (float)H);
    DVSequenceManager::trace = getenv("DVSEQ") != nullptr;
    if (!engine.LoadStateFromFile(argv[2])) return 1;
    if (getenv("DVNAMES"))
        for (size_t i = 0; i < engine.Script()->marrayElement.size(); ++i) {
            DVElement* e = engine.Script()->marrayElement[i];
        {
            auto* a = dynamic_cast<DVElementActor*>(e);
            fprintf(stderr, "#%zu %s %s at %.0f,%.0f layer %u%s%s role %d\n", i + 1, e->KindName(), e->scriptName.c_str(),
                    e->sprite ? e->sprite->pos.posMap.x : 0.f, e->sprite ? e->sprite->pos.posMap.y : 0.f,
                    e->sprite ? e->sprite->pos.layer : 0, e->active ? "" : " (off)", a && a->InBuilding() ? " (in a house)" : "",
                    a && a->ai ? (int)a->ai->role : (a ? -1000 - (int)a->profile : -1));
        }
        }
    // DVANIMS=handle,...: the animations an element's sprite has (and their walking distance)
    if (const char* an = getenv("DVANIMS"))
        for (const char* q = an; q && *q; q = strchr(q, ',') ? strchr(q, ',') + 1 : nullptr) {
            DVElement* e = DVScript::Element(atoi(q));
            if (!e || !e->sprite) continue;
            fprintf(stderr, "#%d %s anims:", atoi(q), e->KindName());
            for (uint32_t a = 0; a < 0xb9; ++a)
                if (e->sprite->HasAnimation(a)) fprintf(stderr, " %x(%.0f)", a, e->sprite->GetDistanceForAnimation(a));
            fprintf(stderr, "\n");
        }
    // DVPATCHES: the level's patches; DVAPPLY=i applies one at the start
    if (getenv("DVPATCHES"))
        for (size_t i = 0; i < engine.Grid()->Patches().size(); ++i) {
            const DVPatch* p = engine.Grid()->Patches()[i].get();
            fprintf(stderr, "patch %zu at %.0f,%.0f layer %u: trigger %d permanent %d fx %d door %d zone %d masks %zu/%zu obstacles %zu/%zu\n", i,
                    p->at.x, p->at.y, p->layer, p->trigger, p->permanent, p->hasFx, p->door != nullptr, p->zone != nullptr,
                    p->masksOff.size(), p->masksOn.size(), p->obstaclesOff.size(), p->obstaclesOn.size());
        }
    if (const char* ap = getenv("DVAPPLY"))
        if (DVPatch* p = engine.Grid()->Patch((unsigned)atoi(ap))) {
            p->Apply();
            engine.camera = p->at - engine.screen * 0.25f;
        }
    // DVDOORS: the level's doors
    if (getenv("DVDOORS"))
        for (const auto& d : engine.Grid()->Doors())
            fprintf(stderr, "door type %u mid %.0f,%.0f A %.0f,%.0f (area %d layer %u) B %.0f,%.0f (area %d layer %u%s%s) plane %d\n", d->type,
                    d->mid.x, d->mid.y, d->pA.x, d->pA.y, d->a ? d->a->id : -1, d->layerA, d->pB.x, d->pB.y, d->b ? d->b->id : -1, d->layerB,
                    d->b && d->b->IsBuilding() ? ", a house" : "", d->b && d->b->IsLift() ? ", a lift" : "", d->plane ? 1 : 0);
    // DVOBS=handle: the sight obstacles around an actor
    if (const char* ob = getenv("DVOBS")) {
        auto* a = dynamic_cast<DVElementActor*>(DVScript::Element(atoi(ob)));
        if (a) {
            SBGeoPoint3D m, h, f;
            a->DetectionPoints(m, h, f);
            fprintf(stderr, "points mid %.1f,%.1f,%.1f head %.1f,%.1f,%.1f feet %.1f,%.1f,%.1f\n", m.x, m.y, m.z, h.x, h.y, h.z, f.x, f.y, f.z);
            SBGeoBoundingBox2D b;
            b.Expand(SBGeoPoint2D(m.x - 1, m.y - 1));
            b.Expand(SBGeoPoint2D(m.x + 1, m.y + 1));
            std::vector<DVSightObstacle*> obs;
            engine.Grid()->GetObstacles(obs, 0, b, 0);
            for (DVSightObstacle* o : obs) {
                bool in = o->polygon.IsInside_p(SBGeoPoint2D(m.x, m.y));
                fprintf(stderr, "  obstacle %u flags %u inside %d flat %d top %.1f ground %.1f box %.0f,%.0f-%.0f,%.0f\n", o->serial, o->flags, in,
                        o->flat, o->plane.zx * m.x + o->plane.zy * m.y + o->plane.zc,
                        o->planeGround.zx * m.x + o->planeGround.zy * m.y + o->planeGround.zc, o->box.p0.x, o->box.p0.y, o->box.p1.x,
                        o->box.p1.y);
            }
        }
    }
    // DVPFTEST=x,y: paths from the selected hero to points around (x, y) on its area
    if (const char* pt = getenv("DVPFTEST")) {
        float cx = 0, cy = 0;
        sscanf(pt, "%f,%f", &cx, &cy);
        DVElementActor* h = engine.Selected() ? engine.Selected() : (engine.Heroes().empty() ? nullptr : engine.Heroes()[0]);
        if (h) {
            if (cx || cy) h->Teleport(SBGeoPoint2D(cx, cy), h->sprite->pos.layer, engine.Grid()->FindMotionArea(h->sprite->pos.layer, SBGeoPoint2D(cx, cy)));
            int ok = 0, bad = 0, skip = 0;
            for (int dy = -400; dy <= 400; dy += 50)
                for (int dx = -600; dx <= 600; dx += 60) {
                    SBGeoPoint2D g(h->sprite->pos.posMap.x + dx, h->sprite->pos.posMap.y + dy);
                    SBGeoBoundingBox2D box = h->sprite->pos.GetMoveBoxMap() - h->sprite->pos.posMap + g;
                    if (!engine.Grid()->FindAutorizedPosition(box, g, h->sprite->pos.layer)) { ++skip; continue; }
                    g = SBCenter(box);
                    if (engine.Grid()->FindMotionArea(h->sprite->pos.layer, g) != h->Area()) { ++skip; continue; }
                    std::deque<DVOrder> q;
                    if (h->BuildMove(g, CMD_WALK, 0, q)) ++ok; else { ++bad; fprintf(stderr, "  fail to %.0f,%.0f\n", g.x, g.y); }
                }
            fprintf(stderr, "pftest: %d ok, %d failed, %d skipped\n", ok, bad, skip);
        }
    }
    SBFont font;
    bool haveFont = font.LoadFromFile("Data\\Interface\\Fonts\\dialog.fnt");
    int seconds = atoi(argv[3]);
    const char* out = argc > 5 ? argv[4] : nullptr;
    int every = argc > 5 ? atoi(argv[5]) : 0;
    int moveHero = -1, moveTick = -1;
    float mx = 0, my = 0;
    if (const char* m = getenv("DVMOVE")) sscanf(m, "%d,%f,%f,%d", &moveHero, &mx, &my, &moveTick);
    std::string lastText;
    uint32_t lastHint = 0;
    size_t lastSentences = 0;
    bool lastBriefing = false, lastLocked = false;
    int frame = 0;
    for (int t = 0; t <= seconds * 25; ++t) {
        if (t == 1 && getenv("DVSKIP") && engine.briefing) {
            fprintf(stderr, "t %d: skipping the intro\n", t);
            engine.SkipBriefing();
        }
        if (t == moveTick && moveHero >= 0 && (size_t)moveHero < engine.Heroes().size()) {
            engine.Select(engine.Heroes()[moveHero]);
            fprintf(stderr, "t %d: %s to %.0f,%.0f: %s\n", t, engine.Heroes()[moveHero]->KindName(), mx, my,
                    engine.OrderMove(SBGeoPoint2D(mx, my), false) ? "ok" : "refused");
        }
        // DVCMDS="tick:crouch;tick:move:x,y;tick:run:x,y" for the selected hero
        if (const char* c = getenv("DVCMDS")) {
            std::string all = c;
            size_t at = 0;
            while (at < all.size()) {
                size_t end = all.find(';', at);
                std::string one = all.substr(at, end == std::string::npos ? std::string::npos : end - at);
                at = end == std::string::npos ? all.size() : end + 1;
                int tick = atoi(one.c_str());
                if (tick != t) continue;
                float x = 0, y = 0;
                int hh = 0;
                if (sscanf(one.c_str(), "%*d:mount:%d", &hh) == 1) {
                    auto* h = dynamic_cast<DVElementActor*>(DVScript::Element(hh));
                    fprintf(stderr, "t %d: mount #%d: %s\n", t, hh, engine.OrderMount(h) ? "ok" : "refused");
                } else if (sscanf(one.c_str(), "%*d:take:%d", &hh) == 1) {
                    auto* h = dynamic_cast<DVElementActor*>(DVScript::Element(hh));
                    fprintf(stderr, "t %d: take #%d: %s\n", t, hh, engine.OrderTakeBody(h) ? "ok" : "refused");
                } else if (sscanf(one.c_str(), "%*d:kill:%d", &hh) == 1) {
                    if (auto* h = dynamic_cast<DVElementActor*>(DVScript::Element(hh))) h->GotBullet(1000, engine.Selected());
                    fprintf(stderr, "t %d: kill #%d\n", t, hh);
                } else if (sscanf(one.c_str(), "%*d:ko:%d", &hh) == 1) {
                    if (auto* h = dynamic_cast<DVElementActor*>(DVScript::Element(hh))) h->KnockHit(20, engine.Selected());
                    fprintf(stderr, "t %d: knock out #%d\n", t, hh);
                } else if (one.find(":crouch") != std::string::npos) {
                    engine.OrderCrouch();
                    fprintf(stderr, "t %d: crouch -> posture %u\n", t, engine.Selected() ? engine.Selected()->Posture() : 0);
                } else if (sscanf(one.c_str(), "%*d:move:%f,%f", &x, &y) == 2 || sscanf(one.c_str(), "%*d:run:%f,%f", &x, &y) == 2) {
                    bool run = one.find(":run:") != std::string::npos;
                    fprintf(stderr, "t %d: %s to %.0f,%.0f: %s\n", t, run ? "run" : "move", x, y,
                            engine.OrderMove(SBGeoPoint2D(x, y), run) ? "ok" : "refused");
                }
            }
        }
        engine.PerformHourglass();
        if (engine.briefing != lastBriefing || engine.userLocked != lastLocked) {
            fprintf(stderr, "t %d (%ds): briefing %d, player locked %d\n", t, t / 25, engine.briefing, engine.userLocked);
            lastBriefing = engine.briefing;
            lastLocked = engine.userLocked;
        }
        std::string text = SBToUtf8(engine.dialogue->Text());
        if (text != lastText) {
            if (!text.empty()) fprintf(stderr, "t %d (%ds): [%d] %s\n", t, t / 25, engine.dialogue->Speaker(), text.c_str());
            lastText = text;
        }
        if (engine.hint != lastHint) {
            fprintf(stderr, "t %d: hint %u: %s\n", t, engine.hint, SBToUtf8(engine.dialogue->HintText(engine.hint)).c_str());
            lastHint = engine.hint;
        }
        if (engine.sentences.size() != lastSentences) {
            for (size_t i = lastSentences; i < engine.sentences.size(); ++i)
                fprintf(stderr, "t %d: objective: %s\n", t, SBToUtf8(engine.dialogue->SentenceText(engine.sentences[i].id)).c_str());
            lastSentences = engine.sentences.size();
        }
        if (engine.cameraMoved) {
            engine.cameraMoved = false;
            if (getenv("DVCAMLOG")) fprintf(stderr, "t %d: camera %.0f,%.0f%s\n", t, engine.camera.x, engine.camera.y, engine.follow ? " (following)" : "");
        }
        if (out && every > 0 && t % (every * 25) == 0) {
            engine.Draw();
            if (haveFont && !text.empty()) {
                auto lines = font.Wrap(engine.dialogue->Text(), W - 40);
                int y = 10;
                for (auto& l : lines) {
                    font.Draw(1, 20, y, l, true);
                    y += font.Height();
                }
            }
            char name[512];
            snprintf(name, sizeof name, "%s%03d.ppm", out, frame++);
            draw.Finish();
            savePPM(draw, name);
        }
        if (getenv("DVWHO") && t % (25 * atoi(getenv("DVWHO"))) == 0)
            for (DVElementActor* a : engine.Characters())
                if (a->active && (a->IsHero() || !a->orders.empty() || a->scriptElement || !a->scriptName.empty()))
                    fprintf(stderr, "t %d:   %s #%d at %.0f,%.0f orders %zu%s%s\n", t, a->KindName(), DVScript::Handle(a),
                            a->sprite->pos.posMap.x, a->sprite->pos.posMap.y, a->orders.size(), a->scriptElement ? " (script)" : "",
                            a->ai && a->ai->locked ? " locked" : "");
        // DVWATCH=handle[,handle]: an NPC's brain every second
        if (const char* w = getenv("DVWATCH"))
            if (t % 25 == 0)
                for (const char* q = w; q && *q; q = strchr(q, ',') ? strchr(q, ',') + 1 : nullptr) {
                    auto* a = dynamic_cast<DVElementActor*>(DVScript::Element(atoi(q)));
                    if (a && a->ai)
                        fprintf(stderr, "t %d: #%d %s at %.0f,%.0f state %u/%u att %u alert %u suspect %u locked %d active %d cmd 0x%x posture %u life %d\n", t,
                                atoi(q), a->KindName(), a->sprite->pos.posMap.x, a->sprite->pos.posMap.y, a->ai->state,
                                a->ai->substate, a->ai->attitude, a->ai->alertStatus, a->ai->suspect, a->ai->locked, a->active,
                                a->CurrentCommand(), a->Posture(), a->life);
                    if (a && a->ai) fprintf(stderr, "        view len %u radius %u status %u dir %.2f,%.2f\n", a->view.length, a->view.radius, a->view.status, a->view.dir.x, a->view.dir.y);
                    if (a) fprintf(stderr, "        shadow zone %d (night %d)\n", a->ShadowZone(), engine.night);
                    if (a && !a->ai)
                        fprintf(stderr, "t %d: #%d %s at %.0f,%.0f layer %u active %d cmd 0x%x posture %u life %d orders %zu%s\n", t, atoi(q),
                                a->KindName(), a->sprite->pos.posMap.x, a->sprite->pos.posMap.y, a->sprite->pos.layer, a->active,
                                a->CurrentCommand(), a->Posture(), a->life, a->orders.size(), a->scriptElement ? " (script)" : "");
                }
        if (engine.missionResult || engine.missionFailed) {
            fprintf(stderr, "t %d: mission %s\n", t, engine.missionResult == 1 ? "ACCOMPLISHED" : "FAILED");
            break;
        }
    }
    // DVPENDING: the sequences still waiting, and on what
    if (getenv("DVPENDING") && DVSequenceManager::mpInstance)
        for (auto& sq : DVSequenceManager::mpInstance->Sequences()) {
            if (sq->Finished()) continue;
            fprintf(stderr, "pending sequence %p: %zu/%zu elements, %d running\n", (void*)sq.get(), sq->next, sq->elements.size(), sq->pending);
            for (auto& el : sq->elements)
                if (el->state == SEQ_RUNNING || el->state == SEQ_WAITING)
                    fprintf(stderr, "   lvl %u cmd 0x%x state %d actor %s #%d at %.0f,%.0f\n", el->level, el->command, el->state,
                            el->actor ? el->actor->KindName() : "-", DVScript::Handle(el->actor), el->point.x, el->point.y);
        }
    int locked = 0, onMap = 0;
    for (DVElementActor* n : engine.NPCs()) {
        if (n->ai && n->ai->locked) ++locked;
        if (n->active) ++onMap;
    }
    fprintf(stderr, "end: %zu NPCs, %d on the map, %d locked; camera %.0f,%.0f; selected %s\n", engine.NPCs().size(), onMap,
            locked, engine.camera.x, engine.camera.y, engine.Selected() ? engine.Selected()->KindName() : "-");
    return 0;
}
