//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies VM execution, calls, values, and limits.
//===----------------------------------------------------------------------===//

#include "Compiler/compiler.hpp"
#include "Compiler/opcode.hpp"
#include "Diag/diagnostic.hpp"
#include "Interpreter/object.hpp"
#include "Interpreter/vm.hpp"
#include "Parser/ast.hpp"
#include "Support/array.hpp"
#include "test_common.h"
#include "test_config.h"
#include "test_source_program.hpp"

#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>

using namespace fairuz;
using namespace fairuz::runtime;

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
        ch->name = "<اختبار>";
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
        return ch->add_constant(Value::from_obj(reinterpret_cast<ObjHeader*>(strs_.back().get()))).value;
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
            u16 k = ch->add_constant(Value::from_int(v)).value;
            return ABx(OpCode::LOAD_CONST, r, k); // spill to constant pool
        }
    }

private:
    std::vector<std::unique_ptr<ObjString>> strs_;
};

struct VMRunner {
    // Low-level harness for bytecode inspection, returned Values/errors, and
    // repeated execution in one VM. Whole-program tests use test_source::run.
    VM vm;
    Chunk* chunk_ = nullptr;

    VM::RuntimeResult run(CB& builder) { return run(builder.ch); }
    VM::RuntimeResult run(Chunk* chunk)
    {
        chunk_ = chunk;
        return vm.run(chunk_);
    }
    void compile(std::string const& source) { chunk_ = test_source::compile(source); }
    void compile(Array<std::string> const& source) { compile(test_source::join(source)); }
    void compile(std::initializer_list<std::string> source) { compile(Array<std::string>(source)); }
    VM::RuntimeResult run() { return run(chunk_); }
    VM::RuntimeResult compile_and_run(std::string const& source)
    {
        compile(source);
        return run();
    }
    VM::RuntimeResult compile_and_run(Array<std::string> const& source)
    {
        compile(source);
        return run();
    }
    VM::RuntimeResult compile_and_run(std::initializer_list<std::string> source)
    {
        compile(source);
        return run();
    }
};

TEST(VMLoads, Nil)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_NIL, 0, 0, 1).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_TRUE(run_success(r.run(b)).is_nil());
}

TEST(VMLoads, NilFillsMultiple)
{
    VMRunner r;
    CB b;
    b.regs(3).ABC(OpCode::LOAD_NIL, 0, 0, 3).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_TRUE(run_success(r.run(b)).is_nil());
}

TEST(VMLoads, True)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMLoads, False)
{
    VMRunner r;
    CB b;
    b.regs(1).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMLoads, IntPositive)
{
    VMRunner r;
    CB b;
    b.regs(1).load_int(0, 42).ret(0);
    Value v = run_success(r.run(b));
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
    Value v = run_success(r.run(b));
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
    Value v = run_success(r.run(b));
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
    Value v = run_success(r.run(b));
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
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -32767);
}

TEST(VMLoads, ConstDouble)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.ch->add_constant(Value::from_real(3.14)).value;
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
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
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_string());
    EXPECT_EQ(v.as_string()->str, "hello");
}

TEST(VMLoads, ConstLargeInt)
{
    VMRunner r;
    CB b;
    b.regs(1);
    u16 k = b.ch->add_constant(Value::from_int(1000000LL)).value;
    b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 1000000LL);
}

TEST(VMLoads, ReturnNil)
{
    VMRunner r;
    CB b;
    b.regs(1).ret_nil();
    EXPECT_TRUE(run_success(r.run(b)).is_nil());
}

TEST(VMMove, Copies)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 77).mov(1, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
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
    Value v = run_success(r.run(b));
    EXPECT_EQ(v.as_int(), 55);
}

TEST(VMArith, AddIntFastPath)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 32).ABC(OpCode::OP_ADD, 2, 0, 1).nop().ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 42);
}

TEST(VMArith, AddDoubles)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.ch->add_constant(Value::from_real(1.5)).value;
    u16 k1 = b.ch->add_constant(Value::from_real(2.5)).value;
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_ADD, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
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
    Value v = run_success(r.run(b));
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
    EXPECT_EQ(run_success(r.run(b)).as_int(), 42);
}

TEST(VMArith, MulInt)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 7).ABC(OpCode::OP_MUL, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 42);
}

TEST(VMArith, DivDouble)
{
    VMRunner r;
    CB b;
    b.regs(3);
    u16 k0 = b.ch->add_constant(Value::from_real(84.0)).value;
    u16 k1 = b.ch->add_constant(Value::from_real(2.0)).value;
    b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_DIV, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_DOUBLE_EQ(run_success(r.run(b)).as_double(), 42.0);
}

TEST(VMArith, ModPositive)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 17).load_int(1, 5).ABC(OpCode::OP_MOD, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value result = run_success(r.run(b));
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
    Value result = run_success(r.run(b));
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
    EXPECT_EQ(run_success(r.run(b)).as_int(), -7);
}

TEST(VMArith, NegDouble)
{
    VMRunner r;
    CB b;
    b.regs(2);
    u16 k = b.ch->add_constant(Value::from_real(3.5)).value;
    b.ABx(OpCode::LOAD_CONST, 0, k).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_DOUBLE_EQ(run_success(r.run(b)).as_double(), -3.5);
}

TEST(VMArith, DivByZeroThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run(
        "طبيعي(1.0 / 0.0)\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMArith, NegOnStringThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run(
        "طبيعي(-\"س\")\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMBitwise, And)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b1010).ABC(OpCode::OP_BITAND, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 0b1010);
}

TEST(VMBitwise, Or)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1100).load_int(1, 0b0011).ABC(OpCode::OP_BITOR, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 0b1111);
}

TEST(VMBitwise, Xor)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 0b1111).load_int(1, 0b0101).ABC(OpCode::OP_BITXOR, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 0b1010);
}

TEST(VMBitwise, Not)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(OpCode::OP_BITNOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), ~i64(0));
}

TEST(VMBitwise, Shl)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1).ABC(OpCode::OP_LSHIFT, 1, 0, 8).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 256);
}

TEST(VMBitwise, Shr)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1024).ABC(OpCode::OP_RSHIFT, 1, 0, 3).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 128);
}

TEST(VMBitwise, ShrArithmetic_NegativeInput)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, -8).load_int(1, 1).ABC(OpCode::OP_RSHIFT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -4);
}

TEST(VMBitwise, AndOnDoubleThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run(
        "طبيعي(1.0 & 1)\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMCompare, EqIntsTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, EqIntsFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, NeqTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_NEQ, 2, 0, 1).ret(2);
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LtTrue)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 3).load_int(1, 7).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LtFalseEqual)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, LeTrue_Equal)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, LeFalse)
{
    VMRunner r;
    CB b;
    b.regs(3).load_int(0, 6).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
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
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, NotFalseIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, NotTrueIsFalse)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && !v.as_bool());
}

TEST(VMCompare, NotNilIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LOAD_NIL, 0, 0, 1).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMCompare, NotZeroIsTrue)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    Value v = run_success(r.run(b));
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
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_bool() && v.as_bool());
}

TEST(VMGlobals, StoreAndLoad)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 123).stg(0, "عام").load_int(0, 0).ldg(1, "عام").ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 123);
}

TEST(VMGlobals, MissingGlobalReturnsError)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run(
        "غير_موجود\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMGlobals, Overwrite)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 1).stg(0, "س").load_int(0, 2).stg(0, "س").ldg(1, "س").ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 2);
}

TEST(VMLists, NewEmpty)
{
    VMRunner r;
    CB b;
    b.regs(2).ABC(OpCode::LIST_NEW, 0, 0, 0).ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
    if (test_config::dump_bytecode)
        b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 0);
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
    EXPECT_EQ(run_success(r.run(b)).as_int(), 3);
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
    EXPECT_EQ(run_success(r.run(b)).as_int(), 77);
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
    EXPECT_EQ(run_success(r.run(b)).as_int(), 20);
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
    EXPECT_EQ(run_success(r.run(b)).as_int(), 99);
}

TEST(VMLists, OutOfBoundsThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run(
        "طبيعي([][0])\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMLists, LenOnNonListThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run(
        "لكل عنصر في 5:\n"
        "    اكتب(عنصر)\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMDicts, IndexReturnsStoredValue)
{
    VMRunner r;
    auto ast = Array<std::string> {
        "دالة دالة_اختبار():\n"
        "    مفتاح := \"lang\"\n"
        "    قاموس := {}\n"
        "    قاموس[مفتاح] := 7\n"
        "    س := قاموس[مفتاح]\n"
        "    ارجع س\n",
        "دالة_اختبار()\n",
    };

    Value result = run_success(r.compile_and_run(ast));

    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 7);
}

TEST(VMDicts, SetUpdatesAndAppendsByKey)
{
    VMRunner r;
    auto ast = Array<std::string> {
        "دالة دالة_اختبار():\n"
        "    قاموس := {}\n"
        "    قاموس[\"fk\"] := 1\n"
        "    قاموس[\"sk\"] := 2\n"
        "    ارجع [قاموس[\"fk\"], قاموس[\"sk\"]]\n",
        "دالة_اختبار()\n",
    };

    Value result = run_success(r.compile_and_run(ast));
    ASSERT_TRUE(result.is_list());
    ObjList* ret_obj = result.as_list();
    ASSERT_EQ(ret_obj->size(), 2);
    EXPECT_EQ(ret_obj->elements[0].as_int(), 1);
    EXPECT_EQ(ret_obj->elements[1].as_int(), 2);
}

TEST(VMDicts, MissingKeyReturnsNil)
{
    VMRunner r;
    auto ast = Array<std::string> {
        "س := {}[\"missing\"]\n",
    };

    EXPECT_TRUE(run_success(r.compile_and_run(ast)).is_nil());
}

static Chunk* make_adder_chunk()
{
    auto fn = make_chunk();
    fn->name = "اجمع_اثنين";
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
    top->name = "<اختبار>";
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
    EXPECT_EQ(run_success(r.run(top)).as_int(), 7);
}

TEST(VMCalls, WrongArgcThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run(
        "دالة اجمع(س، ص):\n"
        "    ارجع س + ص\n"
        "اجمع(1)\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMCalls, CallNonFunctionThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run(
        "قيمة := 5\n"
        "قيمة()\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
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
    EXPECT_EQ(run_success(r.run(b)).as_int(), 3);
}

TEST(VMCalls, TailCall_DoesNotOverflowFrames)
{
    auto fn = make_chunk();
    fn->name = "تنازل";
    fn->arity = 1;
    fn->local_count = 4;

    u16 nk = fn->add_constant(str("cd")).value;

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
    top->name = "<اختبار>";
    top->local_count = 3;
    top->functions.push(fn);

    u16 tk = top->add_constant(str("cd")).value;
    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::STORE_GLOBAL, 0, tk), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(300)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 1, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    if (test_config::dump_bytecode)
        top->disassemble();
    VMRunner r;
    Value v = run_success(r.run(std::move(top)));
    EXPECT_EQ(v.as_int(), 0);
}

TEST(VMCalls, StackOverflowDetected)
{
    VMRunner r;
    auto ast = Array<std::string> {
        "دالة تكرار_لا_نهائي():\n"
        "    تكرار_لا_نهائي()\n"
        "    ارجع عدم\n",
        "تكرار_لا_نهائي()\n",
    };
    run_failed(r.compile_and_run(ast));
}

TEST(VMGlobals, UndefinedGlobalRaisesRuntimeError)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run(
        "مفقود\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMIntegration, FunctionLocalDeclarationShadowsGlobal)
{
    std::string make_local = "دالة انشئ_محليا():\n"
                             "    س := 2\n"
                             "    ارجع س\n";

    std::string read_global = "دالة اقرا_عام():\n"
                              "    ارجع س\n";

    std::string main_fn = "دالة رئيسي():\n"
                          "    ارجع [انشئ_محليا(), اقرا_عام()]\n";

    auto ast = Array<std::string> {
        "س := 1\n",
        make_local,
        read_global,
        main_fn,
        "نتيجة := رئيسي()\n"
        "تاكد(صنف(نتيجة) = \"قائمة\")\n"
        "تاكد(طول(نتيجة) = 2)\n"
        "تاكد(صنف(نتيجة[0]) = \"طبيعي\")\n"
        "تاكد(صنف(نتيجة[1]) = \"طبيعي\")\n"
        "تاكد(نتيجة[0] = 2)\n"
        "تاكد(نتيجة[1] = 1)\n",
    };
    EXPECT_EQ(test_source::run(test_source::join(ast)), ExitStatus::SUCCESS);
}

TEST(VMICProfile, BinaryOpUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 3).load_int(1, 4).ABC(OpCode::OP_ADD, 2, 0, 1).nop(0).ret(2);
    run_success(r.run(b));
    if (test_config::dump_bytecode)
        b.dump();
    auto const& s = r.chunk_->ic_slots[0];
    EXPECT_TRUE(has_tag(TypeTag(s.seen_lhs), TypeTag::INT));
    EXPECT_TRUE(has_tag(TypeTag(s.seen_rhs), TypeTag::INT));
    EXPECT_TRUE(has_tag(TypeTag(s.seen_ret), TypeTag::INT));
    EXPECT_GE(s.hit_count, 1u);
}

TEST(VMICProfile, SubUpdatesSlot)
{
    VMRunner r;
    CB b;
    b.regs(3).slot().load_int(0, 10).load_int(1, 3).ABC(OpCode::OP_SUB, 2, 0, 1).nop(0).ret(2);
    if (test_config::dump_bytecode)
        b.dump();
    run_success(r.run(b));
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
    run_success(r.run(b));
    EXPECT_EQ(r.chunk_->ic_slots[0].hit_count, 5u);
}

TEST(VMIntegration, TopLevelWhileAssignmentUpdatesGlobal)
{
    std::string read_global = "دالة اقرا_عام():\n"
                              "    ارجع س\n";

    auto ast = Array<std::string> {
        "س := 0\n",
        "حد := 3\n",
        "طالما (س < حد):\n"
        "    س := (س + 1)\n",
        read_global,
        "نتيجة := اقرا_عام()\n"
        "تاكد(صنف(نتيجة) = \"طبيعي\")\n"
        "تاكد(نتيجة = 3)\n",
    };

    EXPECT_EQ(test_source::run(test_source::join(ast)), ExitStatus::SUCCESS);
}

TEST(VMIntegration, Fibonacci_fib10_equals_55)
{
    std::string fib = (std::string("دالة فيبوناتشي(ن):\n") + test_source::indent((std::string((std::string("اذا (ن <= 1):\n"
                                                                                                           "    ارجع ن\n")
                                                                                      + (std::string("غيره:\n") + test_source::indent({ }))))
                           + "ارجع (فيبوناتشي((ن - 1)) + فيبوناتشي((ن - 2)))\n")));

    EXPECT_EQ(test_source::run(fib + "نتيجة := فيبوناتشي(10)\n"
                                     "تاكد(صنف(نتيجة) = \"طبيعي\")\n"
                                     "تاكد(نتيجة = 55)\n"),
        ExitStatus::SUCCESS);
}

TEST(VMIntegration, SumForLoopOverList)
{
    std::string sum = "دالة مجموع():\n"
                      "    عناصر := [1, 2, 3, 4, 5]\n"
                      "    اجمالي := 0\n"
                      "    لكل عنصر في عناصر:\n"
                      "        اجمالي := (اجمالي + عنصر)\n"
                      "    ارجع اجمالي\n";

    EXPECT_EQ(test_source::run(sum + "نتيجة := مجموع()\n"
                                     "تاكد(صنف(نتيجة) = \"طبيعي\")\n"
                                     "تاكد(نتيجة = 15)\n"),
        ExitStatus::SUCCESS);
}

TEST(VMIntegration, StringConcat_3Parts)
{
    std::string test = "دالة اختبار():\n"
                       "    ارجع ((\"hello\" + \", \") + \"world\")\n";

    EXPECT_EQ(test_source::run(test + "نتيجة := اختبار()\n"
                                      "تاكد(صنف(نتيجة) = \"سلسلة\")\n"
                                      "تاكد(نتيجة = \"hello, world\")\n"),
        ExitStatus::SUCCESS);
}

TEST(VMIntegration, EmptyForLoopLeavesStateUnchanged)
{
    std::string first = "دالة اول():\n"
                        "    عناصر := []\n"
                        "    مرئي := 99\n"
                        "    لكل عنصر في عناصر:\n"
                        "        مرئي := عنصر\n"
                        "    ارجع مرئي\n";

    EXPECT_EQ(test_source::run(first + "نتيجة := اول()\n"
                                       "تاكد(صنف(نتيجة) = \"طبيعي\")\n"
                                       "تاكد(نتيجة = 99)\n"),
        ExitStatus::SUCCESS);
}

TEST(VMIntegration, BreakAndContinueWorkInLoops)
{
    std::string test = "دالة اختبار():\n"
                       "    عناصر := [1, 2, 3, 4, 5]\n"
                       "    اجمالي := 0\n"
                       "    لكل عنصر في عناصر:\n"
                       "        اذا (عنصر = 2):\n"
                       "            اكمل\n"
                       "        اذا (عنصر = 5):\n"
                       "            اخرج\n"
                       "        اجمالي := (اجمالي + عنصر)\n"
                       "    ارجع اجمالي\n";

    EXPECT_EQ(test_source::run(test + "نتيجة := اختبار()\n"
                                      "تاكد(صنف(نتيجة) = \"طبيعي\")\n"
                                      "تاكد(نتيجة = 8)\n"),
        ExitStatus::SUCCESS);
}

TEST(NativeLen, MissingArgumentReturnsError)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run(
        "طول()\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(NativeLen, EmptyString)
{
    VM vm;
    auto s = str("");

    auto result = run_success(vm.len(1, &s));
    EXPECT_EQ(result.as_int(), 0);
}

TEST(NativeLen, NonEmptyString)
{
    VM vm;
    auto s = str("hello");
    auto result = run_success(vm.len(1, &s));
    EXPECT_EQ(result.as_int(), 5);
}

TEST(NativePrint, NoArgs_PrintsNewline)
{
    VM vm;
    auto result = run_success(vm.print(0, nullptr));
    EXPECT_TRUE(result.is_truthy());
}

TEST(NativePrint, StringArg)
{
    VM vm;
    auto s = str("hello world");
    auto result = run_success(vm.print(1, &s));
    EXPECT_TRUE(result.is_truthy());
}

TEST(NativePrint, IntegerArg)
{
    VM vm;
    Value arg = Value::from_int(42);
    auto result = run_success(vm.print(1, &arg));
    EXPECT_TRUE(result.is_truthy());
}

TEST(NativePrint, FloatArg)
{
    VM vm;
    Value arg = Value::from_real(3.14);
    auto result = run_success(vm.print(1, &arg));
    EXPECT_TRUE(result.is_truthy());
}

TEST(NativePrint, BoolArg_True)
{
    VM vm;
    Value arg = Value::from_bool(true);
    auto result = run_success(vm.print(1, &arg));
    EXPECT_TRUE(result.is_truthy());
}

TEST(NativePrint, BoolArg_False)
{
    VM vm;
    Value arg = Value::from_bool(false);
    auto result = run_success(vm.print(1, &arg));
    EXPECT_TRUE(result.is_truthy());
}

TEST(NativePrint, NilArg)
{
    VM vm;
    Value arg = Value::nil();
    auto result = run_success(vm.print(1, &arg));
    EXPECT_TRUE(result.is_truthy());
}

TEST(NativePrint, TwoArgs_DoesNotCrash)
{
    VM vm;
    auto s = str("a");
    Value args[] = { s, s };
    auto result = run_success(vm.print(2, args));
    EXPECT_TRUE(result.is_truthy());
}

TEST(NativeStr, NoArgs_ReturnsEmpty)
{
    VM vm;
    auto result = run_success(vm.str(0, nullptr));
    EXPECT_TRUE(result.is_string());
}

TEST(NativeStr, Integer)
{
    VM vm;
    Value arg = Value::from_int(42);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "42");
}

TEST(NativeStr, NegativeInteger)
{
    VM vm;
    Value arg = Value::from_int(-7);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "-7");
}

TEST(NativeStr, Zero)
{
    VM vm;
    Value arg = Value::from_int(0);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "0");
}

TEST(NativeStr, BoolTrue)
{
    VM vm;
    Value arg = Value::from_bool(true);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "صحيح");
}

TEST(NativeStr, BoolFalse)
{
    VM vm;
    Value arg = Value::from_bool(false);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "خطا");
}

TEST(NativeStr, Float)
{
    VM vm;
    Value arg = Value::from_real(1.5);
    auto result = run_success(vm.str(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    char const* text_ptr = v.as_string()->str.data();
    f64 parsed = 0.0;
    auto [end_ptr, ec] = std::from_chars(text_ptr, text_ptr + v.as_string()->str.len(), parsed);
    ASSERT_EQ(ec, std::errc());
    ASSERT_EQ(end_ptr, text_ptr + v.as_string()->str.len());
    EXPECT_DOUBLE_EQ(parsed, 1.5);
}

TEST(NativeStr, StringPassthrough)
{
    VM vm;
    Value s = str("hello");
    auto result = run_success(vm.str(1, &s));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "hello");
}

TEST(NativeBool, TrueBoolean)
{
    VM vm;
    Value arg = Value::from_bool(true);
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_TRUE(v.as_bool());
}

TEST(NativeBool, FalseBoolean)
{
    VM vm;
    Value arg = Value::from_bool(false);
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_FALSE(v.as_bool());
}

TEST(NativeBool, NonZeroInteger_IsTrue)
{
    VM vm;
    Value arg = Value::from_int(1);
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_TRUE(v.as_bool());
}

TEST(NativeBool, ZeroInteger_IsFalsy)
{
    VM vm;
    Value arg = Value::from_int(0);
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_FALSE(v.as_bool());
}

TEST(NativeBool, NilArg)
{
    VM vm;
    Value arg = Value::nil();
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_FALSE(v.as_bool());
}

TEST(NativeBool, NonEmptyString_IsTrue)
{
    VM vm;
    Value arg = str("hi");
    auto result = run_success(vm.Bool(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_TRUE(v.as_bool());
}

TEST(NativeInt, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(7);
    auto result = run_success(vm.Int(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 7);
}

TEST(NativeInt, FloatTruncates)
{
    VM vm;
    Value arg = Value::from_real(3.9);
    auto result = run_success(vm.Int(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 3);
}

TEST(NativeInt, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-2.7);
    auto result = run_success(vm.Int(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -2);
}

TEST(NativeFloat, IntegerToFloat)
{
    VM vm;
    Value arg = Value::from_int(3);
    auto result = run_success(vm.Float(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.0);
}

TEST(NativeFloat, FloatPassthrough)
{
    VM vm;
    Value arg = Value::from_real(2.5);
    auto result = run_success(vm.Float(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 2.5);
}

TEST(NativeFloat, NegativeInteger)
{
    VM vm;
    Value arg = Value::from_int(-10);
    auto result = run_success(vm.Float(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), -10.0);
}

TEST(NativeType, ReturnsTypeNames)
{
    VM vm;
    Value i = Value::from_int(0);
    Value f = Value::from_real(0.0);
    Value b = Value::from_bool(false);
    Value n = Value::nil();
    Value s = str("x");

    EXPECT_EQ(run_success(vm.type(1, &i)).as_string()->str, "طبيعي");
    EXPECT_EQ(run_success(vm.type(1, &f)).as_string()->str, "حقيقي");
    EXPECT_EQ(run_success(vm.type(1, &b)).as_string()->str, "منطقي");
    EXPECT_EQ(run_success(vm.type(1, &n)).as_string()->str, "عدم");
    EXPECT_EQ(run_success(vm.type(1, &s)).as_string()->str, "سلسلة");
}

TEST(NativeFloor, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(5);
    auto result = run_success(vm.floor(1, &arg));
    Value v = result;
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 5);
}

TEST(NativeFloor, PositiveFloat)
{
    VM vm;
    Value arg = Value::from_real(3.7);
    auto result = run_success(vm.floor(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 3);
}

TEST(NativeFloor, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-2.3);
    auto result = run_success(vm.floor(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -3);
}

TEST(NativeFloor, ExactFloat)
{
    VM vm;
    Value arg = Value::from_real(4.0);
    auto result = run_success(vm.floor(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 4);
}

TEST(NativeCeil, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(5);
    auto result = run_success(vm.ceil(1, &arg));
    Value v = result;
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 5);
}

TEST(NativeCeil, PositiveFloat)
{
    VM vm;
    Value arg = Value::from_real(3.2);
    auto result = run_success(vm.ceil(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 4);
}

TEST(NativeCeil, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-2.7);
    auto result = run_success(vm.ceil(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -2);
}

TEST(NativeCeil, ExactFloat)
{
    VM vm;
    Value arg = Value::from_real(4.0);
    auto result = run_success(vm.ceil(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 4);
}

TEST(NativeAbs, PositiveInteger)
{
    VM vm;
    Value arg = Value::from_int(5);
    auto result = run_success(vm.abs(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 5);
}

TEST(NativeAbs, NegativeInteger)
{
    VM vm;
    Value arg = Value::from_int(-5);
    auto result = run_success(vm.abs(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 5);
}

TEST(NativeAbs, ZeroInteger)
{
    VM vm;
    Value arg = Value::from_int(0);
    auto result = run_success(vm.abs(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 0);
}

TEST(NativeAbs, PositiveFloat)
{
    VM vm;
    Value arg = Value::from_real(3.5);
    auto result = run_success(vm.abs(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.5);
}

TEST(NativeAbs, NegativeFloat)
{
    VM vm;
    Value arg = Value::from_real(-3.5);
    auto result = run_success(vm.abs(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.5);
}

TEST(NativeMin, OneArg_ReturnsArg)
{
    VM vm;
    srand(static_cast<unsigned>(time(nullptr)));
    Value n = Value::from_int(static_cast<i64>(rand()));
    auto result = run_success(vm.min(1, &n));
    Value v = result;
    EXPECT_EQ(v.as_int(), n.as_int());
}

TEST(NativeMin, TwoPositiveIntegers)
{
    VM vm;
    Value args[] = { Value::from_int(3), Value::from_int(7) };
    auto result = run_success(vm.min(2, args));
    Value v = result;
    EXPECT_EQ(v.as_int(), 3);
}

TEST(NativeMin, AllIntegersReturnsInteger)
{
    VM vm;
    Value args[] = { Value::from_int(5), Value::from_int(2), Value::from_int(8) };
    auto result = run_success(vm.min(3, args));
    Value v = result;
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 2);
}

TEST(NativeMin, MixedFloatAndInteger_ReturnsFloat)
{
    VM vm;
    Value args[] = { Value::from_int(3), Value::from_real(1.5) };
    auto result = run_success(vm.min(2, args));
    Value v = result;
    EXPECT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 1.5);
}

TEST(NativeMin, NegativeValues)
{
    VM vm;
    Value args[] = { Value::from_int(-1), Value::from_int(-5), Value::from_int(-2) };
    auto result = run_success(vm.min(3, args));
    Value v = result;
    EXPECT_EQ(v.as_int(), -5);
}

TEST(NativeMax, OneArg_ReturnsArg)
{
    VM vm;
    srand(static_cast<unsigned>(time(nullptr)));
    Value n = Value::from_int(static_cast<i64>(rand()));
    auto result = run_success(vm.max(1, &n));
    Value v = result;
    EXPECT_EQ(v.as_int(), n.as_int());
}

TEST(NativeMax, TwoPositiveIntegers)
{
    VM vm;
    Value args[] = { Value::from_int(3), Value::from_int(7) };
    auto result = run_success(vm.max(2, args));
    Value v = result;
    EXPECT_EQ(v.as_int(), 7);
}

TEST(NativeMax, NegativeValues)
{
    VM vm;
    Value args[] = { Value::from_int(-3), Value::from_int(-1) };
    auto result = run_success(vm.max(2, args));
    Value v = result;
    EXPECT_EQ(v.as_int(), -1);
}

TEST(NativeMax, AllIntegersReturnsInteger)
{
    VM vm;
    Value args[] = { Value::from_int(1), Value::from_int(9), Value::from_int(4) };
    auto result = run_success(vm.max(3, args));
    Value v = result;
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 9);
}

TEST(NativeMax, MixedFloatAndInteger)
{
    VM vm;
    Value args[] = { Value::from_int(3), Value::from_real(3.5) };
    auto result = run_success(vm.max(2, args));
    Value v = result;
    EXPECT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.5);
}

TEST(NativeMax, StringFirstArg_ReturnsArg)
{
    VM vm;
    Value args[] = { str("a"), str("b") };
    auto result = run_success(vm.max(2, args));
    Value v = result;
    EXPECT_EQ(v.as_string()->str, "b");
}

TEST(NativeRound, HalfRoundsUp)
{
    VM vm;
    Value arg = Value::from_real(2.5);
    auto result = run_success(vm.round(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 3);
}

TEST(NativeRound, HalfNegativeRoundsDown)
{
    VM vm;
    Value arg = Value::from_real(-2.5);
    auto result = run_success(vm.round(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), -3);
}

TEST(NativeRound, IntegerPassthrough)
{
    VM vm;
    Value arg = Value::from_int(4);
    auto result = run_success(vm.round(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 4);
}

TEST(NativePow, BasicSquare)
{
    VM vm;
    Value args[] = { Value::from_real(3.0), Value::from_real(2.0) };
    auto result = run_success(vm.pow(2, args));
    Value v = result;
    if (!v.is_nil())
        EXPECT_DOUBLE_EQ(v.as_double(), 9.0);
}

TEST(NativePow, ZeroExponent)
{
    VM vm;
    Value args[] = { Value::from_real(5.0), Value::from_real(0.0) };
    auto result = run_success(vm.pow(2, args));
    Value v = result;
    if (!v.is_nil())
        EXPECT_DOUBLE_EQ(v.as_double(), 1.0);
}

TEST(NativePow, NegativeExponent)
{
    VM vm;
    Value args[] = { Value::from_real(2.0), Value::from_real(-1.0) };
    auto result = run_success(vm.pow(2, args));
    Value v = result;
    if (!v.is_nil())
        EXPECT_DOUBLE_EQ(v.as_double(), 0.5);
}

TEST(NativeSqrt, PerfectSquare)
{
    VM vm;
    Value arg = Value::from_real(9.0);
    auto result = run_success(vm.sqrt(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 3.0);
}

TEST(NativeSqrt, Zero)
{
    VM vm;
    Value arg = Value::from_real(0.0);
    auto result = run_success(vm.sqrt(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_double());
    EXPECT_DOUBLE_EQ(v.as_double(), 0.0);
}

TEST(NativeSqrt, NegativeInput_SpecBehavior)
{
    VM vm;
    Value arg = Value::from_real(-1.0);
    EXPECT_EQ(run_failed(vm.sqrt(1, &arg)).errc, ErrorCode::RUNTIME_ERROR);
}

TEST(NativeMathDispatch, SupportsDocumentedUnaryOperations)
{
    VM vm;
    Value sine_args[] = { str("sin"), Value::from_real(0.0) };
    Value exp_args[] = { str("exp"), Value::from_real(0.0) };
    EXPECT_DOUBLE_EQ(run_success(vm.math_unary(2, sine_args)).as_double(), 0.0);
    EXPECT_DOUBLE_EQ(run_success(vm.math_unary(2, exp_args)).as_double(), 1.0);
}

TEST(NativeMathDispatch, SupportsDocumentedBinaryOperations)
{
    VM vm;
    Value hypot_args[] = { str("hypot"), Value::from_int(3), Value::from_int(4) };
    Value atan_args[] = { str("atan2"), Value::from_real(0.0), Value::from_real(1.0) };
    EXPECT_DOUBLE_EQ(run_success(vm.math_binary(3, hypot_args)).as_double(), 5.0);
    EXPECT_DOUBLE_EQ(run_success(vm.math_binary(3, atan_args)).as_double(), 0.0);
}

TEST(NativeMathDispatch, RejectsBadTypesAndUnknownOperations)
{
    VM vm;
    Value bad_type[] = { str("sin"), str("not-ا-number") };
    Value unknown[] = { str("unknown"), Value::from_int(1) };
    EXPECT_EQ(run_failed(vm.math_unary(2, bad_type)).errc, ErrorCode::NATIVE_TYPE_ERROR);
    EXPECT_EQ(run_failed(vm.math_unary(2, unknown)).errc, ErrorCode::RUNTIME_ERROR);
    EXPECT_EQ(run_failed(vm.math_binary(0, nullptr)).errc, ErrorCode::NATIVE_ARG_COUNT);
}

TEST(NativeUrl, EncodesAndDecodesUtf8Bytes)
{
    VM vm;
    Value input = str("لغة فيروز/1");
    Value encoded = run_success(vm.url_encode(1, &input));
    ASSERT_TRUE(encoded.is_string());
    Value decoded = run_success(vm.url_decode(1, &encoded));
    ASSERT_TRUE(decoded.is_string());
    EXPECT_EQ(decoded.as_string()->str, input.as_string()->str);
    Value malformed = str("%GG");
    EXPECT_EQ(run_failed(vm.url_decode(1, &malformed)).errc, ErrorCode::RUNTIME_ERROR);
}

TEST(NativeUrl, ParsesAndRebuildsStructuredUrl)
{
    VM vm;
    Value input = str("https://example.test:443/api?q=1#part");
    Value parsed = run_success(vm.url_parse(1, &input));
    ASSERT_TRUE(parsed.is_dict());
    Value port = vm.dict_get(&parsed, str("port"));
    ASSERT_TRUE(port.is_int());
    EXPECT_EQ(port.as_int(), 443);
    Value rebuilt = run_success(vm.url_build(1, &parsed));
    ASSERT_TRUE(rebuilt.is_string());
    EXPECT_EQ(rebuilt.as_string()->str, input.as_string()->str);
}

TEST(NativeUrl, RejectsInvalidBoundaryArguments)
{
    VM vm;
    Value number = Value::from_int(42);
    EXPECT_EQ(run_failed(vm.url_encode(1, &number)).errc, ErrorCode::NATIVE_TYPE_ERROR);
    EXPECT_EQ(run_failed(vm.url_parse(1, &number)).errc, ErrorCode::NATIVE_TYPE_ERROR);
    EXPECT_EQ(run_failed(vm.url_build(1, &number)).errc, ErrorCode::NATIVE_TYPE_ERROR);
}

TEST(NativeRegex, CompilesSearchesMatchesAndCaptures)
{
    VM vm;
    Value compile_args[] = { str("([a-z])([0-9]+)"), Value::from_int(0) };
    Value handle = run_success(vm.regex_compile(2, compile_args));
    ASSERT_TRUE(handle.is_string());
    Value search_args[] = { handle, str("قبل a12 بعد"), Value::from_int(0) };
    Value found = run_success(vm.regex_search(3, search_args));
    ASSERT_TRUE(found.is_dict());
    EXPECT_EQ(vm.dict_get(&found, str("start")).as_int(), 4);
    Value groups = vm.dict_get(&found, str("groups"));
    ASSERT_TRUE(groups.is_list());
    ASSERT_EQ(groups.as_list()->elements.size(), 3u);
    EXPECT_EQ(groups.as_list()->elements[1].as_string()->str, StringRef("a"));

    Value match_args[] = { handle, str("a12 tail"), Value::from_int(0) };
    EXPECT_TRUE(run_success(vm.regex_match(3, match_args)).is_dict());
    Value full_args[] = { handle, str("a12") };
    EXPECT_TRUE(run_success(vm.regex_fullmatch(2, full_args)).is_dict());
}

TEST(NativeRegex, FindsAllSplitsAndReplacesWithLimits)
{
    VM vm;
    Value handle = str("[0-9]+");
    Value all_args[] = { handle, str("a1 b22 c333") };
    Value all = run_success(vm.regex_findall(2, all_args));
    ASSERT_TRUE(all.is_list());
    EXPECT_EQ(all.as_list()->elements.size(), 3u);

    Value split_args[] = { handle, str("a1b22c"), Value::from_int(1) };
    Value split = run_success(vm.regex_split(3, split_args));
    ASSERT_TRUE(split.is_list());
    EXPECT_EQ(split.as_list()->elements.size(), 2u);

    Value replace_args[] = { handle, str("a1b22c"), str("#"), Value::from_int(1) };
    Value replaced = run_success(vm.regex_replace(4, replace_args));
    ASSERT_TRUE(replaced.is_string());
    EXPECT_EQ(replaced.as_string()->str, StringRef("a#b22c"));
}

TEST(NativeRegex, RejectsInvalidPatternsAndArguments)
{
    VM vm;
    Value invalid[] = { str("["), Value::from_int(0) };
    EXPECT_EQ(run_failed(vm.regex_compile(2, invalid)).errc, ErrorCode::RUNTIME_ERROR);
    EXPECT_EQ(run_failed(vm.regex_search(0, nullptr)).errc, ErrorCode::NATIVE_ARG_COUNT);
    EXPECT_EQ(run_failed(vm.regex_split(0, nullptr)).errc, ErrorCode::NATIVE_ARG_COUNT);
    EXPECT_EQ(run_failed(vm.regex_replace(0, nullptr)).errc, ErrorCode::NATIVE_ARG_COUNT);
}

TEST(NativeSplit, BasicSplit)
{
    VM vm;
    Value args[] = { str("a,b,c"), str(",") };
    auto result = run_success(vm.split(2, args));
    Value v = result;
    ASSERT_TRUE(v.is_list());
    EXPECT_EQ(v.as_list()->elements.size(), 3u);

    ASSERT_TRUE(v.as_list()->elements[0].is_string());
    ASSERT_TRUE(v.as_list()->elements[1].is_string());
    ASSERT_TRUE(v.as_list()->elements[2].is_string());

    EXPECT_EQ(std::string(v.as_list()->elements[0].as_string()->str.data()), "a");
    EXPECT_EQ(std::string(v.as_list()->elements[1].as_string()->str.data()), "b");
    EXPECT_EQ(std::string(v.as_list()->elements[2].as_string()->str.data()), "c");
}

TEST(NativeSplit, NoDelimiterFound)
{
    VM vm;
    Value args[] = { str("hello"), str(",") };
    auto result = run_success(vm.split(2, args));
    Value v = result;
    ASSERT_TRUE(v.is_list());
    EXPECT_EQ(v.as_list()->elements.size(), 1u);
    ASSERT_TRUE(v.as_list()->elements[0].as_string());
    EXPECT_EQ(std::string(v.as_list()->elements[0].as_string()->str.data()), "hello");
}

TEST(NativeSubstr, BasicSubstr)
{
    VM vm; // substr is exclusive
    Value args[] = { str("hello"), Value::from_int(1), Value::from_int(4) };
    auto result = run_success(vm.substr(3, args));
    Value v = result;
    if (!v.is_nil()) {
        ASSERT_TRUE(v.is_string());
        EXPECT_EQ(std::string(v.as_string()->str.data()), "ell");
    }
}

TEST(NativeSubstr, FromStart)
{
    VM vm; // substr is exclusive
    Value args[] = { str("hello"), Value::from_int(0), Value::from_int(3) };
    auto result = run_success(vm.substr(3, args));
    Value v = result;
    if (!v.is_nil()) {
        ASSERT_TRUE(v.is_string());
        EXPECT_EQ(std::string(v.as_string()->str.data()), "hel");
    }
}

TEST(NativeContains, StringContains_True)
{
    VM vm;
    Value args[] = { str("hello world"), str("world") };
    auto result = run_success(vm.contains(2, args));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_TRUE(v.as_bool());
}

TEST(NativeContains, StringContains_False)
{
    VM vm;
    Value args[] = { str("hello"), str("xyz") };
    auto result = run_success(vm.contains(2, args));
    Value v = result;
    ASSERT_TRUE(v.is_bool());
    EXPECT_FALSE(v.as_bool());
}

TEST(NativeTrim, LeadingAndTrailingSpaces)
{
    VM vm;
    Value arg = str("  hello  ");
    auto result = run_success(vm.trim(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "hello");
}

TEST(NativeTrim, NoSpaces)
{
    VM vm;
    Value arg = str("hello");
    auto result = run_success(vm.trim(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "hello");
}

TEST(NativeTrim, OnlySpaces)
{
    VM vm;
    Value arg = str("   ");
    auto result = run_success(vm.trim(1, &arg));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "");
}

TEST(NativeJoin, BasicJoin)
{
    VM vm;
    Value list = run_success(vm.list(0, nullptr));
    ObjList* l = list.as_list();
    l->elements.push(str("a"));
    l->elements.push(str("b"));
    l->elements.push(str("c"));
    Value args[] = { list, str("|") };
    auto result = run_success(vm.join(2, args));
    Value v = result;
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(std::string(v.as_string()->str.data()), "a|b|c");
}

TEST(NativeAssert, TrueCondition_DoesNotCrash)
{
    VM vm;
    Value arg = Value::from_bool(true);
    run_success(vm.Assert(1, &arg));
}

TEST(NativeAssert, OptionalMessageIsNotTreatedAsASecondCondition)
{
    VM vm;
    Value args[] = { Value::from_bool(true), str("") };
    run_success(vm.Assert(2, args));
}

TEST(NativeClock, ReturnsFiniteMonotonicSeconds)
{
    VM vm;
    Value first = run_success(vm.clock(0, nullptr));
    Value second = run_success(vm.clock(0, nullptr));
    ASSERT_TRUE(first.is_double());
    ASSERT_TRUE(second.is_double());
    EXPECT_TRUE(std::isfinite(first.as_double()));
    EXPECT_GE(second.as_double(), first.as_double());
    EXPECT_TRUE(run_success(vm.clock(0, &first)).is_double());
    VMRunner runner;
    run_failed(runner.compile_and_run("ساعة(1)\n"));
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

    std::string test = (std::string("دالة اختبار():\n") + test_source::indent((std::string("فهرس := 0\n"
                                                                                           "خطوة := 1\n")
                            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n") + "طالما (فهرس < حد):\n"
                                                                                                  "    فهرس := (فهرس + خطوة)\n"
                                                                                                  "ارجع فهرس\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
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

    std::string test = (std::string("دالة اختبار():\n") + test_source::indent((std::string("فهرس := 0.0\n"
                                                                                           "خطوة := 1.0\n")
                            + (std::string((std::string("حد := ") + test_source::floating(static_cast<f64>(N)))) + "\n") + "طالما (فهرس < حد):\n"
                                                                                                                           "    فهرس := (فهرس + خطوة)\n"
                                                                                                                           "ارجع فهرس\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });
    run_success(r.run());

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_TRUE(result.is_double());
    std::printf("  Dispatch FloatAdd loop %dk iters:  %.1f µs  (%.2f ns/op)\n",
        N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Global variable lookup throughput
//    STORE_GLOBAL + LOAD_GLOBAL in a loop — exercises the globals hash map.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(VMPerfTest, GlobalLookup_1M_Roundtrips)
{
    constexpr int N = 1'000'000;

    std::string decl = "ا := 0\n";
    std::string while_loop = (std::string("طالما ") + (std::string("(ا < ") + std::to_string(N) + ")") + ":\n"
                                                                                                         "    ا := (ا + 1)\n");

    VMRunner r;
    r.compile({ decl, while_loop });

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    ::printf("  Global store+load %dk roundtrips:  %.1f µs  (%.2f ns/op)\n", N / 1000, us, us * 1000.0 / N);
}

TEST_F(VMPerfTest, CallOverhead_100k_Calls)
{
    constexpr int N = 100;

    std::string m_body = (std::string("فهرس := 0\n") + (std::string((std::string("حد := ") + std::to_string(N))) + "\n") + "طالما (فهرس < حد):\n"
                                                                                                                           "    اجمع(1, 2)\n"
                                                                                                                           "    فهرس := (فهرس + 1)\n"
                                                                                                                           "ارجع فهرس\n");

    std::string func = (std::string("دالة اختبار():\n") + test_source::indent(m_body));

    std::string add = "دالة اجمع(ا, ب):\n"
                      "    ارجع (ا + ب)\n";
    std::string call = "اختبار()\n";

    std::cout << "AUTO:" << '\n';
    VMRunner r;
    r.compile({ add, func, call });
    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
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

    std::string tc_fn = "دالة استدعاء_ذيلي(ن):\n"
                        "    اذا (ن = 0):\n"
                        "        ارجع ن\n"
                        "    ارجع استدعاء_ذيلي((ن - 1))\n";

    VMRunner tc;
    tc.compile({ tc_fn, (std::string((std::string("استدعاء_ذيلي(") + (std::string(std::to_string(DEPTH))) + ")")) + "\n") });

    auto t0 = std::chrono::high_resolution_clock::now();
    Value tc_result = run_success(tc.run());
    f64 tc_us = microseconds_since(t0);

    // ── equivalent iterative loop (no calls) ─────────────────────────────
    std::string loop_fn = (std::string("دالة حلقة():\n") + test_source::indent((std::string((std::string((std::string("ن := ") + std::to_string(DEPTH))) + "\n")) + "طالما (ن != 0):\n"
                                                                                                                                                                    "    ن := (ن - 1)\n"
                                                                                                                                                                    "ارجع ن\n")));

    VMRunner loop;
    loop.compile({ loop_fn, "حلقة()\n" });
    t0 = std::chrono::high_resolution_clock::now();
    Value loop_result = run_success(loop.run());
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
    std::string test = (std::string("دالة اختبار():\n") + test_source::indent((std::string("قائمة_قيم := []\n"
                                                                                           "فهرس := 0\n")
                            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n") + "طالما (فهرس < حد):\n"
                                                                                                  "    اضف(قائمة_قيم, فهرس)\n"
                                                                                                  "    فهرس := (فهرس + 1)\n"
                                                                                                  "فهرس := 0\n"
                                                                                                  "مجموع := 0\n"
                                                                                                  "طالما (فهرس < حد):\n"
                                                                                                  "    مجموع := (مجموع + قائمة_قيم[فهرس])\n"
                                                                                                  "    فهرس := (فهرس + 1)\n"
                                                                                                  "ارجع مجموع\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });
    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
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

    std::string test = (std::string("دالة اختبار():\n") + test_source::indent((std::string("نص := \"hello world\"\n"
                                                                                           "فهرس := 0\n")
                            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n") + "اخير := 0\n"
                                                                                                  "طالما (فهرس < حد):\n"
                                                                                                  "    اخير := طول(نص)\n"
                                                                                                  "    فهرس := (فهرس + 1)\n"
                                                                                                  "ارجع اخير\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });

    // Cold run to prime the IC.
    run_success(r.run());

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
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

static Array<std::string> make_fib_program(int n, int reps)
{
    std::string fib = "دالة فيبوناتشي(س):\n"
                      "    اذا (س <= 1):\n"
                      "        ارجع س\n"
                      "    ارجع (فيبوناتشي((س - 1)) + فيبوناتشي((س - 2)))\n";

    std::string test = (std::string("دالة اختبار():\n") + test_source::indent((std::string("فهرس := 0\n") + (std::string((std::string("حد := ") + std::to_string(reps))) + "\n") + "ناتج := 0\n" + (std::string("طالما (فهرس < حد):\n") + test_source::indent((std::string((std::string((std::string("ناتج := ") + (std::string("فيبوناتشي(") + (std::string(std::to_string(n))) + ")"))) + "\n")) + "فهرس := (فهرس + 1)\n"))) + "ارجع ناتج\n")));

    return { fib, test, "اختبار()\n" };
}

TEST_F(VMPerfTest, Fib20_100reps)
{
    constexpr int FIB_N = 20;
    constexpr int REPS = 100;

    VMRunner r;
    r.compile(make_fib_program(FIB_N, REPS));

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 6765); // fib(20)
    ::printf("  fib(%d) × %d reps:                  %.1f µs  (%.1f µs/call)\n", FIB_N, REPS, us, us / REPS);
}

TEST_F(VMPerfTest, Fib25_10reps)
{
    constexpr int FIB_N = 25;
    constexpr int REPS = 10;

    VMRunner r;
    r.compile(make_fib_program(FIB_N, REPS));

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
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

    std::string test = (std::string("دالة اختبار():\n") + test_source::indent((std::string("فهرس := 0\n"
                                                                                           "خطوة := 1\n")
                            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n") + "طالما (فهرس < حد):\n"
                                                                                                  "    فهرس := (فهرس + خطوة)\n"
                                                                                                  "ارجع فهرس\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });
    run_success(r.run()); // warm quickened arithmetic path

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), N);
    std::printf("  STRESS IntAdd loop %dM iters:       %.1f µs  (%.2f ns/op)\n",
        N / 1'000'000, us, us * 1000.0 / N);
}

TEST_F(VMPerfTest, NativeCall_Len_1M_ICHot)
{
    constexpr int N = 1'000'000;

    std::string test = (std::string("دالة اختبار():\n") + test_source::indent((std::string("نص := \"hello world\"\n"
                                                                                           "فهرس := 0\n")
                            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n") + "اخير := 0\n"
                                                                                                  "طالما (فهرس < حد):\n"
                                                                                                  "    اخير := طول(نص)\n"
                                                                                                  "    فهرس := (فهرس + 1)\n"
                                                                                                  "ارجع اخير\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });
    run_success(r.run()); // warm IC_CALL -> CALL rewrite

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 11);
    std::printf("  STRESS native len() %dM calls:      %.1f µs  (%.2f ns/call)\n",
        N / 1'000'000, us, us * 1000.0 / N);
}

TEST_F(VMPerfTest, List_AppendAndSum_100k)
{
    constexpr int N = 100'000;

    std::string test = (std::string("دالة اختبار():\n") + test_source::indent((std::string("قائمة_قيم := []\n"
                                                                                           "فهرس := 0\n")
                            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n") + "طالما (فهرس < حد):\n"
                                                                                                  "    اضف(قائمة_قيم, فهرس)\n"
                                                                                                  "    فهرس := (فهرس + 1)\n"
                                                                                                  "فهرس := 0\n"
                                                                                                  "مجموع := 0\n"
                                                                                                  "طالما (فهرس < حد):\n"
                                                                                                  "    مجموع := (مجموع + قائمة_قيم[فهرس])\n"
                                                                                                  "    فهرس := (فهرس + 1)\n"
                                                                                                  "ارجع مجموع\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
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

    VMRunner r;
    r.compile(make_fib_program(FIB_N, REPS));
    run_success(r.run()); // warm global lookup and call-site rewriting

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 317811);
    std::printf("  STRESS fib(%d) × %d reps hot:       %.1f µs  (%.1f µs/call)\n",
        FIB_N, REPS, us, us / REPS);
}

TEST(VMClass, ConstructorRejectsWrongArgumentCount)
{
    std::string ast = "نوع يتطلب_وسيطة:\n"
                      "    دالة بداية(وسيطة): ارجع\n"
                      "دالة اختبار():\n"
                      "    كائن := يتطلب_وسيطة()\n"
                      "    ارجع كائن\n"
                      "اختبار()\n";

    VMRunner r;
    run_failed(r.compile_and_run(ast));
}

TEST(VMClass, ConstructorInitializesFieldsFromParameters)
{
    std::string ast = "نوع نقطة:\n"
                      "    دالة بداية(س, ص):\n"
                      "        .س := س\n"
                      "        .ص := ص\n"
                      "دالة اختبار():\n"
                      "    النقطة := نقطة(3, 4)\n"
                      "    ارجع النقطة\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
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
    std::string ast = "نوع علبة:\n"
                      "    دالة بداية(قيمة): .قيمة := قيمة\n"
                      "دالة اختبار():\n"
                      "    العلبة := علبة(12)\n"
                      "    ارجع العلبة.قيمة\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 12);
}

TEST(VMClass, MethodReceivesExplicitArguments)
{
    std::string ast = "نوع جامع:\n"
                      "    دالة اجمع(ا, ب): ارجع ا + ب\n"
                      "دالة اختبار():\n"
                      "    الجامع := جامع()\n"
                      "    ارجع الجامع.اجمع(2, 5)\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 7);
}

TEST(VMClass, MethodReadsInstanceField)
{
    std::string ast = "نوع علبة:\n"
                      "    دالة بداية(قيمة): .قيمة := قيمة\n"
                      "    دالة اقرا_قيمة(): ارجع هذا.قيمة\n"
                      "دالة اختبار():\n"
                      "    العلبة := علبة(31)\n"
                      "    ارجع العلبة.اقرا_قيمة()\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 31);
}

TEST(VMClass, MethodMutatesInstanceFieldAndPersists)
{
    std::string ast = "نوع عداد:\n"
                      "    دالة بداية(): .عدد := 0\n"
                      "    دالة زد():\n"
                      "        .عدد := هذا.عدد + 1\n"
                      "        ارجع هذا.عدد\n"
                      "دالة اختبار():\n"
                      "    العداد := عداد()\n"
                      "    العداد.زد()\n"
                      "    ارجع العداد.زد()\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST(VMClass, MultipleInstancesKeepIndependentFieldState)
{
    std::string ast = "نوع علبة:\n"
                      "    دالة بداية(قيمة): .قيمة := قيمة\n"
                      "    دالة اقرا_قيمة(): ارجع هذا.قيمة\n"
                      "دالة اختبار():\n"
                      "    يسار := علبة(10)\n"
                      "    يمين := علبة(20)\n"
                      "    ارجع يسار.اقرا_قيمة() + يمين.اقرا_قيمة()\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 30);
}

TEST(VMClass, MethodReturningNoValueReturnsSelf)
{
    std::string ast = "نوع متسلسل:\n"
                      "    دالة المس(): عدم\n"
                      "دالة اختبار():\n"
                      "    المتسلسل := متسلسل()\n"
                      "    ارجع المتسلسل.المس()\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_instance());
    EXPECT_EQ(result.as_instance()->klass->name, "متسلسل");
}

TEST(VMClass, MethodRejectsWrongArgumentCount)
{
    std::string ast = "نوع جامع:\n"
                      "    دالة اجمع(قيمة): ارجع قيمة\n"
                      "دالة اختبار():\n"
                      "    الجامع := جامع()\n"
                      "    ارجع الجامع.اجمع()\n"
                      "اختبار()\n";

    VMRunner r;
    run_failed(r.compile_and_run(ast));
}

TEST(VMClass, MultipleMethodsAreStoredInRuntimeClass)
{
    std::string ast = "نوع عمليات:\n"
                      "    دالة اول(): ارجع 1\n"
                      "    دالة ثان(): ارجع 2\n"
                      "دالة اختبار(): ارجع عمليات\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_class());

    ObjClass* klass_obj = result.as_class();
    EXPECT_GE(klass_obj->method_names.size(), static_cast<u32>(ObjClass::_COUNT + 2));
    EXPECT_GE(klass_obj->method_slot("اول"), 0);
    EXPECT_GE(klass_obj->method_slot("ثان"), 0);
}

TEST(VMClass, AddSpecialMethodHandlesBinaryPlus)
{
    std::string ast = "نوع شبيه_عدد:\n"
                      "    دالة عملية+(اخر): ارجع 99\n"
                      "دالة اختبار():\n"
                      "    يسار := شبيه_عدد()\n"
                      "    يمين := شبيه_عدد()\n"
                      "    ارجع يسار + يمين\n"
                      "اختبار()\n";

    VMRunner r;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(r.compile_and_run(ast)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 99);
}
