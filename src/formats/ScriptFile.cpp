#include "ScriptFile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../core/FileSystem.h"

namespace {
struct LineReader {
    const uint8_t* d;
    size_t n, pos = 0;
    bool ok = true;
    std::string line() {
        size_t s = pos;
        while (pos < n && d[pos] != '\n') ++pos;
        if (pos >= n) { ok = false; return {}; }
        std::string out((const char*)d + s, pos - s);
        ++pos;
        if (!out.empty() && out.back() == '\r') out.pop_back();
        return out;
    }
};

// integer following `key` in `s`, or `def`
int valueAfter(const std::string& s, const char* key, int def = 0) {
    size_t p = s.find(key);
    if (p == std::string::npos) return def;
    return std::atoi(s.c_str() + p + std::strlen(key));
}

std::string wordAfter(const std::string& s, const char* key) {
    size_t p = s.find(key);
    if (p == std::string::npos) return {};
    p += std::strlen(key);
    size_t e = p;
    while (e < s.size() && s[e] != ' ' && s[e] != ',') ++e;
    return s.substr(p, e - p);
}
}  // namespace

const ScriptFunction* ScriptClass::function(const char* fn) const {
    for (const auto& f : functions)
        if (f.name == fn) return &f;
    return nullptr;
}

const ScriptClass* ScriptFile::find(const std::string& name) const {
    for (const auto& c : classes)
        if (c.name == name) return &c;
    return nullptr;
}

bool ScriptFile::parse(const uint8_t* data, size_t size) {
    classes.clear();
    LineReader r{data, size};
    r.line();  // version
    int count = valueAfter(r.line(), "nbOfClasses ");
    for (int ci = 0; ci < count && r.ok; ++ci) {
        ScriptClass c;
        c.name = wordAfter(r.line(), "className ");
        std::string l = r.line();
        c.varBytes = valueAfter(l, "sizeOfVariables ");
        l = r.line();
        while (r.ok && l.compare(0, 13, "nbOfFunctions") != 0) l = r.line();
        int nf = valueAfter(l, "nbOfFunctions ");
        for (int fi = 0; fi < nf && r.ok; ++fi) {
            l = r.line();
            ScriptFunction f;
            f.name = wordAfter(l, "functionName ");
            f.address = valueAfter(l, "address ");
            f.params = valueAfter(l, "nbOfParams ");
            f.paramBytes = valueAfter(l, "sizeOfParams ");
            do l = r.line(); while (r.ok && l.compare(0, 15, " sizeOfVolatile") != 0);
            c.functions.push_back(std::move(f));
        }
        do l = r.line(); while (r.ok && l.compare(0, 9, "nbOfQuads") != 0);
        int nq = valueAfter(l, "nbOfQuads ");
        if (!r.ok || nq < 0 || r.pos + (size_t)nq * 10 > size) return false;
        c.quads.resize(nq);
        for (int q = 0; q < nq; ++q) {
            const uint8_t* p = data + r.pos;
            ScriptQuad& s = c.quads[q];
            s.op = p[0];
            s.a = (uint16_t)(p[1] | p[2] << 8);
            s.b = (uint16_t)(p[3] | p[4] << 8);
            s.c = (uint16_t)(p[5] | p[6] << 8);
            s.u0 = (uint32_t)p[1] | (uint32_t)p[2] << 8 | (uint32_t)p[3] << 16 | (uint32_t)p[4] << 24;
            s.u4 = (uint32_t)p[5] | (uint32_t)p[6] << 8 | (uint32_t)p[7] << 16 | (uint32_t)p[8] << 24;
            if (p[9] != '~') return false;
            r.pos += 10;
        }
        classes.push_back(std::move(c));
    }
    return r.ok;
}

bool ScriptFile::load(const std::string& relativePath) {
    std::vector<uint8_t> buf;
    if (!fs_::readData(relativePath, buf)) return false;
    return parse(buf.data(), buf.size());
}
