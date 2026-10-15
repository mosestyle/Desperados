#include "sb/SBSound.h"

#include "sb/SBFile.h"

void (*SBSound::playVoice)(const std::string&) = nullptr;
void (*SBSound::stopVoice)() = nullptr;

void SBSound::PlayVoice(const std::string& gamePath) {
    if (!playVoice) return;
    std::string real = SBFile::Resolve(gamePath);
    if (!real.empty()) playVoice(real);
}

void SBSound::StopVoice() {
    if (stopVoice) stopVoice();
}
