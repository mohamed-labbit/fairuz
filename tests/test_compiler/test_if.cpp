//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerIf, SimpleIfNoElse)
{
    Chunk* chunk = compile_ok("اذا س:\n"
                              "    ص := 1\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP_IF_FALSE) jif_pos = i;
    }
    ASSERT_GE(jif_pos, 0) << "expected JUMP_IF_FALSE";
    int m_target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    EXPECT_EQ(instr_op(chunk->code[m_target]), OpCode::RETURN_NIL);
}

TEST(CompilerIf, IfElse)
{
    Chunk* chunk = compile_ok("اذا س:\n"
                              "    ا := 1\n"
                              "غيره:\n"
                              "    ب := 2\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    bool has_jif = false, has_jmp = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::JUMP_IF_FALSE) has_jif = true;
        if (instr_op(ins) == OpCode::JUMP) has_jmp = true;
    }
    EXPECT_TRUE(has_jif) << "expected JUMP_IF_FALSE";
    EXPECT_TRUE(has_jmp) << "expected JUMP over else";
    int jif_pos = -1, jmp_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP_IF_FALSE) jif_pos = i;
        else if (instr_op(chunk->code[i]) == OpCode::JUMP) jmp_pos = i;
    }
    ASSERT_GE(jif_pos, 0);
    ASSERT_GE(jmp_pos, 0);
    int jif_target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    EXPECT_EQ(jif_target, jmp_pos + 1);
}
