//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_TESTS_TEST_INTERPRETER_TEST_INTERPRETER_COMMON_HPP
#define FAIRUZ_TESTS_TEST_INTERPRETER_TEST_INTERPRETER_COMMON_HPP

#include "Interpreter/vm.hpp"
#include "test_common.h"
#include "test_config.h"
#include "test_source_program.hpp"

using namespace fairuz;
using namespace fairuz::runtime;

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
    default: return { };
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
        if (ch) ch->disassemble();
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

#endif // FAIRUZ_TESTS_TEST_INTERPRETER_TEST_INTERPRETER_COMMON_HPP
