//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMLoads, Nil)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_NIL, 0, 0, 1).ret(0);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_TRUE(run_success(r.run(b)).is_nil());
}

TEST(VMLoads, NilFillsMultiple)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(OpCode::LOAD_NIL, 0, 0, 3).ret(2);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_TRUE(run_success(r.run(b)).is_nil());
}

TEST(VMLoads, True)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ret(0);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMLoads, False)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ret(0);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMLoads, IntPositive)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 42).ret(0);
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 42);
}

TEST(VMLoads, IntZero)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 0).ret(0);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 0);
}

TEST(VMLoads, IntNegative)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, -100).ret(0);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -100);
}

TEST(VMLoads, IntMaxEncodable)
{
    VMRunner r;
    CB b;
    b.regs(1).ABx(OpCode::LOAD_INT, 0, 65535).ret(0);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 32768);
}

TEST(VMLoads, IntMinEncodable)
{
    VMRunner r;
    CB b;
    b.regs(1).ABx(OpCode::LOAD_INT, 0, 0).ret(0);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -32767);
}

TEST(VMLoads, ConstDouble)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.ch->add_constant(Value::from_real(3.14)).value;
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.14);
}

TEST(VMLoads, ConstString)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.str("hello");
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.as_string()->str, "hello");
}

TEST(VMLoads, ConstLargeInt)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.ch->add_constant(Value::from_int(1000000LL)).value;
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 1000000LL);
}

TEST(VMLoads, ReturnNil)
{
    VMRunner r;
    CB b;
    b.regs(1).ret_nil();
    EXPECT_TRUE(run_success(r.run(b)).is_nil());
}
