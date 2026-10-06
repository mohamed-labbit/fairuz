#ifndef FA_AST_BUILDER_HPP
#define FA_AST_BUILDER_HPP

#include "fAST.hpp"
#include "ferror.hpp"

namespace fairuz::AST {

// Parser construction facade; the free factories remain available to AST clients.
class ASTBuilder {
public:
    ASTBuilder() = default;

    // Parse and cache a deferred body only after a successful parse.
    ErrorOr<StmtPtr> materialize(FunctionStub& stub);

    BinaryExpr* make_binary(Expr::Kind kind, ExprPtr lhs, ExprPtr rhs, SourceLocation loc)
    {
        return AST::make_binary(kind, lhs, rhs, loc);
    }
    UnaryExpr* make_unary(Expr::Kind kind, ExprPtr operand, SourceLocation loc)
    {
        return AST::make_unary(kind, operand, loc);
    }
    NilExpr* make_nil(SourceLocation loc)
    {
        return AST::make_nil(loc);
    }
    IntLiteralExpr* make_literal_int(int value, SourceLocation loc)
    {
        return AST::make_literal_int(value, loc);
    }
    IntLiteralExpr* make_literal_int(i64 value, SourceLocation loc)
    {
        return AST::make_literal_int(value, loc);
    }
    FloatLiteralExpr* make_literal_float(f64 value, SourceLocation loc)
    {
        return AST::make_literal_float(value, loc);
    }
    StringLiteralExpr* make_literal_string(StringRef const str, SourceLocation loc)
    {
        return AST::make_literal_string(str, loc);
    }
    BoolLiteralExpr* make_literal_bool(bool value, SourceLocation loc)
    {
        return AST::make_literal_bool(value, loc);
    }
    IdentifierExpr* make_identifier(StringRef const str, SourceLocation loc)
    {
        return AST::make_identifier(str, loc);
    }
    ListExpr* make_list(Array<ExprPtr> elements, SourceLocation loc)
    {
        return AST::make_list(std::move(elements), loc);
    }
    DictExpr* make_dict(Array<std::pair<ExprPtr, ExprPtr>> content, SourceLocation loc)
    {
        return AST::make_dict(std::move(content), loc);
    }
    GetExpr* make_get_expr(ExprPtr obj, IdentifierExpr* member, SourceLocation loc)
    {
        return AST::make_get_expr(obj, member, loc);
    }
    CallExpr* make_call(ExprPtr callee, Array<ExprPtr> args, SourceLocation loc)
    {
        return AST::make_call(callee, std::move(args), loc);
    }
    AssignExpr* make_assignment_expr(ExprPtr target, ExprPtr value, SourceLocation loc)
    {
        return AST::make_assignment_expr(target, value, loc);
    }
    IndexExpr* make_index(ExprPtr obj, ExprPtr idx, SourceLocation loc)
    {
        return AST::make_index(obj, idx, loc);
    }
    BlockStmt* make_block(Array<StmtPtr> stmts, SourceLocation loc)
    {
        return AST::make_block(std::move(stmts), loc);
    }
    ExprStmt* make_expr_stmt(ExprPtr expr, SourceLocation loc)
    {
        return AST::make_expr_stmt(expr, loc);
    }
    // Wrap an assignment expression in a statement.
    ExprStmt* make_assignment_stmt(ExprPtr target, ExprPtr value, SourceLocation loc)
    {
        return AST::make_assignment_stmt(target, value, loc);
    }
    IfElseStmt* make_if(ExprPtr cond, StmtPtr then_block, SourceLocation loc, StmtPtr else_block = nullptr)
    {
        return AST::make_if(cond, then_block, loc, else_block);
    }
    WhileStmt* make_while(ExprPtr cond, StmtPtr body, SourceLocation loc)
    {
        return AST::make_while(cond, body, loc);
    }
    ForStmt* make_for(IdentifierExpr* target, ExprPtr iter, StmtPtr body, SourceLocation loc)
    {
        return AST::make_for(target, iter, body, loc);
    }
    FunctionStub* make_function_stub(Array<tok::Token const*> tokens,
        diagnostic::SourcePtr source, StringRef body_span, SourceLocation loc)
    {
        return AST::make_function_stub(std::move(tokens), std::move(source), body_span, loc);
    }
    FuncDefStmt* make_function(IdentifierExpr* name, Array<ExprPtr> params, StmtPtr body, SourceLocation loc)
    {
        return AST::make_function(name, std::move(params), body, loc);
    }
    ReturnStmt* make_return(SourceLocation loc, ExprPtr value = nullptr)
    {
        return AST::make_return(loc, value);
    }
    ClassDefStmt* make_class_def(ExprPtr name, Array<ExprPtr> members,
        Array<StmtPtr> methods, SourceLocation loc)
    {
        return AST::make_class_def(name, std::move(members), std::move(methods), loc);
    }
    ClassDefStmt* make_class_def(ExprPtr name, ExprPtr parent,
        Array<ExprPtr> members, Array<StmtPtr> methods, SourceLocation loc)
    {
        return AST::make_class_def(name, parent, std::move(members), std::move(methods), loc);
    }
    ImportStmt* make_import(StringRef const module, Array<StringRef> name,
        Array<StringRef> aliases, SourceLocation loc)
    {
        return AST::make_import(module, name, std::move(aliases), loc);
    }
    BreakStmt* make_break(SourceLocation loc)
    {
        return AST::make_break(loc);
    }
    ContinueStmt* make_continue(SourceLocation loc)
    {
        return AST::make_continue(loc);
    }
};

} // namespace AST

#endif // FA_AST_BUILDER_HPP
