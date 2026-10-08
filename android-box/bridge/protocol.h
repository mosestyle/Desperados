// The bridge between the game process (the original Desperados running through Box64) and the
// Android app that owns the screen, the touch screen and the speaker.
//
// The game process has no GPU of its own: its libGL.so.1 (child_gl.c) turns every OpenGL call
// into a small command on a socket; the app (replay.c) replays them with the phone's OpenGL ES.
// Object names (textures, buffers, shaders...) and uniform locations are chosen by the game
// side, so no call ever waits for an answer, except glReadPixels.
// The app sends input (mouse, keys) back on the same socket; the game side injects it into
// SDL's event queue at the end of each frame.
//
// Every message: u16 op, u16 reserved, u32 payload length, payload (little endian, packed).
#pragma once
#include <stdint.h>

#define DESP_BRIDGE_VERSION 1

enum {
    // game -> app
    OP_HELLO = 1,          // u32 version, i32 width, i32 height
    OP_FRAME_END,          // (nothing)
    OP_ENABLE, OP_DISABLE, // u32 cap
    OP_BLEND_FUNC,         // u32 s, u32 d
    OP_BLEND_FUNC_SEP,     // u32 sRGB, dRGB, sA, dA
    OP_BLEND_EQUATION,     // u32 mode
    OP_CLEAR,              // u32 mask
    OP_CLEAR_COLOR,        // f32 r g b a
    OP_VIEWPORT,           // i32 x y w h
    OP_SCISSOR,            // i32 x y w h
    OP_COLOR_MASK,         // u8 r g b a
    OP_PIXEL_STORE,        // u32 pname, i32 value
    OP_ACTIVE_TEXTURE,     // u32 unit
    OP_GEN_TEXTURE,        // u32 name
    OP_DELETE_TEXTURE,     // u32 name
    OP_BIND_TEXTURE,       // u32 target, u32 name
    OP_TEX_PARAMETER_I,    // u32 target, u32 pname, i32 value
    OP_TEX_IMAGE_2D,       // u32 target, i32 level, i32 ifmt, i32 w, i32 h, u32 fmt, u32 type, u8 hasData, data
    OP_TEX_SUB_IMAGE_2D,   // u32 target, i32 level, i32 x, i32 y, i32 w, i32 h, u32 fmt, u32 type, data
    OP_GEN_BUFFER,         // u32 name
    OP_DELETE_BUFFER,      // u32 name
    OP_BIND_BUFFER,        // u32 target, u32 name
    OP_BUFFER_DATA,        // u32 target, u32 usage, u32 size, u8 hasData, data
    OP_BUFFER_SUB_DATA,    // u32 target, u32 offset, u32 size, data
    OP_GEN_VERTEX_ARRAY,   // u32 name
    OP_DELETE_VERTEX_ARRAY,
    OP_BIND_VERTEX_ARRAY,  // u32 name
    OP_ENABLE_ATTRIB, OP_DISABLE_ATTRIB,  // u32 index
    OP_ATTRIB_POINTER,     // u32 index, i32 size, u32 type, u8 normalized, i32 stride, u32 offset
    OP_CREATE_SHADER,      // u32 name, u32 type
    OP_SHADER_SOURCE,      // u32 name, source text
    OP_COMPILE_SHADER,     // u32 name
    OP_DELETE_SHADER,      // u32 name
    OP_CREATE_PROGRAM,     // u32 name
    OP_ATTACH_SHADER, OP_DETACH_SHADER,   // u32 program, u32 shader
    OP_BIND_ATTRIB_LOCATION,  // u32 program, u32 index, name text
    OP_LINK_PROGRAM,       // u32 program
    OP_USE_PROGRAM,        // u32 program
    OP_DELETE_PROGRAM,     // u32 program
    OP_UNIFORM_LOCATION,   // u32 program, i32 virtual location, name text
    OP_UNIFORM_I,          // i32 location, u32 count(1-4), i32 v[count]
    OP_UNIFORM_F,          // i32 location, u32 count(1-4), f32 v[count]
    OP_UNIFORM_FV,         // i32 location, u32 components, u32 count, f32 values
    OP_UNIFORM_MATRIX_FV,  // i32 location, u32 dim, u32 count, u8 transpose, f32 values
    OP_DRAW_ARRAYS,        // u32 mode, i32 first, i32 count
    OP_DRAW_ELEMENTS,      // u32 mode, i32 count, u32 type, u32 offset (bound element buffer)
    OP_GEN_FRAMEBUFFER,    // u32 name
    OP_DELETE_FRAMEBUFFER, // u32 name
    OP_BIND_FRAMEBUFFER,   // u32 target, u32 name
    OP_FRAMEBUFFER_TEXTURE_2D,  // u32 target, u32 attachment, u32 textarget, u32 texture, i32 level
    OP_READ_PIXELS,        // i32 x y w h, u32 fmt, u32 type -> app answers OP_PIXELS
    OP_GENERATE_MIPMAP,    // u32 target
    OP_LINE_WIDTH,         // f32 width
    OP_DEPTH_FUNC,         // u32 func
    OP_DEPTH_MASK,         // u8 flag

    // app -> game
    OP_PIXELS = 100,       // pixel data for OP_READ_PIXELS
    OP_INPUT,              // struct desp_input
    OP_WINDOW_SIZE,        // i32 width, i32 height (the game's window, in game pixels)
    OP_FRAME_DONE,         // (nothing) the app has shown a frame: the game may draw another
                           // (the game keeps at most 2 frames ahead of the screen, so input
                           // never waits behind a queue of old frames)
};

#pragma pack(push, 1)
typedef struct {
    uint16_t op, reserved;
    uint32_t len;
} desp_msg_header;

// IN_MOUSE_TO: put the game's cursor on (x, y) in game window pixels. The game only reads
// relative mouse motion and keeps its own cursor (in the boot menu in 1920x1080 units, in the
// game in window pixels); the game side converts.
enum { IN_MOUSE_MOTION = 1, IN_MOUSE_DOWN, IN_MOUSE_UP, IN_MOUSE_WHEEL, IN_KEY_DOWN, IN_KEY_UP, IN_QUIT, IN_MOUSE_TO };
typedef struct {
    uint32_t type;
    int32_t x, y;          // mouse position in game window pixels / wheel amount
    int32_t dx, dy;        // relative motion
    uint32_t button;       // SDL button (1 left, 3 right)
    uint32_t scancode, keycode;
} desp_input;
#pragma pack(pop)
