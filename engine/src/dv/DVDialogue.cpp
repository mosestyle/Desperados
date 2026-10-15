#include "dv/DVDialogue.h"

#include <cstring>

#include "dv/DVSequence.h"
#include "sb/SBFile.h"
#include "sb/SBFont.h"
#include "sb/SBSound.h"

DVDialogue* DVDialogue::mpDialogue = nullptr;
SBTextResources* DVDialogue::mpTexts = nullptr;

DVDialogue::DVDialogue() {
    mpDialogue = this;
    if (!mpTexts) {
        mpTexts = new SBTextResources();
        if (!mpTexts->Load()) SBLog("texts.res not found: no dialogue texts");
        else SBLog("texts.res: %s", mpTexts->language.c_str());
    }
}

DVDialogue::~DVDialogue() {
    if (voiceOn) SBSound::StopVoice();
    if (mpDialogue == this) mpDialogue = nullptr;
}

int DVDialogue::LoadDialogue(SBFile& f) {
    int start = f.Tell();
    f.version = f.U32();
    if (f.version != 4)
        SBError(true, "../code/Game/DVDialogue.cpp", 0xc4, "Version Check Failed on %s (file version %u, expected version %u).",
                "Hunk Dialogue", f.version, 4);
    textId = f.U32();
    waveId = f.U32();
    uint32_t n = f.U32();
    dialogues.assign(n, DVDialogueEntry());
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t lines = f.U32();
        for (uint32_t k = 0; k < lines; ++k) {
            uint32_t speaker = f.U32();
            uint32_t text = f.U32();
            uint32_t wave = f.U32();
            dialogues[i].speakers.push_back((uint16_t)speaker);
            dialogues[i].texts.push_back(text);
            dialogues[i].waves.push_back(wave);
        }
    }
    hintTextId = f.U32();
    hints.resize(f.U32());
    for (uint32_t& h : hints) h = f.U32();
    sentenceTextId = f.U32();
    sentences.resize(f.U32());
    for (uint32_t& s : sentences) s = f.U32();
    victoryTextId = f.U32();
    victoryCount = f.U32();
    victory.resize(victoryCount);
    for (uint32_t& v : victory) v = f.U32();
    uint32_t defeats = f.U32();
    for (uint32_t k = 0; k < defeats; ++k) victory.push_back(f.U32());
    return f.Tell() - start;
}

bool DVDialogue::PreparePlayDialogue(uint32_t n) {
    if (n >= dialogues.size()) {
        SBError(false, "DVDialogue.cpp", 299, "Trying to play an invalid dialogue");
        return false;
    }
    if (voiceOn) SBSound::StopVoice();
    voiceOn = noVoice = false;
    current = (uint16_t)n;
    line = 0;
    startIn = 2;
    playing = false;
    return true;
}

void DVDialogue::End() {
    if (voiceOn) SBSound::StopVoice();
    voiceOn = noVoice = false;
    playing = false;
    line = 0;
    if (element) {
        DVSequenceElement* e = element;
        element = nullptr;
        e->SetState(SEQ_DONE);
    }
}

void DVDialogue::Stop() {
    startIn = 0;
    if (playing || element) End();
}

void DVDialogue::Mute() {
    if (!voiceOn) return;
    SBSound::StopVoice();
    voiceOn = false;
    noVoice = true;  // the line keeps the rest of its time
}

void DVDialogue::Skip() {
    if (!playing) return;
    if (voiceOn) SBSound::StopVoice();
    voiceOn = noVoice = false;
    ++line;
    if (line >= dialogues[current].texts.size()) End();
}

// DVDialogue::Refresh (0x081fe560): starts the next line's voice and waits for its end
void DVDialogue::Refresh() {
    if (startIn) {
        if (--startIn) return;
        playing = true;  // PlayDialogue
        line = 0;
    }
    if (!playing) return;
    const DVDialogueEntry& d = dialogues[current];
    if (line >= d.texts.size()) {
        End();
        return;
    }
    if (!voiceOn && !noVoice) {
        uint32_t w = d.waves[line];
        std::string path;
        if (w != 0xffffffff && mpTexts) path = SBVoicePath(mpTexts->Wave(waveId, w));
        int ticks = path.empty() ? 0 : SBWaveTicks(path);
        if (ticks <= 0) {
            ticksLeft = 0x4b;  // three seconds
            noVoice = true;
        } else if (fast) {
            // a skipped briefing: the line takes its time, silently, so that what the other
            // sequences do in the meantime keeps its timing
            ticksLeft = ticks + 5;
            noVoice = true;
        } else {
            ticksLeft = ticks + 5;
            voiceOn = true;
            SBSound::PlayVoice(path);
        }
    }
    if (--ticksLeft <= 0) {
        if (voiceOn) SBSound::StopVoice();
        voiceOn = noVoice = false;
        ++line;
        if (line >= d.texts.size()) End();
    }
}

int DVDialogue::Speaker() const {
    if (!playing || current >= dialogues.size() || line >= dialogues[current].speakers.size()) return -1;
    return dialogues[current].speakers[line];
}

std::u16string DVDialogue::Text() const {
    if (!playing || !mpTexts || current >= dialogues.size() || line >= dialogues[current].texts.size()) return u"";
    return mpTexts->Text(textId, dialogues[current].texts[line]);
}

std::u16string DVDialogue::HintText(uint32_t h) const {
    if (!mpTexts || h == 0 || h > hints.size()) return u"";
    return mpTexts->Text(hintTextId, hints[h - 1]);
}

std::u16string DVDialogue::SentenceText(uint32_t s) const {
    if (!mpTexts || s >= sentences.size()) return u"";
    return mpTexts->Text(sentenceTextId, sentences[s]);
}

std::u16string DVDialogue::VictoryText(uint32_t i) const {
    if (!mpTexts || i >= victory.size()) return u"";
    return mpTexts->Text(victoryTextId, victory[i]);
}

std::string SBVoicePath(const std::string& wave) {
    if (wave.empty()) return "";
    std::string lang = DVDialogue::mpTexts ? DVDialogue::mpTexts->language : "";
    if (!lang.empty()) {
        std::string p = "localisation\\" + lang + "\\Data\\Interface\\" + wave;
        if (SBFile::Exists(p)) return p;
    }
    std::string p = "Data\\Interface\\" + wave;
    return SBFile::Exists(p) ? p : "";
}

int SBWaveTicks(const std::string& gamePath) {
    SBFile f;
    if (f.Open(gamePath) != 0) return 0;
    char riff[12];
    if (f.Serialize(riff, 12) != 0 || memcmp(riff, "RIFF", 4) != 0) return 0;
    uint32_t byteRate = 0;
    while (f.Tell() + 8 <= f.Size()) {
        char id[4];
        f.Serialize(id, 4);
        uint32_t len = f.U32();
        int at = f.Tell();
        if (memcmp(id, "fmt ", 4) == 0) {
            f.U16();
            f.U16();
            f.U32();
            byteRate = f.U32();
        } else if (memcmp(id, "data", 4) == 0) {
            if (!byteRate) return 0;
            return (int)((uint64_t)len * 25 / byteRate);
        }
        f.Skip(at + (int)((len + 1) & ~1u), 0);
    }
    return 0;
}
