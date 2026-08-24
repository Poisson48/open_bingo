#include "projectcsv.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace core {
namespace {

std::string trim(std::string s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string toLower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string stripBom(std::string text)
{
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == 0xEF
        && static_cast<unsigned char>(text[1]) == 0xBB
        && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return text;
}

bool parseIntField(const std::string& raw, int& out)
{
    const std::string s = trim(raw);
    if (s.empty())
        return false;
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || (end && *end != '\0'))
        return false;
    out = static_cast<int>(v);
    return true;
}

int clampRate(int v)
{
    return std::clamp(v, 0, 100);
}

bool isCommentRow(const std::vector<std::string>& row)
{
    if (row.empty())
        return true;
    const std::string first = trim(row[0]);
    return first.empty() || first[0] == '#';
}

using ColMap = std::unordered_map<std::string, int>;

ColMap mapHeader(const std::vector<std::string>& header)
{
    ColMap m;
    for (int i = 0; i < static_cast<int>(header.size()); ++i) {
        const std::string key = toLower(trim(header[static_cast<size_t>(i)]));
        if (!key.empty())
            m[key] = i;
    }
    return m;
}

int colIndex(const ColMap& m, std::initializer_list<const char*> aliases)
{
    for (const char* a : aliases) {
        const auto it = m.find(a);
        if (it != m.end())
            return it->second;
    }
    return -1;
}

std::string cellAt(const std::vector<std::string>& row, int idx)
{
    if (idx < 0 || idx >= static_cast<int>(row.size()))
        return {};
    return row[static_cast<size_t>(idx)];
}

} // namespace

CsvImportResult importPhrasesCsv(Project& p, const std::string& text, bool replace)
{
    CsvImportResult r;
    const auto rows = csvParse(stripBom(text));
    size_t headerIdx = 0;
    while (headerIdx < rows.size() && isCommentRow(rows[headerIdx]))
        ++headerIdx;
    if (headerIdx >= rows.size()) {
        r.errors.push_back("phrases.csv: header manquant");
        return r;
    }

    const ColMap cols = mapHeader(rows[headerIdx]);
    const int iLabel  = colIndex(cols, {"label", "phrase", "texte"});
    const int iPoints = colIndex(cols, {"points", "gage", "numero"});
    const int iRate   = colIndex(cols, {"rate"});
    if (iLabel < 0) {
        r.errors.push_back("phrases.csv: colonne label|phrase|texte requise");
        return r;
    }

    std::vector<Case> imported;
    for (size_t ri = headerIdx + 1; ri < rows.size(); ++ri) {
        const auto& row = rows[ri];
        if (isCommentRow(row)) {
            ++r.skipped;
            continue;
        }
        const std::string label = trim(cellAt(row, iLabel));
        if (label.empty()) {
            ++r.skipped;
            continue;
        }

        Case c;
        c.label = label;
        c.points = 1;
        c.rate = 50;
        if (iPoints >= 0) {
            int v = 0;
            if (parseIntField(cellAt(row, iPoints), v))
                c.points = v;
            else if (!trim(cellAt(row, iPoints)).empty()) {
                r.errors.push_back("ligne " + std::to_string(ri + 1) + ": points invalides");
                ++r.skipped;
                continue;
            }
        }
        if (iRate >= 0) {
            int v = 0;
            if (parseIntField(cellAt(row, iRate), v))
                c.rate = clampRate(v);
            else if (!trim(cellAt(row, iRate)).empty()) {
                r.errors.push_back("ligne " + std::to_string(ri + 1) + ": rate invalide");
                ++r.skipped;
                continue;
            }
        } else {
            c.rate = clampRate(c.rate);
        }
        imported.push_back(std::move(c));
        ++r.added;
    }

    if (replace)
        p.cases = std::move(imported);
    else
        p.cases.insert(p.cases.end(), imported.begin(), imported.end());
    return r;
}

CsvImportResult importGagesCsv(Project& p, const std::string& text, bool replace)
{
    CsvImportResult r;
    const auto rows = csvParse(stripBom(text));
    size_t headerIdx = 0;
    while (headerIdx < rows.size() && isCommentRow(rows[headerIdx]))
        ++headerIdx;
    if (headerIdx >= rows.size()) {
        r.errors.push_back("gages.csv: header manquant");
        return r;
    }

    const ColMap cols = mapHeader(rows[headerIdx]);
    const int iDesc   = colIndex(cols, {"description", "texte"});
    const int iNumber = colIndex(cols, {"number", "numero", "n"});
    const int iHp     = colIndex(cols, {"hp"});
    const int iRate   = colIndex(cols, {"rate"});
    if (iDesc < 0) {
        r.errors.push_back("gages.csv: colonne description|texte requise");
        return r;
    }

    std::vector<Gage> imported;
    for (size_t ri = headerIdx + 1; ri < rows.size(); ++ri) {
        const auto& row = rows[ri];
        if (isCommentRow(row)) {
            ++r.skipped;
            continue;
        }
        const std::string description = trim(cellAt(row, iDesc));
        if (description.empty()) {
            ++r.skipped;
            continue;
        }

        Gage g;
        g.description = description;
        g.number = 1;
        g.hp = 5;
        g.rate = 100;
        if (iNumber >= 0) {
            int v = 0;
            if (parseIntField(cellAt(row, iNumber), v))
                g.number = std::max(1, v);
            else if (!trim(cellAt(row, iNumber)).empty()) {
                r.errors.push_back("ligne " + std::to_string(ri + 1) + ": number invalide");
                ++r.skipped;
                continue;
            }
        }
        if (iHp >= 0) {
            int v = 0;
            if (parseIntField(cellAt(row, iHp), v))
                g.hp = std::max(0, v);
            else if (!trim(cellAt(row, iHp)).empty()) {
                r.errors.push_back("ligne " + std::to_string(ri + 1) + ": hp invalide");
                ++r.skipped;
                continue;
            }
        }
        if (iRate >= 0) {
            int v = 0;
            if (parseIntField(cellAt(row, iRate), v))
                g.rate = clampRate(v);
            else if (!trim(cellAt(row, iRate)).empty()) {
                r.errors.push_back("ligne " + std::to_string(ri + 1) + ": rate invalide");
                ++r.skipped;
                continue;
            }
        }
        imported.push_back(std::move(g));
        ++r.added;
    }

    if (replace)
        p.gages = std::move(imported);
    else
        p.gages.insert(p.gages.end(), imported.begin(), imported.end());
    return r;
}

std::string exportPhrasesCsv(const Project& p)
{
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"label", "points", "rate"});
    for (const auto& c : p.cases) {
        rows.push_back({
            c.label,
            std::to_string(c.points),
            std::to_string(c.rate),
        });
    }
    return csvWrite(rows);
}

std::string exportGagesCsv(const Project& p)
{
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"description", "number", "hp", "rate"});
    for (const auto& g : p.gages) {
        rows.push_back({
            g.description,
            std::to_string(g.number),
            std::to_string(g.hp),
            std::to_string(g.rate),
        });
    }
    return csvWrite(rows);
}

} // namespace core
