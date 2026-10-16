// Menus: see Menus.h
#include "app/Menus.h"

#include <SDL.h>
#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "app/GameUI.h"
#include "app/Overlay.h"
#include "dv/DVDialogue.h"
#include "dv/DVElement.h"
#include "dv/DVEngine.h"
#include "sb/SBFile.h"

namespace {

std::u16string Ascii(const char* s) {
    std::u16string o;
    for (; *s; ++s) o += (char16_t)(unsigned char)*s;
    return o;
}

std::u16string Cp1252(const std::string& s) {
    std::u16string o;
    for (unsigned char c : s) o += c == 0x92 ? u'\'' : (char16_t)c;
    return o;
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::string Lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// the menus' colours
const float kGold[3] = {0.98f, 0.83f, 0.48f};
const float kInk[3] = {0.22f, 0.13f, 0.05f};

}  // namespace

// ---- the player's progress ---------------------------------------------------------------------

bool Progress::Load(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        int v = 0;
        char key[64];
        if (sscanf(line, "%63[^=]=%d", key, &v) != 2) continue;
        std::string k = Trim(key);
        if (k == "unlocked") unlocked = std::max(1, v);
        else if (k == "fx") fx = std::max(0, std::min(10, v));
        else if (k == "music") music = std::max(0, std::min(10, v));
        else if (k == "voice") voice = std::max(0, std::min(10, v));
        else if (k.compare(0, 5, "done_") == 0) {
            int i = atoi(k.c_str() + 5) - 1;
            if (i >= 0 && i < 100) {
                if ((int)done.size() <= i) done.resize((size_t)i + 1, 0);
                done[(size_t)i] = v;
            }
        }
    }
    fclose(f);
    return true;
}

bool Progress::Save(const std::string& path) const {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "unlocked=%d\nfx=%d\nmusic=%d\nvoice=%d\n", unlocked, fx, music, voice);
    for (size_t i = 0; i < done.size(); ++i)
        if (done[i]) fprintf(f, "done_%02zu=1\n", i + 1);
    return fclose(f) == 0;
}

// ---- the campaign (Data\Configuration\eldiablo.cpg: [NN. Name] sections, key = value) -------------

bool Menus::Init(const std::string& gameRoot) {
    root = gameRoot;
    savesDir = root + "/saves";
    mkdir(savesDir.c_str(), 0775);
    SBFile f;
    const char* langs[] = {"english", "french", "german", "spanish", "italian", "russian"};
    bool found = f.Open("Data\\Configuration\\eldiablo.cpg", 1) == 0;
    for (const char* l : langs)
        if (!found) found = f.Open(std::string("localisation\\") + l + "\\Data\\Configuration\\eldiablo.cpg", 1) == 0;
    if (found) {
        std::string all((size_t)f.Size(), '\0');
        if (!all.empty()) f.Serialize(&all[0], (int)all.size());
        CampaignMission m;
        std::string section;
        auto flush = [&]() {
            if (!m.level.empty()) {
                if (m.title.empty()) {
                    size_t dot = section.find(". ");
                    m.title = Cp1252(dot == std::string::npos ? section : section.substr(dot + 2));
                }
                m.level = Lower(m.level);
                campaign.push_back(m);
            }
            m = CampaignMission();
        };
        size_t at = 0;
        while (at < all.size()) {
            size_t end = all.find('\n', at);
            std::string line = Trim(all.substr(at, end == std::string::npos ? std::string::npos : end - at));
            at = end == std::string::npos ? all.size() : end + 1;
            if (line.empty()) continue;
            if (line[0] == '[') {
                flush();
                section = line.substr(1, line.find(']') == std::string::npos ? std::string::npos : line.find(']') - 1);
                continue;
            }
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = Lower(Trim(line.substr(0, eq))), v = Trim(line.substr(eq + 1));
            if (k == "title") m.title = Cp1252(v);
            else if (k == "level") m.level = v;
            else if (k == "map") m.map = "Data\\Interface\\Maps\\" + v;
            else if (k == "xposition") m.x = atoi(v.c_str());
            else if (k == "yposition") m.y = atoi(v.c_str());
        }
        flush();
    }
    if (campaign.empty())
        for (int i = 1; i <= 25; ++i) {
            CampaignMission m;
            char n[32];
            snprintf(n, sizeof n, "level_%02d", i);
            m.level = n;
            snprintf(n, sizeof n, "Mission %d", i);
            m.title = Ascii(n);
            campaign.push_back(m);
        }
    progress.Load(ProgressPath());
    progress.done.resize(std::max(progress.done.size(), campaign.size()), 0);
    SBLog("campaign: %zu missions, %d open", campaign.size(), progress.unlocked);
    return true;
}

std::string Menus::SlotPath(int slot) const {
    if (slot == 0) return savesDir + "/auto.sav";
    char n[32];
    snprintf(n, sizeof n, "/slot%d.sav", slot);
    return savesDir + n;
}

std::string Menus::ProgressPath() const { return savesDir + "/progress.ini"; }

void Menus::SaveProgress() const {
    if (!progress.Save(ProgressPath())) SBLog("can't write %s", ProgressPath().c_str());
}

int Menus::MissionOfLevel(const std::string& level) const {
    std::string l = Lower(level);
    for (size_t i = 0; i < campaign.size(); ++i)
        if (campaign[i].level == l) return (int)i;
    return -1;
}

bool Menus::HasSlot(int slot) const {
    FILE* f = fopen(SlotPath(slot).c_str(), "rb");
    if (f) fclose(f);
    return f != nullptr;
}

void Menus::ReadSlots() {
    slots.assign(9, Slot());
    for (int i = 0; i < 9; ++i) {
        DVEngine::SaveInfo si;
        if (!DVEngine::ReadSave(SlotPath(i), si)) continue;
        slots[(size_t)i].used = true;
        slots[(size_t)i].level = si.level;
        slots[(size_t)i].when = si.when;
        slots[(size_t)i].ticks = si.ticks;
    }
}

void Menus::Accomplished(int mission) {
    if (mission < 0) return;
    if ((int)progress.done.size() <= mission) progress.done.resize((size_t)mission + 1, 0);
    progress.done[(size_t)mission] = 1;
    progress.unlocked = std::max(progress.unlocked, std::min((int)campaign.size(), mission + 2));
    SaveProgress();
}

void Menus::Open(Screen s) {
    if (s == LOAD || s == SAVE) ReadSlots();
    screen = s;
}

void Menus::Back(bool inMission) {
    switch (screen) {
    case PAUSE: screen = NONE; break;
    case OPTIONS:
    case LOAD:
    case SAVE: screen = returnTo; break;
    case MAP:
    case MISSIONS:
    case THE_END: screen = MAIN; break;
    default: break;
    }
    (void)inMission;
}

std::u16string Menus::MissionLabel(int i) const {
    char n[16];
    snprintf(n, sizeof n, "%02d. ", i + 1);
    return Ascii(n) + campaign[(size_t)i].title;
}

std::u16string Menus::SlotLabel(int i) const {
    const Slot& s = slots[(size_t)i];
    char head[32];
    if (i == 0) snprintf(head, sizeof head, "Automatic: ");
    else snprintf(head, sizeof head, "%d: ", i);
    if (!s.used) return Ascii(head) + u"empty";
    int m = MissionOfLevel(s.level);
    std::u16string name = m >= 0 ? MissionLabel(m) : Ascii(s.level.c_str());
    char tail[96];
    unsigned secs = s.ticks / 25;
    time_t t = (time_t)s.when;
    struct tm lt;
    localtime_r(&t, &lt);
    snprintf(tail, sizeof tail, "   %u:%02u   %04d-%02d-%02d %02d:%02d", secs / 60, secs % 60, lt.tm_year + 1900, lt.tm_mon + 1,
             lt.tm_mday, lt.tm_hour, lt.tm_min);
    return Ascii(head) + name + Ascii(tail);
}

// the map picture (1024 x 512) fitted to the screen
void Menus::MapRect(int sw, int sh, float& x, float& y, float& w, float& h) const {
    float k = std::min(sw / 1024.0f, sh / 512.0f);
    w = 1024 * k;
    h = 512 * k;
    x = (sw - w) * 0.5f;
    y = (sh - h) * 0.5f;
}

// ---- the buttons of each screen ----------------------------------------------------------------

std::vector<Menus::Button> Menus::Layout(DVEngine* e, int sw, int sh) const {
    std::vector<Button> b;
    float s = sh / 600.0f;
    float bw = 300 * s, bh = 46 * s, gap = 10 * s;
    auto column = [&](float x, float y, std::initializer_list<Button> list) {
        for (Button one : list) {
            one.x = x;
            one.y = y;
            one.w = bw;
            one.h = bh;
            b.push_back(one);
            y += bh + gap;
        }
    };
    auto centred = [&](float top, std::initializer_list<Button> list) { column((sw - bw) * 0.5f, top, list); };
    switch (screen) {
    case MAIN: {
        bool cont = HasSlot(0) || progress.unlocked > 1;
        column(sw * 0.97f - bw, sh * 0.34f,
               {{0, 0, 0, 0, CONTINUE, 0, u"Continue", cont, 0},
                {0, 0, 0, 0, GO, MAP, u"New campaign", true, 0},
                {0, 0, 0, 0, GO, LOAD, u"Load game", true, 0},
                {0, 0, 0, 0, GO, MISSIONS, u"Missions", true, 0},
                {0, 0, 0, 0, GO, OPTIONS, u"Options", true, 0},
                {0, 0, 0, 0, QUIT, 0, u"Quit", true, 0}});
        break;
    }
    case MAP: {
        float mx, my, mw, mh;
        MapRect(sw, sh, mx, my, mw, mh);
        float k = mw / 1024.0f;
        auto plate = [&](int i, Act a, int arg, const char16_t* label, bool on) {
            b.push_back({mx + 656 * k, my + (281 + 35 * i) * k, 160 * k, 28 * k, a, arg, label, on, 1});
        };
        plate(0, PLAY_MISSION, mapMission, u"Start mission", true);
        plate(1, MAP_STEP, -1, u"Previous", mapMission > 0);
        plate(2, MAP_STEP, 1, u"Next", mapMission + 1 < (int)campaign.size());
        plate(5, GO, MAIN, u"Back", true);
        break;
    }
    case MISSIONS: {
        int n = (int)campaign.size();
        int rows = (n + 1) / 2;
        float rh = std::min(bh, (sh * 0.80f) / rows - 4 * s), cw = std::min(sw * 0.46f, 460 * s);
        float top = sh * 0.11f;
        for (int i = 0; i < n; ++i) {
            int c = i / rows, r = i % rows;
            b.push_back({sw * 0.5f + (c == 0 ? -cw - 6 * s : 6 * s), top + r * (rh + 4 * s), cw, rh, GO, 1000 + i,
                         MissionLabel(i), true, 2});
        }
        b.push_back({sw - 140 * s, sh - 50 * s, 130 * s, 40 * s, GO, MAIN, u"Back", true, 0});
        break;
    }
    case LOAD:
    case SAVE: {
        int first = screen == SAVE ? 1 : 0;
        float rh = std::min(bh, sh * 0.75f / 9 - 4 * s), w = std::min(sw * 0.86f, 860 * s);
        float y = sh * 0.13f;
        for (int i = first; i < 9; ++i) {
            bool used = i < (int)slots.size() && slots[(size_t)i].used;
            b.push_back({(sw - w) * 0.5f, y, w, rh, screen == SAVE ? SAVE_SLOT : LOAD_SLOT, i, SlotLabel(i),
                         screen == SAVE || used, 2});
            y += rh + 4 * s;
        }
        b.push_back({sw - 140 * s, sh - 50 * s, 130 * s, 40 * s, GO, -1, u"Back", true, 0});
        break;
    }
    case PAUSE: {
        bool ended = e && (e->missionFailed || e->missionResult);
        centred(sh * 0.18f, {{0, 0, 0, 0, RESUME, 0, u"Resume", true, 0},
                             {0, 0, 0, 0, GO, SAVE, u"Save game", !ended && e && !e->briefing, 0},
                             {0, 0, 0, 0, GO, LOAD, u"Load game", true, 0},
                             {0, 0, 0, 0, RESTART, 0, u"Restart mission", true, 0},
                             {0, 0, 0, 0, GO, OPTIONS, u"Options", true, 0},
                             {0, 0, 0, 0, TO_MAIN, 0, u"Main menu", true, 0}});
        break;
    }
    case OPTIONS: {
        float y = sh * 0.25f, w = 60 * s;
        for (int i = 0; i < 3; ++i) {
            b.push_back({sw * 0.5f + 40 * s, y, w, bh, VOL, i * 2, u"", true, 4});
            b.push_back({sw * 0.5f + 40 * s + w + 140 * s, y, w, bh, VOL, i * 2 + 1, u"", true, 5});
            y += bh + gap * 2;
        }
        b.push_back({(sw - bw) * 0.5f, y + gap * 2, bw, bh, GO, -1, u"Back", true, 0});
        break;
    }
    case VICTORY:
        centred(sh * 0.84f, {{0, 0, 0, 0, NEXT_MISSION, 0, u"Continue", true, 0}});
        break;
    case FAILED: {
        float y = sh * 0.84f, w2 = 220 * s;
        float x = (sw - 3 * w2 - 2 * gap) * 0.5f;
        b.push_back({x, y, w2, bh, RESTART, 0, u"Try again", true, 0});
        b.push_back({x + w2 + gap, y, w2, bh, GO, LOAD, u"Load game", true, 0});
        b.push_back({x + 2 * (w2 + gap), y, w2, bh, TO_MAIN, 0, u"Main menu", true, 0});
        break;
    }
    case THE_END:
        centred(sh * 0.80f, {{0, 0, 0, 0, TO_MAIN, 0, u"Main menu", true, 0}});
        break;
    default: break;
    }
    return b;
}

Menus::Action Menus::Tap(float x, float y, DVEngine* e, int sw, int sh) {
    for (const Button& b : Layout(e, sw, sh)) {
        if (x < b.x || x > b.x + b.w || y < b.y || y > b.y + b.h || !b.enabled) continue;
        switch (b.act) {
        case GO:
            if (b.arg == -1) {
                screen = returnTo;
            } else if (b.arg >= 1000) {
                mapMission = b.arg - 1000;
                screen = MAP;
            } else {
                Screen to = (Screen)b.arg;
                if (to == OPTIONS || to == LOAD || to == SAVE) returnTo = screen;
                if (to == MAP && screen == MAIN) mapMission = 0;
                Open(to);
            }
            return {};
        case VOL: {
            int* v = b.arg / 2 == 0 ? &progress.fx : b.arg / 2 == 1 ? &progress.music : &progress.voice;
            *v = std::max(0, std::min(10, *v + (b.arg % 2 ? 1 : -1)));
            SaveProgress();
            return {VOLUMES, 0};
        }
        case MAP_STEP:
            mapMission = std::max(0, std::min((int)campaign.size() - 1, mapMission + b.arg));
            return {};
        default: return {b.act, b.arg};
        }
    }
    return {};
}

// ---- drawing -------------------------------------------------------------------------------------

void Menus::Draw(Overlay& o, GameUI& ui, DVEngine* e, int sw, int sh) {
    if (screen == NONE) {
        if (!message.empty() && SDL_GetTicks() < messageUntil) {
            float s = sh / 600.0f;
            std::u16string m = GameUI::U16(message);
            float tw = ui.TextWidth(ui.tips, m, s);
            o.Rect((sw - tw) * 0.5f - 16 * s, sh * 0.78f, tw + 32 * s, 36 * s, 0.08f, 0.06f, 0.02f, 0.8f);
            ui.Text(o, ui.tips, m, (sw - tw) * 0.5f, sh * 0.78f + (36 * s - ui.tips.font.Height() * s) * 0.5f, s);
        }
        return;
    }
    float s = sh / 600.0f;
    auto text = [&](const GameUI::Font& f, const std::u16string& t, float cx, float y, float scale, const float* c,
                    bool centred = true) {
        float tw = ui.TextWidth(f, t, scale);
        ui.Text(o, f, t, centred ? cx - tw * 0.5f : cx, y, scale, 1.0f, c[0], c[1], c[2]);
    };
    const GameUI::Font& head = ui.title.ok ? ui.title : ui.dialog;
    const GameUI::Font& btn = ui.buttons.ok ? ui.buttons : ui.tips;
    const GameUI::Font& row = ui.list.ok ? ui.list : ui.tips;
    // the background
    bool overGame = e && (screen == PAUSE || screen == VICTORY || screen == FAILED || screen == REPLAY ||
                          ((screen == OPTIONS || screen == LOAD || screen == SAVE) && returnTo != MAIN));
    if (overGame) {
        o.Rect(0, 0, (float)sw, (float)sh, 0.02f, 0.01f, 0.0f, screen == REPLAY ? 0.85f : 0.74f);
    } else if (screen == MAP) {
        o.Rect(0, 0, (float)sw, (float)sh, 0, 0, 0, 1);
    } else {
        o.Rect(0, 0, (float)sw, (float)sh, 0.1f, 0.03f, 0.02f, 1);
        if (const GameUI::Pic* bg = ui.PngPicture(root + "/bootmenu/background.png")) {
            // filled, cut at the edges
            float k = std::max(sw / (float)bg->w, sh / (float)bg->h);
            float w = bg->w * k, h = bg->h * k;
            o.Tex(bg->tex, 0, 0, 1, 1, (sw - w) * 0.5f, (sh - h) * 0.5f, w, h);
        }
        if (screen != MAIN) o.Rect(0, 0, (float)sw, (float)sh, 0.02f, 0.01f, 0.0f, 0.62f);
    }
    switch (screen) {
    case MAP: {
        float mx, my, mw, mh;
        MapRect(sw, sh, mx, my, mw, mh);
        const CampaignMission& m = campaign[(size_t)mapMission];
        if (const GameUI::Pic* p = ui.FilePicture(m.map.empty() ? "Data\\Interface\\Maps\\louisiana.map" : m.map, 0xffff))
            o.Tex(p->tex, 0, 0, 1, 1, mx, my, mw, mh);
        float k = mw / 1024.0f;
        // where it is: a red mark, beating
        float px = mx + (m.x + 198) * k, py = my + (m.y + 24) * k;
        float beat = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.006f);
        float r = (7 + 4 * beat) * k;
        o.Rect(px - r, py - r, 2 * r, 2 * r, 0.75f, 0.05f, 0.02f, 0.5f + 0.4f * beat);
        o.Rect(px - 3 * k, py - 3 * k, 6 * k, 6 * k, 0.3f, 0.0f, 0.0f, 1);
        // the mission's name on the paper
        char n[32];
        snprintf(n, sizeof n, "Mission %d", mapMission + 1);
        float hs = std::min(s * 1.0f, k * 1.6f);
        float cx = mx + 522 * k;
        o.Rect(mx + 396 * k, my + 398 * k, 252 * k, 74 * k, 0.15f, 0.08f, 0.02f, 0.6f);
        text(row, Ascii(n), cx, my + 403 * k, hs * 0.8f, kGold);
        float ts = hs * 0.85f;
        while (ui.TextWidth(head, m.title, ts) > 244 * k && ts > hs * 0.3f) ts *= 0.92f;
        text(head, m.title, cx, my + 422 * k, ts, kGold);
        if (mapMission < (int)progress.done.size() && progress.done[(size_t)mapMission])
            text(row, u"Accomplished", cx, my + 476 * k, hs * 0.75f, kInk);
        else if (mapMission + 1 > progress.unlocked)
            text(row, u"Not reached yet in the campaign", cx, my + 476 * k, hs * 0.7f, kInk);
        break;
    }
    case MISSIONS: text(head, u"Missions", sw * 0.5f, 14 * s, s, kGold); break;
    case LOAD: text(head, u"Load game", sw * 0.5f, 18 * s, s, kGold); break;
    case SAVE: text(head, u"Save game", sw * 0.5f, 18 * s, s, kGold); break;
    case PAUSE: {
        text(head, u"Pause", sw * 0.5f, sh * 0.05f, s, kGold);
        if (e) {
            int m = MissionOfLevel(e->levelName);
            if (m >= 0) text(row, MissionLabel(m), sw * 0.5f, sh * 0.14f, s * 0.9f, kGold);
        }
        break;
    }
    case OPTIONS: {
        text(head, u"Options", sw * 0.5f, sh * 0.08f, s, kGold);
        const char16_t* names[3] = {u"Sound effects", u"Music", u"Voices"};
        int vals[3] = {progress.fx, progress.music, progress.voice};
        float y = sh * 0.25f, bh = 46 * s;
        for (int i = 0; i < 3; ++i) {
            float ty = y + (bh - btn.font.Height() * s) * 0.5f;
            ui.Text(o, btn, names[i], sw * 0.5f - 20 * s - ui.TextWidth(btn, names[i], s), ty, s, 1, kGold[0], kGold[1], kGold[2]);
            // the level: ten bars
            float bx = sw * 0.5f + 110 * s;
            for (int j = 0; j < 10; ++j)
                o.Rect(bx + j * 12 * s, y + 10 * s, 9 * s, bh - 20 * s, j < vals[i] ? 0.9f : 0.3f, j < vals[i] ? 0.7f : 0.25f,
                       j < vals[i] ? 0.3f : 0.2f, 0.95f);
            y += bh + 20 * s;
        }
        break;
    }
    case VICTORY:
    case FAILED: {
        bool won = screen == VICTORY;
        const char* pic = won ? "Data\\Interface\\Debriefing\\victory.sxt"
                              : (e && e->ticks % 2) ? "Data\\Interface\\Debriefing\\gameover2.sxt"
                                                    : "Data\\Interface\\Debriefing\\gameover1.sxt";
        float top = sh * 0.04f;
        if (const GameUI::Pic* p = ui.FilePicture(pic)) {
            float h = sh * 0.36f, w = h * p->w / p->h;
            o.Tex(p->tex, 0, 0, 1, 1, (sw - w) * 0.5f, top, w, h);
            top += h + 10 * s;
        }
        if (!won && !failedWho.empty()) {
            text(head, GameUI::U16(failedWho + " is dead"), sw * 0.5f, top, s, kGold);
            top += head.font.Height() * s + 6 * s;
        }
        if (e && e->dialogue) {
            int vt = e->victoryText;
            if (vt < 0) vt = won ? 0 : (int)e->dialogue->victoryCount;
            std::u16string body = e->dialogue->VictoryText((uint32_t)vt);
            float w = std::min((float)sw * 0.8f, 800 * s);
            float bs = s * 0.9f;
            float th = ui.Paragraph(o, ui.dialog, body, 0, 0, w, bs, false);
            while (th > sh * 0.84f - top - 10 * s && bs > s * 0.5f) {
                bs *= 0.9f;
                th = ui.Paragraph(o, ui.dialog, body, 0, 0, w, bs, false);
            }
            // centred lines
            if (ui.dialog.ok) {
                std::vector<std::u16string> lines = ui.dialog.font.Wrap(body, (int)(w / bs));
                float lh = ui.dialog.font.Height() * bs;
                for (size_t i = 0; i < lines.size(); ++i)
                    ui.Text(o, ui.dialog, lines[i], (sw - ui.TextWidth(ui.dialog, lines[i], bs)) * 0.5f, top + lh * i, bs);
            }
        }
        break;
    }
    case REPLAY: {
        text(head, u"Loading the saved game", sw * 0.5f, sh * 0.4f, s, kGold);
        float w = sw * 0.5f, h = 14 * s, x = (sw - w) * 0.5f, y = sh * 0.52f;
        o.Rect(x, y, w, h, 0.2f, 0.15f, 0.1f, 0.9f);
        o.Rect(x, y, w * std::max(0.0f, std::min(1.0f, replayProgress)), h, 0.9f, 0.7f, 0.3f, 1);
        break;
    }
    case THE_END:
        text(head, u"The End", sw * 0.5f, sh * 0.3f, s * 1.4f, kGold);
        text(row, u"El Diablo is no more. The campaign is over.", sw * 0.5f, sh * 0.48f, s, kGold);
        break;
    default: break;
    }
    // the buttons
    for (const Button& b : Layout(e, sw, sh)) {
        const GameUI::Font& f = b.style == 0 ? btn : row;
        float a = b.enabled ? 1.0f : 0.45f;
        float ts = b.style == 1 ? b.h / (f.font.Height() * 1.15f) : std::min(s, b.h / (f.font.Height() * 1.2f));
        if (b.style == 4 || b.style == 5) {
            // minus / plus
            o.Rect(b.x, b.y, b.w, b.h, 0.2f, 0.11f, 0.04f, 0.88f * a);
            float cx = b.x + b.w * 0.5f, cy = b.y + b.h * 0.5f, l = b.h * 0.25f, t = 3 * s;
            o.Rect(cx - l, cy - t * 0.5f, 2 * l, t, kGold[0], kGold[1], kGold[2], a);
            if (b.style == 5) o.Rect(cx - t * 0.5f, cy - l, t, 2 * l, kGold[0], kGold[1], kGold[2], a);
            continue;
        }
        if (b.style == 0) {
            o.Rect(b.x, b.y, b.w, b.h, 0.2f, 0.11f, 0.04f, 0.88f * a);
            o.Rect(b.x, b.y, b.w, 2 * s, 0.75f, 0.55f, 0.25f, a);
            o.Rect(b.x, b.y + b.h - 2 * s, b.w, 2 * s, 0.75f, 0.55f, 0.25f, a);
        } else if (b.style == 2) {
            o.Rect(b.x, b.y, b.w, b.h, 0.12f, 0.07f, 0.03f, 0.8f * a);
        }
        float tw = ui.TextWidth(f, b.label, ts);
        while (tw > b.w - 12 * s && ts > 0.3f * s) {
            ts *= 0.92f;
            tw = ui.TextWidth(f, b.label, ts);
        }
        float tx = b.style == 2 ? b.x + 10 * s : b.x + (b.w - tw) * 0.5f;
        float ty = b.y + (b.h - f.font.Height() * ts) * 0.5f;
        const float* c = b.style == 1 ? kInk : kGold;
        // a mission of the list: grey when not reached yet, green when accomplished
        float col[3] = {c[0], c[1], c[2]};
        if (b.style == 2 && b.act == GO && b.arg >= 1000) {
            int i = b.arg - 1000;
            if (i + 1 > progress.unlocked) col[0] = col[1] = col[2] = 0.6f;
            else if (i < (int)progress.done.size() && progress.done[(size_t)i]) col[0] = 0.6f, col[1] = 0.95f, col[2] = 0.5f;
        }
        if (b.style == 1) {
            // (no shadow on the silver: a pale ground under the letters)
            o.Rect(tx - 6 * s, b.y + 3 * s, tw + 12 * s, b.h - 6 * s, 0.94f, 0.91f, 0.84f, 0.8f * a);
            const float W = f.font.PictureWidth(), H = f.font.PictureHeight();
            float px = tx;
            for (char16_t ch : b.label) {
                const SBFont::Glyph* g = f.font.Find(ch);
                if (!g) continue;
                px += g->before * ts;
                if (g->w > 0)
                    o.Tex(f.tex, g->x / W, 0, (g->x + g->w) / W, 1, px, ty, g->w * ts, H * ts, col[0], col[1], col[2], a);
                px += (g->w + g->after) * ts;
            }
        } else {
            ui.Text(o, f, b.label, tx, ty, ts, a, col[0], col[1], col[2]);
        }
    }
}
