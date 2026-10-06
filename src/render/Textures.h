#pragma once
#include <SDL.h>

#include <vector>

#include "../formats/Image16.h"

// A large 16-bit image split into GPU-friendly tiles (phones often cap textures at 4096).
class TiledImage {
public:
    ~TiledImage() { destroy(); }
    bool create(SDL_Renderer* r, const Image16& img, int tile = 1024);
    void destroy();
    // Draws the image so that world point (0,0) lands at screen (ox,oy), scaled by `zoom`.
    void draw(SDL_Renderer* r, float ox, float oy, float zoom, int screenW, int screenH) const;
    int width() const { return w_; }
    int height() const { return h_; }

private:
    struct Tile { SDL_Texture* tex; int x, y, w, h; };
    std::vector<Tile> tiles_;
    int w_ = 0, h_ = 0;
};

// Small image as a single texture; optional colour key becomes transparent.
SDL_Texture* createTexture16(SDL_Renderer* r, const Image16& img, bool colorKey, uint16_t key = kColorKeyGreen);

// Minimal built-in digit font (no game font decoder yet). Draws 0-9 with filled rects.
void drawNumber(SDL_Renderer* r, int value, int digits, float x, float y, float pixel, SDL_Color c);
float numberWidth(int digits, float pixel);
