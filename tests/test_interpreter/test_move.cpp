//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

TEST(VMMove, Copies)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 77).mov(1, 0).ret(1);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_TRUE(v.is_int());
    EXPECT_EQ(v.as_int(), 77);
}

TEST(VMMove, SourceUnchanged)
{
    VMRunner r;
    CB b;
    b.regs(2).load_int(0, 55).mov(1, 0).ret(0);
    if (test_config::dump_bytecode) b.dump();
    Value v = run_success(r.run(b));
    EXPECT_EQ(v.as_int(), 55);
}
