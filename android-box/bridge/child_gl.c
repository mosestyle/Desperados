// libGL.so.1 for the game process: every OpenGL call becomes a bridge command (protocol.h),
// replayed by the app with the phone's GPU. Also provides the EGL entry points SDL's offscreen
// video driver uses (libEGL.so.1 is an empty library that depends on this one).
//
// Object names and uniform locations are handed out here, so the game never waits for the app.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "protocol.h"

#define E __attribute__((visibility("default")))
#define TRACE_ONCE(name) do { static int done_; if (!done_) { done_ = 1; fprintf(stderr, "[bridge] %s called\n", name); } } while (0)
typedef unsigned GLenum; typedef int GLint; typedef int GLsizei; typedef unsigned GLuint; typedef float GLfloat;
typedef unsigned char GLubyte; typedef unsigned char GLboolean; typedef char GLchar; typedef intptr_t GLsizeiptr;
typedef intptr_t GLintptr; typedef unsigned GLbitfield; typedef double GLdouble; typedef float GLclampf;

// ---------------------------------------------------------------------------------------------
// connection

static int sock = -1;
static uint8_t* out;
static size_t outLen, outCap;
static int logOn;

static void die(const char* why) {
    fprintf(stderr, "[bridge] %s (%s)\n", why, strerror(errno));
    _exit(1);
}

static void connectBridge(void) {
    if (sock >= 0) return;
    logOn = getenv("DESP_BRIDGE_LOG") != NULL;
    const char* fd = getenv("DESP_BRIDGE_FD");
    if (fd) sock = atoi(fd);
    else {
        const char* path = getenv("DESP_BRIDGE_SOCKET");
        if (!path) die("no DESP_BRIDGE_FD / DESP_BRIDGE_SOCKET");
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un a = {0};
        a.sun_family = AF_UNIX;
        strncpy(a.sun_path, path, sizeof a.sun_path - 1);
        for (int i = 0; connect(sock, (struct sockaddr*)&a, sizeof a) != 0; ++i) {
            if (i > 100) die("cannot connect to the app");
            usleep(50000);
        }
    }
    outCap = 1 << 20;
    out = malloc(outCap);
}

static void writeAll(const void* p, size_t n) {
    const uint8_t* b = p;
    while (n) {
        ssize_t w = write(sock, b, n);
        if (w < 0) { if (errno == EINTR) continue; die("the app went away"); }
        b += w;
        n -= (size_t)w;
    }
}

static void flushOut(void) {
    if (outLen) writeAll(out, outLen);
    outLen = 0;
}

// Starts a message; returns where its payload goes.
static unsigned long opCount[128], msgTotal;
static void noteOp(uint16_t op) {
    if (msgTotal < 6) fprintf(stderr, "[bridge] command %lu: op %u\n", msgTotal + 1, op);
    opCount[op & 127]++;
    if (++msgTotal % (1u << 20) == 0) {  // a flood of commands without a frame: show which ones
        fprintf(stderr, "[bridge] %lu commands so far; most frequent:", msgTotal);
        for (int k = 0; k < 4; ++k) {
            int best = 0;
            for (int i = 1; i < 128; ++i) if (opCount[i] > opCount[best]) best = i;
            if (!opCount[best]) break;
            fprintf(stderr, " op %d x%lu", best, opCount[best]);
            opCount[best] = 0;
        }
        fprintf(stderr, "\n");
        memset(opCount, 0, sizeof opCount);
    }
}

static uint8_t* msg(uint16_t op, size_t len) {
    connectBridge();
    noteOp(op);
    if (outLen + sizeof(desp_msg_header) + len > outCap) {
        flushOut();
        if (sizeof(desp_msg_header) + len > outCap) {  // huge payload: grow the buffer
            outCap = sizeof(desp_msg_header) + len;
            out = realloc(out, outCap);
        }
    }
    desp_msg_header* h = (desp_msg_header*)(out + outLen);
    h->op = op;
    h->reserved = 0;
    h->len = (uint32_t)len;
    uint8_t* p = out + outLen + sizeof *h;
    outLen += sizeof *h + len;
    return p;
}
#define PUT(p, v) do { __typeof__(v) t_ = (v); memcpy(p, &t_, sizeof t_); p += sizeof t_; } while (0)

static void send0(uint16_t op) { msg(op, 0); }
static void send1(uint16_t op, uint32_t a) { uint8_t* p = msg(op, 4); PUT(p, a); }
static void send2(uint16_t op, uint32_t a, uint32_t b) { uint8_t* p = msg(op, 8); PUT(p, a); PUT(p, b); }
static void sendStr(uint16_t op, uint32_t a, uint32_t b, int nb, const char* s) {
    size_t n = strlen(s);
    uint8_t* p = msg(op, 4 * (size_t)nb + n);
    if (nb > 0) PUT(p, a);
    if (nb > 1) PUT(p, b);
    memcpy(p, s, n);
}

// ---------------------------------------------------------------------------------------------
// input from the app: injected into SDL's queue at the end of each frame

typedef union { uint32_t type; uint8_t pad[56]; } fake_event;
static int winWidth(void);
static int winHeight(void);
static int (*pushEvent)(void*);

static void deliverInput(const desp_input* in);

static int framesInFlight;  // finished frames the app hasn't shown yet

// Handles what the app sent. block: wait for at least one message.
static void pollApp(int block) {
    for (int first = 1;; first = 0) {
        struct pollfd pf = {sock, POLLIN, 0};
        if (poll(&pf, 1, block && first ? -1 : 0) <= 0) return;
        desp_msg_header h;
        size_t got = 0;
        while (got < sizeof h) {
            ssize_t r = read(sock, (uint8_t*)&h + got, sizeof h - got);
            if (r <= 0) { if (r < 0 && errno == EINTR) continue; die("lost the app"); }
            got += (size_t)r;
        }
        uint8_t* data = h.len ? malloc(h.len) : NULL;
        for (got = 0; got < h.len;) {
            ssize_t r = read(sock, data + got, h.len - got);
            if (r <= 0) { if (r < 0 && errno == EINTR) continue; die("lost the app"); }
            got += (size_t)r;
        }
        if (h.op == OP_INPUT && h.len >= sizeof(desp_input)) deliverInput((const desp_input*)data);
        else if (h.op == OP_FRAME_DONE && framesInFlight > 0) framesInFlight--;
        free(data);
    }
}

// SDL2 event layouts (stable ABI)
typedef struct { uint32_t type, timestamp, windowID, which; uint8_t button, state, clicks, pad; int32_t x, y; } sdl_button;
typedef struct { uint32_t type, timestamp, windowID, which, state; int32_t x, y, xrel, yrel; } sdl_motion;
typedef struct { uint32_t type, timestamp, windowID, which; int32_t x, y; uint32_t direction; float px, py; } sdl_wheel;
typedef struct { uint32_t type, timestamp, windowID; uint8_t state, repeat, p2, p3; int32_t scancode, sym; uint16_t mod; uint32_t unused; } sdl_key;

// Addresses of a few of the game's globals, from its symbol table (the Linux build keeps it):
// in-process they are plain memory (Box64 maps the 32-bit program at its own addresses).
static struct { int resolved; uintptr_t bootMode, bootX, bootY, winW, winH, gamePos; } gsym;

static void resolveGameSymbols(void) {
    if (gsym.resolved) return;
    gsym.resolved = 1;
    const char* exe = getenv("DESP_GAME_EXE");
    FILE* f = exe ? fopen(exe, "rb") : NULL;
    if (!f) return;
    unsigned char eh[52];
    if (fread(eh, 1, sizeof eh, f) != sizeof eh || eh[4] != 1) { fclose(f); return; }  // ELF32 only
    uint32_t shoff; uint16_t shentsize, shnum;
    memcpy(&shoff, eh + 32, 4); memcpy(&shentsize, eh + 46, 2); memcpy(&shnum, eh + 48, 2);
    for (uint16_t i = 0; i < shnum; ++i) {
        unsigned char sh[40];
        fseek(f, (long)(shoff + (uint32_t)i * shentsize), SEEK_SET);
        if (fread(sh, 1, 40, f) != 40) break;
        uint32_t type, off, size, link, entsize;
        memcpy(&type, sh + 4, 4); memcpy(&off, sh + 16, 4); memcpy(&size, sh + 20, 4); memcpy(&link, sh + 24, 4); memcpy(&entsize, sh + 36, 4);
        if (type != 2 /*SHT_SYMTAB*/ || entsize != 16) continue;
        unsigned char lsh[40];
        fseek(f, (long)(shoff + link * shentsize), SEEK_SET);
        if (fread(lsh, 1, 40, f) != 40) break;
        uint32_t stroff, strsize;
        memcpy(&stroff, lsh + 16, 4); memcpy(&strsize, lsh + 20, 4);
        char* strs = malloc(strsize);
        unsigned char* syms = malloc(size);
        fseek(f, (long)stroff, SEEK_SET);
        if (fread(strs, 1, strsize, f) != strsize) { free(strs); free(syms); break; }
        fseek(f, (long)off, SEEK_SET);
        if (fread(syms, 1, size, f) != size) { free(strs); free(syms); break; }
        for (uint32_t k = 0; k + 16 <= size; k += 16) {
            uint32_t name, value;
            memcpy(&name, syms + k, 4); memcpy(&value, syms + k + 4, 4);
            if (name >= strsize) continue;
            const char* n = strs + name;
            if (!strcmp(n, "desperados_boot_mode")) gsym.bootMode = value;
            else if (!strcmp(n, "boot_cursor_x")) gsym.bootX = value;
            else if (!strcmp(n, "boot_cursor_y")) gsym.bootY = value;
            else if (!strcmp(n, "thqsdlfw_sdl_window_width")) gsym.winW = value;
            else if (!strcmp(n, "thqsdlfw_sdl_window_height")) gsym.winH = value;
            else if (!strcmp(n, "_ZN15SBThreadedInput14mpointPositionE")) gsym.gamePos = value;
        }
        free(strs);
        free(syms);
        break;
    }
    fclose(f);
    if (logOn) fprintf(stderr, "[bridge] game symbols: boot mode %#lx, cursor %#lx\n", (unsigned long)gsym.bootMode, (unsigned long)gsym.gamePos);
}

static void pushMotion(int dx, int dy) {
    fake_event ev;
    memset(&ev, 0, sizeof ev);
    sdl_motion* m = (sdl_motion*)&ev;
    m->type = 0x400; m->windowID = 1; m->xrel = dx; m->yrel = dy;
    pushEvent(&ev);
}

// Cursor to (x, y) in window pixels: slam it into the top-left corner (the game clamps it),
// then move by the target, converted to the boot menu's 1920x1080 units when needed.
static void mouseTo(int x, int y) {
    resolveGameSymbols();
    int bootMode = gsym.bootMode ? *(volatile int32_t*)gsym.bootMode : 1;
    int w = gsym.winW ? *(volatile int32_t*)gsym.winW : winWidth(), h = gsym.winH ? *(volatile int32_t*)gsym.winH : winHeight();
    double tx = x, ty = y;
    if (!bootMode && w > 0 && h > 0) {  // the boot menu: a 1920x1080 picture fitted into the window
        double s = (double)w / 1920 < (double)h / 1080 ? (double)w / 1920 : (double)h / 1080;
        tx = (x - (w - 1920 * s) / 2) / s;
        ty = (y - (h - 1080 * s) / 2) / s;
    }
    pushMotion(-100000, -100000);
    pushMotion((int)(tx + 0.5), (int)(ty + 0.5));
}

static void deliverInput(const desp_input* in) {
    if (!pushEvent) pushEvent = (int (*)(void*))dlsym(RTLD_DEFAULT, "SDL_PushEvent");
    if (!pushEvent) return;
    if (in->type == IN_MOUSE_TO) { mouseTo(in->x, in->y); return; }
    fake_event ev;
    memset(&ev, 0, sizeof ev);
    switch (in->type) {
    case IN_MOUSE_MOTION: {
        sdl_motion* m = (sdl_motion*)&ev;
        m->type = 0x400; m->windowID = 1; m->x = in->x; m->y = in->y; m->xrel = in->dx; m->yrel = in->dy;
        m->state = in->button;
        break;
    }
    case IN_MOUSE_DOWN: case IN_MOUSE_UP: {
        sdl_button* b = (sdl_button*)&ev;
        b->type = in->type == IN_MOUSE_DOWN ? 0x401 : 0x402; b->windowID = 1; b->button = (uint8_t)in->button;
        b->state = in->type == IN_MOUSE_DOWN; b->clicks = 1; b->x = in->x; b->y = in->y;
        break;
    }
    case IN_MOUSE_WHEEL: {
        sdl_wheel* w = (sdl_wheel*)&ev;
        w->type = 0x403; w->windowID = 1; w->x = in->x; w->y = in->y; w->px = (float)in->x; w->py = (float)in->y;
        break;
    }
    case IN_KEY_DOWN: case IN_KEY_UP: {
        sdl_key* k = (sdl_key*)&ev;
        k->type = in->type == IN_KEY_DOWN ? 0x300 : 0x301; k->windowID = 1; k->state = in->type == IN_KEY_DOWN;
        k->scancode = (int32_t)in->scancode; k->sym = (int32_t)in->keycode;
        break;
    }
    case IN_QUIT: ev.type = 0x100; break;
    default: return;
    }
    pushEvent(&ev);
}

// The window is always "focused" for the game (an offscreen window never gets focus events).
static void sendFocus(void) {
    if (!pushEvent) pushEvent = (int (*)(void*))dlsym(RTLD_DEFAULT, "SDL_PushEvent");
    if (!pushEvent) return;
    static const uint8_t kinds[] = {1 /*SHOWN*/, 3 /*EXPOSED*/, 12 /*FOCUS_GAINED*/};
    for (size_t i = 0; i < sizeof kinds; ++i) {
        fake_event ev;
        memset(&ev, 0, sizeof ev);
        ev.type = 0x200;                       // SDL_WINDOWEVENT
        memcpy(ev.pad + 8, &(uint32_t){1}, 4); // windowID
        ev.pad[12] = kinds[i];                 // event
        pushEvent(&ev);
    }
}

static int frameNo;
E void desp_frame_end(void) {
    send0(OP_FRAME_END);
    flushOut();
    ++framesInFlight;
    if (frameNo < 3) { fprintf(stderr, "[bridge] game frame %d sent\n", frameNo + 1); fflush(stderr); }
    if (frameNo == 1) sendFocus();
    pollApp(0);
    // don't run ahead of the screen: wait while 2 frames are still waiting to be shown
    while (framesInFlight >= 2) pollApp(1);
    ++frameNo;
}

// ---------------------------------------------------------------------------------------------
// names chosen here

static GLuint nextName = 1;
static GLuint newName(void) { return nextName++; }

static GLint unpackAlign = 4, unpackRow = 0, packAlign = 4;
static GLuint arrayBuffer;

static size_t pixelSize(GLenum fmt, GLenum type) {
    if (type == 0x8363 || type == 0x8033 || type == 0x8034) return 2;  // 565, 4444, 5551
    if (type == 0x8367 || type == 0x8035) return 4;                    // 8888 rev
    size_t comps = fmt == 0x1907 /*RGB*/ ? 3 : fmt == 0x1908 /*RGBA*/ || fmt == 0x80E1 /*BGRA*/ ? 4
                 : fmt == 0x8227 /*RG*/ ? 2 : 1;                       // RED / ALPHA / LUMINANCE
    size_t bytes = type == 0x1406 /*FLOAT*/ ? 4 : type == 0x1403 || type == 0x1402 /*SHORT*/ ? 2 : 1;
    return comps * bytes;
}
static size_t imageSize(GLsizei w, GLsizei h, GLenum fmt, GLenum type, GLint align, GLint rowLen) {
    if (w <= 0 || h <= 0) return 0;
    size_t bpp = pixelSize(fmt, type), row = (size_t)(rowLen > 0 ? rowLen : w) * bpp;
    size_t stride = (row + (size_t)align - 1) / (size_t)align * (size_t)align;
    return stride * (size_t)(h - 1) + (size_t)w * bpp;
}

// ---------------------------------------------------------------------------------------------
// OpenGL

E GLenum glGetError(void) { return 0; }
E const GLubyte* glGetString(GLenum n) {
    switch (n) {
    case 0x1F00: return (const GLubyte*)"Desperados bridge";
    case 0x1F01: return (const GLubyte*)"Android GPU";
    case 0x1F02: return (const GLubyte*)"3.3.0 Desperados bridge";
    case 0x8B8C: return (const GLubyte*)"1.40";
    default: return (const GLubyte*)"";
    }
}
E const GLubyte* glGetStringi(GLenum n, GLuint i) { (void)n; (void)i; return (const GLubyte*)""; }
static GLint viewport[4] = {0, 0, 1024, 768};
E void glGetIntegerv(GLenum p, GLint* d) {
    switch (p) {
    case 0x821B: *d = 3; break;                   // MAJOR_VERSION
    case 0x821C: *d = 3; break;                   // MINOR_VERSION
    case 0x821D: *d = 0; break;                   // NUM_EXTENSIONS
    case 0x0D33: *d = 4096; break;                // MAX_TEXTURE_SIZE
    case 0x8872: case 0x8B4D: *d = 16; break;     // texture units
    case 0x0BA2: memcpy(d, viewport, sizeof viewport); break;
    case 0x0CF5: *d = unpackAlign; break;
    case 0x0D05: *d = packAlign; break;
    case 0x8CA6: case 0x8CAA: *d = 0; break;      // framebuffer bindings
    default: *d = 0; break;
    }
}
E void glGetFloatv(GLenum p, GLfloat* d) { GLint v[4] = {0}; glGetIntegerv(p, v); d[0] = (GLfloat)v[0]; }
E void glGetBooleanv(GLenum p, GLboolean* d) { GLint v[4] = {0}; glGetIntegerv(p, v); d[0] = v[0] != 0; }
E void glFlush(void) {}
E void glFinish(void) {}
E void glEnable(GLenum c) { send1(OP_ENABLE, c); }
E void glDisable(GLenum c) { send1(OP_DISABLE, c); }
E void glBlendFunc(GLenum s, GLenum d) { send2(OP_BLEND_FUNC, s, d); }
E void glBlendFuncSeparate(GLenum a, GLenum b, GLenum c, GLenum d) { uint8_t* p = msg(OP_BLEND_FUNC_SEP, 16); PUT(p, a); PUT(p, b); PUT(p, c); PUT(p, d); }
E void glBlendEquation(GLenum m) { send1(OP_BLEND_EQUATION, m); }
E void glClear(GLbitfield m) { send1(OP_CLEAR, m); }
E void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { uint8_t* p = msg(OP_CLEAR_COLOR, 16); PUT(p, r); PUT(p, g); PUT(p, b); PUT(p, a); }
E void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    viewport[0] = x; viewport[1] = y; viewport[2] = w; viewport[3] = h;
    uint8_t* p = msg(OP_VIEWPORT, 16); PUT(p, x); PUT(p, y); PUT(p, w); PUT(p, h);
}
E void glScissor(GLint x, GLint y, GLsizei w, GLsizei h) { uint8_t* p = msg(OP_SCISSOR, 16); PUT(p, x); PUT(p, y); PUT(p, w); PUT(p, h); }
E void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { uint8_t* p = msg(OP_COLOR_MASK, 4); PUT(p, r); PUT(p, g); PUT(p, b); PUT(p, a); }
E void glLineWidth(GLfloat w) { uint8_t* p = msg(OP_LINE_WIDTH, 4); PUT(p, w); }
E void glDepthFunc(GLenum f) { send1(OP_DEPTH_FUNC, f); }
E void glDepthMask(GLboolean f) { uint8_t* p = msg(OP_DEPTH_MASK, 1); PUT(p, f); }
E void glPixelStorei(GLenum pname, GLint v) {
    if (pname == 0x0CF5) unpackAlign = v;
    if (pname == 0x0CF2) unpackRow = v;
    if (pname == 0x0D05) packAlign = v;
    uint8_t* p = msg(OP_PIXEL_STORE, 8); PUT(p, pname); PUT(p, v);
}

// textures
E void glActiveTexture(GLenum t) { send1(OP_ACTIVE_TEXTURE, t); }
E void glGenTextures(GLsizei n, GLuint* t) { for (GLsizei i = 0; i < n; ++i) { t[i] = newName(); send1(OP_GEN_TEXTURE, t[i]); } }
E void glDeleteTextures(GLsizei n, const GLuint* t) { for (GLsizei i = 0; i < n; ++i) send1(OP_DELETE_TEXTURE, t[i]); }
E void glBindTexture(GLenum target, GLuint t) { send2(OP_BIND_TEXTURE, target, t); }
E void glTexParameteri(GLenum target, GLenum pname, GLint v) { uint8_t* p = msg(OP_TEX_PARAMETER_I, 12); PUT(p, target); PUT(p, pname); PUT(p, v); }
E void glTexParameterf(GLenum target, GLenum pname, GLfloat v) { glTexParameteri(target, pname, (GLint)v); }
E void glTexImage2D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border, GLenum fmt, GLenum type, const void* data) {
    (void)border;
    size_t n = data ? imageSize(w, h, fmt, type, unpackAlign, unpackRow) : 0;
    uint8_t* p = msg(OP_TEX_IMAGE_2D, 29 + n);
    PUT(p, target); PUT(p, level); PUT(p, ifmt); PUT(p, w); PUT(p, h); PUT(p, fmt); PUT(p, type);
    uint8_t has = data != NULL; PUT(p, has);
    if (n) memcpy(p, data, n);
}
E void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, const void* data) {
    size_t n = data ? imageSize(w, h, fmt, type, unpackAlign, unpackRow) : 0;
    uint8_t* p = msg(OP_TEX_SUB_IMAGE_2D, 32 + n);
    PUT(p, target); PUT(p, level); PUT(p, x); PUT(p, y); PUT(p, w); PUT(p, h); PUT(p, fmt); PUT(p, type);
    if (n) memcpy(p, data, n);
}
E void glGenerateMipmap(GLenum t) { send1(OP_GENERATE_MIPMAP, t); }

// buffers and vertex arrays
E void glGenBuffers(GLsizei n, GLuint* b) { for (GLsizei i = 0; i < n; ++i) { b[i] = newName(); send1(OP_GEN_BUFFER, b[i]); } }
E void glDeleteBuffers(GLsizei n, const GLuint* b) { for (GLsizei i = 0; i < n; ++i) send1(OP_DELETE_BUFFER, b[i]); }
E void glBindBuffer(GLenum target, GLuint b) { if (target == 0x8892) arrayBuffer = b; send2(OP_BIND_BUFFER, target, b); }
E void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    uint8_t* p = msg(OP_BUFFER_DATA, 13 + (data ? (size_t)size : 0));
    uint32_t s = (uint32_t)size; uint8_t has = data != NULL;
    PUT(p, target); PUT(p, usage); PUT(p, s); PUT(p, has);
    if (data) memcpy(p, data, (size_t)size);
}
E void glBufferSubData(GLenum target, GLintptr off, GLsizeiptr size, const void* data) {
    uint8_t* p = msg(OP_BUFFER_SUB_DATA, 12 + (size_t)size);
    uint32_t o = (uint32_t)off, s = (uint32_t)size;
    PUT(p, target); PUT(p, o); PUT(p, s);
    memcpy(p, data, (size_t)size);
}
E void glGenVertexArrays(GLsizei n, GLuint* a) { for (GLsizei i = 0; i < n; ++i) { a[i] = newName(); send1(OP_GEN_VERTEX_ARRAY, a[i]); } }
E void glDeleteVertexArrays(GLsizei n, const GLuint* a) { for (GLsizei i = 0; i < n; ++i) send1(OP_DELETE_VERTEX_ARRAY, a[i]); }
E void glBindVertexArray(GLuint a) { send1(OP_BIND_VERTEX_ARRAY, a); }
E void glEnableVertexAttribArray(GLuint i) { send1(OP_ENABLE_ATTRIB, i); }
E void glDisableVertexAttribArray(GLuint i) { send1(OP_DISABLE_ATTRIB, i); }
E void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* ptr) {
    if (!arrayBuffer && ptr) fprintf(stderr, "[bridge] client-side vertex arrays are not supported\n");
    uint8_t* p = msg(OP_ATTRIB_POINTER, 21);
    uint32_t off = (uint32_t)(uintptr_t)ptr;
    PUT(p, index); PUT(p, size); PUT(p, type); PUT(p, norm); PUT(p, stride); PUT(p, off);
}
E void glDrawArrays(GLenum mode, GLint first, GLsizei count) { uint8_t* p = msg(OP_DRAW_ARRAYS, 12); PUT(p, mode); PUT(p, first); PUT(p, count); }
E void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* idx) {
    uint8_t* p = msg(OP_DRAW_ELEMENTS, 16);
    uint32_t off = (uint32_t)(uintptr_t)idx;
    PUT(p, mode); PUT(p, count); PUT(p, type); PUT(p, off);
}

// shaders
E GLuint glCreateShader(GLenum type) { GLuint s = newName(); send2(OP_CREATE_SHADER, s, type); return s; }
E void glShaderSource(GLuint s, GLsizei count, const GLchar* const* str, const GLint* len) {
    size_t total = 0;
    for (GLsizei i = 0; i < count; ++i) total += len && len[i] >= 0 ? (size_t)len[i] : strlen(str[i]);
    uint8_t* p = msg(OP_SHADER_SOURCE, 4 + total);
    PUT(p, s);
    for (GLsizei i = 0; i < count; ++i) {
        size_t n = len && len[i] >= 0 ? (size_t)len[i] : strlen(str[i]);
        memcpy(p, str[i], n);
        p += n;
    }
}
E void glCompileShader(GLuint s) { send1(OP_COMPILE_SHADER, s); }
E void glGetShaderiv(GLuint s, GLenum p, GLint* v) { (void)s; *v = p == 0x8B81 /*COMPILE_STATUS*/ ? 1 : 0; }
E void glGetShaderInfoLog(GLuint s, GLsizei max, GLsizei* len, GLchar* log) { (void)s; if (len) *len = 0; if (max > 0) log[0] = 0; }
E void glDeleteShader(GLuint s) { send1(OP_DELETE_SHADER, s); }
E GLuint glCreateProgram(void) { GLuint p = newName(); send1(OP_CREATE_PROGRAM, p); return p; }
E void glAttachShader(GLuint p, GLuint s) { send2(OP_ATTACH_SHADER, p, s); }
E void glDetachShader(GLuint p, GLuint s) { send2(OP_DETACH_SHADER, p, s); }
E void glBindAttribLocation(GLuint p, GLuint i, const GLchar* n) { sendStr(OP_BIND_ATTRIB_LOCATION, p, i, 2, n); }
E void glBindFragDataLocation(GLuint p, GLuint c, const GLchar* n) { (void)p; (void)c; (void)n; }
E void glLinkProgram(GLuint p) { send1(OP_LINK_PROGRAM, p); }
E void glGetProgramiv(GLuint p, GLenum q, GLint* v) { (void)p; *v = q == 0x8B82 /*LINK_STATUS*/ || q == 0x8B83 /*VALIDATE*/ ? 1 : 0; }
E void glGetProgramInfoLog(GLuint p, GLsizei max, GLsizei* len, GLchar* log) { (void)p; if (len) *len = 0; if (max > 0) log[0] = 0; }
E void glValidateProgram(GLuint p) { (void)p; }
E void glUseProgram(GLuint p) { send1(OP_USE_PROGRAM, p); }
E void glDeleteProgram(GLuint p) { send1(OP_DELETE_PROGRAM, p); }

// uniform locations: one per (program, name), numbered here
typedef struct { GLuint prog; char name[64]; GLint loc; } uloc;
static uloc ulocs[512];
static int nulocs;
E GLint glGetUniformLocation(GLuint prog, const GLchar* name) {
    for (int i = 0; i < nulocs; ++i)
        if (ulocs[i].prog == prog && !strncmp(ulocs[i].name, name, sizeof ulocs[i].name - 1)) return ulocs[i].loc;
    if (nulocs >= 512) return -1;
    uloc* u = &ulocs[nulocs++];
    u->prog = prog;
    strncpy(u->name, name, sizeof u->name - 1);
    u->loc = nulocs;  // 1-based, unique over all programs
    sendStr(OP_UNIFORM_LOCATION, prog, (uint32_t)u->loc, 2, name);
    return u->loc;
}
E GLint glGetAttribLocation(GLuint prog, const GLchar* name) { (void)prog; (void)name; return 0; }
static void uniI(GLint loc, int n, const GLint* v) { uint8_t* p = msg(OP_UNIFORM_I, 8 + 4 * (size_t)n); uint32_t c = (uint32_t)n; PUT(p, loc); PUT(p, c); memcpy(p, v, 4 * (size_t)n); }
static void uniF(GLint loc, int n, const GLfloat* v) { uint8_t* p = msg(OP_UNIFORM_F, 8 + 4 * (size_t)n); uint32_t c = (uint32_t)n; PUT(p, loc); PUT(p, c); memcpy(p, v, 4 * (size_t)n); }
E void glUniform1i(GLint l, GLint a) { GLint v[] = {a}; uniI(l, 1, v); }
E void glUniform2i(GLint l, GLint a, GLint b) { GLint v[] = {a, b}; uniI(l, 2, v); }
E void glUniform1f(GLint l, GLfloat a) { GLfloat v[] = {a}; uniF(l, 1, v); }
E void glUniform2f(GLint l, GLfloat a, GLfloat b) { GLfloat v[] = {a, b}; uniF(l, 2, v); }
E void glUniform3f(GLint l, GLfloat a, GLfloat b, GLfloat c) { GLfloat v[] = {a, b, c}; uniF(l, 3, v); }
E void glUniform4f(GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d) { GLfloat v[] = {a, b, c, d}; uniF(l, 4, v); }
static void uniFV(GLint l, uint32_t comps, GLsizei count, const GLfloat* v) {
    uint8_t* p = msg(OP_UNIFORM_FV, 12 + 4 * (size_t)comps * (size_t)count);
    uint32_t c = (uint32_t)count;
    PUT(p, l); PUT(p, comps); PUT(p, c);
    memcpy(p, v, 4 * (size_t)comps * (size_t)count);
}
E void glUniform1fv(GLint l, GLsizei c, const GLfloat* v) { uniFV(l, 1, c, v); }
E void glUniform2fv(GLint l, GLsizei c, const GLfloat* v) { uniFV(l, 2, c, v); }
E void glUniform3fv(GLint l, GLsizei c, const GLfloat* v) { uniFV(l, 3, c, v); }
E void glUniform4fv(GLint l, GLsizei c, const GLfloat* v) { uniFV(l, 4, c, v); }
static void uniM(GLint l, uint32_t dim, GLsizei count, GLboolean tr, const GLfloat* v) {
    uint8_t* p = msg(OP_UNIFORM_MATRIX_FV, 13 + 4 * (size_t)dim * dim * (size_t)count);
    uint32_t c = (uint32_t)count;
    PUT(p, l); PUT(p, dim); PUT(p, c); PUT(p, tr);
    memcpy(p, v, 4 * (size_t)dim * dim * (size_t)count);
}
E void glUniformMatrix2fv(GLint l, GLsizei c, GLboolean t, const GLfloat* v) { uniM(l, 2, c, t, v); }
E void glUniformMatrix3fv(GLint l, GLsizei c, GLboolean t, const GLfloat* v) { uniM(l, 3, c, t, v); }
E void glUniformMatrix4fv(GLint l, GLsizei c, GLboolean t, const GLfloat* v) { uniM(l, 4, c, t, v); }

// framebuffers
E void glGenFramebuffers(GLsizei n, GLuint* f) { for (GLsizei i = 0; i < n; ++i) { f[i] = newName(); send1(OP_GEN_FRAMEBUFFER, f[i]); } }
E void glDeleteFramebuffers(GLsizei n, const GLuint* f) { for (GLsizei i = 0; i < n; ++i) send1(OP_DELETE_FRAMEBUFFER, f[i]); }
E void glBindFramebuffer(GLenum t, GLuint f) { send2(OP_BIND_FRAMEBUFFER, t, f); }
E void glFramebufferTexture2D(GLenum t, GLenum a, GLenum tt, GLuint tex, GLint l) {
    uint8_t* p = msg(OP_FRAMEBUFFER_TEXTURE_2D, 20); PUT(p, t); PUT(p, a); PUT(p, tt); PUT(p, tex); PUT(p, l);
}
E GLenum glCheckFramebufferStatus(GLenum t) { (void)t; return 0x8CD5; }

E void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, void* data) {
    uint8_t* p = msg(OP_READ_PIXELS, 24);
    PUT(p, x); PUT(p, y); PUT(p, w); PUT(p, h); PUT(p, fmt); PUT(p, type);
    flushOut();
    // wait for the pixels (input arriving meanwhile is delivered as usual)
    for (;;) {
        desp_msg_header hd;
        size_t got = 0;
        while (got < sizeof hd) {
            ssize_t r = read(sock, (uint8_t*)&hd + got, sizeof hd - got);
            if (r <= 0) { if (r < 0 && errno == EINTR) continue; die("lost the app"); }
            got += (size_t)r;
        }
        uint8_t* buf = malloc(hd.len ? hd.len : 1);
        for (got = 0; got < hd.len;) {
            ssize_t r = read(sock, buf + got, hd.len - got);
            if (r <= 0) { if (r < 0 && errno == EINTR) continue; die("lost the app"); }
            got += (size_t)r;
        }
        if (hd.op == OP_PIXELS) {
            size_t n = imageSize(w, h, fmt, type, packAlign, 0);
            memcpy(data, buf, hd.len < n ? hd.len : n);
            free(buf);
            return;
        }
        if (hd.op == OP_INPUT && hd.len >= sizeof(desp_input)) deliverInput((const desp_input*)buf);
        else if (hd.op == OP_FRAME_DONE && framesInFlight > 0) framesInFlight--;
        free(buf);
    }
}

// ---------------------------------------------------------------------------------------------
// GL 1.x / desktop-only calls the game might touch: harmless no-ops

E void glPolygonMode(GLenum f, GLenum m) { (void)f; (void)m; }
E void glDrawBuffer(GLenum b) { (void)b; }
E void glReadBuffer(GLenum b) { (void)b; }
E void glHint(GLenum t, GLenum m) { (void)t; (void)m; }
E void glClearDepth(GLdouble d) { (void)d; }
E void glCullFace(GLenum m) { (void)m; }
E void glFrontFace(GLenum m) { (void)m; }

// ---------------------------------------------------------------------------------------------
// lookups

struct fn { const char* n; void* f; };
#define F(x) {#x, (void*)x}
static const struct fn fns[] = {
    F(glGetError), F(glGetString), F(glGetStringi), F(glGetIntegerv), F(glGetFloatv), F(glGetBooleanv), F(glFlush), F(glFinish),
    F(glEnable), F(glDisable), F(glBlendFunc), F(glBlendFuncSeparate), F(glBlendEquation), F(glClear), F(glClearColor),
    F(glViewport), F(glScissor), F(glColorMask), F(glLineWidth), F(glDepthFunc), F(glDepthMask), F(glPixelStorei),
    F(glActiveTexture), F(glGenTextures), F(glDeleteTextures), F(glBindTexture), F(glTexParameteri), F(glTexParameterf),
    F(glTexImage2D), F(glTexSubImage2D), F(glGenerateMipmap), F(glGenBuffers), F(glDeleteBuffers), F(glBindBuffer),
    F(glBufferData), F(glBufferSubData), F(glGenVertexArrays), F(glDeleteVertexArrays), F(glBindVertexArray),
    F(glEnableVertexAttribArray), F(glDisableVertexAttribArray), F(glVertexAttribPointer), F(glDrawArrays), F(glDrawElements),
    F(glCreateShader), F(glShaderSource), F(glCompileShader), F(glGetShaderiv), F(glGetShaderInfoLog), F(glDeleteShader),
    F(glCreateProgram), F(glAttachShader), F(glDetachShader), F(glBindAttribLocation), F(glBindFragDataLocation),
    F(glLinkProgram), F(glGetProgramiv), F(glGetProgramInfoLog), F(glValidateProgram), F(glUseProgram), F(glDeleteProgram),
    F(glGetUniformLocation), F(glGetAttribLocation), F(glUniform1i), F(glUniform2i), F(glUniform1f), F(glUniform2f),
    F(glUniform3f), F(glUniform4f), F(glUniform1fv), F(glUniform2fv), F(glUniform3fv), F(glUniform4fv),
    F(glUniformMatrix2fv), F(glUniformMatrix3fv), F(glUniformMatrix4fv), F(glGenFramebuffers), F(glDeleteFramebuffers),
    F(glBindFramebuffer), F(glFramebufferTexture2D), F(glCheckFramebufferStatus), F(glReadPixels), F(glPolygonMode),
    F(glDrawBuffer), F(glReadBuffer), F(glHint), F(glClearDepth), F(glCullFace), F(glFrontFace),
};
static void* lookup(const char* n) {
    for (size_t i = 0; i < sizeof fns / sizeof fns[0]; ++i)
        if (!strcmp(fns[i].n, n)) return fns[i].f;
    return NULL;
}
static void* lookupAny(const char* n);

E void* glXGetProcAddressARB(const GLubyte* name) { return lookupAny((const char*)name); }
E void* glXGetProcAddress(const GLubyte* name) { return lookupAny((const char*)name); }

static int fakeDisplay;
E void* glXGetCurrentDisplay(void) { return &fakeDisplay; }
E int glXQueryVersion(void* d, int* ma, int* mi) { (void)d; if (ma) *ma = 1; if (mi) *mi = 4; return 1; }
E const char* glXGetClientString(void* d, int n) { (void)d; return n == 2 ? "1.4" : "Desperados bridge"; }
E const char* glXQueryExtensionsString(void* d, int s) { (void)d; (void)s; return ""; }

// ---------------------------------------------------------------------------------------------
// EGL for SDL's offscreen video driver

typedef void* EGLDisplay; typedef void* EGLConfig; typedef void* EGLContext; typedef void* EGLSurface;
typedef int EGLint; typedef unsigned EGLBoolean; typedef unsigned EGLenum;
static int eDpy = 1, eCfg = 1, eCtx = 1, eSurf = 1;
static EGLContext eCurrent;
static int winW = 1024, winH = 768;
static int winWidth(void) { return winW; }
static int winHeight(void) { return winH; }
E EGLDisplay eglGetDisplay(void* n) { TRACE_ONCE("eglGetDisplay"); (void)n; return &eDpy; }
E EGLDisplay eglGetPlatformDisplay(EGLenum p, void* n, const intptr_t* a) { TRACE_ONCE("eglGetPlatformDisplay"); (void)p; (void)n; (void)a; return &eDpy; }
E EGLDisplay eglGetPlatformDisplayEXT(EGLenum p, void* n, const EGLint* a) { TRACE_ONCE("eglGetPlatformDisplayEXT"); (void)p; (void)n; (void)a; return &eDpy; }
E EGLBoolean eglInitialize(EGLDisplay d, EGLint* ma, EGLint* mi) { TRACE_ONCE("eglInitialize"); (void)d; if (ma) *ma = 1; if (mi) *mi = 5; return 1; }
E EGLBoolean eglTerminate(EGLDisplay d) { (void)d; return 1; }
E EGLBoolean eglBindAPI(EGLenum a) { TRACE_ONCE("eglBindAPI"); (void)a; return 1; }
E EGLenum eglQueryAPI(void) { return 0x30A2; }
E EGLint eglGetError(void) { return 0x3000; }
static const char* eglExtensions =
    "EGL_EXT_client_extensions EGL_EXT_platform_base EGL_EXT_platform_device EGL_EXT_device_base "
    "EGL_EXT_device_enumeration EGL_EXT_device_query EGL_KHR_create_context EGL_KHR_surfaceless_context";
E const char* eglQueryString(EGLDisplay d, EGLint n) { (void)d; return n == 0x3055 ? eglExtensions : n == 0x308D ? "OpenGL OpenGL_ES" : n == 0x3053 ? "Desperados" : "1.5 Desperados bridge"; }
static int eDevice = 1;
E EGLBoolean eglQueryDevicesEXT(EGLint max, void** devices, EGLint* num) { TRACE_ONCE("eglQueryDevicesEXT"); if (devices && max > 0) devices[0] = &eDevice; *num = 1; return 1; }
E const char* eglQueryDeviceStringEXT(void* dev, EGLint name) { (void)dev; (void)name; return ""; }
E EGLBoolean eglQueryDeviceAttribEXT(void* dev, EGLint a, intptr_t* v) { (void)dev; (void)a; *v = 0; return 0; }
E EGLBoolean eglGetConfigs(EGLDisplay d, EGLConfig* c, EGLint s, EGLint* n) { (void)d; if (c && s > 0) c[0] = &eCfg; *n = 1; return 1; }
E EGLBoolean eglChooseConfig(EGLDisplay d, const EGLint* a, EGLConfig* c, EGLint s, EGLint* n) { TRACE_ONCE("eglChooseConfig"); (void)d; (void)a; if (c && s > 0) c[0] = &eCfg; *n = 1; return 1; }
E EGLBoolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, EGLint a, EGLint* v) {
    (void)d; (void)c;
    switch (a) {
    case 0x3024: case 0x3023: case 0x3022: case 0x3021: *v = 8; break;  // R G B A
    case 0x3025: *v = 24; break;            // depth
    case 0x3026: *v = 8; break;             // stencil
    case 0x3020: *v = 32; break;            // buffer size
    case 0x3040: *v = 0x4D; break;          // renderable type: GL, ES2, ES3
    case 0x3033: *v = 0x5; break;           // surface type: pbuffer, window
    case 0x3028: *v = 1; break;             // config id
    default: *v = 0;
    }
    return 1;
}
E EGLContext eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext s, const EGLint* a) { TRACE_ONCE("eglCreateContext"); (void)d; (void)c; (void)s; (void)a; return &eCtx; }
E EGLBoolean eglDestroyContext(EGLDisplay d, EGLContext c) { (void)d; (void)c; return 1; }
E EGLSurface eglCreatePbufferSurface(EGLDisplay d, EGLConfig c, const EGLint* a) { TRACE_ONCE("eglCreatePbufferSurface");
    (void)d; (void)c;
    for (const EGLint* p = a; p && *p != 0x3038; p += 2) {
        if (p[0] == 0x3057) winW = p[1];
        if (p[0] == 0x3056) winH = p[1];
    }
    return &eSurf;
}
E EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, void* w, const EGLint* a) { (void)d; (void)c; (void)w; (void)a; return &eSurf; }
E EGLBoolean eglDestroySurface(EGLDisplay d, EGLSurface s) { (void)d; (void)s; return 1; }
E EGLBoolean eglMakeCurrent(EGLDisplay d, EGLSurface r, EGLSurface w, EGLContext c) { TRACE_ONCE("eglMakeCurrent");
    (void)d; (void)r; (void)w;
    static int helloSent;
    if (c && !helloSent) {  // first context: tell the app who we are and how big we draw
        helloSent = 1;
        uint8_t* p = msg(OP_HELLO, 12);
        uint32_t v = DESP_BRIDGE_VERSION;
        PUT(p, v); PUT(p, winW); PUT(p, winH);
        flushOut();
    }
    eCurrent = c;
    return 1;
}
E EGLContext eglGetCurrentContext(void) { return eCurrent; }
E EGLSurface eglGetCurrentSurface(EGLint r) { (void)r; return eCurrent ? &eSurf : 0; }
E EGLDisplay eglGetCurrentDisplay(void) { return &eDpy; }
E EGLBoolean eglSwapBuffers(EGLDisplay d, EGLSurface s) { TRACE_ONCE("eglSwapBuffers"); (void)d; (void)s; desp_frame_end(); return 1; }
E EGLBoolean eglSwapInterval(EGLDisplay d, EGLint i) { (void)d; (void)i; return 1; }
E EGLBoolean eglQuerySurface(EGLDisplay d, EGLSurface s, EGLint a, EGLint* v) { (void)d; (void)s; *v = a == 0x3057 ? winW : a == 0x3056 ? winH : 0; return 1; }
E EGLBoolean eglReleaseThread(void) { return 1; }
E EGLBoolean eglWaitGL(void) { return 1; }
E EGLBoolean eglWaitNative(EGLint e) { (void)e; return 1; }
E EGLBoolean eglWaitClient(void) { return 1; }

static const struct fn eglFns[] = {
    F(eglGetDisplay), F(eglGetPlatformDisplay), F(eglGetPlatformDisplayEXT), F(eglInitialize), F(eglTerminate), F(eglBindAPI),
    F(eglQueryAPI), F(eglGetError), F(eglQueryString), F(eglGetConfigs), F(eglChooseConfig), F(eglGetConfigAttrib),
    F(eglCreateContext), F(eglDestroyContext), F(eglCreatePbufferSurface), F(eglCreateWindowSurface), F(eglDestroySurface),
    F(eglMakeCurrent), F(eglGetCurrentContext), F(eglGetCurrentSurface), F(eglGetCurrentDisplay), F(eglSwapBuffers),
    F(eglSwapInterval), F(eglQuerySurface), F(eglReleaseThread), F(eglWaitGL), F(eglWaitNative), F(eglWaitClient),
    F(eglQueryDevicesEXT), F(eglQueryDeviceStringEXT), F(eglQueryDeviceAttribEXT),
};
static void* lookupAny(const char* n) {
    void* p = lookup(n);
    if (p) return p;
    for (size_t i = 0; i < sizeof eglFns / sizeof eglFns[0]; ++i)
        if (!strcmp(eglFns[i].n, n)) return eglFns[i].f;
    size_t l = strlen(n);
    char b[128];
    if (l > 3 && l < sizeof b && (!strcmp(n + l - 3, "ARB") || !strcmp(n + l - 3, "EXT") || !strcmp(n + l - 3, "OES"))) {
        memcpy(b, n, l - 3);
        b[l - 3] = 0;
        return lookup(b);
    }
    if (logOn) fprintf(stderr, "[bridge] no %s\n", n);
    return NULL;
}
E void* eglGetProcAddress(const char* n) { return lookupAny(n); }
