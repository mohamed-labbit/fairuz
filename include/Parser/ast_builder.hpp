//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Builds AST nodes from parser productions.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_PARSER_AST_BUILDER_HPP
#define FAIRUZ_PARSER_AST_BUILDER_HPP

#include "Support/arena.hpp"
#include "Support/error.hpp"
#include "ast.hpp"

namespace fairuz::AST {

// Parser construction facade; the free factories remain available to AST clients.
class ASTBuilder {
public:
    ASTBuilder() = default;

    // Parse and cache a deferred body only after a successful parse.
    ErrorOr<StmtPtr> materialize(FunctionStub& stub);

    static BinaryExpr* make_binary(Expr::Kind kind, ExprPtr lhs, ExprPtr rhs, SourceLocation loc)
    {
        return get_allocator().allocate_object<BinaryExpr>(kind, lhs, rhs, loc);
    }
    static UnaryExpr* make_unary(Expr::Kind kind, ExprPtr operand, SourceLocation loc)
    {
        return get_allocator().allocate_object<UnaryExpr>(kind, operand, loc);
    }
    static NilExpr* make_nil(SourceLocation loc) { return get_allocator().allocate_object<NilExpr>(loc); }
    static IntLiteralExpr* make_literal_int(int value, SourceLocation loc)
    {
        return get_allocator().allocate_object<IntLiteralExpr>(value, loc);
    }
    static IntLiteralExpr* make_literal_int(i64 value, SourceLocation loc)
    {
        return get_allocator().allocate_object<IntLiteralExpr>(value, loc);
    }
    static FloatLiteralExpr* make_literal_float(f64 value, SourceLocation loc)
    {
        return get_allocator().allocate_object<FloatLiteralExpr>(value, loc);
    }
    static StringLiteralExpr* make_literal_string(StringRef const str, SourceLocation loc)
    {
        return get_allocator().allocate_object<StringLiteralExpr>(str, loc);
    }
    static BoolLiteralExpr* make_literal_bool(bool value, SourceLocation loc)
    {
        return get_allocator().allocate_object<BoolLiteralExpr>(value, loc);
    }
    static IdentifierExpr* make_identifier(StringRef const str, SourceLocation loc)
    {
        return get_allocator().allocate_object<IdentifierExpr>(str, loc);
    }
    static ListExpr* make_list(Array<ExprPtr> elements, SourceLocation loc)
    {
        return get_allocator().allocate_object<ListExpr>(std::move(elements), loc);
    }
    static DictExpr* make_dict(Array<std::pair<ExprPtr, ExprPtr>> content, SourceLocation loc)
    {
        return get_allocator().allocate_object<DictExpr>(std::move(content), loc);
    }
    static GetExpr* make_get_expr(ExprPtr obj, IdentifierExpr* member, SourceLocation loc)
    {
        return get_allocator().allocate_object<GetExpr>(obj, member, loc);
    }
    static CallExpr* make_call(ExprPtr callee, Array<ExprPtr> args, SourceLocation loc)
    {
        return get_allocator().allocate_object<CallExpr>(callee, std::move(args), loc);
    }
    static AssignExpr* make_assignment_expr(ExprPtr target, ExprPtr value, SourceLocation loc)
    {
        return get_allocator().allocate_object<AssignExpr>(target, value, loc);
    }
    static IndexExpr* make_index(ExprPtr obj, ExprPtr idx, SourceLocation loc)
    {
        return get_allocator().allocate_object<IndexExpr>(obj, idx, loc);
    }
    static BlockStmt* make_block(Array<StmtPtr> stmts, SourceLocation loc)
    {
        return get_allocator().allocate_object<BlockStmt>(std::move(stmts), loc);
    }
    static ExprStmt* make_expr_stmt(ExprPtr expr, SourceLocation loc)
    {
        return get_allocator().allocate_object<ExprStmt>(expr, loc);
    }
    // Wrap an assignment expression in a statement.
    static ExprStmt* make_assignment_stmt(ExprPtr target, ExprPtr value, SourceLocation loc)
    {
        auto e = get_allocator().allocate_object<AssignExpr>(target, value, loc);
        return get_allocator().allocate_object<ExprStmt>(e, loc);
    }
    static IfElseStmt* make_if(ExprPtr cond, StmtPtr then_block, SourceLocation loc, StmtPtr else_block = nullptr)
    {
        return get_allocator().allocate_object<IfElseStmt>(cond, then_block, loc, else_block);
    }
    static WhileStmt* make_while(ExprPtr cond, StmtPtr body, SourceLocation loc)
    {
        return get_allocator().allocate_object<WhileStmt>(cond, body, loc);
    }
    static ForStmt* make_for(IdentifierExpr* target, ExprPtr iter, StmtPtr body, SourceLocation loc)
    {
        return get_allocator().allocate_object<ForStmt>(target, iter, body, loc);
    }
    static FunctionStub* make_function_stub(
        Array<tok::Token const*> tokens, diagnostic::SourcePtr source, StringRef body_span, SourceLocation loc)
    {
        return get_allocator().allocate_object<FunctionStub>(std::move(tokens), std::move(source), body_span, loc);
    }
    static FuncDefStmt* make_function(IdentifierExpr* name, Array<ExprPtr> params, StmtPtr body, SourceLocation loc)
    {
        return get_allocator().allocate_object<FuncDefStmt>(name, std::move(params), body, loc);
    }
    static ReturnStmt* make_return(SourceLocation loc, ExprPtr value = nullptr)
    {
        return get_allocator().allocate_object<ReturnStmt>(value, loc);
    }
    static ClassDefStmt* make_class_def(
        ExprPtr name, Array<ExprPtr> members, Array<StmtPtr> methods, SourceLocation loc)
    {
        return get_allocator().allocate_object<ClassDefStmt>(name, nullptr, members, methods, loc);
    }
    static ClassDefStmt* make_class_def(
        ExprPtr name, ExprPtr parent, Array<ExprPtr> members, Array<StmtPtr> methods, SourceLocation loc)
    {
        return get_allocator().allocate_object<ClassDefStmt>(name, parent, std::move(members), std::move(methods), loc);
    }
    static ImportStmt* make_import(
        StringRef const module, Array<StringRef> name, Array<StringRef> aliases, SourceLocation loc)
    {
        return get_allocator().allocate_object<ImportStmt>(module, name, std::move(aliases), loc);
    }
    static BreakStmt* make_break(SourceLocation loc) { return get_allocator().allocate_object<BreakStmt>(loc); }
    static ContinueStmt* make_continue(SourceLocation loc)
    {
        return get_allocator().allocate_object<ContinueStmt>(loc);
    }
};

} // namespace AST

#endif // FAIRUZ_PARSER_AST_BUILDER_HPP
