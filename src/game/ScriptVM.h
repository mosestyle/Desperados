// Virtual machine for the original mission scripts (port of VMCore from the 2018 build).
//
// Every scripted thing (the level's StartUp class, NPCs, objects, waypoints, zones) owns a
// ScriptInstance: a class plus its variable block. The engine calls event functions on it
// (Initialize, Briefing, HourGlass, FilterEvent, EnterZone, ReachPoint, ...); they run to
// completion immediately. Anything that takes time is recorded as a sequence through
// native calls and played by the level afterwards.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "../formats/ScriptFile.h"

class ScriptHost {
public:
    virtual ~ScriptHost() = default;
    // Native function `id` (0..199) with its arguments (ints, or float bit patterns).
    virtual int32_t native(int id, const int32_t* args, int count) = 0;
};

struct ScriptInstance {
    const ScriptClass* cls = nullptr;
    std::vector<uint8_t> vars;
    int owner = -1;  // handle of the thing that owns the script ("This()")
};

class ScriptVM {
public:
    explicit ScriptVM(ScriptHost& host) : host_(host) {}
    // Runs `fn` if the class has it. Returns the function's return value (0 when it has none).
    int32_t call(ScriptInstance& inst, const char* fn, const std::vector<int32_t>& args = {}, bool* found = nullptr);
    int depth() const { return depth_; }

private:
    struct Frame {
        std::vector<uint8_t> params, vol, tmp;
        int returnTo = -1;
        int32_t ret = 0;
    };
    uint8_t* sym(ScriptInstance& inst, Frame& f, uint16_t s);
    ScriptHost& host_;
    std::vector<uint8_t> globals_ = std::vector<uint8_t>(4096, 0);
    int depth_ = 0;  // nested engine->script calls (a native may trigger another event)
};
