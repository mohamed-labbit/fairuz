#pragma once

#include "../macros.hpp"
#include "../types/string.hpp"
#include <omp.h>
#include <string>
#include <vector>

namespace mylang {
namespace util {

static inline bool isWhitespace(wchar_t ch) { return ch == u' ' || ch == u'\t' || ch == u'\r'; }

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
    static constexpr std::u16string_view syms = u",[]().:";
    return syms.find(ch) != std::u16string_view::npos;
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

} // util
} // mylang
