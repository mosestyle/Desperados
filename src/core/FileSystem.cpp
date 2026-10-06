#include "FileSystem.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>

namespace fs_ {
namespace stdfs = std::filesystem;

static std::string g_root;
static std::unordered_map<std::string, std::string> g_cache;

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static std::string resolveFrom(const std::string& root, const std::string& relative) {
    std::error_code ec;
    stdfs::path cur(root);
    std::string rel = relative;
    std::replace(rel.begin(), rel.end(), '\\', '/');
    size_t start = 0;
    while (start <= rel.size()) {
        size_t slash = rel.find('/', start);
        std::string part = rel.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        start = (slash == std::string::npos) ? rel.size() + 1 : slash + 1;
        if (part.empty() || part == ".") continue;
        stdfs::path direct = cur / part;
        if (stdfs::exists(direct, ec)) { cur = direct; continue; }
        const std::string want = lower(part);
        bool found = false;
        for (stdfs::directory_iterator it(cur, ec), end; !ec && it != end; it.increment(ec)) {
            if (lower(it->path().filename().string()) == want) { cur = it->path(); found = true; break; }
        }
        if (!found) return {};
    }
    return cur.string();
}

bool findDataRoot(const std::vector<std::string>& candidates) {
    for (const auto& c : candidates) {
        if (c.empty()) continue;
        SDL_Log("Looking for game data in: %s", c.c_str());
        if (!resolveFrom(c, "data/levels/level_01.dvd").empty()) {
            g_root = c;
            g_cache.clear();
            SDL_Log("Found game data in: %s", c.c_str());
            return true;
        }
    }
    return false;
}

const std::string& dataRoot() { return g_root; }

std::string resolve(const std::string& relative) {
    const std::string key = lower(relative);
    auto it = g_cache.find(key);
    if (it != g_cache.end()) return it->second;
    std::string r = resolveFrom(g_root, relative);
    g_cache[key] = r;
    return r;
}

std::vector<std::string> listDir(const std::string& relative) {
    std::vector<std::string> names;
    std::string dir = resolve(relative);
    if (dir.empty()) return names;
    std::error_code ec;
    for (stdfs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        names.push_back(it->path().filename().string());
    return names;
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return false;
    Sint64 size = SDL_RWsize(rw);
    if (size < 0) { SDL_RWclose(rw); return false; }
    out.resize((size_t)size);
    size_t got = size ? SDL_RWread(rw, out.data(), 1, (size_t)size) : 0;
    SDL_RWclose(rw);
    return got == (size_t)size;
}

bool readData(const std::string& relative, std::vector<uint8_t>& out) {
    std::string p = resolve(relative);
    if (p.empty()) { SDL_Log("Missing game file: %s", relative.c_str()); return false; }
    return readFile(p, out);
}

}  // namespace fs_
