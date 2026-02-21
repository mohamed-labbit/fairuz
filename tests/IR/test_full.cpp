#include "../../include/IR/codegen.hpp"
#include "../../include/IR/env.hpp"
#include "../../include/IR/value.hpp"
#include "../../include/ast/ast.hpp"
#include "../../include/ast/expr.hpp"
#include "../../include/ast/printer.hpp"
#include "../../include/lex/file_manager.hpp"
#include "../../include/lex/lexer.hpp"
#include "../../include/lex/token.hpp"
#include "../../include/parser/parser.hpp"
#include "../test_config.h"
#include <gtest/gtest.h>

using namespace mylang;

TEST(TestFull, TestWhileLoop)
{
    lex::FileManager file_manager("/Users/mohamedrabbit/code/mylang/tests/IR/test_cases/test_while.txt");
    parser::Parser test_parser(&file_manager);
    std::vector<ast::Stmt*> statements = test_parser.parseProgram();

    ASSERT_FALSE(statements.empty());

    ast::ASTPrinter AST_Printer;
    if (test_config::print_ast) {
        for (ast::Stmt const* s : statements)
            AST_Printer.print(s);
    }

    IR::Environment env;
    IR::CodeGenerator code_generator(&env);

    IR::Value result;
    int i = 0;
    for (ast::Stmt const* stmt : statements) {
        std::cout << "EVAL ITER = " << std::to_string(i) << std::endl;
        result = code_generator.eval(stmt);
        i++;
    }

    EXPECT_TRUE(result.isNumber());
    EXPECT_DOUBLE_EQ(result.toInt(), 3);
}
