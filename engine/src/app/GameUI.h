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

private:
    struct Font {
        SBFont font;
        GLuint tex = 0;
        bool ok = false;
    };
    struct Pic {
        GLuint tex = 0;
        int w = 0, h = 0;
    };
    Font dialog, tips, title;
    SBPictureResources res;
    std::map<uint64_t, Pic> pics;
    bool LoadFont(Font& f, const char* path);
    const Pic* Picture(uint32_t id, uint32_t entry);
    float TextWidth(const Font& f, const std::u16string& s, float scale) const;
    void Text(Overlay& o, const Font& f, const std::u16string& s, float x, float y, float scale, float a = 1.0f);
    // the text in lines no wider than `width` (screen pixels); returns the height used
    float Paragraph(Overlay& o, const Font& f, const std::u16string& s, float x, float y, float width, float scale,
                    bool draw);
    float Scale(int sh) const;
};
