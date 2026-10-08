// The game's own interface pictures, read from the player's copy of the game:
// localisation/<language>/data/interface/default.res. The action icons (BTTN 18-49, four states
// each: 0 = on the action ring, 1 = normal, 2 = hovered, 3 = pressed), the toolbar items
// (BTTN 50 watch, 51 crawl, 52 stand up, 96 telescope), portraits (TOGL 2-13)...
//
// File layout: "SRES", u32 version, u32 count; per resource: tag[4], u32 id, u32 ?, then for
// most tags a u32 field and the resource's pictures: u16 w, u16 h, u32 codec (0 raw, 1 zlib,
// 2 bzip2), u32 size, data = RGB565 pixels (pure green 0x07C0 = transparent).
#pragma once
#include <GLES3/gl3.h>
#include <stdint.h>

typedef struct {
    char tag[5];
    int id, state;
    int w, h;
    uint8_t* rgba;      // w*h*4, rows top to bottom
    float uv[4];        // in the atlas: u0, v0 (top), u1, v1 (bottom)
} gameicon_t;

// Loads the pictures (gameDir = the Desperados folder) and uploads them as one texture.
// Needs the GL context. Returns the number of pictures (0 = not found).
int gameicons_load(const char* gameDir);
void gameicons_free(void);
GLuint gameicons_texture(void);
const gameicon_t* gameicon(const char* tag, int id, int state);
// all pictures, for matching
int gameicons_count(void);
const gameicon_t* gameicon_at(int i);
