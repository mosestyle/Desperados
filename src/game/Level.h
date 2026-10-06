// A loaded mission map: background, placed characters and scenery, depth masks,
// walkable areas and simple hero control (select, walk, run).
#pragma once
#include <SDL.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../formats/Dialogues.h"
#include "../formats/Image16.h"
#include "../formats/LevelElements.h"
#include "../formats/LevelFile.h"
#include "../formats/Masks.h"
#include "../formats/MotionAreas.h"
#include "../formats/Paths.h"
#include "../formats/Profiles.h"
#include "../formats/ScriptFile.h"
#include "../formats/ScriptZones.h"
#include "../formats/SightObstacles.h"
#include "../formats/SpriteFile.h"
#include "../formats/Weapons.h"
#include "../render/SpriteAtlas.h"
#include "../render/Textures.h"
#include "Camera.h"
#include "NavGrid.h"
#include "ScriptVM.h"

class Level : public ScriptHost {
public:
    struct ActorDot { float x, y; Faction faction; bool selected; };
    enum class Action { None, Gun, Melee, Throw };

    ~Level();
    void stopMusic();
    bool load(SDL_Renderer* r, int number);
    void update(float dt);
    void render(SDL_Renderer* r, const Camera& cam, int screenW, int screenH);
    void releaseTextures();
    void recreateTextures(SDL_Renderer* r);

    int width() const { return background_.w; }
    int height() const { return background_.h; }
    const Image16& minimap() const { return minimap_; }
    int actorCount() const;
    int instanceCount() const { return (int)instances_.size(); }
    void actorDots(std::vector<ActorDot>& out) const;

    // --- hero control ---
    // Hero under a world point (with `slack` extra world pixels around the sprite), or -1.
    int pickHero(float wx, float wy, float slack) const;
    int selected() const { return selected_; }
    void select(int idx) { selected_ = idx; }
    bool selectedPosition(float& x, float& y) const;
    // Sends the selected hero to a world point. Returns false if it can't get there.
    bool moveSelected(float wx, float wy, bool run);
    void setSelectedRunning(bool run);
    // Stance: standing <-> lying (crawling when moving). Plays the get-down / stand-up animation.
    void toggleStanceSelected();
    bool selectedProne() const;
    // Draws a frame of the selected hero's sprite fitted into `box` (used for HUD buttons).
    void drawSelectedFrame(SDL_Renderer* r, SDL_FRect box, int anim, int dir, int entry = -1);

    // --- hero actions (LevelCombat.cpp) ---
    bool selectedCan(Action a) const;
    int selectedActionAnim(Action a) const;    // animation used as the button icon
    bool orderAttack(Action a, int targetIdx);  // selected hero attacks (walks into range first)
    int pickTarget(float wx, float wy, float slack) const;  // enemy, civilian or scripted object
    float selectedGunRange() const;
    int selectedAmmo(int* maxAmmo = nullptr) const;
    void drawRange(SDL_Renderer* r, const Camera& cam, float range, SDL_Color c);
    int firstHero() const;
    bool showMasks = true;
    bool dumpNav(const std::string& bmpPath, int layer) const;  // debug: walkable cells as an image

    // --- enemies (LevelAI.cpp) ---
    int pickEnemy(float wx, float wy, float slack) const;
    void toggleCone(int idx);   // show/hide an enemy's field of view
    void hideAllCones();
    void showAllCones();
    void debugPlaceSelected(float x, float y);
    bool debugNearestEnemy(float x, float y, float& ex, float& ey, int skip = 0) const;
    void debugState() const;
    bool debugAttackElement(int elem, Action a);  // test helper: selected hero attacks element `elem`
    bool allHeroesDead() const;
    int alertedCount() const;

    // --- mission scripts (LevelScript.cpp) ---
    bool hasScript() const { return !scripts_.empty(); }
    bool userLocked() const { return userLock_ > 0; }  // cutscene: no player control
    bool cinematic() const { return userLock_ > 0; }
    // Camera centre wanted by the script this frame (cutscene camera moves / locks).
    bool scriptCamera(float& cx, float& cy) const;
    void setViewCenter(float cx, float cy) { viewX_ = cx; viewY_ = cy; }
    void setViewSize(float w, float h) { viewW_ = w; viewH_ = h; }
    // Subtitles, hints, objective messages, mission result (screen space).
    void renderOverlay(SDL_Renderer* r, int sw, int sh, float ui);
    int missionResult() const { return result_; }  // 0 running, 1 accomplished, 2 failed
    std::string missionResultText() const;
    const std::vector<std::pair<int, bool>>& objectives() const { return objectives_; }
    std::string objectiveText(int i) const;
    void stopVoices();
    int32_t native(int id, const int32_t* args, int count) override;

private:
    struct Instance {
        LevelElement el;
        const SpriteFile* file = nullptr;
        const SpriteSet* set = nullptr;
        const SpriteRecord* rec = nullptr;
        int entry = 0;
        float ticks = 0;            // time on the current entry, in 25 Hz ticks
        int sortY = 0;
        SDL_Rect world{0, 0, 0, 0}; // current frame rectangle in world pixels
        // actors
        float x = 0, y = 0;         // feet, world pixels
        int floor = 0, dir = 0, anim = 0;
        std::vector<SDL_FPoint> path;
        size_t pathIdx = 0;
        bool running = false;
        float stepAcc = 0;          // ground distance walked on the current frame
        bool prone = false;         // lying down / crawling
        int transition = -1;        // one-shot animation playing, -1 = none
        bool holdLast = false;      // stay on the last frame of the one-shot (dead)
        // --- AI / health ---
        enum class AI { None, Calm, Suspicious, Alert, Searching, KO, Dead } ai = AI::None;
        int health = 100, maxHealth = 100;
        const Profile* profile = nullptr;
        int pathId = -1, wpIdx = 0;          // patrol route and current waypoint
        std::vector<WayCommand> cmds;        // commands of the waypoint being executed
        size_t cmdIdx = 0;
        float waitT = 0;                     // waiting (commands, searching)
        bool atWaypoint = false;
        float meter = 0;                     // detection 0..1
        int target = -1;                     // hero being watched / attacked
        float seenX = 0, seenY = 0;          // last known hero position
        float lostT = 0, repathT = 0, shootT = 0, thinkT = 0, markT = 0;
        bool drawn = false, showCone = false;
        bool untouched = true;               // hero not commanded yet (scripted start positions are safe)
        int searchTurns = 0;
        float koT = 0;                       // knocked out: time until waking up
        bool discovered = false;             // body already found by an enemy
        // hero orders
        Action order = Action::None;
        int orderTarget = -1;
        float orderRepath = 0;
        int pending = 0, pendingTarget = -1; // effect applied when the current one-shot animation ends
        int ammo = 0, maxAmmo = 0;
        float gunRange = 0;
        int meleeAnim = -1;
        bool meleeKills = false;
        // scripting
        int elem = -1;               // index in the level's element list (the scripts' actor id)
        int script = -1;             // ScriptInstance index, -1 = none
        bool hidden = false;         // sent to "Honolulu" (off the map) or animation switched off
        bool aiLocked = false;       // the script drives this actor
        bool deactivated = false;    // hero not available to the player
        int seqBusy = 0;             // sequence actions currently using this actor
        int fxState = -1;            // animated scenery: -1 as placed, 0 off, 1 playing
        bool hasPost = false;        // guard post assigned by the script
        float postX = 0, postY = 0;
        int postDir = -1;
        std::map<int, int> animSwap; // ReplaceAnim
        bool invisible = false;      // script target without a picture (knife-throwing targets...)
        SDL_Rect box{0, 0, 0, 0};    // hit box of scripted objects
    };
    const SpriteFile* sprite(const std::string& folder, const std::string& file);
    void updateFrameRect(Instance& in);
    void setAnim(Instance& in, int anim, int dir);
    void updateMovement(Instance& in, float dt);
    void startTransition(Instance& in, bool toProne);
    static float animSpeed(const SpriteRecord* rec);
    void playOnce(Instance& in, int anim, bool holdLast = false);
    // AI (LevelAI.cpp)
    void initAI();
    void updateAI(float dt);
    void updateEnemy(Instance& e, int idx, float dt);
    void updatePatrol(Instance& e, float dt);
    void beginWaypoint(Instance& e);
    void walkTo(Instance& e, float x, float y, bool run, bool direct);
    bool canSee(const Instance& e, const Instance& hero, bool& nearZone, float& dist) const;
    bool lineOfSight(float ax, float ay, float bx, float by, float targetHeight) const;
    float viewRange(const Instance& e) const;
    void raiseAlarm(Instance& e, int heroIdx, float x, float y);
    void shoot(Instance& e, Instance& hero);
    void hurtHero(Instance& hero, int damage);
    void renderAI(SDL_Renderer* r, const Camera& cam, int sw, int sh);
    void drawCone(SDL_Renderer* r, const Camera& cam, const Instance& e);
    void drawMarker(SDL_Renderer* r, const Camera& cam, const Instance& e);
    void alertTo(Instance& e, int heroIdx, float x, float y);
    void noise(float x, float y, float radius, int heroIdx);
    // combat (LevelCombat.cpp)
    void initHeroes();
    bool actionAvailable(const Instance& h, int action) const;
    int meleeFor(const Instance& h, bool& kills) const;
    void updateHeroOrder(Instance& h, int idx, float dt);
    void applyPending(Instance& in);
    void killEnemy(Instance& e);
    void knockOut(Instance& e);
    void hurtEnemy(Instance& e, int damage, int heroIdx);
    void drawEllipse(SDL_Renderer* r, const Camera& cam, float x, float y, float rx, float ry, SDL_Color c);
    // scripting (LevelScript.cpp)
    struct SeqAction {
        int level = 1, type = 0;
        int32_t a[6] = {0, 0, 0, 0, 0, 0};
        bool started = false, done = false;
        float t = 0;
        int line = -1, voice = 0;  // dialogue playback
    };
    struct Sequence {
        std::vector<SeqAction> acts;
        int level = 1, maxLevel = 1;
    };
    void initScripts();
    void updateScripts(float dt);
    int32_t callScript(int scriptIdx, const char* fn, const std::vector<int32_t>& args, bool* found = nullptr);
    void sendEvent(int elem, int32_t id);
    void record(int type, const int32_t* a, int n);
    bool startAction(SeqAction& s);
    bool updateAction(SeqAction& s, float dt);
    void finishAction(SeqAction& s);
    Instance* actorOf(int32_t handle);
    int elemOf(int32_t handle) const;
    int32_t actorHandle(const Instance& in) const;
    bool locationOf(int32_t handle, float& x, float& y, int& floor);
    int32_t makeLocation(float x, float y, int floor);
    void place(Instance& in, float x, float y, int floor);
    void hide(Instance& in);
    int floorAt(float x, float y, bool upper) const;
    int heroBySub(int sub) const;
    int postureOf(const Instance& in) const;
    void setPosture(Instance& in, int posture);
    void moveActor(Instance& in, float x, float y, bool run);
    void showHint(int idx);
    void updateZones();
    void updateMusic(float dt);

    ScriptFile scriptFile_;
    std::unique_ptr<ScriptVM> vm_;
    std::vector<ScriptInstance> scripts_;  // 0 = the level's StartUp class
    std::vector<int> elemInst_;            // element -> instance (-1 = not shown)
    std::vector<size_t> elemOffsets_;      // element -> offset in the ELEM chunk
    std::vector<int> elemScript_;          // element -> script instance
    std::vector<LevelElement::Kind> elemKind_;
    std::vector<int> elemCls_;
    std::map<std::pair<int, int>, int> waypointScript_;
    std::vector<ScriptZone> zones_;
    std::vector<int> zoneScript_;
    std::vector<std::vector<uint8_t>> zoneInside_;  // per zone: element inside flags
    std::vector<SDL_FPoint> dynLocs_;
    std::vector<int> dynFloors_;
    std::vector<int32_t> thisStack_;
    Dialogues dialogues_;
    std::vector<Sequence> running_;
    Sequence recording_;
    bool isRecording_ = false;
    int userLock_ = 0;
    bool briefing_ = false;
    std::map<int, int32_t> globals_;
    std::map<std::pair<int, int>, int32_t> props_;
    std::map<std::pair<int, int>, bool> actionAvail_;
    std::vector<std::pair<int, bool>> objectives_;
    float hourT_ = 0, victoryT_ = 0, zoneT_ = 0;
    int seconds_ = 0;
    int result_ = 0, defeatText_ = -1;
    bool forceVictory_ = false;
    // camera
    float viewX_ = 0, viewY_ = 0, viewW_ = 800, viewH_ = 600;
    bool camActive_ = false;
    float camX_ = 0, camY_ = 0;
    int camLock_ = -1;
    // overlay
    std::string subtitle_, speaker_;
    float subtitleT_ = 0;
    std::string hint_;
    struct Toast { std::string title, text; float t; };
    std::vector<Toast> toasts_, pendingToasts_;
    float hintT_ = 0;
    // music: calm / suspicious / alarm tracks of the level (MSIC chunk)
    std::string music_[3];
    int musicState_ = -1, musicHandle_ = 0;
    float musicHold_ = 0;

    int number_ = 0;
    LevelFile file_;
    Image16 background_, minimap_;
    TiledImage bgTex_;
    std::vector<Mask> masks_;
    std::vector<SDL_Texture*> maskTex_;
    std::vector<MotionLayer> layers_;
    std::vector<PatrolPath> paths_;
    std::vector<SightObstacle> sight_;
    ProfileTable profiles_;
    WeaponTable weapons_;
    std::vector<NavGrid> nav_;
    std::vector<std::unique_ptr<SpriteFile>> sprites_;
    std::map<std::string, const SpriteFile*> spriteByKey_;
    std::vector<Instance> instances_;
    std::vector<int> drawOrder_;
    SpriteAtlas atlas_;
    int selected_ = -1;
    SDL_FPoint target_{0, 0};
    float targetFade_ = 0;
    float time_ = 0;
};
