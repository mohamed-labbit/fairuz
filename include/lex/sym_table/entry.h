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

struct SymbolTableEntry
{
    using string_type = std::u16string;
    using size_type   = std::size_t;

    SymbolType type_;
    size_type  scope_;  // nesting_level


    SymbolTableEntry() = default;

    SymbolTableEntry(SymbolType type, size_type scope) :
        type_(type),
        scope_(scope)
    {
    }

    SymbolTableEntry(const SymbolTableEntry& other) :
        type_(other.type_),
        scope_(other.scope_)
    {
    }

    SymbolTableEntry& operator=(const SymbolTableEntry& other)
    {
        this->type_  = other.type_;
        this->scope_ = other.scope_;
        return *this;
    }
};

}
}
