#include "../fairuz/ast.hpp"
#include "../fairuz/ast_printer.hpp"
#include "../fairuz/compiler.hpp"
#include "../fairuz/diagnostic.hpp"
#include "../fairuz/string.hpp"
#include "test_common.h"

#include <algorithm>
#include <gtest/gtest.h>

using namespace fairuz::runtime;
using namespace fairuz;

class BytecodeChecker {
public:
    explicit BytecodeChecker(Fa_Chunk const& chunk)
        : chunk_(chunk)
        , pos_(0)
    {
    }

    BytecodeChecker& m_next(Fa_StringRef label = "")
    {
        label_ = std::move(label);
        EXPECT_LT(pos_, chunk_.code.size()) << "ran off end of code at step \"" << label_ << "\"";
        if (pos_ < chunk_.code.size()) {
            cur_ = chunk_.code[pos_];
            pos_ += 1;
        }
        return *this;
    }

    BytecodeChecker& op(Fa_OpCode expected)
    {
        EXPECT_EQ(Fa_instr_op(cur_), expected) << at() << "  expected op=" << Fa_opcode_name(expected)
                                               << "  got=" << Fa_opcode_name(static_cast<Fa_OpCode>(Fa_instr_op(cur_)));
        return *this;
    }
    BytecodeChecker& A(u8 expected)
    {
        EXPECT_EQ(Fa_instr_A(cur_), expected) << at() << "  field A";
        return *this;
    }
    BytecodeChecker& B(u8 expected)
    {
        EXPECT_EQ(Fa_instr_B(cur_), expected) << at() << "  field B";
        return *this;
    }
    BytecodeChecker& C(u8 expected)
    {
        EXPECT_EQ(Fa_instr_C(cur_), expected) << at() << "  field C";
        return *this;
    }
    BytecodeChecker& Bx(u16 expected)
    {
        EXPECT_EQ(Fa_instr_Bx(cur_), expected) << at() << "  field Bx";
        return *this;
    }
    BytecodeChecker& s_bx(int expected)
    {
        EXPECT_EQ(Fa_instr_sBx(cur_), expected) << at() << "  field sBx";
        return *this;
    }

    BytecodeChecker& done()
    {
        EXPECT_EQ(pos_, chunk_.code.size()) << "expected end of code at instruction " << pos_ << " but " << (chunk_.code.size() - pos_) << " more remain";
        return *this;
    }

    u32 current_index() const { return pos_ - 1; }
    u32 next_index() const { return pos_; }

private:
    Fa_StringRef at() const
    {
        std::ostringstream ss;
        ss << "[instr " << (pos_ - 1) << " \"" << label_ << "\"]";
        return ss.str().data();
    }

    Fa_Chunk const& chunk_;
    u32 pos_;
    u32 cur_ = 0;
    Fa_StringRef label_;
};

static Fa_Chunk* compile_ok(Fa_Array<AST::Fa_Stmt*> stmts, Compiler& c)
{
    diagnostic::reset();
    Fa_Chunk* chunk = c.compile(stmts);
    EXPECT_FALSE(diagnostic::has_errors());
    diagnostic::reset();
    return chunk;
}

static void dump(Fa_Chunk const* c)
{
    std::cout << '\n'
              << "Disassembled bytecode :" << '\n';
    c->disassemble();
    std::cout << '\n';
}

static Fa_Chunk* compile_ok(Fa_Array<AST::Fa_Stmt*> stmts)
{
    Compiler c;
    return compile_ok(stmts, c);
}

static Fa_Chunk* compile_ok(AST::Fa_Stmt* root)
{
    Fa_Array<AST::Fa_Stmt*> stmts;
    stmts.push(root);
    return compile_ok(stmts);
}

static Fa_Chunk* compile_ok(AST::Fa_Stmt* root, Compiler& c)
{
    Fa_Array<AST::Fa_Stmt*> stmts;
    stmts.push(root);
    return compile_ok(stmts, c);
}

static Fa_Chunk* compile_fail(Fa_Array<AST::Fa_Stmt*> stmts)
{
    diagnostic::reset();
    Fa_Chunk* chunk = Compiler().compile(stmts);
    EXPECT_TRUE(diagnostic::has_errors());
    diagnostic::reset();
    return chunk;
}

static Fa_Chunk* compile_fail(AST::Fa_Stmt* root)
{
    Fa_Array<AST::Fa_Stmt*> stmts;
    stmts.push(root);
    return compile_fail(stmts);
}

static u16 load_int_bx(i64 i) { return static_cast<u16>(i + 32767); }

TEST(CompilerLiteral, NilFa_Expression)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(lit_nil()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_NIL").op(Fa_OpCode::LOAD_NIL).A(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, TrueLiteral)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(lit_bool(true)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_TRUE").op(Fa_OpCode::LOAD_TRUE).A(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, FalseLiteral)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(lit_bool(false)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_FALSE").op(Fa_OpCode::LOAD_FALSE).A(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, SmallIntegerUsesLoadInt)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(lit_int(42)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(42));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
    EXPECT_TRUE(chunk->constants.empty());
}

TEST(CompilerLiteral, NegativeSmallIntegerUsesLoadInt)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(lit_int(-100)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(-100));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, ZeroUsesLoadInt)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(lit_int(0)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(0));
}

TEST(CompilerLiteral, LargeIntegerUsesConstantPool)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(lit_int(100000)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_CONST").op(Fa_OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(Fa_IS_INTEGER(chunk->constants[0]));
    EXPECT_EQ(Fa_AS_INTEGER(chunk->constants[0]), 100000);
}

TEST(CompilerLiteral, FloatUsesConstantPool)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(lit_flt(3.14)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_CONST").op(Fa_OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(Fa_IS_DOUBLE(chunk->constants[0]));
    EXPECT_NEAR(Fa_AS_DOUBLE(chunk->constants[0]), 3.14, 1e-9);
}

TEST(CompilerLiteral, StringUsesConstantPool)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(lit_str("hello")));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_CONST").op(Fa_OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(Fa_IS_STRING(chunk->constants[0]));
    EXPECT_EQ(Fa_AS_STRING(chunk->constants[0])->str, "hello");
}

TEST(CompilerLiteral, StringsDeduplicated)
{
    Fa_Array<AST::Fa_Stmt*> stmts;
    stmts.push(expr_stmt(lit_str("dup")));
    stmts.push(expr_stmt(lit_str("dup")));
    Fa_Chunk* chunk = compile_ok(blk(std::move(stmts)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    long string_constants = std::count_if(chunk->constants.begin(), chunk->constants.end(), [](Fa_Value const& v) { return Fa_IS_STRING(v); });
    EXPECT_EQ(string_constants, 1);
}

TEST(CompilerVar, LocalDeclaration)
{
    Fa_Chunk* chunk = compile_ok(decl_stmt("x", lit_int(5)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(5));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->local_count, 1);
}

TEST(CompilerVar, TwoLocalsUseConsecutiveRegisters)
{
    Fa_Chunk* chunk = compile_ok({ decl_stmt("x", lit_int(1)), decl_stmt("y", lit_int(2)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT x").op(Fa_OpCode::LOAD_INT).A(0);
    bc.m_next("LOAD_INT y").op(Fa_OpCode::LOAD_INT).A(1);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->local_count, 2);
}

TEST(CompilerVar, LocalAssignmentWritesBackToSameRegister)
{
    Fa_Chunk* chunk = compile_ok({ decl_stmt("x", lit_int(1)), assign_stmt(name_expr("x"), lit_int(2)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl_stmt x=1").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.m_next("assign x=2").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(2));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerVar, GlobalLoadAndStore)
{
    Fa_Chunk* chunk = compile_ok({ assign_stmt(name_expr("g"), lit_int(7)), expr_stmt(name_expr("g")) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("RHS").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(7));
    bc.m_next("STORE_GLOBAL").op(Fa_OpCode::STORE_GLOBAL);
    bc.m_next("LOAD_GLOBAL").op(Fa_OpCode::LOAD_GLOBAL);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
    bool found = false;
    for (auto& v : chunk->constants) {
        if (Fa_IS_STRING(v) && Fa_AS_STRING(v)->str == "g")
            found = true;
    }
    EXPECT_TRUE(found) << "global name_expr 'g' not interned into constant pool";
}

TEST(CompilerUnary, NegateVariable)
{
    Fa_Chunk* chunk = compile_ok({ decl_stmt("x", lit_int(5)), expr_stmt(unary(name_expr("x"), AST::Fa_UnaryOp::OP_NEG)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT x").op(Fa_OpCode::LOAD_INT).A(0);
    bc.m_next("OP_NEG").op(Fa_OpCode::OP_NEG).B(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, NotVariable)
{
    Fa_Chunk* chunk = compile_ok({ decl_stmt("b", lit_bool(true)), expr_stmt(unary(name_expr("b"), AST::Fa_UnaryOp::OP_NOT)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_TRUE b").op(Fa_OpCode::LOAD_TRUE).A(0);
    bc.m_next("OP_NOT").op(Fa_OpCode::OP_NOT).B(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, BitwiseNotVariable)
{
    Fa_Chunk* chunk = compile_ok({ decl_stmt("n", lit_int(0xFF)), expr_stmt(unary(name_expr("n"), AST::Fa_UnaryOp::OP_BITNOT)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl_stmt").op(Fa_OpCode::LOAD_INT).A(0);
    bc.m_next("OP_BITNOT").op(Fa_OpCode::OP_BITNOT).B(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, NegLiteralFolded)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(unary(lit_int(3), AST::Fa_UnaryOp::OP_NEG)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT -3").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(-3));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
    EXPECT_TRUE(chunk->constants.empty());
}

TEST(CompilerUnary, NotTrueFolded)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(unary(lit_bool(true), AST::Fa_UnaryOp::OP_NOT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_FALSE").op(Fa_OpCode::LOAD_FALSE);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, NotFalseFolded)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(unary(lit_bool(false), AST::Fa_UnaryOp::OP_NOT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_TRUE").op(Fa_OpCode::LOAD_TRUE);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, BNotLiteralFolded)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(unary(lit_int(0), AST::Fa_UnaryOp::OP_BITNOT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT -1").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(-1));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, AddTwoLocals)
{
    Fa_Chunk* chunk = compile_ok(
        { decl_stmt("a", lit_int(1)), decl_stmt("b", lit_int(2)),
            expr_stmt(binary(name_expr("a"), name_expr("b"), AST::Fa_BinaryOp::OP_ADD)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl_stmt a").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.m_next("decl_stmt b").op(Fa_OpCode::LOAD_INT).A(1).Bx(load_int_bx(2));
    bc.m_next("OP_ADD").op(Fa_OpCode::OP_ADD).A(2).B(0).C(1);
    bc.m_next("NOP ic").op(Fa_OpCode::NOP).A(0); // IC slot 0
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->ic_slots.size(), 1u);
}

TEST(CompilerBinary, SubtractLiterals)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(binary(lit_int(10), lit_int(3), AST::Fa_BinaryOp::OP_SUB)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 7").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(7));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
    EXPECT_TRUE(chunk->ic_slots.empty());
}

TEST(CompilerBinary, MultiplyLiterals)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(binary(lit_int(6), lit_int(7), AST::Fa_BinaryOp::OP_MUL)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 42").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(42));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, DivisionFolded)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(binary(lit_flt(1.0), lit_flt(2.0), AST::Fa_BinaryOp::OP_DIV)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_CONST 0.5").op(Fa_OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(chunk->constants[0]), 0.5);
}

TEST(CompilerBinary, DivisionByZeroNotFolded)
{
    Fa_Chunk* chunk = compile_ok({ decl_stmt("x", lit_int(5)),
        expr_stmt(binary(name_expr("x"), lit_int(0), AST::Fa_BinaryOp::OP_DIV)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl_stmt x").op(Fa_OpCode::LOAD_INT).A(0);
    bc.m_next("LOAD_INT 0 into r1 temp").op(Fa_OpCode::LOAD_INT);
    bc.m_next("OP_DIV").op(Fa_OpCode::OP_DIV);
    bc.m_next("NOP ic").op(Fa_OpCode::NOP);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, GreaterThanNormalizedToLT)
{
    Fa_Chunk* chunk = compile_ok(
        { decl_stmt("a", lit_int(3)), decl_stmt("b", lit_int(1)),
            expr_stmt(binary(name_expr("a"), name_expr("b"), AST::Fa_BinaryOp::OP_GT)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl_stmt a").op(Fa_OpCode::LOAD_INT).A(0);
    bc.m_next("decl_stmt b").op(Fa_OpCode::LOAD_INT).A(1);
    // GT(a,b) → OP_LT(b,a): B=r1(b), C=r0(a)
    bc.m_next("OP_LT").op(Fa_OpCode::OP_LT).B(1).C(0);
    bc.m_next("NOP ic").op(Fa_OpCode::NOP);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, GreaterEqualNormalizedToLE)
{
    Fa_Chunk* chunk = compile_ok(
        { decl_stmt("a", lit_int(5)), decl_stmt("b", lit_int(5)),
            expr_stmt(binary(name_expr("a"), name_expr("b"), AST::Fa_BinaryOp::OP_GTE)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl_stmt a").op(Fa_OpCode::LOAD_INT).A(0);
    bc.m_next("decl_stmt b").op(Fa_OpCode::LOAD_INT).A(1);
    // GE(a,b) → OP_LTE(b,a)
    bc.m_next("OP_LTE").op(Fa_OpCode::OP_LTE).B(1).C(0);
    bc.m_next("NOP ic").op(Fa_OpCode::NOP);
}

TEST(CompilerBinary, ICSlotAllocatedPerBinaryOp)
{
    Fa_Chunk* chunk = compile_ok({ decl_stmt("x", lit_int(1)), decl_stmt("y", lit_int(2)),
        expr_stmt(binary(name_expr("x"), name_expr("y"), AST::Fa_BinaryOp::OP_ADD)),
        expr_stmt(binary(name_expr("x"), name_expr("y"), AST::Fa_BinaryOp::OP_MUL)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->ic_slots.size(), 2u);
}

TEST(CompilerBinary, EqualityLiteralsFolded)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(binary(lit_int(1), lit_int(1), AST::Fa_BinaryOp::OP_EQ)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_TRUE").op(Fa_OpCode::LOAD_TRUE);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, InequalityLiteralsFolded)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(binary(lit_int(1), lit_int(2), AST::Fa_BinaryOp::OP_NEQ)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_TRUE").op(Fa_OpCode::LOAD_TRUE);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, BitwiseAndFolded)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(binary(lit_int(0b1100), lit_int(0b1010), AST::Fa_BinaryOp::OP_BITAND)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 8").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(8));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, ShiftLeftFolded)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(binary(lit_int(1), lit_int(3), AST::Fa_BinaryOp::OP_LSHIFT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 8").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(8));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, LogicalAndShortCircuit)
{
    Fa_Chunk* chunk = compile_ok({ decl_stmt("a", lit_bool(true)), decl_stmt("b", lit_bool(false)),
        expr_stmt(binary(name_expr("a"), name_expr("b"), AST::Fa_BinaryOp::OP_AND)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl_stmt a").op(Fa_OpCode::LOAD_TRUE).A(0);
    bc.m_next("decl_stmt b").op(Fa_OpCode::LOAD_FALSE).A(1);
    u32 jif_idx;
    bc.m_next("LHS into temp").op(Fa_OpCode::MOVE).A(3).B(0);
    (void)jif_idx;
    bool found_jif = false, found_b = false;
    for (auto& instr : chunk->code) {
        if (Fa_instr_op(instr) == Fa_OpCode::JUMP_IF_FALSE)
            found_jif = true;
        if (found_jif && (Fa_instr_op(instr) == Fa_OpCode::LOAD_FALSE || Fa_instr_op(instr) == Fa_OpCode::MOVE))
            found_b = true;
    }
    EXPECT_TRUE(found_jif) << "expected JUMP_IF_FALSE for && short-circuit";
    EXPECT_TRUE(found_b) << "expected RHS load after JUMP_IF_FALSE";
}

TEST(CompilerBinary, LogicalOrShortCircuit)
{
    Fa_Chunk* chunk = compile_ok(
        { decl_stmt("a", lit_bool(false)), decl_stmt("b", lit_bool(true)),
            expr_stmt(binary(name_expr("a"), name_expr("b"), AST::Fa_BinaryOp::OP_OR)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    bool found_jit = false;
    for (auto& instr : chunk->code) {
        if (Fa_instr_op(instr) == Fa_OpCode::JUMP_IF_TRUE)
            found_jit = true;
    }
    EXPECT_TRUE(found_jit) << "expected JUMP_IF_TRUE for || short-circuit";
}

TEST(CompilerBinary, AndWithBothLiteralsTrueNotFolded)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(binary(lit_bool(true), lit_bool(true), AST::Fa_BinaryOp::OP_AND)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_FALSE(chunk->code.empty());
}

TEST(CompilerIf, SimpleIfNoElse)
{
    Fa_Chunk* chunk = compile_ok(if_stmt(name_expr("x"), blk({ decl_stmt("y", lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i += 1) {
        if (Fa_instr_op(chunk->code[i]) == Fa_OpCode::JUMP_IF_FALSE)
            jif_pos = i;
    }
    ASSERT_GE(jif_pos, 0) << "expected JUMP_IF_FALSE";
    int m_target = jif_pos + 1 + Fa_instr_sBx(chunk->code[jif_pos]);
    EXPECT_EQ(Fa_instr_op(chunk->code[m_target]), Fa_OpCode::RETURN_NIL);
}

TEST(CompilerIf, IfElse)
{
    Fa_Chunk* chunk = compile_ok(
        if_stmt(
            name_expr("x"),
            blk({ decl_stmt("a", lit_int(1)) }),
            blk({ decl_stmt("b", lit_int(2)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    bool has_jif = false, has_jmp = false;
    for (auto& ins : chunk->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::JUMP_IF_FALSE)
            has_jif = true;
        if (Fa_instr_op(ins) == Fa_OpCode::JUMP)
            has_jmp = true;
    }
    EXPECT_TRUE(has_jif) << "expected JUMP_IF_FALSE";
    EXPECT_TRUE(has_jmp) << "expected JUMP over else";
    int jif_pos = -1, jmp_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i += 1) {
        if (Fa_instr_op(chunk->code[i]) == Fa_OpCode::JUMP_IF_FALSE)
            jif_pos = i;
        else if (Fa_instr_op(chunk->code[i]) == Fa_OpCode::JUMP)
            jmp_pos = i;
    }
    ASSERT_GE(jif_pos, 0);
    ASSERT_GE(jmp_pos, 0);
    int jif_target = jif_pos + 1 + Fa_instr_sBx(chunk->code[jif_pos]);
    EXPECT_EQ(jif_target, jmp_pos + 1);
}

TEST(CompilerIf, ConstantTrueConditionDCE)
{
    Fa_Chunk* chunk = compile_ok(if_stmt(lit_bool(true), blk({ decl_stmt("x", lit_int(1)) }), blk({ decl_stmt("y", lit_int(999)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    for (auto& ins : chunk->code)
        EXPECT_NE(Fa_instr_op(ins), Fa_OpCode::JUMP_IF_FALSE) << "JUMP_IF_FALSE should not exist when condition is const-true";
    for (auto& v : chunk->constants)
        EXPECT_NE(Fa_AS_INTEGER(v), 999);
}

TEST(CompilerIf, ConstantFalseConditionDCE)
{
    Fa_Chunk* chunk = compile_ok(if_stmt(lit_bool(false), blk({ decl_stmt("x", lit_int(1)) }), blk({ decl_stmt("y", lit_int(2)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 2").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(2));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerIf, ConstantFalseNoElseEmitsNothing)
{
    Fa_Chunk* chunk = compile_ok(if_stmt(lit_bool(false), blk({ decl_stmt("x", lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerWhile, BasicWhile)
{
    Fa_Chunk* chunk = compile_ok(while_stmt(name_expr("x"), blk({ decl_stmt("a", lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    bool has_jif = false, has_loop = false;
    for (auto& ins : chunk->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::JUMP_IF_FALSE)
            has_jif = true;
        if (Fa_instr_op(ins) == Fa_OpCode::LOOP)
            has_loop = true;
    }
    EXPECT_TRUE(has_jif) << "while needs JUMP_IF_FALSE";
    EXPECT_TRUE(has_loop) << "while needs LOOP back-edge";

    for (auto& ins : chunk->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::LOOP)
            EXPECT_LT(Fa_instr_sBx(ins), 0) << "LOOP offset must be negative";
    }
}

TEST(CompilerWhile, WhileFalseEmitsNothing)
{
    Fa_Chunk* chunk = compile_ok(while_stmt(lit_bool(false), blk({ decl_stmt("x", lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerWhile, WhileTrueEmitsUnconditionalLoop)
{
    Fa_Chunk* chunk = compile_ok(while_stmt(lit_bool(true), blk({ })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    bool has_jif = false, has_loop = false;
    for (auto& ins : chunk->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::JUMP_IF_FALSE)
            has_jif = true;
        if (Fa_instr_op(ins) == Fa_OpCode::LOOP)
            has_loop = true;
    }
    EXPECT_FALSE(has_jif) << "while(true) must not emit JUMP_IF_FALSE";
    EXPECT_TRUE(has_loop) << "while(true) must emit LOOP";
}

TEST(CompilerWhile, JumpIfFalsePointsPastLoop)
{
    Fa_Chunk* chunk = compile_ok(while_stmt(name_expr("cond"), blk({ decl_stmt("x", lit_int(0)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i += 1)
        if (Fa_instr_op(chunk->code[i]) == Fa_OpCode::JUMP_IF_FALSE)
            jif_pos = i;
    ASSERT_GE(jif_pos, 0);
    int m_target = jif_pos + 1 + Fa_instr_sBx(chunk->code[jif_pos]);
    ASSERT_LT(m_target, (int)chunk->code.size());
    EXPECT_EQ(Fa_instr_op(chunk->code[m_target]), Fa_OpCode::RETURN_NIL);
}

TEST(CompilerFor, ListIterationLowersToLoopBytecode)
{
    Fa_Chunk* chunk = compile_ok({ decl_stmt("items", list_expr({ lit_int(1), lit_int(2), lit_int(3) })),
        decl_stmt("sum", lit_int(0)),
        for_stmt(
            name_expr("item"),
            name_expr("items"),
            blk({ assign_stmt(name_expr("sum"), binary(name_expr("sum"), name_expr("item"), AST::Fa_BinaryOp::OP_ADD)) })) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    bool has_list_len = false;
    bool has_list_get = false;
    bool has_jif = false;
    bool has_loop = false;
    bool has_add = false;

    for (auto ins : chunk->code) {
        switch (Fa_instr_op(ins)) {
        case Fa_OpCode::LIST_LEN:
            has_list_len = true;
            break;
        case Fa_OpCode::LIST_GET:
            has_list_get = true;
            break;
        case Fa_OpCode::JUMP_IF_FALSE:
            has_jif = true;
            break;
        case Fa_OpCode::LOOP:
            has_loop = true;
            EXPECT_LT(Fa_instr_sBx(ins), 0) << "for loop back-edge must be negative";
            break;
        case Fa_OpCode::OP_ADD:
            has_add = true;
            break;
        default:
            break;
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
    Fa_Chunk* chunk = compile_ok(
        expr_stmt(
            dict_expr({
                { lit_str("a"), lit_int(1) },
                { lit_str("b"), lit_int(2) },
            })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_GLOBAL جدول").op(Fa_OpCode::LOAD_GLOBAL).A(1).Bx(0);
    bc.m_next("LOAD_CONST key a").op(Fa_OpCode::LOAD_CONST).A(2).Bx(1);
    bc.m_next("LOAD_INT value 1").op(Fa_OpCode::LOAD_INT).A(3).Bx(load_int_bx(1));
    bc.m_next("LOAD_CONST key b").op(Fa_OpCode::LOAD_CONST).A(4).Bx(2);
    bc.m_next("LOAD_INT value 2").op(Fa_OpCode::LOAD_INT).A(5).Bx(load_int_bx(2));
    bc.m_next("IC_CALL").op(Fa_OpCode::IC_CALL).A(1).B(4);
    bc.m_next("MOVE result").op(Fa_OpCode::MOVE).A(0).B(1);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->constants.size(), 3u);
    EXPECT_TRUE(Fa_IS_STRING(chunk->constants[0]));
    EXPECT_EQ(Fa_AS_STRING(chunk->constants[0])->str, "جدول");
    EXPECT_TRUE(Fa_IS_STRING(chunk->constants[1]));
    EXPECT_EQ(Fa_AS_STRING(chunk->constants[1])->str, "a");
    EXPECT_TRUE(Fa_IS_STRING(chunk->constants[2]));
    EXPECT_EQ(Fa_AS_STRING(chunk->constants[2])->str, "b");
}

TEST(CompilerReturn, ReturnNilEmitsReturnNil)
{
    Fa_Chunk* chunk = compile_ok(return_stmt(lit_nil()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerReturn, ReturnValueEmitsReturn)
{
    Fa_Chunk* chunk = compile_ok(return_stmt(lit_int(42)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 42").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(42));
    bc.m_next("RETURN").op(Fa_OpCode::RETURN).B(1);
    bc.done();
}

TEST(CompilerReturn, ReturnIsDeadCodeBarrier)
{
    Fa_Chunk* chunk = compile_ok({ return_stmt(lit_int(1)), decl_stmt("x", lit_int(99)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    for (auto& v : chunk->constants)
        EXPECT_NE(Fa_AS_INTEGER(v), 99) << "dead code leaked into constant pool";
    bool found_99 = false;
    for (auto& ins : chunk->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::LOAD_INT && Fa_instr_Bx(ins) == load_int_bx(99))
            found_99 = true;
    }
    EXPECT_FALSE(found_99) << "dead code (LOAD_INT 99) was emitted after return";
}

TEST(CompilerReturn, TailCallEmitsCallTail)
{
    Fa_Chunk* chunk = compile_ok(func_def(name_expr("wrapper"), list_expr(), blk({ return_stmt(call_expr(name_expr("f"))) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    Fa_Chunk const* fn = chunk->functions[0];
    bool has_tail = false;
    for (auto& ins : fn->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::CALL_TAIL)
            has_tail = true;
    }
    EXPECT_TRUE(has_tail) << "return f() in function should emit CALL_TAIL";
}

TEST(CompilerFunc, EmptyFunction)
{
    Fa_Chunk* chunk = compile_ok(func_def(name_expr("foo"), list_expr(), blk({ })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("CLOSURE").op(Fa_OpCode::CLOSURE).A(0).Bx(0);
    bc.m_next("STORE_GLOBAL").op(Fa_OpCode::STORE_GLOBAL).A(0).Bx(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->functions.size(), 1u);
    Fa_Chunk const* fn = chunk->functions[0];
    EXPECT_EQ(fn->name, "foo");
    EXPECT_EQ(fn->arity, 0);
    BytecodeChecker fbc(*fn);
    fbc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    fbc.done();
}

TEST(CompilerFunc, FunctionWithParams)
{
    Fa_Chunk* chunk = compile_ok(func_def(name_expr("add"), list_expr({ name_expr("a"), name_expr("b") }),
        blk({ return_stmt(binary(name_expr("a"), name_expr("b"), AST::Fa_BinaryOp::OP_ADD)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    Fa_Chunk const* fn = chunk->functions[0];
    EXPECT_EQ(fn->arity, 2);
    EXPECT_EQ(fn->local_count, 3);

    BytecodeChecker fbc(*fn);

    fbc.m_next("OP_ADD").op(Fa_OpCode::OP_ADD).A(2).B(0).C(1);
    fbc.m_next("NOP").op(Fa_OpCode::NOP).A(0);
    fbc.m_next("RETURN").op(Fa_OpCode::RETURN).A(2).B(1);
    fbc.done();
}

TEST(CompilerFunc, FunctionStoredAsLocal)
{
    Fa_Chunk* chunk = compile_ok(func_def(name_expr("foo"), list_expr(), blk({ })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->local_count, 1);
}

TEST(CompilerFunc, NestedFunctionIndexing)
{
    Fa_Chunk* chunk = compile_ok({ func_def(name_expr("a"), list_expr(), blk({ })), func_def(name_expr("b"), list_expr(), blk({ })) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 2u);
    BytecodeChecker bc(*chunk);
    bc.m_next("CLOSURE a").op(Fa_OpCode::CLOSURE).A(0).Bx(0);
    bc.m_next("STORE_GLOBAL a").op(Fa_OpCode::STORE_GLOBAL).A(0).Bx(0);
    bc.m_next("CLOSURE b").op(Fa_OpCode::CLOSURE).A(1).Bx(1);
    bc.m_next("STORE_GLOBAL b").op(Fa_OpCode::STORE_GLOBAL).A(1).Bx(1);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerFunc, RecursiveFunctionBodyCompiles)
{
    // fn fact(n) { if (n) { return n } return 1 }
    Fa_Chunk* chunk = compile_ok(
        func_def(
            name_expr("fact"),
            list_expr({ name_expr("n") }),
            blk({ if_stmt(
                      name_expr("n"),
                      blk(
                          { return_stmt(name_expr("n")) })),
                return_stmt(lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    EXPECT_FALSE(chunk->functions[0]->code.empty());
}

TEST(CompilerFunc, NestedFunctionRejected)
{
    Fa_Chunk* chunk = compile_fail(func_def(name_expr("outer"), list_expr(),
        blk({ decl_stmt("x",
                  lit_int(1)),
            func_def(
                name_expr("inner"), list_expr(),
                blk({ return_stmt(name_expr("x")) })) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->name, "outer");
    EXPECT_TRUE(chunk->functions[0]->functions.empty());
}

TEST(CompilerFunc, FunctionInsideTopLevelBlockRejected)
{
    Fa_Chunk* chunk = compile_fail(blk({ func_def(name_expr("inner"), list_expr(), blk({ })) }));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_TRUE(chunk->functions.empty());
}

TEST(CompilerCall, CallWithNoArgs)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(call_expr(name_expr("f"))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_GLOBAL f").op(Fa_OpCode::LOAD_GLOBAL);
    bc.m_next("IC_CALL").op(Fa_OpCode::IC_CALL).B(0);
    bc.m_next("RETURN").op(Fa_OpCode::RETURN).A(0).B(1);
    bc.done();
    EXPECT_EQ(chunk->ic_slots.size(), 1u);
}

TEST(CompilerCall, CallWithTwoArgs)
{
    Fa_Array<AST::Fa_Expr*> m_args;
    m_args.push(lit_int(1));
    m_args.push(lit_int(2));
    Fa_Chunk* chunk = compile_ok(expr_stmt(call_expr(name_expr("f"), list_expr(std::move(m_args)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_GLOBAL f").op(Fa_OpCode::LOAD_GLOBAL).A(0);
    bc.m_next("arg1").op(Fa_OpCode::LOAD_INT).A(1).Bx(load_int_bx(1));
    bc.m_next("arg2").op(Fa_OpCode::LOAD_INT).A(2).Bx(load_int_bx(2));
    bc.m_next("IC_CALL").op(Fa_OpCode::IC_CALL).A(0).B(2); // argc=2
    bc.m_next("RETURN").op(Fa_OpCode::RETURN).A(0).B(1);
    bc.done();
}

TEST(CompilerCall, CallResultUsed)
{
    Fa_Chunk* chunk = compile_ok(decl_stmt("r", call_expr(name_expr("f"), [] {
        Fa_Array<AST::Fa_Expr*> a;
        a.push(name_expr("x"));
        return list_expr(a);
    }())));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    bool has_ic_call = false;
    for (auto& ins : chunk->code)
        if (Fa_instr_op(ins) == Fa_OpCode::IC_CALL)
            has_ic_call = true;
    EXPECT_TRUE(has_ic_call);
}

TEST(CompilerCall, ICSlotAllocatedPerCallSite)
{
    Fa_Chunk* chunk = compile_ok({ expr_stmt(call_expr(name_expr("f"))), expr_stmt(call_expr(name_expr("g"))) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->ic_slots.size(), 2u);
}

TEST(CompilerList, EmptyList)
{
    Fa_Chunk* chunk = compile_ok(expr_stmt(list_expr()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LIST_NEW").op(Fa_OpCode::LIST_NEW).A(1).B(0);
    bc.m_next("MOVE").op(Fa_OpCode::MOVE).A(0).B(1);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerList, ListWithElements)
{
    Fa_Array<AST::Fa_Expr*> elems;
    elems.push(lit_int(1));
    elems.push(lit_int(2));
    elems.push(lit_int(3));
    Fa_Chunk* chunk = compile_ok(expr_stmt(list_expr(std::move(elems))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LIST_NEW").op(Fa_OpCode::LIST_NEW).A(1).B(3);
    bc.m_next("LOAD 1").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(1));
    bc.m_next("APPEND 1").op(Fa_OpCode::LIST_APPEND).A(1);
    bc.m_next("LOAD 2").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(2));
    bc.m_next("APPEND 2").op(Fa_OpCode::LIST_APPEND).A(1);
    bc.m_next("LOAD 3").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(3));
    bc.m_next("APPEND 3").op(Fa_OpCode::LIST_APPEND).A(1);
    bc.m_next("MOVE").op(Fa_OpCode::MOVE).A(0).B(1);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerList, ListCapHintCappedAt255)
{
    Fa_Array<AST::Fa_Expr*> elems;

    for (int i = 0; i < 300; i += 1)
        elems.push(lit_int(i));

    Fa_Chunk* chunk = compile_fail(expr_stmt(list_expr(std::move(elems)))); // too many regs
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_FALSE(chunk->code.empty());
    u32 new_list = chunk->code[0];
    EXPECT_EQ(Fa_instr_op(new_list), Fa_OpCode::LIST_NEW);
    EXPECT_EQ(Fa_instr_B(new_list), 255u) << "capacity hint must be capped at 255";
}

TEST(CompilerScope, LocalslDontLeakOutOfBlock)
{
    AST::Fa_BlockStmt* _ast = blk({ blk({ decl_stmt("x", lit_int(1)) }), decl_stmt("x", lit_int(2)) });
    Fa_Chunk* chunk = compile_ok(_ast);
    AST::ASTPrinter printer;
    printer.print(_ast);
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 1 (inner x)").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.m_next("LOAD_INT 2 (outer x)").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(2));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->local_count, 1);
}

TEST(CompilerScope, NestedScopesBothVisible)
{
    Fa_Chunk* chunk = compile_ok(
        { decl_stmt("a", lit_int(1)), blk([] {
             Fa_Array<AST::Fa_Stmt*> s;
             s.push(decl_stmt("b", lit_int(2)));
             s.push(decl_stmt("c", binary(name_expr("a"), name_expr("b"), AST::Fa_BinaryOp::OP_ADD)));
             return s;
         }()) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    bool found_add = false;
    for (auto& ins : chunk->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::OP_ADD) {
            EXPECT_EQ(Fa_instr_B(ins), 0u) << "a in r0";
            EXPECT_EQ(Fa_instr_C(ins), 1u) << "b in r1";
            found_add = true;
        }
    }
    EXPECT_TRUE(found_add);
}

TEST(CompilerMeta, TopLevelChunkNameIsMain)
{
    Fa_Chunk* chunk = compile_ok(blk({ }));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->name, "<main>");
}

TEST(CompilerMeta, LocalCountReflectsMaxRegisters)
{
    Fa_Chunk* chunk = compile_ok({ decl_stmt("a", lit_int(1)), decl_stmt("b", lit_int(2)), decl_stmt("c", lit_int(3)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->local_count, 3);
}

TEST(CompilerMeta, FunctionAritySetCorrectly)
{
    Fa_Chunk* chunk = compile_ok(func_def(name_expr("f"), list_expr({ name_expr("x"), name_expr("y"), name_expr("z") }), blk({ })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->arity, 3);
}

TEST(CompilerMeta, FunctionNameSetCorrectly)
{
    Fa_Chunk* chunk = compile_ok(func_def(name_expr("compute"), list_expr(), blk({ })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->name, "compute");
}

TEST(CompilerMeta, LineInfoPresent)
{
    Fa_Chunk* chunk = compile_ok(decl_stmt("x", lit_int(42)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_FALSE(chunk->lines.empty());
}

TEST(CompilerIntegration, Fibonacci)
{
    Fa_Array<AST::Fa_Expr*> args_n1, args_n2;
    args_n1.push(binary(name_expr("n"), lit_int(1), AST::Fa_BinaryOp::OP_SUB));
    args_n2.push(binary(name_expr("n"), lit_int(2), AST::Fa_BinaryOp::OP_SUB));

    Fa_Chunk* chunk = compile_ok(
        func_def(name_expr("fib"),
            list_expr({ name_expr("n") }),
            blk(
                { if_stmt(
                      binary(name_expr("n"), lit_int(1), AST::Fa_BinaryOp::OP_LTE),
                      blk({ return_stmt(name_expr("n")) })),
                    return_stmt(
                        binary(
                            call_expr(name_expr("fib"), list_expr(std::move(args_n1))),
                            call_expr(name_expr("fib"), list_expr(std::move(args_n2))),
                            AST::Fa_BinaryOp::OP_ADD)) })));

    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_FALSE(chunk->functions.empty());
    Fa_Chunk const* fib = chunk->functions[0];
    EXPECT_EQ(fib->name, "fib");
    EXPECT_EQ(fib->arity, 1);
    EXPECT_FALSE(fib->code.empty());
    EXPECT_GE(fib->ic_slots.size(), 2u);
}

TEST(CompilerLoop, BreakPatchesToLoopExit)
{
    Fa_Chunk* chunk = compile_ok(
        while_stmt(
            name_expr("cond"),
            blk({ break_stmt(),
                decl_stmt("x", lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    int jump_pos = -1;
    int loop_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i += 1) {
        if (Fa_instr_op(chunk->code[i]) == Fa_OpCode::JUMP && jump_pos < 0)
            jump_pos = i;
        if (Fa_instr_op(chunk->code[i]) == Fa_OpCode::LOOP)
            loop_pos = i;
    }

    ASSERT_GE(jump_pos, 0);
    ASSERT_GE(loop_pos, 0);
    EXPECT_GT(jump_pos + 1 + Fa_instr_sBx(chunk->code[jump_pos]), loop_pos);
}

TEST(CompilerLoop, ContinuePatchesToLoopLatch)
{
    Fa_Chunk* chunk = compile_ok(
        while_stmt(
            name_expr("cond"),
            blk({ continue_stmt(),
                decl_stmt("x", lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    int jump_pos = -1;
    int loop_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i += 1) {
        if (Fa_instr_op(chunk->code[i]) == Fa_OpCode::JUMP && jump_pos < 0)
            jump_pos = i;
        if (Fa_instr_op(chunk->code[i]) == Fa_OpCode::LOOP)
            loop_pos = i;
    }

    ASSERT_GE(jump_pos, 0);
    ASSERT_GE(loop_pos, 0);
    EXPECT_EQ(jump_pos + 1 + Fa_instr_sBx(chunk->code[jump_pos]), loop_pos);
}

TEST(CompilerLoop, BreakOutsideLoopIsRejected)
{
    Fa_Chunk* chunk = compile_fail(break_stmt());
    ASSERT_NE(chunk, nullptr);
}

TEST(CompilerLoop, ContinueOutsideLoopIsRejected)
{
    Fa_Chunk* chunk = compile_fail(continue_stmt());
    ASSERT_NE(chunk, nullptr);
}

TEST(CompilerIntegration, NestedArithmetic)
{
    Fa_Chunk* chunk = compile_ok(
        decl_stmt(
            "result",
            binary(
                binary(
                    lit_int(2),
                    lit_int(3),
                    AST::Fa_BinaryOp::OP_ADD),
                binary(
                    lit_int(4),
                    lit_int(1),
                    AST::Fa_BinaryOp::OP_SUB),
                AST::Fa_BinaryOp::OP_MUL)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 15").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(15));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
    EXPECT_TRUE(chunk->ic_slots.empty());
}

TEST(CompilerIntegration, StringConstantPoolDedup)
{
    Fa_Chunk* chunk = compile_ok(
        { expr_stmt(
              lit_str("hello")),
            expr_stmt(
                lit_str("hello")),
            expr_stmt(
                lit_str("hello")) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    int count = 0;
    for (auto& v : chunk->constants) {
        if (Fa_IS_STRING(v) && Fa_AS_STRING(v)->str == "hello")
            count += 1;
    }
    EXPECT_EQ(count, 1);
}

TEST(CompilerIntegration, MixedLiteralsInList)
{
    Fa_Array<AST::Fa_Expr*> elems;
    elems.push(lit_bool(true));
    elems.push(lit_int(42));
    elems.push(lit_flt(3.14));
    elems.push(lit_str("hi"));
    Fa_Chunk* chunk = compile_ok(expr_stmt(list_expr(std::move(elems))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    int appends = 0;
    for (auto& ins : chunk->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::LIST_APPEND)
            appends += 1;
    }
    EXPECT_EQ(appends, 4);
}

TEST(CompilerClass, DefinitionLowersToNamespaceDictAndConstructorClosure)
{
    auto* method = func_def(
        name_expr("make"),
        list_expr({ name_expr("x"), name_expr("y") }),
        blk(
            {
                assign_stmt(index_expr(name_expr("__class$instance"), lit_str("x")), name_expr("x")),
                assign_stmt(index_expr(name_expr("__class$instance"), lit_str("y")), name_expr("y")),
            }));

    Fa_Chunk* chunk = compile_ok(class_def(
        name_expr("Point"),
        { name_expr("x"), name_expr("y") },
        { method }));

    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    bool has_top_level_dict_ctor = false;
    bool has_closure = false;
    bool has_store_global = false;
    int top_level_sets = 0;

    for (u32 ins : chunk->code) {
        switch (Fa_instr_op(ins)) {
        case Fa_OpCode::LOAD_GLOBAL:
            has_top_level_dict_ctor = true;
            break;
        case Fa_OpCode::CLOSURE:
            has_closure = true;
            break;
        case Fa_OpCode::LIST_SET:
            top_level_sets += 1;
            break;
        case Fa_OpCode::STORE_GLOBAL:
            has_store_global = true;
            break;
        default:
            break;
        }
    }

    EXPECT_TRUE(has_top_level_dict_ctor);
    EXPECT_TRUE(has_closure);
    EXPECT_TRUE(has_store_global);
    EXPECT_GE(top_level_sets, 2);

    ASSERT_EQ(chunk->functions.size(), 1u);
    Fa_Chunk* ctor = chunk->functions[0];
    ASSERT_NE(ctor, nullptr);
    EXPECT_EQ(ctor->name, "Point.make");
    EXPECT_EQ(ctor->arity, 2);

    bool method_has_dict_ctor = false;
    bool method_has_return = false;
    int method_sets = 0;

    for (u32 ins : ctor->code) {
        switch (Fa_instr_op(ins)) {
        case Fa_OpCode::LOAD_GLOBAL:
            method_has_dict_ctor = true;
            break;
        case Fa_OpCode::LIST_SET:
            method_sets += 1;
            break;
        case Fa_OpCode::RETURN:
            method_has_return = true;
            break;
        default:
            break;
        }
    }

    EXPECT_TRUE(method_has_dict_ctor);
    EXPECT_EQ(method_sets, 2);
    EXPECT_TRUE(method_has_return);
}
