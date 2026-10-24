//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMICProfile, BinaryOpUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 3).load_int(1, 4).ABC(OpCode::OP_ADD, 2, 0, 1).nop(0).ret(2);
    run_success(r.run(b));
    if (test_config::dump_bytecode) b.dump();
    auto const& s = r.chunk_->ic_slots[0];
    EXPECT_TRUE(has_tag(TypeTag(s.seen_lhs), TypeTag::INT));
    EXPECT_TRUE(has_tag(TypeTag(s.seen_rhs), TypeTag::INT));
    EXPECT_TRUE(has_tag(TypeTag(s.seen_ret), TypeTag::INT));
    EXPECT_GE(s.hit_count, 1u);
}

TEST(VMICProfile, SubUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 3).ABC(OpCode::OP_SUB, 2, 0, 1).nop(0).ret(2);
    if (test_config::dump_bytecode) b.dump();
    run_success(r.run(b));
    EXPECT_GE(r.chunk_->ic_slots[0].hit_count, 1u);
}

TEST(VMICProfile, SlotAccumulatesAcrossLoopIterations)
{
    VMRunner r;
    CB b;
    b.regs(5)
        .slot()
        .load_int(0, 0)
        .load_int(1, 5)
        .load_int(2, 0)
        .ABC(OpCode::OP_LT, 3, 0, 1)
        .AsBx(OpCode::JUMP_IF_FALSE, 3, 4)
        .load_int(4, 1)
        .ABC(OpCode::OP_ADD, 0, 0, 4)
        .nop(0)
        .AsBx(OpCode::LOOP, 0, -6)
        .ret(0);
    if (test_config::dump_bytecode) b.dump();
    run_success(r.run(b));
    EXPECT_EQ(r.chunk_->ic_slots[0].hit_count, 5u);
}
