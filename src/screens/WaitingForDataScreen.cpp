#include "WaitingForDataScreen.h"

#include <cmath>

#include "../App.h"
#include "LevelSelectScreen.h"

WaitingForDataScreen::WaitingForDataScreen(App& app, std::string help) : Screen(app), help_(std::move(help)) {}

void WaitingForDataScreen::onGestures(const std::vector<Gesture>& gs) {
    for (const auto& g : gs)
        if (g.type == Gesture::Tap) shown_ = false;  // tap to show the instructions again
}

void WaitingForDataScreen::update(float dt) {
    t_ += dt;
    if (!shown_ && t_ > 0.3f) {
        shown_ = true;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Desperados game files needed", help_.c_str(), nullptr);
    }
    if (t_ >= nextCheck_) {
        nextCheck_ = t_ + 1.5f;
        if (app_.findData()) app_.replace(std::make_unique<LevelSelectScreen>(app_));
    }
}

void WaitingForDataScreen::render(SDL_Renderer* r) {
    SDL_SetRenderDrawColor(r, 34, 24, 16, 255);
    SDL_RenderClear(r);
    // simple pulsing "searching" indicator
    float cx = app_.width() * 0.5f, cy = app_.height() * 0.5f, s = app_.height() * 0.02f;
    for (int i = 0; i < 3; ++i) {
        float a = 0.5f + 0.5f * std::sin(t_ * 4 - i * 0.8f);
        SDL_SetRenderDrawColor(r, (Uint8)(120 + 120 * a), (Uint8)(90 + 90 * a), 50, 255);
        SDL_FRect rc{cx + (i - 1) * s * 2.5f - s / 2, cy - s / 2, s, s};
        SDL_RenderFillRectF(r, &rc);
    }
}
