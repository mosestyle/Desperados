// A loaded mission map: background, placed characters and scenery, depth masks,
// walkable areas and simple hero control (select, walk, run).
#pragma once
#include <SDL.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../formats/Image16.h"
#include "../formats/LevelElements.h"
#include "../formats/LevelFile.h"
#include "../formats/Masks.h"
#include "../formats/MotionAreas.h"
#include "../formats/Paths.h"
#include "../formats/Profiles.h"
#include "../formats/SightObstacles.h"
#include "../formats/SpriteFile.h"
#include "../render/SpriteAtlas.h"
#include "../render/Textures.h"
#include "Camera.h"
#include "NavGrid.h"

class Level {
public:
    struct ActorDot { float x, y; Faction faction; bool selected; };

    ~Level();
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
    void drawSelectedFrame(SDL_Renderer* r, SDL_FRect box, int anim, int dir);
    int firstHero() const;
    bool showMasks = true;
    bool dumpNav(const std::string& bmpPath, int layer) const;  // debug: walkable cells as an image

    // --- enemies (LevelAI.cpp) ---
    int pickEnemy(float wx, float wy, float slack) const;
    void toggleCone(int idx);   // show/hide an enemy's field of view
    void hideAllCones();
    void showAllCones();
    void debugPlaceSelected(float x, float y);
    bool allHeroesDead() const;
    int alertedCount() const;

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
        enum class AI { None, Calm, Suspicious, Alert, Searching, Dead } ai = AI::None;
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
    void drawEllipse(SDL_Renderer* r, const Camera& cam, float x, float y, float rx, float ry, SDL_Color c);

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
