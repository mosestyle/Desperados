// Included by every translated class: fixed-size integers, strings, the original containers,
// the platform layer and the STUB marker for methods not translated yet.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "platform/thq.h"
#include "sb/SBContainers.h"

// A method that has not been translated yet: logs its name the first time it runs.
void SBStubCalled(const char* name);
#define STUB(name)                          \
    do {                                    \
        static bool reported_ = false;      \
        if (!reported_) {                   \
            reported_ = true;               \
            SBStubCalled(name);             \
        }                                   \
    } while (0)

// RECT as the original used it (Windows layout).
struct RECT {
    int32_t left, top, right, bottom;
};

typedef void* HWND;
