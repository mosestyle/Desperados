#include "sb/SBSound.h"

#include <map>

#include "sb/SBFile.h"

void (*SBSound::playVoice)(const std::string&) = nullptr;
void (*SBSound::stopVoice)() = nullptr;
int (*SBSound::mixPlay)(const std::string&, float, float, bool) = nullptr;
void (*SBSound::mixSet)(int, float, float) = nullptr;
void (*SBSound::mixStop)(int) = nullptr;
bool (*SBSound::mixPlaying)(int) = nullptr;
void (*SBSound::musicQueue)(const std::string&) = nullptr;
bool (*SBSound::musicWantsNext)() = nullptr;
void (*SBSound::musicStop)() = nullptr;
void (*SBSound::setVolumes)(float, float, float) = nullptr;

// the game paths already looked for on disk (the folders are searched without case)
static const std::string& Real(const std::string& gamePath) {
    static std::map<std::string, std::string> known;
    auto it = known.find(gamePath);
    if (it != known.end()) return it->second;
    return known[gamePath] = SBFile::Resolve(gamePath);
}

bool SBSound::muted = false;

void SBSound::PlayVoice(const std::string& gamePath) {
    if (!playVoice || muted) return;
    std::string real = SBFile::Resolve(gamePath);
    if (!real.empty()) playVoice(real);
}

void SBSound::StopVoice() {
    if (stopVoice) stopVoice();
}

int SBSound::Play(const std::string& gamePath, float vol, float pan, bool loop) {
    if (!mixPlay || muted) return -1;
    const std::string& real = Real(gamePath);
    if (real.empty()) return -1;
    return mixPlay(real, vol, pan, loop);
}

void SBSound::Set(int ch, float vol, float pan) {
    if (mixSet && ch >= 0) mixSet(ch, vol, pan);
}

void SBSound::Stop(int ch) {
    if (mixStop && ch >= 0) mixStop(ch);
}

bool SBSound::Playing(int ch) { return mixPlaying && ch >= 0 && mixPlaying(ch); }

void SBSound::QueueMusic(const std::string& gamePath) {
    if (!musicQueue || muted) return;
    const std::string& real = Real(gamePath);
    if (!real.empty()) musicQueue(real);
}

bool SBSound::MusicWantsNext() { return musicWantsNext && musicWantsNext(); }

void SBSound::StopMusic() {
    if (musicStop) musicStop();
}
