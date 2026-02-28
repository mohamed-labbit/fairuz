#include <cstdint>
#include <gtest/gtest.h>
#include <limits>

#include "../../../include/runtime/opcode/opcode.hpp"
#include "../../../include/runtime/opcode/chunk.hpp"

using namespace mylang::runtime;

// ============================================================================
// make_ABC round-trip
// ============================================================================

TEST(InstrABC, ZeroFields)
{
    Instruction i = make_ABC(0, 0, 0, 0);
    EXPECT_EQ(instr_op(i), 0u);
    EXPECT_EQ(instr_A(i), 0u);
    EXPECT_EQ(instr_B(i), 0u);
    EXPECT_EQ(instr_C(i), 0u);
}

TEST(InstrABC, MaxFields)
{
    Instruction i = make_ABC(0xFF, 0xFF, 0xFF, 0xFF);
    EXPECT_EQ(instr_op(i), 0xFFu);
    EXPECT_EQ(instr_A(i), 0xFFu);
    EXPECT_EQ(instr_B(i), 0xFFu);
    EXPECT_EQ(instr_C(i), 0xFFu);
}

TEST(InstrABC, FieldsIsolated)
{
    // Writing one field must not affect others
    Instruction i = make_ABC(0x11, 0x22, 0x33, 0x44);
    EXPECT_EQ(instr_op(i), 0x11u);
    EXPECT_EQ(instr_A(i), 0x22u);
    EXPECT_EQ(instr_B(i), 0x33u);
    EXPECT_EQ(instr_C(i), 0x44u);
}

TEST(InstrABC, OpCodeADD)
{
    uint8_t op = static_cast<uint8_t>(OpCode::ADD);
    Instruction i = make_ABC(op, 5, 6, 7);
    EXPECT_EQ(static_cast<OpCode>(instr_op(i)), OpCode::ADD);
    EXPECT_EQ(instr_A(i), 5u);
    EXPECT_EQ(instr_B(i), 6u);
    EXPECT_EQ(instr_C(i), 7u);
}

TEST(InstrABC, FieldsDoNotBleed)
{
    Instruction i = make_ABC(0, 0xFF, 0, 0);
    EXPECT_EQ(instr_A(i), 0xFFu);
    EXPECT_EQ(instr_B(i), 0u);
    EXPECT_EQ(instr_C(i), 0u);

    Instruction j = make_ABC(0, 0, 0xFF, 0);
    EXPECT_EQ(instr_A(j), 0u);
    EXPECT_EQ(instr_B(j), 0xFFu);
    EXPECT_EQ(instr_C(j), 0u);

    Instruction k = make_ABC(0, 0, 0, 0xFF);
    EXPECT_EQ(instr_A(k), 0u);
    EXPECT_EQ(instr_B(k), 0u);
    EXPECT_EQ(instr_C(k), 0xFFu);
}

// ============================================================================
// make_ABx round-trip
// ============================================================================

TEST(InstrABx, ZeroFields)
{
    Instruction i = make_ABx(0, 0, 0);
    EXPECT_EQ(instr_op(i), 0u);
    EXPECT_EQ(instr_A(i), 0u);
    EXPECT_EQ(instr_Bx(i), 0u);
}

TEST(InstrABx, MaxBx)
{
    Instruction i = make_ABx(0, 0, 0xFFFF);
    EXPECT_EQ(instr_Bx(i), 0xFFFFu);
}

TEST(InstrABx, MaxA)
{
    Instruction i = make_ABx(0, 0xFF, 0);
    EXPECT_EQ(instr_A(i), 0xFFu);
    EXPECT_EQ(instr_Bx(i), 0u);
}

TEST(InstrABx, BxDoesNotAffectA)
{
    Instruction i = make_ABx(0, 0x77, 0xABCD);
    EXPECT_EQ(instr_A(i), 0x77u);
    EXPECT_EQ(instr_Bx(i), 0xABCDu);
}

TEST(InstrABx, OpCodeLOAD_CONST)
{
    uint8_t op = static_cast<uint8_t>(OpCode::LOAD_CONST);
    Instruction i = make_ABx(op, 3, 1000);
    EXPECT_EQ(static_cast<OpCode>(instr_op(i)), OpCode::LOAD_CONST);
    EXPECT_EQ(instr_A(i), 3u);
    EXPECT_EQ(instr_Bx(i), 1000u);
}

// ============================================================================
// make_AsBx — signed bias encoding
// ============================================================================

TEST(InstrAsBx, ZeroOffset)
{
    // offset 0 → sBx field = 32767 (bias)
    Instruction i = make_AsBx(static_cast<uint8_t>(OpCode::JUMP), 0, 0);
    EXPECT_EQ(instr_sBx(i), 0);
}

TEST(InstrAsBx, PositiveOffset)
{
    Instruction i = make_AsBx(static_cast<uint8_t>(OpCode::JUMP), 0, 100);
    EXPECT_EQ(instr_sBx(i), 100);
}

TEST(InstrAsBx, NegativeOffset)
{
    Instruction i = make_AsBx(static_cast<uint8_t>(OpCode::LOOP), 0, -50);
    EXPECT_EQ(instr_sBx(i), -50);
}

TEST(InstrAsBx, MaxPositiveOffset)
{
    // Maximum positive offset: 32767 (stored as 32767+32767=65534, fits in uint16)
    Instruction i = make_AsBx(static_cast<uint8_t>(OpCode::JUMP), 0, 32767);
    EXPECT_EQ(instr_sBx(i), 32767);
}

TEST(InstrAsBx, MaxNegativeOffset)
{
    // Maximum negative: -32767 (stored as 0)
    Instruction i = make_AsBx(static_cast<uint8_t>(OpCode::LOOP), 0, -32767);
    EXPECT_EQ(instr_sBx(i), -32767);
}

TEST(InstrAsBx, AFieldPreserved)
{
    Instruction i = make_AsBx(static_cast<uint8_t>(OpCode::JUMP_IF_FALSE), 42, 10);
    EXPECT_EQ(instr_A(i), 42u);
    EXPECT_EQ(instr_sBx(i), 10);
}

TEST(InstrAsBx, AFieldMaxWithNegativeOffset)
{
    Instruction i = make_AsBx(static_cast<uint8_t>(OpCode::JUMP_IF_TRUE), 0xFF, -100);
    EXPECT_EQ(instr_A(i), 0xFFu);
    EXPECT_EQ(instr_sBx(i), -100);
}

TEST(InstrAsBx, BiasIsExactly32767)
{
    // By definition: sBx_raw=0 → offset=-32767, sBx_raw=32767 → offset=0
    Instruction i = make_AsBx(0, 0, 0);
    uint16_t raw_sbx = static_cast<uint16_t>(i & 0xFFFF);
    EXPECT_EQ(raw_sbx, static_cast<uint16_t>(32767));
}

// ============================================================================
// REG_NONE and MAX_REGISTERS constants
// ============================================================================

TEST(Constants, REG_NONE_Is255)
{
    EXPECT_EQ(REG_NONE, 255u);
}

TEST(Constants, MAX_REGISTERS_Leaves_Room)
{
    // Must be < 255 to keep REG_NONE as a sentinel
    EXPECT_LT(MAX_REGISTERS, 255u);
    EXPECT_EQ(MAX_REGISTERS, 250u);
}

TEST(Constants, MAX_CONSTANTS_Is16Bit)
{
    EXPECT_EQ(MAX_CONSTANTS, 65535u);
}

// ============================================================================
// opcode_name — every opcode must return a non-empty, non-"???" string
// ============================================================================

TEST(OpCodeMeta, AllOpcodesHaveName)
{
    for (int op = 0; op < static_cast<int>(OpCode::_COUNT); ++op) {
        auto name = opcode_name(static_cast<OpCode>(op));
        EXPECT_FALSE(name.empty()) << "opcode " << op << " has empty name";
        EXPECT_NE(name, "???") << "opcode " << op << " has no name";
    }
}

TEST(OpCodeMeta, KnownNames)
{
    EXPECT_EQ(opcode_name(OpCode::LOAD_NIL), "LOAD_NIL");
    EXPECT_EQ(opcode_name(OpCode::LOAD_TRUE), "LOAD_TRUE");
    EXPECT_EQ(opcode_name(OpCode::LOAD_FALSE), "LOAD_FALSE");
    EXPECT_EQ(opcode_name(OpCode::LOAD_CONST), "LOAD_CONST");
    EXPECT_EQ(opcode_name(OpCode::LOAD_INT), "LOAD_INT");
    EXPECT_EQ(opcode_name(OpCode::LOAD_GLOBAL), "LOAD_GLOBAL");
    EXPECT_EQ(opcode_name(OpCode::STORE_GLOBAL), "STORE_GLOBAL");
    EXPECT_EQ(opcode_name(OpCode::MOVE), "MOVE");
    EXPECT_EQ(opcode_name(OpCode::ADD), "ADD");
    EXPECT_EQ(opcode_name(OpCode::SUB), "SUB");
    EXPECT_EQ(opcode_name(OpCode::MUL), "MUL");
    EXPECT_EQ(opcode_name(OpCode::DIV), "DIV");
    EXPECT_EQ(opcode_name(OpCode::JUMP), "JUMP");
    EXPECT_EQ(opcode_name(OpCode::JUMP_IF_TRUE), "JUMP_IF_TRUE");
    EXPECT_EQ(opcode_name(OpCode::JUMP_IF_FALSE), "JUMP_IF_FALSE");
    EXPECT_EQ(opcode_name(OpCode::LOOP), "LOOP");
    EXPECT_EQ(opcode_name(OpCode::RETURN), "RETURN");
    EXPECT_EQ(opcode_name(OpCode::RETURN_NIL), "RETURN_NIL");
    EXPECT_EQ(opcode_name(OpCode::CLOSURE), "CLOSURE");
    EXPECT_EQ(opcode_name(OpCode::CALL), "CALL");
    EXPECT_EQ(opcode_name(OpCode::IC_CALL), "IC_CALL");
    EXPECT_EQ(opcode_name(OpCode::NOP), "NOP");
    EXPECT_EQ(opcode_name(OpCode::HALT), "HALT");
    EXPECT_EQ(opcode_name(OpCode::FOR_PREP), "FOR_PREP");
    EXPECT_EQ(opcode_name(OpCode::FOR_STEP), "FOR_STEP");
    EXPECT_EQ(opcode_name(OpCode::NEW_LIST), "NEW_LIST");
    EXPECT_EQ(opcode_name(OpCode::LIST_APPEND), "LIST_APPEND");
    EXPECT_EQ(opcode_name(OpCode::LIST_GET), "LIST_GET");
    EXPECT_EQ(opcode_name(OpCode::LIST_SET), "LIST_SET");
    EXPECT_EQ(opcode_name(OpCode::LIST_LEN), "LIST_LEN");
}

// ============================================================================
// opcode_format — every opcode must return a valid format
// ============================================================================

TEST(OpCodeMeta, AllOpcodesHaveFormat)
{
    // We just verify it doesn't crash and returns one of the valid enumerators
    for (int op = 0; op < static_cast<int>(OpCode::_COUNT); ++op) {
        InstrFmt fmt = opcode_format(static_cast<OpCode>(op));
        int fv = static_cast<int>(fmt);
        EXPECT_GE(fv, 0) << "opcode " << op;
        EXPECT_LE(fv, static_cast<int>(InstrFmt::NONE)) << "opcode " << op;
    }
}

TEST(OpCodeMeta, JumpFormatIsAsBx)
{
    EXPECT_EQ(opcode_format(OpCode::JUMP), InstrFmt::AsBx);
    EXPECT_EQ(opcode_format(OpCode::JUMP_IF_TRUE), InstrFmt::AsBx);
    EXPECT_EQ(opcode_format(OpCode::JUMP_IF_FALSE), InstrFmt::AsBx);
    EXPECT_EQ(opcode_format(OpCode::LOOP), InstrFmt::AsBx);
    EXPECT_EQ(opcode_format(OpCode::FOR_PREP), InstrFmt::AsBx);
    EXPECT_EQ(opcode_format(OpCode::FOR_STEP), InstrFmt::AsBx);
}

TEST(OpCodeMeta, LoadFormatIsABx)
{
    EXPECT_EQ(opcode_format(OpCode::LOAD_CONST), InstrFmt::ABx);
    EXPECT_EQ(opcode_format(OpCode::LOAD_INT), InstrFmt::ABx);
    EXPECT_EQ(opcode_format(OpCode::LOAD_GLOBAL), InstrFmt::ABx);
    EXPECT_EQ(opcode_format(OpCode::STORE_GLOBAL), InstrFmt::ABx);
    EXPECT_EQ(opcode_format(OpCode::CLOSURE), InstrFmt::ABx);
}

TEST(OpCodeMeta, ArithmeticFormatIsABC)
{
    EXPECT_EQ(opcode_format(OpCode::ADD), InstrFmt::ABC);
    EXPECT_EQ(opcode_format(OpCode::SUB), InstrFmt::ABC);
    EXPECT_EQ(opcode_format(OpCode::MUL), InstrFmt::ABC);
    EXPECT_EQ(opcode_format(OpCode::DIV), InstrFmt::ABC);
    EXPECT_EQ(opcode_format(OpCode::MOD), InstrFmt::ABC);
    EXPECT_EQ(opcode_format(OpCode::POW), InstrFmt::ABC);
}

TEST(OpCodeMeta, NoOpFormatIsNONE)
{
    EXPECT_EQ(opcode_format(OpCode::NOP), InstrFmt::NONE);
    EXPECT_EQ(opcode_format(OpCode::HALT), InstrFmt::NONE);
    EXPECT_EQ(opcode_format(OpCode::RETURN_NIL), InstrFmt::NONE);
}
