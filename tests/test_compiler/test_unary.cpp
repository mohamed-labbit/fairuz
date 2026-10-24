//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerUnary, NegateVariable)
{
    Chunk* ch = compile_ok_local({
        "س := 5\n",
        "(-س)\n",
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
        "ب := صحيح\n",
        "(ليس ب)\n",
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
        (std::string("ن := ") + std::to_string(0xFF) + "\n"),
        "(~ن)\n",
    });

    BytecodeChecker bc(*ch);
    bc.next("decl_stmt").op(OpCode::LOAD_INT).A(0);
    bc.next("OP_BITNOT").op(OpCode::OP_BITNOT).B(0);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}
