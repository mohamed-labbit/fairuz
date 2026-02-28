#include "../../../include/runtime/compiler/compiler.hpp"
#include "../../../include/runtime/compiler/value.hpp"
#include "../../../include/runtime/opcode/chunk.hpp"
#include "../../../include/runtime/opcode/opcode.hpp"
#include <gtest/gtest.h>

using namespace mylang::runtime;

TEST(Chunk, EmitReturnsCorrectIndex)
{
    Chunk c;

    uint32_t i0 = c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 1);
    uint32_t i1 = c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 1);
    uint32_t i2 = c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 1);

    EXPECT_EQ(i0, 0u);
    EXPECT_EQ(i1, 1u);
    EXPECT_EQ(i2, 2u);
    EXPECT_EQ(c.code.size(), 3u);
}

TEST(Chunk, EmittedInstructionPreserved)
{
    Chunk c;
    Instruction instr = make_ABC(static_cast<uint8_t>(OpCode::ADD), 1, 2, 3);
    c.emit(instr, 10);
    EXPECT_EQ(c.code[0], instr);
}

TEST(Chunk, PatchJumpForward)
{
    Chunk c;
    uint32_t jump_idx = c.emit(make_AsBx(static_cast<uint8_t>(OpCode::JUMP_IF_FALSE), 0, 0), 1);

    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 2);
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 3);
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 4);

    bool ok = c.patchJump(jump_idx);
    EXPECT_TRUE(ok);
    EXPECT_EQ(instr_sBx(c.code[jump_idx]), 3);
}

TEST(Chunk, PatchJumpToSelf_OffsetZero)
{
    Chunk c;
    uint32_t idx = c.emit(make_AsBx(static_cast<uint8_t>(OpCode::JUMP), 0, 0), 1);
    // No more instructions — patch immediately
    bool ok = c.patchJump(idx);
    EXPECT_TRUE(ok);
    // code.size()=1, offset = 1-0-1 = 0
    EXPECT_EQ(instr_sBx(c.code[idx]), 0);
}

TEST(Chunk, PatchJumpPreservesOpAndA)
{
    Chunk c;
    uint32_t idx = c.emit(make_AsBx(static_cast<uint8_t>(OpCode::JUMP_IF_FALSE), 7, 0), 1);
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 2);
    c.patchJump(idx);
    EXPECT_EQ(static_cast<OpCode>(instr_op(c.code[idx])), OpCode::JUMP_IF_FALSE);
    EXPECT_EQ(instr_A(c.code[idx]), 7u);
}

TEST(Chunk, AddConstantDeduplicatesIntegers)
{
    Chunk c;
    uint16_t i0 = c.addConstant(Value::integer(42));
    uint16_t i1 = c.addConstant(Value::integer(42));
    EXPECT_EQ(i0, i1);
    EXPECT_EQ(c.constants.size(), 1u);
}

TEST(Chunk, AddConstantDeduplicatesDoubles)
{
    Chunk c;
    uint16_t i0 = c.addConstant(Value::real(3.14));
    uint16_t i1 = c.addConstant(Value::real(3.14));
    EXPECT_EQ(i0, i1);
    EXPECT_EQ(c.constants.size(), 1u);
}

TEST(Chunk, AddConstantDeduplicatesNil)
{
    Chunk c;
    uint16_t i0 = c.addConstant(Value::nil());
    uint16_t i1 = c.addConstant(Value::nil());
    EXPECT_EQ(i0, i1);
    EXPECT_EQ(c.constants.size(), 1u);
}

TEST(Chunk, AddConstantDistinguishesDifferentValues)
{
    Chunk c;
    uint16_t i0 = c.addConstant(Value::integer(1));
    uint16_t i1 = c.addConstant(Value::integer(2));
    EXPECT_NE(i0, i1);
    EXPECT_EQ(c.constants.size(), 2u);
}

TEST(Chunk, AddConstantIntAndDoubleNotDeduplicated)
{
    // Value::integer(1) and Value::real(1.0) have different raw bits
    Chunk c;
    uint16_t i0 = c.addConstant(Value::integer(1));
    uint16_t i1 = c.addConstant(Value::real(1.0));
    EXPECT_NE(i0, i1);
    EXPECT_EQ(c.constants.size(), 2u);
}

TEST(Chunk, AddConstantReturnSequentialIndices)
{
    Chunk c;
    for (int i = 0; i < 10; ++i) {
        uint16_t idx = c.addConstant(Value::integer(i * 1000));
        EXPECT_EQ(idx, static_cast<uint16_t>(i));
    }
}

TEST(Chunk, AllocICSlotSequential)
{
    Chunk c;
    uint8_t s0 = c.allocIcSlot();
    uint8_t s1 = c.allocIcSlot();
    uint8_t s2 = c.allocIcSlot();
    EXPECT_EQ(s0, 0u);
    EXPECT_EQ(s1, 1u);
    EXPECT_EQ(s2, 2u);
    EXPECT_EQ(c.ic_slots.size(), 3u);
}

TEST(Chunk, AllocICSlotDefaultState)
{
    Chunk c;
    c.allocIcSlot();
    auto& slot = c.ic_slots[0];
    EXPECT_EQ(slot.seen_lhs, TypeTag::NONE);
    EXPECT_EQ(slot.seen_rhs, TypeTag::NONE);
    EXPECT_EQ(slot.seen_ret, TypeTag::NONE);
    EXPECT_EQ(slot.hit_count, 0u);
    EXPECT_EQ(slot.jit_stub, nullptr);
}

TEST(Chunk, ICSlotCanBeUpdated)
{
    Chunk c;
    c.allocIcSlot();
    auto& slot = c.ic_slots[0];
    slot.seen_lhs = TypeTag::INT;
    slot.seen_rhs = TypeTag::INT;
    slot.hit_count = 500;
    EXPECT_EQ(slot.seen_lhs, TypeTag::INT);
    EXPECT_EQ(slot.hit_count, 500u);
}

TEST(Chunk, GetLineSingleLine)
{
    Chunk c;
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 10);
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 10);
    EXPECT_EQ(c.getLine(0), 10u);
    EXPECT_EQ(c.getLine(1), 10u);
}

TEST(Chunk, GetLineMultipleLines)
{
    Chunk c;
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 1);
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 1);
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 5);
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 9);
    EXPECT_EQ(c.getLine(0), 1u);
    EXPECT_EQ(c.getLine(1), 1u);
    EXPECT_EQ(c.getLine(2), 5u);
    EXPECT_EQ(c.getLine(3), 9u);
}

TEST(Chunk, GetLineRunLengthCompressed)
{
    Chunk c;
    for (int i = 0; i < 10; ++i)
        c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 42);
    EXPECT_EQ(c.lines.size(), 1u);
    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(c.getLine(i), 42u);
}

TEST(Chunk, GetLineNewEntryOnLineChange)
{
    Chunk c;
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 1);
    c.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 2);
    EXPECT_EQ(c.lines.size(), 2u);
}

TEST(Chunk, OwnsSubFunctions)
{
    auto* parent = new Chunk();
    auto* child = new Chunk();
    child->name = "child";
    parent->functions.push_back(child);
    delete parent;
    SUCCEED();
}

TEST(Chunk, SubFunctionPreservesData)
{
    Chunk parent;
    auto* child = new Chunk();
    child->name = "myfunc";
    child->arity = 2;
    child->emit(make_ABC(static_cast<uint8_t>(OpCode::RETURN_NIL), 0, 0, 0), 1);
    parent.functions.push_back(child);

    EXPECT_EQ(parent.functions.size(), 1u);
    EXPECT_EQ(parent.functions[0]->name, "myfunc");
    EXPECT_EQ(parent.functions[0]->arity, 2);
    EXPECT_EQ(parent.functions[0]->code.size(), 1u);
}

TEST(Chunk, IsMoveConstructible)
{
    Chunk a;
    a.name = "moved";
    a.emit(make_ABC(static_cast<uint8_t>(OpCode::NOP), 0, 0, 0), 1);
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
    EXPECT_EQ(s.allocReg(), 0u);
    EXPECT_EQ(s.allocReg(), 1u);
    EXPECT_EQ(s.allocReg(), 2u);
    EXPECT_EQ(s.nextReg, 3u);
    EXPECT_EQ(s.maxReg, 3u);
}

TEST(CompilerState, FreeRegDecrements)
{
    Chunk c;
    CompilerState s;
    s.chunk = &c;
    s.allocReg();
    s.allocReg();
    s.freeReg();
    EXPECT_EQ(s.nextReg, 1u);
    EXPECT_EQ(s.maxReg, 2u);
}

TEST(CompilerState, FreeRegsToWatermark)
{
    Chunk c;
    CompilerState s;
    s.chunk = &c;
    s.allocReg(); // 0
    s.allocReg(); // 1
    s.allocReg(); // 2
    s.freeRegsTo(1);
    EXPECT_EQ(s.nextReg, 1u);
    EXPECT_EQ(s.maxReg, 3u);
}

TEST(CompilerState, MaxRegTracksHighWatermark)
{
    Chunk c;
    CompilerState s;
    s.chunk = &c;
    s.allocReg();
    s.allocReg();
    s.allocReg();
    s.freeRegsTo(0);
    s.allocReg();
    EXPECT_EQ(s.maxReg, 3u);
}

TEST(CompilerState, FreeRegAtZeroIsNoOp)
{
    Chunk c;
    CompilerState s;
    s.chunk = &c;
    EXPECT_EQ(s.nextReg, 0u);
    s.freeReg();
    EXPECT_EQ(s.nextReg, 0u);
}
