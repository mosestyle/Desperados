// The game's own interface for the scripts (our phone layout, the original's pictures and fonts):
// the dialogue panel at the top (SBFramePannelFX: the speaker's portrait from default.res, the
// line in the dialogue font), the tutorial hints (tips font), the objectives, the cut scenes'
// black bars and the end of the mission.
#pragma once
#include <GLES3/gl3.h>

#include <cstdint>
#include <map>
#include <string>

#include "sb/SBFont.h"

class Overlay;
class DVEngine;

class GameUI {
public:
    bool Init();
    void Shutdown();
    // draws what the scripts show now
    void Draw(Overlay& o, DVEngine* e, int sw, int sh);
    // the objectives' list (a tap on the objectives button)
    bool showObjectives = false;
    // the buttons this draws: 1 skip the intro, 2 objectives, 0 none
    int HitButton(float x, float y, DVEngine* e, int sw, int sh) const;

    // ---- for the menus (Menus.cpp) ------------------------------------------------------------
    struct Font {
        SBFont font;
        GLuint tex = 0;
        bool ok = false;
    };
    struct Pic {
        GLuint tex = 0;
        int w = 0, h = 0;
    };
    Font dialog, tips, title, buttons, list;
    SBPictureResources res;
    std::map<uint64_t, Pic> pics;
    bool LoadFont(Font& f, const char* path);
    const Pic* Picture(uint32_t id, uint32_t entry);
    float TextWidth(const Font& f, const std::u16string& s, float scale) const;
    void Text(Overlay& o, const Font& f, const std::u16string& s, float x, float y, float scale, float a = 1.0f,
              float r = 1, float g = 1, float b = 1);
    // a game picture file (16-bit, the colour key transparent), kept
    const Pic* FilePicture(const std::string& gamePath, uint16_t key = 0x07c0);
    // a PNG of the player's folder (the launcher's background), kept
    const Pic* PngPicture(const std::string& realPath);
    std::map<std::string, Pic> files;
    static std::u16string U16(const std::string& utf8);
    // the text in lines no wider than `width` (screen pixels); returns the height used
    float Paragraph(Overlay& o, const Font& f, const std::u16string& s, float x, float y, float width, float scale,
                    bool draw);
    float Scale(int sh) const;
    // the end of the mission is drawn by the menus
    bool drawEnd = false;
};
