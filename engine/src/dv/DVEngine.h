// DVEngine (Game/DVEngine.cpp): one mission. Loads the level file (.dvd: hunks MISC, BGND, MOVE,
// SGHT, MASK, ELEM, ...), keeps the elements sorted by display order, steps them at 25 Hz
// (PerformHourglass) and draws the visible part of the map (Draw).
#pragma once
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "sb/SBGeo.h"

class SBFile;
class SBDrawManager;
class DVElement;
class DVElementActor;
class DVFastFindGrid;
class DVFrameHolder;
class DVPathFinder;
class DVHikingGuide;
class DVShadowPolygon;
class DVArmsTrader;
class DVScript;
class DVPsychoanalyst;
class DVDialogue;
class DVSequenceManager;
class DVSequenceElement;

class DVEngine {
public:
    static DVEngine* mpEngine;
    explicit DVEngine(SBDrawManager* draw);
    ~DVEngine();

    // levelName = "level_01": reads Data\Levels\level_01.dvd. Returns false if it can't.
    bool LoadStateFromFile(const std::string& levelName);
    uint32_t StateGet(int state) const;

    // one 25 Hz step of the mission
    void PerformHourglass();
    // draws the map at the camera onto the screen surface
    void Draw();

    // the camera: top-left of the view on the map, in map pixels (DVEngine +0xb4c)
    SBGeoPoint2D camera;
    SBGeoVector2D screen;      // +0xbb0 size of the screen surface
    float mapW = 0, mapH = 0;  // +0xbb8, +0xbbc
    float zoom = 1.0f;         // +0xbc0
    void ClampCamera();
    bool silhouettes = false;  // the "show silhouettes" key (DVrawInput +0x15)

    // MISC
    bool night = false;            // +0x1000
    uint8_t nightPercent = 100;    // +0x1001
    uint16_t shadowKey = 0x1f;     // +0x1002 (the level's shadow colour)
    uint8_t weather = 0;           // +0x1004
    uint32_t coneColors[3] = {0x00ff00, 0xffff00, 0xff0000};
    float hearingFactor = 0.75f;   // mfHearingFactor
    SBGeoVector2D MapSize() const { return SBGeoVector2D(mapW, mapH); }
    const std::vector<DVElementActor*>& NPCs() const { return npcs; }

    std::string levelName;
    std::string backgroundName;
    const std::vector<DVElement*>& Elements() const { return elements; }
    std::vector<class DVElementMobile*> mobiles;  // +0xddc (the CART hunk)
    DVFastFindGrid* Grid() const { return grid.get(); }
    DVPathFinder* PathFinder() const { return pathFinder.get(); }
    uint32_t BackgroundSurface() const { return background; }
    uint32_t MinimapSurface() const { return minimap; }
    int ticks = 0;

    // ---- the player's heroes (DVEngine +0xbc: the selection) ----------------------------------
    const std::vector<DVElementActor*>& Heroes() const { return heroes; }
    DVElementActor* Selected() const { return selected; }
    void Select(DVElementActor* hero);
    // ---- the player's commands, kept to save the game (DVEngine::journal) --------------------
    // A saved game is the mission and what the player did, tick by tick: the engine gives the same
    // game again when it does it again (its random numbers start the same way each mission).
    struct Command {
        uint32_t tick = 0;
        uint8_t op = 0;
        int32_t a = 0;
        float x = 0, y = 0;
    };
    enum : uint8_t { CMD_SELECT = 1, CMD_MOVE, CMD_SHOOT, CMD_CROUCH, CMD_MOUNT, CMD_TAKE, CMD_RELOAD, CMD_RUN, CMD_STOP,
                     CMD_SKIP_BRIEFING, CMD_SKIP_LINE, CMD_DROP, CMD_PUSH_CART };
    std::vector<Command> journal;
    bool replaying = false;
    void Record(uint8_t op, int32_t a = 0, float x = 0, float y = 0);
    void Apply(const Command& c);
    void PlayerSelect(DVElementActor* hero);   // Select, kept in the journal
    void PlayerSkipLine();                      // the dialogue's next line
    // a saved game: the level and the journal up to now (Data\..\saves\*.sav)
    bool SaveGame(const std::string& path, const std::string& title) const;
    struct SaveInfo {
        std::string level, title;
        uint32_t ticks = 0;
        int64_t when = 0;
        float camX = -1, camY = -1;
        std::vector<Command> journal;
    };
    static bool ReadSave(const std::string& path, SaveInfo& out);
    // after LoadStateFromFile: the journal again, step by step, up to its tick (progress 0..1)
    void StartReplay(const SaveInfo& s);
    bool Replaying() const { return replayAt < replayQueue.size() || (int)replayUntil > ticks; }
    void ReplaySome(int steps);   // a few steps of it (the app shows its progress meanwhile)
    float ReplayProgress() const { return replayUntil ? (float)ticks / (float)replayUntil : 1.0f; }
    std::vector<Command> replayQueue;
    size_t replayAt = 0;
    uint32_t replayUntil = 0;
    SBGeoPoint2D replayCamera = SBGeoPoint2D(-1, -1);
    void SelectNextPlayable();   // the selected hero can't be used any more: the next one
    // the hero under a map point (DVElementActorPC::IsFocusable), the front-most one
    DVElementActor* HeroAt(const SBGeoPoint2D& p, float slack) const;
    // orders for the selected hero (left click / double click on the ground)
    bool OrderMove(const SBGeoPoint2D& p, bool run);
    void OrderMakeRunning();
    // horses: the free horse under a map point, the selected hero gets on it
    DVElementActor* HorseAt(const SBGeoPoint2D& p, float slack) const;
    // a cart Sanchez can push there (DVElementMobile::IsFocusable)
    class DVElementMobile* PushableAt(const SBGeoPoint2D& p, float slack) const;
    bool OrderPush(class DVElementMobile* m);
    bool OrderMount(DVElementActor* horse);
    // Cooper / Sanchez: to a body (out cold or dead) and on the shoulder
    bool OrderTakeBody(DVElementActor* body);
    // lie down / stand up
    void OrderCrouch();
    void OrderStop();
    bool showPaths = false;  // the debug view of the paths (DisplayPathFindGraph)

    // ---- the view cones ---------------------------------------------------------------------
    // the NPC whose view cone is shown (+0x120): the one the player looks at, or the last one
    // who raised the alarm
    DVElementActor* watched = nullptr;
    // the NPC under a map point (DVElementActorNPC::IsFocusable), the front-most one
    DVElementActor* NPCAt(const SBGeoPoint2D& p, float slack) const;
    // a script target one can shoot at (a pot on a balcony) under a map point
    DVElement* ShootableTargetAt(const SBGeoPoint2D& p, float slack) const;
    // shows / hides the cone of the NPC (DVEngine::ManageInputView)
    void ToggleWatched(DVElementActor* npc);

    // ---- fights ---------------------------------------------------------------------------------
    // the selected hero shoots at someone (DVEngine::ManageInputActionShoot) / reloads
    bool OrderShoot(DVElement* target);
    void OrderReload();
    // DVElementActorNPC::Noise (0x08238630): who hears it thinks about it. Types: 0 a hero's
    // shot (650), 2 / 3 a villain's (550), 0x14 a body falling (100)
    void Noise(uint32_t type, const SBGeoPoint3D& at, DVElementActor* who);
    // a hero is dead: the mission is lost
    void HeroDied(DVElementActor* hero);
    bool missionFailed = false;
    DVElementActor* deadHero = nullptr;
    bool highlander = false;   // the cheat: the heroes can't be killed

    // ---- the scripts (step 4) ------------------------------------------------------------------
    DVScript* Script() const { return script.get(); }
    std::unique_ptr<DVDialogue> dialogue;           // +0x27c
    std::unique_ptr<class DVSound> sound;           // +0xe0c
    std::unique_ptr<DVSequenceManager> sequences;   // +0x6d4
    // DVEngine::PerformExecuteCommand (0x08293770): the sequences' elements without an actor
    void PerformExecuteCommand(DVSequenceElement* e);
    // DVEngine::LockUser (0x08293c10): the player's hands are tied (a cut scene)
    void LockUser(bool on);
    bool userLocked = false;      // +0x118
    bool briefing = false;        // +0x11b the intro / a cut scene (the interface is hidden)
    // the camera the scripts move (+0xb5c the goal, +0xd3c the speed, +0x888 the element waiting)
    void CameraScrollTo(const SBGeoPoint2D& p, float speed, uint16_t fixedSpeed);
    void CameraJumpTo(const SBGeoPoint2D& p);
    SBGeoPoint2D CheckLocationIsValidForCamera(const SBGeoPoint2D& p) const;  // DVScript, 0x0834a5d0
    bool cameraScrolling = false;
    SBGeoPoint2D cameraGoal;
    uint16_t cameraFixedSpeed = 0;   // +0xb6c
    float cameraRamp = 0;            // +0xd40 (the table at +0xc64)
    DVSequenceElement* cameraElement = nullptr;
    bool cameraMoved = false;        // the app takes the engine's camera
    DVElement* follow = nullptr;     // +0xd4c... SelectFollowElement: the camera stays on him
    float zoomWanted = -1.0f;        // +0xbc8
    std::vector<DVSequenceElement*> timers;   // +0xf8c
    // DVEngine::SetHint (0x082a7280): the tutorial's hint (1..), 0 = none
    void SetHint(uint32_t h) { hint = h; }
    uint32_t hint = 0;
    // DVMenu::AddSentence / DoneSentence: the objectives
    struct Sentence {
        uint32_t id;
        bool done;
    };
    std::vector<Sentence> sentences;
    void AddSentence(uint32_t id);
    void DoneSentence(uint32_t id);
    int victoryText = -1;            // +0xff4 DVScript::ChooseVictoryDefeatText
    int missionResult = 0;           // CheckVictoryCondition: 1 accomplished, 2 failed
    bool forceVictoryCheck = false;  // +0xd24
    int32_t skipData = -1;           // ISetSkipData: the intro can be skipped
    bool mapShown = false;
    void StateSelectActor(uint16_t kind);   // 0x082a51c0 (ISelectPC)
    // every character (heroes, NPCs, animals) of the level, on the map or not
    const std::vector<DVElementActor*>& Characters() const { return characters; }
    // skips the intro: the game runs on at once (no voices, no waiting) until it is over
    void SkipBriefing();
    bool fast = false;
    uint32_t frameCounter = 0;       // +0xd18

private:
    SBDrawManager* draw;
    std::unique_ptr<DVFrameHolder> frames;  // +0x168
    std::unique_ptr<DVFastFindGrid> grid;   // +0x44c
    std::unique_ptr<DVPathFinder> pathFinder;  // +0xe08
    std::vector<std::unique_ptr<DVElement>> ownElements;
    std::vector<DVElement*> elements;       // +0x84
    int LoadMobileElementsFromFile(SBFile& f);
    uint32_t background = 0;   // +0xba4
    uint32_t minimap = 0;      // +0xba8
    std::vector<DVElementActor*> heroes;
    std::vector<DVElementActor*> npcs;      // +0xd88
    std::vector<DVElementActor*> characters;  // +0xda4
    std::unique_ptr<DVHikingGuide> hiking;  // +0xe10
    std::unique_ptr<DVShadowPolygon> shadow;  // +0x90c
    std::unique_ptr<DVScript> script;          // DVScript (the mission's scripts)
    static std::unique_ptr<DVArmsTrader> sArms;          // DVArmsTrader::mpTheOnlyArmstraderInTown
    static std::unique_ptr<DVPsychoanalyst> sPsy;        // DVPsychoanalyst::mpTheOnlyPsychoanalistInTown
    DVElementActor* selected = nullptr;

    int LoadMiscFromFile(SBFile& f);
    int LoadBackgroundFromFile(SBFile& f);
    int LoadElemFromFile(SBFile& f);
    void SortForEngine();
    void PerformRefreshAllElements(uint32_t surface);
};
