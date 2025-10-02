#pragma once

#include <string>


inline bool wisdigit(const wchar_t c) {
    return (c == L'0' || c == L'1' || c == L'2' || c == L'3' || c == L'4' || c == L'5' || c == L'6' || c == L'7'
            || c == L'8' || c == L'9');
}

inline bool isalpha_arabic(const wchar_t c) { return (c >= 0x0600 && c <= 0x06FF); }

// Very basic UTF-32 (wchar_t) → UTF-8 converter
inline std::string to_utf8(const std::wstring& wstr) {
    std::string result;
    result.reserve(wstr.size());  // guess

    for (wchar_t wc : wstr)
    {
        uint32_t c = static_cast<uint32_t>(wc);

        if (c <= 0x7F)
        {
            result.push_back(static_cast<char>(c));
        }
        else if (c <= 0x7FF)
        {
            result.push_back(static_cast<char>(0xC0 | ((c >> 6) & 0x1F)));
            result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
        else if (c <= 0xFFFF)
        {
            result.push_back(static_cast<char>(0xE0 | ((c >> 12) & 0x0F)));
            result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
        else if (c <= 0x10FFFF)
        {
            result.push_back(static_cast<char>(0xF0 | ((c >> 18) & 0x07)));
            result.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
        else
        {
            throw std::runtime_error("Invalid Unicode code point");
        }
    }

    return result;
}

struct WStringHash
{
    std::size_t operator()(const std::wstring& wstr) const noexcept { return fnv1a_hash(wstr); }

   private:
    std::size_t fnv1a_hash(const std::wstring& wstr) const noexcept {
        std::size_t hash = 1469598103934665603ULL;
        for (wchar_t ch : wstr)
        {
            hash ^= static_cast<std::size_t>(ch);
            hash *= 1099511628211ULL;
        }

        return hash;
    }
};

inline static std::size_t utf8_size(wchar_t wc) {
    unsigned int cp = static_cast<unsigned int>(wc);

    if (cp <= 0x7F)
    {
        return 1;  // 1-byte UTF-8 (ASCII)
    }
    else if (cp <= 0x7FF)
    {
        return 2;  // 2-byte UTF-8
    }
    else if (cp <= 0xFFFF)
    {
        return 3;  // 3-byte UTF-8
    }
    else if (cp <= 0x10FFFF)
    {
        return 4;  // 4-byte UTF-8
    }
    else
    {
        throw std::runtime_error("Invalid Unicode code point");
    }
}