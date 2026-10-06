// Turns raw SDL touch / mouse events into high level gestures.
//
//   one finger tap ............ Tap
//   two quick taps ............ Tap + DoubleTap
//   hold still ~0.45 s ........ LongPress   (mouse: right click)
//   one finger drag ........... DragStart / Pan / DragEnd (+ Fling velocity)
//   two finger pinch .......... Pinch (zoom factor around a centre) + Pan
//   mouse wheel ............... Pinch
#pragma once
#include <SDL.h>

#include <map>
#include <vector>

struct Gesture {
    enum Type { Tap, DoubleTap, LongPress, DragStart, Pan, DragEnd, Pinch } type;
    float x = 0, y = 0;    // screen position (pixels) where it happened / centre
    float dx = 0, dy = 0;  // Pan: movement in pixels.  DragEnd: fling velocity px/s
    float scale = 1;       // Pinch: zoom factor for this step
    int fingers = 1;
};

class TouchInput {
public:
    void setScreenSize(int w, int h);
    void handleEvent(const SDL_Event& e);
    void update(uint32_t nowMs);  // long press detection
    std::vector<Gesture> take();  // gestures since last call
    int fingersDown() const { return (int)fingers_.size(); }

private:
    struct Finger { float x, y, sx, sy; };
    void emit(const Gesture& g) { out_.push_back(g); }
    void centroid(float& cx, float& cy, float& spread) const;
    void beginSingle(uint32_t t);
    void pushVelocitySample(float x, float y, uint32_t t);

    int w_ = 1, h_ = 1;
    float slop_ = 12;
    std::map<SDL_FingerID, Finger> fingers_;
    std::vector<Gesture> out_;

    // gesture state
    bool dragging_ = false, multi_ = false, longFired_ = false, candidateTap_ = false;
    uint32_t downTime_ = 0;
    float lastCx_ = 0, lastCy_ = 0, lastSpread_ = 0;
    uint32_t lastTapTime_ = 0;
    float lastTapX_ = -1e9f, lastTapY_ = -1e9f;
    struct Sample { float x, y; uint32_t t; };
    std::vector<Sample> samples_;

    // mouse
    bool mouseDown_ = false, mouseDrag_ = false;
    float mx_ = 0, my_ = 0, msx_ = 0, msy_ = 0;
    uint32_t mouseDownTime_ = 0;
};
