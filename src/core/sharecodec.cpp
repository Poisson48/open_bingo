#include "sharecodec.h"
#include "jsoncodec.h"

#include <zlib.h>

#include <array>
#include <cstring>
#include <vector>

namespace core {
namespace {

std::string base64Encode(const std::vector<unsigned char>& data, bool urlSafe)
{
    static const char* stdChars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static const char* urlChars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string out;
    const char* table = urlSafe ? urlChars : stdChars;
    int val = 0;
    int valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
    if (!urlSafe) {
        while (out.size() % 4)
            out.push_back('=');
    }
    return out;
}

std::vector<unsigned char> base64Decode(const std::string& in)
{
    auto decodeChar = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };

    std::vector<unsigned char> out;
    int val = 0;
    int valb = -8;
    for (char c : in) {
        if (c == '=')
            break;
        const int d = decodeChar(c);
        if (d < 0)
            continue;
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::vector<unsigned char> deflateRaw(const std::string& input)
{
    z_stream strm{};
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return {};

    std::vector<unsigned char> out;
    out.resize(input.size() + 64);
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    strm.avail_in = static_cast<uInt>(input.size());
    strm.next_out = out.data();
    strm.avail_out = static_cast<uInt>(out.size());

    while (strm.avail_in > 0) {
        if (strm.avail_out == 0) {
            const size_t old = out.size();
            out.resize(old * 2);
            strm.next_out = out.data() + old;
            strm.avail_out = static_cast<uInt>(old);
        }
        if (deflate(&strm, Z_FINISH) == Z_STREAM_ERROR)
            break;
    }
    deflateEnd(&strm);
    out.resize(strm.total_out);
    return out;
}

std::string inflateRaw(const std::vector<unsigned char>& input)
{
    z_stream strm{};
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
        return {};

    std::vector<unsigned char> out;
    out.resize(input.size() * 4 + 64);
    strm.next_in = const_cast<Bytef*>(input.data());
    strm.avail_in = static_cast<uInt>(input.size());
    strm.next_out = out.data();
    strm.avail_out = static_cast<uInt>(out.size());

    int ret = Z_OK;
    while (ret != Z_STREAM_END) {
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR)
            break;
        if (strm.avail_out == 0) {
            const size_t old = out.size();
            out.resize(old * 2);
            strm.next_out = out.data() + old;
            strm.avail_out = static_cast<uInt>(old);
        }
    }
    inflateEnd(&strm);
    if (ret != Z_STREAM_END)
        return {};
    return std::string(reinterpret_cast<char*>(out.data()), strm.total_out);
}

} // namespace

std::string ShareCodec::compress(const std::string& json)
{
    const auto compressed = deflateRaw(json);
    if (compressed.empty())
        return "b64:" + base64Encode(std::vector<unsigned char>(json.begin(), json.end()), false);
    return "z:" + base64Encode(compressed, true);
}

std::string ShareCodec::decompress(const std::string& encoded, bool* ok)
{
    if (encoded.rfind("b64:", 0) == 0) {
        const auto bytes = base64Decode(encoded.substr(4));
        if (ok) *ok = true;
        return std::string(bytes.begin(), bytes.end());
    }
    if (encoded.rfind("z:", 0) != 0) {
        if (ok) *ok = false;
        return {};
    }
    const auto bytes = base64Decode(encoded.substr(2));
    const auto out = inflateRaw(bytes);
    if (out.empty()) {
        if (ok) *ok = false;
        return {};
    }
    if (ok) *ok = true;
    return out;
}

std::string ShareCodec::buildShareUrl(const Project& project)
{
    const std::string json = JsonCodec::projectToJson(project, false);
    return "openbingo://share#s=" + compress(json);
}

Project ShareCodec::parseSharePayload(const std::string& hashOrPayload, bool* ok)
{
    std::string payload = hashOrPayload;
    const auto pos = payload.find("s=");
    if (pos != std::string::npos)
        payload = payload.substr(pos + 2);

    bool decOk = false;
    const std::string json = decompress(payload, &decOk);
    if (!decOk) {
        if (ok) *ok = false;
        return {};
    }
    return JsonCodec::projectFromJson(json, ok);
}

} // namespace core
