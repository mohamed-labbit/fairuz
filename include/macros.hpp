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

// Terminal colors
namespace Color {
const std::u16string RESET   = u"\033[0m";
const std::u16string BOLD    = u"\033[1m";
const std::u16string RED     = u"\033[31m";
const std::u16string GREEN   = u"\033[32m";
const std::u16string YELLOW  = u"\033[33m";
const std::u16string BLUE    = u"\033[34m";
const std::u16string MAGENTA = u"\033[35m";
const std::u16string CYAN    = u"\033[36m";
const std::u16string GRAY    = u"\033[90m";
}  // Color

}  // mylang