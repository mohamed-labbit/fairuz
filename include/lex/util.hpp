#pragma once

#include "../../utfcpp/source/utf8.h"
#include "macros.hpp"
#include <omp.h>
#include <string>
#include <vector>


namespace mylang {
namespace util {

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
  std::size_t operator()(const string_type& s) const noexcept
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
  bool operator()(const string_type& a, const string_type& b) const noexcept { return a == b; }
};

// utility function to parallelize the process of conversion
static string_type buffer_toU16_string(const std::vector<char>& buf)
{
  if (buf.empty()) return u"";
  string_type ret = u"";
  ret.resize(buf.size());
  const char* __restrict bptr = buf.data();
  char16_t* __restrict rptr = ret.data();
  for (std::size_t i = 0, n = buf.size(); i < n; ++i)
    rptr[i] = *utf8::utf8to16(std::string(reinterpret_cast<char*>(bptr[i]))).data();
  return ret;
}

}
}