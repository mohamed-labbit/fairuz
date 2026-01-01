#include "../../include/parser/symbol_table.hpp"


namespace mylang {
namespace parser {

SymbolTable::SymbolTable(SymbolTable* p, std::int32_t level) :
    parent(p),
    scopeLevel_(level)
{
}

void SymbolTable::define(const string_type& name, Symbol symbol)
{
  symbol.name = name;
  symbols_[name] = std::move(symbol);
}

typename SymbolTable::Symbol* SymbolTable::lookup(const string_type& name)
{
  auto it = symbols_.find(name);
  if (it != symbols_.end()) return &it->second;
  return parent ? parent->lookup(name) : nullptr;
}

typename SymbolTable::Symbol* SymbolTable::lookupLocal(const string_type& name)
{
  auto it = symbols_.find(name);
  return it != symbols_.end() ? &it->second : nullptr;
}

bool SymbolTable::isDefined(const string_type& name) const
{
  if (symbols_.count(name)) return true;
  return parent ? parent->isDefined(name) : false;
}

void SymbolTable::markUsed(const string_type& name, std::int32_t line)
{
  if (auto* sym = lookup(name))
  {
    sym->isUsed = true;
    sym->usageLines.push_back(line);
  }
}

SymbolTable* SymbolTable::createChild()
{
  auto child = std::make_unique<SymbolTable>(this, scopeLevel_ + 1);
  auto* ptr = child.get();
  children_.push_back(std::move(child));
  return ptr;
}

std::vector<typename SymbolTable::Symbol*> SymbolTable::getUnusedSymbols()
{
  std::vector<Symbol*> unused;
  for (auto& [name, sym] : symbols_)
    if (!sym.isUsed && sym.symbolType == SymbolType::VARIABLE) unused.push_back(&sym);
  return unused;
}

const std::unordered_map<string_type, typename SymbolTable::Symbol>& SymbolTable::getSymbols() const { return symbols_; }

}
}