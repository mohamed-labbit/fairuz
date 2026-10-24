//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerLiterals, NilExpression)
{
    Chunk* chunk = compile_ok("عدم\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_NIL").op(OpCode::LOAD_NIL).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiterals, TrueLiteral)
{
    Chunk* chunk = compile_ok("صحيح\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_TRUE").op(OpCode::LOAD_TRUE).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiterals, FalseLiteral)
{
    Chunk* chunk = compile_ok("خطا\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_FALSE").op(OpCode::LOAD_FALSE).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiterals, SmallIntegerUsesLoadInt)
{
    Chunk* chunk = compile_ok("42\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(42));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_TRUE(chunk->constants.empty());
}

TEST(CompilerLiterals, VeryBigPositiveIntegerUsesLoadBigInt)
{
#if FA_USE_NANBOX
    Chunk* chunk = compile_ok(std::to_string(static_cast<i64>(Value::int_max() + 2)) + "\n");
#else
    Chunk* chunk = compile_ok("9223372036854775808\n");
#endif
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_BIG_INT").op(OpCode::LOAD_BIG_INT).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_FALSE(chunk->big_ints.empty());
}

TEST(CompilerLiterals, VeryBigNegativeIntegerUsesLoadBigInt)
{
#if FA_USE_NANBOX
    Chunk* chunk = compile_ok(std::to_string(static_cast<i64>(Value::int_min() - 2)) + "\n");
#else
    Chunk* chunk = compile_ok("-9223372036854775809\n");
#endif
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_BIG_INT").op(OpCode::LOAD_BIG_INT).A(0);
    bc.next("OP_NEG").op(OpCode::OP_NEG).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_FALSE(chunk->big_ints.empty());
}

TEST(CompilerLiterals, NegativeSmallIntegerUsesLoadIntAndNegation)
{
    Chunk* chunk = compile_ok("-100\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).Bx(load_int_bx(100));
    bc.next("OP_NEG").op(OpCode::OP_NEG).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiterals, ZeroUsesLoadInt)
{
    Chunk* chunk = compile_ok("0\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).Bx(load_int_bx(0));
}

TEST(CompilerLiterals, LargeIntegerUsesConstantPool)
{
    Chunk* chunk = compile_ok("100000\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].is_int());
    EXPECT_EQ(chunk->constants[0].as_int(), 100000);
}

TEST(CompilerLiterals, FloatUsesConstantPool)
{
    Chunk* chunk = compile_ok("3.14\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].is_double());
    EXPECT_NEAR(chunk->constants[0].as_double(), 3.14, 1e-9);
}

TEST(CompilerLiterals, StringUsesConstantPool)
{
    Chunk* chunk = compile_ok("\"hello\"\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].is_string());
    EXPECT_EQ(chunk->constants[0].as_string()->str, "hello");
}

TEST(CompilerLiterals, StringsDeduplicated)
{
    Array<std::string> stmts;
    stmts.push("\"dup\"\n");
    stmts.push("\"dup\"\n");
    Chunk* chunk = compile_ok(test_source::join(std::move(stmts), ""));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    long string_constants
        = std::count_if(chunk->constants.begin(), chunk->constants.end(), [](Value const& v) { return v.is_string(); });
    EXPECT_EQ(string_constants, 1);
}
