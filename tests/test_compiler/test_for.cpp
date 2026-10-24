//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerFor, ListIterationLowersToLoopBytecode)
{
    Chunk* chunk = compile_ok({ "عناصر := [1, 2, 3]\n", "مجموع := 0\n",
        "لكل عنصر في عناصر:\n"
        "    مجموع := (مجموع + عنصر)\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    bool has_list_len = false;
    bool has_list_get = false;
    bool has_jif = false;
    bool has_loop = false;
    bool has_add = false;

    for (auto ins : chunk->code) {
        switch (instr_op(ins)) {
        case OpCode::LIST_LEN: has_list_len = true; break;
        case OpCode::LIST_GET: has_list_get = true; break;
        case OpCode::JUMP_IF_FALSE: has_jif = true; break;
        case OpCode::LOOP:
            has_loop = true;
            EXPECT_LT(instr_sBx(ins), 0) << "for loop back-edge must be negative";
            break;
        case OpCode::OP_ADD: has_add = true; break;
        default: break;
        }
    }

    EXPECT_TRUE(has_list_len);
    EXPECT_TRUE(has_list_get);
    EXPECT_TRUE(has_jif);
    EXPECT_TRUE(has_loop);
    EXPECT_TRUE(has_add);
}
