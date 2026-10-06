// Small software mixer on top of SDL audio: voices, sound effects and music.
// Sounds are decoded (SDL_LoadWAV handles the game's PCM and MS-ADPCM files) and converted
// to the device format once, then cached.
#pragma once
#include <SDL.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

class Audio {
public:
    enum Group { Voice, Effect, Music, GroupCount };
    static Audio& get();
    bool init();
    void shutdown();
    // Plays a game file (path relative to the data folder). Returns a handle (0 = failed).
    int play(const std::string& relativePath, Group g, float volume = 1.0f, bool loop = false);
    void stop(int handle);
    void stopGroup(Group g);
    bool playing(int handle) const;
    float length(const std::string& relativePath);  // seconds, 0 if missing
    void setGroupVolume(Group g, float v) { groupVol_[g] = v; }
    void pauseAll(bool pause);

private:
    struct Clip { std::vector<float> pcm; };  // stereo interleaved, device rate
    struct Channel { std::shared_ptr<Clip> clip; size_t pos = 0; float vol = 1; bool loop = false; Group group = Effect; int id = 0; };
    std::shared_ptr<Clip> load(const std::string& relativePath);
    static void callback(void* user, Uint8* stream, int len);
    void mix(float* out, int frames);

    SDL_AudioDeviceID dev_ = 0;
    SDL_AudioSpec spec_{};
    std::map<std::string, std::shared_ptr<Clip>> cache_;
    std::vector<Channel> channels_;
    float groupVol_[GroupCount] = {1.0f, 0.8f, 0.55f};
    int nextId_ = 1;
};
