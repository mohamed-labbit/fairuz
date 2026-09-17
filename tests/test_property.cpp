#include "../fairuz/fAST.hpp"
#include "../fairuz/fcompiler.hpp"
#include "../fairuz/fparser.hpp"
#include "../fairuz/fvm.hpp"
#include "test_common.h"

#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <string>

using namespace fairuz;
using namespace fairuz::lex;
using namespace fairuz::parser;
using namespace fairuz::runtime;

namespace {

struct ExprSpec {
    enum class Kind {
        Lit,
        Neg,
        Add,
        Sub,
        Mul
    };

    Kind kind;
    i64 m_value { 0 };
    std::unique_ptr<ExprSpec> m_left;
    std::unique_ptr<ExprSpec> m_right;
};

ExprSpec make_lit(i64 v)
{
    ExprSpec e;
    e.kind = ExprSpec::Kind::Lit;
    e.m_value = v;
    return e;
}

ExprSpec make_binary(ExprSpec::Kind kind, ExprSpec lhs, ExprSpec rhs)
{
    ExprSpec e;
    e.kind = kind;
    e.m_left = std::make_unique<ExprSpec>(std::move(lhs));
    e.m_right = std::make_unique<ExprSpec>(std::move(rhs));
    return e;
}

ExprSpec gen_fa_expr(std::mt19937_64& rng, int depth)
{
    std::uniform_int_distribution<int> lit_dist(0, 9);
    if (depth <= 0)
        return make_lit(lit_dist(rng));

    std::uniform_int_distribution<int> node_dist(0, 3);
    switch (node_dist(rng)) {
    case 0:
        return make_lit(lit_dist(rng));
    case 1:
        return make_binary(ExprSpec::Kind::Add, gen_fa_expr(rng, depth - 1), gen_fa_expr(rng, depth - 1));
    case 2:
        return make_binary(ExprSpec::Kind::Sub, gen_fa_expr(rng, depth - 1), gen_fa_expr(rng, depth - 1));
    default:
        return make_binary(ExprSpec::Kind::Mul, gen_fa_expr(rng, depth - 1), gen_fa_expr(rng, depth - 1));
    }
}

std::string to_source(ExprSpec const& m_expr)
{
    switch (m_expr.kind) {
    case ExprSpec::Kind::Lit:
        if (m_expr.m_value < 0)
            return "(0 - " + std::to_string(-m_expr.m_value) + ")";
        return std::to_string(m_expr.m_value);
    case ExprSpec::Kind::Neg:
        return "(-" + to_source(*m_expr.m_left) + ")";
    case ExprSpec::Kind::Add:
        return "(" + to_source(*m_expr.m_left) + " + " + to_source(*m_expr.m_right) + ")";
    case ExprSpec::Kind::Sub:
        return "(" + to_source(*m_expr.m_left) + " - " + to_source(*m_expr.m_right) + ")";
    case ExprSpec::Kind::Mul:
        return "(" + to_source(*m_expr.m_left) + " * " + to_source(*m_expr.m_right) + ")";
    }
    return "0";
}

i64 eval_host(ExprSpec const& m_expr)
{
    switch (m_expr.kind) {
    case ExprSpec::Kind::Lit:
        return m_expr.m_value;
    case ExprSpec::Kind::Neg:
        return -eval_host(*m_expr.m_left);
    case ExprSpec::Kind::Add:
        return eval_host(*m_expr.m_left) + eval_host(*m_expr.m_right);
    case ExprSpec::Kind::Sub:
        return eval_host(*m_expr.m_left) - eval_host(*m_expr.m_right);
    case ExprSpec::Kind::Mul:
        return eval_host(*m_expr.m_left) * eval_host(*m_expr.m_right);
    }
    return 0;
}

AST::Expr* build_ast(ExprSpec const& m_expr)
{
    switch (m_expr.kind) {
    case ExprSpec::Kind::Lit:
        return lit_int(m_expr.m_value);
    case ExprSpec::Kind::Neg:
        return unary(build_ast(*m_expr.m_left), AST::UnaryOp::OP_NEG);
    case ExprSpec::Kind::Add:
        return binary(build_ast(*m_expr.m_left), build_ast(*m_expr.m_right), AST::BinaryOp::OP_ADD);
    case ExprSpec::Kind::Sub:
        return binary(build_ast(*m_expr.m_left), build_ast(*m_expr.m_right), AST::BinaryOp::OP_SUB);
    case ExprSpec::Kind::Mul:
        return binary(build_ast(*m_expr.m_left), build_ast(*m_expr.m_right), AST::BinaryOp::OP_MUL);
    }
    return lit_int(0);
}

Value run_fa_expr_source(std::string const& source)
{
    FileManager fm;
    fm.buffer() = source.c_str();
    Parser parser(&fm);
    auto parsed = parser.parse();
    EXPECT_TRUE(parsed.has_value()) << source;
    Chunk* chunk = Compiler().compile({ expr_stmt(call_expr(name_expr("طبيعي"), list_expr({ parsed.value() }))) });
    VM vm;
    return vm.run(chunk);
}

Value run_fa_expr_ast(AST::Expr* m_expr)
{
    Chunk* chunk = Compiler().compile({ expr_stmt(call_expr(name_expr("طبيعي"), list_expr({ m_expr }))) });
    VM vm;
    return vm.run(chunk);
}

} // namespace

TEST(PropertyExpr, RandomArithmeticMatchesHostAndAst)
{
    diagnostic::reset();
    std::mt19937_64 rng(0xC0FFEE);

    for (int i = 0; i < 250; i++) {
        ExprSpec spec = gen_fa_expr(rng, 4);
        std::string source = to_source(spec);
        i64 expected = eval_host(spec);

        Value parsed_value = run_fa_expr_source(source);
        Value ast_value = run_fa_expr_ast(build_ast(spec));

        ASSERT_TRUE(parsed_value.is_int()) << source;
        ASSERT_TRUE(ast_value.is_int()) << source;
        EXPECT_EQ(parsed_value.as_int(), expected) << source;
        EXPECT_EQ(ast_value.as_int(), expected) << source;
        EXPECT_EQ(parsed_value.as_int(), ast_value.as_int()) << source;
    }
}
