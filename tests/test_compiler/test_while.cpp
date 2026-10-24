//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerWhile, BasicWhile)
{
    Chunk* chunk = compile_ok("طالما س:\n"
                              "    ا := 1\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    bool has_jif = false, has_loop = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::JUMP_IF_FALSE) has_jif = true;
        if (instr_op(ins) == OpCode::LOOP) has_loop = true;
    }
    EXPECT_TRUE(has_jif) << "while needs JUMP_IF_FALSE";
    EXPECT_TRUE(has_loop) << "while needs LOOP back-edge";

    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::LOOP) EXPECT_LT(instr_sBx(ins), 0) << "LOOP offset must be negative";
    }
}

TEST(CompilerWhile, JumpIfFalsePointsPastLoop)
{
    Chunk* chunk = compile_ok("طالما شرط:\n"
                              "    س := 0\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++)
        if (instr_op(chunk->code[i]) == OpCode::JUMP_IF_FALSE) jif_pos = i;
    ASSERT_GE(jif_pos, 0);
    int m_target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    ASSERT_LT(m_target, (int)chunk->code.size());
    EXPECT_EQ(instr_op(chunk->code[m_target]), OpCode::RETURN_NIL);
}
