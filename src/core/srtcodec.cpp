#include "srtcodec.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>

namespace core {
namespace {

std::string_view trimView(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
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

std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::string cur;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\r') {
            lines.push_back(cur);
            cur.clear();
            if (i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
        } else if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    lines.push_back(cur);
    return lines;
}

bool parseTimeToken(std::string_view tok, int& msOut)
{
    // HH:MM:SS,mmm | HH:MM:SS.mmm | MM:SS.mmm (VTT)
    tok = trimView(tok);
    if (tok.empty())
        return false;

    int parts[3] = {0, 0, 0};
    int nParts = 0;
    size_t i = 0;
    while (i < tok.size() && nParts < 3) {
        if (!std::isdigit(static_cast<unsigned char>(tok[i])))
            return false;
        int v = 0;
        while (i < tok.size() && std::isdigit(static_cast<unsigned char>(tok[i]))) {
            v = v * 10 + (tok[i] - '0');
            ++i;
        }
        parts[nParts++] = v;
        if (i < tok.size() && tok[i] == ':') {
            ++i;
            continue;
        }
        break;
    }
    if (nParts < 2)
        return false;

    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    if (nParts == 3) {
        hours = parts[0];
        minutes = parts[1];
        seconds = parts[2];
    } else {
        minutes = parts[0];
        seconds = parts[1];
    }

    int millis = 0;
    if (i < tok.size() && (tok[i] == ',' || tok[i] == '.')) {
        ++i;
        int digits = 0;
        while (i < tok.size() && std::isdigit(static_cast<unsigned char>(tok[i])) && digits < 3) {
            millis = millis * 10 + (tok[i] - '0');
            ++i;
            ++digits;
        }
        while (digits < 3) {
            millis *= 10;
            ++digits;
        }
        while (i < tok.size() && std::isdigit(static_cast<unsigned char>(tok[i])))
            ++i;
    }
    if (i != tok.size())
        return false;

    msOut = ((hours * 60 + minutes) * 60 + seconds) * 1000 + millis;
    return true;
}

bool parseTimingLine(std::string_view line, int& startMs, int& endMs)
{
    line = trimView(line);
    const auto arrow = line.find("-->");
    if (arrow == std::string_view::npos)
        return false;

    const auto left = trimView(line.substr(0, arrow));
    auto right = trimView(line.substr(arrow + 3));
    const auto sp = right.find_first_of(" \t");
    if (sp != std::string_view::npos)
        right = trimView(right.substr(0, sp));

    return parseTimeToken(left, startMs) && parseTimeToken(right, endMs);
}

std::string joinCueText(const std::vector<std::string>& textLines)
{
    std::string out;
    for (const auto& l : textLines) {
        if (l.empty())
            continue;
        if (!out.empty())
            out.push_back('\n');
        out += l;
    }
    return out;
}

void pushCue(SrtParseResult& r, int startMs, int endMs, const std::vector<std::string>& textLines)
{
    const std::string raw = joinCueText(textLines);
    if (trimView(raw).empty())
        return;

    Cue c;
    c.startMs = startMs;
    c.endMs = endMs;
    c.text = raw;
    c.plain = cuePlainText(raw);
    c.likelySfx = cueLooksLikeSfx(c.plain);
    r.cues.push_back(std::move(c));
}

SrtParseResult parseBlocks(const std::vector<std::string>& lines, bool vtt)
{
    SrtParseResult r;
    size_t i = 0;
    if (vtt) {
        while (i < lines.size() && !trimView(lines[i]).empty())
            ++i;
        while (i < lines.size() && trimView(lines[i]).empty())
            ++i;
    }

    while (i < lines.size()) {
        while (i < lines.size() && trimView(lines[i]).empty())
            ++i;
        if (i >= lines.size())
            break;

        int startMs = 0;
        int endMs = 0;
        if (!parseTimingLine(lines[i], startMs, endMs)) {
            if (i + 1 < lines.size() && parseTimingLine(lines[i + 1], startMs, endMs)) {
                ++i; // skip index / cue id
            } else {
                r.errors.push_back("invalid timing near line " + std::to_string(i + 1));
                ++i;
                continue;
            }
        }

        ++i;
        std::vector<std::string> textLines;
        while (i < lines.size() && !trimView(lines[i]).empty()) {
            if (vtt && trimView(lines[i]).rfind("NOTE", 0) == 0)
                break;
            textLines.push_back(lines[i]);
            ++i;
        }
        pushCue(r, startMs, endMs, textLines);
    }
    return r;
}

} // namespace

std::string cuePlainText(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());

    for (size_t i = 0; i < raw.size();) {
        const char c = raw[i];
        if (c == '<') {
            const auto end = raw.find('>', i + 1);
            if (end == std::string::npos) {
                out.push_back(c);
                ++i;
            } else {
                i = end + 1;
            }
            continue;
        }
        if (c == '{') {
            const auto end = raw.find('}', i + 1);
            if (end == std::string::npos) {
                out.push_back(c);
                ++i;
            } else {
                i = end + 1;
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            if (!out.empty() && out.back() != ' ')
                out.push_back(' ');
            ++i;
            continue;
        }
        out.push_back(c);
        ++i;
    }

    std::string collapsed;
    collapsed.reserve(out.size());
    bool prevSpace = false;
    for (char ch : out) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!prevSpace) {
                collapsed.push_back(' ');
                prevSpace = true;
            }
        } else {
            collapsed.push_back(ch);
            prevSpace = false;
        }
    }
    const auto v = trimView(collapsed);
    return std::string(v);
}

bool cueLooksLikeSfx(const std::string& plain)
{
    const auto t = trimView(plain);
    if (t.empty())
        return true;

    const char open = t.front();
    char close = '\0';
    if (open == '[')
        close = ']';
    else if (open == '(')
        close = ')';
    else
        return false;

    return t.back() == close;
}

SrtParseResult parseSrt(const std::string& textIn)
{
    const std::string text = stripBom(textIn);
    const auto lines = splitLines(text);

    bool vtt = false;
    if (!lines.empty()) {
        const auto first = trimView(lines.front());
        if (first.rfind("WEBVTT", 0) == 0)
            vtt = true;
    }

    return parseBlocks(lines, vtt);
}

SrtParseResult parseSrtFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        SrtParseResult r;
        r.errors.push_back("cannot open: " + path);
        return r;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return parseSrt(ss.str());
}

} // namespace core
