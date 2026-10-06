#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Compression codecs used by the game's containers.
enum class Codec : uint32_t { Zlib = 1, Bzip2 = 2 };

// Decompresses one stream. `expected` is the size the caller expects (0 = unknown).
// `consumed` receives how many input bytes the stream used.
bool decompress(Codec codec, const uint8_t* in, size_t inSize, size_t expected,
                std::vector<uint8_t>& out, size_t* consumed = nullptr);
