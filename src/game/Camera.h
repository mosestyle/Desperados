// 2D camera over the isometric pre-rendered background.
#pragma once
#include <algorithm>
#include <cmath>

struct Camera {
    float cx = 0, cy = 0;      // world point at the centre of the screen
    float zoom = 1;            // screen pixels per world pixel
    float minZoom = 0.1f, maxZoom = 8;
    float vx = 0, vy = 0;      // fling velocity, world px/s
    int screenW = 1, screenH = 1;
    int worldW = 1, worldH = 1;

    // animated "fly to"
    bool flying = false;
    float flyX = 0, flyY = 0, flyZoom = 1;

    void setup(int sw, int sh, int ww, int wh) {
        screenW = sw; screenH = sh; worldW = ww; worldH = wh;
        // Never zoom out past the point where the map stops filling the screen.
        minZoom = std::max((float)sw / ww, (float)sh / wh);
        // The original showed 768 world pixels on a monitor; on a phone ~560 reads well.
        maxZoom = std::max(minZoom * 1.5f, sh / 200.0f);
        clamp();
    }
    float defaultZoom() const { return std::clamp(screenH / 560.0f, minZoom, maxZoom); }

    float toScreenX(float wx) const { return (wx - cx) * zoom + screenW * 0.5f; }
    float toScreenY(float wy) const { return (wy - cy) * zoom + screenH * 0.5f; }
    float toWorldX(float sx) const { return (sx - screenW * 0.5f) / zoom + cx; }
    float toWorldY(float sy) const { return (sy - screenH * 0.5f) / zoom + cy; }
    float originX() const { return toScreenX(0); }
    float originY() const { return toScreenY(0); }

    void pan(float dxScreen, float dyScreen) { cx -= dxScreen / zoom; cy -= dyScreen / zoom; flying = false; clamp(); }

    // Zoom keeping the world point under (sx,sy) fixed on screen.
    void zoomAt(float factor, float sx, float sy) {
        float wx = toWorldX(sx), wy = toWorldY(sy);
        zoom = std::clamp(zoom * factor, minZoom, maxZoom);
        cx = wx - (sx - screenW * 0.5f) / zoom;
        cy = wy - (sy - screenH * 0.5f) / zoom;
        flying = false;
        clamp();
    }

    void flyTo(float wx, float wy, float z) { flying = true; flyX = wx; flyY = wy; flyZoom = std::clamp(z, minZoom, maxZoom); vx = vy = 0; }

    void clamp() {
        float halfW = screenW * 0.5f / zoom, halfH = screenH * 0.5f / zoom;
        if (halfW * 2 >= worldW) cx = worldW * 0.5f; else cx = std::clamp(cx, halfW, worldW - halfW);
        if (halfH * 2 >= worldH) cy = worldH * 0.5f; else cy = std::clamp(cy, halfH, worldH - halfH);
    }

    void update(float dt) {
        if (flying) {
            float k = 1 - std::exp(-dt * 10);
            cx += (flyX - cx) * k; cy += (flyY - cy) * k;
            zoom += (flyZoom - zoom) * k;
            if (std::fabs(flyX - cx) < 0.5f && std::fabs(flyY - cy) < 0.5f && std::fabs(flyZoom - zoom) < 0.002f) {
                cx = flyX; cy = flyY; zoom = flyZoom; flying = false;
            }
            clamp();
        } else if (vx != 0 || vy != 0) {
            float ocx = cx, ocy = cy;
            cx += vx * dt; cy += vy * dt;
            clamp();
            if (cx == ocx) vx = 0;  // hit the edge
            if (cy == ocy) vy = 0;
            float decay = std::exp(-dt * 4.5f);
            vx *= decay; vy *= decay;
            if (std::hypot(vx, vy) < 8) vx = vy = 0;
        }
    }
};
