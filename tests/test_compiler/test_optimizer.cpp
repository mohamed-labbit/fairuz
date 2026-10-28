//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
// SPDX-License-Identifier: MIT
//===----------------------------------------------------------------------===//

#include "../test_interpreter/test_interpreter_common.h"
#include "Compiler/optimizer.hpp"
#include "Interpreter/integer.hpp"
#include "Parser/ast_builder.hpp"

#include <cmath>
#include <limits>

namespace {

using Kind = AST::ExprKind;
using Builder = AST::ASTBuilder;

AST::IntLiteralExpr* integer_literal(i64 value) { return Builder::make_literal_int(value, { }); }

AST::IntLiteralExpr* large_literal(char const* value, int base = 10)
{
    auto* literal = integer_literal(0);
    literal->large_literal = value;
    literal->literal_base = base;
    return literal;
}

Value literal_value(AST::ExprPtr expression, GarbageCollector& gc)
{
    if (AST::is_literal_bool(expression)) return Value::from_bool(AST::as_literal_bool(expression)->value);
    if (AST::is_literal_float(expression)) return Value::from_real(AST::as_literal_float(expression)->value);
    auto* literal = AST::as_literal_int(expression);
    return literal->large_literal.empty()
        ? Value::from_int(literal->value, gc)
        : integer::finish(integer::parse(literal->large_literal, literal->literal_base), gc);
}

void expect_matches_vm(Kind kind, OpCode opcode, AST::ExprPtr lhs, AST::ExprPtr rhs)
{
    SourceLocation loc { 7, 3, 5, 42 };
    auto* expression = Builder::make_binary(kind, lhs, rhs, loc);
    auto folded = Optimizer::try_constant_folding(expression);
    ASSERT_TRUE(folded.has_value());
    ASSERT_NE(*folded, expression);
    EXPECT_EQ((*folded)->get_location().line, loc.line);
    EXPECT_EQ((*folded)->get_location().column, loc.column);
    EXPECT_EQ((*folded)->get_location().length, loc.length);
    EXPECT_EQ((*folded)->get_location().offset, loc.offset);
    EXPECT_EQ(expression->lhs, lhs);
    EXPECT_EQ(expression->rhs, rhs);

    GarbageCollector gc;
    VMRunner runner;
    CB code;
    auto left = code.ch->add_constant(literal_value(lhs, gc));
    auto right = code.ch->add_constant(literal_value(rhs, gc));
    code.regs(3)
        .slot()
        .ABx(OpCode::LOAD_CONST, 0, left.value)
        .ABx(OpCode::LOAD_CONST, 1, right.value)
        .ABC(opcode, 2, 0, 1)
        .nop()
        .ret(2);
    Value runtime = run_success(runner.run(code));
    Value constant = literal_value(*folded, gc);
    if (runtime.is_int()) {
        ASSERT_TRUE(constant.is_int());
        EXPECT_EQ(integer::compare(runtime, constant), 0);
    } else if (runtime.is_double()) {
        ASSERT_TRUE(constant.is_double());
        if (std::isnan(runtime.as_double())) EXPECT_TRUE(std::isnan(constant.as_double()));
        else {
            EXPECT_EQ(runtime.as_double(), constant.as_double());
            EXPECT_EQ(std::signbit(runtime.as_double()), std::signbit(constant.as_double()));
        }
    } else {
        ASSERT_TRUE(runtime.is_bool());
        ASSERT_TRUE(constant.is_bool());
        EXPECT_EQ(runtime.as_bool(), constant.as_bool());
    }
}

} // namespace

TEST(Optimizer, IntegerArithmeticMatchesVm)
{
    struct Operation {
        Kind kind;
        OpCode opcode;
    };
    for (auto operation : { Operation { Kind::OP_ADD, OpCode::OP_ADD }, Operation { Kind::OP_SUB, OpCode::OP_SUB },
             Operation { Kind::OP_MUL, OpCode::OP_MUL }, Operation { Kind::OP_DIV, OpCode::OP_DIV },
             Operation { Kind::OP_MOD, OpCode::OP_MOD }, Operation { Kind::OP_BITAND, OpCode::OP_BITAND },
             Operation { Kind::OP_BITOR, OpCode::OP_BITOR }, Operation { Kind::OP_BITXOR, OpCode::OP_BITXOR } }) {
        for (i64 lhs : { i64 { 0 }, i64 { -7 }, i64 { 12 }, INT64_MIN, INT64_MAX }) {
            for (i64 rhs : { i64 { -1 }, i64 { 2 }, i64 { 3 } })
                expect_matches_vm(operation.kind, operation.opcode, integer_literal(lhs), integer_literal(rhs));
        }
    }
}

TEST(Optimizer, FloatingAndMixedArithmeticMatchesVm)
{
    for (auto [kind, opcode] : { std::pair { Kind::OP_ADD, OpCode::OP_ADD }, std::pair { Kind::OP_SUB, OpCode::OP_SUB },
             std::pair { Kind::OP_MUL, OpCode::OP_MUL }, std::pair { Kind::OP_DIV, OpCode::OP_DIV },
             std::pair { Kind::OP_MOD, OpCode::OP_MOD }, std::pair { Kind::OP_POW, OpCode::OP_POW } }) {
        expect_matches_vm(kind, opcode, integer_literal(7), Builder::make_literal_float(2.5, { }));
        expect_matches_vm(kind, opcode, Builder::make_literal_float(-7.5, { }), integer_literal(2));
        expect_matches_vm(kind, opcode, Builder::make_literal_float(1.5, { }), Builder::make_literal_float(2.5, { }));
    }
    expect_matches_vm(Kind::OP_MUL, OpCode::OP_MUL, Builder::make_literal_float(-0.0, { }), integer_literal(2));
}

TEST(Optimizer, LargeIntegersSurviveTemporaryCollector)
{
    expect_matches_vm(Kind::OP_ADD, OpCode::OP_ADD, large_literal("0xffffffffffffffffffff", 16), integer_literal(1));
    expect_matches_vm(Kind::OP_DIV, OpCode::OP_DIV, large_literal("18446744073709551616"), integer_literal(2));
    expect_matches_vm(Kind::OP_MUL, OpCode::OP_MUL, integer_literal(INT64_MIN), integer_literal(-1));
}

TEST(Optimizer, IntegerPowersAndShiftsMatchVm)
{
    expect_matches_vm(Kind::OP_POW, OpCode::OP_POW, integer_literal(2), integer_literal(100));
    expect_matches_vm(Kind::OP_POW, OpCode::OP_POW, integer_literal(2), integer_literal(-3));
    expect_matches_vm(Kind::OP_POW, OpCode::OP_POW, integer_literal(0), integer_literal(0));
    expect_matches_vm(Kind::OP_LSHIFT, OpCode::OP_LSHIFT_REG, integer_literal(-3), integer_literal(100));
    expect_matches_vm(Kind::OP_RSHIFT, OpCode::OP_RSHIFT_REG, integer_literal(-7), integer_literal(1));
    expect_matches_vm(Kind::OP_RSHIFT, OpCode::OP_RSHIFT_REG, integer_literal(-7), integer_literal(INT64_MAX));
}

TEST(Optimizer, ExactNumericComparisonsAndNanMatchVm)
{
    for (auto [kind, opcode] : { std::pair { Kind::OP_EQ, OpCode::OP_EQ }, std::pair { Kind::OP_NEQ, OpCode::OP_NEQ },
             std::pair { Kind::OP_LT, OpCode::OP_LT }, std::pair { Kind::OP_LTE, OpCode::OP_LTE },
             std::pair { Kind::OP_GT, OpCode::OP_GT }, std::pair { Kind::OP_GTE, OpCode::OP_GTE } }) {
        expect_matches_vm(kind, opcode, integer_literal(INT64_MAX), Builder::make_literal_float(0x1p63, { }));
        expect_matches_vm(kind, opcode, integer_literal(9007199254740993LL), Builder::make_literal_float(0x1p53, { }));
        expect_matches_vm(kind, opcode, Builder::make_literal_float(std::numeric_limits<double>::quiet_NaN(), { }),
            integer_literal(1));
    }
}

TEST(Optimizer, LeavesFailingAndUnsupportedExpressionsUntouched)
{
    for (Kind kind : { Kind::OP_DIV, Kind::OP_MOD }) {
        for (auto* zero : { static_cast<AST::ExprPtr>(integer_literal(0)),
                 static_cast<AST::ExprPtr>(Builder::make_literal_float(-0.0, { })) }) {
            auto* expression = Builder::make_binary(kind, integer_literal(1), zero, { });
            auto* original = expression->clone();
            EXPECT_FALSE(Optimizer::try_constant_folding(expression));
            EXPECT_TRUE(expression->equals(original));
        }
    }
    EXPECT_FALSE(Optimizer::try_constant_folding(nullptr));
    EXPECT_FALSE(Optimizer::try_constant_folding(Builder::make_binary(Kind::OP_ADD,
        Builder::make_call(Builder::make_identifier("side_effect", { }), { }, { }), integer_literal(0), { })));
    EXPECT_FALSE(Optimizer::try_constant_folding(
        Builder::make_binary(Kind::OP_AND, integer_literal(0), integer_literal(1), { })));
    EXPECT_FALSE(Optimizer::try_constant_folding(
        Builder::make_binary(Kind::OP_BITAND, integer_literal(1), Builder::make_literal_float(2.0, { }), { })));
    EXPECT_FALSE(Optimizer::try_constant_folding(
        Builder::make_binary(Kind::OP_LSHIFT, integer_literal(1), integer_literal(-1), { })));
}

TEST(Optimizer, BoundsCompileTimeWork)
{
    EXPECT_FALSE(Optimizer::try_constant_folding(
        Builder::make_binary(Kind::OP_POW, integer_literal(2), integer_literal(INT64_MAX), { })));
    EXPECT_FALSE(Optimizer::try_constant_folding(
        Builder::make_binary(Kind::OP_LSHIFT, integer_literal(1), integer_literal(INT64_MAX), { })));
    std::string huge(2000, '9');
    EXPECT_FALSE(Optimizer::try_constant_folding(
        Builder::make_binary(Kind::OP_ADD, large_literal(huge.c_str()), integer_literal(1), { })));
}
