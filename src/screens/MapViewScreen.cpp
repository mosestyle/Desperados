#include "MapViewScreen.h"

#include <algorithm>
#include <cmath>

#include "../App.h"
#include "../audio/Audio.h"
#include "../render/BitmapFont.h"

static const Level::Action kActions[3] = {Level::Action::Gun, Level::Action::Melee, Level::Action::Throw};

MapViewScreen::MapViewScreen(App& app, int number) : Screen(app), level_(number) {
    loaded_ = level.load(app.renderer(), number);
    if (!loaded_) SDL_Log("Could not load level %d", number);
    if (level.minimap().w > 0) miniTex_ = createTexture16(app.renderer(), level.minimap(), true);
    onResize(app.width(), app.height());
}

MapViewScreen::~MapViewScreen() {
    if (miniTex_) SDL_DestroyTexture(miniTex_);
}

void MapViewScreen::releaseTextures() {
    level.releaseTextures();
    if (miniTex_) { SDL_DestroyTexture(miniTex_); miniTex_ = nullptr; }
}

void MapViewScreen::recreateTextures(SDL_Renderer* r) {
    releaseTextures();
    if (loaded_) level.recreateTextures(r);
    if (level.minimap().w > 0) miniTex_ = createTexture16(r, level.minimap(), true);
}

void MapViewScreen::onResize(int w, int h) {
    if (!loaded_) return;
    cam_.setup(w, h, level.width(), level.height());
    if (firstLayout_) {
        cam_.zoom = cam_.defaultZoom();
        cam_.cx = level.width() * 0.5f;
        cam_.cy = level.height() * 0.5f;
        firstLayout_ = false;
    }
    cam_.zoom = std::clamp(cam_.zoom, cam_.minZoom, cam_.maxZoom);
    cam_.clamp();
}

SDL_FRect MapViewScreen::minimapRect() const {
    if (level.minimap().w <= 0) return {0, 0, 0, 0};
    float w = std::min(app_.width() * 0.24f, app_.height() * 0.42f * level.minimap().w / level.minimap().h);
    float h = w * level.minimap().h / level.minimap().w;
    float m = app_.height() * 0.025f;
    return {app_.width() - w - m, m, w, h};
}

bool MapViewScreen::inMinimap(float x, float y) const {
    SDL_FRect rc = minimapRect();
    float slack = rc.h * 0.08f;
    return x >= rc.x - slack && x < rc.x + rc.w + slack && y >= rc.y - slack && y < rc.y + rc.h + slack;
}

void MapViewScreen::jumpFromMinimap(float x, float y, bool animate) {
    SDL_FRect rc = minimapRect();
    float u = std::clamp((x - rc.x) / rc.w, 0.0f, 1.0f), v = std::clamp((y - rc.y) / rc.h, 0.0f, 1.0f);
    float wx = u * level.width(), wy = v * level.height();
    if (animate) cam_.flyTo(wx, wy, cam_.zoom);
    else { cam_.cx = wx; cam_.cy = wy; cam_.vx = cam_.vy = 0; cam_.flying = false; cam_.clamp(); }
}

void MapViewScreen::onGestures(const std::vector<Gesture>& gs) {
    if (!loaded_) return;
    if (level.missionResult() != 0) {  // mission over: a tap goes back to the level list
        if (resultT_ > 1.5f)
            for (const auto& g : gs)
                if (g.type == Gesture::Tap) { level.stopVoices(); app_.pop(); return; }
        return;
    }
    if (level.userLocked()) {  // cutscene: the only control is "skip"
        for (const auto& g : gs)
            if (g.type == Gesture::Tap && inButton(skipButton(), g.x, g.y)) { skipping_ = true; level.stopVoices(); }
        return;
    }
    for (const auto& g : gs)
        if (g.type == Gesture::Tap && inButton(objectivesButton(), g.x, g.y) && !level.objectives().empty()) {
            objectivesT_ = objectivesT_ > 0 ? 0 : 8.0f;
            return;
        }
    if (failedT_ > 2.0f) {  // mission failed: any tap goes back to the level list
        for (const auto& g : gs)
            if (g.type == Gesture::Tap) { app_.pop(); return; }
        return;
    }
    for (const auto& g : gs) {
        switch (g.type) {
        case Gesture::DragStart:
            dragFromButton_ = inButton(stanceButton(), g.x, g.y);
            draggingMinimap_ = g.fingers == 1 && inMinimap(g.x, g.y);
            cam_.vx = cam_.vy = 0;
            cam_.flying = false;
            break;
        case Gesture::Pan:
            if (draggingMinimap_) jumpFromMinimap(g.x, g.y, false);
            else cam_.pan(g.dx, g.dy);
            break;
        case Gesture::Pinch:
            draggingMinimap_ = false;
            cam_.zoomAt(g.scale, g.x, g.y);
            break;
        case Gesture::DragEnd:
            if (!draggingMinimap_) { cam_.vx = -g.dx / cam_.zoom; cam_.vy = -g.dy / cam_.zoom; }
            draggingMinimap_ = false;
            break;
        case Gesture::Tap: {
            bool buttonHit = false;
            int slot = 0;
            for (Level::Action a : kActions) {
                if (!level.selectedCan(a)) continue;
                ++slot;
                if (inButton(actionButton(slot), g.x, g.y)) {
                    mode_ = mode_ == a ? Level::Action::None : a;
                    buttonHit = true;
                }
            }
            if (buttonHit) break;
            if (mode_ != Level::Action::None && !inMinimap(g.x, g.y) && !inButton(stanceButton(), g.x, g.y)) {
                // targeting: tap an enemy (or a person / object) to attack; tapping anything else cancels
                const float wx = cam_.toWorldX(g.x), wy = cam_.toWorldY(g.y);
                int target = level.pickTarget(wx, wy, 16.0f * app_.uiScale() / cam_.zoom + 4.0f);
                if (target >= 0 && level.orderAttack(mode_, target)) {
                    if (mode_ != Level::Action::Gun) mode_ = Level::Action::None;
                    break;
                }
                int hero = level.pickHero(wx, wy, 14.0f * app_.uiScale() / cam_.zoom + 4.0f);
                if (hero >= 0) level.select(hero);
                mode_ = Level::Action::None;
                break;
            }
            if (inButton(stanceButton(), g.x, g.y)) {
                uint32_t now = SDL_GetTicks();
                if (now - lastButtonTap_ > 350) level.toggleStanceSelected();  // ignore the 2nd tap of a double tap
                lastButtonTap_ = now;
                break;
            }
            if (inMinimap(g.x, g.y)) {
                if (!selectHeroOnMinimap(g.x, g.y)) jumpFromMinimap(g.x, g.y, true);
                break;
            }
            const float wx = cam_.toWorldX(g.x), wy = cam_.toWorldY(g.y);
            // fingers are big: accept taps a little outside the sprite
            int hero = level.pickHero(wx, wy, 14.0f * app_.uiScale() / cam_.zoom + 4.0f);
            if (hero >= 0) { level.select(hero); lastTapMoved_ = false; break; }
            lastTapMoved_ = level.moveSelected(wx, wy, false);
            break;
        }
        case Gesture::LongPress: {  // like the original's spyglass: show an enemy's field of view
            if (inMinimap(g.x, g.y) || inButton(stanceButton(), g.x, g.y)) break;
            const float wx = cam_.toWorldX(g.x), wy = cam_.toWorldY(g.y);
            int enemy = level.pickEnemy(wx, wy, 16.0f * app_.uiScale() / cam_.zoom + 4.0f);
            if (enemy >= 0) level.toggleCone(enemy);
            else level.hideAllCones();
            break;
        }
        case Gesture::DoubleTap: {
            if (inMinimap(g.x, g.y) || inButton(stanceButton(), g.x, g.y)) break;
            if (inButton(actionButton(1), g.x, g.y) || inButton(actionButton(2), g.x, g.y) || inButton(actionButton(3), g.x, g.y)) break;
            if (mode_ != Level::Action::None) break;
            const float wx = cam_.toWorldX(g.x), wy = cam_.toWorldY(g.y);
            int hero = level.pickHero(wx, wy, 14.0f * app_.uiScale() / cam_.zoom + 4.0f);
            float hx, hy;
            if (hero >= 0 && level.selectedPosition(hx, hy)) { cam_.flyTo(hx, hy, cam_.zoom); break; }  // centre on hero
            if (level.selected() >= 0) { level.moveSelected(wx, wy, true); break; }  // double tap = run
            // nobody to command: zoom in around the tap; when already close, zoom back out
            float target = cam_.zoom < cam_.defaultZoom() * 1.4f ? cam_.zoom * 2 : cam_.defaultZoom();
            cam_.flyTo(wx, wy, target);
            break;
        }
        default:
            break;
        }
    }
}

SDL_FRect MapViewScreen::actionButton(int slot) const {
    SDL_FRect b = stanceButton();
    b.x -= slot * (b.w * 1.12f);
    return b;
}

SDL_FRect MapViewScreen::stanceButton() const {
    float s = std::min(app_.height() * 0.2f, app_.width() * 0.12f);
    float m = app_.height() * 0.03f;
    return {app_.width() - s - m, app_.height() - s - m, s, s};
}

bool MapViewScreen::inButton(const SDL_FRect& b, float x, float y) const {
    float cx = b.x + b.w / 2, cy = b.y + b.h / 2, r = b.w * 0.6f;  // a bit larger than drawn
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
}

static void fillCircle(SDL_Renderer* r, float cx, float cy, float rad, SDL_Color c) {
    constexpr int N = 40;
    SDL_Vertex v[N + 2];
    v[0] = {{cx, cy}, c, {0, 0}};
    for (int i = 0; i <= N; ++i) {
        float a = (float)i / N * 6.2831853f;
        v[i + 1] = {{cx + std::cos(a) * rad, cy + std::sin(a) * rad}, c, {0, 0}};
    }
    int idx[N * 3];
    for (int i = 0; i < N; ++i) { idx[i * 3] = 0; idx[i * 3 + 1] = i + 1; idx[i * 3 + 2] = i + 2; }
    SDL_RenderGeometry(r, nullptr, v, N + 2, idx, N * 3);
}

void MapViewScreen::drawButton(SDL_Renderer* r, const SDL_FRect& b, bool active) {
    float cx = b.x + b.w / 2, cy = b.y + b.h / 2, rad = b.w / 2;
    fillCircle(r, cx, cy, rad, active ? SDL_Color{200, 150, 70, 235} : SDL_Color{120, 84, 40, 235});
    fillCircle(r, cx, cy, rad * 0.88f, SDL_Color{38, 26, 16, 225});
}

bool MapViewScreen::selectHeroOnMinimap(float x, float y) {
    SDL_FRect rc = minimapRect();
    std::vector<Level::ActorDot> dots;
    level.actorDots(dots);
    const float sx = rc.w / level.width(), sy = rc.h / level.height();
    const float radius = std::max(10.0f, rc.h * 0.06f);
    int best = -1;
    float bestD = radius * radius;
    for (size_t i = 0; i < dots.size(); ++i) {
        if (dots[i].faction != Faction::Hero) continue;
        float dx = rc.x + dots[i].x * sx - x, dy = rc.y + dots[i].y * sy - y;
        if (dx * dx + dy * dy < bestD) { bestD = dx * dx + dy * dy; best = (int)i; }
    }
    if (best < 0) return false;
    float wx = dots[best].x, wy = dots[best].y;
    int idx = level.pickHero(wx, wy - 1, 2);
    if (idx >= 0) level.select(idx);
    cam_.flyTo(wx, wy, cam_.zoom);
    return true;
}

void MapViewScreen::update(float dt) {
    cam_.update(dt);
    if (!loaded_) return;
    level.setViewCenter(cam_.cx, cam_.cy);
    level.setViewSize(cam_.screenW / cam_.zoom, cam_.screenH / cam_.zoom);
    if (level.missionResult() != 0) { resultT_ += dt; return; }
    if (!level.userLocked()) skipping_ = false;
    const int steps = skipping_ ? 10 : 1;  // fast-forward a cutscene the player skips
    for (int i = 0; i < steps; ++i) {
        level.update(dt);
        if (skipping_) level.stopVoices();
        if (!level.userLocked() && i > 0) { skipping_ = false; break; }
    }
    float x, y;
    if (level.scriptCamera(x, y)) {  // the script directs the camera (intros, cutscenes)
        cam_.cx = x;
        cam_.cy = y;
        cam_.vx = cam_.vy = 0;
        cam_.flying = false;
        cam_.clamp();
    }
    objectivesT_ = std::max(0.0f, objectivesT_ - dt);
    static bool moveReached = SDL_getenv("DESP_TESTMOVE") == nullptr;
    if (const char* t = SDL_getenv("DESP_TESTMOVE")) {  // "x,y" once control is given: walk there (any floor)
        static bool done = false;
        static float delay = SDL_getenv("DESP_TESTDELAY") ? (float)SDL_atof(SDL_getenv("DESP_TESTDELAY")) : 0.0f;
        float x, y;
        if (!level.userLocked() && level.firstHero() >= 0) delay -= dt;
        static int tries = 0;
        static float retry = 0;
        retry -= dt;
        float hx0, hy0;
        if (done && tries < 6 && retry <= 0 && !level.userLocked() && level.selectedIdle() && level.selectedPosition(hx0, hy0) &&
            SDL_sscanf(t, "%f,%f", &x, &y) == 2 && std::hypot(hx0 - x, hy0 - y) > 12) {
            done = false;  // interrupted by a cutscene: order again
            ++tries;
        }
        if (!done && delay <= 0 && !level.userLocked() && level.firstHero() >= 0 && SDL_sscanf(t, "%f,%f", &x, &y) == 2) {
            done = true;
            retry = 3.0f;
            if (level.selected() < 0) level.select(level.firstHero());
            if (SDL_getenv("DESP_TESTPRONE") && !level.selectedProne()) level.toggleStanceSelected();
            SDL_Log("debug: move to %.0f,%.0f: %s", x, y, level.moveSelected(x, y, false) ? "ok" : "refused");
        }
        float hx, hy;
        if (done && level.selectedPosition(hx, hy)) { cam_.cx = hx; cam_.cy = hy; cam_.clamp(); }
        if (done && level.selectedPosition(hx, hy) && SDL_sscanf(t, "%f,%f", &x, &y) == 2 && std::hypot(hx - x, hy - y) <= 12 &&
            level.selectedIdle() && !level.userLocked())
            moveReached = true;
    }
    if (const char* t = SDL_getenv("DESP_TESTATTACK")) {  // "elem,action[;elem,action...]" once control is given
        static size_t step = 0;
        static float wait = 0;
        std::vector<std::pair<int, int>> list;
        for (const char* p = t; *p;) {
            int e = 0, a = 1;
            if (SDL_sscanf(p, "%d,%d", &e, &a) == 2) list.push_back({e, a});
            while (*p && *p != ';') ++p;
            if (*p) ++p;
        }
        wait -= dt;
        if (moveReached && !level.userLocked() && level.firstHero() >= 0 && wait <= 0 && step < list.size()) {
            if (level.debugAttackElement(list[step].first, (Level::Action)list[step].second)) { ++step; wait = 8.0f; }
            else wait = 2.0f;
        }
    }
    if (level.allHeroesDead()) failedT_ += dt;
}

SDL_FRect MapViewScreen::skipButton() const {
    float s = std::min(app_.height() * 0.14f, app_.width() * 0.09f), m = app_.height() * 0.03f;
    return {app_.width() - s - m, app_.height() - s - m, s, s};
}

SDL_FRect MapViewScreen::objectivesButton() const {
    float s = std::min(app_.height() * 0.13f, app_.width() * 0.08f), m = app_.height() * 0.03f;
    return {m, m, s, s};
}

void MapViewScreen::drawObjectives(SDL_Renderer* r) {
    BitmapFont& f = uiFont();
    if (!f.ready()) return;
    const float ui = app_.uiScale(), scale = std::max(1.0f, std::round(ui * 1.6f * 2) / 2), lh = f.height() * scale * 1.1f;
    const float w = app_.width() * 0.6f, x = (app_.width() - w) / 2, pad = 14 * ui;
    std::vector<std::pair<std::string, bool>> rows;
    for (const auto& o : level.objectives())
        for (const auto& line : f.wrap(level.objectiveText(o.first), w - pad * 2 - 30 * ui, scale)) rows.push_back({line, o.second});
    float h = (rows.size() + 1.5f) * lh + pad * 2, y = app_.height() * 0.12f;
    SDL_FRect bg{x, y, w, h};
    SDL_SetRenderDrawColor(r, 18, 11, 6, 225);
    SDL_RenderFillRectF(r, &bg);
    SDL_SetRenderDrawColor(r, 150, 110, 60, 230);
    SDL_RenderDrawRectF(r, &bg);
    f.draw(r, "Mission objectives", x + pad, y + pad, scale, {255, 200, 120, 255});
    float ty = y + pad + lh * 1.5f;
    for (const auto& row : rows) {
        f.draw(r, row.first, x + pad + 30 * ui, ty, scale, row.second ? SDL_Color{150, 140, 120, 255} : SDL_Color{255, 245, 220, 255});
        ty += lh;
    }
}

void MapViewScreen::drawResult(SDL_Renderer* r) {
    const bool won = level.missionResult() == 1;
    Uint8 a = (Uint8)std::min(170.0f, resultT_ * 120.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    if (won) SDL_SetRenderDrawColor(r, 0, 0, 0, a);
    else SDL_SetRenderDrawColor(r, 90, 0, 0, a);
    SDL_Rect all{0, 0, app_.width(), app_.height()};
    SDL_RenderFillRect(r, &all);
    BitmapFont& f = uiFont();
    if (!f.ready()) return;
    const float ui = app_.uiScale(), big = std::max(2.0f, std::round(ui * 3.0f)), small = std::max(1.0f, std::round(ui * 1.6f * 2) / 2);
    std::string title = won ? "MISSION SUCCESSFUL!" : "MISSION FAILED";
    float y = app_.height() * 0.38f;
    f.draw(r, title, (app_.width() - f.width(title, big)) / 2, y, big, {255, 210, 120, 255});
    y += f.height() * big * 1.4f;
    std::string text = won ? "" : level.missionResultText();
    for (const auto& line : f.wrap(text, app_.width() * 0.7f, small)) {
        f.draw(r, line, (app_.width() - f.width(line, small)) / 2, y, small, {255, 245, 220, 255});
        y += f.height() * small * 1.1f;
    }
    if (resultT_ > 1.5f) {
        std::string t = "Tap to continue";
        f.draw(r, t, (app_.width() - f.width(t, small)) / 2, app_.height() * 0.8f, small, {200, 190, 170, 255});
    }
}

void MapViewScreen::render(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    if (!loaded_) return;
    level.render(r, cam_, app_.width(), app_.height());

    if (failedT_ > 0) {  // mission failed: darken the screen in red
        Uint8 a = (Uint8)std::min(170.0f, failedT_ * 90.0f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 90, 0, 0, a);
        SDL_Rect all{0, 0, app_.width(), app_.height()};
        SDL_RenderFillRect(r, &all);
        return;
    }

    level.renderOverlay(r, app_.width(), app_.height(), app_.uiScale());
    if (level.missionResult() != 0) { drawResult(r); return; }
    if (level.userLocked()) {  // cutscene: just the skip button
        SDL_FRect b = skipButton();
        drawButton(r, b, skipping_);
        SDL_SetRenderDrawColor(r, 240, 220, 170, 255);
        float cx = b.x + b.w / 2, cy = b.y + b.h / 2, s = b.w * 0.16f;
        for (int k = 0; k < 2; ++k) {  // fast-forward symbol
            float ox = cx - s * 1.1f + k * s * 1.1f;
            SDL_Vertex v[3] = {{{ox - s * 0.5f, cy - s}, {240, 220, 170, 255}, {0, 0}},
                               {{ox - s * 0.5f, cy + s}, {240, 220, 170, 255}, {0, 0}},
                               {{ox + s * 0.7f, cy}, {240, 220, 170, 255}, {0, 0}}};
            SDL_RenderGeometry(r, nullptr, v, 3, nullptr, 0);
        }
        return;
    }
    if (!level.objectives().empty()) {  // objectives button (a scroll with lines)
        SDL_FRect b = objectivesButton();
        drawButton(r, b, objectivesT_ > 0);
        SDL_SetRenderDrawColor(r, 240, 220, 170, 255);
        for (int k = 0; k < 3; ++k) {
            SDL_FRect l{b.x + b.w * 0.3f, b.y + b.h * (0.33f + k * 0.15f), b.w * 0.4f, std::max(2.0f, b.h * 0.05f)};
            SDL_RenderFillRectF(r, &l);
        }
        if (objectivesT_ > 0) drawObjectives(r);
    }
    if (mode_ != Level::Action::None && !level.selectedCan(mode_)) mode_ = Level::Action::None;
    if (mode_ == Level::Action::Gun) level.drawRange(r, cam_, level.selectedGunRange(), {255, 220, 90, 170});

    // stance button: shows the posture you switch to (lying figure = get down, standing = get up)
    if (level.selected() >= 0) {
        SDL_FRect b = stanceButton();
        const bool prone = level.selectedProne();
        drawButton(r, b, prone);
        float pad = b.w * 0.2f;
        level.drawSelectedFrame(r, {b.x + pad, b.y + pad, b.w - 2 * pad, b.h - 2 * pad}, prone ? 0 : 7, 6);
        // action buttons: the hero's own shooting / melee animation as the icon
        int slot = 0;
        for (Level::Action a : kActions) {
            if (!level.selectedCan(a)) continue;
            SDL_FRect ab = actionButton(++slot);
            drawButton(r, ab, mode_ == a);
            level.drawSelectedFrame(r, {ab.x + pad, ab.y + pad, ab.w - 2 * pad, ab.h - 2 * pad},
                                    level.selectedActionAnim(a), 6, 1);
            if (a == Level::Action::Gun) {  // ammo pips
                int maxAmmo = 0, ammo = level.selectedAmmo(&maxAmmo);
                float pip = std::max(3.0f, ab.w * 0.05f), gap = pip * 0.6f;
                float total = maxAmmo * pip + (maxAmmo - 1) * gap, x0 = ab.x + (ab.w - total) / 2, y0 = ab.y + ab.h - pip * 0.5f;
                for (int i = 0; i < maxAmmo; ++i) {
                    SDL_FRect p{x0 + i * (pip + gap), y0, pip, pip};
                    SDL_SetRenderDrawColor(r, i < ammo ? 240 : 70, i < ammo ? 200 : 55, i < ammo ? 90 : 40, 255);
                    SDL_RenderFillRectF(r, &p);
                }
            }
        }
    }

    if (miniTex_) {
        SDL_FRect rc = minimapRect();
        float b = std::max(2.0f, rc.h * 0.02f);
        SDL_FRect back{rc.x - b, rc.y - b, rc.w + 2 * b, rc.h + 2 * b};
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 25, 16, 8, 170);
        SDL_RenderFillRectF(r, &back);
        SDL_RenderCopyF(r, miniTex_, nullptr, &rc);
        // current view rectangle
        float sx = rc.w / level.width(), sy = rc.h / level.height();
        SDL_FRect view{rc.x + cam_.toWorldX(0) * sx, rc.y + cam_.toWorldY(0) * sy,
                       app_.width() / cam_.zoom * sx, app_.height() / cam_.zoom * sy};
        SDL_SetRenderDrawColor(r, 255, 236, 190, 255);
        for (int k = 0; k < (int)std::max(1.0f, b * 0.6f); ++k) {
            SDL_FRect v{view.x - k, view.y - k, view.w + 2 * k, view.h + 2 * k};
            SDL_RenderDrawRectF(r, &v);
        }
        // characters: heroes green, enemies red, civilians blue, animals small grey
        std::vector<Level::ActorDot> dots;
        level.actorDots(dots);
        const float d = std::max(2.0f, std::round(rc.h * 0.016f));
        auto dot = [&](const Level::ActorDot& a, float size, SDL_Color c) {
            float px = rc.x + a.x * sx, py = rc.y + a.y * sy;
            if (px < rc.x || py < rc.y || px > rc.x + rc.w || py > rc.y + rc.h) return;
            SDL_FRect outer{px - size - 1, py - size - 1, 2 * size + 2, 2 * size + 2};
            SDL_SetRenderDrawColor(r, a.selected ? 255 : 20, a.selected ? 255 : 20, a.selected ? 255 : 20, 220);
            SDL_RenderFillRectF(r, &outer);
            SDL_FRect inner{px - size, py - size, 2 * size, 2 * size};
            SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
            SDL_RenderFillRectF(r, &inner);
        };
        for (int pass = 0; pass < 4; ++pass)  // animals first, heroes on top
            for (const auto& a : dots) {
                if (pass == 0 && a.faction == Faction::Animal) dot(a, d * 0.6f, {170, 160, 140, 255});
                if (pass == 1 && a.faction == Faction::Civilian) dot(a, d * 0.8f, {90, 160, 255, 255});
                if (pass == 2 && a.faction == Faction::Enemy) dot(a, d, {235, 40, 30, 255});
                if (pass == 3 && a.faction == Faction::Hero) dot(a, d * 1.25f, {60, 230, 60, 255});
            }
    }
}
