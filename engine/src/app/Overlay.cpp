#include "app/Overlay.h"

#include <cstdio>
#include <cstring>

#include "sb/SBFile.h"

static const char* kVS =
    "#version 300 es\n"
    "in vec2 a;\n"
    "uniform vec4 uRect; uniform vec4 uUV; uniform vec2 uScreen;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "  vec2 p = uRect.xy + a * uRect.zw;\n"
    "  gl_Position = vec4(p.x / uScreen.x * 2.0 - 1.0, 1.0 - p.y / uScreen.y * 2.0, 0.0, 1.0);\n"
    "  vUV = uUV.xy + a * uUV.zw;\n"
    "}\n";
static const char* kFS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 vUV; uniform sampler2D uTex; uniform vec4 uColor; out vec4 o;\n"
    "void main() { o = texture(uTex, vUV) * uColor; }\n";

static GLuint compile(GLenum t, const char* src) {
    GLuint s = glCreateShader(t);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        SBLog("overlay shader: %s", log);
    }
    return s;
}

bool Overlay::Init() {
    prog = glCreateProgram();
    GLuint vs = compile(GL_VERTEX_SHADER, kVS), fs = compile(GL_FRAGMENT_SHADER, kFS);
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "a");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    uRect = glGetUniformLocation(prog, "uRect");
    uUV = glGetUniformLocation(prog, "uUV");
    uColor = glGetUniformLocation(prog, "uColor");
    uScreen = glGetUniformLocation(prog, "uScreen");
    uTex = glGetUniformLocation(prog, "uTex");
    static const float quad[] = {0, 0, 1, 0, 0, 1, 1, 1};
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);
    const unsigned char px[4] = {255, 255, 255, 255};
    glGenTextures(1, &white);
    glBindTexture(GL_TEXTURE_2D, white);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return true;
}

void Overlay::Shutdown() {
    if (white) glDeleteTextures(1, &white);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (prog) glDeleteProgram(prog);
    white = vbo = vao = prog = 0;
}

void Overlay::Begin(int w, int h) {
    scrW = (float)w;
    scrH = (float)h;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(prog);
    glUniform2f(uScreen, scrW, scrH);
    glUniform1i(uTex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);
}

void Overlay::Quad(GLuint tex, float x, float y, float w, float h, const float uv[4], const float rgba[4]) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform4f(uRect, x, y, w, h);
    glUniform4f(uUV, uv[0], uv[1], uv[2], uv[3]);
    glUniform4f(uColor, rgba[0], rgba[1], rgba[2], rgba[3]);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Overlay::Picture(GLuint tex, int texW, int texH, float x, float y, float w, float h) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    const float uv[4] = {0, 0, 1, 1};
    const float c[4] = {1, 1, 1, 1};
    glDisable(GL_BLEND);
    Quad(tex, x, y, w, h, uv, c);
    glEnable(GL_BLEND);
    // the engine draws with nearest sampling
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void Overlay::Rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    const float uv[4] = {0, 0, 1, 1};
    const float c[4] = {r, g, b, a};
    Quad(white, x, y, w, h, uv, c);
}

// 5x7 font: digits, capitals and a few signs, one byte per row (5 low bits)
static const unsigned char* glyph(char ch) {
    static const unsigned char digits[10][7] = {
        {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},  {14, 17, 1, 2, 4, 8, 31},  {31, 2, 4, 2, 1, 17, 14},
        {2, 6, 10, 18, 31, 2, 2},     {31, 16, 30, 1, 1, 17, 14}, {6, 8, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
        {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 2, 12}};
    static const unsigned char letters[26][7] = {
        {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14}, {28, 18, 17, 17, 17, 18, 28},
        {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16}, {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
        {14, 4, 4, 4, 4, 4, 14},      {7, 2, 2, 2, 2, 18, 12},      {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
        {17, 27, 21, 21, 17, 17, 17}, {17, 17, 25, 21, 19, 17, 17}, {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
        {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17}, {15, 16, 16, 14, 1, 1, 30},  {31, 4, 4, 4, 4, 4, 4},
        {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
        {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31}};
    static const unsigned char slash[7] = {1, 1, 2, 4, 8, 16, 16};
    static const unsigned char colon[7] = {0, 12, 12, 0, 12, 12, 0};
    static const unsigned char dot[7] = {0, 0, 0, 0, 0, 12, 12};
    static const unsigned char dash[7] = {0, 0, 0, 31, 0, 0, 0};
    if (ch >= '0' && ch <= '9') return digits[ch - '0'];
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z') return letters[ch - 'A'];
    if (ch == '/') return slash;
    if (ch == ':') return colon;
    if (ch == '.') return dot;
    if (ch == '-' || ch == '_') return dash;
    return nullptr;
}

void Overlay::Text(const char* s, float x, float y, float size, bool centred) {
    float px = size / 7.0f;
    float w = (float)strlen(s) * 6 * px;
    if (centred) {
        x -= w * 0.5f;
        y -= size * 0.5f;
    }
    Rect(x - px * 2, y - px * 2, w + px * 3, size + px * 4, 0, 0, 0, 0.55f);
    for (const char* c = s; *c; ++c, x += 6 * px) {
        const unsigned char* g = glyph(*c);
        if (!g) continue;
        for (int r = 0; r < 7; ++r)
            for (int b = 0; b < 5; ++b)
                if (g[r] >> (4 - b) & 1) Rect(x + b * px, y + r * px, px, px, 1, 0.92f, 0.75f, 1);
    }
}

void Overlay::LevelBar(size_t level, size_t count, int w, int h) {
    float size = h * 0.045f;
    char buf[32];
    snprintf(buf, sizeof buf, "MISSION %zu/%zu", level, count);
    float cx = w * 0.5f, cy = size * 1.2f;
    Text(buf, cx, cy, size, true);
    // arrows
    float bw = size * 2.2f;
    for (int side = -1; side <= 1; side += 2) {
        float bx = cx + side * (size * 5.5f + bw * 0.5f) - bw * 0.5f, by = cy - bw * 0.5f;
        Rect(bx, by, bw, bw, 0, 0, 0, 0.45f);
        for (int i = 0; i < 9; ++i) {
            float t = (float)i / 8.0f;
            float len = bw * 0.5f * (side > 0 ? 1 - t : t);
            float ax = side > 0 ? bx + bw * 0.3f + t * bw * 0.4f : bx + bw * 0.3f + t * bw * 0.4f;
            Rect(ax, cy - len * 0.5f, bw * 0.05f, len, 1, 0.92f, 0.75f, 0.9f);
        }
    }
}

int Overlay::HitButton(float x, float y, int w, int h) const {
    float size = h * 0.045f;
    float cx = w * 0.5f, cy = size * 1.2f, bw = size * 2.2f;
    if (y > cy + bw) return 0;
    if (x < cx - size * 5.5f && x > cx - size * 5.5f - bw * 1.6f) return -1;
    if (x > cx + size * 5.5f && x < cx + size * 5.5f + bw * 1.6f) return 1;
    return 0;
}
