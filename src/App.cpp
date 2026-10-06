#include "App.h"

#include <algorithm>
#include <cmath>

#include "core/FileSystem.h"
#include "screens/LevelSelectScreen.h"
#include "screens/MapViewScreen.h"
#include "screens/WaitingForDataScreen.h"

#ifdef __ANDROID__
static const char* kHelp =
    "Copy your Desperados: Wanted Dead or Alive folder (from your PC, Steam or GOG) to the phone as:\n\n"
    "  Internal storage / Desperados\n\n"
    "(the folder that contains 'data' and 'localisation'). The videos (.ogv) are not needed yet.\n\n"
    "Then allow 'All files access' for this app when Android asks. The app will continue by itself.";
#else
static const char* kHelp =
    "Start the program with the path to your Desperados folder:\n\n  desperados --data \"C:/Games/Desperados\"\n\n"
    "or run it from inside that folder.";
#endif

static std::vector<std::string> dataCandidates(const std::string& explicitPath) {
    std::vector<std::string> base;
    if (!explicitPath.empty()) base.push_back(explicitPath);
    if (const char* env = SDL_getenv("DESPERADOS_DATA")) base.push_back(env);
#ifdef __ANDROID__
    const char* ext = SDL_AndroidGetExternalStoragePath();  // app-private folder, no permission needed
    for (const char* root : {"/storage/emulated/0", "/sdcard"}) {
        base.push_back(std::string(root) + "/Desperados");
        base.push_back(std::string(root) + "/Desperados Wanted Dead or Alive");
        base.push_back(std::string(root) + "/Download/Desperados");
        base.push_back(std::string(root) + "/Download/Desperados Wanted Dead or Alive");
    }
    if (ext) { base.push_back(std::string(ext) + "/Desperados"); base.push_back(ext); }
#else
    base.push_back(".");
    if (char* bp = SDL_GetBasePath()) { base.push_back(bp); SDL_free(bp); }
#endif
    // Also accept the folder one level deeper (e.g. an unzipped folder inside the folder).
    std::vector<std::string> all;
    for (const auto& b : base) {
        all.push_back(b);
        for (const char* sub : {"Desperados", "Desperados Wanted Dead or Alive", "Desperados - Wanted Dead or Alive"})
            all.push_back(b + "/" + sub);
    }
    return all;
}

bool App::findData() { return fs_::findDataRoot(dataCandidates(opt_.dataPath)); }

bool App::init() {
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");   // we handle touch ourselves
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");  // smooth zooming
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __ANDROID__
    flags |= SDL_WINDOW_FULLSCREEN;
#endif
    window_ = SDL_CreateWindow("Desperados", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, opt_.width, opt_.height, flags);
    if (!window_) { SDL_Log("CreateWindow failed: %s", SDL_GetError()); return false; }
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) renderer_ = SDL_CreateRenderer(window_, -1, 0);
    if (!renderer_) { SDL_Log("CreateRenderer failed: %s", SDL_GetError()); return false; }
    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer_, &info);
    SDL_Log("Renderer: %s, max texture %dx%d", info.name, info.max_texture_width, info.max_texture_height);
    updateSize();
    return true;
}

void App::updateSize() {
    SDL_GetRendererOutputSize(renderer_, &w_, &h_);
    uiScale_ = h_ / 720.0f;
    input_.setScreenSize(w_, h_);
    for (auto& s : screens_) s->onResize(w_, h_);
}

void App::push(std::unique_ptr<Screen> s) { screens_.push_back(std::move(s)); }

void App::pop() {
    if (screens_.empty()) return;
    graveyard_.push_back(std::move(screens_.back()));
    screens_.pop_back();
    if (screens_.empty()) running_ = false;
}

void App::replace(std::unique_ptr<Screen> s) {
    if (!screens_.empty()) { graveyard_.push_back(std::move(screens_.back())); screens_.pop_back(); }
    screens_.push_back(std::move(s));
}

void App::shutdown() {
    screens_.clear();
    graveyard_.clear();
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

int App::run() {
    if (!init()) return 1;
    if (findData()) {
        push(std::make_unique<LevelSelectScreen>(*this));
        if (opt_.startLevel >= 1 && opt_.startLevel <= 25) push(std::make_unique<MapViewScreen>(*this, opt_.startLevel));
    } else {
        push(std::make_unique<WaitingForDataScreen>(*this, kHelp));
    }
    if (!opt_.autotest.empty()) testStart_ = SDL_GetTicks();

    Uint64 last = SDL_GetPerformanceCounter();
    bool paused = false;
    while (running_) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running_ = false; break;
            case SDL_APP_WILLENTERBACKGROUND: paused = true; break;
            case SDL_APP_DIDENTERFOREGROUND: paused = false; last = SDL_GetPerformanceCounter(); break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) updateSize();
                break;
            case SDL_RENDER_DEVICE_RESET:
            case SDL_RENDER_TARGETS_RESET:
                SDL_Log("Render device reset: recreating textures");
                for (auto& s : screens_) s->recreateTextures(renderer_);
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.scancode == SDL_SCANCODE_AC_BACK || e.key.keysym.sym == SDLK_ESCAPE) {
                    if (!screens_.empty() && !screens_.back()->onBack()) pop();
                }
                break;
            default:
                input_.handleEvent(e);
                break;
            }
        }
        if (!running_) break;
        if (paused) { SDL_Delay(50); continue; }

        Uint64 now = SDL_GetPerformanceCounter();
        float dt = std::min(0.1f, (float)(now - last) / (float)SDL_GetPerformanceFrequency());
        last = now;

        input_.update(SDL_GetTicks());
        auto gestures = input_.take();
        if (!screens_.empty() && !gestures.empty()) screens_.back()->onGestures(gestures);
        if (!screens_.empty()) screens_.back()->update(dt);
        graveyard_.clear();
        if (screens_.empty()) break;

        if (!opt_.shot.empty() && ++frameCount_ == 3) {
            if (auto* mv = dynamic_cast<MapViewScreen*>(screens_.back().get()))
                if (opt_.viewX >= 0) mv->setView(opt_.viewX, opt_.viewY, opt_.viewZoom > 0 ? opt_.viewZoom : 1.0f);
        }
        screens_.back()->render(renderer_);
        if (!opt_.shot.empty() && frameCount_ == 40) { saveScreenshot(opt_.shot); running_ = false; }
        if (!opt_.autotest.empty()) runAutotestStep(SDL_GetTicks());
        SDL_RenderPresent(renderer_);
    }
    shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// Scripted self-test (desktop): drives the UI with synthetic touch events and
// saves screenshots, so gestures can be verified without a phone.

void App::injectFinger(SDL_EventType type, SDL_FingerID id, float x, float y) {
    SDL_Event e{};
    e.type = type;
    e.tfinger.timestamp = SDL_GetTicks();
    e.tfinger.touchId = 1;
    e.tfinger.fingerId = id;
    e.tfinger.x = x / w_;
    e.tfinger.y = y / h_;
    e.tfinger.pressure = 1;
    SDL_PushEvent(&e);
}

void App::saveScreenshot(const std::string& path) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w_, h_, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return;
    if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0) {
        SDL_SaveBMP(s, path.c_str());
        SDL_Log("Saved %s", path.c_str());
    }
    SDL_FreeSurface(s);
}

void App::runAutotestStep(uint32_t now) {
    if (steps_.empty()) {
        const std::string P = opt_.autotest;
        auto at = [this](uint32_t t, std::function<void()> fn) { steps_.push_back({t, std::move(fn)}); };
        auto drag = [this, at](uint32_t t0, SDL_FingerID id, float x0, float y0, float x1, float y1, int n, uint32_t stepMs) {
            at(t0, [=] { injectFinger(SDL_FINGERDOWN, id, x0, y0); });
            for (int i = 1; i <= n; ++i)
                at(t0 + i * stepMs, [=] { injectFinger(SDL_FINGERMOTION, id, x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n); });
            at(t0 + (n + 1) * stepMs, [=] { injectFinger(SDL_FINGERUP, id, x1, y1); });
        };
        float W = (float)w_, H = (float)h_;
        at(400, [=] { saveScreenshot(P + "_1_levelselect.bmp"); });
        at(500, [=] {  // tap level 1
            auto* sel = dynamic_cast<LevelSelectScreen*>(screens_.back().get());
            if (!sel) return;
            SDL_FPoint c = sel->cellCenter(0);
            injectFinger(SDL_FINGERDOWN, 1, c.x, c.y);
            injectFinger(SDL_FINGERUP, 1, c.x, c.y);
        });
        at(1200, [=] { saveScreenshot(P + "_2_map_start.bmp"); });
        drag(1300, 1, W * 0.6f, H * 0.6f, W * 0.2f, H * 0.3f, 12, 16);  // one finger pan
        at(2000, [=] { saveScreenshot(P + "_3_after_pan.bmp"); });
        // two finger pinch out (zoom in) around the centre
        at(2100, [=] { injectFinger(SDL_FINGERDOWN, 1, W * 0.45f, H * 0.5f); injectFinger(SDL_FINGERDOWN, 2, W * 0.55f, H * 0.5f); });
        for (int i = 1; i <= 10; ++i)
            at(2100 + i * 16, [=] {
                injectFinger(SDL_FINGERMOTION, 1, W * (0.45f - 0.02f * i), H * 0.5f);
                injectFinger(SDL_FINGERMOTION, 2, W * (0.55f + 0.02f * i), H * 0.5f);
            });
        at(2300, [=] { injectFinger(SDL_FINGERUP, 1, W * 0.25f, H * 0.5f); injectFinger(SDL_FINGERUP, 2, W * 0.75f, H * 0.5f); });
        at(2600, [=] { saveScreenshot(P + "_4_after_pinch.bmp"); });
        at(2700, [=] {  // tap the bottom-left part of the minimap
            auto* mv = dynamic_cast<MapViewScreen*>(screens_.back().get());
            (void)mv;
            float mw = std::min(W * 0.24f, H * 0.42f * 300 / 159), mh = mw * 159 / 300, m = H * 0.025f;
            float x = W - mw - m + mw * 0.15f, y = m + mh * 0.85f;
            injectFinger(SDL_FINGERDOWN, 1, x, y);
            injectFinger(SDL_FINGERUP, 1, x, y);
        });
        at(3600, [=] { saveScreenshot(P + "_5_after_minimap_tap.bmp"); });
        at(3700, [=] {
            auto* mv = dynamic_cast<MapViewScreen*>(screens_.back().get());
            if (mv) SDL_Log("camera: centre %.0f,%.0f zoom %.2f (min %.2f max %.2f)", mv->camera().cx, mv->camera().cy,
                            mv->camera().zoom, mv->camera().minZoom, mv->camera().maxZoom);
            running_ = false;
        });
    }
    while (stepIndex_ < steps_.size() && now - testStart_ >= steps_[stepIndex_].at) steps_[stepIndex_++].fn();
}
