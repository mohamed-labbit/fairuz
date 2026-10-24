//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMCompare, EqIntsTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, EqIntsFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, NeqTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_NEQ, 2, 0, 1).ret(2);
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LtTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 3).load_int(1, 7).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LtFalseEqual)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, LeTrue_Equal)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LeFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, EqSameString)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.str("hello");
    u16 k1 = b.str("hello");
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, NotFalseIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, NotTrueIsFalse)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, NotNilIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_NIL, 0, 0, 1).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, NotZeroIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LtStrings)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 ka = b.str("apple");
    u16 kb = b.str("banana");
    b.ABx(OpCode::LOAD_CONST, 0, ka).ABx(OpCode::LOAD_CONST, 1, kb).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}
