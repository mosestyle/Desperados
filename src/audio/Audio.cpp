#include "Audio.h"

#include <algorithm>
#include <cstring>

#include "../core/FileSystem.h"

Audio& Audio::get() {
    static Audio a;
    return a;
}

bool Audio::init() {
    if (dev_) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { SDL_Log("Audio: %s", SDL_GetError()); return false; }
    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = callback;
    want.userdata = this;
    dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, &spec_, 0);
    if (!dev_) { SDL_Log("Audio: cannot open device: %s", SDL_GetError()); return false; }
    SDL_PauseAudioDevice(dev_, 0);
    return true;
}

void Audio::shutdown() {
    if (dev_) SDL_CloseAudioDevice(dev_);
    dev_ = 0;
    channels_.clear();
    cache_.clear();
}

std::shared_ptr<Audio::Clip> Audio::load(const std::string& rel) {
    auto it = cache_.find(rel);
    if (it != cache_.end()) return it->second;
    std::shared_ptr<Clip> clip;
    std::vector<uint8_t> file;
    if (fs_::readData(rel, file)) {
        SDL_AudioSpec src;
        Uint8* buf = nullptr;
        Uint32 len = 0;
        if (SDL_LoadWAV_RW(SDL_RWFromConstMem(file.data(), (int)file.size()), 1, &src, &buf, &len)) {
            SDL_AudioStream* st = SDL_NewAudioStream(src.format, src.channels, src.freq, AUDIO_F32SYS, 2,
                                                     dev_ ? spec_.freq : 44100);
            if (st) {
                SDL_AudioStreamPut(st, buf, (int)len);
                SDL_AudioStreamFlush(st);
                clip = std::make_shared<Clip>();
                clip->pcm.resize(SDL_AudioStreamAvailable(st) / sizeof(float));
                SDL_AudioStreamGet(st, clip->pcm.data(), (int)(clip->pcm.size() * sizeof(float)));
                SDL_FreeAudioStream(st);
            }
            SDL_FreeWAV(buf);
        } else {
            SDL_Log("Audio: %s: %s", rel.c_str(), SDL_GetError());
        }
    }
    cache_[rel] = clip;
    return clip;
}

float Audio::length(const std::string& rel) {
    auto c = load(rel);
    if (!c) return 0;
    return c->pcm.size() / 2.0f / (dev_ ? spec_.freq : 44100);
}

int Audio::play(const std::string& rel, Group g, float volume, bool loop) {
    if (!dev_) return 0;
    auto c = load(rel);
    if (!c || c->pcm.empty()) return 0;
    SDL_LockAudioDevice(dev_);
    Channel ch;
    ch.clip = c;
    ch.vol = volume;
    ch.loop = loop;
    ch.group = g;
    ch.id = nextId_++;
    channels_.push_back(ch);
    SDL_UnlockAudioDevice(dev_);
    return ch.id;
}

void Audio::stop(int handle) {
    if (!dev_) return;
    SDL_LockAudioDevice(dev_);
    channels_.erase(std::remove_if(channels_.begin(), channels_.end(), [&](const Channel& c) { return c.id == handle; }),
                    channels_.end());
    SDL_UnlockAudioDevice(dev_);
}

void Audio::stopGroup(Group g) {
    if (!dev_) return;
    SDL_LockAudioDevice(dev_);
    channels_.erase(std::remove_if(channels_.begin(), channels_.end(), [&](const Channel& c) { return c.group == g; }),
                    channels_.end());
    SDL_UnlockAudioDevice(dev_);
}

bool Audio::playing(int handle) const {
    if (!dev_ || !handle) return false;
    SDL_LockAudioDevice(dev_);
    bool found = false;
    for (const auto& c : channels_) found |= c.id == handle;
    SDL_UnlockAudioDevice(dev_);
    return found;
}

void Audio::pauseAll(bool pause) {
    if (dev_) SDL_PauseAudioDevice(dev_, pause ? 1 : 0);
}

void Audio::callback(void* user, Uint8* stream, int len) {
    static_cast<Audio*>(user)->mix((float*)stream, len / (int)(2 * sizeof(float)));
}

void Audio::mix(float* out, int frames) {
    std::memset(out, 0, frames * 2 * sizeof(float));
    for (auto& c : channels_) {
        const float v = c.vol * groupVol_[c.group];
        const auto& pcm = c.clip->pcm;
        for (int i = 0; i < frames; ++i) {
            if (c.pos + 1 >= pcm.size()) {
                if (!c.loop) break;
                c.pos = 0;
            }
            out[i * 2] += pcm[c.pos] * v;
            out[i * 2 + 1] += pcm[c.pos + 1] * v;
            c.pos += 2;
        }
    }
    channels_.erase(std::remove_if(channels_.begin(), channels_.end(),
                                   [](const Channel& c) { return !c.loop && c.pos + 1 >= c.clip->pcm.size(); }),
                    channels_.end());
    for (int i = 0; i < frames * 2; ++i) out[i] = std::clamp(out[i], -1.0f, 1.0f);
}
