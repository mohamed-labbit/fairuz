#include "../../include/parser/analyzer.hpp"
#include "../../utfcpp/source/utf8.h"

#include <iostream>


namespace mylang {
namespace parser {

// semantic analyzer

// Type inference engine
typename SymbolTable::DataType_t SemanticAnalyzer::inferType(const ast::Expr* expr)
{
  if (!expr) return SymbolTable::DataType_t::UNKNOWN;

  switch (expr->getKind())
  {
  case ast::Expr::Kind::LITERAL : {
    auto* lit = static_cast<const ast::LiteralExpr*>(expr);
    switch (lit->getType())
    {
    case ast::LiteralExpr::Type::NUMBER :
      return lit->getValue().find('.') != std::string::npos ? SymbolTable::DataType_t::FLOAT : SymbolTable::DataType_t::INTEGER;
    case ast::LiteralExpr::Type::STRING : return SymbolTable::DataType_t::STRING;
    case ast::LiteralExpr::Type::BOOLEAN : return SymbolTable::DataType_t::BOOLEAN;
    case ast::LiteralExpr::Type::NONE : return SymbolTable::DataType_t::NONE;
    }
    break;
  }
  case ast::Expr::Kind::NAME : {
    auto* name = static_cast<const ast::NameExpr*>(expr);
    if (auto* sym = CurrentScope_->lookup(name->getValue())) return sym->DataType;
    break;
  }
  case ast::Expr::Kind::BINARY : {
    auto* bin = static_cast<const ast::BinaryExpr*>(expr);
    auto leftType = inferType(bin->getLeft());
    auto rightType = inferType(bin->getRight());
    // Type promotion rules
    if (leftType == SymbolTable::DataType_t::FLOAT || rightType == SymbolTable::DataType_t::FLOAT) return SymbolTable::DataType_t::FLOAT;
    if (leftType == SymbolTable::DataType_t::INTEGER && rightType == SymbolTable::DataType_t::INTEGER) return SymbolTable::DataType_t::INTEGER;
    if (bin->getOperator() == lex::tok::TokenType::OP_PLUS && leftType == SymbolTable::DataType_t::STRING) return SymbolTable::DataType_t::STRING;
    if (bin->getOperator() == lex::tok::TokenType::KW_AND || bin->getOperator() == lex::tok::TokenType::KW_OR)
      return SymbolTable::DataType_t::BOOLEAN;
    break;
  }
  case ast::Expr::Kind::LIST : return SymbolTable::DataType_t::LIST;
  case ast::Expr::Kind::CALL : return SymbolTable::DataType_t::ANY;
  default : break;
  }
  return SymbolTable::DataType_t::UNKNOWN;
}

void SemanticAnalyzer::reportIssue(Issue::Severity sev, const string_type& msg, std::int32_t line, const string_type& sugg)
{
  Issues_.push_back({sev, msg, line, sugg});
}

void SemanticAnalyzer::analyzeExpr(const ast::Expr* expr)
{
  if (!expr) return;

  switch (expr->getKind())
  {
  case ast::Expr::Kind::NAME : {
    auto* name = static_cast<const ast::NameExpr*>(expr);
    if (!CurrentScope_->isDefined(name->getValue()))
      reportIssue(Issue::Severity::ERROR, u"Undefined variable: " + name->getValue(), expr->line, u"Did you forget to initialize it?");
    else
      CurrentScope_->markUsed(name->getValue(), expr->line);
    break;
  }

  case ast::Expr::Kind::BINARY : {
    auto* bin = static_cast<const ast::BinaryExpr*>(expr);
    analyzeExpr(bin->getLeft());
    analyzeExpr(bin->getRight());
    // Type compatibility checking
    auto leftType = inferType(bin->getLeft());
    auto rightType = inferType(bin->getRight());
    if (leftType != rightType && leftType != SymbolTable::DataType_t::UNKNOWN && rightType != SymbolTable::DataType_t::UNKNOWN)
      // Check for invalid operations
      if ((bin->getOperator() == lex::tok::TokenType::OP_MINUS || bin->getOperator() == lex::tok::TokenType::OP_STAR
           || bin->getOperator() == lex::tok::TokenType::OP_SLASH)
          && (leftType == SymbolTable::DataType_t::STRING || rightType == SymbolTable::DataType_t::STRING))
        reportIssue(Issue::Severity::ERROR, u"Invalid operation on string", expr->line, u"Strings don't support this operator");
    // Division by zero detection (constant folding)
    if (bin->getOperator() == lex::tok::TokenType::OP_SLASH && bin->getRight()->getKind() == ast::Expr::Kind::LITERAL)
    {
      auto* lit = static_cast<const ast::LiteralExpr*>(bin->getRight());
      if (lit->getValue() == u"0") reportIssue(Issue::Severity::ERROR, u"Division by zero", expr->line, u"This will cause a runtime error");
    }
    break;
  }

  case ast::Expr::Kind::UNARY : {
    const ast::UnaryExpr* un = dynamic_cast<const ast::UnaryExpr*>(expr);
    analyzeExpr(dynamic_cast<const ast::Expr*>(un));
    break;
  }

  case ast::Expr::Kind::CALL : {
    auto* call = dynamic_cast<const ast::CallExpr*>(expr);
    analyzeExpr(call->getCallee());
    for (const auto& arg : call->getArgs()) analyzeExpr(arg);
    // Check if calling undefined function
    if (call->getCallee()->getKind() == ast::Expr::Kind::NAME)
    {
      auto* name = static_cast<const ast::NameExpr*>(call->getCallee());
      if (auto* sym = CurrentScope_->lookup(name->getValue()))
        if (sym->SymbolType != SymbolTable::SymbolType::FUNCTION)
          reportIssue(Issue::Severity::ERROR, u"'" + name->getValue() + u"' is not callable", expr->line);
    }
    break;
  }

  case ast::Expr::Kind::LIST : {
    auto* list = dynamic_cast<const ast::ListExpr*>(expr);
    for (const auto& elem : list->getElements()) analyzeExpr(elem);
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

void SemanticAnalyzer::analyzeStmt(const ast::Stmt* stmt)
{
  if (!stmt) return;

  switch (stmt->getKind())
  {
  case ast::Stmt::Kind::ASSIGNMENT : {
    auto* assign = static_cast<const ast::AssignmentStmt*>(stmt);
    analyzeExpr(assign->getValue());
    auto type = inferType(assign->getValue());
    SymbolTable::Symbol sym;
    sym.SymbolType = SymbolTable::SymbolType::VARIABLE;
    sym.DataType = type;
    sym.DefinitionLine = stmt->line;
    CurrentScope_->define(assign->getTarget()->getValue(), sym);
    break;
  }

  case ast::Stmt::Kind::EXPR : {
    auto* exprStmt = static_cast<const ast::ExprStmt*>(stmt);
    analyzeExpr(exprStmt->getExpr());
    // Warn about unused expression results
    if (exprStmt->getExpr()->getKind() != ast::Expr::Kind::CALL) reportIssue(Issue::Severity::INFO, u"Expression result not used", stmt->line);
    break;
  }

  case ast::Stmt::Kind::IF : {
    auto* ifStmt = static_cast<const ast::IfStmt*>(stmt);
    analyzeExpr(ifStmt->getCondition());
    // Check for constant conditions
    if (ifStmt->getCondition()->getKind() == ast::Expr::Kind::LITERAL)
      reportIssue(Issue::Severity::WARNING, u"Condition is always constant", stmt->line, u"Consider removing if statement");
    for (const auto& s : ifStmt->getThenBlock()->getStatements()) analyzeStmt(s);
    for (const auto& s : ifStmt->getElseBlock()->getStatements()) analyzeStmt(s);
    break;
  }

  case ast::Stmt::Kind::WHILE : {
    auto* whileStmt = static_cast<const ast::WhileStmt*>(stmt);
    analyzeExpr(whileStmt->getCondition());
    // Detect infinite loops
    if (whileStmt->getCondition()->getKind() == ast::Expr::Kind::LITERAL)
    {
      auto* lit = static_cast<const ast::LiteralExpr*>(whileStmt->getCondition());
      if (lit->getType() == ast::LiteralExpr::Type::BOOLEAN && lit->getValue() == u"true")
        reportIssue(Issue::Severity::WARNING, u"Infinite loop detected", stmt->line, u"Add a break condition");
    }
    for (const auto& s : whileStmt->getBlock()->getStatements()) analyzeStmt(s);
    break;
  }

  case ast::Stmt::Kind::FOR : {
    auto* forStmt = static_cast<const ast::ForStmt*>(stmt);
    analyzeExpr(forStmt->getIter());
    // Create new scope for loop variable
    CurrentScope_ = CurrentScope_->createChild();
    SymbolTable::Symbol loopVar;
    loopVar.SymbolType = SymbolTable::SymbolType::VARIABLE;
    loopVar.DataType = SymbolTable::DataType_t::ANY;
    CurrentScope_->define(forStmt->getTarget()->getValue(), loopVar);
    for (const auto& s : forStmt->getBlock()->getStatements()) analyzeStmt(s);
    // Check if loop variable is shadowing
    if (CurrentScope_->parent && CurrentScope_->parent->lookupLocal(forStmt->getTarget()->getValue()))
      reportIssue(Issue::Severity::WARNING, u"Loop variable shadows outer variable", stmt->line);
    // Exit loop scope
    CurrentScope_ = CurrentScope_->parent;
    break;
  }

  case ast::Stmt::Kind::FUNC : {
    auto* funcDef = dynamic_cast<const ast::FunctionDef*>(stmt);
    SymbolTable::Symbol funcSym;
    funcSym.SymbolType = SymbolTable::SymbolType::FUNCTION;
    funcSym.DataType = SymbolTable::DataType_t::FUNCTION;
    funcSym.DefinitionLine = stmt->line;
    CurrentScope_->define(funcDef->getName()->getValue(), funcSym);
    // Create function scope
    CurrentScope_ = CurrentScope_->createChild();
    for (const auto& param : funcDef->getParameters())
    {
      SymbolTable::Symbol paramSym;
      paramSym.SymbolType = SymbolTable::SymbolType::VARIABLE;
      paramSym.DataType = SymbolTable::DataType_t::ANY;
      CurrentScope_->define(param->getValue(), paramSym);
    }
    for (const auto& s : funcDef->getBody()->getStatements()) analyzeStmt(s);
    // Check for missing return statement
    bool hasReturn = false;
    for (const auto& s : funcDef->getBody()->getStatements())
    {
      if (s->getKind() == ast::Stmt::Kind::RETURN)
      {
        hasReturn = true;
        break;
      }
    }
    if (!hasReturn) reportIssue(Issue::Severity::INFO, u"Function may not return a value", stmt->line);
    // Exit function scope
    CurrentScope_ = CurrentScope_->parent;
    break;
  }

  case ast::Stmt::Kind::RETURN : {
    auto* ret = static_cast<const ast::ReturnStmt*>(stmt);
    analyzeExpr(ret->getValue());
    break;
  }

  default : break;
  }
}

SemanticAnalyzer::SemanticAnalyzer()
{
  GlobalScope_ = std::make_unique<SymbolTable>();
  CurrentScope_ = GlobalScope_.get();
  // Add built-in functions
  SymbolTable::Symbol printSym;
  printSym.name = u"print";
  printSym.SymbolType = SymbolTable::SymbolType::FUNCTION;
  printSym.DataType = SymbolTable::DataType_t::FUNCTION;
  GlobalScope_->define(u"print", printSym);
}

void SemanticAnalyzer::analyze(const std::vector<ast::Stmt*>& Statements_)
{
  for (const auto& stmt : Statements_) analyzeStmt(stmt);
  // Check for unused variables
  auto unused = GlobalScope_->getUnusedSymbols();
  for (auto* sym : unused)
    reportIssue(Issue::Severity::WARNING, u"Unused variable: " + sym->name, sym->DefinitionLine, u"Consider removing if not needed");
}

const std::vector<typename SemanticAnalyzer::Issue>& SemanticAnalyzer::getIssues() const { return Issues_; }

const SymbolTable* SemanticAnalyzer::getGlobalScope() const { return GlobalScope_.get(); }

void SemanticAnalyzer::printReport() const
{
  if (Issues_.empty())
  {
    std::cout << "✓ No issues found\n";
    return;
  }

  std::cout << "Found " << Issues_.size() << " issue(s):\n\n";
  for (const auto& issue : Issues_)
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

}
}
