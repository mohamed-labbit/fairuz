#include "../../include/parser/symbol_table.hpp"


namespace mylang {
namespace parser {

SymbolTable::SymbolTable(SymbolTable* p, std::int32_t level) :
    Parent_(p),
    ScopeLevel_(level)
{
}

void SymbolTable::define(const StringRef& name, Symbol symbol)
{
  symbol.name    = name;
  Symbols_[name] = std::move(symbol);
}

typename SymbolTable::Symbol* SymbolTable::lookup(const StringRef& name)
{
  auto it = Symbols_.find(name);
  if (it != Symbols_.end())
  {
    return &it->second;
  }
  return Parent_ ? Parent_->lookup(name) : nullptr;
}

typename SymbolTable::Symbol* SymbolTable::lookupLocal(const StringRef& name)
{
  auto it = Symbols_.find(name);
  return it != Symbols_.end() ? &it->second : nullptr;
}

bool SymbolTable::isDefined(const StringRef& name) const
{
  if (Symbols_.count(name))
  {
    return true;
  }
  return Parent_ ? Parent_->isDefined(name) : false;
}

void SymbolTable::markUsed(const StringRef& name, std::int32_t line)
{
  if (auto* sym = lookup(name))
  {
    sym->IsUsed = true;
    sym->UsageLines.push_back(line);
  }
}

SymbolTable* SymbolTable::createChild()
{
  auto  child = std::make_unique<SymbolTable>(this, ScopeLevel_ + 1);
  auto* ptr   = child.get();
  Children_.push_back(std::move(child));
  return ptr;
}

std::vector<typename SymbolTable::Symbol*> SymbolTable::getUnusedSymbols()
{
  std::vector<Symbol*> unused;
  for (auto& [name, sym] : Symbols_)
  {
    if (!sym.IsUsed && sym.SymbolType == SymbolType::VARIABLE)
    {
      unused.push_back(&sym);
    }
  }
  return unused;
}

const std::unordered_map<StringRef, typename SymbolTable::Symbol>& SymbolTable::getSymbols() const { return Symbols_; }

}
}