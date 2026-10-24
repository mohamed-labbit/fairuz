//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMArith, AddIntFastPath)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 32).ABC(OpCode::OP_ADD, 2, 0, 1).nop().ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 42);
}

TEST(VMArith, AddDoubles)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.ch->add_constant(Value::from_real(1.5)).value;
    u16 k1 = b.ch->add_constant(Value::from_real(2.5)).value;
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_ADD, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 4.0);
}

TEST(VMArith, AddStringsConcat)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.str("foo");
    u16 k1 = b.str("bar");
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_ADD, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.as_string()->str, "foobar");
}

TEST(VMArith, SubInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 100).load_int(1, 58).ABC(OpCode::OP_SUB, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 42);
}

TEST(VMArith, MulInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 7).ABC(OpCode::OP_MUL, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 42);
}

TEST(VMArith, DivDouble)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.ch->add_constant(Value::from_real(84.0)).value;
    u16 k1 = b.ch->add_constant(Value::from_real(2.0)).value;
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_DIV, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_DOUBLE_EQ(run_success(r.run(b)).as_double(), 42.0);
}

TEST(VMArith, ModPositive)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 17).load_int(1, 5).ABC(OpCode::OP_MOD, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value result = run_success(r.run(b));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST(VMArith, Pow)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 2).load_int(1, 10).ABC(OpCode::OP_POW, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value result = run_success(r.run(b));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 1024);
}

TEST(VMArith, NegInt)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 7).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), -7);
}

TEST(VMArith, NegDouble)
{
    VMRunner r;
    CB b;
    b.regs(2);
    u16 k = b.ch->add_constant(Value::from_real(3.5)).value;
    b.ABx(OpCode::LOAD_CONST, 0, k).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_DOUBLE_EQ(run_success(r.run(b)).as_double(), -3.5);
}

TEST(VMArith, DivByZeroThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run("طبيعي(1.0 / 0.0)\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMArith, NegOnStringThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run("طبيعي(-\"س\")\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}
