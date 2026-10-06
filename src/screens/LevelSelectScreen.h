#pragma once
#include <string>
#include <vector>

#include "../formats/Image16.h"
#include "Screen.h"

// Grid of the 25 mission minimaps. Tap one to open it.
class LevelSelectScreen : public Screen {
public:
    explicit LevelSelectScreen(App& app);
    ~LevelSelectScreen() override;
    void onResize(int w, int h) override;
    void onGestures(const std::vector<Gesture>& g) override;
    void update(float dt) override;
    void render(SDL_Renderer* r) override;
    void releaseTextures() override;
    void recreateTextures(SDL_Renderer* r) override;
    SDL_FPoint cellCenter(int i) const { SDL_FRect rc = cellRect(i); return {rc.x + rc.w / 2, rc.y + rc.h / 2}; }

private:
    struct Entry { int level; std::string name; Image16 mini; SDL_Texture* tex = nullptr; };
    std::vector<Entry> entries_;
    int cols_ = 5;
    float cellW_ = 100, cellH_ = 60, pad_ = 10, top_ = 0, scroll_ = 0, maxScroll_ = 0, scrollV_ = 0;
    int pressed_ = -1;
    float pressedGlow_ = 0;
    int hitTest(float x, float y) const;
    SDL_FRect cellRect(int i) const;
};
