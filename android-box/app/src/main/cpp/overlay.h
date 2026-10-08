// The app's own drawing on top of the game: side-bar buttons and the action wheel.
// Coordinates are screen pixels with the origin at the bottom-left (OpenGL style).
#pragma once
#include <GLES3/gl3.h>

enum {
    ICON_RIGHT, ICON_CROUCH, ICON_STAND, ICON_VIEW, ICON_ALL,
    ICON_MENU, ICON_SAVE, ICON_LOAD, ICON_PAUSE, ICON_MAP, ICON_SWAP,
    ICON_COUNT
};

// Loads the shader and the icon atlas (assets/ui_atlas.rgba). Needs the GL context.
int overlay_init(void);
void overlay_shutdown(void);

// Call once per frame before drawing (screen size, sets blending and the program).
void overlay_begin(int screenW, int screenH);

// A filled disc (or ring, when ring > 0: border thickness as a fraction of the radius).
void overlay_disc(float cx, float cy, float radius, const float rgba[4], float ring);
// An icon from the atlas, centered at (cx, cy), size x size pixels, tinted by rgba.
void overlay_icon(int icon, float cx, float cy, float size, const float rgba[4]);
// A round cut-out of a texture (e.g. the game's picture): uv = u0, v0 (bottom), u1, v1 (top).
void overlay_texture_disc(GLuint tex, const float uv[4], float cx, float cy, float radius, float alpha);
