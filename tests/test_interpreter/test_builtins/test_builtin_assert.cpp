//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "../test_interpreter_common.h"

TEST(BuiltinAssert, TrueCondition_DoesNotCrash)
{
    VM vm;
    Value arg = Value::from_bool(true);
    run_success(vm.Assert(1, &arg));
}

TEST(BuiltinAssert, OptionalMessageIsNotTreatedAsASecondCondition)
{
    VM vm;
    Value args[] = { Value::from_bool(true), str("") };
    run_success(vm.Assert(2, args));
}
