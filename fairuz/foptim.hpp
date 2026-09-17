#ifndef FA_OPTIM_HPP
#define FA_OPTIM_HPP

#include "fAST.hpp"
#include "fmacros.hpp"
#include "fvalue.hpp"

namespace fairuz::runtime {

std::optional<Value> const_value(AST::Expr const* e);
std::optional<Value> try_fold_unary(AST::UnaryExpr const* e);
std::optional<Value> _try_fold_binary(AST::BinaryExpr const* e);
std::optional<Value> try_fold_binary(AST::BinaryExpr const* e);
std::optional<Value> try_fold_expr(AST::Expr* e);
std::optional<AST::Expr*> try_strength_reduce_binary(AST::Expr* e);
std::optional<AST::Expr*> try_strength_reduce_unary(AST::Expr* e);
bool is_pure(AST::Expr* e);

} // namespace fairuz::runtime

#endif // FA_OPTIM_HPP
