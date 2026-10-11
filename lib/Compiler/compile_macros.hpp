#pragma once

#define VERIFY_RESULT(r)               \
    do {                               \
        if (UNLIKELY((r).has_error())) \
            return (r).error();        \
    } while (0)

#define TRY_ASSIGN(out, expr) \
    do {                      \
        auto _r = (expr);     \
        VERIFY_RESULT(_r);    \
        *(out) = _r.value();  \
    } while (0)

#define TRY_DISCARD(expr)  \
    do {                   \
        auto _r = (expr);  \
        VERIFY_RESULT(_r); \
        (void)_r;          \
    } while (0)

#define COMPILE_STMT_DISCARD(s) TRY_DISCARD(compile_stmt(s))
#define COMPILE_EXPR_IMPL(e, r) TRY_ASSIGN(r, compile_expr_impl(e))
#define ANY_REG(v, l, out) TRY_ASSIGN(out, any_reg((v), (l)))
#define ALLOC_REG(out) TRY_ASSIGN(out, alloc_register())
