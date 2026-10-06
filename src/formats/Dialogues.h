// DLGS chunk of a level file (DVDialogue): the mission's conversations.
//
//   u32 version (4), u32 textId, u32 waveId (resources in texts.res), u32 dialogueCount,
//   per dialogue: u32 lineCount, lineCount * (u32 speaker, u32 textIndex, u32 waveIndex)
//   then three lists, each u32 textId, u32 n, n * u32 index:
//     hints, objectives ("sentences"), victory/defeat texts (the last one is preceded by
//     an extra u32 n2 + n2 indices of title lines).
// Speakers: 0 Cooper, 1 Sam, 2 Doc, 3 Kate, 4 Sanchez, 5 Mia, 6+ other characters.
#pragma once
#include <cstdint>
#include <vector>

struct DialogueLine {
    int speaker = 0, text = 0, wave = 0;
};

struct Dialogues {
    uint32_t textId = 0, waveId = 0;
    std::vector<std::vector<DialogueLine>> dialogues;
    uint32_t hintId = 0, objectiveId = 0, defeatId = 0;
    std::vector<int> hints, objectives, defeatTexts;
    bool parse(const uint8_t* data, size_t size);
};
