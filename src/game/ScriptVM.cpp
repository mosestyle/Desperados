// Opcodes (from VMCore::Run in the 2018 executable):
//   1 nop                 2 push a (call argument)   3 enter: a = volatile bytes, b = temporary bytes
//   4 end of function     5 call u0                  6 return          7 return a
//   8 a = parameter[u4]   9 parameter[u4] = a        10 a = return value of the last call
//   11 native argument a  12 native call u0          13 a = native return value
//   14 goto u0            15 if a != 0 goto u4       16 if a == 0 goto u4
//   17,18 a = b           19,20 a = immediate u4 (int / float)
//   21 a = -b (int)       22 a = -b (float)          23 a = (int)b float   24 a = (float)b int
//   25..28 a = b + - * / c (int)     29..32 the same for floats
//   33..38 a = b <= < >= > != == c (int)   39..44 the same for floats (result 1.0f / 0.0f)
#include "ScriptVM.h"

#include <SDL.h>

#include <cstring>

namespace {
inline int32_t rd(const uint8_t* p) { int32_t v; std::memcpy(&v, p, 4); return v; }
inline float rdf(const uint8_t* p) { float v; std::memcpy(&v, p, 4); return v; }
inline void wr(uint8_t* p, int32_t v) { std::memcpy(p, &v, 4); }
inline void wrf(uint8_t* p, float v) { std::memcpy(p, &v, 4); }
uint8_t g_dummy[8];
uint8_t* at(std::vector<uint8_t>& buf, size_t off) {
    if (off + 4 > buf.size()) buf.resize(off + 4, 0);
    return buf.data() + off;
}
}  // namespace

uint8_t* ScriptVM::sym(ScriptInstance& inst, Frame& f, uint16_t s) {
    const size_t off = s & 0x3FFF;
    switch (s & 0xC000) {
    case 0x0000: return off + 4 <= globals_.size() ? globals_.data() + off : g_dummy;
    case 0x4000: return at(inst.vars, off);
    case 0x8000: return at(f.vol, off);
    default: return at(f.tmp, off);
    }
}

int32_t ScriptVM::call(ScriptInstance& inst, const char* fn, const std::vector<int32_t>& args, bool* found) {
    const ScriptClass* cls = inst.cls;
    const ScriptFunction* f = cls ? cls->function(fn) : nullptr;
    if (found) *found = f != nullptr;
    if (!f) return 0;
    if (depth_ > 16) { SDL_Log("script: event recursion too deep in %s.%s", cls->name.c_str(), fn); return 0; }
    if ((int)inst.vars.size() < cls->varBytes) inst.vars.resize(cls->varBytes, 0);
    ++depth_;

    std::vector<Frame> frames(1);
    frames[0].params.resize(args.size() * 4 + 4, 0);
    for (size_t i = 0; i < args.size(); ++i) wr(frames[0].params.data() + i * 4, args[i]);
    std::vector<uint8_t> pushed;        // arguments for the next script call
    std::vector<int32_t> nativeArgs;    // arguments for the next native call
    int32_t nativeRet = 0, result = 0;
    const auto& q = cls->quads;
    int ip = f->address;
    long steps = 0;

    while (ip >= 0 && ip < (int)q.size()) {
        if (++steps > 2000000) { SDL_Log("script: %s.%s runs too long, stopped", cls->name.c_str(), fn); break; }
        const ScriptQuad& o = q[ip];
        Frame& fr = frames.back();
        int next = ip + 1;
        switch (o.op) {
        case 1: break;
        case 2: { size_t n = pushed.size(); pushed.resize(n + 4); std::memcpy(&pushed[n], sym(inst, fr, o.a), 4); break; }
        case 3: fr.vol.assign(o.a + 4, 0); fr.tmp.assign(o.b + 4, 0); break;
        case 4: next = -1; break;
        case 5: {
            Frame nf;
            nf.params = std::move(pushed);
            nf.params.resize(nf.params.size() + 4, 0);
            pushed.clear();
            nf.returnTo = ip + 1;
            frames.push_back(std::move(nf));
            next = (int)o.u0;
            break;
        }
        case 6:
        case 7: {
            int32_t v = o.op == 7 ? rd(sym(inst, fr, o.a)) : 0;
            int ret = fr.returnTo;
            frames.pop_back();
            if (frames.empty()) { result = v; next = -1; break; }
            if (o.op == 7) frames.back().ret = v;
            next = ret;
            break;
        }
        case 8: { uint8_t* d = sym(inst, fr, o.a); wr(d, rd(at(fr.params, o.u4))); break; }
        case 9: { int32_t v = rd(sym(inst, fr, o.a)); wr(at(fr.params, o.u4), v); break; }
        case 10: wr(sym(inst, fr, o.a), fr.ret); break;
        case 11: nativeArgs.push_back(rd(sym(inst, fr, o.a))); break;
        case 12: {
            std::vector<int32_t> a;
            a.swap(nativeArgs);
            nativeRet = host_.native((int)o.u0, a.data(), (int)a.size());
            break;
        }
        case 13: wr(sym(inst, fr, o.a), nativeRet); break;
        case 14: next = (int)o.u0; break;
        case 15: if (rd(sym(inst, fr, o.a)) != 0) next = (int)o.u4; break;
        case 16: if (rd(sym(inst, fr, o.a)) == 0) next = (int)o.u4; break;
        case 17: case 18: { int32_t v = rd(sym(inst, fr, o.b)); wr(sym(inst, fr, o.a), v); break; }
        case 19: case 20: wr(sym(inst, fr, o.a), (int32_t)o.u4); break;
        case 21: { int32_t v = rd(sym(inst, fr, o.b)); wr(sym(inst, fr, o.a), -v); break; }
        case 22: { float v = rdf(sym(inst, fr, o.b)); wrf(sym(inst, fr, o.a), -v); break; }
        case 23: { float v = rdf(sym(inst, fr, o.b)); wr(sym(inst, fr, o.a), (int32_t)v); break; }
        case 24: { int32_t v = rd(sym(inst, fr, o.b)); wrf(sym(inst, fr, o.a), (float)v); break; }
        default:
            if (o.op >= 25 && o.op <= 38) {
                int32_t x = rd(sym(inst, fr, o.b)), y = rd(sym(inst, fr, o.c)), v = 0;
                switch (o.op) {
                case 25: v = x + y; break;
                case 26: v = x - y; break;
                case 27: v = x * y; break;
                case 28: v = y != 0 ? x / y : 0; break;
                case 33: v = x <= y; break;
                case 34: v = x < y; break;
                case 35: v = x >= y; break;
                case 36: v = x > y; break;
                case 37: v = x != y; break;
                case 38: v = x == y; break;
                default: break;
                }
                wr(sym(inst, fr, o.a), v);
            } else if (o.op >= 29 && o.op <= 44) {
                float x = rdf(sym(inst, fr, o.b)), y = rdf(sym(inst, fr, o.c)), v = 0;
                switch (o.op) {
                case 29: v = x + y; break;
                case 30: v = x - y; break;
                case 31: v = x * y; break;
                case 32: v = y != 0 ? x / y : 0; break;
                case 39: v = x <= y; break;
                case 40: v = x < y; break;
                case 41: v = x >= y; break;
                case 42: v = x > y; break;
                case 43: v = x != y; break;
                case 44: v = x == y; break;
                default: break;
                }
                wrf(sym(inst, fr, o.a), v);
            } else {
                SDL_Log("script: unknown opcode %d in %s at %d", o.op, cls->name.c_str(), ip);
                next = -1;
            }
        }
        ip = next;
    }
    --depth_;
    return result;
}
