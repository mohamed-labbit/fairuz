#pragma once

#include "lex/token.h"
#include <clocale>
#include <cstdint>
#include <cwchar>
#include <string>


// ---------- Arabic & identifier classification ----------
static inline bool is_ascii_letter(char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'); }

static inline bool is_ascii_digit(char32_t c) { return (c >= U'0' && c <= U'9'); }

static inline bool is_arabic_letter(char32_t c)
{
    // Broad but practical coverage of Arabic script blocks
    return (static_cast<int>(c) >= 0x0600 && static_cast<int>(c) <= 0x06FF) ||  // Arabic
           (static_cast<int>(c) >= 0x0750 && static_cast<int>(c) <= 0x077F) ||  // Arabic Supplement
           (static_cast<int>(c) >= 0x08A0 && static_cast<int>(c) <= 0x08FF) ||  // Arabic Extended-A
           (static_cast<int>(c) >= 0xFB50 && static_cast<int>(c) <= 0xFDFF) ||  // Arabic Presentation Forms-A
           (static_cast<int>(c) >= 0xFE70 && static_cast<int>(c) <= 0xFEFF);    // Arabic Presentation Forms-B
}

static inline bool is_arabic_digit(char32_t c)
{
    // Arabic-Indic digits 0660..0669, Eastern Arabic-Indic 06F0..06F9
    return (static_cast<int>(c) >= 0x0660 && static_cast<int>(c) <= 0x0669)
        || (static_cast<int>(c) >= 0x06F0 && static_cast<int>(c) <= 0x06F9);
}

static inline bool is_arabic_combining_mark(char32_t c)
{
    // Common Arabic combining marks (tashkeel); allow inside identifiers as continuation.
    return (static_cast<int>(c) >= 0x064B && static_cast<int>(c) <= 0x065F) || (static_cast<int>(c) == 0x0670);
}

static inline bool is_ident_start(char32_t c) { return c == U'_' || is_ascii_letter(c) || is_arabic_letter(c); }

static inline bool is_ident_continue(char32_t c)
{
    return is_ident_start(c) || is_ascii_digit(c) || is_arabic_digit(c) || is_arabic_combining_mark(c);
}