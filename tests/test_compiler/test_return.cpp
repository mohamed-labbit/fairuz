//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerReturn, ReturnNilEmitsReturnNil)
{
    Chunk* chunk = compile_ok("ارجع عدم\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerReturn, ReturnValueEmitsReturn)
{
    Chunk* chunk = compile_ok("ارجع 42\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 42").op(OpCode::LOAD_INT).Bx(load_int_bx(42));
    bc.next("RETURN").op(OpCode::RETURN).B(1);
    bc.done();
}

TEST(CompilerReturn, ReturnIsDeadCodeBarrier)
{
    Chunk* chunk = compile_ok({ "ارجع 1\n", "س := 99\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    for (auto& v : chunk->constants) EXPECT_NE(v.as_int(), 99) << "dead code leaked into constant pool";
    bool found_99 = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::LOAD_INT && instr_Bx(ins) == load_int_bx(99)) found_99 = true;
    }
    EXPECT_FALSE(found_99) << "dead code (LOAD_INT 99) was emitted after return";
}

TEST(CompilerReturn, TailCallEmitsCallTail)
{
    Chunk* chunk = compile_ok("دالة غلاف():\n"
                              "    ارجع دال()\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    Chunk const* fn = chunk->functions[0];
    bool has_tail = false;
    for (auto& ins : fn->code) {
        if (instr_op(ins) == OpCode::CALL_TAIL) has_tail = true;
    }
    EXPECT_TRUE(has_tail) << "return f() in function should emit CALL_TAIL";
}
