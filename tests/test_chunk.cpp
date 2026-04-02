#include "../include/compiler.hpp"

#include <gtest/gtest.h>

using namespace fairuz::runtime;

TEST(Fa_Chunk, EmitReturnsCorrectIndex)
{
    Fa_Chunk c;

    u32 i0 = c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { });
    u32 i1 = c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { });
    u32 i2 = c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { });

    EXPECT_EQ(i0, 0u);
    EXPECT_EQ(i1, 1u);
    EXPECT_EQ(i2, 2u);
    EXPECT_EQ(c.code.size(), 3u);
}

TEST(Fa_Chunk, EmittedInstructionPreserved)
{
    Fa_Chunk c;
    u32 instr = Fa_make_ABC(Fa_OpCode::OP_ADD, 1, 2, { });
    c.emit(instr, { });
    EXPECT_EQ(c.code[0], instr);
}

TEST(Fa_Chunk, PatchJumpForward)
{
    Fa_Chunk c;
    u32 jump_idx = c.emit(Fa_make_AsBx(Fa_OpCode::JUMP_IF_FALSE, 0, 0), { });

    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { });
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { });
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { });

    bool ok = c.patch_jump(jump_idx);
    EXPECT_TRUE(ok);
    EXPECT_EQ(Fa_instr_sBx(c.code[jump_idx]), 3);
}

TEST(Fa_Chunk, PatchJumpToSelf_OffsetZero)
{
    Fa_Chunk c;
    u32 idx = c.emit(Fa_make_AsBx(Fa_OpCode::JUMP, 0, 0), { });
    bool ok = c.patch_jump(idx);
    EXPECT_TRUE(ok);
    EXPECT_EQ(Fa_instr_sBx(c.code[idx]), 0);
}

TEST(Fa_Chunk, PatchJumpPreservesOpAndA)
{
    Fa_Chunk c;
    u32 idx = c.emit(Fa_make_AsBx(Fa_OpCode::JUMP_IF_FALSE, 7, 0), { });
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { });
    c.patch_jump(idx);
    EXPECT_EQ(static_cast<Fa_OpCode>(Fa_instr_op(c.code[idx])), Fa_OpCode::JUMP_IF_FALSE);
    EXPECT_EQ(Fa_instr_A(c.code[idx]), 7u);
}

TEST(Fa_Chunk, AddConstantDeduplicatesIntegers)
{
    Fa_Chunk c;
    u16 i0 = c.add_constant(Fa_MAKE_INTEGER(42));
    u16 i1 = c.add_constant(Fa_MAKE_INTEGER(42));
    EXPECT_EQ(i0, i1);
    EXPECT_EQ(c.constants.size(), 1u);
}

TEST(Fa_Chunk, AddConstantDeduplicatesDoubles)
{
    Fa_Chunk c;
    u16 i0 = c.add_constant(Fa_MAKE_REAL(3.14));
    u16 i1 = c.add_constant(Fa_MAKE_REAL(3.14));
    EXPECT_EQ(i0, i1);
    EXPECT_EQ(c.constants.size(), 1u);
}

TEST(Fa_Chunk, AddConstantDeduplicatesNil)
{
    Fa_Chunk c;
    u16 i0 = c.add_constant(NIL_VAL);
    u16 i1 = c.add_constant(NIL_VAL);
    EXPECT_EQ(i0, i1);
    EXPECT_EQ(c.constants.size(), 1u);
}

TEST(Fa_Chunk, AddConstantDistinguishesDifferentValues)
{
    Fa_Chunk c;
    u16 i0 = c.add_constant(Fa_MAKE_INTEGER(1));
    u16 i1 = c.add_constant(Fa_MAKE_INTEGER(2));
    EXPECT_NE(i0, i1);
    EXPECT_EQ(c.constants.size(), 2u);
}

TEST(Fa_Chunk, AddConstantIntAndDoubleNotDeduplicated)
{
    Fa_Chunk c;
    u16 i0 = c.add_constant(Fa_MAKE_INTEGER(1));
    u16 i1 = c.add_constant(Fa_MAKE_REAL(1.0));
    EXPECT_NE(i0, i1);
    EXPECT_EQ(c.constants.size(), 2u);
}

TEST(Fa_Chunk, AddConstantReturnSequentialIndices)
{
    Fa_Chunk c;
    for (int i = 0; i < 10; i += 1) {
        u16 idx = c.add_constant(Fa_MAKE_INTEGER(i * 1000));
        EXPECT_EQ(idx, static_cast<u16>(i));
    }
}

TEST(Fa_Chunk, AllocICSlotSequential)
{
    Fa_Chunk c;
    u8 s0 = c.alloc_ic_slot();
    u8 s1 = c.alloc_ic_slot();
    u8 s2 = c.alloc_ic_slot();
    EXPECT_EQ(s0, 0u);
    EXPECT_EQ(s1, 1u);
    EXPECT_EQ(s2, 2u);
    EXPECT_EQ(c.ic_slots.size(), 3u);
}

TEST(Fa_Chunk, AllocICSlotDefaultState)
{
    Fa_Chunk c;
    c.alloc_ic_slot();
    auto& slot = c.ic_slots[0];
    EXPECT_EQ(slot.seen_lhs, static_cast<u8>(Fa_TypeTag::NONE));
    EXPECT_EQ(slot.seen_rhs, static_cast<u8>(Fa_TypeTag::NONE));
    EXPECT_EQ(slot.seen_ret, static_cast<u8>(Fa_TypeTag::NONE));
    EXPECT_EQ(slot.hit_count, 0u);
    EXPECT_EQ(slot.jit_stub, nullptr);
}

TEST(Fa_Chunk, ICSlotCanBeUpdated)
{
    Fa_Chunk c;
    c.alloc_ic_slot();
    auto& slot = c.ic_slots[0];
    slot.seen_lhs = static_cast<u8>(Fa_TypeTag::INT);
    slot.seen_rhs = static_cast<u8>(Fa_TypeTag::INT);
    slot.hit_count = 500;
    EXPECT_EQ(slot.seen_lhs, static_cast<u8>(Fa_TypeTag::INT));
    EXPECT_EQ(slot.hit_count, 500u);
}

TEST(Fa_Chunk, GetLineSingleLine)
{
    Fa_Chunk c;
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { 10, 0, 0 });
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { 10, 0, 0 });
    EXPECT_EQ(c.get_line(0), 10u);
    EXPECT_EQ(c.get_line(1), 10u);
}

TEST(Fa_Chunk, GetLineMultipleLines)
{
    Fa_Chunk c;
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { 1, 0, 0 });
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { 1, 0, 0 });
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { 5, 0, 0 });
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { 9, 0, 0 });
    EXPECT_EQ(c.get_line(0), 1u);
    EXPECT_EQ(c.get_line(1), 1u);
    EXPECT_EQ(c.get_line(2), 5u);
    EXPECT_EQ(c.get_line(3), 9u);
}

TEST(Fa_Chunk, GetLineRunLengthCompressed)
{
    Fa_Chunk c;
    for (int i = 0; i < 10; i += 1)
        c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { 42 });
    EXPECT_EQ(c.lines.size(), 1u);
    for (int i = 0; i < 10; i += 1)
        EXPECT_EQ(c.get_line(i), 42u);
}

TEST(Fa_Chunk, GetLineNewEntryOnLineChange)
{
    Fa_Chunk c;
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { 0, 0, 0 });
    c.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { 1, 0, 0 });
    EXPECT_EQ(c.lines.size(), 2u);
}

TEST(Fa_Chunk, OwnsSubFunctions)
{
    auto* m_parent = fairuz::runtime::make_chunk();
    auto* child = fairuz::runtime::make_chunk();
    child->m_name = "child";
    m_parent->functions.push(child);
    SUCCEED();
}

TEST(Fa_Chunk, SubFunctionPreservesData)
{
    Fa_Chunk m_parent;
    auto* child = fairuz::runtime::make_chunk();
    child->m_name = "myfunc";
    child->arity = 2;
    child->emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), { });
    m_parent.functions.push(child);

    EXPECT_EQ(m_parent.functions.size(), 1u);
    EXPECT_EQ(m_parent.functions[0]->m_name, "myfunc");
    EXPECT_EQ(m_parent.functions[0]->arity, 2);
    EXPECT_EQ(m_parent.functions[0]->code.size(), 1u);
}

TEST(Fa_Chunk, IsMoveConstructible)
{
    Fa_Chunk a;
    a.m_name = "moved";
    a.emit(Fa_make_ABC(Fa_OpCode::NOP, 0, 0, 0), { });
    Fa_Chunk b(std::move(a));
    EXPECT_EQ(b.m_name, "moved");
    EXPECT_EQ(b.code.size(), 1u);
}

TEST(Fa_Chunk, IsMoveAssignable)
{
    Fa_Chunk a;
    a.m_name = "src";
    Fa_Chunk b;
    b = std::move(a);
    EXPECT_EQ(b.m_name, "src");
}

TEST(CompilerState, AllocRegIncrementsWatermark)
{
    Fa_Chunk c;
    CompilerState s;
    s.chunk = &c;
    EXPECT_EQ(s.alloc_register(), 0u);
    EXPECT_EQ(s.alloc_register(), 1u);
    EXPECT_EQ(s.alloc_register(), 2u);
    EXPECT_EQ(s.next_reg, 3u);
    EXPECT_EQ(s.max_reg, 3u);
}

TEST(CompilerState, FreeRegDecrements)
{
    Fa_Chunk c;
    CompilerState s;
    s.chunk = &c;
    s.alloc_register();
    s.alloc_register();
    s.free_register();
    EXPECT_EQ(s.next_reg, 1u);
    EXPECT_EQ(s.max_reg, 2u);
}

TEST(CompilerState, FreeRegsToWatermark)
{
    Fa_Chunk c;
    CompilerState s;
    s.chunk = &c;
    s.alloc_register(); // 0
    s.alloc_register(); // 1
    s.alloc_register(); // 2
    s.free_regs_to(1);
    EXPECT_EQ(s.next_reg, 1u);
    EXPECT_EQ(s.max_reg, 3u);
}

TEST(CompilerState, MaxRegTracksHighWatermark)
{
    Fa_Chunk c;
    CompilerState s;
    s.chunk = &c;
    s.alloc_register();
    s.alloc_register();
    s.alloc_register();
    s.free_regs_to(0);
    s.alloc_register();
    EXPECT_EQ(s.max_reg, 3u);
}

TEST(CompilerState, FreeRegAtZeroIsNoOp)
{
    Fa_Chunk c;
    CompilerState s;
    s.chunk = &c;
    EXPECT_EQ(s.next_reg, 0u);
    s.free_register();
    EXPECT_EQ(s.next_reg, 0u);
}
