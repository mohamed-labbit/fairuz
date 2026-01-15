#include "../../../include/runtime/vm/symbol_table.hpp"


namespace mylang {
namespace runtime {

typename CompilerSymbolTable::Symbol* CompilerSymbolTable::define(const std::string& name, bool isParam)
{
  if (Symbols_.count(name))
    return &Symbols_[name];

  Symbol sym;
  sym.name        = name;
  sym.index       = NextIndex_++;
  sym.scope       = Parent_ ? SymbolScope::LOCAL : SymbolScope::GLOBAL;
  sym.IsParameter = isParam;
  sym.IsCaptured  = false;
  sym.IsUsed      = false;

  Symbols_[name] = sym;
  return &Symbols_[name];
}

typename CompilerSymbolTable::Symbol* CompilerSymbolTable::resolve(const std::string& name)
{
  // Check local scope
  auto it = Symbols_.find(name);
  if (it != Symbols_.end())
  {
    it->second.IsUsed = true;
    return &it->second;
  }
  // Check parent scopes
  if (Parent_ != nullptr)
  {
    Symbol* parentSym = Parent_->resolve(name);
    if (parentSym != nullptr)
    {
      // Mark as captured for closure
      parentSym->IsCaptured = true;
      // Create closure reference
      Symbol closureSym;
      closureSym.name   = name;
      closureSym.index  = FreeVars_.size();
      closureSym.scope  = SymbolScope::CLOSURE;
      closureSym.IsUsed = true;
      FreeVars_.push_back(name);
      Symbols_[name] = closureSym;
      return &Symbols_[name];
    }
  }
  return nullptr;
}

const std::vector<std::string>& CompilerSymbolTable::getFreeVars() const { return FreeVars_; }

std::int32_t CompilerSymbolTable::getLocalCount() const { return NextIndex_; }

std::vector<typename CompilerSymbolTable::Symbol> CompilerSymbolTable::getUnusedSymbols() const
{
  std::vector<Symbol> unused;
  for (const auto& [name, sym] : Symbols_)
    if (!sym.IsUsed && !sym.IsParameter)
      unused.push_back(sym);
  return unused;
}

}
}