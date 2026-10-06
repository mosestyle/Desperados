#pragma once
#include <SDL.h>

#include <memory>
#include <vector>

#include "../input/TouchInput.h"

class App;

class Screen {
public:
    explicit Screen(App& app) : app_(app) {}
    virtual ~Screen() = default;
    virtual void onResize(int w, int h) { (void)w; (void)h; }
    virtual void onGestures(const std::vector<Gesture>& g) { (void)g; }
    virtual bool onBack() { return false; }  // true = handled, false = leave screen
    virtual void update(float dt) { (void)dt; }
    virtual void render(SDL_Renderer* r) = 0;
    virtual void releaseTextures() {}
    virtual void recreateTextures(SDL_Renderer* r) { (void)r; }

protected:
    App& app_;
};
