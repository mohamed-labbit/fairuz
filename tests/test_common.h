#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "../fairuz/fAST.hpp"
#include "../fairuz/fgc.hpp"
#include "farena.hpp"
#include "fstring.hpp"
#include <functional>

using namespace fairuz;
using namespace fairuz::AST;
using namespace fairuz::runtime;

/// helpers that discard the location parameter for testing
static inline BinaryExpr* binary(Expr* lhs, Expr* rhs, BinaryOp op)
{
    return make_binary(lhs, rhs, op, { });
}
static inline UnaryExpr* unary(Expr* operand, UnaryOp op)
{
    return make_unary(operand, op, { });
}
static inline LiteralExpr* lit_nil()
{
    return make_literal_nil({ });
}
static inline LiteralExpr* lit_int(int v)
{
    return make_literal_int(v, { });
}
static inline LiteralExpr* lit_flt(double v)
{
    return make_literal_float(v, { });
}
static inline LiteralExpr* lit_str(StringRef s)
{
    return make_literal_string(s, { });
}
static inline LiteralExpr* lit_bool(bool v)
{
    return make_literal_bool(v, { });
}
static inline NameExpr* name_expr(StringRef s)
{
    return make_name(s, { });
}
static inline ListExpr* list_expr(Array<Expr*> l = { })
{
    return make_list(l, { });
}
static inline DictExpr* dict_expr(Array<std::pair<Expr*, Expr*>> c)
{
    return make_dict(c, { });
}
static inline CallExpr* call_expr(Expr* c, ListExpr* a = nullptr)
{
    return make_call(c, a, { });
}
static inline AssignmentExpr* assign_expr(Expr* t, Expr* v)
{
    return make_assignment_expr(t, v, { });
}
static inline IndexExpr* index_expr(Expr* obj, Expr* idx)
{
    return make_index(obj, idx, { });
}
static inline GetExpr* get_expr(Expr* obj, Expr* member)
{
    return make_get_expr(obj, member, { });
}
static inline BlockStmt* blk(Array<Stmt*> stmts)
{
    return make_block(stmts, { });
}
static inline ExprStmt* expr_stmt(Expr* e)
{
    return make_expr_stmt(e, { });
}
static inline AssignmentStmt* assign_stmt(Expr* t, Expr* v)
{
    return make_assignment_stmt(t, v, { });
}
static inline AssignmentStmt* decl_stmt(StringRef nm, AST::Expr* val)
{
    return make_assignment_stmt(name_expr(nm), val, { });
}
static inline IfStmt* if_stmt(Expr* c, Stmt* t, Stmt* e = nullptr)
{
    return make_if(c, t, { }, e);
}
static inline WhileStmt* while_stmt(Expr* c, Stmt* b)
{
    return make_while(c, b, { });
}
static inline ForStmt* for_stmt(NameExpr* t, Expr* i, Stmt* b)
{
    return make_for(t, i, b, { });
}
static inline FunctionDef* func_def(NameExpr* n, ListExpr* p, Stmt* b)
{
    return make_function(n, p, b, { });
}
static inline ReturnStmt* return_stmt(Expr* v = nullptr)
{
    return make_return({ }, v);
}
static inline ClassDef* class_def(Expr* n, Array<Expr*> mm, Array<Stmt*> me)
{
    return make_class_def(n, mm, me, { });
}
static inline BreakStmt* break_stmt()
{
    return make_break({ });
}
static inline ContinueStmt* continue_stmt()
{
    return make_continue({ });
}

static inline Value str(char const* s) {
    ObjString* obj =  get_allocator().allocate_object<ObjString>();
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

#define REQUIRE_PERF()                                                                                   \
    do {                                                                                                 \
        if (!require_perf())                                                                             \
            GTEST_SKIP(); \
    } while (0);


#endif // TEST_COMMON_H
