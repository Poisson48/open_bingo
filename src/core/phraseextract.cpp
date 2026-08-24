#include "phraseextract.h"

#include <cctype>
#include <string_view>
#include <unordered_set>

namespace core {
namespace {

std::string toLowerAscii(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trimCopy(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return std::string(s);
}

} // namespace

std::vector<std::string> suggestBingoPhrases(const std::vector<Cue>& cues,
                                            const PhraseSuggestOptions& opt)
{
    std::vector<std::string> out;
    std::unordered_set<std::string> seen;

    for (const auto& c : cues) {
        if (opt.skipSfx && c.likelySfx)
            continue;

        std::string t = trimCopy(c.plain.empty() ? c.text : c.plain);
        if (t.empty())
            continue;
        if (opt.maxLen > 0 && static_cast<int>(t.size()) > opt.maxLen)
            continue;

        if (opt.dedupe) {
            const std::string key = toLowerAscii(t);
            if (!seen.insert(key).second)
                continue;
        }

        out.push_back(std::move(t));
        if (opt.maxPhrases > 0 && static_cast<int>(out.size()) >= opt.maxPhrases)
            break;
    }
    return out;
}

} // namespace core
