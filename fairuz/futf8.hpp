#pragma once

#include "fmacros.hpp"
#include "fstring.hpp"

namespace fairuz::util {

static inline size_t utf8_codepoint_size(u32 const cp)
{
    if (cp < 0x80)
        return 1;
    if (cp < 0x800)
        return 2;
    if (cp < 0x10000) {
        if (cp >= 0xD800 && cp <= 0xDFFF)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid cp: UTF-16 surrogate");
        return 3;
    }
    if (cp <= 0x10FFFF)
        return 4;

    diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid cp: exceeds Unicode range");
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
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid cp: UTF-16 surrogate");

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

    diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid cp: exceeds Unicode range");
    return -1; // unreachable
}

static inline StringRef encode_utf8_str(u32 const cp)
{
    unsigned char bytes[5];
    size_t const len = encode_utf8(cp, bytes);
    StringRef result(len, '\0');
    for (size_t i = 0; i < len; ++i)
        result[i] = static_cast<char>(bytes[i]);
    return result;
}

static u32 decode_utf8_at(StringRef const& buf, size_t const byte_pos, u64* out_bytes)
{
    if (byte_pos >= buf.len())
        diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "UTF8 decode past end of buffer");

    unsigned char const* p = (unsigned char const*)buf.data() + byte_pos;
    unsigned char const* end = (unsigned char const*)buf.data() + buf.len();

    unsigned char const c = *p;

    if (c < 0x80) {
        *out_bytes = 1;
        return c;
    }

    if ((c & 0xE0) == 0xC0) {
        if (p + 1 >= end)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "UTF8 truncated: incomplete 2-byte sequence");

        if ((p[1] & 0xC0) != 0x80)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: bad continuation byte");

        *out_bytes = 2;
        u32 const result = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);

        if (result < 0x80)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: overlong 2-byte sequence");

        return result;
    }

    if ((c & 0xF0) == 0xE0) {
        if (p + 2 >= end)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "UTF8 truncated: incomplete 3-byte sequence");

        if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: bad continuation byte");

        *out_bytes = 3;
        u32 const result = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);

        if (result < 0x800)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: overlong 3-byte sequence");

        if (result >= 0xD800 && result <= 0xDFFF)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: surrogate pair");

        return result;
    }

    if ((c & 0xF8) == 0xF0) {
        if (p + 3 >= end)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "UTF8 truncated: incomplete 4-byte sequence");

        if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: bad continuation byte");

        *out_bytes = 4;
        u32 const result = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);

        if (result < 0x10000)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: overlong 4-byte sequence");

        if (result > 0x10FFFF)
            diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: cp out of range");

        return result;
    }

    diagnostic::fatal_error(ErrorCode::INTERNAL_ERROR, "Invalid UTF-8: invalid start byte");
    return -1; // unreachable
}

} // namespace fairuz::util
