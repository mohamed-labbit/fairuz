#pragma once

#include "../macros.hpp"
#include "../types/string.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mylang {
namespace parser {

// symbol table with type inference
class SymbolTable {
public:
    enum class SymbolType {
        VARIABLE,
        FUNCTION,
        CLASS,
        MODULE,
        UNKNOWN
    };

    enum class DataType_t {
        INTEGER,
        FLOAT,
        STRING,
        BOOLEAN,
        LIST,
        DICT,
        TUPLE,
        NONE,
        FUNCTION,
        ANY,
        UNKNOWN
    };

    struct Symbol {
        StringRef name;
        SymbolType symbolType;
        DataType_t dataType;
        bool isUsed = false;
        int32_t definitionLine = 0;
        std::vector<int32_t> usageLines;
        // for functions
        std::vector<DataType_t> paramTypes;
        DataType_t returnType = DataType_t::UNKNOWN;
        // for type inference
        std::unordered_set<DataType_t> possibleTypes;
    };

    SymbolTable* Parent_ = nullptr;

private:
    std::unordered_map<StringRef, Symbol, StringRefHash, StringRefEqual> Symbols_;
    std::vector<std::unique_ptr<SymbolTable>> Children_;
    unsigned int ScopeLevel_ { 0 };

public:
    explicit SymbolTable(SymbolTable* p = nullptr, int32_t level = 0);

    void define(StringRef const& name, Symbol symbol);

    Symbol* lookup(StringRef const& name);

    Symbol* lookupLocal(StringRef const& name);

    bool isDefined(StringRef const& name) const;

    void markUsed(StringRef const& name, int32_t line);

    SymbolTable* createChild();

    std::vector<Symbol*> getUnusedSymbols();

    std::unordered_map<StringRef, Symbol, StringRefHash, StringRefEqual> const& getSymbols() const;
};

}
}
