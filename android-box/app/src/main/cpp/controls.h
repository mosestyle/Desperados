// Touch controls for the original game: gestures, the side-bar buttons and the action wheel.
//   tap = click, double tap = run (the game's double click), drag = move the camera (cursor in
//   menus), flick = the camera glides on, pinch = zoom, two-finger tap = right click,
//   long press = action wheel (release in its middle = right click).
#pragma once
#include <SDL.h>

#include "replay.h"

void controls_init(desp_replay* r);
// Where the game's picture goes this frame (x, y, w, h, origin bottom-left): the screen minus
// the side bars.
void controls_area(int sw, int sh, int area[4]);
// After presenting: where the whole game window and its picture landed.
void controls_placed(const int windowRect[4], const int picRect[4]);
void controls_event(const SDL_Event* ev, int sw, int sh);
// Once per loop: long press, camera glide, delayed clicks.
void controls_update(void);
// Draws the buttons and the wheel (called from inside the frame's presentation).
void controls_draw(void* unused);
