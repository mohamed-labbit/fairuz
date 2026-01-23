#pragma once

#include <string>
#include <unordered_map>
#include <vector>


// ============================================================================
// SYMBOL TABLE for Bytecode Generation
// ============================================================================

namespace mylang {
namespace runtime {

class CompilerSymbolTable
{
 public:
  enum class SymbolScope { GLOBAL, LOCAL, CLOSURE, CELL };

  struct Symbol
  {
    std::string  name;
    std::int32_t index;
    SymbolScope  scope;
    bool         IsParameter;
    bool         IsCaptured;  // Used in closure
    bool         IsUsed;
  };

 private:
  std::unordered_map<std::string, Symbol> Symbols_;
  CompilerSymbolTable*                    Parent_;
  std::int32_t                            NextIndex_{0};
  std::vector<std::string>                FreeVars_;  // Closure variables

 public:
  explicit CompilerSymbolTable(CompilerSymbolTable* p = nullptr) :
      Parent_(p)
  {
  }

  Symbol*                         define(const std::string& name, bool isParam = false);
  
  Symbol*                         resolve(const std::string& name);
  
  const std::vector<std::string>& getFreeVars() const;
  
  std::int32_t                    getLocalCount() const;
  
  std::vector<Symbol>             getUnusedSymbols() const;
};  // CompilerSymbolTable

}
}