//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerMeta, TopLevelChunkNameIsMain)
{
    Chunk* chunk = compile_ok("");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_EQ(chunk->name, MAIN_PROGRAM);
}

TEST(CompilerMeta, FunctionAritySetCorrectly)
{
    Chunk* chunk = compile_ok("دالة دال(س, ص, ع):\n"
                              "    عدم\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->arity, 3);
}

TEST(CompilerMeta, LineInfoPresent)
{
    Chunk* chunk = compile_ok("س := 42\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_FALSE(chunk->lines.empty());
}
