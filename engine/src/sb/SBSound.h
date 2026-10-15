// SBSound: the sounds the engine wants played. The engine only says what (a game path); the app
// plays it (SDL audio on the phone). Without a player (the desktop tools) nothing is heard and the
// engine goes on by the files' lengths.
#pragma once
#include <string>

struct SBSound {
    // the app's player: plays a WAV of the game folder (real path on disk), stops it
    static void (*playVoice)(const std::string& realPath);
    static void (*stopVoice)();
    static void PlayVoice(const std::string& gamePath);
    static void StopVoice();
};
