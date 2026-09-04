#ifndef FA_OPTIM_HPP
#define FA_OPTIM_HPP

#include "fAST.hpp"
#include "fmacros.hpp"
#include "fvalue.hpp"

namespace fairuz::runtime {

std::optional<Fa_Value> const_value(AST::Fa_Expr const* e);
std::optional<Fa_Value> try_fold_unary(AST::Fa_UnaryExpr const* e);
std::optional<Fa_Value> _try_fold_binary(AST::Fa_BinaryExpr const* e);
std::optional<Fa_Value> try_fold_binary(AST::Fa_BinaryExpr const* e);
std::optional<Fa_Value> try_fold_expr(AST::Fa_Expr* e);
std::optional<AST::Fa_Expr*> try_strength_reduce_binary(AST::Fa_Expr* e);
std::optional<AST::Fa_Expr*> try_strength_reduce_unary(AST::Fa_Expr* e);
bool Fa_is_pure(AST::Fa_Expr* e);

} // namespace fairuz::runtime

#endif // FA_OPTIM_HPP
