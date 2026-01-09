#pragma once

#include "../macros.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace mylang {
namespace parser {

// Symbol table with type inference
class SymbolTable
{
 public:
  enum class SymbolType { VARIABLE, FUNCTION, CLASS, MODULE, UNKNOWN };

  enum class DataType_t { INTEGER, FLOAT, STRING, BOOLEAN, LIST, DICT, TUPLE, NONE, FUNCTION, ANY, UNKNOWN };

  struct Symbol
  {
    StringType name;
    SymbolType SymbolType;
    DataType_t DataType;
    bool IsConstant = false;
    bool IsGlobal = false;
    bool IsUsed = false;
    std::int32_t DefinitionLine = 0;
    std::vector<std::int32_t> UsageLines;
    // For functions
    std::vector<DataType_t> ParamTypes;
    DataType_t returnType = DataType_t::UNKNOWN;
    // For type inference
    std::unordered_set<DataType_t> PossibleTypes;
  };

  SymbolTable* Parent_ = nullptr;

 private:
  std::unordered_map<StringType, Symbol> Symbols_;
  std::vector<std::unique_ptr<SymbolTable>> Children_;
  unsigned ScopeLevel_ {0};

 public:
  explicit SymbolTable(SymbolTable* p = nullptr, std::int32_t level = 0);
  void define(const StringType& name, Symbol symbol);
  Symbol* lookup(const StringType& name);
  Symbol* lookupLocal(const StringType& name);
  bool isDefined(const StringType& name) const;
  void markUsed(const StringType& name, std::int32_t line);
  SymbolTable* createChild();
  std::vector<Symbol*> getUnusedSymbols();
  const std::unordered_map<StringType, Symbol>& getSymbols() const;
};

}
}