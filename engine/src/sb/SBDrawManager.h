// SBDrawManager: the original's drawing layer (SBLibNG/SBDrawManager.cpp, 2018 OpenGL version).
// Every surface is an RGB565 picture kept in memory and as an OpenGL texture; blits are drawn
// with the original's fragment shader (data/shaders/desperados_fragment.txt): colour key cut-out,
// "alpha key" (the shadow colour drawn as translucent black), constant colour.
// Surface 1 (also reached as 0) is the screen: the app shows it, scaled, every display frame.
#pragma once
#include <cstdint>
#include <map>
#include <vector>

#include "sb/SBGeo.h"

// SBDrawViewport: CPU access to a surface's pixels (SBDrawManager::GetSurfaceViewport)
struct SBDrawViewport {
    uint32_t id = 0;         // +0
    uint16_t width = 0;      // +4
    uint16_t height = 0;     // +6
    uint32_t pitch = 0;      // +8 bytes per row
    uint16_t bits = 16;      // +0xc 16 (RGB565) or 15
    uint16_t colorKey = 0;   // +0xe
    uint16_t* data = nullptr;  // +0x10
};

// blit flags (the original's values)
enum : uint32_t {
    BLIT_COLORKEY = 0x1,          // cut out the source's colour key
    BLIT_ALPHATEX = 0x100,        // second texture gives the alpha (red channel: 0x400)
    BLIT_ALPHARED = 0x400,
    BLIT_ALPHAINVERT = 0x1000,    // alpha constants x = -1, y = 1
    BLIT_BLACK = 0x2000,          // constant colour black
    BLIT_ALPHAKEY = 0x100000,     // the "alpha key" colour becomes translucent black
    VIEWPORT_READ = 0x40000,
    VIEWPORT_WRITE = 0x80000,
};

class SBDrawManager {
public:
    static SBDrawManager* mpDrawManager;

    SBDrawManager();
    ~SBDrawManager();

    // Needs the GL context. screenW/H = the size of the screen surface (game pixels).
    bool OpenScreen(uint16_t screenW, uint16_t screenH);
    void ResizeScreen(uint16_t screenW, uint16_t screenH);
    void CloseScreen();

    uint32_t CreateSurface(uint16_t w, uint16_t h);
    void DeleteSurface(uint32_t id);
    bool IsSurface(uint32_t id) const { return surfaces.count(id ? id : 1) != 0; }
    uint16_t SurfaceWidth(uint32_t id) const;
    uint16_t SurfaceHeight(uint32_t id) const;
    uint16_t GetColorKeyingForSurface(uint32_t id) const;
    void SetColorKeyingForSurface(uint32_t id, uint16_t key);

    bool GetSurfaceViewport(uint32_t id, SBDrawViewport& vp, uint32_t flags);
    void ReleaseSurfaceViewport(SBDrawViewport& vp);
    // fills a surface from pixels (SBPicture::SaveToSurface)
    void UploadPicture(uint32_t id, const uint16_t* px, uint16_t w, uint16_t h);

    bool Blit(uint32_t src, const SBGeoBoundingBox2D* srcBox, uint32_t dst, const SBGeoBoundingBox2D* dstBox, uint32_t flags);
    bool BlitAlphaKeying(uint32_t src, const SBGeoBoundingBox2D* srcBox, uint32_t dst, const SBGeoBoundingBox2D* dstBox,
                         uint16_t alphaKey, uint16_t percent, uint32_t flags);
    bool BlitAlpha(uint32_t src, const SBGeoBoundingBox2D* srcBox, uint32_t dst, const SBGeoBoundingBox2D* dstBox,
                   uint32_t alphaSurface, uint32_t flags);
    bool BlitAlphaConstant(uint32_t src, const SBGeoBoundingBox2D* srcBox, uint32_t dst, const SBGeoBoundingBox2D* dstBox,
                           uint16_t percent, uint32_t flags);
    void Fill(uint32_t dst, const SBGeoBoundingBox2D* box, uint16_t color, uint32_t flags);

    // the screen's texture for the app (GL name) and size
    unsigned ScreenTexture() const;
    uint16_t ScreenWidth() const { return SurfaceWidth(1); }
    uint16_t ScreenHeight() const { return SurfaceHeight(1); }
    // makes sure every surface's texture is up to date (call before showing the screen)
    void Finish();

    uint16_t pixelBits = 16;  // +0x6c

private:
    struct Surface {
        uint16_t w = 0, h = 0;
        uint16_t colorKey = 0x7c0;
        std::vector<uint16_t> px;  // CPU copy
        bool cpuDirty = false;     // CPU copy changed: upload before use
        bool gpuNewer = false;     // the texture was drawn into: CPU copy is stale
        unsigned tex = 0, fbo = 0;
    };
    std::map<uint32_t, Surface> surfaces;
    uint32_t nextId = 2;
    unsigned prog = 0, vao = 0, vbo = 0;
    int uDst = -1, uSrc = -1, uTarget = -1, uColorKey = -1, uAlphaKey = -1, uConst = -1, uAlphaConst = -1,
        uTexRGB = -1, uTexAlpha = -1, uUseColorKey = -1, uUseAlpha = -1, uAlphaRed = -1;

    Surface* Get(uint32_t id);
    const Surface* Get(uint32_t id) const;
    void MakeTexture(Surface& s);
    void Upload(Surface& s);
    void BlitGL(uint32_t src, uint32_t alphaSrc, const int* srcRect, uint32_t dst, const int* dstRect, uint32_t flags,
                uint16_t colorKey, uint16_t alphaKey, float alpha, float alphaZ);
};
