#include "fASTBuilder.hpp"
#include "fparser.hpp"

namespace fairuz::AST {

ErrorOr<StmtPtr> ASTBuilder::materialize(FunctionStub& stub)
{
    if (stub.parsed_body != nullptr)
        return stub.parsed_body;

    diagnostic::SourceScope source_scope(stub.source);
    auto errors_before = diagnostic::error_count();
    parser::Parser parser(stub.tokens, stub.source);
    auto body = parser.parse_function_body();
    if (body.has_error())
        return body.error();
    // Block parsing recovers from errors; never cache its partial result.
    if (diagnostic::error_count() != errors_before)
        return Error(ErrorCode::INVALID_STATEMENT_NODE);
    stub.parsed_body = body.value();
    return stub.parsed_body;
}

} // namespace fairuz::AST
