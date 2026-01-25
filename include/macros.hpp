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

}  // mylang