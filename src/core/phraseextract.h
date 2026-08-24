#pragma once

#include "srtcodec.h"

#include <string>
#include <vector>

namespace core {

struct PhraseSuggestOptions {
    bool skipSfx       = true;
    int  maxLen        = 60;
    int  maxPhrases    = 80;
    bool dedupe        = true;
};

// Heuristique locale (pas d’IA cloud) → labels bingo candidats.
std::vector<std::string> suggestBingoPhrases(const std::vector<Cue>& cues,
                                            const PhraseSuggestOptions& opt = {});

} // namespace core
