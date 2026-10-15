//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Provides shared GoogleTest fixtures and configuration.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_TESTS_TEST_COMMON_H
#define FAIRUZ_TESTS_TEST_COMMON_H

#include "Interpreter/object.hpp"
#include "Parser/ast.hpp"
#include "Support/array.hpp"
#include "test_runtime.hpp"

#include <initializer_list>

using namespace fairuz;
using namespace fairuz::AST;
using namespace fairuz::runtime;

static inline Value str(char const* s)
{
    ObjString* obj = get_allocator().allocate_object<ObjString>();
    obj->str = s;
    obj->hash = std::hash<StringRef>()(obj->str);
    return Value::from_string(obj);
}

static inline bool require_perf()
{
    if (auto v = std::getenv("TEST_PERF")) {
        std::string s { v };
        return s == "1" || s == "true";
    }
    return false;
}

#define REQUIRE_PERF()       \
    do {                     \
        if (!require_perf()) \
            GTEST_SKIP();    \
    } while (0);

#endif // FAIRUZ_TESTS_TEST_COMMON_H
