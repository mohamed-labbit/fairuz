#pragma once


#include "../../utfcpp/source/utf8.h"
#include "ast/ast.hpp"
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace mylang {
namespace parser {
namespace symbols {

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
  explicit SymbolTable(SymbolTable* p = nullptr, std::int32_t level = 0) :
      parent(p),
      scopeLevel_(level)
  {
  }

  void define(const string_type& name, Symbol symbol)
  {
    symbol.name = name;
    symbols_[name] = std::move(symbol);
  }

  Symbol* lookup(const string_type& name)
  {
    auto it = symbols_.find(name);
    if (it != symbols_.end()) return &it->second;
    return parent ? parent->lookup(name) : nullptr;
  }

  Symbol* lookupLocal(const string_type& name)
  {
    auto it = symbols_.find(name);
    return it != symbols_.end() ? &it->second : nullptr;
  }

  bool isDefined(const string_type& name) const
  {
    if (symbols_.count(name)) return true;
    return parent ? parent->isDefined(name) : false;
  }

  void markUsed(const string_type& name, std::int32_t line)
  {
    if (auto* sym = lookup(name))
    {
      sym->isUsed = true;
      sym->usageLines.push_back(line);
    }
  }

  SymbolTable* createChild()
  {
    auto child = std::make_unique<SymbolTable>(this, scopeLevel_ + 1);
    auto* ptr = child.get();
    children_.push_back(std::move(child));
    return ptr;
  }

  std::vector<Symbol*> getUnusedSymbols()
  {
    std::vector<Symbol*> unused;
    for (auto& [name, sym] : symbols_)
      if (!sym.isUsed && sym.symbolType == SymbolType::VARIABLE) unused.push_back(&sym);
    return unused;
  }

  const std::unordered_map<string_type, Symbol>& getSymbols() const { return symbols_; }
};

// Control flow graph for advanced analysis
class ControlFlowGraph
{
 public:
  struct BasicBlock
  {
    std::int32_t id;
    std::vector<ast::StmtPtr*> statements;
    std::vector<std::int32_t> predecessors;
    std::vector<std::int32_t> successors;
    bool isReachable = false;
    // Data flow analysis
    std::unordered_set<string_type> defVars;  // Variables defined
    std::unordered_set<string_type> useVars;  // Variables used
    std::unordered_set<string_type> liveIn;   // Live at entry
    std::unordered_set<string_type> liveOut;  // Live at exit
  };

 private:
  std::vector<BasicBlock> blocks_;
  std::int32_t entryBlock_ = 0;
  std::int32_t exitBlock_ = -1;

 public:
  void addBlock(BasicBlock block) { blocks_.push_back(std::move(block)); }

  void addEdge(std::int32_t from, std::int32_t to)
  {
    if (from >= 0 && from < blocks_.size() && to >= 0 && to < blocks_.size())
    {
      blocks_[from].successors.push_back(to);
      blocks_[to].predecessors.push_back(from);
    }
  }

  // Compute reachability
  void computeReachability()
  {
    if (blocks_.empty()) return;

    blocks_[entryBlock_].isReachable = true;
    std::vector<std::int32_t> worklist = {entryBlock_};

    while (!worklist.empty())
    {
      std::int32_t blockId = worklist.back();
      worklist.pop_back();

      for (std::int32_t succ : blocks_[blockId].successors)
      {
        if (!blocks_[succ].isReachable)
        {
          blocks_[succ].isReachable = true;
          worklist.push_back(succ);
        }
      }
    }
  }

  // Liveness analysis for dead code detection
  void computeLiveness()
  {
    bool changed = true;
    while (changed)
    {
      changed = false;

      for (std::int32_t i = blocks_.size() - 1; i >= 0; --i)
      {
        auto& block = blocks_[i];
        // liveOut = union of successors' liveIn
        std::unordered_set<string_type> newLiveOut;
        for (std::int32_t succ : block.successors)
          newLiveOut.insert(blocks_[succ].liveIn.begin(), blocks_[succ].liveIn.end());
        // liveIn = use ∪ (liveOut - def)
        std::unordered_set<string_type> newLiveIn = block.useVars;
        for (const string_type& var : newLiveOut)
          if (!block.defVars.count(var)) newLiveIn.insert(var);
        if (newLiveIn != block.liveIn || newLiveOut != block.liveOut)
        {
          block.liveIn = std::move(newLiveIn);
          block.liveOut = std::move(newLiveOut);
          changed = true;
        }
      }
    }
  }

  std::vector<std::int32_t> getUnreachableBlocks() const
  {
    std::vector<std::int32_t> unreachable;
    for (std::size_t i = 0; i < blocks_.size(); ++i)
      if (!blocks_[i].isReachable) unreachable.push_back(i);
    return unreachable;
  }

  const std::vector<BasicBlock>& getBlocks() const { return blocks_; }
};

// Semantic analyzer with type inference and optimization hints
class SemanticAnalyzer
{
 public:
  struct Issue
  {
    enum class Severity { ERROR, WARNING, INFO };
    Severity severity;
    string_type message;
    std::int32_t line;
    string_type suggestion;
  };

 private:
  SymbolTable* currentScope_;
  std::unique_ptr<SymbolTable> globalScope_;
  std::vector<Issue> issues_;
  ControlFlowGraph cfg_;

  // Type inference engine
  SymbolTable::DataType inferType(const ast::Expr* expr)
  {
    if (!expr) return SymbolTable::DataType::UNKNOWN;

    switch (expr->kind)
    {
    case ast::Expr::Kind::LITERAL : {
      auto* lit = static_cast<const ast::LiteralExpr*>(expr);
      switch (lit->type)
      {
      case ast::LiteralExpr::Type::NUMBER :
        return lit->literal.find('.') != std::string::npos ? SymbolTable::DataType::FLOAT : SymbolTable::DataType::INTEGER;
      case ast::LiteralExpr::Type::STRING : return SymbolTable::DataType::STRING;
      case ast::LiteralExpr::Type::BOOLEAN : return SymbolTable::DataType::BOOLEAN;
      case ast::LiteralExpr::Type::NONE : return SymbolTable::DataType::NONE;
      }
      break;
    }
    case ast::Expr::Kind::NAME : {
      auto* name = static_cast<const ast::NameExpr*>(expr);
      if (auto* sym = currentScope_->lookup(name->name)) return sym->dataType;
      break;
    }
    case ast::Expr::Kind::BINARY : {
      auto* bin = static_cast<const ast::BinaryExpr*>(expr);
      auto leftType = inferType(bin->left.get());
      auto rightType = inferType(bin->right.get());
      // Type promotion rules
      if (leftType == SymbolTable::DataType::FLOAT || rightType == SymbolTable::DataType::FLOAT) return SymbolTable::DataType::FLOAT;
      if (leftType == SymbolTable::DataType::INTEGER && rightType == SymbolTable::DataType::INTEGER) return SymbolTable::DataType::INTEGER;
      // String concatenation
      if (bin->op == u"+" && leftType == SymbolTable::DataType::STRING) return SymbolTable::DataType::STRING;
      // Boolean operations
      if (bin->op == u"و" || bin->op == u"او") return SymbolTable::DataType::BOOLEAN;
      break;
    }
    case ast::Expr::Kind::LIST : return SymbolTable::DataType::LIST;
    case ast::Expr::Kind::CALL : return SymbolTable::DataType::ANY;
    default : break;
    }
    return SymbolTable::DataType::UNKNOWN;
  }

  void reportIssue(Issue::Severity sev, const string_type& msg, std::int32_t line, const string_type& sugg = u"")
  {
    issues_.push_back({sev, msg, line, sugg});
  }

  void analyzeExpr(const ast::Expr* expr)
  {
    if (!expr) return;

    switch (expr->kind)
    {
    case ast::Expr::Kind::NAME : {
      auto* name = static_cast<const ast::NameExpr*>(expr);
      if (!currentScope_->isDefined(name->name))
        reportIssue(Issue::Severity::ERROR, u"Undefined variable: " + name->name, expr->line, u"Did you forget to initialize it?");
      else
        currentScope_->markUsed(name->name, expr->line);
      break;
    }
    case ast::Expr::Kind::BINARY : {
      auto* bin = static_cast<const ast::BinaryExpr*>(expr);
      analyzeExpr(bin->left.get());
      analyzeExpr(bin->right.get());
      // Type compatibility checking
      auto leftType = inferType(bin->left.get());
      auto rightType = inferType(bin->right.get());
      if (leftType != rightType && leftType != SymbolTable::DataType::UNKNOWN && rightType != SymbolTable::DataType::UNKNOWN)
        // Check for invalid operations
        if ((bin->op == u"-" || bin->op == u"*" || bin->op == u"/")
            && (leftType == SymbolTable::DataType::STRING || rightType == SymbolTable::DataType::STRING))
          reportIssue(Issue::Severity::ERROR, u"Invalid operation on string", expr->line, u"Strings don't support this operator");
      // Division by zero detection (constant folding)
      if (bin->op == u"/" && bin->right->kind == ast::Expr::Kind::LITERAL)
      {
        auto* lit = static_cast<const ast::LiteralExpr*>(bin->right.get());
        if (lit->literal == u"0") reportIssue(Issue::Severity::ERROR, u"Division by zero", expr->line, u"This will cause a runtime error");
      }
      break;
    }
    case ast::Expr::Kind::UNARY : {
      const ast::UnaryExpr* un = dynamic_cast<const ast::UnaryExpr*>(expr);
      analyzeExpr(dynamic_cast<const ast::Expr*>(un));
      break;
    }
    case ast::Expr::Kind::CALL : {
      auto* call = static_cast<const ast::CallExpr*>(expr);
      analyzeExpr(call->callee.get());
      for (const auto& arg : call->args)
        analyzeExpr(arg.get());
      // Check if calling undefined function
      if (call->callee->kind == ast::Expr::Kind::NAME)
      {
        auto* name = static_cast<const ast::NameExpr*>(call->callee.get());
        if (auto* sym = currentScope_->lookup(name->name))
          if (sym->symbolType != SymbolTable::SymbolType::FUNCTION)
            reportIssue(Issue::Severity::ERROR, u"'" + name->name + u"' is not callable", expr->line);
      }
      break;
    }
    case ast::Expr::Kind::LIST : {
      auto* list = static_cast<const ast::ListExpr*>(expr);
      for (const auto& elem : list->elements)
        analyzeExpr(elem.get());
      break;
    }
    /*
    case ast::Expr::Kind::TERNARY : {
      auto* tern = static_cast<const ast::TernaryExpr*>(expr);
      analyzeExpr(tern->condition.get());
      analyzeExpr(tern->trueExpr.get());
      analyzeExpr(tern->falseExpr.get());
      break;
    }
    */
    default : break;
    }
  }

  void analyzeStmt(const ast::Stmt* stmt)
  {
    if (!stmt) return;

    switch (stmt->kind)
    {
    case ast::Stmt::Kind::ASSIGNMENT : {
      auto* assign = static_cast<const ast::AssignmentStmt*>(stmt);
      analyzeExpr(assign->value.get());
      auto type = inferType(assign->value.get());
      SymbolTable::Symbol sym;
      sym.symbolType = SymbolTable::SymbolType::VARIABLE;
      sym.dataType = type;
      sym.definitionLine = stmt->line;
      currentScope_->define(assign->target.name, sym);
      break;
    }

    case ast::Stmt::Kind::EXPR : {
      auto* exprStmt = static_cast<const ast::ExprStmt*>(stmt);
      analyzeExpr(exprStmt->expr.get());
      // Warn about unused expression results
      if (exprStmt->expr->kind != ast::Expr::Kind::CALL) reportIssue(Issue::Severity::INFO, u"Expression result not used", stmt->line);
      break;
    }

    case ast::Stmt::Kind::IF : {
      auto* ifStmt = static_cast<const ast::IfStmt*>(stmt);
      analyzeExpr(ifStmt->condition.get());
      // Check for constant conditions
      if (ifStmt->condition->kind == ast::Expr::Kind::LITERAL)
        reportIssue(Issue::Severity::WARNING, u"Condition is always constant", stmt->line, u"Consider removing if statement");
      for (const auto& s : ifStmt->then_stmts)
        analyzeStmt(s.get());
      for (const auto& s : ifStmt->else_stmts)
        analyzeStmt(s.get());
      break;
    }

    case ast::Stmt::Kind::WHILE : {
      auto* whileStmt = static_cast<const ast::WhileStmt*>(stmt);
      analyzeExpr(whileStmt->condition.get());
      // Detect infinite loops
      if (whileStmt->condition->kind == ast::Expr::Kind::LITERAL)
      {
        auto* lit = static_cast<const ast::LiteralExpr*>(whileStmt->condition.get());
        if (lit->type == ast::LiteralExpr::Type::BOOLEAN && lit->literal == u"true")
          reportIssue(Issue::Severity::WARNING, u"Infinite loop detected", stmt->line, u"Add a break condition");
      }
      for (const auto& s : whileStmt->stmts)
        analyzeStmt(s.get());
      break;
    }

    case ast::Stmt::Kind::FOR : {
      auto* forStmt = static_cast<const ast::ForStmt*>(stmt);
      analyzeExpr(forStmt->iter.get());
      // Create new scope for loop variable
      currentScope_ = currentScope_->createChild();
      SymbolTable::Symbol loopVar;
      loopVar.symbolType = SymbolTable::SymbolType::VARIABLE;
      loopVar.dataType = SymbolTable::DataType::ANY;
      currentScope_->define(forStmt->target, loopVar);
      for (const auto& s : forStmt->body)
        analyzeStmt(s.get());
      // Check if loop variable is shadowing
      if (currentScope_->parent && currentScope_->parent->lookupLocal(forStmt->target))
        reportIssue(Issue::Severity::WARNING, u"Loop variable shadows outer variable", stmt->line);
      // Exit loop scope
      currentScope_ = currentScope_->parent;
      break;
    }

    case ast::Stmt::Kind::FUNC : {
      auto* funcDef = static_cast<const ast::FunctionDef*>(stmt);
      SymbolTable::Symbol funcSym;
      funcSym.symbolType = SymbolTable::SymbolType::FUNCTION;
      funcSym.dataType = SymbolTable::DataType::FUNCTION;
      funcSym.definitionLine = stmt->line;
      currentScope_->define(funcDef->name, funcSym);
      // Create function scope
      currentScope_ = currentScope_->createChild();
      for (const auto& param : funcDef->params)
      {
        SymbolTable::Symbol paramSym;
        paramSym.symbolType = SymbolTable::SymbolType::VARIABLE;
        paramSym.dataType = SymbolTable::DataType::ANY;
        currentScope_->define(param, paramSym);
      }
      for (const auto& s : funcDef->body)
        analyzeStmt(s.get());
      // Check for missing return statement
      bool hasReturn = false;
      for (const auto& s : funcDef->body)
        if (s->kind == ast::Stmt::Kind::RETURN)
        {
          hasReturn = true;
          break;
        }
      if (!hasReturn) reportIssue(Issue::Severity::INFO, u"Function may not return a value", stmt->line);
      // Exit function scope
      currentScope_ = currentScope_->parent;
      break;
    }

    case ast::Stmt::Kind::RETURN : {
      auto* ret = static_cast<const ast::ReturnStmt*>(stmt);
      analyzeExpr(ret->value.get());
      break;
    }

    default : break;
    }
  }

 public:
  SemanticAnalyzer()
  {
    globalScope_ = std::make_unique<SymbolTable>();
    currentScope_ = globalScope_.get();
    // Add built-in functions
    SymbolTable::Symbol printSym;
    printSym.name = u"print";
    printSym.symbolType = SymbolTable::SymbolType::FUNCTION;
    printSym.dataType = SymbolTable::DataType::FUNCTION;
    globalScope_->define(u"print", printSym);
  }

  void analyze(const std::vector<ast::StmtPtr>& statements_)
  {
    for (const auto& stmt : statements_)
      analyzeStmt(stmt.get());
    // Check for unused variables
    auto unused = globalScope_->getUnusedSymbols();
    for (auto* sym : unused)
      reportIssue(Issue::Severity::WARNING, u"Unused variable: " + sym->name, sym->definitionLine, u"Consider removing if not needed");
  }

  const std::vector<Issue>& getIssues() const { return issues_; }

  const SymbolTable* getGlobalScope() const { return globalScope_.get(); }

  void printReport() const
  {
    if (issues_.empty())
    {
      std::cout << "✓ No issues found\n";
      return;
    }

    std::cout << "Found " << issues_.size() << " issue(s):\n\n";
    for (const auto& issue : issues_)
    {
      string_type sevStr;
      switch (issue.severity)
      {
      case Issue::Severity::ERROR : sevStr = u"ERROR"; break;
      case Issue::Severity::WARNING : sevStr = u"WARNING"; break;
      case Issue::Severity::INFO : sevStr = u"INFO"; break;
      }
      std::cout << "[" << utf8::utf16to8(sevStr) << "] Line " << issue.line << ": " << utf8::utf16to8(issue.message) << "\n";
      if (!issue.suggestion.empty()) std::cout << "  → " << utf8::utf16to8(issue.suggestion) << "\n";
      std::cout << "\n";
    }
  }
};

}
}
}