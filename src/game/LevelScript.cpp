// Mission scripts: runs the level's original compiled scripts (.scb) and implements the
// engine functions ("natives") they call, plus the cutscene sequences they record.
//
// Handles passed to scripts: actors 0x01000000 + element index, locations 0x02000000 + SCRP
// index (0x02FFFFFF = "Honolulu", the place off the map where unused actors wait),
// actor positions taken at run time 0x03000000 + n, sound sources 0x04000000 + n.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "../audio/Audio.h"
#include "../core/FileSystem.h"
#include "../formats/TextResource.h"
#include "../render/BitmapFont.h"
#include "Level.h"

namespace {
constexpr int32_t kActor = 0x01000000, kLoc = 0x02000000, kDyn = 0x03000000, kSound = 0x04000000;
constexpr int32_t kHonolulu = 0x02FFFFFF;
constexpr float kTick = 1.0f / 25.0f;
constexpr float kPi = 3.14159265358979f;
constexpr int kAnimIdle = 0, kAnimProne = 7, kAnimShoot = 28, kAnimDie = 33;

// Sequence action types
enum Act {
    A_Timer, A_CamJump, A_CamScroll, A_CamMove, A_Zoom, A_StartBriefing, A_EndBriefing, A_Dialog, A_Move, A_LeaveGame,
    A_EnterGame, A_TurnTo, A_PlayAnim, A_PlayAnimLoop, A_PlayAnimFreeze, A_LockAI, A_UnlockAI, A_LockUser, A_UnlockUser,
    A_Event, A_ActionAvail, A_CharAvail, A_LockCam, A_ClearCam, A_Speak, A_FireAt, A_FireLoc, A_ReplaceAnim,
    A_RestoreAnim, A_MoveInto, A_Mount, A_Dismount, A_Nothing
};

float groundDist(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = 2.0f * (by - ay);
    return std::sqrt(dx * dx + dy * dy);
}
int dirTowards(float dx, float dy) {
    float a = std::atan2(dx, -2.0f * dy);
    int d = (int)std::lround(a / (kPi / 8.0f));
    return (d % 16 + 16) % 16;
}
float asFloat(int32_t v) { float f; std::memcpy(&f, &v, 4); return f; }
const char* kHeroNames[6] = {"Cooper", "Sam", "Doc", "Kate", "Sanchez", "Mia"};
}  // namespace

static bool scriptLog() { static int on = SDL_getenv("DESP_SCRIPTLOG") ? 1 : 0; return on != 0; }

// ------------------------------------------------------------------------------------------
// handles

int Level::elemOf(int32_t h) const {
    if ((h & 0xFF000000) != kActor) return -1;
    int e = h & 0xFFFFFF;
    return e < (int)elemInst_.size() ? e : -1;
}

Level::Instance* Level::actorOf(int32_t h) {
    int e = elemOf(h);
    if (e < 0 || elemInst_[e] < 0) return nullptr;
    return &instances_[elemInst_[e]];
}

int32_t Level::actorHandle(const Instance& in) const { return in.elem >= 0 ? kActor + in.elem : 0; }

int Level::floorAt(float x, float y, bool upper) const {
    const int n = (int)layers_.size();
    for (int k = 0; k < n; ++k) {
        int l = upper ? n - 1 - k : k;
        if (!layers_[l].liftLayer && layers_[l].walkable(x, y)) return l;
    }
    return 0;
}

bool Level::locationOf(int32_t h, float& x, float& y, int& floor) {
    if (h == kHonolulu || h == 0) return false;
    switch (h & 0xFF000000) {
    case kLoc: {
        int i = h & 0xFFFFFF;
        if (i >= (int)zones_.size()) return false;
        SDL_FPoint c = zones_[i].isPoint() ? SDL_FPoint{(float)zones_[i].pts[0].x, (float)zones_[i].pts[0].y} : zones_[i].centre();
        x = c.x;
        y = c.y;
        floor = zones_[i].flag < (int)layers_.size() && !layers_[zones_[i].flag].liftLayer ? zones_[i].flag : floorAt(x, y, false);
        return true;
    }
    case kDyn: {
        int i = h & 0xFFFFFF;
        if (i >= (int)dynLocs_.size()) return false;
        x = dynLocs_[i].x;
        y = dynLocs_[i].y;
        floor = dynFloors_[i];
        return true;
    }
    case kActor: {
        Instance* in = actorOf(h);
        if (!in || in->hidden) return false;
        x = in->x;
        y = in->y;
        floor = in->floor;
        return true;
    }
    default: return false;
    }
}

int32_t Level::makeLocation(float x, float y, int floor) {
    dynLocs_.push_back({x, y});
    dynFloors_.push_back(floor);
    return kDyn + (int32_t)dynLocs_.size() - 1;
}

int Level::heroBySub(int sub) const {
    for (size_t i = 0; i < instances_.size(); ++i)
        if (instances_[i].el.faction == Faction::Hero && instances_[i].el.sub == sub) return (int)i;
    return -1;
}

void Level::hide(Instance& in) {
    in.hidden = true;
    in.path.clear();
    in.pathIdx = 0;
    if (selected_ >= 0 && &instances_[selected_] == &in) selected_ = firstHero();
}

void Level::place(Instance& in, float x, float y, int floor) {
    in.hidden = false;
    in.x = x;
    in.y = y;
    in.floor = std::clamp(floor, 0, std::max(0, (int)layers_.size() - 1));
    in.path.clear();
    in.pathIdx = 0;
    in.route.clear();
    in.lift = -1;
    if (in.floor < (int)nav_.size() && !nav_[in.floor].walkableAt(x, y)) {
        SDL_FPoint p;
        if (nav_[in.floor].nearestWalkable({x, y}, p, 6)) { in.x = p.x; in.y = p.y; }
    }
    updateFrameRect(in);
    if (in.el.faction == Faction::Hero && selected_ < 0) selected_ = (int)(&in - &instances_[0]);
}

void Level::moveActor(Instance& in, float x, float y, int floor, bool run) {
    in.running = run;
    in.path.clear();
    in.pathIdx = 0;
    in.route.clear();
    if (in.prone && run) startTransition(in, false);
    if (floor != in.floor && routeTo(in, x, y, floor, run)) return;
    if (in.floor < (int)nav_.size() && nav_[in.floor].findPath({in.x, in.y}, {x, y}, in.path) && !in.path.empty()) return;
    in.path.clear();
    in.path.push_back({x, y});
}

int Level::postureOf(const Instance& in) const {
    if (in.ai == Instance::AI::Dead) return 15;
    if (in.ai == Instance::AI::KO) return 6;
    if (in.prone) return 2;
    return 0;
}

void Level::setPosture(Instance& in, int p) {
    switch (p) {
    case 0: in.prone = false; in.holdLast = false; setAnim(in, kAnimIdle, in.dir); break;
    case 2: case 7: in.prone = true; setAnim(in, kAnimProne, in.dir); break;
    case 6: knockOut(in); break;
    case 15: killEnemy(in); break;
    default: break;
    }
}

std::string Level::objectiveText(int i) const {
    if (i < 0 || i >= (int)dialogues_.objectives.size()) return {};
    return gameTexts().line(dialogues_.objectiveId, dialogues_.objectives[i]);
}

std::string Level::missionResultText() const {
    if (result_ == 1) return "MISSION SUCCESSFUL!";
    std::string t;
    if (defeatText_ >= 0 && defeatText_ < (int)dialogues_.defeatTexts.size())
        t = gameTexts().line(dialogues_.defeatId, dialogues_.defeatTexts[defeatText_]);
    return t;
}

void Level::showHint(int idx) {
    if (idx < 0 || idx >= (int)dialogues_.hints.size()) return;
    hint_ = gameTexts().line(dialogues_.hintId, dialogues_.hints[idx]);
    hintT_ = 0;
}

// ------------------------------------------------------------------------------------------
// set-up

void Level::initScripts() {
    char path[64];
    SDL_snprintf(path, sizeof path, "data/levels/level_%02d.scb", number_);
    if (!scriptFile_.load(path) || scriptFile_.classes.empty()) {
        SDL_Log("Level %d: no mission script", number_);
        return;
    }
    vm_ = std::make_unique<ScriptVM>(*this);
    size_t n = 0;
    if (const uint8_t* d = file_.chunkData("DLGS", &n)) dialogues_.parse(d, n);
    if (const uint8_t* d = file_.chunkData("SCRP", &n)) parseScriptZones(d, n, zones_);
    if (const uint8_t* d = file_.chunkData("MSIC", &n)) {  // u32 version, 3 * (u16 present, u16 len, name)
        size_t o = 4;
        for (int i = 0; i < 3 && o + 4 <= n; ++i) {
            int has = d[o] | d[o + 1] << 8, len = d[o + 2] | d[o + 3] << 8;
            o += 4;
            if (o + len > n) break;
            if (has) music_[i] = std::string((const char*)d + o, len);
            o += len;
        }
    }

    auto newScript = [&](const std::string& cls, int32_t owner) -> int {
        const ScriptClass* c = scriptFile_.find(cls);
        if (!c) return -1;
        ScriptInstance si;
        si.cls = c;
        si.vars.assign(c->varBytes + 4, 0);
        si.owner = owner;
        scripts_.push_back(std::move(si));
        return (int)scripts_.size() - 1;
    };
    newScript(scriptFile_.classes[0].name, 0);  // StartUp

    // Element scripts: the class name is stored inside the element's record.
    const uint8_t* elem = file_.chunkData("ELEM", &n);
    elemScript_.assign(elemInst_.size(), -1);
    for (size_t e = 0; elem && e < elemOffsets_.size(); ++e) {
        if (elemCls_[e] != 1 && elemCls_[e] != 8) continue;
        size_t from = elemOffsets_[e], to = e + 1 < elemOffsets_.size() ? elemOffsets_[e + 1] : n;
        for (size_t o = from; o + 2 < to && o + 2 < n; ++o) {
            size_t len = elem[o] | (elem[o + 1] << 8);
            if (len < 4 || len > 64 || o + 2 + len > n) continue;
            std::string name((const char*)elem + o + 2, len);
            if (!scriptFile_.find(name)) continue;
            int s = newScript(name, kActor + (int32_t)e);
            elemScript_[e] = s;
            if (elemInst_[e] >= 0) instances_[elemInst_[e]].script = s;
            break;
        }
    }
    for (size_t p = 0; p < paths_.size(); ++p)
        for (size_t w = 0; w < paths_[p].size(); ++w)
            if (!paths_[p][w].script.empty()) {
                int s = newScript(paths_[p][w].script, 0);
                if (s >= 0) waypointScript_[{(int)p, (int)w}] = s;
            }
    zoneScript_.assign(zones_.size(), -1);
    zoneInside_.assign(zones_.size(), {});
    for (size_t z = 0; z < zones_.size(); ++z)
        if (!zones_[z].script.empty()) zoneScript_[z] = newScript(zones_[z].script, kLoc + (int32_t)z);

    // the scripts take over: no more "untouched start position" grace for heroes
    for (auto& in : instances_) in.untouched = false;

    SDL_Log("Level %d: %d script classes, %d instances, %d locations/sectors, %d dialogues", number_,
            (int)scriptFile_.classes.size(), (int)scripts_.size(), (int)zones_.size(), (int)dialogues_.dialogues.size());

    // Order of the original: element scripts first (created with the elements), then the level.
    for (size_t s = 1; s < scripts_.size(); ++s) callScript((int)s, "Initialize", {});
    callScript(0, "Initialize", {});
    callScript(0, "Briefing", {});
    if (selected_ < 0 || instances_[selected_].hidden) selected_ = firstHero();
}

int32_t Level::callScript(int s, const char* fn, const std::vector<int32_t>& args, bool* found) {
    if (!vm_ || s < 0 || s >= (int)scripts_.size()) { if (found) *found = false; return 0; }
    thisStack_.push_back(scripts_[s].owner);
    int32_t r = vm_->call(scripts_[s], fn, args, found);
    thisStack_.pop_back();
    return r;
}

void Level::sendEvent(int e, int32_t id) {
    if (scriptLog()) SDL_Log("[%5.1f] event %d -> element %d (%s)", time_, id, e,
                             e >= 0 && e < (int)elemScript_.size() && elemScript_[e] >= 0 ? scripts_[elemScript_[e]].cls->name.c_str() : "-");
    if (e < 0 || e >= (int)elemScript_.size() || elemScript_[e] < 0) {
        SDL_Log("script: custom event %d sent to element %d without a script", id, e);
        return;
    }
    callScript(elemScript_[e], "FilterEvent", {kActor + e, id});
}

// ------------------------------------------------------------------------------------------
// sequences

void Level::record(int type, const int32_t* a, int n) {
    SeqAction s;
    s.type = type;
    for (int i = 0; i < n && i < 6; ++i) s.a[i] = a[i];
    if (!isRecording_) {  // recorded outside Start()/Thanx(): play it on its own
        Sequence q;
        q.acts.push_back(s);
        running_.push_back(std::move(q));
        return;
    }
    s.level = recording_.maxLevel;
    recording_.acts.push_back(s);
}

bool Level::startAction(SeqAction& s) {
    s.started = true;
    Instance* in = actorOf(s.a[0]);
    float x, y;
    int fl;
    switch (s.type) {
    case A_Timer: s.t = s.a[0] * kTick; return false;
    case A_CamJump:
        if (locationOf(s.a[0], x, y, fl)) { camActive_ = true; camX_ = x; camY_ = y; }
        return true;
    case A_CamScroll: case A_CamMove:
        if (!locationOf(s.a[0], x, y, fl)) return true;
        if (!camActive_) { camX_ = viewX_; camY_ = viewY_; }
        camActive_ = true;
        s.t = 0;
        return false;
    case A_StartBriefing: briefing_ = true; return true;
    case A_EndBriefing:
        briefing_ = false;
        return true;
    case A_Dialog: {
        int d = s.a[0];
        if (d < 0 || d >= (int)dialogues_.dialogues.size() || dialogues_.dialogues[d].empty()) return true;
        s.line = -1;
        s.t = 0;
        return false;
    }
    case A_Move: case A_LeaveGame: case A_MoveInto: {
        if (!in || in->hidden) return true;
        if (!locationOf(s.a[1], x, y, fl)) { if (s.type != A_Move) hide(*in); return true; }
        in->seqBusy++;
        moveActor(*in, x, y, fl, s.a[2] != 0);
        s.t = 0;
        return false;
    }
    case A_EnterGame: {  // actor, from, to, style
        if (!in) return true;
        if (locationOf(s.a[1], x, y, fl)) place(*in, x, y, fl);
        if (!locationOf(s.a[2], x, y, fl)) return true;
        in->seqBusy++;
        moveActor(*in, x, y, fl, s.a[3] != 0);
        return false;
    }
    case A_TurnTo:
        if (in && !in->hidden && locationOf(s.a[1], x, y, fl) && (x != in->x || y != in->y)) {
            in->path.clear();
            setAnim(*in, in->prone ? kAnimProne : kAnimIdle, dirTowards(x - in->x, y - in->y));
            updateFrameRect(*in);
        }
        s.t = 0.15f;
        return false;
    case A_PlayAnim: case A_PlayAnimFreeze: {
        if (!in || in->hidden) return true;
        int anim = s.a[1];
        auto sw = in->animSwap.find(anim);
        if (sw != in->animSwap.end()) anim = sw->second;
        in->path.clear();
        if (!in->set->find(anim, in->dir)) return true;
        in->seqBusy++;
        playOnce(*in, anim, s.type == A_PlayAnimFreeze);
        updateFrameRect(*in);
        return false;
    }
    case A_PlayAnimLoop:
        if (in && !in->hidden) { in->path.clear(); in->transition = -1; in->holdLast = false; setAnim(*in, s.a[1], in->dir); updateFrameRect(*in); }
        return true;
    case A_LockAI: if (in) in->aiLocked = true; return true;
    case A_UnlockAI: if (in) in->aiLocked = false; return true;
    case A_LockUser: userLock_ = 1; return true;
    case A_UnlockUser: userLock_ = 0; return true;
    case A_Event: sendEvent(elemOf(s.a[0]), s.a[1]); return true;
    case A_ActionAvail: actionAvail_[{elemOf(s.a[0]), s.a[1]}] = s.a[2] != 0; return true;
    case A_CharAvail: if (in) in->deactivated = s.a[1] == 0; return true;
    case A_LockCam: camLock_ = elemOf(s.a[0]); return true;
    case A_ClearCam: camLock_ = -1; camActive_ = false; return true;
    case A_FireAt: case A_FireLoc: {
        if (!in || in->hidden) return true;
        if (locationOf(s.a[1], x, y, fl)) setAnim(*in, kAnimIdle, dirTowards(x - in->x, y - in->y));
        in->path.clear();
        in->seqBusy++;
        playOnce(*in, in->prone ? 8 : kAnimShoot);
        return false;
    }
    case A_ReplaceAnim: if (in) in->animSwap[s.a[1]] = s.a[2]; return true;
    case A_RestoreAnim: if (in) in->animSwap.erase(s.a[1]); return true;
    default: return true;
    }
}

// Returns true when the action has finished.
bool Level::updateAction(SeqAction& s, float dt) {
    Instance* in = actorOf(s.a[0]);
    switch (s.type) {
    case A_Timer: s.t -= dt; return s.t <= 0;
    case A_CamScroll: case A_CamMove: {
        float x, y;
        int fl;
        if (!locationOf(s.a[0], x, y, fl)) return true;
        // speed: scripts pass world pixels per tick-ish values; a gentle pan reads best on a phone
        float speed = s.type == A_CamMove ? std::max(60.0f, s.a[1] * 6.0f) : 700.0f;
        float dx = x - camX_, dy = y - camY_, d = std::sqrt(dx * dx + dy * dy);
        float step = speed * dt;
        if (d <= step) { camX_ = x; camY_ = y; return true; }
        camX_ += dx / d * step;
        camY_ += dy / d * step;
        return false;
    }
    case A_Dialog: {
        const auto& lines = dialogues_.dialogues[s.a[0]];
        if (s.line < 0 && scriptLog()) SDL_Log("[%5.1f] dialogue %d (%d lines)", time_, s.a[0], (int)lines.size());
        s.t -= dt;
        bool lineOver = s.line < 0 || (s.t <= 0 && !Audio::get().playing(s.voice));
        if (!lineOver) return false;
        if (s.voice) Audio::get().stop(s.voice);
        s.voice = 0;
        if (++s.line >= (int)lines.size()) { subtitle_.clear(); return true; }
        const DialogueLine& l = lines[s.line];
        subtitle_ = gameTexts().line(dialogues_.textId, l.text);
        speaker_ = l.speaker >= 0 && l.speaker < 6 ? kHeroNames[l.speaker] : "";
        std::string wav = gameTexts().line(dialogues_.waveId, l.wave);
        for (auto& c : wav) if (c == '\\') c = '/';
        float len = 0;
        if (!wav.empty()) {
            std::string rel = gameInterfaceFolder() + wav;
            len = Audio::get().length(rel);
            s.voice = Audio::get().play(rel, Audio::Voice);
        }
        // without a voice, give the reader time for the words
        s.t = len > 0 ? len + 0.25f : 1.2f + subtitle_.size() * 0.055f;
        subtitleT_ = s.t;
        return false;
    }
    case A_Move: case A_LeaveGame: case A_EnterGame: case A_MoveInto:
        if (!in) return true;
        s.t += dt;
        if (!in->path.empty() && s.t < 60.0f) return false;
        if (s.type == A_LeaveGame || s.type == A_MoveInto) hide(*in);
        return true;
    case A_TurnTo: s.t -= dt; return s.t <= 0;
    case A_PlayAnim: case A_PlayAnimFreeze: case A_FireAt: case A_FireLoc:
        return !in || in->transition < 0;
    default: return true;
    }
}

void Level::finishAction(SeqAction& s) {
    s.done = true;
    if (s.type == A_Move || s.type == A_LeaveGame || s.type == A_EnterGame || s.type == A_MoveInto ||
        s.type == A_PlayAnim || s.type == A_PlayAnimFreeze || s.type == A_FireAt || s.type == A_FireLoc) {
        if (Instance* in = actorOf(s.a[0])) in->seqBusy = std::max(0, in->seqBusy - 1);
    }
    if (s.type == A_Dialog && s.voice) { Audio::get().stop(s.voice); s.voice = 0; }
}

void Level::stopVoices() { Audio::get().stopGroup(Audio::Voice); }

void Level::stopMusic() {
    if (musicHandle_) Audio::get().stop(musicHandle_);
    musicHandle_ = 0;
    musicState_ = -1;
}

// The original switches between a calm, a tense and an alarm track depending on the enemies.
void Level::updateMusic(float dt) {
    int want = 0;
    for (const auto& i : instances_) {
        if (i.hidden || i.el.faction != Faction::Enemy) continue;
        if (i.ai == Instance::AI::Alert) { want = 2; break; }
        if (i.ai == Instance::AI::Suspicious || i.ai == Instance::AI::Searching) want = 1;
    }
    if (want < musicState_) {  // calm down only after a while
        musicHold_ += dt;
        if (musicHold_ < 6.0f) return;
    }
    musicHold_ = 0;
    if (want == musicState_ && Audio::get().playing(musicHandle_)) return;
    if (music_[want].empty()) return;
    if (musicHandle_) Audio::get().stop(musicHandle_);
    musicHandle_ = Audio::get().play("data/musics/" + music_[want], Audio::Music, 1.0f, true);
    musicState_ = want;
}

// ------------------------------------------------------------------------------------------
// per-frame

void Level::updateZones() {
    for (size_t z = 0; z < zones_.size(); ++z) {
        if (zoneScript_[z] < 0) continue;
        auto& inside = zoneInside_[z];
        inside.resize(elemInst_.size(), 0);
        for (size_t e = 0; e < elemInst_.size(); ++e) {
            if (elemInst_[e] < 0) continue;
            const Instance& in = instances_[elemInst_[e]];
            if (in.el.kind != LevelElement::Actor) continue;
            const bool now = !in.hidden && zones_[z].contains(in.x, in.y);
            if (now == (inside[e] != 0)) continue;
            inside[e] = now;
            if (scriptLog() && in.el.faction == Faction::Hero)
                SDL_Log("[%5.1f] %s %s %s", time_, in.el.set.c_str(), now ? "enters" : "leaves", zones_[z].script.c_str());
            callScript(zoneScript_[z], now ? "EnterZone" : "ExitZone", {kActor + (int32_t)e});
        }
    }
}

void Level::updateScripts(float dt) {
    if (!vm_) return;
    // sequences: actions of the same level run together, levels one after the other
    for (size_t qi = 0; qi < running_.size(); ++qi) {
        for (int guard = 0; guard < 64; ++guard) {
            bool any = false, allDone = true;
            for (size_t i = 0; i < running_[qi].acts.size(); ++i) {
                if (running_[qi].acts[i].level != running_[qi].level) continue;
                any = true;
                if (running_[qi].acts[i].done) continue;
                // work on a copy: an action may run script events that start new sequences
                SeqAction act = running_[qi].acts[i];
                bool done = act.started ? updateAction(act, dt) : startAction(act);
                if (done) finishAction(act);
                running_[qi].acts[i] = act;
                if (!act.done) allDone = false;
            }
            if (any && !allDone) break;
            if (++running_[qi].level > running_[qi].maxLevel) break;
        }
    }
    running_.erase(std::remove_if(running_.begin(), running_.end(), [](const Sequence& q) { return q.level > q.maxLevel; }),
                   running_.end());

    if (camLock_ >= 0 && camLock_ < (int)elemInst_.size() && elemInst_[camLock_] >= 0) {
        const Instance& in = instances_[elemInst_[camLock_]];
        camActive_ = true;
        camX_ = in.x;
        camY_ = in.y;
    } else if (camActive_) {
        bool cameraAction = false;
        for (const auto& q : running_)
            for (const auto& a : q.acts)
                if (a.level == q.level && !a.done && (a.type == A_CamMove || a.type == A_CamScroll)) cameraAction = true;
        // keep the scripted view while the cutscene runs; give the camera back afterwards
        if (!cameraAction && userLock_ == 0) camActive_ = false;
    }

    updateMusic(dt);
    // engine events
    zoneT_ -= dt;
    if (zoneT_ <= 0) { zoneT_ = 0.2f; updateZones(); }
    hourT_ += dt;
    while (hourT_ >= 1.0f) {
        hourT_ -= 1.0f;
        ++seconds_;
        callScript(0, "HourGlass", {seconds_});
        if (result_ == 0 && (seconds_ % 3 == 0 || forceVictory_)) {
            forceVictory_ = false;
            int r = callScript(0, "CheckVictoryCondition", {seconds_});
            if (r == 1 || r == 2) { result_ = r; SDL_Log("Mission %s", r == 1 ? "accomplished" : "failed"); }
        }
        if (scriptLog() && seconds_ % 5 == 0) {
            std::string vis;
            for (const auto& i : instances_)
                if (i.el.faction == Faction::Hero) vis += i.el.set + (i.hidden ? "(hidden) " : "@" + std::to_string((int)i.x) + "," + std::to_string((int)i.y) + " ");
            SDL_Log("[%5.1f] t=%ds lock=%d briefing=%d seqs=%d heroes: %s", time_, seconds_, userLock_, (int)briefing_, (int)running_.size(), vis.c_str());
        }
    }
    if (subtitleT_ > 0) subtitleT_ -= dt;
    if (userLock_ == 0 && !pendingToasts_.empty()) {  // objectives are announced once the cutscene is over
        for (auto& t : pendingToasts_) toasts_.push_back(t);
        pendingToasts_.clear();
    }
    if (!hint_.empty()) { hintT_ += dt; if (hintT_ > 14.0f) hint_.clear(); }
    for (auto& t : toasts_) t.t -= dt;
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [](const Toast& t) { return t.t <= 0; }), toasts_.end());
}

bool Level::scriptCamera(float& cx, float& cy) const {
    if (!camActive_) return false;
    cx = camX_;
    cy = camY_;
    return true;
}

// ------------------------------------------------------------------------------------------
// natives

int32_t Level::native(int id, const int32_t* a, int n) {
    auto arg = [&](int i) -> int32_t { return i < n ? a[i] : 0; };
    Instance* in = actorOf(arg(0));
    float x = 0, y = 0;
    int fl = 0;
    switch (id) {
    case 0x00: record(A_Dialog, a, n); return 1;                      // StartDialog
    case 0x01: case 0x02: case 0x03:                                   // camera scroll / jump
        if (locationOf(arg(0), x, y, fl)) { camActive_ = true; camX_ = x; camY_ = y; }
        return 1;
    case 0x04: case 0x05: case 0x07: case 0x0d: case 0x40: return 1;  // zoom, map, console, freeze
    case 0x06: SDL_Log("script console: %d", arg(0)); return 1;
    case 0x08: return arg(0) == arg(1);                                // IsActorEqual
    case 0x09: case 0x0a: {                                            // IsAny{Civilian,Enemy}Dead
        Faction f = id == 0x09 ? Faction::Civilian : Faction::Enemy;
        for (const auto& i : instances_) if (i.el.faction == f && i.ai == Instance::AI::Dead) return 1;
        return 0;
    }
    case 0x0b: case 0x0c: {                                            // overall alert
        int best = 0;
        for (const auto& i : instances_) {
            if (i.hidden || i.el.faction != (id == 0x0b ? Faction::Enemy : Faction::Civilian)) continue;
            if (i.ai == Instance::AI::Alert) best = std::max(best, 2);
            else if (i.ai == Instance::AI::Suspicious || i.ai == Instance::AI::Searching) best = std::max(best, 1);
        }
        return best;
    }
    case 0x0e:                                                         // InflictPain(actor, amount, ?)
        if (in && in->ai != Instance::AI::Dead) {
            if (in->el.faction == Faction::Hero) hurtHero(*in, arg(1));
            else { in->health -= arg(1); if (in->health <= 0) killEnemy(*in); }
        }
        return 1;
    case 0x0f: return (int32_t)elemInst_.size();
    case 0x10: return arg(0) >= 0 && arg(0) < (int)elemInst_.size() ? kActor + arg(0) : 0;   // GetActor
    case 0x11:                                                                               // GetLocation
        if (arg(0) < 0) return kHonolulu;
        return arg(0) < (int)zones_.size() ? kLoc + arg(0) : kHonolulu;
    case 0x12: { int e = elemOf(arg(0)); return e >= 0 && elemCls_[e] == 16; }               // IsActorAnimation
    case 0x13: { int e = elemOf(arg(0)); return e >= 0 && elemCls_[e] == 8; }                // IsActorObject
    case 0x14: { int e = elemOf(arg(0)); return e >= 0 && elemCls_[e] <= 1; }                // IsActorCharacter
    case 0x15: { int e = elemOf(arg(0)); return e >= 0 && elemCls_[e] == 0; }                // IsActorPC
    case 0x16: { int e = elemOf(arg(0)); return e >= 0 && elemCls_[e] == 1; }                // IsActorNPC
    case 0x17: return in && in->el.faction == Faction::Enemy;
    case 0x18: return in && in->el.faction == Faction::Civilian;
    case 0x19: { int e = elemOf(arg(0)); return e >= 0 && elemCls_[e] == 2; }                // IsActorAnimal
    case 0x1a: return arg(0) == 0;                                                           // IsNull
    case 0x1b: return in ? postureOf(*in) : -1;
    case 0x1c: if (in) setPosture(*in, arg(1)); return 1;
    case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21: case 0x22: {                      // GetCooper..GetMia
        static const int sub[6] = {1, 3, 2, 4, 5, 6};
        int h = heroBySub(sub[id - 0x1d]);
        return h >= 0 ? actorHandle(instances_[h]) : 0;
    }
    case 0x23:                                                                               // GetActorLocation
        if (in && !in->hidden) return makeLocation(in->x, in->y, in->floor);
        return kHonolulu;
    case 0x24:                                                                               // SetActorLocation
        if (!in) return 0;
        if (locationOf(arg(1), x, y, fl)) place(*in, x, y, fl);
        else hide(*in);
        return 1;
    case 0x25: {                                                                             // IsInside(actor, sector)
        if (!in || in->hidden) return 0;
        int z = (arg(1) & 0xFF000000) == kLoc ? (arg(1) & 0xFFFFFF) : -1;
        return z >= 0 && z < (int)zones_.size() && zones_[z].contains(in->x, in->y);
    }
    case 0x26: return 0;                                                                     // IsInsideBuilding
    case 0x27: {                                                                             // GetAnyActorInside
        int z = (arg(0) & 0xFF000000) == kLoc ? (arg(0) & 0xFFFFFF) : -1;
        if (z < 0 || z >= (int)zones_.size()) return 0;
        for (const auto& i : instances_)
            if (!i.hidden && i.el.kind == LevelElement::Actor && zones_[z].contains(i.x, i.y)) return actorHandle(i);
        return 0;
    }
    case 0x28: case 0x29: return 0;                                                          // SwitchProfile, MovementStyle
    case 0x2a: if (in) { in->deactivated = true; if (selected_ >= 0 && &instances_[selected_] == in) selected_ = -1; } return 1;
    case 0x2b: if (in) in->deactivated = false; if (selected_ < 0) selected_ = firstHero(); return 1;
    case 0x2c: actionAvail_[{elemOf(arg(0)), arg(1)}] = arg(2) != 0; return 1;              // SetActionAvailable
    case 0x2d: { auto it = actionAvail_.find({elemOf(arg(0)), arg(1)}); return it == actionAvail_.end() || it->second; }
    case 0x2e: props_[{elemOf(arg(0)), arg(1)}] = arg(2); return 1;                         // SetPersistentProperty
    case 0x2f: { auto it = props_.find({elemOf(arg(0)), arg(1)}); return it == props_.end() ? 0 : it->second; }
    case 0x30:                                                                               // SetAIAlertStatus
        if (in && in->ai != Instance::AI::Dead && in->ai != Instance::AI::KO && in->ai != Instance::AI::None) {
            if (arg(1) == 0) { in->ai = Instance::AI::Calm; in->meter = 0; in->atWaypoint = false; }
            else if (arg(1) == 1) { in->ai = Instance::AI::Suspicious; in->meter = 0.5f; }
        }
        return 1;
    case 0x31:                                                                               // GetAIAlertStatus
        if (!in) return 0;
        if (in->ai == Instance::AI::Alert) return 2;
        return in->ai == Instance::AI::Suspicious || in->ai == Instance::AI::Searching ? 1 : 0;
    case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37: return 0;             // AI state/attitude/level/cone
    case 0x38: case 0x39:                                                                    // StareActor / StareLocation
        if (in && locationOf(arg(1), x, y, fl) && (x != in->x || y != in->y)) setAnim(*in, kAnimIdle, dirTowards(x - in->x, y - in->y));
        return 1;
    case 0x3a:                                                                               // AssignPath(actor, path)
        if (in && arg(1) >= 0 && arg(1) < (int)paths_.size() && !paths_[arg(1)].empty()) {
            in->pathId = arg(1);
            in->hasPost = false;
            float best = 1e30f;
            for (size_t i = 0; i < paths_[in->pathId].size(); ++i) {
                float d = groundDist(in->x, in->y, (float)paths_[in->pathId][i].x, (float)paths_[in->pathId][i].y);
                if (d < best) { best = d; in->wpIdx = (int)i; }
            }
            in->atWaypoint = false;
            if (in->ai == Instance::AI::None && in->el.faction != Faction::Hero) in->ai = Instance::AI::Calm;
        }
        return 1;
    case 0x3b:                                                                               // AssignPost(actor, loc, dir)
        if (in && locationOf(arg(1), x, y, fl)) {
            in->pathId = -1;
            in->hasPost = true;
            in->postX = x;
            in->postY = y;
            in->postDir = arg(2) & 15;
            if (in->ai == Instance::AI::None && in->el.faction != Faction::Hero) in->ai = Instance::AI::Calm;
        }
        return 1;
    case 0x3c: if (in) { in->aiLocked = true; if (in->ai != Instance::AI::Dead) in->path.clear(); } return 1;
    case 0x3d: if (in) in->aiLocked = false; return 1;
    case 0x3e: return 1;                                                                     // ForceBattleDecision
    case 0x3f: if (locationOf(arg(0), x, y, fl)) noise(x, y, (float)std::max(60, arg(1)), -1); return 1;
    case 0x41: case 0x42: return 0;                                                          // rider / horse
    case 0x43: return in && in->fxState != 0 && !in->hidden;                                // IsAnimationActive
    case 0x44:                                                                               // SetAnimationState
        if (in && in->el.kind == LevelElement::Scenery) {
            in->fxState = arg(1) != 0;
            in->hidden = arg(1) == 0;
            if (arg(1) != 0) { in->entry = 0; in->ticks = 0; updateFrameRect(*in); }
        }
        return 1;
    case 0x45: return 0;                                                                     // IsPatchApplied
    case 0x46: case 0xb2: return 1;                                                          // Apply/ResetPatch
    case 0x47: return 0;
    case 0x48: case 0x4b:                                                                    // start recording
        recording_ = Sequence();
        isRecording_ = true;
        return 1;
    case 0x49: case 0x4c:                                                                    // launch
        if (scriptLog()) {
            std::string d;
            for (const auto& act : recording_.acts) d += std::to_string(act.level) + ":" + std::to_string(act.type) + " ";
            SDL_Log("[%5.1f] sequence %s", time_, d.c_str());
        }
        if (isRecording_ && !recording_.acts.empty()) running_.push_back(recording_);
        recording_ = Sequence();
        isRecording_ = false;
        return 1;
    case 0x4a: case 0x4d: if (isRecording_) ++recording_.maxLevel; return recording_.maxLevel;  // Then
    case 0x4e: record(A_CamScroll, a, n); return 1;
    case 0x4f: record(A_CamJump, a, n); return 1;
    case 0x50: case 0x51: return 1;                                                          // RecordSetZoom, DisplayMap
    case 0x52: record(A_StartBriefing, a, n); return 1;
    case 0x53: record(A_EndBriefing, a, n); return 1;
    case 0x54: record(A_ActionAvail, a, n); return 1;
    case 0x55: record(A_CharAvail, a, n); return 1;
    case 0x56: record(A_LockCam, a, n); return 1;
    case 0x57: record(A_ClearCam, a, n); return 1;
    case 0x58: record(A_Dialog, a, n); return 1;
    case 0x59: record(A_Move, a, n); return 1;
    case 0x5a: case 0xc3: record(A_LeaveGame, a, n); return 1;
    case 0x5b: record(A_TurnTo, a, n); return 1;
    case 0x5c: case 0xa1: record(A_Mount, a, n); return 1;
    case 0x5d: record(A_Dismount, a, n); return 1;
    case 0x5e: record(A_FireAt, a, n); return 1;
    case 0xb0: record(A_FireLoc, a, n); return 1;
    case 0x5f: record(A_PlayAnim, a, n); return 1;
    case 0x60: record(A_PlayAnimLoop, a, n); return 1;
    case 0x61: record(A_PlayAnimFreeze, a, n); return 1;
    case 0x62: record(A_LockAI, a, n); return 1;
    case 0x63: record(A_UnlockAI, a, n); return 1;
    case 0x64: record(A_LockUser, a, n); return 1;
    case 0x65: record(A_UnlockUser, a, n); return 1;
    case 0x66: record(A_Timer, a, n); return 1;
    case 0x69: record(A_Event, a, n); return 1;
    case 0x83: record(A_ReplaceAnim, a, n); return 1;
    case 0x84: record(A_RestoreAnim, a, n); return 1;
    case 0x98: record(A_MoveInto, a, n); return 1;
    case 0x99: record(A_CamMove, a, n); return 1;
    case 0x9c: record(A_EnterGame, a, n); return 1;
    case 0xaf: case 0xba: record(A_Speak, a, n); return 1;
    case 0x67: case 0x68: case 0x6a: case 0x97: case 0x9a: case 0x9b: case 0x9d: case 0xa4: case 0xa7: case 0xa8: case 0xbd:
        record(A_Nothing, a, n);
        return 1;
    case 0x6b: if (!globals_.count(arg(0))) globals_[arg(0)] = arg(1); return 1;            // InitGlobal
    case 0x6c: globals_[arg(0)] = arg(1); return 1;                                         // SetGlobal
    case 0x6d: { auto it = globals_.find(arg(0)); return it == globals_.end() ? 0 : it->second; }
    case 0x6e: case 0x6f: return 1;                                                          // suspend/resume sounds
    case 0x70: return kSound + arg(0);                                                       // GetSoundSource
    case 0x71: case 0x72: case 0x73: return 1;
    case 0x74: case 0x75: case 0x76: case 0x77: return 1;                                    // building/zone bookkeeping
    case 0x78: case 0x79: case 0x7a: case 0xa9: case 0xbe: case 0xc2: return id == 0xc2 ? 0 : 1;  // doors
    case 0x7b: return thisStack_.empty() ? 0 : thisStack_.back();                            // This
    case 0x7c: return in ? in->dir : 0;
    case 0x7d: if (in && !in->hidden) { setAnim(*in, in->prone ? kAnimProne : in->anim < 0 ? kAnimIdle : in->anim, arg(1) & 15); updateFrameRect(*in); } return 1;
    case 0x7e: if (in) { in->path.clear(); in->pathIdx = 0; setAnim(*in, in->prone ? kAnimProne : kAnimIdle, in->dir); } return 1;
    case 0x7f: {                                                                             // GetDistance
        float x2, y2;
        int f2;
        if (!locationOf(arg(0), x, y, fl) || !locationOf(arg(1), x2, y2, f2)) return 100000;
        return (int32_t)groundDist(x, y, x2, y2);
    }
    case 0x80: return 0;                                                                     // GetCurrentAction
    case 0x81: { int e = elemOf(arg(0)); return e >= 0 && elemCls_[e] == 4; }               // IsActorCart
    case 0x82: if (in) in->running = arg(1) != 0; return 1;                                  // SetPathWalkingStyle
    case 0x85: {                                                                             // Sees(npc, actor)
        Instance* other = actorOf(arg(1));
        if (!in || !other || in->hidden || other->hidden) return 0;
        bool nearZone = false;
        float d = 0;
        bool wasUntouched = other->untouched;
        other->untouched = false;
        bool r = canSee(*in, *other, nearZone, d);
        other->untouched = wasUntouched;
        return r;
    }
    case 0x86: return elemOf(arg(0));                                                        // GetActorIndex
    case 0x87: case 0x88: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x91: return 1;  // armies
    case 0x89: case 0x8e: case 0x8f: case 0x90: case 0x92: case 0x93: case 0x94: return 0;
    case 0x95: case 0x96: return 1;                                                          // snake / watch
    case 0x9e: case 0x9f: case 0xa2: case 0xa3: case 0xa5: case 0xa6: case 0xac: case 0xae: case 0xbc: case 0xc4:
    case 0xc6: case 0xc7: case 0xb7: case 0xb8: case 0xb9: case 0xc0: case 0xc1: case 0xbf: case 0xad: case 0xb3:
        return 1;
    case 0xa0:                                                                               // ResetAnim
        if (in) { in->transition = -1; in->holdLast = false; in->anim = -1; setAnim(*in, in->prone ? kAnimProne : kAnimIdle, in->dir); }
        return 1;
    case 0xaa: showHint(arg(0)); return 1;
    case 0xab: hint_.clear(); return 1;
    case 0xb1: return arg(0) > 0 ? std::rand() % arg(0) : 0;                                // Rand
    case 0xb4: {                                                                             // AddSentence
        for (auto& o : objectives_) if (o.first == arg(0)) return 1;
        objectives_.push_back({arg(0), false});
        pendingToasts_.push_back({"New objective", objectiveText(arg(0)), 7.0f});
        return 1;
    }
    case 0xb5:                                                                               // DoneSentence
        for (auto& o : objectives_) if (o.first == arg(0)) o.second = true;
        return 1;
    case 0xb6: defeatText_ = arg(0); return 1;                                               // ChooseVictoryDefeatText
    case 0xbb: {                                                                             // SelectPC(hero number)
        int h = in ? (int)(in - &instances_[0]) : heroBySub(arg(0));
        if (h >= 0 && instances_[h].el.faction == Faction::Hero && !instances_[h].hidden) selected_ = h;
        return 1;
    }
    case 0xc5: forceVictory_ = true; return 1;                                               // ForceCheckVictory
    default:
        SDL_Log("script: native %#x not implemented", id);
        return 0;
    }
    (void)asFloat;
}

// ------------------------------------------------------------------------------------------
// overlay

void Level::renderOverlay(SDL_Renderer* r, int sw, int sh, float ui) {
    BitmapFont& font = uiFont();
    if (!font.ready()) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const float scale = std::max(1.0f, std::round(ui * 1.6f * 2) / 2);
    const float lineH = font.height() * scale * 1.05f;

    // cutscene letterbox
    if (cinematic()) {
        float bar = sh * 0.07f;
        SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
        SDL_FRect top{0, 0, (float)sw, bar}, bottom{0, sh - bar, (float)sw, bar};
        SDL_RenderFillRectF(r, &top);
        SDL_RenderFillRectF(r, &bottom);
    }

    auto box = [&](const std::vector<std::string>& lines, const std::string& title, float cx, float y, float maxW,
                   SDL_Color tint, bool bottomAnchor) {
        float w = 0;
        for (const auto& l : lines) w = std::max(w, font.width(l, scale));
        if (!title.empty()) w = std::max(w, font.width(title, scale));
        const float pad = 10 * ui, rows = lines.size() + (title.empty() ? 0 : 1);
        const float h = rows * lineH + pad * 2;
        if (bottomAnchor) y -= h;
        SDL_FRect bg{cx - w / 2 - pad * 1.5f, y, w + pad * 3, h};
        SDL_SetRenderDrawColor(r, 18, 11, 6, 200);
        SDL_RenderFillRectF(r, &bg);
        SDL_SetRenderDrawColor(r, 150, 110, 60, 220);
        SDL_RenderDrawRectF(r, &bg);
        float ty = y + pad;
        if (!title.empty()) {
            font.draw(r, title, cx - font.width(title, scale) / 2, ty, scale, {255, 200, 120, 255});
            ty += lineH;
        }
        for (const auto& l : lines) {
            font.draw(r, l, cx - font.width(l, scale) / 2, ty, scale, tint);
            ty += lineH;
        }
        (void)maxW;
        return h;
    };

    // subtitles of the dialogue being spoken
    if (!subtitle_.empty()) {
        const float maxW = sw * 0.7f;
        box(font.wrap(subtitle_, maxW, scale), speaker_, sw * 0.5f, sh - sh * 0.09f, maxW, {255, 245, 220, 255}, true);
    }
    // tutorial hint
    float y = sh * 0.09f;
    if (!hint_.empty() && subtitle_.empty()) {
        const float maxW = sw * 0.55f;
        y += box(font.wrap(hint_, maxW, scale), "", sw * 0.42f, y, maxW, {240, 230, 170, 255}, false) + 6 * ui;
    }
    // objective messages
    for (const auto& t : toasts_) {
        const float maxW = sw * 0.55f;
        SDL_Color c{255, 245, 220, (Uint8)std::clamp(t.t * 255.0f, 0.0f, 255.0f)};
        y += box(font.wrap(t.text, maxW, scale), t.title, sw * 0.42f, y, maxW, c, false) + 6 * ui;
    }
}
