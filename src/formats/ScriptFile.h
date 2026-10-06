// Compiled mission scripts (data/levels/level_XX.scb).
//
// Text header: "version 1.00, debug 0", "nbOfClasses N". Per class:
//   fileName <path> , className <name>
//   nbOfVariables n, sizeOfVariables bytes
//   nbOfFunctions n, then per function:
//     functionName <name> , address <quad>, nbOfParams n, sizeOfRetVal 4, sizeOfParams bytes
//     functionParameters / (blank) / " sizeOfVolatile n, sizeOfTempor n"
//   nbOfQuads n, then n binary quads of 10 bytes: u8 opcode, 8 operand bytes, '~'.
//
// Operands are u16 symbols (top two bits = storage: 0 global, 0x4000 class variable,
// 0x8000 volatile local, 0xC000 temporary; low 14 bits = byte offset), or u32 values
// (jump targets, native numbers, immediates). See ScriptVM.cpp for the opcode list.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct ScriptQuad {
    uint8_t op = 0;
    uint16_t a = 0, b = 0, c = 0;  // symbols at operand bytes 0, 2, 4
    uint32_t u0 = 0, u4 = 0;       // u32 at operand bytes 0 and 4
};

struct ScriptFunction {
    std::string name;
    int address = 0, params = 0, paramBytes = 0;
};

struct ScriptClass {
    std::string name;
    int varBytes = 0;
    std::vector<ScriptFunction> functions;
    std::vector<ScriptQuad> quads;
    const ScriptFunction* function(const char* name) const;
};

struct ScriptFile {
    std::vector<ScriptClass> classes;
    bool parse(const uint8_t* data, size_t size);
    bool load(const std::string& relativePath);
    const ScriptClass* find(const std::string& name) const;
};
