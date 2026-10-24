//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerDict, LiteralLowersToBuiltinConstructorCall)
{
    Chunk* chunk = compile_ok("{\"a\": 1, \"b\": 2}\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL قاموس").op(OpCode::LOAD_GLOBAL).A(1).Bx(0);
    bc.next("LOAD_CONST key a").op(OpCode::LOAD_CONST).A(2).Bx(1);
    bc.next("LOAD_INT value 1").op(OpCode::LOAD_INT).A(3).Bx(load_int_bx(1));
    bc.next("LOAD_CONST key b").op(OpCode::LOAD_CONST).A(4).Bx(2);
    bc.next("LOAD_INT value 2").op(OpCode::LOAD_INT).A(5).Bx(load_int_bx(2));
    bc.next("IC_CALL").op(OpCode::IC_CALL).A(1).B(4);
    bc.next("MOVE result").op(OpCode::MOVE).A(0).B(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->constants.size(), 3u);
    EXPECT_TRUE(chunk->constants[0].is_string());
    EXPECT_EQ(chunk->constants[0].as_string()->str, "قاموس");
    EXPECT_TRUE(chunk->constants[1].is_string());
    EXPECT_EQ(chunk->constants[1].as_string()->str, "a");
    EXPECT_TRUE(chunk->constants[2].is_string());
    EXPECT_EQ(chunk->constants[2].as_string()->str, "b");
}

// Regression test for a register-allocation bug in compile_dict_impl.
//
// The per-pair loop in compile_dict_impl allocates a register for the key,
// compiles+discharges the key expression into it, then does the same for
// the value -- but, unlike every other place in the compiler that builds a
// contiguous argument block (e.g. the plain-call and member-call argument
// loops in compile_call_impl, which each follow a discharge with
// `m_current->free_regs_to(arg_reg + 1)`), it never reclaims the scratch
// registers used to *compute* that key or value.
//
// That reclaim matters because compile_list_impl/compile_dict_impl each
// leave their own destination register permanently reserved on return
// (their final `free_regs_to(dst + 1)` keeps `dst` allocated), which is one
// register more than the register-stack pointer sat at just before that
// nested expression started compiling. A nested list/dict literal (or a
// call -- see compile_call_impl's own `free_regs_to(fn_reg + 1)`) therefore
// leaks exactly one register past the point the caller expects. Because
// compile_dict_impl never frees back down after each key/value, that leak
// permanently shifts every later entry in the same dict literal one
// register further away from where the `قاموس` native constructor's
// IC_CALL will actually read it from -- corrupting the contiguous
// register block the call relies on for every pair after the leaky one.
TEST(CompilerDict, NestedListValueKeepsLaterEntriesContiguous)
{
    // {"a": [1], "b": 2}
    Chunk* chunk = compile_ok("{\"a\": [1], \"b\": 2}\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL قاموس").op(OpCode::LOAD_GLOBAL).A(1).Bx(0);
    bc.next("LOAD_CONST key a").op(OpCode::LOAD_CONST).A(2).Bx(1);
    bc.next("LIST_NEW").op(OpCode::LIST_NEW).A(5).B(1);
    bc.next("LOAD_INT nested element").op(OpCode::LOAD_INT).A(6).Bx(load_int_bx(1));
    bc.next("LIST_APPEND").op(OpCode::LIST_APPEND).A(5).B(6);
    bc.next("MOVE list result to its own dst").op(OpCode::MOVE).A(4).B(5);
    bc.next("MOVE list into value register").op(OpCode::MOVE).A(3).B(4);
    // Registers 2 and 3 hold the first pair ("a", [1]); fn_reg is 1, so the
    // second pair MUST land in registers 4 and 5 to stay inside the
    // contiguous 4-register argument block ([fn_reg+1 .. fn_reg+4]) that
    // the upcoming IC_CALL(fn_reg=1, argc=4) will read. A compiler that
    // forgets to free the nested list's leaked register instead allocates
    // "b"/2 into registers 5/6 -- one past where IC_CALL will look.
    bc.next("LOAD_CONST key b").op(OpCode::LOAD_CONST).A(4).Bx(2);
    bc.next("LOAD_INT value 2").op(OpCode::LOAD_INT).A(5).Bx(load_int_bx(2));
    bc.next("IC_CALL").op(OpCode::IC_CALL).A(1).B(4);
    bc.next("MOVE result").op(OpCode::MOVE).A(0).B(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->constants.size(), 3u);
    EXPECT_TRUE(chunk->constants[0].is_string());
    EXPECT_EQ(chunk->constants[0].as_string()->str, "قاموس");
    EXPECT_TRUE(chunk->constants[1].is_string());
    EXPECT_EQ(chunk->constants[1].as_string()->str, "a");
    EXPECT_TRUE(chunk->constants[2].is_string());
    EXPECT_EQ(chunk->constants[2].as_string()->str, "b");
}
