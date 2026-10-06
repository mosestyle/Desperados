// A loaded mission map: background, placed characters and scenery, depth masks.
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
#include "../formats/SpriteFile.h"
#include "../render/SpriteAtlas.h"
#include "../render/Textures.h"
#include "Camera.h"

class Level {
public:
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
    bool showMasks = true;

private:
    struct Instance {
        LevelElement el;
        const SpriteFile* file = nullptr;
        const SpriteSet* set = nullptr;
        const SpriteRecord* rec = nullptr;
        int entry = 0;
        float ticks = 0;   // time spent on the current entry, in 25 Hz ticks
        int sortY = 0;
        SDL_Rect world{0, 0, 0, 0};  // current frame rectangle in world pixels
        SDL_Point foot{0, 0};
    };
    const SpriteFile* sprite(const std::string& folder, const std::string& file);
    void updateFrameRect(Instance& in);

    int number_ = 0;
    LevelFile file_;
    Image16 background_, minimap_;
    TiledImage bgTex_;
    std::vector<Mask> masks_;
    std::vector<SDL_Texture*> maskTex_;
    std::vector<std::unique_ptr<SpriteFile>> sprites_;
    std::map<std::string, const SpriteFile*> spriteByKey_;
    std::vector<Instance> instances_;
    std::vector<int> drawOrder_;
    SpriteAtlas atlas_;
};
