#pragma once

#include "../token.hpp"
#include <string>

namespace mylang {
namespace lex {


enum class SymbolType : std::int32_t { VARIABLE, PARAMETER, KEYWORD, FUNCTION, CONSTANT };

struct SymbolTableEntry
{

    SymbolType type;
    std::size_t scope;  // nesting_level


    SymbolTableEntry() = default;

    SymbolTableEntry(SymbolType type, std::size_t scope) :
        type(type),
        scope(scope)
    {
    }

    SymbolTableEntry(const SymbolTableEntry& other) :
        type(other.type),
        scope(other.scope)
    {
    }

    SymbolTableEntry& operator=(const SymbolTableEntry& other)
    {
        this->type = other.type;
        this->scope = other.scope;
        return *this;
    }
};

}
}
