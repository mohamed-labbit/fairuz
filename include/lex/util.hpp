#pragma once

#include "../../utfcpp/source/utf8.h"
#include "../macros.hpp"
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

struct U16StringHash
{
  SizeType operator()(const StringRef& s) const MYLANG_NOEXCEPT
  {
    // FNV-1a hash (simple, fast, decent distribution)
    SizeType hash = 1469598103934665603ull;  // 64-bit FNV offset basis
    for (CharType c : s)
    {
      hash ^= static_cast<SizeType>(c);
      hash *= 1099511628211ull;  // FNV prime
    }
    return hash;
  }
};

struct U16StringEqual
{
  bool operator()(const StringRef& a, const StringRef& b) const MYLANG_NOEXCEPT { return a == b; }
};

// utility function to parallelize the process of conversion
static StringRef bufferToU16String(const std::vector<char>& buf)
{
  if (buf.empty())
  {
    return u"";
  }
  StringRef ret = u"";
  ret.resize(buf.size());
  const char* __restrict bptr = buf.data();
  CharType* __restrict rptr   = ret.data();
  for (SizeType i = 0, n = buf.size(); i < n; ++i)
  {
    rptr[i] = *utf8::utf8to16(std::string(reinterpret_cast<char*>(bptr[i]))).data();
  }
  return ret;
}

}  // util
}  // mylang