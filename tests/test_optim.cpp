#include "../fairuz/fAST.hpp"
#include "../fairuz/foptim.hpp"
#include "test_common.h"

#include <gtest/gtest.h>

using namespace fairuz::AST;
using namespace fairuz::runtime;

TEST(OptimTest, DoesNotRewriteBitnotAsLogicalOperations)
{
    // ~~x;      => x != 0
    // ~(x == y) => x != y
    // ~(x != y) => x == y
    // ~(x < y)  => x >= y
    // ~(x > y)  => x <= y
    // ~(x <= y) => x > y
    // ~(x >= y) => x < y

    NameExpr* x = name_expr("x");
    NameExpr* y = name_expr("y");

    UnaryExpr* ast_1 = unary(unary(x, UnaryOp::OP_BITNOT), UnaryOp::OP_BITNOT);
    UnaryExpr* ast_2 = unary(binary(x, y, BinaryOp::OP_EQ), UnaryOp::OP_BITNOT);
    UnaryExpr* ast_3 = unary(binary(x, y, BinaryOp::OP_NEQ), UnaryOp::OP_BITNOT);
    UnaryExpr* ast_4 = unary(binary(x, y, BinaryOp::OP_LT), UnaryOp::OP_BITNOT);
    UnaryExpr* ast_5 = unary(binary(x, y, BinaryOp::OP_GT), UnaryOp::OP_BITNOT);
    UnaryExpr* ast_6 = unary(binary(x, y, BinaryOp::OP_LTE), UnaryOp::OP_BITNOT);
    UnaryExpr* ast_7 = unary(binary(x, y, BinaryOp::OP_GTE), UnaryOp::OP_BITNOT);

    auto ret_1 = try_strength_reduce_unary(ast_1);
    auto ret_2 = try_strength_reduce_unary(ast_2);
    auto ret_3 = try_strength_reduce_unary(ast_3);
    auto ret_4 = try_strength_reduce_unary(ast_4);
    auto ret_5 = try_strength_reduce_unary(ast_5);
    auto ret_6 = try_strength_reduce_unary(ast_6);
    auto ret_7 = try_strength_reduce_unary(ast_7);

    EXPECT_FALSE(ret_1.has_value());
    EXPECT_FALSE(ret_2.has_value());
    EXPECT_FALSE(ret_3.has_value());
    EXPECT_FALSE(ret_4.has_value());
    EXPECT_FALSE(ret_5.has_value());
    EXPECT_FALSE(ret_6.has_value());
    EXPECT_FALSE(ret_7.has_value());
}

TEST(OptimTest, DoesNotDiscardDynamicOperandErrorsOrOverloads)
{
    // x * 0 = 0
    // x * 1 = x
    // x * 2 = x + x
    // x / 1 = x
    // x / -1 = -x
    // x & 0 = 0
    // x & -1 = x
    // x | 0 = x
    // x | -1 = -1
    // x ^ 0 = x
    // x ^ -1 = ~x
    // x << 0 = x
    // x >> 0 = x

    Expr* x = name_expr("x");
    Expr* zero = lit_int(0);
    Expr* one = lit_int(1);
    Expr* two = lit_int(2);
    Expr* neg = lit_int(-1);

    auto ret_1 = try_strength_reduce_binary(binary(x, zero, BinaryOp::OP_MUL));
    auto ret_2 = try_strength_reduce_binary(binary(x, one, BinaryOp::OP_MUL));
    auto ret_3 = try_strength_reduce_binary(binary(x, two, BinaryOp::OP_MUL));
    auto ret_4 = try_strength_reduce_binary(binary(x, one, BinaryOp::OP_DIV));
    auto ret_5 = try_strength_reduce_binary(binary(x, neg, BinaryOp::OP_DIV));
    auto ret_6 = try_strength_reduce_binary(binary(x, zero, BinaryOp::OP_BITAND));
    auto ret_7 = try_strength_reduce_binary(binary(x, neg, BinaryOp::OP_BITAND));
    auto ret_8 = try_strength_reduce_binary(binary(x, zero, BinaryOp::OP_BITOR));
    auto ret_9 = try_strength_reduce_binary(binary(x, neg, BinaryOp::OP_BITOR));
    auto ret_10 = try_strength_reduce_binary(binary(x, zero, BinaryOp::OP_BITXOR));
    auto ret_11 = try_strength_reduce_binary(binary(x, neg, BinaryOp::OP_BITXOR));
    auto ret_12 = try_strength_reduce_binary(binary(x, zero, BinaryOp::OP_LSHIFT));
    auto ret_13 = try_strength_reduce_binary(binary(x, zero, BinaryOp::OP_RSHIFT));

    EXPECT_FALSE(ret_1.has_value());
    EXPECT_FALSE(ret_2.has_value());
    EXPECT_FALSE(ret_3.has_value());
    EXPECT_FALSE(ret_4.has_value());
    EXPECT_FALSE(ret_5.has_value());
    EXPECT_FALSE(ret_6.has_value());
    EXPECT_FALSE(ret_7.has_value());
    EXPECT_FALSE(ret_8.has_value());
    EXPECT_FALSE(ret_9.has_value());
    EXPECT_FALSE(ret_10.has_value());
    EXPECT_FALSE(ret_11.has_value());
    EXPECT_FALSE(ret_12.has_value());
    EXPECT_FALSE(ret_13.has_value());
}

TEST(OptimTest, PurityChecks)
{
    auto def = func_def(
        name_expr("def"),
        list_expr({ name_expr("x") }),
        blk({ return_stmt(binary(name_expr("x"), name_expr("x"), BinaryOp::OP_ADD)) }));
    Expr* call = call_expr(def->get_name());
    Expr* var = name_expr("x");
    Expr* assign = assign_expr(var, lit_int(0));

    EXPECT_TRUE(is_pure(lit_int(0)));
    EXPECT_TRUE(is_pure(var));
    EXPECT_TRUE(is_pure(binary(var, var, BinaryOp::OP_ADD)));
    EXPECT_TRUE(is_pure(unary(var, UnaryOp::OP_NEG)));
    EXPECT_TRUE(is_pure(list_expr({ var, lit_int(0) })));
    EXPECT_TRUE(is_pure(index_expr(list_expr(), var)));

    EXPECT_FALSE(is_pure(call));
    EXPECT_FALSE(is_pure(assign));
    EXPECT_FALSE(is_pure(binary(var, call, BinaryOp::OP_ADD)));
    EXPECT_FALSE(is_pure(unary(call, UnaryOp::OP_NEG)));
    EXPECT_FALSE(is_pure(list_expr({ assign, var, call })));
    EXPECT_FALSE(is_pure(index_expr(list_expr(), call)));
}
