#include "../../../include/runtime/vm/symbol_table.hpp"


namespace mylang {
namespace runtime {

typename CompilerSymbolTable::Symbol* CompilerSymbolTable::define(const std::string& name, bool isParam)
{
  if (symbols_.count(name)) return &symbols_[name];

  Symbol sym;
  sym.name = name;
  sym.index = nextIndex_++;
  sym.scope = parent_ ? SymbolScope::LOCAL : SymbolScope::GLOBAL;
  sym.isParameter = isParam;
  sym.isCaptured = false;
  sym.isUsed = false;

  symbols_[name] = sym;
  return &symbols_[name];
}

typename CompilerSymbolTable::Symbol* CompilerSymbolTable::resolve(const std::string& name)
{
  // Check local scope
  auto it = symbols_.find(name);
  if (it != symbols_.end())
  {
    it->second.isUsed = true;
    return &it->second;
  }
  // Check parent scopes
  if (parent_)
  {
    Symbol* parentSym = parent_->resolve(name);
    if (parentSym)
    {
      // Mark as captured for closure
      parentSym->isCaptured = true;
      // Create closure reference
      Symbol closureSym;
      closureSym.name = name;
      closureSym.index = freeVars.size();
      closureSym.scope = SymbolScope::CLOSURE;
      closureSym.isUsed = true;
      freeVars.push_back(name);
      symbols_[name] = closureSym;
      return &symbols_[name];
    }
  }
  return nullptr;
}

const std::vector<std::string>& CompilerSymbolTable::getFreeVars() const { return freeVars; }

std::int32_t CompilerSymbolTable::getLocalCount() const { return nextIndex_; }

std::vector<typename CompilerSymbolTable::Symbol> CompilerSymbolTable::getUnusedSymbols() const
{
  std::vector<Symbol> unused;
  for (const auto& [name, sym] : symbols_)
    if (!sym.isUsed && !sym.isParameter) unused.push_back(sym);
  return unused;
}

}
}