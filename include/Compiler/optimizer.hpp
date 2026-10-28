//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Defines the optimizer for constant folding and simplification.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_COMPILER_OPTIMIZER_HPP
#define FAIRUZ_COMPILER_OPTIMIZER_HPP

#include "Parser/ast.hpp"

#include <optional>

namespace fairuz::runtime {

class Optimizer {
public:
    Optimizer() = default;

    /// Fold a binary operation on numeric literals into a fresh arena-owned node.
    /// Return nullopt for unsupported, failing, or over-budget operations; the
    /// caller retains the original tree. Neither input node is modified/shared
    /// into a replacement tree. Nested expressions are left to the caller.
    static std::optional<AST::ExprPtr> try_constant_folding(AST::BinaryExpr* e);
};

} // namespace fairuz

#endif // FAIRUZ_COMPILER_OPTIMIZER_HPP
