#pragma once

#include "../token.h"
#include <string>

namespace mylang {
namespace lex {


enum class SymbolType : int { VARIABLE, PARAMETER, KEYWORD, FUNCTION, CONSTANT };

struct SymbolTableEntry
{

    SymbolType type_;
    std::size_t scope_;  // nesting_level


    SymbolTableEntry() = default;

    SymbolTableEntry(SymbolType type, std::size_t scope) :
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
        this->type_ = other.type_;
        this->scope_ = other.scope_;
        return *this;
    }
};

}
}
