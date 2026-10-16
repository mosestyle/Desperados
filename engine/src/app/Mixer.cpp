// The app's sound mixer: one SDL audio device (22050 Hz stereo, as the game's samples), 24 sound
// channels, a voice channel and the music (a loop and the one queued after it). The WAVs (PCM or
// MS-ADPCM, as the game ships them) are read with SDL, converted once and kept.
#include "app/Mixer.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "sb/SBFile.h"
#include "sb/SBSound.h"

namespace {

constexpr int kRate = 22050;
constexpr int kChannels = 24;

struct Sample {
    std::vector<int16_t> pcm;  // stereo frames, interleaved
    size_t frames() const { return pcm.size() / 2; }
};

struct Channel {
    std::shared_ptr<Sample> s;
    size_t pos = 0;
    float vol = 1, pan = 0;
    bool loop = false;
    bool on = false;
    uint32_t serial = 0;
};

SDL_AudioDeviceID gDev = 0;
std::map<std::string, std::shared_ptr<Sample>> gCache;
Channel gCh[kChannels];
Channel gVoice;
std::shared_ptr<Sample> gMusic, gMusicNext;
size_t gMusicPos = 0;
float gFxVol = 1.0f, gMusicVol = 0.6f, gVoiceVol = 1.0f;
uint32_t gSerial = 1;

std::shared_ptr<Sample> Load(const std::string& path, bool cache = true) {
    if (cache) {
        auto it = gCache.find(path);
        if (it != gCache.end()) return it->second;
    }
    SDL_AudioSpec spec;
    Uint8* buf = nullptr;
    Uint32 len = 0;
    std::shared_ptr<Sample> out;
    if (SDL_LoadWAV(path.c_str(), &spec, &buf, &len)) {
        SDL_AudioCVT cvt;
        if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, 2, kRate) >= 0) {
            std::vector<Uint8> work((size_t)len * (cvt.len_mult > 0 ? cvt.len_mult : 1) + 64);
            std::copy(buf, buf + len, work.begin());
            cvt.buf = work.data();
            cvt.len = (int)len;
            if (cvt.needed == 0 || SDL_ConvertAudio(&cvt) == 0) {
                int bytes = cvt.needed ? cvt.len_cvt : (int)len;
                out = std::make_shared<Sample>();
                out->pcm.resize((size_t)bytes / 2);
                std::copy(work.data(), work.data() + (out->pcm.size() * 2), (Uint8*)out->pcm.data());
            }
        }
        SDL_FreeWAV(buf);
    } else {
        SBLog("sound %s: %s", path.c_str(), SDL_GetError());
    }
    if (cache) gCache[path] = out;
    return out;
}

inline int16_t Clip(int v) { return (int16_t)std::max(-32768, std::min(32767, v)); }

void MixChannel(Channel& c, int* acc, int frames, float master) {
    if (!c.on || !c.s || c.s->frames() == 0) return;
    float l = c.vol * master * (c.pan > 0 ? 1.0f - c.pan : 1.0f);
    float r = c.vol * master * (c.pan < 0 ? 1.0f + c.pan : 1.0f);
    int il = (int)(l * 256.0f), ir = (int)(r * 256.0f);
    const int16_t* p = c.s->pcm.data();
    size_t n = c.s->frames();
    for (int i = 0; i < frames; ++i) {
        if (c.pos >= n) {
            if (!c.loop) {
                c.on = false;
                return;
            }
            c.pos = 0;
        }
        acc[2 * i] += (p[2 * c.pos] * il) >> 8;
        acc[2 * i + 1] += (p[2 * c.pos + 1] * ir) >> 8;
        ++c.pos;
    }
}

void Callback(void*, Uint8* stream, int len) {
    int frames = len / 4;
    static std::vector<int> acc;
    acc.assign((size_t)frames * 2, 0);
    for (Channel& c : gCh) MixChannel(c, acc.data(), frames, gFxVol);
    MixChannel(gVoice, acc.data(), frames, gVoiceVol);
    // the music: the loop, then the queued one without a gap
    int im = (int)(gMusicVol * 256.0f);
    for (int i = 0; i < frames && gMusic; ++i) {
        if (gMusicPos >= gMusic->frames()) {
            gMusic = gMusicNext ? gMusicNext : gMusic;  // (the same again if none is queued)
            gMusicNext = nullptr;
            gMusicPos = 0;
            if (!gMusic || gMusic->frames() == 0) break;
        }
        acc[2 * i] += (gMusic->pcm[2 * gMusicPos] * im) >> 8;
        acc[2 * i + 1] += (gMusic->pcm[2 * gMusicPos + 1] * im) >> 8;
        ++gMusicPos;
    }
    int16_t* out = (int16_t*)stream;
    for (int i = 0; i < frames * 2; ++i) out[i] = Clip(acc[(size_t)i]);
}

// ---- what the engine calls (SBSound) --------------------------------------------------------------

int Play(const std::string& path, float vol, float pan, bool loop) {
    if (!gDev) return -1;
    auto s = Load(path);
    if (!s) return -1;
    SDL_LockAudioDevice(gDev);
    int best = -1;
    for (int i = 0; i < kChannels; ++i)
        if (!gCh[i].on) {
            best = i;
            break;
        }
    if (best < 0) {
        // all busy: the oldest one-shot gives way
        uint32_t oldest = UINT32_MAX;
        for (int i = 0; i < kChannels; ++i)
            if (!gCh[i].loop && gCh[i].serial < oldest) oldest = gCh[i].serial, best = i;
    }
    int id = -1;
    if (best >= 0) {
        Channel& c = gCh[best];
        c.s = s;
        c.pos = 0;
        c.vol = vol;
        c.pan = pan;
        c.loop = loop;
        c.on = true;
        c.serial = gSerial++;
        id = best | (int)((c.serial & 0xffff) << 8);
    }
    SDL_UnlockAudioDevice(gDev);
    return id;
}

Channel* Find(int id) {
    if (id < 0) return nullptr;
    Channel& c = gCh[id & 0xff];
    if ((id & 0xff) >= kChannels || (int)(c.serial & 0xffff) != (id >> 8)) return nullptr;
    return &c;
}

void Set(int id, float vol, float pan) {
    if (!gDev) return;
    SDL_LockAudioDevice(gDev);
    if (Channel* c = Find(id)) c->vol = vol, c->pan = pan;
    SDL_UnlockAudioDevice(gDev);
}

void Stop(int id) {
    if (!gDev) return;
    SDL_LockAudioDevice(gDev);
    if (Channel* c = Find(id)) c->on = false;
    SDL_UnlockAudioDevice(gDev);
}

bool Playing(int id) {
    if (!gDev) return false;
    SDL_LockAudioDevice(gDev);
    Channel* c = Find(id);
    bool on = c && c->on;
    SDL_UnlockAudioDevice(gDev);
    return on;
}

void Voice(const std::string& path) {
    if (!gDev) return;
    auto s = Load(path, false);
    SDL_LockAudioDevice(gDev);
    gVoice.s = s;
    gVoice.pos = 0;
    gVoice.on = s != nullptr;
    gVoice.loop = false;
    gVoice.vol = 1;
    gVoice.pan = 0;
    SDL_UnlockAudioDevice(gDev);
}

void VoiceStop() {
    if (!gDev) return;
    SDL_LockAudioDevice(gDev);
    gVoice.on = false;
    SDL_UnlockAudioDevice(gDev);
}

void MusicQueue(const std::string& path) {
    if (!gDev) return;
    auto s = Load(path, false);  // (the loops are big: read each time)
    SDL_LockAudioDevice(gDev);
    if (!gMusic) {
        gMusic = s;
        gMusicPos = 0;
    } else {
        gMusicNext = s;
    }
    SDL_UnlockAudioDevice(gDev);
}

bool MusicWantsNext() {
    if (!gDev) return false;
    SDL_LockAudioDevice(gDev);
    bool want = !gMusic || !gMusicNext;
    SDL_UnlockAudioDevice(gDev);
    return want;
}

void MusicStop() {
    if (!gDev) return;
    SDL_LockAudioDevice(gDev);
    gMusic = gMusicNext = nullptr;
    gMusicPos = 0;
    SDL_UnlockAudioDevice(gDev);
}

void Volumes(float fx, float music, float voice) {
    if (!gDev) return;
    SDL_LockAudioDevice(gDev);
    gFxVol = fx;
    gMusicVol = music;
    gVoiceVol = voice;
    SDL_UnlockAudioDevice(gDev);
}

}  // namespace

bool MixerOpen() {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = kRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = Callback;
    gDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!gDev) {
        SBLog("audio: %s", SDL_GetError());
        return false;
    }
    SBSound::playVoice = Voice;
    SBSound::stopVoice = VoiceStop;
    SBSound::mixPlay = Play;
    SBSound::mixSet = Set;
    SBSound::mixStop = Stop;
    SBSound::mixPlaying = Playing;
    SBSound::musicQueue = MusicQueue;
    SBSound::musicWantsNext = MusicWantsNext;
    SBSound::musicStop = MusicStop;
    SBSound::setVolumes = Volumes;
    SDL_PauseAudioDevice(gDev, 0);
    return true;
}

void MixerPause(bool pause) {
    if (gDev) SDL_PauseAudioDevice(gDev, pause ? 1 : 0);
}

void MixerStopAll() {
    if (!gDev) return;
    SDL_LockAudioDevice(gDev);
    for (Channel& c : gCh) c.on = false;
    gVoice.on = false;
    gMusic = gMusicNext = nullptr;
    SDL_UnlockAudioDevice(gDev);
}

void MixerFlushCache() {
    if (!gDev) return;
    SDL_LockAudioDevice(gDev);
    for (Channel& c : gCh) c.on = false;
    gVoice.on = false;
    gMusic = gMusicNext = nullptr;
    gCache.clear();
    SDL_UnlockAudioDevice(gDev);
}

void MixerClose() {
    if (gDev) SDL_CloseAudioDevice(gDev);
    gDev = 0;
}
