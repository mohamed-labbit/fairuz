//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerList, EmptyList)
{
    Chunk* chunk = compile_ok("[]\n");
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    BytecodeChecker bc(*chunk);
    bc.next("LIST_NEW").op(OpCode::LIST_NEW).A(1).B(0);
    bc.next("MOVE").op(OpCode::MOVE).A(0).B(1);
    bc.next("RETURN_NIL").op(OpCode::RETURN_NIL);
    bc.done();
}

TEST(CompilerList, ListWithElements)
{
    Array<std::string> elems = { "1", "2", "3" };
    Chunk* chunk
        = compile_ok((std::string((std::string("[") + test_source::join(std::move(elems), ", ") + "]")) + "\n"));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
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
