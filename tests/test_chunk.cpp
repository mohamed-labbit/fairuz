#include "../fairuz/fcompiler.hpp"

#include <gtest/gtest.h>

using namespace fairuz::runtime;

TEST(Chunk, EmitReturnsCorrectIndex)
{
    Chunk c;

    u32 i0 = c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { });
    u32 i1 = c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { });
    u32 i2 = c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { });

    EXPECT_EQ(i0, 0u);
    EXPECT_EQ(i1, 1u);
    EXPECT_EQ(i2, 2u);
    EXPECT_EQ(c.code.size(), 3u);
}

TEST(Chunk, EmittedInstructionPreserved)
{
    Chunk c;
    u32 instr = make_ABC(OpCode::OP_ADD, 1, 2, { });
    c.emit(instr, { });
    EXPECT_EQ(c.code[0], instr);
}

TEST(Chunk, PatchJumpForward)
{
    Chunk c;
    u32 jump_idx = c.emit(make_AsBx(OpCode::JUMP_IF_FALSE, 0, 0), { });

    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { });
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { });
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { });

    bool ok = c.patch_jump(jump_idx);
    EXPECT_TRUE(ok);
    EXPECT_EQ(instr_sBx(c.code[jump_idx]), 3);
}

TEST(Chunk, PatchJumpToSelf_OffsetZero)
{
    Chunk c;
    u32 idx = c.emit(make_AsBx(OpCode::JUMP, 0, 0), { });
    bool ok = c.patch_jump(idx);
    EXPECT_TRUE(ok);
    EXPECT_EQ(instr_sBx(c.code[idx]), 0);
}

TEST(Chunk, PatchJumpPreservesOpAndA)
{
    Chunk c;
    u32 idx = c.emit(make_AsBx(OpCode::JUMP_IF_FALSE, 7, 0), { });
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { });
    c.patch_jump(idx);
    EXPECT_EQ(static_cast<OpCode>(instr_op(c.code[idx])), OpCode::JUMP_IF_FALSE);
    EXPECT_EQ(instr_A(c.code[idx]), 7u);
}

TEST(Chunk, AddConstantDeduplicatesIntegers)
{
    Chunk c;
    u16 i0 = c.add_constant(Value::from_int(42));
    u16 i1 = c.add_constant(Value::from_int(42));
    EXPECT_EQ(i0, i1);
    EXPECT_EQ(c.constants.size(), 1u);
}

TEST(Chunk, AddConstantDeduplicatesDoubles)
{
    Chunk c;
    u16 i0 = c.add_constant(Value::from_real(3.14));
    u16 i1 = c.add_constant(Value::from_real(3.14));
    EXPECT_EQ(i0, i1);
    EXPECT_EQ(c.constants.size(), 1u);
}

TEST(Chunk, AddConstantDeduplicatesNil)
{
    Chunk c;
    u16 i0 = c.add_constant(Value::nil());
    u16 i1 = c.add_constant(Value::nil());
    EXPECT_EQ(i0, i1);
    EXPECT_EQ(c.constants.size(), 1u);
}

TEST(Chunk, AddConstantDistinguishesDifferentValues)
{
    Chunk c;
    u16 i0 = c.add_constant(Value::from_int(1));
    u16 i1 = c.add_constant(Value::from_int(2));
    EXPECT_NE(i0, i1);
    EXPECT_EQ(c.constants.size(), 2u);
}

TEST(Chunk, AddConstantIntAndDoubleNotDeduplicated)
{
    Chunk c;
    u16 i0 = c.add_constant(Value::from_int(1));
    u16 i1 = c.add_constant(Value::from_real(1.0));
    EXPECT_NE(i0, i1);
    EXPECT_EQ(c.constants.size(), 2u);
}

TEST(Chunk, AddConstantReturnSequentialIndices)
{
    Chunk c;
    for (int i = 0; i < 10; i++) {
        u16 idx = c.add_constant(Value::from_int(i * 1000));
        EXPECT_EQ(idx, static_cast<u16>(i));
    }
}

TEST(Chunk, AllocICSlotSequential)
{
    Chunk c;
    u8 s0 = c.alloc_ic_slot();
    u8 s1 = c.alloc_ic_slot();
    u8 s2 = c.alloc_ic_slot();
    EXPECT_EQ(s0, 0u);
    EXPECT_EQ(s1, 1u);
    EXPECT_EQ(s2, 2u);
    EXPECT_EQ(c.ic_slots.size(), 3u);
}

TEST(Chunk, AllocICSlotDefaultState)
{
    Chunk c;
    c.alloc_ic_slot();
    auto& slot = c.ic_slots[0];
    EXPECT_EQ(slot.seen_lhs, static_cast<u8>(TypeTag::NONE));
    EXPECT_EQ(slot.seen_rhs, static_cast<u8>(TypeTag::NONE));
    EXPECT_EQ(slot.seen_ret, static_cast<u8>(TypeTag::NONE));
    EXPECT_EQ(slot.hit_count, 0u);
    EXPECT_EQ(slot.jit_stub, nullptr);
}

TEST(Chunk, ICSlotCanBeUpdated)
{
    Chunk c;
    c.alloc_ic_slot();
    auto& slot = c.ic_slots[0];
    slot.seen_lhs = static_cast<u8>(TypeTag::INT);
    slot.seen_rhs = static_cast<u8>(TypeTag::INT);
    slot.hit_count = 500;
    EXPECT_EQ(slot.seen_lhs, static_cast<u8>(TypeTag::INT));
    EXPECT_EQ(slot.hit_count, 500u);
}

TEST(Chunk, RejectsMoreInlineCacheSlotsThanBytecodeCanEncode)
{
    Chunk c;
    for (u32 i = 0; i <= MAX_IC_SLOTS; i++)
        c.alloc_ic_slot();

    EXPECT_THROW(c.alloc_ic_slot(), fairuz::diagnostic::DiagnosticAbort);
}

TEST(Chunk, GetLineSingleLine)
{
    Chunk c;
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { 10, 0, 0 });
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { 10, 0, 0 });
    EXPECT_EQ(c.get_line(0), 10u);
    EXPECT_EQ(c.get_line(1), 10u);
}

TEST(Chunk, GetLineMultipleLines)
{
    Chunk c;
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { 1, 0, 0 });
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { 1, 0, 0 });
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { 5, 0, 0 });
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { 9, 0, 0 });
    EXPECT_EQ(c.get_line(0), 1u);
    EXPECT_EQ(c.get_line(1), 1u);
    EXPECT_EQ(c.get_line(2), 5u);
    EXPECT_EQ(c.get_line(3), 9u);
}

TEST(Chunk, GetLineRunLengthCompressed)
{
    Chunk c;
    for (int i = 0; i < 10; i++)
        c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { 42 });
    EXPECT_EQ(c.lines.size(), 1u);
    for (int i = 0; i < 10; i++)
        EXPECT_EQ(c.get_line(i), 42u);
}

TEST(Chunk, GetLineNewEntryOnLineChange)
{
    Chunk c;
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { 0, 0, 0 });
    c.emit(make_ABC(OpCode::NOP, 0, 0, 0), { 1, 0, 0 });
    EXPECT_EQ(c.lines.size(), 2u);
}

TEST(Chunk, OwnsSubFunctions)
{
    auto* m_parent = fairuz::runtime::make_chunk();
    auto* child = fairuz::runtime::make_chunk();
    child->name = "child";
    m_parent->functions.push(child);
    SUCCEED();
}

TEST(Chunk, SubFunctionPreservesData)
{
    Chunk m_parent;
    auto* child = fairuz::runtime::make_chunk();
    child->name = "myfunc";
    child->arity = 2;
    child->emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), { });
    m_parent.functions.push(child);

    EXPECT_EQ(m_parent.functions.size(), 1u);
    EXPECT_EQ(m_parent.functions[0]->name, "myfunc");
    EXPECT_EQ(m_parent.functions[0]->arity, 2);
    EXPECT_EQ(m_parent.functions[0]->code.size(), 1u);
}

TEST(Chunk, IsMoveConstructible)
{
    Chunk a;
    a.name = "moved";
    a.emit(make_ABC(OpCode::NOP, 0, 0, 0), { });
    Chunk b(std::move(a));
    EXPECT_EQ(b.name, "moved");
    EXPECT_EQ(b.code.size(), 1u);
}

TEST(Chunk, IsMoveAssignable)
{
    Chunk a;
    a.name = "src";
    Chunk b;
    b = std::move(a);
    EXPECT_EQ(b.name, "src");
}

TEST(CompilerState, AllocRegIncrementsWatermark)
{
    Chunk c;
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
    Chunk c;
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
    Chunk c;
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
    Chunk c;
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
    Chunk c;
    CompilerState s;
    s.chunk = &c;
    EXPECT_EQ(s.next_reg, 0u);
    s.free_register();
    EXPECT_EQ(s.next_reg, 0u);
}
