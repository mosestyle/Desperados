// App side of the bridge (see protocol.h): replays the game's OpenGL commands with OpenGL ES 3.
#include "replay.h"

#include <GLES3/gl3.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdarg.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

static void (*logger)(const char*);
void desp_replay_set_logger(void (*fn)(const char* line)) { logger = fn; }

__attribute__((format(printf, 1, 2))) static void LOGI(const char* fmt, ...) {
    char line[2304];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (logger) { logger(line); return; }
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "DesperadosBridge", "%s", line);
#else
    fprintf(stderr, "[replay] %s\n", line);
#endif
}

typedef struct { GLuint prog; char name[64]; GLint real; int resolved; } uloc_t;

struct desp_replay {
    int fd;
    uint8_t* buf;
    size_t cap;
    // name maps (the game's names -> ours)
    GLuint* names;
    size_t nameCap;
    uloc_t* ulocs;
    int nulocs;
    // the game's default framebuffer = our FBO of the game's window size
    int gameW, gameH;
    GLuint fbo, fboTex;
    GLuint curProgram;
    // present
    GLuint blitProg, blitVao, blitVbo;
    int haveFrame;
    unsigned long long bytes, commands;
    int view[7], hasView;  // the game's picture inside its window (top-left origin), see OP_VIEW_RECT
};

static GLuint* nameSlot(desp_replay* r, uint32_t n) {
    if (n >= r->nameCap) {
        size_t cap = r->nameCap ? r->nameCap : 4096;
        while (cap <= n) cap *= 2;
        r->names = realloc(r->names, cap * sizeof(GLuint));
        memset(r->names + r->nameCap, 0, (cap - r->nameCap) * sizeof(GLuint));
        r->nameCap = cap;
    }
    return &r->names[n];
}
static GLuint real(desp_replay* r, uint32_t n) { return n ? *nameSlot(r, n) : 0; }

desp_replay* desp_replay_create(int fd) {
    desp_replay* r = calloc(1, sizeof *r);
    r->fd = fd;
    return r;
}

void desp_replay_destroy(desp_replay* r) {
    if (!r) return;
    free(r->buf);
    free(r->names);
    free(r->ulocs);
    free(r);
}

void desp_replay_stats(desp_replay* r, unsigned long long* bytes, unsigned long long* commands) {
    *bytes = r->bytes;
    *commands = r->commands;
}

void desp_replay_game_size(desp_replay* r, int* w, int* h) { *w = r->gameW; *h = r->gameH; }

static int readAll(int fd, void* p, size_t n) {
    uint8_t* b = p;
    while (n) {
        ssize_t got = read(fd, b, n);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return 0;
        b += got;
        n -= (size_t)got;
    }
    return 1;
}
static void writeAll(int fd, const void* p, size_t n) {
    const uint8_t* b = p;
    while (n) {
        ssize_t w = write(fd, b, n);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) return;
        b += w;
        n -= (size_t)w;
    }
}
static void sendMsg(desp_replay* r, uint16_t op, const void* data, uint32_t len) {
    desp_msg_header h = {op, 0, len};
    writeAll(r->fd, &h, sizeof h);
    if (len) writeAll(r->fd, data, len);
}

void desp_replay_send_input(desp_replay* r, const desp_input* in) { sendMsg(r, OP_INPUT, in, sizeof *in); }

// ---------------------------------------------------------------------------------------------

static void makeGameFramebuffer(desp_replay* r) {
    if (r->fbo) { glDeleteFramebuffers(1, &r->fbo); glDeleteTextures(1, &r->fboTex); }
    glGenTextures(1, &r->fboTex);
    glBindTexture(GL_TEXTURE_2D, r->fboTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r->gameW, r->gameH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &r->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->fboTex, 0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static char* replaceAll(char* s, const char* from, const char* to) {
    size_t fl = strlen(from), tl = strlen(to), n = 0;
    for (char* p = strstr(s, from); p; p = strstr(p + fl, from)) ++n;
    if (!n) return s;
    char* out = malloc(strlen(s) + n * (tl > fl ? tl - fl : 0) + 1);
    char* o = out;
    for (char *p = s, *q;; p = q + fl) {
        q = strstr(p, from);
        if (!q) { strcpy(o, p); break; }
        memcpy(o, p, (size_t)(q - p));
        o += q - p;
        memcpy(o, to, tl);
        o += tl;
    }
    free(s);
    return out;
}

// GLSL 1.40 -> GLSL ES 3.00
static void shaderSource(GLuint sh, const char* text, size_t len) {
    char* src = malloc(len + 1);
    memcpy(src, text, len);
    src[len] = 0;
    char* v = strstr(src, "#version");
    if (v) { char* eol = strchr(v, '\n'); memset(v, ' ', eol ? (size_t)(eol - v) : strlen(v)); }
    static const char* header = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n";
    char* full = malloc(strlen(header) + strlen(src) + 1);
    strcpy(full, header);
    strcat(full, src);
    free(src);
    full = replaceAll(full, ".a=0;", ".a=0.0;");
    full = replaceAll(full, ".a = 0;", ".a = 0.0;");
    full = replaceAll(full, ".a=1;", ".a=1.0;");
    const GLchar* one = full;
    glShaderSource(sh, 1, &one, NULL);
    free(full);
}

static int unsupportedCap(GLenum cap) {
    switch (cap) {
    case 0x0DE1: case 0x0BC0: case 0x0B50: case 0x0B10: case 0x0B20: case 0x0B41: case 0x809D: case 0x8DB9: case 0x8642: case 0x884F:
        return 1;
    default: return 0;
    }
}

// Texture uploads: GLES 3 wants an internal format matching the data.
static void texFormats(GLint* ifmt, GLenum* fmt, GLenum* type) {
    if (*fmt == 0x80E1 /*BGRA*/) *fmt = GL_RGBA;  // data swapped by the caller
    if (*fmt == GL_RED && *type == GL_UNSIGNED_BYTE) { *ifmt = GL_R8; return; }
    if (*fmt == 0x8227 /*RG*/ && *type == GL_UNSIGNED_BYTE) { *ifmt = GL_RG8; return; }
    if (*type == GL_UNSIGNED_SHORT_5_6_5) { *ifmt = GL_RGB; *fmt = GL_RGB; return; }
    if (*type == GL_UNSIGNED_SHORT_4_4_4_4 || *type == GL_UNSIGNED_SHORT_5_5_5_1) { *ifmt = GL_RGBA; *fmt = GL_RGBA; return; }
    if (*fmt == GL_RGB) *ifmt = GL_RGB;
    else if (*fmt == GL_RGBA) *ifmt = GL_RGBA;
    else if (*fmt == GL_LUMINANCE || *fmt == GL_ALPHA || *fmt == GL_LUMINANCE_ALPHA) *ifmt = (GLint)*fmt;
}
static void swapBGRA(uint8_t* p, size_t n) {
    for (size_t i = 0; i + 3 < n; i += 4) { uint8_t t = p[i]; p[i] = p[i + 2]; p[i + 2] = t; }
}

static GLint uniform(desp_replay* r, GLint vloc) {
    if (vloc <= 0 || vloc > r->nulocs) return -1;
    uloc_t* u = &r->ulocs[vloc - 1];
    if (!u->resolved) {
        u->real = glGetUniformLocation(real(r, u->prog), u->name);
        u->resolved = 1;
    }
    return u->real;
}

#define GET(T) ({ T v_; memcpy(&v_, p, sizeof v_); p += sizeof v_; v_; })

static void execute(desp_replay* r, uint16_t op, uint8_t* p, uint32_t len) {
    uint8_t* end = p + len;
    static int trace = -1;
    if (trace < 0) trace = getenv("DESP_REPLAY_TRACE") ? atoi(getenv("DESP_REPLAY_TRACE")) : 0;
    if (trace > 0) {
        --trace;
        char hex[64] = {0};
        for (uint32_t i = 0; i < len && i < 24; ++i) sprintf(hex + 2 * i, "%02x", p[i]);
        LOGI("op %u len %u: %s", op, len, hex);
    }
    switch (op) {
    case OP_HELLO: {
        uint32_t ver = GET(uint32_t);
        r->gameW = GET(int32_t);
        r->gameH = GET(int32_t);
        LOGI("game connected (bridge v%u), window %dx%d", ver, r->gameW, r->gameH);
        makeGameFramebuffer(r);
        break;
    }
    case OP_VIEW_RECT: {
        memset(r->view, 0, sizeof r->view);
        for (int i = 0; i < 7 && (size_t)(end - p) >= 4; ++i) r->view[i] = GET(int32_t);
        r->hasView = r->view[2] > 0 && r->view[3] > 0;
        break;
    }
    case OP_ENABLE: { GLenum c = GET(uint32_t); if (!unsupportedCap(c)) glEnable(c); break; }
    case OP_DISABLE: { GLenum c = GET(uint32_t); if (!unsupportedCap(c)) glDisable(c); break; }
    case OP_BLEND_FUNC: { GLenum s = GET(uint32_t); GLenum d = GET(uint32_t); glBlendFunc(s, d); break; }
    case OP_BLEND_FUNC_SEP: { GLenum a = GET(uint32_t), b = GET(uint32_t), c = GET(uint32_t), d = GET(uint32_t); glBlendFuncSeparate(a, b, c, d); break; }
    case OP_BLEND_EQUATION: glBlendEquation(GET(uint32_t)); break;
    case OP_CLEAR: glClear(GET(uint32_t)); break;
    case OP_CLEAR_COLOR: { float a = GET(float), b = GET(float), c = GET(float), d = GET(float); glClearColor(a, b, c, d); break; }
    case OP_VIEWPORT: { int x = GET(int32_t), y = GET(int32_t), w = GET(int32_t), h = GET(int32_t); glViewport(x, y, w, h); break; }
    case OP_SCISSOR: { int x = GET(int32_t), y = GET(int32_t), w = GET(int32_t), h = GET(int32_t); glScissor(x, y, w, h); break; }
    case OP_COLOR_MASK: glColorMask(p[0], p[1], p[2], p[3]); break;
    case OP_LINE_WIDTH: glLineWidth(GET(float)); break;
    case OP_DEPTH_FUNC: glDepthFunc(GET(uint32_t)); break;
    case OP_DEPTH_MASK: glDepthMask(p[0]); break;
    case OP_PIXEL_STORE: {
        GLenum pn = GET(uint32_t);
        GLint v = GET(int32_t);
        if (pn == GL_UNPACK_ALIGNMENT || pn == GL_PACK_ALIGNMENT || pn == GL_UNPACK_ROW_LENGTH || pn == GL_PACK_ROW_LENGTH ||
            pn == GL_UNPACK_SKIP_ROWS || pn == GL_UNPACK_SKIP_PIXELS)
            glPixelStorei(pn, v);
        break;
    }
    case OP_ACTIVE_TEXTURE: glActiveTexture(GET(uint32_t)); break;
    case OP_GEN_TEXTURE: { uint32_t n = GET(uint32_t); glGenTextures(1, nameSlot(r, n)); break; }
    case OP_DELETE_TEXTURE: { uint32_t n = GET(uint32_t); GLuint t = real(r, n); if (t) glDeleteTextures(1, &t); *nameSlot(r, n) = 0; break; }
    case OP_BIND_TEXTURE: { GLenum t = GET(uint32_t); uint32_t n = GET(uint32_t); glBindTexture(t, real(r, n)); break; }
    case OP_TEX_PARAMETER_I: {
        GLenum t = GET(uint32_t), pn = GET(uint32_t);
        GLint v = GET(int32_t);
        if (v == 0x2900 /*CLAMP*/ || v == 0x812D /*CLAMP_TO_BORDER*/) v = GL_CLAMP_TO_EDGE;
        glTexParameteri(t, pn, v);
        break;
    }
    case OP_TEX_IMAGE_2D: {
        GLenum t = GET(uint32_t);
        GLint level = GET(int32_t), ifmt = GET(int32_t), w = GET(int32_t), h = GET(int32_t);
        GLenum fmt = GET(uint32_t), type = GET(uint32_t);
        uint8_t has = GET(uint8_t);
        if (has && fmt == 0x80E1) swapBGRA(p, (size_t)(end - p));
        texFormats(&ifmt, &fmt, &type);
        glTexImage2D(t, level, ifmt, w, h, 0, fmt, type, has ? p : NULL);
        break;
    }
    case OP_TEX_SUB_IMAGE_2D: {
        GLenum t = GET(uint32_t);
        GLint level = GET(int32_t), x = GET(int32_t), y = GET(int32_t), w = GET(int32_t), h = GET(int32_t);
        GLenum fmt = GET(uint32_t), type = GET(uint32_t);
        GLint ifmt = 0;
        if (fmt == 0x80E1) swapBGRA(p, (size_t)(end - p));
        texFormats(&ifmt, &fmt, &type);
        glTexSubImage2D(t, level, x, y, w, h, fmt, type, p);
        break;
    }
    case OP_GENERATE_MIPMAP: glGenerateMipmap(GET(uint32_t)); break;
    case OP_GEN_BUFFER: { uint32_t n = GET(uint32_t); glGenBuffers(1, nameSlot(r, n)); break; }
    case OP_DELETE_BUFFER: { uint32_t n = GET(uint32_t); GLuint b = real(r, n); if (b) glDeleteBuffers(1, &b); *nameSlot(r, n) = 0; break; }
    case OP_BIND_BUFFER: { GLenum t = GET(uint32_t); uint32_t n = GET(uint32_t); glBindBuffer(t, real(r, n)); break; }
    case OP_BUFFER_DATA: {
        GLenum t = GET(uint32_t), usage = GET(uint32_t);
        uint32_t size = GET(uint32_t);
        uint8_t has = GET(uint8_t);
        glBufferData(t, size, has ? p : NULL, usage);
        break;
    }
    case OP_BUFFER_SUB_DATA: { GLenum t = GET(uint32_t); uint32_t off = GET(uint32_t), size = GET(uint32_t); glBufferSubData(t, off, size, p); break; }
    case OP_GEN_VERTEX_ARRAY: { uint32_t n = GET(uint32_t); glGenVertexArrays(1, nameSlot(r, n)); break; }
    case OP_DELETE_VERTEX_ARRAY: { uint32_t n = GET(uint32_t); GLuint a = real(r, n); if (a) glDeleteVertexArrays(1, &a); *nameSlot(r, n) = 0; break; }
    case OP_BIND_VERTEX_ARRAY: glBindVertexArray(real(r, GET(uint32_t))); break;
    case OP_ENABLE_ATTRIB: glEnableVertexAttribArray(GET(uint32_t)); break;
    case OP_DISABLE_ATTRIB: glDisableVertexAttribArray(GET(uint32_t)); break;
    case OP_ATTRIB_POINTER: {
        GLuint i = GET(uint32_t);
        GLint size = GET(int32_t);
        GLenum type = GET(uint32_t);
        uint8_t norm = GET(uint8_t);
        GLsizei stride = GET(int32_t);
        uint32_t off = GET(uint32_t);
        glVertexAttribPointer(i, size, type, norm, stride, (const void*)(uintptr_t)off);
        break;
    }
    case OP_CREATE_SHADER: { uint32_t n = GET(uint32_t); GLenum t = GET(uint32_t); *nameSlot(r, n) = glCreateShader(t); break; }
    case OP_SHADER_SOURCE: { uint32_t n = GET(uint32_t); shaderSource(real(r, n), (const char*)p, (size_t)(end - p)); break; }
    case OP_COMPILE_SHADER: {
        GLuint s = real(r, GET(uint32_t));
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { char log[2048]; glGetShaderInfoLog(s, sizeof log, NULL, log); LOGI("shader failed: %s", log); }
        break;
    }
    case OP_DELETE_SHADER: { uint32_t n = GET(uint32_t); GLuint s = real(r, n); if (s) glDeleteShader(s); *nameSlot(r, n) = 0; break; }
    case OP_CREATE_PROGRAM: { uint32_t n = GET(uint32_t); *nameSlot(r, n) = glCreateProgram(); break; }
    case OP_ATTACH_SHADER: { uint32_t a = GET(uint32_t), b = GET(uint32_t); glAttachShader(real(r, a), real(r, b)); break; }
    case OP_DETACH_SHADER: { uint32_t a = GET(uint32_t), b = GET(uint32_t); glDetachShader(real(r, a), real(r, b)); break; }
    case OP_BIND_ATTRIB_LOCATION: {
        uint32_t prog = GET(uint32_t), idx = GET(uint32_t);
        char name[128];
        size_t n = (size_t)(end - p) < sizeof name - 1 ? (size_t)(end - p) : sizeof name - 1;
        memcpy(name, p, n);
        name[n] = 0;
        glBindAttribLocation(real(r, prog), idx, name);
        break;
    }
    case OP_LINK_PROGRAM: {
        uint32_t n = GET(uint32_t);
        GLuint prog = real(r, n);
        glLinkProgram(prog);
        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) { char log[2048]; glGetProgramInfoLog(prog, sizeof log, NULL, log); LOGI("program failed: %s", log); }
        for (int i = 0; i < r->nulocs; ++i)
            if (r->ulocs[i].prog == n) r->ulocs[i].resolved = 0;
        break;
    }
    case OP_USE_PROGRAM: glUseProgram(r->curProgram = real(r, GET(uint32_t))); break;
    case OP_DELETE_PROGRAM: { uint32_t n = GET(uint32_t); GLuint pr = real(r, n); if (pr) glDeleteProgram(pr); *nameSlot(r, n) = 0; break; }
    case OP_UNIFORM_LOCATION: {
        uint32_t prog = GET(uint32_t);
        int32_t vloc = GET(int32_t);
        if (vloc <= 0 || vloc > 65536) break;
        if (vloc > r->nulocs) {
            r->ulocs = realloc(r->ulocs, (size_t)vloc * sizeof(uloc_t));
            memset(r->ulocs + r->nulocs, 0, (size_t)(vloc - r->nulocs) * sizeof(uloc_t));
            r->nulocs = vloc;
        }
        uloc_t* u = &r->ulocs[vloc - 1];
        u->prog = prog;
        size_t n = (size_t)(end - p) < sizeof u->name - 1 ? (size_t)(end - p) : sizeof u->name - 1;
        memcpy(u->name, p, n);
        u->name[n] = 0;
        u->resolved = 0;
        break;
    }
    case OP_UNIFORM_I: {
        GLint loc = uniform(r, GET(int32_t));
        uint32_t c = GET(uint32_t);
        int32_t v[4] = {0};
        memcpy(v, p, 4 * (c > 4 ? 4 : c));
        if (c == 1) glUniform1i(loc, v[0]);
        else if (c == 2) glUniform2i(loc, v[0], v[1]);
        else if (c == 3) glUniform3i(loc, v[0], v[1], v[2]);
        else glUniform4i(loc, v[0], v[1], v[2], v[3]);
        break;
    }
    case OP_UNIFORM_F: {
        GLint loc = uniform(r, GET(int32_t));
        uint32_t c = GET(uint32_t);
        float v[4] = {0};
        memcpy(v, p, 4 * (c > 4 ? 4 : c));
        if (c == 1) glUniform1f(loc, v[0]);
        else if (c == 2) glUniform2f(loc, v[0], v[1]);
        else if (c == 3) glUniform3f(loc, v[0], v[1], v[2]);
        else glUniform4f(loc, v[0], v[1], v[2], v[3]);
        break;
    }
    case OP_UNIFORM_FV: {
        GLint loc = uniform(r, GET(int32_t));
        uint32_t comps = GET(uint32_t), count = GET(uint32_t);
        const float* v = (const float*)p;
        if (comps == 1) glUniform1fv(loc, count, v);
        else if (comps == 2) glUniform2fv(loc, count, v);
        else if (comps == 3) glUniform3fv(loc, count, v);
        else glUniform4fv(loc, count, v);
        break;
    }
    case OP_UNIFORM_MATRIX_FV: {
        GLint loc = uniform(r, GET(int32_t));
        uint32_t dim = GET(uint32_t), count = GET(uint32_t);
        uint8_t tr = GET(uint8_t);
        const float* v = (const float*)p;
        if (dim == 2) glUniformMatrix2fv(loc, count, tr, v);
        else if (dim == 3) glUniformMatrix3fv(loc, count, tr, v);
        else glUniformMatrix4fv(loc, count, tr, v);
        break;
    }
    case OP_DRAW_ARRAYS: { GLenum m = GET(uint32_t); GLint f = GET(int32_t), c = GET(int32_t); if (m == 7 /*QUADS*/) m = GL_TRIANGLE_FAN; glDrawArrays(m, f, c); break; }
    case OP_DRAW_ELEMENTS: { GLenum m = GET(uint32_t); GLint c = GET(int32_t); GLenum t = GET(uint32_t); uint32_t off = GET(uint32_t); glDrawElements(m, c, t, (const void*)(uintptr_t)off); break; }
    case OP_GEN_FRAMEBUFFER: { uint32_t n = GET(uint32_t); glGenFramebuffers(1, nameSlot(r, n)); break; }
    case OP_DELETE_FRAMEBUFFER: { uint32_t n = GET(uint32_t); GLuint f = real(r, n); if (f) glDeleteFramebuffers(1, &f); *nameSlot(r, n) = 0; break; }
    case OP_BIND_FRAMEBUFFER: {
        GLenum t = GET(uint32_t);
        uint32_t n = GET(uint32_t);
        glBindFramebuffer(t == 0x8D40 ? GL_FRAMEBUFFER : t, n ? real(r, n) : r->fbo);  // 0 = the game's window
        break;
    }
    case OP_FRAMEBUFFER_TEXTURE_2D: {
        GLenum t = GET(uint32_t), a = GET(uint32_t), tt = GET(uint32_t);
        uint32_t tex = GET(uint32_t);
        GLint level = GET(int32_t);
        glFramebufferTexture2D(t == 0x8D40 ? GL_FRAMEBUFFER : t, a, tt, real(r, tex), level);
        break;
    }
    case OP_READ_PIXELS: {
        GLint x = GET(int32_t), y = GET(int32_t), w = GET(int32_t), h = GET(int32_t);
        GLenum fmt = GET(uint32_t), type = GET(uint32_t);
        size_t n = (size_t)w * (size_t)h * 4;
        uint8_t* px = malloc(n ? n : 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
        size_t outBytes = n;
        if (fmt == GL_RGB && type == GL_UNSIGNED_BYTE) {  // RGBA -> RGB, rows padded to 4 bytes
            size_t stride = ((size_t)w * 3 + 3) & ~(size_t)3;
            uint8_t* rgb = calloc(stride * (size_t)h + 1, 1);
            for (int yy = 0; yy < h; ++yy)
                for (int xx = 0; xx < w; ++xx) memcpy(rgb + (size_t)yy * stride + (size_t)xx * 3, px + ((size_t)yy * w + xx) * 4, 3);
            free(px);
            px = rgb;
            outBytes = stride * (size_t)h;
        } else if (fmt == 0x80E1) swapBGRA(px, n);
        sendMsg(r, OP_PIXELS, px, (uint32_t)outBytes);
        free(px);
        break;
    }
    default:
        LOGI("unknown command %u (%u bytes)", op, len);
        break;
    }
}

int desp_replay_frame(desp_replay* r, int timeoutMs) {
    for (;;) {
        struct pollfd pf = {r->fd, POLLIN, 0};
        int pr = poll(&pf, 1, timeoutMs);
        if (pr == 0) return -1;
        if (pr < 0) { if (errno == EINTR) continue; return 0; }
        desp_msg_header h;
        if (!readAll(r->fd, &h, sizeof h)) { LOGI("game connection closed (%s)", strerror(errno)); return 0; }
        if (h.len > (256u << 20)) { LOGI("bad message: op %u, %u bytes", h.op, h.len); return 0; }
        if (h.len + 16 > r->cap) {
            r->cap = h.len + 16;
            r->buf = realloc(r->buf, r->cap);
        }
        if (h.len && !readAll(r->fd, r->buf, h.len)) { LOGI("game connection lost in op %u", h.op); return 0; }
        r->bytes += sizeof h + h.len;
        r->commands++;
        if (h.op == OP_FRAME_END) { r->haveFrame = 1; return 1; }
        execute(r, h.op, r->buf, h.len);
    }
}

// ---------------------------------------------------------------------------------------------
// showing the game's picture

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

static void present(desp_replay* r, int sw, int sh, const int area[4], int outRect[4], int picRect[4],
                    float waiting, void (*overlay)(void*), void* ctx) {
    if (!r->blitProg) {
        // (made while the game's objects are bound: put them back afterwards)
        GLint prevVao = 0, prevBuf = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevBuf);
        GLuint vs = compile(GL_VERTEX_SHADER,
            "#version 300 es\nin vec2 pos; out vec2 uv; void main(){ uv = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0.0, 1.0); }");
        GLuint fs = compile(GL_FRAGMENT_SHADER,
            "#version 300 es\nprecision mediump float; in vec2 uv; uniform sampler2D tex; out vec4 color; void main(){ color = vec4(texture(tex, uv).rgb, 1.0); }");
        r->blitProg = glCreateProgram();
        glAttachShader(r->blitProg, vs);
        glAttachShader(r->blitProg, fs);
        glBindAttribLocation(r->blitProg, 0, "pos");
        glLinkProgram(r->blitProg);
        static const float quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
        glGenVertexArrays(1, &r->blitVao);
        glBindVertexArray(r->blitVao);
        glGenBuffers(1, &r->blitVbo);
        glBindBuffer(GL_ARRAY_BUFFER, r->blitVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glBindVertexArray((GLuint)prevVao);
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)prevBuf);
    }
    // the game's state, restored afterwards
    GLint vp[4], sb[4], tex = 0, unit = 0, vao = 0, abuf = 0, fb = 0, bsrc = 0, bdst = 0, bsrcA = 0, bdstA = 0, beq = 0, beqA = 0;
    GLfloat cc[4];
    GLboolean cm[4];
    GLboolean blend = glIsEnabled(GL_BLEND), scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean depth = glIsEnabled(GL_DEPTH_TEST), cull = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_VIEWPORT, vp);
    glGetIntegerv(GL_SCISSOR_BOX, sb);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
    glGetBooleanv(GL_COLOR_WRITEMASK, cm);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &unit);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, cc);
    glGetIntegerv(GL_BLEND_SRC_RGB, &bsrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &bdst);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &bsrcA);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &bdstA);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &beq);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &beqA);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &abuf);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, sw, sh);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    int full[4] = {0, 0, sw, sh};
    if (!area) area = full;
    int rect[4] = {0, 0, sw, sh}, pic[4] = {0, 0, sw, sh};
    if (r->fboTex && r->gameW > 0 && r->gameH > 0) {
        // the part of the game's window that holds its picture (without the game's own bars)
        int vx = 0, vy = 0, vw = r->gameW, vh = r->gameH;
        if (r->hasView) { vx = r->view[0]; vy = r->view[1]; vw = r->view[2]; vh = r->view[3]; }
        // fit it into the area, keeping its shape, centered
        float s = (float)area[2] / vw < (float)area[3] / vh ? (float)area[2] / vw : (float)area[3] / vh;
        pic[2] = (int)(vw * s + 0.5f);
        pic[3] = (int)(vh * s + 0.5f);
        pic[0] = area[0] + (area[2] - pic[2]) / 2;
        pic[1] = area[1] + (area[3] - pic[3]) / 2;
        // and place the whole window around it (its bars fall outside / are cut off)
        rect[0] = pic[0] - (int)(vx * s + 0.5f);
        rect[1] = pic[1] - (int)((r->gameH - vy - vh) * s + 0.5f);
        rect[2] = (int)(r->gameW * s + 0.5f);
        rect[3] = (int)(r->gameH * s + 0.5f);
        glEnable(GL_SCISSOR_TEST);
        glScissor(pic[0], pic[1], pic[2], pic[3]);
        glViewport(rect[0], rect[1], rect[2], rect[3]);
        glUseProgram(r->blitProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, r->fboTex);
        glBindVertexArray(r->blitVao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, sw, sh);
    }
    if (waiting >= 0) {
        // a short bar sliding back and forth along the bottom of the screen
        int barW = sw / 6, barH = sh / 90 > 4 ? sh / 90 : 4, travel = sw / 2 - barW;
        float ph = waiting * 0.6f;
        ph -= (float)(int)ph;
        float k = ph < 0.5f ? ph * 2 : 2 - ph * 2;
        glEnable(GL_SCISSOR_TEST);
        glScissor(sw / 4 + (int)(k * travel), sh / 12, barW, barH);
        glClearColor(0.85f, 0.62f, 0.25f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
    }
    if (outRect) memcpy(outRect, rect, sizeof rect);
    if (picRect) memcpy(picRect, pic, sizeof pic);
    if (overlay) overlay(ctx);
    glUseProgram(r->curProgram);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fb == 0 ? r->fbo : (GLuint)fb);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
    glActiveTexture((GLenum)unit);
    glBindVertexArray((GLuint)vao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)abuf);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glScissor(sb[0], sb[1], sb[2], sb[3]);
    glColorMask(cm[0], cm[1], cm[2], cm[3]);
    glClearColor(cc[0], cc[1], cc[2], cc[3]);
    glBlendFuncSeparate((GLenum)bsrc, (GLenum)bdst, (GLenum)bsrcA, (GLenum)bdstA);
    glBlendEquationSeparate((GLenum)beq, (GLenum)beqA);
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (cull) glEnable(GL_CULL_FACE);
}

void desp_replay_present(desp_replay* r, int sw, int sh, int outRect[4]) {
    present(r, sw, sh, NULL, outRect, NULL, -1, NULL, NULL);
    sendMsg(r, OP_FRAME_DONE, NULL, 0);  // the game may draw the next one
}
void desp_replay_present_ex(desp_replay* r, int sw, int sh, const int area[4], int outRect[4], int picRect[4],
                            void (*overlay)(void*), void* ctx) {
    present(r, sw, sh, area, outRect, picRect, -1, overlay, ctx);
    sendMsg(r, OP_FRAME_DONE, NULL, 0);
}
void desp_replay_present_waiting(desp_replay* r, int sw, int sh, float seconds) {
    present(r, sw, sh, NULL, NULL, NULL, seconds, NULL, NULL);
}

void desp_replay_view(desp_replay* r, int view[7]) {
    memcpy(view, r->view, sizeof r->view);
    if (!r->hasView) { view[0] = view[1] = 0; view[2] = r->gameW; view[3] = r->gameH; view[4] = view[5] = view[6] = 0; }
}
unsigned desp_replay_game_texture(desp_replay* r) { return r->fboTex; }

int desp_replay_read_frame(desp_replay* r, unsigned char* rgba) {
    if (!r->fbo || !r->haveFrame) return 0;
    GLint prev = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, r->gameW, r->gameH, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev);
    return 1;
}
