#include "../../include/parser/codegen.hpp"


namespace mylang {
namespace parser {

// Generate Python bytecode-like instructions
std::vector<typename CodeGenerator::Bytecode> CodeGenerator::generateBytecode(const std::vector<ast::Stmt*>& ast)
{
  std::vector<Bytecode> code;
  for (const auto& stmt : ast)
    generateStmt(stmt, code);
  return code;
}

// Generate C++ code
std::string CodeGenerator::generateCPP(const std::vector<ast::Stmt*>& ast)
{
  std::stringstream ss;
  ss << "#include <iostream>\n";
  ss << "#include <vector>\n";
  ss << "#include <string>\n\n";
  for (const auto& stmt : ast)
    ss << utf8::utf16to8(generateCPPStmt(stmt));
  return ss.str();
}

void CodeGenerator::generateStmt(const ast::Stmt* stmt, std::vector<Bytecode>& code)
{
  if (!stmt) return;

  switch (stmt->getKind())
  {
  case ast::Stmt::Kind::ASSIGNMENT : {
    auto* assign = static_cast<const ast::AssignmentStmt*>(stmt);
    generateExpr(assign->getValue(), code);
    code.push_back({Bytecode::Op::STORE_VAR, 0});  // Store to variable
    break;
  }
  case ast::Stmt::Kind::EXPR : {
    auto* exprStmt = static_cast<const ast::ExprStmt*>(stmt);
    generateExpr(exprStmt->getExpr(), code);
    code.push_back({Bytecode::Op::POP, 0});
    break;
  }
  case ast::Stmt::Kind::RETURN : {
    auto* ret = static_cast<const ast::ReturnStmt*>(stmt);
    generateExpr(ret->getValue(), code);
    code.push_back({Bytecode::Op::RETURN, 0});
    break;
  }
  default : break;
  }
}

void CodeGenerator::generateExpr(const ast::Expr* expr, std::vector<Bytecode>& code)
{
  if (!expr) return;

  switch (expr->getKind())
  {
  case ast::Expr::Kind::LITERAL : code.push_back({Bytecode::Op::LOAD_CONST, 0}); break;
  case ast::Expr::Kind::NAME : code.push_back({Bytecode::Op::LOAD_VAR, 0}); break;
  case ast::Expr::Kind::BINARY : {
    const ast::BinaryExpr* bin = static_cast<const ast::BinaryExpr*>(expr);
    generateExpr(bin->getLeft(), code);
    generateExpr(bin->getRight(), code);
    if (bin->getOperator() == lex::tok::TokenType::OP_PLUS) { code.push_back({Bytecode::Op::ADD, 0}); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_MINUS) { code.push_back({Bytecode::Op::SUB, 0}); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_STAR) { code.push_back({Bytecode::Op::MUL, 0}); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_SLASH) { code.push_back({Bytecode::Op::DIV, 0}); }
    break;
  }
  default : break;
  }
}

string_type CodeGenerator::generateCPPStmt(const ast::Stmt* stmt)
{
  if (!stmt) return u"";

  switch (stmt->getKind())
  {
  case ast::Stmt::Kind::ASSIGNMENT : {
    auto* assign = static_cast<const ast::AssignmentStmt*>(stmt);
    return u"auto " + assign->getTarget()->getValue() + u" = " + generateCPPExpr(assign->getValue()) + u";\n";
  }
  case ast::Stmt::Kind::EXPR : {
    auto* exprStmt = static_cast<const ast::ExprStmt*>(stmt);
    return generateCPPExpr(exprStmt->getExpr()) + u";\n";
  }
  case ast::Stmt::Kind::IF : {
    auto* ifStmt = static_cast<const ast::IfStmt*>(stmt);
    string_type result = u"if (" + generateCPPExpr(ifStmt->getCondition()) + u") {\n";
    for (const auto* s : ifStmt->getThenBlock()->getStatements())
      result += u"  " + generateCPPStmt(s);
    result += u"}\n";
    return result;
  }
  case ast::Stmt::Kind::FUNC : {
    auto* func = static_cast<const ast::FunctionDef*>(stmt);
    string_type result = u"auto " + func->getName()->getValue() + u"(";
    auto params = func->getParameters();
    for (std::size_t i = 0; i < params.size(); i++)
    {
      result += u"auto " + params[i]->getValue();
      if (i + 1 < params.size()) result += u", ";
    }
    result += u") {\n";
    auto body = func->getBody();
    for (const auto& s : body->getStatements())
      result += u"  " + generateCPPStmt(s);
    result += u"}\n";
    return result;
  }
  default : return u"";
  }

  return u"";
}

string_type CodeGenerator::generateCPPExpr(const ast::Expr* expr)
{
  if (!expr) return u"";

  switch (expr->getKind())
  {
  case ast::Expr::Kind::LITERAL : {
    auto* lit = static_cast<const ast::LiteralExpr*>(expr);
    return lit->getValue();
  }
  case ast::Expr::Kind::NAME : {
    auto* name_expr = static_cast<const ast::NameExpr*>(expr);
    return name_expr->getValue();
  }
  case ast::Expr::Kind::BINARY : {
    auto* bin = static_cast<const ast::BinaryExpr*>(expr);
    return u"(" + generateCPPExpr(bin->getLeft()) + u" " + lex::tok::to_string(bin->getOperator()) + u" " + generateCPPExpr(bin->getRight()) + u")";
  }
  case ast::Expr::Kind::CALL : {
    auto* call = static_cast<const ast::CallExpr*>(expr);
    string_type result = generateCPPExpr(call->getCallee()) + u"(";
    for (std::size_t i = 0; i < call->getArgs().size(); i++)
    {
      result += generateCPPExpr(call->getArgs()[i]);
      if (i + 1 < call->getArgs().size()) result += u", ";
    }
    result += u")";
    return result;
  }
  default : return u"";
  }

  return u"";
}

}
}