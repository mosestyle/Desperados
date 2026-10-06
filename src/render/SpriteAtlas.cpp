#include "SpriteAtlas.h"

#include "../formats/SpriteFile.h"

void SpriteAtlas::clear() {
    for (auto* t : pages_) SDL_DestroyTexture(t);
    for (auto* t : singles_) SDL_DestroyTexture(t);
    pages_.clear();
    singles_.clear();
    slots_.clear();
    curX_ = curY_ = shelfH_ = 0;
}

bool SpriteAtlas::allocate(int w, int h, Slot& out) {
    const int pw = w + 2, ph = h + 2;  // 1px transparent gutter against filtering bleed
    if (pw > kPage || ph > kPage) {
        SDL_Texture* t = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
        if (!t) return false;
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        singles_.push_back(t);
        out.tex = t;
        out.rc = {0, 0, w, h};
        return true;
    }
    if (pages_.empty() || curX_ + pw > kPage) {  // next shelf
        curX_ = 0;
        curY_ += shelfH_;
        shelfH_ = 0;
    }
    if (pages_.empty() || curY_ + ph > kPage) {  // next page
        SDL_Texture* t = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, kPage, kPage);
        if (!t) { SDL_Log("Atlas page failed: %s", SDL_GetError()); return false; }
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        // start fully transparent
        std::vector<uint32_t> zero((size_t)kPage * 64, 0);
        for (int y = 0; y < kPage; y += 64) {
            SDL_Rect band{0, y, kPage, 64};
            SDL_UpdateTexture(t, &band, zero.data(), kPage * 4);
        }
        pages_.push_back(t);
        curX_ = curY_ = shelfH_ = 0;
    }
    out.tex = pages_.back();
    out.rc = {curX_ + 1, curY_ + 1, w, h};
    curX_ += pw;
    if (ph > shelfH_) shelfH_ = ph;
    return true;
}

const SpriteAtlas::Slot* SpriteAtlas::get(const SpriteFile& file, int frame) {
    const uint64_t key = ((uint64_t)(uint32_t)file.id() << 32) | (uint32_t)frame;
    auto it = slots_.find(key);
    if (it != slots_.end()) return it->second.tex ? &it->second : nullptr;
    Slot slot;
    if (frame >= 0 && frame < (int)file.frames().size()) {
        const auto& f = file.frames()[frame];
        if (f.w > 0 && f.h > 0 && file.decode(frame, scratch_) && allocate(f.w, f.h, slot))
            SDL_UpdateTexture(slot.tex, &slot.rc, scratch_.data(), f.w * 4);
    }
    auto& stored = slots_[key];
    stored = slot;
    return stored.tex ? &stored : nullptr;
}
