#ifndef OPTIM_HPP
#define OPTIM_HPP

#include "../ast.hpp"
#include <optional>

namespace mylang::runtime {

using namespace ast;

std::optional<double> _foldBinary(Expr const* lhs, Expr const* rhs)
{
    if (!lhs && !rhs)
        return std::nullopt;

}

} // namespace mylang::runtime
#endif // OPTIM_HPP
