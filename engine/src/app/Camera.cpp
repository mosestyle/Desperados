#include "app/Camera.h"

#include <cmath>

void Camera::DragBy(float dx, float dy, int screenH) {
    float s = ScreenScale(screenH);
    x -= dx / s;
    y -= dy / s;
}

void Camera::Release(float sx, float sy, int screenH) {
    float s = ScreenScale(screenH);
    vx = -sx / s;
    vy = -sy / s;
    // slow flicks don't glide
    if (std::fabs(vx) + std::fabs(vy) < 40.0f) vx = vy = 0;
    const float maxV = 4000.0f;
    float v = std::sqrt(vx * vx + vy * vy);
    if (v > maxV) {
        vx *= maxV / v;
        vy *= maxV / v;
    }
}

void Camera::ZoomAt(float factor, float fx, float fy, int screenW, int screenH) {
    float s0 = ScreenScale(screenH);
    float mx = x + fx / s0, my = y + fy / s0;  // map point under the fingers
    viewH /= factor;
    if (viewH < minViewH) viewH = minViewH;
    if (viewH > maxViewH) viewH = maxViewH;
    float s1 = ScreenScale(screenH);
    x = mx - fx / s1;
    y = my - fy / s1;
}

void Camera::Clamp(float mapW, float mapH, int screenW, int screenH) {
    float w = ViewW(screenW, screenH), h = viewH;
    if (h > mapH) {
        viewH = mapH;
        h = mapH;
        w = ViewW(screenW, screenH);
    }
    if (w > mapW) {
        viewH = mapW * (float)screenH / (float)screenW;
        h = viewH;
        w = mapW;
    }
    if (x > mapW - w) {
        x = mapW - w;
        vx = 0;
    }
    if (y > mapH - h) {
        y = mapH - h;
        vy = 0;
    }
    if (x < 0) {
        x = 0;
        vx = 0;
    }
    if (y < 0) {
        y = 0;
        vy = 0;
    }
}

void Camera::Update(float dt, float mapW, float mapH, int screenW, int screenH) {
    if (!dragging && (vx != 0 || vy != 0)) {
        x += vx * dt;
        y += vy * dt;
        float k = std::exp(-4.5f * dt);
        vx *= k;
        vy *= k;
        if (std::fabs(vx) + std::fabs(vy) < 5.0f) vx = vy = 0;
    }
    Clamp(mapW, mapH, screenW, screenH);
}
