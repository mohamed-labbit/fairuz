//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_TESTS_COMPILER_TEST_COMPILER_COMMON_HPP
#define FAIRUZ_TESTS_COMPILER_TEST_COMPILER_COMMON_HPP

#include "../test_source_program.hpp"
#include "Compiler/compiler.hpp"

#include <gtest/gtest.h>

using namespace fairuz;
using namespace fairuz::runtime;

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
        EXPECT_EQ(pos_, chunk_.code.size()) << "expected end of code at instruction " << pos_ << " but "
                                            << (chunk_.code.size() - pos_) << " more remain";
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

static Chunk* compile_ok(std::string const& source)
{
    Chunk* chunk = test_source::compile(source);
    EXPECT_FALSE(Compiler::compile_all(chunk).has_error());
    return chunk;
}

static Chunk* compile_ok(Array<std::string> const& source) { return compile_ok(test_source::join(source)); }

static Chunk* compile_ok(std::initializer_list<std::string> source) { return compile_ok(Array<std::string>(source)); }

static Chunk* compile_ok_local(Array<std::string> const& source)
{
    auto chunk = compile_ok("دالة دالة_الفحص():\n" + test_source::indent(test_source::join(source)));
    EXPECT_EQ(chunk->functions.size(), 1u);
    return chunk->functions[0];
}

static Chunk* compile_fail(std::string const& source)
{
    test_source::Program program(source);
    run_failed(program.compile());
    return nullptr;
}

static Chunk* compile_fail(Array<std::string> const& source) { return compile_fail(test_source::join(source)); }

static void dump(Chunk const* chunk) { chunk->disassemble(); }

static u16 load_int_bx(i64 i) { return static_cast<u16>(i + 32767); }

#endif // FAIRUZ_TESTS_COMPILER_TEST_COMPILER_COMMON_HPP
