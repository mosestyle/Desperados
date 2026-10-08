// App side of the bridge: replays the game process's OpenGL commands with OpenGL ES 3 and
// shows the game's picture on the screen.
#pragma once
#include "protocol.h"

typedef struct desp_replay desp_replay;

// `fd`: the app's end of the socket to the game process. Needs a current GLES 3 context.
desp_replay* desp_replay_create(int fd);
void desp_replay_destroy(desp_replay* r);

// Replays commands until the game finishes a frame. Returns 1 for a new frame, 0 when the game
// process is gone, -1 when nothing arrived within `timeoutMs`.
int desp_replay_frame(desp_replay* r, int timeoutMs);

// Draws the latest game frame into the current framebuffer (screenW x screenH), scaled to fit
// with black bars; writes where it landed (screen pixels) to *outRect when not NULL.
void desp_replay_present(desp_replay* r, int screenW, int screenH, int outRect[4]);

// The same, with the game's picture (without the game's own black bars) fitted into `area`
// (x, y, w, h; screen pixels, origin bottom-left; NULL = the whole screen), keeping its shape.
// outRect: where the whole game window landed (for mapping touches); picRect: where the
// picture landed. overlay(ctx) is called afterwards to draw on top (any GL state it changes is
// restored).
void desp_replay_present_ex(desp_replay* r, int screenW, int screenH, const int area[4], int outRect[4],
                            int picRect[4], void (*overlay)(void*), void* ctx);

// The game's picture inside its window (OP_VIEW_RECT): x, y, w, h (window pixels, top-left
// origin), picture width and height in the game's own units, flags (VIEW_IN_MISSION).
void desp_replay_view(desp_replay* r, int view[7]);
// The texture holding the game's window (rows bottom-up), 0 before the game started drawing.
unsigned desp_replay_game_texture(desp_replay* r);

// While waiting for the game's first picture: the same, plus a moving bar near the bottom of
// the screen so it's clear the app is working. `seconds`: time since the game was started.
void desp_replay_present_waiting(desp_replay* r, int screenW, int screenH, float seconds);

// Where the bridge's messages go (shader errors and such); default: logcat / stderr.
void desp_replay_set_logger(void (*fn)(const char* line));

// How much has come from the game so far (bytes, commands).
void desp_replay_stats(desp_replay* r, unsigned long long* bytes, unsigned long long* commands);

// Size of the game's window (0 before the game has created it).
void desp_replay_game_size(desp_replay* r, int* w, int* h);

// Input for the game (mouse coordinates in game window pixels).
void desp_replay_send_input(desp_replay* r, const desp_input* in);

// The game's picture as RGBA (bottom-up rows), for screenshots / tests; returns 0 if none yet.
int desp_replay_read_frame(desp_replay* r, unsigned char* rgba);
