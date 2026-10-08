// See controls.h.
#include "controls.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gameicons.h"
#include "overlay.h"

// ---------------------------------------------------------------------------------------------
// The buttons show the game's own pictures (from its interface file, see gameicons.h) where it
// has one, and press the game's shortcut keys (Options > Shortcuts, default set). The right side
// shows the selected character's actions: which ones is found by comparing the game's action
// ring with the game's action pictures; tapping one clicks it on the ring (so two quick taps
// are a double click: the gun reloads).

// a place in the game's picture (1024 wide; y counted from the bottom when fromBottom, since
// the bottom toolbar sits at the bottom whatever the picture height), measured from screenshots
typedef struct { float x, y, radius; int fromBottom; } spot_t;
// the action ring next to the portrait: the character's 5 actions (or, with a gun out, the
// cartridges to reload)
static const spot_t kRing[5] = {
    {119.3f, 18.3f, 18.5f, 0}, {108.3f, 54.3f, 18.5f, 0}, {86.7f, 87.3f, 18.5f, 0}, {55.0f, 111.0f, 18.5f, 0}, {19.3f, 122.7f, 18.5f, 0},
};

typedef enum { B_NONE, B_KEY, B_RIGHTMODE, B_RING, B_SWAP } btype_t;
typedef struct {
    btype_t type;
    int icon;                // the app's own icon (atlas), used when there's no game picture
    int gameId;              // the game's picture (BTTN id in default.res), 0 = none
    int slot;                // B_RING: which circle of the action ring
    SDL_Scancode scancode;   // B_KEY
    SDL_Keycode key;
} button_t;
enum { ROWS = 6, PAGES = 2 };
static const button_t kLeft[ROWS] = {
    {B_RIGHTMODE, ICON_RIGHT, 0, 0, 0, 0},                          // the next tap is a right click
    {B_KEY, ICON_CROUCH, 51, 0, SDL_SCANCODE_C, SDLK_c},            // crawl
    {B_KEY, ICON_STAND, 52, 0, SDL_SCANCODE_S, SDLK_s},             // stand up
    {B_KEY, ICON_VIEW, 96, 0, SDL_SCANCODE_LALT, SDLK_LALT},        // field of vision: then tap an enemy
    {B_KEY, ICON_PAUSE, 50, 0, SDL_SCANCODE_Q, SDLK_q},             // quick action (the watch)
    {B_KEY, ICON_ALL, 0, 0, SDL_SCANCODE_A, SDLK_a},                // select all
};
static const button_t kRight[PAGES][ROWS] = {
    {   // the selected character's actions, as on the action ring
        {B_RING, -1, 0, 0, 0, 0}, {B_RING, -1, 0, 1, 0, 0}, {B_RING, -1, 0, 2, 0, 0},
        {B_RING, -1, 0, 3, 0, 0}, {B_RING, -1, 0, 4, 0, 0},
        {B_SWAP, ICON_SWAP, 0, 0, 0, 0},
    },
    {   // the game
        {B_KEY, ICON_MENU, 0, 0, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE},
        {B_KEY, ICON_SAVE, 0, 0, SDL_SCANCODE_F5, SDLK_F5},
        {B_KEY, ICON_LOAD, 0, 0, SDL_SCANCODE_F8, SDLK_F8},
        {B_KEY, ICON_MAP, 0, 0, SDL_SCANCODE_M, SDLK_m},
        {B_NONE, -1, 0, 0, 0, 0},
        {B_SWAP, ICON_SWAP, 0, 0, 0, 0},
    },
};
enum { BUTTONS = 2 * ROWS };

// ---------------------------------------------------------------------------------------------

typedef enum { ROLE_NONE, ROLE_GAME, ROLE_BUTTON, ROLE_PINCH } role_t;

static struct {
    desp_replay* r;
    int sw, sh;
    int win[4], pic[4];      // whole game window / its picture on screen (GL coords)
    int view[7];             // see desp_replay_view
    // the first finger
    role_t role;
    SDL_FingerID id, id2;
    float sx, sy, x, y, lx, ly;   // start, now, last pan step (screen px, top-left origin)
    Uint32 downAt, lastMove;
    int dragged, longDone;
    float vx, vy;            // drag speed (screen px / s)
    int button;              // ROLE_BUTTON: which
    // pinch / two fingers
    float x2, y2, pinchBase;
    Uint32 pinchAt;
    int pinchMoved;
    // camera glide
    int gliding;
    Uint32 glideT;
    // modes
    int rightNext;
    // action wheel
    int wheel, wheelSel;
    float wx, wy;            // wheel center (screen px, top-left origin)
    float tx, ty;            // the spot the action goes to
    // a click to send a few frames later (after an action key)
    Uint32 pendingAt;
    int pendingX, pendingY;
    // keys are let go a couple of frames after they were pressed (the game reads key states
    // once per frame, so a press and release in the same frame could go unnoticed)
    struct { SDL_Scancode sc; SDL_Keycode k; Uint32 at; } up[8];
    int stopTap;             // this touch only stopped a glide: no click
    int page;                // right side: 0 = actions, 1 = menu / save / load / map
    int ringIcon[5], ringMiss[5];  // the action ring's pictures (BTTN id, -1 = unknown)
    Uint32 ringAt;
} c;

static int forceMission;
static int inMission(void) { return forceMission || (c.view[6] & VIEW_IN_MISSION) != 0; }
void controls_debug(int mission, int wheelX, int wheelY, int sel);

void controls_init(desp_replay* r) {
    memset(&c, 0, sizeof c);
    c.r = r;
    c.wheelSel = -1;
    for (int i = 0; i < 5; ++i) c.ringIcon[i] = -1;
    overlay_init();
}

void controls_area(int sw, int sh, int area[4]) {
    c.sw = sw;
    c.sh = sh;
    int bar = (int)(sh * 0.15f);  // room for a column of buttons on each side
    area[0] = bar;
    area[1] = 0;
    area[2] = sw - 2 * bar;
    area[3] = sh;
    desp_replay_view(c.r, c.view);
}

void controls_placed(const int windowRect[4], const int picRect[4]) {
    memcpy(c.win, windowRect, sizeof c.win);
    memcpy(c.pic, picRect, sizeof c.pic);
}

// ---------------------------------------------------------------------------------------------
// to the game

static void toGame(float sx, float sy, int* gx, int* gy) {
    int gw, gh;
    desp_replay_game_size(c.r, &gw, &gh);
    float top = (float)(c.sh - c.win[1] - c.win[3]);
    float u = c.win[2] > 0 ? (sx - c.win[0]) / (float)c.win[2] : 0, v = c.win[3] > 0 ? (sy - top) / (float)c.win[3] : 0;
    if (u < 0) u = 0;
    if (u > 1) u = 1;
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    *gx = (int)(u * gw);
    *gy = (int)(v * gh);
}
static void send(int type, int x, int y, int button) {
    desp_input in;
    memset(&in, 0, sizeof in);
    in.type = (uint32_t)type;
    in.x = x;
    in.y = y;
    in.button = (uint32_t)button;
    desp_replay_send_input(c.r, &in);
}
static void keyEvent(int down, SDL_Scancode sc, SDL_Keycode k) {
    desp_input in;
    memset(&in, 0, sizeof in);
    in.type = down ? IN_KEY_DOWN : IN_KEY_UP;
    in.scancode = (uint32_t)sc;
    in.keycode = (uint32_t)k;
    desp_replay_send_input(c.r, &in);
}
static void key(SDL_Scancode sc, SDL_Keycode k) {
    keyEvent(1, sc, k);
    for (int i = 0; i < 8; ++i)
        if (!c.up[i].at) { c.up[i].sc = sc; c.up[i].k = k; c.up[i].at = SDL_GetTicks() + 70; return; }
    keyEvent(0, sc, k);  // (no room: let go right away)
}
static void cursorTo(float sx, float sy) {
    int gx, gy;
    toGame(sx, sy, &gx, &gy);
    send(IN_MOUSE_TO, gx, gy, 0);
}
static void click(float sx, float sy, int button) {
    int gx, gy;
    toGame(sx, sy, &gx, &gy);
    send(IN_MOUSE_TO, gx, gy, 0);
    send(IN_MOUSE_DOWN, gx, gy, button);
    send(IN_MOUSE_UP, gx, gy, button);
}
// the camera follows a finger that moved by (dx, dy) screen px and is now at (x, y)
static void pan(float dx, float dy, float x, float y) {
    int gw, gh, gx, gy;
    desp_replay_game_size(c.r, &gw, &gh);
    if (c.win[2] <= 0 || c.win[3] <= 0) return;
    toGame(x, y, &gx, &gy);
    desp_input in;
    memset(&in, 0, sizeof in);
    in.type = IN_PAN;
    in.x = gx;
    in.y = gy;
    in.dx = (int32_t)lroundf(dx * gw / c.win[2] * 16);
    in.dy = (int32_t)lroundf(dy * gh / c.win[3] * 16);
    if (in.dx || in.dy) desp_replay_send_input(c.r, &in);
}

// ---------------------------------------------------------------------------------------------
// layout of the buttons (screen px, top-left origin)

static float buttonRadius(void) {
    float barW = (float)c.pic[0];
    float r = barW * 0.40f, maxR = c.sh / (ROWS * 2.25f);
    return r < maxR ? r : maxR;
}
static void buttonCenter(int i, float* x, float* y) {
    int side = i / ROWS, k = i % ROWS;
    float left = (float)c.pic[0], right = (float)(c.pic[0] + c.pic[2]);
    *x = side == 0 ? left / 2 : (right + c.sw) / 2;
    *y = c.sh * (k + 0.5f) / ROWS;
}
static const button_t* buttonAt(int i) { return i < ROWS ? &kLeft[i] : &kRight[c.page][i - ROWS]; }
static int hitButton(float x, float y) {
    if (!inMission() || c.pic[0] < 40) return -1;
    float r = buttonRadius() * 1.2f;
    for (int i = 0; i < BUTTONS; ++i) {
        if (buttonAt(i)->type == B_NONE) continue;  // empty place
        float bx, by;
        buttonCenter(i, &bx, &by);
        if ((x - bx) * (x - bx) + (y - by) * (y - by) <= r * r) return i;
    }
    return -1;
}

// a spot of the game's picture -> game window pixels / texture coordinates
static int spotToWindow(const spot_t* p, float* wx, float* wy, float* wr) {
    float pw = (float)c.view[4], ph = (float)c.view[5];
    if (pw <= 0 || ph <= 0 || c.view[2] <= 0) return 0;
    float k = c.view[2] / pw;
    float py = p->fromBottom ? ph - p->y : p->y;
    *wx = c.view[0] + p->x * k;
    *wy = c.view[1] + py * k;
    *wr = p->radius * k;
    return 1;
}
static void clickSpot(const spot_t* p) {
    float wx, wy, wr;
    if (!spotToWindow(p, &wx, &wy, &wr)) return;
    send(IN_MOUSE_TO, (int)wx, (int)wy, 0);
    send(IN_MOUSE_DOWN, (int)wx, (int)wy, SDL_BUTTON_LEFT);
    send(IN_MOUSE_UP, (int)wx, (int)wy, SDL_BUTTON_LEFT);
}
static void pressButton(int i) {
    const button_t* b = buttonAt(i);
    switch (b->type) {
    case B_RIGHTMODE: c.rightNext = !c.rightNext; break;
    case B_SWAP: c.page = (c.page + 1) % PAGES; break;
    case B_RING: clickSpot(&kRing[b->slot]); break;  // two quick taps = the game's double click (gun: reload)
    case B_KEY: key(b->scancode, b->key); break;
    case B_NONE: break;
    }
}

// Which pictures are on the action ring right now: compare its 5 circles (read back from the
// game's picture) with the game's 40x40 action pictures in all their states.
static void matchRing(void) {
    float pw = (float)c.view[4];
    if (pw <= 0 || c.view[2] <= 0) return;
    float k = c.view[2] / pw;
    int x0 = c.view[0], y0 = c.view[1], w = (int)(145 * k) + 2, h = (int)(145 * k) + 2;
    static unsigned char* buf;
    static int bufN;
    if (bufN < w * h * 4) { free(buf); bufN = w * h * 4; buf = malloc((size_t)bufN); }
    if (!buf || !desp_replay_read_window(c.r, x0, y0, w, h, buf)) return;
    for (int slot = 0; slot < 5; ++slot) {
        float best = 1e30f, second = 1e30f;
        int bestId = -1;
        for (int n = 0; n < gameicons_count(); ++n) {
            const gameicon_t* g = gameicon_at(n);
            if (memcmp(g->tag, "BTTN", 4) || g->w < 36 || g->w > 44 || g->h < 36 || g->h > 44) continue;
            float icx = (g->w - 1) / 2.0f, icy = (g->h - 1) / 2.0f, ssd = 0;
            int cnt = 0;
            for (int dy = -13; dy <= 13; dy += 2)
                for (int dx = -13; dx <= 13; dx += 2) {
                    if (dx * dx + dy * dy > 169) continue;
                    int ax = (int)lroundf(icx + dx), ay = (int)lroundf(icy + dy);
                    const uint8_t* ip = g->rgba + ((size_t)ay * g->w + ax) * 4;
                    if (!ip[3]) continue;
                    int sx = (int)lroundf((kRing[slot].x + dx) * k), sy = (int)lroundf((kRing[slot].y + dy) * k);
                    if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
                    const uint8_t* sp = buf + ((size_t)sy * w + sx) * 4;
                    float dr = (float)sp[0] - ip[0], dg = (float)sp[1] - ip[1], db = (float)sp[2] - ip[2];
                    ssd += dr * dr + dg * dg + db * db;
                    ++cnt;
                }
            if (cnt < 40) continue;
            ssd /= cnt;
            if (ssd < best) { if (g->id != bestId) second = best; best = ssd; bestId = g->id; }
            else if (ssd < second && g->id != bestId) second = ssd;
        }
        if (bestId >= 0 && best < 1500 && best < second * 0.9f) { c.ringIcon[slot] = bestId; c.ringMiss[slot] = 0; }
        else if (++c.ringMiss[slot] >= 3) c.ringIcon[slot] = -1;
    }
}

// ---------------------------------------------------------------------------------------------
// the action wheel

static float wheelRadius(void) { return c.sh * 0.17f; }
static float slotRadius(void) { return c.sh * 0.065f; }
static void slotCenter(int i, float* x, float* y) {
    float a = (float)(-M_PI / 2 + i * 2 * M_PI / 5);
    *x = c.wx + cosf(a) * wheelRadius();
    *y = c.wy + sinf(a) * wheelRadius();
}
static void openWheel(float x, float y) {
    c.wheel = 1;
    c.wheelSel = -1;
    c.tx = x;
    c.ty = y;
    float m = wheelRadius() + slotRadius() * 1.2f;  // keep the whole wheel on the screen
    c.wx = x < m ? m : x > c.sw - m ? c.sw - m : x;
    c.wy = y < m ? m : y > c.sh - m ? c.sh - m : y;
}
static void updateWheel(float x, float y) {
    float dx = x - c.wx, dy = y - c.wy;
    if (dx * dx + dy * dy < (wheelRadius() * 0.45f) * (wheelRadius() * 0.45f)) { c.wheelSel = -1; return; }
    float a = atan2f(dy, dx) + (float)(M_PI / 2);  // 0 = straight up, clockwise
    if (a < 0) a += (float)(2 * M_PI);
    c.wheelSel = (int)lroundf(a / (float)(2 * M_PI / 5)) % 5;
}
static void closeWheel(void) {
    if (c.wheelSel >= 0) {
        // pick the action on the game's ring, then use it on the spot where the finger went down
        clickSpot(&kRing[c.wheelSel]);
        int gx, gy;
        toGame(c.tx, c.ty, &gx, &gy);
        c.pendingX = gx;
        c.pendingY = gy;
        c.pendingAt = SDL_GetTicks() + 150;
    } else {
        click(c.tx, c.ty, SDL_BUTTON_RIGHT);
    }
    c.wheel = 0;
    c.wheelSel = -1;
}

// for the desktop test harness: pretend to be in a mission / show the wheel
void controls_debug(int mission, int wheelX, int wheelY, int sel) {
    forceMission = mission;
    if (wheelX >= 0) { openWheel((float)wheelX, (float)wheelY); c.wheelSel = sel; }
}

// ---------------------------------------------------------------------------------------------

void controls_event(const SDL_Event* ev, int sw, int sh) {
    c.sw = sw;
    c.sh = sh;
    Uint32 now = SDL_GetTicks();
    switch (ev->type) {
    case SDL_FINGERDOWN: {
        float x = ev->tfinger.x * sw, y = ev->tfinger.y * sh;
        if (c.role == ROLE_NONE) {
            c.id = ev->tfinger.fingerId;
            c.sx = c.x = c.lx = x;
            c.sy = c.y = c.ly = y;
            c.downAt = c.lastMove = now;
            c.dragged = c.longDone = 0;
            c.vx = c.vy = 0;
            c.stopTap = c.gliding;
            c.gliding = 0;
            int b = hitButton(x, y);
            if (b >= 0) {
                c.role = ROLE_BUTTON;
                c.button = b;
            } else {
                c.role = ROLE_GAME;
                cursorTo(x, y);
            }
        } else if (c.role == ROLE_GAME && !c.wheel && !c.longDone) {
            // a second finger: pinch to zoom, or a two-finger tap for a right click
            c.role = ROLE_PINCH;
            c.id2 = ev->tfinger.fingerId;
            c.x2 = x;
            c.y2 = y;
            c.pinchBase = hypotf(c.x2 - c.x, c.y2 - c.y);
            c.pinchAt = now;
            c.pinchMoved = c.dragged;
        }
        break;
    }
    case SDL_FINGERMOTION: {
        float x = ev->tfinger.x * sw, y = ev->tfinger.y * sh;
        if (c.role == ROLE_PINCH) {
            if (ev->tfinger.fingerId == c.id) { c.x = x; c.y = y; }
            else if (ev->tfinger.fingerId == c.id2) { c.x2 = x; c.y2 = y; }
            else break;
            float d = hypotf(c.x2 - c.x, c.y2 - c.y);
            if (c.pinchBase > 1) {
                if (d > c.pinchBase * 1.3f) { key(SDL_SCANCODE_KP_PLUS, SDLK_KP_PLUS); c.pinchBase = d; c.pinchMoved = 1; }
                else if (d < c.pinchBase / 1.3f) { key(SDL_SCANCODE_KP_MINUS, SDLK_KP_MINUS); c.pinchBase = d; c.pinchMoved = 1; }
                else if (fabsf(d - c.pinchBase) > c.sh * 0.03f) c.pinchMoved = 1;
            }
            break;
        }
        if (ev->tfinger.fingerId != c.id || c.role != ROLE_GAME) break;
        c.x = x;
        c.y = y;
        if (c.wheel) { updateWheel(x, y); break; }
        if (c.longDone) break;
        float dx = x - c.sx, dy = y - c.sy, slop = sh * 0.02f;
        if (!c.dragged && dx * dx + dy * dy > slop * slop) {
            c.dragged = 1;
            c.lx = c.sx;
            c.ly = c.sy;
        }
        if (c.dragged) {
            float mx = x - c.lx, my = y - c.ly;
            float dt = (now - c.lastMove) / 1000.0f;
            if (dt < 0.001f) dt = 0.001f;
            c.vx = 0.6f * c.vx + 0.4f * (mx / dt);
            c.vy = 0.6f * c.vy + 0.4f * (my / dt);
            c.lastMove = now;
            pan(mx, my, x, y);
            c.lx = x;
            c.ly = y;
        }
        break;
    }
    case SDL_FINGERUP: {
        if (c.role == ROLE_PINCH) {
            if (ev->tfinger.fingerId != c.id && ev->tfinger.fingerId != c.id2) break;
            if (!c.pinchMoved && now - c.pinchAt < 400)
                click((c.x + c.x2) / 2, (c.y + c.y2) / 2, SDL_BUTTON_RIGHT);
            c.role = ROLE_NONE;  // the other finger is ignored until it lifts too
            c.pinchMoved = 1;
            break;
        }
        if (ev->tfinger.fingerId != c.id) break;
        role_t role = c.role;
        c.role = ROLE_NONE;
        if (role == ROLE_BUTTON) {
            if (hitButton(ev->tfinger.x * sw, ev->tfinger.y * sh) == c.button) pressButton(c.button);
        } else if (role == ROLE_GAME) {
            if (c.wheel) closeWheel();
            else if (c.dragged) {
                float speed = hypotf(c.vx, c.vy);
                if (inMission() && now - c.lastMove < 80 && speed > 150) {
                    if (speed > 6000) { c.vx *= 6000 / speed; c.vy *= 6000 / speed; }
                    c.gliding = 1;
                    c.glideT = now;
                }
            } else if (!c.longDone && !c.stopTap) {
                if (c.rightNext) { click(c.sx, c.sy, SDL_BUTTON_RIGHT); c.rightNext = 0; }
                else click(c.sx, c.sy, SDL_BUTTON_LEFT);
            }
        }
        break;
    }
    default: break;
    }
}

void controls_update(void) {
    Uint32 now = SDL_GetTicks();
    if (inMission() && gameicons_count() && now - c.ringAt > 300) {
        c.ringAt = now;
        matchRing();
    }
    // long press: the action wheel in a mission, a right click elsewhere
    if (c.role == ROLE_GAME && !c.dragged && !c.longDone && !c.wheel && now - c.downAt > 450) {
        c.longDone = 1;
        if (inMission()) openWheel(c.sx, c.sy);
        else click(c.sx, c.sy, SDL_BUTTON_RIGHT);
    }
    // the camera glides on after a flick and slows down
    if (c.gliding) {
        float dt = (now - c.glideT) / 1000.0f;
        c.glideT = now;
        if (dt > 0.1f) dt = 0.1f;
        if (dt > 0) {
            pan(c.vx * dt, c.vy * dt, c.x, c.y);
            float k = expf(-4.5f * dt);
            c.vx *= k;
            c.vy *= k;
            if (hypotf(c.vx, c.vy) < 30) c.gliding = 0;
        }
    }
    for (int i = 0; i < 8; ++i)
        if (c.up[i].at && (Sint32)(now - c.up[i].at) >= 0) { keyEvent(0, c.up[i].sc, c.up[i].k); c.up[i].at = 0; }
    if (c.pendingAt && (Sint32)(now - c.pendingAt) >= 0) {
        c.pendingAt = 0;
        send(IN_MOUSE_TO, c.pendingX, c.pendingY, 0);
        send(IN_MOUSE_DOWN, c.pendingX, c.pendingY, SDL_BUTTON_LEFT);
        send(IN_MOUSE_UP, c.pendingX, c.pendingY, SDL_BUTTON_LEFT);
    }
}

// ---------------------------------------------------------------------------------------------
// drawing (GL coordinates: origin bottom-left)

static const float kBack[4] = {0.16f, 0.10f, 0.05f, 0.72f};
static const float kLit[4] = {0.80f, 0.58f, 0.22f, 0.92f};
static const float kEdge[4] = {0.93f, 0.85f, 0.65f, 0.9f};
static const float kInk[4] = {1, 1, 1, 1};

// a picture of the game fitted into a w x h box centered at (x, y) (GL coords)
static int drawGameIcon(int id, int state, float x, float y, float w, float h) {
    const gameicon_t* g = gameicon("BTTN", id, state);
    if (!g) g = gameicon("BTTN", id, 0);
    if (!g) return 0;
    float s = w / g->w < h / g->h ? w / g->w : h / g->h;
    float uv[4] = {g->uv[0], g->uv[3], g->uv[2], g->uv[1]};  // the atlas is top-down
    overlay_texture(gameicons_texture(), uv, x, y, g->w * s, g->h * s, 1);
    return 1;
}

void controls_draw(void* unused) {
    (void)unused;
    overlay_begin(c.sw, c.sh);
    if (inMission() && c.pic[0] >= 40) {
        float r = buttonRadius();
        for (int i = 0; i < BUTTONS; ++i) {
            const button_t* b = buttonAt(i);
            if (b->type == B_NONE) continue;
            float x, y;
            buttonCenter(i, &x, &y);
            int pressed = c.role == ROLE_BUTTON && c.button == i;
            int lit = pressed || (b->type == B_RIGHTMODE && c.rightNext);
            float gy = c.sh - y;
            if (b->type == B_RING) {
                // the action's own round picture fills the button
                int id = c.ringIcon[b->slot];
                if (id < 0 || !drawGameIcon(id, pressed ? 3 : 1, x, gy, 2 * r, 2 * r)) {
                    overlay_disc(x, gy, r * 0.8f, kBack, 0);
                    overlay_disc(x, gy, r * 0.8f, kEdge, 0.05f);
                }
                continue;
            }
            overlay_disc(x, gy, r, lit ? kLit : kBack, 0);
            if (!(b->gameId && drawGameIcon(b->gameId, pressed ? 2 : 0, x, gy, r * 1.5f, r * 1.5f)))
                overlay_icon(b->icon, x, gy, r * 1.62f, kInk);
            overlay_disc(x, gy, r, lit ? kLit : kEdge, 0.06f);
        }
    }
    if (c.wheel) {
        float sr = slotRadius();
        const float shade[4] = {0, 0, 0, 0.45f};
        overlay_disc(c.wx, c.sh - c.wy, wheelRadius() + sr * 1.15f, shade, 0);
        // the middle: lift here for a right click
        overlay_disc(c.wx, c.sh - c.wy, sr * 0.9f, c.wheelSel < 0 ? kLit : kBack, 0);
        overlay_icon(ICON_RIGHT, c.wx, c.sh - c.wy, sr * 1.4f, kInk);
        for (int i = 0; i < 5; ++i) {
            float x, y;
            slotCenter(i, &x, &y);
            float gy = c.sh - y, rr = i == c.wheelSel ? sr * 1.2f : sr;
            int id = c.ringIcon[i];
            if (id < 0 || !drawGameIcon(id, i == c.wheelSel ? 2 : 1, x, gy, 2 * rr, 2 * rr)) {
                overlay_disc(x, gy, rr, kBack, 0);
                overlay_disc(x, gy, rr, kEdge, 0.05f);
            }
        }
    }
}
