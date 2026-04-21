#ifndef OPTIM_HPP
#define OPTIM_HPP

#include "ast.hpp"
#include "macros.hpp"
#include "value.hpp"

namespace fairuz::runtime {

std::optional<Fa_Value> const_value(AST::Fa_Expr const* e);
std::optional<Fa_Value> try_fold_unary(AST::Fa_UnaryExpr const* e);
std::optional<Fa_Value> _try_fold_binary(AST::Fa_BinaryExpr const* e);
std::optional<Fa_Value> try_fold_binary(AST::Fa_BinaryExpr const* e);
std::optional<Fa_Value> try_fold_expr(AST::Fa_Expr const* e);

} // namespace fairuz::runtime

#endif // OPTIM_HPP
