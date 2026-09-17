#ifndef FA_SYNTAX_HIGHLIGHTER_HPP
#define FA_SYNTAX_HIGHLIGHTER_HPP

#include "fAST.hpp"
#include "fstring.hpp"

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
