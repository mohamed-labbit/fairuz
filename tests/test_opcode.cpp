#include "../fairuz/fopcode.hpp"

#include <gtest/gtest.h>

using namespace fairuz::runtime;

TEST(InstrABC, OpCodeADD)
{
    u32 i = make_ABC(OpCode::OP_ADD, 5, 6, 7);
    EXPECT_EQ(static_cast<OpCode>(instr_op(i)), OpCode::OP_ADD);
    EXPECT_EQ(instr_A(i), 5u);
    EXPECT_EQ(instr_B(i), 6u);
    EXPECT_EQ(instr_C(i), 7u);
}

TEST(InstrABC, FieldsDoNotBleed)
{
    u32 i = make_ABC(OpCode(0), 0xFF, 0, 0);
    EXPECT_EQ(instr_A(i), 0xFFu);
    EXPECT_EQ(instr_B(i), 0u);
    EXPECT_EQ(instr_C(i), 0u);

    u32 j = make_ABC(OpCode(0), 0, 0xFF, 0);
    EXPECT_EQ(instr_A(j), 0u);
    EXPECT_EQ(instr_B(j), 0xFFu);
    EXPECT_EQ(instr_C(j), 0u);

    u32 k = make_ABC(OpCode(0), 0, 0, 0xFF);
    EXPECT_EQ(instr_A(k), 0u);
    EXPECT_EQ(instr_B(k), 0u);
    EXPECT_EQ(instr_C(k), 0xFFu);
}

TEST(InstrABx, ZeroFields)
{
    u32 i = make_ABx(OpCode(0), 0, 0);
    EXPECT_EQ((u8)instr_op(i), 0u);
    EXPECT_EQ(instr_A(i), 0u);
    EXPECT_EQ(instr_Bx(i), 0u);
}

TEST(InstrABx, MaxBx)
{
    u32 i = make_ABx(OpCode(0), 0, 0xFFFF);
    EXPECT_EQ(instr_Bx(i), 0xFFFFu);
}

TEST(InstrABx, MaxA)
{
    u32 i = make_ABx(OpCode(0), 0xFF, 0);
    EXPECT_EQ(instr_A(i), 0xFFu);
    EXPECT_EQ(instr_Bx(i), 0u);
}

TEST(InstrABx, BxDoesNotAffectA)
{
    u32 i = make_ABx(OpCode(0), 0x77, 0xABCD);
    EXPECT_EQ(instr_A(i), 0x77u);
    EXPECT_EQ(instr_Bx(i), 0xABCDu);
}

TEST(InstrABx, OpCodeLOAD_CONST)
{
    u32 i = make_ABx(OpCode::LOAD_CONST, 3, 1000);
    EXPECT_EQ(static_cast<OpCode>(instr_op(i)), OpCode::LOAD_CONST);
    EXPECT_EQ(instr_A(i), 3u);
    EXPECT_EQ(instr_Bx(i), 1000u);
}

TEST(InstrAsBx, ZeroOffset)
{
    // offset 0 → sBx field = 32767 (bias)
    u32 i = make_AsBx((OpCode::JUMP), 0, 0);
    EXPECT_EQ(instr_sBx(i), 0);
}

TEST(InstrAsBx, PositiveOffset)
{
    u32 i = make_AsBx((OpCode::JUMP), 0, 100);
    EXPECT_EQ(instr_sBx(i), 100);
}

TEST(InstrAsBx, NegativeOffset)
{
    u32 i = make_AsBx((OpCode::LOOP), 0, -50);
    EXPECT_EQ(instr_sBx(i), -50);
}

TEST(InstrAsBx, MaxPositiveOffset)
{
    u32 i = make_AsBx((OpCode::JUMP), 0, 32767);
    EXPECT_EQ(instr_sBx(i), 32767);
}

TEST(InstrAsBx, MaxNegativeOffset)
{
    u32 i = make_AsBx((OpCode::LOOP), 0, -32767);
    EXPECT_EQ(instr_sBx(i), -32767);
}

TEST(InstrAsBx, AFieldPreserved)
{
    u32 i = make_AsBx((OpCode::JUMP_IF_FALSE), 42, 10);
    EXPECT_EQ(instr_A(i), 42u);
    EXPECT_EQ(instr_sBx(i), 10);
}

TEST(InstrAsBx, AFieldMaxWithNegativeOffset)
{
    u32 i = make_AsBx((OpCode::JUMP_IF_TRUE), 0xFF, -100);
    EXPECT_EQ(instr_A(i), 0xFFu);
    EXPECT_EQ(instr_sBx(i), -100);
}

TEST(InstrAsBx, BiasIsExactly32767)
{
    u32 i = make_AsBx(OpCode(0), 0, 0);
    u16 raw_sbx = static_cast<u16>(i & 0xFFFF);
    EXPECT_EQ(raw_sbx, static_cast<u16>(32767));
}

TEST(Constants, REG_NONE_Is255) { EXPECT_EQ(REG_NONE, 255u); }

TEST(Constants, MAX_REGISTERS_Leaves_Room)
{
    EXPECT_LT(MAX_REGS, 255u);
    EXPECT_EQ(MAX_REGS, 250u);
}

TEST(Constants, MAX_CONSTANTS_Is16Bit) { EXPECT_EQ(MAX_CONSTANTS, 65535u); }

TEST(OpCodeMeta, AllOpcodesHaveName)
{
    for (int op = 0; op < static_cast<int>(OpCode::_COUNT); op++) {
        auto m_name = opcode_name(static_cast<OpCode>(op));
        EXPECT_FALSE(m_name.empty()) << "opcode " << op << " has empty name";
        EXPECT_NE(m_name, "???") << "opcode " << op << " has no name";
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
    EXPECT_EQ(opcode_name(OpCode::IMPORT_MODULE), "IMPORT_MODULE");
    EXPECT_EQ(opcode_name(OpCode::MOVE), "MOVE");
    EXPECT_EQ(opcode_name(OpCode::OP_ADD), "OP_ADD");
    EXPECT_EQ(opcode_name(OpCode::OP_SUB), "OP_SUB");
    EXPECT_EQ(opcode_name(OpCode::OP_MUL), "OP_MUL");
    EXPECT_EQ(opcode_name(OpCode::OP_DIV), "OP_DIV");
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
    EXPECT_EQ(opcode_name(OpCode::LIST_NEW), "LIST_NEW");
    EXPECT_EQ(opcode_name(OpCode::LIST_APPEND), "LIST_APPEND");
    EXPECT_EQ(opcode_name(OpCode::LIST_GET), "LIST_GET");
    EXPECT_EQ(opcode_name(OpCode::LIST_SET), "LIST_SET");
    EXPECT_EQ(opcode_name(OpCode::LIST_LEN), "LIST_LEN");
}

TEST(OpCodeMeta, AllOpcodesHaveFormat)
{
    for (int op = 0; op < static_cast<int>(OpCode::_COUNT); op++) {
        InstrFormat fmt = opcode_format(static_cast<OpCode>(op));
        int fv = static_cast<int>(fmt);
        EXPECT_GE(fv, 0) << "opcode " << op;
        EXPECT_LE(fv, static_cast<int>(InstrFormat::NONE)) << "opcode " << op;
    }
}

TEST(OpCodeMeta, JumpFormatIsAsBx)
{
    EXPECT_EQ(opcode_format(OpCode::JUMP), InstrFormat::AsBx);
    EXPECT_EQ(opcode_format(OpCode::JUMP_IF_TRUE), InstrFormat::AsBx);
    EXPECT_EQ(opcode_format(OpCode::JUMP_IF_FALSE), InstrFormat::AsBx);
    EXPECT_EQ(opcode_format(OpCode::LOOP), InstrFormat::AsBx);
    EXPECT_EQ(opcode_format(OpCode::FOR_PREP), InstrFormat::AsBx);
    EXPECT_EQ(opcode_format(OpCode::FOR_STEP), InstrFormat::AsBx);
}

TEST(OpCodeMeta, LoadFormatIsABx)
{
    EXPECT_EQ(opcode_format(OpCode::LOAD_CONST), InstrFormat::ABx);
    EXPECT_EQ(opcode_format(OpCode::LOAD_INT), InstrFormat::ABx);
    EXPECT_EQ(opcode_format(OpCode::LOAD_GLOBAL), InstrFormat::ABx);
    EXPECT_EQ(opcode_format(OpCode::STORE_GLOBAL), InstrFormat::ABx);
    EXPECT_EQ(opcode_format(OpCode::IMPORT_MODULE), InstrFormat::ABx);
    EXPECT_EQ(opcode_format(OpCode::CLOSURE), InstrFormat::ABx);
}

TEST(OpCodeMeta, ArithmeticFormatIsABC)
{
    EXPECT_EQ(opcode_format(OpCode::OP_ADD), InstrFormat::ABC);
    EXPECT_EQ(opcode_format(OpCode::OP_SUB), InstrFormat::ABC);
    EXPECT_EQ(opcode_format(OpCode::OP_MUL), InstrFormat::ABC);
    EXPECT_EQ(opcode_format(OpCode::OP_DIV), InstrFormat::ABC);
    EXPECT_EQ(opcode_format(OpCode::OP_MOD), InstrFormat::ABC);
    EXPECT_EQ(opcode_format(OpCode::OP_POW), InstrFormat::ABC);
}

TEST(OpCodeMeta, NoOpFormatIsNONE)
{
    EXPECT_EQ(opcode_format(OpCode::NOP), InstrFormat::NONE);
    EXPECT_EQ(opcode_format(OpCode::HALT), InstrFormat::NONE);
    EXPECT_EQ(opcode_format(OpCode::RETURN_NIL), InstrFormat::NONE);
}
