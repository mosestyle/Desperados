// SBSound: the sounds the engine wants played. The engine only says what (a game path) and how
// loud / where; the app mixes them (SDL audio on the phone). Without a player (the desktop tools)
// nothing is heard and the engine goes on by the files' lengths.
#pragma once
#include <string>

struct SBSound {
    // ---- the app's player (set by the app; null in the tools) ----------------------------------
    static void (*playVoice)(const std::string& realPath);
    static void (*stopVoice)();
    // a sample on a free channel: its number (-1 if it can't); vol 0..1, pan -1 (left)..1
    static int (*mixPlay)(const std::string& realPath, float vol, float pan, bool loop);
    static void (*mixSet)(int channel, float vol, float pan);
    static void (*mixStop)(int channel);
    static bool (*mixPlaying)(int channel);
    // the music: a loop is queued after the one that plays; true when it wants the next one
    static void (*musicQueue)(const std::string& realPath);
    static bool (*musicWantsNext)();
    static void (*musicStop)();
    static void (*setVolumes)(float fx, float music, float voice);

    // ---- the engine's side: game paths ----------------------------------------------------------
    static bool muted;  // (a saved game played again in a moment: nothing is heard)
    static void PlayVoice(const std::string& gamePath);
    static void StopVoice();
    static int Play(const std::string& gamePath, float vol, float pan, bool loop);
    static void Set(int channel, float vol, float pan);
    static void Stop(int channel);
    static bool Playing(int channel);
    static void QueueMusic(const std::string& gamePath);
    static bool MusicWantsNext();
    static void StopMusic();
};
