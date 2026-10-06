#include "TouchInput.h"

#include <algorithm>
#include <cmath>

static constexpr uint32_t kTapMaxMs = 350;
static constexpr uint32_t kDoubleTapMs = 320;
static constexpr uint32_t kLongPressMs = 450;

void TouchInput::setScreenSize(int w, int h) {
    w_ = std::max(1, w);
    h_ = std::max(1, h);
    slop_ = std::max(8.0f, std::min(w_, h_) * 0.022f);
}

void TouchInput::centroid(float& cx, float& cy, float& spread) const {
    cx = cy = spread = 0;
    if (fingers_.empty()) return;
    for (auto& [id, f] : fingers_) { cx += f.x; cy += f.y; }
    cx /= fingers_.size();
    cy /= fingers_.size();
    for (auto& [id, f] : fingers_) spread += std::hypot(f.x - cx, f.y - cy);
    spread /= fingers_.size();
}

void TouchInput::beginSingle(uint32_t t) {
    float cx, cy, sp;
    centroid(cx, cy, sp);
    lastCx_ = cx; lastCy_ = cy; lastSpread_ = sp;
    samples_.clear();
    pushVelocitySample(cx, cy, t);
}

void TouchInput::pushVelocitySample(float x, float y, uint32_t t) {
    samples_.push_back({x, y, t});
    while (samples_.size() > 2 && t - samples_.front().t > 100) samples_.erase(samples_.begin());
}

void TouchInput::handleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_FINGERDOWN: {
        Finger f{e.tfinger.x * w_, e.tfinger.y * h_, 0, 0};
        f.sx = f.x; f.sy = f.y;
        fingers_[e.tfinger.fingerId] = f;
        if (fingers_.size() == 1) {
            dragging_ = false; multi_ = false; longFired_ = false; candidateTap_ = true;
            downTime_ = e.tfinger.timestamp;
        } else {
            candidateTap_ = false;
            if (!multi_ && !dragging_) emit({Gesture::DragStart, f.x, f.y, 0, 0, 1, (int)fingers_.size()});
            multi_ = true;
            dragging_ = true;
        }
        beginSingle(e.tfinger.timestamp);
        break;
    }
    case SDL_FINGERMOTION: {
        auto it = fingers_.find(e.tfinger.fingerId);
        if (it == fingers_.end()) break;
        it->second.x = e.tfinger.x * w_;
        it->second.y = e.tfinger.y * h_;
        float cx, cy, sp;
        centroid(cx, cy, sp);
        if (!dragging_) {
            const Finger& f = it->second;
            if (std::hypot(f.x - f.sx, f.y - f.sy) > slop_ && !longFired_) {
                dragging_ = true;
                candidateTap_ = false;
                emit({Gesture::DragStart, f.sx, f.sy, 0, 0, 1, 1});
                lastCx_ = f.sx; lastCy_ = f.sy;  // include the slop distance in the first pan
            } else if (longFired_ && std::hypot(f.x - f.sx, f.y - f.sy) > slop_) {
                candidateTap_ = false;
            }
        }
        if (dragging_) {
            if (cx != lastCx_ || cy != lastCy_)
                emit({Gesture::Pan, cx, cy, cx - lastCx_, cy - lastCy_, 1, (int)fingers_.size()});
            if (fingers_.size() >= 2 && lastSpread_ > 1 && sp > 1 && sp != lastSpread_)
                emit({Gesture::Pinch, cx, cy, 0, 0, sp / lastSpread_, (int)fingers_.size()});
            pushVelocitySample(cx, cy, e.tfinger.timestamp);
        }
        lastCx_ = cx; lastCy_ = cy; lastSpread_ = sp;
        break;
    }
    case SDL_FINGERUP: {
        auto it = fingers_.find(e.tfinger.fingerId);
        if (it == fingers_.end()) break;
        Finger f = it->second;
        fingers_.erase(it);
        uint32_t t = e.tfinger.timestamp;
        if (!fingers_.empty()) {  // still pinching / dragging with the rest: re-anchor to avoid jumps
            beginSingle(t);
            break;
        }
        if (dragging_) {
            float vx = 0, vy = 0;
            if (!multi_ && samples_.size() >= 2) {
                const Sample& a = samples_.front();
                const Sample& b = samples_.back();
                float dt = (b.t - a.t) / 1000.0f;
                if (dt > 0.008f && t - b.t < 60) { vx = (b.x - a.x) / dt; vy = (b.y - a.y) / dt; }
            }
            emit({Gesture::DragEnd, f.x, f.y, vx, vy, 1, 1});
        } else if (candidateTap_ && !longFired_ && t - downTime_ <= kTapMaxMs) {
            emit({Gesture::Tap, f.x, f.y});
            if (t - lastTapTime_ <= kDoubleTapMs && std::hypot(f.x - lastTapX_, f.y - lastTapY_) < slop_ * 3) {
                emit({Gesture::DoubleTap, f.x, f.y});
                lastTapTime_ = 0;
            } else {
                lastTapTime_ = t; lastTapX_ = f.x; lastTapY_ = f.y;
            }
        }
        dragging_ = multi_ = false;
        break;
    }
    case SDL_MOUSEBUTTONDOWN:
        if (e.button.which == SDL_TOUCH_MOUSEID) break;
        if (e.button.button == SDL_BUTTON_LEFT) {
            mouseDown_ = true; mouseDrag_ = false;
            mx_ = msx_ = (float)e.button.x; my_ = msy_ = (float)e.button.y;
            mouseDownTime_ = e.button.timestamp;
        } else if (e.button.button == SDL_BUTTON_RIGHT) {
            emit({Gesture::LongPress, (float)e.button.x, (float)e.button.y});
        }
        break;
    case SDL_MOUSEMOTION:
        if (e.motion.which == SDL_TOUCH_MOUSEID || !mouseDown_) break;
        if (!mouseDrag_ && std::hypot(e.motion.x - msx_, e.motion.y - msy_) > 4) {
            mouseDrag_ = true;
            emit({Gesture::DragStart, msx_, msy_});
        }
        if (mouseDrag_) emit({Gesture::Pan, (float)e.motion.x, (float)e.motion.y, e.motion.x - mx_, e.motion.y - my_});
        mx_ = (float)e.motion.x; my_ = (float)e.motion.y;
        break;
    case SDL_MOUSEBUTTONUP:
        if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT || !mouseDown_) break;
        mouseDown_ = false;
        if (mouseDrag_) emit({Gesture::DragEnd, (float)e.button.x, (float)e.button.y});
        else {
            emit({Gesture::Tap, (float)e.button.x, (float)e.button.y});
            if (e.button.clicks >= 2) emit({Gesture::DoubleTap, (float)e.button.x, (float)e.button.y});
        }
        break;
    case SDL_MOUSEWHEEL: {
        if (e.wheel.which == SDL_TOUCH_MOUSEID || e.wheel.y == 0) break;
        int x, y;
        SDL_GetMouseState(&x, &y);
        emit({Gesture::Pinch, (float)x, (float)y, 0, 0, std::pow(1.15f, (float)e.wheel.y)});
        break;
    }
    default:
        break;
    }
}

void TouchInput::update(uint32_t now) {
    if (fingers_.size() == 1 && !dragging_ && !longFired_ && now - downTime_ >= kLongPressMs) {
        const Finger& f = fingers_.begin()->second;
        longFired_ = true;
        candidateTap_ = false;
        emit({Gesture::LongPress, f.x, f.y});
    }
}

std::vector<Gesture> TouchInput::take() {
    std::vector<Gesture> g;
    g.swap(out_);
    return g;
}
