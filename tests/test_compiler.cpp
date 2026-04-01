#include "../include/ast_printer.hpp"
#include "../include/compiler.hpp"

#include <algorithm>
#include <gtest/gtest.h>

using namespace fairuz::runtime;
using namespace fairuz;

static constexpr AST::Fa_AssignmentExpr* assign_fa_expr(Fa_StringRef m_name, AST::Fa_Expr* val, u32 m_line = 1)
{
    return AST::Fa_makeAssignmentExpr(AST::Fa_makeName(m_name), val /*, line*/);
}
static constexpr AST::Fa_CallExpr* call(AST::Fa_Expr* m_callee, Fa_Array<AST::Fa_Expr*> m_args = { }, u32 m_line = 1)
{
    return Fa_makeCall(m_callee, AST::Fa_makeList(m_args) /*, line*/);
}
static constexpr AST::Fa_AssignmentStmt* decl(Fa_StringRef nm, AST::Fa_Expr* val, bool is_const = false, u32 m_line = 1)
{
    return Fa_makeAssignmentStmt(AST::Fa_makeName(nm), val, true);
}
static constexpr AST::Fa_AssignmentStmt* assign_stmt(Fa_StringRef nm, AST::Fa_Expr* val, u32 m_line = 1)
{
    return Fa_makeAssignmentStmt(AST::Fa_makeName(nm), val, false);
}
static constexpr AST::Fa_FunctionDef* func_stmt(Fa_StringRef nm, Fa_Array<AST::Fa_Expr*> m_params, AST::Fa_BlockStmt* m_body, u32 m_line = 1)
{
    return Fa_makeFunction(AST::Fa_makeName(nm), AST::Fa_makeList(m_params), m_body /*, line*/);
}

template<typename... Stmts>
static AST::Fa_BlockStmt* blk(Stmts&&... s)
{
    Fa_Array<AST::Fa_Stmt*> v;
    (v.push(std::forward<Stmts>(s)), ...);
    return AST::Fa_makeBlock(std::move(v));
}

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
        if (pos_ < chunk_.code.size())
            cur_ = chunk_.code[pos_++];
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

static Fa_Chunk* compile_ok(Fa_Array<AST::Fa_Stmt*> stmts, Compiler& c) { return c.compile(stmts); }

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

static u16 load_int_bx(i64 i) { return static_cast<u16>(i + 32767); }

TEST(CompilerLiteral, NilFa_Expression)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeLiteralNil()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_NIL").op(Fa_OpCode::LOAD_NIL).A(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, TrueLiteral)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeLiteralBool(true)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_TRUE").op(Fa_OpCode::LOAD_TRUE).A(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, FalseLiteral)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeLiteralBool(false)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_FALSE").op(Fa_OpCode::LOAD_FALSE).A(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, SmallIntegerUsesLoadInt)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(42)));
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
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(-100)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(-100));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, ZeroUsesLoadInt)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(0)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(0));
}

TEST(CompilerLiteral, LargeIntegerUsesConstantPool)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeLiteralInt(100000)));
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
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeLiteralFloat(3.14)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_CONST").op(Fa_OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(Fa_IS_DOUBLE(chunk->constants[0]));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(chunk->constants[0]), 3.14);
}

TEST(CompilerLiteral, StringUsesConstantPool)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeLiteralString("hello")));
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
    stmts.push(AST::Fa_makeExprStmt(AST::Fa_makeLiteralString("dup")));
    stmts.push(AST::Fa_makeExprStmt(AST::Fa_makeLiteralString("dup")));
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeBlock(std::move(stmts)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    long string_constants = std::count_if(chunk->constants.begin(), chunk->constants.end(), [](Fa_Value const& v) { return Fa_IS_STRING(v); });
    EXPECT_EQ(string_constants, 1);
}

TEST(CompilerVar, LocalDeclaration)
{
    Fa_Chunk* chunk = compile_ok(decl("x", AST::Fa_makeLiteralInt(5)));
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
    Fa_Chunk* chunk = compile_ok({ decl("x", AST::Fa_makeLiteralInt(1)), decl("y", AST::Fa_makeLiteralInt(2)) });
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
    Fa_Chunk* chunk = compile_ok({ decl("x", AST::Fa_makeLiteralInt(1)), assign_stmt("x", AST::Fa_makeLiteralInt(2)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl x=1").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.m_next("assign x=2").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(2));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerVar, GlobalLoadAndStore)
{
    Fa_Chunk* chunk = compile_ok({ assign_stmt("g", AST::Fa_makeLiteralInt(7)), AST::Fa_makeExprStmt(AST::Fa_makeName("g")) });
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
    EXPECT_TRUE(found) << "global name 'g' not interned into constant pool";
}

TEST(CompilerUnary, NegateVariable)
{
    Fa_Chunk* chunk = compile_ok({ decl("x", AST::Fa_makeLiteralInt(5)), AST::Fa_makeExprStmt(Fa_makeUnary(AST::Fa_makeName("x"), AST::Fa_UnaryOp::OP_NEG)) });
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
    Fa_Chunk* chunk = compile_ok({ decl("b", AST::Fa_makeLiteralBool(true)), AST::Fa_makeExprStmt(Fa_makeUnary(AST::Fa_makeName("b"), AST::Fa_UnaryOp::OP_NOT)) });
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
    Fa_Chunk* chunk = compile_ok({ decl("n", AST::Fa_makeLiteralInt(0xFF)), AST::Fa_makeExprStmt(Fa_makeUnary(AST::Fa_makeName("n"), AST::Fa_UnaryOp::OP_BITNOT)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl").op(Fa_OpCode::LOAD_INT).A(0);
    bc.m_next("OP_BITNOT").op(Fa_OpCode::OP_BITNOT).B(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, NegLiteralFolded)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeUnary(AST::Fa_makeLiteralInt(3), AST::Fa_UnaryOp::OP_NEG)));
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
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeUnary(AST::Fa_makeLiteralBool(true), AST::Fa_UnaryOp::OP_NOT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_FALSE").op(Fa_OpCode::LOAD_FALSE);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, NotFalseFolded)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeUnary(AST::Fa_makeLiteralBool(false), AST::Fa_UnaryOp::OP_NOT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_TRUE").op(Fa_OpCode::LOAD_TRUE);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, BNotLiteralFolded)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeUnary(AST::Fa_makeLiteralInt(0), AST::Fa_UnaryOp::OP_BITNOT)));
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
        { decl("a", AST::Fa_makeLiteralInt(1)), decl("b", AST::Fa_makeLiteralInt(2)),
            AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeName("a"), AST::Fa_makeName("b"), AST::Fa_BinaryOp::OP_ADD)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl a").op(Fa_OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.m_next("decl b").op(Fa_OpCode::LOAD_INT).A(1).Bx(load_int_bx(2));
    bc.m_next("OP_ADD").op(Fa_OpCode::OP_ADD).A(2).B(0).C(1);
    bc.m_next("NOP ic").op(Fa_OpCode::NOP).A(0); // IC slot 0
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->ic_slots.size(), 1u);
}

TEST(CompilerBinary, SubtractLiterals)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeLiteralInt(10), AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_SUB)));
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
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeLiteralInt(6), AST::Fa_makeLiteralInt(7), AST::Fa_BinaryOp::OP_MUL)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 42").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(42));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, DivisionFolded)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeLiteralFloat(1.0), AST::Fa_makeLiteralFloat(2.0), AST::Fa_BinaryOp::OP_DIV)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_CONST 0.5").op(Fa_OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(chunk->constants[0]), 0.5);
}

TEST(CompilerBinary, DivisionByZeroNotFolded)
{
    Fa_Chunk* chunk = compile_ok({ decl("x", AST::Fa_makeLiteralInt(5)),
        AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_DIV)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl x").op(Fa_OpCode::LOAD_INT).A(0);
    bc.m_next("LOAD_INT 0 into r1 temp").op(Fa_OpCode::LOAD_INT);
    bc.m_next("OP_DIV").op(Fa_OpCode::OP_DIV);
    bc.m_next("NOP ic").op(Fa_OpCode::NOP);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, GreaterThanNormalizedToLT)
{
    Fa_Chunk* chunk = compile_ok(
        { decl("a", AST::Fa_makeLiteralInt(3)), decl("b", AST::Fa_makeLiteralInt(1)),
            AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeName("a"), AST::Fa_makeName("b"), AST::Fa_BinaryOp::OP_GT)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl a").op(Fa_OpCode::LOAD_INT).A(0);
    bc.m_next("decl b").op(Fa_OpCode::LOAD_INT).A(1);
    // GT(a,b) → OP_LT(b,a): B=r1(b), C=r0(a)
    bc.m_next("OP_LT").op(Fa_OpCode::OP_LT).B(1).C(0);
    bc.m_next("NOP ic").op(Fa_OpCode::NOP);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, GreaterEqualNormalizedToLE)
{
    Fa_Chunk* chunk = compile_ok(
        { decl("a", AST::Fa_makeLiteralInt(5)), decl("b", AST::Fa_makeLiteralInt(5)),
            AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeName("a"), AST::Fa_makeName("b"), AST::Fa_BinaryOp::OP_GTE)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl a").op(Fa_OpCode::LOAD_INT).A(0);
    bc.m_next("decl b").op(Fa_OpCode::LOAD_INT).A(1);
    // GE(a,b) → OP_LTE(b,a)
    bc.m_next("OP_LTE").op(Fa_OpCode::OP_LTE).B(1).C(0);
    bc.m_next("NOP ic").op(Fa_OpCode::NOP);
}

TEST(CompilerBinary, ICSlotAllocatedPerBinaryOp)
{
    Fa_Chunk* chunk = compile_ok({ decl("x", AST::Fa_makeLiteralInt(1)), decl("y", AST::Fa_makeLiteralInt(2)),
        AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeName("y"), AST::Fa_BinaryOp::OP_ADD)),
        AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeName("y"), AST::Fa_BinaryOp::OP_MUL)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->ic_slots.size(), 2u);
}

TEST(CompilerBinary, EqualityLiteralsFolded)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_EQ)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_TRUE").op(Fa_OpCode::LOAD_TRUE);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, InequalityLiteralsFolded)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_NEQ)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_TRUE").op(Fa_OpCode::LOAD_TRUE);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, BitwiseAndFolded)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeLiteralInt(0b1100), AST::Fa_makeLiteralInt(0b1010), AST::Fa_BinaryOp::OP_BITAND)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 8").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(8));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, ShiftLeftFolded)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_LSHIFT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 8").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(8));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, LogicalAndShortCircuit)
{
    Fa_Chunk* chunk = compile_ok({ decl("a", AST::Fa_makeLiteralBool(true)), decl("b", AST::Fa_makeLiteralBool(false)),
        AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeName("a"), AST::Fa_makeName("b"), AST::Fa_BinaryOp::OP_AND)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("decl a").op(Fa_OpCode::LOAD_TRUE).A(0);
    bc.m_next("decl b").op(Fa_OpCode::LOAD_FALSE).A(1);
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
        { decl("a", AST::Fa_makeLiteralBool(false)), decl("b", AST::Fa_makeLiteralBool(true)),
            AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeName("a"), AST::Fa_makeName("b"), AST::Fa_BinaryOp::OP_OR)) });
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
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(Fa_makeBinary(AST::Fa_makeLiteralBool(true), AST::Fa_makeLiteralBool(true), AST::Fa_BinaryOp::OP_AND)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_FALSE(chunk->code.empty());
}

TEST(CompilerIf, SimpleIfNoElse)
{
    Fa_Chunk* chunk = compile_ok(Fa_makeIf(AST::Fa_makeName("x"), blk(decl("y", AST::Fa_makeLiteralInt(1)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); ++i) {
        if (Fa_instr_op(chunk->code[i]) == Fa_OpCode::JUMP_IF_FALSE)
            jif_pos = i;
    }
    ASSERT_GE(jif_pos, 0) << "expected JUMP_IF_FALSE";
    int m_target = jif_pos + 1 + Fa_instr_sBx(chunk->code[jif_pos]);
    EXPECT_EQ(Fa_instr_op(chunk->code[m_target]), Fa_OpCode::RETURN_NIL);
}

TEST(CompilerIf, IfElse)
{
    Fa_Chunk* chunk = compile_ok(Fa_makeIf(AST::Fa_makeName("x"), blk(decl("a", AST::Fa_makeLiteralInt(1))), blk(decl("b", AST::Fa_makeLiteralInt(2)))));
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
    for (int i = 0; i < (int)chunk->code.size(); ++i) {
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
    Fa_Chunk* chunk = compile_ok(Fa_makeIf(AST::Fa_makeLiteralBool(true), blk(decl("x", AST::Fa_makeLiteralInt(1))), blk(decl("y", AST::Fa_makeLiteralInt(999)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    for (auto& ins : chunk->code)
        EXPECT_NE(Fa_instr_op(ins), Fa_OpCode::JUMP_IF_FALSE) << "JUMP_IF_FALSE should not exist when condition is const-true";
    for (auto& v : chunk->constants)
        EXPECT_NE(Fa_AS_INTEGER(v), 999);
}

TEST(CompilerIf, ConstantFalseConditionDCE)
{
    Fa_Chunk* chunk = compile_ok(Fa_makeIf(AST::Fa_makeLiteralBool(false), blk(decl("x", AST::Fa_makeLiteralInt(1))), blk(decl("y", AST::Fa_makeLiteralInt(2)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 2").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(2));
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerIf, ConstantFalseNoElseEmitsNothing)
{
    Fa_Chunk* chunk = compile_ok(Fa_makeIf(AST::Fa_makeLiteralBool(false), blk(decl("x", AST::Fa_makeLiteralInt(1)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerWhile, BasicWhile)
{
    Fa_Chunk* chunk = compile_ok(Fa_makeWhile(AST::Fa_makeName("x"), blk(decl("a", AST::Fa_makeLiteralInt(1)))));
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
    Fa_Chunk* chunk = compile_ok(Fa_makeWhile(AST::Fa_makeLiteralBool(false), blk(decl("x", AST::Fa_makeLiteralInt(1)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerWhile, WhileTrueEmitsUnconditionalLoop)
{
    Fa_Chunk* chunk = compile_ok(Fa_makeWhile(AST::Fa_makeLiteralBool(true), blk()));
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
    Fa_Chunk* chunk = compile_ok(Fa_makeWhile(AST::Fa_makeName("cond"), blk(decl("x", AST::Fa_makeLiteralInt(0)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); ++i)
        if (Fa_instr_op(chunk->code[i]) == Fa_OpCode::JUMP_IF_FALSE)
            jif_pos = i;
    ASSERT_GE(jif_pos, 0);
    int m_target = jif_pos + 1 + Fa_instr_sBx(chunk->code[jif_pos]);
    ASSERT_LT(m_target, (int)chunk->code.size());
    EXPECT_EQ(Fa_instr_op(chunk->code[m_target]), Fa_OpCode::RETURN_NIL);
}

TEST(CompilerFor, ListIterationLowersToLoopBytecode)
{
    Fa_Chunk* chunk = compile_ok({
        decl("items", AST::Fa_makeList({ AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(3) })),
        decl("sum", AST::Fa_makeLiteralInt(0)),
        AST::Fa_makeFor(AST::Fa_makeName("item"),
            AST::Fa_makeName("items"),
            blk(assign_stmt("sum", AST::Fa_makeBinary(AST::Fa_makeName("sum"), AST::Fa_makeName("item"), AST::Fa_BinaryOp::OP_ADD))))
    });
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

TEST(CompilerReturn, ReturnNilEmitsReturnNil)
{
    Fa_Chunk* chunk = compile_ok(Fa_makeReturn(AST::Fa_makeLiteralNil()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerReturn, ReturnValueEmitsReturn)
{
    Fa_Chunk* chunk = compile_ok(Fa_makeReturn(AST::Fa_makeLiteralInt(42)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("LOAD_INT 42").op(Fa_OpCode::LOAD_INT).Bx(load_int_bx(42));
    bc.m_next("RETURN").op(Fa_OpCode::RETURN).B(1);
    bc.done();
}

TEST(CompilerReturn, ReturnIsDeadCodeBarrier)
{
    Fa_Chunk* chunk = compile_ok({ Fa_makeReturn(AST::Fa_makeLiteralInt(1)), decl("x", AST::Fa_makeLiteralInt(99)) });
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
    Fa_Chunk* chunk = compile_ok(func_stmt("wrapper", { }, blk(Fa_makeReturn(call(AST::Fa_makeName("f"))))));
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
    Fa_Chunk* chunk = compile_ok(func_stmt("foo", { }, blk()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.m_next("CLOSURE").op(Fa_OpCode::CLOSURE).A(0).Bx(0);
    bc.m_next("STORE_GLOBAL").op(Fa_OpCode::STORE_GLOBAL).A(0).Bx(0);
    bc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->functions.size(), 1u);
    Fa_Chunk const* fn = chunk->functions[0];
    EXPECT_EQ(fn->m_name, "foo");
    EXPECT_EQ(fn->arity, 0);
    BytecodeChecker fbc(*fn);
    fbc.m_next("RETURN_NIL").op(Fa_OpCode::RETURN_NIL);
    fbc.done();
}

TEST(CompilerFunc, FunctionWithParams)
{
    Fa_Chunk* chunk = compile_ok(func_stmt("add", { AST::Fa_makeName("a"), AST::Fa_makeName("b") },
        blk(Fa_makeReturn(Fa_makeBinary(AST::Fa_makeName("a"), AST::Fa_makeName("b"), AST::Fa_BinaryOp::OP_ADD)))));
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
    Fa_Chunk* chunk = compile_ok(func_stmt("foo", { }, blk()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->local_count, 1);
}

TEST(CompilerFunc, NestedFunctionIndexing)
{
    Fa_Chunk* chunk = compile_ok({ func_stmt("a", { }, blk()), func_stmt("b", { }, blk()) });
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
    Fa_Chunk* chunk = compile_ok(func_stmt("fact", { AST::Fa_makeName("n") },
        blk(Fa_makeIf(AST::Fa_makeName("n"), blk(Fa_makeReturn(AST::Fa_makeName("n")))), Fa_makeReturn(AST::Fa_makeLiteralInt(1)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    EXPECT_FALSE(chunk->functions[0]->code.empty());
}

TEST(CompilerClosure, CapturesLocalFromEnclosingScope)
{
    GTEST_SKIP() << "Upvalues are intentionally unsupported in the simplified compiler.";
    Fa_Chunk* chunk = compile_ok(func_stmt("outer", { }, blk(decl("x", AST::Fa_makeLiteralInt(1)), func_stmt("inner", { }, blk(Fa_makeReturn(AST::Fa_makeName("x")))))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    Fa_Chunk const* outer = chunk->functions[0];
    ASSERT_EQ(outer->functions.size(), 1u);
    Fa_Chunk const* inner = outer->functions[0];
    EXPECT_EQ(inner->upvalue_count, 1);
    bool has_get_uv = false;
    for (auto& ins : inner->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::GET_UPVALUE)
            has_get_uv = true;
    }
    EXPECT_TRUE(has_get_uv) << "inner should GET_UPVALUE for x";
    bool has_close_uv = false;
    for (auto& ins : outer->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::CLOSE_UPVALUE)
            has_close_uv = true;
    }
    EXPECT_TRUE(has_close_uv) << "outer should CLOSE_UPVALUE when x goes out of scope";
}

TEST(CompilerClosure, UpvalueDescriptorEmittedAfterClosure)
{
    GTEST_SKIP() << "Upvalues are intentionally unsupported in the simplified compiler.";
    Fa_Chunk* chunk = compile_ok(func_stmt("outer", { }, blk(decl("x", AST::Fa_makeLiteralInt(1)), func_stmt("inner", { }, blk(Fa_makeReturn(AST::Fa_makeName("x")))))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    Fa_Chunk const* outer = chunk->functions[0];
    int closure_pos = -1;
    for (int i = 0; i < (int)outer->code.size(); ++i) {
        if (Fa_instr_op(outer->code[i]) == Fa_OpCode::CLOSURE)
            closure_pos = i;
    }
    ASSERT_GE(closure_pos, 0) << "CLOSURE not found in outer";
    ASSERT_LT(closure_pos + 1, (int)outer->code.size());
    u32 desc = outer->code[closure_pos + 1];
    EXPECT_EQ(Fa_instr_op(desc), Fa_OpCode::MOVE) << "upvalue descriptor MOVE must follow CLOSURE";
    EXPECT_EQ(Fa_instr_A(desc), 1u) << "is_local=1 for direct local capture";
    EXPECT_EQ(Fa_instr_B(desc), 0u) << "captures r0 (x)";
}

TEST(CompilerCall, CallWithNoArgs)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(call(AST::Fa_makeName("f"))));
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
    m_args.push(AST::Fa_makeLiteralInt(1));
    m_args.push(AST::Fa_makeLiteralInt(2));
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(call(AST::Fa_makeName("f"), std::move(m_args))));
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
    Fa_Chunk* chunk = compile_ok(decl("r", call(AST::Fa_makeName("f"), [] {
        Fa_Array<AST::Fa_Expr*> a;
        a.push(AST::Fa_makeName("x"));
        return a;
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
    Fa_Chunk* chunk = compile_ok({ AST::Fa_makeExprStmt(call(AST::Fa_makeName("f"))), AST::Fa_makeExprStmt(call(AST::Fa_makeName("g"))) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->ic_slots.size(), 2u);
}

TEST(CompilerList, EmptyList)
{
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeList()));
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
    elems.push(AST::Fa_makeLiteralInt(1));
    elems.push(AST::Fa_makeLiteralInt(2));
    elems.push(AST::Fa_makeLiteralInt(3));
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeList(std::move(elems))));
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
    for (int i = 0; i < 300; ++i)
        elems.push(AST::Fa_makeLiteralInt(i));
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeList(std::move(elems))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_FALSE(chunk->code.empty());
    u32 new_list = chunk->code[0];
    EXPECT_EQ(Fa_instr_op(new_list), Fa_OpCode::LIST_NEW);
    EXPECT_EQ(Fa_instr_B(new_list), 255u) << "capacity hint must be capped at 255";
}

TEST(CompilerScope, LocalslDontLeakOutOfBlock)
{
    AST::Fa_BlockStmt* _ast = blk(AST::Fa_makeBlock({ decl("x", AST::Fa_makeLiteralInt(1)) }), decl("x", AST::Fa_makeLiteralInt(2)));
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
    Fa_Chunk* chunk = compile_ok({ decl("a", AST::Fa_makeLiteralInt(1)), AST::Fa_makeBlock([] {
                                      Fa_Array<AST::Fa_Stmt*> s;
                                      s.push(decl("b", AST::Fa_makeLiteralInt(2)));
                                      s.push(decl("c", Fa_makeBinary(AST::Fa_makeName("a"), AST::Fa_makeName("b"), AST::Fa_BinaryOp::OP_ADD)));
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
    Fa_Chunk* chunk = compile_ok(blk());
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->m_name, "<main>");
}

TEST(CompilerMeta, LocalCountReflectsMaxRegisters)
{
    Fa_Chunk* chunk = compile_ok({ decl("a", AST::Fa_makeLiteralInt(1)), decl("b", AST::Fa_makeLiteralInt(2)), decl("c", AST::Fa_makeLiteralInt(3)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->local_count, 3);
}

TEST(CompilerMeta, FunctionAritySetCorrectly)
{
    Fa_Chunk* chunk = compile_ok(func_stmt("f", { AST::Fa_makeName("x"), AST::Fa_makeName("y"), AST::Fa_makeName("z") }, blk()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->arity, 3);
}

TEST(CompilerMeta, FunctionNameSetCorrectly)
{
    Fa_Chunk* chunk = compile_ok(func_stmt("compute", { }, blk()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->m_name, "compute");
}

TEST(CompilerMeta, LineInfoPresent)
{
    Fa_Chunk* chunk = compile_ok(decl("x", AST::Fa_makeLiteralInt(42), false, /*line=*/7));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_FALSE(chunk->lines.empty());
}

TEST(CompilerIntegration, Fibonacci)
{
    Fa_Array<AST::Fa_Expr*> args_n1, args_n2;
    args_n1.push(Fa_makeBinary(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_SUB));
    args_n2.push(Fa_makeBinary(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_SUB));

    Fa_Chunk* chunk = compile_ok(
        func_stmt("fib", { AST::Fa_makeName("n") },
            blk(Fa_makeIf(Fa_makeBinary(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(1),
                              AST::Fa_BinaryOp::OP_LTE),
                    blk(Fa_makeReturn(AST::Fa_makeName("n")))),
                Fa_makeReturn(Fa_makeBinary(call(AST::Fa_makeName("fib"), std::move(args_n1)),
                    call(AST::Fa_makeName("fib"), std::move(args_n2)), AST::Fa_BinaryOp::OP_ADD)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_FALSE(chunk->functions.empty());
    Fa_Chunk const* fib = chunk->functions[0];
    EXPECT_EQ(fib->m_name, "fib");
    EXPECT_EQ(fib->arity, 1);
    EXPECT_FALSE(fib->code.empty());
    EXPECT_GE(fib->ic_slots.size(), 2u);
}

TEST(CompilerIntegration, CounterWithClosure)
{
    GTEST_SKIP() << "Upvalues are intentionally unsupported in the simplified compiler.";
    Fa_Chunk* chunk = compile_ok(func_stmt("make_counter", { },
        blk(decl("count", AST::Fa_makeLiteralInt(0)),
            func_stmt("inc", { }, blk(assign_stmt("count", Fa_makeBinary(AST::Fa_makeName("count"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD)))),
            Fa_makeReturn(AST::Fa_makeName("inc")))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    Fa_Chunk const* mc = chunk->functions[0];
    ASSERT_EQ(mc->functions.size(), 1u);
    Fa_Chunk const* inc = mc->functions[0];

    bool has_set_uv = false;
    for (auto& ins : inc->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::SET_UPVALUE)
            has_set_uv = true;
    }
    EXPECT_TRUE(has_set_uv) << "inc() should SET_UPVALUE for count";
    EXPECT_EQ(inc->upvalue_count, 1);
}

TEST(CompilerIntegration, NestedArithmetic)
{
    Fa_Chunk* chunk = compile_ok(decl("result",
        Fa_makeBinary(Fa_makeBinary(AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(3), AST::Fa_BinaryOp::OP_ADD),
            Fa_makeBinary(AST::Fa_makeLiteralInt(4), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_SUB), AST::Fa_BinaryOp::OP_MUL)));
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
    Fa_Chunk* chunk = compile_ok({ AST::Fa_makeExprStmt(AST::Fa_makeLiteralString("hello")),
        AST::Fa_makeExprStmt(AST::Fa_makeLiteralString("hello")), AST::Fa_makeExprStmt(AST::Fa_makeLiteralString("hello")) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    int count = 0;
    for (auto& v : chunk->constants) {
        if (Fa_IS_STRING(v) && Fa_AS_STRING(v)->str == "hello")
            ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST(CompilerIntegration, MixedLiteralsInList)
{
    Fa_Array<AST::Fa_Expr*> elems;
    elems.push(AST::Fa_makeLiteralBool(true));
    elems.push(AST::Fa_makeLiteralInt(42));
    elems.push(AST::Fa_makeLiteralFloat(3.14));
    elems.push(AST::Fa_makeLiteralString("hi"));
    Fa_Chunk* chunk = compile_ok(AST::Fa_makeExprStmt(AST::Fa_makeList(std::move(elems))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    int appends = 0;
    for (auto& ins : chunk->code) {
        if (Fa_instr_op(ins) == Fa_OpCode::LIST_APPEND)
            ++appends;
    }
    EXPECT_EQ(appends, 4);
}
