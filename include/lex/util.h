#pragma once

#include <string>


static inline bool is_whitespace(wchar_t ch) { return ch == u' ' || ch == u'\t' || ch == u'\r'; }

static inline bool is_operator_char(wchar_t ch)
{
    static constexpr std::u16string_view ops = u"=<>!+-|&*/";
    return ops.find(ch) != std::u16string_view::npos;
}

static inline bool is_symbol_char(wchar_t ch)
{
    static constexpr std::u16string_view syms = u",[]().:";
    return syms.find(ch) != std::u16string_view::npos;
}

static inline bool isalpha_arabic(const wchar_t c) { return (c >= 0x0600 && c <= 0x06FF); }

struct U16StringHash
{
    std::size_t operator()(const std::u16string& s) const noexcept
    {
        // FNV-1a hash (simple, fast, decent distribution)
        std::size_t hash = 1469598103934665603ull;  // 64-bit FNV offset basis
        for (char16_t c : s)
        {
            hash ^= static_cast<std::size_t>(c);
            hash *= 1099511628211ull;  // FNV prime
        }
        return hash;
    }
};

struct U16StringEqual
{
    bool operator()(const std::u16string& a, const std::u16string& b) const noexcept
    {
        return a == b;
    }
};