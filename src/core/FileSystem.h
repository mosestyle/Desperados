// OpenDesperados - file access to the user's own copy of the game data.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fs_ {

// Tries the candidate folders and remembers the first one that contains the game data.
bool findDataRoot(const std::vector<std::string>& candidates);
const std::string& dataRoot();

// Resolves a path relative to the data root, ignoring upper/lower case in every
// path component (the original game ran on Windows, Android is case sensitive).
// Returns an empty string when the file does not exist.
std::string resolve(const std::string& relative);

// File names (not paths) inside a data folder, e.g. listDir("data/characters").
std::vector<std::string> listDir(const std::string& relative);

bool readFile(const std::string& absolutePath, std::vector<uint8_t>& out);
bool readData(const std::string& relative, std::vector<uint8_t>& out);

}  // namespace fs_
