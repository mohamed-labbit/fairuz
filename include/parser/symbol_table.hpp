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

  enum class DataType { INTEGER, FLOAT, STRING, BOOLEAN, LIST, DICT, TUPLE, NONE, FUNCTION, ANY, UNKNOWN };

  struct Symbol
  {
    string_type name;
    SymbolType symbolType;
    DataType dataType;
    bool isConstant = false;
    bool isGlobal = false;
    bool isUsed = false;
    std::int32_t definitionLine = 0;
    std::vector<std::int32_t> usageLines;
    // For functions
    std::vector<DataType> paramTypes;
    DataType returnType = DataType::UNKNOWN;
    // For type inference
    std::unordered_set<DataType> possibleTypes;
  };

  SymbolTable* parent = nullptr;

 private:
  std::unordered_map<string_type, Symbol> symbols_;
  std::vector<std::unique_ptr<SymbolTable>> children_;
  unsigned scopeLevel_ = 0;

 public:
  explicit SymbolTable(SymbolTable* p = nullptr, std::int32_t level = 0);
  void define(const string_type& name, Symbol symbol);
  Symbol* lookup(const string_type& name);
  Symbol* lookupLocal(const string_type& name);
  bool isDefined(const string_type& name) const;
  void markUsed(const string_type& name, std::int32_t line);
  SymbolTable* createChild();
  std::vector<Symbol*> getUnusedSymbols();
  const std::unordered_map<string_type, Symbol>& getSymbols() const;
};

}
}