//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerLoop, BreakPatchesToLoopExit)
{
    Chunk* chunk = compile_ok("طالما شرط:\n"
                              "    اخرج\n"
                              "    س := 1\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    int jump_pos = -1;
    int loop_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP && jump_pos < 0) jump_pos = i;
        if (instr_op(chunk->code[i]) == OpCode::LOOP) loop_pos = i;
    }

    ASSERT_GE(jump_pos, 0);
    ASSERT_GE(loop_pos, 0);
    EXPECT_GT(jump_pos + 1 + instr_sBx(chunk->code[jump_pos]), loop_pos);
}

TEST(CompilerLoop, ContinuePatchesToLoopLatch)
{
    Chunk* chunk = compile_ok("طالما شرط:\n"
                              "    اكمل\n"
                              "    س := 1\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    int jump_pos = -1;
    int loop_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP && jump_pos < 0) jump_pos = i;
        if (instr_op(chunk->code[i]) == OpCode::LOOP) loop_pos = i;
    }

    ASSERT_GE(jump_pos, 0);
    ASSERT_GE(loop_pos, 0);
    EXPECT_EQ(jump_pos + 1 + instr_sBx(chunk->code[jump_pos]), loop_pos);
}

TEST(CompilerLoop, BreakOutsideLoopIsRejected)
{
    Chunk* chunk = compile_fail("اخرج\n");
    ASSERT_EQ(chunk, nullptr);
}

TEST(CompilerLoop, ContinueOutsideLoopIsRejected)
{
    Chunk* chunk = compile_fail("اكمل\n");
    ASSERT_EQ(chunk, nullptr);
}
