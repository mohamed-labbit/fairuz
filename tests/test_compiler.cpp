#include "../fairuz/fAST.hpp"
#include "../fairuz/fAST_printer.hpp"
#include "../fairuz/fcompiler.hpp"
#include "../fairuz/fdiagnostic.hpp"
#include "../fairuz/fstring.hpp"
#include "fopcode.hpp"
#include "test_common.h"
#include "test_config.h"

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
        EXPECT_EQ(pos_, chunk_.code.size()) << "expected end of code at instruction " << pos_ << " but " << (chunk_.code.size() - pos_) << " more remain";
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

static Chunk* compile_ok(Array<AST::StmtPtr> stmts, Compiler& c)
{
    diagnostic::reset();
    Chunk* chunk = c.compile(stmts);
    EXPECT_FALSE(diagnostic::has_errors());
    diagnostic::reset();
    return chunk;
}

static void dump(Chunk const* c)
{
    std::cout << '\n'
              << "Disassembled bytecode :" << '\n';
    c->disassemble();
    std::cout << '\n';
}

static Chunk* compile_ok_local(Array<AST::StmtPtr> stmts)
{
    AST::StmtPtr f = func_def(ident("test_function"), { }, blk(stmts));
    Compiler c;
    Chunk* ch = compile_ok({ f }, c);
    EXPECT_NE(ch, nullptr);
    if (test_config::dump_bytecode)
        dump(ch);

    BytecodeChecker bc(*ch);
    bc.next("CLOSURE").op(OpCode::CLOSURE).A(0).Bx(0);
    bc.next("STORE_GLOBAL").op(OpCode::STORE_GLOBAL).A(0).Bx(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(ch->functions.size(), 1u);
    Chunk* def_chunk = ch->functions[0];
    EXPECT_NE(def_chunk, nullptr);
    return def_chunk;
}

static Chunk* compile_ok(Array<AST::StmtPtr> stmts)
{
    Compiler c;
    return compile_ok(stmts, c);
}

static Chunk* compile_ok(AST::StmtPtr root)
{
    Array<AST::StmtPtr> stmts;
    stmts.push(root);
    return compile_ok(stmts);
}

static Chunk* compile_fail(Array<AST::StmtPtr> stmts)
{
    diagnostic::reset();
    Chunk* chunk = Compiler().compile(stmts);
    EXPECT_TRUE(diagnostic::has_errors());
    diagnostic::reset();
    return chunk;
}

static Chunk* compile_fail(AST::StmtPtr root)
{
    Array<AST::StmtPtr> stmts;
    stmts.push(root);
    return compile_fail(stmts);
}

static u16 load_int_bx(i64 i) { return static_cast<u16>(i + 32767); }

TEST(CompilerLiteral, NilExpression)
{
    Chunk* chunk = compile_ok(expr_stmt(nil()));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_NIL").op(OpCode::LOAD_NIL).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, TrueLiteral)
{
    Chunk* chunk = compile_ok(expr_stmt(lit_bool(true)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_TRUE").op(OpCode::LOAD_TRUE).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, FalseLiteral)
{
    Chunk* chunk = compile_ok(expr_stmt(lit_bool(false)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_FALSE").op(OpCode::LOAD_FALSE).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, SmallIntegerUsesLoadInt)
{
    Chunk* chunk = compile_ok(expr_stmt(lit_int(42)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(42));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_TRUE(chunk->constants.empty());
}

#if FA_USE_NANBOX

TEST(CompilerLiteral, VeryBigPositiveIntegerUsesLoadBigInt)
{
    i64 big_int = static_cast<i64>(Value::int_max() + 2);
    Chunk* chunk = compile_ok(expr_stmt(lit_int(big_int)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_BIG_INT").op(OpCode::LOAD_BIG_INT).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_FALSE(chunk->big_ints.empty());
}

TEST(CompilerLiteral, VeryBigNegativeIntegerUsesLoadBigInt)
{
    i64 big_int = static_cast<i64>(Value::int_min() - 2);
    Chunk* chunk = compile_ok(expr_stmt(lit_int(big_int)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_BIG_INT").op(OpCode::LOAD_BIG_INT).A(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_FALSE(chunk->big_ints.empty());
}

#endif

TEST(CompilerLiteral, NegativeSmallIntegerUsesLoadInt)
{
    Chunk* chunk = compile_ok(expr_stmt(lit_int(-100)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).Bx(load_int_bx(-100));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerLiteral, ZeroUsesLoadInt)
{
    Chunk* chunk = compile_ok(expr_stmt(lit_int(0)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).Bx(load_int_bx(0));
}

TEST(CompilerLiteral, LargeIntegerUsesConstantPool)
{
    Chunk* chunk = compile_ok(expr_stmt(lit_int(100000)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].is_int());
    EXPECT_EQ(chunk->constants[0].as_int(), 100000);
}

TEST(CompilerLiteral, FloatUsesConstantPool)
{
    Chunk* chunk = compile_ok(expr_stmt(lit_flt(3.14)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].is_double());
    EXPECT_NEAR(chunk->constants[0].as_double(), 3.14, 1e-9);
}

TEST(CompilerLiteral, StringUsesConstantPool)
{
    Chunk* chunk = compile_ok(expr_stmt(lit_str("hello")));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_CONST").op(OpCode::LOAD_CONST).A(0).Bx(0);
    ASSERT_FALSE(chunk->constants.empty());
    EXPECT_TRUE(chunk->constants[0].is_string());
    EXPECT_EQ(chunk->constants[0].as_string()->str, "hello");
}

TEST(CompilerLiteral, StringsDeduplicated)
{
    Array<AST::StmtPtr> stmts;
    stmts.push(expr_stmt(lit_str("dup")));
    stmts.push(expr_stmt(lit_str("dup")));
    Chunk* chunk = compile_ok(blk(std::move(stmts)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    long string_constants = std::count_if(chunk->constants.begin(), chunk->constants.end(), [](Value const& v) { return v.is_string(); });
    EXPECT_EQ(string_constants, 1);
}

TEST(CompilerVar, LocalDeclaration)
{
    Chunk* ch = compile_ok_local({ decl_stmt("x", lit_int(5)) });
    BytecodeChecker bc(*ch);
    bc.next("LOAD_INT").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(5));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    EXPECT_EQ(ch->local_count, 1u);
}

TEST(CompilerVar, TwoLocalsUseConsecutiveRegisters)
{
    Chunk* ch = compile_ok_local({
        decl_stmt("x", lit_int(1)),
        decl_stmt("y", lit_int(2)),
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
        decl_stmt("x", lit_int(1)),
        expr_stmt(assign_expr(ident("x"), lit_int(2))),
    });

    BytecodeChecker bc(*ch);
    bc.next("decl_stmt x=1").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.next("assign x=2").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(2));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerVar, GlobalLoadAndStore)
{
    Chunk* chunk = compile_ok({ expr_stmt(assign_expr(ident("g"), lit_int(7))), expr_stmt(ident("g")) });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("RHS").op(OpCode::LOAD_INT).Bx(load_int_bx(7));
    bc.next("STORE_GLOBAL").op(OpCode::STORE_GLOBAL);
    bc.next("LOAD_GLOBAL").op(OpCode::LOAD_GLOBAL);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    bool found = false;
    for (auto& v : chunk->constants) {
        if (v.is_string() && v.as_string()->str == "g")
            found = true;
    }
    EXPECT_TRUE(found) << "global ident 'g' not interned into constant pool";
}

TEST(CompilerUnary, NegateVariable)
{
    Chunk* ch = compile_ok_local(
        {
            decl_stmt("x", lit_int(5)),
            expr_stmt(unary(ident("x"), AST::Expr::Kind::OP_NEG)),
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
        decl_stmt("b", lit_bool(true)),
        expr_stmt(unary(ident("b"), AST::Expr::Kind::OP_NOT)),
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
        decl_stmt("n", lit_int(0xFF)),
        expr_stmt(unary(ident("n"), AST::Expr::Kind::OP_BITNOT)),
    });

    BytecodeChecker bc(*ch);
    bc.next("decl_stmt").op(OpCode::LOAD_INT).A(0);
    bc.next("OP_BITNOT").op(OpCode::OP_BITNOT).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerBinary, AddTwoLocals)
{
    Chunk* ch = compile_ok_local(
        { decl_stmt("a", lit_int(1)), decl_stmt("b", lit_int(2)),
            expr_stmt(binary(ident("a"), ident("b"), AST::Expr::Kind::OP_ADD)) });
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
    Chunk* chunk = compile_ok({ decl_stmt("x", lit_int(1)), decl_stmt("y", lit_int(2)),
        expr_stmt(binary(ident("x"), ident("y"), AST::Expr::Kind::OP_ADD)),
        expr_stmt(binary(ident("x"), ident("y"), AST::Expr::Kind::OP_MUL)) });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    EXPECT_EQ(chunk->ic_slots.size(), 2u);
}

TEST(CompilerBinary, LogicalAndShortCircuit)
{
    Chunk* ch = compile_ok_local({
        decl_stmt("a", lit_bool(true)),
        decl_stmt("b", lit_bool(false)),
        expr_stmt(binary(ident("a"), ident("b"), AST::Expr::Kind::OP_AND)),
    });

    BytecodeChecker bc(*ch);
    bc.next("decl_stmt a").op(OpCode::LOAD_TRUE).A(0);
    bc.next("decl_stmt b").op(OpCode::LOAD_FALSE).A(1);
    u32 jif_idx;
    bc.next("LHS into temp").op(OpCode::MOVE).A(2).B(0);
    (void)jif_idx;
    bool found_jif = false, found_b = false;
    for (auto& instr : ch->code) {
        if (instr_op(instr) == OpCode::JUMP_IF_FALSE)
            found_jif = true;
        if (found_jif && (instr_op(instr) == OpCode::LOAD_FALSE || instr_op(instr) == OpCode::MOVE))
            found_b = true;
    }
    EXPECT_TRUE(found_jif) << "expected JUMP_IF_FALSE for && short-circuit";
    EXPECT_TRUE(found_b) << "expected RHS load after JUMP_IF_FALSE";
}

TEST(CompilerBinary, LogicalOrShortCircuit)
{
    Chunk* chunk = compile_ok_local({
        decl_stmt("a", lit_bool(false)),
        decl_stmt("b", lit_bool(true)),
        expr_stmt(binary(ident("a"), ident("b"), AST::Expr::Kind::OP_OR)),
    });

    bool found_jit = false;
    for (auto& instr : chunk->code) {
        if (instr_op(instr) == OpCode::JUMP_IF_TRUE)
            found_jit = true;
    }
    EXPECT_TRUE(found_jit) << "expected JUMP_IF_TRUE for || short-circuit";
}

TEST(CompilerIf, SimpleIfNoElse)
{
    Chunk* chunk = compile_ok(if_stmt(ident("x"), blk({ decl_stmt("y", lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);

    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP_IF_FALSE)
            jif_pos = i;
    }
    ASSERT_GE(jif_pos, 0) << "expected JUMP_IF_FALSE";
    int m_target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    EXPECT_EQ(instr_op(chunk->code[m_target]), OpCode::RETURN_NIL);
}

TEST(CompilerIf, IfElse)
{
    Chunk* chunk = compile_ok(
        if_stmt(
            ident("x"),
            blk({ decl_stmt("a", lit_int(1)) }),
            blk({ decl_stmt("b", lit_int(2)) })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);

    bool has_jif = false, has_jmp = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::JUMP_IF_FALSE)
            has_jif = true;
        if (instr_op(ins) == OpCode::JUMP)
            has_jmp = true;
    }
    EXPECT_TRUE(has_jif) << "expected JUMP_IF_FALSE";
    EXPECT_TRUE(has_jmp) << "expected JUMP over else";
    int jif_pos = -1, jmp_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP_IF_FALSE)
            jif_pos = i;
        else if (instr_op(chunk->code[i]) == OpCode::JUMP)
            jmp_pos = i;
    }
    ASSERT_GE(jif_pos, 0);
    ASSERT_GE(jmp_pos, 0);
    int jif_target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    EXPECT_EQ(jif_target, jmp_pos + 1);
}

TEST(CompilerWhile, BasicWhile)
{
    Chunk* chunk = compile_ok(while_stmt(ident("x"), blk({ decl_stmt("a", lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);

    bool has_jif = false, has_loop = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::JUMP_IF_FALSE)
            has_jif = true;
        if (instr_op(ins) == OpCode::LOOP)
            has_loop = true;
    }
    EXPECT_TRUE(has_jif) << "while needs JUMP_IF_FALSE";
    EXPECT_TRUE(has_loop) << "while needs LOOP back-edge";

    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::LOOP)
            EXPECT_LT(instr_sBx(ins), 0) << "LOOP offset must be negative";
    }
}

TEST(CompilerWhile, JumpIfFalsePointsPastLoop)
{
    Chunk* chunk = compile_ok(while_stmt(ident("cond"), blk({ decl_stmt("x", lit_int(0)) })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    int jif_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++)
        if (instr_op(chunk->code[i]) == OpCode::JUMP_IF_FALSE)
            jif_pos = i;
    ASSERT_GE(jif_pos, 0);
    int m_target = jif_pos + 1 + instr_sBx(chunk->code[jif_pos]);
    ASSERT_LT(m_target, (int)chunk->code.size());
    EXPECT_EQ(instr_op(chunk->code[m_target]), OpCode::RETURN_NIL);
}

TEST(CompilerFor, ListIterationLowersToLoopBytecode)
{
    Chunk* chunk = compile_ok({ decl_stmt("items", list_expr({ lit_int(1), lit_int(2), lit_int(3) })),
        decl_stmt("sum", lit_int(0)),
        for_stmt(
            ident("item"),
            ident("items"),
            blk({ expr_stmt(assign_expr(ident("sum"), binary(ident("sum"), ident("item"), AST::Expr::Kind::OP_ADD))) })) });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);

    bool has_list_len = false;
    bool has_list_get = false;
    bool has_jif = false;
    bool has_loop = false;
    bool has_add = false;

    for (auto ins : chunk->code) {
        switch (instr_op(ins)) {
        case OpCode::LIST_LEN:
            has_list_len = true;
            break;
        case OpCode::LIST_GET:
            has_list_get = true;
            break;
        case OpCode::JUMP_IF_FALSE:
            has_jif = true;
            break;
        case OpCode::LOOP:
            has_loop = true;
            EXPECT_LT(instr_sBx(ins), 0) << "for loop back-edge must be negative";
            break;
        case OpCode::OP_ADD:
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
    Chunk* chunk = compile_ok(
        expr_stmt(
            dict_expr({
                { lit_str("a"), lit_int(1) },
                { lit_str("b"), lit_int(2) },
            })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);

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
    Chunk* chunk = compile_ok(
        expr_stmt(
            dict_expr({
                { lit_str("a"), list_expr({ lit_int(1) }) },
                { lit_str("b"), lit_int(2) },
            })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);

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
    Chunk* chunk = compile_ok(return_stmt(nil()));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerReturn, ReturnValueEmitsReturn)
{
    Chunk* chunk = compile_ok(return_stmt(lit_int(42)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 42").op(OpCode::LOAD_INT).Bx(load_int_bx(42));
    bc.next("RETURN").op(OpCode::RETURN).B(1);
    bc.done();
}

TEST(CompilerReturn, ReturnIsDeadCodeBarrier)
{
    Chunk* chunk = compile_ok({ return_stmt(lit_int(1)), decl_stmt("x", lit_int(99)) });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    for (auto& v : chunk->constants)
        EXPECT_NE(v.as_int(), 99) << "dead code leaked into constant pool";
    bool found_99 = false;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::LOAD_INT && instr_Bx(ins) == load_int_bx(99))
            found_99 = true;
    }
    EXPECT_FALSE(found_99) << "dead code (LOAD_INT 99) was emitted after return";
}

TEST(CompilerReturn, TailCallEmitsCallTail)
{
    Chunk* chunk = compile_ok(func_def(ident("wrapper"), { }, blk({ return_stmt(call_expr(ident("f"))) })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    Chunk const* fn = chunk->functions[0];
    bool has_tail = false;
    for (auto& ins : fn->code) {
        if (instr_op(ins) == OpCode::CALL_TAIL)
            has_tail = true;
    }
    EXPECT_TRUE(has_tail) << "return f() in function should emit CALL_TAIL";
}

TEST(CompilerFunc, EmptyFunction)
{
    Chunk* chunk = compile_ok(func_def(ident("foo"), { }, blk({ })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("CLOSURE").op(OpCode::CLOSURE).A(0).Bx(0);
    bc.next("STORE_GLOBAL").op(OpCode::STORE_GLOBAL).A(0).Bx(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();

    ASSERT_EQ(chunk->functions.size(), 1u);
    Chunk const* fn = chunk->functions[0];
    EXPECT_EQ(fn->name, "foo");
    EXPECT_EQ(fn->arity, 0);
    BytecodeChecker fbc(*fn);
    fbc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    fbc.done();
}

TEST(CompilerFunc, FunctionWithParams)
{
    Chunk* chunk = compile_ok(func_def(ident("add"), { ident("a"), ident("b") },
        blk({ return_stmt(binary(ident("a"), ident("b"), AST::Expr::Kind::OP_ADD)) })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
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
    Chunk* chunk = compile_ok(func_def(ident("foo"), { }, blk({ })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    EXPECT_EQ(chunk->local_count, 1);
}

TEST(CompilerFunc, NestedFunctionIndexing)
{
    Chunk* chunk = compile_ok({ func_def(ident("a"), { }, blk({ })), func_def(ident("b"), { }, blk({ })) });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
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
    Chunk* chunk = compile_ok(
        func_def(
            ident("fact"),
            { ident("n") },
            blk({ if_stmt(
                      ident("n"),
                      blk(
                          { return_stmt(ident("n")) })),
                return_stmt(lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    ASSERT_FALSE(chunk->functions.empty());
    EXPECT_FALSE(chunk->functions[0]->code.empty());
}

TEST(CompilerFunc, FunctionInsideTopLevelBlockRejected)
{
    Chunk* chunk = compile_fail(blk({ func_def(ident("inner"), { }, blk({ })) }));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    EXPECT_TRUE(chunk->functions.empty());
}

TEST(CompilerCall, CallWithNoArgs)
{
    Chunk* chunk = compile_ok(expr_stmt(call_expr(ident("f"))));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_GLOBAL f").op(OpCode::LOAD_GLOBAL);
    bc.next("IC_CALL").op(OpCode::IC_CALL).B(0);
    bc.next("RETURN").op(OpCode::RETURN).A(0).B(1);
    bc.done();
    EXPECT_EQ(chunk->ic_slots.size(), 1u);
}

TEST(CompilerCall, CallWithTwoArgs)
{
    Array<AST::ExprPtr> args { lit_int(1), lit_int(2) };
    Chunk* chunk = compile_ok(expr_stmt(call_expr(ident("f"), args)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
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
    Chunk* chunk = compile_ok(decl_stmt("r", call_expr(ident("f"), { ident("x") })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    bool has_ic_call = false;
    for (auto& ins : chunk->code)
        if (instr_op(ins) == OpCode::IC_CALL)
            has_ic_call = true;
    EXPECT_TRUE(has_ic_call);
}

TEST(CompilerCall, ICSlotAllocatedPerCallSite)
{
    Chunk* chunk = compile_ok({ expr_stmt(call_expr(ident("f"))), expr_stmt(call_expr(ident("g"))) });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    EXPECT_EQ(chunk->ic_slots.size(), 2u);
}

TEST(CompilerList, EmptyList)
{
    Chunk* chunk = compile_ok(expr_stmt(list_expr()));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LIST_NEW").op(OpCode::LIST_NEW).A(1).B(0);
    bc.next("MOVE").op(OpCode::MOVE).A(0).B(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerList, ListWithElements)
{
    Array<AST::ExprPtr> elems = { lit_int(1), lit_int(2), lit_int(3) };
    Chunk* chunk = compile_ok(expr_stmt(list_expr(std::move(elems))));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
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

TEST(CompilerScope, LocalslDontLeakOutOfBlock)
{
    AST::BlockStmt* _ast = blk({ blk({ decl_stmt("x", lit_int(1)) }), decl_stmt("x", lit_int(2)) });
    Chunk* chunk = compile_ok(_ast);
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LOAD_INT 1 (inner x)").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(1));
    bc.next("LOAD_INT 2 (outer x)").op(OpCode::LOAD_INT).A(0).Bx(load_int_bx(2));
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
    EXPECT_EQ(chunk->local_count, 1);
}

TEST(CompilerScope, NestedScopesBothVisible)
{
    Chunk* ch = compile_ok_local({
        decl_stmt("a", lit_int(1)),
        blk({
            decl_stmt("b", lit_int(2)),
            decl_stmt("c", binary(ident("a"), ident("b"), AST::Expr::Kind::OP_ADD)),
        }),
    });

    bool found_add = false;
    for (auto& ins : ch->code) {
        if (instr_op(ins) == OpCode::OP_ADD) {
            EXPECT_EQ(instr_B(ins), 0u) << "a in r0";
            EXPECT_EQ(instr_C(ins), 1u) << "b in r1";
            found_add = true;
        }
    }
    EXPECT_TRUE(found_add);
}

TEST(CompilerMeta, TopLevelChunkNameIsMain)
{
    Chunk* chunk = compile_ok(blk({ }));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    EXPECT_EQ(chunk->name, "<main>");
}

TEST(CompilerMeta, FunctionAritySetCorrectly)
{
    Chunk* chunk = compile_ok(func_def(ident("f"), { ident("x"), ident("y"), ident("z") }, blk({ })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->arity, 3);
}

TEST(CompilerMeta, FunctionNameSetCorrectly)
{
    Chunk* chunk = compile_ok(func_def(ident("compute"), { }, blk({ })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    ASSERT_EQ(chunk->functions.size(), 1u);
    EXPECT_EQ(chunk->functions[0]->name, "compute");
}

TEST(CompilerMeta, LineInfoPresent)
{
    Chunk* chunk = compile_ok(decl_stmt("x", lit_int(42)));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    EXPECT_FALSE(chunk->lines.empty());
}

TEST(CompilerIntegration, Fibonacci)
{
    Array<AST::ExprPtr> args_n1, args_n2;
    args_n1.push(binary(ident("n"), lit_int(1), AST::Expr::Kind::OP_SUB));
    args_n2.push(binary(ident("n"), lit_int(2), AST::Expr::Kind::OP_SUB));

    Chunk* chunk = compile_ok(
        func_def(ident("fib"),
            { ident("n") },
            blk(
                { if_stmt(
                      binary(ident("n"), lit_int(1), AST::Expr::Kind::OP_LTE),
                      blk({ return_stmt(ident("n")) })),
                    return_stmt(
                        binary(
                            call_expr(ident("fib"), args_n1),
                            call_expr(ident("fib"), args_n2),
                            AST::Expr::Kind::OP_ADD)) })));

    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    EXPECT_FALSE(chunk->functions.empty());
    Chunk const* fib = chunk->functions[0];
    EXPECT_EQ(fib->name, "fib");
    EXPECT_EQ(fib->arity, 1);
    EXPECT_FALSE(fib->code.empty());
    EXPECT_GE(fib->ic_slots.size(), 2u);
}

TEST(CompilerLoop, BreakPatchesToLoopExit)
{
    Chunk* chunk = compile_ok(
        while_stmt(
            ident("cond"),
            blk({ break_stmt(),
                decl_stmt("x", lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);

    int jump_pos = -1;
    int loop_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP && jump_pos < 0)
            jump_pos = i;
        if (instr_op(chunk->code[i]) == OpCode::LOOP)
            loop_pos = i;
    }

    ASSERT_GE(jump_pos, 0);
    ASSERT_GE(loop_pos, 0);
    EXPECT_GT(jump_pos + 1 + instr_sBx(chunk->code[jump_pos]), loop_pos);
}

TEST(CompilerLoop, ContinuePatchesToLoopLatch)
{
    Chunk* chunk = compile_ok(
        while_stmt(
            ident("cond"),
            blk({ continue_stmt(),
                decl_stmt("x", lit_int(1)) })));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);

    int jump_pos = -1;
    int loop_pos = -1;
    for (int i = 0; i < (int)chunk->code.size(); i++) {
        if (instr_op(chunk->code[i]) == OpCode::JUMP && jump_pos < 0)
            jump_pos = i;
        if (instr_op(chunk->code[i]) == OpCode::LOOP)
            loop_pos = i;
    }

    ASSERT_GE(jump_pos, 0);
    ASSERT_GE(loop_pos, 0);
    EXPECT_EQ(jump_pos + 1 + instr_sBx(chunk->code[jump_pos]), loop_pos);
}

TEST(CompilerLoop, BreakOutsideLoopIsRejected)
{
    Chunk* chunk = compile_fail(break_stmt());
    ASSERT_NE(chunk, nullptr);
}

TEST(CompilerLoop, ContinueOutsideLoopIsRejected)
{
    Chunk* chunk = compile_fail(continue_stmt());
    ASSERT_NE(chunk, nullptr);
}

TEST(CompilerIntegration, StringConstantPoolDedup)
{
    Chunk* chunk = compile_ok(
        { expr_stmt(
              lit_str("hello")),
            expr_stmt(
                lit_str("hello")),
            expr_stmt(
                lit_str("hello")) });
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    int count = 0;
    for (auto& v : chunk->constants) {
        if (v.is_string() && v.as_string()->str == "hello")
            count++;
    }
    EXPECT_EQ(count, 1);
}

TEST(CompilerIntegration, MixedLiteralsInList)
{
    Array<AST::ExprPtr> elems = { lit_bool(true), lit_int(42), lit_flt(3.14), lit_str("mohamed") };
    Chunk* chunk = compile_ok(expr_stmt(list_expr(std::move(elems))));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode)
        dump(chunk);
    int appends = 0;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::LIST_APPEND)
            appends++;
    }
    EXPECT_EQ(appends, 4);
}
