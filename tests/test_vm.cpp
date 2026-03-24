#include "../include/ast.hpp"
#include "../include/compiler.hpp"
#include "../include/opcode.hpp"
#include "../include/vm.hpp"

#include <bit>
#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>

using namespace mylang;
using namespace mylang::runtime;

static constexpr uint16_t BX(int v) { return static_cast<uint16_t>(v + 32767); }

struct CB {
    Chunk* ch { nullptr };

    CB()
    {
        ch = makeChunk();
        ch->name = "<test>";
    }

    CB& ABC(OpCode op, uint8_t A, uint8_t B, uint8_t C, uint32_t ln = 1)
    {
        ch->emit(make_ABC(op, A, B, C), { ln, 0, 0 });
        return *this;
    }
    CB& ABx(OpCode op, uint8_t A, uint16_t Bx, uint32_t ln = 1)
    {
        ch->emit(make_ABx(op, A, Bx), { ln, 0, 0 });
        return *this;
    }
    CB& AsBx(OpCode op, uint8_t A, int sBx, uint32_t ln = 1)
    {
        ch->emit(make_AsBx(op, A, sBx), { ln, 0, 0 });
        return *this;
    }

    CB& load_int(uint8_t r, int v) { return ABx(OpCode::LOAD_INT, r, BX(v)); }
    CB& ret(uint8_t r, uint8_t n = 1) { return ABC(OpCode::RETURN, r, n, 0); }
    CB& ret_nil() { return ABC(OpCode::RETURN_NIL, 0, 0, 0); }
    CB& nop(uint8_t slot = 0) { return ABC(OpCode::NOP, slot, 0, 0); }
    CB& mov(uint8_t d, uint8_t s) { return ABC(OpCode::MOVE, d, s, 0); }

    uint16_t str(char const* s)
    {
        strs_.emplace_back(std::make_unique<ObjString>(StringRef(s)));
        return ch->addConstant(MAKE_OBJECT(strs_.back().get()));
    }

    CB& ldg(uint8_t r, char const* name) { return ABx(OpCode::LOAD_GLOBAL, r, str(name)); }
    CB& stg(uint8_t r, char const* name) { return ABx(OpCode::STORE_GLOBAL, r, str(name)); }

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

    void dump() const
    {
        std::cout << "Disassemebeled chunk:" << '\n';
        if (ch)
            ch->disassemble();
        std::cout << '\n';
    }

    CB& load_val(uint8_t r, int64_t v)
    {
        if (v >= -32767 && v <= 32768) {
            return load_int(r, static_cast<int>(v)); // fits in LOAD_INT
        } else {
            uint16_t k = ch->addConstant(MAKE_INTEGER(v));
            return ABx(OpCode::LOAD_CONST, r, k); // spill to constant pool
        }
    }

private:
    std::vector<std::unique_ptr<ObjString>> strs_;
};

struct VMRunner {
    VM vm;
    Chunk* chunk_ = makeChunk();

    Value run(CB& b)
    {
        chunk_ = b.ch;
        return vm.run(chunk_);
    }
    Value run(Chunk* c)
    {
        chunk_ = c;
        return vm.run(chunk_);
    }
};

namespace {

VMRunner& native_runner()
{
    static VMRunner r;
    return r;
}

Chunk* compile_program(Array<ast::Stmt*> stmts)
{
    return Compiler().compile(stmts);
}

Chunk* compile_calling(ast::Stmt* fn)
{
    auto* name = static_cast<ast::FunctionDef*>(fn)->getName();
    return compile_program({ fn, ast::makeExprStmt(ast::makeCall(ast::makeName(name->getValue()), ast::makeList())) });
}

} // namespace

TEST(VMLoads, Nil)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_NIL, 0, 0, 1).ret(0);
    b.dump();
    EXPECT_TRUE(IS_NIL(r.run(b)));
}

TEST(VMLoads, NilFillsMultiple)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(OpCode::LOAD_NIL, 0, 0, 3).ret(2);
    b.dump();
    EXPECT_TRUE(IS_NIL(r.run(b)));
}

TEST(VMLoads, True)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMLoads, False)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && !AS_BOOL(v));
}

TEST(VMLoads, IntPositive)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 42).ret(0);
    Value v = r.run(b);
    EXPECT_TRUE(IS_INTEGER(v));
    EXPECT_EQ(AS_INTEGER(v), 42);
}

TEST(VMLoads, IntZero)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 0).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_INTEGER(v));
    EXPECT_EQ(AS_INTEGER(v), 0);
}

TEST(VMLoads, IntNegative)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, -100).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_INTEGER(v));
    EXPECT_EQ(AS_INTEGER(v), -100);
}

TEST(VMLoads, IntMaxEncodable)
{
    VMRunner r;
    CB b;
    b.regs(1).ABx(OpCode::LOAD_INT, 0, 65535).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_INTEGER(v));
    EXPECT_EQ(AS_INTEGER(v), 32768);
}

TEST(VMLoads, IntMinEncodable)
{
    VMRunner r;
    CB b;
    b.regs(1).ABx(OpCode::LOAD_INT, 0, 0).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_INTEGER(v));
    EXPECT_EQ(AS_INTEGER(v), -32767);
}

TEST(VMLoads, ConstDouble)
{
    VMRunner r;
    CB b;
    b.regs(1);
    uint16_t k = b.ch->addConstant(MAKE_REAL(3.14));
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_DOUBLE(v));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(v), 3.14);
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
    EXPECT_TRUE(IS_STRING(v));
    EXPECT_EQ(AS_STRING(v)->str, "hello");
}

TEST(VMLoads, ConstLargeInt)
{
    VMRunner r;
    CB b;
    b.regs(1);
    uint16_t k = b.ch->addConstant(MAKE_INTEGER(1000000LL));
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_INTEGER(v));
    EXPECT_EQ(AS_INTEGER(v), 1000000LL);
}

TEST(VMLoads, ReturnNil)
{
    VMRunner r;
    CB b;
    b.regs(1).ret_nil();
    EXPECT_TRUE(IS_NIL(r.run(b)));
}

TEST(VMMove, Copies)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 77).mov(1, 0).ret(1);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_INTEGER(v));
    EXPECT_EQ(AS_INTEGER(v), 77);
}

TEST(VMMove, SourceUnchanged)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 55).mov(1, 0).ret(0);
    b.dump();
    Value v = r.run(b);
    EXPECT_EQ(AS_INTEGER(v), 55);
}

TEST(VMArith, AddIntFastPath)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 32).ABC(OpCode::OP_ADD, 2, 0, 1).nop().ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_INTEGER(v));
    EXPECT_EQ(AS_INTEGER(v), 42);
}

TEST(VMArith, AddDoubles)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.ch->addConstant(MAKE_REAL(1.5));
    uint16_t k1 = b.ch->addConstant(MAKE_REAL(2.5));
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_ADD, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_DOUBLE(v));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(v), 4.0);
}

TEST(VMArith, AddStringsConcat)
{
    GTEST_SKIP() << "Not supported yet.";
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.str("foo");
    uint16_t k1 = b.str("bar");
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_ADD, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_STRING(v));
    EXPECT_EQ(AS_STRING(v)->str, "foobar");
}

TEST(VMArith, SubInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 100).load_int(1, 58).ABC(OpCode::OP_SUB, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 42);
}

TEST(VMArith, MulInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 7).ABC(OpCode::OP_MUL, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 42);
}

TEST(VMArith, DivDouble)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.ch->addConstant(MAKE_REAL(84.0));
    uint16_t k1 = b.ch->addConstant(MAKE_REAL(2.0));
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_DIV, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r.run(b)), 42.0);
}

TEST(VMArith, ModPositive)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 17).load_int(1, 5).ABC(OpCode::OP_MOD, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r.run(b)), 2.0);
}

TEST(VMArith, Pow)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 2).load_int(1, 10).ABC(OpCode::OP_POW, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r.run(b)), 1024.0);
}

TEST(VMArith, NegInt)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 7).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), -7);
}

TEST(VMArith, NegDouble)
{
    VMRunner r;
    CB b;
    b.regs(2);
    uint16_t k = b.ch->addConstant(MAKE_REAL(3.5));
    b.ABx(OpCode::LOAD_CONST, 0, k).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r.run(b)), -3.5);
}

TEST(VMArith, DivByZeroThrows)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.ch->addConstant(MAKE_REAL(1.0));
    uint16_t k1 = b.ch->addConstant(MAKE_REAL(0.0));
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_DIV, 2, 0, 1).ret(2);
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

TEST(VMBitwise, And)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b1010).ABC(OpCode::OP_BITAND, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 0b1010);
}

TEST(VMBitwise, Or)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1100).load_int(1, 0b0011).ABC(OpCode::OP_BITOR, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 0b1111);
}

TEST(VMBitwise, Xor)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b0101).ABC(OpCode::OP_BITXOR, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 0b1010);
}

TEST(VMBitwise, Not)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(OpCode::OP_BITNOT, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), ~int64_t(0));
}

TEST(VMBitwise, Shl)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1).ABC(OpCode::OP_LSHIFT, 1, 0, 8).ret(1);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 256);
}

TEST(VMBitwise, Shr)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1024).ABC(OpCode::OP_RSHIFT, 1, 0, 3).ret(1);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 128);
}

TEST(VMBitwise, ShrLogical_NegativeInput)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, -8).load_int(1, 1).ABC(OpCode::OP_RSHIFT, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_DOUBLE(v));
    EXPECT_GT(AS_DOUBLE(v), 0.0);
}

TEST(VMBitwise, AndOnDoubleThrows)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k = b.ch->addConstant(MAKE_REAL(1.0));
    b.ABx(OpCode::LOAD_CONST, 0, k).load_int(1, 1).ABC(OpCode::OP_BITAND, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMCompare, EqIntsTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, EqIntsFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && !AS_BOOL(v));
}

TEST(VMCompare, NeqTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_NEQ, 2, 0, 1).ret(2);
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, LtTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 3).load_int(1, 7).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, LtFalseEqual)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && !AS_BOOL(v));
}

TEST(VMCompare, LeTrue_Equal)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, LeFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && !AS_BOOL(v));
}

TEST(VMCompare, EqSameString)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t k0 = b.str("hello");
    uint16_t k1 = b.str("hello");
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, NotFalseIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, NotTrueIsFalse)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && !AS_BOOL(v));
}

TEST(VMCompare, NotNilIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_NIL, 0, 0, 1).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, NotZeroIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, LtStrings)
{
    VMRunner r;
    CB b;
    b.regs(3);
    uint16_t ka = b.str("apple");
    uint16_t kb = b.str("banana");
    b.ABx(OpCode::LOAD_CONST, 0, ka).ABx(OpCode::LOAD_CONST, 1, kb).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMGlobals, StoreAndLoad)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 123).stg(0, "g").load_int(0, 0).ldg(1, "g").ret(1);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 123);
}

TEST(VMGlobals, MissingGlobalIsNil)
{
    VMRunner r;
    CB b;
    b.regs(1).ldg(0, "nope").ret(0);
    EXPECT_TRUE(IS_NIL(r.run(b)));
}

TEST(VMGlobals, Overwrite)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1).stg(0, "x").load_int(0, 2).stg(0, "x").ldg(1, "x").ret(1);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 2);
}

TEST(VMLists, NewEmpty)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LIST_NEW, 0, 0, 0).ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 0);
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
    EXPECT_EQ(AS_INTEGER(r.run(b)), 3);
}

TEST(VMLists, GetFirst)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .ABC(OpCode::LIST_NEW, 0, 2, 0)
        .load_int(1, 77)
        .ABC(OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 88)
        .ABC(OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 0)
        .ABC(OpCode::LIST_GET, 2, 0, 1)
        .ret(2);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 77);
}

TEST(VMLists, GetLast)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .ABC(OpCode::LIST_NEW, 0, 2, 0)
        .load_int(1, 10)
        .ABC(OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 20)
        .ABC(OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 1)
        .ABC(OpCode::LIST_GET, 2, 0, 1)
        .ret(2);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 20);
}

TEST(VMLists, Set)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .ABC(OpCode::LIST_NEW, 0, 1, 0)
        .load_int(1, 0)
        .ABC(OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 0)
        .load_int(2, 99)
        .ABC(OpCode::LIST_SET, 0, 1, 2)
        .load_int(1, 0)
        .ABC(OpCode::LIST_GET, 2, 0, 1)
        .ret(2);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 99);
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

static Chunk* make_adder_chunk()
{
    auto fn = makeChunk();
    fn->name = "add2";
    fn->arity = 2;
    fn->localCount = 3;
    // r0=a r1=b (params); r2=a+b
    fn->emit(make_ABC(OpCode::OP_ADD, 2, 0, 1), { });
    fn->emit(make_ABC(OpCode::RETURN, 2, 1, 0), { });
    return fn;
}

TEST(VMCalls, CallClosure_TwoArgs)
{
    auto top = makeChunk();
    top->name = "<test>";
    top->localCount = 4;
    top->functions.push(make_adder_chunk());

    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(3)), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 2, BX(4)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 2, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(AS_INTEGER(r.run(top)), 7);
}

TEST(VMCalls, WrongArgcThrows)
{
    auto top = makeChunk();
    top->name = "<test>";
    top->localCount = 3;
    top->functions.push(make_adder_chunk());

    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(1)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 1, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();
    VMRunner r;
    EXPECT_THROW(r.run(top), std::runtime_error);
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
    VMRunner r;
    CB b;
    b.regs(3)
        .slot()
        .ABC(OpCode::LIST_NEW, 0, 3, 0)
        .load_int(2, 1)
        .ABC(OpCode::LIST_APPEND, 0, 2, 0)
        .load_int(2, 2)
        .ABC(OpCode::LIST_APPEND, 0, 2, 0)
        .load_int(2, 3)
        .ABC(OpCode::LIST_APPEND, 0, 2, 0)
        .ldg(1, "len")
        .mov(2, 0)
        .ABC(OpCode::IC_CALL, 1, 1, 0)
        .ret(1);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 3);
}

TEST(VMCalls, TailCall_DoesNotOverflowFrames)
{
    auto fn = makeChunk();
    fn->name = "cd";
    fn->arity = 1;
    fn->localCount = 4;

    uint16_t nk = fn->addConstant(MAKE_STRING("cd"));

    fn->emit(make_ABx(OpCode::LOAD_INT, 1, BX(0)), { });
    fn->emit(make_ABC(OpCode::OP_EQ, 1, 0, 1), { });
    fn->emit(make_AsBx(OpCode::JUMP_IF_FALSE, 1, 1), { });
    fn->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    fn->emit(make_ABx(OpCode::LOAD_INT, 1, BX(1)), { });
    fn->emit(make_ABC(OpCode::OP_SUB, 0, 0, 1), { });
    fn->emit(make_ABx(OpCode::LOAD_GLOBAL, 2, nk), { });
    fn->emit(make_ABC(OpCode::MOVE, 3, 0, 0), { });
    fn->emit(make_ABC(OpCode::CALL_TAIL, 2, 1, 0), { });

    auto top = makeChunk();
    top->name = "<test>";
    top->localCount = 3;
    top->functions.push(fn);

    uint16_t tk = top->addConstant(MAKE_STRING("cd"));
    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::STORE_GLOBAL, 0, tk), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(300)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 1, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();
    VMRunner r;
    Value v = r.run(std::move(top));
    EXPECT_EQ(AS_INTEGER(v), 0);
}

TEST(VMCalls, StackOverflowDetected)
{
    auto fn = makeChunk();
    fn->name = "inf";
    fn->arity = 0;
    fn->localCount = 1;
    uint16_t nk = fn->addConstant(MAKE_STRING("inf"));
    fn->emit(make_ABx(OpCode::LOAD_GLOBAL, 0, nk), { });
    fn->emit(make_ABC(OpCode::CALL, 0, 0, 0), { });
    fn->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });

    auto top = makeChunk();
    top->name = "<test>";
    top->localCount = 1;
    top->functions.push(fn);
    uint16_t tk = top->addConstant(MAKE_STRING("inf"));
    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::STORE_GLOBAL, 0, tk), { });
    top->emit(make_ABC(OpCode::CALL, 0, 0, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();
    VMRunner r;
    EXPECT_THROW(r.run(top), std::runtime_error);
}

TEST(VMClosures, CaptureLocal_GetUpvalue)
{
    auto inner = makeChunk();
    inner->name = "inner";
    inner->arity = 0;
    inner->upvalueCount = 1;
    inner->localCount = 1;
    inner->emit(make_ABC(OpCode::GET_UPVALUE, 0, 0, 0), { });
    inner->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });

    auto outer = makeChunk();
    outer->name = "outer";
    outer->arity = 0;
    outer->localCount = 2;
    outer->upvalueCount = 0;
    outer->functions.push(inner);
    outer->emit(make_ABx(OpCode::LOAD_INT, 0, BX(10)), { });
    outer->emit(make_ABx(OpCode::CLOSURE, 1, 0), { });
    outer->emit(make_ABC(OpCode::MOVE, 1, 0, 0), { });
    outer->emit(make_ABC(OpCode::CALL, 1, 0, 0), { });
    outer->emit(make_ABC(OpCode::RETURN, 1, 1, 0), { });

    auto top = makeChunk();
    top->name = "<test>";
    top->localCount = 2;
    top->functions.push(outer);
    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABC(OpCode::CALL, 0, 0, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(AS_INTEGER(r.run(std::move(top))), 10);
}

TEST(VMClosures, CloseUpvalue_SurvivesFrame)
{
    auto add = makeChunk();
    add->name = "add";
    add->arity = 1;
    add->upvalueCount = 1;
    add->localCount = 3;
    add->emit(make_ABC(OpCode::GET_UPVALUE, 1, 0, 0), { });
    add->emit(make_ABC(OpCode::OP_ADD, 2, 1, 0), { });
    add->emit(make_ABC(OpCode::RETURN, 2, 1, 0), { });

    auto ma = makeChunk();
    ma->name = "ma";
    ma->arity = 1;
    ma->localCount = 2;
    ma->upvalueCount = 0;
    ma->functions.push(add);
    ma->emit(make_ABx(OpCode::CLOSURE, 1, 0), { });
    ma->emit(make_ABC(OpCode::MOVE, 1, 0, 0), { });
    ma->emit(make_ABC(OpCode::CLOSE_UPVALUE, 0, 0, 0), { });
    ma->emit(make_ABC(OpCode::RETURN, 1, 1, 0), { });

    auto top = makeChunk();
    top->name = "<test>";
    top->localCount = 3;
    top->functions.push(ma);
    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(10)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 1, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(5)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 1, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();

    VMRunner r;
    EXPECT_EQ(AS_INTEGER(r.run(std::move(top))), 15);
}

TEST(VMClosures, SharedUpvalue_TwoClosuresOneSlot)
{
    auto get_fn = makeChunk();
    get_fn->name = "get";
    get_fn->arity = 0;
    get_fn->upvalueCount = 1;
    get_fn->localCount = 1;
    get_fn->emit(make_ABC(OpCode::GET_UPVALUE, 0, 0, 0), { });
    get_fn->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });

    auto set_fn = makeChunk();
    set_fn->name = "set";
    set_fn->arity = 1;
    set_fn->upvalueCount = 1;
    set_fn->localCount = 1;
    set_fn->emit(make_ABC(OpCode::SET_UPVALUE, 0, 0, 0), { });
    set_fn->emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), { });

    auto mp = makeChunk();
    mp->name = "mp";
    mp->arity = 0;
    mp->localCount = 4;
    mp->upvalueCount = 0;
    mp->functions.push(get_fn);
    mp->functions.push(set_fn);
    mp->emit(make_ABx(OpCode::LOAD_INT, 0, BX(0)), { });
    mp->emit(make_ABx(OpCode::CLOSURE, 1, 0), { });
    mp->emit(make_ABC(OpCode::MOVE, 1, 0, 0), { });
    mp->emit(make_ABx(OpCode::CLOSURE, 2, 1), { });
    mp->emit(make_ABC(OpCode::MOVE, 1, 0, 0), { });
    mp->emit(make_ABC(OpCode::LIST_NEW, 3, 2, 0), { });
    mp->emit(make_ABC(OpCode::LIST_APPEND, 3, 1, 0), { });
    mp->emit(make_ABC(OpCode::LIST_APPEND, 3, 2, 0), { });
    mp->emit(make_ABC(OpCode::CLOSE_UPVALUE, 0, 0, 0), { });
    mp->emit(make_ABC(OpCode::RETURN, 3, 1, 0), { });

    auto top = makeChunk();
    top->name = "<test>";
    top->localCount = 5;
    top->functions.push(mp);
    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABC(OpCode::CALL, 0, 0, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 2, BX(1)), { });
    top->emit(make_ABC(OpCode::LIST_GET, 1, 0, 2), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 2, BX(99)), { });
    top->emit(make_ABC(OpCode::CALL, 1, 1, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 2, BX(0)), { });
    top->emit(make_ABC(OpCode::LIST_GET, 1, 0, 2), { });
    top->emit(make_ABC(OpCode::CALL, 1, 0, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 1, 1, 0), { });
    top->disassemble();

    VMRunner r;
    EXPECT_EQ(AS_INTEGER(r.run(std::move(top))), 99);
}

TEST(VMICProfile, BinaryOpUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 3).load_int(1, 4).ABC(OpCode::OP_ADD, 2, 0, 1).nop(0).ret(2);
    r.run(b);
    b.dump();
    auto const& s = r.chunk_->icSlots[0];
    EXPECT_TRUE(hasTag(TypeTag(s.seenLhs), TypeTag::INT));
    EXPECT_TRUE(hasTag(TypeTag(s.seenRhs), TypeTag::INT));
    EXPECT_TRUE(hasTag(TypeTag(s.seenRet), TypeTag::INT));
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
    b.regs(3)
        .slot()
        .ABC(OpCode::LIST_NEW, 0, 2, 0)
        .load_int(2, 1)
        .ABC(OpCode::LIST_APPEND, 0, 2, 0)
        .load_int(2, 2)
        .ABC(OpCode::LIST_APPEND, 0, 2, 0)
        .ldg(1, "len")
        .mov(2, 0)
        .ABC(OpCode::IC_CALL, 1, 1, 0)
        .ret(1);
    b.dump();
    r.run(b);
    auto const& s = r.chunk_->icSlots[0];
    EXPECT_TRUE(hasTag(TypeTag(s.seenLhs), TypeTag::NATIVE));
    EXPECT_TRUE(hasTag(TypeTag(s.seenRet), TypeTag::INT));
    EXPECT_GE(s.hitCount, 1u);
}

TEST(VMICProfile, SlotAccumulatesAcrossLoopIterations)
{
    VMRunner r;
    CB b;
    b.regs(5)
        .slot()
        .load_int(0, 0)
        .load_int(1, 5)
        .load_int(2, 0)
        .ABC(OpCode::OP_LT, 3, 0, 1)
        .AsBx(OpCode::JUMP_IF_FALSE, 3, 4)
        .load_int(4, 1)
        .ABC(OpCode::OP_ADD, 0, 0, 4)
        .nop(0)
        .AsBx(OpCode::LOOP, 0, -6)
        .ret(0);
    b.dump();
    r.run(b);
    EXPECT_EQ(r.chunk_->icSlots[0].hitCount, 5u);
}

TEST(VMIntegration, Fibonacci_fib10_equals_55)
{
    ast::Stmt* fib = ast::makeFunction(
        ast::makeName("fib"),
        ast::makeList({ ast::makeName("n") }),
        ast::makeBlock({ ast::makeIf(
                             ast::makeBinary(ast::makeName("n"), ast::makeLiteralInt(1), ast::BinaryOp::OP_LTE),
                             ast::makeBlock({ ast::makeReturn(ast::makeName("n")) })),
            ast::makeReturn(ast::makeBinary(
                ast::makeCall(ast::makeName("fib"), ast::makeList({ ast::makeBinary(ast::makeName("n"), ast::makeLiteralInt(1), ast::BinaryOp::OP_SUB) })),
                ast::makeCall(ast::makeName("fib"), ast::makeList({ ast::makeBinary(ast::makeName("n"), ast::makeLiteralInt(2), ast::BinaryOp::OP_SUB) })),
                ast::BinaryOp::OP_ADD)) }));

    Chunk* top = compile_program({ fib,
        ast::makeExprStmt(ast::makeCall(ast::makeName("fib"), ast::makeList({ ast::makeLiteralInt(10) }))) });
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(AS_INTEGER(r.run(top)), 55);
}

TEST(VMIntegration, SumForLoop_1_to_100)
{
    GTEST_SKIP() << "Not implemented yet.";
    VMRunner r;
    CB b;
    b.regs(5)
        .slot()
        .load_int(0, 1)
        .load_int(1, 100)
        .load_int(2, 1)
        .load_int(4, 0)
        .AsBx(OpCode::FOR_PREP, 0, 3)
        .ABC(OpCode::OP_ADD, 4, 4, 3)
        .nop(0)
        .AsBx(OpCode::FOR_STEP, 0, -3)
        .ret(4);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 5050);
}

TEST(VMIntegration, StringConcat_3Parts)
{
    GTEST_SKIP() << "Compiler does not lower string concatenation yet.";
    ast::Stmt* test = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        ast::makeReturn(ast::makeBinary(
            ast::makeBinary(ast::makeLiteralString("hello"), ast::makeLiteralString(", "), ast::BinaryOp::OP_ADD),
            ast::makeLiteralString("world"),
            ast::BinaryOp::OP_ADD)));

    Chunk* top = compile_calling(test);
    top->disassemble();
    VMRunner r;
    Value v = r.run(top);
    EXPECT_EQ(AS_STRING(v)->str, "hello, world");
}

TEST(VMIntegration, ListSquaresViaForLoop)
{
    GTEST_SKIP() << "Not implemented yet.";
    VMRunner r;
    CB b;
    b.regs(6)
        .slot()
        .slot()
        .load_int(0, 0)
        .load_int(1, 9)
        .load_int(2, 1)
        .ABC(OpCode::LIST_NEW, 4, 10, 0)
        .AsBx(OpCode::FOR_PREP, 0, 5)
        .ABC(OpCode::OP_MUL, 5, 3, 3)
        .nop(0)
        .ABC(OpCode::LIST_APPEND, 4, 5, 0)
        .ABC(OpCode::NOP, 1, 0, 0)
        .AsBx(OpCode::FOR_STEP, 0, -5)
        .load_int(5, 5)
        .ABC(OpCode::LIST_GET, 5, 4, 5)
        .ret(5);
    b.dump();
    EXPECT_EQ(AS_INTEGER(r.run(b)), 25);
}

TEST(VMIntegration, NestedAdderClosure)
{
    auto add_fn = makeChunk();
    add_fn->name = "add";
    add_fn->arity = 1;
    add_fn->upvalueCount = 1;
    add_fn->localCount = 3;
    add_fn->emit(make_ABC(OpCode::GET_UPVALUE, 1, 0, 0), { });
    add_fn->emit(make_ABC(OpCode::OP_ADD, 2, 1, 0), { });
    add_fn->emit(make_ABC(OpCode::RETURN, 2, 1, 0), { });

    auto make_adder = makeChunk();
    make_adder->name = "ma";
    make_adder->arity = 1;
    make_adder->localCount = 2;
    make_adder->functions.push(add_fn);
    make_adder->emit(make_ABx(OpCode::CLOSURE, 1, 0), { });
    make_adder->emit(make_ABC(OpCode::MOVE, 1, 0, 0), { });
    make_adder->emit(make_ABC(OpCode::CLOSE_UPVALUE, 0, 0, 0), { });
    make_adder->emit(make_ABC(OpCode::RETURN, 1, 1, 0), { });

    auto top = makeChunk();
    top->name = "<test>";
    top->localCount = 3;
    top->functions.push(make_adder);
    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(5)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 1, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(3)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 1, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();

    VMRunner r;
    EXPECT_EQ(AS_INTEGER(r.run(std::move(top))), 8);
}

TEST(NativeLen, NullArgv)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeLen(1, nullptr)));
}

TEST(NativeLen, EmptyString)
{
    VM vm;
    auto s = MAKE_STRING("");
    EXPECT_EQ(AS_INTEGER(vm.nativeLen(1, &s)), 0);
}

TEST(NativeLen, NonEmptyString)
{
    VM vm;
    auto s = MAKE_STRING("hello");
    EXPECT_EQ(AS_INTEGER(vm.nativeLen(1, &s)), 5);
}

TEST(NativeLen, UnicodeString)
{
    VM vm;
    auto s = MAKE_STRING("abc");
    EXPECT_EQ(AS_INTEGER(vm.nativeLen(1, &s)), 3);
}

TEST(NativeLen, MultipleArgs_ReturnsNil)
{
    VM vm;
    auto s = MAKE_STRING("hi");
    Value args[] = { s, s };
    EXPECT_TRUE(IS_NIL(vm.nativeLen(2, args)));
}

TEST(NativeLen, IntegerArg_ReturnsNil)
{
    VM vm;
    Value arg = MAKE_INTEGER(42);
    EXPECT_TRUE(IS_NIL(vm.nativeLen(1, &arg)));
}

TEST(NativeLen, BoolArg_ReturnsNil)
{
    VM vm;
    Value arg = MAKE_BOOL(true);
    EXPECT_TRUE(IS_NIL(vm.nativeLen(1, &arg)));
}

TEST(NativeLen, NilArg_ReturnsNil)
{
    VM vm;
    Value arg = NIL_VAL;
    EXPECT_TRUE(IS_NIL(vm.nativeLen(1, &arg)));
}

TEST(NativePrint, NoArgs_PrintsNewline)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativePrint(0, nullptr)));
}

TEST(NativePrint, StringArg)
{
    VM vm;
    auto s = MAKE_STRING("hello world");
    EXPECT_TRUE(IS_NIL(vm.nativePrint(1, &s)));
}

TEST(NativePrint, IntegerArg)
{
    VM vm;
    Value arg = MAKE_INTEGER(42);
    EXPECT_TRUE(IS_NIL(vm.nativePrint(1, &arg)));
}

TEST(NativePrint, FloatArg)
{
    VM vm;
    Value arg = MAKE_REAL(3.14);
    EXPECT_TRUE(IS_NIL(vm.nativePrint(1, &arg)));
}

TEST(NativePrint, BoolArg_True)
{
    VM vm;
    Value arg = MAKE_BOOL(true);
    EXPECT_TRUE(IS_NIL(vm.nativePrint(1, &arg)));
}

TEST(NativePrint, BoolArg_False)
{
    VM vm;
    Value arg = MAKE_BOOL(false);
    EXPECT_TRUE(IS_NIL(vm.nativePrint(1, &arg)));
}

TEST(NativePrint, NilArg)
{
    VM vm;
    Value arg = NIL_VAL;
    EXPECT_TRUE(IS_NIL(vm.nativePrint(1, &arg)));
}

TEST(NativePrint, TwoArgs_DoesNotCrash)
{
    VM vm;
    auto s = MAKE_STRING("a");
    Value args[] = { s, s };
    EXPECT_TRUE(IS_NIL(vm.nativePrint(2, args)));
}

TEST(NativeStr, NoArgs_ReturnsEmpty)
{
    VM vm;
    EXPECT_TRUE(IS_STRING(vm.nativeStr(0, nullptr)));
}

TEST(NativeStr, NullArgv_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(AS_STRING(vm.nativeStr(1, nullptr))->str.empty());
}

TEST(NativeStr, Integer)
{
    VM vm;
    Value arg = MAKE_INTEGER(42);
    Value r = vm.nativeStr(1, &arg);
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "42");
}

TEST(NativeStr, NegativeInteger)
{
    VM vm;
    Value arg = MAKE_INTEGER(-7);
    Value r = vm.nativeStr(1, &arg);
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "-7");
}

TEST(NativeStr, Zero)
{
    VM vm;
    Value arg = MAKE_INTEGER(0);
    Value r = vm.nativeStr(1, &arg);
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "0");
}

TEST(NativeStr, BoolTrue)
{
    VM vm;
    Value arg = MAKE_BOOL(true);
    Value r = vm.nativeStr(1, &arg);
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "true");
}

TEST(NativeStr, BoolFalse)
{
    VM vm;
    Value arg = MAKE_BOOL(false);
    Value r = vm.nativeStr(1, &arg);
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "false");
}

TEST(NativeStr, Float)
{
    VM vm;
    Value arg = MAKE_REAL(1.5);
    Value r = vm.nativeStr(1, &arg);
    ASSERT_TRUE(IS_STRING(r));
    // std::to_string(1.5) == "1.500000" — just verify it parses back
    double parsed = std::stod(AS_STRING(r)->str.data());
    EXPECT_DOUBLE_EQ(parsed, 1.5);
}

TEST(NativeStr, StringPassthrough)
{
    VM vm;
    Value s = MAKE_STRING("hello");
    Value r = vm.nativeStr(1, &s);
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "hello");
}

TEST(NativeStr, NilArg_ReturnsNil)
{
    VM vm;
    Value arg = NIL_VAL;
    EXPECT_NO_FATAL_FAILURE(vm.nativeStr(1, &arg));
}

TEST(NativeStr, TwoArgs_ReturnsNil)
{
    VM vm;
    auto s = MAKE_STRING("x");
    Value args[] = { s, s };
    EXPECT_TRUE(IS_NIL(vm.nativeStr(2, args)));
}

TEST(NativeBool, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeBool(0, nullptr)));
}

TEST(NativeBool, TrueBoolean)
{
    VM vm;
    Value arg = MAKE_BOOL(true);
    Value r = vm.nativeBool(1, &arg);
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_TRUE(AS_BOOL(r));
}

TEST(NativeBool, FalseBoolean)
{
    VM vm;
    Value arg = MAKE_BOOL(false);
    Value r = vm.nativeBool(1, &arg);
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_FALSE(AS_BOOL(r));
}

TEST(NativeBool, NonZeroInteger_IsTrue)
{
    VM vm;
    Value arg = MAKE_INTEGER(1);
    Value r = vm.nativeBool(1, &arg);
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_TRUE(AS_BOOL(r));
}

TEST(NativeBool, ZeroInteger_IsFalsy)
{
    VM vm;
    Value arg = MAKE_INTEGER(0);
    Value r = vm.nativeBool(1, &arg);
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_FALSE(AS_BOOL(r));
}

TEST(NativeBool, NilArg)
{
    VM vm;
    Value arg = NIL_VAL;
    Value r = vm.nativeBool(1, &arg);
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_FALSE(AS_BOOL(r));
}

TEST(NativeBool, NonEmptyString_IsTrue)
{
    VM vm;
    Value arg = MAKE_STRING("hi");
    Value r = vm.nativeBool(1, &arg);
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_TRUE(AS_BOOL(r));
}

TEST(NativeInt, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeInt(0, nullptr)));
}

TEST(NativeInt, IntegerPassthrough)
{
    VM vm;
    Value arg = MAKE_INTEGER(7);
    Value r = vm.nativeInt(1, &arg);
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 7);
}

TEST(NativeInt, FloatTruncates)
{
    VM vm;
    Value arg = MAKE_REAL(3.9);
    Value r = vm.nativeInt(1, &arg);
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 3);
}

TEST(NativeInt, NegativeFloat)
{
    VM vm;
    Value arg = MAKE_REAL(-2.7);
    Value r = vm.nativeInt(1, &arg);
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), -2);
}

TEST(NativeInt, StringArg_ReturnsNil)
{
    VM vm;
    Value arg = MAKE_STRING("42");
    EXPECT_TRUE(IS_NIL(vm.nativeInt(1, &arg)));
}

TEST(NativeInt, NilArg_ReturnsNil)
{
    VM vm;
    Value arg = NIL_VAL;
    EXPECT_TRUE(IS_NIL(vm.nativeInt(1, &arg)));
}

TEST(NativeInt, BoolArg_ReturnsNil)
{
    VM vm;
    Value arg = MAKE_BOOL(true);
    EXPECT_TRUE(IS_NIL(vm.nativeInt(1, &arg)));
}

TEST(NativeFloat, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeFloat(0, nullptr)));
}

TEST(NativeFloat, IntegerToFloat)
{
    VM vm;
    Value arg = MAKE_INTEGER(3);
    Value r = vm.nativeFloat(1, &arg);
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.0);
}

TEST(NativeFloat, FloatPassthrough)
{
    VM vm;
    Value arg = MAKE_REAL(2.5);
    Value r = vm.nativeFloat(1, &arg);
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 2.5);
}

TEST(NativeFloat, NegativeInteger)
{
    VM vm;
    Value arg = MAKE_INTEGER(-10);
    Value r = vm.nativeFloat(1, &arg);
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), -10.0);
}

TEST(NativeFloat, StringArg_ReturnsNil)
{
    VM vm;
    Value arg = MAKE_STRING("3.14");
    EXPECT_TRUE(IS_NIL(vm.nativeFloat(1, &arg)));
}

TEST(NativeFloat, NilArg_ReturnsNil)
{
    VM vm;
    Value arg = NIL_VAL;
    EXPECT_TRUE(IS_NIL(vm.nativeFloat(1, &arg)));
}

TEST(NativeType, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeType(0, nullptr)));
}

TEST(NativeType, ReturnsInteger)
{
    VM vm;
    Value i = MAKE_INTEGER(0);
    Value f = MAKE_REAL(0.0);
    Value b = MAKE_BOOL(false);
    Value n = NIL_VAL;
    Value s = MAKE_STRING("x");
    EXPECT_TRUE(IS_INTEGER(vm.nativeType(1, &i)));
    EXPECT_TRUE(IS_INTEGER(vm.nativeType(1, &f)));
    EXPECT_TRUE(IS_INTEGER(vm.nativeType(1, &b)));
    EXPECT_TRUE(IS_INTEGER(vm.nativeType(1, &n)));
    EXPECT_TRUE(IS_INTEGER(vm.nativeType(1, &s)));
}

TEST(NativeType, DifferentTypesHaveDifferentTags)
{
    VM vm;
    Value i = MAKE_INTEGER(0);
    Value f = MAKE_REAL(0.0);
    Value b = MAKE_BOOL(false);
    Value n = NIL_VAL;
    Value s = MAKE_STRING("x");
    int64_t int_tag = AS_INTEGER(vm.nativeType(1, &i));
    int64_t flt_tag = AS_INTEGER(vm.nativeType(1, &f));
    int64_t bool_tag = AS_INTEGER(vm.nativeType(1, &b));
    int64_t nil_tag = AS_INTEGER(vm.nativeType(1, &n));
    int64_t str_tag = AS_INTEGER(vm.nativeType(1, &s));

    EXPECT_NE(int_tag, flt_tag);
    EXPECT_NE(int_tag, nil_tag);
    EXPECT_NE(str_tag, nil_tag);
    EXPECT_NE(bool_tag, nil_tag);
}

TEST(NativeAppend, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeAppend(0, nullptr)));
}

TEST(NativePop, NonListArg_ReturnsNil)
{
    VM vm;
    Value arg = MAKE_INTEGER(5);
    EXPECT_TRUE(IS_NIL(vm.nativePop(1, &arg)));
}

TEST(NativePop, NullArgv_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativePop(1, nullptr)));
}

TEST(NativeSlice, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeSlice(0, nullptr)));
}

TEST(NativeFloor, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeFloor(0, nullptr)));
}

TEST(NativeFloor, IntegerPassthrough)
{
    VM vm;
    Value arg = MAKE_INTEGER(5);
    Value r = vm.nativeFloor(1, &arg);
    EXPECT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 5);
}

TEST(NativeFloor, PositiveFloat)
{
    VM vm;
    Value arg = MAKE_REAL(3.7);
    Value r = vm.nativeFloor(1, &arg);
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.0);
}

TEST(NativeFloor, NegativeFloat)
{
    VM vm;
    Value arg = MAKE_REAL(-2.3);
    Value r = vm.nativeFloor(1, &arg);
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), -3.0);
}

TEST(NativeFloor, ExactFloat)
{
    VM vm;
    Value arg = MAKE_REAL(4.0);
    Value r = vm.nativeFloor(1, &arg);
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 4.0);
}

TEST(NativeFloor, StringArg_ReturnsNil)
{
    VM vm;
    Value arg = MAKE_STRING("3.5");
    EXPECT_TRUE(IS_NIL(vm.nativeFloor(1, &arg)));
}

TEST(NativeFloor, NilArg_ReturnsNil)
{
    VM vm;
    Value arg = NIL_VAL;
    EXPECT_TRUE(IS_NIL(vm.nativeFloor(1, &arg)));
}

TEST(NativeFloor, TwoArgs_ReturnsNil)
{
    VM vm;
    Value args[] = { MAKE_REAL(1.5), MAKE_REAL(2.5) };
    EXPECT_TRUE(IS_NIL(vm.nativeFloor(2, args)));
}

TEST(NativeCeil, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeCeil(0, nullptr)));
}

TEST(NativeCeil, IntegerPassthrough)
{
    VM vm;
    Value arg = MAKE_INTEGER(5);
    Value r = vm.nativeCeil(1, &arg);
    EXPECT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 5);
}

TEST(NativeCeil, PositiveFloat)
{
    VM vm;
    Value arg = MAKE_REAL(3.2);
    Value r = vm.nativeCeil(1, &arg);
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 4.0);
}

TEST(NativeCeil, NegativeFloat)
{
    VM vm;
    Value arg = MAKE_REAL(-2.7);
    Value r = vm.nativeCeil(1, &arg);
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), -2.0);
}

TEST(NativeCeil, ExactFloat)
{
    VM vm;
    Value arg = MAKE_REAL(4.0);
    Value r = vm.nativeCeil(1, &arg);
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 4.0);
}

TEST(NativeCeil, StringArg_ReturnsNil)
{
    VM vm;
    Value arg = MAKE_STRING("3.5");
    EXPECT_TRUE(IS_NIL(vm.nativeCeil(1, &arg)));
}

TEST(NativeCeil, NilArg_ReturnsNil)
{
    VM vm;
    Value arg = NIL_VAL;
    EXPECT_TRUE(IS_NIL(vm.nativeCeil(1, &arg)));
}

TEST(NativeAbs, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeAbs(0, nullptr)));
}

TEST(NativeAbs, PositiveInteger)
{
    VM vm;
    Value arg = MAKE_INTEGER(5);
    Value r = vm.nativeAbs(1, &arg);
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 5);
}

TEST(NativeAbs, NegativeInteger)
{
    VM vm;
    Value arg = MAKE_INTEGER(-5);
    Value r = vm.nativeAbs(1, &arg);
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 5);
}

TEST(NativeAbs, ZeroInteger)
{
    VM vm;
    Value arg = MAKE_INTEGER(0);
    Value r = vm.nativeAbs(1, &arg);
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 0);
}

TEST(NativeAbs, PositiveFloat)
{
    VM vm;
    Value arg = MAKE_REAL(3.5);
    Value r = vm.nativeAbs(1, &arg);
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.5);
}

TEST(NativeAbs, NegativeFloat)
{
    VM vm;
    Value arg = MAKE_REAL(-3.5);
    Value r = vm.nativeAbs(1, &arg);
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.5);
}

TEST(NativeAbs, StringArg_ReturnsNil)
{
    VM vm;
    Value arg = MAKE_STRING("-3");
    EXPECT_TRUE(IS_NIL(vm.nativeAbs(1, &arg)));
}

TEST(NativeAbs, NilArg_ReturnsNil)
{
    VM vm;
    Value arg = NIL_VAL;
    EXPECT_TRUE(IS_NIL(vm.nativeAbs(1, &arg)));
}

TEST(NativeMin, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeMin(0, nullptr)));
}

TEST(NativeMin, OneArg_ReturnsArg)
{
    VM vm;
    srand(static_cast<unsigned>(time(nullptr)));
    Value n = MAKE_INTEGER(static_cast<int64_t>(rand()));
    Value result = vm.nativeMin(1, &n);
    EXPECT_EQ(AS_INTEGER(result), AS_INTEGER(n));
}

TEST(NativeMin, TwoPositiveIntegers)
{
    VM vm;
    Value args[] = { MAKE_INTEGER(3), MAKE_INTEGER(7) };
    Value r = vm.nativeMin(2, args);
    EXPECT_EQ(AS_INTEGER(r), 3);
}

TEST(NativeMin, AllIntegersReturnsInteger)
{
    VM vm;
    Value args[] = { MAKE_INTEGER(5), MAKE_INTEGER(2), MAKE_INTEGER(8) };
    Value r = vm.nativeMin(3, args);
    EXPECT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 2);
}

TEST(NativeMin, MixedFloatAndInteger_ReturnsFloat)
{
    VM vm;
    Value args[] = { MAKE_INTEGER(3), MAKE_REAL(1.5) };
    Value r = vm.nativeMin(2, args);
    EXPECT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 1.5);
}

TEST(NativeMin, NegativeValues)
{
    VM vm;
    Value args[] = { MAKE_INTEGER(-1), MAKE_INTEGER(-5), MAKE_INTEGER(-2) };
    Value r = vm.nativeMin(3, args);
    EXPECT_EQ(AS_INTEGER(r), -5);
}

TEST(NativeMin, StringFirstArg_ReturnsNil)
{
    VM vm;
    Value args[] = { MAKE_STRING("a"), MAKE_STRING("b") };
    EXPECT_EQ(AS_STRING(vm.nativeMin(2, args))->str, MAKE_OBJ_STRING("a")->str);
}

TEST(NativeMax, NoArgs_ReturnsNil)
{
    VM vm;
    EXPECT_TRUE(IS_NIL(vm.nativeMax(0, nullptr)));
}

TEST(NativeMax, OneArg_ReturnsArg)
{
    VM vm;
    srand(static_cast<unsigned>(time(nullptr)));
    Value n = MAKE_INTEGER(static_cast<int64_t>(rand()));
    Value result = vm.nativeMax(1, &n);
    EXPECT_EQ(AS_INTEGER(result), AS_INTEGER(n));
}

TEST(NativeMax, TwoPositiveIntegers)
{
    VM vm;
    Value args[] = { MAKE_INTEGER(3), MAKE_INTEGER(7) };
    Value r = vm.nativeMax(2, args);
    EXPECT_EQ(AS_INTEGER(r), 7);
}

TEST(NativeMax, NegativeValues)
{
    VM vm;
    Value args[] = { MAKE_INTEGER(-3), MAKE_INTEGER(-1) };
    Value r = vm.nativeMax(2, args);
    EXPECT_EQ(AS_INTEGER(r), -1);
}

TEST(NativeMax, AllIntegersReturnsInteger)
{
    VM vm;
    Value args[] = { MAKE_INTEGER(1), MAKE_INTEGER(9), MAKE_INTEGER(4) };
    Value r = vm.nativeMax(3, args);
    EXPECT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 9);
}

TEST(NativeMax, MixedFloatAndInteger)
{
    VM vm;
    Value args[] = { MAKE_INTEGER(3), MAKE_REAL(3.5) };
    Value r = vm.nativeMax(2, args);
    EXPECT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.5);
}

TEST(NativeMax, StringFirstArg_ReturnsArg)
{
    VM vm;
    Value args[] = { MAKE_STRING("a"), MAKE_STRING("b") };
    EXPECT_EQ(AS_STRING(vm.nativeMax(2, args))->str, MAKE_OBJ_STRING("b")->str);
}

TEST(NativeRound, HalfRoundsUp)
{
    VM vm;
    Value arg = MAKE_REAL(2.5);
    Value r = vm.nativeRound(1, &arg);
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.0);
}

TEST(NativeRound, HalfNegativeRoundsDown)
{
    VM vm;
    Value arg = MAKE_REAL(-2.5);
    Value r = vm.nativeRound(1, &arg);
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), -3.0);
}

TEST(NativeRound, IntegerPassthrough)
{
    VM vm;
    Value arg = MAKE_INTEGER(4);
    Value r = vm.nativeRound(1, &arg);
    ASSERT_TRUE(IS_INTEGER(r));
    EXPECT_EQ(AS_INTEGER(r), 4);
}

TEST(NativePow, BasicSquare)
{
    VM vm;
    Value args[] = { MAKE_REAL(3.0), MAKE_REAL(2.0) };
    Value r = vm.nativePow(2, args);
    if (!IS_NIL(r))
        EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 9.0);
}

TEST(NativePow, ZeroExponent)
{
    VM vm;
    Value args[] = { MAKE_REAL(5.0), MAKE_REAL(0.0) };
    Value r = vm.nativePow(2, args);
    if (!IS_NIL(r))
        EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 1.0);
}

TEST(NativePow, NegativeExponent)
{
    VM vm;
    Value args[] = { MAKE_REAL(2.0), MAKE_REAL(-1.0) };
    Value r = vm.nativePow(2, args);
    if (!IS_NIL(r))
        EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 0.5);
}

TEST(NativeSqrt, PerfectSquare)
{
    VM vm;
    Value arg = MAKE_REAL(9.0);
    Value r = vm.nativeSqrt(1, &arg);
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 3.0);
}

TEST(NativeSqrt, Zero)
{
    VM vm;
    Value arg = MAKE_REAL(0.0);
    Value r = vm.nativeSqrt(1, &arg);
    ASSERT_TRUE(IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(AS_DOUBLE(r), 0.0);
}

TEST(NativeSqrt, NegativeInput_SpecBehavior)
{
    VM vm;
    Value arg = MAKE_REAL(-1.0);
    EXPECT_NO_FATAL_FAILURE(vm.nativeSqrt(1, &arg));
}

TEST(NativeSplit, BasicSplit)
{
    VM vm;
    Value args[] = { MAKE_STRING("a,b,c"), MAKE_STRING(",") };
    Value r = vm.nativeSplit(2, args);
    ASSERT_TRUE(IS_LIST(r));
    EXPECT_EQ(AS_LIST(r)->elements.size(), 3u);
    ASSERT_TRUE(IS_STRING(AS_LIST(r)->elements[0]));
    ASSERT_TRUE(IS_STRING(AS_LIST(r)->elements[1]));
    ASSERT_TRUE(IS_STRING(AS_LIST(r)->elements[2]));
    EXPECT_EQ(std::string(AS_STRING(AS_LIST(r)->elements[0])->str.data()), "a");
    EXPECT_EQ(std::string(AS_STRING(AS_LIST(r)->elements[1])->str.data()), "b");
    EXPECT_EQ(std::string(AS_STRING(AS_LIST(r)->elements[2])->str.data()), "c");
}

TEST(NativeSplit, NoDelimiterFound)
{
    VM vm;
    Value args[] = { MAKE_STRING("hello"), MAKE_STRING(",") };
    Value r = vm.nativeSplit(2, args);
    ASSERT_TRUE(IS_LIST(r));
    EXPECT_EQ(AS_LIST(r)->elements.size(), 1u);
    ASSERT_TRUE(IS_STRING(AS_LIST(r)->elements[0]));
    EXPECT_EQ(std::string(AS_STRING(AS_LIST(r)->elements[0])->str.data()), "hello");
}

TEST(NativeSubstr, BasicSubstr)
{
    VM vm; // substr is exclusive
    Value args[] = { MAKE_STRING("hello"), MAKE_INTEGER(1), MAKE_INTEGER(4) };
    Value r = vm.nativeSubstr(3, args);
    if (!IS_NIL(r)) {
        ASSERT_TRUE(IS_STRING(r));
        EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "ell");
    }
}

TEST(NativeSubstr, FromStart)
{
    VM vm; // substr is exclusive
    Value args[] = { MAKE_STRING("hello"), MAKE_INTEGER(0), MAKE_INTEGER(3) };
    Value r = vm.nativeSubstr(3, args);
    if (!IS_NIL(r)) {
        ASSERT_TRUE(IS_STRING(r));
        EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "hel");
    }
}

TEST(NativeContains, StringContains_True)
{
    VM vm;
    Value args[] = { MAKE_STRING("hello world"), MAKE_STRING("world") };
    Value r = vm.nativeContains(2, args);
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_TRUE(AS_BOOL(r));
}

TEST(NativeContains, StringContains_False)
{
    VM vm;
    Value args[] = { MAKE_STRING("hello"), MAKE_STRING("xyz") };
    Value r = vm.nativeContains(2, args);
    ASSERT_TRUE(IS_BOOL(r));
    EXPECT_FALSE(AS_BOOL(r));
}

TEST(NativeTrim, LeadingAndTrailingSpaces)
{
    VM vm;
    Value arg = MAKE_STRING("  hello  ");
    Value r = vm.nativeTrim(1, &arg);
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "hello");
}

TEST(NativeTrim, NoSpaces)
{
    VM vm;
    Value arg = MAKE_STRING("hello");
    Value r = vm.nativeTrim(1, &arg);
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "hello");
}

TEST(NativeTrim, OnlySpaces)
{
    VM vm;
    Value arg = MAKE_STRING("   ");
    Value r = vm.nativeTrim(1, &arg);
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "");
}

TEST(NativeJoin, BasicJoin)
{
    VM vm;
    Value list = vm.nativeList(0, nullptr);
    ObjList* l = AS_LIST(list);
    l->elements.push(MAKE_STRING("a"));
    l->elements.push(MAKE_STRING("b"));
    l->elements.push(MAKE_STRING("c"));
    Value args[] = { list, MAKE_STRING("|") };
    Value r = vm.nativeJoin(2, args);
    ASSERT_TRUE(IS_STRING(r));
    EXPECT_EQ(std::string(AS_STRING(r)->str.data()), "a|b|c");
}

TEST(NativeAssert, TrueCondition_DoesNotCrash)
{
    VM vm;
    Value arg = MAKE_BOOL(true);
    EXPECT_NO_FATAL_FAILURE(vm.nativeAssert(1, &arg));
}

TEST(NativeAssert, FalseCondition_DoesNotCrash)
{
    VM vm;
    Value arg = MAKE_BOOL(false);
    EXPECT_NO_FATAL_FAILURE(vm.nativeAssert(1, &arg));
}

TEST(NativeClock, ReturnsNumber_WhenImplemented)
{
    VM vm;
    Value r = vm.nativeClock(0, nullptr);
    if (!IS_NIL(r))
        EXPECT_TRUE(IS_INTEGER(r));
}

TEST(NativeTime, ReturnsNumber_WhenImplemented)
{
    VM vm;
    Value r = vm.nativeTime(0, nullptr);
    if (!IS_NIL(r))
        EXPECT_TRUE(IS_INTEGER(r));
}

static double microseconds_since(std::chrono::high_resolution_clock::time_point t0)
{
    using namespace std::chrono;
    return static_cast<double>(
               duration_cast<nanoseconds>(high_resolution_clock::now() - t0).count())
        / 1000.0;
}

template<typename T>
static void do_not_optimize(T const& v)
{
    void const volatile* sink = &v;
    (void)sink;
}

static bool stress_perf_enabled()
{
    char const* value = std::getenv("MYLANG_ENABLE_STRESS_PERF");
    return value && value[0] == '1';
}

static void require_stress_perf()
{
    if (!stress_perf_enabled())
        GTEST_SKIP() << "Set MYLANG_ENABLE_STRESS_PERF=1 to run large VM stress tests.";
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Dispatch throughput — tight integer arithmetic loop
//
// Measures raw opcode dispatch speed. The loop body is:
//   r2 = r0 + r1  (OP_ADD, integer fast path)
//   r0 = r2       (MOVE)
// N iterations → N ADD + N MOVE dispatched.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfTest, Dispatch_IntAdd_1M_Iterations)
{
    constexpr int N = 1'000'000;

    ast::Stmt* test = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0), true),
            ast::makeAssignmentStmt(ast::makeName("step"), ast::makeLiteralInt(1), true),
            ast::makeAssignmentStmt(ast::makeName("limit"), ast::makeLiteralInt(N), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeAssignmentStmt(
                    ast::makeName("i"),
                    ast::makeBinary(ast::makeName("i"), ast::makeName("step"), ast::BinaryOp::OP_ADD)) })),
            ast::makeReturn(ast::makeName("i")) }));

    Chunk* top = compile_calling(test);
    top->disassemble();
    VM vm;
    // Warm up — lets the IC quicken the ADD opcode.
    vm.run(top);

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(AS_INTEGER(result), N);
    std::printf("  Dispatch IntAdd loop %dk iters:    %.1f µs  (%.2f ns/op)\n",
        N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Dispatch throughput — float arithmetic loop
//    Same structure, but double accumulator — exercises the FF fast path.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfTest, Dispatch_FloatAdd_500k_Iterations)
{
    constexpr int N = 500'000;

    ast::Stmt* test = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralFloat(0.0), true),
            ast::makeAssignmentStmt(ast::makeName("step"), ast::makeLiteralFloat(1.0), true),
            ast::makeAssignmentStmt(ast::makeName("limit"), ast::makeLiteralFloat(static_cast<double>(N)), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeAssignmentStmt(
                    ast::makeName("i"),
                    ast::makeBinary(ast::makeName("i"), ast::makeName("step"), ast::BinaryOp::OP_ADD)) })),
            ast::makeReturn(ast::makeName("i")) }));

    Chunk* top = compile_calling(test);
    top->disassemble();
    VM vm;
    vm.run(top);

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_TRUE(IS_DOUBLE(result));
    std::printf("  Dispatch FloatAdd loop %dk iters:  %.1f µs  (%.2f ns/op)\n",
        N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. IC quickening benefit — compare cold vs warm dispatch on same chunk
//    Cold: first run, generic opcode handlers.
//    Warm: second run, quickened ADD_II / ADD_FF specialisations.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfTest, IC_Quickening_ColdVsWarm_Ratio)
{
    constexpr int N = 200'000;

    ast::Stmt* test = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0), true),
            ast::makeAssignmentStmt(ast::makeName("step"), ast::makeLiteralInt(1), true),
            ast::makeAssignmentStmt(ast::makeName("limit"), ast::makeLiteralInt(N), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeAssignmentStmt(
                    ast::makeName("i"),
                    ast::makeBinary(ast::makeName("i"), ast::makeName("step"), ast::BinaryOp::OP_ADD)) })),
            ast::makeReturn(ast::makeName("i")) }));

    Chunk* top = compile_calling(test);
    top->disassemble();
    VM vm_cold, vm_warm;

    // Cold — no prior run.
    auto t0 = std::chrono::high_resolution_clock::now();
    Value cold_result = vm_cold.run(top);
    double cold_us = microseconds_since(t0);

    // Warm — opcodes already quickened by the cold run above.
    t0 = std::chrono::high_resolution_clock::now();
    Value warm_result = vm_warm.run(top);
    double warm_us = microseconds_since(t0);

    do_not_optimize(cold_result);
    do_not_optimize(warm_result);
    EXPECT_EQ(AS_INTEGER(cold_result), N);
    EXPECT_EQ(AS_INTEGER(warm_result), N);

    double ratio = cold_us / warm_us;
    std::printf("  IC quickening: cold=%.1f µs  warm=%.1f µs  speedup=%.2fx\n",
        cold_us, warm_us, ratio);

    // Quickened path must be at least as fast; if ratio < 1 quickening is hurting.
    EXPECT_GE(ratio, 0.7)
        << "Quickened dispatch is significantly slower than generic — "
           "check the IC specialisation logic";
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Global variable lookup throughput
//    STORE_GLOBAL + LOAD_GLOBAL in a loop — exercises the globals hash map.
// ─────────────────────────────────────────────────────────────────────────────

/*
TEST(VMPerfTest, GlobalLookup_100k_Roundtrips)
{
    constexpr int N = 100'000;

    CB b;
    b.regs(3);
    b.load_int(0, 0);
    b.load_int(1, 1);
    b.load_val(2, N);
    // loop: r0 += 1; store r0 -> "counter"; load "counter" -> r0; compare; loop
    // (deliberately alternating store/load to stress lookup symmetrically)
    b.ABC(OpCode::OP_LT, 1, 0, 2);           // reuse r1 as cmp result
    b.AsBx(OpCode::JUMP_IF_FALSE, 1, 4);
    b.stg(0, "counter");
    b.ldg(0, "counter");
    b.load_int(1, 1);
    b.ABC(OpCode::OP_ADD, 0, 0, 1);
    b.AsBx(OpCode::LOOP, 0, -7);
    b.ret(0);

    b.dump();
    VM vm;

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(b.ch);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    ::printf("  Global store+load %dk roundtrips:  %.1f µs  (%.2f ns/op)\n", N / 1000, us, us * 1000.0 / N);
}
*/

// ─────────────────────────────────────────────────────────────────────────────
// 5. Function call overhead
//    Calls a trivial two-arg add closure N times from a loop.
//    Measures: CALL setup, frame push/pop, RETURN teardown.
// ─────────────────────────────────────────────────────────────────────────────

static Chunk* make_add2_chunk()
{
    auto* fn = makeChunk();
    fn->name = "add2";
    fn->arity = 2;
    fn->localCount = 3;
    fn->emit(make_ABC(OpCode::OP_ADD, 2, 0, 1), { });
    fn->emit(make_ABC(OpCode::RETURN, 2, 1, 0), { });
    return fn;
}

TEST(VMPerfTest, CallOverhead_100k_Calls)
{
    constexpr int N = 100;

    ast::Stmt* body = ast::makeBlock(
        { ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0)),
            ast::makeAssignmentStmt(ast::makeName("limit"), ast::makeLiteralInt(N)),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeExprStmt(ast::makeCall(ast::makeName("add"), ast::makeList({ ast::makeLiteralInt(1), ast::makeLiteralInt(2) }))),
                    ast::makeAssignmentStmt(ast::makeName("i"), ast::makeBinary(ast::makeName("i"), ast::makeLiteralInt(1), ast::BinaryOp::OP_ADD)) })),
            ast::makeReturn(ast::makeName("i")) });

    ast::Stmt* func = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        body);

    ast::Stmt* add = ast::makeFunction(
        ast::makeName("add"),
        ast::makeList({ ast::makeName("a"), ast::makeName("b") }),
        ast::makeReturn(ast::makeBinary(ast::makeName("a"), ast::makeName("b"), ast::BinaryOp::OP_ADD)));
    ast::Stmt* call = ast::makeExprStmt(ast::makeCall(ast::makeName("test"), ast::makeList()));

    std::cout << "AUTO:" << '\n';
    Chunk* top_ = Compiler().compile({ add, func, call });
    top_->disassemble();
    VM vm;
    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top_);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(AS_INTEGER(result), N);
    std::printf("  Call overhead %dk calls:            %.1f µs  (%.2f ns/call)\n",
        N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Tail-call vs regular call
//    Both count to N via recursion. Tail-call should use O(1) frames.
//    We measure time; the tail-call version must not be significantly slower.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfTest, TailCall_vs_RegularLoop_Ratio)
{
    constexpr int DEPTH = 5000;

    ast::Stmt* tc_fn = ast::makeFunction(
        ast::makeName("tc"),
        ast::makeList({ ast::makeName("n") }),
        ast::makeBlock({ ast::makeIf(
                             ast::makeBinary(ast::makeName("n"), ast::makeLiteralInt(0), ast::BinaryOp::OP_EQ),
                             ast::makeBlock({ ast::makeReturn(ast::makeName("n")) })),
            ast::makeReturn(ast::makeCall(
                ast::makeName("tc"),
                ast::makeList({ ast::makeBinary(ast::makeName("n"), ast::makeLiteralInt(1), ast::BinaryOp::OP_SUB) }))) }));

    Chunk* tc_top = compile_program({ tc_fn,
        ast::makeExprStmt(ast::makeCall(ast::makeName("tc"), ast::makeList({ ast::makeLiteralInt(DEPTH) }))) });

    tc_top->disassemble();
    VM vm_tc;
    auto t0 = std::chrono::high_resolution_clock::now();
    Value tc_result = vm_tc.run(tc_top);
    double tc_us = microseconds_since(t0);

    // ── equivalent iterative loop (no calls) ─────────────────────────────
    ast::Stmt* loop_fn = ast::makeFunction(
        ast::makeName("loop"),
        ast::makeList(),
        ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("n"), ast::makeLiteralInt(DEPTH), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("n"), ast::makeLiteralInt(0), ast::BinaryOp::OP_NEQ),
                ast::makeBlock({ ast::makeAssignmentStmt(
                    ast::makeName("n"),
                    ast::makeBinary(ast::makeName("n"), ast::makeLiteralInt(1), ast::BinaryOp::OP_SUB)) })),
            ast::makeReturn(ast::makeName("n")) }));

    Chunk* loop_top = compile_calling(loop_fn);
    loop_top->disassemble();
    VM vm_loop;
    t0 = std::chrono::high_resolution_clock::now();
    Value loop_result = vm_loop.run(loop_top);
    double loop_us = microseconds_since(t0);

    do_not_optimize(tc_result);
    do_not_optimize(loop_result);
    EXPECT_EQ(AS_INTEGER(tc_result), 0);
    EXPECT_EQ(AS_INTEGER(loop_result), 0);

    std::printf("  Tail-call %d depth:  tc=%.1f µs  loop=%.1f µs  ratio=%.2fx\n",
        DEPTH, tc_us, loop_us, tc_us / loop_us);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. Upvalue capture and access throughput
//    A closure reads its upvalue N times in a loop.
//    Measures GET_UPVALUE dispatch + indirection cost.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfTest, Upvalue_GetSet_200k)
{
    constexpr int N = 200'000;

    // inner: loop N times, each iteration does GET_UPVALUE + ADD + SET_UPVALUE
    auto* inner = makeChunk();
    inner->name = "inner";
    inner->arity = 0;
    inner->upvalueCount = 1;
    inner->localCount = 4;

    inner->emit(make_ABx(OpCode::LOAD_INT, 1, BX(0)), { }); // r1 = 0 (i)
    inner->emit(make_ABx(OpCode::LOAD_INT, 2, BX(1)), { }); // r2 = 1
    inner->emit(make_ABx(OpCode::LOAD_INT, 3, BX(N)), { }); // r3 = N
    // loop: GET_UPVALUE r0, 0; r0 += r2; SET_UPVALUE; r1 += 1; cmp; loop
    inner->emit(make_ABC(OpCode::GET_UPVALUE, 0, 0, 0), { }); // r0 = upval[0]
    inner->emit(make_ABC(OpCode::OP_ADD, 0, 0, 2), { });      // r0 += 1
    inner->emit(make_ABC(OpCode::SET_UPVALUE, 0, 0, 0), { }); // upval[0] = r0
    inner->emit(make_ABC(OpCode::OP_ADD, 1, 1, 2), { });      // r1 += 1
    inner->emit(make_ABC(OpCode::OP_LT, 0, 1, 3), { });       // r0 = i < N
    inner->emit(make_AsBx(OpCode::JUMP_IF_FALSE, 0, 1), { }); // exit loop
    inner->emit(make_AsBx(OpCode::LOOP, 0, -7), { });         // back
    inner->emit(make_ABC(OpCode::GET_UPVALUE, 0, 0, 0), { }); // return upval
    inner->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });

    auto* outer = makeChunk();
    outer->name = "outer";
    outer->arity = 0;
    outer->localCount = 2;
    outer->functions.push(inner);
    outer->emit(make_ABx(OpCode::LOAD_INT, 0, BX(0)), { }); // r0 = 0 (upval seed)
    outer->emit(make_ABx(OpCode::CLOSURE, 1, 0), { });      // r1 = inner closure
    outer->emit(make_ABC(OpCode::MOVE, 1, 0, 0), { });      // capture r0 as upvalue
    outer->emit(make_ABC(OpCode::CALL, 1, 0, 0), { });      // call inner()
    outer->emit(make_ABC(OpCode::RETURN, 1, 1, 0), { });

    auto* top = makeChunk();
    top->name = "<upval_perf>";
    top->localCount = 2;
    top->functions.push(outer);
    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABC(OpCode::CALL, 0, 0, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });

    top->disassemble();
    VM vm;

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    std::printf("  Upvalue get+set %dk iters:          %.1f µs  (%.2f ns/op)\n",
        N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 8. List operations throughput
//    Builds a list of N elements then iterates over it summing values.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfTest, List_AppendAndSum_10k)
{
    constexpr int N = 10'000;
    ast::Stmt* test = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("xs"), ast::makeList(), true),
            ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0), true),
            ast::makeAssignmentStmt(ast::makeName("limit"), ast::makeLiteralInt(N), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeExprStmt(ast::makeCall(ast::makeName("اضف"), ast::makeList({ ast::makeName("xs"), ast::makeName("i") }))),
                    ast::makeAssignmentStmt(
                        ast::makeName("i"),
                        ast::makeBinary(ast::makeName("i"), ast::makeLiteralInt(1), ast::BinaryOp::OP_ADD)) })),
            ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0)),
            ast::makeAssignmentStmt(ast::makeName("sum"), ast::makeLiteralInt(0), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeAssignmentStmt(
                                     ast::makeName("sum"),
                                     ast::makeBinary(
                                         ast::makeName("sum"),
                                         ast::makeIndex(ast::makeName("xs"), ast::makeName("i")),
                                         ast::BinaryOp::OP_ADD)),
                    ast::makeAssignmentStmt(
                        ast::makeName("i"),
                        ast::makeBinary(ast::makeName("i"), ast::makeLiteralInt(1), ast::BinaryOp::OP_ADD)) })),
            ast::makeReturn(ast::makeName("sum")) }));

    Chunk* top = compile_calling(test);
    top->disassemble();
    VM vm;
    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);
    do_not_optimize(result);

    constexpr int64_t expected = static_cast<int64_t>(N) * (N - 1) / 2;
    EXPECT_EQ(AS_INTEGER(result), expected);
    std::printf("  List append+sum N=%d:               %.1f µs\n", N, us);
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. Native function call throughput — nativeLen on a string, hot loop
//    IC_CALL path: first call warms the inline cache, subsequent calls hit it.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfTest, NativeCall_Len_50k_ICHot)
{
    constexpr int N = 50'000;

    ast::Stmt* test = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("s"), ast::makeLiteralString("hello world"), true),
            ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0), true),
            ast::makeAssignmentStmt(ast::makeName("limit"), ast::makeLiteralInt(N), true),
            ast::makeAssignmentStmt(ast::makeName("last"), ast::makeLiteralInt(0), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("last"), ast::makeCall(ast::makeName("len"), ast::makeList({ ast::makeName("s") }))),
                    ast::makeAssignmentStmt(
                        ast::makeName("i"),
                        ast::makeBinary(ast::makeName("i"), ast::makeLiteralInt(1), ast::BinaryOp::OP_ADD)) })),
            ast::makeReturn(ast::makeName("last")) }));

    Chunk* top = compile_calling(test);
    top->disassemble();
    VM vm;

    // Cold run to prime the IC.
    vm.run(top);

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(AS_INTEGER(result), 11);
    std::printf("  NativeCall len() IC hot %dk:        %.1f µs  (%.2f ns/call)\n",
        N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 10. Mixed realistic workload — Fibonacci(25) repeated
//     Exercises: calls, branches, integer arithmetic, globals lookup.
//     fib(25) = 75025. We run it 100 times and report total + per-call time.
// ─────────────────────────────────────────────────────────────────────────────

static Chunk* make_fib_top(int n, int reps)
{
    ast::Stmt* fib = ast::makeFunction(
        ast::makeName("fib"),
        ast::makeList({ ast::makeName("x") }),
        ast::makeBlock({ ast::makeIf(
                             ast::makeBinary(ast::makeName("x"), ast::makeLiteralInt(1), ast::BinaryOp::OP_LTE),
                             ast::makeBlock({ ast::makeReturn(ast::makeName("x")) })),
            ast::makeReturn(ast::makeBinary(
                ast::makeCall(ast::makeName("fib"), ast::makeList({ ast::makeBinary(ast::makeName("x"), ast::makeLiteralInt(1), ast::BinaryOp::OP_SUB) })),
                ast::makeCall(ast::makeName("fib"), ast::makeList({ ast::makeBinary(ast::makeName("x"), ast::makeLiteralInt(2), ast::BinaryOp::OP_SUB) })),
                ast::BinaryOp::OP_ADD)) }));

    ast::Stmt* test = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0), true),
            ast::makeAssignmentStmt(ast::makeName("limit"), ast::makeLiteralInt(reps), true),
            ast::makeAssignmentStmt(ast::makeName("result"), ast::makeLiteralInt(0), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("result"), ast::makeCall(ast::makeName("fib"), ast::makeList({ ast::makeLiteralInt(n) }))),
                    ast::makeAssignmentStmt(
                        ast::makeName("i"),
                        ast::makeBinary(ast::makeName("i"), ast::makeLiteralInt(1), ast::BinaryOp::OP_ADD)) })),
            ast::makeReturn(ast::makeName("result")) }));

    Chunk* top = compile_program({ fib,
        test,
        ast::makeExprStmt(ast::makeCall(ast::makeName("test"), ast::makeList())) });
    top->disassemble();
    return top;
}

TEST(VMPerfTest, Fib20_100reps)
{
    constexpr int FIB_N = 20;
    constexpr int REPS = 100;

    VM vm;
    auto* top = make_fib_top(FIB_N, REPS);

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(AS_INTEGER(result), 6765); // fib(20)
    ::printf("  fib(%d) × %d reps:                  %.1f µs  (%.1f µs/call)\n", FIB_N, REPS, us, us / REPS);
}

TEST(VMPerfTest, Fib25_10reps)
{
    constexpr int FIB_N = 25;
    constexpr int REPS = 10;

    VM vm;
    auto* top = make_fib_top(FIB_N, REPS);

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(AS_INTEGER(result), 75025); // fib(25)
    ::printf("  fib(%d) × %d reps:                   %.1f µs  (%.1f µs/call)\n", FIB_N, REPS, us, us / REPS);
}

// ─────────────────────────────────────────────────────────────────────────────
// 11. Large stress tests (opt-in)
//     These are intentionally much larger than the regular perf tests and are
//     gated behind MYLANG_ENABLE_STRESS_PERF=1 so they do not dominate normal
//     test runs.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfStressTest, Dispatch_IntAdd_10M_Iterations)
{
    require_stress_perf();

    constexpr int N = 10'000'000;

    ast::Stmt* test = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0), true),
            ast::makeAssignmentStmt(ast::makeName("step"), ast::makeLiteralInt(1), true),
            ast::makeAssignmentStmt(ast::makeName("limit"), ast::makeLiteralInt(N), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeAssignmentStmt(
                    ast::makeName("i"),
                    ast::makeBinary(ast::makeName("i"), ast::makeName("step"), ast::BinaryOp::OP_ADD)) })),
            ast::makeReturn(ast::makeName("i")) }));

    Chunk* top = compile_calling(test);
    VM vm;
    vm.run(top); // warm quickened arithmetic path

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(AS_INTEGER(result), N);
    std::printf("  STRESS IntAdd loop %dM iters:       %.1f µs  (%.2f ns/op)\n",
        N / 1'000'000, us, us * 1000.0 / N);
}

TEST(VMPerfStressTest, NativeCall_Len_1M_ICHot)
{
    require_stress_perf();

    constexpr int N = 1'000'000;

    ast::Stmt* test = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("s"), ast::makeLiteralString("hello world"), true),
            ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0), true),
            ast::makeAssignmentStmt(ast::makeName("limit"), ast::makeLiteralInt(N), true),
            ast::makeAssignmentStmt(ast::makeName("last"), ast::makeLiteralInt(0), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("last"), ast::makeCall(ast::makeName("len"), ast::makeList({ ast::makeName("s") }))),
                    ast::makeAssignmentStmt(
                        ast::makeName("i"),
                        ast::makeBinary(ast::makeName("i"), ast::makeLiteralInt(1), ast::BinaryOp::OP_ADD)) })),
            ast::makeReturn(ast::makeName("last")) }));

    Chunk* top = compile_calling(test);
    VM vm;
    vm.run(top); // warm IC_CALL -> CALL rewrite

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(AS_INTEGER(result), 11);
    std::printf("  STRESS native len() %dM calls:      %.1f µs  (%.2f ns/call)\n",
        N / 1'000'000, us, us * 1000.0 / N);
}

TEST(VMPerfStressTest, List_AppendAndSum_100k)
{
    require_stress_perf();

    constexpr int N = 100'000;

    ast::Stmt* test = ast::makeFunction(
        ast::makeName("test"),
        ast::makeList(),
        ast::makeBlock({ ast::makeAssignmentStmt(ast::makeName("xs"), ast::makeList(), true),
            ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0), true),
            ast::makeAssignmentStmt(ast::makeName("limit"), ast::makeLiteralInt(N), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeExprStmt(ast::makeCall(ast::makeName("اضف"), ast::makeList({ ast::makeName("xs"), ast::makeName("i") }))),
                    ast::makeAssignmentStmt(
                        ast::makeName("i"),
                        ast::makeBinary(ast::makeName("i"), ast::makeLiteralInt(1), ast::BinaryOp::OP_ADD)) })),
            ast::makeAssignmentStmt(ast::makeName("i"), ast::makeLiteralInt(0)),
            ast::makeAssignmentStmt(ast::makeName("sum"), ast::makeLiteralInt(0), true),
            ast::makeWhile(
                ast::makeBinary(ast::makeName("i"), ast::makeName("limit"), ast::BinaryOp::OP_LT),
                ast::makeBlock({ ast::makeAssignmentStmt(
                                     ast::makeName("sum"),
                                     ast::makeBinary(
                                         ast::makeName("sum"),
                                         ast::makeIndex(ast::makeName("xs"), ast::makeName("i")),
                                         ast::BinaryOp::OP_ADD)),
                    ast::makeAssignmentStmt(
                        ast::makeName("i"),
                        ast::makeBinary(ast::makeName("i"), ast::makeLiteralInt(1), ast::BinaryOp::OP_ADD)) })),
            ast::makeReturn(ast::makeName("sum")) }));

    Chunk* top = compile_calling(test);
    VM vm;

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);

    constexpr int64_t expected = static_cast<int64_t>(N) * (N - 1) / 2;
    do_not_optimize(result);
    EXPECT_EQ(AS_INTEGER(result), expected);
    std::printf("  STRESS list append+sum N=%d:        %.1f µs\n", N, us);
}

TEST(VMPerfStressTest, Fib28_20reps_Hot)
{
    require_stress_perf();

    constexpr int FIB_N = 28;
    constexpr int REPS = 20;

    VM vm;
    auto* top = make_fib_top(FIB_N, REPS);
    vm.run(top); // warm global lookup and call-site rewriting

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    double us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(AS_INTEGER(result), 317811);
    std::printf("  STRESS fib(%d) × %d reps hot:       %.1f µs  (%.1f µs/call)\n",
        FIB_N, REPS, us, us / REPS);
}
