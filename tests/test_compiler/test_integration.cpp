//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_compiler_common.h"

TEST(CompilerIntegration, Fibonacci)
{
    Array<std::string> args_n1, args_n2;
    args_n1.push("(ن - 1)");
    args_n2.push("(ن - 2)");

    Chunk* chunk = compile_ok((std::string("دالة فيبوناتشي(ن):\n")
        + test_source::indent((std::string("اذا (ن <= 1):\n"
                                           "    ارجع ن\n")
            + (std::string("ارجع")
                + (std::string(" ")
                    + (std::string("(") + (std::string("فيبوناتشي(") + test_source::join(args_n1, ", ") + ")") + " + "
                        + (std::string("فيبوناتشي(") + test_source::join(args_n2, ", ") + ")") + ")"))
                + "\n")))));

    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    EXPECT_FALSE(chunk->functions.empty());
    Chunk const* fib = chunk->functions[0];
    EXPECT_EQ(fib->name, "فيبوناتشي");
    EXPECT_EQ(fib->arity, 1);
    EXPECT_FALSE(fib->code.empty());
    EXPECT_GE(fib->ic_slots.size(), 2u);
}

TEST(CompilerIntegration, MixedLiteralsInList)
{
    Array<std::string> elems = { "صحيح", "42", "3.14", "\"mohamed\"" };
    Chunk* chunk
        = compile_ok((std::string((std::string("[") + test_source::join(std::move(elems), ", ") + "]")) + "\n"));
    ASSERT_NE(chunk, nullptr);
    if (test_config::dump_bytecode) dump(chunk);
    int appends = 0;
    for (auto& ins : chunk->code) {
        if (instr_op(ins) == OpCode::LIST_APPEND) appends++;
    }
    EXPECT_EQ(appends, 4);
}
