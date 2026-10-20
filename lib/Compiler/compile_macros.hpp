//===----------------------------------------------------------------------===//
//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Defines compiler macros.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_LIB_COMPILER_COMPILE_MACROS_HPP
#define FAIRUZ_LIB_COMPILER_COMPILE_MACROS_HPP

#define FA_COMPILE_JOIN_I(a, b) a##b
#define FA_COMPILE_JOIN(a, b) FA_COMPILE_JOIN_I(a, b)

#define VERIFY_RESULT(r)                                   \
    do {                                                   \
        if (UNLIKELY((r).has_error())) return (r).error(); \
    } while (0)

/// declares a new variable
#define TRY_BIND(name, expr) \
    auto _r = (expr);        \
    VERIFY_RESULT(_r);       \
    auto name = _r.value();

/// sets an existing variable
#define TRY_SET(out, expr)   \
    do {                     \
        auto _r = (expr);    \
        VERIFY_RESULT(_r);   \
        *(out) = _r.value(); \
    } while (0)

#define TRY_DISCARD(expr)  \
    do {                   \
        auto _r = (expr);  \
        VERIFY_RESULT(_r); \
        (void)_r;          \
    } while (0)

#define FA_COMPILE_TRY(name, expr)                                                                                     \
    auto FA_COMPILE_JOIN(parse_result_, __LINE__) = (expr);                                                            \
    if (FA_COMPILE_JOIN(parse_result_, __LINE__).has_error()) return FA_COMPILE_JOIN(parse_result_, __LINE__).error(); \
    auto name = std::move(FA_COMPILE_JOIN(parse_result_, __LINE__)).value()

#define FA_COMPILE_DISCARD_TRY(expr)                        \
    auto FA_COMPILE_JOIN(parse_result_, __LINE__) = (expr); \
    if (FA_COMPILE_JOIN(parse_result_, __LINE__).has_error()) return FA_COMPILE_JOIN(parse_result_, __LINE__).error();

#define ANY_REG_BIND(v, l, name) FA_COMPILE_TRY(name, any_reg((v), (l)))
#define ANY_REG_SET(v, l, name) TRY_SET(name, any_reg((v), (l)))
#define ALLOC_REG(name, loc) FA_COMPILE_TRY(name, alloc_register(loc))

#endif // FAIRUZ_LIB_COMPILER_COMPILE_MACROS_HPP
