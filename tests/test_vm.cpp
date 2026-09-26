#include "../fairuz/fAST.hpp"
#include "../fairuz/fcompiler.hpp"
#include "../fairuz/fdiagnostic.hpp"
#include "../fairuz/fopcode.hpp"
#include "../fairuz/fvm.hpp"
#include "farray.hpp"
#include "fobject.hpp"
#include "test_common.h"
#include "test_config.h"

#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>

using namespace fairuz;
using namespace fairuz::runtime;

static constexpr char kClassInstanceName[] = "__class$instance";

namespace {

GarbageCollector gc;
static inline StringRef sp_method_name(int m)
{
    switch (m) {
    case ObjClass::INIT: return "بداية";
    case ObjClass::ADD: return "عملية+";
    case ObjClass::SUB: return "عملية-";
    case ObjClass::MUL: return "عملية*";
    case ObjClass::DIV: return "عملية/";
    case ObjClass::MOD: return "عملية%";
    case ObjClass::REPR: return "كتابة";
    default:
        return { };
    }
}

}

class VMPerfTest : public ::testing::Test {
protected:
    void SetUp() override { REQUIRE_PERF(); }

    void TearDown() override { }
};

static constexpr u16 BX(int v) { return static_cast<u16>(v + 32767); }

struct CB {
    Chunk* ch { nullptr };

    CB()
    {
        ch = make_chunk();
        ch->name = "<test>";
    }

    CB& ABC(OpCode op, u8 A, u8 B, u8 C, u32 ln = 1)
    {
        ch->emit(make_ABC(op, A, B, C), { ln, 0, 0 });
        return *this;
    }
    CB& ABx(OpCode op, u8 A, u16 Bx, u32 ln = 1)
    {
        ch->emit(make_ABx(op, A, Bx), { ln, 0, 0 });
        return *this;
    }
    CB& AsBx(OpCode op, u8 A, int s_bx, u32 ln = 1)
    {
        ch->emit(make_AsBx(op, A, s_bx), { ln, 0, 0 });
        return *this;
    }

    CB& load_int(u8 r, int v) { return ABx(OpCode::LOAD_INT, r, BX(v)); }
    CB& ret(u8 r, u8 n = 1) { return ABC(OpCode::RETURN, r, n, 0); }
    CB& ret_nil() { return ABC(OpCode::RETURN_NIL, 0, 0, 0); }
    CB& nop(u8 slot = 0) { return ABC(OpCode::NOP, slot, 0, 0); }
    CB& mov(u8 d, u8 s) { return ABC(OpCode::MOVE, d, s, 0); }

    u16 str(char const* s)
    {
        auto p = std::make_unique<ObjString>();
        p->str = s;
        strs_.emplace_back(std::move(p));
        return ch->add_constant(Value::from_obj(reinterpret_cast<ObjHeader*>(strs_.back().get())));
    }

    CB& ldg(u8 r, char const* name) { return ABx(OpCode::LOAD_GLOBAL, r, str(name)); }
    CB& stg(u8 r, char const* name) { return ABx(OpCode::STORE_GLOBAL, r, str(name)); }

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
            u16 k = ch->add_constant(Value::from_int(v));
            return ABx(OpCode::LOAD_CONST, r, k); // spill to constant pool
        }
    }

private:
    std::vector<std::unique_ptr<ObjString>> strs_;
};

struct VMRunner {
    VM vm;
    Chunk* chunk_ = make_chunk();

    Value run(CB& b)
    {
        diagnostic::reset();
        chunk_ = b.ch;
        return vm.run(chunk_);
    }
    Value run(Chunk* c)
    {
        diagnostic::reset();
        chunk_ = c;
        return vm.run(chunk_);
    }
};

namespace {

Chunk* compile_program(Array<AST::StmtPtr> stmts)
{
    diagnostic::reset();
    Chunk* chunk = Compiler().compile(stmts);
    if (diagnostic::has_errors()) {
        diagnostic::dump(); // or whatever prints pending diagnostics
    }
    // ASSERT_FALSE(diagnostic::has_errors());
    diagnostic::reset();
    return chunk;
}

Chunk* compile_calling(AST::StmtPtr fn)
{
    auto* name = as_function_def(fn)->name;
    return compile_program({ fn, expr_stmt(call_expr(ident(name->spelling))) });
}

} // namespace

TEST(VMLoads, Nil)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_NIL, 0, 0, 1).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_TRUE(r.run(b).is_nil());
}

TEST(VMLoads, NilFillsMultiple)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(OpCode::LOAD_NIL, 0, 0, 3).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_TRUE(r.run(b).is_nil());
}

TEST(VMLoads, True)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMLoads, False)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMLoads, IntPositive)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 42).ret(0);
    Value v = r.run(b);
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 42);
}

TEST(VMLoads, IntZero)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 0).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 0);
}

TEST(VMLoads, IntNegative)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, -100).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -100);
}

TEST(VMLoads, IntMaxEncodable)
{
    VMRunner r;
    CB b;
    b.regs(1).ABx(OpCode::LOAD_INT, 0, 65535).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 32768);
}

TEST(VMLoads, IntMinEncodable)
{
    VMRunner r;
    CB b;
    b.regs(1).ABx(OpCode::LOAD_INT, 0, 0).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -32767);
}

TEST(VMLoads, ConstDouble)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.ch->add_constant(Value::from_real(3.14));
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.14);
}

TEST(VMLoads, ConstString)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.str("hello");
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.as_string()->str, "hello");
}

TEST(VMLoads, ConstLargeInt)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.ch->add_constant(Value::from_int(1000000LL));
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 1000000LL);
}

TEST(VMLoads, ReturnNil)
{
    VMRunner r;
    CB b;
    b.regs(1).ret_nil();
    EXPECT_TRUE(r.run(b).is_nil());
}

TEST(VMMove, Copies)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 77).mov(1, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 77);
}

TEST(VMMove, SourceUnchanged)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 55).mov(1, 0).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_EQ(v.as_int(), 55);
}

TEST(VMArith, AddIntFastPath)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 32).ABC(OpCode::OP_ADD, 2, 0, 1).nop().ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 42);
}

TEST(VMArith, AddDoubles)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.ch->add_constant(Value::from_real(1.5));
    u16 k1 = b.ch->add_constant(Value::from_real(2.5));
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_ADD, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 4.0);
}

TEST(VMArith, AddStringsConcat)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.str("foo");
    u16 k1 = b.str("bar");
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_ADD, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.as_string()->str, "foobar");
}

TEST(VMArith, SubInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 100).load_int(1, 58).ABC(OpCode::OP_SUB, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 42);
}

TEST(VMArith, MulInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 7).ABC(OpCode::OP_MUL, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 42);
}

TEST(VMArith, DivDouble)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.ch->add_constant(Value::from_real(84.0));
    u16 k1 = b.ch->add_constant(Value::from_real(2.0));
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_DIV, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_DOUBLE_EQ(r.run(b).as_double(), 42.0);
}

TEST(VMArith, ModPositive)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 17).load_int(1, 5).ABC(OpCode::OP_MOD, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value result = r.run(b);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST(VMArith, Pow)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 2).load_int(1, 10).ABC(OpCode::OP_POW, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value result = r.run(b);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 1024);
}

TEST(VMArith, NegInt)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 7).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), -7);
}

TEST(VMArith, NegDouble)
{
    VMRunner r;
    CB b;
    b.regs(2);
    u16 k = b.ch->add_constant(Value::from_real(3.5));
    b.ABx(OpCode::LOAD_CONST, 0, k).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_DOUBLE_EQ(r.run(b).as_double(), -3.5);
}

TEST(VMArith, DivByZeroThrows)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.ch->add_constant(Value::from_real(1.0));
    u16 k1 = b.ch->add_constant(Value::from_real(0.0));
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_DIV, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMArith, NegOnStringThrows)
{
    VMRunner r;
    CB b;
    b.regs(2).ABx(OpCode::LOAD_CONST, 0, b.str("x")).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMBitwise, And)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b1010).ABC(OpCode::OP_BITAND, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 0b1010);
}

TEST(VMBitwise, Or)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1100).load_int(1, 0b0011).ABC(OpCode::OP_BITOR, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 0b1111);
}

TEST(VMBitwise, Xor)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b0101).ABC(OpCode::OP_BITXOR, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 0b1010);
}

TEST(VMBitwise, Not)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(OpCode::OP_BITNOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), ~i64(0));
}

TEST(VMBitwise, Shl)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1).ABC(OpCode::OP_LSHIFT, 1, 0, 8).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 256);
}

TEST(VMBitwise, Shr)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1024).ABC(OpCode::OP_RSHIFT, 1, 0, 3).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 128);
}

TEST(VMBitwise, ShrArithmetic_NegativeInput)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, -8).load_int(1, 1).ABC(OpCode::OP_RSHIFT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -4);
}

TEST(VMBitwise, AndOnDoubleThrows)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k = b.ch->add_constant(Value::from_real(1.0));
    b.ABx(OpCode::LOAD_CONST, 0, k).load_int(1, 1).ABC(OpCode::OP_BITAND, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMCompare, EqIntsTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, EqIntsFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, NeqTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_NEQ, 2, 0, 1).ret(2);
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LtTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 3).load_int(1, 7).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LtFalseEqual)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, LeTrue_Equal)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LeFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, EqSameString)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.str("hello");
    u16 k1 = b.str("hello");
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, NotFalseIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, NotTrueIsFalse)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, NotNilIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_NIL, 0, 0, 1).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, NotZeroIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LtStrings)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 ka = b.str("apple");
    u16 kb = b.str("banana");
    b.ABx(OpCode::LOAD_CONST, 0, ka).ABx(OpCode::LOAD_CONST, 1, kb).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = r.run(b);
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMGlobals, StoreAndLoad)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 123).stg(0, "g").load_int(0, 0).ldg(1, "g").ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 123);
}

TEST(VMGlobals, MissingGlobalIsNil)
{
    VMRunner r;
    CB b;
    b.regs(1).ldg(0, "nope").ret(0);
    /// NOTE: this should change when making custom error breaking mechanisme
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMGlobals, Overwrite)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1).stg(0, "x").load_int(0, 2).stg(0, "x").ldg(1, "x").ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 2);
}

TEST(VMLists, NewEmpty)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LIST_NEW, 0, 0, 0).ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 0);
}

TEST(VMLists, AppendAndLen)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LIST_NEW, 0, 3, 0);
    for (int v : { 10, 20, 30 })
        b.load_int(1, v).ABC(OpCode::LIST_APPEND, 0, 1, 0);
    b.ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 3);
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
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 77);
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
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 20);
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
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 99);
}

TEST(VMLists, OutOfBoundsThrows)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(OpCode::LIST_NEW, 0, 0, 0).load_int(1, 0).ABC(OpCode::LIST_GET, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMLists, LenOnNonListThrows)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 5).ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMDicts, IndexReturnsStoredValue)
{
    VMRunner r;

    Chunk* ch = compile_program(
        {
            func_def(
                ident("func"),
                { },
                blk({
                    decl_stmt("k", lit_str("lang")),
                    decl_stmt("dict", dict_expr({ })),
                    expr_stmt(assign_expr(index_expr(ident("dict"), ident("k")), lit_int(7))),
                    decl_stmt("x", index_expr(ident("dict"), ident("k"))),
                    return_stmt(ident("x")),
                })),
            expr_stmt(call_expr(ident("func"))),
        });
    if (test_config::dump_bytecode)
        ch->disassemble();

    Value result = r.run(ch);

    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 7);
}

TEST(VMDicts, SetUpdatesAndAppendsByKey)
{
    Chunk* ch = compile_program(
        {
            func_def(
                ident("func"),
                { },
                blk({
                    decl_stmt("dict", dict_expr({ })),
                    expr_stmt(assign_expr(index_expr(ident("dict"), lit_str("fk")), lit_int(1))),
                    expr_stmt(assign_expr(index_expr(ident("dict"), lit_str("sk")), lit_int(2))),
                    return_stmt(
                        list_expr({
                            index_expr(ident("dict"), lit_str("fk")),
                            index_expr(ident("dict"), lit_str("sk")),
                        })),
                })),
            expr_stmt(call_expr(ident("func"))),
        });

    if (ch != nullptr && test_config::dump_bytecode)
        ch->disassemble();

    VMRunner r;
    Value result = r.run(ch);
    ASSERT_TRUE(result.is_list());
    ObjList* ret_obj = result.as_list();
    ASSERT_EQ(ret_obj->size(), 2);
    EXPECT_EQ(ret_obj->elements[0].as_int(), 1);
    EXPECT_EQ(ret_obj->elements[1].as_int(), 2);
}

TEST(VMDicts, MissingKeyReturnsNil)
{
    VMRunner r;

    Chunk* ch = compile_program({
        decl_stmt("x", index_expr(dict_expr({ }), lit_str("missing"))),
    });

    if (ch != nullptr && test_config::dump_bytecode)
        ch->disassemble();

    EXPECT_TRUE(r.run(ch).is_nil());
}

static Chunk* make_adder_chunk()
{
    auto fn = make_chunk();
    fn->name = "add2";
    fn->arity = 2;
    fn->local_count = 3;
    // r0=a r1=b (params); r2=a+b
    fn->emit(make_ABC(OpCode::OP_ADD, 2, 0, 1), { });
    fn->emit(make_ABC(OpCode::RETURN, 2, 1, 0), { });
    return fn;
}

TEST(VMCalls, CallClosure_TwoArgs)
{
    auto top = make_chunk();
    top->name = "<test>";
    top->local_count = 4;
    top->functions.push(make_adder_chunk());

    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(3)), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 2, BX(4)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 2, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    if (test_config::dump_bytecode)
        top->disassemble();
    VMRunner r;
    EXPECT_EQ(r.run(top).as_int(), 7);
}

TEST(VMCalls, WrongArgcThrows)
{
    auto top = make_chunk();
    top->name = "<test>";
    top->local_count = 3;
    top->functions.push(make_adder_chunk());

    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(1)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 1, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    if (test_config::dump_bytecode)
        top->disassemble();
    VMRunner r;
    EXPECT_THROW(r.run(top), std::runtime_error);
}

TEST(VMCalls, CallNonFunctionThrows)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 5).ABC(OpCode::CALL, 0, 0, 0).ret(0);
    if (test_config::dump_bytecode)
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
        .ldg(1, "طول")
        .mov(2, 0)
        .ABC(OpCode::IC_CALL, 1, 1, 0)
        .ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(r.run(b).as_int(), 3);
}

TEST(VMCalls, TailCall_DoesNotOverflowFrames)
{
    auto fn = make_chunk();
    fn->name = "cd";
    fn->arity = 1;
    fn->local_count = 4;

    u16 nk = fn->add_constant(str("cd"));

    fn->emit(make_ABx(OpCode::LOAD_INT, 1, BX(0)), { });
    fn->emit(make_ABC(OpCode::OP_EQ, 1, 0, 1), { });
    fn->emit(make_AsBx(OpCode::JUMP_IF_FALSE, 1, 1), { });
    fn->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    fn->emit(make_ABx(OpCode::LOAD_INT, 1, BX(1)), { });
    fn->emit(make_ABC(OpCode::OP_SUB, 0, 0, 1), { });
    fn->emit(make_ABx(OpCode::LOAD_GLOBAL, 2, nk), { });
    fn->emit(make_ABC(OpCode::MOVE, 3, 0, 0), { });
    fn->emit(make_ABC(OpCode::CALL_TAIL, 2, 1, 0), { });

    auto top = make_chunk();
    top->name = "<test>";
    top->local_count = 3;
    top->functions.push(fn);

    u16 tk = top->add_constant(str("cd"));
    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::STORE_GLOBAL, 0, tk), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(300)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 1, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    if (test_config::dump_bytecode)
        top->disassemble();
    VMRunner r;
    Value v = r.run(std::move(top));
    EXPECT_EQ(v.as_int(), 0);
}

TEST(VMCalls, StackOverflowDetected)
{
    auto fn = func_def(ident("inf"), { }, blk({
                                              expr_stmt(call_expr(ident("inf"))),
                                              return_stmt(nil()),
                                          }));

    auto ch = compile_program({
        fn,
        expr_stmt(call_expr(ident("inf"))),
    });
    if (test_config::dump_bytecode && ch != nullptr)
        ch->disassemble();
    VMRunner r;
    EXPECT_THROW(r.run(ch), std::runtime_error);
}

TEST(VMGlobals, UndefinedGlobalRaisesRuntimeError)
{

    VMRunner r;
    CB b;
    b.regs(1).ldg(0, "missing").ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_THROW(r.run(b), RuntimeHalt);
    diagnostic::reset();
}

TEST(VMIntegration, FunctionLocalDeclarationShadowsGlobal)
{
    AST::StmtPtr make_local = func_def(
        ident("make_local"),
        { },
        blk(
            { expr_stmt(assign_expr(ident("x"), lit_int(2))),
                return_stmt(ident("x")) }));

    AST::StmtPtr read_global = func_def(
        ident("read_global"),
        { },
        blk({
            return_stmt(ident("x")),
        }));

    AST::StmtPtr main_fn = func_def(
        ident("main"),
        { },
        blk({
            return_stmt(
                list_expr(
                    { call_expr(ident("make_local")),
                        call_expr(ident("read_global")) })),
        }));

    Chunk* top = compile_program({
        expr_stmt(assign_expr(ident("x"), lit_int(1))),
        make_local,
        read_global,
        main_fn,
        expr_stmt(call_expr(ident("main"))),
    });

    if (test_config::dump_bytecode)
        top->disassemble();
    VMRunner r;
    Value v = r.run(top);
    ASSERT_TRUE(v.is_list());
    auto const& elems = v.as_list()->elements;
    ASSERT_EQ(elems.size(), 2u);
    EXPECT_EQ(elems[0].as_int(), 2);
    EXPECT_EQ(elems[1].as_int(), 1);
}

#if FA_USE_NANBOX
TEST(VMICProfile, BinaryOpUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 3).load_int(1, 4).ABC(OpCode::OP_ADD, 2, 0, 1).nop(0).ret(2);
    r.run(b);
    if (test_config::dump_bytecode)
        b.dump();
    auto const& s = r.chunk_->ic_slots[0];
    EXPECT_TRUE(has_tag(TypeTag(s.seen_lhs), TypeTag::INT));
    EXPECT_TRUE(has_tag(TypeTag(s.seen_rhs), TypeTag::INT));
    EXPECT_TRUE(has_tag(TypeTag(s.seen_ret), TypeTag::INT));
    EXPECT_GE(s.hit_count, 1u);
}
#endif // FA_USE_NANBOX

TEST(VMICProfile, SubUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 3).ABC(OpCode::OP_SUB, 2, 0, 1).nop(0).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    r.run(b);
    EXPECT_GE(r.chunk_->ic_slots[0].hit_count, 1u);
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
    if (test_config::dump_bytecode)
        b.dump();
    r.run(b);
    EXPECT_EQ(r.chunk_->ic_slots[0].hit_count, 5u);
}

TEST(VMIntegration, TopLevelWhileAssignmentUpdatesGlobal)
{
    AST::StmtPtr read_global = func_def(
        ident("read_global"),
        { },
        blk({
            return_stmt(ident("x")),
        }));

    Chunk* top = compile_program({
        expr_stmt(assign_expr(ident("x"), lit_int(0))),
        expr_stmt(assign_expr(ident("limit"), lit_int(3))),
        while_stmt(
            binary(ident("x"), ident("limit"), AST::Expr::Kind::OP_LT),
            blk({
                expr_stmt(assign_expr(ident("x"), binary(ident("x"), lit_int(1), AST::Expr::Kind::OP_ADD))),
            })),
        read_global,
        expr_stmt(call_expr(ident("read_global"))),
    });

    if (test_config::dump_bytecode)
        top->disassemble();

    VMRunner r;
    Value v = r.run(top);
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 3);
}

TEST(VMIntegration, Fibonacci_fib10_equals_55)
{
    AST::StmtPtr fib = func_def(
        ident("fib"),
        { ident("n") },
        blk(
            { if_stmt(
                  binary(ident("n"), lit_int(1), AST::Expr::Kind::OP_LTE),
                  blk({ return_stmt(ident("n")) }),
                  { }),
                return_stmt(
                    binary(
                        call_expr(
                            ident("fib"),
                            { binary(
                                ident("n"),
                                lit_int(1),
                                AST::Expr::Kind::OP_SUB) }),
                        call_expr(
                            ident("fib"),
                            { binary(
                                ident("n"),
                                lit_int(2),
                                AST::Expr::Kind::OP_SUB) }),
                        AST::Expr::Kind::OP_ADD)) }));

    Chunk* top = compile_program({ fib, expr_stmt(call_expr(ident("fib"), { lit_int(10) })) });
    if (test_config::dump_bytecode)
        top->disassemble();
    VMRunner r;
    EXPECT_EQ(r.run(top).as_int(), 55);
}

TEST(VMIntegration, SumForLoopOverList)
{
    AST::StmtPtr sum = func_def(
        ident("sum"),
        { },
        blk(
            {
                expr_stmt(assign_expr(
                    ident("items"),
                    list_expr(
                        { lit_int(1),
                            lit_int(2),
                            lit_int(3),
                            lit_int(4),
                            lit_int(5) }))),
                expr_stmt(assign_expr(ident("total"), lit_int(0))),
                for_stmt(ident("item"),
                    ident("items"),
                    blk({
                        expr_stmt(assign_expr(
                            ident("total"),
                            binary(ident("total"), ident("item"), AST::Expr::Kind::OP_ADD))),
                    })),
                return_stmt(ident("total")),
            }));

    Chunk* top = compile_calling(sum);
    if (test_config::dump_bytecode)
        top->disassemble();
    VMRunner r;
    EXPECT_EQ(r.run(top).as_int(), 15);
}

TEST(VMIntegration, StringConcat_3Parts)
{
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        return_stmt(
            binary(
                binary(lit_str("hello"), lit_str(", "), AST::Expr::Kind::OP_ADD),
                lit_str("world"),
                AST::Expr::Kind::OP_ADD)));

    Chunk* top = compile_calling(test);
    if (test_config::dump_bytecode)
        top->disassemble();
    VMRunner r;
    Value v = r.run(top);
    EXPECT_EQ(v.as_string()->str, "hello, world");
}

TEST(VMIntegration, EmptyForLoopLeavesStateUnchanged)
{
    AST::StmtPtr first = func_def(
        ident("first"),
        { },
        blk({
            expr_stmt(assign_expr(ident("items"), list_expr())),
            expr_stmt(assign_expr(ident("seen"), lit_int(99))),
            for_stmt(ident("item"),
                ident("items"),
                blk({ expr_stmt(assign_expr(ident("seen"), ident("item"))) })),
            return_stmt(ident("seen")),
        }));

    Chunk* top = compile_calling(first);
    if (test_config::dump_bytecode)
        top->disassemble();
    VMRunner r;
    EXPECT_EQ(r.run(top).as_int(), 99);
}

TEST(VMIntegration, BreakAndContinueWorkInLoops)
{
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            expr_stmt(assign_expr(ident("items"),
                list_expr({
                    lit_int(1),
                    lit_int(2),
                    lit_int(3),
                    lit_int(4),
                    lit_int(5),
                }))),
            expr_stmt(assign_expr(ident("total"), lit_int(0))),
            for_stmt(
                ident("item"),
                ident("items"),
                blk({
                    if_stmt(binary(ident("item"), lit_int(2), AST::Expr::Kind::OP_EQ), blk({ continue_stmt() })),
                    if_stmt(binary(ident("item"), lit_int(5), AST::Expr::Kind::OP_EQ), blk({ break_stmt() })),
                    expr_stmt(assign_expr(ident("total"), binary(ident("total"), ident("item"), AST::Expr::Kind::OP_ADD))),
                })),
            return_stmt(ident("total")),
        }));

    Chunk* top = compile_calling(test);
    if (test_config::dump_bytecode)
        top->disassemble();
    VMRunner r;
    EXPECT_EQ(r.run(top).as_int(), 8);
}

TEST(NativeLen, NullArgv)
{
    VM vm;
    EXPECT_TRUE(vm.len(1, nullptr).is_nil());
}

TEST(NativeLen, EmptyString)
{
    VM vm;
    auto s = str("");
    EXPECT_EQ(vm.len(1, &s).as_int(), 0);
}

TEST(NativeLen, NonEmptyString)
{
    VM vm;
    auto s = str("hello");
    EXPECT_EQ(vm.len(1, &s).as_int(), 5);
}

TEST(NativeLen, UnicodeString)
{
    VM vm;
    auto s = str("abc");
    EXPECT_EQ(vm.len(1, &s).as_int(), 3);
}

TEST(NativePrint, NoArgs_PrintsNewline)
{
    VM vm;
    EXPECT_TRUE(vm.print(0, nullptr).is_nil());
}

TEST(NativePrint, StringArg)
{
    VM vm;
    auto s = str("hello world");
    EXPECT_TRUE(vm.print(1, &s).is_nil());
}

TEST(NativePrint, IntegerArg)
{
    VM vm;
    Value arg = Value::from_int(42);
    EXPECT_TRUE(vm.print(1, &arg).is_nil());
}

TEST(NativePrint, FloatArg)
{
    VM vm;
    Value arg = Value::from_real(3.14);
    EXPECT_TRUE(vm.print(1, &arg).is_nil());
}

TEST(NativePrint, BoolArg_True)
{
    VM vm;
    Value arg = Value::from_bool(true);
    EXPECT_TRUE(vm.print(1, &arg).is_nil());
}

TEST(NativePrint, BoolArg_False)
{
    VM vm;
    Value arg = Value::from_bool(false);
    EXPECT_TRUE(vm.print(1, &arg).is_nil());
}

TEST(NativePrint, NilArg)
{
    VM vm;
    Value arg = Value::nil();
    EXPECT_TRUE(vm.print(1, &arg).is_nil());
}

TEST(NativePrint, TwoArgs_DoesNotCrash)
{
    VM vm;
    auto s = str("a");
    Value m_args[] = { s, s };
    EXPECT_TRUE(vm.print(2, m_args).is_nil());
}

TEST(NativeStr, NoArgs_ReturnsEmpty)
{
    VM vm;
    EXPECT_TRUE(vm.str(0, nullptr).is_string());
}

TEST(NativeStr, Integer)
{
    VM vm;
    Value arg = Value::from_int(42);
    Value r = vm.str(1, &arg);
    ASSERT_TRUE(r.is_string());
    EXPECT_EQ(std::string(r.as_string()->str.data()), "42");
}

TEST(NativeStr, NegativeInteger)
{
    VM vm;
    Value arg = Value::from_int(-7);
    Value r = vm.str(1, &arg);
    ASSERT_TRUE(r.is_string());
    EXPECT_EQ(std::string(r.as_string()->str.data()), "-7");
}

TEST(NativeStr, Zero)
{
    VM vm;
    Value arg = Value::from_int(0);
    Value r = vm.str(1, &arg);
    ASSERT_TRUE(r.is_string());
    EXPECT_EQ(std::string(r.as_string()->str.data()), "0");
}

TEST(NativeStr, BoolTrue)
{
    VM vm;
    Value arg = Value::from_bool(true);
    Value r = vm.str(1, &arg);
    ASSERT_TRUE(r.is_string());
    EXPECT_EQ(std::string(r.as_string()->str.data()), "صحيح");
}

TEST(NativeStr, BoolFalse)
{
    VM vm;
    Value arg = Value::from_bool(false);
    Value r = vm.str(1, &arg);
    ASSERT_TRUE(r.is_string());
    EXPECT_EQ(std::string(r.as_string()->str.data()), "خطا");
}

TEST(NativeStr, Float)
{
    VM vm;
    Value arg = Value::from_real(1.5);
    Value r = vm.str(1, &arg);
    ASSERT_TRUE(r.is_string());
    char const* text_ptr = r.as_string()->str.data();
    f64 parsed = 0.0;
    auto [end_ptr, ec] = std::from_chars(text_ptr, text_ptr + r.as_string()->str.len(), parsed);
    ASSERT_EQ(ec, std::errc());
    ASSERT_EQ(end_ptr, text_ptr + r.as_string()->str.len());
    EXPECT_DOUBLE_EQ(parsed, 1.5);
}

TEST(NativeStr, StringPassthrough)
{
    VM vm;
    Value s = str("hello");
    Value r = vm.str(1, &s);
    ASSERT_TRUE(r.is_string());
    EXPECT_EQ(std::string(r.as_string()->str.data()), "hello");
}

TEST(NativeBool, TrueBoolean)
{
    VM vm;
    Value arg = Value::from_bool(true);
    Value r = vm.Bool(1, &arg);
    ASSERT_TRUE(r.is_bool());
    EXPECT_TRUE(r.as_bool());
}

TEST(NativeBool, FalseBoolean)
{
    VM vm;
    Value arg = Value::from_bool(false);
    Value r = vm.Bool(1, &arg);
    ASSERT_TRUE(r.is_bool());
    EXPECT_FALSE(r.as_bool());
}

TEST(NativeBool, NonZeroInteger_IsTrue)
{
    VM vm;
    Value arg = Value::from_int(1);
    Value r = vm.Bool(1, &arg);
    ASSERT_TRUE(r.is_bool());
    EXPECT_TRUE(r.as_bool());
}

TEST(NativeBool, ZeroInteger_IsFalsy)
{
    VM vm;
    Value arg = Value::from_int(0);
    Value r = vm.Bool(1, &arg);
    ASSERT_TRUE(r.is_bool());
    EXPECT_FALSE(r.as_bool());
}

TEST(NativeBool, NilArg)
{
    VM vm;
    Value arg = Value::nil();
    Value r = vm.Bool(1, &arg);
    ASSERT_TRUE(r.is_bool());
    EXPECT_FALSE(r.as_bool());
}

TEST(NativeBool, NonEmptyString_IsTrue)
{
    VM vm;
    Value arg = str("hi");
    Value r = vm.Bool(1, &arg);
    ASSERT_TRUE(r.is_bool());
    EXPECT_TRUE(r.as_bool());
}

TEST(NativeInt, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(7);
    Value r = vm.Int(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 7);
}

TEST(NativeInt, FloatTruncates)
{
    VM vm;
    Value arg = Value::from_real(3.9);
    Value r = vm.Int(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 3);
}

TEST(NativeInt, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-2.7);
    Value r = vm.Int(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), -2);
}

TEST(NativeFloat, IntegerToFloat)
{
    VM vm;
    Value arg = Value::from_int(3);
    Value r = vm.Float(1, &arg);
    ASSERT_TRUE(r.is_double());
    EXPECT_DOUBLE_EQ(r.as_double(), 3.0);
}

TEST(NativeFloat, FloatPassthrough)
{
    VM vm;
    Value arg = Value::from_real(2.5);
    Value r = vm.Float(1, &arg);
    ASSERT_TRUE(r.is_double());
    EXPECT_DOUBLE_EQ(r.as_double(), 2.5);
}

TEST(NativeFloat, NegativeInteger)
{
    VM vm;
    Value arg = Value::from_int(-10);
    Value r = vm.Float(1, &arg);
    ASSERT_TRUE(r.is_double());
    EXPECT_DOUBLE_EQ(r.as_double(), -10.0);
}

TEST(NativeType, ReturnsInteger)
{
    VM vm;
    Value i = Value::from_int(0);
    Value f = Value::from_real(0.0);
    Value b = Value::from_bool(false);
    Value n = Value::nil();
    Value s = str("x");

    EXPECT_EQ(vm.type(1, &i).as_string()->str, "طبيعي");
    EXPECT_EQ(vm.type(1, &f).as_string()->str, "حقيقي");
    EXPECT_EQ(vm.type(1, &b).as_string()->str, "منطقي");
    EXPECT_EQ(vm.type(1, &n).as_string()->str, "عدم");
    EXPECT_EQ(vm.type(1, &s).as_string()->str, "سلسلة");
}

TEST(NativeType, DifferentTypesHaveDifferentTags)
{
    VM vm;
    Value i = Value::from_int(0);
    Value f = Value::from_real(0.0);
    Value b = Value::from_bool(false);
    Value n = Value::nil();
    Value s = str("x");
    i64 int_tag = vm.type(1, &i).as_int();
    i64 flt_tag = vm.type(1, &f).as_int();
    i64 bool_tag = vm.type(1, &b).as_int();
    i64 nil_tag = vm.type(1, &n).as_int();
    i64 str_tag = vm.type(1, &s).as_int();

    EXPECT_NE(int_tag, flt_tag);
    EXPECT_NE(int_tag, nil_tag);
    EXPECT_NE(str_tag, nil_tag);
    EXPECT_NE(bool_tag, nil_tag);
}

TEST(NativeFloor, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(5);
    Value r = vm.floor(1, &arg);
    EXPECT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 5);
}

TEST(NativeFloor, PositiveFloat)
{
    VM vm;
    Value arg = Value::from_real(3.7);
    Value r = vm.floor(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 3);
}

TEST(NativeFloor, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-2.3);
    Value r = vm.floor(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), -3);
}

TEST(NativeFloor, ExactFloat)
{
    VM vm;
    Value arg = Value::from_real(4.0);
    Value r = vm.floor(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 4);
}

TEST(NativeCeil, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(5);
    Value r = vm.ceil(1, &arg);
    EXPECT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 5);
}

TEST(NativeCeil, PositiveFloat)
{
    VM vm;
    Value arg = Value::from_real(3.2);
    Value r = vm.ceil(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 4);
}

TEST(NativeCeil, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-2.7);
    Value r = vm.ceil(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), -2);
}

TEST(NativeCeil, ExactFloat)
{
    VM vm;
    Value arg = Value::from_real(4.0);
    Value r = vm.ceil(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 4);
}

TEST(NativeAbs, PositiveInteger)
{
    VM vm;
    Value arg = Value::from_int(5);
    Value r = vm.abs(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 5);
}

TEST(NativeAbs, NegativeInteger)
{
    VM vm;
    Value arg = Value::from_int(-5);
    Value r = vm.abs(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 5);
}

TEST(NativeAbs, ZeroInteger)
{
    VM vm;
    Value arg = Value::from_int(0);
    Value r = vm.abs(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 0);
}

TEST(NativeAbs, PositiveFloat)
{
    VM vm;
    Value arg = Value::from_real(3.5);
    Value r = vm.abs(1, &arg);
    ASSERT_TRUE(r.is_double());
    EXPECT_DOUBLE_EQ(r.as_double(), 3.5);
}

TEST(NativeAbs, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-3.5);
    Value r = vm.abs(1, &arg);
    ASSERT_TRUE(r.is_double());
    EXPECT_DOUBLE_EQ(r.as_double(), 3.5);
}

TEST(NativeMin, OneArg_ReturnsArg)
{
    VM vm;
    srand(static_cast<unsigned>(time(nullptr)));
    Value n = Value::from_int(static_cast<i64>(rand()));
    Value result = vm.min(1, &n);
    EXPECT_EQ(result.as_int(), n.as_int());
}

TEST(NativeMin, TwoPositiveIntegers)
{
    VM vm;
    Value m_args[] = { Value::from_int(3), Value::from_int(7) };
    Value r = vm.min(2, m_args);
    EXPECT_EQ(r.as_int(), 3);
}

TEST(NativeMin, AllIntegersReturnsInteger)
{
    VM vm;
    Value m_args[] = { Value::from_int(5), Value::from_int(2), Value::from_int(8) };
    Value r = vm.min(3, m_args);
    EXPECT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 2);
}

TEST(NativeMin, MixedFloatAndInteger_ReturnsFloat)
{
    VM vm;
    Value m_args[] = { Value::from_int(3), Value::from_real(1.5) };
    Value r = vm.min(2, m_args);
    EXPECT_TRUE(r.is_double());
    EXPECT_DOUBLE_EQ(r.as_double(), 1.5);
}

TEST(NativeMin, NegativeValues)
{
    VM vm;
    Value m_args[] = { Value::from_int(-1), Value::from_int(-5), Value::from_int(-2) };
    Value r = vm.min(3, m_args);
    EXPECT_EQ(r.as_int(), -5);
}

TEST(NativeMax, OneArg_ReturnsArg)
{
    VM vm;
    srand(static_cast<unsigned>(time(nullptr)));
    Value n = Value::from_int(static_cast<i64>(rand()));
    Value result = vm.max(1, &n);
    EXPECT_EQ(result.as_int(), n.as_int());
}

TEST(NativeMax, TwoPositiveIntegers)
{
    VM vm;
    Value m_args[] = { Value::from_int(3), Value::from_int(7) };
    Value r = vm.max(2, m_args);
    EXPECT_EQ(r.as_int(), 7);
}

TEST(NativeMax, NegativeValues)
{
    VM vm;
    Value m_args[] = { Value::from_int(-3), Value::from_int(-1) };
    Value r = vm.max(2, m_args);
    EXPECT_EQ(r.as_int(), -1);
}

TEST(NativeMax, AllIntegersReturnsInteger)
{
    VM vm;
    Value m_args[] = { Value::from_int(1), Value::from_int(9), Value::from_int(4) };
    Value r = vm.max(3, m_args);
    EXPECT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 9);
}

TEST(NativeMax, MixedFloatAndInteger)
{
    VM vm;
    Value m_args[] = { Value::from_int(3), Value::from_real(3.5) };
    Value r = vm.max(2, m_args);
    EXPECT_TRUE(r.is_double());
    EXPECT_DOUBLE_EQ(r.as_double(), 3.5);
}

TEST(NativeMax, StringFirstArg_ReturnsArg)
{
    VM vm;
    Value m_args[] = { str("a"), str("b") };
    EXPECT_EQ(vm.max(2, m_args).as_string()->str, "b");
}

TEST(NativeRound, HalfRoundsUp)
{
    VM vm;
    Value arg = Value::from_real(2.5);
    Value r = vm.round(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 3);
}

TEST(NativeRound, HalfNegativeRoundsDown)
{
    VM vm;
    Value arg = Value::from_real(-2.5);
    Value r = vm.round(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), -3);
}

TEST(NativeRound, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(4);
    Value r = vm.round(1, &arg);
    ASSERT_TRUE(r.is_int());
    EXPECT_EQ(r.as_int(), 4);
}

TEST(NativePow, BasicSquare)
{
    VM vm;
    Value m_args[] = { Value::from_real(3.0), Value::from_real(2.0) };
    Value r = vm.pow(2, m_args);
    if (!r.is_nil())
        EXPECT_DOUBLE_EQ(r.as_double(), 9.0);
}

TEST(NativePow, ZeroExponent)
{
    VM vm;
    Value m_args[] = { Value::from_real(5.0), Value::from_real(0.0) };
    Value r = vm.pow(2, m_args);
    if (!r.is_nil())
        EXPECT_DOUBLE_EQ(r.as_double(), 1.0);
}

TEST(NativePow, NegativeExponent)
{
    VM vm;
    Value m_args[] = { Value::from_real(2.0), Value::from_real(-1.0) };
    Value r = vm.pow(2, m_args);
    if (!r.is_nil())
        EXPECT_DOUBLE_EQ(r.as_double(), 0.5);
}

TEST(NativeSqrt, PerfectSquare)
{
    VM vm;
    Value arg = Value::from_real(9.0);
    Value r = vm.sqrt(1, &arg);
    ASSERT_TRUE(r.is_double());
    EXPECT_DOUBLE_EQ(r.as_double(), 3.0);
}

TEST(NativeSqrt, Zero)
{
    VM vm;
    Value arg = Value::from_real(0.0);
    Value r = vm.sqrt(1, &arg);
    ASSERT_TRUE(r.is_double());
    EXPECT_DOUBLE_EQ(r.as_double(), 0.0);
}

TEST(NativeSqrt, NegativeInput_SpecBehavior)
{
    VM vm;
    Value arg = Value::from_real(-1.0);
    EXPECT_NO_FATAL_FAILURE(vm.sqrt(1, &arg));
}

TEST(NativeMathDispatch, SupportsDocumentedUnaryOperations)
{
    VM vm;
    Value sine_args[] = { str("sin"), Value::from_real(0.0) };
    Value exp_args[] = { str("exp"), Value::from_real(0.0) };
    EXPECT_DOUBLE_EQ(vm.math_unary(2, sine_args).as_double(), 0.0);
    EXPECT_DOUBLE_EQ(vm.math_unary(2, exp_args).as_double(), 1.0);
}

TEST(NativeMathDispatch, SupportsDocumentedBinaryOperations)
{
    VM vm;
    Value hypot_args[] = { str("hypot"), Value::from_int(3), Value::from_int(4) };
    Value atan_args[] = { str("atan2"), Value::from_real(0.0), Value::from_real(1.0) };
    EXPECT_DOUBLE_EQ(vm.math_binary(3, hypot_args).as_double(), 5.0);
    EXPECT_DOUBLE_EQ(vm.math_binary(3, atan_args).as_double(), 0.0);
}

TEST(NativeMathDispatch, RejectsBadTypesAndUnknownOperations)
{
    VM vm;
    Value bad_type[] = { str("sin"), str("not-a-number") };
    Value unknown[] = { str("unknown"), Value::from_int(1) };
    EXPECT_TRUE(vm.math_unary(2, bad_type).is_nil());
    EXPECT_TRUE(vm.math_unary(2, unknown).is_nil());
    EXPECT_TRUE(vm.math_binary(0, nullptr).is_nil());
}

TEST(NativeUrl, EncodesAndDecodesUtf8Bytes)
{
    VM vm;
    Value input = str("لغة فيروز/1");
    Value encoded = vm.url_encode(1, &input);
    ASSERT_TRUE(encoded.is_string());
    Value decoded = vm.url_decode(1, &encoded);
    ASSERT_TRUE(decoded.is_string());
    EXPECT_EQ(decoded.as_string()->str, input.as_string()->str);
    Value malformed = str("%GG");
    EXPECT_TRUE(vm.url_decode(1, &malformed).is_nil());
}

TEST(NativeUrl, ParsesAndRebuildsStructuredUrl)
{
    VM vm;
    Value input = str("https://example.test:443/api?q=1#part");
    Value parsed = vm.url_parse(1, &input);
    ASSERT_TRUE(parsed.is_dict());
    Value port = vm.dict_get(&parsed, str("port"));
    ASSERT_TRUE(port.is_int());
    EXPECT_EQ(port.as_int(), 443);
    Value rebuilt = vm.url_build(1, &parsed);
    ASSERT_TRUE(rebuilt.is_string());
    EXPECT_EQ(rebuilt.as_string()->str, input.as_string()->str);
}

TEST(NativeUrl, RejectsInvalidBoundaryArguments)
{
    VM vm;
    Value number = Value::from_int(42);
    EXPECT_TRUE(vm.url_encode(1, &number).is_nil());
    EXPECT_TRUE(vm.url_parse(1, &number).is_nil());
    EXPECT_TRUE(vm.url_build(1, &number).is_nil());
}

TEST(NativeRegex, CompilesSearchesMatchesAndCaptures)
{
    VM vm;
    Value compile_args[] = { str("([a-z])([0-9]+)"), Value::from_int(0) };
    Value handle = vm.regex_compile(2, compile_args);
    ASSERT_TRUE(handle.is_string());
    Value search_args[] = { handle, str("قبل a12 بعد"), Value::from_int(0) };
    Value found = vm.regex_search(3, search_args);
    ASSERT_TRUE(found.is_dict());
    EXPECT_EQ(vm.dict_get(&found, str("start")).as_int(), 4);
    Value groups = vm.dict_get(&found, str("groups"));
    ASSERT_TRUE(groups.is_list());
    ASSERT_EQ(groups.as_list()->elements.size(), 3u);
    EXPECT_EQ(groups.as_list()->elements[1].as_string()->str, StringRef("a"));

    Value match_args[] = { handle, str("a12 tail"), Value::from_int(0) };
    EXPECT_TRUE(vm.regex_match(3, match_args).is_dict());
    Value full_args[] = { handle, str("a12") };
    EXPECT_TRUE(vm.regex_fullmatch(2, full_args).is_dict());
}

TEST(NativeRegex, FindsAllSplitsAndReplacesWithLimits)
{
    VM vm;
    Value handle = str("[0-9]+");
    Value all_args[] = { handle, str("a1 b22 c333") };
    Value all = vm.regex_findall(2, all_args);
    ASSERT_TRUE(all.is_list());
    EXPECT_EQ(all.as_list()->elements.size(), 3u);

    Value split_args[] = { handle, str("a1b22c"), Value::from_int(1) };
    Value split = vm.regex_split(3, split_args);
    ASSERT_TRUE(split.is_list());
    EXPECT_EQ(split.as_list()->elements.size(), 2u);

    Value replace_args[] = { handle, str("a1b22c"), str("#"), Value::from_int(1) };
    Value replaced = vm.regex_replace(4, replace_args);
    ASSERT_TRUE(replaced.is_string());
    EXPECT_EQ(replaced.as_string()->str, StringRef("a#b22c"));
}

TEST(NativeRegex, RejectsInvalidPatternsAndArguments)
{
    VM vm;
    Value invalid[] = { str("["), Value::from_int(0) };
    EXPECT_TRUE(vm.regex_compile(2, invalid).is_nil());
    EXPECT_TRUE(vm.regex_search(0, nullptr).is_nil());
    EXPECT_TRUE(vm.regex_split(0, nullptr).is_nil());
    EXPECT_TRUE(vm.regex_replace(0, nullptr).is_nil());
}

TEST(NativeSplit, BasicSplit)
{
    VM vm;
    Value m_args[] = { str("a,b,c"), str(",") };
    Value r = vm.split(2, m_args);

    ASSERT_TRUE(r.is_list());
    EXPECT_EQ(r.as_list()->elements.size(), 3u);

    ASSERT_TRUE(r.as_list()->elements[0].is_string());
    ASSERT_TRUE(r.as_list()->elements[1].is_string());
    ASSERT_TRUE(r.as_list()->elements[2].is_string());

    EXPECT_EQ(std::string(r.as_list()->elements[0].as_string()->str.data()), "a");
    EXPECT_EQ(std::string(r.as_list()->elements[1].as_string()->str.data()), "b");
    EXPECT_EQ(std::string(r.as_list()->elements[2].as_string()->str.data()), "c");
}

TEST(NativeSplit, NoDelimiterFound)
{
    VM vm;
    Value m_args[] = { str("hello"), str(",") };
    Value r = vm.split(2, m_args);
    ASSERT_TRUE(r.is_list());
    EXPECT_EQ(r.as_list()->elements.size(), 1u);
    ASSERT_TRUE(r.as_list()->elements[0].as_string());
    EXPECT_EQ(std::string(r.as_list()->elements[0].as_string()->str.data()), "hello");
}

TEST(NativeSubstr, BasicSubstr)
{
    VM vm; // substr is exclusive
    Value m_args[] = { str("hello"), Value::from_int(1), Value::from_int(4) };
    Value r = vm.substr(3, m_args);
    if (!r.is_nil()) {
        ASSERT_TRUE(r.is_string());
        EXPECT_EQ(std::string(r.as_string()->str.data()), "ell");
    }
}

TEST(NativeSubstr, FromStart)
{
    VM vm; // substr is exclusive
    Value m_args[] = { str("hello"), Value::from_int(0), Value::from_int(3) };
    Value r = vm.substr(3, m_args);
    if (!r.is_nil()) {
        ASSERT_TRUE(r.is_string());
        EXPECT_EQ(std::string(r.as_string()->str.data()), "hel");
    }
}

TEST(NativeContains, StringContains_True)
{
    VM vm;
    Value m_args[] = { str("hello world"), str("world") };
    Value r = vm.contains(2, m_args);
    ASSERT_TRUE(r.is_bool());
    EXPECT_TRUE(r.as_bool());
}

TEST(NativeContains, StringContains_False)
{
    VM vm;
    Value m_args[] = { str("hello"), str("xyz") };
    Value r = vm.contains(2, m_args);
    ASSERT_TRUE(r.is_bool());
    EXPECT_FALSE(r.as_bool());
}

TEST(NativeTrim, LeadingAndTrailingSpaces)
{
    VM vm;
    Value arg = str("  hello  ");
    Value r = vm.trim(1, &arg);
    ASSERT_TRUE(r.is_string());
    EXPECT_EQ(std::string(r.as_string()->str.data()), "hello");
}

TEST(NativeTrim, NoSpaces)
{
    VM vm;
    Value arg = str("hello");
    Value r = vm.trim(1, &arg);
    ASSERT_TRUE(r.is_string());
    EXPECT_EQ(std::string(r.as_string()->str.data()), "hello");
}

TEST(NativeTrim, OnlySpaces)
{
    VM vm;
    Value arg = str("   ");
    Value r = vm.trim(1, &arg);
    ASSERT_TRUE(r.is_string());
    EXPECT_EQ(std::string(r.as_string()->str.data()), "");
}

TEST(NativeJoin, BasicJoin)
{
    VM vm;
    Value list = vm.list(0, nullptr);
    ObjList* l = list.as_list();
    l->elements.push(str("a"));
    l->elements.push(str("b"));
    l->elements.push(str("c"));
    Value m_args[] = { list, str("|") };
    Value r = vm.join(2, m_args);
    ASSERT_TRUE(r.is_string());
    EXPECT_EQ(std::string(r.as_string()->str.data()), "a|b|c");
}

TEST(NativeAssert, TrueCondition_DoesNotCrash)
{
    VM vm;
    Value arg = Value::from_bool(true);
    EXPECT_NO_FATAL_FAILURE(vm.Assert(1, &arg));
}

TEST(NativeAssert, OptionalMessageIsNotTreatedAsASecondCondition)
{
    VM vm;
    Value args[] = { Value::from_bool(true), str("") };
    EXPECT_NO_THROW(vm.Assert(2, args));
}

TEST(NativeClock, ReturnsFiniteMonotonicSeconds)
{
    VM vm;
    Value first = vm.clock(0, nullptr);
    Value second = vm.clock(0, nullptr);
    ASSERT_TRUE(first.is_double());
    ASSERT_TRUE(second.is_double());
    EXPECT_TRUE(std::isfinite(first.as_double()));
    EXPECT_GE(second.as_double(), first.as_double());
    EXPECT_TRUE(vm.clock(0, &first).is_double());
    EXPECT_TRUE(vm.clock(1, &first).is_nil());
}

TEST(NativeTime, ReturnsNumber_WhenImplemented)
{
    VM vm;
    Value r = vm.time(0, nullptr);
    if (!r.is_nil())
        EXPECT_TRUE(r.is_int());
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

// Measures raw opcode dispatch speed. The loop body is:
//   r2 = r0 + r1  (OP_ADD, integer fast path)
//   r0 = r2       (MOVE)
// N iterations → N ADD + N MOVE dispatched.

TEST_F(VMPerfTest, Dispatch_IntAdd_1M_Iterations)
{
    constexpr int N = 1'000'000;

    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({ expr_stmt(assign_expr(ident("i"), lit_int(0))),
            expr_stmt(assign_expr(ident("step"), lit_int(1))),
            expr_stmt(assign_expr(ident("limit"), lit_int(N))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(assign_expr(
                    ident("i"),
                    binary(ident("i"), ident("step"), AST::Expr::Kind::OP_ADD))) })),
            return_stmt(ident("i")) }));

    Chunk* top = compile_calling(test);
    if (test_config::dump_bytecode)
        top->disassemble();
    VM vm;
    // Warm up — lets the IC quicken the ADD opcode.
    vm.run(top);

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), N);
    std::printf("  Dispatch IntAdd loop %dk iters:    %.1f µs  (%.2f ns/op)\n",
        N / 1000, us, us * 1000.0 / N);
}

// 2. Dispatch throughput — float arithmetic loop
//    Same structure, but f64 accumulator — exercises the FF fast path.

TEST_F(VMPerfTest, Dispatch_FloatAdd_500k_Iterations)
{
    constexpr int N = 500'000;

    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({ expr_stmt(assign_expr(ident("i"), lit_flt(0.0))),
            expr_stmt(assign_expr(ident("step"), lit_flt(1.0))),
            expr_stmt(assign_expr(ident("limit"), lit_flt(static_cast<f64>(N)))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(assign_expr(
                    ident("i"),
                    binary(ident("i"), ident("step"), AST::Expr::Kind::OP_ADD))) })),
            return_stmt(ident("i")) }));

    Chunk* top = compile_calling(test);
    if (test_config::dump_bytecode)
        top->disassemble();
    VM vm;
    vm.run(top);

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_TRUE(result.is_double());
    std::printf("  Dispatch FloatAdd loop %dk iters:  %.1f µs  (%.2f ns/op)\n",
        N / 1000, us, us * 1000.0 / N);
}

// 3. IC quickening benefit — compare cold vs warm dispatch on same chunk
//    Cold: first run, generic opcode handlers.
//    Warm: second run, quickened ADD_II / ADD_FF specialisations.

TEST_F(VMPerfTest, IC_Quickening_ColdVsWarm_Ratio)
{
    constexpr int N = 200'000;

    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({ decl_stmt("i", lit_int(0)),
            decl_stmt("step", lit_int(1)),
            decl_stmt("limit", lit_int(N)),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(assign_expr(
                    ident("i"),
                    binary(ident("i"), ident("step"), AST::Expr::Kind::OP_ADD))) })),
            return_stmt(ident("i")) }));

    Chunk* top = compile_calling(test);
    if (test_config::dump_bytecode)
        top->disassemble();
    VM vm_cold, vm_warm;

    // Cold — no prior run.
    auto t0 = std::chrono::high_resolution_clock::now();
    Value cold_result = vm_cold.run(top);
    f64 cold_us = microseconds_since(t0);

    // Warm — opcodes already quickened by the cold run above.
    t0 = std::chrono::high_resolution_clock::now();
    Value warm_result = vm_warm.run(top);
    f64 warm_us = microseconds_since(t0);

    do_not_optimize(cold_result);
    do_not_optimize(warm_result);
    EXPECT_EQ(cold_result.as_int(), N);
    EXPECT_EQ(warm_result.as_int(), N);

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

TEST_F(VMPerfTest, GlobalLookup_1M_Roundtrips)
{
    constexpr int N = 1'000'000;

    AST::StmtPtr decl = decl_stmt("a", lit_int(0));
    AST::WhileStmt* while_loop = while_stmt(
        binary(ident("a"), lit_int(N), AST::Expr::Kind::OP_LT),
        expr_stmt(assign_expr(ident("a"), binary(ident("a"), lit_int(1), AST::Expr::Kind::OP_ADD))));

    Chunk* top = compile_program({ decl, while_loop });
    if (top != nullptr && test_config::dump_bytecode)
        top->disassemble();

    VM vm;

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    ::printf("  Global store+load %dk roundtrips:  %.1f µs  (%.2f ns/op)\n", N / 1000, us, us * 1000.0 / N);
}

TEST_F(VMPerfTest, CallOverhead_100k_Calls)
{
    constexpr int N = 100;

    AST::StmtPtr m_body = blk(
        { expr_stmt(assign_expr(ident("i"), lit_int(0))),
            expr_stmt(assign_expr(ident("limit"), lit_int(N))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(call_expr(ident("add"), { lit_int(1), lit_int(2) })),
                    expr_stmt(assign_expr(ident("i"), binary(ident("i"), lit_int(1), AST::Expr::Kind::OP_ADD))) })),
            return_stmt(ident("i")) });

    AST::StmtPtr func = func_def(ident("test"), { }, m_body);

    AST::StmtPtr add = func_def(
        ident("add"),
        { ident("a"), ident("b") },
        return_stmt(binary(ident("a"), ident("b"), AST::Expr::Kind::OP_ADD)));
    AST::StmtPtr call = expr_stmt(call_expr(ident("test")));

    std::cout << "AUTO:" << '\n';
    Chunk* top_ = compile_program({ add, func, call });
    if (test_config::dump_bytecode)
        top_->disassemble();
    VM vm;
    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top_);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), N);
    std::printf("  Call overhead %dk calls:            %.1f µs  (%.2f ns/call)\n",
        N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Tail-call vs regular call
//    Both count to N via recursion. Tail-call should use O(1) frames.
//    We measure time; the tail-call version must not be significantly slower.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(VMPerfTest, TailCall_vs_RegularLoop_Ratio)
{
    constexpr int DEPTH = 5000;

    AST::StmtPtr tc_fn = func_def(
        ident("tc"),
        { ident("n") },
        blk({ if_stmt(
                  binary(ident("n"), lit_int(0), AST::Expr::Kind::OP_EQ),
                  blk({ return_stmt(ident("n")) })),
            return_stmt(call_expr(ident("tc"), { binary(ident("n"), lit_int(1), AST::Expr::Kind::OP_SUB) })) }));

    Chunk* tc_top = compile_program({ tc_fn, expr_stmt(call_expr(ident("tc"), { lit_int(DEPTH) })) });

    if (test_config::dump_bytecode)
        tc_top->disassemble();
    VM vm_tc;
    auto t0 = std::chrono::high_resolution_clock::now();
    Value tc_result = vm_tc.run(tc_top);
    f64 tc_us = microseconds_since(t0);

    // ── equivalent iterative loop (no calls) ─────────────────────────────
    AST::StmtPtr loop_fn = func_def(
        ident("loop"),
        { },
        blk({ expr_stmt(assign_expr(ident("n"), lit_int(DEPTH))),
            while_stmt(
                binary(ident("n"), lit_int(0), AST::Expr::Kind::OP_NEQ),
                blk({ expr_stmt(assign_expr(
                    ident("n"),
                    binary(ident("n"), lit_int(1), AST::Expr::Kind::OP_SUB))) })),
            return_stmt(ident("n")) }));

    Chunk* loop_top = compile_calling(loop_fn);
    if (test_config::dump_bytecode)
        loop_top->disassemble();
    VM vm_loop;
    t0 = std::chrono::high_resolution_clock::now();
    Value loop_result = vm_loop.run(loop_top);
    f64 loop_us = microseconds_since(t0);

    do_not_optimize(tc_result);
    do_not_optimize(loop_result);
    EXPECT_EQ(tc_result.as_int(), 0);
    EXPECT_EQ(loop_result.as_int(), 0);

    ::printf("  Tail-call %d depth:  tc=%.1f µs  loop=%.1f µs  ratio=%.2fx\n",
        DEPTH, tc_us, loop_us, tc_us / loop_us);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. List operations throughput
//    Builds a list of N elements then iterates over it summing values.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(VMPerfTest, List_AppendAndSum_10k)
{
    constexpr int N = 10'000;
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({ expr_stmt(assign_expr(ident("xs"), list_expr())),
            expr_stmt(assign_expr(ident("i"), lit_int(0))),
            expr_stmt(assign_expr(ident("limit"), lit_int(N))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(call_expr(ident("اضف"), { ident("xs"), ident("i") })),
                    expr_stmt(assign_expr(
                        ident("i"),
                        binary(ident("i"), lit_int(1), AST::Expr::Kind::OP_ADD))) })),
            expr_stmt(assign_expr(ident("i"), lit_int(0))),
            expr_stmt(assign_expr(ident("sum"), lit_int(0))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(assign_expr(
                          ident("sum"),
                          binary(
                              ident("sum"),
                              index_expr(ident("xs"), ident("i")),
                              AST::Expr::Kind::OP_ADD))),
                    expr_stmt(assign_expr(
                        ident("i"),
                        binary(ident("i"), lit_int(1), AST::Expr::Kind::OP_ADD))) })),
            return_stmt(ident("sum")) }));

    Chunk* top = compile_calling(test);
    if (test_config::dump_bytecode)
        top->disassemble();
    VM vm;
    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);
    do_not_optimize(result);

    constexpr i64 expected = static_cast<i64>(N) * (N - 1) / 2;
    EXPECT_EQ(result.as_int(), expected);
    std::printf("  List append+sum N=%d:               %.1f µs\n", N, us);
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. Native function call throughput — len on a string, hot loop
//    IC_CALL path: first call warms the inline cache, subsequent calls hit it.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(VMPerfTest, NativeCall_Len_50k_ICHot)
{
    constexpr int N = 50'000;

    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({ expr_stmt(assign_expr(ident("s"), lit_str("hello world"))),
            expr_stmt(assign_expr(ident("i"), lit_int(0))),
            expr_stmt(assign_expr(ident("limit"), lit_int(N))),
            expr_stmt(assign_expr(ident("last"), lit_int(0))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(assign_expr(ident("last"), call_expr(ident("طول"), { ident("s") }))),
                    expr_stmt(assign_expr(
                        ident("i"),
                        binary(ident("i"), lit_int(1), AST::Expr::Kind::OP_ADD))) })),
            return_stmt(ident("last")) }));

    Chunk* top = compile_calling(test);
    if (test_config::dump_bytecode)
        top->disassemble();
    VM vm;

    // Cold run to prime the IC.
    vm.run(top);

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 11);
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
    AST::StmtPtr fib = func_def(
        ident("fib"),
        { ident("x") },
        blk({ if_stmt(
                  binary(ident("x"), lit_int(1), AST::Expr::Kind::OP_LTE),
                  blk({ return_stmt(ident("x")) })),
            return_stmt(
                binary(
                    call_expr(
                        ident("fib"),
                        { binary(ident("x"), lit_int(1), AST::Expr::Kind::OP_SUB) }),
                    call_expr(
                        ident("fib"),
                        { binary(ident("x"), lit_int(2), AST::Expr::Kind::OP_SUB) }),
                    AST::Expr::Kind::OP_ADD)) }));

    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({ expr_stmt(assign_expr(ident("i"), lit_int(0))),
            expr_stmt(assign_expr(ident("limit"), lit_int(reps))),
            expr_stmt(assign_expr(ident("result"), lit_int(0))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(assign_expr(ident("result"), call_expr(ident("fib"), { lit_int(n) }))),
                    expr_stmt(assign_expr(ident("i"), binary(ident("i"), lit_int(1), AST::Expr::Kind::OP_ADD))) })),
            return_stmt(ident("result")) }));

    Chunk* top = compile_program({ fib, test, expr_stmt(call_expr(ident("test"))) });
    if (test_config::dump_bytecode)
        top->disassemble();
    return top;
}

TEST_F(VMPerfTest, Fib20_100reps)
{
    constexpr int FIB_N = 20;
    constexpr int REPS = 100;

    VM vm;
    auto* top = make_fib_top(FIB_N, REPS);

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 6765); // fib(20)
    ::printf("  fib(%d) × %d reps:                  %.1f µs  (%.1f µs/call)\n", FIB_N, REPS, us, us / REPS);
}

TEST_F(VMPerfTest, Fib25_10reps)
{
    constexpr int FIB_N = 25;
    constexpr int REPS = 10;

    VM vm;
    auto* top = make_fib_top(FIB_N, REPS);

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 75025); // fib(25)
    ::printf("  fib(%d) × %d reps:                   %.1f µs  (%.1f µs/call)\n", FIB_N, REPS, us, us / REPS);
}

// ─────────────────────────────────────────────────────────────────────────────
// 11. Large stress tests (opt-in)
//     These are intentionally much larger than the regular perf tests and are
//     gated behind ENABLE_STRESS_PERF=1 so they do not dominate normal
//     test runs.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(VMPerfTest, Dispatch_IntAdd_10M_Iterations)
{
    constexpr int N = 10'000'000;

    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({ expr_stmt(assign_expr(ident("i"), lit_int(0))),
            expr_stmt(assign_expr(ident("step"), lit_int(1))),
            expr_stmt(assign_expr(ident("limit"), lit_int(N))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(assign_expr(
                    ident("i"),
                    binary(ident("i"), ident("step"), AST::Expr::Kind::OP_ADD))) })),
            return_stmt(ident("i")) }));

    Chunk* top = compile_calling(test);
    VM vm;
    vm.run(top); // warm quickened arithmetic path

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), N);
    std::printf("  STRESS IntAdd loop %dM iters:       %.1f µs  (%.2f ns/op)\n",
        N / 1'000'000, us, us * 1000.0 / N);
}

TEST_F(VMPerfTest, NativeCall_Len_1M_ICHot)
{
    constexpr int N = 1'000'000;

    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({ expr_stmt(assign_expr(ident("s"), lit_str("hello world"))),
            expr_stmt(assign_expr(ident("i"), lit_int(0))),
            expr_stmt(assign_expr(ident("limit"), lit_int(N))),
            expr_stmt(assign_expr(ident("last"), lit_int(0))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(assign_expr(ident("last"), call_expr(ident("طول"), { ident("s") }))),
                    expr_stmt(assign_expr(
                        ident("i"),
                        binary(ident("i"), lit_int(1), AST::Expr::Kind::OP_ADD))) })),
            return_stmt(ident("last")) }));

    Chunk* top = compile_calling(test);
    VM vm;
    vm.run(top); // warm IC_CALL -> CALL rewrite

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 11);
    std::printf("  STRESS native len() %dM calls:      %.1f µs  (%.2f ns/call)\n",
        N / 1'000'000, us, us * 1000.0 / N);
}

TEST_F(VMPerfTest, List_AppendAndSum_100k)
{
    constexpr int N = 100'000;

    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({ expr_stmt(assign_expr(ident("xs"), list_expr())),
            expr_stmt(assign_expr(ident("i"), lit_int(0))),
            expr_stmt(assign_expr(ident("limit"), lit_int(N))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(call_expr(ident("اضف"), { ident("xs"), ident("i") })),
                    expr_stmt(assign_expr(
                        ident("i"),
                        binary(ident("i"), lit_int(1), AST::Expr::Kind::OP_ADD))) })),
            expr_stmt(assign_expr(ident("i"), lit_int(0))),
            expr_stmt(assign_expr(ident("sum"), lit_int(0))),
            while_stmt(
                binary(ident("i"), ident("limit"), AST::Expr::Kind::OP_LT),
                blk({ expr_stmt(assign_expr(
                          ident("sum"),
                          binary(
                              ident("sum"),
                              index_expr(ident("xs"), ident("i")),
                              AST::Expr::Kind::OP_ADD))),
                    expr_stmt(assign_expr(
                        ident("i"),
                        binary(ident("i"), lit_int(1), AST::Expr::Kind::OP_ADD))) })),
            return_stmt(ident("sum")) }));

    Chunk* top = compile_calling(test);
    VM vm;

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    constexpr i64 expected = static_cast<i64>(N) * (N - 1) / 2;
    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), expected);
    std::printf("  STRESS list append+sum N=%d:        %.1f µs\n", N, us);
}

TEST_F(VMPerfTest, Fib28_20reps_Hot)
{
    constexpr int FIB_N = 28;
    constexpr int REPS = 20;

    VM vm;
    auto* top = make_fib_top(FIB_N, REPS);
    vm.run(top); // warm global lookup and call-site rewriting

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = vm.run(top);
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 317811);
    std::printf("  STRESS fib(%d) × %d reps hot:       %.1f µs  (%.1f µs/call)\n",
        FIB_N, REPS, us, us / REPS);
}

static FuncDefStmt* class_method(StringRef name, Array<ExprPtr> params, Array<StmtPtr> body)
{
    return func_def(ident(name), params, blk(body));
}

TEST(VMClass, TestConstruction)
{
    AST::StmtPtr klass = class_def(ident("TestClass"), { }, { });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("instance", call_expr(ident("TestClass"))),
            return_stmt(ident("instance")),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value result = r.run(top);
    ASSERT_TRUE(result.is_instance());
    EXPECT_EQ(result.as_instance()->klass->name, "TestClass");
}

TEST(VMClass, ClassDefinitionStoresRuntimeClass)
{
    AST::StmtPtr klass = class_def(
        ident("Point"),
        {
            ident("x"),
            ident("y"),
        },
        { });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            return_stmt(ident("Point")),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value point = r.run(top);
    ASSERT_TRUE(point.is_class());

    ObjClass* point_class = point.as_class();
    EXPECT_EQ(point_class->name, "Point");
    ASSERT_EQ(point_class->field_names.size(), 2u);
    EXPECT_EQ(point_class->field_names[0], "x");
    EXPECT_EQ(point_class->field_names[1], "y");
}

TEST(VMClass, ConstructorAcceptsArgumentsAndReturnsInstance)
{
    AST::StmtPtr klass = class_def(
        ident("Box"),
        { ident("value") },
        {
            func_def(
                ident(sp_method_name(ObjClass::INIT)),
                { ident("value") },
                blk({ })),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("instance", call_expr(ident("Box"), { lit_int(42) })),
            return_stmt(ident("instance")),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value result = r.run(top);
    ASSERT_TRUE(result.is_instance());
    EXPECT_EQ(result.as_instance()->klass->name, "Box");
    EXPECT_EQ(result.as_instance()->fields.size(), 1);
}

TEST(VMClass, ConstructorRejectsWrongArgumentCount)
{
    AST::StmtPtr klass = class_def(
        ident("NeedsArg"),
        { },
        {
            func_def(
                ident(sp_method_name(ObjClass::INIT)),
                { ident("arg") },
                blk({ })),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("instance", call_expr(ident("NeedsArg"))),
            return_stmt(ident("instance")),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    EXPECT_THROW(r.run(top), std::runtime_error);
}

TEST(VMClass, ConstructorWithoutInitRejectsArguments)
{
    AST::StmtPtr klass = class_def(ident("Plain"), { }, { });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("instance", call_expr(ident("Plain"), { lit_int(1) })),
            return_stmt(ident("instance")),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    EXPECT_THROW(r.run(top), std::runtime_error);
}

TEST(VMClass, InstanceFieldsDefaultToNil)
{
    AST::StmtPtr klass = class_def(
        ident("Point"),
        {
            ident("x"),
            ident("y"),
        },
        { });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("point", call_expr(ident("Point"))),
            return_stmt(ident("point")),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_instance());

    ObjInstance* point = result.as_instance();
    ASSERT_EQ(point->fields.size(), 2);
    EXPECT_TRUE(point->fields[0].is_nil());
    EXPECT_TRUE(point->fields[1].is_nil());
}

TEST(VMClass, ConstructorInitializesFieldsFromParameters)
{
    AST::StmtPtr klass = class_def(
        ident("Point"),
        {
            ident("x"),
            ident("y"),
        },
        {
            class_method(
                sp_method_name(ObjClass::INIT),
                { ident("x"), ident("y") },
                {
                    expr_stmt(assign_expr(ident("x"), ident("x"))),
                    expr_stmt(assign_expr(ident("y"), ident("y"))),
                }),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("point", call_expr(ident("Point"), { lit_int(3), lit_int(4) })),
            return_stmt(ident("point")),
        }));

    Chunk* top = compile_program({ klass, test, expr_stmt(call_expr(ident("test"))) });

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_instance());

    ObjInstance* point = result.as_instance();
    ASSERT_EQ(point->fields.size(), 2);

    ASSERT_TRUE(point->fields[0].is_int());
    ASSERT_TRUE(point->fields[1].is_int());

    EXPECT_EQ(point->fields[0].as_int(), 3);
    EXPECT_EQ(point->fields[1].as_int(), 4);
}

TEST(VMClass, FieldGetExpressionReadsInstanceField)
{
    /*
        class Box:
            fn init(this, value):
                this.value := value

        fn test():
            box = Box(12)
            return box.value
    */

    AST::StmtPtr klass = class_def(
        ident("Box"),
        { ident("value") },
        {
            class_method(
                sp_method_name(ObjClass::INIT),
                { ident("value") },
                {
                    expr_stmt(assign_expr(get_expr(ident(kClassInstanceName),
                                              ident("value")),
                        ident("value"))),
                }),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("box", call_expr(ident("Box"), { lit_int(12) })),
            return_stmt(get_expr(ident("box"), ident("value"))),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    if (test_config::dump_bytecode)
        top->disassemble();

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 12);
}

TEST(VMClass, FieldAssignmentUpdatesInstanceField)
{
    AST::StmtPtr klass = class_def(
        ident("Box"),
        { ident("value") },
        { });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("box", call_expr(ident("Box"))),
            expr_stmt(assign_expr(get_expr(ident("box"), ident("value")), lit_int(25))),
            return_stmt(get_expr(ident("box"), ident("value"))),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 25);
}

TEST(VMClass, MethodReceivesExplicitArguments)
{
    /*
        class Adder:
            fn add(a, b):
                return a + b

        fn test():
            adder = Adder()
            return adder.add(2, 5)
    */

    AST::StmtPtr klass = class_def(
        ident("Adder"),
        { },
        {
            class_method(
                "add",
                { ident("a"), ident("b") },
                {
                    return_stmt(binary(ident("a"), ident("b"), AST::Expr::Kind::OP_ADD)),
                }),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("adder", call_expr(ident("Adder"))),
            return_stmt(
                call_expr(get_expr(ident("adder"), ident("add")), { lit_int(2), lit_int(5) })),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    if (test_config::dump_bytecode)
        top->disassemble();

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 7);
}

TEST(VMClass, MethodReadsInstanceField)
{
    AST::StmtPtr klass = class_def(
        ident("Box"),
        { ident("value") },
        {
            class_method(
                sp_method_name(ObjClass::INIT),
                { ident("value") },
                {
                    expr_stmt(assign_expr(ident("value"), ident("value"))),
                }),
            class_method(
                "get_value",
                { },
                {
                    return_stmt(ident("value")),
                }),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("box", call_expr(ident("Box"), { lit_int(31) })),
            return_stmt(call_expr(get_expr(ident("box"), ident("get_value")))),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 31);
}

TEST(VMClass, MethodMutatesInstanceFieldAndPersists)
{
    auto get = [](AST::IdentifierExpr* member) {
        return get_expr(ident(kClassInstanceName), member);
    };

    auto init = [&](AST::IdentifierExpr* member, AST::ExprPtr value) -> AST::StmtPtr {
        return expr_stmt(assign_expr(get(member), value));
    };

    AST::StmtPtr klass = class_def(
        ident("Counter"),
        { ident("count") },
        {
            class_method(
                sp_method_name(ObjClass::INIT),
                { },
                {
                    init(ident("count"), lit_int(0)),
                }),
            class_method(
                "increment",
                { },
                {
                    expr_stmt(assign_expr(get(ident("count")), binary(get(ident("count")), lit_int(1), AST::Expr::Kind::OP_ADD))),
                    return_stmt(get(ident("count"))),
                }),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("counter", call_expr(ident("Counter"))),
            expr_stmt(call_expr(get_expr(ident("counter"), ident("increment")))),
            return_stmt(call_expr(get_expr(ident("counter"), ident("increment")))),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    top->disassemble();

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST(VMClass, MultipleInstancesKeepIndependentFieldState)
{
    AST::StmtPtr klass = class_def(
        ident("Box"),
        { ident("value") },
        {
            class_method(
                sp_method_name(ObjClass::INIT),
                { ident("value") },
                {
                    expr_stmt(assign_expr(ident("value"), ident("value"))),
                }),
            class_method(
                "get_value",
                { },
                {
                    return_stmt(ident("value")),
                }),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("left", call_expr(ident("Box"), { lit_int(10) })),
            decl_stmt("right", call_expr(ident("Box"), { lit_int(20) })),
            return_stmt(binary(
                call_expr(get_expr(ident("left"), ident("get_value"))),
                call_expr(get_expr(ident("right"), ident("get_value"))),
                AST::Expr::Kind::OP_ADD)),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 30);
}

TEST(VMClass, MethodReturningNoValueReturnsSelf)
{
    AST::StmtPtr klass = class_def(
        ident("Fluent"),
        { },
        {
            class_method("touch", { }, { }),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("fluent", call_expr(ident("Fluent"))),
            return_stmt(call_expr(get_expr(ident("fluent"), ident("touch")))),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_instance());
    EXPECT_EQ(result.as_instance()->klass->name, "Fluent");
}

TEST(VMClass, MethodRejectsWrongArgumentCount)
{
    AST::StmtPtr klass = class_def(
        ident("Adder"),
        { },
        {
            class_method(
                "add",
                { ident("value") },
                {
                    return_stmt(ident("value")),
                }),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("adder", call_expr(ident("Adder"))),
            return_stmt(call_expr(get_expr(ident("adder"), ident("add")))),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    EXPECT_THROW(r.run(top), std::runtime_error);
}

TEST(VMClass, UnknownMethodRaisesRuntimeError)
{
    AST::StmtPtr klass = class_def(ident("Empty"), { }, { });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("empty", call_expr(ident("Empty"))),
            return_stmt(call_expr(get_expr(ident("empty"), ident("missing")))),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    EXPECT_THROW(r.run(top), std::runtime_error);
}

TEST(VMClass, DuplicateFieldsAreDeduplicatedInDeclarationOrder)
{
    AST::StmtPtr klass = class_def(
        ident("Record"),
        {
            ident("id"),
            ident("name"),
            ident("id"),
        },
        { });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            return_stmt(ident("Record")),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_class());

    ObjClass* klass_obj = result.as_class();
    ASSERT_EQ(klass_obj->field_names.size(), 2u);
    EXPECT_EQ(klass_obj->field_names[0], "id");
    EXPECT_EQ(klass_obj->field_names[1], "name");
}

TEST(VMClass, MultipleMethodsAreStoredInRuntimeClass)
{
    AST::StmtPtr klass = class_def(
        ident("Ops"),
        { },
        {
            class_method("first", { }, { return_stmt(lit_int(1)) }),
            class_method("second", { }, { return_stmt(lit_int(2)) }),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            return_stmt(ident("Ops")),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_class());

    ObjClass* klass_obj = result.as_class();
    EXPECT_GE(klass_obj->method_names.size(), static_cast<u32>(ObjClass::_COUNT + 2));
    EXPECT_GE(klass_obj->method_slot("first"), 0);
    EXPECT_GE(klass_obj->method_slot("second"), 0);
}

TEST(VMClass, AddSpecialMethodHandlesBinaryPlus)
{
    AST::StmtPtr klass = class_def(
        ident("Numberish"),
        { },
        {
            class_method(
                sp_method_name(ObjClass::ADD),
                { ident("other") },
                {
                    return_stmt(lit_int(99)),
                }),
        });
    AST::StmtPtr test = func_def(
        ident("test"),
        { },
        blk({
            decl_stmt("left", call_expr(ident("Numberish"))),
            decl_stmt("right", call_expr(ident("Numberish"))),
            return_stmt(binary(ident("left"), ident("right"), AST::Expr::Kind::OP_ADD)),
        }));

    Chunk* top = compile_program({
        klass,
        test,
        expr_stmt(call_expr(ident("test"))),
    });

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = r.run(top));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 99);
}
