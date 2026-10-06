// A loaded mission map: background, placed characters and scenery, depth masks,
// walkable areas and simple hero control (select, walk, run).
#pragma once
#include <SDL.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../formats/Image16.h"
#include "../formats/LevelElements.h"
#include "../formats/LevelFile.h"
#include "../formats/Masks.h"
#include "../formats/MotionAreas.h"
#include "../formats/SpriteFile.h"
#include "../render/SpriteAtlas.h"
#include "../render/Textures.h"
#include "Camera.h"
#include "NavGrid.h"

class Level {
public:
    struct ActorDot { float x, y; Faction faction; bool selected; };

    ~Level();
    bool load(SDL_Renderer* r, int number);
    void update(float dt);
    void render(SDL_Renderer* r, const Camera& cam, int screenW, int screenH);
    void releaseTextures();
    void recreateTextures(SDL_Renderer* r);

    int width() const { return background_.w; }
    int height() const { return background_.h; }
    const Image16& minimap() const { return minimap_; }
    int actorCount() const;
    int instanceCount() const { return (int)instances_.size(); }
    void actorDots(std::vector<ActorDot>& out) const;

    // --- hero control ---
    // Hero under a world point (with `slack` extra world pixels around the sprite), or -1.
    int pickHero(float wx, float wy, float slack) const;
    int selected() const { return selected_; }
    void select(int idx) { selected_ = idx; }
    bool selectedPosition(float& x, float& y) const;
    // Sends the selected hero to a world point. Returns false if it can't get there.
    bool moveSelected(float wx, float wy, bool run);
    void setSelectedRunning(bool run);
    int firstHero() const;
    bool showMasks = true;

private:
    struct Instance {
        LevelElement el;
        const SpriteFile* file = nullptr;
        const SpriteSet* set = nullptr;
        const SpriteRecord* rec = nullptr;
        int entry = 0;
        float ticks = 0;            // time on the current entry, in 25 Hz ticks
        int sortY = 0;
        SDL_Rect world{0, 0, 0, 0}; // current frame rectangle in world pixels
        // actors
        float x = 0, y = 0;         // feet, world pixels
        int floor = 0, dir = 0, anim = 0;
        std::vector<SDL_FPoint> path;
        size_t pathIdx = 0;
        bool running = false;
        float stepAcc = 0;          // ground distance walked on the current frame
    };
    const SpriteFile* sprite(const std::string& folder, const std::string& file);
    void updateFrameRect(Instance& in);
    void setAnim(Instance& in, int anim, int dir);
    void updateMovement(Instance& in, float dt);
    void drawEllipse(SDL_Renderer* r, const Camera& cam, float x, float y, float rx, float ry, SDL_Color c);

    int number_ = 0;
    LevelFile file_;
    Image16 background_, minimap_;
    TiledImage bgTex_;
    std::vector<Mask> masks_;
    std::vector<SDL_Texture*> maskTex_;
    std::vector<MotionLayer> layers_;
    std::vector<NavGrid> nav_;
    std::vector<std::unique_ptr<SpriteFile>> sprites_;
    std::map<std::string, const SpriteFile*> spriteByKey_;
    std::vector<Instance> instances_;
    std::vector<int> drawOrder_;
    SpriteAtlas atlas_;
    int selected_ = -1;
    SDL_FPoint target_{0, 0};
    float targetFade_ = 0;
    float time_ = 0;
};
