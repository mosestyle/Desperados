#pragma once
#include <string>

#include "Screen.h"

// Shown when the game folder can't be found. Explains where to copy it and keeps
// checking, so the app continues by itself once files / permission are in place.
class WaitingForDataScreen : public Screen {
public:
    WaitingForDataScreen(App& app, std::string help);
    void update(float dt) override;
    void render(SDL_Renderer* r) override;
    void onGestures(const std::vector<Gesture>& g) override;

private:
    std::string help_;
    float t_ = 0, nextCheck_ = 1.0f;
    bool shown_ = false;
};
