// Packs sprite frames into large textures on first use, so only frames that are
// actually shown are decoded and uploaded to the GPU.
#pragma once
#include <SDL.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

class SpriteFile;

class SpriteAtlas {
public:
    struct Slot { SDL_Texture* tex = nullptr; SDL_Rect rc{0, 0, 0, 0}; };

    ~SpriteAtlas() { clear(); }
    void setRenderer(SDL_Renderer* r) { renderer_ = r; }
    const Slot* get(const SpriteFile& file, int frame);
    void clear();  // drops all textures (they are rebuilt lazily)
    int pageCount() const { return (int)pages_.size(); }

private:
    static constexpr int kPage = 2048;
    bool allocate(int w, int h, Slot& out);

    SDL_Renderer* renderer_ = nullptr;
    std::vector<SDL_Texture*> pages_;
    std::vector<SDL_Texture*> singles_;  // frames too big for a page
    int curX_ = 0, curY_ = 0, shelfH_ = 0;
    std::unordered_map<uint64_t, Slot> slots_;
    std::vector<uint32_t> scratch_;
};
