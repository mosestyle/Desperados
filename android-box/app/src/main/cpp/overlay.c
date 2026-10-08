// See overlay.h.
#include "overlay.h"

#include <SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static GLuint prog, vao, vbo, atlas;
static GLint uDst, uScreen, uUV, uColor, uUseTex, uCircle, uRing, uTex;
static int atlasW, atlasH;
static float scrW = 1, scrH = 1;

static const char* kVS =
    "#version 300 es\n"
    "in vec2 a;\n"
    "uniform vec4 uDst; uniform vec2 uScreen; uniform vec4 uUV;\n"
    "out vec2 vUV; out vec2 vLocal;\n"
    "void main() {\n"
    "  vec2 p = uDst.xy + a * uDst.zw;\n"
    "  gl_Position = vec4(p / uScreen * 2.0 - 1.0, 0.0, 1.0);\n"
    "  vUV = mix(uUV.xy, uUV.zw, a);\n"
    "  vLocal = a * 2.0 - 1.0;\n"
    "}\n";
static const char* kFS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 vUV; in vec2 vLocal;\n"
    "uniform sampler2D uTex; uniform vec4 uColor; uniform float uUseTex, uCircle, uRing;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "  vec4 c = uColor;\n"
    "  if (uUseTex > 0.5) c *= texture(uTex, vUV);\n"
    "  if (uCircle > 0.0) {\n"
    "    float r = length(vLocal);\n"
    "    float edge = clamp((1.0 - r) * uCircle, 0.0, 1.0);\n"  // uCircle = radius in pixels: 1 px soft edge
    "    if (uRing > 0.0) edge *= clamp((r - (1.0 - uRing)) * uCircle, 0.0, 1.0);\n"
    "    c.a *= edge;\n"
    "  }\n"
    "  if (c.a <= 0.0) discard;\n"
    "  color = c;\n"
    "}\n";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        SDL_Log("overlay shader: %s", log);
    }
    return s;
}

int overlay_init(void) {
    if (prog) return 1;
    prog = glCreateProgram();
    GLuint vs = compile(GL_VERTEX_SHADER, kVS), fs = compile(GL_FRAGMENT_SHADER, kFS);
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "a");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    uDst = glGetUniformLocation(prog, "uDst");
    uScreen = glGetUniformLocation(prog, "uScreen");
    uUV = glGetUniformLocation(prog, "uUV");
    uColor = glGetUniformLocation(prog, "uColor");
    uUseTex = glGetUniformLocation(prog, "uUseTex");
    uCircle = glGetUniformLocation(prog, "uCircle");
    uRing = glGetUniformLocation(prog, "uRing");
    uTex = glGetUniformLocation(prog, "uTex");

    static const float quad[] = {0, 0, 1, 0, 0, 1, 1, 1};
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindVertexArray(0);

    SDL_RWops* rw = SDL_RWFromFile("ui_atlas.rgba", "rb");
    if (rw) {
        uint32_t hdr[2];
        if (SDL_RWread(rw, hdr, 4, 2) == 2 && hdr[0] && hdr[1] && hdr[0] <= 8192 && hdr[1] <= 8192) {
            size_t n = (size_t)hdr[0] * hdr[1] * 4;
            unsigned char* px = malloc(n);
            if (px && SDL_RWread(rw, px, 1, n) == n) {
                glGenTextures(1, &atlas);
                glBindTexture(GL_TEXTURE_2D, atlas);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)hdr[0], (GLsizei)hdr[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
                glGenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glBindTexture(GL_TEXTURE_2D, 0);
                atlasW = (int)hdr[0];
                atlasH = (int)hdr[1];
            }
            free(px);
        }
        SDL_RWclose(rw);
    }
    if (!atlas) SDL_Log("overlay: no icon atlas");
    return 1;
}

void overlay_shutdown(void) {
    if (atlas) glDeleteTextures(1, &atlas);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (prog) glDeleteProgram(prog);
    atlas = vbo = vao = prog = 0;
}

void overlay_begin(int screenW, int screenH) {
    scrW = (float)screenW;
    scrH = (float)screenH;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenW, screenH);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(prog);
    glUniform2f(uScreen, scrW, scrH);
    glUniform1i(uTex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);
}

static void quad(float x, float y, float w, float h, const float uv[4], const float rgba[4], GLuint tex, float circle, float ring) {
    glUniform4f(uDst, x, y, w, h);
    if (uv) glUniform4f(uUV, uv[0], uv[1], uv[2], uv[3]);
    else glUniform4f(uUV, 0, 0, 1, 1);
    glUniform4f(uColor, rgba[0], rgba[1], rgba[2], rgba[3]);
    glUniform1f(uUseTex, tex ? 1.0f : 0.0f);
    glUniform1f(uCircle, circle);
    glUniform1f(uRing, ring);
    if (tex) glBindTexture(GL_TEXTURE_2D, tex);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void overlay_disc(float cx, float cy, float radius, const float rgba[4], float ring) {
    quad(cx - radius, cy - radius, 2 * radius, 2 * radius, NULL, rgba, 0, radius, ring);
}

void overlay_icon(int icon, float cx, float cy, float size, const float rgba[4]) {
    if (!atlas || icon < 0 || icon >= ICON_COUNT) return;
    float cell = (float)atlasH / atlasW;  // icons are square cells along the atlas
    float uv[4] = {icon * cell, 1, (icon + 1) * cell, 0};  // bottom of the quad = bottom row of the icon
    quad(cx - size / 2, cy - size / 2, size, size, uv, rgba, atlas, 0, 0);
}

void overlay_texture_disc(GLuint tex, const float uv[4], float cx, float cy, float radius, float alpha) {
    const float white[4] = {1, 1, 1, alpha};
    quad(cx - radius, cy - radius, 2 * radius, 2 * radius, uv, white, tex, radius, 0);
}
