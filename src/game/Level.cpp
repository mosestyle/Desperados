#include "Level.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "../core/FileSystem.h"

static constexpr float kTicksPerSecond = 25.0f;  // original engine rate
static constexpr float kPi = 3.14159265358979f;
static constexpr int kAnimIdle = 0, kAnimWalk = 3, kAnimRun = 5, kAnimGetDown = 6, kAnimProne = 7, kAnimCrawl = 9,
                     kAnimGetUp = 10;

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

// 16 facing directions: 0 = up (north), clockwise. Screen y is squashed 2:1 on the ground.
static int directionOf(float dx, float dy) {
    float a = std::atan2(dx, -2.0f * dy);
    int d = (int)std::lround(a / (kPi / 8.0f));
    return (d % 16 + 16) % 16;
}

Level::~Level() { releaseTextures(); }

const SpriteFile* Level::sprite(const std::string& folder, const std::string& file) {
    const std::string key = folder + "/" + lower(file);
    auto it = spriteByKey_.find(key);
    if (it != spriteByKey_.end()) return it->second;
    const SpriteFile* result = nullptr;
    std::string path = fs_::resolve("data/" + folder + "/" + file + ".dvf");
    if (!path.empty()) {
        auto sf = std::make_unique<SpriteFile>();
        if (sf->load(path)) {
            sf->setId((int)sprites_.size() + 1);
            result = sf.get();
            sprites_.push_back(std::move(sf));
        }
    } else {
        SDL_Log("Missing sprite file %s", key.c_str());
    }
    spriteByKey_[key] = result;
    return result;
}

bool Level::load(SDL_Renderer* r, int number) {
    number_ = number;
    char path[64];
    uint32_t t0 = SDL_GetTicks();

    SDL_snprintf(path, sizeof path, "data/levels/level_%02d.dvm", number);
    std::vector<Image16> imgs;
    if (!loadImageContainer(path, imgs) || imgs.empty()) return false;
    background_ = std::move(imgs[0]);

    SDL_snprintf(path, sizeof path, "data/levels/level_%02d.dvd", number);
    if (file_.load(path)) {
        std::string name;
        file_.readMinimap(name, minimap_);
        size_t n = 0;
        if (const uint8_t* m = file_.chunkData("MASK", &n))
            if (!parseMasks(m, n, masks_)) SDL_Log("Level %d: mask data only partly read", number);
        if (const uint8_t* m = file_.chunkData("MOVE", &n))
            if (!parseMotionAreas(m, n, layers_)) SDL_Log("Level %d: motion areas only partly read", number);
        nav_.resize(layers_.size());
        for (size_t i = 0; i < layers_.size(); ++i) nav_[i].build(layers_[i], background_.w, background_.h);

        SpriteIndex index;
        index.build();
        if (const uint8_t* e = file_.chunkData("ELEM", &n)) {
            for (auto& el : scanElements(e, n, index)) {
                if (el.kind != LevelElement::Actor && el.kind != LevelElement::Scenery) continue;
                Instance in;
                in.el = el;
                in.file = sprite(el.folder, el.file);
                if (!in.file) continue;
                in.set = in.file->set(el.set);
                if (!in.set && !in.file->sets().empty()) in.set = &in.file->sets().front();
                if (!in.set) continue;
                if (el.kind == LevelElement::Actor) {
                    in.dir = in.set->dirCount >= 16 ? el.dir : 0;
                    in.rec = in.set->find(kAnimIdle, in.dir);
                    if (!in.rec) continue;
                    // stored position = top-left of the anchor box; the anchor point is the feet
                    in.x = (float)(el.x + in.rec->anchorX);
                    in.y = (float)(el.y + in.rec->anchorY);
                    in.floor = 0;
                    for (size_t l = 0; l < layers_.size(); ++l)
                        if (layers_[l].walkable(in.x, in.y)) { in.floor = (int)l; break; }
                } else {
                    in.rec = in.set->records.empty() ? nullptr : &in.set->records.front();
                }
                if (!in.rec || in.rec->entries.empty()) continue;
                // desynchronise identical animations (crowds, rivers...)
                in.entry = (int)(((el.x * 7 + el.y * 13) % (int)in.rec->entries.size() + (int)in.rec->entries.size()) %
                                 (int)in.rec->entries.size());
                in.ticks = (float)(((el.x + el.y) % 7 + 7) % 7);
                updateFrameRect(in);
                instances_.push_back(std::move(in));
            }
        }
    }
    recreateTextures(r);
    selected_ = firstHero();
    SDL_Log("Level %d: %dx%d, %d elements (%d actors), %d sprite files, %d masks, %d layers, loaded in %u ms", number,
            background_.w, background_.h, (int)instances_.size(), actorCount(), (int)sprites_.size(),
            (int)masks_.size(), (int)layers_.size(), SDL_GetTicks() - t0);
    return true;
}

int Level::actorCount() const {
    int n = 0;
    for (const auto& in : instances_) n += in.el.kind == LevelElement::Actor;
    return n;
}

int Level::firstHero() const {
    // prefer a hero standing on walkable ground (some start off-map for their entrance)
    int any = -1;
    for (size_t i = 0; i < instances_.size(); ++i) {
        const auto& in = instances_[i];
        if (in.el.faction != Faction::Hero) continue;
        if (in.x >= 0 && in.y >= 0 && in.x < width() && in.y < height()) return (int)i;
        if (any < 0) any = (int)i;
    }
    return any;
}

void Level::actorDots(std::vector<ActorDot>& out) const {
    out.clear();
    for (size_t i = 0; i < instances_.size(); ++i) {
        const auto& in = instances_[i];
        if (in.el.kind != LevelElement::Actor || in.el.faction == Faction::None) continue;
        out.push_back({in.x, in.y, in.el.faction, (int)i == selected_});
    }
}

void Level::releaseTextures() {
    bgTex_.destroy();
    for (auto* t : maskTex_) if (t) SDL_DestroyTexture(t);
    maskTex_.clear();
    atlas_.clear();
}

void Level::recreateTextures(SDL_Renderer* r) {
    releaseTextures();
    atlas_.setRenderer(r);
    bgTex_.create(r, background_);
    // Each mask becomes a texture holding the background pixels it covers.
    maskTex_.assign(masks_.size(), nullptr);
    std::vector<uint32_t> px;
    for (size_t i = 0; i < masks_.size(); ++i) {
        const Mask& m = masks_[i];
        if (m.line.empty()) continue;
        px.assign((size_t)m.rect.w * m.rect.h, 0);
        for (int y = 0; y < m.rect.h; ++y) {
            int wy = m.rect.y + y;
            if (wy < 0 || wy >= background_.h) continue;
            for (int x = 0; x < m.rect.w; ++x) {
                int wx = m.rect.x + x;
                if (wx < 0 || wx >= background_.w || !m.bits[(size_t)y * m.rect.w + x]) continue;
                uint16_t p = background_.px[(size_t)wy * background_.w + wx];
                uint32_t R = ((p >> 11) & 31) * 255 / 31, G = ((p >> 5) & 63) * 255 / 63, B = (p & 31) * 255 / 31;
                px[(size_t)y * m.rect.w + x] = 0xFF000000u | (R << 16) | (G << 8) | B;
            }
        }
        SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, m.rect.w, m.rect.h);
        if (!t) continue;
        SDL_UpdateTexture(t, nullptr, px.data(), m.rect.w * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        maskTex_[i] = t;
    }
}

void Level::updateFrameRect(Instance& in) {
    const SpriteEntry& e = in.rec->entries[in.entry];
    const auto& frames = in.file->frames();
    int w = e.frame < frames.size() ? frames[e.frame].w : 0, h = e.frame < frames.size() ? frames[e.frame].h : 0;
    if (in.el.kind == LevelElement::Actor) {
        in.world = {(int)std::lround(in.x) - in.rec->anchorX + e.x, (int)std::lround(in.y) - in.rec->anchorY + e.y, w, h};
        in.sortY = (int)std::lround(in.y);
    } else {
        in.world = {in.el.x + e.x, in.el.y + e.y, w, h};
        in.sortY = in.el.y + in.el.z;
        in.x = (float)(in.world.x + w / 2);
        in.y = (float)in.sortY;
    }
}

void Level::setAnim(Instance& in, int anim, int dir) {
    if (in.set->dirCount < 16) dir = 0;
    if (anim == in.anim && dir == in.dir && in.rec) return;
    const SpriteRecord* rec = in.set->find(anim, dir);
    if (!rec || rec->entries.empty()) rec = in.set->find(kAnimIdle, dir);
    if (!rec || rec->entries.empty()) return;
    const bool sameAnim = anim == in.anim;
    in.anim = anim;
    in.dir = dir;
    in.rec = rec;
    if (!sameAnim) { in.entry = 0; in.ticks = 0; in.stepAcc = 0; }
    in.entry %= (int)rec->entries.size();
}

// Ground speed of a movement animation: the original moves `step` px per frame and shows
// each frame for `duration` ticks (0 = one tick), at 25 ticks per second.
float Level::animSpeed(const SpriteRecord* rec) {
    if (!rec || rec->entries.empty()) return 50.0f;
    int steps = 0, ticks = 0;
    for (const auto& e : rec->entries) { steps += std::max<int>(0, e.step); ticks += std::max<int>(1, e.duration); }
    float v = steps > 0 ? steps * kTicksPerSecond / (float)ticks : 50.0f;
    return std::clamp(v, 10.0f, 400.0f);
}

void Level::startTransition(Instance& in, bool toProne) {
    if (in.prone == toProne) return;
    in.prone = toProne;
    const int anim = toProne ? kAnimGetDown : kAnimGetUp;
    const SpriteRecord* rec = in.set->find(anim, in.dir);
    if (!rec || rec->entries.empty() || rec->anim != anim) {  // no animation: switch directly
        setAnim(in, toProne ? kAnimProne : kAnimIdle, in.dir);
        return;
    }
    in.transition = anim;
    in.anim = anim;
    in.rec = rec;
    in.entry = 0;
    in.ticks = 0;
}

void Level::updateMovement(Instance& in, float dt) {
    const int moveAnim = in.prone ? kAnimCrawl : (in.running ? kAnimRun : kAnimWalk);
    float budget = animSpeed(in.set->find(moveAnim, in.dir)) * dt;  // ground distance this frame
    while (budget > 0 && in.pathIdx < in.path.size()) {
        const SDL_FPoint wp = in.path[in.pathIdx];
        float dx = wp.x - in.x, dy = wp.y - in.y;
        float len = std::sqrt(dx * dx + 4 * dy * dy);
        if (len < 0.01f) { ++in.pathIdx; continue; }
        setAnim(in, moveAnim, directionOf(dx, dy));
        float move = std::min(budget, len);
        in.x += dx * move / len;
        in.y += dy * move / len;
        budget -= move;
        in.stepAcc += move;
        if (move >= len) ++in.pathIdx;
    }
    // Walk/run frames have no duration and advance by distance (feet don't slide);
    // crawl frames have durations and advance by time.
    const auto& entries = in.rec->entries;
    if (entries[in.entry].duration > 0) {
        in.ticks += dt * kTicksPerSecond;
        for (int guard = 0; guard < 32; ++guard) {
            float dur = (float)std::max<int>(1, entries[in.entry].duration);
            if (in.ticks < dur) break;
            in.ticks -= dur;
            in.entry = (in.entry + 1) % (int)entries.size();
        }
        in.stepAcc = 0;
    } else {
        for (int guard = 0; guard < 32; ++guard) {
            int step = std::max<int>(1, entries[in.entry].step);
            if (in.stepAcc < step) break;
            in.stepAcc -= step;
            in.entry = (in.entry + 1) % (int)entries.size();
        }
    }
    if (in.pathIdx >= in.path.size()) {
        in.path.clear();
        in.pathIdx = 0;
        setAnim(in, in.prone ? kAnimProne : kAnimIdle, in.dir);
    }
}

void Level::update(float dt) {
    time_ += dt;
    targetFade_ = std::max(0.0f, targetFade_ - dt * 1.2f);
    const float ticks = dt * kTicksPerSecond;
    for (auto& in : instances_) {
        if (in.transition >= 0) {  // get down / stand up: play once, then continue
            const auto& entries = in.rec->entries;
            in.ticks += ticks;
            bool done = false;
            for (int guard = 0; guard < 16; ++guard) {
                float dur = (float)std::max<int>(1, entries[in.entry].duration);
                if (in.ticks < dur) break;
                in.ticks -= dur;
                if (in.entry + 1 >= (int)entries.size()) { done = true; break; }
                ++in.entry;
            }
            if (done) {
                in.transition = -1;
                in.anim = -1;
                setAnim(in, in.prone ? kAnimProne : kAnimIdle, in.dir);
            }
            updateFrameRect(in);
            continue;
        }
        if (!in.path.empty()) {
            updateMovement(in, dt);
            updateFrameRect(in);
            continue;
        }
        const auto& entries = in.rec->entries;
        if (entries.size() < 2) continue;
        in.ticks += ticks;
        bool changed = false;
        for (int guard = 0; guard < 64; ++guard) {
            float dur = (float)std::max<int>(1, entries[in.entry].duration);
            if (in.ticks < dur) break;
            in.ticks -= dur;
            in.entry = (in.entry + 1) % (int)entries.size();
            changed = true;
        }
        if (changed) updateFrameRect(in);
    }
}

int Level::pickHero(float wx, float wy, float slack) const {
    int best = -1;
    float bestD = 1e30f;
    for (size_t i = 0; i < instances_.size(); ++i) {
        const auto& in = instances_[i];
        if (in.el.faction != Faction::Hero) continue;
        const SDL_Rect& w = in.world;
        if (wx < w.x - slack || wx > w.x + w.w + slack || wy < w.y - slack || wy > w.y + w.h + slack) continue;
        float cx = in.x, cy = in.y - w.h * 0.4f;  // roughly the body centre
        float d = (wx - cx) * (wx - cx) + (wy - cy) * (wy - cy);
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    return best;
}

bool Level::selectedPosition(float& x, float& y) const {
    if (selected_ < 0 || selected_ >= (int)instances_.size()) return false;
    x = instances_[selected_].x;
    y = instances_[selected_].y;
    return true;
}

bool Level::moveSelected(float wx, float wy, bool run) {
    if (selected_ < 0 || selected_ >= (int)instances_.size()) return false;
    Instance& in = instances_[selected_];
    if (in.floor >= (int)nav_.size()) return false;
    std::vector<SDL_FPoint> path;
    if (!nav_[in.floor].findPath({in.x, in.y}, {wx, wy}, path) || path.empty()) return false;
    in.path = std::move(path);
    in.pathIdx = 0;
    in.running = run;
    if (run && in.prone) startTransition(in, false);  // running means standing up first
    target_ = in.path.back();
    targetFade_ = 1.0f;
    return true;
}

void Level::toggleStanceSelected() {
    if (selected_ < 0 || selected_ >= (int)instances_.size()) return;
    Instance& in = instances_[selected_];
    if (in.transition >= 0) return;
    in.running = false;
    startTransition(in, !in.prone);
}

bool Level::selectedProne() const {
    return selected_ >= 0 && selected_ < (int)instances_.size() && instances_[selected_].prone;
}

void Level::drawSelectedFrame(SDL_Renderer* r, SDL_FRect box, int anim, int dir) {
    if (selected_ < 0 || selected_ >= (int)instances_.size()) return;
    const Instance& in = instances_[selected_];
    const SpriteRecord* rec = in.set->find(anim, dir);
    if (!rec || rec->entries.empty()) return;
    const SpriteEntry& e = rec->entries[rec->entries.size() - 1];
    const SpriteAtlas::Slot* slot = atlas_.get(*in.file, e.frame);
    if (!slot) return;
    float s = std::min(box.w / slot->rc.w, box.h / slot->rc.h);
    SDL_FRect dst{box.x + (box.w - slot->rc.w * s) / 2, box.y + (box.h - slot->rc.h * s) / 2, slot->rc.w * s, slot->rc.h * s};
    SDL_RenderCopyF(r, slot->tex, &slot->rc, &dst);
}

void Level::setSelectedRunning(bool run) {
    if (selected_ >= 0 && selected_ < (int)instances_.size() && !instances_[selected_].path.empty())
        instances_[selected_].running = run;
}

void Level::drawEllipse(SDL_Renderer* r, const Camera& cam, float x, float y, float rx, float ry, SDL_Color c) {
    SDL_FPoint pts[33];
    for (int t = 0; t < 2; ++t) {  // two rings for a thicker line
        float k = 1.0f + t * 0.06f;
        for (int i = 0; i <= 32; ++i) {
            float a = (float)i / 32 * 2 * kPi;
            pts[i] = {cam.toScreenX(x + std::cos(a) * rx * k), cam.toScreenY(y + std::sin(a) * ry * k)};
        }
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_RenderDrawLinesF(r, pts, 33);
    }
}

void Level::render(SDL_Renderer* r, const Camera& cam, int sw, int sh) {
    bgTex_.draw(r, cam.originX(), cam.originY(), cam.zoom, sw, sh);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // ground markers: walk target and selection ring (drawn under the sprites)
    if (targetFade_ > 0) {
        Uint8 a = (Uint8)(220 * targetFade_);
        float s = 6 + 10 * (1 - targetFade_);
        drawEllipse(r, cam, target_.x, target_.y, s, s * 0.5f, {120, 255, 120, a});
    }
    if (selected_ >= 0 && selected_ < (int)instances_.size()) {
        const auto& in = instances_[selected_];
        float pulse = 0.85f + 0.15f * std::sin(time_ * 5);
        drawEllipse(r, cam, in.x, in.y, 15 * pulse, 7.5f * pulse, {90, 255, 90, 230});
    }

    // painter's order: things whose base is further up the screen are drawn first
    drawOrder_.resize(instances_.size());
    for (size_t i = 0; i < instances_.size(); ++i) drawOrder_[i] = (int)i;
    std::stable_sort(drawOrder_.begin(), drawOrder_.end(),
                     [&](int a, int b) { return instances_[a].sortY < instances_[b].sortY; });

    const float vx0 = cam.toWorldX(0), vy0 = cam.toWorldY(0), vx1 = cam.toWorldX((float)sw), vy1 = cam.toWorldY((float)sh);
    for (int idx : drawOrder_) {
        Instance& in = instances_[idx];
        const SDL_Rect& w = in.world;
        if (w.w <= 0 || w.x > vx1 || w.y > vy1 || w.x + w.w < vx0 || w.y + w.h < vy0) continue;
        const SpriteAtlas::Slot* slot = atlas_.get(*in.file, in.rec->entries[in.entry].frame);
        if (!slot) continue;
        SDL_FRect dst{cam.toScreenX((float)w.x), cam.toScreenY((float)w.y), w.w * cam.zoom, w.h * cam.zoom};
        SDL_RenderCopyF(r, slot->tex, &slot->rc, &dst);

        if (!showMasks) continue;
        // Redraw scenery that stands in front of this sprite, clipped to the sprite.
        SDL_Rect clip{(int)std::floor(dst.x), (int)std::floor(dst.y), (int)std::ceil(dst.w) + 1, (int)std::ceil(dst.h) + 1};
        bool clipped = false;
        for (size_t m = 0; m < masks_.size(); ++m) {
            const Mask& mk = masks_[m];
            if (!maskTex_[m] || mk.group != in.floor) continue;
            const SDL_Rect& mr = mk.rect;
            if (mr.x >= w.x + w.w || mr.y >= w.y + w.h || mr.x + mr.w <= w.x || mr.y + mr.h <= w.y) continue;
            if (in.y >= mk.lineY(in.x)) continue;  // sprite is in front of this mask
            if (!clipped) { SDL_RenderSetClipRect(r, &clip); clipped = true; }
            SDL_FRect md{cam.toScreenX((float)mr.x), cam.toScreenY((float)mr.y), mr.w * cam.zoom, mr.h * cam.zoom};
            SDL_RenderCopyF(r, maskTex_[m], nullptr, &md);
        }
        if (clipped) SDL_RenderSetClipRect(r, nullptr);
    }
}

bool Level::dumpNav(const std::string& path, int layer) const {
    if (layer < 0 || layer >= (int)nav_.size()) return false;
    const NavGrid& g = nav_[layer];
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, background_.w, background_.h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return false;
    uint32_t* px = (uint32_t*)s->pixels;
    for (int y = 0; y < background_.h; ++y)
        for (int x = 0; x < background_.w; ++x) {
            uint16_t p = background_.px[(size_t)y * background_.w + x];
            uint32_t R = ((p >> 11) & 31) * 255 / 31, G = ((p >> 5) & 63) * 255 / 63, B = (p & 31) * 255 / 31;
            bool blocked = g.blockedCell(x / NavGrid::kCell, y / NavGrid::kCell);
            if (blocked) { R = R / 3 + 150; G /= 3; B /= 3; }
            px[(size_t)y * (s->pitch / 4) + x] = 0xFF000000u | (R << 16) | (G << 8) | B;
        }
    int rc = SDL_SaveBMP(s, path.c_str());
    SDL_FreeSurface(s);
    return rc == 0;
}
