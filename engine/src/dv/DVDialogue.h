// DVDialogue (Game/DVDialogue.cpp): the level's dialogues and texts (the DLGS hunk). A dialogue is
// a list of lines (speaker, text, voice). PreparePlayDialogue starts one two ticks later; every
// line plays its voice (Data\Interface\Dialogs\Level_XX\..., in the language's folder) and waits for
// its end, or shows for 3 seconds when it has none; a tap skips the line (ProcessInput). The panel
// at the top of the screen shows the speaker's portrait (default.res, by SBFramePannelFX) and the
// text; the app draws it from what is here.
//
// The same hunk holds the tutorial hints (DVEngine::SetHint), the objectives ("sentences",
// DVMenu::AddSentence) and the victory / defeat texts.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

class SBFile;
class SBTextResources;
class DVSequenceElement;

struct DVDialogueEntry {
    std::vector<uint16_t> speakers;   // +0
    std::vector<uint32_t> texts;      // +0x1c
    std::vector<uint32_t> waves;      // +0x38
};

class DVDialogue {
public:
    static DVDialogue* mpDialogue;
    static SBTextResources* mpTexts;  // texts.res (loaded once)
    DVDialogue();
    ~DVDialogue();
    int LoadDialogue(SBFile& f);                  // 0x081fd070
    bool PreparePlayDialogue(uint32_t n);         // 0x081fd950
    void Refresh();                               // 0x081fe560 (once a tick)
    void Skip();  // ProcessInput: the line is over
    void Mute();  // the voice stops, the line goes on silently
    void Stop();                                  // 0x081fcf60
    bool Playing() const { return playing || startIn > 0; }
    // what the panel shows
    int Speaker() const;
    std::u16string Text() const;
    std::u16string HintText(uint32_t hint) const;      // hint 1.. (DVEngine::SetHint)
    std::u16string SentenceText(uint32_t s) const;     // an objective
    std::u16string VictoryText(uint32_t i) const;      // the end-of-mission texts
    size_t VictoryCount() const { return victory.size(); }
    // fast mode (a skipped briefing): no voices, each line still lasts as long as its voice
    bool fast = false;

    uint32_t textId = 0, waveId = 0;           // +0xb0, +0xac
    std::vector<DVDialogueEntry> dialogues;    // +0xc4
    uint32_t hintTextId = 0;                   // +0x14c
    std::vector<uint32_t> hints;               // +0x130
    uint32_t sentenceTextId = 0;               // +0x16c
    std::vector<uint32_t> sentences;           // +0x150
    uint32_t victoryTextId = 0;                // +0x18c
    std::vector<uint32_t> victory;             // +0x170 (the victories first, +0xbc of them)
    uint32_t victoryCount = 0;                 // +0xbc
    DVSequenceElement* element = nullptr;      // +0xc0 the script waiting for the end

private:
    bool playing = false;        // +0x98
    bool voiceOn = false;        // +0x99
    bool noVoice = false;        // +0x9b
    uint16_t startIn = 0;        // +0x9e
    uint16_t current = 0;        // +0xa4
    uint16_t line = 0;           // +0xa6
    int ticksLeft = 0;           // +0xb4 / the voice's length
    void End();
};

// the length of a WAV file in ticks (25 a second), 0 if it can't be read
int SBWaveTicks(const std::string& gamePath);
// where a voice of texts.res is: the language's folder first, then the game's
std::string SBVoicePath(const std::string& wave);
