#include "../include/ast.hpp"
#include "../include/compiler.hpp"
#include "../include/opcode.hpp"
#include "../include/vm.hpp"

#include <bit>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <gtest/gtest.h>

using namespace fairuz;
using namespace fairuz::runtime;

static constexpr u16 BX(int v) { return static_cast<u16>(v + 32767); }

struct CB {
    Fa_Chunk* ch { nullptr };

    CB()
    {
        ch = make_chunk();
        ch->m_name = "<test>";
    }

    CB& ABC(Fa_OpCode op, u8 A, u8 B, u8 C, u32 ln = 1)
    {
        ch->emit(Fa_make_ABC(op, A, B, C), { ln, 0, 0 });
        return *this;
    }
    CB& ABx(Fa_OpCode op, u8 A, u16 Bx, u32 ln = 1)
    {
        ch->emit(Fa_make_ABx(op, A, Bx), { ln, 0, 0 });
        return *this;
    }
    CB& AsBx(Fa_OpCode op, u8 A, int s_bx, u32 ln = 1)
    {
        ch->emit(Fa_make_AsBx(op, A, s_bx), { ln, 0, 0 });
        return *this;
    }

    CB& load_int(u8 r, int v) { return ABx(Fa_OpCode::LOAD_INT, r, BX(v)); }
    CB& ret(u8 r, u8 n = 1) { return ABC(Fa_OpCode::RETURN, r, n, 0); }
    CB& ret_nil() { return ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0); }
    CB& nop(u8 slot = 0) { return ABC(Fa_OpCode::NOP, slot, 0, 0); }
    CB& mov(u8 d, u8 s) { return ABC(Fa_OpCode::MOVE, d, s, 0); }

    u16 str(char const* s)
    {
        strs_.emplace_back(std::make_unique<Fa_ObjString>(Fa_StringRef(s)));
        return ch->add_constant(Fa_MAKE_OBJECT(strs_.back().get()));
    }

    CB& ldg(u8 r, char const* m_name) { return ABx(Fa_OpCode::LOAD_GLOBAL, r, str(m_name)); }
    CB& stg(u8 r, char const* m_name) { return ABx(Fa_OpCode::STORE_GLOBAL, r, str(m_name)); }

    CB& regs(int n)
    {
        ch->local_count = n;
        return *this;
    }
    CB& slot()
    {
        ch->alloc_ic_slot();
        return *this;
    }

    void dump() const
    {
        std::cout << "Disassemebeled chunk:" << '\n';
        if (ch)
            ch->disassemble();
        std::cout << '\n';
    }

    CB& load_val(u8 r, i64 v)
    {
        if (v >= -32767 && v <= 32768) {
            return load_int(r, static_cast<int>(v)); // fits in LOAD_INT
        } else {
            u16 k = ch->add_constant(Fa_MAKE_INTEGER(v));
            return ABx(Fa_OpCode::LOAD_CONST, r, k); // spill to constant pool
        }
    }

private:
    std::vector<std::unique_ptr<Fa_ObjString>> strs_;
};

struct VMRunner {
    Fa_VM vm;
    Fa_Chunk* chunk_ = make_chunk();

    Fa_Value run(CB& b)
    {
        chunk_ = b.ch;
        return vm.run(chunk_);
    }
    Fa_Value run(Fa_Chunk* c)
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

Fa_Chunk* compile_program(Fa_Array<AST::Fa_Stmt*> stmts)
{
    return Compiler().compile(stmts);
}

Fa_Chunk* compile_calling(AST::Fa_Stmt* fn)
{
    auto* m_name = static_cast<AST::Fa_FunctionDef*>(fn)->get_name();
    return compile_program({ fn, AST::Fa_makeExprStmt(AST::Fa_makeCall(AST::Fa_makeName(m_name->get_value()), AST::Fa_makeList())) });
}

} // namespace

TEST(VMLoads, Nil)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(Fa_OpCode::LOAD_NIL, 0, 0, 1).ret(0);
    b.dump();
    EXPECT_TRUE(Fa_IS_NIL(r.run(b)));
}

TEST(VMLoads, NilFillsMultiple)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(Fa_OpCode::LOAD_NIL, 0, 0, 3).ret(2);
    b.dump();
    EXPECT_TRUE(Fa_IS_NIL(r.run(b)));
}

TEST(VMLoads, True)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(Fa_OpCode::LOAD_TRUE, 0, 0, 0).ret(0);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && Fa_AS_BOOL(v));
}

TEST(VMLoads, False)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(Fa_OpCode::LOAD_FALSE, 0, 0, 0).ret(0);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && !Fa_AS_BOOL(v));
}

TEST(VMLoads, IntPositive)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 42).ret(0);
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_INTEGER(v));
    EXPECT_EQ(Fa_AS_INTEGER(v), 42);
}

TEST(VMLoads, IntZero)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 0).ret(0);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_INTEGER(v));
    EXPECT_EQ(Fa_AS_INTEGER(v), 0);
}

TEST(VMLoads, IntNegative)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, -100).ret(0);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_INTEGER(v));
    EXPECT_EQ(Fa_AS_INTEGER(v), -100);
}

TEST(VMLoads, IntMaxEncodable)
{
    VMRunner r;
    CB b;
    b.regs(1).ABx(Fa_OpCode::LOAD_INT, 0, 65535).ret(0);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_INTEGER(v));
    EXPECT_EQ(Fa_AS_INTEGER(v), 32768);
}

TEST(VMLoads, IntMinEncodable)
{
    VMRunner r;
    CB b;
    b.regs(1).ABx(Fa_OpCode::LOAD_INT, 0, 0).ret(0);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_INTEGER(v));
    EXPECT_EQ(Fa_AS_INTEGER(v), -32767);
}

TEST(VMLoads, ConstDouble)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.ch->add_constant(Fa_MAKE_REAL(3.14));
    b.ABx(Fa_OpCode::LOAD_CONST, 0, k).ret(0);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_DOUBLE(v));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(v), 3.14);
}

TEST(VMLoads, ConstString)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.str("hello");
    b.ABx(Fa_OpCode::LOAD_CONST, 0, k).ret(0);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_STRING(v));
    EXPECT_EQ(Fa_AS_STRING(v)->str, "hello");
}

TEST(VMLoads, ConstLargeInt)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.ch->add_constant(Fa_MAKE_INTEGER(1000000LL));
    b.ABx(Fa_OpCode::LOAD_CONST, 0, k).ret(0);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_INTEGER(v));
    EXPECT_EQ(Fa_AS_INTEGER(v), 1000000LL);
}

TEST(VMLoads, ReturnNil)
{
    VMRunner r;
    CB b;
    b.regs(1).ret_nil();
    EXPECT_TRUE(Fa_IS_NIL(r.run(b)));
}

TEST(VMMove, Copies)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 77).mov(1, 0).ret(1);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_INTEGER(v));
    EXPECT_EQ(Fa_AS_INTEGER(v), 77);
}

TEST(VMMove, SourceUnchanged)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 55).mov(1, 0).ret(0);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_EQ(Fa_AS_INTEGER(v), 55);
}

TEST(VMArith, AddIntFastPath)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 32).ABC(Fa_OpCode::OP_ADD, 2, 0, 1).nop().ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_INTEGER(v));
    EXPECT_EQ(Fa_AS_INTEGER(v), 42);
}

TEST(VMArith, AddDoubles)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.ch->add_constant(Fa_MAKE_REAL(1.5));
    u16 k1 = b.ch->add_constant(Fa_MAKE_REAL(2.5));
    b.ABx(Fa_OpCode::LOAD_CONST, 0, k0).ABx(Fa_OpCode::LOAD_CONST, 1, k1).ABC(Fa_OpCode::OP_ADD, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_DOUBLE(v));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(v), 4.0);
}

TEST(VMArith, AddStringsConcat)
{
    GTEST_SKIP() << "Not supported yet.";
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.str("foo");
    u16 k1 = b.str("bar");
    b.ABx(Fa_OpCode::LOAD_CONST, 0, k0).ABx(Fa_OpCode::LOAD_CONST, 1, k1).ABC(Fa_OpCode::OP_ADD, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_STRING(v));
    EXPECT_EQ(Fa_AS_STRING(v)->str, "foobar");
}

TEST(VMArith, SubInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 100).load_int(1, 58).ABC(Fa_OpCode::OP_SUB, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 42);
}

TEST(VMArith, MulInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 7).ABC(Fa_OpCode::OP_MUL, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 42);
}

TEST(VMArith, DivDouble)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.ch->add_constant(Fa_MAKE_REAL(84.0));
    u16 k1 = b.ch->add_constant(Fa_MAKE_REAL(2.0));
    b.ABx(Fa_OpCode::LOAD_CONST, 0, k0).ABx(Fa_OpCode::LOAD_CONST, 1, k1).ABC(Fa_OpCode::OP_DIV, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r.run(b)), 42.0);
}

TEST(VMArith, ModPositive)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 17).load_int(1, 5).ABC(Fa_OpCode::OP_MOD, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r.run(b)), 2.0);
}

TEST(VMArith, Pow)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 2).load_int(1, 10).ABC(Fa_OpCode::OP_POW, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r.run(b)), 1024.0);
}

TEST(VMArith, NegInt)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 7).ABC(Fa_OpCode::OP_NEG, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), -7);
}

TEST(VMArith, NegDouble)
{
    VMRunner r;
    CB b;
    b.regs(2);
    u16 k = b.ch->add_constant(Fa_MAKE_REAL(3.5));
    b.ABx(Fa_OpCode::LOAD_CONST, 0, k).ABC(Fa_OpCode::OP_NEG, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r.run(b)), -3.5);
}

TEST(VMArith, DivByZeroThrows)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.ch->add_constant(Fa_MAKE_REAL(1.0));
    u16 k1 = b.ch->add_constant(Fa_MAKE_REAL(0.0));
    b.ABx(Fa_OpCode::LOAD_CONST, 0, k0).ABx(Fa_OpCode::LOAD_CONST, 1, k1).ABC(Fa_OpCode::OP_DIV, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMArith, NegOnStringThrows)
{
    VMRunner r;
    CB b;
    b.regs(2).ABx(Fa_OpCode::LOAD_CONST, 0, b.str("x")).ABC(Fa_OpCode::OP_NEG, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMBitwise, And)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b1010).ABC(Fa_OpCode::OP_BITAND, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 0b1010);
}

TEST(VMBitwise, Or)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1100).load_int(1, 0b0011).ABC(Fa_OpCode::OP_BITOR, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 0b1111);
}

TEST(VMBitwise, Xor)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b0101).ABC(Fa_OpCode::OP_BITXOR, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 0b1010);
}

TEST(VMBitwise, Not)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(Fa_OpCode::OP_BITNOT, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), ~i64(0));
}

TEST(VMBitwise, Shl)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1).ABC(Fa_OpCode::OP_LSHIFT, 1, 0, 8).ret(1);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 256);
}

TEST(VMBitwise, Shr)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1024).ABC(Fa_OpCode::OP_RSHIFT, 1, 0, 3).ret(1);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 128);
}

TEST(VMBitwise, ShrLogical_NegativeInput)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, -8).load_int(1, 1).ABC(Fa_OpCode::OP_RSHIFT, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_DOUBLE(v));
    EXPECT_GT(Fa_AS_DOUBLE(v), 0.0);
}

TEST(VMBitwise, AndOnDoubleThrows)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k = b.ch->add_constant(Fa_MAKE_REAL(1.0));
    b.ABx(Fa_OpCode::LOAD_CONST, 0, k).load_int(1, 1).ABC(Fa_OpCode::OP_BITAND, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMCompare, EqIntsTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(Fa_OpCode::OP_EQ, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && Fa_AS_BOOL(v));
}

TEST(VMCompare, EqIntsFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(Fa_OpCode::OP_EQ, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && !Fa_AS_BOOL(v));
}

TEST(VMCompare, NeqTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(Fa_OpCode::OP_NEQ, 2, 0, 1).ret(2);
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && Fa_AS_BOOL(v));
}

TEST(VMCompare, LtTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 3).load_int(1, 7).ABC(Fa_OpCode::OP_LT, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && Fa_AS_BOOL(v));
}

TEST(VMCompare, LtFalseEqual)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(Fa_OpCode::OP_LT, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && !Fa_AS_BOOL(v));
}

TEST(VMCompare, LeTrue_Equal)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(Fa_OpCode::OP_LTE, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && Fa_AS_BOOL(v));
}

TEST(VMCompare, LeFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 5).ABC(Fa_OpCode::OP_LTE, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && !Fa_AS_BOOL(v));
}

TEST(VMCompare, EqSameString)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.str("hello");
    u16 k1 = b.str("hello");
    b.ABx(Fa_OpCode::LOAD_CONST, 0, k0).ABx(Fa_OpCode::LOAD_CONST, 1, k1).ABC(Fa_OpCode::OP_EQ, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && Fa_AS_BOOL(v));
}

TEST(VMCompare, NotFalseIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(Fa_OpCode::LOAD_FALSE, 0, 0, 0).ABC(Fa_OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && Fa_AS_BOOL(v));
}

TEST(VMCompare, NotTrueIsFalse)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(Fa_OpCode::LOAD_TRUE, 0, 0, 0).ABC(Fa_OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && !Fa_AS_BOOL(v));
}

TEST(VMCompare, NotNilIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(Fa_OpCode::LOAD_NIL, 0, 0, 1).ABC(Fa_OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && Fa_AS_BOOL(v));
}

TEST(VMCompare, NotZeroIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(Fa_OpCode::OP_NOT, 1, 0, 0).ret(1);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && Fa_AS_BOOL(v));
}

TEST(VMCompare, LtStrings)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 ka = b.str("apple");
    u16 kb = b.str("banana");
    b.ABx(Fa_OpCode::LOAD_CONST, 0, ka).ABx(Fa_OpCode::LOAD_CONST, 1, kb).ABC(Fa_OpCode::OP_LT, 2, 0, 1).ret(2);
    b.dump();
    Fa_Value v = r.run(b);
    EXPECT_TRUE(Fa_IS_BOOL(v) && Fa_AS_BOOL(v));
}

TEST(VMGlobals, StoreAndLoad)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 123).stg(0, "g").load_int(0, 0).ldg(1, "g").ret(1);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 123);
}

TEST(VMGlobals, MissingGlobalIsNil)
{
    VMRunner r;
    CB b;
    b.regs(1).ldg(0, "nope").ret(0);
    EXPECT_TRUE(Fa_IS_NIL(r.run(b)));
}

TEST(VMGlobals, Overwrite)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1).stg(0, "x").load_int(0, 2).stg(0, "x").ldg(1, "x").ret(1);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 2);
}

TEST(VMLists, NewEmpty)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(Fa_OpCode::LIST_NEW, 0, 0, 0).ABC(Fa_OpCode::LIST_LEN, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 0);
}

TEST(VMLists, AppendAndLen)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(Fa_OpCode::LIST_NEW, 0, 3, 0);
    for (int v : { 10, 20, 30 })
        b.load_int(1, v).ABC(Fa_OpCode::LIST_APPEND, 0, 1, 0);
    b.ABC(Fa_OpCode::LIST_LEN, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 3);
}

TEST(VMLists, GetFirst)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .ABC(Fa_OpCode::LIST_NEW, 0, 2, 0)
        .load_int(1, 77)
        .ABC(Fa_OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 88)
        .ABC(Fa_OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 0)
        .ABC(Fa_OpCode::LIST_GET, 2, 0, 1)
        .ret(2);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 77);
}

TEST(VMLists, GetLast)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .ABC(Fa_OpCode::LIST_NEW, 0, 2, 0)
        .load_int(1, 10)
        .ABC(Fa_OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 20)
        .ABC(Fa_OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 1)
        .ABC(Fa_OpCode::LIST_GET, 2, 0, 1)
        .ret(2);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 20);
}

TEST(VMLists, Set)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .ABC(Fa_OpCode::LIST_NEW, 0, 1, 0)
        .load_int(1, 0)
        .ABC(Fa_OpCode::LIST_APPEND, 0, 1, 0)
        .load_int(1, 0)
        .load_int(2, 99)
        .ABC(Fa_OpCode::LIST_SET, 0, 1, 2)
        .load_int(1, 0)
        .ABC(Fa_OpCode::LIST_GET, 2, 0, 1)
        .ret(2);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 99);
}

TEST(VMLists, OutOfBoundsThrows)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(Fa_OpCode::LIST_NEW, 0, 0, 0).load_int(1, 0).ABC(Fa_OpCode::LIST_GET, 2, 0, 1).ret(2);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMLists, LenOnNonListThrows)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 5).ABC(Fa_OpCode::LIST_LEN, 1, 0, 0).ret(1);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMDicts, IndexReturnsStoredValue)
{
    Fa_ObjString key(Fa_StringRef("lang"));
    Fa_ObjDict* obj_dict = get_allocator().allocate_object<Fa_ObjDict>();
    Fa_Value dict = Fa_MAKE_OBJECT(obj_dict);
    Fa_AS_DICT(dict)->data.push({ Fa_MAKE_OBJECT(&key), Fa_MAKE_INTEGER(7) });

    VMRunner r;
    CB b;
    u16 dict_k = b.ch->add_constant(dict);
    u16 key_k = b.ch->add_constant(Fa_MAKE_OBJECT(&key));
    b.regs(3)
        .ABx(Fa_OpCode::LOAD_CONST, 0, dict_k)
        .ABx(Fa_OpCode::LOAD_CONST, 1, key_k)
        .ABC(Fa_OpCode::INDEX, 2, 0, 1)
        .ret(2);

    Fa_Value result = r.run(b);
    ASSERT_TRUE(Fa_IS_INTEGER(result));
    EXPECT_EQ(Fa_AS_INTEGER(result), 7);
}

TEST(VMDicts, SetUpdatesAndAppendsByKey)
{
    Fa_ObjString existing_key(Fa_StringRef("x"));
    Fa_ObjString new_key(Fa_StringRef("y"));
    Fa_ObjDict* obj_dict = get_allocator().allocate_object<Fa_ObjDict>();
    Fa_Value dict = Fa_MAKE_OBJECT(obj_dict);
    Fa_AS_DICT(dict)->data.push({ Fa_MAKE_OBJECT(&existing_key), Fa_MAKE_INTEGER(1) });

    VMRunner r;
    CB b;
    u16 dict_k = b.ch->add_constant(dict);
    u16 existing_key_k = b.ch->add_constant(Fa_MAKE_OBJECT(&existing_key));
    u16 new_key_k = b.ch->add_constant(Fa_MAKE_OBJECT(&new_key));
    b.regs(4)
        .ABx(Fa_OpCode::LOAD_CONST, 0, dict_k)
        .ABx(Fa_OpCode::LOAD_CONST, 1, existing_key_k)
        .load_int(2, 99)
        .ABC(Fa_OpCode::LIST_SET, 0, 1, 2)
        .ABx(Fa_OpCode::LOAD_CONST, 1, new_key_k)
        .load_int(2, 42)
        .ABC(Fa_OpCode::LIST_SET, 0, 1, 2)
        .ABx(Fa_OpCode::LOAD_CONST, 1, existing_key_k)
        .ABC(Fa_OpCode::INDEX, 2, 0, 1)
        .ABx(Fa_OpCode::LOAD_CONST, 1, new_key_k)
        .ABC(Fa_OpCode::INDEX, 3, 0, 1)
        .ret(3);

    Fa_Value result = r.run(b);
    ASSERT_TRUE(Fa_IS_INTEGER(result));
    EXPECT_EQ(Fa_AS_INTEGER(result), 42);
    ASSERT_EQ(Fa_AS_DICT(dict)->data.size(), 2u);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_DICT(dict)->data[0].second), 99);
}

TEST(VMDicts, MissingKeyReturnsNil)
{
    Fa_ObjString key(Fa_StringRef("missing"));
    Fa_ObjDict* obj_dict = get_allocator().allocate_object<Fa_ObjDict>();
    Fa_Value dict = Fa_MAKE_OBJECT(obj_dict);

    VMRunner r;
    CB b;
    u16 dict_k = b.ch->add_constant(dict);
    u16 key_k = b.ch->add_constant(Fa_MAKE_OBJECT(&key));
    b.regs(3)
        .ABx(Fa_OpCode::LOAD_CONST, 0, dict_k)
        .ABx(Fa_OpCode::LOAD_CONST, 1, key_k)
        .ABC(Fa_OpCode::INDEX, 2, 0, 1)
        .ret(2);

    EXPECT_TRUE(Fa_IS_NIL(r.run(b)));
}

static Fa_Chunk* make_adder_chunk()
{
    auto fn = make_chunk();
    fn->m_name = "add2";
    fn->arity = 2;
    fn->local_count = 3;
    // r0=a r1=b (params); r2=a+b
    fn->emit(Fa_make_ABC(Fa_OpCode::OP_ADD, 2, 0, 1), { });
    fn->emit(Fa_make_ABC(Fa_OpCode::RETURN, 2, 1, 0), { });
    return fn;
}

TEST(VMCalls, CallClosure_TwoArgs)
{
    auto top = make_chunk();
    top->m_name = "<test>";
    top->local_count = 4;
    top->functions.push(make_adder_chunk());

    top->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 0, 0), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 1, BX(3)), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 2, BX(4)), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 2, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(Fa_AS_INTEGER(r.run(top)), 7);
}

TEST(VMCalls, WrongArgcThrows)
{
    auto top = make_chunk();
    top->m_name = "<test>";
    top->local_count = 3;
    top->functions.push(make_adder_chunk());

    top->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 0, 0), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 1, BX(1)), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 1, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();
    VMRunner r;
    EXPECT_THROW(r.run(top), std::runtime_error);
}

TEST(VMCalls, CallNonFunctionThrows)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 5).ABC(Fa_OpCode::CALL, 0, 0, 0).ret(0);
    b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMCalls, ICCallNativeLen)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .slot()
        .ABC(Fa_OpCode::LIST_NEW, 0, 3, 0)
        .load_int(2, 1)
        .ABC(Fa_OpCode::LIST_APPEND, 0, 2, 0)
        .load_int(2, 2)
        .ABC(Fa_OpCode::LIST_APPEND, 0, 2, 0)
        .load_int(2, 3)
        .ABC(Fa_OpCode::LIST_APPEND, 0, 2, 0)
        .ldg(1, "حجم")
        .mov(2, 0)
        .ABC(Fa_OpCode::IC_CALL, 1, 1, 0)
        .ret(1);
    b.dump();
    EXPECT_EQ(Fa_AS_INTEGER(r.run(b)), 3);
}

TEST(VMCalls, TailCall_DoesNotOverflowFrames)
{
    auto fn = make_chunk();
    fn->m_name = "cd";
    fn->arity = 1;
    fn->local_count = 4;

    u16 nk = fn->add_constant(Fa_MAKE_STRING("cd"));

    fn->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 1, BX(0)), { });
    fn->emit(Fa_make_ABC(Fa_OpCode::OP_EQ, 1, 0, 1), { });
    fn->emit(Fa_make_AsBx(Fa_OpCode::JUMP_IF_FALSE, 1, 1), { });
    fn->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });
    fn->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 1, BX(1)), { });
    fn->emit(Fa_make_ABC(Fa_OpCode::OP_SUB, 0, 0, 1), { });
    fn->emit(Fa_make_ABx(Fa_OpCode::LOAD_GLOBAL, 2, nk), { });
    fn->emit(Fa_make_ABC(Fa_OpCode::MOVE, 3, 0, 0), { });
    fn->emit(Fa_make_ABC(Fa_OpCode::CALL_TAIL, 2, 1, 0), { });

    auto top = make_chunk();
    top->m_name = "<test>";
    top->local_count = 3;
    top->functions.push(fn);

    u16 tk = top->add_constant(Fa_MAKE_STRING("cd"));
    top->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 0, 0), { });
    top->emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, 0, tk), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 1, BX(300)), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 1, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();
    VMRunner r;
    Fa_Value v = r.run(std::move(top));
    EXPECT_EQ(Fa_AS_INTEGER(v), 0);
}

TEST(VMCalls, StackOverflowDetected)
{
    auto fn = make_chunk();
    fn->m_name = "inf";
    fn->arity = 0;
    fn->local_count = 1;
    u16 nk = fn->add_constant(Fa_MAKE_STRING("inf"));
    fn->emit(Fa_make_ABx(Fa_OpCode::LOAD_GLOBAL, 0, nk), { });
    fn->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 0, 0), { });
    fn->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });

    auto top = make_chunk();
    top->m_name = "<test>";
    top->local_count = 1;
    top->functions.push(fn);
    u16 tk = top->add_constant(Fa_MAKE_STRING("inf"));
    top->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 0, 0), { });
    top->emit(Fa_make_ABx(Fa_OpCode::STORE_GLOBAL, 0, tk), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 0, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();
    VMRunner r;
    EXPECT_THROW(r.run(top), std::runtime_error);
}

TEST(VMClosures, CaptureLocal_GetUpvalue)
{
    auto inner = make_chunk();
    inner->m_name = "inner";
    inner->arity = 0;
    inner->upvalue_count = 1;
    inner->local_count = 1;
    inner->emit(Fa_make_ABC(Fa_OpCode::GET_UPVALUE, 0, 0, 0), { });
    inner->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });

    auto outer = make_chunk();
    outer->m_name = "outer";
    outer->arity = 0;
    outer->local_count = 2;
    outer->upvalue_count = 0;
    outer->functions.push(inner);
    outer->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 0, BX(10)), { });
    outer->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 1, 0), { });
    outer->emit(Fa_make_ABC(Fa_OpCode::MOVE, 1, 0, 0), { });
    outer->emit(Fa_make_ABC(Fa_OpCode::CALL, 1, 0, 0), { });
    outer->emit(Fa_make_ABC(Fa_OpCode::RETURN, 1, 1, 0), { });

    auto top = make_chunk();
    top->m_name = "<test>";
    top->local_count = 2;
    top->functions.push(outer);
    top->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 0, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 0, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(Fa_AS_INTEGER(r.run(std::move(top))), 10);
}

TEST(VMClosures, CloseUpvalue_SurvivesFrame)
{
    auto add = make_chunk();
    add->m_name = "add";
    add->arity = 1;
    add->upvalue_count = 1;
    add->local_count = 3;
    add->emit(Fa_make_ABC(Fa_OpCode::GET_UPVALUE, 1, 0, 0), { });
    add->emit(Fa_make_ABC(Fa_OpCode::OP_ADD, 2, 1, 0), { });
    add->emit(Fa_make_ABC(Fa_OpCode::RETURN, 2, 1, 0), { });

    auto ma = make_chunk();
    ma->m_name = "ma";
    ma->arity = 1;
    ma->local_count = 2;
    ma->upvalue_count = 0;
    ma->functions.push(add);
    ma->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 1, 0), { });
    ma->emit(Fa_make_ABC(Fa_OpCode::MOVE, 1, 0, 0), { });
    ma->emit(Fa_make_ABC(Fa_OpCode::CLOSE_UPVALUE, 0, 0, 0), { });
    ma->emit(Fa_make_ABC(Fa_OpCode::RETURN, 1, 1, 0), { });

    auto top = make_chunk();
    top->m_name = "<test>";
    top->local_count = 3;
    top->functions.push(ma);
    top->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 0, 0), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 1, BX(10)), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 1, 0), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 1, BX(5)), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 1, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();

    VMRunner r;
    EXPECT_EQ(Fa_AS_INTEGER(r.run(std::move(top))), 15);
}

TEST(VMClosures, SharedUpvalue_TwoClosuresOneSlot)
{
    auto get_fn = make_chunk();
    get_fn->m_name = "get";
    get_fn->arity = 0;
    get_fn->upvalue_count = 1;
    get_fn->local_count = 1;
    get_fn->emit(Fa_make_ABC(Fa_OpCode::GET_UPVALUE, 0, 0, 0), { });
    get_fn->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });

    auto set_fn = make_chunk();
    set_fn->m_name = "set";
    set_fn->arity = 1;
    set_fn->upvalue_count = 1;
    set_fn->local_count = 1;
    set_fn->emit(Fa_make_ABC(Fa_OpCode::SET_UPVALUE, 0, 0, 0), { });
    set_fn->emit(Fa_make_ABC(Fa_OpCode::RETURN_NIL, 0, 0, 0), { });

    auto mp = make_chunk();
    mp->m_name = "mp";
    mp->arity = 0;
    mp->local_count = 4;
    mp->upvalue_count = 0;
    mp->functions.push(get_fn);
    mp->functions.push(set_fn);
    mp->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 0, BX(0)), { });
    mp->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 1, 0), { });
    mp->emit(Fa_make_ABC(Fa_OpCode::MOVE, 1, 0, 0), { });
    mp->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 2, 1), { });
    mp->emit(Fa_make_ABC(Fa_OpCode::MOVE, 1, 0, 0), { });
    mp->emit(Fa_make_ABC(Fa_OpCode::LIST_NEW, 3, 2, 0), { });
    mp->emit(Fa_make_ABC(Fa_OpCode::LIST_APPEND, 3, 1, 0), { });
    mp->emit(Fa_make_ABC(Fa_OpCode::LIST_APPEND, 3, 2, 0), { });
    mp->emit(Fa_make_ABC(Fa_OpCode::CLOSE_UPVALUE, 0, 0, 0), { });
    mp->emit(Fa_make_ABC(Fa_OpCode::RETURN, 3, 1, 0), { });

    auto top = make_chunk();
    top->m_name = "<test>";
    top->local_count = 5;
    top->functions.push(mp);
    top->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 0, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 0, 0), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 2, BX(1)), { });
    top->emit(Fa_make_ABC(Fa_OpCode::LIST_GET, 1, 0, 2), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 2, BX(99)), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 1, 1, 0), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 2, BX(0)), { });
    top->emit(Fa_make_ABC(Fa_OpCode::LIST_GET, 1, 0, 2), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 1, 0, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::RETURN, 1, 1, 0), { });
    top->disassemble();

    VMRunner r;
    EXPECT_EQ(Fa_AS_INTEGER(r.run(std::move(top))), 99);
}

TEST(VMICProfile, BinaryOpUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 3).load_int(1, 4).ABC(Fa_OpCode::OP_ADD, 2, 0, 1).nop(0).ret(2);
    r.run(b);
    b.dump();
    auto const& s = r.chunk_->ic_slots[0];
    EXPECT_TRUE(has_tag(Fa_TypeTag(s.seen_lhs), Fa_TypeTag::INT));
    EXPECT_TRUE(has_tag(Fa_TypeTag(s.seen_rhs), Fa_TypeTag::INT));
    EXPECT_TRUE(has_tag(Fa_TypeTag(s.seen_ret), Fa_TypeTag::INT));
    EXPECT_GE(s.hit_count, 1u);
}

TEST(VMICProfile, SubUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 3).ABC(Fa_OpCode::OP_SUB, 2, 0, 1).nop(0).ret(2);
    b.dump();
    r.run(b);
    EXPECT_GE(r.chunk_->ic_slots[0].hit_count, 1u);
}

TEST(VMICProfile, ICCallUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3)
        .slot()
        .ABC(Fa_OpCode::LIST_NEW, 0, 2, 0)
        .load_int(2, 1)
        .ABC(Fa_OpCode::LIST_APPEND, 0, 2, 0)
        .load_int(2, 2)
        .ABC(Fa_OpCode::LIST_APPEND, 0, 2, 0)
        .ldg(1, "len")
        .mov(2, 0)
        .ABC(Fa_OpCode::IC_CALL, 1, 1, 0)
        .ret(1);
    b.dump();
    r.run(b);
    auto const& s = r.chunk_->ic_slots[0];
    EXPECT_TRUE(has_tag(Fa_TypeTag(s.seen_lhs), Fa_TypeTag::NATIVE));
    EXPECT_TRUE(has_tag(Fa_TypeTag(s.seen_ret), Fa_TypeTag::INT));
    EXPECT_GE(s.hit_count, 1u);
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
        .ABC(Fa_OpCode::OP_LT, 3, 0, 1)
        .AsBx(Fa_OpCode::JUMP_IF_FALSE, 3, 4)
        .load_int(4, 1)
        .ABC(Fa_OpCode::OP_ADD, 0, 0, 4)
        .nop(0)
        .AsBx(Fa_OpCode::LOOP, 0, -6)
        .ret(0);
    b.dump();
    r.run(b);
    EXPECT_EQ(r.chunk_->ic_slots[0].hit_count, 5u);
}

TEST(VMIntegration, Fibonacci_fib10_equals_55)
{
    AST::Fa_Stmt* fib = AST::Fa_makeFunction(
        AST::Fa_makeName("fib"),
        AST::Fa_makeList({ AST::Fa_makeName("n") }),
        AST::Fa_makeBlock({ AST::Fa_makeIf(
                                AST::Fa_makeBinary(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_LTE),
                                AST::Fa_makeBlock({ AST::Fa_makeReturn(AST::Fa_makeName("n")) })),
            AST::Fa_makeReturn(AST::Fa_makeBinary(
                AST::Fa_makeCall(AST::Fa_makeName("fib"), AST::Fa_makeList({ AST::Fa_makeBinary(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_SUB) })),
                AST::Fa_makeCall(AST::Fa_makeName("fib"), AST::Fa_makeList({ AST::Fa_makeBinary(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_SUB) })),
                AST::Fa_BinaryOp::OP_ADD)) }));

    Fa_Chunk* top = compile_program({ fib,
        AST::Fa_makeExprStmt(AST::Fa_makeCall(AST::Fa_makeName("fib"), AST::Fa_makeList({ AST::Fa_makeLiteralInt(10) }))) });
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(Fa_AS_INTEGER(r.run(top)), 55);
}

TEST(VMIntegration, SumForLoopOverList)
{
    AST::Fa_Stmt* sum = AST::Fa_makeFunction(
        AST::Fa_makeName("sum"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("items"), AST::Fa_makeList({ AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(2), AST::Fa_makeLiteralInt(3), AST::Fa_makeLiteralInt(4), AST::Fa_makeLiteralInt(5) }),
                true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("total"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeFor(AST::Fa_makeName("item"),
                AST::Fa_makeName("items"),
                AST::Fa_makeBlock({
                    AST::Fa_makeAssignmentStmt(
                        AST::Fa_makeName("total"),
                        AST::Fa_makeBinary(AST::Fa_makeName("total"), AST::Fa_makeName("item"), AST::Fa_BinaryOp::OP_ADD),
                        false),
                })),
            AST::Fa_makeReturn(AST::Fa_makeName("total")),
        }));

    Fa_Chunk* top = compile_calling(sum);
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(Fa_AS_INTEGER(r.run(top)), 15);
}

TEST(VMIntegration, StringConcat_3Parts)
{
    GTEST_SKIP() << "Compiler does not lower string concatenation yet.";
    AST::Fa_Stmt* test = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        AST::Fa_makeReturn(AST::Fa_makeBinary(
            AST::Fa_makeBinary(AST::Fa_makeLiteralString("hello"), AST::Fa_makeLiteralString(", "), AST::Fa_BinaryOp::OP_ADD),
            AST::Fa_makeLiteralString("world"),
            AST::Fa_BinaryOp::OP_ADD)));

    Fa_Chunk* top = compile_calling(test);
    top->disassemble();
    VMRunner r;
    Fa_Value v = r.run(top);
    EXPECT_EQ(Fa_AS_STRING(v)->str, "hello, world");
}

TEST(VMIntegration, EmptyForLoopLeavesStateUnchanged)
{
    AST::Fa_Stmt* first = AST::Fa_makeFunction(
        AST::Fa_makeName("first"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("items"), AST::Fa_makeList(), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("seen"), AST::Fa_makeLiteralInt(99), true),
            AST::Fa_makeFor(AST::Fa_makeName("item"),
                AST::Fa_makeName("items"),
                AST::Fa_makeBlock({
                    AST::Fa_makeAssignmentStmt(AST::Fa_makeName("seen"), AST::Fa_makeName("item"), false),
                })),
            AST::Fa_makeReturn(AST::Fa_makeName("seen")),
        }));

    Fa_Chunk* top = compile_calling(first);
    top->disassemble();
    VMRunner r;
    EXPECT_EQ(Fa_AS_INTEGER(r.run(top)), 99);
}

TEST(VMIntegration, NestedAdderClosure)
{
    auto add_fn = make_chunk();
    add_fn->m_name = "add";
    add_fn->arity = 1;
    add_fn->upvalue_count = 1;
    add_fn->local_count = 3;
    add_fn->emit(Fa_make_ABC(Fa_OpCode::GET_UPVALUE, 1, 0, 0), { });
    add_fn->emit(Fa_make_ABC(Fa_OpCode::OP_ADD, 2, 1, 0), { });
    add_fn->emit(Fa_make_ABC(Fa_OpCode::RETURN, 2, 1, 0), { });

    auto make_adder = make_chunk();
    make_adder->m_name = "ma";
    make_adder->arity = 1;
    make_adder->local_count = 2;
    make_adder->functions.push(add_fn);
    make_adder->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 1, 0), { });
    make_adder->emit(Fa_make_ABC(Fa_OpCode::MOVE, 1, 0, 0), { });
    make_adder->emit(Fa_make_ABC(Fa_OpCode::CLOSE_UPVALUE, 0, 0, 0), { });
    make_adder->emit(Fa_make_ABC(Fa_OpCode::RETURN, 1, 1, 0), { });

    auto top = make_chunk();
    top->m_name = "<test>";
    top->local_count = 3;
    top->functions.push(make_adder);
    top->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 0, 0), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 1, BX(5)), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 1, 0), { });
    top->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 1, BX(3)), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 1, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });
    top->disassemble();

    VMRunner r;
    EXPECT_EQ(Fa_AS_INTEGER(r.run(std::move(top))), 8);
}

TEST(NativeLen, NullArgv)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_len(1, nullptr)));
}

TEST(NativeLen, EmptyString)
{
    Fa_VM vm;
    auto s = Fa_MAKE_STRING("");
    EXPECT_EQ(Fa_AS_INTEGER(vm.Fa_len(1, &s)), 0);
}

TEST(NativeLen, NonEmptyString)
{
    Fa_VM vm;
    auto s = Fa_MAKE_STRING("hello");
    EXPECT_EQ(Fa_AS_INTEGER(vm.Fa_len(1, &s)), 5);
}

TEST(NativeLen, UnicodeString)
{
    Fa_VM vm;
    auto s = Fa_MAKE_STRING("abc");
    EXPECT_EQ(Fa_AS_INTEGER(vm.Fa_len(1, &s)), 3);
}

TEST(NativeLen, MultipleArgs_ReturnsNil)
{
    Fa_VM vm;
    auto s = Fa_MAKE_STRING("hi");
    Fa_Value m_args[] = { s, s };
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_len(2, m_args)));
}

TEST(NativeLen, IntegerArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(42);
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_len(1, &arg)));
}

TEST(NativeLen, BoolArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_BOOL(true);
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_len(1, &arg)));
}

TEST(NativeLen, NilArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = NIL_VAL;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_len(1, &arg)));
}

TEST(NativePrint, NoArgs_PrintsNewline)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_print(0, nullptr)));
}

TEST(NativePrint, StringArg)
{
    Fa_VM vm;
    auto s = Fa_MAKE_STRING("hello world");
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_print(1, &s)));
}

TEST(NativePrint, IntegerArg)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(42);
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_print(1, &arg)));
}

TEST(NativePrint, FloatArg)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(3.14);
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_print(1, &arg)));
}

TEST(NativePrint, BoolArg_True)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_BOOL(true);
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_print(1, &arg)));
}

TEST(NativePrint, BoolArg_False)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_BOOL(false);
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_print(1, &arg)));
}

TEST(NativePrint, NilArg)
{
    Fa_VM vm;
    Fa_Value arg = NIL_VAL;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_print(1, &arg)));
}

TEST(NativePrint, TwoArgs_DoesNotCrash)
{
    Fa_VM vm;
    auto s = Fa_MAKE_STRING("a");
    Fa_Value m_args[] = { s, s };
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_print(2, m_args)));
}

TEST(NativeStr, NoArgs_ReturnsEmpty)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_STRING(vm.Fa_str(0, nullptr)));
}

TEST(NativeStr, NullArgv_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_AS_STRING(vm.Fa_str(1, nullptr))->str.empty());
}

TEST(NativeStr, Integer)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(42);
    Fa_Value r = vm.Fa_str(1, &arg);
    ASSERT_TRUE(Fa_IS_STRING(r));
    EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "42");
}

TEST(NativeStr, NegativeInteger)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(-7);
    Fa_Value r = vm.Fa_str(1, &arg);
    ASSERT_TRUE(Fa_IS_STRING(r));
    EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "-7");
}

TEST(NativeStr, Zero)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(0);
    Fa_Value r = vm.Fa_str(1, &arg);
    ASSERT_TRUE(Fa_IS_STRING(r));
    EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "0");
}

TEST(NativeStr, BoolTrue)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_BOOL(true);
    Fa_Value r = vm.Fa_str(1, &arg);
    ASSERT_TRUE(Fa_IS_STRING(r));
    EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "true");
}

TEST(NativeStr, BoolFalse)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_BOOL(false);
    Fa_Value r = vm.Fa_str(1, &arg);
    ASSERT_TRUE(Fa_IS_STRING(r));
    EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "false");
}

TEST(NativeStr, Float)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(1.5);
    Fa_Value r = vm.Fa_str(1, &arg);
    ASSERT_TRUE(Fa_IS_STRING(r));
    char const* text_ptr = Fa_AS_STRING(r)->str.data();
    f64 parsed = 0.0;
    auto [end_ptr, ec] = std::from_chars(text_ptr, text_ptr + Fa_AS_STRING(r)->str.len(), parsed);
    ASSERT_EQ(ec, std::errc());
    ASSERT_EQ(end_ptr, text_ptr + Fa_AS_STRING(r)->str.len());
    EXPECT_DOUBLE_EQ(parsed, 1.5);
}

TEST(NativeStr, StringPassthrough)
{
    Fa_VM vm;
    Fa_Value s = Fa_MAKE_STRING("hello");
    Fa_Value r = vm.Fa_str(1, &s);
    ASSERT_TRUE(Fa_IS_STRING(r));
    EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "hello");
}

TEST(NativeStr, NilArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = NIL_VAL;
    EXPECT_NO_FATAL_FAILURE(vm.Fa_str(1, &arg));
}

TEST(NativeStr, TwoArgs_ReturnsNil)
{
    Fa_VM vm;
    auto s = Fa_MAKE_STRING("x");
    Fa_Value m_args[] = { s, s };
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_str(2, m_args)));
}

TEST(NativeBool, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_bool(0, nullptr)));
}

TEST(NativeBool, TrueBoolean)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_BOOL(true);
    Fa_Value r = vm.Fa_bool(1, &arg);
    ASSERT_TRUE(Fa_IS_BOOL(r));
    EXPECT_TRUE(Fa_AS_BOOL(r));
}

TEST(NativeBool, FalseBoolean)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_BOOL(false);
    Fa_Value r = vm.Fa_bool(1, &arg);
    ASSERT_TRUE(Fa_IS_BOOL(r));
    EXPECT_FALSE(Fa_AS_BOOL(r));
}

TEST(NativeBool, NonZeroInteger_IsTrue)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(1);
    Fa_Value r = vm.Fa_bool(1, &arg);
    ASSERT_TRUE(Fa_IS_BOOL(r));
    EXPECT_TRUE(Fa_AS_BOOL(r));
}

TEST(NativeBool, ZeroInteger_IsFalsy)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(0);
    Fa_Value r = vm.Fa_bool(1, &arg);
    ASSERT_TRUE(Fa_IS_BOOL(r));
    EXPECT_FALSE(Fa_AS_BOOL(r));
}

TEST(NativeBool, NilArg)
{
    Fa_VM vm;
    Fa_Value arg = NIL_VAL;
    Fa_Value r = vm.Fa_bool(1, &arg);
    ASSERT_TRUE(Fa_IS_BOOL(r));
    EXPECT_FALSE(Fa_AS_BOOL(r));
}

TEST(NativeBool, NonEmptyString_IsTrue)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_STRING("hi");
    Fa_Value r = vm.Fa_bool(1, &arg);
    ASSERT_TRUE(Fa_IS_BOOL(r));
    EXPECT_TRUE(Fa_AS_BOOL(r));
}

TEST(NativeInt, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_int(0, nullptr)));
}

TEST(NativeInt, IntegerPassthrough)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(7);
    Fa_Value r = vm.Fa_int(1, &arg);
    ASSERT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), 7);
}

TEST(NativeInt, FloatTruncates)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(3.9);
    Fa_Value r = vm.Fa_int(1, &arg);
    ASSERT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), 3);
}

TEST(NativeInt, NegativeFloat)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(-2.7);
    Fa_Value r = vm.Fa_int(1, &arg);
    ASSERT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), -2);
}

TEST(NativeInt, StringArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_STRING("42");
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_int(1, &arg)));
}

TEST(NativeInt, NilArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = NIL_VAL;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_int(1, &arg)));
}

TEST(NativeInt, BoolArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_BOOL(true);
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_int(1, &arg)));
}

TEST(NativeFloat, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_float(0, nullptr)));
}

TEST(NativeFloat, IntegerToFloat)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(3);
    Fa_Value r = vm.Fa_float(1, &arg);
    ASSERT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 3.0);
}

TEST(NativeFloat, FloatPassthrough)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(2.5);
    Fa_Value r = vm.Fa_float(1, &arg);
    ASSERT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 2.5);
}

TEST(NativeFloat, NegativeInteger)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(-10);
    Fa_Value r = vm.Fa_float(1, &arg);
    ASSERT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), -10.0);
}

TEST(NativeFloat, StringArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_STRING("3.14");
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_float(1, &arg)));
}

TEST(NativeFloat, NilArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = NIL_VAL;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_float(1, &arg)));
}

TEST(NativeType, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_type(0, nullptr)));
}

TEST(NativeType, ReturnsInteger)
{
    Fa_VM vm;
    Fa_Value i = Fa_MAKE_INTEGER(0);
    Fa_Value f = Fa_MAKE_REAL(0.0);
    Fa_Value b = Fa_MAKE_BOOL(false);
    Fa_Value n = NIL_VAL;
    Fa_Value s = Fa_MAKE_STRING("x");
    EXPECT_TRUE(Fa_IS_INTEGER(vm.Fa_type(1, &i)));
    EXPECT_TRUE(Fa_IS_INTEGER(vm.Fa_type(1, &f)));
    EXPECT_TRUE(Fa_IS_INTEGER(vm.Fa_type(1, &b)));
    EXPECT_TRUE(Fa_IS_INTEGER(vm.Fa_type(1, &n)));
    EXPECT_TRUE(Fa_IS_INTEGER(vm.Fa_type(1, &s)));
}

TEST(NativeType, DifferentTypesHaveDifferentTags)
{
    Fa_VM vm;
    Fa_Value i = Fa_MAKE_INTEGER(0);
    Fa_Value f = Fa_MAKE_REAL(0.0);
    Fa_Value b = Fa_MAKE_BOOL(false);
    Fa_Value n = NIL_VAL;
    Fa_Value s = Fa_MAKE_STRING("x");
    i64 int_tag = Fa_AS_INTEGER(vm.Fa_type(1, &i));
    i64 flt_tag = Fa_AS_INTEGER(vm.Fa_type(1, &f));
    i64 bool_tag = Fa_AS_INTEGER(vm.Fa_type(1, &b));
    i64 nil_tag = Fa_AS_INTEGER(vm.Fa_type(1, &n));
    i64 str_tag = Fa_AS_INTEGER(vm.Fa_type(1, &s));

    EXPECT_NE(int_tag, flt_tag);
    EXPECT_NE(int_tag, nil_tag);
    EXPECT_NE(str_tag, nil_tag);
    EXPECT_NE(bool_tag, nil_tag);
}

TEST(NativeAppend, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_append(0, nullptr)));
}

TEST(NativePop, NonListArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(5);
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_pop(1, &arg)));
}

TEST(NativePop, NullArgv_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_pop(1, nullptr)));
}

TEST(NativeSlice, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_slice(0, nullptr)));
}

TEST(NativeFloor, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_floor(0, nullptr)));
}

TEST(NativeFloor, IntegerPassthrough)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(5);
    Fa_Value r = vm.Fa_floor(1, &arg);
    EXPECT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), 5);
}

TEST(NativeFloor, PositiveFloat)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(3.7);
    Fa_Value r = vm.Fa_floor(1, &arg);
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 3.0);
}

TEST(NativeFloor, NegativeFloat)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(-2.3);
    Fa_Value r = vm.Fa_floor(1, &arg);
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), -3.0);
}

TEST(NativeFloor, ExactFloat)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(4.0);
    Fa_Value r = vm.Fa_floor(1, &arg);
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 4.0);
}

TEST(NativeFloor, StringArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_STRING("3.5");
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_floor(1, &arg)));
}

TEST(NativeFloor, NilArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = NIL_VAL;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_floor(1, &arg)));
}

TEST(NativeFloor, TwoArgs_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_REAL(1.5), Fa_MAKE_REAL(2.5) };
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_floor(2, m_args)));
}

TEST(NativeCeil, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_ceil(0, nullptr)));
}

TEST(NativeCeil, IntegerPassthrough)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(5);
    Fa_Value r = vm.Fa_ceil(1, &arg);
    EXPECT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), 5);
}

TEST(NativeCeil, PositiveFloat)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(3.2);
    Fa_Value r = vm.Fa_ceil(1, &arg);
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 4.0);
}

TEST(NativeCeil, NegativeFloat)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(-2.7);
    Fa_Value r = vm.Fa_ceil(1, &arg);
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), -2.0);
}

TEST(NativeCeil, ExactFloat)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(4.0);
    Fa_Value r = vm.Fa_ceil(1, &arg);
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 4.0);
}

TEST(NativeCeil, StringArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_STRING("3.5");
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_ceil(1, &arg)));
}

TEST(NativeCeil, NilArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = NIL_VAL;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_ceil(1, &arg)));
}

TEST(NativeAbs, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_abs(0, nullptr)));
}

TEST(NativeAbs, PositiveInteger)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(5);
    Fa_Value r = vm.Fa_abs(1, &arg);
    ASSERT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), 5);
}

TEST(NativeAbs, NegativeInteger)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(-5);
    Fa_Value r = vm.Fa_abs(1, &arg);
    ASSERT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), 5);
}

TEST(NativeAbs, ZeroInteger)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(0);
    Fa_Value r = vm.Fa_abs(1, &arg);
    ASSERT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), 0);
}

TEST(NativeAbs, PositiveFloat)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(3.5);
    Fa_Value r = vm.Fa_abs(1, &arg);
    ASSERT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 3.5);
}

TEST(NativeAbs, NegativeFloat)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(-3.5);
    Fa_Value r = vm.Fa_abs(1, &arg);
    ASSERT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 3.5);
}

TEST(NativeAbs, StringArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_STRING("-3");
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_abs(1, &arg)));
}

TEST(NativeAbs, NilArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value arg = NIL_VAL;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_abs(1, &arg)));
}

TEST(NativeMin, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_min(0, nullptr)));
}

TEST(NativeMin, OneArg_ReturnsArg)
{
    Fa_VM vm;
    srand(static_cast<unsigned>(time(nullptr)));
    Fa_Value n = Fa_MAKE_INTEGER(static_cast<i64>(rand()));
    Fa_Value result = vm.Fa_min(1, &n);
    EXPECT_EQ(Fa_AS_INTEGER(result), Fa_AS_INTEGER(n));
}

TEST(NativeMin, TwoPositiveIntegers)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_INTEGER(3), Fa_MAKE_INTEGER(7) };
    Fa_Value r = vm.Fa_min(2, m_args);
    EXPECT_EQ(Fa_AS_INTEGER(r), 3);
}

TEST(NativeMin, AllIntegersReturnsInteger)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_INTEGER(5), Fa_MAKE_INTEGER(2), Fa_MAKE_INTEGER(8) };
    Fa_Value r = vm.Fa_min(3, m_args);
    EXPECT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), 2);
}

TEST(NativeMin, MixedFloatAndInteger_ReturnsFloat)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_INTEGER(3), Fa_MAKE_REAL(1.5) };
    Fa_Value r = vm.Fa_min(2, m_args);
    EXPECT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 1.5);
}

TEST(NativeMin, NegativeValues)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_INTEGER(-1), Fa_MAKE_INTEGER(-5), Fa_MAKE_INTEGER(-2) };
    Fa_Value r = vm.Fa_min(3, m_args);
    EXPECT_EQ(Fa_AS_INTEGER(r), -5);
}

TEST(NativeMin, StringFirstArg_ReturnsNil)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING("a"), Fa_MAKE_STRING("b") };
    EXPECT_EQ(Fa_AS_STRING(vm.Fa_min(2, m_args))->str, Fa_MAKE_OBJ_STRING("a")->str);
}

TEST(NativeMax, NoArgs_ReturnsNil)
{
    Fa_VM vm;
    EXPECT_TRUE(Fa_IS_NIL(vm.Fa_max(0, nullptr)));
}

TEST(NativeMax, OneArg_ReturnsArg)
{
    Fa_VM vm;
    srand(static_cast<unsigned>(time(nullptr)));
    Fa_Value n = Fa_MAKE_INTEGER(static_cast<i64>(rand()));
    Fa_Value result = vm.Fa_max(1, &n);
    EXPECT_EQ(Fa_AS_INTEGER(result), Fa_AS_INTEGER(n));
}

TEST(NativeMax, TwoPositiveIntegers)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_INTEGER(3), Fa_MAKE_INTEGER(7) };
    Fa_Value r = vm.Fa_max(2, m_args);
    EXPECT_EQ(Fa_AS_INTEGER(r), 7);
}

TEST(NativeMax, NegativeValues)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_INTEGER(-3), Fa_MAKE_INTEGER(-1) };
    Fa_Value r = vm.Fa_max(2, m_args);
    EXPECT_EQ(Fa_AS_INTEGER(r), -1);
}

TEST(NativeMax, AllIntegersReturnsInteger)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_INTEGER(1), Fa_MAKE_INTEGER(9), Fa_MAKE_INTEGER(4) };
    Fa_Value r = vm.Fa_max(3, m_args);
    EXPECT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), 9);
}

TEST(NativeMax, MixedFloatAndInteger)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_INTEGER(3), Fa_MAKE_REAL(3.5) };
    Fa_Value r = vm.Fa_max(2, m_args);
    EXPECT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 3.5);
}

TEST(NativeMax, StringFirstArg_ReturnsArg)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING("a"), Fa_MAKE_STRING("b") };
    EXPECT_EQ(Fa_AS_STRING(vm.Fa_max(2, m_args))->str, Fa_MAKE_OBJ_STRING("b")->str);
}

TEST(NativeRound, HalfRoundsUp)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(2.5);
    Fa_Value r = vm.Fa_round(1, &arg);
    ASSERT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 3.0);
}

TEST(NativeRound, HalfNegativeRoundsDown)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(-2.5);
    Fa_Value r = vm.Fa_round(1, &arg);
    ASSERT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), -3.0);
}

TEST(NativeRound, IntegerPassthrough)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_INTEGER(4);
    Fa_Value r = vm.Fa_round(1, &arg);
    ASSERT_TRUE(Fa_IS_INTEGER(r));
    EXPECT_EQ(Fa_AS_INTEGER(r), 4);
}

TEST(NativePow, BasicSquare)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_REAL(3.0), Fa_MAKE_REAL(2.0) };
    Fa_Value r = vm.Fa_pow(2, m_args);
    if (!Fa_IS_NIL(r))
        EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 9.0);
}

TEST(NativePow, ZeroExponent)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_REAL(5.0), Fa_MAKE_REAL(0.0) };
    Fa_Value r = vm.Fa_pow(2, m_args);
    if (!Fa_IS_NIL(r))
        EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 1.0);
}

TEST(NativePow, NegativeExponent)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_REAL(2.0), Fa_MAKE_REAL(-1.0) };
    Fa_Value r = vm.Fa_pow(2, m_args);
    if (!Fa_IS_NIL(r))
        EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 0.5);
}

TEST(NativeSqrt, PerfectSquare)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(9.0);
    Fa_Value r = vm.Fa_sqrt(1, &arg);
    ASSERT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 3.0);
}

TEST(NativeSqrt, Zero)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(0.0);
    Fa_Value r = vm.Fa_sqrt(1, &arg);
    ASSERT_TRUE(Fa_IS_DOUBLE(r));
    EXPECT_DOUBLE_EQ(Fa_AS_DOUBLE(r), 0.0);
}

TEST(NativeSqrt, NegativeInput_SpecBehavior)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_REAL(-1.0);
    EXPECT_NO_FATAL_FAILURE(vm.Fa_sqrt(1, &arg));
}

TEST(NativeSplit, BasicSplit)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING("a,b,c"), Fa_MAKE_STRING(",") };
    Fa_Value r = vm.Fa_split(2, m_args);
    ASSERT_TRUE(Fa_IS_LIST(r));
    EXPECT_EQ(Fa_AS_LIST(r)->m_elements.size(), 3u);
    ASSERT_TRUE(Fa_IS_STRING(Fa_AS_LIST(r)->m_elements[0]));
    ASSERT_TRUE(Fa_IS_STRING(Fa_AS_LIST(r)->m_elements[1]));
    ASSERT_TRUE(Fa_IS_STRING(Fa_AS_LIST(r)->m_elements[2]));
    EXPECT_EQ(std::string(Fa_AS_STRING(Fa_AS_LIST(r)->m_elements[0])->str.data()), "a");
    EXPECT_EQ(std::string(Fa_AS_STRING(Fa_AS_LIST(r)->m_elements[1])->str.data()), "b");
    EXPECT_EQ(std::string(Fa_AS_STRING(Fa_AS_LIST(r)->m_elements[2])->str.data()), "c");
}

TEST(NativeSplit, NoDelimiterFound)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING("hello"), Fa_MAKE_STRING(",") };
    Fa_Value r = vm.Fa_split(2, m_args);
    ASSERT_TRUE(Fa_IS_LIST(r));
    EXPECT_EQ(Fa_AS_LIST(r)->m_elements.size(), 1u);
    ASSERT_TRUE(Fa_IS_STRING(Fa_AS_LIST(r)->m_elements[0]));
    EXPECT_EQ(std::string(Fa_AS_STRING(Fa_AS_LIST(r)->m_elements[0])->str.data()), "hello");
}

TEST(NativeSubstr, BasicSubstr)
{
    Fa_VM vm; // substr is exclusive
    Fa_Value m_args[] = { Fa_MAKE_STRING("hello"), Fa_MAKE_INTEGER(1), Fa_MAKE_INTEGER(4) };
    Fa_Value r = vm.Fa_substr(3, m_args);
    if (!Fa_IS_NIL(r)) {
        ASSERT_TRUE(Fa_IS_STRING(r));
        EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "ell");
    }
}

TEST(NativeSubstr, FromStart)
{
    Fa_VM vm; // substr is exclusive
    Fa_Value m_args[] = { Fa_MAKE_STRING("hello"), Fa_MAKE_INTEGER(0), Fa_MAKE_INTEGER(3) };
    Fa_Value r = vm.Fa_substr(3, m_args);
    if (!Fa_IS_NIL(r)) {
        ASSERT_TRUE(Fa_IS_STRING(r));
        EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "hel");
    }
}

TEST(NativeContains, StringContains_True)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING("hello world"), Fa_MAKE_STRING("world") };
    Fa_Value r = vm.Fa_contains(2, m_args);
    ASSERT_TRUE(Fa_IS_BOOL(r));
    EXPECT_TRUE(Fa_AS_BOOL(r));
}

TEST(NativeContains, StringContains_False)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING("hello"), Fa_MAKE_STRING("xyz") };
    Fa_Value r = vm.Fa_contains(2, m_args);
    ASSERT_TRUE(Fa_IS_BOOL(r));
    EXPECT_FALSE(Fa_AS_BOOL(r));
}

TEST(NativeTrim, LeadingAndTrailingSpaces)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_STRING("  hello  ");
    Fa_Value r = vm.Fa_trim(1, &arg);
    ASSERT_TRUE(Fa_IS_STRING(r));
    EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "hello");
}

TEST(NativeTrim, NoSpaces)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_STRING("hello");
    Fa_Value r = vm.Fa_trim(1, &arg);
    ASSERT_TRUE(Fa_IS_STRING(r));
    EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "hello");
}

TEST(NativeTrim, OnlySpaces)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_STRING("   ");
    Fa_Value r = vm.Fa_trim(1, &arg);
    ASSERT_TRUE(Fa_IS_STRING(r));
    EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "");
}

TEST(NativeJoin, BasicJoin)
{
    Fa_VM vm;
    Fa_Value list = vm.Fa_list(0, nullptr);
    Fa_ObjList* l = Fa_AS_LIST(list);
    l->m_elements.push(Fa_MAKE_STRING("a"));
    l->m_elements.push(Fa_MAKE_STRING("b"));
    l->m_elements.push(Fa_MAKE_STRING("c"));
    Fa_Value m_args[] = { list, Fa_MAKE_STRING("|") };
    Fa_Value r = vm.Fa_join(2, m_args);
    ASSERT_TRUE(Fa_IS_STRING(r));
    EXPECT_EQ(std::string(Fa_AS_STRING(r)->str.data()), "a|b|c");
}

TEST(NativeAssert, TrueCondition_DoesNotCrash)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_BOOL(true);
    EXPECT_NO_FATAL_FAILURE(vm.Fa_assert(1, &arg));
}

TEST(NativeAssert, FalseCondition_DoesNotCrash)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_BOOL(false);
    EXPECT_NO_FATAL_FAILURE(vm.Fa_assert(1, &arg));
}

TEST(NativeClock, ReturnsNumber_WhenImplemented)
{
    Fa_VM vm;
    Fa_Value r = vm.Fa_clock(0, nullptr);
    if (!Fa_IS_NIL(r))
        EXPECT_TRUE(Fa_IS_INTEGER(r));
}

TEST(NativeTime, ReturnsNumber_WhenImplemented)
{
    Fa_VM vm;
    Fa_Value r = vm.Fa_time(0, nullptr);
    if (!Fa_IS_NIL(r))
        EXPECT_TRUE(Fa_IS_INTEGER(r));
}

static f64 microseconds_since(std::chrono::high_resolution_clock::time_point t0)
{
    using namespace std::chrono;
    return static_cast<f64>(
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
    char const* m_value = std::getenv("fairuz_ENABLE_STRESS_PERF");
    return m_value && m_value[0] == '1';
}

static void require_stress_perf()
{
    if (!stress_perf_enabled())
        GTEST_SKIP() << "Set fairuz_ENABLE_STRESS_PERF=1 to run large Fa_VM stress tests.";
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

    AST::Fa_Stmt* test = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("step"), AST::Fa_makeLiteralInt(1), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("limit"), AST::Fa_makeLiteralInt(N), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(
                    AST::Fa_makeName("i"),
                    AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("step"), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("i")) }));

    Fa_Chunk* top = compile_calling(test);
    top->disassemble();
    Fa_VM vm;
    // Warm up — lets the IC quicken the ADD opcode.
    vm.run(top);

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(Fa_AS_INTEGER(result), N);
    std::printf("  Dispatch IntAdd loop %dk iters:    %.1f µs  (%.2f ns/op)\n",
        N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Dispatch throughput — float arithmetic loop
//    Same structure, but f64 accumulator — exercises the FF fast path.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfTest, Dispatch_FloatAdd_500k_Iterations)
{
    constexpr int N = 500'000;

    AST::Fa_Stmt* test = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralFloat(0.0), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("step"), AST::Fa_makeLiteralFloat(1.0), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("limit"), AST::Fa_makeLiteralFloat(static_cast<f64>(N)), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(
                    AST::Fa_makeName("i"),
                    AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("step"), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("i")) }));

    Fa_Chunk* top = compile_calling(test);
    top->disassemble();
    Fa_VM vm;
    vm.run(top);

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_TRUE(Fa_IS_DOUBLE(result));
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

    AST::Fa_Stmt* test = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("step"), AST::Fa_makeLiteralInt(1), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("limit"), AST::Fa_makeLiteralInt(N), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(
                    AST::Fa_makeName("i"),
                    AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("step"), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("i")) }));

    Fa_Chunk* top = compile_calling(test);
    top->disassemble();
    Fa_VM vm_cold, vm_warm;

    // Cold — no prior run.
    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value cold_result = vm_cold.run(top);
    f64 cold_us = microseconds_since(t0);

    // Warm — opcodes already quickened by the cold run above.
    t0 = std::chrono::high_resolution_clock::now();
    Fa_Value warm_result = vm_warm.run(top);
    f64 warm_us = microseconds_since(t0);

    do_not_optimize(cold_result);
    do_not_optimize(warm_result);
    EXPECT_EQ(Fa_AS_INTEGER(cold_result), N);
    EXPECT_EQ(Fa_AS_INTEGER(warm_result), N);

    f64 ratio = cold_us / warm_us;
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
    b.ABC(Fa_OpCode::OP_LT, 1, 0, 2);           // reuse r1 as cmp result
    b.AsBx(Fa_OpCode::JUMP_IF_FALSE, 1, 4);
    b.stg(0, "counter");
    b.ldg(0, "counter");
    b.load_int(1, 1);
    b.ABC(Fa_OpCode::OP_ADD, 0, 0, 1);
    b.AsBx(Fa_OpCode::LOOP, 0, -7);
    b.ret(0);

    b.dump();
    Fa_VM vm;

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(b.ch);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    ::printf("  Global store+load %dk roundtrips:  %.1f µs  (%.2f ns/op)\n", N / 1000, us, us * 1000.0 / N);
}
*/

// ─────────────────────────────────────────────────────────────────────────────
// 5. Function call overhead
//    Calls a trivial two-arg add closure N times from a loop.
//    Measures: CALL setup, frame push/pop, RETURN teardown.
// ─────────────────────────────────────────────────────────────────────────────

static Fa_Chunk* make_add2_chunk()
{
    auto* fn = make_chunk();
    fn->m_name = "add2";
    fn->arity = 2;
    fn->local_count = 3;
    fn->emit(Fa_make_ABC(Fa_OpCode::OP_ADD, 2, 0, 1), { });
    fn->emit(Fa_make_ABC(Fa_OpCode::RETURN, 2, 1, 0), { });
    return fn;
}

TEST(VMPerfTest, CallOverhead_100k_Calls)
{
    constexpr int N = 100;

    AST::Fa_Stmt* m_body = AST::Fa_makeBlock(
        { AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0)),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("limit"), AST::Fa_makeLiteralInt(N)),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeCall(AST::Fa_makeName("add"), AST::Fa_makeList({ AST::Fa_makeLiteralInt(1), AST::Fa_makeLiteralInt(2) }))),
                    AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("i")) });

    AST::Fa_Stmt* func = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        m_body);

    AST::Fa_Stmt* add = AST::Fa_makeFunction(
        AST::Fa_makeName("add"),
        AST::Fa_makeList({ AST::Fa_makeName("a"), AST::Fa_makeName("b") }),
        AST::Fa_makeReturn(AST::Fa_makeBinary(AST::Fa_makeName("a"), AST::Fa_makeName("b"), AST::Fa_BinaryOp::OP_ADD)));
    AST::Fa_Stmt* call = AST::Fa_makeExprStmt(AST::Fa_makeCall(AST::Fa_makeName("test"), AST::Fa_makeList()));

    std::cout << "AUTO:" << '\n';
    Fa_Chunk* top_ = Compiler().compile({ add, func, call });
    top_->disassemble();
    Fa_VM vm;
    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top_);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(Fa_AS_INTEGER(result), N);
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

    AST::Fa_Stmt* tc_fn = AST::Fa_makeFunction(
        AST::Fa_makeName("tc"),
        AST::Fa_makeList({ AST::Fa_makeName("n") }),
        AST::Fa_makeBlock({ AST::Fa_makeIf(
                                AST::Fa_makeBinary(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_EQ),
                                AST::Fa_makeBlock({ AST::Fa_makeReturn(AST::Fa_makeName("n")) })),
            AST::Fa_makeReturn(AST::Fa_makeCall(
                AST::Fa_makeName("tc"),
                AST::Fa_makeList({ AST::Fa_makeBinary(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_SUB) }))) }));

    Fa_Chunk* tc_top = compile_program({ tc_fn,
        AST::Fa_makeExprStmt(AST::Fa_makeCall(AST::Fa_makeName("tc"), AST::Fa_makeList({ AST::Fa_makeLiteralInt(DEPTH) }))) });

    tc_top->disassemble();
    Fa_VM vm_tc;
    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value tc_result = vm_tc.run(tc_top);
    f64 tc_us = microseconds_since(t0);

    // ── equivalent iterative loop (no calls) ─────────────────────────────
    AST::Fa_Stmt* loop_fn = AST::Fa_makeFunction(
        AST::Fa_makeName("loop"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(DEPTH), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(0), AST::Fa_BinaryOp::OP_NEQ),
                AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(
                    AST::Fa_makeName("n"),
                    AST::Fa_makeBinary(AST::Fa_makeName("n"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_SUB)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("n")) }));

    Fa_Chunk* loop_top = compile_calling(loop_fn);
    loop_top->disassemble();
    Fa_VM vm_loop;
    t0 = std::chrono::high_resolution_clock::now();
    Fa_Value loop_result = vm_loop.run(loop_top);
    f64 loop_us = microseconds_since(t0);

    do_not_optimize(tc_result);
    do_not_optimize(loop_result);
    EXPECT_EQ(Fa_AS_INTEGER(tc_result), 0);
    EXPECT_EQ(Fa_AS_INTEGER(loop_result), 0);

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
    auto* inner = make_chunk();
    inner->m_name = "inner";
    inner->arity = 0;
    inner->upvalue_count = 1;
    inner->local_count = 4;

    inner->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 1, BX(0)), { }); // r1 = 0 (i)
    inner->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 2, BX(1)), { }); // r2 = 1
    inner->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 3, BX(N)), { }); // r3 = N
    // loop: GET_UPVALUE r0, 0; r0 += r2; SET_UPVALUE; r1 += 1; cmp; loop
    inner->emit(Fa_make_ABC(Fa_OpCode::GET_UPVALUE, 0, 0, 0), { }); // r0 = upval[0]
    inner->emit(Fa_make_ABC(Fa_OpCode::OP_ADD, 0, 0, 2), { });      // r0 += 1
    inner->emit(Fa_make_ABC(Fa_OpCode::SET_UPVALUE, 0, 0, 0), { }); // upval[0] = r0
    inner->emit(Fa_make_ABC(Fa_OpCode::OP_ADD, 1, 1, 2), { });      // r1 += 1
    inner->emit(Fa_make_ABC(Fa_OpCode::OP_LT, 0, 1, 3), { });       // r0 = i < N
    inner->emit(Fa_make_AsBx(Fa_OpCode::JUMP_IF_FALSE, 0, 1), { }); // exit loop
    inner->emit(Fa_make_AsBx(Fa_OpCode::LOOP, 0, -7), { });         // back
    inner->emit(Fa_make_ABC(Fa_OpCode::GET_UPVALUE, 0, 0, 0), { }); // return upval
    inner->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });

    auto* outer = make_chunk();
    outer->m_name = "outer";
    outer->arity = 0;
    outer->local_count = 2;
    outer->functions.push(inner);
    outer->emit(Fa_make_ABx(Fa_OpCode::LOAD_INT, 0, BX(0)), { }); // r0 = 0 (upval seed)
    outer->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 1, 0), { });      // r1 = inner closure
    outer->emit(Fa_make_ABC(Fa_OpCode::MOVE, 1, 0, 0), { });      // capture r0 as upvalue
    outer->emit(Fa_make_ABC(Fa_OpCode::CALL, 1, 0, 0), { });      // call inner()
    outer->emit(Fa_make_ABC(Fa_OpCode::RETURN, 1, 1, 0), { });

    auto* top = make_chunk();
    top->m_name = "<upval_perf>";
    top->local_count = 2;
    top->functions.push(outer);
    top->emit(Fa_make_ABx(Fa_OpCode::CLOSURE, 0, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::CALL, 0, 0, 0), { });
    top->emit(Fa_make_ABC(Fa_OpCode::RETURN, 0, 1, 0), { });

    top->disassemble();
    Fa_VM vm;

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);

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
    AST::Fa_Stmt* test = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("xs"), AST::Fa_makeList(), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("limit"), AST::Fa_makeLiteralInt(N), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeCall(AST::Fa_makeName("اضف"), AST::Fa_makeList({ AST::Fa_makeName("xs"), AST::Fa_makeName("i") }))),
                    AST::Fa_makeAssignmentStmt(
                        AST::Fa_makeName("i"),
                        AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0)),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("sum"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(
                                        AST::Fa_makeName("sum"),
                                        AST::Fa_makeBinary(
                                            AST::Fa_makeName("sum"),
                                            AST::Fa_makeIndex(AST::Fa_makeName("xs"), AST::Fa_makeName("i")),
                                            AST::Fa_BinaryOp::OP_ADD)),
                    AST::Fa_makeAssignmentStmt(
                        AST::Fa_makeName("i"),
                        AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("sum")) }));

    Fa_Chunk* top = compile_calling(test);
    top->disassemble();
    Fa_VM vm;
    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);
    do_not_optimize(result);

    constexpr i64 expected = static_cast<i64>(N) * (N - 1) / 2;
    EXPECT_EQ(Fa_AS_INTEGER(result), expected);
    std::printf("  List append+sum N=%d:               %.1f µs\n", N, us);
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. Native function call throughput — Fa_len on a string, hot loop
//    IC_CALL path: first call warms the inline cache, subsequent calls hit it.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfTest, NativeCall_Len_50k_ICHot)
{
    constexpr int N = 50'000;

    AST::Fa_Stmt* test = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("s"), AST::Fa_makeLiteralString("hello world"), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("limit"), AST::Fa_makeLiteralInt(N), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("last"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("last"), AST::Fa_makeCall(AST::Fa_makeName("len"), AST::Fa_makeList({ AST::Fa_makeName("s") }))),
                    AST::Fa_makeAssignmentStmt(
                        AST::Fa_makeName("i"),
                        AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("last")) }));

    Fa_Chunk* top = compile_calling(test);
    top->disassemble();
    Fa_VM vm;

    // Cold run to prime the IC.
    vm.run(top);

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(Fa_AS_INTEGER(result), 11);
    std::printf("  NativeCall len() IC hot %dk:        %.1f µs  (%.2f ns/call)\n",
        N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 10. Mixed realistic workload — Fibonacci(25) repeated
//     Exercises: calls, branches, integer arithmetic, globals lookup.
//     fib(25) = 75025. We run it 100 times and report total + per-call time.
// ─────────────────────────────────────────────────────────────────────────────

static Fa_Chunk* make_fib_top(int n, int reps)
{
    AST::Fa_Stmt* fib = AST::Fa_makeFunction(
        AST::Fa_makeName("fib"),
        AST::Fa_makeList({ AST::Fa_makeName("x") }),
        AST::Fa_makeBlock({ AST::Fa_makeIf(
                                AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_LTE),
                                AST::Fa_makeBlock({ AST::Fa_makeReturn(AST::Fa_makeName("x")) })),
            AST::Fa_makeReturn(AST::Fa_makeBinary(
                AST::Fa_makeCall(AST::Fa_makeName("fib"), AST::Fa_makeList({ AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_SUB) })),
                AST::Fa_makeCall(AST::Fa_makeName("fib"), AST::Fa_makeList({ AST::Fa_makeBinary(AST::Fa_makeName("x"), AST::Fa_makeLiteralInt(2), AST::Fa_BinaryOp::OP_SUB) })),
                AST::Fa_BinaryOp::OP_ADD)) }));

    AST::Fa_Stmt* test = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("limit"), AST::Fa_makeLiteralInt(reps), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("result"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("result"), AST::Fa_makeCall(AST::Fa_makeName("fib"), AST::Fa_makeList({ AST::Fa_makeLiteralInt(n) }))),
                    AST::Fa_makeAssignmentStmt(
                        AST::Fa_makeName("i"),
                        AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("result")) }));

    Fa_Chunk* top = compile_program({ fib,
        test,
        AST::Fa_makeExprStmt(AST::Fa_makeCall(AST::Fa_makeName("test"), AST::Fa_makeList())) });
    top->disassemble();
    return top;
}

TEST(VMPerfTest, Fib20_100reps)
{
    constexpr int FIB_N = 20;
    constexpr int REPS = 100;

    Fa_VM vm;
    auto* top = make_fib_top(FIB_N, REPS);

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(Fa_AS_INTEGER(result), 6765); // fib(20)
    ::printf("  fib(%d) × %d reps:                  %.1f µs  (%.1f µs/call)\n", FIB_N, REPS, us, us / REPS);
}

TEST(VMPerfTest, Fib25_10reps)
{
    constexpr int FIB_N = 25;
    constexpr int REPS = 10;

    Fa_VM vm;
    auto* top = make_fib_top(FIB_N, REPS);

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(Fa_AS_INTEGER(result), 75025); // fib(25)
    ::printf("  fib(%d) × %d reps:                   %.1f µs  (%.1f µs/call)\n", FIB_N, REPS, us, us / REPS);
}

// ─────────────────────────────────────────────────────────────────────────────
// 11. Large stress tests (opt-in)
//     These are intentionally much larger than the regular perf tests and are
//     gated behind fairuz_ENABLE_STRESS_PERF=1 so they do not dominate normal
//     test runs.
// ─────────────────────────────────────────────────────────────────────────────

TEST(VMPerfStressTest, Dispatch_IntAdd_10M_Iterations)
{
    if (!stress_perf_enabled())
        GTEST_SKIP() << "Set fairuz_ENABLE_STRESS_PERF=1 to run large Fa_VM stress tests.";

    constexpr int N = 10'000'000;

    AST::Fa_Stmt* test = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("step"), AST::Fa_makeLiteralInt(1), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("limit"), AST::Fa_makeLiteralInt(N), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(
                    AST::Fa_makeName("i"),
                    AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("step"), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("i")) }));

    Fa_Chunk* top = compile_calling(test);
    Fa_VM vm;
    vm.run(top); // warm quickened arithmetic path

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(Fa_AS_INTEGER(result), N);
    std::printf("  STRESS IntAdd loop %dM iters:       %.1f µs  (%.2f ns/op)\n",
        N / 1'000'000, us, us * 1000.0 / N);
}

TEST(VMPerfStressTest, NativeCall_Len_1M_ICHot)
{
    if (!stress_perf_enabled())
        GTEST_SKIP() << "Set fairuz_ENABLE_STRESS_PERF=1 to run large Fa_VM stress tests.";

    constexpr int N = 1'000'000;

    AST::Fa_Stmt* test = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("s"), AST::Fa_makeLiteralString("hello world"), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("limit"), AST::Fa_makeLiteralInt(N), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("last"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("last"), AST::Fa_makeCall(AST::Fa_makeName("len"), AST::Fa_makeList({ AST::Fa_makeName("s") }))),
                    AST::Fa_makeAssignmentStmt(
                        AST::Fa_makeName("i"),
                        AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("last")) }));

    Fa_Chunk* top = compile_calling(test);
    Fa_VM vm;
    vm.run(top); // warm IC_CALL -> CALL rewrite

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(Fa_AS_INTEGER(result), 11);
    std::printf("  STRESS native len() %dM calls:      %.1f µs  (%.2f ns/call)\n",
        N / 1'000'000, us, us * 1000.0 / N);
}

TEST(VMPerfStressTest, List_AppendAndSum_100k)
{
    if (!stress_perf_enabled())
        GTEST_SKIP() << "Set fairuz_ENABLE_STRESS_PERF=1 to run large Fa_VM stress tests.";

    constexpr int N = 100'000;

    AST::Fa_Stmt* test = AST::Fa_makeFunction(
        AST::Fa_makeName("test"),
        AST::Fa_makeList(),
        AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(AST::Fa_makeName("xs"), AST::Fa_makeList(), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("limit"), AST::Fa_makeLiteralInt(N), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeExprStmt(AST::Fa_makeCall(AST::Fa_makeName("اضف"), AST::Fa_makeList({ AST::Fa_makeName("xs"), AST::Fa_makeName("i") }))),
                    AST::Fa_makeAssignmentStmt(
                        AST::Fa_makeName("i"),
                        AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(0)),
            AST::Fa_makeAssignmentStmt(AST::Fa_makeName("sum"), AST::Fa_makeLiteralInt(0), true),
            AST::Fa_makeWhile(
                AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeName("limit"), AST::Fa_BinaryOp::OP_LT),
                AST::Fa_makeBlock({ AST::Fa_makeAssignmentStmt(
                                        AST::Fa_makeName("sum"),
                                        AST::Fa_makeBinary(
                                            AST::Fa_makeName("sum"),
                                            AST::Fa_makeIndex(AST::Fa_makeName("xs"), AST::Fa_makeName("i")),
                                            AST::Fa_BinaryOp::OP_ADD)),
                    AST::Fa_makeAssignmentStmt(
                        AST::Fa_makeName("i"),
                        AST::Fa_makeBinary(AST::Fa_makeName("i"), AST::Fa_makeLiteralInt(1), AST::Fa_BinaryOp::OP_ADD)) })),
            AST::Fa_makeReturn(AST::Fa_makeName("sum")) }));

    Fa_Chunk* top = compile_calling(test);
    Fa_VM vm;

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    constexpr i64 expected = static_cast<i64>(N) * (N - 1) / 2;
    do_not_optimize(result);
    EXPECT_EQ(Fa_AS_INTEGER(result), expected);
    std::printf("  STRESS list append+sum N=%d:        %.1f µs\n", N, us);
}

TEST(VMPerfStressTest, Fib28_20reps_Hot)
{
    require_stress_perf();

    constexpr int FIB_N = 28;
    constexpr int REPS = 20;

    Fa_VM vm;
    auto* top = make_fib_top(FIB_N, REPS);
    vm.run(top); // warm global lookup and call-site rewriting

    auto t0 = std::chrono::high_resolution_clock::now();
    Fa_Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(Fa_AS_INTEGER(result), 317811);
    std::printf("  STRESS fib(%d) × %d reps hot:       %.1f µs  (%.1f µs/call)\n",
        FIB_N, REPS, us, us / REPS);
}
