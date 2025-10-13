#pragma once

#include "lex/token.h"
#include <string>

namespace mylang {
namespace lex {


enum class SymbolType : int {
    VARIABLE,
    PARAMETER,
    KEYWORD,
    FUNCTION,
    CONSTANT
};

enum class ScopeType : int {
    // right now just local and global scopes are considered

    LOCAL,
    GLOBAL
};

struct SymbolTableEntry
{
    using string_type = std::wstring;
    using size_type   = std::size_t;

    string_type                                lexeme_;
    typename mylang::lex::tok::Token::Location location_;
    SymbolType                                 type_;
    ScopeType                                  scope_;  // nesting_level

    SymbolTableEntry(const string_type&                         lexeme,
                     typename mylang::lex::tok::Token::Location location,
                     SymbolType                                 type,
                     ScopeType                                  scope) :
        lexeme_(lexeme),
        location_(location),
        type_(type),
        scope_(scope)
    {
    }
};


}
}