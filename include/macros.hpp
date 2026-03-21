#ifndef MACROS_HPP
#define MACROS_HPP

#include <string>

namespace mylang {

#define SSO_SIZE 20 // to make sizeof(String) = 64 bytes
#define TABWIDTH 8
#define MAX_ALLOWED_INDENT 100
#define DEFAULT_CAPACITY 4096
#define DEFAULT_BLOCK_SIZE 1024
#define MAX_BLOCK_SIZE 4294967296
#define DEFAULT_STRING_CAPACITY 100
#define MY_NODISCARD [[nodiscard]]

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

} // namespace Color

struct SourceLocation {
  uint32_t line{0};
  uint16_t column{0};
  uint16_t length{0};
};

} // namespace mylang

#endif // MACROS_HPP
