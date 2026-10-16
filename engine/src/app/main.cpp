// The app: window, GL context, the player's game folder, then a mission run by the engine.
// Drag / flick / pinch move the view. Tap a hero to select him, tap the ground to walk there,
// tap twice to run (the original's click / double click), tap an NPC to show / hide his view
// cone. The buttons on the left select the heroes (with their life), the ones on the right make
// the hero lie down / stand up and raise his gun (then a tap on an enemy shoots, a double tap on
// the gun reloads). Arrows at the top switch between the missions. The scripts' cut scenes show
// black bars and a Skip button; a tap says the dialogue's next line.
#include <SDL.h>

#include <GLES3/gl3.h>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "app/Camera.h"
#include "app/GameUI.h"
#include "app/Overlay.h"
#include "dv/DVArmsTrader.h"
#include "dv/DVDialogue.h"
#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "dv/DVMobile.h"
#include "dv/DVSprite.h"
#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"
#include "sb/SBSound.h"
#include "app/Mixer.h"
#include "app/Menus.h"

static std::string findGameFolder(int argc, char** argv) {
    std::vector<std::string> tries;
    if (argc > 1) tries.push_back(argv[1]);
#ifdef __ANDROID__
    const char* roots[] = {"/storage/emulated/0", "/sdcard"};
    const char* names[] = {"Desperados", "desperados", "Desperados Wanted Dead or Alive", "DesperadosWDOA"};
    for (const char* r : roots)
        for (const char* n : names) tries.push_back(std::string(r) + "/" + n);
#else
    tries.push_back(".");
#endif
    for (const std::string& t : tries) {
        SBFile::SetGameRoot(t);
        if (SBFile::Exists("Data\\Levels\\level_01.dvd")) return t;
    }
    return "";
}

struct Touch {
    SDL_FingerID id;
    float x, y;
    float x0, y0;   // where it went down
    Uint32 t0;      // when
};

// ---- the voices (SBSound): a WAV at a time, on its own audio device -------------------------------
static SDL_AudioDeviceID gVoiceDev = 0;
static void StopVoice() {
    if (gVoiceDev) {
        SDL_CloseAudioDevice(gVoiceDev);
        gVoiceDev = 0;
    }
}
static void PlayVoice(const std::string& path) {
    StopVoice();
    SDL_AudioSpec spec;
    Uint8* buf = nullptr;
    Uint32 len = 0;
    if (!SDL_LoadWAV(path.c_str(), &spec, &buf, &len)) {
        SBLog("voice %s: %s", path.c_str(), SDL_GetError());
        return;
    }
    gVoiceDev = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
    if (gVoiceDev) {
        SDL_QueueAudio(gVoiceDev, buf, len);
        SDL_PauseAudioDevice(gVoiceDev, 0);
    } else {
        SBLog("audio: %s", SDL_GetError());
    }
    SDL_FreeWAV(buf);
}

// the app's own buttons (until the game's interface comes)
struct UIButton {
    float x, y, w, h;
    int action;     // 1..6 select hero, 100 lie down / stand up
    std::string label;
    bool on;
};

// the gun: on, a tap on an enemy shoots at him (the original's shoot tool)
static bool gAimMode = false;

static std::vector<UIButton> layoutButtons(DVEngine* engine, int sw, int sh) {
    std::vector<UIButton> b;
    if (!engine) return b;
    if (engine->missionFailed || engine->missionResult) return b;
    float bh = sh * 0.085f, bw = sh * 0.24f, gap = sh * 0.015f;
    // the pause menu
    float s = sh / 600.0f;
    b.push_back({10 * s, 10 * s, 100 * s, 34 * s, 200, "MENU", false});
    if (engine->briefing) return b;
    float y = sh * 0.16f;
    const auto& heroes = engine->Heroes();
    for (size_t i = 0; i < heroes.size(); ++i) {
        if (!heroes[i]->playable) continue;
        std::string name = heroes[i]->KindName();
        for (char& c : name) c = (char)toupper((unsigned char)c);
        name += " " + std::to_string(std::max<int>(0, heroes[i]->life));
        b.push_back({gap, y, bw, bh, (int)i + 1, name, heroes[i] == engine->Selected()});
        y += bh + gap;
    }
    if (engine->Selected()) {
        bool lying = engine->Selected()->Posture() == POSTURE_LYING;
        bool riding = engine->Selected()->Horse() != nullptr;
        bool carrying = engine->Selected()->carried != nullptr;
        b.push_back({sw - bw - gap, sh * 0.5f - bh * 0.5f, bw, bh, 100,
                     carrying ? "DROP BODY" : riding ? "GET OFF" : lying ? "STAND UP" : "GET DOWN", lying || riding || carrying});
        DVElementActor* h = engine->Selected();
        if (h->firearm) {
            std::string g = "GUN " + std::to_string(std::max<int>(0, h->ammo)) + "/" + std::to_string(h->firearm->magazine);
            b.push_back({sw - bw - gap, sh * 0.5f - bh * 1.5f - gap, bw, bh, 101, g, gAimMode});
        }
    }
    return b;
}

static const UIButton* hitButton(const std::vector<UIButton>& bs, float x, float y) {
    for (const UIButton& b : bs)
        if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) return &b;
    return nullptr;
}

int main(int argc, char** argv) {
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0 && SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SBLog("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    std::string game = findGameFolder(argc, argv);
    if (!game.empty()) SBLogToFile(game + "/engine-log.txt");
    SBLog("Desperados engine: game folder '%s'", game.c_str());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __ANDROID__
    flags |= SDL_WINDOW_FULLSCREEN;
#endif
    // the mixer plays the voices, the effects and the music; without it, the voices alone
    if (!MixerOpen()) {
        SBSound::playVoice = PlayVoice;
        SBSound::stopVoice = StopVoice;
    }
    SDL_Window* window = SDL_CreateWindow("Desperados", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags);
    if (!window) {
        SBLog("SDL_CreateWindow: %s", SDL_GetError());
        return 1;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) {
        SBLog("SDL_GL_CreateContext: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    int sw = 0, sh = 0;
    SDL_GL_GetDrawableSize(window, &sw, &sh);

    Overlay overlay;
    overlay.Init();
    GameUI ui;
    bool uiReady = false;
    if (game.empty()) {
        // no game files: say where they go
        bool run = true;
        while (run) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                if (ev.type == SDL_QUIT || (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_AC_BACK)) run = false;
            SDL_GL_GetDrawableSize(window, &sw, &sh);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, sw, sh);
            glClearColor(0.15f, 0.05f, 0.05f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            overlay.Begin(sw, sh);
            overlay.Text("NO GAME FOLDER: COPY THE GAME TO INTERNAL STORAGE/DESPERADOS", sw * 0.5f, sh * 0.5f, sh * 0.04f, true);
            SDL_GL_SwapWindow(window);
            SDL_Delay(50);
        }
        SDL_Quit();
        return 0;
    }

    // the missions there are
    std::vector<std::string> levels;
    for (int i = 1; i <= 40; ++i) {
        char n[32];
        snprintf(n, sizeof n, "level_%02d", i);
        if (SBFile::Exists(std::string("Data\\Levels\\") + n + ".dvd")) levels.push_back(n);
    }
    SBLog("%zu missions", levels.size());

    SBDrawManager draw;
    std::unique_ptr<DVEngine> engine;
    Camera cam;
    size_t levelIndex = 0;
    std::string loadError;

    auto engineScreen = [&](int& w, int& h) {
        w = (int)std::ceil(cam.ViewW(sw, sh)) + 2;
        h = (int)std::ceil(cam.viewH) + 2;
    };
    auto loadLevel = [&](size_t i) {
        engine.reset();
        MixerFlushCache();
        draw.CloseScreen();
        int ew, eh;
        engineScreen(ew, eh);
        draw.OpenScreen((uint16_t)ew, (uint16_t)eh);
        engine.reset(new DVEngine(&draw));
        engine->screen = SBGeoVector2D((float)ew, (float)eh);
        Uint32 t0 = SDL_GetTicks();
        bool ok = engine->LoadStateFromFile(levels[i]);
        SBLog("%s loaded in %u ms: %s, %zu elements", levels[i].c_str(), SDL_GetTicks() - t0, ok ? "ok" : "FAILED",
              engine->Elements().size());
        loadError = ok ? "" : "COULD NOT LOAD " + levels[i];
        cam.x = engine->camera.x;
        cam.y = engine->camera.y;
        cam.Stop();
        engine->cameraMoved = false;
        levelIndex = i;
        ui.showObjectives = false;
    };
    if (!uiReady) uiReady = ui.Init() || true;
    Menus menus;
    menus.Init(game);
    auto applyVolumes = [&]() {
        if (SBSound::setVolumes)
            SBSound::setVolumes(menus.progress.fx / 10.0f, menus.progress.music / 10.0f, menus.progress.voice / 10.0f);
    };
    applyVolumes();
    menus.Open(Menus::MAIN);
#ifndef __ANDROID__
    if (const char* l = getenv("DVLEVEL")) {
        if (!levels.empty()) loadLevel((size_t)std::max(0, atoi(l) - 1) % levels.size());
        menus.screen = Menus::NONE;
    }
#endif
    int lastAutoTick = 0;
    // a mission of the campaign / of the folder
    auto levelOf = [&](const std::string& name) -> int {
        for (size_t i = 0; i < levels.size(); ++i)
            if (levels[i] == name) return (int)i;
        return -1;
    };
    auto startMission = [&](int m) {
        int li = m >= 0 && m < (int)menus.campaign.size() ? levelOf(menus.campaign[(size_t)m].level) : -1;
        if (li < 0) return;
        gAimMode = false;
        StopVoice();
        loadLevel((size_t)li);
        lastAutoTick = 0;
        menus.screen = Menus::NONE;
    };
    auto inMission = [&]() { return engine && !engine->missionFailed && !engine->missionResult && !engine->Replaying(); };
    auto autosave = [&]() {
        if (inMission() && menus.screen != Menus::REPLAY) {
            engine->SaveGame(menus.SlotPath(0), "");
            lastAutoTick = engine->ticks;
        }
    };
    auto loadSlot = [&](int slot) {
        DVEngine::SaveInfo si;
        if (!DVEngine::ReadSave(menus.SlotPath(slot), si)) {
            menus.message = "Could not read the saved game";
            menus.messageUntil = SDL_GetTicks() + 2500;
            return;
        }
        int li = levelOf(si.level);
        if (li < 0) return;
        gAimMode = false;
        StopVoice();
        loadLevel((size_t)li);
        engine->StartReplay(si);
        menus.replayProgress = 0;
        menus.screen = Menus::REPLAY;
    };
    auto toMain = [&]() {
        autosave();
        engine.reset();
        MixerStopAll();
        menus.Open(Menus::MAIN);
    };

    std::vector<Touch> touches;
    bool tapPossible = false;
    Uint32 lastHeroTap = 0, lastGroundTap = 0;
    float lastTapX = 0, lastTapY = 0;
    // a tap on the map: a hero selects him; the ground orders the selected hero there; a second tap
    // at the same place soon after makes him run (DVEngine::ManageInputProcessLeftDoubleClick)
    auto onTap = [&](const SBGeoPoint2D& mp, float sx, float sy) {
        if (engine->missionResult || engine->missionFailed) return;
        // a dialogue in a cut scene: the next line (DVDialogue::ProcessInput)
        if (engine->dialogue && engine->dialogue->Playing() && (engine->briefing || engine->userLocked)) {
            engine->PlayerSkipLine();
            return;
        }
        if (engine->briefing) return;
        float slack = 6.0f;
        DVElementActor* sel = engine->Selected();
        bool carrier = sel && sel->CanCarry() && !sel->carried && !sel->Horse() && !gAimMode;
        if (DVElementActor* h = engine->HeroAt(mp, slack)) {
            // a hero out cold: Cooper or Sanchez picks him up
            if (carrier && h != sel && h->IsCarriable() && engine->OrderTakeBody(h)) return;
            if (h != engine->Selected()) {
                engine->PlayerSelect(h);
                return;
            }
        }
        // an enemy or a civilian: with the gun up, shoot at him; else show / hide his view cone
        // (the original's "watch" tool)
        // with the gun up, a target of the mission (the pot of mission 1)
        if (gAimMode)
            if (DVElement* t = engine->ShootableTargetAt(mp, slack)) {
                engine->OrderShoot(t);
                return;
            }
        if (DVElementActor* n = engine->NPCAt(mp, slack)) {
            if (carrier && n->IsCarriable() && engine->OrderTakeBody(n)) return;
            if (gAimMode && !n->dead) engine->OrderShoot(n);
            else engine->ToggleWatched(n);
            return;
        }
        // a cart: Sanchez pushes it (DVElementMobile::MouseClicked)
        if (sel && sel->kind == KIND_SANCHEZ && !gAimMode)
            if (DVElementMobile* m = engine->PushableAt(mp, slack)) {
                engine->OrderPush(m);
                return;
            }
        // a horse: get on it
        if (engine->Selected() && !engine->Selected()->Horse())
            if (DVElementActor* h = engine->HorseAt(mp, 2.0f)) {
                engine->OrderMount(h);
                return;
            }
        Uint32 now = SDL_GetTicks();
        bool twice = now - lastGroundTap < 400 && std::hypot(sx - lastTapX, sy - lastTapY) < sh * 0.06f;
        if (twice && engine->Selected() && engine->Selected()->IsInMotion()) {
            engine->OrderMakeRunning();
            lastGroundTap = 0;
            return;
        }
        engine->OrderMove(mp, twice);
        lastGroundTap = now;
        lastTapX = sx;
        lastTapY = sy;
    };
    // the app's buttons: true if (x, y) was on one
    auto pressButton = [&](float x, float y) -> bool {
        int g = ui.HitButton(x, y, engine.get(), sw, sh);
        if (g == 1) {
            StopVoice();
            engine->SkipBriefing();
            return true;
        }
        if (g == 2) {
            ui.showObjectives = !ui.showObjectives;
            return true;
        }
        std::vector<UIButton> bs = layoutButtons(engine.get(), sw, sh);
        const UIButton* b = hitButton(bs, x, y);
        if (!b) return false;
        if (b->action >= 1 && b->action <= 6) {
            DVElementActor* h = engine->Heroes()[(size_t)b->action - 1];
            if (h == engine->Selected() && SDL_GetTicks() - lastHeroTap < 400) {
                // twice: look at him
                cam.x = h->sprite->pos.posMap.x - cam.ViewW(sw, sh) * 0.5f;
                cam.y = h->sprite->pos.posMap.y - cam.viewH * 0.5f;
                cam.Stop();
            }
            engine->PlayerSelect(h);
            lastHeroTap = SDL_GetTicks();
        } else if (b->action == 200) {
            menus.Open(Menus::PAUSE);
            autosave();
        } else if (b->action == 100) {
            engine->OrderCrouch();
        } else if (b->action == 101) {
            // the gun: a tap raises / lowers it, a double tap reloads
            static Uint32 lastGun = 0;
            Uint32 now = SDL_GetTicks();
            if (now - lastGun < 400) {
                engine->OrderReload();
                gAimMode = true;
                lastGun = 0;
            } else {
                gAimMode = !gAimMode;
                lastGun = now;
            }
        }
        return true;
    };
    bool run = true, paused = false;
    auto menuTap = [&](float x, float y) {
        Menus::Action a = menus.Tap(x, y, engine.get(), sw, sh);
        switch (a.act) {
        case Menus::RESUME: menus.screen = Menus::NONE; break;
        case Menus::PLAY_MISSION: startMission(a.arg); break;
        case Menus::LOAD_SLOT: loadSlot(a.arg); break;
        case Menus::SAVE_SLOT:
            if (engine) {
                bool ok = engine->SaveGame(menus.SlotPath(a.arg), "");
                menus.message = ok ? "Game saved" : "Could not save the game";
                menus.messageUntil = SDL_GetTicks() + 2000;
                menus.screen = Menus::NONE;
            }
            break;
        case Menus::RESTART:
            if (engine) {
                gAimMode = false;
                StopVoice();
                loadLevel(levelIndex);
                lastAutoTick = 0;
                menus.screen = Menus::NONE;
            }
            break;
        case Menus::TO_MAIN: toMain(); break;
        case Menus::QUIT: run = false; break;
        case Menus::VOLUMES: applyVolumes(); break;
        case Menus::CONTINUE:
            if (menus.HasSlot(0)) loadSlot(0);
            else {
                menus.mapMission = std::max(0, std::min((int)menus.campaign.size() - 1, menus.progress.unlocked - 1));
                menus.screen = Menus::MAP;
            }
            break;
        case Menus::NEXT_MISSION: {
            int m = engine ? menus.MissionOfLevel(engine->levelName) : -1;
            engine.reset();
            MixerStopAll();
            if (m + 1 >= (int)menus.campaign.size()) menus.Open(Menus::THE_END);
            else {
                menus.mapMission = m + 1;
                menus.Open(Menus::MAP);
            }
            break;
        }
        default: break;
        }
    };
    auto backKey = [&]() {
        if (menus.screen == Menus::NONE) {
            if (engine) {
                menus.Open(Menus::PAUSE);
                autosave();
            }
        } else if (menus.screen == Menus::MAIN) {
            run = false;
        } else if (menus.screen == Menus::VICTORY || menus.screen == Menus::FAILED || menus.screen == Menus::REPLAY) {
        } else {
            menus.Back(engine != nullptr);
        }
    };
    float pinchDist = 0, lastCx = 0, lastCy = 0;
    Uint32 lastMoveT = 0;
    float speedX = 0, speedY = 0;
    double tickAcc = 0;
    Uint64 prev = SDL_GetPerformanceCounter();
    while (run) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: run = false; break;
            case SDL_APP_WILLENTERBACKGROUND:
                paused = true;
                MixerPause(true);
                autosave();
                break;
            case SDL_APP_DIDENTERFOREGROUND: paused = false; MixerPause(false); prev = SDL_GetPerformanceCounter(); break;
            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_AC_BACK || ev.key.keysym.sym == SDLK_ESCAPE) backKey();
                if (ev.key.keysym.sym == SDLK_RIGHT && !levels.empty() && engine) loadLevel((levelIndex + 1) % levels.size()), menus.screen = Menus::NONE;
                if (ev.key.keysym.sym == SDLK_LEFT && !levels.empty() && engine) loadLevel((levelIndex + levels.size() - 1) % levels.size()), menus.screen = Menus::NONE;
                if (ev.key.keysym.sym == SDLK_CAPSLOCK && engine) engine->silhouettes = !engine->silhouettes;
                break;
            case SDL_FINGERDOWN: {
                float x = ev.tfinger.x * sw, y = ev.tfinger.y * sh;
                if (menus.screen != Menus::NONE) {
                    if (touches.empty()) menuTap(x, y);
                    break;
                }
                if (touches.empty() && engine && pressButton(x, y)) break;
                touches.push_back({ev.tfinger.fingerId, x, y, x, y, SDL_GetTicks()});
                if (touches.size() > 1) tapPossible = false;
                else tapPossible = true;
                cam.dragging = true;
                cam.Stop();
                speedX = speedY = 0;
                lastMoveT = SDL_GetTicks();
                if (touches.size() == 2) {
                    pinchDist = std::hypot(touches[0].x - touches[1].x, touches[0].y - touches[1].y);
                    lastCx = (touches[0].x + touches[1].x) * 0.5f;
                    lastCy = (touches[0].y + touches[1].y) * 0.5f;
                }
                break;
            }
            case SDL_FINGERMOTION: {
                float x = ev.tfinger.x * sw, y = ev.tfinger.y * sh;
                for (Touch& t : touches)
                    if (t.id == ev.tfinger.fingerId) {
                        if (touches.size() == 1) {
                            float dx = x - t.x, dy = y - t.y;
                            cam.DragBy(dx, dy, sh);
                            Uint32 now = SDL_GetTicks();
                            float dt = (now - lastMoveT) / 1000.0f;
                            if (dt > 0.001f) {
                                float k = dt / (dt + 0.05f);  // smoothed finger speed
                                speedX = speedX * (1 - k) + dx / dt * k;
                                speedY = speedY * (1 - k) + dy / dt * k;
                            }
                            lastMoveT = now;
                        }
                        t.x = x;
                        t.y = y;
                    }
                if (touches.size() == 2) {
                    float d = std::hypot(touches[0].x - touches[1].x, touches[0].y - touches[1].y);
                    float cx = (touches[0].x + touches[1].x) * 0.5f, cy = (touches[0].y + touches[1].y) * 0.5f;
                    cam.DragBy(cx - lastCx, cy - lastCy, sh);
                    if (pinchDist > 1 && d > 1) cam.ZoomAt(d / pinchDist, cx, cy, sw, sh);
                    pinchDist = d;
                    lastCx = cx;
                    lastCy = cy;
                }
                break;
            }
            case SDL_FINGERUP: {
                bool single = touches.size() == 1;
                if (single && tapPossible && engine && touches[0].id == ev.tfinger.fingerId) {
                    const Touch& t = touches[0];
                    float moved = std::hypot(t.x - t.x0, t.y - t.y0);
                    Uint32 held = SDL_GetTicks() - t.t0;
                    if (moved < sh * 0.03f && held < 450) {
                        // a tap: undo the little drag it made, then act at that point of the map
                        cam.DragBy(-(t.x - t.x0), -(t.y - t.y0), sh);
                        float s = cam.ScreenScale(sh);
                        SBGeoPoint2D mp(cam.x + t.x0 / s, cam.y + t.y0 / s);
                        onTap(mp, t.x0, t.y0);
                        speedX = speedY = 0;
                        lastMoveT = 0;
                    }
                }
                for (size_t i = 0; i < touches.size(); ++i)
                    if (touches[i].id == ev.tfinger.fingerId) {
                        touches.erase(touches.begin() + i);
                        break;
                    }
                if (touches.empty()) {
                    cam.dragging = false;
                    if (single && SDL_GetTicks() - lastMoveT < 80) cam.Release(speedX, speedY, sh);
                } else if (touches.size() == 1) {
                    lastMoveT = SDL_GetTicks();
                    speedX = speedY = 0;
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.which != SDL_TOUCH_MOUSEID) {
                    if (menus.screen != Menus::NONE) {
                        menuTap((float)ev.button.x, (float)ev.button.y);
                    } else if (engine && pressButton((float)ev.button.x, (float)ev.button.y)) {
                    } else if (engine && ev.button.button == SDL_BUTTON_RIGHT) {
                        engine->OrderStop();
                    } else if (engine && ev.button.button == SDL_BUTTON_LEFT && (SDL_GetModState() & KMOD_SHIFT)) {
                        float s = cam.ScreenScale(sh);
                        onTap(SBGeoPoint2D(cam.x + ev.button.x / s, cam.y + ev.button.y / s), (float)ev.button.x,
                              (float)ev.button.y);
                    } else {
                        cam.dragging = true;
                        cam.Stop();
                    }
                }
                break;
            case SDL_MOUSEMOTION:
                if (ev.motion.which != SDL_TOUCH_MOUSEID && (ev.motion.state & SDL_BUTTON_LMASK))
                    cam.DragBy((float)ev.motion.xrel, (float)ev.motion.yrel, sh);
                break;
            case SDL_MOUSEBUTTONUP:
                if (ev.button.which != SDL_TOUCH_MOUSEID) cam.dragging = false;
                break;
            case SDL_MOUSEWHEEL: {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                cam.ZoomAt(ev.wheel.y > 0 ? 1.1f : 1 / 1.1f, (float)mx, (float)my, sw, sh);
                break;
            }
            }
        }
        if (paused) {
            SDL_Delay(50);
            continue;
        }
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - prev) / (double)SDL_GetPerformanceFrequency();
        prev = now;
        if (dt > 0.1) dt = 0.1;
        SDL_GL_GetDrawableSize(window, &sw, &sh);

        // a saved game played again: as much as fits in a frame
        if (engine && menus.screen == Menus::REPLAY) {
            Uint32 t0 = SDL_GetTicks();
            while (engine->Replaying() && SDL_GetTicks() - t0 < 30) engine->ReplaySome(10);
            menus.replayProgress = engine->ReplayProgress();
            if (!engine->Replaying()) {
                menus.screen = Menus::NONE;
                cam.x = engine->camera.x;
                cam.y = engine->camera.y;
                cam.Stop();
                engine->cameraMoved = false;
                lastAutoTick = engine->ticks;
            }
        }
        // the end of the mission
        if (engine && menus.screen == Menus::NONE) {
            if (engine->missionResult == 1) {
                menus.Accomplished(menus.MissionOfLevel(engine->levelName));
                remove(menus.SlotPath(0).c_str());
                menus.Open(Menus::VICTORY);
            } else if (engine->missionFailed || engine->missionResult == 2) {
                std::string who = engine->deadHero ? engine->deadHero->KindName() : "";
                menus.failedWho = who;
                menus.Open(Menus::FAILED);
            } else if (engine->ticks - lastAutoTick >= 25 * 60) {
                autosave();
            }
        }
        bool showGame = engine && (menus.screen == Menus::NONE || menus.screen == Menus::PAUSE ||
                                   menus.screen == Menus::VICTORY || menus.screen == Menus::FAILED ||
                                   menus.screen == Menus::REPLAY ||
                                   ((menus.screen == Menus::OPTIONS || menus.screen == Menus::LOAD ||
                                     menus.screen == Menus::SAVE) && menus.returnTo != Menus::MAIN));
        if (showGame && menus.screen != Menus::REPLAY) {
            cam.Update((float)dt, engine->mapW, engine->mapH, sw, sh);
            // the engine's screen follows the zoom
            int ew, eh;
            engineScreen(ew, eh);
            if (ew != draw.ScreenWidth() || eh != draw.ScreenHeight()) {
                draw.ResizeScreen((uint16_t)ew, (uint16_t)eh);
                engine->screen = SBGeoVector2D((float)ew, (float)eh);
            }
            // the original's 25 Hz tick
            tickAcc += menus.screen == Menus::NONE ? dt : 0;
            int steps = 0;
            while (tickAcc >= 0.04 && steps < 4) {
                engine->PerformHourglass();
                tickAcc -= 0.04;
                ++steps;
            }
            if (tickAcc > 0.04) tickAcc = 0;
            // the scripts moved the camera / asked for a zoom (2 near, 1, 0.5 far)
            if (engine->zoomWanted > 0) {
                float cx = cam.x + cam.ViewW(sw, sh) * 0.5f, cy = cam.y + cam.viewH * 0.5f;
                cam.viewH = std::max(cam.minViewH, std::min(cam.maxViewH, 640.0f / engine->zoomWanted));
                cam.x = cx - cam.ViewW(sw, sh) * 0.5f;
                cam.y = cy - cam.viewH * 0.5f;
                engine->zoomWanted = -1;
            }
            // the cut scenes were framed for the original's 768 lines: the view is at least that
            // tall while they play, and the player's zoom comes back after
            static float savedViewH = 0;
            if (engine->briefing && savedViewH == 0) {
                savedViewH = cam.viewH;
                if (cam.viewH < 768) {
                    float cx = cam.x + cam.ViewW(sw, sh) * 0.5f, cy = cam.y + cam.viewH * 0.5f;
                    cam.viewH = 768;
                    cam.x = cx - cam.ViewW(sw, sh) * 0.5f;
                    cam.y = cy - cam.viewH * 0.5f;
                    engine->screen = SBGeoVector2D(cam.ViewW(sw, sh), cam.viewH);
                }
            } else if (!engine->briefing && savedViewH > 0) {
                float cx = cam.x + cam.ViewW(sw, sh) * 0.5f, cy = cam.y + cam.viewH * 0.5f;
                cam.viewH = savedViewH;
                cam.x = cx - cam.ViewW(sw, sh) * 0.5f;
                cam.y = cy - cam.viewH * 0.5f;
                savedViewH = 0;
            }
            if (engine->cameraMoved) {
                cam.x = engine->camera.x;
                cam.y = engine->camera.y;
                cam.Stop();
                engine->cameraMoved = false;
            }
            engine->camera = SBGeoPoint2D(std::floor(cam.x), std::floor(cam.y));
            engine->Draw();
            draw.Finish();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, sw, sh);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        overlay.Begin(sw, sh);
        if (showGame && menus.screen != Menus::REPLAY) {
            // the engine's picture, scaled, shifted by the camera's fraction for smooth scrolling
            float s = cam.ScreenScale(sh);
            float fx = cam.x - std::floor(cam.x), fy = cam.y - std::floor(cam.y);
            overlay.Picture(draw.ScreenTexture(), draw.ScreenWidth(), draw.ScreenHeight(), -fx * s, -fy * s,
                            draw.ScreenWidth() * s, draw.ScreenHeight() * s);
        }
        bool hud = engine && menus.screen == Menus::NONE;
        if (hud) ui.Draw(overlay, engine.get(), sw, sh);
        for (const UIButton& b : layoutButtons(hud ? engine.get() : nullptr, sw, sh)) {
            overlay.Rect(b.x, b.y, b.w, b.h, b.on ? 0.25f : 0.0f, b.on ? 0.35f : 0.0f, 0.0f, 0.55f);
            overlay.Text(b.label.c_str(), b.x + b.w * 0.5f, b.y + b.h * 0.5f, b.h * 0.32f, true);
        }
        if (!loadError.empty()) overlay.Text(loadError.c_str(), sw * 0.5f, sh * 0.5f, sh * 0.04f, true);
        menus.Draw(overlay, ui, engine.get(), sw, sh);
#ifndef __ANDROID__
        // desktop check: DVTAPS=ms:x,y;ms:x,y taps the menus / the buttons at those times
        if (const char* taps = getenv("DVTAPS")) {
            static Uint32 start = SDL_GetTicks();
            static size_t next = 0;
            std::vector<std::string> list;
            for (const char* q = taps; *q;) {
                const char* e = strchr(q, ';');
                list.push_back(std::string(q, e ? (size_t)(e - q) : strlen(q)));
                q = e ? e + 1 : q + strlen(q);
            }
            if (next < list.size()) {
                unsigned ms = 0;
                float tx = 0, ty = 0;
                if (sscanf(list[next].c_str(), "%u", &ms) == 1 && SDL_GetTicks() - start >= ms && engine &&
                    (list[next].find(":win") != std::string::npos || list[next].find(":fail") != std::string::npos)) {
                    ++next;
                    if (list[next - 1].find(":win") != std::string::npos) engine->missionResult = 1;
                    else engine->missionFailed = true;
                } else if (sscanf(list[next].c_str(), "%u:%f,%f", &ms, &tx, &ty) == 3 && SDL_GetTicks() - start >= ms) {
                    ++next;
                    SBLog("test tap at %.0f,%.0f", tx, ty);
                    if (menus.screen != Menus::NONE) menuTap(tx, ty);
                    else if (engine && !pressButton(tx, ty)) {
                        float sc = cam.ScreenScale(sh);
                        onTap(SBGeoPoint2D(cam.x + tx / sc, cam.y + ty / sc), tx, ty);
                    }
                }
            }
        }
        // desktop check: DVSHOT=seconds,picture.ppm[,level] writes the window after that long and quits
        if (const char* shot = getenv("DVSHOT")) {
            static Uint32 start = SDL_GetTicks();
            float secs = (float)atof(shot);
            if (SDL_GetTicks() - start > (Uint32)(secs * 1000)) {
                std::vector<unsigned char> px((size_t)sw * sh * 4);
                glReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                const char* name = strchr(shot, ',');
                if (FILE* f = fopen(name ? name + 1 : "shot.ppm", "wb")) {
                    fprintf(f, "P6\n%d %d\n255\n", sw, sh);
                    for (int y = sh - 1; y >= 0; --y)
                        for (int x = 0; x < sw; ++x) fwrite(&px[((size_t)y * sw + x) * 4], 1, 3, f);
                    fclose(f);
                }
                run = false;
            }
        }
#endif
        SDL_GL_SwapWindow(window);
    }
    autosave();
    StopVoice();
    MixerStopAll();
    engine.reset();
    MixerClose();
    draw.CloseScreen();
    ui.Shutdown();
    overlay.Shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
