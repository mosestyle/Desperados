#pragma once
#include <string>

#include "../formats/Image16.h"
#include "../formats/LevelFile.h"
#include "../game/Camera.h"
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

private:
    SDL_FRect minimapRect() const;
    bool inMinimap(float x, float y) const;
    void jumpFromMinimap(float x, float y, bool animate);

    int level_;
    bool loaded_ = false;
    LevelFile file_;
    Image16 background_, minimap_;
    TiledImage bgTex_;
    SDL_Texture* miniTex_ = nullptr;
    Camera cam_;
    bool draggingMinimap_ = false;
    bool firstLayout_ = true;
};
