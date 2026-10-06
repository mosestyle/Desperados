#include "Level.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "../core/FileSystem.h"

static constexpr float kTicksPerSecond = 25.0f;  // original engine rate

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
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
        if (const uint8_t* m = file_.chunkData("MASK", &n)) {
            if (!parseMasks(m, n, masks_)) SDL_Log("Level %d: mask data only partly read", number);
        }
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
                const int dir = in.set->dirCount >= 16 ? el.dir : 0;
                in.rec = el.kind == LevelElement::Actor ? in.set->find(0, dir)
                                                        : (in.set->records.empty() ? nullptr : &in.set->records.front());
                if (!in.rec || in.rec->entries.empty()) continue;
                // desynchronise identical animations (crowds, rivers...)
                in.entry = (int)((el.x * 7 + el.y * 13) % (int)in.rec->entries.size());
                in.ticks = (float)((el.x + el.y) % 7);
                updateFrameRect(in);
                instances_.push_back(std::move(in));
            }
        }
    }
    recreateTextures(r);
    SDL_Log("Level %d: %dx%d, %d elements, %d sprite files, %d masks, loaded in %u ms", number, background_.w,
            background_.h, (int)instances_.size(), (int)sprites_.size(), (int)masks_.size(), SDL_GetTicks() - t0);
    return true;
}

int Level::actorCount() const {
    int n = 0;
    for (const auto& in : instances_) n += in.el.kind == LevelElement::Actor;
    return n;
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
    in.world = {in.el.x - in.rec->anchorX + e.x, in.el.y - in.rec->anchorY + e.y, w, h};
    if (in.el.kind == LevelElement::Actor) {
        in.foot = {in.el.x, in.el.y};
        in.sortY = in.el.y;
    } else {
        in.sortY = in.el.y + in.el.z;
        in.foot = {in.world.x + w / 2, in.sortY};
    }
}

void Level::update(float dt) {
    const float ticks = dt * kTicksPerSecond;
    for (auto& in : instances_) {
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

void Level::render(SDL_Renderer* r, const Camera& cam, int sw, int sh) {
    bgTex_.draw(r, cam.originX(), cam.originY(), cam.zoom, sw, sh);

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
            if (!maskTex_[m] || mk.group != in.el.floor) continue;
            const SDL_Rect& mr = mk.rect;
            if (mr.x >= w.x + w.w || mr.y >= w.y + w.h || mr.x + mr.w <= w.x || mr.y + mr.h <= w.y) continue;
            if (in.foot.y >= mk.lineY((float)in.foot.x)) continue;  // sprite is in front of this mask
            if (!clipped) { SDL_RenderSetClipRect(r, &clip); clipped = true; }
            SDL_FRect md{cam.toScreenX((float)mr.x), cam.toScreenY((float)mr.y), mr.w * cam.zoom, mr.h * cam.zoom};
            SDL_RenderCopyF(r, maskTex_[m], nullptr, &md);
        }
        if (clipped) SDL_RenderSetClipRect(r, nullptr);
    }
}
