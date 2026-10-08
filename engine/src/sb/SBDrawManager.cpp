#include "sb/SBDrawManager.h"

#include <GLES3/gl3.h>
#include <cstring>

#include "sb/SBFile.h"

SBDrawManager* SBDrawManager::mpDrawManager = nullptr;

// The original's fragment shader (data/shaders/desperados_fragment.txt), with its #ifdef
// switches turned into uniforms.
static const char* kVS =
    "#version 300 es\n"
    "in vec2 a;\n"
    "uniform vec4 uDst;\n"     // x, y, w, h in target pixels (top-left origin)
    "uniform vec4 uSrc;\n"     // u, v, du, dv
    "uniform vec2 uTarget;\n"
    "out vec2 var_uv;\n"
    "void main() {\n"
    "  vec2 p = (uDst.xy + a * uDst.zw) / uTarget * 2.0 - 1.0;\n"
    "  gl_Position = vec4(p, 0.0, 1.0);\n"
    "  var_uv = uSrc.xy + a * uSrc.zw;\n"
    "}\n";
static const char* kFS =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 var_uv;\n"
    "out vec4 var_out;\n"
    "uniform sampler2D TEXTURE_RGB;\n"
    "uniform sampler2D TEXTURE_ALPHA;\n"
    "uniform vec4 colorkey_rgb;\n"
    "uniform vec4 alphakey_rgb;\n"
    "uniform vec4 constant_color;\n"
    "uniform vec4 alpha_constants;\n"
    "uniform bool useColorKey, useAlpha, alphaRed;\n"
    "void main() {\n"
    "  var_out = texture(TEXTURE_RGB, var_uv);\n"
    "  if (useColorKey) {\n"
    "    vec3 diff = var_out.rgb - colorkey_rgb.rgb;\n"
    "    if (dot(diff, diff) < 0.00005) var_out.a = 0.0;\n"
    "  }\n"
    "  {\n"
    "    vec3 diff = var_out.rgb - alphakey_rgb.rgb;\n"
    "    if (dot(diff, diff) < 0.00005) var_out = vec4(0.0, 0.0, 0.0, alphakey_rgb.a);\n"
    "  }\n"
    "  if (useAlpha) {\n"
    "    float alpha = alphaRed ? texture(TEXTURE_ALPHA, var_uv).r : texture(TEXTURE_ALPHA, var_uv).b;\n"
    "    var_out.a *= min(alpha, alpha_constants.z) * alpha_constants.x + alpha_constants.y;\n"
    "  }\n"
    "  var_out *= constant_color;\n"
    "}\n";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        SBLog("SBDrawManager shader: %s", log);
    }
    return s;
}

SBDrawManager::SBDrawManager() { mpDrawManager = this; }

SBDrawManager::~SBDrawManager() {
    CloseScreen();
    if (mpDrawManager == this) mpDrawManager = nullptr;
}

bool SBDrawManager::OpenScreen(uint16_t w, uint16_t h) {
    if (!prog) {
        prog = glCreateProgram();
        GLuint vs = compile(GL_VERTEX_SHADER, kVS), fs = compile(GL_FRAGMENT_SHADER, kFS);
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glBindAttribLocation(prog, 0, "a");
        glLinkProgram(prog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        uDst = glGetUniformLocation(prog, "uDst");
        uSrc = glGetUniformLocation(prog, "uSrc");
        uTarget = glGetUniformLocation(prog, "uTarget");
        uColorKey = glGetUniformLocation(prog, "colorkey_rgb");
        uAlphaKey = glGetUniformLocation(prog, "alphakey_rgb");
        uConst = glGetUniformLocation(prog, "constant_color");
        uAlphaConst = glGetUniformLocation(prog, "alpha_constants");
        uTexRGB = glGetUniformLocation(prog, "TEXTURE_RGB");
        uTexAlpha = glGetUniformLocation(prog, "TEXTURE_ALPHA");
        uUseColorKey = glGetUniformLocation(prog, "useColorKey");
        uUseAlpha = glGetUniformLocation(prog, "useAlpha");
        uAlphaRed = glGetUniformLocation(prog, "alphaRed");
        static const float quad[] = {0, 0, 1, 0, 0, 1, 1, 1};
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindVertexArray(0);
    }
    ResizeScreen(w, h);
    return true;
}

void SBDrawManager::ResizeScreen(uint16_t w, uint16_t h) {
    Surface& s = surfaces[1];
    if (s.w == w && s.h == h && s.tex) return;
    if (s.fbo) glDeleteFramebuffers(1, &s.fbo);
    if (s.tex) glDeleteTextures(1, &s.tex);
    s = Surface();
    s.w = w;
    s.h = h;
    s.colorKey = pixelBits == 16 ? 0x7c0 : 0x3e0;
    s.px.assign((size_t)w * h, 0);
    s.cpuDirty = true;
    MakeTexture(s);
}

void SBDrawManager::CloseScreen() {
    for (auto& kv : surfaces) {
        if (kv.second.fbo) glDeleteFramebuffers(1, &kv.second.fbo);
        if (kv.second.tex) glDeleteTextures(1, &kv.second.tex);
    }
    surfaces.clear();
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (prog) glDeleteProgram(prog);
    prog = vao = vbo = 0;
}

SBDrawManager::Surface* SBDrawManager::Get(uint32_t id) {
    auto it = surfaces.find(id ? id : 1);
    return it == surfaces.end() ? nullptr : &it->second;
}
const SBDrawManager::Surface* SBDrawManager::Get(uint32_t id) const {
    auto it = surfaces.find(id ? id : 1);
    return it == surfaces.end() ? nullptr : &it->second;
}

void SBDrawManager::MakeTexture(Surface& s) {
    glGenTextures(1, &s.tex);
    glBindTexture(GL_TEXTURE_2D, s.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, s.w, s.h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, s.px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    s.cpuDirty = false;
}

void SBDrawManager::Upload(Surface& s) {
    if (!s.tex) MakeTexture(s);
    if (!s.cpuDirty) return;
    glBindTexture(GL_TEXTURE_2D, s.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, s.w, s.h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, s.px.data());
    s.cpuDirty = false;
}

uint32_t SBDrawManager::CreateSurface(uint16_t w, uint16_t h) {
    uint32_t id = nextId++;
    Surface& s = surfaces[id];
    s.w = w ? w : 1;
    s.h = h ? h : 1;
    s.colorKey = pixelBits == 16 ? 0x7c0 : 0x3e0;
    s.px.assign((size_t)s.w * s.h, 0);
    s.cpuDirty = true;  // texture made on first use
    return id;
}

void SBDrawManager::DeleteSurface(uint32_t id) {
    if (id <= 1) return;
    auto it = surfaces.find(id);
    if (it == surfaces.end()) return;
    if (it->second.fbo) glDeleteFramebuffers(1, &it->second.fbo);
    if (it->second.tex) glDeleteTextures(1, &it->second.tex);
    surfaces.erase(it);
}

uint16_t SBDrawManager::SurfaceWidth(uint32_t id) const {
    const Surface* s = Get(id);
    return s ? s->w : 0;
}
uint16_t SBDrawManager::SurfaceHeight(uint32_t id) const {
    const Surface* s = Get(id);
    return s ? s->h : 0;
}
uint16_t SBDrawManager::GetColorKeyingForSurface(uint32_t id) const {
    const Surface* s = Get(id);
    return s ? s->colorKey : 0;
}
void SBDrawManager::SetColorKeyingForSurface(uint32_t id, uint16_t key) {
    if (Surface* s = Get(id)) s->colorKey = key;
}

// SBDrawManager::GetSurfaceViewport (0x08148ad0)
bool SBDrawManager::GetSurfaceViewport(uint32_t id, SBDrawViewport& vp, uint32_t flags) {
    Surface* s = Get(id);
    if (!s) {
        SBError(false, "SBDrawManager.cpp", 0x5fc, "Trying to get a viewport on an inexisting surface");
        return false;
    }
    if ((flags & VIEWPORT_READ) && s->gpuNewer && s->fbo) {
        // the texture was drawn into: read it back (RGBA, the format every GL ES reads)
        std::vector<uint8_t> rgba((size_t)s->w * s->h * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
        glReadPixels(0, 0, s->w, s->h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        for (size_t i = 0; i < s->px.size(); ++i) {
            const uint8_t* p = &rgba[i * 4];
            s->px[i] = (uint16_t)((p[0] * 31 + 127) / 255 << 11 | (p[1] * 63 + 127) / 255 << 5 | (p[2] * 31 + 127) / 255);
        }
        s->gpuNewer = false;
    }
    vp.id = id ? id : 1;
    vp.width = s->w;
    vp.height = s->h;
    vp.pitch = (uint32_t)s->w * 2;
    vp.bits = pixelBits;
    vp.colorKey = s->colorKey;
    vp.data = (flags & (VIEWPORT_READ | VIEWPORT_WRITE)) ? s->px.data() : nullptr;
    if (flags & VIEWPORT_WRITE) {
        s->cpuDirty = true;
        s->gpuNewer = false;
    }
    return true;
}

void SBDrawManager::ReleaseSurfaceViewport(SBDrawViewport& vp) { vp.data = nullptr; }

void SBDrawManager::UploadPicture(uint32_t id, const uint16_t* px, uint16_t w, uint16_t h) {
    Surface* s = Get(id);
    if (!s) return;
    for (uint16_t y = 0; y < h && y < s->h; ++y)
        memcpy(&s->px[(size_t)y * s->w], px + (size_t)y * w, (size_t)(w < s->w ? w : s->w) * 2);
    s->cpuDirty = true;
    s->gpuNewer = false;
}

static float c5(uint16_t c) { return (float)(c >> 11) / 31.0f; }
static float c6(uint16_t c) { return (float)(c >> 5 & 0x3f) / 63.0f; }
static float c5b(uint16_t c) { return (float)(c & 0x1f) / 31.0f; }

// SBDrawManager::BlitGL (0x08147a10). Rects are x0, y0, x1, y1 in pixels.
void SBDrawManager::BlitGL(uint32_t src, uint32_t alphaSrc, const int* srcRect, uint32_t dst, const int* dstRect,
                           uint32_t flags, uint16_t colorKey, uint16_t alphaKey, float alpha, float alphaZ) {
    Surface* s = Get(src);
    Surface* d = Get(dst);
    if (!s || !d) return;
    Upload(*s);
    Upload(*d);
    if (!d->fbo) {
        glGenFramebuffers(1, &d->fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, d->tex, 0);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
    }
    glViewport(0, 0, d->w, d->h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(prog);
    glBindVertexArray(vao);

    glUniform1i(uUseColorKey, (flags & BLIT_COLORKEY) ? 1 : 0);
    glUniform4f(uColorKey, c5(colorKey), c6(colorKey), c5b(colorKey), alpha);
    if (flags & BLIT_ALPHAKEY) glUniform4f(uAlphaKey, c5(alphaKey), c6(alphaKey), c5b(alphaKey), alpha);
    else glUniform4f(uAlphaKey, 2, 2, 2, 2);
    bool inv = (flags & BLIT_ALPHAINVERT) != 0;
    glUniform4f(uAlphaConst, inv ? -1.0f : 1.0f, inv ? 1.0f : 0.0f, alphaZ, 0);
    float ca = (flags & BLIT_ALPHAKEY) ? 1.0f : alpha;
    if (flags & BLIT_BLACK) glUniform4f(uConst, 0, 0, 0, ca);
    else glUniform4f(uConst, 1, 1, 1, ca);
    Surface* a = alphaSrc ? Get(alphaSrc) : nullptr;
    glUniform1i(uUseAlpha, a && (flags & (BLIT_ALPHATEX | BLIT_ALPHARED)) ? 1 : 0);
    glUniform1i(uAlphaRed, (flags & BLIT_ALPHARED) ? 1 : 0);
    glActiveTexture(GL_TEXTURE1);
    if (a) {
        Upload(*a);
        glBindTexture(GL_TEXTURE_2D, a->tex);
    }
    glUniform1i(uTexAlpha, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s->tex);
    glUniform1i(uTexRGB, 0);

    int sr[4] = {0, 0, s->w, s->h};
    if (srcRect) memcpy(sr, srcRect, sizeof sr);
    int dr[4] = {0, 0, d->w, d->h};
    if (dstRect) memcpy(dr, dstRect, sizeof dr);
    glUniform2f(uTarget, (float)d->w, (float)d->h);
    glUniform4f(uDst, (float)dr[0], (float)dr[1], (float)(dr[2] - dr[0]), (float)(dr[3] - dr[1]));
    glUniform4f(uSrc, (float)sr[0] / s->w, (float)sr[1] / s->h, (float)(sr[2] - sr[0]) / s->w,
                (float)(sr[3] - sr[1]) / s->h);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    d->gpuNewer = true;
}

static bool boxRect(const SBGeoBoundingBox2D* b, int* r) {
    r[0] = (int)b->p0.x;
    r[1] = (int)b->p0.y;
    r[2] = (int)b->p1.x;
    r[3] = (int)b->p1.y;
    return r[0] != r[2] && r[1] != r[3];
}

// SBDrawManager::Blit (0x081493a0)
bool SBDrawManager::Blit(uint32_t src, const SBGeoBoundingBox2D* srcBox, uint32_t dst, const SBGeoBoundingBox2D* dstBox,
                         uint32_t flags) {
    Surface* s = Get(src);
    Surface* d = Get(dst);
    if (!s || !d) {
        SBError(false, "SBDrawManager.cpp", 0x86b, "Invalid surface for the blit");
        return false;
    }
    int sr[4], dr[4];
    const int* psr = nullptr;
    const int* pdr = nullptr;
    if (dstBox) {
        if (!boxRect(dstBox, dr)) return false;
        pdr = dr;
    }
    if (srcBox) {
        if (!boxRect(srcBox, sr)) return false;
        psr = sr;
    }
    BlitGL(src, 0, psr, dst, pdr, flags, s->colorKey, 0, 1.0f, 1.0f);
    return true;
}

// the source/destination sizes of the alpha blits, as in the original: the destination box gives
// the drawn size, the source box (if any) the picture part
static void alphaRects(const SBGeoBoundingBox2D* srcBox, const SBGeoBoundingBox2D* dstBox, int sw, int sh, int dw,
                       int dh, int* sr, int* dr) {
    if (dstBox) {
        dr[0] = (int)dstBox->p0.x;
        dr[1] = (int)dstBox->p0.y;
        dr[2] = dr[0] + (int)(dstBox->p1.x - dstBox->p0.x);
        dr[3] = dr[1] + (int)(dstBox->p1.y - dstBox->p0.y);
    } else {
        dr[0] = dr[1] = 0;
        dr[2] = dw;
        dr[3] = dh;
    }
    if (srcBox) {
        sr[0] = (int)srcBox->p0.x;
        sr[1] = (int)srcBox->p0.y;
        sr[2] = sr[0] + (int)(srcBox->p1.x - srcBox->p0.x);
        sr[3] = sr[1] + (int)(srcBox->p1.y - srcBox->p0.y);
    } else {
        sr[0] = sr[1] = 0;
        sr[2] = sw;
        sr[3] = sh;
    }
}

// SBDrawManager::BlitAlphaKeying (0x0814bab0): the alpha key colour becomes black at `percent`
bool SBDrawManager::BlitAlphaKeying(uint32_t src, const SBGeoBoundingBox2D* srcBox, uint32_t dst,
                                    const SBGeoBoundingBox2D* dstBox, uint16_t alphaKey, uint16_t percent,
                                    uint32_t flags) {
    Surface* s = Get(src);
    Surface* d = Get(dst);
    if (!s || !d) return false;
    int sr[4], dr[4];
    alphaRects(srcBox, dstBox, s->w, s->h, d->w, d->h, sr, dr);
    if (dr[2] == dr[0] || dr[3] == dr[1]) return false;
    int k = (int)((100 - (unsigned)percent) * 0x100) / 100;
    float alpha = (float)((0x100u - (unsigned)k) & 0xffff) * 0.00390625f;
    BlitGL(src, 0, sr, dst, dr, flags | BLIT_ALPHAKEY, s->colorKey, alphaKey, alpha, 1.0f);
    return true;
}

bool SBDrawManager::BlitAlpha(uint32_t src, const SBGeoBoundingBox2D* srcBox, uint32_t dst,
                              const SBGeoBoundingBox2D* dstBox, uint32_t alphaSurface, uint32_t flags) {
    Surface* s = Get(src);
    Surface* d = Get(dst);
    if (!s || !d) return false;
    if ((flags & 0x4700) == 0) flags |= 0x400;
    int sr[4], dr[4];
    alphaRects(srcBox, dstBox, s->w, s->h, d->w, d->h, sr, dr);
    if (dr[2] == dr[0] || dr[3] == dr[1]) return false;
    BlitGL(src, alphaSurface, sr, dst, dr, flags, (flags & BLIT_COLORKEY) ? s->colorKey : 0, 0, 1.0f, 1.0f);
    return true;
}

bool SBDrawManager::BlitAlphaConstant(uint32_t src, const SBGeoBoundingBox2D* srcBox, uint32_t dst,
                                      const SBGeoBoundingBox2D* dstBox, uint16_t percent, uint32_t flags) {
    Surface* s = Get(src);
    Surface* d = Get(dst);
    if (!s || !d) return false;
    int sr[4], dr[4];
    alphaRects(srcBox, dstBox, s->w, s->h, d->w, d->h, sr, dr);
    if (dr[2] == dr[0] || dr[3] == dr[1]) return false;
    BlitGL(src, 0, sr, dst, dr, flags, s->colorKey, 0, (float)percent / 100.0f, 1.0f);
    return true;
}

void SBDrawManager::Fill(uint32_t dst, const SBGeoBoundingBox2D* box, uint16_t color, uint32_t flags) {
    Surface* d = Get(dst);
    if (!d) return;
    // a one-pixel surface of that colour, stretched
    static uint32_t fillId = 0;
    if (!fillId || !Get(fillId)) fillId = CreateSurface(1, 1);
    Surface* f = Get(fillId);
    if (f->px[0] != color || !f->tex) {
        f->px[0] = color;
        f->cpuDirty = true;
    }
    int dr[4] = {0, 0, d->w, d->h};
    if (box) boxRect(box, dr);
    BlitGL(fillId, 0, nullptr, dst, dr, 0, 0, 0, 1.0f, 1.0f);
}

unsigned SBDrawManager::ScreenTexture() const {
    const Surface* s = Get(1);
    return s ? s->tex : 0;
}

void SBDrawManager::Finish() {
    if (Surface* s = Get(1)) Upload(*s);
}
