#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "../fairuz/ast.hpp"

using namespace fairuz;
using namespace fairuz::AST;

/// helpers that discard the location parameter for testing
static Fa_BinaryExpr* binary(Fa_Expr* lhs, Fa_Expr* rhs, Fa_BinaryOp op) { return Fa_make_binary(lhs, rhs, op, { }); }
static Fa_UnaryExpr* unary(Fa_Expr* operand, Fa_UnaryOp op) { return Fa_make_unary(operand, op, { }); }
static Fa_LiteralExpr* lit_nil() { return Fa_make_literal_nil({ }); }
static Fa_LiteralExpr* lit_int(int v) { return Fa_make_literal_int(v, { }); }
static Fa_LiteralExpr* lit_flt(double v) { return Fa_make_literal_float(v, { }); }
static Fa_LiteralExpr* lit_str(Fa_StringRef s) { return Fa_make_literal_string(s, { }); }
static Fa_LiteralExpr* lit_bool(bool v) { return Fa_make_literal_bool(v, { }); }
static Fa_NameExpr* name_expr(Fa_StringRef s) { return Fa_make_name(s, { }); }
static Fa_ListExpr* list_expr(Fa_Array<Fa_Expr*> l = { }) { return Fa_make_list(l, { }); }
static Fa_DictExpr* dict_expr(Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> c) { return Fa_make_dict(c, { }); }
static Fa_CallExpr* call_expr(Fa_Expr* c, Fa_ListExpr* a = nullptr) { return Fa_make_call(c, a, { }); }
static Fa_AssignmentExpr* assign_expr(Fa_Expr* t, Fa_Expr* v) { return Fa_make_assignment_expr(t, v, { }, false); }
static Fa_IndexExpr* index_expr(Fa_Expr* obj, Fa_Expr* idx) { return Fa_make_index(obj, idx, { }); }
static Fa_BlockStmt* blk(Fa_Array<Fa_Stmt*> stmts) { return Fa_make_block(stmts, { }); }
static Fa_ExprStmt* expr_stmt(Fa_Expr* e) { return Fa_make_expr_stmt(e, { }); }
static Fa_AssignmentStmt* assign_stmt(Fa_Expr* t, Fa_Expr* v) { return Fa_make_assignment_stmt(t, v, { }, false); }
static AST::Fa_AssignmentStmt* decl_stmt(Fa_StringRef nm, AST::Fa_Expr* val) { return Fa_make_assignment_stmt(name_expr(nm), val, { }, true); }
static Fa_IfStmt* if_stmt(Fa_Expr* c, Fa_Stmt* t, Fa_Stmt* e = nullptr) { return Fa_make_if(c, t, { }, e); }
static Fa_WhileStmt* while_stmt(Fa_Expr* c, Fa_Stmt* b) { return Fa_make_while(c, b, { }); }
static Fa_ForStmt* for_stmt(Fa_NameExpr* t, Fa_Expr* i, Fa_Stmt* b) { return Fa_make_for(t, i, b, { }); }
static Fa_FunctionDef* func_def(Fa_NameExpr* n, Fa_ListExpr* p, Fa_Stmt* b) { return Fa_make_function(n, p, b, { }); }
static Fa_ReturnStmt* return_stmt(Fa_Expr* v = nullptr) { return Fa_make_return({ }, v); }
static Fa_ClassDef* class_def(Fa_Expr* n, Fa_Array<Fa_Expr*> mm, Fa_Array<Fa_Stmt*> me) { return Fa_make_class_def(n, mm, me, { }); }
static Fa_BreakStmt* break_stmt() { return Fa_make_break({ }); }
static Fa_ContinueStmt* continue_stmt() { return Fa_make_continue({ }); }

#endif // TEST_COMMON_H
