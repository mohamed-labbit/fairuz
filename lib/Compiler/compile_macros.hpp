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

#define VERIFY_GENERIC_RESULT(r)                      \
    do {                                              \
        if (UNLIKELY((r).has_error())) {              \
            auto error = (r).error();                 \
            auto errc = error.errc;                   \
            auto errloc = error.loc;                  \
            return raise_compile_error(errc, errloc); \
        }                                             \
    } while (0)

#define VERIFY_COMPILE_RESULT(r)       \
    do {                               \
        if (UNLIKELY((r).has_error())) \
            return (r).error();        \
    } while (0)

#define TRY_COMPILE_ASSIGN(out, expr) \
    do {                              \
        auto _r = (expr);             \
        VERIFY_COMPILE_RESULT(_r);    \
        *(out) = _r.value();          \
    } while (0)

#define TRY_GENERIC_ASSIGN(out, expr) \
    do {                              \
        auto _r = (expr);             \
        VERIFY_GENERIC_RESULT(_r);    \
        *(out) = _r.value();          \
    } while (0)

#define TRY_DISCARD(expr)          \
    do {                           \
        auto _r = (expr);          \
        VERIFY_COMPILE_RESULT(_r); \
        (void)_r;                  \
    } while (0)

#define COMPILE_STMT_DISCARD(s) TRY_DISCARD(compile_stmt(s))
#define COMPILE_EXPR_IMPL(e, r) TRY_COMPILE_ASSIGN(r, compile_expr_impl(e))
#define ANY_REG(v, l, out) TRY_GENERIC_ASSIGN(out, any_reg((v), (l)))
#define ALLOC_REG(out) TRY_GENERIC_ASSIGN(out, alloc_register())

#endif // FAIRUZ_LIB_COMPILER_COMPILE_MACROS_HPP
