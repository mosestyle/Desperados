// The game's menus for the phone: the title screen, the campaign's map before each mission (the
// original's Data\Interface\Maps, the missions of eldiablo.cpg), the list of the missions, the
// saved games (a mission and what the player did in it, DVEngine::SaveGame), the pause menu, the
// volumes, and the end of a mission (Data\Interface\Debriefing: well done / game over).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Overlay;
class GameUI;
class DVEngine;

struct CampaignMission {
    std::u16string title;
    std::string level;  // level_01
    std::string map;    // Data\Interface\Maps\louisiana.map
    int x = 0, y = 0;   // where it is on the map
};

// what the player has done (saves\progress.ini) and his volumes
struct Progress {
    int unlocked = 1;        // the campaign's missions open (1: the first)
    std::vector<int> done;   // 1: accomplished
    int fx = 8, music = 6, voice = 10;  // 0..10
    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
};

class Menus {
public:
    enum Screen { NONE, MAIN, MAP, MISSIONS, LOAD, SAVE, PAUSE, OPTIONS, VICTORY, FAILED, REPLAY, THE_END };
    enum Act { NOTHING, RESUME, PLAY_MISSION, LOAD_SLOT, SAVE_SLOT, RESTART, TO_MAIN, QUIT, VOLUMES, CONTINUE, NEXT_MISSION,
               GO, VOL, MAP_STEP, PAGE };
    struct Action {
        Act act = NOTHING;
        int arg = 0;
    };

    bool Init(const std::string& gameRoot);
    std::vector<CampaignMission> campaign;
    Progress progress;
    Screen screen = MAIN;
    Screen returnTo = MAIN;   // where Back goes from the volumes and the saved games
    int mapMission = 0;       // the mission the map shows
    float replayProgress = 0;
    std::string failedWho;
    std::string message;      // a line for a moment ("Game saved")
    uint32_t messageUntil = 0;

    void Open(Screen s);
    void Back(bool inMission);
    void Draw(Overlay& o, GameUI& ui, DVEngine* e, int sw, int sh);
    Action Tap(float x, float y, DVEngine* e, int sw, int sh);

    std::string SlotPath(int slot) const;  // 0: the automatic one
    std::string ProgressPath() const;
    int MissionOfLevel(const std::string& level) const;
    bool HasSlot(int slot) const;
    void ReadSlots();
    void Accomplished(int mission);  // the campaign goes on
    void SaveProgress() const;

private:
    struct Button {
        float x, y, w, h;
        Act act;
        int arg;
        std::u16string label;
        bool enabled;
        int style;  // 0 a button, 1 a plate of the map, 2 a row of a list, 3 text only
    };
    std::vector<Button> Layout(DVEngine* e, int sw, int sh) const;
    struct Slot {
        bool used = false;
        std::string level;
        int64_t when = 0;
        uint32_t ticks = 0;
    };
    std::vector<Slot> slots;  // 0 automatic, 1..8
    std::string root, savesDir;
    void MapRect(int sw, int sh, float& x, float& y, float& w, float& h) const;
    std::u16string MissionLabel(int i) const;
    std::u16string SlotLabel(int i) const;
};
