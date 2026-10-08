// The app: window, GL context, the player's game folder, then a mission drawn by the engine.
// Step 1: look at the missions (drag / flick / pinch), animated scenery; arrows at the top switch
// between the missions.
#include <SDL.h>

#include <GLES3/gl3.h>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "app/Camera.h"
#include "app/Overlay.h"
#include "dv/DVEngine.h"
#include "sb/SBDrawManager.h"
#include "sb/SBFile.h"

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
};

int main(int argc, char** argv) {
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
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
        levelIndex = i;
    };
    if (!levels.empty()) loadLevel(0);

    std::vector<Touch> touches;
    float pinchDist = 0, lastCx = 0, lastCy = 0;
    Uint32 lastMoveT = 0;
    float speedX = 0, speedY = 0;
    double tickAcc = 0;
    Uint64 prev = SDL_GetPerformanceCounter();
    bool run = true, paused = false;
    while (run) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: run = false; break;
            case SDL_APP_WILLENTERBACKGROUND: paused = true; break;
            case SDL_APP_DIDENTERFOREGROUND: paused = false; prev = SDL_GetPerformanceCounter(); break;
            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_AC_BACK || ev.key.keysym.sym == SDLK_ESCAPE) run = false;
                if (ev.key.keysym.sym == SDLK_RIGHT && !levels.empty()) loadLevel((levelIndex + 1) % levels.size());
                if (ev.key.keysym.sym == SDLK_LEFT && !levels.empty()) loadLevel((levelIndex + levels.size() - 1) % levels.size());
                if (ev.key.keysym.sym == SDLK_CAPSLOCK && engine) engine->silhouettes = !engine->silhouettes;
                break;
            case SDL_FINGERDOWN: {
                float x = ev.tfinger.x * sw, y = ev.tfinger.y * sh;
                int button = overlay.HitButton(x, y, sw, sh);
                if (touches.empty() && button != 0 && !levels.empty()) {
                    if (button > 0) loadLevel((levelIndex + 1) % levels.size());
                    else loadLevel((levelIndex + levels.size() - 1) % levels.size());
                    break;
                }
                touches.push_back({ev.tfinger.fingerId, x, y});
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
                    int button = overlay.HitButton((float)ev.button.x, (float)ev.button.y, sw, sh);
                    if (button != 0 && !levels.empty()) {
                        if (button > 0) loadLevel((levelIndex + 1) % levels.size());
                        else loadLevel((levelIndex + levels.size() - 1) % levels.size());
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

        if (engine) {
            cam.Update((float)dt, engine->mapW, engine->mapH, sw, sh);
            // the engine's screen follows the zoom
            int ew, eh;
            engineScreen(ew, eh);
            if (ew != draw.ScreenWidth() || eh != draw.ScreenHeight()) {
                draw.ResizeScreen((uint16_t)ew, (uint16_t)eh);
                engine->screen = SBGeoVector2D((float)ew, (float)eh);
            }
            // the original's 25 Hz tick
            tickAcc += dt;
            int steps = 0;
            while (tickAcc >= 0.04 && steps < 4) {
                engine->PerformHourglass();
                tickAcc -= 0.04;
                ++steps;
            }
            if (tickAcc > 0.04) tickAcc = 0;
            engine->camera = SBGeoPoint2D(std::floor(cam.x), std::floor(cam.y));
            engine->Draw();
            draw.Finish();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, sw, sh);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        overlay.Begin(sw, sh);
        if (engine) {
            // the engine's picture, scaled, shifted by the camera's fraction for smooth scrolling
            float s = cam.ScreenScale(sh);
            float fx = cam.x - std::floor(cam.x), fy = cam.y - std::floor(cam.y);
            overlay.Picture(draw.ScreenTexture(), draw.ScreenWidth(), draw.ScreenHeight(), -fx * s, -fy * s,
                            draw.ScreenWidth() * s, draw.ScreenHeight() * s);
        }
        overlay.LevelBar(levelIndex + 1, levels.size(), sw, sh);
        if (!loadError.empty()) overlay.Text(loadError.c_str(), sw * 0.5f, sh * 0.5f, sh * 0.04f, true);
        SDL_GL_SwapWindow(window);
    }
    engine.reset();
    draw.CloseScreen();
    overlay.Shutdown();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
