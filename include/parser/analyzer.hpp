#pragma once

#include "../macros.hpp"
#include "cfg.hpp"
#include "symbol_table.hpp"

#include <memory>

namespace mylang {
namespace parser {

// Semantic analyzer with type inference and optimization hints
class SemanticAnalyzer {
public:
    struct Issue {
        enum class Severity { ERROR,
            WARNING,
            INFO };
        Severity severity;
        StringRef message;
        std::int32_t line;
        StringRef suggestion;
    };

private:
    SymbolTable* CurrentScope_;
    std::unique_ptr<SymbolTable> GlobalScope_;
    std::vector<Issue> Issues_;
    ControlFlowGraph Cfg_;

public:
    SymbolTable::DataType_t inferType(ast::Expr const* expr);

    void reportIssue(Issue::Severity sev, StringRef const& msg, std::int32_t line, StringRef const& sugg = u"");

    void analyzeExpr(ast::Expr const* expr);
    void analyzeStmt(ast::Stmt const* stmt);

    // public:
    SemanticAnalyzer();

    void analyze(std::vector<ast::Stmt*> const& Statements_);

    std::vector<Issue> const& getIssues() const;

    SymbolTable const* getGlobalScope() const;

    void printReport() const;
};

} // parser
} // mylang
