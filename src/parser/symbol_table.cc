#include "../../include/parser/symbol_table.hpp"

namespace mylang {
namespace parser {

SymbolTable::SymbolTable(SymbolTable* p, int32_t level)
    : Parent_(p)
    , ScopeLevel_(level)
{
}

void SymbolTable::define(StringRef const& name, Symbol symbol)
{
    if (lookupLocal(name))
        // emit redeclaration error
        return;

    symbol.name = name;
    Symbols_.emplace(name, std::move(symbol));
}

typename SymbolTable::Symbol* SymbolTable::lookup(StringRef const& name)
{
    auto it = Symbols_.find(name);
    if (it != Symbols_.end())
        return &it->second;

    return Parent_ ? Parent_->lookup(name) : nullptr;
}

typename SymbolTable::Symbol* SymbolTable::lookupLocal(StringRef const& name)
{
    auto it = Symbols_.find(name);
    return it != Symbols_.end() ? &it->second : nullptr;
}

bool SymbolTable::isDefined(StringRef const& name) const
{
    if (Symbols_.find(name) != Symbols_.end())
        return true;

    return Parent_ ? Parent_->isDefined(name) : false;
}

void SymbolTable::markUsed(StringRef const& name, int32_t line)
{
    if (Symbol* sym = lookup(name)) {
        sym->isUsed = true;
        sym->usageLines.push_back(line);
    }
}

SymbolTable* SymbolTable::createChild()
{
    std::unique_ptr<SymbolTable> child = std::make_unique<SymbolTable>(this, ScopeLevel_ + 1);
    SymbolTable* ptr = child.get();
    Children_.push_back(std::move(child));
    return ptr;
}

std::vector<typename SymbolTable::Symbol*> SymbolTable::getUnusedSymbols()
{
    std::vector<Symbol*> unused;
    for (auto& [name, sym] : Symbols_) {
        if (!sym.isUsed && sym.symbolType == SymbolType::VARIABLE)
            unused.push_back(&sym);
    }

    return unused;
}

std::unordered_map<StringRef, typename SymbolTable::Symbol, StringRefHash, StringRefEqual> const& SymbolTable::getSymbols() const
{
    return Symbols_;
}

}
}
