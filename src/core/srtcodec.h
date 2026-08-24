#pragma once

#include <string>
#include <vector>

namespace core {

// Cue de sous-titre (SRT / VTT). Plan: docs/PLAN-opensubtitles.md
struct Cue {
    int         startMs    = 0;
    int         endMs      = 0;
    std::string text;                 // brut
    std::string plain;                // nettoyé (tags HTML/ASS strip)
    bool        likelySfx  = false;   // [bruit], (musique), etc.
};

struct SrtParseResult {
    std::vector<Cue> cues;
    std::vector<std::string> errors;
};

// Parse SRT (et VTT basique). BOM UTF-8 OK. skipSfxTags ne retire pas les cues,
// mais marque likelySfx + plain sans crochets si possible.
SrtParseResult parseSrt(const std::string& text);
SrtParseResult parseSrtFile(const std::string& path);

std::string cuePlainText(const std::string& raw);
bool cueLooksLikeSfx(const std::string& plain);

} // namespace core
