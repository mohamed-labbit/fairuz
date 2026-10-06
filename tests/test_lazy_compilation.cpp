#include "../fairuz/fcompiler.hpp"
#include "../fairuz/fparser.hpp"
#include "../fairuz/fvm.hpp"
#include "test_common.h"

#include <gtest/gtest.h>

namespace {

class LazyCompilation : public ::testing::Test {
protected:
    void SetUp() override
    {
        diagnostic::reset();
        diagnostic::set_source(nullptr);
    }

    void TearDown() override
    {
        diagnostic::reset();
        diagnostic::set_source(nullptr);
    }

    Chunk* compile(Array<StmtPtr> const& statements)
    {
        // Intentionally destroy the original compiler before execution.
        Chunk* chunk = Compiler().compile(statements);
        EXPECT_FALSE(diagnostic::has_errors());
        return chunk;
    }
};

TEST_F(LazyCompilation, UnusedBodiesStayEmptyWhileDefinitionsAreBound)
{
    auto* fn = func_def(ident("unused"), { }, blk({ break_stmt() }));
    Chunk* main = compile({ fn });
    ASSERT_EQ(main->functions.size(), 1u);
    Chunk* body = main->functions[0];
    ASSERT_TRUE(body->deferred_body);
    EXPECT_TRUE(body->code.empty());
    VM vm;
    EXPECT_NO_THROW(vm.run(main));
    ASSERT_NE(vm.m_root_environment.find("unused"), nullptr);
    EXPECT_TRUE(vm.m_root_environment.find("unused")->is_function());
    EXPECT_TRUE(body->deferred_body);
    EXPECT_TRUE(body->code.empty());
    EXPECT_FALSE(diagnostic::has_errors());
}

TEST_F(LazyCompilation, FirstInlineCacheCallCompilesOnceAndBindsParameters)
{
    auto* fn = func_def(ident("add"), { ident("x"), ident("y") },
        blk({ return_stmt(binary(ident("x"), ident("y"), Expr::Kind::OP_ADD)) }));
    Chunk* main = compile({ fn, expr_stmt(call_expr(ident("add"), { lit_int(20), lit_int(22) })) });
    Chunk* body = main->functions[0];
    EXPECT_TRUE(body->code.empty());
    EXPECT_EQ(body->arity, 2);
    bool has_ic_call = false;
    for (u32 instruction : main->code)
        has_ic_call |= instr_op(instruction) == OpCode::IC_CALL;
    ASSERT_TRUE(has_ic_call);

    VM vm;
    EXPECT_EQ(vm.run(main).as_int(), 42);
    EXPECT_FALSE(body->deferred_body);
    EXPECT_FALSE(body->code.empty());
    u32* code = body->code.data();
    EXPECT_EQ(vm.run(main).as_int(), 42);
    EXPECT_EQ(body->code.data(), code);
}

TEST_F(LazyCompilation, FirstTailCallCompilesCalleeAndSupportsMutualRecursion)
{
    auto recursive = [](char const* name, char const* other) {
        return func_def(ident(name), { ident("n") }, blk({
                                                         if_stmt(binary(ident("n"), lit_int(0), Expr::Kind::OP_EQ), return_stmt(lit_int(42))),
                                                         return_stmt(call_expr(ident(other), {
                                                                                                 binary(ident("n"), lit_int(1), Expr::Kind::OP_SUB),
                                                                                             })),
                                                     }));
    };
    Chunk* main = compile({ recursive("left", "right"), recursive("right", "left"),
        expr_stmt(call_expr(ident("left"), { lit_int(1000) })) });
    VM vm;
    EXPECT_EQ(vm.run(main).as_int(), 42);
    for (Chunk* body : main->functions) {
        EXPECT_FALSE(body->deferred_body);
        bool has_tail_call = false;
        for (u32 instruction : body->code)
            has_tail_call |= instr_op(instruction) == OpCode::CALL_TAIL;
        EXPECT_TRUE(has_tail_call);
    }
}

TEST_F(LazyCompilation, AliasesKeepDistinctBodiesWithTheSameName)
{
    Chunk* main = compile({
        func_def(ident("value"), { }, return_stmt(lit_int(10))),
        decl_stmt("old", ident("value")),
        func_def(ident("value"), { }, return_stmt(lit_int(32))),
        expr_stmt(call_expr(ident("طبيعي"), {
                                                binary(call_expr(ident("old")), call_expr(ident("value")), Expr::Kind::OP_ADD),
                                            })),
    });
    VM vm;
    EXPECT_EQ(vm.run(main).as_int(), 42);
    for (Chunk* body : main->functions)
        EXPECT_FALSE(body->deferred_body);
}

TEST_F(LazyCompilation, NativeCallbackCompilesOnFirstInvocation)
{
    Chunk* main = compile({
        func_def(ident("callback"), { ident("x") }, return_stmt(ident("x"))),
        expr_stmt(call_expr(ident("__استدعاء__"), { ident("callback"), list_expr({ lit_int(42) }) })),
    });
    VM vm;
    EXPECT_EQ(vm.run(main).as_int(), 42);
    EXPECT_FALSE(main->functions[0]->deferred_body);
}

TEST_F(LazyCompilation, OnlyInvokedMethodsCompileIncludingConstructors)
{
    Chunk* main = compile({
        class_def(ident("Box"), { ident("value") }, {
                                                        func_def(ident("بداية"), { ident("value") }, blk({
                                                                                                         expr_stmt(assign_expr(ident("value"), ident("value"))),
                                                                                                     })),
                                                        func_def(ident("get"), { }, return_stmt(ident("value"))),
                                                        func_def(ident("unused"), { }, blk({ break_stmt() })),
                                                    }),
        decl_stmt("box", call_expr(ident("Box"), { lit_int(42) })),
        expr_stmt(call_expr(get_expr(ident("box"), ident("get")))),
    });
    ASSERT_EQ(main->functions.size(), 3u);
    for (Chunk* body : main->functions)
        EXPECT_TRUE(body->code.empty());
    VM vm;
    Value result = vm.run(main);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 42);
    EXPECT_FALSE(main->functions[0]->deferred_body);
    EXPECT_FALSE(main->functions[1]->deferred_body);
    EXPECT_TRUE(main->functions[2]->deferred_body);
    EXPECT_TRUE(main->functions[2]->code.empty());
}

TEST_F(LazyCompilation, FailedCompilationLeavesStubIntactForRetry)
{
    Chunk* main = compile({
        func_def(ident("bad"), { }, blk({ expr_stmt(lit_str("before error")), break_stmt() })),
        call_stmt("bad"),
    });
    VM vm;
    for (int attempt = 0; attempt < 2; ++attempt) {
        diagnostic::reset();
        EXPECT_THROW(vm.run(main), RuntimeHalt);
        EXPECT_TRUE(diagnostic::has_errors());
        EXPECT_TRUE(main->functions[0]->deferred_body);
        EXPECT_TRUE(main->functions[0]->code.empty());
        EXPECT_TRUE(main->functions[0]->constants.empty());
    }
}

TEST_F(LazyCompilation, DeferredErrorsRetainSourceAfterParserDestruction)
{
    Chunk* main;
    {
        lex::FileManager source;
        source.buffer() = "دالة خطأ():\n    اخرج\nخطأ()\n";
        diagnostic::SourceScope source_scope(&source);
        parser::Parser parser(&source);
        main = compile(parser.parse_program());
    }
    VM vm;
    EXPECT_THROW(vm.run(main), RuntimeHalt);
    auto json = diagnostic::engine.to_json();
    EXPECT_NE(json.find("اخرج"), std::string::npos);
    EXPECT_NE(json.find("\"line\":2"), std::string::npos);
}

TEST_F(LazyCompilation, ExplicitValidationCompilesUnusedBodiesWithoutRunningThem)
{
    Chunk* main = compile({
        func_def(ident("unused"), { }, blk({ call_stmt("missing"), return_stmt(lit_int(42)) })),
    });
    EXPECT_TRUE(Compiler::compile_all(main));
    EXPECT_FALSE(main->functions[0]->deferred_body);
    EXPECT_FALSE(main->functions[0]->code.empty());
    EXPECT_FALSE(diagnostic::has_errors());
}

} // namespace
