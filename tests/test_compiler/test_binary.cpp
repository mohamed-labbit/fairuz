//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerBinary, AddTwoLocals)
{
    Chunk* ch = compile_ok_local({ "ا := 1\n", "ب := 2\n", "(ا + ب)\n" });
    BytecodeChecker bc(*ch);
    bc.next("decl_stmt a").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.next("decl_stmt b").op(OpCode::LOAD_INT).A(1).Bx(load_int_bx(2));
    bc.next("OP_ADD").op(OpCode::OP_ADD).A(2).B(0).C(1);
    bc.next("NOP ic").op(OpCode::NOP).A(0); // IC slot 0
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(ch->ic_slots.size(), 1u);
}

TEST(CompilerBinary, ICSlotAllocatedPerBinaryOp)
{
    Chunk* chunk = compile_ok({ "س := 1\n", "ص := 2\n", "(س + ص)\n", "(س * ص)\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_EQ(chunk->ic_slots.size(), 2u);
}

TEST(CompilerBinary, LogicalAndShortCircuit)
{
    Chunk* ch = compile_ok_local({
        "ا := صحيح\n",
        "ب := خطا\n",
        "(ا و ب)\n",
    });

    BytecodeChecker bc(*ch);
    bc.next("decl_stmt a").op(OpCode::LOAD_TRUE).A(0);
    bc.next("decl_stmt b").op(OpCode::LOAD_FALSE).A(1);
    u32 jif_idx;
    bc.next("LHS into temp").op(OpCode::MOVE).A(2).B(0);
    (void)jif_idx;
    bool found_jif = false, found_b = false;
    for (auto& instr : ch->code) {
        if (instr_op(instr) == OpCode::JUMP_IF_FALSE) found_jif = true;
        if (found_jif && (instr_op(instr) == OpCode::LOAD_FALSE || instr_op(instr) == OpCode::MOVE)) found_b = true;
    }
    EXPECT_TRUE(found_jif) << "expected JUMP_IF_FALSE for && short-circuit";
    EXPECT_TRUE(found_b) << "expected RHS load after JUMP_IF_FALSE";
}

TEST(CompilerBinary, LogicalOrShortCircuit)
{
    Chunk* chunk = compile_ok_local({
        "ا := خطا\n",
        "ب := صحيح\n",
        "(ا او ب)\n",
    });

    bool found_jit = false;
    for (auto& instr : chunk->code) {
        if (instr_op(instr) == OpCode::JUMP_IF_TRUE) found_jit = true;
    }
    EXPECT_TRUE(found_jit) << "expected JUMP_IF_TRUE for || short-circuit";
}
