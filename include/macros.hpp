#pragma once

#include <limits>
#include <string>


namespace mylang {

#define BUFFER_END u'\0'
#define TABWIDTH 8
#define MAX_ALLOWED_INDENT 100
#define DEFAULT_CAPACITY 4096
#define DEFAULT_BLOCK_SIZE 1024    // 1 KiB
#define MAX_BLOCK_SIZE 4294967296  // 4 GiB
#define DEFAULT_STRING_CAPACITY 100
/// TODO: chenge to none
#define MYLANG_COMPILER_ABI __attribute__((visibility("default")))
#define MYLANG_INLINE inline
#define MYLANG_NODISCARD [[nodiscard]]
#define MYLANG_NOEXCEPT noexcept
#define MYLANG_CONSTEXPR constexpr

#define DEBUG_PRINT 0

typedef std::u16string StringType;
typedef char16_t       CharType;
typedef std::size_t    SizeType;
typedef unsigned char  Byte;

// Terminal colors
namespace Color {

const std::string RESET   = "\033[0m";
const std::string BOLD    = "\033[1m";
const std::string RED     = "\033[31m";
const std::string GREEN   = "\033[32m";
const std::string YELLOW  = "\033[33m";
const std::string BLUE    = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN    = "\033[36m";
const std::string GRAY    = "\033[90m";

}  // Color

// Add these to your StringRef header or a separate utf_constants.hpp

namespace utf {

// UTF-8 byte masks and markers
constexpr Byte UTF8_1BYTE_MASK   = 0x80;  // 10000000
constexpr Byte UTF8_1BYTE_MARKER = 0x00;  // 0xxxxxxx

constexpr Byte UTF8_2BYTE_MASK       = 0xE0;  // 11100000
constexpr Byte UTF8_2BYTE_MARKER     = 0xC0;  // 110xxxxx
constexpr Byte UTF8_2BYTE_VALUE_MASK = 0x1F;  // 00011111

constexpr Byte UTF8_3BYTE_MASK       = 0xF0;  // 11110000
constexpr Byte UTF8_3BYTE_MARKER     = 0xE0;  // 1110xxxx
constexpr Byte UTF8_3BYTE_VALUE_MASK = 0x0F;  // 00001111

constexpr Byte UTF8_4BYTE_MASK       = 0xF8;  // 11111000
constexpr Byte UTF8_4BYTE_MARKER     = 0xF0;  // 11110xxx
constexpr Byte UTF8_4BYTE_VALUE_MASK = 0x07;  // 00000111

constexpr Byte UTF8_CONTINUATION_MASK       = 0xC0;  // 11000000
constexpr Byte UTF8_CONTINUATION_MARKER     = 0x80;  // 10xxxxxx
constexpr Byte UTF8_CONTINUATION_VALUE_MASK = 0x3F;  // 00111111

// UTF-16 surrogate ranges
constexpr char16_t UTF16_HIGH_SURROGATE_MIN = 0xD800;
constexpr char16_t UTF16_HIGH_SURROGATE_MAX = 0xDBFF;
constexpr char16_t UTF16_LOW_SURROGATE_MIN  = 0xDC00;
constexpr char16_t UTF16_LOW_SURROGATE_MAX  = 0xDFFF;

// UTF-16 surrogate offsets
constexpr uint32_t UTF16_SURROGATE_OFFSET = 0x10000;
constexpr uint32_t UTF16_SURROGATE_MASK   = 0x3FF;  // 10 bits
constexpr uint32_t UTF16_SURROGATE_SHIFT  = 10;

// Code point boundaries
constexpr uint32_t CODEPOINT_MAX_1BYTE = 0x7F;      // 127
constexpr uint32_t CODEPOINT_MAX_2BYTE = 0x7FF;     // 2,047
constexpr uint32_t CODEPOINT_MAX_3BYTE = 0xFFFF;    // 65,535
constexpr uint32_t CODEPOINT_MAX_4BYTE = 0x10FFFF;  // 1,114,111 (max Unicode)

constexpr uint32_t CODEPOINT_MAX_BMP = 0xFFFF;  // Basic Multilingual Plane

// Helper functions
inline bool isUtf8Continuation(Byte byte) { return (byte & UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION_MARKER; }

inline bool isHighSurrogate(char16_t ch) { return ch >= UTF16_HIGH_SURROGATE_MIN && ch <= UTF16_HIGH_SURROGATE_MAX; }

inline bool isLowSurrogate(char16_t ch) { return ch >= UTF16_LOW_SURROGATE_MIN && ch <= UTF16_LOW_SURROGATE_MAX; }

inline bool isSurrogate(char16_t ch) { return ch >= UTF16_HIGH_SURROGATE_MIN && ch <= UTF16_LOW_SURROGATE_MAX; }

inline uint32_t combineSurrogates(char16_t high, char16_t low)
{
  return ((high - UTF16_HIGH_SURROGATE_MIN) << UTF16_SURROGATE_SHIFT) + (low - UTF16_LOW_SURROGATE_MIN) + UTF16_SURROGATE_OFFSET;
}

inline void splitToSurrogates(uint32_t codepoint, char16_t& high, char16_t& low)
{
  codepoint -= UTF16_SURROGATE_OFFSET;
  high = static_cast<char16_t>(UTF16_HIGH_SURROGATE_MIN + (codepoint >> UTF16_SURROGATE_SHIFT));
  low  = static_cast<char16_t>(UTF16_LOW_SURROGATE_MIN + (codepoint & UTF16_SURROGATE_MASK));
}

}  // namespace utf

}  // mylang