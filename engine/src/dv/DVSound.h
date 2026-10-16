// DVSound (Game/DVSound.cpp, DVSoundCache.cpp, DVSoundGeometry.cpp, DVSoundSource.cpp): the sound
// of a mission. The FX bank (Data\Sounds\desperados.fxg) names the samples of the sound effects
// the animations' frames ask for (a footstep group has one sample per ground material); the
// sound bank (desperados.sbk) names the ambient samples of the level's sound sources (SND hunk:
// a river, a saloon piano, birds now and then), heard by their distance to the middle of the
// screen; the music (MSIC hunk) is a pool of loops for each mood: quiet, alert and fight.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "sb/SBGeo.h"

class SBFile;

class DVSound {
public:
    static DVSound* mpSound;
    DVSound();
    ~DVSound();

    int LoadSoundFromFile(SBFile& f, uint32_t len);   // FXBK (0x08391320)
    int LoadMusicFromFile(SBFile& f);                 // MSIC (0x083912b0)
    int LoadSoundSourcesFromFile(SBFile& f);          // SND  (0x08391450)

    // a sound effect at a point of the map (DVSound::PlayFx 0x08391230); material: the ground's
    void PlayFx(uint16_t id, const SBGeoPoint2D& at, uint8_t material = 0);
    // the listener (the middle of the screen) and the music's mood, each step (0x08390840)
    void Hourglass(const SBGeoPoint2D& listener, int mood);
    void StopAll();
    // the scripts' ActivateSoundSource / DeactivateSoundSource (by number in the SND hunk)
    void SetSourceActive(unsigned i, bool on);
    void Suspend(bool on);  // the scripts' Suspend / ResumeAllSoundSources
    size_t SourceCount() const { return sources.size(); }
    bool suspended = false;
    bool enabled = true;

private:
    struct Fx {
        uint32_t type = 2;               // 0 a group by material, 1 a group (random), 2 a sample
        std::vector<std::string> files;  // game paths
    };
    struct Source {
        uint32_t id = 0;
        uint8_t u49 = 0, type = 0;       // type 1 a loop, 2 now and then
        uint16_t minDelay = 0, maxDelay = 0, count = 0;
        bool global = false;             // +0x10: heard everywhere
        uint16_t inner = 0, outer = 0;   // +0x12 / +0x14: full volume within, nothing beyond
        std::vector<SBGeoPoint2D> points;
        uint16_t volNear = 255, volFar = 0, u16 = 0;
        uint8_t u3c = 0;
        bool active = true;
        std::string file;
        int channel = -1;
        int wait = 0;                    // ticks to the next one (type 2)
    };
    std::map<uint32_t, Fx> bank;                 // the FX bank by id
    std::map<uint32_t, std::string> sounds;      // the sound bank by id
    std::vector<Source> sources;
    std::vector<std::string> music[3];           // quiet, alert, fight
    int musicMood = -1;
    unsigned musicNext = 0;
    SBGeoPoint2D listener;
    bool LoadBanks();
    bool banksLoaded = false;
    float Volume(float dist, float inner, float outer, float vNear, float vFar) const;
};
