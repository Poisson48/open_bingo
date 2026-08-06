#pragma once

#include "bingotypes.h"

#include <string>

namespace core {

class ShareCodec
{
public:
    // Compress project JSON for URL sharing: "z:" + base64url(deflate) or "b64:" + base64
    static std::string compress(const std::string& json);
    static std::string decompress(const std::string& encoded, bool* ok = nullptr);

    static std::string buildShareUrl(const Project& project);
    static Project     parseSharePayload(const std::string& hashOrPayload, bool* ok = nullptr);
};

} // namespace core
