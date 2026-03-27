#include "../include/ast.hpp"
#include "../include/compiler.hpp"
#include "../include/parser.hpp"
#include "../include/vm.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <random>
#include <string>

using namespace mylang;
using namespace mylang::ast;
using namespace mylang::lex;
using namespace mylang::parser;
using namespace mylang::runtime;

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
    int64_t value { 0 };
    std::unique_ptr<ExprSpec> left;
    std::unique_ptr<ExprSpec> right;
};

ExprSpec make_lit(int64_t v)
{
    ExprSpec e;
    e.kind = ExprSpec::Kind::Lit;
    e.value = v;
    return e;
}

ExprSpec make_unary(ExprSpec::Kind kind, ExprSpec child)
{
    ExprSpec e;
    e.kind = kind;
    e.left = std::make_unique<ExprSpec>(std::move(child));
    return e;
}

ExprSpec make_binary(ExprSpec::Kind kind, ExprSpec lhs, ExprSpec rhs)
{
    ExprSpec e;
    e.kind = kind;
    e.left = std::make_unique<ExprSpec>(std::move(lhs));
    e.right = std::make_unique<ExprSpec>(std::move(rhs));
    return e;
}

ExprSpec gen_expr(std::mt19937_64& rng, int depth)
{
    std::uniform_int_distribution<int> lit_dist(-9, 9);
    if (depth <= 0)
        return make_lit(lit_dist(rng));

    std::uniform_int_distribution<int> node_dist(0, 4);
    switch (node_dist(rng)) {
    case 0:
        return make_lit(lit_dist(rng));
    case 1:
        return make_unary(ExprSpec::Kind::Neg, gen_expr(rng, depth - 1));
    case 2:
        return make_binary(ExprSpec::Kind::Add, gen_expr(rng, depth - 1), gen_expr(rng, depth - 1));
    case 3:
        return make_binary(ExprSpec::Kind::Sub, gen_expr(rng, depth - 1), gen_expr(rng, depth - 1));
    default:
        return make_binary(ExprSpec::Kind::Mul, gen_expr(rng, depth - 1), gen_expr(rng, depth - 1));
    }
}

std::string to_source(ExprSpec const& expr)
{
    switch (expr.kind) {
    case ExprSpec::Kind::Lit:
        return std::to_string(expr.value);
    case ExprSpec::Kind::Neg:
        return "(-" + to_source(*expr.left) + ")";
    case ExprSpec::Kind::Add:
        return "(" + to_source(*expr.left) + " + " + to_source(*expr.right) + ")";
    case ExprSpec::Kind::Sub:
        return "(" + to_source(*expr.left) + " - " + to_source(*expr.right) + ")";
    case ExprSpec::Kind::Mul:
        return "(" + to_source(*expr.left) + " * " + to_source(*expr.right) + ")";
    }
    return "0";
}

int64_t eval_host(ExprSpec const& expr)
{
    switch (expr.kind) {
    case ExprSpec::Kind::Lit:
        return expr.value;
    case ExprSpec::Kind::Neg:
        return -eval_host(*expr.left);
    case ExprSpec::Kind::Add:
        return eval_host(*expr.left) + eval_host(*expr.right);
    case ExprSpec::Kind::Sub:
        return eval_host(*expr.left) - eval_host(*expr.right);
    case ExprSpec::Kind::Mul:
        return eval_host(*expr.left) * eval_host(*expr.right);
    }
    return 0;
}

Expr* build_ast(ExprSpec const& expr)
{
    switch (expr.kind) {
    case ExprSpec::Kind::Lit:
        return makeLiteralInt(expr.value);
    case ExprSpec::Kind::Neg:
        return makeUnary(build_ast(*expr.left), UnaryOp::OP_NEG);
    case ExprSpec::Kind::Add:
        return makeBinary(build_ast(*expr.left), build_ast(*expr.right), BinaryOp::OP_ADD);
    case ExprSpec::Kind::Sub:
        return makeBinary(build_ast(*expr.left), build_ast(*expr.right), BinaryOp::OP_SUB);
    case ExprSpec::Kind::Mul:
        return makeBinary(build_ast(*expr.left), build_ast(*expr.right), BinaryOp::OP_MUL);
    }
    return makeLiteralInt(0);
}

Value run_expr_source(std::string const& source)
{
    FileManager fm((std::filesystem::temp_directory_path() / "mylang_property_expr.txt").string());
    fm.buffer() = source.c_str();
    Parser parser(&fm);
    auto parsed = parser.parse();
    EXPECT_TRUE(parsed.hasValue()) << source;
    Chunk* chunk = Compiler().compile({ makeExprStmt(parsed.value()) });
    VM vm;
    return vm.run(chunk);
}

Value run_expr_ast(Expr* expr)
{
    Chunk* chunk = Compiler().compile({ makeExprStmt(expr) });
    VM vm;
    return vm.run(chunk);
}

bool sanitizer_stress_enabled()
{
    char const* v = std::getenv("MYLANG_ENABLE_SANITIZER_STRESS");
    return v && std::string(v) == "1";
}

} // namespace

TEST(PropertyExpr, RandomArithmeticMatchesHostAndAst)
{
    diagnostic::reset();
    std::mt19937_64 rng(0xC0FFEE);

    for (int i = 0; i < 250; ++i) {
        ExprSpec spec = gen_expr(rng, 4);
        std::string source = to_source(spec);
        int64_t expected = eval_host(spec);

        Value parsed_value = run_expr_source(source);
        Value ast_value = run_expr_ast(build_ast(spec));

        ASSERT_TRUE(IS_INTEGER(parsed_value)) << source;
        ASSERT_TRUE(IS_INTEGER(ast_value)) << source;
        EXPECT_EQ(AS_INTEGER(parsed_value), expected) << source;
        EXPECT_EQ(AS_INTEGER(ast_value), expected) << source;
        EXPECT_EQ(AS_INTEGER(parsed_value), AS_INTEGER(ast_value)) << source;
    }
}

TEST(SanitizerStress, RandomArithmeticCorpus)
{
    if (!sanitizer_stress_enabled())
        GTEST_SKIP() << "set MYLANG_ENABLE_SANITIZER_STRESS=1 to enable";

    diagnostic::reset();
    std::mt19937_64 rng(0xBAD5EED);

    for (int i = 0; i < 2000; ++i) {
        ExprSpec spec = gen_expr(rng, 5);
        std::string source = to_source(spec);
        Value parsed_value = run_expr_source(source);
        ASSERT_TRUE(IS_INTEGER(parsed_value)) << source;
    }
}
