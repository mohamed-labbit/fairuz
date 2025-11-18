#pragma once

#include <string>


// Terminal colors
namespace Color {
const std::u16string RESET = u"\033[0m";
const std::u16string BOLD = u"\033[1m";
const std::u16string RED = u"\033[31m";
const std::u16string GREEN = u"\033[32m";
const std::u16string YELLOW = u"\033[33m";
const std::u16string BLUE = u"\033[34m";
const std::u16string MAGENTA = u"\033[35m";
const std::u16string CYAN = u"\033[36m";
const std::u16string GRAY = u"\033[90m";
}
