//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Produces syntax highlighting and semantic tokens.
//===----------------------------------------------------------------------===//

#ifndef FA_SYNTAX_HIGHLIGHTER_HPP
#define FA_SYNTAX_HIGHLIGHTER_HPP

#include "Support/string.hpp"

#include <string>
#include <vector>

namespace fairuz::syntax {

struct Token {
    u32 line { 0 };
    u32 start { 0 };
    u32 length { 0 };
    std::string type;
    bool declaration { false };
};

struct Result {
    bool ast_valid { false };
    std::vector<Token> tokens;
};

class Highlighter {
public:
    Result highlight(StringRef const& source);
};

} // namespace fairuz::syntax

#endif
