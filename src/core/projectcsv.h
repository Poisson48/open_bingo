#pragma once

#include "bingotypes.h"
#include "csv.h"

#include <string>
#include <vector>

namespace core {

// Import/export phrases + gages (deux CSV séparés). Voir docs/PLAN-csv-mcp.md.
// Après import côté AppController : grilles conservées, gridsDirty = true (pas de wipe).

struct CsvImportResult {
    int                      added   = 0;
    int                      skipped = 0;
    std::vector<std::string> errors;
};

// Header obligatoire ; colonnes par nom (ordre libre).
// phrases: label|phrase|texte, points|gage|numero, rate
// gages:   description|texte, number|numero|n, hp, rate
// replace=true → remplace la liste ; false → append.
CsvImportResult importPhrasesCsv(Project& p, const std::string& text, bool replace);
CsvImportResult importGagesCsv(Project& p, const std::string& text, bool replace);

std::string exportPhrasesCsv(const Project& p);
std::string exportGagesCsv(const Project& p);

} // namespace core
