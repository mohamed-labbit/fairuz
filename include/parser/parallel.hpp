#pragma once

#include "../ast/ast.hpp"
#include "../lex/token.hpp"
#include <vector>

namespace mylang {
namespace parser {

class ParallelParser {
public:
    static std::vector<ast::Stmt*> parseParallel(std::vector<tok::Token> const& tokens, std::int32_t threadCount = 4);

private:
    static std::vector<std::vector<tok::Token>> splitIntoChunks(std::vector<tok::Token> const& tokens);
};

}
}
