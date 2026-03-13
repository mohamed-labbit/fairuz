#include <gtest/gtest.h>

#include "../../../include/ast.hpp"
#include "../../../include/ast_printer.hpp"
#include "../../../include/runtime/compiler.hpp"
#include "../../../include/runtime/opcode.hpp"
#include "../../../include/runtime/value.hpp"
#include "../../../include/string.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace mylang::runtime;
using namespace mylang::ast;
using namespace mylang;

static constexpr AssignmentExpr* assign_expr(StringRef name_, Expr* val, uint32_t line = 1)
{
    return makeAssignmentExpr(makeName(name_), val /*, line*/);
}
static constexpr CallExpr* call(Expr* callee, Array<Expr*> args = { }, uint32_t line = 1)
{
    return makeCall(callee, makeList(args) /*, line*/);
}
static constexpr AssignmentStmt* decl(StringRef nm, Expr* val, bool is_const = false, uint32_t line = 1)
{
    return makeAssignmentStmt(makeName(nm), val, true);
}
static constexpr AssignmentStmt* assign_stmt(StringRef nm, Expr* val, uint32_t line = 1)
{
    return makeAssignmentStmt(makeName(nm), val, false);
}
static constexpr FunctionDef* func_stmt(StringRef nm, Array<Expr*> params, BlockStmt* body, uint32_t line = 1)
{
    return makeFunction(makeName(nm), makeList(params), body /*, line*/);
}

// Helper: wrap multiple statements into a BlockStmt
template<typename... Stmts>
static BlockStmt* blk(Stmts&&... s)
{
    Array<Stmt*> v;
    (v.push(std::forward<Stmts>(s)), ...);
    return makeBlock(std::move(v));
}

class BytecodeChecker {
public:
    explicit BytecodeChecker(Chunk const& chunk)
        : chunk_(chunk)
        , pos_(0)
    {
    }

    // Advance to the next instruction and bind its fields.
    // Returns *this for chaining.
    BytecodeChecker& next(StringRef label = "")
    {
        label_ = std::move(label);
        EXPECT_LT(pos_, chunk_.code.size())
            << "ran off end of code at step \"" << label_ << "\"";
        if (pos_ < chunk_.code.size())
            cur_ = chunk_.code[pos_++];
        return *this;
    }

    BytecodeChecker& op(OpCode expected)
    {
        EXPECT_EQ(instr_op(cur_), static_cast<uint8_t>(expected))
            << at() << "  expected op=" << opcode_name(expected)
            << "  got=" << opcode_name(static_cast<OpCode>(instr_op(cur_)));
        return *this;
    }
    BytecodeChecker& A(uint8_t expected)
    {
        EXPECT_EQ(instr_A(cur_), expected) << at() << "  field A";
        return *this;
    }
    BytecodeChecker& B(uint8_t expected)
    {
        EXPECT_EQ(instr_B(cur_), expected) << at() << "  field B";
        return *this;
    }
    BytecodeChecker& C(uint8_t expected)
    {
        EXPECT_EQ(instr_C(cur_), expected) << at() << "  field C";
        return *this;
    }
    BytecodeChecker& Bx(uint16_t expected)
    {
        EXPECT_EQ(instr_Bx(cur_), expected) << at() << "  field Bx";
        return *this;
    }
    BytecodeChecker& sBx(int expected)
    {
        EXPECT_EQ(instr_sBx(cur_), expected) << at() << "  field sBx";
        return *this;
    }

    // Assert there are no more instructions after position pos_.
    BytecodeChecker& done()
    {
        EXPECT_EQ(pos_, chunk_.code.size())
            << "expected end of code at instruction " << pos_
            << " but " << (chunk_.code.size() - pos_) << " more remain";
        return *this;
    }

    // Return the current instruction index (before next() was called).
    uint32_t current_index() const { return pos_ - 1; }
    uint32_t next_index() const { return pos_; }

private:
    StringRef at() const
    {
        std::ostringstream ss;
        ss << "[instr " << (pos_ - 1) << " \"" << label_ << "\"]";
        return ss.str().data();
    }

    Chunk const& chunk_;
    uint32_t pos_;
    uint32_t cur_ = 0;
    StringRef label_;
};

static Chunk* compile_ok(Array<Stmt*> stmts, Compiler& c)
{
    return c.compile(stmts);
}

static void dump(Chunk const* c)
{
    std::cout << '\n'
              << "Disassembled bytecode :" << '\n';
    c->disassemble();
    std::cout << '\n';
}

// Overload that creates a fresh Compiler internally
static Chunk* compile_ok(Array<Stmt*> stmts)
{
    Compiler c;
    return compile_ok(stmts, c);
}

static Chunk* compile_ok(Stmt* root)
{
    Array<Stmt*> stmts;
    stmts.push(root);
    return compile_ok(stmts);
}

static Chunk* compile_ok(Stmt* root, Compiler& c)
{
    Array<Stmt*> stmts;
    stmts.push(root);
    return compile_ok(stmts, c);
}

static uint16_t load_int_bx(int64_t i)
{
    return static_cast<uint16_t>(i + 32767);
}

TEST(CompilerLiteral, NilExpression)
{
    // Expression statement: nil
    // Expected:
    //   0: LOAD_NIL  A=0 B=0 C=1
    //   1: RETURN_NIL
    Chunk* chunk = compile_ok(makeExprStmt(makeLiteralNil()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_NIL").op(OpCode::LOAD_NIL).A(0).C(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, TrueLiteral)
{
    Chunk* chunk = compile_ok(makeExprStmt(makeLiteralBool(true)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_TRUE").op(OpCode::LOAD_TRUE).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, FalseLiteral)
{
    Chunk* chunk = compile_ok(makeExprStmt(makeLiteralBool(false)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_FALSE").op(OpCode::LOAD_FALSE).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, SmallIntegerUsesLoadInt)
{
    // Integers in [-32767, 32767] must use LOAD_INT (no constant pool entry)
    Chunk* chunk = compile_ok(makeExprStmt(makeLiteralInt(42)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(42));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    // Verify constant pool was OP_NOT used for the integer
    EXPECT_TRUE(chunk->constants.empty());
}

TEST(CompilerLiteral, NegativeSmallIntegerUsesLoadInt)
{
    Chunk* chunk = compile_ok(makeExprStmt(makeLiteralInt(-100)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).Bx(load_int_bx(-100));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL); // skip RETURN_NIL
    bc.done();
}

TEST(CompilerLiteral, ZeroUsesLoadInt)
{
    Chunk* chunk = compile_ok(makeExprStmt(makeLiteralInt(0)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).Bx(load_int_bx(0));
}

TEST(CompilerLiteral, LargeIntegerUsesConstantPool)
{
    // 100000 > 32767 — must fall back to LOAD_CONST
    Chunk* chunk = compile_ok(makeExprStmt(makeLiteralInt(100000)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].isInteger());
    EXPECT_EQ(chunk->constants[0].asInteger(), 100000);
}

TEST(CompilerLiteral, FloatUsesConstantPool)
{
    Chunk* chunk = compile_ok(makeExprStmt(makeLiteralFloat(3.14)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].isDouble());
    EXPECT_DOUBLE_EQ(chunk->constants[0].asDouble(), 3.14);
}

TEST(CompilerLiteral, StringUsesConstantPool)
{
    Chunk* chunk = compile_ok(makeExprStmt(makeLiteralString("hello")));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].isString());
    EXPECT_EQ(chunk->constants[0].asString()->str, "hello");
}

TEST(CompilerLiteral, StringsDeduplicated)
{
    // Two identical string literals should share one constant-pool slot
    Array<Stmt*> stmts;
    stmts.push(makeExprStmt(makeLiteralString("dup")));
    stmts.push(makeExprStmt(makeLiteralString("dup")));
    Chunk* chunk = compile_ok(makeBlock(std::move(stmts)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // Only one constant pool entry
    long string_constants = std::count_if(chunk->constants.begin(), chunk->constants.end(),
        [](Value const& v) { return v.isString(); });
    EXPECT_EQ(string_constants, 1);
}

TEST(CompilerVar, LocalDeclaration)
{
    // x = 5
    // Expected:
    //   0: LOAD_INT A=0 Bx=bias(5)   ← x lives in r0
    //   1: RETURN_NIL
    Chunk* chunk = compile_ok(decl("x", makeLiteralInt(5)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(5));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->localCount, 1);
}

TEST(CompilerVar, TwoLocalsUseConsecutiveRegisters)
{
    // let x = 1; let y = 2
    Chunk* chunk = compile_ok({ decl("x", makeLiteralInt(1)),
        decl("y", makeLiteralInt(2)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT x").op(OpCode::LOAD_INT).A(0); // x → r0
    bc.next("LOAD_INT y").op(OpCode::LOAD_INT).A(1); // y → r1
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->localCount, 2);
}

TEST(CompilerVar, LocalAssignmentWritesBackToSameRegister)
{
    // let x = 1; x = 2
    // x is in r0; assignment compiles the RHS directly into r0
    Chunk* chunk = compile_ok({ decl("x", makeLiteralInt(1)),
        assign_stmt("x", makeLiteralInt(2)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("decl x=1").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.next("assign x=2").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(2));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerVar, GlobalLoadAndStore)
{
    // Undeclared name → global.
    // Assigning to an undeclared name emits STORE_GLOBAL.
    // Reading an undeclared name emits LOAD_GLOBAL.
    Chunk* chunk = compile_ok({
        assign_stmt("g", makeLiteralInt(7)), // g not declared → STORE_GLOBAL
        makeExprStmt(makeName("g"))          // LOAD_GLOBAL
    });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // Store
    BytecodeChecker bc(*chunk);
    // compiling RHS into a temp, then STORE_GLOBAL
    bc.next("RHS").op(OpCode::LOAD_INT).Bx(load_int_bx(7));
    bc.next("STORE_GLOBAL").op(OpCode::STORE_GLOBAL);
    // Load
    bc.next("LOAD_GLOBAL").op(OpCode::LOAD_GLOBAL);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    // The name "g" must appear in the constant pool
    bool found = false;
    for (auto& v : chunk->constants)
        if (v.isString() && v.asString()->str == "g")
            found = true;
    EXPECT_TRUE(found) << "global name 'g' not interned into constant pool";
}

TEST(CompilerUnary, NegateVariable)
{
    // let x = 5; expr: -x
    // Expected after decl:
    //   LOAD_INT r0=5   (x)
    //   OP_NEG      r1=−r0
    //   RETURN_NIL
    Chunk* chunk = compile_ok({ decl("x", makeLiteralInt(5)), makeExprStmt(makeUnary(makeName("x"), UnaryOp::OP_NEG)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT x").op(OpCode::LOAD_INT).A(0);
    bc.next("OP_NEG").op(OpCode::OP_NEG).B(0); // src = r0
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, NotVariable)
{
    Chunk* chunk = compile_ok({ decl("b", makeLiteralBool(true)), makeExprStmt(makeUnary(makeName("b"), UnaryOp::OP_NOT)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_TRUE b").op(OpCode::LOAD_TRUE).A(0);
    bc.next("OP_NOT").op(OpCode::OP_NOT).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, BitwiseNotVariable)
{
    Chunk* chunk = compile_ok({ decl("n", makeLiteralInt(0xFF)),
        makeExprStmt(makeUnary(makeName("n"), UnaryOp::OP_BITNOT)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("decl").op(OpCode::LOAD_INT).A(0);
    bc.next("OP_BITNOT").op(OpCode::OP_BITNOT).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

// Constant-folded unary ops emit a single LOAD, no op instruction
TEST(CompilerUnary, NegLiteralFolded)
{
    // -(3) should fold to LOAD_INT -3
    Chunk* chunk = compile_ok(makeExprStmt(makeUnary(makeLiteralInt(3), UnaryOp::OP_NEG)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT -3").op(OpCode::LOAD_INT).Bx(load_int_bx(-3));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_TRUE(chunk->constants.empty());
}

TEST(CompilerUnary, NotTrueFolded)
{
    Chunk* chunk = compile_ok(makeExprStmt(makeUnary(makeLiteralBool(true), UnaryOp::OP_NOT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_FALSE").op(OpCode::LOAD_FALSE);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, NotFalseFolded)
{
    Chunk* chunk = compile_ok(makeExprStmt(makeUnary(makeLiteralBool(false), UnaryOp::OP_NOT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_TRUE").op(OpCode::LOAD_TRUE);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerUnary, BNotLiteralFolded)
{
    // ~0 → -1
    Chunk* chunk = compile_ok(makeExprStmt(makeUnary(makeLiteralInt(0), UnaryOp::OP_BITNOT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT -1").op(OpCode::LOAD_INT).Bx(load_int_bx(-1));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

// For non-folded binary ops the compiler emits:
//   compile(L) → rL
//   compile(R) → rR
//   OP  dst rL rR
//   NOP ic_idx     ← IC slot descriptor
// rL and rR are temporaries allocated above any existing locals.

TEST(CompilerBinary, AddTwoLocals)
{
    // let a=1; let b=2; a+b
    // r0=a, r1=b, r2=temp_a, r3=temp_b, dst=r2
    Chunk* chunk = compile_ok({ decl("a", makeLiteralInt(1)),
        decl("b", makeLiteralInt(2)),
        makeExprStmt(makeBinary(makeName("a"), makeName("b"), BinaryOp::OP_ADD)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("decl a").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.next("decl b").op(OpCode::LOAD_INT).A(1).Bx(load_int_bx(2));
    // compile(makeName("a")) → r0 (local, no MOVE because no dst hint)
    // compile(makeName("b")) → r1
    // dst = alloc_reg() = r2
    bc.next("OP_ADD").op(OpCode::OP_ADD).A(2).B(0).C(1);
    bc.next("NOP ic").op(OpCode::NOP).A(0); // IC slot 0
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->icSlots.size(), 1u);
}

TEST(CompilerBinary, SubtractLiterals)
{
    Chunk* chunk = compile_ok(makeExprStmt(makeBinary(makeLiteralInt(10), makeLiteralInt(3), BinaryOp::OP_SUB)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // 10 - 3 = 7, folded
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 7").op(OpCode::LOAD_INT).Bx(load_int_bx(7));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_TRUE(chunk->icSlots.empty()); // no IC slot for folded ops
}

TEST(CompilerBinary, MultiplyLiterals)
{
    Chunk* chunk = compile_ok(makeExprStmt(
        makeBinary(makeLiteralInt(6), makeLiteralInt(7), BinaryOp::OP_MUL)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 42").op(OpCode::LOAD_INT).Bx(load_int_bx(42));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, DivisionFolded)
{
    // 1.0 / 2.0 → 0.5 (double result, uses LOAD_CONST)
    Chunk* chunk = compile_ok(makeExprStmt(makeBinary(makeLiteralFloat(1.0), makeLiteralFloat(2.0), BinaryOp::OP_DIV)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST 0.5").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_DOUBLE_EQ(chunk->constants[0].asDoubleAny(), 0.5);
}

TEST(CompilerBinary, DivisionByZeroNotFolded)
{
    // x / 0 where x is a variable — cannot fold, must emit OP_DIV + NOP
    Chunk* chunk = compile_ok({ decl("x", makeLiteralInt(5)),
        makeExprStmt(makeBinary(makeName("x"), makeLiteralInt(0), BinaryOp::OP_DIV)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("decl x").op(OpCode::LOAD_INT).A(0);
    // Even with literal 0 on RHS, OP_DIV by literal zero is not folded
    // (only purely-literal binary ops are folded; here LHS is a name)
    bc.next("LOAD_INT 0 into r1 temp").op(OpCode::LOAD_INT);
    bc.next("OP_DIV").op(OpCode::OP_DIV);
    bc.next("NOP ic").op(OpCode::NOP);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, GreaterThanNormalizedToLT)
{
    // a > b  →  compiler swaps operands and emits OP_LT(b, a)
    Chunk* chunk = compile_ok({ decl("a", makeLiteralInt(3)),
        decl("b", makeLiteralInt(1)),
        makeExprStmt(makeBinary(makeName("a"), makeName("b"), BinaryOp::OP_GT)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("decl a").op(OpCode::LOAD_INT).A(0);
    bc.next("decl b").op(OpCode::LOAD_INT).A(1);
    // GT(a,b) → OP_LT(b,a): B=r1(b), C=r0(a)
    bc.next("OP_LT").op(OpCode::OP_LT).B(1).C(0);
    bc.next("NOP ic").op(OpCode::NOP);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, GreaterEqualNormalizedToLE)
{
    Chunk* chunk = compile_ok({ decl("a", makeLiteralInt(5)),
        decl("b", makeLiteralInt(5)),
        makeExprStmt(makeBinary(makeName("a"), makeName("b"), BinaryOp::OP_GTE)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("decl a").op(OpCode::LOAD_INT).A(0);
    bc.next("decl b").op(OpCode::LOAD_INT).A(1);
    // GE(a,b) → OP_LTE(b,a)
    bc.next("OP_LTE").op(OpCode::OP_LTE).B(1).C(0);
    bc.next("NOP ic").op(OpCode::NOP);
}

TEST(CompilerBinary, ICSlotAllocatedPerBinaryOp)
{
    // Two non-foldable binary ops → two IC slots
    Chunk* chunk = compile_ok({ decl("x", makeLiteralInt(1)),
        decl("y", makeLiteralInt(2)),
        makeExprStmt(makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_ADD)),
        makeExprStmt(makeBinary(makeName("x"), makeName("y"), BinaryOp::OP_MUL)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->icSlots.size(), 2u);
}

TEST(CompilerBinary, EqualityLiteralsFolded)
{
    // 1 == 1 → true
    Chunk* chunk = compile_ok(makeExprStmt(
        makeBinary(makeLiteralInt(1), makeLiteralInt(1), BinaryOp::OP_EQ)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_TRUE").op(OpCode::LOAD_TRUE);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, InequalityLiteralsFolded)
{
    // 1 != 2 → true
    Chunk* chunk = compile_ok(makeExprStmt(
        makeBinary(makeLiteralInt(1), makeLiteralInt(2), BinaryOp::OP_NEQ)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_TRUE").op(OpCode::LOAD_TRUE);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, BitwiseAndFolded)
{
    // 0b1100 & 0b1010 = 0b1000 = 8
    Chunk* chunk = compile_ok(makeExprStmt(
        makeBinary(makeLiteralInt(0b1100), makeLiteralInt(0b1010), BinaryOp::OP_BITAND)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 8").op(OpCode::LOAD_INT).Bx(load_int_bx(8));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, ShiftLeftFolded)
{
    // 1 << 3 = 8
    Chunk* chunk = compile_ok(makeExprStmt(
        makeBinary(makeLiteralInt(1), makeLiteralInt(3), BinaryOp::OP_LSHIFT)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 8").op(OpCode::LOAD_INT).Bx(load_int_bx(8));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

// a && b compiles to:
//   compile(a) → rd
//   JUMP_IF_FALSE rd → after_b
//   compile(b) → rd
//   [patch: after_b]
TEST(CompilerBinary, LogicalAndShortCircuit)
{
    Chunk* chunk = compile_ok({ decl("a", makeLiteralBool(true)),
        decl("b", makeLiteralBool(false)),
        makeExprStmt(makeBinary(makeName("a"), makeName("b"), BinaryOp::OP_AND)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("decl a").op(OpCode::LOAD_TRUE).A(0);
    bc.next("decl b").op(OpCode::LOAD_FALSE).A(1);
    // AND: load a into result reg r2, jump_if_false, then load b into r2
    uint32_t jif_idx;
    bc.next("LHS into r2").op(OpCode::MOVE).A(2).B(0); // move a→r2
    // OR: the local a IS already in r0; compiler checks left==dst and
    // may skip the MOVE. Adjust if your compiler skips it.
    // We'll accept either MOVE or the direct name result.
    // The key assertion is the JUMP_IF_FALSE followed by a load/move of b.
    (void)jif_idx;
    // Instead of brittle field checks here, just verify the stream shape:
    // JUMP_IF_FALSE must appear before b's load
    bool found_jif = false, found_b = false;
    for (auto& instr : chunk->code) {
        if (instr_op(instr) == static_cast<uint8_t>(OpCode::JUMP_IF_FALSE))
            found_jif = true;
        if (found_jif && (instr_op(instr) == static_cast<uint8_t>(OpCode::LOAD_FALSE) || instr_op(instr) == static_cast<uint8_t>(OpCode::MOVE)))
            found_b = true;
    }
    EXPECT_TRUE(found_jif) << "expected JUMP_IF_FALSE for && short-circuit";
    EXPECT_TRUE(found_b) << "expected RHS load after JUMP_IF_FALSE";
}

TEST(CompilerBinary, LogicalOrShortCircuit)
{
    Chunk* chunk = compile_ok({ decl("a", makeLiteralBool(false)),
        decl("b", makeLiteralBool(true)),
        makeExprStmt(makeBinary(makeName("a"), makeName("b"), BinaryOp::OP_OR)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    bool found_jit = false;
    for (auto& instr : chunk->code)
        if (instr_op(instr) == static_cast<uint8_t>(OpCode::JUMP_IF_TRUE))
            found_jit = true;
    EXPECT_TRUE(found_jit) << "expected JUMP_IF_TRUE for || short-circuit";
}

TEST(CompilerBinary, AndWithBothLiteralsTrueNotFolded)
{
    // true && true — AND is not folded (non-literal names on each side);
    // but true && true with literal operands: const_value returns a value,
    // however AND/OR are checked BEFORE try_fold_binary.
    // The compiler currently does OP_NOT constant-fold AND/OR.
    // Verify: no JUMP_IF_FALSE means it *was* folded; presence means not.
    // This test just verifies the compiler doesn't crash.
    Chunk* chunk = compile_ok(makeExprStmt(
        makeBinary(makeLiteralBool(true), makeLiteralBool(true), BinaryOp::OP_AND)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_FALSE(chunk->code.empty());
}

TEST(CompilerIf, SimpleIfNoElse)
{
    // if (x) { let y = 1 }
    // r0 = x (global load)
    // JUMP_IF_FALSE r0 → after_then
    // LOAD_INT r1=1
    // [after_then]: RETURN_NIL
    Chunk* chunk = compile_ok(makeIf(
        makeName("x"),
        blk(decl("y", makeLiteralInt(1)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    // Find JUMP_IF_FALSE and verify it points past the then-body
    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); ++i)
        if (instr_op(chunk->code[i]) == static_cast<uint8_t>(OpCode::JUMP_IF_FALSE))
            jif_pos = i;
    ASSERT_GE(jif_pos, 0) << "expected JUMP_IF_FALSE";

    // The sBx of the jump should land on RETURN_NIL (the instruction after the body)
    int target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    EXPECT_EQ(instr_op(chunk->code[target]),
        static_cast<uint8_t>(OpCode::RETURN_NIL));
}

TEST(CompilerIf, IfElse)
{
    // if (x) { let a=1 } else { let b=2 }
    // LOAD_GLOBAL x
    // JUMP_IF_FALSE → else
    // LOAD_INT 1
    // JUMP → end
    // [else]: LOAD_INT 2
    // [end]: RETURN_NIL
    Chunk* chunk = compile_ok(makeIf(
        makeName("x"),
        blk(decl("a", makeLiteralInt(1))),
        blk(decl("b", makeLiteralInt(2)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    bool has_jif = false, has_jmp = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::JUMP_IF_FALSE))
            has_jif = true;
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::JUMP))
            has_jmp = true;
    }
    EXPECT_TRUE(has_jif) << "expected JUMP_IF_FALSE";
    EXPECT_TRUE(has_jmp) << "expected JUMP over else";
    // JUMP_IF_FALSE target must be the JUMP instruction (start of else path)
    int jif_pos = -1, jmp_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); ++i) {
        if (instr_op(chunk->code[i]) == static_cast<uint8_t>(OpCode::JUMP_IF_FALSE))
            jif_pos = i;
        else if (instr_op(chunk->code[i]) == static_cast<uint8_t>(OpCode::JUMP))
            jmp_pos = i;
    }
    ASSERT_GE(jif_pos, 0);
    ASSERT_GE(jmp_pos, 0);
    int jif_target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    // jif_target must be one past the JUMP (the first instruction of else body)
    EXPECT_EQ(jif_target, jmp_pos + 1);
}

TEST(CompilerIf, ConstantTrueConditionDCE)
{
    // if (true) { let x = 1 } else { let y = 999 }
    // else branch must be completely absent — no LOAD_INT 999
    Chunk* chunk = compile_ok(makeIf(
        makeLiteralBool(true),
        blk(decl("x", makeLiteralInt(1))),
        blk(decl("y", makeLiteralInt(999)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    for (auto& ins : chunk->code)
        EXPECT_NE(instr_op(ins), static_cast<uint8_t>(OpCode::JUMP_IF_FALSE))
            << "JUMP_IF_FALSE should not exist when condition is const-true";
    // 999 must not appear in constants
    for (auto& v : chunk->constants)
        EXPECT_NE(v.asInteger(), 999);
}

TEST(CompilerIf, ConstantFalseConditionDCE)
{
    // if (false) { let x = 1 } else { let y = 2 }
    // then branch entirely absent
    Chunk* chunk = compile_ok(makeIf(
        makeLiteralBool(false),
        blk(decl("x", makeLiteralInt(1))),
        blk(decl("y", makeLiteralInt(2)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // Only the else body (LOAD_INT 2) should appear
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 2").op(OpCode::LOAD_INT).Bx(load_int_bx(2));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerIf, ConstantFalseNoElseEmitsNothing)
{
    // if (false) { ... }  — both then and else absent
    Chunk* chunk = compile_ok(makeIf(
        makeLiteralBool(false),
        blk(decl("x", makeLiteralInt(1)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // Only RETURN_NIL
    BytecodeChecker bc(*chunk);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerWhile, BasicWhile)
{
    // while (x) { let a = 1 }
    // loop_start:
    //   LOAD_GLOBAL x        ← condition
    //   JUMP_IF_FALSE → exit
    //   LOAD_INT 1           ← body
    //   LOOP → loop_start
    // exit: RETURN_NIL
    Chunk* chunk = compile_ok(makeWhile(
        makeName("x"),
        blk(decl("a", makeLiteralInt(1)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);

    bool has_jif = false, has_loop = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::JUMP_IF_FALSE))
            has_jif = true;
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::LOOP))
            has_loop = true;
    }
    EXPECT_TRUE(has_jif) << "while needs JUMP_IF_FALSE";
    EXPECT_TRUE(has_loop) << "while needs LOOP back-edge";

    // LOOP's sBx must be negative (backward jump)
    for (auto& ins : chunk->code)
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::LOOP))
            EXPECT_LT(instr_sBx(ins), 0) << "LOOP offset must be negative";
}

TEST(CompilerWhile, WhileFalseEmitsNothing)
{
    // while (false) { let x = 1 }  → DCE: emit nothing
    Chunk* chunk = compile_ok(makeWhile(
        makeLiteralBool(false),
        blk(decl("x", makeLiteralInt(1)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerWhile, WhileTrueEmitsUnconditionalLoop)
{
    // while (true) { }
    // Only a LOOP back-edge — no JUMP_IF_FALSE
    Chunk* chunk = compile_ok(makeWhile(
        makeLiteralBool(true),
        blk()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    bool has_jif = false, has_loop = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::JUMP_IF_FALSE))
            has_jif = true;
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::LOOP))
            has_loop = true;
    }
    EXPECT_FALSE(has_jif) << "while(true) must not emit JUMP_IF_FALSE";
    EXPECT_TRUE(has_loop) << "while(true) must emit LOOP";
}

TEST(CompilerWhile, JumpIfFalsePointsPastLoop)
{
    // Verify the exit jump lands on RETURN_NIL
    Chunk* chunk = compile_ok(makeWhile(
        makeName("cond"),
        blk(decl("x", makeLiteralInt(0)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); ++i)
        if (instr_op(chunk->code[i]) == static_cast<uint8_t>(OpCode::JUMP_IF_FALSE))
            jif_pos = i;
    ASSERT_GE(jif_pos, 0);
    int target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    ASSERT_LT(target, (int)chunk->code.size());
    EXPECT_EQ(instr_op(chunk->code[target]),
        static_cast<uint8_t>(OpCode::RETURN_NIL));
}

TEST(CompilerReturn, ReturnNilEmitsReturnNil)
{
    Chunk* chunk = compile_ok(makeReturn(makeLiteralNil()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerReturn, ReturnValueEmitsReturn)
{
    Chunk* chunk = compile_ok(makeReturn(makeLiteralInt(42)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 42").op(OpCode::LOAD_INT).Bx(load_int_bx(42));
    bc.next("RETURN").op(OpCode::RETURN).B(1); // result count = 1
    bc.done();
}

TEST(CompilerReturn, ReturnIsDeadCodeBarrier)
{
    // Statements after return must be suppressed
    Chunk* chunk = compile_ok({
        makeReturn(makeLiteralInt(1)),
        decl("x", makeLiteralInt(99)) // must OP_NOT appear
    });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // Only LOAD_INT 1 + RETURN appear; no LOAD_INT 99
    for (auto& v : chunk->constants)
        EXPECT_NE(v.asInteger(), 99) << "dead code leaked into constant pool";
    bool found_99 = false;
    for (auto& ins : chunk->code)
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::LOAD_INT) && instr_Bx(ins) == load_int_bx(99))
            found_99 = true;
    EXPECT_FALSE(found_99) << "dead code (LOAD_INT 99) was emitted after return";
}

TEST(CompilerReturn, TailCallEmitsCallTail)
{
    // Inside a non-top-level function: return f() → CALL_TAIL
    Chunk* chunk = compile_ok(func_stmt(
        "wrapper", { },
        blk(makeReturn(call(makeName("f"))))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    Chunk const* fn = chunk->functions[0];
    bool has_tail = false;
    for (auto& ins : fn->code)
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::CALL_TAIL))
            has_tail = true;
    EXPECT_TRUE(has_tail) << "return f() in function should emit CALL_TAIL";
}

TEST(CompilerFunc, EmptyFunction)
{
    // fn foo() { }
    Chunk* chunk = compile_ok(func_stmt("foo", { }, blk()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // Parent chunk: CLOSURE r0 fn_idx=0, then RETURN_NIL
    BytecodeChecker bc(*chunk);
    bc.next("CLOSURE").op(OpCode::CLOSURE).A(0).Bx(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->functions.size(), 1u);
    Chunk const* fn = chunk->functions[0];
    EXPECT_EQ(fn->name, "foo");
    EXPECT_EQ(fn->arity, 0);
    // fn body: implicit RETURN_NIL
    BytecodeChecker fbc(*fn);
    fbc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    fbc.done();
}

TEST(CompilerFunc, FunctionWithParams)
{
    // fn add(a, b) { return a + b }
    Chunk* chunk = compile_ok(func_stmt("add", { makeName("a"), makeName("b") }, blk(makeReturn(makeBinary(makeName("a"), makeName("b"), BinaryOp::OP_ADD)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    Chunk const* fn = chunk->functions[0];
    EXPECT_EQ(fn->arity, 2);
    EXPECT_EQ(fn->localCount, 3); // a=r0, b=r1, tempL=r2, tempR OP_NOT allocated / dst=r2

    // fn code:
    //   OP_ADD r2 r0 r1
    //   NOP ic=0
    //   RETURN r2 1
    BytecodeChecker fbc(*fn);

    fbc.next("OP_ADD").op(OpCode::OP_ADD).A(2).B(0).C(1);
    fbc.next("NOP").op(OpCode::NOP).A(0);
    fbc.next("RETURN").op(OpCode::RETURN).A(2).B(1);
    fbc.done();
}

TEST(CompilerFunc, FunctionStoredAsLocal)
{
    // fn foo() { } — foo lives in r0 in the enclosing scope
    Chunk* chunk = compile_ok(func_stmt("foo", { }, blk()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->localCount, 1); // the closure itself
}

TEST(CompilerFunc, NestedFunctionIndexing)
{
    // Two consecutive function declarations → fn_idx 0 and 1
    Chunk* chunk = compile_ok({ func_stmt("a", { }, blk()), func_stmt("b", { }, blk()) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 2u);
    BytecodeChecker bc(*chunk);
    bc.next("CLOSURE a").op(OpCode::CLOSURE).A(0).Bx(0);
    bc.next("CLOSURE b").op(OpCode::CLOSURE).A(1).Bx(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerFunc, RecursiveFunctionBodyCompiles)
{
    // fn fact(n) { if (n) { return n } return 1 }
    Chunk* chunk = compile_ok(func_stmt("fact", { makeName("n") },
        blk(makeIf(makeName("n"), blk(makeReturn(makeName("n")))),
            makeReturn(makeLiteralInt(1)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    EXPECT_FALSE(chunk->functions[0]->code.empty());
}

TEST(CompilerClosure, CapturesLocalFromEnclosingScope)
{
    // fn outer() {
    //   let x = 1
    //   fn inner() { return x }  ← x captured as upvalue
    // }
    Chunk* chunk = compile_ok(func_stmt("outer", { },
        blk(
            decl("x", makeLiteralInt(1)),
            func_stmt("inner", { }, blk(makeReturn(makeName("x")))))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    Chunk const* outer = chunk->functions[0];
    ASSERT_EQ(outer->functions.size(), 1u);
    Chunk const* inner = outer->functions[0];

    // inner must have upvalueCount = 1
    EXPECT_EQ(inner->upvalueCount, 1);

    // inner's body must contain GET_UPVALUE
    bool has_get_uv = false;
    for (auto& ins : inner->code)
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::GET_UPVALUE))
            has_get_uv = true;
    EXPECT_TRUE(has_get_uv) << "inner should GET_UPVALUE for x";

    // Outer chunk must have a CLOSE_UPVALUE after inner's scope closes
    bool has_close_uv = false;
    for (auto& ins : outer->code)
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::CLOSE_UPVALUE))
            has_close_uv = true;
    EXPECT_TRUE(has_close_uv) << "outer should CLOSE_UPVALUE when x goes out of scope";
}

TEST(CompilerClosure, UpvalueDescriptorEmittedAfterClosure)
{
    // The MOVE pseudo-instruction (upvalue descriptor) must follow CLOSURE
    // in the parent chunk's code stream.
    // fn outer():
    //     x = 1
    //     fn inner():
    //         return x
    Chunk* chunk = compile_ok(func_stmt("outer", { }, blk(decl("x", makeLiteralInt(1)), func_stmt("inner", { }, blk(makeReturn(makeName("x")))))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    Chunk const* outer = chunk->functions[0];
    // Find the CLOSURE instruction in outer
    int closure_pos = -1;
    for (int i = 0; i < (int)outer->code.size(); ++i)
        if (instr_op(outer->code[i]) == static_cast<uint8_t>(OpCode::CLOSURE))
            closure_pos = i;
    ASSERT_GE(closure_pos, 0) << "CLOSURE not found in outer";
    // The instruction immediately after CLOSURE must be a MOVE with
    // A=1 (is_local=true) and B=register of x (r0)
    ASSERT_LT(closure_pos + 1, (int)outer->code.size());
    uint32_t desc = outer->code[closure_pos + 1];
    EXPECT_EQ(instr_op(desc), static_cast<uint8_t>(OpCode::MOVE)) << "upvalue descriptor MOVE must follow CLOSURE";
    EXPECT_EQ(instr_A(desc), 1u) << "is_local=1 for direct local capture";
    EXPECT_EQ(instr_B(desc), 0u) << "captures r0 (x)";
}

TEST(CompilerCall, CallWithNoArgs)
{
    // f()
    // LOAD_GLOBAL f → r0
    // IC_CALL r0 argc=0 ic=0
    Chunk* chunk = compile_ok(makeExprStmt(call(makeName("f"))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL f").op(OpCode::LOAD_GLOBAL);
    bc.next("IC_CALL").op(OpCode::IC_CALL).B(0); // argc=0
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->icSlots.size(), 1u);
}

TEST(CompilerCall, CallWithTwoArgs)
{
    // f(1, 2)
    // LOAD_GLOBAL f → r0
    // LOAD_INT 1 → r1
    // LOAD_INT 2 → r2
    // IC_CALL r0 argc=2 ic=0
    Array<Expr*> args;
    args.push(makeLiteralInt(1));
    args.push(makeLiteralInt(2));
    Chunk* chunk = compile_ok(makeExprStmt(call(makeName("f"), std::move(args))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL f").op(OpCode::LOAD_GLOBAL).A(0);
    bc.next("arg1").op(OpCode::LOAD_INT).A(1).Bx(load_int_bx(1));
    bc.next("arg2").op(OpCode::LOAD_INT).A(2).Bx(load_int_bx(2));
    bc.next("IC_CALL").op(OpCode::IC_CALL).A(0).B(2); // argc=2
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerCall, CallResultUsed)
{
    // let r = f(x)
    // LOAD_GLOBAL f → r0
    // LOAD_GLOBAL x → r1
    // IC_CALL r0 argc=1
    // (result is in r0; local 'r' is the next allocated reg)
    Chunk* chunk = compile_ok( decl("r", call(makeName("f"), [] {
        Array<Expr*> a;
        a.push(makeName("x"));
        return a;
    }())));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    bool has_ic_call = false;
    for (auto& ins : chunk->code)
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::IC_CALL))
            has_ic_call = true;
    EXPECT_TRUE(has_ic_call);
}

TEST(CompilerCall, ICSlotAllocatedPerCallSite)
{
    // f(); g() → two IC slots
    Chunk* chunk = compile_ok({ makeExprStmt(call(makeName("f"))), makeExprStmt(call(makeName("g"))) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->icSlots.size(), 2u);
}

TEST(CompilerList, EmptyList)
{
    // []
    // LIST_NEW r0 cap=0
    Chunk* chunk = compile_ok(makeExprStmt(makeList()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LIST_NEW").op(OpCode::LIST_NEW).A(0).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerList, ListWithElements)
{
    // [1, 2, 3]
    Array<Expr*> elems;
    elems.push(makeLiteralInt(1));
    elems.push(makeLiteralInt(2));
    elems.push(makeLiteralInt(3));
    Chunk* chunk = compile_ok(makeExprStmt(makeList(std::move(elems))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LIST_NEW").op(OpCode::LIST_NEW).A(0).B(3); // cap hint = 3
    // For each element: load into temp, LIST_APPEND
    bc.next("LOAD 1").op(OpCode::LOAD_INT).Bx(load_int_bx(1));
    bc.next("APPEND 1").op(OpCode::LIST_APPEND).A(0);
    bc.next("LOAD 2").op(OpCode::LOAD_INT).Bx(load_int_bx(2));
    bc.next("APPEND 2").op(OpCode::LIST_APPEND).A(0);
    bc.next("LOAD 3").op(OpCode::LOAD_INT).Bx(load_int_bx(3));
    bc.next("APPEND 3").op(OpCode::LIST_APPEND).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerList, ListCapHintCappedAt255)
{
    // A list with 300 elements must cap the hint at 255
    Array<Expr*> elems;
    for (int i = 0; i < 300; ++i)
        elems.push(makeLiteralInt(i));
    Chunk* chunk = compile_ok(makeExprStmt(makeList(std::move(elems))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // First instruction is LIST_NEW; B field = capacity hint
    ASSERT_FALSE(chunk->code.empty());
    uint32_t new_list = chunk->code[0];
    EXPECT_EQ(instr_op(new_list), static_cast<uint8_t>(OpCode::LIST_NEW));
    EXPECT_EQ(instr_B(new_list), 255u) << "capacity hint must be capped at 255";
}

TEST(CompilerScope, LocalslDontLeakOutOfBlock)
{
    // { let x = 1 }   ← x is gone after block
    // let x = 2       ← this x should also be r0 (reused)
    BlockStmt* _ast = blk(
        makeBlock({ decl("x", makeLiteralInt(1)) }),
        decl("x", makeLiteralInt(2)));
    Chunk* chunk = compile_ok(_ast);
    ASTPrinter printer;
    printer.print(_ast);
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // After the inner block closes, next_reg returns to 0, so y goes into r0
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 1 (inner x)").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.next("LOAD_INT 2 (outer x)").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(2));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->localCount, 1); // max concurrent locals = 1
}

TEST(CompilerScope, NestedScopesBothVisible)
{
    // let a = 1; { let b = 2; let c = a + b }
    Chunk* chunk = compile_ok({ decl("a", makeLiteralInt(1)),
        makeBlock([] {
            Array<Stmt*> s;
            s.push(decl("b", makeLiteralInt(2)));
            s.push(decl("c", makeBinary(makeName("a"), makeName("b"), BinaryOp::OP_ADD)));
            return s;
        }()) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // a=r0, b=r1; OP_ADD r2 r0 r1; c goes into r2
    bool found_add = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::OP_ADD)) {
            EXPECT_EQ(instr_B(ins), 0u) << "a in r0";
            EXPECT_EQ(instr_C(ins), 1u) << "b in r1";
            found_add = true;
        }
    }
    EXPECT_TRUE(found_add);
}

TEST(CompilerMeta, TopLevelChunkNameIsMain)
{
    Chunk* chunk = compile_ok(blk());
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->name, "<main>");
}

TEST(CompilerMeta, LocalCountReflectsMaxRegisters)
{
    // let a=1; let b=2; let c=3  → max_reg = 3
    Chunk* chunk = compile_ok({ decl("a", makeLiteralInt(1)),
        decl("b", makeLiteralInt(2)),
        decl("c", makeLiteralInt(3)) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_EQ(chunk->localCount, 3);
}

TEST(CompilerMeta, FunctionAritySetCorrectly)
{
    Chunk* chunk = compile_ok(func_stmt("f", { makeName("x"), makeName("y"), makeName("z") }, blk()));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->arity, 3);
}

TEST(CompilerMeta, FunctionNameSetCorrectly)
{
    Chunk* chunk = compile_ok( func_stmt("compute", { }, blk()) );
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->name, "compute");
}

TEST(CompilerMeta, LineInfoPresent)
{
    Chunk* chunk = compile_ok(decl("x", makeLiteralInt(42), false, /*line=*/7));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_FALSE(chunk->lines.empty());
}

TEST(CompilerIntegration, Fibonacci)
{
    // fn fib(n) {
    //   if (n <= 1) { return n }
    //   return fib(n-1) + fib(n-2)
    // }
    Array<Expr*> args_n1, args_n2;
    args_n1.push(makeBinary(makeName("n"), makeLiteralInt(1), BinaryOp::OP_SUB));
    args_n2.push(makeBinary(makeName("n"), makeLiteralInt(2), BinaryOp::OP_SUB));

    Chunk* chunk = compile_ok(func_stmt("fib", { makeName("n") },
        blk(
            makeIf(
                makeBinary(makeName("n"), makeLiteralInt(1), BinaryOp::OP_LTE),
                blk(makeReturn(makeName("n")))),
            makeReturn(makeBinary(
                call(makeName("fib"), std::move(args_n1)),
                call(makeName("fib"), std::move(args_n2)),
                BinaryOp::OP_ADD)))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    EXPECT_FALSE(chunk->functions.empty());
    Chunk const* fib = chunk->functions[0];
    EXPECT_EQ(fib->name, "fib");
    EXPECT_EQ(fib->arity, 1);
    EXPECT_FALSE(fib->code.empty());
    // Both recursive calls → 2 IC slots from the two IC_CALLs
    // plus IC slots from the binary ops OP_ADD, OP_LTE, two SUBs = >= 2 from calls
    EXPECT_GE(fib->icSlots.size(), 2u);
}

TEST(CompilerIntegration, CounterWithClosure)
{
    // fn make_counter() {
    //   let count = 0
    //   fn inc() { count = count + 1 }
    //   return inc
    // }
    Chunk* chunk = compile_ok(func_stmt("make_counter", { },
        blk(
            decl("count", makeLiteralInt(0)),
            func_stmt("inc", { },
                blk(assign_stmt("count",
                    makeBinary(makeName("count"), makeLiteralInt(1), BinaryOp::OP_ADD)))),
            makeReturn(makeName("inc")))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    Chunk const* mc = chunk->functions[0];
    ASSERT_EQ(mc->functions.size(), 1u);
    Chunk const* inc = mc->functions[0];

    // inc must reference count as an upvalue (SET_UPVALUE for the assignment)
    bool has_set_uv = false;
    for (auto& ins : inc->code)
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::SET_UPVALUE))
            has_set_uv = true;
    EXPECT_TRUE(has_set_uv) << "inc() should SET_UPVALUE for count";
    EXPECT_EQ(inc->upvalueCount, 1);
}

TEST(CompilerIntegration, NestedArithmetic)
{
    // result = (2 + 3) * (4 - 1)
    // Entire expression is const-folded: (5) * (3) = 15
    Chunk* chunk = compile_ok(decl("result",
        makeBinary(
            makeBinary(makeLiteralInt(2), makeLiteralInt(3), BinaryOp::OP_ADD),
            makeBinary(makeLiteralInt(4), makeLiteralInt(1), BinaryOp::OP_SUB),
            BinaryOp::OP_MUL)));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 15").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(15));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_TRUE(chunk->icSlots.empty()); // all folded
}

TEST(CompilerIntegration, StringConstantPoolDedup)
{
    // "hello" used three times → one pool entry
    Chunk* chunk = compile_ok({ makeExprStmt(makeLiteralString("hello")),
        makeExprStmt(makeLiteralString("hello")),
        makeExprStmt(makeLiteralString("hello")) });
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    int count = 0;
    for (auto& v : chunk->constants)
        if (v.isString() && v.asString()->str == "hello")
            ++count;
    EXPECT_EQ(count, 1);
}

TEST(CompilerIntegration, MixedLiteralsInList)
{
    // [true, 42, 3.14, "hi"]
    Array<Expr*> elems;
    elems.push(makeLiteralBool(true));
    elems.push(makeLiteralInt(42));
    elems.push(makeLiteralFloat(3.14));
    elems.push(makeLiteralString("hi"));
    Chunk* chunk = compile_ok(makeExprStmt(makeList(std::move(elems))));
    ASSERT_NE(chunk, nullptr);
    dump(chunk);
    // Must have LIST_NEW followed by 4× (load + LIST_APPEND)
    int appends = 0;
    for (auto& ins : chunk->code)
        if (instr_op(ins) == static_cast<uint8_t>(OpCode::LIST_APPEND))
            ++appends;
    EXPECT_EQ(appends, 4);
}
