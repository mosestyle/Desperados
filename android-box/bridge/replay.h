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

// Size of the game's window (0 before the game has created it).
void desp_replay_game_size(desp_replay* r, int* w, int* h);

// Input for the game (mouse coordinates in game window pixels).
void desp_replay_send_input(desp_replay* r, const desp_input* in);

// The game's picture as RGBA (bottom-up rows), for screenshots / tests; returns 0 if none yet.
int desp_replay_read_frame(desp_replay* r, unsigned char* rgba);
