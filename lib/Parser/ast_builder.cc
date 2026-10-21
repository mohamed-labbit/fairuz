//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Provides focused coverage for this Fairuz subsystem.
//===----------------------------------------------------------------------===//

#include "Parser/ast_builder.hpp"
#include "Parser/parser.hpp"

namespace fairuz::AST {

ErrorOr<StmtPtr> ASTBuilder::materialize(FunctionStub& stub)
{
    if (stub.parsed_body != nullptr) return stub.parsed_body;

    diagnostic::SourceScope source_scope(stub.source);
    auto errors_before = diagnostic::error_count();
    parser::Parser parser(stub.tokens, stub.source);
    auto body = parser.parse_function_body();
    if (body.has_error()) return body.error();
    // Block parsing recovers from errors; never cache its partial result.
    if (diagnostic::error_count() != errors_before)
        return Error(ErrorCode::INVALID_STATEMENT_NODE, stub.get_location());
    stub.parsed_body = body.value();
    return stub.parsed_body;
}

} // namespace fairuz::AST
