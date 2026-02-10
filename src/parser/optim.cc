#include "../../include/parser/optim.hpp"


namespace mylang {
namespace parser {

std::optional<double> ASTOptimizer::evaluateConstant(const ast::Expr* expr)
{
  if (!expr)
    return std::nullopt;

  if (expr->getKind() == ast::Expr::Kind::LITERAL)
  {
    const ast::LiteralExpr* lit = static_cast<const ast::LiteralExpr*>(expr);
    if (lit->getType() == ast::LiteralExpr::Type::NUMBER)
      return lit->getValue().toDouble();
  }
  else if (expr->getKind() == ast::Expr::Kind::BINARY)
  {
    const ast::BinaryExpr* bin   = static_cast<const ast::BinaryExpr*>(expr);
    std::optional<double>  left  = evaluateConstant(bin->getLeft());
    std::optional<double>  right = evaluateConstant(bin->getRight());
    if (left == std::nullopt || right == std::nullopt)
      return std::nullopt;
    if (bin->getOperator() == tok::TokenType::OP_PLUS)
      return *left + *right;
    if (bin->getOperator() == tok::TokenType::OP_MINUS)
      return *left - *right;
    if (bin->getOperator() == tok::TokenType::OP_STAR)
      return *left * *right;
    if (bin->getOperator() == tok::TokenType::OP_SLASH)
    {
      if (*right == 0.0)
        return std::nullopt;
      return *left / *right;
    }
    if (bin->getOperator() == tok::TokenType::OP_PERCENT)
    {
      if (*right == 0.0)
        return std::nullopt;
      return std::fmod(*left, *right);
    }
    if (bin->getOperator() == tok::TokenType::OP_POWER)
      return std::pow(*left, *right);
  }
  else if (expr->getKind() == ast::Expr::Kind::UNARY)
  {
    const ast::UnaryExpr* un      = static_cast<const ast::UnaryExpr*>(expr);
    std::optional<double> operand = evaluateConstant(static_cast<const ast::UnaryExpr*>(un));
    if (operand == std::nullopt)
      return std::nullopt;
    if (un->getOperator() == tok::TokenType::OP_PLUS)
      return *operand;
    if (un->getOperator() == tok::TokenType::OP_MINUS)
      return -*operand;
  }
  return std::nullopt;
}

ast::Expr* ASTOptimizer::optimizeConstantFolding(ast::Expr* expr)
{
  if (!expr)
    return expr;

  // First, optimize children
  if (expr->getKind() == ast::Expr::Kind::BINARY)
  {
    ast::BinaryExpr* bin = static_cast<ast::BinaryExpr*>(expr);
    bin->setLeft(optimizeConstantFolding(bin->getLeft()));
    bin->setRight(optimizeConstantFolding(bin->getRight()));
    // Try to evaluate
    if (std::optional<double> val = evaluateConstant(expr))
    {
      ++Stats_.ConstantFolds;
      return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER,
                                                       StringRef::fromUtf8(std::to_string(*val)));
    }
    // Algebraic simplifications
    ast::Expr* left  = bin->getLeft();
    ast::Expr* right = bin->getRight();
    // x + 0 = x, x - 0 = x
    if ((bin->getOperator() == tok::TokenType::OP_PLUS || bin->getOperator() == tok::TokenType::OP_MINUS)
        && right->getKind() == ast::Expr::Kind::LITERAL)
    {
      ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(right);
      if (lit->getValue() == u"0")
      {
        ++Stats_.StrengthReductions;
        return bin->getLeft();
      }
    }
    // x * 1 = x, x / 1 = x
    if ((bin->getOperator() == tok::TokenType::OP_STAR || bin->getOperator() == tok::TokenType::OP_SLASH)
        && right->getKind() == ast::Expr::Kind::LITERAL)
    {
      ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(right);
      if (lit->getValue() == u"1")
      {
        ++Stats_.StrengthReductions;
        return bin->getLeft();
      }
    }
    // x * 0 = 0
    if (bin->getOperator() == tok::TokenType::OP_STAR && right->getKind() == ast::Expr::Kind::LITERAL)
    {
      ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(right);
      if (lit->getValue() == u"0")
      {
        ++Stats_.StrengthReductions;
        return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, u"0");
      }
    }
    // x * 2 = x + x (strength reduction)
    if (bin->getOperator() == tok::TokenType::OP_STAR && right->getKind() == ast::Expr::Kind::LITERAL)
    {
      auto* lit = static_cast<ast::LiteralExpr*>(right);
      if (lit->getValue() == u"2")
      {
        ++Stats_.StrengthReductions;
        ast::NameExpr* leftClone =
          ast::AST_allocator.make<ast::NameExpr>(static_cast<ast::NameExpr*>(left)->getValue());
        return ast::AST_allocator.make<ast::BinaryExpr>(bin->getLeft(), leftClone, tok::TokenType::OP_PLUS);
      }
    }
    // x - x = 0
    if (bin->getOperator() == tok::TokenType::OP_MINUS && left->getKind() == ast::Expr::Kind::NAME
        && right->getKind() == ast::Expr::Kind::NAME)
    {
      ast::NameExpr* lname = static_cast<ast::NameExpr*>(left);
      ast::NameExpr* rname = static_cast<ast::NameExpr*>(right);
      if (lname->getValue() == rname->getValue())
      {
        ++Stats_.StrengthReductions;
        return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER, u"0");
      }
    }
  }
  /// TODO:: ???
  else if (expr->getKind() == ast::Expr::Kind::UNARY)
  {
    ast::UnaryExpr* un = static_cast<ast::UnaryExpr*>(expr);
    un                 = static_cast<ast::UnaryExpr*>(optimizeConstantFolding(un));
    if (std::optional<double> val = evaluateConstant(expr))
    {
      ++Stats_.ConstantFolds;
      return ast::AST_allocator.make<ast::LiteralExpr>(ast::LiteralExpr::Type::NUMBER,
                                                       StringRef::fromUtf8(std::to_string(*val)));
    }
    // Double negation: --x = x
    if (un->getOperator() == tok::TokenType::OP_MINUS && un->getKind() == ast::Expr::Kind::UNARY)
    {
      ast::UnaryExpr* innerUn = static_cast<ast::UnaryExpr*>(un);
      if (innerUn->getOperator() == tok::TokenType::OP_MINUS)
      {
        ++Stats_.StrengthReductions;
        return static_cast<ast::Expr*>(innerUn);
      }
    }
  }
  else if (expr->getKind() == ast::Expr::Kind::CALL)
  {
    ast::CallExpr* call = static_cast<ast::CallExpr*>(expr);
    for (ast::Expr*& arg : call->getArgsMutable())
      arg = optimizeConstantFolding(arg);
  }
  else if (expr->getKind() == ast::Expr::Kind::LIST)
  {
    ast::ListExpr* list = static_cast<ast::ListExpr*>(expr);
    for (ast::Expr*& elem : list->getElementsMutable())
      elem = optimizeConstantFolding(elem);
  }
  return expr;
}

ast::Stmt* ASTOptimizer::eliminateDeadCode(ast::Stmt* stmt)
{
  if (!stmt)
    return stmt;

  if (stmt->getKind() == ast::Stmt::Kind::IF)
  {
    ast::IfStmt* ifStmt = static_cast<ast::IfStmt*>(stmt);
    // Constant condition elimination
    if (ifStmt->getCondition()->getKind() == ast::Expr::Kind::LITERAL)
    {
      ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(ifStmt->getCondition());
      if (lit->getType() == ast::LiteralExpr::Type::BOOLEAN)
      {
        ++Stats_.DeadCodeEliminations;
        if (lit->getValue() == u"true")
          // Return then block as block statement
          return static_cast<ast::Stmt*>(ifStmt->getThenBlock());
        else
        {
          // Return else block or nothing
          if (!ifStmt->getElseBlock()->getStatements().empty())
            return static_cast<ast::Stmt*>(ifStmt->getElseBlock());
        }
        /// TODO:: return std::make_unique<Stmt*>();
      }
    }
    // Recursively eliminate in blocks
    std::vector<ast::Stmt*> newThenStmts;
    std::vector<ast::Stmt*> newElseStmts;

    for (ast::Stmt* const& s : ifStmt->getThenBlock()->getStatements())
      if (ast::Stmt* opt = eliminateDeadCode(s))
        newThenStmts.push_back(opt);

    for (ast::Stmt* const& s : ifStmt->getElseBlock()->getStatements())
      if (ast::Stmt* opt = eliminateDeadCode(s))
        newElseStmts.push_back(opt);

    ast::BlockStmt* newThen = ast::AST_allocator.make<ast::BlockStmt>(newThenStmts);
    ast::BlockStmt* newElse = ast::AST_allocator.make<ast::BlockStmt>(newElseStmts);
    ifStmt->setThenBlock(newThen);
    ifStmt->setElseBlock(newElse);
  }
  else if (stmt->getKind() == ast::Stmt::Kind::WHILE)
  {
    ast::WhileStmt* whileStmt = static_cast<ast::WhileStmt*>(stmt);
    // Infinite loop with false condition
    if (whileStmt->getCondition()->getKind() == ast::Expr::Kind::LITERAL)
    {
      ast::LiteralExpr* lit = static_cast<ast::LiteralExpr*>(whileStmt->getCondition());
      if (lit->getType() == ast::LiteralExpr::Type::BOOLEAN && lit->getValue() == u"false")
        ++Stats_.DeadCodeEliminations;
      /// TODO:: return std::make_unique<PassStmt>();
    }
    std::vector<ast::Stmt*> newBody;
    for (ast::Stmt* const& s : whileStmt->getBlock()->getStatements())
      if (ast::Stmt* opt = eliminateDeadCode(s))
        newBody.push_back(opt);
    whileStmt->getBlockMutable()->setStatements(newBody);
  }
  else if (stmt->getKind() == ast::Stmt::Kind::FOR)
  {
    ast::ForStmt*           forStmt = static_cast<ast::ForStmt*>(stmt);
    std::vector<ast::Stmt*> newBodyStmts;
    for (ast::Stmt* const& s : forStmt->getBlock()->getStatements())
      if (ast::Stmt* opt = eliminateDeadCode(s))
        newBodyStmts.push_back(opt);
    ast::BlockStmt* newBody = ast::AST_allocator.make<ast::BlockStmt>(newBodyStmts);
    forStmt->setBlock(newBody);
  }
  else if (stmt->getKind() == ast::Stmt::Kind::FUNC)
  {
    ast::FunctionDef*       funcDef = static_cast<ast::FunctionDef*>(stmt);
    std::vector<ast::Stmt*> newBodyStmts;
    bool                    seenReturn = false;
    for (ast::Stmt* const& s : funcDef->getBody()->getStatements())
    {
      if (seenReturn)
      {
        ++Stats_.DeadCodeEliminations;
        continue;  // Skip statements after return
      }
      if (s->getKind() == ast::Stmt::Kind::RETURN)
        seenReturn = true;
      if (ast::Stmt* opt = eliminateDeadCode(s))
        newBodyStmts.push_back(opt);
    }
    ast::BlockStmt* newBody = ast::AST_allocator.make<ast::BlockStmt>(newBodyStmts);
    funcDef->setBody(newBody);
  }

  return stmt;
}

StringRef ASTOptimizer::CSEPass::exprToString(const ast::Expr* expr)
{
  if (!expr)
    return u"";

  switch (expr->getKind())
  {
  case ast::Expr::Kind::LITERAL : {
    const ast::LiteralExpr* lit = static_cast<const ast::LiteralExpr*>(expr);
    return lit->getValue();
  }
  case ast::Expr::Kind::NAME : {
    const ast::NameExpr* name = static_cast<const ast::NameExpr*>(expr);
    return name->getValue();
  }
  case ast::Expr::Kind::BINARY : {
    const ast::BinaryExpr* bin = static_cast<const ast::BinaryExpr*>(expr);
    return u"(" + exprToString(bin->getLeft()) + u" " + tok::toString(bin->getOperator()) + u" "
           + exprToString(bin->getRight()) + u")";
  }
  case ast::Expr::Kind::UNARY : {
    const ast::UnaryExpr* un = static_cast<const ast::UnaryExpr*>(expr);
    return tok::toString(un->getOperator()) + exprToString(un);
  }
  default : return u"";
  }
}

StringRef ASTOptimizer::CSEPass::getTempVar()
{
  return StringRef::fromUtf8("__cse_temp_") + static_cast<CharType>(TempCounter_++);
}

std::optional<StringRef> ASTOptimizer::CSEPass::findCSE(const ast::Expr* expr)
{
  StringRef exprStr = exprToString(expr);
  if (exprStr.empty())
    return std::nullopt;
  auto it = ExprCache_.find(exprStr);
  if (it != ExprCache_.end())
    return it->second;
  return std::nullopt;
}

void ASTOptimizer::CSEPass::recordExpr(const ast::Expr* expr, const StringRef& var)
{
  StringRef exprStr = exprToString(expr);
  if (!exprStr.empty())
    ExprCache_[exprStr] = var;
}

bool ASTOptimizer::isLoopInvariant(const ast::Expr*                                                    expr,
                                   const std::unordered_set<StringRef, StringRefHash, StringRefEqual>& loopVars)
{
  if (!expr)
    return true;

  if (expr->getKind() == ast::Expr::Kind::NAME)
  {
    const ast::NameExpr* name = static_cast<const ast::NameExpr*>(expr);
    return !loopVars.count(name->getValue());
  }
  else if (expr->getKind() == ast::Expr::Kind::BINARY)
  {
    const ast::BinaryExpr* bin = static_cast<const ast::BinaryExpr*>(expr);
    return isLoopInvariant(bin->getLeft(), loopVars) && isLoopInvariant(bin->getRight(), loopVars);
  }
  else if (expr->getKind() == ast::Expr::Kind::UNARY)
  {
    const ast::UnaryExpr* un = static_cast<const ast::UnaryExpr*>(expr);
    return isLoopInvariant(un, loopVars);
  }
  else if (expr->getKind() == ast::Expr::Kind::LITERAL)
    return true;
  return false;
}

std::vector<ast::Stmt*> ASTOptimizer::optimize(std::vector<ast::Stmt*> statements, std::int32_t level)
{
  std::vector<ast::Stmt*> result;
  for (ast::Stmt*& stmt : statements)
  {
    // Apply optimizations based on level
    if (level >= 1)
    {
      // O1: Basic optimizations
      if (stmt->getKind() == ast::Stmt::Kind::ASSIGNMENT)
      {
        ast::AssignmentStmt* assign = static_cast<ast::AssignmentStmt*>(stmt);
        assign->setValue(optimizeConstantFolding(assign->getValue()));
      }
      else if (stmt->getKind() == ast::Stmt::Kind::EXPR)
      {
        ast::ExprStmt* exprStmt = static_cast<ast::ExprStmt*>(stmt);
        exprStmt->setExpr(optimizeConstantFolding(exprStmt->getExpr()));
      }
    }
    // O2: Dead code elimination
    if (level >= 2)
      stmt = eliminateDeadCode(stmt);
    if (stmt)
      result.push_back(stmt);
  }
  return result;
}

const typename ASTOptimizer::OptimizationStats& ASTOptimizer::getStats() const { return Stats_; }

void ASTOptimizer::printStats() const
{
  std::cout << "\n=== Optimization Statistics ===\n";
  std::cout << "Constant folds: " << Stats_.ConstantFolds << "\n";
  std::cout << "Dead code eliminations: " << Stats_.DeadCodeEliminations << "\n";
  std::cout << "Strength reductions: " << Stats_.StrengthReductions << "\n";
  std::cout << "Common subexpr eliminations: " << Stats_.CommonSubexprEliminations << "\n";
  std::cout << "Loop invariants moved: " << Stats_.LoopInvariants << "\n";
  std::cout << "Total optimizations: "
            << (Stats_.ConstantFolds + Stats_.DeadCodeEliminations + Stats_.StrengthReductions
                + Stats_.CommonSubexprEliminations + Stats_.LoopInvariants)
            << "\n";
}

}
}