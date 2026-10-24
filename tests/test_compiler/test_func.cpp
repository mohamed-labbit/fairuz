//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerFunc, FunctionReturningNil)
{
    Chunk* chunk = compile_ok("دالة مثال():\n"
                              "    ارجع\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("CLOSURE").op(OpCode::CLOSURE).A(0).Bx(0);
    bc.next("STORE_GLOBAL").op(OpCode::STORE_GLOBAL).A(0).Bx(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->functions.size(), 1u);
    Chunk const* fn = chunk->functions[0];
    EXPECT_EQ(fn->name, "مثال");
    EXPECT_EQ(fn->arity, 0);
    BytecodeChecker fbc(*fn);
    fbc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    fbc.done();
}

TEST(CompilerFunc, FunctionWithParams)
{
    Chunk* chunk = compile_ok("دالة اجمع(ا, ب):\n"
                              "    ارجع (ا + ب)\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    Chunk const* fn = chunk->functions[0];
    EXPECT_EQ(fn->arity, 2);
    EXPECT_EQ(fn->local_count, 3);

    BytecodeChecker fbc(*fn);

    fbc.next("OP_ADD").op(OpCode::OP_ADD).A(2).B(0).C(1);
    fbc.next("NOP").op(OpCode::NOP).A(0);
    fbc.next("RETURN").op(OpCode::RETURN).A(2).B(1);
    fbc.done();
}

TEST(CompilerFunc, FunctionStoredAsLocal)
{
    Chunk* chunk = compile_ok("دالة مثال():\n"
                              "    عدم\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_EQ(chunk->local_count, 1);
}

TEST(CompilerFunc, NestedFunctionIndexing)
{
    Chunk* chunk = compile_ok({ "دالة ا():\n"
                                "    عدم\n",
        "دالة ب():\n"
        "    عدم\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 2u);
    BytecodeChecker bc(*chunk);
    bc.next("CLOSURE a").op(OpCode::CLOSURE).A(0).Bx(0);
    bc.next("STORE_GLOBAL a").op(OpCode::STORE_GLOBAL).A(0).Bx(0);
    bc.next("CLOSURE b").op(OpCode::CLOSURE).A(1).Bx(1);
    bc.next("STORE_GLOBAL b").op(OpCode::STORE_GLOBAL).A(1).Bx(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerFunc, RecursiveFunctionBodyCompiles)
{
    // fn fact(n) { if (n) { return n } return 1 }
    Chunk* chunk = compile_ok("دالة مضروب(ن):\n"
                              "    اذا ن:\n"
                              "        ارجع ن\n"
                              "    ارجع 1\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    EXPECT_FALSE(chunk->functions[0]->code.empty());
}
