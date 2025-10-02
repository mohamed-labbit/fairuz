#pragma once

#include "lex/token.h"
#include <clocale>
#include <cstdint>
#include <cwchar>
#include <string>


// ---------- UTF-8 decoding helpers ----------
static inline unsigned utf8_len(unsigned char lead) {
    if (lead < 0x80)
    {
        return 1;
    }

    if ((lead >> 5) == 0x6)
    {
        return 2;
    }

    if ((lead >> 4) == 0xE)
    {
        return 3;
    }

    if ((lead >> 3) == 0x1E)
    {
        return 4;
    }

    return 1;  // fallback on malformed
}

static inline char32_t utf8_decode_unsafe(const std::string& s, std::size_t i, unsigned len) {
    // Assumes bounds checked by caller; keeps it fast.
    if (len == 1)
    {
        return static_cast<unsigned char>(s[i]);
    }

    if (len == 2)
    {
        return ((s[i] & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
    }

    if (len == 3)
    {
        return ((s[i] & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6)
             | (static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
    }

    // len == 4
    return ((s[i] & 0x07u) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12)
         | ((static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
}

// ---------- Arabic & identifier classification ----------
static inline bool is_ascii_letter(char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'); }

static inline bool is_ascii_digit(char32_t c) { return (c >= U'0' && c <= U'9'); }

static inline bool is_arabic_letter(char32_t c) {
    // Broad but practical coverage of Arabic script blocks
    return (static_cast<int>(c) >= 0x0600 && static_cast<int>(c) <= 0x06FF) ||  // Arabic
           (static_cast<int>(c) >= 0x0750 && static_cast<int>(c) <= 0x077F) ||  // Arabic Supplement
           (static_cast<int>(c) >= 0x08A0 && static_cast<int>(c) <= 0x08FF) ||  // Arabic Extended-A
           (static_cast<int>(c) >= 0xFB50 && static_cast<int>(c) <= 0xFDFF) ||  // Arabic Presentation Forms-A
           (static_cast<int>(c) >= 0xFE70 && static_cast<int>(c) <= 0xFEFF);    // Arabic Presentation Forms-B
}

static inline bool is_arabic_digit(char32_t c) {
    // Arabic-Indic digits 0660..0669, Eastern Arabic-Indic 06F0..06F9
    return (static_cast<int>(c) >= 0x0660 && static_cast<int>(c) <= 0x0669)
        || (static_cast<int>(c) >= 0x06F0 && static_cast<int>(c) <= 0x06F9);
}

static inline bool is_arabic_combining_mark(char32_t c) {
    // Common Arabic combining marks (tashkeel); allow inside identifiers as continuation.
    return (static_cast<int>(c) >= 0x064B && static_cast<int>(c) <= 0x065F) || (static_cast<int>(c) == 0x0670);
}

static inline bool is_ident_start(char32_t c) { return c == U'_' || is_ascii_letter(c) || is_arabic_letter(c); }

static inline bool is_ident_continue(char32_t c) {
    return is_ident_start(c) || is_ascii_digit(c) || is_arabic_digit(c) || is_arabic_combining_mark(c);
}