#include "fchars.hpp"
#include "fmacros.hpp"
#include "fstring.hpp"
#include "futf8.hpp"

#include <cmath>

namespace fairuz::util {

static u8 arab_digit_to_canon(u32 const cp)
{
    switch (cp) {
    case u'٠': return 0;
    case u'١': return 1;
    case u'٢': return 2;
    case u'٣': return 3;
    case u'٤': return 4;
    case u'٥': return 5;
    case u'٦': return 6;
    case u'٧': return 7;
    case u'٨': return 8;
    case u'٩': return 9;
    default: return 0xFF;
    }
}

static inline bool try_parse_float_literal(StringRef const& literal, f64& result);

static inline bool try_parse_indoarab_float_literal(StringRef const& literal, f64& result)
{
    StringRef ascii_copy = "";
    for (u64 i = 0; i < literal.len();) {
        u64 cp_bytes = 0;
        u32 const cp = decode_utf8_at(literal, i, &cp_bytes);
        i += cp_bytes;
        switch (cp) {
        case u'-': ascii_copy += "-"; break;
        case u'.': ascii_copy += "."; break;
        case u'٠': ascii_copy += "0"; break;
        case u'١': ascii_copy += "1"; break;
        case u'٢': ascii_copy += "2"; break;
        case u'٣': ascii_copy += "3"; break;
        case u'٤': ascii_copy += "4"; break;
        case u'٥': ascii_copy += "5"; break;
        case u'٦': ascii_copy += "6"; break;
        case u'٧': ascii_copy += "7"; break;
        case u'٨': ascii_copy += "8"; break;
        case u'٩': ascii_copy += "9"; break;
        default:
            if (cp >= '0' && cp <= '9')
                diagnostic::fatal_error(ErrorCode::INVALID_CHARACTER, "Mixing indo-arabic and ascii digits is not allowed");
            diagnostic::fatal_error(ErrorCode::INVALID_CHARACTER, "Unexpected character");
        }
    }

    return try_parse_float_literal(ascii_copy, result);
}

static inline bool try_parse_float_literal(StringRef const& literal, f64& result)
{
    if (literal.empty())
        return false;
    size_t i = 0;

    if (literal.at(i) == '-') {
        i++;
    }

    u64 bytes = i;
    u64 cp_bytes = 0;
    u32 len = literal.len();

    if (bytes < len && is_arab_digit(decode_utf8_at(literal, i, &cp_bytes)))
        return try_parse_indoarab_float_literal(literal, result);

    bool saw_digit = false;

    /// consume leading zeros
    while (bytes < len && (decode_utf8_at(literal, bytes, &cp_bytes) == '0')) {
        bytes += cp_bytes;
        saw_digit = true;
    }

    // integer part
    while (bytes < len) {
        u32 const cp = decode_utf8_at(literal, bytes, &cp_bytes);
        bytes += cp_bytes;

        if (cp == '\'' || cp == '_')
            continue;

        i32 digit = -1;
        if (cp >= '0' && cp <= '9')
            digit = static_cast<i32>(cp - '0');

        if (digit < 0) {
            bytes -= cp_bytes; // put back the terminating character
            break;
        }

        saw_digit = true;
    }

    // fractional part
    if (bytes < len && decode_utf8_at(literal, bytes, &cp_bytes) == '.') {
        bytes += cp_bytes;
        while (bytes < len) {
            u32 const cp = decode_utf8_at(literal, bytes, &cp_bytes);
            bytes += cp_bytes;

            if (cp == '\'' || cp == '_')
                continue;

            i32 digit = -1;
            if (cp >= '0' && cp <= '9')
                digit = static_cast<i32>(cp - '0');

            if (digit < 0) {
                bytes -= cp_bytes;
                break;
            }

            saw_digit = true;
        }
    }

    if (!saw_digit)
        return false; // "-", ".", "-." etc. are not valid numbers

    // exponent part
    if (bytes < len) {
        u32 const cp = decode_utf8_at(literal, bytes, &cp_bytes);

        if (cp == 'e' || cp == 'E') {
            u64 exp_bytes = bytes + cp_bytes;
            if (exp_bytes < len) {
                u64 sign_cp_bytes = 0;
                u32 const sign_cp = decode_utf8_at(literal, exp_bytes, &sign_cp_bytes);
                if (sign_cp == '+' || sign_cp == '-') {
                    exp_bytes += sign_cp_bytes;
                }
            }

            u64 const exp_digits_start = exp_bytes;
            while (exp_bytes < len) {
                u64 exp_cp_bytes = 0;
                u32 const exp_cp = decode_utf8_at(literal, exp_bytes, &exp_cp_bytes);

                if (exp_cp == '\'' || exp_cp == '_') {
                    exp_bytes += exp_cp_bytes;
                    continue;
                }

                i32 digit = -1;
                if (exp_cp >= '0' && exp_cp <= '9')
                    digit = static_cast<i32>(exp_cp - '0');

                if (digit < 0)
                    break;

                exp_bytes += exp_cp_bytes;
            }

            if (exp_bytes == exp_digits_start)
                return false; // 'e'/'E' with no digits after it

            bytes = exp_bytes;
        }
    }

    if (bytes != len)
        return false; // trailing garbage the loops didn't consume

    // Convert the complete spelling once; accumulating digits in a double
    // introduces rounding errors before the literal has even been parsed.
    std::string normalized;
    normalized.reserve(literal.len());
    for (size_t offset = 0; offset < literal.len(); ++offset) {
        char ch = literal.at(offset);
        if (ch != '\'' && ch != '_')
            normalized.push_back(ch);
    }
    auto parsed = std::from_chars(normalized.data(), normalized.data() + normalized.size(), result);
    return parsed.ec == std::errc { } && parsed.ptr == normalized.data() + normalized.size();
}

static inline bool try_parse_integer_literal(StringRef const& literal, int base, i64& result)
{
    if (base == -1) /*false call*/
        return false;

    size_t i = 0;
    bool negative = false;

    if (literal.at(i) == '-') {
        negative = true;
        i++;
    }

    auto prefix = literal.slice(i, 2);

    if (prefix == "0x" || prefix == "0X"
        || prefix == "0b" || prefix == "0B"
        || prefix == "0o" || prefix == "0O")
        i += 2;
    else if (literal.at(i) == '0' && literal.len() > i + 1)
        i++;

    u64 value = 0;
    u64 limit = negative ? (u64 { 1 } << 63) : u64(INT64_MAX);
    u64 bytes = i;

    while (bytes < literal.len()) {
        u64 codepoint_bytes = 0;
        u32 const cp = decode_utf8_at(literal, bytes, &codepoint_bytes);
        bytes += codepoint_bytes;

        if (cp == '\'' || cp == '_')
            continue;

        int digit = -1;
        if (cp >= '0' && cp <= '9')
            digit = cp - '0';
        else if (is_arab_digit(cp))
            digit = arab_digit_to_canon(cp);
        else if (cp >= 'a' && cp <= 'f')
            digit = cp - 'a' + 10;
        else if (cp >= 'A' && cp <= 'F')
            digit = cp - 'A' + 10;
        if (digit < 0)
            diagnostic::fatal_error(ErrorCode::INVALID_NUMBER_LITERAL,
                "invalid digit in integer literal");

        if (digit >= base)
            diagnostic::fatal_error(ErrorCode::INVALID_NUMBER_LITERAL, "digit is not valid for the literal base");

        if (value > (limit - digit) / base)
            return false;

        value = value * base + digit;
    }

    result = negative ? (value == (u64 { 1 } << 63) ? INT64_MIN : -static_cast<i64>(value)) : static_cast<i64>(value);
    return true;
}

static inline bool is_integer_value(f64 d, i64& out)
{
    if (!std::isfinite(d) || d < -0x1p63 || d >= 0x1p63)
        return false;
    auto iv = static_cast<i64>(d);
    if (static_cast<f64>(iv) != d)
        return false;
    out = iv;
    return true;
}

}
