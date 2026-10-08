// DVFrameHolder (Game/DVFrameHolder.cpp): the bank of every sprite frame of the level. Frames
// come from the .dvf files (rows of RGB565 runs: x0, x1 then the pixels); each sprite file is
// added once and its frames keep their bank index. Unpacking a frame fills the space around the
// runs with the transparent colour 0x07C0; the shadow colour of the file (0x001F) is replaced by
// the level's shadow key (DVEngine state 15) when the frame is added.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class SBFile;
struct SBDrawViewport;

struct DVpackedSprite {
    uint16_t width = 0, height = 0;  // +0, +2
    uint32_t size = 0;               // +4 bytes of packed data
    std::vector<uint16_t> data;      // +8 packed rows
};

class DVFrameHolder {
public:
    static DVFrameHolder* mpFrameHolder;
    DVFrameHolder();
    ~DVFrameHolder();

    // DVFrameHolder::AddRowFromDVFStream: adds `count` frames of the file named `name` (once);
    // returns the bank index of its first frame. nightPercent > 0 darkens them (night levels).
    unsigned AddRowFromDVFStream(SBFile& f, uint16_t count, const std::string& name, uint16_t shadowKey,
                                 bool night, uint16_t nightPercent, bool noNight);
    uint16_t GetSpriteWidth(unsigned i) const { return i < frames.size() ? frames[i].width : 0; }
    uint16_t GetSpriteHeight(unsigned i) const { return i < frames.size() ? frames[i].height : 0; }
    size_t FrameCount() const { return frames.size(); }
    const DVpackedSprite* Frame(unsigned i) const { return i < frames.size() ? &frames[i] : nullptr; }

    uint16_t shadowPercent = 0x28;  // +0x6c how dark shadows are (40 %)

    void UnCompressFrame(SBDrawViewport& vp, unsigned i) const;
    void UnCompressFrameWipeShadow(SBDrawViewport& vp, unsigned i) const;
    void UnCompressFrameIntoTheShadow(SBDrawViewport& vp, unsigned i, uint16_t color) const;

private:
    std::vector<DVpackedSprite> frames;     // +0x14
    std::map<std::string, uint16_t> files;  // +0xfc name -> first frame
};
