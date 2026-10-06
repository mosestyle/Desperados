#pragma once
#include <algorithm>
#include <string>

#include "../formats/Image16.h"
#include "../game/Camera.h"
#include "../game/Level.h"
#include "../render/Textures.h"
#include "Screen.h"

// Milestone 1: scroll and zoom around a level background by touch.
class MapViewScreen : public Screen {
public:
    MapViewScreen(App& app, int level);
    ~MapViewScreen() override;
    void onResize(int w, int h) override;
    void onGestures(const std::vector<Gesture>& g) override;
    void update(float dt) override;
    void render(SDL_Renderer* r) override;
    void releaseTextures() override;
    void recreateTextures(SDL_Renderer* r) override;

    const Camera& camera() const { return cam_; }
    Level& levelData() { return level; }
    void setView(float cx, float cy, float zoom) {
        cam_.zoom = std::clamp(zoom, cam_.minZoom, cam_.maxZoom); cam_.cx = cx; cam_.cy = cy; cam_.flying = false; cam_.clamp();
    }

private:
    SDL_FRect minimapRect() const;
    bool inMinimap(float x, float y) const;
    void jumpFromMinimap(float x, float y, bool animate);
    bool selectHeroOnMinimap(float x, float y);
    SDL_FRect stanceButton() const;
    SDL_FRect actionButton(int slot) const;  // slot 1 = gun, 2 = melee (left of the stance button)
    bool inButton(const SDL_FRect& b, float x, float y) const;
    void drawButton(SDL_Renderer* r, const SDL_FRect& b, bool active);

    SDL_FRect skipButton() const;
    SDL_FRect objectivesButton() const;
    void drawObjectives(SDL_Renderer* r);
    void drawResult(SDL_Renderer* r);
    void runTestPlan(float dt);

    int level_;
    bool loaded_ = false;
    bool skipping_ = false;       // fast-forwarding a cutscene
    float objectivesT_ = 0;       // objectives panel shown
    float resultT_ = 0;
    Level level;
    SDL_Texture* miniTex_ = nullptr;
    Camera cam_;
    bool draggingMinimap_ = false;
    bool firstLayout_ = true;
    bool lastTapMoved_ = false;
    uint32_t lastButtonTap_ = 0;
    bool dragFromButton_ = false;
    float failedT_ = 0;
    Level::Action mode_ = Level::Action::None;  // waiting for a target to be tapped
};
