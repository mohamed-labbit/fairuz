#pragma once

#include "macros.hpp"
#include "types/string.hpp"
#include <omp.h>
#include <string>
#include <vector>

namespace mylang {
namespace util {

static inline bool isWhitespace(wchar_t ch)
{
    return ch == u' ' || ch == u'\t' || ch == u'\r';
}

static inline bool const isOperator(wchar_t const ch)
{
    switch (ch) {
    case '=':
    case '<':
    case '>':
    case '!':
    case '+':
    case '-':
    case '|':
    case '&':
    case '*':
    case '/':
        return true;
    default:
        return false;
    }
}

static inline bool isSymbol(wchar_t ch)
{
    static constexpr std::string_view syms = ",[]().:";
    return syms.find(ch) != std::string_view::npos;
}

static inline bool isalphaArabic(wchar_t const c)
{
    switch (c) {
    case 0x0621:
    case 0x0622:
    case 0x0623:
    case 0x0624:
    case 0x0625:
    case 0x0626:
    case 0x0627:
    case 0x0628:
    case 0x0629:
    case 0x062A:
    case 0x062B:
    case 0x062C:
    case 0x062D:
    case 0x062E:
    case 0x062F:
    case 0x0630:
    case 0x0631:
    case 0x0632:
    case 0x0633:
    case 0x0634:
    case 0x0635:
    case 0x0636:
    case 0x0637:
    case 0x0638:
    case 0x0639:
    case 0x063A:
    case 0x0641:
    case 0x0642:
    case 0x0643:
    case 0x0644:
    case 0x0645:
    case 0x0646:
    case 0x0647:
    case 0x0648:
    case 0x0649:
    case 0x064A:
        return true;
    default:
        return false;
    }

    return (c >= 0x0600 && c <= 0x06FF);
}

static uint32_t decode_utf8_at(StringRef const& buf, std::size_t byte_pos, std::size_t* out_bytes)
{
    if (byte_pos >= buf.len())
        throw std::runtime_error("UTF8 decode past end of buffer");

    unsigned char const* p = (unsigned char const*)buf.data() + byte_pos;
    unsigned char const* end = (unsigned char const*)buf.data() + buf.len();

    unsigned char c = *p;

    if (c < 0x80) {
        *out_bytes = 1;
        return c;
    }

    if ((c & 0xE0) == 0xC0) {
        if (p + 1 >= end)
            throw std::runtime_error("UTF8 truncated: incomplete 2-byte sequence");

        if ((p[1] & 0xC0) != 0x80)
            throw std::runtime_error("Invalid UTF-8: bad continuation byte");

        *out_bytes = 2;
        uint32_t result = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);

        if (result < 0x80)
            throw std::runtime_error("Invalid UTF-8: overlong 2-byte sequence");

        return result;
    }

    if ((c & 0xF0) == 0xE0) {
        if (p + 2 >= end)
            throw std::runtime_error("UTF8 truncated: incomplete 3-byte sequence");

        if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80)
            throw std::runtime_error("Invalid UTF-8: bad continuation byte");

        *out_bytes = 3;
        uint32_t result = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);

        if (result < 0x800)
            throw std::runtime_error("Invalid UTF-8: overlong 3-byte sequence");

        if (result >= 0xD800 && result <= 0xDFFF)
            throw std::runtime_error("Invalid UTF-8: surrogate pair");

        return result;
    }

    if ((c & 0xF8) == 0xF0) {
        if (p + 3 >= end)
            throw std::runtime_error("UTF8 truncated: incomplete 4-byte sequence");

        if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80)
            throw std::runtime_error("Invalid UTF-8: bad continuation byte");

        *out_bytes = 4;
        uint32_t result = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);

        if (result < 0x10000)
            throw std::runtime_error("Invalid UTF-8: overlong 4-byte sequence");

        if (result > 0x10FFFF)
            throw std::runtime_error("Invalid UTF-8: codepoint out of range");

        return result;
    }

    throw std::runtime_error("Invalid UTF-8: invalid start byte");
}

static void configureLocale()
{
    try {
        std::locale::global(std::locale("ar_SA.UTF-8"));
    } catch (std::runtime_error const&) {
        std::locale::global(std::locale::classic());
    }
}

static std::size_t encode_utf8(uint32_t codepoint, unsigned char* out_bytes)
{
    if (codepoint < 0x80) {
        out_bytes[0] = static_cast<unsigned char>(codepoint);
        return 1;
    } else if (codepoint < 0x800) {
        out_bytes[0] = static_cast<unsigned char>(0xC0 | (codepoint >> 6));
        out_bytes[1] = static_cast<unsigned char>(0x80 | (codepoint & 0x3F));
        return 2;
    } else if (codepoint < 0x10000) {
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
            throw std::runtime_error("Invalid codepoint: UTF-16 surrogate");

        out_bytes[0] = static_cast<unsigned char>(0xE0 | (codepoint >> 12));
        out_bytes[1] = static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F));
        out_bytes[2] = static_cast<unsigned char>(0x80 | (codepoint & 0x3F));
        return 3;
    } else if (codepoint <= 0x10FFFF) {
        out_bytes[0] = static_cast<unsigned char>(0xF0 | (codepoint >> 18));
        out_bytes[1] = static_cast<unsigned char>(0x80 | ((codepoint >> 12) & 0x3F));
        out_bytes[2] = static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F));
        out_bytes[3] = static_cast<unsigned char>(0x80 | (codepoint & 0x3F));
        return 4;
    } else
        throw std::runtime_error("Invalid codepoint: exceeds Unicode range");
}

static StringRef encode_utf8_str(uint32_t codepoint)
{
    unsigned char bytes[4];
    std::size_t len = encode_utf8(codepoint, bytes);
    return StringRef(reinterpret_cast<char*>(bytes)).truncate(len);
}

} // util
} // mylang
