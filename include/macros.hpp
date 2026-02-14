#pragma once

#include <limits>
#include <string>

namespace mylang {

#define BUFFER_END '\0'
#define SSO_SIZE 20 // to make sizeof(String) = 64 bytes
#define TABWIDTH 8
#define MAX_ALLOWED_INDENT 100
#define DEFAULT_CAPACITY 4096
#define DEFAULT_BLOCK_SIZE 1024   // 1 KiB
#define MAX_BLOCK_SIZE 4294967296 // 4 GiB
#define DEFAULT_STRING_CAPACITY 100

/// TODO: chenge to none
#define MYLANG_COMPILER_ABI __attribute__((visibility("default")))
#define MYLANG_INLINE inline
#define MYLANG_NODISCARD [[nodiscard]]
#define MYLANG_NOEXCEPT noexcept
#define MYLANG_CONSTEXPR constexpr

#define DEBUG_PRINT 0
#define USE_FREE_LIST 0
#define USE_FAST_POOL 0

typedef std::u16string StringType;
typedef char16_t CharType;
typedef std::size_t SizeType;
typedef unsigned char Byte;

// Terminal colors
namespace Color {

std::string const RESET = "\033[0m";
std::string const BOLD = "\033[1m";
std::string const RED = "\033[31m";
std::string const GREEN = "\033[32m";
std::string const YELLOW = "\033[33m";
std::string const BLUE = "\033[34m";
std::string const MAGENTA = "\033[35m";
std::string const CYAN = "\033[36m";
std::string const GRAY = "\033[90m";

} // Color

} // mylang
