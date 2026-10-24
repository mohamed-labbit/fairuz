//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMBitwise, And)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b1010).ABC(OpCode::OP_BITAND, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 0b1010);
}

TEST(VMBitwise, Or)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1100).load_int(1, 0b0011).ABC(OpCode::OP_BITOR, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 0b1111);
}

TEST(VMBitwise, Xor)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b0101).ABC(OpCode::OP_BITXOR, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 0b1010);
}

TEST(VMBitwise, Not)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(OpCode::OP_BITNOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), ~i64(0));
}

TEST(VMBitwise, Shl)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1).ABC(OpCode::OP_LSHIFT, 1, 0, 8).ret(1);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 256);
}

TEST(VMBitwise, Shr)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1024).ABC(OpCode::OP_RSHIFT, 1, 0, 3).ret(1);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 128);
}

TEST(VMBitwise, ShrArithmetic_NegativeInput)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, -8).load_int(1, 1).ABC(OpCode::OP_RSHIFT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -4);
}

TEST(VMBitwise, AndOnDoubleThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run("طبيعي(1.0 & 1)\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}
