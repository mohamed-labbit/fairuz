// =============================================================================
// test_vm.cpp  —  GTest suite for the VM and its integrated GC
//
// Build (with real GTest):
//   g++ -std=c++17 -Wall -g -fsanitize=address,undefined \
//       tests/test_vm.cpp src/runtime/vm.cpp \
//       -Iinclude -lgtest -lgtest_main -lpthread -o test_vm
//
// The file also compiles under the lightweight shim in run_tests.cpp.
//
// Design rules
// ------------
// • Every test creates its own VMRunner, which owns exactly ONE Chunk* for
//   the lifetime of the test.  We never call vm.run() twice on the same
//   Chunk; doing so re-executes with stale IP / frame state.
// • Stack-allocated ObjString objects are safe as constant-pool entries
//   because LOAD_CONST re-interns them via VM::intern() on first use.
// • LOAD_INT encodes values with bias 32767: Bx = value + 32767.
//   Range: Bx ∈ [0, 65535]  →  value ∈ [-32767, +32768].
//   For larger integers, put a Value::integer() in the constant pool.
// • CALL A B: function at reg(A), args at reg(A+1..A+B), result → reg(A).
// • CLOSURE upvalue descriptors: one MOVE pseudo per upvalue immediately
//   after the CLOSURE instruction.  MOVE A=1,B=reg → capture local reg.
//   MOVE A=0,B=idx → re-capture parent upvalue idx.
// • FOR_PREP A sBx: A+0=counter, A+1=limit, A+2=step, A+3=user idx.
//   sBx jumps past the loop body if the loop is empty.
// • FOR_STEP A sBx: increments counter; sBx jumps back to body if still in
//   range.
// • IC NOP A: profiling hint; A = icSlots index.  Only read when the
//   preceding binary-op handler peeks the next instruction.
// =============================================================================

#ifndef GTEST_SHIM_ACTIVE
#    include <gtest/gtest.h>
#endif

#include "../../include/runtime/opcode.hpp"
#include "../../include/runtime/vm.hpp"

#include <memory>
#include <string>

namespace mylang::runtime {

// =============================================================================
// Helpers
// =============================================================================

// LOAD_INT bias
static constexpr uint16_t BX(int v)
{
    return static_cast<uint16_t>(v + 32767);
}

// Short aliases for opcode casts
static constexpr uint8_t OP(OpCode o)
{
    return static_cast<uint8_t>(o);
}

// ---------------------------------------------------------------------------
// ChunkBuilder — fluent bytecode assembler
// ---------------------------------------------------------------------------
struct CB {
    std::unique_ptr<Chunk> ch;

    CB()
        : ch(std::make_unique<Chunk>())
    {
        ch->name = "<test>";
    }

    // Emit helpers
    CB& ABC(OpCode op, uint8_t A, uint8_t B, uint8_t C, uint32_t ln = 1)
    {
        ch->emit(make_ABC(OP(op), A, B, C), ln);
        return *this;
    }
    CB& ABx(OpCode op, uint8_t A, uint16_t Bx, uint32_t ln = 1)
    {
        ch->emit(make_ABx(OP(op), A, Bx), ln);
        return *this;
    }
    CB& AsBx(OpCode op, uint8_t A, int sBx, uint32_t ln = 1)
    {
        ch->emit(make_AsBx(OP(op), A, sBx), ln);
        return *this;
    }

    // Common shorthands
    CB& load_int(uint8_t r, int v) { return ABx(OpCode::LOAD_INT, r, BX(v)); } // LOAD INT
    CB& ret(uint8_t r, uint8_t n = 1) { return ABC(OpCode::RETURN, r, n, 0); } // RETURN
    CB& ret_nil() { return ABC(OpCode::RETURN_NIL, 0, 0, 0); }                 // RETURN_NIL
    CB& nop(uint8_t slot = 0) { return ABC(OpCode::NOP, slot, 0, 0); }         // IC-profile NOP
    CB& mov(uint8_t d, uint8_t s) { return ABC(OpCode::MOVE, d, s, 0); }       // MOVE

    // Add a stack-allocated ObjString to the constant pool.
    // Safe because LOAD_CONST re-interns through VM::intern on use.
    uint16_t str(char const* s)
    {
        strs_.emplace_back(std::make_unique<ObjString>(StringRef(s)));
        return ch->addConstant(Value::object(strs_.back().get()));
    }

    // Shorthand: load a named global
    CB& ldg(uint8_t r, char const* name) { return ABx(OpCode::LOAD_GLOBAL, r, str(name)); }
    CB& stg(uint8_t r, char const* name) { return ABx(OpCode::STORE_GLOBAL, r, str(name)); }

    // Frame size / IC slots
    CB& regs(int n)
    {
        ch->localCount = n;
        return *this;
    }
    CB& slot()
    {
        ch->allocIcSlot();
        return *this;
    }

    Chunk* release() { return ch.release(); }

    void dump() const
    {
        std::cout << "Disassemebeled chunk:" << '\n';
        if (ch.get())
            ch->disassemble();
        std::cout << '\n';
    }

private:
    // Keep ObjStrings alive for the lifetime of this builder.
    std::vector<std::unique_ptr<ObjString>> strs_;
};

// ---------------------------------------------------------------------------
// VMRunner — owns a VM and one Chunk per test
// ---------------------------------------------------------------------------
struct VMRunner {
    VM vm;
    std::unique_ptr<Chunk> chunk_;

    Value run(CB& b)
    {
        chunk_.reset(b.release());
        return vm.run(chunk_.get());
    }
    Value run(std::unique_ptr<Chunk> c)
    {
        chunk_ = std::move(c);
        return vm.run(chunk_.get());
    }

    // Run and expect a VMError; return the error message.
    std::string throws(CB& b)
    {
        try {
            run(b);
            return "";
        } catch (std::runtime_error const& e) {
            return e.what();
        }
    }
    std::string throws(std::unique_ptr<Chunk> c)
    {
        try {
            run(std::move(c));
            return "";
        } catch (std::runtime_error const& e) {
            return e.what();
        }
    }
};

// =============================================================================
// VMLoads — LOAD_NIL, LOAD_TRUE, LOAD_FALSE, LOAD_INT, LOAD_CONST, RETURN_NIL
// =============================================================================

TEST(VMLoads, Nil)
{
    VMRunner r;
    CB b;
    // LOAD_NIL A=0 B=0 C=1 — fill reg[0] with nil (count=1)
    b.regs(1).ABC(OpCode::LOAD_NIL, 0, 0, 1).ret(0);
    b.dump();
    EXPECT_TRUE(r.run(b).isNil());
}

TEST(VMLoads, NilFillsMultiple)
{
    VMRunner r;
    CB b;
    // LOAD_NIL B=0 C=3 then return reg[2]
    b.regs(3).ABC(OpCode::LOAD_NIL, 0, 0, 3).ret(2);
    b.dump();
    EXPECT_TRUE(r.run(b).isNil());
}

TEST(VMLoads, True)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && v.asBoolean());
}

TEST(VMLoads, False)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && !v.asBoolean());
}

TEST(VMLoads, IntPositive)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 42).ret(0);
    Value v = r.run(b);
    EXPECT_TRUE(v.isInteger());
    EXPECT_EQ(v.asInteger(), 42);
}

TEST(VMLoads, IntZero)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 0).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isInteger());
    EXPECT_EQ(v.asInteger(), 0);
}

TEST(VMLoads, IntNegative)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, -100).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isInteger());
    EXPECT_EQ(v.asInteger(), -100);
}

TEST(VMLoads, IntMaxEncodable)
{
    // BX = 65535 → value = 65535-32767 = 32768
    VMRunner r;
    CB b;
    b.regs(1).ABx(OpCode::LOAD_INT, 0, 65535).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isInteger());
    EXPECT_EQ(v.asInteger(), 32768);
}

TEST(VMLoads, IntMinEncodable)
{
    // BX = 0 → value = 0-32767 = -32767
    VMRunner r;
    CB b;
    b.regs(1).ABx(OpCode::LOAD_INT, 0, 0).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isInteger());
    EXPECT_EQ(v.asInteger(), -32767);
}

TEST(VMLoads, ConstDouble)
{
    VMRunner r;
    CB b;
    b.regs(1);
    uint16_t k = b.ch->addConstant(Value::real(3.14));
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isDouble());
    EXPECT_DOUBLE_EQ(v.asDouble(), 3.14);
}

TEST(VMLoads, ConstString)
{
    VMRunner r;
    CB b;
    b.regs(1);
    uint16_t k = b.str("hello");
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString()->str, "hello");
}

TEST(VMLoads, ConstLargeInt)
{
    // Value too large for LOAD_INT bias — store in constant pool
    VMRunner r;
    CB b;
    b.regs(1);
    uint16_t k = b.ch->addConstant(Value::integer(1000000LL));
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isInteger());
    EXPECT_EQ(v.asInteger(), 1000000LL);
}

TEST(VMLoads, ReturnNil)
{
    VMRunner r;
    CB b;
    b.regs(1).ret_nil();
    EXPECT_TRUE(r.run(b).isNil());
}

// =============================================================================
// VMMove
// =============================================================================

TEST(VMMove, Copies)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 77).mov(1, 0).ret(1);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isInteger());
    EXPECT_EQ(v.asInteger(), 77);
}

TEST(VMMove, SourceUnchanged)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 55).mov(1, 0).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_EQ(v.asInteger(), 55);
}

// =============================================================================
// VMArith — OP_ADD OP_SUB OP_MUL OP_DIV IDIV OP_MOD OP_POW OP_NEG
// =============================================================================

TEST(VMArith, AddIntFastPath)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 32).ABC(OpCode::OP_ADD, 2, 0, 1).nop().ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isInteger());
    EXPECT_EQ(v.asInteger(), 42);
}

TEST(VMArith, AddDoubles)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.ch->addConstant(Value::real(1.5));
    uint16_t k1 = b.ch->addConstant(Value::real(2.5));
    b.ABx(OpCode::LOAD_CONST, 0, k0)
        .ABx(OpCode::LOAD_CONST, 1, k1)
        .ABC(OpCode::OP_ADD, 2, 0, 1)
        .ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isDouble());
    EXPECT_DOUBLE_EQ(v.asDouble(), 4.0);
}

TEST(VMArith, AddStringsConcat)
{
    GTEST_SKIP() << "Not supported yet.";
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.str("foo");
    uint16_t k1 = b.str("bar");
    b.ABx(OpCode::LOAD_CONST, 0, k0)
        .ABx(OpCode::LOAD_CONST, 1, k1)
        .ABC(OpCode::OP_ADD, 2, 0, 1)
        .ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString()->str, "foobar");
}

TEST(VMArith, SubInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 100).load_int(1, 58).ABC(OpCode::OP_SUB, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 42);
}

TEST(VMArith, MulInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 7).ABC(OpCode::OP_MUL, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 42);
}

TEST(VMArith, DivDouble)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.ch->addConstant(Value::real(84.0));
    uint16_t k1 = b.ch->addConstant(Value::real(2.0));
    b.ABx(OpCode::LOAD_CONST, 0, k0)
        .ABx(OpCode::LOAD_CONST, 1, k1)
        .ABC(OpCode::OP_DIV, 2, 0, 1)
        .ret(2);
    b.dump();
    EXPECT_DOUBLE_EQ(r.run(b).asDouble(), 42.0);
}

TEST(VMArith, ModPositive)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 17).load_int(1, 5).ABC(OpCode::OP_MOD, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 2);
}

TEST(VMArith, ModPythonStyle)
{
    // -1 % 5 = 4  (result has same sign as divisor)
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, -1).load_int(1, 5).ABC(OpCode::OP_MOD, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 4);
}

TEST(VMArith, Pow)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 2).load_int(1, 10).ABC(OpCode::OP_POW, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_DOUBLE_EQ(r.run(b).asDoubleAny(), 1024.0);
}

TEST(VMArith, NegInt)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 7).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), -7);
}

TEST(VMArith, NegDouble)
{
    VMRunner r;
    CB b;
    b.regs(2);
    uint16_t k = b.ch->addConstant(Value::real(3.5));
    b.ABx(OpCode::LOAD_CONST, 0, k).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_DOUBLE_EQ(r.run(b).asDouble(), -3.5);
}

TEST(VMArith, DivByZeroThrows)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.ch->addConstant(Value::real(1.0));
    uint16_t k1 = b.ch->addConstant(Value::real(0.0));
    b.ABx(OpCode::LOAD_CONST, 0, k0)
        .ABx(OpCode::LOAD_CONST, 1, k1)
        .ABC(OpCode::OP_DIV, 2, 0, 1)
        .ret(2);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMArith, NegOnStringThrows)
{
    VMRunner r;
    CB b;
    b.regs(2).ABx(OpCode::LOAD_CONST, 0, b.str("x")).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

// =============================================================================
// VMBitwise — OP_BITAND OP_BITOR OP_BITXOR OP_BITNOT OP_LSHIFT OP_RSHIFT
// =============================================================================

TEST(VMBitwise, And)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b1010).ABC(OpCode::OP_BITAND, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 0b1010);
}

TEST(VMBitwise, Or)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1100).load_int(1, 0b0011).ABC(OpCode::OP_BITOR, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 0b1111);
}

TEST(VMBitwise, Xor)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b0101).ABC(OpCode::OP_BITXOR, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 0b1010);
}

TEST(VMBitwise, Not)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(OpCode::OP_BITNOT, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), ~int64_t(0));
}

TEST(VMBitwise, Shl)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 1).load_int(1, 8).ABC(OpCode::OP_LSHIFT, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 256);
}

TEST(VMBitwise, Shr)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 1024).load_int(1, 3).ABC(OpCode::OP_RSHIFT, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 128);
}

TEST(VMBitwise, ShrLogical_NegativeInput)
{
    // Logical (unsigned) shift: -8 >> 1 in 64-bit unsigned = large positive.
    // -8 as int64 = 0xFFFFFFFFFFFFFFF8; >> 1 = 0x7FFFFFFFFFFFFFFC = INT64_MAX-3.
    // That value is outside int48 range, so Value::integer stores it as double.
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, -8).load_int(1, 1).ABC(OpCode::OP_RSHIFT, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isNumber());
    // Just verify it's positive (sign bit cleared → logical shift working).
    EXPECT_GT(v.asDoubleAny(), 0.0);
}

TEST(VMBitwise, AndOnDoubleThrows)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k = b.ch->addConstant(Value::real(1.0));
    b.ABx(OpCode::LOAD_CONST, 0, k).load_int(1, 1).ABC(OpCode::OP_BITAND, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

// =============================================================================
// VMCompare — OP_EQ OP_NEQ OP_LT OP_LTE OP_NOT
// =============================================================================

TEST(VMCompare, EqIntsTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && v.asBoolean());
}

TEST(VMCompare, EqIntsFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && !v.asBoolean());
}

TEST(VMCompare, NeqTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_NEQ, 2, 0, 1).ret(2);
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && v.asBoolean());
}

TEST(VMCompare, LtTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 3).load_int(1, 7).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && v.asBoolean());
}

TEST(VMCompare, LtFalseEqual)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && !v.asBoolean());
}

TEST(VMCompare, LeTrue_Equal)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && v.asBoolean());
}

TEST(VMCompare, LeFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && !v.asBoolean());
}

TEST(VMCompare, EqSameString)
{
    // Interned strings with same content → same pointer → OP_EQ bit-equal → true
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.str("hello");
    uint16_t k1 = b.str("hello");
    b.ABx(OpCode::LOAD_CONST, 0, k0)
        .ABx(OpCode::LOAD_CONST, 1, k1)
        .ABC(OpCode::OP_EQ, 2, 0, 1)
        .ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && v.asBoolean());
}

TEST(VMCompare, NotFalseIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && v.asBoolean());
}

TEST(VMCompare, NotTrueIsFalse)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && !v.asBoolean());
}

TEST(VMCompare, NotNilIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_NIL, 0, 0, 1).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && v.asBoolean());
}

TEST(VMCompare, NotZeroIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && v.asBoolean());
}

TEST(VMCompare, LtStrings)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t ka = b.str("apple");
    uint16_t kb = b.str("banana");
    b.ABx(OpCode::LOAD_CONST, 0, ka)
        .ABx(OpCode::LOAD_CONST, 1, kb)
        .ABC(OpCode::OP_LT, 2, 0, 1)
        .ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isBoolean() && v.asBoolean());
}

/*
// =============================================================================
// VMJumps — JUMP, JUMP_IF_TRUE, JUMP_IF_FALSE, LOOP
// =============================================================================

TEST(VMJumps, Unconditional)
{
    // JUMP +1 skips LOAD_INT 0, then loads 99
    VMRunner r;
    CB b;
    b.regs(2)
        .AsBx(OpCode::JUMP, 0, 1) // skip next
        .load_int(1, 0)           // skipped
        .load_int(1, 99)
        .ret(1);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 99);
}

TEST(VMJumps, JumpIfTrueTaken)
{
    VMRunner r;
    CB b;
    b.regs(2)
    .ABC(OpCode::LOAD_TRUE, 0, 0, 0)
    .AsBx(OpCode::JUMP_IF_TRUE, 0, 1) // taken
    .load_int(1, 0)                   // skipped
    .load_int(1, 55)
        .ret(1);
        b.dump();
        EXPECT_EQ(r.run(b).asInteger(), 55);
    }

TEST(VMJumps, JumpIfTrueNotTaken)
{
    VMRunner r;
    CB b;
    b.regs(2)
        .ABC(OpCode::LOAD_FALSE, 0, 0, 0)
        .AsBx(OpCode::JUMP_IF_TRUE, 0, 1) // not taken
        .load_int(1, 77)                  // executed
        .load_int(1, 0)
        .ret(1);
        b.dump();
        EXPECT_EQ(r.run(b).asInteger(), 77);
    }

    TEST(VMJumps, JumpIfFalseTaken)
    {
        VMRunner r;
        CB b;
        b.regs(2)
        .ABC(OpCode::LOAD_FALSE, 0, 0, 0)
        .AsBx(OpCode::JUMP_IF_FALSE, 0, 1) // taken
        .load_int(1, 0)                    // skipped
        .load_int(1, 33)
        .ret(1);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 33);
}

TEST(VMJumps, JumpIfFalseNotTaken)
{
    VMRunner r;
    CB b;
    b.regs(2)
    .ABC(OpCode::LOAD_TRUE, 0, 0, 0)
    .AsBx(OpCode::JUMP_IF_FALSE, 0, 1) // not taken
    .load_int(1, 44)                   // executed
    .load_int(1, 0)
    .ret(1);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 44);
}

TEST(VMJumps, JumpIfFalseOnNilTaken)
{
    VMRunner r;
    CB b;
    b.regs(2)
    .ABC(OpCode::LOAD_NIL, 0, 0, 1)
    .AsBx(OpCode::JUMP_IF_FALSE, 0, 1)
    .load_int(1, 0)
    .load_int(1, 7)
        .ret(1);
        b.dump();
        EXPECT_EQ(r.run(b).asInteger(), 7);
    }

TEST(VMJumps, LoopSum1To5)
{
    // while i<=5: sum+=i; i++  → 15
    // ip0  load_int r0 0    (sum)
    // ip1  load_int r1 1    (i)
    // ip2  load_int r2 5    (limit)
    // ip3  OP_LTE  r3 r1 r2               ← loop top
    // ip4  JUMP_IF_FALSE r3 +4         → ip9
    // ip5  OP_ADD r0 r0 r1
    // ip6  NOP (IC slot 0)
    // ip7  load_int r4 1
    // ip8  OP_ADD r1 r1 r4
    // ip9  NOP (IC slot 1)
    // ip10 LOOP -8                     → ip3  (ip11+(-8)=3 ✓)
    // ip11 RETURN r0
    VMRunner r;
    CB b;
    b.regs(5).slot().slot().load_int(0, 0).load_int(1, 1).load_int(2, 5) // ip0,1,2
    .ABC(OpCode::OP_LTE, 3, 1, 2)                                    // ip3  loop top
    .AsBx(OpCode::JUMP_IF_FALSE, 3, 5)                               // ip4  → ip10
    .ABC(OpCode::OP_ADD, 0, 0, 1)
        .nop(0)         // ip5,6
        .load_int(4, 1) // ip7
        .ABC(OpCode::OP_ADD, 1, 1, 4)
        .nop(1)                    // ip8,9
        .AsBx(OpCode::LOOP, 0, -8) // ip10 → ip3
        .ret(0);
        b.dump();
        EXPECT_EQ(r.run(b).asInteger(), 15);
}

// =============================================================================
// VMForLoop — FOR_PREP + FOR_STEP
//
// Register layout: base=A: A+0=ctr, A+1=limit, A+2=step, A+3=user_idx
// Template pattern (using regs 0-4, result in r4):
//   load_int r0 start; load_int r1 limit; load_int r2 step; load_int r4 0
//   FOR_PREP r0 +N_skip     ip4
//   body:
//     OP_ADD r4 r4 r3 ; NOP   ip5,6
//   FOR_STEP r0 -2          ip7   (→ ip5)
//   ret r4                  ip8
//
// FOR_PREP sBx = offset to land *after* body if loop is empty.
//   ip=4, body_end=ip7 (FOR_STEP), past_body=ip8
//   sBx = ip8 - (ip4+1) = 8-5 = 3
// FOR_STEP sBx = offset to land at *body start*
//   ip=7, body_start=ip5
//   sBx = ip5 - (ip7+1) = 5-8 = -3... but the pattern above has OP_ADD+NOP
//   at ip5,6, FOR_STEP at ip7: sBx = ip5-(ip7+1)=5-8=-3? No:
//   FOR_STEP at ip7, target=ip5: sBx = target-(ip+1) = 5-8 = -3.
//   But the LOOP example above uses -8... let me recount carefully.
//
// Actually FOR_PREP/FOR_STEP sBx is applied as: ip += sBx (like JUMP).
// So to jump from ip4 to ip8: ip becomes ip4+1+sBx=ip8 → sBx=3.
// To jump from ip7 back to ip5: ip becomes ip7+1+sBx=ip5 → sBx=-3.
//
// =============================================================================

TEST(VMForLoop, AscendingSum_1_5)
{
    // for i=1 to 5 step 1 → sum = 15
    // r0..r3 = FOR regs, r4 = sum, r5 = scratch for NOP IC
    // ip0 load_int r0 1; ip1 load_int r1 5; ip2 load_int r2 1; ip3 load_int r4 0
    // ip4 FOR_PREP r0 +3      → ip8 if empty (sBx: ip8=ip4+1+sBx → sBx=3)
    // ip5  OP_ADD r4 r4 r3
    // ip6  NOP
    // ip7 FOR_STEP r0 -3      → ip5 (sBx: ip5=ip7+1+sBx → sBx=-3)
    // ip8 ret r4
    VMRunner r;
    CB b;
    b.regs(5).slot().load_int(0, 1).load_int(1, 5).load_int(2, 1).load_int(4, 0) // ip0-3
    .AsBx(OpCode::FOR_PREP, 0, 3)                                            // ip4
    .ABC(OpCode::OP_ADD, 4, 4, 3)
    .nop(0)                        // ip5,6  body
    .AsBx(OpCode::FOR_STEP, 0, -3) // ip7
    .ret(4);                       // ip8
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 15);
}

TEST(VMForLoop, AscendingStep2)
{
    // for i=0 to 8 step 2 → visits 0,2,4,6,8 → sum=20
    VMRunner r;
    CB b;
    b.regs(5).slot().load_int(0, 0).load_int(1, 8).load_int(2, 2).load_int(4, 0).AsBx(OpCode::FOR_PREP, 0, 3).ABC(OpCode::OP_ADD, 4, 4, 3).nop(0).AsBx(OpCode::FOR_STEP, 0, -3).ret(4);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 20);
}

TEST(VMForLoop, DescendingSum_5_1)
{
    // for i=5 to 1 step -1 → sum=15
    VMRunner r;
    CB b;
    b.regs(5).slot().load_int(0, 5).load_int(1, 1).load_int(2, -1).load_int(4, 0).AsBx(OpCode::FOR_PREP, 0, 3).ABC(OpCode::OP_ADD, 4, 4, 3).nop(0).AsBx(OpCode::FOR_STEP, 0, -3).ret(4);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 15);
}

TEST(VMForLoop, EmptyLoop)
{
    // for i=10 to 1 step 1 → body never runs → sum=0
    VMRunner r;
    CB b;
    b.regs(5).slot().load_int(0, 10).load_int(1, 1).load_int(2, 1).load_int(4, 0).AsBx(OpCode::FOR_PREP, 0, 3).ABC(OpCode::OP_ADD, 4, 4, 3).nop(0).AsBx(OpCode::FOR_STEP, 0, -3).ret(4);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 0);
}

TEST(VMForLoop, SingleIteration)
{
    // for i=7 to 7 step 1 → sum=7
    VMRunner r;
    CB b;
    b.regs(5).slot().load_int(0, 7).load_int(1, 7).load_int(2, 1).load_int(4, 0).AsBx(OpCode::FOR_PREP, 0, 3).ABC(OpCode::OP_ADD, 4, 4, 3).nop(0).AsBx(OpCode::FOR_STEP, 0, -3).ret(4);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 7);
}

// =============================================================================
// VMGlobals — STORE_GLOBAL / LOAD_GLOBAL / set_global API
// =============================================================================
*/

TEST(VMGlobals, StoreAndLoad)
{
    VMRunner r;
    CB b;
    b.regs(2)
        .load_int(0, 123)
        .stg(0, "g")
        .load_int(0, 0) // overwrite r0 to prove we're reading the global
        .ldg(1, "g")
        .ret(1);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 123);
}

TEST(VMGlobals, MissingGlobalIsNil)
{
    VMRunner r;
    CB b;
    b.regs(1).ldg(0, "nope").ret(0);
    EXPECT_TRUE(r.run(b).isNil());
}

TEST(VMGlobals, Overwrite)
{
    VMRunner r;
    CB b;
    b.regs(2)
        .load_int(0, 1)
        .stg(0, "x")
        .load_int(0, 2)
        .stg(0, "x")
        .ldg(1, "x")
        .ret(1);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 2);
}

// =============================================================================
// VMConcat — CONCAT A B C  (dst=A, first-reg=B, count=C)
// =============================================================================
/*
TEST(VMConcat, TwoStrings)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.str("foo"), k1 = b.str("bar");
    b.ABx(OpCode::LOAD_CONST, 0, k0)
        .ABx(OpCode::LOAD_CONST, 1, k1)
        .ABC(OpCode::CONCAT, 2, 0, 2)
        .ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString()->str, "foobar");
}

TEST(VMConcat, IntegerToString)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 42).ABC(OpCode::CONCAT, 1, 0, 1).ret(1);
    Value v = r.run(b);
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString()->str, "42");
}

TEST(VMConcat, ThreeParts)
{
    VMRunner r;
    CB b;
    b.regs(4);
    uint16_t ka = b.str("a"), kb = b.str("b"), kc = b.str("c");
    b.ABx(OpCode::LOAD_CONST, 0, ka)
        .ABx(OpCode::LOAD_CONST, 1, kb)
        .ABx(OpCode::LOAD_CONST, 2, kc)
        .ABC(OpCode::CONCAT, 3, 0, 3)
        .ret(3);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString()->str, "abc");
}

TEST(VMConcat, BoolAndNil)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .ABC(OpCode::LOAD_TRUE, 0, 0, 0)
        .ABC(OpCode::LOAD_NIL, 1, 1, 1)
        .ABC(OpCode::CONCAT, 2, 0, 2)
        .ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString()->str, "truenil");
}
*/

// =============================================================================
// VMLists — LIST_NEW LIST_APPEND LIST_GET LIST_SET LIST_LEN
// =============================================================================

TEST(VMLists, NewEmpty)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LIST_NEW, 0, 0, 0).ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 0);
}

TEST(VMLists, AppendAndLen)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LIST_NEW, 0, 3, 0);
    for (int v : { 10, 20, 30 })
        b.load_int(1, v).ABC(OpCode::LIST_APPEND, 0, 1, 0);
    b.ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 3);
}

TEST(VMLists, GetFirst)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(OpCode::LIST_NEW, 0, 2, 0).load_int(1, 77).ABC(OpCode::LIST_APPEND, 0, 1, 0).load_int(1, 88).ABC(OpCode::LIST_APPEND, 0, 1, 0).load_int(1, 0).ABC(OpCode::LIST_GET, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 77);
}

TEST(VMLists, GetLast)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(OpCode::LIST_NEW, 0, 2, 0).load_int(1, 10).ABC(OpCode::LIST_APPEND, 0, 1, 0).load_int(1, 20).ABC(OpCode::LIST_APPEND, 0, 1, 0).load_int(1, 1).ABC(OpCode::LIST_GET, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 20);
}

TEST(VMLists, Set)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(OpCode::LIST_NEW, 0, 1, 0).load_int(1, 0).ABC(OpCode::LIST_APPEND, 0, 1, 0).load_int(1, 0).load_int(2, 99).ABC(OpCode::LIST_SET, 0, 1, 2).load_int(1, 0).ABC(OpCode::LIST_GET, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 99);
}

TEST(VMLists, OutOfBoundsThrows)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(OpCode::LIST_NEW, 0, 0, 0).load_int(1, 0).ABC(OpCode::LIST_GET, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMLists, LenOnNonListThrows)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 5).ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

// =============================================================================
// VMCalls — CALL, CALL_TAIL, IC_CALL
// =============================================================================

// Helper: build a 2-arg adder function chunk
static std::unique_ptr<Chunk> make_adder_chunk()
{
    auto fn = std::make_unique<Chunk>();
    fn->name = "add2";
    fn->arity = 2;
    fn->localCount = 3;
    // r0=a r1=b (params); r2=a+b
    fn->emit(make_ABC(OP(OpCode::OP_ADD), 2, 0, 1), 1);
    fn->emit(make_ABC(OP(OpCode::RETURN), 2, 1, 0), 1);
    return fn;
}

TEST(VMCalls, CallClosure_TwoArgs)
{
    // CLOSURE r0 fn[0]; load_int r1 3; load_int r2 4; CALL r0 2; RETURN r0
    auto top = std::make_unique<Chunk>();
    top->name = "<test>";
    top->localCount = 4;
    top->functions.push_back(make_adder_chunk().release());

    top->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1); // no upval descs
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(3)), 1);
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 2, BX(4)), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 2, 0), 1);
    top->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(r.run(std::move(top)).asInteger(), 7);
}

TEST(VMCalls, WrongArgcThrows)
{
    auto top = std::make_unique<Chunk>();
    top->name = "<test>";
    top->localCount = 3;
    top->functions.push_back(make_adder_chunk().release());

    top->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(1)), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 1, 0), 1); // 1 arg, expects 2
    top->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    top->disassemble();
    VMRunner r;
    std::string err = r.throws(std::move(top));
    EXPECT_FALSE(err.empty());
    EXPECT_NE(err.find("args"), std::string::npos);
}

TEST(VMCalls, CallNonFunctionThrows)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 5).ABC(OpCode::CALL, 0, 0, 0).ret(0);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMCalls, ICCallNativeLen)
{
    // IC_CALL r1 argc=1 ic=0  on stdlib len([1,2,3]) → 3
    VMRunner r;
    CB b;
    b.regs(3).slot().ABC(OpCode::LIST_NEW, 0, 3, 0).load_int(2, 1).ABC(OpCode::LIST_APPEND, 0, 2, 0).load_int(2, 2).ABC(OpCode::LIST_APPEND, 0, 2, 0).load_int(2, 3).ABC(OpCode::LIST_APPEND, 0, 2, 0).ldg(1, "len").mov(2, 0).ABC(OpCode::IC_CALL, 1, 1, 0).ret(1);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 3);
}

TEST(VMCalls, TailCall_DoesNotOverflowFrames)
{
    // A tail-recursive countdown from 300 to 0.
    // If frames were not reused this would hit the 256-frame limit.
    //
    // fn countdown(n):
    //   r0=n  r1=tmp  r2=fnclosure
    //   LOAD_INT r1 0
    //   OP_EQ r1 r0 r1
    //   JUMP_IF_FALSE +1
    //   RETURN r0
    //   LOAD_INT r1 1; OP_SUB r0 r0 r1
    //   LOAD_GLOBAL r2 "cd"
    //   MOVE r3 r0            ← arg for tail call
    //   CALL_TAIL r2 1

    auto fn = std::make_unique<Chunk>();
    fn->name = "cd";
    fn->arity = 1;
    fn->localCount = 4;

    uint16_t nk = fn->addConstant(Value::object(new ObjString("cd")));

    fn->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(0)), 1);
    fn->emit(make_ABC(OP(OpCode::OP_EQ), 1, 0, 1), 1);
    fn->emit(make_AsBx(OP(OpCode::JUMP_IF_FALSE), 1, 1), 1); // → ip4
    fn->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);      // ip3
    fn->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(1)), 1);   // ip4
    fn->emit(make_ABC(OP(OpCode::OP_SUB), 0, 0, 1), 1);
    fn->emit(make_ABx(OP(OpCode::LOAD_GLOBAL), 2, nk), 1);
    fn->emit(make_ABC(OP(OpCode::MOVE), 3, 0, 0), 1); // arg
    fn->emit(make_ABC(OP(OpCode::CALL_TAIL), 2, 1, 0), 1);

    auto top = std::make_unique<Chunk>();
    top->name = "<test>";
    top->localCount = 3;
    top->functions.push_back(fn.release());

    uint16_t tk = top->addConstant(Value::object(new ObjString("cd")));
    top->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    top->emit(make_ABx(OP(OpCode::STORE_GLOBAL), 0, tk), 1);
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(300)), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 1, 0), 1);
    top->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    top->disassemble();
    VMRunner r;
    Value v = r.run(std::move(top));
    EXPECT_EQ(v.asInteger(), 0);
}

TEST(VMCalls, StackOverflowDetected)
{
    // Non-tail infinite recursion hits MAX_FRAMES.
    auto fn = std::make_unique<Chunk>();
    fn->name = "inf";
    fn->arity = 0;
    fn->localCount = 1;
    uint16_t nk = fn->addConstant(Value::object(new ObjString("inf")));
    fn->emit(make_ABx(OP(OpCode::LOAD_GLOBAL), 0, nk), 1);
    fn->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1);
    fn->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);

    auto top = std::make_unique<Chunk>();
    top->name = "<test>";
    top->localCount = 1;
    top->functions.push_back(fn.release());
    uint16_t tk = top->addConstant(Value::object(new ObjString("inf")));
    top->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    top->emit(make_ABx(OP(OpCode::STORE_GLOBAL), 0, tk), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1);
    top->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    top->disassemble();
    VMRunner r;
    std::string err = r.throws(std::move(top));
    EXPECT_FALSE(err.empty());
    EXPECT_NE(err.find("overflow"), std::string::npos);
}

// =============================================================================
// VMClosures — CLOSURE / GET_UPVALUE / SET_UPVALUE / CLOSE_UPVALUE
// =============================================================================

TEST(VMClosures, CaptureLocal_GetUpvalue)
{
    // fn outer(): x=10; fn inner(): return x; return inner()
    auto inner = std::make_unique<Chunk>();
    inner->name = "inner";
    inner->arity = 0;
    inner->upvalueCount = 1;
    inner->localCount = 1;
    inner->emit(make_ABC(OP(OpCode::GET_UPVALUE), 0, 0, 0), 1);
    inner->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);

    auto outer = std::make_unique<Chunk>();
    outer->name = "outer";
    outer->arity = 0;
    outer->localCount = 2;
    outer->upvalueCount = 0;
    outer->functions.push_back(inner.release());
    outer->emit(make_ABx(OP(OpCode::LOAD_INT), 0, BX(10)), 1); // x=10
    outer->emit(make_ABx(OP(OpCode::CLOSURE), 1, 0), 1);       // CLOSURE r1 fn[0]
    outer->emit(make_ABC(OP(OpCode::MOVE), 1, 0, 0), 1);       //   upval: local r0
    outer->emit(make_ABC(OP(OpCode::CALL), 1, 0, 0), 1);       // inner()
    outer->emit(make_ABC(OP(OpCode::RETURN), 1, 1, 0), 1);

    auto top = std::make_unique<Chunk>();
    top->name = "<test>";
    top->localCount = 2;
    top->functions.push_back(outer.release());
    top->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1);
    top->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(r.run(std::move(top)).asInteger(), 10);
}

TEST(VMClosures, SetUpvalue_CounterReaches3)
{
    // make_counter() returns an inc closure; calling it 3x returns 3.
    auto inc = std::make_unique<Chunk>();
    inc->name = "inc";
    inc->arity = 0;
    inc->upvalueCount = 1;
    inc->localCount = 2;
    inc->emit(make_ABC(OP(OpCode::GET_UPVALUE), 0, 0, 0), 1); // r0=n
    inc->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(1)), 1);
    inc->emit(make_ABC(OP(OpCode::OP_ADD), 0, 0, 1), 1);
    inc->emit(make_ABC(OP(OpCode::SET_UPVALUE), 0, 0, 0), 1); // n=r0
    inc->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);

    auto mc = std::make_unique<Chunk>();
    mc->name = "mc";
    mc->arity = 0;
    mc->localCount = 2;
    mc->upvalueCount = 0;
    mc->functions.push_back(inc.release());
    mc->emit(make_ABx(OP(OpCode::LOAD_INT), 0, BX(0)), 1); // n=0
    mc->emit(make_ABx(OP(OpCode::CLOSURE), 1, 0), 1);
    mc->emit(make_ABC(OP(OpCode::MOVE), 1, 0, 0), 1); // upval local r0
    mc->emit(make_ABC(OP(OpCode::RETURN), 1, 1, 0), 1);

    auto top = std::make_unique<Chunk>();
    top->name = "<test>";
    top->localCount = 2;
    top->functions.push_back(mc.release());
    // r0=mc; CALL(mc)→r0=inc; then call inc 3x; result in r0
    top->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1); // r0=inc
    top->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1); // call inc → 1
    top->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1); // call inc → 2
    top->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1); // call inc → 3
    top->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(r.run(std::move(top)).asInteger(), 3);
}

TEST(VMClosures, CloseUpvalue_SurvivesFrame)
{
    // make_adder(x) returns add(y)=x+y; the upvalue must survive frame pop.
    auto add = std::make_unique<Chunk>();
    add->name = "add";
    add->arity = 1;
    add->upvalueCount = 1;
    add->localCount = 3;
    add->emit(make_ABC(OP(OpCode::GET_UPVALUE), 1, 0, 0), 1); // r1=x
    add->emit(make_ABC(OP(OpCode::OP_ADD), 2, 1, 0), 1);      // r2=x+y(r0)
    add->emit(make_ABC(OP(OpCode::RETURN), 2, 1, 0), 1);

    auto ma = std::make_unique<Chunk>();
    ma->name = "ma";
    ma->arity = 1;
    ma->localCount = 2;
    ma->upvalueCount = 0;
    ma->functions.push_back(add.release());
    ma->emit(make_ABx(OP(OpCode::CLOSURE), 1, 0), 1);
    ma->emit(make_ABC(OP(OpCode::MOVE), 1, 0, 0), 1);          // upval local r0=x
    ma->emit(make_ABC(OP(OpCode::CLOSE_UPVALUE), 0, 0, 0), 1); // close x
    ma->emit(make_ABC(OP(OpCode::RETURN), 1, 1, 0), 1);

    auto top = std::make_unique<Chunk>();
    top->name = "<test>";
    top->localCount = 3;
    top->functions.push_back(ma.release());
    // r0=ma; call ma(10)→r0=add; call add(5)→r0=15
    top->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(10)), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 1, 0), 1); // add5=ma(10)
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(5)), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 1, 0), 1); // add5(5)=15
    top->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    top->disassemble();

    VMRunner r;
    EXPECT_EQ(r.run(std::move(top)).asInteger(), 15);
}

TEST(VMClosures, SharedUpvalue_TwoClosuresOneSlot)
{
    // get/set closures share upvalue n; set(99) then get() returns 99.
    auto get_fn = std::make_unique<Chunk>();
    get_fn->name = "get";
    get_fn->arity = 0;
    get_fn->upvalueCount = 1;
    get_fn->localCount = 1;
    get_fn->emit(make_ABC(OP(OpCode::GET_UPVALUE), 0, 0, 0), 1);
    get_fn->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);

    auto set_fn = std::make_unique<Chunk>();
    set_fn->name = "set";
    set_fn->arity = 1;
    set_fn->upvalueCount = 1;
    set_fn->localCount = 1;
    set_fn->emit(make_ABC(OP(OpCode::SET_UPVALUE), 0, 0, 0), 1); // upval[0]=r0(v)
    set_fn->emit(make_ABC(OP(OpCode::RETURN_NIL), 0, 0, 0), 1);

    auto mp = std::make_unique<Chunk>();
    mp->name = "mp";
    mp->arity = 0;
    mp->localCount = 4;
    mp->upvalueCount = 0;
    mp->functions.push_back(get_fn.release()); // fn[0]
    mp->functions.push_back(set_fn.release()); // fn[1]
    // n=r0=0; get=r1; set=r2; list=r3
    mp->emit(make_ABx(OP(OpCode::LOAD_INT), 0, BX(0)), 1);
    mp->emit(make_ABx(OP(OpCode::CLOSURE), 1, 0), 1);
    mp->emit(make_ABC(OP(OpCode::MOVE), 1, 0, 0), 1); // upval local r0
    mp->emit(make_ABx(OP(OpCode::CLOSURE), 2, 1), 1);
    mp->emit(make_ABC(OP(OpCode::MOVE), 1, 0, 0), 1); // upval local r0
    mp->emit(make_ABC(OP(OpCode::LIST_NEW), 3, 2, 0), 1);
    mp->emit(make_ABC(OP(OpCode::LIST_APPEND), 3, 1, 0), 1); // [get]
    mp->emit(make_ABC(OP(OpCode::LIST_APPEND), 3, 2, 0), 1); // [get,set]
    mp->emit(make_ABC(OP(OpCode::CLOSE_UPVALUE), 0, 0, 0), 1);
    mp->emit(make_ABC(OP(OpCode::RETURN), 3, 1, 0), 1);

    auto top = std::make_unique<Chunk>();
    top->name = "<test>";
    top->localCount = 5;
    top->functions.push_back(mp.release());
    // r0=pair list; call mp
    top->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1); // r0=[get,set]
    // r1=pair[1]=set; call set(99): set is at r1, arg at r2
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 2, BX(1)), 1);
    top->emit(make_ABC(OP(OpCode::LIST_GET), 1, 0, 2), 1); // r1=set
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 2, BX(99)), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 1, 1, 0), 1); // set(99)
    // r1=pair[0]=get; call get()
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 2, BX(0)), 1);
    top->emit(make_ABC(OP(OpCode::LIST_GET), 1, 0, 2), 1); // r1=get
    top->emit(make_ABC(OP(OpCode::CALL), 1, 0, 0), 1);     // get()
    top->emit(make_ABC(OP(OpCode::RETURN), 1, 1, 0), 1);
    top->disassemble();

    VMRunner r;
    EXPECT_EQ(r.run(std::move(top)).asInteger(), 99);
}

// =============================================================================
// VMICProfile — IC slot population
// =============================================================================

TEST(VMICProfile, BinaryOpUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 3).load_int(1, 4).ABC(OpCode::OP_ADD, 2, 0, 1).nop(0).ret(2);
    r.run(b);
    b.dump();
    auto const& s = r.chunk_->icSlots[0];
    EXPECT_TRUE(hasTag(s.seenLhs, TypeTag::INT));
    EXPECT_TRUE(hasTag(s.seenRhs, TypeTag::INT));
    EXPECT_TRUE(hasTag(s.seenRet, TypeTag::INT));
    EXPECT_GE(s.hitCount, 1u);
}

TEST(VMICProfile, SubUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 3).ABC(OpCode::OP_SUB, 2, 0, 1).nop(0).ret(2);
    b.dump();
    r.run(b);
    EXPECT_GE(r.chunk_->icSlots[0].hitCount, 1u);
}

TEST(VMICProfile, ICCallUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().ABC(OpCode::LIST_NEW, 0, 2, 0).load_int(2, 1).ABC(OpCode::LIST_APPEND, 0, 2, 0).load_int(2, 2).ABC(OpCode::LIST_APPEND, 0, 2, 0).ldg(1, "len").mov(2, 0).ABC(OpCode::IC_CALL, 1, 1, 0).ret(1);
    b.dump();
    r.run(b);
    auto const& s = r.chunk_->icSlots[0];
    EXPECT_TRUE(hasTag(s.seenLhs, TypeTag::NATIVE));
    EXPECT_TRUE(hasTag(s.seenRet, TypeTag::INT));
    EXPECT_GE(s.hitCount, 1u);
}

TEST(VMICProfile, SlotAccumulatesAcrossLoopIterations)
{
    // Loop 5 times doing OP_ADD; hitCount should be 5.
    VMRunner r;
    CB b;
    b.regs(5).slot().load_int(0, 0).load_int(1, 5).load_int(2, 0) // i=0, limit=5, sum=0
        .ABC(OpCode::OP_LT, 3, 0, 1)                              // ip3 loop top
        .AsBx(OpCode::JUMP_IF_FALSE, 3, 4)                        // ip4 → ip9
        .load_int(4, 1)                                           // ip5
        .ABC(OpCode::OP_ADD, 0, 0, 4)
        .nop(0)                    // ip6,7  i+=1
        .AsBx(OpCode::LOOP, 0, -6) // ip8 → ip3
        .ret(0);                   // ip9
    b.dump();
    r.run(b);
    EXPECT_EQ(r.chunk_->icSlots[0].hitCount, 5u);
}

// =============================================================================
// VMIntegration — full programs as bytecode
// =============================================================================

TEST(VMIntegration, Fibonacci_fib10_equals_55)
{
    // fn fib(n): if n<=1: return n; return fib(n-1)+fib(n-2)
    // Registers in fib: r0=n r1=tmp r2=cond r3=a r4=fib r5=arg r6=b r7=fib r8=arg
    auto fib = std::make_unique<Chunk>();
    fib->name = "fib";
    fib->arity = 1;
    fib->localCount = 9;
    uint16_t nk = fib->addConstant(Value::object(new ObjString("fib")));

    // ip0: load_int r1 1
    fib->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(1)), 1);
    // ip1: OP_LTE r2 r0 r1   (n<=1)
    fib->emit(make_ABC(OP(OpCode::OP_LTE), 2, 0, 1), 1);
    // ip2: JUMP_IF_FALSE r2 +1  → ip4
    fib->emit(make_AsBx(OP(OpCode::JUMP_IF_FALSE), 2, 1), 1);
    // ip3: RETURN r0
    fib->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    // ip4: OP_SUB r3 r0 r1  (n-1)
    fib->emit(make_ABC(OP(OpCode::OP_SUB), 3, 0, 1), 1);
    // ip5: LOAD_GLOBAL r4 "fib"
    fib->emit(make_ABx(OP(OpCode::LOAD_GLOBAL), 4, nk), 1);
    // ip6: MOVE r5 r3
    fib->emit(make_ABC(OP(OpCode::MOVE), 5, 3, 0), 1);
    // ip7: CALL r4 1   → fib(n-1) lands in r4
    fib->emit(make_ABC(OP(OpCode::CALL), 4, 1, 0), 1);
    // ip8: load_int r1 2
    fib->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(2)), 1);
    // ip9: OP_SUB r6 r0 r1  (n-2) — r0 still holds original n
    fib->emit(make_ABC(OP(OpCode::OP_SUB), 6, 0, 1), 1);
    // ip10: LOAD_GLOBAL r7 "fib"
    fib->emit(make_ABx(OP(OpCode::LOAD_GLOBAL), 7, nk), 1);
    // ip11: MOVE r8 r6
    fib->emit(make_ABC(OP(OpCode::MOVE), 8, 6, 0), 1);
    // ip12: CALL r7 1  → fib(n-2) lands in r7
    fib->emit(make_ABC(OP(OpCode::CALL), 7, 1, 0), 1);
    // ip13: OP_ADD r4 r4 r7
    fib->emit(make_ABC(OP(OpCode::OP_ADD), 4, 4, 7), 1);
    // ip14: RETURN r4
    fib->emit(make_ABC(OP(OpCode::RETURN), 4, 1, 0), 1);

    auto top = std::make_unique<Chunk>();
    top->name = "<test>";
    top->localCount = 3;
    top->functions.push_back(fib.release());
    uint16_t tk = top->addConstant(Value::object(new ObjString("fib")));
    top->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    top->emit(make_ABx(OP(OpCode::STORE_GLOBAL), 0, tk), 1);
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(10)), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 1, 0), 1);
    top->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(r.run(std::move(top)).asInteger(), 55);
}

TEST(VMIntegration, SumForLoop_1_to_100)
{
    // for i=1,100,1 → sum=5050
    VMRunner r;
    CB b;
    b.regs(5).slot().load_int(0, 1).load_int(1, 100).load_int(2, 1).load_int(4, 0).AsBx(OpCode::FOR_PREP, 0, 3).ABC(OpCode::OP_ADD, 4, 4, 3).nop(0).AsBx(OpCode::FOR_STEP, 0, -3).ret(4);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 5050);
}

TEST(VMIntegration, StringConcat_3Parts)
{
    GTEST_SKIP() << "Not implemented yet.";
    VMRunner r;
    CB b;
    b.regs(4);
    b.ABx(OpCode::LOAD_CONST, 0, b.str("hello"))
        .ABx(OpCode::LOAD_CONST, 1, b.str(", "))
        .ABx(OpCode::LOAD_CONST, 2, b.str("world"))
        .ABC(OpCode::CONCAT, 3, 0, 3)
        .ret(3);
    b.dump();
    Value v = r.run(b);
    EXPECT_EQ(v.asString()->str, "hello, world");
}

TEST(VMIntegration, ListSquaresViaForLoop)
{
    GTEST_SKIP() << "Not implemented yet.";
    // for i=0,9,1: list.append(i*i)  then list[5]==25
    VMRunner r;
    CB b;
    b.regs(6).slot().slot().load_int(0, 0).load_int(1, 9).load_int(2, 1).ABC(OpCode::LIST_NEW, 4, 10, 0).AsBx(OpCode::FOR_PREP, 0, 5) // ip4 → ip10 if empty
        .ABC(OpCode::OP_MUL, 5, 3, 3)
        .nop(0)                            // ip5,6  i*i
        .ABC(OpCode::LIST_APPEND, 4, 5, 0) // ip7
        .ABC(OpCode::NOP, 1, 0, 0)         // ip8  (second IC slot NOP)
        .AsBx(OpCode::FOR_STEP, 0, -5)     // ip9 → ip5
        .load_int(5, 5)
        .ABC(OpCode::LIST_GET, 5, 4, 5)
        .ret(5);
    b.dump();
    EXPECT_EQ(r.run(b).asInteger(), 25);
}

TEST(VMIntegration, NestedAdderClosure)
{
    // make_adder(x) → add(y) = x+y; add5=make_adder(5); add5(3)==8
    auto add_fn = std::make_unique<Chunk>();
    add_fn->name = "add";
    add_fn->arity = 1;
    add_fn->upvalueCount = 1;
    add_fn->localCount = 3;
    add_fn->emit(make_ABC(OP(OpCode::GET_UPVALUE), 1, 0, 0), 1); // r1=x
    add_fn->emit(make_ABC(OP(OpCode::OP_ADD), 2, 1, 0), 1);      // r2=x+y
    add_fn->emit(make_ABC(OP(OpCode::RETURN), 2, 1, 0), 1);

    auto make_adder = std::make_unique<Chunk>();
    make_adder->name = "ma";
    make_adder->arity = 1;
    make_adder->localCount = 2;
    make_adder->functions.push_back(add_fn.release());
    make_adder->emit(make_ABx(OP(OpCode::CLOSURE), 1, 0), 1);
    make_adder->emit(make_ABC(OP(OpCode::MOVE), 1, 0, 0), 1); // upval local r0
    make_adder->emit(make_ABC(OP(OpCode::CLOSE_UPVALUE), 0, 0, 0), 1);
    make_adder->emit(make_ABC(OP(OpCode::RETURN), 1, 1, 0), 1);

    auto top = std::make_unique<Chunk>();
    top->name = "<test>";
    top->localCount = 3;
    top->functions.push_back(make_adder.release());
    top->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(5)), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 1, 0), 1); // add5=make_adder(5)
    top->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(3)), 1);
    top->emit(make_ABC(OP(OpCode::CALL), 0, 1, 0), 1); // add5(3)
    top->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    top->disassemble();

    VMRunner r;
    EXPECT_EQ(r.run(std::move(top)).asInteger(), 8);
}

TEST(VMIntegration, CounterFromGlobal_MultipleVMs)
{
    // Two independent VMs; each counter is independent.
    auto make_mc_chunk = []() {
        auto inc = std::make_unique<Chunk>();
        inc->name = "inc";
        inc->arity = 0;
        inc->upvalueCount = 1;
        inc->localCount = 2;
        inc->emit(make_ABC(OP(OpCode::GET_UPVALUE), 0, 0, 0), 1);
        inc->emit(make_ABx(OP(OpCode::LOAD_INT), 1, BX(1)), 1);
        inc->emit(make_ABC(OP(OpCode::OP_ADD), 0, 0, 1), 1);
        inc->emit(make_ABC(OP(OpCode::SET_UPVALUE), 0, 0, 0), 1);
        inc->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);

        auto mc = std::make_unique<Chunk>();
        mc->name = "mc";
        mc->arity = 0;
        mc->localCount = 2;
        mc->functions.push_back(inc.release());
        mc->emit(make_ABx(OP(OpCode::LOAD_INT), 0, BX(0)), 1);
        mc->emit(make_ABx(OP(OpCode::CLOSURE), 1, 0), 1);
        mc->emit(make_ABC(OP(OpCode::MOVE), 1, 0, 0), 1);
        mc->emit(make_ABC(OP(OpCode::RETURN), 1, 1, 0), 1);
        return mc;
    };

    // VM A: call inc twice
    auto topA = std::make_unique<Chunk>();
    topA->name = "<A>";
    topA->localCount = 2;
    topA->functions.push_back(make_mc_chunk().release());
    topA->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    topA->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1);
    topA->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1);
    topA->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1);
    topA->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);

    // VM B: call inc once
    auto topB = std::make_unique<Chunk>();
    topB->name = "<B>";
    topB->localCount = 2;
    topB->functions.push_back(make_mc_chunk().release());
    topB->emit(make_ABx(OP(OpCode::CLOSURE), 0, 0), 1);
    topB->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1);
    topB->emit(make_ABC(OP(OpCode::CALL), 0, 0, 0), 1);
    topB->emit(make_ABC(OP(OpCode::RETURN), 0, 1, 0), 1);
    topA->disassemble();
    topB->disassemble();

    VMRunner a, b_vm;
    EXPECT_EQ(a.run(std::move(topA)).asInteger(), 3);
    EXPECT_EQ(b_vm.run(std::move(topB)).asInteger(), 2);
}

} // namespace mylang::runtime
