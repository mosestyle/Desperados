// DVSound: see DVSound.h
#include "dv/DVSound.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "sb/SBFile.h"
#include "sb/SBSound.h"

// the sound's own random numbers (the game's are kept for the game: a saved game is replayed)
static unsigned gSoundRand = 12345;
static int SoundRand() {
    gSoundRand = gSoundRand * 1103515245u + 12345u;
    return (int)((gSoundRand >> 16) & 0x7fff);
}

DVSound* DVSound::mpSound = nullptr;

DVSound::DVSound() { mpSound = this; }

DVSound::~DVSound() {
    StopAll();
    if (mpSound == this) mpSound = nullptr;
}

static std::string WithWav(std::string s) {
    s = s.c_str();
    if (s.size() < 4 || s[s.size() - 4] != '.') s += ".wav";
    return s;
}

// DVSoundCache::LoadFXs (0x0838a9c0) and LoadSoundListFromFile (0x0838b700): the two banks
bool DVSound::LoadBanks() {
    if (banksLoaded) return true;
    banksLoaded = true;
    SBFile f;
    if (f.Open("Data\\Sounds\\desperados.fxg", 1) == 0) {
        uint32_t magic = f.U32();
        if (magic != 0x4b425846) {
            SBError(true, "DVSoundCache.cpp", 0x17a, "Invalid FX ID bank file");
        } else {
            f.U32();
            f.U32();
            uint32_t n = f.U32();
            uint32_t i = 0;
            while (i < n && f.Tell() < f.Size()) {
                uint32_t type = f.U32(), id = f.U32();
                f.U16();
                Fx fx;
                fx.type = type;
                if (type < 2) {
                    f.U16();
                    uint32_t k = f.U32();
                    for (uint32_t j = 0; j < k; ++j) {
                        f.U32();
                        f.U32();
                        f.U16();
                        uint16_t l = f.U16();
                        std::string nm(l, '\0');
                        if (l) f.Serialize(&nm[0], l);
                        fx.files.push_back("Data\\Sounds\\" + WithWav(nm));
                    }
                    i += k;
                } else {
                    uint16_t l = f.U16();
                    std::string nm(l, '\0');
                    if (l) f.Serialize(&nm[0], l);
                    fx.files.push_back("Data\\Sounds\\" + WithWav(nm));
                }
                bank[id] = fx;
                ++i;
            }
        }
    } else {
        SBLog("sound: no FX bank");
    }
    SBFile b;
    if (b.Open("Data\\Sounds\\desperados.sbk", 1) == 0) {
        if (b.U32() == 0x4b425344 && b.U32() == 1) {
            uint32_t n = b.U32();
            for (uint32_t i = 0; i < n && b.Tell() < b.Size(); ++i) {
                uint32_t id = b.U32();
                uint16_t l = b.U16();
                std::string nm(l, '\0');
                if (l) b.Serialize(&nm[0], l);
                sounds[id] = "Data\\Sounds\\" + WithWav(nm);
            }
        }
    }
    SBLog("sound: %zu effects, %zu ambient sounds", bank.size(), sounds.size());
    return true;
}

// the level's FX and sound lists (DVSound::LoadSoundFromFile): which of the banks it uses (all of
// them are known here; a sample is read when it first plays)
int DVSound::LoadSoundFromFile(SBFile& f, uint32_t len) {
    (void)len;
    LoadBanks();
    f.version = f.U32();
    if (f.version != 3)
        SBError(true, "DVSound.cpp", 0x64d, "Version Check Failed on %s (file version %u, expected version %u).",
                "Sound filtering chunk", f.version, 3);
    return -1;  // (the rest, the exclamations, comes with the voices of the guards)
}

// DVSoundCache::LoadMusicFromFile (0x0838c910): the loops of the three moods
int DVSound::LoadMusicFromFile(SBFile& f) {
    int start = f.Tell();
    f.version = f.U32();
    if (f.version != 1)
        SBError(true, "DVSoundCache.cpp", 0x3e0, "Version Check Failed on %s (file version %u, expected version %u).",
                "Hunk music", f.version, 1);
    for (auto& pool : music) {
        pool.clear();
        uint16_t n = f.U16();
        for (uint16_t i = 0; i < n; ++i) {
            uint16_t l = f.U16();
            std::string nm(l, '\0');
            if (l) f.Serialize(&nm[0], l);
            pool.push_back("Data\\Musics\\" + std::string(nm.c_str()));
        }
    }
    return f.Tell() - start;
}

// DVSoundSourceManager::LoadSoundSources (0x083960e0) / DVSoundSource::LoadFromFile (0x083954f0)
int DVSound::LoadSoundSourcesFromFile(SBFile& f) {
    LoadBanks();
    int start = f.Tell();
    uint32_t v = f.U32();
    if (v != 7) SBError(true, "DVSoundSourceManager.cpp", 99, "Invalid sound hunk version %d.\n", v);
    uint16_t n = f.U16();
    sources.clear();
    for (uint16_t i = 0; i < n; ++i) {
        Source s;
        s.id = f.U32();
        s.u49 = f.U8();
        s.type = f.U8();
        if (s.type == 2) {
            s.minDelay = f.U16();
            s.maxDelay = f.U16();
            s.count = f.U16();
        }
        s.global = f.U8() != 0;
        if (!s.global) {
            s.inner = f.U16();
            s.outer = f.U16();
            uint16_t k = f.U16();
            for (uint16_t j = 0; j < k; ++j) {
                uint16_t x = f.U16(), y = f.U16();
                s.points.push_back(SBGeoPoint2D((float)x, (float)y));
            }
            s.volNear = std::min<uint16_t>(f.U16(), 0xff);
            s.volFar = std::min<uint16_t>(f.U16(), 0xff);
            s.u16 = f.U16();
        }
        s.u3c = f.U8();
        auto it = sounds.find(s.id);
        if (it != sounds.end()) s.file = it->second;
        s.wait = s.minDelay;
        s.active = s.u49 != 0;  // (the others wait for the scripts' ActivateSoundSource)
        sources.push_back(s);
    }
    return f.Tell() - start;
}

// DVSoundGeometry::GetVolumeForDistance (0x08392b40)
float DVSound::Volume(float dist, float inner, float outer, float vNear, float vFar) const {
    if (dist > outer) return vFar;
    if (dist <= inner) return vNear;
    return vNear + (vFar - vNear) * ((dist - inner) / (outer - inner));
}

// DVSoundGeometry::GetSettingsForLocation (0x08393fd0): full within the inner distance of the
// listener, nothing beyond the outer one, panned to the side it is on
void DVSound::PlayFx(uint16_t id, const SBGeoPoint2D& at, uint8_t material) {
    if (!enabled || !SBSound::mixPlay) return;
    auto it = bank.find(id);
    if (it == bank.end() || it->second.files.empty()) return;
    const Fx& fx = it->second;
    const float inner = 250.0f, outer = 900.0f;
    float dx = at.x - listener.x, dy = (at.y - listener.y) * 1.7434468f;
    float dist = std::sqrt(dx * dx + dy * dy);
    float vol = Volume(dist, inner, outer, 1.0f, 0.0f) * 0.8f;
    if (vol < 0.01f) return;
    float pan = 0;
    float ax = std::fabs(dx);
    if (ax >= inner) pan = ax >= outer ? 1.0f : (ax - inner) / (outer - inner);
    if (dx < 0) pan = -pan;
    pan *= 0.8f;
    size_t k = 0;
    if (fx.type == 0) k = std::min<size_t>(material, fx.files.size() - 1);
    else if (fx.type == 1) k = (size_t)(SoundRand() % (int)fx.files.size());
    SBSound::Play(fx.files[k], vol, pan, false);
}

void DVSound::SetSourceActive(unsigned i, bool on) {
    if (i >= sources.size()) return;
    sources[i].active = on;
    if (!on && sources[i].channel >= 0) {
        SBSound::Stop(sources[i].channel);
        sources[i].channel = -1;
    }
}

void DVSound::Suspend(bool on) {
    suspended = on;
    if (on)
        for (Source& s : sources)
            if (s.channel >= 0) {
                SBSound::Stop(s.channel);
                s.channel = -1;
            }
}

static float DistToPolyline(const std::vector<SBGeoPoint2D>& pts, const SBGeoPoint2D& p) {
    auto sq = [](float x, float y) { return x * x + y * y * 3.04f; };  // (the ground's proportions)
    if (pts.empty()) return 1e9f;
    float best = sq(pts[0].x - p.x, pts[0].y - p.y);
    for (size_t i = 1; i < pts.size(); ++i) {
        SBGeoPoint2D a = pts[i - 1], b = pts[i];
        float vx = b.x - a.x, vy = b.y - a.y;
        float l = vx * vx + vy * vy;
        float t = l > 0 ? ((p.x - a.x) * vx + (p.y - a.y) * vy) / l : 0;
        t = std::max(0.0f, std::min(1.0f, t));
        best = std::min(best, sq(a.x + vx * t - p.x, a.y + vy * t - p.y));
    }
    return std::sqrt(best);
}

void DVSound::Hourglass(const SBGeoPoint2D& l, int mood) {
    listener = l;
    if (!enabled) return;
    // the ambient sounds
    for (Source& s : sources) {
        if (s.file.empty() || suspended) continue;
        float vol = 0, pan = 0;
        if (s.active) {
            if (s.global) {
                vol = 0.6f;
            } else {
                float d = DistToPolyline(s.points, listener);
                vol = Volume(d, (float)s.inner, (float)std::max<uint16_t>(s.outer, s.inner + 1), s.volNear / 255.0f, s.volFar / 255.0f) * 0.7f;
                if (s.points.size() == 1) {
                    float dx = s.points[0].x - listener.x;
                    pan = std::max(-0.6f, std::min(0.6f, dx / 900.0f));
                }
            }
        }
        if (s.type == 0) {
            // once, when the scripts start it
            if (s.active) {
                if (vol >= 0.02f) SBSound::Play(s.file, vol, pan, false);
                s.active = false;
            }
            continue;
        }
        if (s.type == 2) {
            // now and then: after a wait between the two delays (tenths of a second)
            if (--s.wait > 0) continue;
            int span = std::max(1, (int)s.maxDelay - (int)s.minDelay);
            s.wait = ((int)s.minDelay + SoundRand() % span) * 25 / 10 + 1;
            if (vol >= 0.02f) SBSound::Play(s.file, vol, pan, false);
            continue;
        }
        // a loop: on while it is heard
        if (vol >= 0.02f) {
            if (s.channel < 0 || !SBSound::Playing(s.channel)) s.channel = SBSound::Play(s.file, vol, pan, true);
            else SBSound::Set(s.channel, vol, pan);
        } else if (s.channel >= 0) {
            SBSound::Stop(s.channel);
            s.channel = -1;
        }
    }
    // the music: the next loop of the mood (DVSound::SetMusicMode), queued behind the one playing
    if (mood < 0 || mood > 2) mood = 0;
    if (music[mood].empty()) mood = 0;
    if (music[mood].empty()) return;
    if (mood != musicMood) {
        musicMood = mood;
        musicNext = (unsigned)(SoundRand() % (int)music[mood].size());
        SBSound::QueueMusic(music[mood][musicNext % music[mood].size()]);
        return;
    }
    if (SBSound::MusicWantsNext()) {
        musicNext = (musicNext + 1 + (unsigned)(SoundRand() % 2)) % (unsigned)music[mood].size();
        SBSound::QueueMusic(music[mood][musicNext]);
    }
}

void DVSound::StopAll() {
    for (Source& s : sources)
        if (s.channel >= 0) {
            SBSound::Stop(s.channel);
            s.channel = -1;
        }
    SBSound::StopMusic();
    musicMood = -1;
}
