//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies deferred function compilation and retries.
//===----------------------------------------------------------------------===//

#include "Compiler/compiler.hpp"
#include "Interpreter/vm.hpp"
#include "Parser/parser.hpp"
#include "test_common.h"
#include "test_source_program.hpp"

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

    Chunk* compile(std::string const& text) { return test_source::compile(text); }
    Chunk* compile(std::initializer_list<std::string> text)
    {
        return compile(test_source::join(Array<std::string>(text)));
    }
};

TEST_F(LazyCompilation, UnusedBodiesStayEmptyWhileDefinitionsAreBound)
{
    auto fn = "دالة غير_مستخدم():\n"
              "    اخرج\n";
    Chunk* main = compile({ fn });
    ASSERT_EQ(main->functions.size(), 1u);
    Chunk* body = main->functions[0];
    ASSERT_TRUE(body->deferred_body);
    EXPECT_TRUE(body->code.empty());
    VM vm;
    run_success(vm.run(main));
    Chunk* lookup = compile("دالة ابحث():\n"
                            "    ارجع غير_مستخدم\n"
                            "ابحث()\n");
    auto binding = run_success(vm.run(lookup));
    EXPECT_TRUE(binding.is_function());
    EXPECT_TRUE(body->deferred_body);
    EXPECT_TRUE(body->code.empty());
    EXPECT_FALSE(diagnostic::has_errors());
}

TEST_F(LazyCompilation, FirstInlineCacheCallCompilesOnceAndBindsParameters)
{
    auto fn = "دالة اجمع(س, ص):\n"
              "    ارجع (س + ص)\n";
    Chunk* main = compile({ fn, "اجمع(20, 22)\n" });
    Chunk* body = main->functions[0];
    EXPECT_TRUE(body->code.empty());
    EXPECT_EQ(body->arity, 2);
    bool has_ic_call = false;
    for (u32 instruction : main->code)
        has_ic_call |= instr_op(instruction) == OpCode::IC_CALL;
    ASSERT_TRUE(has_ic_call);

    VM vm;
    EXPECT_EQ(run_success(vm.run(main)).as_int(), 42);
    EXPECT_FALSE(body->deferred_body);
    EXPECT_FALSE(body->code.empty());
    u32* code = body->code.data();
    EXPECT_EQ(run_success(vm.run(main)).as_int(), 42);
    EXPECT_EQ(body->code.data(), code);
}

TEST_F(LazyCompilation, FirstTailCallCompilesCalleeAndSupportsMutualRecursion)
{
    auto recursive = [](char const* name, char const* other) {
        return (std::string("دالة ") + name + "(ن):\n" + test_source::indent((std::string("اذا (ن = 0):\n"
                                                                                          "    ارجع 42\n")
                    + (std::string("ارجع") + (std::string(" ") + (std::string(other) + "((ن - 1))")) + "\n"))));
    };
    Chunk* main = compile({ recursive("يسار", "يمين"), recursive("يمين", "يسار"),
        "يسار(1000)\n" });
    VM vm;
    EXPECT_EQ(run_success(vm.run(main)).as_int(), 42);
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
        "دالة قيمة():\n"
        "    ارجع 10\n",
        "قديم := قيمة\n",
        "دالة قيمة():\n"
        "    ارجع 32\n",
        "طبيعي((قديم() + قيمة()))\n",
    });
    VM vm;
    EXPECT_EQ(run_success(vm.run(main)).as_int(), 42);
    for (Chunk* body : main->functions)
        EXPECT_FALSE(body->deferred_body);
}

TEST_F(LazyCompilation, NativeCallbackCompilesOnFirstInvocation)
{
    Chunk* main = compile({
        "دالة استجابة(س):\n"
        "    ارجع س\n",
        "__استدعاء__(استجابة, [42])\n",
    });
    VM vm;
    EXPECT_EQ(run_success(vm.run(main)).as_int(), 42);
    EXPECT_FALSE(main->functions[0]->deferred_body);
}

TEST_F(LazyCompilation, OnlyInvokedMethodsCompileIncludingConstructors)
{
    Chunk* main = compile("نوع علبة:\n"
                          "    دالة بداية(قيمة): .قيمة := قيمة\n"
                          "    دالة احصل(): ارجع هذا.قيمة\n"
                          "    دالة غير_مستخدم(): اخرج\n"
                          "العلبة := علبة(42)\n"
                          "العلبة.احصل()\n");
    ASSERT_EQ(main->functions.size(), 3u);
    for (Chunk* body : main->functions)
        EXPECT_TRUE(body->code.empty());
    VM vm;
    auto result_or_error = run_success(vm.run(main));
    Value result = result_or_error;
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
        "دالة معيب():\n"
        "    \"before error\"\n"
        "    اخرج\n",
        "معيب()\n",
    });
    VM vm;
    for (int attempt = 0; attempt < 2; ++attempt) {
        run_failed(vm.run(main));
        EXPECT_TRUE(main->functions[0]->deferred_body);
        EXPECT_TRUE(main->functions[0]->code.empty());
        EXPECT_TRUE(main->functions[0]->constants.empty());
    }
}

TEST_F(LazyCompilation, DeferredErrorsRetainSourceAfterParserDestruction)
{
    Chunk* main = compile("دالة خطأ():\n"
                          "    اخرج\n"
                          "خطأ()\n");
    VM vm;
    auto error = run_failed(vm.run(main));
    ASSERT_NE(error.source, nullptr);
    EXPECT_NE(error.source->line(2).find("اخرج"), std::string::npos);
    EXPECT_EQ(error.loc.line, 2u);
    EXPECT_FALSE(diagnostic::has_errors());
}

TEST_F(LazyCompilation, ExplicitValidationCompilesUnusedBodiesWithoutRunningThem)
{
    Chunk* main = compile({
        "دالة غير_مستخدم():\n"
        "    مفقود()\n"
        "    ارجع 42\n",
    });
    EXPECT_FALSE(Compiler::compile_all(main).has_error());
    EXPECT_FALSE(main->functions[0]->deferred_body);
    EXPECT_FALSE(main->functions[0]->code.empty());
    EXPECT_FALSE(diagnostic::has_errors());
}

} // namespace
