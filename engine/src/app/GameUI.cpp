#include "app/GameUI.h"

#include <SDL.h>

#include <algorithm>
#include <vector>

#include "app/Overlay.h"
#include "dv/DVDialogue.h"
#include "dv/DVEngine.h"
#include "sb/SBFile.h"
#include "sb/SBPicture.h"
#include "stb_image.h"

#include <cstdio>

// RGB565 pixels -> RGBA, the colour key transparent
static GLuint MakeTexture(const std::vector<uint16_t>& px, int w, int h, uint16_t key, uint16_t glass = 0xffff) {
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    for (size_t i = 0; i < px.size() && i < (size_t)w * h; ++i) {
        uint16_t c = px[i];
        rgba[i * 4 + 0] = (uint8_t)((c >> 11) * 255 / 31);
        rgba[i * 4 + 1] = (uint8_t)((c >> 5 & 63) * 255 / 63);
        rgba[i * 4 + 2] = (uint8_t)((c & 31) * 255 / 31);
        rgba[i * 4 + 3] = c == key ? 0 : 255;
        if (c == glass && glass != 0xffff) {
            // the see-through part of the debriefing's plaque
            rgba[i * 4 + 0] = 30;
            rgba[i * 4 + 1] = 18;
            rgba[i * 4 + 2] = 8;
            rgba[i * 4 + 3] = 170;
        }
    }
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

bool GameUI::LoadFont(Font& f, const char* path) {
    if (!f.font.LoadFromFile(path)) {
        SBLog("font %s not found", path);
        return false;
    }
    f.tex = MakeTexture(f.font.Pixels(), f.font.PictureWidth(), f.font.PictureHeight(), 0);
    f.ok = true;
    return true;
}

bool GameUI::Init() {
    LoadFont(dialog, "Data\\Interface\\Fonts\\dialog.fnt");
    LoadFont(tips, "Data\\Interface\\Fonts\\tips.fnt");
    LoadFont(title, "Data\\Interface\\Fonts\\title.fnt");
    LoadFont(buttons, "Data\\Interface\\Fonts\\buttons_1.fnt");
    LoadFont(list, "Data\\Interface\\Fonts\\list_0.fnt");
    std::string lang = DVDialogue::mpTexts ? DVDialogue::mpTexts->language : "english";
    if (!res.Load(lang)) SBLog("default.res not found: no portraits");
    return dialog.ok;
}

void GameUI::Shutdown() {
    for (auto& p : files)
        if (p.second.tex) glDeleteTextures(1, &p.second.tex);
    files.clear();
    for (Font* f : {&dialog, &tips, &title, &buttons, &list})
        if (f->tex) glDeleteTextures(1, &f->tex);
    for (auto& p : pics)
        if (p.second.tex) glDeleteTextures(1, &p.second.tex);
    pics.clear();
}

const GameUI::Pic* GameUI::Picture(uint32_t id, uint32_t entry) {
    uint64_t k = (uint64_t)id << 32 | entry;
    auto it = pics.find(k);
    if (it != pics.end()) return it->second.tex ? &it->second : nullptr;
    Pic p;
    SBPictureSixteen pic;
    if (res.Get(id, entry, pic)) {
        p.w = pic.GetWidth();
        p.h = pic.GetHeight();
        p.tex = MakeTexture(pic.Pixels(), p.w, p.h, 0x07c0);
    }
    pics[k] = p;
    return p.tex ? &pics[k] : nullptr;
}

const GameUI::Pic* GameUI::FilePicture(const std::string& path, uint16_t key) {
    auto it = files.find(path);
    if (it != files.end()) return it->second.tex ? &it->second : nullptr;
    Pic p;
    SBPictureSixteen pic;
    if (pic.LoadFromFile(path) && pic.GetWidth() > 0) {
        p.w = pic.GetWidth();
        p.h = pic.GetHeight();
        p.tex = MakeTexture(pic.Pixels(), p.w, p.h, key, path.find("Debriefing") != std::string::npos ? 0x001f : 0xffff);
    } else {
        SBLog("picture %s not found", path.c_str());
    }
    files[path] = p;
    return p.tex ? &files[path] : nullptr;
}

const GameUI::Pic* GameUI::PngPicture(const std::string& path) {
    auto it = files.find(path);
    if (it != files.end()) return it->second.tex ? &it->second : nullptr;
    Pic p;
    std::vector<unsigned char> buf;
    if (FILE* f = fopen(path.c_str(), "rb")) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0) {
            buf.resize((size_t)n);
            if (fread(buf.data(), 1, buf.size(), f) != buf.size()) buf.clear();
        }
        fclose(f);
    }
    int w = 0, h = 0, n = 0;
    unsigned char* rgba = buf.empty() ? nullptr : stbi_load_from_memory(buf.data(), (int)buf.size(), &w, &h, &n, 4);
    if (rgba) {
        glGenTextures(1, &p.tex);
        glBindTexture(GL_TEXTURE_2D, p.tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        p.w = w;
        p.h = h;
        stbi_image_free(rgba);
    }
    files[path] = p;
    return p.tex ? &files[path] : nullptr;
}

std::u16string GameUI::U16(const std::string& s) {
    std::u16string o;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp = c;
        int n = 1;
        if (c >= 0xf0 && i + 3 < s.size()) cp = (c & 7u) << 18 | (s[i + 1] & 63u) << 12 | (s[i + 2] & 63u) << 6 | (s[i + 3] & 63u), n = 4;
        else if (c >= 0xe0 && i + 2 < s.size()) cp = (c & 15u) << 12 | (s[i + 1] & 63u) << 6 | (s[i + 2] & 63u), n = 3;
        else if (c >= 0xc0 && i + 1 < s.size()) cp = (c & 31u) << 6 | (s[i + 1] & 63u), n = 2;
        o += (char16_t)(cp > 0xffff ? '?' : cp);
        i += n;
    }
    return o;
}

float GameUI::Scale(int sh) const { return (float)sh / 600.0f; }

float GameUI::TextWidth(const Font& f, const std::u16string& s, float scale) const {
    return f.ok ? (float)f.font.Width(s) * scale : 0.0f;
}

void GameUI::Text(Overlay& o, const Font& f, const std::u16string& s, float x, float y, float scale, float a, float cr,
                  float cg, float cb) {
    if (!f.ok) return;
    const float W = f.font.PictureWidth(), H = f.font.PictureHeight();
    for (int pass = 0; pass < 2; ++pass) {
        float px = x + (pass == 0 ? 1.5f * scale : 0), py = y + (pass == 0 ? 1.5f * scale : 0);
        for (char16_t c : s) {
            int k = c == 0x2026 ? 3 : 1;
            const SBFont::Glyph* g = f.font.Find(c);
            if (!g) continue;
            for (int r = 0; r < k; ++r) {
                px += g->before * scale;
                if (g->w > 0) {
                    if (pass == 0)
                        o.Tex(f.tex, g->x / W, 0, (g->x + g->w) / W, 1, px, py, g->w * scale, H * scale, 0, 0, 0, a * 0.8f);
                    else
                        o.Tex(f.tex, g->x / W, 0, (g->x + g->w) / W, 1, px, py, g->w * scale, H * scale, cr, cg, cb, a);
                }
                px += (g->w + g->after) * scale;
            }
        }
    }
}

float GameUI::Paragraph(Overlay& o, const Font& f, const std::u16string& s, float x, float y, float width, float scale,
                        bool draw) {
    if (!f.ok) return 0;
    std::vector<std::u16string> lines = f.font.Wrap(s, (int)(width / scale));
    float lh = f.font.Height() * scale;
    if (draw)
        for (size_t i = 0; i < lines.size(); ++i) Text(o, f, lines[i], x, y + lh * i, scale);
    return lh * lines.size();
}

int GameUI::HitButton(float x, float y, DVEngine* e, int sw, int sh) const {
    if (!e) return 0;
    float s = Scale(sh);
    float bw = 120 * s, bh = 34 * s, m = 10 * s;
    if (e->briefing) {
        // SKIP, bottom right in the black bar
        if (x > sw - bw - m && y > sh - bh - m) return 1;
        return 0;
    }
    if (!e->sentences.empty() && x > sw - bw - m && x < sw - m && y > m && y < m + bh) return 2;
    return 0;
}

void GameUI::Draw(Overlay& o, DVEngine* e, int sw, int sh) {
    if (!e) return;
    float s = Scale(sh);
    DVDialogue* d = e->dialogue.get();
    // the cut scenes: the Skip button
    if (e->briefing) {
        float bw = 120 * s, bh = 34 * s, m = 10 * s;
        o.Rect(sw - bw - m, sh - bh - m, bw, bh, 0.25f, 0.2f, 0.1f, 0.8f);
        std::u16string skip = u"Skip";
        float tw = TextWidth(tips, skip, s);
        Text(o, tips, skip, sw - bw * 0.5f - m - tw * 0.5f, sh - bh * 0.5f - m - tips.font.Height() * s * 0.5f, s);
    }
    // the dialogue panel (DVDialogue::PlayDialogue: (16, 9) - (1004, 144) of the 1024 screen)
    if (d && d->Speaker() >= 0) {
        float pw = std::min((float)sw - 20 * s, 988 * s), ph = 135 * s;
        float px = (sw - pw) * 0.5f, py = 9 * s;
        o.Rect(px, py, pw, ph, 0.05f, 0.04f, 0.02f, 0.72f);
        o.Rect(px, py, pw, 2 * s, 0.55f, 0.42f, 0.2f, 0.9f);
        o.Rect(px, py + ph - 2 * s, pw, 2 * s, 0.55f, 0.42f, 0.2f, 0.9f);
        // the portrait, its mouth moving while the line is said (SBFramePannelFX::SelectDialoguePannel)
        static const uint32_t kPortraits[25] = {0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5,
                                                0xc2, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xff};
        int sp = d->Speaker();
        uint32_t pid = sp < 25 ? kPortraits[sp] : 0xc2;
        uint32_t frame = (SDL_GetTicks() / 110) % 5;
        float textX = px + 14 * s;
        if (const Pic* p = Picture(pid, frame)) {
            float h = ph - 16 * s, w = h * p->w / p->h;
            o.Tex(p->tex, 0, 0, 1, 1, px + 8 * s, py + 8 * s, w, h);
            textX = px + 8 * s + w + 12 * s;
        }
        float tw = px + pw - 14 * s - textX;
        float ts = s;
        std::u16string text = d->Text();
        float th = Paragraph(o, dialog, text, textX, py, tw, ts, false);
        while (th > ph - 16 * s && ts > s * 0.6f) {
            ts *= 0.9f;
            th = Paragraph(o, dialog, text, textX, py, tw, ts, false);
        }
        Paragraph(o, dialog, text, textX, py + (ph - th) * 0.5f, tw, ts, true);
    }
    // the tutorial's hint (DVEngine::SetHint), at the bottom
    if (e->hint && d && !e->briefing) {
        std::u16string h = d->HintText(e->hint);
        if (!h.empty()) {
            float w = std::min((float)sw * 0.6f, 620 * s);
            float th = Paragraph(o, tips, h, 0, 0, w - 24 * s, s, false);
            float x = (sw - w) * 0.5f, y = sh - th - 34 * s;
            o.Rect(x, y - 10 * s, w, th + 20 * s, 0.08f, 0.06f, 0.02f, 0.75f);
            Paragraph(o, tips, h, x + 12 * s, y, w - 24 * s, s, true);
        }
    }
    // the objectives: a button, and the list when it is open
    if (!e->sentences.empty() && !e->briefing && d) {
        float bw = 120 * s, bh = 34 * s, m = 10 * s;
        o.Rect(sw - bw - m, m, bw, bh, 0.15f, 0.12f, 0.05f, showObjectives ? 0.9f : 0.6f);
        std::u16string label = u"Objectives";
        Text(o, tips, label, sw - bw * 0.5f - m - TextWidth(tips, label, s) * 0.5f, m + (bh - tips.font.Height() * s) * 0.5f, s);
        if (showObjectives) {
            float w = std::min((float)sw * 0.55f, 560 * s), x = sw - w - m, y = m + bh + 6 * s;
            float total = 0;
            for (const auto& sn : e->sentences) total += Paragraph(o, tips, d->SentenceText(sn.id), 0, 0, w - 50 * s, s, false) + 8 * s;
            o.Rect(x, y, w, total + 16 * s, 0.05f, 0.04f, 0.02f, 0.85f);
            float yy = y + 8 * s;
            for (const auto& sn : e->sentences) {
                o.Rect(x + 12 * s, yy + 4 * s, 14 * s, 14 * s, sn.done ? 0.3f : 0.5f, sn.done ? 0.7f : 0.45f, 0.2f, 0.9f);
                yy += Paragraph(o, tips, d->SentenceText(sn.id), x + 36 * s, yy, w - 50 * s, s, true) + 8 * s;
            }
        }
    }
    // the end of the mission
    if (drawEnd && (e->missionResult == 1 || (e->missionResult == 2 && d))) {
        o.Rect(0, sh * 0.3f, (float)sw, sh * 0.4f, 0.05f, 0.04f, 0.02f, 0.82f);
        std::u16string head = e->missionResult == 1 ? u"Mission accomplished" : u"Mission failed";
        const Font& hf = title.ok ? title : dialog;
        Text(o, hf, head, (sw - TextWidth(hf, head, s)) * 0.5f, sh * 0.33f, s);
        int vt = e->victoryText;
        if (vt < 0) vt = e->missionResult == 1 ? 0 : (int)d->victoryCount;
        std::u16string body = d->VictoryText((uint32_t)vt);
        float w = std::min((float)sw * 0.8f, 800 * s);
        Paragraph(o, dialog, body, (sw - w) * 0.5f, sh * 0.42f, w, s * 0.9f, true);
        std::u16string tap = e->missionResult == 1 ? u"Tap for the next mission" : u"Tap to try again";
        Text(o, tips, tap, (sw - TextWidth(tips, tap, s)) * 0.5f, sh * 0.64f, s);
    }
}
