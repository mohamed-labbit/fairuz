//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerVar, LocalDeclaration)
{
    Chunk* ch = compile_ok_local({ "س := 5\n" });
    BytecodeChecker bc(*ch);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(5));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    EXPECT_EQ(ch->local_count, 1u);
}

TEST(CompilerVar, TwoLocalsUseConsecutiveRegisters)
{
    Chunk* ch = compile_ok_local({
        "س := 1\n",
        "ص := 2\n",
    });

    BytecodeChecker bc(*ch);
    bc.next("LOAD_INT x").op(OpCode::LOAD_INT).A(0);
    bc.next("LOAD_INT y").op(OpCode::LOAD_INT).A(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(ch->local_count, 2);
}

TEST(CompilerVar, LocalAssignmentWritesBackToSameRegister)
{
    Chunk* ch = compile_ok_local({
        "س := 1\n",
        "س := 2\n",
    });

    BytecodeChecker bc(*ch);
    bc.next("decl_stmt x=1").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.next("assign x=2").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(2));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerVar, GlobalLoadAndStore)
{
    Chunk* chunk = compile_ok({ "عام := 7\n", "عام\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("RHS").op(OpCode::LOAD_INT).Bx(load_int_bx(7));
    bc.next("STORE_GLOBAL").op(OpCode::STORE_GLOBAL);
    bc.next("LOAD_GLOBAL").op(OpCode::LOAD_GLOBAL);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    bool found = false;
    for (auto& v : chunk->constants) {
        if (v.is_string() && v.as_string()->str == "عام") found = true;
    }
    EXPECT_TRUE(found) << "global ident 'عام' not interned into constant pool";
}
