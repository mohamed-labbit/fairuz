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
#include "Parser/ast_builder.hpp"
#include "Support/array.hpp"

#include <initializer_list>

using namespace fairuz;
using namespace fairuz::AST;
using namespace fairuz::runtime;

/// helpers that discard the location parameter for testing
static inline BinaryExpr* binary(ExprPtr lhs, ExprPtr rhs, Expr::Kind op)
{
    return AST::ASTBuilder().make_binary(op, lhs, rhs, { });
}
static inline UnaryExpr* unary(ExprPtr operand, Expr::Kind op)
{
    return AST::ASTBuilder().make_unary(op, operand, { });
}
static inline NilExpr* nil()
{
    return AST::ASTBuilder().make_nil({ });
}
static inline IntLiteralExpr* lit_int(int v)
{
    return AST::ASTBuilder().make_literal_int(v, { });
}
static inline IntLiteralExpr* lit_int(i64 v)
{
    return AST::ASTBuilder().make_literal_int(v, { });
}
static inline FloatLiteralExpr* lit_flt(double v)
{
    return AST::ASTBuilder().make_literal_float(v, { });
}
static inline StringLiteralExpr* lit_str(StringRef const s)
{
    return AST::ASTBuilder().make_literal_string(s, { });
}
static inline BoolLiteralExpr* lit_bool(bool v)
{
    return AST::ASTBuilder().make_literal_bool(v, { });
}
static inline IdentifierExpr* ident(StringRef const s)
{
    return AST::ASTBuilder().make_identifier(s, { });
}
static inline ListExpr* list_expr(Array<ExprPtr> l = { })
{
    return AST::ASTBuilder().make_list(l, { });
}
static inline DictExpr* dict_expr(Array<std::pair<ExprPtr, ExprPtr>> c)
{
    return AST::ASTBuilder().make_dict(c, { });
}
static inline CallExpr* call_expr(ExprPtr c, Array<ExprPtr> a = { })
{
    return AST::ASTBuilder().make_call(c, a, { });
}
static inline AssignExpr* assign_expr(ExprPtr t, ExprPtr v)
{
    return AST::ASTBuilder().make_assignment_expr(t, v, { });
}
static inline IndexExpr* index_expr(ExprPtr obj, ExprPtr idx)
{
    return AST::ASTBuilder().make_index(obj, idx, { });
}
static inline GetExpr* get_expr(ExprPtr obj, IdentifierExpr* member)
{
    return AST::ASTBuilder().make_get_expr(obj, member, { });
}
static inline BlockStmt* blk(std::initializer_list<StmtPtr> stmts)
{
    return AST::ASTBuilder().make_block(Array<StmtPtr>(stmts), { });
}
static inline BlockStmt* blk(Array<StmtPtr> stmts)
{
    return AST::ASTBuilder().make_block(stmts, { });
}
static inline ExprStmt* expr_stmt(ExprPtr e)
{
    return AST::ASTBuilder().make_expr_stmt(e, { });
}
static inline ExprStmt* decl_stmt(StringRef const nm, AST::ExprPtr val)
{
    return AST::ASTBuilder().make_expr_stmt(AST::ASTBuilder().make_assignment_expr(ident(nm), val, { }), { });
}
static inline IfElseStmt* if_stmt(ExprPtr c, StmtPtr t, StmtPtr e = nullptr)
{
    return AST::ASTBuilder().make_if(c, t, { }, e);
}
static inline WhileStmt* while_stmt(ExprPtr c, StmtPtr b)
{
    return AST::ASTBuilder().make_while(c, b, { });
}
static inline ForStmt* for_stmt(IdentifierExpr* t, ExprPtr i, StmtPtr b)
{
    return AST::ASTBuilder().make_for(t, i, b, { });
}
static inline FuncDefStmt* func_def(IdentifierExpr* n, Array<ExprPtr> p, StmtPtr b)
{
    return AST::ASTBuilder().make_function(n, p, b, { });
}
static inline ReturnStmt* return_stmt(ExprPtr v = nullptr)
{
    return AST::ASTBuilder().make_return({ }, v);
}
static inline ClassDefStmt* class_def(ExprPtr n, std::initializer_list<ExprPtr> mm, std::initializer_list<StmtPtr> me)
{
    return AST::ASTBuilder().make_class_def(n, Array<ExprPtr>(mm), Array<StmtPtr>(me), { });
}
static inline BreakStmt* break_stmt()
{
    return AST::ASTBuilder().make_break({ });
}
static inline ContinueStmt* continue_stmt()
{
    return AST::ASTBuilder().make_continue({ });
}
static inline ExprStmt* call_stmt(StringRef const s)
{
    return expr_stmt(call_expr(ident(s)));
}
static inline ImportStmt* imp(StringRef const module, Array<StringRef> name,
    Array<StringRef> aliases)
{
    return AST::ASTBuilder().make_import(module, name, aliases, { });
}

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
