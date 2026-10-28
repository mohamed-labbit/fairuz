// SPDX-License-Identifier: MIT
#include "test_compiler_common.h"

namespace {

bool contains(Chunk const* chunk, OpCode opcode)
{
    return std::any_of(chunk->code.begin(), chunk->code.end(),
        [opcode](InstrType instruction) { return instr_op(instruction) == opcode; });
}

} // namespace

TEST(CompilerFolding, NestedConstantsEmitOneValueWithoutArithmeticOrCaches)
{
    Chunk* chunk = compile_ok_local({ "ارجع (2 + 3) * (4 + 5)\n" });
    EXPECT_FALSE(contains(chunk, OpCode::OP_ADD));
    EXPECT_FALSE(contains(chunk, OpCode::OP_MUL));
    EXPECT_TRUE(chunk->ic_slots.empty());
    ASSERT_EQ(chunk->code.size(), 2u);
    EXPECT_EQ(instr_op(chunk->code[0]), OpCode::LOAD_INT);
    EXPECT_EQ(instr_Bx(chunk->code[0]), load_int_bx(45));
}

TEST(CompilerFolding, MixedConstantsKeepFloatingPointResults)
{
    Chunk* chunk = compile_ok_local({ "ارجع (7 + 2.5) * (8 / 4)\n" });
    EXPECT_FALSE(contains(chunk, OpCode::OP_ADD));
    EXPECT_FALSE(contains(chunk, OpCode::OP_MUL));
    EXPECT_FALSE(contains(chunk, OpCode::OP_DIV));
    ASSERT_EQ(chunk->constants.size(), 1u);
    ASSERT_TRUE(chunk->constants[0].is_double());
    EXPECT_EQ(chunk->constants[0].as_double(), 19.0);
}

TEST(CompilerFolding, LargeLiteralAdditionEmitsBigIntegerLoad)
{
    Chunk* chunk = compile_ok_local({ "ارجع 9223372036854775807 + 1\n" });
    EXPECT_FALSE(contains(chunk, OpCode::OP_ADD));
    EXPECT_TRUE(contains(chunk, OpCode::LOAD_BIG_INT));
    ASSERT_EQ(chunk->big_ints.size(), 1u);
    EXPECT_EQ(chunk->big_ints[0], integer::parse("9223372036854775808", 10));
}

TEST(CompilerFolding, DivisionByZeroRemainsARuntimeErrorAtItsSource)
{
    test_source::Program source("دالة فحص():\n    ارجع (2 + 3) / (4 - 4)\nفحص()\n");
    auto* chunk = run_success(source.compile());
    run_success(Compiler::compile_all(chunk));
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_TRUE(contains(chunk->functions[0], OpCode::OP_DIV));
    VM vm;
    auto error = run_failed(vm.run(chunk));
    EXPECT_EQ(error.errc, ErrorCode::DIVISION_BY_ZERO);
    EXPECT_EQ(error.loc.line, 2u);
}

TEST(CompilerFolding, PreservesShortCircuitingAndSideEffects)
{
    EXPECT_EQ(test_source::run("ق := [10]\n"
                               "دالة اثر(ن):\n"
                               "    ق[0] := 100\n"
                               "    ارجع ن\n"
                               "تاكد((خطا و (1 / 0)) = خطا)\n"
                               "تاكد((صحيح او (1 / 0)) = صحيح)\n"
                               "تاكد(ق[0] + اثر(7) = 17)\n"
                               "تاكد((2 + 3) + اثر(7) = 12)\n"
                               "تاكد(ق[0] = 100)\n"),
        ExitStatus::SUCCESS);
}
