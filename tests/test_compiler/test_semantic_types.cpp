//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Documents semantic type boundaries and register-count invariants.
//===----------------------------------------------------------------------===//

#include "Compiler/bytecode_emitter.hpp"
#include "Compiler/compiler.hpp"
#include "Compiler/register_allocator.hpp"
#include "Interpreter/value.hpp"

#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

using namespace fairuz;
using namespace fairuz::runtime;

namespace {

template<typename T>
concept HasAddition = requires(T a, T b) { a + b; };

template<typename T>
concept HasPostIncrement = requires(T a) { a++; };

// These assertions intentionally reject accidental integer impersonation and
// cross-pool calls, without requiring a runtime test to exercise the mistakes.
static_assert(RegType { }.value == 0);
static_assert(std::is_trivially_copyable_v<RegType>);
static_assert(sizeof(RegType) == sizeof(u8));
static_assert(!std::is_convertible_v<u8, RegType>);
static_assert(!std::is_convertible_v<RegType, u8>);
static_assert(!std::is_constructible_v<RegType, CacheSlot>);
static_assert(!std::is_constructible_v<MethodSlot, CacheSlot>);
static_assert(!std::is_constructible_v<ConstantIndex, BigIntIndex>);
static_assert(!std::is_constructible_v<ConstantIndex, FunctionIndex>);
static_assert(!std::is_constructible_v<ClassIndex, FunctionIndex>);
static_assert(!HasAddition<RegType>);
static_assert(!HasPostIncrement<PCType>);
static_assert(std::is_same_v<decltype(instr_A(InstrType { })), u8>);
static_assert(std::is_same_v<decltype(instr_Bx(InstrType { })), u16>);
static_assert(std::is_same_v<decltype(std::declval<RegisterAllocator>().max_reg()), u16>);
static_assert(!std::is_invocable_v<decltype(&ByteCodeEmitter::load_constant), ByteCodeEmitter&, RegType, FunctionIndex, SourceLocation>);
static_assert(!std::is_invocable_v<decltype(&ByteCodeEmitter::invoke), ByteCodeEmitter&, RegType, CacheSlot, u32, SourceLocation>);
static_assert(!std::is_invocable_v<decltype(&ByteCodeEmitter::ic_call), ByteCodeEmitter&, RegType, RegType, SourceLocation>);

} // namespace

TEST(SemanticOperands, ExpressionFactoriesInitializeTheSelectedUnionMember)
{
    ExprResult empty;
    EXPECT_EQ(empty.kind, ExprResult::Kind::KNIL);
    auto reg = ExprResult::reg(RegType { 7 });
    EXPECT_EQ(reg.kind, ExprResult::Kind::REG);
    EXPECT_EQ(reg.reg_, RegType { 7 });
    auto reloc = ExprResult::reloc(PCType { 19 });
    EXPECT_EQ(reloc.kind, ExprResult::Kind::RELOC);
    EXPECT_EQ(reloc.reloc_pc, PCType { 19 });
    auto integer = ExprResult::kint(-4);
    EXPECT_EQ(integer.kind, ExprResult::Kind::KINT);
    EXPECT_EQ(integer.ival, -4);
    auto floating = ExprResult::kfloat(1.5);
    EXPECT_EQ(floating.kind, ExprResult::Kind::KFLOAT);
    EXPECT_EQ(floating.dval, 1.5);
    auto boolean = ExprResult::kbool(true);
    EXPECT_EQ(boolean.kind, ExprResult::Kind::KBOOL);
    EXPECT_TRUE(boolean.bval);
    EXPECT_EQ(ExprResult::knil().kind, ExprResult::Kind::KNIL);
}

TEST(RegisterAllocator, AllocatesThePreviousCursorNotTheIncrementedCursor)
{
    RegisterAllocator allocator;
    auto first = allocator.alloc_register();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first.value().value, 0u);
    auto second = allocator.alloc_register();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second.value().value, 1u);
    EXPECT_EQ(allocator.next_reg(), 2u);
    EXPECT_EQ(allocator.max_reg(), 2u);
}

TEST(RegisterAllocator, FullCursorIsACountRatherThanARegister)
{
    RegisterAllocator allocator;
    for (u16 expected = 0; expected < MAX_REGS; ++expected) {
        auto reg = allocator.alloc_register();
        ASSERT_TRUE(reg.has_value());
        EXPECT_EQ(reg.value().value, expected);
    }
    EXPECT_EQ(allocator.next_reg(), MAX_REGS);
    EXPECT_EQ(allocator.max_reg(), MAX_REGS);

    auto overflow = allocator.alloc_register();
    ASSERT_TRUE(overflow.has_error());
    EXPECT_EQ(overflow.error().get_code(), ErrorCode::TOO_MANY_REGISTERS);
    EXPECT_EQ(allocator.next_reg(), MAX_REGS);
    EXPECT_EQ(allocator.max_reg(), MAX_REGS);

    // A checkpoint may be one past the last valid register ID.
    allocator.rewind(MAX_REGS);
    EXPECT_EQ(allocator.next_reg(), MAX_REGS);
}

TEST(RegisterAllocator, RewindReusesSlotsWithoutLosingThePeakCount)
{
    RegisterAllocator allocator;
    ASSERT_TRUE(allocator.reserve_register(RegType { 12 }).has_value());
    allocator.rewind(4);
    auto reg = allocator.alloc_register();
    ASSERT_TRUE(reg.has_value());
    EXPECT_EQ(reg.value().value, 4u);
    EXPECT_EQ(allocator.next_reg(), 5u);
    EXPECT_EQ(allocator.max_reg(), 13u);
}

TEST(RegisterAllocator, InvalidReservationDoesNotMutateState)
{
    RegisterAllocator allocator;
    ASSERT_TRUE(allocator.reserve_register(RegType { 3 }).has_value());
    auto invalid = allocator.reserve_register(RegType { MAX_REGS });
    ASSERT_TRUE(invalid.has_error());
    EXPECT_EQ(invalid.error().get_code(), ErrorCode::TOO_MANY_REGISTERS);
    EXPECT_EQ(allocator.next_reg(), 4u);
    EXPECT_EQ(allocator.max_reg(), 4u);
}

TEST(RegisterAllocator, ReservingAnExistingRegisterDoesNotLowerTheCursor)
{
    RegisterAllocator allocator;
    ASSERT_TRUE(allocator.reserve_register(RegType { 9 }).has_value());
    ASSERT_TRUE(allocator.reserve_register(RegType { 2 }).has_value());
    EXPECT_EQ(allocator.next_reg(), 10u);
    EXPECT_EQ(allocator.max_reg(), 10u);
    allocator.free_register();
    EXPECT_EQ(allocator.next_reg(), 9u);
    EXPECT_EQ(allocator.max_reg(), 10u);
}

TEST(SemanticOperands, InlineCacheMasksPreserveHighTypeBits)
{
    ICSlot slot;
    slot.seen_lhs |= static_cast<u16>(TypeTag::NATIVE);
    slot.seen_rhs |= static_cast<u16>(TypeTag::INSTANCE);
    slot.seen_ret |= static_cast<u16>(TypeTag::MODULE);
    EXPECT_TRUE(has_tag(static_cast<TypeTag>(slot.seen_lhs), TypeTag::NATIVE));
    EXPECT_TRUE(has_tag(static_cast<TypeTag>(slot.seen_rhs), TypeTag::INSTANCE));
    EXPECT_TRUE(has_tag(static_cast<TypeTag>(slot.seen_ret), TypeTag::MODULE));
}

TEST(SemanticOperands, SignedDecodingSubtractsBiasBeforeNarrowing)
{
    EXPECT_EQ(instr_sBx(make_AsBx(OpCode::JUMP, 0, -JUMP_OFFSET)), -JUMP_OFFSET);
    EXPECT_EQ(instr_sBx(make_AsBx(OpCode::JUMP, 0, JUMP_OFFSET)), JUMP_OFFSET);
    EXPECT_EQ(instr_sBx(make_ABx(OpCode::LOAD_INT, 0, 0x8000)), 1);
    EXPECT_EQ(instr_sBx(make_ABx(OpCode::LOAD_INT, 0, 0xFFFF)), 32768);
    EXPECT_EQ(instr_sC(make_ABC(OpCode::NOP, 0, 0, 0)), -128);
    EXPECT_EQ(instr_sC(make_ABC(OpCode::NOP, 0, 0, 0xFF)), 127);
}
