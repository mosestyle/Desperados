// SBFile: the original's file class (SBLibNG/SBFile.cpp). Reads little-endian data from the
// player's game folder. Game paths use backslashes and are relative to the game folder
// ("\data\levels\level_01.dvm"); they are looked up without caring about upper/lower case,
// because the copies on phones and the Linux release differ in case.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

class SBFile {
public:
    SBFile() = default;
    ~SBFile();
    SBFile(const SBFile&) = delete;
    SBFile& operator=(const SBFile&) = delete;

    // SBFile::Open(path, 1 = read). Returns 0 on success, like the original.
    int Open(const std::string& gamePath, int mode = 1);
    void Close();
    bool IsOpen() const { return f != nullptr; }

    // SBFile::Serialize: reads `size` bytes. 0 = ok, negative = error (-5 = short read).
    int Serialize(void* data, int size);
    // SBFile::Skip(offset, whence): 0 = from start, 1 = from current, 2 = from end.
    int Skip(int offset, unsigned whence);
    int Tell() const { return pos; }
    int Size() const { return size; }

    // helpers for readable translations of Serialize calls
    uint8_t U8();
    uint16_t U16();
    int16_t S16() { return (int16_t)U16(); }
    uint32_t U32();
    int32_t S32() { return (int32_t)U32(); }
    float F32();
    std::string String16();   // u16 length + characters (no terminator)

    int lastError = 0;        // +0x14
    uint32_t version = 0;     // +0x10: the hunk loaders read their version here

    // where the game folder is (set once by the app)
    static void SetGameRoot(const std::string& root);
    static const std::string& GameRoot();
    // the real path on disk for a game path ("" if it doesn't exist)
    static std::string Resolve(const std::string& gamePath);
    static bool Exists(const std::string& gamePath) { return !Resolve(gamePath).empty(); }
    // reads a whole file
    static bool ReadAll(const std::string& gamePath, std::vector<uint8_t>& out);

private:
    FILE* f = nullptr;
    int pos = 0;
    int size = 0;
};

// The original's error report (SBError). Fatal errors in the original stop the game; here they
// are logged and the caller carries on as the original's non-fatal path does.
void SBError(bool fatal, const char* file, int line, const char* fmt, ...);
void SBLog(const char* fmt, ...);
// also write the log to this file (the player's game folder, so it can be sent to us)
void SBLogToFile(const std::string& path);
