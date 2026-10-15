// The mission scripts' virtual machine (SBLibNG VMCore / VMStack / VMCodeManager): each level's
// Data\Levels\level_XX.scb holds compiled script classes (StartUp, one for each scripted element,
// waypoint and zone), each with its functions (Initialize, HourGlass, FilterEvent, ...) as
// "quads" of the original compiler. A quad is an opcode and up to three operands; an operand is a
// byte offset with two flag bits: 0 global, 0x4000 the class instance's variables, 0x8000 the
// function's volatile variables, 0xC000 its temporaries. Natives (IGetSam, ILockAI, ...: the 200
// of VMCoreCustom) are called with their parameters in a buffer and give back one 32 bit value.
//
// The file format and the natives' names come from OpenDeathValley's SCB reader and
// disassembler (github.com/OpenDeathValley/OpenDeathValley); the opcodes' meaning from the
// original's VMCore::Run (0x080f0a60) and its handlers.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct VMFunction {
    std::string name;
    uint32_t address = 0;      // first quad, in the class
    int params = 0, sizeOfRetVal = 0, sizeOfParams = 0;
    int sizeOfVolatile = 0, sizeOfTempor = 0;
};

// a quad as VMCore::Run reads it (12 bytes: +0 opcode, +4, +6, +8)
struct VMQuad {
    uint8_t op = 0;
    uint16_t a = 0, b = 0;   // +4, +6
    int32_t c = 0;           // +8 (the third operand in its low 16 bits)
    int32_t imm() const { return (int32_t)((uint32_t)a | ((uint32_t)b << 16)); }  // +4 as a 32 bit value
};

struct VMClass {
    std::string fileName, name;
    int nbOfVariables = 0, sizeOfVariables = 0;
    std::vector<VMFunction> functions;
    std::vector<VMQuad> quads;
    const VMFunction* Function(const std::string& n) const;
};

// VMCodeManager: the classes of a .scb file
class VMCodeManager {
public:
    bool Load(const std::string& gamePath);   // VMCodeManager::Load (0x080ef500)
    const VMClass* SearchClass(const std::string& name) const;
    const std::vector<std::unique_ptr<VMClass>>& Classes() const { return classes; }
private:
    std::vector<std::unique_ptr<VMClass>> classes;
};

class VMCore;
// a native: its parameters (32 bit each) and the VM (for the instance it runs for)
using VMNative = int32_t (*)(VMCore& vm, const int32_t* params);

// an instance of a script class (DVScript): its variables
struct VMInstance {
    const VMClass* cls = nullptr;
    std::vector<uint8_t> vars;   // VMCore +0x34
    void* owner = nullptr;       // the element it belongs to (IThis), none for StartUp
};

class VMCore {
public:
    static VMNative mparrayNativeFunctions[200];
    static const char* const kNativeNames[200];
    static std::vector<uint8_t> globals;   // VMCore +8

    // VMCore::Call(string): runs a function of the instance with these parameters; returns
    // its return value (VMCore::GetReturnValue)
    int32_t Call(VMInstance& inst, const std::string& function, const std::vector<int32_t>& params);
    VMInstance* current = nullptr;
    bool trace = false;

private:
    struct Record {
        std::vector<uint8_t> params;  // +0
        int32_t ret = 0;              // +4 the return value of the function it called
        int32_t retAddress = -1;      // +8
        std::vector<uint8_t> vol;     // +0xc
        std::vector<uint8_t> tmp;     // +0x14
    };
    std::vector<Record> stack;        // VMStack +0xc
    std::vector<uint8_t> pending;     // VMStack +0 the parameters of the next call
    int32_t pc = 0;                   // +0x3c
    int32_t retValue = 0;             // +0x44
    int32_t nativeParams[16] = {};    // +0x48
    int nativeCount = 0;              // +0x4c (in parameters)
    int32_t nativeRet = 0;            // +0x50
    void Run();
    uint8_t* Symbol(uint16_t s);      // VMCore::GetSymbolValuePtr (0x080f1480)
    int32_t& I(uint16_t s) { return *reinterpret_cast<int32_t*>(Symbol(s)); }
    float& F(uint16_t s) { return *reinterpret_cast<float*>(Symbol(s)); }
};
