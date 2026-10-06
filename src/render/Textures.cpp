#include "Textures.h"

#include <algorithm>
#include <cmath>

bool TiledImage::create(SDL_Renderer* r, const Image16& img, int tile) {
    destroy();
    w_ = img.w;
    h_ = img.h;
    for (int ty = 0; ty < img.h; ty += tile) {
        for (int tx = 0; tx < img.w; tx += tile) {
            int tw = std::min(tile, img.w - tx), th = std::min(tile, img.h - ty);
            SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, tw, th);
            if (!t) { SDL_Log("CreateTexture failed: %s", SDL_GetError()); destroy(); return false; }
            SDL_Rect rc{0, 0, tw, th};
            SDL_UpdateTexture(t, &rc, img.px.data() + (size_t)ty * img.w + tx, img.w * 2);
            tiles_.push_back({t, tx, ty, tw, th});
        }
    }
    return true;
}

void TiledImage::destroy() {
    for (auto& t : tiles_) SDL_DestroyTexture(t.tex);
    tiles_.clear();
}

void TiledImage::draw(SDL_Renderer* r, float ox, float oy, float zoom, int sw, int sh) const {
    for (const auto& t : tiles_) {
        // Snap tile edges to whole pixels so neighbouring tiles never leave a hairline gap.
        float x0 = std::floor(ox + t.x * zoom), y0 = std::floor(oy + t.y * zoom);
        float x1 = std::floor(ox + (t.x + t.w) * zoom), y1 = std::floor(oy + (t.y + t.h) * zoom);
        if (x1 < 0 || y1 < 0 || x0 > sw || y0 > sh) continue;
        SDL_FRect dst{x0, y0, x1 - x0, y1 - y0};
        SDL_RenderCopyF(r, t.tex, nullptr, &dst);
    }
}

SDL_Texture* createTexture16(SDL_Renderer* r, const Image16& img, bool colorKey, uint16_t key) {
    if (!colorKey) {
        SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, img.w, img.h);
        if (t) SDL_UpdateTexture(t, nullptr, img.px.data(), img.w * 2);
        return t;
    }
    std::vector<uint32_t> argb(img.px.size());
    for (size_t i = 0; i < img.px.size(); ++i) {
        uint16_t p = img.px[i];
        if (p == key) { argb[i] = 0; continue; }
        uint32_t R = ((p >> 11) & 31) * 255 / 31, G = ((p >> 5) & 63) * 255 / 63, B = (p & 31) * 255 / 31;
        argb[i] = 0xFF000000u | (R << 16) | (G << 8) | B;
    }
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, img.w, img.h);
    if (t) {
        SDL_UpdateTexture(t, nullptr, argb.data(), img.w * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    return t;
}

// 3x5 glyphs, bit 2 = left column
static const uint8_t kDigits[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

float numberWidth(int digits, float px) { return digits * 4 * px - px; }

void drawNumber(SDL_Renderer* r, int value, int digits, float x, float y, float px, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    for (int d = digits - 1; d >= 0; --d) {
        int digit = value % 10;
        value /= 10;
        float gx = x + d * 4 * px;
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 3; ++col)
                if (kDigits[digit][row] & (4 >> col)) {
                    SDL_FRect rc{gx + col * px, y + row * px, px, px};
                    SDL_RenderFillRectF(r, &rc);
                }
    }
}
