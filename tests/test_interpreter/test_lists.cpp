//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMLists, NewEmpty)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LIST_NEW, 0, 0, 0).ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 0);
}

TEST(VMLists, AppendAndLen)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LIST_NEW, 0, 3, 0);
    for (int v : { 10, 20, 30 }) b.load_int(1, v).ABC(OpCode::LIST_APPEND, 0, 1, 0);
    b.ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 3);
}

TEST(VMLists, GetFirst)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .ABC(OpCode::LIST_NEW, 0, 2, 0)
        .load_int(1, 77)
        .ABC(OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 88)
        .ABC(OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 0)
        .ABC(OpCode::LIST_GET, 2, 0, 1)
        .ret(2);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 77);
}

TEST(VMLists, GetLast)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .ABC(OpCode::LIST_NEW, 0, 2, 0)
        .load_int(1, 10)
        .ABC(OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 20)
        .ABC(OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 1)
        .ABC(OpCode::LIST_GET, 2, 0, 1)
        .ret(2);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 20);
}

TEST(VMLists, Set)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .ABC(OpCode::LIST_NEW, 0, 1, 0)
        .load_int(1, 0)
        .ABC(OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 0)
        .load_int(2, 99)
        .ABC(OpCode::LIST_SET, 0, 1, 2)
        .load_int(1, 0)
        .ABC(OpCode::LIST_GET, 2, 0, 1)
        .ret(2);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 99);
}

TEST(VMLists, OutOfBoundsThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run("طبيعي([][0])\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMLists, LenOnNonListThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run("لكل عنصر في 5:\n"
                                                   "    اكتب(عنصر)\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}
