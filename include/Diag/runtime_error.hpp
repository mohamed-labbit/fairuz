#pragma once

#include "Diag/diagnostic.hpp"
#include "Interpreter/value.hpp"
#include "Macros.hpp"
#include "Support/error.hpp"

#include <cassert>
#include <stack>
#include <string>

namespace fairuz::runtime {

/// Every ErrorCode name must fall into one of these categories
/// a created category should not be empty in the sense that it
/// has no ErrorCode assigned to it.
enum class ErrorKind : int {
    /// type, arity, index, div/mod, missing name/module, user assert, explicit user error
    RUNTIME_ERROR,
    // vm frame, register limit
    LIMIT,
    // bigint .. size limit
    SIZE_LIMIT,
    PARSE_ERROR,
    COMPILE_ERROR,
    IMPORT_ERROR,
    // memory , stack exhaustion
    RESOURCE_LIMIT,
    // implementation internal exception
    IMPL_EXCEPT,
    // language semantic UB
    UNDEFINED_BEHAVIOR,
};

struct RuntimeTraceFrame {
    std::string function_name;
    std::string module_name;
    SourceLocation call_site;

    RuntimeTraceFrame() = default;

    bool is_main() { return module_name == "__main__"; }
};

struct RuntimeError : public Error {
    ErrorKind kind;
    ErrorCode code;
    diagnostic::SourcePtr src;
    std::string detail;
    SourceLocation loc;
    std::stack<RuntimeTraceFrame> traceback;
    RuntimeError* cause { nullptr };

    RuntimeError() = default;

    void add_trace_frame(RuntimeTraceFrame rtf)
    {
        if (!rtf.is_main())
            traceback.push(rtf);
    }
};

struct RuntimeRaised {
    RuntimeError const* error { nullptr };

    RuntimeRaised(RuntimeError* err)
        : error(err)
    {
        assert(error != nullptr);
    }
};

using ExecutionResult = ErrorOr<Value, RuntimeError>;

struct ImportCheckpoint { };
struct DiagnosticCheckpoint { };

struct ExecutionCheckpoint {
    int frame_depth;
    int stack_top;
    ImportCheckpoint imports;
    DiagnosticCheckpoint diagnostics;
};

/// Helpers
static constexpr ErrorKind error_kind(ErrorCode errc);

} // namespace fairuz::runtime
