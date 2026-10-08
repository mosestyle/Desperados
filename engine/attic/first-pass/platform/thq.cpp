// See thq.h.
#include "thq.h"

#include <SDL.h>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sys/stat.h>
#include <unistd.h>

int thqsdlfw_sdl_window_width = 1, thqsdlfw_sdl_window_height = 1;
int target_width = 1, target_height = 1;
GLuint thqsdlfw_last_framebuffer = 0;
bool thqsdlfw_last_framebuffer_invalid = false;
bool thqsdlfw_quit = false;
std::string thqsdlfw_pref_path;

static GLuint gVao, gQuad, gDynamic;
static std::map<string_hash_t, thqsdlfw_tech_t> gTechs;
static GLint gDefaultFramebuffer;

// ------------------------------------------------------------------------------------- shaders
// The game's shaders (blit_vertex, desperados_fragment, blit_fragment, fill_fragment,
// video_fragment) written for GLSL ES 3.00.
static const char* kVertex =
    "in vec4 sg3d_position0;\n"
    "out vec2 var_uv;\n"
    "uniform vec4 sg3d_transform;\n"
    "uniform vec4 sg3d_uv;\n"
    "void main() {\n"
    "  gl_Position = sg3d_position0;\n"
    "  gl_Position.xy = gl_Position.xy * sg3d_transform.xy + sg3d_transform.zw;\n"
    "  var_uv = sg3d_position0.xy * sg3d_uv.xy + sg3d_uv.zw;\n"
    "}\n";
static const char* kDesperados =
    "in vec2 var_uv;\n"
    "out vec4 var_out;\n"
    "uniform sampler2D TEXTURE_RGB;\n"
    "uniform sampler2D TEXTURE_ALPHA;\n"
    "uniform vec4 colorkey_rgb;\n"
    "uniform vec4 alphakey_rgb;\n"
    "uniform vec4 constant_color;\n"
    "uniform vec4 alpha_constants;\n"
    "void main() {\n"
    "  var_out = texture(TEXTURE_RGB, var_uv);\n"
    "#ifdef COLORKEY\n"
    "  { vec3 diff = var_out.rgb - colorkey_rgb.rgb; if (dot(diff, diff) < 0.00005) var_out.a = 0.0; }\n"
    "#endif\n"
    "  { vec3 diff = var_out.rgb - alphakey_rgb.rgb; if (dot(diff, diff) < 0.00005) var_out = vec4(0.0, 0.0, 0.0, alphakey_rgb.a); }\n"
    "#ifdef ALPHA\n"
    "#ifdef ALPHARED\n"
    "  float alpha = texture(TEXTURE_ALPHA, var_uv).r;\n"
    "#else\n"
    "  float alpha = texture(TEXTURE_ALPHA, var_uv).b;\n"
    "#endif\n"
    "  var_out.a *= min(alpha, alpha_constants.z) * alpha_constants.x + alpha_constants.y;\n"
    "#endif\n"
    "  var_out *= constant_color;\n"
    "}\n";
static const char* kBlit =
    "in vec2 var_uv;\n"
    "out vec4 var_out;\n"
    "uniform sampler2D TEXTURE_RGB;\n"
    "uniform vec4 constant_color;\n"
    "void main() { var_out = texture(TEXTURE_RGB, var_uv) * constant_color; }\n";
static const char* kFill =
    "out vec4 var_out;\n"
    "uniform vec4 fillcolor;\n"
    "void main() { var_out = fillcolor; }\n";
static const char* kVideo =
    "in vec2 var_uv;\n"
    "out vec4 var_out;\n"
    "uniform sampler2D TEXTURE_VIDEO_Y;\n"
    "uniform sampler2D TEXTURE_VIDEO_U;\n"
    "uniform sampler2D TEXTURE_VIDEO_V;\n"
    "const mat3 yuv2rgb = mat3(1.0, 0.0, 1.596, 1.0, -0.391, -0.813, 1.0, 2.018, 0.0);\n"
    "void main() {\n"
    "  float y = texture(TEXTURE_VIDEO_Y, var_uv).x;\n"
    "  float u = texture(TEXTURE_VIDEO_U, var_uv).x;\n"
    "  float v = texture(TEXTURE_VIDEO_V, var_uv).x;\n"
    "  vec3 rgb = vec3(1.1643 * (y - 0.0625), u - 0.5, v - 0.5) * yuv2rgb;\n"
    "  var_out = vec4(rgb, 1.0);\n"
    "}\n";

static GLuint compileShader(GLenum type, const char* src, const char* defines) {
    std::string text = "#version 300 es\nprecision highp float;\n";
    text += defines;
    text += src;
    GLuint s = glCreateShader(type);
    const char* p = text.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        SBLog("shader did not compile: %s", log);
    }
    return s;
}

static GLuint createProgram(const char* fragment, const char* defines) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertex, defines);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragment, defines);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "sg3d_position0");
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        SBLog("shader program did not link: %s", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

static void addTech(string_hash_t h, const char* fragment, const char* defines, bool blend) {
    thqsdlfw_tech_t& t = gTechs[h];
    t.program = createProgram(fragment, defines);
    t.blend = blend;
}

bool thqsdlfw_gl_init() {
    glGenVertexArrays(1, &gVao);
    glBindVertexArray(gVao);
    glEnableVertexAttribArray(0);
    static const float quad[] = {0, 0, 1, 0, 0, 1, 1, 1};
    glGenBuffers(1, &gQuad);
    glBindBuffer(GL_ARRAY_BUFFER, gQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glGenBuffers(1, &gDynamic);
    glBindBuffer(GL_ARRAY_BUFFER, gDynamic);
    glBufferData(GL_ARRAY_BUFFER, 8 * 4096, nullptr, GL_DYNAMIC_DRAW);

    addTech(TECH_PLAIN, kDesperados, "", true);
    addTech(TECH_COLORKEY, kDesperados, "#define COLORKEY 1\n", true);
    addTech(TECH_ALPHA_RED, kDesperados, "#define ALPHA 1\n#define ALPHARED 1\n", true);
    addTech(TECH_ALPHA_RED_CK, kDesperados, "#define ALPHA 1\n#define ALPHARED 1\n#define COLORKEY 1\n", true);
    addTech(TECH_ALPHA_BLUE, kDesperados, "#define ALPHA 1\n#define ALPHABLUE 1\n", true);
    addTech(TECH_ALPHA_BLUE_CK, kDesperados, "#define ALPHA 1\n#define ALPHABLUE 1\n#define COLORKEY 1\n", true);
    addTech(TECH_FILL, kFill, "", false);
    addTech(TECH_BLIT, kBlit, "", false);
    addTech(TECH_BLIT_BLEND, kBlit, "", true);
    addTech(TECH_FILL_BLEND, kFill, "", true);
    addTech(TECH_VIDEO, kVideo, "", false);

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &gDefaultFramebuffer);
    thqsdlfw_set_window_target();
    return true;
}

void thqsdlfw_gl_shutdown() {
    for (auto& t : gTechs) glDeleteProgram(t.second.program);
    gTechs.clear();
    glDeleteBuffers(1, &gQuad);
    glDeleteBuffers(1, &gDynamic);
    glDeleteVertexArrays(1, &gVao);
}

thqsdlfw_tech_t* thqsdlfw_set_tech(string_hash_t h) {
    thqsdlfw_tech_t& t = gTechs[h];
    if (t.blend) {
        glEnable(GL_BLEND);
        glBlendFunc(t.blendSrc, t.blendDst);
    } else {
        glDisable(GL_BLEND);
    }
    glUseProgram(t.program);
    return &t;
}

void thqsdlfw_set_texture(thqsdlfw_tech_t* t, const char* name, GLuint texture) {
    int unit = -1;
    for (int i = 0; i < t->nsamplers; ++i)
        if (t->samplers[i].name == name) unit = t->samplers[i].unit;
    if (unit < 0) {
        GLint loc = glGetUniformLocation(t->program, name);
        unit = -1;
        if (loc >= 0) {
            // the original counted units from 0 and stored "count" (so the first sampler gets unit 0
            // but is recorded as 1)
            glUniform1i(loc, t->nextUnit);
            unit = ++t->nextUnit;
        }
        if (t->nsamplers < 8) t->samplers[t->nsamplers++] = {name, unit};
    }
    if (unit > 0 && unit <= 4) {
        t->textures[unit] = texture;
        t->dirty |= 1u << (unit - 1);
    }
}

void thqsdlfw_set_constant(thqsdlfw_tech_t* t, const char* name, const thqsdlfw_float4& v) {
    GLint loc = glGetUniformLocation(t->program, name);
    if (loc >= 0) glUniform4fv(loc, 1, &v.x);
}

static void bindTextures(thqsdlfw_tech_t* t) {
    for (int i = 0; t->dirty; ++i, t->dirty >>= 1) {
        if (t->dirty & 1) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, t->textures[i + 1]);
        }
    }
}

void thqsdlfw_blit(thqsdlfw_tech_t* t, float x, float y, float w, float h, float u, float v, float du, float dv) {
    float tw = (float)target_width, th = (float)target_height;
    float transform[4] = {(w + w) / tw, (h + h) / th, (x + x) / tw - 1.0f, (y + y) / th - 1.0f};
    GLint loc = glGetUniformLocation(t->program, "sg3d_transform");
    if (loc >= 0) glUniform4fv(loc, 1, transform);
    float uv[4] = {du, dv, u, v};
    loc = glGetUniformLocation(t->program, "sg3d_uv");
    if (loc >= 0) glUniform4fv(loc, 1, uv);
    bindTextures(t);
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gQuad);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void thqsdlfw_draw(thqsdlfw_tech_t* t, GLenum mode, uint32_t n, const float* xy) {
    float transform[4] = {2.0f / (float)target_width, 2.0f / (float)target_height, -1.0f, -1.0f};
    GLint loc = glGetUniformLocation(t->program, "sg3d_transform");
    if (loc >= 0) glUniform4fv(loc, 1, transform);
    static const float uv[4] = {1, 1, 0, 0};
    loc = glGetUniformLocation(t->program, "sg3d_uv");
    if (loc >= 0) glUniform4fv(loc, 1, uv);
    bindTextures(t);
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gDynamic);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
    while (n) {
        uint32_t k = n > 4096 ? 4096 : n;
        glBufferSubData(GL_ARRAY_BUFFER, 0, k * 8, xy);
        glDrawArrays(mode, 0, k);
        if (k == n) break;
        // only point and line lists can be split safely; strips are never this long
        n -= k;
        xy += k * 2;
    }
}

void thqsdlfw_create_render_target(thqsdlfw_render_target_t* rt, uint32_t w, uint32_t h, GLuint texture) {
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    if (!texture) {
        glGenTextures(1, &texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, nullptr);
    }
    GLuint fb = 0;
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous);
    rt->framebuffer = fb;
    rt->texture = texture;
    rt->width = w;
    rt->height = h;
}

void thqsdlfw_destroy_render_target(thqsdlfw_render_target_t* rt) {
    if (rt->framebuffer) glDeleteFramebuffers(1, &rt->framebuffer);
    if (rt->texture) glDeleteTextures(1, &rt->texture);
    *rt = thqsdlfw_render_target_t();
}

void thqsdlfw_set_render_target(const thqsdlfw_render_target_t& rt) {
    thqsdlfw_last_framebuffer_invalid = false;
    thqsdlfw_last_framebuffer = rt.framebuffer;
    if (rt.framebuffer == 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)gDefaultFramebuffer);
        target_width = thqsdlfw_sdl_window_width;
        target_height = thqsdlfw_sdl_window_height;
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer);
        target_width = (int)rt.width;
        target_height = (int)rt.height;
    }
    glViewport(0, 0, target_width, target_height);
}

void thqsdlfw_set_window_target() {
    thqsdlfw_render_target_t window;
    thqsdlfw_set_render_target(window);
}

// ------------------------------------------------------------------------------------- files
struct RFILE_handle_t {
    SDL_RWops* rw;
};

static std::string gRoot = ".";

void RFILE_set_root(const std::string& gameFolder) { gRoot = gameFolder; }
const std::string& RFILE_root() { return gRoot; }

// The original turned '\' into '/' and lowercased everything (unless the path held "THQNordic").
static std::string fixPath(const char* p) {
    std::string s;
    for (const char* c = p; *c; ++c) s += *c == '\\' ? '/' : *c;
    if (s.find("THQNordic") == std::string::npos)
        for (char& c : s) c = (char)tolower((unsigned char)c);
    if (!s.empty() && s[0] == '/') return s;
    if (s.size() > 1 && s[1] == ':') return s;  // never on our platforms, kept for safety
    return gRoot + "/" + s;
}

RFILE_handle_t* RFILE_open_from_file(const char* path, const char* mode) {
    std::string full;
    // absolute paths (pref path, our own files) are kept as given
    if (path[0] == '/') full = path;
    else full = fixPath(path);
    SDL_RWops* rw = SDL_RWFromFile(full.c_str(), mode);
    if (!rw) return nullptr;
    RFILE_handle_t* f = new RFILE_handle_t;
    f->rw = rw;
    return f;
}

void RFILE_close(RFILE_handle_t* f) {
    if (!f) return;
    SDL_RWclose(f->rw);
    delete f;
}

size_t RFILE_read(void* dst, size_t size, size_t count, RFILE_handle_t* f) {
    if (!f || !size) return 0;
    return SDL_RWread(f->rw, dst, size, count);
}

size_t RFILE_write(const void* src, size_t size, size_t count, RFILE_handle_t* f) {
    if (!f || !size) return 0;
    return SDL_RWwrite(f->rw, src, size, count);
}

int RFILE_seek(RFILE_handle_t* f, long offset, int whence) {
    if (!f) return -1;
    return SDL_RWseek(f->rw, offset, whence) < 0 ? -1 : 0;
}

long RFILE_tell(RFILE_handle_t* f) { return f ? (long)SDL_RWtell(f->rw) : -1; }

long RFILE_size(RFILE_handle_t* f) { return f ? (long)SDL_RWsize(f->rw) : -1; }

bool RFILE_eof(RFILE_handle_t* f) { return !f || SDL_RWtell(f->rw) >= SDL_RWsize(f->rw); }

int RFILE_delete(const char* path) {
    std::string full = path[0] == '/' ? std::string(path) : fixPath(path);
    return remove(full.c_str());
}

// ------------------------------------------------------------------------------------- misc
uint32_t GetTickCountSUBST() { return SDL_GetTicks(); }

void SBLog(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, ap);
    va_end(ap);
}

void SBError(bool fatal, const char* file, int line, const char* message) {
    SBLog("%s: %s (%s:%d)", fatal ? "error" : "warning", message, file ? file : "?", line);
    if (fatal) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Desperados", message, nullptr);
        exit(1);
    }
}
