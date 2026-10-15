// DVScript (Game/DVScript.cpp, VMCoreCustom): the mission's scripts. At the start of a mission
// Data\Levels\level_XX.scb is loaded, StartUp and every scripted element get their instance and
// their Initialize is run (DVGame::GameLoop: DVScript::Initialize). The natives they call act on
// the engine; the actors are handed to the scripts as handles (their index in the level's
// element list + 1, the original's DVScript::marrayElement; 0 = none).
//
// This first part runs the Initialize functions with the natives that set the mission up (who is
// playable, whose brain waits for the story, the view cones); HourGlass, the events, the
// sequences (IStart / IRecord... / IThanx), the dialogues and the objectives come next.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "dv/VMCore.h"

class DVElement;
class DVEngine;

class DVScript {
public:
    static DVScript* mpInstance;
    DVScript();
    ~DVScript();
    // the level's scripts: loads them, gives StartUp and the elements their instance, runs the
    // Initialize functions
    bool Initialize(DVEngine* engine, const std::vector<DVElement*>& elements, const std::string& levelName);

    static int32_t Handle(const DVElement* e);
    static DVElement* Element(int32_t h);

    VMCore vm;
    VMCodeManager code;
    std::vector<DVElement*> marrayElement;
    std::unique_ptr<VMInstance> startUp;
    std::vector<std::unique_ptr<VMInstance>> instances;
};
