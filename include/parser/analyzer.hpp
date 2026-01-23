#pragma once

#include "../macros.hpp"
#include "cfg.hpp"
#include "symbol_table.hpp"

#include <memory>


namespace mylang {
namespace parser {

// Semantic analyzer with type inference and optimization hints
class SemanticAnalyzer
{
 public:
  struct Issue
  {
    enum class Severity { ERROR, WARNING, INFO };
    Severity     severity;
    StringType   message;
    std::int32_t line;
    StringType   suggestion;
  };

 private:
  SymbolTable*                 CurrentScope_;
  std::unique_ptr<SymbolTable> GlobalScope_;
  std::vector<Issue>           Issues_;
  ControlFlowGraph             Cfg_;

  // Type inference engine
  SymbolTable::DataType_t inferType(const ast::Expr* expr);

  void reportIssue(Issue::Severity sev, const StringType& msg, std::int32_t line, const StringType& sugg = u"");

  void analyzeExpr(const ast::Expr* expr);

  void analyzeStmt(const ast::Stmt* stmt);

 public:
  SemanticAnalyzer();

  void analyze(const std::vector<ast::Stmt*>& Statements_);

  const std::vector<Issue>& getIssues() const;

  const SymbolTable* getGlobalScope() const;

  void printReport() const;
};

}  // parser
}  // mylang