// libGLdesp.so: the desktop OpenGL the original game asks for, on top of the phone's OpenGL ES 3.
//
// The 2018 Linux build draws with a tiny, modern subset of OpenGL (textures, vertex buffers,
// GLSL 1.40 shaders), almost all of which exists unchanged in GLES 3. Box64 loads this library
// in place of libGL.so.1 (BOX64_LIBGL) and looks every function up by name in it: the GLES
// functions come straight from libGLESv3 (a dependency of this library); the few desktop-only
// bits are defined here:
//   - glGetString reports a desktop GL 3.3 / GLSL 1.40 context
//   - shaders: "#version 140" becomes "#version 300 es" with default precisions
//   - desktop-only calls (glBindFragDataLocation, glPolygonMode, glDrawBuffer...) are no-ops
//   - glXGetProcAddress & a few glX queries the game's GLEW makes
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#define EXPORT __attribute__((visibility("default")))
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "DesperadosGL", __VA_ARGS__)

static void* gles(void) {
    static void* h;
    if (!h) h = dlopen("libGLESv3.so", RTLD_NOW | RTLD_LOCAL);
    return h;
}
static void* real(const char* name) {
    void* p = dlsym(gles(), name);
    if (!p) p = (void*)eglGetProcAddress(name);
    return p;
}

// ---------------------------------------------------------------------------------------------
// strings

#define GL_SHADING_LANGUAGE_VERSION_ 0x8B8C
EXPORT const GLubyte* glGetString(GLenum name) {
    static const GLubyte* (*fn)(GLenum);
    if (!fn) fn = real("glGetString");
    switch (name) {
    case GL_VERSION: return (const GLubyte*)"3.3.0 (OpenGL ES 3 via Desperados shim)";
    case GL_SHADING_LANGUAGE_VERSION_: return (const GLubyte*)"1.40";
    default: return fn(name);
    }
}

EXPORT void glGetIntegerv(GLenum pname, GLint* data) {
    static void (*fn)(GLenum, GLint*);
    if (!fn) fn = real("glGetIntegerv");
    if (pname == GL_MAJOR_VERSION) { *data = 3; return; }
    if (pname == GL_MINOR_VERSION) { *data = 3; return; }
    fn(pname, data);
}

// ---------------------------------------------------------------------------------------------
// shaders: GLSL 1.40 -> GLSL ES 3.00

static char* replaceAll(char* s, const char* from, const char* to) {
    size_t fl = strlen(from), tl = strlen(to), n = 0;
    for (char* p = strstr(s, from); p; p = strstr(p + fl, from)) ++n;
    if (!n) return s;
    char* out = malloc(strlen(s) + n * (tl > fl ? tl - fl : 0) + 1), *o = out;
    for (char *p = s, *q; ; p = q + fl) {
        q = strstr(p, from);
        if (!q) { strcpy(o, p); break; }
        memcpy(o, p, q - p); o += q - p;
        memcpy(o, to, tl); o += tl;
    }
    free(s);
    return out;
}

EXPORT void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    static void (*fn)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    if (!fn) fn = real("glShaderSource");
    size_t total = 1;
    for (GLsizei i = 0; i < count; ++i) total += length && length[i] >= 0 ? (size_t)length[i] : strlen(string[i]);
    char* src = malloc(total), *o = src;
    for (GLsizei i = 0; i < count; ++i) {
        size_t n = length && length[i] >= 0 ? (size_t)length[i] : strlen(string[i]);
        memcpy(o, string[i], n);
        o += n;
    }
    *o = 0;
    // a version line for GLSL ES, default precisions right after it
    const char* header = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n";
    char* v = strstr(src, "#version");
    if (v) { char* eol = strchr(v, '\n'); memset(v, ' ', eol ? (size_t)(eol - v) : strlen(v)); }
    char* full = malloc(strlen(header) + strlen(src) + 1);
    strcpy(full, header);
    strcat(full, src);
    free(src);
    // GLSL ES has no implicit int -> float conversion
    full = replaceAll(full, ".a=0;", ".a=0.0;");
    full = replaceAll(full, ".a = 0;", ".a = 0.0;");
    full = replaceAll(full, ".a=1;", ".a=1.0;");
    const GLchar* one = full;
    fn(shader, 1, &one, NULL);
    free(full);
}

EXPORT void glCompileShader(GLuint shader) {
    static void (*fn)(GLuint);
    static void (*getiv)(GLuint, GLenum, GLint*);
    static void (*getlog)(GLuint, GLsizei, GLsizei*, GLchar*);
    if (!fn) { fn = real("glCompileShader"); getiv = real("glGetShaderiv"); getlog = real("glGetShaderInfoLog"); }
    fn(shader);
    GLint ok = 0;
    getiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        getlog(shader, sizeof buf, NULL, buf);
        LOG("shader %u failed to compile: %s", shader, buf);
    }
}

// ---------------------------------------------------------------------------------------------
// desktop-only calls

EXPORT void glBindFragDataLocation(GLuint p, GLuint c, const GLchar* n) { (void)p; (void)c; (void)n; }
EXPORT void glBindFragDataLocationEXT(GLuint p, GLuint c, const GLchar* n) { (void)p; (void)c; (void)n; }
EXPORT void glPolygonMode(GLenum f, GLenum m) { (void)f; (void)m; }
EXPORT void glPointSize(GLfloat s) { (void)s; }
EXPORT void glDrawBuffer(GLenum b) { (void)b; }
EXPORT void glClearDepth(double d) { glClearDepthf((GLfloat)d); }
EXPORT void glDepthRange(double n, double f) { glDepthRangef((GLfloat)n, (GLfloat)f); }
EXPORT void glGetTexImage(GLenum t, GLint l, GLenum f, GLenum ty, void* p) { (void)t; (void)l; (void)f; (void)ty; (void)p; }

static int unsupportedCap(GLenum cap) {
    switch (cap) {
    case 0x0DE1:  // GL_TEXTURE_2D
    case 0x0BC0:  // GL_ALPHA_TEST
    case 0x0B50:  // GL_LIGHTING
    case 0x0B10:  // GL_POINT_SMOOTH
    case 0x0B20:  // GL_LINE_SMOOTH
    case 0x0B41:  // GL_POLYGON_SMOOTH
    case 0x809D:  // GL_MULTISAMPLE
    case 0x8DB9:  // GL_FRAMEBUFFER_SRGB
    case 0x8642:  // GL_PROGRAM_POINT_SIZE
    case 0x884F:  // GL_TEXTURE_CUBE_MAP_SEAMLESS
        return 1;
    default: return 0;
    }
}
EXPORT void glEnable(GLenum cap) {
    static void (*fn)(GLenum);
    if (!fn) fn = real("glEnable");
    if (!unsupportedCap(cap)) fn(cap);
}
EXPORT void glDisable(GLenum cap) {
    static void (*fn)(GLenum);
    if (!fn) fn = real("glDisable");
    if (!unsupportedCap(cap)) fn(cap);
}

// Unsized / desktop internal formats that GLES 3 expects to match the data format.
EXPORT void glTexImage2D(GLenum t, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void* p) {
    static void (*fn)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
    if (!fn) fn = real("glTexImage2D");
    if (ifmt == 3 || ifmt == 0x8051 /*GL_RGB8*/) ifmt = GL_RGB;
    if (ifmt == 4 || ifmt == 0x8058 /*GL_RGBA8*/) ifmt = GL_RGBA;
    if (ifmt == GL_RGB && f == GL_RGB && ty == GL_UNSIGNED_SHORT_5_6_5) ifmt = GL_RGB;
    fn(t, l, ifmt, w, h, b, f, ty, p);
}

EXPORT void* glMapBuffer(GLenum target, GLenum access) {
    GLbitfield bits = access == 0x88B8 /*READ_ONLY*/ ? GL_MAP_READ_BIT
                    : access == 0x88B9 /*WRITE_ONLY*/ ? GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT
                    : GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
    GLint size = 0;
    glGetBufferParameteriv(target, GL_BUFFER_SIZE, &size);
    return glMapBufferRange(target, 0, size, bits);
}

// ---------------------------------------------------------------------------------------------
// glX: just enough for SDL-created contexts and GLEW

typedef void (*proc)(void);
EXPORT proc glXGetProcAddressARB(const GLubyte* name);

static int fakeDisplay;
EXPORT void* glXGetCurrentDisplay(void) { return &fakeDisplay; }
EXPORT void* glXGetCurrentContext(void) { return (void*)eglGetCurrentContext(); }
EXPORT int glXQueryVersion(void* dpy, int* major, int* minor) { (void)dpy; if (major) *major = 1; if (minor) *minor = 4; return 1; }
EXPORT const char* glXGetClientString(void* dpy, int name) { (void)dpy; (void)name; return name == 2 ? "1.4" : "Desperados GLES shim"; }
EXPORT const char* glXQueryExtensionsString(void* dpy, int screen) { (void)dpy; (void)screen; return ""; }
EXPORT const char* glXQueryServerString(void* dpy, int screen, int name) { (void)dpy; (void)screen; (void)name; return ""; }
EXPORT void glXSwapIntervalEXT(void* dpy, unsigned long d, int i) { (void)dpy; (void)d; (void)i; }
EXPORT int glXSwapIntervalMESA(unsigned int i) { (void)i; return 0; }
EXPORT int glXSwapIntervalSGI(int i) { (void)i; return 0; }

struct over { const char* name; proc fn; };
static const struct over overrides[] = {
    {"glGetString", (proc)glGetString},
    {"glGetIntegerv", (proc)glGetIntegerv},
    {"glShaderSource", (proc)glShaderSource},
    {"glShaderSourceARB", (proc)glShaderSource},
    {"glCompileShader", (proc)glCompileShader},
    {"glCompileShaderARB", (proc)glCompileShader},
    {"glBindFragDataLocation", (proc)glBindFragDataLocation},
    {"glBindFragDataLocationEXT", (proc)glBindFragDataLocationEXT},
    {"glPolygonMode", (proc)glPolygonMode},
    {"glPointSize", (proc)glPointSize},
    {"glDrawBuffer", (proc)glDrawBuffer},
    {"glClearDepth", (proc)glClearDepth},
    {"glDepthRange", (proc)glDepthRange},
    {"glGetTexImage", (proc)glGetTexImage},
    {"glEnable", (proc)glEnable},
    {"glDisable", (proc)glDisable},
    {"glTexImage2D", (proc)glTexImage2D},
    {"glMapBuffer", (proc)glMapBuffer},
    {"glMapBufferARB", (proc)glMapBuffer},
    {"glXGetProcAddress", (proc)glXGetProcAddressARB},
    {"glXGetProcAddressARB", (proc)glXGetProcAddressARB},
    {"glXGetCurrentDisplay", (proc)glXGetCurrentDisplay},
    {"glXGetCurrentContext", (proc)glXGetCurrentContext},
    {"glXQueryVersion", (proc)glXQueryVersion},
    {"glXGetClientString", (proc)glXGetClientString},
    {"glXQueryExtensionsString", (proc)glXQueryExtensionsString},
    {"glXQueryServerString", (proc)glXQueryServerString},
    {"glXSwapIntervalEXT", (proc)glXSwapIntervalEXT},
    {"glXSwapIntervalMESA", (proc)glXSwapIntervalMESA},
    {"glXSwapIntervalSGI", (proc)glXSwapIntervalSGI},
};

EXPORT proc glXGetProcAddressARB(const GLubyte* uname) {
    const char* name = (const char*)uname;
    if (!name) return NULL;
    for (size_t i = 0; i < sizeof overrides / sizeof overrides[0]; ++i)
        if (!strcmp(overrides[i].name, name)) return overrides[i].fn;
    proc p = (proc)real(name);
    if (!p) {  // core in GLES 3 under the name without the ARB / EXT suffix
        size_t n = strlen(name);
        if (n > 3 && (!strcmp(name + n - 3, "ARB") || !strcmp(name + n - 3, "EXT"))) {
            char base[128];
            if (n - 3 < sizeof base) {
                memcpy(base, name, n - 3);
                base[n - 3] = 0;
                for (size_t i = 0; i < sizeof overrides / sizeof overrides[0]; ++i)
                    if (!strcmp(overrides[i].name, base)) return overrides[i].fn;
                p = (proc)real(base);
            }
        }
    }
    return p;
}
EXPORT proc glXGetProcAddress(const GLubyte* name) { return glXGetProcAddressARB(name); }
