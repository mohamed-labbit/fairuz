#include "../include/ast.hpp"
#include "../include/compiler.hpp"
#include "../include/parser.hpp"
#include "../include/vm.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <string>

using namespace fairuz;
using namespace fairuz::lex;
using namespace fairuz::parser;
using namespace fairuz::runtime;

namespace {

struct Fa_ExprSpec {
    enum class Kind {
        Lit,
        Neg,
        Add,
        Sub,
        Mul
    };

    Kind kind;
    i64 value { 0 };
    std::unique_ptr<Fa_ExprSpec> left;
    std::unique_ptr<Fa_ExprSpec> right;
};

Fa_ExprSpec make_lit(i64 v)
{
    Fa_ExprSpec e;
    e.kind = Fa_ExprSpec::Kind::Lit;
    e.value = v;
    return e;
}

Fa_ExprSpec make_unary(Fa_ExprSpec::Kind kind, Fa_ExprSpec child)
{
    Fa_ExprSpec e;
    e.kind = kind;
    e.left = std::make_unique<Fa_ExprSpec>(std::move(child));
    return e;
}

Fa_ExprSpec make_binary(Fa_ExprSpec::Kind kind, Fa_ExprSpec lhs, Fa_ExprSpec rhs)
{
    Fa_ExprSpec e;
    e.kind = kind;
    e.left = std::make_unique<Fa_ExprSpec>(std::move(lhs));
    e.right = std::make_unique<Fa_ExprSpec>(std::move(rhs));
    return e;
}

Fa_ExprSpec gen_Fa_Expr(std::mt19937_64& rng, int depth)
{
    std::uniform_int_distribution<int> lit_dist(-9, 9);
    if (depth <= 0)
        return make_lit(lit_dist(rng));

    std::uniform_int_distribution<int> node_dist(0, 4);
    switch (node_dist(rng)) {
    case 0:
        return make_lit(lit_dist(rng));
    case 1:
        return make_unary(Fa_ExprSpec::Kind::Neg, gen_Fa_Expr(rng, depth - 1));
    case 2:
        return make_binary(Fa_ExprSpec::Kind::Add, gen_Fa_Expr(rng, depth - 1), gen_Fa_Expr(rng, depth - 1));
    case 3:
        return make_binary(Fa_ExprSpec::Kind::Sub, gen_Fa_Expr(rng, depth - 1), gen_Fa_Expr(rng, depth - 1));
    default:
        return make_binary(Fa_ExprSpec::Kind::Mul, gen_Fa_Expr(rng, depth - 1), gen_Fa_Expr(rng, depth - 1));
    }
}

std::string to_source(Fa_ExprSpec const& expr)
{
    switch (expr.kind) {
    case Fa_ExprSpec::Kind::Lit:
        return std::to_string(expr.value);
    case Fa_ExprSpec::Kind::Neg:
        return "(-" + to_source(*expr.left) + ")";
    case Fa_ExprSpec::Kind::Add:
        return "(" + to_source(*expr.left) + " + " + to_source(*expr.right) + ")";
    case Fa_ExprSpec::Kind::Sub:
        return "(" + to_source(*expr.left) + " - " + to_source(*expr.right) + ")";
    case Fa_ExprSpec::Kind::Mul:
        return "(" + to_source(*expr.left) + " * " + to_source(*expr.right) + ")";
    }
    return "0";
}

i64 eval_host(Fa_ExprSpec const& expr)
{
    switch (expr.kind) {
    case Fa_ExprSpec::Kind::Lit:
        return expr.value;
    case Fa_ExprSpec::Kind::Neg:
        return -eval_host(*expr.left);
    case Fa_ExprSpec::Kind::Add:
        return eval_host(*expr.left) + eval_host(*expr.right);
    case Fa_ExprSpec::Kind::Sub:
        return eval_host(*expr.left) - eval_host(*expr.right);
    case Fa_ExprSpec::Kind::Mul:
        return eval_host(*expr.left) * eval_host(*expr.right);
    }
    return 0;
}

AST::Fa_Expr* build_ast(Fa_ExprSpec const& expr)
{
    switch (expr.kind) {
    case Fa_ExprSpec::Kind::Lit:
        return AST::Fa_makeLiteralInt(expr.value);
    case Fa_ExprSpec::Kind::Neg:
        return Fa_makeUnary(build_ast(*expr.left), AST::Fa_UnaryOp::OP_NEG);
    case Fa_ExprSpec::Kind::Add:
        return Fa_makeBinary(build_ast(*expr.left), build_ast(*expr.right), AST::Fa_BinaryOp::OP_ADD);
    case Fa_ExprSpec::Kind::Sub:
        return Fa_makeBinary(build_ast(*expr.left), build_ast(*expr.right), AST::Fa_BinaryOp::OP_SUB);
    case Fa_ExprSpec::Kind::Mul:
        return Fa_makeBinary(build_ast(*expr.left), build_ast(*expr.right), AST::Fa_BinaryOp::OP_MUL);
    }
    return AST::Fa_makeLiteralInt(0);
}

Fa_Value run_Fa_Expr_source(std::string const& source)
{
    Fa_FileManager fm;
    fm.buffer() = source.c_str();
    Fa_Parser parser(&fm);
    auto parsed = parser.parse();
    EXPECT_TRUE(parsed.hasValue()) << source;
    Fa_Chunk* chunk = Compiler().compile({ Fa_makeExprStmt(parsed.value()) });
    Fa_VM vm;
    return vm.run(chunk);
}

Fa_Value run_Fa_Expr_ast(AST::Fa_Expr* expr)
{
    Fa_Chunk* chunk = Compiler().compile({ Fa_makeExprStmt(expr) });
    Fa_VM vm;
    return vm.run(chunk);
}

bool sanitizer_stress_enabled()
{
    char const* v = std::getenv("fairuz_ENABLE_SANITIZER_STRESS");
    return v && std::string(v) == "1";
}

} // namespace

TEST(PropertyFa_Expr, RandomArithmeticMatchesHostAndAst)
{
    diagnostic::reset();
    std::mt19937_64 rng(0xC0FFEE);

    for (int i = 0; i < 250; ++i) {
        Fa_ExprSpec spec = gen_Fa_Expr(rng, 4);
        std::string source = to_source(spec);
        i64 expected = eval_host(spec);

        Fa_Value parsed_value = run_Fa_Expr_source(source);
        Fa_Value ast_value = run_Fa_Expr_ast(build_ast(spec));

        ASSERT_TRUE(Fa_IS_INTEGER(parsed_value)) << source;
        ASSERT_TRUE(Fa_IS_INTEGER(ast_value)) << source;
        EXPECT_EQ(Fa_AS_INTEGER(parsed_value), expected) << source;
        EXPECT_EQ(Fa_AS_INTEGER(ast_value), expected) << source;
        EXPECT_EQ(Fa_AS_INTEGER(parsed_value), Fa_AS_INTEGER(ast_value)) << source;
    }
}

TEST(SanitizerStress, RandomArithmeticCorpus)
{
    if (!sanitizer_stress_enabled())
        GTEST_SKIP() << "set fairuz_ENABLE_SANITIZER_STRESS=1 to enable";

    diagnostic::reset();
    std::mt19937_64 rng(0xBAD5EED);

    for (int i = 0; i < 2000; ++i) {
        Fa_ExprSpec spec = gen_Fa_Expr(rng, 5);
        std::string source = to_source(spec);
        Fa_Value parsed_value = run_Fa_Expr_source(source);
        ASSERT_TRUE(Fa_IS_INTEGER(parsed_value)) << source;
    }
}
