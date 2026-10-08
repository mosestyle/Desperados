// The app's own drawing on the window (not the game's): the engine's picture scaled to the
// screen, and for now a small mission bar (arrows + number) with a built-in 5x7 font.
#pragma once
#include <GLES3/gl3.h>
#include <cstddef>

class Overlay {
public:
    bool Init();
    void Shutdown();
    void Begin(int screenW, int screenH);
    // a texture (rows top to bottom) drawn at x, y (top-left, pixels), w x h
    void Picture(GLuint tex, int texW, int texH, float x, float y, float w, float h);
    void Rect(float x, float y, float w, float h, float r, float g, float b, float a);
    void Text(const char* s, float x, float y, float size, bool centred);
    void LevelBar(size_t level, size_t count, int screenW, int screenH);
    // the mission bar's arrows: -1 previous, +1 next, 0 none
    int HitButton(float x, float y, int screenW, int screenH) const;

private:
    GLuint prog = 0, vao = 0, vbo = 0, white = 0;
    GLint uRect = -1, uUV = -1, uColor = -1, uScreen = -1, uTex = -1;
    float scrW = 1, scrH = 1;
    void Quad(GLuint tex, float x, float y, float w, float h, const float uv[4], const float rgba[4]);
};
