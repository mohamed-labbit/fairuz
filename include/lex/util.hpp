#pragma once

#include "../macros.hpp"
#include "../types/string.hpp"
#include <omp.h>
#include <string>
#include <vector>


namespace mylang {
namespace util {

static inline bool isWhitespace(wchar_t ch) { return ch == u' ' || ch == u'\t' || ch == u'\r'; }

static inline bool isOperator(wchar_t ch)
{
  static constexpr std::u16string_view ops = u"=<>!+-|&*/";
  return ops.find(ch) != std::u16string_view::npos;
}

static inline bool isSymbol(wchar_t ch)
{
  static constexpr std::u16string_view syms = u",[]().:";
  return syms.find(ch) != std::u16string_view::npos;
}

static inline bool isalphaArabic(const wchar_t c) { return (c >= 0x0600 && c <= 0x06FF); }

}  // util
}  // mylang