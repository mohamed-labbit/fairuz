//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMGlobals, StoreAndLoad)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 123).stg(0, "عام").load_int(0, 0).ldg(1, "عام").ret(1);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 123);
}

TEST(VMGlobals, MissingGlobalReturnsError)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run("غير_موجود\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMGlobals, Overwrite)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1).stg(0, "س").load_int(0, 2).stg(0, "س").ldg(1, "س").ret(1);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 2);
}

TEST(VMGlobals, UndefinedGlobalRaisesRuntimeError)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run("مفقود\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}
