#include "MapViewScreen.h"

#include <algorithm>
#include <cmath>

#include "../App.h"

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
    for (const auto& g : gs) {
        switch (g.type) {
        case Gesture::DragStart:
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
        case Gesture::Tap:
            if (inMinimap(g.x, g.y)) jumpFromMinimap(g.x, g.y, true);
            break;
        case Gesture::DoubleTap:
            if (inMinimap(g.x, g.y)) break;
            {
                // zoom in around the tap; when already close, zoom back out
                float wx = cam_.toWorldX(g.x), wy = cam_.toWorldY(g.y);
                float target = cam_.zoom < cam_.defaultZoom() * 1.4f ? cam_.zoom * 2 : cam_.defaultZoom();
                cam_.flyTo(wx, wy, target);
            }
            break;
        default:
            break;
        }
    }
}

void MapViewScreen::update(float dt) {
    cam_.update(dt);
    if (loaded_) level.update(dt);
}

void MapViewScreen::render(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);
    if (!loaded_) return;
    level.render(r, cam_, app_.width(), app_.height());

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
    }
}
