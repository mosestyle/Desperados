// The phone's camera feel (our own, not the original's): drag to move the map, flick to let it
// glide, pinch to zoom. Works in map pixels; the engine draws at the integer part and the app
// shifts the picture by the fraction, so scrolling is smooth at any frame rate.
#pragma once

struct Camera {
    float x = 0, y = 0;            // top-left of the view on the map
    float viewH = 640;             // map pixels shown from top to bottom (zoom)
    float minViewH = 360, maxViewH = 1100;
    float vx = 0, vy = 0;          // glide speed, map pixels per second
    bool dragging = false;

    // screen size in device pixels
    float ScreenScale(int screenH) const { return (float)screenH / viewH; }
    float ViewW(int screenW, int screenH) const { return viewH * (float)screenW / (float)screenH; }

    void DragBy(float dxPixels, float dyPixels, int screenH);   // finger moved by (device px)
    void Release(float speedX, float speedY, int screenH);      // finger lifted at that speed (device px/s)
    void Stop() { vx = vy = 0; }
    void ZoomAt(float factor, float fx, float fy, int screenW, int screenH);  // keep (fx, fy) under the fingers
    void Update(float dt, float mapW, float mapH, int screenW, int screenH);
    void Clamp(float mapW, float mapH, int screenW, int screenH);
};
