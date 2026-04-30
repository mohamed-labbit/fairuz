#ifndef UTIL_HPP
#define UTIL_HPP

#include "diagnostic.hpp"
#include "macros.hpp"
#include "string.hpp"
#include <cstdint>
#include <string>

namespace fairuz::util {

using ErrorCode = diagnostic::errc::general::Code;

static inline bool is_whitespace(u32 const& ch) { return ch == u' ' || ch == u'\t' || ch == u'\r'; }

static inline bool const is_operator(u32 const& ch)
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

static inline bool isalpha_arabic(u32 const c)
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

static u32 decode_utf8_at(Fa_StringRef const& buf, size_t const byte_pos, u64* out_bytes)
{
    if (byte_pos >= buf.len())
        diagnostic::emit(ErrorCode::INTERNAL_ERROR, "UTF8 decode past end of buffer", diagnostic::Severity::FATAL);

    unsigned char const* p = (unsigned char const*)buf.data() + byte_pos;
    unsigned char const* end = (unsigned char const*)buf.data() + buf.len();

    unsigned char const c = *p;

    if (c < 0x80) {
        *out_bytes = 1;
        return c;
    }

    if ((c & 0xE0) == 0xC0) {
        if (p + 1 >= end)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "UTF8 truncated: incomplete 2-byte sequence", diagnostic::Severity::FATAL);

        if ((p[1] & 0xC0) != 0x80)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: bad continuation byte", diagnostic::Severity::FATAL);

        *out_bytes = 2;
        u32 const result = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);

        if (result < 0x80)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: overlong 2-byte sequence", diagnostic::Severity::FATAL);

        return result;
    }

    if ((c & 0xF0) == 0xE0) {
        if (p + 2 >= end)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "UTF8 truncated: incomplete 3-byte sequence", diagnostic::Severity::FATAL);

        if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: bad continuation byte", diagnostic::Severity::FATAL);

        *out_bytes = 3;
        u32 const result = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);

        if (result < 0x800)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: overlong 3-byte sequence", diagnostic::Severity::FATAL);

        if (result >= 0xD800 && result <= 0xDFFF)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: surrogate pair", diagnostic::Severity::FATAL);

        return result;
    }

    if ((c & 0xF8) == 0xF0) {
        if (p + 3 >= end)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "UTF8 truncated: incomplete 4-byte sequence", diagnostic::Severity::FATAL);

        if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: bad continuation byte", diagnostic::Severity::FATAL);

        *out_bytes = 4;
        u32 const result = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);

        if (result < 0x10000)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: overlong 4-byte sequence", diagnostic::Severity::FATAL);

        if (result > 0x10FFFF)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: cp out of range", diagnostic::Severity::FATAL);

        return result;
    }

    diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: invalid start byte", diagnostic::Severity::FATAL);
    return -1; // unreachable
}

static void configure_locale()
{
    try {
        std::locale::global(std::locale("ar_SA.UTF-8"));
    } catch (std::runtime_error const&) {
        std::locale::global(std::locale::classic());
    }
}

static size_t utf8_codepoint_size(u32 const cp)
{
    if (cp < 0x80)
        return 1;
    if (cp < 0x800)
        return 2;
    if (cp < 0x10000) {
        if (cp >= 0xD800 && cp <= 0xDFFF)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid cp: UTF-16 surrogate", diagnostic::Severity::FATAL);
        return 3;
    }
    if (cp <= 0x10FFFF)
        return 4;

    diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid cp: exceeds Unicode range", diagnostic::Severity::FATAL);
    return -1; // unreachable
}

static size_t encode_utf8(u32 const cp, unsigned char* out_bytes)
{
    if (cp < 0x80) {
        out_bytes[0] = static_cast<unsigned char>(cp);
        out_bytes[1] = '\0';
        return 1;
    }
    if (cp < 0x800) {
        out_bytes[0] = static_cast<unsigned char>(0xC0 | (cp >> 6));
        out_bytes[1] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
        out_bytes[2] = '\0';
        return 2;
    }
    if (cp < 0x10000) {
        if (cp >= 0xD800 && cp <= 0xDFFF)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid cp: UTF-16 surrogate", diagnostic::Severity::FATAL);

        out_bytes[0] = static_cast<unsigned char>(0xE0 | (cp >> 12));
        out_bytes[1] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F));
        out_bytes[2] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
        out_bytes[3] = '\0';
        return 3;
    }
    if (cp <= 0x10FFFF) {
        out_bytes[0] = static_cast<unsigned char>(0xF0 | (cp >> 18));
        out_bytes[1] = static_cast<unsigned char>(0x80 | ((cp >> 12) & 0x3F));
        out_bytes[2] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F));
        out_bytes[3] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
        out_bytes[4] = '\0';
        return 4;
    }

    diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid cp: exceeds Unicode range", diagnostic::Severity::FATAL);
    return -1; // unreachable
}

static Fa_StringRef encode_utf8_str(u32 const cp)
{
    unsigned char bytes[5];
    size_t const len = encode_utf8(cp, bytes);
    return Fa_StringRef(reinterpret_cast<char*>(bytes)).truncate(len);
}

static bool is_arab_digit(u32 const cp)
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

static u8 arab_digit_to_canon(u32 const cp)
{
    switch (cp) {
    case u'٠':
        return 0;
    case u'١':
        return 1;
    case u'٢':
        return 2;
    case u'٣':
        return 3;
    case u'٤':
        return 4;
    case u'٥':
        return 5;
    case u'٦':
        return 6;
    case u'٧':
        return 7;
    case u'٨':
        return 8;
    case u'٩':
        return 9;
    default:
        return 0xFF;
    }
}

static i64 parse_integer_literal(Fa_StringRef const& literal, int base)
{
    if (base == -1) /*false call*/
        return INT16_MIN;

    size_t i = 0;
    bool negative = false;

    if (literal.at(i) == '-') {
        negative = true;
        i += 1;
    }

    if (literal.slice(i, 2) == "0x" || literal.slice(i, 2) == "0X"
        || literal.slice(i, 2) == "0b" || literal.slice(i, 2) == "0B"
        || literal.slice(i, 2) == "0o" || literal.slice(i, 2) == "0O")
        i += 2;
    else if (literal.at(i) == '0' && literal.len() > i + 1)
        i += 1;

    i64 value = 0;
    u64 bytes = 0, out_bytes = 0;
    bool is_arab = false;

    for (; i < literal.len(); i += 1) {
        u32 const cp = decode_utf8_at(literal, bytes, &out_bytes);

        bytes += out_bytes;

        if (cp == '\'')
            continue;

        int digit = 0;

        if (::isdigit(cp))
            digit = cp - '0';
        else if (is_arab_digit(cp))
            digit = arab_digit_to_canon(cp);
        else if (::isalpha(cp))
            digit = ::tolower(cp) - 'a' + 10;
        else
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Invalid digit", diagnostic::Severity::FATAL);

        if (digit >= base)
            diagnostic::emit(ErrorCode::INTERNAL_ERROR, "Digit out of range for base (base=" + std::to_string(base) + ", digit=" + std::to_string(digit) + ")", diagnostic::Severity::FATAL);

        value = value * base + digit;
    }

    return negative ? -value : value;
}

static bool is_integer_value(f64 d, i64& out)
{
    if (!std::isfinite(d))
        return false;
    auto iv = static_cast<i64>(d);
    if (static_cast<f64>(iv) != d)
        return false;
    out = iv;
    return true;
}

} // namespace fairuz::util

#endif // UTIL_HPP
