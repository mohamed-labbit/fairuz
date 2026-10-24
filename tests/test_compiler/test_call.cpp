//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerCall, CallWithNoArgs)
{
    Chunk* chunk = compile_ok("دال()\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL f").op(OpCode::LOAD_GLOBAL);
    bc.next("IC_CALL").op(OpCode::IC_CALL).B(0);
    bc.next("RETURN").op(OpCode::RETURN).A(0).B(1);
    bc.done();
    EXPECT_EQ(chunk->ic_slots.size(), 1u);
}

TEST(CompilerCall, CallWithTwoArgs)
{
    Array<std::string> args { "1", "2" };
    Chunk* chunk = compile_ok((std::string((std::string("دال(") + test_source::join(args, ", ") + ")")) + "\n"));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL f").op(OpCode::LOAD_GLOBAL).A(0);
    bc.next("arg1").op(OpCode::LOAD_INT).A(1).Bx(load_int_bx(1));
    bc.next("arg2").op(OpCode::LOAD_INT).A(2).Bx(load_int_bx(2));
    bc.next("IC_CALL").op(OpCode::IC_CALL).A(0).B(2); // argc=2
    bc.next("RETURN").op(OpCode::RETURN).A(0).B(1);
    bc.done();
}

TEST(CompilerCall, CallResultUsed)
{
    Chunk* chunk = compile_ok("نتيجة := دال(س)\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    bool has_ic_call = false;
    for (auto& ins : chunk->code)
        if (instr_op(ins) == OpCode::IC_CALL) has_ic_call = true;
    EXPECT_TRUE(has_ic_call);
}

TEST(CompilerCall, ICSlotAllocatedPerCallSite)
{
    Chunk* chunk = compile_ok({ "دال()\n", "عام()\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_EQ(chunk->ic_slots.size(), 2u);
}
