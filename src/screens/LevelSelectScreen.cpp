#include "LevelSelectScreen.h"

#include <algorithm>
#include <cmath>

#include "../App.h"
#include "../formats/LevelFile.h"
#include "../render/Textures.h"
#include "MapViewScreen.h"

LevelSelectScreen::LevelSelectScreen(App& app) : Screen(app) {
    char path[64];
    for (int i = 1; i <= 25; ++i) {
        SDL_snprintf(path, sizeof path, "data/levels/level_%02d.dvd", i);
        LevelFile lf;
        Entry e;
        e.level = i;
        if (lf.load(path) && lf.readMinimap(e.name, e.mini)) entries_.push_back(std::move(e));
        else SDL_Log("Could not read minimap of %s", path);
    }
    recreateTextures(app.renderer());
    onResize(app.width(), app.height());
}

LevelSelectScreen::~LevelSelectScreen() { releaseTextures(); }

void LevelSelectScreen::releaseTextures() {
    for (auto& e : entries_) if (e.tex) { SDL_DestroyTexture(e.tex); e.tex = nullptr; }
}

void LevelSelectScreen::recreateTextures(SDL_Renderer* r) {
    releaseTextures();
    for (auto& e : entries_) e.tex = createTexture16(r, e.mini, true);
}

void LevelSelectScreen::onResize(int w, int h) {
    const int n = std::max<int>(1, (int)entries_.size());
    pad_ = std::max(6.0f, h * 0.018f);
    top_ = pad_;
    // Pick the column count that gives the biggest thumbnails while fitting on one screen.
    const float aspect = 300.0f / 170.0f;
    float best = 0;
    for (int c = 3; c <= 9; ++c) {
        int rows = (n + c - 1) / c;
        float cw = (w - pad_ * (c + 1)) / c;
        float ch = cw / aspect;
        if ((ch + pad_) * rows + pad_ > h) { ch = (h - pad_ * (rows + 1)) / rows; cw = ch * aspect; }
        if (cw > best) { best = cw; cols_ = c; cellW_ = cw; cellH_ = ch; }
    }
    // Phones in portrait (or tiny windows): fall back to 2 big columns and scroll.
    if (cellW_ < w * 0.18f && w < h) {
        cols_ = 2;
        cellW_ = (w - pad_ * 3) / 2;
        cellH_ = cellW_ / aspect;
    }
    int rows = (n + cols_ - 1) / cols_;
    float total = rows * (cellH_ + pad_) + pad_;
    maxScroll_ = std::max(0.0f, total - h);
    scroll_ = std::clamp(scroll_, 0.0f, maxScroll_);
}

SDL_FRect LevelSelectScreen::cellRect(int i) const {
    int rows = ((int)entries_.size() + cols_ - 1) / cols_;
    float gridW = cols_ * cellW_ + (cols_ - 1) * pad_;
    float gridH = rows * cellH_ + (rows - 1) * pad_;
    float x0 = (app_.width() - gridW) * 0.5f;
    float y0 = maxScroll_ > 0 ? top_ - scroll_ : (app_.height() - gridH) * 0.5f;
    int c = i % cols_, r = i / cols_;
    return {x0 + c * (cellW_ + pad_), y0 + r * (cellH_ + pad_), cellW_, cellH_};
}

int LevelSelectScreen::hitTest(float x, float y) const {
    for (int i = 0; i < (int)entries_.size(); ++i) {
        SDL_FRect rc = cellRect(i);
        if (x >= rc.x && x < rc.x + rc.w && y >= rc.y && y < rc.y + rc.h) return i;
    }
    return -1;
}

void LevelSelectScreen::onGestures(const std::vector<Gesture>& gs) {
    for (const auto& g : gs) {
        switch (g.type) {
        case Gesture::Tap: {
            int i = hitTest(g.x, g.y);
            if (i >= 0) {
                pressed_ = i;
                pressedGlow_ = 1;
                app_.push(std::make_unique<MapViewScreen>(app_, entries_[i].level));
                return;
            }
            break;
        }
        case Gesture::Pan:
            scroll_ = std::clamp(scroll_ - g.dy, 0.0f, maxScroll_);
            scrollV_ = 0;
            break;
        case Gesture::DragEnd:
            scrollV_ = -g.dy;
            break;
        default:
            break;
        }
    }
}

void LevelSelectScreen::update(float dt) {
    if (scrollV_ != 0) {
        scroll_ = std::clamp(scroll_ + scrollV_ * dt, 0.0f, maxScroll_);
        scrollV_ *= std::exp(-dt * 4);
        if (std::fabs(scrollV_) < 5) scrollV_ = 0;
    }
    pressedGlow_ = std::max(0.0f, pressedGlow_ - dt * 3);
}

void LevelSelectScreen::render(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 34, 24, 16, 255);
    SDL_RenderClear(r);
    const float frame = std::max(2.0f, cellH_ * 0.025f);
    for (int i = 0; i < (int)entries_.size(); ++i) {
        SDL_FRect rc = cellRect(i);
        if (rc.y + rc.h < 0 || rc.y > app_.height()) continue;
        // parchment frame
        SDL_FRect outer{rc.x - frame, rc.y - frame, rc.w + 2 * frame, rc.h + 2 * frame};
        Uint8 glow = (Uint8)(120 * (i == pressed_ ? pressedGlow_ : 0));
        SDL_SetRenderDrawColor(r, 150 + glow / 2, 112 + glow / 2, 64, 255);
        SDL_RenderFillRectF(r, &outer);
        SDL_SetRenderDrawColor(r, 20, 14, 9, 255);
        SDL_RenderFillRectF(r, &rc);
        if (entries_[i].tex) {
            // keep the minimap's aspect ratio inside the cell
            const Image16& m = entries_[i].mini;
            float s = std::min(rc.w / m.w, rc.h / m.h);
            SDL_FRect dst{rc.x + (rc.w - m.w * s) * 0.5f, rc.y + (rc.h - m.h * s) * 0.5f, m.w * s, m.h * s};
            SDL_RenderCopyF(r, entries_[i].tex, nullptr, &dst);
        }
        // level number badge
        float px = std::max(2.0f, std::floor(cellH_ * 0.045f));
        float bw = numberWidth(2, px) + px * 4, bh = px * 9;
        SDL_FRect badge{rc.x, rc.y, bw, bh};
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 20, 12, 6, 200);
        SDL_RenderFillRectF(r, &badge);
        drawNumber(r, entries_[i].level, 2, rc.x + px * 2, rc.y + px * 2, px, {240, 214, 160, 255});
    }
}
