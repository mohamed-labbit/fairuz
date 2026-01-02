#pragma once

#include "ast/ast.hpp"

#include <string>
#include <vector>


namespace mylang {
namespace parser {

class CodeGenerator
{
 public:
  enum class Target { Bytecode, CPP, LLVM };

  struct Bytecode
  {
    enum class Op { LOAD_CONST, LOAD_VAR, STORE_VAR, ADD, SUB, MUL, DIV, JUMP, JUMP_IF_FALSE, CALL, RETURN, POP, DUP };

    Op opcode;
    std::int32_t arg;
  };

  // Generate Python bytecode-like instructions
  std::vector<Bytecode> generateBytecode(const std::vector<ast::Stmt*>& ast);

  // Generate C++ code
  std::string generateCPP(const std::vector<ast::Stmt*>& ast);

 private:
  void generateStmt(const ast::Stmt* stmt, std::vector<Bytecode>& code);

  void generateExpr(const ast::Expr* expr, std::vector<Bytecode>& code);

  string_type generateCPPStmt(const ast::Stmt* stmt);

  string_type generateCPPExpr(const ast::Expr* expr);
};

}
}