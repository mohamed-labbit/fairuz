#ifndef _UTIL_HPP
#define _UTIL_HPP

#include "macros.hpp"
#include "string.hpp"
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

static uint32_t decode_utf8_at(StringRef const& buf, size_t byte_pos, uint64_t* out_bytes)
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
            throw std::runtime_error("Invalid UTF-8: cp out of range");

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

static size_t encode_utf8(uint32_t cp, unsigned char* out_bytes)
{
    if (cp < 0x80) {
        out_bytes[0] = static_cast<unsigned char>(cp);
        return 1;
    } else if (cp < 0x800) {
        out_bytes[0] = static_cast<unsigned char>(0xC0 | (cp >> 6));
        out_bytes[1] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000) {
        if (cp >= 0xD800 && cp <= 0xDFFF)
            throw std::runtime_error("Invalid cp: UTF-16 surrogate");

        out_bytes[0] = static_cast<unsigned char>(0xE0 | (cp >> 12));
        out_bytes[1] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F));
        out_bytes[2] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
        return 3;
    } else if (cp <= 0x10FFFF) {
        out_bytes[0] = static_cast<unsigned char>(0xF0 | (cp >> 18));
        out_bytes[1] = static_cast<unsigned char>(0x80 | ((cp >> 12) & 0x3F));
        out_bytes[2] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F));
        out_bytes[3] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
        return 4;
    } else
        throw std::runtime_error("Invalid cp: exceeds Unicode range");
}

static StringRef encode_utf8_str(uint32_t cp)
{
    unsigned char bytes[4];
    size_t len = encode_utf8(cp, bytes);
    return StringRef(reinterpret_cast<char*>(bytes)).truncate(len);
}

static bool isArabDigit(uint32_t const cp)
{
    switch (cp) {
    case u'٠': // 0
    case u'١': // 1
    case u'٢': // 2
    case u'٣': // 3
    case u'٤': // 4
    case u'٥': // 5
    case u'٦': // 6
    case u'٧': // 7
    case u'٨': // 8
    case u'٩': // 9
        return true;
    default:
        return false;
    }
}

static int64_t parseIntegerLiteral(StringRef const& literal)
{
    int base = 10;
    size_t i = 0;
    bool negative = false;

    if (literal[i] == '-') {
        negative = true;
        ++i;
    }

    if (literal.slice(i, 2) == "0x" || literal.slice(i, 2) == "0X") {
        base = 16;
        i += 2;
    } else if (literal.slice(i, 2) == "0b" || literal.slice(i, 2) == "0B") {
        base = 2;
        i += 2;
    } else if (literal[i] == '0' && literal.len() > i + 1) {
        base = 8;
        ++i;
    }

    int64_t value = 0;

    for (; i < literal.len(); ++i) {
        char c = literal[i];

        if (c == '\'')
            continue; // ignore digit separators

        int digit;

        if (::isdigit(c))
            digit = c - '0';
        else if (std::isalpha(c))
            digit = ::tolower(c) - 'a' + 10;
        else
            throw std::invalid_argument("Invalid digit");

        if (digit >= base)
            throw std::invalid_argument("Digit out of range for base");

        value = value * base + digit;
    }

    return negative ? -value : value;
}

static bool isIntegerValue(double d, int64_t& out)
{
    if (!std::isfinite(d))
        return false;
    int64_t iv = static_cast<int64_t>(d);
    if (static_cast<double>(iv) != d)
        return false;
    out = iv;
    return true;
}

} // util
} // mylang

#endif // _UTIL_HPP
