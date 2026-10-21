//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies AST compilation into bytecode.
//===----------------------------------------------------------------------===//

#include "Compiler/compiler.hpp"
#include "Compiler/opcode.hpp"
#include "Diag/diagnostic.hpp"
#include "Interpreter/gc.hpp"
#include "Macros.hpp"
#include "Parser/ast.hpp"
#include "Parser/ast_printer.hpp"
#include "Support/string.hpp"
#include "test_common.h"
#include "test_config.h"
#include "test_source_program.hpp"

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>

using namespace fairuz::runtime;
using namespace fairuz;

class BytecodeChecker {
public:
    explicit BytecodeChecker(Chunk const& chunk)
        : chunk_(chunk)
        , pos_(0)
    {
    }

    BytecodeChecker& next(StringRef label = "")
    {
        label_ = std::move(label);
        EXPECT_LT(pos_, chunk_.code.size()) << "ran off end of code at step \"" << label_ << "\"";
        if (pos_ < chunk_.code.size()) {
            cur_ = chunk_.code[pos_];
            pos_++;
        }
        return *this;
    }

    BytecodeChecker& op(OpCode expected)
    {
        EXPECT_EQ(instr_op(cur_), expected) << at() << "  expected op=" << opcode_name(expected)
                                            << "  got=" << opcode_name(static_cast<OpCode>(instr_op(cur_)));
        return *this;
    }
    BytecodeChecker& A(u8 expected)
    {
        EXPECT_EQ(instr_A(cur_), expected) << at() << "  field A";
        return *this;
    }
    BytecodeChecker& B(u8 expected)
    {
        EXPECT_EQ(instr_B(cur_), expected) << at() << "  field B";
        return *this;
    }
    BytecodeChecker& C(u8 expected)
    {
        EXPECT_EQ(instr_C(cur_), expected) << at() << "  field C";
        return *this;
    }
    BytecodeChecker& Bx(u16 expected)
    {
        EXPECT_EQ(instr_Bx(cur_), expected) << at() << "  field Bx";
        return *this;
    }
    BytecodeChecker& s_bx(int expected)
    {
        EXPECT_EQ(instr_sBx(cur_), expected) << at() << "  field sBx";
        return *this;
    }

    BytecodeChecker& done()
    {
        EXPECT_EQ(pos_, chunk_.code.size()) << "expected end of code at instruction " << pos_ << " but "
                                            << (chunk_.code.size() - pos_) << " more remain";
        return *this;
    }

    u32 current_index() const { return pos_ - 1; }
    u32 next_index() const { return pos_; }

private:
    StringRef at() const
    {
        std::ostringstream ss;
        ss << "[instr " << (pos_ - 1) << " \"" << label_ << "\"]";
        return ss.str().data();
    }

    Chunk const& chunk_;
    u32 pos_;
    u32 cur_ = 0;
    StringRef label_;
};

static Chunk* compile_ok(std::string const& source)
{
    Chunk* chunk = test_source::compile(source);
    EXPECT_FALSE(Compiler::compile_all(chunk).has_error());
    return chunk;
}

static Chunk* compile_ok(Array<std::string> const& source) { return compile_ok(test_source::join(source)); }

static Chunk* compile_ok(std::initializer_list<std::string> source) { return compile_ok(Array<std::string>(source)); }

static Chunk* compile_ok_local(Array<std::string> const& source)
{
    auto chunk = compile_ok("دالة دالة_الفحص():\n" + test_source::indent(test_source::join(source)));
    EXPECT_EQ(chunk->functions.size(), 1u);
    return chunk->functions[0];
}

static Chunk* compile_fail(std::string const& source)
{
    test_source::Program program(source);
    run_failed(program.compile());
    return nullptr;
}

static Chunk* compile_fail(Array<std::string> const& source) { return compile_fail(test_source::join(source)); }

static void dump(Chunk const* chunk) { chunk->disassemble(); }

static u16 load_int_bx(i64 i) { return static_cast<u16>(i + 32767); }

TEST(CompilerLiteral, NilExpression)
{
    Chunk* chunk = compile_ok("عدم\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_NIL").op(OpCode::LOAD_NIL).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, TrueLiteral)
{
    Chunk* chunk = compile_ok("صحيح\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_TRUE").op(OpCode::LOAD_TRUE).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, FalseLiteral)
{
    Chunk* chunk = compile_ok("خطا\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_FALSE").op(OpCode::LOAD_FALSE).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, SmallIntegerUsesLoadInt)
{
    Chunk* chunk = compile_ok("42\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(42));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_TRUE(chunk->constants.empty());
}

TEST(CompilerLiteral, VeryBigPositiveIntegerUsesLoadBigInt)
{
#if FA_USE_NANBOX
    Chunk* chunk = compile_ok(std::to_string(static_cast<i64>(Value::int_max() + 2)) + "\n");
#else
    Chunk* chunk = compile_ok("9223372036854775808\n");
#endif
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_BIG_INT").op(OpCode::LOAD_BIG_INT).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_FALSE(chunk->big_ints.empty());
}

TEST(CompilerLiteral, VeryBigNegativeIntegerUsesLoadBigInt)
{
#if FA_USE_NANBOX
    Chunk* chunk = compile_ok(std::to_string(static_cast<i64>(Value::int_min() - 2)) + "\n");
#else
    Chunk* chunk = compile_ok("-9223372036854775809\n");
#endif
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_BIG_INT").op(OpCode::LOAD_BIG_INT).A(0);
    bc.next("OP_NEG").op(OpCode::OP_NEG).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_FALSE(chunk->big_ints.empty());
}

TEST(CompilerLiteral, NegativeSmallIntegerUsesLoadIntAndNegation)
{
    Chunk* chunk = compile_ok("-100\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).Bx(load_int_bx(100));
    bc.next("OP_NEG").op(OpCode::OP_NEG).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, ZeroUsesLoadInt)
{
    Chunk* chunk = compile_ok("0\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).Bx(load_int_bx(0));
}

TEST(CompilerLiteral, LargeIntegerUsesConstantPool)
{
    Chunk* chunk = compile_ok("100000\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].is_int());
    EXPECT_EQ(chunk->constants[0].as_int(), 100000);
}

TEST(CompilerLiteral, FloatUsesConstantPool)
{
    Chunk* chunk = compile_ok("3.14\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].is_double());
    EXPECT_NEAR(chunk->constants[0].as_double(), 3.14, 1e-9);
}

TEST(CompilerLiteral, StringUsesConstantPool)
{
    Chunk* chunk = compile_ok("\"hello\"\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].is_string());
    EXPECT_EQ(chunk->constants[0].as_string()->str, "hello");
}

TEST(CompilerLiteral, StringsDeduplicated)
{
    Array<std::string> stmts;
    stmts.push("\"dup\"\n");
    stmts.push("\"dup\"\n");
    Chunk* chunk = compile_ok(test_source::join(std::move(stmts), ""));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    long string_constants
        = std::count_if(chunk->constants.begin(), chunk->constants.end(), [](Value const& v) { return v.is_string(); });
    EXPECT_EQ(string_constants, 1);
}

TEST(CompilerVar, LocalDeclaration)
{
    Chunk* ch = compile_ok_local({ "س := 5\n" });
    BytecodeChecker bc(*ch);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(5));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    EXPECT_EQ(ch->local_count, 1u);
}

TEST(CompilerVar, TwoLocalsUseConsecutiveRegisters)
{
    Chunk* ch = compile_ok_local({
        "س := 1\n",
        "ص := 2\n",
    });

    BytecodeChecker bc(*ch);
    bc.next("LOAD_INT x").op(OpCode::LOAD_INT).A(0);
    bc.next("LOAD_INT y").op(OpCode::LOAD_INT).A(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(ch->local_count, 2);
}

TEST(CompilerVar, LocalAssignmentWritesBackToSameRegister)
{
    Chunk* ch = compile_ok_local({
        "س := 1\n",
        "س := 2\n",
    });

    BytecodeChecker bc(*ch);
    bc.next("decl_stmt x=1").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.next("assign x=2").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(2));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerVar, GlobalLoadAndStore)
{
    Chunk* chunk = compile_ok({ "عام := 7\n", "عام\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("RHS").op(OpCode::LOAD_INT).Bx(load_int_bx(7));
    bc.next("STORE_GLOBAL").op(OpCode::STORE_GLOBAL);
    bc.next("LOAD_GLOBAL").op(OpCode::LOAD_GLOBAL);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    bool found = false;
    for (auto& v : chunk->constants) {
        if (v.is_string() && v.as_string()->str == "عام") found = true;
    }
    EXPECT_TRUE(found) << "global ident 'عام' not interned into constant pool";
}

TEST(CompilerUnary, NegateVariable)
{
    Chunk* ch = compile_ok_local({
        "س := 5\n",
        "(-س)\n",
    });

    BytecodeChecker bc(*ch);
    bc.next("LOAD_INT x").op(OpCode::LOAD_INT).A(0);
    bc.next("OP_NEG").op(OpCode::OP_NEG).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, NotVariable)
{
    Chunk* ch = compile_ok_local({
        "ب := صحيح\n",
        "(ليس ب)\n",
    });

    BytecodeChecker bc(*ch);
    bc.next("LOAD_TRUE b").op(OpCode::LOAD_TRUE).A(0);
    bc.next("OP_NOT").op(OpCode::OP_NOT).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, BitwiseNotVariable)
{
    Chunk* ch = compile_ok_local({
        (std::string("ن := ") + std::to_string(0xFF) + "\n"),
        "(~ن)\n",
    });

    BytecodeChecker bc(*ch);
    bc.next("decl_stmt").op(OpCode::LOAD_INT).A(0);
    bc.next("OP_BITNOT").op(OpCode::OP_BITNOT).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, AddTwoLocals)
{
    Chunk* ch = compile_ok_local({ "ا := 1\n", "ب := 2\n", "(ا + ب)\n" });
    BytecodeChecker bc(*ch);
    bc.next("decl_stmt a").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.next("decl_stmt b").op(OpCode::LOAD_INT).A(1).Bx(load_int_bx(2));
    bc.next("OP_ADD").op(OpCode::OP_ADD).A(2).B(0).C(1);
    bc.next("NOP ic").op(OpCode::NOP).A(0); // IC slot 0
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(ch->ic_slots.size(), 1u);
}

TEST(CompilerBinary, ICSlotAllocatedPerBinaryOp)
{
    Chunk* chunk = compile_ok({ "س := 1\n", "ص := 2\n", "(س + ص)\n", "(س * ص)\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_EQ(chunk->ic_slots.size(), 2u);
}

TEST(CompilerBinary, LogicalAndShortCircuit)
{
    Chunk* ch = compile_ok_local({
        "ا := صحيح\n",
        "ب := خطا\n",
        "(ا و ب)\n",
    });

    BytecodeChecker bc(*ch);
    bc.next("decl_stmt a").op(OpCode::LOAD_TRUE).A(0);
    bc.next("decl_stmt b").op(OpCode::LOAD_FALSE).A(1);
    u32 jif_idx;
    bc.next("LHS into temp").op(OpCode::MOVE).A(2).B(0);
    (void)jif_idx;
    bool found_jif = false, found_b = false;
    for (auto& instr : ch->code) {
        if (instr_op(instr) == OpCode::JUMP_IF_FALSE) found_jif = true;
        if (found_jif && (instr_op(instr) == OpCode::LOAD_FALSE || instr_op(instr) == OpCode::MOVE)) found_b = true;
    }
    EXPECT_TRUE(found_jif) << "expected JUMP_IF_FALSE for && short-circuit";
    EXPECT_TRUE(found_b) << "expected RHS load after JUMP_IF_FALSE";
}

TEST(CompilerBinary, LogicalOrShortCircuit)
{
    Chunk* chunk = compile_ok_local({
        "ا := خطا\n",
        "ب := صحيح\n",
        "(ا او ب)\n",
    });

    bool found_jit = false;
    for (auto& instr : chunk->code) {
        if (instr_op(instr) == OpCode::JUMP_IF_TRUE) found_jit = true;
    }
    EXPECT_TRUE(found_jit) << "expected JUMP_IF_TRUE for || short-circuit";
}

TEST(CompilerIf, SimpleIfNoElse)
{
    Chunk* chunk = compile_ok("اذا س:\n"
                              "    ص := 1\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP_IF_FALSE) jif_pos = i;
    }
    ASSERT_GE(jif_pos, 0) << "expected JUMP_IF_FALSE";
    int m_target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    EXPECT_EQ(instr_op(chunk->code[m_target]), OpCode::RETURN_NIL);
}

TEST(CompilerIf, IfElse)
{
    Chunk* chunk = compile_ok("اذا س:\n"
                              "    ا := 1\n"
                              "غيره:\n"
                              "    ب := 2\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    bool has_jif = false, has_jmp = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::JUMP_IF_FALSE) has_jif = true;
        if (instr_op(ins) == OpCode::JUMP) has_jmp = true;
    }
    EXPECT_TRUE(has_jif) << "expected JUMP_IF_FALSE";
    EXPECT_TRUE(has_jmp) << "expected JUMP over else";
    int jif_pos = -1, jmp_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP_IF_FALSE) jif_pos = i;
        else if (instr_op(chunk->code[i]) == OpCode::JUMP) jmp_pos = i;
    }
    ASSERT_GE(jif_pos, 0);
    ASSERT_GE(jmp_pos, 0);
    int jif_target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    EXPECT_EQ(jif_target, jmp_pos + 1);
}

TEST(CompilerWhile, BasicWhile)
{
    Chunk* chunk = compile_ok("طالما س:\n"
                              "    ا := 1\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    bool has_jif = false, has_loop = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::JUMP_IF_FALSE) has_jif = true;
        if (instr_op(ins) == OpCode::LOOP) has_loop = true;
    }
    EXPECT_TRUE(has_jif) << "while needs JUMP_IF_FALSE";
    EXPECT_TRUE(has_loop) << "while needs LOOP back-edge";

    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::LOOP) EXPECT_LT(instr_sBx(ins), 0) << "LOOP offset must be negative";
    }
}

TEST(CompilerWhile, JumpIfFalsePointsPastLoop)
{
    Chunk* chunk = compile_ok("طالما شرط:\n"
                              "    س := 0\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++)
        if (instr_op(chunk->code[i]) == OpCode::JUMP_IF_FALSE) jif_pos = i;
    ASSERT_GE(jif_pos, 0);
    int m_target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    ASSERT_LT(m_target, (int)chunk->code.size());
    EXPECT_EQ(instr_op(chunk->code[m_target]), OpCode::RETURN_NIL);
}

TEST(CompilerFor, ListIterationLowersToLoopBytecode)
{
    Chunk* chunk = compile_ok({ "عناصر := [1, 2, 3]\n", "مجموع := 0\n",
        "لكل عنصر في عناصر:\n"
        "    مجموع := (مجموع + عنصر)\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    bool has_list_len = false;
    bool has_list_get = false;
    bool has_jif = false;
    bool has_loop = false;
    bool has_add = false;

    for (auto ins : chunk->code) {
        switch (instr_op(ins)) {
        case OpCode::LIST_LEN: has_list_len = true; break;
        case OpCode::LIST_GET: has_list_get = true; break;
        case OpCode::JUMP_IF_FALSE: has_jif = true; break;
        case OpCode::LOOP:
            has_loop = true;
            EXPECT_LT(instr_sBx(ins), 0) << "for loop back-edge must be negative";
            break;
        case OpCode::OP_ADD: has_add = true; break;
        default: break;
        }
    }

    EXPECT_TRUE(has_list_len);
    EXPECT_TRUE(has_list_get);
    EXPECT_TRUE(has_jif);
    EXPECT_TRUE(has_loop);
    EXPECT_TRUE(has_add);
}

TEST(CompilerDict, LiteralLowersToNativeConstructorCall)
{
    Chunk* chunk = compile_ok("{\"a\": 1, \"b\": 2}\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL قاموس").op(OpCode::LOAD_GLOBAL).A(1).Bx(0);
    bc.next("LOAD_CONST key a").op(OpCode::LOAD_CONST).A(2).Bx(1);
    bc.next("LOAD_INT value 1").op(OpCode::LOAD_INT).A(3).Bx(load_int_bx(1));
    bc.next("LOAD_CONST key b").op(OpCode::LOAD_CONST).A(4).Bx(2);
    bc.next("LOAD_INT value 2").op(OpCode::LOAD_INT).A(5).Bx(load_int_bx(2));
    bc.next("IC_CALL").op(OpCode::IC_CALL).A(1).B(4);
    bc.next("MOVE result").op(OpCode::MOVE).A(0).B(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->constants.size(), 3u);
    EXPECT_TRUE(chunk->constants[0].is_string());
    EXPECT_EQ(chunk->constants[0].as_string()->str, "قاموس");
    EXPECT_TRUE(chunk->constants[1].is_string());
    EXPECT_EQ(chunk->constants[1].as_string()->str, "a");
    EXPECT_TRUE(chunk->constants[2].is_string());
    EXPECT_EQ(chunk->constants[2].as_string()->str, "b");
}

// Regression test for a register-allocation bug in compile_dict_impl.
//
// The per-pair loop in compile_dict_impl allocates a register for the key,
// compiles+discharges the key expression into it, then does the same for
// the value -- but, unlike every other place in the compiler that builds a
// contiguous argument block (e.g. the plain-call and member-call argument
// loops in compile_call_impl, which each follow a discharge with
// `m_current->free_regs_to(arg_reg + 1)`), it never reclaims the scratch
// registers used to *compute* that key or value.
//
// That reclaim matters because compile_list_impl/compile_dict_impl each
// leave their own destination register permanently reserved on return
// (their final `free_regs_to(dst + 1)` keeps `dst` allocated), which is one
// register more than the register-stack pointer sat at just before that
// nested expression started compiling. A nested list/dict literal (or a
// call -- see compile_call_impl's own `free_regs_to(fn_reg + 1)`) therefore
// leaks exactly one register past the point the caller expects. Because
// compile_dict_impl never frees back down after each key/value, that leak
// permanently shifts every later entry in the same dict literal one
// register further away from where the `قاموس` native constructor's
// IC_CALL will actually read it from -- corrupting the contiguous
// register block the call relies on for every pair after the leaky one.
TEST(CompilerDict, NestedListValueKeepsLaterEntriesContiguous)
{
    // {"a": [1], "b": 2}
    Chunk* chunk = compile_ok("{\"a\": [1], \"b\": 2}\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL قاموس").op(OpCode::LOAD_GLOBAL).A(1).Bx(0);
    bc.next("LOAD_CONST key a").op(OpCode::LOAD_CONST).A(2).Bx(1);
    bc.next("LIST_NEW").op(OpCode::LIST_NEW).A(5).B(1);
    bc.next("LOAD_INT nested element").op(OpCode::LOAD_INT).A(6).Bx(load_int_bx(1));
    bc.next("LIST_APPEND").op(OpCode::LIST_APPEND).A(5).B(6);
    bc.next("MOVE list result to its own dst").op(OpCode::MOVE).A(4).B(5);
    bc.next("MOVE list into value register").op(OpCode::MOVE).A(3).B(4);
    // Registers 2 and 3 hold the first pair ("a", [1]); fn_reg is 1, so the
    // second pair MUST land in registers 4 and 5 to stay inside the
    // contiguous 4-register argument block ([fn_reg+1 .. fn_reg+4]) that
    // the upcoming IC_CALL(fn_reg=1, argc=4) will read. A compiler that
    // forgets to free the nested list's leaked register instead allocates
    // "b"/2 into registers 5/6 -- one past where IC_CALL will look.
    bc.next("LOAD_CONST key b").op(OpCode::LOAD_CONST).A(4).Bx(2);
    bc.next("LOAD_INT value 2").op(OpCode::LOAD_INT).A(5).Bx(load_int_bx(2));
    bc.next("IC_CALL").op(OpCode::IC_CALL).A(1).B(4);
    bc.next("MOVE result").op(OpCode::MOVE).A(0).B(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->constants.size(), 3u);
    EXPECT_TRUE(chunk->constants[0].is_string());
    EXPECT_EQ(chunk->constants[0].as_string()->str, "قاموس");
    EXPECT_TRUE(chunk->constants[1].is_string());
    EXPECT_EQ(chunk->constants[1].as_string()->str, "a");
    EXPECT_TRUE(chunk->constants[2].is_string());
    EXPECT_EQ(chunk->constants[2].as_string()->str, "b");
}

TEST(CompilerReturn, ReturnNilEmitsReturnNil)
{
    Chunk* chunk = compile_ok("ارجع عدم\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerReturn, ReturnValueEmitsReturn)
{
    Chunk* chunk = compile_ok("ارجع 42\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 42").op(OpCode::LOAD_INT).Bx(load_int_bx(42));
    bc.next("RETURN").op(OpCode::RETURN).B(1);
    bc.done();
}

TEST(CompilerReturn, ReturnIsDeadCodeBarrier)
{
    Chunk* chunk = compile_ok({ "ارجع 1\n", "س := 99\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    for (auto& v : chunk->constants) EXPECT_NE(v.as_int(), 99) << "dead code leaked into constant pool";
    bool found_99 = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::LOAD_INT && instr_Bx(ins) == load_int_bx(99)) found_99 = true;
    }
    EXPECT_FALSE(found_99) << "dead code (LOAD_INT 99) was emitted after return";
}

TEST(CompilerReturn, TailCallEmitsCallTail)
{
    Chunk* chunk = compile_ok("دالة غلاف():\n"
                              "    ارجع دال()\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    Chunk const* fn = chunk->functions[0];
    bool has_tail = false;
    for (auto& ins : fn->code) {
        if (instr_op(ins) == OpCode::CALL_TAIL) has_tail = true;
    }
    EXPECT_TRUE(has_tail) << "return f() in function should emit CALL_TAIL";
}

TEST(CompilerFunc, FunctionReturningNil)
{
    Chunk* chunk = compile_ok("دالة مثال():\n"
                              "    ارجع\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("CLOSURE").op(OpCode::CLOSURE).A(0).Bx(0);
    bc.next("STORE_GLOBAL").op(OpCode::STORE_GLOBAL).A(0).Bx(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->functions.size(), 1u);
    Chunk const* fn = chunk->functions[0];
    EXPECT_EQ(fn->name, "مثال");
    EXPECT_EQ(fn->arity, 0);
    BytecodeChecker fbc(*fn);
    fbc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    fbc.done();
}

TEST(CompilerFunc, FunctionWithParams)
{
    Chunk* chunk = compile_ok("دالة اجمع(ا, ب):\n"
                              "    ارجع (ا + ب)\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    Chunk const* fn = chunk->functions[0];
    EXPECT_EQ(fn->arity, 2);
    EXPECT_EQ(fn->local_count, 3);

    BytecodeChecker fbc(*fn);

    fbc.next("OP_ADD").op(OpCode::OP_ADD).A(2).B(0).C(1);
    fbc.next("NOP").op(OpCode::NOP).A(0);
    fbc.next("RETURN").op(OpCode::RETURN).A(2).B(1);
    fbc.done();
}

TEST(CompilerFunc, FunctionStoredAsLocal)
{
    Chunk* chunk = compile_ok("دالة مثال():\n"
                              "    عدم\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_EQ(chunk->local_count, 1);
}

TEST(CompilerFunc, NestedFunctionIndexing)
{
    Chunk* chunk = compile_ok({ "دالة ا():\n"
                                "    عدم\n",
        "دالة ب():\n"
        "    عدم\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 2u);
    BytecodeChecker bc(*chunk);
    bc.next("CLOSURE a").op(OpCode::CLOSURE).A(0).Bx(0);
    bc.next("STORE_GLOBAL a").op(OpCode::STORE_GLOBAL).A(0).Bx(0);
    bc.next("CLOSURE b").op(OpCode::CLOSURE).A(1).Bx(1);
    bc.next("STORE_GLOBAL b").op(OpCode::STORE_GLOBAL).A(1).Bx(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerFunc, RecursiveFunctionBodyCompiles)
{
    // fn fact(n) { if (n) { return n } return 1 }
    Chunk* chunk = compile_ok("دالة مضروب(ن):\n"
                              "    اذا ن:\n"
                              "        ارجع ن\n"
                              "    ارجع 1\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    EXPECT_FALSE(chunk->functions[0]->code.empty());
}

TEST(CompilerCall, CallWithNoArgs)
{
    Chunk* chunk = compile_ok("دال()\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL f").op(OpCode::LOAD_GLOBAL);
    bc.next("IC_CALL").op(OpCode::IC_CALL).B(0);
    bc.next("RETURN").op(OpCode::RETURN).A(0).B(1);
    bc.done();
    EXPECT_EQ(chunk->ic_slots.size(), 1u);
}

TEST(CompilerCall, CallWithTwoArgs)
{
    Array<std::string> args { "1", "2" };
    Chunk* chunk = compile_ok((std::string((std::string("دال(") + test_source::join(args, ", ") + ")")) + "\n"));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL f").op(OpCode::LOAD_GLOBAL).A(0);
    bc.next("arg1").op(OpCode::LOAD_INT).A(1).Bx(load_int_bx(1));
    bc.next("arg2").op(OpCode::LOAD_INT).A(2).Bx(load_int_bx(2));
    bc.next("IC_CALL").op(OpCode::IC_CALL).A(0).B(2); // argc=2
    bc.next("RETURN").op(OpCode::RETURN).A(0).B(1);
    bc.done();
}

TEST(CompilerCall, CallResultUsed)
{
    Chunk* chunk = compile_ok("نتيجة := دال(س)\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    bool has_ic_call = false;
    for (auto& ins : chunk->code)
        if (instr_op(ins) == OpCode::IC_CALL) has_ic_call = true;
    EXPECT_TRUE(has_ic_call);
}

TEST(CompilerCall, ICSlotAllocatedPerCallSite)
{
    Chunk* chunk = compile_ok({ "دال()\n", "عام()\n" });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_EQ(chunk->ic_slots.size(), 2u);
}

TEST(CompilerList, EmptyList)
{
    Chunk* chunk = compile_ok("[]\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LIST_NEW").op(OpCode::LIST_NEW).A(1).B(0);
    bc.next("MOVE").op(OpCode::MOVE).A(0).B(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerList, ListWithElements)
{
    Array<std::string> elems = { "1", "2", "3" };
    Chunk* chunk
        = compile_ok((std::string((std::string("[") + test_source::join(std::move(elems), ", ") + "]")) + "\n"));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LIST_NEW").op(OpCode::LIST_NEW).A(1).B(3);
    bc.next("LOAD 1").op(OpCode::LOAD_INT).Bx(load_int_bx(1));
    bc.next("APPEND 1").op(OpCode::LIST_APPEND).A(1);
    bc.next("LOAD 2").op(OpCode::LOAD_INT).Bx(load_int_bx(2));
    bc.next("APPEND 2").op(OpCode::LIST_APPEND).A(1);
    bc.next("LOAD 3").op(OpCode::LOAD_INT).Bx(load_int_bx(3));
    bc.next("APPEND 3").op(OpCode::LIST_APPEND).A(1);
    bc.next("MOVE").op(OpCode::MOVE).A(0).B(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerMeta, TopLevelChunkNameIsMain)
{
    Chunk* chunk = compile_ok("");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_EQ(chunk->name, MAIN_PROGRAM);
}

TEST(CompilerMeta, FunctionAritySetCorrectly)
{
    Chunk* chunk = compile_ok("دالة دال(س, ص, ع):\n"
                              "    عدم\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->arity, 3);
}

TEST(CompilerMeta, LineInfoPresent)
{
    Chunk* chunk = compile_ok("س := 42\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_FALSE(chunk->lines.empty());
}

TEST(CompilerIntegration, Fibonacci)
{
    Array<std::string> args_n1, args_n2;
    args_n1.push("(ن - 1)");
    args_n2.push("(ن - 2)");

    Chunk* chunk = compile_ok((std::string("دالة فيبوناتشي(ن):\n")
        + test_source::indent((std::string("اذا (ن <= 1):\n"
                                           "    ارجع ن\n")
            + (std::string("ارجع")
                + (std::string(" ")
                    + (std::string("(") + (std::string("فيبوناتشي(") + test_source::join(args_n1, ", ") + ")") + " + "
                        + (std::string("فيبوناتشي(") + test_source::join(args_n2, ", ") + ")") + ")"))
                + "\n")))));

    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_FALSE(chunk->functions.empty());
    Chunk const* fib = chunk->functions[0];
    EXPECT_EQ(fib->name, "فيبوناتشي");
    EXPECT_EQ(fib->arity, 1);
    EXPECT_FALSE(fib->code.empty());
    EXPECT_GE(fib->ic_slots.size(), 2u);
}

TEST(CompilerLoop, BreakPatchesToLoopExit)
{
    Chunk* chunk = compile_ok("طالما شرط:\n"
                              "    اخرج\n"
                              "    س := 1\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    int jump_pos = -1;
    int loop_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP && jump_pos < 0) jump_pos = i;
        if (instr_op(chunk->code[i]) == OpCode::LOOP) loop_pos = i;
    }

    ASSERT_GE(jump_pos, 0);
    ASSERT_GE(loop_pos, 0);
    EXPECT_GT(jump_pos + 1 + instr_sBx(chunk->code[jump_pos]), loop_pos);
}

TEST(CompilerLoop, ContinuePatchesToLoopLatch)
{
    Chunk* chunk = compile_ok("طالما شرط:\n"
                              "    اكمل\n"
                              "    س := 1\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);

    int jump_pos = -1;
    int loop_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP && jump_pos < 0) jump_pos = i;
        if (instr_op(chunk->code[i]) == OpCode::LOOP) loop_pos = i;
    }

    ASSERT_GE(jump_pos, 0);
    ASSERT_GE(loop_pos, 0);
    EXPECT_EQ(jump_pos + 1 + instr_sBx(chunk->code[jump_pos]), loop_pos);
}

TEST(CompilerLoop, BreakOutsideLoopIsRejected)
{
    Chunk* chunk = compile_fail("اخرج\n");
    ASSERT_EQ(chunk, nullptr);
}

TEST(CompilerLoop, ContinueOutsideLoopIsRejected)
{
    Chunk* chunk = compile_fail("اكمل\n");
    ASSERT_EQ(chunk, nullptr);
}

TEST(CompilerIntegration, MixedLiteralsInList)
{
    Array<std::string> elems = { "صحيح", "42", "3.14", "\"mohamed\"" };
    Chunk* chunk
        = compile_ok((std::string((std::string("[") + test_source::join(std::move(elems), ", ") + "]")) + "\n"));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    int appends = 0;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::LIST_APPEND) appends++;
    }
    EXPECT_EQ(appends, 4);
}
