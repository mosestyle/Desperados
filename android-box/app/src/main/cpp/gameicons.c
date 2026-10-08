// See gameicons.h.
#include "gameicons.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static gameicon_t* icons;
static int nicons, capicons;
static GLuint atlasTex;

static uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }

static int isTag(const uint8_t* p) {
    if (p[0] < 'A' || p[0] > 'Z') return 0;
    for (int i = 1; i < 4; ++i)
        if (!((p[i] >= 'A' && p[i] <= 'Z') || p[i] == ' ')) return 0;
    return 1;
}
static int validPicture(const uint8_t* d, size_t n, size_t p) {
    if (p + 12 > n) return 0;
    uint32_t w = rd16(d + p), h = rd16(d + p + 2), codec = rd32(d + p + 4), size = rd32(d + p + 8);
    if (!w || !h || w >= 4096 || h >= 4096 || !size || p + 12 + size > n || codec > 2) return 0;
    if (codec == 0) return size == w * h * 2;
    if (codec == 1) return d[p + 12] == 'x';
    return d[p + 12] == 'B' && d[p + 13] == 'Z';
}

static void add(const char* tag, int id, int state, int w, int h, const uint16_t* px) {
    if (nicons == capicons) {
        capicons = capicons ? capicons * 2 : 256;
        icons = realloc(icons, (size_t)capicons * sizeof *icons);
    }
    gameicon_t* g = &icons[nicons++];
    memset(g, 0, sizeof *g);
    memcpy(g->tag, tag, 4);
    g->id = id;
    g->state = state;
    g->w = w;
    g->h = h;
    g->rgba = malloc((size_t)w * h * 4);
    for (int i = 0; i < w * h; ++i) {
        uint16_t c = px[i];
        uint8_t* o = g->rgba + i * 4;
        o[0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
        o[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
        o[2] = (uint8_t)((c & 31) * 255 / 31);
        o[3] = c == 0x07C0 ? 0 : 255;
    }
}

static int parse(const uint8_t* d, size_t n) {
    if (n < 12 || memcmp(d, "SRES", 4) != 0) return 0;
    size_t p = 12;
    char tag[5] = {0};
    int id = 0, state = 0;
    while (p + 12 <= n) {
        if (isTag(d + p)) {
            memcpy(tag, d + p, 4);
            id = (int)rd32(d + p + 4);
            state = 0;
            p += 12;
            // a few header fields (counts...) up to the first picture or the next resource
            for (int k = 0; k < 16 && p + 4 <= n && !validPicture(d, n, p) && !isTag(d + p); ++k) p += 4;
            continue;
        }
        if (!validPicture(d, n, p)) break;  // something else (texts...): stop, we have the pictures
        int w = rd16(d + p), h = rd16(d + p + 2);
        uint32_t codec = rd32(d + p + 4), size = rd32(d + p + 8);
        const uint8_t* data = d + p + 12;
        size_t want = (size_t)w * h * 2;
        if (codec == 0) {
            add(tag, id, state, w, h, (const uint16_t*)data);
        } else if (codec == 1) {
            uint8_t* out = malloc(want);
            uLongf got = (uLongf)want;
            if (out && uncompress(out, &got, data, size) == Z_OK && got >= want) add(tag, id, state, w, h, (const uint16_t*)out);
            free(out);
        }  // (bzip2 pictures are big backgrounds we don't need)
        ++state;
        p += 12 + size;
    }
    return nicons;
}

// one texture with all pictures, packed in shelves
static void upload(void) {
    const int W = 2048;
    int x = 0, y = 0, rowH = 0;
    for (int i = 0; i < nicons; ++i) {
        gameicon_t* g = &icons[i];
        if (x + g->w + 2 > W) { x = 0; y += rowH + 2; rowH = 0; }
        g->uv[0] = (float)x;
        g->uv[1] = (float)y;
        x += g->w + 2;
        if (g->h > rowH) rowH = g->h;
    }
    int H = 1;
    while (H < y + rowH + 2) H *= 2;
    uint8_t* px = calloc((size_t)W * H, 4);
    for (int i = 0; i < nicons; ++i) {
        gameicon_t* g = &icons[i];
        int gx = (int)g->uv[0], gy = (int)g->uv[1];
        for (int r = 0; r < g->h; ++r) memcpy(px + ((size_t)(gy + r) * W + gx) * 4, g->rgba + (size_t)r * g->w * 4, (size_t)g->w * 4);
        g->uv[0] = (gx + 0.5f) / W;
        g->uv[1] = (gy + 0.5f) / H;
        g->uv[2] = (gx + g->w - 0.5f) / W;
        g->uv[3] = (gy + g->h - 0.5f) / H;
    }
    glGenTextures(1, &atlasTex);
    glBindTexture(GL_TEXTURE_2D, atlasTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    free(px);
}

int gameicons_load(const char* gameDir) {
    static const char* langs[] = {"english", "german", "french", "spanish", "italian", "russian", "polish", "dutch", NULL};
    for (int i = 0; langs[i] && !nicons; ++i) {
        char path[1024];
        snprintf(path, sizeof path, "%s/localisation/%s/data/interface/default.res", gameDir, langs[i]);
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t* d = n > 0 ? malloc((size_t)n) : NULL;
        if (d && fread(d, 1, (size_t)n, f) == (size_t)n) parse(d, (size_t)n);
        free(d);
        fclose(f);
    }
    if (nicons) upload();
    return nicons;
}

void gameicons_free(void) {
    for (int i = 0; i < nicons; ++i) free(icons[i].rgba);
    free(icons);
    icons = NULL;
    nicons = capicons = 0;
    if (atlasTex) glDeleteTextures(1, &atlasTex);
    atlasTex = 0;
}

GLuint gameicons_texture(void) { return atlasTex; }
int gameicons_count(void) { return nicons; }
const gameicon_t* gameicon_at(int i) { return i >= 0 && i < nicons ? &icons[i] : NULL; }

const gameicon_t* gameicon(const char* tag, int id, int state) {
    for (int i = 0; i < nicons; ++i)
        if (icons[i].id == id && icons[i].state == state && !memcmp(icons[i].tag, tag, 4)) return &icons[i];
    return NULL;
}
