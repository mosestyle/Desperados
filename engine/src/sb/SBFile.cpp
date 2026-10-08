#include "sb/SBFile.h"

#include <cstdarg>
#include <cstring>
#include <dirent.h>
#include <map>
#include <mutex>
#include <sys/stat.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

static std::string gRoot = ".";
// lower-case path relative to the root -> real relative path, built per directory on demand
static std::map<std::string, std::map<std::string, std::string>> gDirCache;
static std::mutex gDirMutex;

void SBFile::SetGameRoot(const std::string& root) {
    std::lock_guard<std::mutex> lock(gDirMutex);
    gRoot = root;
    while (gRoot.size() > 1 && gRoot.back() == '/') gRoot.pop_back();
    gDirCache.clear();
}
const std::string& SBFile::GameRoot() { return gRoot; }

static std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s;
}

// entries of a real directory (relative to the root), keyed by lower-case name
static const std::map<std::string, std::string>& listDir(const std::string& realRel) {
    auto it = gDirCache.find(realRel);
    if (it != gDirCache.end()) return it->second;
    std::map<std::string, std::string>& m = gDirCache[realRel];
    std::string full = realRel.empty() ? gRoot : gRoot + "/" + realRel;
    if (DIR* d = opendir(full.c_str())) {
        while (dirent* e = readdir(d)) {
            if (e->d_name[0] == '.' && (!e->d_name[1] || (e->d_name[1] == '.' && !e->d_name[2]))) continue;
            m.emplace(lower(e->d_name), e->d_name);
        }
        closedir(d);
    }
    return m;
}

std::string SBFile::Resolve(const std::string& gamePath) {
    std::lock_guard<std::mutex> lock(gDirMutex);
    std::string rel;
    size_t i = 0;
    const std::string& p = gamePath;
    while (i < p.size()) {
        while (i < p.size() && (p[i] == '\\' || p[i] == '/')) ++i;
        size_t j = i;
        while (j < p.size() && p[j] != '\\' && p[j] != '/') ++j;
        if (j == i) break;
        std::string part = p.substr(i, j - i);
        i = j;
        if (part == ".") continue;
        const auto& entries = listDir(rel);
        auto e = entries.find(lower(part));
        if (e == entries.end()) return "";
        rel = rel.empty() ? e->second : rel + "/" + e->second;
    }
    std::string full = gRoot + "/" + rel;
    struct stat st;
    if (stat(full.c_str(), &st) != 0) return "";
    return full;
}

bool SBFile::ReadAll(const std::string& gamePath, std::vector<uint8_t>& out) {
    std::string real = Resolve(gamePath);
    if (real.empty()) return false;
    FILE* f = fopen(real.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    bool ok = n <= 0 || fread(out.data(), 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    return ok;
}

SBFile::~SBFile() { Close(); }

int SBFile::Open(const std::string& gamePath, int mode) {
    Close();
    std::string real = Resolve(gamePath);
    if (!real.empty()) f = fopen(real.c_str(), "rb");
    if (!f) {
        lastError = -1;
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    pos = 0;
    lastError = 0;
    return 0;
}

void SBFile::Close() {
    if (f) fclose(f);
    f = nullptr;
}

int SBFile::Serialize(void* data, int n) {
    if (!f) return lastError = -4;
    if (n <= 0) return lastError = 0;
    if ((int)fread(data, 1, (size_t)n, f) != n) {
        memset(data, 0, (size_t)n);
        return lastError = -5;
    }
    pos += n;
    return lastError = 0;
}

int SBFile::Skip(int offset, unsigned whence) {
    if (!f) return lastError = -4;
    int w = whence == 1 ? SEEK_CUR : whence == 2 ? SEEK_END : SEEK_SET;
    if (fseek(f, offset, w) != 0) return lastError = -7;
    pos = (int)ftell(f);
    return lastError = 0;
}

uint8_t SBFile::U8() {
    uint8_t v = 0;
    Serialize(&v, 1);
    return v;
}
uint16_t SBFile::U16() {
    uint8_t b[2] = {0, 0};
    Serialize(b, 2);
    return (uint16_t)(b[0] | b[1] << 8);
}
uint32_t SBFile::U32() {
    uint8_t b[4] = {0, 0, 0, 0};
    Serialize(b, 4);
    return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24;
}
float SBFile::F32() {
    uint32_t u = U32();
    float v;
    memcpy(&v, &u, 4);
    return v;
}
std::string SBFile::String16() {
    uint16_t n = U16();
    std::string s(n, '\0');
    if (n) Serialize(&s[0], n);
    size_t z = s.find('\0');
    if (z != std::string::npos) s.resize(z);
    return s;
}

static FILE* gLogFile = nullptr;

void SBLogToFile(const std::string& path) {
    if (gLogFile) fclose(gLogFile);
    gLogFile = fopen(path.c_str(), "w");
}

void SBLog(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (gLogFile) {
        fprintf(gLogFile, "%s\n", buf);
        fflush(gLogFile);
    }
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "DVEngine", "%s", buf);
#endif
    fprintf(stderr, "%s\n", buf);
}

void SBError(bool fatal, const char* file, int line, const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    SBLog("%s (%s:%d)%s", buf, file ? file : "?", line, fatal ? " [fatal in the original]" : "");
}
