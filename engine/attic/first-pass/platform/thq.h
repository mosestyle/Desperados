// The 2018 port's platform layer, reimplemented: window and GL state ("thqsdlfw"), file access
// ("RFILE"), plus small helpers. The translated game code calls these with the original names and
// the original behaviour.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "gl.h"

// ---------------------------------------------------------------- techs (shader + blend state)
typedef uint32_t string_hash_t;

// The original addressed its techs by a hash of their name; the values are what the game code uses.
enum : string_hash_t {
    TECH_PLAIN = 0x26ea5fffu,          // desperados shader, no keying
    TECH_COLORKEY = 0x842c3ee4u,       // + COLORKEY
    TECH_ALPHA_RED = 0x75211fedu,      // + ALPHA ALPHARED
    TECH_ALPHA_RED_CK = 0x3462d94au,   // + ALPHA ALPHARED COLORKEY
    TECH_ALPHA_BLUE = 0x02a1b908u,     // + ALPHA ALPHABLUE
    TECH_ALPHA_BLUE_CK = 0x9520fcefu,  // + ALPHA ALPHABLUE COLORKEY
    TECH_FILL = 0xfccc2dbdu,           // fill shader (no blending)
    TECH_BLIT = 0x773cf76bu,           // blit shader (no blending)
    TECH_BLIT_BLEND = 0xfcc802adu,     // blit shader, blended
    TECH_FILL_BLEND = 0xbb3dcb74u,     // fill shader, blended
    TECH_VIDEO = 0x4f687665u,          // YUV video
};

struct thqsdlfw_tech_t {
    GLuint program = 0;
    bool blend = false;
    GLenum blendSrc = GL_SRC_ALPHA, blendDst = GL_ONE_MINUS_SRC_ALPHA;
    // texture units 1..4 bound to sampler names, as the original did (unit 0 is never used for samplers)
    int nextUnit = 0;
    GLuint textures[5] = {0, 0, 0, 0, 0};
    uint32_t dirty = 0;
    struct Sampler { std::string name; int unit; };
    Sampler samplers[8];
    int nsamplers = 0;
};

struct thqsdlfw_render_target_t {
    GLuint framebuffer = 0;
    GLuint texture = 0;
    uint32_t width = 0, height = 0;
};

struct thqsdlfw_float4 { float x, y, z, w; };

// window size and the size of the current target
extern int thqsdlfw_sdl_window_width, thqsdlfw_sdl_window_height;
extern int target_width, target_height;
extern GLuint thqsdlfw_last_framebuffer;
extern bool thqsdlfw_last_framebuffer_invalid;
extern bool thqsdlfw_quit;

// Creates the GL objects (call once with a current GLES 3 context).
bool thqsdlfw_gl_init();
void thqsdlfw_gl_shutdown();

thqsdlfw_tech_t* thqsdlfw_set_tech(string_hash_t tech);
void thqsdlfw_set_texture(thqsdlfw_tech_t* tech, const char* name, GLuint texture);
void thqsdlfw_set_constant(thqsdlfw_tech_t* tech, const char* name, const thqsdlfw_float4& v);
// Draws a quad: x, y, w, h in target pixels (origin bottom-left, as GL), u, v, du, dv in texture space.
void thqsdlfw_blit(thqsdlfw_tech_t* tech, float x, float y, float w, float h, float u, float v, float du, float dv);
// Draws n points (x, y pairs in target pixels) as the given GL primitive.
void thqsdlfw_draw(thqsdlfw_tech_t* tech, GLenum mode, uint32_t n, const float* xy);

// Creates a render target (an RGB565 texture, or the given texture, attached to a framebuffer).
void thqsdlfw_create_render_target(thqsdlfw_render_target_t* rt, uint32_t w, uint32_t h, GLuint texture = 0);
void thqsdlfw_destroy_render_target(thqsdlfw_render_target_t* rt);
void thqsdlfw_set_render_target(const thqsdlfw_render_target_t& rt);
// Binds the window.
void thqsdlfw_set_window_target();

// ---------------------------------------------------------------- files
// Paths use '\' or '/', any case: the original lowercased every path (game files are lowercase).
// Relative paths are looked up in the game folder.
struct RFILE_handle_t;
void RFILE_set_root(const std::string& gameFolder);
const std::string& RFILE_root();
RFILE_handle_t* RFILE_open_from_file(const char* path, const char* mode);
void RFILE_close(RFILE_handle_t* f);
size_t RFILE_read(void* dst, size_t size, size_t count, RFILE_handle_t* f);
size_t RFILE_write(const void* src, size_t size, size_t count, RFILE_handle_t* f);
int RFILE_seek(RFILE_handle_t* f, long offset, int whence);
long RFILE_tell(RFILE_handle_t* f);
long RFILE_size(RFILE_handle_t* f);
bool RFILE_eof(RFILE_handle_t* f);
int RFILE_delete(const char* path);
// The folder for saves and settings ("pref path" in the original).
extern std::string thqsdlfw_pref_path;

// ---------------------------------------------------------------- time
uint32_t GetTickCountSUBST();

// ---------------------------------------------------------------- errors
// The original's assertion/error reporter: logs; fatal errors stop the game.
void SBError(bool fatal, const char* file, int line, const char* message);
void SBLog(const char* fmt, ...);
